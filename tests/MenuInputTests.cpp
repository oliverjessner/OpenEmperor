#include "app/MenuSession.h"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("openemperor-menu-input-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Temp() { std::error_code error;std::filesystem::remove_all(path,error); }
};
struct Dialog final:openemperor::menu::DialogAdapter {
    int folders=0;
    void open_folder(SDL_Window*,Callback) override { ++folders; }
    void open_file(SDL_Window*,Callback) override {}
    void cancel_pending() override {}
};
using Menu=openemperor::menu::MenuSession;
SDL_Event button(SDL_Window* window,Uint32 type,float x=90,float y=220) {
    SDL_Event event{};event.type=type;event.button.windowID=SDL_GetWindowID(window);
    event.button.button=SDL_BUTTON_LEFT;event.button.x=x;event.button.y=y;return event;
}
void route(Menu& menu,const SDL_Event& event) {
    auto pushed=event;check(SDL_PushEvent(&pushed),"queue menu input");
    SDL_Event queued{};while (SDL_PollEvent(&queued)) menu.handle_event(queued);
}
void drain(Menu& menu) { SDL_Event event{};while (SDL_PollEvent(&event)) menu.handle_event(event); }
} // namespace

int main() {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"dummy SDL init");
        SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("menu input",1100,700,SDL_WINDOW_RESIZABLE,&window,&renderer),"window");
        Temp temp;
        const std::array<Uint32,8> invalidations{SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_WINDOW_MOUSE_LEAVE,
            SDL_EVENT_WINDOW_HIDDEN,SDL_EVENT_WINDOW_MINIMIZED,SDL_EVENT_WINDOW_RESIZED,
            SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,SDL_EVENT_WINDOW_DISPLAY_CHANGED,SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED};
        for (const auto type:invalidations) {
            auto dialog=std::make_unique<Dialog>();auto* calls=dialog.get();
            Menu menu({},temp.path/std::to_string(type),std::move(dialog));menu.initialize(window,renderer);
            check(menu.render(),"render menu");drain(menu);
            // Authored menu button is logical [40,270) x [135,167), rendered at 1.5x.
            const auto hit=menu.diagnostic_button_hit(90,220);
            const auto p=menu.diagnostic_render_point(90,220);
            check(hit && hit->enabled && p && std::abs(p->x-90)<0.001f &&
                std::abs(hit->logical_point.x-60)<0.001f,"normal menu conversion and button hit");
            check(menu.diagnostic_render_point(10,10) && !menu.diagnostic_button_hit(10,10),
                "diagnostics retain converted misses");
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_DOWN));
            check(menu.diagnostic_pressed_action()==hit->action,"down uses actual button point without motion");
            SDL_Event invalidate{};invalidate.type=type;invalidate.window.windowID=SDL_GetWindowID(window);
            route(menu,invalidate);
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_UP));menu.advance();
            check(calls->folders==0 && menu.diagnostic_pressed_action()==-1 &&
                menu.diagnostic_pending_action()==-1,"window invalidation cancels armed menu action");
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_DOWN));
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_UP));
            check(menu.diagnostic_pending_action()==hit->action,"diagnostics expose queued menu action");
            menu.advance();check(calls->folders==1,"valid routed menu click still opens chooser");menu.shutdown();
        }
        {
            auto dialog=std::make_unique<Dialog>();auto* calls=dialog.get();
            Menu menu({},temp.path/"resize",std::move(dialog));menu.initialize(window,renderer);menu.render();drain(menu);
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_DOWN));
            // This deliberately small fixture tests the public API boundary, below App's native minimum.
            check(SDL_SetWindowSize(window,200,100),"actually resize SDL window");SDL_PumpEvents();drain(menu);
            int width=0,height=0,output_width=0,output_height=0;
            check(SDL_GetWindowSize(window,&width,&height) && SDL_GetCurrentRenderOutputSize(renderer,&output_width,&output_height)
                && width==200 && height==100 && output_width==200 && output_height==100,"actual window and output resized");
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_UP));menu.advance();
            check(calls->folders==0,"actual resize and outside release do not activate old menu button");
            const std::array<SDL_FPoint,6> invalid{{{90,220},{-1,220},{200,80},{90,100},
                {std::numeric_limits<float>::infinity(),80},{std::numeric_limits<float>::quiet_NaN(),80}}};
            for (const auto p:invalid) {
                check(!menu.diagnostic_render_point(p.x,p.y),"outside or nonfinite window point rejected");
                route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_DOWN,p.x,p.y));
                route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_UP,p.x,p.y));menu.advance();
                check(calls->folders==0,"invalid pointer cannot invoke menu action");
            }
            check(SDL_SetWindowSize(window,1100,700),"restore actual window size");SDL_PumpEvents();drain(menu);
            check(menu.render(),"render restored menu");
            route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_DOWN));route(menu,button(window,SDL_EVENT_MOUSE_BUTTON_UP));menu.advance();
            check(calls->folders==1,"menu input recovers after actual resize");menu.shutdown();
        }
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"menu input routing checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';SDL_Quit();return 1; }
}
