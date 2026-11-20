#include "MultiFieldAgricultureFixture.h"
#include "persistence/RecoveryStore.h"

#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace n=oe_multi_field_fixture;
namespace h=oe_hunting_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;
template<class F>void refuses(F operation,const char* message) {
    try{operation();}catch(const std::exception&){return;}throw std::runtime_error(message);
}
std::string roundtrip(const n::Fixture& fixture,const sim::World& world,const fs::path& path,int continuation=31) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto json=Json::parse(bytes);
    check(document.source_schema_version==46 && document.world.rule_version==30 && document.world==before && !document.migrated_from_schema1 &&
        document.world.multi_field_agriculture && document.world.hunting_food && document.world.hunting && document.world.fishing &&
        document.world.irrigation && document.world.plantations && !document.world.agriculture && !document.world.fishing_food &&
        document.world.original_production->food==sim::OriginalFoodState{} &&
        json.at("rules").at("multi_field_agriculture")==sim::multi_field_agriculture_contract &&
        json.at("rules").at("multi_field_agriculture_revision")==sim::multi_field_agriculture_revision &&
        json.at("world").at("multi_field_agriculture").at("revision")==sim::multi_field_agriculture_revision &&
        json.at("world").at("hunting_food").at("catalog")==sim::hunting_food_catalog &&
        !json.at("world").contains("agriculture") && !json.at("world").contains("fishing_food"),
        "Schema46 changed generation, duplicated Food authority or lost inherited lineage");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Exact multi-field restore granted or lost paid authority");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Canonical multi-field rewrite changed bytes or live World");
    auto expected=world;
    for(int tick=0;tick<continuation;++tick) {
        expected.tick();restored.tick();
        check(expected.snapshot()==restored.snapshot(),"Multi-field independent restored continuation diverged");
        if(tick%257==0){n::valid(expected);n::valid(restored);}
    }
    n::valid(expected);n::valid(restored);return bytes;
}
void active_work_cargo_recovery_and_long_replay() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world);
    const auto path=fixture.root.path/"multi-field46.json";roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"multi-field-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());std::size_t checkpoints=0;
    n::until(world,[&]{return n::work(world,scene.fields.front()).productive_ticks>13;},"Paid Farm never earned intermediate sibling work");
    const auto late=n::crop(world,scene.farm,3,{24,25});
    check(!n::work(world,late).productive_ticks && n::work(world,scene.fields.front()).productive_ticks>0,
        "New late Field inherited previous sibling labor");
    roundtrip(fixture,world,path);
    f::buy(world,sim::set_building_operation(scene.fields[1],false));n::ticks(world,37);roundtrip(fixture,world,path);
    const auto paused=world.snapshot();const auto paused_entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
    check(paused_entry.loadable() && paused_entry.schema==46 && paused_entry.rule_version==30 &&
        fixture.restore(save::read_save(paused_entry.path)).snapshot()==paused,
        "Recovery changed a paused sibling's operation, retained work, progress or shared cursor");
    f::buy(world,sim::set_building_operation(scene.fields[1],true));
    constexpr std::array<std::size_t,4> crops{0,1,4,3};std::array<bool,4> farm{},house{};
    const auto all=[](const auto& flags){return std::all_of(flags.begin(),flags.end(),[](bool value){return value;});};
    bool done=false;
    for(int tick=0;tick<30000 && !done;++tick) {
        world.tick();
        // Snapshot IDs/transaction values before persistence calls. No borrowed
        // World range survives a transactional command or restore boundary.
        std::vector<sim::CourierId> ids;for(const auto& actor:world.couriers())ids.push_back(actor.id);
        for(const auto id:ids) {
            const auto actor=world.courier(id);
            if(actor.phase!=sim::CourierPhase::ToWarehouse || !actor.edge_progress ||
                (actor.role!=sim::CourierRole::OriginalFoodToMarket && actor.role!=sim::CourierRole::OriginalFoodToHouse))continue;
            const auto load=n::trip(world,id);if(load.phase!=sim::OriginalGoodsTransferPhase::Loaded)continue;
            auto& flags=actor.role==sim::CourierRole::OriginalFoodToMarket ? farm:house;
            bool needed=false;for(std::size_t i=0;i<crops.size();++i)needed|=!flags[i] && load.loaded[crops[i]]>0;
            if(!needed)continue;
            const auto inventory=h::stock(world,load.target);const auto fields=*world.multi_field_agriculture_state();roundtrip(fixture,world,path);
            const auto restored=fixture.restore(save::read_save(path));
            check(n::trip(restored,id)==load && h::stock(restored,load.target)==inventory && *restored.multi_field_agriculture_state()==fields,
                "Mid-cart reload altered independent work, crop identity, cargo or target stock before arrival");
            const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
            check(entry.loadable() && entry.schema==46 && entry.rule_version==30 &&
                fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery changed multi-field work/cargo generation");
            for(std::size_t i=0;i<crops.size();++i)if(load.loaded[crops[i]]>0)flags[i]=true;
        }
        done=all(farm) && all(house);const auto eaten=n::consumed(world);for(const auto crop:crops)done&=eaten[crop]>0;
    }
    check(done && n::field(world,late).harvested>0,"Four independently phased crop histories did not reach actual carts and consumption");
    roundtrip(fixture,world,path,12000);
    for(int i=0;i<3;++i){n::ticks(world,19);recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;}
    const auto catalog=recovery.catalog();
    check(!catalog.truncated && catalog.histories.size()==1 && catalog.histories.front().error.empty() &&
        catalog.histories.front().entries.size()==std::min(checkpoints,save::recovery_checkpoint_limit)+1,
        "Multi-field Recovery violated bounded start/checkpoint retention");
    std::size_t starts=0;for(const auto& entry:catalog.histories.front().entries) {
        check(entry.loadable() && entry.schema==46 && entry.rule_version==30,"Recovery relabelled schema46 as a historical generation");starts+=entry.start_point;
    }
    check(starts==1,"Recovery replaced the immutable multi-field start");
}

