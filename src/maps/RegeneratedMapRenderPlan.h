#pragma once
#include "maps/LandscapeSelectors.h"
#include "maps/StoredGraphicsPlan.h"

namespace openemperor::maps {
struct RegeneratedCell {
    LandscapeSelection selection;
    std::optional<PackedGraphicId> graphic;
    std::optional<std::size_t> asset_index;
    const char* fallback="historical snapshot preview";
};
// Published const once per load. Historical cells, footprints and buildability
// remain untouched. Indices refer to the renderer's shared physical asset pool.
struct RegeneratedMapRenderPlan {
    std::vector<RegeneratedCell> cells; // Same candidate order as historical plan.
    std::vector<std::optional<std::size_t>> footprint_assets;
    std::size_t historical_asset_count=0;
    double build_milliseconds=0;
};
void build_regenerated_map_render_plan(StoredGraphicsPlan& historical,
    const StoredArchiveRegistrations& registrations);
std::optional<PackedGraphicId> resolve_landscape_variant(ResourceGroupKey key,
    unsigned variant,const std::map<std::uint32_t,GroupRegistration>& registrations);
} // namespace openemperor::maps
