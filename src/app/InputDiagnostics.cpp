#include "app/InputDiagnostics.h"
#include <nlohmann/json.hpp>

namespace openemperor {
namespace {
using Json=nlohmann::json;
Json point(const std::optional<scene::Point>& p) {
    return p ? Json::array({p->x,p->y}) : Json(nullptr);
}
Json state_json(const InputDiagnostics::State& s) {
    return {{"view",s.view},{"menu_state",s.menu_state},{"requested_action",s.requested_action},
        {"render",point(s.render_position)},
        {"ground",s.ground_cell ? Json::array({s.ground_cell->x,s.ground_cell->y}):Json(nullptr)},
        {"selected_cell",s.selected_cell ? Json::array({s.selected_cell->x,s.selected_cell->y}):Json(nullptr)},
        {"selected_landscape",s.selected_landscape},{"selected_walker",s.selected_walker},
        {"ui",s.ui},{"ui_action",s.ui_action ? Json(*s.ui_action):Json(nullptr)},
        {"camera",{s.camera_offset.x,s.camera_offset.y}},{"zoom",s.zoom},
        {"map",{s.map.x,s.map.y,s.map.w,s.map.h}},{"tool",s.tool},
        {"command_sequence",s.command_sequence},{"tick",s.tick},
        {"selected_building",s.selected_building},{"map_pressed",s.map_pressed},
        {"ui_pressed",s.ui_pressed},{"road_drag",s.road_drag},{"focused",s.focused},
        {"paused",s.paused},{"help",s.help},{"budget",s.budget},{"demolition",s.demolition}};
}
Json actions(const InputDiagnostics::State& a,const InputDiagnostics::State& b) {
    Json value=Json::array();
    if (a.command_sequence!=b.command_sequence) value.push_back("world_command");
    if (a.menu_state!=b.menu_state) value.push_back("menu_transition");
    if (a.requested_action!=b.requested_action && b.requested_action>=0) value.push_back("menu_request");
    if (a.camera_offset.x!=b.camera_offset.x || a.camera_offset.y!=b.camera_offset.y || a.zoom!=b.zoom)
        value.push_back("camera");
    if (a.selected_building!=b.selected_building || a.selected_cell!=b.selected_cell ||
        a.selected_landscape!=b.selected_landscape || a.selected_walker!=b.selected_walker)
        value.push_back("selection");
    if (a.tool!=b.tool) value.push_back("tool");
    if (a.help!=b.help || a.budget!=b.budget || a.demolition!=b.demolition)
        value.push_back("modal");
    if (a.paused!=b.paused) value.push_back("pause");
    if (a.map_pressed!=b.map_pressed || a.ui_pressed!=b.ui_pressed || a.road_drag!=b.road_drag)
        value.push_back("gesture");
    return value;
}
}
bool InputDiagnostics::relevant(const SDL_Event& e) {
    return e.type==SDL_EVENT_MOUSE_MOTION || e.type==SDL_EVENT_MOUSE_BUTTON_DOWN ||
        e.type==SDL_EVENT_MOUSE_BUTTON_UP || e.type==SDL_EVENT_MOUSE_WHEEL ||
        e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP ||
        (e.type>=SDL_EVENT_WINDOW_FIRST && e.type<=SDL_EVENT_WINDOW_LAST) || e.type==SDL_EVENT_QUIT;
}
bool InputDiagnostics::belongs_to_window(const SDL_Event& e,SDL_WindowID id) {
    if (e.type==SDL_EVENT_QUIT) return true;
    if (e.type>=SDL_EVENT_WINDOW_FIRST && e.type<=SDL_EVENT_WINDOW_LAST) return e.window.windowID==id;
    if (e.type==SDL_EVENT_MOUSE_MOTION) return e.motion.windowID==id;
    if (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP) return e.button.windowID==id;
    if (e.type==SDL_EVENT_MOUSE_WHEEL) return e.wheel.windowID==id;
    if (e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP) return e.key.windowID==id;
    return false;
}
std::optional<scene::Point> InputDiagnostics::window_position(const SDL_Event& e) {
    if (e.type==SDL_EVENT_MOUSE_MOTION) return scene::Point{e.motion.x,e.motion.y};
    if (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP)
        return scene::Point{e.button.x,e.button.y};
    if (e.type==SDL_EVENT_MOUSE_WHEEL) return scene::Point{e.wheel.mouse_x,e.wheel.mouse_y};
    return std::nullopt;
}
void InputDiagnostics::begin(const SDL_Event& e,SDL_Window* window,SDL_Renderer* renderer,const State& state) {
    pending_={};
    auto& r=pending_;
    r.first_sequence=r.last_sequence=++sequence_;
    r.type=e.type;r.timestamp=e.common.timestamp;r.observed_ns=SDL_GetTicksNS();
    r.observed_window_id=SDL_GetWindowID(window);r.raw=window_position(e);r.before=state;
    if (e.type==SDL_EVENT_MOUSE_MOTION) {
        r.window_id=e.motion.windowID;r.relative_x=e.motion.xrel;r.relative_y=e.motion.yrel;r.motions=1;
    } else if (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP) {
        r.window_id=e.button.windowID;r.button=e.button.button;
    } else if (e.type==SDL_EVENT_MOUSE_WHEEL) {
        r.window_id=e.wheel.windowID;r.wheel_x=e.wheel.x;r.wheel_y=e.wheel.y;
    } else if (e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP) {
        r.window_id=e.key.windowID;
        // Navigation identity only; no text event or typed text is recorded.
        if (e.key.scancode==SDL_SCANCODE_LEFT || e.key.scancode==SDL_SCANCODE_RIGHT ||
            e.key.scancode==SDL_SCANCODE_UP || e.key.scancode==SDL_SCANCODE_DOWN)
            r.navigation_key=static_cast<int>(e.key.scancode);
    } else if (e.type>=SDL_EVENT_WINDOW_FIRST && e.type<=SDL_EVENT_WINDOW_LAST)
        r.window_id=e.window.windowID;
    r.dimensions_valid=SDL_GetWindowSize(window,&r.window_w,&r.window_h) &&
        SDL_GetWindowSizeInPixels(window,&r.pixel_w,&r.pixel_h) &&
        SDL_GetCurrentRenderOutputSize(renderer,&r.output_w,&r.output_h);
    r.viewport_valid=SDL_GetRenderViewport(renderer,&r.viewport);
    r.scale_valid=SDL_GetRenderScale(renderer,&r.scale_x,&r.scale_y);
    (void)SDL_GetRenderLogicalPresentation(renderer,&r.logical_w,&r.logical_h,&r.logical_mode);
    r.has_target=SDL_GetRenderTarget(renderer)!=nullptr;
    r.density=SDL_GetWindowPixelDensity(window);r.display_scale=SDL_GetWindowDisplayScale(window);
    r.runtime=SDL_GetVersion();
    if (const char* name=SDL_GetRendererName(renderer)) r.renderer=name;
    if (const char* driver=SDL_GetCurrentVideoDriver()) r.video_driver=driver;
}
void InputDiagnostics::end(const State& state) {
    pending_.after=state;
    if (size_ && pending_.type==SDL_EVENT_MOUSE_MOTION) {
        auto& previous=records_[(start_+size_-1)%capacity];
        if (previous.type==SDL_EVENT_MOUSE_MOTION && previous.window_id==pending_.window_id) {
            const auto first=previous.first_sequence;
            const auto count=previous.motions+1;
            previous=pending_;previous.first_sequence=first;previous.motions=count;
            return;
        }
    }
    if (size_==capacity) { start_=(start_+1)%capacity;--size_;++overwritten_; }
    records_[(start_+size_)%capacity]=pending_;++size_;
}
void InputDiagnostics::print(std::ostream& out) {
    if (printed_) return;
    printed_=true;
    out<<"INPUT_DIAGNOSTICS "<<Json{{"version",1},{"capacity",capacity},{"records",size_},
        {"events",sequence_},{"overwritten_records",overwritten_},
        {"header_sdl",SDL_VERSION},{"runtime_sdl",SDL_GetVersion()},
        {"runtime_revision",SDL_GetRevision()}}.dump()<<'\n';
    for (std::size_t i=0;i<size_;++i) {
        const auto& r=records_[(start_+i)%capacity];
        out<<"INPUT_EVENT "<<Json{{"sequence",r.last_sequence},{"first_sequence",r.first_sequence},
            {"timestamp_ns",r.timestamp},{"observed_ns",r.observed_ns},{"type",r.type},
            {"window_id",r.window_id},{"app_window_id",r.observed_window_id},{"raw",point(r.raw)},
            {"relative",{r.relative_x,r.relative_y}},{"wheel",{r.wheel_x,r.wheel_y}},
            {"button",r.button},{"navigation_scancode",r.navigation_key},{"motions",r.motions},
            {"window",{r.window_w,r.window_h}},{"pixels",{r.pixel_w,r.pixel_h}},
            {"output",{r.output_w,r.output_h}},{"dimensions_valid",r.dimensions_valid},
            {"viewport",{r.viewport.x,r.viewport.y,r.viewport.w,r.viewport.h}},
            {"viewport_valid",r.viewport_valid},{"scale",{r.scale_x,r.scale_y}},{"scale_valid",r.scale_valid},
            {"logical",{r.logical_w,r.logical_h,static_cast<int>(r.logical_mode)}},
            {"has_render_target",r.has_target},{"pixel_density",r.density},{"display_scale",r.display_scale},
            {"renderer",r.renderer},{"video_driver",r.video_driver},
            {"before",state_json(r.before)},{"after",state_json(r.after)},
            {"actions",actions(r.before,r.after)}}.dump()<<'\n';
    }
}
}
