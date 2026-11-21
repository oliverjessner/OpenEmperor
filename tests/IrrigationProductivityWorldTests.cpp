#include "IrrigationProductivityFixture.h"

#include <iostream>

namespace {
namespace t=oe_irrigation_productivity_fixture;
namespace s=oe_seasonal_agriculture_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace p=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
using f::check;
using Phase=sim::PhysicalFoodGrowerPhase;
std::uint64_t receipts(const sim::World& world,const s::Scene& city,std::size_t kind) {
    if(kind<5)return q::receipt_field(world,city.fields[kind]).returned;
    if(kind==5)return s::hemp_origin(world,city.hemp_farm).harvested;
    return p::plant(world,kind==6?city.materials.tea_plant:kind==7?city.materials.lacquer_plant:city.materials.mulberry_plant).returned;
}

void paid_matched_nine_type_dry_and_wet_production() {
    t::Fixture fixture;auto wet=fixture.world(),dry=fixture.world();const auto city=t::build(wet),control=t::build(dry,false);
    const auto& supplied=city.water;const auto& paused=control.water;const auto fields=t::subjects(city);
    check(fields.size()==9 && wet.treasury()==dry.treasury() && supplied.cells==paused.cells,
        "Matched irrigation controls changed paid geometry, prices or crop identities");
    std::array<bool,9> observed{},matched_increment{};std::array<std::uint64_t,9> previous{};
    for(int tick=0;tick<14000;++tick) {
        std::array<std::uint64_t,9> before{},dry_before{},bonus{};std::array<bool,9> water{};
        const auto dry_fields=t::subjects(control);
        for(std::size_t k=0;k<9;++k){before[k]=t::base_work(wet,fields[k]);dry_before[k]=t::base_work(dry,dry_fields[k]);
            bonus[k]=t::work(wet,fields[k]).bonus_work;water[k]=wet.irrigation_field_status(wet.building(fields[k]).cell).water_available;}
        wet.tick();dry.tick();
        for(std::size_t k=0;k<9;++k) {
            const auto& row=t::work(wet,fields[k]);const auto base=t::base_work(wet,fields[k]);
            check(base>=before[k] && base-before[k]<=1 && row.wet_work_ticks-previous[k]<=base-before[k],
                "Irrigation fabricated base work or counted multiple water providers for one work tick");
            if(row.wet_work_ticks>previous[k])check(water[k],"Bonus used a future or visually inferred wet state");
            previous[k]=row.wet_work_ticks;observed[k]|=row.bonus_work>0 && receipts(wet,city,k)>0;
            matched_increment[k]|=base==before[k]+1 && t::base_work(dry,dry_fields[k])==dry_before[k]+1 && row.bonus_work==bonus[k]+1;
            check(!t::work(dry,dry_fields[k]).wet_work_ticks && !t::work(dry,dry_fields[k]).bonus_work,
                "Paused dry-control Pump granted crop productivity");
        }
        if(tick%251==0){t::valid(wet);t::valid(dry);}
    }
    for(std::size_t k=0;k<9;++k)check(observed[k] && matched_increment[k] && receipts(dry,control,k)>0,
        "A type lacks actual source returns and increased growth on a matched real base-work tick: kind="+std::to_string(k));
    std::size_t active=0;for(const auto cell:supplied.cells)active+=wet.irrigation_ditch_wet(cell);
    check(active>=21,"The productivity proof lacks21 currently supplied paid ditch cells");t::valid(wet);t::valid(dry);
}

void exact_quarter_carry_clipping_and_real_five_tick_harvest() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto water=t::network(world);const auto field=city.fields[0];
    t::until(world,[&]{return t::work(world,field).wet_work_ticks==3;},"Paid wet field never earned its first three actual wet work units");
    f::buy(world,sim::set_building_operation(water.pump,false));
    check(!world.irrigation_field_status(world.building(field).cell).water_available,"Pump pause retained a productivity grant");
    t::until(world,[&]{return t::food_progress(world,field)==79;},"Dry real field work never reached its legally earned79-unit cap boundary");
    check(t::work(world,field).wet_work_ticks==3 && !t::work(world,field).bonus_work,"Dry work consumed wet carry or earned a bonus");
    f::buy(world,sim::set_building_operation(field,false));
    t::until(world,[&]{return !q::active(world,city.farm);},"Paused near-ready field failed its actual empty worker return");
    f::buy(world,sim::set_building_operation(water.pump,true));
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Restarted Pump skipped or never reached real supply phase");
    check(t::food_progress(world,field)==79 && t::work(world,field).wet_work_ticks==3,"Date passage supplied crop work while its field was paused");
    f::buy(world,sim::set_building_operation(field,true));
    t::until(world,[&]{return s::field(world,field).ready_tick.has_value();},"One final real work tick never completed the retained crop");
    check(t::work(world,field).wet_work_ticks==4 && t::work(world,field).bonus_work==0 &&
        t::work(world,field).clipped_bonus==1 && t::food_progress(world,field)==80,
        "Fourth wet tick failed to record exactly one clipped bonus at the80-unit cap");
    const auto ready=world.ticks(),before_receipt=q::receipt_field(world,field).returned;
    t::ticks(world,4);check(!q::field(world,field).harvested && q::receipt_field(world,field).returned==before_receipt,
        "Readiness/bonus bypassed five genuine at-field harvest-work ticks");
    t::ticks(world,1);check(q::field(world,field).harvested==1 && q::worker(world,city.farm).harvested_tick==ready+5 &&
        world.physical_food_grower_cargo(q::farm(world,city.farm).courier_id)==1 && q::receipt_field(world,field).returned==before_receipt,
        "Actual fifth harvest tick skipped physical cargo or credited Farm Food before return");
    f::buy(world,sim::set_building_operation(city.farm,false));f::buy(world,sim::set_building_operation(water.pump,false));
    t::until(world,[&]{return q::receipt_field(world,field).returned==1;},"Source/Pump pause lost a genuine reserved loaded return");
    check(q::receipt_field(world,field).last_receipt->irrigation_bonus_at_harvest==0,"Clipped bonus became retained physical harvest work");t::valid(world);
}

