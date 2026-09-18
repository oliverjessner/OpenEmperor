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
class MapDebugView;
class MapBrowser;
class SandboxView;
namespace menu { class MenuSession; }

class Application {
public:
    explicit Application(std::optional<assets::RgbaImage> preview = std::nullopt,
                         std::unique_ptr<AssetBrowser> browser = nullptr,
                         std::unique_ptr<SceneView> scene = nullptr,
                         std::unique_ptr<MapDebugView> map_debug = nullptr,
                         std::unique_ptr<MapBrowser> map_browser = nullptr,
                         std::unique_ptr<SandboxView> sandbox = nullptr,
                         std::unique_ptr<menu::MenuSession> menu = nullptr);
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
    std::unique_ptr<MapDebugView> map_debug_;
    std::unique_ptr<MapBrowser> map_browser_;
    std::unique_ptr<SandboxView> sandbox_;
    std::unique_ptr<menu::MenuSession> menu_;
    bool sdl_initialized_ = false;
};

} // namespace openemperor
