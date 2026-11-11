#pragma once
#include "assets/BuildingVisualProfile.h"
#include "scene/WorldDrawOrder.h"
#include "simulation/World.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>

namespace openemperor {
enum class SandboxVisualKind : std::uint8_t { Road, Building, Walker };
struct SandboxVisualKey {
    double depth=0; // Projected ground y, independent of image height.
    double ground_x=0;
    SandboxVisualKind kind=SandboxVisualKind::Road;
    unsigned stable_id=0;
};
inline scene::WorldVisualLayer world_layer(SandboxVisualKind kind) {
    switch (kind) {
    case SandboxVisualKind::Road: return scene::WorldVisualLayer::SandboxRoad;
    case SandboxVisualKind::Building: return scene::WorldVisualLayer::SandboxBuilding;
    case SandboxVisualKind::Walker: return scene::WorldVisualLayer::SandboxWalker;
    }
    return scene::WorldVisualLayer::SandboxRoad;
}
inline scene::WorldDrawKey world_key(const SandboxVisualKey& key) {
    return {key.depth,key.ground_x,world_layer(key.kind),key.stable_id};
}
inline bool operator<(const SandboxVisualKey& a,const SandboxVisualKey& b) {
    return world_key(a)<world_key(b);
}
struct FieldVisualNeighbor {
    simulation::Cell origin;
    simulation::BuildingFootprint footprint;
    scene::WorldDrawKey key;
    bool field=false;
};
struct FieldVisualOrder {
    scene::WorldDrawKey key;
    bool adjusted=false;
    bool ambiguous=false;
};
// An authored, bounded whole-sprite correction, not a general footprint
// compositor. The caller supplies at most four actual edge-neighbor owners,
// their complete footprints and their unchanged logical Building keys. Only
// the order key changes; the Field's projected ground and image anchor do not.
inline FieldVisualOrder authored_field_visual_order(simulation::Cell field,
        scene::WorldDrawKey original,std::span<const FieldVisualNeighbor> neighbors) {
    const auto ambiguous=[&] { return FieldVisualOrder{original,false,true}; };
    const auto valid_key=[](const scene::WorldDrawKey& key) {
        return std::isfinite(key.depth) && std::isfinite(key.ground_x) &&
            key.layer==scene::WorldVisualLayer::SandboxBuilding &&
            key.stable_id<=std::numeric_limits<std::uint32_t>::max();
    };
    if (neighbors.size()>4 || !valid_key(original)) return ambiguous();
    const scene::WorldDrawKey* lower=nullptr;
    const scene::WorldDrawKey* upper=nullptr;
    const auto fx=std::int64_t{field.x},fy=std::int64_t{field.y};
    const auto cell_end=std::int64_t{std::numeric_limits<int>::max()}+1;
    for (std::size_t i=0;i<neighbors.size();++i) {
        const auto& next=neighbors[i];
        if (next.field || !valid_key(next.key) ||
            next.footprint.width<=0 || next.footprint.height<=0) return ambiguous();
        const auto x=std::int64_t{next.origin.x},y=std::int64_t{next.origin.y};
        const auto right=x+next.footprint.width,bottom=y+next.footprint.height;
        if (right>cell_end || bottom>cell_end) return ambiguous();
        bool duplicate=false;
        for (std::size_t j=0;j<i;++j) {
            const auto& previous=neighbors[j];
            const bool same_key=!(previous.key<next.key) && !(next.key<previous.key);
            if (previous.key.stable_id==next.key.stable_id) {
                if (previous.origin!=next.origin || previous.footprint!=next.footprint || !same_key)
                    return ambiguous();
                duplicate=true;break;
            }
            const auto px=std::int64_t{previous.origin.x},py=std::int64_t{previous.origin.y};
            if (x<px+previous.footprint.width && px<right &&
                y<py+previous.footprint.height && py<bottom) return ambiguous();
        }
        if (duplicate) continue;
        const bool west=right==fx && y<=fy && fy<bottom;
        const bool north=bottom==fy && x<=fx && fx<right;
        const bool east=x==fx+1 && y<=fy && fy<bottom;
        const bool south=y==fy+1 && x<=fx && fx<right;
        if (!west && !north && !east && !south) return ambiguous();
        if (west || north) {
            if (!lower || *lower<next.key) lower=&next.key;
        } else if (!upper || next.key<*upper) upper=&next.key;
    }
    if (lower && upper && !(*lower<*upper)) return ambiguous();
    if ((!lower || *lower<original) && (!upper || original<*upper))
        return {original,false,false};
    if (!lower || (upper && !(original<*upper))) return ambiguous();
    // All actual Building IDs are uint32_t. This visual-only namespace follows
    // every body at the lower ground key, while its Building layer still
    // precedes Walkers there. No entity identity or World state is changed.
    const scene::WorldDrawKey promoted{lower->depth,lower->ground_x,
        scene::WorldVisualLayer::SandboxBuilding,(std::uint64_t{1}<<32)|original.stable_id};
    if (!(*lower<promoted) || !(original<promoted) || (upper && !(promoted<*upper)))
        return ambiguous();
    return {promoted,true,false};
}
// Presentation identity only; caller must require active Original production.
inline std::optional<assets::OriginalBuildingVisualRole> original_building_visual_role(simulation::Object object) {
    using O=simulation::Object;using R=assets::OriginalBuildingVisualRole;
    switch(object) {
    case O::OriginalWaterWell72:return R::WaterWell;
    case O::ServicePost:return R::Acupuncture;
    case O::OriginalMusicSchool211:return R::MusicSchool;
    case O::OriginalAcrobatSchool212:return R::AcrobatSchool;
    case O::OriginalDramaSchool213:return R::DramaSchool;
    case O::OriginalDaoistShrine215:return R::DaoistShrine;
    case O::OriginalBuddhistShrine217:return R::BuddhistShrine;
    case O::OriginalConfucianAcademy219:return R::ConfucianAcademy;
    case O::OriginalPaperMaker45:return R::PaperMaker;
    case O::OriginalWeaver47:return R::Weaver;
    case O::OriginalHempFarm:return R::HempFarm;
    case O::OriginalFoodFarm:return R::Farmhouse;
    case O::OriginalHempField:return R::HempField;
    case O::OriginalWheatField:return R::WheatField;
    case O::OriginalMilletField:return R::MilletField;
    default:return std::nullopt;
    }
}
inline std::optional<assets::BuildingVisualRole> building_visual_role(simulation::Object object) {
    using O=simulation::Object;
    using R=assets::BuildingVisualRole;
    switch (object) {
    case O::ClaySource: return R::ClaySource;
    case O::Pottery: return R::Pottery;
    case O::Warehouse: return R::Warehouse;
    case O::Household: return R::Household;
    case O::Farm: return R::Farm;
    case O::ServicePost: return R::ServicePost;
    case O::Market: return R::Market;
    case O::Well: return R::Well;
    case O::HealthPost: return R::HealthPost;
    case O::FireWatch: return R::FireWatch;
    default: return std::nullopt;
    }
}
}
