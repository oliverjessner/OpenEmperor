#include "app/SandboxView.h"
#include "app/WalkerAnimationSelection.h"
#include "app/WalkerPose.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <string>

namespace openemperor {
namespace {
bool crop_object(simulation::Object kind) {
    return kind==simulation::Object::OriginalHempField ||
        kind==simulation::Object::OriginalWheatField || kind==simulation::Object::OriginalMilletField ||
        kind==simulation::Object::OriginalRiceField || kind==simulation::Object::OriginalCabbageField ||
        kind==simulation::Object::OriginalSoybeanField || simulation::original_industry_plant(kind);
}
const char* actor_kind(AgricultureActorKind kind) {
    constexpr std::array names{"Food Grower","Hemp Grower","Plantation Grower","Food Courier",
        "Fishing Boat","Hunter","Wild Pig","Pheasant","Antelope"};
    return names.at(static_cast<std::size_t>(kind));
}
const char* actor_action(AgricultureActorAction action) {
    constexpr std::array names{"Standing","Walking","Waiting","Working","Returning","Unloading"};
    return names.at(static_cast<std::size_t>(action));
}
}

void SandboxView::prepare_agriculture_visuals(bool force) {
    if(!world_ || !agriculture_reconfiguration()) {
        agriculture_visual_state_={};agriculture_actor_state_.clear();
        agriculture_actor_meshes_.clear();agriculture_building_meshes_.clear();
        agriculture_world_objects_.clear();agriculture_routes_.clear();
        agriculture_object_revision_=agriculture_visual_tick_=agriculture_visual_sequence_=UINT64_MAX;
        return;
    }
    if(!force && agriculture_visual_tick_==world_->ticks() &&
       agriculture_visual_sequence_==world_->command_sequence())return;
    agriculture_visual_state_=agriculture_visual::prepare(*world_,agriculture_inspection_workers_);
    agriculture_actor_state_.prepare(*world_);
    auto previous_actors=std::move(agriculture_actor_meshes_);
    agriculture_actor_meshes_.clear();agriculture_actor_meshes_.reserve(AgricultureActorPresentationCache::capacity);
    for(const auto& actor:agriculture_actor_state_.entries()) {
        const auto old=std::find_if(previous_actors.begin(),previous_actors.end(),[&](const auto& row){
            return row.actor.domain==actor.domain && row.actor.id==actor.id && row.actor.pose==actor.pose;
        });
        const auto mesh=old==previous_actors.end() ?
            std::make_shared<const AgricultureMesh>(prepare_agriculture_actor_mesh(actor.pose)):old->mesh;
        agriculture_actor_meshes_.push_back({actor,mesh});
    }
    auto previous_buildings=std::move(agriculture_building_meshes_);
    agriculture_building_meshes_.clear();agriculture_building_meshes_.reserve(agriculture_visual_state_.count);
    for(std::size_t i=0;i<agriculture_visual_state_.count;++i) {
        const auto& state=agriculture_visual_state_.rows[i];
        const auto side=static_cast<unsigned>(simulation::building_footprint(rules_,world_->rule_version(),state.kind).width);
        const auto old=std::find_if(previous_buildings.begin(),previous_buildings.end(),[&](const auto& row){
            return row.id==state.id && row.kind==state.kind && row.side==side &&
                row.progress==state.progress && row.target==state.target && row.wet==state.wet;
        });
        auto label=std::string(agriculture_visual::phase_name(state.phase));
        if(state.target)label+=" "+std::to_string(state.progress)+"/"+std::to_string(state.target);
        const auto mesh=state.kind==simulation::Object::OriginalHuntersTent ? std::shared_ptr<const AgricultureMesh>{}:
            old!=previous_buildings.end() ? old->mesh:
            std::make_shared<const AgricultureMesh>(crop_object(state.kind) ?
                prepare_agriculture_crop_mesh(state.kind,state.progress,state.target,state.wet):
                prepare_agriculture_building_mesh(state.kind,static_cast<int>(side),state.progress%4,state.wet));
        agriculture_building_meshes_.push_back({state.id,state.kind,side,state.progress,state.target,state.wet,mesh,std::move(label)});
    }
    agriculture_routes_.clear();agriculture_routes_.reserve(AgricultureActorPresentationCache::capacity);
    for(const auto& actor:agriculture_actor_state_.entries()) {
        if(actor.domain!=AgricultureActorDomain::Courier || actor.pose.cargo<=0)continue;
        const auto& courier=world_->courier(static_cast<simulation::CourierId>(actor.id));
        if(courier.path_vertex>=courier.path.size() || courier.path_vertex+1>=courier.path.size())continue;
        AgricultureRouteVisual route;route.source=courier.owner;
        route.destination=courier.phase==simulation::CourierPhase::Returning ? courier.owner:courier.target;
        route.total_edges=courier.path.size()-courier.path_vertex-1;
        const auto shown=std::min<std::size_t>(256,route.total_edges);route.segments.reserve(shown);
        for(std::size_t i=0;i<shown;++i) {
            const auto a=courier.path[courier.path_vertex+i],b=courier.path[courier.path_vertex+i+1];
            route.segments.emplace_back(i ? world_for({double(a.x),double(a.y)}):world_for(actor.position),
                world_for({double(b.x),double(b.y)}));
        }
        agriculture_routes_.push_back(std::move(route));
    }
    if(force || agriculture_object_revision_!=world_->road_revision()) {
        agriculture_world_objects_.clear();
        agriculture_world_objects_.reserve(static_cast<std::size_t>(world_->width())*static_cast<std::size_t>(world_->height()));
        for(int y=0;y<world_->height();++y)for(int x=0;x<world_->width();++x) {
            const simulation::Cell cell{x,y};const auto kind=world_->object_at(cell);
            if(kind==simulation::Object::Empty)continue;
            if(kind==simulation::Object::Road && world_->source_cell_ramp_source_at(cell))continue;
            const auto owner=world_->building_owner_at(cell);
            if(kind!=simulation::Object::Road && owner && world_->building(*owner).cell!=cell)continue;
            const auto id=owner ? static_cast<std::uint32_t>(*owner):static_cast<std::uint32_t>(y*world_->width()+x);
            const auto mask=kind==simulation::Object::Road ? sandbox_ui::road_neighbor_mask(*world_,cell):0;
            agriculture_world_objects_.push_back({cell,kind,id,static_cast<std::uint8_t>(mask)});
        }
        agriculture_object_revision_=world_->road_revision();
        const auto cells=static_cast<std::size_t>(world_->width())*static_cast<std::size_t>(world_->height());
        if(road_ground_replacements_.size()!=cells) {
            road_ground_replacements_.assign(cells,0);agriculture_road_replacement_cells_.clear();
        }
        draw_instances_.reserve(cells+262U);
        // Wildlife and Boats/Hunters have real channels without Courier rows.
        // Even an empty paid city can already contain all twelve animals.
        visual_hits_.reserve(3U*(world_->buildings().size()+world_->couriers().size()+
            AgricultureActorPresentationCache::capacity+2U));
        agriculture_road_replacement_cells_.reserve(agriculture_world_objects_.size()+original_road_visuals_.size()+256U);
    }
    agriculture_visual_tick_=world_->ticks();agriculture_visual_sequence_=world_->command_sequence();
    ++agriculture_visual_prepare_count_;
}

std::shared_ptr<const AgricultureMesh> SandboxView::agriculture_actor_mesh_owner(AgricultureActorDomain domain,std::uint64_t id) const {
    const auto found=std::find_if(agriculture_actor_meshes_.begin(),agriculture_actor_meshes_.end(),
        [&](const auto& row){return row.actor.domain==domain && row.actor.id==id;});
    return found==agriculture_actor_meshes_.end() ? nullptr:found->mesh;
}
const AgricultureMesh* SandboxView::agriculture_actor_mesh(AgricultureActorDomain domain,std::uint64_t id) const {
    const auto found=std::find_if(agriculture_actor_meshes_.begin(),agriculture_actor_meshes_.end(),
        [&](const auto& row){return row.actor.domain==domain && row.actor.id==id;});
    return found==agriculture_actor_meshes_.end() ? nullptr:found->mesh.get();
}
std::shared_ptr<const AgricultureMesh> SandboxView::agriculture_building_mesh(simulation::BuildingId id) const {
    const auto found=std::find_if(agriculture_building_meshes_.begin(),agriculture_building_meshes_.end(),
        [&](const auto& row){return row.id==id;});
    return found==agriculture_building_meshes_.end() ? nullptr:found->mesh;
}
std::uint8_t SandboxView::agriculture_road_mask(simulation::Cell cell) const {
    const auto found=std::lower_bound(agriculture_world_objects_.begin(),agriculture_world_objects_.end(),cell,
        [](const auto& row,simulation::Cell key){return row.cell.y<key.y || (row.cell.y==key.y && row.cell.x<key.x);});
    if(found!=agriculture_world_objects_.end() && found->cell==cell && found->kind==simulation::Object::Road)return found->road_mask;
    const auto index=original_road_visual_index(cell);
    return index ? original_road_visuals_[*index].mask:0;
}

bool SandboxView::draw_agriculture_state(simulation::Cell cell,scene::Point ground) {
    if(!agriculture_reconfiguration())return true;
    const auto owner=world_->building_owner_at(cell);if(!owner)return true;
    const auto* state=agriculture_visual_state_.find(*owner);if(!state)return true;
    const auto z=camera_.zoom;
    const auto color=state->burning || state->blocked ? SDL_Color{201,104,69,255}:
        state->phase==agriculture_visual::Phase::Ready || state->phase==agriculture_visual::Phase::Harvesting ?
            SDL_Color{232,193,84,255}:state->wet ? SDL_Color{85,178,204,255}:SDL_Color{128,178,98,255};
    if(state->target) {
        const SDL_FRect backing{float(ground.x-15*z),float(ground.y+5*z),float(30*z),float(3*z)};
        auto fill=backing;fill.w*=std::clamp(float(state->progress)/float(state->target),0.F,1.F);
        if(!SDL_SetRenderDrawColor(renderer_,51,48,40,255) || !SDL_RenderFillRect(renderer_,&backing) ||
           !SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) || !SDL_RenderFillRect(renderer_,&fill))return false;
    }
    if(state->working || state->blocked || state->draining) {
        const SDL_FRect status{float(ground.x+17*z),float(ground.y+3*z),float(3*z),float(5*z)};
        if(!SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) || !SDL_RenderFillRect(renderer_,&status))return false;
    }
    const auto selected_owner=selected_ && !selected_landscape_ ? world_->building_owner_at(*selected_):std::nullopt;
    if(selected_owner==owner) {
        const auto row=std::find_if(agriculture_building_meshes_.begin(),agriculture_building_meshes_.end(),
            [&](const auto& value){return value.id==*owner;});
        if(row!=agriculture_building_meshes_.end()) {
            if(!SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) ||
               !SDL_RenderDebugText(renderer_,float(ground.x-28*z),float(ground.y+12*z),row->label.c_str()))return false;
        }
    }
    return true;
}
bool SandboxView::draw_agriculture_routes() {
    if(!agriculture_reconfiguration() || !selected_ || selected_landscape_)return true;
    const auto owner=world_->building_owner_at(*selected_);if(!owner)return true;
    if(!SDL_SetRenderDrawColor(renderer_,90,183,201,185))return false;
    for(const auto& route:agriculture_routes_)if(route.destination==*owner) {
        for(const auto& [a,b]:route.segments) {
            const auto from=camera_.world_to_screen(a),to=camera_.world_to_screen(b);
            if(!SDL_RenderLine(renderer_,float(from.x),float(from.y),float(to.x),float(to.y)))return false;
        }
    }
    return true;
}

