#pragma once

#include "maps/StoredMapSession.h"
#include "maps/MapGeometry.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "simulation/World.h"
#include "persistence/SandboxSave.h"
#include "app/SandboxUiLayout.h"
#include "app/RoadDrag.h"
#include "app/RoadTopology.h"
#include "assets/WalkerVisualProfile.h"
#include "renderer/WalkerSpriteSet.h"
#include "renderer/BuildingSprite.h"
#include "renderer/RoadSpriteSet.h"

#include <memory>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

namespace openemperor {

class SandboxView {
public:
    struct WalkerDisplayStats {
        std::array<bool,4> configured{};
        std::array<bool,4> moving_drawn{};
        std::size_t decoded_assets=0;
        std::size_t texture_uploads=0;
        std::vector<assets::AssetId> decoded_frame_ids;
        std::uint64_t unmapped_fallbacks=0;
        std::uint64_t invalid_edge_fallbacks=0;
    };
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
    void set_walker_visuals(const std::filesystem::path& manifest);
    void set_building_visuals(const std::filesystem::path& manifest);
    void set_road_visuals(const std::filesystem::path& manifest);
    bool road_visuals_active() const { return road_enabled_ && road_profile_.has_value(); }
    struct RoadDisplayStats {
        bool configured=false;
        std::array<bool,16> configured_masks{},masks_seen{};
        std::size_t unique_assets=0,texture_uploads=0;
        std::uint64_t draws=0,fallback_draws=0;
    };
    RoadDisplayStats road_display_stats() const;
    bool building_visuals_active() const { return building_enabled_ && building_profile_.has_value(); }
    std::size_t building_texture_count() const { return building_sprite_ ? building_sprite_->texture_count():0; }
    struct BuildingDisplayStats {
        bool configured=false;
        std::size_t decoded_assets=0,texture_uploads=0;
        std::array<bool,assets::building_role_count> configured_roles{};
        std::array<std::uint64_t,assets::building_role_count> drawn_instances{};
        std::array<std::uint64_t,assets::building_role_count> placeholder_fallbacks{};
    };
    BuildingDisplayStats building_display_stats() const;
    bool walker_visuals_active() const { return walker_visuals_enabled_ && walker_profile_.has_value(); }
    std::size_t walker_texture_count() const { return walker_sprites_ ? walker_sprites_->texture_count():0; }
    WalkerDisplayStats walker_display_stats() const;
    std::uint64_t io_generation() const { return io_generation_; }
    std::uint64_t save_generation() const { return save_generation_; }
    bool dirty() const { return world_ && (world_->ticks()!=saved_tick_ ||
        world_->command_sequence()!=saved_command_); }
    void set_managed(bool managed) { managed_=managed; }
    bool take_menu_request() { const bool value=menu_requested_; menu_requested_=false; return value; }
    const std::filesystem::path& save_path() const { return save_path_; }
    bool paused() const { return clock_.paused(); }
    const sandbox_ui::Layout& layout() const { return layout_; }
    std::optional<simulation::BuildingId> selected_building() const;
    std::optional<simulation::Cell> hovered_cell() const { return hovered_; }
    std::vector<std::string> inspection_lines() const;
    const sandbox_ui::RoadPlan& road_preview() const { return road_preview_; }
    const std::vector<std::uint8_t>& buildable_mask() const { return buildable_mask_; }
private:
    void reset_camera();
    void resize_camera();
    void place_demo();
    bool draw_diamond(scene::Point world, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool fill);
    bool draw_world();
    bool draw_walker_diagnostic();
    bool draw_hud();
    bool draw_text(double x,double y,const std::string& text,int max_width);
    bool action_enabled(sandbox_ui::Action action) const;
    void perform_action(sandbox_ui::Action action);
    void cancel_gesture();
    void refresh_hover();
    std::optional<scene::Point> render_point(float x,float y) const;
    void update_layout(bool preserve_center);
    std::vector<simulation::BuildingId> placed_buildings() const;
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
    sandbox_ui::Layout layout_;
    sandbox_ui::RoadPlan road_preview_;
    std::optional<simulation::Cell> road_start_;
    std::optional<sandbox_ui::Action> pressed_button_;
    std::optional<simulation::BuildingId> pressed_building_;
    bool ui_pressed_=false;
    bool map_pressed_=false;
    bool panel_open_=true;
    bool debug_open_=false;
    int panel_scroll_=0;
    std::optional<scene::Point> pointer_;
    std::string last_message_;
    bool demo_=false;
    int tool_=4;
    simulation::RulesProfile rules_;
    int last_courier_draws_=0;
    std::filesystem::path data_root_,map_relative_,save_path_;
    std::filesystem::path walker_manifest_;
    std::optional<assets::WalkerVisualProfile> walker_profile_;
    std::unique_ptr<WalkerSpriteSet> walker_sprites_;
    bool walker_visuals_enabled_=true;
    bool walker_diagnostic_open_=false, walker_diagnostic_zoom4_=true;
    bool walker_diagnostic_light_=false;
    std::size_t walker_diagnostic_direction_=0, walker_diagnostic_step_=0;
    std::array<bool,4> walker_moving_drawn_{};
    std::uint64_t walker_unmapped_fallbacks_=0, walker_invalid_edge_fallbacks_=0;
    std::filesystem::path building_manifest_;
    std::optional<assets::BuildingVisualProfile> building_profile_;
    std::unique_ptr<BuildingSprite> building_sprite_;
    bool building_enabled_=true;
    std::array<std::uint64_t,assets::building_role_count> building_drawn_instances_{};
    std::array<std::uint64_t,assets::building_role_count> building_placeholder_fallbacks_{};
    std::filesystem::path road_manifest_;
    std::optional<assets::RoadVisualProfile> road_profile_;
    std::unique_ptr<RoadSpriteSet> road_sprites_;
    bool road_enabled_=true;
    std::array<bool,16> road_masks_seen_{};
    std::uint64_t road_draws_=0,road_fallbacks_current_=0;
    std::optional<persistence::SaveDocument> initial_save_;
    std::vector<std::uint8_t> buildable_mask_;
    std::uint64_t io_generation_=0;
    std::uint64_t save_generation_=0, saved_tick_=0, saved_command_=0;
    bool managed_=false, menu_requested_=false, menu_pressed_=false;
};

} // namespace openemperor
