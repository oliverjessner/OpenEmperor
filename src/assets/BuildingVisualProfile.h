#pragma once
#include "assets/AssetCatalog.h"
#include "assets/Sg3RgbaDecoder.h"
#include <filesystem>
#include <string>

namespace openemperor::assets {
struct BuildingVisualProfile {
    AssetId pottery_id; // Physical SG3 record.
    RgbaImage pottery_image;
    double ground_x=0,ground_y=0; // Decoded-image pixels from top-left.
    std::string evidence;
};

BuildingVisualProfile load_building_visual_profile(const std::filesystem::path& data_root,
                                                   const std::filesystem::path& manifest);
}
