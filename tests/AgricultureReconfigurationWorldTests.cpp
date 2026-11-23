#include "AgricultureReconfigurationFixture.h"
#include <iostream>
namespace {
namespace t=oe_agriculture_reconfiguration_fixture;namespace sim=t::sim;using t::check;
void idle_field_rebuild_is_paid_and_active_quota_excludes_retired(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat);
    const auto old=city.fields.front();const auto cell=world.building(old).cell;const auto funds=world.treasury();const auto before=world.snapshot();
    check(world.demolition_status(old).allowed,"Genuinely idle empty paid Field was refused");t::f::buy(world,sim::demolish_building(old));
    check(world.treasury()==funds&&!world.building(old).placed&&!world.building_owner_at(cell)&&world.agriculture_fields().size()==before.processed_food->fields.size(),"Idle removal refunded Funds, erased history or retained cell authority");
    const auto next=world.snapshot().next_building_id;const auto price=world.construction_cost(sim::CommandType::PlaceOriginalSoybeanField);
    const auto rebuilt=t::crop(world,city.farm,4,cell);check(static_cast<unsigned>(rebuilt)==next&&rebuilt>old&&world.treasury()==funds-price&&t::field(world,old).crop==sim::AgricultureCrop::Wheat&&t::field(world,rebuilt).crop==sim::AgricultureCrop::Soybean,"Rebuild reused identity/crop history or evaded full price");
    for(const sim::Cell at:std::array<sim::Cell,7>{{{93,111},{93,112},{93,113},{93,114},{93,115},{94,110},{94,111}}})t::crop(world,city.farm,0,at);
    check(world.agriculture_fields().size()==9,"Retired Field did not remain in lifetime catalog");
    sim::Command ninth{sim::CommandType::PlaceOriginalWheatField,{94,112}};ninth.building_id=city.farm;t::refused(world,ninth,"Ninth active Field bypassed eight-per-Farm quota");
    auto restored=t::json_roundtrip(fixture,world,"idle-rebuilt52.json");check(restored.snapshot()==world.snapshot(),"Rebuilt site failed exact restore");t::valid(world);
    auto old_rule=sim::World::create_food_warehouses(fixture.policy,fixture.model,fixture.terrain);const auto old_farm=t::purchase(old_rule,sim::CommandType::PlaceOriginalFoodFarm,{90,110});const auto old_field=t::crop(old_rule,old_farm,0,{93,110});
    t::refused(old_rule,sim::begin_agriculture_drain(old_field),"Rule35 acquired new drain authority");t::refused(old_rule,sim::demolish_building(old_field),"Rule35 demolition outcomes changed");
}
void partial_then_loaded_field_drain_preserves_actual_return_and_other_field(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,2> kinds{0,4};const auto city=t::q::build(world,kinds);const auto target=city.fields.front();
    t::until(world,[&]{const auto& s=t::worker(world,city.farm);return s.latest_trip&&!s.latest_trip->returned_tick&&s.latest_trip->field_id==target&&world.agriculture_field_progress(target)>0&&world.agriculture_field_progress(target)<80;},"No actual partial Field work");
    const auto start=world.snapshot();t::f::buy(world,sim::begin_agriculture_drain(target));check(world.ticks()==start.ticks&&world.treasury()==start.treasury&&world.agriculture_draining(target),"Beginning drain granted time, Funds or failed to persist");
    t::refused(world,sim::demolish_building(target),"Partial Field work was erased");auto restored=t::json_roundtrip(fixture,world,"partial52.json");
    t::until(world,[&]{const auto& s=t::worker(world,city.farm);return s.latest_trip&&s.latest_trip->field_id==target&&sim::physical_food_trip_cargo(*s.latest_trip)>0;},"Pending partial cycle did not finish real harvest");
    check(t::receipt(world,target).returned<t::field(world,target).harvested,"Loaded harvest credited Food before Farm return");t::refused(world,sim::demolish_building(target),"Loaded Grower or reservation disappeared");
    restored=t::json_roundtrip(fixture,world,"loaded52.json");
    for(int n=0;n<1500&&!world.demolition_status(target).allowed;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Pending loaded return replay diverged");}
    check(world.demolition_status(target).allowed&&t::receipt(world,target).returned>0,"Pending Field did not become removable after real return");
    const auto earned=t::field(world,target);const auto worked=t::work(world,target);const auto received=t::receipt(world,target);t::f::buy(world,sim::demolish_building(target));
    const auto rebuilt=t::crop(world,city.farm,1,world.building(target).cell);check(rebuilt>target,"Replacement Field reused retired ID");
    t::until(world,[&]{return t::receipt(world,city.fields[1]).returned>0;},"Other paid Field stopped producing after reconfiguration");
    t::ticks(world,1500);check(t::field(world,target)==earned&&t::work(world,target)==worked&&t::receipt(world,target)==received,"Retired Field history regrew, expired or changed with later seasons");
    t::valid(world);check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Retired receipt/source cursor or rebuilt site failed restore");
}
void loaded_plant_drains_once_and_rebuild_preserves_material_origin(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::q::material_city(world);const auto target=city.tea_plant;
    t::until(world,[&]{const auto s=t::p::source(world,city.tea_source);return s.latest_trip&&s.latest_trip->plant_id==target&&s.latest_trip->picked_tick&&!s.latest_trip->returned_tick;},"No actual loaded plantation Grower");
    check(t::p::plant(world,target).growth==0,"First picked plant already regrew before drain command");t::f::buy(world,sim::begin_agriculture_drain(target));t::refused(world,sim::demolish_building(target),"Loaded plantation cargo was erased");
    auto restored=t::json_roundtrip(fixture,world,"plant-loaded52.json");
    for(int n=0;n<2000&&!world.demolition_status(target).allowed;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Plant loaded drain replay diverged");}
    check(world.demolition_status(target).allowed&&t::p::plant(world,target).returned>0&&t::p::plant(world,target).growth==0,"Plant drain regrew or failed physical source receipt");
    const auto original=t::p::plant(world,target);const auto funds=world.treasury();const auto price=world.construction_cost(sim::CommandType::PlaceOriginalTeaBush);const auto cell=world.building(target).cell;
    t::f::buy(world,sim::demolish_building(target));const auto rebuilt=t::p::plant(world,sim::CommandType::PlaceOriginalTeaBush,city.tea_source,cell);
    check(rebuilt>target&&world.treasury()==funds-price,"Plant rebuild refunded/reused identity");t::ticks(world,600);check(t::p::plant(world,target)==original,"Retired material plant changed history");t::valid(world);
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Historical plant/source and rebuilt overlap failed restore");
}
void pump_and_ditch_reconfiguration_stops_then_recovers_actual_wet_work(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat,false);const auto water=t::i::network(world);
    t::purchase(world,sim::CommandType::PlaceFireWatch,{86,96});t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});const auto target=city.fields.front();
    t::until(world,[&]{return t::i::work(world,target).wet_work_ticks>0;},"No actual wet work before Pump change");
    t::f::buy(world,sim::begin_agriculture_drain(water.pump));check(!world.irrigation_field_status(world.building(target).cell).water_available,"Pending Pump continued granting water");
    t::until(world,[&]{return world.demolition_status(water.pump).allowed;},"Real Pump commitments never cleared");t::f::buy(world,sim::demolish_building(water.pump));
    const sim::Cell cut{88,114};t::f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    const auto before=t::i::work(world,target).wet_work_ticks;t::ticks(world,25);check(t::i::work(world,target).wet_work_ticks==before,"Retired Pump supplied wet work");
    const auto replacement=t::i::i::pump(world,t::i::pump_origin);check(replacement>water.pump,"Pump identity reused");t::f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    t::until(world,[&]{return t::i::work(world,target).wet_work_ticks>before;},"Paid Pump/ditch rebuild failed actual wet production recovery");
    auto restored=t::json_roundtrip(fixture,world,"pump-rebuilt52.json");check(restored.snapshot()==world.snapshot(),"Rebuilt Pump source context lost retained installation history");t::valid(world);
}
}
int main(){try{idle_field_rebuild_is_paid_and_active_quota_excludes_retired();partial_then_loaded_field_drain_preserves_actual_return_and_other_field();loaded_plant_drains_once_and_rebuild_preserves_material_origin();pump_and_ditch_reconfiguration_stops_then_recovers_actual_wet_work();std::cout<<"Agriculture reconfiguration paid Field/plant/Pump drain, history, rebuild and exact continuation passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
