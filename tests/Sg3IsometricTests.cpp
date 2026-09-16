#include "assets/Sg3IsometricDecoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;
using Pixel = std::array<std::uint8_t, 4>;
constexpr Pixel transparent{0, 0, 0, 0};
constexpr Pixel red{255, 0, 0, 255};
constexpr Pixel green{0, 255, 0, 255};
constexpr Pixel blue{0, 0, 255, 255};
constexpr Pixel white{255, 255, 255, 255};

Bytes tile(std::uint16_t rgb555, std::size_t byte_count) {
    Bytes bytes(byte_count);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2) {
        bytes[offset] = static_cast<std::uint8_t>(rgb555);
        bytes[offset + 1] = static_cast<std::uint8_t>(rgb555 >> 8);
    }
    return bytes;
}

openemperor::assets::Sg3Image metadata(std::int16_t width, std::int16_t height,
                                       std::uint8_t size, std::size_t base_length,
                                       std::size_t overlay_length = 0) {
    openemperor::assets::Sg3Image image;
    image.image_type = 30;
    image.width = width;
    image.height = height;
    image.isometric_size_flag = size;
    image.uncompressed_length = static_cast<std::uint32_t>(base_length);
    image.data_length = static_cast<std::uint32_t>(base_length + overlay_length);
    return image;
}

Pixel pixel(const openemperor::assets::RgbaImage& image, std::size_t x, std::size_t y) {
    const std::size_t offset = (y * image.width + x) * 4;
    return {image.pixels[offset], image.pixels[offset + 1],
            image.pixels[offset + 2], image.pixels[offset + 3]};
}

template <typename Operation>
bool rejects(Operation operation) {
    try {
        operation();
        return false;
    } catch (const openemperor::assets::Sg3DecodeError&) {
        return true;
    }
}

bool run_checks() {
    using openemperor::assets::decode_isometric_rgba;
    const Bytes classic = tile(0x001f, 1800);
    const auto classic_image = decode_isometric_rgba(metadata(58, 30, 1, classic.size()), classic);
    if (classic_image.width != 58 || classic_image.height != 30 ||
        pixel(classic_image, 0, 0) != transparent ||
        pixel(classic_image, 28, 0) != red || pixel(classic_image, 0, 14) != red ||
        pixel(classic_image, 28, 29) != red || pixel(classic_image, 57, 0) != transparent) return false;
    if (decode_isometric_rgba(metadata(58, 30, 0, classic.size()), classic).pixels !=
        classic_image.pixels) return false;

    const Bytes emperor = tile(0x03e0, 3200);
    const auto emperor_image = decode_isometric_rgba(metadata(78, 40, 1, emperor.size()), emperor);
    if (emperor_image.width != 78 || emperor_image.height != 40 ||
        pixel(emperor_image, 0, 0) != transparent ||
        pixel(emperor_image, 38, 0) != green || pixel(emperor_image, 0, 19) != green ||
        pixel(emperor_image, 38, 39) != green) return false;
    if (decode_isometric_rgba(metadata(78, 40, 0, emperor.size()), emperor).pixels !=
        emperor_image.pixels) return false;

    Bytes four_tiles = tile(0x001f, 3200);
    for (const auto& next : {tile(0x03e0, 3200), tile(0x7c00, 3200), tile(0x7fff, 3200)}) {
        four_tiles.insert(four_tiles.end(), next.begin(), next.end());
    }
    const auto footprint = decode_isometric_rgba(metadata(158, 80, 2, four_tiles.size()), four_tiles);
    if (footprint.width != 158 || footprint.height != 80 ||
        pixel(footprint, 78, 0) != red || pixel(footprint, 38, 20) != green ||
        pixel(footprint, 118, 20) != blue || pixel(footprint, 78, 40) != white ||
        pixel(footprint, 0, 0) != transparent) return false;
    if (decode_isometric_rgba(metadata(158, 80, 0, four_tiles.size()), four_tiles).pixels !=
        footprint.pixels) return false;

    const auto lowered = decode_isometric_rgba(metadata(78, 50, 1, emperor.size()), emperor);
    if (pixel(lowered, 38, 0) != transparent || pixel(lowered, 38, 9) != transparent ||
        pixel(lowered, 38, 10) != green || pixel(lowered, 38, 49) != green) return false;

    Bytes overlaid = classic;
    const Bytes omega{255, 28, 1, 0xe0, 0x03};
    overlaid.insert(overlaid.end(), omega.begin(), omega.end());
    const auto overlay = decode_isometric_rgba(metadata(58, 30, 1, classic.size(), omega.size()), overlaid);
    if (pixel(overlay, 28, 0) != green || pixel(overlay, 29, 0) != red ||
        pixel(overlay, 0, 0) != transparent) return false;

    auto bad_length = metadata(78, 40, 1, emperor.size());
    bad_length.uncompressed_length = 3198;
    auto beyond_payload = metadata(78, 40, 1, emperor.size());
    beyond_payload.uncompressed_length = 3201;
    Bytes truncated_overlay = emperor;
    truncated_overlay.push_back(1); // Literal without a complete RGB555 value.
    if (!rejects([&] { decode_isometric_rgba(bad_length, emperor); }) ||
        !rejects([&] { decode_isometric_rgba(beyond_payload, emperor); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(78, 40, 1, emperor.size(), 1),
                                             truncated_overlay); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(78, 40, 1, emperor.size()),
                                             std::span{emperor}.first(emperor.size() - 1)); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(60, 31, 0, 1922), Bytes(1922)); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(78, 39, 1, emperor.size()), emperor); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(78, 40, 2, emperor.size()), emperor); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(0, 40, 1, emperor.size()), emperor); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(-1, 40, 1, emperor.size()), emperor); }) ||
        !rejects([&] { decode_isometric_rgba(metadata(78, -1, 1, emperor.size()), emperor); })) return false;
    auto bad_alpha = metadata(78, 40, 1, emperor.size());
    bad_alpha.alpha_length = 1;
    return rejects([&] { decode_isometric_rgba(bad_alpha, emperor); });
}

} // namespace

int main() {
    if (!run_checks()) {
        std::cerr << "SG3 isometric decoder checks failed\n";
        return 1;
    }
    return 0;
}
