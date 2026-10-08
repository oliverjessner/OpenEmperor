#include "renderer/WalkerSpriteSet.h"
#include "renderer/TextureCompatibility.h"
#include "core/PerformanceDiagnostics.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <stdexcept>

namespace openemperor {
namespace { std::size_t live_textures=0; }
std::size_t WalkerSpriteSet::live_texture_count() { return live_textures; }
WalkerSpriteSet::~WalkerSpriteSet() { shutdown(); }
void WalkerSpriteSet::initialize(SDL_Renderer* renderer,const assets::WalkerVisualProfile& profile,
                                 std::uint64_t available_total_rgba_bytes) {
    shutdown(); renderer_=renderer;
    try { append(profile,available_total_rgba_bytes); }
    catch (...) { shutdown(); throw; }
}
void WalkerSpriteSet::append(const assets::WalkerVisualProfile& profile,
                             std::uint64_t available_total_rgba_bytes) {
    if (!renderer_) throw std::runtime_error("walker textures require a renderer");
    const auto bytes=assets::walker_rgba_bytes(profile);
    if (bytes>std::min(available_total_rgba_bytes,assets::walker_max_rgba_bytes))
        throw std::runtime_error("walker RGBA exceeds remaining session budget");
    if (profile.unique_images.size()<textures_.size())
        throw std::runtime_error("walker append cannot remove the active image prefix");
    for (std::size_t i=0;i<textures_.size();++i)
        if (static_cast<std::uint64_t>(profile.unique_images[i].width)*
                profile.unique_images[i].height*4U!=texture_bytes_[i])
            throw std::runtime_error("walker append changed the active image prefix");
    std::vector<SDL_Texture*> pending;
    pending.reserve(profile.unique_images.size()-textures_.size());
    textures_.reserve(profile.unique_images.size());
    texture_bytes_.reserve(profile.unique_images.size());
    try {
        for (std::size_t i=textures_.size();i<profile.unique_images.size();++i) {
            const auto& image=profile.unique_images[i];
            SDL_Texture* texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                texture_compatibility::eager_rgba_access(renderer_),image.width,image.height);
            if (!texture) throw std::runtime_error(SDL_GetError());
            pending.push_back(texture);
            ++live_textures;
            if (!SDL_UpdateTexture(texture,nullptr,image.pixels.data(),image.width*4) ||
                !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST) ||
                !SDL_SetTextureColorMod(texture,255,255,255) ||
                !SDL_SetTextureAlphaMod(texture,255))
                throw std::runtime_error(SDL_GetError());
            performance::increment(performance::Counter::TextureUploads);
        }
    } catch (...) {
        for (auto* texture:pending) SDL_DestroyTexture(texture);
        live_textures-=pending.size();
        throw;
    }
    for (std::size_t i=textures_.size();i<profile.unique_images.size();++i)
        texture_bytes_.push_back(static_cast<std::uint64_t>(profile.unique_images[i].width)*
                                 profile.unique_images[i].height*4U);
    textures_.insert(textures_.end(),pending.begin(),pending.end());
    logical_bytes_=bytes;
}
void WalkerSpriteSet::truncate(std::size_t image_count) {
    if (image_count>textures_.size())
        throw std::invalid_argument("walker truncate cannot increase the active image count");
    for (std::size_t i=image_count;i<textures_.size();++i) {
        SDL_DestroyTexture(textures_[i]);
        logical_bytes_-=texture_bytes_[i];
    }
    live_textures-=textures_.size()-image_count;
    textures_.resize(image_count);texture_bytes_.resize(image_count);
}
void WalkerSpriteSet::retain_images(const std::vector<std::size_t>& indices) {
    std::vector<SDL_Texture*> retained;
    std::vector<std::uint64_t> bytes;
    retained.reserve(indices.size());bytes.reserve(indices.size());
    std::uint64_t total=0;
    for (std::size_t i=0;i<indices.size();++i) {
        const auto index=indices[i];
        if (index>=textures_.size() || (i && index<=indices[i-1]))
            throw std::invalid_argument("walker retained images must be ordered active indices");
        retained.push_back(textures_[index]);bytes.push_back(texture_bytes_[index]);
        total+=texture_bytes_[index];
    }
    // All validation and allocation precede releasing any texture. Retaining
    // another role never decodes, uploads or recreates its shared pixels.
    std::size_t next=0;
    for (std::size_t i=0;i<textures_.size();++i) {
        if (next<indices.size() && indices[next]==i) { ++next;continue; }
        SDL_DestroyTexture(textures_[i]);--live_textures;
    }
    textures_=std::move(retained);texture_bytes_=std::move(bytes);logical_bytes_=total;
}
void WalkerSpriteSet::shutdown() {
    truncate(0);renderer_=nullptr;
}
bool WalkerSpriteSet::draw(std::size_t frame,scene::Point ground,double zoom,
                           const assets::WalkerRoleVisual& role_visual,
                           const assets::WalkerVisualProfile& profile,
                           scene::Point clip_min,scene::Point clip_max) const {
    const auto& selected=role_visual.frames.at(frame);
    const auto& image=profile.unique_images.at(selected.image_index);
    const SDL_FRect destination{static_cast<float>(ground.x-selected.foot_x*zoom),
        static_cast<float>(ground.y-selected.foot_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    if (destination.x+destination.w<=clip_min.x || destination.y+destination.h<=clip_min.y ||
        destination.x>=clip_max.x || destination.y>=clip_max.y) return true;
    if (selected.flip_x)
        return SDL_RenderTextureRotated(renderer_,textures_.at(selected.image_index),nullptr,
                                       &destination,0.0,nullptr,SDL_FLIP_HORIZONTAL);
    return SDL_RenderTexture(renderer_,textures_.at(selected.image_index),nullptr,&destination);
}
}
