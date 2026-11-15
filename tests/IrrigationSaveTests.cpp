#include "IrrigationFixture.h"
#include "persistence/RecoveryStore.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>

namespace {

namespace q=oe_irrigation_fixture;
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;

template<class Operation>
void refuses(Operation operation,const char* message) {
    try {operation();} catch (const std::exception&) {return;}
    throw std::runtime_error(message);
}

std::string roundtrip(const q::Fixture& fixture,const sim::World& world,const fs::path& path) {
    const auto before=world.snapshot();
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);
    const auto json=Json::parse(bytes);
    const auto parsed=save::read_save(path);
    check(parsed.source_schema_version==42 && parsed.world.rule_version==26 &&
          parsed.world==before && !parsed.migrated_from_schema1 &&
          parsed.map_permissions_policy_version==3 &&
          json.at("rules").at("irrigation")==sim::irrigation_contract &&
          json.at("world").at("irrigation").at("revision")==sim::irrigation_revision &&
          parsed.world.agriculture.has_value() && parsed.world.plantations.has_value(),
          "Schema42 lost its exact rule, source policy or inherited Agriculture state");
    const auto& irrigation=json.at("world").at("irrigation");
    check(!irrigation.contains("wet") && !irrigation.contains("supplier") && !irrigation.contains("last_phase_tick"),
          "Derived irrigation cache became saved authority");
    auto restored=fixture.restore(parsed);
    check(restored.snapshot()==before,"Schema42 restore changed complete paid state");
    for (const auto& ditch:world.irrigation_state()->ditches)
        check(restored.irrigation_ditch_wet(ditch.cell)==world.irrigation_ditch_wet(ditch.cell),
              "Restore derived a different supplied-water state");
    for (const sim::Cell cell:std::array<sim::Cell,3>{{{114,101},{109,100},{122,110}}}) {
        const auto expected=world.irrigation_field_status(cell);
        const auto actual=restored.irrigation_field_status(cell);
        check(actual.water_available==expected.water_available && actual.wet_ditch==expected.wet_ditch &&
              actual.supplying_pump==expected.supplying_pump && !actual.yield_effect_active,
              "Restore changed field water provenance or activated an unproved yield effect");
    }
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Schema42 canonical rewrite changed bytes or source World");
    auto expected=world;
    for (int tick=0;tick<105;++tick) {
        expected.tick();restored.tick();
        check(expected.snapshot()==restored.snapshot(),"Schema42 deterministic continuation diverged");
        for (const auto& ditch:expected.irrigation_state()->ditches)
            check(expected.irrigation_ditch_wet(ditch.cell)==restored.irrigation_ditch_wet(ditch.cell),
                  "Schema42 continuation changed derived wet/dry state");
    }
    return bytes;
}

void supplied_water_pause_cuts_and_recovery() {
    q::Fixture fixture;auto world=fixture.world();
    const auto path=fixture.root.path/"irrigation42.json";
    roundtrip(fixture,world,path);
    const auto scene=q::build(world);
    constexpr std::array<sim::Cell,3> chain{{{114,100},{115,100},{116,100}}};
    const auto batch=world.execute_irrigation_batch(chain);
    check(batch.accepted && batch.changed && world.irrigation_state()->ditches.size()==3,
          "Paid multi-cell irrigation batch failed");
    q::ditch(world,{110,100});
    check(!world.irrigation_ditch_wet(chain.front()),"An unstaffed Pump supplied water during construction");
    roundtrip(fixture,world,path);
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},
             "Paid staffed Pump never supplied its connected ditch chain");
    check(!world.irrigation_ditch_wet({110,100}),"Disconnected ditch gained water");
    roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"irrigation-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=[&] {
        const auto bytes=roundtrip(fixture,world,path);
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable() && entry.schema==42 && entry.rule_version==26 &&
              f::read_text(entry.path)==bytes && fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
              "Recovery lost complete installed irrigation authority");
    };
    f::buy(world,sim::set_building_operation(scene.pump,false));
    q::ticks(world,1);
    check(!world.irrigation_ditch_wet(chain.front()),"Paused Pump retained water supply");
    checkpoint();
    f::buy(world,sim::set_building_operation(scene.pump,true));
    roundtrip(fixture,world,path);
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},
             "Resumed Pump never supplied a later irrigation phase");
    checkpoint();
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,chain[1]});
    q::ticks(world,1);
    check(world.irrigation_ditch_wet(chain.front()) && !world.irrigation_ditch_wet(chain.back()),
          "Removed ditch failed to cut downstream water supply");
    checkpoint();
    const auto catalog=recovery.catalog();
    check(!catalog.truncated && catalog.histories.size()==1 && catalog.histories.front().error.empty() &&
          catalog.histories.front().entries.size()==4,"Recovery lost irrigation history or checkpoints");
    for (const auto& entry:catalog.histories.front().entries)
        check(entry.loadable(),"Recovery rejected a valid irrigation checkpoint");
}

