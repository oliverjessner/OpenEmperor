#include "simulation/original/HarvestRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace original = openemperor::original;
using original::BoatCounterStep;
using original::BoatWorkStage;
using original::HarvestProducer;
using original::HarvestResource;

std::size_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<class Error, class Operation>
void rejected(Operation operation) {
    try { operation(); }
    catch (const Error&) { ++checks; return; }
    throw std::runtime_error("unsupported harvest input was accepted");
}

void source_identities() {
    const auto& fish = original::harvest_rule(HarvestProducer::FishingQuay);
    const auto& meat = original::harvest_rule(HarvestProducer::HuntersTent);
    check(fish.model == original::ModelBuilding::FishingQuay &&
              fish.resource == HarvestResource::Fish && fish.source_point_model_id == 179 &&
              fish.worker_actor == 49 && fish.cart_actor == 19 && fish.initial_side == 2 &&
              fish.raw_return_amount == 100 && fish.ordinary_raw_cart_amount == 100 &&
              !fish.raw_source_capacity && fish.receipt == original::HarvestReceipt::Assign,
          "Fishing descriptor lost an observed identity or supplied an unproved capacity");
    check(meat.model == original::ModelBuilding::HuntersTent &&
              meat.resource == HarvestResource::Meat && meat.source_point_model_id == 183 &&
              meat.worker_actor == 40 && meat.cart_actor == 19 && meat.initial_side == 2 &&
              meat.raw_return_amount == 50 && meat.ordinary_raw_cart_amount == 100 &&
              meat.raw_source_capacity == 400 && meat.receipt == original::HarvestReceipt::Add,
          "Hunter descriptor changed its typed source, actor, resource or raw amount");
    check(original::harvest_resource_name(HarvestResource::Fish) == "Fish" &&
              original::harvest_resource_name(HarvestResource::Meat) == "Meat",
          "Harvest resources changed their verified commodity names");
    for (unsigned actor = 0; actor <= 255; ++actor) {
        const bool expected = actor == 75 || actor == 76 || actor == 77;
        check(original::huntable_actor(static_cast<std::uint8_t>(actor)) == expected,
              "A predator or unsupported actor became huntable");
    }
    for (const auto value : {0U, 29U, 32U, 35U, 65535U}) {
        const auto producer = static_cast<HarvestProducer>(value);
        rejected<std::out_of_range>([&] { (void)original::harvest_rule(producer); });
        rejected<std::out_of_range>([&] { (void)original::harvest_source_return(producer, 0); });
        rejected<std::out_of_range>([&] {
            (void)original::ordinary_harvest_cart_debit(producer, 100, 0);
        });
    }
    for (const auto value : {0U, 1U, 3U, 255U})
        rejected<std::out_of_range>([&] {
            (void)original::harvest_resource_name(static_cast<HarvestResource>(value));
        });
}

void conditional_initial_prey_selection() {
    using original::PreyActor;
    check(original::initial_prey_attempts_per_eligible_point == 7,
          "Initial source-point budget changed from seven attempts");
    check(static_cast<std::uint8_t>(PreyActor::WildPig) == 75 &&
              static_cast<std::uint8_t>(PreyActor::Pheasant) == 76 &&
              static_cast<std::uint8_t>(PreyActor::Antelope) == 77,
          "Typed prey identities changed");
    check(original::initial_prey_actor(0) == PreyActor::Antelope &&
              original::initial_prey_actor(1) == PreyActor::Pheasant,
          "Initial prey selector lost its two exceptional raw values");
    std::array<unsigned, 3> selected{};
    for (unsigned raw = 0; raw <= std::numeric_limits<std::uint16_t>::max(); ++raw) {
        const auto actor = original::initial_prey_actor(static_cast<std::uint16_t>(raw));
        const auto identity = static_cast<std::uint8_t>(actor);
        check(original::huntable_actor(identity),
              "An initial raw selector admitted a predator or an unsupported actor");
        ++selected.at(identity - 75);
        if (raw >= 2)
            check(actor == PreyActor::WildPig,
                  "A nonexceptional raw WORD selector changed the initial prey family");
    }
    check(selected == std::array<unsigned, 3>{65534, 1, 1},
          "Initial prey selection did not cover the complete raw WORD domain");
    for (const auto raw : {std::uint16_t{2}, std::uint16_t{32767},
                           std::uint16_t{32768}, std::uint16_t{65535}})
        check(original::initial_prey_actor(raw) == PreyActor::WildPig,
              "Initial prey selection reinterpreted signed WORD boundary bits");
    check(original::prey_actor_name(PreyActor::WildPig) == "FIG_WILD_PIG" &&
              original::prey_actor_name(PreyActor::Pheasant) == "FIG_PHEASANT" &&
              original::prey_actor_name(PreyActor::Antelope) == "FIG_ANTELOPE",
          "Prey actor names changed their verified source identifiers");
    for (const auto value : {0U, 69U, 74U, 78U, 255U})
        rejected<std::out_of_range>([&] {
            (void)original::prey_actor_name(static_cast<PreyActor>(value));
        });
}

