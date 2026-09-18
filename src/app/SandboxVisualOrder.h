#pragma once
#include "assets/BuildingVisualProfile.h"
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
inline bool operator<(const SandboxVisualKey& a,const SandboxVisualKey& b) {
    return std::tie(a.depth,a.ground_x,a.kind,a.stable_id)<
           std::tie(b.depth,b.ground_x,b.kind,b.stable_id);
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
