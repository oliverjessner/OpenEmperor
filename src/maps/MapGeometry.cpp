#include "maps/MapGeometry.h"

#include <algorithm>
#include <stdexcept>

namespace openemperor::maps {
namespace {
constexpr std::uint32_t supported_sizes[]{84, 112, 140, 170, 226};
constexpr std::uint32_t offmap_bit = 0x00080000U;
std::size_t index(GridCell cell) { return static_cast<std::size_t>(cell.y) * stored_grid_width + cell.x; }
} // namespace

MapGeometry::MapGeometry(std::uint32_t size) : declared_size(size) {
    supported = std::find(std::begin(supported_sizes), std::end(supported_sizes), size) != std::end(supported_sizes);
    if (!supported) return;
    border = (stored_grid_width - size) / 2U;
    candidate.assign(static_cast<std::size_t>(stored_grid_width) * stored_grid_height, 0);
    projected_pixels.resize(static_cast<std::size_t>(size) * size);
    const int half = static_cast<int>(stored_grid_width / 2U);
    const int edge = static_cast<int>(border);
    const int end_y = edge + static_cast<int>(size);
    for (int y = edge; y < end_y; ++y) {
        const int start = y < half ? edge + half - y - 1 : edge + y - half;
        const int stop = y < half ? half + y + 1 - edge : 3 * half - y - edge;
        for (int x = start; x < stop; ++x) {
            if (x < 0 || x >= static_cast<int>(stored_grid_width))
                throw std::runtime_error("reference-derived candidate exceeds storage grid");
            const auto cell = GridCell{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
            candidate[index(cell)] = 1;
            const int local_x = x - edge, local_y = y - edge;
            const int px = static_cast<int>(size / 2U) + local_x - local_y - 1;
            const int py = 1 + local_x + local_y - static_cast<int>(size / 2U);
            for (int dx = 0; dx != 2; ++dx) {
                if (px + dx < 0 || px + dx >= static_cast<int>(size) || py < 0 || py >= static_cast<int>(size))
                    continue; // Reference bitmap writes are clipped at its m x m edge.
                auto& target = projected_pixels[static_cast<std::size_t>(py) * size + static_cast<std::size_t>(px + dx)];
                if (target) throw std::runtime_error("reference-derived bitmap coordinates overlap");
                target = cell;
            }
        }
    }
}
bool MapGeometry::contains(GridCell cell) const {
    return supported && cell.x < stored_grid_width && cell.y < stored_grid_height && candidate[index(cell)] != 0;
}
std::optional<GridCell> MapGeometry::at_projected(BitmapPixel pixel) const {
    if (!supported || pixel.x >= declared_size || pixel.y >= declared_size) return std::nullopt;
    return projected_pixels[static_cast<std::size_t>(pixel.y) * declared_size + pixel.x];
}
std::optional<BitmapPixel> MapGeometry::projected_origin(GridCell cell) const {
    if (!contains(cell)) return std::nullopt;
    const int lx = static_cast<int>(cell.x) - static_cast<int>(border);
    const int ly = static_cast<int>(cell.y) - static_cast<int>(border);
    const int px = static_cast<int>(declared_size / 2U) + lx - ly - 1;
    const int py = 1 + lx + ly - static_cast<int>(declared_size / 2U);
    if (py < 0 || py >= static_cast<int>(declared_size)) return std::nullopt;
    const int visible_x = std::max(0, px);
    if (visible_x >= static_cast<int>(declared_size) || visible_x > px + 1) return std::nullopt;
    return BitmapPixel{static_cast<std::uint32_t>(visible_x), static_cast<std::uint32_t>(py)};
}
MaskComparison compare_masks(const ParsedEmperorMap& map, const MapGeometry& geometry) {
    if (!geometry.supported) throw std::invalid_argument("unsupported map geometry");
    if (map.terrain_raw.values.size() != geometry.candidate.size())
        throw std::invalid_argument("incomplete terrain grid");
    MaskComparison out;
    for (std::uint32_t y = 0; y < stored_grid_height; ++y) {
        for (std::uint32_t x = 0; x < stored_grid_width; ++x) {
            const GridCell cell{x,y};
            const bool candidate = geometry.contains(cell);
            const bool offmap = (map.terrain_at(x,y) & offmap_bit) != 0;
            if (candidate && offmap) ++out.candidate_and_offmap;
            else if (candidate) ++out.candidate_and_onmap;
            else if (offmap) ++out.outside_and_offmap;
            else ++out.outside_and_onmap;
            if (candidate == offmap && out.mismatch_examples.size() < 16) out.mismatch_examples.push_back(cell);
        }
    }
    return out;
}

} // namespace openemperor::maps
