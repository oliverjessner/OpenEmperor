#pragma once

// Complete independent source/model fixtures. Wildlife is created by the fresh
// Rule29 ecology; all people, staffing, kills, Food and arrivals are earned.
#include "FishingFixture.h"

namespace oe_hunting_fixture {
namespace g=oe_fishing_fixture;
namespace m=oe_playable_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using g::purchase;
inline constexpr sim::Cell tent_origin{84,44},second_tent_origin{88,44};

struct Fixture : g::Fixture {
    explicit Fixture(bool lake=true):g::Fixture(lake) {
        auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
        const auto at=text.find("HOUSE MODS\n");check(at!=std::string::npos,"Hunting fixture model section missing");
        // Independent positive inputs demonstrate data-driven cost/workforce;
        // no proprietary row is copied into this authored test model.
        text.insert(at,"33,Hunter's Tent,{,29,0,0,0,0,15,0,0,0,0,0,0,0,},,,\n");
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_playable_hunting_rules_model(root.data);rebind();
    }
    void rebind() {
        g::Fixture::rebind();
        terrain=save::prepare_playable_hunting_terrain_facts(root.data,f::map_relative);
    }
    sim::World world() const {return sim::World::create_playable_hunting(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_playable_hunting(snapshot,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_playable_hunting(document,root.data,mask(),policy,model,terrain);
    }
};
inline void prey_markers(Fixture& fixture,std::span<const sim::Cell> markers) {
    // Independent immutable test geography. The normal no-marker fixture
    // above exercises the file-backed fallback and persistence preparation.
    std::vector<std::uint8_t> boundary(fixture.raw.size());
    for(int y=0;y<228;++y)for(int x=0;x<228;++x)
        boundary[std::size_t(y)*228U+std::size_t(x)]=fixture.terrain->irrigation_boundary_allowed({x,y});
    fixture.terrain=std::make_shared<const sim::OriginalTerrainFacts>(fixture.raw,fixture.fertility,
        fixture.terrain->source_map_sha256(),boundary,std::span<const sim::Cell>{},markers);
}
inline sim::BuildingId tent(sim::World& world,sim::Cell origin=tent_origin) {
    return purchase(world,sim::CommandType::PlaceOriginalHuntersTent,origin);
}
struct Scene : g::Scene {
    sim::BuildingId tent{},second_tent{};
    std::vector<sim::BuildingId> tents;
};
inline Scene build(sim::World& world,bool with_farms=true,bool second=false) {
    Scene result;static_cast<g::Scene&>(result)=g::build(world,with_farms,false);
    for(int y=33;y<=46;++y)f::buy(world,{sim::CommandType::PlaceRoad,{79,y}});
    for(int x=80;x<=92;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,46}});
    result.tent=tent(world);result.tents.push_back(result.tent);
    if(second){result.second_tent=tent(world,second_tent_origin);result.tents.push_back(result.second_tent);}
    check(!world.total_population() && world.hunting_state()->hunters.empty(),"Paid Hunting construction granted people or a Hunter");
    return result;
}
inline const sim::HuntingFoodStock& stock(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.hunting_food_state()->stocks)if(row.building_id==id)return row;
    throw std::runtime_error("Hunting paid owner lost its sole seven-kind Food account");
}
inline const sim::HuntingTent& source(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.hunting_state()->tents)if(row.tent_id==id)return row;
    throw std::runtime_error("Hunting paid Tent lost its source row");
}
inline std::uint64_t consumed(const sim::World& world,std::size_t resource=sim::hunting_food_index) {
    std::uint64_t result=0;for(const auto& row:world.hunting_food_state()->stocks)result+=row.consumed.at(resource);return result;
}
inline std::uint64_t kills(const sim::World& world) {
    std::uint64_t result=0;for(const auto& row:world.hunting_state()->habitats)result+=row.killed;return result;
}
inline std::uint64_t births(const sim::World& world) {
    std::uint64_t result=0;for(const auto& row:world.hunting_state()->habitats)result+=row.births;return result;
}
inline void valid(const sim::World& world) {
    check(world.population_valid() && world.city_economy_valid() && world.production_balance_valid() &&
        world.navigation_valid() && world.fire_state_valid(),"Hunting violated shared World invariants");
    check(world.hunting_state() && world.hunting_food_state() && world.fishing_state() && !world.fishing_food_state() &&
        !world.original_agriculture_state() && world.original_production_state()->food==sim::OriginalFoodState{},
        "Hunting duplicated the sole seven-kind Food authority");
    sim::HuntingFoodCounters produced{},held{},cargo{},used{},departed{},received{},returned{};
    for(const auto& field:world.hunting_food_state()->fields)produced[static_cast<std::size_t>(field.crop)]+=field.harvested;
    for(const auto& quay:world.fishing_state()->quays)produced[sim::fishing_food_index]+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)produced[sim::hunting_food_index]+=tent.unloaded;
    for(const auto& row:world.hunting_food_state()->stocks) {
        check(g::volume(row.amounts)<= (world.building(row.building_id).kind==sim::Object::Market ? 16:8),"Seven-kind Food volume exceeded capacity");
        for(std::size_t i=0;i<7;++i) {
            check(row.amounts[i]>=0,"Hunting Food became negative");held[i]+=static_cast<std::uint64_t>(row.amounts[i]);
            used[i]+=row.consumed[i];departed[i]+=row.departed[i];received[i]+=row.received[i];returned[i]+=row.returned[i];
        }
    }
    for(const auto& trip:world.hunting_food_state()->trips)for(std::size_t i=0;i<7;++i)
        cargo[i]+=static_cast<std::uint64_t>(trip.loaded[i]-trip.delivered[i]-trip.returned[i]);
    for(std::size_t i=0;i<7;++i) {
        check(produced[i]==held[i]+cargo[i]+used[i],"Seven-kind Food source/hold/cargo/consumption mismatch");
        check(departed[i]==received[i]+returned[i]+cargo[i],"Seven-kind Food departures/arrivals/returns mismatch");
    }
    const auto& ecology=*world.hunting_state();std::uint64_t carried=0;
    for(const auto& hunter:ecology.hunters)carried+=static_cast<std::uint64_t>(hunter.cargo);
    check(ecology.habitats.size()*sim::hunting_habitat_capacity+births(world)==ecology.animals.size()+kills(world),"Hunting initial+births != live+killed");
    check(kills(world)==carried+produced[sim::hunting_food_index],"Hunting killed != active Meat+actual Tent receipts");
    check(kills(world)==carried+held[6]+cargo[6]+used[6],"Hunting ecology/Hunter/Food/consumption balance failed");
    sim::World::validate_hunting_snapshot(world.snapshot());
}
inline void ticks(sim::World& world,int count) {for(int i=0;i<count;++i)world.tick();valid(world);}
template<class Predicate> inline void until(sim::World& world,Predicate done,const char* reason,int limit=30000) {
    for(int i=0;i<limit && !done();++i)world.tick();check(done(),reason);valid(world);
}
inline void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,"Refused Hunting command changed paid authority");
}
} // namespace oe_hunting_fixture
