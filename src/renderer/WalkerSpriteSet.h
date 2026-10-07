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
    void initialize(SDL_Renderer* renderer,const assets::WalkerVisualProfile& profile,
                    std::uint64_t available_total_rgba_bytes=assets::walker_max_rgba_bytes);
    // The prepared profile must retain the existing image prefix. Only its new
    // tail is uploaded; failure releases that tail and retains the active core.
    void append(const assets::WalkerVisualProfile& merged_profile,
                std::uint64_t available_total_rgba_bytes=assets::walker_max_rgba_bytes);
    // Releases an optional image tail without recreating the retained prefix.
    void truncate(std::size_t image_count);
    void shutdown();
    bool draw(std::size_t frame,scene::Point ground,double zoom,
              const assets::WalkerRoleVisual& role_visual,
              const assets::WalkerVisualProfile& profile,
              scene::Point clip_min,scene::Point clip_max) const;
    std::size_t texture_count() const { return textures_.size(); }
    std::uint64_t logical_bytes() const { return logical_bytes_; }
    static std::size_t live_texture_count();
private:
    SDL_Renderer* renderer_=nullptr;
    std::vector<SDL_Texture*> textures_;
    std::vector<std::uint64_t> texture_bytes_;
    std::uint64_t logical_bytes_=0;
};
}
