#include "renderer/WellFallbackRenderer.h"
#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
void check(bool okay,const char* message) { if (!okay) throw std::runtime_error(message); }
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
using Renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>;
using Color=std::array<std::uint8_t,4>;
constexpr Color terrain{100,110,60,255};
Color pixel(SDL_Surface* surface,int x,int y) {
    Color color{};
    check(SDL_ReadSurfacePixel(surface,x,y,&color[0],&color[1],&color[2],&color[3]),"read pixel");
    return color;
}
void clear(SDL_Renderer* renderer) {
    check(SDL_SetRenderDrawColor(renderer,terrain[0],terrain[1],terrain[2],terrain[3]) &&
        SDL_RenderClear(renderer),"clear synthetic terrain");
}
}

int main() {
    try {
        Surface canvas(SDL_CreateSurface(512,512,SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
        check(canvas!=nullptr,"create surface");
        Renderer renderer(SDL_CreateSoftwareRenderer(canvas.get()),SDL_DestroyRenderer);
        check(renderer!=nullptr,"create software renderer");
        for (const int scale:{1,2,4}) {
            clear(renderer.get());
            check(SDL_SetRenderDrawBlendMode(renderer.get(),SDL_BLENDMODE_NONE),"set caller blend mode");
            check(openemperor::draw_well_fallback(renderer.get(),{256,256},scale),"draw placed Well");
            SDL_BlendMode blend;
            check(SDL_GetRenderDrawBlendMode(renderer.get(),&blend) && blend==SDL_BLENDMODE_NONE,
                "Well changed caller blend mode");
            check(SDL_FlushRenderer(renderer.get()),"flush geometry");
            int changed=0,water=0,stone=0;
            Color water_sample{};int water_x=0,water_y=0;
            for (int y=0;y<512;++y) for (int x=0;x<512;++x) {
                const auto color=pixel(canvas.get(),x,y);
                if (color==terrain) continue;
                ++changed;
                // Independent generous acceptance envelope, relative to the
                // unchanged ground point. Height is distinct from ground occupancy.
                check(x>=256-24*scale && x<=256+24*scale &&
                    y>=256-33*scale && y<=256+12*scale,"Well escaped one-cell visual bounds");
                if (y>=256) check(std::abs(x-256)/40.0+std::abs(y-256)/20.0<=scale+0.1,
                    "base escaped 80x40 ground diamond");
                if (color[2]>color[0] && color[1]>color[0]) {
                    ++water;water_sample=color;water_x=x;water_y=y;
                    check(color[1]<=80 && color[1]-color[0]<20,"water is bright or saturated");
                }
                if (color[0]>=100 && color[0]>color[1] && color[1]>color[2] &&
                    color[0]-color[2]<45) ++stone;
            }
            check(changed>200*scale*scale,"Well has no substantial visible geometry");
            check(water>0 && water*3<stone,"water dominates stone rim");
            clear(renderer.get());
            check(openemperor::draw_well_fallback(renderer.get(),{256,256},scale,true),"draw placement preview");
            check(SDL_FlushRenderer(renderer.get()),"flush preview");
            const auto preview=pixel(canvas.get(),water_x,water_y);
            check(preview!=water_sample && preview!=terrain,"preview alpha not applied");
            // Water covers its darker opening and base in the same mesh. Check
            // alpha on an exposed top beam, where exactly one face is drawn.
            const int beam_x=256-8*scale,beam_y=256-29*scale;
            const auto ghost=pixel(canvas.get(),beam_x,beam_y);
            clear(renderer.get());
            check(openemperor::draw_well_fallback(renderer.get(),{256,256},scale),"draw opaque comparison");
            check(SDL_FlushRenderer(renderer.get()),"flush opaque comparison");
            const auto opaque=pixel(canvas.get(),beam_x,beam_y);
            check(opaque!=terrain,"beam sample missed geometry");
            for (std::size_t channel=0;channel<3;++channel) {
                const int expected=(opaque[channel]*128+terrain[channel]*127)/255;
                check(std::abs(static_cast<int>(ghost[channel])-expected)<=2,"preview is not alpha 128");
            }
            std::cout<<scale<<"x: visible="<<changed<<", water="<<water<<", stone="<<stone<<'\n';
        }
        for (const int z:{1,2,4}) {
            clear(renderer.get());
            check(openemperor::draw_well_fallback(renderer.get(),{256,400},z,false,2) &&
                SDL_FlushRenderer(renderer.get()),"2x2 Well fallback");
            int visible=0;
            for (int y=0;y<512;++y) for (int x=0;x<512;++x) if (pixel(canvas.get(),x,y)!=terrain) {
                ++visible;
                check(std::abs(x-256)<=48*z && y>=400-86*z && y<=400+4*z,
                    "2x2 Well visual bounds");
                if (y>=400-20*z) check(std::abs(x-256)/40.0+std::abs(y-(400-20*z))/20.0<=2*z+.1,
                    "2x2 Well base outside logical footprint");
            }
            check(visible>800*z*z,"2x2 Well fallback did not scale with footprint");
        }
        clear(renderer.get());
        const double nan=std::numeric_limits<double>::quiet_NaN();
        const double inf=std::numeric_limits<double>::infinity();
        for (const double scale:{0.0,-1.0,nan,inf,std::numeric_limits<double>::max()})
            check(!openemperor::draw_well_fallback(renderer.get(),{256,256},scale),"invalid zoom accepted");
        for (const openemperor::scene::Point ground:std::array<openemperor::scene::Point,4>{
            {{nan,0},{0,inf},{std::numeric_limits<double>::max(),0},{0,-inf}}})
            check(!openemperor::draw_well_fallback(renderer.get(),ground,1),"invalid coordinate accepted");
        check(!openemperor::draw_well_fallback(nullptr,{0,0},1),"null renderer accepted");
        check(SDL_FlushRenderer(renderer.get()),"flush rejected draws");
        for (int y=0;y<512;++y) for (int x=0;x<512;++x)
            check(pixel(canvas.get(),x,y)==terrain,"rejected geometry drew pixels");
        std::cout<<"Finite geometry, bounds, muted water, caller state and alpha checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<": "<<SDL_GetError()<<'\n';return 1;
    }
}