SandboxView::AgricultureDisplayStats SandboxView::agriculture_display_stats() const {
    AgricultureDisplayStats stats;stats.active=world_ && agriculture_reconfiguration();
    stats.cached_entities=agriculture_visual_state_.count;stats.cached_actors=agriculture_actor_state_.entries().size();
    stats.prepared_meshes=agriculture_actor_meshes_.size()+static_cast<std::size_t>(std::count_if(
        agriculture_building_meshes_.begin(),agriculture_building_meshes_.end(),[](const auto& row){return bool(row.mesh);}));
    // Count unique prepared meshes, including a submitted frame retained across
    // the next publication. This diagnostic is outside frame/hover execution.
    std::vector<const AgricultureMesh*> meshes;meshes.reserve(stats.prepared_meshes+visual_hits_.size());
    for(const auto& row:agriculture_actor_meshes_)meshes.push_back(row.mesh.get());
    for(const auto& row:agriculture_building_meshes_)if(row.mesh)meshes.push_back(row.mesh.get());
    for(const auto& hit:visual_hits_)if(hit.agriculture_mesh)meshes.push_back(hit.agriculture_mesh.get());
    std::sort(meshes.begin(),meshes.end(),std::less<const AgricultureMesh*>{});
    meshes.erase(std::unique(meshes.begin(),meshes.end()),meshes.end());
    stats.cpu_mesh_bytes=meshes.size()*sizeof(AgricultureMesh);
    stats.prepare_count=agriculture_visual_prepare_count_;
    stats.prepared_tick=agriculture_visual_tick_;stats.prepared_sequence=agriculture_visual_sequence_;return stats;
}

