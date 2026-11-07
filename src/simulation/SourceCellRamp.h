#pragma once
#include "simulation/MapPermissions.h"
#include <algorithm>
#include <cmath>

namespace openemperor::simulation {
struct SourceCellRampId {
    std::uint32_t value=0;
    auto operator<=>(const SourceCellRampId&) const = default;
};
inline constexpr std::size_t source_cell_ramp_max_links=64;
inline constexpr std::int64_t source_cell_ramp_cost=40;
inline constexpr const char* authored_source_cell_ramp_contract="authored-source-cell-ramps-v1";
// Explicit authored surface. Original raw heights and policy3 remain immutable.
struct SourceCellRamp {
    SourceCellRampId id{};
    Cell source_cell{},high_neighbor{};
    std::int8_t low_height=0,high_height=0;
    std::int64_t paid_cost=0;
    std::uint64_t placed_tick=0;
    bool operator==(const SourceCellRamp&) const = default;
};
struct SourceCellRampValidation {
    bool geometry_valid=false,accepted=false;
    std::string reason;
    BuildDiagnostic diagnostic{};
    SourceCellRamp ramp{};
};
// Pure height on both axial centre-to-centre edges: flat approach, one-cell
// slope, flat departure. Callers supply a World-validated bounded record.
inline double source_cell_ramp_axial_height(const SourceCellRamp& ramp,double along) {
    return double(ramp.low_height)+std::clamp(along+.5,0.,1.);
}
inline double source_cell_ramp_height_at(const SourceCellRamp& ramp,double x,double y) {
    const double dx=double(ramp.high_neighbor.x)-ramp.source_cell.x;
    const double dy=double(ramp.high_neighbor.y)-ramp.source_cell.y;
    return source_cell_ramp_axial_height(ramp,(x-ramp.source_cell.x)*dx+(y-ramp.source_cell.y)*dy);
}
} // namespace openemperor::simulation
