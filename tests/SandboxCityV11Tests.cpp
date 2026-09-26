#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
constexpr int width=32,height=8;

void check(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(width*height,1); }
void put(sim::World& world,sim::CommandType type,int x,int y) {
    const auto result=world.execute({type,{x,y}});
    check(result.accepted,"command rejected at "+std::to_string(x)+","+
        std::to_string(y)+": "+result.reason);
}
void road(sim::World& world,int x,int y) { put(world,sim::CommandType::PlaceRoad,x,y); }

sim::World starter(bool include_market=true) {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV11);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    if (include_market) put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<=14;++x) road(world,x,4);
    int houses=0;
    for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}}) {
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
        if (++houses<4) for (int tick=0;tick<100;++tick) world.tick();
    }
    return world;
}

const sim::BuildingState& kind(const sim::World& world,sim::Object wanted,std::size_t ordinal=0) {
    for (const auto& building:world.buildings()) if (building.kind==wanted) {
        if (ordinal--==0) return building;
    }
    throw std::runtime_error("missing building kind");
}

void check_profile_and_starter() {
    check(std::string(sim::rules_profile_name(sim::RulesProfile::CityV11))=="sandbox-city-v11",
          "City-v11 profile ID differs");
    check(sim::building_footprint(sim::RulesProfile::CityV11,sim::Object::Market)==
              sim::BuildingFootprint{1,1} &&
          sim::building_footprint(sim::RulesProfile::CityV11,sim::Object::Household)==
              sim::BuildingFootprint{2,2},"City-v11 footprints differ");
    auto world=starter();
    check(world.treasury()==100 && world.construction_spent_total()==1200,
          "City-v11 starter is not the measured 1200/100 paid setup");
    check(world.total_population()==24 && world.workforce_required()==22 &&
          world.workforce_used()==22 && world.unemployed_workers()==2,
          "City-v11 starter workforce is not 24/22/22 with two unemployed");
    check(world.buildings().size()==10 && world.couriers().size()==7,
          "City-v11 starter entity counts differ");
    const auto& market=kind(world,sim::Object::Market);
    check(world.workforce_required(market.id)==4 && world.building_staffed(market.id),
          "Market staffing differs");
    std::size_t market_couriers=0;
    for (const auto& courier:world.couriers()) if (courier.owner==market.id) {
        ++market_couriers;
        check(courier.role==sim::CourierRole::MarketPotteryDistribution ||
              courier.role==sim::CourierRole::MarketFoodDistribution,
              "Market created an unexpected courier role");
    }
    check(market_couriers==2,"Market did not create exactly two couriers");
    check(world.production_balance_valid() && world.city_economy_valid(),
          "fresh City-v11 starter invariant failed");
}

void check_no_direct_delivery() {
    auto world=starter(false);
    for (int i=0;i<5000;++i) world.tick();
    check(kind(world,sim::Object::Warehouse).pottery_stock>0 &&
          kind(world,sim::Object::Farm).output>0,
          "no-Market suppliers did not accumulate goods");
    for (const auto& building:world.buildings()) if (building.kind==sim::Object::Household)
        check(building.pottery_stock==0 && building.food_stock==0 &&
              building.fulfilled_demand==0 && building.missed_demand>0,
              "City-v11 retained a hidden direct Household delivery path");
}

struct Metrics {
    std::uint64_t pottery_market=0,food_market=0,pottery_house=0,food_house=0;
    std::uint64_t fulfilled=0,tax=0,population=0;
};

Metrics run_loop(sim::World& world,int limit=30000) {
    Metrics result;
    for (int i=0;i<limit;++i) {
        const int population_before=world.total_population();
        world.tick();
        const auto tick=world.ticks();
        const auto& market=kind(world,sim::Object::Market);
        if (!result.pottery_market && market.pottery_stock>0) result.pottery_market=tick;
        if (!result.food_market && market.food_stock>0) result.food_market=tick;
        for (const auto& building:world.buildings()) if (building.kind==sim::Object::Household) {
            if (!result.pottery_house && (building.pottery_stock>0 || building.consumed_total>0))
                result.pottery_house=tick;
            if (!result.food_house && (building.food_stock>0 || building.food_consumed_total>0))
                result.food_house=tick;
            if (!result.fulfilled && building.fulfilled_demand>0) result.fulfilled=tick;
        }
        if (!result.tax && world.taxes_collected_total()>0) result.tax=tick;
        if (!result.population && world.total_population()>population_before) result.population=tick;
        if (result.pottery_market && result.food_market && result.pottery_house &&
            result.food_house && result.fulfilled && result.tax && result.population) break;
    }
    return result;
}

