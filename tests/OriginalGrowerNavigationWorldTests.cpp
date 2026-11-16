// Explicit movement-only acceptance in real paid Rule26 Worlds. All map/model
// inputs are authored; no movement result, crop work, stock or receipt is injected.
#include "IrrigationFixture.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace q=oe_irrigation_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using f::check;
using Result=sim::GrowerNavigationResult;

template<class Action> void refuses(Action action,const char* reason) {
    try {action();} catch(const std::exception&) {return;}
    throw std::runtime_error(reason);
}

struct Scene {
    sim::BuildingId farm{},field{};
    sim::Cell start{},target{};
};

Scene build(sim::World& world,sim::Cell farm={90,95},sim::Cell field={95,99},
            sim::Cell start={93,97},
            sim::CommandType field_type=sim::CommandType::PlaceOriginalCabbageField) {
    Scene scene;
    scene.farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,farm);
    sim::Command command{field_type,field};
    command.building_id=scene.farm;f::buy(world,command);
    scene.field=*world.building_owner_at(field);scene.start=start;scene.target=field;
    check(sim::building_footprint(world.profile(),world.rule_version(),world.building(scene.farm).kind)==
              sim::BuildingFootprint{3,3} &&
          a::field(world,scene.field).farm_id==scene.farm,
          "Navigation fixture lost the real paid complete Farm or its bound field");
    return scene;
}

sim::GrowerNavigationRequest request(const Scene& scene) {
    sim::GrowerNavigationRequest result;
    result.farm_id=scene.farm;result.field_id=scene.field;
    result.start=scene.start;result.target=scene.target;return result;
}

sim::GrowerNavigationId start(sim::World& world,const sim::GrowerNavigationRequest& intent) {
    const auto result=world.start_grower_navigation(intent);
    check(result.accepted && result.id.has_value(),"Supported actual World navigation was refused: result="+
        std::to_string(static_cast<unsigned>(result.result))+" reason="+result.reason);
    return *result.id;
}

void denied(sim::World& world,const sim::GrowerNavigationRequest& intent,
            std::optional<Result> expected={}) {
    const auto before=world.snapshot();
    const auto tracks=std::vector<sim::GrowerNavigationTrack>(
        world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end());
    const auto result=world.start_grower_navigation(intent);
    check(!result.accepted && !result.id && !result.reason.empty() &&
          result.result!=Result::Arrived && result.result!=Result::Moving &&
          (!expected || result.result==*expected),
          "Navigation denial mismatch: accepted="+std::to_string(result.accepted)+
          " result="+std::to_string(static_cast<unsigned>(result.result))+
          " expected="+(expected ? std::to_string(static_cast<unsigned>(*expected)):"refusal")+
          " id="+(result.id ? std::to_string(result.id->value):"none")+
          " farm="+std::to_string(static_cast<std::uint32_t>(intent.farm_id))+
          " field="+std::to_string(static_cast<std::uint32_t>(intent.field_id))+
          " start="+std::to_string(intent.start.x)+","+std::to_string(intent.start.y)+
          " target="+std::to_string(intent.target.x)+","+std::to_string(intent.target.y)+
          " returning="+std::to_string(intent.returning)+" reason="+result.reason);
    check(world.snapshot()==before &&
          std::vector<sim::GrowerNavigationTrack>(world.grower_navigation_tracks().begin(),
              world.grower_navigation_tracks().end())==tracks,
          "Refused navigation changed paid authority, IDs, Funds or existing motion");
}

