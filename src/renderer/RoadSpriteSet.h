#pragma once
#include "assets/RoadVisualProfile.h"
#include "scene/IsoProjection.h"
#include <cstddef>
#include <vector>
struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {
class RoadSpriteSet {
public:
    RoadSpriteSet()=default;
    RoadSpriteSet(const RoadSpriteSet&)=delete;
    RoadSpriteSet& operator=(const RoadSpriteSet&)=delete;
    ~RoadSpriteSet();
    void initialize(SDL_Renderer* renderer,const assets::RoadVisualProfile& profile);
    void shutdown();
    bool draw(scene::Point ground,double zoom,const assets::RoadVisualProfile& profile,
              const assets::RoadVisualEntry& entry,bool preview=false) const;
    std::size_t texture_count() const { return textures_.size(); }
    static std::size_t live_texture_count();
private:
    SDL_Renderer* renderer_=nullptr;
    std::vector<SDL_Texture*> textures_;
};
}
