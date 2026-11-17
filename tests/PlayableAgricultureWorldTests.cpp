// Authored Rule27 acceptance through paid commands, physical courier movement,
// actual residents/staffing and household sinks. No injected stock or callbacks.
#include "PlayableAgricultureFixture.h"
#include "simulation/original/CropRules.h"

#include <algorithm>
#include <iostream>

namespace {
namespace m=oe_playable_agriculture_fixture;
namespace q=oe_irrigation_fixture;
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using m::check;

template<class Values>bool all_positive(const Values& values) {
    return std::all_of(values.begin(),values.end(),[](auto value){return value>0;});
}
struct SingleScene {
    sim::BuildingId farm{},field{},market{},water{},ancestor{};
    std::vector<sim::BuildingId> homes,watches;
};
SingleScene single(sim::World& world,std::size_t crop) {
    SingleScene result;
    for(int x=19;x<=62;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<56;x+=3)
        result.homes.push_back(m::purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
    result.farm=m::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    result.field=m::crop(world,result.farm,crop,{23,25});
    result.market=m::purchase(world,sim::CommandType::PlaceMarket,{27,25});
    result.water=m::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{31,25});
    result.ancestor=m::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{35,25});
    for(std::size_t index=0;index<sim::Rules::fire_watch_limit;++index)
        result.watches.push_back(m::purchase(world,sim::CommandType::PlaceFireWatch,{39+3*static_cast<int>(index),25}));
    check(!world.total_population(),"Crop construction granted occupants");
    return result;
}
sim::AgricultureTrip trip(const sim::World& world,sim::CourierId id) {
    for(const auto& row:world.original_agriculture_state()->trips)
        if(row.transfer.courier_id==id)return row;
    throw std::runtime_error("Moving Food courier lost its sole typed transaction");
}
void all_five_paid_foods_and_materials_end_to_end() {
    m::Fixture fixture(true);auto world=fixture.world();const auto scene=m::build(world,true);
    check(world.rule_version()==sim::kPlayableAgricultureRuleVersion &&
          world.irrigation_enabled() && world.original_plantations_enabled(),
          "Playable Agriculture did not inherit its explicitly versioned economy");
    check(world.buildings().size()<=82 && world.couriers().size()<=58,
          "Five paid crop chains exceeded the shared actor/body budgets");
    check(world.original_agriculture_state()->farms.size()==5 &&
          world.original_agriculture_state()->fields.size()==5,
          "Five typed crop chains did not retain five paid Farm/Field bindings");
    check(sim::building_footprint(world.profile(),world.rule_version(),sim::Object::OriginalFoodFarm)==sim::BuildingFootprint{3,3} &&
          sim::building_footprint(world.profile(),world.rule_version(),sim::Object::OriginalRiceField)==sim::BuildingFootprint{1,1},
          "Playable Agriculture lost complete Farm/field footprint contracts");
    for(const auto id:scene.farms)for(const auto cell:sim::building_footprint_cells(
        world.profile(),world.rule_version(),world.building(id).kind,world.building(id).cell))
        check(world.building_owner_at(cell)==id,"Farm did not own every cell of its paid complete footprint");
    for(const auto& row:world.original_agriculture_state()->stocks)
        check(!m::volume(row.amounts) && !row.quality_witness,
              "Construction manufactured Food or an arrival witness");
    for(const auto id:scene.fields)check(!a::field(world,id).harvested,"Construction generated Food");
    check(world.irrigation_state()->pumps.empty() &&
          !world.irrigation_field_status(world.building(scene.fields[2]).cell).water_available,
          "Dry Rice positive control received an unearned Pump or irrigation effect");

    std::array<bool,5> staffed{},farm_movement{},market_arrival{},house_movement{},house_arrival{};
    std::array<bool,2> luxury_arrival{};
    constexpr std::array luxuries{orig::Goods::Tea,orig::Goods::Silk};
    bool done=false;
    for(int count=0;count<30000 && !done;++count) {
        world.tick();
        for(std::size_t index=0;index<5;++index) {
            staffed[index]|=world.building_staffed(scene.farms[index]);
            market_arrival[index]|=m::stock(world,scene.market).received[index]>0;
            for(const auto home:scene.homes)house_arrival[index]|=m::stock(world,home).received[index]>0;
        }
        for(const auto& value:world.original_agriculture_state()->trips) {
            const auto& actor=world.courier(value.transfer.courier_id);
            if(actor.phase!=sim::CourierPhase::ToWarehouse || actor.edge_progress<=0 ||
               value.phase!=sim::OriginalGoodsTransferPhase::Loaded)continue;
            for(std::size_t index=0;index<5;++index)if(value.loaded[index]>0) {
                if(actor.role==sim::CourierRole::OriginalFoodToMarket)farm_movement[index]=true;
                if(actor.role==sim::CourierRole::OriginalFoodToHouse)house_movement[index]=true;
            }
        }
        for(const auto& value:world.original_goods_state()->reservations)
            if(value.source==scene.market && value.phase==sim::OriginalGoodsTransferPhase::Completed &&
               value.delivered>0 && std::find(scene.homes.begin(),scene.homes.end(),value.target)!=scene.homes.end())
                for(std::size_t index=0;index<luxuries.size();++index)
                    if(value.good==luxuries[index])luxury_arrival[index]=true;
        done=all_positive(staffed) && all_positive(farm_movement) && all_positive(market_arrival) &&
             all_positive(house_movement) && all_positive(house_arrival) && all_positive(m::consumed(world)) &&
             all_positive(luxury_arrival);
        for(const auto good:luxuries)done&=world.original_goods_balance(good).totals.consumed>0;
        if(count%128==0)m::conserved(world);
    }
    if(!done) {
        const auto used=m::consumed(world);
        std::cerr<<"E2E tick="<<world.ticks()<<" population="<<world.total_population()<<'\n';
        for(std::size_t index=0;index<5;++index)
            std::cerr<<"crop="<<index<<" produced="<<a::field(world,scene.fields[index]).harvested
                     <<" consumed="<<used[index]<<" staffed="<<staffed[index]
                     <<" farm-moving="<<farm_movement[index]<<" market-arrival="<<market_arrival[index]
                     <<" house-moving="<<house_movement[index]<<" house-arrival="<<house_arrival[index]<<'\n';
    }
    check(done,"Paid five-crop city did not physically deliver and consume every Food plus Tea/Silk");
    for(std::size_t index=0;index<5;++index) {
        const auto& value=a::field(world,scene.fields[index]);
        check(static_cast<std::size_t>(value.crop)==index && value.harvested>0 &&
              world.workforce_required(scene.farms[index])==12,
              "Typed crop origin or source workforce changed");
    }
    check(orig::crop_rule(orig::ModelBuilding::SoybeanField).resource_id==1,
          "Soybean199 did not retain its proven BeanCurd1 output");
    check(fixture.model->house_rules().value(orig::ModelHouse::PlainCottage,orig::HouseField::FoodQuality)==1,
          "Luxury control bypassed a positive actual FoodQuality requirement");
    const auto& materials=*scene.materials;
    check(p::plant(world,materials.tea_plant).returned>0 && p::plant(world,materials.mulberry_plant).returned>0 &&
          p::industry(world,materials.tea_source).completed>0 && p::industry(world,materials.mulberry_source).completed>0,
          "Inherited plantation return/production authority was bypassed");
    m::conserved(world);

    auto replay=fixture.restore(world.snapshot());
    for(int tick=0;tick<12000;++tick) {
        world.tick();replay.tick();
        check(world.snapshot()==replay.snapshot(),"Five-crop long-run deterministic restore diverged");
        if(tick%128==0)m::conserved(world);
    }
    m::conserved(world);m::conserved(replay);
    check(all_positive(m::consumed(world)),"Long-run Food ledger lost a real crop sink");
}

