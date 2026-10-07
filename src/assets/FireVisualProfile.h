#pragma once

#include "assets/AssetCatalog.h"
#include "assets/Sg3RgbaDecoder.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {

// One bounded, explicit physical-record sequence; not a Walker/navigation role.
inline constexpr std::size_t fire_frame_limit=64;
inline constexpr std::uint16_t fire_dimension_limit=256;
inline constexpr std::uint64_t fire_rgba_budget=64U*1024U*1024U;

struct FireFrame {
    AssetId id; // Physical SG3 record, never a packed runtime resource ID.
    double anchor_x=0,anchor_y=0;
    std::size_t image_index=0; // Deduplicated unique_images index.
};

struct FireVisualProfile {
    std::uint32_t schema_version=1;
    std::string clip_id;
    std::string evidence;
    std::uint32_t ticks_per_frame=1;
    std::vector<FireFrame> frames; // Explicit sequence order; aliases may reuse one asset.
    std::vector<RgbaImage> unique_images;
};

// Manifest may be outside data_root; every SG3 and resolved .555 stays within it.
FireVisualProfile load_fire_visual_profile(const std::filesystem::path& data_root,
                                           const std::filesystem::path& manifest);
// Preparation-only validation also covers independently constructed test profiles.
void validate_fire_visual_profile(const FireVisualProfile& profile);
// Global authored loop with a bounded stable ID offset of 0..6 frames; no mutable
// counters, deadline inference or wall clock. BuildingId zero selects phase zero.
std::optional<std::size_t> fire_frame_index(const FireVisualProfile& profile,
                                           std::uint64_t world_tick,
                                           std::uint64_t building_id=0);
std::uint64_t fire_rgba_bytes(const FireVisualProfile& profile);

} // namespace openemperor::assets
