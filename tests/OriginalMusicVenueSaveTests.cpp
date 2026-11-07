#include "OriginalMusicVenueFixture.h"
#include "persistence/RecoveryStore.h"

#include <array>
#include <charconv>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace q=oe_music_venue_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using q::check;

sim::World earned(const q::Fixture& fixture) {
    auto world=fixture.world();const auto s=q::scene(world);
    q::until(world,[&]{return q::active(world,s.source)&&world.courier(s.channel).edge_progress>0;},
        "Separate-process fixture did not earn a begun physical performer edge");
    f::buy(world,sim::set_building_operation(s.source,false));return world;
}
void checked_model_capability() {
    q::Fixture fixture;
    check(fixture.model->supports_building(orig::ModelBuilding::TheatrePavilion)&&
          fixture.model->building_value(orig::ModelBuilding::TheatrePavilion,orig::BuildingField::Cost)==105&&
          fixture.model->building_value(orig::ModelBuilding::TheatrePavilion,orig::BuildingField::Employees)==7,
          "New culture preparation lost the actual typed Theatre row");
    const auto old=save::prepare_original_production_rules_model(fixture.root.data);
    check(!old->supports_building(orig::ModelBuilding::TheatrePavilion),"Old production preparation silently gained75 capability");
    for(int variant=0;variant<3;++variant) {
        f::Root root;auto text=q::authored_model();const std::string marker="75,Theatre Pavilion,{,105,";
        const auto at=text.find(marker);check(at!=std::string::npos,"Authored model capability row missing");
        if(variant==0)text.erase(at,text.find('\n',at)-at+1);
        else if(variant==1)text.replace(at,marker.size(),"75,Wrong Theatre Label,{,105,");
        else {
            text.replace(at,marker.size(),"75,Theatre Pavilion,{,2147483647,");
            const std::string modifier="2,Normal Modifier,{,100,";const auto mod=text.find(modifier);
            check(mod!=std::string::npos,"Authored checked-overflow modifier missing");
            text.replace(mod,modifier.size(),"2,Normal Modifier,{,200,");
        }
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        check(!save::prepare_original_production_rules_model(root.data)->supports_building(orig::ModelBuilding::TheatrePavilion),
              "An unused75 field changed historical production preparation");
        q::refuses([&]{(void)save::prepare_original_culture_rules_model(root.data);},
            "Missing/wrong-label/checked-overflow Theatre row entered new capability");
    }
}
void canonical_active_arrived_expired_and_recovery() {
    q::Fixture fixture;auto world=earned(fixture);const auto source=q::state(world).schools.front().building_id;
    const auto target=q::state(world).venues.front().building_id;
    const auto canonical=fixture.root.path/"canonical38.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    for(int phase=0;phase<3;++phase) {
        if(phase==1)q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},
            "Save checkpoint did not reach actual one-way arrival");
        if(phase==2)q::until(world,[&]{return q::venue(world,target).music_timer==0;},
            "Save checkpoint did not reach earned Venue expiry",2000);
        save::write_save(canonical,fixture.document(world),fixture.root.data,fixture.mask());
        const auto bytes=f::read_text(canonical);const auto snapshot=world.snapshot();
        const auto parsed=save::read_save(canonical);const auto json=Json::parse(bytes);
        check(parsed.source_schema_version==38&&parsed.world.rule_version==22&&parsed.world==snapshot&&
              json.at("rules").at("music_venues")==sim::original_music_venues_contract&&
              json.at("world").at("original_production").contains("culture")&&!parsed.migrated_from_schema1,
              "Schema38 lost exact marker, complete authority or explicit generation");
        auto restored=fixture.restore(parsed);check(restored.snapshot()==snapshot,"Independent active/retired performer restore differs");
        save::write_save(canonical,fixture.document(restored),fixture.root.data,fixture.mask());
        check(f::read_text(canonical)==bytes,"Complete canonical Schema38 bytes changed on rewrite");
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==38&&entry.rule_version==22&&f::read_text(entry.path)==bytes&&
              fixture.restore(save::read_save(entry.path)).snapshot()==snapshot,
              "Recovery lost literal complete Schema38 performer/timer authority");
        // Independent in-process continuation also compares every complete
        // snapshot, rather than only the visible timer or a chosen digest.
        auto continued=fixture.restore(parsed);auto expected=world;
        for(int tick=0;tick<103;++tick){continued.tick();expected.tick();check(continued.snapshot()==expected.snapshot(),
            "Complete103-tick active/timer continuation diverged");}
        q::invariants(continued);
    }
}
void nonzero_paid_school_desire_survives_save_and_recovery() {
    auto text=q::authored_model();
    const std::string empty="211,Music  School,{,50,0,0,0,0,8,";
    const auto at=text.find(empty);check(at!=std::string::npos,"Authored zero-spread School row missing");
    // Independently authored spread, not the supplied Original model row.
    // Rings1..3 contribute9, rings4..6 contribute8, and rings7..8 contribute7.
    text.replace(at,empty.size(),"211,Music  School,{,50,9,3,-1,8,8,");
    q::Fixture fixture(text);auto world=fixture.world();const auto scene=q::scene(world);
    // The nearest cells in the three2x2 Homes are at Chebyshev distances2,2,5
    // from the School's complete2x2 body. Flat authored terrain adds no bonus.
    constexpr std::array<int,3> expected{9,9,8};
    const auto check_sources=[&](const sim::World& current) {
        for(std::size_t i=0;i<scene.homes.size();++i) {
            const auto proof=current.original_influence_provenance(scene.homes[i]);
            check(proof.sources.size()==1&&proof.sources.front().building_id==scene.source&&
                  proof.sources.front().type==sim::OriginalInfluenceSourceType::Music211&&
                  proof.sources.front().contribution==expected[i]&&proof.raw_value==expected[i]&&
                  proof.cardinal_water_bonus==0&&proof.value==expected[i]&&
                  current.original_house_influence_value(scene.homes[i])==expected[i],
                  "Paid Rule22 School disappeared, duplicated, or granted an extra Theatre desirability source");
        }
    };
    check_sources(world);q::through(world,142);
    for(std::size_t i=0;i<scene.homes.size();++i) {
        const auto& row=world.original_production_state()->housing.houses[i];
        check(row.building_id==scene.homes[i]&&row.last_evaluation&&row.last_evaluation->tick==142&&
              row.last_evaluation->input.desirability==expected[i],
              "Actual occupied phase39 did not record the independent paid School contribution");
    }
    f::buy(world,sim::set_building_operation(scene.source,false));check_sources(world);
    const auto baseline=world.snapshot();const auto canonical=fixture.root.path/"school-f5.json";
    const auto rewrite=fixture.root.path/"school-f9.json";
    save::write_save(canonical,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(canonical);const auto parsed=save::read_save(canonical);
    auto continued=fixture.restore(parsed);check(continued.snapshot()==baseline,"Complete nonzero-School load changed authority");
    save::write_save(rewrite,fixture.document(continued),fixture.root.data,fixture.mask());
    check(f::read_text(rewrite)==bytes,"F5/F9 persistence boundary changed complete nonzero-School bytes");
    save::RecoveryStore recovery(fixture.root.path/"school-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto first=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
    check(first.loadable()&&f::read_text(first.path)==bytes&&
          fixture.restore(save::read_save(first.path)).snapshot()==baseline,
          "Recovery lost complete nonzero-School phase39 history");
    auto hostile=fixture.document(world);
    --hostile.world.original_economy->houses.front().house.desirability;
    auto& tampered=hostile.world.original_production->housing.houses.front();
    --tampered.last_evaluation->input.desirability;
    for(auto& entry:tampered.first_transitions)
        if(entry&&entry->evaluation.tick==142)--entry->evaluation.input.desirability;
    const auto refuses_paid_desire=[&](auto action,const std::string& message) {
        try{action();}catch(const std::exception& error) {
            check(std::string(error.what()).find("Housing historical desire differs from actual paid sources")!=
                  std::string::npos,"Desire forgery refused for a different reason: "+std::string(error.what()));
            return;
        }
        throw std::runtime_error(message);
    };
    refuses_paid_desire([&]{(void)fixture.restore(hostile);},"Forged paid-source desire reached restore");
    refuses_paid_desire([&]{save::write_save(canonical,hostile,fixture.root.data,fixture.mask());},
        "Forged paid-source desire reached atomic publication");
    check(world.snapshot()==baseline&&f::read_text(canonical)==bytes,
          "Rejected desire forgery changed live authority or prior canonical bytes");
    auto expected_world=world;
    for(int tick=0;tick<103;++tick) {
        continued.tick();expected_world.tick();
        check(continued.snapshot()==expected_world.snapshot(),"Complete nonzero-School103-tick continuation differs");
    }
    check_sources(continued);q::invariants(continued);
    const auto later=fixture.root.path/"school-continued.json",independent=fixture.root.path/"school-expected.json";
    save::write_save(later,fixture.document(continued),fixture.root.data,fixture.mask());
    save::write_save(independent,fixture.document(expected_world),fixture.root.data,fixture.mask());
    const auto last=recovery.write_checkpoint(history,fixture.document(continued),fixture.mask());
    check(f::read_text(later)==f::read_text(independent)&&f::read_text(last.path)==f::read_text(later)&&
          fixture.restore(save::read_save(last.path)).snapshot()==continued.snapshot(),
          "Full103-tick nonzero-School Save/Recovery bytes or restored snapshot differ");
}
void hostile_world_and_atomic_publication() {
    q::Fixture fixture;auto world=earned(fixture);const auto baseline=world.snapshot();
    check(world.ticks()==342&&q::state(world).schools.front().latest_trip->dispatched_tick==342,
          "Three paid Homes did not earn their actual fourth staffed School callback342");
    const auto file=fixture.root.path/"preserved38.json";save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(file);const auto original=save::read_save(file);
    const auto reject=[&](auto mutate) {
        auto hostile=original;mutate(hostile.world);
        q::refuses([&]{(void)fixture.restore(hostile);},"Hostile performer/venue authority reached restore");
        q::refuses([&]{save::write_save(file,hostile,fixture.root.data,fixture.mask());},
            "Hostile performer/venue authority reached atomic save publication");
        check(world.snapshot()==baseline&&f::read_text(file)==bytes,"Rejected restore/write changed live authority or preserved bytes");
    };
    reject([](auto& s){s.original_production->culture.reset();});
    reject([](auto& s){s.original_production->safe_demolition.emplace();});
    reject([](auto& s){s.original_production->culture->schools.clear();});
    reject([](auto& s){s.original_production->culture->venues.clear();});
    reject([](auto& s){auto& rows=s.original_production->culture->schools;rows.push_back(rows.front());});
    reject([](auto& s){auto& rows=s.original_production->culture->venues;rows.push_back(rows.front());});
    reject([](auto& s){s.original_production->culture->next_performer_id=0;});
    reject([](auto& s){++s.original_production->culture->next_performer_id;});
    reject([](auto& s){s.original_production->culture->schools.front().courier_id=static_cast<sim::CourierId>(999);});
    reject([](auto& s){s.original_production->culture->schools.front().building_id=s.buildings.front().id;});
    reject([](auto& s){s.original_production->culture->schools.front().spawn_counter=255;});
    reject([](auto& s){s.original_production->culture->schools.front().attempts=0;});
    reject([](auto& s){s.original_production->culture->schools.front().spawned=0;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip.reset();});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->performer_id=static_cast<sim::PerformerId>(0);});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->venue_id=s.buildings.front().id;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=s.ticks+1;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=36;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=189;});
    // First admissions120 supply only6 residents at138. That phase cannot
    // train the8-worker School, so291 has only three possible staffed calls.
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=291;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->accepted=true;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->arrival_tick=s.ticks;});
    reject([](auto& s){s.original_production->culture->venues.front().music_timer=32;});
    reject([](auto& s){s.couriers.front().role=sim::CourierRole::OriginalMusic;});
    reject([](auto& s){s.couriers.front().owner=s.buildings.front().id;});
    reject([](auto& s){s.couriers.front().phase=sim::CourierPhase::Returning;});
    reject([](auto& s){s.couriers.front().target=s.buildings.front().id;});
    reject([](auto& s){s.couriers.front().cargo=1;});
    reject([](auto& s){s.couriers.front().reserved=1;});
    reject([](auto& s){s.couriers.front().last_dispatched_pottery=s.buildings.front().id;});
    reject([](auto& s){s.couriers.front().path.clear();});
    reject([](auto& s){s.command_sequence=UINT64_MAX;});
    reject([](auto& s){++s.treasury;});
    reject([](auto& s){s.original_economy->model_source.reset();});
    reject([](auto& s){s.original_economy->model_source->sha256=std::string(64,'0');});
    const auto source=q::state(world).schools.front().building_id,target=q::state(world).venues.front().building_id;
    q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},"Hostile-arrival baseline missing");
    const auto arrived=fixture.document(world);const auto arrived_bytes_file=fixture.root.path/"arrived38.json";
    save::write_save(arrived_bytes_file,arrived,fixture.root.data,fixture.mask());const auto arrived_bytes=f::read_text(arrived_bytes_file);
    for(int variant=0;variant<5;++variant) {
        auto bad=arrived;auto& venue=bad.world.original_production->culture->venues.front();
        if(variant==0)venue.last_arrival.reset();
        if(variant==1)++venue.music_timer;
        if(variant==2)venue.last_arrival->arrival_tick=bad.world.ticks+1;
        if(variant==3)venue.last_arrival->performer_id=static_cast<sim::PerformerId>(0);
        if(variant==4)venue.last_arrival->school_id=target;
        q::refuses([&]{(void)fixture.restore(bad);},"Duplicate/phantom/clock-shifted Venue arrival restored");
        q::refuses([&]{save::write_save(arrived_bytes_file,bad,fixture.root.data,fixture.mask());},"Forged arrival overwrote canonical save");
        check(f::read_text(arrived_bytes_file)==arrived_bytes,"Rejected actual-arrival forgery changed preserved bytes");
    }
}
void unused_channel_and_actual_population_clock() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world,false);
    const auto snapshot=world.snapshot();check(!q::school(world,s.source).latest_trip&&!world.total_population(),
        "Unspawned channel fixture unexpectedly acquired residents or a lifetime");
    for(int variant=0;variant<4;++variant) {
        auto hostile=snapshot;auto& school=hostile.original_production->culture->schools.front();
        if(variant==0)school.spawn_counter=1;
        if(variant==1)school.attempts=1;
        if(variant==2)hostile.couriers.front().route_checked_revision=hostile.road_revision;
        if(variant==3)hostile.couriers.front().reroute_attempts=1;
        q::refuses([&]{(void)fixture.restore(hostile);},"Unadmitted population or never-created actor supplied training/route history");
    }
    // This later pre-admission snapshot has sufficient source-age cycles for
    // forged training, but still has no real House admission or workers.
    q::through(world,119);auto no_people=world.snapshot();
    no_people.original_production->culture->schools.front().spawn_counter=1;
    q::refuses([&]{(void)fixture.restore(no_people);},"Prepopulation cycles trained a performer without any admitted residents");
    q::through(world,138);
    check(world.total_population()==6&&!world.workers_assigned(s.source)&&
          q::school(world,s.source).spawn_counter==0,"Six actual residents staffed an8-worker School");
    const auto file=fixture.root.path/"pre-staff38.json";const auto baseline=world.snapshot();
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    auto bad=save::read_save(file);bad.world.original_production->culture->schools.front().spawn_counter=1;
    q::refuses([&]{(void)fixture.restore(bad);},"Residents below actual model Employees trained a performer");
    q::refuses([&]{save::write_save(file,bad,fixture.root.data,fixture.mask());},
        "Pre-staff training reached atomic publication");
    check(world.snapshot()==baseline&&f::read_text(file)==bytes,
          "Rejected pre-staff training changed the live World or prior complete bytes");
}
void model_staffing_ceilings_use_actual_model_and_residents() {
    for(const bool fourth_home:{true,false}) {
        auto text=q::authored_model();
        if(!fourth_home) {
            const std::string from="211,Music  School,{,50,0,0,0,0,8,",to="211,Music  School,{,50,0,0,0,0,4,";
            const auto at=text.find(from);check(at!=std::string::npos,"Authored School Employees input missing");
            text.replace(at,from.size(),to);
        }
        q::Fixture fixture(text);auto world=fixture.world();const auto s=q::scene(world);
        if(fourth_home)(void)q::buy(world,sim::CommandType::PlaceHousehold,{119,112});
        q::until(world,[&]{return q::active(world,s.source)&&world.courier(s.channel).edge_progress>0;},
            "Normally earned additional residents or authored4-worker School did not create an actual performer");
        check(world.ticks()==291&&q::school(world,s.source).latest_trip->dispatched_tick==291&&
              world.workforce_required(s.source)==(fourth_home?8:4)&&q::school(world,s.source).spawned==1,
              "Prepared Employees or a fourth paid House was replaced by a hardcoded staffing clock");
        f::buy(world,sim::set_building_operation(s.source,false));
        const auto file=fixture.root.path/"actual-staff38.json";
        save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
        auto restored=fixture.restore(save::read_save(file));check(restored.snapshot()==world.snapshot(),
            "Legitimate model-aware early performer did not survive full restore");
        save::write_save(file,fixture.document(restored),fixture.root.data,fixture.mask());
        check(f::read_text(file)==bytes,"Legitimate early performer changed complete canonical bytes");
    }
    q::Fixture fixture;auto world=fixture.world();
    for(int x=110;x<=123;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto source=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,q::source_cell);
    const auto target=q::buy(world,sim::CommandType::PlaceOriginalTheatre,q::venue_cell);
    q::through(world,50);(void)q::buy(world,sim::CommandType::PlaceHousehold,{110,112});
    q::through(world,444);
    check(world.total_population()==14&&world.workers_assigned(source)==8&&!world.workers_assigned(target)&&
          q::school(world,source).attempts==1&&!q::school(world,source).spawned&&
          !q::school(world,source).latest_trip&&q::state(world).next_performer_id==1,
          "Four source callbacks created an actor without simultaneous actual Venue staffing");
    while(world.ticks()<1200&&!q::active(world,source))world.tick();q::invariants(world);
    check(q::active(world,source)&&world.ticks()==648&&q::school(world,source).attempts==2&&q::school(world,source).spawned==1&&
          q::school(world,source).latest_trip->dispatched_tick==648,
          "Single paid House did not earn its actual later successful spawn648");
    f::buy(world,sim::set_building_operation(source,false));const auto snapshot=world.snapshot();
    const auto file=fixture.root.path/"simultaneous-staff38.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    check(fixture.restore(save::read_save(file)).snapshot()==snapshot,"Actual later fully staffed spawn failed restore");
    auto bad=save::read_save(file);bad.world.original_production->culture->schools.front().latest_trip->dispatched_tick=444;
    q::refuses([&]{(void)fixture.restore(bad);},"Source-only training admitted a Venue without simultaneous workers");
    q::refuses([&]{save::write_save(file,bad,fixture.root.data,fixture.mask());},
        "Impossible simultaneous staffing reached atomic publication");
    check(world.snapshot()==snapshot&&f::read_text(file)==bytes,
          "Rejected simultaneous-staff forgery changed live authority or complete save bytes");
}
void aggregate_spawns_require_simultaneous_model_workforce() {
    q::Fixture fixture;auto world=fixture.world();
    for(int x=110;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto first=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,q::source_cell);
    const auto second=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,{114,115});
    f::buy(world,sim::set_building_operation(first,false));
    f::buy(world,sim::set_building_operation(second,false));
    (void)q::buy(world,sim::CommandType::PlaceOriginalTheatre,q::venue_cell);
    (void)q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    q::through(world,50);(void)q::buy(world,sim::CommandType::PlaceHousehold,{110,112});
    while(world.ticks()<1200&&world.workforce_supply()<30)world.tick();q::invariants(world);
    check(world.ticks()==730&&world.workforce_supply()==30&&world.workforce_required()==30&&
          !q::school(world,first).attempts&&!q::school(world,second).attempts&&q::state(world).next_performer_id==1,
          "Paused paid pair did not earn its actual30-worker population730");
    f::buy(world,sim::set_building_operation(first,true));
    f::buy(world,sim::set_building_operation(second,true));
    while(world.ticks()<1200&&!(q::active(world,first)&&q::active(world,second)))world.tick();q::invariants(world);
    check(world.ticks()==903&&q::active(world,first)&&q::active(world,second)&&
          q::school(world,first).latest_trip->dispatched_tick==903&&
          q::school(world,second).latest_trip->dispatched_tick==903&&world.workforce_used()==30,
          "Four actual fully staffed callbacks did not create two independent performers903");
    const auto snapshot=world.snapshot();const auto file=fixture.root.path/"aggregate-staff38.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    check(fixture.restore(save::read_save(file)).snapshot()==snapshot,"Actual30-worker pair failed full restore");
    auto bad=save::read_save(file);
    // At494 this one House can have at most18 residents: enough for either
    // individual8+7 trip, but not both distinct Schools and Venues at once.
    for(auto& school:bad.world.original_production->culture->schools)school.latest_trip->dispatched_tick=495;
    q::refuses([&]{(void)fixture.restore(bad);},"Separate per-trip worker ceilings hid an impossible aggregate spawn");
    q::refuses([&]{save::write_save(file,bad,fixture.root.data,fixture.mask());},
        "Aggregate worker forgery reached atomic publication");
    check(world.snapshot()==snapshot&&f::read_text(file)==bytes,
          "Rejected aggregate staffing changed live authority or prior complete bytes");
}
void duplicate_actual_incoming_and_source_ownership() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    f::buy(world,sim::set_building_operation(s.source,false));
    const auto second=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,{114,115});
    f::buy(world,sim::set_building_operation(second,false));
    for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    (void)q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    // Gradual immigration otherwise staffs the first School one or more
    // callbacks before the second, so its short trip can finish first. Both
    // real sources remain paused until normal residents can staff all30 jobs.
    q::until(world,[&]{return world.workforce_supply()>=world.workforce_required();},
        "Two paid Schools and Venues never earned their full workforce");
    check(world.workforce_required()==30&&q::school(world,s.source).spawn_counter==0&&
          q::school(world,second).spawn_counter==0&&!q::school(world,s.source).attempts&&
          !q::school(world,second).attempts&&q::state(world).next_performer_id==1,
          "Paused Schools acquired training, attempts or actor identities");
    f::buy(world,sim::set_building_operation(s.source,true));
    f::buy(world,sim::set_building_operation(second,true));
    check(world.workforce_used()==30&&world.building_staffed(s.source)&&world.building_staffed(second),
          "Actual30-worker population could not staff both Schools and Venues");
    while(world.ticks()<1200&&!(q::active(world,s.source)&&q::active(world,second)))world.tick();
    const auto describe=[&](sim::BuildingId id){const auto& row=q::school(world,id);
        return " School"+std::to_string(static_cast<unsigned>(id))+" assigned="+
            std::to_string(world.workers_assigned(id))+" counter="+std::to_string(row.spawn_counter)+
            " attempts="+std::to_string(row.attempts)+" spawned="+std::to_string(row.spawned)+
            " active="+std::to_string(q::active(world,id));};
    check(q::active(world,s.source)&&q::active(world,second),
          "Two paid Schools never earned simultaneous distinct physical performers: tick="+
          std::to_string(world.ticks())+" supply="+std::to_string(world.workforce_supply())+
          describe(s.source)+describe(second));
    q::invariants(world);
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),
          "Two actually admitted performers did not survive complete restore");
    auto hostile=world.snapshot();auto& schools=hostile.original_production->culture->schools;
    check(schools.size()==2&&schools[0].latest_trip->venue_id!=schools[1].latest_trip->venue_id,
          "Two actual incoming performers were not independently admitted");
    const auto target=schools[0].latest_trip->venue_id;schools[1].latest_trip->venue_id=target;
    for(auto& channel:hostile.couriers)if(channel.id==schools[1].courier_id){channel.target=target;channel.last_dispatched_target=target;}
    q::refuses([&]{sim::World::validate_original_music_venues_snapshot(hostile);},
        "Two active performers reserved one actual empty slot");
    hostile=world.snapshot();auto& duplicate=hostile.original_production->culture->schools;
    duplicate[1].latest_trip->performer_id=duplicate[0].latest_trip->performer_id;
    q::refuses([&]{(void)fixture.restore(hostile);},"One actual performer identity changed paid School ownership");
}
void accepted_arrival_cannot_be_moved_into_a_natural_fire_interval() {
    q::Fixture fixture;auto world=earned(fixture);const auto source=q::state(world).schools.front().building_id;
    q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},"Fire-provenance fixture lacks actual accepted arrival");
    check(q::school(world,source).latest_trip->accepted,"Fire-provenance baseline was not actually accepted");
    q::through(world,2600);auto bad=fixture.document(world);
    auto& culture=*bad.world.original_production->culture;culture.schools.front().latest_trip->arrival_tick=2001;
    culture.venues.front().last_arrival->arrival_tick=2001;
    culture.venues.front().music_timer=q::expected_timer(world.ticks(),2001);
    q::refuses([&]{(void)fixture.restore(bad);},"Accepted Venue arrival was forged into its natural burning interval");
    const auto file=fixture.root.path/"fire-provenance38.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    q::refuses([&]{save::write_save(file,bad,fixture.root.data,fixture.mask());},"Burning-interval receipt reached atomic publication");
    check(f::read_text(file)==bytes,"Rejected burning-interval receipt changed prior canonical bytes");
}
void one_retired_performer_cannot_witness_two_venues() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    q::until(world,[&]{return q::venue(world,s.target).last_arrival.has_value();},"Compacted witness fixture lacks first actual arrival");
    for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto other=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    q::until(world,[&]{return q::school(world,s.source).spawned==2&&q::active(world,s.source);},
        "Compacted witness fixture lacks second actually active performer");
    check(q::school(world,s.source).latest_trip->venue_id==other&&!q::venue(world,other).last_arrival,
          "Second actual trip did not retain its empty physical destination");
    f::buy(world,sim::set_building_operation(s.source,false));
    const auto file=fixture.root.path/"two-venues38.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    auto hostile=save::read_save(file);auto& rows=hostile.world.original_production->culture->venues;
    check(rows.size()==2&&rows[0].last_arrival&&rows[0].music_timer&&!rows[1].last_arrival,
          "Actual compacted arrival baseline did not preserve both distinct Venue states");
    rows[1].last_arrival=rows[0].last_arrival;rows[1].music_timer=rows[0].music_timer;
    q::refuses([&]{(void)fixture.restore(hostile);},"One retired one-way performer witnessed two Venues");
    q::refuses([&]{save::write_save(file,hostile,fixture.root.data,fixture.mask());},"Duplicate historical performer reached atomic publication");
    check(f::read_text(file)==bytes,"Rejected duplicate historical performer changed saved bytes");
}
void strict_json_source_and_generation_isolation() {
    q::Fixture fixture;auto world=earned(fixture);const auto path=fixture.root.path/"canonical.json",bad=fixture.root.path/"hostile.json";
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(path);const auto json=Json::parse(bytes);
    const auto reject=[&](auto mutate){auto hostile=json;mutate(hostile);f::write_text(bad,hostile.dump());
        q::refuses([&]{(void)save::read_save(bad);},"Hostile Schema38 JSON shape/version was accepted");
        check(f::read_text(path)==bytes,"Hostile JSON modified its canonical input");};
    reject([](auto& j){j["rules"].erase("music_venues");});
    reject([](auto& j){j["rules"]["music_venues"]="unverified";});
    reject([](auto& j){j["world"]["original_production"].erase("culture");});
    reject([](auto& j){j["world"]["original_production"]["culture"]["unsupported"]=0;});
    reject([](auto& j){j["world"]["original_production"]["culture"]["schools"][0]["trainees"]=1;});
    reject([](auto& j){j["world"]["original_production"]["culture"]["venues"][0]["house_bonus"]=96;});
    reject([](auto& j){j["schema_version"]=34;});
    reject([](auto& j){j["rules"]["version"]=18;});
    reject([](auto& j){j["world"]["couriers"][0]["role"]=28;});
    auto duplicate=bytes;const auto marker=duplicate.find("\"music_venues\":");check(marker!=std::string::npos,"Canonical marker missing");
    duplicate.insert(marker,"\"music_venues\":\"authored-music-venue-v1\",");f::write_text(bad,duplicate);
    q::refuses([&]{(void)save::read_save(bad);},"Duplicate identical Schema38 marker accepted");
    const auto document=save::read_save(path);q::refuses([&]{(void)sim::World::restore_original_deadline_food_dispatch(document.world,
        fixture.policy,fixture.model,fixture.terrain);},"Historical18 restore acquired new22 state");
    q::refuses([&]{(void)save::restore_original_safe_demolition(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Rule19 restore accepted a relabelled music generation");
    auto wrong=document;wrong.map_sha256=std::string(64,'0');
    q::refuses([&]{(void)fixture.restore(wrong);},"Missing full map fingerprint entered Rule22");
    const auto raw=fixture.raw;auto forged=raw;forged[114*228+110]=4;
    const auto context=std::make_shared<const sim::OriginalTerrainFacts>(forged,fixture.terrain->source_map_sha256());
    q::refuses([&]{(void)save::restore_original_music_venues(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,context);},
        "Prepared same-SHA terrain forgery bypassed live full-source comparison");
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt";const auto original_model=f::read_text(model_path);
    f::write_text(model_path,original_model+"\n");q::refuses([&]{(void)fixture.restore(document);},
        "Retained prepared model bypassed changed live file provenance");f::write_text(model_path,original_model);
    check(f::read_text(path)==bytes,"Source-refusal checks mutated a prior complete save");
    const auto old_model=save::prepare_original_production_rules_model(fixture.root.data);
    auto old=sim::World::create_original_deadline_food_dispatch(fixture.policy,old_model,fixture.terrain);
    const auto old_path=fixture.root.path/"historical34.json";save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());
    const auto old_bytes=f::read_text(old_path);const auto old_doc=save::read_save(old_path);
    check(!old_doc.world.original_production->culture,"Historical34 serialization acquired an empty new authority");
    q::refuses([&]{(void)fixture.restore(old_doc);},"Historical34 silently migrated to38");
    auto injected=Json::parse(old_bytes);injected["rules"]["music_venues"]=sim::original_music_venues_contract;
    injected["world"]["original_production"]["culture"]=json["world"]["original_production"]["culture"];
    f::write_text(bad,injected.dump());q::refuses([&]{(void)save::read_save(bad);},"Old schema accepted injected performer authority");
    const auto restored=save::restore_original_deadline_food_dispatch(old_doc,fixture.root.data,fixture.mask(),fixture.policy,old_model,fixture.terrain);
    save::write_save(old_path,fixture.document(restored),fixture.root.data,fixture.mask());check(f::read_text(old_path)==old_bytes,
        "Historical Rule18 complete canonical bytes changed");
}
fs::path private_path(const char* raw,bool input) {
    const auto allowed=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/oe56";
    std::error_code error;const auto path=fs::weakly_canonical(fs::absolute(raw),error);
    check(!error&&path!=allowed&&std::mismatch(allowed.begin(),allowed.end(),path.begin(),path.end()).first==allowed.end(),
        "Use a strict explicit private .local/oe56 path");
    for(auto p=fs::absolute(raw);!p.empty();p=p.parent_path()) {
        check(!fs::is_symlink(p),"Linked proof paths are forbidden");if(p==p.root_path())break;
    }
    check(input?fs::is_regular_file(path):!fs::exists(path),"Preserve existing process inputs and outputs");return path;
}
unsigned ticks(const char* raw) {
    unsigned result=0;const std::string_view text=raw;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&result<=20400,"Use bounded continuation ticks0..20400");return result;
}
int process(int argc,char** argv) {
    q::Fixture fixture;
    if((argc==3||argc==4)&&std::string_view(argv[1])=="--create") {
        const auto output=private_path(argv[2],false);auto world=earned(fixture);
        const auto count=argc==4?ticks(argv[3]):0;for(unsigned i=0;i<count;++i)world.tick();q::invariants(world);
        fs::create_directories(output.parent_path());save::write_save(output,fixture.document(world),fixture.root.data,fixture.mask());return 0;
    }
    check((argc==4||argc==5)&&std::string_view(argv[1])=="--restore","Use --create OUTPUT [TICKS] or --restore INPUT OUTPUT [TICKS]");
    const auto input=private_path(argv[2],true),output=private_path(argv[3],false);const auto bytes=f::read_text(input);
    auto world=fixture.restore(save::read_save(input));const auto count=argc==5?ticks(argv[4]):0;
    for(unsigned i=0;i<count;++i)world.tick();q::invariants(world);
    fs::create_directories(output.parent_path());save::write_save(output,fixture.document(world),fixture.root.data,fixture.mask());
    check(f::read_text(input)==bytes,"Independent restore changed its input bytes");
    if(!count)check(f::read_text(output)==bytes,"Independent complete Schema38 rewrite differs");return 0;
}
}
int main(int argc,char** argv){try{
    if(argc>1)return process(argc,argv);
    checked_model_capability();canonical_active_arrived_expired_and_recovery();
    nonzero_paid_school_desire_survives_save_and_recovery();hostile_world_and_atomic_publication();
    unused_channel_and_actual_population_clock();model_staffing_ceilings_use_actual_model_and_residents();
    aggregate_spawns_require_simultaneous_model_workforce();
    duplicate_actual_incoming_and_source_ownership();
    accepted_arrival_cannot_be_moved_into_a_natural_fire_interval();
    one_retired_performer_cannot_witness_two_venues();
    strict_json_source_and_generation_isolation();
    std::cout<<"Rule22/schema38 full active/arrived/expired Performer saves, strict source/model/hostile/Recovery and historical18 bytes PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<"Music Venue Save FAIL: "<<error.what()<<'\n';return 1;}}
