#include "HuntingFixture.h"
#include "persistence/RecoveryStore.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace h=oe_hunting_fixture;
namespace g=oe_fishing_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;
template<class F>void refuses(F operation,const char* why) {
    try {operation();}catch(const std::exception&){return;}throw std::runtime_error(why);
}
const sim::HuntingFoodTrip* trip(const sim::World& world,sim::CourierId actor) {
    for(const auto& row:world.hunting_food_state()->trips)if(row.transfer.courier_id==actor)return &row;
    return nullptr;
}
std::string roundtrip(const h::Fixture& fixture,const sim::World& world,const fs::path& path,int continuation=31) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto j=Json::parse(bytes);const auto document=save::read_save(path);
    check(document.source_schema_version==45 && document.world.rule_version==sim::kPlayableHuntingRuleVersion &&
        document.world==before && !document.migrated_from_schema1 && document.world.hunting && document.world.hunting_food &&
        !document.world.fishing_food && !document.world.agriculture && document.world.fishing && document.world.irrigation &&
        document.world.plantations && document.world.original_production->food==sim::OriginalFoodState{} &&
        j.at("rules").at("playable_hunting")==sim::playable_hunting_contract &&
        j.at("rules").at("playable_hunting_revision")==sim::playable_hunting_revision &&
        j.at("world").at("hunting_food").at("catalog")==sim::hunting_food_catalog &&
        !j.at("world").contains("agriculture") && !j.at("world").contains("fishing_food"),
        "Schema45 lost its exact sole seven-kind authority or inherited generation");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Hunting restore granted or lost state before any tick");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Canonical Hunting rewrite changed state or bytes");
    auto expected=world;
    for(int i=0;i<continuation;++i) {
        expected.tick();restored.tick();check(expected.snapshot()==restored.snapshot(),"Hunting Save/Load deterministic continuation diverged");
    }
    h::valid(expected);h::valid(restored);return bytes;
}
void physical_lifecycle_delivery_and_recovery() {
    h::Fixture fixture;auto world=fixture.world();const auto scene=h::build(world,true);
    const auto path=fixture.root.path/"hunting45.json";roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"hunting-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    std::array<bool,4> phases{};std::array<bool,7> market{},house{};std::size_t checkpoints=0;
    const auto all=[](const auto& flags){return std::all_of(flags.begin(),flags.end(),[](bool value){return value;});};
    const auto eaten=[&]{for(std::size_t i=0;i<7;++i)if(!h::consumed(world,i))return false;return true;};
    for(int tick=0;tick<40000 && !(all(phases) && all(market) && all(house) && eaten());++tick) {
        world.tick();
        for(const auto& hunter:world.hunting_state()->hunters) {
            const auto phase=static_cast<std::size_t>(hunter.phase);
            const bool meaningful=hunter.phase==sim::HuntingHunterPhase::Pursuing ? hunter.edge_progress>0:
                hunter.phase==sim::HuntingHunterPhase::Hunting ? hunter.work_progress>0:
                hunter.phase==sim::HuntingHunterPhase::Returning ? hunter.cargo>0 && hunter.edge_progress>0:
                hunter.cargo>0 && hunter.work_progress>0;
            if(phases.at(phase) || !meaningful)continue;
            const auto inventory=h::stock(world,hunter.tent_id);roundtrip(fixture,world,path);
            const auto restored=fixture.restore(save::read_save(path));
            check(h::stock(restored,hunter.tent_id)==inventory && restored.hunting_state()==world.hunting_state(),
                "Reload converted unreturned Hunter cargo into Tent Food");
            const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
            check(entry.loadable() && entry.schema==45 && entry.rule_version==29 &&
                fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost physical Hunter state");
            phases[phase]=true;
        }
        for(const auto& courier:world.couriers()) {
            if(courier.phase!=sim::CourierPhase::ToWarehouse || courier.edge_progress<=0)continue;
            const auto* load=trip(world,courier.id);if(!load || load->phase!=sim::OriginalGoodsTransferPhase::Loaded)continue;
            auto* flags=courier.role==sim::CourierRole::OriginalFoodToMarket ? &market:
                courier.role==sim::CourierRole::OriginalFoodToHouse ? &house:nullptr;
            if(!flags)continue;
            bool needed=false;for(std::size_t i=0;i<7;++i)needed|=!(*flags)[i] && load->loaded[i]>0;
            if(!needed)continue;
            const auto inventory=h::stock(world,load->target);const auto cargo=*load;roundtrip(fixture,world,path);
            const auto restored=fixture.restore(save::read_save(path));
            check(trip(restored,courier.id) && *trip(restored,courier.id)==cargo && h::stock(restored,load->target)==inventory,
                "Reload delivered Food before actual cart arrival or changed its seven-kind identity");
            for(std::size_t i=0;i<7;++i)if(load->loaded[i]>0)(*flags)[i]=true;
        }
    }
    check(all(phases) && all(market) && all(house) && eaten(),
        "Paid Hunting city did not retain all four Hunter phases, all seven physical Food chains and actual consumption");
    check(h::source(world,scene.tent).unloaded>0 && h::stock(world,scene.market).received[6]>0 &&
        h::kills(world)>0,"Hunting save control never earned prey death, physical Tent receipt and Market arrival");
    roundtrip(fixture,world,path,12000);
    for(int i=0;i<3;++i) {h::ticks(world,19);recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;}
    const auto catalog=recovery.catalog();check(!catalog.truncated && catalog.histories.size()==1 &&
        catalog.histories[0].error.empty() && catalog.histories[0].entries.size()==std::min(checkpoints,save::recovery_checkpoint_limit)+1,
        "Hunting Recovery violated bounded checkpoint/start retention");
    std::size_t starts=0;for(const auto& entry:catalog.histories[0].entries) {
        check(entry.loadable() && entry.schema==45 && entry.rule_version==29,"Recovery relabelled Hunting generation");starts+=entry.start_point;
    }
    check(starts==1,"Recovery replaced the immutable Hunting start point");
}
void hostile_authority_and_atomic_publication() {
    h::Fixture fixture;auto world=fixture.world();const auto scene=h::build(world,false);
    h::until(world,[&]{for(const auto& hunter:world.hunting_state()->hunters)
        if(hunter.cargo>0 && hunter.phase==sim::HuntingHunterPhase::Returning && hunter.edge_progress>0)return true;
        return false;},"Hunting hostile save control did not earn real returning Meat cargo");
    const auto path=fixture.root.path/"hostile45.json";const auto bytes=roundtrip(fixture,world,path);
    const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* message) {
        auto value=original;mutate(value);const auto encoded=value.dump();f::write_text(path,encoded);
        refuses([&]{const auto document=save::read_save(path);(void)fixture.restore(document);},message);
        check(world.snapshot()==before && f::read_text(path)==encoded,"Rejected hostile save changed World or supplied bytes");
    };
    reject([](auto& j){j["rules"]["playable_hunting"]="unknown";},"Wrong Hunting marker accepted");
    reject([](auto& j){j["rules"]["playable_hunting_revision"]=2;},"Wrong Hunting revision accepted");
    reject([](auto& j){j["world"]["hunting_food"]["catalog"]=sim::fishing_food_catalog;},"Six-kind catalog accepted by seven-kind generation");
    reject([](auto& j){j["world"]["hunting_food"]["stocks"][0]["amounts"].erase(6);},"Truncated Food width accepted");
    reject([](auto& j){j["world"]["hunting_food"]["stocks"][0]["amounts"][6]=-1;},"Negative Meat accepted");
    reject([](auto& j){j["world"]["hunting_food"]["stocks"][0]["amounts"][6]=1;},"Unearned Meat accepted");
    reject([](auto& j){j["world"]["hunting_food"]["revision"]=0;},"Wrong Food revision accepted");
    reject([](auto& j){j["world"]["fishing_food"]=j["world"]["hunting_food"];},"Duplicate six-kind authority accepted");
    reject([](auto& j){j["world"]["hunting"]["animals"].push_back(j["world"]["hunting"]["animals"][0]);},"Duplicate AnimalId accepted");
    reject([](auto& j){j["world"]["hunting"]["animals"][0]["species"]=74;},"Unknown prey species accepted");
    reject([](auto& j){j["world"]["hunting"]["animals"][0]["reserved_by"]=999999;},"Foreign Hunter reservation accepted");
    reject([](auto& j){j["world"]["hunting"]["animals"][0]["born_tick"]=j["world"]["ticks"].template get<std::uint64_t>()+1;},"Future animal birth accepted");
    reject([](auto& j){j["world"]["hunting"]["habitats"][0]["births"]=99999;},"Invented animal births accepted");
    reject([](auto& j){j["world"]["hunting"]["habitats"][0]["killed"]=99999;},"Invented kills accepted");
    reject([](auto& j){j["world"]["hunting"]["habitats"][0]["regeneration_progress"]=sim::hunting_regeneration_ticks;},"Invalid ecology progress accepted");
    reject([](auto& j){j["world"]["hunting"]["tents"][0]["unloaded"]=99999;},"Unreturned Meat credited by save");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["id"]=0;},"Zero HunterId accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"].push_back(j["world"]["hunting"]["hunters"][0]);},"Duplicate Hunter accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["tent_id"]=999999;},"Foreign Tent accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["phase"]=99;},"Unknown Hunter phase accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["movement"]=99;},"Unknown movement accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["cargo"]=2;},"Over-capacity Hunter cargo accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["reserved"]=0;},"Missing Hunter return reservation accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["kill_cell"]=nullptr;},"Meat without actual kill location accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["killed_tick"]=nullptr;},"Meat without actual kill time accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["launched_tick"]=0;},"Impossible Hunter launch accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["killed_tick"]=j["world"]["ticks"];},"Instant kill and moving return accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["edge_progress"]=sim::hunting_edge_ticks;},"Invalid begun edge accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["path"].back()=j["world"]["hunting"]["hunters"][0]["current"];},"Wrong return endpoint accepted");
    reject([](auto& j){j["world"]["hunting"]["hunters"][0]["movement"]=static_cast<unsigned>(sim::HuntingMovementResult::Arrived);},"Moving return accepted an Arrived result");
    reject([](auto& j){auto& route=j["world"]["hunting"]["hunters"][0]["path"];check(route.size()>2,"Hunter loop control route too short");
        const auto first=route[0],second=route[1];route.insert(route.begin()+2,first);route.insert(route.begin()+3,second);},"Repeated cardinal Hunter route loop accepted");
    reject([](auto& j){j["world"]["hunting"]["next_hunter_id"]=1;},"Reused Hunter highwater accepted");
    f::write_text(path,bytes);
    auto duplicate=bytes;const auto key=std::string("\"playable_hunting\": \"")+sim::playable_hunting_contract+"\"";
    const auto at=duplicate.find(key);check(at!=std::string::npos,"Hunting duplicate marker test lost its canonical source");
    duplicate.replace(at,key.size(),key+",\n    "+key);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate Hunting marker accepted");
    f::write_text(path,bytes);
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},
        "Injected Hunting atomic publication failure ignored");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Failed atomic Hunting save changed target or World");
    (void)scene;
}
void exact_generation_boundaries() {
    h::Fixture current;auto world=current.world();const auto target=current.root.path/"hunting45.json";
    save::write_save(target,current.document(world),current.root.data,current.mask());const auto bytes=f::read_text(target);
    g::Fixture historical;auto old=historical.world();const auto old_target=historical.root.path/"fishing44.json";
    save::write_save(old_target,historical.document(old),historical.root.data,historical.mask());const auto old_bytes=f::read_text(old_target);
    const auto old_document=save::read_save(old_target);check(old_document.source_schema_version==44 && old_document.world.fishing_food &&
        !old_document.world.hunting_food && !old_document.world.hunting,"Historical Fishing save reinterpreted as Hunting");
    refuses([&]{save::write_save(old_target,current.document(world),current.root.data,current.mask());},"Hunting overwrote schema44");
    refuses([&]{save::write_save(target,historical.document(old),historical.root.data,historical.mask());},"Fishing overwrote schema45");
    refuses([&]{(void)save::restore_playable_hunting(old_document,historical.root.data,historical.mask(),historical.policy,historical.model,historical.terrain);},
        "Hunting factory migrated schema44");
    auto wrong=old_document;wrong.world.hunting_food.emplace();wrong.world.hunting.emplace();
    refuses([&]{(void)historical.restore(wrong);},"Historical Fishing accepted Hunting authority");
    check(f::read_text(old_target)==old_bytes && f::read_text(target)==bytes,"Generation rejection changed target bytes");
}
}
int main() {
    try {physical_lifecycle_delivery_and_recovery();hostile_authority_and_atomic_publication();exact_generation_boundaries();
        std::cout<<"Hunting Save/Recovery tests passed\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
