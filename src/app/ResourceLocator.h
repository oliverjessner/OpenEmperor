#pragma once

#include <filesystem>

namespace openemperor {

// `base_directory` is the executable directory, not the process working directory.
std::filesystem::path locate_resource_root(const std::filesystem::path& base_directory,
                                           const std::filesystem::path& injected_root = {});

} // namespace openemperor