void every_crop_pause_full_storage_roads_quality_and_natural_fire() {
    for(std::size_t index=0;index<5;++index) {
        m::Fixture fixture;auto world=fixture.world();const auto scene=single(world,index);
        m::until(world,[&]{
            bool actual_quality=false;
            for(const auto id:scene.homes)actual_quality|=m::quality(world,id)==m::crop_scores[index];
            return actual_quality && m::consumed(world)[index]>0;
        },"Single typed crop did not reach actual House quality and consumption");
        for(const auto& row:world.original_agriculture_state()->stocks)
            for(std::size_t other=0;other<5;++other)if(other!=index)
                check(!row.amounts[other] && !row.received[other] && !row.departed[other] && !row.consumed[other],
                      "Single crop silently converted into another Food type");
        m::until(world,[&]{const auto progress=world.building(scene.farm).progress;return progress>7 && progress<70;},
                 "Crop never earned intermediate productive progress");
        const auto progress=world.building(scene.farm).progress;
        const auto produced=a::field(world,scene.field).harvested;
        f::buy(world,sim::set_building_operation(scene.farm,false));m::ticks(world,90);
        check(world.building(scene.farm).progress==progress && a::field(world,scene.field).harvested==produced,
              "Paused crop advanced production or discarded retained work");
        f::buy(world,sim::set_building_operation(scene.farm,true));
        m::until(world,[&]{return a::field(world,scene.field).harvested>produced;},
                 "Resumed crop did not complete real retained production");

        sim::Command second{m::crop_commands[(index+1)%5],{23,26}};second.building_id=scene.farm;
        m::denied(world,second);
        m::denied(world,sim::demolish_building(scene.farm));
        m::denied(world,sim::demolish_building(scene.field));
        f::buy(world,sim::set_building_operation(scene.market,false));
        m::until(world,[&]{return m::volume(m::stock(world,scene.farm).amounts)==8 && !world.building(scene.farm).progress;},
                 "Stopped Market did not back up finite crop inventory");
        const auto full=a::field(world,scene.field).harvested;m::ticks(world,180);
        check(a::field(world,scene.field).harvested==full && m::volume(m::stock(world,scene.farm).amounts)==8,
              "Full crop storage produced, discarded or duplicated Food");
        f::buy(world,sim::set_building_operation(scene.market,true));

        sim::CourierId courier{};
        m::until(world,[&]{
            for(const auto& actor:world.couriers())
                if(actor.owner==scene.farm && actor.role==sim::CourierRole::OriginalFoodToMarket &&
                   actor.phase==sim::CourierPhase::ToWarehouse && actor.edge_progress>0 &&
                   world.original_food_cargo(actor.id)>0 && actor.path_vertex==0) {
                    courier=actor.id;return true;
                }
            return false;
        },"Crop route control never earned a loaded physical Farm courier");
        const auto moving=world.courier(courier);const auto load=trip(world,courier);
        const auto edge_progress=moving.edge_progress;
        const auto cut=sim::Cell{25,24};
        check(std::find(moving.path.begin(),moving.path.end(),cut)!=moving.path.end(),
              "Route-cut control was not on the actual loaded courier path");
        f::buy(world,{sim::CommandType::RemoveRoad,cut});
        check(world.courier(courier).edge_progress==edge_progress && trip(world,courier).loaded==load.loaded,
              "Road cut rewound a begun edge or changed physical crop cargo");
        m::ticks(world,120);
        check(trip(world,courier).transfer==load.transfer && trip(world,courier).phase==sim::OriginalGoodsTransferPhase::Loaded &&
              trip(world,courier).delivered==sim::AgricultureAmounts{} && world.original_food_cargo(courier)==m::volume(load.loaded),
              "Blocked road teleported or discarded a typed Food delivery");
        auto waiting=fixture.restore(world.snapshot());
        check(waiting.snapshot()==world.snapshot(),"Blocked physical crop cargo was not exactly restorable");
        f::buy(world,{sim::CommandType::PlaceRoad,cut});
        m::until(world,[&]{return trip(world,courier).transfer==load.transfer &&
                                  trip(world,courier).phase==sim::OriginalGoodsTransferPhase::Completed;},
                 "Repaired road failed to deliver the retained actual cargo");
        check(trip(world,courier).delivered==load.loaded,"Repaired route changed crop identity or delivery quantity");

        std::vector<sim::Cell> entrances;
        for(const auto& entrance:world.building_entrances(scene.farm))
            if(std::find(entrances.begin(),entrances.end(),entrance.road_cell)==entrances.end())entrances.push_back(entrance.road_cell);
        for(const auto cell:entrances)f::buy(world,{sim::CommandType::RemoveRoad,cell});
        check(!entrances.empty() && world.building_entrances(scene.farm).empty(),"Crop source road-cut left an entrance");
        const auto disconnected_progress=world.building(scene.farm).progress;
        const auto disconnected_harvest=a::field(world,scene.field).harvested;m::ticks(world,90);
        check(world.building(scene.farm).progress==disconnected_progress &&
              a::field(world,scene.field).harvested==disconnected_harvest,
              "Disconnected crop grew or harvested without required source access");
        for(const auto cell:entrances)f::buy(world,{sim::CommandType::PlaceRoad,cell});

        for(const auto id:scene.watches)f::buy(world,sim::set_building_operation(id,false));
        m::until(world,[&]{return world.building_on_fire(scene.farm);},
                 "Unprotected crop Farm never reached an actual natural fire",12000);
        bool observed=false;
        for(int tick=0;tick<20 && world.building_on_fire(scene.farm);++tick) {
            const auto before=a::field(world,scene.field).harvested;world.tick();
            if(world.building_on_fire(scene.farm)) {
                check(a::field(world,scene.field).harvested==before,"Burning crop Farm harvested Food");observed=true;
            }
        }
        check(observed,"Natural crop fire had no observed stopped interval");m::conserved(world);
    }
}

