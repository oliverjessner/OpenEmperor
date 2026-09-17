#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <span>
#include <string>

namespace openemperor::assets {

struct AlphaSyntaxResult {
    bool valid = false;
    std::string failure;
    std::uint64_t bytes_consumed = 0;
    std::uint64_t pixels_advanced = 0;
    std::uint64_t final_pixel_cursor = 0;
    std::uint64_t image_pixel_count = 0;
};

// Diagnostic interpretation of the documented Omega alpha command grammar.
// Does not write pixels or change the production decoder.
AlphaSyntaxResult inspect_omega_alpha_syntax(std::span<const std::uint8_t> stream,
                                              std::uint16_t width, std::uint16_t height);

// Applies a version-214 Omega alpha stream to straight RGBA pixels in place.
// Skipped pixels retain their existing alpha; literal bytes affect only A.
void apply_omega_alpha_mask(std::span<const std::uint8_t> stream,
                            RgbaImage& destination);

} // namespace openemperor::assets
