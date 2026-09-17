#pragma once

#include "maps/TerrainRenderPlan.h"

#include <map>
#include <string>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

class TerrainPreviewRenderer {
public:
    TerrainPreviewRenderer(maps::TerrainRenderPlan plan, maps::TerrainBindings bindings);
    ~TerrainPreviewRenderer();
    TerrainPreviewRenderer(const TerrainPreviewRenderer&) = delete;
    TerrainPreviewRenderer& operator=(const TerrainPreviewRenderer&) = delete;
    void initialize(SDL_Renderer* renderer);
    void shutdown();
    bool render(const scene::Camera2D& camera, std::optional<maps::GridCell> selected);
    const maps::TerrainRenderPlan& plan() const { return plan_; }
    const maps::TerrainPreviewAsset& asset_for_alias(const std::string& alias) const {
        return bindings_.assets.at(alias);
    }
    std::size_t texture_count() const { return source_textures_.size(); }
    std::size_t upload_count() const { return upload_count_; }
    std::size_t last_drawn_instances() const { return last_drawn_instances_; }
private:
    bool draw_diamond(scene::Point world, const scene::Camera2D& camera, bool selected);
    maps::TerrainRenderPlan plan_;
    maps::TerrainBindings bindings_;
    SDL_Renderer* renderer_ = nullptr;
    std::map<std::string, SDL_Texture*> textures_;
    std::map<std::string, SDL_Texture*> source_textures_;
    std::size_t upload_count_ = 0;
    std::size_t last_drawn_instances_ = 0;
};

bool terrain_rect_visible(scene::Point image_origin, const scene::Camera2D& camera);

} // namespace openemperor
