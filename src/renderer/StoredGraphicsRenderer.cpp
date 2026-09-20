#include "renderer/StoredGraphicsRenderer.h"

#include "assets/Sg3ImageLoader.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor {
namespace { std::atomic<std::size_t> live_textures{0}; }

std::size_t StoredGraphicsRenderer::live_texture_count() { return live_textures.load(); }

std::array<std::uint8_t,3> stored_presentation_fallback_color(
    maps::TerrainCategory category) {
    using C=maps::TerrainCategory;
    switch (category) {
    case C::Flood: case C::Water: return {58,91,112};
    case C::Vegetation: case C::Bamboo: case C::Fertile: return {81,96,67};
    case C::Rock: case C::Elevation: case C::Structure: case C::Monument:
        return {103,94,78};
    case C::Road: return {126,105,75};
    case C::Empty: return {103,101,73};
    case C::OffMap: return {48,50,45};
    case C::OtherMarked: case C::Unknown: return {73,78,65};
    }
    return {73,78,65};
}

StoredGraphicsRenderer::StoredGraphicsRenderer(maps::StoredGraphicsPlan plan)
    : plan_(std::move(plan)) {}
StoredGraphicsRenderer::~StoredGraphicsRenderer() { shutdown(); }

void StoredGraphicsRenderer::initialize(SDL_Renderer* renderer) {
    renderer_ = renderer;
    textures_.assign(plan_.assets.size(),nullptr);
    std::map<std::string,std::filesystem::path> checked_archives;
    for (std::size_t i=0;i<plan_.assets.size();++i) {
        auto& asset = plan_.assets[i];
        if (asset.status != maps::StoredStatus::DecodePending) continue;
        const auto& record = asset.record;
        const auto bytes = static_cast<std::uint64_t>(record.width) *
                           static_cast<std::uint64_t>(record.height) * 4U;
        if (record.data_length > maps::stored_max_payload_bytes ||
            record.alpha_length > maps::stored_max_payload_bytes)
            throw std::runtime_error("stored graphics selected payload exceeds 16 MiB read budget");
        if (bytes > maps::stored_max_image_bytes ||
            bytes > maps::stored_max_texture_bytes - plan_.logical_texture_bytes)
            throw std::runtime_error("stored graphics logical RGBA texture budget exceeded");
        const auto relative = record.id.archive_relative_path.generic_string();
        auto found = checked_archives.find(relative);
        if (found == checked_archives.end()) {
            const auto checked = maps::validate_stored_archive_sources(plan_.data_root,
                                                                        record.id.archive_relative_path);
            found = checked_archives.emplace(relative,checked).first;
        }
        asset.decode_attempted = true;
        try {
            auto rgba = assets::load_sg3_image({found->second,record.id.image_index});
            if (rgba.width != record.width || rgba.height != record.height ||
                rgba.pixels.size() != bytes)
                throw std::runtime_error("stored graphics decoded dimensions differ from metadata");
            asset.decode_succeeded = true;
            ++plan_.decoded_assets;
            SDL_Texture* texture = SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC,rgba.width,rgba.height);
            if (!texture) throw std::runtime_error(SDL_GetError());
            textures_[i] = texture;
            ++live_textures;
            if (!SDL_UpdateTexture(texture,nullptr,rgba.pixels.data(),rgba.width*4) ||
                !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST))
                throw std::runtime_error(SDL_GetError());
            asset.status = maps::StoredStatus::Rendered;
            ++plan_.texture_uploads;
            plan_.logical_texture_bytes += bytes;
        } catch (const std::exception& error) {
            if (textures_[i]) { SDL_DestroyTexture(textures_[i]); textures_[i]=nullptr; --live_textures; }
            asset.status = maps::StoredStatus::DecodeFailed;
            asset.error = error.what();
        }
    }
    for (auto& cell : plan_.cells) {
        if (cell.status == maps::StoredStatus::DecodePending && cell.asset_index) {
            cell.status = plan_.assets[*cell.asset_index].status;
            plan_.status_by_storage[cell.cell_index] = cell.status;
        }
    }
    for (auto& footprint : plan_.footprints)
        footprint.status=plan_.assets[footprint.asset_index].status;
    draw_order_.clear();
    draw_order_.reserve(plan_.cells.size()+plan_.footprints.size());
    for (const auto& footprint : plan_.footprints) {
        const maps::GridCell front{footprint.origin.x+footprint.width_cells-1,
                                   footprint.origin.y+footprint.height_cells-1};
        const auto ground=maps::terrain_ground(front,plan_.border);
        draw_order_.push_back({{ground.y,ground.x,scene::WorldVisualLayer::StoredMap,
                               footprint.cell_indices.front()},true,footprint.id});
    }
    for (std::size_t i=0;i<plan_.cells.size();++i) {
        const auto& cell=plan_.cells[i];
        if (!cell.footprint_index) {
            const auto ground=maps::terrain_ground(cell.storage,plan_.border);
            draw_order_.push_back({{ground.y,ground.x,scene::WorldVisualLayer::StoredMap,i},
                                   false,i});
        }
    }
    std::stable_sort(draw_order_.begin(),draw_order_.end(),[](const StoredDrawItem& a,
                                                                const StoredDrawItem& b) {
        return a.key<b.key;
    });
    ++stored_order_builds_;
}

