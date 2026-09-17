#pragma once

#include "maps/MapVisualization.h"
#include "maps/TerrainBindings.h"
#include "renderer/TerrainPreviewRenderer.h"

#include <memory>
#include <optional>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;

namespace openemperor {

class MapDebugView {
public:
    explicit MapDebugView(maps::ParsedEmperorMap map,
                          maps::RawLayer initial_layer = maps::RawLayer::Terrain,
                          maps::MapViewMode initial_view = maps::MapViewMode::Storage,
                          std::optional<maps::TerrainBindings> bindings = std::nullopt);
    ~MapDebugView();
    MapDebugView(const MapDebugView&) = delete;
    MapDebugView& operator=(const MapDebugView&) = delete;
    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event, bool& running);
    void update(double seconds);
    bool render();
    maps::RawLayer layer() const { return layer_; }
    maps::MapViewMode view() const { return view_; }
    maps::MaskMode mask() const { return mask_; }
    std::optional<maps::GridCell> selected_cell() const { return selected_; }
    const maps::StorageGridCamera& camera() const { return camera_; }
private:
    void set_layer(maps::RawLayer layer);
    void set_view(maps::MapViewMode view);
    void set_mask(maps::MaskMode mask);
    void upload_pixels();
    void show_selected();
    std::optional<maps::GridCell> storage_from_display(
        std::optional<maps::DisplayCell> display) const;
    void update_title();
    void reset_camera();
    void resize_camera();
    void zoom_textured(scene::Point screen, double factor);
    maps::ParsedEmperorMap map_;
    maps::RawLayer layer_;
    maps::MapViewMode view_;
    maps::MaskMode mask_ = maps::MaskMode::Full;
    maps::MapGeometry geometry_;
    std::vector<maps::TerrainCellInterpretation> interpreted_;
    std::uint32_t texture_width_ = maps::stored_grid_width;
    std::uint32_t texture_height_ = maps::stored_grid_height;
    maps::StorageGridCamera camera_;
    scene::Camera2D textured_camera_;
    std::unique_ptr<TerrainPreviewRenderer> textured_renderer_;
    std::optional<maps::GridCell> selected_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};

} // namespace openemperor
