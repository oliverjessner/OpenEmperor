#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <vector>

namespace openemperor::assets {

// Encodes an already decoded RGBA8 image as a PNG. This has no SG3 semantics.
std::vector<std::uint8_t> encode_rgba_png(const RgbaImage& image);

} // namespace openemperor::assets
