#include "simulation/World.h"
#include "simulation/RoadConnectivity.h"
#include "simulation/CityStartGuidance.h"
#include "core/PerformanceDiagnostics.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace sim=openemperor::simulation;
namespace perf=openemperor::performance;
namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action,const char* reason) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(reason);
}
constexpr int size=24;
std::size_t ix(sim::Cell cell,int width=size) {
    return static_cast<std::size_t>(cell.y*width+cell.x);
}
auto cells(int width=size,int height=size) {
    return std::vector<sim::MapCellPermission>(static_cast<std::size_t>(width*height),
        {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
}
void original(std::vector<sim::MapCellPermission>& permissions,sim::Cell cell,int width=size) {
    auto& value=permissions[ix(cell,width)];
    value.original_road=true;
    value.original_road_classification=sim::OriginalRoadClassification::GroundRoad;
    value.building_allowed=false;
    value.building_blocker=sim::BuildBlocker::ProtectedOriginalRoad;
}
auto policy(std::vector<sim::MapCellPermission> permissions,
            std::vector<sim::FixedGatePassage> gates={},int width=size,int height=size) {
    return std::make_shared<const sim::MapPermissions>(width,height,2,
        std::move(permissions),std::move(gates));
}
sim::BuildingId place(sim::World& world,sim::CommandType type,sim::Cell cell) {
    const auto result=world.execute({type,cell});
    if (!result.accepted || !result.changed)
        throw std::runtime_error("Paid fixture command: "+result.reason);
    return *world.building_owner_at(cell);
}
void houses(sim::World& world) {
    place(world,sim::CommandType::PlaceHousehold,{1,15});
    place(world,sim::CommandType::PlaceHousehold,{4,15});
}
sim::CourierId owned(const sim::World& world,sim::BuildingId owner) {
    for (const auto& courier:world.couriers()) if (courier.owner==owner) return courier.id;
    throw std::runtime_error("Fixture courier missing");
}
void invariants(const sim::World& world) {
    check(world.navigation_valid() && world.production_balance_valid() &&
        world.food_balance_valid() && world.city_economy_valid() && world.population_valid() &&
        world.service_state_valid() && world.fire_state_valid() && world.health_state_valid(),
        "Original-road simulation invariant failed");
}
template<class P> void until(sim::World& world,P done,int limit=500) {
    while (limit-- && !done()) { world.tick(); invariants(world); }
    check(done(),"Original-road trip timed out");
}
void version_contract() {
    check(sim::current_rule_version(sim::RulesProfile::CityV16)==4,
          "New City-v16 games must use original-road rule 4");
    for (const auto version:{1U,2U,3U,4U})
        check(sim::rule_version_supported(sim::RulesProfile::CityV16,version),
              "City-v16 historical and original-road rules must remain explicit");
    check(sim::rule_version_supported(sim::RulesProfile::CityV16,6) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,7) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,8) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,9) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,10) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,11) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,12) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,13) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,14) &&
          sim::rule_version_supported(sim::RulesProfile::CityV16,15) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,16) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,17) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,18) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,19) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,20) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,21) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,22) &&
        sim::rule_version_supported(sim::RulesProfile::CityV16,23) &&
        !sim::rule_version_supported(sim::RulesProfile::CityV16,24),
          "Unknown City-v16 rule accepted");
    check(sim::original_fire_protection_enabled(sim::RulesProfile::CityV16,11) &&
        sim::original_fire_protection_enabled(sim::RulesProfile::CityV16,12) &&
        !sim::original_fire_protection_enabled(sim::RulesProfile::CityV16,10),
        "Fire protection escaped its explicit Rule11/12 opt-ins");
    check(sim::original_sustainable_economy_enabled(sim::RulesProfile::CityV16,12) &&
        !sim::original_sustainable_economy_enabled(sim::RulesProfile::CityV16,11),
        "Sustainable economy escaped its explicit Rule12 opt-in");
    const auto p2=policy(cells());
    const auto p1=sim::authored_simple_permissions(size,size,std::vector<std::uint8_t>(size*size,1));
    sim::World old(p1,sim::RulesProfile::CityV16,3),current(p2);
    check(old.rule_version()==3 && current.rule_version()==4 &&
        current.map_permissions()==p2,"Version-bound immutable policy not retained");
    rejects([&] { sim::World invalid(p1,sim::RulesProfile::CityV16,4); },"rule4/policy1 accepted");
    rejects([&] { sim::World invalid(p2,sim::RulesProfile::CityV16,3); },"rule3/policy2 accepted");
    rejects([&] { sim::World invalid(size,size,std::vector<std::uint8_t>(size*size,1),
        sim::RulesProfile::CityV16,4); },"Rule4 substituted a legacy buildability mask for map authority");
    rejects([&] { auto invalid=sim::World::restore(current.snapshot(),p1); (void)invalid; },
        "Rule4 restore substituted policy1");
    rejects([&] { auto invalid=sim::World::restore(current.snapshot(),p2->building_mask()); (void)invalid; },
        "Rule4 restore substituted mask-only authority");
    auto rule3_mask=sim::World(size,size,p1->building_mask(),sim::RulesProfile::CityV16,3);
    check(rule3_mask.map_permissions()->canonical_state()==p1->canonical_state(),
        "Historical rule3 authored mask behavior changed");
    auto expected=sim::profile_rules(sim::RulesProfile::CityV16,3);expected.version=4;
    check(sim::profile_rules(sim::RulesProfile::CityV16,4)==expected,
        "Original-road rule changed economic/timing parameters");
}
void shared_road_and_user_geometry() {
    auto permissions=cells();original(permissions,{2,4});const auto p=policy(permissions);
    sim::World world(p);houses(world);
    const auto clay=place(world,sim::CommandType::PlaceClaySource,{2,5});
    const auto pottery=place(world,sim::CommandType::PlacePottery,{2,2});
    const auto id=owned(world,clay);
    check(world.object_at({2,4})==sim::Object::Empty && world.original_road({2,4}) &&
        world.road_cell({2,4}) && world.transport_cell({2,4}) && world.snapshot().roads.empty() &&
        world.roads_placed_total()==0,"Imported road became mutable or was not transport");
    check(world.building_entrances(clay)==std::vector<sim::BuildingEntrance>{{{2,4},{2,5}}} &&
        world.building_entrances(pottery)==std::vector<sim::BuildingEntrance>{{{2,4},{2,3}}},
        "Shared original road must attach both full building footprints");
    const auto route=world.find_building_route(clay,pottery);
    check(route==std::optional<std::vector<sim::Cell>>{{{2,5},{2,4},{2,3}}},
        "Shared adjacent original road gained an artificial minimum route length");
    const auto before=world.snapshot();const auto report=sim::diagnose_road_connectivity(world,clay,pottery);
    check(report.category==sim::RoadConnectivityCategory::Connected && report.route==route &&
        report.policy_version==2 && report.transport_cells==1 && world.snapshot()==before,
        "New-policy diagnosis differs from authoritative pure route");
    until(world,[&] { return world.courier(id).phase==sim::CourierPhase::ToWarehouse; });
    check(world.courier(id).cargo>0 && world.courier(id).reserved==world.courier(id).cargo &&
        world.building(pottery).reserved_incoming==world.courier(id).reserved,
        "Original-road dispatch did not reserve actual produced Clay");
    until(world,[&] { return world.courier(id).path[world.courier(id).path_vertex]==sim::Cell{2,4}; });
    auto budget=sim::World::restore(world.snapshot(),p);
    for (const int y:{8,11}) for (const int x:{8,11,14,17})
        place(budget,sim::CommandType::PlaceHousehold,{x,y});
    const auto budget_before=budget.snapshot();
    const auto warning=sim::starter_budget_warning(budget,{sim::CommandType::PlaceFarm,{12,18}});
    check(warning.has_value() && budget.snapshot()==budget_before && budget.map_permissions()==p,
        "Concrete budget hypothetical lost full original-road policy or changed active cargo");
    auto loaded=sim::World::restore(world.snapshot(),p);
    check(loaded.snapshot()==world.snapshot() && loaded.object_at({2,4})==sim::Object::Empty,
        "Snapshot restore requires a fake mutable road under actual courier");
    bool arrived=false,returned=false;
    for (int i=0;i<100 && !returned;++i) {
        const auto phase=world.courier(id).phase;
        world.tick();loaded.tick();invariants(world);
        check(world.snapshot()==loaded.snapshot(),"Original-road restored trip diverged");
        arrived|=phase==sim::CourierPhase::ToWarehouse &&
            world.courier(id).phase==sim::CourierPhase::Returning;
        returned|=arrived && world.courier(id).phase==sim::CourierPhase::IdleAtWorkshop;
    }
    check(arrived && returned && world.building(pottery).input_clay>0 &&
        world.building(pottery).reserved_incoming==0 && world.snapshot().roads.empty(),
        "Actual delivery/return required buying or injecting original road");
    until(world,[&] { return world.building(pottery).recipes_completed>0; });
    check(world.roads_placed_total()==0,"Original-road production bought hidden roads");

    // The user's coordinate geometry is an authored regression, not a claim
    // that this matrix is Oliver's actual original map or save.
    constexpr int user_size=128;
    auto user_cells=cells(user_size,user_size);original(user_cells,{116,110},user_size);
    sim::World user(policy(std::move(user_cells),{},user_size,user_size));
    const auto source=place(user,sim::CommandType::PlaceClaySource,{116,111});
    const auto target=place(user,sim::CommandType::PlacePottery,{116,108});
    check(user.find_building_route(source,target)==
        std::optional<std::vector<sim::Cell>>{{{116,111},{116,110},{116,109}}},
        "User's full 2x2 footprint geometry cannot share the intervening road");
}
sim::FixedGatePassage gate() {
    sim::FixedGatePassage g;g.id={101};
    for (int y=7;y<=9;++y) for (int x=5;x<=9;++x) g.protected_footprint.push_back({x,y});
    for (int y=7;y<=9;++y) g.corridor.push_back({7,y});
    g.openings={sim::Cell{7,6},sim::Cell{7,10}};return g;
}
auto gate_policy() {
    auto permissions=cells();const auto g=gate();
    for (const auto cell:g.protected_footprint)
        permissions[ix(cell)]={false,false,true,0,sim::BuildBlocker::GateSolidPart,
            sim::BuildBlocker::OriginalStructure};
    for (const auto cell:g.corridor) permissions[ix(cell)].road_allowed=true;
    for (int x=0;x<size;++x) if (x<5 || x>9)
        permissions[ix({x,8})]={false,false,true,0,sim::BuildBlocker::OriginalStructure,
            sim::BuildBlocker::OriginalStructure};
    for (const auto cell:{sim::Cell{4,5},sim::Cell{5,5},sim::Cell{6,5},sim::Cell{7,5},sim::Cell{7,10}})
        original(permissions,cell);
    return policy(std::move(permissions),{g});
}
void commands_preview_batch_and_gate() {
    const auto p=gate_policy();sim::World world(p);
    const auto before=world.snapshot();const auto funds=world.treasury();
    const auto noop=world.execute({sim::CommandType::PlaceRoad,{7,5}});
    check(noop.accepted && !noop.changed && noop.reason=="Original road already available." &&
        world.command_sequence()==before.command_sequence+1 && world.road_revision()==0 &&
        world.treasury()==funds && world.roads_placed_total()==0 && world.snapshot().roads.empty(),
        "Original road no-op charged, mutated or violated single-command sequence");
    const auto stable=world.snapshot();
    const auto remove=world.execute({sim::CommandType::RemoveRoad,{7,5}});
    check(!remove.accepted && remove.diagnostic.blocker==sim::BuildBlocker::ProtectedOriginalRoad &&
        remove.reason=="Original map road cannot be removed." && world.snapshot()==stable,
        "Immutable original road was removed or unexplained");
    const auto overlap=world.execute({sim::CommandType::PlaceHousehold,{6,4}});
    check(!overlap.accepted && overlap.diagnostic.cell==sim::Cell{6,5} &&
        overlap.diagnostic.blocker==sim::BuildBlocker::ProtectedOriginalRoad && world.snapshot()==stable,
        "Full building footprint overwrote an original road");
    check(world.execute({sim::CommandType::PlaceRoad,{7,6}}).changed,"Owned gate attachment failed");
    const std::vector<sim::Cell> mixed{{7,5},{7,6},{7,7},{7,8},{7,9},{7,10},{7,11}};
    const auto mixed_before=world.snapshot();
    perf::set_enabled(true);perf::reset();
    const auto preview=world.validate_road_batch(mixed);
    check(preview.accepted && preview.new_road_count==1 && preview.total_cost==sim::Rules::road_cost &&
        preview.cell_diagnostics.size()==mixed.size() && world.snapshot()==mixed_before,
        "Mixed original/owned/passage/new preview charged an immutable road");
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,
        perf::Counter::WorldExecutes,perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,
        perf::Counter::FileReads,perf::Counter::AssetDecodes,perf::Counter::TextureUploads})
        check(perf::counter(counter)==0,"Road preview performed prohibited work");
    const auto commit=world.execute_road_batch(mixed);
    check(commit.accepted && commit.changed && world.roads_placed_total()==2 &&
        world.command_sequence()==mixed_before.command_sequence+1 &&
        world.road_revision()==mixed_before.road_revision+1 &&
        world.treasury()==mixed_before.treasury-sim::Rules::road_cost &&
        perf::counter(perf::Counter::WorldCopies)==1 && perf::counter(perf::Counter::RouteRefreshes)==1,
        "Atomic mixed commit does not bill/refresh only new roads");
    perf::reset();const auto all_present=world.snapshot();
    check(world.execute_road_batch(mixed).accepted && world.snapshot()==all_present &&
        perf::counter(perf::Counter::WorldCopies)==0 && perf::counter(perf::Counter::RouteRefreshes)==0,
        "All-existing mixed batch mutated or refreshed World");
    perf::set_enabled(false);
    const std::vector<sim::Cell> blocked{{7,11},{8,11},{8,10},{8,9}};
    check(!world.validate_road_batch(blocked).accepted && !world.execute_road_batch(blocked).accepted &&
        world.snapshot()==all_present,"Blocked mixed batch partially committed or charged");
    check(world.execute({sim::CommandType::RemoveRoad,{7,11}}).accepted &&
        !world.transport_cell({7,11}) && world.original_road({7,10}),
        "Owned attachment removal modified immutable original road");

    houses(world);const auto source=place(world,sim::CommandType::PlaceClaySource,{3,3});
    const auto target=place(world,sim::CommandType::PlacePottery,{7,12});
    check(world.execute({sim::CommandType::PlaceRoad,{7,11}}).changed,"Gate target attachment failed");
    const auto route=world.find_building_route(source,target);
    check(route && std::all_of(p->gates()[0].corridor.begin(),p->gates()[0].corridor.end(),
        [&](sim::Cell cell) { return std::count(route->begin(),route->end(),cell)==1; }),
        "Mixed original road/owned road/gate route skipped the corridor");
    check(world.transport_edge_allowed({7,10},{7,9}) &&
        !world.transport_edge_allowed({7,8},{6,8}) &&
        p->transport_edge_blocker({7,8},{6,8})==sim::BuildBlocker::GateSideEntry,
        "Original road changed gate opening/forbidden side entry");
    const auto id=owned(world,source);until(world,[&] { return world.courier(id).phase==sim::CourierPhase::Returning; });
    check(world.building(target).input_clay>0,"Mixed-gate route failed actual Clay delivery");
    until(world,[&] { return world.courier(id).phase==sim::CourierPhase::IdleAtWorkshop; });
}
void heights_missing_and_policy_revision() {
    auto permission=cells();original(permission,{2,4});permission[ix({2,4})].height=-2;
    sim::World unequal(policy(permission));
    const auto source=place(unequal,sim::CommandType::PlaceClaySource,{2,5});
    const auto target=place(unequal,sim::CommandType::PlacePottery,{2,2});
    check(unequal.building_entrances(source).empty() && unequal.building_entrances(target).empty() &&
        !unequal.find_building_route(source,target),"Original road silently bridged building edge heights");
    const auto report=sim::diagnose_road_connectivity(unequal,source,target);
    check(report.category==sim::RoadConnectivityCategory::SourceNoEntrance &&
        std::any_of(report.blocked_edges.begin(),report.blocked_edges.end(),[](const auto& edge) {
            return edge.category==sim::RoadConnectivityCategory::BlockedHeightTransition;
        }),"Original-road height blocker disappeared from measured diagnosis");
    check(std::any_of(report.blocked_edges.begin(),report.blocked_edges.end(),[](const auto& edge) {
        return edge.to.cell==sim::Cell{2,4} && edge.to.original_road && edge.to.traversable &&
            edge.to.transport_source==sim::TransportCellSource::OriginalMapRoad &&
            edge.to.imported_under_policy==2 && edge.to.height==-2;
    }),"Frozen original-road/height/provenance facts absent from report");
    for (const auto cell:{sim::Cell{2,5},sim::Cell{2,3}}) permission[ix(cell)].height=-2;
    sim::World same(policy(permission));
    const auto a=place(same,sim::CommandType::PlaceClaySource,{2,5});
    const auto b=place(same,sim::CommandType::PlacePottery,{2,2});
    check(same.find_building_route(a,b).has_value(),"Equal negative-height original road entrance rejected");
    auto uphill_cells=cells();
    for (int x=4;x<=8;++x) original(uphill_cells,{x,2});
    uphill_cells[ix({6,2})].height=1;sim::World uphill(policy(uphill_cells));
    const auto uphill_source=place(uphill,sim::CommandType::PlaceClaySource,{2,2});
    const auto uphill_target=place(uphill,sim::CommandType::PlacePottery,{9,2});
    check(!uphill.transport_edge_allowed({5,2},{6,2}) && !uphill.find_building_route(uphill_source,uphill_target),
        "Original-to-original road edge silently bridged different signed heights");
    const auto uphill_report=sim::diagnose_road_connectivity(uphill,uphill_source,uphill_target);
    check(uphill_report.category==sim::RoadConnectivityCategory::DisconnectedRoadComponents &&
        uphill_report.blocked_category_counts[static_cast<std::size_t>(
            sim::RoadConnectivityCategory::BlockedHeightTransition)]>=2,
        "Disconnected original-road components lost measured height-edge facts");
    const auto uphill_before=uphill.snapshot();const std::vector<sim::Cell> uphill_drag{{5,2},{6,2}};
    check(!uphill.validate_road_batch(uphill_drag).accepted && !uphill.execute_road_batch(uphill_drag).accepted &&
        uphill.snapshot()==uphill_before,"All-existing original road drag ignored blocked heights");
    auto line=cells();for (int x=4;x<=8;++x) if (x!=6) original(line,{x,2});
    sim::World gap(policy(line));houses(gap);
    const auto clay=place(gap,sim::CommandType::PlaceClaySource,{2,2});
    const auto pottery=place(gap,sim::CommandType::PlacePottery,{9,2});
    until(gap,[&] { return gap.building(clay).output>0; });
    check(gap.courier_dispatch_status(owned(gap,clay)).status==sim::CourierDispatchStatus::NoRoad &&
        !gap.find_building_route(clay,pottery),"Missing original road segment became implicit grass transport");
    const auto revision=gap.road_revision();
    check(gap.execute({sim::CommandType::PlaceRoad,{6,2}}).changed &&
        gap.road_revision()==revision+1 && gap.find_building_route(clay,pottery).has_value(),
        "Owned repair did not invalidate route caches against immutable topology");
    until(gap,[&] { return gap.courier(owned(gap,clay)).phase==sim::CourierPhase::Returning; });
    check(gap.building(pottery).input_clay>0,"Mixed gap repair never delivered actual cargo");

    auto visual_cells=cells();
    visual_cells[ix({2,4})].original_road_classification=sim::OriginalRoadClassification::GroundRoad;
    sim::World old(std::make_shared<const sim::MapPermissions>(size,size,1,visual_cells,
        std::vector<sim::FixedGatePassage>{}),sim::RulesProfile::CityV16,3);
    const auto old_source=place(old,sim::CommandType::PlaceClaySource,{2,5});
    const auto old_target=place(old,sim::CommandType::PlacePottery,{2,2});
    const auto old_report=sim::diagnose_road_connectivity(old,old_source,old_target);
    check(std::any_of(old_report.blocked_edges.begin(),old_report.blocked_edges.end(),[](const auto& edge) {
        return edge.to.cell==sim::Cell{2,4} && !edge.to.original_road && !edge.to.traversable &&
            edge.reason=="Original road is visual-only under this saved policy.";
    }),"Saved policy1 known visual-only original road still reported as unexplained missing cell");
    visual_cells[ix({2,4})].original_road_classification=sim::OriginalRoadClassification::WaterOrBridge;
    visual_cells[ix({2,4})].road_allowed=false;
    visual_cells[ix({2,4})].road_blocker=sim::BuildBlocker::UnsupportedOriginalRoad;
    sim::World water(policy(visual_cells));
    const auto water_source=place(water,sim::CommandType::PlaceClaySource,{2,5});
    const auto water_target=place(water,sim::CommandType::PlacePottery,{2,2});
    const auto water_report=sim::diagnose_road_connectivity(water,water_source,water_target);
    check(std::any_of(water_report.blocked_edges.begin(),water_report.blocked_edges.end(),[](const auto& edge) {
        return edge.to.cell==sim::Cell{2,4} && edge.category==sim::RoadConnectivityCategory::UnsupportedOrUnknown &&
            edge.reason=="Original water/bridge road is unsupported";
    }),"Known original water/bridge road fell back to generic missing-road diagnosis");
}
void interrupted_original_trip() {
    auto permission=cells();for (int x=4;x<=7;++x) original(permission,{x,2});
    const auto p=policy(permission);sim::World world(p);houses(world);
    const auto source=place(world,sim::CommandType::PlaceClaySource,{2,2});
    const auto target=place(world,sim::CommandType::PlacePottery,{9,2});
    check(world.execute({sim::CommandType::PlaceRoad,{8,2}}).changed,"Paid target attachment failed");
    const auto id=owned(world,source);
    until(world,[&] { const auto& c=world.courier(id);return c.phase==sim::CourierPhase::ToWarehouse &&
        c.path[c.path_vertex]==sim::Cell{4,2} && c.edge_progress==1; });
    const auto before=world.courier(id);const auto position=world.courier_position(id);
    const auto active=world.snapshot();auto loaded=sim::World::restore(active,p);
    check(!world.execute({sim::CommandType::RemoveRoad,{4,2}}).accepted,
        "Courier original road was removable");
    check(world.execute({sim::CommandType::RemoveRoad,{8,2}}).accepted &&
        loaded.execute({sim::CommandType::RemoveRoad,{8,2}}).accepted &&
        world.courier_position(id)==position && world.courier(id).cargo==before.cargo &&
        world.courier(id).reserved==before.reserved,"Cut teleported courier or lost shipment");
    for (int i=0;i<world.active_rules().courier_edge_ticks-before.edge_progress;++i) {
        world.tick();loaded.tick();invariants(world);
        check(world.snapshot()==loaded.snapshot(),"Begun original edge restore diverged");
    }
    check(world.courier_position(id)==sim::Position{5.0,2.0},
        "Removed future attachment interrupted the already begun original edge");
    world.tick();loaded.tick();const auto waiting=world.snapshot();
    check(world.courier(id).route_pending && world.courier(id).edge_progress==0 &&
        world.original_road(world.courier(id).path.front()),"Cut did not wait at actual original road position");
    auto broken=sim::World::restore(waiting,p);const auto attempts=world.courier(id).reroute_attempts;
    for (int i=0;i<20;++i) { world.tick();broken.tick();invariants(world);
        check(world.snapshot()==broken.snapshot(),"Interrupted original road restore diverged"); }
    check(world.courier(id).reroute_attempts==attempts && world.courier(id).cargo==before.cargo &&
        world.building(target).reserved_incoming==before.reserved,
        "Unchanged revision retried routing or lost original-road cargo/reservation");
    check(world.execute({sim::CommandType::PlaceRoad,{8,2}}).changed &&
        broken.execute({sim::CommandType::PlaceRoad,{8,2}}).changed,"Legal attachment rebuild failed");
    bool arrived=false,returned=false;
    for (int i=0;i<180 && !returned;++i) {
        const auto phase=world.courier(id).phase;world.tick();broken.tick();invariants(world);
        check(world.snapshot()==broken.snapshot(),"Restored original-road repair continuation diverged");
        arrived|=phase==sim::CourierPhase::ToWarehouse && world.courier(id).phase==sim::CourierPhase::Returning;
        returned|=arrived && world.courier(id).phase==sim::CourierPhase::IdleAtWorkshop;
    }
    check(arrived && returned && world.building(target).input_clay>0 &&
        world.building(target).reserved_incoming==0,"Original-road interrupted shipment never delivered/returned");
    // The immutable topology is supplied before caches are reconstructed.
    auto wrong=permission;wrong[ix({4,2})].original_road=false;
    wrong[ix({4,2})].original_road_classification=sim::OriginalRoadClassification::None;
    rejects([&] { auto invalid=sim::World::restore(active,policy(wrong)); (void)invalid; },
        "Active restore invented a missing original transport cell");
}
void current_role_routes() {
    struct Case {sim::CommandType owner,target;};
    constexpr std::array cases{
        Case{sim::CommandType::PlaceClaySource,sim::CommandType::PlacePottery},
        Case{sim::CommandType::PlacePottery,sim::CommandType::PlaceWarehouse},
        Case{sim::CommandType::PlaceWarehouse,sim::CommandType::PlaceMarket},
        Case{sim::CommandType::PlaceFarm,sim::CommandType::PlaceMarket},
        Case{sim::CommandType::PlaceServicePost,sim::CommandType::PlaceHousehold},
        Case{sim::CommandType::PlaceMarket,sim::CommandType::PlaceHousehold},
        Case{sim::CommandType::PlaceFireWatch,sim::CommandType::PlacePottery},
        Case{sim::CommandType::PlaceHealthPost,sim::CommandType::PlaceHousehold}};
    std::array<bool,12> seen{};
    for (const auto& value:cases) {
        auto permission=cells();original(permission,{2,4});sim::World world(policy(permission));
        const auto source=place(world,value.owner,{2,5});
        const auto target_kind=*sim::placed_object(value.target);
        const auto footprint=sim::building_footprint(sim::RulesProfile::CityV16,4,target_kind);
        const auto target=place(world,value.target,{2,4-footprint.height});
        for (const auto& courier:world.couriers()) if (courier.owner==source) {
            seen[static_cast<std::size_t>(courier.role)]=true;
            check(sim::courier_can_target(courier.role,world.building(target)) &&
                world.find_building_route(source,target).has_value(),
                "A current courier role cannot use the shared original-road entrance/path contract");
            const auto route=std::find_if(courier.dynamic_target_routes.begin(),courier.dynamic_target_routes.end(),
                [&](const auto& entry) {return entry.first==target;});
            check(route!=courier.dynamic_target_routes.end() && route->second==world.find_building_route(source,target),
                "Current courier role cached a different road authority");
        }
    }
    for (const auto role:{sim::CourierRole::Clay,sim::CourierRole::Pottery,sim::CourierRole::Service,
        sim::CourierRole::MarketPotteryInbound,sim::CourierRole::MarketFoodInbound,
        sim::CourierRole::MarketPotteryDistribution,sim::CourierRole::MarketFoodDistribution,
        sim::CourierRole::FireInspector,sim::CourierRole::HealthWorker})
        check(seen[static_cast<std::size_t>(role)],"Current City-v16 courier role not covered");
}
}
int main() {
    try {
        version_contract();shared_road_and_user_geometry();commands_preview_batch_and_gate();
        heights_missing_and_policy_revision();interrupted_original_trip();current_role_routes();
        std::cout<<"Original-road transport contracts passed\n";return 0;
    }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
