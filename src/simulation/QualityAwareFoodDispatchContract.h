#pragma once

#include "simulation/AgricultureState.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace openemperor::simulation {
inline constexpr const char* quality_aware_food_dispatch_contract="authored-quality-aware-food-dispatch-v1";
inline constexpr std::uint32_t quality_aware_food_dispatch_revision=1;

// Pure selection from physical raw stock, never an arrival or quality grant.
// The requirement is the actual occupied recipient's current source row at
// dispatch; a higher requirement earned during travel is not predicted.
inline ProcessedFoodAmounts quality_aware_raw_food_load(const ProcessedFoodAmounts& available,int limit,int required_quality) {
    if(limit<0||limit>4||required_quality<0||required_quality>100)
        throw std::invalid_argument("Quality-aware raw Food selection requires cart4 and a bounded source-row need");
    for(std::size_t g=0;g<available.size();++g)
        if(available[g]<0||(g>=processed_food_first_meal&&available[g]))
            throw std::invalid_argument("Quality-aware raw Food selection admits only actual nonnegative raw stock");
    const auto amount=std::min(limit,food_batch::volume(available));
    if(!amount)return {};
    const auto historical=food_batch::mixed_portions(available,static_cast<std::uint64_t>(amount));
    if(agriculture_batch_quality(historical).value>=required_quality)return historical;
    std::array<std::uint8_t,processed_food_first_meal> scores{};
    for(std::size_t g=0;g<scores.size();++g){ProcessedFoodAmounts single{};single[g]=1;scores[g]=agriculture_batch_quality(single).value;}
    auto remaining=available;ProcessedFoodAmounts selected{},best{};
    // At most four physical choices, each scanning the same seven raw kinds.
    // Descending scores maximize every prefix; ties keep catalog order.
    for(int n=0;n<amount;++n){
        std::optional<std::size_t> chosen;
        for(std::size_t g=0;g<scores.size();++g)if(remaining[g]&&(!chosen||scores[g]>scores[*chosen]))chosen=g;
        if(!chosen)throw std::logic_error("Quality-aware Food selection lost an actual source portion");
        --remaining[*chosen];++selected[*chosen];
        if(agriculture_batch_quality(selected).value<required_quality)break;
        best=selected;
    }
    return best;
}
} // namespace openemperor::simulation
