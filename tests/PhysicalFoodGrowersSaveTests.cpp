#include "PhysicalFoodGrowersFixture.h"
#include "persistence/RecoveryStore.h"

#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace q=oe_physical_food_growers_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using f::check;
using Phase=sim::PhysicalFoodGrowerPhase;
template<class F>void refuses(F operation,const char* why){try{operation();}catch(const std::exception&){return;}throw std::runtime_error(why);}

std::string roundtrip(const q::Fixture& fixture,const sim::World& world,const fs::path& path,int continuation=31) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto encoded=Json::parse(bytes);
    check(document.source_schema_version==47 && document.world.rule_version==31 && document.world==before && !document.migrated_from_schema1 &&
        document.world.physical_food_growers && document.world.multi_field_agriculture && document.world.hunting_food && document.world.hunting &&
        document.world.fishing && document.world.plantations && document.world.irrigation && !document.world.agriculture && !document.world.fishing_food &&
        document.world.original_production->food==sim::OriginalFoodState{} &&
        encoded.at("rules").at("physical_food_growers")==sim::physical_food_growers_contract &&
        encoded.at("rules").at("physical_food_growers_revision")==sim::physical_food_growers_revision &&
        encoded.at("world").at("physical_food_growers").at("revision")==sim::physical_food_growers_revision,
        "Schema47 lost its exact physical Grower lineage or sole seven-kind inventory");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Schema47 restore granted work, settled cargo or changed exact state");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Canonical physical Grower rewrite changed bytes or live World");
    auto expected=world;for(int tick=0;tick<continuation;++tick) {
        expected.tick();restored.tick();check(expected.snapshot()==restored.snapshot(),"Physical Grower independent restored continuation diverged");
        if(tick%257==0){q::valid(expected);q::valid(restored);}
    }
    q::valid(expected);q::valid(restored);return bytes;
}

