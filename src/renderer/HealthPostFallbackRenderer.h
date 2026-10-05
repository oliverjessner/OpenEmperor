#pragma once
#include "scene/IsoProjection.h"
struct SDL_Renderer;
namespace openemperor {
// Authored small timber treatment pavilion; presentation only, no asset work.
bool draw_health_post_fallback(SDL_Renderer*,scene::Point ground,double zoom,
                               bool placement_preview=false,int footprint_side=1);
// Read-only hit mask from the same opaque mesh triangles and draw transform.
bool hit_health_post_fallback(scene::Point screen,scene::Point ground,double zoom,
                              int footprint_side=1);
}
