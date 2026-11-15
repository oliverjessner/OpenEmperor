#pragma once

#include "simulation/MapPermissions.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace openemperor::original {

struct FishingQuayGeometry {
    std::array<simulation::Cell, 4> body;
    std::array<simulation::Cell, 5> occupied;
    std::array<simulation::Cell, 8> water;
    std::array<simulation::Cell, 2> land;
    std::array<simulation::Cell, 8> perimeter;
    simulation::Cell dock;
    bool operator==(const FishingQuayGeometry&) const = default;
};

// Construction5f3c40 uses the existing oriented shore helper and adds one
// exterior dock claim. The complete occupied shape contains five cells.
// Coordinates are checked through pump_geometry before any extra arithmetic.
// Source permissions, intake eligibility, ownership and payment remain caller
// responsibilities. Returning this geometry grants no World construction.
[[nodiscard]] FishingQuayGeometry fishing_quay_geometry(
    simulation::Cell origin, std::uint8_t orientation);

// One source navigation-byte test from the cardinal origin flood5accd0.
// The caller must resolve the actual navigation plane and its update state.
// Raw terrain, a visual water cell or point179 cannot substitute for that byte.
[[nodiscard]] bool fishing_origin_flood_byte_allowed(std::uint8_t navigation_byte) noexcept;

// Raw distance-preparation5b2c30 rejects FF and adds4 for FE; other bytes
// have no extra. The Original queue separately defers FE expansion. This
// classification supplies neither a base edge cost nor movement time, and
// grants no World navigation or source-plane authority.
[[nodiscard]] std::optional<std::uint8_t> boat_distance_extra_penalty(
    std::uint8_t navigation_byte) noexcept;

// One test of the separately resolved source attribute map's fishing bit2.
// Navigation bytes and connection attributes are separate source domains.
[[nodiscard]] bool fishing_origin_connection_present(
    std::uint8_t resolved_connection_flags) noexcept;

// Source5acac0 inspects the eight cardinal perimeter cells of a2x2 body.
// This query requires an exact caller-resolved connection-attribute plane,
// dimensions1..228 and a complete in-bounds body/perimeter. It performs no
// flood fill, point interpretation, route search or mutable World operation.
[[nodiscard]] bool quay_perimeter_has_fishing_connection(
    simulation::Cell origin, int width, int height,
    std::span<const std::uint8_t> resolved_connection_flags);

} // namespace openemperor::original
