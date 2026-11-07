// Authored source/map/model fixtures and actual paid World commands only.
#include "OriginalInfluenceFixture.h"
#include "persistence/RecoveryStore.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {
namespace f=oe_influence_fixture;
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace maps=openemperor::maps;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
using f::check;
template<class F>void refuses(F action,const char* why){try{action();}catch(const std::exception&){return;}throw std::runtime_error(why);}
void u32(Bytes& b,std::size_t at,std::uint32_t n){for(unsigned i=0;i<4;++i)b.at(at+i)=static_cast<std::uint8_t>(n>>(8*i));}
Bytes authored_map(const maps::MapGeometry& geometry){
    Bytes raw(static_cast<std::size_t>(maps::objects_logical_offset+maps::grid_byte_length));
    constexpr std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());u32(raw,84,226);
    for(std::size_t i=0;i<geometry.candidate.size();++i)
        u32(raw,static_cast<std::size_t>(maps::terrain_logical_offset)+4*i,geometry.candidate[i]?0x80U:0x80000U);
    Bytes result(4);u32(result,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768){
        const auto size=std::min<std::size_t>(32768,raw.size()-at);uLongf length=compressBound(static_cast<uLong>(size));Bytes block(length);
        check(compress2(block.data(),&length,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,"Authored map encoding failed");
        const auto start=result.size();result.resize(start+12);u32(result,start,0x12345678U);
        u32(result,start+4,static_cast<std::uint32_t>(length));u32(result,start+8,static_cast<std::uint32_t>(size));
        result.insert(result.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return result;
}
struct Fixture {
    f::Root root;
    maps::MapGeometry geometry{226};
    std::shared_ptr<const sim::MapPermissions> policy;
    std::shared_ptr<const openemperor::original::OriginalRulesModel> model;
    std::shared_ptr<const sim::LandTradeBoundaryFacts> boundary;
    Fixture(){
        const auto encoded=authored_map(geometry);
        f::write_text(root.data/f::map_relative,std::string(encoded.begin(),encoded.end()));
        std::vector<sim::MapCellPermission> cells(geometry.candidate.size());
        for(std::size_t i=0;i<cells.size();++i)cells[i]=geometry.candidate[i]?
            sim::MapCellPermission{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None}:
            sim::MapCellPermission{false,false,false,0,sim::BuildBlocker::OutsideMap,sim::BuildBlocker::OutsideMap};
        policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
        model=save::prepare_original_rules_model(root.data);
        boundary=save::prepare_land_trade_boundary_facts(root.data,f::map_relative);
    }
    auto mask()const{return policy->building_mask();}
    auto fresh()const{return sim::World::create_authored_land_trade(policy,model,boundary);}
    auto document(const sim::World& w)const{return save::make_document(root.data,f::map_relative,mask(),w);}
    auto restore(const save::SaveDocument& d)const{return save::restore_authored_land_trade(d,root.data,mask(),policy,model,boundary);}
};
sim::BuildingId buy(sim::World& w,sim::CommandType type,sim::Cell cell){f::buy(w,{type,cell});return *w.building_owner_at(cell);}
sim::World configured(const Fixture& f){
    auto w=f.fresh();
    // Classic Houses start with six residents. Five actual paid bodies keep
    // fourteen installed jobs staffed even after ordinary unmet-demand loss.
    for(const int x:{107,111,115,119,123})buy(w,sim::CommandType::PlaceHousehold,{x,18});
    buy(w,sim::CommandType::PlaceClaySource,{108,15});buy(w,sim::CommandType::PlacePottery,{108,11});
    const auto city=buy(w,sim::CommandType::PlaceWarehouse,{113,12});
    const auto gateway=buy(w,sim::CommandType::PlaceWarehouse,{113,1});
    for(int y=3;y<=11;++y)f::buy(w,{sim::CommandType::PlaceRoad,{114,y}});
    for(int x=110;x<=113;++x)f::buy(w,{sim::CommandType::PlaceRoad,{x,11}});
    for(int y=12;y<=16;++y)f::buy(w,{sim::CommandType::PlaceRoad,{110,y}});
    f::buy(w,sim::configure_land_trade(city,gateway));return w;
}
template<class P>void until(sim::World& w,P predicate,const char* why){for(unsigned i=0;i<1800 && !predicate();++i)w.tick();check(predicate(),why);}
const sim::CourierState& trader(const sim::World& w){
    const auto id=w.land_trade_state()->binding->courier;
    const auto found=std::find_if(w.couriers().begin(),w.couriers().end(),[&](const auto& c){return c.id==id;});
    check(found!=w.couriers().end(),"Bound actual trader missing");return *found;
}
sim::World earned_export(const Fixture& f){
    auto w=configured(f);const auto warehouse=w.land_trade_state()->binding->warehouse;
    until(w,[&]{return w.building(warehouse).pottery_stock>=4;},"Own paid production did not reach city Warehouse");
    check(w.pottery_completed_total()>=4 && w.land_trade_balance().imported==0,"Export fixture used unearned imported stock");
    f::buy(w,sim::export_pottery());w.tick();
    check(trader(w).cargo==4 && trader(w).phase==sim::CourierPhase::ToWarehouse && trader(w).edge_progress>0,
        "Earned export did not begin an actual loaded edge");return w;
}
void whole_roundtrip(const Fixture& f,const sim::World& source,const std::string& name){
    const auto file=f.root.path/(name+".json"),again=f.root.path/(name+"-again.json");
    save::write_save(file,f.document(source),f.root.data,f.mask());const auto bytes=f::read_text(file);
    const auto d=save::read_save(file);auto restored=f.restore(d);auto live=source;
    check(d.source_schema_version==37 && d.world.rule_version==21 && d.map_permissions_policy_version==3 &&
        d.world.land_trade && !d.world.original_economy && !d.world.original_production && restored.snapshot()==source.snapshot(),
        "Whole schema37/source/rule identity differs after restore");
    const auto j=Json::parse(bytes);check(j["rules"]["authored_land_trade"]==sim::authored_land_trade_contract &&
        !j["world"]["land_trade"].contains("cargo") && !j["world"]["land_trade"].contains("stock"),"Trade save duplicated physical authority");
    save::write_save(again,f.document(restored),f.root.data,f.mask());check(f::read_text(again)==bytes,"Whole schema37 canonical document differs");
    for(unsigned tick=0;tick<103;++tick){live.tick();restored.tick();check(live.snapshot()==restored.snapshot(),"Independent trade continuation differs");}
    check(f::read_text(file)==bytes && source.snapshot()==d.world,"Roundtrip changed input bytes or source World");
}
void phases_and_recovery(){
    Fixture f;auto empty=f.fresh();whole_roundtrip(f,empty,"empty");
    auto w=earned_export(f);whole_roundtrip(f,w,"export-loaded-outward");
    until(w,[&]{return w.land_trade_state()->orders.back().boundary_tick.has_value();},"Export did not physically reach gateway");
    check(trader(w).cargo==0 && w.land_trade_balance().exported==4 && w.land_trade_balance().exports_received==12,
        "Export boundary did not atomically sink four actual units and book twelve Funds");
    whole_roundtrip(f,w,"export-empty-return");
    until(w,[&]{return w.land_trade_state()->orders.back().completed_tick.has_value();},"Export ordinary return failed");
    const auto funds=w.treasury();f::buy(w,sim::import_pottery());
    check(w.treasury()==funds-24 && trader(w).cargo==0,"Import command did not pay actual finite price before empty outward trip");
    w.tick();whole_roundtrip(f,w,"import-empty-outward");
    until(w,[&]{return w.land_trade_state()->orders.back().boundary_tick.has_value();},"Import did not physically reach gateway");
    check(trader(w).cargo==4 && w.land_trade_balance().imported==4,"Import boundary did not load actual finite cargo");
    whole_roundtrip(f,w,"import-loaded-return");
    until(w,[&]{return w.land_trade_state()->orders.back().completed_tick.has_value();},"Loaded import did not reach owner Warehouse");
    whole_roundtrip(f,w,"completed");
    save::RecoveryStore store(f.root.path/"app",f.root.data);
    const auto d=f.document(w);const auto history=store.create_history(d,f.mask());
    const auto entry=store.write_checkpoint(history,d,f.mask());
    check(entry.loadable() && entry.schema==37 && entry.rule_version==21 &&
        f.restore(save::read_save(entry.path)).snapshot()==w.snapshot(),"Recovery lost complete paid trade state");
    const auto canonical=f.root.path/"recovery-canonical.json";save::write_save(canonical,d,f.root.data,f.mask());
    check(f::read_text(canonical)==f::read_text(entry.path),"Recovery payload canonical bytes differ");
    const auto branch=store.create_history(d,f.mask(),save::RecoveryParent{history,entry.sequence});
    check(!branch.empty(),"Exact selected full trade checkpoint could not branch");
    sim::World old(f.policy,sim::RulesProfile::CityV16,6);const auto old_document=f.document(old);
    refuses([&]{(void)store.write_checkpoint(history,old_document,f.mask());},"Historical generation appended to trade history");
    const auto old_history=store.create_history(old_document,f.mask());
    refuses([&]{(void)store.write_checkpoint(old_history,d,f.mask());},"Trade generation appended to historical history");
    check(store.catalog().histories.size()==3,"Valid finite trade histories did not remain available");
}
void hostile_atomic(){
    Fixture f;auto w=earned_export(f);const auto d=f.document(w);const auto snapshot=w.snapshot();
    const auto file=f.root.path/"valid.json",bad_file=f.root.path/"hostile.json";
    save::write_save(file,d,f.root.data,f.mask());const auto bytes=f::read_text(file),map=f::read_text(f.root.data/f::map_relative);
    const auto base=Json::parse(bytes);
    for(unsigned variant=0;variant<25;++variant){
        auto bad=base;auto& trade=bad["world"]["land_trade"];auto& order=trade["orders"][0];
        switch(variant){
        case 0:bad["rules"].erase("authored_land_trade");break;
        case 1:bad["rules"]["authored_land_trade"]="unknown";break;
        case 2:bad["rules"]["version"]=6;bad["schema_version"]=22;break;
        case 3:bad["world"].erase("land_trade");break;
        case 4:trade["stock"]=8;break;
        case 5:trade["model_source"]["sha256"]=std::string(64,'0');break;
        case 6:trade["model_source"]["difficulty"]=5;break;
        case 7:trade["source_map_sha256"]=std::string(64,'0');break;
        case 8:trade["next_order_id"]=1;break;
        case 9:order["id"]=0;break;
        case 10:order["direction"]=2;break;
        case 11:order["ordered_tick"]=snapshot.ticks+1;break;
        case 12:order["completed_tick"]=snapshot.ticks;break;
        case 13:trade["binding"]["warehouse"]=trade["binding"]["gateway"];break;
        case 14:trade["binding"]["courier"]=0;break;
        case 15:trade["binding"]["configured_tick"]=snapshot.ticks+1;break;
        case 16:bad["profiles"]["map_permissions"]["version"]=2;break;
        case 17:bad["world"]["treasury"]=snapshot.treasury+12;break;
        case 18:bad["world"]["construction_spent_total"]=snapshot.construction_spent_total-40;break;
        case 19:bad["world"]["unknown_cache"]=Json::array();break;
        case 20:bad["rules"]["deadline_food_dispatch"]=sim::original_deadline_food_dispatch_contract;break;
        case 21:trade["orders"].push_back(order);break;
        case 22:order["boundary_tick"]=snapshot.ticks+1;break;
        case 23:bad["schema_version"]=36;bad["rules"]["version"]=20;break;
        case 24:trade["model_source"]["byte_count"]=0;break;
        }
        f::write_text(bad_file,bad.dump());refuses([&]{(void)f.restore(save::read_save(bad_file));},"Hostile schema37 restore admitted");
        check(f::read_text(file)==bytes && w.snapshot()==snapshot && f::read_text(f.root.data/f::map_relative)==map,
            "Rejected trade JSON changed source or complete World");
    }
    auto duplicate=bytes;const std::string marker="\"authored_land_trade\": \"authored-land-trade-v1\"";
    const auto pos=duplicate.find(marker);check(pos!=std::string::npos,"Canonical trade marker absent");
    duplicate.insert(pos,marker+",");f::write_text(bad_file,duplicate);
    refuses([&]{(void)save::read_save(bad_file);},"Duplicate schema37 key admitted");
    for(unsigned variant=0;variant<8;++variant){auto bad=d;
        switch(variant){
        case 0:bad.world.land_trade.reset();break;case 1:bad.world.land_trade->next_order_id=0;break;
        case 2:bad.world.treasury+=12;break;case 3:--bad.world.construction_spent_total;break;
        case 4:bad.world.land_trade->binding->gateway=bad.world.land_trade->binding->warehouse;break;
        case 5:bad.map_permissions_sha256=std::string(64,'0');break;case 6:bad.source_schema_version=22;break;
        case 7:bad.world.source_cell_ramps.push_back({});break;
        }
        refuses([&]{save::write_save(file,bad,f.root.data,f.mask());},"Invalid trade snapshot atomically published");
        check(f::read_text(file)==bytes && w.snapshot()==snapshot,"Rejected atomic trade save changed old bytes or World");
    }
    refuses([&]{(void)save::restore_save(d,f.root.data,f.mask(),f.policy);},"Generic restore activated explicit trade generation");
    refuses([&]{(void)sim::World::restore(d.world,f.policy);},"Generic World restore activated trade without model/boundary");
    auto geometry=f.geometry.candidate;geometry[113+228]=0;
    auto forged=std::make_shared<const sim::LandTradeBoundaryFacts>(228,228,geometry,f.boundary->source_map_sha256());
    refuses([&]{(void)save::restore_authored_land_trade(d,f.root.data,f.mask(),f.policy,f.model,forged);},"Same-hash forged boundary facts granted authority");
    auto wrong_mask=f.mask();wrong_mask[113+228]=0;
    refuses([&]{(void)save::restore_authored_land_trade(d,f.root.data,wrong_mask,f.policy,f.model,f.boundary);},"Different complete legacy mask admitted");
    const auto model_path=f.root.data/"Model/EmperorBuildingModels.txt";const auto model=f::read_text(model_path);
    f::write_text(model_path,model+"\n");refuses([&]{(void)f.restore(d);},"Stale prepared model bypassed changed live model bytes");f::write_text(model_path,model);
    f::write_text(f.root.data/f::map_relative,map+"changed");refuses([&]{(void)f.restore(d);},"Changed full map source hash admitted");f::write_text(f.root.data/f::map_relative,map);
    for(auto fault:{save::WriteFault::BeforeWrite,save::WriteFault::AfterPartialWrite,save::WriteFault::BeforeRename}){
        refuses([&]{save::write_save(file,d,f.root.data,f.mask(),fault);},"Atomic trade publication fault ignored");
        check(f::read_text(file)==bytes,"Atomic fault replaced previous complete trade save");
    }
}
void historical(){
    Fixture f;
    for(unsigned version:{5U,6U,20U}){
        auto w=version==20?sim::World::create_authored_source_cell_ramps(f.policy):sim::World(f.policy,sim::RulesProfile::CityV16,version);
        const auto file=f.root.path/("old-"+std::to_string(version)+".json"),again=f.root.path/"old-again.json";
        const auto d=f.document(w);save::write_save(file,d,f.root.data,f.mask());const auto bytes=f::read_text(file);const auto j=Json::parse(bytes);
        check(!j["rules"].contains("authored_land_trade") && !j["world"].contains("land_trade"),"Historical generation acquired trade fields");
        auto restored=save::restore_save(save::read_save(file),f.root.data,f.mask(),f.policy);
        save::write_save(again,f.document(restored),f.root.data,f.mask());check(f::read_text(again)==bytes,"Historical complete canonical bytes changed");
        refuses([&]{(void)f.restore(d);},"Historical save migrated to trade generation");
    }
}
fs::path private_path(const char* raw,bool input){
    const auto allowed=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/oe54";
    std::error_code error;const auto p=fs::weakly_canonical(fs::absolute(raw),error);
    check(!error && p!=allowed && std::mismatch(allowed.begin(),allowed.end(),p.begin(),p.end()).first==allowed.end(),"Use strict private .local/oe54 path");
    for(auto q=fs::absolute(raw);!q.empty();q=q.parent_path()){check(!fs::is_symlink(q),"No linked proof paths");if(q==q.root_path())break;}
    check(input?fs::is_regular_file(p):!fs::exists(p),"Preserve existing proof inputs/outputs");return p;
}
unsigned ticks(const char* raw){unsigned n=0;const std::string_view s=raw;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
    check(p.ec==std::errc{} && p.ptr==s.data()+s.size() && n<=12000,"Bounded continuation must be 0..12000");return n;}
int process(int argc,char** argv){
    Fixture f;
    if((argc==3 || argc==4) && std::string_view(argv[1])=="--create"){
        const auto out=private_path(argv[2],false);auto w=earned_export(f);const auto n=argc==4?ticks(argv[3]):0;
        for(unsigned i=0;i<n;++i)w.tick();
        save::write_save(out,f.document(w),f.root.data,f.mask());return 0;
    }
    check((argc==4 || argc==5) && std::string_view(argv[1])=="--restore","Use --create OUTPUT [TICKS] or --restore INPUT OUTPUT [TICKS]");
    const auto in=private_path(argv[2],true),out=private_path(argv[3],false);const auto bytes=f::read_text(in);
    auto w=f.restore(save::read_save(in));const auto n=argc==5?ticks(argv[4]):0;
    for(unsigned i=0;i<n;++i)w.tick();
    save::write_save(out,f.document(w),f.root.data,f.mask());
    check(f::read_text(in)==bytes,"Independent restore changed input save");if(!n)check(f::read_text(out)==bytes,"Independent whole-document re-encode differs");return 0;
}
}
int main(int argc,char** argv){try{if(argc>1)return process(argc,argv);phases_and_recovery();hostile_atomic();historical();
    std::cout<<"Rule21/schema37 finite paid land trade Save/Recovery, all cargo phases, hostile guards and historical bytes PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"Land trade save FAIL: "<<e.what()<<'\n';return 1;}}
