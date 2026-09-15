#include "assets/RgbaPngEncoder.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace openemperor::assets {
namespace {

void append_be32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = 0xffffffffU;
    for (const std::uint8_t byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) != 0 ? 0xedb88320U : 0U);
        }
    }
    return crc ^ 0xffffffffU;
}

void append_chunk(std::vector<std::uint8_t>& png,
                  const std::array<std::uint8_t, 4>& type,
                  std::span<const std::uint8_t> data) {
    if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("PNG chunk exceeds its 32-bit length limit");
    }
    append_be32(png, static_cast<std::uint32_t>(data.size()));
    const std::size_t crc_start = png.size();
    png.insert(png.end(), type.begin(), type.end());
    png.insert(png.end(), data.begin(), data.end());
    append_be32(png, crc32(std::span{png}.subspan(crc_start)));
}

std::uint32_t adler32(std::span<const std::uint8_t> bytes) {
    std::uint32_t first = 1;
    std::uint32_t second = 0;
    for (const std::uint8_t byte : bytes) {
        first = (first + byte) % 65521U;
        second = (second + first) % 65521U;
    }
    return (second << 16) | first;
}

} // namespace

std::vector<std::uint8_t> encode_rgba_png(const RgbaImage& image) {
    if (image.width == 0 || image.height == 0) {
        throw std::invalid_argument("PNG image dimensions must be nonzero");
    }
    const std::uint64_t row_bytes = static_cast<std::uint64_t>(image.width) * 4U;
    const std::uint64_t pixel_bytes = row_bytes * image.height;
    const std::uint64_t filtered_bytes = (row_bytes + 1U) * image.height;
    if (pixel_bytes != image.pixels.size()) {
        throw std::invalid_argument("RGBA pixel buffer does not match image dimensions");
    }
    if (filtered_bytes > std::numeric_limits<std::size_t>::max() ||
        filtered_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw std::length_error("PNG scanlines are too large on this platform");
    }

    // PNG filter type 0 leaves each RGBA scanline unchanged.
    std::vector<std::uint8_t> scanlines;
    scanlines.reserve(static_cast<std::size_t>(filtered_bytes));
    const std::size_t row_size = static_cast<std::size_t>(row_bytes);
    for (std::size_t row = 0; row < image.height; ++row) {
        scanlines.push_back(0);
        const auto begin = image.pixels.begin() + static_cast<std::ptrdiff_t>(row * row_size);
        scanlines.insert(scanlines.end(), begin, begin + static_cast<std::ptrdiff_t>(row_size));
    }

    // A zlib stream containing DEFLATE stored blocks avoids an external encoder
    // dependency. Each stored block is limited to 65,535 bytes by RFC 1951.
    std::vector<std::uint8_t> compressed{0x78, 0x01};
    for (std::size_t offset = 0; offset < scanlines.size();) {
        const std::size_t count = std::min<std::size_t>(65535, scanlines.size() - offset);
        const bool final = offset + count == scanlines.size();
        const std::uint16_t length = static_cast<std::uint16_t>(count);
        const std::uint16_t complement = static_cast<std::uint16_t>(~length);
        compressed.push_back(final ? 0x01 : 0x00);
        compressed.push_back(static_cast<std::uint8_t>(length));
        compressed.push_back(static_cast<std::uint8_t>(length >> 8));
        compressed.push_back(static_cast<std::uint8_t>(complement));
        compressed.push_back(static_cast<std::uint8_t>(complement >> 8));
        compressed.insert(compressed.end(), scanlines.begin() + static_cast<std::ptrdiff_t>(offset),
                          scanlines.begin() + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;
    }
    append_be32(compressed, adler32(scanlines));

    constexpr std::array<std::uint8_t, 8> signature{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    std::vector<std::uint8_t> png(signature.begin(), signature.end());
    std::vector<std::uint8_t> header;
    append_be32(header, image.width);
    append_be32(header, image.height);
    header.insert(header.end(), {8, 6, 0, 0, 0}); // RGBA8, standard compression/filter, no interlace.
    append_chunk(png, {'I', 'H', 'D', 'R'}, header);
    constexpr std::size_t max_idat_chunk = 1024 * 1024;
    for (std::size_t offset = 0; offset < compressed.size();) {
        const std::size_t count = std::min(max_idat_chunk, compressed.size() - offset);
        append_chunk(png, {'I', 'D', 'A', 'T'}, std::span{compressed}.subspan(offset, count));
        offset += count;
    }
    append_chunk(png, {'I', 'E', 'N', 'D'}, {});
    return png;
}

} // namespace openemperor::assets
