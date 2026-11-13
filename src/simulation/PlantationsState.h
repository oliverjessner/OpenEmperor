#pragma once

#include "simulation/OriginalIndustryState.h"

namespace openemperor::simulation {

// Agriculture Rule25 only. Original commodity identities retain the existing
// six-Goods/five-Aux ledgers; these rows never contain a second inventory.
enum class PlantationGrowerId : std::uint64_t {};
inline constexpr std::uint32_t plantations_revision=1;
inline constexpr const char* plantations_catalog="tea26-237-lacquer27-238-mulberry28-239-v1";
inline constexpr std::size_t plantation_plant_limit=32;
inline constexpr std::size_t plantation_source_limit=6;
// Retained, explicitly AUTHORED industry work period and unit yield. Original
// quantity increments, tending cadence and seasonal yields are not World ticks.
inline constexpr std::uint32_t plantation_growth_ticks=80;

struct PlantationReceipt {
    PlantationGrowerId grower_id{};
    CourierId courier_id{};
    std::uint64_t dispatched_tick=0,picked_tick=0,returned_tick=0;
    bool operator==(const PlantationReceipt&) const = default;
};
struct PlantationPlant {
    BuildingId plant_id{},source_id{};
    // 0..80 productive ticks. 80 means exactly one harvestable unit; the
    // plant remains after harvest and starts the next paid work cycle at zero.
    std::uint32_t growth=0;
    std::uint64_t productive_ticks=0,grown=0,harvested=0,returned=0;
    std::optional<PlantationReceipt> last_receipt;
    bool operator==(const PlantationPlant&) const = default;
};
struct PlantationTrip {
    PlantationGrowerId grower_id{};
    BuildingId plant_id{};
    std::uint64_t dispatched_tick=0;
    std::optional<std::uint64_t> picked_tick,returned_tick;
    // picked_tick and delivered are the sole harvest cargo authority: one
    // picked unit remains on the worker until its actual source return.
    std::uint8_t delivered=0;
    bool operator==(const PlantationTrip&) const = default;
};
struct PlantationSource {
    BuildingId building_id{};
    CourierId courier_id{};
    std::uint64_t spawned=0;
    std::optional<PlantationTrip> latest_trip;
    bool operator==(const PlantationSource&) const = default;
};
struct PlantationsState {
    std::uint64_t next_grower_id=1;
    std::vector<PlantationPlant> plants;
    std::vector<PlantationSource> sources;
    bool operator==(const PlantationsState&) const = default;
};

} // namespace openemperor::simulation
