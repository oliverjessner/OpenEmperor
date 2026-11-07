#pragma once

// Authored geometry/model inputs for the separate Rule22 contract. Positive
// residents, staff and performer state are earned only through paid commands
// and normal World ticks; no original data is embedded in this fixture.
#include "OriginalProductionFixture.h"
#include "maps/EmperorMap.h"
#include "simulation/OriginalCultureState.h"
#include "simulation/OriginalTerrainFacts.h"
#include "simulation/original/ServiceRules.h"

#include <array>
#include <limits>
#include <zlib.h>

namespace oe_music_venue_fixture {
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace maps=openemperor::maps;
using f::check;
using Bytes=std::vector<std::uint8_t>;
inline constexpr sim::Cell source_cell{110,115},venue_cell{120,115},cut_cell{117,114};

inline void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
inline Bytes encoded_map(const std::vector<std::uint32_t>& terrain) {
    Bytes raw(static_cast<std::size_t>(maps::objects_logical_offset+maps::grid_byte_length));
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());u32(raw,84,226);
    check(terrain.size()==sim::OriginalTerrainFacts::cell_count,"Culture fixture needs a complete source grid");
    for(std::size_t i=0;i<terrain.size();++i)
        u32(raw,static_cast<std::size_t>(maps::terrain_logical_offset)+4*i,terrain[i]);
    Bytes result(4);u32(result,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768) {
        const auto count=std::min<std::size_t>(32768,raw.size()-at);
        uLongf size=compressBound(static_cast<uLong>(count));Bytes block(size);
        check(compress2(block.data(),&size,raw.data()+at,static_cast<uLong>(count),6)==Z_OK,
              "Cannot encode authored culture geography");
        const auto header=result.size();result.resize(header+12);u32(result,header,0x12345678U);
        u32(result,header+4,static_cast<std::uint32_t>(size));u32(result,header+8,static_cast<std::uint32_t>(count));
        result.insert(result.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(size));
    }
    return result;
}
inline std::string authored_model() {
    auto text=oe_production_fixture::authored_model_text(37);
    const auto at=text.find("HOUSE MODS\n");check(at!=std::string::npos,"Culture fixture model section missing");
    text.insert(at,"75,Theatre Pavilion,{,105,0,0,0,0,7,0,0,0,0,0,0,0,},,,\n");
    return text;
}
struct Fixture {
    f::Root root;
    std::vector<std::uint32_t> raw=std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count);
    std::shared_ptr<const sim::MapPermissions> policy;
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    explicit Fixture(const std::string& text=authored_model()) {
        std::vector<sim::MapCellPermission> cells(raw.size(),
            {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
        // One denied interior cell makes a4x4 body test independent of its
        // origin cell. It is outside all successful paid fixture layouts.
        cells[135*228+135]={false,false,false,0,sim::BuildBlocker::LegacyBuildabilityRestriction,
            sim::BuildBlocker::LegacyBuildabilityRestriction};
        policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),
            std::vector<sim::FixedGatePassage>{});
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_culture_rules_model(root.data);
        write_map(raw);terrain=save::prepare_original_terrain_facts(root.data,f::map_relative);
    }
    void write_map(const std::vector<std::uint32_t>& values) const {
        const auto bytes=encoded_map(values);
        f::write_text(root.data/f::map_relative,std::string(bytes.begin(),bytes.end()));
    }
    std::vector<std::uint8_t> mask() const{return policy->building_mask();}
    sim::World world() const{return sim::World::create_original_music_venues(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_original_music_venues(snapshot,policy,model,terrain);
    }
    save::SaveDocument document(const sim::World& world) const {
        return save::make_document(root.data,f::map_relative,mask(),world);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_original_music_venues(document,root.data,mask(),policy,model,terrain);
    }
};
inline const sim::OriginalCultureState& state(const sim::World& world) {
    check(world.original_production_state()&&world.original_production_state()->culture,
          "Rule22 lost its sole culture authority");return *world.original_production_state()->culture;
}
inline const sim::OriginalMusicSchool& school(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:state(world).schools)if(row.building_id==id)return row;
    throw std::runtime_error("Culture fixture lost its paid School");
}
inline const sim::OriginalCultureVenue& venue(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:state(world).venues)if(row.building_id==id)return row;
    throw std::runtime_error("Culture fixture lost its paid Venue");
}
inline sim::BuildingId buy(sim::World& world,sim::CommandType type,sim::Cell cell) {
    f::buy(world,{type,cell});return *world.building_owner_at(cell);
}
struct Scene {
    std::array<sim::BuildingId,3> homes{};
    sim::BuildingId source{},target{};
    sim::CourierId channel{};
};
inline Scene scene(sim::World& world,bool with_venue=true,bool connected=true) {
    Scene result;
    for(int x=110;x<=123;++x)if(connected||x!=117)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    for(std::size_t i=0;i<result.homes.size();++i)
        result.homes[i]=buy(world,sim::CommandType::PlaceHousehold,{110+3*static_cast<int>(i),112});
    result.source=buy(world,sim::CommandType::PlaceOriginalMusicSchool,source_cell);
    result.channel=school(world,result.source).courier_id;
    if(with_venue)result.target=buy(world,sim::CommandType::PlaceOriginalTheatre,venue_cell);
    check(world.total_population()==0&&world.taxes_collected_total()==0&&world.maintenance_spent_total()==0,
          "Paid culture construction granted residents or recurring money");
    return result;
}
// Independent phase35 clock, expressed in completed World ticks. No production
// service countdown helper is used as the expected-result oracle.
inline std::uint64_t countdowns(std::uint64_t tick){return tick<36?0:1+(tick-36)/51;}
inline std::uint8_t expected_timer(std::uint64_t now,std::uint64_t arrival) {
    check(arrival&&arrival<=now,"Culture timer oracle needs an actual past arrival");
    const auto elapsed=countdowns(now)-countdowns(arrival-1);
    return elapsed>=32?0:static_cast<std::uint8_t>(32-elapsed);
}
inline void invariants(const sim::World& world) {
    check(world.population_valid()&&world.navigation_valid()&&world.city_economy_valid()&&
          world.production_balance_valid()&&world.fire_state_valid(),"Culture World invariant failed");
    const auto& culture=state(world);
    check(culture.schools.size()<=2&&culture.venues.size()<=2&&world.buildings().size()<=82&&world.couriers().size()<=58,
          "Culture raised retained body/channel budgets");
    std::uint64_t upkeep=0,taxes=0,spawned=0;
    for(const auto& body:world.buildings()) {
        const auto workers=world.workforce_required(body.id);
        check(workers>=0&&body.placed_tick<=world.ticks(),"Culture installed workforce/age is invalid");
        upkeep+=(world.ticks()-body.placed_tick)/816*2*static_cast<std::uint64_t>(workers);
        taxes+=body.taxes_paid_total;
    }
    check(upkeep==world.maintenance_spent_total()&&taxes==world.taxes_collected_total()&&
          world.treasury()==12000+static_cast<std::int64_t>(taxes)-static_cast<std::int64_t>(upkeep)-
              static_cast<std::int64_t>(world.construction_spent_total()),
          "Culture changed actual paid construction/installed upkeep/tax accounting");
    for(const auto& row:culture.schools) {
        spawned+=row.spawned;const auto& channel=world.courier(row.courier_id);
        check(channel.owner==row.building_id&&channel.role==sim::CourierRole::OriginalMusicPerformer&&
              !channel.cargo&&!channel.reserved&&!world.original_courier_cargo(channel.id),
              "School movement channel gained material cargo or another identity");
    }
    check(culture.next_performer_id==spawned+1,"Successful spawn IDs differ from the actual highwater");
    for(const auto& row:culture.venues) {
        const auto expected=row.last_arrival?expected_timer(world.ticks(),row.last_arrival->arrival_tick):0;
        check(row.music_timer==expected,"Venue32 timer differs from actual-arrival independent clock");
    }
    for(const auto& row:world.original_economy_state()->houses)
        check(!orig::service_timer(row.house,orig::ServiceRole::Music),"Performer/Venue granted an unsupported House96 effect");
}
inline void through(sim::World& world,std::uint64_t tick) {
    check(world.ticks()<=tick,"Culture fixture exceeded its intended time boundary");
    while(world.ticks()<tick)world.tick();invariants(world);
}
template<class P>inline void until(sim::World& world,P done,const std::string& message,int limit=1200) {
    for(int tick=0;tick<limit&&!done();++tick)world.tick();
    check(done(),message+" at tick="+std::to_string(world.ticks())+" Funds="+std::to_string(world.treasury())+
          " population="+std::to_string(world.total_population()));invariants(world);
}
inline bool active(const sim::World& world,sim::BuildingId source) {
    const auto& row=school(world,source);return row.latest_trip&&!row.latest_trip->arrival_tick;
}
template<class F>inline void refuses(F action,const std::string& reason) {
    try{action();}catch(const std::exception&){return;}throw std::runtime_error(reason);
}
} // namespace oe_music_venue_fixture
