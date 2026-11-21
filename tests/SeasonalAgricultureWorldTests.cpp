#include "SeasonalAgricultureFixture.h"

#include <iostream>

namespace {
namespace s=oe_seasonal_agriculture_fixture;
namespace q=oe_physical_food_growers_fixture;
namespace p=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using f::check;
using Phase=sim::PhysicalFoodGrowerPhase;
constexpr std::uint64_t month=816,year=9792;
constexpr std::array<orig::ModelBuilding,9> models{orig::ModelBuilding::WheatField,orig::ModelBuilding::MilletField,
    orig::ModelBuilding::RiceField,orig::ModelBuilding::CabbageField,orig::ModelBuilding::SoybeanField,
    orig::ModelBuilding::HempField,orig::ModelBuilding::TeaBush,orig::ModelBuilding::LacquerTree,orig::ModelBuilding::MulberryTree};
// Independent twelve-slot oracles; no production selector is used to construct them.
constexpr std::array<unsigned,9> masks{0x03cU,0x3c0U,0x1e0U,0x780U,0x0f0U,0x0f8U,0x36cU,0x03eU,0x0d8U};
void calendar_masks_first_last_and_rollover() {
    for(std::size_t i=0;i<models.size();++i)for(std::uint64_t y=0;y<3;++y)for(unsigned slot=0;slot<12;++slot) {
        const bool expected=(masks[i]&(1U<<slot))!=0;
        const auto first=y*year+slot*month;
        check(sim::seasonal_growing(models[i],first)==expected && sim::seasonal_growing(models[i],first+month-1)==expected,
            "Seasonal first/last phase did not match the independent nine-type mask");
        if(expected) {
            unsigned end=slot+1;while(end<12 && (masks[i]&(1U<<end)))++end;
            check(sim::seasonal_ready_expiry(models[i],first)==y*year+(end+1)*month,
                "Ready grace did not expire after exactly its own closing slot");
            check(sim::seasonal_harvest_allowed(models[i],y*year+(end+1)*month-1,first) &&
                !sim::seasonal_harvest_allowed(models[i],y*year+(end+1)*month,first),
                "Ready crop's last harvest opportunity or expiry phase changed");
        }
    }
    for(std::size_t i=0;i<models.size();++i) {
        unsigned first=0;while(!(masks[i]&(1U<<first)))++first;
        check(sim::seasonal_next_opening(models[i],year-1)==year+first*month,
            "Year rollover created an extra season or skipped the next opening");
        for(const auto placed:{0U,1640U,4907U,9780U}) {
            std::uint64_t expected=0;const auto through=2*year+17;
            for(std::uint64_t tick=placed+1;tick<=through;++tick)
                expected+=static_cast<unsigned>((masks[i]&(1U<<((tick/month)%12)))!=0);
            check(sim::seasonal_work_opportunities(models[i],placed,through)==expected,
                "Paid seasonal work bound counted an off-season or pre-purchase opportunity");
        }
    }
}

void paid_mixed_city_five_complete_years() {
    s::Fixture fixture;auto world=fixture.world();const auto city=s::build(world);
    std::array<std::uint64_t,9> year_receipts{};
    sim::HuntingFoodCounters previous_consumed{};
    std::array<bool,9> worked{},waited{},returned{};
    std::size_t complete=0;
    for(std::uint64_t t=0;t<60000;++t) {
        std::array<std::uint64_t,9> before{};
        for(std::size_t i=0;i<5;++i)before[i]=s::work(world,city.fields[i]).productive_ticks;
        before[5]=s::hemp(world,city.hemp_farm).productive_ticks;
        before[6]=p::plant(world,city.materials.tea_plant).productive_ticks;
        before[7]=p::plant(world,city.materials.lacquer_plant).productive_ticks;
        before[8]=p::plant(world,city.materials.mulberry_plant).productive_ticks;
        world.tick();
        std::array<std::uint64_t,9> now{},receipts{};
        for(std::size_t i=0;i<5;++i) {now[i]=s::work(world,city.fields[i]).productive_ticks;receipts[i]=s::receipt_field(world,city.fields[i]).returned;}
        now[5]=s::hemp(world,city.hemp_farm).productive_ticks;receipts[5]=s::hemp_origin(world,city.hemp_farm).harvested;
        now[6]=p::plant(world,city.materials.tea_plant).productive_ticks;receipts[6]=p::plant(world,city.materials.tea_plant).returned;
        now[7]=p::plant(world,city.materials.lacquer_plant).productive_ticks;receipts[7]=p::plant(world,city.materials.lacquer_plant).returned;
        now[8]=p::plant(world,city.materials.mulberry_plant).productive_ticks;receipts[8]=p::plant(world,city.materials.mulberry_plant).returned;
        for(std::size_t i=0;i<9;++i) {
            check(now[i]>=before[i] && now[i]-before[i]<=1,"Seasonal plant earned more than one genuine work unit per tick");
            if(now[i]>before[i]) {check((masks[i]&(1U<<((world.ticks()/month)%12)))!=0,"Off-season date granted real plant work");worked[i]=true;}
            else if(!(masks[i]&(1U<<((world.ticks()/month)%12))))waited[i]=true;
            returned[i]|=receipts[i]>0;
        }
        if(world.ticks()%year==0) {
            // The first year staffs and builds reserves. Five later complete
            // years must each earn new physical returns and actual Food sinks.
            if(world.ticks()>year) {
                for(std::size_t i=0;i<9;++i)check(receipts[i]>year_receipts[i],
                    "Paid plant chain stopped returning output: year="+std::to_string(world.ticks()/year)+" kind="+std::to_string(i));
                const auto eaten=s::consumed(world);
                for(std::size_t i=0;i<5;++i)check(eaten[i]>previous_consumed[i],"Seasonal Food stopped reaching actual House consumption during a complete year");
                ++complete;
            }
            year_receipts=receipts;previous_consumed=s::consumed(world);
        }
        if(t%251==0)s::valid(world);
    }
    check(complete==5 && std::all_of(worked.begin(),worked.end(),[](bool b){return b;}) &&
        std::all_of(waited.begin(),waited.end(),[](bool b){return b;}) && std::all_of(returned.begin(),returned.end(),[](bool b){return b;}),
        "Paid seasonal city lacks five full years or a genuine nine-type work/wait/return witness");
    check(world.original_goods_balance(orig::Goods::Tea).totals.produced>0 &&
        world.original_goods_balance(orig::Goods::Silk).totals.produced>0 &&
        world.original_goods_balance(orig::Goods::Lacquerware).totals.produced>0,
        "Seasonal material returns did not enter the existing real manufacturing/Goods authority");
    s::valid(world);
}

void off_season_planting_partial_decay_and_ready_expiration() {
    s::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    const auto field=city.fields[0];
    s::through(world,2*month-1);
    check(!s::work(world,field).productive_ticks && !s::farm(world,city.farm).spawned && world.building_staffed(city.farm),
        "Off-season staffed paid Farm allocated work or a free Grower");
    s::until(world,[&]{return s::progress(world,field)>11 && s::progress(world,field)<60;},"In-season physical Grower never earned partial crop work");
    f::buy(world,sim::set_building_operation(field,false));
    s::until(world,[&]{return !s::active(world,city.farm);},"Paused partial crop failed to physically return empty");
    const auto earned=s::work(world,field).productive_ticks,partial=s::progress(world,field);
    s::through(world,6*month-1);check(s::progress(world,field)==partial && !s::field(world,field).wasted_work,
        "Partial seasonal work expired before the first outside dispatch");
    world.tick();check(!s::progress(world,field) && s::field(world,field).wasted_work==partial &&
        s::work(world,field).productive_ticks==earned && !q::field(world,field).harvested,
        "First outside phase failed to expire exactly the unharvested partial work");
    f::buy(world,sim::set_building_operation(field,true));
    s::until(world,[&]{return s::field(world,field).ready_tick.has_value();},"Next year's real field work never produced a pending ready crop");
    const auto ready=*s::field(world,field).ready_tick;const auto expiry=sim::seasonal_ready_expiry(orig::ModelBuilding::WheatField,ready);
    check(expiry && s::progress(world,field)==80 && !q::field(world,field).harvested && !s::receipt_field(world,field).returned,
        "Growth readiness granted Food without the separate five at-field harvest work units");
    f::buy(world,sim::set_building_operation(field,false));
    s::until(world,[&]{return !s::active(world,city.farm);},"Pending ready crop did not cancel its paused empty worker");
    const auto inventory=world.hunting_food_state()->stocks;s::through(world,*expiry-1);
    check(s::field(world,field).ready_tick==ready && s::progress(world,field)==80,"Pending crop failed to retain its exact grace period");
    world.tick();check(!s::field(world,field).ready_tick && !s::progress(world,field) && s::field(world,field).expired_ready==1 &&
        !q::field(world,field).harvested && !s::receipt_field(world,field).returned &&
        std::equal(inventory.begin(),inventory.end(),world.hunting_food_state()->stocks.begin(),[](const auto& before,const auto& after){
            return before.building_id==after.building_id && before.amounts==after.amounts && before.received==after.received &&
                before.departed==after.departed && before.returned==after.returned && before.consumed==after.consumed;
        }),
        "Missed ready crop created stock, a receipt or retained an expired cycle");s::valid(world);
}

void loaded_return_and_ready_material_survive_only_their_authority() {
    s::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    f::buy(world,sim::set_building_operation(city.market,false));
    s::until(world,[&]{return world.physical_food_grower_cargo(s::farm(world,city.farm).courier_id)==1;},"Seasonal field never earned actual loaded Grower cargo");
    const auto trip=s::worker(world,city.farm);const auto channel=s::farm(world,city.farm).courier_id;
    f::buy(world,sim::set_building_operation(city.farm,false));
    s::until(world,[&]{return s::receipt_field(world,city.fields[0]).returned==1;},"Seasonal Farm pause discarded its genuine reserved Grower return");
    check(s::receipt_field(world,city.fields[0]).last_receipt->grower_id==trip.grower_id &&
        q::h::stock(world,city.farm).amounts[0]==1 && !world.physical_food_grower_cargo(channel),"Seasonal Farm return duplicated or reidentified physical Food");
    s::through(world,year+1);check(q::h::stock(world,city.farm).amounts[0]==1 && s::receipt_field(world,city.fields[0]).returned==1,
        "Season rollover expired already returned physical Food");

    auto materials=fixture.world();const auto scene=q::material_city(materials);
    s::until(materials,[&]{return p::plant(materials,scene.tea_plant).growth==80;},"Paid seasonal Tea never reached readiness");
    f::buy(materials,sim::set_building_operation(scene.tea_source,false));
    const auto ready=s::plant(materials,scene.tea_plant).ready_tick;check(ready.has_value(),"Ready Tea lacks its exact seasonal epoch");
    const auto expiry=sim::seasonal_ready_expiry(orig::ModelBuilding::TeaBush,*ready);check(expiry.has_value(),"Tea ready cycle lost finite expiry");
    s::through(materials,*expiry-1);check(s::plant(materials,scene.tea_plant).ready_tick==ready,"Paused ready Tea expired before its last grace phase");
    materials.tick();check(!s::plant(materials,scene.tea_plant).ready_tick && !p::plant(materials,scene.tea_plant).growth &&
        s::plant(materials,scene.tea_plant).expired_ready==1 && !p::plant(materials,scene.tea_plant).returned,
        "Expired unpicked Tea credited its source or remained a reusable ready cycle");s::valid(materials);
}

void blocked_loaded_return_crosses_winter_and_repairs() {
    s::Fixture fixture;
    for(int y=102;y<=110;++y)for(int x=93;x<=97;++x)
        fixture.raw[std::size_t(y)*228U+std::size_t(x)]=y==107?0x80U:4U;
    fixture.rebind();auto world=fixture.world();
    for(int x=74;x<=92;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int x=75;x<=90;x+=3)s::purchase(world,sim::CommandType::PlaceHousehold,{x,102});
    s::purchase(world,sim::CommandType::PlaceFireWatch,{75,105});s::purchase(world,sim::CommandType::PlaceFireWatch,{78,105});
    const auto market=s::purchase(world,sim::CommandType::PlaceMarket,{81,105});f::buy(world,sim::set_building_operation(market,false));
    s::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{84,105});s::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{87,105});
    const auto farm=s::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,105});const auto field=s::crop(world,farm,0,{95,107});
    const auto channel=s::farm(world,farm).courier_id;
    s::until(world,[&]{const auto& actor=world.courier(channel);return world.physical_food_grower_cargo(channel)==1 &&
        actor.edge_progress>0 && actor.path.size()>actor.path_vertex+2;},"Seasonal corridor never earned real loaded return cargo");
    const auto trip=s::worker(world,farm);const auto actor=world.courier(channel);const auto cut=actor.path.at(actor.path_vertex+2);
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    check(world.courier(channel).edge_progress==actor.edge_progress,"Seasonal road cut rewound an already begun loaded edge");
    s::through(world,year+1);
    check(world.physical_food_grower_cargo(channel)==1 && s::worker(world,farm).grower_id==trip.grower_id &&
        !s::receipt_field(world,field).returned && q::h::stock(world,farm).amounts[0]==0,
        "Winter or year rollover discarded carried crop or credited a blocked Farm return");
    auto replay=fixture.restore(world.snapshot());for(int i=0;i<100;++i){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Blocked winter crop cargo diverged after exact restore");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    s::until(world,[&]{return s::receipt_field(world,field).returned==1;},"Land repair failed to settle genuine crop cargo after winter");
    check(q::h::stock(world,farm).amounts[0]==1 && s::receipt_field(world,field).last_receipt->grower_id==trip.grower_id,
        "Repaired seasonal return reused identity or failed to credit exactly its original portion");s::valid(world);
}

