#include "simulation/original/SeasonRules.h"

#include "simulation/original/CropRules.h"
#include "simulation/original/OriginalScheduler.h"

#include <array>
#include <stdexcept>

namespace openemperor::original {
namespace {

SeasonRule food_season(ModelBuilding plant) {
    const auto& rule = crop_rule(plant);
    return {plant, rule.crop_index, rule.resource_id,
            {static_cast<std::int8_t>(rule.start_slot), -1, -1, -1},
            {static_cast<std::int8_t>(rule.end_slot), -1, -1, -1},
            1, 1, SeasonSuppression::LastEndSlot, 157, 79};
}

const std::array rules{
    food_season(ModelBuilding::WheatField),
    food_season(ModelBuilding::MilletField),
    food_season(ModelBuilding::RiceField),
    food_season(ModelBuilding::CabbageField),
    food_season(ModelBuilding::SoybeanField),
    SeasonRule{ModelBuilding::HempField, 5, 19, {3, -1, -1, -1}, {8, -1, -1, -1},
               1, 1, SeasonSuppression::None, 125, 63},
    SeasonRule{ModelBuilding::TeaBush, 6, 13, {2, 5, 8, -1}, {4, 7, 10, -1},
               3, 3, SeasonSuppression::LastEndSlot, 105, 53},
    SeasonRule{ModelBuilding::LacquerTree, 7, 14, {1, -1, -1, -1}, {6, 7, -1, -1},
               1, 2, SeasonSuppression::AnyEndSlot, 125, 63},
    SeasonRule{ModelBuilding::MulberryTree, 8, 12, {3, 6, -1, -1}, {5, 8, -1, -1},
               2, 2, SeasonSuppression::LastEndSlot, 157, 79}
};

bool is_food(ModelBuilding plant) noexcept {
    return plant >= ModelBuilding::WheatField && plant <= ModelBuilding::SoybeanField;
}

void validate_slot(std::uint8_t slot) {
    if (slot >= scheduler_months_per_year)
        throw std::out_of_range("Original season slot is outside0..11");
}

} // namespace

const SeasonRule& season_rule(ModelBuilding plant) {
    for (const auto& rule : rules)
        if (rule.plant == plant) return rule;
    throw std::out_of_range("unsupported Original seasonal plant model");
}

bool season_in_growing_period(ModelBuilding plant, std::uint8_t slot) {
    if (is_food(plant)) return crop_in_growing_period(plant, slot);
    const auto& rule = season_rule(plant);
    validate_slot(slot);
    for (std::size_t index = 0; index < rule.start_slots.size(); ++index) {
        const auto start = rule.start_slots[index];
        const auto end = rule.end_slots[index];
        if (start >= 0 && end >= 0 && slot >= start && slot < end) return true;
    }
    return false;
}

bool season_growth_suppressed(ModelBuilding plant, std::uint8_t slot) {
    const auto& rule = season_rule(plant);
    validate_slot(slot);
    switch (rule.suppression) {
    case SeasonSuppression::None: return false;
    case SeasonSuppression::LastEndSlot:
        return slot == rule.end_slots[rule.end_count - 1];
    case SeasonSuppression::AnyEndSlot:
        for (std::size_t index = 0; index < rule.end_count; ++index)
            if (slot == rule.end_slots[index]) return true;
        return false;
    }
    throw std::out_of_range("unsupported Original seasonal suppression rule");
}

std::uint16_t season_growth_increment(
    ModelBuilding plant, std::uint8_t slot, std::uint16_t work_word) {
    if (is_food(plant)) return crop_growth_increment(plant, slot, work_word);
    const auto& rule = season_rule(plant);
    if (season_growth_suppressed(plant, slot)) return 0;
    const auto work = work_word & 0xffU;
    if (work > 32) return rule.high_work_increment;
    return work > 0 ? rule.low_work_increment : 0;
}

} // namespace openemperor::original
