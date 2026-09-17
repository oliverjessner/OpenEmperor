#include "assets/Sg3ImageLoader.h"
#include "assets/Sg3AlphaDecoder.h"
#include "assets/Sg3IsometricDecoder.h"
#include "assets/Sg3OmegaDecoder.h"

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

std::vector<std::uint8_t> read_bitmap_range(std::ifstream& input,
                                             std::uint64_t bitmap_size,
                                             std::uint64_t offset,
                                             std::uint64_t length,
                                             const char* description) {
    if (!range_within_file(offset, length, bitmap_size)) {
        throw Sg3LoadError(std::string{"selected image "} + description +
                           " range exceeds the actual .555 file size");
    }
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
        length > std::numeric_limits<std::size_t>::max() ||
        length > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw Sg3LoadError(std::string{"selected image "} + description +
                           " range cannot be read on this platform");
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) {
        throw Sg3LoadError(std::string{"cannot seek to selected image "} + description + " offset");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw Sg3LoadError(std::string{"cannot read complete selected image "} + description);
    }
    return bytes;
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
    const Sg3ImageKind kind = classify_sg3_image_type(image.image_type);
    if (kind == Sg3ImageKind::Unsupported) {
        throw Sg3DecodeError("unsupported SG3 image type " + std::to_string(image.image_type));
    }
    if (image.width <= 0 || image.height <= 0) {
        throw Sg3DecodeError("selected image has non-positive width or height");
    }
    if (image.group_id >= archive.groups.size()) {
        throw Sg3LoadError("selected image has an out-of-range group ID");
    }
    const std::uint64_t payload_bytes = kind == Sg3ImageKind::Plain
        ? required_uncompressed_payload_size(image) : image.data_length;
    Sg3BitmapLocation bitmap = resolve_sg3_image_bitmap(archive_path, archive, image);
    if (bitmap.status != Sg3BitmapStatus::Resolved) {
        throw Sg3LoadError("selected image .555 path cannot be resolved: " +
                           std::string{sg3_bitmap_status_name(bitmap.status)});
    }
    const std::uint64_t bitmap_size = regular_file_size(bitmap.path, "selected .555 bitmap");
    std::ifstream input{bitmap.path, std::ios::binary};
    if (!input) {
        throw Sg3LoadError("cannot open selected .555 bitmap file");
    }
    const std::vector<std::uint8_t> payload = read_bitmap_range(
        input, bitmap_size, image.data_offset, payload_bytes, "data");
    RgbaImage rgba;
    switch (kind) {
    case Sg3ImageKind::Plain:
        rgba = decode_uncompressed_rgba(image, payload);
        break;
    case Sg3ImageKind::Sprite:
        rgba = decode_omega_color_rgba(payload, static_cast<std::uint16_t>(image.width),
                                       static_cast<std::uint16_t>(image.height));
        break;
    case Sg3ImageKind::Isometric:
        rgba = decode_isometric_rgba(image, payload);
        break;
    case Sg3ImageKind::Unsupported:
        throw Sg3DecodeError("unsupported SG3 image type " + std::to_string(image.image_type));
    }
    if (image.alpha_length != 0 && !request.ignore_alpha) {
        const std::vector<std::uint8_t> alpha = read_bitmap_range(
            input, bitmap_size, image.alpha_offset, image.alpha_length, "alpha");
        apply_omega_alpha_mask(alpha, rgba);
    }
    return {std::move(rgba), std::move(bitmap)};
}

RgbaImage load_sg3_image(const Sg3ImageRequest& request) {
    return load_sg3_image_with_source(request).rgba;
}

} // namespace openemperor::assets