void zero_staff_pause_fire_and_distinct_sibling_windows() {
    s::Fixture fixture;auto empty=fixture.world();
    for(int x=89;x<=104;++x)f::buy(empty,{sim::CommandType::PlaceRoad,{x,109}});
    const auto source=s::purchase(empty,sim::CommandType::PlaceOriginalFoodFarm,{90,110});const auto field=s::crop(empty,source,0,{93,110});
    s::through(empty,5*month);
    check(!s::farm(empty,source).spawned && !s::work(empty,field).productive_ticks && !q::field(empty,field).harvested,
        "A complete in-season interval granted work, a worker or Food without actual residents/staff");
    auto world=fixture.world();constexpr std::array<std::size_t,3> crops{0,1,4};const auto city=q::build(world,crops);
    s::until(world,[&]{return std::all_of(city.fields.begin(),city.fields.end(),[&](auto id){return s::receipt_field(world,id).returned>0;});},
        "One seasonal Farm failed to visit sibling crops in their distinct windows");
    for(const auto watch:city.watches)f::buy(world,sim::set_building_operation(watch,false));
    s::until(world,[&]{return world.building_on_fire(city.farm);},"Unprotected seasonal Farm never reached natural fire",12000);
    const auto earned=q::farm_work(world,city.farm);
    bool observed=false;
    for(int i=0;i<20 && world.building_on_fire(city.farm);++i) {
        world.tick();if(world.building_on_fire(city.farm)) {
            check(q::farm_work(world,city.farm)==earned,"Burning seasonal Farm created physical crop work");observed=true;
        }
    }
    check(observed,"Natural seasonal fire control did not observe a real halted interval");s::valid(world);
}

