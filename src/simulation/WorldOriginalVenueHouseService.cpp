#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
std::uint64_t counts(std::uint64_t tick) {return tick<36 ? 0:1+(tick-36)/51;}
bool cycle(std::uint64_t tick) {return tick>=36 && (tick-36)%51==0;}
template<class Rows,class Id> const auto& find(const Rows& rows,Id id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,Id key) {
        if constexpr(requires {row.building_id;}) return row.building_id<key;
        else return row.id<key;
    });
    if(at==rows.end()) invalid("Venue walker refers to a missing paid identity");
    if constexpr(requires {at->building_id;}) {if(at->building_id!=id) invalid("Venue walker has no matching source");}
    else if(at->id!=id) invalid("Venue walker has no matching entity");
    return *at;
}
template<class A,class B> std::uint64_t travel(const A& a,const B& b) {
    const auto from=building_footprint(RulesProfile::CityV16,kOriginalVenueHouseServiceRuleVersion,a.kind);
    const auto to=building_footprint(RulesProfile::CityV16,kOriginalVenueHouseServiceRuleVersion,b.kind);
    const auto gap=[](int x,int size,int y,int other) {
        return std::max({std::int64_t{0},std::int64_t(x)-(std::int64_t(y)+other-1),std::int64_t(y)-(std::int64_t(x)+size-1)});
    };
    return static_cast<std::uint64_t>(std::max<std::int64_t>(2,
        gap(a.cell.x,from.width,b.cell.x,to.width)+gap(a.cell.y,from.height,b.cell.y,to.height)))*5;
}
struct Observation {
    const OriginalVenueWalkerEvidence* evidence=nullptr;
    BuildingId venue{},house{};
    OriginalGoodsTransferKey transfer;
    std::optional<std::uint64_t> arrival;
};
constexpr std::size_t observation_limit=original_service_trip_limit+original_service_witness_limit+
    Rules::city_v10_household_limit*(original::house_row_count+1);
struct MusicObservation {const OriginalMusicArrival* music=nullptr;BuildingId venue{};};
constexpr std::size_t music_observation_limit=2*observation_limit+original_music_school_limit+original_culture_venue_limit;

