#pragma once

#include "maps/StoredMapSession.h"
#include "maps/MapGeometry.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "simulation/World.h"

#include <memory>
#include <cstdint>
#include <optional>
#include <string>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

namespace openemperor {

class SandboxView {
public:
    explicit SandboxView(maps::StoredMapSession session, bool demo);
    ~SandboxView();
    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event, bool& running);
    void update(double frame_seconds);
    bool render();
    void tick_once(); // Used by the finite, offscreen compatibility check.
    const simulation::World& world() const { return *world_; }
    std::optional<simulation::Cell> pick(scene::Point screen) const;
    simulation::CommandResult preview(simulation::Cell cell) const;
    void set_tool(int tool);
    int tool() const { return tool_; }
    const std::string& last_message() const { return last_message_; }
    std::optional<simulation::Cell> demo_origin() const { return demo_origin_; }
    scene::Camera2D camera() const { return camera_; }
private:
    void reset_camera();
    void resize_camera();
    void place_demo();
    bool draw_diamond(scene::Point world, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool fill);
    bool draw_world();
    bool draw_hud();
    scene::Point world_for(simulation::Position cell) const;
    maps::MapGeometry geometry_;
    StoredGraphicsRenderer background_;
    std::unique_ptr<simulation::World> world_;
    simulation::TickDriver clock_;
    scene::Camera2D camera_;
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
    std::optional<simulation::Cell> hovered_;
    std::optional<simulation::Cell> selected_;
    std::optional<simulation::Cell> demo_origin_;
    std::string last_message_;
    bool demo_=false;
    int tool_=4;
    static constexpr int hud_height_=116;
};

} // namespace openemperor
