#include "FishingFixture.h"
#include "persistence/RecoveryStore.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace g=oe_fishing_fixture;
namespace m=oe_playable_agriculture_fixture;
namespace q=oe_irrigation_fixture;
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;
template<class F>void refuses(F operation,const char* why) {
    try {operation();}catch(const std::exception&){return;}throw std::runtime_error(why);
}
const sim::FishingFoodTrip* trip(const sim::World& world,sim::CourierId actor) {
    const auto& rows=world.fishing_food_state()->trips;
    const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.transfer.courier_id==actor;});
    return at==rows.end() ? nullptr:&*at;
}
sim::FishingFoodCounters consumed(const sim::World& world) {
    sim::FishingFoodCounters out{};for(const auto& stock:world.fishing_food_state()->stocks)
        for(std::size_t i=0;i<out.size();++i)out[i]+=stock.consumed[i];return out;
}
bool positive(const sim::FishingFoodCounters& counts) {
    return std::all_of(counts.begin(),counts.end(),[](auto n){return n>0;});
}
std::string roundtrip(const g::Fixture& fixture,const sim::World& world,const fs::path& path,int continuation=73) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto j=Json::parse(bytes);const auto saved=save::read_save(path);
    check(saved.source_schema_version==44 && saved.world.rule_version==sim::kPlayableFishingRuleVersion &&
        saved.world==before && !saved.migrated_from_schema1 && !saved.world.agriculture &&
        saved.world.fishing_food && saved.world.fishing && saved.world.irrigation && saved.world.plantations &&
        saved.world.original_production->food==sim::OriginalFoodState{} &&
        j.at("rules").at("playable_fishing")==sim::playable_fishing_contract &&
        j.at("rules").at("playable_fishing_revision")==sim::playable_fishing_revision &&
        j.at("world").at("fishing_food").at("catalog")==sim::fishing_food_catalog &&
        !j.at("world").contains("agriculture"),"Schema44 lost its exact sole six-kind authority or inherited state");
    auto restored=fixture.restore(saved);check(restored.snapshot()==before,"Fishing restore granted or lost state before any tick");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Canonical Fishing rewrite mutated source/bytes");
    auto expected=world;
    for(int i=0;i<continuation;++i) {
        expected.tick();restored.tick();check(expected.snapshot()==restored.snapshot(),"Fishing Save/Load deterministic continuation diverged");
    }
    g::valid(expected);g::valid(restored);return bytes;
}
void every_boat_phase_and_both_delivery_legs() {
    g::Fixture fixture;auto world=fixture.world();const auto scene=g::build(world,true);
    const auto quay_courier=std::find_if(world.couriers().begin(),world.couriers().end(),[&](const auto& c){
        return c.owner==scene.quay && c.role==sim::CourierRole::OriginalFoodToMarket;
    });
    check(quay_courier!=world.couriers().end(),"Paid Quay lacks its actual Food courier");
    for(const auto farm:scene.farms) {
        const auto actor=std::find_if(world.couriers().begin(),world.couriers().end(),[&](const auto& c){
            return c.owner==farm && c.role==sim::CourierRole::OriginalFoodToMarket;
        });
        check(farm<scene.quay && actor!=world.couriers().end() && actor->id<quay_courier->id,
            "Six-food positive control must keep Quay behind every Farm in the existing dispatch order");
    }
    const auto path=fixture.root.path/"fishing44.json";roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    std::array<bool,4> boat_phases{};std::array<bool,6> to_market{},to_house{};std::size_t checkpoints=0;
    for(int tick=0;tick<30000 && (!std::all_of(boat_phases.begin(),boat_phases.end(),[](bool n){return n;}) ||
        !std::all_of(to_market.begin(),to_market.end(),[](bool n){return n;}) ||
        !std::all_of(to_house.begin(),to_house.end(),[](bool n){return n;}) || !positive(consumed(world)));++tick) {
        world.tick();
        for(const auto& boat:world.fishing_state()->boats) {
            const auto phase=static_cast<std::size_t>(boat.phase);
            const bool meaningful=boat.phase==sim::FishingBoatPhase::Outbound ? boat.edge_progress>0:
                boat.phase==sim::FishingBoatPhase::Fishing ? boat.cargo>0 && boat.work_progress>0:
                boat.phase==sim::FishingBoatPhase::Returning ? boat.cargo>0 && boat.edge_progress>0:
                boat.cargo>0 && boat.work_progress>0;
            if(boat_phases.at(phase) || !meaningful)continue;
            const auto before_stock=g::stock(world,boat.quay_id);const auto before_boats=world.fishing_state()->boats;
            roundtrip(fixture,world,path);
            const auto reloaded=fixture.restore(save::read_save(path));
            check(g::stock(reloaded,boat.quay_id)==before_stock && reloaded.fishing_state()->boats==before_boats,
                "Reload credited boat cargo into Quay Food before actual unloading");
            const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
            check(entry.loadable() && entry.schema==44 && entry.rule_version==28 &&
                fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost actual boat motion/work/cargo");
            boat_phases[phase]=true;
        }
        for(const auto& courier:world.couriers()) {
            if(courier.phase!=sim::CourierPhase::ToWarehouse || courier.edge_progress<=0)continue;
            const auto* cargo=trip(world,courier.id);if(!cargo || cargo->phase!=sim::OriginalGoodsTransferPhase::Loaded)continue;
            auto* flags=courier.role==sim::CourierRole::OriginalFoodToMarket ? &to_market:
                courier.role==sim::CourierRole::OriginalFoodToHouse ? &to_house:nullptr;
            if(!flags)continue;
            for(std::size_t i=0;i<flags->size();++i)if(!(*flags)[i] && cargo->loaded[i]>0) {
                const auto before_target=g::stock(world,cargo->target);const auto before_cargo=*cargo;
                roundtrip(fixture,world,path);
                const auto restored=fixture.restore(save::read_save(path));const auto* saved=trip(restored,courier.id);
                check(saved && *saved==before_cargo && g::stock(restored,cargo->target)==before_target,
                    "Reload credited Food or changed resource identity before physical arrival");
                (*flags)[i]=true;
            }
        }
    }
    check(std::all_of(boat_phases.begin(),boat_phases.end(),[](bool n){return n;}),"Not all four actual boat phases survived Save/Load");
    check(std::all_of(to_market.begin(),to_market.end(),[](bool n){return n;}) &&
        std::all_of(to_house.begin(),to_house.end(),[](bool n){return n;}) && positive(consumed(world)),
        "Six independently produced foods failed both physical delivery legs and actual consumption");
    check(g::source(world,scene.quay).unloaded>0 && g::stock(world,scene.market).received[5]>0,
        "Fish save control never earned a dock/Market arrival");
    roundtrip(fixture,world,path,816);
    for(int i=0;i<3;++i) {
        g::ticks(world,17);const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
        check(entry.loadable() && fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),"Fishing Recovery continuation failed");
    }
    const auto catalog=recovery.catalog();check(!catalog.truncated && catalog.histories.size()==1 &&
        catalog.histories[0].error.empty() && catalog.histories[0].entries.size()==std::min(checkpoints,save::recovery_checkpoint_limit)+1,
        "Fishing Recovery violated bounded checkpoint/start retention");
    std::size_t starts=0;for(const auto& entry:catalog.histories[0].entries) {
        check(entry.loadable() && entry.schema==44 && entry.rule_version==28,"Recovery relabelled Fishing generation");starts+=entry.start_point;
    }
    check(starts==1,"Recovery replaced its immutable Fishing start point");
}
void returning_fish_and_atomic_write_failure() {
    g::Fixture fixture;auto world=fixture.world();const auto scene=g::build(world,false);
    sim::CourierId moving{};
    g::until(world,[&]{for(const auto& c:world.couriers()) {const auto* row=trip(world,c.id);
        if(c.owner==scene.quay && c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 && row && row->loaded[5]>0) {moving=c.id;return true;}}
        return false;},"Fish never reached actual loaded land transport");
    f::buy(world,sim::set_building_operation(scene.quay,false));
    g::until(world,[&]{const auto* row=trip(world,moving);return row && row->phase==sim::OriginalGoodsTransferPhase::Returning;},
        "Paused Quay discarded its real outgoing Fish cargo");
    const auto path=fixture.root.path/"return44.json";const auto bytes=roundtrip(fixture,world,path,700);
    const auto before=world.snapshot();refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,
        fixture.mask(),save::WriteFault::BeforeRename);},"Injected Fishing atomic publication failure was ignored");
    check(world.snapshot()==before && f::read_text(path)==bytes,"Failed Fishing write altered cargo, source World or target bytes");
}
void actual_dock_endpoints_and_fire_witnesses() {
    g::Fixture fixture;
    // Keep the complete two-row water intake, but add real land on the
    // opposite side of its exterior dock. The dock is the only Quay entrance.
    for(int x=96;x<=114;++x)fixture.raw[28U*228U+static_cast<std::size_t>(x)]=0x80U;
    for(int y=26;y<=27;++y)for(int x=114;x<=115;++x)
        fixture.raw[std::size_t(y)*228U+std::size_t(x)]=0x80U;
    fixture.rebind();auto world=fixture.world();
    for(const int y:{24,27})for(int x=19;x<=79;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
    for(const int x:{19,79})for(const int y:{25,26})f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
    for(int x=79;x<=114;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,28}});
    for(int x=20;x<=29;x+=3)(void)g::purchase(world,sim::CommandType::PlaceHousehold,{x,22});
    const auto market=g::purchase(world,sim::CommandType::PlaceMarket,{64,25});
    (void)g::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{68,25});
    (void)g::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{72,25});
    (void)g::purchase(world,sim::CommandType::PlaceFireWatch,{30,28});
    const auto watch=g::purchase(world,sim::CommandType::PlaceFireWatch,{114,26});
    const auto quay=g::quay(world);const auto dock=orig::fishing_quay_geometry(g::quay_origin,0).dock;
    check(!sim::building_footprint_contains(world.profile(),world.rule_version(),sim::Object::OriginalFishingQuay,
        g::quay_origin,dock) && world.building_contains(quay,dock),"Dock control accidentally remained inside the rectangular body");
    bool outward=false,returning=false,inspected=false,inspector_idle=false;
    const auto path=fixture.root.path/"dock44.json";
    for(int tick=0;tick<20000 && !(outward && returning && inspected && inspector_idle);++tick) {
        world.tick();
        for(const auto& courier:world.couriers()) {
            const auto* food=trip(world,courier.id);
            if(courier.owner==quay && food && courier.edge_progress>0 && !courier.path.empty()) {
                if(!outward && courier.phase==sim::CourierPhase::ToWarehouse && food->loaded[5]>0) {
                    check(courier.path.front()==dock,"Quay Food dispatch did not originate at its only actual dock entrance");
                    roundtrip(fixture,world,path);outward=true;
                }
                if(!returning && courier.phase==sim::CourierPhase::Returning) {
                    check(courier.path.back()==dock,"Quay Food return omitted its actual exterior dock endpoint");
                    roundtrip(fixture,world,path);returning=true;
                }
            }
            if(!inspector_idle && courier.role==sim::CourierRole::FireInspector &&
                courier.phase==sim::CourierPhase::IdleAtWorkshop) {
                const auto& arrivals=world.original_production_state()->fire_protection->arrivals;
                const auto last=std::find_if(arrivals.rbegin(),arrivals.rend(),[&](const auto& row){return row.trip.courier_id==courier.id;});
                if(last!=arrivals.rend() && last->trip.target==quay && last->trip.travelled.back()==dock) {
                    roundtrip(fixture,world,path);inspector_idle=true;
                }
            }
        }
        if(!inspected) for(const auto& arrival:world.original_production_state()->fire_protection->arrivals)
            if(arrival.trip.target==quay && arrival.trip.travelled.back()==dock) {
                roundtrip(fixture,world,path);inspected=true;break;
            }
    }
    check(outward && returning && inspected && inspector_idle && world.workers_assigned(watch)==sim::Rules::fire_watch_workers &&
        g::stock(world,market).received[5]>0,
        "Dock geometry did not earn real Food departure/return and actual Inspector arrival/return witnesses");
    g::valid(world);
}
void hostile_state_and_source_identity() {
    g::Fixture fixture;auto world=fixture.world();const auto scene=g::build(world,false);
    std::optional<sim::WorldSnapshot> working;
    g::until(world,[&]{
        for(const auto& boat:world.fishing_state()->boats)
            if(!working && boat.phase==sim::FishingBoatPhase::Fishing && boat.cargo>0 && boat.work_progress>0)
                working=world.snapshot();
        return std::any_of(world.fishing_state()->boats.begin(),world.fishing_state()->boats.end(),
            [](const auto& boat){return boat.cargo>0 && boat.phase==sim::FishingBoatPhase::Returning;});},
        "Hostile save control lacked actual returning boat cargo");
    check(working.has_value(),"Hostile save control lacked actual fishing work after a real catch");
    const auto path=fixture.root.path/"canonical44.json",hostile=fixture.root.path/"hostile44.json";
    const auto bytes=roundtrip(fixture,world,path);const auto canonical=Json::parse(bytes);const auto before=world.snapshot();
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt",map_path=fixture.root.data/f::map_relative;
    const auto model=f::read_text(model_path),map=f::read_text(map_path);
    const auto unchanged=[&]{check(world.snapshot()==before && f::read_text(path)==bytes &&
        f::read_text(model_path)==model && f::read_text(map_path)==map,"Rejected Fishing authority changed source, World or target");};
    const auto reject=[&](auto change){auto j=canonical;change(j);f::write_text(hostile,j.dump());
        refuses([&]{(void)fixture.restore(save::read_save(hostile));},"Manipulated Fishing save became gameplay authority");unchanged();};
    reject([](auto& j){j["rules"].erase("playable_fishing");});
    reject([](auto& j){j["rules"]["playable_fishing"]="original-runtime-parity";});
    reject([](auto& j){j["rules"]["playable_fishing_revision"]=2;});
    reject([](auto& j){j["schema_version"]=43;j["rules"]["version"]=27;});
    reject([](auto& j){j["world"]["agriculture"]=j["world"]["fishing_food"];});
    reject([](auto& j){j["world"]["fishing_food"]["catalog"]=sim::agriculture_catalog;});
    reject([](auto& j){j["world"]["fishing_food"]["stocks"][0]["amounts"].push_back(0);});
    reject([](auto& j){j["world"]["fishing_food"]["stocks"][0]["amounts"].erase(5);});
    reject([](auto& j){j["world"]["fishing_food"]["stocks"][0]["amounts"][5]=-1;});
    reject([](auto& j){j["world"]["fishing_food"]["stocks"][0]["amounts"][5]=1;});
    reject([](auto& j){j["world"]["fishing"]["quays"][0]["orientation"]=4;});
    reject([](auto& j){j["world"]["fishing"]["quays"][0]["unloaded"]=UINT64_MAX;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["quay_id"]=0;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["id"]=0;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["site_index"]=16;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["phase"]=4;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["movement"]=4;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["cargo"]=5;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["reserved"]=0;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["path_vertex"]=100000;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["path"][0]=Json::array({20,22});});
    reject([](auto& j){
        auto& path=j["world"]["fishing"]["boats"][0]["path"];
        check(path.size()>1,"Actual returning path lacks a removable valid water endpoint");
        path.erase(path.size()-1); // Still legal contiguous water, but not the assigned dock.
    });
    reject([](auto& j){
        auto& path=j["world"]["fishing"]["boats"][0]["path"];
        check(path.size()>1,"Actual returning path lacks a valid water edge");
        auto loop=Json::array({path[0],path[1],path[0]});
        for(std::size_t i=1;i<path.size();++i)loop.push_back(path[i]);
        path=std::move(loop); // Every edge remains cardinal water; the route repeats cells.
    });
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["movement"]=static_cast<unsigned>(sim::FishingMovementResult::Arrived);});
    reject([](auto& j){
        auto& boat=j["world"]["fishing"]["boats"][0];
        boat["phase"]=static_cast<unsigned>(sim::FishingBoatPhase::Outbound);
        boat["path"]=Json::array({boat["current"]});boat["path_vertex"]=0;boat["edge_progress"]=0;
        // The actual current site is now the route endpoint, but an outward actor cannot own this earned cargo.
    });
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["edge_progress"]=5;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["launched_tick"]=UINT64_MAX;});
    reject([](auto& j){j["world"]["fishing"]["boats"][0]["launched_tick"]=0;});
    const auto latest_launch=world.ticks()-((world.ticks()-32U)%51U);
    reject([&](auto& j){j["world"]["fishing"]["boats"][0]["launched_tick"]=latest_launch;});
    // latest_launch is a valid, already elapsed phase31 dispatch, but its age is
    // below51 ticks and cannot account for four real40-tick catches plus travel.

    reject([](auto& j){j["world"]["fishing"]["sites"][0]["stock"]=9;});
    reject([](auto& j){j["world"]["fishing"]["sites"][0]["regeneration_progress"]=sim::fishing_regeneration_ticks;});
    reject([](auto& j){j["world"]["fishing"]["sites"][0]["regenerated"]=UINT64_MAX;});
    auto duplicate=bytes;const auto at=duplicate.find("\"playable_fishing\":");check(at!=std::string::npos,"Fishing marker absent");
    duplicate.insert(at,"\"playable_fishing\":\"authored-playable-fishing-v1\",");f::write_text(hostile,duplicate);
    refuses([&]{(void)save::read_save(hostile);},"Duplicate Fishing contract entered authority");unchanged();
    auto duplicate_counter=bytes;const auto counter_at=duplicate_counter.find("\"next_boat_id\":");
    check(counter_at!=std::string::npos,"Canonical Fishing boat highwater missing");
    duplicate_counter.insert(counter_at,"\"next_boat_id\":"+canonical.at("world").at("fishing").at("next_boat_id").dump()+",");
    f::write_text(hostile,duplicate_counter);
    refuses([&]{(void)save::read_save(hostile);},"Duplicate Fishing identity counter entered authority");unchanged();
    auto duplicate_catalog=bytes;const auto food_at=duplicate_catalog.find("\"fishing_food\":");
    const auto catalog_at=duplicate_catalog.find("\"catalog\":",food_at);
    check(food_at!=std::string::npos && catalog_at!=std::string::npos,"Canonical six-kind catalog missing");
    duplicate_catalog.insert(catalog_at,"\"catalog\":"+Json(sim::fishing_food_catalog).dump()+",");
    f::write_text(hostile,duplicate_catalog);
    refuses([&]{(void)save::read_save(hostile);},"Duplicate six-kind Food catalog entered authority");unchanged();
    auto working_world=fixture.restore(*working);
    const auto working_path=fixture.root.path/"working44.json";
    save::write_save(working_path,fixture.document(working_world),fixture.root.data,fixture.mask());
    const auto working_bytes=f::read_text(working_path);auto work_json=Json::parse(working_bytes);
    work_json["world"]["fishing"]["boats"][0]["movement"]=static_cast<unsigned>(sim::FishingMovementResult::Moving);
    f::write_text(hostile,work_json.dump());
    refuses([&]{(void)fixture.restore(save::read_save(hostile));},"Working Fishing actor admitted without actual arrival outcome");
    check(working_world.snapshot()==*working && f::read_text(working_path)==working_bytes,
        "Rejected work-phase mutation changed its positive control");unchanged();
    const auto saved=save::read_save(path);
    refuses([&]{(void)save::restore_playable_agriculture(saved,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Frozen Rule27 factory accepted Fishing generation");
    f::write_text(model_path,model+"\n");refuses([&]{(void)fixture.restore(saved);},"Changed Fishing model bypassed identity");
    refuses([&]{save::write_save(path,saved,fixture.root.data,fixture.mask());},"Changed Fishing model reached publication");
    f::write_text(model_path,model);unchanged();
    f::write_text(map_path,map+"\n");refuses([&]{(void)fixture.restore(saved);},"Changed Fishing map bypassed identity");
    refuses([&]{save::write_save(path,saved,fixture.root.data,fixture.mask());},"Changed Fishing map reached publication");
    f::write_text(map_path,map);unchanged();
    (void)scene;
}
template<class Historical>void frozen_target(const g::Fixture& fixture,const sim::World& world,const fs::path& path,
    const std::string& bytes,Historical& historical,unsigned schema) {
    auto old=historical.world();const auto before=old.snapshot();const auto target=historical.root.path/"frozen.json";
    save::write_save(target,historical.document(old),historical.root.data,historical.mask());const auto old_bytes=f::read_text(target);
    const auto parsed=save::read_save(target);check(parsed.source_schema_version==schema && historical.restore(parsed).snapshot()==before,
        "Historical Food generation changed under Fishing implementation");
    refuses([&]{save::write_save(target,fixture.document(world),fixture.root.data,fixture.mask());},"Fishing overwrote a historical save");
    refuses([&]{save::write_save(path,historical.document(old),historical.root.data,historical.mask());},"Historical Agriculture writer overwrote schema44");
    refuses([&]{(void)fixture.restore(parsed);},"Fishing restore automatically migrated historical Food");
    auto altered=Json::parse(old_bytes);altered["rules"]["playable_fishing"]=sim::playable_fishing_contract;
    altered["rules"]["playable_fishing_revision"]=sim::playable_fishing_revision;
    altered["world"]["fishing_food"]=Json::object();const auto hostile=historical.root.path/"foreign-fishing.json";
    f::write_text(hostile,altered.dump());refuses([&]{(void)save::read_save(hostile);},"Historical schema accepted Fishing marker/authority");
    check(f::read_text(path)==bytes && f::read_text(target)==old_bytes && old.snapshot()==before,
        "Cross-generation refusal changed either target or historical World");
}
void immutable_historical_food_generations() {
    g::Fixture fixture;auto world=fixture.world();const auto path=fixture.root.path/"separate44.json";const auto bytes=roundtrip(fixture,world,path);
    a::Fixture foundation;frozen_target(fixture,world,path,bytes,foundation,40);
    p::Fixture plantations;frozen_target(fixture,world,path,bytes,plantations,41);
    q::Fixture irrigation;frozen_target(fixture,world,path,bytes,irrigation,42);
    m::Fixture playable;frozen_target(fixture,world,path,bytes,playable,43);
}
}
int main() {
    try {
        every_boat_phase_and_both_delivery_legs();returning_fish_and_atomic_write_failure();
        actual_dock_endpoints_and_fire_witnesses();hostile_state_and_source_identity();immutable_historical_food_generations();
        std::cout<<"Fishing schema44 boat motion/work/cargo, six-food deliveries/consumption, Recovery, hostile/source/atomic guards and historical isolation PASS\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Fishing persistence FAIL: "<<error.what()<<'\n';return 1;}
}
