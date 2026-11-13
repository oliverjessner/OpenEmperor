#include "persistence/SandboxSave.h"
#include "maps/MapCatalog.h"
#include "maps/MapRulesCheck.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"

#include <stdexcept>

namespace openemperor::persistence {
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_agriculture_terrain_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative) {
    if (relative.empty() || relative.is_absolute())
        throw std::runtime_error("Agriculture requires a relative original map path");
    for (const auto& part:relative) if (part==".." || part==".")
        throw std::runtime_error("Agriculture original map path is not canonical relative syntax");
    const auto path=maps::resolve_map_path(root,relative);
    const auto container=maps::EmperorContainer::open(path);
    const auto source_hash=maps::map_input_sha256(container);
    if (container.multipart() || container.parts().size()!=1)
        throw std::runtime_error("Agriculture facts require one complete standalone original map part");
    const auto map=maps::read_emperor_map(container,0,true);
    if (map.terrain_raw.logical_offset!=maps::terrain_logical_offset ||
        map.terrain_raw.values.size()!=maps::stored_grid_width*maps::stored_grid_height ||
        map.fertility_bytes.size()!=maps::stored_grid_width*maps::stored_grid_height ||
        !maps::MapGeometry(map.declared_map_size).supported)
        throw std::runtime_error("Agriculture requires complete source terrain and fertility facts");
    if (maps::map_input_sha256(maps::EmperorContainer::open(path))!=source_hash)
        throw std::runtime_error("Original map changed while preparing Agriculture facts");
    return std::make_shared<const simulation::OriginalTerrainFacts>(
        map.terrain_raw.values,map.fertility_bytes,source_hash);
}
} // namespace openemperor::persistence
