#include "simulation/original/IrrigationRules.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

using simulation::Cell;

constexpr std::array<Cell, 4> footprint{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
constexpr std::array<PumpGeometry, 4> geometry{{
    {footprint,
     {{{-1, -1}, {0, -1}, {1, -1}, {2, -1}, {-1, 0}, {0, 0}, {1, 0}, {2, 0}}},
     {{{0, 1}, {1, 1}}},
     {{{0, 2}, {1, 2}, {-1, 1}, {2, 1}}}},
    {footprint,
     {{{1, -1}, {2, -1}, {1, 0}, {2, 0}, {1, 1}, {2, 1}, {1, 2}, {2, 2}}},
     {{{0, 0}, {0, 1}}},
     {{{-1, 0}, {-1, 1}, {0, -1}, {0, 2}}}},
    {footprint,
     {{{-1, 1}, {0, 1}, {1, 1}, {2, 1}, {-1, 2}, {0, 2}, {1, 2}, {2, 2}}},
     {{{0, 0}, {1, 0}}},
     {{{0, -1}, {1, -1}, {2, 0}, {-1, 0}}}},
    {footprint,
     {{{-1, -1}, {0, -1}, {-1, 0}, {0, 0}, {-1, 1}, {0, 1}, {-1, 2}, {0, 2}}},
     {{{1, 0}, {1, 1}}},
     {{{2, 0}, {2, 1}, {1, -1}, {1, 2}}}}
}};

int translated_coordinate(int value, int offset) {
    if ((offset > 0 && value > std::numeric_limits<int>::max() - offset) ||
        (offset < 0 && value < std::numeric_limits<int>::min() - offset))
        throw std::overflow_error("irrigation coordinate exceeds the Cell integer range");
    return value + offset;
}

template<std::size_t Count>
std::array<Cell, Count> translated_cells(const std::array<Cell, Count>& offsets, Cell origin) {
    std::array<Cell, Count> cells{};
    for (std::size_t index = 0; index < Count; ++index) {
        cells[index] = {translated_coordinate(origin.x, offsets[index].x),
                        translated_coordinate(origin.y, offsets[index].y)};
    }
    return cells;
}

} // namespace

PumpGeometry pump_geometry(Cell origin, std::uint8_t orientation) {
    if (orientation >= geometry.size())
        throw std::out_of_range("original irrigation Pump orientation is outside 0..3");
    const auto& selected = geometry[orientation];
    return {translated_cells(selected.footprint, origin), translated_cells(selected.water, origin),
            translated_cells(selected.land, origin), translated_cells(selected.seeds, origin)};
}

bool ditch_terrain_allowed(
    std::uint32_t terrain, std::uint16_t manager_word, std::uint16_t occupant_model) noexcept {
    if ((terrain & 0x800U) != 0) return true;
    if ((terrain & 0x87375227U) != 0) return false;
    if ((terrain & 0x8U) != 0 && occupant_model != 203 && occupant_model != 126) return false;
    return (manager_word & 0x1ebU) == 0 && (terrain & 0x90060201U) == 0;
}

bool crossing_allowed(std::uint8_t road_neighbor_mask, std::uint8_t ditch_neighbor_mask) noexcept {
    if (road_neighbor_mask > 15 || ditch_neighbor_mask > 15) return false;
    const auto north_south = road_neighbor_mask & 0x5U;
    const auto east_west = road_neighbor_mask & 0xaU;
    if (north_south != 0 && east_west == 0) return (ditch_neighbor_mask & 0x5U) == 0;
    if (east_west != 0 && north_south == 0) return (ditch_neighbor_mask & 0xaU) == 0;
    return false;
}

std::uint8_t ditch_variant(
    std::uint8_t ditch_neighbor_mask, std::optional<IrrigationAxis> road_axis) {
    if (ditch_neighbor_mask > 15)
        throw std::out_of_range("original irrigation neighbor mask is outside 0..15");
    constexpr std::array<std::uint8_t, 16> variants{15, 6, 7, 2, 8, 1, 3, 10, 9, 5, 0, 13, 4, 12, 11, 14};
    const auto variant = variants[ditch_neighbor_mask];
    if (!road_axis) return variant;
    if (*road_axis != IrrigationAxis::X && *road_axis != IrrigationAxis::Y)
        throw std::out_of_range("unsupported original irrigation crossing axis");
    // The source first recognizes axial ditch imagery, then consults the road
    // axis for the remaining topology variants. World checks crossing legality.
    switch (variant) {
    case 0:
    case 7:
    case 9: return 16;
    case 1:
    case 6:
    case 8: return 17;
    default: return *road_axis == IrrigationAxis::Y ? 16 : 17;
    }
}

bool field_in_irrigation_range(Cell field, Cell ditch, bool flood_active) noexcept {
    if (flood_active) return false;
    const auto dx = static_cast<std::int64_t>(field.x) - ditch.x;
    const auto dy = static_cast<std::int64_t>(field.y) - ditch.y;
    const auto distance = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
    return distance >= 1 && distance <= 5;
}

} // namespace openemperor::original
