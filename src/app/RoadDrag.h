#pragma once

#include "simulation/World.h"

#include <string>
#include <vector>

namespace openemperor::sandbox_ui {

struct RoadPlan {
    std::vector<simulation::Cell> cells;
    bool valid=false;
    std::string reason;
};

RoadPlan plan_road(const simulation::World& world,simulation::Cell start,
                   simulation::Cell end);
bool commit_road(simulation::World& world,const RoadPlan& plan,std::string& reason);

} // namespace openemperor::sandbox_ui
