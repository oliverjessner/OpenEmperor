// Rule18 physical Food behavior, earned through paid commands and real ticks.
#include "OriginalFinanceFixture.h"
#include "simulation/OriginalTerrainFacts.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace {
namespace f=oe_influence_fixture;
namespace q=oe_finance_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using f::check;
sim::Cell site(sim::Cell cell) {return {cell.x+20,cell.y+20};}
struct Fixture {
    f::Root root;
    std::shared_ptr<const sim::MapPermissions> policy=std::make_shared<const sim::MapPermissions>(228,228,3,
        std::vector<sim::MapCellPermission>(228*228,{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None}),
        std::vector<sim::FixedGatePassage>{});
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain=std::make_shared<const sim::OriginalTerrainFacts>(
        std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count,0x80U),std::string(64,'a'));
    Fixture() {
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",oe_production_fixture::authored_model_text(37));
        model=f::save::prepare_original_production_rules_model(root.data);
    }
    sim::World world() const {return sim::World::create_original_deadline_food_dispatch(policy,model,terrain);}
    sim::World old() const {return sim::World::create_original_deadline_service_dispatch(policy,model,terrain);}
    sim::World restore(const sim::World& w) const {return sim::World::restore_original_deadline_food_dispatch(w.snapshot(),policy,model,terrain);}
};
sim::BuildingId buy(sim::World& w,sim::CommandType type,sim::Cell cell) {
    cell=site(cell);
    f::buy(w,{type,cell});return *w.building_owner_at(cell);
}
sim::CourierId actor(const sim::World& w,sim::BuildingId owner) {
    for(const auto& c:w.couriers())if(c.owner==owner&&c.role==sim::CourierRole::OriginalFoodToHouse)return c.id;
    throw std::runtime_error("Paid Market lost its actual Food actor");
}
const sim::OriginalFoodStock& food(const sim::World& w,sim::BuildingId owner) {
    for(const auto& s:w.original_production_state()->food.stocks)if(s.building_id==owner)return s;
    throw std::runtime_error("Paid Food owner lost its sole physical stock");
}
const sim::OriginalFoodTrip* trip(const sim::World& w,sim::CourierId id) {
    for(const auto& t:w.original_production_state()->food.trips)if(t.transfer.courier_id==id)return &t;
    return nullptr;
}
int volume(const sim::OriginalFoodAmounts& a) {return a[0]+a[1];}
int accounted_volume(const sim::World& w,sim::BuildingId owner) {
    auto total=volume(food(w,owner).amounts);
    for(const auto& t:w.original_production_state()->food.trips) {
        const auto cargo=volume(t.loaded)-volume(t.delivered)-volume(t.returned);
        if(t.source==owner||(t.target==owner&&t.phase==sim::OriginalGoodsTransferPhase::Loaded))total+=cargo;
    }
    return total;
}
void invariant(const sim::World& w) {
    check(w.population_valid()&&w.city_economy_valid()&&w.production_balance_valid()&&w.navigation_valid()&&w.fire_state_valid(),
        "Paid deadline-Food invariant failed");
    q::accounting(w);
    std::uint64_t harvest=0,held=0,consumed=0;
    for(const auto& farm:w.original_production_state()->food.farms)harvest+=farm.harvested;
    for(const auto& s:w.original_production_state()->food.stocks) {
        const auto cap=w.building(s.building_id).kind==sim::Object::Market ? 16:8;
        check(volume(s.amounts)>=0&&volume(s.amounts)<=cap&&accounted_volume(w,s.building_id)<=cap,"Food physical capacity exceeded");
        held+=static_cast<std::uint64_t>(volume(s.amounts));consumed+=s.consumed[0]+s.consumed[1];
    }
    for(const auto& t:w.original_production_state()->food.trips)held+=static_cast<std::uint64_t>(volume(t.loaded)-volume(t.delivered)-volume(t.returned));
    check(harvest==held+consumed,"Deadline-Food lost actual harvested portions");
}
void through(sim::World& w,std::uint64_t tick) {check(w.ticks()<=tick,"Paid setup passed its intended timing boundary");while(w.ticks()<tick)w.tick();invariant(w);}
template<class P>void until(sim::World& w,P done,const char* why,int limit=6000) {
    for(int i=0;i<limit&&!done();++i)w.tick();check(done(),why);invariant(w);
}
void road(sim::World& w,int x,int y) {const auto cell=site({x,y});if(w.object_at(cell)!=sim::Object::Road)f::buy(w,{sim::CommandType::PlaceRoad,cell});}
void lines(sim::World& w) {for(int x=0;x<=24;++x)for(int y:{2,8})road(w,x,y);}
void connect(sim::World& w) {for(int y=3;y<8;++y)road(w,10,y);}
sim::BuildingId farm(sim::World& w,int x) {
    const auto id=buy(w,sim::CommandType::PlaceOriginalFoodFarm,{x,9});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,site({x+3,9})};field.building_id=id;f::buy(w,field);return id;
}
struct Scene {std::array<sim::BuildingId,3> homes{};sim::BuildingId source{},market{},second{};sim::CourierId a{},b{};};
Scene scene(sim::World& w,bool two=false,bool fill=true) {
    lines(w);Scene s;
    s.homes[0]=buy(w,sim::CommandType::PlaceHousehold,{0,0});
    s.homes[1]=buy(w,sim::CommandType::PlaceHousehold,{10,0});
    s.homes[2]=buy(w,sim::CommandType::PlaceHousehold,{19,0});
    s.source=farm(w,5);if(two)(void)farm(w,0);
    s.market=buy(w,sim::CommandType::PlaceMarket,{10,9});s.a=actor(w,s.market);
    if(two){s.second=buy(w,sim::CommandType::PlaceMarket,{12,9});s.b=actor(w,s.second);}
    if(!fill)return s;
    until(w,[&]{return volume(food(w,s.market).amounts)==16&&(!two||volume(food(w,s.second).amounts)==16);},
        "Disconnected paid Farm supply did not fill the actual Markets");
    for(const auto h:s.homes)check(q::house(w,h).occupants>0&&!volume(food(w,h).amounts)&&q::house(w,h).food_quality==0,
        "Disconnected House received unearned Food or population was injected");
    check(!trip(w,s.a)&&(!two||!trip(w,s.b)),"Disconnected Market dispatched an imaginary House trip");return s;
}
void exact18_and_paid_nearest_empty_historical17() {
    Fixture fxt;auto w=fxt.world();check(w.rule_version()==18&&w.original_deadline_food_dispatch_enabled()&&
        !w.original_deadline_service_dispatch_enabled()&&!w.original_deadline_fire_dispatch_enabled()&&w.original_worker_upkeep_interval()==816,
        "Rule18 widened historical exact predicates or upkeep");
    auto old=fxt.old();const auto s=scene(w),o=scene(old);
    check(w.ticks()==old.ticks()&&food(w,s.market).amounts==food(old,o.market).amounts,"Farm-to-Market behavior changed under18");
    connect(w);connect(old);
    check(w.courier_dispatch_status(s.a).selected_target==s.homes[1]&&old.courier_dispatch_status(o.a).selected_target==o.homes[0],
        "Rule18 failed the closest actual empty rescue, or historical17 changed its cyclic first target");
    const auto before=w.snapshot();const auto canonical=w.canonical_state();const auto caches=w.couriers();
    for(int repeat=0;repeat<8;++repeat) {
        check(w.courier_dispatch_status(s.a).selected_target==s.homes[1],"Pure Food status changed its own choice");
        check(w.snapshot()==before&&w.canonical_state()==canonical,"Food query changed authority/canonical state");
        for(const auto& c:caches)check(w.courier(c.id).dynamic_target_routes==c.dynamic_target_routes&&w.courier(c.id).cached_revision==c.cached_revision,
            "Food query rebuilt or changed a prepared route cache");
    }
    w.tick();old.tick();check(trip(w,s.a)&&trip(w,s.a)->target==s.homes[1]&&w.original_food_cargo(s.a)==4&&
        !volume(food(w,s.homes[1]).amounts),"Dispatch granted Food early or disagreed with completed-tick query");
    auto restored=fxt.restore(w);
    const auto eta=w.ticks()+(w.courier(s.a).path.size()-1-w.courier(s.a).path_vertex)*5-static_cast<std::uint64_t>(w.courier(s.a).edge_progress);
    while(w.ticks()<eta){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Paid Food arrival restore continuation differs");}
    check(volume(food(w,s.homes[1]).amounts)==4&&q::house(w,s.homes[1]).food_quality==80&&trip(w,s.a)->phase==sim::OriginalGoodsTransferPhase::Completed,
        "Actual paid Millet arrival failed sole stock/quality publication");
    while(w.courier(s.a).phase!=sim::CourierPhase::IdleAtWorkshop){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Physical Food return restore differs");}
    check(volume(food(w,s.homes[1]).amounts)==4&&!volume(food(w,s.homes[2]).amounts)&&
        w.courier_dispatch_status(s.a).selected_target==s.homes[2],
        "Deadline-Food refilled the closest stocked House while a reachable House remained empty");
    w.tick();check(trip(w,s.a)->target==s.homes[2],"Actual depletion-priority load differs from its pure query");invariant(w);invariant(old);
}
void paid_exact_consumption_boundary() {
    Fixture fxt;auto w=fxt.world();const auto s=scene(w);through(w,1632);connect(w);w.tick();
    check(trip(w,s.a)&&trip(w,s.a)->target==s.homes[1]&&w.courier(s.a).path.size()==9,
        "Paid exact-consumption setup lost its eight-edge departure");
    const auto missed=food(w,s.homes[1]).missed_demands;through(w,1671);
    check(!volume(food(w,s.homes[1]).amounts)&&q::house(w,s.homes[1]).food_quality==0,"Boundary shipment arrived early");
    w.tick();check(w.ticks()==1672&&volume(food(w,s.homes[1]).amounts)==3&&food(w,s.homes[1]).missed_demands==missed&&
        q::house(w,s.homes[1]).food_quality==80&&food(w,s.homes[1]).quality_witness->tick==1672,
        "Same-tick real arrival did not precede the1672 consumption sink");invariant(w);
}
void paid_simultaneous_loads_coalesce_real_quantity() {
    Fixture fxt;auto w=fxt.world();const auto s=scene(w,true);connect(w);const auto start=w.ticks();w.tick();
    check(trip(w,s.a)&&trip(w,s.b)&&trip(w,s.a)->target==s.homes[1]&&trip(w,s.b)->target!=s.homes[1],
        "Later same-tick Market ignored the earlier actual four-portion commitment");
    check(w.original_food_cargo(s.a)==4&&w.original_food_cargo(s.b)==4&&
        accounted_volume(w,s.homes[1])==4&&!volume(food(w,s.homes[1]).amounts),
        "Two physical loads overbooked capacity, invented receipt or vanished");
    auto restored=fxt.restore(w);
    while(w.ticks()<start+150){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Simultaneous paid Food replay differs");}
    check(food(w,s.homes[1]).received[1]>=4,"Coalesced actual shipment never physically arrived");invariant(w);
}
void paid_one_portion_incoming_does_not_promise_coverage() {
    Fixture fxt;auto w=fxt.world();auto s=scene(w,false,false);
    until(w,[&]{return volume(food(w,s.market).amounts)==1;},"Paid first Market did not earn its first single portion");
    f::buy(w,{sim::CommandType::RemoveRoad,site({10,8})});
    // Buy this independent actual supply after the cut. A second Farm that
    // had already loaded for the isolated first Market must physically wait
    // with that cargo; it cannot magically supply a different Market.
    // A paid bypass preserves the Farm-to-second-Market road component
    // without reopening the first Market's missing sole entrance at10,8.
    for(int x=9;x<=11;++x)road(w,x,7);
    (void)farm(w,0);s.second=buy(w,sim::CommandType::PlaceMarket,{12,9});s.b=actor(w,s.second);
    until(w,[&]{return volume(food(w,s.second).amounts)>=4;},"Other actual Farm/Market supply did not earn four portions");
    through(w,1632);check(volume(food(w,s.market).amounts)==1,"Disconnected Market fabricated additional actual Food");
    road(w,10,8);connect(w);w.tick();
    check(trip(w,s.a)&&volume(trip(w,s.a)->loaded)==1&&trip(w,s.a)->target==s.homes[1]&&
        trip(w,s.b)&&volume(trip(w,s.b)->loaded)==4&&trip(w,s.b)->target==s.homes[1]&&accounted_volume(w,s.homes[1])==5,
        "A real one-portion load falsely promised coverage after its consumption, or two actual loads overbooked capacity");
    const auto missed=food(w,s.homes[1]).missed_demands;through(w,1672);
    check(!volume(food(w,s.homes[1]).amounts)&&food(w,s.homes[1]).missed_demands==missed&&q::house(w,s.homes[1]).food_quality==0,
        "Single actual portion did not arrive then deplete at the exact consumption boundary");
    through(w,1682);check(volume(food(w,s.homes[1]).amounts)==4&&q::house(w,s.homes[1]).food_quality==80,
        "Second real load failed its later physical rescue");invariant(w);
}
void paid_pause_and_road_wait_keep_real_cargo() {
    Fixture fxt;
    for(const bool pause:{false,true}) {
        auto w=fxt.world();const auto s=scene(w);connect(w);w.tick();const auto moving=w.courier(s.a);const auto cargo=w.original_food_cargo(s.a);
        check(cargo==4&&moving.edge_progress>0,"Paid callback setup has no begun loaded edge");
        if(pause) {
            f::buy(w,sim::set_building_operation(s.market,false));
            check(trip(w,s.a)->phase==sim::OriginalGoodsTransferPhase::Returning&&w.original_food_cargo(s.a)==cargo,
                "Explicit Market pause granted target Food or erased return cargo");
        }else f::buy(w,{sim::CommandType::RemoveRoad,site({10,6})});
        check(w.courier(s.a).edge_progress==moving.edge_progress&&!volume(food(w,s.homes[1]).amounts),
            "Pause/cut changed a begun edge or granted Food");
        auto restored=fxt.restore(w);
        for(int i=0;i<15;++i){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Pause/cut continuation differs");}
        if(pause) {
            until(w,[&]{return w.courier(s.a).phase==sim::CourierPhase::IdleAtWorkshop;},"Paused actual Food cargo failed to return");
            check(trip(w,s.a)->phase==sim::OriginalGoodsTransferPhase::Returned&&volume(trip(w,s.a)->returned)==cargo&&
                !volume(food(w,s.homes[1]).amounts),"Paused shipment did not repatriate every actual portion");
        }else {
            check(w.courier(s.a).route_pending&&w.original_food_cargo(s.a)==cargo&&accounted_volume(w,s.homes[1])==4,
                "Road-waiting Food was counted as receipt or lost its real capacity reservation");
            road(w,10,6);until(w,[&]{return food(w,s.homes[1]).received[1]>0;},"Ordinary paid road repair failed actual delivery");
        }
        invariant(w);
    }
}
void paid_source_target_fire_refuses_then_returns() {
    Fixture fxt;auto w=fxt.world();const auto s=scene(w);through(w,1980);connect(w);w.tick();
    check(trip(w,s.a)&&trip(w,s.a)->target==s.homes[1]&&w.original_food_cargo(s.a)==4,"Late paid Food departure absent");
    through(w,2000);check(w.building_on_fire(s.market)&&w.building_on_fire(s.homes[1])&&
        w.workers_assigned(s.market)==0&&w.courier(s.a).phase==sim::CourierPhase::ToWarehouse&&w.original_food_cargo(s.a)==4,
        "Real source fire/workforce loss canceled begun Food movement");
    through(w,2020);check(trip(w,s.a)->phase==sim::OriginalGoodsTransferPhase::Returning&&volume(trip(w,s.a)->delivered)==0&&
        !volume(food(w,s.homes[1]).amounts)&&w.original_food_cargo(s.a)==4,
        "Burning target accepted Food or lost its physical refused cargo");
    through(w,2060);check(trip(w,s.a)->phase==sim::OriginalGoodsTransferPhase::Returned&&volume(trip(w,s.a)->returned)==4&&
        volume(food(w,s.market).amounts)==16&&!w.original_food_cargo(s.a),"Burning source did not accept its real physical return");invariant(w);
}
void paid_full_identity_cursor_survives_target_fire() {
    Fixture fxt;
    for(const bool successor:{false,true}) {
        auto w=successor?fxt.world():fxt.old();
        for(int x=0;x<=24;++x)road(w,x,8);
        for(int x:{0,1,10,11,19,20})road(w,x,2);
        const auto left=buy(w,sim::CommandType::PlaceHousehold,{0,0});through(w,600);
        const auto middle=buy(w,sim::CommandType::PlaceHousehold,{10,0});through(w,1000);
        const auto right=buy(w,sim::CommandType::PlaceHousehold,{19,0});through(w,1100);
        (void)farm(w,0);(void)farm(w,5);const auto market=buy(w,sim::CommandType::PlaceMarket,{10,9});const auto id=actor(w,market);connect(w);
        through(w,2600);
        check(!w.building_on_fire(left)&&w.building_on_fire(middle)&&!w.building_on_fire(right)&&
            w.courier(id).phase==sim::CourierPhase::IdleAtWorkshop&&w.courier(id).last_dispatched_target==middle&&
            !volume(food(w,left).amounts)&&!volume(food(w,right).amounts)&&volume(food(w,market).amounts)>0,
            "Paid filtered-cursor setup failed its staggered real ignition/stock history");
        for(int x=2;x<19;++x)road(w,x,2);
        check(w.courier_dispatch_status(id).selected_target==(successor?right:left),
            "Burning saved cursor reset18rotation or changed historical17filtering");
        w.tick();check(trip(w,id)&&trip(w,id)->target==(successor?right:left),"Actual paid cursor departure differs from pure query");invariant(w);
    }
}
}
int main() {try {
    exact18_and_paid_nearest_empty_historical17();paid_exact_consumption_boundary();paid_simultaneous_loads_coalesce_real_quantity();paid_one_portion_incoming_does_not_promise_coverage();
    paid_pause_and_road_wait_keep_real_cargo();paid_source_target_fire_refuses_then_returns();paid_full_identity_cursor_survives_target_fire();
    std::cout<<"Rule18 paid Food urgency, phase boundary, incoming quantities, fire/return/road callbacks and exact17 history PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"Deadline Food World FAIL: "<<e.what()<<'\n';return 1;}}
