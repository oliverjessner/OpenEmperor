#include "IrrigationProductivityFixture.h"
#include "persistence/RecoveryStore.h"

#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace t=oe_irrigation_productivity_fixture;
namespace s=oe_seasonal_agriculture_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace p=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using Json=nlohmann::json;
using f::check;
using Phase=sim::PhysicalFoodGrowerPhase;
template<class F>void refuses(F action,const char* why){try{action();}catch(const std::exception&){return;}throw std::runtime_error(why);}
Json& json_row(Json& rows,const char* key,sim::BuildingId identity) {
    for(auto& row:rows)if(row.at(key)==static_cast<std::uint32_t>(identity))return row;
    throw std::runtime_error(std::string("Missing paid JSON row: ")+key);
}

std::string roundtrip(const t::Fixture& fixture,const sim::World& world,const std::filesystem::path& path,int continuation=3) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto json=Json::parse(bytes);
    check(document.source_schema_version==49 && document.world.rule_version==33 && document.world==before &&
        document.world.irrigation_productivity && document.world.seasonal_agriculture && document.world.physical_food_growers &&
        json.at("rules").at("irrigation_productivity")==sim::irrigation_productivity_contract &&
        json.at("rules").at("irrigation_productivity_revision")==1 && json.at("world").at("irrigation_productivity").at("revision")==1,
        "Schema49 lost exact irrigation productivity identity or actual base/bonus/carry state");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Productivity restore granted bonus, changed carry or settled cargo");
    for(const auto& row:world.irrigation_productivity_state()->work)
        check(restored.irrigation_field_status(world.building(row.building_id).cell).water_available==
            world.irrigation_field_status(world.building(row.building_id).cell).water_available,"Restore failed to rebuild exact current derived water eligibility");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Canonical productivity rewrite changed bytes or live authority");
    auto expected=world;for(int tick=0;tick<continuation;++tick){expected.tick();restored.tick();
        check(expected.snapshot()==restored.snapshot(),"Independent wet/dry carry or physical return continuation diverged");
        if(tick%251==0){t::valid(expected);t::valid(restored);}}
    t::valid(expected);t::valid(restored);return bytes;
}

void wet_dry_reconnect_ready_cargo_recovery_and_twelve_thousand_ticks() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto water=t::network(world);const auto field=city.fields[0];const auto path=fixture.root.path/"productivity49.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=[&]{roundtrip(fixture,world,path);const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable() && entry.schema==49 && entry.rule_version==33 && fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
            "Recovery lost genuine wet work, rational carry or physical cargo");};
    checkpoint();t::until(world,[&]{return t::work(world,field).wet_work_ticks==3;},"Save control lacks a real3/4 bonus carry");checkpoint();
    f::buy(world,sim::set_building_operation(water.pump,false));checkpoint();
    const auto carry=t::work(world,field).wet_work_ticks%4;t::ticks(world,7);
    check(t::work(world,field).wet_work_ticks%4==carry,"Dry transition consumed a saved wet remainder");
    f::buy(world,sim::set_building_operation(water.pump,true));checkpoint();
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Save control never reached actual resumed water");checkpoint();
    const sim::Cell cut{85,100};f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});checkpoint();
    check(!world.irrigation_field_status(world.building(field).cell).water_available,"Cut save control remained physically supplied");
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});checkpoint();
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Reconnected save never regained a real supply phase");checkpoint();
    t::until(world,[&]{return s::field(world,field).ready_tick.has_value();},"Save control never earned an irrigated ready crop");checkpoint();
    t::until(world,[&]{return world.physical_food_grower_cargo(q::farm(world,city.farm).courier_id)==1;},"Save control lacks real bonus-assisted picked cargo");
    const auto trip=q::worker(world,city.farm);const auto stock=q::h::stock(world,city.farm);checkpoint();
    auto loaded=fixture.restore(save::read_save(path));check(q::worker(loaded,city.farm)==trip && q::h::stock(loaded,city.farm)==stock &&
        world.physical_food_grower_cargo(q::farm(world,city.farm).courier_id)==1,"Loaded restore credited source Food before physical return");
    f::buy(world,sim::set_building_operation(city.farm,false));t::until(world,[&]{return q::receipt_field(world,field).returned>0;},"Paused bonus-assisted cargo failed its actual Farm receipt");checkpoint();
    check(q::receipt_field(world,field).last_receipt->irrigation_bonus_at_harvest>0,"Actual irrigated receipt lost earned bonus provenance");
    f::buy(world,sim::set_building_operation(city.farm,true));roundtrip(fixture,world,path,12000);
    check(recovery.catalog().histories.size()==1 && !recovery.catalog().truncated,"Productivity Recovery exceeded bounded histories");
}

