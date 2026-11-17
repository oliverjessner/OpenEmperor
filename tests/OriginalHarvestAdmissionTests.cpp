// Actual paid World work reaches the sealed harvest boundary. No numeric
// context, callback result, field development or worker state is injected.
#include "IrrigationFixture.h"
#include "maps/EmperorContainer.h"
#include "persistence/RecoveryStore.h"
#include "simulation/HarvestContextAdmission.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace q=oe_irrigation_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;
using Failure=sim::HarvestContextFailure;
using Admission=sim::HarvestContextAdmission;
using Bytes=std::vector<std::uint8_t>;

static_assert(!std::is_aggregate_v<Admission> && !std::is_default_constructible_v<Admission>);
static_assert(!std::is_constructible_v<Admission,sim::HarvestContextBindings,std::vector<Failure>>);
static_assert(!std::is_constructible_v<Admission,orig::CropHarvestContext>);
static_assert(!std::is_copy_assignable_v<Admission>);
static_assert(std::is_same_v<decltype(std::declval<const Admission&>().context()),
    const std::optional<orig::CropHarvestContext>&>);
static_assert(std::is_same_v<decltype(std::declval<const Admission&>().bindings()),
    const sim::HarvestContextBindings&>);
static_assert(!std::is_default_constructible_v<sim::GrowerSourceFacts>);
static_assert(!std::is_constructible_v<sim::GrowerSourceFacts,std::string,sim::Cell,std::int16_t>);

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
              "Authored harvest-admission map compression failed");
        const auto header=result.size();result.resize(header+12);a::u32(result,header,0x12345678U);
        a::u32(result,header+4,static_cast<std::uint32_t>(length));
        a::u32(result,header+8,static_cast<std::uint32_t>(size));
        result.insert(result.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return result;
}
struct Fixture:q::Fixture {
    std::shared_ptr<const sim::GrowerSourceFacts> source;
    explicit Fixture(std::uint32_t region=0,std::optional<sim::Cell> native800={},
                     sim::Cell pump_origin={114,98},std::uint8_t pump_orientation=0)
        :q::Fixture(pump_orientation,pump_origin) {
        if(native800) {
            raw.at(static_cast<std::size_t>(native800->y)*228U+static_cast<std::size_t>(native800->x))=0x800U;
            rebind();
        }
        auto text=q::authored_model();
        // Independent stable staffing inputs; these rows claim no native model parity.
        for(std::size_t row=0;row<3;++row) {
            oe_production_fixture::set_house_field(text,row,17,6);
            oe_production_fixture::set_house_field(text,row,1,100);
        }
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_irrigation_rules_model(root.data);
        const auto path=root.data/f::map_relative;
        auto logical=openemperor::maps::EmperorContainer::open(path).read_part(0);
        a::u32(logical,8,5);a::u32(logical,12,region);
        logical.at(56)=6;logical.at(57)=0;
        logical.at(850)=90;logical.at(851)=0;logical.at(852)=99;logical.at(853)=0;
        const auto bytes=encode(logical);f::write_text(path,std::string(bytes.begin(),bytes.end()));
        terrain=save::prepare_original_irrigation_terrain_facts(root.data,f::map_relative);
        source=save::prepare_original_grower_source_facts(root.data,f::map_relative);
        check(source->storage_entry_cell()==sim::Cell{91,100} &&
              source->source_map_sha256()==terrain->source_map_sha256(),
              "Harvest source lost its actual entry and immutable complete-map identity");
    }
};
struct Scene {sim::BuildingId farm{},field{};};
Scene build(sim::World& world,sim::CommandType crop=sim::CommandType::PlaceOriginalCabbageField) {
    Scene scene;scene.farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,95});
    for(int x=70;x<=91;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int y=98;y<104;++y)f::buy(world,{sim::CommandType::PlaceRoad,{91,y}});
    for(const int x:{70,73,76,79})(void)q::purchase(world,sim::CommandType::PlaceHousehold,{x,102});
    sim::Command command{crop,{93,98}};command.building_id=scene.farm;f::buy(world,command);
    scene.field=*world.building_owner_at(command.cell);
    const auto entrances=world.building_entrances(scene.farm);
    check(entrances.size()==1 && entrances.front().road_cell==sim::Cell{91,98} &&
          !world.total_population() && !world.workers_assigned(scene.farm),
          "Harvest scene did not pay for its unique source port or injected staff");
    return scene;
}
void prewarm(sim::World& world,unsigned slot) {for(unsigned n=0;n<slot*816U;++n)world.tick();}
const sim::GrowerLifecycleState& state(const sim::World& world) {
    check(world.grower_lifecycle().has_value(),"Harvest admission lost its active controller");
    return *world.grower_lifecycle();
}
const sim::GrowerLifecycleField& field(const sim::World& world) {
    check(state(world).fields.size()==1,"Harvest session lost its single bound field");
    return state(world).fields.front();
}
const sim::FoodGrowerActor& actor(const sim::World& world,sim::FoodGrowerId id) {
    const auto& rows=state(world).actors;
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    check(found!=rows.end(),"Harvest admission lost its actual actor lifetime");return *found;
}
std::size_t live(const sim::World& world) {
    return static_cast<std::size_t>(std::count_if(state(world).actors.begin(),state(world).actors.end(),
        [](const auto& row){return !row.retired;}));
}
void historical_equal(const sim::World& world,const sim::World& control) {
    auto snapshot=world.snapshot();snapshot.navigation_session_present=false;
    check(snapshot==control.snapshot(),"Harvest admission changed historical inventory, IDs, finance or chronology");
    if(!world.grower_lifecycle())return;
    const auto& row=field(world);
    check(std::uint64_t(row.state.quantity)+row.quantity_expired+row.quantity_vetoed==row.quantity_added &&
          row.state.history==std::array<std::uint16_t,12>{},
          "Refused harvest changed earned quantity conservation or native history");
    for(const auto& worker:state(world).actors)
        check(!worker.native.raw_yield_word,"Unqualified harvest context granted raw actor cargo");
    check(live(world)<=1 && world.couriers().size()+world.grower_navigation_tracks().size()<=
          sim::Rules::original_production_courier_limit,"Harvest admission exceeded the shared channel/peer bounds");
}
void tick(sim::World& world,sim::World& control) {world.tick();control.tick();historical_equal(world,control);}
template<class Predicate>void until(sim::World& world,sim::World& control,Predicate done,const char* why,int limit=1800) {
    for(int n=0;n<limit && !done();++n)tick(world,control);
    check(done(),std::string(why)+": "+state(world).blocker);
}
void enable(sim::World& world,const Scene& scene,const Fixture& fixture,
            sim::GrowerLifecyclePolicy policy=sim::GrowerLifecyclePolicy::AutonomousFieldCycleV1) {
    sim::GrowerLifecycleRequest request;request.policy=policy;request.farm_id=scene.farm;request.source=fixture.source;
    const auto admitted=world.start_grower_lifecycle(request);
    check(admitted.accepted,"Actual paid harvest session was refused: "+admitted.reason);
}
void drain(sim::World& world,sim::World& control) {
    world.stop_grower_lifecycle();until(world,control,[&]{return !live(world);},"Harvest worker failed to drain",80);
    check(!field(world).state.assigned_actor_word && world.grower_navigation_tracks().empty() &&
          world.clear_grower_lifecycle() && world.snapshot()==control.snapshot(),
          "Harvest cleanup retained claims, field state or historical authority");
}
bool has(const Admission& admission,Failure expected) {
    return std::find(admission.failures().begin(),admission.failures().end(),expected)!=admission.failures().end();
}
Admission inspect(const sim::World& world,sim::FoodGrowerId id) {
    const auto snapshot=world.snapshot();const auto session=world.grower_lifecycle();
    const auto next=world.next_food_grower_id();
    const std::vector<sim::GrowerNavigationTrack> tracks(world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end());
    const auto result=world.prepare_grower_harvest_context(id);
    const auto repeated=world.prepare_grower_harvest_context(id);
    check(!result.accepted() && !result.context() && !result.failures().empty() &&
          result.bindings()==repeated.bindings() && result.failures()==repeated.failures() &&
          world.snapshot()==snapshot && world.grower_lifecycle()==session && world.next_food_grower_id()==next &&
          std::equal(tracks.begin(),tracks.end(),world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end()),
          "Read-only repeated harvest preparation published authority, IDs, work or movement");
    for(const auto failure:result.failures())
        check(!sim::harvest_context_failure_message(failure).empty(),"Typed harvest refusal lost its explanation");
    return result;
}
void exact_failure(const Admission& admission,Failure expected,const char* why) {
    check(admission.failures()==std::vector<Failure>{expected},why);
}
sim::FoodGrowerId reach_harvest_wait(sim::World& world,sim::World& control) {
    bool moved=false,tended=false,developed=false,marked=false;
    until(world,control,[&]{
        tended|=state(world).tending_callbacks>0;
        developed|=field(world).quantity_added>0;
        marked|=field(world).harvest_marks>0 && (field(world).state.work_word&0x8000U);
        if(state(world).actors.empty())return false;
        const auto& row=state(world).actors.back();
        moved|=!row.retired && row.current!=sim::Cell{91,98};
        return !row.retired && row.native.raw_role==2 && row.native.raw_state==12;
    },"Autonomous paid work never reached an actual harvest wait");
    const auto& row=state(world).actors.back();
    check(moved && tended && developed && marked && row.native.wait==25 && row.navigation_id &&
          row.current==world.building(field(world).field_id).cell && row.arrived_tick==world.ticks() &&
          world.grower_navigation(*row.navigation_id).result==sim::GrowerNavigationResult::Arrived &&
          field(world).state.assigned_actor_word==row.id.value,
          "Harvest admission did not follow actual tending, growth, mark and physical arrival");
    return row.id;
}
void reach_due(sim::World& world,sim::World& control,sim::FoodGrowerId id) {
    exact_failure(inspect(world,id),Failure::WorkNotDue,"Arrival alone bypassed the actual harvest wait25");
    for(int eligible=1;eligible<=24;++eligible) {
        tick(world,control);
        check(actor(world,id).native.raw_state==12 && actor(world,id).native.wait==25-eligible &&
              !state(world).harvest_callback_blocks,"Harvest preparation ran before25 eligible worker invocations");
    }
}
void normal_missing_domains(const Admission& admission,bool event_veto) {
    const std::array required{
        Failure::X87PrecisionUnverified,Failure::X87RoundingUnverified,Failure::X87MasksUnverified,
        Failure::X87StackUnverified,Failure::X87ExceptionsUnverified,Failure::RuntimeFertilityEpochUnverified,
        Failure::RegionLifetimeUnverified,Failure::CropRegionFactorUnverified,Failure::NativeDateEpochUnverified,
        Failure::GlobalFactorUnverified,Failure::ResourceVetoUnverified,Failure::YieldDomainUnverified};
    check(admission.failures().size()==required.size()+(event_veto ? 0U:1U),
          "Normal due work omitted or invented a live-context proof obligation");
    for(const auto failure:required)check(has(admission,failure),"Normal due work hid a specific unproved live operand");
    check(has(admission,Failure::EventStateUnverified)!=event_veto &&
          (event_veto ? admission.bindings().event_doubling==false:!admission.bindings().event_doubling),
          "Event crop veto was replaced by invented enabled/state/selector values");
}
void block_due(sim::World& world,sim::World& control,sim::FoodGrowerId id,const Admission& prepared) {
    const auto before=field(world).state;const auto arrival=actor(world,id).arrived_tick;
    const auto blocks=state(world).harvest_callback_blocks;
    tick(world,control);const auto& retired=actor(world,id);
    check(retired.retired && retired.harvest_callback_blocked_tick==arrival+25 &&
          retired.harvest_context_failures==prepared.failures() &&
          state(world).harvest_callback_blocks==blocks+1 &&
          state(world).harvest_blocker==sim::GrowerHarvestBlocker::LiveYieldContext &&
          retired.native==orig::CropGrowerActor{} && !retired.navigation_id &&
          retired.current==world.building(field(world).field_id).cell &&
          field(world).state.quantity==before.quantity && field(world).state.work_word==before.work_word &&
          field(world).state.history==before.history && !(field(world).state.work_word&0x4000U) &&
          !field(world).state.assigned_actor_word,
          "Actual wait25 did not retain its exact failures or partially applied the harvest callback");
    exact_failure(inspect(world,id),Failure::WorkerInactive,"Retired actor acquired a second harvest admission");
    for(int n=0;n<10;++n)tick(world,control);
    check(state(world).harvest_callback_blocks==blocks+1 && field(world).state.quantity==before.quantity &&
          field(world).state.history==before.history,"Repeated work preparation duplicated a harvest effect");
}

