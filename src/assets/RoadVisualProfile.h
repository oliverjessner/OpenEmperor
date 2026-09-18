#pragma once
#include "assets/AssetCatalog.h"
#include "assets/Sg3RgbaDecoder.h"
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {
struct RoadVisualEntry {
    AssetId id; // Physical SG3 record, without runtime ID translation.
    std::size_t image_index=0;
    double ground_x=0,ground_y=0;
    std::string evidence;
};
struct RoadVisualProfile {
    std::array<std::optional<RoadVisualEntry>,16> tiles;
    std::vector<RgbaImage> unique_images;
    const RoadVisualEntry* find(std::uint8_t mask) const {
        if (mask>=tiles.size()) return nullptr;
        const auto& value=tiles[mask];return value ? &*value:nullptr;
    }
    std::size_t configured_count() const;
};
RoadVisualProfile load_road_visual_profile(const std::filesystem::path& data_root,
                                           const std::filesystem::path& manifest);
}
