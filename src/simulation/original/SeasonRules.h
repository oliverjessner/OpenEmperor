#pragma once

#include "simulation/original/ModelSchema.h"

#include <array>
#include <cstdint>

namespace openemperor::original {

enum class SeasonSuppression : std::uint8_t { None, LastEndSlot, AnyEndSlot };

// Pinned default EXE configuration, separate from a live runtime calendar.
// Entries are raw outer slots0..11; unused positions retain sentinel-1.
// Start/end arrays keep their original positions and independent counts.
// In particular Lacquer has one start and two ends, not two growing periods.
struct SeasonRule {
    ModelBuilding plant;
    std::uint8_t crop_index;
    std::uint8_t resource_id;
    std::array<std::int8_t, 4> start_slots;
    std::array<std::int8_t, 4> end_slots;
    std::uint8_t start_count;
    std::uint8_t end_count;
    SeasonSuppression suppression;
    std::uint16_t high_work_increment;
    std::uint16_t low_work_increment;
    bool operator==(const SeasonRule&) const = default;
};

[[nodiscard]] const SeasonRule& season_rule(ModelBuilding plant);

// Original4c0760 query: any position with both operands nonnegative and
// start<=slot<end. No wrapping intervals or named months are inferred.
// Food models195..199 use the existing CropRules predicate unchanged.
[[nodiscard]] bool season_in_growing_period(ModelBuilding plant, std::uint8_t slot);

// Suppression is the concrete quantity callback's calendar predicate, separate
// from the growing-period query. Hemp has no calendar suppression. Tea and
// Mulberry suppress only their last configured end; Lacquer suppresses any end.
[[nodiscard]] bool season_growth_suppressed(ModelBuilding plant, std::uint8_t slot);

// One resolved quantity callback. Zero low work byte returns0;1..32 use the
// low increment and33..255 the high increment, unless explicitly suppressed.
// This does not run the shared terrain/countdown/veto driver, schedule workers,
// advance World time, imply readiness, harvest goods or grant production.
// Food models195..199 delegate the existing CropRules arithmetic unchanged.
[[nodiscard]] std::uint16_t season_growth_increment(
    ModelBuilding plant, std::uint8_t slot, std::uint16_t work_word);

} // namespace openemperor::original