void finite_full_farm_keeps_only_real_reserved_portions() {
    s::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> crops{0};const auto city=q::build(world,crops);
    f::buy(world,sim::set_building_operation(city.market,false));
    s::until(world,[&]{return s::volume(q::h::stock(world,city.farm).amounts)==8 && !s::active(world,city.farm);},
        "Seasonal physical returns failed to fill the unchanged finite Farm bin");
    const auto workers=*world.physical_food_growers_state();const auto earned=*world.multi_field_agriculture_state();
    s::ticks(world,100);
    check(*world.physical_food_growers_state()==workers && *world.multi_field_agriculture_state()==earned &&
        s::volume(q::h::stock(world,city.farm).amounts)==8,"Full seasonal source allocated an extra worker, work or unreserved Food");
    s::through(world,9793);
    check(s::volume(q::h::stock(world,city.farm).amounts)==8 && s::receipt_field(world,city.fields[0]).returned==8,
        "Winter provisions were discarded or invented outside physical source receipts");s::valid(world);
}
}
int main() {
    try {calendar_masks_first_last_and_rollover();paid_mixed_city_five_complete_years();off_season_planting_partial_decay_and_ready_expiration();
        loaded_return_and_ready_material_survive_only_their_authority();blocked_loaded_return_crosses_winter_and_repairs();
        zero_staff_pause_fire_and_distinct_sibling_windows();finite_full_farm_keeps_only_real_reserved_portions();
        std::cout<<"Seasonal Agriculture nine plants/five complete years/physical expiry tests passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
