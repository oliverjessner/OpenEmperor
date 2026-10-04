#pragma once

#include "maps/EmperorMap.h"
#include "maps/StoredGraphicsPlan.h"
#include "maps/GreatWallSelector.h"

#include <filesystem>

namespace openemperor::maps {

struct StoredMapSession {
    ParsedEmperorMap map;
    StoredGraphicsPlan plan;
};

StoredMapSession load_stored_map_session(const std::filesystem::path& data_root,
                                        const std::filesystem::path& map_relative,
                                        FootprintPolicy policy,
                                        StoredGraphicsProfile profile = StoredGraphicsProfile::Base,
                                        GreatWallPresentationMode great_wall = GreatWallPresentationMode::Automatic);

} // namespace openemperor::maps
