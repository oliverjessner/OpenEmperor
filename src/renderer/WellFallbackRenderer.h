#pragma once

#include "scene/IsoProjection.h"

struct SDL_Renderer;

namespace openemperor {

// Authored presentation only. Ground is the existing projected building ground;
// this helper has no assets, World, coverage or gameplay state.
bool draw_well_fallback(SDL_Renderer* renderer,scene::Point ground,double zoom,
                        bool placement_preview=false);

} // namespace openemperor
