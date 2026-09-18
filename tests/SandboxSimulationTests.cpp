#include "simulation/World.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace openemperor::simulation;
namespace {
void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
World world(int width=8,int height=5) {
    return World(width,height,std::vector<std::uint8_t>(static_cast<std::size_t>(width*height),1));
}
void place(World& w,CommandType type,int x,int y) {
    check(w.execute({type,{x,y}}).accepted,"placement failed");
}
void advance(World& w,int ticks) {
    for (int i=0;i<ticks;++i) { w.tick(); check(w.goods_balance_valid(),"goods balance"); }
}
void placement() {
    auto w=World(4,2,{1,1,0,1,1,1,1,1});
    place(w,CommandType::PlaceRoad,0,0);
    check(w.execute({CommandType::PlaceRoad,{0,0}}).accepted,"duplicate road should be no-op");
    check(!w.execute({CommandType::PlaceRoad,{0,0}}).changed,"duplicate road changed world");
    check(!w.execute({CommandType::PlaceRoad,{2,0}}).accepted,"blocked cell accepted");
    check(!w.execute({CommandType::PlaceRoad,{4,0}}).accepted,"out of bounds accepted");
    place(w,CommandType::PlaceWorkshop,1,0);
    place(w,CommandType::PlaceWarehouse,3,0);
    check(!w.execute({CommandType::PlaceRoad,{1,0}}).accepted,"overlap accepted");
    check(!w.execute({CommandType::PlaceWorkshop,{0,1}}).accepted,"second workshop accepted");
    check(!w.execute({CommandType::PlaceWarehouse,{1,1}}).accepted,"second warehouse accepted");
    check(w.command_sequence()==10,"command count");
}
void routing() {
    auto w=world(7,5);
    place(w,CommandType::PlaceWorkshop,0,2);
    place(w,CommandType::PlaceWarehouse,6,2);
    check(!w.find_route(),"route without roads");
    place(w,CommandType::PlaceRoad,1,1);
    place(w,CommandType::PlaceRoad,0,1);
    check(!w.find_route(),"dead-end must not connect");
    for (int x=1;x<=5;++x) place(w,CommandType::PlaceRoad,x,2);
    const auto direct=w.find_route();
    check(direct && direct->size()==7 && (*direct)[1]==Cell{1,2},"BFS missed shortest valid connection");
    auto equal=world(5,5);
    place(equal,CommandType::PlaceWorkshop,0,2);
    place(equal,CommandType::PlaceWarehouse,4,2);
    for (int x=0;x<=4;++x) {
        place(equal,CommandType::PlaceRoad,x,1);
        place(equal,CommandType::PlaceRoad,x,3);
    }
    const auto route=equal.find_route();
    check(route && route->size()==7 && (*route)[1]==Cell{0,1},"equal-route tie-break changed");
    auto diagonal=world(3,3);
    place(diagonal,CommandType::PlaceWorkshop,0,0);
    place(diagonal,CommandType::PlaceWarehouse,2,2);
    place(diagonal,CommandType::PlaceRoad,1,1);
    check(!diagonal.find_route(),"diagonal shortcut accepted");
}
void logistics_gap() {
    auto w=world(6,1);
    place(w,CommandType::PlaceWorkshop,0,0);
    place(w,CommandType::PlaceWarehouse,5,0);
    for (int x : {1,2,4}) place(w,CommandType::PlaceRoad,x,0);
    advance(w,99);
    check(w.total_produced()==0,"early production");
    advance(w,1);
    check(w.workshop_stock()==1 && w.warehouse_stock()==0 && w.courier_cargo()==0,
          "goods moved through gap");
    check(std::string(w.blockage())=="No road connection","missing route not shown");
    const auto revision=w.road_revision();
    place(w,CommandType::PlaceRoad,3,0);
    check(w.road_revision()==revision+1,"gap did not revise road graph");
    advance(w,1);
    check(w.workshop_stock()==0 && w.courier_cargo()==1 && w.warehouse_stock()==0,
          "dispatch did not transfer goods");
    check(w.courier_phase()==CourierPhase::ToWarehouse,"courier did not depart");
    const auto first=*w.courier_position();
    advance(w,20);
    check(w.warehouse_stock()==0 && w.courier_position()->x>first.x,"premature delivery or no motion");
    advance(w,29);
    check(w.warehouse_stock()==1 && w.courier_cargo()==0 &&
          w.courier_phase()==CourierPhase::Returning,"arrival did not unload");
    const auto at_warehouse=*w.courier_position();
    advance(w,10);
    check(w.courier_position()->x<at_warehouse.x,"return teleported or stalled");
    advance(w,40);
    check(w.courier_phase()==CourierPhase::IdleAtWorkshop,"courier failed to return");
}
void capacities() {
    auto w=world(3,1);
    place(w,CommandType::PlaceWorkshop,0,0);
    place(w,CommandType::PlaceRoad,1,0);
    place(w,CommandType::PlaceWarehouse,2,0);
    advance(w,5000);
    check(w.warehouse_stock()==Rules::warehouse_capacity,"warehouse did not fill");
    check(w.workshop_stock()==Rules::workshop_capacity,"workshop buffer did not fill");
    check(w.courier_cargo()==0,"cargo after full warehouse");
    const auto produced=w.total_produced();
    advance(w,500);
    check(w.total_produced()==produced && w.production_progress()==0,
          "full production buffer accumulated goods");
}
std::string replay() {
    auto w=world(6,1);
    place(w,CommandType::PlaceWorkshop,0,0);
    place(w,CommandType::PlaceWarehouse,5,0);
    for (int x : {1,2,4}) place(w,CommandType::PlaceRoad,x,0);
    advance(w,117);
    place(w,CommandType::PlaceRoad,3,0);
    advance(w,283);
    const auto p=w.courier_position();
    return std::to_string(w.ticks())+":"+std::to_string(w.command_sequence())+":"+
        std::to_string(w.total_produced())+":"+std::to_string(w.workshop_stock())+":"+
        std::to_string(w.courier_cargo())+":"+std::to_string(w.warehouse_stock())+":"+
        std::to_string(static_cast<int>(w.courier_phase()))+":"+
        (p?std::to_string(p->x):"none");
}
void timing() {
    check(replay()==replay(),"replay not deterministic");
    auto a=world(1,1),b=world(1,1);
    place(a,CommandType::PlaceWorkshop,0,0);
    place(b,CommandType::PlaceWorkshop,0,0);
    TickDriver clock_a,clock_b;
    for (int i=0;i<100;++i) clock_a.update(0.05,a);
    for (int i=0;i<200;++i) clock_b.update(0.025,b);
    check(a.ticks()==100 && b.ticks()==100 && a.total_produced()==b.total_produced(),
          "frame partition changed simulation");
    clock_a.toggle_pause();
    clock_a.update(3.0,a);
    check(a.ticks()==100,"pause advanced time");
    clock_a.step_once(a);
    check(a.ticks()==101,"step was not exactly one tick");
    clock_a.toggle_pause();
    clock_a.set_speed(2);
    clock_a.update(0.1,a);
    check(a.ticks()==105,"2x speed changed tick delivery");
    clock_a.set_speed(4);
    clock_a.update(0.05,a);
    check(a.ticks()==109,"4x speed changed tick delivery");
    const auto before=a.ticks();
    clock_a.update(100.0,a);
    check(a.ticks()<=before+8,"catch-up unbounded");
}
}
int main() {
    try { placement(); routing(); logistics_gap(); capacities(); timing(); }
    catch (const std::exception& error) {
        std::cerr << "Sandbox simulation test failed: " << error.what() << '\n'; return 1;
    }
    std::cout << "Sandbox simulation tests passed\n";
    return 0;
}