void no_crop_authority(const sim::World& world) {
    check(world.rule_version()==26 && !sim::agriculture_production_clock_verified &&
          world.original_production_state()->food==sim::OriginalFoodState{} &&
          world.original_agriculture_state()->trips.empty(),
          "Movement-only session activated a new Food origin or physical Food cargo");
    for(const auto& field:world.original_agriculture_state()->fields)
        check(!field.quantity && !field.productive_events && !field.harvested && !field.last_harvest_cycle,
              "Actual navigation granted unproved crop work, growth or harvest");
    for(const auto& stock:world.original_agriculture_state()->stocks)
        check(stock.amounts==sim::AgricultureAmounts{} && stock.received==sim::AgricultureCounters{} &&
              stock.departed==sim::AgricultureCounters{} && stock.returned==sim::AgricultureCounters{} &&
              stock.consumed==sim::AgricultureCounters{} && !stock.quality_witness,
              "Actual navigation granted raw/processed stock or FoodQuality evidence");
    check(world.navigation_valid() && world.production_balance_valid() && world.city_economy_valid(),
          "Movement acceptance violated ordinary World navigation or accounting");
}

void tick(sim::World& world,sim::World* control=nullptr) {
    world.tick();if(control)control->tick();no_crop_authority(world);
}

void actual_route_phase_cell_and_return() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=build(world);
    auto control=fixture.restore(world.snapshot());
    const auto original=world.snapshot();const auto id=start(world,request(scene));
    const std::vector<sim::Cell> expected{{93,97},{94,98},{95,99}};
    const auto& prepared=world.grower_navigation(id);
    check(prepared.path==expected && prepared.current==scene.start && prepared.cursor==0 &&
          prepared.phase==20 && !prepared.edge_started && prepared.result==Result::Moving &&
          !prepared.microsteps && !prepared.reroutes &&
          world.couriers().size()==original.couriers.size(),
          "Mode17 admission did not prepare its exact shared deterministic route without a paid actor");
    check(!world.clear_grower_navigation(),"An active movement was silently discarded by session cleanup");
    for(int step=1;step<=41;++step) {
        tick(world,&control);const auto& track=world.grower_navigation(id);
        const auto expected_cell=step<11 ? expected[0]:step<31 ? expected[1]:expected[2];
        check(track.current==expected_cell && track.microsteps==static_cast<std::uint64_t>(step),
              "World movement skipped a real phase10 cell commit or its microstep history");
        if(step==1 || step==21)
            check(track.phase==0 && track.cursor==static_cast<std::size_t>(step==1 ? 1:2) && track.edge_started,
                  "Phase20 did not begin exactly one retained route direction");
        if(step==11 || step==31)
            check(track.phase==10 && track.result==Result::Moving,
                  "Cell arrival prematurely exhausted the native route");
        if(step==20)
            check(track.phase==19 && track.cursor==1 && track.edge_started,
                  "The route cursor advanced before the next phase20 boundary");
        if(step<41)check(track.result==Result::Moving,"A route reported8 before its endpoint history completed");
    }
    check(world.grower_navigation(id).result==Result::Arrived &&
          world.grower_navigation(id).current==scene.target &&
          !world.grower_navigation(id).edge_started && world.clear_grower_navigation() &&
          world.snapshot()==control.snapshot(),
          "Completed real route failed exact endpoint8 or changed historical gameplay authority");

    auto returning=request(scene);returning.returning=true;
    returning.start=scene.target;returning.target=scene.start;
    const auto back=start(world,returning);
    check(back.value>id.value && world.grower_navigation(back).path==
              std::vector<sim::Cell>{{95,99},{94,98},{93,97}},
          "Explicit external return reused a lifetime identity or entered the blocked Farm footprint");
    for(int step=0;step<41;++step)tick(world,&control);
    check(world.grower_navigation(back).result==Result::Arrived &&
          world.grower_navigation(back).current==scene.start && world.clear_grower_navigation() &&
          world.snapshot()==control.snapshot(),
          "Real external return granted work, source stock or divergent historical state");
}

