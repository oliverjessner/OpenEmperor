#include "app/Application.h"
#include "app/AssetBrowser.h"
#include "app/SceneView.h"

#include "renderer/TitleScreen.h"
#include "renderer/ImagePreview.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <limits>
#include <utility>

namespace openemperor {

Application::Application(std::optional<assets::RgbaImage> preview,
                         std::unique_ptr<AssetBrowser> browser,
                         std::unique_ptr<SceneView> scene)
    : preview_(std::move(preview)), browser_(std::move(browser)), scene_(std::move(scene)) {}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return false;
    }
    sdl_initialized_ = true;

    if (!SDL_CreateWindowAndRenderer("OpenEmperor", browser_ || scene_ ? 1100 : 800,
                                     browser_ || scene_ ? 700 : 450,
                                     scene_ ? SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY : 0,
                                     &window_, &renderer_)) {
        std::cerr << "SDL window/renderer creation failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    if (browser_) browser_->initialize(window_, renderer_);
    if (scene_) {
        try { scene_->initialize(window_, renderer_); }
        catch (const std::exception& error) {
            std::cerr << "Scene initialization failed: " << error.what() << '\n';
            shutdown();
            return false;
        }
        std::cout << "Scene loaded " << scene_->texture_count() << " distinct assets\n";
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
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (scene_) {
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
        const std::uint64_t now = SDL_GetTicksNS();
        if (scene_) scene_->update(static_cast<double>(now - last_ticks) / 1000000000.0);
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
    if (scene_) return scene_->render();
    if (browser_) return browser_->render();
    if (preview_texture_ != nullptr && preview_) {
        return render_image_preview(renderer_, preview_texture_, preview_->width, preview_->height);
    }
    return render_title_screen(renderer_);
}

void Application::shutdown() {
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
