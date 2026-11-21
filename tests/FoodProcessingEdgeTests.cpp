#include "FoodProcessingFixture.h"

#include <iostream>

namespace {
namespace t=oe_food_processing_fixture;
namespace sim=openemperor::simulation;
using t::check;
using Phase=sim::OriginalGoodsTransferPhase;

const sim::ProcessedFoodTrip& trip(const sim::World& world,sim::CourierId courier) {
    for(const auto& row:world.processed_food_state()->trips)if(row.transfer.courier_id==courier)return row;
    throw std::runtime_error("A real meal courier lost its sole full-width transaction");
}
sim::CourierId channel(const sim::World& world,sim::BuildingId owner,sim::CourierRole role) {
    for(const auto& row:world.couriers())if(row.owner==owner&&row.role==role)return row.id;
    throw std::runtime_error("A paid processor lost its counted physical courier");
}
int cargo(const sim::ProcessedFoodTrip& row) {
    int result=0;for(std::size_t g=0;g<12;++g)result+=row.loaded[g]-row.delivered[g]-row.returned[g];return result;
}

void raw_deadline_selection_respects_actual_reserved_house_meal_room() {
    t::Fixture fixture;auto world=fixture.world();
    for(int x=89;x<=132;++x)t::f::buy(world,{sim::CommandType::PlaceRoad,{x,109}});
    const std::array homes{t::purchase(world,sim::CommandType::PlaceHousehold,{90,107}),
        t::purchase(world,sim::CommandType::PlaceHousehold,{94,107})};
    const auto farm=t::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    t::crop(world,farm,0,{93,110});t::crop(world,farm,1,{93,111});
    const auto market=t::purchase(world,sim::CommandType::PlaceMarket,{97,110});
    t::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{101,110});
    t::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
    t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});
    const auto mill=t::purchase(world,sim::CommandType::PlaceOriginalMill,{119,110});
    const auto shop=t::purchase(world,sim::CommandType::PlaceOriginalFoodShop,{98,110});
    t::f::buy(world,sim::set_building_operation(mill,false));
    t::f::buy(world,sim::set_building_operation(shop,false));
    const auto actor=channel(world,market,sim::CourierRole::OriginalFoodToHouse);
    const auto raw=[&](sim::BuildingId house){int result=0;for(std::size_t g=0;g<7;++g)result+=t::stock(world,house).amounts[g];return result;};
    t::until(world,[&]{return raw(homes[0])==4&&raw(homes[1])==4&&t::volume(t::stock(world,market).amounts)>0&&
        world.courier(actor).phase==sim::CourierPhase::IdleAtWorkshop;},
        "Raw-capacity control lacks two actually filled Houses and an idle stocked Distributor");
    check(world.courier_dispatch_status(actor).status!=sim::CourierDispatchStatus::Ready,
        "Deadline selector reported a raw dispatch into House capacity reserved for meals");
    t::until(world,[&]{return world.courier(actor).phase==sim::CourierPhase::ToWarehouse;},
        "Real sole House consumption never reopened a feasible raw Distributor departure");
    const auto& load=trip(world,actor);int pending=0;for(std::size_t g=0;g<7;++g)pending+=load.loaded[g];
    check(pending>0&&raw(load.target)+pending<=4,
        "Deadline Distributor failed to reserve actual raw capacity after House consumption");t::valid(world);
}

