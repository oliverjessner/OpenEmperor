#pragma once

// Independent authored sources. Paid commands and ordinary World ticks alone
// earn people, crop work, physical harvest cargo and actual Farm receipts.
#include "MultiFieldAgricultureFixture.h"
#include "simulation/PhysicalFoodGrowersState.h"

namespace oe_physical_food_growers_fixture {
namespace n=oe_multi_field_fixture;
namespace h=oe_hunting_fixture;
namespace g=oe_fishing_fixture;
namespace m=oe_playable_agriculture_fixture;
namespace p=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using f::check;
using h::purchase;
using n::crop;
using n::field;
using n::work;
using n::farm_work;
using n::consumed;
using n::trip;
using g::volume;

struct Fixture:h::Fixture {
    explicit Fixture(bool lake=false,int capacity=28):h::Fixture(lake) {
        if(capacity!=28) {
            auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
            for(std::size_t row=0;row<3;++row)oe_production_fixture::set_house_field(text,row,17,capacity);
            f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
            model=save::prepare_playable_hunting_rules_model(root.data);
        }
    }
    sim::World world()const{return sim::World::create_physical_food_growers(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const {
        return sim::World::restore_physical_food_growers(value,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& value)const {
        return save::restore_physical_food_growers(value,root.data,mask(),policy,model,terrain);
    }
};
inline void source_qualified(const sim::World& world,sim::BuildingId id) {
    const auto terrain=world.original_terrain_facts();const auto policy=world.map_permissions();
    for(const auto cell:world.building_cells(id)) {
        const auto raw=terrain->raw_terrain(cell);
        check(terrain->irrigation_boundary_allowed(cell) && (raw==0x80U || raw==0xc0U) && policy->road_allowed(cell) &&
            !policy->protected_original(cell) && !policy->fixed_passage(cell),
            "Authored Grower body outside supported source land at "+std::to_string(cell.x)+","+std::to_string(cell.y)+
            " raw="+std::to_string(raw)+" boundary="+std::to_string(terrain->irrigation_boundary_allowed(cell)));
    }
}
inline n::Scene build(sim::World& world,std::span<const std::size_t> crops,bool watches=true) {
    n::Scene result;
    for(int x=89;x<=132;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,109}});
    for(int x=90;x<126;x+=3)result.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,107}));
    result.farm=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    for(std::size_t i=0;i<crops.size();++i)result.fields.push_back(crop(world,result.farm,crops[i],{93,110+static_cast<int>(i)}));
    result.market=purchase(world,sim::CommandType::PlaceMarket,{97,110});
    result.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{101,110});
    result.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
    if(watches)for(std::size_t i=0;i<sim::Rules::fire_watch_limit;++i)
        result.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{109+3*static_cast<int>(i),110}));
    source_qualified(world,result.farm);for(const auto field:result.fields)source_qualified(world,field);
    check(!world.total_population() && !world.taxes_collected_total(),"Central paid Grower scene granted residents or income");return result;
}
inline n::Scene build(sim::World& world,bool watches=true) {
    constexpr std::array<std::size_t,3> crops{0,1,4};return build(world,crops,watches);
}
inline p::Scene material_city(sim::World& world) {
    // Translate the independent plantation recipe into the complete source
    // Diamond, retaining its paid bodies, quantities and model requirements.
    p::Scene scene;
    for(const int y:{104,107})for(int x=89;x<=149;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
    for(const int x:{89,149})for(const int y:{105,106})f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
    for(int x=90;x<150;x+=3)scene.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,102}));
    scene.tea_source=purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,{92,105});
    scene.lacquer_source=purchase(world,sim::CommandType::PlaceOriginalLacquerRefinery,{102,105});
    scene.mulberry_source=purchase(world,sim::CommandType::PlaceOriginalSilkwormShed,{112,105});
    scene.tea_plant=p::plant(world,sim::CommandType::PlaceOriginalTeaBush,scene.tea_source,{96,109});
    scene.lacquer_plant=p::plant(world,sim::CommandType::PlaceOriginalLacquerTree,scene.lacquer_source,{106,109});
    scene.mulberry_plant=p::plant(world,sim::CommandType::PlaceOriginalMulberryTree,scene.mulberry_source,{116,109});
    scene.logging=purchase(world,sim::CommandType::PlaceOriginalLoggingShed,{122,105});
    scene.warehouse=purchase(world,sim::CommandType::PlaceWarehouse,{128,105});
    scene.market=purchase(world,sim::CommandType::PlaceMarket,{134,105});
    scene.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{138,105});
    scene.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{142,105});
    scene.lacquerware=purchase(world,sim::CommandType::PlaceOriginalLacquerwareMaker,{124,108});
    scene.weaver=purchase(world,sim::CommandType::PlaceOriginalWeaver,{134,108});
    for(std::size_t i=0;i<sim::Rules::fire_watch_limit;++i)
        scene.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{100+30*static_cast<int>(i),108}));
    return scene;
}
inline const sim::PhysicalFoodGrowerFarm& farm(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.physical_food_growers_state()->farms)if(row.farm_id==id)return row;
    throw std::runtime_error("Paid Farm lost its unique physical Grower channel");
}
inline const sim::PhysicalFoodGrowerField& receipt_field(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.physical_food_growers_state()->fields)if(row.field_id==id)return row;
    throw std::runtime_error("Paid Field lost its actual returned-crop history");
}
inline bool active(const sim::World& world,sim::BuildingId id) {
    const auto& row=farm(world,id);return row.latest_trip && !row.latest_trip->returned_tick;
}
inline const sim::PhysicalFoodGrowerTrip& worker(const sim::World& world,sim::BuildingId id) {
    const auto& row=farm(world,id);check(row.latest_trip.has_value(),"Farm has no real dispatched Grower lifetime");return *row.latest_trip;
}
inline bool phase(const sim::World& world,sim::BuildingId id,sim::PhysicalFoodGrowerPhase wanted) {
    return active(world,id) && worker(world,id).phase==wanted;
}
inline void valid(const sim::World& world) {
    p::valid(world);
    check(world.physical_food_growers_enabled() && world.physical_food_growers_state() && world.multi_field_agriculture_state() &&
        world.hunting_food_state() && !world.original_agriculture_state() && !world.fishing_food_state() &&
        world.original_production_state()->food==sim::OriginalFoodState{},"Physical Growers lost their explicit sole Food authority");
    const auto& state=*world.physical_food_growers_state();const auto& food=*world.hunting_food_state();
    check(state.farms.size()==food.farms.size() && state.fields.size()==food.fields.size(),"Physical Grower source/field identity rows differ");
    sim::HuntingFoodCounters picked{},returned_crop{},grower_cargo{},origin{},held{},cart{},used{},departed{},received{},returned_cart{};
    for(const auto& row:food.fields) {
        const auto index=static_cast<std::size_t>(row.crop);picked[index]+=row.harvested;
        returned_crop[index]+=receipt_field(world,row.field_id).returned;
        check(row.harvested==work(world,row.field_id).productive_ticks/80 &&
            world.agriculture_field_progress(row.field_id)==work(world,row.field_id).productive_ticks%80,
            "Physical field work lost its sole harvest/progress origin");
    }
    std::uint64_t spawned=0,retired=0;
    for(const auto& row:state.farms) {
        spawned+=row.spawned;retired+=row.retired;const auto& actor=world.courier(row.courier_id);
        check(actor.owner==row.farm_id && actor.role==sim::CourierRole::OriginalFoodGrower && !actor.cargo && !actor.reserved &&
            !world.building(row.farm_id).progress,"Grower duplicated legacy cargo, reservation or Farm production progress");
        check(row.spawned==row.retired+static_cast<std::uint64_t>(active(world,row.farm_id)),"Farm has more than one live Grower or lost retirement history");
        if(active(world,row.farm_id))grower_cargo[static_cast<std::size_t>(row.latest_trip->crop)]+=static_cast<std::uint64_t>(world.physical_food_grower_cargo(row.courier_id));
        else check(!world.physical_food_grower_cargo(row.courier_id),"Retired Grower retained physical cargo");
    }
    check(state.next_grower_id==spawned+1 && retired<=spawned,"Physical Grower monotone highwater differs from actual lifetimes");
    origin=returned_crop;
    for(const auto& row:world.fishing_state()->quays)origin[5]+=row.unloaded;
    for(const auto& row:world.hunting_state()->tents)origin[6]+=row.unloaded;
    for(const auto& row:food.stocks) {
        check(volume(row.amounts)<=(world.building(row.building_id).kind==sim::Object::Market?16:8),"Physical Food stock exceeded unchanged capacity");
        for(std::size_t i=0;i<7;++i) {
            check(row.amounts[i]>=0,"Physical Food stock became negative");held[i]+=static_cast<std::uint64_t>(row.amounts[i]);
            used[i]+=row.consumed[i];departed[i]+=row.departed[i];received[i]+=row.received[i];returned_cart[i]+=row.returned[i];
        }
    }
    for(const auto& row:food.trips)for(std::size_t i=0;i<7;++i)cart[i]+=static_cast<std::uint64_t>(row.loaded[i]-row.delivered[i]-row.returned[i]);
    for(std::size_t i=0;i<7;++i) {
        if(i<5)check(picked[i]==returned_crop[i]+grower_cargo[i],"Field harvest was neither physical Grower cargo nor an actual Farm receipt");
        check(origin[i]==held[i]+cart[i]+used[i],"Returned crop/Fish/Meat differs from held/cart/consumed Food");
        check(departed[i]==received[i]+returned_cart[i]+cart[i],"Physical Food departures/arrivals/returns differ");
    }
    const auto channels=world.couriers().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+
        3*world.hunting_state()->habitats.size()+world.grower_navigation_tracks().size();
    check(world.buildings().size()<=82 && channels<=58,"Physical Growers increased shared body/channel limits");
    sim::World::validate_physical_food_growers_snapshot(world.snapshot());
    sim::World::validate_hunting_snapshot(world.snapshot());
}
inline void ticks(sim::World& world,int count){for(int i=0;i<count;++i)world.tick();valid(world);}
template<class Predicate>inline void until(sim::World& world,Predicate done,const char* why,int limit=40000) {
    for(int i=0;i<limit && !done();++i)world.tick();
    check(done(),why);
    valid(world);
}
inline void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,"Refused Grower command changed paid or physical authority");
}
} // namespace oe_physical_food_growers_fixture