void hunter_staffing_and_headroom() {
    struct Case { std::uint8_t staff, threshold, limit; };
    constexpr std::array cases{
        Case{0, 0, 0}, Case{1, 64, 0}, Case{24, 64, 0}, Case{25, 32, 1},
        Case{49, 32, 1}, Case{50, 16, 1}, Case{74, 16, 1}, Case{75, 4, 2},
        Case{99, 4, 2}, Case{100, 1, 3}
    };
    for (const auto& item : cases) {
        const auto rule = original::hunter_dispatch_rule(item.staff, 0);
        check(rule.staffed == (item.staff != 0) && rule.attempt_threshold == item.threshold &&
                  rule.hunter_limit == item.limit,
              "Hunter staffing tier changed its throttle or active quota");
        auto counter = std::uint8_t{0};
        if (!item.staff) {
            check(original::advance_hunter_attempt(255, 0, 0) ==
                      original::HunterAttemptStep{255, false},
                  "Unstaffed Hunter source advanced its throttle");
            continue;
        }
        for (unsigned invocation = 1; invocation <= item.threshold; ++invocation) {
            const auto step = original::advance_hunter_attempt(counter, item.staff, 0);
            check(!step.attempt_due && step.counter == invocation,
                  "Hunter attempt fired before the strict post-increment threshold");
            counter = step.counter;
        }
        check(original::advance_hunter_attempt(counter, item.staff, 0) ==
                  original::HunterAttemptStep{0, true},
              "Hunter due attempt failed to reset its source counter");
        check(original::advance_hunter_attempt(255, item.staff, 0) ==
                  original::HunterAttemptStep{0, false},
              "Hunter byte-counter wrap was widened into an immediate due attempt");
    }

    struct StockCase { std::uint16_t stock; std::uint8_t limit; };
    constexpr std::array headroom{
        StockCase{0, 3}, StockCase{99, 3}, StockCase{100, 3}, StockCase{101, 2},
        StockCase{200, 2}, StockCase{201, 1}, StockCase{299, 1}, StockCase{300, 1},
        StockCase{301, 0}, StockCase{350, 0}, StockCase{399, 0}, StockCase{400, 0}
    };
    for (const auto& item : headroom)
        check(original::hunter_dispatch_rule(100, item.stock).hunter_limit == item.limit,
              "Hunter quota failed the source capacity/headroom boundary");
    check(original::hunter_dispatch_rule(75, 0).hunter_limit == 2 &&
              original::hunter_dispatch_rule(50, 0).hunter_limit == 1 &&
              original::hunter_dispatch_rule(24, 0).hunter_limit == 0,
          "Headroom created extra Hunter capacity above the staffing quota");
    check(original::advance_hunter_attempt(64, 1, 400).attempt_due &&
              original::hunter_dispatch_rule(1, 400).hunter_limit == 0,
          "Source throttling was incorrectly equated with a successful Hunter spawn");

    for (const auto staff : {std::uint8_t{101}, std::uint8_t{255}}) {
        rejected<std::out_of_range>([&] { (void)original::hunter_dispatch_rule(staff, 0); });
        rejected<std::out_of_range>([&] { (void)original::advance_hunter_attempt(10, staff, 0); });
    }
    for (const auto stock : {std::uint16_t{401}, std::uint16_t{32768}, std::uint16_t{65535}}) {
        rejected<std::out_of_range>([&] { (void)original::hunter_dispatch_rule(100, stock); });
        rejected<std::out_of_range>([&] { (void)original::advance_hunter_attempt(10, 100, stock); });
    }
}

