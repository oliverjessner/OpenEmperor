#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <optional>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

class Application {
public:
    explicit Application(std::optional<assets::RgbaImage> preview = std::nullopt);
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    bool initialize();
    int run();
    void shutdown();

private:
    bool render();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* preview_texture_ = nullptr;
    std::optional<assets::RgbaImage> preview_;
    bool sdl_initialized_ = false;
};

} // namespace openemperor