void sealed_source_operands_and_identity_rejections() {
    const std::array cases{
        std::pair{0U,std::int16_t{0}},std::pair{1U,std::int16_t{1}},std::pair{2U,std::int16_t{2}},
        std::pair{0xa5a50001U,std::int16_t{1}},std::pair{0x1234ffffU,std::int16_t{-1}}};
    for(const auto [encoded,expected]:cases) {
        Fixture fixture(encoded);
        check(fixture.source->source_region_operand()==expected,"Source region did not use the signed Header12 low WORD");
    }
    Fixture first,changed_region(1);auto world=first.world();const auto scene=build(world);
    exact_failure(inspect(world,{1}),Failure::NoSession,"An ordinary World admitted an implicit harvest session");
    const auto baseline=world.snapshot();const auto next=world.next_food_grower_id();
    sim::GrowerLifecycleRequest request;request.policy=sim::GrowerLifecyclePolicy::AutonomousFieldCycleV1;
    request.farm_id=scene.farm;request.source=changed_region.source;
    check(!world.start_grower_lifecycle(request).accepted && world.snapshot()==baseline &&
          world.next_food_grower_id()==next && !world.grower_lifecycle(),
          "Changed source region/SHA admitted foreign source facts or consumed an actor identity");
    auto bad_model=baseline;
    check(bad_model.original_economy && bad_model.original_economy->model_source,"Historical model identity is absent");
    auto& sha=bad_model.original_economy->model_source->sha256;
    sha.at(0)=sha.at(0)=='a' ? 'b':'a';
    refuses([&]{(void)first.restore(bad_model);},"Foreign model fingerprint restored as a current harvest-capable World");
    check(world.snapshot()==baseline,"Rejected model/source identity changed the original World");
    enable(world,scene,first,sim::GrowerLifecyclePolicy::AutonomousTendingV1);
    exact_failure(inspect(world,{1}),Failure::UnsupportedPolicy,"Historical tending policy acquired harvest authority");
    auto control=first.restore(baseline);drain(world,control);
    enable(world,scene,first);
    exact_failure(inspect(world,{1}),Failure::UnknownActor,"Controller presence invented an actual worker lifetime");
    drain(world,control);
}

