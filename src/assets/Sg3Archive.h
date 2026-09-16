#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace openemperor::assets {

class Sg3ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Sg3Header {
    std::array<std::uint8_t, 80> raw{};
    std::uint32_t reported_file_size = 0;
    std::uint32_t version = 0;
    std::uint32_t image_capacity = 0;
    std::uint32_t reported_images_in_use = 0;
    std::uint32_t group_count = 0;
    std::uint32_t reported_bitmap_data_size = 0;
    std::uint32_t reported_internal_555_size = 0;
    std::uint32_t reported_external_555_size = 0;
};

struct Sg3Group {
    std::array<std::uint8_t, 200> raw{};
    std::string filename;
    std::string description;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t image_count = 0;
    std::uint32_t first_image_index = 0;
    std::uint32_t last_image_index = 0;
};

struct Sg3Image {
    std::array<std::uint8_t, 72> raw{};
    std::uint32_t data_offset = 0;
    std::uint32_t data_length = 0;
    std::uint32_t uncompressed_length = 0;
    std::uint32_t horizontal_mirror_offset = 0; // Parsed, not applied.
    std::int16_t width = 0;
    std::int16_t height = 0;
    std::uint16_t animation_sprites = 0;
    std::int16_t animation_x_offset = 0;
    std::int16_t animation_y_offset = 0;
    std::uint8_t reversible_animation_flag = 0;
    std::uint16_t image_type = 0;
    std::uint8_t external_flag = 0;
    std::uint8_t isometric_size_flag = 0; // Meaning is not established.
    std::uint8_t group_id = 0;
    std::uint8_t animation_speed_id = 0;
    std::uint32_t alpha_offset = 0;
    std::uint32_t alpha_length = 0;
};

enum class Sg3ImageKind { Plain, Sprite, Isometric, Unsupported };
Sg3ImageKind classify_sg3_image_type(std::uint16_t image_type);
const char* sg3_image_kind_name(Sg3ImageKind kind);

struct Sg3Archive {
    std::uint64_t actual_file_size = 0;
    std::uint64_t parsed_table_size = 0;
    Sg3Header header;
    std::array<std::uint16_t, 300> index{};
    std::vector<Sg3Group> groups;
    std::vector<Sg3Image> images;
};

// The first 680 bytes are enough to determine how much of the SG3 table to read.
// Unknown trailing bytes are not interpreted.
std::uint64_t required_sg3_table_size(std::span<const std::uint8_t> prefix,
                                      std::uint64_t actual_file_size);
Sg3Archive parse_sg3(std::span<const std::uint8_t> table,
                     std::uint64_t actual_file_size);
bool range_within_file(std::uint64_t offset, std::uint64_t length,
                       std::uint64_t file_size);

} // namespace openemperor::assets
