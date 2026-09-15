#include "assets/Sg3ImageLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace openemperor::assets {
namespace {

namespace fs = std::filesystem;

std::uint64_t regular_file_size(const fs::path& path, const char* description) {
    std::error_code error;
    if (!fs::is_regular_file(path, error) || error) {
        throw Sg3LoadError(std::string{description} + " is missing or is not a regular file: " + path.string());
    }
    const std::uintmax_t size = fs::file_size(path, error);
    if (error || size > std::numeric_limits<std::uint64_t>::max()) {
        throw Sg3LoadError(std::string{"cannot determine "} + description + " size: " + path.string());
    }
    return static_cast<std::uint64_t>(size);
}

bool remains_under(const fs::path& base, const fs::path& candidate) {
    const fs::path relative = candidate.lexically_relative(base);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

} // namespace

Sg3Archive read_sg3_archive(const fs::path& archive_path) {
    const std::uint64_t size = regular_file_size(archive_path, "SG3 archive");
    constexpr std::size_t prefix_size = 680;
    if (size < prefix_size) {
        throw Sg3ParseError("SG3 file is shorter than the 680-byte header/index prefix");
    }
    std::ifstream input{archive_path, std::ios::binary};
    if (!input) {
        throw Sg3LoadError("cannot open SG3 archive: " + archive_path.string());
    }
    std::array<std::uint8_t, prefix_size> prefix{};
    input.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
    if (input.gcount() != static_cast<std::streamsize>(prefix.size())) {
        throw Sg3LoadError("cannot read SG3 header/index prefix");
    }
    const std::uint64_t required = required_sg3_table_size(prefix, size);
    if (required > std::numeric_limits<std::size_t>::max() ||
        required > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw Sg3LoadError("SG3 metadata table is too large to read on this platform");
    }
    std::vector<std::uint8_t> table(static_cast<std::size_t>(required));
    std::copy(prefix.begin(), prefix.end(), table.begin());
    const std::size_t remaining = table.size() - prefix.size();
    input.read(reinterpret_cast<char*>(table.data() + prefix.size()),
               static_cast<std::streamsize>(remaining));
    if (input.gcount() != static_cast<std::streamsize>(remaining)) {
        throw Sg3LoadError("cannot read complete SG3 metadata table");
    }
    return parse_sg3(table, size);
}

const char* sg3_bitmap_status_name(Sg3BitmapStatus status) {
    switch (status) {
    case Sg3BitmapStatus::Resolved: return "resolved";
    case Sg3BitmapStatus::UnresolvedName: return "unresolved_name";
    case Sg3BitmapStatus::UnsafeRelativeName: return "unsafe_relative_name";
    case Sg3BitmapStatus::InvalidGroup: return "invalid_group";
    case Sg3BitmapStatus::UnknownExternalFlag: return "unknown_external_flag";
    }
    return "unknown_resolution_status";
}

Sg3BitmapLocation resolve_sg3_group_bitmap(const fs::path& archive_path,
                                            const Sg3Group& group, std::size_t group_index) {
    const std::string ref = "group:" + std::to_string(group_index);
    std::string filename = group.filename;
    std::replace(filename.begin(), filename.end(), '\\', '/');
    const fs::path relative{filename};
    if (filename.empty() || relative.is_absolute() || relative.has_root_name() ||
        (filename.size() >= 2 && std::isalpha(static_cast<unsigned char>(filename[0])) != 0 &&
         filename[1] == ':')) {
        return {{}, ref, Sg3BitmapStatus::UnsafeRelativeName};
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return {{}, ref, Sg3BitmapStatus::UnsafeRelativeName};
        }
    }
    std::string extension = relative.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char letter) {
        return static_cast<char>(std::tolower(letter));
    });
    if (extension != ".bmp") {
        return {{}, ref, Sg3BitmapStatus::UnresolvedName};
    }
    fs::path bitmap_name = relative;
    bitmap_name.replace_extension(".555");
    std::error_code error;
    const fs::path absolute_archive = fs::absolute(archive_path, error);
    if (error) {
        return {{}, ref, Sg3BitmapStatus::UnsafeRelativeName};
    }
    const fs::path candidate = absolute_archive.parent_path() / bitmap_name;
    const fs::path base = fs::weakly_canonical(absolute_archive.parent_path(), error);
    if (error) {
        return {{}, ref, Sg3BitmapStatus::UnsafeRelativeName};
    }
    const fs::path target = fs::weakly_canonical(candidate, error);
    if (error || !remains_under(base, target)) {
        return {{}, ref, Sg3BitmapStatus::UnsafeRelativeName};
    }
    return {candidate.lexically_normal(), ref, Sg3BitmapStatus::Resolved};
}

