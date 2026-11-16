// Autonomous, World-owned transient tending. Every body, resident and worker
// arises through paid commands and ordinary ticks; no movement/work outcome,
// assignment, population or inventory is injected into the live World.
#include "IrrigationFixture.h"
#include "maps/EmperorContainer.h"

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

template<class Action> void refuses(Action action,const char* why) {
    try {action();}catch(const std::exception&){return;}
    throw std::runtime_error(why);
}

Bytes encode(const Bytes& raw) {
    Bytes result(4);a::u32(result,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768) {
        const auto size=std::min<std::size_t>(32768,raw.size()-at);
        uLongf length=compressBound(static_cast<uLong>(size));Bytes block(length);
        check(compress2(block.data(),&length,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,
              "Authored lifecycle map compression failed");
        const auto header=result.size();result.resize(header+12);a::u32(result,header,0x12345678U);
        a::u32(result,header+4,static_cast<std::uint32_t>(length));
        a::u32(result,header+8,static_cast<std::uint32_t>(size));
        result.insert(result.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return result;
}

struct Fixture:q::Fixture {
    std::shared_ptr<const sim::GrowerSourceFacts> source;
    explicit Fixture(sim::Cell packed_entry={90,99}) {
        auto text=q::authored_model();
        // Independent stable24-resident staffing control. These authored
        // capacities are not claims about the Original housing model.
        for(std::size_t row=0;row<3;++row) {
            oe_production_fixture::set_house_field(text,row,17,6);
            oe_production_fixture::set_house_field(text,row,1,100);
        }
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_irrigation_rules_model(root.data);
        const auto path=root.data/f::map_relative;
        auto logical=openemperor::maps::EmperorContainer::open(path).read_part(0);
        a::u32(logical,8,5);logical.at(56)=6;logical.at(57)=0;
        // Packed local entry90,99 plus the declared226-map border1.
        logical.at(850)=static_cast<std::uint8_t>(packed_entry.x);logical.at(851)=0;
        logical.at(852)=static_cast<std::uint8_t>(packed_entry.y);logical.at(853)=0;
        const auto bytes=encode(logical);f::write_text(path,std::string(bytes.begin(),bytes.end()));
        terrain=save::prepare_original_irrigation_terrain_facts(root.data,f::map_relative);
        source=save::prepare_original_grower_source_facts(root.data,f::map_relative);
        check(source->storage_entry_cell()==sim::Cell{packed_entry.x+1,packed_entry.y+1} &&
              source->source_map_sha256()==terrain->source_map_sha256(),
              "Shared source factory lost the actual packed entry/header/map identity");
    }
};

struct Scene {sim::BuildingId farm{},field{};std::vector<sim::BuildingId> homes;};
Scene build(sim::World& world,sim::CommandType kind=sim::CommandType::PlaceOriginalCabbageField,
            sim::Cell target={93,98}) {
    Scene result;result.farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,95});
    for(int x=70;x<=91;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int y=98;y<104;++y)f::buy(world,{sim::CommandType::PlaceRoad,{91,y}});
    for(const int x:{70,73,76,79})result.homes.push_back(q::purchase(world,sim::CommandType::PlaceHousehold,{x,102}));
    sim::Command field{kind,target};field.building_id=result.farm;f::buy(world,field);
    result.field=*world.building_owner_at(target);
    const auto entrances=world.building_entrances(result.farm);
    check(entrances.size()==1 && entrances.front().road_cell==sim::Cell{91,98} &&
          !world.total_population() && !world.workers_assigned(result.farm),
          "Paid lifecycle scene lacked its unique connected port or granted free residents/workforce");
    return result;
}

void prewarm(sim::World& world,unsigned slot) {
    for(unsigned n=0;n<slot*816U;++n)world.tick();
    check(world.agriculture_calendar()->position.month==slot,
          "Real empty-World ticks failed to reach the requested growing slot");
}
const sim::GrowerLifecycleState& state(const sim::World& world) {
    check(world.grower_lifecycle().has_value(),"World lost its explicit lifecycle controller");
    return *world.grower_lifecycle();
}
std::size_t live(const sim::World& world) {
    const auto& actors=state(world).actors;
    return static_cast<std::size_t>(std::count_if(actors.begin(),actors.end(),[](const auto& actor){return !actor.retired;}));
}
const sim::FoodGrowerActor& actor(const sim::World& world,sim::FoodGrowerId id) {
    const auto& rows=state(world).actors;
    const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    check(at!=rows.end(),"Bounded lifecycle audit lost a just-observed actor identity");return *at;
}
void historical_equal(const sim::World& world,const sim::World& control) {
    auto observed=world.snapshot();observed.navigation_session_present=false;
    check(observed==control.snapshot(),"Transient tending changed historical World state, Food, IDs, Funds or timing");
    if(world.grower_lifecycle()) {
        check(live(world)<=1 && world.couriers().size()+world.grower_navigation_tracks().size()<=
                  sim::Rules::original_production_courier_limit,
              "Autonomous acceptance exceeded its explicit peer fence/shared movement budget");
        for(const auto& row:state(world).actors)
            check(!row.native.raw_yield_word,"Tending actor acquired unsupported harvest cargo");
        for(const auto& row:state(world).fields)
            check(!row.state.quantity && row.state.history==std::array<std::uint16_t,12>{},
                  "Transient tending granted field quantity or harvest history");
    }
}
void tick(sim::World& world,sim::World& control) {world.tick();control.tick();historical_equal(world,control);}
template<class Predicate> void until(sim::World& world,sim::World& control,Predicate done,const char* why,int limit=800) {
    for(int n=0;n<limit && !done();++n)tick(world,control);
    check(done(),std::string(why)+(world.grower_lifecycle() ? ": "+state(world).blocker:""));
}
void enable(sim::World& world,const Scene& scene,const Fixture& fixture) {
    sim::GrowerLifecycleRequest request;request.farm_id=scene.farm;request.source=fixture.source;
    const auto result=world.start_grower_lifecycle(request);
    check(result.accepted,"World lifecycle admission failed: "+result.reason);
}
void drain(sim::World& world,sim::World& control) {
    world.stop_grower_lifecycle();
    until(world,control,[&]{return !live(world);},"Stopping retained a live actor",200);
    for(const auto& field:state(world).fields)
        check(!field.state.assigned_actor_word,"Retirement retained a dangling actual field assignment");
    check(world.grower_navigation_tracks().empty() && world.clear_grower_lifecycle() &&
          !world.grower_lifecycle() && world.snapshot()==control.snapshot(),
          "Lifecycle cleanup retained a channel, guard, assignment or historical mutation");
}

void source_identity_and_unsupported_admission() {
    Fixture fixture,other({90,100});auto world=fixture.world();const auto scene=build(world);
    sim::GrowerLifecycleRequest request;request.farm_id=scene.farm;request.source=fixture.source;
    const auto before=world.snapshot();
    const auto reject=[&](const sim::GrowerLifecycleRequest& bad) {
        const auto result=world.start_grower_lifecycle(bad);
        check(!result.accepted && !result.reason.empty() && !world.grower_lifecycle() &&
              world.snapshot()==before && world.next_food_grower_id()==1,
              "Rejected lifecycle context published authority, identity or session state");
    };
    auto bad=request;bad.source=other.source;reject(bad); // Real factory, different authored source bytes.
    bad=request;bad.source.reset();reject(bad);
    bad=request;bad.farm_id=scene.field;reject(bad);
    bad=request;bad.policy=static_cast<sim::GrowerLifecyclePolicy>(255);reject(bad);
    for(const auto interaction:{sim::GrowerNavigationInteraction::PeerState5,
            sim::GrowerNavigationInteraction::Suspension13c,sim::GrowerNavigationInteraction::Ferry48,
            sim::GrowerNavigationInteraction::Linked66,sim::GrowerNavigationInteraction::Exceptional14,
            sim::GrowerNavigationInteraction::Exceptional15,sim::GrowerNavigationInteraction::Exceptional16,
            sim::GrowerNavigationInteraction::GlobalGate88ec38}) {
        bad=request;bad.interaction=interaction;reject(bad);
    }
}

void autonomous_real_motion_tending_and_all_five_kinds() {
    Fixture fixture;
    const std::array cases{
        std::pair{sim::CommandType::PlaceOriginalWheatField,2U},
        std::pair{sim::CommandType::PlaceOriginalMilletField,6U},
        std::pair{sim::CommandType::PlaceOriginalRiceField,5U},
        std::pair{sim::CommandType::PlaceOriginalCabbageField,7U},
        std::pair{sim::CommandType::PlaceOriginalSoybeanField,4U}};
    for(const auto& [kind,slot]:cases) {
        auto world=fixture.world();prewarm(world,slot);const auto scene=build(world,kind);
        for(int n=0;n<200;++n)world.tick();
        check(world.total_population()==24 && world.workers_assigned(scene.farm)==12 &&
              world.building_staffed(scene.farm),"Paid houses never supplied the actual twelve Farm workers");
        auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        until(world,control,[&]{return state(world).source_callbacks==1;},"First native phase31 source callback missing");
        check(state(world).dispatch_counter==1 && !state(world).due_attempts && !state(world).spawned,
              "Full staffing bypassed the old-byte first-attempt rule");
        until(world,control,[&]{return state(world).spawned==1;},"Due World source did not autonomously allocate its Grower");
        const auto id=state(world).actors.back().id;
        check(state(world).due_attempts==1 && state(world).allocation_attempts==1 && live(world)==1 &&
              state(world).source_cell==sim::Cell{91,98} &&
              state(world).fields.front().state.assigned_actor_word==id.value &&
              state(world).retained_counters.tending==1,
              "Successful native attempt lacked one real actor, source endpoint, assignment or retained counter");
        bool changed_cell=false,shared_path=false;
        until(world,control,[&]{
            const auto& row=actor(world,id);changed_cell|=row.current!=sim::Cell{91,98};
            if(row.navigation_id) {
                const auto& path=world.grower_navigation(*row.navigation_id);
                shared_path|=path.lifecycle_owned && path.path.size()>=2 && path.current==row.current;
            }
            return row.native.raw_state==8;
        },"Actual shared movement never reached tending state8");
        const auto arrived=actor(world,id);
        check(changed_cell && shared_path && arrived.current==world.building(scene.field).cell &&
              arrived.native.wait==51 && arrived.arrived_tick==world.ticks() &&
              arrived.last_field_id==scene.field && !state(world).tending_callbacks,
              "Tending wait was not established by a real typed endpoint arrival");
        for(int eligible=1;eligible<=50;++eligible) {
            tick(world,control);
            check(!state(world).tending_callbacks && actor(world,id).native.wait==51-eligible,
                  "Tending completed before fifty-one actual eligible actor callbacks");
        }
        tick(world,control);
        check(state(world).tending_callbacks==1 && actor(world,id).tending_callbacks==1 &&
              state(world).fields.front().state.work_word==0x40 &&
              !state(world).fields.front().state.assigned_actor_word &&
              state(world).fields.front().last_tending_tick==arrived.arrived_tick+51,
              "Actual tending51 failed its exact work40/assignment-clear effect");
        until(world,control,[&]{return actor(world,id).retired;},"Worked actor never physically returned and retired");
        check(actor(world,id).current==sim::Cell{91,98} && actor(world,id).retired_tick>arrived.arrived_tick+51 &&
              actor(world,id).native==orig::CropGrowerActor{} && !actor(world,id).navigation_id &&
              !state(world).retained_counters.tending && !live(world),
              "Natural source return failed actual cell/identity/reference cleanup");
        drain(world,control);
        if(kind==sim::CommandType::PlaceOriginalCabbageField)
            check(a::stock(world,scene.farm).amounts==sim::AgricultureAmounts{},"Tending created Cabbage Food");
    }
}

void repeated_lifetimes_and_failed_peer_free_allocation() {
    Fixture fixture;auto world=fixture.world();prewarm(world,7);const auto scene=build(world,
        sim::CommandType::PlaceOriginalCabbageField,{87,92});
    for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
    until(world,control,[&]{return state(world).spawned==1;},"Long-path actor was not allocated");
    const auto first=state(world).actors.back().id;const auto next=world.next_food_grower_id();
    until(world,control,[&]{return state(world).allocation_refusals>0;},"Real due return-phase allocation never reached its peer-free refusal");
    check(live(world)==1 && !state(world).retained_counters.tending && state(world).spawned==1 &&
          world.next_food_grower_id()==next && !state(world).fields.front().state.assigned_actor_word,
          "Failed allocation consumed an ID/assignment or repaired native counters into a live census");
    until(world,control,[&]{return actor(world,first).retired;},"Long actor never completed its real return");
    check(actor(world,first).current==sim::Cell{91,98},"Long-path retirement teleported instead of returning");
    drain(world,control);

    // Re-enable the controller, without allocating a worker explicitly. The
    // same World must retain monotone identities across terminal cleanup.
    enable(world,scene,fixture);
    until(world,control,[&]{return state(world).spawned==1;},"Repeated source cycle never allocated its next lifetime");
    check(state(world).actors.back().id.value>first.value && world.next_food_grower_id()>next,
          "A repeated autonomous cycle reused an already retired actor identity");
    drain(world,control);
}

void two_complete_cycles_in_one_continuous_controller() {
    Fixture fixture;auto world=fixture.world();prewarm(world,7);const auto scene=build(world);
    for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
    until(world,control,[&]{return state(world).retired>=2;},
          "Continuously active controller did not complete two autonomous tending/return cycles",600);
    check(state(world).spawned>=2 && state(world).tending_callbacks==2 &&
          world.agriculture_calendar()->position.month==7 &&
          a::stock(world,scene.farm).amounts==sim::AgricultureAmounts{},
          "Repeated autonomous cycles crossed their admitted slot or produced native Food");
    std::vector<std::uint64_t> retired_ids;
    for(const auto& row:state(world).actors) if(row.retired) {
        check(row.current==sim::Cell{91,98} && row.tending_callbacks==1 &&
              row.native==orig::CropGrowerActor{} && !row.navigation_id,
              "Repeated lifetime failed actual return, one tending callback or full native cleanup");
        retired_ids.push_back(row.id.value);
    }
    std::sort(retired_ids.begin(),retired_ids.end());
    check(retired_ids.size()>=2 && retired_ids.front()>0 &&
          std::adjacent_find(retired_ids.begin(),retired_ids.end())==retired_ids.end(),
          "A continuously active controller reused a retired worker identity");
    drain(world,control);
}

void controller_only_guard_and_multiple_candidate_refusal() {
    Fixture fixture;auto world=fixture.world();prewarm(world,7);const auto scene=build(world);
    auto control=fixture.restore(world.snapshot());const auto clean=fixture.document(world);
    const auto path=fixture.root.path/"autonomous-guard42.json";
    save::write_save(path,clean,fixture.root.data,fixture.mask());const auto bytes=f::read_text(path);
    enable(world,scene,fixture);
    check(state(world).actors.empty() && world.grower_navigation_tracks().empty() &&
          world.snapshot().navigation_session_present,"Controller-only state bypassed the transient publication guard");
    refuses([&]{(void)fixture.document(world);},"Controller-only state entered historical save preparation");
    refuses([&]{(void)fixture.restore(world.snapshot());},"Controller-only state silently restored as historical authority");
    auto hostile=clean;hostile.world=world.snapshot();
    refuses([&]{save::write_save(path,hostile,fixture.root.data,fixture.mask());},"Controller-only state was published");
    check(f::read_text(path)==bytes,"Rejected controller publication changed existing save bytes");
    until(world,control,[&]{return state(world).source_callbacks>=2;},"Unstaffed source callbacks were not observed");
    check(!state(world).dispatch_counter && !state(world).due_attempts && !state(world).spawned &&
          world.next_food_grower_id()==1,"Unstaffed source advanced attempts, actor identity or work");
    drain(world,control);
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto reference=fixture.root.path/"autonomous-control42.json";
    save::write_save(reference,fixture.document(control),fixture.root.data,fixture.mask());
    check(f::read_text(path)==f::read_text(reference),"Controller cleanup changed canonical historical schema42 bytes");

    auto ambiguous=fixture.world();prewarm(ambiguous,7);const auto many=build(ambiguous);
    sim::Command second{sim::CommandType::PlaceOriginalCabbageField,{94,98}};second.building_id=many.farm;f::buy(ambiguous,second);
    for(int n=0;n<200;++n)ambiguous.tick();auto other=fixture.restore(ambiguous.snapshot());enable(ambiguous,many,fixture);
    until(ambiguous,other,[&]{return state(ambiguous).status==sim::GrowerLifecycleStatus::Unsupported;},
          "Multiple unresolved candidate scores were silently ordered");
    check(!state(ambiguous).spawned && !state(ambiguous).allocation_attempts &&
          state(ambiguous).actors.empty() && ambiguous.next_food_grower_id()==1 && !state(ambiguous).blocker.empty(),
          "Ambiguous candidates allocated a worker before admission was proved");
    for(const auto& field:state(ambiguous).fields)
        check(!field.state.work_word && !field.state.assigned_actor_word,"Ambiguous admission retained work or claims");
    drain(ambiguous,other);
}

void access_cut_repair_and_actual_route_replan() {
    Fixture fixture;auto world=fixture.world();prewarm(world,7);const auto scene=build(world);
    for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());
    // The native entry flood admits ordinary land as well as roads. Remove
    // the unique Farm perimeter road itself, not merely its connecting spur.
    f::buy(world,{sim::CommandType::RemoveRoad,{91,98}});f::buy(control,{sim::CommandType::RemoveRoad,{91,98}});
    enable(world,scene,fixture);
    until(world,control,[&]{return state(world).source_callbacks>=2;},"Disconnected access was not checked by the actual source");
    check(state(world).status==sim::GrowerLifecycleStatus::WaitingForAccess && !state(world).spawned &&
          !state(world).due_attempts && world.next_food_grower_id()==1,"Disconnected source invented a native endpoint or actor");
    f::buy(world,{sim::CommandType::PlaceRoad,{91,98}});f::buy(control,{sim::CommandType::PlaceRoad,{91,98}});
    until(world,control,[&]{return state(world).retired==1;},"Repaired source did not autonomously work and return");
    check(state(world).tending_callbacks==1,"Repaired route did not earn actual tending");drain(world,control);

    auto rerouted=fixture.world();prewarm(rerouted,7);const auto long_scene=build(rerouted,
        sim::CommandType::PlaceOriginalCabbageField,{95,100});
    for(int n=0;n<200;++n)rerouted.tick();auto reference=fixture.restore(rerouted.snapshot());enable(rerouted,long_scene,fixture);
    until(rerouted,reference,[&]{
        if(state(rerouted).actors.empty())return false;
        const auto& row=state(rerouted).actors.back();return row.native.raw_state==7 && row.navigation_id &&
            rerouted.grower_navigation(*row.navigation_id).edge_started;
    },"Replan scene never began a real shared movement section");
    const auto id=state(rerouted).actors.back().id;
    f::buy(rerouted,{sim::CommandType::PlaceOriginalFoodFarm,{92,99}});
    f::buy(reference,{sim::CommandType::PlaceOriginalFoodFarm,{92,99}});
    bool retried=false,replanned=false;
    until(rerouted,reference,[&]{
        const auto& row=actor(rerouted,id);
        if(row.navigation_id) {const auto& route=rerouted.grower_navigation(*row.navigation_id);
            retried|=route.result==sim::GrowerNavigationResult::Retry;replanned|=route.reroutes>0;}
        return row.retired;
    },"Changed actual path did not replan, tend and return");
    check(retried && replanned && actor(rerouted,id).tending_callbacks==1 &&
          actor(rerouted,id).current==sim::Cell{91,98},"Actual retry/replan was replaced by work or arrival inference");
    drain(rerouted,reference);
}

