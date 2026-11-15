#include "simulation/original/FoodRules.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace original = openemperor::original;
std::size_t checks = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

template<typename Operation>
void refuses(Operation operation, std::string_view message) {
    try { operation(); }
    catch (const std::invalid_argument&) { ++checks; return; }
    throw std::runtime_error(std::string(message));
}

void score_boundaries() {
    struct Range { std::int32_t first, last; std::uint8_t grade; };
    constexpr std::array ranges{
        Range{std::numeric_limits<std::int32_t>::min(), 0, 0},
        Range{1, 29, 1}, Range{30, 49, 2}, Range{50, 69, 3},
        Range{70, 89, 4}, Range{90, std::numeric_limits<std::int32_t>::max(), 5}
    };
    for (const auto& range : ranges) {
        check(original::food_grade_from_raw_score(range.first) == range.grade,
              "raw score lower boundary");
        check(original::food_grade_from_raw_score(range.last) == range.grade,
              "raw score upper boundary");
    }
    for (std::int32_t score = -1000; score <= 1000; ++score) {
        for (const auto& range : ranges) {
            if (score < range.first || score > range.last) continue;
            check(original::food_grade_from_raw_score(score) == range.grade,
                  "raw score remains in its independently listed interval");
        }
    }
}

void actual_event_gates() {
    const original::RawHouseFoodState initial{1, 20, 11, 90};
    for (unsigned status = 0; status <= 255; ++status) {
        auto house = initial;
        house.status = static_cast<std::uint8_t>(status);
        const auto result = original::consume_raw_house_food(house, 1, 0);
        if (status == 1) {
            check(result.house.stock == 6 && result.removed == 5 && result.house.quality == 90,
                  "only exact status1 consumes actual Food");
        } else {
            check(result.house == house && result.removed == 0,
                  "other raw statuses preserve Food and quality");
        }
    }
    for (const auto requirement : {std::numeric_limits<std::int32_t>::min(), -1, 0}) {
        const auto result = original::consume_raw_house_food(initial, requirement, 0);
        check(result.house == initial && result.removed == 0,
              "nonpositive FoodQuality requirement gates consumption");
    }
    const auto low = original::consume_raw_house_food(initial, 1, 0);
    for (const auto requirement : {30, 90, std::numeric_limits<std::int32_t>::max()})
        check(original::consume_raw_house_food(initial, requirement, 0) == low,
              "FoodQuality requirement magnitude never becomes consumed quantity");

    for (unsigned quality = 0; quality <= 255; ++quality) {
        for (std::uint16_t population = 0; population < 4; ++population) {
            const original::RawHouseFoodState empty{1, population, 0, static_cast<std::uint8_t>(quality)};
            const auto result = original::consume_raw_house_food(empty, 1, 0);
            check(result.house.stock == 0 && result.house.quality == 0 && result.removed == 0,
                  "eligible empty stock clears quality even when demand is zero");
            check(original::consume_raw_house_food(empty, 0, 0).house == empty,
                  "requirement gate precedes empty-stock quality reset");
            auto inactive = empty;
            inactive.status = 3;
            check(original::consume_raw_house_food(inactive, 1, 0).house == inactive,
                  "status gate precedes empty-stock quality reset");
            auto stocked = empty;
            stocked.stock = 1;
            check(original::consume_raw_house_food(stocked, 1, 0).house == stocked,
                  "positive stock preserves quality with zero demand");
        }
    }
}