void check_one_market_loop() {
    auto world=starter();
    const auto metrics=run_loop(world);
    std::string homes;
    for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household)
        homes+=" [p="+std::to_string(b.population)+" f="+
            std::to_string(b.fulfilled_demand)+" m="+std::to_string(b.missed_demand)+
            " s="+std::to_string(world.household_service_remaining(b.id))+"]";
    check(metrics.pottery_market && metrics.food_market && metrics.pottery_house &&
          metrics.food_house && metrics.fulfilled && metrics.tax && metrics.population,
          "City-v11 starter did not complete its Market supply/tax/population loop: "+
          std::to_string(metrics.pottery_market)+","+std::to_string(metrics.food_market)+","+
          std::to_string(metrics.pottery_house)+","+std::to_string(metrics.food_house)+","+
          std::to_string(metrics.fulfilled)+","+std::to_string(metrics.tax)+","+
          std::to_string(metrics.population)+homes);
    check(world.production_balance_valid() && world.food_balance_valid() &&
          world.service_state_valid() && world.population_valid() && world.city_economy_valid(),
          "City-v11 loop invariant failed");
    while (world.ticks()<30000) world.tick();
    check(world.taxes_collected_total()>0 && world.production_balance_valid(),
          "City-v11 starter stopped producing valid taxed activity over 30k ticks");
    std::cout<<"starter_cost=1200 remaining=100 first_pottery_market="<<metrics.pottery_market
             <<" first_food_market="<<metrics.food_market
             <<" first_pottery_house="<<metrics.pottery_house
             <<" first_food_house="<<metrics.food_house
             <<" first_fulfilled="<<metrics.fulfilled<<" first_tax="<<metrics.tax
             <<" first_population_growth="<<metrics.population
             <<" population_30k="<<world.total_population()
             <<" treasury_30k="<<world.treasury()<<'\n';
}

