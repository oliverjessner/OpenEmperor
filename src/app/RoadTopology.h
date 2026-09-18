#pragma once

#include "simulation/World.h"
#include <cstdint>
#include <span>

namespace openemperor::sandbox_ui {
// OpenEmperor preview bits in storage coordinates; no claim about Emperor IDs.
// bit 0: (0,-1), bit 1: (+1,0), bit 2: (0,+1), bit 3: (-1,0).
using RoadTopology = std::uint8_t;
RoadTopology topology_for_road(const simulation::World& world, simulation::Cell cell);
RoadTopology topology_for_preview(const simulation::World& world,
                                  std::span<const simulation::Cell> planned,
                                  simulation::Cell cell);
}
