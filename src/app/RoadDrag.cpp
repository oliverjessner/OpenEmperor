#include "app/RoadDrag.h"

#include <cstdlib>
#include <utility>

namespace openemperor::sandbox_ui {

RoadPlan plan_road(const simulation::World& world,simulation::Cell start,
                   simulation::Cell end) {
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
    plan.valid=true;
    auto candidate=world;
    for (const auto cell:plan.cells) {
        if (candidate.object_at(cell)==simulation::Object::Road) continue;
        const auto result=candidate.execute({simulation::CommandType::PlaceRoad,cell});
        if (!result.accepted) {
            plan.valid=false;
            if (plan.reason.empty()) plan.reason=result.reason;
        }
    }
    if (plan.valid) plan.reason="Road ready";
    return plan;
}

bool commit_road(simulation::World& world,const RoadPlan& plan,std::string& reason) {
    if (plan.cells.empty() || plan.cells.size()>256) {
        reason=plan.reason.empty() ? "Invalid road drag":plan.reason; return false;
    }
    // Revalidate against the current world. No simulation tick occurs in this copy.
    const auto current=plan_road(world,plan.cells.front(),plan.cells.back());
    if (!current.valid || current.cells!=plan.cells) { reason=current.reason; return false; }
    auto candidate=world;
    for (const auto cell:current.cells) {
        if (candidate.object_at(cell)==simulation::Object::Road) continue;
        const auto result=candidate.execute({simulation::CommandType::PlaceRoad,cell});
        if (!result.accepted) { reason=result.reason; return false; }
    }
    world=std::move(candidate);
    reason="Road placed";
    return true;
}

} // namespace openemperor::sandbox_ui
