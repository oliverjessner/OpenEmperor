#include "PlayableAgricultureFixture.h"
#include "persistence/RecoveryStore.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>

namespace {
namespace m=oe_playable_agriculture_fixture;
namespace q=oe_irrigation_fixture;
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;

template<class Operation>void refuses(Operation operation,const char* message) {
    try {operation();} catch(const std::exception&) {return;}
    throw std::runtime_error(message);
}
template<class Predicate>void until(sim::World& world,Predicate done,const char* message,int limit=30000) {
    for(int tick=0;tick<limit && !done();++tick) world.tick();
    check(done(),message);
}
const sim::AgricultureTrip* trip_for(const sim::World& world,sim::CourierId id) {
    const auto& trips=world.original_agriculture_state()->trips;
    const auto row=std::find_if(trips.begin(),trips.end(),[&](const auto& trip){return trip.transfer.courier_id==id;});
    return row==trips.end() ? nullptr:&*row;
}
sim::AgricultureCounters consumed(const sim::World& world) {
    sim::AgricultureCounters out{};
    for(const auto& stock:world.original_agriculture_state()->stocks)
        for(std::size_t crop=0;crop<out.size();++crop) out[crop]+=stock.consumed[crop];
    return out;
}
bool all_positive(const sim::AgricultureCounters& values) {
    return std::all_of(values.begin(),values.end(),[](auto value){return value>0;});
}

std::string roundtrip(const m::Fixture& fixture,const sim::World& world,const fs::path& path,int replay_ticks=97) {
    const auto before=world.snapshot();
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto json=Json::parse(bytes);
    const auto parsed=save::read_save(path);
    check(parsed.source_schema_version==43 && parsed.world.rule_version==sim::kPlayableAgricultureRuleVersion &&
          parsed.world==before && !parsed.migrated_from_schema1 &&
          parsed.map_permissions_policy_version==3 &&
          json.at("rules").at("playable_agriculture")==sim::playable_agriculture_contract &&
          json.at("rules").at("playable_agriculture_revision")==sim::playable_agriculture_revision &&
          json.at("rules").at("irrigation")==sim::irrigation_contract &&
          parsed.world.agriculture && parsed.world.plantations && parsed.world.irrigation,
          "Schema43 lost its explicit AUTHORED identity, inherited authority or complete World");
    check(parsed.world.original_production->food==sim::OriginalFoodState{},
          "Schema43 duplicated five-kind Food into the historical two-crop ledger");
    auto restored=fixture.restore(parsed);
    check(restored.snapshot()==before,"Schema43 restore changed paid progress, typed cargo or goods");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,
          "Canonical schema43 rewrite changed bytes or source World");
    auto expected=world;
    for(int tick=0;tick<replay_ticks;++tick) {
        expected.tick();restored.tick();
        check(expected.snapshot()==restored.snapshot(),
              "Schema43 continuation duplicated harvest, changed typed physical delivery or diverged");
    }
    m::conserved(expected);m::conserved(restored);
    return bytes;
}

