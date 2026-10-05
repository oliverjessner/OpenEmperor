#include "app/Application.h"
#include "app/AssetBrowser.h"
#include "app/SceneView.h"
#include "app/MapDebugView.h"
#include "app/MapBrowser.h"
#include "app/SandboxView.h"
#include "app/MenuSession.h"
#include "app/FramePacing.h"
#include "app/InputDiagnostics.h"
#include "core/Version.h"
#include "core/PerformanceDiagnostics.h"

#include "renderer/TitleScreen.h"
#include "renderer/ImagePreview.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <limits>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace openemperor {

Application::Application(std::optional<assets::RgbaImage> preview,
                         std::unique_ptr<AssetBrowser> browser,
                         std::unique_ptr<SceneView> scene,
                         std::unique_ptr<MapDebugView> map_debug,
                         std::unique_ptr<MapBrowser> map_browser,
                         std::unique_ptr<SandboxView> sandbox,
                         std::unique_ptr<menu::MenuSession> menu)
    : preview_(std::move(preview)), browser_(std::move(browser)), scene_(std::move(scene)),
      map_debug_(std::move(map_debug)), map_browser_(std::move(map_browser)),sandbox_(std::move(sandbox)),
      menu_(std::move(menu)) {}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    if (const char* diagnostics=std::getenv("OPENEMPEROR_PERF_DIAGNOSTICS");
        diagnostics && std::string_view(diagnostics)=="1")
        performance::set_enabled(true);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return false;
    }
    sdl_initialized_ = true;
    if (const char* diagnostics=std::getenv("OPENEMPEROR_INPUT_DIAGNOSTICS");
        diagnostics && std::string_view(diagnostics)=="1")
        input_diagnostics_=std::make_unique<InputDiagnostics>();

    const std::string title = "OpenEmperor - " + std::string(version::display);
    if (!SDL_CreateWindowAndRenderer(title.c_str(), browser_ || scene_ || map_debug_ || map_browser_ || sandbox_ || menu_ ? 1100 : 800,
                                     browser_ || scene_ || map_debug_ || map_browser_ || sandbox_ || menu_ ? 700 : 450,
                                     scene_ || map_debug_ || map_browser_ || sandbox_ || menu_ ? SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY : 0,
                                     &window_, &renderer_)) {
        std::cerr << "SDL window/renderer creation failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    if (menu_) SDL_SetWindowMinimumSize(window_,900,600);
    if (browser_) browser_->initialize(window_, renderer_);
    if (map_browser_) map_browser_->initialize(window_,renderer_);
    if (scene_) {
        try { scene_->initialize(window_, renderer_); }
        catch (const std::exception& error) {
            std::cerr << "Scene initialization failed: " << error.what() << '\n';
            shutdown();
            return false;
        }
        std::cout << "Scene loaded " << scene_->texture_count() << " distinct assets\n";
    }
    if (map_debug_) {
        try { map_debug_->initialize(window_, renderer_); }
        catch (const std::exception& error) {
            std::cerr << "Map debug initialization failed: " << error.what() << '\n';
            shutdown();
            return false;
        }
    }
    if (sandbox_) {
        try { sandbox_->initialize(window_,renderer_); }
        catch (const std::exception& error) {
            std::cerr << "Sandbox initialization failed: " << error.what() << '\n';
            shutdown();
            return false;
        }
    }
    if (menu_) {
        try { menu_->initialize(window_,renderer_); }
        catch (const std::exception& error) {
            std::cerr << "Menu initialization failed: " << error.what() << '\n';
            shutdown(); return false;
        }
    }
    if (preview_) {
        const std::uint64_t pitch = static_cast<std::uint64_t>(preview_->width) * 4U;
        if (pitch > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            std::cerr << "Preview row pitch is too large for SDL\n";
            shutdown();
            return false;
        }
        preview_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC,
                                             preview_->width, preview_->height);
        if (preview_texture_ == nullptr ||
            !SDL_UpdateTexture(preview_texture_, nullptr, preview_->pixels.data(), static_cast<int>(pitch)) ||
            !SDL_SetTextureBlendMode(preview_texture_, SDL_BLENDMODE_BLEND) ||
            !SDL_SetTextureScaleMode(preview_texture_, SDL_SCALEMODE_NEAREST)) {
            std::cerr << "SDL preview texture creation failed: " << SDL_GetError() << '\n';
            shutdown();
            return false;
        }
        preview_->pixels.clear();
    }
    return true;
}