void every_crop_requires_real_staff_and_recovers_from_competition() {
    for(std::size_t index=0;index<5;++index) {
        m::Fixture fixture(false,12);auto world=fixture.world();
        for(int x=19;x<=34;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
        const auto farm=m::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
        const auto field=m::crop(world,farm,index,{23,25});
        m::ticks(world,200);
        check(!world.total_population() && !world.workers_assigned(farm) &&
              !world.building(farm).progress && !a::field(world,field).harvested,
              "Unstaffed paid crop created Food or productive work");
        (void)m::purchase(world,sim::CommandType::PlaceHousehold,{20,22});
        m::until(world,[&]{return world.workers_assigned(farm)==12 && world.building(farm).progress>4;},
                 "Actual immigration never supplied the paid Farm");
        const auto competitor=m::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{27,25});
        (void)m::crop(world,competitor,index,{30,25});
        f::buy(world,sim::set_building_workforce_priority(competitor,sim::WorkforcePriority::High));
        f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::Low));
        check(world.total_population()==12 && !world.workers_assigned(farm) && world.workers_assigned(competitor)==12,
              "Paid competing employer did not take the actual limited workforce");
        const auto progress=world.building(farm).progress;const auto harvest=a::field(world,field).harvested;
        m::ticks(world,100);
        check(world.building(farm).progress==progress && a::field(world,field).harvested==harvest,
              "Farm that lost staff continued productive work");
        f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High));
        f::buy(world,sim::set_building_workforce_priority(competitor,sim::WorkforcePriority::Low));
        m::until(world,[&]{return a::field(world,field).harvested>harvest;},
                 "Recovered workforce never resumed the crop's preserved productive cycle");
    }
}

