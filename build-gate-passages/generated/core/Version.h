#pragma once

#include <string_view>

namespace openemperor::version {

inline constexpr std::string_view display = "0.1.0-alpha.2";
inline constexpr std::string_view project = "0.1.0";
inline constexpr std::string_view revision = "d48b00aec762";
inline constexpr bool dirty = false;
inline constexpr std::string_view build_type = "Release";
inline constexpr std::string_view target = "Darwin arm64";

} // namespace openemperor::version
