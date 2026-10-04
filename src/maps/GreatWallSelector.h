#pragma once

#include "maps/GreatWallModels.h"
#include "maps/LandscapeSelectors.h"
#include "maps/ResourceGroupLookup.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace openemperor::maps {

struct GreatWallRegistration {
    std::uint32_t slot = 0;
    std::string_view archive_basename;
};

enum class GreatWallPresentationMode {
    Automatic, HistoricalFallback, PreviewRuined, PreviewEarthen, PreviewStone
};
const char* great_wall_presentation_mode_name(GreatWallPresentationMode mode);
std::optional<GreatWallPresentationMode> parse_great_wall_presentation_mode(std::string_view name);
enum class GreatWallContextSource { Unavailable, VerifiedOriginal, ExplicitPreview };
const char* great_wall_context_source_name(GreatWallContextSource source);

// cMonInfo restore overwrites stored +5c at 0x562e2b..0x562e44 using
// 0x563720(-1): mode 1 gives material 3, otherwise current-player mission
// goals choose 2/3 and the no-goal result becomes 1. Standalone map bytes do
// not establish this context. A caller must supply the separately evidenced
// derived material; raw serialized material is deliberately not an input.
struct GreatWallRestoreContext {
    std::optional<std::int32_t> material;
    unsigned camera_view = 0; // Original even value 0,2,4,6.
    GreatWallContextSource source = GreatWallContextSource::Unavailable;
    GreatWallPresentationMode mode = GreatWallPresentationMode::Automatic;
    const char* reason = "original restore inputs unavailable";
};
bool great_wall_original_context_verified(const GreatWallRestoreContext& context);
GreatWallRestoreContext great_wall_context_from_mode(GreatWallPresentationMode mode,
    GreatWallRestoreContext original = {});
struct GreatWallOriginalGoal { std::int32_t type=0, value=0; };
// A present mode is an independently validated original value. Non-editor
// modes also require a complete validated current-player list; an absent list
// and a validated empty list have different meanings. Never fill from map IDs.
struct GreatWallOriginalRestoreInputs {
    std::optional<std::int32_t> mode;
    std::optional<std::vector<GreatWallOriginalGoal>> current_player_goals;
    bool goals_validated=false;
    unsigned camera_view=0;
};
GreatWallRestoreContext resolve_original_great_wall_context(const GreatWallOriginalRestoreInputs& inputs);

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
    GreatWallRestoreContext restore_context;
    SelectorEvidence evidence = SelectorEvidence::Unresolved;
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
