#include "simulation/World.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
namespace sim=openemperor::simulation;

void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void put(sim::World& world,sim::CommandType type,int x,int y) {
    check(world.execute({type,{x,y}}).accepted,"synthetic placement command failed");
}

void road(sim::World& world,int x,int y) {
    put(world,sim::CommandType::PlaceRoad,x,y);
}

template<class Predicate>
void until(sim::World& world,int limit,Predicate predicate,const char* message) {
    for (int tick=0;tick<limit && !predicate();++tick) world.tick();
    check(predicate(),message);
}

sim::World screenshot_layout(std::optional<sim::Cell> omitted_road=std::nullopt) {
    sim::World world(228,228,std::vector<std::uint8_t>(228U*228U,1),
                     sim::RulesProfile::IndustryV5);
    // Preserve the observed placement order and therefore the stable IDs.
    put(world,sim::CommandType::PlaceClaySource,100,118); // Building 1 / courier 1.
    put(world,sim::CommandType::PlacePottery,97,118);     // Building 2 / courier 2.
    put(world,sim::CommandType::PlaceWarehouse,97,116);   // Building 3 / courier 3.
    put(world,sim::CommandType::PlaceHousehold,100,122);  // Buildings 4..7.
    put(world,sim::CommandType::PlaceHousehold,100,124);
    put(world,sim::CommandType::PlaceHousehold,100,126);
    put(world,sim::CommandType::PlaceHousehold,97,126);
    put(world,sim::CommandType::PlacePottery,97,122);     // Building 8 / courier 4.

    const std::vector<sim::Cell> roads{
        {99,118},{98,118},                  // ClaySource -> first Pottery.
        {97,117},                           // first Pottery -> Warehouse.
        {98,116},{99,116},{99,117},         // Warehouse -> shared trunk.
        {99,119},{99,120},{99,121},{99,122},
        {99,123},{99,124},{99,125},{99,126},
        {98,122},{98,126}                   // second Pottery / fourth House.
    };
    for (const auto cell:roads)
        if (!omitted_road || cell!=*omitted_road) road(world,cell.x,cell.y);
    return world;
}

void report_clay_courier(const sim::World& world,sim::CourierId id) {
    const auto& courier=world.courier(id);
    const auto& owner=world.building(courier.owner);
    const auto decision=world.courier_dispatch_status(id);
    std::cout<<"clay_courier owner="<<static_cast<unsigned>(courier.owner)
             <<" role="<<static_cast<unsigned>(courier.role)
             <<" phase="<<static_cast<unsigned>(courier.phase)
             <<" output="<<owner.output
             <<" cached_revision="<<courier.cached_revision
             <<" road_revision="<<world.road_revision();
    for (unsigned target_id=1;target_id<=9;++target_id) {
        const auto target=static_cast<sim::BuildingId>(target_id);
        const auto& building=world.building(target);
        if (!building.placed || building.kind!=sim::Object::Pottery) continue;
        std::cout<<" candidate="<<target_id
                 <<":route="<<(courier.target_routes[target_id-1] ? "yes":"no")
                 <<",input="<<building.input_clay
                 <<",reserved="<<building.reserved_incoming;
    }
    std::cout<<" status="<<sim::courier_dispatch_status_name(decision.status)
             <<" selected="<<(decision.selected_target ?
                 std::to_string(static_cast<unsigned>(*decision.selected_target)):"none")<<'\n';
}

void connected_screenshot_case() {
    auto world=screenshot_layout();
    const auto source=world.building(sim::BuildingId::ClaySource).cell;
    const auto first=world.building(sim::BuildingId::Pottery).cell;
    check(world.find_route(source,first).has_value(),
          "connected screenshot ClaySource-to-Pottery route missing");
    check(world.courier_dispatch_status(sim::CourierId::Clay).status==
              sim::CourierDispatchStatus::NoStock,
          "empty connected Clay courier status is not NoStock");
    for (int tick=0;tick<1000;++tick) world.tick();
    report_clay_courier(world,sim::CourierId::Clay);
    std::uint64_t household_consumed=0;
    for (unsigned id=4;id<=7;++id)
        household_consumed+=world.building(static_cast<sim::BuildingId>(id)).consumed_total;
    std::cout<<"screenshot_1000_ticks clay_output="
             <<world.building(sim::BuildingId::ClaySource).output
             <<" pottery_2_input="<<world.building(sim::BuildingId::Pottery).input_clay
             <<" pottery_8_input="<<world.building(static_cast<sim::BuildingId>(8)).input_clay
             <<" pottery_completed="<<world.pottery_completed_total()
             <<" warehouse_stock="<<world.building(sim::BuildingId::Warehouse).pottery_stock
             <<" household_consumed="<<household_consumed
             <<" courier_phase="
             <<static_cast<unsigned>(world.courier(sim::CourierId::Clay).phase)
             <<" courier_target="
             <<static_cast<unsigned>(world.courier(sim::CourierId::Clay).target)<<'\n';
    check(world.pottery_completed_total()>0,
          "one-source screenshot layout stalled despite complete roads");
    check(world.production_balance_valid() && world.navigation_valid(),
          "one-source screenshot layout violated an invariant");
}

