#include "app/SandboxView.h"

#include "maps/SandboxPlacement.h"
#include "renderer/StoredCamera.h"

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
        default: return simulation::CommandType::PlaceHousehold;
        }
    }
    switch (tool) {
    case 1: return simulation::CommandType::PlaceRoad;
    case 2: return simulation::CommandType::PlaceWorkshop;
    default: return simulation::CommandType::PlaceWarehouse;
    }
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
    background_.initialize(renderer_);
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
    SDL_SetWindowTitle(window_,rules_==simulation::RulesProfile::IndustryV5 ?
        "OpenEmperor | Industry Sandbox - prototype rules" :
        rules_==simulation::RulesProfile::SettlementV4 ?
        "OpenEmperor | Settlement Sandbox - prototype rules" :
        rules_==simulation::RulesProfile::HouseholdV3 ?
        "OpenEmperor | Household Sandbox - prototype rules" :
        rules_==simulation::RulesProfile::ProductionV2 ?
        "OpenEmperor | Production Sandbox - prototype rules" :
        "OpenEmperor | Logistics Sandbox - prototype rules");
}
void SandboxView::shutdown() {
    cancel_gesture();
    background_.shutdown();
    world_.reset();
    window_=nullptr;
    renderer_=nullptr;
}
void SandboxView::reset_camera() {
    fit_stored_camera(background_.plan(),camera_);
    camera_.offset.x+=layout_.map.x;
    camera_.offset.y+=layout_.map.y;
    if (demo_origin_) {
        const auto center=maps::terrain_world({static_cast<std::uint32_t>(demo_origin_->x+
            (rules_==simulation::RulesProfile::IndustryV5 ? 6 :
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
    default: return;
    }
    if (action_enabled(action)) perform_action(action);
}
bool SandboxView::action_enabled(sandbox_ui::Action action) const {
    if (action==sandbox_ui::Action::Save || action==sandbox_ui::Action::Load)
        return !save_path_.empty();
    if (action==sandbox_ui::Action::RemoveRoad)
        return simulation::production_profile(rules_);
    if (action==sandbox_ui::Action::Household)
        return simulation::household_profile(rules_) &&
            world_->next_household_id()<simulation::household_id_end;
    if (action==sandbox_ui::Action::Clay || action==sandbox_ui::Action::Pottery ||
        action==sandbox_ui::Action::Warehouse) {
        const auto wanted=action==sandbox_ui::Action::Clay ?
            (simulation::production_profile(rules_) ? simulation::Object::ClaySource :
                simulation::Object::Workshop) :
            action==sandbox_ui::Action::Pottery ? simulation::Object::Pottery :
                simulation::Object::Warehouse;
        if (action==sandbox_ui::Action::Pottery && !simulation::production_profile(rules_)) return false;
        int count=0;
        for (unsigned id=1;id<=9;++id) {
            const auto& b=world_->building(static_cast<simulation::BuildingId>(id));
            if (b.placed && b.kind==wanted) ++count;
        }
        const int limit=(rules_==simulation::RulesProfile::IndustryV5 &&
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
        last_message_=save_path_.empty() && (action==A::Save || action==A::Load) ?
            "No sandbox save path configured" : "Tool unavailable or building limit reached";
        return;
    }
    if (action==A::Select || action==A::Road || action==A::Clay || action==A::Pottery ||
        action==A::Warehouse || action==A::RemoveRoad || action==A::Household) {
        cancel_gesture();
        tool_=action==A::Select ? (simulation::production_profile(rules_) ? 5:4) :
            action==A::Road ? 1 : action==A::Clay ? 2 : action==A::Pottery ? 3 :
            action==A::Warehouse ? (simulation::production_profile(rules_) ? 4:3) :
            action==A::RemoveRoad ? 6:7;
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
        catch (const std::exception& error) { last_message_=error.what(); }
    } else if (action==A::TogglePanel) {
        cancel_gesture();
        panel_open_=!panel_open_;
        update_layout(true);
    } else if (action==A::ToggleDebug) debug_open_=!debug_open_;
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
        if (event.key.key==SDLK_ESCAPE) {
            if (map_pressed_ || road_start_ || pressed_button_ || ui_pressed_ || menu_pressed_)
                cancel_gesture();
            else if (managed_) menu_requested_=true;
            else running=false;
            return;
        }
        if (event.key.key>=SDLK_1 && event.key.key<=
            (simulation::household_profile(rules_) ? SDLK_7 :
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
        else if (event.key.key==SDLK_TAB) perform_action(sandbox_ui::Action::TogglePanel);
        else if (event.key.key==SDLK_F1) perform_action(sandbox_ui::Action::ToggleDebug);
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
bool SandboxView::draw_world() {
    last_courier_draws_=0;
    for (int y=0;y<world_->height();++y) for (int x=0;x<world_->width();++x) {
        const auto object=world_->object_at({x,y});
        if (object==simulation::Object::Empty) continue;
        const auto top=world_for({static_cast<double>(x),static_cast<double>(y)});
        const auto center=camera_.world_to_screen(top);
        if (center.x<layout_.map.x-90 || center.y<layout_.map.y-90 ||
            center.x>layout_.map.x+layout_.map.w+90 ||
            center.y>layout_.map.y+layout_.map.h+90) continue;
        if (object==simulation::Object::Road) {
            if (!draw_diamond({top.x,top.y-20},225,174,65,true)) return false;
            for (const simulation::Cell next : {simulation::Cell{x+1,y},simulation::Cell{x,y+1},
                                                simulation::Cell{x-1,y},simulation::Cell{x,y-1}}) {
                const auto other=world_->object_at(next);
                if (other==simulation::Object::Empty ||
                    (other==simulation::Object::Road && (next.x<x || next.y<y))) continue;
                const auto screen=camera_.world_to_screen(world_for({static_cast<double>(next.x),
                    static_cast<double>(next.y)}));
                if (!SDL_SetRenderDrawColor(renderer_,255,215,94,255) ||
                    !SDL_RenderLine(renderer_,static_cast<float>(center.x),static_cast<float>(center.y),
                        static_cast<float>(screen.x),static_cast<float>(screen.y))) return false;
            }
        } else {
            SDL_Color color{255,105,100,255};
            if (object==simulation::Object::Workshop || object==simulation::Object::ClaySource)
                color={50,210,245,255};
            if (object==simulation::Object::Pottery) color={220,95,245,255};
            if (object==simulation::Object::Household) color={90,245,125,255};
            if (!draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true)) return false;
            const float size=static_cast<float>(std::max(5.0,11.0*camera_.zoom));
            const SDL_FRect rect{static_cast<float>(center.x)-size/2,
                static_cast<float>(center.y)-size*1.5F,size,size};
            if (!SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) ||
                !SDL_RenderFillRect(renderer_,&rect)) return false;
            if (object==simulation::Object::Household ||
                (rules_==simulation::RulesProfile::IndustryV5 &&
                 (object==simulation::Object::ClaySource || object==simulation::Object::Pottery))) {
                const auto id=world_->building_owner_at({x,y});
                const char prefix=object==simulation::Object::Household ? 'H':
                    object==simulation::Object::ClaySource ? 'C':'P';
                const std::string label=std::string(1,prefix)+
                    std::to_string(static_cast<unsigned>(*id));
                if (!SDL_SetRenderDrawColor(renderer_,255,255,255,255) ||
                    !SDL_RenderDebugText(renderer_,static_cast<float>(center.x)+7,
                        static_cast<float>(center.y)-16,label.c_str())) return false;
            }
        }
    }
    const auto draw_agent=[&](simulation::Position position,SDL_Color body,SDL_Color cargo_color,
                              int cargo,int shift)->bool {
        const auto screen=camera_.world_to_screen(world_for(position));
        const float size=static_cast<float>(std::max(5.0,10.0*camera_.zoom));
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
    if (simulation::production_profile(rules_)) {
        if (const auto a=world_->courier_position(simulation::CourierId::Clay))
            if (!draw_agent(*a,world_->courier(simulation::CourierId::Clay).route_pending ?
                SDL_Color{255,120,40,255}:SDL_Color{100,240,255,255},{25,95,255,255},
                world_->courier(simulation::CourierId::Clay).cargo,-5)) return false;
        if (const auto b=world_->courier_position(simulation::CourierId::Pottery))
            if (!draw_agent(*b,world_->courier(simulation::CourierId::Pottery).route_pending ?
                SDL_Color{255,120,40,255}:SDL_Color{255,225,130,255},{240,45,190,255},
                world_->courier(simulation::CourierId::Pottery).cargo,5)) return false;
        if (simulation::household_profile(rules_))
            if (const auto c=world_->courier_position(simulation::CourierId::Household))
                if (!draw_agent(*c,world_->courier(simulation::CourierId::Household).route_pending ?
                    SDL_Color{255,120,40,255}:SDL_Color{110,255,130,255},
                    {255,90,150,255},world_->courier(simulation::CourierId::Household).cargo,0)) return false;
        if (rules_==simulation::RulesProfile::IndustryV5)
            for (unsigned id=4;id<=5;++id) {
                const auto courier_id=static_cast<simulation::CourierId>(id);
                const auto& courier=world_->courier(courier_id);
                if (const auto position=world_->courier_position(courier_id))
                    if (!draw_agent(*position,courier.route_pending ? SDL_Color{255,120,40,255}:
                        courier.role==simulation::CourierRole::Clay ?
                            SDL_Color{50,175,255,255}:SDL_Color{225,145,255,255},
                        courier.role==simulation::CourierRole::Clay ?
                            SDL_Color{25,95,255,255}:SDL_Color{240,45,190,255},
                        courier.cargo,static_cast<int>(id==4 ? -10:10))) return false;
            }
    } else if (const auto agent=world_->courier_position()) {
        if (!draw_agent(*agent,{255,255,255,255},{255,215,20,255},
            world_->courier_cargo(),0)) return false;
    }
    if (selected_) {
        const auto top=world_for({static_cast<double>(selected_->x),static_cast<double>(selected_->y)});
        if (!draw_diamond({top.x,top.y-20},255,255,255,false)) return false;
    }
    if (road_start_) {
        for (const auto cell:road_preview_.cells) {
            const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
            const bool okay=world_->object_at(cell)==simulation::Object::Road ||
                world_->validate({simulation::CommandType::PlaceRoad,cell}).accepted;
            if (!draw_diamond({top.x,top.y-20},okay ? 45:245,okay ? 230:60,
                              okay ? 110:65,true)) return false;
        }
        return true;
    }
    if (hovered_ && tool_!=(simulation::production_profile(rules_) ? 5 : 4)) {
        const auto top=world_for({static_cast<double>(hovered_->x),static_cast<double>(hovered_->y)});
        const auto result=preview(*hovered_);
        if (!draw_diamond({top.x,top.y-20},result.accepted?70:255,
            result.accepted?245:65,result.accepted?100:65,true)) return false;
    }
    return true;
}
std::vector<simulation::BuildingId> SandboxView::placed_buildings() const {
    std::vector<simulation::BuildingId> result;
    for (unsigned id=1;id<=9;++id) {
        const auto key=static_cast<simulation::BuildingId>(id);
        if (world_->building(key).placed) result.push_back(key);
    }
    return result;
}
std::optional<simulation::BuildingId> SandboxView::selected_building() const {
    return selected_ ? world_->building_owner_at(*selected_) : std::nullopt;
}
std::vector<std::string> SandboxView::inspection_lines() const {
    std::vector<std::string> lines;
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
        lines.push_back(b.last_demand_status==0 ? "Demand: not due" :
            b.last_demand_status==1 ? "Demand: supplied" : "Demand: unmet");
    }
    if (simulation::production_profile(rules_)) {
        for (unsigned courier_id=1;courier_id<=5;++courier_id) {
            const auto& c=world_->courier(static_cast<simulation::CourierId>(courier_id));
            if (!c.enabled || c.owner!=*id) continue;
            lines.push_back("Courier #"+std::to_string(courier_id)+" "+
                (c.role==simulation::CourierRole::Clay ? "Clay" :
                 c.role==simulation::CourierRole::Pottery ? "Pottery" : "House supply"));
            lines.push_back("Target "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
                std::string("-"):std::to_string(static_cast<unsigned>(c.target)))+
                " cargo "+std::to_string(c.cargo));
            lines.push_back(std::string("Phase ")+simulation::delivery_phase_name(c.phase));
            std::string status=world_->courier_blockage(c.id);
            if (status=="No eligible pottery route or space") {
                bool free_target=false,reachable=false;
                for (unsigned target_id=1;target_id<=9;++target_id) {
                    const auto& target=world_->building(static_cast<simulation::BuildingId>(target_id));
                    if (!target.placed || target.kind!=simulation::Object::Pottery ||
                        target.input_clay+target.reserved_incoming>=
                            simulation::Rules::pottery_input_capacity) continue;
                    free_target=true;
                    if (c.target_routes[target_id-1]) reachable=true;
                }
                status=!free_target ? "Target buffer full":
                    !reachable ? "No road connection":"No eligible Pottery";
            } else if (status=="No eligible household route or space") {
                bool free_target=false,reachable=false;
                for (unsigned target_id=4;target_id<8;++target_id) {
                    const auto& target=world_->building(static_cast<simulation::BuildingId>(target_id));
                    if (!target.placed || target.kind!=simulation::Object::Household ||
                        target.pottery_stock+target.reserved_incoming>=
                            simulation::Rules::household_capacity) continue;
                    free_target=true;
                    if (world_->household_route_available(target.id)) reachable=true;
                }
                status=!free_target ? "Target buffer full":
                    !reachable ? "No road connection":"No eligible household";
            }
            lines.push_back("Status "+status);
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
    const int max_chars=max_width/(8*layout_.scale);
    if (max_chars<=0) return true;
    std::string text=value;
    if (static_cast<int>(text.size())>max_chars) {
        text.resize(static_cast<std::size_t>(max_chars));
        if (max_chars>=3) text.replace(text.size()-3,3,"...");
    }
    const float scale=static_cast<float>(layout_.scale);
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
    const std::string profile=simulation::rules_profile_name(rules_);
    std::string overview=profile+" | Tick "+std::to_string(world_->ticks())+
        (clock_.paused()?" PAUSED ":" RUNNING ")+std::to_string(clock_.speed())+"x";
    if (simulation::production_profile(rules_)) overview+=" | Clay "+
        std::to_string(world_->clay_extracted_total())+" Pottery "+
        std::to_string(world_->pottery_completed_total())+" Store "+
        std::to_string(world_->building(simulation::BuildingId::Warehouse).pottery_stock);
    else overview+=" | Goods "+std::to_string(world_->total_produced());
    if (!draw_text(8*layout_.scale,18*layout_.scale,overview,
        layout_.top.w-200*layout_.scale)) return false;
    if (managed_ && !draw_text(layout_.top.w-82*layout_.scale,18*layout_.scale,
                               "[ MENU ]",80*layout_.scale)) return false;
    const std::string status=road_start_ && !road_preview_.valid ? road_preview_.reason :
        last_message_.empty() ? "OpenEmperor sandbox | Select a tool, then click the map" :
        last_message_;
    if (!draw_text(8*layout_.scale,layout_.status.y+7*layout_.scale,status,
                   layout_.status.w-16*layout_.scale)) return false;
    const auto label=[&](A action)->std::string {
        switch (action) {
        case A::Select: return simulation::production_profile(rules_) ? "5 Select":"4 Select";
        case A::Road: return "1 Road";
        case A::Clay: return simulation::production_profile(rules_) ? "2 Clay" : "2 Workshop";
        case A::Pottery: return "3 Pottery";
        case A::Warehouse: return simulation::production_profile(rules_) ? "4 Store" : "3 Store";
        case A::RemoveRoad: return "6 Remove";
        case A::Household: return "7 House";
        case A::Pause: return clock_.paused()?"Continue":"Pause";
        case A::Step: return "Step";
        case A::Speed1: return "1x";
        case A::Speed2: return "2x";
        case A::Speed4: return "4x";
        case A::Reset: return "Reset";
        case A::Save: return "Save F5";
        case A::Load: return "Load F9";
        case A::TogglePanel: return layout_.panel_open ? "Hide info":"Show info";
        case A::ToggleDebug: return debug_open_ ? "Hide debug":"Debug";
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
        default: break;
        }
        const bool enabled=action_enabled(button.action);
        if (!fill(button.rect,!enabled ? SDL_Color{43,47,55,255}:
            active ? SDL_Color{38,100,125,255}:SDL_Color{38,58,77,255})) return false;
        if (!SDL_SetRenderDrawColor(renderer_,enabled ? 245:135,enabled ? 247:140,
                                    enabled ? 250:145,255)) return false;
        std::string text=label(button.action);
        if (button.action==A::Clay || button.action==A::Pottery ||
            button.action==A::Warehouse || button.action==A::Household) {
            const auto wanted=button.action==A::Clay ?
                (simulation::production_profile(rules_) ? simulation::Object::ClaySource:
                 simulation::Object::Workshop) :
                button.action==A::Pottery ? simulation::Object::Pottery :
                button.action==A::Warehouse ? simulation::Object::Warehouse:
                simulation::Object::Household;
            int count=0;
            for (const auto id:placed_buildings()) if (world_->building(id).kind==wanted) ++count;
            const int limit=button.action==A::Household ?
                (rules_==simulation::RulesProfile::IndustryV5 ||
                 rules_==simulation::RulesProfile::SettlementV4 ? 4:1) :
                rules_==simulation::RulesProfile::IndustryV5 &&
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
        const std::string debug="Road revision "+std::to_string(world_->road_revision())+
            " | Commands "+std::to_string(world_->command_sequence())+
            " | Balance "+(world_->goods_balance_valid()?"OK":"ERROR");
        if (!draw_text(8*layout_.scale,layout_.map.y+8*layout_.scale,debug,
                       layout_.map.w-16*layout_.scale)) return false;
    }
    return true;
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
        const bool map_ok=background_.render(render_camera,std::nullopt) && draw_world();
        if (!SDL_SetRenderClipRect(renderer_,nullptr) || !map_ok) return false;
    }
    const bool ui_ok=draw_hud();
    const bool reset=SDL_SetRenderClipRect(renderer_,nullptr) &&
        SDL_SetRenderViewport(renderer_,nullptr) && SDL_SetRenderScale(renderer_,1,1);
    if (!ui_ok || !reset) return false;
    return SDL_GetRenderTarget(renderer_) ? true:SDL_RenderPresent(renderer_);
}

} // namespace openemperor
