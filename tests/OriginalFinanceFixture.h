#pragma once

// Paid authored geography shared by the finance World/save/process tests.
// Original model bytes, when requested, are read from an explicit installation
// and copied only into the fixture's private temporary data root.
#include "OriginalProductionFixture.h"
#include "simulation/OriginalFinanceState.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace oe_finance_fixture {
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;

inline void set_building_field(std::string& text,orig::ModelBuilding model,orig::BuildingField field,int value) {
    const auto marker=std::to_string(orig::model_id(model).value)+","+std::string(orig::model_label(model))+",{,";
    auto at=text.find(marker);f::check(at!=std::string::npos,"Authored finance building row missing");at+=marker.size();
    for (unsigned i=0;i<static_cast<unsigned>(field);++i) {
        at=text.find(',',at);f::check(at!=std::string::npos,"Authored finance building field missing");++at;
    }
    const auto end=text.find(',',at);f::check(end!=std::string::npos,"Authored finance building value missing");
    text.replace(at,end-at,std::to_string(value));
}

inline std::string authored_model(int tax=3) {
    auto text=oe_production_fixture::authored_model_text(7);
    for (std::size_t row=0;row<orig::house_row_count;++row)
        oe_production_fixture::set_house_field(text,row,18,tax);
    for (std::size_t row=0;row<3;++row) {
        oe_production_fixture::set_house_field(text,row,0,-99);
        oe_production_fixture::set_house_field(text,row,1,row==2 ? 100:-5);
        oe_production_fixture::set_house_field(text,row,3,row ? 1:0);
        oe_production_fixture::set_house_field(text,row,8,row==2 ? 20:0);
        oe_production_fixture::set_house_field(text,row,21,row==2 ? 1:0);
    }
    return text;
}

inline std::shared_ptr<const orig::OriginalRulesModel> original_model_real(const f::fs::path& data,orig::Difficulty difficulty) {
    return save::prepare_original_production_rules_model(data,difficulty);
}

struct Fixture {
    f::Root root;
    std::shared_ptr<const sim::MapPermissions> policy=f::permissions();
    std::shared_ptr<const orig::OriginalRulesModel> model;
    explicit Fixture(const f::fs::path& original={},orig::Difficulty difficulty=orig::Difficulty::Normal,int tax=3) {
        const auto text=original.empty() ? authored_model(tax):f::read_text(original/"Model/EmperorBuildingModels.txt");
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=original_model_real(root.data,difficulty);
    }
    sim::World world() const {return sim::World::create_original_production_development(policy,model);}
    save::SaveDocument document(const sim::World& world) const {
        return save::make_document(root.data,f::map_relative,policy->building_mask(),world);
    }
    sim::World restore(const save::SaveDocument& document) const {
        return save::restore_original_production_development(document,root.data,policy->building_mask(),policy);
    }
    sim::World restore(const sim::World& world) const {return restore(document(world));}
};

struct Scene {
    std::vector<sim::BuildingId> commons;
    std::optional<sim::BuildingId> elite;
    sim::BuildingId farm{},field{},market{},water{},ancestor{};
    std::uint64_t spent=0;
};
inline Scene build(sim::World& world,bool with_elite=false) {
    Scene result;const auto before=world.construction_spent_total();
    const auto buy=[&](sim::CommandType type,sim::Cell cell) {
        f::buy(world,{type,cell});return *world.building_owner_at(cell);
    };
    for (int x=0;x<24;++x) f::buy(world,{sim::CommandType::PlaceRoad,{x,4}});
    for (int x=0;x<24;x+=3) result.commons.push_back(buy(sim::CommandType::PlaceHousehold,{x,2}));
    result.farm=buy(sim::CommandType::PlaceOriginalFoodFarm,{0,5});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{3,5}};field.building_id=result.farm;
    f::buy(world,field);result.field=*world.building_owner_at(field.cell);
    result.market=buy(sim::CommandType::PlaceMarket,{5,5});
    result.water=buy(sim::CommandType::PlaceOriginalWaterWell,{9,5});
    result.ancestor=buy(sim::CommandType::PlaceOriginalAncestorShrine,{13,5});
    if (with_elite) {
        for (int x=0;x<24;++x) f::buy(world,{sim::CommandType::PlaceRoad,{x,9}});
        for (int y=5;y<9;++y) f::buy(world,{sim::CommandType::PlaceRoad,{23,y}});
        result.elite=buy(sim::CommandType::PlaceOriginalElitePlot,{0,10});
    }
    result.spent=world.construction_spent_total()-before;
    f::check(!world.total_population() && world.taxes_collected_total()==0 && world.maintenance_spent_total()==0,
             "Paid finance construction granted residents or recurring money");
    return result;
}
inline const orig::HouseState& house(const sim::World& world,sim::BuildingId id) {
    for (const auto& value:world.original_economy_state()->houses) if (value.building_id==id) return value.house;
    throw std::runtime_error("Finance scene lost paid House");
}
inline const sim::OriginalFinanceHouse& finance(const sim::World& world,sim::BuildingId id) {
    for (const auto& row:world.original_production_state()->finance.houses) if (row.building_id==id) return row;
    throw std::runtime_error("Finance scene lost exact House finance row");
}

