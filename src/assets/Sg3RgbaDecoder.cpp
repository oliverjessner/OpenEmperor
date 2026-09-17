#include "assets/Sg3RgbaDecoder.h"

#include <limits>

namespace openemperor::assets {
namespace {

std::uint8_t expand_five_bits(std::uint16_t value) {
    const std::uint8_t five = static_cast<std::uint8_t>(value & 0x1f);
    return static_cast<std::uint8_t>((five << 3) | (five >> 2));
}

} // namespace

std::array<std::uint8_t, 4> decode_rgb555_pixel(std::uint16_t color) {
    if (color == 0xf81f) {
        return {0, 0, 0, 0};
    }
    // The source word is already assembled little-endian. These shifts select
    // its channel bit groups; the returned bytes remain straight R, G, B, A.
    return {expand_five_bits(static_cast<std::uint16_t>(color >> 10)),
            expand_five_bits(static_cast<std::uint16_t>(color >> 5)),
            expand_five_bits(color), 255};
}

std::uint64_t required_uncompressed_payload_size(const Sg3Image& image) {
    if (classify_sg3_image_type(image.image_type) != Sg3ImageKind::Plain) {
        throw Sg3DecodeError("selected image type is not a documented plain image type");
    }
    if (image.width <= 0 || image.height <= 0) {
        throw Sg3DecodeError("selected image has non-positive width or height");
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(image.width) *
                                      static_cast<std::uint64_t>(image.height);
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
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(image.width) *
                                      static_cast<std::uint64_t>(image.height);
    const std::uint64_t expected_rgba_bytes = pixel_count * 4;

    RgbaImage result;
    result.width = static_cast<std::uint16_t>(image.width);
    result.height = static_cast<std::uint16_t>(image.height);
    result.pixels.resize(static_cast<std::size_t>(expected_rgba_bytes));
    for (std::uint64_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        const std::size_t source = static_cast<std::size_t>(pixel_index * 2);
        const std::size_t target = static_cast<std::size_t>(pixel_index * 4);
        const std::uint16_t color = static_cast<std::uint16_t>(payload[source]) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(payload[source + 1]) << 8);
        const auto rgba = decode_rgb555_pixel(color);
        for (std::size_t channel = 0; channel < rgba.size(); ++channel) {
            result.pixels[target + channel] = rgba[channel];
        }
    }
    return result;
}

} // namespace openemperor::assets