void output_eight_and_late_road() {
    auto world=screenshot_layout(sim::Cell{99,118});
    const auto source=world.building(sim::BuildingId::ClaySource).cell;
    const auto first=world.building(sim::BuildingId::Pottery).cell;
    check(!world.find_route(source,first),"one-cell road gap unexpectedly routed");
    for (int tick=0;tick<800;++tick) world.tick();
    check(world.building(sim::BuildingId::ClaySource).output==8,
          "disconnected source did not reach eight Clay");
    auto decision=world.courier_dispatch_status(sim::CourierId::Clay);
    check(decision.status==sim::CourierDispatchStatus::NoRoad && !decision.selected_target,
          "one-cell gap status is not NoRoad");
    const auto revision=world.road_revision();
    road(world,99,118);
    check(world.road_revision()==revision+1 && world.find_route(source,first),
          "late road did not refresh the route revision and path");
    decision=world.courier_dispatch_status(sim::CourierId::Clay);
    check(decision.status==sim::CourierDispatchStatus::Ready &&
          decision.selected_target==sim::BuildingId::Pottery,
          "full source was not ready immediately after route repair");
    world.tick();
    const auto& courier=world.courier(sim::CourierId::Clay);
    check(courier.phase==sim::CourierPhase::ToWarehouse && courier.cargo==4 &&
          courier.target==sim::BuildingId::Pottery,
          "full source did not dispatch on the first tick after repair");
}

void reachable_second_pottery() {
    auto world=screenshot_layout(sim::Cell{98,118});
    for (int tick=0;tick<100;++tick) world.tick();
    const auto second=static_cast<sim::BuildingId>(8);
    const auto& courier=world.courier(sim::CourierId::Clay);
    check(!courier.target_routes[static_cast<unsigned>(sim::BuildingId::Pottery)-1] &&
          courier.target_routes[static_cast<unsigned>(second)-1],
          "synthetic first/second Pottery reachability is wrong");
    check(courier.phase==sim::CourierPhase::ToWarehouse && courier.target==second,
          "Clay courier did not choose the reachable second Pottery");
}

void full_first_does_not_block_second() {
    sim::World world(8,5,std::vector<std::uint8_t>(40,1),sim::RulesProfile::IndustryV5);
    put(world,sim::CommandType::PlaceClaySource,0,1);
    put(world,sim::CommandType::PlacePottery,3,1);
    put(world,sim::CommandType::PlacePottery,3,3);
    road(world,1,1);road(world,2,1);
    until(world,12000,[&] {
        const auto& first=world.building(sim::BuildingId::Pottery);
        const auto& source=world.building(sim::BuildingId::ClaySource);
        return first.output==sim::Rules::pottery_output_capacity &&
            first.input_clay==sim::Rules::pottery_input_capacity &&
            source.output==sim::Rules::clay_output_capacity &&
            world.courier(sim::CourierId::Clay).phase==sim::CourierPhase::IdleAtWorkshop;
    },"first Pottery did not reach a normal full-buffer state");
    check(world.courier_dispatch_status(sim::CourierId::Clay).status==
              sim::CourierDispatchStatus::NoRoad,
          "free but disconnected second Pottery status is not NoRoad");
    road(world,1,2);road(world,1,3);road(world,2,3);
    const auto second=static_cast<sim::BuildingId>(8);
    const auto decision=world.courier_dispatch_status(sim::CourierId::Clay);
    check(decision.status==sim::CourierDispatchStatus::Ready &&
          decision.selected_target==second,
          "free reachable second Pottery was blocked by the full first Pottery");
    world.tick();
    check(world.courier(sim::CourierId::Clay).target==second &&
          world.courier(sim::CourierId::Clay).phase==sim::CourierPhase::ToWarehouse,
          "dispatch decision and actual second-Pottery dispatch differ");
}

}

int main() {
    try {
        connected_screenshot_case();
        output_eight_and_late_road();
        reachable_second_pottery();
        full_first_does_not_block_second();
        std::cout<<"courier triage: screenshot layout and dispatch statuses passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Courier triage test failed: "<<error.what()<<'\n';
        return 1;
    }
}
