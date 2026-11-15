#include "simulation/original/SeasonRules.h"

#include "simulation/original/CropRules.h"
#include "simulation/original/OriginalScheduler.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace original = openemperor::original;
using original::ModelBuilding;
using original::SeasonSuppression;

std::size_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<class Operation>
void rejected(Operation operation) {
    try { operation(); }
    catch (const std::out_of_range&) { ++checks; return; }
    throw std::runtime_error("unsupported seasonal input was accepted");
}

struct Expected {
    ModelBuilding plant;
    std::uint8_t index;
    std::uint8_t resource;
    std::string_view growing;
    std::string_view suppressed;
    std::uint16_t high;
    std::uint16_t low;
};

// Independently enumerated raw slot masks0..11, including season gaps and
// unmatched Lacquer end7. They do not derive expected results from descriptors.
constexpr std::array expected{
    Expected{ModelBuilding::WheatField, 0, 7, "001111000000", "000000100000", 157, 79},
    Expected{ModelBuilding::MilletField, 1, 5, "000000111100", "000000000010", 157, 79},
    Expected{ModelBuilding::RiceField, 2, 6, "000001111000", "000000000100", 157, 79},
    Expected{ModelBuilding::CabbageField, 3, 3, "000000011110", "000000000001", 157, 79},
    Expected{ModelBuilding::SoybeanField, 4, 1, "000011110000", "000000001000", 157, 79},
    Expected{ModelBuilding::HempField, 5, 19, "000111110000", "000000000000", 125, 63},
    Expected{ModelBuilding::TeaBush, 6, 13, "001101101100", "000000000010", 105, 53},
    Expected{ModelBuilding::LacquerTree, 7, 14, "011111000000", "000000110000", 125, 63},
    Expected{ModelBuilding::MulberryTree, 8, 12, "000110110000", "000000001000", 157, 79}
};

void pinned_configuration_descriptors() {
    for (const auto& item : expected) {
        const auto& rule = original::season_rule(item.plant);
        check(rule.plant == item.plant && rule.crop_index == item.index &&
                  rule.resource_id == item.resource && rule.high_work_increment == item.high &&
                  rule.low_work_increment == item.low,
              "Season descriptor changed its source identity or raw callback increments");
        check(rule.start_count <= 4 && rule.end_count <= 4 &&
                  rule.start_count > 0 && rule.end_count > 0,
              "Season descriptor exceeded the four-entry source table bound");
        for (std::size_t index = 0; index < 4; ++index) {
            if (index < rule.start_count)
                check(rule.start_slots[index] >= 0 && rule.start_slots[index] < 12,
                      "Configured start slot left the raw slot domain");
            else check(rule.start_slots[index] == -1, "Unused start lost its source sentinel");
            if (index < rule.end_count)
                check(rule.end_slots[index] >= 0 && rule.end_slots[index] < 12,
                      "Configured end slot left the raw slot domain");
            else check(rule.end_slots[index] == -1, "Unused end lost its source sentinel");
        }
    }
    const auto& hemp = original::season_rule(ModelBuilding::HempField);
    check(hemp.start_slots == std::array<std::int8_t, 4>{3, -1, -1, -1} &&
              hemp.end_slots == std::array<std::int8_t, 4>{8, -1, -1, -1} &&
              hemp.suppression == SeasonSuppression::None,
          "Hemp acquired a calendar veto absent from its concrete callback");
    const auto& tea = original::season_rule(ModelBuilding::TeaBush);
    check(tea.start_slots == std::array<std::int8_t, 4>{2, 5, 8, -1} &&
              tea.end_slots == std::array<std::int8_t, 4>{4, 7, 10, -1} &&
              tea.start_count == 3 && tea.end_count == 3 &&
              tea.suppression == SeasonSuppression::LastEndSlot,
          "Tea lost a configured period or gained an earlier-end veto");
    const auto& lacquer = original::season_rule(ModelBuilding::LacquerTree);
    check(lacquer.start_slots == std::array<std::int8_t, 4>{1, -1, -1, -1} &&
              lacquer.end_slots == std::array<std::int8_t, 4>{6, 7, -1, -1} &&
              lacquer.start_count == 1 && lacquer.end_count == 2 &&
              lacquer.suppression == SeasonSuppression::AnyEndSlot,
          "Lacquer's unmatched end was converted into a second growth interval");
    const auto& mulberry = original::season_rule(ModelBuilding::MulberryTree);
    check(mulberry.start_slots == std::array<std::int8_t, 4>{3, 6, -1, -1} &&
              mulberry.end_slots == std::array<std::int8_t, 4>{5, 8, -1, -1} &&
              mulberry.start_count == 2 && mulberry.end_count == 2 &&
              mulberry.suppression == SeasonSuppression::LastEndSlot,
          "Mulberry changed its source periods or suppression predicate");
}

