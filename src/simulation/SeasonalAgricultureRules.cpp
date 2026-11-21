#include "simulation/SeasonalAgricultureState.h"
#include "simulation/original/SeasonRules.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
std::uint64_t prefix(original::ModelBuilding model,std::uint64_t exclusive){
    const auto& r=original::season_rule(model);
    std::uint64_t eligible=0,tail=0;const auto rest=exclusive%seasonal_year_ticks;
    for(std::size_t i=0;i<r.start_slots.size();++i)if(r.start_slots[i]>=0&&r.end_slots[i]>=0){
        const auto a=static_cast<std::uint64_t>(r.start_slots[i])*seasonal_month_ticks;
        const auto b=static_cast<std::uint64_t>(r.end_slots[i])*seasonal_month_ticks;
        eligible+=b-a;if(rest>a)tail+=std::min(rest,b)-a;
    }
    return exclusive/seasonal_year_ticks*eligible+tail;
}
}
bool seasonal_growing(original::ModelBuilding model,std::uint64_t tick){
    return original::season_in_growing_period(model,static_cast<std::uint8_t>(tick/seasonal_month_ticks%12));
}
std::uint64_t seasonal_work_opportunities(original::ModelBuilding model,std::uint64_t placed,std::uint64_t through){
    if(placed>through||through==UINT64_MAX)throw std::invalid_argument("Seasonal work requires bounded paid World age");
    return prefix(model,through+1)-prefix(model,placed+1);
}
std::optional<std::uint64_t> seasonal_next_opening(original::ModelBuilding model,std::uint64_t tick){
    const auto& r=original::season_rule(model);const auto base=tick-tick%seasonal_year_ticks;
    std::optional<std::uint64_t> result;
    for(const auto start:r.start_slots)if(start>=0){
        const auto offset=static_cast<std::uint64_t>(start)*seasonal_month_ticks;
        if(offset>UINT64_MAX-base)continue;auto value=base+offset;
        if(value<=tick){if(value>UINT64_MAX-seasonal_year_ticks)continue;value+=seasonal_year_ticks;}
        if(!result||value<*result)result=value;
    }
    return result;
}
std::optional<std::uint64_t> seasonal_ready_expiry(original::ModelBuilding model,std::uint64_t ready){
    const auto& r=original::season_rule(model);const auto base=ready-ready%seasonal_year_ticks;
    const auto slot=ready/seasonal_month_ticks%12;
    for(std::size_t i=0;i<r.start_slots.size();++i)if(r.start_slots[i]>=0&&r.end_slots[i]>=0&&
        slot>=static_cast<unsigned>(r.start_slots[i])&&slot<static_cast<unsigned>(r.end_slots[i])){
        const auto offset=(static_cast<std::uint64_t>(r.end_slots[i])+1)*seasonal_month_ticks;
        if(offset>UINT64_MAX-base)return {};return base+offset;
    }
    return {};
}
bool seasonal_harvest_allowed(original::ModelBuilding model,std::uint64_t tick,std::uint64_t ready){
    const auto expiry=seasonal_ready_expiry(model,ready);return expiry&&ready<=tick&&tick<*expiry;
}
}
