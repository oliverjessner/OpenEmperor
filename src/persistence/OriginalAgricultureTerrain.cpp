#include "persistence/SandboxSave.h"
#include "maps/MapCatalog.h"
#include "maps/MapRulesCheck.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"

#include <stdexcept>

namespace openemperor::persistence {
namespace {
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_agriculture_terrain_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative,bool irrigation) {
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
    if (!irrigation)
        return std::make_shared<const simulation::OriginalTerrainFacts>(
            map.terrain_raw.values,map.fertility_bytes,source_hash);
    const maps::MapGeometry geometry(map.declared_map_size);
    std::vector<std::uint8_t> boundary(simulation::OriginalTerrainFacts::cell_count,0);
    for (std::uint32_t y=3;y+3<maps::stored_grid_height;++y) {
        for (std::uint32_t x=3;x+3<maps::stored_grid_width;++x) {
            const bool allowed=geometry.contains({x,y}) && geometry.contains({x-3,y}) &&
                geometry.contains({x+3,y}) && geometry.contains({x,y-3}) && geometry.contains({x,y+3});
            boundary[std::size_t(y)*maps::stored_grid_width+x]=allowed ? 1:0;
        }
    }
    return std::make_shared<const simulation::OriginalTerrainFacts>(
        map.terrain_raw.values,map.fertility_bytes,source_hash,boundary);
}
} // namespace

std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_agriculture_terrain_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative) {
    return prepare_agriculture_terrain_facts(root,relative,false);
}

std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_irrigation_terrain_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative) {
    return prepare_agriculture_terrain_facts(root,relative,true);
}
} // namespace openemperor::persistence
