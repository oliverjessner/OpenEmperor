#pragma once
#include "assets/BuildingVisualProfile.h"
#include "scene/IsoProjection.h"
#include <cstddef>
struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {
class BuildingSprite {
public:
    BuildingSprite()=default;
    BuildingSprite(const BuildingSprite&)=delete;
    BuildingSprite& operator=(const BuildingSprite&)=delete;
    ~BuildingSprite();
    void initialize(SDL_Renderer* renderer,const assets::BuildingVisualProfile& profile);
    void shutdown();
    bool draw(scene::Point ground,double zoom,const assets::BuildingVisualProfile& profile) const;
    std::size_t texture_count() const { return texture_ ? 1U:0U; }
    static std::size_t live_texture_count();
private:
    SDL_Renderer* renderer_=nullptr;
    SDL_Texture* texture_=nullptr;
};
}
