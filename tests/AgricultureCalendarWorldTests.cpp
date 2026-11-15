// Independent authored map/model inputs. Every worker, harvest, delivery and
// household receipt is earned by paid commands and the existing World ticks.
#include "IrrigationFixture.h"
#include "persistence/RecoveryStore.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <nlohmann/json.hpp>

namespace {
namespace q=oe_irrigation_fixture;
namespace p=oe_plantations_fixture;
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
using f::check;

sim::Cell at(sim::Cell cell) {return {cell.x+70,cell.y+70};}

struct Fixture : q::Fixture {
    Fixture():q::Fixture(0,{87,83}) {
        auto text=q::authored_model();
        // Actual Food, Water and Ancestor receipts gate the luxury-consuming
        // row. These positive demand values are independent scenario inputs.
        for(const unsigned field:{3U,8U,21U})
            oe_production_fixture::set_house_field(text,2,field,1);
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        model=save::prepare_original_irrigation_rules_model(root.data);
    }
};

p::Scene materials(sim::World& world) {
    // Translate the existing independent paid plantation scene into the actual
    // source Diamond. Its distances and20-house/body budgets stay unchanged.
    p::Scene scene;
    const auto buy=[&](sim::CommandType type,sim::Cell cell) {return q::purchase(world,type,at(cell));};
    for(const int y:{24,27})for(int x=19;x<=79;++x)
        f::buy(world,{sim::CommandType::PlaceRoad,at({x,y})});
    for(const int x:{19,79})for(const int y:{25,26})
        f::buy(world,{sim::CommandType::PlaceRoad,at({x,y})});
    for(int x=20;x<80;x+=3)scene.homes.push_back(buy(sim::CommandType::PlaceHousehold,{x,22}));
    scene.tea_source=buy(sim::CommandType::PlaceOriginalTeaCuringShed,{22,25});
    scene.lacquer_source=buy(sim::CommandType::PlaceOriginalLacquerRefinery,{32,25});
    scene.mulberry_source=buy(sim::CommandType::PlaceOriginalSilkwormShed,{42,25});
    scene.tea_plant=p::plant(world,sim::CommandType::PlaceOriginalTeaBush,scene.tea_source,at({26,29}));
    scene.lacquer_plant=p::plant(world,sim::CommandType::PlaceOriginalLacquerTree,scene.lacquer_source,at({36,29}));
    scene.mulberry_plant=p::plant(world,sim::CommandType::PlaceOriginalMulberryTree,scene.mulberry_source,at({46,29}));
    scene.logging=buy(sim::CommandType::PlaceOriginalLoggingShed,{52,25});
    scene.warehouse=buy(sim::CommandType::PlaceWarehouse,{58,25});
    scene.market=buy(sim::CommandType::PlaceMarket,{64,25});
    scene.water=buy(sim::CommandType::PlaceOriginalWaterWell,{68,25});
    scene.ancestor=buy(sim::CommandType::PlaceOriginalAncestorShrine,{72,25});
    scene.lacquerware=buy(sim::CommandType::PlaceOriginalLacquerwareMaker,{54,28});
    scene.weaver=buy(sim::CommandType::PlaceOriginalWeaver,{64,28});
    for(std::size_t i=0;i<sim::Rules::fire_watch_limit;++i)
        scene.watches.push_back(buy(sim::CommandType::PlaceFireWatch,{30+30*static_cast<int>(i),28}));
    return scene;
}

struct Scene {
    p::Scene materials;
    sim::BuildingId wheat{},millet{},hemp{},hemp_field{},pump{};
    std::array<sim::BuildingId,5> fields{};
};

sim::BuildingId crop(sim::World& world,sim::BuildingId farm,sim::CommandType type,sim::Cell cell) {
    sim::Command command{type,cell};command.building_id=farm;
    f::buy(world,command);return *world.building_owner_at(cell);
}

Scene build(sim::World& world) {
    Scene scene;scene.materials=materials(world);
    for(int x=19;x<=41;++x) f::buy(world,{sim::CommandType::PlaceRoad,at({x,20})});
    for(int y=21;y<24;++y) f::buy(world,{sim::CommandType::PlaceRoad,at({19,y})});
    scene.wheat=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,at({20,17}));
    scene.millet=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,at({27,17}));
    scene.fields[0]=crop(world,scene.wheat,sim::CommandType::PlaceOriginalWheatField,at({23,17}));
    scene.fields[1]=crop(world,scene.millet,sim::CommandType::PlaceOriginalMilletField,at({30,17}));
    scene.fields[2]=crop(world,scene.wheat,sim::CommandType::PlaceOriginalRiceField,at({24,17}));
    scene.fields[3]=crop(world,scene.wheat,sim::CommandType::PlaceOriginalCabbageField,at({24,18}));
    scene.fields[4]=crop(world,scene.wheat,sim::CommandType::PlaceOriginalSoybeanField,at({24,19}));
    scene.hemp=q::purchase(world,sim::CommandType::PlaceOriginalHempFarm,at({35,17}));
    scene.hemp_field=crop(world,scene.hemp,sim::CommandType::PlaceOriginalHempField,at({37,17}));
    scene.pump=q::pump(world,at({17,13}));
    std::array<sim::Cell,9> ditches{{
        {17,15},{17,16},{18,16},{19,16},{20,16},
        {21,16},{22,16},{23,16},{24,16}
    }};
    for(auto& cell:ditches)cell=at(cell);
    const auto irrigation=world.execute_irrigation_batch(ditches);
    check(irrigation.accepted,"Paid mixed-city ditch chain was refused: "+irrigation.reason);
    check(!world.total_population() && world.construction_spent_total()>0 &&
          world.original_agriculture_state()->fields.size()==5,
          "Paid mixed construction granted residents or lost separate crop identities");
    return scene;
}

