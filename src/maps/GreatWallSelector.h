#pragma once

#include "maps/GreatWallModels.h"
#include "maps/ResourceGroupLookup.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace openemperor::maps {

struct GreatWallRegistration {
    std::uint32_t slot = 0;
    std::string_view archive_basename;
};

// cMonInfo restore overwrites stored +5c at 0x562e2b..0x562e44 using
// 0x563720(-1): mode 1 gives material 3, otherwise current-player mission
// goals choose 2/3 and the no-goal result becomes 1. Standalone map bytes do
// not establish this context. A caller must supply the separately evidenced
// derived material; raw serialized material is deliberately not an input.
struct GreatWallRestoreContext {
    std::optional<std::int32_t> material;
    unsigned camera_view = 0; // Original even value 0,2,4,6.
};

struct GreatWallSelectorInput {
    GreatWallModelPiece model_piece;
    std::int32_t phase = -1; // Serialized cMonInfo +08, independent of model type.
    std::uint8_t orientation = 0; // Serialized cMonInfo +84, quarter turns.
    GreatWallRestoreContext restore_context;
};

struct GreatWallSelection {
    bool supported = false;
    std::optional<GreatWallRegistration> required_registration;
    std::uint32_t slot = 0;
    ResourceGroupKey group;
    unsigned variant = 0;
    unsigned flags = 0;
    unsigned side = 0;
    unsigned effective_view = 0;
    const char* reason = "unsupported Great Wall state";
};

// Pure description of 0x57bba0/0x57cb10/0x57d2b0/0x57d860. The caller
// applies any registration in restored entity order to its own immutable
// lookup snapshot. Gates and roads inherit the current slot: their numeric
// phase must not select a same-numbered Great Wall archive. Resolve group and
// variant through ResourceGroupLookup/RuntimeArchiveLayout; no physical ID is
// inferred here. Claim placement, heights and full draw composition are separate.
GreatWallSelection select_great_wall(const GreatWallSelectorInput& input);

} // namespace openemperor::maps
