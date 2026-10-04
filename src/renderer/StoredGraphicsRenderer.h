#pragma once

#include "maps/StoredGraphicsPlan.h"
#include "maps/TerrainInterpretation.h"
#include "scene/WorldDrawOrder.h"

#include <cstddef>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {
namespace maps { struct RegeneratedLandscapeInstance; }

std::array<std::uint8_t,3> stored_presentation_fallback_color(
    maps::TerrainCategory category);

enum class LandscapeDebugMode { Ground, Water, Elevation, MountainsRocks, WallsMonuments, Decorations, Regenerated, Snapshot };
const char* landscape_debug_mode_name(LandscapeDebugMode mode);

struct StoredDrawItem {
    scene::WorldDrawKey key;
    bool footprint = false;
    std::size_t plan_index = 0;
    bool regenerated = false;
};

class StoredGraphicsRenderer {
public:
    explicit StoredGraphicsRenderer(maps::StoredGraphicsPlan plan);
    ~StoredGraphicsRenderer();
    StoredGraphicsRenderer(const StoredGraphicsRenderer&) = delete;
    StoredGraphicsRenderer& operator=(const StoredGraphicsRenderer&) = delete;
    void initialize(SDL_Renderer* renderer);
    void shutdown();
    bool render(const scene::Camera2D& camera, std::optional<maps::GridCell> selected);
    const std::vector<StoredDrawItem>& draw_items() const { return draw_order_; }
    void begin_frame();
    bool draw_item(std::size_t renderer_index, const scene::Camera2D& camera);
    bool draw_ground_item(std::size_t renderer_index, const scene::Camera2D& camera);
    void set_landscape_mode(LandscapeDebugMode mode);
    LandscapeDebugMode landscape_mode() const { return landscape_mode_; }
    bool elevated() const { return landscape_mode_!=LandscapeDebugMode::Snapshot && plan_.landscape_layers_available; }
    std::optional<maps::GridCell> hit_test(scene::Point screen, const scene::Camera2D& camera) const;

    bool draw_selection(const scene::Camera2D& camera,
                        std::optional<maps::GridCell> selected);
    const maps::StoredGraphicsPlan& plan() const { return plan_; }
    std::size_t upload_count() const { return plan_.texture_uploads; }
    std::size_t last_drawn_instances() const { return last_drawn_instances_; }
    std::size_t last_texture_draws() const { return last_texture_draws_; }
    std::size_t last_diagnostic_draws() const { return last_diagnostic_draws_; }
    std::size_t stored_order_builds() const { return stored_order_builds_; }
    void set_debug_diagnostics(bool enabled) { debug_diagnostics_=enabled; }
    bool debug_diagnostics() const { return debug_diagnostics_; }
    static std::size_t live_texture_count(); // Textures owned by this renderer class.
private:
    bool draw_diagnostic(scene::Point world, const scene::Camera2D& camera, bool selected,
                         maps::TerrainCategory category=maps::TerrainCategory::Unknown);
    bool draw_component(std::size_t index, const scene::Camera2D& camera, bool base);
    scene::Point image_origin(const maps::PlacedFootprint& footprint) const;
    std::size_t render_asset_index(const maps::PlacedFootprint& footprint) const;
    bool overlay_visible(const maps::PlacedFootprint& footprint) const;
    bool regenerated_visible(std::size_t instance) const;
    scene::Point regenerated_image_origin(const maps::RegeneratedLandscapeInstance& instance) const;
    bool regenerated_placement_supported(const maps::RegeneratedLandscapeInstance& instance) const;
    LandscapeDebugMode landscape_mode_=LandscapeDebugMode::Snapshot;
    std::vector<SDL_Texture*> base_textures_, overlay_textures_;
    std::vector<std::vector<std::uint8_t>> snapshot_alpha_, overlay_alpha_;
    maps::StoredGraphicsPlan plan_;
    SDL_Renderer* renderer_ = nullptr;
    std::vector<SDL_Texture*> textures_;
    std::vector<StoredDrawItem> draw_order_;
    std::vector<bool> regenerated_ready_, suppressed_footprints_, suppressed_cells_;
    std::size_t stored_order_builds_ = 0;
    std::size_t last_drawn_instances_ = 0;
    std::size_t last_texture_draws_ = 0;
    std::size_t last_diagnostic_draws_ = 0;
    bool debug_diagnostics_ = true;
};

} // namespace openemperor
