#include "app/SceneView.h"
#include "renderer/SceneRenderer.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace sc = openemperor::scene;
void check(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
void u16(std::vector<std::uint8_t>& b, std::size_t at, std::uint16_t v) {
    b[at] = static_cast<std::uint8_t>(v); b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}
void u32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    u16(b, at, static_cast<std::uint16_t>(v)); u16(b, at + 2, static_cast<std::uint16_t>(v >> 16));
}
void write(const fs::path& p, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out{p, std::ios::binary};
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
void archive(const fs::path& root, const std::string& name, int width, int height,
             int type, const std::vector<std::uint8_t>& color,
             const std::vector<std::uint8_t>& alpha = {}) {
    constexpr std::size_t at = 40680;
    std::vector<std::uint8_t> bytes(at + 72, 0);
    u32(bytes, 0, static_cast<std::uint32_t>(bytes.size())); u32(bytes, 4, 214);
    u32(bytes, 12, 1); u32(bytes, 16, 1); u32(bytes, 20, 1); u32(bytes, 680 + 124, 1);
    u32(bytes, at, 0); u32(bytes, at + 4, static_cast<std::uint32_t>(color.size()));
    if (type == 30) u32(bytes, at + 8, 3200);
    u16(bytes, at + 20, static_cast<std::uint16_t>(width));
    u16(bytes, at + 22, static_cast<std::uint16_t>(height));
    u16(bytes, at + 50, static_cast<std::uint16_t>(type));
    if (!alpha.empty()) {
        u32(bytes, at + 64, static_cast<std::uint32_t>(color.size() * 2U));
        u32(bytes, at + 68, static_cast<std::uint32_t>(alpha.size()));
    }
    write(root / (name + ".sg3"), bytes);
    auto bitmap = color;
    bitmap.insert(bitmap.end(), alpha.begin(), alpha.end());
    write(root / (name + ".555"), bitmap);
}
sc::Asset asset(const fs::path& root, const std::string& name, int width, int height,
                sc::Point anchor, bool tile) {
    sc::Asset a;
    a.id = {name + ".sg3", 0};
    a.archive_path = root / (name + ".sg3");
    a.width = width; a.height = height; a.anchor = anchor; a.emperor_type30 = tile;
    return a;
}
void pixel(SDL_Surface* surface, int x, int y, std::uint8_t& r, std::uint8_t& g,
           std::uint8_t& b, std::uint8_t& a) {
    check(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a), "read surface pixel");
}
} // namespace

