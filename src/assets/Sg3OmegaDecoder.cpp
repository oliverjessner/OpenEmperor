#include "assets/Sg3OmegaDecoder.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace openemperor::assets {

RgbaImage decode_omega_color_rgba(std::span<const std::uint8_t> stream,
                                  std::uint16_t width, std::uint16_t height) {
    if (width == 0 || height == 0) {
        throw Sg3DecodeError("Omega image dimensions must be nonzero");
    }
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    const std::uint64_t output_bytes = pixel_count * 4U;
    if (output_bytes > std::numeric_limits<std::size_t>::max()) {
        throw Sg3DecodeError("Omega RGBA output is too large on this platform");
    }
    RgbaImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(output_bytes), 0);

    decode_omega_color_into(stream, image);
    return image;
}

void decode_omega_color_into(std::span<const std::uint8_t> stream, RgbaImage& image) {
    if (image.width == 0 || image.height == 0) {
        throw Sg3DecodeError("Omega destination dimensions must be nonzero");
    }
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(image.width) * image.height;
    if (pixel_count * 4U != image.pixels.size()) {
        throw Sg3DecodeError("Omega destination pixel buffer does not match dimensions");
    }

    std::size_t input = 0;
    std::uint64_t cursor = 0;
    while (input < stream.size()) {
        if (cursor == pixel_count) {
            throw Sg3DecodeError("Omega stream has trailing commands after the final pixel");
        }
        const std::uint8_t command = stream[input++];
        if (command == 255) {
            if (input == stream.size()) {
                throw Sg3DecodeError("Omega skip command is missing its count byte");
            }
            const std::uint8_t skip = stream[input++];
            if (skip > pixel_count - cursor) {
                throw Sg3DecodeError("Omega skip advances beyond image bounds");
            }
            cursor += skip;
            continue;
        }
        const std::size_t count = command;
        if (count > pixel_count - cursor) {
            throw Sg3DecodeError("Omega literal run advances beyond image bounds");
        }
        if (count * 2U > stream.size() - input) {
            throw Sg3DecodeError("Omega literal run is truncated");
        }
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint16_t color = static_cast<std::uint16_t>(stream[input]) |
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(stream[input + 1]) << 8);
            input += 2;
            const auto rgba = decode_rgb555_pixel(color);
            const std::size_t target = static_cast<std::size_t>(cursor * 4U);
            for (std::size_t channel = 0; channel < rgba.size(); ++channel) {
                image.pixels[target + channel] = rgba[channel];
            }
            ++cursor; // A linear cursor naturally wraps to the next row.
        }
    }
}

} // namespace openemperor::assets
