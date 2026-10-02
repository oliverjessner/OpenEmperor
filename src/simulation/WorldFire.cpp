#include "simulation/World.h"

#include <algorithm>

namespace openemperor::simulation {

bool fire_eligible(Object kind) {
    return kind==Object::ClaySource || kind==Object::Pottery || kind==Object::Warehouse ||
        kind==Object::Household || kind==Object::Farm || kind==Object::ServicePost ||
        kind==Object::Market;
}

bool World::building_on_fire(BuildingId id) const {
    return fire_profile(profile_) && building(id).fire_until_tick>ticks_;
}

bool World::building_fire_protected(BuildingId id) const {
    return fire_profile(profile_) && building(id).fire_protection_until_tick>ticks_;
}

std::uint64_t World::fire_protection_remaining(BuildingId id) const {
    return building_fire_protected(id) ? building(id).fire_protection_until_tick-ticks_:0;
}

std::uint64_t World::fire_remaining(BuildingId id) const {
    return building_on_fire(id) ? building(id).fire_until_tick-ticks_:0;
}

std::size_t World::fire_eligible_buildings() const {
    if (!fire_profile(profile_)) return 0;
    return static_cast<std::size_t>(std::count_if(buildings_.begin(),buildings_.end(),
        [](const auto& b) { return b.placed && fire_eligible(b.kind); }));
}

std::size_t World::protected_buildings() const {
    return static_cast<std::size_t>(std::count_if(buildings_.begin(),buildings_.end(),
        [&](const auto& b) { return b.placed && fire_eligible(b.kind) && building_fire_protected(b.id); }));
}

std::size_t World::burning_buildings() const {
    return static_cast<std::size_t>(std::count_if(buildings_.begin(),buildings_.end(),
        [&](const auto& b) { return b.placed && fire_eligible(b.kind) && building_on_fire(b.id); }));
}

void World::update_fire() {
    if (!fire_profile(profile_)) return;
    // A visit in the preceding movement phase resets risk and protects immediately.
    // The placement-relative risk clock is never paused or persisted separately.
    for (auto& b:buildings_) {
        if (!b.placed || !fire_eligible(b.kind) || building_on_fire(b.id) ||
            building_fire_protected(b.id)) continue;
        if (ticks_>b.placed_tick && (ticks_-b.placed_tick)%Rules::fire_risk_step_ticks==0) {
            if (++b.fire_risk==Rules::fire_risk_threshold) {
                b.fire_risk=0;
                b.fire_protection_until_tick=0;
                b.fire_until_tick=ticks_+Rules::fire_incident_ticks;
            }
        }
    }
}

bool World::fire_state_valid() const {
    for (const auto& b:buildings_) {
        if (!fire_profile(profile_) || !fire_eligible(b.kind)) {
            if (b.fire_risk || b.fire_protection_until_tick || b.fire_until_tick) return false;
            continue;
        }
        if (b.fire_risk<0 || b.fire_risk>=Rules::fire_risk_threshold || b.placed_tick>ticks_)
            return false;
        // Expired deadlines remain historical values. Subtraction is performed
        // only for future deadlines, avoiding overflow near the integer limit.
        if ((b.fire_protection_until_tick>ticks_ &&
             b.fire_protection_until_tick-ticks_>Rules::fire_protection_ticks) ||
            (b.fire_until_tick>ticks_ && b.fire_until_tick-ticks_>Rules::fire_incident_ticks))
            return false;
        if (building_on_fire(b.id) && (b.fire_risk || b.fire_protection_until_tick)) return false;
        if (building_fire_protected(b.id) && b.fire_risk) return false;
    }
    for (const auto& c:couriers_) if (c.role==CourierRole::FireInspector) {
        if (!fire_profile(profile_) || c.good!=Good::Goods || c.cargo || c.reserved ||
            building(c.owner).kind!=Object::FireWatch ||
            c.dynamic_target_routes.size()>Rules::city_v11_building_limit) return false;
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
