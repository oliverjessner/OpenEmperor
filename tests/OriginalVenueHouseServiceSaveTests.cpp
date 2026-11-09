#include "OriginalVenueHouseServiceFixture.h"
#include "persistence/RecoveryStore.h"

#include <charconv>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace q=oe_venue_house_fixture;
namespace old=oe_music_venue_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using q::check;

const sim::OriginalServiceTrip& trip(const sim::World& world,sim::CourierId courier) {
    for(const auto& row:world.original_production_state()->services.trips)
        if(row.transfer.courier_id==courier)return row;
    throw std::runtime_error("Paid Venue channel has no actual trip");
}
sim::OriginalServiceTrip& trip(sim::WorldSnapshot& world,sim::CourierId courier) {
    for(auto& row:world.original_production->services.trips)
        if(row.transfer.courier_id==courier)return row;
    throw std::runtime_error("Hostile control lost its real trip");
}
sim::CourierSnapshot& actor(sim::WorldSnapshot& world,sim::CourierId courier) {
    for(auto& row:world.couriers)if(row.id==courier)return row;
    throw std::runtime_error("Hostile control lost its paid channel");
}
std::uint8_t music(const sim::World& world,sim::BuildingId house) {
    for(const auto& row:world.original_economy_state()->houses)
        if(row.building_id==house)return orig::service_timer(row.house,orig::ServiceRole::Music);
    throw std::runtime_error("Actual visit target is not a House");
}
bool has_trip(const sim::World& world,sim::CourierId channel) {
    for(const auto& row:world.original_production_state()->services.trips)
        if(row.transfer.courier_id==channel)return true;
    return false;
}
sim::World earned(const q::Fixture& fixture) {
    auto world=fixture.world();const auto scene=q::scene(world);
    q::until(world,[&]{return old::venue(world,scene.target).last_arrival.has_value();},
        "Paid School did not supply a real Theatre Music input");
    f::buy(world,sim::set_building_operation(scene.source,false));
    const auto channel=q::source(world,scene.target).courier_id;
    q::until(world,[&]{return has_trip(world,channel)&&!trip(world,channel).arrival_tick&&
        world.courier(channel).edge_progress>0;},"Paid Venue did not earn a begun physical House visit");
    const auto& visit=trip(world,channel);
    check(visit.venue_walker&&visit.venue_walker->walker_id==static_cast<sim::VenueWalkerId>(1)&&
          !visit.visit_applied&&!visit.venue_walker->arrival_music&&music(world,visit.target)==0,
          "A spawn, route, or dispatch granted House Music before actual arrival");
    return world;
}
void roundtrip(const q::Fixture& fixture,const sim::World& world,const fs::path& path) {
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto parsed=save::read_save(path);const auto json=Json::parse(bytes);
    check(parsed.source_schema_version==39&&parsed.world.rule_version==23&&parsed.world==before&&
          json.at("rules").at("venue_house_service")==sim::original_venue_house_service_contract&&
          json.at("rules").at("music_venues")==sim::original_music_venues_contract&&!parsed.migrated_from_schema1,
          "Schema39 lost complete authority, exact generation, or required markers");
    auto restored=fixture.restore(parsed);check(restored.snapshot()==before,"Complete Schema39 restore changed World");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes,"Complete Schema39 F5/F9 bytes differ");
    auto expected=world;
    for(int tick=0;tick<103;++tick) {
        restored.tick();expected.tick();
        check(restored.snapshot()==expected.snapshot(),"Live versus restored complete103 continuation diverged");
    }
    q::invariants(restored);
}
void actual_active_grant_return_expiry_and_recovery() {
    q::Fixture fixture;auto world=earned(fixture);
    const auto venue=q::walkers(world).sources.front().building_id;
    const auto channel=q::source(world,venue).courier_id;
    const auto target=trip(world,channel).target;
    const auto path=fixture.root.path/"canonical39.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    for(int phase=0;phase<4;++phase) {
        if(phase==1) {
            q::until(world,[&]{return trip(world,channel).arrival_tick.has_value();},"Venue visitor never physically arrived");
            const auto& visit=trip(world,channel);
            check(visit.visit_applied&&visit.venue_walker->arrival_music&&music(world,target)>0&&
                  world.courier(channel).phase==sim::CourierPhase::Returning,
                  "Physical visit did not retain the current actual Music input and House grant");
            f::buy(world,sim::set_building_operation(venue,false));
        }
        if(phase==2) {
            q::until(world,[&]{return world.courier(channel).phase==sim::CourierPhase::IdleAtWorkshop;},
                "Actual visitor never completed its ordinary physical return");
            check(!world.courier_position(channel),"Retired Venue actor remained visible at idle channel");
            q::through(world,world.ticks()+52);
            bool retained=false;
            for(const auto& row:world.original_production_state()->housing.houses)if(row.last_evaluation)
                for(const auto& witness:row.last_evaluation->service_witnesses)
                    retained=retained||(witness&&witness->venue_walker.has_value());
            check(retained,"Actual phase39 Housing evaluation lost Venue Music provenance");
        }
        if(phase==3) {
            q::until(world,[&]{return music(world,target)==0;},"House Music did not expire after visits stopped",5100);
            check(q::venue(world,venue).music_timer==0,"Finite School input did not independently expire");
        }
        roundtrip(fixture,world,path);const auto bytes=f::read_text(path);
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==39&&entry.rule_version==23&&f::read_text(entry.path)==bytes&&
              fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
              "Recovery lost complete Schema39 visit, history, or timer bytes");
    }
    const auto catalog=recovery.catalog();check(catalog.histories.size()==1&&!catalog.truncated,
        "Recovery catalogue lost its explicit Rule23 history");
}
void begun_edge_cut_wait_and_paid_repair() {
    q::Fixture fixture;auto world=earned(fixture);
    const auto channel=q::walkers(world).sources.front().courier_id;
    const auto identity=trip(world,channel).venue_walker;
    f::buy(world,{sim::CommandType::RemoveRoad,old::cut_cell});
    const auto before=world.courier(channel);check(before.edge_progress>0,"Cut control lost its begun edge");
    q::until(world,[&]{return world.courier(channel).route_pending&&world.courier(channel).edge_progress==0;},
        "Disconnected real Venue visitor did not wait at an ordinary physical vertex",100);
    check(!trip(world,channel).arrival_tick&&trip(world,channel).venue_walker==identity&&
          music(world,trip(world,channel).target)==0,"Road cut lost identity or granted a planned visit");
    roundtrip(fixture,world,fixture.root.path/"cut-wait39.json");
    const auto spent=world.construction_spent_total();f::buy(world,{sim::CommandType::PlaceRoad,old::cut_cell});
    check(world.construction_spent_total()>spent,"Road repair was not actually paid");
    q::until(world,[&]{return trip(world,channel).arrival_tick.has_value();},"Paid road repair never enabled real arrival");
    check(trip(world,channel).visit_applied&&trip(world,channel).venue_walker->walker_id==identity->walker_id,
          "Repair replaced the actual Walker identity or failed its real House grant");
    roundtrip(fixture,world,fixture.root.path/"repaired39.json");
}
template<class Edit>void hostile(const q::Fixture& fixture,const sim::World& world,const fs::path& target,
                                Edit edit,const std::string& reason) {
    const auto snapshot=world.snapshot();save::write_save(target,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(target);auto invalid=fixture.document(world);edit(invalid.world);
    q::refuses([&]{(void)fixture.restore(invalid);},reason+" entered full restore");
    q::refuses([&]{save::write_save(target,invalid,fixture.root.data,fixture.mask());},reason+" reached atomic publication");
    check(world.snapshot()==snapshot&&f::read_text(target)==bytes,reason+" changed prior World or canonical bytes");
}
void hostile_ids_music_clocks_geometry_and_embedded_history() {
    q::Fixture fixture;auto world=earned(fixture);
    const auto channel=q::walkers(world).sources.front().courier_id;
    const auto venue=q::walkers(world).sources.front().building_id;
    const auto path=fixture.root.path/"atomic39.json";
    const auto bad=[&](auto edit,const std::string& reason){hostile(fixture,world,path,edit,reason);};
    bad([](auto& s){s.original_production->culture->venue_house_service.reset();},"Missing mandatory Venue authority");
    bad([](auto& s){++s.original_production->culture->venue_house_service->next_walker_id;},"Phantom Walker highwater");
    bad([](auto& s){auto& rows=s.original_production->culture->venue_house_service->sources;rows.push_back(rows.front());},"Duplicate source/channel");
    bad([](auto& s){s.original_production->culture->venue_house_service->sources.front().spawn_counter=4;},"Out-of-range spawn counter");
    bad([](auto& s){s.original_production->culture->venue_house_service->sources.front().attempts=UINT64_MAX;},"Impossible attempt clock");
    bad([](auto& s){s.original_production->culture->venue_house_service->sources.front().first_music.reset();},"Missing first actual Music input");
    bad([](auto& s){++s.original_production->culture->venue_house_service->sources.front().first_music->arrival_tick;},"Counterfeit first Music arrival clock");
    bad([&](auto& s){s.original_production->culture->venue_house_service->sources.front().first_music->school_id=venue;},"Foreign first Music source");
    bad([&](auto& s){trip(s,channel).venue_walker.reset();},"Real actor without identity evidence");
    bad([&](auto& s){trip(s,channel).venue_walker->walker_id=static_cast<sim::VenueWalkerId>(0);},"Zero Walker ID");
    bad([&](auto& s){auto& t=trip(s,channel);t.dispatched_tick-=51;t.venue_walker->dispatched_tick-=51;},"Spawn predating four eligible calls");
    bad([&](auto& s){trip(s,channel).venue_walker->dispatch_music.arrival_tick=world.ticks()+1;},"Future received Music");
    bad([&](auto& s){trip(s,channel).venue_walker->dispatch_music.school_id=venue;},"Foreign Performer source");
    bad([&](auto& s){trip(s,channel).venue_walker->dispatch_music.performer_id=
        static_cast<sim::PerformerId>(s.original_production->culture->next_performer_id);},"Uncreated Performer input");
    bad([&](auto& s){auto& t=trip(s,channel);t.venue_walker->arrival_music=t.venue_walker->dispatch_music;},"Arrival Music before actual visit");
    bad([&](auto& s){trip(s,channel).visit_applied=true;},"Dispatch-only House effect");
    bad([&](auto& s){actor(s,channel).cargo=1;},"Material cargo on service actor");
    bad([&](auto& s){actor(s,channel).edge_progress=5;},"Counterfeit physical edge progress");
    bad([&](auto& s){actor(s,channel).owner=s.original_production->culture->schools.front().building_id;},"Foreign movement owner");
    bad([&](auto& s){trip(s,channel).target=venue;},"Non-House target");
    bad([&](auto& s){s.original_production->services.providers.clear();},"Orphaned Provider");
    bad([&](auto& s){s.original_production->services.trips.clear();},"Used channel without lifetime trip");
    q::until(world,[&]{return trip(world,channel).visit_applied;},"Hostile witness control never earned real grant");
    f::buy(world,sim::set_building_operation(venue,false));q::through(world,world.ticks()+52);
    bad([&](auto& s){trip(s,channel).venue_walker->arrival_music.reset();},"Applied visit without arrival Music");
    bad([](auto& s){s.original_production->services.witnesses.front().venue_walker.reset();},"House grant without Walker witness");
    bad([](auto& s){auto& w=s.original_production->services.witnesses.front();
        w.venue_walker->walker_id=static_cast<sim::VenueWalkerId>(99);},"Phantom House grant identity");
    bool edited=false;
    bad([&](auto& s){for(auto& h:s.original_production->housing.houses)if(h.last_evaluation)
        for(auto& w:h.last_evaluation->service_witnesses)if(w&&w->venue_walker){
            w->venue_walker->dispatch_music.arrival_tick=world.ticks()+1;edited=true;return;}},
        "Forged embedded Housing evaluation Music");
    check(edited,"Embedded Housing hostile test did not modify a real retained visit");
}
template<class Action>void refuses_for(Action action,const std::string& fragment) {
    try{action();}catch(const std::exception& error) {
        check(std::string(error.what()).find(fragment)!=std::string::npos,
              "Hostile control hit another guard: "+std::string(error.what()));return;
    }
    throw std::runtime_error("Hostile control did not reach expected refusal: "+fragment);
}
void rewrite_retained_music(sim::WorldSnapshot& state,sim::PerformerId id,std::uint64_t arrival) {
    const auto rewrite=[&](sim::OriginalMusicArrival& m){if(m.performer_id==id)m.arrival_tick=arrival;};
    for(auto& source:state.original_production->culture->venue_house_service->sources)
        if(source.first_music)rewrite(*source.first_music);
    for(auto& venue:state.original_production->culture->venues)if(venue.last_arrival)rewrite(*venue.last_arrival);
    const auto evidence=[&](std::optional<sim::OriginalVenueWalkerEvidence>& e){if(e) {
        rewrite(e->dispatch_music);if(e->arrival_music)rewrite(*e->arrival_music);
    }};
    for(auto& t:state.original_production->services.trips)evidence(t.venue_walker);
    for(auto& w:state.original_production->services.witnesses)evidence(w.venue_walker);
    for(auto& h:state.original_production->housing.houses) {
        const auto evaluation=[&](sim::OriginalHousingEvaluation& e){for(auto& w:e.service_witnesses)if(w)evidence(w->venue_walker);};
        if(h.last_evaluation)evaluation(*h.last_evaluation);
        for(auto& t:h.first_transitions)if(t)evaluation(t->evaluation);
    }
}
void compacted_music_still_requires_physical_travel_and_fire_history() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::scene(world);
    const auto channel=q::source(world,scene.target).courier_id;
    q::until(world,[&]{return q::source(world,scene.target).first_music.has_value();},"Compaction control lacks first real Music input");
    const auto first=*q::source(world,scene.target).first_music;
    // Keep the real School active across ordinary expiry and the natural
    // 2000..2599 incident. Its next actual accepted input must replace this
    // one; no timer, resident, staff or fire state is injected.
    while(world.ticks()<4000&&q::venue(world,scene.target).last_arrival->performer_id==first.performer_id)world.tick();
    check(q::venue(world,scene.target).last_arrival->performer_id!=first.performer_id&&
          q::school(world,scene.source).latest_trip->performer_id!=first.performer_id,
          "Paid School never earned a later actual input within bounded4000 ticks");
    f::buy(world,sim::set_building_operation(scene.source,false));
    q::until(world,[&]{return has_trip(world,channel)&&trip(world,channel).visit_applied&&
        trip(world,channel).venue_walker->arrival_music->performer_id!=first.performer_id;},
        "Compaction control did not earn an actual House grant from its later Music input");
    f::buy(world,sim::set_building_operation(scene.target,false));q::invariants(world);
    const auto path=fixture.root.path/"compacted39.json";roundtrip(fixture,world,path);
    const auto snapshot=world.snapshot();const auto bytes=f::read_text(path);
    auto impossible=fixture.document(world);
    rewrite_retained_music(impossible.world,first.performer_id,first.dispatched_tick+1);
    refuses_for([&]{(void)fixture.restore(impossible);},"Venue walker Music has no possible actual paid Performer arrival");
    refuses_for([&]{save::write_save(path,impossible,fixture.root.data,fixture.mask());},
        "Venue walker Music has no possible actual paid Performer arrival");
    check(world.snapshot()==snapshot&&f::read_text(path)==bytes,"Compacted travel refusal changed live World or canonical bytes");
    // These isolated fire-authority controls deliberately mutate historical
    // event clocks. The untouched positive snapshot passed full restore above;
    // the shared fire validator must itself see every new retained location,
    // independent of an earlier syntax/identity/phase rejection elsewhere.
    sim::World::validate_original_fire_protection_snapshot(snapshot);
    const auto fire_refusal=[&](auto edit,const std::string& reason) {
        auto invalid=fixture.document(world);edit(invalid.world);
        refuses_for([&]{sim::World::validate_original_fire_protection_snapshot(invalid.world);},reason);
        q::refuses([&]{(void)fixture.restore(invalid);},"Burning retained event entered full restore");
        q::refuses([&]{save::write_save(path,invalid,fixture.root.data,fixture.mask());},"Burning retained event reached atomic publication");
        check(world.snapshot()==snapshot&&f::read_text(path)==bytes,"Fire-history refusal changed live or canonical state");
    };
    fire_refusal([](auto& s){s.original_production->culture->venue_house_service->sources.front().first_music->arrival_tick=2001;},
        "accepted performer arrived during actual pre-movement venue fire");
    fire_refusal([&](auto& s){trip(s,channel).venue_walker->dispatched_tick=2001;},
        "venue walker dispatched from or toward an actually burning body");
    fire_refusal([&](auto& s){trip(s,channel).arrival_tick=2001;},
        "House Music was applied during actual pre-movement fire");
    fire_refusal([](auto& s){s.original_production->services.witnesses.front().venue_walker->dispatched_tick=2001;},
        "venue walker dispatched from or toward an actually burning body");
}
void each_new_lifetime_requires_four_idle_callbacks_after_real_return() {
    q::Fixture fixture;auto world=fixture.world();
    for(int x=110;x<=153;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
    (void)q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,old::source_cell);
    const auto venue=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{150,115});
    const auto channel=q::source(world,venue).courier_id;
    q::until(world,[&]{return q::source(world,venue).spawned==1;},"Long physical return fixture never earned first actor");
    const auto path=fixture.root.path/"lifetime39.json";
    hostile(fixture,world,path,[](auto& s){s.original_production->culture->venue_house_service->sources.front().spawn_counter=1;},
        "Moving actor accumulated idle training");
    q::until(world,[&]{return world.courier(channel).phase==sim::CourierPhase::IdleAtWorkshop;},"Long real actor never returned");
    const auto returned=world.ticks();const auto previous=trip(world,channel);
    check(previous.visit_applied&&previous.arrival_tick&&q::source(world,venue).spawn_counter==0,
          "Returned lifetime lacks a real grant or gained a same-tick idle callback");
    hostile(fixture,world,path,[](auto& s){s.original_production->culture->venue_house_service->sources.front().spawn_counter=1;},
        "Idle counter predates first callback after real return");
    q::until(world,[&]{return q::source(world,venue).spawned==2;},"Second long-trip lifetime never earned four fresh callbacks");
    check(trip(world,channel).target!=previous.target&&trip(world,channel).transfer.sequence==2,
          "Paid repeated fixture did not retain two distinct actual House trips");
    roundtrip(fixture,world,path);
    // The second eligible phase after this OBSERVED physical return is too
    // early, even when the aggregate age since first Music permits8 calls.
    const auto early=36+51*old::countdowns(returned)+51;
    check(early<trip(world,channel).dispatched_tick&&old::countdowns(early)-old::countdowns(returned)==2&&
          old::countdowns(early)-old::countdowns(q::source(world,venue).first_music->arrival_tick)>=8,
          "Lifetime hostile was not independent of aggregate-age training bounds");
    hostile(fixture,world,path,[&](auto& s){auto& t=trip(s,channel);t.dispatched_tick=early;t.venue_walker->dispatched_tick=early;},
        "Next lifetime before four fresh post-return callbacks");
}
void two_paid_sources_keep_identity_order_and_share_actual_workforce() {
    auto model=old::authored_model();const std::string row="75,Theatre Pavilion,{,105,0,0,0,0,7,";
    const auto at=model.find(row);check(at!=std::string::npos,"Authored Venue model row missing");
    // Authored worker demand exercises generic checked model authority. This
    //20-worker row is not the supplied Original75/7 observation.
    model.replace(at,row.size(),"75,Theatre Pavilion,{,105,0,0,0,0,20,");
    // The restore ceiling intentionally considers every possible evolved
    // House row, including the fixture's otherwise unreachable later Common
    // capacities41/52/63/74. Bound those INPUT rows too, before preparation.
    for(std::size_t house_row=4;house_row<=7;++house_row)oe_production_fixture::set_house_field(model,house_row,17,37);
    q::Fixture fixture(model);
    for(std::size_t house_row=0;house_row<orig::house_row_count;++house_row)
        check(fixture.model->house_rules().row_capacity(static_cast<orig::ModelHouse>(house_row))<=37,
              "Shared-workforce authored model has a possible House capacity above37");
    auto world=fixture.world();
    for(int x=110;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto home=q::buy(world,sim::CommandType::PlaceHousehold,{110,112});
    const auto school=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,old::source_cell);
    // Farther first means its earlier-ID dispatch can be overtaken by the
    // nearer real visitor under the existing deadline-aware selector.
    const auto far=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    const auto near=q::buy(world,sim::CommandType::PlaceOriginalTheatre,old::venue_cell);
    f::buy(world,sim::set_building_operation(near,false));
    q::until(world,[&]{return q::source(world,far).first_music.has_value();},"Paid far Venue never received its actual input");
    f::buy(world,sim::set_building_operation(far,false));f::buy(world,sim::set_building_operation(near,true));
    q::until(world,[&]{return q::source(world,near).first_music.has_value();},"Paid near Venue never received its actual input");
    f::buy(world,sim::set_building_operation(near,false));f::buy(world,sim::set_building_operation(school,false));
    check(q::source(world,far).spawn_counter==0&&q::source(world,near).spawn_counter==0&&
          !q::source(world,far).spawned&&!q::source(world,near).spawned,"Input arrival itself trained or created actors");
    q::through(world,1400);(void)q::buy(world,sim::CommandType::PlaceHousehold,{113,112});
    q::until(world,[&]{return world.total_population()>=40;},"Second paid House never earned shared40-worker capacity",300);
    f::buy(world,sim::set_building_operation(far,true));f::buy(world,sim::set_building_operation(near,true));
    q::until(world,[&]{return q::source(world,far).spawned==1&&q::source(world,near).spawned==1;},
        "Two staffed paid Venues never created distinct simultaneous physical actors",300);
    const auto a=q::source(world,far).courier_id,b=q::source(world,near).courier_id;
    const auto first=trip(world,a),second=trip(world,b);
    check(a<b&&first.dispatched_tick==second.dispatched_tick&&first.target==home&&second.target==home&&
          first.venue_walker->walker_id!=second.venue_walker->walker_id&&
          world.courier(a).edge_progress>0&&world.courier(b).edge_progress>0,
          "Two-source control lacks simultaneous distinct IDs and an already occupied physical target");
    const auto path=fixture.root.path/"two-source39.json";roundtrip(fixture,world,path);
    hostile(fixture,world,path,[&](auto& s){std::swap(trip(s,a).venue_walker->walker_id,trip(s,b).venue_walker->walker_id);},
        "Actor IDs contradict stable same-tick source order");
    hostile(fixture,world,path,[&](auto& s){trip(s,b).venue_walker->walker_id=trip(s,a).venue_walker->walker_id;},
        "One actual actor identity claimed by two paid sources");
    hostile(fixture,world,path,[&](auto& s){trip(s,b).transfer.sequence=2;},"Uncreated per-source lifetime sequence");
    hostile(fixture,world,path,[](auto& s){auto& rows=s.original_production->culture->venue_house_service->sources;
        std::swap(rows[0].courier_id,rows[1].courier_id);},"Cross-owned Venue movement channels");
    auto invalid=fixture.document(world);constexpr std::uint64_t early=1362;
    check(old::countdowns(early)-old::countdowns(q::source(world,near).first_music->arrival_tick)>=4&&
          early<1400&&world.total_population()>=40,"Aggregate hostile did not isolate the later paid population");
    for(const auto channel:{a,b}) {auto& t=trip(invalid.world,channel);t.dispatched_tick=early;t.venue_walker->dispatched_tick=early;}
    // Syntax, IDs, geometry, individual training and physical lower bounds
    // remain valid. Only shared prepared-model population can prove that a
    // single37-capacity House cannot staff two20-worker Venues at1362.
    sim::World::validate_original_production_snapshot(invalid.world);
    const auto before=world.snapshot();const auto bytes=f::read_text(path);
    refuses_for([&]{(void)fixture.restore(invalid);},"Simultaneous actual culture spawns exceed possible shared model workforce");
    refuses_for([&]{save::write_save(path,invalid,fixture.root.data,fixture.mask());},
        "Simultaneous actual culture spawns exceed possible shared model workforce");
    check(world.snapshot()==before&&f::read_text(path)==bytes,"Shared-workforce refusal changed paid World or canonical bytes");
}
void same_tick_music_input_cannot_follow_its_house_callback() {
    for(const bool school_first:{true,false}) {
        q::Fixture fixture;auto world=fixture.world();
        for(int x=110;x<=123;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
        for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
        if(school_first)(void)q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,old::source_cell);
        const auto venue=q::buy(world,sim::CommandType::PlaceOriginalTheatre,old::venue_cell);
        if(!school_first)(void)q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,old::source_cell);
        const auto channel=q::source(world,venue).courier_id;
        q::until(world,[&]{return has_trip(world,channel)&&trip(world,channel).visit_applied;},
            "Courier-order control did not earn an actual paid Music House visit");
        f::buy(world,sim::set_building_operation(venue,false));
        const auto visit=trip(world,channel);const auto performer=visit.venue_walker->arrival_music->courier_id;
        check((performer<channel)==school_first&&visit.venue_walker->arrival_music->arrival_tick<*visit.arrival_tick,
              "Paid source order did not earn the intended actual channel order and earlier Music input");
        const auto path=fixture.root.path/"arrival-order39.json";roundtrip(fixture,world,path);
        const auto snapshot=world.snapshot();const auto bytes=f::read_text(path);auto invalid=fixture.document(world);
        // An explicitly HOSTILE retained clock, not an invented positive
        // simultaneous movement. Keep the actual paid source, performer,
        // channel and Model identities. Every copy of this Walker's arrival
        // observation receives the same forged timestamp.
        const auto rewrite=[&](std::optional<sim::OriginalVenueWalkerEvidence>& e) {
            if(e&&e->walker_id==visit.venue_walker->walker_id&&e->arrival_music)
                e->arrival_music->arrival_tick=*visit.arrival_tick;
        };
        for(auto& t:invalid.world.original_production->services.trips)rewrite(t.venue_walker);
        for(auto& w:invalid.world.original_production->services.witnesses)rewrite(w.venue_walker);
        for(auto& h:invalid.world.original_production->housing.houses) {
            const auto evaluation=[&](sim::OriginalHousingEvaluation& e){for(auto& w:e.service_witnesses)if(w)rewrite(w->venue_walker);};
            if(h.last_evaluation)evaluation(*h.last_evaluation);
            for(auto& t:h.first_transitions)if(t)evaluation(t->evaluation);
        }
        if(school_first) {
            // A lower-ID Performer is eligible to run first in this movement
            // phase. The new order guard must allow it through; the separate
            // unchanged receipt guard still rejects this tampered history.
            refuses_for([&]{sim::World::validate_original_venue_house_service_snapshot(invalid.world);},
                "Venue walker Music has no possible actual paid Performer arrival");
        } else {
            refuses_for([&]{(void)fixture.restore(invalid);},"Venue House grant precedes its same-tick Performer input");
            refuses_for([&]{save::write_save(path,invalid,fixture.root.data,fixture.mask());},
                "Venue House grant precedes its same-tick Performer input");
        }
        check(world.snapshot()==snapshot&&f::read_text(path)==bytes,"Courier-order refusal changed paid World or prior bytes");
    }
}
void strict_json_generation_source_and_atomic_inputs() {
    q::Fixture fixture;auto world=earned(fixture);const auto good=fixture.root.path/"good39.json",bad=fixture.root.path/"bad39.json";
    save::write_save(good,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(good);const auto canonical=Json::parse(bytes);
    const auto rejects=[&](Json candidate,const std::string& reason) {
        f::write_text(bad,candidate.dump());q::refuses([&]{(void)save::read_save(bad);},reason);
        check(f::read_text(good)==bytes,"Rejected JSON changed existing canonical39 bytes");
    };
    for(const auto key:{"venue_house_service","music_venues","deadline_food_dispatch","cardinal_water"}) {
        auto candidate=canonical;candidate["rules"].erase(key);rejects(candidate,"Missing required Rule23 marker accepted");
    }
    auto candidate=canonical;candidate["rules"]["venue_house_service"]="unknown";rejects(candidate,"Unknown Rule23 contract accepted");
    candidate=canonical;candidate["rules"]["version"]=22;rejects(candidate,"Schema39 relabelled as Rule22");
    candidate=canonical;candidate["schema_version"]=38;rejects(candidate,"Rule23 relabelled as Schema38");
    candidate=canonical;candidate["world"]["original_production"]["culture"].erase("venue_house_service");rejects(candidate,"Missing Walker authority accepted");
    candidate=canonical;candidate["world"]["original_production"]["culture"]["venue_house_service"]["extra"]=1;rejects(candidate,"Unknown Walker authority key accepted");
    candidate=canonical;candidate["world"]["original_production"]["culture"]["venue_house_service"]["sources"][0].erase("first_music");rejects(candidate,"Missing first Music field accepted");
    candidate=canonical;candidate["world"]["original_production"]["services"]["trips"][0]["venue_walker"]["extra"]=1;rejects(candidate,"Unknown trip Evidence key accepted");
    candidate=canonical;candidate["world"]["original_production"]["services"]["trips"][0]["venue_walker"]["walker_id"]=-1;rejects(candidate,"Negative Walker ID accepted");
    candidate=canonical;candidate["world"]["original_production"]["culture"]["venue_house_service"]["sources"][0]["spawn_counter"]=256;rejects(candidate,"Wrapped Walker counter accepted");
    auto duplicate=bytes;const auto opening=duplicate.find('{');duplicate.insert(opening+1,"\"schema_version\":39,");
    f::write_text(bad,duplicate);q::refuses([&]{(void)save::read_save(bad);},"Duplicate Schema39 key accepted");
    auto document=save::read_save(good);const auto snapshot=world.snapshot();
    auto invalid=document;invalid.map_sha256=std::string(64,'0');q::refuses([&]{(void)fixture.restore(invalid);},"Foreign map SHA accepted");
    invalid=document;invalid.map_permissions_sha256=std::string(64,'0');q::refuses([&]{(void)fixture.restore(invalid);},"Foreign policy accepted");
    invalid=document;invalid.world.original_economy->model_source->sha256=std::string(64,'0');q::refuses([&]{(void)fixture.restore(invalid);},"Foreign model accepted");
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt";const auto model=f::read_text(model_path);
    f::write_text(model_path,model+"\n");q::refuses([&]{(void)fixture.restore(document);},"Changed live model accepted despite prepared model");f::write_text(model_path,model);
    const auto map_path=fixture.root.data/f::map_relative;const auto map=f::read_text(map_path);
    f::write_text(map_path,map+"\n");q::refuses([&]{(void)fixture.restore(document);},"Changed live map accepted despite prepared terrain");f::write_text(map_path,map);
    check(world.snapshot()==snapshot&&f::read_text(good)==bytes,"Rejected foreign inputs changed paid World or SaveDocument");
    const auto old_path=fixture.root.path/"old38.json";auto old_world=static_cast<const old::Fixture&>(fixture).world();
    save::write_save(old_path,fixture.document(old_world),fixture.root.data,fixture.mask());const auto old_bytes=f::read_text(old_path);
    const auto old_doc=save::read_save(old_path);auto old_json=Json::parse(old_bytes);
    check(old_doc.source_schema_version==38&&!old_json["rules"].contains("venue_house_service")&&
          !old_json["world"]["original_production"]["culture"].contains("venue_house_service"),"Schema38 gained new empty authority");
    q::refuses([&]{(void)fixture.restore(old_doc);},"Old38 silently migrated to39");
    auto old_restored=save::restore_original_music_venues(old_doc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);
    save::write_save(old_path,fixture.document(old_restored),fixture.root.data,fixture.mask());check(f::read_text(old_path)==old_bytes,"Historical38 canonical bytes changed");
    old_json["rules"]["venue_house_service"]=sim::original_venue_house_service_contract;
    old_json["world"]["original_production"]["culture"]["venue_house_service"]=canonical["world"]["original_production"]["culture"]["venue_house_service"];
    rejects(old_json,"Historical38 accepted injected Walker authority");
    q::refuses([&]{(void)save::restore_original_music_venues(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Old38 explicit restore accepted Rule23");
}
fs::path private_path(const char* raw,bool input) {
    const auto allowed=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/oe57";
    const auto absolute=fs::absolute(raw).lexically_normal();
    for(auto p=absolute;!p.empty();p=p.parent_path()) {
        check(!fs::is_symlink(p),"Linked proof paths are forbidden");if(p==p.root_path())break;
    }
    std::error_code error;const auto path=fs::weakly_canonical(absolute,error);
    check(!error&&path!=allowed&&std::mismatch(allowed.begin(),allowed.end(),path.begin(),path.end()).first==allowed.end(),
        "Use a strict explicit private .local/oe57 path");
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
    check(f::read_text(input)==bytes,"Independent process changed its source SaveDocument");
    if(!count)check(f::read_text(output)==bytes,"Independent complete Schema39 zero-tick rewrite differs");return 0;
}
}
int main(int argc,char** argv){try{
    if(argc>1)return process(argc,argv);
    actual_active_grant_return_expiry_and_recovery();begun_edge_cut_wait_and_paid_repair();
    hostile_ids_music_clocks_geometry_and_embedded_history();compacted_music_still_requires_physical_travel_and_fire_history();
    each_new_lifetime_requires_four_idle_callbacks_after_real_return();two_paid_sources_keep_identity_order_and_share_actual_workforce();
    same_tick_music_input_cannot_follow_its_house_callback();
    strict_json_generation_source_and_atomic_inputs();
    std::cout<<"Rule23/schema39 full active/granted/returned/expired saves, physical cut/repair, strict hostile/source/Recovery and historical38 bytes PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<"Venue House Service Save FAIL: "<<error.what()<<'\n';return 1;}}
