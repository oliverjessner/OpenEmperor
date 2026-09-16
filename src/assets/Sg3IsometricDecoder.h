#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <span>

namespace openemperor::assets {

// Decodes only documented Type-30 diamond bases and their optional Omega
// color overlay. The shared loader applies any separate alpha stream afterward.
// Horizontal mirroring is not applied.
RgbaImage decode_isometric_rgba(const Sg3Image& image,
                                std::span<const std::uint8_t> payload);

} // namespace openemperor::assets
