#include "app/SandboxView.h"

#include "maps/SandboxPlacement.h"
#include "renderer/StoredCamera.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
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
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&camera_.viewport_width,&camera_.viewport_height))
        throw std::runtime_error(SDL_GetError());
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
    background_.shutdown();
    world_.reset();
    window_=nullptr;
    renderer_=nullptr;
}
void SandboxView::reset_camera() {
    fit_stored_camera(background_.plan(),camera_);
    if (demo_origin_) {
        const auto center=maps::terrain_world({static_cast<std::uint32_t>(demo_origin_->x+
            (rules_==simulation::RulesProfile::IndustryV5 ? 6 :
             rules_==simulation::RulesProfile::SettlementV4 ? 6 :
             rules_==simulation::RulesProfile::HouseholdV3 ? 5 :
             rules_==simulation::RulesProfile::ProductionV2 ? 3 : 2)),
            static_cast<std::uint32_t>(demo_origin_->y)},geometry_.border);
        camera_.zoom=std::clamp(camera_.zoom,1.0,4.0);
        camera_.center_on({center.x,center.y+20.0});
        camera_.offset.y+=75.0;
    }
}
void SandboxView::resize_camera() {
    int width=0,height=0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&width,&height))
        throw std::runtime_error(SDL_GetError());
    if (width==camera_.viewport_width && height==camera_.viewport_height) return;
    const auto center=camera_.screen_to_world({camera_.viewport_width*0.5,camera_.viewport_height*0.5});
    camera_.viewport_width=width;
    camera_.viewport_height=height;
    camera_.center_on(center);
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
int SandboxView::hud_height() const {
    return rules_==simulation::RulesProfile::IndustryV5 ? 405 :
        rules_==simulation::RulesProfile::SettlementV4 ? 310 :
        rules_==simulation::RulesProfile::HouseholdV3 ? 205 :
        rules_==simulation::RulesProfile::ProductionV2 ? 151 : 116;
}
std::optional<simulation::Cell> SandboxView::pick(scene::Point screen) const {
    if (screen.y<hud_height()) return std::nullopt;
    const auto cell=maps::pick_terrain_cell(camera_.screen_to_world(screen),geometry_);
    if (!cell) return std::nullopt;
    return simulation::Cell{static_cast<int>(cell->x),static_cast<int>(cell->y)};
}
simulation::CommandResult SandboxView::preview(simulation::Cell cell) const {
    if (tool_==(simulation::production_profile(rules_) ? 5 : 4))
        return {false,false,"Select tool",world_->command_sequence()+1,world_->ticks()};
    return world_->validate({command_type(rules_,tool_),cell});
}
void SandboxView::set_tool(int tool) {
    if (tool>=1 && tool<=(simulation::household_profile(rules_) ? 7 :
                          rules_==simulation::RulesProfile::ProductionV2 ? 6 : 4)) tool_=tool;
}
void SandboxView::handle_event(const SDL_Event& event,bool& running) {
    if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
        (event.type==SDL_EVENT_KEY_DOWN && event.key.key==SDLK_ESCAPE)) { running=false; return; }
    if (event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) resize_camera();
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key>=SDLK_1 && event.key.key<=
            (simulation::household_profile(rules_) ? SDLK_7 :
             rules_==simulation::RulesProfile::ProductionV2 ? SDLK_6 : SDLK_4))
            set_tool(static_cast<int>(event.key.key-SDLK_1)+1);
        else if (event.key.key==SDLK_SPACE) clock_.toggle_pause();
        else if (event.key.key==SDLK_PERIOD) clock_.step_once(*world_);
        else if (event.key.key==SDLK_PLUS || event.key.key==SDLK_EQUALS || event.key.key==SDLK_KP_PLUS)
            clock_.set_speed(clock_.speed()==1 ? 2 : 4);
        else if (event.key.key==SDLK_MINUS || event.key.key==SDLK_KP_MINUS)
            clock_.set_speed(clock_.speed()==4 ? 2 : 1);
        else if (event.key.key==SDLK_R) reset_camera();
        else if (event.key.key==SDLK_F5 || event.key.key==SDLK_F9) {
            ++io_generation_;
            try {
                if (event.key.key==SDLK_F5) save_now(); else load_now();
            } catch (const std::exception& error) { last_message_=error.what(); }
        }
    }
    if (event.type==SDL_EVENT_MOUSE_WHEEL) {
        float x=event.wheel.mouse_x,y=event.wheel.mouse_y;
        if (SDL_RenderCoordinatesFromWindow(renderer_,x,y,&x,&y)) {
            camera_.zoom_at({x,y},std::pow(1.15,event.wheel.y));
            camera_.zoom=std::clamp(camera_.zoom,0.05,8.0);
        }
    }
    if (event.type==SDL_EVENT_MOUSE_MOTION) {
        float x=event.motion.x,y=event.motion.y;
        hovered_=SDL_RenderCoordinatesFromWindow(renderer_,x,y,&x,&y) ? pick({x,y}) : std::nullopt;
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button==SDL_BUTTON_RIGHT) {
            tool_=simulation::production_profile(rules_) ? 5 : 4; return;
        }
        if (event.button.button!=SDL_BUTTON_LEFT) return;
        float x=event.button.x,y=event.button.y;
        if (!SDL_RenderCoordinatesFromWindow(renderer_,x,y,&x,&y)) return;
        const auto cell=pick({x,y});
        if (!cell) return;
        hovered_=cell;
        if (tool_==(simulation::production_profile(rules_) ? 5 : 4)) {
            selected_=cell; return;
        }
        (void)execute({command_type(rules_,tool_),*cell});
    }
}
void SandboxView::update(double seconds) {
    const bool* keys=SDL_GetKeyboardState(nullptr);
    const double movement=400.0*std::clamp(seconds,0.0,0.05);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) camera_.offset.x+=movement;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) camera_.offset.x-=movement;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) camera_.offset.y+=movement;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) camera_.offset.y-=movement;
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
        if (center.x<-90 || center.y<-90 || center.x>camera_.viewport_width+90 ||
            center.y>camera_.viewport_height+90) continue;
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
    if (hovered_ && tool_!=(simulation::production_profile(rules_) ? 5 : 4)) {
        const auto top=world_for({static_cast<double>(hovered_->x),static_cast<double>(hovered_->y)});
        const auto result=preview(*hovered_);
        if (!draw_diamond({top.x,top.y-20},result.accepted?70:255,
            result.accepted?245:65,result.accepted?100:65,true)) return false;
    }
    return true;
}
bool SandboxView::draw_hud() {
    if (simulation::production_profile(rules_)) return draw_hud_v2();
    const SDL_FRect panel{0,0,static_cast<float>(camera_.viewport_width),
        static_cast<float>(hud_height())};
    if (!SDL_SetRenderDrawColor(renderer_,11,16,24,245) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,245,245,245,255)) return false;
    const std::string line0="LOGISTICS SANDBOX - prototype rules | 1 Road 2 Workshop 3 Warehouse 4 Select";
    const std::string line1="Tool: "+std::string{tool_name(rules_,tool_)}+" | Tick: "+std::to_string(world_->ticks())+
        " ("+std::to_string(world_->ticks()/simulation::Rules::ticks_per_second)+"s) | "+
        (clock_.paused()?"PAUSED":"RUNNING")+" "+std::to_string(clock_.speed())+"x | Goods produced: "+
        std::to_string(world_->total_produced());
    const std::string line2="Workshop: "+std::to_string(world_->workshop_stock())+"/"+
        std::to_string(simulation::Rules::workshop_capacity)+" progress "+
        std::to_string(world_->production_progress())+"/100 | Courier: "+
        simulation::courier_phase_name(world_->courier_phase())+" cargo "+
        std::to_string(world_->courier_cargo())+"/4 | Warehouse: "+
        std::to_string(world_->warehouse_stock())+"/32";
    const std::string line3="Status: "+std::string{world_->blockage()}+
        " | Last: "+(last_message_.empty()?std::string{"-"}:last_message_)+
        " | Road revision: "+std::to_string(world_->road_revision());
    std::string line4="Space Pause  . Step  +/- Speed  WASD/Arrows Pan  Wheel Zoom  R Reset  Esc Exit";
    if (hovered_ && tool_!=4) {
        const auto p=preview(*hovered_);
        line4+=" | Preview ("+std::to_string(hovered_->x)+","+std::to_string(hovered_->y)+"): "+
            (p.accepted?"VALID ":"INVALID ")+p.reason;
    }
    const std::string line5=selected_ ?
        "Selected: ("+std::to_string(selected_->x)+","+std::to_string(selected_->y)+") "+
        object_name(world_->object_at(*selected_))+" | Buildable: "+
        (world_->buildable(*selected_)?"yes":"no") : "Selected: none";
    return SDL_RenderDebugText(renderer_,8,5,line0.c_str()) &&
        SDL_RenderDebugText(renderer_,8,23,line1.c_str()) &&
        SDL_RenderDebugText(renderer_,8,41,line2.c_str()) &&
        SDL_RenderDebugText(renderer_,8,59,line3.c_str()) &&
        SDL_RenderDebugText(renderer_,8,77,line4.c_str()) &&
        SDL_RenderDebugText(renderer_,8,95,line5.c_str());
}
bool SandboxView::draw_hud_v2() {
    const SDL_FRect panel{0,0,static_cast<float>(camera_.viewport_width),
        static_cast<float>(hud_height())};
    if (!SDL_SetRenderDrawColor(renderer_,11,16,24,245) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,245,245,245,255)) return false;
    const auto& clay=world_->building(simulation::BuildingId::ClaySource);
    const auto& pottery=world_->building(simulation::BuildingId::Pottery);
    const auto& store=world_->building(simulation::BuildingId::Warehouse);
    const auto& a=world_->courier(simulation::CourierId::Clay);
    const auto& b=world_->courier(simulation::CourierId::Pottery);
    const std::string line0=rules_==simulation::RulesProfile::IndustryV5 ?
        "sandbox-industry-v5 rules 1 | 1 Road 2 Clay 3 Pottery 4 Warehouse 5 Select 6 Remove 7 Household" :
        rules_==simulation::RulesProfile::SettlementV4 ?
        "sandbox-settlement-v4 rules 1 | 1 Road 2 Clay 3 Pottery 4 Warehouse 5 Select 6 Remove 7 Household" :
        rules_==simulation::RulesProfile::HouseholdV3 ?
        "sandbox-household-v3 rules 1 | 1 Road 2 Clay 3 Pottery 4 Warehouse 5 Select 6 Remove 7 Household" :
        "sandbox-production-v2 rules 2 | 1 Road 2 Clay 3 Pottery 4 Warehouse 5 Select 6 Remove";
    const std::string line1="Tool: "+std::string{tool_name(rules_,tool_)}+
        " | Tick: "+std::to_string(world_->ticks())+" | "+
        (clock_.paused()?"PAUSED":"RUNNING")+" "+std::to_string(clock_.speed())+
        "x | Clay extracted: "+std::to_string(world_->clay_extracted_total())+
        " | Pottery completed: "+std::to_string(world_->pottery_completed_total());
    const std::string line2="Clay source: "+std::to_string(clay.output)+"/8 progress "+
        std::to_string(clay.progress)+"/100 | A: "+simulation::delivery_phase_name(a.phase)+
        " Clay cargo "+std::to_string(a.cargo)+" reserved "+std::to_string(a.reserved)+
        " reroutes "+std::to_string(a.reroute_attempts)+" | "+
        world_->courier_blockage(simulation::CourierId::Clay);
    const std::string line3="Pottery: Clay in "+std::to_string(pottery.input_clay)+"/8 reserved "+
        std::to_string(pottery.reserved_incoming)+" recipe Clay "+
        std::to_string(pottery.active_recipe_clay)+" progress "+
        std::to_string(pottery.progress)+"/150 | "+world_->pottery_blockage();
    const std::string line4="Pottery output: "+std::to_string(pottery.output)+"/8 | B: "+
        simulation::delivery_phase_name(b.phase)+" Pottery cargo "+std::to_string(b.cargo)+
        " reserved "+std::to_string(b.reserved)+" reroutes "+std::to_string(b.reroute_attempts)+
        " | "+world_->courier_blockage(simulation::CourierId::Pottery);
    const std::string line5="Warehouse: Pottery "+std::to_string(store.pottery_stock)+
        "/32 reserved "+std::to_string(store.reserved_incoming)+
        " free "+std::to_string(simulation::Rules::warehouse_capacity-store.pottery_stock-
            store.reserved_incoming)+" | Balance: "+
        (world_->production_balance_valid() && world_->navigation_valid()?"OK":"ERROR")+
        " | Road revision "+std::to_string(world_->road_revision());
    std::string line6="Last: "+(last_message_.empty()?std::string{"-"}:last_message_);
    if (hovered_ && tool_!=5) {
        const auto p=preview(*hovered_);
        line6+=" | Preview ("+std::to_string(hovered_->x)+","+std::to_string(hovered_->y)+"): "+
            (p.accepted?"VALID ":"INVALID ")+p.reason;
    }
    const std::string line7=selected_ ?
        "Selected ("+std::to_string(selected_->x)+","+std::to_string(selected_->y)+"): "+
        object_name(world_->object_at(*selected_))+" | Space Pause . Step +/- Speed Wheel Zoom R Reset Esc Exit" :
        "Selected: none | Space Pause . Step +/- Speed Wheel Zoom R Reset Esc Exit";
    bool drawn=SDL_RenderDebugText(renderer_,8,5,line0.c_str()) &&
        SDL_RenderDebugText(renderer_,8,23,line1.c_str()) &&
        SDL_RenderDebugText(renderer_,8,41,line2.c_str()) &&
        SDL_RenderDebugText(renderer_,8,59,line3.c_str()) &&
        SDL_RenderDebugText(renderer_,8,77,line4.c_str()) &&
        SDL_RenderDebugText(renderer_,8,95,line5.c_str()) &&
        SDL_RenderDebugText(renderer_,8,113,line6.c_str()) &&
        SDL_RenderDebugText(renderer_,8,131,line7.c_str());
    if (!drawn) return false;
    if (rules_==simulation::RulesProfile::IndustryV5) {
        const auto& supplier=world_->courier(simulation::CourierId::Household);
        const auto courier_for=[&](simulation::BuildingId owner)->const simulation::CourierState* {
            for (unsigned id=1;id<=5;++id) {
                const auto& c=world_->courier(static_cast<simulation::CourierId>(id));
                if (c.enabled && c.owner==owner) return &c;
            }
            return nullptr;
        };
        const auto id_text=[](simulation::BuildingId id) {
            return std::to_string(static_cast<unsigned>(id));
        };
        const std::string supplier_line="House supplier: "+
            std::string{simulation::delivery_phase_name(supplier.phase)}+
            " target "+(supplier.phase==simulation::CourierPhase::IdleAtWorkshop ?
                "-":id_text(supplier.target))+" cargo "+std::to_string(supplier.cargo)+
            " reserved "+std::to_string(supplier.reserved)+
            " last H"+(world_->last_dispatched_household() ?
                id_text(*world_->last_dispatched_household()):"-");
        drawn=SDL_RenderDebugText(renderer_,8,149,supplier_line.c_str());
        unsigned clay_count=0,pot_count=0;
        for (unsigned id=1;id<=9;++id) {
            const auto& instance=world_->building(static_cast<simulation::BuildingId>(id));
            if (instance.placed && instance.kind==simulation::Object::ClaySource) ++clay_count;
            if (instance.placed && instance.kind==simulation::Object::Pottery) ++pot_count;
        }
        const std::string overview="Industry: "+std::to_string(clay_count)+"/2 Clay, "+
            std::to_string(pot_count)+"/2 Pottery | total Clay "+
            std::to_string(world_->clay_extracted_total())+" Pottery "+
            std::to_string(world_->pottery_completed_total())+" | Warehouse "+
            std::to_string(store.pottery_stock)+" reserved "+
            std::to_string(store.reserved_incoming);
        drawn=drawn && SDL_RenderDebugText(renderer_,8,167,overview.c_str());
        int row=0;
        for (unsigned id=1;id<=9;++id) {
            const auto& instance=world_->building(static_cast<simulation::BuildingId>(id));
            if (!instance.placed || (instance.kind!=simulation::Object::ClaySource &&
                              instance.kind!=simulation::Object::Pottery)) continue;
            const auto* c=courier_for(instance.id);
            const bool clay_source=instance.kind==simulation::Object::ClaySource;
            const std::string line=std::string(clay_source ? "C":"P")+id_text(instance.id)+
                " ("+std::to_string(instance.cell.x)+","+std::to_string(instance.cell.y)+")"+
                (clay_source ? " out "+std::to_string(instance.output)+"/8 progress "+
                    std::to_string(instance.progress)+"/100 extracted "+
                    std::to_string(instance.clay_extracted)+
                    (instance.output==simulation::Rules::clay_output_capacity ?
                        " OUTPUT FULL":"") :
                    " in "+std::to_string(instance.input_clay)+"/8 reserved "+
                    std::to_string(instance.reserved_incoming)+" out "+std::to_string(instance.output)+
                    "/8 recipe "+std::to_string(instance.progress)+"/150 completed "+
                    std::to_string(instance.recipes_completed)+" "+
                    (instance.active_recipe_clay>0 ? "Processing":
                     instance.output>=simulation::Rules::pottery_output_capacity ? "OUTPUT FULL":
                     instance.input_clay<simulation::Rules::pottery_recipe_clay ? "No Clay input":
                     "Ready"))+
                (c ? " | courier "+std::to_string(static_cast<unsigned>(c->id))+
                    " target "+id_text(c->target)+" "+
                    simulation::delivery_phase_name(c->phase)+" cargo "+
                    std::to_string(c->cargo)+" "+world_->courier_blockage(c->id):"");
            drawn=drawn && SDL_RenderDebugText(renderer_,8,static_cast<float>(185+row*18),line.c_str());
            ++row;
        }
        for (;row<4;++row) {
            const std::string line="Production slot available";
            drawn=drawn && SDL_RenderDebugText(renderer_,8,static_cast<float>(185+row*18),line.c_str());
        }
        for (unsigned id=4;id<8;++id) {
            const auto& h=world_->building(static_cast<simulation::BuildingId>(id));
            const std::string line=h.placed ?
                "H"+std::to_string(id)+" stock "+std::to_string(h.pottery_stock)+
                    "/8 reserved "+std::to_string(h.reserved_incoming)+
                    " demand "+std::to_string(h.demand_progress)+"/400 fulfilled "+
                    std::to_string(h.fulfilled_demand)+" missed "+
                    std::to_string(h.missed_demand)+" consumed "+
                    std::to_string(h.consumed_total) :
                "H"+std::to_string(id)+": not placed";
            drawn=drawn && SDL_RenderDebugText(renderer_,8,
                static_cast<float>(257+static_cast<int>(id-4)*18),line.c_str());
        }
        std::string selection="Selected: none";
        if (selected_) {
            if (const auto owner=world_->building_owner_at(*selected_)) {
                const auto& instance=world_->building(*owner);
                selection="Selected ID "+id_text(*owner)+" "+object_name(instance.kind)+
                    " ("+std::to_string(instance.cell.x)+","+std::to_string(instance.cell.y)+")";
                if (instance.kind==simulation::Object::ClaySource)
                    selection+=" out "+std::to_string(instance.output)+" progress "+
                        std::to_string(instance.progress)+" extracted "+
                        std::to_string(instance.clay_extracted);
                else if (instance.kind==simulation::Object::Pottery)
                    selection+=" input "+std::to_string(instance.input_clay)+" reserved "+
                        std::to_string(instance.reserved_incoming)+" out "+
                        std::to_string(instance.output)+" progress "+
                        std::to_string(instance.progress)+" recipes "+
                        std::to_string(instance.recipes_completed);
                else if (instance.kind==simulation::Object::Warehouse ||
                         instance.kind==simulation::Object::Household)
                    selection+=" Pottery "+std::to_string(instance.pottery_stock)+
                        " reserved "+std::to_string(instance.reserved_incoming);
                if (const auto* c=courier_for(*owner))
                    selection+=" | courier "+std::to_string(static_cast<unsigned>(c->id))+
                        " target "+id_text(c->target)+" "+world_->courier_blockage(c->id);
            }
        }
        return drawn && SDL_RenderDebugText(renderer_,8,329,selection.c_str());
    }
    if (rules_==simulation::RulesProfile::SettlementV4) {
        const auto& supplier=world_->courier(simulation::CourierId::Household);
        const auto id_text=[](std::optional<simulation::BuildingId> id) {
            return id ? "H"+std::to_string(static_cast<unsigned>(*id)) : std::string{"-"};
        };
        const std::string target=supplier.phase==simulation::CourierPhase::IdleAtWorkshop ?
            "-" : id_text(supplier.target);
        const std::string supply="Supplier: "+std::string{simulation::delivery_phase_name(supplier.phase)}+
            " target "+target+" cargo "+std::to_string(supplier.cargo)+
            " reserved "+std::to_string(supplier.reserved)+
            (supplier.route_pending ? " WAITING":"")+
            " | last dispatched "+id_text(world_->last_dispatched_household());
        drawn=SDL_RenderDebugText(renderer_,8,149,supply.c_str());
        const std::string status="Supply: "+std::string{world_->courier_blockage(simulation::CourierId::Household)}+
            " | reroutes "+std::to_string(supplier.reroute_attempts)+
            " | Houses "+std::to_string(static_cast<unsigned>(world_->next_household_id()-4))+"/4";
        drawn=drawn && SDL_RenderDebugText(renderer_,8,167,status.c_str());
        for (unsigned id=4;id<8;++id) {
            const auto& home=world_->building(static_cast<simulation::BuildingId>(id));
            const std::string line=home.placed ?
                "H"+std::to_string(id)+" ("+std::to_string(home.cell.x)+","+
                    std::to_string(home.cell.y)+") stock "+std::to_string(home.pottery_stock)+
                    "/8 reserved "+std::to_string(home.reserved_incoming)+
                    " demand "+std::to_string(home.demand_progress)+"/400 fulfilled "+
                    std::to_string(home.fulfilled_demand)+" missed "+
                    std::to_string(home.missed_demand)+" consumed "+
                    std::to_string(home.consumed_total) :
                "H"+std::to_string(id)+": not placed";
            drawn=drawn && SDL_RenderDebugText(renderer_,8,
                static_cast<float>(185+static_cast<int>(id-4)*18),
                line.c_str());
        }
        std::string selected="Selected house: none";
        if (selected_) {
            const auto owner=world_->building_owner_at(*selected_);
            if (owner && static_cast<unsigned>(*owner)>=4 && static_cast<unsigned>(*owner)<8) {
                const auto& home=world_->building(*owner);
                selected="Selected H"+std::to_string(static_cast<unsigned>(*owner))+
                    " ("+std::to_string(home.cell.x)+","+std::to_string(home.cell.y)+")"+
                    " last demand "+(home.last_demand_status==0 ? "none" :
                        home.last_demand_status==1 ? "fulfilled":"missed")+
                    " | route "+(world_->household_route_available(*owner) ? "available":"unreachable");
            }
        }
        return drawn && SDL_RenderDebugText(renderer_,8,257,selected.c_str());
    }
    if (rules_!=simulation::RulesProfile::HouseholdV3) return drawn;
    const auto& home=world_->building(simulation::BuildingId::Household);
    const auto& courier=world_->courier(simulation::CourierId::Household);
    const std::string last=home.last_demand_status==0 ? "no demand yet" :
        home.last_demand_status==1 ? "fulfilled" : "unmet";
    const std::string line8="Household: "+std::to_string(home.pottery_stock)+"/8 reserved "+
        std::to_string(home.reserved_incoming)+" demand progress "+
        std::to_string(home.demand_progress)+"/400 | Last: "+last;
    const std::string line9="Demand fulfilled "+std::to_string(home.fulfilled_demand)+
        " missed "+std::to_string(home.missed_demand)+" consumed "+
        std::to_string(home.consumed_total)+" | Supplier: "+
        simulation::delivery_phase_name(courier.phase)+" cargo "+std::to_string(courier.cargo)+
        " reserved "+std::to_string(courier.reserved);
    const std::string line10="Supply: "+std::string{world_->courier_blockage(simulation::CourierId::Household)}+
        " | reroutes "+std::to_string(courier.reroute_attempts);
    return SDL_RenderDebugText(renderer_,8,149,line8.c_str()) &&
        SDL_RenderDebugText(renderer_,8,167,line9.c_str()) &&
        SDL_RenderDebugText(renderer_,8,185,line10.c_str());
}
bool SandboxView::render() {
    resize_camera();
    if (!SDL_SetRenderDrawColor(renderer_,22,26,32,255) || !SDL_RenderClear(renderer_)) return false;
    if (!background_.render(camera_,std::nullopt)) return false;
    if (!draw_world() || !draw_hud()) return false;
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