void inherited_irrigation_works_without_an_authored_yield_bonus() {
    m::Fixture fixture;auto wet=fixture.world();const auto irrigation=q::build(wet);
    const auto farm=m::purchase(wet,sim::CommandType::PlaceOriginalFoodFarm,{118,105});
    const auto field=m::crop(wet,farm,2,{121,105});
    f::buy(wet,sim::set_building_operation(farm,false));
    std::vector<sim::Cell> chain;
    for(int y=100;y<=109;++y)chain.push_back({114,y});
    for(int x=115;x<=118;++x)chain.push_back({x,109});
    check(wet.execute_irrigation_batch(chain).accepted,"Rule27 rejected its paid inherited ditch infrastructure");
    m::until(wet,[&]{return wet.total_population()>=100 &&
        wet.irrigation_field_status(wet.building(field).cell).water_available &&
        wet.irrigation_pump_status(irrigation.pump).supplying;},
        "Rule27 did not earn actual staffed Pump supply through the inherited ditch network");
    check(!a::field(wet,field).harvested && !wet.building(farm).progress,
          "Irrigation infrastructure produced Food for a paused Farm");
    auto dry=fixture.restore(wet.snapshot());
    f::buy(dry,sim::set_building_operation(irrigation.pump,false));
    check(!dry.irrigation_field_status(dry.building(field).cell).water_available,
          "Paused inherited Pump left stale water availability");
    for(auto* world:{&wet,&dry})f::buy(*world,sim::set_building_operation(farm,true));
    for(int tick=0;tick<240;++tick) {wet.tick();dry.tick();}
    check(wet.building_staffed(farm) && dry.building_staffed(farm) &&
          a::field(wet,field).harvested==3 && a::field(dry,field).harvested==3 &&
          wet.building(farm).progress==dry.building(farm).progress,
          "Wet versus dry Rice changed the documented authored base yield or invented a Pump requirement");
    f::buy(wet,{sim::CommandType::RemoveIrrigationDitch,{114,104}});
    check(!wet.irrigation_field_status(wet.building(field).cell).water_available &&
          wet.object_at({114,104})==sim::Object::Road,
          "Ditch cut did not invalidate real supply or destroyed the separate paid crossing road");
    for(int tick=0;tick<160;++tick) {wet.tick();dry.tick();}
    check(a::field(wet,field).harvested==5 && a::field(dry,field).harvested==5 &&
          m::stock(wet,farm).amounts[2]==5 && m::stock(dry,farm).amounts[2]==5,
          "Interrupted irrigation changed base Rice accounting or duplicated Food");
    m::conserved(wet);m::conserved(dry);
}