void finite_inventory_and_word_bounds() {
    for (std::uint32_t population = 0; population <= 32767; ++population) {
        const auto demand = population * 25 / 100;
        for (const auto stock : std::array<std::uint16_t, 7>{0, 1, 4, 100, 400, 8191, 32767}) {
            const original::RawHouseFoodState house{1, static_cast<std::uint16_t>(population), stock, 73};
            const auto result = original::consume_raw_house_food(house, 50, 0);
            const auto expected_removed = stock < demand ? stock : demand;
            check(result.removed == expected_removed,
                  "normal event uses independently computed population25percent");
            check(static_cast<std::uint32_t>(result.house.stock) + result.removed == stock,
                  "actual raw Food is conserved through consumption");
            check(result.house.quality == (result.house.stock == 0 ? 0 : 73),
                  "quality clears only after actual Food empties");
            check(result.house.population == house.population && result.house.status == house.status,
                  "consumption cannot change occupants or status");
        }
    }
    for (const auto invalid : {std::uint16_t{32768}, std::uint16_t{65535}}) {
        refuses([&] { (void)original::consume_raw_house_food({1, invalid, 1, 50}, 1, 0); },
                "invalid physical population is refused");
        refuses([&] { (void)original::consume_raw_house_food({1, 1, invalid, 50}, 1, 0); },
                "invalid physical Food stock is refused");
        refuses([&] { (void)original::consume_raw_house_food({3, 0, invalid, 50}, 0, 0); },
                "no-op gates cannot hide invalid physical stock");
    }
    for (const auto bypass : {std::numeric_limits<std::int32_t>::min(), -1, 1,
                              std::numeric_limits<std::int32_t>::max()}) {
        refuses([&] { (void)original::consume_raw_house_food({1, 20, 100, 90}, 50, bypass); },
                "nonzero native bypass is refused without Food or quality grant");
        refuses([&] { (void)original::consume_raw_house_food({3, 0, 0, 90}, 0, bypass); },
                "no-op gates cannot silently accept the native bypass");
    }
}

void recipe_grade_choice() {
    using Selection = original::FoodRecipeGradeSelection;
    check(original::select_food_recipe_grade({0, 0, 0, 0, 0}, 1, 5, 900) == Selection{},
          "empty availability selects no grade or budget");
    check(original::select_food_recipe_grade({50, 300, 0, 0, 0}, 1, 5, 900) == Selection{1, 50},
          "exact request-third equality does not replace first fallback");
    check(original::select_food_recipe_grade({50, 301, 0, 0, 0}, 1, 5, 900) == Selection{2, 301},
          "strictly greater than request-third selects higher grade");
    check(original::select_food_recipe_grade({0, 0, 7, 6, 5}, 1, 5, 900) == Selection{3, 7},
          "first nonzero grade is fallback when all availability is small");
    check(original::select_food_recipe_grade({900, 0, 800, 0, 400}, 1, 5, 900) == Selection{5, 400},
          "highest qualifying grade wins without maximizing quantity");
    check(original::select_food_recipe_grade({1, 0, 2, 0, 3}, 1, 5, 0) == Selection{5, 0},
          "zero request can select positive grade with zero budget");
    check(original::select_food_recipe_grade({1000, 0, 0, 0, 0}, 1, 1, 1000) == Selection{1, 600},
          "selected recipe budget is capped at600 raw units");
    check(original::select_food_recipe_grade({1000, 0, 0, 0, 0}, 1, 1, 99) == Selection{1, 99},
          "raw request can cap selected recipe budget below100");
    check(original::select_food_recipe_grade({1000, 0, 0, 0, 0}, 1, 1, 1001) == Selection{1, 600},
          "raw request above planner normal budget stays bounded");
    constexpr auto largest = std::numeric_limits<std::int32_t>::max();
    check(original::select_food_recipe_grade({largest, largest, largest, largest, largest},
                                             1, 5, largest) == Selection{5, 600},
          "largest valid signed inputs cannot overflow grade selection");

    // Independent oracle: search qualifying grades from high to low; when
    // none qualifies, search the fallback from low to high. This differs from
    // the production ascending replacement algorithm.
    constexpr std::array<std::int32_t, 5> amounts{1, 10, 100, 400, largest};
    for (unsigned present = 0; present < 32; ++present) {
        std::array<std::int32_t, 5> availability{};
        for (unsigned index = 0; index < 5; ++index)
            if ((present & (1U << index)) != 0) availability[index] = amounts[index];
        for (unsigned lower = 1; lower <= 5; ++lower) {
            for (unsigned upper = lower; upper <= 5; ++upper) {
                for (const auto request : {0, 1, 2, 3, 29, 30, 31, 299, 300, 301,
                                           599, 600, 601, 1199, 1200, 1201, largest}) {
                    Selection expected;
                    for (auto grade = upper; grade >= lower; --grade) {
                        if (static_cast<std::int64_t>(availability[grade - 1]) * 3 >
                            static_cast<std::int64_t>(request) - request % 3) {
                            expected.grade = static_cast<std::uint8_t>(grade);
                            break;
                        }
                    }
                    if (expected.grade == 0) {
                        for (auto grade = lower; grade <= upper; ++grade) {
                            if (availability[grade - 1] != 0) {
                                expected.grade = static_cast<std::uint8_t>(grade);
                                break;
                            }
                        }
                    }
                    if (expected.grade != 0) {
                        auto amount = availability[expected.grade - 1];
                        if (amount > request) amount = request;
                        if (amount > 600) amount = 600;
                        expected.raw_budget = static_cast<std::uint16_t>(amount);
                    }
                    check(original::select_food_recipe_grade(availability,
                            static_cast<std::uint8_t>(lower), static_cast<std::uint8_t>(upper),
                            request) == expected, "bounded grade selection matches independent search");
                }
            }
        }
    }
    for (const auto invalid : {std::uint8_t{0}, std::uint8_t{6}, std::uint8_t{255}}) {
        refuses([&] { (void)original::select_food_recipe_grade({}, invalid, 5, 0); },
                "invalid lower grade is refused");
        refuses([&] { (void)original::select_food_recipe_grade({}, 1, invalid, 0); },
                "invalid upper grade is refused");
    }
    refuses([] { (void)original::select_food_recipe_grade({}, 5, 1, 0); },
            "reversed grade range is refused");
    refuses([] { (void)original::select_food_recipe_grade({}, 1, 5, -1); },
            "negative recipe request is refused");
    refuses([] { (void)original::select_food_recipe_grade({}, 1, 5,
                    std::numeric_limits<std::int32_t>::min()); },
            "most-negative recipe request is refused");
    for (unsigned index = 0; index < 5; ++index) {
        std::array<std::int32_t, 5> negative{};
        negative[index] = -1;
        refuses([&] { (void)original::select_food_recipe_grade(negative, 1, 1, 0); },
                "negative availability is refused even outside selected range");
    }
}

