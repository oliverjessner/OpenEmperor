#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace openemperor::maps {

inline constexpr std::uint32_t normal_wall_resource_key = 0x451U;

// Storage directions N, NE, E, SE, S, SW, W, NW. These are raw original
// terrain words, independent of sandbox roads, buildings and saved graphics.
struct WallTopologyInput {
    std::uint32_t center_terrain = 0;
    std::array<std::uint32_t, 8> neighboring_terrain{};
    // Already generated variants of resource 0x451, in N, E, S, W order.
    // The post-load updater walks increasing storage X within increasing Y.
    // Unvisited/non-wall cells have no value; saved IDs are never an input.
    std::array<std::optional<unsigned>, 4> generated_cardinal_variants{};
    unsigned orientation = 0; // Original even view value divided by two: 0..3.
};

struct WallTopologySelection {
    bool wall_bit = false;
    bool selector_gate = false;
    bool verified = false;
    bool gate_context = false;
    std::uint8_t neighbor_mask = 0;
    std::optional<unsigned> semantic_row; // One-based, 1..16.
    std::optional<unsigned> variant;
    const char* reason = "not normal wall terrain";
};

// Reproduces the observed static, gate-free branch of post-load updater
// 0x4b67b0. Special gate/model-object composition remains unresolved.
WallTopologySelection select_normal_wall(const WallTopologyInput& input);

} // namespace openemperor::maps
