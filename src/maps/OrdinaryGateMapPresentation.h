#pragma once
#include "maps/OriginalMapEntities.h"
#include "maps/LandscapeInstances.h"
#include "maps/StoredGraphicsPlan.h"

namespace openemperor::maps {
inline constexpr std::uint32_t ordinary_gate_resource_key=0x4afU;
struct OrdinaryGatePresentation {
    OriginalEntityRecord source;
    GridCell origin{};
    unsigned width=0,height=0;
    std::vector<LandscapeInstanceSpec> components;
    std::string fallback;
};
struct OrdinaryGateMapPresentation {
    std::size_t manager_records=0;
    std::vector<OrdinaryGatePresentation> gates;
    std::vector<std::optional<std::size_t>> gate_by_storage;
    std::string error;
};
// Saved GateHouse restore, boolean zero and camera view zero only. This uses
// the proven class/layout and complete raw footprint, never saved image IDs.
OrdinaryGateMapPresentation prepare_ordinary_gate_presentation(
    const OriginalMapEntities& entities,std::span<const std::uint32_t> terrain);
void read_ordinary_gate_presentation(StoredGraphicsPlan& plan,
    const EmperorContainer& container,std::size_t part);
} // namespace openemperor::maps
