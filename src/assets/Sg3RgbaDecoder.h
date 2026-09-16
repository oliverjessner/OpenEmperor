#pragma once

#include "assets/Sg3Archive.h"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace openemperor::assets {

class Sg3DecodeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct RgbaImage {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> pixels; // Row-major, 8-bit R, G, B, A per pixel.
};

std::array<std::uint8_t, 4> decode_rgb555_pixel(std::uint16_t color);

// Decodes only a documented plain image. No isometric or separate alpha-mask
// semantics are assumed.
std::uint64_t required_uncompressed_payload_size(const Sg3Image& image);
RgbaImage decode_uncompressed_rgba(const Sg3Image& image,
                                   std::span<const std::uint8_t> payload);

} // namespace openemperor::assets
