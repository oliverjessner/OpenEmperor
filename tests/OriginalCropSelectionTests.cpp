#include "simulation/original/CropSelectionRules.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace original = openemperor::original;
namespace {
std::uint64_t checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Exception, class Operation> void refuses(Operation operation) {
    try { operation(); } catch (const Exception&) { ++checks; return; }
    throw std::runtime_error("unsupported crop selection input was accepted");
}
constexpr std::array fields{
    original::ModelBuilding::WheatField, original::ModelBuilding::MilletField,
    original::ModelBuilding::RiceField, original::ModelBuilding::CabbageField,
    original::ModelBuilding::SoybeanField};

original::CropCandidateFacts ready(original::ModelBuilding field) {
    return {field, 1, 7500, 0x8000, 12, 65, true, true, true, true, true};
}

void candidate_gates_and_side_effects() {
    for (const auto field : fields) {
        auto facts = ready(field);
        check(original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts) ==
                  original::CropCandidateEvaluation{true, 0x8000, 24},
              "harvest score uses quantity percentage and prepared distance");
        facts.work_word = 255;
        check(original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts) ==
                  original::CropCandidateEvaluation{true, 255, 174},
              "Food tending score uses low work, distance and resolved fertility query");
        facts.resource_enabled = false;
        facts.operating = false;
        check(original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts).eligible,
              "initial tending must not acquire later resource/operation vetoes");
        for (unsigned mask = 0; mask < 256; ++mask) {
            facts = ready(field);
            facts.status = (mask & 1U) ? 1 : 2;
            facts.assignment_matches = (mask & 2U) != 0;
            facts.within_source_range = (mask & 4U) != 0;
            facts.growing_period = (mask & 8U) != 0;
            facts.resource_enabled = (mask & 16U) != 0;
            facts.operating = (mask & 32U) != 0;
            facts.prepared_distance = (mask & 64U) ? 12 : 0;
            facts.work_word = (mask & 128U) ? 0x8000 : 63;
            const bool harvest = (mask & 247U) == 247U;
            const bool tending = (mask & 79U) == 79U && (mask & 128U) == 0;
            check(original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts).eligible == harvest,
                  "independent harvest gate truth table");
            check(original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts).eligible == tending,
                  "independent tending gate truth table");
        }
        facts = ready(field);
        facts.quantity = 0;
        facts.work_word = 0xffff;
        facts.assignment_matches = false;
        facts.within_source_range = false;
        check(original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts) ==
                  original::CropCandidateEvaluation{false, 0x7fff, 0},
              "Q0 clears only harvest mark before later range and assignment gates");
        for (unsigned early_gate = 0; early_gate < 4; ++early_gate) {
            auto blocked = facts;
            if (early_gate == 0) blocked.status = 2;
            if (early_gate == 1) blocked.resource_enabled = false;
            if (early_gate == 2) blocked.operating = false;
            if (early_gate == 3) blocked.prepared_distance = 0;
            check(original::evaluate_crop_initial_candidate(original::CropWork::Harvest, blocked).retained_work_word == 0xffff,
                  "an earlier gate must prevent Q0 mark clearing");
        }
        for (std::uint32_t quantity = 1; quantity <= 10000; ++quantity) {
            facts = ready(field);
            facts.quantity = static_cast<std::uint16_t>(quantity);
            const auto result = original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts);
            check(result.eligible && result.score == 12 + (100 - static_cast<int>(quantity / 100U)) / 2,
                  "all physical quantities preserve percentage truncation");
        }
        for (unsigned work = 0; work <= 65535; ++work) {
            facts = ready(field);
            facts.work_word = static_cast<std::uint16_t>(work);
            facts.fertility_query = static_cast<std::int32_t>(work % 101U);
            const auto result = original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts);
            check(result.retained_work_word == work &&
                      result.eligible == ((work & 0x8000U) == 0),
                  "tending never changes work bits or ignores retained harvest mark");
            if (result.eligible)
                check(result.score == 112 + static_cast<int>(work & 255U) / 2 - facts.fertility_query,
                      "high work flags cannot change Food tending priority");
        }
    }
}