void raw_slot_and_work_boundaries() {
    constexpr std::array<std::uint16_t, 6> high_words{0, 0x100, 0x3f00, 0x4000, 0x8000, 0xff00};
    for (const auto& item : expected) {
        for (std::uint8_t slot = 0; slot < 12; ++slot) {
            check(original::season_in_growing_period(item.plant, slot) == (item.growing[slot] == '1'),
                  "Growing-period query changed start inclusion, end exclusion or season gaps");
            const bool suppressed = item.suppressed[slot] == '1';
            check(original::season_growth_suppressed(item.plant, slot) == suppressed,
                  "Quantity callback suppression was conflated with a growing period");
            check(original::season_growth_increment(item.plant, slot, 0) == 0,
                  "A field without actual low-byte work produced a quantity increment");
            for (const auto prefix : high_words) {
                check(original::season_growth_increment(item.plant, slot, prefix) == 0,
                      "High work flags or unrelated retained bits granted quantity work");
                for (const auto low : {std::uint16_t{1}, std::uint16_t{32}})
                    check(original::season_growth_increment(item.plant, slot, prefix | low) ==
                              (suppressed ? 0 : item.low),
                          "Low work threshold changed a concrete quantity callback");
                for (const auto low : {std::uint16_t{33}, std::uint16_t{255}})
                    check(original::season_growth_increment(item.plant, slot, prefix | low) ==
                              (suppressed ? 0 : item.high),
                          "High work threshold changed a concrete quantity callback");
            }
        }
    }
    check(!original::season_in_growing_period(ModelBuilding::HempField, 8) &&
              original::season_growth_increment(ModelBuilding::HempField, 8, 64) == 125,
          "Hemp's separate growing interval incorrectly gated its raw quantity callback");
    for (const auto slot : {std::uint8_t{4}, std::uint8_t{7}})
        check(!original::season_in_growing_period(ModelBuilding::TeaBush, slot) &&
                  original::season_growth_increment(ModelBuilding::TeaBush, slot, 64) == 105,
              "Tea's earlier end slots were incorrectly treated as quantity suppression");
    check(!original::season_in_growing_period(ModelBuilding::MulberryTree, 5) &&
              original::season_growth_increment(ModelBuilding::MulberryTree, 5, 64) == 157,
          "Mulberry's first end incorrectly suppressed its raw quantity callback");
}

void historical_food_arithmetic_delegation() {
    for (const auto& item : expected) {
        if (item.index >= 5) continue;
        for (std::uint8_t slot = 0; slot < 12; ++slot) {
            check(original::season_in_growing_period(item.plant, slot) ==
                      original::crop_in_growing_period(item.plant, slot),
                  "Season wrapper changed the historical Food interval query");
            for (unsigned work = 0; work <= std::numeric_limits<std::uint16_t>::max(); ++work)
                check(original::season_growth_increment(item.plant, slot, static_cast<std::uint16_t>(work)) ==
                          original::crop_growth_increment(item.plant, slot, static_cast<std::uint16_t>(work)),
                      "Season wrapper changed historical Food raw-work arithmetic");
        }
    }
}

void complete_shared_calendar_slot_cycles() {
    constexpr std::uint64_t year_ticks = original::scheduler_phase_count *
        original::scheduler_steps_per_month * original::scheduler_months_per_year;
    std::array<std::array<unsigned, 12>, 9> visits{};
    for (std::uint64_t tick = 0; tick < year_ticks * 3; ++tick) {
        const auto position = original::scheduler_position(tick);
        for (std::size_t index = 0; index < expected.size(); ++index) {
            const auto& item = expected[index];
            check(original::season_in_growing_period(item.plant, position.month) ==
                      (item.growing[position.month] == '1'),
                  "A year rollover changed raw growing-period query results");
            ++visits[index][position.month];
        }
    }
    for (const auto& by_plant : visits)
        for (const auto count : by_plant)
            check(count == 3 * original::scheduler_phase_count * original::scheduler_steps_per_month,
                  "The shared scheduler omitted or duplicated a raw slot across year boundaries");
}

void unsupported_inputs() {
    for (const auto& item : expected)
        for (const auto slot : {std::uint8_t{12}, std::uint8_t{255}}) {
            rejected([&] { (void)original::season_in_growing_period(item.plant, slot); });
            rejected([&] { (void)original::season_growth_suppressed(item.plant, slot); });
            rejected([&] { (void)original::season_growth_increment(item.plant, slot, 64); });
        }
    for (const auto model : {0U, 29U, 192U, 193U, 200U, 237U, 65535U}) {
        const auto plant = static_cast<ModelBuilding>(model);
        rejected([&] { (void)original::season_rule(plant); });
        rejected([&] { (void)original::season_in_growing_period(plant, 0); });
        rejected([&] { (void)original::season_growth_suppressed(plant, 0); });
        rejected([&] { (void)original::season_growth_increment(plant, 0, 64); });
    }
}

} // namespace

int main() {
    try {
        pinned_configuration_descriptors();
        raw_slot_and_work_boundaries();
        historical_food_arithmetic_delegation();
        complete_shared_calendar_slot_cycles();
        unsupported_inputs();
        std::cout << "Original season reference rules " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
