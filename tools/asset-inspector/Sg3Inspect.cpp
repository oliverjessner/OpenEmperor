#include "Sg3Inspect.h"

#include "assets/Sg3Archive.h"
#include "assets/Sg3RgbaDecoder.h"
#include "assets/RgbaPngEncoder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using openemperor::assets::Sg3Archive;
using openemperor::assets::Sg3Group;
using openemperor::assets::Sg3Image;

struct BitmapFile {
    fs::path path;
    std::optional<std::uint64_t> size;
    std::string status;
};

struct ImageValidation {
    std::string bitmap_ref;
    std::string data_bounds;
    std::string alpha_bounds;
    std::string part_length_bounds;
};

std::string hex_bytes(std::span<const std::uint8_t> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 15]);
    }
    return result;
}

void json_string(std::ostream& output, const std::string& value) {
    constexpr char digits[] = "0123456789abcdef";
    output << '"';
    for (const char letter : value) {
        const unsigned char byte = static_cast<unsigned char>(letter);
        if (byte == '"' || byte == '\\') {
            output << '\\' << static_cast<char>(byte);
        } else if (byte < 0x20 || byte >= 0x80) {
            output << "\\u00" << digits[byte >> 4] << digits[byte & 15];
        } else {
            output << static_cast<char>(byte);
        }
    }
    output << '"';
}

void json_key_string(std::ostream& output, const char* key, const std::string& value) {
    output << '"' << key << "\":";
    json_string(output, value);
}

std::string lowercase(std::string value) {
    for (char& letter : value) {
        letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    }
    return value;
}

BitmapFile inspect_file(const fs::path& path) {
    BitmapFile file{path.lexically_normal(), std::nullopt, "missing"};
    std::error_code error;
    if (!fs::is_regular_file(file.path, error) || error) {
        return file;
    }
    const std::uintmax_t size = fs::file_size(file.path, error);
    if (!error) {
        file.size = static_cast<std::uint64_t>(size);
        file.status = "present";
    } else {
        file.status = "size_unavailable";
    }
    return file;
}

BitmapFile inspect_external_file(const fs::path& archive_path, const Sg3Group& group) {
    std::string filename = group.filename;
    std::replace(filename.begin(), filename.end(), '\\', '/');
    const fs::path relative{filename};
    if (filename.empty() || relative.is_absolute() || relative.has_root_name() ||
        lowercase(relative.extension().string()) != ".bmp") {
        return BitmapFile{{}, std::nullopt, "unresolved_name"};
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return BitmapFile{{}, std::nullopt, "unsafe_relative_name"};
        }
    }
    fs::path bitmap_name = relative;
    bitmap_name.replace_extension(".555");
    return inspect_file(archive_path.parent_path() / bitmap_name);
}

std::string check_bounds(std::uint32_t offset, std::uint32_t length, const BitmapFile& file) {
    if (!file.size) {
        return file.status;
    }
    if (!openemperor::assets::range_within_file(offset, length, *file.size)) {
        return "out_of_bounds";
    }
    return "in_bounds";
}

Sg3Archive read_archive(const fs::path& path, std::uint64_t actual_file_size) {
    if (actual_file_size < 680) {
        throw openemperor::assets::Sg3ParseError("SG3 file is shorter than the 680-byte header/index prefix");
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("cannot open SG3 file");
    }
    std::array<std::uint8_t, 680> prefix{};
    input.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
    if (input.gcount() != static_cast<std::streamsize>(prefix.size())) {
        throw std::runtime_error("cannot read SG3 header/index prefix");
    }
    const std::uint64_t required = openemperor::assets::required_sg3_table_size(prefix, actual_file_size);
    if (required > std::numeric_limits<std::size_t>::max() ||
        required > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("SG3 metadata table is too large to read on this platform");
    }
    std::vector<std::uint8_t> table(static_cast<std::size_t>(required));
    std::copy(prefix.begin(), prefix.end(), table.begin());
    const std::size_t remaining = table.size() - prefix.size();
    input.read(reinterpret_cast<char*>(table.data() + prefix.size()),
               static_cast<std::streamsize>(remaining));
    if (input.gcount() != static_cast<std::streamsize>(remaining)) {
        throw std::runtime_error("cannot read complete SG3 metadata table");
    }
    return openemperor::assets::parse_sg3(table, actual_file_size);
}

