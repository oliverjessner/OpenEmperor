#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <chrono>
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
constexpr int width=24,height=17;

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

void check_footprints_and_entrances() {
    check(sim::building_footprint(sim::RulesProfile::CityV10,sim::Object::Pottery)==
              sim::BuildingFootprint{2,2} &&
          sim::building_footprint(sim::RulesProfile::CityV10,sim::Object::Farm)==
              sim::BuildingFootprint{1,1} &&
          sim::building_footprint(sim::RulesProfile::CityV9,sim::Object::Pottery)==
              sim::BuildingFootprint{1,1},"building footprint profile rules differ");
    check(sim::building_front_cell(sim::RulesProfile::CityV10,sim::Object::Pottery,{2,3})==
              sim::Cell{3,4},"2x2 front-cell convention differs");

    sim::World occupied(12,8,std::vector<std::uint8_t>(96,1),sim::RulesProfile::CityV10);
    put(occupied,sim::CommandType::PlacePottery,2,2);
    const auto pottery=occupied.buildings().front().id;
    for (const auto cell:sim::building_footprint_cells(sim::RulesProfile::CityV10,
                                                       sim::Object::Pottery,{2,2}))
        check(occupied.object_at(cell)==sim::Object::Pottery &&
              occupied.building_owner_at(cell)==pottery,
              "footprint cell does not resolve to the single Pottery entity");
    const auto before=occupied.snapshot();
    auto rejected=occupied.execute({sim::CommandType::PlaceHousehold,{3,3}});
    check(!rejected.accepted && occupied.snapshot()==before,
          "one-cell footprint overlap mutated treasury or IDs");
    rejected=occupied.execute({sim::CommandType::PlaceRoad,{3,2}});
    check(!rejected.accepted,"road was accepted inside a building footprint");
    road(occupied,5,3);
    rejected=occupied.execute({sim::CommandType::PlaceHousehold,{4,2}});
    check(!rejected.accepted,"building was accepted over an existing road");

    sim::World perimeter(8,8,std::vector<std::uint8_t>(64,1),sim::RulesProfile::CityV10);
    put(perimeter,sim::CommandType::PlacePottery,3,3);
    for (const auto cell:{sim::Cell{3,2},sim::Cell{4,2},sim::Cell{2,3},sim::Cell{5,3},
                          sim::Cell{2,4},sim::Cell{5,4},sim::Cell{3,5},sim::Cell{4,5}})
        road(perimeter,cell.x,cell.y);
    check(perimeter.building_entrances(perimeter.buildings().front().id).size()==8,
          "2x2 building did not expose all eight orthogonal road entrances");

    sim::World routes(10,7,std::vector<std::uint8_t>(70,1),sim::RulesProfile::CityV10);
    put(routes,sim::CommandType::PlaceClaySource,1,2);
    put(routes,sim::CommandType::PlacePottery,6,2);
    for (int x=1;x<=7;++x) { road(routes,x,1); road(routes,x,4); }
    const auto source=routes.buildings()[0].id,target=routes.buildings()[1].id;
    const auto first=routes.find_building_route(source,target);
    check(first && first->at(1)==sim::Cell{2,1} && first->at(first->size()-2)==sim::Cell{6,1},
          "equal routes did not choose source then target entrance in storage order");
    check(routes.execute({sim::CommandType::RemoveRoad,{4,1}}).accepted,
          "alternate-entrance test could not remove the first route");
    const auto alternate=routes.find_building_route(source,target);
    check(alternate && alternate->at(1)==sim::Cell{2,4} &&
          alternate->at(alternate->size()-2)==sim::Cell{6,4},
          "remaining connected entrance was not selected after road removal");

    sim::World moving(12,10,std::vector<std::uint8_t>(120,1),sim::RulesProfile::CityV10);
    put(moving,sim::CommandType::PlaceClaySource,1,2);
    put(moving,sim::CommandType::PlacePottery,6,2);
    put(moving,sim::CommandType::PlaceHousehold,1,6);
    put(moving,sim::CommandType::PlaceHousehold,4,6);
    for (int x=1;x<=7;++x) { road(moving,x,1); road(moving,x,4); }
    road(moving,3,2); road(moving,3,3);
    for (int x=1;x<=5;++x) road(moving,x,5);
    for (int i=0;i<100;++i) moving.tick();
    const auto clay=moving.couriers().front().id;
    check(moving.courier(clay).phase==sim::CourierPhase::ToWarehouse,
          "active alternate-entrance fixture did not dispatch");
    check(moving.execute({sim::CommandType::RemoveRoad,{4,1}}).accepted &&
          moving.courier(clay).route_pending,
          "future road removal did not put the active route into pending state");
    for (int i=0;i<200 && moving.buildings()[1].input_clay==0;++i) moving.tick();
    check(moving.buildings()[1].input_clay>0 && moving.navigation_valid(),
          "active courier did not reroute from its reached waypoint to another entrance");
}