void paid_road_movement_and_removal_claims() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=build(world);
    const std::array<sim::Cell,2> roads{{scene.start,{94,98}}};
    for(const auto cell:roads) {
        check(fixture.terrain->raw_terrain(cell)==0x80U,
              "Paid-road acceptance did not use independently known raw80 land");
        f::buy(world,{sim::CommandType::PlaceRoad,cell});
        check(world.object_at(cell)==sim::Object::Road &&
              world.validate({sim::CommandType::RemoveRoad,cell}).accepted,
              "Paid-road fixture did not create a real otherwise removable World road");
    }
    auto control=fixture.restore(world.snapshot());const auto id=start(world,request(scene));
    check(world.grower_navigation(id).path==
              std::vector<sim::Cell>{{93,97},{94,98},{95,99}},
          "Mode17 did not route through the actual paid Road04 cells");
    tick(world,&control);const auto begun=world.grower_navigation(id);
    check(begun.edge_started && begun.current==roads[0] && begun.path.at(begun.cursor)==roads[1],
          "Paid-road removal control did not begin the actual road-to-road section");
    for(const auto cell:roads) {
        const auto before=world.snapshot();
        const auto result=world.execute({sim::CommandType::RemoveRoad,cell});
        check(!result.accepted && !result.changed && world.object_at(cell)==sim::Object::Road &&
              world.snapshot()==before && world.grower_navigation(id)==begun,
              "Road removal changed an actual current or begun movement section");
    }
    for(int step=2;step<=41;++step) {
        tick(world,&control);const auto& track=world.grower_navigation(id);
        if(step==11)check(track.current==roads[1],"World movement never committed the paid Road04 cell");
        if(step==31)check(track.current==scene.target,"Paid-road route never committed its actual field endpoint");
        if(step<41)check(track.result==Result::Moving,"Paid-road route reported8 before completing its real sections");
    }
    check(world.grower_navigation(id).result==Result::Arrived &&
          world.grower_navigation(id).current==scene.target &&
          world.grower_navigation(id).microsteps==41 && world.clear_grower_navigation() &&
          world.snapshot()==control.snapshot(),
          "Actual paid-road movement failed endpoint8 or changed historical road/accounting authority");
}

void literal_obstacle_tie_and_all_five_paid_fields() {
    q::Fixture fixture;auto world=fixture.world();
    const auto scene=build(world,{90,95},{93,96},{89,96});
    auto control=fixture.restore(world.snapshot());const auto id=start(world,request(scene));
    // The complete3x3 Farm separates west/east endpoints. The cardinal flood
    // reaches the goal at distance9. Its reverse North/South candidates tie;
    // direct West is blocked, so the native first-equal North candidate wins.
    // This literal vector independently distinguishes gradient extraction from
    // both a cardinal-only path and a stable/nearest-road tie substitute.
    const std::vector<sim::Cell> expected{
        {89,96},{89,95},{90,94},{91,94},{92,94},{93,95},{93,96}};
    check(world.grower_navigation(id).path==expected,
          "Mode17 obstacle route did not preserve the literal first-equal North tie");
    for(const auto cell:expected)
        check(world.building_owner_at(cell)!=scene.farm,
              "Literal obstacle route crossed the complete blocked Farm footprint");
    const std::array<int,6> commits{11,31,51,71,91,111};std::size_t committed=0;
    for(int step=1;step<=121;++step) {
        tick(world,&control);
        check(!world.building_on_fire(scene.farm) && !world.building_on_fire(scene.field),
              "Literal tie sample encountered real source/field fire at World tick "+std::to_string(world.ticks()));
        if(committed<commits.size() && step==commits[committed])++committed;
        const auto& track=world.grower_navigation(id);
        check(track.current==expected[committed] && track.microsteps==static_cast<std::uint64_t>(step),
              "Literal tie route skipped its actual phase10 cell-commit schedule");
        if(step<121)check(track.result==Result::Moving,"Literal tie route exhausted before all six real edges");
    }
    check(committed==6 && world.grower_navigation(id).result==Result::Arrived &&
          world.grower_navigation(id).current==scene.target && world.clear_grower_navigation() &&
          world.snapshot()==control.snapshot(),
          "Literal tie route failed its real121-microstep endpoint or changed historical authority");

    const std::array kinds{
        std::pair{sim::CommandType::PlaceOriginalWheatField,sim::AgricultureCrop::Wheat},
        std::pair{sim::CommandType::PlaceOriginalMilletField,sim::AgricultureCrop::Millet},
        std::pair{sim::CommandType::PlaceOriginalRiceField,sim::AgricultureCrop::Rice},
        std::pair{sim::CommandType::PlaceOriginalCabbageField,sim::AgricultureCrop::Cabbage},
        std::pair{sim::CommandType::PlaceOriginalSoybeanField,sim::AgricultureCrop::Soybean}};
    for(const auto& [command,crop]:kinds) {
        auto field_world=fixture.world();
        const auto field_scene=build(field_world,{90,95},{95,99},{93,97},command);
        auto field_control=fixture.restore(field_world.snapshot());
        check(a::field(field_world,field_scene.field).crop==crop && !field_world.total_population() &&
              !field_world.workers_assigned(field_scene.farm) &&
              (crop!=sim::AgricultureCrop::Rice || fixture.terrain->fertility(field_scene.target)==100),
              "Five-kind navigation sample lost paid crop identity, source Rice fertility or zero-staff boundary");
        const auto field_id=start(field_world,request(field_scene));
        check(field_world.grower_navigation(field_id).path==
                  std::vector<sim::Cell>{{93,97},{94,98},{95,99}},
              "A separately paid Food field did not enter the shared two-edge route");
        for(int step=0;step<41;++step)tick(field_world,&field_control);
        check(field_world.grower_navigation(field_id).result==Result::Arrived &&
              field_world.grower_navigation(field_id).current==field_scene.target &&
              !field_world.workers_assigned(field_scene.farm) && !field_world.total_population() &&
              field_world.clear_grower_navigation() && field_world.snapshot()==field_control.snapshot(),
              "A paid Food kind failed actual endpoint8 or gained production/staff from navigation");
    }
}

