#include "simulation/World.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace openemperor::simulation {

std::int64_t maintenance_cost(RulesProfile profile,Object kind) {
    if (!maintenance_profile(profile)) return 0;
    switch (kind) {
    case Object::ClaySource: return 8;
    case Object::Pottery: return 12;
    case Object::Warehouse: return 4;
    case Object::Farm: return 8;
    case Object::ServicePost: return 4;
    case Object::Market: return 8;
    case Object::FireWatch: return 4;
    case Object::Well: return 2;
    case Object::HealthPost: return 4;
    default: return 0;
    }
}

namespace {
bool due(const BuildingState& b,std::uint64_t tick) {
    return b.placed && tick>b.placed_tick &&
        (tick-b.placed_tick)%Rules::maintenance_interval_ticks==0;
}
}

std::int64_t World::current_maintenance_rate() const {
    std::int64_t rate=0;
    // Bounded by 46 buildings, each with a maximum rate of 12.
    for (const auto& b:buildings_) if (b.placed) rate+=maintenance_cost(profile_,b.kind);
    return rate;
}

std::optional<std::uint64_t> World::maintenance_due_in(BuildingId id) const {
    const auto found=std::ranges::find_if(buildings_,[&](const auto& b) { return b.id==id; });
    if (found==buildings_.end() || !found->placed || !maintenance_cost(profile_,found->kind) ||
        ticks_<found->placed_tick) return std::nullopt;
    const auto age=ticks_-found->placed_tick;
    if (age>0 && age%Rules::maintenance_interval_ticks==0) return 0;
    return Rules::maintenance_interval_ticks-age%Rules::maintenance_interval_ticks;
}

MaintenanceProjection World::maintenance_projection(std::uint64_t horizon) const {
    MaintenanceProjection result;
    result.applicable=maintenance_profile(profile_);
    result.horizon_ticks=horizon;
    if (horizon<=UINT64_MAX-ticks_) result.horizon_end_tick=ticks_+horizon;
    result.installed_rate=current_maintenance_rate();
    std::int64_t total=0;
    bool representable=result.horizon_end_tick.has_value();
    // Bounded building collection; no simulation, navigation or mutable cache.
    for (const auto& b:buildings_) {
        const auto cost=b.placed ? maintenance_cost(profile_,b.kind):0;
        if (!cost) continue;
        if (ticks_<b.placed_tick) { representable=false; continue; }
        // Unlike maintenance_due_in(), an already booked bill at this tick is
        // excluded. At age zero the first bill is also one full interval away.
        const auto distance=Rules::maintenance_interval_ticks-
            (ticks_-b.placed_tick)%Rules::maintenance_interval_ticks;
        if (distance<=UINT64_MAX-ticks_) {
            const auto next=ticks_+distance;
            if (!result.next_bill_tick || next<*result.next_bill_tick) {
                result.next_bill_tick=next;
                result.next_bill_cost=cost;
            } else if (next==*result.next_bill_tick) result.next_bill_cost+=cost;
        }
        if (horizon<distance) continue;
        const auto count=1+(horizon-distance)/Rules::maintenance_interval_ticks;
        if (count>static_cast<std::uint64_t>((INT64_MAX-total)/cost)) {
            representable=false;
            continue;
        }
        total+=static_cast<std::int64_t>(count)*cost;
    }
    if (representable) {
        result.interval_cost=total;
        if (treasury_>=INT64_MIN+total) result.funds_after_interval=treasury_-total;
    }
    return result;
}

HouseholdDemandStatus World::household_demand_status(BuildingId id) const {
    return household_demand_status(building(id),ticks_);
}

HouseholdDemandStatus World::household_demand_status(
    const BuildingState& home,std::uint64_t evaluation_tick) const {
    HouseholdDemandStatus result;
    result.food_required=food_profile(profile_);
    result.service_required=service_profile(profile_);
    if (!home.placed || home.kind!=Object::Household) return result;
    result.pottery_available=home.pottery_stock>0;
    result.food_available=home.food_stock>0;
    result.service_available=result.service_required && home.service_until_tick>evaluation_tick;
    result.burning=fire_profile(profile_) && home.fire_until_tick>evaluation_tick;
    result.sick=health_profile(profile_) && home.sick_until_tick>evaluation_tick;
    result.ready=!result.burning && !result.sick && result.pottery_available &&
        (!result.food_required || result.food_available) &&
        (!result.service_required || result.service_available);
    return result;
}