void all_five_actual_due_contexts_and_zero_cargo() {
    Fixture fixture;
    struct Case {sim::CommandType command;orig::ModelBuilding model;unsigned end;bool event_veto;};
    const std::array cases{
        Case{sim::CommandType::PlaceOriginalWheatField,orig::ModelBuilding::WheatField,6,false},
        Case{sim::CommandType::PlaceOriginalMilletField,orig::ModelBuilding::MilletField,10,false},
        Case{sim::CommandType::PlaceOriginalRiceField,orig::ModelBuilding::RiceField,9,true},
        Case{sim::CommandType::PlaceOriginalCabbageField,orig::ModelBuilding::CabbageField,11,true},
        Case{sim::CommandType::PlaceOriginalSoybeanField,orig::ModelBuilding::SoybeanField,8,false}};
    for(const auto& item:cases) {
        auto world=fixture.world();prewarm(world,item.end-1);const auto scene=build(world,item.command);
        for(int n=0;n<200;++n)world.tick();
        check(world.total_population()==24 && world.workers_assigned(scene.farm)==12,"Harvest test did not earn actual residents/staff");
        auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        const auto id=reach_harvest_wait(world,control);reach_due(world,control,id);
        const auto admission=inspect(world,id);normal_missing_domains(admission,item.event_veto);
        const auto bound=admission.bindings();
        check(bound.actor_id==id && bound.farm_id==scene.farm && bound.field_id==scene.field &&
              bound.crop==item.model && bound.tick==world.ticks() && bound.slot==item.end &&
              bound.source_map_sha256==fixture.source->source_map_sha256() &&
              bound.model_sha256==fixture.model->provenance().sha256 && bound.difficulty==fixture.model->difficulty() &&
              bound.source_region_operand==0 && bound.construction_fertility==100 &&
              bound.quantity==field(world).state.quantity && bound.work_word==field(world).state.work_word &&
              !bound.carried_word && bound.irrigated==false && bound.checked_irrigation_cells==120 &&
              !bound.unsupported_terrain_cell,
              "Sealed refusal did not bind actual World identities, source/model, due field and complete dry120 observations");
        block_due(world,control,id,admission);drain(world,control);
        check(admission.bindings()==bound && !admission.accepted() && !admission.context(),
              "Session cleanup changed a retained immutable refusal into authority");
    }
}

