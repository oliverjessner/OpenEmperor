#include "assets/AssetCatalog.h"

#include "assets/Sg3ImageLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace openemperor::assets {
namespace {

namespace fs = std::filesystem;

bool is_within(const fs::path& root, const fs::path& path) {
    const fs::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& component : relative) {
        if (component == "..") return false;
    }
    return true;
}

bool is_sg3(const fs::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char byte) {
        return static_cast<char>(std::tolower(byte));
    });
    return extension == ".sg3";
}

struct SourceInfo {
    bool resolved = false;
    std::optional<std::uint64_t> size;
};

std::optional<std::uint64_t> source_size(
    const fs::path& path, std::map<fs::path, std::optional<std::uint64_t>>& cache) {
    const auto cached = cache.find(path);
    if (cached != cache.end()) return cached->second;
    std::error_code error;
    std::optional<std::uint64_t> result;
    if (fs::is_regular_file(path, error) && !error) {
        const std::uintmax_t size = fs::file_size(path, error);
        if (!error && size <= std::numeric_limits<std::uint64_t>::max()) {
            result = static_cast<std::uint64_t>(size);
        }
    }
    cache.emplace(path, result);
    return result;
}

AssetRangeStatus range_status(std::uint64_t offset, std::uint64_t length,
                              const SourceInfo& source) {
    if (!source.resolved || !source.size) return AssetRangeStatus::SourceUnavailable;
    return range_within_file(offset, length, *source.size)
        ? AssetRangeStatus::InBounds : AssetRangeStatus::OutOfBounds;
}

bool supported_color_metadata(const Sg3Image& image, Sg3ImageKind kind) {
    if (image.width <= 0 || image.height <= 0) return false;
    const std::uint64_t width = static_cast<std::uint64_t>(image.width);
    const std::uint64_t height = static_cast<std::uint64_t>(image.height);
    switch (kind) {
    case Sg3ImageKind::Plain: return image.data_length == width * height * 2U;
    case Sg3ImageKind::Sprite: return true;
    case Sg3ImageKind::Isometric: {
        const std::uint64_t footprint_height = (width + 2U) / 2U;
        return height >= footprint_height &&
               image.uncompressed_length <= image.data_length &&
               image.uncompressed_length == (width + 2U) * footprint_height;
    }
    case Sg3ImageKind::Unsupported: return false;
    }
    return false;
}

void append_archive(AssetCatalog& catalog, const fs::path& archive_path,
                    const fs::path& relative_path,
                    std::map<fs::path, std::optional<std::uint64_t>>& size_cache) {
    const Sg3Archive archive = read_sg3_archive(archive_path);
    fs::path internal_path = archive_path;
    internal_path.replace_extension(".555");
    const SourceInfo internal{true, source_size(internal_path, size_cache)};
    std::vector<SourceInfo> external;
    external.reserve(archive.groups.size());
    for (std::size_t group_index = 0; group_index < archive.groups.size(); ++group_index) {
        const Sg3BitmapLocation location = resolve_sg3_group_bitmap(
            archive_path, archive.groups[group_index], group_index);
        const bool resolved = location.status == Sg3BitmapStatus::Resolved;
        external.push_back({resolved, resolved ? source_size(location.path, size_cache)
                                                : std::nullopt});
    }

    for (std::size_t image_index = 0; image_index < archive.images.size(); ++image_index) {
        const Sg3Image& image = archive.images[image_index];
        AssetRecord record;
        record.id = {relative_path, static_cast<std::uint32_t>(image_index)};
        record.sg3_version = archive.header.version;
        record.group_id = image.group_id;
        if (image.group_id < archive.groups.size()) {
            record.group_filename = archive.groups[image.group_id].filename;
            record.group_description = archive.groups[image.group_id].description;
        }
        record.image_type = image.image_type;
        record.image_kind = classify_sg3_image_type(image.image_type);
        record.width = image.width;
        record.height = image.height;
        record.data_offset = image.data_offset;
        record.data_length = image.data_length;
        record.uncompressed_length = image.uncompressed_length;
        record.external_flag = image.external_flag;
        record.isometric_size_flag = image.isometric_size_flag;
        record.alpha_offset = image.alpha_offset;
        record.alpha_length = image.alpha_length;
        const Sg3PayloadLayout layout = sg3_payload_layout(archive.header.version, image);
        record.alpha_policy = layout.alpha_policy;
        record.alpha_profile_supported = layout.alpha_profile_supported;
        if (layout.alpha) record.effective_alpha_offset = layout.alpha->offset;
        record.horizontal_mirror_offset = image.horizontal_mirror_offset;
        const SourceInfo unavailable{};
        const SourceInfo* source = &unavailable;
        if (image.external_flag == 0) source = &internal;
        if (image.external_flag == 1 && image.group_id < external.size()) {
            source = &external[image.group_id];
        }
        record.color_bounds = range_status(layout.color.offset, layout.color.length, *source);
        record.raw_alpha_bounds = image.alpha_length == 0 ? AssetRangeStatus::NotPresent
            : range_status(image.alpha_offset, image.alpha_length, *source);
        record.alpha_bounds = image.alpha_length == 0 ? AssetRangeStatus::NotPresent
            : layout.alpha ? range_status(layout.alpha->offset, layout.alpha->length, *source)
                           : AssetRangeStatus::Unverified;
        record.payload_in_bounds = record.color_bounds == AssetRangeStatus::InBounds;
        record.color_decoder_supported = supported_color_metadata(image, record.image_kind);
        record.decoder_supported = record.color_decoder_supported && layout.alpha_profile_supported;
        catalog.records.push_back(std::move(record));
    }
}

} // namespace

