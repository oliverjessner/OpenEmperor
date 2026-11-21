#pragma once

#include "simulation/original/OriginalScheduler.h"
#include "simulation/original/ModelSchema.h"

#include <cstdint>
#include <optional>
#include <string>

namespace openemperor::simulation {

enum class BuildingId : std::uint32_t;

// Disposable observations of the existing World clock and paid production.
// No additional epoch, saved authority or Original month names are introduced.
struct AgricultureCalendarStatus {
    std::uint64_t completed_ticks=0;
    original::SchedulerPosition position;
    std::uint32_t mapping_version=original::scheduler_mapping_version;
    std::optional<std::uint64_t> next_source_growth_tick;
    std::optional<std::uint64_t> next_irrigation_update_tick;
    bool operator==(const AgricultureCalendarStatus&) const = default;
};

struct AgricultureSeasonStatus {
    BuildingId subject{},producer{};
    original::ModelBuilding model{};
    AgricultureCalendarStatus calendar;
    bool reference_growing_period=false,reference_growth_suppressed=false;
    bool authored_production_active=false;
    std::uint32_t productive_progress=0,productive_target=0;
    bool harvest_ready=false,actual_grower_active=false;
    std::uint64_t harvested=0,returned=0;
    bool operating=false,burning=false,staffed=false,road_connected=false;
    int assigned_workers=0,required_workers=0;
    bool water_available=false,yield_effect_active=false;
    bool seasonal_production=false,growing=false,harvest_allowed=false;
    std::uint32_t harvest_progress=0;
    std::uint64_t wasted_work=0,expired_ready=0;
    std::optional<std::uint64_t> ready_expiry_tick,next_opening_tick;
    // Original work-word, harvest lifecycle and unit binding remain unresolved.
    std::string blocker;
};

} // namespace openemperor::simulation
