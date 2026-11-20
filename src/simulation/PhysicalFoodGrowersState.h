#pragma once
#include "simulation/AgricultureState.h"
namespace openemperor::simulation {
inline constexpr std::uint32_t physical_food_growers_revision=1;
inline constexpr const char* physical_food_growers_contract="authored-physical-food-growers-v1";
inline constexpr std::uint8_t physical_food_grower_wait_ticks=5;
struct PhysicalFoodGrowerId { std::uint64_t value=0; auto operator<=>(const PhysicalFoodGrowerId&) const=default; };
enum class PhysicalFoodGrowerPhase : std::uint8_t { Outward=1,Waiting=2,Working=3,Returning=4 };
struct PhysicalFoodGrowerReceipt {
    PhysicalFoodGrowerId grower_id{};CourierId courier_id{};AgricultureCrop crop{};
    std::uint64_t dispatched_tick=0,arrived_tick=0,harvested_tick=0,returned_tick=0,work_at_harvest=0;
    bool operator==(const PhysicalFoodGrowerReceipt&) const=default;
};
struct PhysicalFoodGrowerTrip {
    PhysicalFoodGrowerId grower_id{};BuildingId field_id{};AgricultureCrop crop{};
    PhysicalFoodGrowerPhase phase=PhysicalFoodGrowerPhase::Outward;
    std::uint64_t dispatched_tick=0;
    std::optional<std::uint64_t> arrived_tick,harvested_tick,returned_tick;
    std::uint64_t work_before=0;std::uint32_t work_ticks=0;
    std::uint8_t wait_left=0,delivered=0;bool cancel_requested=false;
    bool operator==(const PhysicalFoodGrowerTrip&) const=default;
};
struct PhysicalFoodGrowerFarm {
    BuildingId farm_id{};CourierId courier_id{};std::uint64_t spawned=0,retired=0;
    std::optional<PhysicalFoodGrowerTrip> latest_trip;
    bool operator==(const PhysicalFoodGrowerFarm&) const=default;
};
struct PhysicalFoodGrowerField {
    BuildingId field_id{},farm_id{};std::uint64_t returned=0;
    std::optional<PhysicalFoodGrowerReceipt> last_receipt;
    bool operator==(const PhysicalFoodGrowerField&) const=default;
};
// No inventory: only actual Field harvests, physical cargo and Farm receipts.
struct PhysicalFoodGrowersState {
    std::uint64_t next_grower_id=1;
    std::vector<PhysicalFoodGrowerFarm> farms;
    std::vector<PhysicalFoodGrowerField> fields;
    bool operator==(const PhysicalFoodGrowersState&) const=default;
};
inline int physical_food_grower_reservations(const PhysicalFoodGrowersState* state,BuildingId farm) {
    if(state)for(const auto& row:state->farms)if(row.farm_id==farm)
        return row.latest_trip && !row.latest_trip->returned_tick ? 1:0;
    return 0;
}
inline int physical_food_trip_cargo(const PhysicalFoodGrowerTrip& trip) {
    return trip.harvested_tick && !trip.returned_tick ? 1:0;
}
}
