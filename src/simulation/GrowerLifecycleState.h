#pragma once

#include "simulation/GrowerNavigationState.h"
#include "simulation/GrowerSourceFacts.h"
#include "simulation/original/CropGrowerRules.h"
#include "simulation/original/CropHarvestRules.h"

#include <memory>

namespace openemperor::simulation {

// Explicit transient tending-only contract. No harvest, Food or saved actor
// authority. Actor callback frequency remains an AUTHORED World binding.
enum class GrowerLifecyclePolicy : std::uint8_t { AutonomousTendingV1=1 };
enum class GrowerLifecycleStatus : std::uint8_t {
    WaitingForStaff,WaitingForAccess,WaitingForSeason,Running,Unsupported,Draining,Stopped
};
struct FoodGrowerId {
    std::uint64_t value=0;
    auto operator<=>(const FoodGrowerId&) const = default;
};
struct GrowerLifecycleRequest {
    GrowerLifecyclePolicy policy=GrowerLifecyclePolicy::AutonomousTendingV1;
    BuildingId farm_id{};
    std::shared_ptr<const GrowerSourceFacts> source;
    GrowerNavigationInteraction interaction=GrowerNavigationInteraction::None;
};
struct GrowerLifecycleField {
    BuildingId field_id{};
    original::CropHarvestFieldState state;
    std::uint64_t tending_callbacks=0,last_tending_tick=0;
    bool operator==(const GrowerLifecycleField&) const = default;
};
struct FoodGrowerActor {
    static constexpr std::uint16_t original_type=26;
    FoodGrowerId id{};
    BuildingId farm_id{};
    std::optional<BuildingId> last_field_id;
    original::CropGrowerActor native;
    std::optional<GrowerNavigationId> navigation_id;
    Cell current{};
    bool retired=false,cancel_requested=false;
    std::uint64_t spawned_tick=0,arrived_tick=0,work_started_tick=0;
    std::uint64_t tending_callbacks=0,retired_tick=0;
    bool operator==(const FoodGrowerActor&) const = default;
};
struct GrowerLifecycleState {
    GrowerLifecyclePolicy policy=GrowerLifecyclePolicy::AutonomousTendingV1;
    BuildingId farm_id{};
    GrowerLifecycleStatus status=GrowerLifecycleStatus::WaitingForStaff;
    std::string blocker;
    bool stop_requested=false;
    std::uint8_t dispatch_counter=0;
    std::uint8_t admitted_slot=0;
    original::CropGrowerCounters retained_counters;
    std::optional<Cell> source_cell;
    std::uint64_t source_callbacks=0,due_attempts=0,allocation_attempts=0;
    std::uint64_t spawned=0,retired=0,tending_callbacks=0,allocation_refusals=0;
    // Live occupancy is independent of the native retained counters. Retired
    // records are a bounded audit tail, never live actors or reusable IDs.
    std::vector<GrowerLifecycleField> fields;
    std::vector<FoodGrowerActor> actors;
    bool operator==(const GrowerLifecycleState&) const = default;
};
struct GrowerLifecycleAdmission {
    bool accepted=false;
    std::string reason;
};

} // namespace openemperor::simulation
