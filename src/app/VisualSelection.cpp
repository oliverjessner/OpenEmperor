#include "app/VisualSelection.h"

namespace openemperor {

const char* visual_profile_source_name(VisualProfileSource source) {
    switch (source) {
    case VisualProfileSource::Fallback: return "fallback";
    case VisualProfileSource::Builtin: return "builtin";
    case VisualProfileSource::Custom: return "custom";
    }
    return "fallback";
}

VisualSelection select_visual_profiles(const assets::CompatibilityResult& compatibility,
                                       const std::filesystem::path& custom_walker,
                                       const std::filesystem::path& custom_building,
                                       const std::filesystem::path& custom_road) {
    VisualSelection result;
    result.compatibility = compatibility;
    if (compatibility.compatible()) {
        result.walker = compatibility.profile->walker_profile;
        result.building = compatibility.profile->building_profile;
        result.road = compatibility.profile->road_profile;
        result.walker_source = result.building_source = result.road_source =
            VisualProfileSource::Builtin;
    }
    if (!custom_walker.empty()) {
        result.walker = custom_walker;
        result.walker_source = VisualProfileSource::Custom;
    }
    if (!custom_building.empty()) {
        result.building = custom_building;
        result.building_source = VisualProfileSource::Custom;
    }
    if (!custom_road.empty()) {
        result.road = custom_road;
        result.road_source = VisualProfileSource::Custom;
    }
    return result;
}

VisualSelection detect_and_select_visual_profiles(const std::filesystem::path& data_root,
                                                  const std::filesystem::path& resource_root,
                                                  const std::filesystem::path& custom_walker,
                                                  const std::filesystem::path& custom_building,
                                                  const std::filesystem::path& custom_road) {
    const auto compatibility = assets::detect_compatibility(data_root,
        resource_root.empty() ? std::filesystem::path{} : resource_root / "Compatibility");
    return select_visual_profiles(compatibility, custom_walker, custom_building, custom_road);
}

} // namespace openemperor