sim::World separated_starter() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV11);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,6,5);
    put(world,sim::CommandType::PlaceMarket,10,5);
    put(world,sim::CommandType::PlaceServicePost,12,5);
    for (int x=0;x<=20;++x) road(world,x,4);
    for (const auto cell:{sim::Cell{13,2},sim::Cell{13,5},sim::Cell{16,2},sim::Cell{16,5}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    check(world.construction_spent_total()==1212 && world.treasury()==88,
          "separated Market fixture cost differs");
    return world;
}

sim::World experienced_separated_starter() {
    auto initial=separated_starter();
    auto snapshot=initial.snapshot();
    constexpr std::uint64_t prior_demands=5;
    std::size_t homes=0;
    for (auto& building:snapshot.buildings) {
        if (building.kind==sim::Object::Household) {
            building.fulfilled_demand=prior_demands;
            building.consumed_total=prior_demands;
            building.food_consumed_total=prior_demands;
            building.last_demand_status=1;
            building.population=sim::Rules::household_level2_capacity;
            ++homes;
        } else if (building.kind==sim::Object::ClaySource) {
            building.clay_extracted=2*prior_demands*4;
        } else if (building.kind==sim::Object::Pottery) {
            building.recipes_completed=prior_demands*4;
        } else if (building.kind==sim::Object::Farm) {
            building.food_produced=prior_demands*4;
        }
    }
    check(homes==4,"experienced fixture House count differs");
    snapshot.clay_extracted_total=2*prior_demands*homes;
    snapshot.pottery_completed_total=prior_demands*homes;
    snapshot.food_produced_total=prior_demands*homes;
    snapshot.taxes_collected_total=4*205;
    snapshot.treasury+=static_cast<std::int64_t>(snapshot.taxes_collected_total);
    auto world=sim::World::restore(snapshot,mask());
    check(world.production_balance_valid() && world.food_balance_valid() &&
          world.city_economy_valid() && world.population_valid(),
          "experienced Market fixture is inconsistent");
    return world;
}

std::uint64_t household_total(const sim::World& world,bool fulfilled) {
    std::uint64_t total=0;
    for (const auto& building:world.buildings()) if (building.kind==sim::Object::Household)
        total+=fulfilled ? building.fulfilled_demand:building.missed_demand;
    return total;
}

void remove_road_when_free(sim::World& world,sim::Cell cell) {
    for (int attempt=0;attempt<2000;++attempt) {
        const auto result=world.execute({sim::CommandType::RemoveRoad,cell});
        if (result.accepted) return;
        world.tick();
    }
    throw std::runtime_error("road remained protected for 2000 ticks");
}

void check_market_breaks_and_backpressure() {
    // Outbound break: producers can still fill the Market while every House is isolated.
    auto outbound=experienced_separated_starter();
    for (int tick=0;tick<800;++tick) outbound.tick();
    remove_road_when_free(outbound,{12,4});
    const auto misses_before=household_total(outbound,false);
    bool full=false,missed=false;
    int max_pottery=0,max_food=0;
    for (int tick=0;tick<5000 && !(full && missed);++tick) {
        outbound.tick();
        const auto& market=kind(outbound,sim::Object::Market);
        max_pottery=std::max(max_pottery,market.pottery_stock+market.reserved_incoming);
        max_food=std::max(max_food,market.food_stock+market.reserved_food_incoming);
        full=full || (market.pottery_stock+market.reserved_incoming==
                          sim::Rules::market_pottery_capacity &&
                      market.food_stock+market.reserved_food_incoming==
                          sim::Rules::market_food_capacity);
        missed=household_total(outbound,false)>misses_before;
    }
    check(full && missed,"outbound break did not fill the Market and starve Houses: pottery="+
          std::to_string(max_pottery)+" food="+std::to_string(max_food)+
          " missed="+std::to_string(household_total(outbound,false)-misses_before));
    const auto fulfilled_before=household_total(outbound,true);
    road(outbound,12,4);
    for (int tick=0;tick<5000 && household_total(outbound,true)==fulfilled_before;++tick)
        outbound.tick();
    check(household_total(outbound,true)>fulfilled_before,
          "outbound Market distribution did not recover after road repair");
    check(outbound.production_balance_valid() && outbound.food_balance_valid() &&
          outbound.navigation_valid(),"outbound break/recovery violated an invariant");

    // Inbound break: the Market and House buffers drain before demands begin to miss.
    auto inbound=experienced_separated_starter();
    for (int tick=0;tick<800;++tick) inbound.tick();
    remove_road_when_free(inbound,{8,4});
    const auto inbound_misses=household_total(inbound,false);
    bool drained=false;
    for (int tick=0;tick<8000 && !(drained && household_total(inbound,false)>inbound_misses);
         ++tick) {
        inbound.tick();
        const auto& market=kind(inbound,sim::Object::Market);
        drained=market.pottery_stock==0 && market.food_stock==0;
    }
    check(drained && household_total(inbound,false)>inbound_misses,
          "inbound break did not drain the Market before House misses");
    const auto inbound_fulfilled=household_total(inbound,true);
    road(inbound,8,4);
    for (int tick=0;tick<8000 && household_total(inbound,true)==inbound_fulfilled;++tick)
        inbound.tick();
    check(household_total(inbound,true)>inbound_fulfilled,
          "inbound Market supply did not recover after road repair");
}

void check_market_staffing_recovery() {
    auto staffed=experienced_separated_starter();
    auto snapshot=staffed.snapshot();
    for (auto& building:snapshot.buildings)
        if (building.kind==sim::Object::Household) building.population=4;
    auto world=sim::World::restore(snapshot,mask());
    check(world.total_population()==16 &&
          !world.building_staffed(kind(world,sim::Object::Market).id),
          "Market staffing fixture did not begin unstaffed");
    for (int tick=0;tick<1500;++tick) world.tick();
    const auto& stored=kind(world,sim::Object::Market);
    check(stored.pottery_stock>0 && stored.food_stock>0,
          "unstaffed Market did not accept inbound goods");
    for (const auto& building:world.buildings()) if (building.kind==sim::Object::Household)
        check(building.pottery_stock==0 && building.food_stock==0,
              "unstaffed Market started a new outbound delivery");

    for (int x=21;x<=28;++x) road(world,x,4);
    for (int x:{21,24,27}) put(world,sim::CommandType::PlaceHousehold,x,2);
    check(world.total_population()>=22 &&
          world.building_staffed(kind(world,sim::Object::Market).id),
          "added workforce did not restaff the Market");
    bool delivered=false;
    for (int tick=0;tick<5000 && !delivered;++tick) {
        world.tick();
        for (const auto& building:world.buildings())
            delivered=delivered || (building.kind==sim::Object::Household &&
                (building.pottery_stock>0 || building.food_stock>0));
    }
    check(delivered,"restaffed Market did not resume distribution automatically");
}

double market_route_score(int market_x) {
    constexpr int route_width=24,route_height=8;
    sim::World world(route_width,route_height,
        std::vector<std::uint8_t>(route_width*route_height,1),sim::RulesProfile::CityV11);
    put(world,sim::CommandType::PlaceWarehouse,0,1);
    put(world,sim::CommandType::PlaceFarm,2,4);
    put(world,sim::CommandType::PlaceMarket,market_x,4);
    for (const auto cell:{sim::Cell{10,1},sim::Cell{10,4},sim::Cell{13,1},sim::Cell{13,4}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    for (int x=0;x<=18;++x) road(world,x,3);
    const auto market=kind(world,sim::Object::Market).id;
    std::vector<std::pair<sim::BuildingId,sim::BuildingId>> legs{
        {kind(world,sim::Object::Warehouse).id,market},
        {kind(world,sim::Object::Farm).id,market}};
    for (const auto& building:world.buildings()) if (building.kind==sim::Object::Household)
        legs.emplace_back(market,building.id);
    std::size_t total=0;
    for (const auto [source,target]:legs) {
        const auto route=world.find_building_route(source,target);
        check(route.has_value() && !route->empty(),"Market route diagnostic found no route");
        total+=route->size()-1;
    }
    return static_cast<double>(total)/static_cast<double>(legs.size());
}

void check_market_placement_diagnostic() {
    const double far=market_route_score(3);
    const double centered=market_route_score(9);
    check(centered<far,"centered Market did not shorten the constructed average route");
    std::cout<<"market_route_average_far="<<far
             <<" market_route_average_centered="<<centered<<'\n';
}

void check_two_district_expansion_and_recovery() {
    constexpr int district_height=16;
    sim::World world(width,district_height,
        std::vector<std::uint8_t>(width*district_height,1),sim::RulesProfile::CityV11);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<=14;++x) road(world,x,4);
    int staged=0;
    for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}}) {
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
        if (++staged<4) for (int tick=0;tick<100;++tick) world.tick();
    }
    while (world.treasury()<2500 && world.ticks()<30000) world.tick();
    check(world.treasury()>=2500,
          "first district did not finance the normal-command expansion");
    const auto expansion_funds=world.treasury();

    // Extend A to ten Houses, then build an entirely disconnected ten-House B.
    for (int x=15;x<=20;++x) road(world,x,4);
    for (int x:{12,15,18}) {
        put(world,sim::CommandType::PlaceHousehold,x,2);
        put(world,sim::CommandType::PlaceHousehold,x,5);
    }
    put(world,sim::CommandType::PlaceClaySource,0,9);
    put(world,sim::CommandType::PlacePottery,0,12);
    put(world,sim::CommandType::PlaceWarehouse,3,9);
    put(world,sim::CommandType::PlaceFarm,4,12);
    put(world,sim::CommandType::PlaceMarket,3,12);
    put(world,sim::CommandType::PlaceServicePost,5,12);
    for (int x=0;x<=20;++x) road(world,x,11);
    for (int x:{6,9,12,15,18}) {
        put(world,sim::CommandType::PlaceHousehold,x,9);
        put(world,sim::CommandType::PlaceHousehold,x,12);
    }
    check(world.treasury()==expansion_funds-2184 && world.buildings().size()==32 &&
          world.couriers().size()==14,
          "tax-funded two-district expansion cost or entity count differs");

    struct SupplySeen { bool pottery=false,food=false,service=false; };
    std::vector<std::pair<sim::BuildingId,SupplySeen>> supplied;
    for (const auto& building:world.buildings()) if (building.kind==sim::Object::Household)
        supplied.emplace_back(building.id,SupplySeen{});
    for (int tick=0;tick<60000;++tick) {
        world.tick();
        for (auto& [id,seen]:supplied) {
            const auto& home=world.building(id);
            seen.pottery=seen.pottery || home.pottery_stock>0 || home.consumed_total>0;
            seen.food=seen.food || home.food_stock>0 || home.food_consumed_total>0;
            seen.service=seen.service || world.household_service_active(id);
        }
        if (std::all_of(supplied.begin(),supplied.end(),[](const auto& value) {
                return value.second.pottery && value.second.food && value.second.service;
            })) break;
    }
    check(supplied.size()==20 && std::all_of(supplied.begin(),supplied.end(),[](const auto& value) {
              return value.second.pottery && value.second.food && value.second.service;
          }),"not all 20 Houses observed Pottery, Food and Service in two districts");

    // Every selected target must remain inside its owner's disconnected road component.
    for (const auto& courier:world.couriers()) if (courier.last_dispatched_target) {
        const bool owner_a=world.building(courier.owner).cell.y<8;
        const bool target_a=world.building(*courier.last_dispatched_target).cell.y<8;
        check(owner_a==target_a,"goods or Service crossed disconnected districts");
    }
    const auto total_for=[&](bool district_a,bool fulfilled) {
        std::uint64_t total=0;
        for (const auto& building:world.buildings())
            if (building.kind==sim::Object::Household && (building.cell.y<8)==district_a)
                total+=fulfilled ? building.fulfilled_demand:building.missed_demand;
        return total;
    };
    remove_road_when_free(world,{5,4});
    const auto a_missed=total_for(true,false);
    const auto b_fulfilled=total_for(false,true);
    for (int tick=0;tick<8000 &&
         (total_for(true,false)==a_missed || total_for(false,true)==b_fulfilled);++tick)
        world.tick();
    check(total_for(true,false)>a_missed && total_for(false,true)>b_fulfilled,
          "District A break did not leave disconnected District B operating");
    const auto a_fulfilled=total_for(true,true);
    road(world,5,4);
    for (int tick=0;tick<12000 && total_for(true,true)==a_fulfilled;++tick) world.tick();
    check(total_for(true,true)>a_fulfilled,
          "District A did not resume fulfillment after road repair");
    check(world.production_balance_valid() && world.food_balance_valid() &&
          world.service_state_valid() && world.population_valid() &&
          world.city_economy_valid() && world.navigation_valid(),
          "two-district City-v11 invariant failed");
    std::cout<<"two_district_buildings="<<world.buildings().size()
             <<" two_district_couriers="<<world.couriers().size()
             <<" expansion_spent=2184 district_break_recovered=true\n";
}

