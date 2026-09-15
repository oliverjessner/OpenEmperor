#include "assets/RgbaPngReader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace openemperor::assets {
namespace {

constexpr std::uint64_t max_file_bytes = 256ULL * 1024 * 1024;

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("PNG contains a truncated 32-bit value");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           bytes[offset + 3];
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

std::uint32_t adler32(std::span<const std::uint8_t> bytes) {
    std::uint32_t first = 1;
    std::uint32_t second = 0;
    for (const std::uint8_t byte : bytes) {
        first = (first + byte) % 65521U;
        second = (second + first) % 65521U;
    }
    return (second << 16) | first;
}

std::vector<std::uint8_t> decompress_stored_zlib(std::span<const std::uint8_t> stream,
                                                  std::size_t expected_bytes) {
    if (stream.size() < 11) {
        throw std::runtime_error("PNG zlib stream is truncated");
    }
    const std::uint8_t cmf = stream[0];
    const std::uint8_t flg = stream[1];
    if ((cmf & 15U) != 8 || (cmf >> 4) > 7 ||
        ((static_cast<unsigned>(cmf) << 8) | flg) % 31U != 0 || (flg & 0x20U) != 0) {
        throw std::runtime_error("PNG has an unsupported zlib header");
    }
    const std::size_t deflate_end = stream.size() - 4;
    std::vector<std::uint8_t> raw;
    raw.reserve(expected_bytes);
    std::size_t offset = 2;
    bool final = false;
    while (!final) {
        if (offset >= deflate_end) {
            throw std::runtime_error("PNG DEFLATE block header is truncated");
        }
        const std::uint8_t header = stream[offset++];
        final = (header & 1U) != 0;
        if ((header & 6U) != 0) {
            throw std::runtime_error("PNG preview supports stored DEFLATE blocks only");
        }
        if (deflate_end - offset < 4) {
            throw std::runtime_error("PNG stored block length is truncated");
        }
        const std::uint16_t length = static_cast<std::uint16_t>(stream[offset] | (stream[offset + 1] << 8));
        const std::uint16_t complement = static_cast<std::uint16_t>(stream[offset + 2] | (stream[offset + 3] << 8));
        offset += 4;
        if (static_cast<std::uint16_t>(~length) != complement ||
            length > deflate_end - offset || length > expected_bytes - raw.size()) {
            throw std::runtime_error("PNG stored block has an invalid length");
        }
        raw.insert(raw.end(), stream.begin() + static_cast<std::ptrdiff_t>(offset),
                   stream.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
    }
    if (offset != deflate_end || raw.size() != expected_bytes ||
        adler32(raw) != be32(stream, deflate_end)) {
        throw std::runtime_error("PNG decompressed data length or checksum is invalid");
    }
    return raw;
}

} // namespace

RgbaImage decode_exported_png(std::span<const std::uint8_t> png) {
    constexpr std::array<std::uint8_t, 8> signature{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (png.size() < signature.size() || !std::equal(signature.begin(), signature.end(), png.begin())) {
        throw std::runtime_error("file is not a PNG");
    }
    std::size_t offset = signature.size();
    bool have_header = false;
    bool have_data = false;
    bool have_end = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> compressed;
    while (offset < png.size() && !have_end) {
        if (png.size() - offset < 12) {
            throw std::runtime_error("PNG chunk header is truncated");
        }
        const std::uint32_t length = be32(png, offset);
        if (length > png.size() - offset - 12) {
            throw std::runtime_error("PNG chunk exceeds file bounds");
        }
        const auto type = png.subspan(offset + 4, 4);
        const auto data = png.subspan(offset + 8, length);
        if (crc32(png.subspan(offset + 4, static_cast<std::size_t>(length) + 4)) !=
            be32(png, offset + 8 + length)) {
            throw std::runtime_error("PNG chunk CRC is invalid");
        }
        const std::string name(type.begin(), type.end());
        if (name == "IHDR") {
            if (have_header || have_data || length != 13) {
                throw std::runtime_error("PNG IHDR has invalid position or length");
            }
            width = be32(data, 0);
            height = be32(data, 4);
            if (width == 0 || height == 0 ||
                width > std::numeric_limits<std::uint16_t>::max() ||
                height > std::numeric_limits<std::uint16_t>::max() ||
                data[8] != 8 || data[9] != 6 || data[10] != 0 || data[11] != 0 || data[12] != 0) {
                throw std::runtime_error("PNG preview requires non-interlaced RGBA8 images");
            }
            have_header = true;
        } else if (name == "IDAT") {
            if (!have_header || compressed.size() > max_file_bytes ||
                length > max_file_bytes - compressed.size()) {
                throw std::runtime_error("PNG IDAT is missing its header or exceeds the preview limit");
            }
            have_data = true;
            compressed.insert(compressed.end(), data.begin(), data.end());
        } else if (name == "IEND") {
            if (!have_header || !have_data || length != 0) {
                throw std::runtime_error("PNG IEND is invalid");
            }
            have_end = true;
        } else {
            throw std::runtime_error("PNG preview supports only the exported IHDR/IDAT/IEND subset");
        }
        offset += static_cast<std::size_t>(length) + 12;
    }
    if (!have_end || offset != png.size()) {
        throw std::runtime_error("PNG is missing IEND or has trailing bytes");
    }
    const std::uint64_t row_bytes = static_cast<std::uint64_t>(width) * 4U;
    const std::uint64_t filtered_bytes = (row_bytes + 1U) * height;
    if (filtered_bytes > max_file_bytes || filtered_bytes > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("PNG decoded image exceeds the preview limit");
    }
    const std::vector<std::uint8_t> scanlines = decompress_stored_zlib(compressed,
                                                  static_cast<std::size_t>(filtered_bytes));
    RgbaImage image;
    image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    image.pixels.reserve(static_cast<std::size_t>(row_bytes * height));
    const std::size_t stride = static_cast<std::size_t>(row_bytes + 1U);
    for (std::size_t row = 0; row < height; ++row) {
        const std::size_t start = row * stride;
        if (scanlines[start] != 0) {
            throw std::runtime_error("PNG preview supports filter type 0 only");
        }
        image.pixels.insert(image.pixels.end(), scanlines.begin() + static_cast<std::ptrdiff_t>(start + 1),
                            scanlines.begin() + static_cast<std::ptrdiff_t>(start + stride));
    }
    return image;
}

RgbaImage read_exported_png(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        throw std::runtime_error("PNG preview file is not a regular file");
    }
    const std::uintmax_t file_size = std::filesystem::file_size(path, error);
    if (error || file_size > max_file_bytes ||
        file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("PNG preview file size is unavailable or exceeds the preview limit");
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("cannot open PNG preview file");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("cannot read complete PNG preview file");
    }
    return decode_exported_png(bytes);
}

} // namespace openemperor::assets
