#include "simulation/original/CropRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace original = openemperor::original;
using original::CropRule;
using original::CropWaitStep;
using original::CropWork;
using original::ModelBuilding;

std::size_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<class Operation>
void rejected(Operation operation) {
    try { operation(); }
    catch (const std::out_of_range&) { ++checks; return; }
    throw std::runtime_error("unsupported original crop input was accepted");
}

void identities_and_calendar() {
    constexpr std::array expected{
        CropRule{ModelBuilding::WheatField, 0, 7, 2, 6},
        CropRule{ModelBuilding::MilletField, 1, 5, 6, 10},
        CropRule{ModelBuilding::RiceField, 2, 6, 5, 9},
        CropRule{ModelBuilding::CabbageField, 3, 3, 7, 11},
        CropRule{ModelBuilding::SoybeanField, 4, 1, 4, 8}
    };
    // Literal slot masks bind the separate period query. They deliberately
    // differ from the callback's sole end-slot suppression below.
    constexpr std::array<std::uint16_t, 5> growing_slots{0x03c, 0x3c0, 0x1e0, 0x780, 0x0f0};
    constexpr std::array<std::uint16_t, 5> growth_callback_slots{0xfbf, 0xbff, 0xdff, 0x7ff, 0xeff};
    for (std::size_t crop = 0; crop < expected.size(); ++crop) {
        check(original::crop_rule(expected[crop].field) == expected[crop],
              "Food field identity, raw resource or calendar slots changed");
        for (std::uint8_t slot = 0; slot < 12; ++slot) {
            check(original::crop_in_growing_period(expected[crop].field, slot) ==
                      ((growing_slots[crop] & (1U << slot)) != 0),
                  "Original growing-period boundary changed");
            check(original::crop_growth_increment(expected[crop].field, slot, 0x40) ==
                      ((growth_callback_slots[crop] & (1U << slot)) != 0 ? 157 : 0),
                  "Growth callback acquired an inferred four-slot period gate");
        }
    }
    check(original::crop_rule(ModelBuilding::SoybeanField).resource_id == 1,
          "Soybean output lost the actual BeanCurd resource identity");
    for (const auto model : {ModelBuilding::Farmhouse, ModelBuilding::HempField,
                            ModelBuilding::TeaBush, static_cast<ModelBuilding>(65535)}) {
        rejected([&] { (void)original::crop_rule(model); });
        rejected([&] { (void)original::crop_in_growing_period(model, 0); });
        rejected([&] { (void)original::crop_growth_increment(model, 0, 0); });
    }
    for (const auto slot : {std::uint8_t{12}, std::uint8_t{255}}) {
        rejected([&] { (void)original::crop_in_growing_period(ModelBuilding::RiceField, slot); });
        rejected([&] { (void)original::crop_growth_increment(ModelBuilding::RiceField, slot, 64); });
    }
}

void work_and_quantity() {
    struct GrowthCase { std::uint16_t work, increment; };
    constexpr std::array growth_cases{
        GrowthCase{0x0000, 0}, GrowthCase{0x8000, 0}, GrowthCase{0xff00, 0},
        GrowthCase{0x0001, 79}, GrowthCase{0x0020, 79}, GrowthCase{0x8020, 79},
        GrowthCase{0x0021, 157}, GrowthCase{0x0040, 157}, GrowthCase{0x00ff, 157},
        GrowthCase{0xffff, 157}
    };
    for (const auto& item : growth_cases) {
        check(original::crop_growth_increment(ModelBuilding::RiceField, 0, item.work) == item.increment,
              "Food growth increment did not use the exact low work byte");
        check(original::crop_growth_increment(ModelBuilding::RiceField, 9, item.work) == 0,
              "End-slot growth was allowed by a work or harvest flag");
    }

    struct WorkCase { std::uint16_t before, tended, decayed; };
    constexpr std::array work_cases{
        WorkCase{0x0000, 0x0040, 0x0000}, WorkCase{0x0001, 0x0041, 0x0000},
        WorkCase{0x0040, 0x0040, 0x003f}, WorkCase{0x8001, 0x8041, 0x8000},
        WorkCase{0x4002, 0x4042, 0x4001}, WorkCase{0xffff, 0xffff, 0xc0fe},
        WorkCase{0x3f21, 0x3f61, 0x0020}, WorkCase{0xff00, 0xff40, 0xc000}
    };
    for (const auto& item : work_cases) {
        check(original::complete_crop_tending(item.before) == item.tended,
              "Tending completion failed to OR only the low work flag");
        check(original::decay_crop_work(item.before) == item.decayed,
              "Decay lost retained flags or preserved unsupported high bits");
    }
    auto work = original::complete_crop_tending(0x8000);
    check(work == 0x8040, "Tending changed the harvest marker");
    for (int call = 0; call < 32; ++call) work = original::decay_crop_work(work);
    check(work == 0x8020 && original::crop_growth_increment(ModelBuilding::RiceField, 0, work) == 79,
          "Actual work decay did not cross the full/half increment boundary");
    for (int call = 0; call < 32; ++call) work = original::decay_crop_work(work);
    check(work == 0x8000 && original::crop_growth_increment(ModelBuilding::RiceField, 0, work) == 0,
          "Exhausted tending work continued generating quantity");

    struct QuantityCase { std::uint16_t before, increment, after; };
    constexpr std::array quantity_cases{
        QuantityCase{0, 0, 0}, QuantityCase{0, 79, 79}, QuantityCase{0, 157, 157},
        QuantityCase{9842, 157, 9999}, QuantityCase{9843, 157, 10000},
        QuantityCase{9999, 79, 10000}, QuantityCase{10000, 157, 10000}
    };
    for (const auto& item : quantity_cases)
        check(original::apply_crop_growth(item.before, item.increment) == item.after,
              "Bounded field quantity addition or 10000 cap changed");
    rejected([] { (void)original::apply_crop_growth(10001, 0); });
    rejected([] { (void)original::apply_crop_growth(65535, 157); });
    rejected([] { (void)original::apply_crop_growth(0, 1); });
    rejected([] { (void)original::apply_crop_growth(0, 158); });
}