struct Totals {
    std::array<std::uint64_t,2> harvested{},consumed{};
    std::array<std::uint64_t,3> returned{};
};

Totals totals(const sim::World& world,const Scene& scene) {
    Totals result;
    for(std::size_t i=0;i<2;++i) result.harvested[i]=a::field(world,scene.fields[i]).harvested;
    for(const auto& row:world.original_agriculture_state()->stocks)
        if(sim::original_house_kind(world.building(row.building_id).kind))
            for(std::size_t i=0;i<2;++i) result.consumed[i]+=row.consumed[i];
    result.returned={p::plant(world,scene.materials.tea_plant).returned,
        p::plant(world,scene.materials.lacquer_plant).returned,
        p::plant(world,scene.materials.mulberry_plant).returned};
    return result;
}

void conserved_and_locked(const sim::World& world,const Scene& scene) {
    p::valid(world);
    check(world.original_production_state()->food==sim::OriginalFoodState{},
          "Calendar observation duplicated Food into the historical two-kind ledger");
    std::array<std::uint64_t,5> origin{},stock{},cargo{},consumed{};
    for(std::size_t i=0;i<scene.fields.size();++i) {
        const auto& field=a::field(world,scene.fields[i]);
        check(static_cast<std::size_t>(field.crop)==i && !field.quantity &&
              !field.productive_events && !field.last_harvest_cycle,
              "Calendar query changed a typed field or claimed Original growth authority");
        origin[i]=field.harvested;
        const auto view=world.agriculture_season_status(field.field_id);
        check(view && view->subject==field.field_id && view->producer==field.farm_id &&
              !view->yield_effect_active && !view->blocker.empty(),
              "Field observation lost its paid source, explicit blocker or yield lock");
        if(i>=2) check(!field.harvested && !view->authored_production_active &&
            !view->productive_progress && !view->productive_target && !view->harvest_ready &&
            !view->actual_grower_active,
            "Locked Rice/Cabbage/Soy borrowed Wheat progress or acquired seasonal production");
        else check(view->authored_production_active && view->productive_target==80 && !view->harvest_ready,
                   "Existing auto-harvest Food adapter acquired a false stored-ready state");
    }
    for(const auto& row:world.original_agriculture_state()->stocks) {
        const auto amount=std::accumulate(row.amounts.begin(),row.amounts.end(),0);
        check(amount>=0 && amount<=(world.building(row.building_id).kind==sim::Object::Market ? 16:8),
              "Mixed-city Food exceeded its existing finite owner capacity");
        for(std::size_t i=0;i<5;++i) {
            check(row.amounts[i]>=0,"Mixed-city Food became negative");
            stock[i]+=static_cast<std::uint64_t>(row.amounts[i]);consumed[i]+=row.consumed[i];
            if(i>=2) check(!row.amounts[i] && !row.received[i] && !row.departed[i] &&
                !row.returned[i] && !row.consumed[i],"A locked crop gained stock or a transport/consumption receipt");
        }
    }
    for(const auto& trip:world.original_agriculture_state()->trips)
        for(std::size_t i=0;i<5;++i) {
            const auto remaining=trip.loaded[i]-trip.delivered[i]-trip.returned[i];
            check(remaining>=0,"Calendar traversal duplicated an in-flight Food delivery");
            cargo[i]+=static_cast<std::uint64_t>(remaining);
        }
    for(std::size_t i=0;i<5;++i)
        check(origin[i]==stock[i]+cargo[i]+consumed[i],"Two-cycle mixed Food conservation failed");
    for(const auto id:{scene.materials.tea_plant,scene.materials.lacquer_plant,scene.materials.mulberry_plant}) {
        const auto view=world.agriculture_season_status(id);const auto& plant=p::plant(world,id);
        check(view && view->authored_production_active && view->productive_progress==plant.growth &&
            view->harvest_ready==(plant.growth==80) && view->returned==plant.returned && !view->yield_effect_active,
            "Plantation seasonal presentation diverged from actual readiness/return authority");
        if(plant.last_receipt) check(plant.last_receipt->dispatched_tick<plant.last_receipt->picked_tick &&
            plant.last_receipt->picked_tick<plant.last_receipt->returned_tick,
            "Plantation receipt lacks actual outward, pickup and return chronology");
    }
    const auto hemp=world.agriculture_season_status(scene.hemp_field);
    check(hemp && hemp->model==orig::ModelBuilding::HempField && hemp->producer==scene.hemp &&
          hemp->authored_production_active && hemp->productive_target==80 && !hemp->harvest_ready,
          "Paid Hemp field lost its unchanged authored auto-harvest status");
    check(!world.agriculture_season_status(scene.pump) &&
          !world.agriculture_season_status(scene.materials.market) &&
          !world.agriculture_season_status(static_cast<sim::BuildingId>(999999)),
          "Calendar observation invented a crop for a non-field or unknown identity");
}

