#pragma once
#include "simulation/PhysicalFoodGrowersState.h"
#include "simulation/IrrigationProductivityState.h"
#include "simulation/original/ModelSchema.h"

namespace openemperor::simulation {
inline constexpr std::uint32_t seasonal_agriculture_revision=1;
inline constexpr const char* seasonal_agriculture_contract="authored-seasonal-agriculture-v1";
inline constexpr std::uint64_t seasonal_month_ticks=51*16;
inline constexpr std::uint64_t seasonal_year_ticks=seasonal_month_ticks*12;
inline constexpr std::uint8_t seasonal_harvest_work_ticks=5;

// Calendar is always derived from completed World ticks; these are observations.
bool seasonal_growing(original::ModelBuilding,std::uint64_t tick);
std::uint64_t seasonal_work_opportunities(original::ModelBuilding,std::uint64_t placed,std::uint64_t through);
std::optional<std::uint64_t> seasonal_next_opening(original::ModelBuilding,std::uint64_t tick);
std::optional<std::uint64_t> seasonal_ready_expiry(original::ModelBuilding,std::uint64_t ready_tick);
bool seasonal_harvest_allowed(original::ModelBuilding,std::uint64_t tick,std::uint64_t ready_tick);

struct SeasonalFoodField {
    BuildingId field_id{},farm_id{};
    std::uint64_t wasted_work=0,expired_ready=0,harvest_work=0,wasted_harvest_work=0;
    std::uint8_t harvest_progress=0;
    std::optional<std::uint64_t> ready_tick;
    bool operator==(const SeasonalFoodField&)const=default;
};
struct SeasonalPlant {
    BuildingId plant_id{},source_id{};
    std::uint64_t wasted_work=0,expired_ready=0;
    std::optional<std::uint64_t> ready_tick;
    bool operator==(const SeasonalPlant&)const=default;
};
struct SeasonalHempGrowerId {std::uint64_t value=0;auto operator<=>(const SeasonalHempGrowerId&)const=default;};
struct SeasonalHempTrip {
    SeasonalHempGrowerId grower_id{};BuildingId field_id{};
    std::uint64_t dispatched_tick=0,ready_tick=0;
    std::optional<std::uint64_t> picked_tick,returned_tick;
    std::uint8_t delivered=0;
    std::optional<IrrigationWorkWitness> irrigation_ready_work{};
    bool operator==(const SeasonalHempTrip&)const=default;
};
struct SeasonalHempReceipt {
    SeasonalHempGrowerId grower_id{};CourierId courier_id{};BuildingId field_id{};
    std::uint64_t dispatched_tick=0,ready_tick=0,picked_tick=0,returned_tick=0;
    std::optional<IrrigationWorkWitness> irrigation_ready_work{};
    bool operator==(const SeasonalHempReceipt&)const=default;
};
struct SeasonalHempFarm {
    BuildingId farm_id{};CourierId courier_id{};
    std::uint64_t productive_ticks=0,wasted_work=0,grown=0,picked=0,expired_ready=0,spawned=0,retired=0;
    std::optional<std::uint64_t> ready_tick;
    std::optional<SeasonalHempTrip> latest_trip;
    std::optional<SeasonalHempReceipt> last_receipt;
    bool operator==(const SeasonalHempFarm&)const=default;
};
// No second inventory or season clock. Existing work and source accounts stay sole.
struct SeasonalAgricultureState {
    std::uint64_t next_hemp_grower_id=1;
    std::vector<SeasonalFoodField> fields;
    std::vector<SeasonalPlant> plants;
    std::vector<SeasonalHempFarm> hemp_farms;
    bool operator==(const SeasonalAgricultureState&)const=default;
};
inline int seasonal_hemp_cargo(const SeasonalHempTrip& t){return t.picked_tick&&!t.returned_tick?1:0;}
inline int seasonal_hemp_reserved(const SeasonalAgricultureState* s,BuildingId id){
    if(s)for(const auto& f:s->hemp_farms)if(f.farm_id==id)return f.latest_trip&&!f.latest_trip->returned_tick?1:0;
    return 0;
}
}