void print_bitmap(std::ostream& output, const BitmapFile& file, const std::string& ref) {
    output << "    {";
    json_key_string(output, "ref", ref);
    output << ',';
    json_key_string(output, "status", file.status);
    output << ',';
    json_key_string(output, "path", file.path.string());
    output << ",\"size\":";
    if (file.size) {
        output << *file.size;
    } else {
        output << "null";
    }
    output << '}';
}

void print_group(std::ostream& output, const Sg3Group& group, std::size_t index) {
    output << "    {\"index\":" << index << ',';
    json_key_string(output, "filename", group.filename);
    output << ',';
    json_key_string(output, "description", group.description);
    output << ",\"width\":" << group.width
           << ",\"height\":" << group.height
           << ",\"image_count\":" << group.image_count
           << ",\"first_image_index\":" << group.first_image_index
           << ",\"last_image_index\":" << group.last_image_index << ',';
    json_key_string(output, "unknown_136_199_hex", hex_bytes(std::span{group.raw}.subspan(136, 64)));
    output << '}';
}

void print_image(std::ostream& output, const Sg3Image& image, std::size_t index,
                 const ImageValidation& validation, bool has_alpha) {
    output << "    {\"index\":" << index
           << ",\"data_offset\":" << image.data_offset
           << ",\"data_length\":" << image.data_length
           << ",\"uncompressed_part_length\":" << image.uncompressed_part_length
           << ",\"width\":" << image.width
           << ",\"height\":" << image.height
           << ",\"animation_sprites\":" << image.animation_sprites
           << ",\"animation_x_offset\":" << image.animation_x_offset
           << ",\"animation_y_offset\":" << image.animation_y_offset
           << ",\"reversible_animation_flag\":" << static_cast<unsigned int>(image.reversible_animation_flag)
           << ",\"image_type\":" << static_cast<unsigned int>(image.image_type)
           << ",\"fully_compressed_flag\":" << static_cast<unsigned int>(image.fully_compressed_flag)
           << ",\"external_flag\":" << static_cast<unsigned int>(image.external_flag)
           << ",\"partly_compressed_flag\":" << static_cast<unsigned int>(image.partly_compressed_flag)
           << ",\"group_id\":" << static_cast<unsigned int>(image.group_id)
           << ",\"animation_speed_id\":" << static_cast<unsigned int>(image.animation_speed_id);
    if (has_alpha) {
        output << ",\"alpha_offset\":" << image.alpha_offset
               << ",\"alpha_length\":" << image.alpha_length;
    }
    output << ',';
    json_key_string(output, "bitmap_ref", validation.bitmap_ref);
    output << ',';
    json_key_string(output, "data_bounds", validation.data_bounds);
    output << ',';
    json_key_string(output, "alpha_bounds", validation.alpha_bounds);
    output << ',';
    json_key_string(output, "uncompressed_part_bounds", validation.part_length_bounds);
    output << ',';
    json_key_string(output, "documented_zero_12_15_hex", hex_bytes(std::span{image.raw}.subspan(12, 4)));
    output << ',';
    json_key_string(output, "unknown_16_19_hex", hex_bytes(std::span{image.raw}.subspan(16, 4)));
    output << ',';
    json_key_string(output, "unknown_24_29_hex", hex_bytes(std::span{image.raw}.subspan(24, 6)));
    output << ',';
    json_key_string(output, "unknown_32_33_hex", hex_bytes(std::span{image.raw}.subspan(32, 2)));
    output << ',';
    json_key_string(output, "unknown_38_47_hex", hex_bytes(std::span{image.raw}.subspan(38, 10)));
    output << ',';
    json_key_string(output, "unknown_49_hex", hex_bytes(std::span{image.raw}.subspan(49, 1)));
    output << ',';
    json_key_string(output, "unknown_54_55_hex", hex_bytes(std::span{image.raw}.subspan(54, 2)));
    output << ',';
    json_key_string(output, "unknown_57_hex", hex_bytes(std::span{image.raw}.subspan(57, 1)));
    output << ',';
    json_key_string(output, "unknown_59_hex", hex_bytes(std::span{image.raw}.subspan(59, 1)));
    output << ',';
    json_key_string(output, "documented_zero_60_63_hex", hex_bytes(std::span{image.raw}.subspan(60, 4)));
    output << '}';
}

} // namespace

