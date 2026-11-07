#include "persistence/SandboxSave.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using Json=nlohmann::json;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void refuses(F action,const char* why){try{action();}catch(const std::exception&){return;}throw std::runtime_error(why);}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"Test input missing");return {std::istreambuf_iterator<char>(in),{}};}
void text(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<s;check(bool(out),"Fixture write failed");}
void buy(sim::World& w,sim::Command c){const auto result=w.execute(c);if(!result.accepted)throw std::runtime_error("Paid command refused: "+result.reason);}
fs::path own_temporary_root(){
    const auto parent=fs::canonical(fs::temp_directory_path());
    const auto name="openemperor-source-cell-save-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for(unsigned attempt=0;attempt<256;++attempt){
        const auto path=parent/(name+"-"+std::to_string(attempt));std::error_code error;
        if(fs::create_directory(path,error))return path;
        if(error && error!=std::errc::file_exists)throw fs::filesystem_error("Reserve private fixture directory",path,error);
    }
    throw std::runtime_error("Cannot reserve own private fixture directory");
}
struct Fixture {
    fs::path root=own_temporary_root();
    fs::path data=root/"data",relative="Cities/AuthoredSourceCell.map";
    std::vector<std::uint8_t> mask=std::vector<std::uint8_t>(24U*16U,1);
    std::shared_ptr<const sim::MapPermissions> policy;
    Fixture(){
        // Authored source identity and immutable rectangular step, not copied
        // Emperor bytes and not evidence of original raw-slope admission.
        text(data/relative,"OpenEmperor authored 24x16 source-cell step fixture v1\n");
        std::vector<sim::MapCellPermission> cells(mask.size(),{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
        for(int y=0;y<16;++y)for(int x=8;x<24;++x)cells[std::size_t(y*24+x)].height=1;
        policy=std::make_shared<const sim::MapPermissions>(24,16,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
    }
    ~Fixture(){std::error_code e;fs::remove_all(root,e);}
    auto fresh() const{return sim::World::create_authored_source_cell_ramps(policy);}
    auto document(const sim::World& w) const{return save::make_document(data,relative,mask,w);}
    auto restore(const save::SaveDocument& d) const{return save::restore_authored_source_cell_ramps(d,data,mask,policy);}
};
sim::World earned(const Fixture& f){
    auto w=f.fresh();
    for(const auto c:std::array<sim::Command,4>{{{sim::CommandType::PlaceHousehold,{1,1}},
        {sim::CommandType::PlaceHousehold,{1,3}},{sim::CommandType::PlaceClaySource,{4,2}},
        {sim::CommandType::PlacePottery,{14,2}}}})buy(w,c);
    for(int x=4;x<=15;++x)buy(w,{sim::CommandType::PlaceRoad,{x,5}});
    for(const auto c:{sim::Cell{5,4},sim::Cell{14,4},sim::Cell{7,9},sim::Cell{8,9}})buy(w,{sim::CommandType::PlaceRoad,c});
    buy(w,sim::place_ramp({7,9},{8,9}));
    buy(w,sim::place_source_cell_ramp({7,5},{8,5}));
    for(int tick=0;tick<6000;++tick){
        w.tick();
        for(const auto& c:w.couriers())if(c.cargo>0 && c.edge_progress>0 && c.path_vertex+1<c.path.size() &&
            w.source_cell_ramp_claim(c.path[c.path_vertex]) && w.source_cell_ramp_claim(c.path[c.path_vertex+1]))return w;
    }
    throw std::runtime_error("Real loaded begun source-cell edge not reached");
}
void canonical_and_recovery(){
    Fixture f;auto w=earned(f);const auto original=w.snapshot();
    const auto path=f.root/"earned36.json",roundtrip=f.root/"roundtrip36.json";
    save::write_save(path,f.document(w),f.data,f.mask);const auto full=bytes(path);
    const auto d=save::read_save(path);const auto j=Json::parse(full);
    check(d.source_schema_version==36 && d.world.rule_version==20 && d.map_permissions_policy_version==3,
        "New exact schema/rule/policy identity absent");
    check(j.at("rules").at("source_cell_ramps")==sim::authored_source_cell_ramp_contract &&
        j.at("world").at("source_cell_ramps").size()==1 && d.world.ramps.size()==1 &&
        !d.world.original_production && !d.world.original_economy,"New slice imported Original authority or lost classic RampLink");
    auto restored=f.restore(d);check(restored.snapshot()==original,"Full source-cell snapshot differs immediately");
    save::write_save(roundtrip,f.document(restored),f.data,f.mask);check(bytes(roundtrip)==full,"Whole schema36 document bytes differ");
    for(int i=0;i<103;++i){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Independent deterministic continuation differs");}
    auto empty=f.fresh();const auto empty_file=f.root/"empty36.json";
    save::write_save(empty_file,f.document(empty),f.data,f.mask);
    const auto e=Json::parse(bytes(empty_file));check(e["rules"]["source_cell_ramps"]==sim::authored_source_cell_ramp_contract &&
        e["world"]["source_cell_ramps"].empty() && e["world"]["next_source_cell_ramp_id"]==1,"Empty new generation lost marker/DTO");
    save::RecoveryStore store(f.root/"app",f.data);
    const auto history=store.create_history(f.document(restored),f.mask);
    const auto entry=store.write_checkpoint(history,f.document(restored),f.mask);
    check(entry.schema==36 && entry.rule_version==20 && entry.loadable(),"Normal classic Recovery lost schema36");
    check(f.restore(save::read_save(entry.path)).snapshot()==restored.snapshot(),"Recovery complete source-cell authority differs");
    const auto recovered=f.root/"recovered.json";auto recovery_document=save::read_save(entry.path);
    recovery_document.prepared_permissions=f.policy;
    save::write_save(recovered,recovery_document,f.data,f.mask);
    check(bytes(entry.path)==bytes(recovered),"Recovery payload bytes changed");
}
void hostile_and_atomic(){
    Fixture f;auto w=earned(f);const auto d=f.document(w);const auto snap=w.snapshot();
    const auto valid=f.root/"valid.json",hostile=f.root/"hostile.json";
    save::write_save(valid,d,f.data,f.mask);const auto original=bytes(valid),map=bytes(f.data/f.relative);
    const auto base=Json::parse(original);
    for(int variant=0;variant<18;++variant){
        auto bad=base;auto& r=bad["world"]["source_cell_ramps"][0];
        switch(variant){
        case 0:bad["rules"].erase("source_cell_ramps");break;
        case 1:bad["rules"]["source_cell_ramps"]="unknown";break;
        case 2:bad["schema_version"]=22;bad["rules"]["version"]=6;break;
        case 3:bad["rules"]["version"]=19;break;
        case 4:bad["world"].erase("next_source_cell_ramp_id");break;
        case 5:bad["world"].erase("source_cell_ramps");break;
        case 6:r["unknown_cache"]={1,2,3};break;
        case 7:bad["world"]["source_cell_ramp_cache"]={1,2,3};break;
        case 8:r["low_height"]=0.5;break;
        case 9:r["high_height"]=128;break;
        case 10:r["paid_cost"]=0;break;
        case 11:r["id"]=0;break;
        case 12:bad["profiles"]["map_permissions"]["version"]=2;break;
        case 13:bad["world"]["source_cell_ramps"]=Json::array();for(int n=0;n<65;++n)bad["world"]["source_cell_ramps"].push_back(base["world"]["source_cell_ramps"][0]);break;
        case 14:bad["rules"]["deadline_food_dispatch"]=sim::original_deadline_food_dispatch_contract;break;
        case 15:bad["unknown_root"]=1;break;
        case 16:r["source_cell"]={-1,5};break;
        case 17:r["placed_tick"]=snap.ticks+1;break;
        }
        text(hostile,bad.dump());refuses([&]{auto p=save::read_save(hostile);(void)f.restore(p);},"Hostile schema36 JSON admitted");
        check(bytes(valid)==original && w.snapshot()==snap && bytes(f.data/f.relative)==map,"Hostile read/restore mutated protected state");
    }
    text(hostile,"{\"format\":\"openemperor-sandbox-save\",\"schema_version\":36,\"schema_version\":36}");
    refuses([&]{(void)save::read_save(hostile);},"Duplicate schema36 keys admitted");
    for(int variant=0;variant<12;++variant){
        auto bad=d;auto& r=bad.world.source_cell_ramps[0];
        switch(variant){
        case 0:r.id.value=0;break;case 1:bad.world.next_source_cell_ramp_id=1;break;
        case 2:r.high_neighbor={8,6};break;case 3:r.source_cell={8,5};break;
        case 4:r.low_height=-128;r.high_height=127;break;case 5:r.paid_cost=39;break;
        case 6:r.placed_tick=snap.ticks+1;break;case 7:--bad.world.construction_spent_total;break;
        case 8:bad.world.source_cell_ramps.push_back(r);bad.world.next_source_cell_ramp_id=3;break;
        case 9:bad.world.rule_version=6;break;case 10:bad.source_schema_version=22;break;
        case 11:bad.map_permissions_sha256=std::string(64,'0');break;
        }
        refuses([&]{save::write_save(valid,bad,f.data,f.mask);},"Malformed source-cell snapshot atomically published");
        check(bytes(valid)==original && w.snapshot()==snap,"Rejected write altered old complete bytes or live World");
    }
    auto wrong_mask=f.mask;wrong_mask[0]=0;refuses([&]{(void)save::restore_save(d,f.data,wrong_mask,f.policy);},"Wrong mask binding admitted");
    auto cells=f.policy->cells();cells[5U*24U+8U].height=2;
    auto changed=std::make_shared<const sim::MapPermissions>(24,16,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
    refuses([&]{(void)save::restore_save(d,f.data,f.mask,changed);},"Different source permissions admitted");
    text(f.data/f.relative,map+"changed");refuses([&]{(void)f.restore(d);},"Different complete source-map hash admitted");text(f.data/f.relative,map);
    for(auto fault:{save::WriteFault::BeforeWrite,save::WriteFault::AfterPartialWrite,save::WriteFault::BeforeRename}){
        refuses([&]{save::write_save(valid,d,f.data,f.mask,fault);},"Atomic publication fault ignored");
        check(bytes(valid)==original,"Atomic publication fault replaced previous valid bytes");
    }
}
void historical_generations(){
    Fixture f;
    for(unsigned version:{5U,6U}){
        sim::World old(f.policy,sim::RulesProfile::CityV16,version);
        const auto path=f.root/("old"+std::to_string(version)+".json"),again=f.root/"old-again.json";
        save::write_save(path,f.document(old),f.data,f.mask);const auto full=bytes(path);const auto j=Json::parse(full);
        check(j["schema_version"]==(version==5?21:22) && !j["rules"].contains("source_cell_ramps") &&
            !j["world"].contains("source_cell_ramps") && !j["world"].contains("next_source_cell_ramp_id"),"Historical save acquired new authority");
        const auto parsed=save::read_save(path);auto restored=save::restore_save(parsed,f.data,f.mask,f.policy);
        save::write_save(again,f.document(restored),f.data,f.mask);check(bytes(again)==full,"Historical bridge/ramp canonical bytes changed");
        refuses([&]{(void)f.restore(parsed);},"Old generation silently upgraded to Rule20");
    }
}
fs::path private_path(const char* raw,bool input){
    const auto allowed=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/oe53";
    std::error_code error;const auto p=fs::weakly_canonical(fs::absolute(raw),error);
    check(!error && p!=allowed && std::mismatch(allowed.begin(),allowed.end(),p.begin(),p.end()).first==allowed.end(),
        "Use an explicit strict private .local/oe53 path");
    for(auto q=fs::absolute(raw);!q.empty();q=q.parent_path()){check(!fs::is_symlink(q),"No linked proof paths");if(q==q.root_path())break;}
    check(input ? fs::is_regular_file(p) : !fs::exists(p),"Preserve existing private inputs/outputs");return p;
}
unsigned continuation_ticks(const char* raw){
    unsigned ticks=0;const std::string_view value=raw;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),ticks);
    check(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&ticks<=12000,"Invalid bounded continuation");return ticks;
}
int process(int argc,char** argv){
    Fixture f;
    if((argc==3||argc==4) && std::string_view(argv[1])=="--create"){
        const auto out=private_path(argv[2],false);auto w=earned(f);
        const auto ticks=argc==4?continuation_ticks(argv[3]):0;
        for(unsigned i=0;i<ticks;++i)w.tick();
        save::write_save(out,f.document(w),f.data,f.mask);return 0;
    }
    check((argc==4 || argc==5) && std::string_view(argv[1])=="--restore","Use --create OUTPUT [TICKS] or --restore INPUT OUTPUT [TICKS]");
    const auto in=private_path(argv[2],true),out=private_path(argv[3],false);const auto original=bytes(in);
    auto w=f.restore(save::read_save(in));unsigned ticks=0;
    if(argc==5)ticks=continuation_ticks(argv[4]);
    for(unsigned i=0;i<ticks;++i)w.tick();save::write_save(out,f.document(w),f.data,f.mask);
    check(bytes(in)==original,"Separate restore mutated input");if(!ticks)check(bytes(out)==original,"Separate complete re-encode differs");return 0;
}
}
int main(int argc,char** argv){try{if(argc>1)return process(argc,argv);canonical_and_recovery();hostile_and_atomic();historical_generations();
    std::cout<<"Rule20/schema36 authored source-cell Save/Recovery, paid cargo, strict hostile guards and historical bytes PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"Source-cell save FAIL: "<<e.what()<<'\n';return 1;}}
