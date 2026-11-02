#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
bool service_role(CourierRole role) {
    return role==CourierRole::Service || role==CourierRole::HealthWorker || role==CourierRole::OriginalAncestor ||
        role==CourierRole::OriginalDaoist || role==CourierRole::OriginalBuddhist || role==CourierRole::OriginalConfucian ||
        role==CourierRole::OriginalMusic || role==CourierRole::OriginalAcrobat || role==CourierRole::OriginalDrama;
}
bool supported_role(original::ServiceRole role) {
    return role==original::ServiceRole::Herbalist || role==original::ServiceRole::Acupuncture ||
        role==original::ServiceRole::Ancestor || role==original::ServiceRole::Confucian ||
        role==original::ServiceRole::Daoist || role==original::ServiceRole::Buddhist ||
        role==original::ServiceRole::Music || role==original::ServiceRole::Acrobat || role==original::ServiceRole::Drama;
}
original::ServiceRole role_for(OriginalServiceProviderType type) {
    switch (type) {
    case OriginalServiceProviderType::Herbalist207:return original::ServiceRole::Herbalist;
    case OriginalServiceProviderType::Acupuncture208:return original::ServiceRole::Acupuncture;
    case OriginalServiceProviderType::Ancestor214:return original::ServiceRole::Ancestor;
    case OriginalServiceProviderType::Daoist215:return original::ServiceRole::Daoist;
    case OriginalServiceProviderType::Buddhist217:return original::ServiceRole::Buddhist;
    case OriginalServiceProviderType::Confucian219:return original::ServiceRole::Confucian;
    case OriginalServiceProviderType::Music211:return original::ServiceRole::Music;
    case OriginalServiceProviderType::Acrobat212:return original::ServiceRole::Acrobat;
    case OriginalServiceProviderType::Drama213:return original::ServiceRole::Drama;
    }
    invalid("unsupported original service Provider identity");
}
Object kind_for(OriginalServiceProviderType type) {
    switch (type) {
    case OriginalServiceProviderType::Herbalist207:return Object::HealthPost;
    case OriginalServiceProviderType::Acupuncture208:return Object::ServicePost;
    case OriginalServiceProviderType::Ancestor214:return Object::OriginalAncestorShrine214;
    case OriginalServiceProviderType::Daoist215:return Object::OriginalDaoistShrine215;
    case OriginalServiceProviderType::Buddhist217:return Object::OriginalBuddhistShrine217;
    case OriginalServiceProviderType::Confucian219:return Object::OriginalConfucianAcademy219;
    case OriginalServiceProviderType::Music211:return Object::OriginalMusicSchool211;
    case OriginalServiceProviderType::Acrobat212:return Object::OriginalAcrobatSchool212;
    case OriginalServiceProviderType::Drama213:return Object::OriginalDramaSchool213;
    }
    invalid("unsupported original service Provider body");
}
CourierRole courier_role_for(OriginalServiceProviderType type) {
    switch (type) {
    case OriginalServiceProviderType::Herbalist207:return CourierRole::HealthWorker;
    case OriginalServiceProviderType::Acupuncture208:return CourierRole::Service;
    case OriginalServiceProviderType::Ancestor214:return CourierRole::OriginalAncestor;
    case OriginalServiceProviderType::Daoist215:return CourierRole::OriginalDaoist;
    case OriginalServiceProviderType::Buddhist217:return CourierRole::OriginalBuddhist;
    case OriginalServiceProviderType::Confucian219:return CourierRole::OriginalConfucian;
    case OriginalServiceProviderType::Music211:return CourierRole::OriginalMusic;
    case OriginalServiceProviderType::Acrobat212:return CourierRole::OriginalAcrobat;
    case OriginalServiceProviderType::Drama213:return CourierRole::OriginalDrama;
    }
    invalid("unsupported original service Provider actor");
}
original::ServiceVisit visit_for_role(original::ServiceRole role) {
    // Restore the initial original Confucian elite-only condition. The
    // physical Paper input/tour sink and occupied targeted dispatch remain
    // explicitly authored; no original global27/10 selector is inferred.
    return {role>=original::ServiceRole::Ancestor ? original::VisitPath::DirectReligion:
        original::VisitPath::DirectService,static_cast<original::VisitAction>(role),role==original::ServiceRole::Confucian};
}
original::ServiceVisit visit_for(OriginalServiceProviderType type) {return visit_for_role(role_for(type));}
template<class Rows> decltype(auto) building_row(Rows& rows,BuildingId id) {
    const auto found=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.id<key;});
    if (found==rows.end() || found->id!=id) invalid("original service refers to a missing paid body");return *found;
}
template<class Rows> decltype(auto) house_row(Rows& rows,BuildingId id) {
    const auto found=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.building_id<key;});
    if (found==rows.end() || found->building_id!=id) invalid("original service refers to a missing paid House");return *found;
}
const OriginalServiceProvider& provider(const OriginalServicesState& state,BuildingId owner) {
    const auto found=std::lower_bound(state.providers.begin(),state.providers.end(),owner,[](const auto& row,BuildingId key){return row.building_id<key;});
    if (found==state.providers.end() || found->building_id!=owner) invalid("original service source lost its explicit paid Provider identity");return *found;
}
template<class Rows> auto latest_trip(Rows& rows,CourierId actor) {
    const auto found=std::lower_bound(rows.begin(),rows.end(),actor,[](const auto& row,CourierId key){return row.transfer.courier_id<key;});
    return found==rows.end() || found->transfer.courier_id!=actor ? nullptr:&*found;
}
auto witness_key(const OriginalServiceWitness& row) {return std::pair{row.building_id,row.role};}
const OriginalServiceWitness* witness_for(const OriginalServicesState& state,BuildingId house,original::ServiceRole role) {
    const auto key=std::pair{house,role};
    const auto found=std::lower_bound(state.witnesses.begin(),state.witnesses.end(),key,[](const auto& row,const auto& value){return witness_key(row)<value;});
    return found==state.witnesses.end() || witness_key(*found)!=key ? nullptr:&*found;
}
std::uint64_t countdowns(std::uint64_t through) {return through<36 ? 0:1+(through-36)/51;}
original::ServiceHouseContext context(const OriginalHouseState& house) {return {house.original_status,house.access_enabled};}
bool eligible(const OriginalHouseState& house,original::ServiceVisit visit) {
    return house.house.occupants>0 && original::accepts_service_visit(house.house,context(house),visit);
}
template<class Building> std::uint64_t minimum_travel_ticks(const Building& source,const Building& target) {
    // Use both real paid footprints, including Confucian219's3x3 body. The shared navigator
    // requires a source-body edge, at least one road cell and a target-body
    // edge. Manhattan distance is only a necessary lower bound; saved routes
    // may have waited or replanned and are not a retained travel chronicle.
    const auto from=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,source.kind);
    const auto to=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,target.kind);
    const auto gap=[](int a,int a_size,int b,int b_size) {
        const auto first=static_cast<std::int64_t>(a),second=static_cast<std::int64_t>(b);
        return std::max({std::int64_t{0},first-(second+b_size-1),second-(first+a_size-1)});
    };
    const auto edges=static_cast<std::uint64_t>(std::max<std::int64_t>(2,
        gap(source.cell.x,from.width,target.cell.x,to.width)+gap(source.cell.y,from.height,target.cell.y,to.height)));
    return edges*static_cast<std::uint64_t>(
        profile_rules(RulesProfile::CityV16,kOriginalProductionRuleVersion).courier_edge_ticks);
}
template<class Building,class Courier> std::uint64_t minimum_movement_ticks(const Building& start,const Courier& actor) {
    const auto edge_ticks=profile_rules(RulesProfile::CityV16,kOriginalProductionRuleVersion).courier_edge_ticks;
    if (actor.path_vertex>=actor.path.size() || actor.edge_progress<0 || actor.edge_progress>=edge_ticks)
        invalid("original service movement has no valid current physical vertex/progress");
    const auto current=actor.path[actor.path_vertex];
    const auto footprint=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,start.kind);
    const auto gap=[](int origin,int size,int coordinate) {
        const auto delta=static_cast<std::int64_t>(coordinate)-static_cast<std::int64_t>(origin);
        return delta<0 ? -delta:std::max<std::int64_t>(0,delta-size+1);
    };
    const auto edges=static_cast<std::uint64_t>(gap(start.cell.x,footprint.width,current.x)+gap(start.cell.y,footprint.height,current.y));
    // A replan may shorten the saved path prefix, never the physical distance
    // already covered or progress on the current begun edge. No historical
    // route is reconstructed from current road topology/path_vertex.
    return edges*static_cast<std::uint64_t>(edge_ticks)+static_cast<std::uint64_t>(actor.edge_progress);
}

