#include "simulation/World.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace openemperor::simulation {
Command place_bridge(Cell a,Cell b) {
    Command result{CommandType::PlaceBridge,a};result.bridge_end=b;return result;
}
const BridgeSpan* World::bridge_at(Cell cell) const {
    if(bridge_index_.empty()||!in_bounds(cell))return nullptr;
    const auto id=bridge_index_[index(cell)];return id?&bridges_[std::size_t(id)-1]:nullptr;
}
const BridgeSpan* World::bridge_bank_at(Cell cell) const {
    if(bridge_bank_index_.empty()||!in_bounds(cell))return nullptr;
    const auto id=bridge_bank_index_[index(cell)];return id?&bridges_[std::size_t(id)-1]:nullptr;
}
void World::index_bridge(const BridgeSpan& span) {
    const auto id=std::uint8_t(bridges_.size());
    for(const auto cell:span.water_cells)bridge_index_[index(cell)]=id;
    bridge_bank_index_[index(span.bank_a)]=bridge_bank_index_[index(span.bank_b)]=id;
}
std::int8_t World::transport_height(Cell cell) const {
    if(const auto* bridge=bridge_at(cell))return bridge->deck_height;
    return permissions_?permissions_->cell_height(cell):0;
}
BuildBlocker World::transport_edge_blocker(Cell from,Cell to) const {
    if(!in_bounds(from)||!in_bounds(to))return BuildBlocker::OutsideMap;
    if(std::abs(from.x-to.x)+std::abs(from.y-to.y)!=1)return BuildBlocker::InvalidEdge;
    const auto* source_from=source_cell_ramp_source_at(from);
    const auto* source_to=source_cell_ramp_source_at(to);
    const auto* source_ramp=source_from?source_from:source_to;
    if(source_ramp) {
        const Cell other=source_from?to:from;
        const auto source=source_ramp->source_cell,high=source_ramp->high_neighbor;
        const Cell low{2*source.x-high.x,2*source.y-high.y};
        if((other!=low&&other!=high)||building_owner_at(other))
            return BuildBlocker::UnsupportedHeightTransition;
    }
    const auto* a=bridge_at(from);const auto* b=bridge_at(to);
    if(a||b) {
        const auto* bridge=a?a:b;
        if(a&&b&&a!=b)return BuildBlocker::BridgeSideEntry;
        const bool longitudinal=bridge->axis==BridgeAxis::StorageX ? from.y==to.y:from.x==to.x;
        const auto other=a?to:from;
        if(!longitudinal || ((!a || !b) && other!=bridge->bank_a && other!=bridge->bank_b))
            return BuildBlocker::BridgeSideEntry;
        return BuildBlocker::None;
    }
    const auto base=permissions_?permissions_->transport_edge_blocker(from,to):BuildBlocker::None;
    if(source_ramp&&base==BuildBlocker::UnsupportedHeightTransition)return BuildBlocker::None;
    if(base==BuildBlocker::UnsupportedHeightTransition&&ramp_between(from,to))return BuildBlocker::None;
    return base;
}
BridgeValidation World::validate_bridge(Cell a,Cell b) const {
    BridgeValidation result;
    const auto block=[&](BuildBlocker blocker,Cell cell,const char* reason=nullptr) {
        result.reason=reason?reason:build_blocker_reason(blocker);result.diagnostic={blocker,cell,{}};return result;
    };
    if(!bridges_supported())return block(BuildBlocker::UnsupportedTerrain,a,"Bridges require explicit City-v16 rule 5");
    if(!in_bounds(a))return block(BuildBlocker::OutsideMap,a);
    if(!in_bounds(b))return block(BuildBlocker::OutsideMap,b);
    if(a==b || (a.x!=b.x && a.y!=b.y))return block(BuildBlocker::InvalidEdge,b,"Bridge must be one straight storage axis");
    const int distance=std::abs(a.x-b.x)+std::abs(a.y-b.y);
    if(distance<2 || distance>int(bridge_max_water_cells)+1)
        return block(BuildBlocker::InvalidEdge,b,"Bridge requires 1 to 8 water cells between its banks");
    if(a.x>b.x || a.y>b.y)std::swap(a,b);
    for(const auto bank:{a,b}) {
        if(permissions_->road_blocker(bank)==BuildBlocker::OutsideMap)return block(BuildBlocker::OutsideMap,bank);
        if(permissions_->protected_original(bank))return block(permissions_->fixed_gate_at(bank)?BuildBlocker::ProtectedGatePassage:BuildBlocker::OriginalStructure,bank);
        if(!permissions_->road_allowed(bank)||permissions_->protected_original(bank)||permissions_->fixed_gate_at(bank)||!road_cell(bank))
            return block(BuildBlocker::BridgeBankRoadRequired,bank);
        if(bridge_bank_at(bank)||bridge_at(bank))return block(BuildBlocker::SandboxOccupied,bank,"Bridge spans cannot overlap or share a bank");
        if(source_cell_ramp_claim(bank))return block(BuildBlocker::SandboxOccupied,bank,
            "Bridge bank cannot overlap a source-cell ramp axial claim");
    }
    if(permissions_->cell_height(a)!=permissions_->cell_height(b))
        return block(BuildBlocker::UnsupportedHeightTransition,b,"Bridge banks must have the same signed deck height");
    const Cell delta{a.x!=b.x?1:0,a.y!=b.y?1:0};
    auto& span=result.span;span.id={next_bridge_id_};span.bank_a=a;span.bank_b=b;
    span.axis=delta.x?BridgeAxis::StorageX:BridgeAxis::StorageY;span.deck_height=permissions_->cell_height(a);
    span.placed_tick=ticks_;
    for(int i=1;i<distance;++i) {
        const Cell cell{a.x+i*delta.x,a.y+i*delta.y};
        if(permissions_->road_blocker(cell)==BuildBlocker::OutsideMap)return block(BuildBlocker::OutsideMap,cell);
        if(permissions_->protected_original(cell))return block(permissions_->fixed_gate_at(cell)?BuildBlocker::ProtectedGatePassage:BuildBlocker::OriginalStructure,cell);
        if(!permissions_->bridge_water(cell))return block(BuildBlocker::UnsupportedBridgeWater,cell);
        if(object_at(cell)!=Object::Empty||bridge_at(cell)||bridge_bank_at(cell))
            return block(BuildBlocker::SandboxOccupied,cell,"Bridge overlaps existing sandbox occupancy");
        span.water_cells.push_back(cell);
    }
    span.paid_cost=bridge_base_cost+bridge_water_cell_cost*std::int64_t(span.water_cells.size());
    result.geometry_valid=true;
    if(bridges_.size()>=bridge_max_spans || next_bridge_id_==UINT32_MAX ||
        road_revision_==UINT64_MAX || command_sequence_>=UINT64_MAX-1 ||
        construction_spent_total_>UINT64_MAX-std::uint64_t(span.paid_cost))
        return block(BuildBlocker::CounterExhausted,a);
    if(treasury_<span.paid_cost)return block(BuildBlocker::InsufficientFunds,a);
    result.accepted=true;result.reason="Bridge ready: whole span, no upkeep; removal unavailable in V1";return result;
}
void World::restore_bridges(const WorldSnapshot& snapshot) {
    if(!bridges_supported()) {
        if(!snapshot.bridges.empty()||snapshot.next_bridge_id!=1)throw std::invalid_argument("bridge authority requires City-v16 rule 5");
        return;
    }
    if(snapshot.bridges.size()>bridge_max_spans||snapshot.next_bridge_id!=snapshot.bridges.size()+1)
        throw std::invalid_argument("invalid bridge count or next identity");
    for(const auto& span:snapshot.bridges) {
        if(span.id.value!=bridges_.size()+1 || !in_bounds(span.bank_a)||!in_bounds(span.bank_b)||
            (span.axis!=BridgeAxis::StorageX&&span.axis!=BridgeAxis::StorageY)||
            span.water_cells.empty()||span.water_cells.size()>bridge_max_water_cells||span.placed_tick>snapshot.ticks)
            throw std::invalid_argument("invalid saved bridge identity, bounds, axis, length or tick");
        const Cell delta=span.axis==BridgeAxis::StorageX?Cell{1,0}:Cell{0,1};
        const int distance=int(span.water_cells.size())+1;
        if(span.bank_b!=Cell{span.bank_a.x+distance*delta.x,span.bank_a.y+distance*delta.y}||
            span.paid_cost!=bridge_base_cost+bridge_water_cell_cost*std::int64_t(span.water_cells.size()))
            throw std::invalid_argument("noncanonical saved bridge geometry or cost");
        // A normally removed bank road may be absent. Its immutable land and
        // reserved bank geometry remain exact; no building may occupy it.
        for(const auto bank:{span.bank_a,span.bank_b})
            if(!permissions_->road_allowed(bank)||permissions_->protected_original(bank)||permissions_->fixed_gate_at(bank)||
                permissions_->cell_height(bank)!=span.deck_height||bridge_bank_at(bank)||bridge_at(bank)||
                (object_at(bank)!=Object::Empty&&object_at(bank)!=Object::Road))
                throw std::invalid_argument("invalid saved bridge bank, deck height or overlap");
        for(std::size_t i=0;i<span.water_cells.size();++i) {
            const Cell expected{span.bank_a.x+int(i+1)*delta.x,span.bank_a.y+int(i+1)*delta.y};
            if(span.water_cells[i]!=expected||!permissions_->bridge_water(expected)||
                object_at(expected)!=Object::Empty||bridge_at(expected)||bridge_bank_at(expected))
                throw std::invalid_argument("invalid saved bridge water claim or overlap");
        }
        bridges_.push_back(span);
        index_bridge(bridges_.back());
    }
    next_bridge_id_=snapshot.next_bridge_id;
}
} // namespace openemperor::simulation