sim::World starter() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV10);
    put(world,sim::CommandType::PlaceClaySource,12,3);
    put(world,sim::CommandType::PlacePottery,9,3);
    put(world,sim::CommandType::PlaceWarehouse,9,6);
    put(world,sim::CommandType::PlaceFarm,11,4);
    put(world,sim::CommandType::PlaceServicePost,14,4);
    put(world,sim::CommandType::PlaceHousehold,6,3);
    put(world,sim::CommandType::PlaceHousehold,15,3);
    put(world,sim::CommandType::PlaceHousehold,18,3);
    for (int x=4;x<=18;++x) road(world,x,5);
    check(world.treasury()==20 && world.construction_spent_total()==980,
          "City-v10 starter does not preserve the 980-cost City-v9 opening");
    return world;
}

void wait_for_funds(sim::World& world,std::int64_t amount) {
    for (int i=0;i<60000 && world.treasury()<amount;++i) {
        try { world.tick(); }
        catch (const std::exception& e) {
            throw std::runtime_error("starter tick "+std::to_string(world.ticks())+": "+e.what()+
                " production="+std::to_string(world.production_balance_valid())+
                " navigation="+std::to_string(world.navigation_valid())+
                " economy="+std::to_string(world.city_economy_valid())+
                " population="+std::to_string(world.population_valid()));
        }
    }
    check(world.treasury()>=amount,"starter failed to finance City-v10 expansion");
}

