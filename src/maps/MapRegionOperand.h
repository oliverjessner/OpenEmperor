#pragma once

#include "maps/EmperorContainer.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace openemperor::maps {

inline constexpr std::uint64_t crop_region_logical_offset = 12;

// A read-only operand from the supported standalone schema5/header2 profile.
// The Original copies only its low WORD into the signed region-table selector.
// Unknown values stay observable and grant no climate or World authority.
struct MapRegionOperand {
    std::uint32_t serialized = 0;

    [[nodiscard]] constexpr std::int32_t native_selector() const noexcept {
        const auto low_word = serialized & 0xffffU;
        const auto value = static_cast<std::int32_t>(low_word);
        return low_word < 0x8000U ? value : value - 0x10000;
    }

    [[nodiscard]] constexpr std::optional<std::uint8_t> numeric_factor_index() const noexcept {
        const auto selector = native_selector();
        if (selector < 0 || selector > 2) return std::nullopt;
        return static_cast<std::uint8_t>(selector);
    }

    bool operator==(const MapRegionOperand&) const = default;
};

// Reuses the historical profile probe and bounded container range reader.
// This does not read other map/header versions or any Original save profile.
[[nodiscard]] MapRegionOperand read_crop_region_operand(
    const EmperorContainer& container, std::size_t part);

} // namespace openemperor::maps
