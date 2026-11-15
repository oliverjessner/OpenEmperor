#pragma once

#include "simulation/original/CropRules.h"

#include <array>
#include <cstdint>
#include <optional>

namespace openemperor::original {

// Explicit operands at the actual marked Food-field callback. Their runtime
// owners, validity and live control-word admission remain the caller's proof
// obligations. No startup default, FarmConfig loader or World clock is implied.
struct CropHarvestContext {
    std::uint8_t runtime_fertility = 0;
    bool irrigated = false;
    std::uint32_t region_factor_binary32 = 0;
    std::int32_t raw_date_operand = 0;
    std::uint32_t global_factor_binary32 = 0;
    std::uint16_t x87_control_word = 0;
    std::uint8_t event_enabled = 0;
    std::uint32_t event_state = 0;
    std::uint32_t event_crop_selector = 0;
    bool operator==(const CropHarvestContext&) const = default;
};

struct CropHarvestYield {
    std::uint16_t integer_base = 0;
    std::uint32_t fertility_factor_binary32 = 0;
    std::uint32_t first_integer_yield = 0;
    std::uint32_t numeric_yield = 0;
    // The source retains AX in history and adds that WORD to pending yield.
    // This modulo-65536 observation is not a safe physical cargo capacity.
    std::uint16_t retained_yield_word = 0;
    bool operator==(const CropHarvestYield&) const = default;
};

// The locally qualified nonfaulting arithmetic of4c1dd0 for Food195..199,
// evaluated without host floating-point arithmetic or changes to its FPU.
// Quantity0..10000; finite nonnegative raw binary32 factors (signed zero is
// accepted). PC24/53/64 and every RC are explicit in the supplied control word;
// all six native FP exception masks must be set. Reserved PC01 is unsupported.
// Reproduce each precision rounding, both binary32 fertility stores, the first
// truncation, and the signed-int32 reload/global multiply only when raw date
// is greater than-300. An unqueried global factor is not decoded or validated.
// Both reached truncations must lie in0..INT32_MAX; wrapping/invalid conversions
// are refused. Final WORD narrowing is exposed separately from numeric yield.
// This operation does not check a harvest mark, update fields/history/pending,
// create cargo, perform a return or grant any World production authority.
[[nodiscard]] CropHarvestYield calculate_crop_harvest_yield(
    ModelBuilding field, std::uint16_t quantity, const CropHarvestContext& context);

// Native field operands only. assigned_actor_word is a source WORD, not a
// newly admitted World identity. body_counter denotes body+150, not a timer
// inferred from the quantity or the separate actor wait word.
struct CropHarvestFieldState {
    std::uint16_t quantity = 0;
    std::uint16_t work_word = 0;
    std::uint32_t body_counter = 0;
    std::uint16_t assigned_actor_word = 0;
    std::array<std::uint16_t, 12> history{};
    bool operator==(const CropHarvestFieldState&) const = default;
};

// One actual Food-field tending callback4c1d20, without caller eligibility or
// arrival inference. Reuse the existing complete_crop_tending work-word rule,
// clear the actual body counter and assignment, and preserve quantity/history.
// This does not acquire a harvest mark, calculate yield or modify actor cargo.
// Require Food195..199 and quantity0..10000 before publishing the value result.
[[nodiscard]] CropHarvestFieldState apply_crop_tending_callback(
    ModelBuilding field, CropHarvestFieldState state);

struct CropHarvestCallback {
    CropHarvestFieldState field;
    // The same resolved actual actor+7c operand, not a second pending/cargo
    // inventory. Its physical interpretation/lifecycle remains external.
    std::uint16_t actor_yield_word = 0;
    std::optional<CropHarvestYield> yield;
    bool numeric_word_narrowed = false;
    bool actor_word_wrapped = false;
    bool operator==(const CropHarvestCallback&) const = default;
};

// One actual field callback, not proof that its caller was eligible or arrived.
// OR4000 and clear body+150 before testing the retained8000 mark. Without the
// mark, preserve quantity/history/assignment/actor word and ignore context.
// With it, calculate the resolved yield, consume8000, overwrite only the current
// history WORD, add the retained result modulo65536 to the actual actor WORD,
// and clear quantity/assignment. The source callback does not compare assignment
// with the passed actor; this API makes no such admission claim. Slot0..11 and
// quantity0..10000 are required. Unsupported arithmetic publishes no partial
// result. Word narrowing/carry are reported, never converted to World capacity.
[[nodiscard]] CropHarvestCallback apply_crop_harvest_callback(
    ModelBuilding field, std::uint8_t slot, CropHarvestFieldState state,
    std::uint16_t actual_actor_yield_word, const CropHarvestContext& context);

} // namespace openemperor::original
