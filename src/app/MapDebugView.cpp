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

MapDebugView::MapDebugView(maps::ParsedEmperorMap map, maps::RawLayer initial_layer,
                           maps::MapViewMode initial_view)
    : map_(std::move(map)), layer_(initial_layer), view_(initial_view),
      geometry_(map_.declared_map_size), interpreted_(maps::interpret_map(map_)) {
    if (view_ == maps::MapViewMode::Projected && !geometry_.supported)
        throw std::invalid_argument("projected view requires a supported map geometry");
}
MapDebugView::~MapDebugView() { shutdown(); }

void MapDebugView::initialize(SDL_Window* window, SDL_Renderer* renderer) {
    window_ = window;
    renderer_ = renderer;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &camera_.viewport_width, &camera_.viewport_height))
        throw std::runtime_error(SDL_GetError());
    camera_.grid_width = view_ == maps::MapViewMode::Projected ? geometry_.declared_size : maps::stored_grid_width;
    camera_.grid_height = camera_.grid_width;
    reset_camera();
    upload_pixels();
}
void MapDebugView::shutdown() {
    if (texture_) { SDL_DestroyTexture(texture_); texture_ = nullptr; }
    renderer_ = nullptr;
    window_ = nullptr;
}
void MapDebugView::reset_camera() {
    if (view_ == maps::MapViewMode::Projected) {
        const double available_width = std::max(1, camera_.viewport_width - 40);
        const double available_height = std::max(1, camera_.viewport_height - 110);
        camera_.zoom = std::clamp(std::min(available_width, available_height) /
                                  (maps::StorageGridCamera::base_cell_pixels * geometry_.declared_size),
                                  0.25, 16.0);
    } else camera_.zoom = 1.0;
    camera_.center_on({camera_.grid_width * 0.5, camera_.grid_height * 0.5});
    if (view_ == maps::MapViewMode::Projected) camera_.offset.y += 43.0;
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
void MapDebugView::upload_pixels() {
    const auto pixels = maps::make_map_debug_pixels(map_, interpreted_, geometry_, view_, layer_, mask_);
    if (!texture_ || texture_width_ != pixels.width || texture_height_ != pixels.height) {
        if (texture_) SDL_DestroyTexture(texture_);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                     static_cast<int>(pixels.width), static_cast<int>(pixels.height));
        if (!texture_ || !SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST) ||
            !SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE))
            throw std::runtime_error(std::string{"map debug texture creation failed: "} + SDL_GetError());
        texture_width_ = pixels.width;
        texture_height_ = pixels.height;
    }
    if (!SDL_UpdateTexture(texture_, nullptr, pixels.rgba.data(), static_cast<int>(pixels.width * 4U)))
        throw std::runtime_error(std::string{"map debug texture upload failed: "} + SDL_GetError());
    update_title();
}
void MapDebugView::set_layer(maps::RawLayer layer) {
    if (layer_ == layer) return;
    layer_ = layer;
    if (view_ == maps::MapViewMode::Storage) upload_pixels();
    else update_title();
}
void MapDebugView::set_view(maps::MapViewMode view) {
    if (view == maps::MapViewMode::Projected && !geometry_.supported) return;
    if (view_ == view) return;
    view_ = view;
    camera_.grid_width = view == maps::MapViewMode::Projected ? geometry_.declared_size : maps::stored_grid_width;
    camera_.grid_height = camera_.grid_width;
    reset_camera();
    if (selected_) {
        if (view == maps::MapViewMode::Projected) {
            const auto pixel = geometry_.projected_origin(*selected_);
            if (pixel) camera_.center_on({pixel->x + 0.5, pixel->y + 0.5});
        } else camera_.center_on({selected_->x + 0.5, selected_->y + 0.5});
    }
    upload_pixels();
}
void MapDebugView::set_mask(maps::MaskMode mask) {
    if (!geometry_.supported && (mask == maps::MaskMode::Candidate || mask == maps::MaskMode::Compare)) return;
    if (mask_ == mask) return;
    mask_ = mask;
    upload_pixels();
}
void MapDebugView::update_title() {
    std::string title = "OpenEmperor map debug | " + std::string{maps::view_name(view_)} +
        " | mask=" + maps::mask_name(mask_) + " | reference-derived categories";
    if (selected_) {
        const auto x = selected_->x, y = selected_->y;
        const auto& item = interpreted_[static_cast<std::size_t>(y) * maps::stored_grid_width + x];
        title += " | storage(" + std::to_string(x) + "," + std::to_string(y) + ")";
        title += " T=" + hex32(item.terrain_raw) + " O=" + hex32(item.objects_raw);
        title += " " + std::string{maps::category_name(item.category)};
    }
    SDL_SetWindowTitle(window_, title.c_str());
}
void MapDebugView::show_selected() {
    if (!selected_) return;
    const auto x = selected_->x, y = selected_->y;
    const auto& item = interpreted_[static_cast<std::size_t>(y) * maps::stored_grid_width + x];
    std::cout << "storage_cell=(" << x << ',' << y << ") terrain=" << item.terrain_raw << " ("
              << hex32(item.terrain_raw) << ") objects=" << item.objects_raw << " ("
              << hex32(item.objects_raw) << ") recognized_terrain_flags="
              << hex32(item.recognized_terrain_flags) << " unknown_terrain_bits="
              << hex32(item.unknown_terrain_bits) << " recognized_object_flags="
              << hex32(item.recognized_object_flags) << " unknown_object_bits="
              << hex32(item.unknown_object_bits) << " category=" << maps::category_name(item.category)
              << " rule=" << item.rule << " partial=" << item.partial
              << " evidence=reference-derived candidate=" <<
                 (geometry_.supported ? (geometry_.contains(*selected_) ? "yes" : "no") : "unknown")
              << " offmap_bit=" << ((item.terrain_raw & 0x80000U) ? "yes" : "no")
              << " terrain_offset=" << map_.terrain_cell_offset(x, y)
              << " objects_offset=" << map_.object_cell_offset(x, y) << '\n';
}
std::optional<maps::GridCell> MapDebugView::storage_from_display(
    std::optional<maps::DisplayCell> display) const {
    if (!display) return std::nullopt;
    if (view_ == maps::MapViewMode::Projected)
        return geometry_.at_projected({display->x,display->y});
    return maps::GridCell{display->x,display->y};
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
        if (event.key.key == SDLK_V) {
            const auto next = view_ == maps::MapViewMode::Storage ? maps::MapViewMode::Semantic :
                view_ == maps::MapViewMode::Semantic ?
                    (geometry_.supported ? maps::MapViewMode::Projected : maps::MapViewMode::Storage) :
                    maps::MapViewMode::Storage;
            set_view(next);
        }
        if (event.key.key == SDLK_M) {
            const auto next = !geometry_.supported ?
                (mask_ == maps::MaskMode::Full ? maps::MaskMode::OffMap : maps::MaskMode::Full) :
                mask_ == maps::MaskMode::Full ? maps::MaskMode::Candidate :
                mask_ == maps::MaskMode::Candidate ? maps::MaskMode::OffMap :
                mask_ == maps::MaskMode::OffMap ? maps::MaskMode::Compare : maps::MaskMode::Full;
            set_mask(next);
        }
        if (event.key.key == SDLK_R) reset_camera();
        if (event.key.key == SDLK_RETURN) {
            const auto display_cell = camera_.pick({camera_.viewport_width * 0.5,
                                                    camera_.viewport_height * 0.5});
            selected_ = storage_from_display(display_cell);
            update_title();
            show_selected();
        }
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        float x = event.wheel.mouse_x, y = event.wheel.mouse_y;
        if (SDL_RenderCoordinatesFromWindow(renderer_, x, y, &x, &y))
            camera_.zoom_at({x, y}, std::pow(1.15, event.wheel.y));
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        float x = event.button.x, y = event.button.y;
        if (SDL_RenderCoordinatesFromWindow(renderer_, x, y, &x, &y)) {
            const auto display_cell = camera_.pick({x, y});
            selected_ = storage_from_display(display_cell);
            update_title();
            show_selected();
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
        static_cast<float>(texture_width_ * scale), static_cast<float>(texture_height_ * scale)};
    if (!SDL_RenderTexture(renderer_, texture_, nullptr, &destination)) return false;
    if (selected_) {
        std::optional<maps::GridPoint> display;
        if (view_ == maps::MapViewMode::Projected) {
            const auto pixel = geometry_.projected_origin(*selected_);
            if (pixel) display = maps::GridPoint{static_cast<double>(pixel->x),static_cast<double>(pixel->y)};
        } else display = maps::GridPoint{static_cast<double>(selected_->x),static_cast<double>(selected_->y)};
        if (display) {
            const auto corner = camera_.grid_to_screen(*display);
            const SDL_FRect rectangle{static_cast<float>(corner.x), static_cast<float>(corner.y),
                static_cast<float>((view_ == maps::MapViewMode::Projected ? 2.0 : 1.0) * scale),
                static_cast<float>(scale)};
            if (!SDL_SetRenderDrawColor(renderer_, 255, 245, 90, 255) || !SDL_RenderRect(renderer_, &rectangle)) return false;
        }
    }
    const SDL_FRect legend{0, 0, static_cast<float>(camera_.viewport_width),
                           camera_.viewport_height < 200 ? 42.0F : 86.0F};
    if (!SDL_SetRenderDrawColor(renderer_, 12, 17, 23, 255) || !SDL_RenderFillRect(renderer_, &legend) ||
        !SDL_SetRenderDrawColor(renderer_, 235, 240, 245, 255)) return false;
    const std::string heading = std::string{"MAP "} + maps::view_name(view_) + " | MASK " + maps::mask_name(mask_) +
        " | RAW " + layer_name(layer_) + " | REFERENCE-DERIVED | V VIEW M MASK 1/2 RAW LAYER";
    if (!SDL_RenderDebugText(renderer_, 8, 5, heading.c_str())) return false;
    if (!SDL_RenderDebugText(renderer_, 8, 21,
        "SEMANTIC: WATER BLUE, VEGETATION GREEN, ROCK GRAY, ROAD TAN, FERTILE LIME, OTHER PURPLE, UNKNOWN MAGENTA")) return false;
    if (!SDL_RenderDebugText(renderer_, 8, 37,
        "COMPARE: INSIDE+ON GREEN, INSIDE+OFF PINK, OUTSIDE+OFF BLUE, OUTSIDE+ON ORANGE | WASD PAN WHEEL ZOOM")) return false;
    if (selected_) {
        const auto x = selected_->x, y = selected_->y;
        const auto& item = interpreted_[static_cast<std::size_t>(y) * maps::stored_grid_width + x];
        const std::string details = "CELL (" + std::to_string(x) + "," + std::to_string(y) +
            ") T=" + hex32(item.terrain_raw) + " O=" + hex32(item.objects_raw) +
            " " + maps::category_name(item.category) + " RULE=" + std::string{item.rule} +
            " CAND=" + (geometry_.supported ? (geometry_.contains(*selected_) ? "Y" : "N") : "?") +
            " OFF=" + ((item.terrain_raw & 0x80000U) ? "Y" : "N");
        if (!SDL_RenderDebugText(renderer_, 8, 53, details.c_str())) return false;
        const std::string flags = "FLAGS T=" + hex32(item.recognized_terrain_flags) +
            " O=" + hex32(item.recognized_object_flags) +
            " UNKNOWN T=" + hex32(item.unknown_terrain_bits) +
            " O=" + hex32(item.unknown_object_bits) +
            " T@" + std::to_string(map_.terrain_cell_offset(x,y)) +
            " O@" + std::to_string(map_.object_cell_offset(x,y)) +
            " PARTIAL=" + (item.partial ? "Y" : "N");
        if (!SDL_RenderDebugText(renderer_, 8, 69, flags.c_str())) return false;
    }
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
