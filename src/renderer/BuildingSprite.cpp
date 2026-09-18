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
    for (const auto& image:profile.unique_images) {
        SDL_Texture* texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STATIC,image.width,image.height);
        if (!texture) { const std::string message=SDL_GetError();shutdown();throw std::runtime_error(message); }
        textures_.push_back(texture);++live_textures;
        if (!SDL_UpdateTexture(texture,nullptr,image.pixels.data(),image.width*4) ||
            !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
            !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST)) {
            const std::string message=SDL_GetError();shutdown();throw std::runtime_error(message);
        }
    }
}
void BuildingSprite::shutdown() {
    for (auto* texture:textures_) { SDL_DestroyTexture(texture);--live_textures; }
    textures_.clear();
    renderer_=nullptr;
}
bool BuildingSprite::draw(scene::Point ground,double zoom,
                          const assets::BuildingVisualProfile& profile,
                          const assets::BuildingVisualEntry& entry,bool preview) const {
    const auto& image=profile.unique_images.at(entry.image_index);
    const SDL_FRect destination{
        static_cast<float>(ground.x-entry.ground_x*zoom),
        static_cast<float>(ground.y-entry.ground_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    auto* texture=textures_.at(entry.image_index);
    if (preview && !SDL_SetTextureAlphaMod(texture,128)) return false;
    const bool drawn=SDL_RenderTexture(renderer_,texture,nullptr,&destination);
    const bool restored=!preview || SDL_SetTextureAlphaMod(texture,255);
    return drawn && restored;
}
}
