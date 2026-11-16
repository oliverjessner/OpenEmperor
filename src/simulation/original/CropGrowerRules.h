#pragma once

#include "simulation/original/CropRules.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace openemperor::original {

// These are native signed-WORD observations, not World cells or monotone IDs.
struct CropGrowerCell {
    std::int16_t x=0,y=0;
    bool operator==(const CropGrowerCell&) const = default;
};

// The ordinary Food path uses states6..13, role1 tending/2 harvest and
// status1 active/2 retiring. Source/field words are native references in the
// nonnegative signed-WORD domain; source is nonzero, field may be0. +7c is
// the SAME carried raw-yield word throughout work and return, not two stocks.
// All16bits are retained so a wrapped/nonpositive native amount is observable.
// This DTO grants no actor, route, field claim, yield, source receipt or save.
struct CropGrowerActor {
    std::uint16_t raw_source_word=0,raw_field_word=0;
    std::uint8_t raw_state=0,raw_status=0,raw_role=0;
    std::int16_t wait=0;
    std::uint16_t raw_yield_word=0;
    CropGrowerCell target;
    std::uint8_t raw_flag_6f=0;
    bool operator==(const CropGrowerActor&) const = default;
};

// Native source-status check precedes the body. Mark status2 for any source
// status other than1, clear rawflag6f, and continue. Later movement/field
// observations must describe THIS post-begin actor, never an assumed status1.
// Supports state4 and the separately bounded ordinary states6..13 only.
[[nodiscard]] CropGrowerActor begin_crop_grower_callback(
    CropGrowerActor actor,std::uint8_t source_status);

// Explicit shared-helper arithmetic for native state4 only. Negative wait is
// reset to0, then WORD-incremented. Signed result>=128 becomes127/status2.
// Preserve the native32767->-32768 wrap. Other represented fields, including
// rawflag6f, stay unchanged; begin owns its clear. External state4 entry/meaning
// remains unknown; this helper does not infer an injury, death or retirement event.
[[nodiscard]] CropGrowerActor advance_crop_grower_state4_wait(CropGrowerActor actor);

struct CropGrowerCounters {
    std::int32_t harvest=0,tending=0;
    bool operator==(const CropGrowerCounters&) const = default;
};

enum class CropGrowerFieldCallback : std::uint8_t { None,Tending,Harvest };

struct CropGrowerWorkStep {
    CropGrowerActor actor;
    bool select_target=false;
    CropGrowerFieldCallback field_callback=CropGrowerFieldCallback::None;
    bool operator==(const CropGrowerWorkStep&) const = default;
};

// One body invocation in native state6/8/10/12. Selection states set flag6f,
// decrement the wait, and reset a due wait to0 before requesting the actual
// source selector. Work states keep the decremented word and advance to6/10
// when due even when the retained field is incompatible. Only compatibility
// gates the actual field callback: matching assignment, quantity, calendar,
// staffing and harvest mark are NOT extra gates in this helper. The callback
// request alone is not completed field work or yield; use the field rules.
[[nodiscard]] CropGrowerWorkStep advance_crop_grower_work(
    CropGrowerActor actor,bool field_compatible);

struct CropGrowerTarget {
    std::uint16_t raw_field_word=0;
    CropGrowerCell position;
    bool operator==(const CropGrowerTarget&) const = default;
};

struct CropGrowerTargetStep {
    CropGrowerActor actor;
    CropGrowerCounters counters;
    bool clear_route=true;
    bool operator==(const CropGrowerTargetStep&) const = default;
};

// Actual selector result, not ranking/admission/navigation. State6 additionally
// rechecks the target's growing-period result. Every attempt clears the route.
// A valid target sets field/target and state7/11. Failure retains the LAST field
// reference, targets the source-return coordinates and immediately debits the
// corresponding retained counter, floored at0. Failed harvest selects13 only
// for a SIGNED-positive +7c word; zero/negative selects9. Counters are not a
// live actor census. Require state6/10 and nonnegative signed-DWORD counters.
[[nodiscard]] CropGrowerTargetStep apply_crop_grower_target(
    CropGrowerActor actor,CropGrowerCounters counters,
    const std::optional<CropGrowerTarget>& actual_target,
    bool target_in_growing_period,CropGrowerCell source_return);

struct CropGrowerMovement {
    std::uint8_t outcome=0;
    std::optional<ModelBuilding> food_field;
    bool field_compatible=false;
    bool field_assignment_nonzero=false;
    bool terrain_flag_100=false;
};

struct CropGrowerMovementStep {
    CropGrowerActor actor;
    bool clear_route=false;
    bool source_receipt=false;
    bool operator==(const CropGrowerMovementStep&) const = default;
};

// Resolved shared-movement result for states7/9/11/13. Outcome9 requests route
// clearing and retries. Outcome10 retires WITHOUT a source receipt. Outcome8
// initializes Food work waits in7/11, retires empty return9, or requests the
// source's ACTUAL receipt and retires13. Other outcomes preserve the state.
// Accepted Food work arrivals require a typed195..199 field observation. An
// incompatible/unresolved field can initialize zero wait but cannot authorize
// a later callback. Food identity/assignment/terrain are explicit operands.
// Movement outcome8 is supplied evidence, not computed physical World arrival.
[[nodiscard]] CropGrowerMovementStep apply_crop_grower_movement(
    CropGrowerActor actor,const CropGrowerMovement& movement);

struct CropGrowerCallbackFinish {
    CropGrowerActor actor;
    CropGrowerCounters counters;
    bool retirement_requested=false;
    bool operator==(const CropGrowerCallbackFinish&) const = default;
};

// Apply after begin_crop_grower_callback and its actual resolved body/target
// result. Any retained status2 debits its role's counter again, floored at0.
// State4 and ordinary6..13 are supported; the other source branches are not.
// In particular, failure-selection debit followed by retirement is two native
// debits. This neither retires a World entity nor destroys/credits raw yield.
[[nodiscard]] CropGrowerCallbackFinish finish_crop_grower_callback(
    CropGrowerActor actor,CropGrowerCounters counters);

struct CropGrowerFieldAssignment {
    std::uint8_t raw_status=0;
    std::uint16_t raw_actor_word=0;
    bool operator==(const CropGrowerFieldAssignment&) const = default;
};

// One actual type26 manager-removal field pass. Clear every status1 field's
// matching actor word; inactive fields and other identities remain unchanged.
// ID1..32767 is a safe native-reference domain, not allocation/World authority.
// Invalid actor IDs are refused before any mutation. No field work is cleared.
[[nodiscard]] std::size_t clear_crop_grower_assignments(
    std::span<CropGrowerFieldAssignment> fields,std::uint16_t actual_actor_word);

struct CropGrowerReset {
    CropGrowerActor actor;
    std::uint16_t cleared_raw_yield_word=0;
    bool operator==(const CropGrowerReset&) const = default;
};

// Represented fields of the native reset AFTER actual manager removal.
// Require retiring status2. The reported old +7c word is neither automatically
// lost cargo nor a refund: successful source receipt leaves it retained until
// reset too. Its disposition needs the separate receipt/accepted-stock facts.
// Shared route/link ownership and generic actor fields are outside this DTO.
[[nodiscard]] CropGrowerReset reset_retired_crop_grower(CropGrowerActor actor);

} // namespace openemperor::original
