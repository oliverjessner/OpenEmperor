#include "simulation/CityStartGuidance.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace sim=openemperor::simulation;

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

void put(sim::World& world,sim::CommandType type,int x,int y) {
    const auto result=world.execute({type,{x,y}});
    require(result.accepted,"command rejected at "+std::to_string(x)+","+
        std::to_string(y)+": "+result.reason);
}

sim::World make_world() {
    constexpr int width=48,height=24;
    return {width,height,std::vector<std::uint8_t>(width*height,1),
            sim::RulesProfile::CityV11,2};
}

void put_house(sim::World& world,int ordinal) {
    const int x=(ordinal%10)*3;
    const int y=12+(ordinal/10)*3;
    put(world,sim::CommandType::PlaceHousehold,x,y);
}

void screenshot_budget_blockade() {
    auto world=make_world();
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,3,2);
    put(world,sim::CommandType::PlaceWarehouse,6,2);
    for (int i=0;i<9;++i) put_house(world,i);
    for (int x=0;x<48;++x) put(world,sim::CommandType::PlaceRoad,x,8);
    for (int x=0;x<17;++x) put(world,sim::CommandType::PlaceRoad,x,9);
    require(world.treasury()==0 && world.workforce_supply()==54 &&
            world.workforce_required()==12,"screenshot-like treasury/workforce setup differs");
    const auto before=world.snapshot();
    const auto status=sim::inspect_city_start(world);
    require(status.missing_supply_buildings==std::vector<sim::Object>{
                sim::Object::Farm,sim::Object::Market,sim::Object::ServicePost} &&
            status.minimum_missing_building_funds==400 &&
            status.workforce_required_now==12 &&
            status.workforce_required_for_starter==22 &&
            status.starter_workforce_shortfall==0 &&
            status.suggested_additional_houses==0,
        "missing infrastructure was misreported as a worker shortage");
    for (int i=0;i<1600;++i) world.tick();
    require(world.taxes_collected_total()==0,
        "incomplete Food/Market/Service chain produced tax");
    auto restored=sim::World::restore(before,std::vector<std::uint8_t>(48*24,1));
    const auto restored_before=restored.snapshot();
    require(sim::inspect_city_start(restored).minimum_missing_building_funds==400 &&
            restored.snapshot()==restored_before,
        "read-only start inspection changed the World");
}

void build_one_house_chain(sim::World& world) {
    // The reviewed order deliberately makes stable staffing order observable.
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlaceHousehold,6,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<=14;++x) put(world,sim::CommandType::PlaceRoad,x,4);
}