void dry_query_corners_source_markers_and_region_domain() {
    for(const bool outside:{false,true}) {
        Fixture fixture;auto world=fixture.world();prewarm(world,10);const auto scene=build(world);
        for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        const auto id=reach_harvest_wait(world,control);reach_due(world,control,id);
        const sim::Cell cell=outside ? sim::Cell{99,104}:sim::Cell{98,103};
        q::ditch(world,cell);q::ditch(control,cell);historical_equal(world,control);
        const auto prepared=inspect(world,id);
        if(outside) {
            normal_missing_domains(prepared,true);
            check(prepared.bindings().checked_irrigation_cells==120 && prepared.bindings().irrigated==false,
                  "Ditch outside radius5 contaminated the actual120-cell dry query");
        }else {
            exact_failure(prepared,Failure::UnsupportedIrrigationTerrain,"Paid radius5 corner Ditch was treated as dry land");
            check(prepared.bindings().unsupported_terrain_cell==cell &&
                  prepared.bindings().checked_irrigation_cells==119 && !prepared.bindings().irrigated,
                  "Dry query skipped its final native corner or invented a wet/dry answer");
        }
        block_due(world,control,id,prepared);drain(world,control);
    }
    {
        // Orientation1 keeps all eight source-water cells beyond query x98,
        // while the two land cells of the actual Pump footprint enter ring5.
        const sim::Cell origin{98,98};constexpr std::uint8_t orientation=1;
        Fixture fixture(0,{},origin,orientation);auto world=fixture.world();prewarm(world,10);
        const auto scene=build(world);
        for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        const auto id=reach_harvest_wait(world,control);reach_due(world,control,id);
        const auto dry=inspect(world,id);normal_missing_domains(dry,true);
        check(dry.bindings().irrigated==false && dry.bindings().checked_irrigation_cells==120 &&
              !dry.bindings().unsupported_terrain_cell,
              "Pump-source geography alone failed the complete dry120 control");
        const auto pump=q::pump(world,origin,orientation);
        check(q::pump(control,origin,orientation)==pump && world.workers_assigned(scene.farm)==12,
              "Paid Pump control lost its identity or displaced actual Farm workers");
        historical_equal(world,control);
        const auto preparation=inspect(world,id);
        exact_failure(preparation,Failure::UnsupportedIrrigationTerrain,"Paid Pump footprint became a dry irrigation grant");
        const auto shape=orig::pump_geometry(origin,orientation);
        check(preparation.bindings().unsupported_terrain_cell &&
              std::find(shape.footprint.begin(),shape.footprint.end(),
                  *preparation.bindings().unsupported_terrain_cell)!=shape.footprint.end() &&
              preparation.bindings().checked_irrigation_cells<120 && !preparation.bindings().irrigated,
              "Pump rejection did not identify an actual paid footprint cell in the dry query");
        block_due(world,control,id,preparation);drain(world,control);
    }
    Fixture marker(0,sim::Cell{98,103});auto world=marker.world();prewarm(world,10);const auto scene=build(world);
    for(int n=0;n<200;++n)world.tick();auto control=marker.restore(world.snapshot());enable(world,scene,marker);
    const auto id=reach_harvest_wait(world,control);reach_due(world,control,id);
    const auto unsupported=inspect(world,id);
    exact_failure(unsupported,Failure::UnsupportedIrrigationTerrain,"Native source800 marker became a dry irrigation grant");
    check(unsupported.bindings().unsupported_terrain_cell==sim::Cell{98,103} &&
          !unsupported.bindings().irrigated,"Unsupported effective terrain acquired an irrigation answer");
    block_due(world,control,id,unsupported);drain(world,control);
    for(const auto region:{3U,0xffffffffU}) {
        Fixture invalid(region);auto candidate=invalid.world();prewarm(candidate,10);const auto paid=build(candidate);
        for(int n=0;n<200;++n)candidate.tick();auto independent=invalid.restore(candidate.snapshot());enable(candidate,paid,invalid);
        const auto worker=reach_harvest_wait(candidate,independent);reach_due(candidate,independent,worker);
        const auto preparation=inspect(candidate,worker);
        exact_failure(preparation,Failure::RegionDomain,"Out-of-domain signed source region reached numeric admission");
        check(preparation.bindings().source_region_operand==(region==3 ? 3:-1),"Region rejection lost the actual signed operand");
        block_due(candidate,independent,worker,preparation);drain(candidate,independent);
    }
}