std::int64_t World::household_demand_tax(const BuildingState& home,std::uint64_t count) const {
    const int history=count>=Rules::city_v7_level2_demands ? 2:
        count>=Rules::city_v7_level1_demands ? 1:0;
    const int effective=water_profile(profile_) && !household_has_water(home.id) ? 0:
        desirability_profile(profile_) ?
        std::min(history,desirability_level_cap(household_desirability(home.id))):history;
    if (desirability_profile(profile_) || food_profile(profile_))
        return effective==2 ? Rules::city_v7_level2_tax:
            effective==1 ? Rules::city_v7_level1_tax:Rules::city_v7_level0_tax;
    return Rules::tax_income_per_fulfilled_demand;
}

void World::preflight_maintenance_tick() const {
    // Production never changes Household stocks. Arrivals, Health and Fire run
    // after demand. Evaluate its exact next-tick inputs before any World mutation.
    const auto next=ticks_+1; // Tick/deadline headroom was already checked.
    std::int64_t funds=treasury_;
    auto taxes=taxes_collected_total_;
    std::uint64_t bill=0;
    for (const auto& b:buildings_) if (b.placed) {
        if (b.kind==Object::Household && b.demand_progress==Rules::household_demand_ticks-1 &&
            household_demand_status(b,next).ready) {
            if (b.fulfilled_demand==UINT64_MAX)
                throw std::overflow_error("household fulfilled counter exhausted");
            const auto tax=household_demand_tax(b,b.fulfilled_demand+1);
            const auto unsigned_tax=static_cast<std::uint64_t>(tax);
            if (funds>INT64_MAX-tax || taxes>UINT64_MAX-unsigned_tax ||
                b.taxes_paid_total>UINT64_MAX-unsigned_tax)
                throw std::overflow_error("City tax counter exhausted");
            funds+=tax;
            taxes+=unsigned_tax;
        }
        if (due(b,next)) bill+=static_cast<std::uint64_t>(maintenance_cost(profile_,b.kind));
    }
    const auto signed_bill=static_cast<std::int64_t>(bill); // At most 46 * 12.
    if (maintenance_spent_total_>UINT64_MAX-bill || funds<INT64_MIN+signed_bill)
        throw std::overflow_error("City maintenance counter or treasury exhausted");
}

void World::bill_maintenance() {
    if (!maintenance_profile(profile_)) return;
    // Full ownership cost, irrespective of staffing, operation or incidents.
    // Aggregate arithmetic has been validated before the tick's first mutation.
    for (const auto& b:buildings_) if (due(b,ticks_)) {
        const auto cost=maintenance_cost(profile_,b.kind);
        treasury_-=cost;
        maintenance_spent_total_+=static_cast<std::uint64_t>(cost);
    }
}

bool World::maintenance_economy_valid() const {
    // Cancel unsigned terms before adding. This handles the complete uint64
    // counter range and INT64_MIN without overflowing intermediate sums or
    // narrowing counters to signed integers.
    const auto debt=treasury_<0 ? static_cast<std::uint64_t>(-(treasury_+1))+1:0;
    std::array<std::uint64_t,3> credits{static_cast<std::uint64_t>(starting_treasury_for(profile_)),
        taxes_collected_total_,debt};
    std::array<std::uint64_t,3> debits{construction_spent_total_,maintenance_spent_total_,
        treasury_>=0 ? static_cast<std::uint64_t>(treasury_):0};
    for (auto& credit:credits) for (auto& debit:debits) {
        const auto amount=std::min(credit,debit);
        credit-=amount;
        debit-=amount;
    }
    return std::ranges::all_of(credits,[](auto n) { return n==0; }) &&
        std::ranges::all_of(debits,[](auto n) { return n==0; });
}

} // namespace openemperor::simulation