void independent_reference_windows(const sim::World& world,const Scene& scene) {
    struct Window {sim::BuildingId id;orig::ModelBuilding model;unsigned growing,suppressed;};
    // Independent twelve-slot masks; this tests binding the same clock to nine
    // paid subjects, not merely calling the pure seasonal predicate twice.
    const std::array windows{
        Window{scene.fields[0],orig::ModelBuilding::WheatField,0x03c,0x040},
        Window{scene.fields[1],orig::ModelBuilding::MilletField,0x3c0,0x400},
        Window{scene.fields[2],orig::ModelBuilding::RiceField,0x1e0,0x200},
        Window{scene.fields[3],orig::ModelBuilding::CabbageField,0x780,0x800},
        Window{scene.fields[4],orig::ModelBuilding::SoybeanField,0x0f0,0x100},
        Window{scene.hemp_field,orig::ModelBuilding::HempField,0x0f8,0},
        Window{scene.materials.tea_plant,orig::ModelBuilding::TeaBush,0x36c,0x400},
        Window{scene.materials.lacquer_plant,orig::ModelBuilding::LacquerTree,0x03e,0x0c0},
        Window{scene.materials.mulberry_plant,orig::ModelBuilding::MulberryTree,0x0d8,0x100}
    };
    const auto calendar=world.agriculture_calendar();check(calendar.has_value(),"Paid Agriculture lost its derived calendar");
    const auto bit=1U<<calendar->position.month;
    for(const auto& window:windows) {
        const auto status=world.agriculture_season_status(window.id);
        check(status && status->model==window.model && status->calendar==*calendar &&
            status->reference_growing_period==((window.growing&bit)!=0) &&
            status->reference_growth_suppressed==((window.suppressed&bit)!=0),
            "Paid field/plant status lost the common calendar or its independent reference window");
    }
}

