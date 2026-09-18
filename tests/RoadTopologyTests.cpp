#include "app/RoadTopology.h"
#include "app/RoadDrag.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace sim=openemperor::simulation;
namespace ui=openemperor::sandbox_ui;
void check(bool result,const char* message) { if (!result) throw std::runtime_error(message); }
sim::World make_world() { return sim::World(7,7,std::vector<std::uint8_t>(49,1),
                                           sim::RulesProfile::ProductionV2); }
void place(sim::World& world,sim::CommandType type,sim::Cell cell) {
    check(world.execute({type,cell}).accepted,"synthetic placement failed");
}
constexpr std::array<sim::Cell,4> neighbors{{{3,2},{4,3},{3,4},{2,3}}};
}
int main() {
    try {
        for (int expected=0;expected<16;++expected) {
            auto world=make_world();place(world,sim::CommandType::PlaceRoad,{3,3});
            for (int bit=0;bit<4;++bit)
                if ((expected & (1<<bit))!=0)
                    place(world,sim::CommandType::PlaceRoad,neighbors[static_cast<std::size_t>(bit)]);
            check(ui::topology_for_road(world,{3,3})==expected,"one of 16 road masks wrong");
            check(world.object_at({3,3})==sim::Object::Road,"topology mutated road");
        }
        auto world=make_world();place(world,sim::CommandType::PlaceRoad,{3,3});
        const auto before=world.snapshot();
        const std::array<sim::Cell,2> proposed{{{4,3},{4,4}}};
        check(ui::topology_for_preview(world,proposed,{3,3})==0x2 &&
              ui::topology_for_preview(world,proposed,{4,3})==0xc &&
              world.snapshot()==before,"preview topology changed World or L corner wrong");
        place(world,sim::CommandType::PlaceClaySource,{3,2});
        const auto adjacent=world.snapshot();
        const auto route_before=world.find_route({3,3},{3,2});
        check(ui::topology_for_road(world,{3,3})==0x1 &&
              world.snapshot()==adjacent &&
              world.find_route({3,3},{3,2})==route_before,
              "building visual entrance altered navigation or World");

        auto live=make_world();
        for (const auto cell:std::array<sim::Cell,3>{{{2,3},{3,3},{4,3}}})
            place(live,sim::CommandType::PlaceRoad,cell);
        check(ui::topology_for_road(live,{3,3})==0xa,"initial straight");
        place(live,sim::CommandType::PlaceRoad,{3,2});
        check(ui::topology_for_road(live,{3,3})==0xb,"live T");
        place(live,sim::CommandType::PlaceRoad,{3,4});
        check(ui::topology_for_road(live,{3,3})==0xf,"live crossing");
        place(live,sim::CommandType::RemoveRoad,{3,2});
        check(ui::topology_for_road(live,{3,3})==0xe,"live remove to T");
        place(live,sim::CommandType::RemoveRoad,{2,3});
        check(ui::topology_for_road(live,{3,3})==0x6,"live remove to corner");
        const auto state=live.snapshot();
        const auto invalid=ui::plan_road(live,{3,3},{3,3});
        check(invalid.valid && live.snapshot()==state,"drag planning mutated World");
        check(!live.execute({sim::CommandType::RemoveRoad,{1,1}}).accepted &&
              ui::topology_for_road(live,{3,3})==0x6,"rejected command changed topology");
        std::cout<<"road topology: all 16 masks and live preview passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
