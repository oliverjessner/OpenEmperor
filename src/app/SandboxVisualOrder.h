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
inline std::optional<assets::BuildingVisualRole> building_visual_role(simulation::Object object) {
    using O=simulation::Object;
    using R=assets::BuildingVisualRole;
    switch (object) {
    case O::ClaySource: return R::ClaySource;
    case O::Pottery: return R::Pottery;
    case O::Warehouse: return R::Warehouse;
    case O::Household: return R::Household;
    default: return std::nullopt;
    }
}
}
