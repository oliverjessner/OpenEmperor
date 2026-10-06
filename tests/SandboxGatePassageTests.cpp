#include "simulation/World.h"
#include "core/PerformanceDiagnostics.h"

#include <algorithm>
#include <iostream>
#include <locale>
#include <limits>
#include <stdexcept>

namespace sim=openemperor::simulation;
namespace perf=openemperor::performance;
namespace {
void check(bool ok,const char* reason) { if (!ok)throw std::runtime_error(reason); }
template<class F> void rejects(F action,const char* reason) {
    try {action();}catch(const std::invalid_argument&){return;}
    throw std::runtime_error(reason);
}
constexpr int size=24;
std::size_t ix(sim::Cell p) {return static_cast<std::size_t>(p.y*size+p.x);}
sim::Cell axis(sim::Cell p,bool horizontal) {return horizontal ? sim::Cell{p.y,p.x}:p;}
std::vector<sim::MapCellPermission> free_cells() {
    return std::vector<sim::MapCellPermission>(size*size,{true,true,false,0,
        sim::BuildBlocker::None,sim::BuildBlocker::None});
}
sim::FixedGatePassage gate(bool horizontal,sim::FixedGateId id={101}) {
    sim::FixedGatePassage g;g.id=id;
    for(int y=7;y<=9;++y)for(int x=5;x<=9;++x)g.protected_footprint.push_back(axis({x,y},horizontal));
    for(int y=7;y<=9;++y)g.corridor.push_back(axis({7,y},horizontal));
    g.openings={axis({7,6},horizontal),axis({7,10},horizontal)};
    return g;
}
void protect(std::vector<sim::MapCellPermission>& cells,const sim::FixedGatePassage& g) {
    for(const auto p:g.protected_footprint)cells[ix(p)]={false,false,true,0,
        sim::BuildBlocker::GateSolidPart,sim::BuildBlocker::OriginalStructure};
    for(const auto p:g.corridor)cells[ix(p)].road_allowed=true;
}
auto policy(bool horizontal=false) {
    auto cells=free_cells();const auto g=gate(horizontal);protect(cells,g);
    // A complete wall stripe makes the passage the only north/south route.
    for(int x=0;x<size;++x)if(x<5 || x>9)cells[ix(axis({x,8},horizontal))]=
        {false,false,true,0,sim::BuildBlocker::OriginalStructure,sim::BuildBlocker::OriginalStructure};
    return std::make_shared<const sim::MapPermissions>(size,size,sim::kMapPermissionsPolicyVersion,
        std::move(cells),std::vector<sim::FixedGatePassage>{g});
}
sim::BuildingId place(sim::World& w,sim::CommandType type,sim::Cell p) {
    const auto r=w.execute({type,p});
    if(!r.accepted || !r.changed)throw std::runtime_error("fixture placement: "+r.reason);
    return *w.building_owner_at(p);
}
sim::CourierId owned_courier(const sim::World& w,sim::BuildingId owner) {
    for(const auto& c:w.couriers())if(c.owner==owner)return c.id;
    throw std::runtime_error("missing fixture courier");
}
void invariants(const sim::World& w) {
    check(w.navigation_valid() && w.production_balance_valid() && w.food_balance_valid() &&
        w.city_economy_valid() && w.population_valid() && w.service_state_valid() &&
        w.fire_state_valid() && w.health_state_valid(),"passage simulation invariant");
}
template<class P> void until(sim::World& w,P done,int limit=2000) {
    while(limit-- && !done()){w.tick();invariants(w);}
    check(done(),"courier wait timed out");
}
void contract_cases() {
    const auto p=policy();check(p->gates().size()==1 && p->fixed_gate_at({7,8})==sim::FixedGateId{101} &&
        p->fixed_gate({7,8})==&p->gates()[0] && !p->fixed_gate({-1,0}) && !p->fixed_gate({0,0}),"typed fixed gate identity/indexed lookup");
    for(const auto cell:p->gates()[0].protected_footprint)
        check(p->protected_original(cell) && !p->building_allowed(cell),"full protected gate footprint");
    check(p->transport_edge_allowed({7,6},{7,7}) && p->transport_edge_allowed({7,7},{7,6}) &&
        p->transport_edge_allowed({7,7},{7,8}) && p->transport_edge_allowed({7,9},{7,10}),"exact bidirectional corridor/openings");
    check(p->transport_edge_blocker({7,8},{6,8})==sim::BuildBlocker::GateSideEntry &&
        p->transport_edge_blocker({7,6},{7,9})==sim::BuildBlocker::InvalidEdge,"side/teleport edge rejected");
    auto cells=p->cells();cells[ix({7,10})].height=1;
    sim::MapPermissions uneven(size,size,1,cells,p->gates());
    check(uneven.transport_edge_blocker({7,9},{7,10})==sim::BuildBlocker::UnsupportedHeightTransition,"signed height transition rejection");
    cells=p->cells();cells[ix({2,2})].height=-2;
    sim::MapPermissions signed_height(size,size,1,cells,p->gates());
    check(signed_height.cell_height({2,2})==-2 && signed_height.canonical_state()!=p->canonical_state(),"signed height canonical fact");
    auto g=p->gates()[0];std::reverse(g.protected_footprint.begin(),g.protected_footprint.end());
    std::reverse(g.corridor.begin(),g.corridor.end());std::swap(g.openings[0],g.openings[1]);
    sim::MapPermissions reordered(size,size,1,p->cells(),{g});
    check(reordered.canonical_state()==p->canonical_state(),"canonical ordered topology independent of input order");
    struct Grouping:std::numpunct<char>{char do_thousands_sep()const override{return '_';}std::string do_grouping()const override{return "\1";}};
    const auto old=std::locale();std::locale::global(std::locale(old,new Grouping));
    const auto localized=p->canonical_state();std::locale::global(old);
    check(localized==p->canonical_state(),"canonical bytes independent of process locale");
    rejects([&]{sim::MapPermissions bad(size,size,2,p->cells(),p->gates());},"unknown policy accepted");
    g=p->gates()[0];g.id={0};rejects([&]{sim::MapPermissions bad(size,size,1,p->cells(),{g});},"zero gate ID accepted");
    g=p->gates()[0];g.protected_footprint.pop_back();rejects([&]{sim::MapPermissions bad(size,size,1,p->cells(),{g});},"partial protected footprint accepted");
    g=p->gates()[0];g.protected_footprint.front()={std::numeric_limits<int>::min(),std::numeric_limits<int>::min()};
    g.protected_footprint.back()={std::numeric_limits<int>::max(),std::numeric_limits<int>::max()};
    rejects([&]{sim::MapPermissions bad(size,size,1,p->cells(),{g});},"extreme footprint coordinates accepted");
    g=p->gates()[0];g.corridor[1]={6,8};rejects([&]{sim::MapPermissions bad(size,size,1,p->cells(),{g});},"side corridor accepted");
    rejects([&]{sim::MapPermissions bad(size,size,1,p->cells(),{p->gates()[0],p->gates()[0]});},"duplicate/conflicting gate accepted");
    // Two complete, disjoint gates preserve identities and sort canonical rows.
    auto two=p->gates();auto other=gate(false,{202});
    for(auto& q:other.protected_footprint){q.x+=10;q.y+=5;}for(auto& q:other.corridor){q.x+=10;q.y+=5;}
    for(auto& q:other.openings){q.x+=10;q.y+=5;}two.push_back(other);cells=p->cells();protect(cells,other);
    sim::MapPermissions pair(size,size,1,cells,two);std::reverse(two.begin(),two.end());
    sim::MapPermissions pair_reverse(size,size,1,cells,two);
    check(pair.canonical_state()==pair_reverse.canonical_state() && pair.fixed_gate_at({17,13})==sim::FixedGateId{202},"independent sorted gate identities");
    rejects([&]{sim::World bad(p,sim::RulesProfile::CityV16,2);},"new topology injected into old rule");
    sim::World authored(size,size,std::vector<std::uint8_t>(size*size,1),sim::RulesProfile::CityV16);
    check(authored.rule_version()==3 && authored.map_permissions() && authored.map_permissions()->gates().empty(),"explicit authored simple policy/default rule3");
    for(const auto k:{sim::Object::Well,sim::Object::HealthPost})
        check(sim::building_footprint(sim::RulesProfile::CityV16,3,k)==sim::BuildingFootprint{2,2},"rule3 retains safety footprints");
    const auto& r2=sim::profile_rules(sim::RulesProfile::CityV16,2);const auto& r3=authored.active_rules();
    check(r2.clay_ticks==r3.clay_ticks && r2.pottery_recipe_ticks==r3.pottery_recipe_ticks &&
        r2.farm_ticks==r3.farm_ticks && r2.courier_edge_ticks==r3.courier_edge_ticks &&
        r2.household_move_in_grace_ticks==r3.household_move_in_grace_ticks,"rule3 inherits economy timings");
}
void command_cases() {
    const auto permissions=policy();sim::World w(permissions);const auto initial=w.snapshot();
    for(const auto cell:permissions->gates()[0].protected_footprint)
        check(!w.validate({sim::CommandType::PlaceFarm,cell}).accepted,"building on protected gate accepted");
    const auto noop=w.execute({sim::CommandType::PlaceRoad,{7,8}});
    check(noop.accepted && !noop.changed && w.command_sequence()==initial.command_sequence+1 &&
        w.object_at({7,8})==sim::Object::Empty && w.treasury()==initial.treasury &&
        w.roads_placed_total()==0 && w.road_revision()==0,"fixed passage command no-op semantics");
    const auto removal=w.execute({sim::CommandType::RemoveRoad,{7,8}});
    check(!removal.accepted && removal.reason=="Original gate passage cannot be removed." &&
        removal.diagnostic.blocker==sim::BuildBlocker::ProtectedGatePassage,"fixed passage removal reason");
    const std::vector<sim::Cell> mixed{{7,5},{7,6},{7,7},{7,8},{7,9},{7,10},{7,11}};
    const auto before=w.snapshot();const auto canonical=w.canonical_state();
    perf::set_enabled(true);perf::reset();
    const auto preview=w.validate_road_batch(mixed);
    check(preview.accepted && preview.new_road_count==4 && preview.total_cost==4*sim::Rules::road_cost &&
        preview.cell_diagnostics.size()==mixed.size() && w.snapshot()==before && w.canonical_state()==canonical,"pure mixed batch cost/diagnostics");
    for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,perf::Counter::WorldExecutes,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::FileReads,perf::Counter::AssetDecodes,perf::Counter::TextureUploads})
        check(perf::counter(counter)==0,"preview did work beyond pure validation");
    check(w.execute_road_batch(mixed).accepted && perf::counter(perf::Counter::WorldCopies)==1 &&
        perf::counter(perf::Counter::RouteRefreshes)==1 && w.command_sequence()==before.command_sequence+4 &&
        w.roads_placed_total()==4 && w.road_revision()==4 && w.treasury()==before.treasury-preview.total_cost,"atomic mixed batch commit counts");
    for(const auto p:gate(false).corridor)check(w.object_at(p)==sim::Object::Empty,"batch created Sandbox road inside gate");
    const auto built=w.snapshot();perf::reset();check(w.execute_road_batch(mixed).accepted && w.snapshot()==built &&
        perf::counter(perf::Counter::WorldCopies)==0 && perf::counter(perf::Counter::RouteRefreshes)==0,"all-existing/fixed batch changed World");
    perf::set_enabled(false);
    const std::vector<sim::Cell> blocked{{7,5},{7,6},{7,7},{6,7},{5,7}};
    const auto failure=w.validate_road_batch(blocked);
    check(!failure.accepted && failure.diagnostic.cell==sim::Cell{6,7} &&
        failure.diagnostic.blocker==sim::BuildBlocker::GateSolidPart && failure.additional_blockers.size()==1 &&
        failure.cell_diagnostics[0].allowed && failure.cell_diagnostics[1].allowed && failure.cell_diagnostics[2].allowed &&
        !failure.cell_diagnostics[3].allowed && !failure.cell_diagnostics[4].allowed,"first actual cell blocker and safe partial diagnostics");
    check(!w.execute_road_batch(blocked).accepted && w.snapshot()==built,"blocked batch published partial changes");
    const std::vector<sim::Cell> skip{{7,6},{7,9}};const auto edge=w.validate_road_batch(skip);
    check(!edge.accepted && edge.diagnostic.edge==sim::TransportEdge{{7,6},{7,9}} &&
        edge.cell_diagnostics[0].allowed && edge.cell_diagnostics[1].allowed,"edge blocker distinct from allowed cells");
    auto cells=policy()->cells();cells[ix({7,10})].height=-1;
    sim::World heights(std::make_shared<const sim::MapPermissions>(size,size,1,cells,policy()->gates()));
    const auto hf=heights.validate_road_batch(mixed);
    check(!hf.accepted && hf.diagnostic.blocker==sim::BuildBlocker::UnsupportedHeightTransition &&
        hf.diagnostic.edge==sim::TransportEdge{{7,9},{7,10}},"height edge has exact diagnostic");
    cells=free_cells();cells[ix({1,1})].road_allowed=false;cells[ix({1,1})].road_blocker=sim::BuildBlocker::UnsupportedTerrain;
    cells[ix({2,1})].road_allowed=false;cells[ix({2,1})].protected_original=true;cells[ix({2,1})].building_allowed=false;
    cells[ix({2,1})].road_blocker=sim::BuildBlocker::OriginalStructure;
    sim::World terrain(std::make_shared<const sim::MapPermissions>(size,size,1,cells,std::vector<sim::FixedGatePassage>{}));
    check(terrain.validate({sim::CommandType::PlaceRoad,{-1,0}}).diagnostic.blocker==sim::BuildBlocker::OutsideMap &&
        terrain.validate({sim::CommandType::PlaceRoad,{1,1}}).diagnostic.blocker==sim::BuildBlocker::UnsupportedTerrain &&
        terrain.validate({sim::CommandType::PlaceRoad,{2,1}}).diagnostic.blocker==sim::BuildBlocker::OriginalStructure,"separate road permission categories");
    auto bad=w.snapshot();bad.roads.push_back({7,8});
    rejects([&]{auto restored=sim::World::restore(bad,w.map_permissions());(void)restored;},"saved road authority inside gate accepted");
    // Equal cell permissions do not permit an uphill road-to-building edge.
    cells=free_cells();cells[ix({4,2})].height=1;
    sim::World entrance_height(std::make_shared<const sim::MapPermissions>(size,size,1,cells,std::vector<sim::FixedGatePassage>{}));
    const auto industry=place(entrance_height,sim::CommandType::PlaceClaySource,{2,2});
    check(entrance_height.execute({sim::CommandType::PlaceRoad,{4,2}}).accepted &&
        entrance_height.building_entrances(industry).empty(),"unsupported road-to-building height admitted an entrance");
    // A building at an opening never gets a direct entrance from the passage.
    sim::World direct(permissions);const auto farm=place(direct,sim::CommandType::PlaceFarm,{7,6});
    check(direct.building_entrances(farm).empty() && !direct.transport_edge_allowed({7,6},{7,7}),"direct gate-to-building entrance");
    for(const auto version:{1U,2U}) {
        sim::World legacy(size,size,permissions->building_mask(),sim::RulesProfile::CityV16,version);
        check(!legacy.map_permissions() && !legacy.fixed_passage({7,8}) &&
            !legacy.execute({sim::CommandType::PlaceRoad,{7,8}}).accepted,"legacy mask silently acquired fixed gate topology");
        auto clone=sim::World::restore(legacy.snapshot(),permissions->building_mask());
        check(clone.snapshot()==legacy.snapshot() && clone.canonical_state()==legacy.canonical_state(),"legacy canonical/restore changed");
    }
    std::vector<sim::Cell> long_road;for(int x=0;x<size;++x)long_road.push_back({x,23});
    for(int y=22;y>=0;--y)long_road.push_back({23,y});
    const auto spend=[](sim::World& world) {
        for(int y:{0,2})for(int x=0;x<16;x+=2)place(world,sim::CommandType::PlaceHousehold,{x,y});
    };
    sim::World poor(size,size,std::vector<std::uint8_t>(size*size,1),sim::RulesProfile::CityV16);spend(poor);
    const auto poverty=poor.snapshot();const auto money=poor.validate_road_batch(long_road);
    check(!money.accepted && money.diagnostic.blocker==sim::BuildBlocker::InsufficientFunds &&
        money.new_road_count==long_road.size() &&
        money.total_cost==static_cast<std::int64_t>(long_road.size())*sim::Rules::road_cost &&
        std::all_of(money.cell_diagnostics.begin(),money.cell_diagnostics.end(),[](const auto& d){return d.allowed;}),"funds misclassified allowed terrain cells");
    check(!poor.execute_road_batch(long_road).accepted && poor.snapshot()==poverty,"insufficient batch changed World");
    cells=free_cells();cells[ix({23,12})].road_allowed=false;cells[ix({23,12})].road_blocker=sim::BuildBlocker::UnsupportedTerrain;
    sim::World poor_terrain(std::make_shared<const sim::MapPermissions>(size,size,1,cells,std::vector<sim::FixedGatePassage>{}));spend(poor_terrain);
    const auto terrain_and_money=poor_terrain.validate_road_batch(long_road);
    check(!terrain_and_money.accepted && terrain_and_money.diagnostic.blocker==sim::BuildBlocker::UnsupportedTerrain &&
        terrain_and_money.diagnostic.cell==sim::Cell{23,12},"funds hid actual terrain blocker");
    cells=free_cells();for(int x=0;x<size;++x)cells[ix({x,23})].road_allowed=false;
    sim::World many(std::make_shared<const sim::MapPermissions>(size,size,1,cells,std::vector<sim::FixedGatePassage>{}));
    const auto bounded=many.validate_road_batch(long_road);
    check(!bounded.accepted && bounded.additional_blockers.size()==16 && bounded.cell_diagnostics.size()==long_road.size(),"additional blocker budget");
}
void courier_cases(bool horizontal,bool reverse) {
    const auto permissions=policy(horizontal);sim::World w(permissions);
    for(const auto p:{sim::Cell{0,0},sim::Cell{3,0},sim::Cell{10,0},sim::Cell{13,0}})
        place(w,sim::CommandType::PlaceHousehold,axis(p,horizontal));
    const auto source=place(w,sim::CommandType::PlaceClaySource,axis(reverse ? sim::Cell{7,12}:sim::Cell{3,3},horizontal));
    const auto target=place(w,sim::CommandType::PlacePottery,axis(reverse ? sim::Cell{3,3}:sim::Cell{7,12},horizontal));
    const std::vector<sim::Cell> north_south{{4,5},{5,5},{6,5},{7,5},{7,6},{7,7},{7,8},{7,9},{7,10},{7,11}};
    std::vector<sim::Cell> road;for(auto p:north_south)road.push_back(axis(p,horizontal));
    check(w.execute_road_batch(road).accepted,"paid through-gate connection");
    const auto id=owned_courier(w,source);const auto route=w.find_building_route(source,target);
    check(route && route->size()>=7,"no building route through gate");
    const auto corridor=gate(horizontal).corridor;
    for(const auto p:corridor)check(std::count(route->begin(),route->end(),p)==1,"route skipped/doubled gate cell");
    for(std::size_t i=1;i<route->size();++i)check(std::abs((*route)[i].x-(*route)[i-1].x)+
        std::abs((*route)[i].y-(*route)[i-1].y)==1,"route teleported gate");
    until(w,[&]{return w.courier(id).phase==sim::CourierPhase::ToWarehouse;});
    auto continuing=sim::World::restore(w.snapshot(),permissions);
    check(continuing.snapshot()==w.snapshot() && continuing.map_permissions()==permissions,"outside-gate restore loses policy");
    const auto started=w.ticks();const auto edges=w.courier(id).path.size()-1;
    const auto progress=w.courier(id).edge_progress;
    for(std::size_t tick=0;tick<edges*static_cast<std::size_t>(w.active_rules().courier_edge_ticks)-static_cast<std::size_t>(progress);++tick){
        w.tick();continuing.tick();check(w.snapshot()==continuing.snapshot(),"outside restore continuation diverged");invariants(w);
    }
    check(w.courier(id).phase==sim::CourierPhase::Returning && w.building(target).input_clay>0 &&
        w.ticks()-started==edges*static_cast<std::size_t>(w.active_rules().courier_edge_ticks)-static_cast<std::size_t>(progress),"gate delivery has ordinary edge time");
    until(w,[&]{return w.courier(id).phase==sim::CourierPhase::IdleAtWorkshop;});
    until(w,[&]{const auto& c=w.courier(id);return c.phase==sim::CourierPhase::ToWarehouse &&
        c.path[c.path_vertex]==(reverse ? corridor.back():corridor.front()) && c.edge_progress==1;});
    const auto inside=w.snapshot();auto restored=sim::World::restore(inside,permissions);
    check(restored.snapshot()==inside,"inside-gate restore mismatch");
    const auto cut=axis(reverse ? sim::Cell{7,6}:sim::Cell{7,10},horizontal);
    const auto courier_before=w.courier(id);const auto waiting_start=w.courier_position(id);
    check(w.execute({sim::CommandType::RemoveRoad,cut}).accepted &&
        restored.execute({sim::CommandType::RemoveRoad,cut}).accepted,"future outside opening cannot be removed");
    check(w.courier_position(id)==waiting_start && w.courier(id).cargo==courier_before.cargo &&
        w.courier(id).reserved==courier_before.reserved,"cut moved gate courier or lost shipment");
    const int remaining=w.active_rules().courier_edge_ticks-courier_before.edge_progress;
    for(int i=0;i<remaining;++i){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"begun gate edge continuation differs");invariants(w);}
    check(w.courier_position(id)==sim::Position{static_cast<double>(corridor[1].x),static_cast<double>(corridor[1].y)},"cut interrupted begun gate edge");
    w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"cut continuation differs");
    const auto& waiting=w.courier(id);check(waiting.route_pending && waiting.edge_progress==0 &&
        w.fixed_passage(waiting.path.front()),"courier did not wait inside fixed passage");
    auto broken=sim::World::restore(w.snapshot(),permissions);const auto attempts=waiting.reroute_attempts;
    for(int i=0;i<25;++i){w.tick();broken.tick();check(w.snapshot()==broken.snapshot(),"broken gate restore diverged");invariants(w);}
    check(w.courier(id).reroute_attempts==attempts && w.courier(id).cargo==courier_before.cargo &&
        w.building(target).reserved_incoming==courier_before.reserved,"waiting gate courier reran BFS or lost reservation");
    check(w.execute({sim::CommandType::PlaceRoad,cut}).accepted &&
        broken.execute({sim::CommandType::PlaceRoad,cut}).accepted,"opening repair failed");
    const auto paired_tick=[&] {
        auto checkpoint=sim::World::restore(w.snapshot(),permissions);
        w.tick();broken.tick();checkpoint.tick();
        check(w.snapshot()==broken.snapshot() && w.snapshot()==checkpoint.snapshot(),"resumed route checkpoint continuation");
        invariants(w);
    };
    // Reroute again from an outside road, then remove that historical origin
    // behind the courier. Only current/future roads must remain in the graph.
    int budget=100;
    while(budget-- && !(w.courier(id).path[w.courier(id).path_vertex]==cut &&
        w.courier(id).edge_progress==0))paired_tick();
    check(budget>=0,"resumed courier did not reach opening");
    const auto next=w.courier(id).path[w.courier(id).path_vertex+1];
    check(w.object_at(next)==sim::Object::Road &&
        w.execute({sim::CommandType::RemoveRoad,next}).accepted &&
        broken.execute({sim::CommandType::RemoveRoad,next}).accepted &&
        w.execute({sim::CommandType::PlaceRoad,next}).accepted &&
        broken.execute({sim::CommandType::PlaceRoad,next}).accepted,"outside road reroute preparation");
    paired_tick();check(w.courier(id).path.front()==cut,"reroute did not retain actual Road origin");
    budget=100;
    while(budget-- && !(w.courier(id).path[w.courier(id).path_vertex]==next &&
        w.courier(id).edge_progress==0))paired_tick();
    check(budget>=0 && w.execute({sim::CommandType::RemoveRoad,cut}).accepted &&
        broken.execute({sim::CommandType::RemoveRoad,cut}).accepted,"historical reroute origin removal");
    check(w.object_at(w.courier(id).path.front())==sim::Object::Empty &&
        w.courier(id).path_vertex>0 && !w.courier(id).route_pending,"test lost removed historical Road origin");
    auto historical=sim::World::restore(w.snapshot(),permissions);
    check(historical.snapshot()==w.snapshot(),"historical Road-origin checkpoint refused");
    bool return_waited=false;
    bool arrived=false,returned=false;
    for(int i=0;i<250;++i){const auto before=w.courier(id).phase;
        auto checkpoint=sim::World::restore(w.snapshot(),permissions);
        w.tick();broken.tick();checkpoint.tick();
        check(w.snapshot()==checkpoint.snapshot(),"post-repair/exit/return checkpoint continuation diverged");
        check(w.snapshot()==broken.snapshot(),"repaired gate continuation diverged");invariants(w);
        arrived=arrived || (before==sim::CourierPhase::ToWarehouse && w.courier(id).phase==sim::CourierPhase::Returning);
        if(arrived && w.courier(id).route_pending && !return_waited) {
            check(w.courier(id).cargo==0 && w.courier(id).reserved==0 &&
                sim::building_footprint_contains(w.profile(),w.rule_version(),w.building(target).kind,
                    w.building(target).cell,w.courier(id).path.front()),"return wait did not remain at delivery building");
            return_waited=true;
            check(w.execute({sim::CommandType::PlaceRoad,cut}).accepted &&
                broken.execute({sim::CommandType::PlaceRoad,cut}).accepted,"return connection repair");
        }
        returned=returned || (arrived && w.courier(id).phase==sim::CourierPhase::IdleAtWorkshop);
    }
    check(arrived && return_waited && returned,"gate courier did not wait/deliver/return after repairs");
    const auto stable=w.snapshot();auto forged=stable;
    if(!forged.couriers.empty())forged.couriers[0].path={{6,7},{7,7}};
    rejects([&]{w.import_snapshot(forged);},"forged side route import accepted");
    check(w.snapshot()==stable && w.map_permissions()==permissions,"failed import changed World/topology");
}
} // namespace
int main() {
    try {contract_cases();command_cases();for(bool horizontal:{false,true})for(bool reverse:{false,true})courier_cases(horizontal,reverse);
        std::cout<<"immutable permissions, protected gates, pure atomic roads, both axes/directions, paid courier delivery/wait/repair/restore passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