void walking_wait_work_return_carts_recovery_and_long_replay() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);const auto path=fixture.root.path/"growers47.json";
    roundtrip(fixture,world,path);
    save::RecoveryStore recovery(fixture.root.path/"growers-recovery",fixture.root.data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());std::size_t checkpoints=0;
    std::array<bool,4> phases{};bool source_cart=false,house_cart=false;const auto channel=q::farm(world,scene.farm).courier_id;
    const auto all=[](const auto& flags){return std::all_of(flags.begin(),flags.end(),[](bool value){return value;});};
    bool done=false;
    for(int tick=0;tick<30000 && !done;++tick) {
        world.tick();
        if(q::active(world,scene.farm)) {
            const auto worker=q::worker(world,scene.farm);const auto actor=world.courier(channel);const auto index=static_cast<std::size_t>(worker.phase)-1;
            const bool meaningful=worker.phase==Phase::Outward ? actor.edge_progress>0:
                worker.phase==Phase::Waiting ? worker.wait_left>0:
                worker.phase==Phase::Working ? worker.work_ticks>3:
                actor.edge_progress>0 && world.physical_food_grower_cargo(channel)>0;
            if(meaningful && !phases.at(index)) {
                const auto check_field_position=[&](const sim::World& observed) {
                    if(worker.phase!=Phase::Waiting && worker.phase!=Phase::Working)return;
                    const auto cell=observed.building(worker.field_id).cell;const auto& actual=observed.courier(channel);
                    check(actual.path.size()==1 && actual.path.front()==cell && !actual.path_vertex && !actual.edge_progress,
                        "Stationary Grower position control did not retain its single actual Field cell");
                    const auto position=observed.courier_position(channel);
                    check(position && position->x==cell.x && position->y==cell.y,
                        "Waiting/working Grower position did not equal its actual assigned Field before/after restore");
                };
                check_field_position(world);
                const auto inventory=q::h::stock(world,scene.farm);roundtrip(fixture,world,path);
                const auto restored=fixture.restore(save::read_save(path));
                check_field_position(restored);
                check(q::worker(restored,scene.farm)==worker && q::h::stock(restored,scene.farm)==inventory &&
                    restored.physical_food_grower_cargo(channel)==world.physical_food_grower_cargo(channel),
                    "Grower reload changed phase/wait/work or turned unreturned cargo into Farm Food");
                const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
                check(entry.loadable() && entry.schema==47 && entry.rule_version==31 && fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
                    "Recovery lost actual Grower movement, assignment, work or loaded return");phases[index]=true;
            }
        }
        std::vector<sim::CourierId> ids;for(const auto& actor:world.couriers())ids.push_back(actor.id);
        for(const auto id:ids) {
            const auto actor=world.courier(id);
            if(actor.phase!=sim::CourierPhase::ToWarehouse || !actor.edge_progress || !world.original_food_cargo(id))continue;
            bool* observed=actor.role==sim::CourierRole::OriginalFoodToMarket ? &source_cart:
                actor.role==sim::CourierRole::OriginalFoodToHouse ? &house_cart:nullptr;
            if(!observed || *observed)continue;
            const auto cargo=q::trip(world,id);const auto target=q::h::stock(world,cargo.target);roundtrip(fixture,world,path);
            const auto restored=fixture.restore(save::read_save(path));
            check(q::trip(restored,id)==cargo && q::h::stock(restored,cargo.target)==target,"Reload delivered crop cargo before actual Cart arrival");*observed=true;
        }
        const auto eaten=q::consumed(world);done=all(phases) && source_cart && house_cart && eaten[0] && eaten[1] && eaten[4];
    }
    check(done,"Physical Grower Save/Recovery control lacks an actual movement/work/return/Cart/House phase");
    q::until(world,[&]{return q::phase(world,scene.farm,Phase::Working) && q::worker(world,scene.farm).work_ticks>5;},"Partial work save control never reached an actual field");
    const auto field=q::worker(world,scene.farm).field_id;f::buy(world,sim::set_building_operation(field,false));q::ticks(world,2);
    roundtrip(fixture,world,path);const auto paused=world.snapshot();
    const auto paused_entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());++checkpoints;
    check(fixture.restore(save::read_save(paused_entry.path)).snapshot()==paused,"Paused/cancelling Grower Recovery changed work, cursor or physical return");
    f::buy(world,sim::set_building_operation(field,true));roundtrip(fixture,world,path,12000);
    const auto catalog=recovery.catalog();check(!catalog.truncated && catalog.histories.size()==1 && catalog.histories[0].error.empty() &&
        catalog.histories[0].entries.size()==std::min(checkpoints,save::recovery_checkpoint_limit)+1,"Grower Recovery exceeded bounded start/checkpoint retention");
}