void new_field_membership_refuses_dispatch_and_pending_work() {
    Fixture fixture;
    for(const bool pending_work:{false,true}) {
        auto world=fixture.world();prewarm(world,7);const auto scene=build(world);
        for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        if(pending_work)
            until(world,control,[&]{return !state(world).actors.empty() && state(world).actors.back().native.raw_state==8;},
                  "Membership case never reached actual pending tending51");
        const auto next_id=world.next_food_grower_id();
        sim::Command extra{sim::CommandType::PlaceOriginalCabbageField,{94,98}};extra.building_id=scene.farm;
        f::buy(world,extra);f::buy(control,extra);
        until(world,control,[&]{return state(world).status==sim::GrowerLifecycleStatus::Unsupported && !live(world);},
              "New paid field silently changed the admitted candidate/work membership",40);
        check(state(world).fields.size()==1 && !state(world).tending_callbacks &&
              !state(world).fields.front().state.work_word && !state(world).fields.front().state.assigned_actor_word &&
              world.next_food_grower_id()==next_id && !state(world).blocker.empty(),
              "Membership change allocated another identity or completed unsupported tending");
        drain(world,control);
    }
}

void pause_staff_loss_and_natural_fire_cancel_work() {
    Fixture fixture;
    for(const bool workforce_loss:{false,true}) {
        auto world=fixture.world();prewarm(world,7);const auto scene=build(world);
        for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        until(world,control,[&]{return !state(world).actors.empty() && state(world).actors.back().native.raw_state==8;},
              "Interruption case never reached actual pending tending51");
        const auto id=state(world).actors.back().id;
        if(workforce_loss) {
            const auto source=q::purchase(world,sim::CommandType::PlaceOriginalHempFarm,{103,108});
            const auto same=q::purchase(control,sim::CommandType::PlaceOriginalHempFarm,{103,108});
            check(source==same,"Independent workforce purchase lost identity");
            f::buy(world,sim::set_building_workforce_priority(source,sim::WorkforcePriority::High));
            f::buy(control,sim::set_building_workforce_priority(source,sim::WorkforcePriority::High));
            check(world.workforce_supply()==24 && world.workers_assigned(source)==18 && !world.workers_assigned(scene.farm),
                  "Paid priority change did not produce actual Farm workforce loss");
        } else {
            f::buy(world,sim::set_building_operation(scene.farm,false));
            f::buy(control,sim::set_building_operation(scene.farm,false));
        }
        until(world,control,[&]{return actor(world,id).retired;},"Unsupported source/staff context left an actor hanging",40);
        check(!state(world).tending_callbacks && state(world).status==sim::GrowerLifecycleStatus::Unsupported,
              "Pause or lost actual staff was silently converted into successful tending");drain(world,control);
    }

    auto burning=fixture.world();prewarm(burning,7);const auto scene=build(burning);
    for(int n=0;n<1850;++n)burning.tick();auto control=fixture.restore(burning.snapshot());enable(burning,scene,fixture);
    until(burning,control,[&]{return !state(burning).actors.empty() && state(burning).actors.back().native.raw_state==8;},
          "Natural-fire case did not reach pending real field work",160);
    const auto id=state(burning).actors.back().id;
    until(burning,control,[&]{return burning.building_on_fire(scene.farm) && actor(burning,id).retired;},
          "Natural source fire did not safely retire actual pending work",100);
    check(!state(burning).tending_callbacks && state(burning).status==sim::GrowerLifecycleStatus::Unsupported,
          "Natural fire manufactured a tending completion");drain(burning,control);
}

