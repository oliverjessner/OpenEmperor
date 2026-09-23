#include "assets/Sg3RgbaDecoder.h"
#include "assets/Sg3ShadowComposition.h"
#include "assets/RgbaPngEncoder.h"
#include "assets/RgbaPngReader.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

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
    using namespace openemperor::assets;
    struct PixelCase {
        std::uint16_t source;
        std::array<std::uint8_t, 4> expected;
    };
    constexpr std::array<PixelCase, 7> reference_vectors{{
        {0x7c00, {255, 0, 0, 255}},
        {0x03e0, {0, 255, 0, 255}},
        {0x001f, {0, 0, 255, 255}},
        {0x0000, {0, 0, 0, 255}},
        {0x7fff, {255, 255, 255, 255}},
        {0xf81f, {0, 0, 0, 0}},
        {0x4a23, {148, 140, 24, 255}}, // Asymmetric R/G/B bit groups.
    }};
    for (const auto& entry : reference_vectors) {
        if (decode_rgb555_pixel(entry.source) != entry.expected) {
            std::cerr << "RGB555 regression: source 0x" << std::hex << entry.source
                      << " has incorrect RGBA channel order\n";
            return false;
        }
    }
    Sg3Image image;
    image.image_type = 13;
    image.width = 2;
    image.height = 2;
    image.data_length = 8;
    const std::array<std::uint8_t, 8> colors{
        0x00, 0x7c, // Little-endian source value 0x7C00 = red.
        0xe0, 0x03, // Green.
        0x1f, 0x00, // Blue.
        0x1f, 0xf8, // Documented transparent color.
    };
    const std::vector<std::uint8_t> expected{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        0, 0, 0, 0,
    };
    if (required_uncompressed_payload_size(image) != colors.size() ||
        decode_uncompressed_rgba(image, colors).pixels != expected) {
        return false;
    }
    Sg3Image mixed_image;
    mixed_image.image_type = 13;
    mixed_image.width = 1;
    mixed_image.height = 1;
    mixed_image.data_length = 2;
    constexpr std::array<std::uint8_t, 2> mixed_bytes{0x23, 0x4a};
    const auto mixed = decode_uncompressed_rgba(mixed_image, mixed_bytes);
    if (mixed.pixels != std::vector<std::uint8_t>{148, 140, 24, 255} ||
        decode_exported_png(encode_rgba_png(mixed)).pixels !=
            std::vector<std::uint8_t>{148, 140, 24, 255}) return false;
    if (!rejects([&] { decode_uncompressed_rgba(image, std::span{colors}.first(7)); })) {
        return false;
    }
    image.image_type = 256;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;
    image.image_type = 13;
    image.alpha_length = 1;
    if (required_uncompressed_payload_size(image) != colors.size() ||
        decode_uncompressed_rgba(image, colors).pixels != expected) return false;
    image.alpha_length = 0;
    image.image_type = 30;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;
    image.image_type = 13;
    image.data_length = 7;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;

    Sg3Image sprite;
    sprite.image_type = 256;
    sprite.shadow_marker_flag = 1;
    RgbaImage presentation{3, 1, {
        255, 0, 0, 255,
        247, 0, 0, 255,
        0, 255, 0, 128,
    }};
    if (prepare_omega_shadow_composition(sprite, presentation) != 1 ||
        presentation.pixels != std::vector<std::uint8_t>{
            0, 0, 0, 128,
            247, 0, 0, 255,
            0, 255, 0, 128,
        }) return false;
    sprite.shadow_marker_flag = 0;
    const auto unchanged = presentation.pixels;
    if (prepare_omega_shadow_composition(sprite, presentation) != 0 ||
        presentation.pixels != unchanged) return false;
    sprite.shadow_marker_flag = 1;
    sprite.alpha_length = 1;
    if (!rejects([&] { prepare_omega_shadow_composition(sprite, presentation); })) return false;
    return true;
}

} // namespace

int main() {
    if (!run_checks()) {
        std::cerr << "SG3 RGBA decode checks failed\n";
        return 1;
    }
    return 0;
}
