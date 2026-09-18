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
    WalkerFallback fallback=WalkerFallback::None;
};
WalkerPose walker_pose(const simulation::CourierState& courier,std::uint64_t world_tick,
                       const assets::WalkerVisualProfile& profile);
}
