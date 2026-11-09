#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
std::uint64_t value(PerformerId id) {return static_cast<std::uint64_t>(id);}
std::uint64_t countdowns(std::uint64_t tick) {return tick<36 ? 0:1+(tick-36)/51;}
bool culture_cycle(std::uint64_t tick) {return tick>=36 && (tick-36)%51==0;}
template<class Rows,class Id> decltype(auto) body(Rows& rows,Id id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,Id key){return row.id<key;});
    if (at==rows.end() || at->id!=id) invalid("Music venue reference has no actual paid entity");
    return *at;
}
template<class Rows> decltype(auto) owned_row(Rows& rows,BuildingId id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.building_id<key;});
    if (at==rows.end() || at->building_id!=id) invalid("Music venue reference has no matching paid authority");
    return *at;
}
template<class Building> std::uint64_t travel(const Building& from,const Building& to) {
    const auto a=building_footprint(RulesProfile::CityV16,kOriginalMusicVenuesRuleVersion,from.kind);
    const auto b=building_footprint(RulesProfile::CityV16,kOriginalMusicVenuesRuleVersion,to.kind);
    const auto gap=[](int p,int size,int q,int other) {
        return std::max({std::int64_t{0},std::int64_t(p)-(std::int64_t(q)+other-1),std::int64_t(q)-(std::int64_t(p)+size-1)});
    };
    return static_cast<std::uint64_t>(std::max<std::int64_t>(2,
        gap(from.cell.x,a.width,to.cell.x,b.width)+gap(from.cell.y,a.height,to.cell.y,b.height)))*5;
}
template<class Building,class Courier> std::uint64_t movement(const Building& from,const Courier& c) {
    if (c.path_vertex>=c.path.size() || c.edge_progress<0 || c.edge_progress>=5)
        invalid("Music performer has invalid actual movement progress");
    const auto p=c.path[c.path_vertex];
    const auto a=building_footprint(RulesProfile::CityV16,kOriginalMusicVenuesRuleVersion,from.kind);
    const auto gap=[](int p0,int size,int p1) {
        return std::max({std::int64_t{0},std::int64_t(p0)-p1,std::int64_t(p1)-(std::int64_t(p0)+size-1)});
    };
    return static_cast<std::uint64_t>(gap(from.cell.x,a.width,p.x)+gap(from.cell.y,a.height,p.y))*5+
        static_cast<std::uint64_t>(c.edge_progress);
}
template<class Buildings,class Couriers>
void validate_culture(const OriginalCultureState& state,const Buildings& buildings,const Couriers& couriers,std::uint64_t tick,
              const OriginalHousingState& housing) {
    if (!state.next_performer_id || state.schools.size()>original_music_school_limit || state.venues.size()>original_culture_venue_limit)
        invalid("Music venue authority exceeds its fixed bounds");
    if (std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return b.kind==Object::OriginalMusicSchool211;})!=static_cast<std::ptrdiff_t>(state.schools.size()) ||
        std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return b.kind==Object::OriginalTheatre75;})!=static_cast<std::ptrdiff_t>(state.venues.size()) ||
        std::count_if(couriers.begin(),couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalMusicPerformer;})!=static_cast<std::ptrdiff_t>(state.schools.size()))
        invalid("Music venue body, school or movement-channel authority is incomplete");
    std::optional<std::uint64_t> first_admission;
    for (const auto& house:housing.houses) if (house.first_admission_tick) {
        if (!*house.first_admission_tick || *house.first_admission_tick>tick) invalid("Music training requires an actual past resident admission");
        if (!first_admission || *house.first_admission_tick<*first_admission) first_admission=house.first_admission_tick;
    }
    const auto training_start=[&](std::uint64_t placed) {return std::max(placed,first_admission.value_or(tick));};
    std::uint64_t spawned=0,latest=0;
    std::array<PerformerId,original_music_school_limit> latest_ids{};
    for (std::size_t i=0;i<state.schools.size();++i) {
        const auto& row=state.schools[i];const auto& source=body(buildings,row.building_id);const auto& c=body(couriers,row.courier_id);
        if ((i && !(state.schools[i-1].building_id<row.building_id)) || source.kind!=Object::OriginalMusicSchool211 ||
            !source.placed || source.placed_tick>tick || c.owner!=source.id || c.role!=CourierRole::OriginalMusicPerformer ||
            !c.enabled || c.good!=Good::Goods || c.cargo || c.reserved || c.last_dispatched_pottery ||
            row.spawn_counter>3 || row.spawned>row.attempts)
            invalid("Music school lost its paid identity, own channel or bounded counter");
        const auto cycles=countdowns(tick)-countdowns(training_start(source.placed_tick));
        if (row.spawn_counter>cycles || row.attempts>(cycles-row.spawn_counter)/4 || spawned>UINT64_MAX-row.spawned)
            invalid("Music attempts predate possible eligible training cycles");
        spawned+=row.spawned;
        if (!row.latest_trip) {
            if (row.spawned || c.phase!=CourierPhase::IdleAtWorkshop || c.last_dispatched_target || c.target!=source.id ||
                !c.path.empty() || c.path_vertex || c.edge_progress || c.route_pending || c.route_checked_revision || c.reroute_attempts)
                invalid("Unspawned music channel owns a phantom performer or destination");
            continue;
        }
        const auto& trip=*row.latest_trip;const auto& target=body(buildings,trip.venue_id);
        (void)owned_row(state.venues,target.id);
        if (!row.spawned || !value(trip.performer_id) || value(trip.performer_id)>=state.next_performer_id ||
            value(trip.performer_id)<row.spawned || !culture_cycle(trip.dispatched_tick) || trip.dispatched_tick>tick ||
            trip.dispatched_tick<=source.placed_tick || trip.dispatched_tick<=target.placed_tick || target.kind!=Object::OriginalTheatre75 ||
            c.target!=target.id || c.last_dispatched_target!=target.id)
            invalid("Music performer has no possible actual spawn or admitted venue");
        if (!first_admission || trip.dispatched_tick<=training_start(source.placed_tick) ||
            row.spawned>(countdowns(trip.dispatched_tick)-countdowns(training_start(source.placed_tick)))/4)
            invalid("Music performer predates the required eligible training attempts");
        for (std::size_t j=0;j<i;++j) if (latest_ids[j]==trip.performer_id)
            invalid("Music performer identity is owned by multiple schools");
        latest_ids[i]=trip.performer_id;latest=std::max(latest,value(trip.performer_id));
        if (trip.arrival_tick) {
            if (*trip.arrival_tick<=trip.dispatched_tick || *trip.arrival_tick>tick ||
                *trip.arrival_tick-trip.dispatched_tick<travel(source,target)-1 || c.phase!=CourierPhase::IdleAtWorkshop ||
                !c.path.empty() || c.path_vertex || c.edge_progress || c.route_pending || c.route_checked_revision)
                invalid("Retired music performer has impossible arrival, return or active movement");
            if (trip.accepted) {
                const auto& venue=owned_row(state.venues,target.id);
                if (!venue.last_arrival || venue.last_arrival->arrival_tick<*trip.arrival_tick)
                    invalid("Accepted performer lost its actual venue arrival witness");
                if (venue.last_arrival->performer_id==trip.performer_id &&
                    (venue.last_arrival->school_id!=source.id || venue.last_arrival->courier_id!=c.id ||
                     venue.last_arrival->dispatched_tick!=trip.dispatched_tick || venue.last_arrival->arrival_tick!=*trip.arrival_tick))
                    invalid("Music trip disagrees with its retained venue arrival");
            }
        } else {
            if (trip.accepted || c.phase!=CourierPhase::ToWarehouse || tick-trip.dispatched_tick+1<movement(source,c) ||
                owned_row(state.venues,target.id).music_timer)
                invalid("Active performer has a phantom grant, occupied slot or impossible physical progress");
            for (std::size_t j=0;j<i;++j) if (state.schools[j].latest_trip && !state.schools[j].latest_trip->arrival_tick &&
                state.schools[j].latest_trip->venue_id==target.id)
                invalid("Multiple active performers cannot own one venue admission");
        }
    }
    if (spawned==UINT64_MAX || state.next_performer_id!=spawned+1 || latest!=spawned)
        invalid("Music performer IDs lost their monotone successful-spawn highwater");
    for (std::size_t i=0;i<state.venues.size();++i) {
        const auto& row=state.venues[i];const auto& venue=body(buildings,row.building_id);
        if ((i && !(state.venues[i-1].building_id<row.building_id)) || venue.kind!=Object::OriginalTheatre75 ||
            !venue.placed || venue.placed_tick>tick || row.music_timer>32)
            invalid("Music venue has no unique paid Theatre75 identity");
        std::uint8_t expected=0;
        if (row.last_arrival) {
            const auto& w=*row.last_arrival;const auto& school=owned_row(state.schools,w.school_id);const auto& source=body(buildings,w.school_id);
            for (std::size_t j=0;j<i;++j) if (state.venues[j].last_arrival &&
                state.venues[j].last_arrival->performer_id==w.performer_id)
                invalid("One music performer cannot supply multiple venues");
            const auto& c=body(couriers,w.courier_id);
            if (!value(w.performer_id) || value(w.performer_id)>=state.next_performer_id || school.courier_id!=c.id || c.owner!=source.id ||
                !culture_cycle(w.dispatched_tick) || w.dispatched_tick<=source.placed_tick || w.dispatched_tick<=venue.placed_tick ||
                w.arrival_tick<=w.dispatched_tick || w.arrival_tick>tick || w.arrival_tick-w.dispatched_tick<travel(source,venue)-1 ||
                !school.latest_trip || value(w.performer_id)>value(school.latest_trip->performer_id))
                invalid("Venue timer has no possible paid performer arrival");
            if (!first_admission || w.dispatched_tick<=training_start(source.placed_tick) ||
                countdowns(w.dispatched_tick)-countdowns(training_start(source.placed_tick))<4)
                invalid("Venue arrival predates its performer's training attempts");
            const auto& trip=*school.latest_trip;
            if (trip.performer_id==w.performer_id) {
                if (!trip.accepted || trip.venue_id!=venue.id || trip.arrival_tick!=w.arrival_tick || trip.dispatched_tick!=w.dispatched_tick)
                    invalid("Venue witness contradicts its actual performer trip");
            } else if (trip.dispatched_tick<=w.arrival_tick)
                invalid("Compacted venue arrival postdates its school's later spawn");
            const auto elapsed=countdowns(tick)-countdowns(w.arrival_tick-1);
            expected=elapsed>=32 ? 0:static_cast<std::uint8_t>(32-elapsed);
        }
        if (row.music_timer!=expected) invalid("Sole venue timer differs from actual arrival and authored countdowns");
    }
    // At most four retained lifetime observations. Check identities and spawn
    // order, not a reconstructed route or an unbounded historical actor log.
    struct Observation {PerformerId id;BuildingId school,venue;CourierId courier;std::uint64_t dispatch;};
    std::array<Observation,original_music_school_limit+original_culture_venue_limit> observations{};
    std::size_t count=0;
    for (const auto& school:state.schools) if (school.latest_trip)
        observations[count++]={school.latest_trip->performer_id,school.building_id,school.latest_trip->venue_id,school.courier_id,school.latest_trip->dispatched_tick};
    for (const auto& venue:state.venues) if (venue.last_arrival) {
        const auto& w=*venue.last_arrival;observations[count++]={w.performer_id,w.school_id,venue.building_id,w.courier_id,w.dispatched_tick};
    }
    for (std::size_t i=0;i<count;++i) for (std::size_t j=0;j<i;++j) {
        const auto& a=observations[i];const auto& b=observations[j];
        if (a.id==b.id) {
            if (a.school!=b.school || a.venue!=b.venue || a.courier!=b.courier || a.dispatch!=b.dispatch)
                invalid("One performer identity cannot change school, channel, venue or spawn time");
        } else {
            const auto& first=a.id<b.id ? a:b;const auto& last=a.id<b.id ? b:a;
            if (first.dispatch>last.dispatch || (first.dispatch==last.dispatch && !(first.courier<last.courier)))
                invalid("Performer identities contradict actual monotone dispatch order");
        }
    }
    for (const auto& school:state.schools) {
        std::uint64_t retained=school.latest_trip ? 1:0;
        for (const auto& venue:state.venues) if (venue.last_arrival && venue.last_arrival->school_id==school.building_id &&
            (!school.latest_trip || venue.last_arrival->performer_id!=school.latest_trip->performer_id)) ++retained;
        if (retained>school.spawned) invalid("Retained performer lifetimes exceed their school's actual spawns");
    }
}
}

