#include "maps/WallTopology.h"

namespace openemperor::maps {
namespace {
struct WallRule {
    std::uint8_t required_present, required_absent;
    std::array<unsigned, 4> variants;
};

// Independently expressed neighbor requirements and orientation choices
// observed through 0x4b67b0 -> 0x4b8f70 -> 0x4bbdc0. The first cross rule
// requires absent diagonals; the other rules ignore diagonals. This is not a
// road-mask table and does not infer a missing rule for solid wall clusters.
constexpr std::array<WallRule, 16> rules{{
    {0x55, 0xaa, {12, 12, 12, 12}},
    {0x45, 0x10, {8, 11, 10, 9}},
    {0x15, 0x40, {9, 8, 11, 10}},
    {0x54, 0x01, {10, 9, 8, 11}},
    {0x51, 0x04, {11, 10, 9, 8}},
    {0x11, 0x44, {2, 0, 2, 0}},
    {0x44, 0x11, {0, 2, 0, 2}},
    {0x05, 0x50, {7, 6, 5, 4}},
    {0x14, 0x41, {4, 7, 6, 5}},
    {0x50, 0x05, {5, 4, 7, 6}},
    {0x41, 0x14, {6, 5, 4, 7}},
    {0x01, 0x54, {15, 14, 13, 16}},
    {0x04, 0x51, {16, 15, 14, 13}},
    {0x10, 0x45, {13, 16, 15, 14}},
    {0x40, 0x15, {14, 13, 16, 15}},
    {0x00, 0x55, {17, 17, 17, 17}},
}};
}

const char* wall_gate_connection_status_name(WallGateConnectionStatus status) {
    switch (status) {
    case WallGateConnectionStatus::NoContext:return "no_gate_context";
    case WallGateConnectionStatus::GateOriginCompositionRequired:return "gate_origin_composition_required";
    case WallGateConnectionStatus::AdjacentStaticSelected:return "gate_adjacent_static_selected";
    case WallGateConnectionStatus::UnsupportedTopology:return "gate_connected_topology_unsupported";
    }
    return "invalid";
}

WallTopologySelection select_normal_wall(const WallTopologyInput& input) {
    WallTopologySelection result;
    result.wall_bit = (input.center_terrain & 0x4000U) != 0;
    if (!result.wall_bit) return result;
    // 0x4b67fe/0x4b6807/0x4b6810: wall, no flood bit, no road/building bits.
    result.selector_gate = (input.center_terrain & 0x148U) == 0;
    result.reason = "post-load wall gate excludes flood, road or building terrain";
    if (!result.selector_gate) return result;
    if (input.orientation >= 4U) {
        result.reason = "unsupported original view orientation";
        return result;
    }
    result.center_gate = (input.center_terrain & 0x8000U) != 0;
    result.gate_context = result.center_gate;
    for (unsigned direction = 0; direction < 8U; ++direction) {
        const auto terrain = input.neighboring_terrain[direction];
        const auto bit=std::uint8_t(1U << direction);
        if ((terrain & 0x4000U) != 0) result.wall_neighbor_mask |= bit;
        if ((terrain & 0x8000U) != 0) result.gate_neighbor_mask |= bit;
        result.gate_context = result.gate_context || (terrain & 0x8000U) != 0;
    }
    result.neighbor_mask=result.wall_neighbor_mask | result.gate_neighbor_mask;
    // A gate bit on this cell does not establish the gate object's geometry.
    // This bounded extension owns only adjacent singleton wall cells.
    if (result.center_gate) {
        result.gate_connection=WallGateConnectionStatus::GateOriginCompositionRequired;
        result.reason = "gate origin needs separate original model composition";
        return result;
    }
    // 4b67b0 calls 4b8f70(0x4000), then 4b6290 for the same eight neighbors.
    // The matcher tests nonzero. 4b6370's orientation context reaches 4b6440,
    // whose remaps affect only variants 18..40, never this table's 0..17.
    // No saved image, entity ID or gate-object orientation selects this base.
    for (unsigned row = 0; row < rules.size(); ++row) {
        const auto& rule = rules[row];
        if ((result.neighbor_mask & rule.required_present) != rule.required_present ||
            (result.neighbor_mask & rule.required_absent) != 0) continue;
        auto variant = rule.variants[input.orientation];
        // 0x4bee90 changes only straight variants 0/2: use the adjacent alternate
        // 1/3 as a stop condition, otherwise alternate after an already generated
        // matching straight. This preserves the updater's observed scan order.
        if (variant == 0U || variant == 2U) {
            bool matching = false, alternate = false;
            for (const auto neighbor : input.generated_cardinal_variants) {
                matching = matching || neighbor == variant;
                alternate = alternate || neighbor == variant + 1U;
            }
            if (matching && !alternate) ++variant;
        }
        result.semantic_row = row + 1U;
        result.variant = variant;
        result.verified = true;
        if (result.gate_neighbor_mask) {
            result.gate_connection=WallGateConnectionStatus::AdjacentStaticSelected;
            result.reason="EXE-observed static gate-adjacent post-load normal-wall selector";
        } else result.reason = "EXE-observed static gate-free post-load normal-wall selector";
        return result;
    }
    if (result.gate_neighbor_mask) {
        result.gate_connection=WallGateConnectionStatus::UnsupportedTopology;
        result.reason="no observed gate-connected wall topology row matches; historical preview retained";
    } else result.reason = "no observed wall topology row matches; original stale-state behavior unresolved";
    return result;
}

} // namespace openemperor::maps
