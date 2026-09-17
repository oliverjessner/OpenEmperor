#include "app/MapDebugView.h"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
namespace maps = openemperor::maps;
void check(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
maps::ParsedEmperorMap fixture() {
    maps::ParsedEmperorMap map;
    map.declared_map_size = 112;
    map.terrain_raw.logical_offset = maps::terrain_logical_offset;
    map.objects_raw.logical_offset = maps::objects_logical_offset;
    map.terrain_raw.values.resize(228U * 228U, 1);
    map.objects_raw.values.resize(228U * 228U, 2);
    map.terrain_raw.values[114U * 228U + 114U] = 0x12345678U;
    map.objects_raw.values[114U * 228U + 114U] = 0xaabbccddU;
    map.terrain_raw.values[113U * 228U + 115U] = 0x10203040U; // Asymmetric reference.
    return map;
}
void pixel(SDL_Renderer* renderer, int x, int y, const std::array<std::uint8_t,4>& expected) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    check(surface != nullptr, "read software render pixels");
    std::uint8_t r=0,g=0,b=0,a=0;
    const bool read = SDL_ReadSurfacePixel(surface, x, y, &r,&g,&b,&a);
    SDL_DestroySurface(surface);
    check(read && r==expected[0] && g==expected[1] && b==expected[2] && a==expected[3],
          "raw value appears at expected storage cell");
}
} // namespace

int main() {
    try {
        maps::StorageGridCamera math;
        math.viewport_width = 160; math.viewport_height = 120;
        math.center_on({114,114});
        check(math.pick({80,60}) == maps::DisplayCell{114,114}, "center selection");
        check(math.pick({83,60}) == maps::DisplayCell{115,114}, "positive boundary");
        check(!math.pick(math.grid_to_screen({-0.5,1})), "outside selection");
        const auto fixed = math.screen_to_grid({65,72});
        math.zoom_at({65,72}, 2.0);
        const auto after = math.screen_to_grid({65,72});
        check(std::abs(fixed.x-after.x)<1e-8 && std::abs(fixed.y-after.y)<1e-8,
              "pointer-centered grid zoom");
        check(maps::raw_value_color(0x12345678U) == maps::raw_value_color(0x12345678U) &&
              maps::raw_value_color(0x12345678U) != maps::raw_value_color(0xaabbccddU),
              "deterministic distinct fixture colors");
        auto map = fixture();
        const auto rgba = maps::make_storage_rgba(map, maps::RawLayer::Terrain);
        const auto expected = maps::raw_value_color(0x12345678U);
        const auto at = (114U * 228U + 114U) * 4U;
        check(rgba[at] == expected[0] && rgba[at+1] == expected[1] &&
              rgba[at+2] == expected[2], "row-major RGBA generation");

        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"), "select dummy video driver");
        check(SDL_Init(SDL_INIT_VIDEO), "SDL init");
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        check(SDL_CreateWindowAndRenderer("map test", 160, 120, 0, &window, &renderer),
              "dummy renderer");
        openemperor::MapDebugView view{std::move(map)};
        view.initialize(window, renderer);
        check(view.render(), "render terrain layer");
        pixel(renderer, 81,61, maps::raw_value_color(0x12345678U));
        bool running = true;
        SDL_Event change{}; change.type = SDL_EVENT_KEY_DOWN; change.key.key = SDLK_2;
        view.handle_event(change, running);
        check(view.layer() == maps::RawLayer::Objects && view.render(), "layer switch");
        pixel(renderer, 81,61, maps::raw_value_color(0xaabbccddU));
        SDL_Event click{}; click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        click.button.button = SDL_BUTTON_LEFT; click.button.x = 81; click.button.y = 61;
        view.handle_event(click, running);
        check(view.selected_cell() == maps::GridCell{114,114}, "SDL coordinate selection");
        SDL_Event wheel{}; wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.mouse_x = 81; wheel.wheel.mouse_y = 61; wheel.wheel.y = 1;
        view.handle_event(wheel, running);
        view.handle_event(click, running);
        check(view.selected_cell() == maps::GridCell{114,114}, "selection after wheel zoom");
        SDL_Event change_view{}; change_view.type = SDL_EVENT_KEY_DOWN; change_view.key.key = SDLK_V;
        view.handle_event(change_view, running);
        check(view.view() == maps::MapViewMode::Semantic &&
              view.selected_cell() == maps::GridCell{114,114} && view.render(),
              "semantic view preserves selected storage cell");
        SDL_Event change_mask{}; change_mask.type = SDL_EVENT_KEY_DOWN; change_mask.key.key = SDLK_M;
        view.handle_event(change_mask, running);
        check(view.mask() == maps::MaskMode::Candidate && view.render(), "candidate mask view");
        view.handle_event(change_mask, running);
        check(view.mask() == maps::MaskMode::OffMap && view.render(), "offmap diagnostic view");
        view.handle_event(change_mask, running);
        check(view.mask() == maps::MaskMode::Compare && view.render(), "mask comparison view");
        view.handle_event(change_view, running);
        check(view.view() == maps::MapViewMode::Projected &&
              view.selected_cell() == maps::GridCell{114,114} && view.render(),
              "projected view retains selected storage cell");
        click.button.x = 80; click.button.y = 60;
        view.handle_event(click, running);
        check(view.selected_cell() == maps::GridCell{114,114},
              "projected screen selection maps back to same storage cell");
        view.handle_event(change_view, running);
        check(view.view() == maps::MapViewMode::Storage &&
              view.selected_cell() == maps::GridCell{114,114} && view.render(),
              "raw storage view remains reachable after cycling");
        SDL_Event center_select{}; center_select.type = SDL_EVENT_KEY_DOWN;
        center_select.key.key = SDLK_RETURN;
        view.handle_event(center_select, running);
        check(view.selected_cell() == maps::GridCell{114,114},
              "keyboard selection uses the visible center storage cell");
        const auto old_center = view.camera().screen_to_grid(
            {view.camera().viewport_width*0.5,view.camera().viewport_height*0.5});
        check(SDL_SetWindowSize(window,200,150), "resize dummy window");
        check(view.render(), "render after resize");
        const auto new_center = view.camera().screen_to_grid(
            {view.camera().viewport_width*0.5,view.camera().viewport_height*0.5});
        check(std::abs(old_center.x-new_center.x)<1e-8 &&
              std::abs(old_center.y-new_center.y)<1e-8, "resize preserves grid center");
        view.shutdown();
        auto unsupported_map = fixture(); unsupported_map.declared_map_size = 85;
        openemperor::MapDebugView unsupported_view{std::move(unsupported_map)};
        unsupported_view.initialize(window,renderer);
        unsupported_view.handle_event(change_view,running);
        check(unsupported_view.view() == maps::MapViewMode::Semantic && unsupported_view.render(),
              "unsupported geometry retains semantic storage view");
        unsupported_view.handle_event(change_view,running);
        check(unsupported_view.view() == maps::MapViewMode::Storage,
              "unsupported geometry cycles back to raw storage instead of projection");
        unsupported_view.handle_event(change_mask,running);
        check(unsupported_view.mask() == maps::MaskMode::OffMap,
              "unsupported geometry can still show offmap bit without candidate mask");
        unsupported_view.shutdown();
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        auto bad = fixture(); bad.terrain_raw.values.clear();
        try { maps::make_storage_rgba(bad, maps::RawLayer::Terrain); }
        catch (const std::runtime_error&) {
            std::cout << "software map debug checks passed\n"; return 0;
        }
        throw std::runtime_error("incomplete layer was accepted");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
