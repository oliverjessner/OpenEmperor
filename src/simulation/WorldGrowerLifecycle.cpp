#include "simulation/World.h"
#include "simulation/original/CropSelectionRules.h"

#include <algorithm>
#include <array>
#include <limits>

namespace openemperor::simulation {
namespace {
constexpr original::CropDispatchConfig initial_config{5,2};
constexpr std::size_t audit_limit=8;
bool food(Object kind) {
    return kind==Object::OriginalWheatField || kind==Object::OriginalMilletField ||
        kind==Object::OriginalRiceField || kind==Object::OriginalCabbageField || kind==Object::OriginalSoybeanField;
}
original::ModelBuilding model(Object kind) {
    switch(kind) {
    case Object::OriginalWheatField:return original::ModelBuilding::WheatField;
    case Object::OriginalMilletField:return original::ModelBuilding::MilletField;
    case Object::OriginalRiceField:return original::ModelBuilding::RiceField;
    case Object::OriginalCabbageField:return original::ModelBuilding::CabbageField;
    case Object::OriginalSoybeanField:return original::ModelBuilding::SoybeanField;
    default:throw std::logic_error("Non-Food lifecycle field");
    }
}
bool complete(const World& world,const BuildingState& body) {
    if(!body.placed) return false;
    for(const auto cell:building_footprint_cells(world.profile(),world.rule_version(),body.kind,body.cell))
        if(!world.in_bounds(cell) || world.building_owner_at(cell)!=body.id || world.object_at(cell)!=body.kind) return false;
    return true;
}
bool in_range(Cell farm,Cell field) {
    return field.x>=farm.x-3 && field.x<=farm.x+5 && field.y>=farm.y-3 && field.y<=farm.y+5;
}
original::CropGrowerCell native_cell(Cell cell) {
    return {static_cast<std::int16_t>(cell.x),static_cast<std::int16_t>(cell.y)};
}
std::size_t live(const GrowerLifecycleState& state) {
    return static_cast<std::size_t>(std::count_if(state.actors.begin(),state.actors.end(),[](const auto& row){return !row.retired;}));
}
} // namespace

GrowerLifecycleAdmission World::start_grower_lifecycle(const GrowerLifecycleRequest& request) {
    if(request.policy!=GrowerLifecyclePolicy::AutonomousTendingV1 ||
        request.interaction!=GrowerNavigationInteraction::None)
        return {false,"Unsupported actor/global interaction or lifecycle policy"};
    if(has_grower_navigation_session()) return {false,"Another Grower session owns the transient context"};
    if(!request.source || !original_terrain_facts_ ||
        request.source->source_map_sha256()!=original_terrain_facts_->source_map_sha256() ||
        !prepare_grower_navigation_source()) return {false,"Complete matching Rule26 source/entry facts are required"};
    const auto farm=std::find_if(buildings_.begin(),buildings_.end(),[&](const auto& b){return b.id==request.farm_id;});
    if(farm==buildings_.end() || static_cast<std::uint32_t>(farm->id)>32767 ||
        farm->kind!=Object::OriginalFoodFarm || !complete(*this,*farm))
        return {false,"Lifecycle requires a complete paid Farm193"};
    GrowerLifecycleState state;state.policy=request.policy;state.farm_id=request.farm_id;
    state.admitted_slot=agriculture_calendar()->position.month;
    for(const auto& field:agriculture_->fields) {
        const auto& body=building(field.field_id);
        if(field.farm_id!=farm->id) {
            if(in_range(farm->cell,body.cell)) return {false,"Foreign field in the native source range is unsupported"};
            continue;
        }
        if(static_cast<std::uint32_t>(field.field_id)>32767 || !food(body.kind) ||
            !complete(*this,body) || !in_range(farm->cell,body.cell))
            return {false,"Lifecycle requires complete typed Food fields in the native footprint range"};
        state.fields.push_back({field.field_id,{}});
    }
    state.actors.reserve(audit_limit);
    grower_lifecycle_=std::move(state);grower_lifecycle_source_=request.source;
    grower_lifecycle_access_revision_.reset();
    return {true,"Explicit autonomous tending-only session admitted; no harvest or saved authority"};
}

void World::stop_grower_lifecycle() {
    if(!grower_lifecycle_) return;
    grower_lifecycle_->stop_requested=true;
    grower_lifecycle_->status=live(*grower_lifecycle_) ? GrowerLifecycleStatus::Draining:GrowerLifecycleStatus::Stopped;
}
bool World::clear_grower_lifecycle() {
    if(!grower_lifecycle_ || live(*grower_lifecycle_)) return !grower_lifecycle_;
    grower_lifecycle_.reset();grower_lifecycle_source_.reset();grower_lifecycle_access_revision_.reset();
    grower_lifecycle_tick_staff_percent_=0;return true;
}
void World::capture_grower_lifecycle_staffing() {
    const auto id=grower_lifecycle_->farm_id;
    const auto required=workforce_required(id),assigned=workers_assigned(id);
    grower_lifecycle_tick_staff_percent_=required>0
        ? static_cast<std::uint8_t>(std::clamp(100*assigned/required,0,100)):0;
}

std::optional<Cell> World::grower_lifecycle_source_cell() {
    auto& state=*grower_lifecycle_;
    if(grower_lifecycle_access_revision_==road_revision_) return state.source_cell;
    grower_lifecycle_access_revision_=road_revision_;state.source_cell.reset();
    const auto& farm=building(state.farm_id);
    // Actual side3 native perimeter. Restrict to one ordinary road so priority
    // region and non-road fallback branches cannot choose another endpoint.
    constexpr std::array<Cell,12> offsets{{{0,-1},{1,-1},{2,-1},{3,0},{3,1},{3,2},
        {2,3},{1,3},{0,3},{-1,2},{-1,1},{-1,0}}};
    std::optional<Cell> unique;
    for(const auto offset:offsets) {
        const Cell cell{farm.cell.x+offset.x,farm.cell.y+offset.y};
        const auto mask=original_grower_navigation_mask(cell);
        if(!mask) return {};
        if(*mask!=0x04U) continue;
        if(unique) return {};
        unique=cell;
    }
    if(!unique) return {};
    const auto entry=grower_lifecycle_source_->storage_entry_cell();
    const auto mask=original_grower_navigation_mask(entry);
    if(!mask || *mask!=0x04U) return {};
    std::int32_t distance=0;GrowerNavigationResult result{};
    const auto witness=find_road_route(entry,*unique,{},{},false,&result,&distance,true);
    if(!witness || distance<=0) return {};
    state.source_cell=unique;return unique;
}

void World::dispatch_food_grower() {
    auto& state=*grower_lifecycle_;const auto& farm=building(state.farm_id);
    ++state.source_callbacks;
    if(!farm.operating_enabled || building_on_fire(farm.id) || !grower_lifecycle_tick_staff_percent_) {
        state.status=GrowerLifecycleStatus::WaitingForStaff;return;
    }
    const auto source=grower_lifecycle_source_cell();
    if(!source) {state.status=GrowerLifecycleStatus::WaitingForAccess;state.blocker="No unique entry-reachable ordinary Farm road";return;}
    const auto attempt=original::advance_crop_dispatch_attempt(state.dispatch_counter,
        grower_lifecycle_tick_staff_percent_,initial_config);
    state.dispatch_counter=attempt.counter;if(!attempt.attempt_due) return;
    ++state.due_attempts;
    const auto quota=original::crop_dispatch_rule(grower_lifecycle_tick_staff_percent_,initial_config).tending_limit;
    if(quota<=state.retained_counters.tending) return;
    std::vector<GrowerLifecycleField*> candidates;
    for(auto& field:state.fields) {
        const auto& body=building(field.field_id);
        if(field.state.quantity || (field.state.work_word&0x8000U)) {
            state.status=GrowerLifecycleStatus::Unsupported;state.blocker="Harvest context is outside tending-only admission";return;
        }
        if(field.state.assigned_actor_word || !original::crop_in_growing_period(model(body.kind),state.admitted_slot)) continue;
        std::int32_t distance=0;GrowerNavigationResult outcome{};
        (void)find_road_route(*source,body.cell,{},{},true,&outcome,&distance);
        if(distance<=0) {
            if(outcome==GrowerNavigationResult::Unsupported) {
                state.status=GrowerLifecycleStatus::Unsupported;state.blocker="Initial candidate distance is unsupported";return;
            }
            continue;
        }
        // The single candidate's eligibility is independent of the unresolved
        // runtime fertility score. Check both supported extrema, retain NO
        // invented numeric score and never substitute construction fertility.
        original::CropCandidateFacts facts{model(body.kind),1,field.state.quantity,field.state.work_word,
            distance,0,true,in_range(farm.cell,body.cell),true,true,body.operating_enabled};
        const auto low=original::evaluate_crop_initial_candidate(original::CropWork::Tending,facts);
        facts.fertility_query=100;
        const auto high=original::evaluate_crop_initial_candidate(original::CropWork::Tending,facts);
        if(low.eligible && high.eligible) candidates.push_back(&field);
    }
    if(candidates.empty()) {state.status=GrowerLifecycleStatus::WaitingForSeason;return;}
    if(candidates.size()!=1) {
        state.status=GrowerLifecycleStatus::Unsupported;state.blocker="Multiple initial candidates require admitted runtime-fertility scores";return;
    }
    auto& selected=*candidates.front();const auto& body=building(selected.field_id);
    if(!body.operating_enabled || building_on_fire(body.id)) return;
    ++state.allocation_attempts;
    // One live actor is an explicit peer-free context fence, NOT the native
    // quota. Keep native counters independent, including their double debit.
    if(live(state) || grower_navigation_cell_claimed(*source) ||
        couriers_.size()+grower_navigation_tracks_.size()>=Rules::original_production_courier_limit ||
        !next_food_grower_id_ || next_food_grower_id_>32767 ||
        !next_grower_navigation_id_ || next_grower_navigation_id_==UINT64_MAX) {
        ++state.allocation_refusals;
        (void)original::commit_crop_spawn(original::CropWork::Tending,false,0,state.retained_counters.tending);return;
    }
    const auto committed=original::commit_crop_spawn(original::CropWork::Tending,true,0,state.retained_counters.tending);
    FoodGrowerActor actor;actor.id={next_food_grower_id_};actor.farm_id=farm.id;actor.current=*source;
    actor.spawned_tick=ticks_;actor.navigation_id=GrowerNavigationId{next_grower_navigation_id_};
    actor.native.raw_source_word=static_cast<std::uint16_t>(farm.id);
    actor.native.raw_state=committed.actor->state;actor.native.raw_role=committed.actor->role;
    actor.native.raw_status=1;actor.native.wait=committed.actor->initial_wait;
    GrowerNavigationTrack track;track.id=*actor.navigation_id;track.lifecycle_owned=true;
    track.request.farm_id=farm.id;track.request.field_id=selected.field_id;
    track.request.start=*source;track.request.target=*source;track.current=*source;
    track.path={*source};track.result=GrowerNavigationResult::Arrived;track.route_revision=road_revision_;
    grower_navigation_tracks_.reserve(grower_navigation_tracks_.size()+1);
    if(state.actors.size()==audit_limit) state.actors.erase(state.actors.begin());
    state.actors.push_back(std::move(actor));grower_navigation_tracks_.push_back(std::move(track));
    ++next_food_grower_id_;++next_grower_navigation_id_;
    selected.state.assigned_actor_word=static_cast<std::uint16_t>(state.actors.back().id.value);
    state.retained_counters.tending=committed.retained_counter;++state.spawned;
    state.status=GrowerLifecycleStatus::Running;state.blocker.clear();
}

bool World::prepare_food_grower_route(FoodGrowerActor& actor,BuildingId field,Cell target,bool returning) {
    auto& track=*std::find_if(grower_navigation_tracks_.begin(),grower_navigation_tracks_.end(),
        [&](const auto& row){return row.id==actor.navigation_id;});
    GrowerNavigationResult outcome{};
    auto route=find_road_route(actor.current,target,{},{},true,&outcome);
    track.request={GrowerNavigationPolicy::Mode17MicrostepV1,actor.farm_id,field,actor.current,target,returning,GrowerNavigationInteraction::None};
    track.path=route ? std::move(*route):std::vector<Cell>{actor.current};
    track.current=actor.current;track.cursor=0;track.phase=20;track.edge_started=false;
    track.unsupported_context_seen=false;track.route_revision=road_revision_;
    track.result=route ? (actor.current==target ? GrowerNavigationResult::Arrived:GrowerNavigationResult::Moving):outcome;
    return route.has_value();
}

void World::retire_food_grower(FoodGrowerActor& actor) {
    auto& state=*grower_lifecycle_;
    actor.native.raw_status=2;
    const auto finish=original::finish_crop_grower_callback(actor.native,state.retained_counters);
    state.retained_counters=finish.counters;
    std::vector<original::CropGrowerFieldAssignment> assignments;
    for(const auto& row:state.fields) assignments.push_back({static_cast<std::uint8_t>(building(row.field_id).placed ? 1:0),row.state.assigned_actor_word});
    (void)original::clear_crop_grower_assignments(assignments,static_cast<std::uint16_t>(actor.id.value));
    for(std::size_t i=0;i<state.fields.size();++i) state.fields[i].state.assigned_actor_word=assignments[i].raw_actor_word;
    actor.native=original::reset_retired_crop_grower(finish.actor).actor;
    std::erase_if(grower_navigation_tracks_,[&](const auto& row){return row.id==actor.navigation_id && row.lifecycle_owned;});
    actor.navigation_id.reset();actor.retired=true;actor.retired_tick=ticks_;++state.retired;
}

void World::advance_food_grower(FoodGrowerActor& actor) {
    auto& state=*grower_lifecycle_;
    auto& track=*std::find_if(grower_navigation_tracks_.begin(),grower_navigation_tracks_.end(),
        [&](const auto& row){return row.id==actor.navigation_id;});
    if(state.stop_requested || actor.cancel_requested) {
        actor.cancel_requested=true;
        if(track.edge_started) {track.unsupported_context_seen=true;advance_grower_navigation_track(track);actor.current=track.current;}
        if(!track.edge_started) retire_food_grower(actor);
        return;
    }
    actor.native=original::begin_crop_grower_callback(actor.native,1);
    if(actor.native.raw_state==6 || actor.native.raw_state==8) {
        auto field=std::find_if(state.fields.begin(),state.fields.end(),[&](const auto& f){return actor.last_field_id==f.field_id;});
        const bool compatible=field!=state.fields.end() && complete(*this,building(field->field_id));
        const auto step=original::advance_crop_grower_work(actor.native,compatible);actor.native=step.actor;
        if(step.field_callback==original::CropGrowerFieldCallback::Tending) {
            field->state=original::apply_crop_tending_callback(model(building(field->field_id).kind),field->state);
            field->last_tending_tick=ticks_;++field->tending_callbacks;++actor.tending_callbacks;++state.tending_callbacks;
        }
        if(step.select_target) {
            // Tending passes literal cursor0. The retained-last-field cursor
            // belongs to the separate harvest selector, not this role.
            const auto selected=std::find_if(state.fields.begin(),state.fields.end(),[&](const auto& f){
                return f.state.assigned_actor_word==actor.id.value &&
                    original::crop_in_growing_period(model(building(f.field_id).kind),state.admitted_slot);
            });
            std::optional<original::CropGrowerTarget> target;
            if(selected!=state.fields.end()) target=original::CropGrowerTarget{static_cast<std::uint16_t>(selected->field_id),native_cell(building(selected->field_id).cell)};
            const auto result=original::apply_crop_grower_target(actor.native,state.retained_counters,target,true,native_cell(*state.source_cell));
            actor.native=result.actor;state.retained_counters=result.counters;
            if(selected!=state.fields.end()) actor.last_field_id=selected->field_id;
            const Cell destination{actor.native.target.x,actor.native.target.y};
            (void)prepare_food_grower_route(actor,actor.last_field_id.value_or(state.fields.front().field_id),destination,actor.native.raw_state==9);
        }
    } else if(actor.native.raw_state==7 || actor.native.raw_state==9) {
        if(track.result==GrowerNavigationResult::Moving || track.result==GrowerNavigationResult::Retry)
            advance_grower_navigation_track(track);
        actor.current=track.current;
        if(track.result==GrowerNavigationResult::Unsupported) {
            state.stop_requested=true;state.status=GrowerLifecycleStatus::Unsupported;
            state.blocker="Unsupported actor/terrain interaction; draining without work";actor.cancel_requested=true;
            if(!track.edge_started) retire_food_grower(actor);return;
        }
        original::CropGrowerMovement movement;movement.outcome=static_cast<std::uint8_t>(track.result);
        if(actor.last_field_id) {
            const auto& field=*std::find_if(state.fields.begin(),state.fields.end(),[&](const auto& f){return f.field_id==actor.last_field_id;});
            movement.food_field=model(building(field.field_id).kind);movement.field_compatible=true;
            movement.field_assignment_nonzero=field.state.assigned_actor_word!=0;
        }
        const auto step=original::apply_crop_grower_movement(actor.native,movement);actor.native=step.actor;
        if(actor.native.raw_state==8 && movement.outcome==8) actor.arrived_tick=actor.work_started_tick=ticks_;
    } else throw std::logic_error("Unsupported state in tending-only actor");
    if(actor.native.raw_yield_word) throw std::logic_error("Tending-only actor acquired cargo");
    if(actor.native.raw_status==2) retire_food_grower(actor);
}

void World::advance_grower_lifecycle() {
    auto& state=*grower_lifecycle_;const auto& farm=building(state.farm_id);
    const auto active=live(state)>0;
    bool unsafe=agriculture_calendar()->position.month!=state.admitted_slot;
    for(const auto& field:agriculture_->fields) {
        if(!in_range(farm.cell,building(field.field_id).cell)) continue;
        if(std::none_of(state.fields.begin(),state.fields.end(),[&](const auto& known){return known.field_id==field.field_id;}))
            unsafe=true;
    }
    if(active) {
        const auto before=state.source_cell;
        unsafe=unsafe || !farm.operating_enabled || building_on_fire(farm.id) ||
            !grower_lifecycle_tick_staff_percent_ || grower_lifecycle_source_cell()!=before;
        for(const auto& field:state.fields) unsafe=unsafe || !complete(*this,building(field.field_id)) ||
            !building(field.field_id).operating_enabled || building_on_fire(field.field_id);
    }
    unsafe=unsafe || std::any_of(couriers_.begin(),couriers_.end(),[](const auto& c){return c.enabled && c.phase!=CourierPhase::IdleAtWorkshop;});
    if(unsafe) {
        state.stop_requested=true;state.status=GrowerLifecycleStatus::Unsupported;
        state.blocker="Outside admitted slot/source/staff/fire/peer context; no further work";
    }
    if(!state.stop_requested && original::next_scheduler_dispatch(ticks_-1)->position.phase==31) dispatch_food_grower();
    for(auto& actor:state.actors) if(!actor.retired) advance_food_grower(actor);
    if(state.stop_requested && !live(state) && state.status!=GrowerLifecycleStatus::Unsupported)
        state.status=GrowerLifecycleStatus::Stopped;
}
} // namespace openemperor::simulation
