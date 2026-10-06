#include "app/RoadDrag.h"
#include "app/RoadTopology.h"
#include "core/PerformanceDiagnostics.h"

#include <iostream>
#include <stdexcept>

namespace {
namespace sim=openemperor::simulation;
namespace ui=openemperor::sandbox_ui;
void check(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
sim::World fixture(bool horizontal) {
    constexpr int side=14;
    std::vector<sim::MapCellPermission> cells(side*side);
    for (auto& cell:cells) {
        cell.road_allowed=cell.building_allowed=true;
        cell.road_blocker=cell.building_blocker=sim::BuildBlocker::None;
    }
    sim::FixedGatePassage gate;
    gate.id={17};
    for (int y=4;y<4+(horizontal ? 5:3);++y)
        for (int x=4;x<4+(horizontal ? 3:5);++x) {
            gate.protected_footprint.push_back({x,y});
            auto& cell=cells[static_cast<std::size_t>(y*side+x)];
            cell.road_allowed=cell.building_allowed=false;
            cell.protected_original=true;
            cell.road_blocker=cell.building_blocker=sim::BuildBlocker::GateSolidPart;
        }
    for (int i=0;i<3;++i) {
        const sim::Cell p=horizontal ? sim::Cell{4+i,6}:sim::Cell{6,4+i};
        gate.corridor.push_back(p);
        cells[static_cast<std::size_t>(p.y*side+p.x)].road_allowed=true;
        cells[static_cast<std::size_t>(p.y*side+p.x)].road_blocker=sim::BuildBlocker::None;
    }
    gate.openings=horizontal ? std::array<sim::Cell,2>{{{3,6},{7,6}}}:
        std::array<sim::Cell,2>{{{6,3},{6,7}}};
    return sim::World(std::make_shared<const sim::MapPermissions>(side,side,1,
        std::move(cells),std::vector<sim::FixedGatePassage>{std::move(gate)}),
        sim::RulesProfile::CityV16,3);
}
}
int main() {
    try {
        for (const bool horizontal:{false,true}) {
            auto world=fixture(horizontal);
            const auto& gate=world.map_permissions()->gates().front();
            const auto before=world.snapshot();
            openemperor::performance::reset();
            openemperor::performance::set_enabled(true);
            const auto plan=ui::plan_road(world,gate.openings[0],gate.openings[1]);
            check(plan.valid && plan.new_road_count==2 && plan.total_cost==4,
                "gate preview charged fixed cells");
            check(world.snapshot()==before &&
                openemperor::performance::counter(openemperor::performance::Counter::WorldCopies)==0 &&
                openemperor::performance::counter(openemperor::performance::Counter::BfsCalls)==0 &&
                openemperor::performance::counter(openemperor::performance::Counter::FileReads)==0,
                "gate preview performed mutation, BFS or file work");
            const auto expected_a=horizontal ? 2:4;
            const auto expected_b=horizontal ? 8:1;
            check(ui::road_neighbor_mask_for_preview(world,plan.cells,gate.openings[0])==expected_a &&
                ui::road_neighbor_mask_for_preview(world,plan.cells,gate.openings[1])==expected_b &&
                ui::road_neighbor_mask_for_preview(world,plan.cells,gate.corridor[1])==0 &&
                ui::entrance_mask_for_preview(world,plan.cells,gate.corridor[1])==0,
                "preview road topology did not preserve exact gate openings");
            std::string reason;
            check(ui::commit_road(world,plan,reason),"valid gate drag did not commit");
            check(ui::road_neighbor_mask(world,gate.openings[0])==expected_a &&
                ui::road_neighbor_mask(world,gate.openings[1])==expected_b,
                "live road picture does not connect at gate openings");
            check(world.object_at(gate.corridor[1])==sim::Object::Empty,
                "preview/commit added a sandbox road in gate");
            openemperor::performance::set_enabled(false);

            const sim::Cell start=horizontal ? sim::Cell{1,5}:sim::Cell{5,1};
            const sim::Cell end=horizontal ? sim::Cell{4,5}:sim::Cell{5,4};
            const auto invalid=ui::plan_road(world,start,end);
            check(!invalid.valid && invalid.diagnostic.blocker==sim::BuildBlocker::GateSolidPart &&
                invalid.diagnostic.cell==end,"first solid blocker differs from drag order");
            check(ui::road_cell_blocked(invalid,end) && !ui::road_cell_blocked(invalid,start),
                "invalid batch labels safe cells as individually forbidden");
            check(ui::road_plan_status(invalid).find("Blocked at (")==0,
                "road status lacks central blocking coordinate");
            const auto blocked_before=world.snapshot();
            check(!ui::commit_road(world,invalid,reason) && world.snapshot()==blocked_before,
                "invalid visible drag partially committed");
        }
        std::cout<<"gate road preview, exact visual openings and atomic diagnostics passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
