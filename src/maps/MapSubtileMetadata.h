#pragma once

#include "maps/StorageGridView.h"

#include <cstdint>
#include <optional>

namespace openemperor::maps {

// Reference-derived interpretation of the saved candidate byte. The high bit
// remains unknown; these profiles are opt-in saved-graphics previews.
inline constexpr const char* edge_byte_profile = "reference-edge-byte-2x2-preview";
inline constexpr const char* edge_byte_4x4_profile = "reference-edge-byte-4x4-preview";
struct MapSubtileMetadata {
    std::uint8_t raw_byte = 0;
    std::uint8_t part_x = 0;
    std::uint8_t part_y = 0;
    bool draw_marker_candidate = false;
    std::uint8_t unknown_bits = 0;
};

MapSubtileMetadata decode_map_subtile_byte(std::uint8_t raw);
std::optional<GridCell> map_subtile_origin(GridCell storage, MapSubtileMetadata metadata);

} // namespace openemperor::maps
