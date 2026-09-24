#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <array>
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
constexpr int width=24,height=10;

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

sim::World starter() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV8);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    road(world,1,2); road(world,2,2);
    put(world,sim::CommandType::PlacePottery,3,2);
    road(world,4,2); road(world,5,2);
    put(world,sim::CommandType::PlaceWarehouse,6,2);
    road(world,7,2); road(world,8,2); road(world,9,2);
    put(world,sim::CommandType::PlaceHousehold,10,2);
    road(world,7,1); road(world,8,1);
    put(world,sim::CommandType::PlaceHousehold,9,1);
    put(world,sim::CommandType::PlaceFarm,6,4);
    road(world,7,4); road(world,8,4); road(world,8,3); road(world,9,3);
    put(world,sim::CommandType::PlaceHousehold,10,3);
    road(world,9,4); road(world,10,4);
    put(world,sim::CommandType::PlaceServicePost,11,4);
    check(world.construction_spent_total()==980 && world.treasury()==20,
          "City v8 starter must cost 980 and leave 20");
    check(world.workforce_supply()==24 && world.workforce_required()==18 &&
          world.workforce_used()==18 && world.building_staffed(sim::BuildingId::ServicePost),
          "City v8 starter must be fully staffed at 18/24");
    check(world.production_balance_valid() && world.food_balance_valid() &&
          world.service_state_valid() && world.navigation_valid() &&
          world.city_economy_valid(),"City v8 starter invariant failed");
    return world;
}

struct SaveFixture {
    fs::path root;
    fs::path relative="Cities/Synthetic.map";
    fs::path target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-city-v8-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)<<"independently authored city v8 map";
        target=root/"saves/city-v8.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
    sim::World roundtrip(const sim::World& world) const {
        save::write_save(target,save::make_document(root/"data",relative,mask(),world),
                         root/"data",mask());
        std::ifstream input(target);
        const auto json=nlohmann::json::parse(input);
        check(json.at("schema_version")==8 &&
              json.at("world").at("buildings").size()==sim::max_buildings &&
              json.at("world").at("couriers").size()==sim::max_couriers &&
              json.at("world").at("buildings").at(3).contains("service_until_tick") &&
              json.at("world").contains("last_dispatched_service_household"),
              "City v8 save structure is not schema 8 with 11/7 entries");
        const auto document=save::read_save(target);
        check(document.source_schema_version==8,"City v8 save did not use schema 8");
        auto restored=save::restore_save(document,root/"data",mask());
        check(restored.snapshot()==world.snapshot(),"City v8 save roundtrip changed state");
        return restored;
    }
};

void check_profile_cost_and_workforce() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV8);
    check(world.treasury()==1000 && !world.building(sim::BuildingId::ServicePost).placed &&
          !world.courier(sim::CourierId::Service).enabled && world.service_state_valid(),
          "empty City v8 contains Service state");
    put(world,sim::CommandType::PlaceServicePost,1,1);
    check(world.treasury()==900 && world.construction_spent_total()==100,
          "Service Post cost differs");
    const auto state=world.snapshot();
    const auto duplicate=world.execute({sim::CommandType::PlaceServicePost,{2,1}});
    check(!duplicate.accepted && world.snapshot()==state,"second Service Post changed state");

    sim::World old(width,height,mask(),sim::RulesProfile::CityV7);
    const auto old_state=old.snapshot();
    const auto unavailable=old.execute({sim::CommandType::PlaceServicePost,{1,1}});
    check(!unavailable.accepted && old.snapshot()==old_state && old.service_state_valid(),
          "City v7 gained Service state");

    sim::World staff(width,height,mask(),sim::RulesProfile::CityV8);
    put(staff,sim::CommandType::PlaceClaySource,0,0);
    put(staff,sim::CommandType::PlacePottery,1,0);
    put(staff,sim::CommandType::PlaceWarehouse,2,0);
    put(staff,sim::CommandType::PlaceHousehold,3,0);
    put(staff,sim::CommandType::PlaceHousehold,4,0);
    put(staff,sim::CommandType::PlaceFarm,5,0);
    put(staff,sim::CommandType::PlaceServicePost,6,0);
    check(staff.workforce_supply()==16 && staff.workforce_used()==16 &&
          !staff.building_staffed(sim::BuildingId::ServicePost) &&
          staff.courier_dispatch_status(sim::CourierId::Service).status==
              sim::CourierDispatchStatus::Unstaffed,
          "Service Post did not lose last-ID staffing at 16 workers");
    put(staff,sim::CommandType::PlaceHousehold,7,0);
    check(staff.workforce_supply()==24 && staff.workforce_used()==18 &&
          staff.building_staffed(sim::BuildingId::ServicePost),
          "third House did not staff Service Post");

    SaveFixture saves;
    sim::World legacy(width,height,mask(),sim::RulesProfile::CityV7);
    save::write_save(saves.target,
        save::make_document(saves.root/"data",saves.relative,mask(),legacy),
        saves.root/"data",mask());
    std::ifstream input(saves.target);
    const auto json=nlohmann::json::parse(input);
    check(json.at("schema_version")==7 &&
          json.at("world").at("buildings").size()==sim::city_v7_max_buildings &&
          json.at("world").at("couriers").size()==sim::city_v7_max_couriers &&
          !json.at("world").contains("last_dispatched_service_household") &&
          !json.at("world").at("buildings").at(3).contains("service_until_tick"),
          "schema 7 length or bytes gained Service fields");
    const auto restored=save::restore_save(save::read_save(saves.target),saves.root/"data",mask());
    check(restored.snapshot()==legacy.snapshot(),"City v7 legacy roundtrip changed state");
}