void carry_survives_a_missed_season_and_cuts_wait_for_phase27() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto network=t::network(world);const auto field=city.fields[0];
    t::until(world,[&]{return t::work(world,field).wet_work_ticks==3;},"Seasonal carry control has no genuine three-wet-tick remainder");
    f::buy(world,sim::set_building_operation(field,false));t::until(world,[&]{return !q::active(world,city.farm);},"Carry control failed physical empty retirement");
    const auto retained=t::work(world,field);t::through(world,9793);
    check(t::work(world,field)==retained && !t::food_progress(world,field) && s::field(world,field).wasted_work==3,
        "Season/year reset destroyed carry or credited it without actual work");
    const sim::Cell cut{85,100};f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    check(!world.irrigation_field_status(world.building(field).cell).water_available,"Ditch cut failed to invalidate downstream productivity immediately");
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    check(!world.irrigation_field_status(world.building(field).cell).water_available,"Paid reconnect granted bonus water before the real source phase");
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Reconnected paid network never supplied a later phase");
    f::buy(world,sim::set_building_operation(field,true));
    t::until(world,[&]{return t::work(world,field).wet_work_ticks==4;},"Next season failed to use retained carry on actual wet work");
    check(t::work(world,field).bonus_work==1 && t::work(world,field).clipped_bonus==0 &&
        t::food_progress(world,field)==2,"Retained3/4 carry did not produce exactly one bounded bonus with the next real base tick");
    (void)network;t::valid(world);
}

void overlapping_pumps_do_not_stack_and_unstaffed_owners_do_not_work() {
    t::Fixture fixture;for(const auto cell:openemperor::original::pump_geometry({89,92},0).water)
        fixture.raw.at(std::size_t(cell.y)*228U+std::size_t(cell.x))=4U;
    fixture.rebind();auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto first=t::network(world);const auto second=oe_irrigation_fixture::pump(world,{89,92});
    t::until(world,[&]{return world.irrigation_pump_status(first.pump).supplying && world.irrigation_pump_status(second).supplying &&
        q::phase(world,city.farm,Phase::Working);},"Two paid overlapping Pumps never reached actual staffed supply and field work");
    const auto initial=t::work(world,city.fields[0]);const auto base=t::base_work(world,city.fields[0]);
    t::ticks(world,16);const auto row=t::work(world,city.fields[0]);
    check(t::base_work(world,city.fields[0])-base==16 && row.wet_work_ticks-initial.wet_work_ticks==16 &&
        row.bonus_work+row.clipped_bonus-initial.bonus_work-initial.clipped_bonus==4,
        "Overlapping suppliers stacked bonus or skipped eligible actual wet work");

    for(const bool pump_priority:{false,true}) {
        t::Fixture scarce(12);auto limited=scarce.world();
        for(int x=89;x<=99;++x)f::buy(limited,{sim::CommandType::PlaceRoad,{x,109}});
        q::purchase(limited,sim::CommandType::PlaceHousehold,{90,107});
        const auto farm=q::purchase(limited,sim::CommandType::PlaceOriginalFoodFarm,{90,110});const auto field=q::crop(limited,farm,0,{93,110});
        const auto water=t::network(limited);
        f::buy(limited,sim::set_building_workforce_priority(pump_priority?water.pump:farm,sim::WorkforcePriority::High));
        f::buy(limited,sim::set_building_workforce_priority(pump_priority?farm:water.pump,sim::WorkforcePriority::Low));
        t::through(limited,4000);
        check(!t::work(limited,field).wet_work_ticks && !t::work(limited,field).bonus_work,
            "A zero-staff Farm or zero-staff Pump granted an irrigation bonus");
        check(pump_priority ? limited.workers_assigned(farm)==0 && t::base_work(limited,field)==0 :
            limited.workers_assigned(water.pump)==0 && t::base_work(limited,field)>0,
            "Scarce-worker control did not preserve actual exclusive staffing");t::valid(limited);
    }
}

