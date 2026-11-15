#pragma once

#include "simulation/original/ModelSchema.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace openemperor::original {

// These are Original source identities and raw operands. Descriptors confer
// no World building, actor, source-point population or Food inventory authority.
enum class HarvestProducer : std::uint16_t { FishingQuay = 31, HuntersTent = 33 };
enum class HarvestResource : std::uint8_t { Fish = 2, Meat = 4 };
enum class HarvestReceipt : std::uint8_t { Assign, Add };

struct HarvestRule {
    ModelBuilding model;
    HarvestResource resource;
    std::uint16_t source_point_model_id;
    std::uint8_t worker_actor;
    std::uint8_t cart_actor;
    std::uint8_t initial_side;
    std::uint16_t raw_return_amount;
    std::uint16_t ordinary_raw_cart_amount;
    std::optional<std::uint16_t> raw_source_capacity;
    HarvestReceipt receipt;
    bool operator==(const HarvestRule&) const = default;
};

[[nodiscard]] const HarvestRule& harvest_rule(HarvestProducer producer);
[[nodiscard]] std::string_view harvest_resource_name(HarvestResource resource);
[[nodiscard]] bool huntable_actor(std::uint8_t actor) noexcept;

enum class PreyActor : std::uint8_t { WildPig = 75, Pheasant = 76, Antelope = 77 };

// These are the complete raw WORD selector cases, with no inferred climate
// names. Actual creation additionally requires the source initializer's zero
// bypass flag, positive source coordinates, successful source-group allocation
// and an admitted spawn candidate. Seven attempts do not guarantee seven actors.
inline constexpr std::uint8_t initial_prey_attempts_per_eligible_point = 7;
[[nodiscard]] PreyActor initial_prey_actor(std::uint16_t source_selector) noexcept;
[[nodiscard]] std::string_view prey_actor_name(PreyActor actor);

struct HunterDispatchRule {
    bool staffed;
    std::uint8_t attempt_threshold;
    std::uint8_t hunter_limit;
    bool operator==(const HunterDispatchRule&) const = default;
};

// Staff percentage0..100 and raw source stock0..400 are resolved operands.
// The limit combines the staffing tier with floor((400-stock)/100). It is
// a quota, not a successful spawn or proof of prey/route/source eligibility.
[[nodiscard]] HunterDispatchRule hunter_dispatch_rule(
    std::uint8_t staff_percent, std::uint16_t raw_stock);

struct HunterAttemptStep {
    std::uint8_t counter;
    bool attempt_due;
    bool operator==(const HunterAttemptStep&) const = default;
};

// One resolved source-update throttle. The byte increments before a strict
// >threshold comparison and resets on a due attempt. Byte wrap is explicit.
// Zero staffing preserves the counter. Counts have no World-tick mapping.
[[nodiscard]] HunterAttemptStep advance_hunter_attempt(
    std::uint8_t counter, std::uint8_t staff_percent, std::uint16_t raw_stock);

enum class BoatWorkStage : std::uint8_t { Fishing = 8, Dock = 10 };

// Work thresholds count actual eligible callback invocations. Zero staffing
// has no work threshold; the separate interruption/global-effect helper is
// outside these counter operations.
[[nodiscard]] std::optional<std::uint16_t> boat_work_threshold(
    BoatWorkStage stage, std::uint8_t staff_percent);

struct BoatCounterStep {
    std::int16_t counter;
    bool transition_due;
    bool operator==(const BoatCounterStep&) const = default;
};

// Fishing compares the incremented signed WORD. Dock compares the old WORD,
// then increments. Both reset the counter when their transition is due.
// Signed WORD wrap is modeled explicitly. This does not move a boat, choose
// a destination, finish its return or assign any source stock.
[[nodiscard]] BoatCounterStep advance_boat_work_counter(
    BoatWorkStage stage, std::int16_t counter, std::uint8_t staff_percent);

// One resolved state6 search-wait branch. Increment, then compare >=51 and
// reset on the due search attempt. Actual point selection may still fail.
[[nodiscard]] BoatCounterStep advance_boat_search_counter(std::int16_t counter) noexcept;

struct HarvestStockStep {
    std::uint16_t stock;
    std::uint16_t overwritten_stock;
    bool operator==(const HarvestStockStep&) const = default;
};

// One already-resolved successful return branch in the supported nonnegative
// stock domain. Hunter adds50 within its400 capacity; overflow is refused.
// Quay assigns100 and exposes the prior stock replaced by that assignment.
// A World conservation policy for that replacement remains a separate gate.
[[nodiscard]] HarvestStockStep harvest_source_return(
    HarvestProducer producer, std::uint16_t raw_stock);

struct HarvestCartStep {
    std::uint16_t stock;
    std::uint16_t cargo;
    bool operator==(const HarvestCartStep&) const = default;
};

// Call only for an actual successful Actor19 creation after its source gates.
// The ordinary effect byte must be exactly0. The separate nonzero-byte400
// branch is unsupported and rejected. All quantities remain Original raw
// units; no Food portion, Warehouse acceptance or delivery is inferred.
[[nodiscard]] HarvestCartStep ordinary_harvest_cart_debit(
    HarvestProducer producer, std::uint16_t raw_stock, std::uint8_t external_effect_byte);

} // namespace openemperor::original