void all_five_midtransport_growth_consumption_and_recovery() {
    m::Fixture fixture(true);auto world=fixture.world();const auto scene=m::build(world,true);
    const auto path=fixture.root.path/"playable43.json";
    roundtrip(fixture,world,path);
    until(world,[&]{return std::any_of(scene.farms.begin(),scene.farms.end(),[&](auto id) {
        return world.building(id).progress>0;
    });},"Paid staff never reached savable productive field progress");
    roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"playable-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    std::array<bool,sim::agriculture_crop_count> captured{};
    std::size_t checkpoints=0;
    for(int tick=0;tick<30000 && !std::all_of(captured.begin(),captured.end(),[](bool value){return value;});++tick) {
        world.tick();
        for(const auto& actor:world.couriers()) {
            if(actor.role!=sim::CourierRole::OriginalFoodToMarket || actor.phase!=sim::CourierPhase::ToWarehouse ||
               actor.edge_progress<=0 || world.original_food_cargo(actor.id)<=0) continue;
            const auto* trip=trip_for(world,actor.id);
            check(trip && trip->phase==sim::OriginalGoodsTransferPhase::Loaded,
                  "Actual moving Food courier lost its single typed cargo transaction");
            for(std::size_t crop=0;crop<captured.size();++crop) if(!captured[crop] && trip->loaded[crop]>0) {
                roundtrip(fixture,world,path);
                const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
                check(entry.loadable() && entry.schema==43 && entry.rule_version==27 &&
                      fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
                      "Recovery failed an actual moving typed crop load");
                captured[crop]=true;++checkpoints;
            }
        }
    }
    check(std::all_of(captured.begin(),captured.end(),[](bool value){return value;}),
          "All five independently produced crops did not reach actual savable Farm-to-Market cargo");
    std::array<bool,sim::agriculture_crop_count> household_captured{};
    for(int tick=0;tick<30000 &&
        !std::all_of(household_captured.begin(),household_captured.end(),[](bool value){return value;});++tick) {
        world.tick();
        for(const auto& actor:world.couriers()) {
            if(actor.role!=sim::CourierRole::OriginalFoodToHouse || actor.phase!=sim::CourierPhase::ToWarehouse ||
               actor.edge_progress<=0 || world.original_food_cargo(actor.id)<=0) continue;
            const auto* trip=trip_for(world,actor.id);
            check(trip && trip->phase==sim::OriginalGoodsTransferPhase::Loaded && trip->source==scene.market &&
                  sim::original_house_kind(world.building(trip->target).kind),
                  "Actual moving household Food lost its paid Market-to-House transaction");
            for(std::size_t crop=0;crop<household_captured.size();++crop)
                if(!household_captured[crop] && trip->loaded[crop]>0) {
                    const auto target=trip->target;
                    const auto before_stock=m::stock(world,target);
                    const auto before_quality=m::quality(world,target);
                    const auto before_cargo=world.original_food_cargo(actor.id);
                    const auto loaded=trip->loaded;
                    roundtrip(fixture,world,path);
                    auto restored=fixture.restore(save::read_save(path));
                    const auto* restored_trip=trip_for(restored,actor.id);
                    check(restored_trip && restored_trip->phase==sim::OriginalGoodsTransferPhase::Loaded &&
                          restored_trip->target==target && restored_trip->loaded==loaded &&
                          restored.original_food_cargo(actor.id)==before_cargo &&
                          m::stock(restored,target)==before_stock && m::quality(restored,target)==before_quality &&
                          m::stock(world,target)==before_stock && m::quality(world,target)==before_quality,
                          "Schema43 restore credited House Food or FoodQuality before physical arrival");
                    m::conserved(world);m::conserved(restored);
                    household_captured[crop]=true;
                }
        }
    }
    check(std::all_of(household_captured.begin(),household_captured.end(),[](bool value){return value;}),
          "All five crops did not reach actual savable Market-to-House cargo");
    until(world,[&]{return all_positive(consumed(world));},
          "All five typed crops did not reach real household consumption");
    const auto totals=consumed(world);
    roundtrip(fixture,world,path,816);
    const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
    check(entry.loadable() && entry.schema==43 && all_positive(totals),
          "Recovery failed the actual post-consumption World");
    const auto catalog=recovery.catalog();
    const auto retained=std::min(checkpoints,save::recovery_checkpoint_limit);
    check(!catalog.truncated && catalog.histories.size()==1 && catalog.histories.front().error.empty() &&
          catalog.histories.front().entries.size()==retained+1,
          "Schema43 Recovery lost its exact start point or checkpoint sequence");
    std::vector<std::uint64_t> sequences;
    std::size_t starts=0;
    for(const auto& checkpoint:catalog.histories.front().entries)
    {
        check(checkpoint.loadable() && checkpoint.schema==43 && checkpoint.rule_version==27,
              "Recovery catalog did not preserve the exact playable generation");
        if(checkpoint.start_point) ++starts;
        else sequences.push_back(checkpoint.sequence);
    }
    std::sort(sequences.begin(),sequences.end());
    check(starts==1,"Recovery rotation replaced its immutable start point");
    for(std::size_t index=0;index<sequences.size();++index)
        check(sequences[index]==checkpoints-retained+1+index,
              "Recovery did not retain its bounded latest consecutive checkpoint sequence");
}