void hostile_physical_lifetimes_routes_and_receipts() {
    q::Fixture fixture;auto world=fixture.world();const std::array<std::size_t,1> crops{4};const auto scene=q::build(world,crops);
    f::buy(world,sim::set_building_operation(scene.market,false));const auto channel=q::farm(world,scene.farm).courier_id;
    q::until(world,[&]{return q::phase(world,scene.farm,Phase::Returning) && world.physical_food_grower_cargo(channel)==1 && world.courier(channel).edge_progress>0;},
        "Hostile Grower input control never earned actual loaded return cargo");
    const auto path=fixture.root.path/"hostile47.json";const auto bytes=roundtrip(fixture,world,path);const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why) {
        auto value=original;mutate(value);const auto encoded=value.dump();f::write_text(path,encoded);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before && f::read_text(path)==encoded,"Rejected Grower input changed supplied bytes or live World");
    };
    const auto transfer=[](auto& j)->auto&{return j["world"]["physical_food_growers"]["farms"][0]["latest_trip"];};
    reject([](auto& j){j["rules"]["physical_food_growers"]="unknown";},"Unknown physical Grower marker accepted");
    reject([](auto& j){j["rules"]["physical_food_growers_revision"]=2;},"Unknown physical Grower contract revision accepted");
    reject([](auto& j){j["world"].erase("physical_food_growers");},"Missing physical Grower authority accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["revision"]=2;},"Unknown physical Grower state revision accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["inventory"]=1;},"Duplicate mutable physical Food inventory accepted");
    reject([](auto& j){auto& rows=j["world"]["physical_food_growers"]["farms"];rows.push_back(rows[0]);},"Duplicate physical Farm/channel row accepted");
    reject([](auto& j){auto& rows=j["world"]["physical_food_growers"]["fields"];rows.push_back(rows[0]);},"Duplicate physical Field receipt row accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["fields"].clear();},"Missing paid Field receipt history accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["next_grower_id"]=1;},"Reused physical Grower lifetime highwater accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["courier_id"]=99999;},"Foreign physical Grower channel accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["spawned"]=99999;},"Invented physical Grower spawns accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["retired"]=1;},"Live loaded Grower already counted retired");
    reject([&](auto& j){transfer(j)["grower_id"]=0;},"Zero physical Grower identity accepted");
    reject([&](auto& j){transfer(j)["field_id"]=99999;},"Foreign assigned Field accepted");
    reject([&](auto& j){transfer(j)["crop"]=0;},"Loaded BeanCurd worker reassigned to Wheat");
    reject([&](auto& j){transfer(j)["phase"]=99;},"Unknown Grower phase accepted");
    reject([&](auto& j){transfer(j)["wait_left"]=6;},"Grower wait exceeded its finite5 ticks");
    reject([&](auto& j){transfer(j)["arrived_tick"]=nullptr;},"Physical harvest without actual field arrival accepted");
    reject([&](auto& j){transfer(j)["harvested_tick"]=nullptr;},"Loaded return without actual earned harvest accepted");
    reject([&](auto& j){transfer(j)["returned_tick"]=j["world"]["ticks"];},"Moving loaded worker forged an actual Farm receipt");
    reject([&](auto& j){transfer(j)["delivered"]=1;},"Unreturned physical crop credited as delivered");
    reject([&](auto& j){transfer(j)["work_ticks"]=79;},"Harvest without all80 real work units accepted");
    reject([&](auto& j){transfer(j)["work_before"]=80;},"Physical trip borrowed historical field work");
    reject([&](auto& j){transfer(j)["dispatched_tick"]=j["world"]["ticks"];},"Instant dispatch/arrival/work/loaded return accepted");
    reject([&](auto& j){transfer(j)["harvested_tick"]=transfer(j)["arrived_tick"];},"Instant field work/harvest chronology accepted");
    reject([&](auto& j){const auto cell=world.building(scene.fields[0]).cell;for(auto& actor:j["world"]["couriers"])if(actor["id"]==static_cast<std::uint32_t>(channel))actor["path"].back()=Json::array({cell.x,cell.y});},
        "Loaded return route ended at Field instead of actual owning Farm");
    reject([&](auto& j){for(auto& actor:j["world"]["couriers"])if(actor["id"]==static_cast<std::uint32_t>(channel))actor["cargo"]=1;},
        "Physical crop duplicated into legacy courier cargo");
    reject([&](auto& j){
        j["world"]["physical_food_growers"]["fields"][0]["returned"]=1;
        for(auto& stock:j["world"]["hunting_food"]["stocks"])if(stock["building_id"]==static_cast<std::uint32_t>(scene.farm))stock["amounts"][4]=1;
    },"Coherent Farm Food credit duplicated a still-carried Grower harvest");
    f::write_text(path,bytes);auto duplicate=bytes;const std::string key=std::string("\"physical_food_growers\": \"")+sim::physical_food_growers_contract+"\"";
    const auto at=duplicate.find(key);check(at!=std::string::npos,"Duplicate Grower marker control lost canonical key");duplicate.replace(at,key.size(),key+",\n    "+key);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate physical Grower contract key accepted");f::write_text(path,bytes);
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Injected schema47 atomic write failure ignored");
    check(world.snapshot()==before && f::read_text(path)==bytes,"Failed Grower publication changed existing target or World");
}

