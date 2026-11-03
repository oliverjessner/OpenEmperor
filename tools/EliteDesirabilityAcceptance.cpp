// OE40 Gate C: actual paid commands and observations, never a projected tier.
// Reuse only the existing generic path/SDL helpers, not its Rule11/12 factories.
#define OE_SUSTAINABLE_PROOF_DIRECTORY "oe40"
#define main oe40_reused_sustainable_main
#include "SustainableEconomyAcceptance.cpp"
#undef main
#undef OE_SUSTAINABLE_PROOF_DIRECTORY
#include "../tests/EliteDesirabilityStrategy.h"
#include "../tests/EliteStabilityStrategy.h"
#include "../tests/MarketDistributionMetrics.h"
#include "../tests/EliteStabilityDiagnosis.h"
#include "../tests/EliteFireCoverageMetrics.h"
#include "../tests/EliteFireReplayDigest.h"
#include "../tests/CitywideReliabilityView.h"
#include <openssl/evp.h>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace oe40_cli {
namespace strategy=oe_elite_desirability_strategy;
namespace paid_strategy=oe_elite_stability_strategy;
namespace high=oe_high_tier_metrics;
namespace trace=oe_market_distribution_metrics;
namespace orig=oe::original;
bool food_diagnosis(std::string_view name) {return name=="oe47"||name=="oe48";}
bool city_diagnosis(std::string_view name) {return name=="oe46"||food_diagnosis(name);}
bool fire_diagnosis(std::string_view name) {return name=="oe45"||city_diagnosis(name);}
constexpr const char* help="elite-desirability-acceptance --mode create|resume|render --data ABS --output NEW_IGNORED_OE40_OR_OE43_DIR [--variant staged|all-paid|lean-med|lean-med-extra-markets] [--rule 14|15|16|17|18 (default14;16 requires diagnosis oe45, oe46, oe47 or oe48;17 requires oe46, oe47 or oe48;18 requires oe47 or oe48)] [--search-ticks 40000|60000] [--tick-digests sha256 (explicit OE45 Rule15/16 or OE46 Rule15/16/17 or OE47/OE48 Rule15/16/17/18 QA only)] [--input EARNED_EXACT_GENERATION_SAVE] [--expected CREATOR_RESULT_SAVE (resume)] [--resources ABS (render)] [--diagnosis oe44|oe45|oe46|oe47|oe48 (fresh matching diagnosis root only; render also requires explicit --rule)] [--paid-strategy road-grid|market-near|medical-near|medical-pair|watch-near|paper-buffer|early-elite|finance-staged|combined|early-religion|combined-religion|combined-services|combined-paper-usable|early-refined|combined-refined|early-stagger-watch|early-stagger-medical|early-stagger-all|early-adaptive-watch|early-stagger-medical-four-markets|oe45-grid|oe45-watch-early|oe45-watch-central|oe45-watch-west-door|oe45-medical-pair|oe45-market-near|oe45-warehouse-swap|oe45-grid-central|oe45-grid-watch-near|oe45-grid-watch-north|oe45-grid-watch-west|oe45-grid-balanced|oe45-grid-south-door|oe46-ancestor-music-swap|oe46-ancestor-acrobat-swap|oe46-ancestor-drama-swap|oe46-market-south|oe46-markets-four|oe46-ancestor-west|oe46-ancestor-clinic|oe46-ancestor-music-early|oe46-ancestor-music-four-markets|oe46-ancestor-music-south-door|oe47-south-door-three-markets|oe47-south-door-four-markets|oe47-south-door-market-south|oe47-south-door-early-third-food|oe47-south-door-early-all-food|oe47-south-door-early-markets|oe48-watch-north|oe48-watch-west|oe48-watch-balanced|oe48-watch-central|oe48-watch-corridor|oe48-watch-early|oe48-watch-phase1|oe48-watch-corridor-early|oe48-grid-wide|oe48-watch-priority|oe48-farm-swap|oe48-market-north|oe48-watch-phase1-early-third-food|oe48-watch-phase1-early-all-food|oe48-watch-phase1-early-market|oe48-watch-phase1-farm-swap|oe48-watch-phase1-third-food-early-market|oe48-watch-phase1-third-food-three-markets|oe48-watch-phase1-third-food-four-markets (OE48 recipes require diagnosis oe48; OE47 recipes require diagnosis oe47 or oe48; OE46 recipes require diagnosis oe46, oe47 or oe48; OE45 recipes require diagnosis oe45, oe46, oe47 or oe48; all require variant lean-med)]";
struct Options {std::string mode,variant="staged",diagnosis,paid_strategy;fs::path data,output,input,expected,resources;unsigned rule=14,search=40000;bool tick_digests=false;};
Options options(int argc,char** argv){
    Options a;std::map<std::string,std::string> values;
    for(int i=1;i<argc;i+=2){check(i+1<argc,help);const std::string key=argv[i];
        check(key=="--mode"||key=="--data"||key=="--output"||key=="--variant"||key=="--rule"||key=="--search-ticks"||key=="--input"||key=="--expected"||key=="--resources"||key=="--diagnosis"||key=="--paid-strategy"||key=="--tick-digests",help);
        check(values.emplace(key,argv[i+1]).second,"Duplicate Elite proof argument");}
    for(const auto* key:{"--mode","--data","--output"})check(values.contains(key),help);
    a.mode=values.at("--mode");check(a.mode=="create"||a.mode=="resume"||a.mode=="render",help);
    if(values.contains("--variant"))a.variant=values.at("--variant");check(strategy::known(a.variant),help);
    if(values.contains("--rule")) {
        const auto& value=values.at("--rule");const auto result=std::from_chars(value.data(),value.data()+value.size(),a.rule);
        check(result.ec==std::errc{}&&result.ptr==value.data()+value.size()&&(a.rule==14||a.rule==15||a.rule==16||a.rule==17||a.rule==18),help);
    }
    if(values.contains("--diagnosis")){a.diagnosis=values.at("--diagnosis");check((a.diagnosis=="oe44"||fire_diagnosis(a.diagnosis))&&(a.mode!="render"||values.contains("--rule")),help);}
    check(a.rule!=15||(!a.diagnosis.empty()&&values.contains("--rule")),"Rule15 requires explicit --rule 15 and selected diagnosis oe44, oe45 or oe46");
    check(a.rule!=16||(fire_diagnosis(a.diagnosis)&&values.contains("--rule")),"Rule16 requires explicit --rule 16 and --diagnosis oe45 or oe46");
    check(a.rule!=18||(food_diagnosis(a.diagnosis)&&values.contains("--rule")),"Rule18 requires explicit --rule 18 and --diagnosis oe47 or oe48");
    check(a.rule!=17||(city_diagnosis(a.diagnosis)&&values.contains("--rule")),"Rule17 requires explicit --rule 17 and --diagnosis oe46 or oe47");
    check(a.diagnosis!="oe46"||a.mode!="render"||a.rule==16||a.rule==17,"OE46 rendering requires explicit Rule16 or Rule17");
    check(a.diagnosis!="oe47"||a.mode!="render"||a.rule==16||a.rule==17||a.rule==18,"OE47 rendering requires explicit Rule16 or Rule17 or Rule18");
    check(a.diagnosis!="oe48"||a.mode!="render"||a.rule==16||a.rule==17||a.rule==18,"OE48 rendering requires explicit Rule16 or Rule17 or Rule18");
    if(values.contains("--tick-digests")) {
        check(values.at("--tick-digests")=="sha256"&&fire_diagnosis(a.diagnosis)&&a.mode!="render"&&
            (a.rule==15||a.rule==16||a.rule==17||a.rule==18),"Tick digests require explicit OE45 Rule15/16 or OE46 Rule15/16/17 simulation and sha256");
        a.tick_digests=true;
    }
    if(values.contains("--paid-strategy")) {
        a.paid_strategy=values.at("--paid-strategy");
        check(paid_strategy::known(a.paid_strategy)&&!a.diagnosis.empty()&&a.variant=="lean-med"&&a.mode!="render",help);
        check(!a.paid_strategy.starts_with("oe45-")||fire_diagnosis(a.diagnosis),"OE45 player experiments require --diagnosis oe45 or oe46");
        check(!paid_strategy::oe46_recipe(a.paid_strategy)||city_diagnosis(a.diagnosis),"OE46 player experiments require --diagnosis oe46 or oe47");
        check(!paid_strategy::oe47_recipe(a.paid_strategy)||food_diagnosis(a.diagnosis),"OE47 player experiments require --diagnosis oe47 or oe48");
        check(!paid_strategy::oe48_recipe(a.paid_strategy)||a.diagnosis=="oe48","OE48 player experiments require --diagnosis oe48");
    }
    for(const auto& [key,path]:{std::pair{"--data",&a.data},{"--output",&a.output},{"--input",&a.input},{"--expected",&a.expected},{"--resources",&a.resources}})
        if(values.contains(key))*path=fs::absolute(values.at(key));
    if(values.contains("--search-ticks")){const auto& text=values.at("--search-ticks");const auto parsed=std::from_chars(text.data(),text.data()+text.size(),a.search);
        check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&(a.search==40000||a.search==60000),help);}
    safe(a.data);safe(a.output);check(fs::is_directory(a.data),"Original installation missing");
    const auto diagnosis_root=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/
        (a.diagnosis=="oe48"?".local/original-economy/oe48":a.diagnosis=="oe47"?".local/original-economy/oe47":a.diagnosis=="oe46"?".local/original-economy/oe46":a.diagnosis=="oe45"?".local/original-economy/oe45":".local/original-economy/oe44");
    const auto prior_diagnosis_root=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/original-economy/oe44";
    const auto oe45_diagnosis_root=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/original-economy/oe45";
    const auto oe46_diagnosis_root=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/original-economy/oe46";
    const auto oe47_diagnosis_root=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/original-economy/oe47";
    const bool allowed_output=a.diagnosis.empty()?private_output(a.output):
        a.output!=diagnosis_root&&below(a.output,diagnosis_root);
    check(allowed_output&&!fs::exists(a.output),"Use a fresh ignored output in the selected OE40/OE43 or explicit matching diagnosis root");
    check(!below(a.output,a.data)&&!below(a.data,a.output),"Output overlaps original data");
    for(const auto& path:{a.input,a.expected})if(!path.empty()){
        safe(path);const bool allowed_input=below(path,private_root())||
            (!a.diagnosis.empty()&&path!=diagnosis_root&&below(path,diagnosis_root))||
            (fire_diagnosis(a.diagnosis)&&path!=prior_diagnosis_root&&below(path,prior_diagnosis_root))||
            (city_diagnosis(a.diagnosis)&&path!=oe45_diagnosis_root&&below(path,oe45_diagnosis_root))||
            (food_diagnosis(a.diagnosis)&&path!=oe46_diagnosis_root&&below(path,oe46_diagnosis_root))||
            (a.diagnosis=="oe48"&&path!=oe47_diagnosis_root&&below(path,oe47_diagnosis_root));
        check(allowed_input&&fs::is_regular_file(path)&&fs::file_size(path)<=8U*1024U*1024U&&!below(path,a.output),
            "Use bounded explicit protected OE40 or selected explicit diagnosis saves");
    }
    if(a.mode=="create")check(a.input.empty()&&a.expected.empty()&&a.resources.empty(),help);
    else check(!a.input.empty(),help);
    if(a.mode=="resume")check(!a.expected.empty()&&a.resources.empty(),"Resume requires the independent creator's expected result");
    if(a.mode=="render"){safe(a.resources);check(fs::is_directory(a.resources)&&a.expected.empty(),help);}
    q::save::validate_save_target(a.output/"report.json",a.data);return a;
}
std::string hash_file(const fs::path& path){
    check(fs::is_regular_file(path)&&!fs::is_symlink(path)&&fs::file_size(path)<=64U*1024U*1024U,"Bounded regular source file required");
    std::ifstream in(path,std::ios::binary);check(bool(in),"Cannot read source identity");
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    check(ctx&&EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)==1,"SHA256 setup failed");std::array<char,65536> buffer{};
    while(in){in.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto n=in.gcount();if(n>0)check(EVP_DigestUpdate(ctx.get(),buffer.data(),static_cast<std::size_t>(n))==1,"SHA256 update failed");}
    check(in.eof(),"Source read failed");std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned length=0;
    check(EVP_DigestFinal_ex(ctx.get(),digest.data(),&length)==1&&length==32,"SHA256 finish failed");std::ostringstream text;
    for(unsigned i=0;i<length;++i)text<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(digest[i]);return text.str();
}
Json source_pins(const fs::path& data){
    Json out=Json::object();for(const auto* name:{"Cities/Xia.map","Model/EmperorBuildingModels.txt","Emperor.exe"}){
        const auto file=data/name;safe(file);out[name]={{"sha256",hash_file(file)},{"bytes",fs::file_size(file)},
            {"mode",static_cast<unsigned>(fs::status(file).permissions())},{"mtime",fs::last_write_time(file).time_since_epoch().count()}};}
    check(out["Cities/Xia.map"]["sha256"]==strategy::map_sha&&out["Model/EmperorBuildingModels.txt"]["sha256"]==strategy::model_sha&&
        out["Emperor.exe"]["sha256"]=="6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e","Unchanged pinned Xia/Normal/EXE inputs required");return out;
}
sim::World restore14(const q::Context& c,const q::save::SaveDocument& d){
    check(d.source_schema_version==30&&d.world.rule_version==14,"Exact schema30/Rule14 save required");
    return q::save::restore_original_cardinal_water(d,c.data,c.mask,c.policy,c.model);
}
unsigned schema_for_rule(unsigned rule) {
    check(rule==14||rule==15||rule==16||rule==17||rule==18,"Exact QA Rule14 through Rule18 required");return rule==18?34U:rule==17?33U:rule==16?32U:rule==15?31U:30U;
}
sim::World restore_selected(const q::Context& c,const q::save::SaveDocument& d,unsigned rule) {
    check(d.source_schema_version==schema_for_rule(rule)&&d.world.rule_version==rule,"Exact selected QA rule/schema save required");
    if(rule==18)return q::save::restore_original_deadline_food_dispatch(d,c.data,c.mask,c.policy,c.model);
    if(rule==17)return q::save::restore_original_deadline_service_dispatch(d,c.data,c.mask,c.policy,c.model);
    if(rule==16)return q::save::restore_original_deadline_fire_dispatch(d,c.data,c.mask,c.policy,c.model);
    if(rule==15)return q::save::restore_original_inspector_coordination(d,c.data,c.mask,c.policy,c.model);
    return restore14(c,d);
}
void checkpoint14(const q::Context& c,const fs::path& output,const std::string& label,const sim::World& w,Json& manifest){
    check(!label.empty()&&label.size()<=80&&std::all_of(label.begin(),label.end(),[](unsigned char v){return (v>='a'&&v<='z')||(v>='0'&&v<='9')||v=='-';}),"Invalid bounded checkpoint label");
    q::authority(w);const auto path=output/(label+".save.json");check(below(path,output)&&!fs::exists(path),"Preserve existing contained checkpoint");
    q::save::write_save(path,c.document(w),c.data,c.mask);const auto bytes=q::f::read_text(path);const auto document=q::save::read_save(path);
    auto loaded=restore_selected(c,document,w.rule_version());check(document.world==w.snapshot()&&loaded.snapshot()==w.snapshot(),"Complete selected-generation snapshot restore differs");
    const auto canonical=output/(label+".roundtrip.json");q::save::write_save(canonical,c.document(loaded),c.data,c.mask);
    check(q::f::read_text(canonical)==bytes,"Complete canonical selected-generation SaveDocument bytes differ");
    write(output/(label+".snapshot.json"),Json::parse(bytes).at("world"));
    manifest.push_back({{"label",label},{"tick",w.ticks()},{"file",path.filename().string()}});
}
void citywide_routes(const Options& a,const sim::World& w,std::string_view boundary) {
    if(!city_diagnosis(a.diagnosis))return;
    const auto path=a.output/("oe46-"+std::string(boundary)+"-routes.json");
    check(!fs::exists(path),"Preserve previous OE46 prepared-route checkpoint");
    Json actors=Json::array();const auto edge=sim::profile_rules(w.profile(),w.rule_version()).courier_edge_ticks;
    for(const auto& actor:w.couriers()) {
        const bool moving=actor.enabled&&actor.phase!=sim::CourierPhase::IdleAtWorkshop&&!actor.route_pending&&
            !actor.path.empty()&&actor.path_vertex<actor.path.size();
        const auto remaining=moving?static_cast<std::uint64_t>(actor.path.size()-1-actor.path_vertex)*
            static_cast<std::uint64_t>(edge)-static_cast<std::uint64_t>(actor.edge_progress):0U;
        actors.push_back({{"courier",static_cast<unsigned>(actor.id)},{"owner",static_cast<unsigned>(actor.owner)},
            {"target",static_cast<unsigned>(actor.target)},{"phase",sim::courier_phase_name(actor.phase)},
            {"path",trace::cells(actor.path)},{"path_vertex",actor.path_vertex},{"edge_progress",actor.edge_progress},
            {"route_pending",actor.route_pending},{"dispatch_status",sim::courier_dispatch_status_name(w.courier_dispatch_status(actor.id).status)},
            {"uninterrupted_movement_endpoint_tick",moving?Json(w.ticks()+remaining):Json(nullptr)}});
    }
    write(path,{{"format","openemperor-citywide-prepared-routes-v1"},{"tick",w.ticks()},{"road_revision",w.road_revision()},
        {"routes",oe_elite_stability_diagnosis::Observation::prepared_routes(w)},{"actors",actors},
        {"movement_eta_is_not_a_future_visit_or_dispatch_guarantee",true},{"no_route_search_or_world_mutation",true}});
}
const sim::OriginalHousingTransition* entry17(const sim::World& w,sim::BuildingId target){
    const auto& proof=high::history(w,target).first_transitions[14];
    const bool valid=proof&&proof->evaluation.tick<=w.ticks()&&proof->kind==sim::OriginalHousingTransitionKind::Evolution&&proof->evaluation.input.type==orig::HouseType::ImpressiveCompound&&
        proof->evaluation.input.desirability>=80&&orig::needs_met(w.original_rules_model()->house_rules(),orig::house_row(proof->evaluation.input.type),proof->evaluation.input)&&
        orig::needs_met(w.original_rules_model()->house_rules(),orig::next_row(proof->evaluation.input.type),proof->evaluation.input)&&
        orig::evolve_step(w.original_rules_model()->house_rules(),proof->evaluation.input).type==orig::HouseType::HeavenlyCompound;
    return valid?&*proof:nullptr;
}
bool entered17(const sim::World& w,sim::BuildingId target){
    return entry17(w,target)&&high::actual_first_target_success(w,target,orig::HouseType::HeavenlyCompound);
}
Json compare_tick_digests(const Options& a,std::uint64_t begin,std::uint64_t end) {
    const auto creator=a.expected.parent_path()/"oe45-state-digests.csv";
    const auto replay=a.output/"oe45-state-digests.csv";
    safe(creator);safe(replay);
    for(const auto& path:{creator,replay})check(fs::is_regular_file(path)&&fs::file_size(path)<=32U*1024U*1024U,
        "Bounded independent complete tick-digest trace required");
    std::ifstream input(creator,std::ios::binary);std::string line,expected="tick,sha256\n";
    check(bool(std::getline(input,line))&&line=="tick,sha256","Invalid creator digest header");
    std::optional<std::uint64_t> previous;std::uint64_t rows=0,compared=0;
    while(std::getline(input,line)) {
        check(++rows<=262144&&line.size()<=85,"Creator tick trace exceeds bounded rows or line size");
        const auto comma=line.find(',');check(comma!=std::string::npos&&line.size()-comma-1==64,"Invalid creator digest row");
        std::uint64_t tick=0;const auto parsed=std::from_chars(line.data(),line.data()+comma,tick);
        check(parsed.ec==std::errc{}&&parsed.ptr==line.data()+comma&&
            (!previous||(*previous!=UINT64_MAX&&tick==*previous+1)),"Creator digest ticks are not consecutive");
        check(std::all_of(line.begin()+static_cast<std::ptrdiff_t>(comma+1),line.end(),
            [](unsigned char v){return (v>='0'&&v<='9')||(v>='a'&&v<='f');}),"Invalid creator SHA256");
        previous=tick;
        if(tick>=begin&&tick<=end){expected+=line+'\n';++compared;}
    }
    check(input.eof()&&compared==end-begin+1,"Creator digest trace omits an observed tick");
    check(q::f::read_text(replay)==expected,"Separate-process complete per-tick state digests differ");
    return {{"status","PASS"},{"begin_tick",begin},{"end_tick",end},{"compared_states",compared},
        {"complete_csv_bytes_equal",true},{"scope","Validated saved authority, including full fire history; derived caches remain outside this claim"}};
}
Json provenance(const sim::World& w,sim::BuildingId target){const auto value=w.original_influence_provenance(target);
    return {{"raw",value.raw_value},{"cardinal_bonus",value.cardinal_water_bonus},{"query",value.value},{"source_map_sha256",value.source_map_sha256},
        {"model_sha256",value.model_sha256},{"selected_cell",{value.selected_cell.x,value.selected_cell.y}}};}