void natural_pump_fire_drains_water_and_paid_inspector_restores_supply() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops,false);
    const auto water=t::network(world);const auto field=city.fields[0];
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Fire control lacks an initially supplied paid network");
    t::until(world,[&]{return world.building_on_fire(water.pump);},"Unprotected paid Pump did not enter genuine natural fire",5000);
    check(!world.irrigation_field_status(world.building(field).cell).water_available,"A burning Pump retained water authority");
    const auto wet=t::work(world,field).wet_work_ticks;t::ticks(world,20);
    check(t::work(world,field).wet_work_ticks==wet,"A burning Pump granted new bonus work");
    const auto funds=world.treasury();const auto watch=q::purchase(world,sim::CommandType::PlaceFireWatch,{89,105});
    check(world.treasury()==funds-world.construction_cost(sim::CommandType::PlaceFireWatch),"Recovery Watch was not actually paid");
    const auto arrived=[&]{for(const auto& row:world.original_production_state()->fire_protection->arrivals) {
        if(row.trip.owner==watch && row.trip.target==water.pump && row.extinguished)return true;
    }return false;};
    t::until(world,arrived,"Paid Inspector never physically arrived and extinguished the real Pump fire",12000);
    check(!world.building_on_fire(water.pump) && !world.irrigation_field_status(world.building(field).cell).water_available,
        "Extinguish arrival bypassed the next actual irrigation phase");
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Extinguished staffed Pump never regained real phase27 supply");t::valid(world);
}

void finite_food_bin_never_earns_unreserved_base_or_wet_work() {
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto water=t::network(world);f::buy(world,sim::set_building_operation(city.market,false));
    t::until(world,[&]{return q::volume(q::h::stock(world,city.farm).amounts)==7 && q::active(world,city.farm);},
        "Wet finite-bin control never retained seven real portions and one actual worker reservation");
    check(sim::physical_food_grower_reservations(&*world.physical_food_growers_state(),city.farm)==1,
        "Irrigation discarded the active worker's physical return reservation");
    t::until(world,[&]{return q::volume(q::h::stock(world,city.farm).amounts)==8 && !q::active(world,city.farm);},
        "Actual wet Grower returns never filled the unchanged finite Food bin");
    const auto wet=t::work(world,city.fields[0]);const auto base=t::base_work(world,city.fields[0]);const auto spawned=q::farm(world,city.farm).spawned;
    t::ticks(world,150);
    check(t::work(world,city.fields[0])==wet && t::base_work(world,city.fields[0])==base && q::farm(world,city.farm).spawned==spawned &&
        q::volume(q::h::stock(world,city.farm).amounts)==8,"Full Food bin earned unreserved base work, wet carry, bonus or extra cargo");
    f::buy(world,sim::set_building_operation(city.market,true));
    t::until(world,[&]{return q::volume(q::h::stock(world,city.farm).amounts)==4 &&
        std::any_of(world.couriers().begin(),world.couriers().end(),[&](const auto& c){return c.owner==city.farm &&
            c.role==sim::CourierRole::OriginalFoodToMarket && c.edge_progress && world.original_food_cargo(c.id)==4;});},
        "Real four-portion cart never exposed its retained return headroom");
    check(!q::active(world,city.farm) && t::work(world,city.fields[0])==wet,
        "Apparent free bin space bypassed an actual Food cart's reserved return");(void)water;t::valid(world);
}