void retained_lifetime_order_and_hostile_cart_arithmetic() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    q::until(world,[&]{return q::phase(world,scene.farm,Phase::Outward) && world.courier(q::farm(world,scene.farm).courier_id).edge_progress>0;},
        "Lifetime-count control lacks its first real outward Grower");
    const auto count_path=fixture.root.path/"retired-count47.json";const auto count_bytes=roundtrip(fixture,world,count_path);
    const auto count_before=world.snapshot();auto impossible=fixture.document(world);auto& count_state=*impossible.world.physical_food_growers;
    auto& count_source=count_state.farms[0];
    check(count_source.spawned==1 && !count_source.retired && count_source.latest_trip,"Lifetime-count control was not the first actual trip");
    const auto age=world.ticks()-world.building(scene.farm).placed_tick;
    count_source.retired=age/10+1;count_source.spawned=count_source.retired+1;
    check(count_source.spawned<=age,"Lifetime-count attack did not preserve the older loose age bound");
    count_source.latest_trip->grower_id={count_source.spawned};count_state.next_grower_id=count_source.spawned+1;
    auto count_json=Json::parse(count_bytes);auto& counts=count_json["world"]["physical_food_growers"];
    counts["farms"][0]["spawned"]=count_source.spawned;counts["farms"][0]["retired"]=count_source.retired;
    counts["farms"][0]["latest_trip"]["grower_id"]=count_source.spawned;counts["next_grower_id"]=count_state.next_grower_id;
    const auto hostile_counts=count_json.dump();f::write_text(count_path,hostile_counts);
    refuses([&]{(void)fixture.restore(save::read_save(count_path));},"Coherent Grower counters invented too many completed physical roundtrips");
    check(f::read_text(count_path)==hostile_counts && world.snapshot()==count_before,"Rejected lifetime counts changed input bytes or World");
    f::write_text(count_path,count_bytes);
    refuses([&]{save::write_save(count_path,impossible,fixture.root.data,fixture.mask());},"Writer published impossible Grower retirement chronology");
    check(f::read_text(count_path)==count_bytes && world.snapshot()==count_before,"Rejected lifetime-count publication changed target or World");
    q::until(world,[&]{
        const auto& source=q::farm(world,scene.farm);if(!source.latest_trip)return false;
        std::size_t old_receipts=0;
        for(const auto& field:world.physical_food_growers_state()->fields)
            if(field.last_receipt && field.last_receipt->grower_id!=source.latest_trip->grower_id)++old_receipts;
        return old_receipts>=2 && std::all_of(scene.fields.begin(),scene.fields.end(),[&](const auto id){return q::receipt_field(world,id).returned>0;}) &&
            std::any_of(world.hunting_food_state()->trips.begin(),world.hunting_food_state()->trips.end(),[&](const auto& trip){return trip.source==scene.farm;});
    },"Lifetime-order control lacks three real crop returns, two older receipts or an actual Farm Cart");
    const auto path=fixture.root.path/"retained-lifetimes47.json";const auto bytes=roundtrip(fixture,world,path);
    const auto before=world.snapshot();auto document=fixture.document(world);auto& state=*document.world.physical_food_growers;
    std::vector<std::size_t> old;
    for(std::size_t i=0;i<state.fields.size();++i)
        if(state.fields[i].last_receipt && state.fields[i].last_receipt->grower_id!=state.farms[0].latest_trip->grower_id)old.push_back(i);
    check(old.size()>=2,"Lifetime-order control lost independent old receipts");
    auto& first=*state.fields[old[0]].last_receipt;auto& second=*state.fields[old[1]].last_receipt;
    check(first.dispatched_tick!=second.dispatched_tick && first.grower_id!=second.grower_id,"Old receipt swap would not invert actual allocation chronology");
    std::swap(first.grower_id,second.grower_id);
    auto encoded=Json::parse(bytes);auto& rows=encoded["world"]["physical_food_growers"]["fields"];
    std::swap(rows[old[0]]["last_receipt"]["grower_id"],rows[old[1]]["last_receipt"]["grower_id"]);
    const auto hostile=encoded.dump();f::write_text(path,hostile);
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Retained Grower IDs contradicted actual monotone dispatch chronology");
    check(f::read_text(path)==hostile && world.snapshot()==before,"Rejected receipt-ID inversion changed input bytes or paid World");
    f::write_text(path,bytes);
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask());},"Writer published impossible retained Grower allocation order");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Rejected receipt-ID publication changed its existing target or World");

    document=fixture.document(world);auto& trips=document.world.hunting_food->trips;
    const auto cart=std::find_if(trips.begin(),trips.end(),[&](const auto& trip){return trip.source==scene.farm;});
    check(cart!=trips.end(),"Hostile arithmetic control lost an actual source Cart");
    const auto index=static_cast<std::size_t>(cart-trips.begin());
    cart->loaded[0]=0;cart->delivered[0]=INT32_MAX;cart->returned[0]=INT32_MAX;
    encoded=Json::parse(bytes);auto& json_cart=encoded["world"]["hunting_food"]["trips"][index];
    json_cart["loaded"][0]=0;json_cart["delivered"][0]=INT32_MAX;json_cart["returned"][0]=INT32_MAX;
    const auto hostile_cart=encoded.dump();f::write_text(path,hostile_cart);
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Overflowing hostile Cart return reservation was admitted");
    check(f::read_text(path)==hostile_cart && world.snapshot()==before,"Rejected hostile Cart arithmetic changed input bytes or paid World");
    f::write_text(path,bytes);
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask());},"Writer published overflowing hostile Cart counters");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Rejected hostile Cart publication changed target or World");
}

