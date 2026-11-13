#pragma once

#include "assets/BuildingVisualProfile.h"
#include <cstdint>
#include <optional>

namespace openemperor::assets {
inline constexpr const char* plantation_visual_selection_contract=
    "authored80-source-quantity-canonical-unharvested-v1";
constexpr std::optional<std::size_t> plantation_visual_index(std::uint16_t model) {
    return model>=26 && model<=28 ? std::optional<std::size_t>{model-26U}:std::nullopt;
}
// Original quantity publisher, canonical variation0 and harvest-done=false.
// A verified image selector is separate from the authored World work clock.
constexpr std::optional<std::uint8_t> plantation_quantity_visual_stage(
        std::uint16_t model,std::uint32_t quantity) {
    if (!plantation_visual_index(model) || quantity>10000) return std::nullopt;
    const auto percent=quantity/100U;
    return static_cast<std::uint8_t>(model==26 ? std::min(3U,3U*percent/32U):
        std::min(5U,(model==28 ? 2U:1U)*percent/20U));
}
// Explicit presentation adapter only:80 authored productive ticks span the
// Original quantity range. It does not assert Original growth/time parity.
constexpr std::optional<std::uint8_t> plantation_growth_visual_stage(
        std::uint16_t model,std::uint32_t growth) {
    return growth<=80 ? plantation_quantity_visual_stage(model,growth*125U):std::nullopt;
}
inline const BuildingVisualEntry* find_plantation_visual(const BuildingVisualProfile& profile,
        std::uint16_t model,std::uint32_t growth,std::uint8_t actual_side=1) {
    const auto index=plantation_visual_index(model);
    const auto stage=plantation_growth_visual_stage(model,growth);
    if (!profile.plantation_prepared || !index || !stage || actual_side!=1) return nullptr;
    const auto& entry=profile.plantation_entries[*index][*stage];
    return entry && entry->footprint_side==actual_side ? &*entry:nullptr;
}
// Metadata-only optional append, shared source readers and existing combined
// image pool/budget. Failure leaves every existing pixel and role unchanged.
void append_plantation_visual_profile(const std::filesystem::path& data_root,
    const std::filesystem::path& manifest,BuildingVisualProfile& profile,
    std::uint64_t max_final_rgba=building_max_rgba_bytes);
}