void hostile_bonus_carry_identity_and_epoch_inputs_are_atomic() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{4};const auto city=q::build(world,crops);
    const auto water=t::network(world);const auto field=city.fields[0];
    t::until(world,[&]{return world.physical_food_grower_cargo(q::farm(world,city.farm).courier_id)==1;},"Hostile productivity control lacks a genuine loaded BeanCurd return");
    const auto path=fixture.root.path/"hostile49.json";const auto bytes=roundtrip(fixture,world,path);const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why){auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before && f::read_text(path)==hostile,"Rejected bonus/carry input changed supplied bytes or live paid authority");};
    reject([](auto& j){j["rules"].erase("irrigation_productivity");},"Missing productivity contract accepted");
    reject([](auto& j){j["rules"]["irrigation_productivity"]="other";},"Unknown productivity contract accepted");
    reject([](auto& j){j["rules"]["irrigation_productivity_revision"]=2;},"Unknown productivity rule revision accepted");
    reject([](auto& j){j["world"].erase("irrigation_productivity");},"Missing productivity state accepted");
    reject([](auto& j){j["world"]["irrigation_productivity"]["revision"]=2;},"Unknown productivity state revision accepted");
    reject([](auto& j){j["world"]["irrigation_productivity"]["wet_mask"]=1;},"Derived water mask became saved authority");
    reject([](auto& j){auto& rows=j["world"]["irrigation_productivity"]["work"];rows.push_back(rows[0]);},"Duplicate productivity identity accepted");
    reject([](auto& j){j["world"]["irrigation_productivity"]["work"].clear();},"Missing paid productive field accepted");
    reject([&](auto& j){j["world"]["irrigation_productivity"]["work"][0]["building_id"]=static_cast<std::uint32_t>(water.pump);},"Pump body became a productive field");
    reject([](auto& j){j["world"]["irrigation_productivity"]["work"][0]["wet_work_ticks"]=UINT64_MAX;},"Wet work exceeded actual base work");
    reject([](auto& j){j["world"]["irrigation_productivity"]["work"][0]["bonus_work"]=UINT64_MAX;},"Bonus caused earned-work overflow");
    reject([](auto& j){j["world"]["irrigation_productivity"]["work"][0]["clipped_bonus"]=UINT64_MAX;},"Clipped bonus caused quarter-work overflow");
    reject([](auto& j){auto& amount=j["world"]["irrigation_productivity"]["work"][0]["bonus_work"];amount=amount.template get<std::uint64_t>()+1;},"A coherent real cargo source accepted an extra free bonus unit");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["latest_trip"]["irrigation_bonus_before"]=UINT64_MAX;},"Trip borrowed future bonus work");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["latest_trip"].erase("irrigation_bonus_before");},"Schema49 trip lost required bonus provenance");
    reject([](auto& j){j["schema_version"]=50;j["rules"]["version"]=34;},"Unknown successor generation admitted");
    auto duplicate=bytes;const std::string marker="\"irrigation_productivity_revision\": 1";
    const auto at=duplicate.find(marker);check(at!=std::string::npos,"Productivity duplicate-key control lost canonical marker");
    duplicate.replace(at,marker.size(),marker+",\n    "+marker);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate schema49 productivity marker accepted");
    f::write_text(path,bytes);auto bad=fixture.document(world);bad.world.irrigation_productivity->work[0].bonus_work=UINT64_MAX;
    refuses([&]{save::write_save(path,bad,fixture.root.data,fixture.mask());},"Writer published overflowing bonus work");
    check(world.snapshot()==before && f::read_text(path)==bytes,"Rejected productivity writer replaced an existing target");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Injected productivity atomic-write failure ignored");
    check(f::read_text(path)==bytes,"Failed productivity publication replaced its existing target");
    (void)field;
}