void check_market_limit_and_fairness() {
    sim::World limit(width,height,mask(),sim::RulesProfile::CityV11);
    for (int x=0;x<4;++x) put(limit,sim::CommandType::PlaceMarket,x,0);
    const auto building_next=limit.next_building_id(),courier_next=limit.next_courier_id();
    const auto rejected=limit.execute({sim::CommandType::PlaceMarket,{4,0}});
    check(!rejected.accepted && limit.next_building_id()==building_next &&
          limit.next_courier_id()==courier_next && limit.buildings().size()==4 &&
          limit.couriers().size()==8,"fifth Market consumed money or stable IDs");

    sim::World world(width,height,mask(),sim::RulesProfile::CityV11);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,3,2);
    put(world,sim::CommandType::PlaceWarehouse,6,2);
    put(world,sim::CommandType::PlaceFarm,9,3);
    for (int x:{11,14,17,20}) put(world,sim::CommandType::PlaceHousehold,x,2);
    put(world,sim::CommandType::PlaceMarket,8,5);
    put(world,sim::CommandType::PlaceMarket,10,5);
    for (int x=0;x<=21;++x) road(world,x,4);
    std::vector<bool> pottery_seen(2),food_seen(2);
    for (int tick=0;tick<2000;++tick) {
        world.tick();
        for (std::size_t i=0;i<2;++i) {
            const auto& market=kind(world,sim::Object::Market,i);
            pottery_seen[i]=pottery_seen[i] || market.pottery_stock>0;
            food_seen[i]=food_seen[i] || market.food_stock>0;
        }
    }
    check(pottery_seen[0] && pottery_seen[1] && food_seen[0] && food_seen[1],
          "two reachable Markets did not both receive cyclic inbound goods");
    check(world.route_cache_entries()<=220,"City-v11 route cache exceeded role-pair bound");
}

