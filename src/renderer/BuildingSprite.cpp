#include "renderer/BuildingSprite.h"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace openemperor {
namespace { std::size_t live_textures=0; }
std::size_t BuildingSprite::live_texture_count() { return live_textures; }
BuildingSprite::~BuildingSprite() { shutdown(); }
void BuildingSprite::initialize(SDL_Renderer* renderer,
                                const assets::BuildingVisualProfile& profile) {
    shutdown(); renderer_=renderer;
    const auto& image=profile.pottery_image;
    texture_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,
                               image.width,image.height);
    if (!texture_) throw std::runtime_error(SDL_GetError());
    ++live_textures;
    if (!SDL_UpdateTexture(texture_,nullptr,image.pixels.data(),image.width*4) ||
        !SDL_SetTextureBlendMode(texture_,SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(texture_,SDL_SCALEMODE_NEAREST)) {
        const std::string message=SDL_GetError();shutdown();throw std::runtime_error(message);
    }
}
void BuildingSprite::shutdown() {
    if (texture_) { SDL_DestroyTexture(texture_);texture_=nullptr;--live_textures; }
    renderer_=nullptr;
}
bool BuildingSprite::draw(scene::Point ground,double zoom,
                          const assets::BuildingVisualProfile& profile) const {
    const auto& image=profile.pottery_image;
    const SDL_FRect destination{
        static_cast<float>(ground.x-profile.ground_x*zoom),
        static_cast<float>(ground.y-profile.ground_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    return SDL_RenderTexture(renderer_,texture_,nullptr,&destination);
}
}
