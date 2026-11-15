#include "simulation/original/CropSelectionRules.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

void validate_work(CropWork work) {
    if (work != CropWork::Tending && work != CropWork::Harvest)
        throw std::out_of_range("unsupported crop selection role");
}

} // namespace

CropCandidateEvaluation evaluate_crop_initial_candidate(
    CropWork work, const CropCandidateFacts& facts) {
    validate_work(work);
    (void)crop_rule(facts.field);
    if (facts.quantity > 10000 || facts.prepared_distance < 0 ||
        facts.fertility_query < 0 || facts.fertility_query > 100)
        throw std::out_of_range("crop candidate input is outside its supported physical domain");

    CropCandidateEvaluation result{false, facts.work_word, 0};
    if (facts.status != 1) return result;
    const bool marked = (facts.work_word & 0x8000U) != 0;
    std::int32_t extra = 0;
    if (work == CropWork::Harvest) {
        if (!facts.resource_enabled || !facts.operating || !marked || facts.prepared_distance == 0)
            return result;
        if (facts.quantity == 0) {
            result.retained_work_word = static_cast<std::uint16_t>(facts.work_word & 0x7fffU);
            return result;
        }
        if (!facts.within_source_range || !facts.assignment_matches) return result;
        const auto quantity_percent = facts.quantity / 100;
        extra = (100 - quantity_percent) / 2;
    } else {
        if (!facts.growing_period || !facts.assignment_matches || marked ||
            facts.prepared_distance == 0 || !facts.within_source_range)
            return result;
        extra = static_cast<std::int32_t>(facts.work_word & 0xffU) / 2 +
            (100 - facts.fertility_query);
    }
    if (facts.prepared_distance > std::numeric_limits<std::int32_t>::max() - extra)
        throw std::overflow_error("reached crop candidate score overflows signed-DWORD arithmetic");
    result.score = facts.prepared_distance + extra;
    result.eligible = true;
    return result;
}

std::vector<std::size_t> order_crop_initial_candidates(
    std::span<const std::int32_t> scores, std::int32_t requested_count) {
    if (scores.size() > 2000 || requested_count < 0)
        throw std::out_of_range("crop candidate queue exceeds its proved bounds");
    std::vector<std::size_t> order;
    order.reserve(scores.size());
    std::size_t last_position = 0;
    for (std::size_t index = 0; index < scores.size(); ++index) {
        std::size_t insertion = 0;
        if (!order.empty()) {
            auto position = last_position;
            if (scores[index] <= scores[order[position]]) {
                while (position > 0 && scores[index] < scores[order[position]]) --position;
                insertion = scores[index] < scores[order[position]] ? 0 : position + 1;
            } else {
                while (position < order.size() && scores[index] > scores[order[position]]) ++position;
                insertion = position;
            }
        }
        order.insert(order.begin() + static_cast<std::ptrdiff_t>(insertion), index);
        last_position = insertion;
    }
    order.resize(std::min(order.size(), static_cast<std::size_t>(requested_count)));
    return order;
}

CropSpawnCommit commit_crop_spawn(CropWork work, bool actual_creation_succeeded,
    std::int32_t successful_spawn_ordinal, std::int32_t retained_counter) {
    validate_work(work);
    if (successful_spawn_ordinal < 0 || successful_spawn_ordinal >= 2000 || retained_counter < 0)
        throw std::out_of_range("crop spawn arithmetic input is outside its supported domain");
    CropSpawnCommit result{retained_counter, std::nullopt};
    if (!actual_creation_succeeded) return result;
    if (retained_counter == std::numeric_limits<std::int32_t>::max())
        throw std::overflow_error("reached crop spawn counter increment overflows signed-DWORD arithmetic");
    ++result.retained_counter;
    result.actor = CropSpawnInitialization{
        static_cast<std::uint8_t>(work == CropWork::Tending ? 6 : 10),
        static_cast<std::uint8_t>(work == CropWork::Tending ? 1 : 2),
        static_cast<std::int16_t>(successful_spawn_ordinal * 10)};
    return result;
}

} // namespace openemperor::original
