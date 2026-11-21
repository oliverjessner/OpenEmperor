#pragma once
#include "simulation/AgricultureState.h"
#include "simulation/MapPermissions.h"
#include <cstdint>
#include <optional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace openemperor::simulation {
inline constexpr std::uint32_t irrigation_productivity_revision=1;
inline constexpr const char* irrigation_productivity_contract="authored-irrigation-productivity-v1";

// Immutable readiness observation retained by physical material trips/receipts.
// Passive regrowth after pickup must never rewrite an older crop's earned work.
struct IrrigationWorkWitness {
    std::uint64_t base_work=0,wet_work_ticks=0,bonus_work=0,clipped_bonus=0,wasted_work=0;
    bool operator==(const IrrigationWorkWitness&)const=default;
};

// Existing productive_ticks remain actual base-work ticks. This records only
// earned wet work and its bounded rational bonus, never stock or a water mask.
struct IrrigationProductivityWork {
    BuildingId building_id{};
    std::uint64_t wet_work_ticks=0,bonus_work=0,clipped_bonus=0;
    bool operator==(const IrrigationProductivityWork&)const=default;
};
struct IrrigationProductivityState {
    std::vector<IrrigationProductivityWork> work;
    bool operator==(const IrrigationProductivityState&)const=default;
};
inline std::uint64_t irrigation_bonus(const IrrigationProductivityState* state,BuildingId id) {
    if(!state)return 0;
    const auto at=std::lower_bound(state->work.begin(),state->work.end(),id,
        [](const auto& row,BuildingId key){return row.building_id<key;});
    if(at==state->work.end()||at->building_id!=id)
        throw std::invalid_argument("Irrigation productivity lost its paid crop identity");
    return at->bonus_work;
}
inline std::uint64_t irrigation_earned_work(std::uint64_t base,const IrrigationProductivityState* state,BuildingId id) {
    const auto bonus=irrigation_bonus(state,id);
    if(bonus>UINT64_MAX-base)throw std::invalid_argument("Irrigation earned work overflow");
    return base+bonus;
}
struct IrrigationProductivityStatus {
    bool supported=false,water_available=false,work_eligible=false,bonus_active=false;
    BuildingId building_id{};
    std::optional<Cell> wet_ditch;
    std::optional<BuildingId> supplying_pump;
    std::uint64_t base_work_ticks=0,wet_work_ticks=0,bonus_work=0,clipped_bonus=0;
    std::uint32_t progress=0;
    std::uint8_t carry=0,next_increment=0;
    std::optional<std::uint64_t> next_opening_tick;
    std::string reason;
};
}
