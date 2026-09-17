#include "maps/StorageGridView.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace openemperor::maps {

GridPoint StorageGridCamera::grid_to_screen(GridPoint grid) const {
    const double scale = base_cell_pixels * zoom;
    return {offset.x + grid.x * scale, offset.y + grid.y * scale};
}
GridPoint StorageGridCamera::screen_to_grid(GridPoint screen) const {
    const double scale = base_cell_pixels * zoom;
    return {(screen.x - offset.x) / scale, (screen.y - offset.y) / scale};
}
std::optional<GridCell> StorageGridCamera::pick(GridPoint screen) const {
    const auto point = screen_to_grid(screen);
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0 || point.y < 0 ||
        point.x >= stored_grid_width || point.y >= stored_grid_height) return std::nullopt;
    return GridCell{static_cast<std::uint32_t>(std::floor(point.x)),
                    static_cast<std::uint32_t>(std::floor(point.y))};
}
void StorageGridCamera::zoom_at(GridPoint screen, double factor) {
    if (!std::isfinite(factor) || factor <= 0) return;
    const auto fixed = screen_to_grid(screen);
    zoom = std::clamp(zoom * factor, 0.25, 16.0);
    const double scale = base_cell_pixels * zoom;
    offset = {screen.x - fixed.x * scale, screen.y - fixed.y * scale};
}
void StorageGridCamera::center_on(GridPoint grid) {
    const double scale = base_cell_pixels * zoom;
    offset = {viewport_width * 0.5 - grid.x * scale,
              viewport_height * 0.5 - grid.y * scale};
}

std::array<std::uint8_t, 4> raw_value_color(std::uint32_t value) {
    std::uint32_t h = value ^ 0x9e3779b9U;
    h ^= h >> 16U; h *= 0x7feb352dU;
    h ^= h >> 15U; h *= 0x846ca68bU;
    h ^= h >> 16U;
    return {static_cast<std::uint8_t>(48U + (h & 0xbfU)),
            static_cast<std::uint8_t>(48U + ((h >> 8U) & 0xbfU)),
            static_cast<std::uint8_t>(48U + ((h >> 16U) & 0xbfU)), 255};
}
std::vector<std::uint8_t> make_storage_rgba(const ParsedEmperorMap& map, RawLayer layer) {
    const auto& values = layer == RawLayer::Terrain ? map.terrain_raw.values : map.objects_raw.values;
    if (values.size() != static_cast<std::size_t>(stored_grid_width) * stored_grid_height)
        throw std::runtime_error("raw layer does not contain a complete 228x228 grid");
    std::vector<std::uint8_t> pixels(values.size() * 4U);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto color = raw_value_color(values[i]);
        std::copy(color.begin(), color.end(), pixels.begin() + static_cast<std::ptrdiff_t>(i * 4U));
    }
    return pixels;
}

} // namespace openemperor::maps
