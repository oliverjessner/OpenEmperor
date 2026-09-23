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
constexpr int width=20,height=8;

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
    sim::World world(width,height,mask(),sim::RulesProfile::CityV6);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    road(world,1,2); road(world,2,2);
    put(world,sim::CommandType::PlacePottery,3,2);
    road(world,4,2); road(world,5,2);
    put(world,sim::CommandType::PlaceWarehouse,6,2);
    road(world,7,2); road(world,8,2); road(world,9,2);
    put(world,sim::CommandType::PlaceHousehold,10,2);
    road(world,7,1); road(world,8,1);
    put(world,sim::CommandType::PlaceHousehold,9,1);
    check(world.treasury()==372 && world.construction_spent_total()==628,
          "starter cost differs from authored City rules");
    check(world.workforce_supply()==16 && world.workforce_required()==12 &&
          world.workforce_used()==12 && world.city_economy_valid(),
          "starter workforce or economy invalid");
    return world;
}

void expand(sim::World& world) {
    put(world,sim::CommandType::PlaceClaySource,0,4);
    road(world,1,4); road(world,2,4); road(world,2,3);
    put(world,sim::CommandType::PlacePottery,3,4);
    road(world,4,4); road(world,5,4); road(world,6,4); road(world,6,3);
    road(world,7,3); road(world,8,3);
    put(world,sim::CommandType::PlaceHousehold,9,3);
    road(world,7,4); road(world,8,4);
    put(world,sim::CommandType::PlaceHousehold,9,4);
}

struct SaveFixture {
    fs::path root;
    fs::path relative="Cities/Synthetic.map";
    fs::path target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-city-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities");
        fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)<<"independently authored city map";
        target=root/"saves/city.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
    sim::World roundtrip(const sim::World& world) const {
        save::write_save(target,save::make_document(root/"data",relative,mask(),world),
                         root/"data",mask());
        const auto document=save::read_save(target);
        check(document.source_schema_version==6,"City save did not use schema 6");
        auto restored=save::restore_save(document,root/"data",mask());
        check(restored.snapshot()==world.snapshot(),"City save roundtrip changed state");
        return restored;
    }
};

void check_costs_and_rejections() {
    const std::array cases{
        std::pair{sim::CommandType::PlaceRoad,sim::Rules::road_cost},
        std::pair{sim::CommandType::PlaceClaySource,sim::Rules::clay_source_cost},
        std::pair{sim::CommandType::PlacePottery,sim::Rules::pottery_cost},
        std::pair{sim::CommandType::PlaceWarehouse,sim::Rules::warehouse_cost},
        std::pair{sim::CommandType::PlaceHousehold,sim::Rules::household_cost}};
    for (const auto [type,cost]:cases) {
        sim::World world(width,height,mask(),sim::RulesProfile::CityV6);
        const auto before=world.treasury();
        put(world,type,1,1);
        check(world.treasury()==before-cost &&
              world.construction_spent_total()==static_cast<std::uint64_t>(cost) &&
              world.city_economy_valid(),"construction charged the wrong amount");
    }
    sim::World world(width,height,mask(),sim::RulesProfile::CityV6);
    const auto pristine=world.snapshot();
    const auto invalid=world.execute({sim::CommandType::PlaceRoad,{-1,0}});
    check(!invalid.accepted && world.snapshot()==pristine,
          "rejected City command mutated authoritative state");
    road(world,1,1);
    const auto after_road=world.snapshot();
    const auto duplicate=world.execute({sim::CommandType::PlaceRoad,{1,1}});
    check(duplicate.accepted && !duplicate.changed && world.treasury()==after_road.treasury &&
          world.construction_spent_total()==after_road.construction_spent_total &&
          world.object_at({1,1})==sim::Object::Road,
          "duplicate road was not a free no-op");
    const auto before_remove=world.treasury();
    put(world,sim::CommandType::RemoveRoad,1,1);
    check(world.treasury()==before_remove && world.construction_spent_total()==2 &&
          world.city_economy_valid(),"road removal refunded or charged funds");

    sim::World exact(width,height,mask(),sim::RulesProfile::CityV6);
    for (int i=0;i<499;++i) {
        road(exact,0,0);
        if (i!=498) put(exact,sim::CommandType::RemoveRoad,0,0);
    }
    check(exact.treasury()==2,"normal commands did not reach exact road cost");
    put(exact,sim::CommandType::RemoveRoad,0,0);
    road(exact,0,0);
    check(exact.treasury()==0,"exact funds placement failed");
    put(exact,sim::CommandType::RemoveRoad,0,0);
    const auto empty=exact.snapshot();
    const auto denied=exact.execute({sim::CommandType::PlaceRoad,{0,0}});
    check(!denied.accepted && std::string(denied.reason)=="Not enough money" &&
          exact.snapshot()==empty,"insufficient-funds command was not exact and transactional");

    sim::World one_short(width,height,mask(),sim::RulesProfile::CityV6);
    put(one_short,sim::CommandType::PlaceClaySource,0,2);
    road(one_short,1,2); road(one_short,2,2);
    put(one_short,sim::CommandType::PlacePottery,3,2);
    road(one_short,4,2); road(one_short,5,2);
    put(one_short,sim::CommandType::PlaceWarehouse,6,2);
    road(one_short,7,2); road(one_short,8,2); road(one_short,9,2);
    put(one_short,sim::CommandType::PlaceHousehold,10,2);
    for (int i=0;i<50;++i) one_short.tick();
    road(one_short,7,1); road(one_short,8,1);
    put(one_short,sim::CommandType::PlaceHousehold,9,1);
    while (one_short.taxes_collected_total()==0 && one_short.ticks()<5000) one_short.tick();
    check(one_short.treasury()%2==1,"one-house tax state was not odd");
    while (one_short.treasury()>1) {
        road(one_short,19,7);
        put(one_short,sim::CommandType::RemoveRoad,19,7);
    }
    const auto one_short_state=one_short.snapshot();
    const auto one_short_result=one_short.execute({sim::CommandType::PlaceRoad,{19,7}});
    check(!one_short_result.accepted && std::string(one_short_result.reason)=="Not enough money" &&
          one_short.snapshot()==one_short_state,"cost-minus-one boundary was not transactional");
}

