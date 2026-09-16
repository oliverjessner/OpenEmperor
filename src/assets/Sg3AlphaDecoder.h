#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <span>

namespace openemperor::assets {

// Applies a version-214 Omega alpha stream to straight RGBA pixels in place.
// Skipped pixels retain their existing alpha; literal bytes affect only A.
void apply_omega_alpha_mask(std::span<const std::uint8_t> stream,
                            RgbaImage& destination);

} // namespace openemperor::assets
