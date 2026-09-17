#pragma once

#include "maps/EmperorMap.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace openemperor::maps {

// Diagnostic split hypothesis for the 5*228*228 bytes skipped by the pinned
// public map reader. These names do not assert any graphics semantics.
constexpr std::uint64_t candidate_word_logical_offset = 1535;
constexpr std::uint64_t candidate_byte_logical_offset = 209471;
constexpr std::uint64_t candidate_word_byte_length = grid_byte_length;
constexpr std::uint64_t candidate_byte_byte_length = stored_grid_width * stored_grid_height;

struct MapGraphicCandidates {
    std::vector<std::uint32_t> candidate_word_layer;
    std::vector<std::uint8_t> candidate_byte_layer;
    std::size_t cell_index(std::uint32_t x, std::uint32_t y) const;
    std::uint32_t word_at(std::uint32_t x, std::uint32_t y) const;
    std::uint8_t byte_at(std::uint32_t x, std::uint32_t y) const;
    std::uint64_t word_offset(std::uint32_t x, std::uint32_t y) const;
    std::uint64_t byte_offset(std::uint32_t x, std::uint32_t y) const;
};

MapGraphicCandidates read_map_graphic_candidates(const EmperorContainer& container, std::size_t part);
MapGraphicCandidates decode_map_graphic_candidates(std::span<const std::uint8_t> words,
                                                   std::span<const std::uint8_t> bytes);

} // namespace openemperor::maps