void inspect_sg3(const fs::path& path, std::uint64_t actual_file_size, std::ostream& output) {
    const Sg3Archive archive = read_archive(path, actual_file_size);
    const BitmapFile internal = inspect_file(fs::path{path}.replace_extension(".555"));
    std::vector<BitmapFile> external;
    external.reserve(archive.groups.size());
    for (const Sg3Group& group : archive.groups) {
        external.push_back(inspect_external_file(path, group));
    }

    output << "{\n  \"format\":\"SG3\",\n  \"archive_path\":";
    json_string(output, path.lexically_normal().string());
    output << ",\n  \"actual_file_size\":" << archive.actual_file_size
           << ",\n  \"header\":{\"reported_file_size\":" << archive.header.reported_file_size
           << ",\"version\":" << archive.header.version
           << ",\"image_capacity\":" << archive.header.image_capacity
           << ",\"reported_images_in_use\":" << archive.header.reported_images_in_use
           << ",\"group_count\":" << archive.header.group_count
           << ",\"reported_bitmap_data_size\":" << archive.header.reported_bitmap_data_size
           << ",\"reported_internal_555_size\":" << archive.header.reported_internal_555_size
           << ",\"reported_external_555_size\":" << archive.header.reported_external_555_size << ',';
    json_key_string(output, "unknown_8_11_hex", hex_bytes(std::span{archive.header.raw}.subspan(8, 4)));
    output << ',';
    json_key_string(output, "unknown_24_27_hex", hex_bytes(std::span{archive.header.raw}.subspan(24, 4)));
    output << ',';
    json_key_string(output, "unknown_40_79_hex", hex_bytes(std::span{archive.header.raw}.subspan(40, 40)));
    output << "},\n  \"layout\":{\"header_bytes\":80,\"index_bytes\":600,"
           << "\"reserved_group_slots\":200,\"group_record_bytes\":200,"
           << "\"image_table_offset\":40680,\"image_record_bytes\":"
           << (archive.header.version == 214 ? 72 : 64)
           << ",\"parsed_table_bytes\":" << archive.parsed_table_size
           << ",\"unparsed_trailing_bytes\":" << archive.actual_file_size - archive.parsed_table_size
           << "},\n  \"index\":[";
    for (std::size_t index = 0; index < archive.index.size(); ++index) {
        if (index != 0) output << ',';
        output << archive.index[index];
    }
    output << "],\n  \"bitmap_files\":[\n";
    print_bitmap(output, internal, "internal");
    for (std::size_t index = 0; index < external.size(); ++index) {
        output << ",\n";
        print_bitmap(output, external[index], "group:" + std::to_string(index));
    }
    output << "\n  ],\n  \"groups\":[\n";
    for (std::size_t index = 0; index < archive.groups.size(); ++index) {
        if (index != 0) output << ",\n";
        print_group(output, archive.groups[index], index);
    }
    output << "\n  ],\n  \"images\":[\n";

    std::uint64_t invalid_index_count = 0;
    for (const std::uint16_t image_id : archive.index) {
        if (image_id != 0 && image_id >= archive.images.size()) {
            ++invalid_index_count;
        }
    }
    std::uint64_t invalid_group_range_count = 0;
    for (const Sg3Group& group : archive.groups) {
        if (group.image_count != 0 &&
            (group.first_image_index >= archive.images.size() ||
             group.last_image_index >= archive.images.size() ||
             group.first_image_index > group.last_image_index)) {
            ++invalid_group_range_count;
        }
    }
    std::uint64_t outside_count = 0;
    std::uint64_t unavailable_count = 0;
    std::uint64_t invalid_group_count = 0;
    std::uint64_t invalid_image_group_count = 0;
    std::uint64_t unknown_external_flag_count = 0;
    std::uint64_t part_length_count = 0;
    for (std::size_t index = 0; index < archive.images.size(); ++index) {
        const Sg3Image& image = archive.images[index];
        if ((image.data_length != 0 || image.alpha_length != 0) &&
            image.group_id >= archive.groups.size()) {
            ++invalid_image_group_count;
        }
        ImageValidation validation;
        const BitmapFile* source = &internal;
        validation.bitmap_ref = "internal";
        if (image.external_flag == 1) {
            if (image.group_id >= external.size()) {
                source = nullptr;
                validation.bitmap_ref = "invalid_group";
                ++invalid_group_count;
            } else {
                source = &external[image.group_id];
                validation.bitmap_ref = "group:" + std::to_string(image.group_id);
            }
        } else if (image.external_flag != 0) {
            source = nullptr;
            validation.bitmap_ref = "unknown_external_flag";
            ++unknown_external_flag_count;
        }
        if (source) {
            validation.data_bounds = check_bounds(image.data_offset, image.data_length, *source);
            validation.alpha_bounds = archive.header.version == 214 &&
                (image.alpha_offset != 0 || image.alpha_length != 0)
                ? check_bounds(image.alpha_offset, image.alpha_length, *source)
                : "not_present";
        } else {
            validation.data_bounds = "source_unresolved";
            validation.alpha_bounds = "source_unresolved";
        }
        validation.part_length_bounds = image.uncompressed_part_length <= image.data_length
            ? "within_data_length" : "exceeds_data_length";
        if (validation.data_bounds == "out_of_bounds") ++outside_count;
        if (validation.alpha_bounds == "out_of_bounds") ++outside_count;
        if (validation.data_bounds != "in_bounds" && validation.data_bounds != "out_of_bounds") ++unavailable_count;
        if (validation.part_length_bounds == "exceeds_data_length") ++part_length_count;
        if (index != 0) output << ",\n";
        print_image(output, image, index, validation, archive.header.version == 214);
    }
    output << "\n  ],\n  \"validation\":{\"reported_file_size_matches_actual\":"
           << (archive.header.reported_file_size == archive.actual_file_size ? "true" : "false")
           << ",\"reported_internal_555_size_matches_actual\":";
    if (internal.size) {
        output << (archive.header.reported_internal_555_size == *internal.size ? "true" : "false");
    } else {
        output << "null";
    }
    output
           << ",\"reported_images_in_use_within_capacity\":"
           << (archive.header.reported_images_in_use <= archive.header.image_capacity ? "true" : "false")
           << ",\"out_of_bounds_bitmap_ranges\":" << outside_count
           << ",\"image_ranges_with_unavailable_source\":" << unavailable_count
           << ",\"out_of_range_index_entries\":" << invalid_index_count
           << ",\"out_of_range_group_image_ranges\":" << invalid_group_range_count
           << ",\"out_of_range_image_group_ids\":" << invalid_image_group_count
           << ",\"invalid_external_group_references\":" << invalid_group_count
           << ",\"unknown_external_flag_values\":" << unknown_external_flag_count
           << ",\"uncompressed_part_exceeds_data_length\":" << part_length_count
           << "}\n}\n";
}