void equality_identity_and_unsupported_interactions() {
    q::Fixture fixture;auto world=fixture.world();
    const auto scene=build(world,{90,95},{93,96},{93,96});
    auto intent=request(scene);
    auto bad=intent;bad.start={92,96};denied(world,bad); // Inside the full paid3x3 Farm.
    bad=intent;bad.start={100,100};denied(world,bad); // Supported land, no source adjacency.
    bad=intent;bad.target={94,96};denied(world,bad); // No actual target identity at that cell.
    bad=intent;bad.field_id=scene.farm;bad.target={90,95};denied(world,bad,Result::Failed);
    bad=intent;bad.farm_id=static_cast<sim::BuildingId>(999999);denied(world,bad);
    bad=intent;bad.start={std::numeric_limits<int>::min(),std::numeric_limits<int>::max()};denied(world,bad);
    bad=intent;bad.target={228,228};denied(world,bad);
    bad=intent;bad.policy=static_cast<sim::GrowerNavigationPolicy>(2);denied(world,bad,Result::Unsupported);
    for(const auto interaction:{sim::GrowerNavigationInteraction::PeerState5,
            sim::GrowerNavigationInteraction::Suspension13c,sim::GrowerNavigationInteraction::Ferry48,
            sim::GrowerNavigationInteraction::Linked66,sim::GrowerNavigationInteraction::Exceptional14,
            sim::GrowerNavigationInteraction::Exceptional15,sim::GrowerNavigationInteraction::Exceptional16,
            sim::GrowerNavigationInteraction::GlobalGate88ec38}) {
        bad=intent;bad.interaction=interaction;denied(world,bad,Result::Unsupported);
    }
    const auto id=start(world,intent);check(id.value==1,"Refused requests consumed session identities");
    for(int step=0;step<2 && world.grower_navigation(id).result==Result::Moving;++step)tick(world);
    check(world.grower_navigation(id).current==scene.target &&
          world.grower_navigation(id).result==Result::Arrived &&
          world.grower_navigation(id).path.size()<=1 && world.clear_grower_navigation(),
          "Missing-route result8 was not justified by exact current/target equality");

    a::Fixture historical;auto old=historical.world();const auto old_scene=build(old);
    denied(old,request(old_scene),Result::Unsupported);
    oe_plantations_fixture::Fixture plantations;auto previous=plantations.world();
    const auto previous_scene=build(previous);denied(previous,request(previous_scene),Result::Unsupported);
}

