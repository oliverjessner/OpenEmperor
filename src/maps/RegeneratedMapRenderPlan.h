#pragma once
#include "maps/LandscapeSelectors.h"
#include "maps/GreatWallSelector.h"
#include "maps/LandscapeInstances.h"
#include "maps/StoredGraphicsPlan.h"

namespace openemperor::maps {
// File-format Base/Overlay components do not determine their scene roles.
// Structural bodies stay together at one spatial painter key; the existing
// landscape path keeps its early base and spatial overlay convention.
enum class LandscapeCompositionPolicy { EarlyBaseSpatialOverlay, SpatialCombined };
inline const char* landscape_composition_policy_name(LandscapeCompositionPolicy policy) {
    switch (policy) {
    case LandscapeCompositionPolicy::EarlyBaseSpatialOverlay: return "split_base_spatial_overlay";
    case LandscapeCompositionPolicy::SpatialCombined: return "spatial_combined";
    }
    return "invalid";
}
struct RegeneratedCell {
    LandscapeSelection selection;
    std::optional<PackedGraphicId> graphic;
    std::optional<std::size_t> asset_index;
    std::optional<std::size_t> instance_index;
    const char* fallback="historical snapshot preview";
};
struct RegeneratedLandscapeInstance {
    LandscapeInstanceSpec geometry;
    PackedGraphicId graphic;
    std::size_t asset_index=0;
    std::vector<std::size_t> cell_indices;
    // Images belonging to one restored composition activate together after
    // eager decoding. Unset preserves existing independent instances.
    std::optional<std::size_t> composition_group;
    std::optional<GreatWallRestoreContext> great_wall_context;
    LandscapeCompositionPolicy composition_policy=LandscapeCompositionPolicy::EarlyBaseSpatialOverlay;
};
// Published const once per load. Historical cells, footprints and buildability
// remain untouched. Indices refer to the renderer's shared physical asset pool.
struct RegeneratedMapRenderPlan {
    std::vector<RegeneratedCell> cells; // Same candidate order as historical plan.
    std::vector<std::optional<std::size_t>> footprint_assets;
    std::vector<RegeneratedLandscapeInstance> instances;
    // Load-time refusal reason for a selected original piece that cannot
    // publish a complete image. Provenance only; never grants ownership.
    std::map<std::size_t,std::string> original_wall_fallbacks;
    std::size_t historical_asset_count=0;
    double build_milliseconds=0;
};
void build_regenerated_map_render_plan(StoredGraphicsPlan& historical,
    const StoredArchiveRegistrations& registrations);
std::optional<PackedGraphicId> resolve_landscape_variant(ResourceGroupKey key,
    unsigned variant,const std::map<std::uint32_t,GroupRegistration>& registrations);
} // namespace openemperor::maps