void check_maximum_entities() {
    constexpr int large_width=48,large_height=20;
    sim::WorldSnapshot snapshot;
    snapshot.width=large_width; snapshot.height=large_height;
    snapshot.profile=sim::RulesProfile::CityV11; snapshot.rule_version=1;
    snapshot.ticks=4000; snapshot.command_sequence=38; snapshot.road_revision=38;
    snapshot.next_building_id=39; snapshot.next_courier_id=23;
    snapshot.clay_extracted_total=400; snapshot.pottery_completed_total=200;
    snapshot.food_produced_total=200; snapshot.taxes_collected_total=10100;
    snapshot.construction_spent_total=4180; snapshot.treasury=7220;
    std::uint32_t next_building=1,next_courier=1;
    const auto add=[&](sim::Object object,int ordinal)->sim::BuildingId {
        sim::BuildingSnapshot b;
        b.id=static_cast<sim::BuildingId>(next_building++); b.kind=object; b.placed=true;
        const int index=static_cast<int>(b.id)-1;
        b.cell={(index%10)*4,(index/10)*4};
        if (object==sim::Object::ClaySource) b.clay_extracted=100;
        if (object==sim::Object::Pottery) b.recipes_completed=50;
        if (object==sim::Object::Farm) b.food_produced=100;
        if (object==sim::Object::Household) {
            b.fulfilled_demand=10; b.consumed_total=10; b.food_consumed_total=10;
            b.last_demand_status=1; b.population=6;
        }
        snapshot.buildings.push_back(b);
        const auto add_courier=[&](sim::CourierRole role,sim::Good good) {
            sim::CourierSnapshot c;
            c.id=static_cast<sim::CourierId>(next_courier++); c.role=role;
            c.owner=b.id; c.target=b.id; c.good=good; c.enabled=true;
            snapshot.couriers.push_back(c);
        };
        if (object==sim::Object::ClaySource) add_courier(sim::CourierRole::Clay,sim::Good::Clay);
        if (object==sim::Object::Pottery) add_courier(sim::CourierRole::Pottery,sim::Good::Pottery);
        if (object==sim::Object::Warehouse)
            add_courier(sim::CourierRole::MarketPotteryInbound,sim::Good::Pottery);
        if (object==sim::Object::Farm)
            add_courier(sim::CourierRole::MarketFoodInbound,sim::Good::Food);
        if (object==sim::Object::ServicePost)
            add_courier(sim::CourierRole::Service,sim::Good::Goods);
        if (object==sim::Object::Market) {
            add_courier(sim::CourierRole::MarketPotteryDistribution,sim::Good::Pottery);
            add_courier(sim::CourierRole::MarketFoodDistribution,sim::Good::Food);
        }
        (void)ordinal;
        return b.id;
    };
    for (int i=0;i<4;++i) add(sim::Object::ClaySource,i);
    for (int i=0;i<4;++i) add(sim::Object::Pottery,i);
    for (int i=0;i<2;++i) add(sim::Object::Warehouse,i);
    for (int i=0;i<2;++i) add(sim::Object::Farm,i);
    for (int i=0;i<2;++i) add(sim::Object::ServicePost,i);
    for (int i=0;i<4;++i) add(sim::Object::Market,i);
    for (int i=0;i<20;++i) add(sim::Object::Household,i);
    check(snapshot.buildings.size()==38 && snapshot.couriers.size()==22,
          "synthetic maximum did not construct 38 buildings and 22 couriers");
    auto world=sim::World::restore(snapshot,
        std::vector<std::uint8_t>(large_width*large_height,1));
    check(world.buildings().size()==38 && world.couriers().size()==22 &&
          world.total_population()==120 && world.workforce_required()==72 &&
          world.settlement_goal_reached() && world.route_cache_entries()==240 &&
          world.production_balance_valid() && world.food_balance_valid() &&
          world.service_state_valid() && world.city_economy_valid() &&
          world.population_valid() && world.navigation_valid(),
          "maximum 38/22 City-v11 state failed invariants or bounded route cache");
    for (int i=0;i<100;++i) world.tick();
}

