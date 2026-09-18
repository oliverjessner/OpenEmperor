#include "app/Application.h"
#include "app/AssetBrowser.h"
#include "app/SceneView.h"
#include "app/MapDebugView.h"
#include "app/MapBrowser.h"
#include "app/SandboxView.h"

#include "renderer/TitleScreen.h"
#include "renderer/ImagePreview.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <limits>
#include <utility>

namespace openemperor {

Application::Application(std::optional<assets::RgbaImage> preview,
                         std::unique_ptr<AssetBrowser> browser,
                         std::unique_ptr<SceneView> scene,
                         std::unique_ptr<MapDebugView> map_debug,
                         std::unique_ptr<MapBrowser> map_browser,
                         std::unique_ptr<SandboxView> sandbox)
    : preview_(std::move(preview)), browser_(std::move(browser)), scene_(std::move(scene)),
      map_debug_(std::move(map_debug)), map_browser_(std::move(map_browser)),sandbox_(std::move(sandbox)) {}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return false;
    }
    sdl_initialized_ = true;

    if (!SDL_CreateWindowAndRenderer("OpenEmperor", browser_ || scene_ || map_debug_ || map_browser_ || sandbox_ ? 1100 : 800,
                                     browser_ || scene_ || map_debug_ || map_browser_ || sandbox_ ? 700 : 450,
                                     scene_ || map_debug_ || map_browser_ || sandbox_ ? SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY : 0,
                                     &window_, &renderer_)) {
        std::cerr << "SDL window/renderer creation failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
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
    while (running) {
        const auto sandbox_io=sandbox_ ? sandbox_->io_generation() : 0;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (sandbox_) {
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
        }

        if (!running) {
            break;
        }
        if (sandbox_ && sandbox_->io_generation()!=sandbox_io) last_ticks=SDL_GetTicksNS();
        const std::uint64_t now = SDL_GetTicksNS();
        if (scene_) scene_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (map_debug_) map_debug_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (map_browser_) map_browser_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        if (sandbox_) sandbox_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
        last_ticks = now;
        if (!render()) {
            std::cerr << "SDL rendering failed: " << SDL_GetError() << '\n';
            return 1;
        }
        SDL_Delay(16);
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
    return 0;
}

bool Application::render() {
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
