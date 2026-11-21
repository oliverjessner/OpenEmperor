#include "SeasonalAgricultureFixture.h"
#include "persistence/RecoveryStore.h"

#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace s=oe_seasonal_agriculture_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace p=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using Json=nlohmann::json;
using f::check;
using Phase=sim::PhysicalFoodGrowerPhase;
template<class F>void refuses(F action,const char* why) {try{action();}catch(const std::exception&){return;}throw std::runtime_error(why);}

std::string roundtrip(const s::Fixture& fixture,const sim::World& world,const std::filesystem::path& path,int continuation=3) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto encoded=Json::parse(bytes);
    check(document.source_schema_version==48 && document.world.rule_version==32 && document.world==before &&
        document.world.seasonal_agriculture && document.world.physical_food_growers && document.world.hunting_food &&
        !document.world.agriculture && !document.world.fishing_food && !document.migrated_from_schema1 &&
        encoded.at("rules").at("seasonal_agriculture")==sim::seasonal_agriculture_contract &&
        encoded.at("rules").at("seasonal_agriculture_revision")==1 && encoded.at("world").at("seasonal_agriculture").at("revision")==1,
        "Schema48 lost exact seasonal lineage or duplicated inherited Food authority");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Seasonal restore advanced the derived calendar, work, expiry or cargo");
    check(restored.agriculture_calendar()==world.agriculture_calendar(),"Seasonal restore changed the sole derived World calendar");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,"Seasonal canonical rewrite changed bytes or live authority");
    auto expected=world;
    for(int i=0;i<continuation;++i) {
        expected.tick();restored.tick();check(expected.snapshot()==restored.snapshot(),"Independent seasonal continuation diverged");
        if(i%257==0){s::valid(expected);s::valid(restored);}
    }
    s::valid(expected);s::valid(restored);return bytes;
}

