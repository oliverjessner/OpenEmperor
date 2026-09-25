#include "app/SandboxView.h"

#include "maps/SandboxPlacement.h"
#include "renderer/StoredCamera.h"
#include "app/WalkerPose.h"
#include "app/SandboxVisualOrder.h"
#include "core/Version.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor {
namespace {
const char* tool_name(simulation::RulesProfile rules,int tool) {
    if (simulation::production_profile(rules)) {
        switch (tool) {
        case 1: return "Road";
        case 2: return "Clay source";
        case 3: return "Pottery";
        case 4: return "Warehouse";
        case 6: return "Remove road";
        case 7: return "Household";
        case 8: return "Farm";
        case 9: return "Service post";
        default: return "Select";
        }
    }
    switch (tool) {
    case 1: return "Road";
    case 2: return "Workshop";
    case 3: return "Warehouse";
    default: return "Select";
    }
}
const char* object_name(simulation::Object object) {
    switch (object) {
    case simulation::Object::Empty: return "Empty";
    case simulation::Object::Road: return "Road";
    case simulation::Object::Workshop: return "Workshop";
    case simulation::Object::Warehouse: return "Warehouse";
    case simulation::Object::ClaySource: return "Clay source";
    case simulation::Object::Pottery: return "Pottery";
    case simulation::Object::Household: return "Household";
    case simulation::Object::Farm: return "Farm";
    case simulation::Object::ServicePost: return "Service post";
    }
    return "Unknown";
}
simulation::CommandType command_type(simulation::RulesProfile rules,int tool) {
    if (simulation::production_profile(rules)) {
        switch (tool) {
        case 1: return simulation::CommandType::PlaceRoad;
        case 2: return simulation::CommandType::PlaceClaySource;
        case 3: return simulation::CommandType::PlacePottery;
        case 4: return simulation::CommandType::PlaceWarehouse;
        case 6: return simulation::CommandType::RemoveRoad;
        case 8: return simulation::CommandType::PlaceFarm;
        case 9: return simulation::CommandType::PlaceServicePost;
        default: return simulation::CommandType::PlaceHousehold;
        }
    }
    switch (tool) {
    case 1: return simulation::CommandType::PlaceRoad;
    case 2: return simulation::CommandType::PlaceWorkshop;
    default: return simulation::CommandType::PlaceWarehouse;
    }
}
const char* profile_title(simulation::RulesProfile rules) {
    switch (rules) {
    case simulation::RulesProfile::LogisticsV1: return "Logistics v1";
    case simulation::RulesProfile::ProductionV2: return "Production v2";
    case simulation::RulesProfile::HouseholdV3: return "Household v3";
    case simulation::RulesProfile::SettlementV4: return "Settlement v4";
    case simulation::RulesProfile::IndustryV5: return "Industry v5";
    case simulation::RulesProfile::CityV6: return "City v6";
    case simulation::RulesProfile::CityV7: return "City v7";
    case simulation::RulesProfile::CityV8: return "City v8";
    case simulation::RulesProfile::CityV9: return "City v9";
    case simulation::RulesProfile::CityV10: return "City v10";
    }
    return "Sandbox";
}
}

SandboxView::SandboxView(maps::StoredMapSession session,bool demo,simulation::RulesProfile rules)
    : geometry_(session.map.declared_map_size),background_(std::move(session.plan)),demo_(demo),
      rules_(rules) {
    if (!geometry_.supported) throw std::invalid_argument("sandbox requires supported map geometry");
    if (simulation::production_profile(rules_)) tool_=5;
}
SandboxView::~SandboxView() { shutdown(); }

void SandboxView::configure_save(std::filesystem::path root,std::filesystem::path map,
                                 std::filesystem::path save,
                                 std::optional<persistence::SaveDocument> initial) {
    data_root_=std::move(root); map_relative_=std::move(map); save_path_=std::move(save);
    initial_save_=std::move(initial);
}

void SandboxView::save_now() {
    if (save_path_.empty()) throw std::runtime_error("No sandbox save path configured");
    const auto document=persistence::make_document(data_root_,map_relative_,buildable_mask_,*world_);
    persistence::write_save(save_path_,document,data_root_,buildable_mask_);
    saved_tick_=world_->ticks(); saved_command_=world_->command_sequence();
    ++save_generation_;
    last_message_="Saved tick "+std::to_string(world_->ticks());
}

void SandboxView::load_now() {
    if (save_path_.empty()) throw std::runtime_error("No sandbox save path configured");
    persistence::validate_save_target(save_path_,data_root_);
    const auto document=persistence::read_save(save_path_);
    if (document.map_relative!=map_relative_ || document.world.profile!=rules_)
        throw std::runtime_error("Save map or rules differ from current sandbox");
    auto replacement=persistence::restore_save(document,data_root_,buildable_mask_);
    world_=std::make_unique<simulation::World>(std::move(replacement));
    saved_tick_=world_->ticks(); saved_command_=world_->command_sequence();
    cancel_gesture();
    if (selected_ && !world_->building_owner_at(*selected_)) selected_.reset();
    clock_.pause_and_reset();
    demo_origin_.reset();
    last_message_="Loaded tick "+std::to_string(world_->ticks())+" (paused)"+
        (document.migrated_from_schema1 ?
            (rules_==simulation::RulesProfile::ProductionV2 ?
                "; save schema 1 migrated to rules 2 in memory":
                "; save schema 1 migrated in memory") : "");
}

