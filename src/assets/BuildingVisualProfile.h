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
// Independent Original-only presentation identities. The historical core role
// values above and House stage table remain unchanged.
enum class OriginalBuildingVisualRole {
    WaterWell, Acupuncture, MusicSchool, AcrobatSchool, DramaSchool, DaoistShrine,
    BuddhistShrine, ConfucianAcademy, PaperMaker, Weaver,
    HempFarm, Farmhouse, HempField, WheatField, MilletField
};
struct OriginalBuildingVisualIdentity {
    OriginalBuildingVisualRole role;
    std::uint16_t model_type;
    std::uint32_t group_key;
    std::uint8_t footprint_side;
    // A known model/group registration alone does not reconstruct a stateful
    // field selector. Such roles stay outside the historical static-body path;
    // schema3 requires a separate explicitly authored field-stage policy.
    bool static_base_supported=true;
};
inline constexpr std::array original_building_visual_identities{
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::WaterWell,72,0x482,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::Acupuncture,208,0x485,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::MusicSchool,211,0x412,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::AcrobatSchool,212,0x410,3},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::DramaSchool,213,0x411,3},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::DaoistShrine,215,0x49b,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::BuddhistShrine,217,0x49c,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::ConfucianAcademy,219,0x49f,3},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::PaperMaker,45,0x44a,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::Weaver,47,0x446,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::HempFarm,192,0x4ad,2},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::Farmhouse,193,0x43a,3},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::HempField,194,0x431,1,false},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::WheatField,195,0x42e,1,false},
    OriginalBuildingVisualIdentity{OriginalBuildingVisualRole::MilletField,196,0x42c,1,false}};
inline constexpr std::size_t original_building_schema1_role_count=10;
inline constexpr std::size_t original_building_role_count=original_building_visual_identities.size();
inline constexpr std::array original_field_roles{OriginalBuildingVisualRole::HempField,
    OriginalBuildingVisualRole::WheatField,OriginalBuildingVisualRole::MilletField};
inline constexpr std::size_t original_field_role_count=original_field_roles.size();
inline constexpr std::size_t original_field_stage_count=6;
inline constexpr const char* original_field_selection_contract="authored-farm-progress-six-canonical-0-v1";
constexpr std::optional<std::size_t> original_field_role_index(OriginalBuildingVisualRole role) {
    for (std::size_t index=0;index<original_field_roles.size();++index)
        if (original_field_roles[index]==role) return index;
    return std::nullopt;
}
// Presentation quantization of the existing authored80-productive-tick clock.
// This does not reconstruct Original field quantity, calendar or terrain state.
constexpr std::optional<std::uint8_t> authored_farm_progress_stage(int progress) {
    if (progress<0 || progress>=80) return std::nullopt;
    return static_cast<std::uint8_t>((6U*static_cast<unsigned>(progress))/80U);
}
constexpr const OriginalBuildingVisualIdentity* original_building_visual_identity(OriginalBuildingVisualRole role) {
    const auto index=static_cast<std::size_t>(role);
    return index<original_building_role_count ? &original_building_visual_identities[index]:nullptr;
}
constexpr std::optional<OriginalBuildingVisualRole> original_building_visual_role(std::uint16_t model_type) {
    for (const auto& identity:original_building_visual_identities)
        if (identity.model_type==model_type) return identity.role;
    return std::nullopt;
}
const char* original_building_role_name(OriginalBuildingVisualRole role);
inline constexpr std::uint16_t original_house_first_type=2;
inline constexpr std::size_t original_house_type_count=16;
inline constexpr std::uint64_t building_max_rgba_bytes=64U*1024U*1024U;
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
    // Optional, prepared only for active Original production. Legacy role
    // selection and its image pool remain exact when this is absent.
    bool original_house_prepared=false;
    std::array<std::optional<BuildingVisualEntry>,original_house_type_count> original_house_entries;
    std::array<std::string,original_house_type_count> original_house_fallbacks;
    bool original_building_prepared=false;
    std::array<std::optional<BuildingVisualEntry>,original_building_role_count> original_building_entries;
    std::array<std::string,original_building_role_count> original_building_fallbacks;
    // Schema3 only; kept distinct from historical static-body selection.
    bool original_field_prepared=false;
    std::array<std::array<std::optional<BuildingVisualEntry>,original_field_stage_count>,original_field_role_count> original_field_entries;
    const BuildingVisualEntry* find_original_field(OriginalBuildingVisualRole role,
            std::uint8_t actual_side,std::uint8_t stage) const {
        const auto index=original_field_role_index(role);
        if (!index || !original_field_prepared || !original_building_prepared || actual_side!=1 || stage>=original_field_stage_count)
            return nullptr;
        const auto& entry=original_field_entries[*index][stage];
        return entry && entry->footprint_side==actual_side ? &*entry:nullptr;
    }
    const BuildingVisualEntry* find_original_building(OriginalBuildingVisualRole role,std::uint8_t actual_side) const {
        const auto index=static_cast<std::size_t>(role);
        if (index>=original_building_role_count || !original_building_prepared) return nullptr;
        const auto& entry=original_building_entries[index];
        return entry && entry->footprint_side==actual_side ? &*entry:nullptr;
    }
    std::string fallback_original_building(OriginalBuildingVisualRole role,std::uint8_t actual_side) const {
        const auto index=static_cast<std::size_t>(role);
        if (index>=original_building_role_count) return "Unsupported Original building role";
        const auto name=std::string(original_building_role_name(role));
        if (!original_building_prepared) return name+": Original building profile unavailable; authored fallback";
        if (const auto& entry=original_building_entries[index];entry && entry->footprint_side!=actual_side)
            return name+": original sprite side "+std::to_string(entry->footprint_side)+
                " differs from actual side "+std::to_string(actual_side)+"; authored fallback";
        return original_building_fallbacks[index];
    }
    const BuildingVisualEntry* find_original_house(std::uint16_t type,std::uint8_t actual_side) const {
        if (type<original_house_first_type || std::size_t(type-original_house_first_type)>=original_house_type_count) return nullptr;
        const auto& entry=original_house_entries[std::size_t(type-original_house_first_type)];
        return original_house_prepared && entry && entry->footprint_side==actual_side ? &*entry:nullptr;
    }
    std::string fallback_original_house(std::uint16_t type,std::uint8_t actual_side) const {
        if (type<original_house_first_type || std::size_t(type-original_house_first_type)>=original_house_type_count)
            return "Unsupported original House type";
        const auto index=std::size_t(type-original_house_first_type);
        if (!original_house_prepared) return "Original House profile unavailable; authored fallback";
        if (const auto& entry=original_house_entries[index];entry && entry->footprint_side!=actual_side)
            return "Original sprite side "+std::to_string(entry->footprint_side)+
                " differs from actual side "+std::to_string(actual_side)+"; authored fallback";
        return original_house_fallbacks[index];
    }
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
// Atomic load-time extension. max_final_rgba bounds the complete combined
// physical image pool, including existing legacy entries. No runtime ID skip.
void append_original_house_visual_profile(const std::filesystem::path& data_root,
    const std::filesystem::path& manifest,BuildingVisualProfile& profile,
    std::uint64_t max_final_rgba=building_max_rgba_bytes);
// Complete native rasters only. Schema3 field illustrations quantize existing
// authored progress; Original growth/animation and gameplay authority are absent.
void append_original_building_visual_profile(const std::filesystem::path& data_root,
    const std::filesystem::path& manifest,BuildingVisualProfile& profile,
    std::uint64_t max_final_rgba=building_max_rgba_bytes);
}
