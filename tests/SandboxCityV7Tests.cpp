#include "persistence/SandboxSave.h"

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
    sim::World world(width,height,mask(),sim::RulesProfile::CityV7);
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
    road(world,7,4); road(world,8,4); road(world,8,3);
    check(world.treasury()==206 && world.construction_spent_total()==794,
          "City v7 starter cost must be exactly 794");
    check(world.workforce_supply()==16 && world.workforce_required()==16 &&
          world.workforce_used()==16 && world.building_staffed(sim::BuildingId::Farm),
          "City v7 starter must use exactly 16 workers");
    check(world.city_economy_valid() && world.food_balance_valid() &&
          world.production_balance_valid(),"City v7 starter invariants failed");
    return world;
}

void expand(sim::World& world) {
    put(world,sim::CommandType::PlaceClaySource,0,6);
    road(world,1,6); road(world,2,6);
    put(world,sim::CommandType::PlacePottery,3,6);
    road(world,4,6); road(world,5,6); road(world,6,6); road(world,7,6);
    road(world,7,5); road(world,7,3);
    put(world,sim::CommandType::PlaceHousehold,9,3);
    put(world,sim::CommandType::PlaceHousehold,9,4);
}

struct SaveFixture {
    fs::path root;
    fs::path relative="Cities/Synthetic.map";
    fs::path target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-city-v7-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities");
        fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)<<"independently authored city v7 map";
        target=root/"saves/city-v7.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
    sim::World roundtrip(const sim::World& world) const {
        save::write_save(target,save::make_document(root/"data",relative,mask(),world),
                         root/"data",mask());
        const auto document=save::read_save(target);
        check(document.source_schema_version==7,"City v7 save did not use schema 7");
        auto restored=save::restore_save(document,root/"data",mask());
        check(restored.snapshot()==world.snapshot(),"City v7 save roundtrip changed state");
        return restored;
    }
};

void check_start_and_costs() {
    sim::World empty(width,height,mask(),sim::RulesProfile::CityV7);
    check(empty.treasury()==1000 && empty.food_produced_total()==0 &&
          !empty.building(sim::BuildingId::Farm).placed &&
          !empty.courier(sim::CourierId::Food).enabled && !empty.settlement_goal_reached() &&
          empty.food_balance_valid() && empty.city_economy_valid(),"invalid empty City v7");
    const auto before=empty.treasury();
    put(empty,sim::CommandType::PlaceFarm,1,1);
    check(empty.treasury()==before-sim::Rules::farm_cost &&
          empty.construction_spent_total()==static_cast<std::uint64_t>(sim::Rules::farm_cost),
          "Farm cost differs");
    const auto placed=empty.snapshot();
    const auto duplicate=empty.execute({sim::CommandType::PlaceFarm,{2,1}});
    check(!duplicate.accepted && empty.snapshot()==placed,"second Farm changed state");
    const auto occupied=empty.execute({sim::CommandType::PlaceRoad,{1,1}});
    check(!occupied.accepted && empty.snapshot()==placed,"rejected occupied command cost funds");

    sim::World poor(width,height,mask(),sim::RulesProfile::CityV7);
    put(poor,sim::CommandType::PlaceClaySource,0,2);
    road(poor,1,2); road(poor,2,2);
    put(poor,sim::CommandType::PlacePottery,3,2);
    road(poor,4,2); road(poor,5,2);
    put(poor,sim::CommandType::PlaceWarehouse,6,2);
    road(poor,7,2); road(poor,8,2); road(poor,9,2);
    put(poor,sim::CommandType::PlaceHousehold,10,2);
    road(poor,7,1); road(poor,8,1);
    put(poor,sim::CommandType::PlaceFarm,6,4);
    road(poor,7,4); road(poor,8,4); road(poor,8,3);
    for (int i=0;i<36;++i) {
        put(poor,sim::CommandType::PlaceRoad,0,0);
        put(poor,sim::CommandType::RemoveRoad,0,0);
    }
    poor.tick();
    put(poor,sim::CommandType::PlaceHousehold,9,1);
    while (poor.building(sim::BuildingId::Household).fulfilled_demand==0 && poor.ticks()<1000)
        poor.tick();
    check(poor.treasury()==159 && poor.taxes_collected_total()==25,
          "insufficient-funds fixture did not reach exactly 159 after first tax");
    const auto poor_state=poor.snapshot();
    const auto unaffordable=poor.execute({sim::CommandType::PlaceFarm,{1,1}});
    check(!unaffordable.accepted && std::string(unaffordable.reason)=="Not enough money" &&
          poor.snapshot()==poor_state,
          "unaffordable Farm changed state or funds");

    sim::World old(width,height,mask(),sim::RulesProfile::CityV6);
    const auto old_state=old.snapshot();
    const auto unavailable=old.execute({sim::CommandType::PlaceFarm,{1,1}});
    check(!unavailable.accepted && std::string(unavailable.reason)==
          "Command unavailable in selected rules profile" && old.snapshot()==old_state &&
          old.food_balance_valid(),"City v6 gained Farm or Food state");
}

