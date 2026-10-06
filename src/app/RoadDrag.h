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
    simulation::BuildDiagnostic diagnostic;
    std::vector<simulation::BuildDiagnostic> additional_blockers;
    std::vector<simulation::RoadCellDiagnostic> cell_diagnostics;
};

RoadPlan plan_road(const simulation::World& world,simulation::Cell start,
                   simulation::Cell end);
bool commit_road(simulation::World& world,const RoadPlan& plan,std::string& reason);
// Read-only display of the central validation result; no second build rule.
std::string road_plan_status(const RoadPlan& plan);
bool road_cell_blocked(const RoadPlan& plan,simulation::Cell cell);

} // namespace openemperor::sandbox_ui