Sg3BitmapLocation resolve_sg3_image_bitmap(const fs::path& archive_path,
                                            const Sg3Archive& archive, const Sg3Image& image) {
    if (image.external_flag == 0) {
        fs::path internal = archive_path;
        internal.replace_extension(".555");
        return {internal.lexically_normal(), "internal", Sg3BitmapStatus::Resolved};
    }
    if (image.external_flag != 1) {
        return {{}, "unknown_external_flag", Sg3BitmapStatus::UnknownExternalFlag};
    }
    if (image.group_id >= archive.groups.size()) {
        return {{}, "invalid_group", Sg3BitmapStatus::InvalidGroup};
    }
    return resolve_sg3_group_bitmap(archive_path, archive.groups[image.group_id], image.group_id);
}

LoadedSg3Image load_sg3_image_with_source(const Sg3ImageRequest& request) {
    std::error_code error;
    const fs::path archive_path = fs::absolute(request.archive_path, error);
    if (error) {
        throw Sg3LoadError("cannot resolve SG3 archive path: " + error.message());
    }
    const Sg3Archive archive = read_sg3_archive(archive_path);
    if (request.image_index >= archive.images.size()) {
        throw Sg3LoadError("selected image index " + std::to_string(request.image_index) +
                           " is outside the SG3 image table");
    }
    const Sg3Image& image = archive.images[request.image_index];
    if (image.group_id >= archive.groups.size()) {
        throw Sg3LoadError("selected image has an out-of-range group ID");
    }
    const std::uint64_t payload_bytes = required_uncompressed_payload_size(image);
    Sg3BitmapLocation bitmap = resolve_sg3_image_bitmap(archive_path, archive, image);
    if (bitmap.status != Sg3BitmapStatus::Resolved) {
        throw Sg3LoadError("selected image .555 path cannot be resolved: " +
                           std::string{sg3_bitmap_status_name(bitmap.status)});
    }
    const std::uint64_t bitmap_size = regular_file_size(bitmap.path, "selected .555 bitmap");
    if (!range_within_file(image.data_offset, payload_bytes, bitmap_size)) {
        throw Sg3LoadError("selected image data range exceeds the actual .555 file size");
    }
    const std::streamoff seek_offset = static_cast<std::streamoff>(image.data_offset);
    if (seek_offset < 0 || static_cast<std::uint64_t>(seek_offset) != image.data_offset ||
        payload_bytes > std::numeric_limits<std::size_t>::max() ||
        payload_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw Sg3LoadError("selected image data range cannot be read on this platform");
    }
    std::ifstream input{bitmap.path, std::ios::binary};
    if (!input) {
        throw Sg3LoadError("cannot open selected .555 bitmap file");
    }
    input.seekg(seek_offset);
    if (!input) {
        throw Sg3LoadError("cannot seek to selected image data offset");
    }
    std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_bytes));
    input.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (input.gcount() != static_cast<std::streamsize>(payload.size())) {
        throw Sg3LoadError("cannot read the complete selected image payload");
    }
    return {decode_uncompressed_rgba(image, payload), std::move(bitmap)};
}

RgbaImage load_sg3_image(const Sg3ImageRequest& request) {
    return load_sg3_image_with_source(request).rgba;
}

} // namespace openemperor::assets
