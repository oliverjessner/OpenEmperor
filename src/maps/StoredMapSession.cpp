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
                                        FootprintPolicy policy, StoredGraphicsProfile profile) {
    const auto map_path=resolve_map_path(data_root,map_relative);
    const auto container=EmperorContainer::open(map_path);
    if (container.multipart()) throw std::runtime_error("map browser requires standalone map");
    auto map=read_emperor_map(container,0);
    const MapGeometry geometry{map.declared_map_size};
    if (!geometry.supported) throw std::runtime_error("unsupported map geometry");
    const auto candidates=read_map_graphic_candidates(container,0);
    const auto registrations=load_stored_archive_registrations(data_root,candidates,geometry,profile);
    auto plan=make_stored_graphics_plan(map,candidates,geometry,registrations,policy,profile);
    return {std::move(map),std::move(plan)};
}
} // namespace openemperor::maps