void hostile_references_histories_and_atomic_publication() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world);
    n::until(world,[&]{return n::work(world,scene.fields.front()).productive_ticks>17;},"Hostile work control never earned real labor");
    const auto other_farm=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{50,25});
    const auto other_field=n::crop(world,other_farm,0,{53,25});
    n::until(world,[&]{return n::work(world,other_field).productive_ticks>0;},"Foreign-parent cursor control lacks real positive work");
    const auto late=n::crop(world,scene.farm,3,{24,25});
    const auto path=fixture.root.path/"hostile46.json";const auto bytes=roundtrip(fixture,world,path);
    const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* message) {
        auto value=original;mutate(value);const auto encoded=value.dump();f::write_text(path,encoded);
        refuses([&]{const auto document=save::read_save(path);(void)fixture.restore(document);},message);
        check(world.snapshot()==before && f::read_text(path)==encoded,"Rejected schema46 input changed World or supplied bytes");
    };
    reject([](auto& j){j["rules"]["multi_field_agriculture"]="unknown";},"Wrong multi-field marker accepted");
    reject([](auto& j){j["rules"].erase("multi_field_agriculture");},"Missing multi-field marker accepted");
    reject([](auto& j){j["rules"]["multi_field_agriculture_revision"]=2;},"Unknown multi-field rule revision accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["revision"]=2;},"Unknown multi-field work revision accepted");
    reject([](auto& j){j["world"].erase("multi_field_agriculture");},"Missing schema46 work authority accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["raw_food"]=1;},"Unknown duplicate inventory authority accepted");
    reject([](auto& j){j["world"]["hunting_food"]["catalog"]=sim::fishing_food_catalog;},"Six-kind catalog accepted by schema46");
    reject([](auto& j){j["world"]["hunting_food"]["stocks"][0]["amounts"].erase(6);},"Truncated seven-kind account accepted");
    reject([](auto& j){j["world"]["hunting_food"]["stocks"][0]["amounts"][0]=-1;},"Negative multi-field Food accepted");
    reject([](auto& j){j["world"]["fishing_food"]=j["world"]["hunting_food"];},"Duplicate six-kind mutable ledger accepted");
    reject([](auto& j){auto& a=j["world"]["multi_field_agriculture"]["fields"];a.push_back(a[0]);},"Duplicate field work identity accepted");
    reject([](auto& j){auto& a=j["world"]["multi_field_agriculture"]["farms"];a.push_back(a[0]);},"Duplicate Farm cursor identity accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"].erase(0);},"Missing paid Field work history accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["farms"].erase(0);},"Missing paid Farm work schedule accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["field_id"]=0;},"Zero Field identity accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["farm_id"]=99999;},"Foreign Field parent accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["field_id"]=99999;},"Foreign Field history accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["farms"][0]["last_field"]=99999;},"Foreign work cursor accepted");
    reject([&](auto& j){j["world"]["multi_field_agriculture"]["farms"][0]["last_field"]=static_cast<std::uint32_t>(other_field);},"Another Farm's real worked Field accepted as cursor");
    reject([&](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["farm_id"]=static_cast<std::uint32_t>(other_farm);},"Work history reassigned away from sole crop parent");
    reject([&](auto& j){j["world"]["multi_field_agriculture"]["farms"][0]["last_field"]=static_cast<std::uint32_t>(late);},"Zero-work field accepted as last productive cursor");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["farms"][0]["last_field"]=nullptr;},"Worked Farm accepted an absent productive cursor");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["productive_ticks"]=-1;},"Negative field work accepted");
    reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["productive_ticks"]=UINT64_MAX;},"Unbounded/future field work accepted");
    reject([](auto& j){auto& value=j["world"]["hunting_food"]["fields"][0]["harvested"];value=value.template get<std::uint64_t>()+1;},"Crop harvest without80 real work units accepted");
    reject([&](auto& j){for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<std::uint32_t>(scene.farm))b["progress"]=1;},"Legacy Farm production clock accepted in schema46");
    f::write_text(path,bytes);
    auto duplicate=bytes;const std::string key=std::string("\"multi_field_agriculture\": \"")+sim::multi_field_agriculture_contract+"\"";
    const auto at=duplicate.find(key);check(at!=std::string::npos,"Duplicate multi-field marker control lost canonical source");
    duplicate.replace(at,key.size(),key+",\n    "+key);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate multi-field marker accepted");
    check(f::read_text(path)==duplicate && world.snapshot()==before,"Duplicate-key refusal mutated input or live World");
    f::write_text(path,bytes);
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},
        "Injected schema46 atomic publication failure ignored");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Failed schema46 atomic save changed existing bytes or World");
}