void workforce_recovery_and_real_tax() {
    auto world=make_world();
    build_one_house_chain(world);
    const auto status=sim::inspect_city_start(world);
    require(status.complete_supply_chain && status.workforce_supply==6 &&
            status.workforce_required_for_starter==22 &&
            status.starter_workforce_shortfall==16 &&
            status.suggested_additional_houses==3 && status.suggested_house_cost==240,
        "fresh one-house worker recommendation differs");
    const auto clay=world.buildings()[0].id;
    require(world.building_staffed(clay),"stable ID staffing did not staff Clay Source");
    bool pottery_unstaffed=false,market_unstaffed=false,warehouse_staffed=false,
         farm_unstaffed=false,service_unstaffed=false;
    for (const auto& building:world.buildings()) {
        if (building.kind==sim::Object::Pottery) pottery_unstaffed=!world.building_staffed(building.id);
        if (building.kind==sim::Object::Market) market_unstaffed=!world.building_staffed(building.id);
        if (building.kind==sim::Object::Warehouse) warehouse_staffed=world.building_staffed(building.id);
        if (building.kind==sim::Object::Farm) farm_unstaffed=!world.building_staffed(building.id);
        if (building.kind==sim::Object::ServicePost) service_unstaffed=!world.building_staffed(building.id);
    }
    require(pottery_unstaffed && market_unstaffed && warehouse_staffed && farm_unstaffed &&
            service_unstaffed,"one-house stable ID staffing allocation differs");
    for (const auto& courier:world.couriers()) if (courier.enabled && courier.owner!=clay)
        require(world.courier_dispatch_status(courier.id).status==
                    sim::CourierDispatchStatus::Unstaffed ||
                world.building(courier.owner).kind==sim::Object::Warehouse,
            "unstaffed owner did not report unstaffed dispatch");

    for (const auto cell:{sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    require(world.ticks()==0 && world.workforce_supply()==24 &&
            world.workforce_required()==22,
        "three paused house commands did not recover workforce immediately");
    for (const auto& building:world.buildings())
        if (building.kind!=sim::Object::Household)
            require(world.building_staffed(building.id),"starter facility remained unstaffed");

    for (int x=0;x<=14;++x)
        put(world,sim::CommandType::RemoveRoad,x,4);
    for (int i=0;i<200;++i) world.tick();
    const auto disconnected=sim::inspect_city_start(world);
    require(std::ranges::any_of(disconnected.facilities,[](const auto& facility) {
                return facility.condition==sim::StarterSupplyCondition::NoReachableTarget;
            }) && std::ranges::any_of(disconnected.facilities,[](const auto& facility) {
                return facility.condition==sim::StarterSupplyCondition::AwaitingGoods;
            }),"routing and not-yet-produced supply states were not distinguished");
    for (int x=0;x<=14;++x) put(world,sim::CommandType::PlaceRoad,x,4);

    bool market_pottery_seen=false,market_food_seen=false,house_pottery_seen=false,
         house_food_seen=false,service_seen=false;
    const auto deadline=world.ticks()+8'000;
    while (world.taxes_collected_total()==0 && world.ticks()<deadline) {
        world.tick();
        for (const auto& building:world.buildings()) {
            if (building.kind==sim::Object::Market) {
                market_pottery_seen=market_pottery_seen || building.pottery_stock>0;
                market_food_seen=market_food_seen || building.food_stock>0;
            }
            if (building.kind==sim::Object::Household) {
                house_pottery_seen=house_pottery_seen || building.pottery_stock>0;
                house_food_seen=house_food_seen || building.food_stock>0;
                service_seen=service_seen || world.household_service_active(building.id);
            }
        }
    }
    require(world.taxes_collected_total()>0,"complete recovered chain earned no real tax");
    require(world.clay_extracted_total()>0 && world.pottery_completed_total()>0 &&
            world.food_produced_total()>0 && world.covered_households()>0,
        "recovery did not run Clay, Pottery, Food and Service");
    require(market_pottery_seen && market_food_seen && house_pottery_seen &&
            house_food_seen && service_seen,
        "Warehouse/Farm -> Market -> House or Service arrival was not observed");
    std::cout<<"Recovered four-house starter paid its first tax at tick "
             <<world.ticks()<<'\n';
}

void shrunken_house_recommendation() {
    auto world=make_world();
    put(world,sim::CommandType::PlaceHousehold,6,2);
    for (int i=0;i<2600;++i) world.tick();
    require(world.total_population()==sim::Rules::household_min_population,
        "isolated first House did not shrink through ordinary unmet demands");
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<48;++x) put(world,sim::CommandType::PlaceRoad,x,20);
    for (int x=0;x<12;++x) put(world,sim::CommandType::PlaceRoad,x,21);
    const auto status=sim::inspect_city_start(world);
    require(status.starter_workforce_shortfall==20 &&
            status.suggested_additional_houses==4 &&
            status.affordable_suggested_houses==3 &&
            status.suggested_house_cost==320 && status.suggested_house_funds_missing==70,
        "recommendation was fixed at three Houses instead of using actual population");
}

void budget_confirmation_contract() {
    auto world=make_world();
    for (int i=0;i<5;++i) put_house(world,i);
    const sim::Command sixth{sim::CommandType::PlaceHousehold,{15,12}};
    const auto warning=sim::starter_budget_warning(world,sixth);
    require(warning && warning->purchase_cost==sim::Rules::household_cost &&
            warning->funds_after_purchase==820 &&
            warning->minimum_remaining_building_funds==850,
        "budget warning did not preserve the starter-building reserve");
    const auto cancelled=world.snapshot();
    require(world.snapshot()==cancelled,"cancel path changed World before a command");
    const auto built=world.execute(sixth);
    require(built.accepted && built.changed && world.command_sequence()==
            cancelled.command_sequence+1 && world.treasury()==820 &&
            world.next_building_id()==cancelled.next_building_id+1,
        "Build anyway did not run exactly one normally validated command");

    auto complete=make_world();
    build_one_house_chain(complete);
    require(!sim::starter_budget_warning(complete,
                {sim::CommandType::PlaceRoad,{20,4}}),
        "complete adequately funded supply emitted an irrelevant reserve warning");
}

} // namespace

int main() {
    try {
        screenshot_budget_blockade();
        workforce_recovery_and_real_tax();
        shrunken_house_recommendation();
        budget_confirmation_contract();
        std::cout<<"City-v11 start diagnosis, recovery, tax and budget warning passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
