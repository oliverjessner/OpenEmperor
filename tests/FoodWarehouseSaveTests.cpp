#include "FoodWarehouseFixture.h"
#include "persistence/RecoveryStore.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace t=oe_food_warehouse_fixture;
namespace sim=t::sim;
namespace save=t::save;
namespace f=t::f;
using Json=nlohmann::json;
using t::check;
template<class F>void refuses(F action,const char* why){try{action();}catch(const std::exception&){return;}throw std::runtime_error(why);}
std::string roundtrip(const t::Fixture& fixture,const sim::World& world,const std::filesystem::path& path,int continuation=3){
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto json=Json::parse(bytes);const auto doc=save::read_save(path);
    check(doc.source_schema_version==51&&doc.world.rule_version==35&&doc.world==before&&doc.world.food_warehouses&&
        doc.world.processed_food&&doc.world.food_processing&&!doc.world.hunting_food&&
        json["rules"]["food_warehouses"]==sim::food_warehouses_contract&&json["rules"]["food_warehouses_revision"]==1&&
        json["world"]["food_warehouses"]["revision"]==1&&json["world"]["processed_food"]["catalog"]==sim::processed_food_catalog,
        "Schema51 lost exclusive Food authority, complete history or exact generation");
    auto restored=save::restore_food_warehouses(doc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);check(restored.snapshot()==before,"Schema51 changed paid state on exact restore");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Schema51 canonical roundtrip changed bytes or live World");
    auto expected=world;for(int n=0;n<continuation;++n){expected.tick();restored.tick();check(expected.snapshot()==restored.snapshot(),"Schema51 independent continuation diverged");}
    t::valid(expected);t::valid(restored);return bytes;
}
t::Scene earned_pipeline(sim::World& world){
    constexpr std::array<std::size_t,1> kinds{0};auto city=t::build(world,true,kinds);
    const auto millet=t::single_crop_farm(world,1,{94,125}),soy=t::single_crop_farm(world,4,{104,125});
    for(const auto id:{city.mill,city.market,city.shop})t::operate(world,id,false);
    t::until(world,[&]{return t::kinds_held(world,city.warehouse)>=3;},"Save control never earned three simultaneous Warehouse Food kinds");
    for(const auto id:{city.farm,millet,soy})t::operate(world,id,false);
    city.second_mill=t::add_second_mill(world);f::buy(world,sim::set_mill_recipe(city.second_mill,3));
    t::until(world,[&]{return t::stock(world,city.warehouse).received[9]>0;},"Save control never earned real Warehouse-routed grade3 meals");
    t::operate(world,city.second_mill,false);city.second_market=t::add_second_market(world);t::operate(world,city.shop,true);
    t::until(world,[&]{return t::stock(world,city.second_market).received!=sim::ProcessedFoodCounters{}&&t::stock(world,city.shop).received[9]>0;},
        "Save control lacks actual Warehouse to Market and Shop accepted receipts");
    t::until(world,[&]{for(const auto& h:world.food_processing_state()->houses)if(h.first_consumption&&h.first_consumption->grade==3)return true;return false;},
        "Save control lacks real Warehouse-routed House meal consumption");
    return city;
}
void paid_pipeline_roundtrip_recovery_and_twelve_thousand_ticks(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> kinds{0};auto city=t::build(world,true,kinds);
    const auto millet=t::single_crop_farm(world,1,{94,125}),soy=t::single_crop_farm(world,4,{104,125});
    const auto path=fixture.root.path/"warehouses51.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=[&]{roundtrip(fixture,world,path);const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==51&&entry.rule_version==35&&fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
            "Schema51 recovery lost physical Food or causal receipts");};
    checkpoint();for(const auto id:{city.mill,city.market,city.shop})t::operate(world,id,false);
    t::until(world,[&]{return t::kinds_held(world,city.warehouse)>=3;},"No three-kind earned storage");checkpoint();
    for(const auto id:{city.farm,millet,soy})t::operate(world,id,false);
    city.second_mill=t::add_second_mill(world);f::buy(world,sim::set_mill_recipe(city.second_mill,3));
    t::until(world,[&]{return t::mill(world,city.second_mill).active.has_value();},"No actual Warehouse-fed active recipe");checkpoint();
    t::until(world,[&]{return t::stock(world,city.warehouse).received[9]>0;},"No actual stored meal return");checkpoint();
    t::operate(world,city.second_mill,false);city.second_market=t::add_second_market(world);t::operate(world,city.shop,true);
    const auto channel=t::channel(world,city.warehouse);
    t::until(world,[&]{for(const auto& trip:world.processed_food_state()->trips)if(trip.transfer.courier_id==channel&&t::cargo(trip)>0)return true;return false;},
        "No loaded real Warehouse outbound Cart");checkpoint();
    t::operate(world,city.warehouse,false);checkpoint();
    t::until(world,[&]{return t::cargo(t::trip(world,channel))==0;},"Paused Warehouse did not physically return its actual cargo");checkpoint();
    t::operate(world,city.warehouse,true);
    t::until(world,[&]{return t::stock(world,city.second_market).received!=sim::ProcessedFoodCounters{}&&t::stock(world,city.shop).received[9]>0;},
        "Warehouse did not physically deliver raw and meal stock after restart");
    t::until(world,[&]{for(const auto& h:world.food_processing_state()->houses)if(h.first_consumption&&h.first_consumption->grade==3)return true;return false;},
        "Warehouse meals did not reach real House consumption");checkpoint();roundtrip(fixture,world,path,12000);
    check(recovery.catalog().histories.size()==1&&!recovery.catalog().truncated,"Warehouse recovery exceeded its bounded history");
}
void hostile_warehouse_provenance_is_strict_and_atomic(){
    t::Fixture fixture;auto world=fixture.world();const auto city=earned_pipeline(world);
    const auto path=fixture.root.path/"hostile51.json";const auto bytes=roundtrip(fixture,world,path);const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* reason){auto hostile=original;mutate(hostile);const auto input=hostile.dump();f::write_text(path,input);
        refuses([&]{(void)fixture.restore(save::read_save(path));},reason);
        check(world.snapshot()==before&&f::read_text(path)==input,"Rejected Warehouse restore changed authority or supplied bytes");f::write_text(path,bytes);};
    reject([](auto& j){j["rules"].erase("food_warehouses");},"Missing Warehouse marker accepted");
    reject([](auto& j){j["rules"]["food_warehouses"]="other";},"Unknown Warehouse marker accepted");
    reject([](auto& j){j["rules"]["food_warehouses_revision"]=2;},"Unknown Warehouse rule revision accepted");
    reject([](auto& j){j["world"].erase("food_warehouses");},"Missing Warehouse history accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["revision"]=2;},"Unknown Warehouse state revision accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["amounts"]={1,2,3};},"Parallel Warehouse Food inventory accepted");
    reject([](auto& j){auto& rows=j["world"]["food_warehouses"]["warehouses"];rows.push_back(rows[0]);},"Duplicate Warehouse owner accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"].clear();},"Missing paid Warehouse row accepted");
    reject([](auto& j){auto& rows=j["world"]["food_warehouses"]["markets"];rows.push_back(rows[0]);},"Duplicate paid Market row accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["markets"].clear();},"Missing paid Market receipt rows accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_received"].push_back(nullptr);},"Wrong incoming width accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_delivered"].erase(0);},"Wrong outgoing width accepted");
    reject([](auto& j){auto& row=j["world"]["food_warehouses"]["warehouses"][0];row["first_received"][11]=row["first_received"][0];},"Unavailable grade5 provenance accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_received"][0]=nullptr;},"Actual incoming receipt lost first provenance");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_delivered"][9]=nullptr;},"Actual accepted output lost first destination receipt");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_received"][0]["amount"]=0;},"Zero first Food amount accepted");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_received"][0]["tick"]=1;},"Warehouse input borrowed a later real crop origin");
    reject([](auto& j){auto& row=j["world"]["food_warehouses"]["warehouses"][0];row["first_received"][0]["source"]=row["warehouse_id"];},"Warehouse-to-Warehouse raw receipt accepted");
    reject([](auto& j){auto& row=j["world"]["food_warehouses"]["warehouses"][0];row["first_delivered"][9]["tick"]=row["first_received"][9]["tick"];},"Warehouse output borrowed stock before its real input and forwarding hop");
    reject([&](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_delivered"][9]["target"]=static_cast<std::uint32_t>(city.second_market);},"Warehouse sent meals through Market semantics");
    reject([](auto& j){j["world"]["food_warehouses"]["warehouses"][0]["first_delivered"][9]["mirror"]=1;},"Unknown outgoing authority accepted");
    reject([](auto& j){auto& row=j["world"]["food_warehouses"]["warehouses"][0];row["first_delivered"][9]["transfer"]=row["first_received"][9]["transfer"];},"One transfer key certified two different directed Food hops");
    reject([](auto& j){auto& rows=j["world"]["food_warehouses"]["markets"];for(auto& market:rows)for(auto& a:market["first_received"])if(!a.is_null()){a["tick"]=1;return;}},"Market input borrowed future producer or Warehouse cargo");
    reject([](auto& j){for(auto& mill:j["world"]["food_processing"]["mills"])for(auto& a:mill["first_ingredients"])if(!a.is_null()){a["tick"]=1;return;}},"Warehouse-fed Mill borrowed future actual ingredients");
    auto duplicate=bytes;const std::string key="\"food_warehouses_revision\": 1";const auto pos=duplicate.find(key);
    check(pos!=std::string::npos,"Duplicate control lost Warehouse revision");duplicate.replace(pos,key.size(),key+",\n    "+key);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate schema51 key accepted");f::write_text(path,bytes);
    auto forged=fixture.document(world);forged.world.food_warehouses->warehouses[0].first_delivered[9]->tick=1;
    refuses([&]{save::write_save(path,forged,fixture.root.data,fixture.mask());},"Writer published forged Warehouse destination receipt");
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Rejected Warehouse writer changed valid target bytes or World");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Atomic Warehouse publication fault ignored");
    check(f::read_text(path)==bytes,"Failed Warehouse publication changed prior target bytes");
}
void historical50_and_cross_generation_targets_stay_exact(){
    t::Fixture fixture;auto current=fixture.world();t::build(current);
    const auto path=fixture.root.path/"current51.json";const auto bytes=roundtrip(fixture,current,path);
    auto old=sim::World::create_food_processing(fixture.policy,fixture.model,fixture.terrain);t::p::build(old);
    const auto old_path=fixture.root.path/"historical50.json";save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());
    const auto old_bytes=f::read_text(old_path);const auto doc=save::read_save(old_path);const auto json=Json::parse(old_bytes);
    check(doc.source_schema_version==50&&!doc.world.food_warehouses&&!json["world"].contains("food_warehouses")&&!json["rules"].contains("food_warehouses")&&
        sim::World::restore_food_processing(doc.world,fixture.policy,fixture.model,fixture.terrain).snapshot()==old.snapshot(),"Historical50 acquired Warehouse authority");
    save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());check(f::read_text(old_path)==old_bytes,"Historical50 canonical bytes changed");
    refuses([&]{(void)save::restore_food_warehouses(doc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},"Schema50 automatically migrated to51");
    refuses([&]{(void)sim::World::restore_food_processing(current.snapshot(),fixture.policy,fixture.model,fixture.terrain);},"Rule34 factory admitted Rule35");
    refuses([&]{save::write_save(old_path,fixture.document(current),fixture.root.data,fixture.mask());},"Schema51 writer overwrote50");
    refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"Schema50 writer overwrote51");
    check(f::read_text(path)==bytes&&f::read_text(old_path)==old_bytes,"Cross-generation refusal changed supplied target bytes");
    auto hidden=fixture.document(old);hidden.world.food_warehouses.emplace();
    refuses([&]{save::write_save(old_path,hidden,fixture.root.data,fixture.mask());},"Historical writer silently dropped new Warehouse authority");
    for(const char* damaged:{"{\"schema_version\":51,broken","{\"schema_version\":\"51\",broken"}){
        f::write_text(path,damaged);refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"Historical writer replaced damaged51 target");
        check(f::read_text(path)==damaged,"Damaged51 target protection changed supplied bytes");}
}
}
int main(){try{paid_pipeline_roundtrip_recovery_and_twelve_thousand_ticks();hostile_warehouse_provenance_is_strict_and_atomic();historical50_and_cross_generation_targets_stay_exact();
    std::cout<<"Food Warehouse strict schema51 Save/Recovery, causal receipts and12k replay passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
