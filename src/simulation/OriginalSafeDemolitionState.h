#pragma once

#include "simulation/OriginalIndustryState.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::simulation {

// Rule19-only closed purchase provenance. Retired rows own no physical cells,
// inventory, residents, workers or future transport. Existing lifetime entity
// budgets still apply in addition to this smaller journal bound.
inline constexpr std::size_t original_safe_demolition_retired_limit=8;
enum class OriginalRetiredKind : std::uint8_t { EmptyHouse=0, ClaySource=1 };
struct OriginalRetiredPurchase {
    BuildingId building_id=static_cast<BuildingId>(0);
    OriginalRetiredKind kind=OriginalRetiredKind::EmptyHouse;
    int origin_x=0,origin_y=0;
    std::uint64_t placed_tick=0,removed_tick=0,clay_extracted=0;
    std::optional<CourierId> courier_id;
    // The exact latest completed native Clay trip retains its sequence and
    // real arrival. It cannot be replayed as new cargo or a second delivery.
    std::optional<OriginalClayTrip> clay_trip;
    bool operator==(const OriginalRetiredPurchase&) const = default;
};
struct OriginalSafeDemolitionState {
    std::vector<OriginalRetiredPurchase> retired;
    bool operator==(const OriginalSafeDemolitionState&) const = default;
};

} // namespace openemperor::simulation
