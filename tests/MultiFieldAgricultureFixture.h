#pragma once

// Independent authored terrain/model inputs. Commands and actual World ticks
// alone earn residents, staffing, field work, Food, arrivals and consumption.
#include "HuntingFixture.h"
#include "simulation/MultiFieldAgricultureState.h"

#include <map>
#include <numeric>

namespace oe_multi_field_fixture {
namespace h=oe_hunting_fixture;
namespace m=oe_playable_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using f::check;
using h::purchase;

struct Fixture : h::Fixture {
    explicit Fixture(int capacity=28):h::Fixture(false) {
        if(capacity!=28) {
            auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
            for(std::size_t row=0;row<3;++row)
                oe_production_fixture::set_house_field(text,row,17,capacity);
            f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
            model=save::prepare_playable_hunting_rules_model(root.data);
        }
    }
    sim::World world()const{return sim::World::create_multi_field_agriculture(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const {
        return sim::World::restore_multi_field_agriculture(value,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& value)const {
        return save::restore_multi_field_agriculture(value,root.data,mask(),policy,model,terrain);
    }
};
inline sim::BuildingId crop(sim::World& world,sim::BuildingId farm,std::size_t index,sim::Cell cell) {
    return m::crop(world,farm,index,cell);
}
struct Scene {
    sim::BuildingId farm{},market{},water{},ancestor{};
    std::vector<sim::BuildingId> fields,homes,watches;
};
inline Scene build(sim::World& world,std::span<const std::size_t> crops,
        sim::Cell farm_at={20,25},bool watches=true) {
    Scene result;
    for(int x=19;x<=62;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<56;x+=3)result.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
    result.farm=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,farm_at);
    for(std::size_t i=0;i<crops.size();++i)
        result.fields.push_back(crop(world,result.farm,crops[i],{farm_at.x+3,farm_at.y+static_cast<int>(i)}));
    result.market=purchase(world,sim::CommandType::PlaceMarket,{27,25});
    result.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{31,25});
    result.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{35,25});
    if(watches)for(std::size_t i=0;i<sim::Rules::fire_watch_limit;++i)
        result.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{39+3*static_cast<int>(i),25}));
    check(!world.total_population() && !world.taxes_collected_total(),"Multi-field construction granted residents or income");
    return result;
}
inline Scene build(sim::World& world,bool watches=true) {
    constexpr std::array<std::size_t,3> crops{0,1,4};return build(world,crops,{20,25},watches);
}
inline const sim::AgricultureField& field(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.hunting_food_state()->fields)if(row.field_id==id)return row;
    throw std::runtime_error("Paid multi-field plot lost its sole crop binding");
}
inline const sim::MultiFieldWork& work(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.multi_field_agriculture_state()->fields)if(row.field_id==id)return row;
    throw std::runtime_error("Paid multi-field plot lost its work history");
}
inline const sim::MultiFieldFarmSchedule& schedule(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.multi_field_agriculture_state()->farms)if(row.farm_id==id)return row;
    throw std::runtime_error("Paid multi-field Farm lost its shared work cursor");
}
inline std::uint64_t farm_work(const sim::World& world,sim::BuildingId farm) {
    std::uint64_t result=0;
    for(const auto& row:world.multi_field_agriculture_state()->fields)if(row.farm_id==farm)result+=row.productive_ticks;
    return result;
}
inline sim::HuntingFoodCounters consumed(const sim::World& world) {
    sim::HuntingFoodCounters values{};
    for(const auto& row:world.hunting_food_state()->stocks)
        for(std::size_t i=0;i<values.size();++i)values[i]+=row.consumed[i];
    return values;
}
inline sim::HuntingFoodTrip trip(const sim::World& world,sim::CourierId id) {
    for(const auto& value:world.hunting_food_state()->trips)if(value.transfer.courier_id==id)return value;
    throw std::runtime_error("Actual Food courier lost its seven-kind physical transaction");
}
inline void valid(const sim::World& world) {
    h::valid(world);
    check(world.rule_version()==sim::kMultiFieldAgricultureRuleVersion && world.multi_field_agriculture_state(),
        "Multi-field QA lost its explicit World generation");
    const auto& state=*world.multi_field_agriculture_state();
    check(state.fields.size()==world.hunting_food_state()->fields.size() &&
          state.farms.size()==world.hunting_food_state()->farms.size(),"Multi-field work rows duplicate or omit paid identities");
    for(const auto& farm:state.farms) {
        check(world.building(farm.farm_id).progress==0,"Multi-field mode retained the old Farm-only production clock");
        check(farm_work(world,farm.farm_id)<=world.ticks()-world.building(farm.farm_id).placed_tick,
            "Multiple fields manufactured more labor than their real paid Farm age");
    }
    for(const auto& row:state.fields) {
        const auto& origin=field(world,row.field_id);
        check(origin.farm_id==row.farm_id && row.productive_ticks/sim::multi_field_work_per_portion==origin.harvested &&
            world.agriculture_field_progress(row.field_id)==row.productive_ticks%sim::multi_field_work_per_portion,
            "Per-field work, derived progress and sole actual Food origin differ");
        check(!origin.quantity && !origin.productive_events && !origin.last_harvest_cycle,
            "Authored multi-field labor claimed Original Grower/calendar authority");
    }
    sim::World::validate_multi_field_agriculture_snapshot(world.snapshot());
    check(world.buildings().size()<=82 && world.couriers().size()+12<=58,"Multi-field city exceeded shared body/channel bounds");
}
inline void ticks(sim::World& world,int count) {for(int i=0;i<count;++i)world.tick();valid(world);}
template<class Predicate>inline void until(sim::World& world,Predicate done,const char* reason,int limit=30000) {
    for(int i=0;i<limit&&!done();++i)world.tick();check(done(),reason);valid(world);
}
inline void denied(sim::World& world,sim::Command command){h::denied(world,command);}
} // namespace oe_multi_field_fixture
