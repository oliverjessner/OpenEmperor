#pragma once

#include "assets/AssetCatalog.h"
#include "maps/TerrainInterpretation.h"

#include <filesystem>
#include <map>
#include <string>
#include <utility>

namespace openemperor::maps {

struct TerrainPreviewAsset {
    assets::AssetId id;
    std::filesystem::path archive_path;
    std::string provenance;
};

struct TerrainBinding {
    std::string id;
    std::string asset_alias;
    std::string expected_category;
    std::string expected_rule;
    std::string provenance;
};

struct TerrainBindings {
    std::string profile;
    std::map<std::string, TerrainPreviewAsset> assets;
    std::map<std::pair<std::uint32_t, std::uint32_t>, TerrainBinding> exact;
};

// Curated local preview data. All asset paths and their derived .555 sources must stay
// under data_root. Parsing and metadata validation do not depend on SDL.
TerrainBindings load_terrain_bindings(const std::filesystem::path& data_root,
                                      const std::filesystem::path& json_path);

} // namespace openemperor::maps