void aggregate_physical_farm_time_bound() {
    q::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{1};const auto scene=q::build(world,crops);
    f::buy(world,sim::set_building_operation(scene.market,false));f::buy(world,sim::set_building_operation(scene.farm,false));
    q::ticks(world,280);f::buy(world,sim::set_building_operation(scene.farm,true));
    q::until(world,[&]{return q::receipt_field(world,scene.fields[0]).returned==1;},"Aggregate-time control lacks one earned late physical Farm receipt");
    f::buy(world,sim::set_building_operation(scene.farm,false));
    std::uint64_t age=world.ticks()-world.building(scene.farm).placed_tick,cycles=age/94+1;
    while(cycles*80>age){q::ticks(world,1);age=world.ticks()-world.building(scene.farm).placed_tick;cycles=age/94+1;}
    check(cycles>1 && cycles<=8 && cycles*80<=age && cycles*94>age && q::farm(world,scene.farm).spawned==1,
        "Aggregate-time attack does not preserve individual work/age/capacity controls");
    const auto path=fixture.root.path/"aggregate-farm-time47.json";const auto bytes=roundtrip(fixture,world,path);const auto before=world.snapshot();
    auto document=fixture.document(world);auto& physical=*document.world.physical_food_growers;auto& source=physical.farms[0];
    source.spawned=cycles;source.retired=cycles;source.latest_trip->grower_id={cycles};source.latest_trip->work_before=(cycles-1)*80;
    physical.next_grower_id=cycles+1;physical.fields[0].returned=cycles;physical.fields[0].last_receipt->grower_id={cycles};
    physical.fields[0].last_receipt->work_at_harvest=cycles*80;document.world.multi_field_agriculture->fields[0].productive_ticks=cycles*80;
    document.world.hunting_food->fields[0].harvested=cycles;
    const auto stock=std::find_if(document.world.hunting_food->stocks.begin(),document.world.hunting_food->stocks.end(),[&](const auto& row){return row.building_id==scene.farm;});
    check(stock!=document.world.hunting_food->stocks.end() && stock->amounts[1]==1,"Aggregate-time control lost its sole earned Millet portion");
    stock->amounts[1]=static_cast<std::int32_t>(cycles);
    auto encoded=Json::parse(bytes);auto& data=encoded["world"]["physical_food_growers"];auto& farm=data["farms"][0];
    farm["spawned"]=cycles;farm["retired"]=cycles;farm["latest_trip"]["grower_id"]=cycles;farm["latest_trip"]["work_before"]=(cycles-1)*80;
    data["next_grower_id"]=cycles+1;data["fields"][0]["returned"]=cycles;data["fields"][0]["last_receipt"]["grower_id"]=cycles;
    data["fields"][0]["last_receipt"]["work_at_harvest"]=cycles*80;encoded["world"]["multi_field_agriculture"]["fields"][0]["productive_ticks"]=cycles*80;
    encoded["world"]["hunting_food"]["fields"][0]["harvested"]=cycles;
    for(auto& row:encoded["world"]["hunting_food"]["stocks"])if(row["building_id"]==static_cast<std::uint32_t>(scene.farm))row["amounts"][1]=cycles;
    const auto hostile=encoded.dump();f::write_text(path,hostile);
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Coherent crop histories borrowed movement/wait time to invent completed physical harvests");
    check(f::read_text(path)==hostile && world.snapshot()==before,"Rejected aggregate Farm timeline changed input bytes or World");
    f::write_text(path,bytes);
    refuses([&]{save::write_save(path,document,fixture.root.data,fixture.mask());},"Writer published impossible aggregate physical Farm work/return time");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Rejected aggregate Farm publication changed target or World");
}

