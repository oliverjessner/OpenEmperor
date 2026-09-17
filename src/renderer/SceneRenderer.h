#pragma once

#include "scene/Scene.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

class SceneRenderer {
public:
    explicit SceneRenderer(scene::Scene scene);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    void initialize(SDL_Renderer* renderer);
    void shutdown();
    bool render(const scene::Camera2D& camera, bool grid,
                std::optional<scene::Cell> selected);
    const scene::Scene& scene_data() const { return scene_; }
    std::size_t texture_count() const { return source_textures_.size(); }
    std::size_t last_drawn_instances() const { return last_drawn_instances_; }
private:
    bool draw_asset(const std::string& alias, scene::Point tile, const scene::Camera2D& camera);
    bool draw_diamond(scene::Cell cell, const scene::Camera2D& camera);
    scene::Scene scene_;
    SDL_Renderer* renderer_ = nullptr;
    std::map<std::string, SDL_Texture*> textures_;
    std::map<std::string, SDL_Texture*> source_textures_;
    std::size_t last_drawn_instances_ = 0;
};

} // namespace openemperor