void genuine_wet_hemp_and_plant_cargo_roundtrip_without_arrival_credit() {
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    const auto path=fixture.root.path/"wet-material-cargo49.json";
    t::until(world,[&]{return world.seasonal_hemp_grower_cargo(s::hemp(world,city.hemp_farm).courier_id)==1 &&
        t::work(world,city.hemp_field).bonus_work>0;},
        "Save control never earned a real bonus-assisted physical Hemp load");
    check(t::work(world,city.hemp_field).bonus_work>0,"Saved Hemp cargo lacks actual wet-earned growth at tick="+std::to_string(world.ticks())+
        " base="+std::to_string(t::base_work(world,city.hemp_field))+" wet="+std::to_string(t::work(world,city.hemp_field).wet_work_ticks)+
        " supplied="+std::to_string(world.irrigation_pump_status(city.water.pump).supplying)+
        " pumpWorkers="+std::to_string(world.workers_assigned(city.water.pump))+
        " pumpFireUntil="+std::to_string(world.building(city.water.pump).fire_until_tick));
    const auto hemp=s::hemp_origin(world,city.hemp_farm).harvested;roundtrip(fixture,world,path);
    auto restored=fixture.restore(save::read_save(path));
    check(s::hemp_origin(restored,city.hemp_farm).harvested==hemp &&
        restored.seasonal_hemp_grower_cargo(s::hemp(restored,city.hemp_farm).courier_id)==1,
        "Hemp cargo restore granted a false Goods source receipt");
    t::until(world,[&]{const auto& row=p::source(world,city.materials.tea_source);return row.latest_trip &&
        row.latest_trip->picked_tick && !row.latest_trip->returned_tick;},"Save control never earned a real bonus-assisted Tea pickup");
    const auto tea=p::plant(world,city.materials.tea_plant).returned;
    check(t::work(world,city.materials.tea_plant).bonus_work>0,"Saved Tea cargo lacks actual wet-earned growth");
    roundtrip(fixture,world,path);restored=fixture.restore(save::read_save(path));
    check(p::plant(restored,city.materials.tea_plant).returned==tea &&
        p::source(restored,city.materials.tea_source).latest_trip==p::source(world,city.materials.tea_source).latest_trip,
        "Plantation cargo restore credited a false return or changed its genuine picked epoch");
}

