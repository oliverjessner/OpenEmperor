#pragma once

// Independent authored source inputs. Every work unit, receipt and resident is
// earned by the shared paid World; retired rows never supply a second inventory.
#include "FoodWarehouseFixture.h"
#include "IrrigationProductivityFixture.h"

namespace oe_agriculture_reconfiguration_fixture {
namespace w=oe_food_warehouse_fixture;
namespace p=oe_plantations_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace i=oe_irrigation_productivity_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using w::check;using w::purchase;using w::crop;using w::stock;using w::operate;using w::until;using w::ticks;
struct Fixture:w::Fixture {
    explicit Fixture(bool blocked_source_cells=false){
        for(const auto cell:openemperor::original::pump_geometry(i::pump_origin,0).water)
            raw.at(std::size_t(cell.y)*228U+std::size_t(cell.x))=4U;
        for(int y=6;y<=30;++y)for(int x=96;x<=134;++x)raw[std::size_t(y)*228U+std::size_t(x)]=4U;
        if(blocked_source_cells)for(const sim::Cell cell:std::array<sim::Cell,2>{{{94,114},{95,108}}})raw.at(std::size_t(cell.y)*228U+std::size_t(cell.x))=4U;
        rebind();
    }
    sim::World world()const{return sim::World::create_agriculture_reconfiguration(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& s)const{return sim::World::restore_agriculture_reconfiguration(s,policy,model,terrain);}
    sim::World restore(const save::SaveDocument& s)const{return save::restore_agriculture_reconfiguration(s,root.data,mask(),policy,model,terrain);}
};
inline const sim::AgricultureField& field(const sim::World& world,sim::BuildingId id){for(const auto& r:world.agriculture_fields())if(r.field_id==id)return r;throw std::runtime_error("Paid Field binding missing");}
inline const sim::MultiFieldWork& work(const sim::World& world,sim::BuildingId id){for(const auto& r:world.multi_field_agriculture_state()->fields)if(r.field_id==id)return r;throw std::runtime_error("Paid Field work missing");}
inline const sim::PhysicalFoodGrowerField& receipt(const sim::World& world,sim::BuildingId id){for(const auto& r:world.physical_food_growers_state()->fields)if(r.field_id==id)return r;throw std::runtime_error("Physical Field receipt missing");}
inline const sim::PhysicalFoodGrowerFarm& worker(const sim::World& world,sim::BuildingId id){for(const auto& r:world.physical_food_growers_state()->farms)if(r.farm_id==id)return r;throw std::runtime_error("Paid Farm worker missing");}
inline void refused(sim::World& world,sim::Command c,const char* why){const auto before=world.snapshot();check(!world.execute(c).accepted&&world.snapshot()==before,why);}
inline void valid(const sim::World& world){
    check(world.rule_version()==36&&world.agriculture_reconfiguration_enabled()&&world.agriculture_reconfiguration_state()&&world.processed_food_state()&&
        !world.hunting_food_state()&&!world.fishing_food_state()&&!world.original_agriculture_state()&&world.original_production_state()->food==sim::OriginalFoodState{},"Reconfiguration changed the sole exact-generation Food authority");
    const bool population=world.population_valid(),city=world.city_economy_valid(),production=world.production_balance_valid(),navigation=world.navigation_valid(),fire=world.fire_state_valid();
    check(population&&city&&production&&navigation&&fire,"Reconfiguration broke shared World invariants tick="+std::to_string(world.ticks())+
        " population="+std::to_string(population)+" city="+std::to_string(city)+" production="+std::to_string(production)+" navigation="+std::to_string(navigation)+" fire="+std::to_string(fire)+
        " journal="+std::to_string(world.agriculture_reconfiguration_state()->entries.size()));
    check(world.buildings().size()<=82&&world.couriers().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size()<=58,"Retirement expanded lifetime body/channel budgets");
    sim::ProcessedFoodCounters origin{},held{},carried{},used{};
    for(const auto& r:world.physical_food_growers_state()->fields)origin[static_cast<std::size_t>(field(world,r.field_id).crop)]+=r.returned;
    for(const auto& r:world.fishing_state()->quays)origin[5]+=r.unloaded;
    for(const auto& r:world.hunting_state()->tents)origin[6]+=r.unloaded;
    for(const auto& r:world.food_processing_state()->mills)for(std::size_t g=0;g<4;++g)origin[7+g]+=r.completed_by_grade[g]*(g+1);
    for(const auto& r:world.processed_food_state()->stocks)for(std::size_t g=0;g<12;++g){check(r.amounts[g]>=0,"Negative retained Food");held[g]+=static_cast<unsigned>(r.amounts[g]);used[g]+=r.consumed[g];}
    for(const auto& r:world.processed_food_state()->trips)for(std::size_t g=0;g<12;++g)carried[g]+=static_cast<std::uint64_t>(r.loaded[g]-r.delivered[g]-r.returned[g]);
    for(std::size_t g=0;g<12;++g)check(origin[g]==held[g]+carried[g]+used[g],"Retirement lost or duplicated earned Food");
    for(std::size_t g=0;g<openemperor::original::goods_count;++g)check(world.original_goods_balance(static_cast<openemperor::original::Goods>(g)).conserved,"Retirement changed Goods conservation");
    for(std::size_t g=0;g<sim::original_aux_material_count;++g)check(world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(g)).conserved,"Retirement changed Aux conservation");
    sim::World::validate_agriculture_reconfiguration_snapshot(world.snapshot());sim::World::validate_irrigation_productivity_snapshot(world.snapshot());
}
inline sim::World json_roundtrip(const Fixture& fixture,const sim::World& world,const char* name){const auto path=fixture.root.path/name;const auto before=world.snapshot();
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(path);const auto doc=save::read_save(path);
    check(doc.source_schema_version==52&&doc.world==before,"Schema52 changed exact pending/retired authority");auto restored=fixture.restore(doc);
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());check(f::read_text(path)==bytes&&world.snapshot()==before,"Schema52 canonical bytes or original World changed");return restored;}
} // namespace oe_agriculture_reconfiguration_fixture
