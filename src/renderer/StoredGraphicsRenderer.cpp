#include "renderer/StoredGraphicsRenderer.h"
#include "core/PerformanceDiagnostics.h"

#include "assets/Sg3ImageLoader.h"
#include "maps/LandscapeProvenance.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include <cmath>

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace openemperor {
namespace { std::atomic<std::size_t> live_textures{0}; }
const char* landscape_debug_mode_name(LandscapeDebugMode mode) {
    switch (mode) {
    case LandscapeDebugMode::Ground: return "Ground only";
    case LandscapeDebugMode::Water: return "Ground + Water";
    case LandscapeDebugMode::Elevation: return "Ground + Water + Elevation";
    case LandscapeDebugMode::MountainsRocks: return "Ground + Water + Elevation + Mountains/Rocks";
    case LandscapeDebugMode::WallsMonuments: return "Ground + Water + Elevation + Mountains/Rocks + Walls/Monuments";
    case LandscapeDebugMode::Decorations: return "Ground + Water + Elevation + Decorations";
    case LandscapeDebugMode::Regenerated: return "Regenerated full (partial; explicit preview fallbacks)";
    case LandscapeDebugMode::Snapshot: return "Historical saved-ID snapshot";
    }
    return "invalid";
}
void StoredGraphicsRenderer::set_landscape_mode(LandscapeDebugMode mode) {
    landscape_mode_=plan_.landscape_layers_available ? mode:LandscapeDebugMode::Snapshot;
}
scene::Point StoredGraphicsRenderer::image_origin(const maps::PlacedFootprint& f) const {
    auto origin=f.image_origin;
    const auto asset=render_asset_index(f);
    if (asset!=f.asset_index) {
        const auto& r=plan_.assets[asset].record;
        origin=maps::stored_image_origin(maps::terrain_world(f.origin,plan_.border),
            static_cast<std::uint32_t>(r.width),static_cast<std::uint32_t>(r.height));
    }
    if (elevated()) origin.y-=maps::landscape_height(plan_,f.draw_cell_candidate.value_or(f.origin))*maps::landscape_height_step;
    return origin;
}
std::size_t StoredGraphicsRenderer::render_asset_index(const maps::PlacedFootprint& f) const {
    if (landscape_mode_!=LandscapeDebugMode::Snapshot && plan_.regenerated &&
        f.id<plan_.regenerated->footprint_assets.size()) {
        const auto index=plan_.regenerated->footprint_assets[f.id];
        if (index && plan_.assets[*index].status==maps::StoredStatus::Rendered) return *index;
    }
    return f.asset_index;
}
bool StoredGraphicsRenderer::overlay_visible(const maps::PlacedFootprint& f) const {
    if (landscape_mode_==LandscapeDebugMode::Snapshot) return true;
    if (landscape_mode_==LandscapeDebugMode::Ground) return false;
    const auto& c=plan_.cells[f.cell_indices.front()];
    if (c.slot==16) return landscape_mode_>=LandscapeDebugMode::Elevation;
    if (c.terrain_raw&4U) return true;
    if (c.terrain_raw&0x2000002U) return landscape_mode_>=LandscapeDebugMode::MountainsRocks;
    if ((c.terrain_raw&0x4000U) || c.slot==8) return landscape_mode_>=LandscapeDebugMode::WallsMonuments;
    return landscape_mode_==LandscapeDebugMode::Decorations || landscape_mode_==LandscapeDebugMode::Regenerated;
}
bool StoredGraphicsRenderer::regenerated_visible(std::size_t index) const {
    if (landscape_mode_==LandscapeDebugMode::Snapshot || !regenerated_ready_[index]) return false;
    const auto family=plan_.regenerated->instances[index].geometry.selection.family;
    if (family==maps::LandscapeFamily::Mountain || family==maps::LandscapeFamily::Rock)
        return landscape_mode_>=LandscapeDebugMode::MountainsRocks;
    if (family==maps::LandscapeFamily::Wall || family==maps::LandscapeFamily::GreatWall)
        return landscape_mode_>=LandscapeDebugMode::WallsMonuments;
    return landscape_mode_>=LandscapeDebugMode::Decorations;
}
bool StoredGraphicsRenderer::regenerated_placement_supported(
    const maps::RegeneratedLandscapeInstance& instance) const {
    if (instance.composition_policy!=maps::LandscapeCompositionPolicy::EarlyBaseSpatialOverlay &&
        instance.composition_policy!=maps::LandscapeCompositionPolicy::SpatialCombined) return false;
    const auto& geometry=instance.geometry;
    const auto owned=[&](maps::GridCell cell) {
        return std::find(geometry.owned_cells.begin(),geometry.owned_cells.end(),cell)!=geometry.owned_cells.end();
    };
    if (geometry.explicit_height &&
        (geometry.explicit_height->source!=maps::LandscapeInstanceHeightSource::SerializedCellHeight ||
         !owned(geometry.explicit_height->cell))) return false;
    if (geometry.depth_cell && !owned(*geometry.depth_cell)) return false;
    if (!geometry.explicit_anchor) return true;
    // Only the traced ordinary Type-30 wall claim uses this explicit caller
    // chain. A raw rectangle or an arbitrary custom anchor is not admitted.
    const auto& record=plan_.assets[instance.asset_index].record;
    const auto side=geometry.side;
    if ((side!=1 && side!=2 && side!=4) || record.image_type!=30 ||
        record.width!=int(80U*side-2U) || record.height<int(40U*side) ||
        record.uncompressed_length!=3200U*side*side || record.isometric_size_flag!=side ||
        record.horizontal_mirror_offset!=0 || record.animation_sprites!=0 ||
        geometry.origin.x>=maps::stored_grid_width || geometry.origin.y>=maps::stored_grid_height ||
        side>maps::stored_grid_width-geometry.origin.x || side>maps::stored_grid_height-geometry.origin.y)
        return false;
    const maps::GridCell marker{geometry.origin.x,geometry.origin.y+side-1};
    const maps::GridCell front{geometry.origin.x+side-1,geometry.origin.y+side-1};
    const auto& anchor=*geometry.explicit_anchor;
    return geometry.draw_cell==marker && geometry.explicit_height &&
        geometry.explicit_height->cell==marker && anchor.ground_cell==front &&
        anchor.x==int(40U*side-1U) && anchor.y_from_image_bottom==20 &&
        (!geometry.depth_cell || *geometry.depth_cell==front);
}
scene::Point StoredGraphicsRenderer::regenerated_image_origin(
    const maps::RegeneratedLandscapeInstance& instance) const {
    const auto& geometry=instance.geometry;
    const auto& record=plan_.assets[instance.asset_index].record;
    const auto height_cell=geometry.explicit_height ? geometry.explicit_height->cell:geometry.draw_cell;
    const int height=maps::landscape_height(plan_,height_cell);
    return maps::regenerated_instance_origin(geometry,plan_.border,
        unsigned(record.width),unsigned(record.height),height);
}

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
    base_textures_.assign(plan_.assets.size(),nullptr);
    overlay_textures_.assign(plan_.assets.size(),nullptr);
    snapshot_alpha_.resize(plan_.assets.size()); overlay_alpha_.resize(plan_.assets.size());
    std::vector<bool> software_spatial_combined(plan_.assets.size(),false);
    const auto* renderer_name=SDL_GetRendererName(renderer_);
    if (renderer_name && std::string_view(renderer_name)=="software" && plan_.regenerated)
        for (const auto& instance:plan_.regenerated->instances)
            if (instance.asset_index<software_spatial_combined.size() &&
                instance.composition_policy==maps::LandscapeCompositionPolicy::SpatialCombined &&
                regenerated_placement_supported(instance))
                software_spatial_combined[instance.asset_index]=true;
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
            bytes*(plan_.landscape_layers_available ? 3U:1U) > maps::stored_max_texture_bytes - plan_.logical_texture_bytes)
            throw std::runtime_error("stored graphics logical RGBA texture budget exceeded");
        const auto relative = record.id.archive_relative_path.generic_string();
        auto found = checked_archives.find(relative);
        if (found == checked_archives.end()) {
            const auto checked = maps::validate_stored_archive_sources(plan_.data_root,
                                                                        record.id.archive_relative_path);
            found = checked_archives.emplace(relative,checked).first;
        }
        asset.decode_attempted = true;
        const auto bytes_before=plan_.logical_texture_bytes;
        const auto uploads_before=plan_.texture_uploads;
        try {
            assets::Sg3ImageRequest request{found->second,record.id.image_index};
            request.split_isometric=plan_.landscape_layers_available;
            auto loaded=assets::load_sg3_image_with_source(request);
            auto& rgba=loaded.rgba;
            if (rgba.width != record.width || rgba.height != record.height ||
                rgba.pixels.size() != bytes)
                throw std::runtime_error("stored graphics decoded dimensions differ from metadata");
            asset.decode_succeeded = true;
            ++plan_.decoded_assets;
            // SDL's software STATIC surface enables RLE; repeated scaled copies
            // can then fail during deferred execution with an invalid source.
            // STREAMING avoids RLE for these Combined bodies. Upload still
            // happens exactly once here; all other components remain STATIC.
            // Reproduction: docs/rendering/scene-composition.md.
            const auto access=software_spatial_combined[i] ?
                SDL_TEXTUREACCESS_STREAMING:SDL_TEXTUREACCESS_STATIC;
            SDL_Texture* texture = SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                access,rgba.width,rgba.height);
            if (!texture) throw std::runtime_error(SDL_GetError());
            textures_[i] = texture;
            ++live_textures;
            if (!SDL_UpdateTexture(texture,nullptr,rgba.pixels.data(),rgba.width*4) ||
                !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST))
                throw std::runtime_error(SDL_GetError());
            performance::increment(performance::Counter::TextureUploads);
            asset.status = maps::StoredStatus::Rendered;
            ++plan_.texture_uploads;
            plan_.logical_texture_bytes += bytes;
            const auto retain_alpha=[](const assets::RgbaImage& image) {
                std::vector<std::uint8_t> alpha(image.pixels.size()/4);
                for (std::size_t pixel=0;pixel<alpha.size();++pixel) alpha[pixel]=image.pixels[pixel*4+3];
                return alpha;
            };
            snapshot_alpha_[i]=retain_alpha(rgba);
            const auto upload=[&](const assets::RgbaImage& component, SDL_Texture*& target) {
                target=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,
                    component.width,component.height);
                if (!target) throw std::runtime_error(SDL_GetError());
                ++live_textures;
                if (!SDL_UpdateTexture(target,nullptr,component.pixels.data(),component.width*4) ||
                    !SDL_SetTextureBlendMode(target,SDL_BLENDMODE_BLEND) ||
                    !SDL_SetTextureScaleMode(target,SDL_SCALEMODE_NEAREST)) throw std::runtime_error(SDL_GetError());
                ++plan_.texture_uploads; plan_.logical_texture_bytes+=bytes;
                performance::increment(performance::Counter::TextureUploads);
            };
            if (loaded.base && loaded.overlay) {
                upload(*loaded.base,base_textures_[i]); upload(*loaded.overlay,overlay_textures_[i]);
                overlay_alpha_[i]=retain_alpha(*loaded.overlay);
            }
        } catch (const std::exception& error) {
            if (textures_[i]) { SDL_DestroyTexture(textures_[i]); textures_[i]=nullptr; --live_textures; }
            for (auto* list : {&base_textures_, &overlay_textures_})
                if ((*list)[i]) { SDL_DestroyTexture((*list)[i]); (*list)[i]=nullptr; --live_textures; }
            plan_.logical_texture_bytes=bytes_before;
            plan_.texture_uploads=uploads_before;
            snapshot_alpha_[i].clear(); overlay_alpha_[i].clear();
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
    regenerated_ready_.clear();
    suppressed_footprints_.assign(plan_.footprints.size(),false);
    suppressed_cells_.assign(plan_.cells.size(),false);
    if (plan_.regenerated) {
        const auto& generated=*plan_.regenerated;
        for (const auto& instance:generated.instances)
            regenerated_ready_.push_back(plan_.assets[instance.asset_index].status==maps::StoredStatus::Rendered &&
                regenerated_placement_supported(instance));
        // Never combine a partial new instance with a partial old footprint.
        // Propagate a failed overlapping decode to its whole connected group.
        bool changed=true;
        while (changed) {
            changed=false;
            std::map<std::size_t,bool> compositions;
            for (std::size_t i=0;i<generated.instances.size();++i) {
                const auto group=generated.instances[i].composition_group;
                if (!group) continue;
                auto [entry,inserted]=compositions.emplace(*group,regenerated_ready_[i]);
                if (!inserted) entry->second=entry->second && regenerated_ready_[i];
            }
            for (std::size_t i=0;i<generated.instances.size();++i) {
                const auto group=generated.instances[i].composition_group;
                if (group && !compositions.at(*group) && regenerated_ready_[i]) {
                    regenerated_ready_[i]=false;changed=true;
                }
            }
            for (const auto& f:plan_.footprints) {
                bool any=false,all=true;
                for (const auto member:f.cell_indices) {
                    const auto owner=generated.cells[member].instance_index;
                    const bool ready=owner && regenerated_ready_[*owner];
                    any|=ready;all&=ready;
                }
                if (any && !all) for (const auto member:f.cell_indices) {
                    const auto owner=generated.cells[member].instance_index;
                    if (owner && regenerated_ready_[*owner]) {regenerated_ready_[*owner]=false;changed=true;}
                }
            }
        }
        for (std::size_t i=0;i<generated.cells.size();++i) {
            const auto owner=generated.cells[i].instance_index;
            suppressed_cells_[i]=owner && regenerated_ready_[*owner];
        }
        for (const auto& f:plan_.footprints)
            suppressed_footprints_[f.id]=!f.cell_indices.empty() && suppressed_cells_[f.cell_indices.front()];
    }
    plan_.regenerated_instance_active.assign(regenerated_ready_.begin(),regenerated_ready_.end());
    if (plan_.regenerated) for (std::size_t i=0;i<plan_.regenerated->instances.size();++i) {
        const auto& instance=plan_.regenerated->instances[i];
        if (instance.great_wall_context &&
            instance.great_wall_context->source==maps::GreatWallContextSource::ExplicitPreview &&
            !regenerated_ready_[i])
            throw std::runtime_error("Great Wall preview preparation failed: selected instance unavailable after decode/atomic activation");
    }
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
    if (plan_.regenerated) for (std::size_t i=0;i<plan_.regenerated->instances.size();++i) {
        const auto& f=plan_.regenerated->instances[i].geometry;
        const auto front=f.depth_cell.value_or(maps::GridCell{f.origin.x+f.side-1,f.origin.y+f.side-1});
        const auto ground=maps::terrain_ground(front,plan_.border);
        draw_order_.push_back({{ground.y,ground.x,scene::WorldVisualLayer::StoredMap,
            plan_.cells.size()+i},false,i,true});
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
    for (auto* list : {&base_textures_, &overlay_textures_}) {
        for (auto* texture : *list) if (texture) { SDL_DestroyTexture(texture); --live_textures; }
        list->clear();
    }
    snapshot_alpha_.clear(); overlay_alpha_.clear();
    draw_order_.clear();
    regenerated_ready_.clear();suppressed_footprints_.clear();suppressed_cells_.clear();
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
    if (landscape_mode_!=LandscapeDebugMode::Snapshot)
        return draw_component(renderer_index,camera,false);
    const auto& item=draw_order_.at(renderer_index);
    if (item.regenerated) return true; // Historical snapshot is exact old path.
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
    auto ground=selected ? (elevated() ? maps::landscape_ground(plan_,*selected):maps::terrain_ground(*selected,plan_.border)):scene::Point{};
    ground.y-=20;
    if (selected && !draw_diagnostic(ground,camera,true))
        return false;
    return true;
}

bool StoredGraphicsRenderer::render(const scene::Camera2D& camera,
                                    std::optional<maps::GridCell> selected) {
    begin_frame();
    for (std::size_t i=0;i<draw_order_.size();++i)
        if (!draw_ground_item(i,camera)) return false;
    for (std::size_t i=0;i<draw_order_.size();++i)
        if (!draw_item(i,camera)) return false;
    return draw_selection(camera,selected);
}

bool StoredGraphicsRenderer::draw_ground_item(std::size_t index, const scene::Camera2D& camera) {
    return landscape_mode_==LandscapeDebugMode::Snapshot || draw_component(index,camera,true);
}
bool StoredGraphicsRenderer::draw_component(std::size_t index, const scene::Camera2D& camera, bool base) {
    const auto& item=draw_order_.at(index);
    if (item.regenerated) {
        if (landscape_mode_==LandscapeDebugMode::Snapshot || !regenerated_ready_[item.plan_index] ||
            (!base && !regenerated_visible(item.plan_index))) return true;
        const auto& instance=plan_.regenerated->instances[item.plan_index];
        const auto asset=instance.asset_index;const auto& r=plan_.assets[asset].record;
        const bool combined=instance.composition_policy==maps::LandscapeCompositionPolicy::SpatialCombined;
        // Structural Base pixels belong to the same spatial body as Overlay.
        // Draw its existing combined texture once, never in the ground pass.
        if (combined && base) return true;
        if (!combined && !base && r.data_length==r.uncompressed_length) return true;
        const auto origin=regenerated_image_origin(instance);
        if (!maps::stored_rect_visible(origin,unsigned(r.width),unsigned(r.height),camera)) return true;
        auto* texture=combined ? textures_[asset]:(base ? base_textures_:overlay_textures_)[asset];
        if (!texture) return true;
        const auto top=camera.world_to_screen(origin);
        const SDL_FRect destination{float(top.x),float(top.y),float(r.width*camera.zoom),float(r.height*camera.zoom)};
        if (!SDL_RenderTexture(renderer_,texture,nullptr,&destination)) return false;
        ++last_texture_draws_;++last_drawn_instances_;return true;
    }
    if (item.footprint ? suppressed_footprints_[item.plan_index]:suppressed_cells_[item.plan_index]) return true;
    const auto diagnostic=[&](const maps::StoredCell& cell) {
        auto world=cell.world;
        world.y-=maps::landscape_height(plan_,cell.storage)*maps::landscape_height_step;
        if (!maps::stored_rect_visible({world.x-40,world.y},80,40,camera)) return true;
        if (!draw_diagnostic(world,camera,false,
            maps::interpret_terrain(cell.terrain_raw,cell.objects_raw).category)) return false;
        ++last_diagnostic_draws_; ++last_drawn_instances_;
        return true;
    };
    if (!item.footprint)
        return !base || diagnostic(plan_.cells[item.plan_index]);
    const auto& f=plan_.footprints[item.plan_index];
    const auto asset_index=render_asset_index(f);
    if (plan_.assets[asset_index].status!=maps::StoredStatus::Rendered) {
        if (base) for (const auto member:f.cell_indices)
            if (!diagnostic(plan_.cells[member])) return false;
        return true;
    }
    if (!base && !overlay_visible(f)) return true;
    const auto& r=plan_.assets[asset_index].record;
    if (!base && r.data_length==r.uncompressed_length) return true;
    const auto origin=image_origin(f);
    if (!maps::stored_rect_visible(origin,static_cast<std::uint32_t>(r.width),
        static_cast<std::uint32_t>(r.height),camera)) return true;
    auto* texture=(base ? base_textures_:overlay_textures_)[asset_index];
    if (!texture) return true;
    const auto top=camera.world_to_screen(origin);
    const SDL_FRect destination{static_cast<float>(top.x),static_cast<float>(top.y),
        static_cast<float>(r.width*camera.zoom),static_cast<float>(r.height*camera.zoom)};
    if (!SDL_RenderTexture(renderer_,texture,nullptr,&destination)) return false;
    ++last_texture_draws_; ++last_drawn_instances_;
    return true;
}
std::optional<StoredVisualHit> StoredGraphicsRenderer::hit_test_item(scene::Point screen,
    const scene::Camera2D& camera) const {
    const auto world=camera.screen_to_world(screen);
    for (auto it=draw_order_.rbegin();it!=draw_order_.rend();++it) {
        if (it->regenerated) {
            if (!regenerated_visible(it->plan_index)) continue;
            const auto& instance=plan_.regenerated->instances[it->plan_index];
            const auto asset=instance.asset_index;const auto& r=plan_.assets[asset].record;
            const auto origin=regenerated_image_origin(instance);
            const int x=int(std::floor(world.x-origin.x)),y=int(std::floor(world.y-origin.y));
            if (x<0 || y<0 || x>=r.width || y>=r.height) continue;
            const auto& alpha=(instance.composition_policy==maps::LandscapeCompositionPolicy::SpatialCombined ?
                snapshot_alpha_:overlay_alpha_)[asset];
            if (!alpha.empty() && alpha[std::size_t(y)*unsigned(r.width)+unsigned(x)])
                return StoredVisualHit{instance.geometry.draw_cell,it->key};
            continue;
        }
        if (!it->footprint) continue;
        if (landscape_mode_!=LandscapeDebugMode::Snapshot && suppressed_footprints_[it->plan_index]) continue;
        const auto& f=plan_.footprints[it->plan_index];
        const auto asset_index=render_asset_index(f);
        if (plan_.assets[asset_index].status!=maps::StoredStatus::Rendered || !overlay_visible(f)) continue;
        const auto& r=plan_.assets[asset_index].record;
        const auto origin=image_origin(f);
        const int x=static_cast<int>(std::floor(world.x-origin.x));
        const int y=static_cast<int>(std::floor(world.y-origin.y));
        if (x<0 || y<0 || x>=r.width || y>=r.height) continue;
        const auto& alpha=(landscape_mode_==LandscapeDebugMode::Snapshot ? snapshot_alpha_:overlay_alpha_)[asset_index];
        if (!alpha.empty() && alpha[static_cast<std::size_t>(y)*static_cast<unsigned>(r.width)+static_cast<unsigned>(x)])
            return StoredVisualHit{f.draw_cell_candidate.value_or(f.origin),it->key};
    }
    return std::nullopt;
}
std::optional<maps::GridCell> StoredGraphicsRenderer::hit_test(scene::Point screen,
    const scene::Camera2D& camera) const {
    const auto hit=hit_test_item(screen,camera);
    return hit ? std::optional{hit->cell}:std::nullopt;
}

} // namespace openemperor
