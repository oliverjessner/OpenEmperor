#include "maps/MapSubtileMetadata.h"

#include "maps/MapGraphicCandidates.h"

#include <cstdint>

namespace openemperor::maps {

MapSubtileMetadata decode_map_subtile_byte(std::uint8_t raw) {
    return {raw, static_cast<std::uint8_t>(raw & 0x07U),
            static_cast<std::uint8_t>((raw >> 3U) & 0x07U),
            (raw & 0x40U) != 0, static_cast<std::uint8_t>(raw & 0x80U)};
}

std::optional<GridCell> map_subtile_origin(GridCell storage, MapSubtileMetadata metadata) {
    if (storage.x >= stored_grid_width || storage.y >= stored_grid_height) return std::nullopt;
    const auto x = static_cast<std::int64_t>(storage.x) - metadata.part_x;
    const auto y = static_cast<std::int64_t>(storage.y) - metadata.part_y;
    if (x < 0 || y < 0 || x >= stored_grid_width || y >= stored_grid_height) return std::nullopt;
    return GridCell{static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y)};
}

} // namespace openemperor::maps