void shared_channel_budget_and_monotone_session_identity() {
    q::Fixture fixture;auto world=fixture.world();
    const auto scene=build(world,{90,95},{93,96},{93,96});
    const auto intent=request(scene);const auto baseline=world.snapshot();
    check(world.couriers().size()<sim::Rules::original_production_courier_limit,
          "Budget fixture did not retain room inside the existing58-channel bound");
    sim::GrowerNavigationId last{};
    while(world.couriers().size()+world.grower_navigation_tracks().size()<
            sim::Rules::original_production_courier_limit) {
        const auto id=start(world,intent);const auto& track=world.grower_navigation(id);
        check(id.value>last.value && track.result==Result::Arrived && track.current==scene.target &&
              !track.edge_started && track.path.size()<=1,
              "Exact-equality admission reused identity or retained an active route during the budget control");
        last=id;
    }
    check(world.couriers().size()+world.grower_navigation_tracks().size()==
              sim::Rules::original_production_courier_limit,
          "Session acceptance overshot the unchanged movement-channel budget");
    denied(world,intent,Result::Failed);

    const auto full=world.snapshot();const auto tracks=std::vector<sim::GrowerNavigationTrack>(
        world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end());
    const auto result=world.execute({sim::CommandType::PlaceOriginalFoodFarm,{104,95}});
    check(!result.accepted && !result.changed && world.snapshot()==full &&
          std::vector<sim::GrowerNavigationTrack>(world.grower_navigation_tracks().begin(),
              world.grower_navigation_tracks().end())==tracks,
          "Paid Farm construction bypassed the shared full channel budget or changed Funds, IDs or history");
    check(world.clear_grower_navigation() && world.grower_navigation_tracks().empty() &&
          world.snapshot()==baseline,
          "Clearing terminal navigation changed gameplay identities, paid history or historical authority");
    const auto next=start(world,intent);
    check(next.value>last.value && world.grower_navigation(next).result==Result::Arrived &&
          world.clear_grower_navigation() && world.snapshot()==baseline,
          "Session cleanup reused a navigation identity or consumed a paid gameplay identity");
    no_crop_authority(world);
}

void invalid_terrain_occupancy_height_and_boundary() {
    q::Fixture region;
    // A distant marker is outside this short route's flood. The stronger
    // complete-source gate must still refuse it before enabling the session.
    region.raw[140U*228U+150U]|=0x10000U;region.rebind();
    auto region_world=region.world();const auto region_scene=build(region_world);
    denied(region_world,request(region_scene),Result::Unsupported);

    q::Fixture unknown;unknown.raw[97U*228U+94U]=0x80000000U;unknown.rebind();
    auto raw_world=unknown.world();const auto raw_scene=build(raw_world);
    denied(raw_world,request(raw_scene),Result::Unsupported);

    q::Fixture occupied;auto body_world=occupied.world();const auto body_scene=build(body_world);
    (void)q::purchase(body_world,sim::CommandType::PlaceHousehold,{94,97});
    denied(body_world,request(body_scene),Result::Unsupported);

    q::Fixture height;
    auto cells=height.policy->cells();cells[97U*228U+94U].height=1;
    height.policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),
        std::vector<sim::FixedGatePassage>{});
    auto high_world=height.world();const auto high_scene=build(high_world);
    denied(high_world,request(high_scene),Result::Unsupported);

    q::Fixture boundary;auto outside=boundary.world();
    const auto outside_scene=build(outside,{20,25},{25,29},{23,27});
    denied(outside,request(outside_scene));

    // This source and every Farm cell lie inside the admitted source domain;
    // only the separately paid field endpoint lacks the required boundary.
    auto blocked_target=boundary.world();
    const auto edge_scene=build(blocked_target,{10,110},{7,107},{13,112});
    check(boundary.terrain->irrigation_boundary_allowed(edge_scene.start) &&
          !boundary.terrain->irrigation_boundary_allowed(edge_scene.target),
          "Blocked-target fixture did not distinguish valid source and outside endpoint");
    for(const auto cell:sim::building_footprint_cells(blocked_target.profile(),blocked_target.rule_version(),
            blocked_target.building(edge_scene.farm).kind,blocked_target.building(edge_scene.farm).cell))
        check(boundary.terrain->irrigation_boundary_allowed(cell),
              "Blocked-target failure used an incomplete or outside source footprint");
    denied(blocked_target,request(edge_scene),Result::Failed);

    q::Fixture binding;auto different=binding.world();const auto first=build(different);
    const auto second=build(different,{103,105},{108,109},{106,107});
    auto wrong=request(first);wrong.field_id=second.field;wrong.target=second.target;denied(different,wrong);
}

