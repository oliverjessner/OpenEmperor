#include "simulation/RoadConnectivity.h"

#include <algorithm>
#include <deque>

namespace openemperor::simulation {
namespace {
// The existing World traversal order, in storage coordinates.
constexpr std::array<Cell,4> neighbors{{{0,-1},{-1,0},{1,0},{0,1}}};
Cell add(Cell a,Cell b) { return {a.x+b.x,a.y+b.y}; }
std::size_t index(const World& world,Cell p) {
    return static_cast<std::size_t>(p.y)*static_cast<std::size_t>(world.width())+
           static_cast<std::size_t>(p.x);
}
bool contains(const std::vector<std::uint32_t>& values,std::uint32_t id) {
    return std::binary_search(values.begin(),values.end(),id);
}
RoadConnectivityCategory category(BuildBlocker blocker) {
    switch (blocker) {
    case BuildBlocker::UnsupportedHeightTransition:
        return RoadConnectivityCategory::BlockedHeightTransition;
    case BuildBlocker::OriginalStructure:
    case BuildBlocker::GateSolidPart:
        return RoadConnectivityCategory::BlockedOriginalStructure;
    case BuildBlocker::GateSideEntry:
    case BuildBlocker::ProtectedGatePassage:
        return RoadConnectivityCategory::BlockedGateEntry;
    default:return RoadConnectivityCategory::UnsupportedOrUnknown;
    }
}
RoadConnectivityCell facts(const World& world,Cell cell) {
    RoadConnectivityCell result;
    result.cell=cell;result.in_bounds=world.in_bounds(cell);result.object=world.object_at(cell);
    const auto& permissions=*world.map_permissions();
    result.height=permissions.cell_height(cell);
    result.road_allowed=permissions.road_allowed(cell);
    result.building_allowed=permissions.building_allowed(cell);
    result.protected_original=permissions.protected_original(cell);
    result.fixed_passage=permissions.fixed_passage(cell);
    result.fixed_gate=permissions.fixed_gate_at(cell);
    result.road_blocker=permissions.road_blocker(cell);
    result.building_blocker=permissions.building_blocker(cell);
    return result;
}
RoadConnectivityEdge edge(const World& world,Cell from,Cell to,bool candidate) {
    RoadConnectivityEdge result;
    result.from=facts(world,from);result.to=facts(world,to);
    result.permission_blocker=world.map_permissions()->transport_edge_blocker(from,to);
    result.category=category(result.permission_blocker);
    result.reason=build_blocker_reason(result.permission_blocker);
    result.candidate=candidate;
    if (candidate) {
        result.candidate_cell=to;
        const auto validation=world.validate({CommandType::PlaceRoad,to});
        result.place_road_allowed=validation.accepted;
        result.place_road_diagnostic=validation.diagnostic;
        if (result.permission_blocker==BuildBlocker::None) {
            const auto permission=world.map_permissions()->road_blocker(to);
            result.category=permission==BuildBlocker::None ?
                RoadConnectivityCategory::MissingRoadCell:category(permission);
            result.reason=permission==BuildBlocker::None ?
                "Cell has no Sandbox road or fixed passage":build_blocker_reason(permission);
        }
    }
    return result;
}
RoadConnectivityEndpoint endpoint(const World& world,const BuildingState& building,bool target) {
    RoadConnectivityEndpoint result;
    result.id=building.id;result.kind=building.kind;result.origin=building.cell;
    result.footprint=building_footprint_cells(world.profile(),world.rule_version(),
                                           building.kind,building.cell);
    result.entrances=world.building_entrances(building.id);
    for (const auto from:result.footprint) for (const auto delta:neighbors) {
        const auto to=add(from,delta);
        if (!world.in_bounds(to) || world.building_owner_at(to)==building.id) continue;
        const bool valid=std::find(result.entrances.begin(),result.entrances.end(),
                                  BuildingEntrance{to,from})!=result.entrances.end();
        if (valid) continue;
        const bool actual_road=world.object_at(to)==Object::Road;
        const bool fixed=world.fixed_passage(to);
        if (!actual_road && !fixed && world.object_at(to)!=Object::Empty) continue;
        auto rejected=edge(world,from,to,!actual_road && !fixed);
        // World::building_entrances requires an actual Road. Even an opening
        // cannot enter a building directly from the fixed corridor.
        if (fixed) {
            rejected.category=RoadConnectivityCategory::BlockedGateEntry;
            if (rejected.permission_blocker==BuildBlocker::None)
                rejected.reason="Building entrances require a Sandbox road, not a fixed passage";
        }
        if (target) std::swap(rejected.from,rejected.to);
        if (actual_road || fixed) result.rejected_entrances.push_back(std::move(rejected));
        else result.candidate_entrances.push_back(std::move(rejected));
    }
    return result;
}
void add_component_ids(const RoadConnectivityEndpoint& endpoint,
                       const std::vector<std::uint32_t>& memberships,const World& world,
                       std::vector<std::uint32_t>& output,
                       std::vector<RoadConnectivityComponent>& components,bool source) {
    for (const auto& entrance:endpoint.entrances) {
        const auto id=memberships[index(world,entrance.road_cell)];
        output.push_back(id);
        if (source) ++components[id-1].source_entrances;
        else ++components[id-1].target_entrances;
    }
    std::sort(output.begin(),output.end());
    output.erase(std::unique(output.begin(),output.end()),output.end());
}
} // namespace

const char* road_connectivity_category_name(RoadConnectivityCategory value) {
    switch (value) {
    case RoadConnectivityCategory::Connected:return "CONNECTED";
    case RoadConnectivityCategory::SourceNoEntrance:return "SOURCE_NO_ENTRANCE";
    case RoadConnectivityCategory::TargetNoEntrance:return "TARGET_NO_ENTRANCE";
    case RoadConnectivityCategory::DisconnectedRoadComponents:return "DISCONNECTED_ROAD_COMPONENTS";
    case RoadConnectivityCategory::BlockedHeightTransition:return "BLOCKED_HEIGHT_TRANSITION";
    case RoadConnectivityCategory::BlockedOriginalStructure:return "BLOCKED_ORIGINAL_STRUCTURE";
    case RoadConnectivityCategory::BlockedGateEntry:return "BLOCKED_GATE_ENTRY";
    case RoadConnectivityCategory::MissingRoadCell:return "MISSING_ROAD_CELL";
    case RoadConnectivityCategory::UnsupportedOrUnknown:return "UNSUPPORTED_OR_UNKNOWN";
    }
    return "UNSUPPORTED_OR_UNKNOWN";
}

std::vector<BuildingId> road_connectivity_targets(const World& world,CourierId id) {
    std::vector<BuildingId> result;
    const auto couriers=world.couriers();
    const auto found=std::find_if(couriers.begin(),couriers.end(),
        [id](const auto& value) { return value.id==id; });
    if (found==couriers.end()) return result;
    for (const auto& target:world.buildings())
        if (target.id!=found->owner && courier_can_target(found->role,target))
            result.push_back(target.id);
    std::sort(result.begin(),result.end());
    return result;
}

RoadConnectivityReport diagnose_road_connectivity(
    const World& world,BuildingId source_id,BuildingId target_id) {
    RoadConnectivityReport result;
    result.source.id=source_id;result.target.id=target_id;
    result.measured_tick=world.ticks();result.command_sequence=world.command_sequence();
    result.road_revision=world.road_revision();
    const auto permissions=world.map_permissions();
    if (world.profile()!=RulesProfile::CityV16 || world.rule_version()!=3 || !permissions ||
        permissions->policy_version()!=kMapPermissionsPolicyVersion) {
        result.unsupported_reason="Requires City-v16 rule 3 and complete policy-1 permissions";
        return result;
    }
    result.policy_version=permissions->policy_version();
    if (world.width()<=0 || world.height()<=0 || world.width()>228 || world.height()>228 ||
        static_cast<std::size_t>(world.width())*static_cast<std::size_t>(world.height())>
            road_connectivity_cell_limit) {
        result.unsupported_reason="World exceeds the bounded 228x228 diagnostic grid";
        return result;
    }
    const auto buildings=world.buildings();
    const auto find=[&](BuildingId id) {
        return std::find_if(buildings.begin(),buildings.end(),
            [id](const auto& value) { return value.id==id && value.placed; });
    };
    const auto source=find(source_id),target=find(target_id);
    if (source==buildings.end() || target==buildings.end() || source_id==target_id) {
        result.unsupported_reason="Requires two distinct placed building IDs";
        return result;
    }
    result.source=endpoint(world,*source,false);result.target=endpoint(world,*target,true);
    result.authoritative_route_queries=1;
    result.route=world.find_building_route(source_id,target_id);
    if (result.route) result.category=RoadConnectivityCategory::Connected;
    else if (result.source.entrances.empty()) result.category=RoadConnectivityCategory::SourceNoEntrance;
    else if (result.target.entrances.empty()) result.category=RoadConnectivityCategory::TargetNoEntrance;
    else result.category=RoadConnectivityCategory::DisconnectedRoadComponents;

    const auto count=static_cast<std::size_t>(world.width())*static_cast<std::size_t>(world.height());
    std::vector<std::uint32_t> memberships(count,0);
    std::vector<Cell> transport_cells;
    std::deque<Cell> queue;
    for (int y=0;y<world.height();++y) for (int x=0;x<world.width();++x) {
        ++result.cells_scanned;
        const Cell first{x,y};
        if (!world.transport_cell(first)) continue;
        ++result.transport_cells;
        transport_cells.push_back(first);
        if (memberships[index(world,first)]!=0) continue;
        const auto id=static_cast<std::uint32_t>(result.components.size()+1);
        result.components.push_back({id,0,0,0,first});
        memberships[index(world,first)]=id;queue.push_back(first);
        while (!queue.empty()) {
            const auto from=queue.front();queue.pop_front();
            ++result.components.back().cell_count;
            for (const auto delta:neighbors) {
                ++result.component_edge_checks;
                const auto to=add(from,delta);
                if (!world.transport_edge_allowed(from,to)) continue;
                if (memberships[index(world,to)]!=0) continue;
                memberships[index(world,to)]=id;queue.push_back(to);
            }
        }
    }
    add_component_ids(result.source,memberships,world,result.source_components,result.components,true);
    add_component_ids(result.target,memberships,world,result.target_components,result.components,false);

    // Highest priority: rejected real building entrances and cross-component
    // transport edges; then local single-cell candidates touching the opposite
    // endpoint component; finally the other measured component boundaries.
    // All examined reasons are counted even when the display list is truncated.
    std::array<std::vector<RoadConnectivityEdge>,3> buckets;
    const auto record=[&](RoadConnectivityEdge value,std::size_t priority) {
        ++result.blocked_edge_count;
        ++result.blocked_category_counts[static_cast<std::size_t>(value.category)];
        if (buckets[priority].size()<road_connectivity_edge_limit)
            buckets[priority].push_back(std::move(value));
    };
    for (const auto& value:result.source.rejected_entrances) record(value,0);
    for (const auto& value:result.target.rejected_entrances) record(value,0);
    if (result.source.entrances.empty())
        for (const auto& value:result.source.candidate_entrances) record(value,1);
    if (result.target.entrances.empty())
        for (const auto& value:result.target.candidate_entrances) record(value,1);
    for (const auto from:transport_cells) {
        const auto id=memberships[index(world,from)];
        const bool source_member=contains(result.source_components,id);
        const bool target_member=contains(result.target_components,id);
        if (id==0 || (!source_member && !target_member)) continue;
        for (const auto delta:neighbors) {
            ++result.boundary_edge_checks;
            const auto to=add(from,delta);
            if (!world.in_bounds(to) || world.transport_edge_allowed(from,to)) continue;
            const auto owner=world.building_owner_at(to);
            if (owner==source_id || owner==target_id) continue;
            const auto other=memberships[index(world,to)];
            const bool opposite=(source_member && contains(result.target_components,other)) ||
                                (target_member && contains(result.source_components,other));
            // Avoid duplicate directed observations of the same relevant edge.
            if (other!=0 && (contains(result.source_components,other) ||
                            contains(result.target_components,other)) && index(world,from)>index(world,to))
                continue;
            if (other==0 && world.object_at(to)!=Object::Empty) continue;
            bool touches_opposite=false;
            if (other==0) for (const auto next_delta:neighbors) {
                const auto next=add(to,next_delta);
                if (!world.in_bounds(next)) continue;
                const auto next_id=memberships[index(world,next)];
                touches_opposite|=(source_member && contains(result.target_components,next_id)) ||
                                  (target_member && contains(result.source_components,next_id));
            }
            record(edge(world,from,to,other==0),opposite ? 0:touches_opposite ? 1:2);
        }
    }
    for (const auto& bucket:buckets) for (const auto& value:bucket) {
        if (result.blocked_edges.size()==road_connectivity_edge_limit) break;
        result.blocked_edges.push_back(value);
    }
    for (const auto& courier:world.couriers()) if (courier.owner==source_id) {
        const auto decision=world.courier_dispatch_status(courier.id);
        RoadConnectivityCourier value;
        value.id=courier.id;value.role=courier.role;
        value.workers_assigned=world.workers_assigned(source_id);
        value.workers_required=world.workforce_required(source_id);
        value.output_stock=source->output;value.cargo=courier.cargo;value.reserved=courier.reserved;
        value.phase=courier.phase;value.route_pending=courier.route_pending;
        value.target=courier.target;value.dispatch_status=decision.status;
        value.selected_target=decision.selected_target;value.cached_revision=courier.cached_revision;
        value.road_revision=world.road_revision();result.couriers.push_back(value);
    }
    return result;
}
} // namespace openemperor::simulation