void wet_history_requires_a_real_pump_paid_ditch_and_possible_supply_phase() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    t::until(world,[&]{return q::receipt_field(world,city.fields[0]).returned==1;},
        "Wet-history negative control lacks its genuine dry80-work harvest and source receipt");
    f::buy(world,sim::set_building_operation(city.farm,false));
    check(t::base_work(world,city.fields[0])==80 && !t::work(world,city.fields[0]).wet_work_ticks,
        "Wet-history control borrowed water or advanced past its actual dry first cycle");
    const auto path=fixture.root.path/"impossible-wet-history49.json";
    const auto reject=[&](const char* why) {
        const auto bytes=roundtrip(fixture,world,path);auto json=Json::parse(bytes);const auto before=world.snapshot();
        // A clipped quarter grants no growth, so every inherited actual work,
        // harvest, cargo and receipt equation remains exactly unchanged.
        auto& row=json["world"]["irrigation_productivity"]["work"][0];row["wet_work_ticks"]=4;row["clipped_bonus"]=1;
        const auto hostile=json.dump();f::write_text(path,hostile);refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before && f::read_text(path)==hostile,"Impossible wet-history refusal changed source bytes or actual dry authority");
        f::write_text(path,bytes);
    };
    reject("Wet work accepted although no paid Pump ever existed");
    const auto pump=oe_irrigation_fixture::pump(world,t::pump_origin);
    reject("Wet work accepted although no ditch was ever paid");
    const std::array<sim::Cell,1> seed{{{85,94}}};check(world.execute_irrigation_batch(seed).accepted,"Late paid supply control lacks its actual ditch");
    check(!world.irrigation_pump_status(pump).supplying,"Late construction control already crossed a real supply phase");
    reject("Wet work predating the earliest possible actual paid Pump refill was accepted");

    auto early_pump=fixture.world();const auto dry=q::build(early_pump,crops);
    oe_irrigation_fixture::pump(early_pump,t::pump_origin);
    t::until(early_pump,[&]{return q::receipt_field(early_pump,dry.fields[0]).returned==1;},
        "Early-Pump/late-first-ditch control lacks a genuine dry source receipt");
    f::buy(early_pump,sim::set_building_operation(dry.farm,false));
    check(early_pump.execute_irrigation_batch(seed).accepted && !early_pump.irrigation_state()->ditches_removed_total,
        "Known first paid ditch control lost its exact no-removal history");
    const auto late_ditch_path=fixture.root.path/"late-first-ditch49.json";
    const auto late_ditch_bytes=roundtrip(fixture,early_pump,late_ditch_path);auto late_ditch_json=Json::parse(late_ditch_bytes);
    auto& late_work=late_ditch_json["world"]["irrigation_productivity"]["work"][0];late_work["wet_work_ticks"]=4;late_work["clipped_bonus"]=1;
    const auto hostile=late_ditch_json.dump();f::write_text(late_ditch_path,hostile);
    refuses([&]{(void)fixture.restore(save::read_save(late_ditch_path));},
        "Early Pump borrowed wet work from before its known first paid ditch and possible refill");
    check(f::read_text(late_ditch_path)==hostile,"Known-first-ditch refusal changed hostile supplied bytes");
}

void still_current_material_epochs_freeze_even_cancelled_work_witnesses() {
    for(const bool hemp:{false,true}) {
        t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
        for(const auto farm:city.farms)f::buy(world,sim::set_building_operation(farm,false));
        const auto source=hemp?city.hemp_farm:city.materials.tea_source;
        for(const auto id:{city.hemp_farm,city.materials.tea_source,city.materials.lacquer_source,city.materials.mulberry_source})
            f::buy(world,sim::set_building_operation(id,false));
        const auto field=hemp?city.hemp_field:city.materials.tea_plant;
        t::through(world,(hemp?3U:2U)*816);
        t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},
            "Partial-loss witness control never reached an actual wet source phase");
        f::buy(world,sim::set_building_operation(source,true));
        t::until(world,[&]{return t::work(world,field).wet_work_ticks==3;},"Partial-loss witness control lacks three genuine wet work units");
        f::buy(world,sim::set_building_operation(source,false));t::through(world,(hemp?8U:4U)*816);
        check(hemp?s::hemp(world,source).wasted_work==3:s::plant(world,field).wasted_work==3,
            "Partial-loss witness control failed its real off-season three-unit loss");
        t::through(world,hemp?9792+3*816:5*816);
        t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},
            "Regrowth witness control never regained actual supplied water");
        f::buy(world,sim::set_building_operation(source,true));
        t::until(world,[&]{return hemp?s::hemp(world,source).ready_tick.has_value():s::plant(world,field).ready_tick.has_value();},
            "Lost partial crop never earned its later real ready cycle");
        t::until(world,[&]{if(hemp){const auto& r=s::hemp(world,source);return r.latest_trip&&!r.latest_trip->picked_tick&&!r.latest_trip->returned_tick;}
            const auto& r=p::source(world,source);return r.latest_trip&&!r.latest_trip->picked_tick&&!r.latest_trip->returned_tick;},
            "Ready witness exact-state control lacks its real unpicked outward worker");
        f::buy(world,sim::set_building_operation(source,false));t::ticks(world,4);
        const auto path=fixture.root.path/"frozen-current-ready49.json";
        const auto reject_drift=[&]{const auto bytes=roundtrip(fixture,world,path);auto json=Json::parse(bytes);const auto before=world.snapshot();
            auto& row=hemp?json_row(json["world"]["seasonal_agriculture"]["hemp_farms"],"farm_id",source):
                json_row(json["world"]["plantations"]["sources"],"building_id",source);
            auto& witness=row["latest_trip"]["irrigation_ready_work"];
            check(witness["wasted_work"].template get<std::uint64_t>()>=2 && witness["bonus_work"].template get<std::uint64_t>()>0,
                "Exact-state witness mutation lacks genuine prior loss and bonus work");
            witness["base_work"]=witness["base_work"].template get<std::uint64_t>()-1;
            witness["wet_work_ticks"]=witness["wet_work_ticks"].template get<std::uint64_t>()-4;
            witness["bonus_work"]=witness["bonus_work"].template get<std::uint64_t>()-1;
            witness["wasted_work"]=witness["wasted_work"].template get<std::uint64_t>()-2;
            const auto hostile=json.dump();f::write_text(path,hostile);
            refuses([&]{(void)fixture.restore(save::read_save(path));},
                "A still-current ready cycle accepted an internally balanced lower-work/lower-loss witness");
            check(world.snapshot()==before&&f::read_text(path)==hostile,"Rejected frozen witness changed live authority or supplied bytes");f::write_text(path,bytes);};
        reject_drift();
        t::until(world,[&]{return hemp?s::hemp(world,source).latest_trip->returned_tick.has_value():p::source(world,source).latest_trip->returned_tick.has_value();},
            "Paused unpicked witness control failed its real empty return");
        reject_drift();
    }
}

