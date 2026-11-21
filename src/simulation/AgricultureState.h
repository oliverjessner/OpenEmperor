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
// New-generation catalog indices, distinct from the Original resource IDs.
enum class FishingFoodResource : std::uint8_t { Wheat=0, Millet=1, Rice=2, Cabbage=3, BeanCurd=4, Fish=5 };
inline constexpr std::size_t fishing_food_count=6;
inline constexpr std::size_t fishing_food_index=static_cast<std::size_t>(FishingFoodResource::Fish);
inline constexpr std::uint32_t fishing_food_revision=1;
inline constexpr const char* fishing_food_catalog="wheat7-millet5-rice6-cabbage3-beancurd1-fish2-v1";
using FishingFoodAmounts=std::array<std::int32_t,fishing_food_count>;
using FishingFoodCounters=std::array<std::uint64_t,fishing_food_count>;
// Rule29 owns a distinct seven-kind ledger; no old DTO changes width.
enum class HuntingFoodResource : std::uint8_t { Wheat=0, Millet=1, Rice=2, Cabbage=3, BeanCurd=4, Fish=5, Meat=6 };
inline constexpr std::size_t hunting_food_count=7;
inline constexpr std::size_t hunting_food_index=static_cast<std::size_t>(HuntingFoodResource::Meat);
inline constexpr std::uint32_t hunting_food_revision=1;
inline constexpr const char* hunting_food_catalog="wheat7-millet5-rice6-cabbage3-beancurd1-fish2-meat4-v1";
using HuntingFoodAmounts=std::array<std::int32_t,hunting_food_count>;
using HuntingFoodCounters=std::array<std::uint64_t,hunting_food_count>;
// Rule34's sole authority keeps the seven source identities and five graded
// lots of the same Dinners28 commodity. There is no mirrored meal total.
enum class ProcessedFoodResource : std::uint8_t {
    Wheat=0,Millet=1,Rice=2,Cabbage=3,BeanCurd=4,Fish=5,Meat=6,
    DinnersGrade1=7,DinnersGrade2=8,DinnersGrade3=9,DinnersGrade4=10,DinnersGrade5=11
};
inline constexpr std::size_t processed_food_count=12;
inline constexpr std::size_t processed_food_first_meal=7;
inline constexpr std::uint32_t processed_food_revision=1;
inline constexpr const char* processed_food_catalog="wheat7-millet5-rice6-cabbage3-beancurd1-fish2-meat4-dinners28-grades1-5-v1";
using ProcessedFoodAmounts=std::array<std::int32_t,processed_food_count>;
using ProcessedFoodCounters=std::array<std::uint64_t,processed_food_count>;
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
template<std::size_t Count> struct BasicAgricultureQualityWitness {
    OriginalGoodsTransferKey transfer;
    std::uint64_t tick=0;
    std::array<std::int32_t,Count> accepted{};
    std::array<std::uint64_t,Count> consumed_before{};
    std::uint8_t previous_quality=0;
    bool operator==(const BasicAgricultureQualityWitness&) const = default;
};
template<std::size_t Count> struct BasicAgricultureStock {
    BuildingId building_id{};
    std::array<std::int32_t,Count> amounts{};
    std::array<std::uint64_t,Count> received{},departed{},returned{},consumed{};
    std::uint64_t missed_demands=0;
    std::optional<BasicAgricultureQualityWitness<Count>> quality_witness;
    bool operator==(const BasicAgricultureStock&) const = default;
};
template<std::size_t Count> struct BasicAgricultureTrip {
    OriginalGoodsTransferKey transfer;
    BuildingId source{},target{};
    OriginalGoodsTransferPhase phase=OriginalGoodsTransferPhase::Loaded;
    std::array<std::int32_t,Count> loaded{},delivered{},returned{};
    bool operator==(const BasicAgricultureTrip&) const = default;
};
// Separate full-width historical inputs, never a lossy legacy Food projection.
template<std::size_t Count> struct BasicAgricultureHousingEvaluation {
    BuildingId building_id{};
    std::uint64_t tick=0;
    std::optional<BasicAgricultureQualityWitness<Count>> quality_witness;
    std::array<std::uint64_t,Count> received{},consumed{};
    bool operator==(const BasicAgricultureHousingEvaluation&) const = default;
};
template<std::size_t Count> struct BasicAgricultureState {
    static constexpr std::size_t resource_count=Count;
    std::vector<AgricultureFarm> farms;
    std::vector<AgricultureField> fields;
    std::vector<BasicAgricultureStock<Count>> stocks;
    std::vector<BasicAgricultureTrip<Count>> trips;
    std::vector<BasicAgricultureHousingEvaluation<Count>> housing_evaluations;
    bool operator==(const BasicAgricultureState&) const = default;
};
// Historical aliases remain exactly five-wide; only the explicit successor
// owns six-wide arrays. No projection or shared mutable inventory exists.
using AgricultureQualityWitness=BasicAgricultureQualityWitness<agriculture_crop_count>;
using AgricultureStock=BasicAgricultureStock<agriculture_crop_count>;
using AgricultureTrip=BasicAgricultureTrip<agriculture_crop_count>;
using AgricultureHousingEvaluation=BasicAgricultureHousingEvaluation<agriculture_crop_count>;
using AgricultureState=BasicAgricultureState<agriculture_crop_count>;
using FishingFoodQualityWitness=BasicAgricultureQualityWitness<fishing_food_count>;
using FishingFoodStock=BasicAgricultureStock<fishing_food_count>;
using FishingFoodTrip=BasicAgricultureTrip<fishing_food_count>;
using FishingFoodHousingEvaluation=BasicAgricultureHousingEvaluation<fishing_food_count>;
using FishingFoodState=BasicAgricultureState<fishing_food_count>;
using HuntingFoodQualityWitness=BasicAgricultureQualityWitness<hunting_food_count>;
using HuntingFoodStock=BasicAgricultureStock<hunting_food_count>;
using HuntingFoodTrip=BasicAgricultureTrip<hunting_food_count>;
using HuntingFoodHousingEvaluation=BasicAgricultureHousingEvaluation<hunting_food_count>;
using HuntingFoodState=BasicAgricultureState<hunting_food_count>;
using ProcessedFoodQualityWitness=BasicAgricultureQualityWitness<processed_food_count>;
using ProcessedFoodStock=BasicAgricultureStock<processed_food_count>;
using ProcessedFoodTrip=BasicAgricultureTrip<processed_food_count>;
using ProcessedFoodHousingEvaluation=BasicAgricultureHousingEvaluation<processed_food_count>;
using ProcessedFoodState=BasicAgricultureState<processed_food_count>;
// AUTHORED nutrition scores and quantity-weighted blend, not original scores.
inline FoodBatchQuality agriculture_batch_quality(const AgricultureAmounts& amounts) {
    return {food_batch::weighted_quality(amounts,std::array<std::uint8_t,5>{20,80,60,70,90})};
}
inline FoodBatchQuality agriculture_batch_quality(const FishingFoodAmounts& amounts) {
    return {food_batch::weighted_quality(amounts,std::array<std::uint8_t,6>{20,80,60,70,90,75})};
}
inline FoodBatchQuality agriculture_batch_quality(const HuntingFoodAmounts& amounts) {
    return {food_batch::weighted_quality(amounts,std::array<std::uint8_t,7>{20,80,60,70,90,75,85})};
}
inline FoodBatchQuality agriculture_batch_quality(const ProcessedFoodAmounts& amounts) {
    return {food_batch::weighted_quality(amounts,std::array<std::uint8_t,12>{20,80,60,70,90,75,85,20,40,60,80,100})};
}
} // namespace openemperor::simulation
