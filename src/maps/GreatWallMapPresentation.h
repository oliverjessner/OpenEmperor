#pragma once
#include "maps/GreatWallSelector.h"
#include "maps/LandscapeInstances.h"
#include "maps/OriginalMapEntities.h"
#include "maps/StoredGraphicsPlan.h"

namespace openemperor::maps {
struct GreatWallPresentationPiece {
    OriginalEntityRecord source;
    std::optional<GreatWallModelPiece> model_piece;
    GreatWallSelection selection;
    std::optional<LandscapeInstanceSpec> geometry;
    std::filesystem::path archive_relative;
    std::string fallback;
};
// Prepared once during load. Missing external restore context is an explicit
// unresolved result; serialized material and historical IDs never fill it.
struct GreatWallMapPresentation {
    std::uint64_t manager_offset=0, manager_bytes=0;
    std::size_t manager_records=0;
    GreatWallRestoreContext restore_context;
    std::vector<GreatWallPresentationPiece> pieces;
    std::vector<std::optional<std::size_t>> piece_by_storage;
    std::map<std::pair<std::uint32_t,std::string>,StoredArchiveRegistration> archives;
    std::string error;
};
GreatWallMapPresentation prepare_great_wall_presentation(
    const OriginalMapEntities& entities,
    const std::map<std::int16_t,GreatWallModelResult>& models,
    GreatWallRestoreContext context={});
void read_great_wall_presentation(StoredGraphicsPlan& plan,
    const EmperorContainer& container,std::size_t part,GreatWallRestoreContext context={});
} // namespace openemperor::maps
