#include "assets/Sg3RgbaDecoder.h"

#include <limits>

namespace openemperor::assets {
namespace {

bool is_documented_regular_type(std::uint8_t type) {
    switch (type) {
    case 0:
    case 1:
    case 10:
    case 12:
    case 13:
    case 20:
        return true;
    default:
        return false;
    }
}

std::uint8_t expand_five_bits(std::uint16_t value) {
    const std::uint8_t five = static_cast<std::uint8_t>(value & 0x1f);
    return static_cast<std::uint8_t>((five << 3) | (five >> 2));
}

} // namespace

std::uint64_t required_uncompressed_payload_size(const Sg3Image& image) {
    if (!is_documented_regular_type(image.image_type)) {
        throw Sg3DecodeError("selected image type is not a documented regular image type");
    }
    if (image.fully_compressed_flag != 0 || image.partly_compressed_flag != 0) {
        throw Sg3DecodeError("selected image uses compression; this decoder supports uncompressed images only");
    }
    if (image.alpha_offset != 0 || image.alpha_length != 0) {
        throw Sg3DecodeError("selected image has alpha-mask metadata; alpha-mask decoding is not implemented");
    }
    if (image.width == 0 || image.height == 0) {
        throw Sg3DecodeError("selected image has zero width or height");
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(image.width) * image.height;
    const std::uint64_t expected_data_bytes = pixel_count * 2;
    if (expected_data_bytes != image.data_length) {
        throw Sg3DecodeError("selected image payload size does not match width * height * 2");
    }
    if (pixel_count * 4 > std::numeric_limits<std::size_t>::max()) {
        throw Sg3DecodeError("selected RGBA output is too large for this platform");
    }
    return expected_data_bytes;
}

RgbaImage decode_uncompressed_rgba(const Sg3Image& image,
                                   std::span<const std::uint8_t> payload) {
    const std::uint64_t expected_data_bytes = required_uncompressed_payload_size(image);
    if (payload.size() != expected_data_bytes) {
        throw Sg3DecodeError("selected image payload read is incomplete");
    }
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(image.width) * image.height;
    const std::uint64_t expected_rgba_bytes = pixel_count * 4;

    RgbaImage result;
    result.width = image.width;
    result.height = image.height;
    result.pixels.resize(static_cast<std::size_t>(expected_rgba_bytes));
    for (std::uint64_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        const std::size_t source = static_cast<std::size_t>(pixel_index * 2);
        const std::size_t target = static_cast<std::size_t>(pixel_index * 4);
        const std::uint16_t color = static_cast<std::uint16_t>(payload[source]) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(payload[source + 1]) << 8);
        if (color == 0xf81f) {
            result.pixels[target] = 0;
            result.pixels[target + 1] = 0;
            result.pixels[target + 2] = 0;
            result.pixels[target + 3] = 0;
        } else {
            result.pixels[target] = expand_five_bits(color);
            result.pixels[target + 1] = expand_five_bits(static_cast<std::uint16_t>(color >> 5));
            result.pixels[target + 2] = expand_five_bits(static_cast<std::uint16_t>(color >> 10));
            result.pixels[target + 3] = 255;
        }
    }
    return result;
}

} // namespace openemperor::assets
