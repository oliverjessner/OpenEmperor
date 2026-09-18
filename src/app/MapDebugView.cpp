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
                           maps::MapViewMode initial_view,
                           std::optional<maps::TerrainBindings> bindings,
                           std::optional<maps::StoredGraphicsPlan> stored_plan)
    : map_(std::move(map)), layer_(initial_layer), view_(initial_view),
      geometry_(map_.declared_map_size), interpreted_(maps::interpret_map(map_)) {
    if ((view_ == maps::MapViewMode::Projected || view_ == maps::MapViewMode::Textured ||
         view_ == maps::MapViewMode::StoredGraphics) && !geometry_.supported)
        throw std::invalid_argument("projected/texture view requires supported map geometry");
    if (bindings) textured_renderer_ = std::make_unique<TerrainPreviewRenderer>(
        maps::make_terrain_render_plan(map_, geometry_, *bindings), std::move(*bindings));
    if (stored_plan) stored_renderer_ = std::make_unique<StoredGraphicsRenderer>(std::move(*stored_plan));
    if (view_ == maps::MapViewMode::Textured && !textured_renderer_)
        throw std::invalid_argument("textured view requires --terrain-bindings");
    if (view_ == maps::MapViewMode::StoredGraphics && !stored_renderer_)
        throw std::invalid_argument("stored graphics view requires --graphics-profile");
}
MapDebugView::~MapDebugView() { shutdown(); }
bool MapDebugView::is_texture_view() const {
    return view_ == maps::MapViewMode::Textured || view_ == maps::MapViewMode::StoredGraphics;
}

