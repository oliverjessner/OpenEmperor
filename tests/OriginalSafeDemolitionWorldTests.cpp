// Rule19 acceptance uses paid construction, ordinary production/movement and
// actual immigration. Hostile snapshot edits below are rejection tests only.
#include "OriginalProductionFixture.h"
#include "simulation/OriginalSafeDemolitionState.h"
#include "simulation/OriginalTerrainFacts.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using f::check;
struct Fixture {
    f::Root root;
    std::shared_ptr<const sim::MapPermissions> policy=std::make_shared<const sim::MapPermissions>(228,228,3,
        std::vector<sim::MapCellPermission>(228*228,{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None}),
        std::vector<sim::FixedGatePassage>{});
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain=std::make_shared<const sim::OriginalTerrainFacts>(
        std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count,0x80U),std::string(64,'a'));
    Fixture() {
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",oe_production_fixture::authored_model_text(37,0,1));
        model=f::save::prepare_original_production_rules_model(root.data);
    }
    sim::World world() const {return sim::World::create_original_safe_demolition(policy,model,terrain);}
    sim::World old() const {return sim::World::create_original_deadline_food_dispatch(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& state) const {
        return sim::World::restore_original_safe_demolition(state,policy,model,terrain);
    }
};
sim::BuildingId buy(sim::World& world,sim::CommandType type,sim::Cell at) {
    f::buy(world,{type,at});return *world.building_owner_at(at);
}
void road(sim::World& world,int x,int y=22) {
    if (world.object_at({x,y})!=sim::Object::Road) f::buy(world,{sim::CommandType::PlaceRoad,{x,y}});
}
sim::CourierId actor(const sim::World& world,sim::BuildingId owner,sim::CourierRole role) {
    for (const auto& row:world.couriers()) if (row.owner==owner && row.role==role) return row.id;
    throw std::runtime_error("Paid source lost its owned courier");
}
const sim::OriginalSafeDemolitionState& history(const sim::World& world) {
    check(world.original_production_state() && world.original_production_state()->safe_demolition.has_value(),
        "Rule19 lacks its explicit retirement authority");
    return *world.original_production_state()->safe_demolition;
}
void invariant(const sim::World& world) {
    check(world.production_balance_valid() && world.navigation_valid() && world.city_economy_valid() &&
        world.population_valid() && world.fire_state_valid(),"Paid Rule19 World invariant failed");
    // Independent closed-lifetime upkeep oracle, including a retired Clay's
    // last charged installation interval rather than its current absence.
    std::uint64_t maintenance=0,taxes=0;
    for (const auto& body:world.buildings()) {
        maintenance+=(world.ticks()-body.placed_tick)/816*static_cast<std::uint64_t>(world.workforce_required(body.id))*2;
        taxes+=body.taxes_paid_total;
    }
    for (const auto& row:history(world).retired) if (row.kind==sim::OriginalRetiredKind::ClaySource)
        maintenance+=(row.removed_tick-row.placed_tick)/816*static_cast<std::uint64_t>(sim::Rules::clay_source_workers)*2;
    check(maintenance==world.maintenance_spent_total() && taxes==world.taxes_collected_total(),
        "Retired installation upkeep or retained House taxes disappeared");
    check(world.treasury()==12000+static_cast<std::int64_t>(taxes)-static_cast<std::int64_t>(world.construction_spent_total())-
        static_cast<std::int64_t>(maintenance),"Paid Rule19 signed Funds equation differs");
    for (const auto& balance:world.original_goods_balances()) check(balance.conserved,"Rule19 six-goods balance failed");
}
void through(sim::World& world,std::uint64_t tick) {
    check(world.ticks()<=tick,"Paid Rule19 test passed its intended clock boundary");
    while (world.ticks()<tick) world.tick();invariant(world);
}
template<class Predicate> void until(sim::World& world,Predicate done,const char* reason,int limit=4000) {
    for (int n=0;n<limit && !done();++n) world.tick();
    check(done(),reason);invariant(world);
}
void rejected(sim::World& world,sim::BuildingId id,sim::DemolitionBlocker expected) {
    const auto before=world.snapshot();const auto canonical=world.canonical_state();
    const auto revision=world.route_refresh_count();
    const auto status=world.demolition_status(id);
    const auto validation=world.validate(sim::demolish_building(id));
    const auto result=world.execute(sim::demolish_building(id));
    check(!status.allowed && status.blocker==expected && !status.reason.empty() &&
        !validation.accepted && !result.accepted && result.reason==status.reason,
        "Unexpected Rule19 refusal: "+status.reason);
    check(world.snapshot()==before && world.canonical_state()==canonical && world.route_refresh_count()==revision,
        "Rejected Rule19 demolition changed authority, IDs, money or route caches");
}
void refused_command(sim::World& world,sim::Command command,
                     std::optional<sim::BuildBlocker> blocker=std::nullopt) {
    const auto before=world.snapshot();const auto canonical=world.canonical_state();
    const auto refreshes=world.route_refresh_count();
    const auto checked=world.validate(command);const auto result=world.execute(command);
    check(!checked.accepted && !checked.changed && !result.accepted && !result.changed &&
        !checked.reason.empty() && result.reason==checked.reason &&
        (!blocker || (checked.diagnostic.blocker==*blocker && result.diagnostic.blocker==*blocker)),
        "Expected bounded Rule19 command refusal: "+result.reason);
    check(world.snapshot()==before && world.canonical_state()==canonical && world.route_refresh_count()==refreshes,
        "Refused Rule19 placement/infrastructure command changed complete authority or route caches");
}
sim::OriginalRetiredPurchase remove(sim::World& world,sim::BuildingId id) {
    const auto body=world.building(id);const auto before=world.snapshot();
    const auto refresh=world.route_refresh_count();const auto population=world.total_population();
    const auto own=static_cast<std::size_t>(std::count_if(world.couriers().begin(),world.couriers().end(),
        [&](const auto& row){return row.owner==id;}));
    const auto status=world.demolition_status(id);
    const auto result=world.execute(sim::demolish_building(id));
    check(status.allowed && result.accepted && result.changed,"Paid Rule19 demolition failed: "+result.reason);
    check(world.ticks()==before.ticks && world.treasury()==before.treasury &&
        world.construction_spent_total()==before.construction_spent_total && world.maintenance_spent_total()==before.maintenance_spent_total &&
        world.taxes_collected_total()==before.taxes_collected_total && world.next_building_id()==before.next_building_id &&
        world.next_courier_id()==before.next_courier_id && world.command_sequence()==before.command_sequence+1 &&
        world.road_revision()==before.road_revision+1 && world.route_refresh_count()==refresh+1 &&
        world.total_population()==population && world.buildings().size()+1==before.buildings.size() &&
        world.couriers().size()+own==before.couriers.size() && world.snapshot().demolition_history==sim::DemolitionHistory{},
        "Rule19 demolition refunded money, reset IDs/population or refreshed topology more than once");
    for (const auto cell:sim::building_footprint_cells(world.profile(),world.rule_version(),body.kind,body.cell))
        check(world.object_at(cell)==sim::Object::Empty && !world.building_owner_at(cell),"Retired footprint retained a physical owner");
    for (const auto& row:world.couriers()) {
        check(row.owner!=id && row.target!=id && row.last_dispatched_target!=id,"Retired identity remains in live courier authority");
        for (const auto& route:row.dynamic_target_routes) check(route.first!=id,"Retired target remains in a derived route cache");
    }
    const auto& rows=history(world).retired;
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.building_id==id;});
    check(found!=rows.end() && found->origin_x==body.cell.x && found->origin_y==body.cell.y &&
        found->placed_tick==body.placed_tick && found->removed_tick==before.ticks && found->clay_extracted==body.clay_extracted,
        "Retirement lost exact paid purchase geometry/lifetime/counters");
    invariant(world);return *found;
}
template<class Action> void refuses_restore(Action action,const char* reason) {
    try {action();} catch (const std::exception&) {return;}
    throw std::runtime_error(reason);
}
void exact_identity_empty_house_and_neutral_history() {
    Fixture fixture;auto world=fixture.world();
    check(world.rule_version()==19 && world.active_rules().version==19 && world.original_safe_demolition_enabled() &&
        !sim::original_deadline_food_dispatch_enabled(world.profile(),19) &&
        !sim::original_deadline_service_dispatch_enabled(world.profile(),19) &&
        !sim::original_deadline_fire_dispatch_enabled(world.profile(),19) &&
        !sim::original_inspector_coordination_enabled(world.profile(),19) &&
        !sim::original_cardinal_water_enabled(world.profile(),19) &&
        world.original_deadline_food_dispatch_enabled() && world.original_worker_upkeep_interval()==816 &&
        sim::current_rule_version(sim::RulesProfile::CityV16)==4 && sim::current_rule_version(sim::RulesProfile::CityV11)==3 &&
        history(world).retired.empty(),"Rule19 widened historical exact generation predicates or changed defaults");
    refuses_restore([&]{sim::World direct(fixture.policy,sim::RulesProfile::CityV16,19);},"Ordinary constructor enabled Rule19");
    refuses_restore([&]{(void)sim::World::create_original_safe_demolition(fixture.policy,fixture.model,nullptr);},"Rule19 accepted absent terrain");
    auto old=fixture.old();const auto old_house=buy(old,sim::CommandType::PlaceHousehold,{20,20});
    check(!old.original_production_state()->safe_demolition && !old.demolition_status(old_house).allowed,
        "Historical Rule18 acquired safe demolition");
    refuses_restore([&]{(void)fixture.restore(old.snapshot());},"Rule18 silently migrated to19");
    refuses_restore([&]{(void)sim::World::restore_original_deadline_food_dispatch(world.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "Rule19 restored through Rule18");
    const auto house=buy(world,sim::CommandType::PlaceHousehold,{20,20});
    const auto before=world.snapshot();const auto canonical=world.canonical_state();
    for (int repeat=0;repeat<8;++repeat) check(world.demolition_status(house).allowed,"Pure empty-House query changed its result");
    check(world.snapshot()==before && world.canonical_state()==canonical,"Demolition query changed World authority");
    const auto retired=remove(world,house);
    check(retired.kind==sim::OriginalRetiredKind::EmptyHouse && !retired.courier_id && !retired.clay_trip &&
        world.original_economy_state()->houses.empty() && world.original_production_state()->food.stocks.empty() &&
        world.original_production_state()->housing.houses.empty() && world.original_production_state()->water.houses.empty() &&
        world.original_production_state()->finance.houses.empty() && world.original_production_state()->house_consumption.empty() &&
        world.total_population()==0 && world.workforce_supply()==0,"Never-admitted House left live parallel owner records");
    auto restored=fixture.restore(world.snapshot());check(restored.snapshot()==world.snapshot(),"Empty House retirement restore differs");
    const auto funds=restored.treasury();const auto spent=restored.construction_spent_total();
    const auto replacement=buy(restored,sim::CommandType::PlaceHousehold,{20,20});
    check(replacement>house && restored.treasury()==funds-sim::Rules::household_cost &&
        restored.construction_spent_total()==spent+static_cast<std::uint64_t>(sim::Rules::household_cost) &&
        restored.household_population(replacement)==0,"House rebuild reused identity, inherited residents or avoided its cost");
    const auto neutral=buy(restored,sim::CommandType::PlaceOriginalInfluenceSource,{30,30});
    f::buy(restored,sim::demolish_building(neutral));
    check(history(restored).retired.size()==1 && restored.original_influence_state()->removed_sources.size()==1,
        "Rule19 changed neutral Well72 removal into production retirement");
    check(fixture.restore(restored.snapshot()).snapshot()==restored.snapshot(),"Mixed neutral/House retirement restore differs");
    invariant(restored);
}
struct ClayScene {sim::BuildingId clay{},kiln{},warehouse{};sim::CourierId courier{};};
ClayScene clay_scene(sim::World& world) {
    for (int x=20;x<=35;++x) road(world,x);
    // Clay's actor precedes the other live actors. Its later removal creates
    // an actual ID gap; a new Inspector must not use IDs as vector indexes.
    ClayScene scene;
    scene.clay=buy(world,sim::CommandType::PlaceClaySource,{20,23});
    scene.kiln=buy(world,sim::CommandType::PlacePottery,{23,23});
    scene.warehouse=buy(world,sim::CommandType::PlaceWarehouse,{26,23});
    (void)buy(world,sim::CommandType::PlaceHousehold,{20,20});
    (void)buy(world,sim::CommandType::PlaceHousehold,{23,20});
    scene.courier=actor(world,scene.clay,sim::CourierRole::Clay);return scene;
}
void first_clay_departure(sim::World& world,const ClayScene& scene) {
    until(world,[&]{return world.courier(scene.courier).phase==sim::CourierPhase::ToWarehouse;},"Ordinary paid Clay never loaded");
    check(world.building(scene.clay).clay_extracted==1 && world.building(scene.clay).output==0 &&
        world.building(scene.clay).progress==0 && world.courier(scene.courier).cargo==1 &&
        world.courier(scene.courier).reserved==1 && world.building(scene.kiln).reserved_incoming==1,
        "Positive Clay drain did not originate in one real extracted, loaded, reserved unit");
    f::buy(world,sim::set_building_operation(scene.clay,false));
}
void paid_clay_drain_retirement_upkeep_rebuild_and_gap_restore() {
    Fixture fixture;auto world=fixture.world();const auto scene=clay_scene(world);
    rejected(world,scene.clay,sim::DemolitionBlocker::OperationEnabled);
    first_clay_departure(world,scene);rejected(world,scene.clay,sim::DemolitionBlocker::OwnedActiveCourier);
    until(world,[&]{return world.courier(scene.courier).phase==sim::CourierPhase::Returning;},"Loaded Clay failed to arrive");
    rejected(world,scene.clay,sim::DemolitionBlocker::OwnedActiveCourier);
    until(world,[&]{return world.courier(scene.courier).phase==sim::CourierPhase::IdleAtWorkshop;},"Real Clay courier did not return");
    through(world,900);check(world.maintenance_spent_total()==24,"Paid Clay scene failed its actual816t installed upkeep");
    const auto body=world.building(scene.clay);
    const auto trip=world.original_production_state()->industry.clay_trips.front();
    check(trip.loaded==1 && trip.delivered==1 && trip.arrival_tick && world.building(scene.kiln).input_clay==1,
        "Drained source lacks a physical destination and completed trip");
    const auto retired=remove(world,scene.clay);
    check(retired.kind==sim::OriginalRetiredKind::ClaySource && retired.courier_id==scene.courier && retired.clay_trip==trip &&
        retired.clay_extracted==1 && world.original_production_state()->industry.clay_trips.empty() &&
        world.clay_extracted_total()==1 && world.building(scene.kiln).input_clay==1,
        "Retirement lost Clay, delivered twice or discarded its terminal sequence");
    auto restored=fixture.restore(world.snapshot());check(restored.snapshot()==world.snapshot(),"Earned Clay retirement restore differs");
    const auto funds=world.treasury();const auto spent=world.construction_spent_total();
    const auto replacement=buy(world,sim::CommandType::PlaceClaySource,body.cell);
    check(replacement>scene.clay && world.treasury()==funds-sim::Rules::clay_source_cost &&
        world.construction_spent_total()==spent+static_cast<std::uint64_t>(sim::Rules::clay_source_cost) &&
        world.building(replacement).placed_tick==900 && world.building(replacement).clay_extracted==0 &&
        actor(world,replacement,sim::CourierRole::Clay)>scene.courier,"Clay rebuild inherited production, reused an ID or avoided payment");
    f::buy(world,sim::set_building_operation(replacement,false));
    const auto watch=buy(world,sim::CommandType::PlaceFireWatch,{29,23});
    const auto inspector=actor(world,watch,sim::CourierRole::FireInspector);
    check(static_cast<std::uint32_t>(inspector)>world.couriers().size(),"Paid gap proof did not actually create a nonindex CourierId");
    until(world,[&]{return !world.original_production_state()->fire_protection->arrivals.empty();},"Post-retirement Inspector did not arrive");
    restored=fixture.restore(world.snapshot());check(restored.snapshot()==world.snapshot(),"Inspector restore used gapped ID as vector index");
    const auto maintenance=world.maintenance_spent_total();
    while (world.ticks()<1800) {
        world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Retirement/rebuild continuation differs");
    }
    check(world.maintenance_spent_total()>maintenance,"New paid installations were never billed");
    invariant(world);

    const auto good=world.snapshot();
    auto hostile=good;hostile.buildings.insert(hostile.buildings.begin(),sim::BuildingSnapshot{body.id,body.kind,body.cell,true});
    refuses_restore([&]{(void)fixture.restore(hostile);},"Resurrected retired BuildingId restored");
    hostile=good;hostile.treasury+=sim::Rules::clay_source_cost;
    refuses_restore([&]{(void)fixture.restore(hostile);},"Fabricated demolition refund restored");
    hostile=good;hostile.original_production->safe_demolition->retired.clear();
    refuses_restore([&]{(void)fixture.restore(hostile);},"Lost retirement purchase provenance restored");
    hostile=good;++hostile.original_production->safe_demolition->retired.front().clay_extracted;
    refuses_restore([&]{(void)fixture.restore(hostile);},"Fabricated historical extraction restored");
    hostile=good;hostile.original_production->safe_demolition->retired.front().clay_trip->transfer.sequence=0;
    refuses_restore([&]{(void)fixture.restore(hostile);},"Invalid retired trip sequence restored");
    hostile=good;hostile.original_production->safe_demolition->retired.front().clay_trip->arrival_tick.reset();
    refuses_restore([&]{(void)fixture.restore(hostile);},"Unsettled retired Clay trip restored");
    hostile=good;hostile.couriers.front().id=*good.original_production->safe_demolition->retired.front().courier_id;
    refuses_restore([&]{(void)fixture.restore(hostile);},"Reused retired CourierId restored");
    check(world.snapshot()==good,"Hostile restore mutated the paid source World");
}
void paid_road_wait_keeps_cargo_and_allows_only_real_settlement() {
    Fixture fixture;auto world=fixture.world();const auto scene=clay_scene(world);first_clay_departure(world,scene);
    f::buy(world,{sim::CommandType::RemoveRoad,{22,22}});
    until(world,[&]{return world.courier(scene.courier).route_pending && world.courier(scene.courier).edge_progress==0;},
        "Paid missing road did not retain its waiting actor");
    check(world.courier(scene.courier).cargo==1 && world.courier(scene.courier).reserved==1 &&
        world.building(scene.kiln).input_clay==0 && world.building(scene.kiln).reserved_incoming==1,
        "Road cut or refusal lost physical Clay/reservation or invented arrival");
    rejected(world,scene.clay,sim::DemolitionBlocker::OwnedActiveCourier);
    auto restored=fixture.restore(world.snapshot());
    for (int n=0;n<20;++n) {world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Waiting Clay restore differs");}
    road(world,22);
    until(world,[&]{return world.courier(scene.courier).phase==sim::CourierPhase::Returning;},"Paid road repair did not deliver Clay");
    rejected(world,scene.clay,sim::DemolitionBlocker::OwnedActiveCourier);
    until(world,[&]{return world.courier(scene.courier).phase==sim::CourierPhase::IdleAtWorkshop;},"Repaired Clay did not return home");
    (void)remove(world,scene.clay);
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Repaired/retired Clay restore differs");
}
void occupied_elite_dependencies_stock_progress_fire_and_history_refuse() {
    Fixture fixture;
    {
        auto world=fixture.world();road(world,20);road(world,21);
        const auto house=buy(world,sim::CommandType::PlaceHousehold,{20,20});through(world,120);
        check(world.household_population(house)==2,"Occupied-House refusal lacks real immigration");
        rejected(world,house,sim::DemolitionBlocker::OccupiedHouse);
        const auto elite=buy(world,sim::CommandType::PlaceOriginalElitePlot,{30,25});
        rejected(world,elite,sim::DemolitionBlocker::UnsupportedKind);
        for (int x=30;x<34;++x) road(world,x,29);
        until(world,[&]{return world.building(elite).kind==sim::Object::OriginalEliteHouse;},"Paid Elite never materialized its actual4x4");
        rejected(world,elite,sim::DemolitionBlocker::UnsupportedKind);
        for (int y=25;y<29;++y) for (int x=30;x<34;++x) check(world.building_owner_at({x,y})==elite,"Elite refusal changed its4x4 body");
    }
    {
        auto world=fixture.world();
        const auto farm=buy(world,sim::CommandType::PlaceOriginalFoodFarm,{20,20});
        sim::Command field{sim::CommandType::PlaceOriginalMilletField,{23,20}};field.building_id=farm;f::buy(world,field);
        rejected(world,farm,sim::DemolitionBlocker::DependentField);
        rejected(world,*world.building_owner_at(field.cell),sim::DemolitionBlocker::DependentField);
        const auto hemp=buy(world,sim::CommandType::PlaceOriginalHempFarm,{30,20});
        field={sim::CommandType::PlaceOriginalHempField,{32,20}};field.building_id=hemp;f::buy(world,field);
        rejected(world,hemp,sim::DemolitionBlocker::DependentField);
        rejected(world,*world.building_owner_at(field.cell),sim::DemolitionBlocker::DependentField);
        const auto tea=buy(world,sim::CommandType::PlaceOriginalTeaCuringShed,{40,20});
        sim::Command plant{sim::CommandType::PlaceOriginalTeaBush,{42,20}};plant.building_id=tea;f::buy(world,plant);
        rejected(world,tea,sim::DemolitionBlocker::DependentField);
        rejected(world,*world.building_owner_at(plant.cell),sim::DemolitionBlocker::DependentField);
        for (const auto [type,at]:{
            std::pair{sim::CommandType::PlaceWarehouse,sim::Cell{50,20}},
            std::pair{sim::CommandType::PlaceMarket,sim::Cell{54,20}},
            std::pair{sim::CommandType::PlacePottery,sim::Cell{58,20}},
            std::pair{sim::CommandType::PlaceServicePost,sim::Cell{62,20}},
            std::pair{sim::CommandType::PlaceOriginalWaterWell,sim::Cell{66,20}},
            std::pair{sim::CommandType::PlaceFireWatch,sim::Cell{70,20}},
            std::pair{sim::CommandType::PlaceOriginalPaperMaker,sim::Cell{74,20}},
            std::pair{sim::CommandType::PlaceOriginalSilkwormShed,sim::Cell{78,20}},
            std::pair{sim::CommandType::PlaceOriginalLacquerRefinery,sim::Cell{82,20}}})
            rejected(world,buy(world,type,at),sim::DemolitionBlocker::UnsupportedKind);
    }
    {
        auto world=fixture.world();for (int x=20;x<35;++x) road(world,x);
        const auto clay=buy(world,sim::CommandType::PlaceClaySource,{20,23});
        (void)buy(world,sim::CommandType::PlaceHousehold,{30,20});
        until(world,[&]{return world.building(clay).output>0;},"Paid isolated source did not extract Clay");
        f::buy(world,sim::set_building_operation(clay,false));
        rejected(world,clay,sim::DemolitionBlocker::StoredGoods);
    }
    {
        auto world=fixture.world();for (int x=20;x<35;++x) road(world,x);
        const auto clay=buy(world,sim::CommandType::PlaceClaySource,{20,23});
        (void)buy(world,sim::CommandType::PlaceHousehold,{30,20});
        until(world,[&]{return world.building(clay).progress==1;},"Paid source never began actual extraction");
        f::buy(world,sim::set_building_operation(clay,false));
        rejected(world,clay,sim::DemolitionBlocker::ActiveProduction);
    }
    {
        auto world=fixture.world();const auto clay=buy(world,sim::CommandType::PlaceClaySource,{20,23});
        f::buy(world,sim::set_building_operation(clay,false));
        until(world,[&]{return world.building_on_fire(clay);},"Paid paused source did not follow natural fire clock");
        rejected(world,clay,sim::DemolitionBlocker::OnFire);
    }
    {
        auto world=fixture.world();for (int x=20;x<=35;++x) road(world,x);
        const auto clay=buy(world,sim::CommandType::PlaceClaySource,{20,23});f::buy(world,sim::set_building_operation(clay,false));
        (void)buy(world,sim::CommandType::PlaceHousehold,{30,20});(void)buy(world,sim::CommandType::PlaceHousehold,{33,20});
        const auto watch=buy(world,sim::CommandType::PlaceFireWatch,{22,23});const auto inspector=actor(world,watch,sim::CourierRole::FireInspector);
        until(world,[&]{return world.courier(inspector).phase==sim::CourierPhase::ToWarehouse && world.courier(inspector).target==clay;},
            "Paid Inspector never committed to nearest empty Clay source");
        rejected(world,clay,sim::DemolitionBlocker::IncomingDelivery);
        until(world,[&]{return !world.original_production_state()->fire_protection->arrivals.empty();},"Actual Inspector did not arrive");
        f::buy(world,sim::set_building_operation(watch,false));
        until(world,[&]{return world.courier(inspector).phase==sim::CourierPhase::IdleAtWorkshop;},"Actual Inspector did not return");
        rejected(world,clay,sim::DemolitionBlocker::HistoricalReference);
        check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Retained Inspector-proof refusal failed restore");
    }
}
void bounded_history_and_missing_ids() {
    Fixture fixture;auto world=fixture.world();
    rejected(world,static_cast<sim::BuildingId>(999),sim::DemolitionBlocker::MissingBuilding);
    // Synthetic counter boundary only; all purchase/House authority was paid.
    auto boundary=fixture.world();const auto empty=buy(boundary,sim::CommandType::PlaceHousehold,{20,20});
    auto state=boundary.snapshot();state.command_sequence=UINT64_MAX-1;
    boundary=fixture.restore(state);rejected(boundary,empty,sim::DemolitionBlocker::ExhaustedCounter);
    for (std::size_t n=0;n<sim::original_safe_demolition_retired_limit;++n) {
        const auto id=buy(world,sim::CommandType::PlaceHousehold,{20,20});(void)remove(world,id);
    }
    check(history(world).retired.size()==8 && world.next_building_id()==9,"Bounded retirement history reused a paid identity");
    auto restored=fixture.restore(world.snapshot());
    const auto id=buy(restored,sim::CommandType::PlaceHousehold,{20,20});
    rejected(restored,id,sim::DemolitionBlocker::HistoryCapacity);
    invariant(restored);
}
void paid_goods_food_and_house_visitors_refuse_atomically() {
    Fixture fixture;auto world=fixture.world();
    for(int x=20;x<=47;++x)road(world,x,24);
    (void)buy(world,sim::CommandType::PlaceClaySource,{20,25});
    (void)buy(world,sim::CommandType::PlacePottery,{23,25});
    const auto warehouse=buy(world,sim::CommandType::PlaceWarehouse,{26,25});
    const auto market=buy(world,sim::CommandType::PlaceMarket,{29,25});
    const auto farm=buy(world,sim::CommandType::PlaceOriginalFoodFarm,{32,25});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{35,25}};field.building_id=farm;f::buy(world,field);
    const auto water=buy(world,sim::CommandType::PlaceOriginalWaterWell,{37,25});
    const auto health=buy(world,sim::CommandType::PlaceHealthPost,{40,25});
    const auto service=buy(world,sim::CommandType::PlaceServicePost,{43,25});
    (void)buy(world,sim::CommandType::PlaceHousehold,{20,22});
    (void)buy(world,sim::CommandType::PlaceHousehold,{23,22});
    // Four normally admitted Houses can staff all47 installed workers even
    // after their real2x2 evolution reaches Hut's14-resident capacity. Two
    // homes cannot keep both medical providers staffed in this paid plan.
    (void)buy(world,sim::CommandType::PlaceHousehold,{26,22});
    (void)buy(world,sim::CommandType::PlaceHousehold,{29,22});
    const auto goods_actor=actor(world,warehouse,sim::CourierRole::MarketPotteryInbound);
    const auto farm_actor=actor(world,farm,sim::CourierRole::OriginalFoodToMarket);
    const auto food_actor=actor(world,market,sim::CourierRole::OriginalFoodToHouse);
    const auto water_actor=actor(world,water,sim::CourierRole::OriginalWater);
    const auto health_actor=actor(world,health,sim::CourierRole::HealthWorker);
    const auto service_actor=actor(world,service,sim::CourierRole::Service);
    bool goods_checked=false,farm_checked=false,food_checked=false,water_checked=false,health_checked=false,service_checked=false;
    const auto complete=[&]{return goods_checked&&farm_checked&&food_checked&&water_checked&&health_checked&&service_checked;};
    for(int n=0;n<1800&&!complete();++n) {
        world.tick();
        if(!goods_checked&&world.courier(goods_actor).phase==sim::CourierPhase::ToWarehouse&&world.original_courier_cargo(goods_actor)>0) {
            check(world.original_production_state()->pottery_handed_off>0&&world.courier(goods_actor).target==market,
                "Goods refusal lacks actual produced Ceramics and a loaded Warehouse-to-Market trip");
            rejected(world,warehouse,sim::DemolitionBlocker::UnsupportedKind);
            rejected(world,market,sim::DemolitionBlocker::UnsupportedKind);goods_checked=true;
        }
        if(!farm_checked&&world.courier(farm_actor).phase==sim::CourierPhase::ToWarehouse&&world.original_food_cargo(farm_actor)>0) {
            check(world.original_production_state()->food.farms.front().harvested>0&&world.courier(farm_actor).target==market,
                "Farm refusal lacks actual harvested, loaded Food");
            rejected(world,farm,sim::DemolitionBlocker::DependentField);
            rejected(world,market,sim::DemolitionBlocker::UnsupportedKind);farm_checked=true;
        }
        if(!food_checked&&world.courier(food_actor).phase==sim::CourierPhase::ToWarehouse&&world.original_food_cargo(food_actor)>0) {
            const auto target=world.courier(food_actor).target;
            check(world.household_population(target)>0,"Food visitor targeted an unearned House");
            rejected(world,market,sim::DemolitionBlocker::UnsupportedKind);
            rejected(world,target,sim::DemolitionBlocker::IncomingDelivery);food_checked=true;
        }
        for(const auto [owner,id,flag]:{
            std::tuple{water,water_actor,&water_checked},std::tuple{health,health_actor,&health_checked},
            std::tuple{service,service_actor,&service_checked}}) {
            if(*flag||world.courier(id).phase!=sim::CourierPhase::ToWarehouse)continue;
            const auto target=world.courier(id).target;
            check(world.household_population(target)>0&&!world.courier(id).cargo&&!world.courier(id).reserved,
                "Service refusal lacks an actual occupied-House cargo-free visitor");
            rejected(world,owner,sim::DemolitionBlocker::UnsupportedKind);
            rejected(world,target,sim::DemolitionBlocker::IncomingDelivery);*flag=true;
        }
    }
    const auto missing=std::string("goods=")+std::to_string(goods_checked)+" farm="+std::to_string(farm_checked)+
        " house-food="+std::to_string(food_checked)+" water="+std::to_string(water_checked)+
        " herbalist="+std::to_string(health_checked)+" acupuncture="+std::to_string(service_checked)+
        " population="+std::to_string(world.total_population())+" workers-used="+std::to_string(world.workforce_used())+
        " workers-required="+std::to_string(world.workforce_required());
    auto diagnostic=missing;
    for(const auto id:{goods_actor,farm_actor,food_actor,water_actor,health_actor,service_actor}) {
        const auto& courier=world.courier(id);
        diagnostic+="; actor"+std::to_string(static_cast<unsigned>(id))+"="+sim::courier_phase_name(courier.phase)+
            " cargo="+std::to_string(world.original_courier_cargo(id))+" workers="+std::to_string(world.workers_assigned(courier.owner))+
            "/"+std::to_string(world.workforce_required(courier.owner));
    }
    check(complete(),"Paid Rule19 visitor refusal controls did not all become physical: "+diagnostic);
    invariant(world);auto restored=fixture.restore(world.snapshot());
    for(int n=0;n<100;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Active visitor refusals changed restored continuation");}
    invariant(world);
}
void paid_lifetime_building_budget_includes_retired_and_neutral_sources() {
    Fixture fixture;auto world=fixture.world();
    const auto removed=buy(world,sim::CommandType::PlaceHousehold,{180,180});(void)remove(world,removed);
    int slot=0;
    const auto next=[&]{const auto n=slot++;return sim::Cell{20+5*(n%18),20+5*(n/18)};};
    for(int n=0;n<3;++n) {
        const auto at=next();const auto farm=buy(world,sim::CommandType::PlaceOriginalFoodFarm,at);
        sim::Command field{sim::CommandType::PlaceOriginalMilletField,{at.x+3,at.y}};field.building_id=farm;f::buy(world,field);
    }
    for(int n=0;n<2;++n) {
        const auto at=next();const auto farm=buy(world,sim::CommandType::PlaceOriginalHempFarm,at);
        sim::Command field{sim::CommandType::PlaceOriginalHempField,{at.x+2,at.y}};field.building_id=farm;f::buy(world,field);
    }
    for(const auto [source,plant]:{
        std::pair{sim::CommandType::PlaceOriginalTeaCuringShed,sim::CommandType::PlaceOriginalTeaBush},
        std::pair{sim::CommandType::PlaceOriginalLacquerRefinery,sim::CommandType::PlaceOriginalLacquerTree},
        std::pair{sim::CommandType::PlaceOriginalSilkwormShed,sim::CommandType::PlaceOriginalMulberryTree}})
        for(int n=0;n<2;++n) {
            const auto at=next();const auto parent=buy(world,source,at);
            sim::Command child{plant,{at.x+2,at.y}};child.building_id=parent;f::buy(world,child);
        }
    for(int n=0;n<20;++n)(void)buy(world,sim::CommandType::PlaceHousehold,next());
    for(int n=0;n<3;++n)(void)buy(world,sim::CommandType::PlaceOriginalInfluenceSource,next());
    for(const auto type:{sim::CommandType::PlaceOriginalLoggingShed,sim::CommandType::PlaceOriginalBronzeSmelter,
        sim::CommandType::PlaceOriginalBronzewareMaker,sim::CommandType::PlaceOriginalLacquerwareMaker,
        sim::CommandType::PlaceOriginalPaperMaker,sim::CommandType::PlaceOriginalWeaver,
        sim::CommandType::PlaceHealthPost,sim::CommandType::PlaceServicePost,sim::CommandType::PlaceOriginalAncestorShrine,
        sim::CommandType::PlaceOriginalDaoistShrine,sim::CommandType::PlaceOriginalBuddhistShrine,
        sim::CommandType::PlaceOriginalConfucianAcademy,sim::CommandType::PlaceOriginalMusicSchool,
        sim::CommandType::PlaceOriginalAcrobatSchool,sim::CommandType::PlaceOriginalDramaSchool,
        sim::CommandType::PlaceOriginalWaterWell})
        for(int n=0;n<2;++n)(void)buy(world,type,next());
    for(int n=0;n<4;++n)(void)buy(world,sim::CommandType::PlaceClaySource,next());
    check(world.buildings().size()==81&&history(world).retired.size()==1&&world.couriers().size()==47&&
        world.original_influence_state()->sources.size()==3,"Paid lifetime boundary did not reach exactly81live+1retired");
    invariant(world);check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Exact full lifetime budget failed restore");
    const auto empty=next();
    for(const auto type:{sim::CommandType::PlaceOriginalInfluenceSource,sim::CommandType::PlacePottery}) {
        const auto before=world.snapshot();const auto canonical=world.canonical_state();
        const auto checked=world.validate({type,empty});const auto result=world.execute({type,empty});
        check(!checked.accepted&&!result.accepted&&world.snapshot()==before&&world.canonical_state()==canonical&&
            world.object_at(empty)==sim::Object::Empty,"A paid neutral or ordinary purchase bypassed the82 lifetime-body budget");
    }
}
void paid_low_funds_cannot_rebuild_a_retired_empty_house() {
    Fixture fixture;auto cells=fixture.policy->cells();
    // Existing authored Bridge authority provides a bounded paid expenditure:
    //58 spans remain below64, each has exactly8 eligible water cells. No time,
    // stock, population, treasury or historical state is assigned by this test.
    for (int n=0;n<58;++n) for (int x=21;x<=28;++x) {
        auto& cell=cells[static_cast<std::size_t>((20+3*n)*228+x)];
        cell.road_allowed=cell.building_allowed=false;
        cell.road_blocker=cell.building_blocker=sim::BuildBlocker::UnsupportedBridgeWater;
        cell.bridge_water=true;
    }
    fixture.policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),
        std::vector<sim::FixedGatePassage>{});
    auto world=fixture.world();const sim::Cell site{180,180};
    const auto house=buy(world,sim::CommandType::PlaceHousehold,site);
    for (int n=0;n<58;++n) {
        const int y=20+3*n;road(world,20,y);road(world,29,y);
        f::buy(world,sim::place_bridge({20,y},{29,y}));
    }
    for (int x=160;x<167;++x) road(world,x,160);
    check(world.ticks()==0 && world.total_population()==0 && world.bridges().size()==58 &&
        world.snapshot().roads.size()==123 && world.treasury()==74 && world.construction_spent_total()==11926 &&
        world.maintenance_spent_total()==0 && world.taxes_collected_total()==0,
        "Low-Funds rebuild fixture did not pay its real80+58*200+123*2 construction");
    const auto retired=remove(world,house);
    check(retired.kind==sim::OriginalRetiredKind::EmptyHouse && world.treasury()==74 &&
        world.next_building_id()==2 && world.buildings().empty(),
        "Low-Funds removal granted a refund, reused an ID or retained the empty House");
    refused_command(world,{sim::CommandType::PlaceHousehold,site},sim::BuildBlocker::InsufficientFunds);
    for (int y=0;y<2;++y) for (int x=0;x<2;++x)
        check(!world.building_owner_at({site.x+x,site.y+y}) && world.object_at({site.x+x,site.y+y})==sim::Object::Empty,
            "Unaffordable rebuild acquired a paid footprint");
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),
        "Low-Funds post-removal complete Rule19 restore changed the actual paid ledger");invariant(world);
}
void protected_map_and_paid_infrastructure_removal_stay_closed() {
    Fixture fixture;auto cells=fixture.policy->cells();
    const auto at=[&](int x,int y)->sim::MapCellPermission& {
        return cells[static_cast<std::size_t>(y*228+x)];
    };
    // These are independently authored immutable policy facts prepared before
    // the World, not an original map-name or source-ID admission exception.
    for (int y=60;y<62;++y) for (int x=60;x<62;++x)
        at(x,y)={false,false,true,0,sim::BuildBlocker::OriginalStructure,sim::BuildBlocker::OriginalStructure};
    at(80,60)={true,false,false,0,sim::BuildBlocker::None,sim::BuildBlocker::ProtectedOriginalRoad,
        true,sim::OriginalRoadClassification::GroundRoad};
    sim::FixedGatePassage gate;gate.id={1};
    for (int y=60;y<63;++y) for (int x=90;x<95;++x) {
        gate.protected_footprint.push_back({x,y});
        at(x,y)={false,false,true,0,sim::BuildBlocker::GateSolidPart,sim::BuildBlocker::GateSolidPart,
            false,sim::OriginalRoadClassification::FixedGateFootprint};
    }
    for (int y=60;y<63;++y) {
        gate.corridor.push_back({92,y});at(92,y).road_allowed=true;at(92,y).road_blocker=sim::BuildBlocker::None;
    }
    gate.openings={sim::Cell{92,59},sim::Cell{92,63}};
    for (int x=21;x<=28;++x) {
        auto& cell=at(x,20);cell.road_allowed=cell.building_allowed=false;
        cell.road_blocker=cell.building_blocker=sim::BuildBlocker::UnsupportedBridgeWater;cell.bridge_water=true;
    }
    at(41,20).height=1;
    fixture.policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),
        std::vector<sim::FixedGatePassage>{gate});
    const auto immutable=fixture.policy->canonical_state();auto world=fixture.world();
    check(world.original_safe_demolition_enabled() && world.original_road({80,60}) &&
        world.fixed_passage({92,61}) && world.map_permissions()->protected_original({60,60}) &&
        !world.building_owner_at({60,60}),"Rule19 protected fixture lacks its exact immutable authority");
    refused_command(world,{sim::CommandType::PlaceHousehold,{60,60}},sim::BuildBlocker::OriginalStructure);
    refused_command(world,{sim::CommandType::PlaceRoad,{60,60}},sim::BuildBlocker::OriginalStructure);
    refused_command(world,{sim::CommandType::PlaceHousehold,{80,60}},sim::BuildBlocker::ProtectedOriginalRoad);
    refused_command(world,{sim::CommandType::RemoveRoad,{80,60}},sim::BuildBlocker::ProtectedOriginalRoad);
    refused_command(world,{sim::CommandType::RemoveRoad,{92,61}},sim::BuildBlocker::ProtectedGatePassage);
    refused_command(world,{sim::CommandType::PlaceHousehold,{92,60}},sim::BuildBlocker::GateSolidPart);
    for (const auto cell:{sim::Cell{60,60},sim::Cell{80,60},sim::Cell{92,61}}) {
        auto command=sim::demolish_building(static_cast<sim::BuildingId>(1));command.cell=cell;
        refused_command(world,command);
    }
    for (const auto cell:{sim::Cell{20,20},sim::Cell{29,20},sim::Cell{40,20},sim::Cell{41,20}})
        road(world,cell.x,cell.y);
    f::buy(world,sim::place_bridge({20,20},{29,20}));f::buy(world,sim::place_ramp({40,20},{41,20}));
    check(world.bridges().size()==1 && world.ramps().size()==1 && world.buildings().empty() &&
        world.transport_edge_allowed({20,20},{21,20}) && world.transport_edge_allowed({40,20},{41,20}),
        "Infrastructure refusals lack actual paid traversable Bridge and Ramp authority");
    refused_command(world,{sim::CommandType::RemoveRoad,{24,20}},sim::BuildBlocker::ProtectedBridge);
    refused_command(world,{sim::CommandType::RemoveBridge,{24,20}});
    refused_command(world,{sim::CommandType::RemoveRamp,{40,20}});
    refused_command(world,{sim::CommandType::PlaceHousehold,{19,19}},sim::BuildBlocker::ProtectedBridge);
    refused_command(world,{sim::CommandType::PlaceHousehold,{39,19}},sim::BuildBlocker::SandboxOccupied);
    for (const auto cell:{sim::Cell{24,20},sim::Cell{40,20}}) {
        auto command=sim::demolish_building(static_cast<sim::BuildingId>(1));command.cell=cell;
        refused_command(world,command);
    }
    check(world.bridges().size()==1 && world.ramps().size()==1 &&
        fixture.policy->canonical_state()==immutable && fixture.restore(world.snapshot()).snapshot()==world.snapshot(),
        "Rule19 infrastructure refusal changed source permissions or complete restorable transport authority");
    invariant(world);
}
}
int main() {try {
    exact_identity_empty_house_and_neutral_history();paid_clay_drain_retirement_upkeep_rebuild_and_gap_restore();
    paid_road_wait_keeps_cargo_and_allows_only_real_settlement();
    occupied_elite_dependencies_stock_progress_fire_and_history_refuse();bounded_history_and_missing_ids();
    paid_goods_food_and_house_visitors_refuse_atomically();paid_lifetime_building_budget_includes_retired_and_neutral_sources();
    paid_low_funds_cannot_rebuild_a_retired_empty_house();protected_map_and_paid_infrastructure_removal_stay_closed();
    std::cout<<"Rule19 paid empty-House/earned-Clay demolition, bounded history, rebuild, refusals and restore PASS\n";return 0;
} catch (const std::exception& error) {std::cerr<<"Rule19 safe demolition World FAIL: "<<error.what()<<'\n';return 1;}}
