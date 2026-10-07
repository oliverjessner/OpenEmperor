#pragma once

#include "assets/FireVisualProfile.h"
#include "scene/IsoProjection.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace openemperor {

struct FireSpriteBounds { scene::Point min,max; };

class FireSpriteSet {
public:
    FireSpriteSet()=default;
    FireSpriteSet(const FireSpriteSet&)=delete;
    FireSpriteSet& operator=(const FireSpriteSet&)=delete;
    ~FireSpriteSet();
    // Caller supplies the remaining existing session budget; no budget increase.
    void initialize(SDL_Renderer* renderer,const assets::FireVisualProfile& profile,
                    std::uint64_t remaining_bytes=assets::fire_rgba_budget);
    void shutdown();
    bool draw(std::size_t frame,scene::Point attachment,double zoom,
              const assets::FireVisualProfile& profile,
              scene::Point clip_min,scene::Point clip_max) const;
    static std::optional<FireSpriteBounds> bounds(std::size_t frame,scene::Point attachment,
                                                 double zoom,
                                                 const assets::FireVisualProfile& profile);
    // All-frame union prepared once, so owner culling retains an entering tip.
    std::optional<FireSpriteBounds> clip_bounds(scene::Point attachment,double zoom) const;
    std::size_t texture_count() const { return textures_.size(); }
    std::uint64_t logical_bytes() const { return logical_bytes_; }
    static std::size_t live_texture_count();
private:
    SDL_Renderer* renderer_=nullptr;
    std::vector<SDL_Texture*> textures_;
    std::uint64_t logical_bytes_=0;
    std::optional<FireSpriteBounds> clip_bounds_;
};

} // namespace openemperor