sim::World demand_case(bool pottery,bool food,bool service) {
    sim::World base(width,height,mask(),sim::RulesProfile::CityV8);
    put(base,sim::CommandType::PlaceClaySource,0,0);
    put(base,sim::CommandType::PlacePottery,1,0);
    put(base,sim::CommandType::PlaceHousehold,2,0);
    put(base,sim::CommandType::PlaceFarm,3,0);
    if (service) put(base,sim::CommandType::PlaceServicePost,4,0);
    auto s=base.snapshot();
    s.ticks=399;
    auto& clay=s.buildings[0];
    auto& pots=s.buildings[1];
    auto& home=s.buildings[3];
    auto& farm=s.buildings[9];
    home.demand_progress=399;
    if (pottery) {
        home.pottery_stock=1;
        clay.clay_extracted=2; s.clay_extracted_total=2;
        pots.recipes_completed=1; s.pottery_completed_total=1;
    }
    if (food) {
        home.food_stock=1; farm.food_produced=1; s.food_produced_total=1;
    }
    if (service) home.service_until_tick=1000;
    return sim::World::restore(s,mask());
}

void check_demand_matrix_and_expiry() {
    for (int bits=0;bits<8;++bits) {
        const bool pottery=(bits&1)!=0,food=(bits&2)!=0,service=(bits&4)!=0;
        auto world=demand_case(pottery,food,service);
        world.tick();
        const auto& home=world.building(sim::BuildingId::Household);
        const bool success=pottery && food && service;
        check(home.fulfilled_demand==static_cast<std::uint64_t>(success) &&
              home.missed_demand==static_cast<std::uint64_t>(!success),
              "City v8 P/F/S demand matrix result differs");
        check(home.pottery_stock==(success ? 0:static_cast<int>(pottery)) &&
              home.food_stock==(success ? 0:static_cast<int>(food)),
              "City v8 miss partially consumed inventory");
        check(world.taxes_collected_total()==(success ? 25U:0U) &&
              world.production_balance_valid() && world.food_balance_valid() &&
              world.service_state_valid() && world.city_economy_valid(),
              "City v8 demand matrix invariant failed");
    }

    sim::World base(width,height,mask(),sim::RulesProfile::CityV8);
    put(base,sim::CommandType::PlaceHousehold,20,8);
    put(base,sim::CommandType::PlaceServicePost,0,0); // Deliberately disconnected.
    auto s=base.snapshot();
    s.ticks=1000; s.buildings[3].demand_progress=200;
    s.buildings[3].missed_demand=2; s.buildings[3].last_demand_status=2;
    s.buildings[3].service_until_tick=1500;
    auto world=sim::World::restore(s,mask());
    SaveFixture saves;
    world=saves.roundtrip(world);
    check(world.household_service_remaining(sim::BuildingId::Household)==500,
          "saved Service coverage remaining differs");
    s.buildings[3].service_until_tick=2200;
    world=sim::World::restore(s,mask());
    for (int i=0;i<1199;++i) world.tick();
    check(world.ticks()==2199 && world.household_service_active(sim::BuildingId::Household),
          "coverage expired before T+1199 boundary");
    world.tick();
    check(world.ticks()==2200 && !world.household_service_active(sim::BuildingId::Household) &&
          world.household_service_remaining(sim::BuildingId::Household)==0,
          "coverage remained active at exact T+1200 expiry tick");
}

sim::World service_network(bool unreachable_first=false) {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV8);
    put(world,sim::CommandType::PlaceServicePost,0,4);
    if (unreachable_first) put(world,sim::CommandType::PlaceHousehold,22,8);
    for (int x=1;x<=9;++x) road(world,x,4);
    if (!unreachable_first) put(world,sim::CommandType::PlaceHousehold,10,4);
    road(world,7,3); road(world,8,3);
    put(world,sim::CommandType::PlaceHousehold,9,3);
    road(world,7,5); road(world,8,5);
    put(world,sim::CommandType::PlaceHousehold,9,5);
    road(world,6,5); road(world,6,6); road(world,7,6);
    put(world,sim::CommandType::PlaceHousehold,8,6);
    return world;
}