void StoredGraphicsRenderer::shutdown() {
    for (auto* texture : textures_) if (texture) { SDL_DestroyTexture(texture); --live_textures; }
    textures_.clear();
    draw_order_.clear();
    renderer_ = nullptr;
}

bool StoredGraphicsRenderer::draw_diagnostic(scene::Point world,
                                               const scene::Camera2D& camera, bool selected,
                                               maps::TerrainCategory category) {
    const auto top = camera.world_to_screen(world);
    const float x = static_cast<float>(top.x), y = static_cast<float>(top.y);
    const float half_width = static_cast<float>(40.0 * camera.zoom);
    const float half_height = static_cast<float>(20.0 * camera.zoom);
    const SDL_FPoint points[] = {{x,y},{x+half_width,y+half_height},
                                {x,y+2*half_height},{x-half_width,y+half_height}};
    if (!selected) {
        const auto rgb=debug_diagnostics_ ? std::array<std::uint8_t,3>{112,31,128}:
            stored_presentation_fallback_color(category);
        const SDL_FColor fill{rgb[0]/255.0F,rgb[1]/255.0F,rgb[2]/255.0F,1.0F};
        const SDL_Vertex vertices[] = {{points[0],fill,{}},{points[1],fill,{}},
                                       {points[2],fill,{}},{points[3],fill,{}}};
        const int indices[] = {0,1,2,0,2,3};
        if (!SDL_RenderGeometry(renderer_,nullptr,vertices,4,indices,6)) return false;
    }
    const auto outline=debug_diagnostics_ ? std::array<std::uint8_t,3>{255,100,190}:
        stored_presentation_fallback_color(category);
    if (!SDL_SetRenderDrawColor(renderer_,selected ? 255:outline[0],
                                selected ? 236:outline[1],selected ? 55:outline[2],255)) return false;
    for (int i=0;i<4;++i)
        if (!SDL_RenderLine(renderer_,points[i].x,points[i].y,
                            points[(i+1)%4].x,points[(i+1)%4].y)) return false;
    return true;
}

void StoredGraphicsRenderer::begin_frame() {
    last_drawn_instances_=0;
    last_texture_draws_=0;
    last_diagnostic_draws_=0;
}

bool StoredGraphicsRenderer::draw_item(std::size_t renderer_index,
                                      const scene::Camera2D& camera) {
    const auto& item=draw_order_.at(renderer_index);
    if (item.footprint) {
        const auto& footprint=plan_.footprints[item.plan_index];
        const auto& record=plan_.assets[footprint.asset_index].record;
        if (footprint.status==maps::StoredStatus::Rendered) {
            if (!maps::stored_rect_visible(footprint.image_origin,
                static_cast<std::uint32_t>(record.width),
                static_cast<std::uint32_t>(record.height),camera)) return true;
            const auto top=camera.world_to_screen(footprint.image_origin);
            const SDL_FRect destination{static_cast<float>(top.x),static_cast<float>(top.y),
                static_cast<float>(record.width*camera.zoom),
                static_cast<float>(record.height*camera.zoom)};
            if (!SDL_RenderTexture(renderer_,textures_[footprint.asset_index],nullptr,&destination))
                return false;
            ++last_texture_draws_;
        } else {
            for (const auto member : footprint.cell_indices) {
                const auto& cell=plan_.cells[member];
                if (!maps::stored_rect_visible({cell.world.x-40,cell.world.y},80,40,camera)) continue;
                if (!draw_diagnostic(cell.world,camera,false,
                    maps::interpret_terrain(cell.terrain_raw,cell.objects_raw).category)) return false;
                ++last_diagnostic_draws_;
            }
        }
    } else {
        const auto& cell=plan_.cells[item.plan_index];
        const scene::Point origin{cell.world.x-40,cell.world.y};
        if (!maps::stored_rect_visible(origin,80,40,camera)) return true;
        if (!draw_diagnostic(cell.world,camera,false,
            maps::interpret_terrain(cell.terrain_raw,cell.objects_raw).category)) return false;
        ++last_diagnostic_draws_;
    }
    ++last_drawn_instances_;
    return true;
}

bool StoredGraphicsRenderer::draw_selection(const scene::Camera2D& camera,
                                            std::optional<maps::GridCell> selected) {
    if (selected && !draw_diagnostic(maps::terrain_world(*selected,plan_.border),camera,true))
        return false;
    return true;
}

bool StoredGraphicsRenderer::render(const scene::Camera2D& camera,
                                    std::optional<maps::GridCell> selected) {
    begin_frame();
    for (std::size_t i=0;i<draw_order_.size();++i)
        if (!draw_item(i,camera)) return false;
    return draw_selection(camera,selected);
}

} // namespace openemperor
