#pragma once
#include "assets/WalkerVisualProfile.h"
#include "simulation/World.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace openemperor {
enum class WalkerFallback { None, UnmappedDirection, InvalidEdge };
struct WalkerPose {
    bool moving=false;
    bool loaded=false;
    std::optional<assets::StorageDirection> direction;
    std::optional<std::size_t> frame; // Profile frame index, not an SG3 record.
    std::optional<assets::WalkerVisualRole> role;
    WalkerFallback fallback=WalkerFallback::None;
};
std::optional<assets::WalkerVisualRole> walker_visual_role(simulation::CourierRole role);
WalkerPose walker_pose(const simulation::CourierState& courier,std::uint64_t world_tick,
                       const assets::WalkerRoleVisual& role_visual);
WalkerPose walker_pose(const simulation::CourierState& courier,std::uint64_t world_tick,
                       const assets::WalkerVisualProfile& profile);
}
