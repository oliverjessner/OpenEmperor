#include "maps/OrdinaryGateMapPresentation.h"
#include <memory>
#include <set>

namespace openemperor::maps {
namespace {
constexpr std::size_t count=stored_grid_width*stored_grid_height;
}
OrdinaryGateMapPresentation prepare_ordinary_gate_presentation(
    const OriginalMapEntities& entities,std::span<const std::uint32_t> terrain) {
    OrdinaryGateMapPresentation out;
    out.manager_records=entities.records.size();out.gate_by_storage.resize(count);
    const MapGeometry map{entities.declared_map_size};
    if (!map.supported || terrain.size()!=count) {
        out.error="ordinary GateHouse requires complete supported map terrain";return out;
    }
    std::set<std::size_t> claims;
    for (const auto& entity:entities.records) {
        if (!entity.active() || entity.entity_class!=OriginalEntityClass::GateHouse) continue;
        OrdinaryGatePresentation gate;gate.source=entity;
        if (entity.type!=130 || !entity.gate_house ||
            (entity.gate_house->layout!=0 && entity.gate_house->layout!=1)) {
            gate.fallback="unsupported original GateHouse type/layout; historical preview retained";
        } else if (entity.local_x<0 || entity.local_y<0) {
            gate.fallback="invalid original GateHouse origin; historical preview retained";
        } else {
            const auto layout=unsigned(entity.gate_house->layout);
            gate.origin={unsigned(entity.local_x)+map.border,unsigned(entity.local_y)+map.border};
            gate.width=layout ? 3U:5U;gate.height=layout ? 5U:3U;
            bool valid=entity.serialized_cell_reference==
                std::int32_t(gate.origin.y*stored_grid_width+gate.origin.x);
            std::vector<std::size_t> members;
            for (unsigned row=0;row<15 && valid;++row) {
                // 850fc0: layout0 rows x0..4 within y0..2; layout1 rows
                // x2,y0..4, then x1,y0..4, then x0,y0..4. Camera zero uses
                // 4f9170's variant (row.+0c - 1) + 15*layout.
                const unsigned dx=layout ? 2U-row/5U:row%5U;
                const unsigned dy=layout ? row%5U:row/5U;
                const GridCell cell{gate.origin.x+dx,gate.origin.y+dy};
                if (!map.contains(cell)) {valid=false;break;}
                const auto at=std::size_t(cell.y)*stored_grid_width+cell.x;
                if ((terrain[at]&0x8008U)!=0x8008U || (terrain[at]&0x4000U) || claims.contains(at)) {
                    valid=false;break;
                }
                LandscapeInstanceSpec spec;
                spec.selection={LandscapeFamily::OrdinaryGate,SelectorEvidence::Verified,
                    "52f030 / 4f9780 / 4f9570 / 4f9170",
                    "EXE-observed saved GateHouse layout, restore boolean zero and camera-zero component",
                    {ordinary_gate_resource_key},row+15U*layout,{}};
                spec.origin=cell;spec.draw_cell=cell;spec.owned_cells={cell};
                spec.explicit_height=LandscapeInstanceHeight{LandscapeInstanceHeightSource::SerializedCellHeight,cell};
                spec.placement_evidence="EXE-observed singleton GateHouse marker; existing Type30 anchor and signed height operand";
                spec.composition_evidence="complete fifteen-component GateHouse; OPENEMPEROR PREVIEW existing split Base/Overlay painter";
                gate.components.push_back(std::move(spec));members.push_back(at);
            }
            if (!valid || members.size()!=15) {
                gate.components.clear();
                gate.fallback="original GateHouse complete footprint/reference mismatch or conflict; historical preview retained";
            } else for (const auto member:members) {
                claims.insert(member);out.gate_by_storage[member]=out.gates.size();
            }
        }
        out.gates.push_back(std::move(gate));
    }
    return out;
}
void read_ordinary_gate_presentation(StoredGraphicsPlan& plan,
    const EmperorContainer& container,std::size_t part) {
    if (!plan.landscape_layers_available || plan.original_ordinary_gates) return;
    auto result=std::make_shared<OrdinaryGateMapPresentation>();
    try {
        const auto entities=read_original_map_entities(container,part);
        *result=prepare_ordinary_gate_presentation(entities,plan.raw_terrain);
    } catch (const std::exception& error) {result->error=error.what();}
    plan.original_ordinary_gates=std::move(result);
}
} // namespace openemperor::maps
