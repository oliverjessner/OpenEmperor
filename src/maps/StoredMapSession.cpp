#include "maps/StoredMapSession.h"

#include "assets/AssetCatalog.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/EmperorContainer.h"
#include "maps/MapCatalog.h"
#include "maps/MapGraphicCandidates.h"

#include <stdexcept>
#include <utility>

namespace openemperor::maps {
StoredMapSession load_stored_map_session(const std::filesystem::path& data_root,
                                        const std::filesystem::path& map_relative,
                                        FootprintPolicy policy) {
    const auto map_path=resolve_map_path(data_root,map_relative);
    const auto container=EmperorContainer::open(map_path);
    if (container.multipart()) throw std::runtime_error("map browser requires standalone map");
    auto map=read_emperor_map(container,0);
    const MapGeometry geometry{map.declared_map_size};
    if (!geometry.supported) throw std::runtime_error("unsupported map geometry");
    constexpr auto terrain_relative="DATA/China_Terrain.sg3";
    constexpr auto elevation_relative="DATA/China_Elevation.sg3";
    const auto terrain_path=validate_stored_archive_sources(data_root,terrain_relative);
    const auto elevation_path=validate_stored_archive_sources(data_root,elevation_relative);
    const auto terrain_layout=build_runtime_archive_layout(3,assets::read_sg3_archive(terrain_path));
    const auto elevation_layout=build_runtime_archive_layout(16,assets::read_sg3_archive(elevation_path));
    if (!terrain_layout || !elevation_layout)
        throw std::runtime_error("unsupported v213 Terrain/Elevation runtime layout");
    const auto terrain_catalog=assets::scan_asset_archive(data_root,terrain_relative);
    const auto elevation_catalog=assets::scan_asset_archive(data_root,elevation_relative);
    const auto candidates=read_map_graphic_candidates(container,0);
    auto plan=make_stored_graphics_plan(map,candidates,geometry,terrain_catalog,*terrain_layout,
        elevation_catalog,*elevation_layout,policy);
    return {std::move(map),std::move(plan)};
}
} // namespace openemperor::maps
