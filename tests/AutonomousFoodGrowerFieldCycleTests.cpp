// Paid autonomous field development under an explicit transient policy. Actual
// World ticks earn tending and quantity; no actor, arrival, mark, quantity,
// harvest context or stock is injected into the live World.
#include "IrrigationFixture.h"
#include "maps/EmperorContainer.h"
#include "persistence/RecoveryStore.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

namespace {
namespace q=oe_irrigation_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using Bytes=std::vector<std::uint8_t>;

template<class Action>void refuses(Action action,const char* why) {
    try {action();}catch(const std::exception&){return;}
    throw std::runtime_error(why);
}
Bytes encode(const Bytes& raw) {
    Bytes result(4);a::u32(result,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768) {
        const auto size=std::min<std::size_t>(32768,raw.size()-at);
        uLongf length=compressBound(static_cast<uLong>(size));Bytes block(length);
        check(compress2(block.data(),&length,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,
              "Authored field-cycle map compression failed");
        const auto header=result.size();result.resize(header+12);a::u32(result,header,0x12345678U);
        a::u32(result,header+4,static_cast<std::uint32_t>(length));
        a::u32(result,header+8,static_cast<std::uint32_t>(size));
        result.insert(result.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return result;
}
struct Fixture:q::Fixture {
    std::shared_ptr<const sim::GrowerSourceFacts> source;
    Fixture() {
        auto text=q::authored_model();
        // Stable24-resident authored staffing inputs, not native model parity.
        for(std::size_t row=0;row<3;++row) {
            oe_production_fixture::set_house_field(text,row,17,6);
            oe_production_fixture::set_house_field(text,row,1,100);
        }
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_irrigation_rules_model(root.data);
        const auto path=root.data/f::map_relative;
        auto logical=openemperor::maps::EmperorContainer::open(path).read_part(0);
        a::u32(logical,8,5);logical.at(56)=6;logical.at(57)=0;
        logical.at(850)=90;logical.at(851)=0;logical.at(852)=99;logical.at(853)=0;
        const auto bytes=encode(logical);f::write_text(path,std::string(bytes.begin(),bytes.end()));
        terrain=save::prepare_original_irrigation_terrain_facts(root.data,f::map_relative);
        source=save::prepare_original_grower_source_facts(root.data,f::map_relative);
        check(source->storage_entry_cell()==sim::Cell{91,100} &&
              source->source_map_sha256()==terrain->source_map_sha256(),
              "Field-cycle source lost the sealed header/entry/SHA binding");
    }
};
struct Scene {sim::BuildingId farm{},field{};};
Scene build(sim::World& world,sim::CommandType kind) {
    Scene scene;scene.farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,95});
    for(int x=70;x<=91;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int y=98;y<104;++y)f::buy(world,{sim::CommandType::PlaceRoad,{91,y}});
    for(const int x:{70,73,76,79})(void)q::purchase(world,sim::CommandType::PlaceHousehold,{x,102});
    sim::Command command{kind,{93,98}};command.building_id=scene.farm;f::buy(world,command);
    scene.field=*world.building_owner_at(command.cell);
    const auto entrances=world.building_entrances(scene.farm);
    check(entrances.size()==1 && entrances.front().road_cell==sim::Cell{91,98} &&
          !world.total_population() && !world.workers_assigned(scene.farm),
          "Paid field-cycle scene granted workforce or lost its unique native source port");
    return scene;
}
void prewarm(sim::World& world,unsigned slot) {
    for(unsigned n=0;n<slot*816U;++n)world.tick();
    check(world.agriculture_calendar()->position.month==slot,"Empty World failed to reach the real requested slot");
}
const sim::GrowerLifecycleState& state(const sim::World& world) {
    check(world.grower_lifecycle().has_value(),"Field-cycle controller disappeared");return *world.grower_lifecycle();
}
const sim::GrowerLifecycleField& field(const sim::World& world) {
    check(state(world).fields.size()==1,"Field-cycle session changed its admitted single-field identity");
    return state(world).fields.front();
}
std::size_t live(const sim::World& world) {
    const auto& rows=state(world).actors;
    return static_cast<std::size_t>(std::count_if(rows.begin(),rows.end(),[](const auto& row){return !row.retired;}));
}
const sim::FoodGrowerActor& actor(const sim::World& world,sim::FoodGrowerId id) {
    const auto& rows=state(world).actors;
    const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    check(at!=rows.end(),"Field-cycle audit lost a just-observed physical actor");return *at;
}
void historical_equal(const sim::World& world,const sim::World& control) {
    auto observed=world.snapshot();observed.navigation_session_present=false;
    check(observed==control.snapshot(),
          "Transient field development changed historical Food, accounts, paid IDs, fields or World timing");
    if(!world.grower_lifecycle())return;
    const auto& row=field(world);
    check(std::uint64_t(row.state.quantity)+row.quantity_expired+row.quantity_vetoed==row.quantity_added,
          "Earned field quantity, expiration and veto removals do not conserve the separate development quantity");
    check(row.state.history==std::array<std::uint16_t,12>{},"Blocked live yield published native harvest history");
    for(const auto& actor:state(world).actors)
        check(!actor.native.raw_yield_word,"Blocked live yield published actor cargo");
    check(live(world)<=1 && world.couriers().size()+world.grower_navigation_tracks().size()<=
              sim::Rules::original_production_courier_limit,
          "Field cycle exceeded the admitted peer/channel bounds");
}
void tick(sim::World& world,sim::World& control) {world.tick();control.tick();historical_equal(world,control);}
template<class Predicate>void until(sim::World& world,sim::World& control,Predicate done,const char* why,int limit=1800) {
    for(int n=0;n<limit && !done();++n)tick(world,control);
    check(done(),std::string(why)+": "+state(world).blocker);
}
void enable(sim::World& world,const Scene& scene,const Fixture& fixture) {
    sim::GrowerLifecycleRequest request;request.policy=sim::GrowerLifecyclePolicy::AutonomousFieldCycleV1;
    request.farm_id=scene.farm;request.source=fixture.source;
    const auto result=world.start_grower_lifecycle(request);
    check(result.accepted,"Field-cycle admission failed: "+result.reason);
}
void drain(sim::World& world,sim::World& control) {
    world.stop_grower_lifecycle();until(world,control,[&]{return !live(world);},"Field-cycle stop retained a live worker",80);
    check(!field(world).state.assigned_actor_word && world.grower_navigation_tracks().empty() &&
          world.clear_grower_lifecycle() && world.snapshot()==control.snapshot(),
          "Field-cycle cleanup retained claims, movement or historical authority");
}

void earn_first_growth(sim::World& world,sim::World& control,const Scene& scene) {
    bool actual_motion=false,actual_wait=false;
    for(int n=0;n<500 && !state(world).tending_callbacks;++n) {
        check(!field(world).state.quantity && !field(world).quantity_added,
              "Calendar or staffing granted quantity before actual completed tending");
        for(const auto& actor:state(world).actors) if(!actor.retired) {
            actual_motion|=actor.current!=sim::Cell{91,98};
            actual_wait|=actor.native.raw_state==8 && actor.current==world.building(scene.field).cell &&
                actor.arrived_tick>actor.spawned_tick && actor.native.wait>0 && actor.native.wait<=51;
        }
        tick(world,control);
    }
    check(actual_motion && actual_wait && state(world).tending_callbacks==1 &&
          field(world).state.work_word==0x40 && !field(world).state.quantity &&
          !field(world).state.assigned_actor_word,
          "First field work did not follow autonomous physical arrival and tending51");
    const auto decay=field(world).work_decay_callbacks;
    until(world,control,[&]{return field(world).work_decay_callbacks>decay;},"Completed cycle never decayed earned work",51);
    check(field(world).state.work_word==0x3f && !field(world).state.quantity,
          "First native cycle decay failed literal work64-to63 or fabricated quantity");
    until(world,control,[&]{return field(world).state.quantity>0;},"Actual phase20 never developed earned field quantity",51);
    const auto position=orig::scheduler_position(world.ticks()-1);
    check(position.phase==20 && field(world).state.quantity==157 && field(world).quantity_added==157 &&
          field(world).last_growth_tick==world.ticks() && !field(world).harvest_marks,
          "First earned quantity did not follow the literal157 phase20 branch");
}

void actual_mark_physical_harvest_and_blocked_yield(sim::World& world,sim::World& control,unsigned end_slot) {
    until(world,control,[&]{return field(world).harvest_marks==1;},"Real end-slot phase20 did not create a harvest mark");
    const auto position=orig::scheduler_position(world.ticks()-1);
    check(position.phase==20 && position.step==0 && position.month==end_slot &&
          field(world).last_mark_tick==world.ticks() && (field(world).state.work_word&0x8000U) &&
          field(world).state.quantity>0,
          "Harvest mark did not arise from the actual end-slot/subslot0 phase20 schedule");
    const auto next_id=world.next_food_grower_id();
    until(world,control,[&]{return state(world).harvest_candidate_attempts>0;},"Positive marked field never reached actual harvest selection",160);
    const auto id=state(world).actors.back().id;
    check(id.value>=next_id && world.next_food_grower_id()>next_id &&
          actor(world,id).native.raw_role==2 && state(world).farm_retained_word40==0 &&
          field(world).state.assigned_actor_word==id.value && field(world).state.quantity>0,
          "Actual marked candidate did not allocate a real bounded harvest-role actor");
    bool moved=false,shared_route=false;
    until(world,control,[&]{
        const auto& row=actor(world,id);moved|=row.current!=sim::Cell{91,98};
        if(row.navigation_id) {const auto& route=world.grower_navigation(*row.navigation_id);
            shared_route|=route.lifecycle_owned && route.path.size()>=2 && route.current==row.current;}
        return row.native.raw_state==12;
    },"Physical harvest actor never reached actual state12",100);
    const auto arrival=actor(world,id);
    const auto quantity=field(world).state.quantity;const auto blocks=state(world).harvest_callback_blocks;
    check(moved && shared_route && arrival.native.wait==25 &&
          arrival.current==world.building(field(world).field_id).cell && arrival.arrived_tick==world.ticks() &&
          arrival.last_field_id==field(world).field_id && (field(world).state.work_word&0x8000U),
          "Harvest wait25 was not established by its actual retained-field arrival");
    for(int eligible=1;eligible<=24;++eligible) {
        tick(world,control);
        check(actor(world,id).native.raw_state==12 && actor(world,id).native.wait==25-eligible &&
              state(world).harvest_callback_blocks==blocks && field(world).state.quantity==quantity,
              "Harvest work matured before25 actual eligible actor callbacks");
    }
    tick(world,control);
    const auto& blocked=actor(world,id);
    // BLOCKED_RETURN: this is explicit no-cargo cancellation at the field,
    // never an Original completed callback, State13 return or Farm receipt.
    check(state(world).harvest_callback_blocks==blocks+1 &&
          state(world).harvest_blocker==sim::GrowerHarvestBlocker::LiveYieldContext &&
          state(world).status==sim::GrowerLifecycleStatus::HarvestBlocked && !state(world).blocker.empty() &&
          blocked.harvest_callback_blocked_tick==arrival.arrived_tick+25 && blocked.retired &&
          blocked.current==arrival.current && blocked.current!=sim::Cell{91,98} &&
          !blocked.navigation_id && blocked.native==orig::CropGrowerActor{} &&
          field(world).state.quantity==quantity && (field(world).state.work_word&0x8000U) &&
          !(field(world).state.work_word&0x4000U) && !field(world).state.assigned_actor_word,
          "Unproved live yield consumed quantity/history/cargo or pretended to return and credit the Farm");
    for(int step=0;step<10;++step)tick(world,control);
    check(field(world).state.quantity==quantity && field(world).state.history==std::array<std::uint16_t,12>{} &&
          state(world).harvest_callback_blocks==blocks+1,
          "Retired blocked callback repeated a harvest effect");
}

void all_five_paid_cycles_marks_and_expiration() {
    Fixture fixture;
    const std::array cases{
        std::pair{sim::CommandType::PlaceOriginalWheatField,6U},
        std::pair{sim::CommandType::PlaceOriginalMilletField,10U},
        std::pair{sim::CommandType::PlaceOriginalRiceField,9U},
        std::pair{sim::CommandType::PlaceOriginalCabbageField,11U},
        std::pair{sim::CommandType::PlaceOriginalSoybeanField,8U}};
    for(const auto& [kind,end]:cases) {
        auto world=fixture.world();prewarm(world,end-1);const auto scene=build(world,kind);
        for(int n=0;n<200;++n)world.tick();
        check(world.total_population()==24 && world.workers_assigned(scene.farm)==12 &&
              fixture.terrain->fertility(world.building(scene.field).cell)==100,
              "Actual paid scene lost its real Farm workforce or source Rice fertility");
        auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        check(!field(world).countdown && !field(world).state.quantity && !field(world).state.work_word,
              "Fresh field-cycle controller imported invented development state");
        earn_first_growth(world,control,scene);actual_mark_physical_harvest_and_blocked_yield(world,control,end);
        const auto unharvested=field(world).state.quantity;
        const auto next_grower=world.next_food_grower_id();
        until(world,control,[&]{return field(world).expirations==1;},"Actual completed end slot never expired unharvested quantity");
        const auto completed=orig::scheduler_position(world.ticks()-1);
        check(completed.phase==50 && completed.step==15 && completed.month==end &&
              field(world).last_expiration_tick==world.ticks() &&
              field(world).quantity_expired==unharvested && !field(world).state.quantity &&
              !field(world).state.work_word && !field(world).state.assigned_actor_word &&
              field(world).quantity_added==field(world).quantity_expired,
              "Expiration failed its actual old-slot boundary, quantity accounting or reference cleanup");
        check(state(world).harvest_callback_blocks==1 && world.next_food_grower_id()==next_grower,
              "The blocked live-yield boundary allocated another harvest lifetime before expiration");
        drain(world,control);
        if(kind==sim::CommandType::PlaceOriginalCabbageField)
            check(a::stock(world,scene.farm).amounts==sim::AgricultureAmounts{},"Field quantity became historical Cabbage Food");
    }
}

void active_quantity_save_recovery_guard_and_independent_replay() {
    Fixture fixture;auto world=fixture.world();prewarm(world,10);
    const auto scene=build(world,sim::CommandType::PlaceOriginalCabbageField);
    for(int n=0;n<200;++n)world.tick();
    auto control=fixture.restore(world.snapshot()),replay=fixture.restore(world.snapshot());
    const auto clean=fixture.document(world);const auto path=fixture.root.path/"field-cycle42.json";
    save::write_save(path,clean,fixture.root.data,fixture.mask());const auto bytes=f::read_text(path);
    save::RecoveryStore recovery(fixture.root.path/"app",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(clean,fixture.mask());
    const auto metadata=recovery.root()/history/"history.json";const auto before_metadata=f::read_text(metadata);
    const auto map=f::read_text(fixture.root.data/f::map_relative);
    const auto model=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    enable(world,scene,fixture);enable(replay,scene,fixture);
    bool guarded=false;
    for(int step=0;step<1600 && !field(world).expirations;++step) {
        tick(world,control);replay.tick();historical_equal(replay,control);
        check(state(world)==state(replay) && world.next_food_grower_id()==replay.next_food_grower_id() &&
              std::equal(world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end(),
                  replay.grower_navigation_tracks().begin(),replay.grower_navigation_tracks().end()),
              "Independent autonomous field/cursor/wait/calendar replay diverged");
        if(!guarded && field(world).state.quantity>0) {
            auto active=clean;active.world=world.snapshot();
            refuses([&]{(void)fixture.document(world);},"Active quantity entered historical document preparation");
            refuses([&]{(void)fixture.restore(active);},"Active quantity silently restored through schema42");
            refuses([&]{save::write_save(path,active,fixture.root.data,fixture.mask());},"Transient quantity was saved");
            refuses([&]{(void)recovery.write_checkpoint(history,active,fixture.mask());},"Transient quantity entered Recovery");
            check(f::read_text(path)==bytes && f::read_text(metadata)==before_metadata &&
                  recovery.catalog().histories.size()==1,
                  "Rejected active-field publication changed an existing save or Recovery history");
            guarded=true;
        }
    }
    check(guarded && field(world).quantity_added>0 && field(world).expirations==1 &&
          state(world).harvest_candidate_attempts>0,"Replay never reached actual earned quantity, mark, blocked selection and expiration");
    world.stop_grower_lifecycle();replay.stop_grower_lifecycle();
    for(int step=0;step<80 && (live(world) || live(replay));++step) {
        tick(world,control);replay.tick();historical_equal(replay,control);
        check(state(world)==state(replay),"Expiration/stop drain diverged while preserving begun movement");
    }
    check(!live(world) && !live(replay) && world.clear_grower_lifecycle() && replay.clear_grower_lifecycle() &&
          world.snapshot()==control.snapshot() && replay.snapshot()==control.snapshot(),
          "Replay cleanup retained field-cycle authority");
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto reference=fixture.root.path/"field-cycle-control42.json";
    save::write_save(reference,fixture.document(control),fixture.root.data,fixture.mask());
    const auto checkpoint=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
    check(f::read_text(path)==f::read_text(reference) && save::read_save(path).source_schema_version==42 &&
          fixture.restore(save::read_save(checkpoint.path)).snapshot()==control.snapshot() &&
          f::read_text(fixture.root.data/f::map_relative)==map &&
          f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model,
          "Field cleanup changed canonical schema42, historical Recovery continuation or source bytes");
}

void single_field_admission_and_historical_tending_fence() {
    Fixture fixture;auto empty=fixture.world();
    const auto farm=q::purchase(empty,sim::CommandType::PlaceOriginalFoodFarm,{90,95});
    sim::GrowerLifecycleRequest request;request.policy=sim::GrowerLifecyclePolicy::AutonomousFieldCycleV1;
    request.farm_id=farm;request.source=fixture.source;
    const auto before=empty.snapshot();const auto absent=empty.start_grower_lifecycle(request);
    check(!absent.accepted && !empty.grower_lifecycle() && empty.snapshot()==before && empty.next_food_grower_id()==1,
          "Field-cycle admission invented a missing paid field");
    auto mixed=fixture.world();const auto scene=build(mixed,sim::CommandType::PlaceOriginalCabbageField);
    sim::Command second{sim::CommandType::PlaceOriginalSoybeanField,{94,98}};second.building_id=scene.farm;f::buy(mixed,second);
    request.farm_id=scene.farm;const auto multiple=mixed.snapshot();
    check(!mixed.start_grower_lifecycle(request).accepted && !mixed.grower_lifecycle() && mixed.snapshot()==multiple &&
          mixed.next_food_grower_id()==1,"Field-cycle admission silently widened a single crop into mixed harvest identity");

    auto old=fixture.world();prewarm(old,10);const auto old_scene=build(old,sim::CommandType::PlaceOriginalCabbageField);
    for(int n=0;n<200;++n)old.tick();auto control=fixture.restore(old.snapshot());
    request.policy=sim::GrowerLifecyclePolicy::AutonomousTendingV1;request.farm_id=old_scene.farm;
    check(old.start_grower_lifecycle(request).accepted,"Historical tending policy was lost");
    until(old,control,[&]{return state(old).status==sim::GrowerLifecycleStatus::Unsupported && !live(old);},
          "Historical E3 policy silently crossed its original raw-slot fence",700);
    check(!field(old).state.quantity && !field(old).growth_callbacks && !field(old).harvest_marks &&
          !field(old).quantity_added,"Historical E3 policy gained E4 field development");drain(old,control);
}

void harvest_source_cut_preserves_begun_geometry_without_work() {
    Fixture fixture;auto world=fixture.world();prewarm(world,10);
    const auto scene=build(world,sim::CommandType::PlaceOriginalCabbageField);
    for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
    until(world,control,[&]{
        if(state(world).actors.empty())return false;
        const auto& row=state(world).actors.back();
        if(row.native.raw_role!=2 || row.native.raw_state!=11 || !row.navigation_id)return false;
        const auto& route=world.grower_navigation(*row.navigation_id);
        return route.edge_started && route.cursor==2;
    },"Harvest interruption never began its actual second shared movement edge");
    const auto id=state(world).actors.back().id;const auto begun_actor=actor(world,id);
    const auto begun=world.grower_navigation(*begun_actor.navigation_id);
    const auto quantity=field(world).state.quantity;const auto blocks=state(world).harvest_callback_blocks;
    check(quantity>0 && (field(world).state.work_word&0x8000U) && begun.current!=sim::Cell{91,98},
          "Harvest interruption lacked earned marked quantity or still claimed the source port");
    sim::Command overlap{sim::CommandType::PlaceOriginalCabbageField,begun.current};overlap.building_id=scene.farm;
    check(control.validate(overlap).accepted,"Harvest claim control was not an otherwise valid empty-cell purchase");
    const auto before=world.snapshot();const auto rejected=world.execute(overlap);
    check(!rejected.accepted && !rejected.changed && world.snapshot()==before &&
          actor(world,id)==begun_actor && world.grower_navigation(*begun_actor.navigation_id)==begun,
          "Harvest-role geometry allowed construction through an actual current/begun claim");
    // The source port is now behind the actor. Its real removal invalidates
    // native source access without overwriting the already begun destination.
    f::buy(world,{sim::CommandType::RemoveRoad,{91,98}});
    f::buy(control,{sim::CommandType::RemoveRoad,{91,98}});
    until(world,control,[&]{return actor(world,id).retired;},"Harvest source cut left begun motion or identity hanging",40);
    check(actor(world,id).current==begun.path.at(begun.cursor) &&
          !actor(world,id).harvest_callback_blocked_tick && state(world).harvest_callback_blocks==blocks &&
          state(world).status==sim::GrowerLifecycleStatus::Unsupported && field(world).state.quantity==quantity &&
          (field(world).state.work_word&0x8000U) && !field(world).state.assigned_actor_word &&
          !actor(world,id).native.raw_yield_word,
          "Harvest cancellation skipped a begun cell, reached the callback gate or granted quantity/cargo/receipt");
    drain(world,control);
}

void pause_staff_and_fire_cannot_replace_actual_tending() {
    Fixture fixture;
    for(const bool workforce_loss:{false,true}) {
        auto world=fixture.world();prewarm(world,10);const auto scene=build(world,sim::CommandType::PlaceOriginalCabbageField);
        for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        until(world,control,[&]{return !state(world).actors.empty() && state(world).actors.back().native.raw_state==8;},
              "Field interruption never reached real pending tending51",300);
        if(workforce_loss) {
            const auto worker=q::purchase(world,sim::CommandType::PlaceOriginalHempFarm,{103,108});
            check(q::purchase(control,sim::CommandType::PlaceOriginalHempFarm,{103,108})==worker,"Workforce control lost identity");
            f::buy(world,sim::set_building_workforce_priority(worker,sim::WorkforcePriority::High));
            f::buy(control,sim::set_building_workforce_priority(worker,sim::WorkforcePriority::High));
            check(world.workforce_supply()==24 && !world.workers_assigned(scene.farm),"Real paid staffing change did not remove Farm workers");
        }else {
            f::buy(world,sim::set_building_operation(scene.farm,false));
            f::buy(control,sim::set_building_operation(scene.farm,false));
        }
        until(world,control,[&]{return state(world).status==sim::GrowerLifecycleStatus::Unsupported && !live(world);},
              "Unsupported source/staff change left field work active",40);
        check(!state(world).tending_callbacks && !field(world).quantity_added && !field(world).state.quantity,
              "Source pause or workforce loss substituted for completed field work");drain(world,control);
    }
    auto world=fixture.world();prewarm(world,8);const auto scene=build(world,sim::CommandType::PlaceOriginalCabbageField);
    for(int n=0;n<1850;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
    until(world,control,[&]{return !state(world).actors.empty() && state(world).actors.back().native.raw_state==8;},
          "Natural-fire case never reached actual pending field work",160);
    until(world,control,[&]{return world.building_on_fire(scene.farm) && !live(world);},
          "Actual natural fire did not safely cancel pending field work",100);
    check(state(world).status==sim::GrowerLifecycleStatus::Unsupported && !state(world).tending_callbacks &&
          !field(world).quantity_added && !field(world).state.quantity,
          "Natural source fire granted unsupported tending or field quantity");drain(world,control);
}
} // namespace

int main() {
    try {
        all_five_paid_cycles_marks_and_expiration();
        active_quantity_save_recovery_guard_and_independent_replay();
        single_field_admission_and_historical_tending_fence();
        harvest_source_cut_preserves_begun_geometry_without_work();
        pause_staff_and_fire_cannot_replace_actual_tending();
        std::cout<<"Autonomous field development and physical harvest entrance PASS; live yield/raw return remain BLOCKED\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
