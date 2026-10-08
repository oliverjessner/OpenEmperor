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
    std::filesystem::path fire;
    VisualProfileSource walker_source = VisualProfileSource::Fallback;
    VisualProfileSource building_source = VisualProfileSource::Fallback;
    VisualProfileSource road_source = VisualProfileSource::Fallback;
    VisualProfileSource fire_source = VisualProfileSource::Fallback;
    std::string fire_fallback_reason;
    std::filesystem::path fire_inspector;
    VisualProfileSource fire_inspector_source = VisualProfileSource::Fallback;
    std::string fire_inspector_fallback_reason;
    std::filesystem::path market_walker;
    VisualProfileSource market_walker_source = VisualProfileSource::Fallback;
    std::string market_walker_fallback_reason;
    std::filesystem::path service_walker;
    VisualProfileSource service_walker_source = VisualProfileSource::Fallback;
    std::string service_walker_fallback_reason;
    std::filesystem::path health_walker;
    VisualProfileSource health_walker_source = VisualProfileSource::Fallback;
    std::string health_walker_fallback_reason;
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
