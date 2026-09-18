#pragma once

#include "maps/StoredGraphicsPlan.h"
#include "scene/WorldDrawOrder.h"

#include <cstddef>
#include <optional>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

struct StoredDrawItem {
    scene::WorldDrawKey key;
    bool footprint = false;
    std::size_t plan_index = 0;
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
    bool draw_selection(const scene::Camera2D& camera,
                        std::optional<maps::GridCell> selected);
    const maps::StoredGraphicsPlan& plan() const { return plan_; }
    std::size_t upload_count() const { return plan_.texture_uploads; }
    std::size_t last_drawn_instances() const { return last_drawn_instances_; }
    std::size_t last_texture_draws() const { return last_texture_draws_; }
    std::size_t last_diagnostic_draws() const { return last_diagnostic_draws_; }
    std::size_t stored_order_builds() const { return stored_order_builds_; }
    static std::size_t live_texture_count(); // Textures owned by this renderer class.
private:
    bool draw_diagnostic(scene::Point world, const scene::Camera2D& camera, bool selected);
    maps::StoredGraphicsPlan plan_;
    SDL_Renderer* renderer_ = nullptr;
    std::vector<SDL_Texture*> textures_;
    std::vector<StoredDrawItem> draw_order_;
    std::size_t stored_order_builds_ = 0;
    std::size_t last_drawn_instances_ = 0;
    std::size_t last_texture_draws_ = 0;
    std::size_t last_diagnostic_draws_ = 0;
};

} // namespace openemperor