void boundary_growth_ready_harvest_cargo_and_recovery() {
    s::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto path=fixture.root.path/"seasonal48.json";const auto field=city.fields[0],farm=city.farm;
    const auto channel=s::farm(world,farm).courier_id;
    save::RecoveryStore recovery(fixture.root.path/"seasonal-recovery",fixture.root.data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=[&] {
        roundtrip(fixture,world,path);
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable() && entry.schema==48 && entry.rule_version==32 &&
            fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost exact seasonal work, ready epoch or physical cargo");
    };
    for(const auto boundary:{1632U,4896U,5712U,9792U}) {
        s::through(world,boundary-1);checkpoint();s::ticks(world,1);checkpoint();s::ticks(world,1);checkpoint();
    }
    bool outward=false,waiting=false,working=false,ready=false,harvesting=false,loaded=false,receipt=false;
    for(int i=0;i<22000 && !receipt;++i) {
        world.tick();
        if(s::active(world,farm)) {
            const auto trip=s::worker(world,farm);const auto& actor=world.courier(channel);
            bool* seen=trip.phase==Phase::Outward && actor.edge_progress>0 ? &outward:
                trip.phase==Phase::Waiting && trip.wait_left>0 ? &waiting:
                trip.phase==Phase::Working && trip.work_ticks>3 && !s::field(world,field).ready_tick ? &working:
                s::field(world,field).ready_tick && !s::field(world,field).harvest_progress ? &ready:
                s::field(world,field).harvest_progress>0 ? &harvesting:
                world.physical_food_grower_cargo(channel)==1 && actor.edge_progress>0 ? &loaded:nullptr;
            if(seen && !*seen){checkpoint();*seen=true;}
        }
        if(outward && waiting && working && ready && harvesting && loaded && s::receipt_field(world,field).returned>0) {
            checkpoint();receipt=true;
        }
    }
    check(outward && waiting && working && ready && harvesting && loaded && receipt,
        "Seasonal save acceptance lacks an actual outward/wait/growth/ready/harvest/cargo/return checkpoint");
    roundtrip(fixture,world,path,12000);
    check(recovery.catalog().histories.size()==1 && !recovery.catalog().truncated,"Seasonal Recovery exceeded bounded history publication");
}

void hostile_seasonal_work_and_exact_generation() {
    s::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{4};const auto city=q::build(world,crops);
    s::until(world,[&]{return s::field(world,city.fields[0]).ready_tick.has_value();},"Hostile seasonal source never earned a real ready crop");
    const auto path=fixture.root.path/"hostile48.json";const auto bytes=roundtrip(fixture,world,path);const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why) {
        auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before && f::read_text(path)==hostile,"Rejected seasonal input changed supplied bytes or live paid authority");
    };
    reject([](auto& j){j["schema_version"]=49;},"Unknown successor schema accepted");
    reject([](auto& j){j["rules"]["version"]=33;},"Unknown successor rule accepted");
    reject([](auto& j){j["rules"].erase("seasonal_agriculture");},"Missing seasonal marker accepted");
    reject([](auto& j){j["rules"]["seasonal_agriculture"]="other";},"Unknown seasonal marker accepted");
    reject([](auto& j){j["rules"]["seasonal_agriculture_revision"]=2;},"Unknown seasonal rule revision accepted");
    reject([](auto& j){j["world"].erase("seasonal_agriculture");},"Missing sole seasonal state accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["revision"]=2;},"Unknown seasonal state revision accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["calendar"]=1;},"Second mutable seasonal calendar accepted");
    reject([](auto& j){auto& rows=j["world"]["seasonal_agriculture"]["fields"];rows.push_back(rows[0]);},"Duplicate seasonal Field row accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"].clear();},"Missing seasonal Field row accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["field_id"]=99999;},"Foreign seasonal Field identity accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["farm_id"]=99999;},"Foreign seasonal source identity accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["wasted_work"]=UINT64_MAX;},"Seasonal expired-work underflow accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["ready_tick"]=nullptr;},"Ready crop without bound seasonal epoch accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["ready_tick"]=0;},"Off-season ready epoch accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["ready_tick"]=j["world"]["ticks"].template get<std::uint64_t>()+1;},"Future ready epoch accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["harvest_progress"]=5;},"Unsettled fifth harvest-work unit accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["harvest_work"]=1;},"Invented real harvest labor accepted");
    reject([](auto& j){j["world"]["seasonal_agriculture"]["fields"][0]["expired_ready"]=99;},"Invented seasonal ready expirations accepted");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["latest_trip"]["ready_tick"]=nullptr;},
        "Ready physical Food worker lost its bound crop cycle");
    reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["latest_trip"]["ready_tick"]=0;},
        "Physical Food worker borrowed an off-season ready cycle");
    f::write_text(path,bytes);auto duplicate=bytes;
    const std::string marker=std::string("\"seasonal_agriculture\": \"")+sim::seasonal_agriculture_contract+"\"";
    const auto at=duplicate.find(marker);check(at!=std::string::npos,"Seasonal duplicate-marker control lost canonical key");
    duplicate.replace(at,marker.size(),marker+",\n    "+marker);f::write_text(path,duplicate);
    refuses([&]{(void)save::read_save(path);},"Duplicate seasonal contract marker accepted");f::write_text(path,bytes);
    auto impossible=fixture.document(world);impossible.world.seasonal_agriculture->fields[0].wasted_work=UINT64_MAX;
    refuses([&]{save::write_save(path,impossible,fixture.root.data,fixture.mask());},"Writer published impossible seasonal expired work");
    check(f::read_text(path)==bytes && world.snapshot()==before,"Rejected seasonal writer changed existing target or World");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Injected seasonal atomic-write failure ignored");
    check(f::read_text(path)==bytes,"Seasonal atomic failure replaced its existing target");

    q::Fixture historical;auto old=historical.world();const auto old_path=historical.root.path/"old47.json";
    save::write_save(old_path,historical.document(old),historical.root.data,historical.mask());const auto old_bytes=f::read_text(old_path);
    const auto document=save::read_save(old_path);
    check(document.source_schema_version==47 && document.world.rule_version==31 && !document.world.seasonal_agriculture &&
        historical.restore(document).snapshot()==old.snapshot(),"Historical Rule31 acquired seasonal authority or changed exact restore");
    refuses([&]{(void)save::restore_seasonal_agriculture(document,historical.root.data,historical.mask(),historical.policy,historical.model,historical.terrain);},
        "Seasonal restore automatically migrated Rule31/schema47");
    refuses([&]{save::write_save(old_path,fixture.document(world),fixture.root.data,fixture.mask());},"Seasonal writer overwrote schema47");
    refuses([&]{save::write_save(path,historical.document(old),historical.root.data,historical.mask());},"Historical writer overwrote schema48");
    auto injected=document;injected.world.seasonal_agriculture.emplace();refuses([&]{(void)historical.restore(injected);},"Rule31 accepted seasonal authority");
    check(f::read_text(path)==bytes && f::read_text(old_path)==old_bytes,"Cross-generation refusal changed either retained input");
    const auto old_city=q::build(old,crops);
    q::until(old,[&]{return q::phase(old,old_city.farm,Phase::Working);},"Historical hidden-state control lacks its real Rule31 worker");
    auto hidden=historical.document(old);hidden.world.physical_food_growers->farms[0].latest_trip->ready_tick=123;
    refuses([&]{save::write_save(old_path,hidden,historical.root.data,historical.mask());},
        "Rule31 writer silently discarded a new seasonal ready-cycle operand");
    hidden=historical.document(old);hidden.world.physical_food_growers->farms[0].latest_trip->wasted_work_before=1;
    refuses([&]{save::write_save(old_path,hidden,historical.root.data,historical.mask());},
        "Rule31 writer silently discarded new seasonal expired-work authority");
    check(f::read_text(old_path)==old_bytes,"Rejected historical hidden-state publication changed existing schema47 bytes");
}

