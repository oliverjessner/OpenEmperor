#include "simulation/World.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace openemperor::simulation;
namespace {
void check(bool yes,const char* reason) { if (!yes) throw std::runtime_error(reason); }
World make(int width=11) {
    return World(width,1,std::vector<std::uint8_t>(static_cast<std::size_t>(width),1),
                 RulesProfile::ProductionV2);
}
void place(World& w,CommandType type,int x) {
    check(w.execute({type,{x,0}}).accepted,"placement rejected");
}
void step(World& w,int count=1) {
    for (int i=0;i<count;++i) { w.tick(); check(w.production_balance_valid(),"production invariant"); }
}
World connected() {
    auto w=make();
    place(w,CommandType::PlaceClaySource,0);
    place(w,CommandType::PlacePottery,5);
    place(w,CommandType::PlaceWarehouse,10);
    for (int x : {1,2,3,4,6,7,8,9}) place(w,CommandType::PlaceRoad,x);
    return w;
}
void placement_and_routes() {
    auto w=make();
    check(!w.execute({CommandType::PlaceWorkshop,{0,0}}).accepted,"v1 building in v2");
    check(!w.execute({CommandType::PlaceClaySource,{11,0}}).accepted,"outside accepted");
    place(w,CommandType::PlaceClaySource,0);
    place(w,CommandType::PlacePottery,5);
    place(w,CommandType::PlaceWarehouse,10);
    check(!w.execute({CommandType::PlacePottery,{3,0}}).accepted,"second pottery accepted");
    check(!w.execute({CommandType::PlaceRoad,{5,0}}).accepted,"building overwrite");
    check(w.building_owner_at({5,0})==BuildingId::Pottery,"unstable owner ID");
    check(w.building(BuildingId::ClaySource).id==BuildingId::ClaySource &&
          w.courier(CourierId::Pottery).owner==BuildingId::Pottery,"unstable entity IDs");
    for (int x : {1,2,3,4,6,7,8,9}) place(w,CommandType::PlaceRoad,x);
    check(w.find_route({0,0},{5,0}) && w.find_route({5,0},{10,0}),"independent routes missing");
    check(!w.find_route({0,0},{10,0}),"third building used as transit shortcut");
    auto v1=World(2,1,{1,1});
    check(!v1.execute({CommandType::PlaceClaySource,{0,0}}).accepted,"v2 building in v1");
    check(!v1.execute({static_cast<CommandType>(100),{0,0}}).accepted,"bad command accepted");
    auto no_pottery=make(5);
    place(no_pottery,CommandType::PlaceClaySource,0);
    place(no_pottery,CommandType::PlaceWarehouse,4);
    for (int x=1;x<4;++x) place(no_pottery,CommandType::PlaceRoad,x);
    step(no_pottery,1000);
    check(no_pottery.building(BuildingId::Warehouse).pottery_stock==0 &&
          no_pottery.courier(CourierId::Clay).cargo==0,
          "Clay was accepted by the Pottery-only warehouse");
}
void no_resources_and_recipe_timing() {
    auto w=make(3);
    place(w,CommandType::PlacePottery,2);
    step(w,500);
    check(w.pottery_completed_total()==0 && w.building(BuildingId::Pottery).active_recipe_clay==0,
          "pottery produced without Clay");
    auto a=make(3);
    place(a,CommandType::PlaceClaySource,0);
    place(a,CommandType::PlaceRoad,1);
    place(a,CommandType::PlacePottery,2);
    step(a,99);
    check(a.clay_extracted_total()==0,"early Clay extraction");
    step(a,21);
    check(a.building(BuildingId::Pottery).input_clay==1 &&
          a.building(BuildingId::Pottery).active_recipe_clay==0 &&
          a.pottery_completed_total()==0,"one Clay started recipe");
    step(a,99);
    check(a.building(BuildingId::Pottery).input_clay==2 &&
          a.building(BuildingId::Pottery).active_recipe_clay==0,"second Clay arrival timing");
    step(a);
    check(a.building(BuildingId::Pottery).input_clay==0 &&
          a.building(BuildingId::Pottery).active_recipe_clay==2 &&
          a.building(BuildingId::Pottery).progress==0,"start tick consumed progress or lost reserved Clay");
    const auto frozen=a.canonical_state();
    TickDriver driver;
    driver.toggle_pause();
    driver.update(20,a);
    check(a.canonical_state()==frozen,"pause changed active recipe");
    driver.step_once(a);
    check(a.building(BuildingId::Pottery).progress==1,"one paused step not one processing tick");
    step(a,148);
    check(a.pottery_completed_total()==0 && a.building(BuildingId::Pottery).progress==149,
          "recipe completed too early");
    step(a);
    check(a.pottery_completed_total()==1 && a.building(BuildingId::Pottery).output==1 &&
          a.building(BuildingId::Pottery).active_recipe_clay==0,"recipe did not finish once");
}
void two_gaps() {
    auto w=make();
    place(w,CommandType::PlaceClaySource,0);
    place(w,CommandType::PlacePottery,5);
    place(w,CommandType::PlaceWarehouse,10);
    for (int x : {1,2,4,6,7,9}) place(w,CommandType::PlaceRoad,x);
    step(w,300);
    check(w.building(BuildingId::ClaySource).output==3 &&
          w.building(BuildingId::Pottery).input_clay==0 &&
          w.pottery_completed_total()==0 && w.building(BuildingId::Warehouse).pottery_stock==0,
          "gap did not block raw-material chain");
    check(std::string(w.courier_blockage(CourierId::Clay))=="No road connection", "first gap reason");
    place(w,CommandType::PlaceRoad,3);
    step(w);
    check(w.courier(CourierId::Clay).cargo==3 &&
          w.building(BuildingId::Pottery).reserved_incoming==3 &&
          w.building(BuildingId::Pottery).input_clay==0,"Clay dispatch/reservation");
    step(w,49);
    check(w.building(BuildingId::Pottery).input_clay==3 &&
          w.building(BuildingId::Pottery).reserved_incoming==0 &&
          w.courier(CourierId::Clay).cargo==0,"Clay arrival/reservation release");
    step(w,151);
    check(w.pottery_completed_total()>=1 && w.building(BuildingId::Pottery).output>=1 &&
          w.building(BuildingId::Warehouse).pottery_stock==0,"Pottery before second gap closure");
    check(std::string(w.courier_blockage(CourierId::Pottery))=="No road connection", "second gap reason");
    place(w,CommandType::PlaceRoad,8);
    step(w);
    check(w.courier(CourierId::Pottery).cargo>=1 &&
          w.building(BuildingId::Warehouse).reserved_incoming>=1 &&
          w.building(BuildingId::Warehouse).pottery_stock==0,"Pottery dispatched without delivery");
    step(w,49);
    check(w.building(BuildingId::Warehouse).pottery_stock>=1 &&
          w.building(BuildingId::Warehouse).reserved_incoming==0,
          "Pottery did not arrive or reservation survived arrival");
    bool both=false,clay_returned=false,pottery_returned=false;
    for (int i=0;i<1800;++i) {
        step(w);
        both=both || (w.courier(CourierId::Clay).phase==CourierPhase::ToWarehouse &&
                      w.courier(CourierId::Pottery).phase==CourierPhase::ToWarehouse);
        clay_returned=clay_returned || w.courier(CourierId::Clay).phase==CourierPhase::Returning;
        pottery_returned=pottery_returned || w.courier(CourierId::Pottery).phase==CourierPhase::Returning;
    }
    check(both && clay_returned && pottery_returned,"independent couriers not active/returning");
    check(w.building(BuildingId::Warehouse).pottery_stock>=2,"no repeated deliveries");
}
void capacity_and_determinism() {
    auto blocked=make();
    place(blocked,CommandType::PlaceClaySource,0);
    place(blocked,CommandType::PlacePottery,5);
    place(blocked,CommandType::PlaceWarehouse,10);
    for (int x : {1,2,3,4,6,7,9}) place(blocked,CommandType::PlaceRoad,x);
    step(blocked,5000);
    check(blocked.building(BuildingId::Pottery).output==Rules::pottery_output_capacity &&
          blocked.building(BuildingId::Pottery).input_clay==Rules::pottery_input_capacity &&
          blocked.building(BuildingId::ClaySource).output==Rules::clay_output_capacity &&
          blocked.building(BuildingId::Warehouse).pottery_stock==0 &&
          blocked.building(BuildingId::Pottery).reserved_incoming==0 &&
          blocked.courier(CourierId::Clay).cargo==0,
          "blocked destination did not enforce all three buffer limits");
    check(std::string(blocked.courier_blockage(CourierId::Clay))=="Target buffer full",
          "full input status missing");
    const auto before=blocked.clay_extracted_total(),pots=blocked.pottery_completed_total();
    step(blocked,500);
    check(blocked.clay_extracted_total()==before && blocked.pottery_completed_total()==pots,
          "full input/output accumulated production backlog");
    place(blocked,CommandType::PlaceRoad,8);
    step(blocked,200);
    check(blocked.building(BuildingId::Warehouse).pottery_stock>0,
          "closing blocked output did not resume delivery");

    auto w=connected();
    for (int i=0;i<12000;++i) step(w);
    check(w.building(BuildingId::Warehouse).pottery_stock==Rules::warehouse_capacity,
          "warehouse failed to fill");
    check(w.building(BuildingId::Pottery).output==Rules::pottery_output_capacity,
          "pottery output did not back up");
    check(w.building(BuildingId::ClaySource).output==Rules::clay_output_capacity,
          "clay source did not back up");
    const auto extracted=w.clay_extracted_total(),completed=w.pottery_completed_total();
    step(w,500);
    check(w.clay_extracted_total()==extracted && w.pottery_completed_total()==completed,
          "full buffers generated backlog or lost goods");
    auto replay=[] {
        auto a=connected();
        step(a,421);
        const auto before=a.canonical_state();
        for (int i=0;i<50;++i) {
            (void)a.pottery_blockage();
            (void)a.courier_blockage(CourierId::Clay);
            (void)a.courier_blockage(CourierId::Pottery);
            (void)a.courier_position(CourierId::Clay);
        }
        check(a.canonical_state()==before,"HUD queries changed simulation");
        step(a,379);
        return a.canonical_state();
    };
    check(replay()==replay(),"complete replay differs");
    auto a=connected(),b=connected();
    TickDriver one,two;
    for (int i=0;i<500;++i) one.update(0.05,a);
    for (int i=0;i<1000;++i) two.update(0.025,b);
    check(a.ticks()==500 && b.ticks()==500 && a.canonical_state()==b.canonical_state(),
          "render-time partition changed complete production state");
}
}
int main() {
    try { placement_and_routes(); no_resources_and_recipe_timing(); two_gaps(); capacity_and_determinism(); }
    catch (const std::exception& error) { std::cerr<<"Production test failed: "<<error.what()<<'\n'; return 1; }
    std::cout<<"Production chain tests passed\n";
    return 0;
}
