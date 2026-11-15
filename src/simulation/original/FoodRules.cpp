#include "simulation/original/FoodRules.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace openemperor::original {

RawHouseFoodConsumption consume_raw_house_food(
    RawHouseFoodState house, std::int32_t food_quality_requirement,
    std::int32_t raw_food_bypass) {
    if (house.population > 32767 || house.stock > 32767)
        throw std::invalid_argument("raw House Food population or stock exceeds physical WORD domain");
    if (raw_food_bypass != 0)
        throw std::invalid_argument("raw House Food bypass is unsupported");

    RawHouseFoodConsumption result{house, 0};
    if (house.status != 1 || food_quality_requirement <= 0) return result;

    const auto demand = static_cast<std::uint16_t>(house.population / 4);
    result.removed = std::min(house.stock, demand);
    result.house.stock = static_cast<std::uint16_t>(house.stock - result.removed);
    if (result.house.stock == 0) result.house.quality = 0;
    return result;
}

std::uint8_t food_grade_from_raw_score(std::int32_t score) noexcept {
    if (score >= 90) return 5;
    if (score >= 70) return 4;
    if (score >= 50) return 3;
    if (score >= 30) return 2;
    return score > 0 ? 1 : 0;
}

FoodRecipeGradeSelection select_food_recipe_grade(
    const std::array<std::int32_t, 5>& availability,
    std::uint8_t lower_grade, std::uint8_t upper_grade,
    std::int32_t raw_request) {
    if (lower_grade < 1 || upper_grade > 5 || lower_grade > upper_grade)
        throw std::invalid_argument("invalid raw Food recipe grade bounds");
    if (raw_request < 0 || std::any_of(availability.begin(), availability.end(),
                                    [](auto amount) { return amount < 0; }))
        throw std::invalid_argument("negative raw Food recipe availability or request");

    FoodRecipeGradeSelection result;
    std::int32_t selected_availability = 0;
    const auto threshold = raw_request / 3;
    for (auto grade = lower_grade; grade <= upper_grade; ++grade) {
        const auto available = availability[grade - 1];
        if (available > threshold || (result.grade == 0 && available != 0)) {
            result.grade = grade;
            selected_availability = available;
        }
    }
    if (result.grade != 0)
        result.raw_budget = static_cast<std::uint16_t>(
            std::min({selected_availability, raw_request, std::int32_t{600}}));
    return result;
}

FoodRecipeRequests build_food_recipe_requests(
    const std::array<std::int32_t, 9>& aggregate_stocks,
    std::uint8_t grade, std::int32_t raw_request) {
    if (grade < 1 || grade > 5 || raw_request < 0 || raw_request > 600)
        throw std::invalid_argument("invalid raw Food recipe grade or caller budget");
    if (std::any_of(aggregate_stocks.begin(), aggregate_stocks.end(),
                    [](auto amount) { return amount < 0; }))
        throw std::invalid_argument("negative raw Food recipe aggregate stock");

    FoodRecipeRequests result;
    if (raw_request == 0) {
        result.raw_return_value = aggregate_stocks[8];
        return result;
    }
    const auto salt_stock = aggregate_stocks[7];
    const auto spices_stock = aggregate_stocks[8];
    const auto assign = [&](std::size_t slot, std::int32_t resource, std::int32_t amount) {
        result.ingredients[slot] = {resource, static_cast<std::uint16_t>(amount)};
    };
    const auto bases = [&](std::size_t count) {
        // The caller already capped total request600, so count*per_base<=600.
        const auto per_base = (raw_request / static_cast<std::int32_t>(count) / 100) * 100;
        result.ingredients.fill(FoodIngredientRequest{-1, 0});
        std::size_t selected = 0;
        for (std::size_t index = 0; index < 7 && selected < count; ++index) {
            if (aggregate_stocks[index] < per_base) continue;
            assign(selected++, static_cast<std::int32_t>(index + 1), per_base);
        }
        return selected == count ? per_base * static_cast<std::int32_t>(count) : 0;
    };
    const auto either_supplement = [&](std::size_t count) {
        const auto total = bases(count);
        const auto salt = (total / 100) * 20;
        const auto spices = (total / 100) * 10;
        if (total != 0 && salt_stock >= salt) {
            assign(count, 8, salt);
            result.raw_return_value = salt;
            return true;
        }
        if (total != 0 && spices_stock >= spices) {
            assign(count, 9, spices);
            result.raw_return_value = salt;
            return true;
        }
        result.raw_return_value = salt;
        return false;
    };
    const auto both_supplements = [&](std::size_t count) {
        const auto total = bases(count);
        const auto salt = (total / 100) * 20;
        const auto spices = (total / 100) * 10;
        if (total != 0 && salt_stock >= salt && spices_stock >= spices) {
            assign(count, 8, salt);
            assign(count + 1, 9, spices);
        }
        result.raw_return_value = salt;
    };

    if (grade == 1) {
        result.raw_return_value = bases(1);
        return result;
    }
    if (grade <= 4) {
        result.raw_return_value = bases(grade);
        if (result.raw_return_value != 0) return result;
        if (either_supplement(grade - 1) || grade == 2) return result;
        both_supplements(grade - 2);
        return result;
    }
    const auto total = bases(4);
    const auto spices = (total / 100) * 10;
    if (total != 0 && spices_stock >= spices) {
        assign(4, 9, spices);
        result.raw_return_value = spices;
        return result;
    }
    both_supplements(3);
    return result;
}

