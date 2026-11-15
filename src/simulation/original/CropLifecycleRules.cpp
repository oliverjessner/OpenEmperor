#include "simulation/original/CropLifecycleRules.h"

#include <stdexcept>

namespace openemperor::original {

std::uint16_t schedule_crop_harvest_mark(ModelBuilding field,
    std::uint8_t slot,std::uint8_t subslot,bool active,std::uint16_t work_word) {
    const auto& rule=crop_rule(field);
    if(slot>11 || subslot>15)
        throw std::out_of_range("crop harvest schedule requires slot0..11 and subslot0..15");
    if(active && slot==rule.end_slot && subslot==0)
        return static_cast<std::uint16_t>(work_word|0x8000U);
    return work_word;
}

CropFieldExpiration expire_crop_completed_slot(ModelBuilding field,
    std::uint8_t completed_slot,bool active,CropFieldLifecycleInput input) {
    const auto& rule=crop_rule(field);
    if(completed_slot>11)
        throw std::out_of_range("crop completed-slot event requires slot0..11");
    if(input.quantity>10000)
        throw std::out_of_range("crop expiration quantity is outside0..10000");
    if(!active || completed_slot<6 || completed_slot!=rule.end_slot)
        return {input,0,false,false};
    return {{0,0,false},input.quantity,true,input.assignment_present};
}

} // namespace openemperor::original