void actual_wait_cancellation_and_changed_field_membership() {
    for(const bool membership:{false,true}) {
        Fixture fixture;auto world=fixture.world();prewarm(world,10);const auto scene=build(world);
        for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
        const auto id=reach_harvest_wait(world,control);reach_due(world,control,id);
        const auto retained=field(world).state;const auto next=world.next_food_grower_id();
        if(membership) {
            sim::Command another{sim::CommandType::PlaceOriginalCabbageField,{94,98}};another.building_id=scene.farm;
            f::buy(world,another);f::buy(control,another);
            exact_failure(inspect(world,id),Failure::FieldIdentity,"Manager change reused a former single-field admission");
        }else {
            world.stop_grower_lifecycle();
            exact_failure(inspect(world,id),Failure::WorkerCancelled,"Stopped wait1 worker remained harvest-eligible");
        }
        until(world,control,[&]{return actor(world,id).retired;},"Cancelled actual harvest worker remained live",40);
        check(!state(world).harvest_callback_blocks && actor(world,id).harvest_context_failures.empty() &&
              !actor(world,id).harvest_callback_blocked_tick && world.next_food_grower_id()==next &&
              field(world).state.quantity==retained.quantity && field(world).state.work_word==retained.work_word &&
              field(world).state.history==retained.history && !field(world).state.assigned_actor_word,
              "Cancellation at actual wait1 ran the25th harvest callback or granted resources");
        drain(world,control);
    }
}

