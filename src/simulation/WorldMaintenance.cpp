#include "simulation/World.h"

#include <algorithm>
#include <array>
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
            b.fire_until_tick<=next && b.sick_until_tick<=next && b.pottery_stock>0 &&
            b.food_stock>0 && b.service_until_tick>next) {
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
