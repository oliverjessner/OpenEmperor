#include "renderer/HealthPostFallbackRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <limits>

namespace openemperor {
namespace {
struct Face { std::array<SDL_FPoint,4> points; SDL_Color color; };
// Quiet tiled roof, plaster/timber walls, shaded doorway and two herb planters.
constexpr std::array<Face,17> faces{{
    {{{{0,-16},{30,-1},{0,14},{-30,-1}}},{116,109,89,255}},
    {{{{-23,-6},{0,5},{0,-22},{-23,-33}}},{153,144,112,255}},
    {{{{0,5},{23,-6},{23,-33},{0,-22}}},{114,119,96,255}},
    {{{{-15,-10},{-7,-6},{-7,-25},{-15,-29}}},{64,61,50,255}},
    {{{{-3,3},{0,5},{0,-22},{-3,-24}}},{98,82,61,255}},
    {{{{12,-1},{19,-4},{19,-17},{12,-14}}},{73,84,70,255}},
    {{{{-29,-33},{-16,-48},{15,-32},{2,-17}}},{105,111,100,255}},
    {{{{-16,-48},{-3,-46},{28,-30},{15,-32}}},{79,88,80,255}},
    {{{{2,-17},{28,-30},{15,-32},{15,-32}}},{127,132,111,255}},
    {{{{-25,-37},{6,-21},{7,-22},{-24,-38}}},{87,95,87,255}},
    {{{{-21,-42},{10,-26},{11,-27},{-20,-43}}},{87,95,87,255}},
    {{{{-29,-33},{2,-17},{2,-13},{-29,-29}}},{72,78,72,255}},
    {{{{2,-17},{28,-30},{28,-26},{2,-13}}},{61,71,65,255}},
    {{{{-24,0},{-17,3},{-17,8},{-24,5}}},{117,89,65,255}},
    {{{{-24,0},{-20,-3},{-13,0},{-17,3}}},{83,104,74,255}},
    {{{{10,5},{16,2},{16,7},{10,10}}},{105,82,61,255}},
    {{{{10,5},{7,3},{13,0},{16,2}}},{79,97,70,255}}
}};
constexpr auto mesh=[] {
    std::array<SDL_Vertex,faces.size()*4> v{};
    for (std::size_t i=0;i<faces.size();++i) for (std::size_t p=0;p<4;++p) {
        const auto c=faces[i].color;
        v[i*4+p]={faces[i].points[p],{c.r/255.F,c.g/255.F,c.b/255.F,1.F},{0,0}};
    }
    return v;
}();
constexpr auto indices=[] {
    std::array<int,faces.size()*6> out{};
    for (std::size_t i=0;i<faces.size();++i) {
        const int n=static_cast<int>(4*i);
        out[6*i]=n;out[6*i+1]=n+1;out[6*i+2]=n+2;
        out[6*i+3]=n;out[6*i+4]=n+2;out[6*i+5]=n+3;
    }
    return out;
}();
}
bool draw_health_post_fallback(SDL_Renderer* renderer,scene::Point ground,double zoom,bool preview,int footprint_side) {
    constexpr double limit=static_cast<double>(std::numeric_limits<float>::max())/4;
    if (!renderer || !std::isfinite(ground.x) || !std::isfinite(ground.y) ||
        !std::isfinite(zoom) || zoom<=0 || std::abs(ground.x)>limit ||
        std::abs(ground.y)>limit || zoom>limit/128 || (footprint_side!=1 && footprint_side!=2))
        return SDL_SetError("Health Post requires finite ground and positive representable zoom");
    ground.y-=20.0*zoom*(footprint_side-1);
    zoom*=footprint_side;
    auto vertices=mesh;
    for (auto& v:vertices) {
        v.position={static_cast<float>(ground.x+v.position.x*zoom),
                    static_cast<float>(ground.y+v.position.y*zoom)};
        v.color.a=preview ? 128.F/255.F:1.F;
    }
    SDL_BlendMode previous;
    if (!SDL_GetRenderDrawBlendMode(renderer,&previous) ||
        !SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND)) return false;
    const bool drawn=SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),
        indices.data(),static_cast<int>(indices.size()));
    const bool restored=SDL_SetRenderDrawBlendMode(renderer,previous);
    return drawn && restored;
}
bool hit_health_post_fallback(scene::Point screen,scene::Point ground,double zoom,int footprint_side) {
    constexpr double limit=static_cast<double>(std::numeric_limits<float>::max())/4;
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
        !std::isfinite(ground.x) || !std::isfinite(ground.y) || !std::isfinite(zoom) ||
        zoom<=0 || std::abs(ground.x)>limit || std::abs(ground.y)>limit ||
        zoom>limit/128 || (footprint_side!=1 && footprint_side!=2)) return false;
    ground.y-=20.0*zoom*(footprint_side-1);
    zoom*=footprint_side;
    const auto vertex=[&](int index) {
        const auto p=mesh[static_cast<std::size_t>(index)].position;
        return scene::Point{static_cast<float>(ground.x+p.x*zoom),
                            static_cast<float>(ground.y+p.y*zoom)};
    };
    const auto cross=[](scene::Point a,scene::Point b,scene::Point p) {
        return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
    };
    for (std::size_t i=0;i<indices.size();i+=3) {
        const auto a=vertex(indices[i]),b=vertex(indices[i+1]),c=vertex(indices[i+2]);
        if (cross(a,b,c)==0) continue;
        const auto ab=cross(a,b,screen),bc=cross(b,c,screen),ca=cross(c,a,screen);
        if ((ab>=0 && bc>=0 && ca>=0) || (ab<=0 && bc<=0 && ca<=0)) return true;
    }
    return false;
}
}
