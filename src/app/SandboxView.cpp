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
const char* tool_name(int tool) {
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
    }
    return "Unknown";
}
simulation::CommandType command_type(int tool) {
    switch (tool) {
    case 1: return simulation::CommandType::PlaceRoad;
    case 2: return simulation::CommandType::PlaceWorkshop;
    default: return simulation::CommandType::PlaceWarehouse;
    }
}
}

SandboxView::SandboxView(maps::StoredMapSession session,bool demo)
    : geometry_(session.map.declared_map_size),background_(std::move(session.plan)),demo_(demo) {
    if (!geometry_.supported) throw std::invalid_argument("sandbox requires supported map geometry");
}
SandboxView::~SandboxView() { shutdown(); }

void SandboxView::initialize(SDL_Window* window,SDL_Renderer* renderer) {
    window_=window;
    renderer_=renderer;
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&camera_.viewport_width,&camera_.viewport_height))
        throw std::runtime_error(SDL_GetError());
    background_.initialize(renderer_);
    world_=std::make_unique<simulation::World>(maps::stored_grid_width,maps::stored_grid_height,
        maps::make_sandbox_buildable_mask(background_.plan(),geometry_));
    reset_camera();
    if (demo_) place_demo();
    SDL_SetWindowTitle(window_,"OpenEmperor | Logistics Sandbox - prototype rules");
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
        const auto center=maps::terrain_world({static_cast<std::uint32_t>(demo_origin_->x+2),
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
    constexpr int length=6;
    for (int y=0;y<world_->height();++y) for (int x=0;x+length<=world_->width();++x) {
        bool valid=true;
        for (int i=0;i<length;++i) valid=valid && world_->buildable({x+i,y});
        if (!valid) continue;
        demo_origin_=simulation::Cell{x,y};
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
    throw std::runtime_error("no six-cell sandbox-buildable straight row for demo");
}
std::optional<simulation::Cell> SandboxView::pick(scene::Point screen) const {
    if (screen.y<hud_height_) return std::nullopt;
    const auto cell=maps::pick_terrain_cell(camera_.screen_to_world(screen),geometry_);
    if (!cell) return std::nullopt;
    return simulation::Cell{static_cast<int>(cell->x),static_cast<int>(cell->y)};
}
simulation::CommandResult SandboxView::preview(simulation::Cell cell) const {
    if (tool_==4) return {false,false,"Select tool",world_->command_sequence()+1,world_->ticks()};
    return world_->validate({command_type(tool_),cell});
}
void SandboxView::set_tool(int tool) {
    if (tool>=1 && tool<=4) tool_=tool;
}
void SandboxView::handle_event(const SDL_Event& event,bool& running) {
    if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
        (event.type==SDL_EVENT_KEY_DOWN && event.key.key==SDLK_ESCAPE)) { running=false; return; }
    if (event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) resize_camera();
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key>=SDLK_1 && event.key.key<=SDLK_4)
            set_tool(static_cast<int>(event.key.key-SDLK_1)+1);
        else if (event.key.key==SDLK_SPACE) clock_.toggle_pause();
        else if (event.key.key==SDLK_PERIOD) clock_.step_once(*world_);
        else if (event.key.key==SDLK_PLUS || event.key.key==SDLK_EQUALS || event.key.key==SDLK_KP_PLUS)
            clock_.set_speed(clock_.speed()==1 ? 2 : 4);
        else if (event.key.key==SDLK_MINUS || event.key.key==SDLK_KP_MINUS)
            clock_.set_speed(clock_.speed()==4 ? 2 : 1);
        else if (event.key.key==SDLK_R) reset_camera();
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
        if (event.button.button==SDL_BUTTON_RIGHT) { tool_=4; return; }
        if (event.button.button!=SDL_BUTTON_LEFT) return;
        float x=event.button.x,y=event.button.y;
        if (!SDL_RenderCoordinatesFromWindow(renderer_,x,y,&x,&y)) return;
        const auto cell=pick({x,y});
        if (!cell) return;
        hovered_=cell;
        if (tool_==4) { selected_=cell; return; }
        const auto result=world_->execute({command_type(tool_),*cell});
        last_message_=result.reason;
        selected_=cell;
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
            const auto color=object==simulation::Object::Workshop ?
                SDL_Color{50,210,245,255} : SDL_Color{255,105,100,255};
            if (!draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true)) return false;
            const float size=static_cast<float>(std::max(5.0,11.0*camera_.zoom));
            const SDL_FRect rect{static_cast<float>(center.x)-size/2,
                static_cast<float>(center.y)-size*1.5F,size,size};
            if (!SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) ||
                !SDL_RenderFillRect(renderer_,&rect)) return false;
        }
    }
    if (const auto agent=world_->courier_position()) {
        const auto screen=camera_.world_to_screen(world_for(*agent));
        const float size=static_cast<float>(std::max(5.0,10.0*camera_.zoom));
        const SDL_FRect marker{static_cast<float>(screen.x)-size/2,
            static_cast<float>(screen.y)-size/2,size,size};
        if (!SDL_SetRenderDrawColor(renderer_,255,255,255,255) ||
            !SDL_RenderFillRect(renderer_,&marker)) return false;
        if (world_->courier_cargo()>0) {
            const SDL_FRect cargo{marker.x+size*.5F,marker.y-size*.5F,size*.5F,size*.5F};
            if (!SDL_SetRenderDrawColor(renderer_,255,215,20,255) ||
                !SDL_RenderFillRect(renderer_,&cargo)) return false;
        }
    }
    if (selected_) {
        const auto top=world_for({static_cast<double>(selected_->x),static_cast<double>(selected_->y)});
        if (!draw_diamond({top.x,top.y-20},255,255,255,false)) return false;
    }
    if (hovered_ && tool_!=4) {
        const auto top=world_for({static_cast<double>(hovered_->x),static_cast<double>(hovered_->y)});
        const auto result=preview(*hovered_);
        if (!draw_diamond({top.x,top.y-20},result.accepted?70:255,
            result.accepted?245:65,result.accepted?100:65,true)) return false;
    }
    return true;
}
bool SandboxView::draw_hud() {
    const SDL_FRect panel{0,0,static_cast<float>(camera_.viewport_width),hud_height_};
    if (!SDL_SetRenderDrawColor(renderer_,11,16,24,245) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,245,245,245,255)) return false;
    const std::string line0="LOGISTICS SANDBOX - prototype rules | 1 Road 2 Workshop 3 Warehouse 4 Select";
    const std::string line1="Tool: "+std::string{tool_name(tool_)}+" | Tick: "+std::to_string(world_->ticks())+
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
bool SandboxView::render() {
    resize_camera();
    if (!SDL_SetRenderDrawColor(renderer_,22,26,32,255) || !SDL_RenderClear(renderer_)) return false;
    if (!background_.render(camera_,std::nullopt)) return false;
    if (!draw_world() || !draw_hud()) return false;
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