void real_hemp_and_plant_cargo_save_without_arrival_credit() {
    s::Fixture fixture;auto world=fixture.world();const auto city=s::build(world);
    const auto path=fixture.root.path/"material48.json";bool hemp=false,tea=false;
    for(int i=0;i<16000 && !(hemp && tea);++i) {
        world.tick();
        const auto& source=s::hemp(world,city.hemp_farm);
        if(!hemp && world.seasonal_hemp_grower_cargo(source.courier_id)>0 && world.courier(source.courier_id).edge_progress>0) {
            const auto origin=s::hemp_origin(world,city.hemp_farm).harvested;const auto trip=source.latest_trip;
            roundtrip(fixture,world,path);auto restored=fixture.restore(save::read_save(path));
            check(s::hemp_origin(restored,city.hemp_farm).harvested==origin && s::hemp(restored,city.hemp_farm).latest_trip==trip &&
                restored.seasonal_hemp_grower_cargo(source.courier_id)==1,"Loaded Hemp restore granted stock before physical source arrival");
            hemp=true;
        }
        const auto& plant=p::source(world,city.materials.tea_source);
        if(!tea && plant.latest_trip && plant.latest_trip->picked_tick && !plant.latest_trip->returned_tick && world.courier(plant.courier_id).edge_progress>0) {
            const auto origin=p::plant(world,city.materials.tea_plant).returned;const auto trip=plant.latest_trip;
            roundtrip(fixture,world,path);auto restored=fixture.restore(save::read_save(path));
            check(p::plant(restored,city.materials.tea_plant).returned==origin && p::source(restored,city.materials.tea_source).latest_trip==trip,
                "Loaded plantation restore credited an unreturned seasonal pickup");tea=true;
        }
    }
    check(hemp && tea,"Seasonal Save lacks both real Hemp and plantation moving-cargo witnesses");s::valid(world);
}

