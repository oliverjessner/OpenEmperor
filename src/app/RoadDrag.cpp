#include "app/RoadDrag.h"

#include <cstdlib>
#include <utility>

namespace openemperor::sandbox_ui {

RoadPlan plan_road(const simulation::World& world,simulation::Cell start,
                   simulation::Cell end) {
    performance::ScopedTimer timer(performance::Timing::RoadPlanning);
    performance::increment(performance::Counter::RoadPlans);
    RoadPlan plan;
    constexpr int limit=256;
    const auto length=static_cast<long long>(std::abs(static_cast<long long>(end.x)-start.x))+
        std::abs(static_cast<long long>(end.y)-start.y)+1;
    if (length>limit) { plan.reason="Road drag exceeds 256 cells"; return plan; }
    plan.cells.reserve(static_cast<std::size_t>(length));
    auto at=start;
    plan.cells.push_back(at);
    while (at.x!=end.x) { at.x+=end.x>at.x ? 1:-1; plan.cells.push_back(at); }
    while (at.y!=end.y) { at.y+=end.y>at.y ? 1:-1; plan.cells.push_back(at); }
    const auto validated=world.validate_road_batch(plan.cells);
    plan.valid=validated.accepted;
    plan.reason=validated.reason;
    plan.new_road_count=validated.new_road_count;
    plan.total_cost=validated.total_cost;
    return plan;
}

bool commit_road(simulation::World& world,const RoadPlan& plan,std::string& reason) {
    performance::ScopedTimer timer(performance::Timing::RoadCommit);
    if (plan.cells.empty() || plan.cells.size()>256) {
        reason=plan.reason.empty() ? "Invalid road drag":plan.reason; return false;
    }
    const auto result=world.execute_road_batch(plan.cells);
    reason=result.reason;
    return result.accepted;
}

} // namespace openemperor::sandbox_ui
