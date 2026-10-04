#include "maps/PinnacleSelector.h"

namespace openemperor::maps {
namespace {
constexpr std::size_t cell_count = stored_grid_width * stored_grid_height;
constexpr unsigned pinnacle_side = 5;
constexpr std::uint32_t pinnacle_bit = 0x02000000U;

std::size_t index(GridCell cell) {
    return std::size_t(cell.y) * stored_grid_width + cell.x;
}

ResourceGroupKey pinnacle_bank(std::uint32_t objects) {
    if (objects & 0x08U) return {0x60d};
    if (objects & 0x10U) return {0x60a};
    if (objects & 0x20U) return {0x609};
    if (objects & 0x40U) return {0x619};
    return {0x60d};
}
bool admitted_pinnacle_terrain(std::uint32_t terrain) {
    // The bounded corpus uses pinnacle with the ordinary fertility bit only.
    // Earlier 53ec90 water/vegetation/rock/exclusion branches must retain their
    // priority; an unstudied combination cannot be replaced by a mountain.
    return terrain == pinnacle_bit || terrain == (pinnacle_bit | 0x80U);
}
} // namespace

std::optional<PinnacleSelection> select_pinnacle(
    const LandscapeSelectorInput& input, GridCell cell,
    std::span<const std::uint8_t> draw_properties,
    std::span<const std::uint8_t> candidate_bytes) {
    if (input.terrain.size() != cell_count || input.objects.size() != cell_count ||
        draw_properties.size() != cell_count || candidate_bytes.size() != cell_count ||
        input.orientation != 0 || cell.x >= stored_grid_width || cell.y >= stored_grid_height)
        return {};

    const auto selected = index(cell);
    if (!admitted_pinnacle_terrain(input.terrain[selected]) ||
        (draw_properties[selected] & 0x0fU) != pinnacle_side - 1)
        return {};

    // 4b7bd0 walks until each coordinate field is zero. It does not derive
    // the origin from the historic graphics ID or from an image's size flag.
    GridCell origin = cell;
    for (unsigned step = 0; candidate_bytes[index(origin)] & 0x07U; ++step) {
        if (step >= pinnacle_side || origin.x == 0) return {};
        --origin.x;
    }
    for (unsigned step = 0; candidate_bytes[index(origin)] & 0x38U; ++step) {
        if (step >= pinnacle_side || origin.y == 0) return {};
        --origin.y;
    }
    if (origin.x + pinnacle_side > stored_grid_width ||
        origin.y + pinnacle_side > stored_grid_height ||
        cell.x >= origin.x + pinnacle_side || cell.y >= origin.y + pinnacle_side)
        return {};

    const auto group = pinnacle_bank(input.objects[selected]);
    for (unsigned y = 0; y < pinnacle_side; ++y) {
        for (unsigned x = 0; x < pinnacle_side; ++x) {
            const auto member = index({origin.x + x, origin.y + y});
            const unsigned coordinate = x | (y << 3U);
            const unsigned marker = x == 0 && y == pinnacle_side - 1 ? 0x40U : 0;
            if (!admitted_pinnacle_terrain(input.terrain[member]) ||
                (draw_properties[member] & 0x0fU) != pinnacle_side - 1 ||
                candidate_bytes[member] != (coordinate | marker) ||
                pinnacle_bank(input.objects[member]).value != group.value)
                return {};
        }
    }
    return PinnacleSelection{origin, pinnacle_side, group};
}
} // namespace openemperor::maps