void exact_historical_generation_and_old_writer_guards() {
    q::Fixture current;auto world=current.world();const auto path=current.root.path/"current47.json";const auto bytes=roundtrip(current,world,path);
    q::n::Fixture historical;auto old=historical.world();const auto old_path=historical.root.path/"historical46.json";
    save::write_save(old_path,historical.document(old),historical.root.data,historical.mask());const auto old_bytes=f::read_text(old_path);const auto old_document=save::read_save(old_path);
    check(old_document.source_schema_version==46 && old_document.world.rule_version==30 && !old_document.world.physical_food_growers &&
        historical.restore(old_document).snapshot()==old.snapshot(),"Historical abstract Rule30 changed identity or acquired physical workers");
    refuses([&]{save::write_save(old_path,current.document(world),current.root.data,current.mask());},"Physical Grower writer overwrote schema46");
    refuses([&]{save::write_save(path,historical.document(old),historical.root.data,historical.mask());},"Abstract multi-field writer overwrote schema47");
    refuses([&]{(void)save::restore_physical_food_growers(old_document,historical.root.data,historical.mask(),historical.policy,historical.model,historical.terrain);},
        "Physical Grower factory automatically migrated schema46");
    auto injected=old_document;injected.world.physical_food_growers.emplace();refuses([&]{(void)historical.restore(injected);},"Rule30 accepted physical worker state");
    sim::World classic(current.policy,sim::RulesProfile::CityV16,6);const auto classic_document=current.document(classic);
    refuses([&]{save::write_save(path,classic_document,current.root.data,current.mask());},"Generic historical writer overwrote schema47");
    auto damaged=Json::parse(bytes);damaged["rules"].erase("physical_food_growers");const auto encoded=damaged.dump();f::write_text(path,encoded);
    refuses([&]{save::write_save(path,classic_document,current.root.data,current.mask());},"Generic historical writer overwrote marker-damaged schema47");
    check(f::read_text(path)==encoded && f::read_text(old_path)==old_bytes,"Cross-generation refusal changed retained source bytes");
    f::write_text(path,bytes);const auto map=current.root.data/f::map_relative;const auto source=f::read_text(map);auto changed=source;changed.back()=static_cast<char>(changed.back()^1);f::write_text(map,changed);
    refuses([&]{(void)current.restore(save::read_save(path));},"Physical Grower restore ignored changed source map identity");
    check(f::read_text(map)==changed && f::read_text(path)==bytes,"Source-bound Grower refusal modified supplied fixture bytes");
}
}
int main() {
    try{walking_wait_work_return_carts_recovery_and_long_replay();hostile_physical_lifetimes_routes_and_receipts();retained_lifetime_order_and_hostile_cart_arithmetic();aggregate_physical_farm_time_bound();exact_historical_generation_and_old_writer_guards();
        std::cout<<"Physical Food Growers Save/Recovery tests passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