void boat_work_invocations() {
    struct Case { std::uint8_t staff; std::uint16_t fishing_calls, dock_calls; };
    constexpr std::array cases{
        Case{1, 2040, 1021}, Case{24, 2040, 1021}, Case{25, 1632, 817},
        Case{49, 1632, 817}, Case{50, 1224, 613}, Case{74, 1224, 613},
        Case{75, 816, 409}, Case{99, 816, 409}, Case{100, 408, 205}
    };
    for (const auto& item : cases) {
        check(original::boat_work_threshold(BoatWorkStage::Fishing, item.staff) == item.fishing_calls &&
                  original::boat_work_threshold(BoatWorkStage::Dock, item.staff) == item.dock_calls - 1,
              "Boat staff thresholds lost their separately observed work stages");
        for (const auto stage : {BoatWorkStage::Fishing, BoatWorkStage::Dock}) {
            const auto calls = stage == BoatWorkStage::Fishing ? item.fishing_calls : item.dock_calls;
            auto counter = std::int16_t{0};
            for (std::uint16_t invocation = 1; invocation < calls; ++invocation) {
                const auto step = original::advance_boat_work_counter(stage, counter, item.staff);
                check(!step.transition_due && step.counter == invocation,
                      "Boat work completed before its source comparison admitted it");
                counter = step.counter;
            }
            check(original::advance_boat_work_counter(stage, counter, item.staff) ==
                      BoatCounterStep{0, true},
                  "Boat completion count or reset changed");
        }
    }
    check(original::advance_boat_work_counter(BoatWorkStage::Fishing, 407, 100) ==
              BoatCounterStep{0, true},
          "Fishing failed to compare the incremented counter");
    check(original::advance_boat_work_counter(BoatWorkStage::Dock, 203, 100) ==
              BoatCounterStep{204, false} &&
              original::advance_boat_work_counter(BoatWorkStage::Dock, 204, 100) ==
              BoatCounterStep{0, true},
          "Dock work changed its old-counter comparison into an off-by-one early receipt");
    for (const auto stage : {BoatWorkStage::Fishing, BoatWorkStage::Dock}) {
        check(!original::boat_work_threshold(stage, 0) &&
                  original::advance_boat_work_counter(stage, 407, 0) == BoatCounterStep{407, false},
              "Zero staffing advanced or completed the work counter");
        check(original::advance_boat_work_counter(stage, -1, 100) == BoatCounterStep{0, false},
              "Negative raw counter lost its signed-WORD interpretation");
    }
    constexpr auto maximum = std::numeric_limits<std::int16_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int16_t>::min();
    check(original::advance_boat_work_counter(BoatWorkStage::Fishing, maximum, 100) ==
              BoatCounterStep{minimum, false},
          "Fishing signed-WORD wrap became an immediate completion");
    check(original::advance_boat_work_counter(BoatWorkStage::Dock, maximum, 100) ==
              BoatCounterStep{0, true},
          "Dock failed to use its old positive counter before wrapping/resetting");
    check(original::advance_boat_work_counter(BoatWorkStage::Fishing, minimum, 100) ==
              BoatCounterStep{static_cast<std::int16_t>(minimum + 1), false},
          "Minimum raw signed counter did not advance safely");
    for (const auto value : {0U, 7U, 9U, 11U, 255U}) {
        const auto stage = static_cast<BoatWorkStage>(value);
        rejected<std::out_of_range>([&] { (void)original::boat_work_threshold(stage, 0); });
        rejected<std::out_of_range>([&] { (void)original::advance_boat_work_counter(stage, 0, 100); });
    }
    for (const auto staff : {std::uint8_t{101}, std::uint8_t{255}})
        rejected<std::out_of_range>([&] { (void)original::boat_work_threshold(BoatWorkStage::Fishing, staff); });
}

