#pragma once

#include "simulation/OriginalFoodState.h"
#include "simulation/FoodBatchArithmetic.h"

namespace openemperor::simulation {

// Rule24 catalog indices are deliberately distinct from original resource IDs.
enum class AgricultureCrop : std::uint8_t { Wheat=0, Millet=1, Rice=2, Cabbage=3, Soybean=4 };
inline constexpr std::size_t agriculture_crop_count=5;
inline constexpr std::size_t agriculture_field_limit=32;
inline constexpr std::uint32_t agriculture_revision=28;
// Complete Original Grower tending/harvest timing is not reconstructed. This
// generation admits paid source-verified construction, never a Food origin.
inline constexpr bool agriculture_production_clock_verified=false;
inline constexpr const char* agriculture_catalog="wheat7-millet5-rice6-cabbage3-beancurd1-v1";
using AgricultureAmounts=std::array<std::int32_t,agriculture_crop_count>;
using AgricultureCounters=std::array<std::uint64_t,agriculture_crop_count>;
struct AgricultureFarm {
    BuildingId farm_id{};
    bool operator==(const AgricultureFarm&) const = default;
};
struct AgricultureField {
    BuildingId field_id{},farm_id{};
    AgricultureCrop crop= AgricultureCrop::Wheat;
    std::uint32_t quantity=0;
    std::uint64_t productive_events=0,harvested=0,last_harvest_cycle=0;
    bool operator==(const AgricultureField&) const = default;
};
struct AgricultureQualityWitness {
    OriginalGoodsTransferKey transfer;
    std::uint64_t tick=0;
    AgricultureAmounts accepted{};
    AgricultureCounters consumed_before{};
    std::uint8_t previous_quality=0;
    bool operator==(const AgricultureQualityWitness&) const = default;
};
struct AgricultureStock {
    BuildingId building_id{};
    AgricultureAmounts amounts{};
    AgricultureCounters received{},departed{},returned{},consumed{};
    std::uint64_t missed_demands=0;
    std::optional<AgricultureQualityWitness> quality_witness;
    bool operator==(const AgricultureStock&) const = default;
};
struct AgricultureTrip {
    OriginalGoodsTransferKey transfer;
    BuildingId source{},target{};
    OriginalGoodsTransferPhase phase=OriginalGoodsTransferPhase::Loaded;
    AgricultureAmounts loaded{},delivered{},returned{};
    bool operator==(const AgricultureTrip&) const = default;
};
// Separate full-width historical inputs, never a lossy legacy Food projection.
struct AgricultureHousingEvaluation {
    BuildingId building_id{};
    std::uint64_t tick=0;
    std::optional<AgricultureQualityWitness> quality_witness;
    AgricultureCounters received{},consumed{};
    bool operator==(const AgricultureHousingEvaluation&) const = default;
};
struct AgricultureState {
    std::vector<AgricultureFarm> farms;
    std::vector<AgricultureField> fields;
    std::vector<AgricultureStock> stocks;
    std::vector<AgricultureTrip> trips;
    std::vector<AgricultureHousingEvaluation> housing_evaluations;
    bool operator==(const AgricultureState&) const = default;
};
// AUTHORED nutrition scores and quantity-weighted blend, not original scores.
inline FoodBatchQuality agriculture_batch_quality(const AgricultureAmounts& amounts) {
    return {food_batch::weighted_quality(amounts,std::array<std::uint8_t,5>{20,80,60,70,90})};
}
} // namespace openemperor::simulation
