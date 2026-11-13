#include "PlantationsFixture.h"
#include "persistence/RecoveryStore.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>

namespace {
namespace p=oe_plantations_fixture;
namespace q=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;
template<class Action> void refuses(Action action,const char* reason) {
    try {action();} catch (const std::exception&) {return;}
    throw std::runtime_error(reason);
}
std::string roundtrip(const p::Fixture& fixture,const sim::World& world,const fs::path& path) {
    const auto before=world.snapshot();
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto json=Json::parse(bytes);const auto parsed=save::read_save(path);
    check(parsed.source_schema_version==41 && parsed.world.rule_version==25 && parsed.world==before &&
          !parsed.migrated_from_schema1 &&
          json.at("rules").at("plantations")==sim::original_plantations_contract &&
          json.at("world").at("plantations").at("revision")==sim::plantations_revision &&
          json.at("world").at("plantations").at("catalog")==sim::plantations_catalog &&
          json.at("world").at("agriculture").at("revision")==sim::agriculture_revision &&
          json.at("world").at("agriculture").at("catalog")==sim::agriculture_catalog &&
          parsed.world.original_production->development_revision==27,
          "Schema41 changed complete plantation identity or inherited commodity contracts");
    for(const auto& row:json.at("world").at("original_goods").at("stocks"))
        check(row.at("amounts").size()==6,"Schema41 widened the six household goods");
    for(const auto& row:json.at("world").at("original_production").at("industry").at("auxiliary"))
        check(row.at("amounts").size()==5,"Schema41 widened the five auxiliary materials");
    auto restored=fixture.restore(parsed);check(restored.snapshot()==before,"Schema41 restore changed complete paid World");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Schema41 canonical rewrite changed bytes or source World");
    auto expected=world;
    for(int i=0;i<100;++i) {
        expected.tick();restored.tick();
        check(expected.snapshot()==restored.snapshot(),"Schema41 work/cargo/return replay diverged");
    }
    return bytes;
}
void paid_work_recovery_and_hostile_authority() {
    p::Fixture fixture;auto world=fixture.world();
    const auto path=fixture.root.path/"plantations41.json";roundtrip(fixture,world,path);
    p::build(world);
    save::RecoveryStore recovery(fixture.root.path/"plantation-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    std::array<bool,5> observed{};
    std::optional<sim::WorldSnapshot> outward_work;
    std::optional<sim::WorldSnapshot> picked_cargo;
    for(int tick=0;tick<30000 && !std::all_of(observed.begin(),observed.end(),[](bool x){return x;});++tick) {
        world.tick();const auto& state=*world.plantations_state();
        const std::array reached{
            std::any_of(state.plants.begin(),state.plants.end(),[](const auto& row){return row.growth>0 && row.growth<sim::plantation_growth_ticks;}),
            std::any_of(state.sources.begin(),state.sources.end(),[](const auto& row){return row.latest_trip && !row.latest_trip->picked_tick;}),
            std::any_of(state.sources.begin(),state.sources.end(),[](const auto& row){return row.latest_trip && row.latest_trip->picked_tick && !row.latest_trip->returned_tick;}),
            !state.plants.empty() && std::all_of(state.plants.begin(),state.plants.end(),[](const auto& row){return row.returned>0 && row.last_receipt.has_value();}),
            std::any_of(world.original_goods_state()->cargo.begin(),world.original_goods_state()->cargo.end(),[](const auto& row){
                return row.good==openemperor::original::Goods::Tea || row.good==openemperor::original::Goods::Silk ||
                    row.good==openemperor::original::Goods::Lacquerware;}) ||
                std::any_of(world.original_production_state()->industry.aux_trips.begin(),
                    world.original_production_state()->industry.aux_trips.end(),[](const auto& row){
                        return (row.material==sim::OriginalAuxMaterial::Lacquer || row.material==sim::OriginalAuxMaterial::RawSilk) &&
                            row.phase==sim::OriginalGoodsTransferPhase::Loaded && row.loaded>row.delivered+row.returned;})};
        for(std::size_t i=0;i<observed.size();++i) if(reached[i] && !observed[i]) {
            if(i==1) outward_work=world.snapshot();
            if(i==2) picked_cargo=world.snapshot();
            observed[i]=true;const auto bytes=roundtrip(fixture,world,path);
            const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
            check(entry.loadable() && entry.schema==41 && entry.rule_version==25 &&
                  f::read_text(entry.path)==bytes &&
                  fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
                  "Recovery lost physical plantation growth, pickup, return or goods cargo");
        }
    }
    check(std::all_of(observed.begin(),observed.end(),[](bool x){return x;}),
          "Paid plantations did not earn growth, outward work, picked cargo, all returns and ordinary goods transport");
    check(world.plantations_state()->plants.size()==3 && world.plantations_state()->sources.size()==3 &&
          std::all_of(world.plantations_state()->sources.begin(),world.plantations_state()->sources.end(),
              [](const auto& row){return row.latest_trip.has_value();}),
          "Paid three-chain fixture lost its retained worker trip identities");
    const auto retained_receipt=[&] {
        const auto& state=*world.plantations_state();
        for(const auto& plant:state.plants) if(plant.last_receipt)
            for(const auto& source:state.sources) if(source.building_id==plant.source_id && source.latest_trip &&
                source.latest_trip->grower_id>plant.last_receipt->grower_id) return true;
        return false;
    };
    p::until(world,retained_receipt,"Paid work did not retain its previous receipt after a new actual worker spawn");
    const auto bytes=roundtrip(fixture,world,path);const auto canonical=Json::parse(bytes);
    const auto before=world.snapshot();const auto map=f::read_text(fixture.root.data/f::map_relative);
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt";const auto model=f::read_text(model_path);
    const auto unchanged=[&] {
        check(f::read_text(path)==bytes && world.snapshot()==before &&
              f::read_text(fixture.root.data/f::map_relative)==map && f::read_text(model_path)==model,
              "Rejected plantation authority changed save, World identities/funds or authored source bytes");
    };
    const auto hostile_path=fixture.root.path/"hostile41.json";
    const auto reject_json=[&](auto edit) {
        auto hostile=canonical;edit(hostile);f::write_text(hostile_path,hostile.dump());
        refuses([&]{(void)save::read_save(hostile_path);},"Malformed plantation JSON became authority");unchanged();
    };
    reject_json([](auto& j){j["rules"].erase("plantations");});
    reject_json([](auto& j){j["rules"]["plantations"]="unverified-fruit";});
    reject_json([](auto& j){j["world"].erase("plantations");});
    reject_json([](auto& j){j["world"]["plantations"]["revision"]=2;});
    reject_json([](auto& j){j["world"]["plantations"]["catalog"]="fruit29";});
    reject_json([](auto& j){j["world"]["plantations"]["inventory"]=Json::array({1});});
    reject_json([](auto& j){j["world"]["plantations"]["next_grower_id"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["next_grower_id"]=1;});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["growth"]=81;});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["growth"]=-1;});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["source_id"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["source_id"]=j["world"]["plantations"]["plants"][1]["source_id"];});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["productive_ticks"]=UINT64_MAX;});
    reject_json([](auto& j){auto& row=j["world"]["plantations"]["plants"][0];row["harvested"]=row["grown"].template get<std::uint64_t>()+1;});
    reject_json([](auto& j){auto& row=j["world"]["plantations"]["plants"][0];row["returned"]=row["harvested"].template get<std::uint64_t>()+1;});
    reject_json([](auto& j){auto& rows=j["world"]["plantations"]["plants"];rows.push_back(rows.front());});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["last_receipt"]["grower_id"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["plants"][0]["last_receipt"]["returned_tick"]=1;});
    reject_json([](auto& j){
        auto& state=j["world"]["plantations"];
        for(auto& plant:state["plants"]) if(!plant["last_receipt"].is_null())
            for(const auto& source:state["sources"]) if(source["building_id"]==plant["source_id"] &&
                !source["latest_trip"].is_null() && source["latest_trip"]["grower_id"].template get<std::uint64_t>()>
                    plant["last_receipt"]["grower_id"].template get<std::uint64_t>()) {
                auto& receipt=plant["last_receipt"];const auto start=receipt["dispatched_tick"].template get<std::uint64_t>();
                receipt["picked_tick"]=start+1;receipt["returned_tick"]=start+2;return;
            }
        throw std::runtime_error("Retained physical receipt negative control has no actual prior worker");
    });
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["courier_id"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["spawned"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["latest_trip"]["grower_id"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["latest_trip"]["plant_id"]=0;});
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["latest_trip"]["delivered"]=2;});
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["latest_trip"]["dispatched_tick"]=UINT64_MAX;});
    reject_json([](auto& j){j["world"]["plantations"]["sources"][0]["latest_trip"]["picked_tick"]=0;});
    auto duplicate=bytes;const auto key=duplicate.find("\"plantations\":");
    check(key!=std::string::npos,"Canonical plantation marker missing");duplicate.insert(key,"\"plantations\":null,");
    f::write_text(hostile_path,duplicate);
    refuses([&]{(void)save::read_save(hostile_path);},"Duplicate plantation key became authority");unchanged();
    const auto reject_state=[&](auto edit) {
        auto malformed=fixture.document(world);edit(malformed.world);
        refuses([&]{(void)fixture.restore(malformed);},"Malformed plantation state entered full restore");
        refuses([&]{save::write_save(path,malformed,fixture.root.data,fixture.mask());},
                "Malformed plantation state reached atomic publication");unchanged();
    };
    reject_state([](auto& state){state.plantations.reset();});
    reject_state([](auto& state){state.plantations->next_grower_id=1;});
    reject_state([](auto& state){state.plantations->plants.front().growth=81;});
    reject_state([](auto& state){++state.plantations->plants.front().returned;});
    reject_state([](auto& state){state.plantations->sources.front().latest_trip->grower_id=sim::PlantationGrowerId{};});
    reject_state([](auto& state){
        auto& rows=state.plantations->sources;
        const auto maximum=std::max_element(rows.begin(),rows.end(),[](const auto& a,const auto& b){
            return a.latest_trip->grower_id<b.latest_trip->grower_id;});
        auto& victim=rows.front().building_id==maximum->building_id ? rows[1]:rows.front();
        const auto old=victim.latest_trip->grower_id;victim.latest_trip->grower_id=maximum->latest_trip->grower_id;
        for(auto& plant:state.plantations->plants) if(plant.source_id==victim.building_id &&
            plant.last_receipt && plant.last_receipt->grower_id==old)
            plant.last_receipt->grower_id=victim.latest_trip->grower_id;
    });
    check(picked_cargo.has_value(),"Physical picked-cargo snapshot was not retained");
    auto malformed_path=fixture.document(world);malformed_path.world=*picked_cargo;
    const auto picked_source=std::find_if(malformed_path.world.plantations->sources.begin(),
        malformed_path.world.plantations->sources.end(),[](const auto& row){
            return row.latest_trip && row.latest_trip->picked_tick && !row.latest_trip->returned_tick;});
    check(picked_source!=malformed_path.world.plantations->sources.end(),"Retained picked worker disappeared");
    auto worker=std::find_if(malformed_path.world.couriers.begin(),malformed_path.world.couriers.end(),
        [&](const auto& row){return row.id==picked_source->courier_id;});
    check(worker!=malformed_path.world.couriers.end() && worker->path_vertex<worker->path.size(),
          "Retained picked worker has no actual physical path");
    worker->path[worker->path_vertex]={std::numeric_limits<int>::min(),std::numeric_limits<int>::min()};
    refuses([&]{(void)fixture.restore(malformed_path);},"Typed extreme worker position entered full restore");
    refuses([&]{save::write_save(path,malformed_path,fixture.root.data,fixture.mask());},
            "Typed extreme worker position reached atomic publication");unchanged();
    check(outward_work.has_value(),"Physical outward worker snapshot was not retained");
    auto cancelled=fixture.restore(*outward_work);
    const auto outward_source=std::find_if(cancelled.plantations_state()->sources.begin(),
        cancelled.plantations_state()->sources.end(),[](const auto& row){
            return row.latest_trip && !row.latest_trip->picked_tick && !row.latest_trip->returned_tick;});
    check(outward_source!=cancelled.plantations_state()->sources.end(),"Retained outward worker disappeared");
    const auto cancelled_source=outward_source->building_id;const auto cancelled_courier=outward_source->courier_id;
    f::buy(cancelled,sim::set_building_operation(cancelled_source,false));
    p::until(cancelled,[&]{
        const auto& trip=p::source(cancelled,cancelled_source).latest_trip;
        return trip && !trip->picked_tick && !trip->returned_tick &&
            cancelled.courier(cancelled_courier).phase==sim::CourierPhase::Returning;
    },"A paused outward worker did not earn an empty physical return");
    roundtrip(fixture,cancelled,fixture.root.path/"empty-return41.json");
    auto malformed_phase=fixture.document(cancelled);
    for(auto& actor:malformed_phase.world.couriers) if(actor.id==cancelled_courier)
        actor.phase=static_cast<sim::CourierPhase>(3);
    refuses([&]{(void)fixture.restore(malformed_phase);},"Unknown phase entered a real empty worker return");
    refuses([&]{save::write_save(path,malformed_phase,fixture.root.data,fixture.mask());},
            "Unknown empty-return phase reached atomic publication");unchanged();
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},
            "Injected plantation save replacement failure was ignored");unchanged();
    const auto catalog=recovery.catalog();check(catalog.histories.size()==1 && !catalog.truncated &&
        catalog.histories.front().error.empty() && catalog.histories.front().entries.size()==6,
        "Recovery lost its start or five physical plantation checkpoints");
    for(const auto& entry:catalog.histories.front().entries) check(entry.loadable(),"Recovery rejected a genuine plantation checkpoint");
    f::write_text(model_path,model+"\n");
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Changed live model bypassed plantation source binding");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());},
            "Changed model source reached plantation publication");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Source refusal changed plantation save or World");
    f::write_text(model_path,model);unchanged();
}
void frozen_generations_are_never_migrated_or_overwritten() {
    p::Fixture fixture;auto world=fixture.world();p::build(world);
    const auto path=fixture.root.path/"separate41.json";const auto bytes=roundtrip(fixture,world,path);
    auto old=sim::World::create_original_agriculture(fixture.policy,fixture.model,fixture.terrain);q::build(old);
    const auto old_path=fixture.root.path/"frozen40.json";
    const auto old_document=fixture.document(old);save::write_save(old_path,old_document,fixture.root.data,fixture.mask());
    const auto old_bytes=f::read_text(old_path);const auto before=world.snapshot();const auto old_before=old.snapshot();
    refuses([&]{(void)fixture.restore(save::read_save(old_path));},"Plantations automatically migrated Rule24/schema40");
    refuses([&]{(void)save::restore_original_agriculture(save::read_save(path),fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
            "Rule24 restore admitted plantation authority");
    refuses([&]{(void)save::restore_save(save::read_save(path),fixture.root.data,fixture.mask(),fixture.policy);},
            "Ordinary restore silently selected plantations");
    refuses([&]{save::write_save(old_path,fixture.document(world),fixture.root.data,fixture.mask());},
            "Rule25 overwrote a frozen Rule24 save");
    refuses([&]{save::write_save(path,old_document,fixture.root.data,fixture.mask());},
            "Rule24 overwrote a Rule25 save");
    auto disguised=Json::parse(bytes);disguised["schema_version"]=40;disguised["rules"]["version"]=24;
    const auto hostile=fixture.root.path/"relabelled40.json";f::write_text(hostile,disguised.dump());
    refuses([&]{(void)save::read_save(hostile);},"Schema40 admitted relabelled plantation rows or worker roles");
    const auto legacy_terrain=save::prepare_original_terrain_facts(fixture.root.data,f::map_relative);
    auto historical=sim::World::create_original_deadline_food_dispatch(fixture.policy,fixture.model,legacy_terrain);
    const auto farm=q::purchase(historical,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{23,25}};field.building_id=farm;f::buy(historical,field);
    const auto historical_path=fixture.root.path/"historical34.json";
    save::write_save(historical_path,fixture.document(historical),fixture.root.data,fixture.mask());
    const auto historical_bytes=f::read_text(historical_path);
    refuses([&]{save::write_save(historical_path,fixture.document(world),fixture.root.data,fixture.mask());},
            "Rule25 overwrote historical Wheat/Millet authority");
    check(f::read_text(path)==bytes && f::read_text(old_path)==old_bytes &&
          f::read_text(historical_path)==historical_bytes && world.snapshot()==before && old.snapshot()==old_before &&
          save::read_save(historical_path).source_schema_version==34 && save::read_save(old_path).source_schema_version==40,
          "Cross-generation refusal changed complete World or existing historical bytes");
    save::RecoveryStore recovery(fixture.root.path/"separate-history",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    refuses([&]{(void)recovery.write_checkpoint(history,old_document,fixture.mask());},
            "Recovery mixed frozen Agriculture and plantation generations");
    check(recovery.catalog().histories.front().entries.size()==1,"Refused migration altered recovery history");
}
} // namespace
int main() {
    try {
        paid_work_recovery_and_hostile_authority();frozen_generations_are_never_migrated_or_overwritten();
        std::cout<<"Plantation schema41 physical lifecycle/replay/recovery/identity tests passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
