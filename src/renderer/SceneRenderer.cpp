#include "renderer/SceneRenderer.h"

#include "assets/Sg3ImageLoader.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>

namespace openemperor {

SceneRenderer::SceneRenderer(scene::Scene scene) : scene_(std::move(scene)) {}
SceneRenderer::~SceneRenderer() { shutdown(); }

void SceneRenderer::initialize(SDL_Renderer* renderer) {
    renderer_ = renderer;
    std::uint64_t total_bytes = 0;
    std::set<std::string> used{scene_.default_terrain};
    for (const auto& over : scene_.terrain_overrides) used.insert(over.asset);
    for (const auto& object : scene_.objects) used.insert(object.asset);
    for (const auto& [alias, asset] : scene_.assets) {
        if (!used.contains(alias)) continue;
        const std::string id = asset.id.archive_relative_path.generic_string() + "#" +
            std::to_string(asset.id.image_index);
        if (const auto existing = source_textures_.find(id); existing != source_textures_.end()) {
            textures_.emplace(alias, existing->second);
            continue;
        }
        const std::uint64_t bytes = static_cast<std::uint64_t>(asset.width) *
            static_cast<std::uint64_t>(asset.height) * 4U;
        if (bytes > 16U * 1024U * 1024U || total_bytes > 64U * 1024U * 1024U - bytes)
            throw std::runtime_error("scene texture budget exceeded (64 MiB total, 16 MiB each)");
        total_bytes += bytes;
        try {
            auto rgba = assets::load_sg3_image({asset.archive_path, asset.id.image_index});
            if (rgba.width != asset.width || rgba.height != asset.height ||
                rgba.pixels.size() != bytes || static_cast<std::uint64_t>(rgba.width) * 4U >
                    static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                throw std::runtime_error("decoded image dimensions changed or are invalid");
            SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STATIC, rgba.width, rgba.height);
            if (!texture) throw std::runtime_error(SDL_GetError());
            source_textures_.emplace(id, texture);
            textures_.emplace(alias, texture);
            if (!SDL_UpdateTexture(texture, nullptr, rgba.pixels.data(), static_cast<int>(rgba.width * 4U)) ||
                !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST))
                throw std::runtime_error(SDL_GetError());
        } catch (const std::exception& error) {
            throw std::runtime_error("scene asset " + id + ": " + error.what());
        }
    }
}

void SceneRenderer::shutdown() {
    for (auto& [id, texture] : source_textures_) {
        static_cast<void>(id);
        SDL_DestroyTexture(texture);
    }
    source_textures_.clear();
    textures_.clear();
    renderer_ = nullptr;
}

bool SceneRenderer::draw_asset(const std::string& alias, scene::Point tile,
                               const scene::Camera2D& camera) {
    const auto& asset = scene_.assets.at(alias);
    const auto origin = camera.world_to_screen(scene::image_origin(tile, asset));
    const SDL_FRect rect{static_cast<float>(origin.x), static_cast<float>(origin.y),
                         static_cast<float>(asset.width * camera.zoom),
                         static_cast<float>(asset.height * camera.zoom)};
    if (rect.x + rect.w <= 0 || rect.y + rect.h <= 0 ||
        rect.x >= static_cast<float>(camera.viewport_width) ||
        rect.y >= static_cast<float>(camera.viewport_height)) return true;
    ++last_drawn_instances_;
    return SDL_RenderTexture(renderer_, textures_.at(alias), nullptr, &rect);
}

bool SceneRenderer::draw_diamond(scene::Cell cell, const scene::Camera2D& camera) {
    const scene::Point points[] = {{static_cast<double>(cell.x), static_cast<double>(cell.y)},
                                   {cell.x + 1.0, static_cast<double>(cell.y)},
                                   {cell.x + 1.0, cell.y + 1.0},
                                   {static_cast<double>(cell.x), cell.y + 1.0}};
    for (int i = 0; i < 4; ++i) {
        const auto a = camera.world_to_screen(scene::project(points[i]));
        const auto b = camera.world_to_screen(scene::project(points[(i + 1) % 4]));
        if (!SDL_RenderLine(renderer_, static_cast<float>(a.x), static_cast<float>(a.y),
                            static_cast<float>(b.x), static_cast<float>(b.y))) return false;
    }
    return true;
}

bool SceneRenderer::render(const scene::Camera2D& camera, bool grid,
                           std::optional<scene::Cell> selected) {
    last_drawn_instances_ = 0;
    if (!SDL_SetRenderDrawColor(renderer_, 25, 31, 38, 255) || !SDL_RenderClear(renderer_)) return false;
    for (int sum = 0; sum < scene_.width + scene_.height - 1; ++sum) {
        for (int y = 0; y < scene_.height; ++y) {
            const int x = sum - y;
            if (x < 0 || x >= scene_.width) continue;
            const std::string* alias = &scene_.default_terrain;
            for (const auto& over : scene_.terrain_overrides)
                if (over.cell.x == x && over.cell.y == y) { alias = &over.asset; break; }
            if (!draw_asset(*alias, {static_cast<double>(x), static_cast<double>(y)}, camera)) return false;
        }
    }
    for (const auto& object : scene::sorted_objects(scene_))
        if (!draw_asset(object.asset, object.tile, camera)) return false;
    if (grid) {
        if (!SDL_SetRenderDrawColor(renderer_, 210, 220, 220, 90)) return false;
        for (int y = 0; y < scene_.height; ++y)
            for (int x = 0; x < scene_.width; ++x)
                if (!draw_diamond({x, y}, camera)) return false;
    }
    if (selected) {
        if (!SDL_SetRenderDrawColor(renderer_, 255, 234, 80, 255) ||
            !draw_diamond(*selected, camera)) return false;
    }
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
