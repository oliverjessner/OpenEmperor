#pragma once

#include "simulation/AgricultureState.h"
#include <string>

namespace openemperor::simulation {
inline constexpr const char* food_processing_contract="authored-food-processing-v1";
inline constexpr std::uint32_t food_processing_revision=1;
inline constexpr std::size_t food_processing_mill_limit=2,food_processing_shop_limit=2;
inline constexpr std::uint32_t food_processing_work_per_ingredient=20;
// These are recipe/debit histories, never another ingredient or meal inventory.
struct FoodProcessingBatch {
    std::uint64_t sequence=0,started_tick=0;
    std::uint8_t grade=0;
    std::array<std::uint8_t,7> ingredients{};
    std::uint32_t work=0;
    bool operator==(const FoodProcessingBatch&) const = default;
};
struct FoodProcessingReceipt {
    FoodProcessingBatch batch;
    std::uint64_t completed_tick=0;
    bool operator==(const FoodProcessingReceipt&) const = default;
};
struct FoodProcessingArrival {
    OriginalGoodsTransferKey transfer;
    BuildingId source{};
    std::uint64_t tick=0;
    std::int32_t amount=0;
    bool operator==(const FoodProcessingArrival&) const = default;
};
struct MillProcessing {
    BuildingId mill_id{};
    std::uint8_t requested_grade=2;
    std::uint64_t started=0,completed=0,productive_ticks=0;
    std::array<std::array<std::uint64_t,7>,4> ingredients_consumed{};
    std::array<std::uint64_t,4> completed_by_grade{};
    std::array<std::optional<FoodProcessingArrival>,7> first_ingredients{};
    std::array<std::optional<FoodProcessingReceipt>,4> first_completions{};
    std::optional<FoodProcessingBatch> active;
    std::optional<FoodProcessingReceipt> last_receipt;
    bool operator==(const MillProcessing&) const = default;
};
struct FoodProcessingShop {
    BuildingId shop_id{},market_id{};
    std::array<std::optional<FoodProcessingArrival>,4> first_meals{};
    bool operator==(const FoodProcessingShop&) const = default;
};
struct MealConsumptionReceipt {
    std::uint64_t tick=0;
    std::uint8_t grade=0;
    bool operator==(const MealConsumptionReceipt&) const = default;
};
struct MealHouseHistory {
    BuildingId house_id{};
    ProcessedFoodQualityWitness first_receipt;
    std::array<std::optional<FoodProcessingArrival>,4> first_by_grade{};
    std::optional<MealConsumptionReceipt> first_consumption;
    bool operator==(const MealHouseHistory&) const = default;
};
struct FoodProcessingState {
    std::vector<MillProcessing> mills;
    std::vector<FoodProcessingShop> shops;
    std::vector<MealHouseHistory> houses;
    bool operator==(const FoodProcessingState&) const = default;
};
struct FoodProcessingStatus {
    BuildingId building_id{};
    std::optional<BuildingId> parent_market;
    std::uint8_t requested_grade=0,active_grade=0;
    std::uint32_t work=0,required_work=0;
    std::array<std::int32_t,7> ingredients{};
    std::array<std::int32_t,5> meals{};
    std::uint64_t started=0,completed=0,productive_ticks=0;
    int incoming=0,return_reserved=0,processing_reserved=0;
    std::string blocker;
};
} // namespace openemperor::simulation
