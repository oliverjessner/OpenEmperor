#include "simulation/World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
bool terminal(const GrowerNavigationTrack& track) {
    return !track.edge_started && track.result!=GrowerNavigationResult::Moving &&
        track.result!=GrowerNavigationResult::Retry;
}
bool food_field(Object kind) {
    return kind==Object::OriginalWheatField || kind==Object::OriginalMilletField ||
        kind==Object::OriginalRiceField || kind==Object::OriginalCabbageField ||
        kind==Object::OriginalSoybeanField;
}
bool footprint_contains(const World& world,const BuildingState& body,Cell cell) {
    const auto cells=building_footprint_cells(world.profile(),world.rule_version(),body.kind,body.cell);
    return std::find(cells.begin(),cells.end(),cell)!=cells.end();
}
bool complete_body(const World& world,const BuildingState& body) {
    if(!body.placed) return false;
    for(const auto cell:building_footprint_cells(world.profile(),world.rule_version(),body.kind,body.cell))
        if(!world.in_bounds(cell) || world.building_owner_at(cell)!=body.id || world.object_at(cell)!=body.kind) return false;
    return true;
}
bool external_source_cell(const World& world,const BuildingState& farm,Cell cell) {
    if(footprint_contains(world,farm,cell)) return false;
    for(const auto occupied:building_footprint_cells(world.profile(),world.rule_version(),farm.kind,farm.cell))
        if(std::abs(occupied.x-cell.x)+std::abs(occupied.y-cell.y)==1) return true;
    return false;
}
} // namespace

bool World::prepare_grower_navigation_source() {
    if(grower_navigation_source_prepared_) return grower_navigation_no_regions_;
    // Explicit request-time preparation, never a normal tick/frame scan. This
    // strict domain proves an empty region grid after the qualified native
    // rebuild. It admits neither imported native saves nor height regions.
    if(rule_version_!=kOriginalIrrigationRuleVersion || !irrigation_enabled() || !original_terrain_facts_ || !original_terrain_facts_->irrigation_complete() ||
        !permissions_ || width_!=OriginalTerrainFacts::width || height_!=OriginalTerrainFacts::height) return false;
    bool none=true;
    for(int y=0;y<height_;++y) for(int x=0;x<width_;++x)
        none=none && !(original_terrain_facts_->raw_terrain({x,y})&0x10000U);
    grower_navigation_no_regions_=none;grower_navigation_source_prepared_=true;
    return none;
}

std::optional<std::uint16_t> World::original_grower_navigation_mask(Cell cell) const {
    if(!in_bounds(cell)) return 0;
    if(!grower_navigation_source_prepared_ || !grower_navigation_no_regions_ || !original_terrain_facts_) return {};
    if(!original_terrain_facts_->irrigation_boundary_allowed(cell)) return 0;
    const auto* permission=permissions_->cell_permission(cell);
    if(!permission || permission->protected_original || permission->height!=0 ||
        (!permission->road_allowed && !permission->building_allowed) || fixed_passage(cell) || bridge_at(cell)) return {};
    if(!ramps_.empty() || !source_cell_ramps_.empty()) return {};
    if(irrigation_) {
        const auto ditch=std::lower_bound(irrigation_->ditches.begin(),irrigation_->ditches.end(),cell,
            [](const auto& row,Cell key){return row.cell.y!=key.y ? row.cell.y<key.y:row.cell.x<key.x;});
        if(ditch!=irrigation_->ditches.end() && ditch->cell==cell) return {};
    }
    const auto raw=original_terrain_facts_->raw_terrain(cell);
    if(raw!=0 && raw!=0x80U && raw!=0x40U && raw!=0xc0U) return {};
    if(const auto owner=building_owner_at(cell)) {
        const auto& body=building(*owner);
        if(!complete_body(*this,body)) return {};
        if(body.kind==Object::OriginalFoodFarm) return 0x02;
        if(food_field(body.kind) && (raw==0 || raw==0x80U)) return 0x10;
        return {};
    }
    if(object_at(cell)==Object::Road || original_road(cell)) return 0x04;
    if(object_at(cell)!=Object::Empty || raw==0x40U || raw==0xc0U) return {};
    return 0x10;
}

