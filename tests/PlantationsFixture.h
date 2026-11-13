#pragma once

// Entirely authored paid scenario inputs. No Original map/model data, stock,
// resident, harvest or worker state is copied or injected into a live World.
#include "AgricultureFixture.h"

namespace oe_plantations_fixture {
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using f::check;

inline std::string authored_model() {
    auto text=a::authored_model();
    // Explicit independent positive controls: actual paid households can earn
    // workers and consume the three real luxuries while Food stays unproduced.
    for(std::size_t row=0;row<3;++row) {
        oe_production_fixture::set_house_field(text,row,0,-99);
        oe_production_fixture::set_house_field(text,row,1,row==2 ? 100:-5);
        oe_production_fixture::set_house_field(text,row,3,0);
        oe_production_fixture::set_house_field(text,row,8,0);
        oe_production_fixture::set_house_field(text,row,17,28);
        oe_production_fixture::set_house_field(text,row,21,0);
    }
    for(const unsigned need:{11U,12U,13U})
        oe_production_fixture::set_house_field(text,2,need,1);
    return text;
}

struct Fixture : a::Fixture {
    Fixture() {
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",authored_model());
        model=save::prepare_original_agriculture_rules_model(root.data);
    }
    sim::World world() const {return sim::World::create_original_plantations(policy,model,terrain);}
    save::SaveDocument document(const sim::World& world) const {
        return save::make_document(root.data,f::map_relative,mask(),world);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_original_plantations(document,root.data,mask(),policy,model,terrain);
    }
    sim::World restore(const sim::WorldSnapshot& snapshot) const {
        return sim::World::restore_original_plantations(snapshot,policy,model,terrain);
    }
};

inline sim::BuildingId purchase(sim::World& world,sim::CommandType type,sim::Cell cell) {
    f::buy(world,{type,cell});return *world.building_owner_at(cell);
}
inline sim::BuildingId plant(sim::World& world,sim::CommandType type,
                             sim::BuildingId source,sim::Cell cell) {
    sim::Command command{type,cell};command.building_id=source;
    f::buy(world,command);return *world.building_owner_at(cell);
}
struct Scene {
    sim::BuildingId tea_source{},lacquer_source{},mulberry_source{};
    sim::BuildingId tea_plant{},lacquer_plant{},mulberry_plant{};
    sim::BuildingId warehouse{},market{},logging{},lacquerware{},weaver{},water{},ancestor{};
    std::vector<sim::BuildingId> homes,watches;
};
inline Scene build(sim::World& world) {
    Scene scene;
    for(const int y:{24,27})for(int x=19;x<=79;++x)
        f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
    for(const int x:{19,79})for(const int y:{25,26})
        f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
    for(int x=20;x<80;x+=3)
        scene.homes.push_back(purchase(world,sim::CommandType::PlaceHousehold,{x,22}));
    scene.tea_source=purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,{22,25});
    scene.lacquer_source=purchase(world,sim::CommandType::PlaceOriginalLacquerRefinery,{32,25});
    scene.mulberry_source=purchase(world,sim::CommandType::PlaceOriginalSilkwormShed,{42,25});
    // These are separately bought, nonadjacent plants at the inclusive +4
    // source-range boundary; the worker must travel over actual land to them.
    scene.tea_plant=plant(world,sim::CommandType::PlaceOriginalTeaBush,scene.tea_source,{26,29});
    scene.lacquer_plant=plant(world,sim::CommandType::PlaceOriginalLacquerTree,scene.lacquer_source,{36,29});
    scene.mulberry_plant=plant(world,sim::CommandType::PlaceOriginalMulberryTree,scene.mulberry_source,{46,29});
    scene.logging=purchase(world,sim::CommandType::PlaceOriginalLoggingShed,{52,25});
    scene.warehouse=purchase(world,sim::CommandType::PlaceWarehouse,{58,25});
    scene.market=purchase(world,sim::CommandType::PlaceMarket,{64,25});
    scene.water=purchase(world,sim::CommandType::PlaceOriginalWaterWell,{68,25});
    scene.ancestor=purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{72,25});
    scene.lacquerware=purchase(world,sim::CommandType::PlaceOriginalLacquerwareMaker,{54,28});
    scene.weaver=purchase(world,sim::CommandType::PlaceOriginalWeaver,{64,28});
    for(std::size_t index=0;index<sim::Rules::fire_watch_limit;++index)
        scene.watches.push_back(purchase(world,sim::CommandType::PlaceFireWatch,
            {30+30*static_cast<int>(index),28}));
    check(!world.total_population() && !world.taxes_collected_total(),
          "Paid plantation construction granted residents or income");
    return scene;
}
inline const sim::PlantationPlant& plant(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.plantations_state()->plants)if(row.plant_id==id)return row;
    throw std::runtime_error("Actual paid plantation plant lost its authoritative state");
}
inline const sim::PlantationSource& source(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.plantations_state()->sources)if(row.building_id==id)return row;
    throw std::runtime_error("Actual paid plantation source lost its worker channel");
}
inline const sim::OriginalIndustrySource& industry(const sim::World& world,sim::BuildingId id) {
    for(const auto& row:world.original_production_state()->industry.sources)if(row.building_id==id)return row;
    throw std::runtime_error("Actual paid plantation source lost its sole industry account");
}
inline int auxiliary(const sim::World& world,sim::BuildingId id,sim::OriginalAuxMaterial material) {
    for(const auto& row:world.original_production_state()->industry.auxiliary)
        if(row.building_id==id)return row.amounts[static_cast<std::size_t>(material)];
    return 0;
}
inline void valid(const sim::World& world) {
    check(world.population_valid() && world.city_economy_valid() &&
          world.production_balance_valid() && world.navigation_valid() && world.fire_state_valid(),
          "Actual plantation World violated population/accounting/navigation/fire invariants");
    for(std::size_t good=0;good<orig::goods_count;++good) {
        const auto balance=world.original_goods_balance(static_cast<orig::Goods>(good));
        check(balance.supported && balance.conserved && !balance.totals.imported && !balance.totals.demolished,
              "Plantation six-good conservation or paid origin failed");
    }
    for(std::size_t material=0;material<sim::original_aux_material_count;++material)
        check(world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(material)).conserved,
              "Plantation auxiliary material conservation failed");
}
template<class Predicate> inline void until(sim::World& world,Predicate done,const char* reason,int limit=16000) {
    for(int tick=0;tick<limit && !done();++tick)world.tick();
    check(done(),reason);valid(world);
}
inline void ticks(sim::World& world,int count) {
    for(int tick=0;tick<count;++tick)world.tick();
    valid(world);
}
inline void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,
          "Refused plantation action changed paid authority, IDs, goods or Funds");
}
} // namespace oe_plantations_fixture
