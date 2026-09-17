#pragma once

#include "renderer/SceneRenderer.h"

#include <optional>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace openemperor {

class SceneView {
public:
    explicit SceneView(scene::Scene scene);
    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event, bool& running);
    void update(double seconds);
    bool render();
    std::size_t texture_count() const { return renderer_.texture_count(); }
    std::size_t last_drawn_instances() const { return renderer_.last_drawn_instances(); }
    std::optional<scene::Cell> selected_cell() const { return selected_; }
    const scene::Camera2D& camera() const { return camera_; }
private:
    void reset_camera();
    void update_title();
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SceneRenderer renderer_;
    scene::Camera2D camera_;
    std::optional<scene::Cell> selected_;
    bool grid_ = false;
};

} // namespace openemperor