void check_target_cycle_and_unreachable() {
    auto world=service_network();
    std::vector<unsigned> visits;
    auto previous=world.courier(sim::CourierId::Service).phase;
    for (int i=0;i<5000 && visits.size()<8;++i) {
        const auto target=world.courier(sim::CourierId::Service).target;
        world.tick();
        const auto phase=world.courier(sim::CourierId::Service).phase;
        if (previous==sim::CourierPhase::ToWarehouse && phase==sim::CourierPhase::Returning)
            visits.push_back(static_cast<unsigned>(target));
        previous=phase;
    }
    const std::vector<unsigned> expected{4,5,6,7,4,5,6,7};
    check(visits==expected,"Service target cycle is not 4,5,6,7");

    auto disconnected=service_network(true);
    std::array<bool,4> covered{};
    for (int i=0;i<5000;++i) {
        disconnected.tick();
        for (unsigned id=4;id<8;++id)
            covered[id-4]=covered[id-4] ||
                disconnected.building(static_cast<sim::BuildingId>(id)).service_until_tick>0;
    }
    check(!covered[0] && covered[1] && covered[2] && covered[3] &&
          disconnected.service_state_valid(),"unreachable House blocked Service cycle: "+
          std::to_string(covered[0])+std::to_string(covered[1])+
          std::to_string(covered[2])+std::to_string(covered[3]));
}

void check_break_repair_and_save() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV8);
    put(world,sim::CommandType::PlaceServicePost,0,2);
    for (int x=1;x<=5;++x) road(world,x,2);
    put(world,sim::CommandType::PlaceHousehold,6,2);
    while (!(world.courier(sim::CourierId::Service).phase==sim::CourierPhase::ToWarehouse &&
             world.courier(sim::CourierId::Service).edge_progress>0)) world.tick();
    SaveFixture saves;
    auto restored=saves.roundtrip(world);
    for (int i=0;i<200;++i) { world.tick(); restored.tick();
        check(world.snapshot()==restored.snapshot(),"outbound Service continuation diverged"); }

    world=sim::World(width,height,mask(),sim::RulesProfile::CityV8);
    put(world,sim::CommandType::PlaceServicePost,0,2);
    for (int x=1;x<=5;++x) road(world,x,2);
    put(world,sim::CommandType::PlaceHousehold,6,2);
    while (!(world.courier(sim::CourierId::Service).phase==sim::CourierPhase::ToWarehouse &&
             world.courier(sim::CourierId::Service).edge_progress>0)) world.tick();
    check(world.execute({sim::CommandType::RemoveRoad,{4,2}}).accepted,
          "future Service road was not removable");
    check(world.courier(sim::CourierId::Service).route_pending,
          "Service route was not invalidated by future-road removal");
    const auto before=world.building(sim::BuildingId::Household).service_until_tick;
    for (int i=0;i<100;++i) world.tick();
    check(world.building(sim::BuildingId::Household).service_until_tick==before &&
          world.courier(sim::CourierId::Service).route_pending,
          "disconnected Service walker arrived or stopped waiting");
    put(world,sim::CommandType::PlaceRoad,4,2);
    while (world.building(sim::BuildingId::Household).service_until_tick==0 &&
           world.ticks()<1000) world.tick();
    check(world.household_service_active(sim::BuildingId::Household),
          "repaired road did not restore Service visit");
    while (world.courier(sim::CourierId::Service).phase!=sim::CourierPhase::Returning)
        world.tick();
    (void)saves.roundtrip(world);
}