int main() {
    try {
        const fs::path root = fs::temp_directory_path() / "openemperor-scene-renderer-test";
        fs::remove_all(root); fs::create_directory(root);
        std::vector<std::uint8_t> red(3200);
        for (std::size_t i = 0; i < red.size(); i += 2) { red[i] = 0x1f; red[i + 1] = 0; }
        archive(root, "ground", 78, 40, 30, red);
        archive(root, "overlay", 1, 1, 256, {1, 0x00, 0x7c}, {1, 0xf0});
        archive(root, "front", 1, 1, 13, {0xe0, 0x03});
        std::vector<std::uint8_t> green(200);
        for (std::size_t i = 0; i < green.size(); i += 2) { green[i] = 0xe0; green[i + 1] = 0x03; }
        archive(root, "tall", 1, 100, 13, green);
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"), "select dummy video driver");
        check(SDL_Init(SDL_INIT_VIDEO), "SDL init");
        SDL_Surface* surface = SDL_CreateSurface(160, 120, SDL_PIXELFORMAT_RGBA32);
        check(surface != nullptr, "software surface");
        SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
        check(sdl != nullptr, "software renderer");

        sc::Scene scene;
        scene.width = 3; scene.height = 3;
        scene.assets["ground"] = asset(root, "ground", 78, 40, {39, 0}, true);
        scene.assets["duplicate"] = scene.assets["ground"];
        scene.assets["overlay"] = asset(root, "overlay", 1, 1, {0, 0}, false);
        scene.default_terrain = "ground";
        scene.terrain_overrides.push_back({{1, 1}, "duplicate"});
        scene.objects.push_back({"overlay", "overlay", {1.5, 1.5}});
        openemperor::SceneRenderer renderer{scene};
        renderer.initialize(sdl);
        check(renderer.texture_count() == 2, "texture reuse by AssetId");
        sc::Camera2D camera; camera.viewport_width = 160; camera.viewport_height = 120;
        camera.center_on(sc::project({1.5, 1.5}));
        check(renderer.render(camera, false, std::nullopt), "render scene");
        std::uint8_t r=0,g=0,b=0,a=0;
        pixel(surface, 80, 60, r,g,b,a);
        check(r > 100 && r < 140 && g == 0 && b > 120 && b < 150 && a == 255,
              "straight alpha overlay on terrain");
        pixel(surface, 40, 80, r,g,b,a);
        check(r == 255 && g == 0 && b == 0, "adjacent unscaled terrain pixel");
        renderer.shutdown();

        scene.assets["front"] = asset(root, "front", 1, 1, {0, 0}, false);
        scene.objects.push_back({"z-front", "front", {1.5, 1.5}});
        openemperor::SceneRenderer overlap_renderer{scene};
        overlap_renderer.initialize(sdl);
        check(overlap_renderer.render(camera, false, std::nullopt), "render overlapping objects");
        pixel(surface, 80, 60, r,g,b,a);
        check(r == 0 && g == 255 && b == 0, "instance ID resolves equal-depth overlap");
        overlap_renderer.shutdown();
        scene.objects.pop_back();

        sc::Scene tall_scene;
        tall_scene.width = 1; tall_scene.height = 1;
        tall_scene.assets["ground"] = scene.assets.at("ground");
        tall_scene.assets["tall"] = asset(root, "tall", 1, 100, {0, 99}, false);
        tall_scene.default_terrain = "ground";
        tall_scene.objects.push_back({"tall", "tall", {0, 0}});
        openemperor::SceneRenderer tall_renderer{tall_scene};
        tall_renderer.initialize(sdl);
        camera.offset = {40, 150}; camera.zoom = 1;
        check(tall_renderer.render(camera, false, std::nullopt), "render tall object");
        check(tall_renderer.last_drawn_instances() == 1, "tall visible with anchor outside viewport");
        pixel(surface, 40, 80, r,g,b,a);
        check(r == 0 && g == 255 && b == 0, "tall visible pixel");
        tall_renderer.shutdown();

        SDL_Window* window = nullptr;
        SDL_Renderer* window_renderer = nullptr;
        check(SDL_CreateWindowAndRenderer("scene input test", 160, 120, 0,
                                          &window, &window_renderer), "dummy window renderer");
        openemperor::SceneView view{scene};
        view.initialize(window, window_renderer);
        bool running = true;
        SDL_Event click{};
        click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x = 80;
        click.button.y = 60;
        view.handle_event(click, running);
        check(view.selected_cell() == sc::Cell{1, 1}, "mouse cell selection through SDL coordinates");
        SDL_Event wheel{};
        wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.mouse_x = 80;
        wheel.wheel.mouse_y = 60;
        wheel.wheel.y = 1;
        const auto fixed = view.camera().screen_to_world({80, 60});
        view.handle_event(wheel, running);
        const auto zoomed = view.camera().screen_to_world({80, 60});
        check(view.camera().zoom > 1 && std::abs(fixed.x - zoomed.x) < 1e-8 &&
              std::abs(fixed.y - zoomed.y) < 1e-8, "wheel zoom fixed under pointer");
        view.handle_event(click, running);
        check(view.selected_cell() == sc::Cell{1, 1}, "mouse selection after zoom");
        const auto old_center = view.camera().screen_to_world(
            {view.camera().viewport_width * 0.5, view.camera().viewport_height * 0.5});
        check(SDL_SetWindowSize(window, 200, 150), "resize dummy window");
        check(view.render(), "render after resize");
        const auto new_center = view.camera().screen_to_world(
            {view.camera().viewport_width * 0.5, view.camera().viewport_height * 0.5});
        check(std::abs(old_center.x - new_center.x) < 1e-8 &&
              std::abs(old_center.y - new_center.y) < 1e-8, "preserve camera center on resize");
        view.shutdown();
        SDL_DestroyRenderer(window_renderer);
        SDL_DestroyWindow(window);
        SDL_DestroyRenderer(sdl); SDL_DestroySurface(surface); SDL_Quit();
        fs::remove_all(root);
        std::cout << "software scene renderer checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
