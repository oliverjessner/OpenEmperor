#include "simulation/World.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace openemperor::simulation {
Command place_ramp(Cell a,Cell b) {
    Command c{CommandType::PlaceRamp,a};c.ramp_end=b;return c;
}
const RampLink* World::ramp_endpoint(Cell cell) const {
    if(ramp_index_.empty()||!in_bounds(cell))return nullptr;
    const auto i=ramp_index_[index(cell)];return i?&ramps_[std::size_t(i)-1]:nullptr;
}
const RampLink* World::ramp_between(Cell a,Cell b) const {
    const auto* r=ramp_endpoint(a);
    return r&&((r->endpoint_a==a&&r->endpoint_b==b)||(r->endpoint_a==b&&r->endpoint_b==a))?r:nullptr;
}
void World::index_ramp(const RampLink& r) {
    ramp_index_[index(r.endpoint_a)]=ramp_index_[index(r.endpoint_b)]=std::uint8_t(ramps_.size());
}
BuildBlocker World::ramp_land_blocker(Cell p) const {
    if(!in_bounds(p))return BuildBlocker::OutsideMap;
    if(permissions_->road_blocker(p)==BuildBlocker::OutsideMap)return BuildBlocker::OutsideMap;
    if(permissions_->fixed_gate_at(p))return BuildBlocker::ProtectedGatePassage;
    if(permissions_->protected_original(p))return BuildBlocker::OriginalStructure;
    if(bridge_at(p)||permissions_->bridge_water(p))return BuildBlocker::UnsupportedBridgeWater;
    const auto classification=permissions_->original_road_classification(p);
    if(!permissions_->road_allowed(p)||permissions_->road_blocker(p)!=BuildBlocker::None||
       (classification!=OriginalRoadClassification::None&&classification!=OriginalRoadClassification::GroundRoad))
        return BuildBlocker::UnsupportedTerrain;
    return BuildBlocker::None;
}
RampValidation World::validate_ramp(Cell a,Cell b) const {
    RampValidation v;
    auto block=[&](BuildBlocker reason,Cell cell,const char* text) {
        v.diagnostic={reason,cell,TransportEdge{a,b}};v.reason=text;return v;
    };
    if(!ramps_supported())return block(BuildBlocker::UnsupportedTerrain,a,"Ramps require explicit City-v16 rule 6");
    if(!in_bounds(a)||!in_bounds(b))return block(BuildBlocker::OutsideMap,!in_bounds(a)?a:b,"Ramp endpoint outside map");
    if(std::abs(a.x-b.x)+std::abs(a.y-b.y)!=1)return block(BuildBlocker::InvalidEdge,b,"Ramp must join two orthogonal neighbors");
    if(a.x>b.x||a.y>b.y)std::swap(a,b);
    for(const auto end:{a,b}) {
        const auto blocker=ramp_land_blocker(end);
        if(blocker!=BuildBlocker::None)return block(blocker,end,
            blocker==BuildBlocker::UnsupportedTerrain?"Unsupported cliff geometry":
            blocker==BuildBlocker::UnsupportedBridgeWater?"Water crossing requires Bridge":build_blocker_reason(blocker));
        if(!road_cell(end))return block(BuildBlocker::BridgeBankRoadRequired,end,"Road required at both ends");
        if(ramp_endpoint(end))return block(BuildBlocker::SandboxOccupied,end,
            ramp_between(a,b)?"Ramp already exists":"Ramp endpoints cannot share conflicting links");
        if(source_cell_ramp_claim(end))return block(BuildBlocker::SandboxOccupied,end,
            "Ramp endpoint overlaps source-cell ramp axial claim");
    }
    const int ha=permissions_->cell_height(a),hb=permissions_->cell_height(b);
    if(std::abs(ha-hb)!=1)return block(BuildBlocker::UnsupportedHeightTransition,b,
        ha==hb?"No height difference":"Unsupported height difference: exactly one signed level required");
    if(permissions_->transport_edge_blocker(a,b)!=BuildBlocker::UnsupportedHeightTransition)
        return block(permissions_->transport_edge_blocker(a,b),b,"Ramp cannot bypass a protected edge");
    v.link={{next_ramp_id_},a,b,std::int8_t(ha),std::int8_t(hb),ramp_cost,ticks_};v.geometry_valid=true;
    if(ramps_.size()+source_cell_ramps_.size()>=ramp_max_links||next_ramp_id_==UINT32_MAX||road_revision_==UINT64_MAX||
       command_sequence_>=UINT64_MAX-1||construction_spent_total_>UINT64_MAX-std::uint64_t(ramp_cost))
        return block(BuildBlocker::CounterExhausted,a,"Ramp construction counter exhausted");
    if(treasury_<ramp_cost)return block(BuildBlocker::InsufficientFunds,a,"Not enough funds");
    v.accepted=true;v.reason="Ramp ready: single level, $40; removal unavailable in V1";return v;
}
void World::restore_ramps(const WorldSnapshot& s) {
    if(!ramps_supported()) {
        if(!s.ramps.empty()||s.next_ramp_id!=1)throw std::invalid_argument("ramp authority requires rule6");
        return;
    }
    if(s.ramps.size()>ramp_max_links||s.next_ramp_id!=s.ramps.size()+1)throw std::invalid_argument("invalid ramp count/next ID");
    for(const auto& r:s.ramps) {
        if(r.id.value!=ramps_.size()+1||!in_bounds(r.endpoint_a)||!in_bounds(r.endpoint_b)||
           std::abs(r.endpoint_a.x-r.endpoint_b.x)+std::abs(r.endpoint_a.y-r.endpoint_b.y)!=1||
           r.endpoint_a.x>r.endpoint_b.x||r.endpoint_a.y>r.endpoint_b.y||
           std::abs(int(r.height_a)-int(r.height_b))!=1||r.paid_cost!=ramp_cost||r.placed_tick>s.ticks||
           permissions_->cell_height(r.endpoint_a)!=r.height_a||permissions_->cell_height(r.endpoint_b)!=r.height_b||
           permissions_->transport_edge_blocker(r.endpoint_a,r.endpoint_b)!=BuildBlocker::UnsupportedHeightTransition)
            throw std::invalid_argument("invalid saved ramp geometry/height/cost/tick/edge");
        // Normal road removal retains the link. Validate immutable land and
        // forbid replacement by a building; actual routing still needs roads.
        for(const auto end:{r.endpoint_a,r.endpoint_b})
            if(ramp_land_blocker(end)!=BuildBlocker::None||ramp_endpoint(end)||
               (object_at(end)!=Object::Empty&&object_at(end)!=Object::Road))
                throw std::invalid_argument("invalid saved ramp endpoint or overlapping claim");
        ramps_.push_back(r);index_ramp(ramps_.back());
    }
    next_ramp_id_=s.next_ramp_id;
}
std::optional<double> World::ramp_surface_height(Position position) const {
    if(!ramps_supported()||!std::isfinite(position.x)||!std::isfinite(position.y)||
       position.x<0||position.y<0||position.x>=width_||position.y>=height_)return {};
    const Cell low{int(std::floor(position.x)),int(std::floor(position.y))};
    const auto* r=ramp_endpoint(low);if(!r)return {};
    const bool x=r->endpoint_a.x!=r->endpoint_b.x;
    if((x&&position.y!=r->endpoint_a.y)||(!x&&position.x!=r->endpoint_a.x))return {};
    const double t=x?position.x-r->endpoint_a.x:position.y-r->endpoint_a.y;
    if(t<0||t>1)return {};
    return double(r->height_a)+(int(r->height_b)-int(r->height_a))*t;
}
} // namespace openemperor::simulation