void check_farm_production_and_delivery() {
    sim::World paused(width,height,mask(),sim::RulesProfile::CityV7);
    put(paused,sim::CommandType::PlaceFarm,0,2);
    for (int i=0;i<1000;++i) paused.tick();
    check(paused.building(sim::BuildingId::Farm).output==0 &&
          paused.building(sim::BuildingId::Farm).progress==0 &&
          paused.courier_dispatch_status(sim::CourierId::Food).status==
              sim::CourierDispatchStatus::Unstaffed,"unstaffed Farm advanced");
    put(paused,sim::CommandType::PlaceHousehold,4,2);
    for (int i=0;i<80;++i) paused.tick();
    check(paused.building(sim::BuildingId::Farm).output==1,"restaffed Farm did not resume");

    sim::World capped(width,height,mask(),sim::RulesProfile::CityV7);
    put(capped,sim::CommandType::PlaceHousehold,5,5);
    put(capped,sim::CommandType::PlaceFarm,0,0);
    for (int i=0;i<2000;++i) capped.tick();
    check(capped.building(sim::BuildingId::Farm).output==sim::Rules::farm_output_capacity &&
          capped.building(sim::BuildingId::Farm).progress==0,"Farm did not stop at capacity");

    auto world=starter();
    bool reserved=false,delivered=false,returning=false;
    for (int i=0;i<1000;++i) {
        world.tick();
        const auto& food=world.courier(sim::CourierId::Food);
        for (unsigned id=sim::first_household_id;id<sim::household_id_end;++id) {
            const auto& home=world.building(static_cast<sim::BuildingId>(id));
            reserved=reserved || home.reserved_food_incoming>0;
            delivered=delivered || home.food_stock>0 || home.food_consumed_total>0;
        }
        returning=returning || (delivered && food.phase==sim::CourierPhase::Returning);
        check(world.food_balance_valid(),"Food conservation failed during delivery");
    }
    check(reserved && delivered && returning,"Food courier did not reserve, deliver and return: "+
        std::to_string(reserved)+","+std::to_string(delivered)+","+std::to_string(returning)+
        " status="+sim::courier_dispatch_status_name(
            world.courier_dispatch_status(sim::CourierId::Food).status));
}