void natural_expiration_before_late_harvest_arrival() {
    Fixture fixture;auto world=fixture.world();prewarm(world,10);const auto scene=build(world);
    for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot());enable(world,scene,fixture);
    until(world,control,[&]{return state(world).tending_callbacks==1 && state(world).retired==1 && !live(world);},
          "First actual tending worker did not naturally return and retire",350);
    check(state(world).dispatch_counter==0 && !state(world).retained_counters.tending &&
          !state(world).retained_counters.harvest && !field(world).state.assigned_actor_word,
          "Late-harvest setup did not retain the actual zero counter after tending retirement");
    const auto spawned=state(world).spawned;const auto next=world.next_food_grower_id();
    f::buy(world,{sim::CommandType::RemoveRoad,{91,98}});
    f::buy(control,{sim::CommandType::RemoveRoad,{91,98}});
    until(world,control,[&]{
        const auto position=orig::scheduler_position(world.ticks());
        return position.month==11 && position.step==14 && position.phase==0;
    },"Real calendar did not reach the late source-repair boundary",1300);
    check(!live(world) && state(world).spawned==spawned && world.next_food_grower_id()==next &&
          state(world).dispatch_counter==0 && state(world).status==sim::GrowerLifecycleStatus::WaitingForAccess &&
          !state(world).stop_requested && field(world).state.quantity>0 && field(world).harvest_marks==1 &&
          (field(world).state.work_word&0x8000U) && world.workers_assigned(scene.farm)==12,
          "Access loss advanced the source counter, stopped earned growth or spawned a worker");
    f::buy(world,{sim::CommandType::PlaceRoad,{91,98}});
    f::buy(control,{sim::CommandType::PlaceRoad,{91,98}});
    const auto callbacks=state(world).source_callbacks;const auto due=state(world).due_attempts;
    until(world,control,[&]{return state(world).source_callbacks>callbacks;},"Repaired source missed step14 phase31",51);
    const auto first=orig::scheduler_position(world.ticks()-1);
    check(first.month==11 && first.step==14 && first.phase==31 && state(world).dispatch_counter==1 &&
          state(world).due_attempts==due && state(world).spawned==spawned && !live(world) &&
          world.next_food_grower_id()==next,"First actual repaired source pass did not retain old0-to1 without allocation");
    until(world,control,[&]{return state(world).source_callbacks>callbacks+1;},"Repaired source missed step15 due attempt",51);
    const auto second=orig::scheduler_position(world.ticks()-1);
    const auto id=state(world).actors.back().id;
    check(second.month==11 && second.step==15 && second.phase==31 && !state(world).dispatch_counter &&
          state(world).due_attempts==due+1 && state(world).spawned==spawned+1 && live(world)==1 &&
          id.value==next && actor(world,id).native.raw_role==2 && actor(world,id).native.raw_state==11 &&
          actor(world,id).navigation_id && field(world).state.assigned_actor_word==id.value &&
          (field(world).state.work_word&0x8000U) && field(world).state.quantity>0,
          "Late actual due attempt did not allocate a marked autonomous harvest worker");
    const auto route=world.grower_navigation(*actor(world,id).navigation_id);
    check(route.path==std::vector<sim::Cell>{{91,98},{92,98},{93,98}},
          "Late harvest did not retain its actual two-edge shared route");
    const auto quantity=field(world).state.quantity;
    until(world,control,[&]{return field(world).expirations==1;},"Actual end-slot expiration did not precede late arrival",51);
    const auto expiration=orig::scheduler_position(world.ticks()-1);
    check(expiration.month==11 && expiration.step==15 && expiration.phase==50 &&
          !field(world).state.quantity && !field(world).state.work_word && !field(world).state.assigned_actor_word &&
          field(world).quantity_expired==quantity && actor(world,id).native.raw_state==11 &&
          !actor(world,id).native.wait && !actor(world,id).retired &&
          actor(world,id).current!=world.building(scene.field).cell && !state(world).harvest_callback_blocks,
          "Expiration failed to clear the actual marked field and moving worker wait before arrival");
    until(world,control,[&]{return actor(world,id).native.raw_state==12;},"Late harvest worker never physically arrived",80);
    const auto arrived=actor(world,id);
    check(!arrived.retired && !arrived.native.wait && arrived.current==world.building(scene.field).cell &&
          arrived.arrived_tick==world.ticks() && arrived.navigation_id &&
          world.grower_navigation(*arrived.navigation_id).result==sim::GrowerNavigationResult::Arrived &&
          !world.building_on_fire(scene.farm),"Expired field did not produce actual state12/wait0 at physical arrival");
    const auto preparation=inspect(world,id);
    exact_failure(preparation,Failure::MissingHarvestMark,"Actual expired late arrival did not identify its missing harvest mark");
    check(!preparation.bindings().quantity && !preparation.bindings().work_word && !preparation.bindings().carried_word,
          "Missing-mark refusal did not bind the actual expired field operands");
    const auto expired=field(world).state;tick(world,control);
    check(actor(world,id).retired && actor(world,id).harvest_callback_blocked_tick==arrived.arrived_tick+1 &&
          actor(world,id).harvest_context_failures==std::vector<Failure>{Failure::MissingHarvestMark} &&
          state(world).harvest_callback_blocks==1 && field(world).state==expired &&
          !actor(world,id).native.raw_yield_word && actor(world,id).current==arrived.current &&
          !(field(world).state.work_word&0x4000U),
          "Actual markless invocation ran callback prefix stores, history, cargo or a fictitious Farm return");
    exact_failure(inspect(world,id),Failure::WorkerInactive,"Retired markless invocation gained a second callback");
    drain(world,control);
}

