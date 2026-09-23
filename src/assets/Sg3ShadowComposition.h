#pragma once

#include "assets/Sg3Archive.h"
#include "assets/Sg3RgbaDecoder.h"

#include <cstddef>

namespace openemperor::assets {

// Converts the studied Emperor Omega-sprite shadow marker to a straight-RGBA
// source-over representation. The general RGB555 decoder intentionally keeps
// 0x7c00 as opaque red; callers opt into this presentation step only when the
// record's verified byte-59 flag is set.
std::size_t prepare_omega_shadow_composition(const Sg3Image& metadata,
                                             RgbaImage& image);

} // namespace openemperor::assets