void build_large(sim::World& world) {
    wait_for_funds(world,4000);
    for (int x=0;x<=3;++x) road(world,x,5);
    for (int x=19;x<=22;++x) road(world,x,5);
    for (int x=0;x<=22;++x) road(world,x,13);

    // Complete district A: 10 homes, two Clay sources and two Potteries.
    put(world,sim::CommandType::PlaceClaySource,21,3);
    put(world,sim::CommandType::PlacePottery,21,6);
    for (const auto cell:{sim::Cell{0,3},sim::Cell{3,3},sim::Cell{0,6},
                          sim::Cell{3,6},sim::Cell{6,6},sim::Cell{15,6},sim::Cell{18,6}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);

    // District B has its own warehouse, farm and service post and is disconnected from A.
    put(world,sim::CommandType::PlaceWarehouse,9,14);
    put(world,sim::CommandType::PlaceFarm,11,12);
    put(world,sim::CommandType::PlaceServicePost,14,12);
    put(world,sim::CommandType::PlaceClaySource,12,11);
    put(world,sim::CommandType::PlacePottery,9,11);
    put(world,sim::CommandType::PlaceClaySource,21,11);
    put(world,sim::CommandType::PlacePottery,21,14);
    for (const auto cell:{sim::Cell{0,11},sim::Cell{3,11},sim::Cell{6,11},
                          sim::Cell{15,11},sim::Cell{18,11},sim::Cell{0,14},
                          sim::Cell{3,14},sim::Cell{6,14},sim::Cell{15,14},
                          sim::Cell{18,14}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);

    check(world.buildings().size()==34,"large City-v10 did not create exactly 34 buildings");
    check(world.couriers().size()==14,"large City-v10 did not create exactly 14 couriers");
    check(world.total_population()>=120 && world.workforce_required()==56 &&
          world.workforce_used()==56,"large City-v10 workforce is not 120+/56/56");
    check(world.next_building_id()==35 && world.next_courier_id()==15,
          "City-v10 stable IDs are not monotonic");
    check(world.route_cache_entries()<=144,"City-v10 route cache exceeded its role-pair bound");
    const auto rejected=world.execute({sim::CommandType::PlaceHousehold,{22,0}});
    check(!rejected.accepted,"City-v10 accepted a 21st house");
}

struct SaveFixture {
    fs::path root;
    fs::path relative="Cities/Synthetic.map";
    fs::path target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-city-v10-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)<<"independently authored City-v10 map";
        target=root/"saves/city-v10.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
    sim::World roundtrip(const sim::World& world) const {
        save::write_save(target,save::make_document(root/"data",relative,mask(),world),
                         root/"data",mask());
        std::ifstream input(target); const auto json=nlohmann::json::parse(input);
        check(json.at("schema_version")==10 &&
              json.at("world").at("buildings").size()==34 &&
              json.at("world").at("couriers").size()==14,
              "schema 10 did not write variable large-city collections");
        const auto document=save::read_save(target);
        check(document.source_schema_version==10,"City-v10 save did not use schema 10");
        auto restored=save::restore_save(document,root/"data",mask());
        check(restored.snapshot()==world.snapshot(),"City-v10 save roundtrip changed state");
        const auto rejected=[&](nlohmann::json changed,const std::string& label) {
            std::ofstream output(target,std::ios::binary|std::ios::trunc);
            output<<changed.dump(); output.close();
            bool failed=false;
            try {
                const auto parsed=save::read_save(target);
                (void)save::restore_save(parsed,root/"data",mask());
            } catch (const std::exception&) { failed=true; }
            check(failed,"schema 10 accepted "+label);
        };
        auto invalid=json;
        invalid["world"]["buildings"][1]["id"]=invalid["world"]["buildings"][0]["id"];
        rejected(invalid,"duplicate building ID");
        invalid=json; invalid["world"]["next_building_id"]=34;
        rejected(invalid,"nonmonotonic next building ID");
        invalid=json; invalid["world"]["couriers"][0]["owner"]=999;
        rejected(invalid,"missing courier owner");
        invalid=json;
        invalid["world"]["buildings"][1]["cell"]=invalid["world"]["buildings"][0]["cell"];
        rejected(invalid,"overlapping building footprints");
        return restored;
    }
};

void check_large_city() {
    auto world=starter(); build_large(world);
    bool goal_seen=world.settlement_goal_reached();
    for (int i=0;i<40000;++i) { world.tick(); goal_seen=goal_seen || world.settlement_goal_reached(); }
    std::size_t producing_clay=0,productive_pottery=0,warehouses=0,farms=0,services=0,
        supplied_houses=0,food_houses=0,covered_houses=0;
    for (const auto& b:world.buildings()) {
        if (b.kind==sim::Object::ClaySource && b.clay_extracted>0) ++producing_clay;
        if (b.kind==sim::Object::Pottery && b.recipes_completed>0) ++productive_pottery;
        if (b.kind==sim::Object::Warehouse) ++warehouses;
        if (b.kind==sim::Object::Farm && b.food_produced>0) ++farms;
        if (b.kind==sim::Object::ServicePost) ++services;
        if (b.kind==sim::Object::Household) {
            if (b.consumed_total>0) ++supplied_houses;
            if (b.food_consumed_total>0) ++food_houses;
            if (b.service_until_tick>0) ++covered_houses;
        }
    }
    check(producing_clay==4 && productive_pottery==4 && warehouses==2 && farms==2 && services==2,
          "multi-producer City-v10 logistics did not operate in both districts: clay="+
          std::to_string(producing_clay)+" pottery="+std::to_string(productive_pottery)+
          " warehouses="+std::to_string(warehouses)+" farms="+std::to_string(farms)+
          " services="+std::to_string(services));
    check(supplied_houses==20 && food_houses==20 && covered_houses==20,
          "not every City-v10 household was reached by local supplies and service");
    check(world.production_balance_valid() && world.food_balance_valid() &&
          world.service_state_valid() && world.city_economy_valid() && world.population_valid(),
          "large City-v10 invariant failed");
    check(goal_seen || world.settlement_goal_households_ready()==20,
          "City-v10 households did not all reach the goal supply tier");

    const auto district=[&](bool second) {
        std::pair<std::uint64_t,std::uint64_t> totals{};
        for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household &&
            (second ? b.cell.y>9:b.cell.y<9)) {
            totals.first+=b.fulfilled_demand; totals.second+=b.missed_demand;
        }
        return totals;
    };
    bool removed=false;
    for (int i=0;i<2000 && !removed;++i) {
        const auto result=world.execute({sim::CommandType::RemoveRoad,{8,5}});
        removed=result.accepted;
        if (!removed) world.tick();
    }
    check(removed,"could not isolate district A at an unoccupied road moment");
    const auto a_before=district(false),b_before=district(true);
    for (int i=0;i<2400;++i) world.tick();
    const auto a_broken=district(false),b_broken=district(true);
    check(a_broken.second>a_before.second && b_broken.first>b_before.first,
          "breaking district A did not leave district B independently operating");
    road(world,8,5);
    for (int i=0;i<2400;++i) world.tick();
    check(district(false).first>a_broken.first,"district A did not recover after road repair");

    bool simultaneous=false;
    for (int tick=0;tick<10000 && !simultaneous;++tick) {
        bool clay=false,pottery=false,house=false,food=false,service=false;
        for (const auto& c:world.couriers()) if (c.phase!=sim::CourierPhase::IdleAtWorkshop) {
            clay=clay || c.role==sim::CourierRole::Clay;
            pottery=pottery || c.role==sim::CourierRole::Pottery;
            house=house || c.role==sim::CourierRole::Household;
            food=food || c.role==sim::CourierRole::Food;
            service=service || c.role==sim::CourierRole::Service;
        }
        simultaneous=clay && pottery && house && food && service;
        if (!simultaneous) world.tick();
    }
    check(simultaneous,"did not reach a checkpoint with every courier role active");

    SaveFixture fixture;
    auto restored=fixture.roundtrip(world);
    for (int i=0;i<2000;++i) { world.tick(); restored.tick(); }
    check(world.snapshot()==restored.snapshot(),"schema-10 continuation is not deterministic");
}

