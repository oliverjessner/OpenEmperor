#pragma once

// Independently authored map/model inputs; paid commands and real World work
// alone earn water, rational carry, growth, cargo and source receipts.
#include "SeasonalAgricultureFixture.h"
#include "simulation/IrrigationProductivityState.h"

namespace oe_irrigation_productivity_fixture {
namespace s=oe_seasonal_agriculture_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace p=oe_plantations_fixture;
namespace i=oe_irrigation_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using q::purchase;
using q::crop;
using q::farm;
using q::worker;
using q::active;
using q::phase;
using q::receipt_field;
using q::consumed;
using q::volume;
inline constexpr sim::Cell pump_origin{85,92};

struct Fixture:s::Fixture {
    explicit Fixture(int capacity=28):s::Fixture(capacity) {
        for(const auto cell:orig::pump_geometry(pump_origin,0).water)
            raw.at(std::size_t(cell.y)*228U+std::size_t(cell.x))=4U;
        rebind();
    }
    sim::World world()const{return sim::World::create_irrigation_productivity(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const {
        return sim::World::restore_irrigation_productivity(value,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& value)const {
        return save::restore_irrigation_productivity(value,root.data,mask(),policy,model,terrain);
    }
};
struct Network {sim::BuildingId pump{};std::vector<sim::Cell> cells;};
inline Network network(sim::World& world,bool operating=true,bool inspector_road=true) {
    Network result;result.pump=i::pump(world,pump_origin);
    // Real Inspector access to the land-side Pump entrance. The y94 crossing
    // is perpendicular to the horizontal ditch, never a parallel overlay.
    if(inspector_road) {
        const auto road=[&](sim::Cell cell){if(!world.road_cell(cell))f::buy(world,{sim::CommandType::PlaceRoad,cell});};
        for(int y=93;y<=109;++y)road({87,y});
        for(const int y:{104,109})for(int x=88;x<=89;++x)road({x,y});
    }
    for(int x=85;x<=149;++x)result.cells.push_back({x,94});
    for(int y=95;y<=114;++y)result.cells.push_back({85,y});
    for(int x=86;x<=149;++x)result.cells.push_back({x,114});
    const auto batch=world.execute_irrigation_batch(result.cells);
    check(batch.accepted && batch.changed && result.cells.size()>=21,
        "Paid productivity network failed at tick="+std::to_string(world.ticks())+" bodies="+std::to_string(world.buildings().size())+": "+batch.reason);
    if(!operating)f::buy(world,sim::set_building_operation(result.pump,false));
    return result;
}
struct Scene:s::Scene {Network water;};
inline Scene build(sim::World& world,bool supplied=true) {
    Scene result;result.water=network(world,supplied);
    // Buy the first real Watch next to the Pump, before the city's longer
    // canonical target list. A distant material-city Watch is not protection.
    result.materials.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{86,96}));
    const auto road=[&](sim::Cell cell){if(!world.road_cell(cell))f::buy(world,{sim::CommandType::PlaceRoad,cell});};
    for(const int y:{104,107})for(int x=89;x<=149;++x)road({x,y});
    for(const int x:{89,149})for(const int y:{105,106})road({x,y});
    for(int x=90;x<150;x+=3)result.materials.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,102}));
    auto& m=result.materials;
    m.tea_source=purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,{92,105});
    m.lacquer_source=purchase(world,sim::CommandType::PlaceOriginalLacquerRefinery,{102,105});
    m.mulberry_source=purchase(world,sim::CommandType::PlaceOriginalSilkwormShed,{112,105});
    m.tea_plant=p::plant(world,sim::CommandType::PlaceOriginalTeaBush,m.tea_source,{96,109});
    m.lacquer_plant=p::plant(world,sim::CommandType::PlaceOriginalLacquerTree,m.lacquer_source,{106,109});
    m.mulberry_plant=p::plant(world,sim::CommandType::PlaceOriginalMulberryTree,m.mulberry_source,{116,109});
    m.logging=purchase(world,sim::CommandType::PlaceOriginalLoggingShed,{122,105});
    m.warehouse=purchase(world,sim::CommandType::PlaceWarehouse,{128,105});
    m.market=purchase(world,sim::CommandType::PlaceMarket,{134,105});
    m.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{138,105});
    m.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{142,105});
    m.lacquerware=purchase(world,sim::CommandType::PlaceOriginalLacquerwareMaker,{124,108});
    m.weaver=purchase(world,sim::CommandType::PlaceOriginalWeaver,{134,108});
    m.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{130,108}));
    for(int x=89;x<=149;++x)road({x,100});
    for(int y=101;y<104;++y)road({89,y});
    for(std::size_t k=0;k<5;++k){const int x=90+7*static_cast<int>(k);
        result.farms[k]=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{x,97});
        result.fields[k]=crop(world,result.farms[k],k,{x+3,97});
    }
    result.hemp_farm=purchase(world,sim::CommandType::PlaceOriginalHempFarm,{132,98});
    result.hemp_field=p::plant(world,sim::CommandType::PlaceOriginalHempField,result.hemp_farm,{134,98});
    check(!world.total_population()&&!world.taxes_collected_total(),"Paid supplied/dry cohort gained free population or tax");
    return result;
}
inline const sim::IrrigationProductivityWork& work(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.irrigation_productivity_state()->work)if(row.building_id==id)return row;
    throw std::runtime_error("Paid productive field lost its sole irrigation work row");
}
inline std::uint64_t base_work(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.multi_field_agriculture_state()->fields)if(row.field_id==id)return row.productive_ticks;
    for(const auto& row:world.seasonal_agriculture_state()->hemp_farms)
        if(s::hemp_origin(world,row.farm_id).field_id==id)return row.productive_ticks;
    for(const auto& row:world.plantations_state()->plants)if(row.plant_id==id)return row.productive_ticks;
    throw std::runtime_error("Irrigation work row has no uniquely paid growth owner");
}
inline std::uint64_t earned_work(const sim::World& world,sim::BuildingId id) {
    return base_work(world,id)+work(world,id).bonus_work;
}
inline std::uint64_t food_progress(const sim::World& world,sim::BuildingId id) {
    return earned_work(world,id)-s::field(world,id).wasted_work-80*q::field(world,id).harvested;
}
inline std::vector<sim::BuildingId> subjects(const s::Scene& city) {
    std::vector<sim::BuildingId> result(city.fields.begin(),city.fields.end());
    result.push_back(city.hemp_field);result.push_back(city.materials.tea_plant);
    result.push_back(city.materials.lacquer_plant);result.push_back(city.materials.mulberry_plant);return result;
}
inline void valid(const sim::World& world) {
    p::valid(world);
    check(world.rule_version()==33 && world.irrigation_productivity_state() && world.seasonal_agriculture_state() &&
        world.physical_food_growers_state() && world.hunting_food_state() && !world.original_agriculture_state() &&
        !world.fishing_food_state() && world.original_production_state()->food==sim::OriginalFoodState{},
        "Irrigation productivity lost explicit lineage or duplicated the sole Food inventory");
    std::vector<sim::BuildingId> expected;
    for(const auto& row:world.hunting_food_state()->fields)expected.push_back(row.field_id);
    for(const auto& row:world.original_production_state()->hemp_farms)if(row.field_id)expected.push_back(*row.field_id);
    for(const auto& row:world.plantations_state()->plants)expected.push_back(row.plant_id);
    std::sort(expected.begin(),expected.end());std::vector<sim::BuildingId> actual;
    for(const auto& row:world.irrigation_productivity_state()->work) {
        actual.push_back(row.building_id);
        check(row.wet_work_ticks<=base_work(world,row.building_id) && row.wet_work_ticks/4==row.bonus_work+row.clipped_bonus,
            "Wet work, earned quarter bonus and clipped cap equation failed");
    }
    check(actual==expected,"Irrigation work rows are not the exact canonical paid field/plant identities");
    sim::HuntingFoodCounters picked{},returned{},grower_cargo{},held{},cart{},used{},departed{},received{},cart_returned{};
    for(const auto& row:world.hunting_food_state()->fields) {
        const auto kind=static_cast<std::size_t>(row.crop);picked[kind]+=row.harvested;
        returned[kind]+=receipt_field(world,row.field_id).returned;
        const auto& season=s::field(world,row.field_id);const auto progress=food_progress(world,row.field_id);
        check(progress<=80 && season.ready_tick.has_value()==(progress==80) &&
            season.harvest_work==5*row.harvested+season.wasted_harvest_work+season.harvest_progress && season.harvest_progress<5,
            "Irrigated Food growth/readiness/actual harvest-work equation failed");
    }
    for(const auto& row:world.physical_food_growers_state()->farms)if(active(world,row.farm_id))
        grower_cargo[static_cast<std::size_t>(worker(world,row.farm_id).crop)]+=static_cast<std::uint64_t>(world.physical_food_grower_cargo(row.courier_id));
    for(std::size_t k=0;k<5;++k)check(picked[k]==returned[k]+grower_cargo[k],"Irrigation bonus became Food before physical Farm return");
    for(const auto& row:world.fishing_state()->quays)returned[5]+=row.unloaded;
    for(const auto& row:world.hunting_state()->tents)returned[6]+=row.unloaded;
    for(const auto& row:world.hunting_food_state()->stocks) {
        check(volume(row.amounts)<=(world.building(row.building_id).kind==sim::Object::Market?16:8),"Irrigated Food exceeded unchanged bin capacity");
        for(std::size_t k=0;k<7;++k){check(row.amounts[k]>=0,"Irrigated Food became negative");held[k]+=static_cast<std::uint64_t>(row.amounts[k]);
            used[k]+=row.consumed[k];departed[k]+=row.departed[k];received[k]+=row.received[k];cart_returned[k]+=row.returned[k];}
    }
    for(const auto& row:world.hunting_food_state()->trips)for(std::size_t k=0;k<7;++k)cart[k]+=static_cast<std::uint64_t>(row.loaded[k]-row.delivered[k]-row.returned[k]);
    for(std::size_t k=0;k<7;++k){check(returned[k]==held[k]+cart[k]+used[k],"Irrigated physical Food conservation failed");
        check(departed[k]==received[k]+cart_returned[k]+cart[k],"Irrigated Food actual transfer equation failed");}
    for(const auto& row:world.plantations_state()->plants) {
        const auto& season=s::plant(world,row.plant_id);
        check(earned_work(world,row.plant_id)==80*row.harvested+season.wasted_work+row.growth &&
            row.grown==row.harvested+season.expired_ready+static_cast<unsigned>(season.ready_tick.has_value()) &&
            season.ready_tick.has_value()==(row.growth==80),"Irrigated plantation base/bonus/picked/expiry equation failed");
    }
    for(const auto& row:world.seasonal_agriculture_state()->hemp_farms)if(const auto field=s::hemp_origin(world,row.farm_id).field_id) {
        const auto progress=world.building(row.farm_id).progress;
        check(earned_work(world,*field)==80*row.picked+row.wasted_work+static_cast<unsigned>(progress) &&
            row.grown==row.picked+row.expired_ready+static_cast<unsigned>(row.ready_tick.has_value()) &&
            row.ready_tick.has_value()==(progress==80) && row.picked==s::hemp_origin(world,row.farm_id).harvested+
                static_cast<std::uint64_t>(world.seasonal_hemp_grower_cargo(row.courier_id)),"Irrigated Hemp base/bonus/ready/return equation failed");
    }
    check(world.buildings().size()<=82 && world.couriers().size()+world.fishing_state()->quays.size()+
        world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size()<=58,"Irrigation productivity expanded body/channel limits");
    sim::World::validate_irrigation_productivity_snapshot(world.snapshot());
    sim::World::validate_seasonal_agriculture_snapshot(world.snapshot());
    sim::World::validate_physical_food_growers_snapshot(world.snapshot());
}
inline void ticks(sim::World& world,std::uint64_t count){for(std::uint64_t t=0;t<count;++t)world.tick();valid(world);}
inline void through(sim::World& world,std::uint64_t tick){check(world.ticks()<=tick,"Productivity boundary already passed");ticks(world,tick-world.ticks());}
template<class Predicate>inline void until(sim::World& world,Predicate done,const char* why,int limit=40000){
    for(int t=0;t<limit&&!done();++t)world.tick();check(done(),why);valid(world);
}
inline void denied(sim::World& world,sim::Command command){const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted&&!result.changed&&world.snapshot()==before,"Rejected productivity command changed paid authority");}
} // namespace oe_irrigation_productivity_fixture
