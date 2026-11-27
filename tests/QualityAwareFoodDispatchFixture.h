#pragma once

// Independent authored sources. Positive stocks and arrivals are earned only
// by paid construction, immigration, staffed work and actual World transport.
#include "IrrigatedHarvestFixture.h"

namespace oe_quality_aware_food_dispatch_fixture {
namespace inherited=oe_irrigated_harvest_fixture;
namespace q=inherited::q;
namespace f=inherited::f;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using inherited::until;
using inherited::ticks;
using inherited::build_early_food;
using inherited::r::stock;

struct Fixture:inherited::Fixture {
    explicit Fixture(int common_food_need=0){
        if(common_food_need){
            auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
            for(std::size_t row=0;row<8;++row)oe_production_fixture::set_house_field(text,row,8,common_food_need);
            f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
            model=save::prepare_food_processing_rules_model(root.data);
        }
    }
    sim::World world()const{return sim::World::create_quality_aware_food_dispatch(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const {
        return sim::World::restore_quality_aware_food_dispatch(value,policy,model,terrain);
    }
};
inline const sim::ProcessedFoodTrip& trip(const sim::World& world,sim::CourierId id){
    for(const auto& row:world.processed_food_state()->trips)if(row.transfer.courier_id==id)return row;
    throw std::runtime_error("Paid raw distributor has no real transfer history");
}
inline const orig::HouseState& house(const sim::World& world,sim::BuildingId id){
    for(const auto& row:world.original_economy_state()->houses)if(row.building_id==id)return row.house;
    throw std::runtime_error("Paid raw recipient lost its actual House state");
}
using Scene=q::n::Scene;
inline Scene build_raw(sim::World& world,std::size_t kind=4){
    const std::array kinds{kind};return q::build(world,kinds);
}
struct LoadedRaw {Scene city;sim::CourierId courier{};sim::BuildingId target{};sim::OriginalGoodsTransferKey transfer{};};
inline LoadedRaw earn_loaded_raw(sim::World& world,std::size_t kind=4){
    LoadedRaw result;result.city=build_raw(world,kind);
    for(const auto& actor:world.couriers())if(actor.owner==result.city.market&&actor.role==sim::CourierRole::OriginalFoodToHouse)result.courier=actor.id;
    check(result.courier!=sim::CourierId{},"Paid Market lacks its original raw distributor");
    until(world,[&]{return world.courier(result.courier).phase==sim::CourierPhase::ToWarehouse;},
        "Paid source never earned an actual qualified loaded Market-to-House trip",9000);
    const auto& actual=trip(world,result.courier);result.target=actual.target;result.transfer=actual.transfer;
    check(actual.phase==sim::OriginalGoodsTransferPhase::Loaded&&actual.source==result.city.market&&
        house(world,result.target).occupants>0&&sim::food_batch::volume(actual.loaded)>0,
        "Raw dispatch lacks its actual paid source, occupied target or physical cargo");
    return result;
}
inline LoadedRaw earn_loaded_mixed_raw(sim::World& world){
    LoadedRaw result;result.city=build_raw(world,0);
    // First earn low-quality stock through the complete paid source/cart leg.
    // An ordinary paid-field operation command then changes the real crop mix;
    // no lifetime work, live trip, stock or earlier receipt is edited.
    until(world,[&]{return stock(world,result.city.market).amounts[0]>0;},
        "Paid mixed-load scene never physically supplied Wheat to its Market",4000);
    f::buy(world,sim::set_building_operation(result.city.fields.front(),false));
    result.city.fields.push_back(q::crop(world,result.city.farm,4,{93,111}));
    for(const auto& actor:world.couriers())if(actor.owner==result.city.market&&actor.role==sim::CourierRole::OriginalFoodToHouse)result.courier=actor.id;
    check(result.courier!=sim::CourierId{},"Paid mixed Market lacks a raw distributor");
    until(world,[&]{return world.courier(result.courier).phase==sim::CourierPhase::ToWarehouse;},
        "Paid mixed-load scene never earned a qualified actual raw dispatch",9000);
    const auto& actual=trip(world,result.courier);result.target=actual.target;result.transfer=actual.transfer;
    auto available=stock(world,result.city.market).amounts;
    for(std::size_t g=0;g<available.size();++g)available[g]+=actual.loaded[g];
    const auto historical=sim::food_batch::mixed_portions(available,static_cast<std::uint64_t>(std::min(4,sim::food_batch::volume(available))));
    const auto need=world.original_rules_model()->house_rules().value(orig::house_row(house(world,result.target).type),orig::HouseField::FoodQuality);
    check(stock(world,result.city.market).amounts[0]>0&&actual.loaded[4]>0&&!actual.loaded[0]&&
        sim::agriculture_batch_quality(historical).value<need&&sim::agriculture_batch_quality(actual.loaded).value>=need,
        "Paid mixed-load scene did not isolate an unsafe historical load and a real qualified subset");
    return result;
}
inline void valid(const sim::World& world){
    check(world.rule_version()==38&&world.quality_aware_food_dispatch_enabled()&&world.agriculture_reconfiguration_enabled()&&
        world.food_warehouses_enabled()&&world.processed_food_state()&&!world.original_agriculture_state()&&
        !world.hunting_food_state()&&!world.fishing_food_state()&&world.original_production_state()->food==sim::OriginalFoodState{},
        "Quality-aware dispatch lost its inherited sole Food authority");
    check(world.population_valid()&&world.city_economy_valid()&&world.production_balance_valid()&&
        world.navigation_valid()&&world.fire_state_valid(),"Quality-aware dispatch broke shared paid World invariants");
    const auto state=world.snapshot();sim::World::validate_original_production_snapshot(state);
    sim::World::validate_irrigation_productivity_snapshot(state);sim::World::validate_seasonal_agriculture_snapshot(state);
    sim::World::validate_physical_food_growers_snapshot(state);sim::World::validate_agriculture_reconfiguration_snapshot(state);
}
} // namespace oe_quality_aware_food_dispatch_fixture
