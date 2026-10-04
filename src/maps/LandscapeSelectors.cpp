#include "maps/LandscapeSelectors.h"
#include <bit>
#include <string_view>

namespace openemperor::maps {
namespace {
constexpr std::size_t count=stored_grid_width*stored_grid_height;
struct ShoreRule {
    std::uint8_t required_land, required_water;
    std::array<std::uint8_t,4> offsets;
    std::uint8_t variants;
};
// Semantic constraints in N,NE,E,SE,S,SW,W,NW bit order. Row priority and
// four orientation offsets are EXE-observed; this is not the binary row format.
// See docs/reverse/map-first-draw.md, Pass 2. No cursor/global mutable state.
constexpr std::array<ShoreRule,46> shore_rules{{
    {0x55,0x00,{71,71,71,71},1}, // row 1
    {0x15,0x40,{39,38,37,36},1}, // row 2
    {0x54,0x01,{36,39,38,37},1}, // row 3
    {0x51,0x04,{37,36,39,38},1}, // row 4
    {0x45,0x10,{38,37,36,39},1}, // row 5
    {0x11,0x44,{32,34,32,34},2}, // row 6
    {0x44,0x11,{34,32,34,32},2}, // row 7
    {0x05,0x70,{24,20,16,28},4}, // row 8
    {0x14,0xc1,{28,24,20,16},4}, // row 9
    {0x50,0x07,{16,28,24,20},4}, // row 10
    {0x41,0x1c,{20,16,28,24},4}, // row 11
    {0x25,0x50,{69,68,67,70},1}, // row 12
    {0x94,0x41,{70,69,68,67},1}, // row 13
    {0x52,0x05,{67,70,69,68},1}, // row 14
    {0x49,0x14,{68,67,70,69},1}, // row 15
    {0x01,0x7c,{8,4,0,12},4}, // row 16
    {0x04,0xf1,{12,8,4,0},4}, // row 17
    {0x10,0xc7,{0,12,8,4},4}, // row 18
    {0x40,0x1f,{4,0,12,8},4}, // row 19
    {0x09,0x74,{61,58,55,64},1}, // row 20
    {0x24,0xd1,{64,61,58,55},1}, // row 21
    {0x90,0x47,{55,64,61,58},1}, // row 22
    {0x42,0x1d,{58,55,64,61},1}, // row 23
    {0x21,0x5c,{62,59,56,65},1}, // row 24
    {0x84,0x71,{65,62,59,56},1}, // row 25
    {0x12,0xc5,{56,65,62,59},1}, // row 26
    {0x48,0x17,{59,56,65,62},1}, // row 27
    {0x29,0x54,{63,60,57,66},1}, // row 28
    {0xa4,0x51,{66,63,60,57},1}, // row 29
    {0x92,0x45,{57,66,63,60},1}, // row 30
    {0x4a,0x15,{60,57,66,63},1}, // row 31
    {0xaa,0x55,{54,54,54,54},1}, // row 32
    {0x2a,0xd5,{52,51,50,53},1}, // row 33
    {0xa8,0x57,{53,52,51,50},1}, // row 34
    {0xa2,0x5d,{50,53,52,51},1}, // row 35
    {0x8a,0x75,{51,50,53,52},1}, // row 36
    {0x22,0xdd,{40,41,40,41},1}, // row 37
    {0x88,0x77,{41,40,41,40},1}, // row 38
    {0x0a,0xf5,{48,47,46,49},1}, // row 39
    {0x28,0xd7,{49,48,47,46},1}, // row 40
    {0xa0,0x5f,{46,49,48,47},1}, // row 41
    {0x82,0x7d,{47,46,49,48},1}, // row 42
    {0x02,0xfd,{44,43,42,45},1}, // row 43
    {0x08,0xf7,{45,44,43,42},1}, // row 44
    {0x20,0xdf,{42,45,44,43},1}, // row 45
    {0x80,0x7f,{43,42,45,44},1}, // row 46
}};
constexpr std::array<int,8> dx{0,1,1,1,0,-1,-1,-1};
constexpr std::array<int,8> dy{-1,-1,0,1,1,1,0,-1};
std::uint32_t raw(std::span<const std::uint32_t> terrain,int x,int y) {
    if (terrain.size()!=count || x<0 || y<0 || x>=int(stored_grid_width) || y>=int(stored_grid_height)) return 0;
    return terrain[std::size_t(y)*stored_grid_width+std::size_t(x)];
}
bool possible_rock_square(std::span<const std::uint32_t> terrain,GridCell cell,std::uint32_t rock) {
    for (unsigned y=0;y<2;++y) for (unsigned x=0;x<2;++x) {
        if (cell.x+x>=stored_grid_width || cell.y+y>=stored_grid_height ||
            (raw(terrain,int(cell.x+x),int(cell.y+y))&0xaffede6fU)!=rock) return false;
    }
    return true;
}
LandscapeSelection result(LandscapeFamily family,const char* selector,unsigned key,unsigned variant,
    SelectorEvidence evidence=SelectorEvidence::Verified,const char* reason="EXE-observed static selector") {
    return {family,evidence,selector,reason,{key},variant,{}};
}
}
std::uint8_t WaterNeighborhood::mask() const {
    const std::array<bool,8> values{n,ne,e,se,s,sw,w,nw};unsigned result=0;
    for (unsigned i=0;i<values.size();++i) if (values[i]) result|=1U<<i;
    return static_cast<std::uint8_t>(result);
}
WaterNeighborhood terrain_neighborhood(std::span<const std::uint32_t> terrain,GridCell cell,std::uint32_t bits) {
    if (cell.x>=stored_grid_width || cell.y>=stored_grid_height) return {};
    std::array<bool,8> v{};
    for (unsigned i=0;i<8;++i) v[i]=(raw(terrain,int(cell.x)+dx[i],int(cell.y)+dy[i])&bits)!=0;
    return {v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]};
}
std::optional<WaterMatch> match_water(WaterNeighborhood water,unsigned orientation,std::uint8_t variation) {
    if (orientation>=4) return std::nullopt;
    const auto wet=water.mask(); const auto land=static_cast<std::uint8_t>(~wet);
    for (unsigned i=0;i<shore_rules.size();++i) {
        const auto& r=shore_rules[i];
        if ((land&r.required_land)==r.required_land && (wet&r.required_water)==r.required_water)
            return WaterMatch{static_cast<std::uint8_t>(i+1),r.offsets[orientation],r.variants,
                static_cast<std::uint8_t>(variation%r.variants)};
    }
    return std::nullopt;
}
const char* selector_evidence_name(SelectorEvidence e) {
    switch(e) {case SelectorEvidence::Verified:return "verified_selector";
    case SelectorEvidence::Preview:return "preview_context_unresolved";
    case SelectorEvidence::Unresolved:return "unresolved";} return "invalid";
}
const char* landscape_family_name(LandscapeFamily f) {
    switch(f) {case LandscapeFamily::Ground:return "ground";case LandscapeFamily::Water:return "water";
    case LandscapeFamily::Decoration:return "decoration";case LandscapeFamily::Preserved:return "preserved";}return "invalid";
}
bool LandscapeSelection::operator==(const LandscapeSelection& r) const {
    return family==r.family && evidence==r.evidence && std::string_view(selector)==r.selector &&
        std::string_view(reason)==r.reason && group.value==r.group.value && variant==r.variant && water_match==r.water_match;
}
LandscapeSelection select_landscape(const LandscapeSelectorInput& in,GridCell cell) {
    if (in.terrain.size()!=count || in.objects.size()!=count || in.variation.size()!=count ||
        cell.x>=stored_grid_width || cell.y>=stored_grid_height || in.orientation>=4) return {};
    const auto index=std::size_t(cell.y)*stored_grid_width+cell.x;
    const auto t=in.terrain[index]; const auto v=in.variation[index];
    if (t&0x8c008U) return result(LandscapeFamily::Preserved,"53ec90 early exclusion",0,0,
        SelectorEvidence::Unresolved,"original early exclusion; historical preview preserved");
    if (t&4U) {
        const auto ordinary=terrain_neighborhood(in.terrain,cell,4);
        const auto contextual=terrain_neighborhood(in.terrain,cell,0x104);
        const bool context_possible=contextual.mask()==255;
        const bool context=context_possible && in.flood_context.value_or(false);
        auto m=match_water(context ? contextual:ordinary,in.orientation,v);
        auto r=result(LandscapeFamily::Water,"53fc30 / 46-row shore matcher",0x605,0);
        r.water_match=m;
        if (context) {r.group={0x61c};r.variant=48+v%24;}
        else if ((t&0x4000000U) && !(t&0x100U)) {
            r.group={0x61c};r.variant=terrain_neighborhood(in.terrain,cell,0x4000000).mask()==255 ? 48+v%24:v%48;
        } else if (ordinary.mask()==255) {r.group={0x61c};r.variant=v%24+(v&1U);}
        else if (m) {r.variant=m->orientation_offset+m->variant;}
        else {r.evidence=SelectorEvidence::Unresolved;r.reason="no shoreline row matched";return r;}
        if (!context && (t&0x10000U)) {
            r.group={0x61a};
            if (!m) {r.evidence=SelectorEvidence::Unresolved;r.reason="special shoreline requires a matching row";return r;}
            r.variant=m->orientation_offset+m->variant;
        } else if (!context && terrain_neighborhood(in.terrain,cell,0x10000).mask()!=0) {
            r.evidence=SelectorEvidence::Unresolved;r.group={0x61b};r.reason="61a/61b transient per-neighbour +0x18 context unresolved";return r;
        }
        if (context_possible && !in.flood_context && !(r.group.value==0x61c && r.variant==48+v%24)) {
            r.evidence=SelectorEvidence::Preview;r.reason="static ordinary-water phase; original 425250 transient context unknown";
        }
        return r;
    }
    if ((t&0x104U)==0x100U) return result(LandscapeFamily::Ground,"flood branch",0,0,
        SelectorEvidence::Unresolved,"flood is distinct from water; transient flood selector unresolved");
    if (t&1U) return result(LandscapeFamily::Decoration,"53f520 / 4bc6a0 vegetation",(in.objects[index]&2U)?0x625:0x624,v%9,
        SelectorEvidence::Unresolved,"post-load stage reset, ring bank and occupancy dependencies unresolved");
    if ((t&0xaffede6fU)==2U || (t&0xaffede6fU)==0x100002U || (t&0xaffede6fU)==0x200002U) {
        const auto rock=t&0x300002U;const unsigned key=rock==0x100002U?0x607:rock==0x200002U?0x608:0x606;
        return result(LandscapeFamily::Decoration,"53f660 rock",key,v&7U,
            possible_rock_square(in.terrain,cell,rock)?SelectorEvidence::Unresolved:SelectorEvidence::Verified,
            possible_rock_square(in.terrain,cell,rock)?"2x2/3x3 occupancy packing unresolved; historical footprint retained":"larger rock rectangles rejected by raw terrain; singleton selector");
    }
    if (t&0x40000U) return result(LandscapeFamily::Ground,"5400f0 / 4bc800 marsh",0x620,v%9,
        SelectorEvidence::Unresolved,"marsh is distinct from water and flood; post-load stage dependencies unresolved");
    if (t==0x80U) {
        if (v<8) return result(LandscapeFamily::Ground,"53ee00 fertile multi-cell branch",0x604,0,
            SelectorEvidence::Unresolved,"4x4/3x3/2x2 packing and first-pass occupancy unresolved");
        if (terrain_neighborhood(in.terrain,cell,0x10000).mask()!=0) return result(LandscapeFamily::Ground,
            "53ee00 special-ground adjacency",0x61b,0,SelectorEvidence::Unresolved,"26-row special-ground table and cyclic variation unresolved");
        if (in.fertility.size()!=count || in.fertility[index]>100 || in.flood_context.value_or(false))
            return result(LandscapeFamily::Ground,"53ee00 fertile context",0,0,SelectorEvidence::Unresolved,
                "missing/out-of-range fertility operand or active transient context");
        const int scaled=in.fertility[index]/10;
        int band=scaled/2;
        if (scaled==10 || (!(scaled&1) && (v&1U))) --band;
        if (band<0) band=0;
        return result(LandscapeFamily::Ground,"53ee00 / 4bd8c0 fertile",0x602,unsigned(band)*9+v%9,
            in.flood_context ? SelectorEvidence::Verified:SelectorEvidence::Preview,
            in.flood_context ? "inactive context, serialized fertility operand and variation":"ordinary ground preview; original 425250 transient context unknown");
    }
    if (t==0 && v>=8) return result(LandscapeFamily::Ground,"53f090 ordinary dry singleton",0x603,v&7U);
    return result(LandscapeFamily::Preserved,"53ec90 later branches",0,0,SelectorEvidence::Unresolved,
        "sand/beach/garden/structure/elevation or multi-cell path not reconstructed");
}
} // namespace openemperor::maps
