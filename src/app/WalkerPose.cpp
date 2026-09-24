#include "app/WalkerPose.h"

namespace openemperor {
std::optional<assets::WalkerVisualRole> walker_visual_role(simulation::CourierRole role) {
    switch (role) {
    case simulation::CourierRole::Clay: return assets::WalkerVisualRole::Clay;
    case simulation::CourierRole::Pottery: return assets::WalkerVisualRole::Pottery;
    case simulation::CourierRole::Household: return assets::WalkerVisualRole::Household;
    case simulation::CourierRole::Food: return std::nullopt;
    case simulation::CourierRole::Service: return std::nullopt;
    case simulation::CourierRole::None: return std::nullopt;
    }
    return std::nullopt;
}
WalkerPose walker_pose(const simulation::CourierState& courier,std::uint64_t world_tick,
                       const assets::WalkerRoleVisual& role_visual) {
    WalkerPose pose;
    pose.loaded=courier.cargo>0;
    if (!courier.enabled || courier.phase==simulation::CourierPhase::IdleAtWorkshop ||
        courier.path.size()<2 || courier.path_vertex>=courier.path.size()-1 ||
        (courier.route_pending && courier.edge_progress==0)) {
        pose.frame=role_visual.idle_frame;
        return pose;
    }
    const auto& from=courier.path[courier.path_vertex];
    const auto& to=courier.path[courier.path_vertex+1];
    if (to.x==from.x+1 && to.y==from.y) pose.direction=assets::StorageDirection::PosX;
    else if (to.x==from.x-1 && to.y==from.y) pose.direction=assets::StorageDirection::NegX;
    else if (to.x==from.x && to.y==from.y+1) pose.direction=assets::StorageDirection::PosY;
    else if (to.x==from.x && to.y==from.y-1) pose.direction=assets::StorageDirection::NegY;
    else { pose.fallback=WalkerFallback::InvalidEdge; return pose; }
    pose.moving=true;
    const auto& clip=role_visual.clips[assets::direction_index(*pose.direction)];
    if (clip.empty()) { pose.fallback=WalkerFallback::UnmappedDirection; return pose; }
    pose.frame=clip[(world_tick/role_visual.ticks_per_frame)%clip.size()];
    return pose;
}
WalkerPose walker_pose(const simulation::CourierState& courier,std::uint64_t world_tick,
                       const assets::WalkerVisualProfile& profile) {
    const auto visual_role=walker_visual_role(courier.role);
    if (!visual_role) return {};
    const auto* visual=profile.find(*visual_role);
    if (!visual) return {};
    auto pose=walker_pose(courier,world_tick,*visual);
    pose.role=*visual_role;
    return pose;
}
}