void real_incoming_meals_cover_the_same_food_deadline_as_raw_arrivals() {
    t::Fixture fixture(64);auto world=fixture.world();
    for(int x=89;x<=132;++x)t::f::buy(world,{sim::CommandType::PlaceRoad,{x,109}});
    const auto house=t::purchase(world,sim::CommandType::PlaceHousehold,{90,107});
    const auto farm=t::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    t::crop(world,farm,0,{93,110});t::crop(world,farm,1,{93,111});
    const auto market=t::purchase(world,sim::CommandType::PlaceMarket,{97,110});
    t::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{101,110});
    t::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
    t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});
    t::purchase(world,sim::CommandType::PlaceOriginalMill,{119,110});
    const auto shop=t::purchase(world,sim::CommandType::PlaceOriginalFoodShop,{98,110});
    const auto raw_actor=channel(world,market,sim::CourierRole::OriginalFoodToHouse);
    const auto meal_actor=channel(world,shop,sim::CourierRole::OriginalMealsToHouse);
    // Keep actual output at its Mill while the real raw route fills both the
    // House and Market. Then let the sole sink empty the House before reopening
    // the physical meal path. No inventory or movement state is injected.
    t::f::buy(world,sim::set_building_operation(shop,false));
    t::until(world,[&]{return t::meals_produced(world)>0&&t::volume(t::stock(world,market).amounts)>0;},
        "Shared forecast control lacks actual processor output and independently stocked raw Market");
    t::f::buy(world,sim::set_building_operation(market,false));
    t::until(world,[&]{return !t::volume(t::stock(world,house).amounts)&&world.courier(raw_actor).phase==sim::CourierPhase::IdleAtWorkshop;},
        "Sole real House consumption never depleted its retained raw portions");
    t::f::buy(world,sim::set_building_operation(shop,true));
    t::until(world,[&]{
        const auto& raw=world.courier(raw_actor);const auto& meal=world.courier(meal_actor);
        if(raw.phase!=sim::CourierPhase::IdleAtWorkshop||t::volume(t::stock(world,market).amounts)<=0||
           meal.phase!=sim::CourierPhase::ToWarehouse||meal.route_pending||!meal.edge_progress||
           meal.path_vertex+1>=meal.path.size()||meal.target!=house||world.original_food_cargo(meal_actor)<=0)return false;
        int held_raw=0;for(std::size_t g=0;g<7;++g)held_raw+=t::stock(world,house).amounts[g];
        if(held_raw>=4||t::volume(t::stock(world,house).amounts)+world.original_food_cargo(meal_actor)>=8)return false;
        const auto route=std::find_if(raw.dynamic_target_routes.begin(),raw.dynamic_target_routes.end(),
            [&](const auto& entry){return entry.first==house;});
        if(route==raw.dynamic_target_routes.end()||!route->second||route->second->size()<2)return false;
        const auto incoming_movement=5*(meal.path.size()-1-meal.path_vertex)-static_cast<std::size_t>(meal.edge_progress);
        const auto raw_movement=5*(route->second->size()-1);
        return incoming_movement<=raw_movement;
    },"Shared Food forecast control lacks a real earlier meal arrival and an otherwise feasible raw Distributor");
    t::f::buy(world,sim::set_building_operation(market,true));
    check(world.courier_dispatch_status(raw_actor).status==sim::CourierDispatchStatus::NoTarget,
        "Raw urgency ignored the same House's usable actual incoming meal portions");
    const auto received=t::stock(world,house).received;
    t::until(world,[&]{for(std::size_t g=7;g<12;++g)if(t::stock(world,house).received[g]>received[g])return true;return false;},
        "Forecast meal never completed its actual occupied-House arrival");t::valid(world);
}

