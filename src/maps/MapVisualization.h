#pragma once

#include "maps/MapGeometry.h"
#include "maps/TerrainInterpretation.h"

#include <cstdint>
#include <vector>

namespace openemperor::maps {

enum class MapViewMode { Storage, Semantic, Projected, Textured };
enum class MaskMode { Full, Candidate, OffMap, Compare };
const char* view_name(MapViewMode view);
const char* mask_name(MaskMode mask);
struct DebugPixels {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
DebugPixels make_map_debug_pixels(const ParsedEmperorMap& map,
                                  const std::vector<TerrainCellInterpretation>& interpreted,
                                  const MapGeometry& geometry, MapViewMode view,
                                  RawLayer raw_layer, MaskMode mask);

} // namespace openemperor::maps