// Independent fixed-contract oracle. It does not call the production finance
// calculator or use its saved historical ledger to predict the next payment.
inline std::uint64_t expected_maintenance(const sim::World& world) {
    std::uint64_t total=0;
    for (const auto& body:world.buildings()) if (body.placed) {
        const auto workers=world.workforce_required(body.id);
        f::check(workers>=0,"Negative installed workforce in finance oracle");
        const auto bills=(world.ticks()-body.placed_tick)/((world.rule_version()==12||world.rule_version()==13||world.rule_version()==14||world.rule_version()==15||world.rule_version()==16||world.rule_version()==17||world.rule_version()==18) ? 816U:400U);
        const auto cost=static_cast<std::uint64_t>(workers)*2;
        f::check(!cost || bills<=(UINT64_MAX-total)/cost,"Finance oracle maintenance overflow");
        total+=bills*cost;
    }
    return total;
}
inline void accounting(const sim::World& world) {
    f::check(world.maintenance_spent_total()==expected_maintenance(world),"Actual upkeep differs from installed workers/versioned placement-relative bills");
    std::uint64_t paid=0;
    for (const auto& body:world.buildings()) {
        f::check(!body.taxes_paid_total || sim::original_house_kind(body.kind),"Non-House owns actual house tax");
        f::check(body.taxes_paid_total<=UINT64_MAX-paid,"Finance oracle House tax sum overflow");paid+=body.taxes_paid_total;
    }
    f::check(paid==world.taxes_collected_total(),"Actual global taxes differ from the sole House payment counters");
    f::check(sim::original_finance_accounting_valid(world.treasury(),paid,world.construction_spent_total(),world.maintenance_spent_total()),
             "Complete actual signed funds ledger differs");
}
inline std::int64_t construction_funds(const sim::World& world) {
    accounting(world);
    f::check(world.taxes_collected_total()<static_cast<std::uint64_t>(INT64_MAX) &&
             world.maintenance_spent_total()<static_cast<std::uint64_t>(INT64_MAX),"Small fixture finance oracle exceeded signed comparison domain");
    return world.treasury()-static_cast<std::int64_t>(world.taxes_collected_total())+
        static_cast<std::int64_t>(world.maintenance_spent_total());
}
inline void tick(sim::World& world) {
    const auto next=world.ticks()+1;std::uint64_t income=0,bill=0;
    std::vector<std::pair<sim::BuildingId,std::uint64_t>> house_payments;
    for (const auto& body:world.buildings()) {
        const auto workers=world.workforce_required(body.id);
        if (body.placed && next>body.placed_tick && (next-body.placed_tick)%400==0)
            bill+=2*static_cast<std::uint64_t>(workers);
        if (!sim::original_house_kind(body.kind)) continue;
        auto payment=std::uint64_t{0};const auto& home=house(world,body.id);
        if (next%816==0 && home.occupants>0 && !world.building_on_fire(body.id)) {
            const auto rate=world.original_rules_model()->house_rules().value(orig::house_row(home.type),orig::HouseField::TaxRate);
            f::check(rate>=0,"Negative prepared tax entered active finance fixture");
            payment=static_cast<std::uint64_t>(rate)*static_cast<std::uint64_t>(home.occupants);
        }
        income+=payment;house_payments.emplace_back(body.id,body.taxes_paid_total+payment);
    }
    const auto funds=world.treasury();
    const auto taxes=world.taxes_collected_total(),maintenance=world.maintenance_spent_total();
    f::check(income<=static_cast<std::uint64_t>(INT64_MAX) && bill<=static_cast<std::uint64_t>(INT64_MAX),
             "Small actual finance scenario exceeded signed oracle domain");
    const auto expected=funds+static_cast<std::int64_t>(income)-static_cast<std::int64_t>(bill);
    world.tick();
    f::check(world.treasury()==expected && world.taxes_collected_total()==taxes+income &&
             world.maintenance_spent_total()==maintenance+bill,"Actual finance tick ignored exact pre-tick816t tax or installed400t upkeep");
    for (const auto& [id,paid]:house_payments)
        f::check(world.building(id).taxes_paid_total==paid,"House actual tax delta differs from pre-tick occupants/type/fire");
    accounting(world);
}
inline void through(sim::World& world,std::uint64_t target) {while (world.ticks()<target) tick(world);}
inline void advance_to_tax_or_goal(sim::World& world,std::uint64_t limit=20000) {
    const auto deadline=world.ticks()+limit;
    while (!world.original_goal_status().reached && world.ticks()<deadline) tick(world);
    f::check(world.original_goal_status().reached && world.taxes_collected_total()>0,
             "Genuinely paid Food/Water/Ancestor houses did not reach current Original finance mission");
}
} // namespace oe_finance_fixture
