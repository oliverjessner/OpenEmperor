#include "assets/Sg3Archive.h"

#include <algorithm>
#include <string_view>

namespace openemperor::assets {
namespace {

constexpr std::size_t header_size = 80;
constexpr std::size_t index_size = 600;
constexpr std::size_t group_size = 200;
constexpr std::size_t group_capacity = 200;
constexpr std::size_t group_start = header_size + index_size;
constexpr std::size_t image_start = group_start + group_capacity * group_size;

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::span<const std::uint8_t> slice(std::size_t offset, std::size_t length) const {
        if (offset > bytes_.size() || length > bytes_.size() - offset) {
            throw Sg3ParseError("SG3 field extends beyond the available table");
        }
        return bytes_.subspan(offset, length);
    }

    std::uint16_t u16(std::size_t offset) const {
        const auto field = slice(offset, 2);
        return static_cast<std::uint16_t>(field[0]) |
               static_cast<std::uint16_t>(static_cast<std::uint16_t>(field[1]) << 8);
    }

    std::int16_t i16(std::size_t offset) const {
        const std::uint16_t raw = u16(offset);
        const std::int32_t signed_value = raw < 0x8000
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw) - 0x10000;
        return static_cast<std::int16_t>(signed_value);
    }

    std::uint32_t u32(std::size_t offset) const {
        const auto field = slice(offset, 4);
        return static_cast<std::uint32_t>(field[0]) |
               (static_cast<std::uint32_t>(field[1]) << 8) |
               (static_cast<std::uint32_t>(field[2]) << 16) |
               (static_cast<std::uint32_t>(field[3]) << 24);
    }

private:
    std::span<const std::uint8_t> bytes_;
};

template <std::size_t N>
void copy_record(std::array<std::uint8_t, N>& destination,
                 std::span<const std::uint8_t> source) {
    std::copy(source.begin(), source.end(), destination.begin());
}

std::string bounded_c_string(std::span<const std::uint8_t> bytes) {
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<std::size_t>(end - bytes.begin()));
}

} // namespace

std::uint64_t required_sg3_table_size(std::span<const std::uint8_t> prefix,
                                      std::uint64_t actual_file_size) {
    const Reader reader{prefix};
    reader.slice(0, group_start);

    const std::uint32_t version = reader.u32(4);
    if (version != 213 && version != 214) {
        throw Sg3ParseError("unsupported SG3 version (expected 213 or 214)");
    }
    const std::uint32_t group_count = reader.u32(20);
    if (group_count > group_capacity) {
        throw Sg3ParseError("SG3 group count exceeds the 200 reserved group slots");
    }
    if (actual_file_size < image_start) {
        throw Sg3ParseError("SG3 file ends before the image metadata table");
    }

    const std::uint64_t stride = version == 214 ? 72 : 64;
    const std::uint64_t capacity = reader.u32(12);
    if (capacity > (actual_file_size - image_start) / stride) {
        throw Sg3ParseError("SG3 image metadata table extends beyond the file");
    }
    return image_start + capacity * stride;
}

Sg3Archive parse_sg3(std::span<const std::uint8_t> table,
                     std::uint64_t actual_file_size) {
    const std::uint64_t required = required_sg3_table_size(table, actual_file_size);
    if (required > table.size()) {
        throw Sg3ParseError("SG3 image metadata table is truncated");
    }
    const Reader reader{table};

    Sg3Archive archive;
    archive.actual_file_size = actual_file_size;
    archive.parsed_table_size = required;
    copy_record(archive.header.raw, reader.slice(0, header_size));
    archive.header.reported_file_size = reader.u32(0);
    archive.header.version = reader.u32(4);
    archive.header.image_capacity = reader.u32(12);
    archive.header.reported_images_in_use = reader.u32(16);
    archive.header.group_count = reader.u32(20);
    archive.header.reported_bitmap_data_size = reader.u32(28);
    archive.header.reported_internal_555_size = reader.u32(32);
    archive.header.reported_external_555_size = reader.u32(36);

    for (std::size_t index = 0; index < archive.index.size(); ++index) {
        archive.index[index] = reader.u16(header_size + index * 2);
    }

    archive.groups.reserve(archive.header.group_count);
    for (std::uint32_t index = 0; index < archive.header.group_count; ++index) {
        const std::size_t offset = group_start + static_cast<std::size_t>(index) * group_size;
        const auto record = reader.slice(offset, group_size);
        Sg3Group group;
        copy_record(group.raw, record);
        group.filename = bounded_c_string(record.subspan(0, 65));
        group.description = bounded_c_string(record.subspan(65, 51));
        group.width = reader.u32(offset + 116);
        group.height = reader.u32(offset + 120);
        group.image_count = reader.u32(offset + 124);
        group.first_image_index = reader.u32(offset + 128);
        group.last_image_index = reader.u32(offset + 132);
        archive.groups.push_back(std::move(group));
    }

    const std::size_t stride = archive.header.version == 214 ? 72 : 64;
    archive.images.reserve(archive.header.image_capacity);
    for (std::uint32_t index = 0; index < archive.header.image_capacity; ++index) {
        const std::size_t offset = image_start + static_cast<std::size_t>(index) * stride;
        const auto record = reader.slice(offset, stride);
        Sg3Image image;
        copy_record(image.raw, record);
        image.data_offset = reader.u32(offset);
        image.data_length = reader.u32(offset + 4);
        image.uncompressed_part_length = reader.u32(offset + 8);
        image.width = reader.u16(offset + 20);
        image.height = reader.u16(offset + 22);
        image.animation_sprites = reader.u16(offset + 30);
        image.animation_x_offset = reader.i16(offset + 34);
        image.animation_y_offset = reader.i16(offset + 36);
        image.reversible_animation_flag = record[48];
        image.image_type = record[50];
        image.fully_compressed_flag = record[51];
        image.external_flag = record[52];
        image.partly_compressed_flag = record[53];
        image.group_id = record[56];
        image.animation_speed_id = record[58];
        if (stride == 72) {
            image.alpha_offset = reader.u32(offset + 64);
            image.alpha_length = reader.u32(offset + 68);
        }
        archive.images.push_back(std::move(image));
    }
    return archive;
}

bool range_within_file(std::uint64_t offset, std::uint64_t length,
                       std::uint64_t file_size) {
    return offset <= file_size && length <= file_size - offset;
}

} // namespace openemperor::assets
