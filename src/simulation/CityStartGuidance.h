#pragma once

#include "simulation/World.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace openemperor::simulation {

enum class StarterSupplyCondition : std::uint8_t {
    MissingBuilding,
    Paused,
    Unstaffed,
    NoReachableTarget,
    AwaitingGoods,
    DeliveringOrReady
};

struct StarterFacilityStatus {
    Object kind=Object::Empty;
    StarterSupplyCondition condition=StarterSupplyCondition::MissingBuilding;
};

struct HouseholdStartStatus {
    BuildingId id=BuildingId::Household;
    bool pottery_available=false;
    bool food_available=false;
    bool service_available=false;
    std::uint64_t demand_ticks_remaining=0;
    std::uint64_t move_in_grace_remaining=0;
    int last_demand_status=0;
    std::uint64_t taxes_contributed=0;
};

struct CityStartGuidance {
    bool applicable=false;
    std::vector<Object> missing_supply_buildings;
    std::vector<StarterFacilityStatus> facilities;
    std::vector<HouseholdStartStatus> households;
    std::int64_t minimum_missing_building_funds=0;
    int workforce_supply=0;
    int workforce_required_now=0;
    int workforce_active_demand=0;
    int workforce_installed_demand=0;
    int workforce_used=0;
    int workforce_required_for_starter=0;
    int starter_workforce_shortfall=0;
    std::size_t household_count=0;
    std::size_t household_slots_remaining=0;
    std::size_t houses_needed_for_shortfall=0;
    std::size_t suggested_additional_houses=0;
    std::size_t affordable_suggested_houses=0;
    std::int64_t suggested_house_cost=0;
    std::int64_t suggested_house_funds_missing=0;
    std::int64_t minimum_house_funds_for_starter=0;
    bool starter_workforce_within_house_limit=true;
    bool complete_supply_chain=false;
    bool taxes_have_been_collected=false;
};

struct StarterBudgetWarning {
    std::int64_t purchase_cost=0;
    std::int64_t funds_after_purchase=0;
    std::int64_t minimum_remaining_building_funds=0;
    std::int64_t minimum_remaining_house_funds=0;
    std::int64_t minimum_remaining_start_cost=0;
    std::size_t additional_houses_needed=0;
    std::size_t household_slots_remaining=0;
    bool starter_workforce_within_house_limit=true;
    std::vector<Object> remaining_missing_supply_buildings;
};

CityStartGuidance inspect_city_start(const World& world);

// This is a conservative, read-only affordability check. The caller must first
// validate the command and must validate it again before an approved execution.
std::optional<StarterBudgetWarning> starter_budget_warning(const World& world,
                                                           Command command);
std::optional<StarterBudgetWarning> starter_budget_warning(const World& world,
                                                           std::span<const Command> commands);

const char* starter_building_name(Object kind);

} // namespace openemperor::simulation