void check_workforce() {
    for (int households=0;households<=4;++households) {
        sim::World world(width,height,mask(),sim::RulesProfile::CityV6);
        put(world,sim::CommandType::PlaceClaySource,0,0);
        put(world,sim::CommandType::PlacePottery,1,0);
        put(world,sim::CommandType::PlaceWarehouse,2,0);
        for (int i=0;i<households;++i)
            put(world,sim::CommandType::PlaceHousehold,4+i,0);
        const int expected_used=households==0 ? 0:households==1 ? 6:12;
        check(world.workforce_supply()==households*8 && world.workforce_required()==12 &&
              world.workforce_used()==expected_used,"derived workforce totals differ");
        check(world.building_staffed(sim::BuildingId::ClaySource)==(households>0),
              "Clay staffing priority differs");
        check(world.building_staffed(sim::BuildingId::Pottery)==(households>=2),
              "Pottery all-or-none staffing differs");
        check(world.building_staffed(sim::BuildingId::Warehouse)==(households>0),
              "warehouse staffing allocation differs");
    }
    sim::World paused(width,height,mask(),sim::RulesProfile::CityV6);
    put(paused,sim::CommandType::PlaceClaySource,0,0);
    for (int i=0;i<1000;++i) paused.tick();
    check(paused.building(sim::BuildingId::ClaySource).output==0 &&
          paused.building(sim::BuildingId::ClaySource).progress==0 &&
          paused.courier_dispatch_status(sim::CourierId::Clay).status==
              sim::CourierDispatchStatus::Unstaffed,
          "unstaffed production or courier did not pause");
    put(paused,sim::CommandType::PlaceHousehold,2,0);
    for (int i=0;i<100;++i) paused.tick();
    check(paused.building(sim::BuildingId::ClaySource).output==1,
          "staffed Clay source did not resume production");
}

