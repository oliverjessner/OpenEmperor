#include "renderer/WellFallbackRenderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <limits>

namespace openemperor {
namespace {
struct Point { float x,y; };
struct Palette {
    SDL_Color ground,inner,water,stone_dark,stone_mid,stone_light,mortar,
        wood_dark,wood_mid,wood_light,rope;
};
constexpr Palette palette{
    {78,76,53,255},{51,49,38,255},{55,66,65,255},
    {103,98,78,255},{133,124,100,255},{157,146,119,255},{87,81,63,255},
    {76,57,38,255},{102,77,48,255},{120,93,60,255},{132,116,81,255}};

// Fixed stack storage; faces are convex and kept in back-to-front order.
struct Mesh {
    std::array<SDL_Vertex,192> vertices{};
    std::array<int,320> indices{};
    int vertex_count=0,index_count=0;

    template<std::size_t N>
    constexpr void face(const std::array<Point,N>& points,SDL_Color color) {
        static_assert(N>=3);
        const int base=vertex_count;
        const SDL_FColor tint{color.r/255.0F,color.g/255.0F,color.b/255.0F,1};
        for (const auto p:points) vertices[static_cast<std::size_t>(vertex_count++)]={
            {p.x,p.y},tint,{0,0}};
        for (int i=1;i<static_cast<int>(N)-1;++i) {
            indices[static_cast<std::size_t>(index_count++)]=base;
            indices[static_cast<std::size_t>(index_count++)]=base+i;
            indices[static_cast<std::size_t>(index_count++)]=base+i+1;
        }
    }
    constexpr void quad(Point a,Point b,Point c,Point d,SDL_Color color) {
        face(std::array{a,b,c,d},color);
    }
};

constexpr void stone_ring(Mesh& mesh) {
    constexpr std::array<Point,8> outer{{{-7,-15},{8,-14},{18,-9},{19,-4},
        {7,3},{-8,2},{-19,-3},{-18,-9}}};
    constexpr std::array<Point,8> inner{{{-4,-11},{4,-11},{10,-8},{10,-5},
        {4,-2},{-4,-2},{-10,-5},{-10,-8}}};
    // Low front masonry; no rectangular plinth or cylindrical ellipse stack.
    for (std::size_t i=2;i<7;++i) {
        const auto a=outer[i],b=outer[(i+1)%8];
        mesh.quad(a,b,{b.x,b.y+6},{a.x,a.y+6},i<4 ? palette.stone_dark:palette.stone_mid);
    }
    mesh.face(outer,palette.mortar);
    mesh.face(inner,palette.inner);
    mesh.face(std::array<Point,6>{{{-4,-8},{3,-8},{7,-6},{3,-4},{-4,-4},{-7,-6}}},palette.water);
    // Slightly inset stones leave restrained joints in the octagonal rim.
    for (std::size_t i=0;i<8;++i) {
        const std::size_t j=(i+1)%8;
        const auto inset=[](Point a,Point b) { return Point{a.x+(b.x-a.x)*0.04F,a.y+(b.y-a.y)*0.04F}; };
        mesh.quad(inset(outer[i],outer[j]),inset(outer[j],outer[i]),
            inset(inner[j],inner[i]),inset(inner[i],inner[j]),
            i==0 || i>=5 ? palette.stone_light:palette.stone_mid);
    }
    // Chipped edges and a few uneven stone planes break up the large faces.
    // These are authored fixed facets, not sampled pixels or procedural noise.
    mesh.quad({-14,-10},{-10,-12},{-9,-11},{-13,-9},palette.stone_mid);
    mesh.quad({-5,-14},{0,-14},{-1,-13},{-5,-13},palette.stone_mid);
    mesh.quad({7,-13},{12,-11},{11,-10},{7,-12},palette.stone_light);
    mesh.quad({15,-8},{17,-7},{16,-6},{14,-7},palette.stone_dark);
    mesh.quad({10,-1},{14,-3},{14,-2},{10,0},palette.stone_light);
    mesh.quad({0,0},{4,0},{3,1},{0,1},palette.stone_mid);
    mesh.quad({-13,-2},{-9,-1},{-10,0},{-14,-1},palette.stone_mid);
    mesh.quad({-18,-6},{-15,-7},{-14,-6},{-17,-5},palette.stone_mid);
    mesh.quad({-14,1},{-10,2},{-10,3},{-14,2},palette.stone_light);
    mesh.quad({-5,4},{-1,4.5F},{-2,6},{-5,5.5F},palette.stone_dark);
    mesh.quad({2,6},{6,6.5F},{6,7.5F},{2,7},palette.stone_light);
    mesh.quad({13,2},{16,0.5F},{16,1.5F},{13,3},palette.stone_mid);
    // Two small staggered joints on the visible wall, rather than a uniform grid.
    mesh.quad({-8,3},{-7,3.2F},{-7,7.5F},{-8,7.2F},palette.mortar);
    mesh.quad({11,4},{12,3.5F},{12,6.8F},{11,7.3F},palette.mortar);
}

constexpr Mesh make_well_mesh() {
    Mesh mesh;
    mesh.face(std::array<Point,4>{{{-21,0},{-7,-10},{21,0},{8,10}}},palette.ground);
    {
        // Rear support leans slightly; beam follows the cell's isometric axis.
        mesh.quad({-10,-29},{-8,-28},{-7,-7},{-9,-8},palette.wood_dark);
        mesh.quad({-10,-29},{-9,-29.5F},{-8,-8},{-9,-8},palette.wood_mid);
    }
    stone_ring(mesh);
    {
        mesh.quad({12,-19},{14,-18},{13,2},{11,1},palette.wood_dark);
        mesh.quad({12,-19},{13,-19.5F},{12,1.5F},{11,1},palette.wood_mid);
        mesh.quad({-13,-29},{-10,-31},{17,-17},{15,-15},palette.wood_mid);
        mesh.quad({-13,-29},{15,-15},{15,-13.5F},{-13,-27.5F},palette.wood_dark);
        mesh.quad({-10,-31},{17,-17},{16,-16.5F},{-11,-30.5F},palette.wood_light);
        // Short, low-contrast grain facets, with no uniform stripe pattern.
        mesh.quad({-7,-27},{-1,-24},{-1,-23.5F},{-7,-26.5F},palette.wood_dark);
        mesh.quad({5,-21},{10,-18.5F},{9,-18.3F},{5,-20.5F},palette.wood_light);
        mesh.quad({12,-10},{12.7F,-9.6F},{12.3F,-4},{11.8F,-4.4F},palette.wood_light);
        mesh.quad({1,-23},{2,-22.5F},{1.5F,-6},{0.5F,-6.5F},palette.rope);
    }
    return mesh;
}
constexpr Mesh well_mesh=make_well_mesh();
static_assert(well_mesh.vertex_count==178 && well_mesh.index_count==282);
static_assert([] {
    for (int i=0;i<well_mesh.vertex_count;++i) {
        const auto p=well_mesh.vertices[static_cast<std::size_t>(i)].position;
        if (p.x < -21 || p.x > 21 || p.y < -31 || p.y > 10) return false;
    }
    return true;
}());
}

bool draw_well_fallback(SDL_Renderer* renderer,scene::Point ground,double zoom,bool placement_preview,int footprint_side) {
    // Check before any transform or SDL submission, including float overflow.
    constexpr double limit=static_cast<double>(std::numeric_limits<float>::max())/4;
    if (!renderer || !std::isfinite(ground.x) || !std::isfinite(ground.y) ||
        !std::isfinite(zoom) || zoom<=0 || std::abs(ground.x)>limit ||
        std::abs(ground.y)>limit || zoom>limit/128 || (footprint_side!=1 && footprint_side!=2))
        return SDL_SetError("Well fallback requires finite, representable ground and positive zoom");
    ground.y-=20.0*zoom*(footprint_side-1);
    zoom*=footprint_side;
    auto vertices=well_mesh.vertices; // Fixed stack copy; indices and shape are constant.
    for (int i=0;i<well_mesh.vertex_count;++i) {
        auto& vertex=vertices[static_cast<std::size_t>(i)];
        vertex.position={static_cast<float>(ground.x+vertex.position.x*zoom),
                         static_cast<float>(ground.y+vertex.position.y*zoom)};
        vertex.color.a=placement_preview ? 128.0F/255.0F:1.0F;
    }
    SDL_BlendMode previous;
    if (!SDL_GetRenderDrawBlendMode(renderer,&previous) ||
        !SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND)) return false;
    const bool drawn=SDL_RenderGeometry(renderer,nullptr,vertices.data(),well_mesh.vertex_count,
                                        well_mesh.indices.data(),well_mesh.index_count);
    const bool restored=SDL_SetRenderDrawBlendMode(renderer,previous);
    return drawn && restored;
}
bool hit_well_fallback(scene::Point screen,scene::Point ground,double zoom,int footprint_side) {
    constexpr double limit=static_cast<double>(std::numeric_limits<float>::max())/4;
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
        !std::isfinite(ground.x) || !std::isfinite(ground.y) || !std::isfinite(zoom) ||
        zoom<=0 || std::abs(ground.x)>limit || std::abs(ground.y)>limit ||
        zoom>limit/128 || (footprint_side!=1 && footprint_side!=2)) return false;
    ground.y-=20.0*zoom*(footprint_side-1);
    zoom*=footprint_side;
    const auto vertex=[&](int index) {
        const auto p=well_mesh.vertices[static_cast<std::size_t>(index)].position;
        return scene::Point{static_cast<float>(ground.x+p.x*zoom),
                            static_cast<float>(ground.y+p.y*zoom)};
    };
    const auto cross=[](scene::Point a,scene::Point b,scene::Point p) {
        return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
    };
    for (int i=0;i<well_mesh.index_count;i+=3) {
        const auto a=vertex(well_mesh.indices[static_cast<std::size_t>(i)]);
        const auto b=vertex(well_mesh.indices[static_cast<std::size_t>(i+1)]);
        const auto c=vertex(well_mesh.indices[static_cast<std::size_t>(i+2)]);
        if (cross(a,b,c)==0) continue;
        const auto ab=cross(a,b,screen),bc=cross(b,c,screen),ca=cross(c,a,screen);
        if ((ab>=0 && bc>=0 && ca>=0) || (ab<=0 && bc<=0 && ca<=0)) return true;
    }
    return false;
}
} // namespace openemperor
