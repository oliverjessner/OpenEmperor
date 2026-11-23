#pragma once

#include "app/AgricultureActorPresentation.h"
#include "scene/IsoProjection.h"
#include "simulation/World.h"
#include <array>
#include <cstddef>
#include <cstdint>

struct SDL_Renderer;

namespace openemperor {

// Independently authored geometry. Build only at a World publication boundary;
// rendering and picking consume the same immutable triangles without assets.
struct AgricultureTriangle {
    std::array<scene::Point,3> points{};
    std::array<std::uint8_t,4> color{};
};
struct AgricultureMesh {
    static constexpr std::size_t triangle_limit=128;
    std::array<AgricultureTriangle,triangle_limit> triangles{};
    std::size_t count=0;
    double left=0,top=0,width=0,height=0;
};
AgricultureMesh prepare_agriculture_actor_mesh(const AgricultureActorPose&);
AgricultureMesh prepare_agriculture_building_mesh(simulation::Object,int footprint_side,
                                                 unsigned actual_work_phase=0,bool supplied=false);
AgricultureMesh prepare_agriculture_crop_mesh(simulation::Object,unsigned progress,
                                             unsigned target,bool wet);
bool draw_agriculture_mesh(SDL_Renderer*,const AgricultureMesh&,scene::Point ground,
                           double zoom,bool preview=false);
// Optional local tolerance represents at most one displayed pixel per axis, not
// a camera/anchor offset. Native image alpha picking is independent and exact.
bool hit_agriculture_mesh(const AgricultureMesh&,scene::Point local,double tolerance=0);

} // namespace openemperor
