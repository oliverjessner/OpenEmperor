#pragma once
#include "assets/AssetCatalog.h"
#include "assets/Sg3RgbaDecoder.h"
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {
enum class BuildingVisualRole {
    ClaySource, Pottery, Warehouse, Household, Farm, ServicePost, Market, FireWatch,
    HouseholdLevel0, HouseholdLevel1, HouseholdLevel2, Well, HealthPost
};
constexpr std::array building_roles{
    BuildingVisualRole::ClaySource,BuildingVisualRole::Pottery,
    BuildingVisualRole::Warehouse,BuildingVisualRole::Household,
    BuildingVisualRole::Farm,BuildingVisualRole::ServicePost,BuildingVisualRole::Market,
    BuildingVisualRole::FireWatch,BuildingVisualRole::HouseholdLevel0,
    BuildingVisualRole::HouseholdLevel1,BuildingVisualRole::HouseholdLevel2,BuildingVisualRole::Well,BuildingVisualRole::HealthPost};
constexpr std::array household_stage_roles{
    BuildingVisualRole::HouseholdLevel0,BuildingVisualRole::HouseholdLevel1,
    BuildingVisualRole::HouseholdLevel2};
constexpr std::size_t building_role_count=building_roles.size();
constexpr bool is_household_stage(BuildingVisualRole role) {
    return role==BuildingVisualRole::HouseholdLevel0 ||
           role==BuildingVisualRole::HouseholdLevel1 || role==BuildingVisualRole::HouseholdLevel2;
}
constexpr bool requires_one_cell(BuildingVisualRole role) {
    return role==BuildingVisualRole::Farm || role==BuildingVisualRole::ServicePost ||
           role==BuildingVisualRole::Market || role==BuildingVisualRole::FireWatch;
}
const char* building_role_name(BuildingVisualRole role);
constexpr std::size_t role_index(BuildingVisualRole role) { return static_cast<std::size_t>(role); }

struct BuildingVisualEntry {
    AssetId id; // Physical SG3 record.
    std::size_t image_index=0; // Index into deduplicated decoded images.
    double ground_x=0,ground_y=0; // Decoded-image pixels from top-left.
    std::uint8_t footprint_side=0; // Verified Type-30 square base geometry.
    std::string evidence;
};
struct BuildingVisualProfile {
    std::array<std::optional<BuildingVisualEntry>,building_role_count> entries;
    std::vector<RgbaImage> unique_images;
    const BuildingVisualEntry* find(BuildingVisualRole role) const {
        const auto& entry=entries[role_index(role)];
        return entry ? &*entry:nullptr;
    }
    BuildingVisualRole household_role(unsigned level) const {
        const auto stage=household_stage_roles.at(level);
        return find(stage) ? stage:BuildingVisualRole::Household;
    }
};

BuildingVisualProfile load_building_visual_profile(const std::filesystem::path& data_root,
                                                   const std::filesystem::path& manifest);
}
