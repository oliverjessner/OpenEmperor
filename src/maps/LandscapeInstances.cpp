#include "maps/LandscapeInstances.h"
#include "maps/TerrainRenderPlan.h"
#include <stdexcept>

namespace openemperor::maps {
namespace {
constexpr std::size_t count=stored_grid_width*stored_grid_height;
std::size_t index(GridCell c) { return std::size_t(c.y)*stored_grid_width+c.x; }
std::optional<std::uint32_t> rock_kind(std::uint32_t t) {
    if (t&0x100U) return {}; // Earlier flood branch owns even a singleton.
    const auto kind=t&0xaffede6fU;
    if (kind==2U || kind==0x100002U || kind==0x200002U) return kind;
    return {};
}
}
std::vector<LandscapeInstanceSpec> derive_rock_instances(
    const LandscapeSelectorInput& input, std::span<const std::uint8_t> eligible) {
    std::vector<LandscapeInstanceSpec> result;
    if (input.terrain.size()!=count || input.variation.size()!=count || eligible.size()!=count ||
        input.orientation!=0) return result;
    std::vector<std::uint8_t> claimed(count,0);
    for (unsigned y=0;y<stored_grid_height;++y) for (unsigned x=0;x<stored_grid_width;++x) {
        const GridCell origin{x,y};const auto at=index(origin);
        const auto kind=rock_kind(input.terrain[at]);
        if (!eligible[at] || claimed[at] || !kind) continue;
        const auto fits=[&](unsigned side) {
            if (x+side>stored_grid_width || y+side>stored_grid_height) return false;
            for (unsigned dy=0;dy<side;++dy) for (unsigned dx=0;dx<side;++dx) {
                const auto cell=index({x+dx,y+dy});
                if (!eligible[cell] || claimed[cell] ||
                    (input.terrain[cell]&0xaffede6fU)!=*kind || (input.terrain[cell]&0x100U)) return false;
            }
            return true;
        };
        const unsigned side=fits(3)?3U:fits(2)?2U:1U;
        const auto variation=input.variation[at];
        const unsigned variant=side==3 ? 12U+(variation&1U):side==2 ? 8U+(variation&3U):(variation&7U);
        const unsigned key=*kind==0x100002U ? 0x607U:*kind==0x200002U ? 0x608U:0x606U;
        LandscapeInstanceSpec next;
        next.selection={LandscapeFamily::Rock,SelectorEvidence::Verified,"53f660 / 4b8000 / 4b72b0",
            "EXE-OBSERVED: raw rock family, size priority, variation and unique occupancy",{key},variant,{}};
        next.origin=origin;next.side=side;
        // The writer marks the west/front edge in view zero, not the diagonal
        // front cell used by our painter convention (4b72c8,4b7434).
        next.draw_cell={x,y+side-1};
        for (unsigned dy=0;dy<side;++dy) for (unsigned dx=0;dx<side;++dx) {
            const GridCell cell{x+dx,y+dy};claimed[index(cell)]=1;next.owned_cells.push_back(cell);
        }
        result.push_back(std::move(next));
    }
    return result;
}
scene::Point regenerated_instance_origin(const LandscapeInstanceSpec& instance,
    std::uint32_t border,unsigned width,unsigned height,int elevation) {
    if ((instance.side!=1 && instance.side!=2 && instance.side!=3 && instance.side!=4 && instance.side!=5) ||
        width!=80U*instance.side-2U || height<40U*instance.side)
        throw std::invalid_argument("unsupported regenerated Emperor footprint geometry");
    auto p=terrain_world(instance.origin,border);
    p.x-=double(width)/2.0;
    p.y-=double(height)-40.0*instance.side;
    p.y-=40.0*elevation;
    return p;
}
} // namespace openemperor::maps
