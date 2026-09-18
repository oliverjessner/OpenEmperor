#include "app/WalkerPose.h"

namespace openemperor {
WalkerPose walker_pose(const simulation::CourierState& courier,std::uint64_t world_tick,
                       const assets::WalkerVisualProfile& profile) {
    WalkerPose pose;
    pose.loaded=courier.cargo>0;
    if (!courier.enabled || courier.phase==simulation::CourierPhase::IdleAtWorkshop ||
        courier.path.size()<2 || courier.path_vertex>=courier.path.size()-1 ||
        (courier.route_pending && courier.edge_progress==0)) {
        pose.frame=profile.idle_frame;
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
    const auto& clip=profile.clips[assets::direction_index(*pose.direction)];
    if (clip.empty()) { pose.fallback=WalkerFallback::UnmappedDirection; return pose; }
    pose.frame=clip[(world_tick/profile.ticks_per_frame)%clip.size()];
    return pose;
}
}
