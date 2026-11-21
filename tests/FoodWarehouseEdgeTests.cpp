#include "FoodWarehouseFixture.h"
#include <iostream>

namespace {
namespace t=oe_food_warehouse_fixture;namespace sim=t::sim;
using t::check;using Phase=sim::OriginalGoodsTransferPhase;
void intake_quota_then_begun_cut_source_pause_and_exact_return(){
    t::Fixture fixture;auto w=fixture.world();constexpr std::array<std::size_t,1> wheat{0};auto s=t::build(w,true,wheat);
    for(const auto id:{s.mill,s.market,s.shop})t::operate(w,id,false);
    t::until(w,[&]{return t::stock(w,s.warehouse).amounts[0]==4;},"Real raw intake never filled its finite per-kind Warehouse quota");
    const auto held=t::stock(w,s.warehouse);t::ticks(w,150);check(t::stock(w,s.warehouse)==held,"A fifth raw portion bypassed the actual per-kind quota");
    s.second_market=t::add_second_market(w);const auto id=t::channel(w,s.warehouse);
    t::until(w,[&]{const auto& c=w.courier(id);return c.phase==sim::CourierPhase::ToWarehouse&&c.edge_progress>0&&c.path_vertex+2<c.path.size()&&t::cargo(t::trip(w,id))>0;},
        "Warehouse did not load and begin a real outward Food edge");
    const auto begun=w.courier(id);const auto loaded=t::trip(w,id);const auto received=t::stock(w,s.second_market).received;
    std::optional<sim::Cell> cut;
    for(std::size_t i=begun.path_vertex+2;i<begun.path.size();++i){const sim::Command command{sim::CommandType::RemoveRoad,begun.path[i]};
        if(!w.validate(command).accepted)continue;auto probe=w;if(probe.execute(command).accepted&&!probe.find_building_route(s.warehouse,s.second_market)){cut=begun.path[i];break;}}
    check(cut.has_value(),"Real Warehouse route has no independently removable future cut");t::f::buy(w,{sim::CommandType::RemoveRoad,*cut});
    check(w.courier(id).edge_progress==begun.edge_progress&&t::trip(w,id).loaded==loaded.loaded,"Cut rewound a begun Warehouse edge or replaced its Food");
    t::ticks(w,70);check(t::cargo(t::trip(w,id))==t::cargo(loaded)&&t::stock(w,s.second_market).received==received,
        "Disconnected Warehouse cargo vanished or was credited without arrival");
    auto restored=fixture.restore(w.snapshot());for(int i=0;i<31;++i){w.tick();restored.tick();}check(w.snapshot()==restored.snapshot(),"Blocked Warehouse cargo failed exact restore continuation");
    t::operate(w,s.warehouse,false);const auto before=t::stock(w,s.warehouse);
    t::until(w,[&]{return t::trip(w,id).phase==Phase::Returned;},"Paused Warehouse failed to settle its genuine loaded return");
    check(t::trip(w,id).returned==loaded.loaded&&!t::cargo(t::trip(w,id))&&t::stock(w,s.second_market).received==received,"Returned Food was lost, delivered or booked twice");
    for(std::size_t g=0;g<12;++g)check(t::stock(w,s.warehouse).amounts[g]==before.amounts[g]+loaded.loaded[g]&&
        t::stock(w,s.warehouse).returned[g]==before.returned[g]+static_cast<unsigned>(loaded.loaded[g]),"Exact Warehouse return did not use its reserved original slots");
    const auto terminal=t::stock(w,s.warehouse);t::ticks(w,40);check(t::stock(w,s.warehouse)==terminal,"Terminal return credited Food again");
    t::f::buy(w,{sim::CommandType::PlaceRoad,*cut});t::operate(w,s.warehouse,true);
    t::until(w,[&]{return t::stock(w,s.second_market).received[0]>0;},"Paid repair did not restore a real Warehouse-to-Market delivery");t::valid(w);
}
void full_shared_compartment_waits_without_discard_or_household_authority(){
    t::Fixture fixture;auto w=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto s=t::build(w,true,wheat);
    // Each paid producer has its own finite source buffer. A single early
    // crop cannot fill the Farm's8 slots and block the later seasonal crops.
    const std::array extra_farms{t::single_crop_farm(w,1,{94,125}),t::single_crop_farm(w,2,{104,125}),
        t::single_crop_farm(w,3,{114,125}),t::single_crop_farm(w,4,{124,125})};
    t::operate(w,s.market,false);t::operate(w,s.shop,false);
    std::uint8_t grade=1;t::f::buy(w,sim::set_mill_recipe(s.mill,grade));
    // Stop at the first genuinely full shared bin; requiring every grade to
    // fill first could itself wait forever behind the32-slot backpressure.
    for(int tick=0;tick<50000&&t::volume(t::stock(w,s.warehouse).amounts)<32;++tick){
        if(grade<4&&t::stock(w,s.warehouse).amounts[6+grade]==4)t::f::buy(w,sim::set_mill_recipe(s.mill,++grade));w.tick();}
    t::operate(w,s.mill,false);
    t::until(w,[&]{return t::volume(t::stock(w,s.warehouse).amounts)==32;},"Real raw/meal supply never filled the shared32-slot Food compartment",50000);
    const auto full=t::stock(w,s.warehouse);const auto before=w.snapshot();check(!full.quality_witness&&!full.missed_demands,"Warehouse stock became a House need or quality witness");
    check(w.agriculture_farms().size()==5&&t::volume(full.amounts)==32,"Full-compartment control did not use exactly five real paid Farm buffers and32 held portions");
    check(!w.execute(sim::demolish_building(s.warehouse)).accepted&&w.snapshot()==before,"Full Warehouse demolition discarded paid Food");
    t::ticks(w,100);check(t::stock(w,s.warehouse)==full,"Full Warehouse overflowed, consumed its stock or accepted a duplicate receipt");
    t::operate(w,s.market,true);t::operate(w,s.shop,true);t::until(w,[&]{return t::volume(t::stock(w,s.warehouse).amounts)<32;},"Eligible real outlets never drained full Food storage");t::valid(w);(void)extra_farms;
}
void real_zero_staff_holds_stock_and_cannot_start_a_food_dispatch(){
    t::Fixture fixture;auto w=fixture.world();for(int x=89;x<=132;++x)t::f::buy(w,{sim::CommandType::PlaceRoad,{x,109}});
    t::purchase(w,sim::CommandType::PlaceHousehold,{90,107});const auto farm=t::purchase(w,sim::CommandType::PlaceOriginalFoodFarm,{90,110});t::crop(w,farm,0,{93,110});
    const auto market=t::purchase(w,sim::CommandType::PlaceMarket,{97,110});const auto well=t::purchase(w,sim::CommandType::PlaceOriginalWaterWell,{101,110});
    const auto ancestor=t::purchase(w,sim::CommandType::PlaceOriginalAncestorShrine,{105,110});const auto watch=t::purchase(w,sim::CommandType::PlaceFireWatch,{109,110});
    const auto other_watch=t::purchase(w,sim::CommandType::PlaceFireWatch,{112,110});const auto warehouse=t::purchase(w,sim::CommandType::PlaceWarehouse,{128,110});
    const auto mill=t::purchase(w,sim::CommandType::PlaceOriginalMill,{119,110});t::operate(w,mill,false);
    t::f::buy(w,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High));
    t::f::buy(w,sim::set_building_workforce_priority(warehouse,sim::WorkforcePriority::High));
    t::until(w,[&]{return t::stock(w,warehouse).amounts[0]>0&&w.workers_assigned(warehouse)==2;},"Scarce-worker Warehouse lacks an actual staffed raw arrival");
    t::f::buy(w,sim::set_building_workforce_priority(mill,sim::WorkforcePriority::High));t::operate(w,mill,true);
    t::f::buy(w,sim::set_building_workforce_priority(warehouse,sim::WorkforcePriority::Low));
    const auto actor=t::channel(w,warehouse);check(w.workers_assigned(warehouse)==0&&w.courier_dispatch_status(actor).status==sim::CourierDispatchStatus::Unstaffed,
        "Scarce paid labor did not establish a genuinely unstaffed Food Warehouse");
    const auto departures=t::stock(w,warehouse).departed;t::ticks(w,40);check(t::stock(w,warehouse).departed==departures,"Zero-staff Warehouse loaded a new Food trip");
    t::operate(w,farm,false);t::operate(w,mill,false);t::f::buy(w,sim::set_building_workforce_priority(warehouse,sim::WorkforcePriority::High));
    t::until(w,[&]{return t::stock(w,warehouse).departed[0]>departures[0];},"Actually staffed Warehouse failed to dispatch its physically held Food");
    check(w.workers_assigned(warehouse)==2,"Warehouse used invented extra staffing");t::valid(w);(void)market;(void)well;(void)ancestor;(void)watch;(void)other_watch;
}
void natural_fire_preserves_real_storage_and_refuses_new_dispatches(){
    t::Fixture fixture;auto w=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto s=t::build(w,false,wheat);
    for(const auto id:{s.mill,s.market,s.shop})t::operate(w,id,false);
    t::until(w,[&]{return t::stock(w,s.warehouse).amounts[0]>0;},"Natural-fire control lacks real Warehouse stock");
    t::operate(w,s.farm,false);t::operate(w,s.warehouse,false);t::until(w,[&]{return w.building_on_fire(s.warehouse);},"Unprotected Warehouse never entered actual natural fire");
    const auto stock=t::stock(w,s.warehouse);t::operate(w,s.market,true);t::operate(w,s.warehouse,true);const auto actor=t::channel(w,s.warehouse);
    check(w.courier_dispatch_status(actor).status==sim::CourierDispatchStatus::OnFire,"Burning Warehouse can initiate a Food dispatch");
    t::ticks(w,20);check(w.building_on_fire(s.warehouse)&&t::stock(w,s.warehouse)==stock,"Fire erased, consumed or dispatched held Warehouse Food");
    const auto restored=fixture.restore(w.snapshot());check(restored.snapshot()==w.snapshot(),"Burning Warehouse changed on exact restore");
    t::until(w,[&]{return t::stock(w,s.market).received[0]>0;},"Recovered staffed Warehouse never physically redistributed its retained stock");t::valid(w);
}
void food_goods_and_auxiliary_materials_coexist_without_a_mirrored_inventory(){
    t::Fixture fixture;auto w=fixture.world();const auto material=t::p::q::material_city(w);
    for(int y=108;y<=109;++y){t::f::buy(w,{sim::CommandType::PlaceRoad,{143,y}});t::f::buy(w,{sim::CommandType::PlaceRoad,{145,y}});}
    const auto farm=t::purchase(w,sim::CommandType::PlaceOriginalFoodFarm,{142,110});t::crop(w,farm,0,{145,110});
    const auto positive=[&]{bool goods=false,aux=false;for(const auto& s:w.original_goods_state()->stocks)if(s.building_id==material.warehouse)goods=s.amounts[static_cast<std::size_t>(openemperor::original::Goods::Tea)]>0;
        for(const auto& s:w.original_production_state()->industry.auxiliary)if(s.building_id==material.warehouse)aux=s.amounts[static_cast<std::size_t>(sim::OriginalAuxMaterial::Wood)]>0;
        return goods&&aux&&t::stock(w,material.warehouse).amounts[0]>0;};
    t::until(w,positive,"Actually produced Food, Tea and Wood did not coexist in separate Warehouse compartments",50000);t::valid(w);
}
}
int main(){try{intake_quota_then_begun_cut_source_pause_and_exact_return();full_shared_compartment_waits_without_discard_or_household_authority();
    real_zero_staff_holds_stock_and_cannot_start_a_food_dispatch();natural_fire_preserves_real_storage_and_refuses_new_dispatches();
    food_goods_and_auxiliary_materials_coexist_without_a_mirrored_inventory();std::cout<<"Food Warehouses finite quotas/full bins, real cut/return, zero staff, natural fire and Goods/Aux coexistence passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