void hostile_ready_epochs_harvest_clock_and_hemp_position() {
    s::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto field=city.fields[0];const auto path=fixture.root.path/"ready-epochs48.json";
    s::until(world,[&]{return s::field(world,field).ready_tick.has_value();},"Ready-epoch control lacks80 real at-field growth units");
    auto bytes=roundtrip(fixture,world,path);auto original=Json::parse(bytes);auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why) {
        auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before && f::read_text(path)==hostile,"Rejected ready-epoch input changed bytes or the paid World");
        f::write_text(path,bytes);
    };
    reject([](auto& j){
        j["world"]["seasonal_agriculture"]["fields"][0]["ready_tick"]=1632;
        j["world"]["physical_food_growers"]["farms"][0]["latest_trip"]["ready_tick"]=1632;
    },"Coherent ready epoch predating actual field arrival and80 work units accepted");

    // Preserve a genuinely ready crop through an empty cancellation. Its next
    // worker then harvests an earlier cycle, leaving room for a contradictory
    // but individually time-bounded receipt epoch to exercise exact binding.
    f::buy(world,sim::set_building_operation(field,false));
    s::until(world,[&]{return !s::active(world,city.farm);},"Ready-epoch control failed its physical empty return");
    s::ticks(world,20);f::buy(world,sim::set_building_operation(field,true));
    s::until(world,[&]{return s::receipt_field(world,field).returned==1;},"Retained ready cycle never earned five harvest ticks and a real Farm receipt");
    bytes=roundtrip(fixture,world,path);original=Json::parse(bytes);before=world.snapshot();
    const auto& trip=s::worker(world,city.farm);const auto& receipt=*s::receipt_field(world,field).last_receipt;
    check(trip.ready_tick && receipt.ready_tick==*trip.ready_tick && receipt.ready_tick+1<trip.dispatched_tick &&
        receipt.harvested_tick-receipt.ready_tick>5,"Ready receipt mutation lacks a real retained-cycle time interval");
    reject([](auto& j){auto& epoch=j["world"]["physical_food_growers"]["fields"][0]["last_receipt"]["ready_tick"];epoch=epoch.template get<std::uint64_t>()+1;},
        "Same physical Food Grower retained two different ready-cycle epochs");
    reject([](auto& j){
        auto& trip=j["world"]["physical_food_growers"]["farms"][0]["latest_trip"];
        const auto late=trip["harvested_tick"].template get<std::uint64_t>()-1;
        trip["ready_tick"]=late;j["world"]["physical_food_growers"]["fields"][0]["last_receipt"]["ready_tick"]=late;
    },"Coherent Food harvest skipped four of its five real harvest-work ticks");

    auto materials=fixture.world();const auto scene=s::build(materials);const auto material_path=fixture.root.path/"material-ready-epochs48.json";
    s::until(materials,[&]{return p::plant(materials,scene.materials.tea_plant).returned==1;},
        "Plantation epoch control lacks a real first seasonal Tea pickup/return");
    const auto material_bytes=roundtrip(fixture,materials,material_path);auto material_json=Json::parse(material_bytes);
    const auto material_before=materials.snapshot();
    auto& tea=material_json["world"]["plantations"]["plants"][0]["last_receipt"];
    check(tea["ready_tick"].get<std::uint64_t>()+1<tea["picked_tick"].get<std::uint64_t>(),
        "Plantation epoch control lacks a genuine physical outward interval");
    tea["ready_tick"]=tea["ready_tick"].get<std::uint64_t>()+1;const auto hostile_tea=material_json.dump();f::write_text(material_path,hostile_tea);
    refuses([&]{(void)fixture.restore(save::read_save(material_path));},"Same plantation Grower retained two different ready-cycle epochs");
    check(materials.snapshot()==material_before && f::read_text(material_path)==hostile_tea,"Rejected plantation receipt epoch changed bytes or World");
    f::write_text(material_path,material_bytes);

    s::until(materials,[&]{const auto& farm=s::hemp(materials,scene.hemp_farm);return farm.latest_trip &&
        !farm.latest_trip->picked_tick && !farm.latest_trip->returned_tick && materials.ticks()==farm.latest_trip->dispatched_tick;},
        "Hemp elapsed-position control lacks its real first outward dispatch");
    const auto hemp_bytes=roundtrip(fixture,materials,material_path);auto hemp_json=Json::parse(hemp_bytes);
    const auto hemp_before=materials.snapshot();const auto channel=s::hemp(materials,scene.hemp_farm).courier_id;
    bool changed=false;for(auto& actor:hemp_json["world"]["couriers"])if(actor["id"]==static_cast<std::uint32_t>(channel)) {
        check(actor["edge_progress"]==1,"Fresh Hemp dispatch did not retain exactly one actual movement tick");
        actor["edge_progress"]=4;changed=true;
    }
    check(changed,"Hemp elapsed-position control lost its exact courier channel");const auto hostile_hemp=hemp_json.dump();f::write_text(material_path,hostile_hemp);
    refuses([&]{(void)fixture.restore(save::read_save(material_path));},"Newly dispatched Hemp Grower moved four ticks in one actual World tick");
    check(materials.snapshot()==hemp_before && f::read_text(material_path)==hostile_hemp,"Rejected Hemp position changed bytes or World");
}