template<class Buildings,class Couriers>
void validate_venue_service(bool enabled,const OriginalCultureState* culture,const Buildings& buildings,
    const Couriers& couriers,const OriginalHousingState& housing,
    const OriginalServicesState& services,std::uint64_t tick) {
    std::array<Observation,observation_limit> observations{};std::size_t observation_count=0;
    const auto retain=[&](const std::optional<OriginalVenueWalkerEvidence>& evidence,BuildingId source,BuildingId target,
                          OriginalGoodsTransferKey transfer,std::optional<std::uint64_t> arrival,bool applied) {
        if(!evidence) {
            if(enabled && find(buildings,source).kind==Object::OriginalTheatre75)
                invalid("Actual venue service trip or witness lost its walker evidence");
            return;
        }
        if(!enabled || !culture || !culture->venue_house_service ||
            find(buildings,source).kind!=Object::OriginalTheatre75 ||
            evidence->arrival_music.has_value()!=applied || (applied && !arrival))
            invalid("Venue walker evidence belongs only to an actual Rule23 Music trip or grant");
        if(observation_count==observations.size()) invalid("Venue walker retained observation budget exceeded");
        observations[observation_count++]={&*evidence,source,target,transfer,arrival};
    };
    for(const auto& t:services.trips) {
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==t.transfer.courier_id;});
        if(actor==couriers.end()) invalid("Service trip has no actual courier");
        retain(t.venue_walker,actor->owner,t.target,t.transfer,t.arrival_tick,t.visit_applied);
        if(t.venue_walker && t.venue_walker->dispatched_tick!=t.dispatched_tick)
            invalid("Walker spawn and real trip dispatch disagree");
    }
    const auto witness=[&](const OriginalServiceWitness& w) {
        retain(w.venue_walker,w.provider_id,w.building_id,w.transfer,w.arrival_tick,true);
        if(w.venue_walker && w.role!=original::ServiceRole::Music)
            invalid("Theatre Music evidence cannot authorize another service");
    };
    for(const auto& w:services.witnesses) witness(w);
    for(const auto& house:housing.houses) {
        const auto evaluation=[&](const OriginalHousingEvaluation& e) {for(const auto& w:e.service_witnesses) if(w) witness(*w);};
        if(house.last_evaluation) evaluation(*house.last_evaluation);
        for(const auto& t:house.first_transitions) if(t) evaluation(t->evaluation);
    }
    if(!enabled) {
        if((culture && culture->venue_house_service) ||
            std::any_of(couriers.begin(),couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalVenueMusic;}))
            invalid("Historical generation cannot acquire venue House authority");
        return;
    }
    if(!culture || !culture->venue_house_service) invalid("Rule23 requires its separate venue House authority");
    const auto& state=*culture->venue_house_service;
    if(!state.next_walker_id || state.sources.size()!=culture->venues.size() || state.sources.size()>original_culture_venue_limit ||
        std::count_if(couriers.begin(),couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalVenueMusic;})!=static_cast<std::ptrdiff_t>(state.sources.size()))
        invalid("Venue House authority lost its bounded paid source/channel bindings");
    std::uint64_t spawned=0,maximum_id=0;
    std::optional<std::uint64_t> first_admission;
    for(const auto& h:housing.houses) if(h.first_admission_tick && (!first_admission || *h.first_admission_tick<*first_admission)) first_admission=h.first_admission_tick;
    for(std::size_t i=0;i<state.sources.size();++i) {
        const auto& row=state.sources[i];const auto& body=find(buildings,row.building_id);const auto& actor=find(couriers,row.courier_id);
        const auto& venue=find(culture->venues,row.building_id);
        if(row.first_music.has_value()!=venue.last_arrival.has_value()) invalid("Venue source lost its first actual Music admission");
        if(row.first_music && (row.first_music->arrival_tick>venue.last_arrival->arrival_tick ||
            row.first_music->performer_id>venue.last_arrival->performer_id ||
            (row.first_music->performer_id==venue.last_arrival->performer_id && *row.first_music!=*venue.last_arrival)))
            invalid("Venue first Music input contradicts its latest actual arrival");
        if(!venue.last_arrival && (row.spawn_counter || row.attempts || row.spawned))
            invalid("Venue attempts cannot precede every actual Music input");
        if((i && !(state.sources[i-1].building_id<row.building_id)) || body.kind!=Object::OriginalTheatre75 || !body.placed ||
            actor.owner!=body.id || actor.role!=CourierRole::OriginalVenueMusic || !actor.enabled || actor.cargo || actor.reserved ||
            row.spawn_counter>3 || row.spawned>row.attempts || spawned>UINT64_MAX-row.spawned)
            invalid("Venue source lost its unique paid body, own channel or bounded counter");
        const auto floor=std::max({body.placed_tick,first_admission.value_or(tick),row.first_music ? row.first_music->arrival_tick:tick});
        const auto possible=counts(tick)-counts(floor);
        if(row.spawn_counter>possible || row.attempts>(possible-row.spawn_counter)/4)
            invalid("Venue attempts predate possible occupied staffing cycles");
        spawned+=row.spawned;
        const auto trip=std::find_if(services.trips.begin(),services.trips.end(),[&](const auto& t){return t.transfer.courier_id==actor.id;});
        if(!row.spawned) {
            if(trip!=services.trips.end() || actor.phase!=CourierPhase::IdleAtWorkshop || actor.target!=actor.owner || actor.last_dispatched_target ||
                !actor.path.empty() || actor.path_vertex || actor.edge_progress || actor.route_pending || actor.reroute_attempts)
                invalid("Unspawned venue channel owns a phantom actor or destination");
        } else {
            if(trip==services.trips.end() || !trip->venue_walker || trip->transfer.sequence!=row.spawned)
                invalid("Venue source lost the actual lifetime count and latest real trip");
            if(actor.phase!=CourierPhase::IdleAtWorkshop && row.spawn_counter)
                invalid("Moving venue walker cannot advance its source's idle spawn counter");
            if(actor.phase==CourierPhase::IdleAtWorkshop && trip->arrival_tick) {
                const auto minimum_return=travel(find(buildings,trip->target),body);
                if(*trip->arrival_tick>UINT64_MAX-minimum_return || *trip->arrival_tick+minimum_return>tick ||
                    row.spawn_counter>counts(tick)-counts(*trip->arrival_tick+minimum_return))
                    invalid("Venue idle counter predates its latest possible physical return");
            }
            maximum_id=std::max(maximum_id,static_cast<std::uint64_t>(trip->venue_walker->walker_id));
            if(row.spawned>(counts(trip->dispatched_tick)-counts(floor))/4)
                invalid("Latest venue walker predates its required eligible spawn calls");
        }
    }
    if(spawned==UINT64_MAX || state.next_walker_id!=spawned+1 || maximum_id!=spawned)
        invalid("Venue walker IDs lost their exact monotone successful-spawn highwater");

    std::array<MusicObservation,music_observation_limit> music_observations{};std::size_t music_count=0;
    const auto music=[&](const OriginalMusicArrival& m,BuildingId venue,std::uint64_t event,bool before_dispatch) {
        if(music_count==music_observations.size()) invalid("Retained Music observation budget exceeded");
        music_observations[music_count++]={&m,venue};
        const auto& source=find(buildings,m.school_id);const auto& target=find(buildings,venue);
        const auto& school=find(culture->schools,m.school_id);const auto& actor=find(couriers,m.courier_id);
        const auto& latest=find(culture->venues,venue).last_arrival;
        if(!static_cast<std::uint64_t>(m.performer_id) || static_cast<std::uint64_t>(m.performer_id)>=culture->next_performer_id ||
            source.kind!=Object::OriginalMusicSchool211 || school.courier_id!=actor.id || actor.owner!=source.id ||
            actor.role!=CourierRole::OriginalMusicPerformer || !cycle(m.dispatched_tick) || m.dispatched_tick<=source.placed_tick ||
            m.dispatched_tick<=target.placed_tick || m.arrival_tick<=m.dispatched_tick ||
            m.arrival_tick-m.dispatched_tick<travel(source,target)-1 || m.arrival_tick>event ||
            (before_dispatch && m.arrival_tick==event) || !latest || latest->arrival_tick<m.arrival_tick ||
            !school.latest_trip || static_cast<std::uint64_t>(m.performer_id)>static_cast<std::uint64_t>(school.latest_trip->performer_id))
            invalid("Venue walker Music has no possible actual paid Performer arrival");
        // Before phase35 decay at this actual event, current received Music
        // must still be positive. Neither dispatch nor restore refreshes it.
        if(!event || counts(event-1)<counts(m.arrival_tick-1) || counts(event-1)-counts(m.arrival_tick-1)>=32)
            invalid("Venue walker claimed Music after its actual input expired");
        if(latest->performer_id==m.performer_id && *latest!=m)
            invalid("Venue Music evidence contradicts its retained actual arrival");
        if(!first_admission || m.dispatched_tick<=std::max(source.placed_tick,*first_admission) ||
            counts(m.dispatched_tick)-counts(std::max(source.placed_tick,*first_admission))<4)
            invalid("Retained Music input predates possible School training");
        const auto& trip=*school.latest_trip;
        if(trip.performer_id==m.performer_id) {
            if(!trip.accepted || trip.venue_id!=venue || trip.dispatched_tick!=m.dispatched_tick || trip.arrival_tick!=m.arrival_tick)
                invalid("Venue Music evidence contradicts its Performer trip");
        } else if(trip.dispatched_tick<=m.arrival_tick) invalid("Compacted Music input postdates a later Performer spawn");
    };
    for(std::size_t i=0;i<observation_count;++i) {
        const auto& o=observations[i];const auto& e=*o.evidence;const auto& row=find(state.sources,o.venue);
        const auto& actor=find(couriers,o.transfer.courier_id);
        const auto id=static_cast<std::uint64_t>(e.walker_id);
        if(!row.first_music || !id || id>=state.next_walker_id || !cycle(e.dispatched_tick) || e.dispatched_tick>tick ||
            e.dispatched_tick<=find(buildings,o.venue).placed_tick || actor.owner!=o.venue || actor.id!=row.courier_id ||
            actor.role!=CourierRole::OriginalVenueMusic || !o.transfer.sequence || id<o.transfer.sequence || o.transfer.sequence>row.spawned ||
            (o.arrival && (*o.arrival<=e.dispatched_tick || *o.arrival>tick || *o.arrival-e.dispatched_tick<travel(find(buildings,o.venue),find(buildings,o.house))-1)))
            invalid("Venue walker observation has no possible paid spawn and arrival");
        if(e.dispatch_music.arrival_tick<row.first_music->arrival_tick ||
            counts(e.dispatched_tick)-counts(row.first_music->arrival_tick)<4)
            invalid("Venue walker predates four eligible calls after its first actual Music input");
        music(e.dispatch_music,o.venue,e.dispatched_tick,true);
        if(e.arrival_music) {
            // Movement callbacks run in ascending courier order. A Performer
            // arriving later in this same tick cannot supply an earlier
            // Theatre channel's already completed House callback.
            if(e.arrival_music->arrival_tick==*o.arrival &&
                e.arrival_music->courier_id>=o.transfer.courier_id)
                invalid("Venue House grant precedes its same-tick Performer input");
            music(*e.arrival_music,o.venue,*o.arrival,false);
        }
    }
    std::sort(observations.begin(),observations.begin()+static_cast<std::ptrdiff_t>(observation_count),[](const auto& a,const auto& b){return a.evidence->walker_id<b.evidence->walker_id;});
    std::array<std::uint64_t,original_culture_venue_limit> retained{};
    std::array<const Observation*,original_culture_venue_limit> previous{};
    for(std::size_t i=0;i<observation_count;++i) {
        const auto& a=observations[i];
        if(i && observations[i-1].evidence->walker_id==a.evidence->walker_id) {
            const auto& b=observations[i-1];
            if(a.venue!=b.venue || a.house!=b.house || a.transfer!=b.transfer || a.arrival!=b.arrival || *a.evidence!=*b.evidence)
                invalid("One venue walker identity cannot change source, trip, House or Music history");
        } else {
            const auto at=std::lower_bound(state.sources.begin(),state.sources.end(),a.venue,[](const auto& r,BuildingId id){return r.building_id<id;});
            const auto index=static_cast<std::size_t>(at-state.sources.begin());
            ++retained[index];
            if(const auto* old=previous[index]) {
                if(old->transfer.sequence>=a.transfer.sequence ||
                    static_cast<std::uint64_t>(a.evidence->walker_id)-static_cast<std::uint64_t>(old->evidence->walker_id)<a.transfer.sequence-old->transfer.sequence || !old->arrival ||
                    a.evidence->dispatched_tick<=*old->arrival ||
                    a.evidence->dispatched_tick-*old->arrival<=travel(find(buildings,old->house),find(buildings,old->venue)))
                    invalid("Venue walker source reused a sequence or spawned before the previous real return");
                const auto return_ticks=travel(find(buildings,old->house),find(buildings,old->venue));
                if(*old->arrival>UINT64_MAX-return_ticks ||
                    a.transfer.sequence-old->transfer.sequence>
                        (counts(a.evidence->dispatched_tick)-counts(*old->arrival+return_ticks))/4)
                    invalid("Venue walker spawned before four new eligible idle calls after physical return");
            }
            previous[index]=&a;
            if(i) {
                const auto& b=observations[i-1];
                if(std::pair{b.evidence->dispatched_tick,b.transfer.courier_id}>=std::pair{a.evidence->dispatched_tick,a.transfer.courier_id})
                    invalid("Venue walker identities contradict stable actual spawn order");
            }
        }
    }
    for(std::size_t i=0;i<state.sources.size();++i) if(retained[i]>state.sources[i].spawned)
        invalid("Retained venue walker lifetimes exceed the actual source spawn count");
    // The first accepted input is immutable and remains physically validated
    // even after later School arrivals replace the current Venue32 witness.
    for(const auto& source:state.sources) if(source.first_music) {
        const auto& m=*source.first_music;
        music(m,source.building_id,m.arrival_tick,false);
    }
    for(const auto& venue:culture->venues) if(venue.last_arrival) {
        if(music_count==music_observations.size()) invalid("Retained Music observation budget exceeded");
        music_observations[music_count++]={&*venue.last_arrival,venue.building_id};
    }
    // Repeated input observations must retain one immutable Performer identity.
    std::sort(music_observations.begin(),music_observations.begin()+static_cast<std::ptrdiff_t>(music_count),[](const auto& a,const auto& b){return a.music->performer_id<b.music->performer_id;});
    std::array<std::uint64_t,original_music_school_limit> retained_music{};
    std::array<const OriginalMusicArrival*,original_culture_venue_limit> previous_music{};
    for(std::size_t i=0;i<music_count;++i) {
        const auto& a=music_observations[i];
        if(i && music_observations[i-1].music->performer_id==a.music->performer_id) continue;
        const auto school=std::lower_bound(culture->schools.begin(),culture->schools.end(),a.music->school_id,
            [](const auto& row,BuildingId id){return row.building_id<id;});
        if(school==culture->schools.end() || school->building_id!=a.music->school_id) invalid("Music proof lost its paid School");
        ++retained_music[static_cast<std::size_t>(school-culture->schools.begin())];
        const auto venue=std::lower_bound(culture->venues.begin(),culture->venues.end(),a.venue,
            [](const auto& row,BuildingId id){return row.building_id<id;});
        const auto index=static_cast<std::size_t>(venue-culture->venues.begin());
        if(const auto* old=previous_music[index]) {
            if(a.music->arrival_tick<=old->arrival_tick || counts(a.music->arrival_tick-1)-counts(old->arrival_tick-1)<32)
                invalid("One Theatre cannot accept a second Music input before the prior slot expired");
        }
        previous_music[index]=a.music;
    }
    for(std::size_t i=0;i<culture->schools.size();++i) if(retained_music[i]>culture->schools[i].spawned)
        invalid("Retained Music input lifetimes exceed actual School spawns");
    for(std::size_t i=1;i<music_count;++i) {
        const auto& a=music_observations[i-1];const auto& b=music_observations[i];
        if(a.music->performer_id==b.music->performer_id && (a.venue!=b.venue || *a.music!=*b.music))
            invalid("One Performer cannot supply different venue Music histories");
        if(a.music->performer_id!=b.music->performer_id &&
            std::pair{a.music->dispatched_tick,a.music->courier_id}>=std::pair{b.music->dispatched_tick,b.music->courier_id})
            invalid("Retained Music inputs contradict monotone Performer spawn order");
    }
}
} // namespace