void raw_yield_and_runtime_fertility() {
    struct BaseCase { std::uint16_t quantity, ordinary, doubled; };
    constexpr std::array base_cases{
        BaseCase{0, 0, 0}, BaseCase{1, 0, 0}, BaseCase{49, 0, 0},
        BaseCase{50, 0, 1}, BaseCase{99, 0, 1}, BaseCase{100, 1, 2},
        BaseCase{149, 1, 2}, BaseCase{199, 1, 3},
        BaseCase{9999, 99, 199}, BaseCase{10000, 100, 200}
    };
    for (const auto& item : base_cases) {
        check(original::raw_base_yield(item.quantity, false) == item.ordinary,
              "Ordinary raw base yield lost its integer truncation boundary");
        check(original::raw_base_yield(item.quantity, true) == item.doubled,
              "Resolved doubled-base event changed the raw integer stage");
    }
    for (const auto quantity : {std::uint16_t{10001}, std::uint16_t{65535}}) {
        rejected([&] { (void)original::raw_base_yield(quantity, false); });
        rejected([&] { (void)original::raw_base_yield(quantity, true); });
    }

    struct FertilityCase { std::uint8_t serialized, modifier, runtime; };
    constexpr std::array fertility_cases{
        FertilityCase{0, 0, 0}, FertilityCase{1, 10, 10},
        FertilityCase{2, 10, 11}, FertilityCase{99, 10, 64},
        FertilityCase{100, 10, 65}, FertilityCase{128, 17, 87},
        FertilityCase{255, 0, 140}, FertilityCase{100, 255, 54},
        FertilityCase{200, 200, 54}, FertilityCase{255, 255, 139}
    };
    for (const auto& item : fertility_cases)
        check(original::runtime_fertility(item.serialized, item.modifier) == item.runtime,
              "Runtime fertility lost its serialized-byte transform or wrapping global modifier");
}

void actor_waits() {
    for (const auto model : {ModelBuilding::WheatField, ModelBuilding::MilletField,
                            ModelBuilding::RiceField, ModelBuilding::CabbageField,
                            ModelBuilding::SoybeanField}) {
        check(original::initial_crop_work_wait(model, CropWork::Tending, true, true, false) == 51,
              "Food tending wait lost its source operand");
        check(original::initial_crop_work_wait(model, CropWork::Harvest, true, true, false) == 25,
              "Food harvest wait did not truncate 0.5 times 51 toward zero");
        for (const auto branch : {CropWork::Tending, CropWork::Harvest}) {
            check(original::initial_crop_work_wait(model, branch, false, true, false) == 0,
                  "Rejected field still acquired a work wait");
            check(original::initial_crop_work_wait(model, branch, true, false, false) == 0,
                  "Zero retained field-body word still acquired a work wait");
            check(original::initial_crop_work_wait(model, branch, true, true, true) == 0,
                  "Terrain flag 0x100 failed to override the initialized wait");
        }
    }
    rejected([] { (void)original::initial_crop_work_wait(
        ModelBuilding::TeaBush, CropWork::Tending, false, false, true); });
    rejected([] { (void)original::initial_crop_work_wait(
        ModelBuilding::RiceField, static_cast<CropWork>(255), false, false, true); });

    constexpr std::array wait_cases{
        std::pair{std::int16_t{51}, CropWaitStep{50, false}},
        std::pair{std::int16_t{25}, CropWaitStep{24, false}},
        std::pair{std::int16_t{2}, CropWaitStep{1, false}},
        std::pair{std::int16_t{1}, CropWaitStep{0, true}},
        std::pair{std::int16_t{0}, CropWaitStep{-1, true}},
        std::pair{std::int16_t{-1}, CropWaitStep{-2, true}},
        std::pair{std::numeric_limits<std::int16_t>::min(), CropWaitStep{32767, false}}
    };
    for (const auto& [before, expected] : wait_cases)
        check(original::advance_crop_work_wait(before) == expected,
              "Signed WORD wait decrement or callback gate changed");
    auto wait = std::int16_t{51};
    for (int call = 1; call <= 51; ++call) {
        const auto step = original::advance_crop_work_wait(wait);
        check(step.callback_due == (call == 51), "Tending became due at the wrong eligible actor call");
        wait = step.remaining;
    }
}
} // namespace

int main() {
    try {
        identities_and_calendar();
        work_and_quantity();
        raw_yield_and_runtime_fertility();
        actor_waits();
        std::cout << "Original crop identities, callback arithmetic, work decay and eligible-call waits: "
                  << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original crop rules failed: " << error.what() << '\n';
        return 1;
    }
}
