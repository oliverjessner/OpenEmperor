#pragma once

#include "simulation/World.h"

#include <array>
#include <cstddef>
#include <string>

namespace openemperor::simulation {

// An explicit diagnostic query, never a gameplay path or repair decision.
enum class RoadConnectivityCategory : std::uint8_t {
    Connected, SourceNoEntrance, TargetNoEntrance, DisconnectedRoadComponents,
    BlockedHeightTransition, BlockedOriginalStructure, BlockedGateEntry,
    MissingRoadCell, UnsupportedOrUnknown
};
const char* road_connectivity_category_name(RoadConnectivityCategory category);
inline constexpr std::size_t road_connectivity_edge_limit=64;
inline constexpr std::size_t road_connectivity_cell_limit=228U*228U;

struct RoadConnectivityCell {
    Cell cell{};
    Object object=Object::Empty;
    bool in_bounds=false,road_allowed=false,building_allowed=false;
    bool protected_original=false,fixed_passage=false;
    std::int8_t height=0;
    std::optional<FixedGateId> fixed_gate;
    BuildBlocker road_blocker=BuildBlocker::None,building_blocker=BuildBlocker::None;
    bool operator==(const RoadConnectivityCell&) const = default;
};
struct RoadConnectivityEdge {
    RoadConnectivityCell from{},to{};
    RoadConnectivityCategory category=RoadConnectivityCategory::UnsupportedOrUnknown;
    BuildBlocker permission_blocker=BuildBlocker::None;
    std::string reason;
    // Candidate means only this measured local attachment was examined.
    // Accepted PlaceRoad validation never promises a complete repaired route.
    bool candidate=false,place_road_allowed=false;
    std::optional<Cell> candidate_cell;
    BuildDiagnostic place_road_diagnostic{};
    bool operator==(const RoadConnectivityEdge&) const = default;
};
struct RoadConnectivityEndpoint {
    BuildingId id=BuildingId::ClaySource;
    Object kind=Object::Empty;
    Cell origin{};
    std::vector<Cell> footprint;
    std::vector<BuildingEntrance> entrances;
    std::vector<RoadConnectivityEdge> rejected_entrances;
    std::vector<RoadConnectivityEdge> candidate_entrances;
    bool operator==(const RoadConnectivityEndpoint&) const = default;
};
struct RoadConnectivityComponent {
    std::uint32_t id=0; // Stable row-major first-cell order, starting at one.
    std::size_t cell_count=0,source_entrances=0,target_entrances=0;
    Cell first_cell{};
    bool operator==(const RoadConnectivityComponent&) const = default;
};
struct RoadConnectivityCourier {
    CourierId id=CourierId::Clay;
    CourierRole role=CourierRole::None;
    int workers_assigned=0,workers_required=0,output_stock=0,cargo=0,reserved=0;
    CourierPhase phase=CourierPhase::IdleAtWorkshop;
    bool route_pending=false;
    BuildingId target=BuildingId::Pottery;
    CourierDispatchStatus dispatch_status=CourierDispatchStatus::Disabled;
    std::optional<BuildingId> selected_target;
    std::uint64_t cached_revision=0,road_revision=0;
    bool operator==(const RoadConnectivityCourier&) const = default;
};
struct RoadConnectivityReport {
    RoadConnectivityCategory category=RoadConnectivityCategory::UnsupportedOrUnknown;
    std::string unsupported_reason;
    RoadConnectivityEndpoint source{},target{};
    std::uint64_t measured_tick=0,command_sequence=0,road_revision=0;
    std::uint32_t policy_version=0;
    // This is precisely World::find_building_route(), including both building
    // endpoint cells. No diagnostic route is substituted for that authority.
    std::optional<std::vector<Cell>> route;
    std::vector<RoadConnectivityComponent> components;
    std::vector<std::uint32_t> source_components,target_components;
    std::vector<RoadConnectivityEdge> blocked_edges;
    std::size_t blocked_edge_count=0;
    std::array<std::size_t,9> blocked_category_counts{};
    std::vector<RoadConnectivityCourier> couriers;
    std::size_t cells_scanned=0,transport_cells=0,component_edge_checks=0;
    std::size_t boundary_edge_checks=0,authoritative_route_queries=0;
    bool operator==(const RoadConnectivityReport&) const = default;
};

RoadConnectivityReport diagnose_road_connectivity(
    const World& world,BuildingId source,BuildingId target);
// Stable role-compatible candidates only, using existing courier_can_target.
// This is neither a dispatch target choice nor a promise of available stock.
std::vector<BuildingId> road_connectivity_targets(const World& world,CourierId courier);

} // namespace openemperor::simulation
