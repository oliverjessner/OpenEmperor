#include "persistence/SandboxSave.h"
#include "maps/MapCatalog.h"
#include "maps/MapRulesCheck.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"
#include "maps/OriginalMapEntities.h"
#include "maps/SandboxPlacement.h"
#include "maps/LandscapeProvenance.h"

#include <algorithm>

#include <stdexcept>

namespace openemperor::persistence {
namespace {
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_agriculture_terrain_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative,bool irrigation,bool fishing=false) {
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
    std::vector<simulation::Cell> fishing_points;
    if(fishing) {
        const auto entities=maps::read_original_map_entities(container,0);
        const auto heights=container.read_range(0,maps::landscape_height_offset,simulation::OriginalTerrainFacts::cell_count);
        // Reuse the complete existing source identity, occupancy and geometry
        // predicates. This does not create a second marker admission list.
        maps::validate_sandbox_original_map_rules(map,geometry,entities,heights,
            simulation::kBridgesMapPermissionsPolicyVersion);
        for(const auto& entity:entities.records) if(entity.active() && entity.type==179 &&
            entity.entity_class==maps::OriginalEntityClass::Industrial && entity.footprint_side==0) {
            const simulation::Cell cell{static_cast<int>(entity.local_x)+static_cast<int>(geometry.border),
                static_cast<int>(entity.local_y)+static_cast<int>(geometry.border)};
            if(std::find(fishing_points.begin(),fishing_points.end(),cell)==fishing_points.end())
                fishing_points.push_back(cell);
        }
        std::sort(fishing_points.begin(),fishing_points.end(),[](auto a,auto b){return a.y!=b.y ? a.y<b.y:a.x<b.x;});
        if(maps::map_input_sha256(maps::EmperorContainer::open(path))!=source_hash)
            throw std::runtime_error("Original map changed while preparing Fishing marker geography");
    }
    return std::make_shared<const simulation::OriginalTerrainFacts>(
        map.terrain_raw.values,map.fertility_bytes,source_hash,boundary,fishing_points);
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
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_playable_fishing_terrain_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative) {
    return prepare_agriculture_terrain_facts(root,relative,true,true);
}
} // namespace openemperor::persistence