bool World::original_grower_navigation_edge_allowed(Cell from,Cell to) const {
    const auto dx=std::abs(from.x-to.x),dy=std::abs(from.y-to.y);
    if((!dx && !dy) || dx>1 || dy>1) return false;
    const auto a=original_grower_navigation_mask(from),b=original_grower_navigation_mask(to);
    return a && b && (*a&0x13dU) && (*b&0x13dU);
}

bool World::grower_navigation_cell_claimed(Cell cell,std::optional<GrowerNavigationId> except) const {
    for(const auto& track:grower_navigation_tracks_) {
        if((except && track.id==*except) || (!track.lifecycle_owned && terminal(track))) continue;
        if(track.current==cell) return true;
        if(track.edge_started && track.cursor && track.cursor<track.path.size() &&
            (track.path[track.cursor-1]==cell || track.path[track.cursor]==cell)) return true;
    }
    return false;
}

GrowerNavigationAdmission World::start_grower_navigation(const GrowerNavigationRequest& request) {
    const auto deny=[](GrowerNavigationResult result,const char* reason) {
        return GrowerNavigationAdmission{false,{},result,reason};
    };
    if(grower_lifecycle_) return deny(GrowerNavigationResult::Unsupported,"Autonomous lifecycle owns its movement channels");
    if(request.policy!=GrowerNavigationPolicy::Mode17MicrostepV1 ||
        request.interaction!=GrowerNavigationInteraction::None)
        return deny(GrowerNavigationResult::Unsupported,"Native peer, suspension, ferry, link and exceptional/global contexts are unsupported");
    if(!in_bounds(request.start) || !in_bounds(request.target))
        return deny(GrowerNavigationResult::Failed,"Navigation endpoint is outside the World");
    if(!prepare_grower_navigation_source()) return deny(GrowerNavigationResult::Unsupported,"Navigation requires complete Rule26 source facts without native height-region markers");
    if(!next_grower_navigation_id_ || next_grower_navigation_id_==UINT64_MAX ||
        couriers_.size()+grower_navigation_tracks_.size()>=Rules::original_production_courier_limit)
        return deny(GrowerNavigationResult::Failed,"Shared movement-channel budget exhausted");
    const auto source=std::find_if(buildings_.begin(),buildings_.end(),[&](const auto& b){return b.id==request.farm_id;});
    const auto target=std::find_if(buildings_.begin(),buildings_.end(),[&](const auto& b){return b.id==request.field_id;});
    if(source==buildings_.end() || target==buildings_.end() || source->kind!=Object::OriginalFoodFarm ||
        !food_field(target->kind) || !complete_body(*this,*source) || !complete_body(*this,*target))
        return deny(GrowerNavigationResult::Failed,"Navigation requires complete paid Farm193 and Food-field identities");
    const auto field=std::find_if(agriculture_->fields.begin(),agriculture_->fields.end(),[&](const auto& row){return row.field_id==target->id;});
    if(field==agriculture_->fields.end() || field->farm_id!=source->id ||
        (!request.returning && (request.target!=target->cell || !external_source_cell(*this,*source,request.start))) ||
        (request.returning && (request.start!=target->cell || !external_source_cell(*this,*source,request.target))))
        return deny(GrowerNavigationResult::Failed,"Source/target cells do not match their paid full footprints and explicit external endpoint");
    if(!source->operating_enabled || !target->operating_enabled || building_on_fire(source->id) || building_on_fire(target->id))
        return deny(GrowerNavigationResult::Unsupported,"Source operation/fire lifecycle is outside motion-only acceptance");
    if(std::any_of(couriers_.begin(),couriers_.end(),[](const auto& c){return c.enabled && c.phase!=CourierPhase::IdleAtWorkshop;}) ||
        grower_navigation_cell_claimed(request.start))
        return deny(GrowerNavigationResult::Unsupported,"Native actor interactions are not admitted");
    auto result=GrowerNavigationResult::Failed;
    auto path=find_road_route(request.start,request.target,{},{},true,&result);
    if(!path) return deny(result,"No complete supported mode17 route");
    GrowerNavigationTrack track;track.id={next_grower_navigation_id_};track.request=request;
    track.current=request.start;track.path=std::move(*path);track.route_revision=road_revision_;
    if(request.start==request.target) track.result=GrowerNavigationResult::Arrived;
    const auto id=track.id;result=track.result;
    grower_navigation_tracks_.push_back(std::move(track));++next_grower_navigation_id_;
    return {true,id,result,"Explicit motion-only mode17 session admitted"};
}

