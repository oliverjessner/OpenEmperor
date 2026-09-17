#pragma once

#include "maps/EmperorMap.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::maps {

enum class RawLayer { Terrain, Objects };
struct GridPoint { double x = 0; double y = 0; };
struct GridCell {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    bool operator==(const GridCell&) const = default;
};
struct DisplayCell {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    bool operator==(const DisplayCell&) const = default;
};

class StorageGridCamera {
public:
    double zoom = 1.0; // 0.25..16, relative to three screen pixels per cell.
    GridPoint offset{};
    int viewport_width = 1;
    int viewport_height = 1;
    std::uint32_t grid_width = stored_grid_width;
    std::uint32_t grid_height = stored_grid_height;
    static constexpr double base_cell_pixels = 3.0;
    GridPoint grid_to_screen(GridPoint grid) const;
    GridPoint screen_to_grid(GridPoint screen) const;
    std::optional<DisplayCell> pick(GridPoint screen) const;
    void zoom_at(GridPoint screen, double factor);
    void center_on(GridPoint grid);
};

std::array<std::uint8_t, 4> raw_value_color(std::uint32_t value);
std::vector<std::uint8_t> make_storage_rgba(const ParsedEmperorMap& map, RawLayer layer);

} // namespace openemperor::maps