void decode_one_sg3_image(const fs::path& path, std::uint64_t actual_file_size,
                          std::uint32_t image_index, const fs::path& image_output_path,
                          ImageOutputFormat output_format,
                          std::ostream& output) {
    const Sg3Archive archive = read_archive(path, actual_file_size);
    if (image_index >= archive.images.size()) {
        throw openemperor::assets::Sg3DecodeError("selected image index is outside the SG3 image table");
    }
    const Sg3Image& image = archive.images[image_index];
    if (image.group_id >= archive.groups.size()) {
        throw openemperor::assets::Sg3DecodeError("selected image has an out-of-range group ID");
    }
    const std::uint64_t payload_bytes = openemperor::assets::required_uncompressed_payload_size(image);

    BitmapFile bitmap;
    std::string bitmap_ref = "internal";
    if (image.external_flag == 0) {
        bitmap = inspect_file(fs::path{path}.replace_extension(".555"));
    } else if (image.external_flag == 1) {
        bitmap = inspect_external_file(path, archive.groups[image.group_id]);
        bitmap_ref = "group:" + std::to_string(image.group_id);
    } else {
        throw openemperor::assets::Sg3DecodeError("selected image has an undocumented external flag value");
    }
    if (!bitmap.size) {
        throw openemperor::assets::Sg3DecodeError("selected image bitmap file is unavailable: " + bitmap.status);
    }
    if (!openemperor::assets::range_within_file(image.data_offset, payload_bytes, *bitmap.size)) {
        throw openemperor::assets::Sg3DecodeError("selected image data range exceeds the actual .555 file size");
    }
    const std::streamoff seek_offset = static_cast<std::streamoff>(image.data_offset);
    if (seek_offset < 0 || static_cast<std::uint64_t>(seek_offset) != image.data_offset ||
        payload_bytes > std::numeric_limits<std::size_t>::max() ||
        payload_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw openemperor::assets::Sg3DecodeError("selected image data range cannot be read on this platform");
    }

    std::ifstream bitmap_input{bitmap.path, std::ios::binary};
    if (!bitmap_input) {
        throw std::runtime_error("cannot open selected .555 bitmap file");
    }
    bitmap_input.seekg(seek_offset);
    if (!bitmap_input) {
        throw std::runtime_error("cannot seek to selected image data offset");
    }
    std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_bytes));
    bitmap_input.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (bitmap_input.gcount() != static_cast<std::streamsize>(payload.size())) {
        throw std::runtime_error("cannot read the complete selected image payload");
    }
    const openemperor::assets::RgbaImage rgba = openemperor::assets::decode_uncompressed_rgba(image, payload);
    const std::vector<std::uint8_t> png = output_format == ImageOutputFormat::Png
        ? openemperor::assets::encode_rgba_png(rgba) : std::vector<std::uint8_t>{};
    const std::vector<std::uint8_t>& output_bytes = output_format == ImageOutputFormat::Png ? png : rgba.pixels;
    const char* format_name = output_format == ImageOutputFormat::Png ? "PNG" : "RGBA8";

    std::error_code error;
    const fs::path absolute_output = fs::absolute(image_output_path, error);
    if (error) {
        throw std::runtime_error("cannot resolve image output path: " + error.message());
    }
    if (!absolute_output.parent_path().empty()) {
        fs::create_directories(absolute_output.parent_path(), error);
        if (error) {
            throw std::runtime_error("cannot create image output directory: " + error.message());
        }
    }
    const fs::file_status existing = fs::symlink_status(absolute_output, error);
    if (!error && existing.type() != fs::file_type::not_found) {
        throw std::runtime_error("image output path already exists; choose a new path");
    }
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("cannot inspect image output path: " + error.message());
    }
    if (output_bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("image output is too large to write on this platform");
    }
    std::ofstream image_output{absolute_output, std::ios::binary};
    if (!image_output) {
        throw std::runtime_error("cannot create image output file");
    }
    image_output.write(reinterpret_cast<const char*>(output_bytes.data()),
                       static_cast<std::streamsize>(output_bytes.size()));
    image_output.close();
    if (!image_output) {
        std::error_code cleanup_error;
        fs::remove(absolute_output, cleanup_error);
        throw std::runtime_error("cannot write complete image output file");
    }

    output << "{\"format\":\"" << format_name << "\",\"source_archive\":";
    json_string(output, path.lexically_normal().string());
    output << ",\"image_index\":" << image_index
           << ",\"width\":" << rgba.width
           << ",\"height\":" << rgba.height
           << ",\"rgba_bytes\":" << rgba.pixels.size()
           << ",\"output_bytes\":" << output_bytes.size() << ',';
    json_key_string(output, "bitmap_ref", bitmap_ref);
    output << ',';
    json_key_string(output, "bitmap_path", bitmap.path.string());
    output << ',';
    json_key_string(output, "output_path", absolute_output.lexically_normal().string());
    output << "}\n";
}
