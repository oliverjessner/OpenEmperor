#include "maps/MapVisualization.h"

#include <algorithm>
#include <stdexcept>

namespace openemperor::maps {
const char* view_name(MapViewMode view) {
    switch (view) {
    case MapViewMode::Storage: return "storage_raw";
    case MapViewMode::Semantic: return "semantic_storage";
    case MapViewMode::Projected: return "projected_reference";
    }
    return "unknown";
}
const char* mask_name(MaskMode mask) {
    switch (mask) {
    case MaskMode::Full: return "full";
    case MaskMode::Candidate: return "candidate";
    case MaskMode::OffMap: return "offmap_bit";
    case MaskMode::Compare: return "compare";
    }
    return "unknown";
}
DebugPixels make_map_debug_pixels(const ParsedEmperorMap& map,
                                  const std::vector<TerrainCellInterpretation>& interpreted,
                                  const MapGeometry& geometry, MapViewMode view,
                                  RawLayer raw_layer, MaskMode mask) {
    const auto cells = static_cast<std::size_t>(stored_grid_width) * stored_grid_height;
    if (interpreted.size() != cells || map.terrain_raw.values.size() != cells ||
        map.objects_raw.values.size() != cells)
        throw std::invalid_argument("incomplete map debug input");
    if ((view == MapViewMode::Projected || mask == MaskMode::Candidate || mask == MaskMode::Compare) &&
        !geometry.supported) throw std::invalid_argument("mask/projected view requires supported geometry");
    DebugPixels result;
    result.width = view == MapViewMode::Projected ? geometry.declared_size : stored_grid_width;
    result.height = view == MapViewMode::Projected ? geometry.declared_size : stored_grid_height;
    result.rgba.resize(static_cast<std::size_t>(result.width) * result.height * 4U);
    for (std::uint32_t y = 0; y < result.height; ++y) {
        for (std::uint32_t x = 0; x < result.width; ++x) {
            const auto cell = view == MapViewMode::Projected ? geometry.at_projected({x,y}) :
                std::optional<GridCell>{GridCell{x,y}};
            std::array<std::uint8_t,4> color{25, 28, 35, 255};
            if (cell) {
                const auto i = static_cast<std::size_t>(cell->y) * stored_grid_width + cell->x;
                const auto& item = interpreted[i];
                color = view == MapViewMode::Storage ?
                    raw_value_color(raw_layer == RawLayer::Terrain ? item.terrain_raw : item.objects_raw) :
                    semantic_color(item.category);
                const bool candidate = geometry.contains(*cell);
                const bool offmap = (item.terrain_raw & 0x80000U) != 0;
                switch (mask) {
                case MaskMode::Full: break;
                case MaskMode::Candidate:
                    if (!candidate) for (int channel = 0; channel < 3; ++channel)
                        color[static_cast<std::size_t>(channel)] = static_cast<std::uint8_t>(color[static_cast<std::size_t>(channel)] / 4U);
                    break;
                case MaskMode::OffMap:
                    color = offmap ? std::array<std::uint8_t,4>{255, 57, 157, 255} :
                                     std::array<std::uint8_t,4>{45, 53, 66, 255};
                    break;
                case MaskMode::Compare:
                    if (candidate && offmap) color = {255, 58, 170, 255};
                    else if (candidate) color = {71, 177, 97, 255};
                    else if (offmap) color = {49, 71, 128, 255};
                    else color = {255, 181, 43, 255};
                    break;
                }
            }
            const auto out = (static_cast<std::size_t>(y) * result.width + x) * 4U;
            std::copy(color.begin(), color.end(), result.rgba.begin() + static_cast<std::ptrdiff_t>(out));
        }
    }
    return result;
}
} // namespace openemperor::maps
