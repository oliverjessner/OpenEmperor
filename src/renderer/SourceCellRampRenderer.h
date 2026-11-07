#pragma once

#include "renderer/BridgeRenderer.h"
#include "simulation/SourceCellRamp.h"
#include <cstddef>
#include <memory>
#include <span>

namespace openemperor::simulation { class World; }
namespace openemperor {

// Authored geometry on the complete low source cell, never a replacement for
// historical centre-to-centre RampLinks or an original sprite selector.
// along and side are storage-cell coordinates relative to the source centre.
scene::Point source_cell_ramp_surface_point(scene::Point source_low_ground,
    const simulation::SourceCellRamp&,double along,double side,bool elevated=true);

class SourceCellRampPresentation {
public:
    SourceCellRampPresentation();
    ~SourceCellRampPresentation();
    SourceCellRampPresentation(SourceCellRampPresentation&&) noexcept;
    SourceCellRampPresentation& operator=(SourceCellRampPresentation&&) noexcept;
    SourceCellRampPresentation(const SourceCellRampPresentation&)=delete;
    SourceCellRampPresentation& operator=(const SourceCellRampPresentation&)=delete;

    // Event/load-time only. Copies bounded DTOs, not World, and atomically
    // prepares opaque raster runs. The span overload supports authored tests.
    void prepare(const simulation::World&);
    void prepare(std::span<const simulation::SourceCellRamp>,std::size_t historical_links=0);
    void clear();
    bool prepared(simulation::SourceCellRampId) const;
    std::size_t geometry_bytes() const;
    std::size_t face_count() const;
    std::size_t maximum_part_runs() const;

    // source_low_ground uses the unchanged signed low-height projection.
    // Back/tread precede local actors; only the near bank follows them.
    // East/South planes are edge-on: the authored crest/banks remain visible
    // and pickable, without pretending to have an invertible broad tread.
    bool draw_part(SDL_Renderer*,const scene::Camera2D&,scene::Point source_low_ground,
        simulation::SourceCellRampId,bool front,bool elevated=true,
        BridgeTint=BridgeTint::Normal) const;
    // A validated/tinted preview selects the same eager orientation templates.
    // It does not bind an ID, rasterize, allocate or change the owned geometry.
    bool draw_part(SDL_Renderer*,const scene::Camera2D&,scene::Point source_low_ground,
        const simulation::SourceCellRamp&,bool front,bool elevated=true,
        BridgeTint=BridgeTint::Normal) const;
    bool hit_part(scene::Point world,scene::Point source_low_ground,
        simulation::SourceCellRampId,bool front,bool elevated=true) const;
    bool hit_part(scene::Point world,scene::Point source_low_ground,
        const simulation::SourceCellRamp&,bool front,bool elevated=true) const;
private:
    struct Data;
    std::unique_ptr<Data> data_;
};
} // namespace openemperor
