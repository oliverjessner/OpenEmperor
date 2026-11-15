#pragma once

#include "simulation/MapPermissions.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::simulation {

enum class BuildingId : std::uint32_t;

inline constexpr std::uint32_t irrigation_revision=1;
inline constexpr const char* irrigation_contract="authored-agriculture-irrigation-v1";
inline constexpr std::size_t irrigation_ditch_limit=228U*228U;
inline constexpr std::size_t irrigation_gesture_limit=256;

// Ditches are paid terrain overlays, not industrial bodies or courier owners.
// Their cell and successful paid-command sequence form the retained identity.
struct IrrigationDitch {
    Cell cell{};
    std::uint64_t placed_tick=0,placed_sequence=0;
    bool operator==(const IrrigationDitch&) const = default;
};
struct IrrigationPump {
    BuildingId building_id{};
    std::uint8_t orientation=0;
    // World-derived supply epoch. Revalidation stops supply immediately;
    // renewed eligibility waits for the next source phase27 opportunity.
    std::optional<std::uint64_t> eligible_since_tick;
    bool operator==(const IrrigationPump&) const = default;
};
struct IrrigationState {
    std::vector<IrrigationDitch> ditches; // Canonical (y,x) order.
    std::vector<IrrigationPump> pumps; // Canonical paid BuildingId order.
    std::uint64_t ditches_placed_total=0,ditches_removed_total=0;
    std::uint64_t ditch_construction_spent=0;
    bool operator==(const IrrigationState&) const = default;
};

// Derived command/inspection results. No wet mask or status is saved authority.
struct IrrigationBatchValidation {
    bool accepted=false;
    std::string reason;
    std::size_t new_ditch_count=0;
    std::int64_t total_cost=0;
    BuildDiagnostic diagnostic{};
};
struct IrrigationFieldStatus {
    bool supported=false,water_available=false,yield_effect_active=false;
    std::optional<Cell> wet_ditch;
    std::optional<BuildingId> supplying_pump;
    std::string reason;
};
struct IrrigationPumpStatus {
    bool supported=false,placed=false,intake_valid=false,operating=false;
    bool burning=false,staffed=false,supplying=false;
    int assigned_workers=0;
    std::uint8_t orientation=0;
    std::optional<std::uint64_t> eligible_since_tick,last_phase_tick;
    std::array<Cell,4> footprint{},seeds{};
    std::array<Cell,8> intake_water{};
    std::string reason;
};

} // namespace openemperor::simulation
