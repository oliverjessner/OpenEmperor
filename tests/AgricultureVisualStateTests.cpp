#include "AgricultureReconfigurationFixture.h"
#include "app/AgricultureVisualState.h"
#include "core/PerformanceDiagnostics.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace t=oe_agriculture_reconfiguration_fixture;
namespace sim=t::sim;
namespace visual=openemperor::agriculture_visual;
namespace perf=openemperor::performance;
using t::check;

const visual::State& state(const visual::Cache& cache,sim::BuildingId id) {
    const auto* row=cache.find(id);check(row!=nullptr,"A live paid agriculture body lost its cached visual state");return *row;
}
std::vector<std::pair<sim::BuildingId,int>> staffing(const sim::World& world) {
    std::vector<std::pair<sim::BuildingId,int>> result;
    for(const auto& body:world.buildings())if(body.placed)result.emplace_back(body.id,world.workers_assigned(body.id));
    return result;
}
void no_forbidden_work() {
    for(std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,"Agriculture visual preparation/lookup performed forbidden resource, World or route work");
}
void pure(const sim::World& world) {
    const auto before=world.snapshot();const auto workers=staffing(world);const auto expected=visual::prepare(world);
    perf::set_enabled(true);perf::reset();const auto prepared=visual::prepare(world,workers);
    check(prepared==expected,"Current cached staffing changed actual World visual observations");
    std::uint64_t observed=0;
    for(unsigned sample=0;sample<3000;++sample)for(std::size_t i=0;i<prepared.count;++i) {
        const auto* row=prepared.find(prepared.rows[i].id);check(row==&prepared.rows[i],"Bounded typed visual lookup selected another paid identity");
        observed+=static_cast<unsigned>(row->id)+row->progress;
        check(visual::phase_name(row->phase)[0]!='\0',"A supported visual phase has no label");
    }
    check(!prepared.count||observed>0,"Cached observation loop was empty");no_forbidden_work();perf::set_enabled(false);
    check(world.snapshot()==before,"Read-only agriculture visuals mutated actual paid authority");
}
void exact_restore(const t::Fixture& fixture,const sim::World& world) {
    const auto expected=visual::prepare(world);const auto restored=fixture.restore(world.snapshot());
    check(restored.snapshot()==world.snapshot()&&visual::prepare(restored)==expected,
        "Visual state depends on a previous frame, wall clock or unsaved stage instead of the current restored World");
}

void exact_generation_identity_retirement_and_pure_cache() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,5> crops{0,1,2,3,4};const auto city=t::q::build(world,crops);
    const auto initial=visual::prepare(world);
    check(initial.tick==0&&initial.sequence==world.command_sequence()&&initial.count<=82,"Visual cache invented a clock or expanded the body budget");
    for(const auto id:city.fields) {
        const auto& row=state(initial,id);check(row.source==city.farm&&row.id==id&&row.kind==world.building(id).kind&&
            row.cell==world.building(id).cell&&row.phase==visual::Phase::Initial&&row.progress==0&&row.target==80&&
            !row.working&&!row.worker&&!row.cargo&&!row.wet,
            "Initial field state inferred growth/work/cargo from placement time or another Field");
    }
    check(initial.find(sim::BuildingId{})==nullptr&&initial.find(static_cast<sim::BuildingId>(UINT32_MAX))==nullptr,
        "Unknown ID borrowed another body's visual state");pure(world);
    const auto old=city.fields.front();const auto cell=world.building(old).cell;
    t::f::buy(world,sim::demolish_building(old));const auto replacement=t::crop(world,city.farm,4,cell);const auto rebuilt=visual::prepare(world);
    check(!rebuilt.find(old)&&replacement>old&&state(rebuilt,replacement).source==city.farm&&state(rebuilt,replacement).phase==visual::Phase::Initial,
        "Retired visual survived or a same-cell replacement inherited a closed crop phase");exact_restore(fixture,world);pure(world);t::valid(world);
    auto historical=sim::World::create_food_warehouses(fixture.policy,fixture.model,fixture.terrain);
    t::q::build(historical,std::span<const std::size_t>(crops.data(),1));const auto before=historical.snapshot();
    check(visual::prepare(historical).count==0&&historical.snapshot()==before,"Presentation-only work changed Rule35 historical visuals/authority");
}

