#pragma once

#include "maps/MapGeometry.h"
#include "maps/TerrainBindings.h"
#include "scene/IsoProjection.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::maps {

enum class PreviewStatus { CuratedPreview, Unmapped, ExcludedByPreviewMask, AssetError };
const char* preview_status_name(PreviewStatus status);

struct TerrainInstance {
    GridCell storage;
    scene::Point world;
    scene::Point image_origin;
    TerrainCellInterpretation interpretation;
    PreviewStatus status = PreviewStatus::Unmapped;
    std::string binding_id;
    std::string asset_alias;
};

struct TerrainPlanCounts {
    std::size_t candidate = 0;
    std::size_t bound = 0;
    std::size_t unmapped = 0;
    std::size_t excluded = 0;
    std::size_t asset_error = 0;
    std::map<std::string, std::size_t> by_binding;
};

struct TerrainRenderPlan {
    std::vector<TerrainInstance> instances; // Every candidate storage cell exactly once; painter order.
    std::vector<PreviewStatus> status_by_storage; // Includes excluded storage cells.
    TerrainPlanCounts counts;
    std::uint32_t border = 0;
};

scene::Point terrain_world(GridCell cell, std::uint32_t border);
scene::Point terrain_ground(GridCell cell, std::uint32_t border);
scene::Point terrain_image_origin(scene::Point world);
std::optional<GridCell> pick_terrain_cell(scene::Point world, const MapGeometry& geometry);
TerrainRenderPlan make_terrain_render_plan(const ParsedEmperorMap& map,
                                           const MapGeometry& geometry,
                                           const TerrainBindings& bindings);

} // namespace openemperor::maps
