#pragma once

#include "maps/EmperorMap.h"
#include "maps/StoredGraphicsPlan.h"
#include "maps/GreatWallSelector.h"

#include <filesystem>
#include <string>

namespace openemperor::maps {

struct StoredMapSession {
    ParsedEmperorMap map;
    StoredGraphicsPlan plan;
    // Session-only content binding for explicitly prechecked new-game inputs.
    // Historical load callers retain their existing full validation path.
    std::string input_sha256{};
};

StoredMapSession load_stored_map_session(const std::filesystem::path& data_root,
                                        const std::filesystem::path& map_relative,
                                        FootprintPolicy policy,
                                        StoredGraphicsProfile profile = StoredGraphicsProfile::Base,
                                        GreatWallPresentationMode great_wall = GreatWallPresentationMode::Automatic,
                                        const std::string& expected_input_sha256 = {});

} // namespace openemperor::maps
