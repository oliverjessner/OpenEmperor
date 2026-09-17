#pragma once

#include "maps/StorageGridView.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::maps {

struct BitmapPixel {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    bool operator==(const BitmapPixel&) const = default;
};

// Reference-derived bitmap selection; not an original-game world-coordinate model.
struct MapGeometry {
    explicit MapGeometry(std::uint32_t declared_size);
    std::uint32_t declared_size;
    bool supported = false;
    std::uint32_t border = 0;
    std::vector<std::uint8_t> candidate; // Storage row-major, 0/1.
    std::vector<std::optional<GridCell>> projected_pixels; // size*size, both pixels of a cell.
    bool contains(GridCell cell) const;
    std::optional<GridCell> at_projected(BitmapPixel pixel) const;
    std::optional<BitmapPixel> projected_origin(GridCell cell) const;
};

struct MaskComparison {
    std::uint64_t candidate_and_offmap = 0;
    std::uint64_t candidate_and_onmap = 0;
    std::uint64_t outside_and_offmap = 0;
    std::uint64_t outside_and_onmap = 0;
    std::vector<GridCell> mismatch_examples; // bounded to 16, storage coordinates.
    std::uint64_t mismatches() const { return candidate_and_offmap + outside_and_onmap; }
};
MaskComparison compare_masks(const ParsedEmperorMap& map, const MapGeometry& geometry);

} // namespace openemperor::maps
