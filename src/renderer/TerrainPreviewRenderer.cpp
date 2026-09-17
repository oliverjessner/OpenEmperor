#include "renderer/TerrainPreviewRenderer.h"

#include "assets/Sg3ImageLoader.h"

#include <SDL3/SDL.h>

#include <set>
#include <stdexcept>
#include <utility>

namespace openemperor {

bool terrain_rect_visible(scene::Point origin, const scene::Camera2D& camera) {
    const auto top_left = camera.world_to_screen(origin);
    const auto width = 78.0 * camera.zoom;
    const auto height = 40.0 * camera.zoom;
    return top_left.x + width > 0 && top_left.y + height > 0 &&
           top_left.x < camera.viewport_width && top_left.y < camera.viewport_height;
}

TerrainPreviewRenderer::TerrainPreviewRenderer(maps::TerrainRenderPlan plan, maps::TerrainBindings bindings)
    : plan_(std::move(plan)), bindings_(std::move(bindings)) {}
TerrainPreviewRenderer::~TerrainPreviewRenderer() { shutdown(); }

void TerrainPreviewRenderer::initialize(SDL_Renderer* renderer) {
    renderer_ = renderer;
    std::set<std::string> used;
    for (const auto& cell : plan_.instances)
        if (cell.status == maps::PreviewStatus::CuratedPreview) used.insert(cell.asset_alias);
    if (used.size() > 16) throw std::runtime_error("terrain preview exceeds 16 used asset aliases");
    constexpr std::uint64_t one_tile_bytes = 78U * 40U * 4U;
    constexpr std::uint64_t total_budget = 4U * 1024U * 1024U;
    for (const auto& alias : used) {
        const auto& asset = bindings_.assets.at(alias);
        const auto id = asset.id.archive_relative_path.generic_string() + "#" +
            std::to_string(asset.id.image_index);
        if (const auto it = source_textures_.find(id); it != source_textures_.end()) {
            textures_.emplace(alias, it->second);
            continue;
        }
        if ((source_textures_.size() + 1) * one_tile_bytes > total_budget)
            throw std::runtime_error("terrain preview texture budget exceeded");
        try {
            const auto loaded = assets::load_sg3_image_with_source({asset.archive_path, asset.id.image_index});
            const auto& rgba = loaded.rgba;
            if (rgba.width != 78 || rgba.height != 40 || rgba.pixels.size() != one_tile_bytes)
                throw std::runtime_error("decoded tile is not 78x40 RGBA");
            SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STATIC, 78, 40);
            if (!texture) throw std::runtime_error(SDL_GetError());
            source_textures_.emplace(id, texture);
            if (!SDL_UpdateTexture(texture, nullptr, rgba.pixels.data(), 78 * 4) ||
                !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST))
                throw std::runtime_error(SDL_GetError());
            textures_.emplace(alias, texture);
            ++upload_count_;
        } catch (const std::exception& error) {
            for (auto& cell : plan_.instances) {
                if (cell.asset_alias != alias) continue;
                cell.status = maps::PreviewStatus::AssetError;
                const auto at = static_cast<std::size_t>(cell.storage.y) * maps::stored_grid_width + cell.storage.x;
                plan_.status_by_storage[at] = maps::PreviewStatus::AssetError;
                ++plan_.counts.asset_error;
            }
            throw std::runtime_error("terrain asset_error " + id + ": " + error.what());
        }
    }
}
void TerrainPreviewRenderer::shutdown() {
    for (const auto& [id, texture] : source_textures_) {
        static_cast<void>(id);
        SDL_DestroyTexture(texture);
    }
    source_textures_.clear();
    textures_.clear();
    renderer_ = nullptr;
}

bool TerrainPreviewRenderer::draw_diamond(scene::Point world, const scene::Camera2D& camera, bool selected) {
    const auto top = camera.world_to_screen(world);
    const float x = static_cast<float>(top.x), y = static_cast<float>(top.y);
    const float half_width = static_cast<float>(40.0 * camera.zoom);
    const float half_height = static_cast<float>(20.0 * camera.zoom);
    const SDL_FPoint points[] = {{x, y}, {x + half_width, y + half_height},
                                 {x, y + 2 * half_height}, {x - half_width, y + half_height}};
    if (!selected) {
        const SDL_FColor fill{0.46F, 0.13F, 0.52F, 1.0F};
        const SDL_Vertex vertices[] = {{points[0], fill, {}}, {points[1], fill, {}},
                                       {points[2], fill, {}}, {points[3], fill, {}}};
        const int indices[] = {0, 1, 2, 0, 2, 3};
        if (!SDL_RenderGeometry(renderer_, nullptr, vertices, 4, indices, 6)) return false;
    }
    if (!SDL_SetRenderDrawColor(renderer_, selected ? 255 : 255, selected ? 236 : 105,
                                selected ? 55 : 193, 255)) return false;
    for (int i = 0; i < 4; ++i)
        if (!SDL_RenderLine(renderer_, points[i].x, points[i].y,
                            points[(i + 1) % 4].x, points[(i + 1) % 4].y)) return false;
    if (!selected && !SDL_RenderLine(renderer_, x - half_width * 0.27F, y + half_height * 0.65F,
                                      x + half_width * 0.27F, y + half_height * 1.35F)) return false;
    return true;
}

bool TerrainPreviewRenderer::render(const scene::Camera2D& camera,
                                     std::optional<maps::GridCell> selected) {
    last_drawn_instances_ = 0;
    for (const auto& instance : plan_.instances) {
        if (!terrain_rect_visible(instance.image_origin, camera)) continue;
        ++last_drawn_instances_;
        if (instance.status == maps::PreviewStatus::CuratedPreview) {
            const auto origin = camera.world_to_screen(instance.image_origin);
            const SDL_FRect destination{static_cast<float>(origin.x), static_cast<float>(origin.y),
                                        static_cast<float>(78.0 * camera.zoom),
                                        static_cast<float>(40.0 * camera.zoom)};
            if (!SDL_RenderTexture(renderer_, textures_.at(instance.asset_alias), nullptr, &destination)) return false;
        } else if (!draw_diamond(instance.world, camera, false)) return false;
    }
    if (selected) {
        const auto world = maps::terrain_world(*selected, plan_.border);
        if (!draw_diamond(world, camera, true)) return false;
    }
    return true;
}
} // namespace openemperor
