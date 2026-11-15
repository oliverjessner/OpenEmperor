#include "simulation/original/FishingRules.h"
#include "simulation/original/IrrigationRules.h"

#include <algorithm>
#include <stdexcept>

namespace openemperor::original {

FishingQuayGeometry fishing_quay_geometry(simulation::Cell origin, std::uint8_t orientation) {
    const auto shore = pump_geometry(origin, orientation);
    // The shore helper checks all four perimeter extents through its body,
    // water context and exterior seeds, including origin-1 and origin+2.
    const std::array<simulation::Cell, 8> perimeter{{
        {origin.x, origin.y - 1},
        {origin.x + 1, origin.y - 1},
        {origin.x + 2, origin.y},
        {origin.x + 2, origin.y + 1},
        {origin.x + 1, origin.y + 2},
        {origin.x, origin.y + 2},
        {origin.x - 1, origin.y + 1},
        {origin.x - 1, origin.y}
    }};
    const auto dock = perimeter[2U * orientation];
    const std::array<simulation::Cell, 5> occupied{{
        shore.footprint[0], shore.footprint[1], shore.footprint[2], shore.footprint[3], dock
    }};
    return {shore.footprint, occupied, shore.water, shore.land, perimeter, dock};
}

bool fishing_origin_flood_byte_allowed(std::uint8_t navigation_byte) noexcept {
    return navigation_byte != 0xffU && navigation_byte != 0xfeU;
}

std::optional<std::uint8_t> boat_distance_extra_penalty(std::uint8_t navigation_byte) noexcept {
    if (navigation_byte == 0xffU) return std::nullopt;
    return navigation_byte == 0xfeU ? std::uint8_t{4} : std::uint8_t{0};
}

bool fishing_origin_connection_present(std::uint8_t resolved_connection_flags) noexcept {
    return (resolved_connection_flags & 0x2U) != 0;
}

bool quay_perimeter_has_fishing_connection(
    simulation::Cell origin, int width, int height,
    std::span<const std::uint8_t> resolved_connection_flags) {
    if (width < 1 || height < 1 || width > 228 || height > 228 ||
        resolved_connection_flags.size() !=
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        throw std::invalid_argument("fishing connection plane requires exact bounded dimensions");
    if (origin.x < 1 || origin.y < 1 || origin.x > width - 3 || origin.y > height - 3)
        throw std::out_of_range("fishing connection query requires a complete in-bounds perimeter");
    const auto geometry = fishing_quay_geometry(origin, 0);
    return std::any_of(geometry.perimeter.begin(), geometry.perimeter.end(),
        [&](simulation::Cell cell) {
            const auto index = static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(cell.x);
            return fishing_origin_connection_present(resolved_connection_flags[index]);
        });
}

} // namespace openemperor::original
