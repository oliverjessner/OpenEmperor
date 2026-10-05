#pragma once

#include <SDL3/SDL.h>
#include <string_view>

namespace openemperor::texture_compatibility {

// Unchanged, eagerly uploaded RGBA sprites reused in scaled/clipped copies.
// SDL 3.4.14 software STATIC textures can stop writing pixels after scaling;
// STREAMING avoids the independently reproduced failure with one eager upload.
// Only this measured backend/runtime is admitted. This is not a claim about
// other SDL versions, and does not change native Metal or texture lifetimes.
// Evidence and scope: docs/rendering/texture-compatibility.md.
constexpr SDL_TextureAccess eager_rgba_access_for(std::string_view backend,
                                                  int runtime_version) {
    return backend=="software" && runtime_version==SDL_VERSIONNUM(3,4,14) ?
        SDL_TEXTUREACCESS_STREAMING:SDL_TEXTUREACCESS_STATIC;
}

inline SDL_TextureAccess eager_rgba_access(SDL_Renderer* renderer) {
    const auto* name=SDL_GetRendererName(renderer);
    return eager_rgba_access_for(name ? std::string_view{name}:std::string_view{},
                                SDL_GetVersion());
}

} // namespace openemperor::texture_compatibility