void literal_queue_order() {
    const auto expect = [](std::initializer_list<std::int32_t> scores,
                           std::initializer_list<std::size_t> expected) {
        const std::vector<std::int32_t> input(scores);
        const std::vector<std::size_t> order(expected);
        check(original::order_crop_initial_candidates(input, 2000) == order,
              "native initial candidate order differs from literal cached-insertion witness");
        for (std::size_t count = 0; count <= order.size(); ++count)
            check(original::order_crop_initial_candidates(input, static_cast<std::int32_t>(count)) ==
                      std::vector<std::size_t>(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(count)),
                  "requested candidate count must keep the exact native prefix");
    };
    expect({}, {});
    expect({5}, {0});
    expect({5, 5, 5}, {0, 1, 2});
    expect({10, 10, 0, 10}, {2, 3, 0, 1});
    expect({10, 10, 20, 10}, {0, 1, 3, 2});
    expect({2, 1, 2, 1, 2}, {1, 3, 4, 2, 0});
    expect({3, 2, 1, 3, 2, 1}, {2, 5, 1, 4, 3, 0});
    expect({0, 1, 0, 1, 0}, {0, 2, 4, 3, 1});
    expect({1, 0, 1, 0, 1}, {1, 3, 4, 2, 0});
    expect({3, 1, 2, 1, 3, 2}, {1, 3, 2, 5, 4, 0});
    expect({1, 3, 1, 2, 3, 2}, {0, 2, 3, 5, 4, 1});
    expect({2, 2, 3, 2, 1, 2}, {4, 5, 0, 1, 3, 2});
    constexpr auto lo = std::numeric_limits<std::int32_t>::min();
    constexpr auto hi = std::numeric_limits<std::int32_t>::max();
    expect({hi, lo, 0, hi, lo}, {1, 4, 2, 3, 0});

    for (unsigned sequence = 0; sequence < 2187; ++sequence) {
        std::array<std::int32_t, 7> scores{};
        auto digits = sequence;
        for (auto& score : scores) { score = static_cast<std::int32_t>(digits % 3U); digits /= 3U; }
        const auto before = scores;
        const auto order = original::order_crop_initial_candidates(scores, 2000);
        auto permutation = order;
        std::sort(permutation.begin(), permutation.end());
        check(permutation == std::vector<std::size_t>({0, 1, 2, 3, 4, 5, 6}),
              "initial queue must preserve every candidate exactly once");
        check(std::is_sorted(order.begin(), order.end(), [&](auto a, auto b) { return scores[a] < scores[b]; }),
              "native tie ordering must still preserve signed score order");
        check(scores == before, "ordering must not mutate input scores");
    }
    std::vector<std::int32_t> full(2000, 7);
    const auto order = original::order_crop_initial_candidates(full, 2000);
    for (std::size_t index = 0; index < full.size(); ++index)
        check(order[index] == index, "full equal-score queue preserves its literal insertion order");
    full.push_back(7);
    refuses<std::out_of_range>([&] { (void)original::order_crop_initial_candidates(full, 0); });
    refuses<std::out_of_range>([] { (void)original::order_crop_initial_candidates({}, -1); });
}

void spawn_commit_and_refusals() {
    constexpr auto maximum = std::numeric_limits<std::int32_t>::max();
    for (const auto work : {original::CropWork::Tending, original::CropWork::Harvest}) {
        for (std::int32_t ordinal = 0; ordinal < 2000; ++ordinal) {
            check(original::commit_crop_spawn(work, false, ordinal, maximum) ==
                      original::CropSpawnCommit{maximum, std::nullopt},
                  "failed actual creation cannot initialize actor or increment retained counter");
            const auto result = original::commit_crop_spawn(work, true, ordinal, 17);
            const original::CropSpawnInitialization expected{
                static_cast<std::uint8_t>(work == original::CropWork::Tending ? 6 : 10),
                static_cast<std::uint8_t>(work == original::CropWork::Tending ? 1 : 2),
                static_cast<std::int16_t>(ordinal * 10)};
            check(result == original::CropSpawnCommit{18, expected},
                  "only actual success establishes raw state, role, stagger and one counter increment");
        }
        refuses<std::overflow_error>([&] { (void)original::commit_crop_spawn(work, true, 0, maximum); });
        for (const auto ordinal : {-1, 2000, maximum})
            refuses<std::out_of_range>([&] { (void)original::commit_crop_spawn(work, false, ordinal, 0); });
        refuses<std::out_of_range>([&] { (void)original::commit_crop_spawn(work, false, 0, -1); });
    }
    auto facts = ready(fields[0]);
    facts.prepared_distance = maximum;
    facts.quantity = 10000;
    check(original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts).score == maximum,
          "last safe signed-DWORD harvest score");
    facts.quantity = 9800;
    refuses<std::overflow_error>([&] { (void)original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts); });
    facts.quantity = 0;
    check(!original::evaluate_crop_initial_candidate(original::CropWork::Harvest, facts).eligible,
          "unreached score arithmetic cannot impose an overflow gate");
    facts.work_word = 0;
    facts.fertility_query = 100;
    check(original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts).score == maximum,
          "last safe signed-DWORD tending score");
    facts.fertility_query = 99;
    refuses<std::overflow_error>([&] { (void)original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts); });
    for (unsigned invalid = 0; invalid < 5; ++invalid) {
        facts = ready(fields[0]); facts.status = 2;
        if (invalid == 0) facts.quantity = 10001;
        if (invalid == 1) facts.prepared_distance = -1;
        if (invalid == 2) facts.fertility_query = -1;
        if (invalid == 3) facts.fertility_query = 101;
        if (invalid == 4) facts.field = original::ModelBuilding::HempField;
        refuses<std::out_of_range>([&] { (void)original::evaluate_crop_initial_candidate(original::CropWork::Tending, facts); });
    }
    const auto invalid_work = static_cast<original::CropWork>(255);
    refuses<std::out_of_range>([&] { (void)original::evaluate_crop_initial_candidate(invalid_work, ready(fields[0])); });
    refuses<std::out_of_range>([&] { (void)original::commit_crop_spawn(invalid_work, false, 0, 0); });
}
} // namespace

int main() {
    try {
        candidate_gates_and_side_effects();
        literal_queue_order();
        spawn_commit_and_refusals();
        std::cout << "Original crop selection PASS: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
