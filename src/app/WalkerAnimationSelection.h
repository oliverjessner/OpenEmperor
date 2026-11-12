#pragma once

#include "assets/WalkerVisualProfile.h"

#include <filesystem>

namespace openemperor {

// Preparation-only qualification of the built-in schema-7 contract. Generic
// custom profiles remain previews; selecting native pixels does not qualify a
// profession. Source group keys resolve through the shared runtime layout.
// Throws before publication if the selected frames, directions, authored feet,
// cadence or idle differ from that bounded contract. Does not read World state.
void validate_builtin_walker_animations(const std::filesystem::path& data_root,
                                      const assets::WalkerVisualProfile& profile);

} // namespace openemperor