void ready_epochs_and_material_witnesses_cannot_borrow_future_wet_work() {
    constexpr std::array<unsigned,9> opening{2,6,5,7,4,3,2,1,3};
    for(unsigned kind=0;kind<9;++kind) {
        t::Fixture fixture;auto world=fixture.world();t::through(world,opening[kind]*816+50);
        const auto city=s::build(world);const auto fields=t::subjects(city);
        std::vector<sim::BuildingId> sources(city.farms.begin(),city.farms.end());
        sources.push_back(city.hemp_farm);sources.push_back(city.materials.tea_source);
        sources.push_back(city.materials.lacquer_source);sources.push_back(city.materials.mulberry_source);
        for(const auto id:sources)f::buy(world,sim::set_building_operation(id,false));
        // Plenty of real elapsed growing opportunities exist before any Pump
        // purchase, but actual paused sources have earned precisely no work.
        t::ticks(world,400);const auto impossible_ready=world.ticks()-1;const auto water=t::network(world);
        f::buy(world,sim::set_building_operation(sources[kind],true));const auto field=fields[kind];
        const auto is_ready=[&]{return kind<5?s::field(world,field).ready_tick.has_value():kind==5?
            s::hemp(world,city.hemp_farm).ready_tick.has_value():s::plant(world,field).ready_tick.has_value();};
        t::until(world,is_ready,"Delayed paid Pump failed to earn the target's real wet ready crop");
        check(t::work(world,field).bonus_work>0 && t::base_work(world,field)<80,
            "Ready-epoch hostile control lacks genuinely bonus-assisted growth");
        if(kind<5) {
            f::buy(world,sim::set_building_operation(field,false));
            t::until(world,[&]{return !q::active(world,sources[kind]);},"Ready epoch control failed its genuine empty worker return");
            t::ticks(world,20);f::buy(world,sim::set_building_operation(field,true));
            t::until(world,[&]{return q::active(world,sources[kind]) && !q::worker(world,sources[kind]).work_ticks;},
                "Retained ready crop failed to allocate its later zero-growth harvest worker");
        }
        const auto path=fixture.root.path/("ready-witness-"+std::to_string(kind)+".json");
        std::string bytes;Json original;sim::WorldSnapshot before;
        const auto capture=[&]{bytes=roundtrip(fixture,world,path);original=Json::parse(bytes);before=world.snapshot();};
        const auto reject=[&](auto mutate,const char* why){auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
            refuses([&]{(void)fixture.restore(save::read_save(path));},why);
            check(world.snapshot()==before && f::read_text(path)==hostile,"Rejected ready/witness input changed actual work, cargo or source bytes");
            f::write_text(path,bytes);};
        capture();
        reject([&](auto& j){auto& w=j["world"];
            if(kind<5){json_row(w["seasonal_agriculture"]["fields"],"field_id",field)["ready_tick"]=impossible_ready;
                json_row(w["physical_food_growers"]["farms"],"farm_id",sources[kind])["latest_trip"]["ready_tick"]=impossible_ready;}
            else if(kind==5){auto& row=json_row(w["seasonal_agriculture"]["hemp_farms"],"farm_id",city.hemp_farm);
                row["ready_tick"]=impossible_ready;if(!row["latest_trip"].is_null())row["latest_trip"]["ready_tick"]=impossible_ready;}
            else{json_row(w["seasonal_agriculture"]["plants"],"plant_id",field)["ready_tick"]=impossible_ready;
                auto& trip=json_row(w["plantations"]["sources"],"building_id",sources[kind])["latest_trip"];
                if(!trip.is_null())trip["ready_tick"]=impossible_ready;}
        },"A currently ready crop borrowed wet bonus work before its paid Pump could supply");
        if(kind<5) {
            t::until(world,[&]{return q::receipt_field(world,field).returned==1;},"Retained wet-ready Food never earned actual harvest work and Farm return");
            check(!q::worker(world,sources[kind]).work_ticks,"Food receipt hostile control accidentally added later growth work");capture();
            reject([&](auto& j){auto& w=j["world"];
                json_row(w["physical_food_growers"]["farms"],"farm_id",sources[kind])["latest_trip"]["ready_tick"]=impossible_ready;
                json_row(w["physical_food_growers"]["fields"],"field_id",field)["last_receipt"]["ready_tick"]=impossible_ready;
            },"A coherent Food trip and receipt borrowed future bonus for an impossible early ready epoch");
            continue;
        }
        t::until(world,[&]{if(kind==5){const auto& r=s::hemp(world,city.hemp_farm);return r.latest_trip && !r.latest_trip->returned_tick;}
            const auto& r=p::source(world,sources[kind]);return r.latest_trip && !r.latest_trip->returned_tick;},
            "Material ready witness lacks its actual dispatched physical worker");capture();
        const auto trip=[&](Json& j)->Json& {auto& w=j["world"];return kind==5?
            json_row(w["seasonal_agriculture"]["hemp_farms"],"farm_id",city.hemp_farm)["latest_trip"]:
            json_row(w["plantations"]["sources"],"building_id",sources[kind])["latest_trip"];};
        reject([&](auto& j){trip(j).erase("irrigation_ready_work");},"Schema49 material trip accepted a missing immutable ready-work witness");
        reject([&](auto& j){trip(j)["irrigation_ready_work"]=nullptr;},"Schema49 material trip accepted a null ready-work witness");
        reject([&](auto& j){trip(j)["irrigation_ready_work"]=7;},"Schema49 material trip accepted a non-object ready-work witness");
        reject([&](auto& j){trip(j)["irrigation_ready_work"]["wet_cache"]=true;},"Material witness admitted unknown derived water authority");
        for(const char* key:{"base_work","wet_work_ticks","bonus_work","clipped_bonus","wasted_work"}) {
            reject([&](auto& j){trip(j)["irrigation_ready_work"].erase(key);},"Material ready witness accepted a missing work counter");
            reject([&](auto& j){trip(j)["irrigation_ready_work"][key]=UINT64_MAX;},"Material ready witness accepted impossible future or overflowing work");
        }
        reject([&](auto& j){auto& w=trip(j)["irrigation_ready_work"];
            for(const char* key:{"base_work","wet_work_ticks"})w[key]=w[key].template get<std::uint64_t>()+4;
            w["bonus_work"]=w["bonus_work"].template get<std::uint64_t>()+1;
            w["wasted_work"]=w["wasted_work"].template get<std::uint64_t>()+5;
        },"An internally balanced material ready witness borrowed future base/wet/bonus/loss counters");
        t::until(world,[&]{return kind==5?s::hemp_origin(world,city.hemp_farm).harvested==1:p::plant(world,field).returned==1;},
            "Material ready witness never reached its genuine physical source receipt");capture();
        const auto receipt=[&](Json& j)->Json& {auto& w=j["world"];return kind==5?
            json_row(w["seasonal_agriculture"]["hemp_farms"],"farm_id",city.hemp_farm)["last_receipt"]:
            json_row(w["plantations"]["plants"],"plant_id",field)["last_receipt"];};
        auto witness_control=original;
        check(trip(witness_control)["irrigation_ready_work"]==receipt(witness_control)["irrigation_ready_work"],
            "Actual material receipt failed to copy its exact immutable dispatch-ready work");
        reject([&](auto& j){receipt(j).erase("irrigation_ready_work");},"Schema49 material receipt lost mandatory readiness provenance");
        reject([&](auto& j){receipt(j)["irrigation_ready_work"]=nullptr;},"Schema49 material receipt accepted null readiness provenance");
        reject([&](auto& j){trip(j)["ready_tick"]=impossible_ready;receipt(j)["ready_tick"]=impossible_ready;},
            "Coherent material trip/receipt borrowed wet readiness before the first possible supply");
        reject([&](auto& j){auto& w=receipt(j)["irrigation_ready_work"];
            w["base_work"]=w["base_work"].template get<std::uint64_t>()+1;
            w["wet_work_ticks"]=w["wet_work_ticks"].template get<std::uint64_t>()-4;
            w["bonus_work"]=w["bonus_work"].template get<std::uint64_t>()-1;
        },"One material Grower retained two contradictory but individually bounded ready-work witnesses");
        (void)water;
    }
}