void boat_search_invocations() {
    auto counter = std::int16_t{0};
    for (std::int16_t invocation = 1; invocation <= 50; ++invocation) {
        const auto step = original::advance_boat_search_counter(counter);
        check(step == BoatCounterStep{invocation, false},
              "Boat search attempt fired before its51st eligible invocation");
        counter = step.counter;
    }
    check(original::advance_boat_search_counter(counter) == BoatCounterStep{0, true},
          "Boat due point-search attempt lost its counter reset");
    check(original::advance_boat_search_counter(-1) == BoatCounterStep{0, false},
          "Boat negative search counter changed interpretation");
    check(original::advance_boat_search_counter(std::numeric_limits<std::int16_t>::max()) ==
              BoatCounterStep{std::numeric_limits<std::int16_t>::min(), false},
          "Boat search counter overflow was not an explicit signed-WORD wrap");
}

void receipts_and_ordinary_cart_debits() {
    check(original::harvest_source_return(HarvestProducer::HuntersTent, 0) ==
              original::HarvestStockStep{50, 0} &&
              original::harvest_source_return(HarvestProducer::HuntersTent, 349) ==
              original::HarvestStockStep{399, 0} &&
              original::harvest_source_return(HarvestProducer::HuntersTent, 350) ==
              original::HarvestStockStep{400, 0},
          "Hunter actual-return addition or capacity boundary changed");
    for (const auto stock : {std::uint16_t{351}, std::uint16_t{399}, std::uint16_t{400}})
        rejected<std::overflow_error>([&] { (void)original::harvest_source_return(HarvestProducer::HuntersTent, stock); });
    for (const auto stock : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{50},
                             std::uint16_t{100}, std::uint16_t{400}, std::uint16_t{1000},
                             std::uint16_t{32767}})
        check(original::harvest_source_return(HarvestProducer::FishingQuay, stock) ==
                  original::HarvestStockStep{100, stock},
              "Quay return became an addition or concealed overwritten source stock");
    for (const auto producer : {HarvestProducer::FishingQuay, HarvestProducer::HuntersTent}) {
        const auto rule = original::harvest_rule(producer);
        const auto bound = rule.raw_source_capacity.value_or(32767);
        for (std::uint16_t stock = 100; stock <= bound; ++stock) {
            const auto step = original::ordinary_harvest_cart_debit(producer, stock, 0);
            check(step.cargo == 100 && static_cast<unsigned>(step.stock) + step.cargo == stock,
                  "Ordinary successful cart debit changed raw resource conservation");
        }
        for (const auto stock : {std::uint16_t{0}, std::uint16_t{50}, std::uint16_t{99}})
            rejected<std::out_of_range>([&] { (void)original::ordinary_harvest_cart_debit(producer, stock, 0); });
        for (const auto effect : {std::uint8_t{1}, std::uint8_t{255}})
            rejected<std::invalid_argument>([&] { (void)original::ordinary_harvest_cart_debit(producer, 100, effect); });
        for (const auto stock : {std::uint16_t{32768}, std::uint16_t{65535}}) {
            rejected<std::out_of_range>([&] { (void)original::harvest_source_return(producer, stock); });
            rejected<std::out_of_range>([&] { (void)original::ordinary_harvest_cart_debit(producer, stock, 0); });
        }
    }
    rejected<std::out_of_range>([] { (void)original::harvest_source_return(HarvestProducer::HuntersTent, 401); });
}

} // namespace

int main() {
    try {
        source_identities();
        conditional_initial_prey_selection();
        hunter_staffing_and_headroom();
        boat_work_invocations();
        boat_search_invocations();
        receipts_and_ordinary_cart_debits();
        std::cout << "Original harvest identities, raw work counters and stock effects "
                  << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