void check_service_failure_visibility() {
    auto world=starter();
    const auto home_id=sim::BuildingId::Household;
    while ((world.building(home_id).pottery_stock==0 ||
            world.building(home_id).food_stock==0 ||
            !world.household_service_active(home_id)) && world.ticks()<4000)
        world.tick();
    check(world.ticks()<4000,"failure fixture did not establish all three supplies");
    while (true) {
        const auto& service=world.courier(sim::CourierId::Service);
        bool protected_cell=false;
        if (service.phase!=sim::CourierPhase::IdleAtWorkshop) {
            protected_cell=service.path.at(service.path_vertex)==sim::Cell{10,4} ||
                (service.edge_progress>0 &&
                 service.path.at(service.path_vertex+1)==sim::Cell{10,4});
        }
        if (!protected_cell) break;
        world.tick();
    }
    check(world.execute({sim::CommandType::RemoveRoad,{10,4}}).accepted,
          "Service-post access road could not be cut");
    while ((world.household_service_active(home_id) ||
            world.building(home_id).pottery_stock==0 || world.building(home_id).food_stock==0) &&
           world.ticks()<7000) world.tick();
    check(world.ticks()<7000,"Service coverage did not visibly expire with inventory present");
    const auto misses=world.building(home_id).missed_demand;
    const int pottery=world.building(home_id).pottery_stock;
    const int food=world.building(home_id).food_stock;
    while (world.building(home_id).missed_demand==misses && world.ticks()<8000) world.tick();
    check(world.building(home_id).missed_demand==misses+1 &&
          world.building(home_id).pottery_stock>=pottery &&
          world.building(home_id).food_stock>=food,
          "missing Service did not cause a non-consuming demand miss");
    put(world,sim::CommandType::PlaceRoad,10,4);
    while (!world.household_service_active(home_id) && world.ticks()<10'000) world.tick();
    check(world.household_service_active(home_id),"road repair did not restore Service coverage");
    const auto fulfilled=world.building(home_id).fulfilled_demand;
    while (world.building(home_id).fulfilled_demand==fulfilled && world.ticks()<11'000)
        world.tick();
    check(world.building(home_id).fulfilled_demand==fulfilled+1,
          "demand did not recover after Service road repair");
}

void expand(sim::World& world) {
    put(world,sim::CommandType::PlaceClaySource,0,6);
    road(world,1,6); road(world,2,6);
    put(world,sim::CommandType::PlacePottery,3,6);
    road(world,4,6); road(world,5,6); road(world,6,6); road(world,7,6);
    road(world,7,5); road(world,8,5);
    put(world,sim::CommandType::PlaceHousehold,9,5);
}

void check_playable_loop_and_determinism() {
    auto world=starter();
    std::uint64_t first_service=0,first_food=0,first_fulfilled=0,first_tax=0,first_level1=0;
    std::uint64_t expansion_tick=0,goal_tick=0;
    while (world.ticks()<15'000 && !world.settlement_goal_reached()) {
        const auto old_coverage=world.covered_households();
        world.tick();
        const auto& first=world.building(sim::BuildingId::Household);
        if (!first_service && world.covered_households()>old_coverage) first_service=world.ticks();
        if (!first_food && (first.food_stock || first.food_consumed_total)) first_food=world.ticks();
        if (!first_fulfilled && first.fulfilled_demand) first_fulfilled=world.ticks();
        if (!first_tax && world.taxes_collected_total()) first_tax=world.ticks();
        if (!first_level1 && world.household_level(sim::BuildingId::Household)>=1)
            first_level1=world.ticks();
        if (!expansion_tick && world.treasury()>=396) {
            check(world.taxes_collected_total()>=376,
                  "City v8 expansion became affordable without earned taxes");
            expand(world); expansion_tick=world.ticks();
        }
        if (world.settlement_goal_reached()) goal_tick=world.ticks();
        check(world.production_balance_valid() && world.food_balance_valid() &&
              world.service_state_valid() && world.navigation_valid() &&
              world.city_economy_valid(),"City v8 playable loop invariant failed");
    }
    check(first_service && first_food && first_fulfilled && first_tax && first_level1 &&
          expansion_tick && goal_tick,"City v8 playable loop did not reach every milestone");
    for (unsigned id=4;id<8;++id)
        check(world.household_level(static_cast<sim::BuildingId>(id))==2,
              "City v8 goal House is below level 2");

    auto a=sim::World::restore(world.snapshot(),mask());
    auto b=sim::World::restore(world.snapshot(),mask());
    for (int i=0;i<20'000;++i) {
        a.tick(); b.tick();
        check(a.snapshot()==b.snapshot(),"City v8 20k deterministic continuation diverged");
    }
    check(a.service_state_valid() && a.production_balance_valid() && a.food_balance_valid() &&
          a.navigation_valid() && a.city_economy_valid(),"City v8 20k invariant failed");
    std::cout<<"starter_cost=980 starter_remaining=20 target_order=4,5,6,7"
             <<" first_service_visit_tick="<<first_service
             <<" first_food_delivery_tick="<<first_food
             <<" first_fulfilled_tick="<<first_fulfilled
             <<" first_tax_tick="<<first_tax
             <<" first_level1_tick="<<first_level1
             <<" expansion_tick="<<expansion_tick<<" goal_tick="<<goal_tick
             <<" final_funds="<<a.treasury()<<'\n';
}
}

int main() {
    try {
        check_profile_cost_and_workforce();
        check_demand_matrix_and_expiry();
        check_target_cycle_and_unreachable();
        check_break_repair_and_save();
        check_service_failure_visibility();
        check_playable_loop_and_determinism();
        std::cout<<"sandbox City v8 tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"sandbox City v8 test failed: "<<error.what()<<'\n';
        return 1;
    }
}