void literal_calendar(const sim::World& world) {
    struct Case {
        std::uint64_t tick;std::uint8_t phase,subslot,slot;std::uint64_t cycle,growth,irrigation;
    };
    constexpr std::array cases{
        Case{0,0,0,0,0,21,28},Case{20,20,0,0,0,21,28},Case{21,21,0,0,0,72,28},
        Case{27,27,0,0,0,72,28},Case{28,28,0,0,0,72,79},Case{50,50,0,0,0,72,79},
        Case{51,0,1,0,0,72,79},Case{815,50,15,0,0,837,844},Case{816,0,0,1,0,837,844},
        Case{9791,50,15,11,0,9813,9820},Case{9792,0,0,0,1,9813,9820},
        Case{19584,0,0,0,2,19605,19612}
    };
    const auto found=std::find_if(cases.begin(),cases.end(),[&](const auto& row){return row.tick==world.ticks();});
    if(found==cases.end()) return;
    const auto calendar=world.agriculture_calendar();
    check(calendar && calendar->completed_ticks==found->tick && calendar->mapping_version==1 &&
          calendar->position.phase==found->phase && calendar->position.step==found->subslot &&
          calendar->position.month==found->slot && calendar->position.year==found->cycle &&
          calendar->next_source_growth_tick==found->growth &&
          calendar->next_irrigation_update_tick==found->irrigation,
          "World calendar changed a literal phase/subslot/slot/cycle boundary or next opportunity");
}

