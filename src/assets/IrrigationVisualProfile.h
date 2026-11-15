#pragma once
#include "assets/BuildingVisualProfile.h"

namespace openemperor::assets {
inline constexpr const char* irrigation_visual_selection_contract=
    "source-view0-ditch-wet-dry-pump-orientation-v1";
// A static source pose, with water selected exclusively from World authority.
// The source animation cadence has not been reconstructed.
inline const BuildingVisualEntry* find_irrigation_ditch_visual(
        const BuildingVisualProfile& profile,std::uint8_t variant,bool wet) {
    if (!profile.irrigation_prepared || variant>=18) return nullptr;
    const auto& entry=profile.irrigation_ditch_entries[variant+(wet ? 0U:18U)];
    return entry ? &*entry:nullptr;
}
inline const BuildingVisualEntry* find_irrigation_pump_visual(
        const BuildingVisualProfile& profile,std::uint8_t orientation) {
    if (!profile.irrigation_prepared || orientation>=4) return nullptr;
    const auto& entry=profile.irrigation_pump_entries[orientation];
    return entry ? &*entry:nullptr;
}
// Atomic optional metadata supplement, sharing physical images, containment,
// SG3/555 readers and the existing combined64MiB image budget.
void append_irrigation_visual_profile(const std::filesystem::path& data_root,
    const std::filesystem::path& manifest,BuildingVisualProfile& profile,
    std::uint64_t max_final_rgba=building_max_rgba_bytes);
}
