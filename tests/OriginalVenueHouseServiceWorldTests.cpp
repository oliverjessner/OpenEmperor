#include "OriginalVenueHouseServiceFixture.h"
#include "core/PerformanceDiagnostics.h"

#include <iostream>

namespace {
namespace q=oe_venue_house_fixture;
namespace old=oe_music_venue_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace perf=openemperor::performance;
using q::check;

const sim::OriginalServiceTrip& trip(const sim::World& world,sim::BuildingId venue) {
    const auto channel=q::source(world,venue).courier_id;
    for(const auto& t:world.original_production_state()->services.trips) if(t.transfer.courier_id==channel) return t;
    throw std::runtime_error("No actual venue walker trip");
}
std::uint8_t music(const sim::World& world,sim::BuildingId home) {
    for(const auto& h:world.original_economy_state()->houses) if(h.building_id==home)
        return orig::service_timer(h.house,orig::ServiceRole::Music);
    throw std::runtime_error("Missing actual House");
}
void no_music(const sim::World& world) {
    for(const auto& h:world.original_economy_state()->houses)
        check(!orig::service_timer(h.house,orig::ServiceRole::Music),"Non-arrival granted House Music");
}
void rejected(sim::World& world,const sim::Command& command) {
    const auto before=world.snapshot();const auto key=world.canonical_state();
    check(!world.validate(command).accepted,"Invalid venue command validated");
    const auto result=world.execute(command);
    check(!result.accepted&&!result.changed&&world.snapshot()==before&&world.canonical_state()==key,
          "Refusal changed authority, IDs or Funds");
}
void pure(const sim::World& world,sim::CourierId channel,sim::BuildingId home) {
    const auto before=world.snapshot();const auto key=world.canonical_state();
    perf::set_enabled(true);perf::reset();
    for(int i=0;i<100;++i) {
        (void)world.original_service_availability(home,orig::ServiceRole::Music);
        (void)world.courier_dispatch_status(channel);(void)world.courier_position(channel);
    }
    for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,perf::Counter::WorldExecutes,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::SimulationTicks,
        perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::AssetDecodes,perf::Counter::TextureUploads})
        check(!perf::counter(counter),"Venue inspection performed authority, route or asset work");
    perf::set_enabled(false);check(world.snapshot()==before&&world.canonical_state()==key,"Inspection changed venue authority");
}
void contracts_and_no_phantom_service() {
    q::Fixture fixture;auto world=fixture.world();
    check(world.rule_version()==23&&world.active_rules().version==23&&world.original_venue_house_service_enabled()&&
          world.original_music_venues_enabled()&&world.original_deadline_food_dispatch_enabled()&&
          world.original_worker_upkeep_interval()==816&&!world.original_safe_demolition_enabled()&&
          sim::current_rule_version(sim::RulesProfile::CityV16)==4,
          "Rule23 changed its inherited clock, capability, default or retirement policy");
    q::refuses([&]{(void)sim::World(fixture.policy,sim::RulesProfile::CityV16,23);},"Ordinary constructor admitted Rule23");
    q::refuses([&]{(void)sim::World::create_original_venue_house_service(fixture.policy,fixture.model,nullptr);},"Missing terrain accepted");
    auto legacy=static_cast<const old::Fixture&>(fixture).world();
    q::refuses([&]{(void)fixture.restore(legacy.snapshot());},"Rule22 migrated to23");
    q::refuses([&]{(void)sim::World::restore_original_music_venues(world.snapshot(),fixture.policy,fixture.model,fixture.terrain);},"Rule23 migrated through22 restore");
    const auto s=q::scene(world);const auto channel=q::source(world,s.target).courier_id;
    check(world.construction_spent_total()==423&&world.couriers().size()==2&&
          q::walkers(world).next_walker_id==1&&!world.courier_position(channel)&&
          world.original_production_state()->services.providers.size()==1,
          "Paid Theatre created an actor, raised cost or lost its one bounded channel");
    no_music(world);pure(world,channel,s.homes[0]);q::through(world,119);
    check(!q::source(world,s.target).attempts&&!q::source(world,s.target).spawned,"Unstaffed source attempted a venue spawn");
    no_music(world);
    q::until(world,[&]{return q::venue(world,s.target).last_arrival.has_value();},"School never physically supplied Theatre");
    check(!q::source(world,s.target).spawned,"Performer arrival itself created a venue walker");no_music(world);
    rejected(world,sim::demolish_building(s.target));
    q::until(world,[&]{return q::source(world,s.target).spawned==1;},"Received Music did not earn an actual walker spawn");
    const auto first=trip(world,s.target);const auto moving=world.courier(channel);
    check(first.venue_walker&&static_cast<std::uint64_t>(first.venue_walker->walker_id)==1&&
          first.venue_walker->dispatch_music==*q::venue(world,s.target).last_arrival&&
          !first.arrival_tick&&!first.visit_applied&&!first.venue_walker->arrival_music&&
          moving.phase==sim::CourierPhase::ToWarehouse&&moving.edge_progress>0&&world.courier_position(channel),
          "Spawn confused fixed channel, lifetime identity, route or actual House arrival");
    no_music(world);pure(world,channel,first.target);
    auto replay=fixture.restore(world.snapshot());
    while(!trip(world,s.target).arrival_tick) {
        world.tick();replay.tick();check(replay.snapshot()==world.snapshot(),"Active venue restore diverged");
    }
    const auto arrived=trip(world,s.target);
    check(arrived.visit_applied&&arrived.venue_walker->arrival_music&&
          world.courier(channel).phase==sim::CourierPhase::Returning&&
          music(world,arrived.target)==96-(old::countdowns(world.ticks())-old::countdowns(*arrived.arrival_tick-1)),
          "Actual current-Music arrival failed sole House96 assignment");
    const auto status=world.original_service_availability(arrived.target,orig::ServiceRole::Music);
    check(status.last_arrival_tick==arrived.arrival_tick&&status.delivered_timer==music(world,arrived.target),"House inspection lacks real visit");
    f::buy(world,sim::set_building_operation(s.target,false));f::buy(replay,sim::set_building_operation(s.target,false));
    while(world.courier(channel).phase!=sim::CourierPhase::IdleAtWorkshop) {
        world.tick();replay.tick();check(replay.snapshot()==world.snapshot(),"Physical return restore diverged");
    }
    check(!world.courier_position(channel)&&q::source(world,s.target).spawned==1&&
          q::walkers(world).next_walker_id==2,"Return did not retire the actual actor identity");
    // The ordinary House evaluation retains the actual arrival input. No new
    // evolution bonus or level is manufactured by the culture adapter.
    q::through(world,world.ticks()+60);
    for(const auto& h:world.original_production_state()->housing.houses) if(h.building_id==arrived.target) {
        check(h.last_evaluation&&h.last_evaluation->service_witnesses[6]&&
              h.last_evaluation->service_witnesses[6]->venue_walker==arrived.venue_walker&&
              orig::service_timer(h.last_evaluation->input,orig::ServiceRole::Music)>0,
              "House Needs/evolution evaluation discarded the actual Music arrival");
    }
    q::invariants(world);
}
void source_and_venue_negatives() {
    for(const int variant:{0,1,2,3}) {
        q::Fixture fixture;auto world=fixture.world();
        for(int x=110;x<=123;++x)if(variant!=2||x!=117) f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
        for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
        const auto venue=q::buy(world,sim::CommandType::PlaceOriginalTheatre,old::venue_cell);
        if(variant!=0) {
            const auto school=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,old::source_cell);
            if(variant==1) f::buy(world,sim::set_building_operation(school,false));
        }
        if(variant==3) f::buy(world,sim::set_building_operation(venue,false));
        q::through(world,700);
        check(!q::venue(world,venue).music_timer&&!q::source(world,venue).attempts&&!q::source(world,venue).spawned&&
              q::walkers(world).next_walker_id==1,"Absent/paused/disconnected School or Theatre created venue authority");
        no_music(world);
    }
}
void arrival_requires_current_source() {
    for(const bool pause:{true,false}) {
        auto model=old::authored_model();
        if(!pause) {
            // Authored staffing stress:80 real paid jobs fit the three37-cap
            // Houses before departure. Two installed18-job Hemp competitors
            // then leave less than80; no residents or allocation are injected.
            const std::string row="75,Theatre Pavilion,{,105,0,0,0,0,7,";
            const auto at=model.find(row);check(at!=std::string::npos,"Authored Venue staffing control missing");
            model.replace(at,row.size(),"75,Theatre Pavilion,{,105,0,0,0,0,80,");
        }
        q::Fixture fixture(model);auto world=fixture.world();const auto s=q::scene(world);
        const auto channel=q::source(world,s.target).courier_id;
        q::until(world,[&]{return q::source(world,s.target).spawned==1;},"Source-refusal walker never spawned");
        const auto before=world.courier(channel);const auto evidence=trip(world,s.target);
        if(pause) f::buy(world,sim::set_building_operation(s.target,false));
        else {
            const auto a=q::buy(world,sim::CommandType::PlaceOriginalHempFarm,{106,120});
            const auto b=q::buy(world,sim::CommandType::PlaceOriginalHempFarm,{110,120});
            f::buy(world,sim::set_building_workforce_priority(a,sim::WorkforcePriority::High));
            f::buy(world,sim::set_building_workforce_priority(b,sim::WorkforcePriority::High));
            f::buy(world,sim::set_building_workforce_priority(s.target,sim::WorkforcePriority::Low));
            check(!world.building_staffed(s.target),"Paid competing workforce did not unstaff Theatre");
        }
        check(world.courier(channel).edge_progress==before.edge_progress&&trip(world,s.target)==evidence,
              "Source pause/staff loss erased begun movement or granted a visit");
        q::until(world,[&]{return trip(world,s.target).arrival_tick.has_value();},"Unavailable-source trip never arrived");
        check(!trip(world,s.target).visit_applied&&!trip(world,s.target).venue_walker->arrival_music&&
              world.courier(channel).phase==sim::CourierPhase::Returning,"Unavailable source granted Music or teleported home");
        no_music(world);q::until(world,[&]{return world.courier(channel).phase==sim::CourierPhase::IdleAtWorkshop;},"Refused walker failed real return");
        check(!world.courier_position(channel),"Retired refused walker remains visible");
    }
}
void road_cut_expiry_and_paid_repair() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    const auto channel=q::source(world,s.target).courier_id;
    q::until(world,[&]{return q::source(world,s.target).spawned==1;},"Road-cut walker never spawned");
    const auto first=trip(world,s.target);const auto actor=world.courier(channel);const auto position=world.courier_position(channel);
    f::buy(world,sim::set_building_operation(s.source,false));
    f::buy(world,{sim::CommandType::RemoveRoad,old::cut_cell});
    check(world.courier(channel).edge_progress==actor.edge_progress&&world.courier_position(channel)==position&&
          trip(world,s.target)==first,"Road cut changed begun edge, ID or authority");
    q::through(world,world.ticks()+1700);
    check(!q::venue(world,s.target).music_timer&&!trip(world,s.target).arrival_tick&&
          trip(world,s.target).venue_walker==first.venue_walker&&q::source(world,s.target).spawned==1,
          "Disconnected walker delivered or was replaced while Music expired");
    no_music(world);auto replay=fixture.restore(world.snapshot());const auto funds=world.treasury();
    f::buy(world,{sim::CommandType::PlaceRoad,old::cut_cell});f::buy(replay,{sim::CommandType::PlaceRoad,old::cut_cell});
    check(world.treasury()==funds-2,"Road repair was not paid");
    while(!trip(world,s.target).arrival_tick) {world.tick();replay.tick();check(world.snapshot()==replay.snapshot(),"Wait/repair restore diverged");}
    check(!trip(world,s.target).visit_applied&&!trip(world,s.target).venue_walker->arrival_music,
          "Dispatch-time Music authorized a grant after actual expiry");
    no_music(world);q::until(world,[&]{return world.courier(channel).phase==sim::CourierPhase::IdleAtWorkshop;},"Expired-input walker did not return");
}
void repeated_real_walkers_and_independent_venues() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    const auto channel=q::source(world,s.target).courier_id;
    q::until(world,[&]{return q::source(world,s.target).spawned==1;},"First real walker missing");
    const auto first=trip(world,s.target);
    q::until(world,[&]{return q::source(world,s.target).spawned==2;},"Second real visit cycle missing");
    const auto second=trip(world,s.target);
    check(second.venue_walker->walker_id>first.venue_walker->walker_id&&second.transfer.sequence==2&&
          second.target!=first.target&&world.courier(channel).phase==sim::CourierPhase::ToWarehouse,
          "Repeated trip reused Actor ID, skipped real return or lost canonical House choice");
    check(music(world,first.target)>0&&!music(world,second.target),"Later spawn granted a future House visit");
    q::until(world,[&]{return trip(world,s.target).arrival_tick.has_value();},"Second real walker never arrived");
    check(trip(world,s.target).visit_applied&&music(world,second.target)>0,"Repeated real House arrival did not grant");
    for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto other=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    q::until(world,[&]{return q::source(world,other).spawned>0;},"Independent paid second Venue never spawned");
    check(q::source(world,other).courier_id!=channel&&
          trip(world,other).venue_walker->walker_id!=trip(world,s.target).venue_walker->walker_id&&
          world.original_production_state()->services.providers.size()==2,
          "Independent venues share a movement channel or active Actor identity");q::invariants(world);
}
void natural_source_fire_blocks_attempts_and_real_grants() {
    for(const std::uint64_t houses_at:{200U,1480U}) {
        q::Fixture fixture;auto world=fixture.world();
        for(int x=110;x<=123;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
        const auto school=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,old::source_cell);
        const auto venue=q::buy(world,sim::CommandType::PlaceOriginalTheatre,old::venue_cell);
        q::through(world,houses_at);
        for(int x:{110,113,116})(void)q::buy(world,sim::CommandType::PlaceHousehold,{x,112});
        const auto channel=q::source(world,venue).courier_id;
        q::until(world,[&]{return q::venue(world,venue).last_arrival.has_value();},"Fire control never earned real Venue Music");
        f::buy(world,sim::set_building_operation(school,false));
        if(houses_at==200) {
            q::until(world,[&]{return q::source(world,venue).spawned==1;},"Fire-refusal walker never spawned");
            f::buy(world,{sim::CommandType::RemoveRoad,old::cut_cell});
        }
        q::through(world,1999);const auto before=q::source(world,venue);
        world.tick();q::invariants(world);
        check(world.building_on_fire(venue)&&q::venue(world,venue).music_timer>0,
              "Paid fire control did not produce an actual supplied burning Theatre");
        if(houses_at==1480) {
            check(!before.spawned&&!before.attempts,"Late paid staffing did not isolate pre-spawn fire");
            q::through(world,2080);
            check(q::source(world,venue)==before,"Burning supplied Theatre advanced its eligible spawn counter");
        } else {
            check(!trip(world,venue).arrival_tick&&q::source(world,venue).spawned==1,
                  "Blocked physical walker delivered or was replaced before source fire");
            f::buy(world,{sim::CommandType::PlaceRoad,old::cut_cell});
            q::until(world,[&]{return trip(world,venue).arrival_tick.has_value();},"Fire-refusal walker never completed real paid repair");
            check(world.building_on_fire(venue)&&q::venue(world,venue).music_timer>0&&
                  !world.building_on_fire(trip(world,venue).target)&&!trip(world,venue).visit_applied&&
                  !trip(world,venue).venue_walker->arrival_music,
                  "Burning source granted House Music despite a real current-Music refusal");
            check(world.courier(channel).phase==sim::CourierPhase::Returning,"Fire refusal teleported the actual walker");
        }
        no_music(world);check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Valid fire history failed exact restore");
    }
}
void unusable_incoming_music_never_suppresses_another_real_source() {
    for(const bool expire:{false,true}) {
        q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
        // Four paid homes earn eight residents at the first admission, so the
        // first staffed School cycle and subsequent Music expiry precede the
        // independent natural-fire clock. No resident or timer is injected.
        for(int x=107;x<=109;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
        (void)q::buy(world,sim::CommandType::PlaceHousehold,{107,112});
        for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
        const auto far=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
        q::until(world,[&]{return q::venue(world,s.target).last_arrival.has_value();},"Near Venue input missing");
        f::buy(world,sim::set_building_operation(s.target,false));
        q::until(world,[&]{return q::venue(world,far).last_arrival.has_value();},"Far Venue input missing");
        f::buy(world,sim::set_building_operation(far,false));
        f::buy(world,sim::set_building_operation(s.source,false));
        f::buy(world,sim::set_building_operation(s.target,true));
        q::until(world,[&]{return q::source(world,s.target).spawned==1;},"Near Venue actor missing");
        const auto target=trip(world,s.target).target;
        const auto other=q::source(world,far).courier_id;
        if(expire) {
            f::buy(world,{sim::CommandType::RemoveRoad,old::cut_cell});
            q::until(world,[&]{return !q::venue(world,s.target).music_timer;},"Held near Venue Music did not expire",1800);
            check(!trip(world,s.target).arrival_tick&&q::venue(world,far).music_timer>0,
                  "Expiry control lost the actual incoming actor or independent later Music");
            f::buy(world,{sim::CommandType::PlaceRoad,old::cut_cell});
            if(world.courier(q::source(world,s.target).courier_id).route_pending)world.tick();
            const auto& held=world.courier(q::source(world,s.target).courier_id);
            check(world.ticks()<2000&&held.phase==sim::CourierPhase::ToWarehouse&&
                  !held.route_pending&&held.path_vertex+1<held.path.size(),
                  "Expiry control did not retain a physically usable incoming route before natural fire");
        } else {
            f::buy(world,sim::set_building_operation(far,true));
            const auto before=world.courier_dispatch_status(other);
            check(before.status==sim::CourierDispatchStatus::Ready&&before.selected_target!=target,
                  "Usable earlier incoming visit was not coalesced by the inherited selector");
            f::buy(world,sim::set_building_operation(s.target,false));
        }
        if(expire)f::buy(world,sim::set_building_operation(far,true));
        const auto snapshot=world.snapshot();const auto decision=world.courier_dispatch_status(other);
        check(decision.status==sim::CourierDispatchStatus::Ready&&decision.selected_target==target&&
              world.snapshot()==snapshot,
              "Unusable paused/expired incoming Music suppressed a reachable uncovered House");
        no_music(world);q::invariants(world);
    }
}
}
int main(){try{
    contracts_and_no_phantom_service();source_and_venue_negatives();arrival_requires_current_source();
    road_cut_expiry_and_paid_repair();repeated_real_walkers_and_independent_venues();
    natural_source_fire_blocks_attempts_and_real_grants();
    unusable_incoming_music_never_suppresses_another_real_source();
    std::cout<<"Rule23 paid Performer/Venue/House96, distinct real lifetimes, return, refusal, source expiry, roads and historical boundaries PASS\n";return 0;
}catch(const std::exception& error){perf::set_enabled(false);std::cerr<<"Venue House World FAIL: "<<error.what()<<'\n';return 1;}}
