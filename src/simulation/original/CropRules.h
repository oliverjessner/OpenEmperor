#pragma once

#include "simulation/original/ModelSchema.h"

#include <cstdint>

namespace openemperor::original {

// Original field models, raw resource IDs and calendar slots are separate
// domains. This descriptor is an observation, never a World purchase or clock.
struct CropRule {
    ModelBuilding field;
    std::uint8_t crop_index;
    std::uint8_t resource_id;
    std::uint8_t start_slot;
    std::uint8_t end_slot;
    bool operator==(const CropRule&) const = default;
};

[[nodiscard]] const CropRule& crop_rule(ModelBuilding field);

// Slots are the observed outer calendar positions 0..11. The period query
// includes start and excludes end. It does not gate the growth callback below.
[[nodiscard]] bool crop_in_growing_period(ModelBuilding field, std::uint8_t slot);

// One resolved Food-field callback. It suppresses the configured end slot,
// otherwise reads the low work byte. No actor arrival or callback cadence is
// inferred, and calling this function does not grow a field.
[[nodiscard]] std::uint16_t crop_growth_increment(
    ModelBuilding field, std::uint8_t slot, std::uint16_t work_word);

// Bounded nonnegative field-quantity arithmetic. The observed driver adds a
// WORD then caps at 10000. Inputs 0..10000 and deltas 0/79/157 cannot overflow
// that WORD. Other raw signed initial states remain outside this API.
[[nodiscard]] std::uint16_t apply_crop_growth(
    std::uint16_t quantity, std::uint16_t increment);

// Integer first stage of an actual raw harvest. Quantity 0..10000 is divided
// by 100, or by 50 when the separately resolved crop-qualified event applies.
// This intermediate precedes runtime fertility, region and floating modifiers.
// It supplies neither a final harvest nor a conversion to World Food portions.
[[nodiscard]] std::uint16_t raw_base_yield(std::uint16_t quantity, bool doubled_base);

// One source runtime-fertility preparation operation, with the global modifier
// supplied explicitly. Serialized and runtime fertility are distinct bytes.
// The final byte conversion retains the observed modulo-256 addition.
[[nodiscard]] std::uint8_t runtime_fertility(
    std::uint8_t serialized_fertility, std::uint8_t modifier) noexcept;

// One actual tending-completion word effect. The separate field assignment
// clear and actor state transition remain the caller's responsibility.
[[nodiscard]] std::uint16_t complete_crop_tending(std::uint16_t work_word) noexcept;

// One observed decay call retains only flags 0xc000 and the decayed low byte.
// Bits 8..13 are cleared. Neither operation implies one World tick.
[[nodiscard]] std::uint16_t decay_crop_work(std::uint16_t work_word) noexcept;

enum class CropWork : std::uint8_t { Tending = 0, Harvest = 1 };

// One checked wait-initialization branch for Food fields 195..199. The field
// predicate and nonzero retained field-body word +0x2e are resolved inputs.
// This API takes that observed word test explicitly.
// Terrain flag 0x100 overrides the result with zero. Waits count eligible
// actor-state invocations, with no wall-clock or World-tick interpretation.
[[nodiscard]] std::int16_t initial_crop_work_wait(
    ModelBuilding field, CropWork work, bool field_accepted,
    bool retained_field_word_nonzero, bool terrain_flag_100);

struct CropWaitStep {
    std::int16_t remaining;
    bool callback_due;
    bool operator==(const CropWaitStep&) const = default;
};

// One raw signed-WORD decrement followed by a signed >0 test. Wrap is modeled
// explicitly, including -32768 -> 32767. Due only describes the counter gate.
[[nodiscard]] CropWaitStep advance_crop_work_wait(std::int16_t wait) noexcept;

} // namespace openemperor::original