sim::World disk_restart(const Fixture& fixture,const sim::World& world) {
    const auto before=world.snapshot();const auto calendar=world.agriculture_calendar();
    const auto path=fixture.root.path/"calendar42.json";
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto json=nlohmann::json::parse(bytes);
    const auto document=save::read_save(path);
    check(document.source_schema_version==42 && document.world.rule_version==26 && document.world==before &&
        !document.migrated_from_schema1 && !json.at("world").contains("agriculture_calendar") &&
        !json.at("world").contains("season_state"),
        "Derived calendar introduced saved authority, migrated or changed the exact Rule26/schema42 state");
    auto loaded=fixture.restore(document);
    check(loaded.snapshot()==before && loaded.agriculture_calendar()==calendar,
          "Calendar-boundary disk restore changed paid state or the next dispatch");
    save::write_save(path,fixture.document(loaded),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes && world.snapshot()==before,
          "Calendar query/rewrite changed canonical save bytes or the running World");
    save::RecoveryStore recovery(fixture.root.path/"calendar-recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
    check(entry.loadable() && entry.schema==42 && entry.rule_version==26 &&
          fixture.restore(save::read_save(entry.path)).snapshot()==before,
          "Calendar-boundary Recovery checkpoint changed current production, cargo or dispatch authority");
    return loaded;
}

void paid_two_cycles_with_exact_restarts() {
    Fixture fixture;auto world=fixture.world();const auto scene=build(world);
    const auto source_map=f::read_text(fixture.root.data/f::map_relative);
    const auto source_model=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    literal_calendar(world);
    independent_reference_windows(world,scene);
    // Frame time and speed only choose how many existing World ticks run.
    sim::TickDriver driver;driver.pause_and_reset();const auto paused=world.snapshot();
    driver.update(1.0,world);driver.set_speed(4);driver.update(0.25,world);
    check(world.snapshot()==paused && world.agriculture_calendar()->completed_ticks==0,
          "Paused application clock advanced calendar or production");
    auto direct=fixture.restore(paused);
    driver.step_once(world);direct.tick();driver.toggle_pause();
    driver.set_speed(2);driver.update(1.0/40,world);direct.tick();
    driver.set_speed(4);driver.update(1.0/80,world);direct.tick();
    check(world.ticks()==3 && world.snapshot()==direct.snapshot(),
          "Pause/step/speed changed paid production beyond its actual World tick count");

    std::optional<sim::World> restarted;std::uint64_t replay_end=0;
    std::array<std::uint64_t,2> prior_harvest{};
    std::array<bool,2> authored_outside_reference{};
    std::array<bool,3> grower_outward{},grower_picked_return{},grower_returned_idle{};
    const std::array source_ids{scene.materials.tea_source,scene.materials.lacquer_source,scene.materials.mulberry_source};
    const std::array plant_ids{scene.materials.tea_plant,scene.materials.lacquer_plant,scene.materials.mulberry_plant};
    bool physical_food=false,wet_and_dry=false,natural_fire=false,paused_sources=false;
    std::array<std::uint64_t,3> pause_state{};
    std::optional<Totals> first_cycle;
    const auto actions=[&](sim::World& city) {
        if(city.ticks()==816) {
            f::buy(city,sim::set_building_operation(scene.wheat,false));
            f::buy(city,sim::set_building_operation(scene.materials.tea_source,false));
            f::buy(city,sim::set_building_operation(scene.pump,false));
            check(!city.irrigation_field_status(at({24,17})).water_available,
                  "Paused Pump retained seasonal water availability");
        }
        if(city.ticks()==916) {
            f::buy(city,sim::set_building_operation(scene.wheat,true));
            f::buy(city,sim::set_building_operation(scene.materials.tea_source,true));
            f::buy(city,sim::set_building_operation(scene.pump,true));
        }
    };
    while(world.ticks()<19584) {
        if(world.ticks()==815 || world.ticks()==9791) {
            check(!restarted,"Bounded calendar replay windows overlapped");
            restarted=disk_restart(fixture,world);replay_end=world.ticks()+64;
        }
        actions(world);if(restarted) actions(*restarted);
        if(world.ticks()==816) pause_state={static_cast<std::uint64_t>(world.building(scene.wheat).progress),
            a::field(world,scene.fields[0]).harvested,p::plant(world,scene.materials.tea_plant).productive_ticks};
        if(world.ticks()==916) {
            check(pause_state==std::array<std::uint64_t,3>{static_cast<std::uint64_t>(world.building(scene.wheat).progress),
                a::field(world,scene.fields[0]).harvested,p::plant(world,scene.materials.tea_plant).productive_ticks},
                "A producer operating pause advanced existing authored field or plant work");
            paused_sources=true;
        }
        const auto before=world.agriculture_calendar()->position;
        world.tick();if(restarted) restarted->tick();
        literal_calendar(world);
        if(restarted && world.ticks()==replay_end) {
            check(restarted->snapshot()==world.snapshot() && restarted->agriculture_calendar()==world.agriculture_calendar(),
                  "Independent disk-restored continuation drifted across a slot/year boundary");
            restarted.reset();
        }
        for(std::size_t i=0;i<2;++i) {
            const auto harvested=a::field(world,scene.fields[i]).harvested;
            const bool reference=i==0 ? before.month>=2 && before.month<6:before.month>=6 && before.month<10;
            if(harvested>prior_harvest[i] && !reference) authored_outside_reference[i]=true;
            prior_harvest[i]=harvested;
        }
        for(const auto& actor:world.couriers()) if(actor.role==sim::CourierRole::OriginalFoodToHouse &&
            actor.phase!=sim::CourierPhase::IdleAtWorkshop && actor.edge_progress>0)
            physical_food=true;
        for(std::size_t i=0;i<source_ids.size();++i) {
            if(grower_outward[i] && grower_picked_return[i] && grower_returned_idle[i]) continue;
            const auto& source=p::source(world,source_ids[i]);
            if(!source.latest_trip) continue;
            const auto& trip=*source.latest_trip;const auto& actor=world.courier(source.courier_id);
            const auto view=world.agriculture_season_status(plant_ids[i]);
            check(view && trip.plant_id==plant_ids[i] &&
                view->actual_grower_active==!trip.returned_tick.has_value(),
                "Plantation grower status lost its actual current trip/pickup/return lifetime");
            if(actor.edge_progress>0) {
                check(actor.role==sim::CourierRole::OriginalPlantationGrower && actor.path.size()>=2 &&
                    view->actual_grower_active,"A physically moving plantation grower was reported inactive");
                if(trip.picked_tick) grower_picked_return[i]=true;
                else grower_outward[i]=true;
            }
            if(trip.returned_tick && actor.phase==sim::CourierPhase::IdleAtWorkshop) {
                check(!view->actual_grower_active,"A completed idle grower remained active after actual return");
                grower_returned_idle[i]=true;
            }
        }
        const auto pump=world.irrigation_pump_status(scene.pump);
        if(pump.supplying) {
            const auto wet=world.agriculture_season_status(scene.fields[2]);
            const auto dry=world.agriculture_season_status(scene.fields[1]);
            wet_and_dry|=wet && wet->water_available && dry && !dry->water_available &&
                !wet->authored_production_active && !wet->productive_progress;
        }
        if(pump.burning) {
            check(!pump.supplying && !world.irrigation_field_status(at({24,17})).water_available,
                  "Natural Pump fire retained a current supplied-water grant");
            natural_fire=true;
        }
        if(world.ticks()%816==0) {
            const auto before_queries=world.snapshot();conserved_and_locked(world,scene);
            independent_reference_windows(world,scene);
            check(world.snapshot()==before_queries,"Calendar/status queries mutated production or paid state");
        }
        if(world.ticks()==9792) first_cycle=totals(world,scene);
    }
    const auto final=totals(world,scene);check(first_cycle.has_value(),"First complete elapsed calendar cycle was not visited");
    for(std::size_t i=0;i<2;++i) check(first_cycle->harvested[i]>0 && first_cycle->consumed[i]>0 &&
        final.harvested[i]>first_cycle->harvested[i] && final.consumed[i]>first_cycle->consumed[i] &&
        authored_outside_reference[i],
        "Two full cycles lost real Wheat/Millet harvest/House consumption or silently applied Original seasonal gates");
    for(std::size_t i=0;i<3;++i) check(first_cycle->returned[i]>0 && final.returned[i]>=first_cycle->returned[i],
        "Mixed paid city lost an actual Tea/Lacquer/Mulberry physical-return origin");
    check(physical_food,"Mixed calendar city missed actual Food-to-House movement");
    check(wet_and_dry,"Mixed calendar city missed simultaneously wet Rice and dry Millet fields");
    check(natural_fire,"Mixed calendar city missed an actual natural Pump fire");
    check(paused_sources,"Mixed calendar city missed its preserved producer operating pause");
    for(std::size_t i=0;i<source_ids.size();++i)
        check(grower_outward[i] && grower_picked_return[i] && grower_returned_idle[i],
              "A material chain missed actual outward movement, picked return or completed-idle grower observations");
    (void)disk_restart(fixture,world);conserved_and_locked(world,scene);
    check(f::read_text(fixture.root.data/f::map_relative)==source_map &&
          f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==source_model,
          "Calendar traversal modified the independently authored source inputs");
}

void no_workers_and_historical_readonly_boundary() {
    Fixture fixture;auto world=fixture.world();
    const auto farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,at({20,17}));
    const auto field=crop(world,farm,sim::CommandType::PlaceOriginalWheatField,at({23,17}));
    const auto source=q::purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,at({32,25}));
    const auto plant=p::plant(world,sim::CommandType::PlaceOriginalTeaBush,source,at({36,29}));
    const auto pump=q::pump(world,at({17,13}));q::ditch(world,at({17,15}));
    for(int i=0;i<2448;++i) world.tick();
    const auto field_status=world.agriculture_season_status(field);
    const auto plant_status=world.agriculture_season_status(plant);
    check(!world.total_population() && field_status && !field_status->staffed && !field_status->productive_progress &&
          !field_status->harvested && plant_status && !plant_status->staffed && !plant_status->productive_progress &&
          !plant_status->harvested && !world.irrigation_pump_status(pump).supplying &&
          world.agriculture_calendar()->position.month==3,
          "Unstaffed seasonal observations granted growth, harvests, water or a frozen calendar");
    q::valid(world);
    auto historical=sim::World::create_original_plantations(fixture.policy,fixture.model,fixture.terrain);
    const auto before=historical.snapshot();const auto calendar=historical.agriculture_calendar();
    check(calendar && !calendar->next_irrigation_update_tick && historical.snapshot()==before,
          "Read-only Rule25 calendar granted Rule26 irrigation or changed a historical save");
    auto ordinary=sim::World::create_original_economy_development(fixture.policy);
    check(!ordinary.agriculture_calendar() && !ordinary.agriculture_season_status(field),
          "An ordinary historical World acquired Agriculture calendar status");
}
} // namespace

int main() {
    try {
        paid_two_cycles_with_exact_restarts();no_workers_and_historical_readonly_boundary();
        std::cout<<"Agriculture calendar World: paid mixed city, two full cycles, existing Food/material returns, locks, irrigation, interruptions and exact disk continuation PASS\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Agriculture calendar World FAIL: "<<error.what()<<'\n';return 1;
    }
}
