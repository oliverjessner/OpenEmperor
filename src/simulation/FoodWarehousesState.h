#pragma once

#include "simulation/FoodProcessingState.h"

namespace openemperor::simulation {
inline constexpr const char* food_warehouses_contract="authored-food-warehouses-v1";
inline constexpr std::uint32_t food_warehouses_revision=1;
inline constexpr int food_warehouse_capacity=32;
inline constexpr int food_warehouse_kind_intake=4;
// Immutable accepted-arrival provenance, never a second inventory.
struct FoodWarehouseDelivery {
    OriginalGoodsTransferKey transfer;
    BuildingId target{};
    std::uint64_t tick=0;
    std::int32_t amount=0;
    bool operator==(const FoodWarehouseDelivery&) const = default;
};
struct FoodWarehouseHistory {
    BuildingId warehouse_id{};
    std::array<std::optional<FoodProcessingArrival>,processed_food_count> first_received{};
    std::array<std::optional<FoodWarehouseDelivery>,processed_food_count> first_delivered{};
    bool operator==(const FoodWarehouseHistory&) const = default;
};
struct FoodWarehouseMarket {
    BuildingId market_id{};
    std::array<std::optional<FoodProcessingArrival>,processed_food_first_meal> first_received{};
    bool operator==(const FoodWarehouseMarket&) const = default;
};
struct FoodWarehousesState {
    std::vector<FoodWarehouseHistory> warehouses;
    std::vector<FoodWarehouseMarket> markets;
    bool operator==(const FoodWarehousesState&) const = default;
};
struct FoodWarehouseStatus {
    BuildingId building_id{};
    ProcessedFoodAmounts amounts{};
    int incoming=0,return_reserved=0,capacity=food_warehouse_capacity;
    std::optional<CourierId> courier;
    std::string blocker;
};
} // namespace openemperor::simulation
