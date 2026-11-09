#pragma once

// Authored inputs shared with Rule22; Rule23 is always selected explicitly.
// Residents, staffing, Music inputs and House visits are earned by paid World
// commands and ordinary ticks. No original game data is embedded here.
#include "OriginalMusicVenueFixture.h"

namespace oe_venue_house_fixture {
namespace q=oe_music_venue_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using q::buy;
using q::scene;
using q::school;
using q::venue;
using q::state;
using q::refuses;
using Scene=q::Scene;

struct Fixture : q::Fixture {
    using q::Fixture::Fixture;
    sim::World world() const {
        return sim::World::create_original_venue_house_service(policy,model,terrain);
    }
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_original_venue_house_service(snapshot,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_original_venue_house_service(document,root.data,mask(),policy,model,terrain);
    }
};
inline const sim::OriginalVenueHouseServiceState& walkers(const sim::World& world) {
    const auto& culture=state(world);
    check(culture.venue_house_service.has_value(),"Rule23 lost its venue walker authority");
    return *culture.venue_house_service;
}
inline const sim::OriginalVenueWalkerSource& source(const sim::World& world,sim::BuildingId venue_id) {
    for(const auto& row:walkers(world).sources)if(row.building_id==venue_id)return row;
    throw std::runtime_error("Venue walker fixture lost its paid Theatre channel");
}
inline void invariants(const sim::World& world) {
    check(world.rule_version()==sim::kOriginalVenueHouseServiceRuleVersion &&
          world.population_valid()&&world.navigation_valid()&&world.city_economy_valid()&&
          world.production_balance_valid()&&world.fire_state_valid(),"Rule23 World invariant failed");
    const auto& culture=state(world);const auto& service=walkers(world);
    check(culture.schools.size()<=2&&culture.venues.size()<=2&&service.sources.size()<=2&&
          world.buildings().size()<=82&&world.couriers().size()<=58,"Rule23 raised body/channel budgets");
    std::uint64_t taxes=0,upkeep=0,spawned=0;
    for(const auto& body:world.buildings()) {
        const auto workers=world.workforce_required(body.id);
        check(workers>=0&&body.placed_tick<=world.ticks(),"Rule23 installed workforce/age is invalid");
        upkeep+=(world.ticks()-body.placed_tick)/816*2*static_cast<std::uint64_t>(workers);
        taxes+=body.taxes_paid_total;
    }
    check(upkeep==world.maintenance_spent_total()&&taxes==world.taxes_collected_total()&&
          world.treasury()==12000+static_cast<std::int64_t>(taxes)-static_cast<std::int64_t>(upkeep)-
              static_cast<std::int64_t>(world.construction_spent_total()),"Rule23 paid finance changed");
    for(const auto& row:service.sources) {
        spawned+=row.spawned;const auto& channel=world.courier(row.courier_id);
        check(channel.owner==row.building_id&&channel.role==sim::CourierRole::OriginalVenueMusic&&
              !channel.cargo&&!channel.reserved&&!world.original_courier_cargo(channel.id),
              "Venue movement channel gained cargo or foreign ownership");
    }
    check(service.next_walker_id==spawned+1,"Venue walker highwater differs from successful spawns");
    for(const auto& row:culture.venues)
        check(row.music_timer==(row.last_arrival?q::expected_timer(world.ticks(),row.last_arrival->arrival_tick):0),
              "Rule23 changed actual arrival-derived Venue32 clock");
}
inline void through(sim::World& world,std::uint64_t tick) {
    check(world.ticks()<=tick,"Rule23 fixture exceeded intended time boundary");
    while(world.ticks()<tick)world.tick();invariants(world);
}
template<class P>inline void until(sim::World& world,P done,const std::string& message,int limit=1200) {
    for(int tick=0;tick<limit&&!done();++tick)world.tick();
    check(done(),message+" tick="+std::to_string(world.ticks())+" Funds="+std::to_string(world.treasury())+
          " population="+std::to_string(world.total_population()));invariants(world);
}
} // namespace oe_venue_house_fixture
