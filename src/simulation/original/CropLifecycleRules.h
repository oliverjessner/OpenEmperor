#pragma once

#include "simulation/original/CropRules.h"

#include <cstdint>

namespace openemperor::original {

// One explicitly resolved phase20 schedule invocation for an actual Food field.
// At its end slot and subslot0, an active field retains word|0x8000. Quantity,
// staffing, assignment and terrain do not gate this setter. It establishes no
// actual harvest readiness or actor dispatch. Slots0..11 and subslots0..15 are
// caller inputs; this operation does not advance or map a clock.
[[nodiscard]] std::uint16_t schedule_crop_harvest_mark(ModelBuilding field,
    std::uint8_t slot,std::uint8_t subslot,bool active,std::uint16_t work_word);

struct CropFieldLifecycleInput {
    std::uint16_t quantity=0;
    std::uint16_t work_word=0;
    bool assignment_present=false;
    bool operator==(const CropFieldLifecycleInput&) const = default;
};

struct CropFieldExpiration {
    CropFieldLifecycleInput field;
    std::uint16_t expired_quantity=0;
    bool reset_applied=false;
    bool clear_assigned_actor_wait=false;
    bool operator==(const CropFieldExpiration&) const = default;
};

// One resolved COMPLETED-outer-slot event, before the source advances its slot.
// Only native completed slots6..11 reach the reset manager; active Food fields
// whose last configured end slot matches lose quantity/work/assignment. The
// separate wait clear requires the caller's actual retained actor assignment.
// Supported quantity is0..10000. Countdown/history/body+150 are not mutated by
// the checked reset and remain outside this value operation. Expired field
// quantity is neither a Food/cargo debit nor a yield or portion conversion.
[[nodiscard]] CropFieldExpiration expire_crop_completed_slot(ModelBuilding field,
    std::uint8_t completed_slot,bool active,CropFieldLifecycleInput input);

} // namespace openemperor::original