void actual_zero_mill_and_shop_staff_never_grant_work_or_house_meals() {
    t::Fixture fixture(16);auto world=fixture.world();
    for(int x=89;x<=132;++x)t::f::buy(world,{sim::CommandType::PlaceRoad,{x,109}});
    t::purchase(world,sim::CommandType::PlaceHousehold,{90,107});
    const auto farm=t::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    t::crop(world,farm,0,{93,110});t::crop(world,farm,1,{93,111});
    const auto market=t::purchase(world,sim::CommandType::PlaceMarket,{97,110});
    t::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{101,110});
    t::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
    t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});
    const auto mill=t::purchase(world,sim::CommandType::PlaceOriginalMill,{119,110});
    const auto shop=t::purchase(world,sim::CommandType::PlaceOriginalFoodShop,{98,110});
    t::f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High));
    t::f::buy(world,sim::set_building_workforce_priority(mill,sim::WorkforcePriority::Low));
    t::f::buy(world,sim::set_building_workforce_priority(shop,sim::WorkforcePriority::Low));
    t::until(world,[&]{return t::stock(world,mill).amounts[0]>0&&t::stock(world,mill).amounts[1]>0;},
        "Zero-staff Mill control lacks two genuine physical producer arrivals");
    check(world.workers_assigned(mill)==0&&!t::mill(world,mill).started&&!t::meals_produced(world),
        "An unstaffed Mill committed ingredients or fabricated processing work/output");
    const auto ingredients=t::stock(world,mill);t::ticks(world,40);
    check(!t::mill(world,mill).started&&t::stock(world,mill).consumed==ingredients.consumed,
        "Unstaffed waiting consumed physical ingredients");
    t::f::buy(world,sim::set_building_workforce_priority(mill,sim::WorkforcePriority::High));
    t::f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::Low));
    t::until(world,[&]{return t::meals_produced(world)>0;},"Actually staffed Mill failed its already delivered finite recipe");
    check(world.workers_assigned(mill)==16&&world.workers_assigned(shop)==0,
        "Scarce workforce did not establish a real fully staffed Mill and zero-staff FoodShop");
    t::until(world,[&]{int meals=0;for(std::size_t g=7;g<12;++g)meals+=t::stock(world,shop).amounts[g];return meals>0;},
        "Mill meals never physically arrived at the unstaffed FoodShop");
    t::ticks(world,100);check(!t::meals_consumed(world)&&world.food_processing_state()->houses.empty(),
        "Unstaffed FoodShop became an actual House receipt or consumption");
    t::f::buy(world,sim::set_building_workforce_priority(shop,sim::WorkforcePriority::High));
    t::f::buy(world,sim::set_building_workforce_priority(mill,sim::WorkforcePriority::Low));
    t::until(world,[&]{return t::meals_consumed(world)>0;},"Real staffed FoodShop failed physical House delivery and sole meal consumption");
    check(world.food_processing_state()->shops.front().market_id==market,"Scarce-worker control changed its paid parent Market");t::valid(world);
}

void cut_loaded_meal_edge_restore_and_paused_source_return_keep_the_exact_grade() {
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    const auto actor=channel(world,city.mill,sim::CourierRole::OriginalMealsToShop);
    t::until(world,[&]{const auto& c=world.courier(actor);return c.phase==sim::CourierPhase::ToWarehouse&&c.edge_progress>0&&
        world.original_food_cargo(actor)>0&&c.path.size()>c.path_vertex+2;},"No actual loaded Mill-to-Shop movement for route-cut control");
    const auto begun=world.courier(actor);const auto loaded=trip(world,actor);const auto delivered=t::stock(world,city.shop).received;
    std::optional<sim::Cell> cut;
    for(std::size_t index=begun.path_vertex+2;index<begun.path.size();++index)if(world.validate({sim::CommandType::RemoveRoad,begun.path[index]}).accepted){cut=begun.path[index];break;}
    check(cut.has_value(),"Loaded meal route lacks an independently removable future road");
    t::f::buy(world,{sim::CommandType::RemoveRoad,*cut});
    check(world.courier(actor).edge_progress==begun.edge_progress,"Meal road cut rewound a genuinely begun edge");
    t::ticks(world,100);
    check(cargo(trip(world,actor))==cargo(loaded)&&trip(world,actor).loaded==loaded.loaded&&
        t::stock(world,city.shop).received==delivered,"Blocked physical meals changed grade, disappeared or falsely arrived at FoodShop");
    auto restored=fixture.restore(world.snapshot());
    for(int tick=0;tick<50;++tick){world.tick();restored.tick();}
    check(world.canonical_state()==restored.canonical_state(),"Blocked graded meal cargo diverged after source-bound restore");
    t::f::buy(world,sim::set_building_operation(city.mill,false));const auto before=t::stock(world,city.mill);
    t::until(world,[&]{return trip(world,actor).phase==Phase::Returned;},"Paused Mill failed to receive its genuine cancelled meal return");
    const auto& returned=trip(world,actor);const auto& settled=t::stock(world,city.mill);
    check(returned.loaded==loaded.loaded&&returned.returned==loaded.loaded&&returned.delivered==sim::ProcessedFoodAmounts{}&&
        t::stock(world,city.shop).received==delivered,"Cancelled physical meal return discarded or regraded its carried portions");
    for(std::size_t g=7;g<12;++g)check(settled.amounts[g]==before.amounts[g]+loaded.loaded[g]&&
        settled.returned[g]==before.returned[g]+static_cast<std::uint64_t>(loaded.loaded[g]),
        "Source return failed to settle the exact original meal grade once");
    t::f::buy(world,{sim::CommandType::PlaceRoad,*cut});t::valid(world);
}

