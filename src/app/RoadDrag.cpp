#include "app/RoadDrag.h"
#include "app/RoadTopology.h"
#include "simulation/original/IrrigationRules.h"
#include <algorithm>
#include <array>
#include <tuple>
#include <limits>

#include <cstdlib>
#include <utility>

namespace openemperor::sandbox_ui {

namespace {
// Shared bounded axial gesture geometry; only World validates the chosen layer.
bool axial_cells(simulation::Cell start,simulation::Cell end,
        std::vector<simulation::Cell>& cells) {
    const auto length=std::abs(static_cast<long long>(end.x)-start.x)+
        std::abs(static_cast<long long>(end.y)-start.y)+1;
    if (length>256) return false;
    cells.reserve(static_cast<std::size_t>(length));auto at=start;cells.push_back(at);
    while (at.x!=end.x) {at.x+=end.x>at.x ? 1:-1;cells.push_back(at);}
    while (at.y!=end.y) {at.y+=end.y>at.y ? 1:-1;cells.push_back(at);}
    return true;
}
}
std::uint8_t irrigation_visual_variant(const simulation::World& world,simulation::Cell cell,
        std::span<const simulation::Cell> planned) {
    if (!world.irrigation_state()) return 15;
    const auto& ditches=world.irrigation_state()->ditches;
    const auto has_ditch=[&](simulation::Cell wanted) {
        if (std::find(planned.begin(),planned.end(),wanted)!=planned.end()) return true;
        const auto at=std::lower_bound(ditches.begin(),ditches.end(),wanted,[](const auto& ditch,auto other){
            return std::tie(ditch.cell.y,ditch.cell.x)<std::tie(other.y,other.x);});
        return at!=ditches.end() && at->cell==wanted;
    };
    std::uint8_t mask=0;
    constexpr std::array<simulation::Cell,4> offsets{{{0,-1},{1,0},{0,1},{-1,0}}};
    for (std::size_t i=0;i<offsets.size();++i) {
        const auto x=std::int64_t{cell.x}+offsets[i].x,y=std::int64_t{cell.y}+offsets[i].y;
        if (x<std::numeric_limits<int>::min() || x>std::numeric_limits<int>::max() ||
            y<std::numeric_limits<int>::min() || y>std::numeric_limits<int>::max()) continue;
        const simulation::Cell next{static_cast<int>(x),static_cast<int>(y)};
        bool connected=has_ditch(next);
        if (!connected) if (const auto id=world.building_owner_at(next);
            id && world.building(*id).kind==simulation::Object::OriginalIrrigationPump) {
            const auto& pumps=world.irrigation_state()->pumps;
            const auto pump=std::lower_bound(pumps.begin(),pumps.end(),*id,[](const auto& row,auto wanted){return row.building_id<wanted;});
            if (pump!=pumps.end() && pump->building_id==*id) {
                const auto shape=original::pump_geometry(world.building(*id).cell,pump->orientation);
                connected=std::find(shape.land.begin(),shape.land.end(),next)!=shape.land.end() &&
                    std::find(shape.seeds.begin(),shape.seeds.end(),cell)!=shape.seeds.end();
            }
        }
        if (connected) mask|=static_cast<std::uint8_t>(1U<<i);
    }
    std::optional<original::IrrigationAxis> road_axis;
    if (world.road_cell(cell)) road_axis=(road_neighbor_mask(world,cell)&5U) ? original::IrrigationAxis::Y:original::IrrigationAxis::X;
    return original::ditch_variant(mask,road_axis);
}
IrrigationPlan plan_irrigation(const simulation::World& world,simulation::Cell start,
        simulation::Cell end) {
    IrrigationPlan plan;
    if (!axial_cells(start,end,plan.cells)) {plan.validation.reason="Ditch drag exceeds256 cells";return plan;}
    plan.validation=world.validate_irrigation_batch(plan.cells);
    plan.variants.reserve(plan.cells.size());
    for (const auto cell:plan.cells) plan.variants.push_back(irrigation_visual_variant(world,cell,plan.cells));
    return plan;
}
RoadPlan plan_road(const simulation::World& world,simulation::Cell start,
                   simulation::Cell end) {
    performance::ScopedTimer timer(performance::Timing::RoadPlanning);
    performance::increment(performance::Counter::RoadPlans);
    RoadPlan plan;
    if (!axial_cells(start,end,plan.cells)) {plan.reason="Road drag exceeds 256 cells";return plan;}
    const auto validated=world.validate_road_batch(plan.cells);
    plan.valid=validated.accepted;
    plan.reason=validated.reason;
    plan.new_road_count=validated.new_road_count;
    plan.total_cost=validated.total_cost;
    plan.diagnostic=validated.diagnostic;
    plan.additional_blockers=validated.additional_blockers;
    plan.cell_diagnostics=validated.cell_diagnostics;
    return plan;
}

std::string road_plan_status(const RoadPlan& plan) {
    if (plan.valid || !plan.diagnostic.cell) return plan.reason;
    const auto cell=*plan.diagnostic.cell;
    return "Blocked at ("+std::to_string(cell.x)+","+std::to_string(cell.y)+
        "): "+plan.reason;
}

bool road_cell_blocked(const RoadPlan& plan,simulation::Cell cell) {
    for (const auto& item:plan.cell_diagnostics)
        if (item.cell==cell && !item.allowed) return true;
    const auto blocks=[&](const simulation::BuildDiagnostic& item) {
        return (item.cell && *item.cell==cell) ||
            (item.edge && (item.edge->from==cell || item.edge->to==cell));
    };
    if (blocks(plan.diagnostic)) return true;
    for (const auto& item:plan.additional_blockers) if (blocks(item)) return true;
    return false;
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