void check_long_determinism() {
    auto a=starter(); build_large(a);
    auto b=sim::World::restore(a.snapshot(),mask());
    const auto refreshes=a.route_refresh_count();
    const auto begin=std::chrono::steady_clock::now();
    for (int i=0;i<20000;++i) { a.tick(); b.tick(); }
    check(a.snapshot()==b.snapshot(),"two City-v10 worlds diverged over 20k ticks");
    check(a.route_refresh_count()==refreshes && a.route_cache_entries()<=144,
          "City-v10 rebuilt or grew route caches without a topology change");
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-begin).count();
    std::cout<<"city_v10 buildings="<<a.buildings().size()<<" couriers="<<a.couriers().size()
             <<" population="<<a.total_population()<<" ticks=20000 elapsed_ms="<<elapsed
             <<" route_refreshes="<<a.route_refresh_count()
             <<" route_cache_entries="<<a.route_cache_entries()<<'\n';
}
}

int main() {
    try {
        check(std::string(sim::rules_profile_name(sim::RulesProfile::CityV10))=="sandbox-city-v10",
              "City-v10 profile ID differs");
        check_footprints_and_entrances();
        check_large_city();
        check_long_determinism();
        std::cout<<"Sandbox City-v10 tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"Sandbox City-v10 test failure: "<<e.what()<<'\n';
        return 1;
    }
}
