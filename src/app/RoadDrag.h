#pragma once

#include "simulation/World.h"

#include <string>
#include <cstdint>
#include <vector>

namespace openemperor::sandbox_ui {

struct RoadPlan {
    std::vector<simulation::Cell> cells;
    bool valid=false;
    std::string reason;
    std::size_t new_road_count=0;
    std::int64_t total_cost=0;
};

RoadPlan plan_road(const simulation::World& world,simulation::Cell start,
                   simulation::Cell end);
bool commit_road(simulation::World& world,const RoadPlan& plan,std::string& reason);

} // namespace openemperor::sandbox_ui
