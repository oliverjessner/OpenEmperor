#pragma once

#include "maps/EmperorMap.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace openemperor::maps {

// Reference-derived minimap conditions. Categories and colors are our diagnostics,
// not verified original-game terrain IDs or palette entries.
enum class TerrainCategory {
    OffMap, Flood, Vegetation, Bamboo, Rock, Monument, Structure,
    Water, Elevation, Road, Fertile, OtherMarked, Empty, Unknown
};
const char* category_name(TerrainCategory category);
struct TerrainCellInterpretation {
    std::uint32_t terrain_raw = 0;
    std::uint32_t objects_raw = 0;
    std::uint32_t recognized_terrain_flags = 0;
    std::uint32_t unknown_terrain_bits = 0;
    std::uint32_t recognized_object_flags = 0;
    std::uint32_t unknown_object_bits = 0;
    TerrainCategory category = TerrainCategory::Unknown;
    std::string_view rule;
    bool partial = false;
    static constexpr std::string_view evidence = "reference-derived; original game unverified";
};
TerrainCellInterpretation interpret_terrain(std::uint32_t terrain, std::uint32_t objects);
std::vector<TerrainCellInterpretation> interpret_map(const ParsedEmperorMap& map);
std::array<std::uint8_t, 4> semantic_color(TerrainCategory category);

} // namespace openemperor::maps