void coherent_late_field_labor_overflow() {
    n::Fixture fixture;auto world=fixture.world();
    const auto farm=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});n::ticks(world,200);
    const auto first=n::crop(world,farm,0,{23,25});const auto second=n::crop(world,farm,1,{23,26});n::ticks(world,60);
    const auto path=fixture.root.path/"late-fields46.json";const auto bytes=roundtrip(fixture,world,path);const auto before=world.snapshot();
    auto forged=Json::parse(bytes);auto& state=forged["world"]["multi_field_agriculture"];
    state["fields"][0]["productive_ticks"]=40;state["fields"][1]["productive_ticks"]=40;
    state["farms"][0]["last_field"]=static_cast<std::uint32_t>(second);
    // Both individual ages60 and the total Farm age260 admit these counters.
    // No harvest reaches80, so every sole Food source/stock equation stays0.
    // Only the shared late-placement suffix quota detects 80 work in60 ticks.
    const auto encoded=forged.dump();f::write_text(path,encoded);
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Coherent late Field histories manufactured shared Farm labor");
    check(world.snapshot()==before && f::read_text(path)==encoded && !n::field(world,first).harvested && !n::field(world,second).harvested,
        "Late-field quota refusal changed authority or input bytes");
    auto snapshot=before;snapshot.multi_field_agriculture->fields[0].productive_ticks=40;snapshot.multi_field_agriculture->fields[1].productive_ticks=40;
    snapshot.multi_field_agriculture->farms[0].last_field=second;
    refuses([&]{(void)fixture.restore(snapshot);},"Direct exact restore bypassed shared late-placement labor bound");
    check(world.snapshot()==before,"Rejected direct late-field restore changed its source World");
}

