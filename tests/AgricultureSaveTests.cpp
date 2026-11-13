#include "AgricultureFixture.h"
#include "persistence/RecoveryStore.h"

#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace q=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using q::check;

template<class Action>void refuses(Action action,const char* reason) {
    try {action();}catch(const std::exception&){return;}
    throw std::runtime_error(reason);
}
std::string roundtrip(const q::Fixture& fixture,const sim::World& world,const fs::path& path) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto parsed=save::read_save(path);const auto json=Json::parse(bytes);
    check(parsed.source_schema_version==40 && parsed.world.rule_version==24 && parsed.world==before &&
          json.at("rules").at("agriculture")==sim::original_agriculture_contract &&
          json.at("world").at("agriculture").at("revision")==sim::agriculture_revision &&
          json.at("world").at("agriculture").at("catalog")==sim::agriculture_catalog &&
          parsed.world.original_production->development_revision==27 && !parsed.migrated_from_schema1,
          "Schema40 lost exact catalog/revision, source generation or complete World");
    auto restored=fixture.restore(parsed);check(restored.snapshot()==before,"Schema40 complete restore changed World");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes,"Schema40 canonical F5/F9 bytes changed");
    auto expected=world;
    for(int i=0;i<151;++i) {
        restored.tick();expected.tick();
        check(restored.snapshot()==expected.snapshot(),"Schema40 construction/calendar/House replay diverged");
    }
    return bytes;
}
void identity_and_canonical_hostile_json() {
    q::Fixture fixture;auto world=fixture.world();
    const auto path=fixture.root.path/"agriculture40.json";
    const auto historical_terrain=save::prepare_original_terrain_facts(fixture.root.data,f::map_relative);
    auto historical=sim::World::create_original_deadline_food_dispatch(fixture.policy,fixture.model,historical_terrain);
    const auto old_farm=q::purchase(historical,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command old_field{sim::CommandType::PlaceOriginalMilletField,{23,25}};old_field.building_id=old_farm;
    f::buy(historical,old_field);
    const auto historical_path=fixture.root.path/"historical-millet34.json";
    save::write_save(historical_path,fixture.document(historical),fixture.root.data,fixture.mask());
    const auto historical_bytes=f::read_text(historical_path),historical_source=f::read_text(fixture.root.data/f::map_relative);
    const auto agriculture_before=world.snapshot();
    refuses([&]{save::write_save(historical_path,fixture.document(world),fixture.root.data,fixture.mask());},
            "Agriculture overwrote a historical Wheat/Millet save");
    check(f::read_text(historical_path)==historical_bytes && world.snapshot()==agriculture_before &&
          f::read_text(fixture.root.data/f::map_relative)==historical_source &&
          fixture.restore(fixture.document(world)).snapshot()==agriculture_before,
          "Refused cross-generation overwrite changed source, save or Agriculture World");
    roundtrip(fixture,world,path);q::build(world);for(int i=0;i<400;++i)world.tick();
    const auto bytes=roundtrip(fixture,world,path);const auto canonical=Json::parse(bytes);
    const auto invalid=fixture.root.path/"hostile40.json";
    const auto reject=[&](auto edit) {
        auto json=canonical;edit(json);f::write_text(invalid,json.dump());
        refuses([&]{(void)save::read_save(invalid);},"Noncanonical Agriculture JSON became authority");
        check(f::read_text(path)==bytes,"Rejected Agriculture JSON changed canonical save bytes");
    };
    reject([](auto& j){j["rules"].erase("agriculture");});
    reject([](auto& j){j["rules"]["agriculture"]="unknown";});
    reject([](auto& j){j["rules"]["version"]=18;});
    reject([](auto& j){j["schema_version"]=34;});
    reject([](auto& j){j["world"].erase("agriculture");});
    reject([](auto& j){j["world"]["agriculture"]["revision"]=27;});
    reject([](auto& j){j["world"]["agriculture"]["catalog"]="soy-is-rice";});
    reject([](auto& j){j["world"]["agriculture"]["unknown"]=1;});
    reject([](auto& j){j["world"]["agriculture"]["stocks"][0]["amounts"]=Json::array({0,0});});
    reject([](auto& j){j["world"]["agriculture"]["stocks"][0]["received"]=Json::array({0,0,0,0,0,0});});
    reject([](auto& j){j["world"]["agriculture"]["stocks"][0]["amounts"][3]=-1;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["crop"]=5;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["farm_id"]=0;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["harvested"]=4;});
    reject([](auto& j){auto& rows=j["world"]["agriculture"]["fields"];rows.push_back(rows.front());});
    reject([](auto& j){j["world"]["original_production"]["food"]["farms"].push_back(
        {{"farm_id",1},{"field_id",nullptr},{"crop",0},{"harvested",0}});});
    reject([](auto& j){j["profiles"]["fertility"]=Json::array({100});});
    auto duplicated=bytes;const auto at=duplicated.find("\"agriculture\":");
    check(at!=std::string::npos,"Canonical Agriculture marker missing");
    duplicated.insert(at,"\"agriculture\":\"authored-agriculture-v1\",");f::write_text(invalid,duplicated);
    refuses([&]{(void)save::read_save(invalid);},"Duplicate Agriculture key entered authority");
    const auto parsed=save::read_save(path);
    refuses([&]{(void)save::restore_save(parsed,fixture.root.data,fixture.mask(),fixture.policy);},
            "Normal restore silently opted into Agriculture");
    refuses([&]{(void)save::restore_original_deadline_food_dispatch(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
            "Rule18 restore reinterpreted five-column Agriculture Food");
    auto legacy=sim::World::create_original_deadline_food_dispatch(fixture.policy,fixture.model,
        save::prepare_original_terrain_facts(fixture.root.data,f::map_relative));
    const auto oldpath=fixture.root.path/"historical34.json";
    save::write_save(oldpath,fixture.document(legacy),fixture.root.data,fixture.mask());const auto oldbytes=f::read_text(oldpath);
    const auto old=save::read_save(oldpath);
    refuses([&]{(void)fixture.restore(old);},"Agriculture restore automatically migrated Rule18");
    check(f::read_text(oldpath)==oldbytes && old.source_schema_version==34 && !old.world.agriculture,
          "Agriculture changed historical save authority or bytes");
}
void construction_clock_gate_recovery_and_atomic_refusals() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    check(!sim::agriculture_production_clock_verified,"Proof-gate test requires the unreconstructed Original work clock");
    const auto path=fixture.root.path/"construction40.json";roundtrip(fixture,world,path);
    const auto no_food=[&] {
        const auto& state=*world.original_agriculture_state();
        for(const auto& field:state.fields)
            check(!field.quantity && !field.productive_events && !field.harvested && !field.last_harvest_cycle,
                  "An unverified work clock created field growth or harvest authority");
        for(const auto& stock:state.stocks)
            check(stock.amounts==sim::AgricultureAmounts{} && stock.received==sim::AgricultureCounters{} &&
                  stock.departed==sim::AgricultureCounters{} && stock.returned==sim::AgricultureCounters{} &&
                  stock.consumed==sim::AgricultureCounters{} && !stock.quality_witness,
                  "Construction or calendar advancement created typed Food or a delivery witness");
        check(state.trips.empty() && world.original_production_state()->food==sim::OriginalFoodState{},
              "Proof-gated Agriculture created a Food trip or mirrored legacy Food authority");
        for(const auto& house:world.original_economy_state()->houses)
            check(!house.house.food_quality,"House received FoodQuality without an actual proven harvest/delivery");
        check(q::field(world,scene.cabbage_field).crop==sim::AgricultureCrop::Cabbage &&
              q::field(world,scene.soy_field).crop==sim::AgricultureCrop::Soybean &&
              q::field(world,scene.cabbage_field).farm_id==scene.cabbage_farm &&
              q::field(world,scene.soy_field).farm_id==scene.soy_farm,
              "Calendar/restore changed distinct paid crop or Farm identities");
    };
    no_food();
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    // Cross the complete existing calendar once. The clock remains the inherited
    // World clock; none of its phase/month/year boundaries substitutes for the
    // missing Original Grower work and harvest proof.
    for(const std::uint64_t boundary:{400U,3264U,6528U,9000U,9840U}) {
        while(world.ticks()<boundary)world.tick();
        no_food();const auto bytes=roundtrip(fixture,world,path);const auto before=world.snapshot();
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable() && entry.schema==40 && entry.rule_version==24 && f::read_text(entry.path)==bytes &&
              fixture.restore(save::read_save(entry.path)).snapshot()==before,
              "Recovery changed paid Agriculture construction, proof gate or catalog authority");
    }
    const auto bytes=f::read_text(path);const auto before=world.snapshot();
    const auto reject=[&](auto edit) {
        auto malformed=fixture.document(world);edit(*malformed.world.agriculture);
        refuses([&]{(void)fixture.restore(malformed);},"Unproved Agriculture production entered full restore");
        refuses([&]{save::write_save(path,malformed,fixture.root.data,fixture.mask());},
                "Unproved Agriculture production reached atomic publication");
        check(f::read_text(path)==bytes && world.snapshot()==before,
              "Rejected production restore/write changed canonical bytes or World");
    };
    reject([](auto& state){state.stocks.front().amounts[4]=1;});
    reject([](auto& state){state.fields.front().quantity=160;});
    reject([](auto& state){state.fields.front().productive_events=1;});
    reject([](auto& state){state.fields.front().harvested=4;});
    reject([](auto& state){state.fields.front().last_harvest_cycle=1;});
    const auto catalog=recovery.catalog();check(catalog.histories.size()==1 && !catalog.truncated,
        "Recovery lost its explicit Agriculture history");
    // Live source bytes, including the separate fertility lane, remain mandatory.
    const auto source=f::read_text(fixture.root.data/f::map_relative);
    fixture.fertility[25*228+23]=99;fixture.write_map();
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Changed source fertility/map bypassed complete SHA binding");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());},
            "Changed source context reached atomic Agriculture publication");
    check(f::read_text(path)==bytes,"Failed source validation replaced earlier Agriculture bytes");
    f::write_text(fixture.root.data/f::map_relative,source);
}
} // namespace
int main() {
    try {
        identity_and_canonical_hostile_json();construction_clock_gate_recovery_and_atomic_refusals();
        std::cout<<"Agriculture schema40 construction/proof-gate/source/recovery tests passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