template<class Buildings,class Couriers,class Houses>
void validate_services(const OriginalServicesState& state,const Buildings& buildings,const Couriers& couriers,const Houses& houses,const OriginalHousingState& housing,
                       std::uint64_t tick,std::uint64_t decayed_through,bool bind_movement=true) {
    if (state.providers.size()>original_service_provider_limit || state.trips.size()>original_service_trip_limit ||
        state.witnesses.size()>original_service_witness_limit || decayed_through>tick)
        invalid("original service bounded authority exceeded");
    std::array<std::size_t,9> counts{};
    std::size_t source_count=0,actor_count=0;
    for (const auto& b:buildings) source_count+=b.kind==Object::HealthPost || b.kind==Object::ServicePost ||
        b.kind==Object::OriginalAncestorShrine214 || b.kind==Object::OriginalDaoistShrine215 ||
        b.kind==Object::OriginalBuddhistShrine217 || b.kind==Object::OriginalConfucianAcademy219 ||
        b.kind==Object::OriginalMusicSchool211 || b.kind==Object::OriginalAcrobatSchool212 || b.kind==Object::OriginalDramaSchool213;
    for (const auto& c:couriers) actor_count+=service_role(c.role);
    if (source_count!=state.providers.size() || actor_count!=source_count) invalid("paid original service Provider/Courier authority is incomplete");
    for (std::size_t i=0;i<state.providers.size();++i) {
        const auto& p=state.providers[i];(void)role_for(p.type);const auto& b=building_row(buildings,p.building_id);
        if ((i && !(state.providers[i-1].building_id<p.building_id)) || b.kind!=kind_for(p.type) || !b.placed || b.placed_tick>tick)
            invalid("original service Providers require unique sorted matching paid bodies");
        if (++counts[static_cast<std::size_t>(p.type)]>2) invalid("original service Provider type limit exceeded");
        const auto matching=std::count_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.owner==b.id && c.role==courier_role_for(p.type);});
        if (matching!=1) invalid("original service Provider requires exactly its own real Courier");
    }
    for (std::size_t i=0;i<state.trips.size();++i) {
        const auto& trip=state.trips[i];
        if ((i && !(state.trips[i-1].transfer.courier_id<trip.transfer.courier_id)) || !trip.transfer.sequence ||
            trip.transfer.sequence>trip.dispatched_tick || !trip.dispatched_tick || trip.dispatched_tick>tick)
            invalid("original service trips require bounded latest real dispatch sequences");
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==trip.transfer.courier_id;});
        if (actor==couriers.end() || !service_role(actor->role)) invalid("original service trip has no compatible actual Courier");
        const auto& p=provider(state,actor->owner);const auto& source=building_row(buildings,p.building_id);
        const auto& target=building_row(buildings,trip.target);const auto& h=house_row(houses,trip.target);
        const auto& first_admission=house_row(housing.houses,trip.target).first_admission_tick;
        if (actor->role!=courier_role_for(p.type) || !original_house_kind(target.kind) ||
            trip.dispatched_tick<=source.placed_tick || trip.dispatched_tick<target.placed_tick || trip.dispatched_tick-target.placed_tick<121 ||
            !first_admission || trip.dispatched_tick<=*first_admission ||
            !eligible(h,visit_for(p.type)))
            invalid("original service dispatch predates or mismatches its paid source/occupied target");
        if (bind_movement && (actor->target!=trip.target || actor->last_dispatched_target!=trip.target))
            invalid("original service latest trip lost its fixed actual destination");
        if (trip.arrival_tick) {
            if (*trip.arrival_tick<=trip.dispatched_tick || *trip.arrival_tick>tick) invalid("original service arrival must follow actual dispatch");
            if (*trip.arrival_tick-trip.dispatched_tick<minimum_travel_ticks(source,target)-1)
                invalid("original service arrival predates the minimum physical trip time");
            if (bind_movement && actor->phase!=CourierPhase::Returning && actor->phase!=CourierPhase::IdleAtWorkshop)
                invalid("arrived original service Courier must be returning or home");
            if (bind_movement) {
                const auto elapsed=tick-*trip.arrival_tick;
                const auto required=actor->phase==CourierPhase::IdleAtWorkshop ? minimum_travel_ticks(source,target):
                    minimum_movement_ticks(target,*actor);
                if (elapsed<required) invalid("original service return movement predates its actual arrival");
            }
        } else if (trip.visit_applied || (bind_movement && actor->phase!=CourierPhase::ToWarehouse))
            invalid("original service visit exists without an actual outbound arrival");
        else if (bind_movement && tick-trip.dispatched_tick+1<minimum_movement_ticks(source,*actor))
            invalid("original service outbound movement predates its actual dispatch");
    }
    for (const auto& c:couriers) if (service_role(c.role)) {
        const auto& p=provider(state,c.owner);
        if (c.role!=courier_role_for(p.type) || c.good!=Good::Goods || c.cargo || c.reserved || !c.enabled)
            invalid("original service Courier cannot carry goods or change Provider category");
        const auto* trip=latest_trip(state.trips,c.id);
        if (!trip && (c.phase!=CourierPhase::IdleAtWorkshop || c.last_dispatched_target || c.target!=c.owner))
            invalid("used original service Courier lost its latest trip and sequence highwater");
    }
    for (std::size_t i=0;i<state.witnesses.size();++i) {
        const auto& witness=state.witnesses[i];
        if ((i && !(witness_key(state.witnesses[i-1])<witness_key(witness))) ||
            !supported_role(witness.role))
            invalid("original service Witnesses must be sorted unique House/category pairs");
        const auto& p=provider(state,witness.provider_id);const auto& source=building_row(buildings,p.building_id);
        const auto& target=building_row(buildings,witness.building_id);const auto& h=house_row(houses,witness.building_id);
        const auto& first_admission=house_row(housing.houses,witness.building_id).first_admission_tick;
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==witness.transfer.courier_id;});
        const auto* trip=latest_trip(state.trips,witness.transfer.courier_id);
        if (!original_house_kind(target.kind) || role_for(p.type)!=witness.role || actor==couriers.end() || actor->owner!=p.building_id ||
            actor->role!=courier_role_for(p.type) || !trip || !witness.transfer.sequence || witness.transfer.sequence>trip->transfer.sequence ||
            !witness.arrival_tick || witness.arrival_tick>tick || witness.transfer.sequence>witness.arrival_tick ||
            witness.arrival_tick<=source.placed_tick || witness.arrival_tick-source.placed_tick<2 ||
            witness.arrival_tick<target.placed_tick || witness.arrival_tick-target.placed_tick<122 || !first_admission || witness.arrival_tick<*first_admission ||
            !eligible(h,visit_for(p.type)))
            invalid("original service Witness has no possible paid direct-service arrival");
        if (trip->transfer==witness.transfer) {
            if (!trip->visit_applied || trip->target!=witness.building_id || trip->arrival_tick!=witness.arrival_tick)
                invalid("original service Witness disagrees with its retained successful arrival");
        } else if (witness.arrival_tick>=trip->dispatched_tick)
            invalid("compacted service visit cannot postdate its Courier's later dispatch");
        const auto travel=minimum_travel_ticks(source,target);
        if (witness.arrival_tick-source.placed_tick<travel || witness.arrival_tick-target.placed_tick<travel+120 || witness.arrival_tick-*first_admission<travel)
            invalid("original service Witness predates possible physical travel after purchase and immigration");
    }
    // Every retained successful arrival remains represented unless a later
    // actual visit of this category superseded it. Same-tick movement follows
    // the ordinary ascending Courier-ID loop, so the last such actor wins.
    for (const auto& trip:state.trips) if (trip.visit_applied) {
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==trip.transfer.courier_id;});
        const auto role=role_for(provider(state,actor->owner).type);const auto* witness=witness_for(state,trip.target,role);
        if (!witness || std::pair{witness->arrival_tick,witness->transfer.courier_id}<std::pair{*trip.arrival_tick,trip.transfer.courier_id})
            invalid("original service Witness discarded a newer retained successful visit");
    }
    for (const auto& h:houses) {
        for (const auto role:{original::ServiceRole::Herbalist,original::ServiceRole::Acupuncture,original::ServiceRole::Ancestor,
            original::ServiceRole::Confucian,original::ServiceRole::Daoist,original::ServiceRole::Buddhist,
            original::ServiceRole::Music,original::ServiceRole::Acrobat,original::ServiceRole::Drama}) {
            const auto* witness=witness_for(state,h.building_id,role);std::uint8_t expected=0;
            if (witness) {
                const auto before=countdowns(witness->arrival_tick-1),after=countdowns(decayed_through);
                if (after<before) invalid("service Witness countdown precedes its actual arrival");
                const auto elapsed=after-before;const auto duration=original::service_duration(role);
                expected=elapsed>=duration ? 0:static_cast<std::uint8_t>(duration-elapsed);
            }
            if (original::service_timer(h.house,role)!=expected) invalid("sole original House timer disagrees with actual arrival and phase35 countdowns");
        }
    }
}
}

