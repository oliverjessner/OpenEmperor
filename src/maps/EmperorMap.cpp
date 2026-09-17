#include "maps/EmperorMap.h"

#include <algorithm>
#include <array>
#include <span>

namespace openemperor::maps {
namespace {
constexpr std::array<std::uint8_t, 8> map_signature{5, 0, 0xfe, 0xca, 0, 0, 2, 0};

std::uint32_t le32(std::span<const std::uint8_t> bytes, std::size_t at) {
    if (at > bytes.size() || bytes.size() - at < 4)
        throw UnsupportedMapProfile("map profile word is outside uncompressed part");
    return static_cast<std::uint32_t>(bytes[at]) |
        (static_cast<std::uint32_t>(bytes[at + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[at + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[at + 3]) << 24U);
}
std::size_t checked_index(std::uint32_t x, std::uint32_t y) {
    if (x >= stored_grid_width || y >= stored_grid_height)
        throw std::out_of_range("storage-grid cell is outside 228x228");
    return static_cast<std::size_t>(y) * stored_grid_width + x;
}
MapLayer read_layer(std::span<const std::uint8_t> bytes, std::uint64_t offset) {
    if (offset > bytes.size() || grid_byte_length > bytes.size() - offset)
        throw UnsupportedMapProfile("declared map profile has a truncated 228x228 uint32 layer");
    MapLayer layer;
    layer.logical_offset = offset;
    layer.values.reserve(stored_grid_width * stored_grid_height);
    for (std::size_t i = 0; i < stored_grid_width * stored_grid_height; ++i)
        layer.values.push_back(le32(bytes, static_cast<std::size_t>(offset) + i * 4U));
    return layer;
}
} // namespace

const char* part_profile_name(PartProfile profile) {
    switch (profile) {
    case PartProfile::Map: return "emperor_map_v1_storage_grid";
    case PartProfile::SavedGame: return "saved_game_unsupported";
    case PartProfile::UnsupportedMap: return "unsupported_map_profile";
    case PartProfile::Unknown: return "unknown_part";
    }
    return "unknown_part";
}

MapProbe probe_map_part(const EmperorContainer& container, std::size_t part) {
    if (part >= container.parts().size()) throw ContainerError("part index is outside container");
    const auto size = container.parts()[part].uncompressed_size;
    const auto prefix = container.read_range(part, 0, std::min<std::uint64_t>(size, 88));
    if (prefix.size() >= 2 && prefix[0] == 0x13 && prefix[1] == 0)
        return {PartProfile::SavedGame, 0, "savegame prefix; outside current scope"};
    if (prefix.size() < map_signature.size() ||
        !std::equal(map_signature.begin(), map_signature.end(), prefix.begin()))
        return {PartProfile::Unknown, 0, "map signature absent"};
    if (prefix.size() < 88) return {PartProfile::UnsupportedMap, 0, "map header is shorter than 88 bytes"};
    const std::uint32_t size_field = le32(prefix, 84);
    if (size_field == 0 || size_field > stored_grid_width)
        return {PartProfile::UnsupportedMap, size_field, "declared map size is outside 1..228"};
    if (size < objects_logical_offset + grid_byte_length)
        return {PartProfile::UnsupportedMap, size_field, "terrain or object layer exceeds uncompressed part"};
    return {PartProfile::Map, size_field, "signature, map size, and layer ranges match supported profile"};
}

ParsedEmperorMap read_emperor_map(const EmperorContainer& container, std::size_t part) {
    const auto probe = probe_map_part(container, part);
    if (probe.profile != PartProfile::Map)
        throw UnsupportedMapProfile("part " + std::to_string(part) + ": " + probe.reason);
    const auto bytes = container.read_part(part); // Validates every block, not only the layers.
    ParsedEmperorMap map;
    map.source_part = part;
    map.source_uncompressed_size = bytes.size();
    map.declared_map_size = probe.declared_map_size;
    map.terrain_raw = read_layer(bytes, terrain_logical_offset);
    map.objects_raw = read_layer(bytes, objects_logical_offset);
    return map;
}

std::uint32_t ParsedEmperorMap::terrain_at(std::uint32_t x, std::uint32_t y) const {
    return terrain_raw.values.at(checked_index(x, y));
}
std::uint32_t ParsedEmperorMap::object_at(std::uint32_t x, std::uint32_t y) const {
    return objects_raw.values.at(checked_index(x, y));
}
std::uint64_t ParsedEmperorMap::terrain_cell_offset(std::uint32_t x, std::uint32_t y) const {
    return terrain_raw.logical_offset + checked_index(x, y) * 4U;
}
std::uint64_t ParsedEmperorMap::object_cell_offset(std::uint32_t x, std::uint32_t y) const {
    return objects_raw.logical_offset + checked_index(x, y) * 4U;
}

} // namespace openemperor::maps