const char* asset_range_status_name(AssetRangeStatus status) {
    switch (status) {
    case AssetRangeStatus::NotPresent: return "not_present";
    case AssetRangeStatus::InBounds: return "in_bounds";
    case AssetRangeStatus::OutOfBounds: return "out_of_bounds";
    case AssetRangeStatus::SourceUnavailable: return "source_unavailable";
    case AssetRangeStatus::Unverified: return "unverified";
    }
    return "unknown";
}

AssetCatalog scan_asset_catalog(const fs::path& data_directory) {
    std::error_code error;
    const fs::path root = fs::canonical(data_directory, error);
    if (error || !fs::is_directory(root)) {
        throw Sg3LoadError("asset data directory is missing or inaccessible: " +
                           data_directory.string());
    }
    AssetCatalog catalog;
    catalog.data_root = root;
    std::vector<fs::path> candidates;
    fs::recursive_directory_iterator iterator{root, fs::directory_options::skip_permission_denied,
                                              error};
    const fs::recursive_directory_iterator end;
    if (error) throw Sg3LoadError("cannot scan asset data directory: " + error.message());
    while (iterator != end) {
        const fs::directory_entry& entry = *iterator;
        const bool symlink = entry.is_symlink(error);
        if (error) {
            error.clear();
        } else if (symlink) {
            iterator.disable_recursion_pending();
        } else if (is_sg3(entry.path()) && entry.is_regular_file(error) && !error) {
            candidates.push_back(entry.path());
        }
        error.clear();
        iterator.increment(error);
        if (error) {
            catalog.archive_errors.push_back({{}, "directory traversal error: " + error.message()});
            error.clear();
        }
    }
    std::sort(candidates.begin(), candidates.end(), [&](const fs::path& left, const fs::path& right) {
        return left.lexically_relative(root).generic_string() <
               right.lexically_relative(root).generic_string();
    });
    std::map<fs::path, std::optional<std::uint64_t>> size_cache;
    for (const fs::path& candidate : candidates) {
        const fs::path relative = candidate.lexically_relative(root);
        const fs::path canonical = fs::weakly_canonical(candidate, error);
        if (error || !is_within(root, canonical)) {
            catalog.archive_errors.push_back({relative, "SG3 path escapes the data directory"});
            error.clear();
            continue;
        }
        ++catalog.archive_count;
        try {
            append_archive(catalog, candidate, relative, size_cache);
        } catch (const std::exception& failure) {
            std::string message = failure.what();
            const std::string absolute_root = root.generic_string();
            for (std::size_t at = message.find(absolute_root); at != std::string::npos;
                 at = message.find(absolute_root, at + 1U)) {
                message.replace(at, absolute_root.size(), ".");
            }
            catalog.archive_errors.push_back({relative, std::move(message)});
        }
    }
    return catalog;
}

bool asset_matches_filter(const AssetRecord& record, const AssetFilter& filter) {
    if (filter.kind && record.image_kind != *filter.kind) return false;
    if (filter.image_type && record.image_type != *filter.image_type) return false;
    if (filter.archive_substring && record.id.archive_relative_path.generic_string().find(
            *filter.archive_substring) == std::string::npos) return false;
    if (filter.group_substring && record.group_filename.find(*filter.group_substring) ==
            std::string::npos && record.group_description.find(*filter.group_substring) ==
            std::string::npos) return false;
    if (filter.with_alpha && (record.alpha_length != 0) != *filter.with_alpha) return false;
    if (filter.min_width && (record.width < 0 ||
            static_cast<std::uint16_t>(record.width) < *filter.min_width)) return false;
    if (filter.min_height && (record.height < 0 ||
            static_cast<std::uint16_t>(record.height) < *filter.min_height)) return false;
    return true;
}

std::vector<std::size_t> matching_asset_indices(const AssetCatalog& catalog,
                                                 const AssetFilter& filter) {
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < catalog.records.size(); ++index) {
        if (asset_matches_filter(catalog.records[index], filter)) result.push_back(index);
    }
    return result;
}

AssetCounts count_assets(const AssetCatalog& catalog,
                         const std::vector<std::size_t>& indices) {
    AssetCounts counts;
    counts.records = indices.size();
    for (const std::size_t index : indices) {
        const AssetRecord& record = catalog.records.at(index);
        switch (record.image_kind) {
        case Sg3ImageKind::Plain: ++counts.plain; break;
        case Sg3ImageKind::Sprite: ++counts.sprite; break;
        case Sg3ImageKind::Isometric: ++counts.isometric; break;
        case Sg3ImageKind::Unsupported: ++counts.unsupported; break;
        }
        ++counts.type_counts[record.image_type];
        if (record.alpha_length != 0) ++counts.with_alpha;
        if (record.color_bounds == AssetRangeStatus::OutOfBounds) ++counts.color_out_of_bounds;
        if (record.alpha_bounds == AssetRangeStatus::OutOfBounds) ++counts.alpha_out_of_bounds;
        if (record.color_bounds == AssetRangeStatus::SourceUnavailable) {
            ++counts.color_source_unavailable;
        }
        if (record.alpha_bounds == AssetRangeStatus::SourceUnavailable) {
            ++counts.alpha_source_unavailable;
        }
    }
    return counts;
}

} // namespace openemperor::assets