OriginalServiceAvailability World::original_service_availability(BuildingId house,original::ServiceRole role) const {
    if (!supported_role(role)) invalid("unsupported original service inspection category");
    OriginalServiceAvailability result;
    if (!original_production_enabled() || !original_production_ || !original_economy_) return result;

    const auto& houses=original_economy_->houses;
    const auto home=std::lower_bound(houses.begin(),houses.end(),house,
        [](const auto& value,BuildingId id){return value.building_id<id;});
    const auto body=std::lower_bound(buildings_.begin(),buildings_.end(),house,
        [](const auto& value,BuildingId id){return value.id<id;});
    if (home!=houses.end() && home->building_id==house && body!=buildings_.end() && body->id==house &&
        original_house_kind(body->kind) && body->placed) {
        const auto visit=visit_for_role(role);
        result.eligible_target=!building_on_fire(house) && eligible(*home,visit);
        result.delivered_timer=original::service_timer(home->house,role);
        if (const auto* witness=witness_for(original_production_->services,house,role))
            result.last_arrival_tick=witness->arrival_tick;
    }

    // Read the existing current allocation and prepared routes. No World or
    // authority snapshot is copied and no route/model/asset work is triggered.
    const auto staffing=workforce_allocation();
    for (const auto& p:original_production_->services.providers) {
        if (role_for(p.type)!=role) continue;
        ++result.provider_count;
        const auto source=std::lower_bound(buildings_.begin(),buildings_.end(),p.building_id,
            [](const auto& value,BuildingId id){return value.id<id;});
        if (source==buildings_.end() || source->id!=p.building_id) continue;
        const auto actor=std::find_if(couriers_.begin(),couriers_.end(),[&](const auto& value){
            return value.owner==p.building_id && value.role==courier_role_for(p.type);
        });
        if (actor==couriers_.end()) continue;
        const auto* trip=latest_trip(original_production_->services.trips,actor->id);
        if (actor->phase==CourierPhase::ToWarehouse && actor->target==house && trip && trip->target==house && !trip->arrival_tick)
            ++result.travelling_to_target_count;
        if (!source->placed || !source->operating_enabled || building_on_fire(source->id) ||
            !staffing[static_cast<std::size_t>(source-buildings_.begin())] ||
            (p.type==OriginalServiceProviderType::Confucian219 && !original_academy_paper_available(source->id))) continue;
        ++result.available_source_count;
        if (!result.eligible_target || actor->cached_revision!=road_revision_) continue;
        const auto route=std::lower_bound(actor->dynamic_target_routes.begin(),actor->dynamic_target_routes.end(),house,
            [](const auto& value,BuildingId id){return value.first<id;});
        if (route!=actor->dynamic_target_routes.end() && route->first==house && route->second)
            ++result.reachable_source_count;
    }
    return result;
}

