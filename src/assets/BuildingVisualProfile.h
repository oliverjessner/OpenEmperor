#pragma once
#include "assets/AssetCatalog.h"
#include "assets/Sg3RgbaDecoder.h"
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {
enum class BuildingVisualRole { ClaySource, Pottery, Warehouse, Household };
constexpr std::size_t building_role_count=4;
constexpr std::array<BuildingVisualRole,building_role_count> building_roles{
    BuildingVisualRole::ClaySource,BuildingVisualRole::Pottery,
    BuildingVisualRole::Warehouse,BuildingVisualRole::Household};
const char* building_role_name(BuildingVisualRole role);
constexpr std::size_t role_index(BuildingVisualRole role) { return static_cast<std::size_t>(role); }

struct BuildingVisualEntry {
    AssetId id; // Physical SG3 record.
    std::size_t image_index=0; // Index into deduplicated decoded images.
    double ground_x=0,ground_y=0; // Decoded-image pixels from top-left.
    std::string evidence;
};
struct BuildingVisualProfile {
    std::array<std::optional<BuildingVisualEntry>,building_role_count> entries;
    std::vector<RgbaImage> unique_images;
    const BuildingVisualEntry* find(BuildingVisualRole role) const {
        const auto& entry=entries[role_index(role)];
        return entry ? &*entry:nullptr;
    }
};

BuildingVisualProfile load_building_visual_profile(const std::filesystem::path& data_root,
                                                   const std::filesystem::path& manifest);
}
