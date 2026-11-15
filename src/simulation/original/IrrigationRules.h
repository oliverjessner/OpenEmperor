#pragma once

#include "simulation/MapPermissions.h"

#include <array>
#include <cstdint>
#include <optional>

namespace openemperor::original {

struct PumpGeometry {
    std::array<simulation::Cell, 4> footprint;
    std::array<simulation::Cell, 8> water;
    std::array<simulation::Cell, 2> land;
    std::array<simulation::Cell, 4> seeds;
    bool operator==(const PumpGeometry&) const = default;
};

// Physical orientation0..3 selects the observed 2x2 body, eight intake-water
// cells, two land cells and four exterior ditch seeds. Coordinates are checked
// for integer overflow. The caller validates every cell against its source map.
[[nodiscard]] PumpGeometry pump_geometry(simulation::Cell origin, std::uint8_t orientation);

// Observed terrain/manager predicate525750. An existing800 flag takes its
// original early-success branch. The caller separately resolves occupancy,
// source permissions, paid World ownership, bounds and road crossings.
[[nodiscard]] bool ditch_terrain_allowed(
    std::uint32_t terrain, std::uint16_t manager_word,
    std::uint16_t occupant_model = 0) noexcept;

// Cardinal masks use N/E/S/W bits1/2/4/8. A road crossing requires one axial
// road family and excludes parallel neighboring ditch connections.
[[nodiscard]] bool crossing_allowed(
    std::uint8_t road_neighbor_mask, std::uint8_t ditch_neighbor_mask) noexcept;

enum class IrrigationAxis : std::uint8_t { X, Y };

// Group435 variant0..17 in physical view0. Wet/dry bank selection belongs to
// the caller's authoritative supplied-water state. Masks outside0..15 refuse.
[[nodiscard]] std::uint8_t ditch_variant(
    std::uint8_t ditch_neighbor_mask,
    std::optional<IrrigationAxis> road_axis = std::nullopt);

// Observed square perimeters1..5, excluding the field cell. Flooding suppresses
// the query. The caller resolves map bounds and actual wet-ditch eligibility.
[[nodiscard]] bool field_in_irrigation_range(
    simulation::Cell field, simulation::Cell ditch, bool flood_active = false) noexcept;

} // namespace openemperor::original
