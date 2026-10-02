#include "simulation/World.h"

#include <algorithm>

namespace openemperor::simulation {

int World::household_health_risk(BuildingId id) const {
    return health_profile(profile_) && building(id).placed && building(id).kind==Object::Household ?
        building(id).health_risk:0;
}

bool World::household_health_protected(BuildingId id) const {
    return health_profile(profile_) && building(id).placed && building(id).kind==Object::Household &&
        building(id).health_protection_until_tick>ticks_;
}

bool World::household_sick(BuildingId id) const {
    return health_profile(profile_) && building(id).placed && building(id).kind==Object::Household &&
        building(id).sick_until_tick>ticks_;
}

std::uint64_t World::household_health_protection_remaining(BuildingId id) const {
    return household_health_protected(id) ? building(id).health_protection_until_tick-ticks_:0;
}

std::uint64_t World::household_sickness_remaining(BuildingId id) const {
    return household_sick(id) ? building(id).sick_until_tick-ticks_:0;
}

void World::update_health() {
    if (!health_profile(profile_)) return;
    // Arrivals precede this update. Water slows risk; it never cures or protects.
    // Keep the clock derived from placement, including after protection/recovery.
    for (auto& b:buildings_) {
        if (!b.placed || b.kind!=Object::Household || household_sick(b.id) ||
            household_health_protected(b.id)) continue;
        if (ticks_>b.placed_tick && (ticks_-b.placed_tick)%Rules::health_risk_step_ticks==0) {
            b.health_risk+=household_has_water(b.id) ? 1:3;
            if (b.health_risk>=Rules::health_risk_threshold) {
                b.health_risk=0;
                b.health_protection_until_tick=0;
                b.sick_until_tick=ticks_+Rules::illness_ticks;
            }
        }
    }
}

bool World::health_state_valid() const {
    for (const auto& b:buildings_) {
        if (!health_profile(profile_) || !b.placed || b.kind!=Object::Household) {
            if (b.health_risk || b.health_protection_until_tick || b.sick_until_tick) return false;
            continue;
        }
        if (b.health_risk<0 || b.health_risk>=Rules::health_risk_threshold || b.placed_tick>ticks_)
            return false;
        // Historical expired deadlines remain unchanged. Subtract only future ones.
        if ((b.health_protection_until_tick>ticks_ &&
             b.health_protection_until_tick-ticks_>Rules::health_protection_ticks) ||
            (b.sick_until_tick>ticks_ && b.sick_until_tick-ticks_>Rules::illness_ticks)) return false;
        if (household_sick(b.id) && (b.health_risk || b.health_protection_until_tick)) return false;
        if (household_health_protected(b.id) && b.health_risk) return false;
    }
    for (const auto& c:couriers_) if (c.role==CourierRole::HealthWorker) {
        if (!health_profile(profile_) || c.good!=Good::Goods || c.cargo || c.reserved ||
            !building(c.owner).placed || building(c.owner).kind!=Object::HealthPost ||
            c.dynamic_target_routes.size()>Rules::city_v10_household_limit) return false;
        if (c.phase!=CourierPhase::IdleAtWorkshop &&
            !courier_can_target(c.role,building(c.target))) return false;
        if (c.last_dispatched_target && !courier_can_target(c.role,building(*c.last_dispatched_target)))
            return false;
        for (const auto& route:c.dynamic_target_routes)
            if (!courier_can_target(c.role,building(route.first))) return false;
    }
    return true;
}

} // namespace openemperor::simulation
