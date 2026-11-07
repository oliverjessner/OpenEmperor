#include "simulation/World.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace openemperor::simulation {
Command place_source_cell_ramp(Cell source,Cell high) {
    Command command{CommandType::PlaceSourceCellRamp,source};command.ramp_end=high;return command;
}
World World::create_authored_source_cell_ramps(std::shared_ptr<const MapPermissions> permissions) {
    if(!permissions||permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion)
        throw std::invalid_argument("authored source-cell ramps require complete policy3 permissions");
    return World(std::move(permissions),RulesProfile::CityV16,kAuthoredSourceCellRampRuleVersion);
}
const SourceCellRamp* World::source_cell_ramp_claim(Cell cell) const {
    if(source_cell_ramp_index_.empty()||!in_bounds(cell))return nullptr;
    const auto id=source_cell_ramp_index_[index(cell)];
    return id?&source_cell_ramps_[std::size_t(id)-1]:nullptr;
}
const SourceCellRamp* World::source_cell_ramp_source_at(Cell cell) const {
    const auto* ramp=source_cell_ramp_claim(cell);
    return ramp&&ramp->source_cell==cell?ramp:nullptr;
}
void World::index_source_cell_ramp(const SourceCellRamp& ramp) {
    const Cell delta{ramp.high_neighbor.x-ramp.source_cell.x,ramp.high_neighbor.y-ramp.source_cell.y};
    for(int step=-1;step<=1;++step)
        source_cell_ramp_index_[index({ramp.source_cell.x+delta.x*step,ramp.source_cell.y+delta.y*step})]=
            std::uint8_t(source_cell_ramps_.size());
}
SourceCellRampValidation World::validate_source_cell_ramp_geometry(Cell source,Cell high,bool require_roads) const {
    SourceCellRampValidation result;
    const auto block=[&](BuildBlocker blocker,Cell cell,const char* reason) {
        result.diagnostic={blocker,cell,TransportEdge{source,high}};result.reason=reason;return result;
    };
    if(!source_cell_ramps_supported())return block(BuildBlocker::UnsupportedTerrain,source,
        "Source-cell ramps require explicit authored City-v16 rule20");
    if(!in_bounds(source)||!in_bounds(high))return block(BuildBlocker::OutsideMap,
        !in_bounds(source)?source:high,"Source-cell ramp outside map");
    if(std::abs(high.x-source.x)+std::abs(high.y-source.y)!=1)
        return block(BuildBlocker::InvalidEdge,high,"Source-cell ramp needs one cardinal high neighbor");
    const Cell delta{high.x-source.x,high.y-source.y};
    const Cell low{source.x-delta.x,source.y-delta.y};
    if(!in_bounds(low))return block(BuildBlocker::OutsideMap,low,"Source-cell low join outside map");
    const int low_height=permissions_->cell_height(source),high_height=permissions_->cell_height(high);
    if(high_height-low_height!=1)return block(BuildBlocker::UnsupportedHeightTransition,high,
        "Source-cell ramp requires exactly one higher signed level");
    // Deliberately authored rectangular step, not an original selector or
    // normalization replay. Every surrounding fact must be supported land.
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        const Cell cell{source.x+x,source.y+y};
        const auto blocker=ramp_land_blocker(cell);
        if(blocker!=BuildBlocker::None)return block(blocker,cell,
            "Source-cell ramp needs a complete permitted land step neighborhood");
        const int expected=low_height+(x*delta.x+y*delta.y>0?1:0);
        if(permissions_->cell_height(cell)!=expected)
            return block(BuildBlocker::UnsupportedHeightTransition,cell,
                "Source-cell ramp requires exact low/lateral and high-facing step heights");
    }
    for(const auto cell:{low,source,high}) {
        if(bridge_at(cell)||bridge_bank_at(cell))return block(BuildBlocker::ProtectedBridge,cell,
            "Source-cell ramp cannot overlap bridge deck or bank");
        if(ramp_endpoint(cell)||source_cell_ramp_claim(cell))return block(BuildBlocker::SandboxOccupied,cell,
            "Source-cell ramp axial claims cannot overlap another ramp");
        const auto object=object_at(cell);
        if((require_roads&&!road_cell(cell))||(object!=Object::Empty&&object!=Object::Road))
            return block(BuildBlocker::BridgeBankRoadRequired,cell,
                "Source-cell ramp requires three existing normal land roads");
    }
    if(permissions_->transport_edge_blocker(source,high)!=BuildBlocker::UnsupportedHeightTransition ||
       permissions_->transport_edge_blocker(low,source)!=BuildBlocker::None)
        return block(BuildBlocker::UnsupportedTerrain,source,"Source-cell ramp cannot bypass protected edges");
    result.ramp={{next_source_cell_ramp_id_},source,high,std::int8_t(low_height),std::int8_t(high_height),
        source_cell_ramp_cost,ticks_};
    result.geometry_valid=true;return result;
}
SourceCellRampValidation World::validate_source_cell_ramp(Cell source,Cell high) const {
    auto result=validate_source_cell_ramp_geometry(source,high,true);
    if(!result.geometry_valid)return result;
    const Cell delta{high.x-source.x,high.y-source.y};
    const Cell low{source.x-delta.x,source.y-delta.y};
    for(const auto& courier:couriers_)for(const auto point:courier.path)
        if(point==low||point==source||point==high) {
            result.diagnostic={BuildBlocker::SandboxOccupied,point,{}};
            result.reason="Source-cell ramp cannot change a referenced courier surface or path";return result;
        }
    if(ramps_.size()+source_cell_ramps_.size()>=ramp_max_links ||
       next_source_cell_ramp_id_==UINT32_MAX||road_revision_==UINT64_MAX||command_sequence_>=UINT64_MAX-1 ||
       construction_spent_total_>UINT64_MAX-std::uint64_t(source_cell_ramp_cost)) {
        result.diagnostic={BuildBlocker::CounterExhausted,source,{}};
        result.reason="Source-cell ramp construction counter or shared 64-ramp capacity exhausted";return result;
    }
    if(treasury_<source_cell_ramp_cost) {
        result.diagnostic={BuildBlocker::InsufficientFunds,source,{}};
        result.reason="Not enough funds for source-cell ramp";return result;
    }
    result.accepted=true;result.reason="Authored source-cell ramp ready: $40, no upkeep or removal";return result;
}
void World::restore_source_cell_ramps(const WorldSnapshot& snapshot) {
    if(!source_cell_ramps_supported()) {
        if(!snapshot.source_cell_ramps.empty()||snapshot.next_source_cell_ramp_id!=1)
            throw std::invalid_argument("source-cell ramp authority requires exact rule20");
        return;
    }
    if(snapshot.source_cell_ramps.size()>source_cell_ramp_max_links ||
       snapshot.source_cell_ramps.size()+ramps_.size()>ramp_max_links ||
       snapshot.next_source_cell_ramp_id!=snapshot.source_cell_ramps.size()+1)
        throw std::invalid_argument("invalid source-cell ramp count or next identity");
    for(const auto& ramp:snapshot.source_cell_ramps) {
        const auto checked=validate_source_cell_ramp_geometry(ramp.source_cell,ramp.high_neighbor,false);
        if(!checked.geometry_valid||ramp.id.value!=source_cell_ramps_.size()+1 ||
           ramp.low_height!=checked.ramp.low_height||ramp.high_height!=checked.ramp.high_height||
           ramp.paid_cost!=source_cell_ramp_cost||ramp.placed_tick>snapshot.ticks||
           (!source_cell_ramps_.empty()&&ramp.placed_tick<source_cell_ramps_.back().placed_tick))
            throw std::invalid_argument("invalid saved source-cell ramp geometry, height, claim, identity, cost or tick");
        source_cell_ramps_.push_back(ramp);index_source_cell_ramp(source_cell_ramps_.back());
    }
    // A live road may be removed after construction, but every disjoint owned
    // axial claim required a real earlier purchase. Existing immutable imported
    // GroundRoads are the sole free case. Global counters can establish these
    // necessary history bounds; they do not invent per-cell placement times.
    std::uint64_t paid_claims=0,removed_claims=0;
    for(const auto& ramp:source_cell_ramps_) {
        const Cell delta{ramp.high_neighbor.x-ramp.source_cell.x,ramp.high_neighbor.y-ramp.source_cell.y};
        for(int step=-1;step<=1;++step) {
            const Cell cell{ramp.source_cell.x+delta.x*step,ramp.source_cell.y+delta.y*step};
            if(permissions_->original_road(cell))continue;
            ++paid_claims;if(object_at(cell)==Object::Empty)++removed_claims;
        }
    }
    if(snapshot.roads_placed_total<paid_claims||snapshot.roads_removed_total<removed_claims)
        throw std::invalid_argument("source-cell ramp claims lack necessary paid road construction/removal history");
    next_source_cell_ramp_id_=snapshot.next_source_cell_ramp_id;
}
std::optional<double> World::source_cell_ramp_surface_height(Position position) const {
    if(!source_cell_ramps_supported()||!std::isfinite(position.x)||!std::isfinite(position.y)||
       position.x<0||position.y<0||position.x>=width_||position.y>=height_)return {};
    const Cell owner{int(std::floor(position.x+.5)),int(std::floor(position.y+.5))};
    const auto* ramp=source_cell_ramp_claim(owner);if(!ramp)return {};
    const Cell delta{ramp->high_neighbor.x-ramp->source_cell.x,ramp->high_neighbor.y-ramp->source_cell.y};
    const double x=position.x-ramp->source_cell.x,y=position.y-ramp->source_cell.y;
    if((delta.x&&y!=0)||(delta.y&&x!=0))return {};
    const double along=x*delta.x+y*delta.y;if(along<-1||along>1)return {};
    return source_cell_ramp_axial_height(*ramp,along);
}
} // namespace openemperor::simulation
