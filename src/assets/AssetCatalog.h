#pragma once

#include "assets/Sg3Archive.h"
#include "assets/Sg3PayloadLayout.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {

struct AssetId {
    std::filesystem::path archive_relative_path;
    std::uint32_t image_index = 0;
    bool operator==(const AssetId&) const = default;
};

enum class AssetRangeStatus { NotPresent, InBounds, OutOfBounds, SourceUnavailable, Unverified };
const char* asset_range_status_name(AssetRangeStatus status);

struct AssetRecord {
    AssetId id;
    std::uint32_t sg3_version = 0;
    std::uint8_t group_id = 0;
    std::string group_filename;
    std::string group_description;
    std::uint16_t image_type = 0;
    Sg3ImageKind image_kind = Sg3ImageKind::Unsupported;
    std::int16_t width = 0;
    std::int16_t height = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t data_length = 0;
    std::uint32_t uncompressed_length = 0;
    std::uint8_t external_flag = 0;
    std::uint8_t isometric_size_flag = 0;
    std::uint16_t animation_sprites = 0;
    std::uint32_t alpha_offset = 0;
    std::uint32_t alpha_length = 0;
    std::optional<std::uint64_t> effective_alpha_offset;
    AlphaPolicy alpha_policy = AlphaPolicy::None;
    bool alpha_profile_supported = true;
    std::uint32_t horizontal_mirror_offset = 0;
    AssetRangeStatus color_bounds = AssetRangeStatus::SourceUnavailable;
    AssetRangeStatus alpha_bounds = AssetRangeStatus::NotPresent;
    AssetRangeStatus raw_alpha_bounds = AssetRangeStatus::NotPresent;
    bool metadata_supported = true;
    bool payload_in_bounds = false;
    bool color_decoder_supported = false;
    bool decoder_supported = false; // Supported layout, independent of source bounds or decode outcome.
    bool decode_attempted = false;
    bool decode_succeeded = false;
    std::string decode_error;
};

struct ArchiveError {
    std::filesystem::path archive_relative_path;
    std::string message;
};

struct AssetCatalog {
    std::filesystem::path data_root; // Runtime-only absolute path; never part of AssetId/JSON.
    std::size_t archive_count = 0;
    std::vector<ArchiveError> archive_errors;
    std::vector<AssetRecord> records;
};

struct AssetFilter {
    std::optional<Sg3ImageKind> kind;
    std::optional<std::uint16_t> image_type;
    std::optional<std::string> archive_substring;
    std::optional<std::string> group_substring;
    std::optional<bool> with_alpha;
    std::optional<std::uint16_t> min_width;
    std::optional<std::uint16_t> min_height;
};

struct AssetCounts {
    std::size_t records = 0;
    std::size_t plain = 0;
    std::size_t sprite = 0;
    std::size_t isometric = 0;
    std::size_t unsupported = 0;
    std::size_t with_alpha = 0;
    std::size_t color_out_of_bounds = 0;
    std::size_t alpha_out_of_bounds = 0;
    std::size_t color_source_unavailable = 0;
    std::size_t alpha_source_unavailable = 0;
    std::map<std::uint16_t, std::size_t> type_counts;
};

AssetCatalog scan_asset_catalog(const std::filesystem::path& data_directory);
// Inspect exactly one archive with the same metadata/range logic as a full scan.
AssetCatalog scan_asset_archive(const std::filesystem::path& data_directory,
                                const std::filesystem::path& archive_relative_path);
bool asset_matches_filter(const AssetRecord& record, const AssetFilter& filter);
std::vector<std::size_t> matching_asset_indices(const AssetCatalog& catalog,
                                                 const AssetFilter& filter);
AssetCounts count_assets(const AssetCatalog& catalog,
                         const std::vector<std::size_t>& indices);

} // namespace openemperor::assets