void begun_claims_future_obstruction_retry_and_reroute() {
    q::Fixture fixture;auto world=fixture.world();
    const auto scene=build(world,{90,95},{95,100},{93,95});
    const auto id=start(world,request(scene));tick(world);
    const auto begun=world.grower_navigation(id);
    check(begun.edge_started && begun.cursor==1 && begun.path.size()>3,
          "Road-cut acceptance did not begin a real native movement section");
    for(const auto cell:{begun.current,begun.path.at(begun.cursor)}) {
        const auto before=world.snapshot();sim::Command command{sim::CommandType::PlaceOriginalCabbageField,cell};
        command.building_id=scene.farm;const auto result=world.execute(command);
        check(!result.accepted && !result.changed && world.snapshot()==before &&
              world.grower_navigation(id)==begun,
              "Construction crossed an actual current cell or already begun section");
        const std::array<sim::Cell,1> ditch_cells{cell};
        const auto ditch=world.execute_irrigation_batch(ditch_cells);
        check(!ditch.accepted && !ditch.changed && world.snapshot()==before &&
              world.grower_navigation(id)==begun,
              "Irrigation batch bypassed an actual current-cell or begun-edge claim");
    }
    // Farm193 is a proved blocked navigation class. Unknown House/body classes
    // must reject the query, so they are deliberately not used as opaque walls.
    (void)q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{92,98});
    bool saw_retry=false,saw_preserved_commit=false;
    for(int step=0;step<240 && world.grower_navigation(id).result!=Result::Arrived;++step) {
        tick(world);const auto& track=world.grower_navigation(id);
        if(track.current==begun.path.at(begun.cursor))saw_preserved_commit=true;
        if(track.result==Result::Retry) {
            saw_retry=true;check(!track.edge_started && track.phase==20 && !track.blocker.empty(),
                "Retry was promoted to arrival or interrupted an already begun movement section");
        }
        check(track.result!=Result::Failed && track.result!=Result::Unsupported,
              "Proved Farm obstruction failed to use the available actual shared reroute");
    }
    const auto& arrived=world.grower_navigation(id);
    check(saw_preserved_commit && saw_retry && arrived.reroutes>0 && arrived.result==Result::Arrived &&
          arrived.current==scene.target && arrived.path.back()==scene.target,
          "Future blockage did not preserve its begun cell, distinguish9 and recompute to real endpoint8");
    check(world.clear_grower_navigation(),"Terminal rerouted session could not be cleared");
}

