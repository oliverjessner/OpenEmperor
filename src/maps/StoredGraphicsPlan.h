#pragma once

#include "maps/GraphicsIdHypothesis.h"
#include "maps/MapGraphicCandidates.h"
#include "maps/MapSubtileMetadata.h"
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
inline constexpr const char* stored_graphics_slot8_profile = "exe-6373328b-v213-slot8-runtime-table";
enum class StoredGraphicsProfile { Base, Slot8 };
const char* stored_graphics_profile_name(StoredGraphicsProfile profile);
std::optional<std::filesystem::path> stored_registered_archive_path(
    std::uint32_t slot, StoredGraphicsProfile profile);
inline constexpr std::size_t stored_max_assets = 2048;
inline constexpr std::uint64_t stored_max_image_bytes = 16U * 1024U * 1024U;
inline constexpr std::uint64_t stored_max_payload_bytes = 16U * 1024U * 1024U;
inline constexpr std::uint64_t stored_max_texture_bytes = 64U * 1024U * 1024U;

enum class StoredStatus { Excluded, DecodePending, Rendered, UnsupportedHighBit,
    UnregisteredSlot, UnverifiedRegistration, ArchiveMissing, IndexOutOfRange, EmptyRecord,
    SourceUnavailable, UnsupportedLayout, MultiTilePlacementUnverified,
    AmbiguousFootprint, IncompleteFootprint, AnchorUnresolved,
    UnsupportedFootprintSize, MirrorUnverified, DecodeFailed,
    SubtilePositionInvalid, ConflictingFootprint };
const char* stored_status_name(StoredStatus status);

enum class FootprintPolicy { Disabled, IsolatedPreview, EdgeBytePreview };
const char* footprint_policy_name(FootprintPolicy policy);

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
    std::optional<MapSubtileMetadata> subtile;
    std::optional<GridCell> subtile_origin;
    bool offmap_bit = false;
    std::uint32_t slot = 0;
    std::uint32_t local_index = 0;
    std::uint32_t system_record_skip = 0;
    std::optional<std::uint32_t> physical_record;
    std::optional<std::size_t> asset_index;
    std::optional<std::size_t> footprint_index;
    GraphicsIdStatus lookup_status = GraphicsIdStatus::UnregisteredSlot;
    bool record_present = false;
    bool source_ranges_valid = false;
    bool footprint_supported = false;
    StoredStatus status = StoredStatus::UnregisteredSlot;
    scene::Point world{};
    scene::Point image_origin{};
};

// Preview placement, not an assertion about the original game's draw anchor.
struct PlacedFootprint {
    std::size_t id = 0;
    std::size_t asset_index = 0;
    GridCell origin{}; // Rear/top storage cell of our isometric projection.
    std::uint32_t width_cells = 1;
    std::uint32_t height_cells = 1;
    std::vector<std::size_t> cell_indices; // Indices into StoredGraphicsPlan::cells.
    std::optional<GridCell> draw_cell_candidate;
    scene::Point image_origin{};
    const char* rule = "single_cell_geometry";
    StoredStatus status = StoredStatus::DecodePending;
};

struct StoredGraphicsPlan {
    std::filesystem::path data_root;
    StoredGraphicsProfile profile = StoredGraphicsProfile::Base;
    std::vector<StoredCell> cells; // Exactly one per candidate, in painter order.
    std::vector<StoredAsset> assets; // Distinct physical AssetIds.
    std::vector<PlacedFootprint> footprints;
    bool multi_tile_preview = false;
    FootprintPolicy footprint_policy = FootprintPolicy::Disabled;
    std::size_t marker_deviations = 0;
    std::size_t unknown_bit_cells = 0;
    std::vector<StoredStatus> status_by_storage; // Excluded cells retained.
    std::vector<std::optional<std::size_t>> cell_by_storage;
    std::uint32_t border = 0;
    MaskComparison mask_comparison;
    std::size_t excluded = 0;
    std::size_t texture_uploads = 0;
    std::size_t decoded_assets = 0;
    std::uint64_t logical_texture_bytes = 0;
    const StoredCell* at(GridCell cell) const;
    std::map<std::string,std::size_t> status_counts() const;
    std::size_t covered_cells() const;
    std::size_t footprint_count(std::uint32_t side) const;
};

// Each entry owns its metadata, physical catalog and runtime layout. A known
// optional registration may lack its local archive; it is never substituted.
struct StoredArchiveRegistration {
    std::uint32_t slot = 0;
    std::filesystem::path relative_path;
    std::optional<assets::Sg3Archive> metadata;
    std::optional<assets::AssetCatalog> catalog;
    std::optional<RuntimeArchiveLayout> layout;
    bool archive_missing = false;
};
using StoredArchiveRegistrations = std::map<std::uint32_t,StoredArchiveRegistration>;

StoredArchiveRegistrations load_stored_archive_registrations(
    const std::filesystem::path& data_root, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, StoredGraphicsProfile profile);
StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const StoredArchiveRegistrations& registrations,
    FootprintPolicy policy, StoredGraphicsProfile profile);

scene::Point stored_image_origin(scene::Point world, std::uint32_t width, std::uint32_t height);
scene::Point stored_two_by_two_image_origin(scene::Point rear_world, std::uint32_t width,
                                            std::uint32_t height);
bool stored_rect_visible(scene::Point origin, std::uint32_t width, std::uint32_t height,
                         const scene::Camera2D& camera);
StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const assets::AssetCatalog& terrain,
    const RuntimeArchiveLayout& terrain_layout, const assets::AssetCatalog& elevation,
    const RuntimeArchiveLayout& elevation_layout, bool multi_tile_preview = false);
StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const assets::AssetCatalog& terrain,
    const RuntimeArchiveLayout& terrain_layout, const assets::AssetCatalog& elevation,
    const RuntimeArchiveLayout& elevation_layout, FootprintPolicy policy);

// Checks archive and every documented bitmap source against the canonical
// user data root before catalog scanning or image loading, including symlinks.
std::filesystem::path validate_stored_archive_sources(
    const std::filesystem::path& data_root, const std::filesystem::path& archive_relative);

} // namespace openemperor::maps
