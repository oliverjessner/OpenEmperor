#pragma once

#include "maps/StoredMapSession.h"
#include "maps/MapGeometry.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "renderer/AuthoredTerrainSides.h"
#include "renderer/RampRenderer.h"
#include "renderer/RampTopology.h"
#include "simulation/World.h"
#include "simulation/CityStartGuidance.h"
#include "simulation/RoadConnectivity.h"
#include "persistence/SandboxSave.h"
#include "app/SandboxUiLayout.h"
#include "app/OriginalHouseInspection.h"
#include "app/RoadDrag.h"
#include "app/RoadTopology.h"
#include "app/VisualSelection.h"
#include "assets/WalkerVisualProfile.h"
#include "renderer/WalkerSpriteSet.h"
#include "renderer/BuildingSprite.h"
#include "renderer/RoadSpriteSet.h"
#include "renderer/FireSpriteSet.h"
#include "scene/WorldDrawOrder.h"

#include <memory>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

namespace openemperor {

class SandboxView {
public:
    struct WalkerDisplayStats {
        struct Role {
            bool configured=false;
            std::array<bool,4> directions_configured{};
            std::array<bool,4> directions_drawn{};
            std::uint64_t draws=0;
            std::uint64_t fallback_unmapped=0,fallback_invalid_edge=0;
        };
        std::uint32_t schema_version=0;
        std::size_t frame_aliases=0;
        std::size_t frame_alias_limit=0;
        std::size_t logical_rgba_bytes=0;
        std::array<Role,assets::walker_visual_role_count> roles{};
        std::array<bool,4> configured{};
        std::array<bool,4> moving_drawn{};
        std::size_t decoded_assets=0;
        std::size_t texture_uploads=0;
        std::vector<assets::AssetId> decoded_frame_ids;
        std::uint64_t unmapped_fallbacks=0;
        std::uint64_t invalid_edge_fallbacks=0;
    };
    explicit SandboxView(maps::StoredMapSession session, bool demo,
                         simulation::RulesProfile rules=simulation::RulesProfile::LogisticsV1,
                         std::uint32_t rule_version=0);
    // Explicit optional original-economy entry. Older development rules remain
    // fixture-only; ordinary constructors never select original rules.
    static std::unique_ptr<SandboxView> create_original_production(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit Rule11 opt-in; Rule10 and all ordinary defaults remain unchanged.
    static std::unique_ptr<SandboxView> create_original_fire_protection(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit Rule12/schema28; no new-game default or saved-rule migration.
    static std::unique_ptr<SandboxView> create_original_sustainable_economy(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit Rule14/schema30 recipient water query; requires a bound map input.
    static std::unique_ptr<SandboxView> create_original_cardinal_water(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit authored Rule15/schema31; never selected by an inherited capability.
    static std::unique_ptr<SandboxView> create_original_inspector_coordination(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit authored Rule16/schema32; never selected by an inherited capability.
    static std::unique_ptr<SandboxView> create_original_deadline_fire_dispatch(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit authored Rule17/schema33; inherited fire behavior, new service selection.
    static std::unique_ptr<SandboxView> create_original_deadline_service_dispatch(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit authored Rule18/schema34; inherited fire behavior, inherited services, new Food selection.
    static std::unique_ptr<SandboxView> create_original_deadline_food_dispatch(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
    // Explicit authored Rule13/schema29 logistics, separate from Rule12 saves.
    static std::unique_ptr<SandboxView> create_original_market_logistics(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt,
        bool paid_starter=false);
#if defined(OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE)
    // Explicit development fixtures; no shipped application entry point.
    static std::unique_ptr<SandboxView> create_original_economy_save_fixture(
        maps::StoredMapSession session);
    static std::unique_ptr<SandboxView> create_original_influence_save_fixture(
        maps::StoredMapSession session);
    static std::unique_ptr<SandboxView> create_original_goods_save_fixture(
        maps::StoredMapSession session);
    static std::unique_ptr<SandboxView> create_original_production_save_fixture(
        maps::StoredMapSession session,
        std::optional<original::Difficulty> difficulty=std::nullopt);
#endif
    ~SandboxView();
    void initialize(SDL_Window* window, SDL_Renderer* renderer);
    void set_authored_terrain_sides(bool enabled) { authored_terrain_sides_enabled_=enabled; }
    std::size_t authored_terrain_side_count() const { return authored_terrain_sides_.size(); }
    std::size_t authored_terrain_open_edges() const { return authored_terrain_sides_.open_edges(); }
    std::size_t authored_terrain_geometry_bytes() const { return authored_terrain_sides_.logical_geometry_bytes(); }
    unsigned ramp_visual_adjacency(simulation::RampId id,bool committed=false) const {
        return committed?ramp_topology_.committed_mask(id):ramp_topology_.effective_mask(id);
    }
    std::size_t ramp_geometry_bytes() const { return ramp_presentation_.geometry_bytes(); }
    void shutdown();
    void handle_event(const SDL_Event& event, bool& running);
    void update(double frame_seconds);
    struct PanKeyState {
        bool left=false,right=false,up=false,down=false;
        bool a=false,d=false,w=false,s=false;
    };
    // Explicit held-device state for deterministic tests; the normal entry
    // point reads SDL's keyboard state and uses this same update path.
    void update(double frame_seconds, PanKeyState keys);
    struct InputDiagnosticState {
        std::optional<scene::Point> render_position;
        std::optional<sandbox_ui::Action> ui_action;
        std::optional<simulation::Cell> ground_cell;
        std::optional<simulation::Cell> selected_cell;
        bool selected_landscape=false;
        std::uint32_t selected_walker=0;
        bool ui=false,map_pressed=false,ui_pressed=false,road_drag=false,input_focused=true;
    };
    InputDiagnosticState input_diagnostic_state(
        std::optional<scene::Point> raw_window_position=std::nullopt) const;
    bool render();
    void tick_once(); // Used by the finite, offscreen compatibility check.
    simulation::CommandResult execute(simulation::Command command);
    simulation::CommandResult request_execute(simulation::Command command);
    bool budget_warning_pending() const { return budget_warning_.has_value(); }
    const std::optional<simulation::StarterBudgetWarning>& budget_warning() const {
        return budget_warning_;
    }
    bool resolve_budget_warning(bool build_anyway);
    bool demolition_pending() const { return pending_demolition_.has_value(); }
    bool resolve_demolition(bool confirm);
    const simulation::World& world() const { return *world_; }
    std::optional<simulation::Cell> pick(scene::Point screen) const;
    simulation::CommandResult preview(simulation::Cell cell) const;
    void set_tool(int tool);
    int tool() const { return tool_; }
    bool industry_page() const { return industry_page_; }
    bool industry_inspector_selected() const;
    const std::string& last_message() const { return last_message_; }
    std::optional<simulation::Cell> demo_origin() const { return demo_origin_; }
    scene::Camera2D camera() const { return camera_; }
    int last_courier_draws() const { return last_courier_draws_; }
    void configure_save(std::filesystem::path data_root,std::filesystem::path map_relative,
                        std::filesystem::path save_path,
                        std::optional<persistence::SaveDocument> initial=std::nullopt);
    void save_now();
    persistence::SaveDocument capture_save_document() const;
    bool recovery_safe_point() const;
    void set_recovery_status(std::string status) { recovery_status_=std::move(status); }
    void load_now();
    void set_walker_visuals(const std::filesystem::path& manifest,
                            VisualProfileSource source=VisualProfileSource::Custom);
    // Built-in optional tail; explicit custom walker profiles are never supplemented.
    void set_fire_inspector_visuals(const std::filesystem::path& manifest,
                                   VisualProfileSource source=VisualProfileSource::Builtin,
                                   std::string fallback_reason={});
    struct FireInspectorDisplayStats {
        bool configured=false,active=false;
        std::string clip_id,fallback_reason;
        std::size_t frames=0,additional_assets=0;
        std::uint64_t additional_bytes=0,draws=0,fallback_draws=0;
    };
    FireInspectorDisplayStats fire_inspector_display_stats() const;
    void set_market_walker_visuals(const std::filesystem::path& manifest,
                                  VisualProfileSource source=VisualProfileSource::Builtin,
                                  std::string fallback_reason={});
    struct MarketWalkerDisplayStats {
        bool configured=false,active=false;
        std::array<std::string,2> clip_ids;
        std::string fallback_reason;
        std::size_t frames=0,additional_assets=0;
        std::uint64_t additional_bytes=0,draws=0,fallback_draws=0;
    };
    MarketWalkerDisplayStats market_walker_display_stats() const;
    void set_service_walker_visuals(const std::filesystem::path& manifest,
                                   VisualProfileSource source=VisualProfileSource::Builtin,
                                   std::string fallback_reason={});
    struct ServiceWalkerDisplayStats {
        bool configured=false,active=false;
        std::string clip_id,fallback_reason;
        std::size_t frames=0,additional_assets=0;
        std::uint64_t additional_bytes=0,draws=0,fallback_draws=0;
    };
    ServiceWalkerDisplayStats service_walker_display_stats() const;
    void set_health_walker_visuals(const std::filesystem::path& manifest,
                                   VisualProfileSource source=VisualProfileSource::Builtin,
                                   std::string fallback_reason={});
    struct HealthWalkerDisplayStats {
        bool configured=false,active=false;
        std::string clip_id,fallback_reason;
        std::size_t frames=0,additional_assets=0;
        std::uint64_t additional_bytes=0,draws=0,fallback_draws=0;
    };
    HealthWalkerDisplayStats health_walker_display_stats() const;
    void set_building_visuals(const std::filesystem::path& manifest,
                              VisualProfileSource source=VisualProfileSource::Custom);
    void set_original_house_visuals(const std::filesystem::path& manifest,
                                   VisualProfileSource source=VisualProfileSource::Builtin,
                                   std::string fallback_reason={});
    struct OriginalHouseDisplayStats {
        bool configured=false,active=false;
        VisualProfileSource source=VisualProfileSource::Fallback;
        std::string fallback_reason,preparation_error;
        std::size_t additional_assets=0;
        std::uint64_t additional_bytes=0;
        std::array<bool,16> configured_types{};
        std::array<std::uint64_t,16> drawn_instances{},authored_fallbacks{};
    };
    OriginalHouseDisplayStats original_house_display_stats() const;
    struct OriginalHouseVisualDiagnostic {
        bool applicable=false,configured=false,active=false;
        std::uint16_t house_type=0;
        std::uint8_t actual_side=0;
        std::optional<assets::AssetId> asset;
        std::uint16_t image_width=0,image_height=0;
        double ground_x=0,ground_y=0;
        scene::Point ground{},image_origin{}; // Unscaled world coordinates.
        std::string selection,evidence,fallback_reason;
    };
    OriginalHouseVisualDiagnostic original_house_visual_diagnostic(simulation::BuildingId id) const;
    void set_road_visuals(const std::filesystem::path& manifest,
                          VisualProfileSource source=VisualProfileSource::Custom);
    // Optional presentation; an incomplete clip selects the complete marker fallback.
    void set_fire_visuals(const std::filesystem::path& manifest,
                         VisualProfileSource source=VisualProfileSource::Builtin,
                         std::string fallback_reason={});
    struct FireDisplayStats {
        bool animated=false;
        std::string clip_id,fallback_reason;
        std::size_t frames=0,unique_assets=0,texture_uploads=0;
        std::uint64_t logical_bytes=0,draws=0,fallback_draws=0;
        std::array<bool,assets::fire_frame_limit> frames_drawn{};
    };
    FireDisplayStats fire_display_stats() const;
    std::optional<std::size_t> fire_frame_for(simulation::BuildingId id) const;
    void set_compatibility(std::string id) { compatibility_id_=std::move(id); }
    VisualProfileSource walker_visual_source() const { return walker_source_; }
    VisualProfileSource building_visual_source() const { return building_source_; }
    VisualProfileSource road_visual_source() const { return road_source_; }
    const std::string& compatibility_id() const { return compatibility_id_; }
    bool debug_diagnostics() const { return debug_open_; }
    bool road_visuals_active() const { return road_enabled_ && road_profile_.has_value(); }
    struct RoadDisplayStats {
        bool configured=false;
        std::array<bool,16> configured_masks{},masks_seen{};
        std::size_t unique_assets=0,texture_uploads=0;
        std::uint64_t draws=0,fallback_draws=0;
    };
    RoadDisplayStats road_display_stats() const;
    struct PainterStats : scene::WorldMergeStats {
        std::size_t stored_order_builds=0,road_ground_items=0;
        bool road_ground_pass=false;
    };
    PainterStats painter_stats() const { return painter_stats_; }
    std::size_t original_road_ground_items() const { return background_.original_road_ground_items(); }
    struct RoadVisualDiagnostic {
        simulation::Cell cell{};
        bool original=false,road=false,supported=false,dynamic=false,preview=false;
        std::uint8_t mask=0,entrance=0,replacement=0;
        std::optional<std::uint8_t> historical_mask;
        bool stale_ground=false;
        std::optional<assets::AssetId> historical_asset,regenerated_ground,actual_asset;
        scene::Point image_origin{};
        std::string mode,fallback;
    };
    RoadVisualDiagnostic road_visual_diagnostic(simulation::Cell cell) const;
    bool unified_depth() const { return unified_depth_; }
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
    // Bounded read-only diagnosis of current Couriers and the last submitted
    // live frame. Exporting these rows is an external test/tool operation.
    struct WalkerDiagnostic {
        simulation::CourierId id{};
        simulation::CourierRole role{};
        simulation::BuildingId owner{},target{};
        simulation::CourierPhase phase{};
        simulation::Good good{};
        int cargo=0;
        bool live_visible=false,submitted=false,sprite_drawn=false,route_pending=false;
        bool flip_x=false,marker_comparison=false;
        std::optional<simulation::Cell> edge_from,edge_to;
        std::optional<assets::StorageDirection> direction;
        std::string family,profile_source,clip_id,status;
        std::optional<assets::AssetId> native_asset;
        scene::Point screen_ground{},image_origin{};
        double image_width=0,image_height=0;
    };
    std::vector<WalkerDiagnostic> walker_diagnostics() const;
    std::uint64_t io_generation() const { return io_generation_; }
    std::uint64_t save_generation() const { return save_generation_; }
    std::uint64_t load_generation() const { return load_generation_; }
    bool dirty() const { return world_ && (world_->ticks()!=saved_tick_ ||
        world_->command_sequence()!=saved_command_); }
    void set_managed(bool managed) { managed_=managed; }
    bool take_menu_request() { const bool value=menu_requested_; menu_requested_=false; return value; }
    const std::filesystem::path& save_path() const { return save_path_; }
    bool paused() const { return clock_.paused(); }
    bool help_open() const { return help_open_; }
    bool income_open() const { return income_open_; }
    bool water_overlay() const { return water_overlay_; }
    bool health_overlay() const { return health_overlay_; }
    bool service_overlay() const { return service_overlay_; }
    std::optional<bool> predicted_water() const { return predicted_water_; }
    std::optional<simulation::World::WellCoverage> predicted_well_coverage() const { return predicted_well_coverage_; }
    std::uint64_t water_preview_build_count() const { return water_preview_build_count_; }
    bool desirability_overlay() const { return desirability_overlay_; }
    std::optional<int> predicted_desirability() const { return predicted_desirability_; }
    std::uint64_t desirability_preview_build_count() const { return desirability_preview_build_count_; }
    const sandbox_ui::Layout& layout() const { return layout_; }
    std::optional<simulation::BuildingId> selected_building() const;
    sandbox_ui::Rect diagnostic_operation_toggle_rect() const { return operation_toggle_rect(); }
    sandbox_ui::InspectorLayout inspector_layout() const;
    int inspector_scroll() const { return inspector_layout().scroll; }
    int inspector_hint_scroll() const { return inspector_layout().hint_scroll; }
    std::optional<simulation::Cell> hovered_cell() const { return hovered_; }
    std::vector<std::string> inspection_lines() const;
    using OriginalHouseSection=original_house_ui::Section;
    using OriginalHouseOverlay=original_house_ui::Overlay;
    // Rule10 presentation state only; neither choice enters a save.
    bool original_house_inspector_active() const;
    OriginalHouseSection original_house_section() const { return original_house_section_; }
    OriginalHouseOverlay original_house_overlay() const { return original_house_overlay_; }
    std::vector<std::string> original_house_section_lines() const;
    sandbox_ui::Rect original_house_section_rect(OriginalHouseSection section) const;
    sandbox_ui::Rect original_house_overlay_rect() const;
    sandbox_ui::Rect original_house_content_rect() const;
    int original_house_scroll() const { return original_house_scroll_; }
    std::string original_house_overlay_legend() const;
    struct OriginalHouseOverlayRecord {
        simulation::BuildingId building_id=static_cast<simulation::BuildingId>(0);
        simulation::Cell origin{};
        simulation::BuildingFootprint footprint{};
        original_house_ui::Status status=original_house_ui::Status::Unknown;
        char symbol='?';
        int value=0;
        scene::Point screen_ground{};
        std::size_t visible_cells=0;
        bool marker_visible=false;
    };
    struct OriginalHouseOverlayStats { std::size_t houses=0,cells=0,markers=0; };
    OriginalHouseOverlayStats original_house_overlay_stats() const { return original_house_overlay_stats_; }
    std::span<const OriginalHouseOverlayRecord> original_house_overlay_records() const {
        return std::span<const OriginalHouseOverlayRecord>(original_house_overlay_records_).first(
            original_house_overlay_stats_.houses);
    }
    // Explicit F1/G diagnosis only. Inspecting this cache never runs routing.
    const simulation::RoadConnectivityReport* road_connectivity_report() const;
    std::uint64_t road_connectivity_check_count() const { return road_connectivity_check_count_; }
    std::vector<std::string> road_connectivity_lines() const;
    std::vector<std::string> income_summary_lines() const;
    std::vector<std::string> income_lines() const;
    // Same read-only status text submitted by draw_hud; no cached authority.
    std::string hud_status_line() const;
    std::vector<std::string> budget_warning_lines() const;
    std::vector<std::string> demolition_hint_lines() const;
    const sandbox_ui::RoadPlan& road_preview() const { return road_preview_; }
    std::uint64_t road_plan_build_count() const { return road_plan_build_count_; }
    const std::vector<std::uint8_t>& buildable_mask() const { return buildable_mask_; }
private:
    SandboxView(maps::StoredMapSession session,bool demo,simulation::RulesProfile rules,
                std::uint32_t rule_version,bool original_save_fixture);
    const assets::BuildingVisualEntry* building_entry(assets::BuildingVisualRole role) const;
    std::optional<assets::BuildingVisualRole> visual_role(simulation::Cell cell,
        simulation::Object object,bool placement_preview=false) const;
    bool fire_watch_selected() const;
    bool status_first_selected() const;
    std::vector<std::string> wrap_panel_lines(const std::vector<std::string>& lines) const;
    void select_original_house_section(OriginalHouseSection section);
    void cycle_original_house_overlay();
    int original_house_scroll_limit() const;
    bool draw_original_house_panel();
    bool draw_original_house_overlay();
    bool draw_original_house_overlay_legend();
    void request_road_connectivity_diagnosis();
    enum class OperationAction { Toggle, PriorityHigh, PriorityNormal, PriorityLow };
    struct DrawInstance {
        scene::WorldDrawKey key;
        simulation::Cell cell{};
        simulation::Object object=simulation::Object::Empty;
        simulation::CourierId courier=simulation::CourierId::Clay;
        simulation::Position position{};
        bool placement_preview=false;
        const simulation::BridgeSpan* bridge=nullptr;
        bool bridge_front_rail=false;
        const simulation::RampLink* ramp=nullptr;
        int ramp_half=0;
        bool ramp_front=false;
        unsigned ramp_adjacency=0;
    };
    // Descriptors of the dynamic pixels actually submitted to the painter.
    // Image pixels remain in the eagerly loaded profiles; no hit mask is copied.
    struct VisualHit {
        scene::WorldDrawKey key;
        simulation::Cell cell{};
        std::optional<simulation::CourierId> walker;
        std::optional<simulation::BridgeId> bridge;
        bool bridge_front_rail=false;
        std::optional<simulation::RampId> ramp;
        int ramp_half=0;
        bool ramp_front=false;
        unsigned ramp_adjacency=0;
        std::optional<std::size_t> image;
        scene::Point origin;
        double width=0,height=0;
        bool flip_x=false; // Display transform; alpha is sampled from the native image.
        bool diamond=false;
        simulation::Object mesh=simulation::Object::Empty;
        scene::Point mesh_ground;
        int footprint_side=1;
    };
    void record_visual_hit(const DrawInstance& instance);
    bool select_visual(scene::Point screen);
    void clear_visual_selection();
    void clear_hidden_walker_selection();
    bool pause_for_original_fire_limit(const char* reason);
    void reset_camera();
    void update_window_title();
    void resize_camera();
    void place_demo();
    simulation::Command placement_command(simulation::Cell cell) const;
    bool draw_diamond(scene::Point world, std::uint8_t r, std::uint8_t g, std::uint8_t b, bool fill, float alpha=0.65F);
    bool draw_world(const scene::Camera2D& render_camera);
    bool draw_walker_diagnostic();
    std::string walker_clip_status(const simulation::CourierState& courier) const;
    bool draw_hud();
    bool draw_help_overlay();
    bool draw_budget_warning_overlay();
    bool draw_demolition_overlay();
    sandbox_ui::Rect demolition_button_rect() const;
    void request_demolition();
    bool draw_text(double x,double y,const std::string& text,int max_width);
    bool action_enabled(sandbox_ui::Action action) const;
    void perform_action(sandbox_ui::Action action);
    void cancel_gesture();
    void refresh_hover(bool force_road_plan=false);
    void invalidate_road_preview_cache();
    bool request_road(const sandbox_ui::RoadPlan& plan);
    sandbox_ui::Rect budget_build_rect() const;
    sandbox_ui::Rect budget_cancel_rect() const;
    sandbox_ui::Rect budget_panel_rect() const;
    sandbox_ui::Rect menu_button_rect() const;
    std::optional<OperationAction> operation_action_at(double x,double y) const;
    sandbox_ui::Rect operation_toggle_rect() const;
    sandbox_ui::Rect operation_priority_rect(int index) const;
    void perform_operation_action(OperationAction action);
    std::optional<scene::Point> render_point(float x,float y) const;
    void update_layout(bool preserve_center);
    std::vector<simulation::BuildingId> placed_buildings() const;
    scene::Point world_for(simulation::Position cell) const;
    scene::Point building_visual_ground(simulation::Cell origin,
                                                  simulation::Object kind) const;
    int walker_marker_shift(simulation::Position position,simulation::CourierId id) const;
    std::optional<std::uint64_t> session_texture_headroom(bool include_walkers,
                                                        bool include_fire,
                                                        bool include_buildings=true) const;
    struct OriginalHouseVisualSelection {
        bool applicable=false;
        std::uint16_t house_type=0;
        std::uint8_t actual_side=0;
        const assets::BuildingVisualEntry* entry=nullptr;
    };
    OriginalHouseVisualSelection original_house_visual_selection(simulation::Cell cell,
        simulation::Object kind,bool placement_preview=false) const;
    assets::BuildingVisualProfile core_building_visual_profile() const;
    bool append_requested_original_house_visuals(assets::BuildingVisualProfile& profile,
        VisualProfileSource core_source,const std::filesystem::path& manifest,
        VisualProfileSource source,std::string& reason,std::string& error) const;
    void publish_building_visual_profile(assets::BuildingVisualProfile profile);
    void prepare_original_house_visuals();
    void remove_fire_inspector_extension();
    void prepare_fire_inspector_extension();
    void remove_market_walker_extension();
    void prepare_market_walker_extension();
    void remove_service_walker_extension();
    void prepare_service_walker_extension();
    void remove_health_walker_extension();
    void prepare_health_walker_extension();
    void remove_walker_roles(const std::array<bool,assets::walker_visual_role_count>& removed);
    std::pair<std::size_t,std::uint64_t> exclusive_walker_images(
        const std::array<bool,assets::walker_visual_role_count>& selected) const;
    void enforce_fire_texture_budget();
    maps::MapGeometry geometry_;
    StoredGraphicsRenderer background_;
    std::unique_ptr<simulation::World> world_;
    simulation::TickDriver clock_;
    // Start-time selection only. The saved ModelSource remains sole authority.
    std::optional<original::Difficulty> original_requested_difficulty_;
    bool original_paid_starter_=false;
    scene::Camera2D camera_;
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
    std::optional<simulation::Cell> hovered_;
    std::optional<simulation::Cell> selected_;
    bool selected_landscape_=false;
    std::optional<simulation::CourierId> selected_walker_;
    std::optional<simulation::RoadConnectivityReport> road_connectivity_report_;
    const simulation::World* road_connectivity_world_=nullptr;
    std::optional<simulation::CourierId> road_connectivity_courier_;
    bool road_connectivity_target_candidate_=true;
    std::uint64_t road_connectivity_check_count_=0;
    std::optional<simulation::Cell> demo_origin_;
    sandbox_ui::Layout layout_;
    sandbox_ui::RoadPlan road_preview_;
    // Render-only indexed projection: 0 additive, 1 opaque replacement, 2 alpha preview.
    std::vector<std::uint8_t> road_ground_replacements_;
    struct OriginalRoadVisual {
        simulation::Cell cell{};
        std::size_t renderer_index=0;
        std::optional<std::uint8_t> historical_mask;
        bool stale_ground=false;
        std::uint8_t mask=0,entrance=0,preview_mask=0;
    };
    // Load-time singleton index and small revision/preview caches. No map scan
    // or asset work is added to hover/render; only changed road/building rims
    // invalidate their adjacent original roads.
    std::vector<OriginalRoadVisual> original_road_visuals_;
    std::vector<std::size_t> preview_original_roads_;
    std::uint64_t original_road_visual_revision_=UINT64_MAX;
    std::uint64_t original_road_preview_build_=UINT64_MAX;
    bool original_road_preview_valid_=false;
    void prepare_original_road_visuals();
    void refresh_original_road_visuals(std::span<const simulation::Cell> changed={});
    void refresh_original_road_preview();
    std::optional<std::size_t> original_road_visual_index(simulation::Cell cell) const;
    bool original_road_dynamic(const OriginalRoadVisual& road) const;
    std::optional<simulation::Cell> road_start_;
    bool authored_terrain_sides_enabled_=false; // Explicit preview; absent from saves/settings.
    AuthoredTerrainSides authored_terrain_sides_;
    RampTopology ramp_topology_;
    RampPresentation ramp_presentation_;
    void refresh_ramp_visual_topology();
    std::optional<simulation::Cell> ramp_start_;
    simulation::RampValidation ramp_preview_{};
    std::optional<simulation::Cell> bridge_start_;
    simulation::BridgeValidation bridge_preview_{};
    std::optional<sandbox_ui::Action> pressed_button_;
    std::optional<simulation::BuildingId> pressed_building_;
    bool ui_pressed_=false;
    bool map_pressed_=false;
    bool input_focused_=true;
    bool panel_open_=true;
    bool debug_open_=false;
    bool help_open_=false;
    bool income_open_=false;
    OriginalHouseSection original_house_section_=OriginalHouseSection::State;
    OriginalHouseOverlay original_house_overlay_=OriginalHouseOverlay::Off;
    int original_house_scroll_=0;
    std::optional<OriginalHouseSection> pressed_original_house_section_;
    bool pressed_original_house_overlay_=false;
    bool original_house_panel_pressed_=false;
    std::array<OriginalHouseOverlayRecord,simulation::Rules::city_v10_household_limit> original_house_overlay_records_{};
    OriginalHouseOverlayStats original_house_overlay_stats_;
    bool water_overlay_=false;
    bool health_overlay_=false;
    bool service_overlay_=false;
    std::optional<bool> predicted_water_;
    std::optional<simulation::World::WellCoverage> predicted_well_coverage_;
    std::optional<simulation::Cell> water_preview_cell_;
    std::uint64_t water_preview_revision_=UINT64_MAX;
    std::uint64_t water_preview_build_count_=0;
    int water_preview_tool_=-1;
    bool desirability_overlay_=false;
    std::optional<int> predicted_desirability_;
    std::optional<simulation::Cell> desirability_preview_cell_;
    std::uint64_t desirability_preview_revision_=UINT64_MAX;
    std::uint64_t desirability_preview_build_count_=0;
    int panel_scroll_=0;
    int inspector_hint_scroll_=0;
    int income_scroll_=0;
    std::optional<scene::Point> pointer_;
    bool hover_dirty_=true;
    std::optional<simulation::Cell> planned_start_,planned_end_;
    std::uint64_t planned_road_revision_=UINT64_MAX,planned_command_sequence_=UINT64_MAX;
    std::int64_t planned_treasury_=INT64_MIN;
    std::uint64_t road_plan_build_count_=0;
    std::string last_message_;
    std::string recovery_status_;
    bool demo_=false;
    int tool_=4;
    bool industry_page_=false; // View-local page choice, never World/save authority.
    simulation::RulesProfile rules_;
    std::uint32_t requested_rule_version_=0;
    bool original_influence_fixture() const {
        return original_save_fixture_ && simulation::original_influence_enabled(rules_,requested_rule_version_);
    }
    bool original_goods_fixture() const {
        return original_save_fixture_ && simulation::original_goods_enabled(rules_,requested_rule_version_);
    }
    bool original_production_fixture() const {
        return original_save_fixture_ && simulation::original_production_enabled(rules_,requested_rule_version_);
    }
    bool original_fire_protection() const {
        return simulation::original_fire_protection_enabled(rules_,requested_rule_version_);
    }
    bool original_sustainable_economy() const {
        return simulation::original_sustainable_economy_enabled(rules_,requested_rule_version_);
    }
    bool original_cardinal_water() const {
        return simulation::original_cardinal_water_enabled(rules_,requested_rule_version_);
    }
    bool original_deadline_service_dispatch() const {
        return simulation::original_deadline_service_dispatch_enabled(rules_,requested_rule_version_);
    }
    bool original_deadline_food_dispatch() const {
        return simulation::original_deadline_food_dispatch_enabled(rules_,requested_rule_version_);
    }
    bool original_deadline_fire_dispatch() const {
        return simulation::original_deadline_fire_dispatch_enabled(rules_,requested_rule_version_);
    }
    bool original_inspector_coordination() const {
        return simulation::original_inspector_coordination_enabled(rules_,requested_rule_version_);
    }
    bool original_market_logistics() const {
        return simulation::original_market_logistics_enabled(rules_,requested_rule_version_);
    }
    bool original_save_fixture_=false; // Explicit original entry/legacy fixture; never saved authority.
    // Retained only through initialization, when immutable map permissions are
    // prepared. Rendering and navigation never consult the original raw map.
    maps::ParsedEmperorMap original_map_;
    std::string expected_map_input_sha256_; // Session-start binding; never a save or policy field.
    int last_courier_draws_=0;
    std::filesystem::path data_root_,map_relative_,save_path_;
    std::filesystem::path walker_manifest_;
    VisualProfileSource walker_source_=VisualProfileSource::Fallback;
    std::optional<assets::WalkerVisualProfile> walker_profile_;
    std::unique_ptr<WalkerSpriteSet> walker_sprites_;
    bool walker_visuals_enabled_=true;
    bool walker_diagnostic_open_=false, walker_diagnostic_zoom4_=true;
    bool walker_diagnostic_light_=false;
    std::size_t walker_diagnostic_role_=0,walker_diagnostic_direction_=0,
                walker_diagnostic_step_=0;
    std::array<WalkerDisplayStats::Role,assets::walker_visual_role_count> walker_role_stats_{};
    std::filesystem::path fire_inspector_manifest_;
    VisualProfileSource fire_inspector_source_=VisualProfileSource::Fallback;
    std::string fire_inspector_fallback_reason_="No Inspector clip selected";
    std::uint32_t walker_core_schema_=0;
    bool fire_inspector_extension_=false,market_walker_extension_=false;
    std::uint64_t fire_inspector_fallback_draws_=0,market_walker_fallback_draws_=0;
    std::filesystem::path market_walker_manifest_;
    VisualProfileSource market_walker_source_=VisualProfileSource::Fallback;
    std::string market_walker_fallback_reason_="No Food/Market clips selected";
    std::filesystem::path service_walker_manifest_;
    VisualProfileSource service_walker_source_=VisualProfileSource::Fallback;
    std::string service_walker_fallback_reason_="No Service clip selected";
    bool service_walker_extension_=false;
    std::uint64_t service_walker_fallback_draws_=0;
    std::filesystem::path health_walker_manifest_;
    VisualProfileSource health_walker_source_=VisualProfileSource::Fallback;
    std::string health_walker_fallback_reason_="No HealthWorker clip selected";
    bool health_walker_extension_=false;
    std::uint64_t health_walker_fallback_draws_=0;
    std::array<bool,4> walker_moving_drawn_{};
    std::uint64_t walker_unmapped_fallbacks_=0, walker_invalid_edge_fallbacks_=0;
    std::filesystem::path building_manifest_;
    VisualProfileSource building_source_=VisualProfileSource::Fallback;
    std::optional<assets::BuildingVisualProfile> building_profile_;
    std::unique_ptr<BuildingSprite> building_sprite_;
    std::size_t building_core_image_count_=0;
    std::filesystem::path original_house_manifest_;
    VisualProfileSource original_house_source_=VisualProfileSource::Fallback;
    std::string original_house_fallback_reason_="No original House profile selected";
    std::string original_house_preparation_error_;
    std::array<std::uint64_t,16> original_house_drawn_instances_{},original_house_authored_fallbacks_{};
    bool building_enabled_=true;
    std::array<std::uint64_t,assets::building_role_count> building_drawn_instances_{};
    std::array<std::uint64_t,assets::building_role_count> building_placeholder_fallbacks_{};
    std::filesystem::path road_manifest_;
    VisualProfileSource road_source_=VisualProfileSource::Fallback;
    std::optional<assets::RoadVisualProfile> road_profile_;
    std::unique_ptr<RoadSpriteSet> road_sprites_;
    std::filesystem::path fire_manifest_;
    VisualProfileSource fire_source_=VisualProfileSource::Fallback;
    std::optional<assets::FireVisualProfile> fire_profile_;
    std::unique_ptr<FireSpriteSet> fire_sprites_;
    std::string fire_fallback_reason_="No fire clip selected";
    std::array<bool,assets::fire_frame_limit> fire_frames_drawn_{};
    std::uint64_t fire_draws_=0,fire_fallback_draws_=0;
    bool road_enabled_=true;
    std::array<bool,16> road_masks_seen_{};
    std::uint64_t road_draws_=0,road_fallbacks_current_=0;
    bool unified_depth_=true;
    std::string compatibility_id_="unknown";
    PainterStats painter_stats_{};
    std::optional<persistence::SaveDocument> initial_save_;
    std::vector<std::uint8_t> buildable_mask_;
    std::uint64_t io_generation_=0;
    std::uint64_t load_generation_=0; // Successful active-original loads; view-local only.
    std::uint64_t save_generation_=0, saved_tick_=0, saved_command_=0;
    bool managed_=false, menu_requested_=false, menu_pressed_=false;
    std::optional<simulation::BuildingId> pending_demolition_;
    std::optional<bool> demolition_button_pressed_;
    bool pressed_demolition_=false;
    std::optional<simulation::StarterBudgetWarning> budget_warning_;
    std::optional<simulation::Command> pending_command_;
    std::optional<sandbox_ui::RoadPlan> pending_road_;
    std::optional<bool> budget_button_pressed_;
    std::optional<OperationAction> pressed_operation_action_;
    // Reused by draw_world(); its capacity remains bounded by the fixed World grid.
    std::vector<DrawInstance> draw_instances_;
    std::vector<VisualHit> visual_hits_;
    bool visual_frame_valid_=false,visual_hit_unified_=true;
    scene::Camera2D visual_hit_camera_;
    LandscapeDebugMode visual_hit_mode_=LandscapeDebugMode::Snapshot;
};

} // namespace openemperor