void MapDebugView::initialize(SDL_Window* window, SDL_Renderer* renderer) {
    window_ = window;
    renderer_ = renderer;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &camera_.viewport_width, &camera_.viewport_height))
        throw std::runtime_error(SDL_GetError());
    textured_camera_.viewport_width = camera_.viewport_width;
    textured_camera_.viewport_height = camera_.viewport_height;
    if (textured_renderer_) {
        textured_renderer_->initialize(renderer_);
        const auto& counts = textured_renderer_->plan().counts;
        std::cout << "Curated terrain preview: candidate=" << counts.candidate
                  << " bound=" << counts.bound << " unmapped=" << counts.unmapped
                  << " excluded_by_preview_mask=" << counts.excluded
                  << " asset_error=" << counts.asset_error
                  << " loaded_assets=" << textured_renderer_->texture_count() << '\n';
        for (const auto& [id, count] : counts.by_binding)
            std::cout << "  binding " << id << "=" << count << '\n';
    }
    if (stored_renderer_) {
        stored_renderer_->initialize(renderer_);
        const auto& plan = stored_renderer_->plan();
        std::cout << "Stored graphics preview: profile=" << maps::stored_graphics_profile
                  << " candidate=" << plan.cells.size() << " excluded=" << plan.excluded
                  << " mask_mismatches=" << plan.mask_comparison.mismatches()
                  << " distinct_referenced_assets=" << plan.assets.size()
                  << " decoded_assets=" << plan.decoded_assets
                  << " texture_uploads=" << plan.texture_uploads
                  << " covered_cells=" << plan.covered_cells()
                  << " one_by_one_instances=" << plan.footprint_count(1)
                  << " two_by_two_instances=" << plan.footprint_count(2)
                  << " distinct_textures=" << plan.texture_uploads
                  << " multi_tile_preview=" << plan.multi_tile_preview
                  << " footprint_policy=" << maps::footprint_policy_name(plan.footprint_policy)
                  << " marker_deviations=" << plan.marker_deviations
                  << " unknown_bit_cells=" << plan.unknown_bit_cells
                  << " logical_texture_bytes=" << plan.logical_texture_bytes << '\n';
        for (const auto& [status,count] : plan.status_counts())
            std::cout << "  status " << status << '=' << count << '\n';
    }
    camera_.grid_width = view_ == maps::MapViewMode::Projected ? geometry_.declared_size : maps::stored_grid_width;
    camera_.grid_height = camera_.grid_width;
    reset_camera();
    if (!is_texture_view()) upload_pixels();
    else update_title();
}
void MapDebugView::shutdown() {
    if (textured_renderer_) textured_renderer_->shutdown();
    if (stored_renderer_) stored_renderer_->shutdown();
    if (texture_) { SDL_DestroyTexture(texture_); texture_ = nullptr; }
    renderer_ = nullptr;
    window_ = nullptr;
}
void MapDebugView::reset_camera() {
    if (is_texture_view()) {
        double min_x=0,max_x=0,min_y=0,max_y=0;
        bool first=true;
        const auto include = [&](scene::Point origin,double width,double height) {
            if (first) { min_x=origin.x; max_x=origin.x+width; min_y=origin.y; max_y=origin.y+height; first=false; }
            else { min_x=std::min(min_x,origin.x); max_x=std::max(max_x,origin.x+width);
                   min_y=std::min(min_y,origin.y); max_y=std::max(max_y,origin.y+height); }
        };
        if (view_ == maps::MapViewMode::Textured) {
            for (const auto& instance : textured_renderer_->plan().instances)
                include(instance.image_origin,78,40);
        } else {
            const auto& plan = stored_renderer_->plan();
            for (const auto& footprint : plan.footprints) {
                if (footprint.status != maps::StoredStatus::Rendered) continue;
                const auto& record=plan.assets[footprint.asset_index].record;
                include(footprint.image_origin,record.width,record.height);
            }
            for (const auto& cell : plan.cells) {
                if (!cell.footprint_index || cell.status != maps::StoredStatus::Rendered)
                    include({cell.world.x-40,cell.world.y},80,40);
            }
        }
        if (first) return;
        textured_camera_.zoom = std::clamp(std::min(
            std::max(1, textured_camera_.viewport_width - 40) / (max_x - min_x),
            std::max(1, textured_camera_.viewport_height - 110) / (max_y - min_y)), 0.05, 8.0);
        textured_camera_.center_on({(min_x + max_x) * 0.5, (min_y + max_y) * 0.5});
        textured_camera_.offset.y += 43.0;
        return;
    }
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
        const auto textured_center = textured_camera_.screen_to_world(
            {textured_camera_.viewport_width * 0.5, textured_camera_.viewport_height * 0.5});
        textured_camera_.viewport_width = width;
        textured_camera_.viewport_height = height;
        textured_camera_.center_on(textured_center);
    }
}
void MapDebugView::upload_pixels() {
    if (is_texture_view()) { update_title(); return; }
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
    if (view == maps::MapViewMode::Textured && !textured_renderer_) return;
    if (view == maps::MapViewMode::StoredGraphics && !stored_renderer_) return;
    if (view_ == view) return;
    view_ = view;
    camera_.grid_width = view == maps::MapViewMode::Projected ? geometry_.declared_size : maps::stored_grid_width;
    camera_.grid_height = camera_.grid_width;
    reset_camera();
    if (selected_) {
        if (view == maps::MapViewMode::Textured || view == maps::MapViewMode::StoredGraphics)
            textured_camera_.center_on(maps::terrain_world(*selected_, geometry_.border));
        else if (view == maps::MapViewMode::Projected) {
            const auto pixel = geometry_.projected_origin(*selected_);
            if (pixel) camera_.center_on({pixel->x + 0.5, pixel->y + 0.5});
        } else camera_.center_on({selected_->x + 0.5, selected_->y + 0.5});
    }
    upload_pixels();
}
void MapDebugView::set_mask(maps::MaskMode mask) {
    if (is_texture_view()) return;
    if (!geometry_.supported && (mask == maps::MaskMode::Candidate || mask == maps::MaskMode::Compare)) return;
    if (mask_ == mask) return;
    mask_ = mask;
    upload_pixels();
}
void MapDebugView::update_title() {
    std::string title = (view_ == maps::MapViewMode::Textured ? "Curated terrain preview" :
                         view_ == maps::MapViewMode::StoredGraphics ? "Stored graphics preview" :
                         "OpenEmperor map debug") +
        std::string{" | "} + maps::view_name(view_) +
        " | mask=" + maps::mask_name(mask_) +
        (view_ == maps::MapViewMode::StoredGraphics ? " | saved IDs, diagnostic placement" :
         " | reference-derived categories");
    if (selected_) {
        const auto x = selected_->x, y = selected_->y;
        const auto& item = interpreted_[static_cast<std::size_t>(y) * maps::stored_grid_width + x];
        title += " | storage(" + std::to_string(x) + "," + std::to_string(y) + ")";
        title += " T=" + hex32(item.terrain_raw) + " O=" + hex32(item.objects_raw);
        title += " " + std::string{maps::category_name(item.category)};
        if (view_ == maps::MapViewMode::Textured && textured_renderer_) {
            const auto status = textured_renderer_->plan().status_by_storage[
                static_cast<std::size_t>(y) * maps::stored_grid_width + x];
            title += " " + std::string{maps::preview_status_name(status)};
        }
        if (view_ == maps::MapViewMode::StoredGraphics && stored_renderer_) {
            const auto* cell = stored_renderer_->plan().at(*selected_);
            title += cell ? " ID=" + hex32(cell->stored_id) + " " + maps::stored_status_name(cell->status)
                          : " outside_candidate_mask";
        }
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
    if (view_ == maps::MapViewMode::Textured && textured_renderer_) {
        const auto status = textured_renderer_->plan().status_by_storage[
            static_cast<std::size_t>(y) * maps::stored_grid_width + x];
        std::cout << "preview_status=" << maps::preview_status_name(status);
        if (status == maps::PreviewStatus::CuratedPreview) {
            const auto& plan = textured_renderer_->plan();
            const auto found = std::find_if(plan.instances.begin(), plan.instances.end(),
                [x,y](const auto& cell) { return cell.storage.x == x && cell.storage.y == y; });
            if (found != plan.instances.end()) {
                const auto& asset = textured_renderer_->asset_for_alias(found->asset_alias);
                std::cout << " binding=" << found->binding_id << " asset_alias=" << found->asset_alias
                          << " archive=" << asset.id.archive_relative_path.generic_string()
                          << " image_index=" << asset.id.image_index;
            }
        }
        std::cout << '\n';
    }
    if (view_ == maps::MapViewMode::StoredGraphics && stored_renderer_) {
        const auto& plan = stored_renderer_->plan();
        const auto* cell = plan.at(*selected_);
        if (!cell) { std::cout << "stored_graphics_status=excluded\n"; return; }
        std::cout << "stored_graphics_profile=" << maps::stored_graphics_profile
                  << " stored_id=" << hex32(cell->stored_id) << " (" << cell->stored_id << ')'
                  << " logical_offset=" << cell->logical_offset
                  << " candidate_byte=" << static_cast<unsigned>(cell->candidate_byte)
                  << " candidate_byte_hex=" << hex32(cell->candidate_byte)
                  << " slot=" << cell->slot << " local_index=" << cell->local_index
                  << " system_record_skip=" << cell->system_record_skip
                  << " physical_record=";
        if (cell->physical_record) std::cout << *cell->physical_record;
        else std::cout << "unresolved";
        std::cout << " lookup_status=" << maps::graphics_id_status_name(cell->lookup_status)
                  << " record_present=" << cell->record_present
                  << " source_ranges_valid=" << cell->source_ranges_valid
                  << " footprint_supported=" << cell->footprint_supported
                  << " render_status=" << maps::stored_status_name(cell->status);
        if (cell->subtile) {
            std::cout << " subtile_profile=" << maps::edge_byte_profile
                      << " part_x=" << static_cast<unsigned>(cell->subtile->part_x)
                      << " part_y=" << static_cast<unsigned>(cell->subtile->part_y)
                      << " draw_marker_candidate=" << cell->subtile->draw_marker_candidate
                      << " unknown_bits=" << hex32(cell->subtile->unknown_bits);
            if (cell->subtile_origin)
                std::cout << " metadata_origin=(" << cell->subtile_origin->x << ','
                          << cell->subtile_origin->y << ')';
        }
        if (cell->asset_index) {
            const auto& asset = plan.assets[*cell->asset_index];
            const auto& record = asset.record;
            std::cout << " archive=" << record.id.archive_relative_path.generic_string()
                      << " type=" << record.image_type << " size=" << record.width << 'x' << record.height;
            if (cell->footprint_supported)
                std::cout << " preview_anchor=(" << record.width / 2.0 << ','
                          << record.height - (cell->footprint_index &&
                             plan.footprints[*cell->footprint_index].width_cells==2 ? 80 : 40) << ')';
            else std::cout << " preview_anchor=unverified";
            std::cout << " omega_overlay=" << (record.image_type == 30 &&
                record.uncompressed_length <= record.data_length ?
                (record.data_length > record.uncompressed_length ? "yes" : "no") : "unverified")
                      << " color_bounds=" << assets::asset_range_status_name(record.color_bounds)
                      << " alpha_bounds=" << assets::asset_range_status_name(record.alpha_bounds)
                      << " decode_attempted=" << asset.decode_attempted
                      << " decode_success=" << asset.decode_succeeded;
            if (!asset.error.empty()) std::cout << " decode_error=" << asset.error;
        }
        if (cell->footprint_index) {
            const auto& footprint=plan.footprints[*cell->footprint_index];
            std::cout << " footprint_instance=" << footprint.id
                      << " footprint_origin=(" << footprint.origin.x << ',' << footprint.origin.y << ')'
                      << " footprint_size=" << footprint.width_cells << 'x' << footprint.height_cells
                      << " placement_rule=" << footprint.rule
                      << " draw_cell_candidate=";
            if (footprint.draw_cell_candidate)
                std::cout << '(' << footprint.draw_cell_candidate->x << ','
                          << footprint.draw_cell_candidate->y << ')';
            else std::cout << "none";
            std::cout
                      << " image_origin=(" << footprint.image_origin.x << ','
                      << footprint.image_origin.y << ')';
        }
        std::cout << '\n';
    }
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
                view_ == maps::MapViewMode::Projected && textured_renderer_ ?
                    maps::MapViewMode::Textured :
                view_ == maps::MapViewMode::Projected && stored_renderer_ ?
                    maps::MapViewMode::StoredGraphics : maps::MapViewMode::Storage;
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
            if (is_texture_view())
                selected_ = maps::pick_terrain_cell(textured_camera_.screen_to_world(
                    {textured_camera_.viewport_width * 0.5, textured_camera_.viewport_height * 0.5}), geometry_);
            else selected_ = storage_from_display(camera_.pick({camera_.viewport_width * 0.5,
                                                               camera_.viewport_height * 0.5}));
            update_title();
            show_selected();
        }
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        float x = event.wheel.mouse_x, y = event.wheel.mouse_y;
        if (SDL_RenderCoordinatesFromWindow(renderer_, x, y, &x, &y)) {
            if (is_texture_view()) zoom_textured({x,y}, std::pow(1.15, event.wheel.y));
            else camera_.zoom_at({x, y}, std::pow(1.15, event.wheel.y));
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        float x = event.button.x, y = event.button.y;
        if (SDL_RenderCoordinatesFromWindow(renderer_, x, y, &x, &y)) {
            if (is_texture_view())
                selected_ = maps::pick_terrain_cell(textured_camera_.screen_to_world({x,y}), geometry_);
            else selected_ = storage_from_display(camera_.pick({x, y}));
            update_title();
            show_selected();
        }
    }
}
void MapDebugView::zoom_textured(scene::Point screen, double factor) {
    if (!std::isfinite(factor) || factor <= 0) return;
    const auto before = textured_camera_.screen_to_world(screen);
    textured_camera_.zoom = std::clamp(textured_camera_.zoom * factor, 0.05, 8.0);
    textured_camera_.offset = {screen.x - before.x * textured_camera_.zoom,
                               screen.y - before.y * textured_camera_.zoom};
}
void MapDebugView::update(double seconds) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const double movement = 400.0 * std::clamp(seconds, 0.0, 0.05);
    double dx = 0, dy = 0;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) dx += movement;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dx -= movement;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) dy += movement;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) dy -= movement;
    if (is_texture_view()) {
        textured_camera_.offset.x += dx;
        textured_camera_.offset.y += dy;
    } else {
        camera_.offset.x += dx;
        camera_.offset.y += dy;
    }
}
bool MapDebugView::render() {
    resize_camera();
    if (!SDL_SetRenderDrawColor(renderer_, 22, 26, 32, 255) || !SDL_RenderClear(renderer_)) return false;
    if (is_texture_view()) {
        if (view_ == maps::MapViewMode::Textured) {
            if (!textured_renderer_->render(textured_camera_, selected_)) return false;
        } else {
            if (!stored_renderer_->render(textured_camera_,selected_)) return false;
            if (!reported_stored_draws_) {
                std::cout << "Stored graphics viewport: texture_draws="
                          << stored_renderer_->last_texture_draws()
                          << " diagnostic_draws=" << stored_renderer_->last_diagnostic_draws() << '\n';
                reported_stored_draws_=true;
            }
        }
    } else {
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
    }
    const SDL_FRect legend{0, 0, static_cast<float>(camera_.viewport_width),
                           camera_.viewport_height < 200 ? 42.0F :
                           (is_texture_view() ? 103.0F : 86.0F)};
    if (!SDL_SetRenderDrawColor(renderer_, 12, 17, 23, 255) || !SDL_RenderFillRect(renderer_, &legend) ||
        !SDL_SetRenderDrawColor(renderer_, 235, 240, 245, 255)) return false;
    const std::string heading = std::string{"MAP "} + maps::view_name(view_) + " | MASK " + maps::mask_name(mask_) +
        " | RAW " + layer_name(layer_) +
        (view_ == maps::MapViewMode::StoredGraphics ?
         " | SAVED IDS / DIAGNOSTIC | V VIEW M MASK 1/2 RAW LAYER" :
         " | REFERENCE-DERIVED | V VIEW M MASK 1/2 RAW LAYER");
    if (!SDL_RenderDebugText(renderer_, 8, 5, heading.c_str())) return false;
    if (!SDL_RenderDebugText(renderer_, 8, 21,
        view_ == maps::MapViewMode::StoredGraphics ?
        "STORED GRAPHICS PREVIEW: SAVED IDS, DIAGNOSTIC PLACEMENT; PURPLE = UNRESOLVED" :
        view_ == maps::MapViewMode::Textured ?
        "CURATED TERRAIN PREVIEW: ORIGINAL SG3 TILE FOR EXACT RAW PAIRS; PURPLE DIAMOND = UNMAPPED" :
        "SEMANTIC: WATER BLUE, VEGETATION GREEN, ROCK GRAY, ROAD TAN, FERTILE LIME, OTHER PURPLE, UNKNOWN MAGENTA")) return false;
    if (!SDL_RenderDebugText(renderer_, 8, 37, is_texture_view() ?
        "PREVIEW PLACEMENT IS DIAGNOSTIC | WASD PAN WHEEL ZOOM R RESET" :
        "COMPARE: INSIDE+ON GREEN, INSIDE+OFF PINK, OUTSIDE+OFF BLUE, OUTSIDE+ON ORANGE | WASD PAN WHEEL ZOOM")) return false;
    if (selected_) {
        const auto x = selected_->x, y = selected_->y;
        const auto& item = interpreted_[static_cast<std::size_t>(y) * maps::stored_grid_width + x];
        const std::string details = "CELL (" + std::to_string(x) + "," + std::to_string(y) +
            ") T=" + hex32(item.terrain_raw) + " O=" + hex32(item.objects_raw) +
            " " + maps::category_name(item.category) + " RULE=" + std::string{item.rule} +
            " CAND=" + (geometry_.supported ? (geometry_.contains(*selected_) ? "Y" : "N") : "?") +
            " OFF=" + ((item.terrain_raw & 0x80000U) ? "Y" : "N") +
            (view_ == maps::MapViewMode::Textured ?
             " STATUS=" + std::string{maps::preview_status_name(textured_renderer_->plan().status_by_storage[
                 static_cast<std::size_t>(y) * maps::stored_grid_width + x])} :
             view_ == maps::MapViewMode::StoredGraphics ?
             " STATUS=" + std::string{maps::stored_status_name(stored_renderer_->plan().status_by_storage[
                 static_cast<std::size_t>(y) * maps::stored_grid_width + x])} : "");
        if (!SDL_RenderDebugText(renderer_, 8, 53, details.c_str())) return false;
        const std::string flags = "FLAGS T=" + hex32(item.recognized_terrain_flags) +
            " O=" + hex32(item.recognized_object_flags) +
            " UNKNOWN T=" + hex32(item.unknown_terrain_bits) +
            " O=" + hex32(item.unknown_object_bits) +
            " T@" + std::to_string(map_.terrain_cell_offset(x,y)) +
            " O@" + std::to_string(map_.object_cell_offset(x,y)) +
            " PARTIAL=" + (item.partial ? "Y" : "N");
        if (!SDL_RenderDebugText(renderer_, 8, 69, flags.c_str())) return false;
        if (view_ == maps::MapViewMode::Textured) {
            const auto& plan = textured_renderer_->plan();
            const auto found = std::find_if(plan.instances.begin(), plan.instances.end(),
                [x,y](const auto& cell) { return cell.storage.x == x && cell.storage.y == y; });
            if (found != plan.instances.end() && found->status == maps::PreviewStatus::CuratedPreview) {
                const auto& asset = textured_renderer_->asset_for_alias(found->asset_alias);
                const auto line = "BINDING " + found->binding_id + " ASSET=" +
                    asset.id.archive_relative_path.generic_string() + "#" + std::to_string(asset.id.image_index);
                if (!SDL_RenderDebugText(renderer_, 8, 85, line.c_str())) return false;
            }
        }
        if (view_ == maps::MapViewMode::StoredGraphics) {
            const auto* cell = stored_renderer_->plan().at(*selected_);
            if (cell) {
                const auto line = "SAVED ID=" + hex32(cell->stored_id) + " SLOT=" +
                    std::to_string(cell->slot) + " LOCAL=" + std::to_string(cell->local_index) +
                    " PHYSICAL=" + (cell->physical_record ? std::to_string(*cell->physical_record) : "?") +
                    " STATUS=" + maps::stored_status_name(cell->status);
                if (!SDL_RenderDebugText(renderer_, 8, 85, line.c_str())) return false;
            }
        }
    }
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
