#pragma once

// Independent authored terrain/model inputs. Only paid World commands and
// normal ticks earn people, workers, boat catches, Food or delivery history.
#include "PlayableAgricultureFixture.h"
#include "maps/OriginalMapEntities.h"
#include "simulation/original/FishingRules.h"

namespace oe_fishing_fixture {
namespace m=oe_playable_agriculture_fixture;
namespace q=oe_irrigation_fixture;
namespace f=oe_influence_fixture;
namespace a=oe_agriculture_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace maps=openemperor::maps;
using f::check;
inline constexpr sim::Cell quay_origin{114,30},second_quay_origin{122,30};

inline a::Bytes encoded_map(const std::vector<std::uint32_t>& terrain,const std::vector<std::uint8_t>& fertility,
        bool presentation_tiles=false) {
    check(terrain.size()==sim::OriginalTerrainFacts::cell_count && fertility.size()==terrain.size(),
          "Authored Fishing fixture requires complete source planes");
    a::Bytes raw(static_cast<std::size_t>(maps::original_entities_logical_offset)+6U,0);
    raw[static_cast<std::size_t>(maps::original_entities_logical_offset)]=1;
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());a::u32(raw,84,226);
    for(std::size_t i=0;i<terrain.size();++i) {
        a::u32(raw,static_cast<std::size_t>(maps::terrain_logical_offset)+4U*i,terrain[i]);
        if (presentation_tiles) {
            // Same independent one-cell terrain graphic as EconomyStartViewFixture.
            // Explicit UI-only fixture data; production admission remains unchanged.
            a::u32(raw,static_cast<std::size_t>(maps::candidate_word_logical_offset)+4U*i,0xc000U);
            raw[static_cast<std::size_t>(maps::candidate_byte_logical_offset)+i]=64;
        }
    }
    std::copy(fertility.begin(),fertility.end(),raw.begin()+static_cast<std::ptrdiff_t>(maps::fertility_logical_offset));
    a::Bytes bytes(4);a::u32(bytes,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768) {
        const auto size=std::min<std::size_t>(32768,raw.size()-at);
        uLongf length=compressBound(static_cast<uLong>(size));a::Bytes block(length);
        check(compress2(block.data(),&length,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,
              "Authored Fishing source compression failed");
        const auto header=bytes.size();bytes.resize(header+12);a::u32(bytes,header,0x12345678U);
        a::u32(bytes,header+4,static_cast<std::uint32_t>(length));a::u32(bytes,header+8,static_cast<std::uint32_t>(size));
        bytes.insert(bytes.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return bytes;
}
struct Fixture : m::Fixture {
    explicit Fixture(bool lake=true):m::Fixture(false) {
        auto text=f::read_text(root.data/"Model/EmperorBuildingModels.txt");
        const auto at=text.find("HOUSE MODS\n");check(at!=std::string::npos,"Fishing fixture model section missing");
        text.insert(at,"31,Fishing Quay,{,23,0,0,0,0,10,0,0,0,0,0,0,0,},,,\n");
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_playable_fishing_rules_model(root.data);
        raw.assign(sim::OriginalTerrainFacts::cell_count,0x80U);
        if(lake) for(int y=6;y<=30;++y)for(int x=96;x<=134;++x)
            raw[std::size_t(y)*228U+std::size_t(x)]=4U;
        rebind();
    }
    void rebind() {
        std::vector<sim::MapCellPermission> cells(raw.size());
        for(std::size_t i=0;i<raw.size();++i) {
            const bool land=raw[i]==0x80U,water=raw[i]==4U;
            cells[i]={land,land,false,0,land ? sim::BuildBlocker::None:sim::BuildBlocker::UnsupportedTerrain,
                land ? sim::BuildBlocker::None:sim::BuildBlocker::UnsupportedTerrain};
            cells[i].bridge_water=water;
        }
        policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
        const auto encoded=encoded_map(raw,fertility);
        f::write_text(root.data/f::map_relative,std::string(encoded.begin(),encoded.end()));
        terrain=save::prepare_playable_fishing_terrain_facts(root.data,f::map_relative);
    }
    sim::World world() const {return sim::World::create_playable_fishing(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_playable_fishing(snapshot,policy,model,terrain);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_playable_fishing(document,root.data,mask(),policy,model,terrain);
    }
};
inline sim::BuildingId purchase(sim::World& world,sim::CommandType type,sim::Cell cell) {
    f::buy(world,{type,cell});return *world.building_owner_at(cell);
}
inline sim::BuildingId quay(sim::World& world,sim::Cell origin=quay_origin,std::uint8_t orientation=0) {
    sim::Command command{sim::CommandType::PlaceOriginalFishingQuay,origin};command.orientation=orientation;
    f::buy(world,command);return *world.building_owner_at(origin);
}
struct Scene {
    m::Scene crops;
    sim::BuildingId quay{},second_quay{},market{},water{},ancestor{};
    std::vector<sim::BuildingId> homes,watches,quays;
    std::array<sim::BuildingId,5> farms{},fields{};
};
inline Scene build(sim::World& world,bool with_farms=true,bool second=false) {
    Scene result;
    if(with_farms) {
        result.crops=m::build(world,false);result.market=result.crops.market;result.water=result.crops.water;
        result.ancestor=result.crops.ancestor;result.homes=result.crops.homes;result.watches=result.crops.watches;
        result.farms=result.crops.farms;result.fields=result.crops.fields;
    } else {
        for(const int y:{24,27})for(int x=19;x<=79;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
        for(const int x:{19,79})for(const int y:{25,26})f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
        for(int x=20;x<80;x+=3)result.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
        result.market=purchase(world,sim::CommandType::PlaceMarket,{64,25});
        result.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{68,25});
        result.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{72,25});
        for(std::size_t i=0;i<sim::Rules::fire_watch_limit;++i)
            result.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,{30+30*static_cast<int>(i),28}));
    }
    for(int y=28;y<=32;++y)f::buy(world,{sim::CommandType::PlaceRoad,{79,y}});
    for(int x=80;x<=126;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,32}});
    result.quay=quay(world);result.quays.push_back(result.quay);
    if(second) {result.second_quay=quay(world,second_quay_origin);result.quays.push_back(result.second_quay);}
    check(!world.total_population(),"Fishing construction granted residents");
    return result;
}
inline const sim::FishingFoodStock& stock(const sim::World& world,sim::BuildingId owner) {
    for(const auto& row:world.fishing_food_state()->stocks)if(row.building_id==owner)return row;
    throw std::runtime_error("Paid Fishing Food owner lost its sole six-kind account");
}
inline const sim::FishingQuay& source(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.fishing_state()->quays)if(row.quay_id==id)return row;
    throw std::runtime_error("Paid Fishing Quay lost its source identity");
}
template<class Amounts> inline int volume(const Amounts& values) {return std::accumulate(values.begin(),values.end(),0);}
inline std::uint64_t consumed(const sim::World& world,std::size_t resource=sim::fishing_food_index) {
    std::uint64_t value=0;for(const auto& row:world.fishing_food_state()->stocks)value+=row.consumed.at(resource);return value;
}
inline void valid(const sim::World& world) {
    check(world.population_valid() && world.city_economy_valid() && world.production_balance_valid() &&
        world.navigation_valid() && world.fire_state_valid(),"Fishing World shared invariant failure");
    check(world.fishing_food_state() && world.fishing_state() && !world.original_agriculture_state() &&
        world.original_production_state()->food==sim::OriginalFoodState{},"Fishing duplicated its Food authority");
    const auto& food=*world.fishing_food_state();sim::FishingFoodCounters origin{},held{},cargo{},used{},departed{},received{},returned{};
    for(const auto& field:food.fields)origin[static_cast<std::size_t>(field.crop)]+=field.harvested;
    for(const auto& quay:world.fishing_state()->quays)origin[sim::fishing_food_index]+=quay.unloaded;
    for(const auto& row:food.stocks)for(std::size_t i=0;i<6;++i) {
        check(row.amounts[i]>=0,"Fishing Food became negative");held[i]+=static_cast<std::uint64_t>(row.amounts[i]);
        used[i]+=row.consumed[i];departed[i]+=row.departed[i];received[i]+=row.received[i];returned[i]+=row.returned[i];
    }
    for(const auto& trip:food.trips)for(std::size_t i=0;i<6;++i)
        cargo[i]+=static_cast<std::uint64_t>(trip.loaded[i]-trip.delivered[i]-trip.returned[i]);
    for(std::size_t i=0;i<6;++i) {
        check(origin[i]==held[i]+cargo[i]+used[i],"Six-kind Food origin/hold/cargo/consumption mismatch");
        check(departed[i]==received[i]+returned[i]+cargo[i],"Six-kind Food departure/arrival/return mismatch");
    }
    std::uint64_t natural=0,remaining=0,boat_cargo=0;
    for(const auto& site:world.fishing_state()->sites) {natural+=sim::fishing_site_capacity+site.regenerated;remaining+=static_cast<std::uint64_t>(site.stock);}
    for(const auto& boat:world.fishing_state()->boats)boat_cargo+=static_cast<std::uint64_t>(boat.cargo);
    check(natural==remaining+boat_cargo+held[5]+cargo[5]+used[5],"Fishing ecology/boat/Food/consumption conservation mismatch");
}
inline void ticks(sim::World& world,int count) {for(int i=0;i<count;++i)world.tick();valid(world);}
template<class Predicate> inline void until(sim::World& world,Predicate done,const char* why,int limit=20000) {
    for(int i=0;i<limit && !done();++i)world.tick();check(done(),why);valid(world);
}
inline void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,"Refused Fishing command changed World authority");
}
} // namespace oe_fishing_fixture
