#pragma once

#include "assets/Sg3RgbaDecoder.h"

#include <optional>
#include <memory>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

class AssetBrowser;
class SceneView;

class Application {
public:
    explicit Application(std::optional<assets::RgbaImage> preview = std::nullopt,
                         std::unique_ptr<AssetBrowser> browser = nullptr,
                         std::unique_ptr<SceneView> scene = nullptr);
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
    std::unique_ptr<AssetBrowser> browser_;
    std::unique_ptr<SceneView> scene_;
    bool sdl_initialized_ = false;
};

} // namespace openemperor
