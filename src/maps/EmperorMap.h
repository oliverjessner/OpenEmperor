#pragma once

#include "maps/EmperorContainer.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace openemperor::maps {

class UnsupportedMapProfile : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

constexpr std::uint32_t stored_grid_width = 228;
constexpr std::uint32_t stored_grid_height = 228;
constexpr std::uint64_t terrain_logical_offset = 261455;
constexpr std::uint64_t objects_logical_offset = 469391;
constexpr std::uint64_t grid_byte_length = 228U * 228U * 4U;

enum class PartProfile { Map, SavedGame, Unknown, UnsupportedMap };
const char* part_profile_name(PartProfile profile);
struct MapProbe {
    PartProfile profile = PartProfile::Unknown;
    std::uint32_t declared_map_size = 0;
    std::string reason;
};
MapProbe probe_map_part(const EmperorContainer& container, std::size_t part);

struct MapLayer {
    std::uint64_t logical_offset = 0;
    std::vector<std::uint32_t> values; // Row-major: values[y * 228 + x].
};
struct ParsedEmperorMap {
    std::size_t source_part = 0;
    std::uint64_t source_uncompressed_size = 0;
    std::uint32_t stored_width = stored_grid_width;
    std::uint32_t stored_height = stored_grid_height;
    std::uint32_t declared_map_size = 0;
    MapLayer terrain_raw;
    MapLayer objects_raw;
    std::uint32_t terrain_at(std::uint32_t x, std::uint32_t y) const;
    std::uint32_t object_at(std::uint32_t x, std::uint32_t y) const;
    std::uint64_t terrain_cell_offset(std::uint32_t x, std::uint32_t y) const;
    std::uint64_t object_cell_offset(std::uint32_t x, std::uint32_t y) const;
};
ParsedEmperorMap read_emperor_map(const EmperorContainer& container, std::size_t part);

} // namespace openemperor::maps
