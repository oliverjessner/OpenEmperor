#include "app/RoadTopology.h"
#include <algorithm>
#include <array>

namespace openemperor::sandbox_ui {
namespace {
constexpr std::array<simulation::Cell,4> directions{{{0,-1},{1,0},{0,1},{-1,0}}};
bool is_road(const simulation::World& world,std::span<const simulation::Cell> planned,
             simulation::Cell cell) {
    if (world.fixed_passage(cell)) return false;
    return world.object_at(cell)==simulation::Object::Road ||
        std::find(planned.begin(),planned.end(),cell)!=planned.end();
}
bool is_building(simulation::Object object) {
    return object!=simulation::Object::Empty && object!=simulation::Object::Road;
}
}
RoadNeighborMask road_neighbor_mask_for_preview(const simulation::World& world,
                                                std::span<const simulation::Cell> planned,
                                                simulation::Cell cell) {
    if (!is_road(world,planned,cell)) return 0;
    RoadNeighborMask mask=0;
    for (std::size_t bit=0;bit<directions.size();++bit) {
        const simulation::Cell neighbor{cell.x+directions[bit].x,cell.y+directions[bit].y};
        const auto& permissions=world.map_permissions();
        if ((is_road(world,planned,neighbor) || world.fixed_passage(neighbor)) &&
            (!permissions || permissions->transport_edge_allowed(cell,neighbor)))
            mask=static_cast<RoadNeighborMask>(mask | (1U<<bit));
    }
    return mask;
}
RoadNeighborMask road_neighbor_mask(const simulation::World& world,simulation::Cell cell) {
    return road_neighbor_mask_for_preview(world,{},cell);
}
EntranceMask entrance_mask_for_preview(const simulation::World& world,
                                       std::span<const simulation::Cell> planned,
                                       simulation::Cell cell) {
    if (!is_road(world,planned,cell)) return 0;
    EntranceMask mask=0;
    for (std::size_t bit=0;bit<directions.size();++bit) {
        const simulation::Cell neighbor{cell.x+directions[bit].x,cell.y+directions[bit].y};
        const auto& permissions=world.map_permissions();
        if (is_building(world.object_at(neighbor)) &&
            (!permissions || permissions->transport_edge_allowed(cell,neighbor)))
            mask=static_cast<EntranceMask>(mask | (1U<<bit));
    }
    return mask;
}
EntranceMask entrance_mask(const simulation::World& world,simulation::Cell cell) {
    return entrance_mask_for_preview(world,{},cell);
}
}
