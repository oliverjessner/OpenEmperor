#pragma once
#include "scene/IsoProjection.h"
struct SDL_Renderer;
namespace openemperor {
// Authored small timber treatment pavilion; presentation only, no asset work.
bool draw_health_post_fallback(SDL_Renderer*,scene::Point ground,double zoom,
                               bool placement_preview=false,int footprint_side=1);
}
