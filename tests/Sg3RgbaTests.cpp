#include "assets/Sg3RgbaDecoder.h"

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
    Sg3Image image;
    image.image_type = 13;
    image.width = 2;
    image.height = 2;
    image.data_length = 8;
    const std::array<std::uint8_t, 8> colors{
        0x1f, 0x00, // Red = five bits at the low end.
        0xe0, 0x03, // Green.
        0x00, 0x7c, // Blue.
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
    if (!rejects([&] { decode_uncompressed_rgba(image, std::span{colors}.first(7)); })) {
        return false;
    }
    image.fully_compressed_flag = 1;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;
    image.fully_compressed_flag = 0;
    image.alpha_length = 1;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;
    image.alpha_length = 0;
    image.image_type = 30;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;
    image.image_type = 13;
    image.data_length = 7;
    if (!rejects([&] { required_uncompressed_payload_size(image); })) return false;
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