const GrowerNavigationTrack& World::grower_navigation(GrowerNavigationId id) const {
    const auto found=std::find_if(grower_navigation_tracks_.begin(),grower_navigation_tracks_.end(),[&](const auto& row){return row.id==id;});
    if(found==grower_navigation_tracks_.end()) throw std::out_of_range("Unknown navigation session identity");
    return *found;
}
bool World::clear_grower_navigation() {
    if(grower_lifecycle_) return false;
    if(std::any_of(grower_navigation_tracks_.begin(),grower_navigation_tracks_.end(),[](const auto& row){return !terminal(row);})) return false;
    grower_navigation_tracks_.clear();return true;
}

void World::advance_grower_navigation() {
    for(auto& track:grower_navigation_tracks_)
        if(!track.lifecycle_owned) advance_grower_navigation_track(track);
}

void World::advance_grower_navigation_track(GrowerNavigationTrack& track) {
    if(terminal(track)) return;
    const auto stop=[&](GrowerNavigationResult result,const char* reason) {track.result=result;track.blocker=reason;};
    const auto current_mask=original_grower_navigation_mask(track.current);
    if(!current_mask || !(*current_mask&0x13dU)) track.unsupported_context_seen=true;
    if(track.edge_started) {
        const auto destination_mask=original_grower_navigation_mask(track.path.at(track.cursor));
        if(!destination_mask || !(*destination_mask&0x13dU)) track.unsupported_context_seen=true;
    }
    const auto& farm=building(track.request.farm_id);const auto& field=building(track.request.field_id);
    if(!farm.placed || !field.placed || !farm.operating_enabled || !field.operating_enabled ||
        building_on_fire(farm.id) || building_on_fire(field.id) ||
        std::any_of(couriers_.begin(),couriers_.end(),[](const auto& c){return c.enabled && c.phase!=CourierPhase::IdleAtWorkshop;}))
        track.unsupported_context_seen=true;
    if(track.unsupported_context_seen && !track.edge_started) {
        stop(GrowerNavigationResult::Unsupported,"Actor/source interaction outside admitted motion policy");return;
    }
    if(track.result==GrowerNavigationResult::Retry) {
        // A real direction9 releases the old route. Rebuild from the actual
        // committed cell; no supplied movement8 or target proximity grant.
        auto outcome=GrowerNavigationResult::Failed;
        auto route=find_road_route(track.current,track.request.target,{},{},true,&outcome);
        ++track.reroutes;track.route_revision=road_revision_;
        if(!route) {stop(outcome,"Retry has no complete supported route");return;}
        track.path=std::move(*route);track.cursor=0;track.phase=20;track.edge_started=false;
        track.result=GrowerNavigationResult::Moving;track.blocker.clear();
    }
    ++track.microsteps;
    if(track.phase<19) {
        ++track.phase;
        if(track.phase==10) track.current=track.path.at(track.cursor);
        return;
    }
    // Phase20 is a boundary, including preparation's initial phase20.
    track.phase=20;track.edge_started=false;
    if(track.unsupported_context_seen) {
        stop(GrowerNavigationResult::Unsupported,"Actor/source interaction outside admitted motion policy");return;
    }
    if(track.cursor+1>=track.path.size()) {
        stop(track.current==track.request.target ? GrowerNavigationResult::Arrived:GrowerNavigationResult::Failed,
             track.current==track.request.target ? "Endpoint reached by actual cell movement":"Route endpoint mismatch");
        return;
    }
    const auto next=track.path.at(track.cursor+1);
    if(grower_navigation_cell_claimed(next,track.id)) {
        stop(GrowerNavigationResult::Unsupported,"Peer interaction is unsupported");return;
    }
    if(!original_grower_navigation_edge_allowed(track.current,next)) {
        stop(GrowerNavigationResult::Retry,"Direction9: next edge invalidated");return;
    }
    ++track.cursor;track.phase=0;track.edge_started=true;track.route_revision=road_revision_;
}
} // namespace openemperor::simulation
