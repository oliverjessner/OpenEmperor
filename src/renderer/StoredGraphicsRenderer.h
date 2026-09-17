#pragma once

#include "maps/StoredGraphicsPlan.h"

#include <cstddef>
#include <optional>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

class StoredGraphicsRenderer {
public:
    explicit StoredGraphicsRenderer(maps::StoredGraphicsPlan plan);
    ~StoredGraphicsRenderer();
    StoredGraphicsRenderer(const StoredGraphicsRenderer&) = delete;
    StoredGraphicsRenderer& operator=(const StoredGraphicsRenderer&) = delete;
    void initialize(SDL_Renderer* renderer);
    void shutdown();
    bool render(const scene::Camera2D& camera, std::optional<maps::GridCell> selected);
    const maps::StoredGraphicsPlan& plan() const { return plan_; }
    std::size_t upload_count() const { return plan_.texture_uploads; }
    std::size_t last_drawn_instances() const { return last_drawn_instances_; }
private:
    bool draw_diagnostic(scene::Point world, const scene::Camera2D& camera, bool selected);
    maps::StoredGraphicsPlan plan_;
    SDL_Renderer* renderer_ = nullptr;
    std::vector<SDL_Texture*> textures_;
    std::size_t last_drawn_instances_ = 0;
};

} // namespace openemperor
