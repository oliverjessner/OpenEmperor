#include "maps/TerrainInterpretation.h"

#include <stdexcept>

namespace openemperor::maps {
namespace {
constexpr std::uint32_t known =
    0x00080000U | 0x00000001U | 0x00000002U | 0x00000004U |
    0x00000008U | 0x00000020U | 0x00000040U | 0x00000080U |
    0x00000100U | 0x00000200U | 0x00000800U | 0x00001000U |
    0x00004000U | 0x00010000U | 0x00020000U | 0x00040000U |
    0x00100000U | 0x00200000U | 0x02000000U | 0x04000000U |
    0x10000000U | 0x80000000U;
}
const char* category_name(TerrainCategory category) {
    switch (category) {
    case TerrainCategory::OffMap: return "off_map";
    case TerrainCategory::Flood: return "flood";
    case TerrainCategory::Vegetation: return "vegetation";
    case TerrainCategory::Bamboo: return "bamboo";
    case TerrainCategory::Rock: return "rock_or_ore";
    case TerrainCategory::Monument: return "monument";
    case TerrainCategory::Structure: return "structure";
    case TerrainCategory::Water: return "water";
    case TerrainCategory::Elevation: return "elevation_hint";
    case TerrainCategory::Road: return "road";
    case TerrainCategory::Fertile: return "fertile_hint";
    case TerrainCategory::OtherMarked: return "other_marked";
    case TerrainCategory::Empty: return "empty_by_reference";
    case TerrainCategory::Unknown: return "unknown";
    }
    return "unknown";
}
TerrainCellInterpretation interpret_terrain(std::uint32_t terrain, std::uint32_t objects) {
    TerrainCellInterpretation out;
    out.terrain_raw = terrain;
    out.objects_raw = objects;
    out.recognized_terrain_flags = terrain & known;
    out.unknown_terrain_bits = terrain & ~known;
    // The reference uses object bit 1 only while classifying tree/bamboo.
    out.recognized_object_flags = (terrain & 1U) ? (objects & 2U) : 0U;
    out.unknown_object_bits = objects & ~out.recognized_object_flags;
    auto choose = [&](TerrainCategory category, std::string_view rule, bool incomplete = false) {
        out.category = category;
        out.rule = rule;
        out.partial = incomplete || out.unknown_terrain_bits != 0 || out.unknown_object_bits != 0;
    };
    if (terrain & 0x80000U) choose(TerrainCategory::OffMap, "offmap_bit");
    else if ((terrain & 0x104U) == 0x100U) choose(TerrainCategory::Flood, "flood_without_water");
    else if (terrain & 1U) choose((objects & 2U) ? TerrainCategory::Bamboo : TerrainCategory::Vegetation,
                                 (objects & 2U) ? "tree_object_bamboo_bit" : "tree_bit");
    else if (terrain & 2U) choose(TerrainCategory::Rock, "rock_or_ore_bits", (terrain & 0x300000U) != 0);
    else if (terrain & 0x101000U) choose(TerrainCategory::Rock, "ruins_before_buildings", true);
    else if (terrain & 0x10000000U) choose(TerrainCategory::Monument, "monument_or_canal_bit", true);
    else if (terrain & 8U) choose(TerrainCategory::Structure, "building_bit", true);
    else if ((terrain & 0x44U) == 0x4U || (terrain & 0x104U) == 0x104U)
        choose(TerrainCategory::Water, "water_without_road_or_with_flood", (terrain & 0x4000000U) != 0);
    else if (terrain & 0x20000U) choose(TerrainCategory::Rock, "quarry_bit_without_depth", true);
    else if (terrain & 0x200U) choose(TerrainCategory::Elevation, "elevation_bit_without_height", true);
    else if (terrain & 0x20U) choose(TerrainCategory::OtherMarked, "garden_bit", true);
    else if (terrain & 0x40U) choose(TerrainCategory::Road, "road_bit");
    else if (terrain & 0x800U) choose(TerrainCategory::OtherMarked, "irrigation_bit", true);
    else if (terrain & 0x4000U) choose(TerrainCategory::OtherMarked, "wall_bit", true);
    else if (terrain & 0x10000U) choose(TerrainCategory::OtherMarked, "beach_bit", true);
    else if (terrain & 0x40000U) choose(TerrainCategory::OtherMarked, "marsh_bit", true);
    else if (terrain & 0x2000000U) choose(TerrainCategory::OtherMarked, "pinnacle_bit", true);
    else if (terrain & 0x80000000U) choose(TerrainCategory::OtherMarked, "sand_bit", true);
    else if (terrain & 0x80U) choose(TerrainCategory::Fertile, "fertility_bit_without_value", true);
    else if (terrain == 0) choose(TerrainCategory::Empty, "no_terrain_flags");
    else choose(TerrainCategory::Unknown, "unclassified_combination", true);
    return out;
}
std::vector<TerrainCellInterpretation> interpret_map(const ParsedEmperorMap& map) {
    const auto cells = static_cast<std::size_t>(stored_grid_width) * stored_grid_height;
    if (map.terrain_raw.values.size() != cells || map.objects_raw.values.size() != cells)
        throw std::invalid_argument("map interpretation requires complete raw grids");
    std::vector<TerrainCellInterpretation> result;
    result.reserve(cells);
    for (std::size_t i = 0; i < cells; ++i)
        result.push_back(interpret_terrain(map.terrain_raw.values[i], map.objects_raw.values[i]));
    return result;
}
std::array<std::uint8_t, 4> semantic_color(TerrainCategory category) {
    switch (category) {
    case TerrainCategory::OffMap: return {48, 45, 55, 255};
    case TerrainCategory::Flood: return {56, 143, 187, 255};
    case TerrainCategory::Vegetation: return {31, 116, 56, 255};
    case TerrainCategory::Bamboo: return {99, 165, 56, 255};
    case TerrainCategory::Rock: return {133, 126, 113, 255};
    case TerrainCategory::Monument: return {151, 94, 143, 255};
    case TerrainCategory::Structure: return {184, 126, 86, 255};
    case TerrainCategory::Water: return {36, 92, 176, 255};
    case TerrainCategory::Elevation: return {153, 137, 104, 255};
    case TerrainCategory::Road: return {179, 151, 109, 255};
    case TerrainCategory::Fertile: return {126, 177, 82, 255};
    case TerrainCategory::OtherMarked: return {181, 91, 182, 255};
    case TerrainCategory::Empty: return {203, 188, 141, 255};
    case TerrainCategory::Unknown: return {255, 0, 200, 255};
    }
    return {255, 0, 200, 255};
}
} // namespace openemperor::maps
