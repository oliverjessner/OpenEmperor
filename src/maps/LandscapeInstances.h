#pragma once
#include "maps/LandscapeSelectors.h"
#include "maps/TerrainRenderPlan.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace openemperor::maps {
enum class LandscapeInstanceHeightSource { SerializedCellHeight };
struct LandscapeInstanceHeight {
    LandscapeInstanceHeightSource source=LandscapeInstanceHeightSource::SerializedCellHeight;
    GridCell cell{};
};
// Pixel anchor relative to terrain_ground(ground_cell). The image's Y anchor
// is image_height-y_from_image_bottom; no raster scaling is implied.
struct LandscapeInstanceAnchor {
    GridCell ground_cell{};
    int x=0, y_from_image_bottom=20;
};
// Derived load-time geometry. These are not historical saved-ID footprints.
struct LandscapeInstanceSpec {
    LandscapeSelection selection;
    GridCell origin{}, draw_cell{};
    unsigned side=1;
    std::vector<GridCell> owned_cells;
    const char* placement_evidence="OPENEMPEROR PREVIEW: projected footprint anchor";
    const char* composition_evidence="partial regeneration; original first-pass occupancy unresolved";
    // Restored monuments can preserve the EXE's marker as draw_cell while
    // expressing its equivalent front-cell image anchor and painter depth.
    // Existing landscape paths retain their current placement when unset.
    std::optional<LandscapeInstanceHeight> explicit_height;
    std::optional<LandscapeInstanceAnchor> explicit_anchor;
    std::optional<GridCell> depth_cell;
    std::optional<std::size_t> original_entity_index;
};
// Row-major generation order (54044f..540482), with 3x3 before 2x2 before
// singleton (53f660). Eligibility bounds the explicitly preserved layers.
std::vector<LandscapeInstanceSpec> derive_rock_instances(
    const LandscapeSelectorInput& input, std::span<const std::uint8_t> eligible);
scene::Point regenerated_instance_origin(const LandscapeInstanceSpec& instance,
    std::uint32_t border, unsigned image_width, unsigned image_height, int height);
} // namespace openemperor::maps
