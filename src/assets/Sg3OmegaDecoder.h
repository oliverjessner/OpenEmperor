#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <span>

namespace openemperor::assets {

// Decodes the documented Omega color stream for Sprite Images.
// Transparent skips leave the initially transparent RGBA pixels unchanged.
RgbaImage decode_omega_color_rgba(std::span<const std::uint8_t> stream,
                                  std::uint16_t width, std::uint16_t height);

// Applies the same stream to existing pixels. Skips preserve destination data.
void decode_omega_color_into(std::span<const std::uint8_t> stream, RgbaImage& destination);

} // namespace openemperor::assets
