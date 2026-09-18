#include "renderer/RoadSpriteSet.h"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace openemperor {
namespace { std::size_t live_textures=0; }
std::size_t RoadSpriteSet::live_texture_count() { return live_textures; }
RoadSpriteSet::~RoadSpriteSet() { shutdown(); }
void RoadSpriteSet::initialize(SDL_Renderer* renderer,const assets::RoadVisualProfile& profile) {
    shutdown();renderer_=renderer;
    for (const auto& image:profile.unique_images) {
        SDL_Texture* texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STATIC,image.width,image.height);
        if (!texture) { const std::string error=SDL_GetError();shutdown();throw std::runtime_error(error); }
        textures_.push_back(texture);++live_textures;
        if (!SDL_UpdateTexture(texture,nullptr,image.pixels.data(),image.width*4) ||
            !SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) ||
            !SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST)) {
            const std::string error=SDL_GetError();shutdown();throw std::runtime_error(error);
        }
    }
}
void RoadSpriteSet::shutdown() {
    for (auto* texture:textures_) { SDL_DestroyTexture(texture);--live_textures; }
    textures_.clear();renderer_=nullptr;
}
bool RoadSpriteSet::draw(scene::Point ground,double zoom,const assets::RoadVisualProfile& profile,
                         const assets::RoadVisualEntry& entry,bool preview) const {
    const auto& image=profile.unique_images.at(entry.image_index);
    const SDL_FRect destination{static_cast<float>(ground.x-entry.ground_x*zoom),
        static_cast<float>(ground.y-entry.ground_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    auto* texture=textures_.at(entry.image_index);
    if (preview && !SDL_SetTextureAlphaMod(texture,128)) return false;
    const bool okay=SDL_RenderTexture(renderer_,texture,nullptr,&destination);
    const bool restored=!preview || SDL_SetTextureAlphaMod(texture,255);
    return okay && restored;
}
}