void full_shared_mill_preserves_output_and_never_reprocesses_without_input() {
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    t::f::buy(world,sim::set_building_operation(city.shop,false));
    t::until(world,[&]{const auto& m=t::mill(world,city.mill);const auto& stock=t::stock(world,city.mill);unsigned distinct=0;
        for(std::size_t g=0;g<7;++g)distinct+=stock.amounts[g]>0;
        return t::volume(stock.amounts)==16&&!m.active&&distinct<m.requested_grade;},
        "Unavailable FoodShop did not produce a genuine finite full-Mill backpressure state");
    const auto state=t::mill(world,city.mill);const auto stock=t::stock(world,city.mill);const auto produced=t::meals_produced(world);
    t::ticks(world,100);
    check(t::mill(world,city.mill)==state&&t::stock(world,city.mill)==stock&&t::meals_produced(world)==produced,
        "Full shared Mill bins consumed inputs again, lost output or bypassed finite capacity");
    t::f::buy(world,sim::set_building_operation(city.shop,true));
    t::until(world,[&]{return t::meals_consumed(world)>0;},"Reopened real FoodShop failed to drain preserved output into actual House consumption");t::valid(world);
}

void natural_fire_freezes_a_real_committed_batch_without_redebit() {
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world,false);
    t::until(world,[&]{return t::mill(world,city.mill).active.has_value();},"Fire control never committed actual physically arrived recipe inputs");
    t::f::buy(world,sim::set_building_operation(city.mill,false));
    t::until(world,[&]{return world.building_on_fire(city.mill);},"Unprotected Mill did not enter a genuine natural fire");
    const auto committed=t::mill(world,city.mill);const auto consumed=t::stock(world,city.mill).consumed;
    check(committed.active.has_value(),"Paused batch vanished before actual fire");
    t::f::buy(world,sim::set_building_operation(city.mill,true));t::ticks(world,20);
    check(world.building_on_fire(city.mill)&&t::mill(world,city.mill)==committed&&t::stock(world,city.mill).consumed==consumed,
        "Burning Mill advanced paused work, recreated its batch or debited committed inputs twice");
    auto restored=fixture.restore(world.snapshot());check(restored.canonical_state()==world.canonical_state(),"Burning committed batch changed after exact restore");
    t::until(world,[&]{return t::mill(world,city.mill).completed>committed.completed;},"Recovered Mill never completed its retained real recipe");
    const auto& receipt=*t::mill(world,city.mill).last_receipt;
    check(receipt.batch.sequence==committed.active->sequence&&receipt.batch.ingredients==committed.active->ingredients&&
        t::stock(world,city.mill).consumed==consumed,"Post-fire completion lost batch identity or consumed its ingredients again");t::valid(world);
}

