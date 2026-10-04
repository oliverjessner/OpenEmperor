#pragma once

#include "maps/LandscapeSelectors.h"

namespace openemperor::maps {

struct PinnacleSelection {
    GridCell origin;
    unsigned side = 5;
    ResourceGroupKey group;
    const char* reason = "EXE-observed pinnacle bank and canonical 5x5 ownership; complete composition remains preview";
};

// Static 53fa20 bank selection and bounded 4b7bd0 origin recovery. The
// renderer's orientation-zero shape is admitted only when all 25 raw cells
// independently agree on ownership. Saved IDs, height, and variation are not
// selector inputs; original runtime-ID/first-pass composition remains separate.
std::optional<PinnacleSelection> select_pinnacle(
    const LandscapeSelectorInput& input, GridCell cell,
    std::span<const std::uint8_t> draw_properties,
    std::span<const std::uint8_t> candidate_bytes);

} // namespace openemperor::maps