void SandboxView::initialize(SDL_Window* window,SDL_Renderer* renderer) {
    window_=window;
    renderer_=renderer;
    update_layout(false);
    background_.set_debug_diagnostics(debug_open_);
    background_.initialize(renderer_);
    std::vector<std::string> builtin_errors;
    if (!walker_manifest_.empty()) {
        const auto manifest=walker_manifest_; const auto source=walker_source_;
        walker_manifest_.clear();
        try { set_walker_visuals(manifest,source); }
        catch (const std::exception& error) {
            if (source!=VisualProfileSource::Builtin) throw;
            set_walker_visuals({});
            builtin_errors.push_back(std::string("Built-in walker preview could not be loaded; using markers: ")+error.what());
        }
    }
    if (!building_manifest_.empty()) {
        const auto manifest=building_manifest_; const auto source=building_source_;
        building_manifest_.clear();
        try { set_building_visuals(manifest,source); }
        catch (const std::exception& error) {
            if (source!=VisualProfileSource::Builtin) throw;
            set_building_visuals({});
            builtin_errors.push_back(std::string("Built-in building preview could not be loaded; using markers: ")+error.what());
        }
    }
    if (!road_manifest_.empty()) {
        const auto manifest=road_manifest_; const auto source=road_source_;
        road_manifest_.clear();
        try { set_road_visuals(manifest,source); }
        catch (const std::exception& error) {
            if (source!=VisualProfileSource::Builtin) throw;
            set_road_visuals({});
            builtin_errors.push_back(std::string("Built-in road preview could not be loaded; using fallback tiles: ")+error.what());
        }
    }
    buildable_mask_=maps::make_sandbox_buildable_mask(background_.plan(),geometry_);
    if (initial_save_) {
        world_=std::make_unique<simulation::World>(persistence::restore_save(*initial_save_,data_root_,
            buildable_mask_));
        clock_.pause_and_reset();
        last_message_="Loaded tick "+std::to_string(world_->ticks())+" (paused)"+
            (initial_save_->migrated_from_schema1 ? "; save schema 1 migrated in memory":"");
        initial_save_.reset();
    } else world_=std::make_unique<simulation::World>(maps::stored_grid_width,maps::stored_grid_height,
        buildable_mask_,rules_);
    reset_camera();
    if (demo_) place_demo();
    if (initial_save_ == std::nullopt && !demo_) {
        saved_tick_=world_->ticks(); saved_command_=world_->command_sequence();
    }
    if (!builtin_errors.empty()) last_message_=builtin_errors.front();
    const std::string title="OpenEmperor "+std::string(version::display)+" - "+profile_title(rules_);
    SDL_SetWindowTitle(window_,title.c_str());
}
void SandboxView::shutdown() {
    cancel_gesture();
    walker_diagnostic_open_=false;
    walker_sprites_.reset();
    walker_profile_.reset();
    building_sprite_.reset();
    building_profile_.reset();
    road_sprites_.reset();
    road_profile_.reset();
    background_.shutdown();
    world_.reset();
    window_=nullptr;
    renderer_=nullptr;
}
void SandboxView::set_walker_visuals(const std::filesystem::path& manifest,
                                     VisualProfileSource source) {
    if (manifest.empty()) {
        walker_sprites_.reset(); walker_profile_.reset(); walker_manifest_.clear();
        walker_source_=VisualProfileSource::Fallback;
        walker_diagnostic_open_=false;
        return;
    }
    if (!renderer_) { walker_manifest_=manifest; walker_source_=source; return; }
    if (!simulation::production_profile(rules_))
        throw std::runtime_error("walker visuals require a production sandbox profile");
    auto profile=assets::load_walker_visual_profile(data_root_,manifest);
    auto textures=std::make_unique<WalkerSpriteSet>();
    textures->initialize(renderer_,profile);
    walker_sprites_=std::move(textures);
    walker_profile_=std::move(profile);
    walker_manifest_=manifest;
    walker_source_=source;
    walker_visuals_enabled_=true;
    walker_role_stats_={};
    walker_moving_drawn_.fill(false);
    walker_unmapped_fallbacks_=walker_invalid_edge_fallbacks_=0;
    walker_diagnostic_role_=walker_diagnostic_direction_=walker_diagnostic_step_=0;
    last_message_="Curated walker previews active";
}
SandboxView::WalkerDisplayStats SandboxView::walker_display_stats() const {
    WalkerDisplayStats stats;
    if (walker_profile_) {
        stats.schema_version=walker_profile_->schema_version;
        for (std::size_t role=0;role<3;++role) {
            const auto& visual=walker_profile_->roles[role];
            stats.roles[role]=walker_role_stats_[role];
            stats.roles[role].configured=visual.has_value();
            if (visual) for (std::size_t direction=0;direction<4;++direction)
                stats.roles[role].directions_configured[direction]=
                    !visual->clips[direction].empty();
        }
        stats.configured=stats.roles[0].directions_configured;
        stats.decoded_assets=walker_profile_->unique_images.size();
        std::vector<bool> seen(stats.decoded_assets,false);
        for (const auto& visual:walker_profile_->roles) if (visual)
            for (const auto& frame:visual->frames)
                if (!seen.at(frame.image_index)) {
                    seen[frame.image_index]=true;
                    stats.decoded_frame_ids.push_back(frame.id);
                }
    }
    stats.texture_uploads=walker_texture_count();
    stats.moving_drawn=stats.roles[0].directions_drawn;
    stats.unmapped_fallbacks=walker_unmapped_fallbacks_;
    stats.invalid_edge_fallbacks=walker_invalid_edge_fallbacks_;
    return stats;
}
void SandboxView::set_building_visuals(const std::filesystem::path& manifest,
                                       VisualProfileSource source) {
    if (manifest.empty()) {
        building_sprite_.reset();building_profile_.reset();building_manifest_.clear();
        building_source_=VisualProfileSource::Fallback;
        return;
    }
    if (!renderer_) { building_manifest_=manifest;building_source_=source;return; }
    if (!simulation::production_profile(rules_))
        throw std::runtime_error("Pottery visuals require a production sandbox profile");
    auto profile=assets::load_building_visual_profile(data_root_,manifest);
    auto texture=std::make_unique<BuildingSprite>();
    texture->initialize(renderer_,profile);
    building_sprite_=std::move(texture);
    building_profile_=std::move(profile);
    building_manifest_=manifest;
    building_source_=source;
    building_enabled_=true;
    building_drawn_instances_.fill(0);building_placeholder_fallbacks_.fill(0);
    last_message_="Curated building preview active";
}
SandboxView::BuildingDisplayStats SandboxView::building_display_stats() const {
    BuildingDisplayStats stats;
    stats.configured=building_profile_.has_value();
    stats.decoded_assets=building_profile_ ? building_profile_->unique_images.size():0;
    stats.texture_uploads=building_texture_count();
    stats.drawn_instances=building_drawn_instances_;
    stats.placeholder_fallbacks=building_placeholder_fallbacks_;
    if (building_profile_)
        for (const auto role:assets::building_roles)
            stats.configured_roles[assets::role_index(role)]=building_profile_->find(role)!=nullptr;
    return stats;
}
void SandboxView::set_road_visuals(const std::filesystem::path& manifest,
                                   VisualProfileSource source) {
    if (manifest.empty()) {
        road_sprites_.reset();road_profile_.reset();road_manifest_.clear();
        road_source_=VisualProfileSource::Fallback;return;
    }
    if (!renderer_) { road_manifest_=manifest;road_source_=source;return; }
    auto profile=assets::load_road_visual_profile(data_root_,manifest);
    auto sprites=std::make_unique<RoadSpriteSet>();
    sprites->initialize(renderer_,profile);
    road_sprites_=std::move(sprites);
    road_profile_=std::move(profile);
    road_manifest_=manifest;
    road_source_=source;
    road_enabled_=true;
    road_masks_seen_.fill(false);road_draws_=road_fallbacks_current_=0;
    last_message_="Curated road preview active";
}
SandboxView::RoadDisplayStats SandboxView::road_display_stats() const {
    RoadDisplayStats stats;
    stats.configured=road_profile_.has_value();
    if (road_profile_) {
        for (std::size_t i=0;i<16;++i) stats.configured_masks[i]=road_profile_->tiles[i].has_value();
        stats.unique_assets=road_profile_->unique_images.size();
    }
    stats.texture_uploads=road_sprites_ ? road_sprites_->texture_count():0;
    stats.draws=road_draws_;stats.fallback_draws=road_fallbacks_current_;
    stats.masks_seen=road_masks_seen_;
    return stats;
}
void SandboxView::reset_camera() {
    fit_stored_camera(background_.plan(),camera_);
    camera_.offset.x+=layout_.map.x;
    camera_.offset.y+=layout_.map.y;
    if (demo_origin_) {
        const auto center=maps::terrain_world({static_cast<std::uint32_t>(demo_origin_->x+
            (simulation::industry_profile(rules_) ? 6 :
             rules_==simulation::RulesProfile::SettlementV4 ? 6 :
             rules_==simulation::RulesProfile::HouseholdV3 ? 5 :
             rules_==simulation::RulesProfile::ProductionV2 ? 3 : 2)),
            static_cast<std::uint32_t>(demo_origin_->y)},geometry_.border);
        camera_.zoom=std::clamp(camera_.zoom,1.0,4.0);
        camera_.center_on({center.x,center.y+20.0});
        camera_.offset.y+=layout_.map.y+std::min(75,layout_.map.h/5);
    }
    refresh_hover();
}
void SandboxView::resize_camera() {
    update_layout(true);
}
void SandboxView::update_layout(bool preserve_center) {
    int width=0,height=0,window_width=0,window_height=0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&width,&height))
        throw std::runtime_error(SDL_GetError());
    if (!SDL_GetWindowSize(window_,&window_width,&window_height))
        throw std::runtime_error(SDL_GetError());
    const auto next=sandbox_ui::make_layout(width,height,window_width,window_height,panel_open_);
    if (preserve_center && next.map.x==layout_.map.x && next.map.y==layout_.map.y &&
        next.map.w==layout_.map.w && next.map.h==layout_.map.h) return;
    scene::Point center{};
    if (preserve_center && layout_.map.w>0 && layout_.map.h>0)
        center=camera_.screen_to_world({layout_.map.x+layout_.map.w*0.5,
                                       layout_.map.y+layout_.map.h*0.5});
    layout_=next;
    camera_.viewport_width=layout_.map.w;
    camera_.viewport_height=layout_.map.h;
    if (preserve_center && layout_.map.w>0 && layout_.map.h>0) {
        camera_.center_on(center);
        camera_.offset.x+=layout_.map.x;
        camera_.offset.y+=layout_.map.y;
    }
    panel_scroll_=0;
    if (preserve_center) {
        float mouse_x=0,mouse_y=0;
        (void)SDL_GetMouseState(&mouse_x,&mouse_y);
        pointer_=render_point(mouse_x,mouse_y);
    }
    refresh_hover();
}
void SandboxView::place_demo() {
    if (simulation::service_profile(rules_)) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+12<=world_->width();++x) {
            const std::array cells{
                simulation::Cell{x+0,y+2},simulation::Cell{x+1,y+2},
                simulation::Cell{x+2,y+2},simulation::Cell{x+3,y+2},
                simulation::Cell{x+4,y+2},simulation::Cell{x+5,y+2},
                simulation::Cell{x+6,y+2},simulation::Cell{x+7,y+2},
                simulation::Cell{x+8,y+2},simulation::Cell{x+9,y+2},
                simulation::Cell{x+10,y+2},simulation::Cell{x+7,y+1},
                simulation::Cell{x+8,y+1},simulation::Cell{x+9,y+1},
                simulation::Cell{x+6,y+4},simulation::Cell{x+7,y+4},
                simulation::Cell{x+8,y+4},simulation::Cell{x+8,y+3},
                simulation::Cell{x+9,y+3},simulation::Cell{x+10,y+3},
                simulation::Cell{x+9,y+4},simulation::Cell{x+10,y+4},
                simulation::Cell{x+11,y+4}};
            if (!std::all_of(cells.begin(),cells.end(),[&](simulation::Cell cell) {
                    return world_->buildable(cell); })) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("City service demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,2);
            run(simulation::CommandType::PlaceRoad,1,2);
            run(simulation::CommandType::PlaceRoad,2,2);
            run(simulation::CommandType::PlacePottery,3,2);
            run(simulation::CommandType::PlaceRoad,4,2);
            run(simulation::CommandType::PlaceRoad,5,2);
            run(simulation::CommandType::PlaceWarehouse,6,2);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,2);
            run(simulation::CommandType::PlaceHousehold,10,2);
            run(simulation::CommandType::PlaceRoad,7,1);
            run(simulation::CommandType::PlaceRoad,8,1);
            run(simulation::CommandType::PlaceHousehold,9,1);
            run(simulation::CommandType::PlaceFarm,6,4);
            run(simulation::CommandType::PlaceRoad,7,4);
            run(simulation::CommandType::PlaceRoad,8,4);
            run(simulation::CommandType::PlaceRoad,8,3);
            run(simulation::CommandType::PlaceRoad,9,3);
            run(simulation::CommandType::PlaceHousehold,10,3);
            run(simulation::CommandType::PlaceRoad,9,4);
            run(simulation::CommandType::PlaceRoad,10,4);
            run(simulation::CommandType::PlaceServicePost,11,4);
            demo_origin_=simulation::Cell{x,y+2};
            reset_camera();
            last_message_=std::string(profile_title(rules_))+
                " service starter placed: 980 spent, 20 funds remain";
            return;
        }
        throw std::runtime_error("no suitable 12x5 sandbox-buildable City service starter pattern");
    }
    if (simulation::food_profile(rules_)) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int dx=0;dx<11;++dx) valid=valid && world_->buildable({x+dx,y+2});
            for (int dx : {7,8,9}) valid=valid && world_->buildable({x+dx,y+1});
            for (const auto cell:{simulation::Cell{x+6,y+4},simulation::Cell{x+7,y+4},
                                  simulation::Cell{x+8,y+4},simulation::Cell{x+8,y+3}})
                valid=valid && world_->buildable(cell);
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("City v7 demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,2);
            run(simulation::CommandType::PlaceRoad,1,2);
            run(simulation::CommandType::PlaceRoad,2,2);
            run(simulation::CommandType::PlacePottery,3,2);
            run(simulation::CommandType::PlaceRoad,4,2);
            run(simulation::CommandType::PlaceRoad,5,2);
            run(simulation::CommandType::PlaceWarehouse,6,2);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,2);
            run(simulation::CommandType::PlaceHousehold,10,2);
            run(simulation::CommandType::PlaceRoad,7,1);
            run(simulation::CommandType::PlaceRoad,8,1);
            run(simulation::CommandType::PlaceHousehold,9,1);
            run(simulation::CommandType::PlaceFarm,6,4);
            run(simulation::CommandType::PlaceRoad,7,4);
            run(simulation::CommandType::PlaceRoad,8,4);
            run(simulation::CommandType::PlaceRoad,8,3);
            demo_origin_=simulation::Cell{x,y+2};
            reset_camera();
            last_message_="City v7 starter placed through paid commands";
            return;
        }
        throw std::runtime_error("no suitable 11x5 sandbox-buildable City v7 starter pattern");
    }
    if (rules_==simulation::RulesProfile::CityV6) {
        for (int y=0;y+2<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int dx=0;dx<11;++dx) valid=valid && world_->buildable({x+dx,y+1});
            for (int dx : {7,8,9}) valid=valid && world_->buildable({x+dx,y});
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("city demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,1);
            run(simulation::CommandType::PlaceRoad,1,1);
            run(simulation::CommandType::PlaceRoad,2,1);
            run(simulation::CommandType::PlacePottery,3,1);
            run(simulation::CommandType::PlaceRoad,4,1);
            run(simulation::CommandType::PlaceRoad,5,1);
            run(simulation::CommandType::PlaceWarehouse,6,1);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,1);
            run(simulation::CommandType::PlaceHousehold,10,1);
            run(simulation::CommandType::PlaceRoad,7,0);
            run(simulation::CommandType::PlaceRoad,8,0);
            run(simulation::CommandType::PlaceHousehold,9,0);
            demo_origin_=simulation::Cell{x,y+1};
            reset_camera();
            last_message_="City starter placed through paid commands";
            return;
        }
        throw std::runtime_error("no suitable 11x3 sandbox-buildable city starter pattern");
    }
    if (rules_==simulation::RulesProfile::IndustryV5) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int dx=0;dx<11;++dx) valid=valid && world_->buildable({x+dx,y+2});
            for (int dy : {1,3}) for (int dx : {7,8,9})
                valid=valid && world_->buildable({x+dx,y+dy});
            for (int dx=0;dx<=6;++dx) valid=valid && world_->buildable({x+dx,y+4});
            valid=valid && world_->buildable({x+2,y+3}) && world_->buildable({x+6,y+3});
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("industry demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,2);
            run(simulation::CommandType::PlaceRoad,1,2);
            run(simulation::CommandType::PlaceRoad,2,2);
            run(simulation::CommandType::PlacePottery,3,2);
            run(simulation::CommandType::PlaceRoad,4,2);
            run(simulation::CommandType::PlaceRoad,5,2);
            run(simulation::CommandType::PlaceWarehouse,6,2);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,2);
            run(simulation::CommandType::PlaceHousehold,10,2);
            for (int dy : {1,3}) {
                run(simulation::CommandType::PlaceRoad,7,dy);
                run(simulation::CommandType::PlaceRoad,8,dy);
                run(simulation::CommandType::PlaceHousehold,9,dy);
            }
            run(simulation::CommandType::PlaceClaySource,0,4);
            run(simulation::CommandType::PlacePottery,3,4);
            run(simulation::CommandType::PlaceRoad,1,4);
            run(simulation::CommandType::PlaceRoad,2,4);
            run(simulation::CommandType::PlaceRoad,2,3);
            for (int dx=4;dx<=6;++dx) run(simulation::CommandType::PlaceRoad,dx,4);
            run(simulation::CommandType::PlaceRoad,6,3);
            demo_origin_=simulation::Cell{x,y+2};
            reset_camera();
            last_message_="Two-source, two-pottery industry demo placed";
            return;
        }
        throw std::runtime_error("no suitable 11x5 sandbox-buildable industry branch pattern");
    }
    if (rules_==simulation::RulesProfile::SettlementV4) {
        // A fixed three-branch pattern, searched only in the actual buildability mask.
        for (int y=0;y+2<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int i=0;i<11;++i) valid=valid && world_->buildable({x+i,y+1});
            for (int dy : {0,2}) for (int dx : {7,8})
                valid=valid && world_->buildable({x+dx,y+dy});
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("settlement demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,1);
            run(simulation::CommandType::PlaceRoad,1,1);
            run(simulation::CommandType::PlaceRoad,2,1);
            run(simulation::CommandType::PlacePottery,3,1);
            run(simulation::CommandType::PlaceRoad,4,1);
            run(simulation::CommandType::PlaceRoad,5,1);
            run(simulation::CommandType::PlaceWarehouse,6,1);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,1);
            run(simulation::CommandType::PlaceHousehold,10,1);
            for (int dy : {0,2}) {
                run(simulation::CommandType::PlaceRoad,7,dy);
                run(simulation::CommandType::PlaceRoad,8,dy);
                run(simulation::CommandType::PlaceHousehold,9,dy);
            }
            demo_origin_=simulation::Cell{x,y+1};
            reset_camera();
            last_message_="Three-house settlement demo placed through normal commands";
            return;
        }
        throw std::runtime_error("no suitable 11x3 sandbox-buildable branch pattern for settlement demo");
    }
    const int length=rules_==simulation::RulesProfile::HouseholdV3 ? 10 :
        rules_==simulation::RulesProfile::ProductionV2 ? 7 : 6;
    for (int y=0;y<world_->height();++y) for (int x=0;x+length<=world_->width();++x) {
        bool valid=true;
        for (int i=0;i<length;++i) valid=valid && world_->buildable({x+i,y});
        if (!valid) continue;
        demo_origin_=simulation::Cell{x,y};
        if (simulation::production_profile(rules_)) {
            const auto run=[&](simulation::CommandType type,int at) {
                if (!world_->execute({type,{at,y}}).accepted)
                    throw std::logic_error("production demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,x);
            for (int i=1;i<3;++i) run(simulation::CommandType::PlaceRoad,x+i);
            run(simulation::CommandType::PlacePottery,x+3);
            for (int i=4;i<6;++i) run(simulation::CommandType::PlaceRoad,x+i);
            run(simulation::CommandType::PlaceWarehouse,x+6);
            if (rules_==simulation::RulesProfile::HouseholdV3) {
                run(simulation::CommandType::PlaceRoad,x+7);
                run(simulation::CommandType::PlaceRoad,x+8);
                run(simulation::CommandType::PlaceHousehold,x+9);
            }
            reset_camera();
            last_message_="Production demo placed through normal commands";
            return;
        }
        if (!world_->execute({simulation::CommandType::PlaceWorkshop,{x,y}}).accepted)
            throw std::logic_error("demo workshop placement failed");
        for (int i=1;i<length-1;++i)
            if (!world_->execute({simulation::CommandType::PlaceRoad,{x+i,y}}).accepted)
                throw std::logic_error("demo road placement failed");
        if (!world_->execute({simulation::CommandType::PlaceWarehouse,{x+length-1,y}}).accepted)
            throw std::logic_error("demo warehouse placement failed");
        reset_camera();
        last_message_="Demo placed through normal commands";
        return;
    }
    throw std::runtime_error("no suitable straight sandbox-buildable row for demo");
}
std::optional<simulation::Cell> SandboxView::pick(scene::Point screen) const {
    if (!layout_.map.contains(screen.x,screen.y)) return std::nullopt;
    const auto cell=maps::pick_terrain_cell(camera_.screen_to_world(screen),geometry_);
    if (!cell) return std::nullopt;
    return simulation::Cell{static_cast<int>(cell->x),static_cast<int>(cell->y)};
}
simulation::CommandResult SandboxView::preview(simulation::Cell cell) const {
    if (tool_==(simulation::production_profile(rules_) ? 5 : 4))
        return {false,false,"Select tool",world_->command_sequence()+1,world_->ticks()};
    if (tool_==1 && world_->object_at(cell)==simulation::Object::Road)
        return {true,false,"Existing road",world_->command_sequence()+1,world_->ticks()};
    return world_->validate({command_type(rules_,tool_),cell});
}
void SandboxView::set_tool(int tool) {
    cancel_gesture();
    sandbox_ui::Action action;
    switch (tool) {
    case 1: action=sandbox_ui::Action::Road; break;
    case 2: action=sandbox_ui::Action::Clay; break;
    case 3: action=simulation::production_profile(rules_) ? sandbox_ui::Action::Pottery :
        sandbox_ui::Action::Warehouse; break;
    case 4: action=simulation::production_profile(rules_) ? sandbox_ui::Action::Warehouse :
        sandbox_ui::Action::Select; break;
    case 5: action=sandbox_ui::Action::Select; break;
    case 6: action=sandbox_ui::Action::RemoveRoad; break;
    case 7: action=sandbox_ui::Action::Household; break;
    case 8: action=sandbox_ui::Action::Farm; break;
    case 9: action=sandbox_ui::Action::ServicePost; break;
    default: return;
    }
    if (action_enabled(action)) perform_action(action);
}
bool SandboxView::action_enabled(sandbox_ui::Action action) const {
    const auto count_kind=[&](simulation::Object kind) {
        return std::count_if(world_->buildings().begin(),world_->buildings().end(),
            [&](const simulation::BuildingState& b) { return b.placed && b.kind==kind; });
    };
    if (action==sandbox_ui::Action::Save || action==sandbox_ui::Action::Load)
        return !save_path_.empty();
    if (simulation::city_profile(rules_)) {
        std::optional<simulation::CommandType> command;
        if (action==sandbox_ui::Action::Road) command=simulation::CommandType::PlaceRoad;
        else if (action==sandbox_ui::Action::Clay) command=simulation::CommandType::PlaceClaySource;
        else if (action==sandbox_ui::Action::Pottery) command=simulation::CommandType::PlacePottery;
        else if (action==sandbox_ui::Action::Warehouse) command=simulation::CommandType::PlaceWarehouse;
        else if (action==sandbox_ui::Action::Household) command=simulation::CommandType::PlaceHousehold;
        else if (action==sandbox_ui::Action::Farm) command=simulation::CommandType::PlaceFarm;
        else if (action==sandbox_ui::Action::ServicePost)
            command=simulation::CommandType::PlaceServicePost;
        if (command && world_->treasury()<world_->construction_cost(*command)) return false;
    }
    if (action==sandbox_ui::Action::RemoveRoad)
        return simulation::production_profile(rules_);
    if (action==sandbox_ui::Action::Household)
        return simulation::household_profile(rules_) && count_kind(simulation::Object::Household)<
            (rules_==simulation::RulesProfile::CityV10 ? 20:4);
    if (action==sandbox_ui::Action::Farm)
        return simulation::food_profile(rules_) &&
            count_kind(simulation::Object::Farm)<
                (rules_==simulation::RulesProfile::CityV10 ? 2:1);
    if (action==sandbox_ui::Action::ServicePost)
        return simulation::service_profile(rules_) &&
            count_kind(simulation::Object::ServicePost)<
                (rules_==simulation::RulesProfile::CityV10 ? 2:1);
    if (action==sandbox_ui::Action::Clay || action==sandbox_ui::Action::Pottery ||
        action==sandbox_ui::Action::Warehouse) {
        const auto wanted=action==sandbox_ui::Action::Clay ?
            (simulation::production_profile(rules_) ? simulation::Object::ClaySource :
                simulation::Object::Workshop) :
            action==sandbox_ui::Action::Pottery ? simulation::Object::Pottery :
                simulation::Object::Warehouse;
        if (action==sandbox_ui::Action::Pottery && !simulation::production_profile(rules_)) return false;
        int count=0;
        count=static_cast<int>(count_kind(wanted));
        const int limit=rules_==simulation::RulesProfile::CityV10 ?
            (wanted==simulation::Object::ClaySource || wanted==simulation::Object::Pottery ? 4:
             wanted==simulation::Object::Warehouse ? 2:1):
            (simulation::industry_profile(rules_) &&
            (wanted==simulation::Object::ClaySource || wanted==simulation::Object::Pottery)) ? 2:1;
        return count<limit;
    }
    return true;
}
void SandboxView::cancel_gesture() {
    pressed_button_.reset();
    pressed_building_.reset();
    ui_pressed_=false;
    map_pressed_=false;
    road_start_.reset();
    road_preview_={};
    menu_pressed_=false;
}
void SandboxView::perform_action(sandbox_ui::Action action) {
    using A=sandbox_ui::Action;
    if (action==A::Save || action==A::Load) cancel_gesture();
    if (!action_enabled(action)) {
        std::optional<simulation::CommandType> command;
        if (action==A::Road) command=simulation::CommandType::PlaceRoad;
        else if (action==A::Clay) command=simulation::CommandType::PlaceClaySource;
        else if (action==A::Pottery) command=simulation::CommandType::PlacePottery;
        else if (action==A::Warehouse) command=simulation::CommandType::PlaceWarehouse;
        else if (action==A::Household) command=simulation::CommandType::PlaceHousehold;
        else if (action==A::Farm) command=simulation::CommandType::PlaceFarm;
        else if (action==A::ServicePost) command=simulation::CommandType::PlaceServicePost;
        if (simulation::city_profile(rules_) && command &&
            world_->treasury()<world_->construction_cost(*command))
            last_message_="Need "+std::to_string(world_->construction_cost(*command))+
                " funds; treasury "+std::to_string(world_->treasury());
        else last_message_=save_path_.empty() && (action==A::Save || action==A::Load) ?
            "No sandbox save path configured" : "Tool unavailable or building limit reached";
        return;
    }
    if (action==A::Select || action==A::Road || action==A::Clay || action==A::Pottery ||
        action==A::Warehouse || action==A::RemoveRoad || action==A::Household ||
        action==A::Farm || action==A::ServicePost) {
        cancel_gesture();
        tool_=action==A::Select ? (simulation::production_profile(rules_) ? 5:4) :
            action==A::Road ? 1 : action==A::Clay ? 2 : action==A::Pottery ? 3 :
            action==A::Warehouse ? (simulation::production_profile(rules_) ? 4:3) :
            action==A::RemoveRoad ? 6:action==A::Household ? 7:action==A::Farm ? 8:9;
        last_message_=tool_name(rules_,tool_);
    } else if (action==A::Pause) clock_.toggle_pause();
    else if (action==A::Step) clock_.step_once(*world_);
    else if (action==A::Speed1) clock_.set_speed(1);
    else if (action==A::Speed2) clock_.set_speed(2);
    else if (action==A::Speed4) clock_.set_speed(4);
    else if (action==A::Reset) { cancel_gesture(); reset_camera(); }
    else if (action==A::Save || action==A::Load) {
        cancel_gesture();
        ++io_generation_;
        try { if (action==A::Save) save_now(); else load_now(); }
        catch (const std::exception& error) {
            last_message_=(action==A::Save ? "Save failed: ":"Load failed: ")+
                std::string(error.what());
        }
    } else if (action==A::TogglePanel) {
        cancel_gesture();
        panel_open_=!panel_open_;
        update_layout(true);
    } else if (action==A::ToggleHelp) {
        cancel_gesture(); help_open_=!help_open_;
        last_message_=help_open_ ? "Help opened; press H or ? to close":"Help closed";
    }
    refresh_hover();
}
std::optional<scene::Point> SandboxView::render_point(float x,float y) const {
    int width=0,height=0;
    if (!SDL_GetWindowSize(window_,&width,&height) || x<0 || y<0 ||
        x>=static_cast<float>(width) || y>=static_cast<float>(height))
        return std::nullopt;
    float rx=0,ry=0;
    if (!SDL_RenderCoordinatesFromWindow(renderer_,x,y,&rx,&ry)) return std::nullopt;
    return scene::Point{rx,ry};
}
void SandboxView::refresh_hover() {
    hovered_=pointer_ ? pick(*pointer_) : std::nullopt;
    if (road_start_ && hovered_) road_preview_=sandbox_ui::plan_road(*world_,*road_start_,*hovered_);
}
void SandboxView::handle_event(const SDL_Event& event,bool& running) {
    if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        cancel_gesture(); if (managed_) menu_requested_=true; else running=false; return;
    }
    if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST) { cancel_gesture(); pointer_.reset(); refresh_hover(); return; }
    if (event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type==SDL_EVENT_WINDOW_RESIZED) {
        cancel_gesture(); resize_camera(); return;
    }
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key==SDLK_H || event.key.key==SDLK_QUESTION) {
            cancel_gesture();
            help_open_=!help_open_;
            last_message_=help_open_ ? "Help opened; press H or ? to close":"Help closed";
            return;
        }
        if (event.key.key==SDLK_ESCAPE) {
            if (help_open_) { help_open_=false; last_message_="Help closed"; return; }
            if (map_pressed_ || road_start_ || pressed_button_ || ui_pressed_ || menu_pressed_)
                cancel_gesture();
            else if (managed_) menu_requested_=true;
            else running=false;
            return;
        }
        if (help_open_) return;
        if (event.key.key>=SDLK_1 && event.key.key<=
            (simulation::service_profile(rules_) ? SDLK_9 :
             rules_==simulation::RulesProfile::CityV7 ? SDLK_8 :
             simulation::household_profile(rules_) ? SDLK_7 :
             rules_==simulation::RulesProfile::ProductionV2 ? SDLK_6 : SDLK_4))
            set_tool(static_cast<int>(event.key.key-SDLK_1)+1);
        else if (event.key.key==SDLK_SPACE) perform_action(sandbox_ui::Action::Pause);
        else if (event.key.key==SDLK_PERIOD) perform_action(sandbox_ui::Action::Step);
        else if (event.key.key==SDLK_PLUS || event.key.key==SDLK_EQUALS || event.key.key==SDLK_KP_PLUS)
            perform_action(clock_.speed()==1 ? sandbox_ui::Action::Speed2 : sandbox_ui::Action::Speed4);
        else if (event.key.key==SDLK_MINUS || event.key.key==SDLK_KP_MINUS)
            perform_action(clock_.speed()==4 ? sandbox_ui::Action::Speed2 : sandbox_ui::Action::Speed1);
        else if (event.key.key==SDLK_R) perform_action(sandbox_ui::Action::Reset);
        else if (event.key.key==SDLK_F5) perform_action(sandbox_ui::Action::Save);
        else if (event.key.key==SDLK_F9) perform_action(sandbox_ui::Action::Load);
        else if (event.key.key==SDLK_Z) {
            const double target=camera_.zoom<1.5 ? 2.0:camera_.zoom<3.0 ? 4.0:1.0;
            camera_.zoom_at({static_cast<double>(layout_.map.x+layout_.map.w/2),
                             static_cast<double>(layout_.map.y+layout_.map.h/2)},
                            target/camera_.zoom);
            refresh_hover();
        }
        else if (event.key.key==SDLK_TAB) perform_action(sandbox_ui::Action::TogglePanel);
        else if (event.key.key==SDLK_F1) {
            debug_open_=!debug_open_;
            background_.set_debug_diagnostics(debug_open_);
            last_message_=debug_open_ ? "Debug diagnostics ON":"Debug diagnostics OFF";
        }
        else if (event.key.key==SDLK_F2 && walker_profile_) {
            walker_visuals_enabled_=!walker_visuals_enabled_;
            last_message_=walker_visuals_enabled_ ? "Walker visuals ON" : "Walker visuals OFF";
        }
        else if (event.key.key==SDLK_F3 && walker_profile_) {
            walker_diagnostic_open_=!walker_diagnostic_open_;
            last_message_=walker_diagnostic_open_ ? "Walker clip inspection ON":"Walker clip inspection OFF";
        }
        else if (event.key.key==SDLK_F4) {
            if (building_profile_) {
                building_enabled_=!building_enabled_;
                last_message_=building_enabled_ ? "Building visuals ON":"Building visuals OFF";
            } else last_message_="No building visuals loaded";
        }
        else if (event.key.key==SDLK_F6) {
            if (road_profile_) {
                road_enabled_=!road_enabled_;
                last_message_=road_enabled_ ? "Road visuals ON":"Road visuals OFF";
            } else last_message_="No road visuals loaded";
        }
        else if (event.key.key==SDLK_F7) {
            unified_depth_=!unified_depth_;
            last_message_=unified_depth_ ? "Depth painter: unified":"Depth painter: legacy";
        }
        else if (walker_diagnostic_open_ && walker_profile_) {
            if (event.key.key==SDLK_V) {
                walker_diagnostic_role_=(walker_diagnostic_role_+1)%3;
                walker_diagnostic_step_=0;
            } else if (event.key.key==SDLK_Q || event.key.key==SDLK_BACKSLASH) {
                walker_diagnostic_direction_=(walker_diagnostic_direction_+1)%4;
                walker_diagnostic_step_=0;
            } else if (event.key.key==SDLK_E || event.key.key==SDLK_RIGHTBRACKET)
                ++walker_diagnostic_step_;
            else if (event.key.key==SDLK_C || event.key.key==SDLK_LEFTBRACKET) {
                const auto& role=walker_profile_->roles[walker_diagnostic_role_];
                if (role) {
                    const auto& clip=role->clips[walker_diagnostic_direction_];
                    if (!clip.empty())
                        walker_diagnostic_step_=(walker_diagnostic_step_+clip.size()-1)%clip.size();
                }
            } else if (event.key.key==SDLK_X) walker_diagnostic_zoom4_=!walker_diagnostic_zoom4_;
            else if (event.key.key==SDLK_B) walker_diagnostic_light_=!walker_diagnostic_light_;
        }
    }
    if (help_open_) {
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
            const auto point=render_point(event.button.x,event.button.y);
            if (point && layout_.button_at(point->x,point->y)==sandbox_ui::Action::ToggleHelp)
                perform_action(sandbox_ui::Action::ToggleHelp);
        }
        return;
    }
    if (event.type==SDL_EVENT_MOUSE_WHEEL) {
        const auto point=render_point(event.wheel.mouse_x,event.wheel.mouse_y);
        if (point) {
            pointer_=point;
            if (layout_.panel.contains(point->x,point->y)) {
                const auto rows=placed_buildings().size()+inspection_lines().size();
                const int total=56*layout_.scale+static_cast<int>(rows)*18*layout_.scale;
                const int limit=std::max(0,total-layout_.panel.h);
                panel_scroll_=std::clamp(panel_scroll_-static_cast<int>(event.wheel.y)*24*layout_.scale,
                                         0,limit);
            }
            else if (layout_.map.contains(point->x,point->y) && !road_start_)
                camera_.zoom_at(*point,std::pow(1.15,event.wheel.y));
            refresh_hover();
        }
    }
    if (event.type==SDL_EVENT_MOUSE_MOTION) {
        pointer_=render_point(event.motion.x,event.motion.y);
        refresh_hover();
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button==SDL_BUTTON_RIGHT) {
            cancel_gesture(); return;
        }
        if (event.button.button!=SDL_BUTTON_LEFT) return;
        pointer_=render_point(event.button.x,event.button.y);
        if (managed_ && pointer_ && layout_.top.contains(pointer_->x,pointer_->y) &&
            pointer_->x>=layout_.top.w-88*layout_.scale) { menu_pressed_=true; return; }
        refresh_hover();
        if (!pointer_) return;
        if (const auto button=layout_.button_at(pointer_->x,pointer_->y)) {
            pressed_button_=button; return;
        }
        if (layout_.ui_at(pointer_->x,pointer_->y)) {
            ui_pressed_=true;
            if (layout_.panel.contains(pointer_->x,pointer_->y)) {
                const int relative=static_cast<int>(pointer_->y)-layout_.panel.y-
                    46*layout_.scale+panel_scroll_;
                if (relative>=0) {
                    const auto entries=placed_buildings();
                    const auto index=static_cast<std::size_t>(relative/(18*layout_.scale));
                    if (index<entries.size()) pressed_building_=entries[index];
                }
            }
            return;
        }
        const auto cell=pick(*pointer_);
        if (!cell) return;
        map_pressed_=true;
        if (tool_==1) {
            road_start_=cell;
            road_preview_=sandbox_ui::plan_road(*world_,*cell,*cell);
        }
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
        pointer_=render_point(event.button.x,event.button.y);
        if (menu_pressed_) {
            const bool hit=pointer_ && layout_.top.contains(pointer_->x,pointer_->y) &&
                pointer_->x>=layout_.top.w-88*layout_.scale;
            cancel_gesture(); if (hit) menu_requested_=true; return;
        }
        refresh_hover();
        if (pressed_button_) {
            const auto action=*pressed_button_;
            const bool matching=pointer_ && layout_.button_at(pointer_->x,pointer_->y)==action;
            cancel_gesture();
            if (matching) perform_action(action);
            return;
        }
        if (ui_pressed_) {
            if (pressed_building_ && pointer_ && layout_.panel.contains(pointer_->x,pointer_->y)) {
                const int relative=static_cast<int>(pointer_->y)-layout_.panel.y-
                    46*layout_.scale+panel_scroll_;
                const auto entries=placed_buildings();
                if (relative>=0) {
                    const auto index=static_cast<std::size_t>(relative/(18*layout_.scale));
                    if (index<entries.size() && entries[index]==*pressed_building_)
                        selected_=world_->building(*pressed_building_).cell;
                }
            }
            cancel_gesture(); return;
        }
        if (!map_pressed_) return;
        const auto cell=pointer_ ? pick(*pointer_) : std::nullopt;
        if (cell && road_start_ && tool_==1) {
            const auto plan=sandbox_ui::plan_road(*world_,*road_start_,*cell);
            std::string reason;
            if (!sandbox_ui::commit_road(*world_,plan,reason)) last_message_=reason;
            else last_message_=reason;
            selected_=cell;
        } else if (cell) {
            if (tool_==(simulation::production_profile(rules_) ? 5:4)) selected_=cell;
            else (void)execute({command_type(rules_,tool_),*cell});
        }
        cancel_gesture();
        refresh_hover();
    }
}
void SandboxView::update(double seconds) {
    const bool* keys=SDL_GetKeyboardState(nullptr);
    const double movement=400.0*std::clamp(seconds,0.0,0.05);
    if (!road_start_) {
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) camera_.offset.x+=movement;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) camera_.offset.x-=movement;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) camera_.offset.y+=movement;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) camera_.offset.y-=movement;
        refresh_hover();
    }
    clock_.update(seconds,*world_);
}
void SandboxView::tick_once() { world_->tick(); }
simulation::CommandResult SandboxView::execute(simulation::Command command) {
    const auto result=world_->execute(command);
    last_message_=result.reason;
    selected_=command.cell;
    return result;
}
scene::Point SandboxView::world_for(simulation::Position cell) const {
    const double u=cell.x-static_cast<double>(geometry_.border);
    const double v=cell.y-static_cast<double>(geometry_.border);
    return {(u-v)*40.0,(u+v)*20.0+20.0};
}
bool SandboxView::draw_diamond(scene::Point world,std::uint8_t r,std::uint8_t g,std::uint8_t b,bool fill) {
    const auto p=camera_.world_to_screen(world);
    const float x=static_cast<float>(p.x),y=static_cast<float>(p.y);
    const float w=static_cast<float>(40*camera_.zoom),h=static_cast<float>(20*camera_.zoom);
    const SDL_FPoint points[]={{x,y},{x+w,y+h},{x,y+2*h},{x-w,y+h}};
    if (fill) {
        const SDL_FColor color{r/255.0F,g/255.0F,b/255.0F,0.65F};
        const SDL_Vertex vertices[]={{points[0],color,{}},{points[1],color,{}},
                                     {points[2],color,{}},{points[3],color,{}}};
        const int indices[]={0,1,2,0,2,3};
        if (!SDL_RenderGeometry(renderer_,nullptr,vertices,4,indices,6)) return false;
    }
    if (!SDL_SetRenderDrawColor(renderer_,r,g,b,255)) return false;
    for (int i=0;i<4;++i)
        if (!SDL_RenderLine(renderer_,points[i].x,points[i].y,
                            points[(i+1)%4].x,points[(i+1)%4].y)) return false;
    return true;
}
bool SandboxView::draw_world(const scene::Camera2D& render_camera) {
    last_courier_draws_=0;
    road_fallbacks_current_=0;
    auto& instances=draw_instances_;
    instances.clear();
    const auto maximum=static_cast<std::size_t>(world_->width())*
        static_cast<std::size_t>(world_->height())+road_preview_.cells.size()+6U;
    if (instances.capacity()<maximum) instances.reserve(maximum);
    for (int y=0;y<world_->height();++y) for (int x=0;x<world_->width();++x) {
        const simulation::Cell cell{x,y};
        const auto object=world_->object_at(cell);
        if (object==simulation::Object::Empty) continue;
        const auto ground=world_for({static_cast<double>(x),static_cast<double>(y)});
        const auto owner=world_->building_owner_at(cell);
        const auto id=owner ? static_cast<unsigned>(*owner):
            static_cast<unsigned>(y*world_->width()+x);
        instances.push_back({{ground.y,ground.x,object==simulation::Object::Road ?
            scene::WorldVisualLayer::SandboxRoad:scene::WorldVisualLayer::SandboxBuilding,id},
            cell,object});
    }
    if (road_start_ && road_preview_.valid) {
        for (const auto cell:road_preview_.cells) {
            if (world_->object_at(cell)==simulation::Object::Road) continue;
            const auto ground=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
            const auto id=static_cast<unsigned>(cell.y*world_->width()+cell.x);
            instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxRoad,id},cell,
                                 simulation::Object::Road});
        }
    }
    const auto draw_object=[&](simulation::Cell cell,simulation::Object object,
                               bool placement_preview)->bool {
        const int x=cell.x,y=cell.y;
        const auto top=world_for({static_cast<double>(x),static_cast<double>(y)});
        const auto center=camera_.world_to_screen(top);
        if (object==simulation::Object::Road) {
            const bool new_preview=road_start_ && road_preview_.valid &&
                world_->object_at(cell)!=simulation::Object::Road;
            const auto mask=road_start_ && road_preview_.valid ?
                sandbox_ui::road_neighbor_mask_for_preview(*world_,road_preview_.cells,cell):
                sandbox_ui::road_neighbor_mask(*world_,cell);
            road_masks_seen_[mask]=true;
            const auto* entry=road_profile_ ? road_profile_->find(mask):nullptr;
            if (entry && road_visuals_active() && road_sprites_) {
                if (!road_sprites_->draw(center,camera_.zoom,*road_profile_,*entry,new_preview))
                    return false;
                if (!new_preview) ++road_draws_;
                return true;
            }
            if (!new_preview) ++road_fallbacks_current_;
            if (new_preview) return draw_diamond({top.x,top.y-20},45,180,100,true);
            const SDL_Color road_color=debug_open_ ? SDL_Color{225,174,65,255}:
                SDL_Color{137,109,72,255};
            if (!draw_diamond({top.x,top.y-20},road_color.r,road_color.g,road_color.b,true)) return false;
            for (const simulation::Cell next : {simulation::Cell{x+1,y},simulation::Cell{x,y+1},
                                                simulation::Cell{x-1,y},simulation::Cell{x,y-1}}) {
                const auto other=world_->object_at(next);
                if (other==simulation::Object::Empty ||
                    (other==simulation::Object::Road && (next.x<x || next.y<y))) continue;
                const auto screen=camera_.world_to_screen(world_for({static_cast<double>(next.x),
                    static_cast<double>(next.y)}));
                const SDL_Color line=debug_open_ ? SDL_Color{255,215,94,255}:
                    SDL_Color{161,132,88,255};
                if (!SDL_SetRenderDrawColor(renderer_,line.r,line.g,line.b,255) ||
                    !SDL_RenderLine(renderer_,static_cast<float>(center.x),static_cast<float>(center.y),
                        static_cast<float>(screen.x),static_cast<float>(screen.y))) return false;
            }
        } else {
            const auto role=building_visual_role(object);
            if (role && building_profile_) {
                const auto* entry=building_profile_->find(*role);
                if (entry && building_visuals_active() && building_sprite_) {
                    if (!building_sprite_->draw(center,camera_.zoom,*building_profile_,*entry,
                                                placement_preview))
                        return false;
                    if (!placement_preview) ++building_drawn_instances_[assets::role_index(*role)];
                    return true;
                }
                if (placement_preview) return true;
                ++building_placeholder_fallbacks_[assets::role_index(*role)];
            }
            SDL_Color color=debug_open_ ? SDL_Color{255,105,100,255}:SDL_Color{137,101,85,255};
            if (object==simulation::Object::Workshop || object==simulation::Object::ClaySource)
                color=debug_open_ ? SDL_Color{50,210,245,255}:SDL_Color{104,124,120,255};
            if (object==simulation::Object::Pottery)
                color=debug_open_ ? SDL_Color{220,95,245,255}:SDL_Color{135,101,125,255};
            if (object==simulation::Object::Household)
                color=debug_open_ ? SDL_Color{90,245,125,255}:SDL_Color{101,128,100,255};
            if (object==simulation::Object::Farm)
                color=debug_open_ ? SDL_Color{220,190,55,255}:SDL_Color{126,119,70,255};
            if (object==simulation::Object::ServicePost)
                color=debug_open_ ? SDL_Color{80,205,255,255}:SDL_Color{91,119,132,255};
            if (!draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true)) return false;
            const float size=static_cast<float>(std::max(5.0,11.0*camera_.zoom));
            const SDL_FRect rect{static_cast<float>(center.x)-size/2,
                static_cast<float>(center.y)-size*1.5F,size,size};
            if (!SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) ||
                !SDL_RenderFillRect(renderer_,&rect)) return false;
            if (debug_open_ && (object==simulation::Object::Household || object==simulation::Object::Farm ||
                object==simulation::Object::ServicePost ||
                (simulation::industry_profile(rules_) &&
                 (object==simulation::Object::ClaySource || object==simulation::Object::Pottery)))) {
                const auto id=world_->building_owner_at({x,y});
                const char prefix=object==simulation::Object::Household ? 'H':
                    object==simulation::Object::Farm ? 'F':
                    object==simulation::Object::ServicePost ? 'S':
                    object==simulation::Object::ClaySource ? 'C':'P';
                const std::string label=std::string(1,prefix)+
                    std::to_string(static_cast<unsigned>(*id));
                if (!SDL_SetRenderDrawColor(renderer_,255,255,255,255) ||
                    !SDL_RenderDebugText(renderer_,static_cast<float>(center.x)+7,
                        static_cast<float>(center.y)-16,label.c_str())) return false;
            }
        }
        return true;
    };
    const auto draw_agent=[&](simulation::Position position,SDL_Color body,SDL_Color cargo_color,
                              int cargo,int shift)->bool {
        const auto screen=camera_.world_to_screen(world_for(position));
        const double base=debug_open_ ? 10.0:7.0;
        const float size=static_cast<float>(std::max(debug_open_ ? 5.0:4.0,base*camera_.zoom));
        const SDL_FRect marker{static_cast<float>(screen.x)-size/2+shift,
            static_cast<float>(screen.y)-size/2,size,size};
        if (!SDL_SetRenderDrawColor(renderer_,body.r,body.g,body.b,255) ||
            !SDL_RenderFillRect(renderer_,&marker)) return false;
        if (cargo>0) {
            const SDL_FRect cargo_rect{marker.x+size*.5F,marker.y-size*.5F,size*.5F,size*.5F};
            if (!SDL_SetRenderDrawColor(renderer_,cargo_color.r,cargo_color.g,cargo_color.b,255) ||
                !SDL_RenderFillRect(renderer_,&cargo_rect)) return false;
        }
        ++last_courier_draws_;
        return true;
    };
    const auto draw_courier=[&](simulation::CourierId id,simulation::Position position,
                               SDL_Color marker_color,SDL_Color cargo_color,
                               int marker_shift)->bool {
        const auto& courier=world_->courier(id);
        const auto visual_role=walker_visual_role(courier.role);
        const auto* visual=visual_role && walker_profile_ ?
            walker_profile_->find(*visual_role):nullptr;
        if (walker_visuals_active() && walker_sprites_ && visual) {
            const auto role_index=assets::walker_role_index(*visual_role);
            const auto pose=walker_pose(courier,world_->ticks(),*visual);
            const auto ground=camera_.world_to_screen(world_for(position));
            if (pose.frame) {
                if (!walker_sprites_->draw(*pose.frame,ground,camera_.zoom,*visual,*walker_profile_,
                    {static_cast<double>(layout_.map.x),static_cast<double>(layout_.map.y)},
                    {static_cast<double>(layout_.map.x+layout_.map.w),
                     static_cast<double>(layout_.map.y+layout_.map.h)})) return false;
                const auto& frame=visual->frames[*pose.frame];
                const auto& image=walker_profile_->unique_images[frame.image_index];
                const double left=ground.x-frame.foot_x*camera_.zoom;
                const double top=ground.y-frame.foot_y*camera_.zoom;
                const bool visible=left+image.width*camera_.zoom>layout_.map.x &&
                    top+image.height*camera_.zoom>layout_.map.y &&
                    left<layout_.map.x+layout_.map.w &&
                    top<layout_.map.y+layout_.map.h;
                if (visible) {
                    ++walker_role_stats_[role_index].draws;
                    if (pose.moving && pose.direction)
                        walker_role_stats_[role_index].directions_drawn[
                            assets::direction_index(*pose.direction)]=true;
                }
                if (pose.loaded) {
                    const float size=static_cast<float>(std::max(5.0,8.0*camera_.zoom));
                    const SDL_FRect cargo{static_cast<float>(ground.x)+size*.3F,
                        static_cast<float>(ground.y)-size*1.5F,size*.6F,size*.6F};
                    if (!SDL_SetRenderDrawColor(renderer_,cargo_color.r,cargo_color.g,
                                                cargo_color.b,255) ||
                        !SDL_RenderFillRect(renderer_,&cargo)) return false;
                }
                ++last_courier_draws_;
                return true;
            }
            if (pose.fallback==WalkerFallback::UnmappedDirection) {
                ++walker_unmapped_fallbacks_;
                ++walker_role_stats_[role_index].fallback_unmapped;
            } else if (pose.fallback==WalkerFallback::InvalidEdge) {
                ++walker_invalid_edge_fallbacks_;
                ++walker_role_stats_[role_index].fallback_invalid_edge;
            }
            const SDL_Color missing=debug_open_ ? SDL_Color{255,90,60,255}:
                SDL_Color{126,126,116,255};
            if (!draw_agent(position,missing,cargo_color,courier.cargo,
                            marker_shift)) return false;
            return !debug_open_ || SDL_RenderDebugText(renderer_,static_cast<float>(ground.x)+5,
                static_cast<float>(ground.y)-20,"W?");
        }
        if (!draw_agent(position,marker_color,cargo_color,courier.cargo,marker_shift)) return false;
        if (debug_open_ && (courier.role==simulation::CourierRole::Food ||
                            courier.role==simulation::CourierRole::Service)) {
            const auto ground=camera_.world_to_screen(world_for(position));
            return SDL_RenderDebugText(renderer_,static_cast<float>(ground.x)+5,
                                       static_cast<float>(ground.y)-20,
                courier.role==simulation::CourierRole::Food ? "F":"S");
        }
        return true;
    };
    if (!simulation::production_profile(rules_)) {
        const auto position=world_->courier_position();
        if (position) {
            const auto ground=world_for(*position);
            instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxWalker,1},{},
                                 simulation::Object::Empty,simulation::CourierId::Clay,*position});
        }
    } else for (const auto& state:world_->couriers()) {
        const auto position=world_->courier_position(state.id);
        if (!position) continue;
        const auto ground=world_for(*position);
        instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxWalker,
                              static_cast<std::uint32_t>(state.id)},{},
                             simulation::Object::Empty,state.id,*position});
    }
    if (!road_start_ && hovered_ && tool_!=(simulation::production_profile(rules_) ? 5:4)) {
        const auto result=preview(*hovered_);
        const auto object=tool_==2 ? simulation::Object::ClaySource:
            tool_==3 ? simulation::Object::Pottery:
            tool_==4 ? simulation::Object::Warehouse:
            tool_==7 ? simulation::Object::Household:
            tool_==8 ? simulation::Object::Farm:
            tool_==9 ? simulation::Object::ServicePost:simulation::Object::Empty;
        const auto role=simulation::production_profile(rules_) ?
            building_visual_role(object):std::nullopt;
        if (result.accepted && role && building_visuals_active() && building_sprite_ &&
            building_profile_ && building_profile_->find(*role)) {
            const auto ground=world_for({static_cast<double>(hovered_->x),
                                         static_cast<double>(hovered_->y)});
            instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxBuilding,
                                  std::numeric_limits<unsigned>::max()},*hovered_,object,
                                 simulation::CourierId::Clay,{},true});
        }
    }
    std::sort(instances.begin(),instances.end(),[](const DrawInstance& a,const DrawInstance& b) {
        return a.key<b.key;
    });
    const auto draw_instance=[&](std::size_t index)->bool {
        const auto& instance=instances[index];
        if (instance.key.layer!=scene::WorldVisualLayer::SandboxWalker)
            return draw_object(instance.cell,instance.object,instance.placement_preview);
        const auto id=static_cast<unsigned>(instance.courier);
        const auto position=instance.position;
        if (!simulation::production_profile(rules_)) {
            return draw_agent(position,{255,255,255,255},{255,215,20,255},
                              world_->courier_cargo(),0);
        }
        const auto& courier=world_->courier(instance.courier);
        const auto pending=courier.route_pending;
        const SDL_Color color=debug_open_ ? (pending ? SDL_Color{255,120,40,255}:
            courier.role==simulation::CourierRole::Clay ?
                (id==1 ? SDL_Color{100,240,255,255}:SDL_Color{50,175,255,255}):
            id==2 ? SDL_Color{255,225,130,255}:
            id==3 ? SDL_Color{110,255,130,255}:
            courier.role==simulation::CourierRole::Food ? SDL_Color{235,200,70,255}:
            courier.role==simulation::CourierRole::Service ? SDL_Color{120,205,220,255}:
            SDL_Color{225,145,255,255}) :
            (pending ? SDL_Color{151,104,74,255}:
             courier.role==simulation::CourierRole::Clay ? SDL_Color{104,126,132,255}:
             courier.role==simulation::CourierRole::Pottery ? SDL_Color{143,121,95,255}:
             courier.role==simulation::CourierRole::Food ? SDL_Color{137,125,77,255}:
             courier.role==simulation::CourierRole::Service ? SDL_Color{105,120,126,255}:
             SDL_Color{112,132,108,255});
        const SDL_Color cargo=debug_open_ ? (courier.role==simulation::CourierRole::Clay ?
            SDL_Color{25,95,255,255}:courier.role==simulation::CourierRole::Food ?
            SDL_Color{240,190,30,255}:courier.role==simulation::CourierRole::Household ?
            SDL_Color{255,90,150,255}:SDL_Color{240,45,190,255}) :
            (courier.role==simulation::CourierRole::Clay ? SDL_Color{72,94,130,255}:
             courier.role==simulation::CourierRole::Food ? SDL_Color{153,132,68,255}:
             courier.role==simulation::CourierRole::Household ? SDL_Color{148,92,104,255}:
             SDL_Color{133,86,119,255});
        const int shift=id==1 ? -5:id==2 ? 5:id==3 ? 0:id==4 ? -10:id==5 ? 10:14;
        return draw_courier(instance.courier,position,color,cargo,shift);
    };
    painter_stats_={};
    painter_stats_.stored_order_builds=background_.stored_order_builds();
    if (unified_depth_) {
        background_.begin_frame();
        scene::WorldMergeStats stats;
        if (!scene::merge_world_draw_streams(background_.draw_items(),instances,
            [&](std::size_t i) { return background_.draw_item(i,render_camera); },
            draw_instance,stats)) return false;
        static_cast<scene::WorldMergeStats&>(painter_stats_)=stats;
    } else {
        painter_stats_.sandbox_items=instances.size();
        for (std::size_t i=0;i<instances.size();++i)
            if (!draw_instance(i)) return false;
    }
    if (selected_) {
        const auto top=world_for({static_cast<double>(selected_->x),static_cast<double>(selected_->y)});
        if (!draw_diamond({top.x,top.y-20},255,255,255,false)) return false;
    }
    if (road_start_) {
        if (!road_preview_.valid) for (const auto cell:road_preview_.cells) {
            const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
            if (!draw_diamond({top.x,top.y-20},245,60,65,true)) return false;
        }
        return true;
    }
    if (hovered_ && tool_!=(simulation::production_profile(rules_) ? 5 : 4)) {
        const auto top=world_for({static_cast<double>(hovered_->x),static_cast<double>(hovered_->y)});
        const auto result=preview(*hovered_);
        const auto object=tool_==2 ? simulation::Object::ClaySource:
            tool_==3 ? simulation::Object::Pottery:
            tool_==4 ? simulation::Object::Warehouse:
            tool_==7 ? simulation::Object::Household:
            tool_==8 ? simulation::Object::Farm:
            tool_==9 ? simulation::Object::ServicePost:simulation::Object::Empty;
        const auto role=simulation::production_profile(rules_) ?
            building_visual_role(object):std::nullopt;
        const auto* entry=role && building_profile_ ? building_profile_->find(*role):nullptr;
        if (result.accepted && entry && building_visuals_active() && building_sprite_) {
            if (!draw_diamond({top.x,top.y-20},70,245,100,false)) return false;
        } else if (!draw_diamond({top.x,top.y-20},result.accepted?70:255,
            result.accepted?245:65,result.accepted?100:65,true)) return false;
    }
    return true;
}
std::vector<simulation::BuildingId> SandboxView::placed_buildings() const {
    std::vector<simulation::BuildingId> result;
    for (const auto& b:world_->buildings()) if (b.placed) result.push_back(b.id);
    return result;
}
std::optional<simulation::BuildingId> SandboxView::selected_building() const {
    return selected_ ? world_->building_owner_at(*selected_) : std::nullopt;
}
std::vector<std::string> SandboxView::inspection_lines() const {
    std::vector<std::string> lines;
    if (selected_ && world_->object_at(*selected_)==simulation::Object::Road) {
        constexpr const char* names[]{"neg_y","pos_x","pos_y","neg_x"};
        constexpr char hex[]="0123456789abcdef";
        const auto roads=sandbox_ui::road_neighbor_mask(*world_,*selected_);
        const auto entrances=sandbox_ui::entrance_mask(*world_,*selected_);
        lines.push_back("Road cell "+std::to_string(selected_->x)+", "+
                        std::to_string(selected_->y));
        lines.push_back(std::string("Road mask 0x")+hex[roads & 0x0fU]);
        lines.push_back("Road neighbors");
        for (unsigned bit=0;bit<4;++bit)
            lines.push_back(std::string(names[bit])+" "+
                ((roads & (1U<<bit))!=0 ? "yes":"no"));
        lines.push_back("Entrances");
        for (unsigned bit=0;bit<4;++bit)
            lines.push_back(std::string(names[bit])+" "+
                ((entrances & (1U<<bit))!=0 ? "building":"none"));
        const auto* entry=road_profile_ ? road_profile_->find(roads):nullptr;
        lines.push_back(std::string("Road asset configured ")+(entry ? "yes":"no"));
        lines.push_back(std::string("Road visuals ")+(road_visuals_active() ? "ON":"OFF"));
        if (entry) lines.push_back("SG3 "+entry->id.archive_relative_path.generic_string()+
                                  " #"+std::to_string(entry->id.image_index));
        return lines;
    }
    const auto id=selected_building();
    if (!id) {
        lines.push_back("Select a building or list entry");
        lines.push_back("Placed: "+std::to_string(placed_buildings().size()));
        return lines;
    }
    const auto& b=world_->building(*id);
    const auto number=std::to_string(static_cast<unsigned>(*id));
    lines.push_back(object_name(b.kind)+std::string(" #")+number);
    lines.push_back("Cell "+std::to_string(b.cell.x)+", "+std::to_string(b.cell.y));
    if (simulation::city_profile(rules_)) {
        if (b.kind==simulation::Object::Household)
            lines.push_back("Workers supplied "+std::to_string(
                simulation::population_profile(rules_) ? b.population:
                    simulation::Rules::household_workers));
        else {
            const auto required=world_->workforce_required(*id);
            lines.push_back(world_->building_staffed(*id) ?
                "Workers "+std::to_string(required)+"/"+std::to_string(required)+" staffed":
                "Unstaffed - need "+std::to_string(required)+" workers");
        }
    }
    if (b.kind==simulation::Object::Workshop) {
        lines.push_back("Output "+std::to_string(world_->workshop_stock())+"/8");
        lines.push_back("Progress "+std::to_string(world_->production_progress())+"/100");
        lines.push_back("Produced "+std::to_string(world_->total_produced()));
        lines.push_back(world_->workshop_stock()==simulation::Rules::workshop_capacity ?
            "Output full" : "Workshop active");
    } else if (b.kind==simulation::Object::ClaySource) {
        lines.push_back("Clay output "+std::to_string(b.output)+"/8");
        lines.push_back("Progress "+std::to_string(b.progress)+"/100");
        lines.push_back("Extracted "+std::to_string(b.clay_extracted));
        lines.push_back(b.output==simulation::Rules::clay_output_capacity ? "Output full" :
            "Extraction active");
    } else if (b.kind==simulation::Object::Pottery) {
        lines.push_back("Clay input "+std::to_string(b.input_clay)+"/8");
        lines.push_back("Reserved input "+std::to_string(b.reserved_incoming));
        lines.push_back("Recipe Clay "+std::to_string(b.active_recipe_clay));
        lines.push_back("Recipe "+std::to_string(b.progress)+"/150");
        lines.push_back("Pottery output "+std::to_string(b.output)+"/8");
        lines.push_back("Completed "+std::to_string(b.recipes_completed));
        lines.push_back(b.active_recipe_clay>0 ? "Processing" :
            b.output>=simulation::Rules::pottery_output_capacity ? "Output full" :
            b.input_clay<simulation::Rules::pottery_recipe_clay ? "No Clay input" : "Ready");
    } else if (b.kind==simulation::Object::Warehouse) {
        const int stock=simulation::production_profile(rules_) ? b.pottery_stock :
            world_->warehouse_stock();
        lines.push_back("Pottery stock "+std::to_string(stock)+"/32");
        lines.push_back("Reserved input "+std::to_string(b.reserved_incoming));
        lines.push_back("Free "+std::to_string(simulation::Rules::warehouse_capacity-stock-
            b.reserved_incoming));
    } else if (b.kind==simulation::Object::Household) {
        lines.push_back("Pottery stock "+std::to_string(b.pottery_stock)+"/8");
        lines.push_back("Reserved "+std::to_string(b.reserved_incoming));
        lines.push_back("Demand clock "+std::to_string(b.demand_progress)+"/400");
        lines.push_back("Fulfilled "+std::to_string(b.fulfilled_demand)+
            " Missed "+std::to_string(b.missed_demand));
        lines.push_back("Consumed "+std::to_string(b.consumed_total));
        if (simulation::food_profile(rules_)) {
            const auto next=b.fulfilled_demand+1;
            const auto next_tax=next>=simulation::Rules::city_v7_level2_demands ? 60:
                next>=simulation::Rules::city_v7_level1_demands ? 40:25;
            lines.push_back("House level "+std::to_string(world_->household_level(*id)));
            if (simulation::population_profile(rules_))
                lines.push_back("Population "+std::to_string(b.population)+"/"+
                    std::to_string(world_->household_population_capacity(*id)));
            lines.push_back("Food stock "+std::to_string(b.food_stock)+"/8");
            lines.push_back("Food reserved "+std::to_string(b.reserved_food_incoming));
            lines.push_back("Food consumed "+std::to_string(b.food_consumed_total));
            lines.push_back("Next fulfilled tax "+std::to_string(next_tax));
            lines.push_back("Taxes earned "+std::to_string(world_->household_tax_contributed(*id)));
            if (simulation::service_profile(rules_)) {
                lines.push_back(std::string("Service ")+
                    (world_->household_service_active(*id) ? "active":"expired"));
                lines.push_back("Coverage remaining "+
                    std::to_string(world_->household_service_remaining(*id))+" ticks");
                std::string missing;
                const auto add_missing=[&](const char* value) {
                    if (!missing.empty()) missing+=" + ";
                    missing+=value;
                };
                if (b.pottery_stock==0) add_missing("Pottery");
                if (b.food_stock==0) add_missing("Food");
                if (!world_->household_service_active(*id)) add_missing("Service");
                lines.push_back(missing.empty() ? "Demand ready":"Missing "+missing);
            }
        } else if (rules_==simulation::RulesProfile::CityV6) {
            lines.push_back("Tax per supplied demand 25");
            lines.push_back("Taxes earned "+std::to_string(world_->household_tax_contributed(*id)));
        }
        lines.push_back(b.last_demand_status==0 ? "Demand: not due" :
            b.last_demand_status==1 ? "Demand: supplied" : "Demand: unmet");
    } else if (b.kind==simulation::Object::Farm) {
        lines.push_back("Food output "+std::to_string(b.output)+"/12");
        lines.push_back("Progress "+std::to_string(b.progress)+"/80");
        lines.push_back("Produced "+std::to_string(b.food_produced));
    } else if (b.kind==simulation::Object::ServicePost) {
        const auto found=std::find_if(world_->couriers().begin(),world_->couriers().end(),
            [&](const simulation::CourierState& c) { return c.owner==b.id; });
        if (found==world_->couriers().end()) return lines;
        const auto& c=*found;
        lines.push_back("Covered houses "+std::to_string(world_->covered_households())+"/"+
            std::to_string(std::count_if(world_->buildings().begin(),world_->buildings().end(),
                [](const simulation::BuildingState& value) {
                    return value.placed && value.kind==simulation::Object::Household;
                })));
        lines.push_back(std::string("Walker phase ")+simulation::delivery_phase_name(c.phase));
        lines.push_back("Current target "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
            std::string("-"):std::to_string(static_cast<unsigned>(c.target))));
        const auto decision=world_->courier_dispatch_status(c.id);
        lines.push_back(std::string("Road status ")+
            simulation::courier_dispatch_status_name(decision.status));
        if (world_->last_dispatched_service_household())
            lines.push_back("Last target "+std::to_string(static_cast<unsigned>(
                *world_->last_dispatched_service_household())));
    }
    if (const auto role=building_visual_role(b.kind)) {
        lines.push_back(std::string("Building visuals ")+(building_enabled_ ? "ON":"OFF"));
        lines.push_back(std::string("Visual role ")+assets::building_role_name(*role));
        const auto* entry=building_profile_ ? building_profile_->find(*role):nullptr;
        lines.push_back(std::string("Visual configured ")+(entry ? "yes":"no"));
        if (entry) {
            const auto& image=building_profile_->unique_images.at(entry->image_index);
            lines.push_back("SG3 "+entry->id.archive_relative_path.generic_string()+
                " #"+std::to_string(entry->id.image_index));
            lines.push_back("Image "+std::to_string(image.width)+"x"+
                std::to_string(image.height));
            lines.push_back("Anchor "+std::to_string(entry->ground_x)+","+
                std::to_string(entry->ground_y));
            lines.push_back("Evidence: curated preview");
        }
    }
    if (simulation::production_profile(rules_)) {
        for (const auto& c:world_->couriers()) {
            if (!c.enabled || c.owner!=*id) continue;
            lines.push_back("Courier #"+std::to_string(static_cast<std::uint32_t>(c.id))+" "+
                (c.role==simulation::CourierRole::Clay ? "Clay" :
                 c.role==simulation::CourierRole::Pottery ? "Pottery" :
                 c.role==simulation::CourierRole::Food ? "Food" :
                 c.role==simulation::CourierRole::Service ? "Service" : "House supply"));
            lines.push_back("Target "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
                std::string("-"):std::to_string(static_cast<unsigned>(c.target)))+
                " cargo "+std::to_string(c.cargo));
            lines.push_back(std::string("Phase ")+simulation::delivery_phase_name(c.phase));
            const auto decision=world_->courier_dispatch_status(c.id);
            lines.push_back(std::string("Status ")+
                simulation::courier_dispatch_status_name(decision.status));
            if (decision.selected_target)
                lines.push_back("Next target "+
                    std::to_string(static_cast<unsigned>(*decision.selected_target)));
            break;
        }
    } else if (b.kind==simulation::Object::Warehouse || b.kind==simulation::Object::Workshop) {
        lines.push_back(std::string("Courier ")+simulation::courier_phase_name(world_->courier_phase()));
        lines.push_back("Cargo "+std::to_string(world_->courier_cargo()));
    }
    return lines;
}
bool SandboxView::draw_text(double x,double y,const std::string& value,int max_width) {
    if (max_width<=0) return true;
    const float scale=1.25F*static_cast<float>(layout_.scale);
    const int max_chars=static_cast<int>(static_cast<float>(max_width)/(8.0F*scale));
    if (max_chars<=0) return true;
    std::string text=value;
    if (static_cast<int>(text.size())>max_chars) {
        text.resize(static_cast<std::size_t>(max_chars));
        if (max_chars>=3) text.replace(text.size()-3,3,"...");
    }
    if (!SDL_SetRenderScale(renderer_,scale,scale)) return false;
    const bool okay=SDL_RenderDebugText(renderer_,static_cast<float>(x/scale),
                                        static_cast<float>(y/scale),text.c_str());
    return SDL_SetRenderScale(renderer_,1,1) && okay;
}
bool SandboxView::draw_hud() {
    using A=sandbox_ui::Action;
    const auto fill=[&](sandbox_ui::Rect rect,SDL_Color color)->bool {
        if (rect.w<=0 || rect.h<=0) return true;
        const SDL_FRect area{static_cast<float>(rect.x),static_cast<float>(rect.y),
            static_cast<float>(rect.w),static_cast<float>(rect.h)};
        return SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,color.a) &&
            SDL_RenderFillRect(renderer_,&area);
    };
    if (!fill(layout_.top,{14,22,34,255}) || !fill(layout_.toolbar,{16,24,36,255}) ||
        !fill(layout_.status,{13,21,31,255}) ||
        !fill(layout_.panel,{19,29,43,250})) return false;
    if (!SDL_SetRenderDrawColor(renderer_,230,236,244,255)) return false;
    const std::string profile=debug_open_ ? simulation::rules_profile_name(rules_):profile_title(rules_);
    std::string overview=profile+" | Tick "+std::to_string(world_->ticks())+" | "+
        (clock_.paused()?"Paused ":"Running ")+std::to_string(clock_.speed())+"x";
    if (simulation::city_profile(rules_)) {
        overview+=" | Funds "+std::to_string(world_->treasury())+" | Workers ";
        if (simulation::population_profile(rules_))
            overview+=std::to_string(world_->workforce_supply())+"/"+
                std::to_string(world_->workforce_required());
        else
            overview+=std::to_string(world_->workforce_used())+"/"+
                std::to_string(world_->workforce_supply());
    }
    if (simulation::population_profile(rules_))
        overview+=" | Pop "+std::to_string(world_->total_population());
    if (simulation::food_profile(rules_)) {
        int food=0; std::size_t houses=0;
        for (const auto& b:world_->buildings()) {
            if (b.kind==simulation::Object::Farm) food+=b.output;
            if (b.kind==simulation::Object::Household) { food+=b.food_stock; ++houses; }
        }
        overview+=" | Food "+std::to_string(food);
        if (simulation::service_profile(rules_))
            overview+=" | Service "+std::to_string(world_->covered_households())+"/"+
                std::to_string(houses);
        overview+=" | Goal "+std::to_string(world_->settlement_goal_households_ready())+"/"+
            std::to_string(rules_==simulation::RulesProfile::CityV10 ?
                           simulation::Rules::city_v10_goal_level2_households:4);
        if (simulation::population_profile(rules_))
            overview+=" Pop "+std::to_string(world_->total_population())+"/"+
                std::to_string(rules_==simulation::RulesProfile::CityV10 ?
                    simulation::Rules::city_v10_population_goal:
                    simulation::Rules::city_v9_population_goal);
    }
    if (simulation::production_profile(rules_)) {
        int stored=0; for (const auto& b:world_->buildings())
            if (b.kind==simulation::Object::Warehouse) stored+=b.pottery_stock;
        overview+=" | Clay "+std::to_string(world_->clay_extracted_total())+" | Pottery "+
            std::to_string(world_->pottery_completed_total())+" | Store "+std::to_string(stored);
    }
    else overview+=" | Goods "+std::to_string(world_->total_produced());
    if (!draw_text(8*layout_.scale,18*layout_.scale,overview,
        layout_.top.w-200*layout_.scale)) return false;
    if (managed_ && !draw_text(layout_.top.w-82*layout_.scale,18*layout_.scale,
                               "[ MENU ]",80*layout_.scale)) return false;
    std::string status=road_start_ && !road_preview_.valid ? road_preview_.reason :
        last_message_.empty() ? "OpenEmperor sandbox | Select a tool, then click the map" :
        last_message_;
    if (simulation::city_profile(rules_) && road_start_ &&
        !road_preview_.valid && road_preview_.reason=="Not enough money") {
        const auto count=std::count_if(road_preview_.cells.begin(),road_preview_.cells.end(),
            [&](simulation::Cell cell) { return world_->object_at(cell)!=simulation::Object::Road; });
        status="Need "+std::to_string(count*simulation::Rules::road_cost)+
            " funds; treasury "+std::to_string(world_->treasury());
    }
    if (simulation::city_profile(rules_) && hovered_ &&
        tool_!=(simulation::production_profile(rules_) ? 5:4)) {
        const auto result=preview(*hovered_);
        if (!result.accepted && std::string_view(result.reason)=="Not enough money") {
            const auto cost=world_->construction_cost(command_type(rules_,tool_));
            status="Need "+std::to_string(cost)+" funds; treasury "+
                std::to_string(world_->treasury());
        }
    }
    if (simulation::city_profile(rules_))
        status+=world_->settlement_goal_reached() ?
            (simulation::population_profile(rules_) ?
                " | GOAL REACHED - houses developed and population stable":
             simulation::food_profile(rules_) ?
                " | GOAL REACHED - all houses level 2":" | GOAL REACHED - settlement supplied") :
            " | Goal "+std::to_string(world_->settlement_goal_households_ready())+
                (simulation::food_profile(rules_) ?
                    (rules_==simulation::RulesProfile::CityV10 ? "/8 houses at level 2":
                     "/4 houses at level 2"):"/4 households ready");
    if (simulation::population_profile(rules_) && !world_->settlement_goal_reached())
        status+=" | Population "+std::to_string(world_->total_population())+"/"+
            std::to_string(rules_==simulation::RulesProfile::CityV10 ?
                simulation::Rules::city_v10_population_goal:simulation::Rules::city_v9_population_goal);
    if (debug_open_ && walker_profile_) {
        status+=" | Walkers ";
        status+=walker_visuals_enabled_ ? "ON ":"OFF ";
        for (std::size_t r=0;r<3;++r) {
            if (r) status+=' ';
            status+=assets::walker_role_name(static_cast<assets::WalkerVisualRole>(r));
            status+=walker_profile_->roles[r] ? ":yes":":no";
        }
    }
    if (!draw_text(8*layout_.scale,layout_.status.y+7*layout_.scale,status,
                   layout_.status.w-16*layout_.scale)) return false;
    const auto label=[&](A action)->std::string {
        const auto count=[&](simulation::Object kind) {
            return std::count_if(world_->buildings().begin(),world_->buildings().end(),
                [&](const simulation::BuildingState& b) { return b.placed && b.kind==kind; });
        };
        const bool v10=rules_==simulation::RulesProfile::CityV10;
        switch (action) {
        case A::Select: return simulation::production_profile(rules_) ? "5 Select":"4 Select";
        case A::Road: return simulation::city_profile(rules_) ? "1 Road $2":"1 Road";
        case A::Clay: return v10 ? "2 Clay "+std::to_string(count(simulation::Object::ClaySource))+
            "/4 $120":simulation::city_profile(rules_) ? "2 Clay $120":
            simulation::production_profile(rules_) ? "2 Clay" : "2 Workshop";
        case A::Pottery: return v10 ? "3 Pottery "+std::to_string(count(simulation::Object::Pottery))+
            "/4 $180":simulation::city_profile(rules_) ? "3 Pottery $180":"3 Pottery";
        case A::Warehouse: return v10 ? "4 Store "+std::to_string(count(simulation::Object::Warehouse))+
            "/2 $150":simulation::city_profile(rules_) ? "4 Store $150":
            simulation::production_profile(rules_) ? "4 Store" : "3 Store";
        case A::RemoveRoad: return "6 Remove";
        case A::Household: return v10 ? "7 House "+std::to_string(count(simulation::Object::Household))+
            "/20 $80":simulation::city_profile(rules_) ? "7 House $80":"7 House";
        case A::Farm: return v10 ? "8 Farm "+std::to_string(count(simulation::Object::Farm))+
            "/2 $160":"8 Farm $160";
        case A::ServicePost: return v10 ? "9 Service "+
            std::to_string(count(simulation::Object::ServicePost))+"/2 $100":"9 Service $100";
        case A::Pause: return clock_.paused()?"Continue":"Pause";
        case A::Step: return "Step";
        case A::Speed1: return "1x";
        case A::Speed2: return "2x";
        case A::Speed4: return "4x";
        case A::Reset: return "Reset";
        case A::Save: return "Save F5";
        case A::Load: return "Load F9";
        case A::TogglePanel: return layout_.panel_open ? "Hide info":"Show info";
        case A::ToggleHelp: return help_open_ ? "Hide help":"Help";
        }
        return "";
    };
    for (const auto& button:layout_.buttons) {
        bool active=false;
        switch (button.action) {
        case A::Select: active=tool_==(simulation::production_profile(rules_) ? 5:4); break;
        case A::Road: active=tool_==1; break;
        case A::Clay: active=tool_==2; break;
        case A::Pottery: active=simulation::production_profile(rules_) && tool_==3; break;
        case A::Warehouse: active=tool_==(simulation::production_profile(rules_) ? 4:3); break;
        case A::RemoveRoad: active=tool_==6; break;
        case A::Household: active=tool_==7; break;
        case A::Farm: active=tool_==8; break;
        case A::ServicePost: active=tool_==9; break;
        default: break;
        }
        const bool enabled=action_enabled(button.action);
        if (!fill(button.rect,!enabled ? SDL_Color{43,47,55,255}:
            active ? SDL_Color{38,100,125,255}:SDL_Color{38,58,77,255})) return false;
        if (!SDL_SetRenderDrawColor(renderer_,enabled ? 245:135,enabled ? 247:140,
                                    enabled ? 250:145,255)) return false;
        std::string text=label(button.action);
        if (button.action==A::Clay || button.action==A::Pottery ||
            button.action==A::Warehouse || button.action==A::Household ||
            button.action==A::Farm || button.action==A::ServicePost) {
            const auto wanted=button.action==A::Clay ?
                (simulation::production_profile(rules_) ? simulation::Object::ClaySource:
                 simulation::Object::Workshop) :
                button.action==A::Pottery ? simulation::Object::Pottery :
                button.action==A::Warehouse ? simulation::Object::Warehouse:
                button.action==A::Household ? simulation::Object::Household:
                button.action==A::Farm ? simulation::Object::Farm:
                simulation::Object::ServicePost;
            int count=0;
            for (const auto id:placed_buildings()) if (world_->building(id).kind==wanted) ++count;
            const int limit=button.action==A::Household ?
                (simulation::industry_profile(rules_) ||
                 rules_==simulation::RulesProfile::SettlementV4 ? 4:1) :
                simulation::industry_profile(rules_) &&
                (button.action==A::Clay || button.action==A::Pottery) ? 2:1;
            text+=" "+std::to_string(count)+"/"+std::to_string(limit);
        }
        if (!draw_text(button.rect.x+5*layout_.scale,button.rect.y+10*layout_.scale,
                       text,button.rect.w-8*layout_.scale)) return false;
    }
    if (layout_.panel_open) {
        if (!SDL_SetRenderDrawColor(renderer_,240,244,250,255)) return false;
        if (!draw_text(layout_.panel.x+10*layout_.scale,layout_.panel.y+14*layout_.scale,
                       "BUILDINGS",layout_.panel.w-20*layout_.scale)) return false;
        const auto entries=placed_buildings();
        for (std::size_t i=0;i<entries.size();++i) {
            const auto& b=world_->building(entries[i]);
            const int y=layout_.panel.y+46*layout_.scale+
                static_cast<int>(i)*18*layout_.scale-panel_scroll_;
            if (y<layout_.panel.y || y+10*layout_.scale>=layout_.panel.y+layout_.panel.h) continue;
            const std::string text=std::to_string(static_cast<unsigned>(b.id))+" "+
                object_name(b.kind)+" ("+std::to_string(b.cell.x)+","+
                std::to_string(b.cell.y)+")";
            if (selected_building()==entries[i]) {
                if (!fill({layout_.panel.x+4*layout_.scale,y-2*layout_.scale,
                    layout_.panel.w-8*layout_.scale,16*layout_.scale},{34,92,113,255})) return false;
            }
            if (!SDL_SetRenderDrawColor(renderer_,235,240,250,255) ||
                !draw_text(layout_.panel.x+10*layout_.scale,y,text,
                           layout_.panel.w-20*layout_.scale)) return false;
        }
        const int detail_y=layout_.panel.y+52*layout_.scale+
            static_cast<int>(entries.size())*18*layout_.scale-panel_scroll_;
        const auto details=inspection_lines();
        for (std::size_t i=0;i<details.size();++i) {
            const int y=detail_y+static_cast<int>(i)*17*layout_.scale;
            if (y<layout_.panel.y || y+10*layout_.scale>=layout_.panel.y+layout_.panel.h) continue;
            if (!SDL_SetRenderDrawColor(renderer_,205,225,238,255) ||
                !draw_text(layout_.panel.x+10*layout_.scale,y,details[i],
                           layout_.panel.w-20*layout_.scale)) return false;
        }
    }
    if (debug_open_) {
        if (!SDL_SetRenderDrawColor(renderer_,255,230,150,255)) return false;
        const bool balance_valid=simulation::production_profile(rules_) ?
            world_->production_balance_valid():world_->goods_balance_valid();
        std::string debug="Road revision "+std::to_string(world_->road_revision())+
            " | Commands "+std::to_string(world_->command_sequence())+
            " | Balance "+(balance_valid?"OK":"ERROR");
        if (simulation::city_profile(rules_))
            debug+=" | Taxes "+std::to_string(world_->taxes_collected_total())+
                " | Spent "+std::to_string(world_->construction_spent_total())+
                " | Worker need "+std::to_string(world_->workforce_required())+
                " | Goal "+std::to_string(world_->settlement_goal_households_ready())+"/"+
                std::to_string(rules_==simulation::RulesProfile::CityV10 ? 8:4);
        if (simulation::food_profile(rules_))
            debug+=" | Food produced "+std::to_string(world_->food_produced_total())+
                " | Food balance "+(world_->food_balance_valid()?"OK":"ERROR");
        if (simulation::service_profile(rules_))
            debug+=" | Service "+std::to_string(world_->covered_households())+"/"+
                std::to_string(std::count_if(world_->buildings().begin(),world_->buildings().end(),
                    [](const simulation::BuildingState& b) {
                        return b.placed && b.kind==simulation::Object::Household;
                    }))+
                " | Service state "+(world_->service_state_valid()?"OK":"ERROR");
        if (!draw_text(8*layout_.scale,layout_.map.y+8*layout_.scale,debug,
                       layout_.map.w-16*layout_.scale)) return false;
        int debug_row=22;
        if (simulation::population_profile(rules_)) {
            std::string staffing="Staffing order:";
            for (const auto& b:world_->buildings()) {
                if (!b.placed || world_->workforce_required(b.id)==0) continue;
                staffing+=" "+std::to_string(static_cast<std::uint32_t>(b.id))+" "+object_name(b.kind)+
                    (world_->building_staffed(b.id)?" yes":" no");
            }
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,staffing,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
        }
        const auto roads=road_display_stats();
        const std::string road_line=std::string("Road visuals ")+
            (road_visuals_active()?"ON":"OFF")+" | masks "+
            std::to_string(road_profile_ ? road_profile_->configured_count():0)+
            "/16 | assets "+std::to_string(roads.unique_assets)+
            " | fallbacks "+std::to_string(roads.fallback_draws);
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,road_line,
                       layout_.map.w-16*layout_.scale)) return false;
        debug_row+=14;
        const std::string depth_line=std::string("Depth painter: ")+
            (unified_depth_ ? "unified":"legacy")+" | stored "+
            std::to_string(painter_stats_.stored_items_visited)+" | sandbox "+
            std::to_string(painter_stats_.sandbox_items);
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,depth_line,
                       layout_.map.w-16*layout_.scale)) return false;
        debug_row+=14;
        const auto debug_source=[](VisualProfileSource source) {
            return source==VisualProfileSource::Builtin ? "auto":
                visual_profile_source_name(source);
        };
        const std::string visual_line="Compatibility: "+compatibility_id_+
            " | Walker: "+debug_source(walker_source_)+
            " | Buildings: "+debug_source(building_source_)+
            " | Roads: "+debug_source(road_source_);
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,visual_line,
                       layout_.map.w-16*layout_.scale)) return false;
    }
    return true;
}
bool SandboxView::draw_walker_diagnostic() {
    if (!walker_diagnostic_open_ || !walker_profile_ || !walker_sprites_) return true;
    const float scale=static_cast<float>(layout_.scale);
    const SDL_FRect panel{static_cast<float>(layout_.map.x)+8*scale,
        static_cast<float>(layout_.map.y)+8*scale,
        std::min(470*scale,static_cast<float>(layout_.map.w)-16*scale),320*scale};
    if (!SDL_SetRenderDrawColor(renderer_,walker_diagnostic_light_ ? 232:22,
        walker_diagnostic_light_ ? 232:26,walker_diagnostic_light_ ? 220:34,255) ||
        !SDL_RenderFillRect(renderer_,&panel)) return false;
    if (!SDL_SetRenderDrawColor(renderer_,walker_diagnostic_light_ ? 15:245,
        walker_diagnostic_light_ ? 20:245,walker_diagnostic_light_ ? 30:245,255)) return false;
    constexpr const char* directions[]={"pos_x  right/down","neg_x  left/up",
        "pos_y  left/down","neg_y  right/up"};
    const auto label=[&](int row,const std::string& value)->bool {
        return draw_text(panel.x+8*scale,panel.y+static_cast<float>(12+row*18)*scale,value,
                         static_cast<int>(std::min(255*scale,panel.w-16*scale)));
    };
    const auto role=static_cast<assets::WalkerVisualRole>(walker_diagnostic_role_);
    const auto* visual=walker_profile_->find(role);
    if (!label(0,std::string("WALKER ")+assets::walker_role_name(role)+" | F3 close") ||
        !label(1,std::string(directions[walker_diagnostic_direction_])+
            (walker_diagnostic_zoom4_ ? "  4x":"  1x")) ||
        !label(2,"V role Q dir C/E X zoom B bg")) return false;
    if (!visual) return label(3,"UNCONFIGURED - no original series");
    const auto& clip=visual->clips[walker_diagnostic_direction_];
    if (clip.empty()) return label(3,"UNMAPPED - no image selected");
    const auto step=walker_diagnostic_step_%clip.size();
    const auto& frame=visual->frames[clip[step]];
    const auto& image=walker_profile_->unique_images[frame.image_index];
    const auto compact=[](double value) {
        auto text=std::to_string(value);
        while (!text.empty() && text.back()=='0') text.pop_back();
        if (!text.empty() && text.back()=='.') text.pop_back();
        return text;
    };
    if (!label(3,"Physical #"+std::to_string(frame.id.image_index)+"  step "+
        std::to_string(step+1)+"/"+std::to_string(clip.size())) ||
        !label(4,"Size "+std::to_string(image.width)+"x"+std::to_string(image.height)+
            "  foot "+compact(frame.foot_x)+","+compact(frame.foot_y)) ||
        !label(5,frame.id.archive_relative_path.generic_string()) ||
        !label(6,"tick/frame "+std::to_string(visual->ticks_per_frame)+" | "+visual->evidence))
        return false;
    const double zoom=(walker_diagnostic_zoom4_ ? 4.0:1.0)*layout_.scale;
    const scene::Point ground{panel.x+panel.w-105*scale,panel.y+300*scale};
    const SDL_FRect bounds{static_cast<float>(ground.x-frame.foot_x*zoom),
        static_cast<float>(ground.y-frame.foot_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    if (!walker_sprites_->draw(clip[step],ground,zoom,*visual,*walker_profile_,
            {panel.x,panel.y},{panel.x+panel.w,panel.y+panel.h}) ||
        !SDL_SetRenderDrawColor(renderer_,20,210,245,255) ||
        !SDL_RenderRect(renderer_,&bounds)) return false;
    const float gx=static_cast<float>(ground.x),gy=static_cast<float>(ground.y);
    return SDL_SetRenderDrawColor(renderer_,245,225,40,255) &&
        SDL_RenderLine(renderer_,gx-5*scale,gy,gx+5*scale,gy) &&
        SDL_RenderLine(renderer_,gx,gy-5*scale,gx,gy+5*scale);
}
bool SandboxView::draw_help_overlay() {
    if (!help_open_) return true;
    const int margin=12*layout_.scale;
    const int x=layout_.map.x+margin;
    const int y=layout_.map.y+margin;
    const int width=std::max(0,layout_.map.w-2*margin);
    const int height=std::max(0,std::min(layout_.map.h-2*margin,250*layout_.scale));
    if (width<=0 || height<=0) return true;
    const SDL_FRect panel{static_cast<float>(x),static_cast<float>(y),
        static_cast<float>(width),static_cast<float>(height)};
    if (!SDL_SetRenderDrawColor(renderer_,12,19,30,248) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,235,241,248,255)) return false;
    const int pad=12*layout_.scale;
    const int line=16*layout_.scale;
    const int column=std::max(160*layout_.scale,(width-3*pad)/2);
    const auto text=[&](int col,int row,const std::string& value)->bool {
        return draw_text(x+pad+col*column,y+pad+row*line,value,column-pad);
    };
    return text(0,0,"HELP - H / ? closes") &&
        text(0,2,"GAMEPLAY") && text(0,3,"1 Road | 2 Clay | 3 Pottery") &&
        text(0,4,"4 Store | 5 Select") && text(0,5,"6 Remove | 7 House | 8 Farm") &&
        text(0,6,"9 Service (City v8/v9)") &&
        text(0,7,"Space Pause | . Single step") && text(0,8,"+ / - Simulation speed") &&
        text(0,9,"F5 Save | F9 Load") &&
        text(0,10,"CAMERA AND MENU") && text(0,11,"WASD / Arrow keys Move") &&
        text(0,12,"Mouse wheel Zoom | R Reset") && text(0,13,"Tab Info panel | Esc Menu") &&
        text(1,2,"ADVANCED VISUAL DIAGNOSTICS") &&
        text(1,3,"F2 Walker graphics") && text(1,4,"F3 Walker frame inspector") &&
        text(1,5,"F4 Building graphics") && text(1,6,"F6 Road graphics") &&
        text(1,7,"F7 Depth comparison") &&
        text(1,9,"DEBUG") && text(1,10,"F1 Runtime debug overlay") &&
        text(1,12,"These views are diagnostic") && text(1,13,"and do not change the world.");
}
bool SandboxView::render() {
    resize_camera();
    if (!SDL_SetRenderViewport(renderer_,nullptr) ||
        !SDL_SetRenderClipRect(renderer_,nullptr) ||
        !SDL_SetRenderScale(renderer_,1,1) ||
        !SDL_SetRenderDrawColor(renderer_,22,26,32,255) || !SDL_RenderClear(renderer_)) return false;
    const SDL_Rect map_clip{layout_.map.x,layout_.map.y,layout_.map.w,layout_.map.h};
    if (layout_.map.w>0 && layout_.map.h>0) {
        if (!SDL_SetRenderClipRect(renderer_,&map_clip)) return false;
        auto render_camera=camera_;
        render_camera.viewport_width=layout_.map.x+layout_.map.w;
        render_camera.viewport_height=layout_.map.y+layout_.map.h;
        const bool map_ok=(!unified_depth_ ? background_.render(render_camera,std::nullopt):true) &&
            draw_world(render_camera);
        if (!SDL_SetRenderClipRect(renderer_,nullptr) || !map_ok) return false;
    }
    const bool ui_ok=draw_hud() && draw_walker_diagnostic() && draw_help_overlay();
    const bool reset=SDL_SetRenderClipRect(renderer_,nullptr) &&
        SDL_SetRenderViewport(renderer_,nullptr) && SDL_SetRenderScale(renderer_,1,1);
    if (!ui_ok || !reset) return false;
    return SDL_GetRenderTarget(renderer_) ? true:SDL_RenderPresent(renderer_);
}

} // namespace openemperor
