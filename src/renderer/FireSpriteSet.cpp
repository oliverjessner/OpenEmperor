#include "renderer/FireSpriteSet.h"

#include "core/PerformanceDiagnostics.h"
#include "renderer/TextureCompatibility.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace openemperor {
namespace { std::size_t live_textures=0; }

std::size_t FireSpriteSet::live_texture_count() { return live_textures; }
FireSpriteSet::~FireSpriteSet() { shutdown(); }

void FireSpriteSet::initialize(SDL_Renderer* renderer,const assets::FireVisualProfile& profile,
                                std::uint64_t remaining_bytes) {
    shutdown();
    assets::validate_fire_visual_profile(profile);
    const auto bytes=assets::fire_rgba_bytes(profile);
    if (bytes>remaining_bytes) throw std::runtime_error("fire clip exceeds remaining session RGBA budget");
    if (!renderer) throw std::runtime_error("fire renderer missing");
    renderer_=renderer;
    try {
        textures_.reserve(profile.unique_images.size());
        for (const auto& image:profile.unique_images) {
            auto* texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                texture_compatibility::eager_rgba_access(renderer_),image.width,image.height);
            if (!texture) throw std::runtime_error(SDL_GetError());
            textures_.push_back(texture);
            ++live_textures;
            if (!SDL_UpdateTexture(texture,nullptr,image.pixels.data(),image.width*4))
                throw std::runtime_error(SDL_GetError());
            performance::increment(performance::Counter::TextureUploads);
            if (!SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST) ||
                !SDL_SetTextureColorMod(texture,255,255,255) || !SDL_SetTextureAlphaMod(texture,255))
                throw std::runtime_error(SDL_GetError());
        }
        FireSpriteBounds clip{{std::numeric_limits<double>::max(),std::numeric_limits<double>::max()},
                               {std::numeric_limits<double>::lowest(),std::numeric_limits<double>::lowest()}};
        for (const auto& frame:profile.frames) {
            const auto& image=profile.unique_images[frame.image_index];
            clip.min.x=std::min(clip.min.x,-frame.anchor_x);
            clip.min.y=std::min(clip.min.y,-frame.anchor_y);
            clip.max.x=std::max(clip.max.x,image.width-frame.anchor_x);
            clip.max.y=std::max(clip.max.y,image.height-frame.anchor_y);
        }
        clip_bounds_=clip;
        logical_bytes_=bytes;
    } catch (...) { shutdown(); throw; }
}

void FireSpriteSet::shutdown() {
    for (auto* texture:textures_) SDL_DestroyTexture(texture);
    live_textures-=textures_.size();
    textures_.clear();
    renderer_=nullptr;
    logical_bytes_=0;
    clip_bounds_.reset();
}

std::optional<FireSpriteBounds> FireSpriteSet::clip_bounds(scene::Point attachment,double zoom) const {
    if (!clip_bounds_ || !std::isfinite(zoom) || zoom<=0 ||
        !std::isfinite(attachment.x) || !std::isfinite(attachment.y)) return std::nullopt;
    const FireSpriteBounds box{{attachment.x+clip_bounds_->min.x*zoom,attachment.y+clip_bounds_->min.y*zoom},
                              {attachment.x+clip_bounds_->max.x*zoom,attachment.y+clip_bounds_->max.y*zoom}};
    constexpr double limit=std::numeric_limits<float>::max();
    if (!std::isfinite(box.min.x) || !std::isfinite(box.min.y) || !std::isfinite(box.max.x) ||
        !std::isfinite(box.max.y) || std::abs(box.min.x)>limit || std::abs(box.min.y)>limit ||
        std::abs(box.max.x)>limit || std::abs(box.max.y)>limit ||
        box.max.x-box.min.x>limit || box.max.y-box.min.y>limit) return std::nullopt;
    return box;
}

std::optional<FireSpriteBounds> FireSpriteSet::bounds(std::size_t frame,scene::Point attachment,
                                                     double zoom,
                                                     const assets::FireVisualProfile& profile) {
    if (frame>=profile.frames.size() || !std::isfinite(zoom) || zoom<=0 ||
        !std::isfinite(attachment.x) || !std::isfinite(attachment.y)) return std::nullopt;
    const auto& selected=profile.frames[frame];
    if (selected.image_index>=profile.unique_images.size() ||
        !std::isfinite(selected.anchor_x) || !std::isfinite(selected.anchor_y)) return std::nullopt;
    const auto& image=profile.unique_images[selected.image_index];
    if (image.width==0 || image.height==0) return std::nullopt;
    const scene::Point min{attachment.x-selected.anchor_x*zoom,attachment.y-selected.anchor_y*zoom};
    const scene::Point max{min.x+image.width*zoom,min.y+image.height*zoom};
    constexpr double limit=std::numeric_limits<float>::max();
    if (!std::isfinite(min.x) || !std::isfinite(min.y) || !std::isfinite(max.x) ||
        !std::isfinite(max.y) || std::abs(min.x)>limit || std::abs(min.y)>limit ||
        std::abs(max.x)>limit || std::abs(max.y)>limit ||
        max.x-min.x>limit || max.y-min.y>limit) return std::nullopt;
    return FireSpriteBounds{min,max};
}

bool FireSpriteSet::draw(std::size_t frame,scene::Point attachment,double zoom,
                         const assets::FireVisualProfile& profile,
                         scene::Point clip_min,scene::Point clip_max) const {
    const auto box=bounds(frame,attachment,zoom,profile);
    if (!renderer_ || !box || profile.frames[frame].image_index>=textures_.size() ||
        !std::isfinite(clip_min.x) || !std::isfinite(clip_min.y) ||
        !std::isfinite(clip_max.x) || !std::isfinite(clip_max.y) ||
        clip_max.x<=clip_min.x || clip_max.y<=clip_min.y) return false;
    if (box->max.x<=clip_min.x || box->max.y<=clip_min.y ||
        box->min.x>=clip_max.x || box->min.y>=clip_max.y) return true;
    const SDL_FRect destination{static_cast<float>(box->min.x),static_cast<float>(box->min.y),
        static_cast<float>(box->max.x-box->min.x),static_cast<float>(box->max.y-box->min.y)};
    // Dedicated shared effect textures stay BLEND/white/alpha255. Rendering
    // changes neither texture modulation nor renderer draw-blend state.
    return SDL_RenderTexture(renderer_,textures_[profile.frames[frame].image_index],nullptr,&destination);
}
} // namespace openemperor