std::int32_t food_recipe_grade_availability(
    const std::array<std::int32_t, 9>& aggregate_stocks,
    const std::array<bool, 7>& base_vetoes, std::uint8_t grade) {
    if (grade < 1 || grade > 5)
        throw std::invalid_argument("invalid raw Food availability grade");
    if (std::any_of(aggregate_stocks.begin(), aggregate_stocks.end(),
                    [](auto amount) { return amount < 0; }))
        throw std::invalid_argument("negative raw Food availability aggregate stock");

    std::array<std::int32_t, 7> available_bases{};
    for (std::size_t index = 0; index < available_bases.size(); ++index)
        if (!base_vetoes[index]) available_bases[index] = aggregate_stocks[index];
    std::sort(available_bases.begin(), available_bases.end(),
              [](auto left, auto right) { return left > right; });
    std::array<std::int32_t, 5> base{};
    for (std::int32_t count = 1; count <= 4; ++count)
        base[static_cast<std::size_t>(count)] = count *
            std::min(std::int32_t{10000}, available_bases[static_cast<std::size_t>(count - 1)]);

    const auto salt = aggregate_stocks[7];
    const auto spices = aggregate_stocks[8];
    const auto cap_from_supplement = [](std::int32_t stock, std::int32_t divisor) {
        const auto blocks = stock / divisor;
        if (blocks > std::numeric_limits<std::int32_t>::max() / 100)
            throw std::overflow_error("reached raw Food supplement cap exceeds supported signed-DWORD arithmetic");
        return blocks * 100;
    };
    const auto one_supplement = [&](std::int32_t count, std::int32_t best) {
        auto candidate = base[static_cast<std::size_t>(count)];
        if (candidate == 0) return best;
        if (salt >= (candidate / 100) * 20) best = std::max(best, candidate);
        else if (salt >= count * 20) {
            candidate = cap_from_supplement(salt, 20);
            if (candidate >= count * 100) best = std::max(best, candidate);
        }
        // The native Spices branch reuses Salt's retained clipped candidate.
        if (candidate != 0) {
            if (spices >= (candidate / 100) * 10) best = std::max(best, candidate);
            else if (spices >= count * 10) {
                const auto capped = cap_from_supplement(spices, 10);
                if (capped >= count * 100) best = std::max(best, capped);
            }
        }
        return best;
    };
    const auto both_supplements = [&](std::int32_t count, std::int32_t best) {
        const auto candidate = base[static_cast<std::size_t>(count)];
        if (candidate == 0) return best;
        if (salt >= (candidate / 100) * 20 && spices >= (candidate / 100) * 10)
            return std::max(best, candidate);
        if (salt >= count * 20 && spices >= count * 10) {
            const auto capped = std::min(cap_from_supplement(salt, 20),
                                         cap_from_supplement(spices, 10));
            if (capped >= count * 100) best = std::max(best, capped);
        }
        return best;
    };

    std::int32_t result = 0;
    switch (grade) {
    case 1: result = base[1]; break;
    case 2: result = one_supplement(1, base[2]); break;
    case 3: result = both_supplements(1, one_supplement(2, base[3])); break;
    case 4: result = both_supplements(2, one_supplement(3, base[4])); break;
    case 5:
        if (base[4] != 0) {
            if (spices >= (base[4] / 100) * 10) result = base[4];
            else if (spices >= 40) {
                const auto capped = cap_from_supplement(spices, 10);
                if (capped >= 400) result = capped;
            }
        }
        result = both_supplements(3, result);
        break;
    }
    return result >= 100 ? result : 0;
}

} // namespace openemperor::original
