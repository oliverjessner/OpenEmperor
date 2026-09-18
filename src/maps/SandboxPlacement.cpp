#include "maps/SandboxPlacement.h"

#include "maps/EmperorMap.h"

#include <stdexcept>

namespace openemperor::maps {

std::vector<std::uint8_t> make_sandbox_buildable_mask(const StoredGraphicsPlan& plan,
                                                      const MapGeometry& geometry) {
    if (!geometry.supported || plan.border!=geometry.border)
        throw std::invalid_argument("sandbox map geometry mismatch");
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(stored_grid_width)*stored_grid_height,0);
    for (const auto& cell:plan.cells) {
        if (cell.cell_index>=mask.size() ||
            cell.cell_index!=static_cast<std::size_t>(cell.storage.y)*stored_grid_width+cell.storage.x)
            throw std::invalid_argument("invalid stored cell for sandbox placement");
        if (!cell.footprint_index) continue;
        if (*cell.footprint_index>=plan.footprints.size())
            throw std::invalid_argument("invalid stored footprint for sandbox placement");
        const auto& footprint=plan.footprints[*cell.footprint_index];
        if (geometry.contains(cell.storage) && cell.status==StoredStatus::Rendered &&
            footprint.status==StoredStatus::Rendered &&
            footprint.width_cells==1 && footprint.height_cells==1 &&
            !cell.offmap_bit && cell.terrain_raw==0x80U && cell.objects_raw==0)
            mask[cell.cell_index]=1;
    }
    return mask;
}

} // namespace openemperor::maps
