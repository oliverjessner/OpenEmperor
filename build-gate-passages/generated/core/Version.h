#pragma once

#include <string_view>

namespace openemperor::version {

inline constexpr std::string_view display = "0.1.0-alpha.2";
inline constexpr std::string_view project = "0.1.0";
inline constexpr std::string_view revision = "8c2ef2a63417";
inline constexpr bool dirty = true;
inline constexpr std::string_view build_type = "Release";
inline constexpr std::string_view target = "Darwin arm64";

} // namespace openemperor::version