void check_staffing_transitions() {
    sim::World recipe(width,height,mask(),sim::RulesProfile::CityV6);
    put(recipe,sim::CommandType::PlacePottery,3,2);
    put(recipe,sim::CommandType::PlacePottery,3,4);
    put(recipe,sim::CommandType::PlaceHousehold,10,2);
    put(recipe,sim::CommandType::PlaceHousehold,9,1);
    put(recipe,sim::CommandType::PlaceClaySource,0,2);
    road(recipe,1,2); road(recipe,2,2); road(recipe,2,3); road(recipe,2,4);
    const auto second_pottery=static_cast<sim::BuildingId>(8);
    while (recipe.building(second_pottery).active_recipe_clay==0 && recipe.ticks()<5000)
        recipe.tick();
    check(recipe.building(second_pottery).active_recipe_clay==2 &&
          recipe.building_staffed(second_pottery),"second Pottery never began a staffed recipe");
    put(recipe,sim::CommandType::PlaceWarehouse,6,2);
    check(!recipe.building_staffed(second_pottery),"lower-ID warehouse did not reallocate workers");
    const auto progress=recipe.building(second_pottery).progress;
    for (int i=0;i<200;++i) recipe.tick();
    check(recipe.building(second_pottery).active_recipe_clay==2 &&
          recipe.building(second_pottery).progress==progress,
          "active recipe advanced while its owner was unstaffed");
    put(recipe,sim::CommandType::PlaceHousehold,15,7);
    check(recipe.building_staffed(second_pottery),"additional household did not restaff Pottery");
    recipe.tick();
    check(recipe.building(second_pottery).progress==progress+1,
          "restaffed active recipe did not resume at the saved progress");

    sim::World courier_world(width,height,mask(),sim::RulesProfile::CityV6);
    put(courier_world,sim::CommandType::PlaceClaySource,0,2);
    road(courier_world,1,2); road(courier_world,2,2);
    put(courier_world,sim::CommandType::PlacePottery,3,2);
    road(courier_world,4,2); road(courier_world,5,2);
    put(courier_world,sim::CommandType::PlaceWarehouse,6,2);
    road(courier_world,2,3); road(courier_world,2,4);
    road(courier_world,6,3); road(courier_world,6,4);
    put(courier_world,sim::CommandType::PlacePottery,3,4);
    road(courier_world,4,4); road(courier_world,5,4);
    put(courier_world,sim::CommandType::PlaceHousehold,10,2);
    put(courier_world,sim::CommandType::PlaceHousehold,10,3);
    put(courier_world,sim::CommandType::PlaceHousehold,15,7); // Deliberately disconnected.
    const auto second_courier=static_cast<sim::CourierId>(4);
    while ((courier_world.courier(second_courier).phase!=sim::CourierPhase::ToWarehouse ||
            courier_world.courier(second_courier).cargo==0) && courier_world.ticks()<10'000)
        courier_world.tick();
    check(courier_world.courier(second_courier).phase==sim::CourierPhase::ToWarehouse,
          "second Pottery courier never became active");
    auto reduced=courier_world.snapshot();
    const auto removed=static_cast<std::size_t>(6-1);
    reduced.buildings[removed]=sim::BuildingSnapshot{};
    reduced.buildings[removed].id=static_cast<sim::BuildingId>(6);
    reduced.next_household_id=6;
    --reduced.road_revision;
    reduced.construction_spent_total-=sim::Rules::household_cost;
    reduced.treasury+=sim::Rules::household_cost;
    auto finishing=sim::World::restore(reduced,mask());
    check(!finishing.building_staffed(static_cast<sim::BuildingId>(8)) &&
          finishing.courier(second_courier).phase==sim::CourierPhase::ToWarehouse,
          "synthetic staffing transition was not established");
    const auto delivered_before=finishing.building(sim::BuildingId::Warehouse).pottery_stock;
    for (int i=0;i<500 && finishing.courier(second_courier).phase!=
         sim::CourierPhase::IdleAtWorkshop;++i) finishing.tick();
    check(finishing.courier(second_courier).phase==sim::CourierPhase::IdleAtWorkshop &&
          finishing.building(sim::BuildingId::Warehouse).pottery_stock>delivered_before &&
          finishing.courier_dispatch_status(second_courier).status==
              sim::CourierDispatchStatus::Unstaffed,
          "already active courier did not finish before staffing blocked redispatch");
}

void check_taxes() {
    sim::World missed(width,height,mask(),sim::RulesProfile::CityV6);
    put(missed,sim::CommandType::PlaceHousehold,1,1);
    const auto funds=missed.treasury();
    for (int i=0;i<sim::Rules::household_demand_ticks;++i) missed.tick();
    const auto& empty_home=missed.building(sim::BuildingId::Household);
    check(empty_home.missed_demand==1 && empty_home.fulfilled_demand==0 &&
          missed.treasury()==funds && missed.taxes_collected_total()==0,
          "missed demand changed City tax state");

    auto supplied=starter();
    bool observed=false;
    for (int i=0;i<5000 && !observed;++i) {
        std::uint64_t fulfilled_before=0;
        for (unsigned id=4;id<8;++id)
            fulfilled_before+=supplied.building(static_cast<sim::BuildingId>(id)).fulfilled_demand;
        const auto treasury_before=supplied.treasury();
        const auto taxes_before=supplied.taxes_collected_total();
        supplied.tick();
        std::uint64_t fulfilled_after=0;
        for (unsigned id=4;id<8;++id)
            fulfilled_after+=supplied.building(static_cast<sim::BuildingId>(id)).fulfilled_demand;
        if (fulfilled_after>fulfilled_before) {
            const auto count=fulfilled_after-fulfilled_before;
            check(supplied.taxes_collected_total()-taxes_before==count*25U &&
                  supplied.treasury()-treasury_before==static_cast<std::int64_t>(count*25U),
                  "fulfilled demand did not add exactly 25 funds and taxes");
            observed=true;
        }
    }
    check(observed && supplied.city_economy_valid(),"no fulfilled tax event was observed");
}

void check_playable_loop() {
    SaveFixture files;
    auto world=starter();
    world=files.roundtrip(world); // Initial paid buildings and roads.
    std::uint64_t first_tax_tick=0,expansion_tick=0,goal_tick=0;
    bool recipe_checkpoint=false;
    while (world.taxes_collected_total()==0 && world.ticks()<10'000) {
        world.tick();
        if (!recipe_checkpoint && world.building(sim::BuildingId::Pottery).active_recipe_clay>0) {
            world=files.roundtrip(world);
            recipe_checkpoint=true;
        }
    }
    first_tax_tick=world.ticks();
    check(recipe_checkpoint && world.taxes_collected_total()>=25 && world.treasury()>=397 &&
          world.city_economy_valid(),"starter never collected its first tax");
    world=files.roundtrip(world);
    while (world.treasury()<482 && world.ticks()<20'000) world.tick();
    check(world.treasury()>=482,"starter did not earn expansion funds");
    expand(world);
    expansion_tick=world.ticks();
    check(world.workforce_supply()==32 && world.workforce_required()==22 &&
          world.workforce_used()==22 && world.treasury()>=0 && world.city_economy_valid(),
          "paid expansion or full workforce invalid");
    world=files.roundtrip(world);
    while (!world.settlement_goal_reached() && world.ticks()<40'000) world.tick();
    goal_tick=world.ticks();
    check(world.settlement_goal_reached() && world.settlement_goal_households_ready()==4 &&
          world.taxes_collected_total()>=12U*25U && world.production_balance_valid() &&
          world.navigation_valid() && world.city_economy_valid(),
          "four-house settlement goal was not reached through simulation");
    world=files.roundtrip(world);

    auto resumed=files.roundtrip(world);
    for (int i=0;i<20'000;++i) {
        world.tick(); resumed.tick();
        check(world.snapshot()==resumed.snapshot(),"20k deterministic continuation diverged");
        check(world.production_balance_valid() && world.navigation_valid() &&
              world.city_economy_valid(),"20k continuation invariant failed");
    }
    std::ifstream input(files.target);
    nlohmann::json json; input>>json;
    check(json["schema_version"]==6 && json["rules"]["id"]==sim::city_profile_name &&
          json["world"].contains("treasury") &&
          json["world"].contains("taxes_collected_total") &&
          json["world"].contains("construction_spent_total"),
          "schema 6 omitted authoritative City fields");
    std::cout<<"city_metrics first_tax_tick="<<first_tax_tick
             <<" expansion_tick="<<expansion_tick<<" goal_tick="<<goal_tick
             <<" final_funds="<<world.treasury()
             <<" taxes="<<world.taxes_collected_total()<<'\n';
}

void check_v5_regression() {
    sim::World world(width,height,mask(),sim::RulesProfile::IndustryV5);
    put(world,sim::CommandType::PlaceClaySource,0,0);
    put(world,sim::CommandType::PlacePottery,1,0);
    put(world,sim::CommandType::PlaceWarehouse,2,0);
    for (int i=0;i<1000;++i) world.tick();
    check(world.treasury()==0 && world.taxes_collected_total()==0 &&
          world.construction_spent_total()==0 && world.workforce_supply()==0 &&
          world.workforce_required()==0 && world.workforce_used()==0 &&
          world.production_balance_valid() && world.city_economy_valid(),
          "Industry v5 acquired City-v6 state or behavior");
}
}

int main() {
    try {
        check(sim::Rules::starting_treasury==1000,"City starting funds changed");
        sim::World empty(width,height,mask(),sim::RulesProfile::CityV6);
        check(empty.treasury()==1000 && empty.taxes_collected_total()==0 &&
              empty.construction_spent_total()==0 && empty.city_economy_valid(),
              "empty City economy invalid");
        check_costs_and_rejections();
        check_workforce();
        check_staffing_transitions();
        check_taxes();
        check_playable_loop();
        check_v5_regression();
        std::cout<<"Sandbox City v6 tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
