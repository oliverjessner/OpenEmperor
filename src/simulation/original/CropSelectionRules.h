#pragma once

#include "simulation/original/CropRules.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace openemperor::original {

// Inputs to one initial Food-field candidate evaluation for a resolved Farm193
// scan. These are source observations, not map admission or a World assignment.
struct CropCandidateFacts {
    ModelBuilding field{};
    std::uint8_t status = 1;
    std::uint16_t quantity = 0;
    std::uint16_t work_word = 0;
    std::int32_t prepared_distance = 0;
    // Resolved query4c1af0, which reads runtime fertility or a global override.
    // It is not a countdown or the serialized construction-fertility byte.
    std::int32_t fertility_query = 0;
    bool assignment_matches = false;
    bool within_source_range = false;
    bool growing_period = false;
    bool resource_enabled = false;
    bool operating = false;
    bool operator==(const CropCandidateFacts&) const = default;
};

struct CropCandidateEvaluation {
    bool eligible = false;
    std::uint16_t retained_work_word = 0;
    std::int32_t score = 0;
    bool operator==(const CropCandidateEvaluation&) const = default;
};

// Food195..199 only. Quantity0..10000, distance>=0 and resolved fertility0..100
// define the supported physical domain. Other native callback/admission facts
// remain explicit. Harvest Q0 clears its mark after the positive-distance gate,
// before range/assignment checks, even though it cannot become a candidate.
// Tending selection has no resource-enabled or operating check at this stage.
// Reached signed-DWORD score overflow is refused. No route or work is created.
[[nodiscard]] CropCandidateEvaluation evaluate_crop_initial_candidate(
    CropWork work, const CropCandidateFacts& facts);

// A fresh native initial-candidate queue has at most2000 nodes. Return input
// indices in its actual score order, limited by a nonnegative requested count.
// Cached-last insertion makes equal-score order depend on earlier insertions;
// it is not a stable sort or an entity/crop ownership policy. Scores retain the
// full signed-DWORD domain. No navigator, source manager or actor is created.
[[nodiscard]] std::vector<std::size_t> order_crop_initial_candidates(
    std::span<const std::int32_t> scores, std::int32_t requested_count);

struct CropSpawnInitialization {
    std::uint8_t state = 0;
    std::uint8_t role = 0;
    std::int16_t initial_wait = 0;
    bool operator==(const CropSpawnInitialization&) const = default;
};

struct CropSpawnCommit {
    std::int32_t retained_counter = 0;
    std::optional<CropSpawnInitialization> actor;
    bool operator==(const CropSpawnCommit&) const = default;
};

// Only the post-factory arithmetic. An actual successful creation increments
// its role's retained source counter and initializes state6/role1 or state10/
// role2 with wait10*successful ordinal. Failure changes no counter or actor.
// Ordinals0..1999 are this API's support domain for successfully populated
// queues with at most2000 selected entries; failed insertion/count-wrap histories
// do not establish a universal native worker limit. Counter inputs are
// nonnegative signed-DWORD observations; a reached increment overflow refuses.
// This creates no ID, field binding, claim, channel, goods or World authority.
[[nodiscard]] CropSpawnCommit commit_crop_spawn(CropWork work,
    bool actual_creation_succeeded, std::int32_t successful_spawn_ordinal,
    std::int32_t retained_counter);

} // namespace openemperor::original