void edge_range_and_loaded_land_cut_preserve_earned_bonus_and_cargo() {
    t::Fixture fixture;
    for(int y=102;y<=110;++y)for(int x=93;x<=97;++x)fixture.raw[std::size_t(y)*228U+std::size_t(x)]=y==107?0x80U:4U;
    fixture.rebind();auto world=fixture.world();
    for(int x=74;x<=92;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int x=75;x<=90;x+=3)q::purchase(world,sim::CommandType::PlaceHousehold,{x,102});
    q::purchase(world,sim::CommandType::PlaceFireWatch,{75,105});q::purchase(world,sim::CommandType::PlaceFireWatch,{78,105});
    q::purchase(world,sim::CommandType::PlaceMarket,{81,105});q::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{84,105});
    q::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{87,105});
    const auto farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,105});const auto field=q::crop(world,farm,0,{95,107});
    // This narrow corridor's House84,102 legitimately occupies x85, so its
    // separate paid network uses the actual empty x83 spine instead.
    const auto pump=oe_irrigation_fixture::pump(world,t::pump_origin);std::vector<sim::Cell> branch;
    for(int x=83;x<=95;++x)branch.push_back({x,94});
    for(int y=95;y<=112;++y)branch.push_back({83,y});
    for(int x=84;x<=95;++x)branch.push_back({x,112});
    const auto installed=world.execute_irrigation_batch(branch);
    check(installed.accepted,"Paid five-cell boundary network was refused: "+installed.reason);
    t::until(world,[&]{return world.irrigation_field_status(world.building(field).cell).water_available;},"Exact five-cell field never received actual network water");
    check(!world.irrigation_field_status({95,106}).water_available,"Water availability escaped the exact five-cell query boundary");
    const auto channel=q::farm(world,farm).courier_id;
    t::until(world,[&]{const auto& c=world.courier(channel);return world.physical_food_grower_cargo(channel)==1 &&
        c.edge_progress>0 && c.path.size()>c.path_vertex+2;},"Irrigated corridor never earned a physically returning loaded Grower");
    const auto trip=q::worker(world,farm);const auto actor=world.courier(channel);const auto cut=actor.path.at(actor.path_vertex+2);
    check(t::work(world,field).bonus_work>0,"Loaded cut control never earned any actual irrigation bonus");
    t::denied(world,{sim::CommandType::PlaceIrrigationDitch,actor.path.at(actor.path_vertex+1)});
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    check(world.courier(channel).edge_progress==actor.edge_progress,"Future cut rewound a genuinely begun loaded edge");
    f::buy(world,sim::set_building_operation(farm,false));f::buy(world,sim::set_building_operation(pump,false));
    const auto retained=t::work(world,field);t::ticks(world,100);
    check(q::worker(world,farm).grower_id==trip.grower_id && world.physical_food_grower_cargo(channel)==1 &&
        !q::receipt_field(world,field).returned && t::work(world,field)==retained,
        "Paused dry source or cut discarded loaded cargo, changed earned bonus or credited a false source receipt");
    auto replay=fixture.restore(world.snapshot());for(int tick=0;tick<200;++tick){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Blocked irrigated loaded return diverged after exact source-bound restore");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    t::until(world,[&]{return q::receipt_field(world,field).returned==1;},"Physical land repair failed to settle preserved bonus-assisted cargo");
    check(q::receipt_field(world,field).last_receipt->irrigation_bonus_at_harvest==retained.bonus_work &&
        q::h::stock(world,farm).amounts[0]==1,"Recovered cargo lost bonus provenance or duplicated its genuine source receipt");t::valid(world);
}
}
template<class F>void run(const char* name,F action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}}
int main(){try{run("matched nine",paid_matched_nine_type_dry_and_wet_production);
    run("quarter carry/cap",exact_quarter_carry_clipping_and_real_five_tick_harvest);
    run("year/cut",carry_survives_a_missed_season_and_cuts_wait_for_phase27);
    run("overlap/staff",overlapping_pumps_do_not_stack_and_unstaffed_owners_do_not_work);
    run("natural fire",natural_pump_fire_drains_water_and_paid_inspector_restores_supply);
    run("finite Food",finite_food_bin_never_earns_unreserved_base_or_wet_work);
    run("edge/loaded cut",edge_range_and_loaded_land_cut_preserve_earned_bonus_and_cargo);
    std::cout<<"Irrigation productivity paid nine-type wet/dry, carry/cap, staffing, real fire recovery and loaded cut passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