void a_cancelled_begun_meal_trip_preserves_valid_first_arrivals_below_a_full_visit_rate() {
    t::Fixture fixture;auto world=fixture.world();
    // Workforce is earned in a separate paid road component. It supplies the
    // shared pool while the actual Shop has no reachable House recipient.
    for(int x=89;x<=132;++x)t::f::buy(world,{sim::CommandType::PlaceRoad,{x,72}});
    std::vector<sim::BuildingId> distant_homes;
    for(int x=90;x<126;x+=3)distant_homes.push_back(t::purchase(world,sim::CommandType::PlaceHousehold,{x,70}));
    t::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{101,73});
    t::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{105,73});
    for(int x=89;x<=132;++x)t::f::buy(world,{sim::CommandType::PlaceRoad,{x,109}});
    const auto farm=t::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    t::crop(world,farm,0,{93,110});
    const auto market=t::purchase(world,sim::CommandType::PlaceMarket,{97,110});
    t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});
    t::purchase(world,sim::CommandType::PlaceFireWatch,{112,110});
    const auto mill=t::purchase(world,sim::CommandType::PlaceOriginalMill,{119,110});
    const auto shop=t::purchase(world,sim::CommandType::PlaceOriginalFoodShop,{98,110});
    t::f::buy(world,sim::set_mill_recipe(mill,1));
    const auto actor=channel(world,shop,sim::CourierRole::OriginalMealsToHouse);
    t::until(world,[&]{return t::stock(world,shop).amounts[7]>=8;},"No real buffered meals for cancelled-trip publication control");
    for(const auto home:distant_homes)check(!world.find_building_route(shop,home),"Isolated workforce accidentally gave the Shop an earlier real visit");
    check(world.food_processing_state()->houses.empty(),"Buffered Shop gained an arrival without a reachable paid House");
    for(const auto owner:{shop,farm,mill,market})t::f::buy(world,sim::set_building_operation(owner,false));
    const auto a=t::purchase(world,sim::CommandType::PlaceHousehold,{99,107});
    const auto b=t::purchase(world,sim::CommandType::PlaceHousehold,{96,107});
    const auto occupied=[&](sim::BuildingId id){
        for(const auto& row:world.original_economy_state()->houses) {
            if(row.building_id==id)return row.house.occupants>0;
        }
        return false;
    };
    t::until(world,[&]{return occupied(a)&&occupied(b);},"New real near Houses never became occupied");
    const auto route_a=world.find_building_route(shop,a),route_b=world.find_building_route(shop,b);
    check(route_a&&route_b&&route_a->size()==3&&route_b->size()==4,"Paid cancellation geometry lost its actual two/three-edge routes");
    t::f::buy(world,sim::set_building_operation(shop,true));
    t::until(world,[&]{return !world.food_processing_state()->houses.empty();},"First actual near meal arrival absent");
    const auto first=world.food_processing_state()->houses.front();
    check(first.house_id==a&&first.first_receipt.transfer.courier_id==actor&&first.first_receipt.transfer.sequence==1&&
        first.first_receipt.accepted[7]==4,"First real near visit lost source, grade, sequence or accepted cargo");
    const auto first_tick=first.first_receipt.tick;
    t::until(world,[&]{return world.courier(actor).phase==sim::CourierPhase::IdleAtWorkshop;},"First actual delivery did not physically return");
    check(world.ticks()==first_tick+10,"First real two-edge return changed its movement clock");
    // Keep the later successful recipient distinct. These ordinary cuts remove
    // A's two access cells only after its actor has really returned; B remains
    // reachable and all actual/begun actor claims retain normal admission.
    t::f::buy(world,sim::set_building_operation(shop,false));
    t::f::buy(world,{sim::CommandType::RemoveRoad,{99,109}});
    t::f::buy(world,{sim::CommandType::RemoveRoad,{100,109}});
    check(!world.find_building_route(shop,a)&&world.find_building_route(shop,b),"Paid access changes did not preserve only the unserved B route");
    const auto returned_before=t::stock(world,shop).returned[7];
    t::f::buy(world,sim::set_building_operation(shop,true));
    t::until(world,[&]{const auto& c=world.courier(actor);return c.phase==sim::CourierPhase::ToWarehouse&&c.edge_progress==1&&
        trip(world,actor).transfer.sequence==2;},"Actual second loaded trip never began its first edge");
    check(world.ticks()==first_tick+11&&cargo(trip(world,actor))==4&&trip(world,actor).loaded[7]==4,
        "Second trip failed to retain four actual meals at its first begun edge");
    t::f::buy(world,sim::set_building_operation(shop,false));
    const auto amount_after_cancel=t::stock(world,shop).amounts[7];
    check(world.courier(actor).phase==sim::CourierPhase::Returning&&world.courier(actor).edge_progress==1&&cargo(trip(world,actor))==4,
        "Pausing a real begun trip rewound movement, discarded cargo or granted an immediate return");
    t::until(world,[&]{return world.courier(actor).phase==sim::CourierPhase::IdleAtWorkshop;},"Cancelled second trip did not drain its begun edge and return");
    check(world.ticks()==first_tick+20&&trip(world,actor).transfer.sequence==2&&trip(world,actor).phase==Phase::Returned&&
        trip(world,actor).returned[7]==4&&!cargo(trip(world,actor))&&
        t::stock(world,shop).amounts[7]==amount_after_cancel+4&&t::stock(world,shop).returned[7]==returned_before+4,
        "Cancelled trip lost its sequence or failed to settle the exact four meals once after actual return");
    t::f::buy(world,sim::set_building_operation(shop,true));
    const auto received_b=[&]{for(const auto& row:world.food_processing_state()->houses)if(row.house_id==b)return true;return false;};
    t::until(world,received_b,"Actual third trip never reached its still-unserved House");
    const auto at=std::find_if(world.food_processing_state()->houses.begin(),world.food_processing_state()->houses.end(),
        [&](const auto& row){return row.house_id==b;});
    check(at!=world.food_processing_state()->houses.end()&&at->first_receipt.transfer.courier_id==actor&&
        at->first_receipt.transfer.sequence==3&&at->first_receipt.tick-first_tick==35&&at->first_receipt.accepted[7]==4&&
        t::stock(world,shop).returned[7]==returned_before+4,"Real cancellation no longer establishes valid35-tick first arrivals across sequence1/3");
    t::valid(world); // Includes all five shared World invariants and publication.
    // Exercise the exact manual/F5 save backend; no SDL key or native-window
    // acceptance is inferred from this core persistence regression.
    const auto snapshot=world.snapshot();const auto path=fixture.root.path/"begun-cancel-first-arrivals50.json";
    t::save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=t::f::read_text(path);const auto document=t::save::read_save(path);auto restored=fixture.restore(document);
    check(document.source_schema_version==50&&document.world==snapshot&&restored.snapshot()==snapshot,
        "Manual save/read/restore rejected or changed genuine35-tick first-arrival provenance");
    const auto roundtrip=fixture.root.path/"begun-cancel-first-arrivals50.roundtrip.json";
    t::save::write_save(roundtrip,fixture.document(restored),fixture.root.data,fixture.mask());
    check(t::f::read_text(roundtrip)==bytes,"Canonical manual-save roundtrip changed valid cancelled-trip receipts");
    for(int n=0;n<30;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Restored real cancelled-trip continuation diverged");}
    check(t::stock(world,shop).returned[7]==returned_before+4,"Later successful movement settled the cancelled cargo twice");t::valid(world);
}

template<class F>void run(const char* name,F action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}}
}
int main(){try{run("raw deadline room",raw_deadline_selection_respects_actual_reserved_house_meal_room);
    run("shared incoming meals",real_incoming_meals_cover_the_same_food_deadline_as_raw_arrivals);
    run("zero staffing",actual_zero_mill_and_shop_staff_never_grant_work_or_house_meals);
    run("loaded edge/cancel",cut_loaded_meal_edge_restore_and_paused_source_return_keep_the_exact_grade);
    run("full Mill",full_shared_mill_preserves_output_and_never_reprocesses_without_input);
    run("natural fire",natural_fire_freezes_a_real_committed_batch_without_redebit);
    run("begun cancellation/publication",a_cancelled_begun_meal_trip_preserves_valid_first_arrivals_below_a_full_visit_rate);
    std::cout<<"Food processing actual raw room, shared meal forecast, staffing, finite bins, loaded cut/return natural fire and begun-cancellation publication tests passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