void check_dual_demand() {
    sim::World food_only(width,height,mask(),sim::RulesProfile::CityV7);
    put(food_only,sim::CommandType::PlaceFarm,0,2);
    road(food_only,1,2); road(food_only,2,2);
    put(food_only,sim::CommandType::PlaceHousehold,3,2);
    for (int i=0;i<400;++i) food_only.tick();
    const auto& food_home=food_only.building(sim::BuildingId::Household);
    check(food_home.missed_demand==1 && food_home.food_stock>0 &&
          food_home.food_consumed_total==0,"Food was partially consumed without Pottery");

    sim::World pottery_only(width,height,mask(),sim::RulesProfile::CityV7);
    put(pottery_only,sim::CommandType::PlaceClaySource,0,2);
    road(pottery_only,1,2); road(pottery_only,2,2);
    put(pottery_only,sim::CommandType::PlacePottery,3,2);
    road(pottery_only,4,2); road(pottery_only,5,2);
    put(pottery_only,sim::CommandType::PlaceWarehouse,6,2);
    road(pottery_only,7,2); road(pottery_only,8,2);
    put(pottery_only,sim::CommandType::PlaceHousehold,9,2);
    put(pottery_only,sim::CommandType::PlaceHousehold,12,2);
    while (pottery_only.building(sim::BuildingId::Household).pottery_stock==0 &&
           pottery_only.ticks()<2000) pottery_only.tick();
    const auto stock_before=pottery_only.building(sim::BuildingId::Household).pottery_stock;
    const auto misses_before=pottery_only.building(sim::BuildingId::Household).missed_demand;
    while (pottery_only.building(sim::BuildingId::Household).missed_demand==misses_before &&
           pottery_only.ticks()<3000) pottery_only.tick();
    const auto& pottery_home=pottery_only.building(sim::BuildingId::Household);
    check(stock_before>0 && pottery_home.missed_demand==misses_before+1 &&
          pottery_home.pottery_stock>=stock_before && pottery_home.consumed_total==0,
          "Pottery was partially consumed without Food");

    sim::World neither(width,height,mask(),sim::RulesProfile::CityV7);
    put(neither,sim::CommandType::PlaceHousehold,1,1);
    for (int i=0;i<400;++i) neither.tick();
    check(neither.building(sim::BuildingId::Household).missed_demand==1 &&
          neither.taxes_collected_total()==0,"empty dual demand did not miss cleanly");
}

void check_multiple_house_targeting() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV7);
    put(world,sim::CommandType::PlaceFarm,0,2);
    put(world,sim::CommandType::PlaceHousehold,20,8); // Deliberately unreachable ID 4.
    for (int x=1;x<=8;++x) road(world,x,2);
    put(world,sim::CommandType::PlaceHousehold,9,2);
    road(world,8,1);
    put(world,sim::CommandType::PlaceHousehold,9,1);
    road(world,8,3); road(world,8,4);
    put(world,sim::CommandType::PlaceHousehold,9,4);

    std::vector<unsigned> deliveries;
    std::array<int,4> previous{};
    for (int tick=0;tick<6000;++tick) {
        world.tick();
        for (unsigned id=sim::first_household_id;id<sim::household_id_end;++id) {
            const auto stock=world.building(static_cast<sim::BuildingId>(id)).food_stock;
            if (stock>previous[id-sim::first_household_id] && deliveries.size()<6)
                deliveries.push_back(id);
            previous[id-sim::first_household_id]=stock;
        }
        check(world.food_balance_valid(),"multi-house Food balance failed");
    }
    const std::vector<unsigned> expected{5,6,7,5,6,7};
    check(deliveries==expected,"Food target selection was not deterministic and cyclic");
    check(world.building(sim::BuildingId::Household).food_stock==0,
          "unreachable Household unexpectedly received Food");
    for (unsigned id=5;id<sim::household_id_end;++id)
        check(world.building(static_cast<sim::BuildingId>(id)).food_stock==
                  sim::Rules::household_food_capacity,
              "reachable full Household blocked later Food targets");
    check(world.snapshot().last_dispatched_food_household==static_cast<sim::BuildingId>(7),
          "Food cursor did not retain the last cyclic target");
    SaveFixture saves;
    (void)saves.roundtrip(world);
}