void loaded_return_and_atomic_rejection() {
    m::Fixture fixture;auto world=fixture.world();const auto scene=m::build(world);
    sim::CourierId actor{};
    until(world,[&] {
        for(const auto& courier:world.couriers()) {
            const auto* trip=trip_for(world,courier.id);
            if(courier.owner==scene.farms[2] && courier.phase==sim::CourierPhase::ToWarehouse &&
               courier.edge_progress>0 && trip && trip->loaded[2]>0) {actor=courier.id;return true;}
        }
        return false;
    },"Rice never reached an actual loaded outward movement");
    f::buy(world,sim::set_building_operation(scene.farms[2],false));
    until(world,[&]{const auto* trip=trip_for(world,actor);return trip &&
        trip->phase==sim::OriginalGoodsTransferPhase::Returning;},
        "Paused paid Farm did not retain and physically return Rice cargo");
    const auto path=fixture.root.path/"return43.json";
    const auto bytes=roundtrip(fixture,world,path,600);
    const auto before=world.snapshot();
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},
            "Injected schema43 publication failure was ignored");
    check(world.snapshot()==before && f::read_text(path)==bytes,
          "Rejected schema43 publication changed returning cargo, state or target bytes");
}

void exact_markers_hostile_ledgers_and_sources() {
    m::Fixture fixture;auto world=fixture.world();m::build(world);
    until(world,[&]{return all_positive(consumed(world));},"Hostile-save control lacked real five-kind consumption");
    const auto path=fixture.root.path/"canonical43.json";
    const auto bytes=roundtrip(fixture,world,path);const auto canonical=Json::parse(bytes);
    const auto before=world.snapshot();
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt";
    const auto map_path=fixture.root.data/f::map_relative;
    const auto model=f::read_text(model_path),map=f::read_text(map_path);
    const auto hostile=fixture.root.path/"hostile43.json";
    const auto unchanged=[&] {
        check(world.snapshot()==before && f::read_text(path)==bytes &&
              f::read_text(model_path)==model && f::read_text(map_path)==map,
              "Rejected schema43 authority changed World, source inputs or target bytes");
    };
    const auto reject=[&](auto change) {
        auto altered=canonical;change(altered);f::write_text(hostile,altered.dump());
        refuses([&]{(void)fixture.restore(save::read_save(hostile));},
                "Manipulated playable save became paid authority");unchanged();
    };
    reject([](auto& j){j["rules"].erase("playable_agriculture");});
    reject([](auto& j){j["rules"]["playable_agriculture"]="original-runtime-verified";});
    reject([](auto& j){j["rules"].erase("playable_agriculture_revision");});
    reject([](auto& j){j["rules"]["playable_agriculture_revision"]=2;});
    reject([](auto& j){j["schema_version"]=42;});
    reject([](auto& j){j["rules"]["version"]=26;});
    reject([](auto& j){j["rules"].erase("irrigation");});
    reject([](auto& j){j["rules"].erase("plantations");});
    reject([](auto& j){j["rules"].erase("agriculture");});
    reject([](auto& j){j["world"]["playable_agriculture"]=Json::object();});
    reject([](auto& j){j["world"]["agriculture"]["stocks"][0]["amounts"][2]=-1;});
    reject([](auto& j){j["world"]["agriculture"]["stocks"][0]["amounts"][2]=INT32_MAX;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["crop"]=4;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["harvested"]=UINT64_MAX;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["quantity"]=1;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["farm_id"]=0;});
    reject([](auto& j){j["world"]["agriculture"]["trips"][0]["source"]=0;});
    auto duplicate=bytes;const auto at=duplicate.find("\"playable_agriculture\":");
    check(at!=std::string::npos,"Schema43 marker missing from canonical bytes");
    duplicate.insert(at,"\"playable_agriculture\":\"authored-playable-agriculture-v1\",");
    f::write_text(hostile,duplicate);
    refuses([&]{(void)save::read_save(hostile);},"Duplicate schema43 marker entered authority");unchanged();
    const auto document=save::read_save(path);
    refuses([&]{(void)save::restore_original_irrigation(document,fixture.root.data,fixture.mask(),
        fixture.policy,fixture.model,fixture.terrain);},"Historical Rule26 factory accepted Rule27");unchanged();
    f::write_text(model_path,model+"\n");
    refuses([&]{(void)fixture.restore(document);},"Changed model bytes bypassed playable source identity");
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask());},"Changed model source reached publication");
    f::write_text(model_path,model);unchanged();
    f::write_text(map_path,map+"\n");
    refuses([&]{(void)fixture.restore(document);},"Changed map bytes bypassed playable source identity");
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask());},"Changed map source reached publication");
    f::write_text(map_path,map);unchanged();
}

