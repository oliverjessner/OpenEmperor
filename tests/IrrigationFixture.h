#pragma once

// Independent authored source geography and model inputs. Residents, staffing,
// money, installations and wet state all arise through paid World commands.
#include "PlantationsFixture.h"
#include "simulation/original/IrrigationRules.h"

namespace oe_irrigation_fixture {
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;

inline std::string authored_model() {
    auto text=p::authored_model();const auto at=text.find("HOUSE MODS\n");
    check(at!=std::string::npos,"Authored irrigation model section missing");
    // Deliberately independent positive prices and staffing. These are not
    // copies of the Original model rows or original-data test fixtures.
    text.insert(at,
        "202,Irrigation Ditch,{,3,0,0,0,0,0,0,0,0,0,0,0,0,},,,\n"
        "203,Irrigation Pump,{,17,0,0,0,0,10,0,0,0,0,0,0,0,},,,\n");
    return text;
}
struct Fixture {
    f::Root root;
    std::vector<std::uint32_t> raw=std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count,0x80U);
    std::vector<std::uint8_t> fertility=std::vector<std::uint8_t>(sim::OriginalTerrainFacts::cell_count,100);
    std::shared_ptr<const sim::MapPermissions> policy;
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    sim::Cell origin{114,98};
    std::uint8_t orientation=0;
    explicit Fixture(std::uint8_t direction=0,sim::Cell at={114,98}):origin(at),orientation(direction) {
        for(const auto cell:orig::pump_geometry(origin,orientation).water)
            raw.at(std::size_t(cell.y)*228U+std::size_t(cell.x))=4;
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",authored_model());
        model=save::prepare_original_irrigation_rules_model(root.data);rebind();
    }
    void rebind() {
        std::vector<sim::MapCellPermission> cells(raw.size());
        for(std::size_t i=0;i<raw.size();++i) {
            const bool land=raw[i]==0x80U,water=raw[i]==4;
            cells[i]={land,land,false,0,land ? sim::BuildBlocker::None:sim::BuildBlocker::UnsupportedTerrain,
                land ? sim::BuildBlocker::None:sim::BuildBlocker::UnsupportedTerrain};
            cells[i].bridge_water=water;
        }
        policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
        const auto encoded=a::encoded_map(raw,fertility);
        f::write_text(root.data/f::map_relative,std::string(encoded.begin(),encoded.end()));
        terrain=save::prepare_original_irrigation_terrain_facts(root.data,f::map_relative);
    }
    std::vector<std::uint8_t> mask() const {return policy->building_mask();}
    sim::World world() const {return sim::World::create_original_irrigation(policy,model,terrain);}
    save::SaveDocument document(const sim::World& world) const {
        return save::make_document(root.data,f::map_relative,mask(),world);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_original_irrigation(document,root.data,mask(),policy,model,terrain);
    }
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_original_irrigation(snapshot,policy,model,terrain);
    }
};
inline sim::BuildingId purchase(sim::World& world,sim::CommandType type,sim::Cell cell) {
    f::buy(world,{type,cell});return *world.building_owner_at(cell);
}
inline sim::BuildingId pump(sim::World& world,sim::Cell origin={114,98},std::uint8_t orientation=0) {
    sim::Command command{sim::CommandType::PlaceIrrigationPump,origin};command.orientation=orientation;
    f::buy(world,command);return *world.building_owner_at(origin);
}
inline void ditch(sim::World& world,sim::Cell cell) {f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cell});}
struct Scene {
    sim::BuildingId pump{},water{};
    std::vector<sim::BuildingId> homes;
};
inline Scene build(sim::World& world,sim::Cell origin={114,98},std::uint8_t orientation=0) {
    // Settlement lies outside every orientation's intake. A short road spur
    // reaches a real land-side entrance, without importing a navigator.
    Scene result;
    for(int x=96;x<=121;++x) f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int y=100;y<104;++y) f::buy(world,{sim::CommandType::PlaceRoad,{117,y}});
    for(int x=96;x<=108;x+=3) result.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,102}));
    result.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{108,105});
    const auto shape=orig::pump_geometry(origin,orientation);
    // Add only permitted cells along the right/bottom perimeter. Orientation0
    // gets the shared default spur at(116,99), joined through117,100.
    if(orientation==0) {
        f::buy(world,{sim::CommandType::PlaceRoad,{116,99}});
        f::buy(world,{sim::CommandType::PlaceRoad,{117,99}});
    } else {
        // The ownership tests exercise all orientations without requiring a
        // universal road planner; functional scenarios use physical view0.
        (void)shape;
    }
    result.pump=pump(world,origin,orientation);return result;
}
inline void valid(const sim::World& world) {
    check(world.population_valid() && world.city_economy_valid() && world.production_balance_valid() &&
        world.navigation_valid() && world.fire_state_valid(),"Paid irrigation World violated shared simulation invariants");
}
inline void ticks(sim::World& world,int count) {for(int i=0;i<count;++i) world.tick();valid(world);}
template<class Predicate>
inline void until(sim::World& world,Predicate done,const char* reason,int limit=16000) {
    for(int i=0;i<limit && !done();++i) world.tick();check(done(),reason);valid(world);
}
inline void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,
        "Refused irrigation command changed paid state, money, identities or chronology");
}
} // namespace oe_irrigation_fixture
