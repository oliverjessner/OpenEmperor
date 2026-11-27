// AGRI-MAC04: unchanged supplied model, paid Rule29 progression and evidence.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/CheckpointRecoveryProof.h"

#include <charconv>
#include <iostream>
#include <openssl/evp.h>

namespace l=oe_luxury_progression;
namespace {
constexpr const char* usage="openemperor-luxury-progression-acceptance --data ABS --output FRESH_PRIVATE_SUBDIR [--ticks 30000] [--hold 20400] [--input EXACT_SAVE --expected CREATOR_FINAL_SAVE] [--latest-agriculture [--latest-plan-v2] [--checkpoint-recovery] [--irrigated-harvest | --quality-aware-food-dispatch]] (default Rule29/schema45: .local/agri-mac04; latest Rule36/schema52 or explicit Rule37/schema53 or Rule38/schema54: .local/agri-mac14)";
struct Options {l::fs::path data,output,input,expected;unsigned ticks=30000,hold=20400;bool latest_agriculture=false,latest_plan_v2=false,checkpoint_recovery=false,irrigated_harvest=false,quality_aware_food_dispatch=false;};
Options parse(int argc,char** argv) {
    Options result;std::map<std::string,std::string> arguments;
    for(int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if(key=="--latest-agriculture") {
            l::check(!result.latest_agriculture,"Duplicate acceptance argument");result.latest_agriculture=true;continue;
        }
        if(key=="--latest-plan-v2") {
            l::check(!result.latest_plan_v2,"Duplicate acceptance argument");result.latest_plan_v2=true;continue;
        }
        if(key=="--checkpoint-recovery") {
            l::check(!result.checkpoint_recovery,"Duplicate acceptance argument");result.checkpoint_recovery=true;continue;
        }
        if(key=="--quality-aware-food-dispatch") {
            l::check(!result.quality_aware_food_dispatch,"Duplicate acceptance argument");result.quality_aware_food_dispatch=true;continue;
        }
        if(key=="--irrigated-harvest") {
            l::check(!result.irrigated_harvest,"Duplicate acceptance argument");result.irrigated_harvest=true;continue;
        }
        l::check(i+1<argc,usage);
        l::check(key=="--data"||key=="--output"||key=="--ticks"||key=="--hold"||key=="--input"||key=="--expected",usage);
        l::check(arguments.emplace(key,argv[++i]).second,"Duplicate acceptance argument");
    }
    l::check(!result.latest_plan_v2||result.latest_agriculture,"--latest-plan-v2 requires --latest-agriculture");
    l::check(!result.checkpoint_recovery||result.latest_agriculture,"--checkpoint-recovery requires --latest-agriculture");
    l::check(!result.irrigated_harvest||result.latest_agriculture,"--irrigated-harvest requires --latest-agriculture");
    l::check(!result.quality_aware_food_dispatch||result.latest_agriculture,"--quality-aware-food-dispatch requires --latest-agriculture");
    l::check(!(result.irrigated_harvest&&result.quality_aware_food_dispatch),"Choose one explicit Agriculture successor");
    l::check(arguments.contains("--data")&&arguments.contains("--output"),usage);
    result.data=l::fs::canonical(arguments.at("--data"));
    if(result.latest_agriculture) {
        l::no_symlinks(arguments.at("--output"));
        result.output=l::fs::weakly_canonical(l::fs::absolute(arguments.at("--output")));
        const auto allowed=l::fs::weakly_canonical(l::fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac14");
        l::check(result.output!=allowed&&l::below(result.output,allowed)&&!l::below(result.output,result.data)&&
            !l::below(result.data,result.output)&&!l::fs::exists(result.output),
            "Use a fresh .local/agri-mac14 subdirectory disjoint from Original data");
        l::save::validate_save_target(result.output/"final.save.json",result.data);
    } else result.output=l::output_root(arguments.at("--output"),result.data);
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
std::string checkpoint_sha256(const l::fs::path& path) {
    l::no_symlinks(path);
    l::check(l::fs::is_regular_file(path)&&l::fs::file_size(path)<=16U*1024U*1024U,"Bounded regular QA replay artifact required");
    const auto bytes=l::q::f::read_text(path);std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned length=0;
    l::check(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)==1&&length==32,"QA checkpoint SHA256 failed");
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
    for(unsigned i=0;i<length;++i) {result.push_back(hex[digest[i]>>4U]);result.push_back(hex[digest[i]&15U]);}
    return result;
}
struct ReplayPlanGuard {
    struct Input {l::fs::path path;std::string sha256;l::fs::perms mode;l::fs::file_time_type modified;};
    std::vector<Input> inputs;
    void retain(const l::fs::path& path) {
        if(std::any_of(inputs.begin(),inputs.end(),[&](const auto& row){return row.path==path;}))return;
        inputs.push_back({path,checkpoint_sha256(path),l::fs::status(path).permissions(),l::fs::last_write_time(path)});
    }
    void verify(const l::fs::path& path,const l::save::SaveDocument& document,bool latest_plan_v2,std::string_view model_sha256,bool irrigated_harvest=false,bool quality_aware_food_dispatch=false) {
        const auto report_path=path.parent_path()/"report.json";
        if(!latest_plan_v2) {
            // Legacy replay inputs do not acquire a new sidecar requirement.
            // Reject a known V2 creator rather than silently changing its plan.
            std::error_code error;const auto status=l::fs::symlink_status(report_path,error);
            if(error||!l::fs::is_regular_file(status)||l::fs::file_size(report_path)>16U*1024U*1024U)return;
            l::no_symlinks(report_path);
            const auto report=l::Json::parse(l::q::f::read_text(report_path),nullptr,false);
            if(report.is_object()&&report.contains("recipe")&&report.at("recipe").is_string())
                l::check(report.at("recipe").get<std::string>()!=l::latest_v2_recipe,"V2 creator replay requires --latest-plan-v2");
            return;
        }
        retain(path);retain(report_path);
        const auto report=l::Json::parse(l::q::f::read_text(report_path));
        l::check(report.is_object()&&report.value("format",std::string{})=="openemperor-latest-agriculture-luxury-v2"&&
            report.value("recipe",std::string{})==l::latest_v2_recipe&&report.value("rule",0U)==(quality_aware_food_dispatch?38U:irrigated_harvest?37U:36U)&&report.value("schema",0U)==(quality_aware_food_dispatch?54U:irrigated_harvest?53U:52U)&&
            report.value("map",std::string{})=="Cities/Xia.map"&&report.value("model_sha256",std::string{})==model_sha256&&
            report.contains("snapshot_or_stock_injection")&&report.at("snapshot_or_stock_injection")==false,
            "V2 replay requires matching explicit creator QA plan provenance; saves retain their own authority");
        if(quality_aware_food_dispatch)l::check(report.at("quality_aware_food_dispatch")==l::sim::quality_aware_food_dispatch_contract&&
            report.at("quality_aware_food_dispatch_revision")==l::sim::quality_aware_food_dispatch_revision,
            "Rule38 V2 replay requires its exact explicit contract metadata");
        if(irrigated_harvest||quality_aware_food_dispatch)l::check(report.at("irrigated_harvest")==l::sim::irrigated_harvest_contract&&
            report.at("irrigated_harvest_revision")==l::sim::irrigated_harvest_revision,
            "Rule37 V2 replay requires its exact explicit contract metadata");
        const auto& checkpoints=report.at("checkpoints");
        l::check(checkpoints.is_array()&&checkpoints.size()<=16,"Bounded V2 creator checkpoint manifest required");
        unsigned matches=0;const auto hash=checkpoint_sha256(path);
        for(const auto& row:checkpoints)if(row.is_object()&&row.value("file",std::string{})==path.filename().string()) {
            l::check(row.at("tick")==document.world.ticks&&row.value("sha256",std::string{})==hash,"V2 replay checkpoint bytes/tick do not match selected creator plan");
            ++matches;
        }
        l::check(matches==1,"V2 replay requires one exact creator checkpoint record");
    }
    void unchanged()const {
        for(const auto& input:inputs)l::check(checkpoint_sha256(input.path)==input.sha256&&
            l::fs::status(input.path).permissions()==input.mode&&l::fs::last_write_time(input.path)==input.modified,
            "V2 creator QA metadata or replay input changed during the run");
    }
};
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
    if(context.latest_agriculture) {
        if(!document.world.processed_food->fields.empty())reject([](auto& state){++state.processed_food->fields.front().harvested;});
    } else if(!document.world.hunting_food->fields.empty())reject([](auto& state){++state.hunting_food->fields.front().harvested;});
    l::save::RecoveryStore recovery(output/"recovery",context.map.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(document,context.map.mask);
    const auto entry=recovery.write_checkpoint(history,document,context.map.mask);
    l::check(entry.loadable()&&entry.schema==context.schema()&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost exact selected luxury generation");
    return {{"hostile_controls",rejected},{"rejection_preserved_existing_save",true},{"recovery_exact",true}};
}
}
int main(int argc,char** argv) {
    l::fs::path output;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto options=parse(argc,argv);l::SourceGuard sources(options.data);l::Context context(options.data,options.latest_agriculture,options.irrigated_harvest,options.quality_aware_food_dispatch);
        const auto loaded=l::orig::load_model_data(options.data);l::check(bool(loaded),"Cannot independently parse supplied model");
        const l::orig::HouseEvolutionRules independent(*loaded.model,l::orig::Difficulty::Normal);
        l::check(independent.model_sha256()==context.model->provenance().sha256,"Independent model identity differs");
        l::check(independent.model_sha256()==l::base::model_sha&&context.terrain->source_map_sha256()==l::base::map_sha,
            "MAC04 positive acceptance requires unchanged supplied Xia/Normal source identities");
        ReplayPlanGuard replay_plan;
        if(options.latest_plan_v2&&!options.input.empty()) {replay_plan.retain(options.input);replay_plan.retain(options.expected);}
        std::optional<l::save::SaveDocument> input_document;
        if(!options.input.empty())input_document=l::save::read_save(options.input);
        auto world=input_document?context.restore(*input_document):context.world();
        std::optional<l::save::SaveDocument> expected_document;
        if(!options.expected.empty()) {
            expected_document=l::save::read_save(options.expected);(void)context.restore(*expected_document);
            l::check(expected_document->world.ticks>world.ticks()&&expected_document->world.ticks-world.ticks()<=100000,
                "Expected replay endpoint outside bound");
        }
        if(options.latest_agriculture&&input_document) {
            replay_plan.verify(options.input,*input_document,options.latest_plan_v2,independent.model_sha256(),options.irrigated_harvest,options.quality_aware_food_dispatch);
            replay_plan.verify(options.expected,*expected_document,options.latest_plan_v2,independent.model_sha256(),options.irrigated_harvest,options.quality_aware_food_dispatch);
        }
        input_document.reset();
        l::fs::create_directories(options.output);output=options.output;
        l::Json commands=l::Json::array(),blocked=nullptr,manifest=l::Json::array();bool recipe_ok=true;
        std::unique_ptr<oe_checkpoint_recovery::Proof> event_recovery;
        if(options.checkpoint_recovery)event_recovery=std::make_unique<oe_checkpoint_recovery::Proof>(output,options.data);
        const auto checkpoint=[&](const char* label) {
            l::checkpoint(context,world,output,label,manifest);
            if(options.latest_plan_v2)manifest.back()["sha256"]=checkpoint_sha256(output/manifest.back().at("file").get<std::string>());
            if(event_recovery) {
                const auto path=output/manifest.back().at("file").get<std::string>();
                event_recovery->capture(label,l::save::read_save(path),context.map.mask,world,path,
                    [&](const auto& document){return context.restore(document);});
            }
        };
        if(options.input.empty())recipe_ok=l::apply_recipe(world,commands,blocked,options.latest_plan_v2);
        checkpoint("seed");const auto initial=l::city_report(world);
        const auto begin=world.ticks();l::Observation observation;observation.minimum_funds=world.treasury();std::optional<std::uint64_t> cargo_tick;
        std::array<bool,3> house_receipts{};std::size_t arrival_at=0;
        auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
        std::uint64_t end=begin+options.ticks;
        if(expected_document)end=expected_document->world.ticks;
        while(recipe_ok&&world.ticks()<end) {
            l::Json replay_commands=l::Json::array(),replay_blocked=nullptr;
            recipe_ok=l::apply_recipe(world,commands,blocked,options.latest_plan_v2);
            const auto repeated=l::apply_recipe(replay,replay_commands,replay_blocked,options.latest_plan_v2);
            l::check(repeated==recipe_ok&&replay_blocked==blocked,"Restored paid player command decisions diverged");
            if(!recipe_ok)break;
            observation.before(world);world.tick();replay.tick();observation.after(world);
            if(event_recovery) {
                constexpr std::array labels{"house-tea-receipt","house-silk-receipt","house-lacquerware-receipt"};
                for(;arrival_at<observation.deliveries.size();++arrival_at) {
                    const auto& arrival=observation.deliveries[arrival_at];
                    if(!l::sim::original_house_kind(static_cast<l::sim::Object>(arrival.at("target_kind").get<unsigned>()))||
                       arrival.at("arrived_units").get<int>()<=0)continue;
                    for(std::size_t i=0;i<l::luxury_goods.size();++i)if(!house_receipts[i]&&
                        arrival.at("good")==static_cast<unsigned>(l::luxury_goods[i])) {
                        l::check(arrival.at("tick")==world.ticks(),"Luxury receipt checkpoint missed the actual arrival epoch");
                        checkpoint(labels[i]);house_receipts[i]=true;
                    }
                }
            }
            l::check(world.ticks()==replay.ticks(),"Replay clock differs");
            if(world.ticks()%816==0) {l::valid(world);l::check(world.snapshot()==replay.snapshot(),"Independent restored continuation diverged");}
            if(!cargo_tick)for(const auto& trip:world.original_goods_state()->reservations)
                if(std::find(l::luxury_goods.begin(),l::luxury_goods.end(),trip.good)!=l::luxury_goods.end()&&trip.phase==l::sim::OriginalGoodsTransferPhase::Loaded&&
                    trip.loaded>trip.delivered+trip.returned&&world.courier(trip.transfer.courier_id).edge_progress>0) {
                    checkpoint("mid-cargo");cargo_tick=world.ticks();break;
                }
            if(options.expected.empty()&&observation.maturity) {
                end=*observation.maturity+options.hold;
                if(world.ticks()==*observation.maturity)checkpoint("maturity");
            } else if(options.checkpoint_recovery&&observation.maturity&&world.ticks()==*observation.maturity) {
                checkpoint("maturity");
            }
        }
        l::valid(world);l::check(world.snapshot()==replay.snapshot(),"Terminal independent continuation differs");
        checkpoint("final");
        bool separate_replay=false;
        if(!options.expected.empty()) {
            l::check(expected_document->world==world.snapshot(),"Separate creator/restore endpoint differs");
            l::check(l::q::f::read_text(options.expected)==l::q::f::read_text(output/"final.save.json"),"Separate creator/restore canonical save bytes differ");separate_replay=true;
        }
        const auto persistence=hostile_and_recovery(context,world,output);sources.unchanged();replay_plan.unchanged();
        bool positive=std::all_of(observation.earned.begin(),observation.earned.end(),[](bool value){return value;});
        if(options.latest_agriculture)positive=positive&&observation.maturity&&
            world.ticks()-*observation.maturity>=options.hold&&observation.minimum_funds>=0;
        if(options.checkpoint_recovery)positive=positive&&std::all_of(house_receipts.begin(),house_receipts.end(),[](bool value){return value;});
        const auto status=positive&&recipe_ok?"PASS":"PARTIAL";
        l::Json transient_refusals=l::Json::array();for(const auto& command:commands)
            if(command.value("retry_after_real_ticks",false))transient_refusals.push_back(command);
        const auto remaining=l::remaining_plan(world,options.latest_plan_v2);
        l::Json report={{"format","openemperor-luxury-progression-v1"},{"status",status},{"rule",context.rule()},{"schema",context.schema()},
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
        if(options.latest_agriculture) {
            report["format"]="openemperor-latest-agriculture-luxury-v1";
            report["recipe"]="authored-latest-agriculture-xia-compact-luxury-v1";
            report["local_omitted_plan_ids"]=l::Json::array({13,15});
            for(const auto id:l::latest_omitted_plan_ids)report["local_omitted_plan_ids"].push_back(id);
            report["local_omission_reason"]="Finite paid player plan: omit the historically unbought wildlife-blocked Farm, second Warehouse/Clay/Pottery pair, and paused duplicate Daoist/Buddhist shrines to retain the unchanged58-channel budget with physical Food/Hemp Growers. Any changed DES and unmet needs remain actual report observations.";
            report["claim_scope"]="Paid Rule36/schema52 finite supplied-model city, authored seasons and physical Food/Hemp Growers. Exact historical modes remain separate; no Original parity or universal retention claim.";
            const auto channels=world.couriers().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size();
            report["resource_bounds"]={{"bodies",world.buildings().size()},{"channels",channels},{"body_limit",82},{"channel_limit",58},
                {"research_navigation_tracks",world.grower_navigation_tracks().size()},{"research_navigation_session",world.has_grower_navigation_session()}};
        }
        if(options.latest_plan_v2) {
            report["format"]="openemperor-latest-agriculture-luxury-v2";report["recipe"]=l::latest_v2_recipe;
            report["local_omitted_plan_ids"]=l::Json::array({13,15});
            for(const auto id:l::latest_v2_omitted_plan_ids)report["local_omitted_plan_ids"].push_back(id);
            report["local_omission_reason"]="Explicit V2 paid player plan: retain Daoist20/Buddhist22 and Market143, omit Market125 plus the historically unbought wildlife-blocked Farm and second Warehouse/Clay/Pottery pair. Add exactly two ordinary paid Wheat fields, with actual parent binding and prepared physical Grower routes; costs, limits and source admission remain unchanged.";
            report["v2_extra_paid_wheat"]=l::latest_v2_wheat_report(world);
            report["claim_scope"]="Paid Rule36/schema52 V2 finite supplied-model city. PASS requires actual Tea/Silk/Lacquerware consumption, the requested post-maturity hold and nonnegative observed Funds. Devolutions, unmet needs and seasonal Food gaps remain visible; no uninterrupted Elite17, citywide reliability, all-Food/meal integration or Original parity claim. Checkpoint digests bind QA plan selection only, never saved gameplay authority.";
        }
        if(options.irrigated_harvest||options.quality_aware_food_dispatch) {
            report["policy"]=3;
            report["irrigated_harvest"]=l::sim::irrigated_harvest_contract;
            report["irrigated_harvest_revision"]=l::sim::irrigated_harvest_revision;
            report["claim_scope"]="Explicit AUTHORED Rule37/schema53, using the same paid latest-Agriculture player recipe and actual arrivals/consumption. No migration, Original parity, uninterrupted highest-tier retention or human acceptance claim.";
        }
        if(options.quality_aware_food_dispatch) {
            report["quality_aware_food_dispatch"]=l::sim::quality_aware_food_dispatch_contract;
            report["quality_aware_food_dispatch_revision"]=l::sim::quality_aware_food_dispatch_revision;
            report["claim_scope"]="Explicit AUTHORED Rule38/schema54 quality-aware Food dispatch, using the same paid latest-Agriculture player recipe and actual arrivals/consumption. No migration, Original parity, uninterrupted highest-tier retention or human acceptance claim.";
        }
        if(event_recovery)report["checkpoint_recovery"]=event_recovery->report();
        l::write(output/"commands.json",commands);l::write(output/"report.json",report);
        std::cout<<status<<" Rule"<<context.rule()<<"/schema"<<context.schema()<<" luxury progression; tick="<<world.ticks()<<" population="<<world.total_population()
            <<" Tea/Silk/Lacquerware="<<observation.earned[0]<<'/'<<observation.earned[1]<<'/'<<observation.earned[2]<<" report="<<output/"report.json"<<'\n';return positive&&recipe_ok?0:2;
    } catch(const std::exception& error) {
        if(!output.empty()&&!l::fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()}});}catch(...){}
        std::cerr<<"Luxury progression FAIL: "<<error.what()<<'\n';return 1;
    }
}