std::optional<std::pair<SandboxView::AgricultureVisualDiagnostic::Domain,std::uint64_t>> SandboxView::selected_agriculture_actor() const {
    if(!selected_agriculture_actor_ || !agriculture_reconfiguration())return std::nullopt;
    const auto [domain,id]=*selected_agriculture_actor_;
    const auto present=std::any_of(agriculture_actor_state_.entries().begin(),agriculture_actor_state_.entries().end(),[&](const auto& actor){
        const auto matches=domain==AgricultureVisualDiagnostic::Domain::Courier ? actor.domain==AgricultureActorDomain::Courier:
            domain==AgricultureVisualDiagnostic::Domain::Boat ? actor.domain==AgricultureActorDomain::Boat:
            domain==AgricultureVisualDiagnostic::Domain::Hunter ? actor.domain==AgricultureActorDomain::Hunter:
            domain==AgricultureVisualDiagnostic::Domain::Animal && actor.domain==AgricultureActorDomain::Animal;
        return matches && actor.id==id && actor.lifetime_id==selected_agriculture_lifetime_;
    });
    return present ? selected_agriculture_actor_:std::nullopt;
}

std::vector<SandboxView::AgricultureVisualDiagnostic> SandboxView::agriculture_visual_diagnostics() const {
    std::vector<AgricultureVisualDiagnostic> rows;
    if(!world_ || !agriculture_reconfiguration())return rows;
    const auto submitted=[&](AgricultureVisualDiagnostic& row) {
        const auto found=std::find_if(visual_hits_.rbegin(),visual_hits_.rend(),[&](const auto& hit){
            switch(row.domain) {
            case AgricultureVisualDiagnostic::Domain::Courier:return hit.walker && static_cast<std::uint64_t>(*hit.walker)==row.id &&
                hit.agriculture_lifetime==row.lifetime_id;
            case AgricultureVisualDiagnostic::Domain::Boat:return hit.boat && hit.boat->value==row.id;
            case AgricultureVisualDiagnostic::Domain::Hunter:return hit.hunter && hit.hunter->value==row.id;
            case AgricultureVisualDiagnostic::Domain::Animal:return hit.animal && hit.animal->value==row.id;
            case AgricultureVisualDiagnostic::Domain::Building:return hit.agriculture_building &&
                static_cast<std::uint64_t>(*hit.agriculture_building)==row.id;
            }
            return false;
        });
        if(found==visual_hits_.rend())return;
        row.submitted=true;row.key=found->key;row.sprite_drawn=found->image.has_value();
        row.image_origin=visual_hit_camera_.world_to_screen(found->origin);
        row.image_width=found->width*visual_hit_camera_.zoom;row.image_height=found->height*visual_hit_camera_.zoom;
        row.screen_ground=visual_hit_camera_.world_to_screen(found->mesh_ground);
        if(found->image) {
            if(found->walker && walker_profile_) {
                for(const auto& role:walker_profile_->roles)if(role)for(const auto& frame:role->frames)
                    if(frame.image_index==*found->image)row.asset=frame.id;
            } else if(building_profile_) {
                const auto inspect=[&](const auto& entries){for(const auto& entry:entries)
                    if(entry && entry->image_index==*found->image)row.asset=entry->id;};
                inspect(building_profile_->entries);inspect(building_profile_->original_house_entries);
                inspect(building_profile_->original_building_entries);inspect(building_profile_->irrigation_ditch_entries);
                inspect(building_profile_->irrigation_pump_entries);inspect(building_profile_->fishing_quay_entries);
                inspect(building_profile_->original_rice_entries);
                for(const auto& entries:building_profile_->original_field_entries)inspect(entries);
                for(const auto& entries:building_profile_->plantation_entries)inspect(entries);
            }
        }
        row.art=row.sprite_drawn ? "Prepared Original pixels; authored display mapping":"AUTHORED procedural fallback";
    };
    rows.reserve(agriculture_visual_state_.count+agriculture_actor_state_.entries().size());
    for(std::size_t i=0;i<agriculture_visual_state_.count;++i) {
        const auto& state=agriculture_visual_state_.rows[i];AgricultureVisualDiagnostic row;
        row.id=static_cast<std::uint64_t>(state.id);row.source=state.source;row.object=state.kind;
        row.kind=simulation::starter_building_name(state.kind);
        row.state=agriculture_visual::phase_name(state.phase);row.position={double(state.cell.x),double(state.cell.y)};
        row.progress=state.progress;row.target_progress=state.target;row.cargo=state.cargo;row.wet=state.wet;
        row.harvest_progress=state.harvest_progress;row.harvest_target=state.harvest_target;
        row.working=state.working;row.blocked=state.blocked;
        row.screen_ground=camera_.world_to_screen(building_visual_ground(state.cell,state.kind));submitted(row);rows.push_back(std::move(row));
    }
    for(const auto& actor:agriculture_actor_state_.entries()) {
        AgricultureVisualDiagnostic row;
        switch(actor.domain) {
        case AgricultureActorDomain::Courier:row.domain=AgricultureVisualDiagnostic::Domain::Courier;break;
        case AgricultureActorDomain::Boat:row.domain=AgricultureVisualDiagnostic::Domain::Boat;break;
        case AgricultureActorDomain::Hunter:row.domain=AgricultureVisualDiagnostic::Domain::Hunter;break;
        case AgricultureActorDomain::Animal:row.domain=AgricultureVisualDiagnostic::Domain::Animal;break;
        }
        row.id=actor.id;row.lifetime_id=actor.lifetime_id;row.source=actor.owner;row.target=actor.target;row.kind=actor_kind(actor.pose.kind);
        row.state=actor_action(actor.pose.action);row.position=actor.position;row.cargo=actor.pose.cargo;
        row.progress=actor.pose.phase;row.target_progress=4;row.moving=actor.pose.moving;row.working=actor.pose.working;
        row.dx=actor.pose.dx;row.dy=actor.pose.dy;
        row.screen_ground=camera_.world_to_screen(world_for(actor.position));submitted(row);rows.push_back(std::move(row));
    }
    return rows;
}
} // namespace openemperor