int Application::run() {
    bool running = true;
    std::uint64_t last_ticks = SDL_GetTicksNS();
    const auto run_started=last_ticks;
    const auto run_started_simulation_ticks=
        performance::counter(performance::Counter::SimulationTicks);
    while (running) {
        const auto frame_start=SDL_GetTicksNS();
        const auto sandbox_io=sandbox_ ? sandbox_->io_generation() : 0;
        {
            performance::ScopedTimer event_timer(performance::Timing::EventHandling);
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
            const bool observe=input_diagnostics_ && InputDiagnostics::relevant(event) &&
                InputDiagnostics::belongs_to_window(event,SDL_GetWindowID(window_));
            const auto diagnostic_state=[&] {
                InputDiagnostics::State state;
                const auto raw=InputDiagnostics::window_position(event);
                const auto* active=sandbox_.get();
                if (menu_) {
                    state.view="menu";state.menu_state=static_cast<int>(menu_->state());
                    state.requested_action=menu_->diagnostic_pending_action();
                    state.ui_pressed=menu_->diagnostic_pressed_action()>=0;
                    if (menu_->state()==menu::MenuSession::State::Playing) active=menu_->sandbox();
                    else if (raw) {
                        if (const auto point=menu_->diagnostic_render_point(static_cast<float>(raw->x),
                                                                          static_cast<float>(raw->y)))
                            state.render_position=scene::Point{point->x,point->y};
                        if (const auto hit=menu_->diagnostic_button_hit(static_cast<float>(raw->x),
                                                                      static_cast<float>(raw->y))) {
                            state.ui=true;state.ui_action=hit->action;
                        }
                    }
                }
                if (active) {
                    state.view="sandbox";
                    const auto view=active->input_diagnostic_state(raw);
                    state.render_position=view.render_position;
                    if (view.ground_cell) state.ground_cell=scene::Cell{view.ground_cell->x,view.ground_cell->y};
                    if (view.ui_action) state.ui_action=static_cast<int>(*view.ui_action);
                    state.ui=view.ui;state.map_pressed=view.map_pressed;state.ui_pressed=view.ui_pressed;
                    state.road_drag=view.road_drag;state.focused=view.input_focused;
                    if (view.selected_cell) state.selected_cell=scene::Cell{view.selected_cell->x,view.selected_cell->y};
                    state.selected_landscape=view.selected_landscape;state.selected_walker=view.selected_walker;
                    const auto camera=active->camera();state.camera_offset=camera.offset;state.zoom=camera.zoom;
                    const auto map=active->layout().map;state.map={map.x,map.y,map.w,map.h};
                    state.tool=active->tool();state.paused=active->paused();state.help=active->help_open();
                    state.budget=active->budget_warning_pending();state.demolition=active->demolition_pending();
                    state.command_sequence=active->world().command_sequence();state.tick=active->world().ticks();
                    if (const auto selected=active->selected_building())
                        state.selected_building=static_cast<std::uint32_t>(*selected);
                }
                return state;
            };
            if (observe) input_diagnostics_->begin(event,window_,renderer_,diagnostic_state());
            if (menu_) menu_->handle_event(event);
            else if (sandbox_) {
                sandbox_->handle_event(event,running);
            } else if (map_browser_) {
                map_browser_->handle_event(event,running);
            } else if (map_debug_) {
                try { map_debug_->handle_event(event, running); }
                catch (const std::exception& error) {
                    std::cerr << "Map debug event failed: " << error.what() << '\n';
                    return 1;
                }
            } else if (scene_) {
                scene_->handle_event(event, running);
            } else if (browser_) {
                browser_->handle_event(event, running);
            } else if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
            if (observe) input_diagnostics_->end(diagnostic_state());
            }
        }

        if (menu_) { menu_->advance(); running=menu_->running(); }

        if (!running) {
            break;
        }
        if (sandbox_ && sandbox_->io_generation()!=sandbox_io) last_ticks=SDL_GetTicksNS();
        const std::uint64_t now = SDL_GetTicksNS();
        if (scene_) scene_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (map_debug_) map_debug_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (map_browser_) map_browser_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (sandbox_) sandbox_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (menu_) menu_->update(static_cast<double>(now-last_ticks)/1000000000.0);
        last_ticks = now;
        if (!render()) {
            std::cerr << "SDL rendering failed: " << SDL_GetError() << '\n';
            return 1;
        }
        const auto work_end=SDL_GetTicksNS();
        const auto work_time=work_end-frame_start;
        const auto wait=frame_wait_nanoseconds(work_time);
        if (wait>0) {
            performance::ScopedTimer wait_timer(performance::Timing::FrameWait);
            SDL_DelayPrecise(wait);
        }
    }
    if (browser_) {
        std::cout << "Browser decode attempts: " << browser_->decode_attempts()
                  << ", failures: " << browser_->decode_failures()
                  << ", cache misses: " << browser_->cache_misses()
                  << ", peak cache: " << browser_->cache_peak_entries() << " textures / "
                  << browser_->cache_peak_bytes() << " bytes\n";
    }
    if (scene_) std::cout << "Scene final frame: " << scene_->last_drawn_instances()
                          << " drawn instances, " << scene_->texture_count() << " textures\n";
    performance::print_summary(std::cout);
    if (performance::enabled()) {
        const auto run_ns=SDL_GetTicksNS()-run_started;
        const auto ticks=performance::counter(performance::Counter::SimulationTicks)-
            run_started_simulation_ticks;
        const auto rate=run_ns==0 ? 0.0:static_cast<double>(ticks)*1'000'000'000.0/
            static_cast<double>(run_ns);
        std::cout<<"  observed_simulation_ticks_per_wall_second="<<rate
                 <<" (includes paused/menu time)\n";
    }
    return 0;
}

bool Application::render() {
    if (menu_) return menu_->render();
    if (sandbox_) return sandbox_->render();
    if (map_browser_) return map_browser_->render();
    if (map_debug_) {
        try { return map_debug_->render(); }
        catch (const std::exception& error) {
            std::cerr << "Map debug render failed: " << error.what() << '\n';
            return false;
        }
    }
    if (scene_) return scene_->render();
    if (browser_) return browser_->render();
    if (preview_texture_ != nullptr && preview_) {
        return render_image_preview(renderer_, preview_texture_, preview_->width, preview_->height);
    }
    return render_title_screen(renderer_);
}

void Application::shutdown() {
    if (input_diagnostics_ && sdl_initialized_) input_diagnostics_->print(std::cout);
    if (menu_) menu_->shutdown();
    if (sandbox_) sandbox_->shutdown();
    if (map_browser_) map_browser_->shutdown();
    if (map_debug_) map_debug_->shutdown();
    if (scene_) scene_->shutdown();
    if (browser_) browser_->shutdown();
    if (preview_texture_ != nullptr) {
        SDL_DestroyTexture(preview_texture_);
        preview_texture_ = nullptr;
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
}

} // namespace openemperor
