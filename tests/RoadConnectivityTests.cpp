#include "simulation/RoadConnectivity.h"
#include "persistence/SandboxSave.h"
#include "core/PerformanceDiagnostics.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace perf=openemperor::performance;
namespace {
constexpr int width=20,height=18;
using Category=sim::RoadConnectivityCategory;
void check(bool ok,const char* reason) { if (!ok) throw std::runtime_error(reason); }
std::size_t ix(sim::Cell p) { return static_cast<std::size_t>(p.y*width+p.x); }
auto free_cells() {
    return std::vector<sim::MapCellPermission>(width*height,
        {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
}
auto policy(std::vector<sim::MapCellPermission> cells=free_cells(),
            std::vector<sim::FixedGatePassage> gates={}) {
    return std::make_shared<const sim::MapPermissions>(width,height,1,std::move(cells),std::move(gates));
}
sim::BuildingId place(sim::World& world,sim::CommandType type,sim::Cell cell) {
    const auto result=world.execute({type,cell});
    if (!result.accepted || !result.changed) throw std::runtime_error("Paid placement: "+result.reason);
    return *world.building_owner_at(cell);
}
void road(sim::World& world,sim::Cell cell) {
    check(world.execute({sim::CommandType::PlaceRoad,cell}).accepted,"Paid road failed");
}
void houses(sim::World& world) {
    place(world,sim::CommandType::PlaceHousehold,{1,12});
    place(world,sim::CommandType::PlaceHousehold,{4,12});
}
struct City {
    sim::World world;
    sim::BuildingId clay,pottery;
    explicit City(std::shared_ptr<const sim::MapPermissions> permissions=policy(),bool staffed=true)
        :world(std::move(permissions)),
         clay(place(world,sim::CommandType::PlaceClaySource,{2,2})),
         pottery(place(world,sim::CommandType::PlacePottery,{8,2})) {
        if (staffed) houses(world);
    }
    void connected() { for (int x=4;x<=7;++x) road(world,{x,2}); }
};
sim::CourierId owned(const sim::World& world,sim::BuildingId building) {
    for (const auto& courier:world.couriers()) if (courier.owner==building) return courier.id;
    throw std::runtime_error("Missing owned courier");
}
auto caches(const sim::World& world) {
    using Entry=std::tuple<sim::CourierId,std::uint64_t,std::optional<std::vector<sim::Cell>>,
        std::array<std::optional<std::vector<sim::Cell>>,sim::max_buildings>,
        std::vector<std::pair<sim::BuildingId,std::optional<std::vector<sim::Cell>>>>>;
    std::vector<Entry> result;
    for (const auto& courier:world.couriers()) result.emplace_back(courier.id,courier.cached_revision,
        courier.cached_route,courier.target_routes,courier.dynamic_target_routes);
    return result;
}
sim::RoadConnectivityReport diagnose(const sim::World& world,sim::BuildingId source,sim::BuildingId target) {
    const auto snapshot=world.snapshot();const auto canonical=world.canonical_state();
    const auto cache=caches(world);const auto refreshes=world.route_refresh_count();
    const auto permissions=world.map_permissions();const auto permission_state=permissions->canonical_state();
    perf::set_enabled(true);perf::reset();
    const auto report=sim::diagnose_road_connectivity(world,source,target);
    check(world.snapshot()==snapshot && world.canonical_state()==canonical && caches(world)==cache &&
          world.route_refresh_count()==refreshes && world.map_permissions()==permissions &&
          permissions->canonical_state()==permission_state,"Diagnostic changed World/cache/permissions");
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,
        perf::Counter::WorldExecutes,perf::Counter::RouteRefreshes,perf::Counter::FileReads,
        perf::Counter::FileWrites,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::SimulationTicks}) check(perf::counter(counter)==0,"Read-only diagnosis performed mutation/I/O");
    check(report.cells_scanned<=sim::road_connectivity_cell_limit &&
          report.component_edge_checks<=4*report.transport_cells &&
          report.boundary_edge_checks<=4*report.transport_cells &&
          report.blocked_edges.size()<=sim::road_connectivity_edge_limit &&
          report.authoritative_route_queries<=1,"Diagnostic work/display bound exceeded");
    const auto entrance_pairs=report.source.entrances.size()*report.target.entrances.size();
    check(perf::counter(perf::Counter::BfsCalls)<=entrance_pairs &&
          perf::counter(perf::Counter::BfsVisitedCells)<=entrance_pairs*
              static_cast<std::size_t>(world.width())*static_cast<std::size_t>(world.height()),
          "Authoritative route query exceeded bounded entrance-pair/grid work");
    perf::set_enabled(false);
    return report;
}
bool has_edge(const sim::RoadConnectivityReport& report,sim::Cell from,sim::Cell to,Category category) {
    return std::any_of(report.blocked_edges.begin(),report.blocked_edges.end(),[&](const auto& edge) {
        return edge.from.cell==from && edge.to.cell==to && edge.category==category;
    });
}
void a_connected_and_l_no_stock() {
    City city;city.connected();const auto report=diagnose(city.world,city.clay,city.pottery);
    check(report.category==Category::Connected && report.route==city.world.find_building_route(city.clay,city.pottery),
          "A: connected report must use exact authoritative route");
    check(report.source.footprint==std::vector<sim::Cell>{{2,2},{3,2},{2,3},{3,3}} &&
          report.source.entrances==std::vector<sim::BuildingEntrance>{{{4,2},{3,2}}} &&
          report.target.entrances==std::vector<sim::BuildingEntrance>{{{7,2},{8,2}}} &&
          report.components.size()==1 && report.components.front().cell_count==4 &&
          report.source_components==report.target_components,"A: whole footprint entrances/components wrong");
    check(report.couriers.size()==1 && report.couriers[0].workers_assigned==4 &&
          report.couriers[0].dispatch_status==sim::CourierDispatchStatus::NoStock &&
          city.world.courier_dispatch_status(owned(city.world,city.clay)).status==sim::CourierDispatchStatus::NoStock,
          "L: valid road and NoStock dispatch must remain separate");
    check(diagnose(city.world,city.clay,city.pottery)==report,"A: repeated diagnosis is not deterministic");
    check(sim::road_connectivity_targets(city.world,owned(city.world,city.clay))==
          std::vector<sim::BuildingId>{city.pottery},"A: role-compatible targets differ from World contract");
}
void b_c_missing_entrances() {
    City source;road(source.world,{7,2});
    const auto no_source=diagnose(source.world,source.clay,source.pottery);
    check(no_source.category==Category::SourceNoEntrance && no_source.source.entrances.empty() &&
          no_source.target.entrances.size()==1 && !no_source.source.candidate_entrances.empty(),
          "B: source no entrance misclassified");
    City target;road(target.world,{4,2});
    const auto no_target=diagnose(target.world,target.clay,target.pottery);
    check(no_target.category==Category::TargetNoEntrance && no_target.source.entrances.size()==1 &&
          no_target.target.entrances.empty(),"C: target no entrance misclassified");
    auto cells=free_cells();cells[ix({4,2})].height=1;
    City blocked(policy(cells));road(blocked.world,{4,2});road(blocked.world,{7,2});
    const auto height_report=diagnose(blocked.world,blocked.clay,blocked.pottery);
    check(height_report.category==Category::SourceNoEntrance &&
          has_edge(height_report,{3,2},{4,2},Category::BlockedHeightTransition),
          "B: source entrance category must retain secondary measured height rejection");
    cells=free_cells();cells[ix({7,2})].height=-2;
    City ingress(policy(cells));road(ingress.world,{4,2});road(ingress.world,{7,2});
    const auto target_height=diagnose(ingress.world,ingress.clay,ingress.pottery);
    check(target_height.category==Category::TargetNoEntrance &&
          has_edge(target_height,{7,2},{8,2},Category::BlockedHeightTransition),
          "C: road-to-target height rejection omitted");
}
void d_islands_and_o_revision() {
    City city;road(city.world,{4,2});road(city.world,{5,2});road(city.world,{7,2});
    const auto disconnected=diagnose(city.world,city.clay,city.pottery);
    check(disconnected.category==Category::DisconnectedRoadComponents && disconnected.components.size()==2 &&
          disconnected.source_components!=disconnected.target_components &&
          has_edge(disconnected,{5,2},{6,2},Category::MissingRoadCell),
          "D: same-height road islands or measured one-cell candidate absent");
    const auto candidate=std::find_if(disconnected.blocked_edges.begin(),disconnected.blocked_edges.end(),[](const auto& edge) {
        return edge.from.cell==sim::Cell{5,2} && edge.to.cell==sim::Cell{6,2};
    });
    check(candidate!=disconnected.blocked_edges.end() && candidate->candidate && candidate->place_road_allowed &&
          candidate->candidate_cell==sim::Cell{6,2},"D: candidate not validated with real World");
    const auto revision=city.world.road_revision();road(city.world,{6,2});
    const auto repaired=diagnose(city.world,city.clay,city.pottery);
    check(repaired.category==Category::Connected && repaired.road_revision==revision+1 &&
          repaired!=disconnected,"O: road revision returned stale diagnostic");
    bool outbound=false,arrived=false,returned=false;
    const auto courier=owned(city.world,city.clay);
    for (int i=0;i<150 && !returned;++i) {
        city.world.tick();const auto& state=city.world.courier(courier);
        outbound|=state.phase==sim::CourierPhase::ToWarehouse && state.cargo>0;
        arrived|=outbound && state.phase==sim::CourierPhase::Returning && state.cargo==0;
        returned|=arrived && state.phase==sim::CourierPhase::IdleAtWorkshop;
    }
    check(outbound && arrived && returned &&
          city.world.production_balance_valid() && city.world.navigation_valid(),
          "O: legal paid repair did not preserve actual dispatch/delivery/return");
}
void e_height_and_f_alternative_entrance() {
    auto cells=free_cells();
    for (const auto p:std::vector<sim::Cell>{{2,2},{3,2},{2,3},{3,3},{4,2},{5,2}}) cells[ix(p)].height=-1;
    for (const auto p:std::vector<sim::Cell>{{6,2},{7,2},{8,2},{9,2},{8,3},{9,3}}) cells[ix(p)].height=1;
    City city(policy(cells));city.connected();
    const auto report=diagnose(city.world,city.clay,city.pottery);
    check(report.category==Category::DisconnectedRoadComponents && report.components.size()==2 &&
          has_edge(report,{5,2},{6,2},Category::BlockedHeightTransition),
          "E: unequal signed Road-to-Road heights must split components");
    const auto edge=std::find_if(report.blocked_edges.begin(),report.blocked_edges.end(),[](const auto& value) {
        return value.from.cell==sim::Cell{5,2} && value.to.cell==sim::Cell{6,2};
    });
    check(edge!=report.blocked_edges.end() && edge->from.height==-1 && edge->to.height==1 && !edge->candidate &&
          edge->permission_blocker==sim::BuildBlocker::UnsupportedHeightTransition,
          "E: actual Road heights/reason changed or claimed missing cell");
    check(!city.world.validate_road_batch(std::vector<sim::Cell>{{5,2},{6,2}}).accepted,
          "E: diagnostics relaxed existing atomic road-preview policy");
    cells=free_cells();cells[ix({4,2})].height=1;City alternative(policy(cells));
    road(alternative.world,{4,2});for (int x=4;x<=7;++x) road(alternative.world,{x,3});
    const auto multiple=diagnose(alternative.world,alternative.clay,alternative.pottery);
    check(multiple.category==Category::Connected && multiple.source.entrances.size()==1 &&
          multiple.source.entrances[0].road_cell==sim::Cell{4,3} &&
          multiple.source.rejected_entrances.size()==1 &&
          multiple.source.rejected_entrances[0].category==Category::BlockedHeightTransition,
          "F: one rejected edge must not hide valid non-origin footprint entrance");
}
void g_original_and_j_road_only() {
    auto cells=free_cells();
    for (int y=0;y<height;++y) cells[ix({6,y})]={false,false,true,0,
        sim::BuildBlocker::OriginalStructure,sim::BuildBlocker::OriginalStructure};
    City city(policy(cells));road(city.world,{4,2});road(city.world,{5,2});road(city.world,{7,2});
    const auto report=diagnose(city.world,city.clay,city.pottery);
    check(!report.route && report.category==Category::DisconnectedRoadComponents &&
          has_edge(report,{5,2},{6,2},Category::BlockedOriginalStructure),
          "G: original protected stripe must not acquire an invented route");
    const auto protected_edge=std::find_if(report.blocked_edges.begin(),report.blocked_edges.end(),[](const auto& edge) {
        return edge.to.cell==sim::Cell{6,2};
    });
    check(protected_edge!=report.blocked_edges.end() && protected_edge->to.protected_original &&
          !protected_edge->place_road_allowed &&
          protected_edge->place_road_diagnostic.blocker==sim::BuildBlocker::OriginalStructure,
          "G: actual World placement blocker lost");
    cells=free_cells();cells[ix({10,10})].building_allowed=false;
    cells[ix({10,10})].building_blocker=sim::BuildBlocker::LegacyBuildabilityRestriction;
    City road_only(policy(cells));road_only.connected();
    const auto before=road_only.world.snapshot();diagnose(road_only.world,road_only.clay,road_only.pottery);
    check(road_only.world.validate({sim::CommandType::PlaceRoad,{10,10}}).accepted &&
          !road_only.world.validate({sim::CommandType::PlaceHousehold,{10,10}}).accepted &&
          road_only.world.snapshot()==before,"J: road-only permission became House-buildable");
}
auto gate_policy(bool wall=true) {
    auto cells=free_cells();sim::FixedGatePassage gate;gate.id={101};
    for (int y=7;y<=9;++y) for (int x=5;x<=9;++x) {
        gate.protected_footprint.push_back({x,y});cells[ix({x,y})]={false,false,true,0,
            sim::BuildBlocker::GateSolidPart,sim::BuildBlocker::OriginalStructure};
    }
    gate.corridor={{7,7},{7,8},{7,9}};gate.openings={sim::Cell{7,6},sim::Cell{7,10}};
    for (const auto p:gate.corridor) cells[ix(p)].road_allowed=true;
    if (wall) for (int x=0;x<width;++x) if (x<5 || x>9) cells[ix({x,8})]=
        {false,false,true,0,sim::BuildBlocker::OriginalStructure,sim::BuildBlocker::OriginalStructure};
    return policy(cells,{gate});
}
void h_gate_and_i_side() {
    sim::World world(gate_policy());
    const auto clay=place(world,sim::CommandType::PlaceClaySource,{6,3});
    const auto pottery=place(world,sim::CommandType::PlacePottery,{6,12});
    for (const auto p:std::vector<sim::Cell>{{7,5},{7,6},{7,10},{7,11}}) road(world,p);
    const auto report=diagnose(world,clay,pottery);
    check(report.category==Category::Connected && report.components.size()==1 &&
          report.components[0].cell_count==7 && std::find(report.route->begin(),report.route->end(),
          sim::Cell{7,8})!=report.route->end(),"H: fixed gate passage no longer routes");
    check(world.validate({sim::CommandType::PlaceRoad,{7,8}}).accepted &&
          !world.validate({sim::CommandType::RemoveRoad,{7,8}}).accepted,
          "H: diagnostics changed free fixed passage/protected removal");
    sim::World direct(gate_policy());
    const auto farm=place(direct,sim::CommandType::PlaceFarm,{7,6});
    const auto target=place(direct,sim::CommandType::PlacePottery,{6,12});road(direct,{7,11});
    const auto rejected=diagnose(direct,farm,target);
    check(rejected.category==Category::SourceNoEntrance &&
          has_edge(rejected,{7,6},{7,7},Category::BlockedGateEntry) &&
          !direct.transport_edge_allowed({7,6},{7,7}),
          "I: direct fixed corridor cannot replace Building-to-Road entrance");
    check(direct.map_permissions()->transport_edge_blocker({6,8},{7,8})==sim::BuildBlocker::GateSideEntry,
          "I: forbidden physical side-entry policy changed");
    // The rejected side cell is a Candidate only; it is not silently made a
    // Road. A measured frontier of the real gate component retains GateSideEntry.
    const auto side=std::find_if(report.blocked_edges.begin(),report.blocked_edges.end(),[](const auto& edge) {
        return edge.from.cell==sim::Cell{7,8} && edge.to.cell==sim::Cell{6,8};
    });
    check(side!=report.blocked_edges.end() && side->category==Category::BlockedGateEntry &&
          side->permission_blocker==sim::BuildBlocker::GateSideEntry && !side->place_road_allowed,
          "I: measured gate-side boundary blocker absent");
}
void k_unstaffed_and_m_protected_edge() {
    City unstaffed(policy(),false);unstaffed.connected();
    const auto report=diagnose(unstaffed.world,unstaffed.clay,unstaffed.pottery);
    check(report.category==Category::Connected && report.couriers.size()==1 &&
          report.couriers[0].dispatch_status==sim::CourierDispatchStatus::Unstaffed &&
          report.couriers[0].workers_assigned==0,"K: valid route must stay separate from Unstaffed");
    City active;active.connected();const auto id=owned(active.world,active.clay);
    for (int i=0;i<100;++i) {
        const auto& courier=active.world.courier(id);
        if (courier.phase==sim::CourierPhase::ToWarehouse && courier.edge_progress>0 &&
            courier.path[courier.path_vertex]==sim::Cell{4,2}) break;
        active.world.tick();
    }
    const auto& courier=active.world.courier(id);
    check(courier.phase==sim::CourierPhase::ToWarehouse && courier.edge_progress>0 &&
          courier.path[courier.path_vertex]==sim::Cell{4,2},"M: ordinary begun edge not reached");
    const auto snapshot=active.world.snapshot();const auto position=active.world.courier_position(id);
    diagnose(active.world,active.clay,active.pottery);
    check(!active.world.validate({sim::CommandType::RemoveRoad,{4,2}}).accepted &&
          !active.world.validate({sim::CommandType::RemoveRoad,{5,2}}).accepted &&
          active.world.snapshot()==snapshot && active.world.courier_position(id)==position,
          "M: diagnostic loosened begun-edge protection or moved courier");
    check(active.world.execute({sim::CommandType::RemoveRoad,{6,2}}).accepted,
          "M: future Road removal should retain normal semantics");
    const auto after=diagnose(active.world,active.clay,active.pottery);
    check(after.category==Category::DisconnectedRoadComponents && active.world.navigation_valid() &&
          active.world.courier(id).cargo==snapshot.couriers[0].cargo,
          "M: removed future edge changed cargo/position or diagnostic connectivity");
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void n_save_readonly_and_limits() {
    const auto root=std::filesystem::canonical(std::filesystem::temp_directory_path())/("openemperor-road-diagnostics-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {std::filesystem::path root;~Cleanup(){std::error_code error;std::filesystem::remove_all(root,error);}} cleanup{root};
    std::filesystem::create_directories(root/"data/Cities");std::filesystem::create_directories(root/"saves");
    {std::ofstream out(root/"data/Cities/authored.map",std::ios::binary);out<<"Independently authored policy identity";}
    City city;road(city.world,{4,2});road(city.world,{5,2});road(city.world,{7,2});
    const auto mask=city.world.map_permissions()->building_mask();
    const auto before=save::make_document(root/"data","Cities/authored.map",mask,city.world);
    save::write_save(root/"saves/before.json",before,root/"data",mask);
    const auto report=diagnose(city.world,city.clay,city.pottery);
    const auto after=save::make_document(root/"data","Cities/authored.map",mask,city.world);
    save::write_save(root/"saves/after.json",after,root/"data",mask);
    check(bytes(root/"saves/before.json")==bytes(root/"saves/after.json") && before.world==after.world &&
          before.map_permissions_sha256==after.map_permissions_sha256,"N: diagnosis changed complete SaveDocument bytes");
    const auto document=save::read_save(root/"saves/after.json");
    auto loaded=save::restore_save(document,root/"data",mask,city.world.map_permissions());
    check(loaded.snapshot()==city.world.snapshot() &&
          diagnose(loaded,city.clay,city.pottery)==report,"N: Save/Load changed identical diagnostic");
    const auto invalid=diagnose(city.world,static_cast<sim::BuildingId>(999),city.pottery);
    check(invalid.category==Category::UnsupportedOrUnknown && invalid.cells_scanned==0 &&
          invalid.authoritative_route_queries==0,"Invalid ID must fail before grid/route work");
    sim::World legacy(20,18,std::vector<std::uint8_t>(360,1),sim::RulesProfile::CityV16,2);
    const auto unsupported=sim::diagnose_road_connectivity(legacy,city.clay,city.pottery);
    check(unsupported.category==Category::UnsupportedOrUnknown && unsupported.cells_scanned==0,
          "Legacy profile silently borrowed policy authority");
    sim::World oversized(std::make_shared<const sim::MapPermissions>(229,3,1,
        std::vector<sim::MapCellPermission>(229U*3U,{true,true,false,0,
            sim::BuildBlocker::None,sim::BuildBlocker::None}),std::vector<sim::FixedGatePassage>{}));
    const auto excessive=sim::diagnose_road_connectivity(oversized,city.clay,city.pottery);
    check(excessive.category==Category::UnsupportedOrUnknown && excessive.cells_scanned==0 &&
          excessive.authoritative_route_queries==0,"Out-of-scope dimension did diagnostic work");
    auto crowded_cells=std::vector<sim::MapCellPermission>(228U*228U,
        {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
    sim::World large(std::make_shared<const sim::MapPermissions>(228,228,1,std::move(crowded_cells),
        std::vector<sim::FixedGatePassage>{}));
    const auto first=place(large,sim::CommandType::PlaceClaySource,{2,2});
    const auto last=place(large,sim::CommandType::PlacePottery,{8,2});
    for (int x=4;x<=7;++x) road(large,{x,2});
    for (int y=3;y<=100;++y) road(large,{4,y});
    const auto bounded=diagnose(large,first,last);
    check(bounded.category==Category::Connected && bounded.cells_scanned==sim::road_connectivity_cell_limit &&
          bounded.blocked_edge_count>sim::road_connectivity_edge_limit &&
          bounded.blocked_edges.size()==sim::road_connectivity_edge_limit &&
          diagnose(large,first,last)==bounded,"Full-size diagnostic/display truncation was not bounded/deterministic");
}
} // namespace
int main() {
    try {
        a_connected_and_l_no_stock();b_c_missing_entrances();d_islands_and_o_revision();
        e_height_and_f_alternative_entrance();g_original_and_j_road_only();h_gate_and_i_side();
        k_unstaffed_and_m_protected_edge();n_save_readonly_and_limits();
        std::cout<<"Road connectivity A..O: paid authored fixtures, exact routes, pure snapshots/cache/policy/Save bytes passed\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