void live_unpicked_material_trips_keep_the_exact_ready_cycle() {
    s::Fixture fixture;auto world=fixture.world();const auto city=s::build(world);
    const auto path=fixture.root.path/"unpicked-cycle48.json";
    const auto reject_trip_drift=[&](const char* collection,std::size_t index,const char* why) {
        const auto before=world.snapshot();const auto bytes=roundtrip(fixture,world,path);auto json=Json::parse(bytes);
        auto& epoch=json["world"][collection]["sources"][index]["latest_trip"]["ready_tick"];
        epoch=epoch.get<std::uint64_t>()+1;const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before && f::read_text(path)==hostile,"Rejected unpicked cycle drift changed input bytes or World");
        f::write_text(path,bytes);
    };
    const auto tea=city.materials.tea_source,tea_plant=city.materials.tea_plant;
    s::until(world,[&]{return p::plant(world,tea_plant).growth==80;},"Live plantation cycle control lacks genuinely ready Tea");
    f::buy(world,sim::set_building_operation(tea,false));
    s::until(world,[&]{const auto& row=p::source(world,tea);return row.latest_trip && row.latest_trip->returned_tick.has_value();},
        "Paused Tea failed its real empty outward/return cancellation");
    check(!p::source(world,tea).latest_trip->picked_tick && s::plant(world,tea_plant).ready_tick,
        "Tea cancellation loaded cargo or discarded its unexpired ready crop");
    s::ticks(world,20);f::buy(world,sim::set_building_operation(tea,true));
    s::until(world,[&]{const auto& row=p::source(world,tea);return row.latest_trip && !row.latest_trip->returned_tick &&
        !row.latest_trip->picked_tick && world.ticks()==row.latest_trip->dispatched_tick;},"Retained ready Tea never dispatched its later real worker");
    check(p::source(world,tea).latest_trip->ready_tick+1<p::source(world,tea).latest_trip->dispatched_tick,
        "Tea trip drift lacks two individually valid ready/dispatch epochs");
    reject_trip_drift("plantations",0,"Unpicked live Tea worker changed to a different still-live ready epoch");

    const auto hemp=city.hemp_farm;
    s::until(world,[&]{return s::hemp(world,hemp).ready_tick.has_value();},"Live Hemp cycle control lacks genuinely ready work");
    f::buy(world,sim::set_building_operation(hemp,false));
    s::until(world,[&]{const auto& row=s::hemp(world,hemp);return row.latest_trip && row.latest_trip->returned_tick.has_value();},
        "Paused Hemp failed its real empty outward/return cancellation");
    check(!s::hemp(world,hemp).latest_trip->picked_tick && s::hemp(world,hemp).ready_tick,
        "Hemp cancellation loaded cargo or discarded its unexpired ready crop");
    s::ticks(world,20);f::buy(world,sim::set_building_operation(hemp,true));
    s::until(world,[&]{const auto& row=s::hemp(world,hemp);return row.latest_trip && !row.latest_trip->returned_tick &&
        !row.latest_trip->picked_tick && world.ticks()==row.latest_trip->dispatched_tick;},"Retained ready Hemp never dispatched its later real worker");
    check(s::hemp(world,hemp).latest_trip->ready_tick+1<s::hemp(world,hemp).latest_trip->dispatched_tick,
        "Hemp trip drift lacks two individually valid ready/dispatch epochs");
    const auto before=world.snapshot();const auto bytes=roundtrip(fixture,world,path);auto json=Json::parse(bytes);
    auto& epoch=json["world"]["seasonal_agriculture"]["hemp_farms"][0]["latest_trip"]["ready_tick"];
    epoch=epoch.get<std::uint64_t>()+1;const auto hostile=json.dump();f::write_text(path,hostile);
    refuses([&]{(void)fixture.restore(save::read_save(path));},"Unpicked live Hemp worker changed to a different still-live ready epoch");
    check(world.snapshot()==before && f::read_text(path)==hostile,"Rejected live Hemp cycle drift changed input bytes or World");
}
}
int main() {
    try {boundary_growth_ready_harvest_cargo_and_recovery();hostile_seasonal_work_and_exact_generation();real_hemp_and_plant_cargo_save_without_arrival_credit();
        hostile_ready_epochs_harvest_clock_and_hemp_position();live_unpicked_material_trips_keep_the_exact_ready_cycle();
        std::cout<<"Seasonal Agriculture exact Save/Recovery/boundaries/12k replay tests passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
