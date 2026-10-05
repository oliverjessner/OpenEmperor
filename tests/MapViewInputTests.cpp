#include "app/MapBrowser.h"
#include "app/MapDebugView.h"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>

namespace {
std::optional<SDL_Point> sample_point;
std::optional<std::array<Uint8,4>> sample_rgba;
}
bool openemperor_test_present(SDL_Renderer* renderer) {
    if (sample_point) {
        sample_rgba.reset();
        if (auto* surface=SDL_RenderReadPixels(renderer,nullptr)) {
            Uint8 r=0,g=0,b=0,a=0;
            if (SDL_ReadSurfacePixel(surface,sample_point->x,sample_point->y,&r,&g,&b,&a)) sample_rgba={{r,g,b,a}};
            SDL_DestroySurface(surface);
        }
    }
    return SDL_RenderPresent(renderer);
}

namespace {
namespace maps=openemperor::maps;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
maps::ParsedEmperorMap fixture() {
    maps::ParsedEmperorMap map;map.declared_map_size=112;
    map.terrain_raw.logical_offset=maps::terrain_logical_offset;
    map.objects_raw.logical_offset=maps::objects_logical_offset;
    map.terrain_raw.values.resize(228U*228U,1);map.objects_raw.values.resize(228U*228U,2);return map;
}
SDL_Event click(SDL_Window* window,float x,float y) {
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.windowID=SDL_GetWindowID(window);
    event.button.button=SDL_BUTTON_LEFT;event.button.x=x;event.button.y=y;return event;
}
template<class View> void route(View& view,SDL_Event event,bool& running) {
    check(SDL_PushEvent(&event),"queue map input");
    SDL_Event queued{};while (SDL_PollEvent(&queued)) view.handle_event(queued,running);
}
void render_opaque_ui(openemperor::MapDebugView& view,int x,int y) {
    sample_point=SDL_Point{x,y};sample_rgba.reset();
    const bool rendered=view.render();sample_point.reset();
    check(rendered,"render map UI");
    check(sample_rgba==std::array<Uint8,4>{12,17,23,255},"before-Present pixel is opaque map UI");
}
} // namespace

int main() {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"dummy SDL init");
        SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("map input",160,120,SDL_WINDOW_RESIZABLE,&window,&renderer),"window");
        bool running=true;
        {
            openemperor::MapDebugView view(fixture());view.initialize(window,renderer);render_opaque_ui(view,80,20);
            route(view,click(window,80,20),running);
            check(!view.selected_cell(),"opaque legend cannot select storage cell behind UI");
            route(view,click(window,80,60),running);
            check(view.selected_cell()==maps::GridCell{114,114},"valid map down uses actual position without motion");
            const auto selected=view.selected_cell();const auto zoom=view.camera().zoom;const auto offset=view.camera().offset;
            const std::array<SDL_FPoint,7> blocked{{{80,20},{160,60},{80,120},{-1,60},{80,-1},
                {std::numeric_limits<float>::infinity(),60},{std::numeric_limits<float>::quiet_NaN(),60}}};
            for (const auto p:blocked) {
                route(view,click(window,p.x,p.y),running);
                SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.windowID=SDL_GetWindowID(window);
                wheel.wheel.mouse_x=p.x;wheel.wheel.mouse_y=p.y;wheel.wheel.y=1;route(view,wheel,running);
                check(view.selected_cell()==selected && view.camera().zoom==zoom &&
                    view.camera().offset.x==offset.x && view.camera().offset.y==offset.y,
                    "UI/outside/nonfinite pointer neither selects nor zooms map");
            }
            SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.mouse_x=80;wheel.wheel.mouse_y=60;
            wheel.wheel.y=std::numeric_limits<float>::quiet_NaN();route(view,wheel,running);
            check(view.camera().zoom==zoom && view.camera().offset.x==offset.x,"nonfinite wheel amount rejected");
            wheel.wheel.y=1;route(view,wheel,running);
            check(view.camera().zoom>zoom,"ordinary map wheel still zooms");view.shutdown();
        }
        check(SDL_SetWindowSize(window,500,400),"actual window resize");SDL_PumpEvents();
        {
            // A diagnostic cell needs no asset, but exercises the actual stored-view inspector UI.
            maps::StoredGraphicsPlan plan;plan.border=58;plan.cell_by_storage.resize(228U*228U);
            plan.status_by_storage.resize(228U*228U,maps::StoredStatus::Excluded);
            maps::StoredCell cell;cell.storage={114,114};cell.world={0,2240};
            plan.cells.push_back(cell);plan.cell_by_storage[114U*228U+114U]=0;
            openemperor::MapDebugView view(fixture(),maps::RawLayer::Terrain,maps::MapViewMode::StoredGraphics,
                std::nullopt,std::move(plan));view.initialize(window,renderer);check(view.render(),"stored diagnostic render");
            route(view,click(window,250,243),running);check(view.selected_cell()==maps::GridCell{114,114},"stored ground selection");
            SDL_Event f1{};f1.type=SDL_EVENT_KEY_DOWN;f1.key.key=SDLK_F1;route(view,f1,running);
            render_opaque_ui(view,10,243);
            const auto selected=view.selected_cell();route(view,click(window,10,243),running);
            check(view.selected_cell()==selected,"open inspector blocks click through opaque panel");
            route(view,f1,running);route(view,click(window,10,243),running);
            check(view.selected_cell()==selected,"closing inspector before redraw retains submitted UI blocking");
            check(view.render(),"render closed inspector");route(view,click(window,10,243),running);
            check(view.selected_cell()==maps::GridCell{113,115},"unobstructed redraw restores independent map selection");view.shutdown();
        }
        check(SDL_SetWindowSize(window,640,400),"resize browser window");SDL_PumpEvents();
        {
            maps::MapCatalog catalog;catalog.entries.resize(3);
            openemperor::MapBrowser browser(std::move(catalog),maps::FootprintPolicy::Disabled);browser.initialize(window,renderer);
            check(browser.render(),"browser render");
            route(browser,click(window,630,145),running);check(browser.selected_index()==1,"whole visible row remains selectable");
            route(browser,click(window,20,110),running);check(browser.selected_index()==0,"first row selection");
            const std::array<SDL_FPoint,6> invalid{{{10000,130},{640,130},{-1,130},{20,400},
                {20,std::numeric_limits<float>::infinity()},{20,std::numeric_limits<float>::quiet_NaN()}}};
            for (const auto p:invalid) {
                route(browser,click(window,p.x,p.y),running);check(browser.selected_index()==0,"invalid browser position rejected");
            }
            check(SDL_SetWindowSize(window,200,100),"actual browser boundary resize");SDL_PumpEvents();
            route(browser,click(window,20,130),running);check(browser.selected_index()==0,"row outside resized window is not selectable");
            browser.shutdown();
        }
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"map view input routing checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';SDL_Quit();return 1; }
}