World World::create_original_music_venues(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!model || !model->supports_building(original::ModelBuilding::TheatrePavilion)) invalid("Music venues require separately prepared Theatre75 model capability");
    auto world=create_original_deadline_food_dispatch(std::move(permissions),std::move(model),std::move(terrain));
    world.rule_version_=kOriginalMusicVenuesRuleVersion;world.original_production_->culture.emplace();
    world.validate_original_music_venues_model();return world;
}
World World::restore_original_music_venues(const WorldSnapshot& state,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (state.profile!=RulesProfile::CityV16 || state.rule_version!=kOriginalMusicVenuesRuleVersion || !permissions ||
        permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain ||
        !model->supports_building(original::ModelBuilding::TheatrePavilion)) invalid("Music venue restore requires exact Rule22, policy3 and prepared model/terrain");
    validate_original_production_snapshot(state);
    auto world=restore_impl(state,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();world.validate_original_music_venues_model();return world;
}
void World::validate_original_music_venues_model() const {
    if (!original_music_venues_enabled()) return;
    if (!original_rules_model_ || !original_rules_model_->supports_building(original::ModelBuilding::TheatrePavilion) ||
        original_rules_model_->building_value(original::ModelBuilding::TheatrePavilion,original::BuildingField::Cost)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::TheatrePavilion,original::BuildingField::Employees)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::TheatrePavilion,original::BuildingField::Employees)>INT16_MAX)
        invalid("Paid Theatre75 requires positive checked model price and Employees");
    // Restore-only necessary workforce bounds. These optimistic ceilings use
    // actual first admissions and the authored maximum two residents per40
    // ticks; they do not reconstruct historical priorities, pauses or routes.
    const auto school_workers=static_cast<std::uint64_t>(original_rules_model_->building_value(
        original::ModelBuilding::MusicSchool,original::BuildingField::Employees));
    const auto venue_workers=static_cast<std::uint64_t>(original_rules_model_->building_value(
        original::ModelBuilding::TheatrePavilion,original::BuildingField::Employees));
    std::uint64_t maximum_capacity=0;
    for (std::size_t i=0;i<original::house_row_count;++i) {
        const auto cap=original_rules_model_->house_rules().row_capacity(static_cast<original::ModelHouse>(i));
        if (cap<0 || cap>INT16_MAX) invalid("Music workforce requires checked resident capacities");
        maximum_capacity=std::max(maximum_capacity,static_cast<std::uint64_t>(cap));
    }
    if (!school_workers || school_workers>INT16_MAX || !maximum_capacity)
        invalid("Music workforce requires bounded positive model Employees and capacity");
    const auto enough_pool=[&](std::uint64_t at,std::uint64_t required) {
        for (const auto& house:original_production_->housing.houses) {
            if (!house.first_admission_tick || at<*house.first_admission_tick) continue;
            const auto intervals=(at-*house.first_admission_tick)/40;
            // Saturate before multiplication; even UINT64_MAX cannot overflow.
            const auto possible=intervals>=(maximum_capacity+1)/2-1 ? maximum_capacity:2*(1+intervals);
            if (possible>=required) return true;
            required-=possible;
        }
        return required==0;
    };
    std::optional<std::uint64_t> first_possible_staff;
    if (enough_pool(ticks_,school_workers)) {
        std::uint64_t low=0,high=ticks_;
        while (low<high) {
            const auto middle=low+(high-low)/2;
            if (enough_pool(middle,school_workers)) high=middle;else low=middle+1;
        }
        first_possible_staff=low;
    }
    const auto eligible=[&](std::uint64_t at,BuildingId source) {
        if (!first_possible_staff) return std::uint64_t{0};
        const auto floor=std::max(building(source).placed_tick,*first_possible_staff);
        return at<=floor ? std::uint64_t{0}:countdowns(at)-countdowns(floor);
    };
    const auto& state=*original_production_->culture;
    for (const auto& school:state.schools) {
        const auto cycles=eligible(ticks_,school.building_id);
        if (school.spawn_counter>cycles || school.attempts>(cycles-school.spawn_counter)/4 ||
            (school.latest_trip && school.spawned>eligible(school.latest_trip->dispatched_tick,school.building_id)/4))
            invalid("Music training predates possible complete model staffing");
    }
    struct StaffTrip {PerformerId id;BuildingId school,venue;std::uint64_t dispatch;};
    std::array<StaffTrip,original_music_school_limit+original_culture_venue_limit> trips{};
    std::size_t count=0;
    const auto retain=[&](StaffTrip trip) {
        for (std::size_t i=0;i<count;++i) if (trips[i].id==trip.id) return;
        trips[count++]=trip;
    };
    for (const auto& school:state.schools) if (school.latest_trip) {
        const auto& trip=*school.latest_trip;retain({trip.performer_id,school.building_id,trip.venue_id,trip.dispatched_tick});
    }
    for (const auto& venue:state.venues) if (venue.last_arrival) {
        const auto& w=*venue.last_arrival;retain({w.performer_id,w.school_id,venue.building_id,w.dispatched_tick});
    }
    for (std::size_t i=0;i<count;++i) {
        if (eligible(trips[i].dispatch,trips[i].school)<4)
            invalid("Venue arrival predates possible complete School training");
        std::uint64_t required=0;
        for (std::size_t j=0;j<count;++j) if (trips[j].dispatch==trips[i].dispatch) {
            for (std::size_t k=0;k<j;++k) if (trips[k].dispatch==trips[j].dispatch &&
                (trips[k].school==trips[j].school || trips[k].venue==trips[j].venue))
                invalid("Simultaneous performers cannot share a School or Venue admission");
            required+=school_workers+venue_workers;
        }
        // Staffing is captured before this tick's immigration and movement.
        if (!enough_pool(trips[i].dispatch-1,required))
            invalid("Actual performer spawn exceeds possible simultaneous paid workforce");
    }
}
void World::validate_original_music_venues_snapshot(const WorldSnapshot& s) {
    const bool enabled=simulation::original_music_venues_enabled(s.profile,s.rule_version);
    if (!enabled) {
        if ((s.original_production && s.original_production->culture) ||
            std::any_of(s.buildings.begin(),s.buildings.end(),[](const auto& b){return b.kind==Object::OriginalTheatre75;}) ||
            std::any_of(s.couriers.begin(),s.couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalMusicPerformer;}))
            invalid("Historical generation cannot acquire venue or performer authority");
        return;
    }
    if (!s.original_production || !s.original_production->culture || s.original_production->safe_demolition)
        invalid("Rule22 requires its own culture authority and no retirement journal");
    validate_culture(*s.original_production->culture,s.buildings,s.couriers,s.ticks,s.original_production->housing);
    validate_original_venue_house_service_snapshot(s);
}
void World::validate_original_music_venues_state() const {
    if (!original_music_venues_enabled()) return;
    if (!original_production_ || !original_production_->culture) invalid("Missing actual music venue state");
    validate_culture(*original_production_->culture,buildings_,couriers_,ticks_,original_production_->housing);
}
CourierDispatchDecision World::original_music_dispatch_status(const CourierState& c) const {
    if (!original_music_venues_enabled() || !c.enabled) return {CourierDispatchStatus::Disabled,{}};
    if (c.phase!=CourierPhase::IdleAtWorkshop) return {c.route_pending ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::AlreadyMoving,c.target};
    const auto& source=building(c.owner);
    if (building_on_fire(source.id)) return {CourierDispatchStatus::OnFire,{}};
    if (!source.operating_enabled) return {CourierDispatchStatus::OperationPaused,{}};
    if (!building_staffed_for_tick(source.id)) return {CourierDispatchStatus::Unstaffed,{}};
    if (c.cached_revision!=road_revision_) return {CourierDispatchStatus::NoRoad,{}};
    const auto& venues=original_production_->culture->venues;
    std::size_t first=0;for (std::size_t i=0;i<venues.size();++i) if (c.last_dispatched_target==venues[i].building_id) first=(i+1)%venues.size();
    bool available=false;
    for (std::size_t n=0;n<venues.size();++n) {
        const auto& row=venues[(first+n)%venues.size()];const auto& venue=building(row.building_id);
        if (row.music_timer || !venue.operating_enabled || building_on_fire(venue.id) || !building_staffed_for_tick(venue.id)) continue;
        const bool incoming=std::any_of(original_production_->culture->schools.begin(),original_production_->culture->schools.end(),[&](const auto& school){
            return school.latest_trip && !school.latest_trip->arrival_tick && school.latest_trip->venue_id==venue.id;
        });
        if (incoming) continue;
        available=true;
        const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),venue.id,[](const auto& row0,BuildingId id){return row0.first<id;});
        if (route!=c.dynamic_target_routes.end() && route->first==venue.id && route->second) return {CourierDispatchStatus::Ready,venue.id};
    }
    return {available ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::NoTarget,{}};
}
void World::dispatch_original_music_performer(CourierState& c) {
    if (!original_music_venues_enabled() || !culture_cycle(ticks_) || !c.enabled || c.phase!=CourierPhase::IdleAtWorkshop) return;
    const auto& source=building(c.owner);
    if (!source.operating_enabled || building_on_fire(source.id) || !building_staffed_for_tick(source.id) ||
        c.cached_revision!=road_revision_ || building_entrances(source.id).empty()) return;
    auto& state=*original_production_->culture;auto& school=owned_row(state.schools,source.id);
    const auto step=original::service_spawn_step(original::SpawnBuilding::MusicSchool,100,school.spawn_counter,true);
    if (!step.attempt_due) {school.spawn_counter=step.counter;return;}
    const auto choice=original_music_dispatch_status(c);
    std::vector<Cell> path;
    if (choice.status==CourierDispatchStatus::Ready && choice.selected_target) {
        const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),*choice.selected_target,[](const auto& row,BuildingId id){return row.first<id;});
        if (route==c.dynamic_target_routes.end() || !route->second) throw std::logic_error("Admitted music performer lost its prepared route");
        path=*route->second;
    }
    if (school.attempts==UINT64_MAX || (!path.empty() && (school.spawned==UINT64_MAX || state.next_performer_id==UINT64_MAX)))
        throw std::overflow_error("Music performer attempt or identity exhausted");
    school.spawn_counter=step.counter;++school.attempts;
    if (path.empty()) return; // A due attempt has created no Actor, target or timer.
    const auto target=*choice.selected_target;const auto id=static_cast<PerformerId>(state.next_performer_id++);
    ++school.spawned;school.latest_trip=OriginalMusicTrip{id,target,ticks_};
    c.path=std::move(path);c.path_vertex=0;c.edge_progress=0;c.target=target;c.last_dispatched_target=target;
    c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;
}
void World::arrive_original_music_performer(CourierState& c) {
    auto& state=*original_production_->culture;auto& school=owned_row(state.schools,c.owner);
    if (!school.latest_trip || school.latest_trip->arrival_tick || school.latest_trip->venue_id!=c.target || c.phase!=CourierPhase::ToWarehouse)
        throw std::logic_error("Music performer arrival is stale, duplicated or orphaned");
    auto& trip=*school.latest_trip;auto& venue=owned_row(state.venues,c.target);const auto& target=building(c.target);
    trip.arrival_tick=ticks_;
    if (!venue.music_timer && target.operating_enabled && !building_on_fire(target.id) && building_staffed_for_tick(target.id)) {
        venue.music_timer=32;trip.accepted=true;
        venue.last_arrival=OriginalMusicArrival{trip.performer_id,c.owner,c.id,trip.dispatched_tick,ticks_};
        if (original_venue_house_service_enabled()) {
            auto& source=owned_row(state.venue_house_service->sources,venue.building_id);
            if (!source.first_music) source.first_music=venue.last_arrival;
        }
    }
    // The actual one-way Actor ends here. The idle fixed channel has no visible
    // position or performer identity; the next real spawn gets a new ID.
    c.path.clear();c.path_vertex=0;c.edge_progress=0;c.phase=CourierPhase::IdleAtWorkshop;
    c.route_pending=false;c.route_checked_revision.reset();
}
void World::decay_original_music_venues() {
    if (original_music_venues_enabled()) for (auto& venue:original_production_->culture->venues)
        if (venue.music_timer) --venue.music_timer;
}
} // namespace openemperor::simulation
