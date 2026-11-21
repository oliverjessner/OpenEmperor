#pragma once

// Independent source/model inputs. Paid commands, normal production, real
// transports and staffed work are the only origins of ingredients and meals.
#include "PhysicalFoodGrowersFixture.h"
#include "simulation/FoodProcessingState.h"

namespace oe_food_processing_fixture {
namespace q=oe_physical_food_growers_fixture;
namespace h=oe_hunting_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using f::check;
using q::purchase;
using q::crop;
using q::volume;
struct Fixture:q::Fixture {
    explicit Fixture(int capacity=28):q::Fixture(false,capacity){
        auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
        const auto at=text.find("HOUSE MODS\n");check(at!=std::string::npos,"Processing fixture lacks its model section");
        // Deliberately authored prices/workforce differ from the user's model.
        text.insert(at,"53,Mill,{,91,0,0,0,0,16,0,0,0,0,0,0,0,},,,\n66,Food Shop,{,19,0,0,0,0,4,0,0,0,0,0,0,0,},,,\n");
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_food_processing_rules_model(root.data);
    }
    sim::World world()const{return sim::World::create_food_processing(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& value)const{return sim::World::restore_food_processing(value,policy,model,terrain);}
    sim::World restore(const save::SaveDocument& value)const{return save::restore_food_processing(value,root.data,mask(),policy,model,terrain);}
};
inline const sim::ProcessedFoodStock& stock(const sim::World& world,sim::BuildingId id){
    for(const auto& row:world.processed_food_state()->stocks)if(row.building_id==id)return row;
    throw std::runtime_error("Paid owner lacks its sole graded Food account");
}
inline const sim::MillProcessing& mill(const sim::World& world,sim::BuildingId id){
    for(const auto& row:world.food_processing_state()->mills)if(row.mill_id==id)return row;
    throw std::runtime_error("Paid Mill lacks its committed recipe history");
}
struct Scene:q::n::Scene {sim::BuildingId mill{},shop{};};
inline Scene build(sim::World& world,std::span<const std::size_t> crops,bool watches=true){
    Scene city;static_cast<q::n::Scene&>(city)=q::build(world,crops,watches);
    city.mill=purchase(world,sim::CommandType::PlaceOriginalMill,{119,110});
    city.shop=purchase(world,sim::CommandType::PlaceOriginalFoodShop,{98,110});
    check(world.food_processing_state()->shops.back().market_id==city.market,"Paid FoodShop lost its actual parent Market");
    return city;
}
inline Scene build(sim::World& world,bool watches=true){constexpr std::array<std::size_t,5> crops{0,1,2,3,4};return build(world,crops,watches);}
inline std::uint64_t meals_consumed(const sim::World& world){std::uint64_t result=0;
    for(const auto& row:world.processed_food_state()->stocks)if(sim::original_house_kind(world.building(row.building_id).kind))
        for(std::size_t g=7;g<12;++g)result+=row.consumed[g];
    return result;}
inline std::uint64_t meals_produced(const sim::World& world){std::uint64_t result=0;for(const auto& m:world.food_processing_state()->mills)
    for(std::size_t g=0;g<4;++g)result+=m.completed_by_grade[g]*(g+1);
    return result;}
inline void valid(const sim::World& world){
    check(world.rule_version()==34 && world.food_processing_enabled() && world.processed_food_state()&&world.food_processing_state()&&
        !world.hunting_food_state()&&!world.fishing_food_state()&&!world.original_agriculture_state()&&
        world.original_production_state()->food==sim::OriginalFoodState{},"Processing duplicated or projected Food authority");
    check(world.population_valid()&&world.city_economy_valid()&&world.production_balance_valid()&&world.navigation_valid()&&world.fire_state_valid(),
        "Processing broke shared World invariants");
    check(world.buildings().size()<=82&&world.couriers().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size()<=58,
        "Processing raised shared body or movement budgets");
    sim::ProcessedFoodCounters origin{},held{},cargo{},used{},departed{},received{},returned{};
    for(const auto& field:world.physical_food_growers_state()->fields){const auto at=std::find_if(world.agriculture_fields().begin(),world.agriculture_fields().end(),[&](const auto& paid){return paid.field_id==field.field_id;});check(at!=world.agriculture_fields().end(),"Physical receipt lost its paid crop");origin[static_cast<std::size_t>(at->crop)]+=field.returned;}
    for(const auto& quay:world.fishing_state()->quays)origin[5]+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)origin[6]+=tent.unloaded;
    for(const auto& m:world.food_processing_state()->mills)for(std::size_t g=0;g<4;++g)origin[7+g]+=m.completed_by_grade[g]*(g+1);
    for(const auto& row:world.processed_food_state()->stocks)for(std::size_t g=0;g<12;++g){
        check(row.amounts[g]>=0,"Processing stock became negative");held[g]+=static_cast<unsigned>(row.amounts[g]);used[g]+=row.consumed[g];departed[g]+=row.departed[g];received[g]+=row.received[g];returned[g]+=row.returned[g];}
    for(const auto& trip:world.processed_food_state()->trips)for(std::size_t g=0;g<12;++g)cargo[g]+=static_cast<std::uint64_t>(trip.loaded[g]-trip.delivered[g]-trip.returned[g]);
    for(std::size_t g=0;g<12;++g){check(origin[g]==held[g]+cargo[g]+used[g],"Earned raw/meal origin does not equal held+transit+actually consumed");
        check(departed[g]==received[g]+returned[g]+cargo[g],"Physical Food debit differs from arrival+return+actual cargo");}
    check(!origin[11]&&!held[11]&&!cargo[11]&&!used[11],"Processing invented unavailable Salt/Spices grade5");
    sim::World::validate_food_processing_snapshot(world.snapshot());
}
template<class Predicate> void until(sim::World& world,Predicate predicate,const std::string& why,int limit=14000){
    for(int n=0;n<limit&&!predicate();++n)world.tick();check(predicate(),why+" tick="+std::to_string(world.ticks()));
}
inline void ticks(sim::World& world,int n){for(int i=0;i<n;++i)world.tick();}
} // namespace oe_food_processing_fixture