template<class Fixture>void frozen_target(const m::Fixture& playable,const sim::World& world,
        const fs::path& playable_path,const std::string& playable_bytes,Fixture& historical,unsigned schema) {
    auto old=historical.world();const auto before=old.snapshot();
    const auto path=historical.root.path/"frozen.json";
    save::write_save(path,historical.document(old),historical.root.data,historical.mask());
    const auto bytes=f::read_text(path);
    const auto parsed=save::read_save(path);
    check(parsed.source_schema_version==schema && historical.restore(parsed).snapshot()==before,
          "Frozen Agriculture generation changed during exact restore");
    save::write_save(path,historical.document(historical.restore(parsed)),historical.root.data,historical.mask());
    check(f::read_text(path)==bytes,"Frozen Agriculture canonical bytes changed");
    refuses([&]{save::write_save(path,playable.document(world),playable.root.data,playable.mask());},
            "Schema43 overwrote a historical save");
    refuses([&]{save::write_save(playable_path,historical.document(old),historical.root.data,historical.mask());},
            "Historical Agriculture writer overwrote schema43");
    refuses([&]{(void)playable.restore(parsed);},"Playable restore silently migrated a historical save");
    auto altered=Json::parse(bytes);altered["rules"]["playable_agriculture"]=sim::playable_agriculture_contract;
    altered["rules"]["playable_agriculture_revision"]=sim::playable_agriculture_revision;
    const auto hostile=historical.root.path/"foreign-marker.json";f::write_text(hostile,altered.dump());
    refuses([&]{(void)save::read_save(hostile);},"Historical schema admitted the new playable marker");
    check(f::read_text(path)==bytes && f::read_text(playable_path)==playable_bytes && old.snapshot()==before,
          "Cross-generation rejection altered either save or World");
}
void immutable_historical_generations() {
    m::Fixture fixture;auto world=fixture.world();const auto path=fixture.root.path/"separate43.json";
    const auto bytes=roundtrip(fixture,world,path);
    a::Fixture foundation;frozen_target(fixture,world,path,bytes,foundation,40);
    p::Fixture plantations;frozen_target(fixture,world,path,bytes,plantations,41);
    q::Fixture irrigation;frozen_target(fixture,world,path,bytes,irrigation,42);
}
} // namespace

int main() {
    try {
        all_five_midtransport_growth_consumption_and_recovery();
        loaded_return_and_atomic_rejection();
        exact_markers_hostile_ledgers_and_sources();
        immutable_historical_generations();
        std::cout<<"Playable Agriculture schema43 typed transport, replay, Recovery and historical-boundary tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Playable Agriculture persistence failed: "<<error.what()<<'\n';
        return 1;
    }
}