void strict_authority_and_atomic_source_checks() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    q::ditch(world,{114,100});q::ditch(world,{115,100});
    q::until(world,[&]{return world.irrigation_ditch_wet({115,100});},"Hostile-save control never earned actual water supply");
    const auto path=fixture.root.path/"canonical42.json";
    const auto bytes=roundtrip(fixture,world,path);const auto canonical=Json::parse(bytes);
    const auto before=world.snapshot();
    const auto map_path=fixture.root.data/f::map_relative;
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt";
    const auto map=f::read_text(map_path),model=f::read_text(model_path);
    const auto hostile_path=fixture.root.path/"hostile42.json";
    const auto unchanged=[&] {
        check(world.snapshot()==before && f::read_text(path)==bytes &&
              f::read_text(map_path)==map && f::read_text(model_path)==model,
              "Rejected irrigation restore changed paid state, target or authored source bytes");
    };
    const auto reject=[&](auto edit) {
        auto hostile=canonical;edit(hostile);f::write_text(hostile_path,hostile.dump());
        refuses([&]{(void)fixture.restore(save::read_save(hostile_path));},"Manipulated irrigation save became live authority");
        unchanged();
    };
    reject([](auto& j){j["rules"].erase("irrigation");});
    reject([](auto& j){j["rules"]["irrigation"]="unproved-water";});
    reject([](auto& j){j["rules"]["version"]=25;});
    reject([](auto& j){j["schema_version"]=41;});
    reject([](auto& j){j["world"].erase("irrigation");});
    reject([](auto& j){j["world"]["irrigation"]["revision"]=2;});
    reject([](auto& j){j["world"]["irrigation"]["wet"]=true;});
    reject([](auto& j){j["world"]["irrigation"]["supplier"]=1;});
    reject([](auto& j){j["world"]["irrigation"]["ditches"][0]["wet"]=true;});
    reject([](auto& j){j["world"]["irrigation"]["pumps"][0]["last_phase_tick"]=0;});
    reject([](auto& j){j["world"]["irrigation"]["pumps"][0]["orientation"]=4;});
    reject([](auto& j){j["world"]["irrigation"]["pumps"][0]["orientation"]=1;});
    reject([](auto& j){j["world"]["irrigation"]["pumps"][0]["building_id"]=0;});
    reject([](auto& j){auto& rows=j["world"]["irrigation"]["pumps"];rows.push_back(rows.front());});
    reject([](auto& j){j["world"]["irrigation"]["pumps"][0]["eligible_since_tick"]=0;});
    reject([&](auto& j){j["world"]["irrigation"]["pumps"][0]["eligible_since_tick"]=world.ticks()+2;});
    reject([](auto& j){j["world"]["irrigation"]["pumps"][0]["eligible_since_tick"]=nullptr;});
    reject([](auto& j){j["world"]["irrigation"]["ditches"][0]["placed_sequence"]=0;});
    reject([&](auto& j){j["world"]["irrigation"]["ditches"][0]["placed_tick"]=world.ticks()+1;});
    reject([](auto& j){j["world"]["irrigation"]["ditches"][0]["cell"]=Json::array({0,0});});
    reject([](auto& j){j["world"]["irrigation"]["ditches"][0]["cell"]=Json::array({114,98});});
    reject([](auto& j){auto& rows=j["world"]["irrigation"]["ditches"];rows.push_back(rows.front());});
    reject([](auto& j){j["world"]["irrigation"]["ditches_removed_total"]=1;});
    reject([](auto& j){j["world"]["irrigation"]["ditch_construction_spent"]=1;});
    reject([](auto& j){j["world"]["irrigation"]["ditches_placed_total"]=UINT64_MAX;});
    reject([](auto& j){j["world"].erase("plantations");});
    reject([](auto& j){j["world"].erase("agriculture");});
    reject([](auto& j){j["profiles"]["map_permissions"]["version"]=2;});
    auto duplicate=bytes;const auto at=duplicate.find("\"irrigation\":");
    check(at!=std::string::npos,"Canonical irrigation marker missing");
    duplicate.insert(at,"\"irrigation\":\"authored-agriculture-irrigation-v1\",");
    f::write_text(hostile_path,duplicate);
    refuses([&]{(void)save::read_save(hostile_path);},"Duplicate irrigation key entered authority");unchanged();
    refuses([&]{(void)save::restore_original_plantations(save::read_save(path),fixture.root.data,fixture.mask(),
        fixture.policy,fixture.model,fixture.terrain);},"Rule25 restore accepted a Rule26 save");unchanged();
    const auto document=save::read_save(path);
    const auto old_model=save::prepare_original_agriculture_rules_model(fixture.root.data);
    refuses([&]{(void)save::restore_original_irrigation(document,fixture.root.data,fixture.mask(),
        fixture.policy,old_model,fixture.terrain);},"Historical model factory acquired irrigation capability");unchanged();
    const auto old_terrain=save::prepare_original_agriculture_terrain_facts(fixture.root.data,f::map_relative);
    refuses([&]{(void)save::restore_original_irrigation(document,fixture.root.data,fixture.mask(),
        fixture.policy,fixture.model,old_terrain);},"Historical terrain facts acquired irrigation capability");unchanged();
    const std::vector<std::uint8_t> forged_boundary(sim::OriginalTerrainFacts::cell_count,1);
    const auto forged_terrain=std::make_shared<const sim::OriginalTerrainFacts>(
        fixture.raw,fixture.fertility,document.map_sha256,forged_boundary);
    refuses([&]{(void)save::restore_original_irrigation(document,fixture.root.data,fixture.mask(),
        fixture.policy,fixture.model,forged_terrain);},"Claimed source SHA bypassed complete source geometry equality");unchanged();
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},
            "Injected schema42 publication failure was ignored");unchanged();
    f::write_text(model_path,model+"\n");
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Changed model source bypassed irrigation binding");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());},"Changed model source reached publication");
    f::write_text(model_path,model);unchanged();
    f::write_text(map_path,map+"\n");
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Changed map source bypassed irrigation binding");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());},"Changed map source reached publication");
    f::write_text(map_path,map);unchanged();
    f::buy(world,sim::set_building_operation(scene.pump,false));
    auto paused=fixture.document(world);paused.world.irrigation->pumps.front().eligible_since_tick=world.ticks();
    refuses([&]{(void)fixture.restore(paused);},"Paused Pump restored a forged supply epoch");
}

