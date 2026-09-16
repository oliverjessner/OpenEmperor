#include "assets/Sg3IsometricDecoder.h"

#include "assets/Sg3OmegaDecoder.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace openemperor::assets {
namespace {

struct Geometry {
    std::uint64_t tile_width;
    std::uint64_t tile_height;
    std::uint64_t tile_bytes;
    std::uint64_t footprint_size;
};

bool matches(const Geometry& geometry, std::uint64_t width,
             std::uint64_t footprint_height, std::uint64_t base_bytes) {
    const std::uint64_t size = geometry.footprint_size;
    return size != 0 && size * geometry.tile_height == footprint_height &&
           size * (geometry.tile_width + 2U) - 2U == width &&
           size * size * geometry.tile_bytes == base_bytes;
}

Geometry choose_geometry(const Sg3Image& image, std::uint64_t width,
                         std::uint64_t footprint_height, std::uint64_t base_bytes) {
    const std::uint64_t size_hint = image.isometric_size_flag;
    const Geometry classic{58, 30, 1800,
                           size_hint != 0 ? size_hint : footprint_height / 30U};
    const Geometry emperor{78, 40, 3200,
                           size_hint != 0 ? size_hint : footprint_height / 40U};
    if ((size_hint != 0 || footprint_height % 30U == 0) &&
        matches(classic, width, footprint_height, base_bytes)) {
        return classic;
    }
    if ((size_hint != 0 || footprint_height % 40U == 0) &&
        matches(emperor, width, footprint_height, base_bytes)) {
        return emperor;
    }
    throw Sg3DecodeError("isometric size flag, geometry, and base byte count are inconsistent");
}

void draw_tile(std::span<const std::uint8_t> base, std::size_t& source,
               RgbaImage& destination, const Geometry& geometry,
               std::uint64_t origin_x, std::uint64_t origin_y) {
    const std::size_t tile_start = source;
    const std::uint64_t half_height = geometry.tile_height / 2U;
    for (std::uint64_t y = 0; y < geometry.tile_height; ++y) {
        const std::uint64_t start_x = y < half_height
            ? geometry.tile_height - 2U * (y + 1U)
            : 2U * y - geometry.tile_height;
        const std::uint64_t end_x = geometry.tile_width - start_x;
        for (std::uint64_t x = start_x; x < end_x; ++x) {
            if (source > base.size() || base.size() - source < 2) {
                throw Sg3DecodeError("isometric base tile has truncated RGB555 pixels");
            }
            const std::uint64_t target_x = origin_x + x;
            const std::uint64_t target_y = origin_y + y;
            if (target_x >= destination.width || target_y >= destination.height) {
                throw Sg3DecodeError("isometric tile destination exceeds image bounds");
            }
            const std::uint16_t color = static_cast<std::uint16_t>(base[source]) |
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(base[source + 1]) << 8);
            source += 2;
            const auto rgba = decode_rgb555_pixel(color);
            const std::size_t target = static_cast<std::size_t>(
                (target_y * destination.width + target_x) * 4U);
            for (std::size_t channel = 0; channel < rgba.size(); ++channel) {
                destination.pixels[target + channel] = rgba[channel];
            }
        }
    }
    if (source - tile_start != geometry.tile_bytes) {
        throw Sg3DecodeError("isometric tile consumed an unexpected number of bytes");
    }
}

} // namespace

RgbaImage decode_isometric_rgba(const Sg3Image& image,
                                std::span<const std::uint8_t> payload) {
    if (image.image_type != 30) {
        throw Sg3DecodeError("isometric decoder requires SG3 image type 30");
    }
    if (image.width <= 0 || image.height <= 0) {
        throw Sg3DecodeError("isometric image dimensions must be positive");
    }
    if (payload.size() != image.data_length) {
        throw Sg3DecodeError("isometric payload size does not match data_length");
    }
    if (image.uncompressed_length > image.data_length) {
        throw Sg3DecodeError("isometric uncompressed_length exceeds data_length");
    }
    const std::uint64_t width = static_cast<std::uint64_t>(image.width);
    const std::uint64_t height = static_cast<std::uint64_t>(image.height);
    const std::uint64_t footprint_height = (width + 2U) / 2U;
    if (height < footprint_height) {
        throw Sg3DecodeError("isometric footprint exceeds image height");
    }
    const std::uint64_t base_bytes = (width + 2U) * footprint_height;
    if (base_bytes != image.uncompressed_length) {
        throw Sg3DecodeError("isometric uncompressed_length does not match footprint byte count");
    }
    const Geometry geometry = choose_geometry(image, width, footprint_height, base_bytes);
    const std::uint64_t output_bytes = width * height * 4U;
    if (output_bytes > std::numeric_limits<std::size_t>::max()) {
        throw Sg3DecodeError("isometric RGBA output is too large on this platform");
    }
    RgbaImage destination;
    destination.width = static_cast<std::uint16_t>(image.width);
    destination.height = static_cast<std::uint16_t>(image.height);
    destination.pixels.resize(static_cast<std::size_t>(output_bytes), 0);
    const auto base = payload.first(image.uncompressed_length);
    std::size_t source = 0;
    std::uint64_t tiles = 0;
    std::uint64_t origin_y = height - footprint_height;
    const std::uint64_t size = geometry.footprint_size;
    for (std::uint64_t row = 0; row < 2U * size - 1U; ++row) {
        const std::uint64_t row_tiles = row < size ? row + 1U : 2U * size - row - 1U;
        std::uint64_t origin_x = (row < size ? size - row - 1U : row - size + 1U) * geometry.tile_height;
        for (std::uint64_t column = 0; column < row_tiles; ++column) {
            draw_tile(base, source, destination, geometry, origin_x, origin_y);
            origin_x += geometry.tile_width + 2U;
            ++tiles;
        }
        origin_y += geometry.tile_height / 2U;
    }
    if (tiles != size * size || source != base.size()) {
        throw Sg3DecodeError("isometric footprint did not consume its complete base stream");
    }
    const auto overlay = payload.subspan(image.uncompressed_length);
    if (!overlay.empty()) {
        decode_omega_color_into(overlay, destination);
    }
    return destination;
}

} // namespace openemperor::assets