void historical_generation_and_source_boundaries() {
    n::Fixture current;auto world=current.world();const auto path=current.root.path/"current46.json";
    const auto bytes=roundtrip(current,world,path);h::Fixture old_fixture(false);auto old=old_fixture.world();
    const auto old_path=old_fixture.root.path/"historical45.json";save::write_save(old_path,old_fixture.document(old),old_fixture.root.data,old_fixture.mask());
    const auto old_bytes=f::read_text(old_path);const auto historical=save::read_save(old_path);
    check(historical.source_schema_version==45 && historical.world.rule_version==29 && !historical.world.multi_field_agriculture &&
        old_fixture.restore(historical).snapshot()==old.snapshot(),"Historical schema45 was reinterpreted or changed on restore");
    refuses([&]{save::write_save(old_path,current.document(world),current.root.data,current.mask());},"Schema46 writer overwrote schema45");
    refuses([&]{save::write_save(path,old_fixture.document(old),old_fixture.root.data,old_fixture.mask());},"Schema45 writer overwrote schema46");
    refuses([&]{(void)save::restore_multi_field_agriculture(historical,old_fixture.root.data,old_fixture.mask(),old_fixture.policy,old_fixture.model,old_fixture.terrain);},
        "Multi-field factory automatically migrated schema45");
    auto wrong=historical;wrong.world.multi_field_agriculture.emplace();refuses([&]{(void)old_fixture.restore(wrong);},"Historical Rule29 accepted new schedule authority");
    check(f::read_text(path)==bytes && f::read_text(old_path)==old_bytes,"Cross-generation refusal changed either canonical target");
    sim::World classic(current.policy,sim::RulesProfile::CityV16,6);
    const auto classic_document=current.document(classic);const auto classic_path=current.root.path/"classic22.json";
    save::write_save(classic_path,classic_document,current.root.data,current.mask());
    check(save::read_save(classic_path).source_schema_version==22,"Generic classic-writer control was not a valid schema22");
    const auto model=save::prepare_original_production_rules_model(current.root.data);
    const auto terrain=save::prepare_original_terrain_facts(current.root.data,f::map_relative);
    const auto original=sim::World::create_original_deadline_food_dispatch(current.policy,model,terrain);
    const auto original_document=current.document(original);const auto original_path=current.root.path/"original34.json";
    save::write_save(original_path,original_document,current.root.data,current.mask());
    check(save::read_save(original_path).source_schema_version==34,"Generic Original-writer control was not a valid schema34");
    const auto nested_prefix=[](Json value) {
        Json nested=0;for(int depth=0;depth<33;++depth)nested=Json::array({std::move(nested)});
        value["a_deep_prefix"]=std::move(nested);const auto encoded=value.dump();
        check(encoded.find("\"a_deep_prefix\"")<encoded.find("\"schema_version\""),
            "Damaged schema46 guard control must encounter deep content before its top-level schema key");
        return encoded;
    };
    auto oversized_target=Json::parse(bytes);
    oversized_target["a_large_prefix"]=std::string(8U*1024U*1024U+1U,'x');
    const auto oversized=oversized_target.dump();
    check(oversized.find("\"schema_version\"")>8U*1024U*1024U,
        "Oversized schema46 guard control must place its top-level schema key beyond the bounded prefix");
    for(const auto* document:{&classic_document,&original_document}) {
        refuses([&]{save::write_save(path,*document,current.root.data,current.mask());},"Generic historical writer overwrote valid schema46");
        check(f::read_text(path)==bytes,"Generic writer changed existing canonical schema46 bytes");
        auto suspected=Json::parse(bytes);suspected["rules"].erase("multi_field_agriculture");
        const auto missing=suspected.dump();f::write_text(path,missing);
        refuses([&]{save::write_save(path,*document,current.root.data,current.mask());},"Generic historical writer overwrote schema46 with removed marker");
        check(f::read_text(path)==missing,"Generic writer replaced a schema46 target with a missing marker");
        const auto deep_missing=nested_prefix(suspected);f::write_text(path,deep_missing);
        refuses([&]{save::write_save(path,*document,current.root.data,current.mask());},"Deep prefix hid marker-damaged schema46 from historical writer guard");
        check(f::read_text(path)==deep_missing,"Historical writer replaced deeply prefixed marker-damaged schema46 bytes");
        suspected=Json::parse(bytes);suspected["world"]["multi_field_agriculture"]["farms"].push_back({{"farm_id",99999},{"last_field",99999}});
        const auto bad_cursor=suspected.dump();f::write_text(path,bad_cursor);
        refuses([&]{save::write_save(path,*document,current.root.data,current.mask());},"Generic historical writer overwrote schema46 with invalid cursor");
        check(f::read_text(path)==bad_cursor,"Generic writer replaced malformed schema46 work authority");
        const auto deep_cursor=nested_prefix(suspected);f::write_text(path,deep_cursor);
        refuses([&]{save::write_save(path,*document,current.root.data,current.mask());},"Deep prefix hid cursor-damaged schema46 from historical writer guard");
        check(f::read_text(path)==deep_cursor,"Historical writer replaced deeply prefixed cursor-damaged schema46 bytes");
        f::write_text(path,oversized);
        refuses([&]{save::write_save(path,*document,current.root.data,current.mask());},"Oversized prefix hid schema46 from historical writer guard");
        check(f::read_text(path)==oversized,"Historical writer replaced oversized schema46 target bytes");f::write_text(path,bytes);
    }
    const auto current_source=current.root.data/f::map_relative;const auto current_source_bytes=f::read_text(current_source);
    auto changed_source=current_source_bytes;changed_source.back()=static_cast<char>(changed_source.back()^1);f::write_text(current_source,changed_source);
    refuses([&]{(void)current.restore(save::read_save(path));},"Schema46 restore ignored changed source-map identity");
    check(f::read_text(current_source)==changed_source && f::read_text(path)==bytes,"Schema46 source refusal rewrote supplied fixture bytes");
    const auto source=old_fixture.root.data/f::map_relative;const auto source_bytes=f::read_text(source);
    auto altered=source_bytes;altered.back()=static_cast<char>(altered.back()^1);f::write_text(source,altered);
    refuses([&]{(void)old_fixture.restore(historical);},"Exact historical restore ignored changed source-map identity");
    check(f::read_text(source)==altered && f::read_text(old_path)==old_bytes,"Source mismatch refusal rewrote supplied fixture bytes");
}
}
int main() {
    try{active_work_cargo_recovery_and_long_replay();hostile_references_histories_and_atomic_publication();
        coherent_late_field_labor_overflow();historical_generation_and_source_boundaries();
        std::cout<<"Multi-field Agriculture Save/Recovery tests passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
