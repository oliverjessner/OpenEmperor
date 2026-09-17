#include "app/MapDebugView.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor {
namespace {
std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
    return out.str();
}
const char* layer_name(maps::RawLayer layer) {
    return layer == maps::RawLayer::Terrain ? "terrain_raw" : "objects_raw";
}
} // namespace

MapDebugView::MapDebugView(maps::ParsedEmperorMap map, maps::RawLayer initial_layer)
    : map_(std::move(map)), layer_(initial_layer) {}
MapDebugView::~MapDebugView() { shutdown(); }

void MapDebugView::initialize(SDL_Window* window, SDL_Renderer* renderer) {
    window_ = window;
    renderer_ = renderer;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &camera_.viewport_width, &camera_.viewport_height))
        throw std::runtime_error(SDL_GetError());
    reset_camera();
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                 static_cast<int>(maps::stored_grid_width),
                                 static_cast<int>(maps::stored_grid_height));
    if (!texture_ || !SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE))
        throw std::runtime_error(std::string{"map debug texture creation failed: "} + SDL_GetError());
    set_layer(layer_);
}
void MapDebugView::shutdown() {
    if (texture_) { SDL_DestroyTexture(texture_); texture_ = nullptr; }
    renderer_ = nullptr;
    window_ = nullptr;
}
void MapDebugView::reset_camera() {
    camera_.zoom = 1.0;
    camera_.center_on({maps::stored_grid_width * 0.5, maps::stored_grid_height * 0.5});
}
void MapDebugView::resize_camera() {
    int width = 0, height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &width, &height))
        throw std::runtime_error(SDL_GetError());
    if (width != camera_.viewport_width || height != camera_.viewport_height) {
        const auto center = camera_.screen_to_grid(
            {camera_.viewport_width * 0.5, camera_.viewport_height * 0.5});
        camera_.viewport_width = width;
        camera_.viewport_height = height;
        camera_.center_on(center);
    }
}
void MapDebugView::set_layer(maps::RawLayer layer) {
    const auto pixels = maps::make_storage_rgba(map_, layer);
    if (!SDL_UpdateTexture(texture_, nullptr, pixels.data(),
                           static_cast<int>(maps::stored_grid_width * 4U)))
        throw std::runtime_error(std::string{"map debug texture upload failed: "} + SDL_GetError());
    layer_ = layer;
    update_title();
}
void MapDebugView::update_title() {
    std::string title = "OpenEmperor map debug | storage 228x228 | " +
        std::string{layer_name(layer_)} + " | raw-value colors (no terrain meaning)";
    if (selected_) {
        const auto x = selected_->x, y = selected_->y;
        title += " | (" + std::to_string(x) + "," + std::to_string(y) + ")";
        title += " T=" + std::to_string(map_.terrain_at(x, y)) + " " +
            hex32(map_.terrain_at(x, y));
        title += " O=" + std::to_string(map_.object_at(x, y)) + " " +
            hex32(map_.object_at(x, y));
        title += " T@" + std::to_string(map_.terrain_cell_offset(x, y)) +
            " O@" + std::to_string(map_.object_cell_offset(x, y)) +
            " active=unknown";
    }
    SDL_SetWindowTitle(window_, title.c_str());
}
void MapDebugView::handle_event(const SDL_Event& event, bool& running) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
        (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) { running = false; return; }
    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) resize_camera();
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key == SDLK_1) set_layer(maps::RawLayer::Terrain);
        if (event.key.key == SDLK_2 || event.key.key == SDLK_TAB)
            set_layer(event.key.key == SDLK_TAB && layer_ == maps::RawLayer::Objects
                          ? maps::RawLayer::Terrain : maps::RawLayer::Objects);
        if (event.key.key == SDLK_R) reset_camera();
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        float x = event.wheel.mouse_x, y = event.wheel.mouse_y;
        if (SDL_RenderCoordinatesFromWindow(renderer_, x, y, &x, &y))
            camera_.zoom_at({x, y}, std::pow(1.15, event.wheel.y));
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        float x = event.button.x, y = event.button.y;
        if (SDL_RenderCoordinatesFromWindow(renderer_, x, y, &x, &y)) {
            selected_ = camera_.pick({x, y});
            update_title();
            if (selected_) {
                const auto cx = selected_->x, cy = selected_->y;
                std::cout << "storage_cell=(" << cx << ',' << cy << ") terrain="
                          << map_.terrain_at(cx, cy) << " (" << hex32(map_.terrain_at(cx, cy))
                          << ") objects=" << map_.object_at(cx, cy) << " ("
                          << hex32(map_.object_at(cx, cy)) << ") terrain_offset="
                          << map_.terrain_cell_offset(cx, cy) << " objects_offset="
                          << map_.object_cell_offset(cx, cy) << " active=unknown\n";
            }
        }
    }
}
void MapDebugView::update(double seconds) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const double movement = 400.0 * std::clamp(seconds, 0.0, 0.05);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) camera_.offset.x += movement;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) camera_.offset.x -= movement;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) camera_.offset.y += movement;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) camera_.offset.y -= movement;
}
bool MapDebugView::render() {
    resize_camera();
    if (!SDL_SetRenderDrawColor(renderer_, 22, 26, 32, 255) || !SDL_RenderClear(renderer_)) return false;
    const auto top_left = camera_.grid_to_screen({0, 0});
    const double scale = maps::StorageGridCamera::base_cell_pixels * camera_.zoom;
    const SDL_FRect destination{static_cast<float>(top_left.x), static_cast<float>(top_left.y),
        static_cast<float>(maps::stored_grid_width * scale),
        static_cast<float>(maps::stored_grid_height * scale)};
    if (!SDL_RenderTexture(renderer_, texture_, nullptr, &destination)) return false;
    if (selected_) {
        const auto corner = camera_.grid_to_screen({static_cast<double>(selected_->x),
                                                    static_cast<double>(selected_->y)});
        const SDL_FRect rectangle{static_cast<float>(corner.x), static_cast<float>(corner.y),
                                  static_cast<float>(scale), static_cast<float>(scale)};
        if (!SDL_SetRenderDrawColor(renderer_, 255, 245, 90, 255) ||
            !SDL_RenderRect(renderer_, &rectangle)) return false;
    }
    const SDL_FRect legend{0, 0, static_cast<float>(camera_.viewport_width), 42.0F};
    if (!SDL_SetRenderDrawColor(renderer_, 12, 17, 23, 255) ||
        !SDL_RenderFillRect(renderer_, &legend) ||
        !SDL_SetRenderDrawColor(renderer_, 235, 240, 245, 255)) return false;
    const std::string label = std::string{"STORAGE GRID 228x228 | "} + layer_name(layer_) +
        " | COLORS = RAW VALUE HASH, NO TERRAIN MEANING | 1/2 LAYER, WASD PAN, WHEEL ZOOM";
    if (!SDL_RenderDebugText(renderer_, 8, 7, label.c_str())) return false;
    if (selected_) {
        const auto x = selected_->x, y = selected_->y;
        const std::string details = "CELL (" + std::to_string(x) + "," + std::to_string(y) +
            ") T=" + std::to_string(map_.terrain_at(x, y)) + " " + hex32(map_.terrain_at(x, y)) +
            " O=" + std::to_string(map_.object_at(x, y)) + " " + hex32(map_.object_at(x, y)) +
            " T@" + std::to_string(map_.terrain_cell_offset(x, y)) +
            " O@" + std::to_string(map_.object_cell_offset(x, y)) + " ACTIVE=UNKNOWN";
        if (!SDL_RenderDebugText(renderer_, 8, 23, details.c_str())) return false;
    }
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
