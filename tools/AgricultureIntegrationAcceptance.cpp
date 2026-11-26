// Read-only integration observer for an explicitly supplied earned Rule36 city.
// Source-bound restore and ordinary World ticks remain the only authority.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "FoodNativePlaytest.h"

#include <charconv>
#include <chrono>
#include <limits>

namespace l=oe_luxury_progression;
namespace p=oe_food_playtest;
namespace {
using Id=sim::BuildingId;
using Clock=std::chrono::steady_clock;
constexpr std::uint64_t year_ticks=51U*16U*12U;
constexpr std::array<const char*,12> food_names{"Wheat7","Millet5","Rice6","Cabbage3","BeanCurd1","Fish2","Meat4","Dinners28-grade1","Dinners28-grade2","Dinners28-grade3","Dinners28-grade4","Dinners28-grade5"};
struct Guard {
    fs::path path;std::string bytes;fs::perms mode;fs::file_time_type modified;
    explicit Guard(fs::path value):path(std::move(value)) {
        l::no_symlinks(path);l::check(fs::is_regular_file(path)&&fs::file_size(path)<=64U*1024U*1024U,"Bounded regular input required");
        bytes=l::q::f::read_text(path);mode=fs::status(path).permissions();modified=fs::last_write_time(path);
    }
    void unchanged()const {l::check(l::q::f::read_text(path)==bytes&&fs::status(path).permissions()==mode&&fs::last_write_time(path)==modified,"Read-only input bytes/mode/mtime changed");}
};
struct Options {fs::path data,input,output,map,replay;unsigned ticks=100000;};
constexpr const char* usage="openemperor-agriculture-integration-acceptance --data ABS --input EXPLICIT_EARNED_SCHEMA52_SAVE --output FRESH_.local/agri-mac14/SUBDIR [--map Cities/NAME.map] [--ticks 100000] [--replay PRIOR_RUN_DIR]";
Options options(int argc,char** argv) {
    Options o;std::map<std::string,std::string> args;
    for(int n=1;n<argc;n+=2) {
        l::check(n+1<argc,usage);const std::string key=argv[n];
        l::check(key=="--data"||key=="--input"||key=="--output"||key=="--map"||key=="--ticks"||key=="--replay",usage);
        l::check(args.emplace(key,argv[n+1]).second,"Duplicate argument");
    }
    l::check(args.contains("--data")&&args.contains("--input")&&args.contains("--output"),usage);
    l::no_symlinks(args.at("--data"));l::no_symlinks(args.at("--input"));
    o.data=fs::canonical(args.at("--data"));o.input=fs::canonical(args.at("--input"));
    if(args.contains("--map"))o.map=args.at("--map");
    if(args.contains("--ticks")) {
        const auto& text=args.at("--ticks");const auto parsed=std::from_chars(text.data(),text.data()+text.size(),o.ticks);
        l::check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size(),"Invalid tick count");
    }
    l::check(o.ticks>=20400&&o.ticks<=200000,"Require20400..200000 real continuation ticks");
    if(args.contains("--replay")){l::no_symlinks(args.at("--replay"));o.replay=fs::canonical(args.at("--replay"));}
    o.output=p::output_root(args.at("--output"),o.data,".local/agri-mac14");return o;
}
struct Context {
    l::q::Context map;std::shared_ptr<const orig::OriginalRulesModel> model;std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    Context(const fs::path& data,const fs::path& selected,orig::Difficulty difficulty):
        map(oe_longplay_fixture::real_map_context(data,selected)),
        model(l::save::prepare_food_processing_rules_model(data,difficulty)),
        terrain(l::save::prepare_playable_hunting_terrain_facts(data,selected)){}
    l::save::SaveDocument document(const sim::World& world)const{return l::save::make_document(map.data,map.map_relative,map.mask,world);}
    sim::World restore(const l::save::SaveDocument& document)const {
        l::check(document.source_schema_version==52&&document.world.rule_version==36,"Exact Rule36/schema52 required");
        return l::save::restore_agriculture_reconfiguration(document,map.data,map.mask,map.policy,model,terrain);
    }
};
struct Totals {
    sim::ProcessedFoodCounters origin{},held{},used{},carried{},house_consumed{};
    std::array<std::uint64_t,orig::goods_count> house_goods{};
};
Totals totals(const sim::World& world) {
    l::check(world.rule_version()==36&&world.processed_food_state()&&!world.hunting_food_state()&&!world.fishing_food_state()&&!world.original_agriculture_state(),"Single Rule36 Food authority required");
    Totals result;
    for(const auto& field:world.agriculture_fields()) {
        const auto& receipts=world.physical_food_growers_state()->fields;
        const auto row=std::find_if(receipts.begin(),receipts.end(),[&](const auto& item){return item.field_id==field.field_id;});
        l::check(row!=receipts.end(),"Missing physical Field return history");
        result.origin.at(static_cast<std::size_t>(field.crop))+=row->returned;
    }
    for(const auto& quay:world.fishing_state()->quays)result.origin[5]+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)result.origin[6]+=tent.unloaded;
    for(const auto& mill:world.food_processing_state()->mills)for(std::size_t grade=0;grade<4;++grade)result.origin[7+grade]+=(grade+1)*mill.completed_by_grade[grade];
    for(const auto& stock:world.processed_food_state()->stocks)for(std::size_t i=0;i<12;++i) {
        l::check(stock.amounts[i]>=0,"Negative physical Food stock");
        result.held[i]+=static_cast<unsigned>(stock.amounts[i]);result.used[i]+=stock.consumed[i];
        if(sim::original_house_kind(world.building(stock.building_id).kind))result.house_consumed[i]+=stock.consumed[i];
    }
    for(const auto& trip:world.processed_food_state()->trips)for(std::size_t i=0;i<12;++i) {
        const auto cargo=trip.loaded[i]-trip.delivered[i]-trip.returned[i];l::check(cargo>=0&&cargo<=4,"Invalid physical Food cargo");result.carried[i]+=static_cast<unsigned>(cargo);
    }
    for(std::size_t i=0;i<12;++i)l::check(result.origin[i]==result.held[i]+result.used[i]+result.carried[i],"Food origin != held + consumed/processor-debited + physical cargo");
    for(const auto& home:world.original_production_state()->housing.houses)for(const auto& row:home.consumption_rows)
        for(std::size_t i=0;i<orig::goods_count;++i)result.house_goods[i]+=row.consumed[i];
    return result;
}
std::size_t channels(const sim::World& world) {
    return world.couriers().size()+world.grower_navigation_tracks().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size();
}
void valid(const sim::World& world) {
    l::check(world.population_valid()&&world.city_economy_valid()&&world.production_balance_valid()&&world.navigation_valid()&&world.fire_state_valid(),"Authoritative World invariant failed");
    l::check(world.buildings().size()<=82&&channels(world)<=58,"Lifetime82/58 bounds exceeded");
    for(const auto good:l::luxury_goods) {
        const auto balance=world.original_goods_balance(good);
        l::check(balance.supported&&balance.conserved&&!balance.totals.imported&&!balance.totals.demolished,"Luxury origin/conservation failed");
    }
    for(std::size_t i=0;i<sim::original_aux_material_count;++i)l::check(world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(i)).conserved,"Auxiliary conservation failed");
}
Json sample(const sim::World& world) {
    valid(world);const auto counts=totals(world);Json houses=Json::array(),sources=Json::array(),fields=Json::array(),plants=Json::array(),mills=Json::array();
    for(const auto& home:world.original_economy_state()->houses) {
        const auto diagnosis=world.original_housing_diagnosis(home.building_id);auto query=diagnosis.actual;query.desirability=static_cast<std::int8_t>(diagnosis.cached_desirability);
        houses.push_back({{"id",static_cast<unsigned>(home.building_id)},{"actual",l::matrix::state(home.house)},
            {"burning",world.building_on_fire(home.building_id)},{"raw_des",diagnosis.raw_desirability},{"cardinal_water_bonus",diagnosis.cardinal_water_bonus},{"query_des",diagnosis.cached_desirability},
            {"current",l::matrix::row(diagnosis.current,query,world.original_rules_model()->house_rules())},
            {"next",diagnosis.common_terminal||diagnosis.elite_terminal?Json(nullptr):l::matrix::row(diagnosis.next,query,world.original_rules_model()->house_rules())}});
    }
    for(const auto& body:world.buildings())if(!sim::original_house_kind(body.kind))sources.push_back(l::trace::owner(world,body.id));
    for(const auto& row:world.multi_field_agriculture_state()->fields)fields.push_back({{"field",static_cast<unsigned>(row.field_id)},{"productive_ticks",row.productive_ticks}});
    for(const auto& plant:world.plantations_state()->plants)plants.push_back({{"plant",static_cast<unsigned>(plant.plant_id)},{"source",static_cast<unsigned>(plant.source_id)},{"work",plant.productive_ticks},{"growth",plant.growth},{"picked",plant.harvested},{"returned",plant.returned}});
    for(const auto& mill:world.food_processing_state()->mills)mills.push_back({{"id",static_cast<unsigned>(mill.mill_id)},{"completed",mill.completed},{"productive_ticks",mill.productive_ticks},{"active_work",mill.active?Json(mill.active->work):Json(nullptr)}});
    return {{"tick",world.ticks()},{"year_ordinal",world.ticks()/year_ticks},{"population",world.total_population()},{"funds",world.treasury()},
        {"taxes",world.taxes_collected_total()},{"upkeep",world.maintenance_spent_total()},{"construction",world.construction_spent_total()},
        {"installed_workers",world.workforce_required()},{"active_workers",world.active_workforce_required()},{"workers_used",world.workforce_used()},
        {"bodies",world.buildings().size()},{"channels",channels(world)},
        {"origin",counts.origin},{"held",counts.held},{"used_including_processor_debits",counts.used},{"physical_cargo",counts.carried},
        {"house_food_consumed",counts.house_consumed},{"house_goods_consumed",counts.house_goods},{"houses",houses},{"sources",sources},
        {"field_lifetime_work",fields},{"plants",plants},{"mills",mills},{"goods_balances",l::trace::balances(world)}};
}
template<std::size_t N>std::array<std::uint64_t,N> delta(const std::array<std::uint64_t,N>& after,const std::array<std::uint64_t,N>& before) {
    std::array<std::uint64_t,N> result{};
    for(std::size_t i=0;i<N;++i){l::check(after[i]>=before[i],"Lifetime receipt/consumption counter decreased");result[i]=after[i]-before[i];}return result;
}
}
int main(int argc,char** argv) {
    fs::path output;std::uint64_t completed=0;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto o=options(argc,argv);output=o.output;Guard input(o.input);const auto document=l::save::read_save(o.input);
        l::check(document.source_schema_version==52&&document.world.rule_version==36&&document.world.original_economy&&document.world.original_economy->model_source,"Only explicit source-bound Rule36/schema52 city inputs");
        const auto selected=o.map.empty()?document.map_relative:o.map;
        l::check(!selected.is_absolute()&&selected==selected.lexically_normal()&&selected.parent_path()=="Cities"&&selected.extension()==".map"&&selected==document.map_relative,"Exact supplied Cities map identity required");
        Guard model(o.data/"Model/EmperorBuildingModels.txt"),map(o.data/selected);
        Context context(o.data,selected,document.world.original_economy->model_source->difficulty);
        auto world=context.restore(document);auto restored=context.restore(document);
        const auto begin=world.ticks();
        l::check(begin<=std::numeric_limits<std::uint64_t>::max()-o.ticks,"Continuation tick horizon overflows");
        const auto end=begin+o.ticks;completed=begin;const auto baseline=totals(world);const auto initial=sample(world);
        std::vector<Guard> replay_inputs;
        if(!o.replay.empty())for(const char* name:{"initial.save.json","final.save.json","telemetry.jsonl"})replay_inputs.emplace_back(o.replay/name);
        Json checkpoints=Json::array();l::save::RecoveryStore recovery(output/"recovery",o.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);std::string history;
        const auto checkpoint=[&](const std::string& label) {
            l::check(checkpoints.size()<16,"Bounded16 checkpoint allowance exhausted");valid(world);const auto path=output/(label+".save.json");
            l::save::write_save(path,context.document(world),context.map.data,context.map.mask);
            const auto saved=l::save::read_save(path);auto loaded=context.restore(saved);
            l::check(loaded.snapshot()==world.snapshot()&&restored.snapshot()==world.snapshot(),"Fresh independent World checkpoint diverged");
            const auto again=output/(label+".roundtrip.json");l::save::write_save(again,context.document(loaded),context.map.data,context.map.mask);
            l::check(l::q::f::read_text(path)==l::q::f::read_text(again),"Canonical JSON roundtrip changed bytes");
            if(history.empty())history=recovery.create_history(saved,context.map.mask);
            const auto r=recovery.write_checkpoint(history,saved,context.map.mask);
            l::check(r.loadable()&&r.schema==52&&context.restore(l::save::read_save(r.path)).snapshot()==world.snapshot(),"Recovery checkpoint lost authority");
            checkpoints.push_back({{"tick",world.ticks()},{"file",path.filename().string()},{"canonical_roundtrip_exact",true},{"recovery_exact",true}});
        };
        checkpoint("initial");
        if(!o.replay.empty())l::check(l::q::f::read_text(output/"initial.save.json")==replay_inputs[0].bytes,"Separate process input city differs");
        std::ofstream telemetry(output/"telemetry.jsonl");l::check(bool(telemetry),"Cannot create telemetry stream");telemetry<<initial.dump()<<'\n';
        std::map<Id,unsigned> previous_types;for(const auto& home:world.original_economy_state()->houses)previous_types.emplace(home.building_id,static_cast<unsigned>(home.house.type));
        std::uint64_t devolutions=0,burning_house_ticks=0,negative_funds_ticks=0;std::int64_t minimum_funds=world.treasury();
        std::uint64_t maximum_population=static_cast<std::uint64_t>(world.total_population());std::size_t maximum_bodies=world.buildings().size(),maximum_channels=channels(world);
        Json first_consumption=Json::object(),first_luxury_consumption=Json::object();std::optional<std::uint64_t> maturity;
        const auto started=Clock::now();std::chrono::nanoseconds tick_total{};std::chrono::nanoseconds tick_max{};
        // Twelve real JSON/Recovery checkpoints, independent of duration.
        const auto interval=(std::uint64_t(o.ticks)+10U)/11U;
        while(world.ticks()<end) {
            const auto tick_started=Clock::now();world.tick();const auto elapsed=Clock::now()-tick_started;tick_total+=elapsed;tick_max=std::max(tick_max,std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
            restored.tick();completed=world.ticks();minimum_funds=std::min(minimum_funds,world.treasury());negative_funds_ticks+=world.treasury()<0;
            maximum_population=std::max(maximum_population,std::uint64_t(world.total_population()));maximum_bodies=std::max(maximum_bodies,world.buildings().size());maximum_channels=std::max(maximum_channels,channels(world));
            for(const auto& home:world.original_economy_state()->houses) {
                const auto type=static_cast<unsigned>(home.house.type);const auto previous=previous_types.at(home.building_id);devolutions+=type<previous;previous_types[home.building_id]=type;burning_house_ticks+=world.building_on_fire(home.building_id);
            }
            // Bound the observer to existing inventories; no retained event vector.
            sim::ProcessedFoodCounters consumed{};for(const auto& stock:world.processed_food_state()->stocks)if(sim::original_house_kind(world.building(stock.building_id).kind))for(std::size_t i=0;i<12;++i)consumed[i]+=stock.consumed[i];
            bool all_raw=true,any_meal=false;
            for(std::size_t i=0;i<12;++i) {
                const bool earned=consumed[i]>baseline.house_consumed[i];
                if(earned&&!first_consumption.contains(food_names[i]))first_consumption[food_names[i]]=world.ticks();
                if(i<7)all_raw&=earned;else any_meal|=earned;
            }
            if(!maturity&&all_raw&&any_meal)maturity=world.ticks();
            for(const auto good:l::luxury_goods) {
                std::uint64_t consumed_good=0;const auto i=static_cast<std::size_t>(good);
                for(const auto& home:world.original_production_state()->housing.houses)for(const auto& row:home.consumption_rows)consumed_good+=row.consumed[i];
                const auto key=std::to_string(i);if(consumed_good>baseline.house_goods[i]&&!first_luxury_consumption.contains(key))first_luxury_consumption[key]=world.ticks();
            }
            if((world.ticks()-begin)%816U==0||world.ticks()==end) {
                l::check(restored.snapshot()==world.snapshot(),"Independent restored continuation differs");telemetry<<sample(world).dump()<<'\n';l::check(bool(telemetry),"Telemetry stream failed");
            }
            if((world.ticks()-begin)%interval==0&&world.ticks()!=end)checkpoint("tick-"+std::to_string(world.ticks()));
        }
        checkpoint("final");telemetry.close();const auto final=sample(world);const auto ending=totals(world);
        const auto food_delta=delta(ending.house_consumed,baseline.house_consumed);
        const auto goods_delta=delta(ending.house_goods,baseline.house_goods);
        Json missing=Json::array();for(std::size_t i=0;i<7;++i)if(!food_delta[i])missing.push_back(std::string("No new House consumption of ")+food_names[i]+" during this continuation");
        const bool meals=std::any_of(food_delta.begin()+7,food_delta.begin()+11,[](auto n){return n>0;});if(!meals)missing.push_back("No new House Dinners consumption during this continuation");
        for(const auto good:l::luxury_goods)if(!goods_delta[static_cast<std::size_t>(good)])missing.push_back("No new House luxury debit for Goods index "+std::to_string(static_cast<unsigned>(good))+"; inspect actual current/next needs and paid source catalog");
        bool process_exact=false;
        if(!o.replay.empty()) {
            l::check(l::q::f::read_text(output/"final.save.json")==replay_inputs[1].bytes&&l::q::f::read_text(output/"telemetry.jsonl")==replay_inputs[2].bytes,"Separate process canonical final or entire deterministic telemetry differs");process_exact=true;
        }
        input.unchanged();model.unchanged();map.unchanged();for(const auto& guard:replay_inputs)guard.unchanged();
        const auto duration=std::chrono::duration<double>(Clock::now()-started).count();
        l::write(output/"report.json",{{"format","openemperor-agriculture-integration-acceptance-v1"},{"status","PASS"},{"gameplay_coverage",missing.empty()?"PASS":"PARTIAL"},
            {"rule",36},{"policy",3},{"schema",52},{"map",selected.generic_string()},{"input",o.input.generic_string()},
            {"continued_real_ticks",o.ticks},{"initial_tick",begin},{"final_tick",world.ticks()},{"complete_year_boundaries_crossed",end/year_ticks-begin/year_ticks},
            {"initial",initial},{"final",final},{"new_house_food_consumption",food_delta},{"new_house_goods_consumption",goods_delta},
            {"first_new_food_consumption_ticks",first_consumption},{"first_new_luxury_consumption_ticks_by_goods_index",first_luxury_consumption},
            {"food_and_meal_maturity_tick",maturity?Json(*maturity):Json(nullptr)},{"actual_post_food_and_meal_maturity_ticks",maturity?end-*maturity:0},
            {"minimum_funds",minimum_funds},{"negative_funds_ticks",negative_funds_ticks},{"maximum_population",maximum_population},{"house_devolutions",devolutions},{"burning_house_ticks",burning_house_ticks},
            {"maximum_lifetime_bodies",maximum_bodies},{"maximum_reserved_channels",maximum_channels},{"checkpoints",checkpoints},
            {"fresh_restored_world_continuation_exact",true},{"separate_process_replay_exact",process_exact},{"source_bytes_mode_mtime_unchanged",true},
            {"stock_population_model_or_arrival_injection",false},{"commands_after_restore",0},{"supplied_seed_provenance","Caller must independently verify the supplied save was earned; valid restore alone does not prove its creation"},
            {"missing_gameplay_witnesses",missing},{"paid_source_catalog",l::matrix::source_catalog(world)},
            {"timing",{{"wall_seconds",duration},{"measured_primary_tick_mean_us",double(tick_total.count())/double(o.ticks)/1000.0},{"measured_primary_tick_max_us",double(tick_max.count())/1000.0},{"observer_and_independent_world_excluded_from_tick_timing",true}}},
            {"memory_peak","Measure externally; no frame/tick instrumentation added to game"},{"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},
            {"claim_scope","Technical continuation and source-bound persistence; gameplay coverage is reported separately. AUTHORED Rule36, no Original runtime parity or human acceptance"}});
        std::cout<<"PASS technical continuation ticks="<<o.ticks<<" gameplay="<<(missing.empty()?"PASS":"PARTIAL")<<" report="<<output/"report.json"<<'\n';return 0;
    } catch(const std::exception& error) {
        if(!output.empty()&&!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()},{"last_completed_tick",completed}});}catch(...){}
        std::cerr<<"Agriculture integration FAIL: "<<error.what()<<'\n';return 1;
    }
}