void simulate(const Options& a){
    const auto pins=source_pins(a.data);const auto c=q::Context::real(a.data);const bool resumed=a.mode=="resume";
    const auto input=resumed?q::f::read_text(a.input):std::string{},expected_bytes=resumed?q::f::read_text(a.expected):std::string{};
    auto w=resumed?restore_selected(c,q::save::read_save(a.input),a.rule):a.rule==18?
        sim::World::create_original_deadline_food_dispatch(c.policy,c.model,q::save::prepare_original_terrain_facts(c.data,c.map_relative)):a.rule==17?
        sim::World::create_original_deadline_service_dispatch(c.policy,c.model,q::save::prepare_original_terrain_facts(c.data,c.map_relative)):a.rule==16?
        sim::World::create_original_deadline_fire_dispatch(c.policy,c.model,q::save::prepare_original_terrain_facts(c.data,c.map_relative)):a.rule==15?
        sim::World::create_original_inspector_coordination(c.policy,c.model,q::save::prepare_original_terrain_facts(c.data,c.map_relative)):
        sim::World::create_original_cardinal_water(c.policy,c.model,q::save::prepare_original_terrain_facts(c.data,c.map_relative));
    check(w.rule_version()==a.rule&&w.original_cardinal_water_enabled()&&
        (a.rule!=15||w.original_inspector_coordination_enabled())&&
        (a.rule!=16||w.original_deadline_fire_dispatch_enabled())&&
        (a.rule!=18||w.original_deadline_food_dispatch_enabled())&&
        (a.rule!=17||w.original_deadline_service_dispatch_enabled())&&w.original_worker_upkeep_interval()==816&&
        w.original_rules_model()->provenance().sha256==strategy::model_sha,"Explicit source-bound selected Rule14/15 with unchanged816 upkeep required");
    fs::create_directories(a.output);Json commands=Json::array(),blocked=nullptr,manifest=Json::array(),comparisons=Json::array();
    const auto independent=orig::load_model_data(c.data);check(bool(independent),"Independent original model preparation failed");
    const orig::HouseEvolutionRules rules(*independent.model,orig::Difficulty::Normal);
    std::string stop_reason;
    const auto apply_recipe=[&]{return a.paid_strategy.empty()?strategy::apply(w,a.variant,commands,blocked):
        paid_strategy::apply(w,a.paid_strategy,commands,blocked);};
    if(!resumed&&!apply_recipe())stop_reason="Actual initial paid command refused";
    const auto begin=w.ticks();checkpoint14(c,a.output,"seed",w,manifest);
    if(resumed)check(q::f::read_text(a.output/"seed.save.json")==input,"Separate process changed complete input SaveDocument bytes");
    Json expected_manifest=Json::array();std::set<std::string> compared;
    std::uint64_t requested_end=resumed?q::save::read_save(a.expected).world.ticks:a.search;
    if(resumed){check(requested_end>begin&&requested_end-begin<=80400,"Expected creator endpoint is outside bounded replay");
        const auto path=a.expected.parent_path()/"checkpoints.json";safe(path);check(fs::is_regular_file(path)&&fs::file_size(path)<=65536,"Creator checkpoint manifest required");
        expected_manifest=Json::parse(q::f::read_text(path));check(expected_manifest.is_array()&&expected_manifest.size()<=128,"Invalid bounded checkpoint manifest");
        std::set<std::string> names;for(const auto& entry:expected_manifest){
            const auto name=entry.at("file").get<std::string>();const auto label=entry.at("label").get<std::string>();
            check(!name.empty()&&name.size()<=128&&fs::path(name).filename()==fs::path(name)&&name==label+".save.json"&&
                !label.empty()&&label.size()<=70&&std::all_of(label.begin(),label.end(),[](unsigned char v){return (v>='a'&&v<='z')||(v>='0'&&v<='9')||v=='-';})&&
                names.insert(name).second,"Invalid or duplicate creator checkpoint label");
            check(entry.at("tick").is_number_unsigned(),"Invalid creator checkpoint tick");}
        const auto creator_report=a.expected.parent_path()/"report.json";safe(creator_report);
        check(fs::is_regular_file(creator_report)&&fs::file_size(creator_report)<=1024U*1024U,"Bounded creator recipe report required");
        const auto creator=Json::parse(q::f::read_text(creator_report));check(creator.at("variant")==a.variant&&creator.at("rule")==a.rule&&creator.at("schema")==schema_for_rule(a.rule),"Replay variant or generation differs from creator");
        if(a.rule==15)check(creator.at("inspector_coordination_contract")==sim::original_inspector_coordination_contract,"Replay Inspector coordination contract differs");
        if(a.rule==16)check(creator.at("deadline_fire_dispatch_contract")==sim::original_deadline_fire_dispatch_contract,"Replay deadline fire-dispatch contract differs");
        if(a.rule==18)check(creator.at("deadline_food_dispatch_contract")==sim::original_deadline_food_dispatch_contract,"Replay deadline food contract differs");
        if(a.rule==17||a.rule==18)check(creator.at("deadline_service_dispatch_contract")==sim::original_deadline_service_dispatch_contract&&
            creator.at("deadline_fire_dispatch_contract")==sim::original_deadline_fire_dispatch_contract,"Replay deadline service/fire contract differs");
        check(creator.value("paid_strategy",std::string{})==a.paid_strategy,"Replay paid strategy differs from the independently supplied creator");}
    const auto compare_expected=[&]{if(!resumed)return;for(const auto& entry:expected_manifest)if(entry.at("tick").get<std::uint64_t>()==w.ticks()){
        const auto name=entry.at("file").get<std::string>();if(compared.contains(name))continue;
        const auto path=a.expected.parent_path()/name;safe(path);check(fs::is_regular_file(path)&&fs::file_size(path)<=8U*1024U*1024U,"Bounded creator checkpoint required");
        const auto original=q::f::read_text(path);const auto d=q::save::read_save(path);
        check(d.world==w.snapshot(),"Separate-process full checkpoint snapshot differs");Json ignored=Json::array();
        const auto label="replay-"+entry.at("label").get<std::string>();checkpoint14(c,a.output,label,w,ignored);
        check(q::f::read_text(a.output/(label+".save.json"))==original,"Separate-process full checkpoint SaveDocument bytes differ");
        compared.insert(name);comparisons.push_back({{"file",name},{"tick",w.ticks()},{"full_snapshot_equal",true},{"full_document_bytes_equal",true}});}};
    compare_expected();metrics::Observation city(w);std::unique_ptr<high::Observation> observed,retained;
    std::unique_ptr<oe_elite_stability_diagnosis::Observation> diagnosis;
    if(!a.diagnosis.empty())diagnosis=std::make_unique<oe_elite_stability_diagnosis::Observation>(w,a.output);
    std::unique_ptr<oe_elite_fire_coverage_metrics::Observation> fire_observed;
    if(fire_diagnosis(a.diagnosis))fire_observed=std::make_unique<oe_elite_fire_coverage_metrics::Observation>(w,a.output);
    std::unique_ptr<oe_elite_fire_replay_digest::Observation> tick_digests;
    if(a.tick_digests)tick_digests=std::make_unique<oe_elite_fire_replay_digest::Observation>(w,a.output);
    std::unique_ptr<trace::Observation> transported;std::optional<sim::BuildingId> target=strategy::elite(w);
    const auto attach=[&]{if(target&&!observed){observed=std::make_unique<high::Observation>(w,*target,rules);transported=std::make_unique<trace::Observation>(w,*target,rules);}};attach();
    std::optional<std::uint64_t> first;std::uint64_t lost=0,unmet=0,next_unmet=0,evaluations=0;std::int64_t retain_min=w.treasury();Json first_entry=nullptr,evaluation_provenance=Json::array();
    if(target&&entry17(w,*target)){const auto& proof=*entry17(w,*target);first=proof.evaluation.tick;
        first_entry={{"tick",*first},{"input",high::evaluation(proof.evaluation,rules)},{"entry_observed_in_this_process",false}};
        retained=std::make_unique<high::Observation>(w,*target,rules);
        if(fire_observed)fire_observed->begin_retention(w);citywide_routes(a,w,"retention-begin");}
    std::map<std::string,bool> failure_saved{{"first-current-failure",false},{"first-fire",false},{"first-devolution",false}};
    std::map<std::string,std::uint64_t> creator_failure_ticks;
    if(resumed&&!a.paid_strategy.empty())for(const auto& entry:expected_manifest) {
        const auto label=entry.at("label").get<std::string>();
        if(failure_saved.contains(label)) {
            const auto tick=entry.at("tick").get<std::uint64_t>();creator_failure_ticks.emplace(label,tick);
            if(tick<=begin)failure_saved.at(label)=true;
        }
    }
    const auto failure_checkpoint=[&](const char* label,bool actual_event) {
        if(a.paid_strategy.empty()||failure_saved.at(label))return;
        if(resumed) {
            // An old creator manifest may predate these observations. Never
            // relabel a later replay event as its unknown historical first.
            const auto expected=creator_failure_ticks.find(label);
            if(expected==creator_failure_ticks.end()||expected->second!=w.ticks())return;
            check(actual_event,"Creator first-failure checkpoint disagrees with the actual replay event");
        }else if(!actual_event)return;
        check(manifest.size()<128,"First-failure observation exceeds unchanged checkpoint manifest bound");
        checkpoint14(c,a.output,label,w,manifest);failure_saved.at(label)=true;
    };
    const auto tick_once=[&]{city.before(w);if(observed)observed->before(w);if(transported)transported->before(w);if(retained)retained->before(w);
        if(diagnosis)diagnosis->before(w);
        if(fire_observed)fire_observed->before(w);
        const auto tick=w.ticks();w.tick();check(w.ticks()==tick+1,"World did not advance one actual tick");city.after(w);
        if(observed)observed->after(w);if(transported)transported->after(w);if(retained)retained->after(w);
        if(diagnosis)diagnosis->after(w);
        if(fire_observed)fire_observed->after(w);
        if(tick_digests)tick_digests->after(w);};
    try{while(stop_reason.empty()&&w.ticks()<requested_end){
        const bool continued_timing=!a.paid_strategy.empty()&&paid_strategy::timing_recipe(a.paid_strategy);
        if(!first||continued_timing){if(!apply_recipe()){stop_reason="Actual paid command refused";break;}target=strategy::elite(w);attach();}
        if(!a.paid_strategy.empty()&&!paid_strategy::maintenance(w,a.paid_strategy,commands,blocked)) {
            stop_reason="Actual paid maintenance command refused";break;
        }
        if(continued_timing&&retained)retain_min=std::min(retain_min,w.treasury());
        const bool was_retaining=retained!=nullptr;
        const auto previous_target_type=target?std::optional{q::house(w,*target).type}:std::nullopt;
        tick_once();
        if(target){const auto& e=high::history(w,*target).last_evaluation;if(e&&e->tick==w.ticks())evaluation_provenance.push_back({{"tick",w.ticks()},{"provenance",provenance(w,*target)}});}
        if(target&&!first&&entered17(w,*target)){
            const auto& proof=*high::history(w,*target).first_transitions[14];check(proof.evaluation.tick==w.ticks(),"First success was not captured at exact actual16to17 tick");
            first=w.ticks();checkpoint14(c,a.output,"first-success",w,manifest);
            citywide_routes(a,w,"retention-begin");
            first_entry={{"tick",w.ticks()},{"input",high::evaluation(proof.evaluation,rules)},{"provenance",provenance(w,*target)},{"entry_observed_in_this_process",true}};
            retained=std::make_unique<high::Observation>(w,*target,rules);retain_min=w.treasury();
            if(fire_observed)fire_observed->begin_retention(w);
            if(!resumed)requested_end=w.ticks()+20400;
        }
        if(was_retaining&&target){retain_min=std::min(retain_min,w.treasury());if(!entered17(w,*target))++lost;
            failure_checkpoint("first-fire",w.building_on_fire(*target));
            failure_checkpoint("first-devolution",previous_target_type&&q::house(w,*target).type<*previous_target_type);
            bool current_failed=false;
            const auto& e=high::history(w,*target).last_evaluation;if(e&&e->tick==w.ticks()){++evaluations;current_failed=!orig::needs_met(rules,orig::house_row(e->input.type),e->input);if(current_failed)++unmet;
                if(!orig::needs_met(rules,orig::next_row(e->input.type),e->input))++next_unmet;
            }
            failure_checkpoint("first-current-failure",current_failed);
        }
        if(first&&w.ticks()==*first+10200)checkpoint14(c,a.output,"midpoint",w,manifest);
        else if(w.ticks()%8160==0)checkpoint14(c,a.output,"checkpoint-"+std::to_string(w.ticks()),w,manifest);
        // Explicit external OE48 observation of the reproduced House19 chain.
        // These complete documents read the real post-tick World; they neither
        // change its commands nor introduce a production tick/frame hook.
        if(a.diagnosis=="oe48"&&a.paid_strategy=="oe47-south-door-market-south") {
            constexpr std::array<std::uint64_t,13> observations{5920,6060,6371,6460,6540,6559,6659,
                6925,6976,7140,7184,7282,7284};
            if(std::find(observations.begin(),observations.end(),w.ticks())!=observations.end())
                checkpoint14(c,a.output,"oe48-house19-"+std::to_string(w.ticks()),w,manifest);
        }
        compare_expected();
    }}catch(const std::exception& error){stop_reason=error.what();}
    if(stop_reason.empty()&&!a.paid_strategy.empty()&&!paid_strategy::timing_purchases_complete(w,a.paid_strategy))
        stop_reason="Bounded player strategy ended with unresolved actual-arrival purchase gates; no fallback purchase performed";
    try{q::authority(w);q::worker_oracle(w);checkpoint14(c,a.output,"result",w,manifest);compare_expected();}
    catch(const std::exception& error){write(a.output/"failed-run.json",{{"technical_status","FAIL"},{"tick",w.ticks()},
        {"tick_or_observer_error",stop_reason.empty()?Json(nullptr):Json(stop_reason)},{"authority_save_or_comparison_error",error.what()},
        {"previous_checked_checkpoints",manifest},{"commands",commands},{"last_refused_command",blocked},
        {"paid_elite17_certified",false},{"retention_certified",false},{"native_human","NOT_RUN"}});throw;}
    if(resumed){check(w.ticks()==requested_end,"Replay stopped before creator endpoint");check(q::f::read_text(a.output/"result.save.json")==expected_bytes,"Separate-process final complete SaveDocument bytes differ");
        for(const auto& entry:expected_manifest){const auto tick=entry.at("tick").get<std::uint64_t>();if(tick>=begin&&tick<=requested_end)check(compared.contains(entry.at("file").get<std::string>()),"Replay omitted an in-range complete checkpoint comparison");}}
    q::save::RecoveryStore recovery(a.output/"recovery-app",c.data,q::save::recovery_storage_budget_bytes,q::save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(c.document(w),c.mask);const auto entry=recovery.write_checkpoint(history,c.document(w),c.mask);
    check(entry.loadable()&&entry.schema==schema_for_rule(a.rule)&&entry.rule_version==a.rule,"Recovery lost exact selected rule/schema");auto recovered=restore_selected(c,q::save::read_save(entry.path),a.rule);
    check(recovered.snapshot()==w.snapshot()&&q::f::read_text(entry.path)==q::f::read_text(a.output/"result.save.json"),"Complete Recovery bytes or snapshot differ");
    const bool retained_pass=first&&w.ticks()==*first+20400&&lost==0&&unmet==0&&evaluations==400;
    Json retention=retained?retained->report(w):Json(nullptr);std::int64_t cashflow=0;if(retained)cashflow=retention.at("city").at("summary").at("operating_cashflow");
    const auto financial_ticks=retained?w.ticks()-std::max(begin,*first):0;
    write(a.output/"city-metrics.json",city.report(w));if(observed)write(a.output/"elite-metrics.json",observed->report(w));
    if(transported)write(a.output/"transport-metrics.json",transported->report(w));if(retained)write(a.output/"retention-metrics.json",retention);
    write(a.output/"commands.json",commands);write(a.output/"checkpoints.json",manifest);write(a.output/"evaluation-provenance.json",evaluation_provenance);
    Json diagnostic_result=diagnosis?diagnosis->finish(w):Json(nullptr);
    if(diagnosis)write(a.output/"oe44-diagnosis.json",diagnostic_result);
    Json fire_result=fire_observed?fire_observed->finish(w):Json(nullptr);
    citywide_routes(a,w,"result");
    if(fire_observed)write(a.output/"oe45-fire-coverage.json",fire_result);
    Json digest_result=tick_digests?tick_digests->finish():Json(nullptr);
    if(tick_digests) {
        check(!digest_result.at("overflow").get<bool>(),"Per-tick state-digest budget exhausted");
        digest_result["separate_process_comparison"]=resumed?compare_tick_digests(a,begin,w.ticks()):Json(nullptr);
        write(a.output/"oe45-state-digests.json",digest_result);
    }
    check(source_pins(a.data)==pins,"Original input bytes/modes/mtimes changed");if(resumed)check(q::f::read_text(a.input)==input&&q::f::read_text(a.expected)==expected_bytes,"Replay changed protected creator files");
    const Json outcome= !stop_reason.empty()?Json(stop_reason):!first?Json("Bounded search ended without actual Elite16-to-17; final paid population, phase gate, pending purchases and unmet needs are retained"):
        !retained_pass?Json("Actual Elite17 entry retained, but this observed interval does not certify 20400 uninterrupted ticks with 400 needs-met evaluations"):Json(nullptr);
    Json report{{"format","openemperor-paid-elite-desirability-v1"},{"technical_status",stop_reason.empty()?"PASS":"PARTIAL"},
        {"mode",a.mode},{"variant",a.variant},{"rule",w.rule_version()},{"schema",schema_for_rule(w.rule_version())},{"policy",3},{"revision",27},{"snapshot_injection",false},
        {"begin_tick",begin},{"end_tick",w.ticks()},{"paid_elite17",first?"PASS":"PARTIAL"},{"retention",first?(retained_pass?"PASS":"PARTIAL"):"NOT_RUN"},
        {"first_entry",first_entry},{"retention_observation_begin",retained?Json(std::max(begin,*first)):Json(nullptr)},
        {"retention_lost_ticks",lost},{"retention_current_unmet",unmet},{"retention_next_unmet",next_unmet},{"retention_evaluations",evaluations},
        {"operating_cashflow",cashflow},{"minimum_retention_funds",retain_min},{"financial_observed_ticks",financial_ticks},
        {"financial_stability",retained?(financial_ticks<20400?"PARTIAL":cashflow>=0&&retain_min>=0?"PASS":"FAIL"):"NOT_RUN"},
        {"actual_blocker",outcome},{"last_refused_command",blocked},{"final_plan_and_actual_needs",a.paid_strategy.empty()?
            strategy::status(w,a.variant):paid_strategy::status(w,a.paid_strategy)},
        {"final_provenance",target?provenance(w,*target):Json(nullptr)},{"complete_recovery_equality",true},
        {"separate_process_comparison",resumed?"PASS":"NOT_RUN"},{"checkpoint_comparisons",comparisons},{"source_pins",pins},
        {"input_bytes_modes_mtimes_unchanged",true},{"original_executable_ran",false},{"native_human","NOT_RUN"}};
    if(!a.paid_strategy.empty())report["paid_strategy"]=a.paid_strategy;
    if(a.rule==15)report["inspector_coordination_contract"]=sim::original_inspector_coordination_contract;
    if(a.rule==16)report["deadline_fire_dispatch_contract"]=sim::original_deadline_fire_dispatch_contract;
    if(a.rule==18)report["deadline_food_dispatch_contract"]=sim::original_deadline_food_dispatch_contract;
    if(a.rule==17||a.rule==18){report["deadline_service_dispatch_contract"]=sim::original_deadline_service_dispatch_contract;
        report["deadline_fire_dispatch_contract"]=sim::original_deadline_fire_dispatch_contract;}
    write(a.output/"report.json",report);
    if(fire_diagnosis(a.diagnosis)) {
        const bool complete=first&&financial_ticks==20400&&evaluations==400;
        const bool fire_ok=retained&&retention.at("summary").at("burning_ticks")==0;
        const bool occupied=retained&&retention.at("summary").at("minimum_target_occupants").get<int>()>0;
        const bool no_devolution=retained&&retention.at("summary").at("devolutions")==0;
        const bool finance_ok=complete&&cashflow>=0&&retain_min>=0;
        const bool technical=stop_reason.empty()&&!diagnostic_result.at("overflow").get<bool>()&&
            !fire_result.at("trace_overflow").get<bool>()&&
            (!tick_digests||!digest_result.at("overflow").get<bool>());
        write(a.output/"oe45-outcome.json",{{"TECHNICAL",technical?"PASS":"FAIL"},
            {"ELITE17_ENTRY",first?"PASS":"FAIL"},{"FIRE_COVERAGE",complete&&fire_ok?"PASS":"PARTIAL"},
            {"SUPPLY",complete&&unmet==0?"PASS":"PARTIAL"},{"FINANCE",finance_ok?"PASS":"PARTIAL"},
            {"ELITE17_RETENTION",technical&&complete&&retained_pass&&fire_ok&&occupied&&no_devolution&&finance_ok?"PASS":"PARTIAL"},
            {"current_generation_checkpoint_bytes_validated",true},{"historical_compatibility_requires_separate_gate",true},
            {"whole_city_assessment_requires_comparison",true},{"ORIGINAL_PARITY","UNKNOWN"},{"NATIVE_HUMAN","NOT_RUN"}});
    }
    std::cout<<"OE40 actual Elite17="<<(first?"reached":"not reached")<<" retention="<<retained_pass<<" tick="<<w.ticks()<<'\n';
}
void render_inspector_matrix(const Options& a,oe::SandboxView& view,Sdl& sdl,
        const std::string& input,const q::save::SaveDocument& saved,const fs::path& copy,
        sim::BuildingId target,const Json& pins) {
    check((a.rule==15||a.rule==16||a.rule==17||a.rule==18)&&!a.diagnosis.empty()&&saved.source_schema_version==schema_for_rule(a.rule)&&
        (view.world().original_inspector_coordination_enabled()||view.world().original_deadline_fire_dispatch_enabled()||
         view.world().original_deadline_service_dispatch_enabled()||view.world().original_deadline_food_dispatch_enabled()),
        "Exact explicit Inspector generation render required");
    using Point=oe::scene::Point;
    using Rect=oe::sandbox_ui::Rect;
    Json captures=Json::array(),matrix=Json::array(),watch_captures=Json::array();std::uint64_t actions=0;
    const auto pure=[&]{check(view.paused()&&view.world().snapshot()==saved.world,
        "Inspector action changed complete World or paused state");};
    const auto send=[&](SDL_Event e) {
        check(SDL_PushEvent(&e),SDL_GetError());bool running=true;
        while(SDL_PollEvent(&e)){view.handle_event(e,running);pure();}
        check(running,"Inspector action exited actual View");view.update(0);pure();++actions;
    };
    const auto keyboard=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;send(e);
        e={};e.type=SDL_EVENT_KEY_UP;e.key.key=code;send(e);};
    const auto raw=[&](Point p){float x=0,y=0;check(SDL_RenderCoordinatesToWindow(sdl.renderer,float(p.x),float(p.y),&x,&y),
        "Actual render-to-window transform failed");return Point{x,y};};
    const auto middle=[](Rect r){return Point{r.x+r.w*.5,r.y+r.h*.5};};
    const auto click=[&](Point p){const auto at=raw(p);const auto diagnostic=view.input_diagnostic_state(at);
        check(diagnostic.ui&&diagnostic.render_position.has_value(),"Production UI diagnostic rejected its rendered hitbox");
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;
        e.motion.x=float(at.x);e.motion.y=float(at.y);send(e);
        e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        e.button.x=float(at.x);e.button.y=float(at.y);send(e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;send(e);};
    const auto wheel=[&](Point p,float amount){const auto at=raw(p);SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;
        e.wheel.mouse_x=float(at.x);e.wheel.mouse_y=float(at.y);e.wheel.y=amount;send(e);};
    const auto render=[&]{check(view.render(),"Actual OE44 inspector render failed");pure();};
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;send(focus);
    if(!view.layout().panel_open)keyboard(SDLK_TAB);
    check(!view.selected_building()&&!view.original_house_inspector_active(),"Initial building list is not available");
    check(SDL_SetWindowSize(sdl.window,1280,800),SDL_GetError());
    SDL_Event resized{};resized.type=SDL_EVENT_WINDOW_RESIZED;resized.window.data1=1280;resized.window.data2=800;send(resized);render();
    std::vector<sim::BuildingId> entries;for(const auto& b:view.world().buildings())if(b.placed)entries.push_back(b.id);
    const auto found=std::find(entries.begin(),entries.end(),target);check(found!=entries.end(),"Actual Elite missing from building list");
    const auto target_row=static_cast<std::size_t>(found-entries.begin());
    auto layout=view.inspector_layout();check(layout.list_rows==entries.size(),"Production list identity/order differs");
    wheel(middle(layout.content),1000000);render();std::vector<bool> list_seen(entries.size());
    for(unsigned n=0;;++n){check(n<1024,"Building list traversal exceeded its bound");layout=view.inspector_layout();
        for(std::size_t i=0;i<entries.size();++i)if(layout.list_row_rect(i).h>0)list_seen[i]=true;
        if(layout.scroll==layout.scroll_limit)break;
        const auto before=layout.scroll;wheel(middle(layout.content),-1);render();
        check(view.inspector_scroll()>before,"Production building list scroll stalled");
    }
    check(std::all_of(list_seen.begin(),list_seen.end(),[](bool v){return v;}),"A production building list row is unreachable");
    // Existing OE42 contract: fragments and fixed header/footer cannot select.
    wheel(middle(layout.content),1000000);render();
    wheel(middle(view.inspector_layout().content),-1);render();
    layout=view.inspector_layout();const auto panel=view.layout().panel;std::size_t fragment_probes=0;
    for(const Point p:{Point{panel.x+panel.w*.5,double(panel.y+2)},Point{panel.x+panel.w*.5,double(panel.y+panel.h-2)}}){
        click(p);check(!view.selected_building(),"Header/footer selected an invisible list entry");}
    for(std::size_t i=0;i<entries.size();++i)if(layout.list_row_rect(i).h==0){
        const int y=layout.list_y+int(i)*18*layout.scale-layout.scroll;
        const int top=std::max(y-2*layout.scale,layout.content.y),bottom=std::min(y+16*layout.scale,layout.content.y+layout.content.h);
        if(top>=bottom)continue;
        const Point p{layout.content.x+layout.content.w*.5,(top+bottom)*.5};
        check(!layout.list_row_at(p.x,p.y),"Partial list row retained a layout hitbox");click(p);
        check(!view.selected_building(),"Clipped invisible list row remained clickable");++fragment_probes;
    }
    check(fragment_probes>0,"No actually clipped list fragment was probed");
    if(fire_diagnosis(a.diagnosis)) {
        std::vector<sim::CourierId> inspectors;
        for(const auto& actor:view.world().couriers())if(actor.role==sim::CourierRole::FireInspector)inspectors.push_back(actor.id);
        for(const auto id:inspectors) {
            // F9 replaces World; retain only this bounded Inspector record.
            const auto actor=view.world().courier(id);
            const auto owner=std::find(entries.begin(),entries.end(),actor.owner);
            check(owner!=entries.end(),"Actual Inspector lost its paid Watch row");
            const auto index=static_cast<std::size_t>(owner-entries.begin());
            wheel(middle(view.inspector_layout().content),1000000);render();
            for(unsigned n=0;;++n) {
                check(n<1024,"Watch list selection exceeds its bound");layout=view.inspector_layout();
                const auto row=layout.list_row_rect(index);
                if(row.h>0){click(middle(row));break;}
                check(layout.scroll<layout.scroll_limit,"Paid Watch row is unreachable");
                wheel(middle(layout.content),-1);render();
            }
            check(view.selected_building()==actor.owner,"Actual list did not select the paid Watch");
            wheel(middle(view.inspector_layout().content),1000000);render();
            for(const unsigned z:{1U,2U,4U}) {
                const auto anchor=middle(view.layout().map);
                wheel(anchor,float(std::log(double(z)/view.camera().zoom)/std::log(1.15)));render();
                const auto diagnostic=view.walker_diagnostics();
                const auto found_actor=std::find_if(diagnostic.begin(),diagnostic.end(),[&](const auto& d){return d.id==actor.id;});
                check(found_actor!=diagnostic.end(),"Actual Inspector render diagnostic is missing");
                if(found_actor->live_visible) {
                    const auto camera=view.camera();
                    const Point ground{(found_actor->screen_ground.x-camera.offset.x)/camera.zoom,
                        (found_actor->screen_ground.y-camera.offset.y)/camera.zoom};
                    center(view,ground);render();
                    const auto centered=view.walker_diagnostics();
                    const auto visible=std::find_if(centered.begin(),centered.end(),[&](const auto& d){return d.id==actor.id;});
                    check(visible!=centered.end()&&visible->submitted,"Centered live Inspector was not submitted by production rendering");
                }
                std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(sdl.renderer,nullptr),SDL_DestroySurface);
                const auto name="inspector-"+std::to_string(static_cast<unsigned>(actor.id))+"-"+std::to_string(z)+"x.png";
                check(frame&&!fs::exists(a.output/name)&&SDL_SavePNG(frame.get(),(a.output/name).string().c_str()),"Cannot capture actual Watch inspection");
                const auto before_save=view.save_generation(),before_load=view.load_generation();
                keyboard(SDLK_F5);keyboard(SDLK_F9);
                check(view.save_generation()==before_save+1&&view.load_generation()==before_load+1&&
                    q::f::read_text(copy)==input,"Actual Watch F5/F9 changed complete canonical bytes");pure();
                watch_captures.push_back({{"file",name},{"zoom",z},{"courier",static_cast<unsigned>(actor.id)},
                    {"owner",static_cast<unsigned>(actor.owner)},{"target",static_cast<unsigned>(actor.target)},
                    {"phase",sim::courier_phase_name(actor.phase)},{"route_pending",actor.route_pending},
                    {"actual_path",trace::cells(actor.path)},{"path_vertex",actor.path_vertex},{"edge_progress",actor.edge_progress},
                    {"live_walker_visible",found_actor->live_visible},{"inspection",view.inspection_lines()},
                    {"complete_f5_f9_equality",true},{"visually_inspected",false}});
            }
        }
        check(watch_captures.size()==6,"Exact two-Watch three-zoom capture scope required");
    }
    wheel(middle(view.inspector_layout().content),1000000);render();
    for(unsigned n=0;;++n){check(n<1024,"Elite list selection exceeded its bound");layout=view.inspector_layout();
        const auto row=layout.list_row_rect(target_row);
        if(row.h>0){check(layout.list_row_at(middle(row).x,middle(row).y)==target_row,"Elite list hitbox differs");click(middle(row));break;}
        check(layout.scroll<layout.scroll_limit,"Actual Elite list row is unreachable");wheel(middle(layout.content),-1);render();
    }
    check(view.selected_building()==target&&view.original_house_inspector_active(),"Actual list click did not select the paid Elite");
    const auto visual=view.original_house_visual_diagnostic(target);
    check(visual.applicable&&visual.active&&visual.image_width>0&&visual.image_height>0,
        "Paid Elite lacks active original sprite bounds; do not claim original display");
    const Point art_center{visual.image_origin.x+visual.image_width*.5,visual.image_origin.y+visual.image_height*.5};
    const auto zoom=[&](double wanted,Point anchor){wheel(anchor,float(std::log(wanted/view.camera().zoom)/std::log(1.15)));
        check(std::abs(view.camera().zoom-wanted)<1e-5,"Normal wheel zoom differs");};
    const auto center_art=[&](double wanted){const auto map=view.layout().map;const auto m=middle(map);zoom(1,m);
        bool centered=false;for(unsigned n=0;n<48;++n){const auto p=view.camera().world_to_screen(art_center);
            const double dx=m.x-p.x,dy=m.y-p.y;if(std::abs(dx)<.01&&std::abs(dy)<.01){centered=true;break;}
            const double x=std::clamp(dx,-map.w*.3,map.w*.3),y=std::clamp(dy,-map.h*.3,map.h*.3);
            zoom(2,{m.x-x,m.y-y});zoom(1,{m.x+x,m.y+y});}
        check(centered,"Bounded normal-wheel art centering did not converge");zoom(wanted,m);render();};
    const auto capture=[&](const std::string& name,const std::string& position){render();
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(sdl.renderer,nullptr),SDL_DestroySurface);
        const auto file=a.output/(name+".png");check(frame&&!fs::exists(file)&&SDL_SavePNG(frame.get(),file.string().c_str()),"Cannot capture actual inspector frame");
        const auto origin=view.camera().world_to_screen(visual.image_origin);const auto map=view.layout().map;
        const auto displayed=view.original_house_display_stats();const auto slot=static_cast<std::size_t>(visual.house_type)-2;
        check(slot<displayed.drawn_instances.size()&&displayed.drawn_instances[slot]>0&&displayed.authored_fallbacks[slot]==0,
            "Centered paid Elite type was not drawn with original art");
        const double width=visual.image_width*view.camera().zoom,height=visual.image_height*view.camera().zoom;
        const double visible_w=std::max(0.0,std::min(origin.x+width,double(map.x+map.w))-std::max(origin.x,double(map.x)));
        const double visible_h=std::max(0.0,std::min(origin.y+height,double(map.y+map.h))-std::max(origin.y,double(map.y)));
        captures.push_back({{"file",file.filename().string()},{"width",frame->w},{"height",frame->h},
            {"zoom",view.camera().zoom},{"section",static_cast<unsigned>(view.original_house_section())},{"scroll",view.original_house_scroll()},
            {"position",position},{"selected_owner",static_cast<unsigned>(target)},{"visually_inspected",false},
            {"sprite_screen_bounds",{origin.x,origin.y,width,height}},{"visible_sprite_area_fraction",visible_w*visible_h/(width*height)},
            {"sprite_fits_viewport",width<=map.w&&height<=map.h},{"target_type_original_draws",displayed.drawn_instances[slot]},
            {"target_type_authored_fallbacks",displayed.authored_fallbacks[slot]},{"complete_world_equal",true},{"paused",true}});
        pure();};
    for(const auto size:std::array<std::array<int,2>,3>{{{1280,800},{1600,900},{1920,1080}}}){
        check(SDL_SetWindowSize(sdl.window,size[0],size[1]),SDL_GetError());resized={};resized.type=SDL_EVENT_WINDOW_RESIZED;
        resized.window.data1=size[0];resized.window.data2=size[1];send(resized);send(focus);render();
        int output_w=0,output_h=0;check(SDL_GetRenderOutputSize(sdl.renderer,&output_w,&output_h)&&output_w==size[0]&&output_h==size[1],
            "Actual renderer dimensions differ from requested matrix");
        for(const unsigned z:{1U,2U,4U}){center_art(z);
            for(unsigned index=0;index<4;++index){const auto section=static_cast<oe::SandboxView::OriginalHouseSection>(index);
                const auto tab=view.original_house_section_rect(section);check(tab.h>0&&tab.w>0,"Production tab is not visible");click(middle(tab));render();
                check(view.selected_building()==target&&view.original_house_section()==section,"Actual House tab hitbox selected another state");
                const auto content=view.original_house_content_rect();const auto lines=view.original_house_section_lines();
                check(content.w>0&&content.h>0&&!lines.empty(),"Production section has no readable content");
                wheel(middle(content),1000000);render();check(view.original_house_scroll()==0,"House section cannot return to top");
                const auto name=std::to_string(output_w)+"x"+std::to_string(output_h)+"-"+std::to_string(z)+"x-section"+std::to_string(index);
                capture(name+"-top","top");std::vector<bool> seen(lines.size());
                for(unsigned n=0;;++n){check(n<512,"House section traversal exceeded bound");const auto scroll=view.original_house_scroll();
                    const int scale=view.layout().scale;for(std::size_t row=0;row<lines.size();++row)
                        if(oe::sandbox_ui::visible_text_row(content.y+int(row)*17*scale-scroll,12*scale,content).h>0)seen[row]=true;
                    wheel(middle(content),-1);render();if(view.original_house_scroll()==scroll)break;
                    check(view.original_house_scroll()>scroll,"House section scrolled backwards");
                }
                check(std::all_of(seen.begin(),seen.end(),[](bool v){return v;}),"At least one actual House section line is unreachable");
                if(view.original_house_scroll()>0)capture(name+"-end","end");
                const auto before_save=view.save_generation();keyboard(SDLK_F5);
                check(view.save_generation()==before_save+1,"Per-case F5 did not complete exactly one actual save");
                check(q::f::read_text(copy)==input,"Per-case F5 canonical schema31 bytes differ");
                const auto before_load=view.load_generation();keyboard(SDLK_F9);
                check(view.load_generation()==before_load+1,"Per-case F9 did not complete exactly one actual load");
                check(q::f::read_text(copy)==input&&view.world().snapshot()==saved.world&&view.paused(),"Per-case F9 changed complete saved authority");
                matrix.push_back({{"width",output_w},{"height",output_h},{"zoom",z},{"section",index},
                    {"rows",lines.size()},{"all_rows_reachable",true},{"complete_f5_f9_equality",true},{"status","PASS"}});
            }
        }
    }
    check(matrix.size()==36&&captures.size()<=72,"Unexpected bounded inspector matrix size");
    check(q::f::read_text(a.input)==input&&source_pins(a.data)==pins,"Inspector render changed protected input or original source pins");
    Json report{{"format",a.diagnosis=="oe48"?"openemperor-oe48-inspector-view-v1":a.rule==16?"openemperor-oe45-inspector-view-v1":"openemperor-oe44-inspector-view-v1"},{"status","PASS"},
        {"rule",a.rule},{"schema",schema_for_rule(a.rule)},
        {"actual_sandbox_renderer",true},{"backend",SDL_GetRendererName(sdl.renderer)},{"scripted_dummy_software",true},
        {"target",static_cast<unsigned>(target)},{"actual_type",visual.house_type},{"actual_side",visual.actual_side},
        {"selected_via_actual_building_list",true},{"list_rows_reached",list_seen.size()},{"invisible_list_hits_refused",true},
        {"actually_clipped_fragment_probes",fragment_probes},{"save_load_generations_checked",true},
        {"sprite_center_authority","production OriginalHouseVisualDiagnostic image_origin/width/height"},
        {"matrix",matrix},{"captures",captures},{"checked_actions",actions},{"world_unchanged",true},{"paused_after_every_action",true},
        {"complete_f5_f9_equality",true},{"source_inputs_unchanged",true},{"captures_visually_inspected",false},{"native_human","NOT_RUN"}};
    if(a.rule>=16)report["deadline_fire_dispatch_contract"]=sim::original_deadline_fire_dispatch_contract;
    else report["inspector_coordination_contract"]=sim::original_inspector_coordination_contract;
    if(a.rule>=17)report["deadline_service_dispatch_contract"]=sim::original_deadline_service_dispatch_contract;
    if(a.rule==18)report["deadline_food_dispatch_contract"]=sim::original_deadline_food_dispatch_contract;
    if(fire_diagnosis(a.diagnosis))report["fire_watch_captures"]=watch_captures;
    write(a.output/"report.json",report);
}
void render_citywide_matrix(const Options& a,oe::SandboxView& view,Sdl& sdl,
        const std::string& input,const q::save::SaveDocument& saved,const fs::path& copy,
        sim::BuildingId target,const Json& pins) {
    check(city_diagnosis(a.diagnosis)&&(a.rule==16||a.rule==17||a.rule==18),"Citywide capture is explicit OE46 Rule16/17 or OE47 Rule16/17/18 QA only");
    auto report=oe_citywide_reliability_view::run(view,sdl.window,sdl.renderer,saved,input,copy,a.output,target,food_diagnosis(a.diagnosis),a.diagnosis=="oe48",
        [](const fs::path& path){return q::f::read_text(path);},
        [&](oe::scene::Point point){center(view,point);});
    check(q::f::read_text(a.input)==input&&source_pins(a.data)==pins,
        "OE46 citywide view changed protected input or original source pins");
    report["source_inputs_unchanged"]=true;
    report["deadline_fire_dispatch_contract"]=sim::original_deadline_fire_dispatch_contract;
    if(a.rule==18)report["deadline_food_dispatch_contract"]=sim::original_deadline_food_dispatch_contract;
    if(a.rule==17||a.rule==18)report["deadline_service_dispatch_contract"]=sim::original_deadline_service_dispatch_contract;
    write(a.output/"report.json",report);
}
void render14(const Options& a){
    const auto pins=source_pins(a.data);const auto c=q::Context::real(a.data);const auto input=q::f::read_text(a.input);const auto d=q::save::read_save(a.input);
    auto restored=restore_selected(c,d,a.rule);q::authority(restored);const auto target=strategy::elite(restored);check(target.has_value(),"Render save has no actual paid Elite family");
    fs::create_directories(a.output);const auto copy=a.output/"view.save.json";q::f::write_text(copy,input);Sdl sdl;
    auto session=oe::maps::load_stored_map_session(c.data,c.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,
        oe::maps::GreatWallPresentationMode::Automatic,{},true);
    auto view=a.rule==18?oe::SandboxView::create_original_deadline_food_dispatch(std::move(session),orig::Difficulty::Normal,false):
        a.rule==17?oe::SandboxView::create_original_deadline_service_dispatch(std::move(session),orig::Difficulty::Normal,false):
        a.rule==16?oe::SandboxView::create_original_deadline_fire_dispatch(std::move(session),orig::Difficulty::Normal,false):
        a.rule==15?oe::SandboxView::create_original_inspector_coordination(std::move(session),orig::Difficulty::Normal,false):
        oe::SandboxView::create_original_cardinal_water(std::move(session),orig::Difficulty::Normal,false);
    view->configure_save(c.data,c.map_relative,copy);
    const auto art=oe::detect_and_select_visual_profiles(c.data,a.resources);
    if(!art.walker.empty())view->set_walker_visuals(art.walker,art.walker_source);if(!art.building.empty())view->set_building_visuals(art.building,art.building_source);
    if(!art.road.empty())view->set_road_visuals(art.road,art.road_source);view->set_fire_visuals(art.fire,art.fire_source,art.fire_fallback_reason);
    view->set_fire_inspector_visuals(art.fire_inspector,art.fire_inspector_source,art.fire_inspector_fallback_reason);
    view->set_market_walker_visuals(art.market_walker,art.market_walker_source,art.market_walker_fallback_reason);
    view->set_service_walker_visuals(art.service_walker,art.service_walker_source,art.service_walker_fallback_reason);
    view->set_health_walker_visuals(art.health_walker,art.health_walker_source,art.health_walker_fallback_reason);
    view->set_original_house_visuals(art.original_house,art.original_house_source,art.original_house_fallback_reason);
    view->initialize(sdl.window,sdl.renderer);const auto before_initial_load=view->load_generation();
    key(*view,SDLK_F9);check(view->paused()&&view->world().snapshot()==d.world,"Actual Rule14 View F9 differs");
    if((a.rule==15||a.rule==16||a.rule==17||a.rule==18)&&!a.diagnosis.empty())check(view->load_generation()==before_initial_load+1,
        "Initial OE44 F9 did not complete exactly one actual load");
    const auto before_initial_save=view->save_generation();key(*view,SDLK_F5);
    check(q::f::read_text(copy)==input,"Actual Rule14 View F5 canonical bytes differ");
    if((a.rule==15||a.rule==16||a.rule==17||a.rule==18)&&!a.diagnosis.empty())check(view->save_generation()==before_initial_save+1,
        "Initial OE44 F5 did not complete exactly one actual save");
    if((a.rule==15||a.rule==16||a.rule==17||a.rule==18)&&!a.diagnosis.empty()) {
        if(city_diagnosis(a.diagnosis)) {
            if(a.diagnosis=="oe48") {
                auto watches=a;watches.output=a.output/"fire-inspection";
                check(!fs::exists(watches.output),"Preserve existing OE48 Watch inspection");
                fs::create_directories(watches.output);
                render_inspector_matrix(watches,*view,sdl,input,d,copy,*target,pins);
            }
            render_citywide_matrix(a,*view,sdl,input,d,copy,*target,pins);
            view->shutdown();return;
        }
        render_inspector_matrix(a,*view,sdl,input,d,copy,*target,pins);
        view->shutdown();return;
    }
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;bool running=true;view->handle_event(focus,running);
    const auto visual=view->original_house_visual_diagnostic(*target);check(visual.applicable&&std::isfinite(visual.ground.x)&&std::isfinite(visual.ground.y),"Actual Elite renderer lacks a finite production anchor");
    const auto at=visual.ground;Json captures=Json::array();
    for(unsigned zoom:{1U,2U,4U}){for(int n=0;n<3&&view->camera().zoom!=zoom;++n)key(*view,SDLK_Z);check(view->camera().zoom==zoom,"Actual View zoom failed");
        center(*view,at);view->update(0);check(view->render(),"Actual Rule14 SandboxView render failed");
        const auto drawn=view->original_house_display_stats();const auto slot=static_cast<unsigned>(visual.house_type)-2U;
        const auto original_draws=slot<drawn.drawn_instances.size()?drawn.drawn_instances[slot]:0;
        const auto fallback_draws=slot<drawn.authored_fallbacks.size()?drawn.authored_fallbacks[slot]:0;
        const char* draw_status=original_draws>0&&fallback_draws==0?"ORIGINAL_DRAW_OBSERVED":fallback_draws>0?"AUTHORED_FALLBACK_DRAW_OBSERVED":"TARGET_TYPE_DRAW_NOT_OBSERVED";
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> image(SDL_RenderReadPixels(sdl.renderer,nullptr),SDL_DestroySurface);
        const auto file=a.output/("sandbox-"+std::to_string(zoom)+"x.png");check(image&&SDL_SavePNG(image.get(),file.string().c_str()),"Cannot capture actual Rule14 View");
        check(view->world().snapshot()==d.world&&view->paused(),"Capture changed World authority");
        key(*view,SDLK_F5);key(*view,SDLK_F9);check(view->world().snapshot()==d.world&&view->paused()&&q::f::read_text(copy)==input,"Per-zoom actual F5/F9 changed complete schema30 bytes or World");
        captures.push_back({{"file",file.filename().string()},{"zoom",zoom},{"tick",view->world().ticks()},{"complete_f5_f9_equality",true},
            {"target_type_original_draws",original_draws},{"target_type_authored_fallbacks",fallback_draws},{"actual_draw_status",draw_status},
            {"frame_original_house_art_configured",drawn.configured},{"frame_original_house_art_active",drawn.active}});}
    key(*view,SDLK_F9);key(*view,SDLK_F5);check(view->world().snapshot()==d.world&&q::f::read_text(copy)==input&&q::f::read_text(a.input)==input,"Post-capture F5/F9 differs");view->shutdown();
    check(source_pins(a.data)==pins,"Rendering changed original inputs");write(a.output/"report.json",{{"format","openemperor-elite-desirability-view-v1"},{"status","PASS"},
        {"rule",a.rule},{"schema",schema_for_rule(a.rule)},{"actual_sandbox_renderer",true},{"backend",SDL_GetRendererName(sdl.renderer)},{"complete_f5_f9_equality",true},
        {"target",static_cast<unsigned>(*target)},{"actual_type",visual.house_type},{"actual_side",visual.actual_side},
        {"original_house_art_configured",visual.configured},{"original_house_art_active",visual.active},{"original_house_asset_selected",visual.asset.has_value()},
        {"original_house_fallback_reason",visual.fallback_reason},{"captures",captures},{"captures_visually_inspected",false},
        {"world_unchanged",true},{"source_inputs_unchanged",true},{"native_human","NOT_RUN"}});
}
}
int main(int argc,char** argv){try{if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<oe40_cli::help<<'\n';return 0;}
    const auto args=oe40_cli::options(argc,argv);if(args.mode=="render")oe40_cli::render14(args);else oe40_cli::simulate(args);return 0;
}catch(const std::exception& error){std::cerr<<"Elite desirability proof FAIL: "<<error.what()<<'\n';return 1;}}
