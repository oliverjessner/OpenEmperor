#include "maps/MapGraphicCandidates.h"

#include <stdexcept>
#include <utility>

namespace openemperor::maps {

std::size_t MapGraphicCandidates::cell_index(std::uint32_t x, std::uint32_t y) const {
    if (x >= stored_grid_width || y >= stored_grid_height)
        throw std::out_of_range("candidate storage cell is outside 228x228");
    return static_cast<std::size_t>(y) * stored_grid_width + x;
}
std::uint32_t MapGraphicCandidates::word_at(std::uint32_t x, std::uint32_t y) const {
    return candidate_word_layer.at(cell_index(x, y));
}
std::uint8_t MapGraphicCandidates::byte_at(std::uint32_t x, std::uint32_t y) const {
    return candidate_byte_layer.at(cell_index(x, y));
}
std::uint64_t MapGraphicCandidates::word_offset(std::uint32_t x, std::uint32_t y) const {
    return candidate_word_logical_offset + cell_index(x, y) * 4U;
}
std::uint64_t MapGraphicCandidates::byte_offset(std::uint32_t x, std::uint32_t y) const {
    return candidate_byte_logical_offset + cell_index(x, y);
}

MapGraphicCandidates decode_map_graphic_candidates(std::span<const std::uint8_t> words,
                                                   std::span<const std::uint8_t> bytes) {
    if (words.size() != candidate_word_byte_length || bytes.size() != candidate_byte_byte_length)
        throw UnsupportedMapProfile("truncated candidate-layer range");
    MapGraphicCandidates result;
    result.candidate_word_layer.reserve(candidate_byte_byte_length);
    for (std::size_t at = 0; at < words.size(); at += 4) {
        result.candidate_word_layer.push_back(static_cast<std::uint32_t>(words[at]) |
            (static_cast<std::uint32_t>(words[at + 1]) << 8U) |
            (static_cast<std::uint32_t>(words[at + 2]) << 16U) |
            (static_cast<std::uint32_t>(words[at + 3]) << 24U));
    }
    result.candidate_byte_layer.assign(bytes.begin(),bytes.end());
    return result;
}

MapGraphicCandidates read_map_graphic_candidates(const EmperorContainer& container, std::size_t part) {
    const auto probe = probe_map_part(container, part);
    if (probe.profile != PartProfile::Map)
        throw UnsupportedMapProfile("candidate layers require the supported map profile: " + probe.reason);
    // read_range retains the container's block, decompression and range budgets.
    const auto words = container.read_range(part, candidate_word_logical_offset,
                                            candidate_word_byte_length);
    const auto bytes = container.read_range(part, candidate_byte_logical_offset,
                                      candidate_byte_byte_length);
    return decode_map_graphic_candidates(words,bytes);
}

std::uint8_t read_auxiliary_map_byte(const EmperorContainer& container, std::size_t part,
                                     std::uint32_t x, std::uint32_t y) {
    const auto probe = probe_map_part(container,part);
    if (probe.profile != PartProfile::Map)
        throw UnsupportedMapProfile("auxiliary byte requires the supported map profile: " + probe.reason);
    if (x >= stored_grid_width || y >= stored_grid_height)
        throw std::out_of_range("auxiliary byte cell is outside 228x228");
    const auto index = static_cast<std::uint64_t>(y) * stored_grid_width + x;
    return container.read_range(part,auxiliary_byte_logical_offset+index,1).front();
}

} // namespace openemperor::maps
