// AGRI-MAC04: unchanged supplied model, paid Rule29 progression and evidence.
#include "../tests/LuxuryProgressionFixture.h"

#include <charconv>
#include <iostream>

namespace l=oe_luxury_progression;
namespace {
constexpr const char* usage="openemperor-luxury-progression-acceptance --data ABS --output FRESH_.local/agri-mac04/SUBDIR [--ticks 30000] [--hold 20400] [--input EXACT_RULE29_SAVE --expected CREATOR_FINAL_SAVE]";
struct Options {l::fs::path data,output,input,expected;unsigned ticks=30000,hold=20400;};
Options parse(int argc,char** argv) {
    Options result;std::map<std::string,std::string> arguments;
    for(int i=1;i<argc;i+=2) {
        l::check(i+1<argc,usage);const std::string key=argv[i];
        l::check(key=="--data"||key=="--output"||key=="--ticks"||key=="--hold"||key=="--input"||key=="--expected",usage);
        l::check(arguments.emplace(key,argv[i+1]).second,"Duplicate acceptance argument");
    }
    l::check(arguments.contains("--data")&&arguments.contains("--output"),usage);
    result.data=l::fs::canonical(arguments.at("--data"));result.output=l::output_root(arguments.at("--output"),result.data);
    for(const auto& [name,destination]:{std::pair{"--input",&result.input},{"--expected",&result.expected}})
        if(arguments.contains(name)) {
            *destination=l::fs::absolute(arguments.at(name));l::no_symlinks(*destination);
            l::check(l::fs::is_regular_file(*destination)&&l::fs::file_size(*destination)<=16U*1024U*1024U,"Bounded explicit replay file required");
        }
    l::check(result.input.empty()==result.expected.empty(),"Replay requires both --input and --expected");
    for(const auto& [name,destination]:{std::pair{"--ticks",&result.ticks},{"--hold",&result.hold}})if(arguments.contains(name)) {
        const auto& value=arguments.at(name);const auto parsed=std::from_chars(value.data(),value.data()+value.size(),*destination);
        l::check(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&*destination<=100000,"Tick bound must be an integer in0..100000");
    }
    return result;
}
template<class Action> void refuses(Action action) {
    bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}l::check(rejected,"Hostile authority was admitted");
}
l::Json hostile_and_recovery(const l::Context& context,const l::sim::World& world,const l::fs::path& output) {
    const auto protected_path=output/"final.save.json";const auto document=l::save::read_save(protected_path);
    l::check(context.restore(document).snapshot()==world.snapshot(),"Unedited hostile baseline must restore before mutations");
    const auto protected_bytes=l::q::f::read_text(protected_path);unsigned rejected=0;
    const auto reject=[&](auto edit) {
        auto bad=document;edit(bad.world);
        refuses([&]{(void)context.restore(bad);});refuses([&]{l::save::write_save(protected_path,bad,context.map.data,context.map.mask);});
        l::check(l::q::f::read_text(protected_path)==protected_bytes&&world.snapshot()==document.world,"Rejected hostile write mutated authority");++rejected;
    };
    reject([](auto& state){++state.treasury;});
    reject([](auto& state){++state.original_goods->totals[static_cast<unsigned>(l::orig::Goods::Tea)].consumed;});
    reject([](auto& state){state.rule_version=28;});
    if(!document.world.hunting_food->fields.empty())reject([](auto& state){++state.hunting_food->fields.front().harvested;});
    l::save::RecoveryStore recovery(output/"recovery",context.map.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(document,context.map.mask);
    const auto entry=recovery.write_checkpoint(history,document,context.map.mask);
    l::check(entry.loadable()&&entry.schema==45&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost exact Rule29 state");
    return {{"hostile_controls",rejected},{"rejection_preserved_existing_save",true},{"recovery_exact",true}};
}
}
int main(int argc,char** argv) {
    l::fs::path output;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto options=parse(argc,argv);l::SourceGuard sources(options.data);l::Context context(options.data);
        const auto loaded=l::orig::load_model_data(options.data);l::check(bool(loaded),"Cannot independently parse supplied model");
        const l::orig::HouseEvolutionRules independent(*loaded.model,l::orig::Difficulty::Normal);
        l::check(independent.model_sha256()==context.model->provenance().sha256,"Independent model identity differs");
        l::check(independent.model_sha256()==l::base::model_sha&&context.terrain->source_map_sha256()==l::base::map_sha,
            "MAC04 positive acceptance requires unchanged supplied Xia/Normal source identities");
        auto world=options.input.empty()?context.world():context.restore(l::save::read_save(options.input));
        std::optional<l::save::SaveDocument> expected_document;
        if(!options.expected.empty()) {
            expected_document=l::save::read_save(options.expected);(void)context.restore(*expected_document);
            l::check(expected_document->world.ticks>world.ticks()&&expected_document->world.ticks-world.ticks()<=100000,
                "Expected replay endpoint outside bound");
        }
        l::fs::create_directories(options.output);output=options.output;
        l::Json commands=l::Json::array(),blocked=nullptr,manifest=l::Json::array();bool recipe_ok=true;
        if(options.input.empty())recipe_ok=l::apply_recipe(world,commands,blocked);
        l::checkpoint(context,world,output,"seed",manifest);const auto initial=l::city_report(world);
        const auto begin=world.ticks();l::Observation observation;observation.minimum_funds=world.treasury();std::optional<std::uint64_t> cargo_tick;
        auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
        std::uint64_t end=begin+options.ticks;
        if(expected_document)end=expected_document->world.ticks;
        while(recipe_ok&&world.ticks()<end) {
            l::Json replay_commands=l::Json::array(),replay_blocked=nullptr;
            recipe_ok=l::apply_recipe(world,commands,blocked);
            const auto repeated=l::apply_recipe(replay,replay_commands,replay_blocked);
            l::check(repeated==recipe_ok&&replay_blocked==blocked,"Restored paid player command decisions diverged");
            if(!recipe_ok)break;
            observation.before(world);world.tick();replay.tick();observation.after(world);
            l::check(world.ticks()==replay.ticks(),"Replay clock differs");
            if(world.ticks()%816==0) {l::valid(world);l::check(world.snapshot()==replay.snapshot(),"Independent restored continuation diverged");}
            if(!cargo_tick)for(const auto& trip:world.original_goods_state()->reservations)
                if(std::find(l::luxury_goods.begin(),l::luxury_goods.end(),trip.good)!=l::luxury_goods.end()&&trip.phase==l::sim::OriginalGoodsTransferPhase::Loaded&&
                    trip.loaded>trip.delivered+trip.returned&&world.courier(trip.transfer.courier_id).edge_progress>0) {
                    l::checkpoint(context,world,output,"mid-cargo",manifest);cargo_tick=world.ticks();break;
                }
            if(options.expected.empty()&&observation.maturity) {
                end=*observation.maturity+options.hold;
                if(world.ticks()==*observation.maturity)l::checkpoint(context,world,output,"maturity",manifest);
            }
        }
        l::valid(world);l::check(world.snapshot()==replay.snapshot(),"Terminal independent continuation differs");
        l::checkpoint(context,world,output,"final",manifest);
        bool separate_replay=false;
        if(!options.expected.empty()) {
            l::check(expected_document->world==world.snapshot(),"Separate creator/restore endpoint differs");
            l::check(l::q::f::read_text(options.expected)==l::q::f::read_text(output/"final.save.json"),"Separate creator/restore canonical save bytes differ");separate_replay=true;
        }
        const auto persistence=hostile_and_recovery(context,world,output);sources.unchanged();
        const bool positive=std::all_of(observation.earned.begin(),observation.earned.end(),[](bool value){return value;});
        const auto status=positive&&recipe_ok?"PASS":"PARTIAL";
        l::Json transient_refusals=l::Json::array();for(const auto& command:commands)
            if(command.value("retry_after_real_ticks",false))transient_refusals.push_back(command);
        const auto remaining=l::remaining_plan(world);
        l::Json report={{"format","openemperor-luxury-progression-v1"},{"status",status},{"rule",29},{"schema",45},
            {"recipe",l::recipe},{"map","Cities/Xia.map"},{"model_sha256",independent.model_sha256()},
            {"local_omitted_plan_ids",{13,15}},{"local_omission_reason","Two paused duplicate western schools; real wildlife and paid Tea retain the58-channel budget. Primary Drama, Confucian and Paper remain for natural Elite17 Tea progression."},
            {"unchanged_supplied_model",true},{"source_bytes_mode_mtime_unchanged",true},{"snapshot_or_stock_injection",false},
            {"begin_tick",begin},{"end_tick",world.ticks()},{"search_bound",options.ticks},{"requested_post_maturity_ticks",options.hold},
            {"maturity_tick",observation.maturity?l::Json(*observation.maturity):l::Json(nullptr)},
            {"actual_post_maturity_ticks",observation.maturity?world.ticks()-*observation.maturity:0},
            {"paid_recipe_without_permanent_abort",recipe_ok},{"first_permanent_command_blocker",blocked},
            {"recorded_transient_refusals",transient_refusals},{"remaining_plan_nodes",remaining},{"all_planned_purchases_completed",remaining.empty()},
            {"positive_luxury_consumption",observation.earned},{"luxury_order",{"Tea","Silk","Lacquerware"}},
            {"minimum_funds",observation.minimum_funds},{"house_devolutions",observation.devolutions},{"burning_house_ticks",observation.burning_house_ticks},
            {"post_maturity",{{"house_devolutions",observation.post_maturity_devolutions},{"burning_house_ticks",observation.post_maturity_burning_house_ticks},
                {"funds_at_maturity",observation.maturity_funds?l::Json(*observation.maturity_funds):l::Json(nullptr)},
                {"taxes",observation.maturity?world.taxes_collected_total()-observation.maturity_taxes:0},
                {"installed_upkeep",observation.maturity?world.maintenance_spent_total()-observation.maturity_upkeep:0}}},
            {"post_maturity_houses",observation.post_maturity_houses()},
            {"initial",initial},{"final",l::city_report(world)},{"actual_transitions",observation.transitions},
            {"actual_upstream_work_pickup_return_and_processor_debits",observation.upstream},
            {"actual_luxury_dispatches",observation.dispatches},{"actual_luxury_arrivals",observation.deliveries},{"actual_luxury_consumption",observation.consumption},
            {"mid_cargo_checkpoint_tick",cargo_tick?l::Json(*cargo_tick):l::Json(nullptr)},{"checkpoints",manifest},{"persistence",persistence},
            {"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",separate_replay},
            {"native_metal","NOT_RUN"},{"f5_f9_input","NOT_RUN; strict same persistence API exercised"},{"human_acceptance","NOT_RUN"},
            {"claim_scope","Paid Rule29 finite supplied-model city. Unmet current/next requirements and refused commands are evidence, never a universal impossibility claim."}};
        l::write(output/"commands.json",commands);l::write(output/"report.json",report);
        std::cout<<status<<" Rule29/schema45 luxury progression; tick="<<world.ticks()<<" population="<<world.total_population()
            <<" Tea/Silk/Lacquerware="<<observation.earned[0]<<'/'<<observation.earned[1]<<'/'<<observation.earned[2]<<" report="<<output/"report.json"<<'\n';return positive&&recipe_ok?0:2;
    } catch(const std::exception& error) {
        if(!output.empty()&&!l::fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()}});}catch(...){}
        std::cerr<<"Luxury progression FAIL: "<<error.what()<<'\n';return 1;
    }
}
