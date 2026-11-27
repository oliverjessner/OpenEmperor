#include "IrrigatedHarvestFixture.h"
#include "persistence/RecoveryStore.h"
#include "simulation/IrrigatedHarvestContract.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

namespace {
namespace t=oe_irrigated_harvest_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
namespace f=t::f;
using Json=nlohmann::json;
using t::check;
template<class Action>void refuses(Action action,const char* reason) {
    try {action();}catch(const std::exception&){return;}
    throw std::runtime_error(reason);
}
struct Fixture:t::Fixture {
    sim::World restore(const save::SaveDocument& document)const {
        return save::restore_irrigated_harvest(document,root.data,mask(),policy,model,terrain);
    }
};
Json& row(Json& values,const char* key,sim::BuildingId id) {
    for(auto& value:values)if(value.at(key)==static_cast<unsigned>(id))return value;
    throw std::runtime_error("Hostile control lost its actual paid identity");
}
std::map<fs::path,std::string> inventory(const fs::path& root) {
    std::map<fs::path,std::string> result;
    for(const auto& entry:fs::recursive_directory_iterator(root))
        if(entry.is_regular_file())result.emplace(fs::relative(entry.path(),root),f::read_text(entry.path()));
    return result;
}
sim::World roundtrip(const Fixture& fixture,const sim::World& world,const fs::path& path) {
    const auto before=world.snapshot();
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto json=Json::parse(bytes);
    check(document.source_schema_version==53&&document.world.rule_version==37&&document.world==before&&
        json.at("rules").at("irrigated_harvest")==sim::irrigated_harvest_contract&&
        json.at("rules").at("irrigated_harvest_revision")==sim::irrigated_harvest_revision&&
        document.world.agriculture_reconfiguration&&document.world.processed_food&&
        !document.world.hunting_food&&!document.world.fishing_food&&!document.world.agriculture,
        "Schema53 lost its exact marker or inherited sole Food authority");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Fresh schema53 restore changed actual authority");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Schema53 canonical roundtrip changed payload or live World");
    t::valid(restored);return restored;
}
void genuine_early_harvest_and_recovery_keep_cargo_separate_from_receipt() {
    Fixture fixture;auto world=fixture.world();const auto scene=t::build_early_food(world);
    const auto model_bytes=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    const auto map_bytes=f::read_text(fixture.root.data/f::map_relative);
    const auto ready_tick=world.ticks();const auto placed=world.building(scene.field).placed_tick;
    auto restored=roundtrip(fixture,world,fixture.root.path/"ready53.json");
    for(int tick=0;tick<4;++tick){world.tick();restored.tick();
        check(world.snapshot()==restored.snapshot()&&!t::field(world,scene.field).harvested&&
            !t::receipt(world,scene.field).returned,"Restore skipped one of the five actual harvest-work ticks");}
    world.tick();restored.tick();
    check(world.snapshot()==restored.snapshot()&&world.ticks()==ready_tick+5&&world.ticks()-placed<80&&
        t::work(world,scene.field).productive_ticks==64&&t::i::work(world,scene.field).wet_work_ticks==64&&
        t::i::work(world,scene.field).bonus_work==16&&t::field(world,scene.field).harvested==1&&
        !t::receipt(world,scene.field).returned&&sim::physical_food_trip_cargo(t::q::worker(world,scene.farm))==1,
        "Paid early64+16 harvest was not physical cargo before its separate real return");
    const auto path=fixture.root.path/"loaded53.json";
    restored=roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
    check(entry.loadable()&&entry.schema==53&&entry.rule_version==37&&entry.tick==world.ticks()&&
        f::read_text(entry.path)==f::read_text(path)&&fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
        "Ordinary Recovery did not preserve the exact early-harvest payload");
    for(int tick=0;tick<100&&!t::receipt(world,scene.field).returned;++tick){world.tick();restored.tick();
        check(world.snapshot()==restored.snapshot(),"Independent loaded53 continuation changed physical return");}
    check(t::receipt(world,scene.field).returned==1&&!sim::physical_food_trip_cargo(t::q::worker(world,scene.farm)),
        "Early-harvest cargo never earned its actual Farm receipt");
    (void)roundtrip(fixture,world,fixture.root.path/"returned53.json");
    const auto catalog=recovery.catalog();
    check(catalog.histories.size()==1&&!catalog.truncated&&
        std::all_of(catalog.histories[0].entries.begin(),catalog.histories[0].entries.end(),[](const auto& e){return e.loadable()&&e.schema==53&&e.rule_version==37;}),
        "Recovery catalog lost explicit53 admission or changed bounded histories");
    check(f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model_bytes&&
        f::read_text(fixture.root.data/f::map_relative)==map_bytes,"Save/Recovery changed authored source inputs");
}
void hostile_markers_earned_work_bonus_and_cargo_refuse_atomically() {
    Fixture fixture;auto world=fixture.world();const auto scene=t::build_early_food(world);t::ticks(world,5);
    const auto path=fixture.root.path/"hostile53.json";
    (void)roundtrip(fixture,world,path);const auto bytes=f::read_text(path);const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why){auto json=original;mutate(json);const auto input=json.dump();f::write_text(path,input);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before&&f::read_text(path)==input,"Refused53 restore changed World or supplied hostile bytes");f::write_text(path,bytes);};
    reject([](auto& j){j["rules"].erase("irrigated_harvest");},"Missing53 contract accepted");
    reject([](auto& j){j["rules"]["irrigated_harvest"]="other";},"Wrong53 contract accepted");
    reject([](auto& j){j["rules"].erase("irrigated_harvest_revision");},"Missing53 revision accepted");
    reject([](auto& j){j["rules"]["irrigated_harvest_revision"]=2;},"Wrong53 revision accepted");
    reject([](auto& j){j["world"]["irrigated_harvest"]={{"stock",1}};},"Invented53 state authority accepted");
    reject([&](auto& j){row(j["world"]["multi_field_agriculture"]["fields"],"field_id",scene.field)["productive_ticks"]=65;},
        "Forged earned base work accepted");
    reject([&](auto& j){row(j["world"]["irrigation_productivity"]["work"],"building_id",scene.field)["bonus_work"]=17;},
        "Unearned irrigation bonus accepted");
    reject([&](auto& j){row(j["world"]["irrigation_productivity"]["work"],"building_id",scene.field)["wet_work_ticks"]=60;},
        "Bonus without four real wet ticks accepted");
    reject([&](auto& j){row(j["world"]["physical_food_growers"]["farms"],"farm_id",scene.farm)["latest_trip"]["delivered"]=1;},
        "Loaded cargo was forged into a source receipt");
    reject([&](auto& j){row(j["world"]["physical_food_growers"]["fields"],"field_id",scene.field)["returned"]=1;},
        "Unreturned harvest was forged into the sole Food account");
    reject([](auto& j){j["schema_version"]=52;},"Schema53 marker admitted under schema52");
    reject([](auto& j){j["rules"]["version"]=36;},"Schema53 admitted a historical Rule36 world");
    reject([](auto& j){j["schema_version"]=52;j["rules"]["version"]=36;
        j["rules"].erase("irrigated_harvest");j["rules"].erase("irrigated_harvest_revision");},
        "Historical52 reinterpreted an early irrigated harvest as its own elapsed-work authority");
    auto duplicate=bytes;const std::string marker="\"irrigated_harvest_revision\": 1";const auto at=duplicate.find(marker);
    check(at!=std::string::npos,"Duplicate53 control lost exact revision marker");
    duplicate.replace(at,marker.size(),marker+",\n    "+marker);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate53 key accepted");
    check(f::read_text(path)==duplicate&&world.snapshot()==before,"Duplicate53 refusal changed supplied bytes or live World");f::write_text(path,bytes);

    save::RecoveryStore recovery(fixture.root.path/"hostile-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto document=fixture.document(world);const auto history=recovery.create_history(document,fixture.mask());
    const auto files=inventory(recovery.root());auto forged=document;forged.world.irrigation_productivity->work[0].bonus_work+=1;
    refuses([&]{save::write_save(path,forged,fixture.root.data,fixture.mask());},"Writer published forged earned work");
    refuses([&]{(void)recovery.write_checkpoint(history,forged,fixture.mask());},"Recovery published forged earned work");
    check(inventory(recovery.root())==files&&f::read_text(path)==bytes&&world.snapshot()==before,
        "Rejected writer/Recovery changed valid payload, history, IDs, stock or Funds");
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Atomic53 fault was ignored");
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Failed53 publication replaced valid payload or live World");
}
void historical52_and_new53_never_migrate_or_overwrite_each_other() {
    Fixture fixture;auto current=fixture.world();auto old=sim::World::create_agriculture_reconfiguration(fixture.policy,fixture.model,fixture.terrain);
    const auto path53=fixture.root.path/"current53.json",path52=fixture.root.path/"historical52.json";
    (void)roundtrip(fixture,current,path53);save::write_save(path52,fixture.document(old),fixture.root.data,fixture.mask());
    const auto bytes53=f::read_text(path53),bytes52=f::read_text(path52);const auto json52=Json::parse(bytes52);
    const auto doc52=save::read_save(path52),doc53=save::read_save(path53);
    check(doc52.source_schema_version==52&&doc52.world.rule_version==36&&!json52["rules"].contains("irrigated_harvest")&&
        save::restore_agriculture_reconfiguration(doc52,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain).snapshot()==old.snapshot(),
        "Historical36/schema52 identity or restore changed");
    refuses([&]{(void)fixture.restore(doc52);},"Schema52 automatically migrated to37");
    refuses([&]{(void)save::restore_agriculture_reconfiguration(doc53,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Schema53 silently restored as historical36");
    refuses([&]{save::write_save(path52,fixture.document(current),fixture.root.data,fixture.mask());},"Rule37 overwrote historical52");
    refuses([&]{save::write_save(path53,fixture.document(old),fixture.root.data,fixture.mask());},"Historical36 overwrote53");
    auto mismatched=fixture.document(current);mismatched.source_schema_version=52;
    refuses([&]{save::write_save(path53,mismatched,fixture.root.data,fixture.mask());},"Rule37 writer migrated a retained52 identity");
    check(f::read_text(path53)==bytes53&&f::read_text(path52)==bytes52,"Cross-generation refusal changed retained canonical bytes");
    const auto damaged=fixture.root.path/"damaged53.json";const std::string damaged_bytes="{\"schema_version\":53,\"damaged\":true}";
    f::write_text(damaged,damaged_bytes);
    refuses([&]{save::write_save(damaged,fixture.document(old),fixture.root.data,fixture.mask());},"Old writer replaced damaged53 target");
    refuses([&]{save::write_save(damaged,fixture.document(current),fixture.root.data,fixture.mask());},"New writer replaced invalid53 instead of requiring a valid exact target");
    check(f::read_text(damaged)==damaged_bytes,"Damaged53 target lost its supplied bytes");
    save::RecoveryStore recovery(fixture.root.path/"generation-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(current),fixture.mask());const auto files=inventory(recovery.root());
    refuses([&]{(void)recovery.write_checkpoint(history,fixture.document(old),fixture.mask());},"Recovery mixed schemas52/53 within one history");
    check(inventory(recovery.root())==files,"Cross-generation Recovery refusal changed history bytes");
    save::RecoveryStore legacy(fixture.root.path/"legacy-access",fixture.root.data);
    refuses([&]{(void)legacy.create_history(fixture.document(current),fixture.mask());},"Legacy-only Recovery implicitly enabled37");
}
} // namespace
int main(){try {
    genuine_early_harvest_and_recovery_keep_cargo_separate_from_receipt();
    hostile_markers_earned_work_bonus_and_cargo_refuse_atomically();
    historical52_and_new53_never_migrate_or_overwrite_each_other();
    std::cout<<"Irrigated harvest schema53 early-work/physical-return/Recovery/hostile tests passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
