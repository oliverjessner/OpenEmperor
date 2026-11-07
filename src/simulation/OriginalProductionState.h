#pragma once

#include "simulation/original/GoodsRules.h"
#include "simulation/OriginalFoodState.h"
#include "simulation/OriginalServicesState.h"
#include "simulation/OriginalHousingState.h"
#include "simulation/OriginalIndustryState.h"
#include "simulation/OriginalFinanceState.h"
#include "simulation/OriginalFireProtectionState.h"
#include "simulation/OriginalSafeDemolitionState.h"
#include "simulation/OriginalCultureState.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::simulation {

// Explicit authored working-generation revision. A later stage must opt into
// its own revision; a saved earlier development state is never reinterpreted.
enum class BuildingId : std::uint32_t;
inline constexpr std::uint32_t kOriginalProductionDevelopmentRevision=27;
struct OriginalHempFarmState {
    BuildingId farm_id=static_cast<BuildingId>(0);
    std::optional<BuildingId> field_id;
    std::uint64_t harvested=0;
    bool operator==(const OriginalHempFarmState&) const = default;
};
// Cumulative historical sinks, never a second household stock or clock.
struct OriginalHouseConsumptionState {
    BuildingId building_id=static_cast<BuildingId>(0);
    std::array<std::uint64_t,original::goods_count> consumed{};
    bool operator==(const OriginalHouseConsumptionState&) const = default;
};
struct OriginalProductionState {
    std::uint32_t development_revision=kOriginalProductionDevelopmentRevision;
    std::uint64_t residents_admitted=0;
    std::uint64_t pottery_handed_off=0;
    std::vector<OriginalHempFarmState> hemp_farms;
    std::vector<OriginalHouseConsumptionState> house_consumption;
    OriginalFoodState food;
    OriginalServicesState services;
    OriginalWaterState water;
    OriginalHousingState housing;
    OriginalIndustryState industry;
    OriginalFinanceState finance;
    std::optional<OriginalFireProtectionState> fire_protection;
    // Rule19 only: closed paid purchases, never stock or active entities.
    std::optional<OriginalSafeDemolitionState> safe_demolition;
    // Rule22 only; old school-to-House services retain their exact authority.
    std::optional<OriginalCultureState> culture;
    bool operator==(const OriginalProductionState&) const = default;
};

} // namespace openemperor::simulation
