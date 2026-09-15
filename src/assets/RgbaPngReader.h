#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace openemperor::assets {

// Reads the narrow RGBA8, filter-0, stored-DEFLATE PNG subset emitted by
// RgbaPngEncoder. Unsupported standard PNG features are rejected explicitly.
RgbaImage decode_exported_png(std::span<const std::uint8_t> png);
RgbaImage read_exported_png(const std::filesystem::path& path);

} // namespace openemperor::assets
