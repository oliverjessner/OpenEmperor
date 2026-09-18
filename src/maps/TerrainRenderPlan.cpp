#include "maps/TerrainRenderPlan.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace openemperor::maps {
const char* preview_status_name(PreviewStatus status) {
    switch (status) {
    case PreviewStatus::CuratedPreview: return "curated_preview";
    case PreviewStatus::Unmapped: return "unmapped";
    case PreviewStatus::ExcludedByPreviewMask: return "excluded_by_preview_mask";
    case PreviewStatus::AssetError: return "asset_error";
    }
    return "invalid_status";
}

scene::Point terrain_world(GridCell cell, std::uint32_t border) {
    const auto u = static_cast<double>(cell.x) - static_cast<double>(border);
    const auto v = static_cast<double>(cell.y) - static_cast<double>(border);
    return {(u - v) * 40.0, (u + v) * 20.0};
}
scene::Point terrain_ground(GridCell cell, std::uint32_t border) {
    const auto top = terrain_world(cell, border);
    return {top.x, top.y + 20.0};
}
scene::Point terrain_image_origin(scene::Point world) { return {world.x - 39.0, world.y}; }

std::optional<GridCell> pick_terrain_cell(scene::Point world, const MapGeometry& geometry) {
    if (!geometry.supported || !std::isfinite(world.x) || !std::isfinite(world.y)) return std::nullopt;
    // Half-open logical diamonds: the top/left boundary belongs to the cell,
    // while the bottom/right boundary belongs to its neighbor.
    const auto u = std::floor(world.x / 80.0 + world.y / 40.0);
    const auto v = std::floor(world.y / 40.0 - world.x / 80.0);
    const auto x = u + static_cast<double>(geometry.border);
    const auto y = v + static_cast<double>(geometry.border);
    if (x < 0 || y < 0 || x >= stored_grid_width || y >= stored_grid_height) return std::nullopt;
    const GridCell cell{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
    return geometry.contains(cell) ? std::optional<GridCell>{cell} : std::nullopt;
}

TerrainRenderPlan make_terrain_render_plan(const ParsedEmperorMap& map,
                                           const MapGeometry& geometry,
                                           const TerrainBindings& bindings) {
    if (!geometry.supported) throw std::invalid_argument("textured view requires supported map geometry");
    constexpr auto cells = static_cast<std::size_t>(stored_grid_width) * stored_grid_height;
    if (map.terrain_raw.values.size() != cells || map.objects_raw.values.size() != cells ||
        geometry.candidate.size() != cells)
        throw std::invalid_argument("incomplete map grids or candidate mask");
    TerrainRenderPlan plan;
    plan.border = geometry.border;
    plan.status_by_storage.assign(cells, PreviewStatus::ExcludedByPreviewMask);
    const auto interpreted = interpret_map(map);
    for (std::uint32_t y = 0; y < stored_grid_height; ++y) {
        for (std::uint32_t x = 0; x < stored_grid_width; ++x) {
            const GridCell cell{x,y};
            const auto i = static_cast<std::size_t>(y) * stored_grid_width + x;
            if (!geometry.contains(cell)) { ++plan.counts.excluded; continue; }
            ++plan.counts.candidate;
            TerrainInstance instance;
            instance.storage = cell;
            instance.world = terrain_world(cell, geometry.border);
            instance.image_origin = terrain_image_origin(instance.world);
            instance.interpretation = interpreted[i];
            const auto binding = bindings.exact.find({instance.interpretation.terrain_raw,
                                                       instance.interpretation.objects_raw});
            if (binding != bindings.exact.end()) {
                instance.status = PreviewStatus::CuratedPreview;
                instance.binding_id = binding->second.id;
                instance.asset_alias = binding->second.asset_alias;
                ++plan.counts.bound;
                ++plan.counts.by_binding[instance.binding_id];
            } else ++plan.counts.unmapped;
            plan.status_by_storage[i] = instance.status;
            plan.instances.push_back(std::move(instance));
        }
    }
    std::stable_sort(plan.instances.begin(), plan.instances.end(), [](const auto& a, const auto& b) {
        if (a.world.y != b.world.y) return a.world.y < b.world.y;
        return a.world.x < b.world.x;
    });
    if (plan.instances.size() != plan.counts.candidate ||
        plan.counts.candidate != plan.counts.bound + plan.counts.unmapped ||
        plan.counts.candidate + plan.counts.excluded != cells)
        throw std::logic_error("terrain render plan counts inconsistent");
    return plan;
}
} // namespace openemperor::maps
