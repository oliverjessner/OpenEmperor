#pragma once

#include "assets/Sg3Archive.h"

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

// Decodes only a documented, uncompressed regular image. No isometric,
// compressed, or alpha-mask semantics are assumed.
std::uint64_t required_uncompressed_payload_size(const Sg3Image& image);
RgbaImage decode_uncompressed_rgba(const Sg3Image& image,
                                   std::span<const std::uint8_t> payload);

} // namespace openemperor::assets