void check_levels_and_gameplay() {
    auto world=starter();
    SaveFixture saves;
    (void)saves.roundtrip(world); // Farm producing checkpoint begins at progress zero.
    std::uint64_t first_food=0,first_dual=0,first_level1=0,expansion_tick=0,goal_tick=0;
    bool outbound_saved=false,reserved_saved=false,delivered_saved=false,dual_saved=false,level_saved=false;
    std::array<bool,7> tax_step_seen{};
    constexpr std::array<std::uint64_t,7> tax_totals{0,25,65,105,145,205,265};
    while (world.ticks()<10'000 && !world.settlement_goal_reached()) {
        world.tick();
        const auto& food=world.courier(sim::CourierId::Food);
        const auto& first=world.building(sim::BuildingId::Household);
        if (first.fulfilled_demand<=6) {
            const auto fulfilled=static_cast<std::size_t>(first.fulfilled_demand);
            const int expected_level=fulfilled>=5 ? 2:fulfilled>=2 ? 1:0;
            check(world.household_level(sim::BuildingId::Household)==expected_level &&
                  world.household_tax_contributed(sim::BuildingId::Household)==tax_totals[fulfilled],
                  "house level or 25/40/60 tax schedule differs");
            tax_step_seen[fulfilled]=true;
        }
        if (!outbound_saved && food.phase==sim::CourierPhase::ToWarehouse) {
            (void)saves.roundtrip(world); outbound_saved=true;
        }
        if (!reserved_saved && first.reserved_food_incoming>0) {
            (void)saves.roundtrip(world); reserved_saved=true;
        }
        if (!first_food && (first.food_stock>0 || first.food_consumed_total>0)) {
            first_food=world.ticks(); (void)saves.roundtrip(world); delivered_saved=true;
        }
        if (!first_dual && first.fulfilled_demand>0) {
            first_dual=world.ticks(); (void)saves.roundtrip(world); dual_saved=true;
        }
        if (!first_level1 && world.household_level(sim::BuildingId::Household)>=1) {
            first_level1=world.ticks(); (void)saves.roundtrip(world); level_saved=true;
        }
        if (!expansion_tick && world.treasury()>=482) {
            check(world.taxes_collected_total()>0,"full expansion was affordable before taxes");
            expand(world); expansion_tick=world.ticks();
            (void)saves.roundtrip(world);
        }
        if (world.settlement_goal_reached()) goal_tick=world.ticks();
        check(world.food_balance_valid() && world.city_economy_valid() &&
              world.production_balance_valid() && world.navigation_valid(),
              "City v7 gameplay invariant failed");
    }
    check(first_food && first_dual && first_level1 && expansion_tick && goal_tick,
          "City v7 gameplay loop did not reach all milestones");
    check(outbound_saved && reserved_saved && delivered_saved && dual_saved && level_saved,
          "City v7 save checkpoints were not exercised");
    for (std::size_t i=0;i<tax_step_seen.size();++i)
        check(tax_step_seen[i],"City v7 tax threshold was not observed: "+std::to_string(i));
    check(world.workforce_supply()==32 && world.workforce_required()==26 &&
          world.workforce_used()==26,"expanded City v7 workforce differs");
    for (unsigned id=sim::first_household_id;id<sim::household_id_end;++id) {
        const auto key=static_cast<sim::BuildingId>(id);
        check(world.household_level(key)==2 && world.building(key).fulfilled_demand>=5,
              "goal house did not reach level 2");
    }
    check(world.household_tax_contributed(sim::BuildingId::Household)>=225,
          "tax schedule did not include 25, 40 and 60 tiers");
    auto resumed=saves.roundtrip(world);
    auto control=sim::World::restore(world.snapshot(),mask());
    for (int i=0;i<20'000;++i) {
        resumed.tick(); control.tick();
        check(resumed.snapshot()==control.snapshot(),"City v7 continuation diverged");
        check(resumed.food_balance_valid() && resumed.city_economy_valid(),
              "City v7 long deterministic balance failed");
    }
    std::cout<<"first_food_delivery_tick="<<first_food
             <<" first_dual_fulfilled_tick="<<first_dual
             <<" first_level1_tick="<<first_level1
             <<" expansion_tick="<<expansion_tick
             <<" goal_tick="<<goal_tick
             <<" final_funds="<<resumed.treasury()
             <<" taxes="<<resumed.taxes_collected_total()<<'\n';
}
}

int main() {
    try {
        check_start_and_costs();
        check_farm_production_and_delivery();
        check_dual_demand();
        check_multiple_house_targeting();
        check_levels_and_gameplay();
        std::cout<<"sandbox City v7 tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"sandbox City v7 test failed: "<<error.what()<<'\n';
        return 1;
    }
}
