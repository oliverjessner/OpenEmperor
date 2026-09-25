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
            check(ui::road_neighbor_mask(world,{3,3})==expected,"one of 16 road masks wrong");
            check(world.object_at({3,3})==sim::Object::Road,"topology mutated road");
        }
        auto world=make_world();place(world,sim::CommandType::PlaceRoad,{3,3});
        const auto before=world.snapshot();
        const std::array<sim::Cell,2> proposed{{{4,3},{4,4}}};
        check(ui::road_neighbor_mask_for_preview(world,proposed,{3,3})==0x2 &&
              ui::road_neighbor_mask_for_preview(world,proposed,{4,3})==0xc &&
              world.snapshot()==before,"preview topology changed World or L corner wrong");

        auto entrances=make_world();
        for (const auto cell:std::array<sim::Cell,3>{{{2,3},{3,3},{4,3}}})
            place(entrances,sim::CommandType::PlaceRoad,cell);
        place(entrances,sim::CommandType::PlaceClaySource,{3,2});
        place(entrances,sim::CommandType::PlacePottery,{3,4});
        const auto adjacent=entrances.snapshot();
        const auto route_before=entrances.find_route({2,3},{4,3});
        check(ui::road_neighbor_mask(entrances,{3,3})==0xa &&
              ui::entrance_mask(entrances,{3,3})==0x5 &&
              ui::road_neighbor_mask_for_preview(entrances,{}, {3,3})==0xa &&
              ui::entrance_mask_for_preview(entrances,{}, {3,3})==0x5 &&
              entrances.snapshot()==adjacent &&
              entrances.find_route({2,3},{4,3})==route_before,
              "building entrances changed the road mask, navigation, or World");

        auto one_road=make_world();
        place(one_road,sim::CommandType::PlaceRoad,{3,3});
        place(one_road,sim::CommandType::PlaceRoad,{3,2});
        place(one_road,sim::CommandType::PlaceClaySource,{4,3});
        check(ui::road_neighbor_mask(one_road,{3,3})==0x1 &&
              ui::entrance_mask(one_road,{3,3})==0x2,
              "one road plus one building did not retain separate masks");

        auto preview_entrance=make_world();
        place(preview_entrance,sim::CommandType::PlaceClaySource,{3,2});
        const std::array<sim::Cell,3> planned{{{2,3},{3,3},{4,3}}};
        const auto preview_before=preview_entrance.snapshot();
        check(ui::road_neighbor_mask_for_preview(preview_entrance,planned,{3,3})==0xa &&
              ui::entrance_mask_for_preview(preview_entrance,planned,{3,3})==0x1 &&
              preview_entrance.snapshot()==preview_before,
              "building changed drag-preview road mask or preview mutated World");

        sim::World footprint(8,8,std::vector<std::uint8_t>(64,1),sim::RulesProfile::CityV10);
        place(footprint,sim::CommandType::PlacePottery,{3,3});
        for (const auto road_cell:std::array<sim::Cell,4>{{{3,2},{5,3},{4,5},{2,4}}})
            place(footprint,sim::CommandType::PlaceRoad,road_cell);
        check(ui::entrance_mask(footprint,{3,2})==0x4 &&
              ui::entrance_mask(footprint,{5,3})==0x8 &&
              ui::entrance_mask(footprint,{4,5})==0x1 &&
              ui::entrance_mask(footprint,{2,4})==0x2,
              "roads beside different 2x2 footprint cells were not building entrances");
        for (const auto road_cell:std::array<sim::Cell,4>{{{3,2},{5,3},{4,5},{2,4}}})
            check(ui::road_neighbor_mask(footprint,road_cell)==0,
                  "2x2 footprint entrance changed the roads-only neighbor mask");

        auto live=make_world();
        for (const auto cell:std::array<sim::Cell,3>{{{2,3},{3,3},{4,3}}})
            place(live,sim::CommandType::PlaceRoad,cell);
        check(ui::road_neighbor_mask(live,{3,3})==0xa,"initial straight");
        place(live,sim::CommandType::PlaceRoad,{3,2});
        check(ui::road_neighbor_mask(live,{3,3})==0xb,"live T");
        place(live,sim::CommandType::PlaceRoad,{3,4});
        check(ui::road_neighbor_mask(live,{3,3})==0xf,"live crossing");
        place(live,sim::CommandType::RemoveRoad,{3,2});
        check(ui::road_neighbor_mask(live,{3,3})==0xe,"live remove to T");
        place(live,sim::CommandType::RemoveRoad,{2,3});
        check(ui::road_neighbor_mask(live,{3,3})==0x6,"live remove to corner");
        place(live,sim::CommandType::RemoveRoad,{3,4});
        check(ui::road_neighbor_mask(live,{3,3})==0x2,"live remove to end");
        place(live,sim::CommandType::RemoveRoad,{4,3});
        check(ui::road_neighbor_mask(live,{3,3})==0x0,"live remove to isolated");
        const auto state=live.snapshot();
        const auto invalid=ui::plan_road(live,{3,3},{3,3});
        check(invalid.valid && live.snapshot()==state,"drag planning mutated World");
        check(!live.execute({sim::CommandType::RemoveRoad,{1,1}}).accepted &&
              ui::road_neighbor_mask(live,{3,3})==0x0,"rejected command changed topology");
        std::cout<<"road topology: all 16 road-only masks, entrances and live preview passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
