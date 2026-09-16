#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <span>

namespace openemperor::assets {

// Decodes the documented Omega color stream for Sprite Images only.
// Transparent skips leave the initially transparent RGBA pixels unchanged.
RgbaImage decode_omega_color_rgba(std::span<const std::uint8_t> stream,
                                  std::uint16_t width, std::uint16_t height);

} // namespace openemperor::assets
