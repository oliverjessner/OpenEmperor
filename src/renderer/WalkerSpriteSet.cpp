#include "renderer/WalkerSpriteSet.h"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace openemperor {
WalkerSpriteSet::~WalkerSpriteSet() { shutdown(); }
void WalkerSpriteSet::initialize(SDL_Renderer* renderer,const assets::WalkerVisualProfile& profile) {
    shutdown(); renderer_=renderer;
    try {
        for (const auto& image:profile.unique_images) {
            SDL_Texture* texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC,image.width,image.height);
            if (!texture) throw std::runtime_error(SDL_GetError());
            textures_.push_back(texture);
            if (!SDL_UpdateTexture(texture,nullptr,image.pixels.data(),image.width*4) ||
                !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST))
                throw std::runtime_error(SDL_GetError());
        }
    } catch (...) { shutdown(); throw; }
}
void WalkerSpriteSet::shutdown() {
    for (auto* texture:textures_) SDL_DestroyTexture(texture);
    textures_.clear(); renderer_=nullptr;
}
bool WalkerSpriteSet::draw(std::size_t frame,scene::Point ground,double zoom,
                           const assets::WalkerVisualProfile& profile,
                           scene::Point clip_min,scene::Point clip_max) const {
    const auto& selected=profile.frames.at(frame);
    const auto& image=profile.unique_images.at(selected.image_index);
    const SDL_FRect destination{static_cast<float>(ground.x-selected.foot_x*zoom),
        static_cast<float>(ground.y-selected.foot_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    if (destination.x+destination.w<=clip_min.x || destination.y+destination.h<=clip_min.y ||
        destination.x>=clip_max.x || destination.y>=clip_max.y) return true;
    return SDL_RenderTexture(renderer_,textures_.at(selected.image_index),nullptr,&destination);
}
}