World World::create_original_venue_house_service(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=create_original_music_venues(std::move(permissions),std::move(model),std::move(terrain));
    world.rule_version_=kOriginalVenueHouseServiceRuleVersion;
    world.original_production_->culture->venue_house_service.emplace();return world;
}
World World::restore_original_venue_house_service(const WorldSnapshot& state,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if(!simulation::original_venue_house_service_enabled(state.profile,state.rule_version) || !permissions ||
        permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain ||
        !model->supports_building(original::ModelBuilding::TheatrePavilion))
        invalid("Venue House restore requires exact Rule23, policy3 and prepared culture model/terrain");
    validate_original_production_snapshot(state);
    auto world=restore_impl(state,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();
    world.validate_original_industry_model();world.validate_original_finance_model();world.validate_original_music_venues_model();
    world.validate_original_venue_house_service_model();return world;
}
void World::validate_original_venue_house_service_snapshot(const WorldSnapshot& s) {
    if(!s.original_production) return;
    validate_venue_service(simulation::original_venue_house_service_enabled(s.profile,s.rule_version),
        s.original_production->culture ? &*s.original_production->culture:nullptr,s.buildings,s.couriers,
        s.original_production->housing,s.original_production->services,s.ticks);
}
void World::validate_original_venue_house_service_state() const {
    if(!original_venue_house_service_enabled()) return;
    validate_venue_service(true,&*original_production_->culture,buildings_,couriers_,original_production_->housing,original_production_->services,ticks_);
}
bool World::original_venue_music_available(BuildingId id) const {
    if(!original_venue_house_service_enabled() || !original_production_ || !original_production_->culture) return false;
    const auto& rows=original_production_->culture->venues;
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.building_id<key;});
    return at!=rows.end() && at->building_id==id && at->music_timer>0 && at->last_arrival.has_value();
}
void World::validate_original_venue_house_service_model() const {
    if(!original_venue_house_service_enabled()) return;
    // Necessary optimistic staffing ceilings, not reconstructed priorities or
    // a claim about historical source pause/fire states.
    const auto workers=static_cast<std::uint64_t>(original_rules_model_->building_value(
        original::ModelBuilding::TheatrePavilion,original::BuildingField::Employees));
    std::uint64_t capacity=0;
    for(std::size_t i=0;i<original::house_row_count;++i) capacity=std::max(capacity,static_cast<std::uint64_t>(
        original_rules_model_->house_rules().row_capacity(static_cast<original::ModelHouse>(i))));
    const auto enough=[&](std::uint64_t at,std::uint64_t required) {
        for(const auto& h:original_production_->housing.houses) if(h.first_admission_tick && at>=*h.first_admission_tick) {
            const auto intervals=(at-*h.first_admission_tick)/40;
            const auto possible=intervals>=(capacity+1)/2-1 ? capacity:2*(1+intervals);
            if(possible>=required) return true;required-=possible;
        }
        return required==0;
    };
    for(const auto& row:original_production_->culture->venue_house_service->sources) {
        if(!enough(ticks_,workers)) {
            if(row.attempts || row.spawn_counter || row.spawned) invalid("Venue attempts predate possible complete model staffing");
            continue;
        }
        std::uint64_t low=0,high=ticks_;
        while(low<high) {const auto middle=low+(high-low)/2;if(enough(middle,workers)) high=middle;else low=middle+1;}
        const auto floor=std::max({low,building(row.building_id).placed_tick,row.first_music ? row.first_music->arrival_tick:ticks_});
        const auto possible=counts(ticks_)-counts(floor);
        if(row.spawn_counter>possible || row.attempts>(possible-row.spawn_counter)/4)
            invalid("Venue spawn counter exceeds possible staffed model cycles");
        for(const auto& t:original_production_->services.trips) if(t.transfer.courier_id==row.courier_id &&
            (!enough(t.dispatched_tick-1,workers) || row.spawned>(counts(t.dispatched_tick)-counts(floor))/4))
            invalid("Venue walker predates possible actual model workforce");
    }
    struct Event {std::uint64_t tick;BuildingId source;};
    std::vector<Event> events;
    events.reserve(4*observation_limit+16);
    const auto musician=[&](const OriginalMusicArrival& m,BuildingId venue) {
        events.push_back({m.dispatched_tick,m.school_id});events.push_back({m.dispatched_tick,venue});
    };
    const auto evidence=[&](const std::optional<OriginalVenueWalkerEvidence>& e,BuildingId venue) {if(e) {
        events.push_back({e->dispatched_tick,venue});musician(e->dispatch_music,venue);
        if(e->arrival_music) musician(*e->arrival_music,venue);
    }};
    for(const auto& school:original_production_->culture->schools) if(school.latest_trip)
        musician({school.latest_trip->performer_id,school.building_id,school.courier_id,
            school.latest_trip->dispatched_tick,school.latest_trip->arrival_tick.value_or(ticks_)},school.latest_trip->venue_id);
    for(const auto& venue:original_production_->culture->venues) if(venue.last_arrival) musician(*venue.last_arrival,venue.building_id);
    for(const auto& row:original_production_->culture->venue_house_service->sources) if(row.first_music) musician(*row.first_music,row.building_id);
    for(const auto& t:original_production_->services.trips) evidence(t.venue_walker,courier(t.transfer.courier_id).owner);
    const auto witness=[&](const OriginalServiceWitness& w){evidence(w.venue_walker,w.provider_id);};
    for(const auto& w:original_production_->services.witnesses) witness(w);
    for(const auto& h:original_production_->housing.houses) {
        const auto evaluation=[&](const OriginalHousingEvaluation& e){for(const auto& w:e.service_witnesses)if(w)witness(*w);};
        if(h.last_evaluation)evaluation(*h.last_evaluation);
        for(const auto& t:h.first_transitions)if(t)evaluation(t->evaluation);
    }
    std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return std::pair{a.tick,a.source}<std::pair{b.tick,b.source};});
    for(std::size_t i=0;i<events.size();) {
        const auto tick=events[i].tick;std::uint64_t required=0;std::optional<BuildingId> previous;
        while(i<events.size()&&events[i].tick==tick) {
            if(!previous || *previous!=events[i].source) {
                required+=static_cast<std::uint64_t>(workforce_required(events[i].source));previous=events[i].source;
            }
            ++i;
        }
        if(!tick || !enough(tick-1,required)) invalid("Simultaneous actual culture spawns exceed possible shared model workforce");
    }
}
} // namespace openemperor::simulation