void source_fertility_and_historical_generations_stay_closed() {
    m::Fixture fixture;fixture.fertility.at(25U*228U+23U)=99;fixture.rebind();
    auto world=fixture.world();const auto farm=m::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command rice{sim::CommandType::PlaceOriginalRiceField,{23,25}};rice.building_id=farm;
    m::denied(world,rice);
    check(world.original_agriculture_state()->fields.empty(),"Rice fertility99 bypassed its retained construction prerequisite");
    m::Fixture historical;
    for(const auto rule:{24U,25U,26U}) {
        auto old=rule==24 ? sim::World::create_original_agriculture(historical.policy,historical.model,historical.terrain):
                 rule==25 ? sim::World::create_original_plantations(historical.policy,historical.model,historical.terrain):
                            sim::World::create_original_irrigation(historical.policy,historical.model,historical.terrain);
        for(int x=19;x<=62;++x)f::buy(old,{sim::CommandType::PlaceRoad,{x,24}});
        for(int x=20;x<56;x+=3)(void)m::purchase(old,sim::CommandType::PlaceHousehold,{x,22});
        const auto source=m::purchase(old,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
        const auto field=m::crop(old,source,2,{23,25});
        m::ticks(old,1000);
        check(old.rule_version()==rule && old.workers_assigned(source)==12 && !old.building(source).progress &&
              !a::field(old,field).harvested,
              "Historical Agriculture activated a new authored Rice origin");
        for(const auto& row:old.original_agriculture_state()->stocks)
            check(!m::volume(row.amounts) && !row.quality_witness,"Historical generation acquired Food or quality");
    }
}
} // namespace

int main() {
    try {
        all_five_paid_foods_and_materials_end_to_end();
        every_crop_pause_full_storage_roads_quality_and_natural_fire();
        every_crop_requires_real_staff_and_recovers_from_competition();
        inherited_irrigation_works_without_an_authored_yield_bonus();
        source_fertility_and_historical_generations_stay_closed();
        std::cout<<"Playable Agriculture: five paid crops, real staffing/work/harvest, physical deliveries, quality/consumption, finite storage, roads/fire, materials and deterministic long-run PASS\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Playable Agriculture World FAIL: "<<error.what()<<'\n';return 1;
    }
}