void nine_real_crop_phases_and_exact_work_binding() {
    t::Fixture fixture;auto world=fixture.world();const auto city=t::i::build(world);
    const std::array<sim::BuildingId,9> ids{city.fields[0],city.fields[1],city.fields[2],city.fields[3],city.fields[4],
        city.hemp_field,city.materials.tea_plant,city.materials.lacquer_plant,city.materials.mulberry_plant};
    std::array<bool,9> growing{},ready{},picked{},regrowing{};bool harvesting=false,food_restore=false,material_restore=false;
    const auto initial=visual::prepare(world);for(const auto id:ids)check(state(initial,id).progress==0,"Paid cohort received initial growth");
    for(unsigned tick=0;tick<14000;++tick) {
        world.tick();const auto cache=visual::prepare(world);
        for(std::size_t i=0;i<ids.size();++i) {
            const auto& row=state(cache,ids[i]);
            const auto source=i<5?city.farms[i]:i==5?city.hemp_farm:i==6?city.materials.tea_source:
                i==7?city.materials.lacquer_source:city.materials.mulberry_source;
            check(row.source==source&&row.target==80&&row.progress<=80,"Visual growth lost its actual paid source or bounded work units");
            growing[i]=growing[i]||(row.progress>0&&row.progress<80);
            ready[i]=ready[i]||row.phase==visual::Phase::Ready||row.phase==visual::Phase::Harvesting;
            picked[i]=picked[i]||row.cargo>0;
            regrowing[i]=regrowing[i]||row.phase==visual::Phase::Regrowing;
            check(row.wet==world.irrigation_field_status(world.building(ids[i]).cell).water_available,
                "Water overlay came from proximity or a stale visual epoch");
            if(i<5) {
                const auto& season=t::i::s::field(world,ids[i]);
                check(row.progress==static_cast<unsigned>(world.agriculture_field_progress(ids[i])),"Another field's progress selected the current crop stage");
                if(row.phase==visual::Phase::Harvesting) {
                    check(row.progress==80&&row.harvest_progress==season.harvest_progress&&row.harvest_progress>0&&row.harvest_target==5,
                        "Ready growth was substituted for actual physical harvest work");harvesting=true;
                }
                if(row.cargo) {
                    const auto& actor=t::worker(world,source);check(actor.latest_trip&&actor.latest_trip->field_id==ids[i]&&
                        row.worker==actor.courier_id&&row.worker_lifetime==actor.latest_trip->grower_id.value&&
                        row.cargo==sim::physical_food_trip_cargo(*actor.latest_trip)&&row.returning&&row.phase==visual::Phase::Picked,
                        "Field overlay displayed a phantom/misbound physical Grower or Farm receipt");
                    if(!food_restore){exact_restore(fixture,world);food_restore=true;}
                }
            } else {
                check(!row.working,"Stationary material growth invented a physical tending/work animation");
                if(i>5)check(row.progress==t::p::plant(world,ids[i]).growth,"Material stage borrowed time or another plant's progress");
                else check(row.progress==static_cast<unsigned>(world.building(city.hemp_farm).progress),"Hemp visual borrowed Food work");
                if(row.cargo&&!material_restore){exact_restore(fixture,world);material_restore=true;}
            }
        }
        if(std::all_of(growing.begin(),growing.end(),[](bool value){return value;})&&
           std::all_of(ready.begin(),ready.end(),[](bool value){return value;})&&
           std::all_of(picked.begin(),picked.end(),[](bool value){return value;})&&
           std::all_of(regrowing.begin(),regrowing.end(),[](bool value){return value;}))break;
    }
    for(std::size_t i=0;i<ids.size();++i)check(growing[i]&&ready[i]&&picked[i]&&regrowing[i],
        "A supported crop never earned growing/ready/actual picked/regrowing display witnesses");
    check(harvesting&&food_restore&&material_restore,"Actual harvest work or loaded restore evidence was absent");
    pure(world);t::valid(world);
}

void drain_pause_and_loaded_return_never_reset_crop_state() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=t::q::build(world,crops);const auto field=city.fields.front();
    t::until(world,[&]{return world.agriculture_field_progress(field)>0&&world.agriculture_field_progress(field)<80;},"Visual control lacks real partial Food work");
    const auto earned=world.agriculture_field_progress(field);t::f::buy(world,sim::begin_agriculture_drain(field));t::operate(world,city.farm,false);
    auto cache=visual::prepare(world);check(state(cache,field).draining&&!state(cache,field).operating&&state(cache,field).blocked&&
        !state(cache,field).working&&state(cache,field).progress==static_cast<unsigned>(earned),"Paused/draining visual discarded earned work or continued an invented worker loop");
    t::ticks(world,12);check(state(visual::prepare(world),field).progress==static_cast<unsigned>(earned),"A paused display advanced the crop clock");
    t::operate(world,city.farm,true);
    t::until(world,[&]{return state(visual::prepare(world),field).cargo==1;},"Drain never physically harvested its retained crop");
    cache=visual::prepare(world);check(state(cache,field).draining&&state(cache,field).phase==visual::Phase::Picked&&state(cache,field).returning,
        "Loaded drain visual lost its actual return phase");exact_restore(fixture,world);
    t::operate(world,city.farm,false);const auto cargo=state(visual::prepare(world),field).cargo;
    check(cargo==1,"Pausing a loaded source erased actual crop cargo");
    t::until(world,[&]{return world.demolition_status(field).allowed;},"Actual return did not settle a drained visual crop");
    cache=visual::prepare(world);check(state(cache,field).cargo==0&&state(cache,field).progress==0&&state(cache,field).draining&&
        state(cache,field).blocked&&!state(cache,field).working&&t::receipt(world,field).returned>0,
        "Settled drain visual invented regrowth, unreturned stock or another worker");
    t::f::buy(world,sim::demolish_building(field));check(!visual::prepare(world).find(field),"Closed loaded crop remained visibly live");pure(world);t::valid(world);
}

