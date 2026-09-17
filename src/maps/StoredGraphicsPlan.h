#pragma once

#include "maps/GraphicsIdHypothesis.h"
#include "maps/MapGraphicCandidates.h"
#include "maps/TerrainRenderPlan.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::maps {

inline constexpr const char* stored_graphics_profile = "exe-6373328b-v213-runtime-table";
inline constexpr std::size_t stored_max_assets = 2048;
inline constexpr std::uint64_t stored_max_image_bytes = 16U * 1024U * 1024U;
inline constexpr std::uint64_t stored_max_payload_bytes = 16U * 1024U * 1024U;
inline constexpr std::uint64_t stored_max_texture_bytes = 64U * 1024U * 1024U;

enum class StoredStatus { Excluded, DecodePending, Rendered, UnsupportedHighBit,
    UnregisteredSlot, UnverifiedRegistration, IndexOutOfRange, EmptyRecord,
    SourceUnavailable, UnsupportedLayout, MultiTilePlacementUnverified,
    MirrorUnverified, DecodeFailed };
const char* stored_status_name(StoredStatus status);

struct StoredAsset {
    assets::AssetRecord record;
    StoredStatus status = StoredStatus::DecodePending;
    bool decode_attempted = false;
    bool decode_succeeded = false;
    std::string error;
};

struct StoredCell {
    GridCell storage{};
    std::size_t cell_index = 0;
    std::uint32_t stored_id = 0;
    std::uint64_t logical_offset = 0;
    std::uint32_t terrain_raw = 0;
    std::uint32_t objects_raw = 0;
    std::uint8_t candidate_byte = 0;
    bool offmap_bit = false;
    std::uint32_t slot = 0;
    std::uint32_t local_index = 0;
    std::uint32_t system_record_skip = 0;
    std::optional<std::uint32_t> physical_record;
    std::optional<std::size_t> asset_index;
    GraphicsIdStatus lookup_status = GraphicsIdStatus::UnregisteredSlot;
    bool record_present = false;
    bool source_ranges_valid = false;
    bool footprint_supported = false;
    StoredStatus status = StoredStatus::UnregisteredSlot;
    scene::Point world{};
    scene::Point image_origin{};
};

struct StoredGraphicsPlan {
    std::filesystem::path data_root;
    std::vector<StoredCell> cells; // Exactly one per candidate, in painter order.
    std::vector<StoredAsset> assets; // Distinct physical AssetIds.
    std::vector<StoredStatus> status_by_storage; // Excluded cells retained.
    std::vector<std::optional<std::size_t>> cell_by_storage;
    std::uint32_t border = 0;
    MaskComparison mask_comparison;
    std::size_t excluded = 0;
    std::size_t texture_uploads = 0;
    std::uint64_t logical_texture_bytes = 0;
    const StoredCell* at(GridCell cell) const;
    std::map<std::string,std::size_t> status_counts() const;
};

scene::Point stored_image_origin(scene::Point world, std::uint32_t width, std::uint32_t height);
bool stored_rect_visible(scene::Point origin, std::uint32_t width, std::uint32_t height,
                         const scene::Camera2D& camera);
StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const assets::AssetCatalog& terrain,
    const RuntimeArchiveLayout& terrain_layout, const assets::AssetCatalog& elevation,
    const RuntimeArchiveLayout& elevation_layout);

// Checks archive and every documented bitmap source against the canonical
// user data root before catalog scanning or image loading, including symlinks.
std::filesystem::path validate_stored_archive_sources(
    const std::filesystem::path& data_root, const std::filesystem::path& archive_relative);

} // namespace openemperor::maps