struct SaveFixture {
    fs::path root,relative="Cities/Synthetic.map",target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-city-v11-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)<<"independently authored City-v11 map";
        target=root/"saves/city-v11.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
};

void check_save_and_determinism() {
    auto world=starter();
    bool all_active=false;
    for (int i=0;i<30000 && !all_active;++i) {
        world.tick();
        bool pi=false,fi=false,po=false,fo=false;
        for (const auto& c:world.couriers()) {
            const bool active=c.phase!=sim::CourierPhase::IdleAtWorkshop;
            pi=pi || (active && c.role==sim::CourierRole::MarketPotteryInbound);
            fi=fi || (active && c.role==sim::CourierRole::MarketFoodInbound);
            po=po || (active && c.role==sim::CourierRole::MarketPotteryDistribution);
            fo=fo || (active && c.role==sim::CourierRole::MarketFoodDistribution);
        }
        all_active=pi && fi && po && fo;
    }
    check(all_active,"active Market-flow checkpoint was not reached");
    SaveFixture files;
    save::write_save(files.target,save::make_document(files.root/"data",files.relative,mask(),world),
                     files.root/"data",mask());
    nlohmann::json json; { std::ifstream input(files.target); input>>json; }
    check(json.at("schema_version")==11,"City-v11 did not write schema 11");
    const auto document=save::read_save(files.target);
    auto restored=save::restore_save(document,files.root/"data",mask());
    check(restored.snapshot()==world.snapshot(),"active Market flow changed across schema-11 roundtrip");
    for (int i=0;i<20000;++i) { world.tick(); restored.tick(); }
    check(restored.snapshot()==world.snapshot(),"City-v11 diverged over 20k deterministic ticks");

    auto full_market=experienced_separated_starter();
    remove_road_when_free(full_market,{12,4});
    bool capacity_reached=false;
    for (int tick=0;tick<5000 && !capacity_reached;++tick) {
        full_market.tick();
        const auto& market=kind(full_market,sim::Object::Market);
        capacity_reached=market.pottery_stock+market.reserved_incoming==
                             sim::Rules::market_pottery_capacity &&
                         market.food_stock+market.reserved_food_incoming==
                             sim::Rules::market_food_capacity;
    }
    check(capacity_reached,"full-Market persistence checkpoint was not reached");
    save::write_save(files.target,
        save::make_document(files.root/"data",files.relative,mask(),full_market),
        files.root/"data",mask());
    const auto full_document=save::read_save(files.target);
    const auto full_restored=save::restore_save(full_document,files.root/"data",mask());
    check(full_restored.snapshot()==full_market.snapshot(),
          "full Market inventory/reservations changed across schema-11 roundtrip");

    const auto reject=[&](nlohmann::json changed,const char* label) {
        std::ofstream output(files.target,std::ios::binary|std::ios::trunc);
        output<<changed.dump(); output.close();
        bool failed=false;
        try {
            const auto parsed=save::read_save(files.target);
            (void)save::restore_save(parsed,files.root/"data",mask());
        } catch (const std::exception&) { failed=true; }
        check(failed,std::string("schema 11 accepted ")+label);
    };
    auto changed=json;
    for (auto& b:changed["world"]["buildings"]) if (b["kind"]==9) {
        b["pottery_stock"]=17; break;
    }
    reject(changed,"Market stock above capacity");
    changed=json;
    for (auto& c:changed["world"]["couriers"]) if (c["role"]==8) {
        c["owner"]=999; break;
    }
    reject(changed,"missing Market courier owner");
    changed=json;
    for (auto& c:changed["world"]["couriers"]) if (c["role"]==8) {
        c["target"]=1; break;
    }
    reject(changed,"Market outbound target that is not a Household");
    changed=json;
    std::uint32_t household_id=0;
    for (const auto& b:changed["world"]["buildings"])
        if (b["kind"]==6) { household_id=b["id"].get<std::uint32_t>(); break; }
    for (auto& c:changed["world"]["couriers"]) if (c["role"]==6) {
        c["target"]=household_id; break;
    }
    reject(changed,"Market inbound target that is not a Market");
    changed=json;
    changed["world"]["couriers"][1]["id"]=changed["world"]["couriers"][0]["id"];
    reject(changed,"duplicate courier ID");

    // Schema 10 remains its own profile and rejects both the new kind and roles.
    changed=json; changed["schema_version"]=10;
    changed["rules"]["id"]=sim::city_v10_profile_name;
    reject(changed,"Market data relabeled as schema 10");
}
}

int main() {
    try {
        check_profile_and_starter();
        check_no_direct_delivery();
        check_one_market_loop();
        check_market_breaks_and_backpressure();
        check_market_staffing_recovery();
        check_market_placement_diagnostic();
        check_two_district_expansion_and_recovery();
        check_market_limit_and_fairness();
        check_maximum_entities();
        check_save_and_determinism();
        std::cout<<"City-v11 Market distribution tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
