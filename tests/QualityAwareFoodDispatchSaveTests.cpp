#include "QualityAwareFoodDispatchFixture.h"
#include "persistence/RecoveryStore.h"
#include "simulation/IrrigatedHarvestContract.h"
#include "simulation/QualityAwareFoodDispatchContract.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

namespace {
namespace t=oe_irrigated_harvest_fixture;
namespace e=oe_quality_aware_food_dispatch_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
namespace f=t::f;
using Json=nlohmann::json;
using t::check;
template<class Action>void refuses(Action action,const char* reason) {
    try{action();}catch(const std::exception&){return;}
    throw std::runtime_error(reason);
}
// The inherited, independently authored map/model fixture earns every paid
// body, immigrant and Food portion. It imports no original-data test fixture.
struct Fixture:e::Fixture {
    using e::Fixture::Fixture;
    sim::World restore(const save::SaveDocument& document)const {
        return save::restore_quality_aware_food_dispatch(document,root.data,mask(),policy,model,terrain);
    }
};
std::map<fs::path,std::string> inventory(const fs::path& root) {
    std::map<fs::path,std::string> result;
    for(const auto& entry:fs::recursive_directory_iterator(root))if(entry.is_regular_file())
        result.emplace(fs::relative(entry.path(),root),f::read_text(entry.path()));
    return result;
}
sim::World roundtrip(const Fixture& fixture,const sim::World& world,const fs::path& path) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto json=Json::parse(bytes);
    check(document.source_schema_version==54&&document.world.rule_version==38&&document.world==before&&
        json.at("rules").at("quality_aware_food_dispatch")==sim::quality_aware_food_dispatch_contract&&
        json.at("rules").at("quality_aware_food_dispatch_revision")==sim::quality_aware_food_dispatch_revision&&
        json.at("rules").at("irrigated_harvest")==sim::irrigated_harvest_contract&&
        document.world.processed_food&&document.world.food_processing&&document.world.agriculture_reconfiguration&&
        !document.world.hunting_food&&!document.world.fishing_food&&!document.world.agriculture&&
        !json.at("world").contains("quality_aware_food_dispatch"),
        "Schema54 lost its exact selection markers or duplicated inherited Food authority");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Fresh schema54 restore changed actual authority");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Schema54 canonical roundtrip changed payload or live World");
    check(restored.quality_aware_food_dispatch_enabled()&&restored.production_balance_valid()&&
        restored.population_valid()&&restored.city_economy_valid()&&restored.navigation_valid()&&restored.fire_state_valid(),
        "Schema54 restore broke shared World invariants");return restored;
}
void actual_earned_cargo_and_bounded_recovery_continue_exactly() {
    Fixture fixture;auto world=fixture.world();const auto scene=t::build_early_food(world);t::ticks(world,5);
    check(t::field(world,scene.field).harvested==1&&!t::receipt(world,scene.field).returned&&
        sim::physical_food_trip_cargo(t::q::worker(world,scene.farm))==1,
        "Schema54 test did not earn actual loaded cargo before separate source return");
    const auto model=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    const auto map=f::read_text(fixture.root.data/f::map_relative);
    const auto loaded_tick=world.ticks();auto restored=roundtrip(fixture,world,fixture.root.path/"loaded54.json");
    save::RecoveryStore recovery(fixture.root.path/"recovery54",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    for(unsigned index=0;index<8;++index) {
        const auto manual=fixture.root.path/("boundary54-"+std::to_string(index)+".json");
        (void)roundtrip(fixture,world,manual);
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==54&&entry.rule_version==38&&entry.tick==world.ticks()&&
            f::read_text(entry.path)==f::read_text(manual)&&
            fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
            "Ordinary Recovery changed actual schema54 payload, clock or explicit generation");
        for(int tick=0;tick<16;++tick){world.tick();restored.tick();
            check(restored.snapshot()==world.snapshot(),"Schema54 independent loaded/returned continuation differs");}
    }
    check(t::receipt(world,scene.field).returned>0&&world.ticks()>=loaded_tick+64U,
        "Schema54 physical harvest never earned its actual Farm return");
    const auto catalog=recovery.catalog();check(catalog.histories.size()==1&&!catalog.truncated,
        "Schema54 Recovery changed bounded history admission");
    const auto& entries=catalog.histories.front().entries;
    check(entries.size()==6&&std::all_of(entries.begin(),entries.end(),[](const auto& entry){
        return entry.loadable()&&entry.schema==54&&entry.rule_version==38;}),
        "Schema54 Recovery changed its ordinary start-plus-five rotation");
    unsigned starts=0;for(const auto& entry:entries) {
        starts+=entry.start_point;
        check(entry.start_point?entry.sequence==0:entry.sequence>=4&&entry.sequence<=8,
            "Schema54 Recovery retained an incorrect sequence after ordinary rotation");
    }
    check(starts==1&&f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model&&
        f::read_text(fixture.root.data/f::map_relative)==map,"Save/Recovery changed authored source bytes");
}
void actual_quality_qualified_raw_trip_restores_without_reselection() {
    Fixture fixture(70);auto world=fixture.world();const auto scene=e::earn_loaded_raw(world);
    const auto before=e::trip(world,scene.courier);const auto initial=e::stock(world,scene.target).received;
    check(before.transfer==scene.transfer&&before.source==scene.city.market&&before.target==scene.target&&
        before.loaded[4]>0&&before.phase==sim::OriginalGoodsTransferPhase::Loaded&&
        world.courier(scene.courier).phase==sim::CourierPhase::ToWarehouse,
        "Schema54 persistence scene did not earn a genuinely qualified loaded raw transfer");
    const auto path=fixture.root.path/"loaded-qualified-raw54.json";auto restored=roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"raw-recovery54",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
    check(entry.schema==54&&entry.rule_version==38&&f::read_text(entry.path)==f::read_text(path)&&
        fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
        "Qualified raw cargo changed its exact manual/Recovery payload");
    bool arrived=false;
    for(unsigned tick=0;tick<256;++tick) {
        world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),
            "Restored qualified trip was reselected, granted early or changed ordinary physical continuation");
        if(e::stock(world,scene.target).received[4]>initial[4]){arrived=true;break;}
    }
    check(arrived&&e::trip(world,scene.courier).transfer==before.transfer&&
        e::trip(world,scene.courier).delivered[4]>0&&e::house(world,scene.target).food_quality>=70,
        "Loaded qualified raw never physically arrived at its unchanged real recipient");
    (void)roundtrip(fixture,world,fixture.root.path/"qualified-raw-arrived54.json");e::valid(world);
}
void required_markers_unknown_fields_and_atomic_failures_refuse() {
    Fixture fixture;auto world=fixture.world();const auto scene=t::build_early_food(world);t::ticks(world,5);
    const auto path=fixture.root.path/"hostile54.json";(void)roundtrip(fixture,world,path);
    const auto bytes=f::read_text(path);const auto canonical=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why) {
        auto value=canonical;mutate(value);const auto supplied=value.dump();f::write_text(path,supplied);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before&&f::read_text(path)==supplied,
            "Rejected schema54 restore changed live World or supplied hostile bytes");f::write_text(path,bytes);
    };
    reject([](auto& j){j["rules"].erase("quality_aware_food_dispatch");},"Missing54 marker accepted");
    reject([](auto& j){j["rules"]["quality_aware_food_dispatch"]="other";},"Wrong54 marker accepted");
    reject([](auto& j){j["rules"].erase("quality_aware_food_dispatch_revision");},"Missing54 revision accepted");
    reject([](auto& j){j["rules"]["quality_aware_food_dispatch_revision"]=2;},"Wrong54 revision accepted");
    reject([](auto& j){j["rules"].erase("irrigated_harvest");},"Schema54 omitted inherited53 marker");
    reject([](auto& j){j["rules"]["quality_aware_food_dispatch_policy"]=1;},"Unknown54 dispatch authority accepted");
    reject([](auto& j){j["world"]["quality_aware_food_dispatch"]={{"need",70}};},"Invented54 saved needs accepted");
    reject([](auto& j){j["quality_aware_food_dispatch_revision"]=1;},"Top-level54 authority accepted");
    reject([](auto& j){j["schema_version"]=53;},"Schema54 marker silently accepted by historical53");
    reject([](auto& j){j["rules"]["version"]=37;},"Schema54 accepted historical37 authority");
    auto duplicate=bytes;const std::string key="\"quality_aware_food_dispatch_revision\": 1";
    const auto at=duplicate.find(key);check(at!=std::string::npos,"Duplicate54 control lost exact marker");
    duplicate.replace(at,key.size(),key+",\n    "+key);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate54 save key accepted");
    check(f::read_text(path)==duplicate&&world.snapshot()==before,"Duplicate54 refusal changed bytes or authority");
    f::write_text(path,bytes);
    save::RecoveryStore recovery(fixture.root.path/"hostile-recovery54",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto document=fixture.document(world);const auto history=recovery.create_history(document,fixture.mask());
    const auto retained=inventory(recovery.root());auto forged=document;
    auto field=std::find_if(forged.world.physical_food_growers->fields.begin(),forged.world.physical_food_growers->fields.end(),
        [&](const auto& row){return row.field_id==scene.field;});
    check(field!=forged.world.physical_food_growers->fields.end(),"Hostile54 source-return row missing");++field->returned;
    refuses([&]{save::write_save(path,forged,fixture.root.data,fixture.mask());},"Writer published unearned source return54");
    refuses([&]{(void)recovery.write_checkpoint(history,forged,fixture.mask());},"Recovery published unearned source return54");
    check(inventory(recovery.root())==retained&&f::read_text(path)==bytes&&world.snapshot()==before,
        "Rejected54 writer/Recovery changed files, IDs, stock or Funds");
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},
        "Atomic54 publication fault was ignored");
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Failed54 publication replaced valid bytes or live World");
}
void historical53_and_quality54_never_migrate_or_overwrite() {
    Fixture fixture;auto current=fixture.world();auto old=sim::World::create_irrigated_harvest(fixture.policy,fixture.model,fixture.terrain);
    const auto path54=fixture.root.path/"current54.json",path53=fixture.root.path/"historical53.json";
    (void)roundtrip(fixture,current,path54);save::write_save(path53,fixture.document(old),fixture.root.data,fixture.mask());
    const auto bytes54=f::read_text(path54),bytes53=f::read_text(path53);
    const auto json54=Json::parse(bytes54),json53=Json::parse(bytes53);
    const auto doc54=save::read_save(path54),doc53=save::read_save(path53);
    check(doc53.source_schema_version==53&&doc53.world.rule_version==37&&
        !json53.at("rules").contains("quality_aware_food_dispatch")&&
        !json53.at("rules").contains("quality_aware_food_dispatch_revision")&&
        json54.at("world")==json53.at("world")&&
        save::restore_irrigated_harvest(doc53,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain).snapshot()==old.snapshot(),
        "Historical37/schema53 changed or schema54 introduced parallel saved state");
    refuses([&]{(void)fixture.restore(doc53);},"Historical53 implicitly migrated to38");
    refuses([&]{(void)save::restore_irrigated_harvest(doc54,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Schema54 silently restored through historical37 API");
    refuses([&]{save::write_save(path53,fixture.document(current),fixture.root.data,fixture.mask());},"Rule38 overwrote historical53");
    refuses([&]{save::write_save(path54,fixture.document(old),fixture.root.data,fixture.mask());},"Historical37 overwrote54");
    auto mismatch=fixture.document(current);mismatch.source_schema_version=53;
    refuses([&]{save::write_save(path54,mismatch,fixture.root.data,fixture.mask());},"Rule38 migrated retained53 document identity");
    check(f::read_text(path54)==bytes54&&f::read_text(path53)==bytes53,"Cross-generation refusal changed canonical retained bytes");
    for(const auto& text:{std::string("{\"schema_version\":54,\"damaged\":true}"),
                         std::string("{\"schema_version\":\"54\",\"damaged\":true}")}) {
        const auto damaged=fixture.root.path/"damaged54.json";f::write_text(damaged,text);
        refuses([&]{save::write_save(damaged,fixture.document(old),fixture.root.data,fixture.mask());},"Old writer replaced damaged54");
        refuses([&]{save::write_save(damaged,fixture.document(current),fixture.root.data,fixture.mask());},"New writer replaced invalid54");
        check(f::read_text(damaged)==text,"Damaged54 target lost supplied bytes");
    }
    save::RecoveryStore recovery(fixture.root.path/"generation-recovery54",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(current),fixture.mask());const auto retained=inventory(recovery.root());
    refuses([&]{(void)recovery.write_checkpoint(history,fixture.document(old),fixture.mask());},"Recovery mixed53/54 in one history");
    auto retained53=fixture.document(current);retained53.source_schema_version=53;
    refuses([&]{(void)recovery.write_checkpoint(history,retained53,fixture.mask());},"Recovery migrated retained53 document identity");
    check(inventory(recovery.root())==retained,"Cross-generation Recovery refusal changed retained history bytes");
    save::RecoveryStore legacy(fixture.root.path/"legacy-access54",fixture.root.data);
    refuses([&]{(void)legacy.create_history(fixture.document(current),fixture.mask());},"Legacy Recovery implicitly admitted38");
}
} // namespace
int main(){try {
    actual_earned_cargo_and_bounded_recovery_continue_exactly();
    actual_quality_qualified_raw_trip_restores_without_reselection();
    required_markers_unknown_fields_and_atomic_failures_refuse();
    historical53_and_quality54_never_migrate_or_overwrite();
    std::cout<<"Quality-aware Food dispatch schema54 exact-save/Recovery/hostile/generation tests passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
