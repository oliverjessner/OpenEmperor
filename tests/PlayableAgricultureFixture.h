#pragma once

// Independent authored source inputs and paid construction only. Food,
// occupants, staffing, trips and quality are earned by actual World ticks.
#include "IrrigationFixture.h"

#include <numeric>

namespace oe_playable_agriculture_fixture {
namespace q=oe_irrigation_fixture;
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using a::field;
using a::stock;
using q::purchase;

inline constexpr std::array<sim::CommandType,5> crop_commands{
    sim::CommandType::PlaceOriginalWheatField,sim::CommandType::PlaceOriginalMilletField,
    sim::CommandType::PlaceOriginalRiceField,sim::CommandType::PlaceOriginalCabbageField,
    sim::CommandType::PlaceOriginalSoybeanField};
inline constexpr std::array<std::uint8_t,5> crop_scores{20,80,60,70,90};

struct Fixture : q::Fixture {
    explicit Fixture(bool with_luxuries=false,int capacity=28) {
        auto text=q::authored_model();
        for(std::size_t row=0;row<3;++row)
            oe_production_fixture::set_house_field(text,row,17,capacity);
        for(const unsigned need:{3U,8U,21U})
            oe_production_fixture::set_house_field(text,2,need,1);
        for(const unsigned need:{11U,12U,13U})
            oe_production_fixture::set_house_field(text,2,need,with_luxuries ? 1:0);
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_irrigation_rules_model(root.data);
    }
    sim::World world() const {return sim::World::create_playable_agriculture(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_playable_agriculture(snapshot,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_playable_agriculture(document,root.data,mask(),policy,model,terrain);
    }
};
inline sim::BuildingId crop(sim::World& world,sim::BuildingId farm,std::size_t index,sim::Cell cell) {
    check(index<crop_commands.size(),"Authored crop command outside five-kind domain");
    sim::Command command{crop_commands[index],cell};command.building_id=farm;
    f::buy(world,command);return *world.building_owner_at(cell);
}
struct Scene {
    std::array<sim::BuildingId,5> farms{},fields{};
    sim::BuildingId market{},water{},ancestor{};
    std::vector<sim::BuildingId> homes,watches;
    std::optional<p::Scene> materials;
};
inline Scene build(sim::World& world,bool with_materials=false) {
    Scene result;
    if(with_materials) {
        result.materials=p::build(world);
        result.market=result.materials->market;result.water=result.materials->water;
        result.ancestor=result.materials->ancestor;result.homes=result.materials->homes;
        result.watches=result.materials->watches;
    } else {
        for(const int y:{24,27})for(int x=19;x<=79;++x)
            f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
        for(const int x:{19,79})for(const int y:{25,26})
            f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
        for(int x=20;x<80;x+=3)
            result.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
        result.market=purchase(world,sim::CommandType::PlaceMarket,{64,25});
        result.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{68,25});
        result.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{72,25});
        for(std::size_t i=0;i<sim::Rules::fire_watch_limit;++i)
            result.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{30+30*static_cast<int>(i),28}));
    }
    for(int x=19;x<=53;++x) f::buy(world,{sim::CommandType::PlaceRoad,{x,20}});
    for(int y=21;y<24;++y) f::buy(world,{sim::CommandType::PlaceRoad,{19,y}});
    for(std::size_t index=0;index<5;++index) {
        const int x=20+7*static_cast<int>(index);
        result.farms[index]=purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{x,17});
        result.fields[index]=crop(world,result.farms[index],index,{x+3,17});
    }
    check(!world.total_population() && !world.taxes_collected_total(),
          "Playable Agriculture construction granted residents or tax");
    return result;
}
inline int volume(const sim::AgricultureAmounts& values) {
    return std::accumulate(values.begin(),values.end(),0);
}
inline unsigned quality(const sim::World& world,sim::BuildingId id) {
    for(const auto& home:world.original_economy_state()->houses)
        if(home.building_id==id)return home.house.food_quality;
    throw std::runtime_error("Paid Agriculture House lost its sole FoodQuality state");
}
inline sim::AgricultureCounters consumed(const sim::World& world) {
    sim::AgricultureCounters values{};
    for(const auto& row:world.original_agriculture_state()->stocks)
        for(std::size_t index=0;index<values.size();++index)values[index]+=row.consumed[index];
    return values;
}
inline void conserved(const sim::World& world) {
    p::valid(world);
    check(world.original_production_state()->food==sim::OriginalFoodState{},
          "Playable Agriculture duplicated its sole Food authority into the historical ledger");
    const auto& food=*world.original_agriculture_state();
    sim::AgricultureCounters produced{},held{},cargo{},used{},departed{},received{},returned{};
    for(const auto& value:food.fields) {
        check(!value.quantity && !value.productive_events && !value.last_harvest_cycle,
              "Authored Food production claimed unproved Original field quantity/work/calendar authority");
        produced[static_cast<std::size_t>(value.crop)]+=value.harvested;
    }
    for(const auto& row:food.stocks) {
        const auto kind=world.building(row.building_id).kind;
        check(volume(row.amounts)<= (kind==sim::Object::Market ? 16:8),
              "Typed Food exceeded its finite shared physical inventory");
        for(std::size_t index=0;index<5;++index) {
            check(row.amounts[index]>=0,"Typed Food inventory became negative");
            std::uint64_t origin=0;
            for(const auto& value:food.fields)
                if(value.farm_id==row.building_id && static_cast<std::size_t>(value.crop)==index)
                    origin+=value.harvested;
            check(origin+row.received[index]+row.returned[index]==
                  static_cast<std::uint64_t>(row.amounts[index])+row.departed[index]+row.consumed[index],
                  "Crop-specific owner origin/arrival/departure/consumption failed");
            held[index]+=static_cast<std::uint64_t>(row.amounts[index]);used[index]+=row.consumed[index];
            departed[index]+=row.departed[index];received[index]+=row.received[index];returned[index]+=row.returned[index];
        }
        if(sim::original_house_kind(kind)) {
            check(volume(row.amounts)>0 || quality(world,row.building_id)==0,
                  "Empty House retained FoodQuality");
            check(!volume(row.amounts) || row.quality_witness.has_value(),
                  "Positive House Food has no actual physical-arrival witness");
        }
    }
    for(const auto& trip:food.trips)for(std::size_t index=0;index<5;++index) {
        const auto remaining=trip.loaded[index]-trip.delivered[index]-trip.returned[index];
        check(remaining>=0,"Food trip created a negative or duplicated cargo");
        cargo[index]+=static_cast<std::uint64_t>(remaining);
    }
    for(std::size_t index=0;index<5;++index) {
        check(produced[index]==held[index]+cargo[index]+used[index],
              "Crop-specific global harvest=stock+cargo+consumption failed");
        check(departed[index]==received[index]+returned[index]+cargo[index],
              "Crop-specific physical departure/arrival/return equation failed");
    }
}
template<class Predicate> inline void until(sim::World& world,Predicate done,const char* reason,int limit=30000) {
    for(int count=0;count<limit && !done();++count)world.tick();
    check(done(),reason);conserved(world);
}
inline void ticks(sim::World& world,int count) {
    for(int tick=0;tick<count;++tick)world.tick();conserved(world);
}
inline void denied(sim::World& world,sim::Command command) {q::denied(world,command);}
} // namespace oe_playable_agriculture_fixture
