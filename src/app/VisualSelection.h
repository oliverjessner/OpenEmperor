#pragma once

#include "assets/CompatibilityProfile.h"

#include <filesystem>

namespace openemperor {

enum class VisualProfileSource { Fallback, Builtin, Custom };
const char* visual_profile_source_name(VisualProfileSource source);

struct VisualSelection {
    assets::CompatibilityResult compatibility;
    std::filesystem::path walker;
    std::filesystem::path building;
    std::filesystem::path road;
    VisualProfileSource walker_source = VisualProfileSource::Fallback;
    VisualProfileSource building_source = VisualProfileSource::Fallback;
    VisualProfileSource road_source = VisualProfileSource::Fallback;
};

VisualSelection select_visual_profiles(const assets::CompatibilityResult& compatibility,
                                       const std::filesystem::path& custom_walker = {},
                                       const std::filesystem::path& custom_building = {},
                                       const std::filesystem::path& custom_road = {});

VisualSelection detect_and_select_visual_profiles(
    const std::filesystem::path& data_root,
    const std::filesystem::path& resource_root,
    const std::filesystem::path& custom_walker = {},
    const std::filesystem::path& custom_building = {},
    const std::filesystem::path& custom_road = {});

} // namespace openemperor
