#pragma once

#include "maps/StoredGraphicsPlan.h"
#include "maps/MapGeometry.h"

#include <cstdint>
#include <vector>

namespace openemperor::maps {

inline constexpr const char* sandbox_buildable_profile = "sandbox_buildable_v1";

// Conservative, diagnostic placement mask. Call after the stored renderer has
// updated decode statuses. Neither the map nor its plan is mutated here.
std::vector<std::uint8_t> make_sandbox_buildable_mask(const StoredGraphicsPlan& plan,
                                                      const MapGeometry& geometry);

} // namespace openemperor::maps
