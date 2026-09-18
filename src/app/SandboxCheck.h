#pragma once

#include <filesystem>
#include "simulation/World.h"

namespace openemperor {
int run_sandbox_check(const std::filesystem::path& data_root,
                      const std::filesystem::path& map_relative,
                      simulation::RulesProfile rules=simulation::RulesProfile::LogisticsV1,
                      bool resume_check=false);
}
