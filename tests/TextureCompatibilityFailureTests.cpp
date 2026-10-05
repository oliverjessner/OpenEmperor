#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

namespace {
bool fail_next_draw=false;
bool draw_with_failure(SDL_Renderer* renderer,SDL_Texture* texture,
                       const SDL_FRect* source,const SDL_FRect* destination) {
    if (fail_next_draw) {
        fail_next_draw=false;
        return SDL_SetError("injected texture submission failure");
    }
    return SDL_RenderTexture(renderer,texture,source,destination);
}
}

// Compile the actual preview owners with only their draw call intercepted.
// Upload, modulation, restoration, normal drawing and destruction still use SDL.
#define SDL_RenderTexture draw_with_failure
#define live_textures building_live_textures
#include "renderer/BuildingSprite.cpp"
#undef live_textures
#define live_textures road_live_textures
#include "renderer/RoadSpriteSet.cpp"
#undef live_textures
#undef SDL_RenderTexture

namespace {
void check(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Draw> void verify(SDL_Renderer* renderer,const Draw& draw) {
    check(SDL_SetRenderDrawColor(renderer,0,0,0,255)&&SDL_RenderClear(renderer),"clear");
    fail_next_draw=true;
    check(!draw(true)&&!fail_next_draw,"preview propagates injected submission failure");
    SDL_ClearError();
    check(draw(false),"normal shared instance after failed preview");
    auto* pixels=SDL_RenderReadPixels(renderer,nullptr);
    check(pixels!=nullptr,"readback before Present");
    Uint8 r=0,g=0,b=0,a=0;
    const bool read=SDL_ReadSurfacePixel(pixels,39,20,&r,&g,&b,&a);
    SDL_DestroySurface(pixels);
    check(read&&r==255&&g==0&&b==0&&a==255,"failed preview restores full normal alpha");
    check(SDL_RenderPresent(renderer),"present");
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),"SDL init");
        window=SDL_CreateWindow("preview failure regression",96,64,SDL_WINDOW_HIDDEN);
        check(window!=nullptr,"window");
        renderer=SDL_CreateRenderer(window,"software");check(renderer!=nullptr,"software renderer");
        check(std::string_view(SDL_GetRendererName(renderer))=="software","actual software backend");
        openemperor::assets::RgbaImage image{78,40,{}};
        image.pixels.resize(78*40*4);
        for(std::size_t i=0;i<image.pixels.size();i+=4) {image.pixels[i]=255;image.pixels[i+3]=255;}
        {
            openemperor::assets::BuildingVisualProfile profile;profile.unique_images={image};
            const openemperor::assets::BuildingVisualEntry entry{{"authored",1},0,39,20,1,"authored"};
            openemperor::BuildingSprite sprites;sprites.initialize(renderer,profile);
            verify(renderer,[&](bool preview){return sprites.draw({39,20},1,profile,entry,preview);});
        }
        {
            openemperor::assets::RoadVisualProfile profile;profile.unique_images={image};
            const openemperor::assets::RoadVisualEntry entry{{"authored",1},0,39,20,"authored"};
            openemperor::RoadSpriteSet sprites;sprites.initialize(renderer,profile);
            verify(renderer,[&](bool preview){return sprites.draw({39,20},1,profile,entry,preview);});
        }
        check(openemperor::BuildingSprite::live_texture_count()==0&&
              openemperor::RoadSpriteSet::live_texture_count()==0,"owned textures released");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Preview submission failures restore shared texture alpha\n";return 0;
    }catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
