#include "renderer/HealthPostFallbackRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
using Pixel=std::array<Uint8,4>;
Pixel pixel(SDL_Surface* s,int x,int y){Pixel p{};check(SDL_ReadSurfacePixel(s,x,y,&p[0],&p[1],&p[2],&p[3]),"pixel");return p;}
}
int main(){try{
 auto* s=SDL_CreateSurface(512,512,SDL_PIXELFORMAT_RGBA32);check(s,"surface");
 auto* r=SDL_CreateSoftwareRenderer(s);check(r,"software renderer");
 constexpr Pixel background{70,81,60,255};
 const auto clear=[&]{check(SDL_SetRenderDrawColor(r,70,81,60,255) && SDL_RenderClear(r),"clear");};
 for(int z:{1,2,4}){
  clear();check(SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE),"caller state");
  check(openemperor::draw_health_post_fallback(r,{256,256},z) && SDL_FlushRenderer(r),"draw HealthPost");
  SDL_BlendMode mode;check(SDL_GetRenderDrawBlendMode(r,&mode) && mode==SDL_BLENDMODE_NONE,"changed caller blend");
  int visible=0;
  for(int y=0;y<512;++y)for(int x=0;x<512;++x)if(pixel(s,x,y)!=background){
   ++visible;check(x>=256-31*z && x<=256+31*z && y>=256-49*z && y<=256+15*z,"Health Post bounds");
   if(y>=256)check(std::abs(x-256)/40.+std::abs(y-256)/20.<=z+.1,"base outside one-cell diamond");
  }
  check(visible>500*z*z,"Health Post invisible at reviewed zoom");
  const auto opaque=pixel(s,256-12*z,256-30*z);check(opaque!=background,"roof sample");
  clear();check(openemperor::draw_health_post_fallback(r,{256,256},z,true) && SDL_FlushRenderer(r),"ghost");
  const auto ghost=pixel(s,256-12*z,256-30*z);
  for(std::size_t c=0;c<3;++c)check(std::abs(static_cast<int>(ghost[c])-(opaque[c]*128+background[c]*127)/255)<=2,"ghost alpha128");
 }
 clear();const auto nan=std::numeric_limits<double>::quiet_NaN();
 for(double z:{0.,-1.,nan,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::max()})
  check(!openemperor::draw_health_post_fallback(r,{0,0},z),"invalid zoom accepted");
 check(!openemperor::draw_health_post_fallback(r,{nan,0},1) &&
  !openemperor::draw_health_post_fallback(nullptr,{0,0},1),"invalid render input accepted");
 check(SDL_FlushRenderer(r),"flush");
 for(int y=0;y<512;++y)for(int x=0;x<512;++x)check(pixel(s,x,y)==background,"rejected input drew");
 SDL_DestroyRenderer(r);SDL_DestroySurface(s);std::cout<<"Health pavilion 1x/2x/4x bounds, alpha128, caller state and finite inputs\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