void World::validate_original_services_model() const {
    if (!original_rules_model_) invalid("original service Providers require a bound prepared model");
    for (const auto kind:{original::ModelBuilding::HerbalistsStall,original::ModelBuilding::AcupuncturistsClinic,
        original::ModelBuilding::AncestralShrine,original::ModelBuilding::DaoistShrine,
        original::ModelBuilding::BuddhistShrine,original::ModelBuilding::ConfucianAcademy,
        original::ModelBuilding::MusicSchool,original::ModelBuilding::AcrobatSchool,original::ModelBuilding::DramaSchool}) {
        if (!original_rules_model_->supports_building(kind) || original_rules_model_->building_value(kind,original::BuildingField::Cost)<=0)
            invalid("original service Provider requires positive prepared model price/capability");
        const auto employees=original_rules_model_->building_value(kind,original::BuildingField::Employees);
        if (employees<=0 || employees>INT16_MAX) invalid("original service Provider requires positive bounded model Employees");
    }
}
void World::validate_original_services_snapshot(const WorldSnapshot& snapshot) {
    if (!snapshot.original_production || !snapshot.original_economy) invalid("original services require explicit production authority");
    validate_services(snapshot.original_production->services,snapshot.buildings,snapshot.couriers,snapshot.original_economy->houses,snapshot.original_production->housing,snapshot.ticks,snapshot.ticks);
}
void World::validate_original_services_state() const {
    validate_services(original_production_->services,buildings_,couriers_,original_economy_->houses,original_production_->housing,ticks_,ticks_);
}
bool World::original_service_courier(CourierRole role) const {return original_production_enabled() && service_role(role);}
CourierDispatchDecision World::original_service_dispatch_status(const CourierState& c) const {
    const auto& p=provider(original_production_->services,c.owner);
    if (c.role!=courier_role_for(p.type)) throw std::logic_error("original service dispatch has incompatible paid Provider");
    if (p.type==OriginalServiceProviderType::Confucian219 && !original_academy_paper_available(c.owner))
        return {CourierDispatchStatus::NoStock,{}};
    if (original_deadline_service_dispatch_enabled() || original_deadline_food_dispatch_enabled()) {
        const auto add=[](std::uint64_t a,std::uint64_t b) {
            return b>UINT64_MAX-a ? UINT64_MAX:a+b;
        };
        // Public queries see the completed tick; actual dispatch sees this
        // tick before movement/countdown. Both use the same physical phase.
        const auto dispatch_tick=tick_staffing_active_ ? ticks_:add(ticks_,1);
        const auto edge=static_cast<std::uint64_t>(active_rules().courier_edge_ticks);
        const auto role=role_for(p.type);
        struct Incoming {BuildingId target{};std::uint64_t arrival=0;};
        std::array<Incoming,original_service_provider_limit> incoming{};
        std::size_t incoming_count=0;
        for (const auto& trip:original_production_->services.trips) {
            if (trip.transfer.courier_id==c.id || trip.arrival_tick) continue;
            const auto actor=std::find_if(couriers_.begin(),couriers_.end(),[&](const auto& other) {
                return other.id==trip.transfer.courier_id;
            });
            if (actor==couriers_.end() || actor->role!=c.role || !actor->enabled ||
                actor->phase!=CourierPhase::ToWarehouse || actor->route_pending ||
                actor->target!=trip.target || actor->path.empty() ||
                actor->path_vertex+1>=actor->path.size()) continue;
            const auto& target=house_row(original_economy_->houses,trip.target);
            if (building_on_fire(trip.target) || !eligible(target,visit_for(p.type))) continue;
            const auto remaining=static_cast<std::uint64_t>(actor->path.size()-1-actor->path_vertex)*edge-
                static_cast<std::uint64_t>(actor->edge_progress);
            if (incoming_count<incoming.size())
                incoming[incoming_count++]={trip.target,add(dispatch_tick,remaining-1)};
        }
        // Keep the cursor in the complete canonical paid House list. A last
        // destination that burns stays an anchor, rather than resetting the
        // scan to the first healthy House and skipping later recipients.
        std::array<const BuildingState*,Rules::city_v10_household_limit> canonical{};
        std::size_t count=0,start=0;
        for (const auto& b:buildings_) if (b.placed && original_house_kind(b.kind)) canonical[count++]=&b;
        for (std::size_t i=0;i<count;++i) if (c.last_dispatched_target==canonical[i]->id) start=(i+1)%count;
        struct Choice {BuildingId target{};std::uint64_t deadline=0,arrival=0,roundtrip=0;};
        std::optional<Choice> selected;
        const auto earlier=[](const Choice& a,const Choice& b) {
            const bool a_late=a.arrival>a.deadline,b_late=b.arrival>b.deadline;
            // Unlike an expired fire incident, a late service visit still
            // renews real coverage. Restore uncovered/negative-laxity Houses
            // before refreshing positive timers; no lateness grants a visit.
            if (a_late!=b_late) return a_late;
            const auto a_gap=a_late ? a.arrival-a.deadline:a.deadline-a.arrival;
            const auto b_gap=b_late ? b.arrival-b.deadline:b.deadline-b.arrival;
            if (a_gap!=b_gap) return a_late ? a_gap>b_gap:a_gap<b_gap;
            if (a.deadline!=b.deadline) return a.deadline<b.deadline;
            if (a.roundtrip!=b.roundtrip) return a.roundtrip<b.roundtrip;
            return false; // Exact ties retain the stable canonical cursor.
        };
        bool unserved=false;
        for (std::size_t i=0;i<count;++i) {
            const auto& target=*canonical[(start+i)%count];
            const auto& home=house_row(original_economy_->houses,target.id);
            if (building_on_fire(target.id) || !eligible(home,visit_for(p.type))) continue;
            const auto* committed=static_cast<const Incoming*>(nullptr);
            for (std::size_t n=0;n<incoming_count;++n)
                if (incoming[n].target==target.id && (!committed || incoming[n].arrival<committed->arrival)) committed=&incoming[n];
            const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target.id,
                [](const auto& entry,BuildingId key){return entry.first<key;});
            if (route==c.dynamic_target_routes.end() || route->first!=target.id || !route->second || route->second->size()<2) {
                if (!committed) unserved=true;
                continue;
            }
            const auto edges=static_cast<std::uint64_t>(route->second->size()-1);
            if (!edge || edges>UINT64_MAX/edge/2) continue;
            const auto movement=edges*edge,arrival=add(dispatch_tick,movement-1);
            std::uint64_t deadline=0;
            if (const auto* witness=witness_for(original_production_->services,target.id,role)) {
                // The retained actual-arrival origin makes this exact on the
                // phase35 boundary both before and after this tick's decay.
                const auto index=add(countdowns(witness->arrival_tick-1),original::service_duration(role)-1U);
                deadline=index>UINT64_MAX/51 ? UINT64_MAX:add(36,index*51);
            } else {
                const auto& admitted=house_row(original_production_->housing.houses,target.id).first_admission_tick;
                if (!admitted) throw std::logic_error("Eligible service House has no real admission");
                deadline=*admitted;
            }
            // Moving visitors remain real after source pause/fire/staff loss.
            // A waiting, returning or merely planned actor promised nothing.
            if (committed && (committed->arrival<=deadline || committed->arrival<=arrival)) continue;
            unserved=true;
            const Choice candidate{target.id,deadline,arrival,movement*2};
            if (!selected || earlier(candidate,*selected)) selected=candidate;
        }
        if (selected) return {CourierDispatchStatus::Ready,selected->target};
        return {unserved ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::NoTarget,{}};
    }
    std::array<const BuildingState*,Rules::city_v10_household_limit> targets{};std::size_t count=0;
    for (const auto& b:buildings_) if (original_house_kind(b.kind) && !building_on_fire(b.id)) {
        const auto& h=house_row(original_economy_->houses,b.id);
        if (eligible(h,visit_for(p.type))) targets[count++]=&b;
    }
    if (!count) return {CourierDispatchStatus::NoTarget,{}};std::size_t start=0;
    for (std::size_t i=0;i<count;++i) if (c.last_dispatched_target==targets[i]->id) start=(i+1)%count;
    for (std::size_t i=0;i<count;++i) {
        const auto target=targets[(start+i)%count]->id;
        const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target,[](const auto& entry,BuildingId key){return entry.first<key;});
        if (route!=c.dynamic_target_routes.end() && route->first==target && route->second) return {CourierDispatchStatus::Ready,target};
    }
    return {CourierDispatchStatus::NoRoad,{}};
}
void World::dispatch_original_service(CourierState& c) {
    const auto decision=courier_dispatch_status(c.id);if (decision.status!=CourierDispatchStatus::Ready || !decision.selected_target) return;
    const auto target=*decision.selected_target;const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target,[](const auto& entry,BuildingId key){return entry.first<key;});
    if (route==c.dynamic_target_routes.end() || !route->second) throw std::logic_error("original service dispatch lost its shared cached route");
    auto candidate=original_production_->services;const auto* previous=latest_trip(candidate.trips,c.id);
    if (previous && previous->transfer.sequence==UINT64_MAX) return;
    OriginalServiceTrip next{{c.id,previous ? previous->transfer.sequence+1:1},target,ticks_};
    if (auto* current=latest_trip(candidate.trips,c.id)) *current=next;
    else candidate.trips.insert(std::lower_bound(candidate.trips.begin(),candidate.trips.end(),c.id,[](const auto& trip,CourierId key){return trip.transfer.courier_id<key;}),next);
    auto path=*route->second;
    if (!ticks_) throw std::logic_error("original service dispatch has no advanced ordinary World tick");
    validate_services(candidate,buildings_,couriers_,original_economy_->houses,original_production_->housing,ticks_,ticks_-1,false);
    // All route allocations/service candidate checks precede the sole physical
    // input debit. The remaining service/courier publication cannot allocate.
    if (provider(candidate,c.owner).type==OriginalServiceProviderType::Confucian219) consume_original_academy_paper(c.owner);
    original_production_->services=std::move(candidate);c.path=std::move(path);c.target=target;c.last_dispatched_target=target;
    c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;
}
void World::arrive_original_service(CourierState& c) {
    if (c.phase!=CourierPhase::ToWarehouse) {
        c.path.clear();c.path_vertex=0;c.phase=CourierPhase::IdleAtWorkshop;c.route_pending=false;c.route_checked_revision.reset();return;
    }
    auto candidate=original_production_->services;auto houses=original_economy_->houses;auto* trip=latest_trip(candidate.trips,c.id);
    if (!trip || trip->arrival_tick || trip->target!=c.target) throw std::logic_error("original service arrival lost its actual outbound trip");
    const auto& p=provider(candidate,c.owner);const auto& target=building(c.target);auto& h=house_row(houses,target.id);
    trip->arrival_tick=ticks_;
    // A dispatched visitor finishes despite later source pause or fire. The
    // actual destination and original DirectService admission are rechecked.
    if (!building_on_fire(target.id) && eligible(h,visit_for(p.type))) {
        h.house=original::apply_service_visit(h.house,context(h),visit_for(p.type));trip->visit_applied=true;
        OriginalServiceWitness witness{target.id,role_for(p.type),p.building_id,trip->transfer,ticks_};const auto key=witness_key(witness);
        const auto at=std::lower_bound(candidate.witnesses.begin(),candidate.witnesses.end(),key,[](const auto& row,const auto& value){return witness_key(row)<value;});
        if (at!=candidate.witnesses.end() && witness_key(*at)==key) *at=witness;else candidate.witnesses.insert(at,witness);
    }
    validate_services(candidate,buildings_,couriers_,houses,original_production_->housing,ticks_,ticks_-1,false);
    original_production_->services=std::move(candidate);original_economy_->houses=std::move(houses);c.phase=CourierPhase::Returning;
    if (valid_return_path(c)) {std::reverse(c.path.begin(),c.path.end());c.path_vertex=0;}
    else {c.path={c.path.back()};c.path_vertex=0;c.route_pending=true;c.route_checked_revision.reset();reroute_v2(c);}
}
void World::decay_original_services() {
    if (!ticks_ || countdowns(ticks_)==countdowns(ticks_-1)) throw std::logic_error("original service countdown invoked outside phase35");
    auto houses=original_economy_->houses;
    for (auto& h:houses) h.house=original::decay_house_services(h.house,context(h));
    validate_services(original_production_->services,buildings_,couriers_,houses,original_production_->housing,ticks_,ticks_);
    original_economy_->houses=std::move(houses);
}

} // namespace openemperor::simulation
