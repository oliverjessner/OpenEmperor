#include "OriginalMusicVenueFixture.h"
#include "core/PerformanceDiagnostics.h"

#include <iostream>

namespace {
namespace q=oe_music_venue_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace perf=openemperor::performance;
using q::check;

void rejected(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto canonical=world.canonical_state();
    const auto validation=world.validate(command);const auto result=world.execute(command);
    check(!validation.accepted&&!result.accepted&&!result.changed&&world.snapshot()==before&&
          world.canonical_state()==canonical,"Rejected culture command changed state, counters, IDs or Funds");
}
void pure_inspection(const sim::World& world,sim::CourierId channel) {
    const auto before=world.snapshot();const auto canonical=world.canonical_state();
    perf::set_enabled(true);perf::reset();
    for(int query=0;query<100;++query) {
        (void)world.courier_dispatch_status(channel);(void)world.courier_position(channel);
        (void)q::state(world).schools.front().latest_trip;(void)q::state(world).venues.front().music_timer;
    }
    for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,perf::Counter::WorldExecutes,
         perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::SimulationTicks,perf::Counter::FileReads,
         perf::Counter::FileWrites,perf::Counter::AssetDecodes,perf::Counter::TextureUploads})
        check(perf::counter(counter)==0,"Culture inspection copied World, changed authority or prepared routes/files/assets");
    perf::set_enabled(false);check(world.snapshot()==before&&world.canonical_state()==canonical,
        "Read-only culture inspection changed complete authority");
}
void contracts_and_complete_paid_footprint() {
    q::Fixture fixture;auto world=fixture.world();
    check(world.rule_version()==22&&world.original_music_venues_enabled()&&
          world.original_deadline_food_dispatch_enabled()&&
          !sim::original_deadline_food_dispatch_enabled(world.profile(),world.rule_version())&&
          !world.original_safe_demolition_enabled()&&
          world.original_cardinal_water_enabled()&&world.original_sustainable_economy_enabled()&&
          world.original_worker_upkeep_interval()==816&&q::state(world).next_performer_id==1&&
          q::state(world).schools.empty()&&q::state(world).venues.empty()&&
          !world.original_production_state()->safe_demolition,
          "Rule22 did not retain its separate Rule18-derived contract");
    q::refuses([&]{(void)sim::World(fixture.policy,sim::RulesProfile::CityV16,22);},
        "Ordinary constructor silently activated Rule22");
    q::refuses([&]{(void)sim::World::create_original_music_venues(fixture.policy,fixture.model,nullptr);},
        "Rule22 omitted mandatory source-bound terrain");
    const auto old_model=f::save::prepare_original_production_rules_model(fixture.root.data);
    q::refuses([&]{(void)sim::World::create_original_music_venues(fixture.policy,old_model,fixture.terrain);},
        "Historical production capability silently admitted Theatre75");
    auto old=sim::World::create_original_deadline_food_dispatch(fixture.policy,old_model,fixture.terrain);
    rejected(old,{sim::CommandType::PlaceOriginalTheatre,q::venue_cell});
    check(!old.original_production_state()->culture,"Historical18 gained culture authority");
    q::refuses([&]{(void)sim::World::restore_original_music_venues(old.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "Rule18 silently migrated into Rule22");
    const auto funds=world.treasury();const auto body=q::buy(world,sim::CommandType::PlaceOriginalTheatre,q::venue_cell);
    check(world.treasury()==funds-105&&world.workforce_required(body)==7&&
          world.building(body).kind==sim::Object::OriginalTheatre75&&
          sim::building_footprint(world.profile(),world.rule_version(),sim::Object::OriginalTheatre75)==sim::BuildingFootprint{4,4},
          "Paid Theatre changed its model price/Employees/full4x4 body");
    for(int y=0;y<4;++y)for(int x=0;x<4;++x)
        check(world.building_owner_at({q::venue_cell.x+x,q::venue_cell.y+y})==body,
              "Theatre failed complete physical body ownership");
    check(world.couriers().empty()&&!q::venue(world,body).music_timer&&!q::venue(world,body).last_arrival,
          "Theatre purchase invented performer, timer or venue service actor");
    rejected(world,{sim::CommandType::PlaceRoad,{q::venue_cell.x+3,q::venue_cell.y+3}});
    rejected(world,{sim::CommandType::PlaceHousehold,{q::venue_cell.x+3,q::venue_cell.y+3}});
    rejected(world,{sim::CommandType::PlaceOriginalTheatre,{132,132}}); // denied far interior cell135,135
    rejected(world,{sim::CommandType::PlaceOriginalTheatre,{226,226}});
    (void)q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    rejected(world,{sim::CommandType::PlaceOriginalTheatre,{140,115}});
    q::invariants(world);

    // An explicitly authored expensive School tests genuine paid scarcity in
    // bounded time. The Theatre keeps its independently checked105 price.
    auto text=q::authored_model();const std::string row="211,Music  School,{,50,";
    const auto at=text.find(row);check(at!=std::string::npos,"Authored School cost control missing");
    text.replace(at,row.size(),"211,Music  School,{,11950,");q::Fixture scarce(text);auto poor=scarce.world();
    (void)q::buy(poor,sim::CommandType::PlaceOriginalMusicSchool,q::source_cell);
    check(poor.treasury()==50,"Scarce fixture did not actually pay its prepared School price");
    rejected(poor,{sim::CommandType::PlaceOriginalTheatre,q::venue_cell});
}
void attempt_requires_actual_admission() {
    for(const bool connected:{false,true}) {
        q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world,true,connected);
        q::until(world,[&]{return q::school(world,s.source).attempts>0;},"Staffed School never made a real due attempt");
        if(connected) {
            // This branch has a Venue; independently pause it before the next
            // full period in the dedicated missing-destination case below.
            check(q::school(world,s.source).spawned>0,"Admitted reachable target failed actual creation");
        } else check(!q::school(world,s.source).spawned&&q::state(world).next_performer_id==1&&
                     !q::school(world,s.source).latest_trip,"Unreachable Venue created a performer or used an ID");
    }
    q::Fixture fixture;auto missing=fixture.world();const auto s=q::scene(missing,false);
    q::through(missing,119);
    check(q::school(missing,s.source).attempts==0&&q::school(missing,s.source).spawn_counter==0&&
          !missing.workers_assigned(s.source),"Unstaffed School attempted or spawned before actual immigration");
    q::until(missing,[&]{return q::school(missing,s.source).attempts>0;},"Missing-Venue attempt never occurred");
    check(!q::school(missing,s.source).spawned&&!q::school(missing,s.source).latest_trip&&
          q::state(missing).next_performer_id==1,"Due attempt fabricated a missing Venue/Performer");
    const auto before=q::school(missing,s.source);f::buy(missing,sim::set_building_operation(s.source,false));
    q::through(missing,missing.ticks()+204);
    check(q::school(missing,s.source)==before,"Paused School advanced its eligible spawn clock");
    const auto wrong=q::buy(missing,sim::CommandType::PlaceWarehouse,q::venue_cell);
    f::buy(missing,sim::set_building_operation(s.source,true));const auto attempts=before.attempts;
    q::until(missing,[&]{return q::school(missing,s.source).attempts>attempts;},"Wrong-target attempt never occurred");
    check(!q::school(missing,s.source).spawned&&missing.building(wrong).kind==sim::Object::Warehouse&&
          q::state(missing).next_performer_id==1,"Wrong physical kind was treated as a Venue");
}
void paid_one_way_arrival_expiry_and_fresh_identity() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    check(world.construction_spent_total()==423&&world.workforce_required(s.source)==8&&
          q::state(world).schools.size()==1&&q::state(world).venues.size()==1,
          "Normal three-House/School/Venue construction did not pay all actual prices");
    q::until(world,[&]{return q::active(world,s.source)&&world.courier(s.channel).edge_progress>0;},
        "Paid School never created a physically moving performer");
    const auto first=*q::school(world,s.source).latest_trip;
    pure_inspection(world,s.channel);
    check(first.performer_id==static_cast<sim::PerformerId>(1)&&first.venue_id==s.target&&
          !first.arrival_tick&&!first.accepted&&!q::venue(world,s.target).music_timer&&
          q::state(world).next_performer_id==2,"Dispatch granted receipt or confused channel/Performer IDs");
    check(world.original_production_state()->services.providers.empty()&&world.original_production_state()->services.trips.empty(),
          "Rule22 Music School retained the historical School-to-House adapter");
    const auto moving=world.snapshot().couriers;f::buy(world,sim::set_building_operation(s.source,false));
    check(world.snapshot().couriers==moving,"Pausing source cancelled a real begun performer edge");
    rejected(world,sim::demolish_building(s.source));rejected(world,sim::demolish_building(s.target));
    auto replay=fixture.restore(world.snapshot());
    q::until(world,[&]{return q::school(world,s.source).latest_trip->arrival_tick.has_value();},
        "Paused School performer never physically arrived");
    while(replay.ticks()<world.ticks())replay.tick();check(replay.snapshot()==world.snapshot(),"Active-performer restore diverged");
    const auto arrived=*q::school(world,s.source).latest_trip;const auto witness=q::venue(world,s.target).last_arrival;
    check(arrived.accepted&&witness&&witness->performer_id==first.performer_id&&witness->school_id==s.source&&
          witness->courier_id==s.channel&&witness->arrival_tick==arrived.arrival_tick&&
          q::venue(world,s.target).music_timer==q::expected_timer(world.ticks(),*arrived.arrival_tick)&&
          world.courier(s.channel).phase==sim::CourierPhase::IdleAtWorkshop&&world.courier(s.channel).path.empty()&&
          !world.courier_position(s.channel),
          "Arrival did not publish sole Venue32 witness and retire the one-way performer");
    pure_inspection(world,s.channel);
    const auto retired=q::school(world,s.source);
    for(int tick=0;tick<2000&&q::venue(world,s.target).music_timer;++tick) {
        world.tick();replay.tick();q::invariants(world);
        check(replay.snapshot()==world.snapshot()&&q::school(world,s.source)==retired,
              "Expiry fabricated return travel, reused performer or changed paused spawn state");
    }
    check(!q::venue(world,s.target).music_timer,"Paid paused Venue timer did not expire within32 real countdowns");
    const auto expired=world.ticks();q::through(world,expired+102);
    check(!q::venue(world,s.target).music_timer&&q::venue(world,s.target).last_arrival==witness,
          "Zero timer underflowed or lost its actual historical arrival witness");
}
void full_target_and_monotone_successful_spawns() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    q::until(world,[&]{return q::venue(world,s.target).last_arrival.has_value();},"First actual Venue arrival missing");
    const auto first=*q::venue(world,s.target).last_arrival;const auto before=q::school(world,s.source);
    q::until(world,[&]{return q::school(world,s.source).attempts>before.attempts;},"Full-Venue due attempt absent");
    check(q::venue(world,s.target).music_timer>0&&q::school(world,s.source).spawned==1&&
          q::state(world).next_performer_id==2&&q::school(world,s.source).latest_trip->performer_id==first.performer_id,
          "Nonempty Venue accepted another actual spawn or consumed an identity");
    for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto second=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    q::until(world,[&]{return q::school(world,s.source).spawned==2;},"Second actually empty staffed Venue never admitted a fresh performer");
    check(q::school(world,s.source).latest_trip->performer_id==static_cast<sim::PerformerId>(2)&&
          q::school(world,s.source).latest_trip->venue_id==second&&q::state(world).next_performer_id==3&&world.couriers().size()==1,
          "Channel reuse recycled PerformerId or created another movement channel");
    f::buy(world,sim::set_building_operation(s.source,false));
    q::until(world,[&]{return q::venue(world,second).last_arrival.has_value();},"Second performer failed physical arrival");
    check(q::venue(world,s.target).last_arrival==first,"New trip overwrote a different Venue's actual historical witness");
}
void road_cut_wait_and_paid_repair() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    q::until(world,[&]{return q::active(world,s.source)&&world.courier(s.channel).edge_progress>0;},"Road-cut performer never departed");
    const auto actor=world.courier(s.channel);const auto position=world.courier_position(s.channel);
    const auto trip=*q::school(world,s.source).latest_trip;
    f::buy(world,{sim::CommandType::RemoveRoad,q::cut_cell});
    check(world.courier(s.channel).edge_progress==actor.edge_progress&&world.courier_position(s.channel)==position&&
          q::school(world,s.source).latest_trip==trip,"Road cut teleported or erased a begun performer");
    q::through(world,world.ticks()+100);
    check(q::active(world,s.source)&&!q::venue(world,s.target).music_timer&&
          q::school(world,s.source).latest_trip->performer_id==trip.performer_id&&
          q::school(world,s.source).spawned==1,"Road disconnection delivered or duplicated a waiting performer");
    auto replay=fixture.restore(world.snapshot());const auto funds=world.treasury();
    f::buy(world,{sim::CommandType::PlaceRoad,q::cut_cell});f::buy(replay,{sim::CommandType::PlaceRoad,q::cut_cell});
    check(world.treasury()==funds-2,"Road repair was not paid");
    for(int tick=0;tick<1200&&!q::school(world,s.source).latest_trip->arrival_tick;++tick) {
        world.tick();replay.tick();check(world.snapshot()==replay.snapshot(),"Paid repair/active wait restore diverged");
    }
    check(q::school(world,s.source).latest_trip->arrival_tick.has_value(),"Paid repair never finished within its physical bound");
    q::invariants(world);check(q::school(world,s.source).latest_trip->accepted,"Paid repair never completed real admission");
}
void arrival_requires_operating_staffed_unburnt_venue() {
    for(const bool pause:{true,false}) {
        q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
        q::until(world,[&]{return q::active(world,s.source)&&world.courier(s.channel).edge_progress>0;},"Refused-arrival performer never departed");
        if(pause)f::buy(world,sim::set_building_operation(s.target,false));
        else {
            // Two paid18-worker installations fit the36 real residents at
            // departure. An18+19 pair instead skips the second whole building
            // and leaves enough workers for the7-worker Venue.
            const auto first=q::buy(world,sim::CommandType::PlaceOriginalHempFarm,{106,120});
            const auto second=q::buy(world,sim::CommandType::PlaceOriginalHempFarm,{110,120});
            f::buy(world,sim::set_building_workforce_priority(first,sim::WorkforcePriority::High));
            f::buy(world,sim::set_building_workforce_priority(second,sim::WorkforcePriority::High));
            f::buy(world,sim::set_building_workforce_priority(s.target,sim::WorkforcePriority::Low));
            check(world.workers_assigned(first)==18&&world.workers_assigned(second)==18&&
                  !world.building_staffed(s.target),
                  "Paid competing installations did not create real Venue worker shortage: supply="+
                  std::to_string(world.workforce_supply())+" first="+std::to_string(world.workers_assigned(first))+
                  " second="+std::to_string(world.workers_assigned(second))+" Venue="+
                  std::to_string(world.workers_assigned(s.target)));
        }
        q::until(world,[&]{return q::school(world,s.source).latest_trip->arrival_tick.has_value();},"Unavailable-target performer never finished its physical trip");
        check((pause?!world.building(s.target).operating_enabled:!world.building_staffed(s.target))&&
              !q::school(world,s.source).latest_trip->accepted&&!q::venue(world,s.target).music_timer&&
              !q::venue(world,s.target).last_arrival&&world.courier(s.channel).phase==sim::CourierPhase::IdleAtWorkshop,
              "Paused/unstaffed Venue received an arrival grant or retained a ghost performer");
    }
}
void original_fire_does_not_create_arrivals() {
    for(const bool source_fire:{true,false}) {
        q::Fixture fixture;auto world=fixture.world();
        for(int x=110;x<=123;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
        const auto early=q::buy(world,source_fire?sim::CommandType::PlaceOriginalMusicSchool:sim::CommandType::PlaceOriginalTheatre,
            source_fire?q::source_cell:q::venue_cell);
        q::through(world,1640);
        for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
        const auto late=q::buy(world,source_fire?sim::CommandType::PlaceOriginalTheatre:sim::CommandType::PlaceOriginalMusicSchool,
            source_fire?q::venue_cell:q::source_cell);
        const auto source=source_fire?early:late,target=source_fire?late:early;
        // Both purchase orders give the actual8-worker School priority over
        // the7-worker Venue. Immigration earns12 residents at1800 and18 at1840;
        // training therefore uses1821/1872/1923/1974 in either variant.
        f::buy(world,sim::set_building_workforce_priority(source,sim::WorkforcePriority::High));
        q::until(world,[&]{return q::active(world,source);},"Naturally staffed late fire control never spawned");
        check(world.ticks()==1974,"Fire control did not earn the four real phase35 spawn evaluations: tick="+
              std::to_string(world.ticks())+" population="+std::to_string(world.total_population())+
              " SchoolWorkers="+std::to_string(world.workers_assigned(source))+" VenueWorkers="+
              std::to_string(world.workers_assigned(target)));
        const auto channel=q::school(world,source).courier_id;
        q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},"Fire control lost actual physical arrival");
        check(world.building_on_fire(early)&&!world.building_on_fire(late)&&
              q::school(world,source).latest_trip->accepted==source_fire&&
              q::venue(world,target).last_arrival.has_value()==source_fire&&
              world.courier(channel).phase==sim::CourierPhase::IdleAtWorkshop,
              "Fire erased a begun source trip or admitted a burning Venue");
        if(source_fire) {
            const auto attempts=q::school(world,source).attempts;q::through(world,world.ticks()+102);
            check(q::school(world,source).attempts==attempts,"Burning School made another eligible spawn attempt");
        }
    }
}
void exact_arrival_before_same_tick_countdown() {
    q::Fixture fixture;auto world=fixture.world();
    for(int x=110;x<=153;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
    const auto source=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,q::source_cell);
    const auto target=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{150,115});
    const auto channel=q::school(world,source).courier_id;
    q::until(world,[&]{return q::active(world,source);},"Aligned School did not earn actual creation");
    check(world.courier(channel).path.size()==42,"Aligned paid geometry lost its forty-one physical edges");
    f::buy(world,sim::set_building_operation(source,false));
    q::until(world,[&]{return q::venue(world,target).last_arrival.has_value();},"Aligned performer did not arrive");
    check(world.ticks()>=36&&(world.ticks()-36)%51==0&&q::venue(world,target).music_timer==31,
          "Same-tick physical arrival did not precede one authored phase35 countdown");
}
void exact_arrival_before_same_tick_natural_ignition() {
    q::Fixture fixture;auto world=fixture.world();
    for(int x=110;x<=148;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto source=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,q::source_cell);
    const auto target=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{145,115});
    // First admission1600 gives6 residents, then1640 gives12. The School can
    // train only from1668, so its fourth real callback is1821.
    q::through(world,1480);
    for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
    const auto channel=q::school(world,source).courier_id;
    q::until(world,[&]{return q::active(world,source);},"Ignition-boundary School never earned actual creation");
    check(world.ticks()==1821&&world.courier(channel).path.size()==37,
          "Ignition-boundary paid geometry/training did not earn its thirty-six-edge trip");
    f::buy(world,sim::set_building_operation(source,false));
    q::through(world,1999);check(q::active(world,source)&&!q::venue(world,target).music_timer,
        "Ignition-boundary performer arrived before its actual physical time");
    world.tick();q::invariants(world);
    check(world.ticks()==2000&&q::school(world,source).latest_trip->accepted&&
          q::venue(world,target).last_arrival->arrival_tick==2000&&q::venue(world,target).music_timer==32&&
          world.building_on_fire(target),"Actual arrival at ignition tick was refused or granted a House effect");
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Valid ignition-tick arrival failed full restore");
}
}
int main(){try{
    contracts_and_complete_paid_footprint();attempt_requires_actual_admission();
    paid_one_way_arrival_expiry_and_fresh_identity();full_target_and_monotone_successful_spawns();
    road_cut_wait_and_paid_repair();arrival_requires_operating_staffed_unburnt_venue();
    original_fire_does_not_create_arrivals();exact_arrival_before_same_tick_countdown();
    exact_arrival_before_same_tick_natural_ignition();
    std::cout<<"Rule22 paid School/Performer/Venue, real spawn/retirement/arrival32/expiry, no House grant, fire/roads/refusals PASS\n";return 0;
}catch(const std::exception& error){perf::set_enabled(false);std::cerr<<"Music Venue World FAIL: "<<error.what()<<'\n';return 1;}}