void historical32_water_has_no_bonus_and_cross_generation_writers_refuse() {
    t::Fixture fixture;auto current=fixture.world();const auto path=fixture.root.path/"current49.json";const auto bytes=roundtrip(fixture,current,path);
    auto old=sim::World::create_seasonal_agriculture(fixture.policy,fixture.model,fixture.terrain);constexpr std::array<std::size_t,1> crops{0};
    const auto scene=q::build(old,crops);const auto water=t::network(old);
    s::until(old,[&]{return s::field(old,scene.fields[0]).ready_tick.has_value();},"Historical Rule32 never earned its unchanged80 base-work ready crop");
    check(q::work(old,scene.fields[0]).productive_ticks==80 && !old.irrigation_productivity_state() &&
        old.irrigation_field_status(old.building(scene.fields[0]).cell).water_available &&
        !old.irrigation_field_status(old.building(scene.fields[0]).cell).yield_effect_active,"Historical supplied water acquired a gameplay bonus");
    const auto old_path=fixture.root.path/"historical48.json";save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());
    const auto old_bytes=f::read_text(old_path);const auto document=save::read_save(old_path);
    check(document.source_schema_version==48 && document.world.rule_version==32 && !document.world.irrigation_productivity &&
        sim::World::restore_seasonal_agriculture(document.world,fixture.policy,fixture.model,fixture.terrain).snapshot()==old.snapshot(),
        "Historical Rule32 changed exact identity or acquired productivity state");
    refuses([&]{(void)save::restore_irrigation_productivity(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Productivity restore automatically migrated Rule32/schema48");
    refuses([&]{save::write_save(old_path,fixture.document(current),fixture.root.data,fixture.mask());},"Productivity writer overwrote historical schema48");
    refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"Historical writer overwrote schema49");
    auto injected=document;injected.world.irrigation_productivity.emplace();
    refuses([&]{(void)sim::World::restore_seasonal_agriculture(injected.world,fixture.policy,fixture.model,fixture.terrain);},"Rule32 accepted new productivity authority");
    auto hidden=fixture.document(old);hidden.world.physical_food_growers->farms[0].latest_trip->irrigation_bonus_before=1;
    refuses([&]{save::write_save(old_path,hidden,fixture.root.data,fixture.mask());},"Historical writer silently discarded new trip bonus provenance");
    check(f::read_text(path)==bytes && f::read_text(old_path)==old_bytes,"Cross-generation refusal changed retained save bytes");(void)water;

    for(int x=89;x<=123;++x)if(!old.road_cell({x,104}))f::buy(old,{sim::CommandType::PlaceRoad,{x,104}});
    const auto tea=p::purchase(old,sim::CommandType::PlaceOriginalTeaCuringShed,{120,105});
    const auto tea_plant=p::plant(old,sim::CommandType::PlaceOriginalTeaBush,tea,{123,105});
    const auto hemp=q::purchase(old,sim::CommandType::PlaceOriginalHempFarm,{110,105});
    p::plant(old,sim::CommandType::PlaceOriginalHempField,hemp,{112,105});
    s::until(old,[&]{return p::plant(old,tea_plant).returned>0 && s::hemp_origin(old,hemp).harvested>0;},
        "Historical material witness controls lack their unchanged actual source returns");
    const auto historical_path=fixture.root.path/"historical-material48.json";
    save::write_save(historical_path,fixture.document(old),fixture.root.data,fixture.mask());const auto historical_bytes=f::read_text(historical_path);
    auto historical_json=Json::parse(historical_bytes);auto& historical_world=historical_json["world"];
    check(!json_row(historical_world["plantations"]["sources"],"building_id",tea)["latest_trip"].contains("irrigation_ready_work") &&
        !json_row(historical_world["plantations"]["plants"],"plant_id",tea_plant)["last_receipt"].contains("irrigation_ready_work") &&
        !json_row(historical_world["seasonal_agriculture"]["hemp_farms"],"farm_id",hemp)["latest_trip"].contains("irrigation_ready_work") &&
        !json_row(historical_world["seasonal_agriculture"]["hemp_farms"],"farm_id",hemp)["last_receipt"].contains("irrigation_ready_work"),
        "Historical schema48 output acquired new immutable irrigation witness keys");
    for(unsigned target=0;target<4;++target) {
        auto hidden_material=fixture.document(old);
        if(target<2){for(auto& row:hidden_material.world.plantations->sources)if(row.building_id==tea&&target==0)row.latest_trip->irrigation_ready_work.emplace();
            for(auto& row:hidden_material.world.plantations->plants)if(row.plant_id==tea_plant&&target==1)row.last_receipt->irrigation_ready_work.emplace();}
        else for(auto& row:hidden_material.world.seasonal_agriculture->hemp_farms)if(row.farm_id==hemp) {
            if(target==2)row.latest_trip->irrigation_ready_work.emplace();else row.last_receipt->irrigation_ready_work.emplace();}
        refuses([&]{save::write_save(historical_path,hidden_material,fixture.root.data,fixture.mask());},
            "Historical writer silently dropped an engaged schema49 material readiness witness");
        check(f::read_text(historical_path)==historical_bytes,"Historical witness refusal replaced exact schema48 bytes");
    }
}
}
template<class F>void run(const char* name,F action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}}
int main(){try{run("wet/dry/12k",wet_dry_reconnect_ready_cargo_recovery_and_twelve_thousand_ticks);
    run("hostile counters",hostile_bonus_carry_identity_and_epoch_inputs_are_atomic);
    run("wet material cargo",genuine_wet_hemp_and_plant_cargo_roundtrip_without_arrival_credit);
    run("impossible wet history",wet_history_requires_a_real_pump_paid_ditch_and_possible_supply_phase);
    run("ready epochs/material witnesses",ready_epochs_and_material_witnesses_cannot_borrow_future_wet_work);
    run("frozen material witness",still_current_material_epochs_freeze_even_cancelled_work_witnesses);
    run("historical32",historical32_water_has_no_bonus_and_cross_generation_writers_refuse);
    std::cout<<"Irrigation productivity strict Save/Recovery/carry/12k replay tests passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
