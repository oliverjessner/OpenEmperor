#include "FoodWarehouseFixture.h"
#include <iostream>

namespace {
namespace t=oe_food_warehouse_fixture;namespace sim=t::sim;
using t::check;
template<class F>void refuses(F action){try{action();}catch(const std::exception&){return;}throw std::runtime_error("Forged Warehouse state was admitted");}
void exact_successor_paid_identity_and_channel_budget(){
    t::Fixture fixture;auto w=fixture.world();const auto s=t::build(w);const auto id=s.warehouse;
    check(w.rule_version()==35&&sim::building_footprint(w.profile(),35,sim::Object::Warehouse)==sim::BuildingFootprint{2,2}&&w.building_cells(id).size()==4,
        "Warehouse successor changed existing complete footprint");
    check(w.construction_cost(sim::CommandType::PlaceWarehouse)==150&&w.workforce_required(id)==2,"Warehouse silently adopted a different model price/staff");
    check(std::count_if(w.couriers().begin(),w.couriers().end(),[&](const auto& c){return c.owner==id;})==3&&
        !t::volume(t::stock(w,id).amounts)&&!w.total_population(),"Construction created Food/population or omitted its third counted channel");
    const auto second=t::purchase(w,sim::CommandType::PlaceWarehouse,{125,116});const auto before=w.snapshot();
    check(!w.execute({sim::CommandType::PlaceWarehouse,{125,113}}).accepted&&w.snapshot()==before,"Third Warehouse changed paid limits or IDs");
    check(!w.execute(sim::demolish_building(id)).accepted&&w.snapshot()==before,"Food Warehouse acquired unsupported demolition");
    auto old=sim::World::create_food_processing(fixture.policy,fixture.model,fixture.terrain);const auto legacy=t::purchase(old,sim::CommandType::PlaceWarehouse,{128,116});
    check(!old.food_warehouses_state()&&std::none_of(old.processed_food_state()->stocks.begin(),old.processed_food_state()->stocks.end(),[&](const auto& r){return r.building_id==legacy;})&&
        std::count_if(old.couriers().begin(),old.couriers().end(),[&](const auto& c){return c.owner==legacy;})==2,"Rule34 Warehouse gained Food authority or a new channel");
    auto foreign=old.snapshot();foreign.food_warehouses=sim::FoodWarehousesState{};refuses([&]{(void)sim::World::restore_food_processing(foreign,fixture.policy,fixture.model,fixture.terrain);});
    check(fixture.restore(w.snapshot()).canonical_state()==w.canonical_state(),"Empty paid Warehouse changed on exact restore");t::valid(w);(void)second;
}
void physical_three_kind_storage_and_warehouse_only_processing_and_distribution(){
    t::Fixture fixture;auto w=fixture.world();constexpr std::array<std::size_t,1> kinds{0};auto s=t::build(w,true,kinds);
    const auto millet=t::single_crop_farm(w,1,{94,125}),soy=t::single_crop_farm(w,4,{104,125});
    for(const auto id:{s.mill,s.market,s.shop})t::operate(w,id,false);
    t::until(w,[&]{return t::kinds_held(w,s.warehouse)>=3;},"Three actual Farm kinds never reached the paid Warehouse");
    const auto stored=w.snapshot();check(t::history(w,s.warehouse).first_received[0]&&t::history(w,s.warehouse).first_received[1]&&t::history(w,s.warehouse).first_received[4],
        "Simultaneous Warehouse stock lacks actual typed first receipts");
    for(const auto id:{s.farm,millet,soy})t::operate(w,id,false);s.second_mill=t::add_second_mill(w);t::f::buy(w,sim::set_mill_recipe(s.second_mill,3));
    t::until(w,[&]{return t::stock(w,s.warehouse).received[9]>0;},"Warehouse-only ingredients did not make real grade3 meals and return them to storage");
    const auto& m=t::mill(w,s.second_mill);check(m.completed_by_grade[2]>0,"No actual grade3 recipe completed");
    for(std::size_t g=0;g<7;++g)if(t::stock(w,s.second_mill).received[g])check(m.first_ingredients[g]&&m.first_ingredients[g]->source==s.warehouse,
        "Fresh Mill borrowed a direct Farm delivery instead of the Warehouse route");
    check(t::history(w,s.warehouse).first_received[9]->source==s.second_mill,"Stored meal lacked its actual producing Mill");
    t::operate(w,s.second_mill,false);s.second_market=t::add_second_market(w);t::operate(w,s.shop,true);
    t::until(w,[&]{return t::stock(w,s.second_market).received!=sim::ProcessedFoodCounters{}&&t::stock(w,s.shop).received[9]>0;},
        "Warehouse failed its independent raw Market and graded Shop delivery legs");
    for(std::size_t g=0;g<12;++g)check(!t::stock(w,s.warehouse).consumed[g],"Warehouse consumed House Food");
    t::until(w,[&]{for(const auto& h:w.food_processing_state()->houses)if(h.first_consumption&&h.first_consumption->grade==3)return true;return false;},
        "Warehouse-routed meals never reached actual House consumption");
    bool raw_used=false;for(const auto& stock:w.processed_food_state()->stocks)if(sim::original_house_kind(w.building(stock.building_id).kind))
        for(std::size_t g=0;g<7;++g)raw_used|=stock.consumed[g]>0;
    check(raw_used,"Independent Warehouse-fed Market never supplied actual raw House consumption");
    auto restored=fixture.restore(w.snapshot());for(int i=0;i<1200;++i){w.tick();restored.tick();}check(w.snapshot()==restored.snapshot(),"Warehouse-held Food/receipts or real return diverged after restore");
    t::valid(w);check(stored.food_warehouses.has_value(),"Three-kind control lost its generation");
}
void earned_twelve_thousand_tick_continuation_and_atomic_hostile_bounds(){
    t::Fixture fixture;auto a=fixture.world();const auto s=t::build(a);auto b=fixture.restore(a.snapshot());
    for(int i=0;i<12000;++i){if(i==2000||i==2031){t::operate(a,s.warehouse,i==2031);t::operate(b,s.warehouse,i==2031);}a.tick();b.tick();
        if(i%503==0){t::valid(a);check(a.snapshot()==b.snapshot(),"Ordinary paid Warehouse continuation diverged");b=fixture.restore(b.snapshot());}}
    const auto before=a.snapshot();const auto reject=[&](auto mutate){auto hostile=before;mutate(hostile);refuses([&]{(void)fixture.restore(hostile);});check(a.snapshot()==before,"Rejected Warehouse restore changed live authority");};
    reject([](auto& s){s.food_warehouses.reset();});
    reject([](auto& s){s.food_warehouses->warehouses.push_back(s.food_warehouses->warehouses[0]);});
    reject([](auto& s){s.food_warehouses->warehouses[0].warehouse_id=s.buildings[0].id;});
    reject([&](auto& state){for(auto& row:state.processed_food->stocks)if(row.building_id==s.warehouse)row.amounts[11]=1;});
    check(a.snapshot()==b.snapshot(),"Terminal paid replay differs");t::valid(a);
}
}
int main(){try{exact_successor_paid_identity_and_channel_budget();physical_three_kind_storage_and_warehouse_only_processing_and_distribution();
    earned_twelve_thousand_tick_continuation_and_atomic_hostile_bounds();std::cout<<"Food Warehouses paid identity, three-kind storage, physical redistribution and12k replay passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