void historical_targets_and_unknown_fields() {
    q::Fixture fixture;auto world=fixture.world();q::build(world);q::ditch(world,{114,100});
    const auto path=fixture.root.path/"separate42.json";const auto bytes=roundtrip(fixture,world,path);
    p::Fixture legacy;auto old=legacy.world();p::build(old);
    const auto old_path=legacy.root.path/"frozen41.json";
    save::write_save(old_path,legacy.document(old),legacy.root.data,legacy.mask());
    const auto old_bytes=f::read_text(old_path);const auto old_before=old.snapshot();
    auto restored=legacy.restore(save::read_save(old_path));
    check(restored.snapshot()==old_before,"Historical Rule25/schema41 restore changed");
    save::write_save(old_path,legacy.document(restored),legacy.root.data,legacy.mask());
    check(f::read_text(old_path)==old_bytes,"Historical schema41 canonical bytes changed");
    refuses([&]{save::write_save(old_path,fixture.document(world),fixture.root.data,fixture.mask());},"Schema42 overwrote schema41");
    refuses([&]{save::write_save(path,legacy.document(old),legacy.root.data,legacy.mask());},"Schema41 overwrote schema42");
    check(f::read_text(path)==bytes && f::read_text(old_path)==old_bytes && old.snapshot()==old_before,
          "Cross-generation overwrite refusal changed either World or target");
    const auto hostile=legacy.root.path/"hostile41.json";
    for (const bool marker:{false,true}) {
        auto changed=Json::parse(old_bytes);
        if (marker) changed["rules"]["irrigation"]=sim::irrigation_contract;
        else changed["world"]["irrigation"]=Json::parse(bytes).at("world").at("irrigation");
        f::write_text(hostile,changed.dump());
        refuses([&]{(void)save::read_save(hostile);},"Historical schema41 admitted irrigation fields");
    }
    a::Fixture older;auto foundation=older.world();
    const auto foundation_path=older.root.path/"frozen40.json";
    save::write_save(foundation_path,older.document(foundation),older.root.data,older.mask());
    const auto foundation_bytes=f::read_text(foundation_path);
    refuses([&]{save::write_save(foundation_path,fixture.document(world),fixture.root.data,fixture.mask());},"Schema42 overwrote schema40");
    auto changed=Json::parse(foundation_bytes);changed["world"]["irrigation"]=Json::object();
    f::write_text(older.root.path/"hostile40.json",changed.dump());
    refuses([&]{(void)save::read_save(older.root.path/"hostile40.json");},"Historical schema40 admitted irrigation authority");
    check(f::read_text(foundation_path)==foundation_bytes,"Schema40 target changed during refusal");
}

} // namespace

int main() {
    try {
        supplied_water_pause_cuts_and_recovery();
        strict_authority_and_atomic_source_checks();
        historical_targets_and_unknown_fields();
        std::cout << "Schema42 irrigation persistence, replay, Recovery and hostile inputs passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Irrigation persistence failed " << error.what() << '\n';
        return 1;
    }
}