void independent_autonomous_replay_and_source_preservation() {
    Fixture fixture;auto world=fixture.world();prewarm(world,7);const auto scene=build(world);
    for(int n=0;n<200;++n)world.tick();
    auto replay=fixture.restore(world.snapshot()),control=fixture.restore(world.snapshot());
    const auto map=f::read_text(fixture.root.data/f::map_relative);
    const auto model=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    enable(world,scene,fixture);enable(replay,scene,fixture);
    for(int step=0;step<700 && !state(world).retired;++step) {
        tick(world,control);replay.tick();historical_equal(replay,control);
        check(state(world)==state(replay) && world.next_food_grower_id()==replay.next_food_grower_id() &&
              std::equal(world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end(),
                  replay.grower_navigation_tracks().begin(),replay.grower_navigation_tracks().end()),
              "Independently restored autonomous actor/assignment/movement/work replay diverged");
    }
    check(state(world).retired==1 && state(world).tending_callbacks==1 &&
          state(world).actors.back().current==sim::Cell{91,98},
          "Independent replay never completed its real autonomous tending/return cycle");
    world.stop_grower_lifecycle();replay.stop_grower_lifecycle();
    check(world.clear_grower_lifecycle() && replay.clear_grower_lifecycle() &&
          world.snapshot()==control.snapshot() && replay.snapshot()==control.snapshot() &&
          f::read_text(fixture.root.data/f::map_relative)==map &&
          f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model,
          "Autonomous cleanup/replay changed historical state or source input bytes");
}
} // namespace

int main() {
    try {
        source_identity_and_unsupported_admission();
        autonomous_real_motion_tending_and_all_five_kinds();
        repeated_lifetimes_and_failed_peer_free_allocation();
        two_complete_cycles_in_one_continuous_controller();
        controller_only_guard_and_multiple_candidate_refusal();
        access_cut_repair_and_actual_route_replan();
        new_field_membership_refuses_dispatch_and_pending_work();
        pause_staff_loss_and_natural_fire_cancel_work();
        independent_autonomous_replay_and_source_preservation();
        std::cout<<"Autonomous transient Food Grower World acceptance passed; no native harvest/Food authority\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
