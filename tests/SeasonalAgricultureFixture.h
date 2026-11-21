#pragma once

// Independent paid inputs. Calendar passage never injects work, cargo or stock.
#include "PhysicalFoodGrowersFixture.h"
#include "simulation/SeasonalAgricultureState.h"

namespace oe_seasonal_agriculture_fixture {
namespace q=oe_physical_food_growers_fixture;
namespace p=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using q::purchase;
using q::crop;
using q::work;
using q::farm;
using q::worker;
using q::active;
using q::phase;
using q::receipt_field;
using q::consumed;
using q::volume;

struct Fixture:q::Fixture {
    explicit Fixture(int capacity=28):q::Fixture(false,capacity) {
        // Explicit independent consumers. The inherited playable Food fixture
        // disables luxury needs; finite bins alone cannot sustain these chains.
        auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
        for(const unsigned need:{9U,11U,12U,13U})
            oe_production_fixture::set_house_field(text,2,need,1);
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_playable_hunting_rules_model(root.data);
    }
    sim::World world()const{return sim::World::create_seasonal_agriculture(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const {
        return sim::World::restore_seasonal_agriculture(value,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& value)const {
        return save::restore_seasonal_agriculture(value,root.data,mask(),policy,model,terrain);
    }
};
inline const sim::SeasonalFoodField& field(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.seasonal_agriculture_state()->fields)if(row.field_id==id)return row;
    throw std::runtime_error("Paid seasonal Food field lost its sole seasonal row");
}
inline const sim::SeasonalPlant& plant(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.seasonal_agriculture_state()->plants)if(row.plant_id==id)return row;
    throw std::runtime_error("Paid seasonal plantation lost its sole seasonal row");
}
inline const sim::SeasonalHempFarm& hemp(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.seasonal_agriculture_state()->hemp_farms)if(row.farm_id==id)return row;
    throw std::runtime_error("Paid seasonal Hemp Farm lost its sole seasonal row");
}
inline const sim::OriginalHempFarmState& hemp_origin(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.original_production_state()->hemp_farms)if(row.farm_id==id)return row;
    throw std::runtime_error("Paid seasonal Hemp Farm lost its sole Goods origin");
}
inline std::uint64_t progress(const sim::World& world,sim::BuildingId id) {
    const auto& row=field(world,id);const auto& food=q::field(world,id);
    return work(world,id).productive_ticks-row.wasted_work-80*food.harvested;
}
struct Scene {
    p::Scene materials;
    std::array<sim::BuildingId,5> farms{},fields{};
    sim::BuildingId hemp_farm{},hemp_field{};
};
inline Scene build(sim::World& world) {
    Scene result;result.materials=q::material_city(world);
    for(int x=89;x<=149;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,100}});
    for(int y=101;y<104;++y)f::buy(world,{sim::CommandType::PlaceRoad,{89,y}});
    for(std::size_t i=0;i<5;++i) {
        const int x=90+7*static_cast<int>(i);
        result.farms[i]=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{x,97});
        result.fields[i]=crop(world,result.farms[i],i,{x+3,97});
        q::source_qualified(world,result.farms[i]);q::source_qualified(world,result.fields[i]);
    }
    result.hemp_farm=purchase(world,sim::CommandType::PlaceOriginalHempFarm,{132,98});
    result.hemp_field=p::plant(world,sim::CommandType::PlaceOriginalHempField,result.hemp_farm,{134,98});
    check(!world.total_population() && !world.taxes_collected_total(),"Seasonal paid scene received free residents or tax");
    return result;
}
inline void valid(const sim::World& world) {
    p::valid(world);
    check(world.rule_version()==32 && world.seasonal_agriculture_state() && world.physical_food_growers_state() &&
        world.multi_field_agriculture_state() && world.hunting_food_state() && !world.original_agriculture_state() &&
        !world.fishing_food_state() && world.original_production_state()->food==sim::OriginalFoodState{},
        "Seasonal generation duplicated Food or lost inherited physical work authority");
    sim::HuntingFoodCounters picked{},returned{},origin{},held{},cargo{},used{},departed{},received{},cart_returned{};
    for(const auto& row:world.hunting_food_state()->fields) {
        const auto index=static_cast<std::size_t>(row.crop);picked[index]+=row.harvested;
        returned[index]+=receipt_field(world,row.field_id).returned;
        const auto& seasonal=field(world,row.field_id);const auto earned=work(world,row.field_id).productive_ticks;
        check(seasonal.wasted_work<=earned && row.harvested<=(earned-seasonal.wasted_work)/80,
            "Seasonal Food spent unearned or expired work");
        const auto current=progress(world,row.field_id);
        check(current<=80 && seasonal.ready_tick.has_value()==(current==80) &&
            seasonal.harvest_work==5*row.harvested+seasonal.wasted_harvest_work+seasonal.harvest_progress &&
            seasonal.harvest_progress<5,"Seasonal growth/readiness/real harvest-work equation failed");
    }
    for(const auto& row:world.physical_food_growers_state()->farms)if(active(world,row.farm_id))
        cargo[static_cast<std::size_t>(worker(world,row.farm_id).crop)]+=static_cast<std::uint64_t>(
            world.physical_food_grower_cargo(row.courier_id));
    for(std::size_t i=0;i<5;++i)check(picked[i]==returned[i]+cargo[i],
        "Seasonal field harvest is neither physical Grower cargo nor a genuine Farm receipt");
    origin=returned;cargo={};
    for(const auto& row:world.fishing_state()->quays)origin[5]+=row.unloaded;
    for(const auto& row:world.hunting_state()->tents)origin[6]+=row.unloaded;
    for(const auto& row:world.hunting_food_state()->stocks) {
        check(volume(row.amounts)<=(world.building(row.building_id).kind==sim::Object::Market?16:8),"Seasonal Food exceeded physical bin volume");
        for(std::size_t i=0;i<7;++i) {
            check(row.amounts[i]>=0,"Seasonal Food stock became negative");held[i]+=static_cast<std::uint64_t>(row.amounts[i]);
            used[i]+=row.consumed[i];departed[i]+=row.departed[i];received[i]+=row.received[i];cart_returned[i]+=row.returned[i];
        }
    }
    for(const auto& row:world.hunting_food_state()->trips)for(std::size_t i=0;i<7;++i)
        cargo[i]+=static_cast<std::uint64_t>(row.loaded[i]-row.delivered[i]-row.returned[i]);
    for(std::size_t i=0;i<7;++i) {
        check(origin[i]==held[i]+cargo[i]+used[i],"Seasonal real source returns differ from stock/cart/consumption");
        check(departed[i]==received[i]+cart_returned[i]+cargo[i],"Seasonal Food transfer equation failed");
    }
    for(const auto& row:world.plantations_state()->plants) {
        const auto& season=plant(world,row.plant_id);
        check(row.productive_ticks==80*row.harvested+season.wasted_work+row.growth &&
            row.grown==row.harvested+season.expired_ready+static_cast<unsigned>(season.ready_tick.has_value()) &&
            season.ready_tick.has_value()==(row.growth==80),"Seasonal plantation work/readiness/expiry equation failed");
    }
    for(const auto& row:world.seasonal_agriculture_state()->hemp_farms) {
        const auto current=world.building(row.farm_id).progress;const auto& original=hemp_origin(world,row.farm_id);
        check(current>=0 && current<=80 && row.productive_ticks==80*row.picked+row.wasted_work+static_cast<unsigned>(current) &&
            row.grown==row.picked+row.expired_ready+static_cast<unsigned>(row.ready_tick.has_value()) &&
            row.ready_tick.has_value()==(current==80) && row.picked==original.harvested+static_cast<std::uint64_t>(world.seasonal_hemp_grower_cargo(row.courier_id)),
            "Seasonal Hemp work/ready/pick/actual-return equation failed");
    }
    const auto channels=world.couriers().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+
        3*world.hunting_state()->habitats.size()+world.grower_navigation_tracks().size();
    check(world.buildings().size()<=82 && channels<=58,"Seasonal generation expanded shared body/channel limits");
    sim::World::validate_seasonal_agriculture_snapshot(world.snapshot());
    sim::World::validate_physical_food_growers_snapshot(world.snapshot());
}
inline void ticks(sim::World& world,std::uint64_t count) {for(std::uint64_t i=0;i<count;++i)world.tick();valid(world);}
inline void through(sim::World& world,std::uint64_t completed) {
    check(world.ticks()<=completed,"Seasonal boundary already passed");ticks(world,completed-world.ticks());
}
template<class Predicate>inline void until(sim::World& world,Predicate done,const char* why,int limit=40000) {
    for(int i=0;i<limit && !done();++i)world.tick();check(done(),why);valid(world);
}
inline void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,"Rejected seasonal command changed paid authority");
}
} // namespace oe_seasonal_agriculture_fixture
