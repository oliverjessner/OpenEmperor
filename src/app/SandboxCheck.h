#pragma once

#include <filesystem>

namespace openemperor {
int run_sandbox_check(const std::filesystem::path& data_root,
                      const std::filesystem::path& map_relative);
}
