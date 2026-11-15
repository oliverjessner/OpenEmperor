#pragma once

#include <array>
#include <cstdint>

namespace openemperor::original {

// Raw House observations. The physical API domain is population and stock
// 0..32767, matching the nonnegative subset of the native signed WORDs.
// Status is the native byte, not an OpenEmperor BuildingStatus identity.
struct RawHouseFoodState {
    std::uint8_t status = 1;
    std::uint16_t population = 0;
    std::uint16_t stock = 0;
    std::uint8_t quality = 0;
    bool operator==(const RawHouseFoodState&) const = default;
};

struct RawHouseFoodConsumption {
    RawHouseFoodState house;
    std::uint16_t removed = 0;
    bool operator==(const RawHouseFoodConsumption&) const = default;
};

// One caller-resolved normal event from 518690, without its scheduler or World
// authority. Current-row HouseField8 (FoodQuality) only gates requirement>0;
// it does not set the consumed amount. Exact native status1 consumes up to
// floor(population/4) raw Dinners units. An eligible event clears quality when
// stock is empty, including zero population/demand. Gated events preserve it.
// raw_food_bypass is source global c5cda0. Nonzero is refused, never reproduced
// as a stock or quality grant. Invalid physical inputs are refused even when
// another input would make the event a no-op. No Food-portion mapping is implied.
[[nodiscard]] RawHouseFoodConsumption consume_raw_house_food(
    RawHouseFoodState house, std::int32_t food_quality_requirement,
    std::int32_t raw_food_bypass);

// Exact signed-int32 score mapper 545100. This maps an already resolved raw
// score to grade0..5; it does not create a recipe, score or delivery event.
[[nodiscard]] std::uint8_t food_grade_from_raw_score(std::int32_t score) noexcept;

struct FoodRecipeGradeSelection {
    std::uint8_t grade = 0;
    std::uint16_t raw_budget = 0;
    bool operator==(const FoodRecipeGradeSelection&) const = default;
};

// Choice arithmetic from Mill callback555330, not a complete recipe. Array
// entry0 is caller-resolved availability for grade1, through entry4 for grade5.
// Require nonnegative signed-int32 availability/request and 1<=lower<=upper<=5.
// The first available grade is fallback. A higher grade replaces it only when
// its availability is strictly greater than request/3 (integer truncation).
// A selected grade has budget min(availability,request,600), including budget0
// for request0. Source555330 then calls its ingredient builder even with that
// zero budget and ignores the builder result. This query neither calls that
// builder nor establishes ingredients, stock changes, actors or deliveries.
[[nodiscard]] FoodRecipeGradeSelection select_food_recipe_grade(
    const std::array<std::int32_t, 5>& availability,
    std::uint8_t lower_grade, std::uint8_t upper_grade,
    std::int32_t raw_request);

struct FoodIngredientRequest {
    // Native request-array IDs include0 for untouched fresh slots and-1 for
    // cleared slots. Positive IDs1..7 are bases,8 Salt and9 Spices.
    std::int32_t resource = 0;
    // Native amounts are DWORDs; the proved caller budget bounds this pure
    // operation's generated amounts to0..600, without narrowing native stock.
    std::uint16_t amount = 0;
    bool operator==(const FoodIngredientRequest&) const = default;
};

struct FoodRecipeRequests {
    std::array<FoodIngredientRequest, 6> ingredients{};
    // Opaque native555410 return. It is neither produced Food nor a success
    // flag; the actual caller ignores it. It may be positive after a failed
    // supplement attempt or when request0 leaves all output slots untouched.
    std::int32_t raw_return_value = 0;
    bool operator==(const FoodRecipeRequests&) const = default;
};

// Complete555410/5557d0 request-output arithmetic with the planner's fresh
// zeroed arrays. Stocks are9 caller-resolved aggregated resource quantities,
// index0=resource1 through index8=Spices9, each nonnegative signed-int32.
// They are not native physical bins: Mill53 has8 bins. Require grade1..5 and
// caller budget0..600. The builder does not consult the base-veto flags that
// availability uses. Preserve source order, partial/zero requests and opaque
// return values; do not interpret retained base requests as a complete recipe.
// No source debit, assistant creation, Food grant or World authority occurs.
[[nodiscard]] FoodRecipeRequests build_food_recipe_requests(
    const std::array<std::int32_t, 9>& aggregate_stocks,
    std::uint8_t grade, std::int32_t raw_request);

// One native Mill5558d0 grade-availability query. Nine aggregate stocks are
// resources1..9, not nine physical bins; all require nonnegative signed-DWORD
// observations. Seven caller-resolved veto flags apply only to bases1..7.
// Base availability uses distinct positive unvetoed categories and the native
// selected-minimum cap10000. Salt8/Spices9 retain the source's ordered fallback
// arithmetic, including Salt clipping a candidate reused by Spices. Grade5
// has four-base+Spices and three-base+both routes, no four-base+Salt route.
// A reached supplement-cap multiplication outside nonwrapping signed-DWORD
// arithmetic is refused, without claiming native wrap parity. Unreached caps
// do not restrict large otherwise valid stocks. Require grade1..5. Availability
// is not an ingredient request, successful recipe, stock debit or Food origin;
// a positive value can still lead to zero/partial builder output.
[[nodiscard]] std::int32_t food_recipe_grade_availability(
    const std::array<std::int32_t, 9>& aggregate_stocks,
    const std::array<bool, 7>& base_vetoes, std::uint8_t grade);

} // namespace openemperor::original
