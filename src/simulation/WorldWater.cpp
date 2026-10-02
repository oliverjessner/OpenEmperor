#include "simulation/World.h"

#include <climits>
#include <stdexcept>

namespace openemperor::simulation {

std::optional<BuildingId> World::nearest_water_source_at(Cell origin) const {
    if (!water_profile(profile_)) return std::nullopt;
    std::optional<BuildingId> nearest;
    int distance=INT_MAX;
    const auto home=building_footprint(profile_,Object::Household);
    for (const auto& well:buildings_) if (well.placed && well.kind==Object::Well) {
        const auto d=footprint_distance(origin,home,well.cell,
            building_footprint(profile_,Object::Well));
        if (d<=Rules::water_radius &&
            (d<distance || (d==distance && (!nearest || well.id<*nearest)))) {
            nearest=well.id; distance=d;
        }
    }
    return nearest;
}

std::optional<BuildingId> World::nearest_water_source(BuildingId id) const {
    const auto& home=building(id);
    if (!home.placed || home.kind!=Object::Household)
        throw std::invalid_argument("water query requires a placed Household");
    return nearest_water_source_at(home.cell);
}

bool World::household_has_water_at(Cell origin) const {
    return nearest_water_source_at(origin).has_value();
}

bool World::household_has_water(BuildingId id) const {
    return nearest_water_source(id).has_value();
}

int World::water_covered_households() const {
    int count=0;
    for (const auto& b:buildings_)
        if (b.placed && b.kind==Object::Household && household_has_water(b.id)) ++count;
    return count;
}

World::WellCoverage World::well_coverage_at(Cell origin) const {
    WellCoverage result;
    if (!water_profile(profile_)) return result;
    for (const auto& home:buildings_) if (home.placed && home.kind==Object::Household &&
        footprint_distance(home.cell,building_footprint(profile_,home.kind),origin,
            building_footprint(profile_,Object::Well))<=Rules::water_radius) {
        ++result.households;
        if (!household_has_water(home.id)) ++result.currently_dry;
    }
    return result;
}

} // namespace openemperor::simulation
