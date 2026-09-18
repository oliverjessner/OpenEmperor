#pragma once

#include "maps/StoredGraphicsPlan.h"

#include <filesystem>

namespace openemperor {
int run_map_render_check(const std::filesystem::path& data_root,
                         const std::filesystem::path& relative_map,
                         maps::FootprintPolicy policy,
                         maps::StoredGraphicsProfile profile = maps::StoredGraphicsProfile::Base);
}
