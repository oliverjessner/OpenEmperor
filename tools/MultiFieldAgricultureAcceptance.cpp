// Supplied-data acceptance only. Reuse the existing read-only source guards
// and paid player recipe; no synthetic Fixture is instantiated by this tool.
#include "../tests/LuxuryProgressionFixture.h"

#include <charconv>
#include <iostream>

namespace l=oe_luxury_progression;
namespace {
using Json=l::Json;
using Id=l::sim::BuildingId;
constexpr const char* usage="openemperor-multi-field-agriculture-acceptance --data ABS --output FRESH_.local/agri-mac05/SUBDIR [--ticks 14000] [--replay EXACT46_SAVE --expected CREATOR_FINAL_SAVE]";
struct Options {l::fs::path data,output,replay,expected;unsigned ticks=14000;};
Options parse(int argc,char** argv) {
    Options result;std::map<std::string,std::string> arguments;
    for(int i=1;i<argc;i+=2) {
        l::check(i+1<argc,usage);const std::string key=argv[i];
        l::check(key=="--data"||key=="--output"||key=="--ticks"||key=="--replay"||key=="--expected",usage);
        l::check(arguments.emplace(key,argv[i+1]).second,"Duplicate acceptance argument");
    }
    l::check(arguments.contains("--data")&&arguments.contains("--output"),usage);
    const auto data=l::fs::absolute(arguments.at("--data"));l::no_symlinks(data);result.data=l::fs::canonical(data);
    const auto requested=l::fs::absolute(arguments.at("--output"));l::no_symlinks(requested);
    result.output=l::fs::weakly_canonical(requested);
    const auto allowed=l::fs::weakly_canonical(l::fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac05");
    l::check(result.output!=allowed&&l::below(result.output,allowed)&&!l::below(result.output,result.data)&&
        !l::below(result.data,result.output)&&!l::fs::exists(result.output),
        "Use a fresh .local/agri-mac05 subdirectory disjoint from Original data");
    l::save::validate_save_target(result.output/"final.save.json",result.data);
    for(const auto& [name,destination]:{std::pair{"--replay",&result.replay},{"--expected",&result.expected}})
        if(arguments.contains(name)) {
            *destination=l::fs::absolute(arguments.at(name));l::no_symlinks(*destination);
            l::check(l::fs::is_regular_file(*destination)&&l::fs::file_size(*destination)<=8U*1024U*1024U,
                "Bounded explicit replay file required");
            l::check(!l::below(*destination,result.data),"Replay saves must be outside Original data");
        }
    l::check(result.replay.empty()==result.expected.empty(),"Replay requires both --replay and --expected");
    if(arguments.contains("--ticks")) {
        const auto& value=arguments.at("--ticks");const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result.ticks);
        l::check(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size(),"Invalid tick count");
    }
    l::check(result.ticks>=12000&&result.ticks<=100000,"Acceptance requires12000..100000 real ticks");return result;
}
struct Context {
    l::Context source;
    explicit Context(const l::fs::path& data):source(data) {}
    l::sim::World create()const {
        return l::sim::World::create_multi_field_agriculture(source.map.policy,source.model,source.terrain);
    }
    l::save::SaveDocument document(const l::sim::World& world)const{return source.document(world);}
    l::sim::World restore(const l::save::SaveDocument& document)const {
        l::check(document.source_schema_version==46&&document.world.rule_version==30,"Exact Rule30/schema46 input required");
        return l::save::restore_multi_field_agriculture(document,source.map.data,source.map.mask,
            source.map.policy,source.model,source.terrain);
    }
};
Json conservation(const l::sim::World& world) {
    l::check(world.rule_version()==30&&world.multi_field_agriculture_state()&&world.hunting_food_state()&&
        !world.original_agriculture_state()&&!world.fishing_food_state()&&
        world.original_production_state()->food==l::sim::OriginalFoodState{},"Food authority is not the sole seven-kind account");
    l::check(world.population_valid()&&world.city_economy_valid()&&world.production_balance_valid()&&
        world.navigation_valid()&&world.fire_state_valid(),"Shared World authority failed");
    const auto& food=*world.hunting_food_state();
    l::sim::HuntingFoodCounters produced{},held{},cargo{},consumed{},departed{},received{},returned{};
    for(const auto& field:food.fields)produced[static_cast<unsigned>(field.crop)]+=field.harvested;
    for(const auto& quay:world.fishing_state()->quays)produced[l::sim::fishing_food_index]+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)produced[l::sim::hunting_food_index]+=tent.unloaded;
    for(const auto& stock:food.stocks)for(std::size_t g=0;g<7;++g) {
        l::check(stock.amounts[g]>=0,"Negative physical Food stock");held[g]+=static_cast<std::uint64_t>(stock.amounts[g]);
        consumed[g]+=stock.consumed[g];departed[g]+=stock.departed[g];received[g]+=stock.received[g];returned[g]+=stock.returned[g];
    }
    for(const auto& trip:food.trips)for(std::size_t g=0;g<7;++g) {
        const auto remaining=std::int64_t(trip.loaded[g])-trip.delivered[g]-trip.returned[g];
        l::check(remaining>=0&&remaining<=4,"Impossible physical Cart cargo");cargo[g]+=static_cast<std::uint64_t>(remaining);
    }
    for(std::size_t g=0;g<7;++g) {
        l::check(produced[g]==held[g]+cargo[g]+consumed[g],"Food source/stock/cargo/consumption conservation failed");
        l::check(departed[g]==received[g]+returned[g]+cargo[g],"Food departures/receipts/returns/cargo conservation failed");
    }
    l::sim::World::validate_multi_field_agriculture_snapshot(world.snapshot());
    l::sim::World::validate_hunting_snapshot(world.snapshot());
    return {{"produced",produced},{"held",held},{"physical_cart_cargo",cargo},{"consumed",consumed},
        {"departed",departed},{"received",received},{"returned",returned},{"conserved",true}};
}
void checkpoint(const Context& context,const l::sim::World& world,const l::fs::path& output,
                const char* label,Json& manifest) {
    (void)conservation(world);const auto path=output/(std::string(label)+".save.json");
    l::check(!l::fs::exists(path),"Preserve earlier acceptance checkpoint");
    l::save::write_save(path,context.document(world),context.source.map.data,context.source.map.mask);
    auto restored=context.restore(l::save::read_save(path));l::check(restored.snapshot()==world.snapshot(),"Schema46 checkpoint lost exact state");
    const auto roundtrip=output/(std::string(label)+".roundtrip.json");
    l::save::write_save(roundtrip,context.document(restored),context.source.map.data,context.source.map.mask);
    l::check(l::q::f::read_text(path)==l::q::f::read_text(roundtrip),"Schema46 canonical checkpoint bytes differ");
    manifest.push_back({{"label",label},{"tick",world.ticks()},{"file",path.filename().string()},{"exact_roundtrip",true}});
}
bool paid_command(l::sim::World& world,l::sim::Command command,Json& commands) {
    const auto before=world.snapshot();const auto quote=world.construction_cost(command.type);const auto planned=world.validate(command);
    const auto result=world.execute(command);
    Json row={{"tick",before.ticks},{"command",static_cast<unsigned>(command.type)},{"cell",{command.cell.x,command.cell.y}},
        {"owner_argument",static_cast<unsigned>(command.building_id)},{"reason","MAC05 actual paid multi-field purchase"},
        {"quoted_cost",quote},{"funds_before",before.treasury},{"funds_after",world.treasury()},
        {"sequence_before",before.command_sequence},{"sequence_after",world.command_sequence()},
        {"preview_accepted",planned.accepted},{"accepted",result.accepted},{"changed",result.changed},{"result",result.reason}};
    commands.push_back(std::move(row));
    if(!result.accepted) {
        l::check(world.snapshot()==before,"Refused scene candidate changed paid authority");
        std::cerr<<"Paid scene candidate refused: command="<<static_cast<unsigned>(command.type)<<" cell="<<command.cell.x<<','<<command.cell.y
            <<" owner="<<static_cast<unsigned>(command.building_id)<<" preview="<<planned.accepted<<" reason="<<result.reason<<'\n';
        return false;
    }
    l::check(planned.accepted&&result.changed&&world.ticks()==before.ticks&&world.treasury()==before.treasury-quote&&
        world.construction_spent_total()==before.construction_spent_total+static_cast<std::uint64_t>(quote),"Scene purchase bypassed actual Funds");
    return true;
}
std::optional<Id> try_buy(l::sim::World& world,l::sim::Command command,Json& commands) {
    if(!paid_command(world,command,commands))return {};
    const auto id=world.building_owner_at(command.cell);l::check(id.has_value(),"Accepted purchase has no actual paid body");return id;
}
Id build_fields(l::sim::World& world,Json& commands) {
    const auto deadline=world.ticks()+400;
    const l::base::Planned* selected=nullptr;Id farm{};
    while(!selected) {
        // These are existing paid plan sites, not source admission exceptions.
        // Prefer the two secondary Farms instead of requiring an occupied
        // primary habitat to clear before the city can begin.
        for(const unsigned plan_id:{103U,105U,101U}) {
            const auto& candidate=l::base::planned(plan_id);
            if(const auto id=try_buy(world,{candidate.command,candidate.cell},commands)) {
                selected=&candidate;farm=*id;break;
            }
            const auto reason=commands.back().at("result").get<std::string>();
            l::check(reason.find("actual wildlife, a Hunter or its begun edge")!=std::string::npos,
                "Planned paid Farm refused: plan="+std::to_string(plan_id)+" cell="+std::to_string(candidate.cell.x)+","+
                std::to_string(candidate.cell.y)+" reason="+reason);
        }
        if(selected)break;
        l::check(world.ticks()<deadline,"No planned Farm site naturally cleared within400 real ticks");world.tick();
    }
    const auto& farm_node=*selected;const auto& millet=l::base::planned(farm_node.plan_id+1);
    const auto plan=l::paid::plan_for(l::recipe);const auto reserved=l::paid::reserved_footprints(world,plan);
    // Buy before the existing tick0 grid. Its production probes preserve these
    // new paid claims; reserve every later city body and baseline road as well.
    for(const auto type:{l::sim::CommandType::PlaceOriginalMilletField,l::sim::CommandType::PlaceOriginalWheatField,l::sim::CommandType::PlaceOriginalCabbageField,
                         l::sim::CommandType::PlaceOriginalSoybeanField}) {
        bool placed=false;
        if(type==l::sim::CommandType::PlaceOriginalMilletField) {
            l::sim::Command preferred{type,millet.cell};preferred.building_id=farm;
            placed=try_buy(world,preferred,commands).has_value();
        }
        for(int y=farm_node.cell.y-3;y<=farm_node.cell.y+5&&!placed;++y)
            for(int x=farm_node.cell.x-3;x<=farm_node.cell.x+5&&!placed;++x) {
                const l::sim::Cell cell{x,y};
                if(reserved.contains({x,y})||std::find(l::base::roads.begin(),l::base::roads.end(),cell)!=l::base::roads.end())continue;
                l::sim::Command command{type,cell};command.building_id=farm;
                if(!world.validate(command).accepted)continue;
                placed=try_buy(world,command,commands).has_value();
            }
        l::check(placed,"No admitted paid crop site for command="+std::to_string(static_cast<unsigned>(type))+" at planned Farm="+
            std::to_string(farm_node.plan_id)+" cell="+std::to_string(farm_node.cell.x)+","+std::to_string(farm_node.cell.y));
    }
    l::check(!world.total_population()&&std::all_of(world.hunting_food_state()->fields.begin(),world.hunting_food_state()->fields.end(),
        [](const auto& field){return !field.harvested;}),"Startup waiting granted residents or unstaffed harvest");return farm;
}
Id mixed_farm(const l::sim::World& world) {
    for(const auto& farm:world.hunting_food_state()->farms) {
        std::set<l::sim::AgricultureCrop> crops;
        for(const auto& field:world.hunting_food_state()->fields)if(field.farm_id==farm.farm_id)crops.insert(field.crop);
        if(crops.size()>=3)return farm.farm_id;
    }
    throw std::runtime_error("Replay lacks an actual paid Farm bound to at least three crop kinds");
}
void delayed_grid(l::sim::World& world,Json& commands) {
    if(!world.ticks())return; // The unchanged shared recipe owns tick0's grid.
    const auto plan=l::paid::plan_for(l::recipe);const auto reserved=l::paid::reserved_footprints(world,plan);
    // Same finite authored city rectangle and shared future-footprint helper.
    // A delayed start pays its roads at the actual date; World validates every
    // cell and retains all wildlife/current or begun movement claims.
    for(int y=110;y<=143;++y)for(int x=108;x<=136;++x) {
        const l::sim::Cell cell{x,y};
        if(reserved.contains({x,y})||l::base::initial_road(world,cell))continue;
        const l::sim::Command command{l::sim::CommandType::PlaceRoad,cell};
        if(!world.validate(command).accepted)continue;
        (void)paid_command(world,command,commands);
    }
}
struct Observation {
    std::map<Id,l::sim::HuntingFoodCounters> receipts,consumption;
    std::map<Id,std::uint64_t> work;
    std::set<std::pair<l::sim::CourierId,std::uint64_t>> accepted_transfers;
    Json arrivals=Json::array(),meals=Json::array(),physical_arrivals=Json::array();
    std::uint64_t observed_work=0,staffed_ticks=0;
    void after(const l::sim::World& world,Id farm) {
        std::uint64_t delta=0;
        for(const auto& row:world.multi_field_agriculture_state()->fields) {
            auto& previous=work[row.field_id];l::check(row.productive_ticks>=previous,"Retained field work went backwards");
            if(row.farm_id==farm)delta+=row.productive_ticks-previous;previous=row.productive_ticks;
        }
        if(world.ticks())l::check(delta<=1,"An observed Farm created more than one work unit in a tick");observed_work+=delta;
        const auto required=world.workforce_required(farm);
        if(required>0&&world.workers_assigned(farm)==required)++staffed_ticks;
        for(const auto& row:world.hunting_food_state()->stocks) {
            const auto kind=world.building(row.building_id).kind;
            for(std::size_t g=0;g<7;++g) {
                auto& received=receipts[row.building_id][g];auto& eaten=consumption[row.building_id][g];
                l::check(row.received[g]>=received&&row.consumed[g]>=eaten,"Food lifetime counters went backwards");
                if(row.received[g]>received&&(kind==l::sim::Object::Market||l::sim::original_house_kind(kind)))
                    arrivals.push_back({{"tick",world.ticks()},{"owner",static_cast<unsigned>(row.building_id)},
                        {"market",kind==l::sim::Object::Market},{"resource",g},{"accepted",row.received[g]-received}});
                if(row.consumed[g]>eaten&&l::sim::original_house_kind(kind))
                    meals.push_back({{"tick",world.ticks()},{"house",static_cast<unsigned>(row.building_id)},
                        {"resource",g},{"portions",row.consumed[g]-eaten}});
                received=row.received[g];eaten=row.consumed[g];
            }
        }
        for(const auto& trip:world.hunting_food_state()->trips)
            if(std::any_of(trip.delivered.begin(),trip.delivered.end(),[](int amount){return amount>0;})&&
               accepted_transfers.emplace(trip.transfer.courier_id,trip.transfer.sequence).second)
                physical_arrivals.push_back({{"observed_tick",world.ticks()},{"source",static_cast<unsigned>(trip.source)},
                    {"target",static_cast<unsigned>(trip.target)},{"courier",static_cast<unsigned>(trip.transfer.courier_id)},
                    {"sequence",trip.transfer.sequence},{"accepted",trip.delivered}});
    }
    void seed(const l::sim::World& world) {
        for(const auto& row:world.multi_field_agriculture_state()->fields)work[row.field_id]=row.productive_ticks;
        for(const auto& row:world.hunting_food_state()->stocks){receipts[row.building_id]=row.received;consumption[row.building_id]=row.consumed;}
        for(const auto& trip:world.hunting_food_state()->trips)
            if(std::any_of(trip.delivered.begin(),trip.delivered.end(),[](int amount){return amount>0;}))
                accepted_transfers.emplace(trip.transfer.courier_id,trip.transfer.sequence);
    }
};
Json farm_report(const l::sim::World& world,Id farm) {
    Json fields=Json::array();const auto& food=*world.hunting_food_state();
    for(const auto& field:food.fields)if(field.farm_id==farm) {
        const auto at=std::find_if(world.multi_field_agriculture_state()->fields.begin(),world.multi_field_agriculture_state()->fields.end(),
            [&](const auto& work){return work.field_id==field.field_id;});
        fields.push_back({{"field",static_cast<unsigned>(field.field_id)},{"crop",static_cast<unsigned>(field.crop)},
            {"productive_ticks",at->productive_ticks},{"progress",world.agriculture_field_progress(field.field_id)},{"harvested",field.harvested},
            {"operating",world.building(field.field_id).operating_enabled}});
    }
    std::array<std::uint64_t,7> market_received{},house_received{},house_consumed{},farm_departed{},other_farm_harvests{};
    for(const auto& field:food.fields)if(field.farm_id!=farm)other_farm_harvests[static_cast<unsigned>(field.crop)]+=field.harvested;
    for(const auto& row:food.stocks)for(std::size_t g=0;g<7;++g) {
        const auto kind=world.building(row.building_id).kind;
        if(row.building_id==farm)farm_departed[g]+=row.departed[g];
        if(kind==l::sim::Object::Market)market_received[g]+=row.received[g];
        if(l::sim::original_house_kind(kind)){house_received[g]+=row.received[g];house_consumed[g]+=row.consumed[g];}
    }
    const auto cell=world.building(farm).cell;
    return {{"farm",static_cast<unsigned>(farm)},{"cell",{cell.x,cell.y}},
        {"assigned_workers",world.workers_assigned(farm)},{"required_workers",world.workforce_required(farm)},
        {"fields",fields},{"farm_departed",farm_departed},{"other_farm_harvests",other_farm_harvests},
        {"market_received",market_received},{"house_received",house_received},{"house_consumed",house_consumed}};
}
bool mixed_progress(const l::sim::World& world,Id farm) {
    unsigned active=0;std::set<int> phases;
    for(const auto& row:world.multi_field_agriculture_state()->fields)if(row.farm_id==farm) {
        const auto progress=world.agriculture_field_progress(row.field_id);active+=progress>0;phases.insert(progress);
    }
    return active>=3&&phases.size()>1;
}
bool loaded_edge(const l::sim::World& world,Id farm) {
    for(const auto& trip:world.hunting_food_state()->trips)if(trip.source==farm&&trip.phase==l::sim::OriginalGoodsTransferPhase::Loaded) {
        int cargo=0;for(std::size_t g=0;g<7;++g)cargo+=trip.loaded[g]-trip.delivered[g]-trip.returned[g];
        const auto& courier=world.courier(trip.transfer.courier_id);
        if(cargo>0&&courier.role==l::sim::CourierRole::OriginalFoodToMarket&&courier.edge_progress>0)return true;
    }
    return false;
}
}
int main(int argc,char** argv) {
    l::fs::path output;Json commands=Json::array();
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto options=parse(argc,argv);l::SourceGuard sources(options.data);Context context(options.data);
        l::check(context.source.model->provenance().sha256==l::base::model_sha&&
            context.source.terrain->source_map_sha256()==l::base::map_sha,
            "Positive acceptance requires unchanged supplied Xia/Normal source identities");
        auto world=options.replay.empty()?context.create():context.restore(l::save::read_save(options.replay));
        l::fs::create_directories(options.output);output=options.output;
        Json blocked=nullptr,manifest=Json::array();Id farm{};bool recipe_ok=true;const auto startup_begin=world.ticks();
        if(options.replay.empty()){farm=build_fields(world,commands);delayed_grid(world,commands);recipe_ok=l::apply_recipe(world,commands,blocked);}
        else farm=mixed_farm(world);
        const auto begin=world.ticks();std::uint64_t end=begin+options.ticks;
        std::optional<l::save::SaveDocument> expected;
        if(!options.expected.empty()) {
            expected=l::save::read_save(options.expected);(void)context.restore(*expected);end=expected->world.ticks;
            l::check(end>=begin&&end-begin>=12000&&end-begin<=100000,"Separate replay must cover12000..100000 real ticks");
        }
        checkpoint(context,world,output,"seed",manifest);auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
        Observation observation;observation.seed(world);std::optional<std::uint64_t> progress_tick,cargo_tick;
        while(recipe_ok&&world.ticks()<end) {
            Json repeated_commands=Json::array(),repeated_blocked=nullptr;
            recipe_ok=l::apply_recipe(world,commands,blocked);
            const auto repeated=l::apply_recipe(replay,repeated_commands,repeated_blocked);
            l::check(recipe_ok==repeated&&blocked==repeated_blocked,"Independent paid command decisions differ");if(!recipe_ok)break;
            world.tick();replay.tick();observation.after(world,farm);
            if(!progress_tick&&mixed_progress(world,farm)){checkpoint(context,world,output,"mid-progress",manifest);progress_tick=world.ticks();}
            if(!cargo_tick&&loaded_edge(world,farm)){checkpoint(context,world,output,"mid-cargo",manifest);cargo_tick=world.ticks();}
            if(world.ticks()%816==0){(void)conservation(world);l::check(world.snapshot()==replay.snapshot(),"Restored continuation differs");sources.unchanged();}
        }
        const auto balances=conservation(world);l::check(world.snapshot()==replay.snapshot(),"Terminal independent state differs");
        checkpoint(context,world,output,"final",manifest);bool independent=false;
        if(expected) {
            l::check(expected->world==world.snapshot(),"Separate creator/replay endpoint differs");
            l::check(l::q::f::read_text(options.expected)==l::q::f::read_text(output/"final.save.json"),"Separate creator/replay canonical bytes differ");independent=true;
        }
        l::save::RecoveryStore recovery(output/"recovery",options.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
        const auto final=l::save::read_save(output/"final.save.json");const auto history=recovery.create_history(final,context.source.map.mask);
        const auto entry=recovery.write_checkpoint(history,final,context.source.map.mask);
        l::check(entry.loadable()&&entry.schema==46&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost exact multi-field state");
        const auto farm_state=farm_report(world,farm);std::set<unsigned> productive;
        for(const auto& field:farm_state.at("fields"))if(field.at("harvested").get<std::uint64_t>()>0) {
            const auto crop=field.at("crop").get<unsigned>();
            if(!farm_state.at("other_farm_harvests")[crop].get<std::uint64_t>()&&farm_state.at("farm_departed")[crop].get<std::uint64_t>()&&
               farm_state.at("market_received")[crop].get<std::uint64_t>()&&farm_state.at("house_received")[crop].get<std::uint64_t>()&&
               farm_state.at("house_consumed")[crop].get<std::uint64_t>())productive.insert(crop);
        }
        sources.unchanged();const bool positive=recipe_ok&&world.ticks()-begin>=12000&&productive.size()>=3&&
            progress_tick.has_value()&&cargo_tick.has_value()&&observation.staffed_ticks>0;
        const auto status=positive?"PASS":"PARTIAL";
        Json report={{"format","openemperor-multi-field-agriculture-acceptance-v1"},{"status",status},{"rule",30},{"schema",46},
            {"map","Cities/Xia.map"},{"model_sha256",context.source.model->provenance().sha256},
            {"unchanged_supplied_model",true},{"source_bytes_mode_mtime_unchanged",true},{"snapshot_or_stock_injection",false},
            {"startup_begin_tick",startup_begin},{"startup_real_wait_ticks",begin-startup_begin},
            {"begin_tick",begin},{"end_tick",world.ticks()},{"real_ticks",world.ticks()-begin},{"paid_recipe",l::recipe},
            {"paid_recipe_without_permanent_abort",recipe_ok},{"first_permanent_command_blocker",blocked},
            {"multi_field_farm",farm_state},{"actual_crop_receipt_and_consumption_count",productive.size()},
            {"actual_crop_resources",productive},{"crop_contributor_provenance","Each counted crop has this Farm as its sole actual harvest origin, real Farm departure, Market/House receipts and House consumption."},
            {"observed_one_unit_farm_work",observation.observed_work},{"observed_fully_staffed_ticks",observation.staffed_ticks},
            {"actual_food_arrivals",observation.arrivals},{"physical_arrival_transfers",observation.physical_arrivals},
            {"actual_house_consumption",observation.meals},{"conservation",balances},
            {"mid_independent_progress_tick",progress_tick?Json(*progress_tick):Json(nullptr)},
            {"mid_loaded_cart_edge_tick",cargo_tick?Json(*cargo_tick):Json(nullptr)},{"checkpoints",manifest},
            {"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",independent},{"recovery_exact",true},
            {"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},
            {"claim_scope","Paid source-bound Rule30 Farm; at least three crop kinds actually harvested, received at Market/House and consumed over12000+ real ticks. AUTHORED shared labor, not Original crop timing parity."}};
        l::write(output/"commands.json",commands);l::write(output/"report.json",report);
        std::cout<<status<<" Rule30/schema46 paid multi-field Agriculture; ticks="<<world.ticks()-begin<<" crops="<<productive.size()
            <<" report="<<output/"report.json"<<'\n';return positive?0:2;
    } catch(const std::exception& error) {
        if(!output.empty()&&!l::fs::exists(output/"commands.json"))try{l::write(output/"commands.json",commands);}catch(...){}
        if(!output.empty()&&!l::fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()}});}catch(...){}
        std::cerr<<"Multi-field Agriculture acceptance FAIL: "<<error.what()<<'\n';return 1;
    }
}
