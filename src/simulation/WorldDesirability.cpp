#include "simulation/World.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
std::int64_t interval_gap(int a,int length_a,int b,int length_b) {
    const auto end_a=static_cast<std::int64_t>(a)+length_a-1;
    const auto end_b=static_cast<std::int64_t>(b)+length_b-1;
    return std::max<std::int64_t>({0,static_cast<std::int64_t>(b)-end_a,
                                   static_cast<std::int64_t>(a)-end_b});
}
}

int footprint_distance(Cell a,BuildingFootprint af,Cell b,BuildingFootprint bf) {
    if (af.width<1 || af.height<1 || bf.width<1 || bf.height<1)
        throw std::invalid_argument("footprint dimensions must be positive");
    const auto gap=interval_gap(a.x,af.width,b.x,bf.width)+
                   interval_gap(a.y,af.height,b.y,bf.height);
    return static_cast<int>(std::min<std::int64_t>(gap,INT_MAX));
}

int desirability_impact(Object kind) {
    switch (kind) {
    case Object::ClaySource: return -18;
    case Object::Pottery: return -22;
    case Object::Warehouse: return -10;
    case Object::Farm: return -4;
    case Object::Market: return 12;
    case Object::ServicePost: return 10;
    case Object::FireWatch: return 8;
    default: return 0;
    }
}

int desirability_contribution(Object kind,int distance) {
    if (distance<0) throw std::invalid_argument("negative footprint distance");
    const int quarters=distance<=2 ? 4:distance<=4 ? 3:distance<=6 ? 2:
                       distance<=desirability_radius ? 1:0;
    // Signed integer division rounds towards zero, independently per source.
    return desirability_impact(kind)*quarters/4;
}
int clamp_desirability(int score) { return std::clamp(score,-100,100); }
int desirability_level_cap(int score) { return score< -20 ? 0:score<10 ? 1:2; }

int World::building_distance(BuildingId a,BuildingId b) const {
    const auto& first=building(a);
    const auto& second=building(b);
    if (!first.placed || !second.placed)
        throw std::invalid_argument("distance requires two placed buildings");
    return footprint_distance(first.cell,building_footprint(profile_,first.kind),
                              second.cell,building_footprint(profile_,second.kind));
}

int World::household_desirability_at(Cell origin) const {
    if (!desirability_profile(profile_))
        throw std::invalid_argument("desirability requires City-v13");
    const auto footprint=building_footprint(profile_,Object::Household);
    int score=0;
    for (const auto& b:buildings_) if (b.placed && desirability_impact(b.kind)!=0)
        score+=desirability_contribution(b.kind,footprint_distance(origin,footprint,
            b.cell,building_footprint(profile_,b.kind)));
    return clamp_desirability(score);
}

int World::household_desirability(BuildingId id) const {
    const auto& home=building(id);
    if (!home.placed || home.kind!=Object::Household)
        throw std::invalid_argument("desirability requires a placed Household");
    return household_desirability_at(home.cell);
}

std::vector<DesirabilitySource> World::household_desirability_sources(BuildingId id) const {
    const auto& home=building(id);
    if (!desirability_profile(profile_) || !home.placed || home.kind!=Object::Household)
        throw std::invalid_argument("desirability sources require a placed City-v13 Household");
    std::vector<DesirabilitySource> result;
    for (const auto& b:buildings_) if (b.placed && desirability_impact(b.kind)!=0) {
        const auto distance=building_distance(id,b.id);
        const auto contribution=desirability_contribution(b.kind,distance);
        if (contribution!=0) result.push_back({b.id,b.kind,distance,contribution});
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        const auto magnitude_a=std::abs(a.contribution),magnitude_b=std::abs(b.contribution);
        return magnitude_a!=magnitude_b ? magnitude_a>magnitude_b:a.id<b.id;
    });
    return result;
}
} // namespace openemperor::simulation
