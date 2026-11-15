#include "simulation/original/CropRules.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

constexpr std::array rules{
    CropRule{ModelBuilding::WheatField, 0, 7, 2, 6},
    CropRule{ModelBuilding::MilletField, 1, 5, 6, 10},
    CropRule{ModelBuilding::RiceField, 2, 6, 5, 9},
    CropRule{ModelBuilding::CabbageField, 3, 3, 7, 11},
    CropRule{ModelBuilding::SoybeanField, 4, 1, 4, 8}
};

void validate_slot(std::uint8_t slot) {
    if (slot >= 12) throw std::out_of_range("original crop slot is outside 0..11");
}

} // namespace

const CropRule& crop_rule(ModelBuilding field) {
    for (const auto& rule : rules)
        if (rule.field == field) return rule;
    throw std::out_of_range("unsupported original Food field model");
}

bool crop_in_growing_period(ModelBuilding field, std::uint8_t slot) {
    const auto& rule = crop_rule(field);
    validate_slot(slot);
    return slot >= rule.start_slot && slot < rule.end_slot;
}

std::uint16_t crop_growth_increment(
    ModelBuilding field, std::uint8_t slot, std::uint16_t work_word) {
    const auto& rule = crop_rule(field);
    validate_slot(slot);
    if (slot == rule.end_slot) return 0;
    const auto work = work_word & 0xffU;
    if (work > 32) return 157;
    return work > 0 ? 79 : 0;
}

std::uint16_t apply_crop_growth(std::uint16_t quantity, std::uint16_t increment) {
    if (quantity > 10000) throw std::out_of_range("original crop quantity is outside 0..10000");
    if (increment != 0 && increment != 79 && increment != 157)
        throw std::out_of_range("unsupported original Food growth increment");
    const auto result = static_cast<std::uint32_t>(quantity) + increment;
    return static_cast<std::uint16_t>(std::min(result, std::uint32_t{10000}));
}

std::uint16_t raw_base_yield(std::uint16_t quantity, bool doubled_base) {
    if (quantity > 10000) throw std::out_of_range("original crop quantity is outside 0..10000");
    const auto divisor = doubled_base ? 50 : 100;
    return static_cast<std::uint16_t>(quantity / divisor);
}

std::uint8_t runtime_fertility(
    std::uint8_t serialized_fertility, std::uint8_t modifier) noexcept {
    const auto scaled = 55 * static_cast<unsigned>(serialized_fertility) / 100;
    return static_cast<std::uint8_t>(scaled + modifier);
}

std::uint16_t complete_crop_tending(std::uint16_t work_word) noexcept {
    return static_cast<std::uint16_t>(work_word | 0x40U);
}

std::uint16_t decay_crop_work(std::uint16_t work_word) noexcept {
    const auto work = work_word & 0xffU;
    const auto decayed = work > 0 ? work - 1 : 0;
    return static_cast<std::uint16_t>((work_word & 0xc000U) | decayed);
}

std::int16_t initial_crop_work_wait(
    ModelBuilding field, CropWork work, bool field_accepted,
    bool retained_field_word_nonzero, bool terrain_flag_100) {
    (void)crop_rule(field);
    if (work != CropWork::Tending && work != CropWork::Harvest)
        throw std::out_of_range("unsupported original crop work branch");
    if (!field_accepted || !retained_field_word_nonzero || terrain_flag_100) return 0;
    return work == CropWork::Tending ? 51 : 25;
}

CropWaitStep advance_crop_work_wait(std::int16_t wait) noexcept {
    const auto next = wait == std::numeric_limits<std::int16_t>::min()
        ? std::numeric_limits<std::int16_t>::max()
        : static_cast<std::int16_t>(wait - 1);
    return {next, next <= 0};
}

CropDispatchRule crop_dispatch_rule(std::uint8_t staff_percent,CropDispatchConfig configured) {
    if(staff_percent>100)
        throw std::out_of_range("crop dispatch staffing percentage is outside0..100");
    if(configured.harvest_limit<0 || configured.tending_limit<0)
        throw std::out_of_range("crop dispatch requires nonnegative configured limits");
    if(staff_percent==0) return {};
    if(staff_percent<25) return {true,32,0,0};
    if(staff_percent>=100)
        return {true,1,configured.harvest_limit,configured.tending_limit};
    const auto scale=[&](std::int32_t limit) {
        if(staff_percent>=75) {
            if(limit>std::numeric_limits<std::int32_t>::max()/3)
                throw std::overflow_error("crop dispatch three-quarter quota exceeds supported signed-DWORD multiplication");
            return static_cast<std::int32_t>(limit*3/4);
        }
        return static_cast<std::int32_t>(limit/(staff_percent>=50 ? 2:4));
    };
    const auto threshold=static_cast<std::uint8_t>(staff_percent>=75 ? 4:staff_percent>=50 ? 8:16);
    return {true,threshold,scale(configured.harvest_limit),std::max(std::int32_t{1},scale(configured.tending_limit))};
}

CropDispatchAttempt advance_crop_dispatch_attempt(
    std::uint8_t counter,std::uint8_t staff_percent,CropDispatchConfig configured) {
    const auto rule=crop_dispatch_rule(staff_percent,configured);
    if(!rule.staffed) return {counter,false};
    const auto next=static_cast<std::uint8_t>(static_cast<unsigned>(counter)+1U);
    const bool due=counter>=rule.attempt_threshold;
    return {due ? std::uint8_t{0}:next,due};
}

} // namespace openemperor::original