void recipe_request_outputs() {
    using Ingredient = original::FoodIngredientRequest;
    const auto expect = [](const std::array<std::int32_t, 9>& stocks,
                           std::uint8_t grade, std::int32_t request,
                           std::initializer_list<Ingredient> ingredients,
                           std::int32_t raw_return) {
        original::FoodRecipeRequests expected;
        expected.ingredients.fill(Ingredient{-1, 0});
        std::size_t index = 0;
        for (const auto ingredient : ingredients) expected.ingredients[index++] = ingredient;
        expected.raw_return_value = raw_return;
        check(original::build_food_recipe_requests(stocks, grade, request) == expected,
              "source-qualified literal ingredient requests and opaque return");
    };
    constexpr std::array<std::int32_t, 9> four_bases{600, 600, 600, 600, 0, 0, 0, 120, 60};
    expect(four_bases, 1, 600, {{1, 600}}, 600);
    expect(four_bases, 2, 600, {{1, 300}, {2, 300}}, 600);
    expect(four_bases, 3, 600, {{1, 200}, {2, 200}, {3, 200}}, 600);
    expect(four_bases, 4, 600, {{1, 100}, {2, 100}, {3, 100}, {4, 100}}, 400);
    expect(four_bases, 5, 600, {{1, 100}, {2, 100}, {3, 100}, {4, 100}, {9, 40}}, 40);
    expect({600, 0, 0, 0, 0, 0, 0, 120, 60}, 2, 600, {{1, 600}, {8, 120}}, 120);
    expect({600, 0, 0, 0, 0, 0, 0, 119, 60}, 2, 600, {{1, 600}, {9, 60}}, 120);
    expect({600, 0, 0, 0, 0, 0, 0, 120, 60}, 3, 600,
           {{1, 600}, {8, 120}, {9, 60}}, 120);
    expect({600, 600, 0, 0, 0, 0, 0, 120, 60}, 4, 600,
           {{1, 300}, {2, 300}, {8, 120}, {9, 60}}, 120);
    expect({600, 600, 600, 0, 0, 0, 0, 120, 60}, 5, 600,
           {{1, 200}, {2, 200}, {3, 200}, {8, 120}, {9, 60}}, 120);
    // No four-base-plus-Salt alternative exists for grade5. Failed supplements
    // leave the last attempted base requests and a positive opaque return.
    expect({600, 600, 600, 600, 0, 0, 0, 120, 0}, 5, 600,
           {{1, 200}, {2, 200}, {3, 200}}, 120);
    expect({}, 3, 99, {{1, 0}}, 0);
    expect({0, 600, 0, 600, 0, 0, 0, 0, 0}, 2, 600,
           {{2, 300}, {4, 300}}, 600);
    expect({0, 200, 0, 0, 0, 0, 0, 0, 0}, 1, 300, {}, 0);
    expect({600, 0, 0, 0, 0, 0, 0, 0, 0}, 2, 600, {{1, 600}}, 120);

    constexpr auto largest = std::numeric_limits<std::int32_t>::max();
    const auto untouched = original::build_food_recipe_requests(
        {0, 0, 0, 0, 0, 0, 0, 0, largest}, 5, 0);
    check(untouched.ingredients == std::array<Ingredient, 6>{} &&
          untouched.raw_return_value == largest,
          "zero request leaves fresh slots untouched and returns raw Spices stock");
    expect({largest, largest, largest, largest, largest, largest, largest, largest, largest},
           5, 600, {{1, 100}, {2, 100}, {3, 100}, {4, 100}, {9, 40}}, 40);

    for (unsigned present = 0; present < 16; ++present) {
        for (const auto salt : {0, 39, 40, 60, 119, 120, largest}) {
            for (const auto spices : {0, 39, 40, 60, 119, 120, largest}) {
                std::array<std::int32_t, 9> stocks{};
                for (unsigned index = 0; index < 4; ++index)
                    if ((present & (1U << index)) != 0) stocks[index] = 600;
                stocks[7] = salt;
                stocks[8] = spices;
                const auto original_stocks = stocks;
                for (unsigned grade = 1; grade <= 5; ++grade) {
                    for (const auto request : {0, 1, 99, 100, 199, 200, 299, 300, 399, 400, 599, 600}) {
                        const auto result = original::build_food_recipe_requests(
                            stocks, static_cast<std::uint8_t>(grade), request);
                        std::array<bool, 10> seen{};
                        std::uint32_t base_total = 0;
                        std::int32_t previous_base = 0;
                        for (const auto& ingredient : result.ingredients) {
                            check(ingredient.resource >= -1 && ingredient.resource <= 9,
                                  "request resource identity is native empty or verified category");
                            if (ingredient.resource <= 0) {
                                check(ingredient.amount == 0, "empty identities cannot carry a request");
                                continue;
                            }
                            const auto resource = static_cast<std::size_t>(ingredient.resource);
                            check(!seen[resource], "recipe request never duplicates an ingredient identity");
                            seen[resource] = true;
                            check(ingredient.amount <= stocks[resource - 1],
                                  "retained ingredient request cannot exceed actual supplied stock");
                            if (resource <= 7) {
                                check(ingredient.amount % 100 == 0 && ingredient.resource > previous_base,
                                      "base requests use whole100-unit blocks in ascending identity order");
                                previous_base = ingredient.resource;
                                base_total += ingredient.amount;
                            }
                        }
                        check(base_total <= 600 && base_total <= static_cast<std::uint32_t>(request),
                              "base request budget remains bounded without a produced-Food claim");
                        check(stocks == original_stocks, "request arithmetic cannot debit input stock");
                    }
                }
            }
        }
    }
    for (const auto invalid : {std::uint8_t{0}, std::uint8_t{6}, std::uint8_t{255}})
        refuses([&] { (void)original::build_food_recipe_requests({}, invalid, 0); },
                "invalid request-builder grade is refused");
    for (const auto invalid : {std::numeric_limits<std::int32_t>::min(), -1, 601, largest})
        refuses([&] { (void)original::build_food_recipe_requests({}, 1, invalid); },
                "request-builder budget outside proved caller0..600 is refused");
    for (unsigned index = 0; index < 9; ++index) {
        std::array<std::int32_t, 9> negative{};
        negative[index] = -1;
        refuses([&] { (void)original::build_food_recipe_requests(negative, 1, 0); },
                "negative aggregate stock is refused even with zero request");
    }
}
void recipe_availability_from_actual_categories() {
    struct Case {
        std::string_view name;
        std::array<std::int32_t, 9> stocks;
        std::array<std::int32_t, 5> expected;
        std::array<bool, 7> vetoes{};
    };
    constexpr auto largest = std::numeric_limits<std::int32_t>::max();
    const std::array cases{
        Case{"empty", {}, {0,0,0,0,0}},
        Case{"one base", {1000,0,0,0,0,0,0,0,0}, {1000,0,0,0,0}},
        Case{"supplements alone", {0,0,0,0,0,0,0,2000,2000}, {0,0,0,0,0}},
        Case{"one base plus Spices", {1000,0,0,0,0,0,0,0,100}, {1000,1000,0,0,0}},
        Case{"Salt clips reused candidate", {1000,0,0,0,0,0,0,20,100}, {1000,100,100,0,0}},
        Case{"Salt below clipping minimum", {1000,0,0,0,0,0,0,19,100}, {1000,1000,0,0,0}},
        Case{"Salt partial block", {1000,0,0,0,0,0,0,199,100}, {1000,900,900,0,0}},
        Case{"complete supplements", {1000,0,0,0,0,0,0,200,100}, {1000,1000,1000,0,0}},
        Case{"two bases", {1000,1000,0,0,0,0,0,0,0}, {1000,2000,0,0,0}},
        Case{"two bases plus Spices", {1000,1000,0,0,0,0,0,0,200}, {1000,2000,2000,0,0}},
        Case{"two-base clipped candidate", {1000,1000,0,0,0,0,0,40,200}, {1000,2000,200,200,0}},
        Case{"three bases plus Spices", {1000,1000,1000,0,0,0,0,0,300}, {1000,2000,3000,3000,0}},
        Case{"three-base clipped candidate", {1000,1000,1000,0,0,0,0,60,300}, {1000,2000,3000,300,300}},
        Case{"Salt alone cannot make grade5", {1000,1000,1000,1000,0,0,0,800,0}, {1000,2000,3000,4000,0}},
        Case{"four bases plus Spices", {1000,1000,1000,1000,0,0,0,0,400}, {1000,2000,3000,4000,4000}},
        Case{"partial grade5 Spices", {1000,1000,1000,1000,0,0,0,600,300}, {1000,2000,3000,4000,3000}},
        Case{"uneven bases", {400,300,200,100,0,0,0,0,0}, {400,600,600,400,0}},
        Case{"availability is not rounded100", {250,249,248,247,0,0,0,0,0}, {250,498,744,988,0}},
        Case{"selected minimum capped10000", {12000,11000,10000,9999,0,0,0,0,0}, {10000,20000,30000,39996,0}},
        Case{"tiny bases", {50,50,0,0,0,0,0,0,0}, {0,100,0,0,0}},
        Case{"tiny full-Spices grade5", {25,25,25,25,0,0,0,0,10}, {0,0,0,100,100}},
        Case{"vetoed largest base", {1000,500,0,0,0,0,0,0,0}, {500,0,0,0,0}, {true,false,false,false,false,false,false}},
        Case{"all bases vetoed", {1000,1000,1000,1000,1000,1000,1000,1000,1000}, {0,0,0,0,0}, {true,true,true,true,true,true,true}},
        Case{"large base observations", {largest,largest,largest,largest,largest,largest,largest,0,0}, {10000,20000,30000,40000,0}},
        Case{"large full-supply paths", {largest,largest,largest,largest,largest,largest,largest,largest,largest}, {10000,20000,30000,40000,40000}},
        Case{"Spices below partial minimum", {1000,0,0,0,0,0,0,0,9}, {1000,0,0,0,0}},
        Case{"Spices first partial block", {1000,0,0,0,0,0,0,0,10}, {1000,100,0,0,0}},
        Case{"Spices below full requirement", {1000,0,0,0,0,0,0,0,99}, {1000,900,0,0,0}},
        Case{"both route partial Spices", {1000,0,0,0,0,0,0,200,99}, {1000,1000,900,0,0}},
        Case{"grade5 partial minimum39", {1000,1000,1000,1000,0,0,0,0,39}, {1000,2000,3000,4000,0}},
        Case{"grade5 partial minimum40", {1000,1000,1000,1000,0,0,0,0,40}, {1000,2000,3000,4000,400}}
    };
    for (const auto& row : cases) {
        std::array<std::int32_t, 5> actual{};
        for (std::uint8_t grade = 1; grade <= 5; ++grade)
            actual[grade - 1] = original::food_recipe_grade_availability(row.stocks, row.vetoes, grade);
        check(actual == row.expected, std::string("literal availability case: ") + std::string(row.name));
    }

    // Independent ranking oracle: the literal stocks increase by identity,
    // so descending identities give the needed ranked base without sorting.
    constexpr std::array<std::int32_t, 9> ascending{101,202,303,404,505,606,707,0,0};
    for (unsigned mask = 0; mask < 128; ++mask) {
        std::array<bool, 7> vetoes{};
        for (unsigned resource = 0; resource < 7; ++resource)
            vetoes[resource] = (mask & (1U << resource)) != 0;
        for (std::uint8_t grade = 1; grade <= 5; ++grade) {
            std::int32_t expected = 0;
            unsigned rank = 0;
            if (grade <= 4) {
                for (int resource = 6; resource >= 0; --resource) {
                    const auto index = static_cast<std::size_t>(resource);
                    if (!vetoes[index] && ++rank == grade) {
                        expected = ascending[index] * grade;
                        break;
                    }
                }
            }
            check(original::food_recipe_grade_availability(ascending, vetoes, grade) == expected,
                  "all seven resource-veto combinations preserve distinct base ranking");
        }
    }
    for (unsigned resource = 0; resource < 7; ++resource) {
        std::array<std::int32_t, 9> stocks{};
        stocks[resource] = 1000;stocks[8] = 100;
        check(original::food_recipe_grade_availability(stocks, {}, 1) == 1000 &&
              original::food_recipe_grade_availability(stocks, {}, 2) == 1000 &&
              original::food_recipe_grade_availability(stocks, {}, 3) == 0,
              "all seven actual base resource identities participate without conversion");
    }

    // Real composition of the three pure operations exposes their different
    // contracts: available grade2/budget100 still creates no ingredient amount.
    const std::array<std::int32_t, 9> tiny{50,50,0,0,0,0,0,0,0};
    std::array<std::int32_t, 5> tiny_availability{};
    for (std::uint8_t grade = 1; grade <= 5; ++grade)
        tiny_availability[grade - 1] = original::food_recipe_grade_availability(tiny, {}, grade);
    const auto chosen = original::select_food_recipe_grade(tiny_availability, 1, 5, 100);
    const auto requests = original::build_food_recipe_requests(tiny, chosen.grade, chosen.raw_budget);
    check(chosen == original::FoodRecipeGradeSelection{2,100} && requests.raw_return_value == 0,
          "positive native availability and selected grade do not prove produced Food");
    for (const auto& ingredient : requests.ingredients)
        check(ingredient == original::FoodIngredientRequest{-1,0},
              "tiny-stock fallback preserves the actual empty ingredient output");

    for (const auto invalid : {std::uint8_t{0}, std::uint8_t{6}, std::uint8_t{255}})
        refuses([&] { (void)original::food_recipe_grade_availability({}, {}, invalid); },
                "availability grade outside1..5 is refused");
    for (unsigned resource = 0; resource < 9; ++resource) {
        for (const auto value : {-1, std::numeric_limits<std::int32_t>::min()}) {
            std::array<std::int32_t, 9> negative{};
            negative[resource] = value;
            refuses([&] { (void)original::food_recipe_grade_availability(negative, {}, 1); },
                    "negative native aggregate is refused even on an unused grade1 category");
        }
    }
    const auto overflows = [](const std::array<std::int32_t, 9>& stocks) {
        try { (void)original::food_recipe_grade_availability(stocks, {}, 3); }
        catch (const std::overflow_error&) { ++checks; return; }
        throw std::runtime_error("reached supplement-cap signed-DWORD overflow was not refused");
    };
    check(original::food_recipe_grade_availability({1000,0,0,0,0,0,0,20,214748369}, {}, 3) == 100 &&
          original::food_recipe_grade_availability({1000,0,0,0,0,0,0,429496739,10}, {}, 3) == 100,
          "last safe supplement-cap products remain exact");
    overflows({1000,0,0,0,0,0,0,20,214748370});
    overflows({1000,0,0,0,0,0,0,429496740,10});
    overflows({1000,0,0,0,0,0,0,20,largest});
    check(original::food_recipe_grade_availability({1000,0,0,0,0,0,0,20,largest}, {}, 2) == 100 &&
          original::food_recipe_grade_availability({1000,0,0,0,0,0,0,200,largest}, {}, 3) == 1000,
          "unreached overflow calculations do not reject otherwise valid queried grades");
}
} // namespace

int main() {
    try {
        score_boundaries();
        actual_event_gates();
        finite_inventory_and_word_bounds();
        recipe_grade_choice();
        recipe_request_outputs();
        recipe_availability_from_actual_categories();
        std::cout << "Original Food rules: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original Food rules failed: " << error.what() << '\n';
        return 1;
    }
}
