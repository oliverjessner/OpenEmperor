#pragma once

#include "maps/StorageGridView.h"

#include <optional>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;

namespace openemperor {

class MapDebugView {
public:
    explicit MapDebugView(maps::ParsedEmperorMap map,
                          maps::RawLayer initial_layer = maps::RawLayer::Terrain);
    ~MapDebugView();
    MapDebugView(const MapDebugView&) = delete;
    MapDebugView& operator=(const MapDebugView&) = delete;
    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event, bool& running);
    void update(double seconds);
    bool render();
    maps::RawLayer layer() const { return layer_; }
    std::optional<maps::GridCell> selected_cell() const { return selected_; }
    const maps::StorageGridCamera& camera() const { return camera_; }
private:
    void set_layer(maps::RawLayer layer);
    void update_title();
    void reset_camera();
    void resize_camera();
    maps::ParsedEmperorMap map_;
    maps::RawLayer layer_;
    maps::StorageGridCamera camera_;
    std::optional<maps::GridCell> selected_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};

} // namespace openemperor
