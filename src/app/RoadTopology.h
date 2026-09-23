#pragma once

#include "simulation/World.h"
#include <cstdint>
#include <span>

namespace openemperor::sandbox_ui {
// OpenEmperor preview bits in storage coordinates; no claim about Emperor IDs.
// bit 0: (0,-1), bit 1: (+1,0), bit 2: (0,+1), bit 3: (-1,0).
using RoadNeighborMask = std::uint8_t;
using EntranceMask = std::uint8_t;

RoadNeighborMask road_neighbor_mask(const simulation::World& world, simulation::Cell cell);
RoadNeighborMask road_neighbor_mask_for_preview(const simulation::World& world,
                                                std::span<const simulation::Cell> planned,
                                                simulation::Cell cell);
EntranceMask entrance_mask(const simulation::World& world, simulation::Cell cell);
EntranceMask entrance_mask_for_preview(const simulation::World& world,
                                       std::span<const simulation::Cell> planned,
                                       simulation::Cell cell);
}
