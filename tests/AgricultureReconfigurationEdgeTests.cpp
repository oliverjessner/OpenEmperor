#include "AgricultureReconfigurationFixture.h"
#include <iostream>
namespace {
namespace t=oe_agriculture_reconfiguration_fixture;namespace sim=t::sim;using t::check;
template<class F>void rejects(F run){try{run();}catch(const std::exception&){return;}throw std::runtime_error("Hostile reconfiguration snapshot was accepted");}
void begun_empty_assignment_returns_without_first_work_or_harvest(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat);const auto target=city.fields.front();
    t::until(world,[&]{const auto s=t::worker(world,city.farm);if(!s.latest_trip||s.latest_trip->returned_tick||s.latest_trip->field_id!=target)return false;const auto& c=world.courier(s.courier_id);return s.latest_trip->phase==sim::PhysicalFoodGrowerPhase::Outward&&c.edge_progress>0&&world.agriculture_field_progress(target)==0;},"No actual empty outward begun Grower edge");
    const auto c=world.courier(t::worker(world,city.farm).courier_id);const auto work=t::work(world,target);const auto harvested=t::field(world,target).harvested;
    t::f::buy(world,sim::begin_agriculture_drain(target));check(world.courier(c.id).edge_progress==c.edge_progress&&world.courier(c.id).path==c.path,"Drain rewound a begun Grower edge");
    t::refused(world,sim::demolish_building(target),"Removal erased a begun empty actor");auto restored=t::json_roundtrip(fixture,world,"empty-begun52.json");
    for(int n=0;n<1000&&!world.demolition_status(target).allowed;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Begun empty drain changed after exact restore");}
    check(world.demolition_status(target).allowed&&t::work(world,target)==work&&t::field(world,target).harvested==harvested,"Pending zero-progress assignment started a new crop or failed its actual empty return");
    t::f::buy(world,sim::demolish_building(target));t::valid(world);
}
void paid_source_road_cut_preserves_pending_partial_work_then_repair_finishes_once(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat);const auto target=city.fields.front();
    const std::array<sim::Cell,3> cuts{{{90,109},{91,109},{92,109}}};
    const auto removable=[&]{auto probe=world;for(const auto cell:cuts)if(!probe.execute({sim::CommandType::RemoveRoad,cell}).accepted)return false;return !probe.find_building_route(city.farm,city.market);};
    t::until(world,[&]{const auto progress=world.agriculture_field_progress(target);return progress>0&&progress<30&&removable();},"No genuinely partial worker with independently removable Farm road access");
    t::f::buy(world,sim::begin_agriculture_drain(target));const auto earned=t::work(world,target);const auto count=t::field(world,target).harvested;
    for(const auto cell:cuts)t::f::buy(world,{sim::CommandType::RemoveRoad,cell});t::refused(world,sim::demolish_building(target),"Road cut discarded pending crop work/assignment");
    auto restored=t::json_roundtrip(fixture,world,"pending-road-cut52.json");for(int n=0;n<20;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Disconnected pending-worker continuation diverged");}
    check(t::work(world,target)==earned&&t::field(world,target).harvested==count&&world.agriculture_field_progress(target)>0,"Road loss fabricated harvest or erased retained partial work");
    const auto funds=world.treasury();for(const auto cell:cuts)t::f::buy(world,{sim::CommandType::PlaceRoad,cell});check(world.treasury()<funds,"Physical Farm road repair was free");
    t::until(world,[&]{return world.demolition_status(target).allowed;},"Paid repair did not let the pending real cycle finish and return");check(t::field(world,target).harvested==count+1&&t::receipt(world,target).returned==count+1,"Pending repaired cycle did not return exactly one earned harvest");
    t::f::buy(world,sim::demolish_building(target));t::valid(world);
}
void unrelated_real_grower_claims_a_drained_plant_footprint_and_begun_edge(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::q::material_city(world);sim::CourierId actor{};std::vector<sim::Cell> route;
    for(const auto& c:world.couriers())if(c.owner==city.tea_source&&c.role==sim::CourierRole::OriginalPlantationGrower){actor=c.id;for(const auto& r:c.dynamic_target_routes)if(r.first==city.tea_plant&&r.second)route=*r.second;}
    check(route.size()>2,"Unrelated footprint control lacks an actual prepared plantation path");std::optional<sim::BuildingId> empty;
    for(std::size_t n=1;n+1<route.size()&&!empty;++n){sim::Command c{sim::CommandType::PlaceOriginalTeaBush,route[n]};c.building_id=city.tea_source;if(world.validate(c).accepted){t::f::buy(world,c);empty=world.building_owner_at(c.cell);}}
    check(empty.has_value(),"No legally paid empty plant fits the actual worker corridor");t::f::buy(world,sim::begin_agriculture_drain(*empty));const auto cell=world.building(*empty).cell;
    t::until(world,[&]{const auto& c=world.courier(actor);return c.target==city.tea_plant&&c.phase!=sim::CourierPhase::IdleAtWorkshop&&!c.path.empty()&&(c.path[c.path_vertex]==cell||(c.edge_progress&&c.path_vertex+1<c.path.size()&&c.path[c.path_vertex+1]==cell));},"Unrelated actual Grower did not enter or begin the paid empty plant footprint");
    check(!t::p::plant(world,*empty).growth&&world.demolition_status(*empty).blocker==sim::DemolitionBlocker::OwnedActiveCourier,"Empty unrelated footprint did not report actual actor/begun-edge blocker");
    t::refused(world,sim::demolish_building(*empty),"Removal erased an unrelated real worker's current/begun cell");
    auto restored=t::json_roundtrip(fixture,world,"unrelated-begun52.json");check(restored.snapshot()==world.snapshot(),"Unrelated occupied drain checkpoint changed authority");
    t::until(world,[&]{return world.demolition_status(*empty).allowed;},"Unrelated actor never physically cleared the footprint");t::f::buy(world,sim::demolish_building(*empty));t::valid(world);
}
void finite_journal_and_hostile_terminal_histories_fail_atomically(){
    t::Fixture fixture;auto world=fixture.world();const auto farm=t::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});sim::BuildingId first{};
    for(int n=0;n<8;++n){const auto id=t::crop(world,farm,0,{93,110});if(!n)first=id;t::f::buy(world,sim::demolish_building(id));}
    check(world.agriculture_reconfiguration_state()->entries.size()==8,"Closed rows did not consume the bounded lifetime journal");const auto ninth=t::crop(world,farm,4,{93,110});
    t::refused(world,sim::begin_agriculture_drain(ninth),"Ninth journal row bypassed capacity");t::refused(world,sim::demolish_building(ninth),"Journal overflow erased a paid leaf");
    const auto before=world.snapshot();const auto reject=[&](auto mutation){auto bad=before;mutation(bad);rejects([&]{(void)fixture.restore(bad);});check(world.snapshot()==before,"Rejected hostile restore changed live authority");};
    reject([](auto& s){s.agriculture_reconfiguration.reset();});
    reject([](auto& s){s.agriculture_reconfiguration->entries.push_back(s.agriculture_reconfiguration->entries.front());});
    reject([](auto& s){s.agriculture_reconfiguration->entries.front().removed_sequence.reset();});
    reject([](auto& s){s.agriculture_reconfiguration->entries.front().started_tick=s.ticks+1;});
    reject([](auto& s){s.agriculture_reconfiguration->entries.front().started_sequence=0;});
    reject([&](auto& s){for(auto& b:s.buildings)if(b.id==first)b.placed=true;});
    reject([&](auto& s){for(auto& r:s.multi_field_agriculture->fields)if(r.field_id==first)r.productive_ticks=1;});
    reject([&](auto& s){for(auto& r:s.physical_food_growers->fields)if(r.field_id==first)r.returned=1;});
    reject([&](auto& s){for(auto& r:s.irrigation_productivity->work)if(r.building_id==first)r.wet_work_ticks=1;});
    check(fixture.restore(before).snapshot()==before,"Full legitimate journal failed exact restore");t::valid(world);
}
void closed_zero_work_leaf_cannot_move_to_an_immutable_blocked_source_cell(){
    t::Fixture fixture(true);auto world=fixture.world();const auto farm=t::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});const auto field=t::crop(world,farm,0,{93,110});t::f::buy(world,sim::demolish_building(field));
    const auto before=world.snapshot();auto hostile=before;for(auto& b:hostile.buildings)if(b.id==field)b.cell={94,114};
    check(!fixture.policy->building_allowed({94,114})&&fixture.terrain->raw_terrain({94,114})==4U,"Field negative lacks immutable disallowed source water");rejects([&]{(void)fixture.restore(hostile);});check(world.snapshot()==before,"Rejected historical Field source claim changed live authority");
    auto material=fixture.world();const auto city=t::q::material_city(material);t::f::buy(material,sim::demolish_building(city.tea_plant));const auto original=material.snapshot();hostile=original;for(auto& b:hostile.buildings)if(b.id==city.tea_plant)b.cell={95,108};
    check(!fixture.policy->building_allowed({95,108})&&fixture.terrain->raw_terrain({95,108})==4U,"Plant negative lacks immutable disallowed source water");rejects([&]{(void)fixture.restore(hostile);});check(material.snapshot()==original,"Rejected historical plant source claim changed live authority");
    check(fixture.restore(before).snapshot()==before&&fixture.restore(original).snapshot()==original,"Legitimate zero-work retired leaves failed exact source-bound restore");
}
void loaded_food_and_working_or_stocked_owners_remain_exact_refusals(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::w::build(world,true,wheat);
    t::f::buy(world,sim::set_mill_recipe(city.mill,1));
    t::until(world,[&]{for(const auto& c:world.couriers())if(c.owner==city.farm&&c.role==sim::CourierRole::OriginalFoodToMarket&&world.original_food_cargo(c.id)>0)return true;return false;},"No real loaded Food courier refusal control");
    t::refused(world,sim::begin_agriculture_drain(city.farm),"Farm drain deleted child/channel authority");t::refused(world,sim::demolish_building(city.farm),"Loaded Food courier lost stock-owner authority");
    t::until(world,[&]{return t::w::mill(world,city.mill).active.has_value();},"No actual working Mill refusal control");
    t::refused(world,sim::demolish_building(city.mill),"Working Mill discarded already-debited recipe inputs");t::refused(world,sim::begin_agriculture_drain(city.mill),"Mill gained unsupported source drain");
    for(const auto id:{city.mill,city.market,city.shop})t::operate(world,id,false);
    t::until(world,[&]{return t::w::volume(t::stock(world,city.warehouse).amounts)>0;},"No actual stored Warehouse Food refusal control");
    t::refused(world,sim::demolish_building(city.warehouse),"Stocked Warehouse discarded sole-account Food");t::refused(world,sim::begin_agriculture_drain(city.warehouse),"Warehouse gained unsupported owner retirement");t::valid(world);
}
void actually_full_shared_warehouse_refuses_without_stock_or_history_loss(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::w::build(world,true,wheat);
    const std::array extra{t::w::single_crop_farm(world,1,{94,125}),t::w::single_crop_farm(world,2,{104,125}),t::w::single_crop_farm(world,3,{114,125}),t::w::single_crop_farm(world,4,{124,125})};
    t::operate(world,city.market,false);t::operate(world,city.shop,false);std::uint8_t grade=1;t::f::buy(world,sim::set_mill_recipe(city.mill,grade));
    for(int n=0;n<50000&&t::w::volume(t::stock(world,city.warehouse).amounts)<32;++n){if(grade<4&&t::stock(world,city.warehouse).amounts[6+grade]==4)t::f::buy(world,sim::set_mill_recipe(city.mill,++grade));world.tick();}
    t::operate(world,city.mill,false);t::until(world,[&]{return t::w::volume(t::stock(world,city.warehouse).amounts)==32;},"Five paid independent buffers never filled the actual32-slot Warehouse",50000);
    const auto stored=t::stock(world,city.warehouse);check(world.agriculture_farms().size()==5&&t::w::volume(stored.amounts)==32,"Full Warehouse control was not genuinely earned");
    t::refused(world,sim::demolish_building(city.warehouse),"Full32 Warehouse removal lost real Food");t::refused(world,sim::begin_agriculture_drain(city.warehouse),"Full32 Warehouse acquired unsupported evacuation");
    t::ticks(world,100);check(t::stock(world,city.warehouse)==stored,"Full Warehouse refusal changed held32 or typed receipt history");t::valid(world);(void)extra;
}
void actual_loaded_hunter_and_boat_survive_source_refusal_and_return(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::q::h::build(world,false);bool hunter=false,boat=false;
    sim::HuntingHunterId hunter_id{};sim::FishingBoatId boat_id{};std::uint64_t meat_before=0,fish_before=0;
    for(int n=0;n<30000&&(!hunter||!boat);++n){world.tick();
        if(!hunter)for(const auto& a:world.hunting_state()->hunters)if(a.cargo){hunter_id=a.id;for(const auto& r:world.hunting_state()->tents)if(r.tent_id==a.tent_id)meat_before=r.unloaded;t::refused(world,sim::demolish_building(city.tent),"Loaded Hunter or exclusive prey identity was erased");t::refused(world,sim::begin_agriculture_drain(city.tent),"Tent gained unsafe source drainage");hunter=true;break;}
        if(!boat)for(const auto& a:world.fishing_state()->boats)if(a.cargo){boat_id=a.id;for(const auto& r:world.fishing_state()->quays)if(r.quay_id==a.quay_id)fish_before=r.unloaded;t::refused(world,sim::demolish_building(city.quay),"Loaded Boat or dock reservation was erased");t::refused(world,sim::begin_agriculture_drain(city.quay),"Quay gained unsafe source drainage");boat=true;break;}}
    check(hunter&&boat,"Actual loaded Hunter/Boat refusal controls were not earned");t::operate(world,city.tent,false);t::operate(world,city.quay,false);
    auto restored=t::json_roundtrip(fixture,world,"paused-loaded-source52.json");for(int n=0;n<20;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Paused Boat/Hunter cargo or physical return changed after JSON restore");}t::valid(world);
    // Inherited28/29 unloading requires an eligible source. Paused actors retain
    // their cargo; resume the real source instead of inventing a free unload.
    t::operate(world,city.tent,true);t::operate(world,city.quay,true);
    t::until(world,[&]{const bool old_hunter=std::any_of(world.hunting_state()->hunters.begin(),world.hunting_state()->hunters.end(),[&](const auto& a){return a.id==hunter_id;});const bool old_boat=std::any_of(world.fishing_state()->boats.begin(),world.fishing_state()->boats.end(),[&](const auto& a){return a.id==boat_id;});std::uint64_t meat=0,fish=0;for(const auto& r:world.hunting_state()->tents)if(r.tent_id==city.tent)meat=r.unloaded;for(const auto& r:world.fishing_state()->quays)if(r.quay_id==city.quay)fish=r.unloaded;return !old_hunter&&!old_boat&&meat>meat_before&&fish>fish_before;},"Original loaded Boat/Hunter IDs did not physically unload and retire after actual source resume");
    const auto snap=world.snapshot();check(fixture.restore(snap).snapshot()==snap,"Retained Boat/Hunter lifetimes failed exact restore");t::valid(world);
}
}
int main(){try{begun_empty_assignment_returns_without_first_work_or_harvest();paid_source_road_cut_preserves_pending_partial_work_then_repair_finishes_once();unrelated_real_grower_claims_a_drained_plant_footprint_and_begun_edge();finite_journal_and_hostile_terminal_histories_fail_atomically();closed_zero_work_leaf_cannot_move_to_an_immutable_blocked_source_cell();loaded_food_and_working_or_stocked_owners_remain_exact_refusals();actually_full_shared_warehouse_refuses_without_stock_or_history_loss();actual_loaded_hunter_and_boat_survive_source_refusal_and_return();std::cout<<"Agriculture reconfiguration begun-edge, bounded hostile histories and actual loaded-source refusals passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