void deterministic_admission_replay_and_historical_publication_guards() {
    Fixture fixture;auto world=fixture.world();prewarm(world,10);const auto scene=build(world);
    for(int n=0;n<200;++n)world.tick();auto control=fixture.restore(world.snapshot()),replay=fixture.restore(world.snapshot());
    const auto clean=fixture.document(world);const auto path=fixture.root.path/"harvest-admission42.json";
    save::write_save(path,clean,fixture.root.data,fixture.mask());const auto before=f::read_text(path);
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(clean,fixture.mask());
    const auto metadata=recovery.root()/history/"history.json";const auto original_metadata=f::read_text(metadata);
    const auto map_bytes=f::read_text(fixture.root.data/f::map_relative);
    const auto model_bytes=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    enable(world,scene,fixture);enable(replay,scene,fixture);
    bool checked_due=false;
    for(int n=0;n<1200 && !state(world).harvest_callback_blocks;++n) {
        if(!state(world).actors.empty()) {
            const auto id=state(world).actors.back().id;
            const auto left=inspect(world,id),right=inspect(replay,id);
            check(left.bindings()==right.bindings() && left.failures()==right.failures(),"Independent admission replay diverged");
            if(actor(world,id).native.raw_state==12 && actor(world,id).native.wait==1) {
                normal_missing_domains(left,true);checked_due=true;
            }
        }
        tick(world,control);replay.tick();historical_equal(replay,control);
        check(state(world)==state(replay) && world.next_food_grower_id()==replay.next_food_grower_id() &&
              std::equal(world.grower_navigation_tracks().begin(),world.grower_navigation_tracks().end(),
                  replay.grower_navigation_tracks().begin(),replay.grower_navigation_tracks().end()),
              "Independent actual World callback/failure/claim replay diverged");
    }
    check(checked_due && state(world).harvest_callback_blocks==1,"Replay never observed actual due admission and its callback audit");
    auto active=clean;active.world=world.snapshot();
    refuses([&]{(void)fixture.document(world);},"Harvest admission session entered historical document preparation");
    refuses([&]{(void)fixture.restore(active);},"Harvest admission session restored through schema42");
    refuses([&]{save::write_save(path,active,fixture.root.data,fixture.mask());},"Harvest admission session overwrote historical JSON");
    refuses([&]{(void)recovery.write_checkpoint(history,active,fixture.mask());},"Harvest admission audit entered Recovery");
    check(f::read_text(path)==before && f::read_text(metadata)==original_metadata && recovery.catalog().histories.size()==1,
          "Rejected transient admission publication changed saved bytes or Recovery metadata");
    const auto next=world.next_food_grower_id();
    check(world.clear_grower_lifecycle() && replay.clear_grower_lifecycle() && world.snapshot()==control.snapshot() &&
          replay.snapshot()==control.snapshot(),"Read-only admission/audit survived historical cleanup");
    exact_failure(inspect(world,{}),Failure::NoSession,"Cleared admission retained session authority");
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto reference=fixture.root.path/"historical-control42.json";
    save::write_save(reference,fixture.document(control),fixture.root.data,fixture.mask());
    const auto checkpoint=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
    check(f::read_text(path)==f::read_text(reference) && save::read_save(path).source_schema_version==42 &&
          fixture.restore(save::read_save(checkpoint.path)).snapshot()==control.snapshot() &&
          f::read_text(fixture.root.data/f::map_relative)==map_bytes &&
          f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model_bytes &&
          world.next_food_grower_id()==next,"Admission cleanup changed historical schema42/recovery, identities or source bytes");
}
} // namespace

int main() {
    try {
        sealed_source_operands_and_identity_rejections();
        all_five_actual_due_contexts_and_zero_cargo();
        dry_query_corners_source_markers_and_region_domain();
        actual_wait_cancellation_and_changed_field_membership();
        natural_expiration_before_late_harvest_arrival();
        deterministic_admission_replay_and_historical_publication_guards();
        std::cout<<"Sealed live harvest admission, paid due-work refusals and historical guards PASS; positive raw cargo remains BLOCKED\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
