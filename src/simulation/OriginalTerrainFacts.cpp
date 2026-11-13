#include "simulation/OriginalTerrainFacts.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {

OriginalTerrainFacts::OriginalTerrainFacts(std::span<const std::uint32_t> terrain,
                                         std::string source_map_sha256)
    : source_map_sha256_(std::move(source_map_sha256)) {
    if (terrain.size()!=cell_count || source_map_sha256_.size()!=64 ||
        !std::all_of(source_map_sha256_.begin(),source_map_sha256_.end(),[](char c) {
            return (c>='0' && c<='9') || (c>='a' && c<='f');
        })) throw std::invalid_argument("cardinal-water facts require complete 228x228 terrain and a source-map SHA-256");
    for (const int side:{2,4}) {
        auto& mask=side==2 ? side2_:side4_;
        for (int y=1;y+side<height;++y) for (int x=1;x+side<width;++x) {
            bool water=false;
            const auto test=[&](int xx,int yy) {
                water|=(terrain[std::size_t(yy)*std::size_t(width)+std::size_t(xx)]&0x4U)!=0;
            };
            for (int d=0;d<side;++d) {
                test(x+d,y-1);test(x+side,y+d);
                test(x+d,y+side);test(x-1,y+d);
            }
            mask[std::size_t(y)*std::size_t(width)+std::size_t(x)]=water ? 10:0;
        }
    }
}

OriginalTerrainFacts::OriginalTerrainFacts(std::span<const std::uint32_t> terrain,
    std::span<const std::uint8_t> fertility,std::string sha)
    : OriginalTerrainFacts(terrain,std::move(sha)) {
    if (fertility.size()!=cell_count || std::any_of(fertility.begin(),fertility.end(),[](auto f){return f>100;}))
        throw std::invalid_argument("Agriculture requires complete source fertility bytes in0..100");
    std::copy(fertility.begin(),fertility.end(),fertility_.begin());fertility_complete_=true;
}
std::uint8_t OriginalTerrainFacts::fertility(Cell cell) const {
    if (!fertility_complete_ || cell.x<0 || cell.y<0 || cell.x>=width || cell.y>=height)
        throw std::invalid_argument("Agriculture fertility has no complete bound source cell");
    return fertility_[std::size_t(cell.y)*width+std::size_t(cell.x)];
}

bool OriginalTerrainFacts::supports_cardinal_footprint(Cell origin,int side) const noexcept {
    return (side==2 || side==4) && origin.x>=1 && origin.y>=1 &&
        origin.x<width-side && origin.y<height-side;
}

int OriginalTerrainFacts::cardinal_water_bonus(Cell origin,int side) const {
    if (!supports_cardinal_footprint(origin,side))
        throw std::invalid_argument("cardinal-water House requires a complete side2/4 footprint and cardinal perimeter");
    return (side==2 ? side2_:side4_)[std::size_t(origin.y)*std::size_t(width)+std::size_t(origin.x)];
}

} // namespace openemperor::simulation
