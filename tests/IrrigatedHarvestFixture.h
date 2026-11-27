#pragma once

// Independent authored model/map inputs. The positive early harvest is earned
// entirely by paid placement, actual wet work and the shared physical Grower.
#include "AgricultureReconfigurationFixture.h"

namespace oe_irrigated_harvest_fixture {
namespace r=oe_agriculture_reconfiguration_fixture;
namespace i=oe_irrigation_productivity_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace s=oe_seasonal_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
using f::check;
using r::field;
using r::work;
using r::receipt;

struct Fixture:r::Fixture {
    sim::World world()const{return sim::World::create_irrigated_harvest(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const {
        return sim::World::restore_irrigated_harvest(value,policy,model,terrain);
    }
};
template<class Predicate>inline void until(sim::World& world,Predicate done,const char* why,int bound=3000){
    for(int t=0;t<bound&&!done();++t)world.tick();check(done(),why);
}
inline void ticks(sim::World& world,std::uint64_t count){for(std::uint64_t n=0;n<count;++n)world.tick();}
struct Scene {sim::BuildingId farm{},field{},pump{};};
inline Scene build_early_food(sim::World& world){
    constexpr std::array<std::size_t,0> no_fields{};
    const auto city=q::build(world,no_fields);const auto water=i::network(world);
    // Earn immigration, full pre-tick staffing and phase27 supply first. Wheat
    // is then paid during its genuine slot2 window, before the first fire.
    check(world.ticks()==0,"Early-harvest scene requires a fresh paid World");
    ticks(world,1700);
    check(world.workers_assigned(city.farm)==world.workforce_required(city.farm)&&
        world.irrigation_field_status({93,110}).water_available,
        "Early-harvest setup lacks actual staffed Farm or current paid ditch supply");
    Scene result{city.farm,q::crop(world,city.farm,0,{93,110}),water.pump};
    until(world,[&]{return s::field(world,result.field).ready_tick.has_value();},
        "New paid wet Wheat never earned real readiness",160);
    const auto& base=work(world,result.field);const auto& wet=i::work(world,result.field);
    const auto& season=s::field(world,result.field);
    check(base.productive_ticks==64&&wet.wet_work_ticks==64&&wet.bonus_work==16&&!wet.clipped_bonus&&
        !season.wasted_work&&!season.harvest_progress&&!field(world,result.field).harvested&&
        !receipt(world,result.field).returned&&world.ticks()-world.building(result.field).placed_tick<75,
        "Early-harvest setup lacks 64 real wet growth ticks and precisely16 earned bonus units");
    check(q::worker(world,result.farm).arrived_tick&&q::worker(world,result.farm).work_ticks==64&&
        q::worker(world,result.farm).ready_tick==season.ready_tick,
        "Ready crop lacks its actual arrived and working physical Grower");
    return result;
}
inline void valid(const sim::World& world){
    check(world.rule_version()==sim::kIrrigatedHarvestRuleVersion&&world.irrigated_harvest_enabled()&&
        world.agriculture_reconfiguration_enabled()&&world.food_warehouses_enabled()&&world.processed_food_state()&&
        !world.hunting_food_state()&&!world.fishing_food_state()&&!world.original_agriculture_state()&&
        world.original_production_state()->food==sim::OriginalFoodState{},
        "Rule37 lost explicit inherited authority or duplicated the sole Food account");
    check(world.population_valid()&&world.city_economy_valid()&&world.production_balance_valid()&&
        world.navigation_valid()&&world.fire_state_valid(),"Irrigated harvest broke shared World invariants");
    const auto snapshot=world.snapshot();
    sim::World::validate_original_production_snapshot(snapshot);
    sim::World::validate_irrigation_productivity_snapshot(snapshot);
    sim::World::validate_seasonal_agriculture_snapshot(snapshot);
    sim::World::validate_physical_food_growers_snapshot(snapshot);
    sim::World::validate_agriculture_reconfiguration_snapshot(snapshot);
}
} // namespace oe_irrigated_harvest_fixture