void actual_route_failure_and_safe_source_stop() {
    q::Fixture fixture;auto failed=fixture.world();
    const auto scene=build(failed);
    auto returning=request(scene);returning.returning=true;
    returning.start=scene.target;returning.target=scene.start;
    const auto id=start(failed,returning);tick(failed);
    const auto begun=failed.grower_navigation(id);
    check(begun.path==std::vector<sim::Cell>{{95,99},{94,98},{93,97}},
          "Return failure fixture did not prepare its real external endpoint route");
    // A second complete paid Farm covers only the unbegun return endpoint.
    // Its proved02 class blocks both runtime entry and route re-preparation.
    (void)q::purchase(failed,sim::CommandType::PlaceOriginalFoodFarm,{93,95});
    bool saw_retry=false,saw_committed_cell=false;
    for(int step=0;step<160 && failed.grower_navigation(id).result!=Result::Failed;++step) {
        tick(failed);const auto& track=failed.grower_navigation(id);
        saw_retry|=track.result==Result::Retry;
        saw_committed_cell|=track.current==begun.path.at(begun.cursor);
        check(track.result!=Result::Arrived,"Unsupported future occupancy was promoted to endpoint8");
    }
    const auto& failure=failed.grower_navigation(id);
    check(saw_committed_cell && saw_retry && failure.result==Result::Failed && failure.reroutes>0 &&
          failure.current!=returning.target && !failure.edge_started && !failure.blocker.empty() &&
          failed.clear_grower_navigation(),
          "Actual World cell history did not distinguish retry9 and failed re-preparation10");

    auto unknown=fixture.world();const auto unknown_scene=build(unknown,{90,95},{95,100},{93,95});
    const auto unknown_id=start(unknown,request(unknown_scene));tick(unknown);
    const auto unknown_begun=unknown.grower_navigation(unknown_id);
    // Unknown occupancy is not a wall that may be routed around. Introduce
    // it beyond the begun edge and require an explicit unsupported outcome.
    (void)q::purchase(unknown,sim::CommandType::PlaceHousehold,{93,98});
    bool unknown_retry=false,unknown_commit=false;
    for(int step=0;step<160 && unknown.grower_navigation(unknown_id).result!=Result::Unsupported;++step) {
        tick(unknown);const auto& track=unknown.grower_navigation(unknown_id);
        unknown_retry|=track.result==Result::Retry;
        unknown_commit|=track.current==unknown_begun.path.at(unknown_begun.cursor);
        check(track.result!=Result::Arrived && track.result!=Result::Failed,
              "Unknown future occupancy was treated as a successful or proved failed route");
    }
    const auto& unsupported=unknown.grower_navigation(unknown_id);
    check(unknown_commit && unknown_retry && unsupported.result==Result::Unsupported &&
          unsupported.current!=unknown_scene.target && !unsupported.edge_started &&
          !unsupported.blocker.empty() && unknown.clear_grower_navigation(),
          "Unsupported route preparation lost actual cell history or silently used unknown occupancy as a wall");

    auto stopped=fixture.world();const auto stop_scene=build(stopped);
    const auto stop_id=start(stopped,request(stop_scene));tick(stopped);
    const auto stop_begun=stopped.grower_navigation(stop_id);
    f::buy(stopped,sim::set_building_operation(stop_scene.farm,false));
    bool preserved=false;
    for(int step=0;step<40 && stopped.grower_navigation(stop_id).result==Result::Moving;++step) {
        tick(stopped);const auto& track=stopped.grower_navigation(stop_id);
        preserved|=track.current==stop_begun.path.at(stop_begun.cursor);
        check(track.result!=Result::Arrived,"Unsupported source lifecycle granted an arrival");
    }
    const auto& stop=stopped.grower_navigation(stop_id);
    check(preserved && stop.result==Result::Unsupported && !stop.edge_started && stop.phase==20 &&
          stop.current!=stop_scene.target && !stop.blocker.empty() && stopped.clear_grower_navigation(),
          "Source lifecycle rejection destroyed a begun edge or pretended to reproduce Actor26 interruption");

    auto final_edge=fixture.world();const auto final_scene=build(final_edge);
    const auto final_id=start(final_edge,request(final_scene));
    for(int step=0;step<21;++step)tick(final_edge);
    check(final_edge.grower_navigation(final_id).edge_started &&
          final_edge.grower_navigation(final_id).cursor==2,
          "Final-edge stop fixture did not begin its actual last route direction");
    f::buy(final_edge,sim::set_building_operation(final_scene.farm,false));
    bool reached=false;
    for(int step=0;step<40 && final_edge.grower_navigation(final_id).result==Result::Moving;++step) {
        tick(final_edge);const auto& track=final_edge.grower_navigation(final_id);
        reached|=track.current==final_scene.target;
        check(track.result!=Result::Arrived,"Last-edge source veto was lost before route completion");
    }
    const auto& final_stop=final_edge.grower_navigation(final_id);
    check(reached && final_stop.current==final_scene.target && final_stop.result==Result::Unsupported &&
          !final_stop.edge_started && final_stop.phase==20 && final_edge.clear_grower_navigation(),
          "A safely completed final cell discarded the retained unsupported context or granted endpoint8");
}