void pump_uses_actual_supply_staff_fire_and_recovery() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=t::q::build(world,crops,false);
    const auto water=t::i::network(world);const auto field=city.fields.front();
    const auto same=[&] {
        const auto actual=world.irrigation_pump_status(water.pump);const auto cache=visual::prepare(world);const auto& row=state(cache,water.pump);
        check(row.staffed==actual.staffed&&row.workers==actual.assigned_workers&&row.operating==actual.operating&&
            row.burning==actual.burning&&row.wet==actual.supplying&&row.working==(actual.supplying&&!row.blocked)&&row.reason==actual.reason,
            "Pump visual reimplemented full-staff, intake, refill or supply policy");
        check(state(cache,field).wet==world.irrigation_field_status(world.building(field).cell).water_available,"Field wet overlay survived actual Pump loss");
    };
    same();check(!state(visual::prepare(world),water.pump).wet,"Fresh unstaffed Pump displayed free supply");
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Actual paid Pump never supplied crop");same();
    t::operate(world,water.pump,false);same();check(!state(visual::prepare(world),water.pump).wet,"Paused Pump retained visual water");
    t::operate(world,water.pump,true);same();check(!state(visual::prepare(world),water.pump).wet,"Pump restart visually skipped actual refill phase");
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Pump restart never physically refilled");same();
    t::until(world,[&]{return world.building_on_fire(water.pump);},"Unprotected Pump did not earn actual fire witness",5000);same();
    check(state(visual::prepare(world),water.pump).blocked&&!state(visual::prepare(world),water.pump).working,"Burning Pump displayed active supply/work");exact_restore(fixture,world);
    const auto watch=t::purchase(world,sim::CommandType::PlaceFireWatch,{89,105});
    t::until(world,[&]{
        for(const auto& arrival:world.original_production_state()->fire_protection->arrivals) {
            if(arrival.trip.owner==watch&&arrival.trip.target==water.pump&&arrival.extinguished)return true;
        }
        return false;
    },
        "Paid Inspector never physically restored real Pump fire",12000);
    same();check(!state(visual::prepare(world),water.pump).burning&&!state(visual::prepare(world),water.pump).wet,
        "Extinguish display granted water before actual phase27");
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Extinguished Pump never earned actual refill");same();pure(world);t::valid(world);
}

void committed_processing_and_sole_food_storage_remain_observations() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=t::w::build(world,true,crops);
    t::f::buy(world,sim::set_mill_recipe(city.mill,1));bool processing=false,warehouse_stock=false,warehouse_incoming=false,completed=false;
    for(unsigned tick=0;tick<14000;++tick) {
        world.tick();const auto cache=visual::prepare(world);const auto mill=t::w::mill(world,city.mill);const auto& row=state(cache,city.mill);
        const auto& storage=t::stock(world,city.warehouse);const auto& wh=state(cache,city.warehouse);int raw=0,meals=0;
        for(std::size_t i=0;i<storage.amounts.size();++i)(i<7?raw:meals)+=storage.amounts[i];
        check(wh.input==raw&&wh.output==meals&&wh.incoming==world.food_warehouse_status(city.warehouse).incoming&&
            wh.return_reserved==world.food_warehouse_status(city.warehouse).return_reserved,
            "Food Warehouse visuals created a parallel stock or inferred an arrival");
        warehouse_stock=warehouse_stock||raw+meals>0;warehouse_incoming=warehouse_incoming||wh.incoming>0;
        if(mill.active) {
            check(row.phase==visual::Phase::Processing&&row.progress==mill.active->work&&row.target==20U*mill.active->grade,
                "Mill visual fabricated recipe timing or lost committed debited work");
            if(!processing) {
                processing=true;t::operate(world,city.mill,false);const auto paused=visual::prepare(world);
                check(state(paused,city.mill).blocked&&!state(paused,city.mill).working&&state(paused,city.mill).progress==row.progress,
                    "Paused Mill display advanced work or discarded committed inputs");exact_restore(fixture,world);t::operate(world,city.mill,true);
            }
        }
        completed=completed||mill.completed>0;
        if(processing&&warehouse_stock&&warehouse_incoming&&completed)break;
    }
    check(processing&&warehouse_stock&&warehouse_incoming&&completed,"Processing/warehouse visual test lacked real work, inbound cargo, stock or completed meals");
    t::operate(world,city.warehouse,false);const auto cache=visual::prepare(world);
    check(state(cache,city.warehouse).blocked&&!state(cache,city.warehouse).working,"Paused Warehouse displayed active operation");pure(world);t::valid(world);
}
} // namespace

int main() {
    try {
        exact_generation_identity_retirement_and_pure_cache();
        nine_real_crop_phases_and_exact_work_binding();
        drain_pause_and_loaded_return_never_reset_crop_state();
        pump_uses_actual_supply_staff_fire_and_recovery();
        committed_processing_and_sole_food_storage_remain_observations();
        std::cout<<"Agriculture visual state: exact36 identities, nine earned crop phases, real cargo/return, Pump fire/refill, committed processing, sole stock and pure cached lookups PASS\n";
        return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);std::cerr<<error.what()<<'\n';return 1;
    }
}
