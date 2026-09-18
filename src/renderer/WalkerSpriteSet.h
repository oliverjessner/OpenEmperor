#pragma once
#include "assets/WalkerVisualProfile.h"
#include "scene/IsoProjection.h"
#include <cstddef>
#include <vector>
struct SDL_Renderer;
struct SDL_Texture;
namespace openemperor {
class WalkerSpriteSet {
public:
    WalkerSpriteSet()=default;
    WalkerSpriteSet(const WalkerSpriteSet&)=delete;
    WalkerSpriteSet& operator=(const WalkerSpriteSet&)=delete;
    ~WalkerSpriteSet();
    void initialize(SDL_Renderer* renderer,const assets::WalkerVisualProfile& profile);
    void shutdown();
    bool draw(std::size_t frame,scene::Point ground,double zoom,
              const assets::WalkerVisualProfile& profile,
              scene::Point clip_min,scene::Point clip_max) const;
    std::size_t texture_count() const { return textures_.size(); }
    static std::size_t live_texture_count();
private:
    SDL_Renderer* renderer_=nullptr;
    std::vector<SDL_Texture*> textures_;
};
}
