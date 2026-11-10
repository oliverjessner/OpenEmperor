#pragma once
#include "assets/BuildingVisualProfile.h"
#include "scene/WorldDrawOrder.h"
#include "simulation/World.h"
#include <cstdint>
#include <optional>
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
