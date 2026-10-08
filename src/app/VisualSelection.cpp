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
    result.fire_fallback_reason=compatibility.compatible() ? compatibility.fire_detail:
        compatibility.detail.empty() ? "unsupported original data revision":
        "unsupported original data: "+compatibility.detail;
    result.fire_inspector_fallback_reason=compatibility.compatible() ? compatibility.fire_inspector_detail:
        compatibility.detail.empty() ? "unsupported original data revision":
        "unsupported original data: "+compatibility.detail;
    result.market_walker_fallback_reason=compatibility.compatible() ? compatibility.market_walker_detail:
        compatibility.detail.empty() ? "unsupported original data revision":
        "unsupported original data: "+compatibility.detail;
    if (compatibility.compatible()) {
        result.walker = compatibility.profile->walker_profile;
        result.building = compatibility.profile->building_profile;
        result.road = compatibility.profile->road_profile;
        result.walker_source = result.building_source = result.road_source =
            VisualProfileSource::Builtin;
        if (compatibility.fire_compatible()) {
            result.fire=compatibility.profile->fire_profile;
            result.fire_source=VisualProfileSource::Builtin;
            result.fire_fallback_reason.clear();
        }
        if (compatibility.fire_inspector_compatible()) {
            result.fire_inspector=compatibility.profile->fire_inspector_profile;
            result.fire_inspector_source=VisualProfileSource::Builtin;
            result.fire_inspector_fallback_reason.clear();
        }
        if (compatibility.market_walker_compatible()) {
            result.market_walker=compatibility.profile->market_walker_profile;
            result.market_walker_source=VisualProfileSource::Builtin;
            result.market_walker_fallback_reason.clear();
        }
    }
    if (!custom_walker.empty()) {
        result.walker = custom_walker;
        result.walker_source = VisualProfileSource::Custom;
        result.fire_inspector.clear();
        result.fire_inspector_source=VisualProfileSource::Custom;
        result.fire_inspector_fallback_reason=
            "Custom walker profile selected; no built-in FireInspector supplement";
        result.market_walker.clear();
        result.market_walker_source=VisualProfileSource::Custom;
        result.market_walker_fallback_reason=
            "Custom walker profile selected; no built-in Food/Market supplement";
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
