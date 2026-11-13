// Explicit Rule25 Wheat/Millet adapter. All maps/models are authored inputs;
// paid commands and actual ticks earn every resident, portion and receipt.
#include "PlantationsFixture.h"
#include "persistence/RecoveryStore.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <nlohmann/json.hpp>

namespace {
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using p::check;
using Json=nlohmann::json;

struct Fixture : p::Fixture {
    explicit Fixture(bool with_luxuries=false) {
        auto text=p::authored_model();
        // Keep actual Water/Ancestor and positive Food requirements at the
        // luxury-consuming row. Initial paid households still earn workers.
        oe_production_fixture::set_house_field(text,2,3,1);
        oe_production_fixture::set_house_field(text,2,8,1);
        oe_production_fixture::set_house_field(text,2,21,1);
        for(const unsigned need:{11U,12U,13U})
            oe_production_fixture::set_house_field(text,2,need,with_luxuries ? 1:0);
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_agriculture_rules_model(root.data);
    }
};
struct Foods {
    sim::BuildingId wheat{},millet{},wheat_field{},millet_field{};
};
sim::BuildingId crop(sim::World& world,sim::BuildingId farm,sim::CommandType type,sim::Cell cell) {
    sim::Command command{type,cell};command.building_id=farm;
    f::buy(world,command);return *world.building_owner_at(cell);
}
Foods foods(sim::World& world,bool northern=false) {
    Foods result;
    const int y=northern ? 17:25;
    if(northern) {
        for(int x=19;x<=32;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,20}});
        for(int y2=21;y2<24;++y2)f::buy(world,{sim::CommandType::PlaceRoad,{19,y2}});
    }
    result.wheat=p::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,y});
    result.millet=p::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{27,y});
    result.wheat_field=crop(world,result.wheat,sim::CommandType::PlaceOriginalWheatField,{23,y});
    result.millet_field=crop(world,result.millet,sim::CommandType::PlaceOriginalMilletField,{30,y});
    return result;
}
struct Scene {Foods farms;sim::BuildingId market{};std::vector<sim::BuildingId> homes,watches;};
Scene basic_scene(sim::World& world) {
    Scene scene;
    for(int x=20;x<=51;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<44;x+=3)scene.homes.push_back(p::purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
    scene.farms=foods(world);
    scene.market=p::purchase(world,sim::CommandType::PlaceMarket,{33,25});
    (void)p::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{37,25});
    (void)p::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{41,25});
    for(std::size_t index=0;index<sim::Rules::fire_watch_limit;++index)
        scene.watches.push_back(p::purchase(world,sim::CommandType::PlaceFireWatch,{45+3*static_cast<int>(index),25}));
    check(!world.total_population(),"Food construction injected residents");
    return scene;
}
int volume(const sim::AgricultureAmounts& amounts) {return std::accumulate(amounts.begin(),amounts.end(),0);}
unsigned quality(const sim::World& world,sim::BuildingId id) {
    for(const auto& home:world.original_economy_state()->houses)if(home.building_id==id)return home.house.food_quality;
    throw std::runtime_error("Actual Food recipient lost its sole House quality");
}
void conserved(const sim::World& world) {
    p::valid(world);
    check(world.original_production_state()->food==sim::OriginalFoodState{},
          "Rule25 mirrored its five-kind Food into the historical two-kind state");
    const auto& food=*world.original_agriculture_state();
    sim::AgricultureCounters produced{},stock{},cargo{},consumed{};
    for(const auto& field:food.fields) {
        check(!field.quantity && !field.productive_events && !field.last_harvest_cycle,
              "Authored Food adapter claimed Original field work/quantity/calendar authority");
        const auto index=static_cast<std::size_t>(field.crop);
        check(index<5 && (index<2 || !field.harvested),"Locked Rice/Cabbage/Soy acquired a harvest");
        produced[index]+=field.harvested;
    }
    for(const auto& row:food.stocks) {
        const auto kind=world.building(row.building_id).kind;
        check(volume(row.amounts)<= (kind==sim::Object::Market ? 16:8),"Five-kind Food exceeded shared physical storage");
        for(std::size_t index=0;index<5;++index) {
            check(row.amounts[index]>=0,"Five-kind Food became negative");
            std::uint64_t origin=0;
            for(const auto& field:food.fields)if(field.farm_id==row.building_id && static_cast<std::size_t>(field.crop)==index)origin+=field.harvested;
            check(origin+row.received[index]+row.returned[index]==
                  static_cast<std::uint64_t>(row.amounts[index])+row.departed[index]+row.consumed[index],
                  "Typed Food owner origin/arrival/departure/consumption equation failed");
            stock[index]+=static_cast<std::uint64_t>(row.amounts[index]);consumed[index]+=row.consumed[index];
            if(index>=2)check(!row.amounts[index] && !row.received[index] && !row.departed[index] &&
                              !row.returned[index] && !row.consumed[index],"Food adapter activated a locked crop column");
        }
        if(sim::original_house_kind(kind)) {
            check(volume(row.amounts)>0 || quality(world,row.building_id)==0,"Empty House retained FoodQuality");
            check(!volume(row.amounts) || row.quality_witness.has_value(),"Positive House Food lacks an actual arrival witness");
        }
    }
    for(const auto& trip:food.trips)for(std::size_t index=0;index<5;++index) {
        const auto remaining=trip.loaded[index]-trip.delivered[index]-trip.returned[index];
        check(remaining>=0,"Typed Food transfer duplicated or lost cargo");cargo[index]+=static_cast<std::uint64_t>(remaining);
    }
    for(std::size_t index=0;index<5;++index)
        check(produced[index]==stock[index]+cargo[index]+consumed[index],"Typed Food global conservation failed");
}
template<class Predicate> void until(sim::World& world,Predicate predicate,const char* reason,int limit=16000) {
    for(int tick=0;tick<limit && !predicate();++tick)world.tick();
    check(predicate(),reason);conserved(world);
}
template<class Action> void refuses(Action action,const char* reason) {
    try {action();}catch(const std::exception&){return;}
    throw std::runtime_error(reason);
}
void closed_rule24_and_new_crops() {
    Fixture fixture;
    auto old=sim::World::create_original_agriculture(fixture.policy,fixture.model,fixture.terrain);
    const auto old_scene=basic_scene(old);
    for(int tick=0;tick<1600;++tick)old.tick();
    check(old.rule_version()==24,"Historical Agriculture generation changed");
    for(const auto& field:old.original_agriculture_state()->fields)check(!field.harvested,"Rule24 acquired the Rule25 Food adapter");
    for(const auto& row:old.original_agriculture_state()->stocks)
        check(!volume(row.amounts) && !row.quality_witness,"Rule24 gained Food or FoodQuality");
    check(!old.building(old_scene.farms.wheat).progress && !old.building(old_scene.farms.millet).progress,
          "Rule24 advanced an authored Food timer");
    conserved(old);

    auto closed=fixture.world();const auto scene=a::build(closed);
    (void)crop(closed,scene.cabbage_farm,sim::CommandType::PlaceOriginalRiceField,{23,26});
    for(int tick=0;tick<1600;++tick)closed.tick();
    check(closed.building_staffed(scene.cabbage_farm),"Closed-crop control never earned actual Farm workers");
    for(const auto& field:closed.original_agriculture_state()->fields)check(!field.harvested,"New crop used the W/M adapter");
    for(const auto& row:closed.original_agriculture_state()->stocks)
        check(!volume(row.amounts) && !row.quality_witness,"Rice/Cabbage/Soy generated unearned Food");
    conserved(closed);

    auto unstaffed=fixture.world();const auto farm=p::purchase(unstaffed,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    const auto field=crop(unstaffed,farm,sim::CommandType::PlaceOriginalWheatField,{23,25});
    for(int tick=0;tick<200;++tick)unstaffed.tick();
    check(!unstaffed.building_staffed(farm) && !unstaffed.building(farm).progress && !a::field(unstaffed,field).harvested,
          "Unstaffed paid Wheat generated free Food");conserved(unstaffed);
}
void typed_paid_arrivals_pause_and_full_storage() {
    Fixture fixture;auto world=fixture.world();const auto scene=basic_scene(world);
    const auto& farms=scene.farms;
    sim::Command second{sim::CommandType::PlaceOriginalMilletField,{23,26}};second.building_id=farms.wheat;
    p::denied(world,second);second.type=sim::CommandType::PlaceOriginalWheatField;p::denied(world,second);
    (void)crop(world,farms.wheat,sim::CommandType::PlaceOriginalRiceField,{23,26});
    (void)crop(world,farms.wheat,sim::CommandType::PlaceOriginalCabbageField,{23,27});
    (void)crop(world,farms.millet,sim::CommandType::PlaceOriginalSoybeanField,{30,26});
    f::buy(world,sim::set_building_operation(farms.millet,false));
    until(world,[&]{return world.building(farms.wheat).progress>0;},"Paid workers never advanced Wheat's authored timer");
    const auto progress=world.building(farms.wheat).progress;const auto harvested=a::field(world,farms.wheat_field).harvested;
    check(progress<80 && !harvested,"First Wheat portion bypassed the 80 productive-tick adapter");
    f::buy(world,sim::set_building_operation(farms.wheat,false));
    for(int tick=0;tick<90;++tick)world.tick();
    check(world.building(farms.wheat).progress==progress && !a::field(world,farms.wheat_field).harvested,
          "Paused Farm advanced work or produced Food");
    f::buy(world,sim::set_building_operation(farms.wheat,true));
    until(world,[&]{for(const auto id:scene.homes)if(quality(world,id)==20)return true;return false;},
          "Real Wheat20 never physically reached a House");
    bool wheat_receipt=false;
    for(const auto& row:world.original_agriculture_state()->stocks)if(row.quality_witness) {
        const auto& witness=*row.quality_witness;
        check(witness.accepted[0]>0 && !witness.accepted[1] && witness.previous_quality==0,
              "Wheat-only House arrival changed crop identity or fabricated previous quality");wheat_receipt=true;
    }
    check(wheat_receipt && !a::field(world,farms.millet_field).harvested,"Paused Millet generated a hidden Food origin");
    f::buy(world,sim::set_building_operation(farms.millet,true));
    until(world,[&]{for(const auto& row:world.original_agriculture_state()->stocks)
        if(row.quality_witness && row.quality_witness->accepted[1]>0 && quality(world,row.building_id)>20)return true;return false;},
          "Distinct real Millet80 arrival never improved House FoodQuality");
    until(world,[&]{for(const auto& row:world.original_agriculture_state()->stocks)if(row.consumed[0]+row.consumed[1]>0)return true;return false;},
          "Real Food arrivals never reached actual household consumption");
    f::buy(world,sim::set_building_operation(scene.market,false));
    until(world,[&]{return volume(a::stock(world,farms.wheat).amounts)==8 && volume(a::stock(world,farms.millet).amounts)==8 &&
        !world.building(farms.wheat).progress && !world.building(farms.millet).progress;},"Paused Market did not back up finite Farm stores");
    const std::array before{a::field(world,farms.wheat_field).harvested,a::field(world,farms.millet_field).harvested};
    for(int tick=0;tick<180;++tick)world.tick();
    check(a::field(world,farms.wheat_field).harvested==before[0] && a::field(world,farms.millet_field).harvested==before[1],
          "Full Farm storage generated or discarded excess Food");conserved(world);
    f::buy(world,sim::set_building_operation(scene.market,true));
    until(world,[&]{return a::field(world,farms.wheat_field).harvested>before[0];},"Real Food deliveries did not unblock paid Farm production");

    auto burning=fixture.world();const auto fire_scene=basic_scene(burning);
    for(const auto id:fire_scene.watches)f::buy(burning,sim::set_building_operation(id,false));
    until(burning,[&]{return burning.building_on_fire(fire_scene.farms.wheat);},"Food Farm never reached a genuine natural fire",12000);
    const auto stopped=a::field(burning,fire_scene.farms.wheat_field).harvested;bool observed=false;
    for(int tick=0;tick<20 && burning.building_on_fire(fire_scene.farms.wheat);++tick) {
        burning.tick();if(burning.building_on_fire(fire_scene.farms.wheat)) {
            check(a::field(burning,fire_scene.farms.wheat_field).harvested==stopped,"Burning Farm harvested authored Food");observed=true;
        }
    }
    check(observed,"Natural fire had no actual stopped Farm interval");conserved(burning);
}
void schema41_physical_food_recovery_and_hostile_relabels() {
    Fixture fixture;auto world=fixture.world();const auto scene=basic_scene(world);
    until(world,[&]{
        bool wheat_stock=false;
        for(const auto& stock:world.original_agriculture_state()->stocks)wheat_stock|=stock.amounts[0]>0;
        for(const auto& actor:world.couriers())if(wheat_stock && actor.role==sim::CourierRole::OriginalFoodToHouse &&
            actor.phase==sim::CourierPhase::ToWarehouse && actor.edge_progress>0 && world.original_food_cargo(actor.id)>0)return true;
        return false;},
        "Save control never earned a physically loaded Food courier");
    const auto path=fixture.root.path/"food41.json";const auto snapshot=world.snapshot();
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto parsed=save::read_save(path);
    check(parsed.source_schema_version==41 && parsed.world==snapshot,"Schema41 lost typed real Food cargo");
    auto restored=fixture.restore(parsed);auto continued=world;
    for(int tick=0;tick<240;++tick) {continued.tick();restored.tick();check(continued.snapshot()==restored.snapshot(),"Typed Food save continuation diverged");}
    conserved(restored);
    save::RecoveryStore recovery(fixture.root.path/"food-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=recovery.write_checkpoint(history,fixture.document(continued),fixture.mask());
    check(checkpoint.loadable() && checkpoint.schema==41 && fixture.restore(save::read_save(checkpoint.path)).snapshot()==continued.snapshot(),
          "Recovery lost earned Food crops, cargo or House receipts");
    const auto reject=[&](auto edit) {
        auto bad=fixture.document(world);edit(bad.world);
        refuses([&]{(void)fixture.restore(bad);},"Hostile Food authority entered schema41 World");
        refuses([&]{save::write_save(path,bad,fixture.root.data,fixture.mask());},"Hostile Food authority reached publication");
        check(world.snapshot()==snapshot && f::read_text(path)==bytes,"Rejected Food authority changed live World or save bytes");
    };
    reject([&](auto& state){for(auto& field:state.agriculture->fields)if(field.field_id==scene.farms.wheat_field)field.crop=sim::AgricultureCrop::Millet;});
    reject([&](auto& state){for(auto& field:state.agriculture->fields)if(field.field_id==scene.farms.wheat_field)field.crop=sim::AgricultureCrop::Rice;});
    reject([](auto& state){state.agriculture->fields.front().quantity=1;});
    reject([](auto& state){state.agriculture->fields.front().productive_events=1;});
    reject([](auto& state){state.agriculture->fields.front().last_harvest_cycle=1;});
    reject([](auto& state){for(auto& stock:state.agriculture->stocks)if(stock.amounts[0]>0){--stock.amounts[0];++stock.amounts[1];return;}
        throw std::runtime_error("Typed relabel control has no actual Wheat stock");});
    const auto hostile=fixture.root.path/"relabelled40.json";auto json=Json::parse(bytes);
    json["schema_version"]=40;json["rules"]["version"]=24;json["rules"].erase("plantations");json["world"].erase("plantations");
    f::write_text(hostile,json.dump());
    refuses([&]{(void)save::read_save(hostile);},"Frozen schema40 admitted relabelled earned Rule25 Food");
    check(f::read_text(path)==bytes && world.snapshot()==snapshot,"Relabel refusal changed source World or save");
}
void real_food_enables_all_three_luxury_consumers() {
    Fixture fixture(true);auto world=fixture.world();const auto scene=p::build(world);(void)foods(world,true);
    constexpr std::array goods{orig::Goods::Tea,orig::Goods::Silk,orig::Goods::Lacquerware};
    std::array<bool,3> actual_arrivals{};
    for(int tick=0;tick<20000;++tick) {
        world.tick();
        for(const auto& trip:world.original_goods_state()->reservations)
            if(trip.source==scene.market && trip.phase==sim::OriginalGoodsTransferPhase::Completed && trip.delivered>0 &&
               std::find(scene.homes.begin(),scene.homes.end(),trip.target)!=scene.homes.end())
                for(std::size_t index=0;index<goods.size();++index)if(trip.good==goods[index])actual_arrivals[index]=true;
        bool done=std::all_of(actual_arrivals.begin(),actual_arrivals.end(),[](bool value){return value;});
        for(const auto good:goods)done&=world.original_goods_balance(good).totals.consumed>0;
        if(done)break;
    }
    check(std::all_of(actual_arrivals.begin(),actual_arrivals.end(),[](bool value){return value;}),
          "Food-positive paid city never received all three real plantation luxuries");
    for(const auto good:goods)check(world.original_goods_balance(good).totals.consumed>0,
        "Real Food and plantations failed to reach actual luxury household consumption");
    bool consumed_food=false;
    for(const auto& row:world.original_agriculture_state()->stocks)consumed_food|=row.consumed[0]+row.consumed[1]>0;
    check(consumed_food && fixture.model->house_rules().value(orig::ModelHouse::PlainCottage,orig::HouseField::FoodQuality)==1,
          "Luxury consumption positive control bypassed its explicit positive Food requirement");
    conserved(world);
}
} // namespace

int main() {
    try {
        closed_rule24_and_new_crops();typed_paid_arrivals_pause_and_full_storage();
        schema41_physical_food_recovery_and_hostile_relabels();real_food_enables_all_three_luxury_consumers();
        std::cout<<"Rule25 authored Wheat/Millet: paid typed Food, actual arrivals/consumption, strict41 Recovery and closed Rule24/new crops PASS\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"Plantation Food adapter FAIL: "<<error.what()<<'\n';return 1;
    }
}
