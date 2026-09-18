#pragma once

#include "maps/StoredMapSession.h"
#include "maps/MapGeometry.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "simulation/World.h"
#include "persistence/SandboxSave.h"

#include <memory>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

namespace openemperor {

class SandboxView {
public:
    explicit SandboxView(maps::StoredMapSession session, bool demo,
                         simulation::RulesProfile rules=simulation::RulesProfile::LogisticsV1);
    ~SandboxView();
    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event, bool& running);
    void update(double frame_seconds);
    bool render();
    void tick_once(); // Used by the finite, offscreen compatibility check.
    simulation::CommandResult execute(simulation::Command command);
    const simulation::World& world() const { return *world_; }
    std::optional<simulation::Cell> pick(scene::Point screen) const;
    simulation::CommandResult preview(simulation::Cell cell) const;
    void set_tool(int tool);
    int tool() const { return tool_; }
    const std::string& last_message() const { return last_message_; }
    std::optional<simulation::Cell> demo_origin() const { return demo_origin_; }
    scene::Camera2D camera() const { return camera_; }
    int last_courier_draws() const { return last_courier_draws_; }
    void configure_save(std::filesystem::path data_root,std::filesystem::path map_relative,
                        std::filesystem::path save_path,
                        std::optional<persistence::SaveDocument> initial=std::nullopt);
    void save_now();
    void load_now();
    std::uint64_t io_generation() const { return io_generation_; }
    bool paused() const { return clock_.paused(); }
    const std::vector<std::uint8_t>& buildable_mask() const { return buildable_mask_; }
private:
    void reset_camera();
    void resize_camera();
    void place_demo();
    bool draw_diamond(scene::Point world, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool fill);
    bool draw_world();
    bool draw_hud();
    bool draw_hud_v2();
    int hud_height() const;
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
    simulation::RulesProfile rules_;
    int last_courier_draws_=0;
    std::filesystem::path data_root_,map_relative_,save_path_;
    std::optional<persistence::SaveDocument> initial_save_;
    std::vector<std::uint8_t> buildable_mask_;
    std::uint64_t io_generation_=0;
};

} // namespace openemperor