void simultaneous_independent_replay_and_historical_save() {
    q::Fixture fixture;auto world=fixture.world();
    const auto first=build(world),second=build(world,{103,105},{108,109},{106,107});
    auto replay=fixture.restore(world.snapshot());auto control=fixture.restore(world.snapshot());
    const auto old_document=fixture.document(world);
    const auto saved=fixture.root.path/"navigation-scope42.json";
    save::write_save(saved,old_document,fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(saved),source=f::read_text(fixture.root.data/f::map_relative);
    const auto model=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    const auto first_id=start(world,request(first)),second_id=start(world,request(second));
    check(start(replay,request(first))==first_id && start(replay,request(second))==second_id &&
          first_id!=second_id && world.grower_navigation_tracks().size()==2 &&
          world.couriers().size()+world.grower_navigation_tracks().size()<=sim::Rules::original_production_courier_limit,
          "Concurrent actual routes reused identity or exceeded the existing movement-channel budget");
    denied(world,request(first),Result::Unsupported); // A real peer occupies this start.
    refuses([&]{(void)fixture.document(world);},"Active session silently entered an old save document");
    refuses([&]{(void)fixture.restore(world.snapshot());},"Historical restore silently dropped active session motion");
    auto transient=old_document;transient.world=world.snapshot();
    refuses([&]{save::write_save(saved,transient,fixture.root.data,fixture.mask());},
            "Active session was published through a manually retained historical document");
    check(f::read_text(saved)==bytes,"Refused session publication changed an existing historical save");
    for(int step=0;step<41;++step) {
        tick(world,&control);tick(replay);
        check(world.grower_navigation(first_id)==replay.grower_navigation(first_id) &&
              world.grower_navigation(second_id)==replay.grower_navigation(second_id) &&
              world.snapshot()==replay.snapshot(),
              "Independently restored World routes diverged in actual cells, cursors, outcomes or gameplay");
    }
    check(world.grower_navigation(first_id).result==Result::Arrived &&
          world.grower_navigation(second_id).result==Result::Arrived &&
          world.clear_grower_navigation() && replay.clear_grower_navigation() &&
          world.snapshot()==control.snapshot() && world.snapshot()==replay.snapshot(),
          "Concurrent completed navigation left persistent production or changed historical World state");
    save::write_save(saved,fixture.document(world),fixture.root.data,fixture.mask());
    const auto completed=f::read_text(saved);const auto parsed=save::read_save(saved);
    const auto json=nlohmann::json::parse(completed);
    const auto control_path=fixture.root.path/"navigation-independent-control42.json";
    save::write_save(control_path,fixture.document(control),fixture.root.data,fixture.mask());
    check(completed==f::read_text(control_path) && parsed.source_schema_version==42 &&
          parsed.world.rule_version==26 && fixture.restore(parsed).snapshot()==world.snapshot() &&
          !json.at("world").contains("grower_navigation") &&
          !json.at("world").at("agriculture").contains("food_growers") &&
          f::read_text(fixture.root.data/f::map_relative)==source &&
          f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model,
          "Navigation cleanup changed schema42 bytes, raw authority, exact restore or source inputs");
}
} // namespace

int main() {
    try {
        actual_route_phase_cell_and_return();
        paid_road_movement_and_removal_claims();
        literal_obstacle_tie_and_all_five_paid_fields();
        equality_identity_and_unsupported_interactions();
        shared_channel_budget_and_monotone_session_identity();
        invalid_terrain_occupancy_height_and_boundary();
        begun_claims_future_obstruction_retry_and_reroute();
        actual_route_failure_and_safe_source_stop();
        simultaneous_independent_replay_and_historical_save();
        std::cout<<"Original Grower movement-only World navigation acceptance passed; no Food production grant\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
