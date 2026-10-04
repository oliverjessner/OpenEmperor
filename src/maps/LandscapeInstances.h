#pragma once
#include "maps/LandscapeSelectors.h"
#include "maps/TerrainRenderPlan.h"
#include <vector>

namespace openemperor::maps {
// Derived load-time geometry. These are not historical saved-ID footprints.
struct LandscapeInstanceSpec {
    LandscapeSelection selection;
    GridCell origin{}, draw_cell{};
    unsigned side=1;
    std::vector<GridCell> owned_cells;
    const char* placement_evidence="OPENEMPEROR PREVIEW: projected footprint anchor";
    const char* composition_evidence="partial regeneration; original first-pass occupancy unresolved";
};
// Row-major generation order (54044f..540482), with 3x3 before 2x2 before
// singleton (53f660). Eligibility bounds the explicitly preserved layers.
std::vector<LandscapeInstanceSpec> derive_rock_instances(
    const LandscapeSelectorInput& input, std::span<const std::uint8_t> eligible);
scene::Point regenerated_instance_origin(const LandscapeInstanceSpec& instance,
    std::uint32_t border, unsigned image_width, unsigned image_height, int height);
} // namespace openemperor::maps
