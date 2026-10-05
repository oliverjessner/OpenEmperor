#include "scene/IsoProjection.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void api(bool result,const char* what) {
    if (!result) throw std::runtime_error(std::string(what)+": "+SDL_GetError());
}
void expected(bool okay,const char* what) {
    if (!okay) throw std::runtime_error(what);
}
bool near(double a,double b) {return std::abs(a-b)<.0001;}
void point(SDL_Renderer* r,float x,float y,double ex,double ey,const char* label) {
    float rx=0,ry=0;
    api(SDL_RenderCoordinatesFromWindow(r,x,y,&rx,&ry),label);
    expected(near(rx,ex)&&near(ry,ey),label);
    std::cout<<"{\"case\":\""<<label<<"\",\"window\":["<<x<<','<<y
        <<"],\"render\":["<<rx<<','<<ry<<"],\"expected\":["<<ex<<','<<ey<<"]}\n";
}
}
int main() {
    SDL_Window* w=nullptr; SDL_Renderer* r=nullptr; SDL_Texture* t=nullptr;
    try {
        api(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),"dummy hint");
        api(SDL_Init(SDL_INIT_VIDEO),"init");
        w=SDL_CreateWindow("input coordinate contract tests",800,600,SDL_WINDOW_RESIZABLE);
        expected(w,"create window");
        r=SDL_CreateRenderer(w,"software"); expected(r,"create software renderer");
        expected(std::string(SDL_GetRendererName(r))=="software","actual backend");
        int ww=0,wh=0,pw=0,ph=0,ow=0,oh=0;
        api(SDL_GetWindowSize(w,&ww,&wh),"window size");
        api(SDL_GetWindowSizeInPixels(w,&pw,&ph),"pixel size");
        api(SDL_GetRenderOutputSize(r,&ow,&oh),"main output");
        expected(ww==800&&wh==600&&pw==800&&ph==600&&ow==800&&oh==600,"measured 1:1 dummy sizes");
        std::cout<<"{\"runtime\":"<<SDL_GetVersion()<<",\"revision\":\""<<SDL_GetRevision()
            <<"\",\"video\":\""<<SDL_GetCurrentVideoDriver()<<"\",\"renderer\":\""
            <<SDL_GetRendererName(r)<<"\",\"window\":["<<ww<<','<<wh<<"],\"pixels\":["
            <<pw<<','<<ph<<"],\"output\":["<<ow<<','<<oh<<"]}\n";
        point(r,123.5f,246.25f,123.5,246.25,"full-window literal");
        SDL_Rect clip{80,100,200,200}; api(SDL_SetRenderClipRect(r,&clip),"clip");
        point(r,123.5f,246.25f,123.5,246.25,"clip does not transform position");
        api(SDL_SetRenderClipRect(r,nullptr),"reset clip");
        SDL_Rect vp{31,47,400,300}; api(SDL_SetRenderViewport(r,&vp),"nonzero viewport");
        api(SDL_SetRenderScale(r,1.25f,1.5f),"fractional renderer scale");
        point(r,123.5f,246.25f,67.8,117.1666666667,"scale then subtract viewport");
        SDL_Event e{}; e.type=SDL_EVENT_MOUSE_MOTION; e.motion.windowID=SDL_GetWindowID(w);
        e.motion.x=123.5f;e.motion.y=246.25f;e.motion.xrel=4.75f;e.motion.yrel=-3;
        api(SDL_ConvertEventToRenderCoordinates(r,&e),"convert motion");
        expected(near(e.motion.x,67.8)&&near(e.motion.y,117.1666666667)&&
            near(e.motion.xrel,3.8)&&near(e.motion.yrel,-2),"absolute and relative motion oracle");
        e={};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.windowID=SDL_GetWindowID(w);
        e.wheel.mouse_x=123.5f;e.wheel.mouse_y=246.25f;e.wheel.x=-.5f;e.wheel.y=1.25f;
        api(SDL_ConvertEventToRenderCoordinates(r,&e),"convert wheel");
        expected(near(e.wheel.mouse_x,67.8)&&near(e.wheel.mouse_y,117.1666666667)&&
            e.wheel.x==-.5f&&e.wheel.y==1.25f,"wheel position converts while delta unchanged");
        api(SDL_SetRenderScale(r,1,1),"reset scale");
        t=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,200,100);
        expected(t,"create authored target");api(SDL_SetRenderTarget(r,t),"target");
        api(SDL_GetCurrentRenderOutputSize(r,&ow,&oh),"target output");
        expected(ow==200&&oh==100,"current output is target dimensions");
        point(r,210,300,179,253,"FromWindow retains main-view viewport under target");
        api(SDL_SetRenderTarget(r,nullptr),"reset target");
        api(SDL_SetRenderViewport(r,nullptr),"reset viewport");
        api(SDL_SetRenderLogicalPresentation(r,400,400,SDL_LOGICAL_PRESENTATION_LETTERBOX),"logical letterbox");
        point(r,160,300,40,200,"letterbox literal interior");
        point(r,70,300,-20,200,"letterbox outside remains outside");
        // These are synthetic logical-coordinate scale cases, not physical
        // high-DPI configurations inferred from the dummy driver.
        api(SDL_SetRenderLogicalPresentation(r,1600,1200,SDL_LOGICAL_PRESENTATION_STRETCH),"synthetic logical 2:1");
        point(r,123.5f,246.25f,247,492.5,"synthetic logical 2:1 literal");
        api(SDL_SetRenderLogicalPresentation(r,1200,900,SDL_LOGICAL_PRESENTATION_STRETCH),"synthetic logical 1.5:1");
        point(r,123.5f,246.25f,185.25,369.375,"synthetic logical 1.5:1 literal");
        api(SDL_SetRenderLogicalPresentation(r,1400,900,SDL_LOGICAL_PRESENTATION_STRETCH),"synthetic fractional logical scale");
        point(r,123.5f,246.25f,216.125,369.375,"synthetic logical 1.75:1.5 literal");
        api(SDL_SetRenderLogicalPresentation(r,0,0,SDL_LOGICAL_PRESENTATION_DISABLED),"reset logical presentation");
        api(SDL_SetWindowSize(w,1000,700),"real dummy resize");
        while(SDL_PollEvent(&e)) {}
        api(SDL_GetWindowSize(w,&ww,&wh),"resized window");
        api(SDL_GetRenderOutputSize(r,&ow,&oh),"resized output");
        expected(ww==1000&&wh==700&&ow==1000&&oh==700,"actual resize + measured output");
        point(r,500,350,500,350,"real resize center");
        const bool before=SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT];
        e={};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=SDL_GetWindowID(w);
        e.key.scancode=SDL_SCANCODE_RIGHT;e.key.key=SDLK_RIGHT;e.key.down=true;
        api(SDL_PushEvent(&e),"push authored key");
        bool delivered=false;while(SDL_PollEvent(&e)) if(e.type==SDL_EVENT_KEY_DOWN) delivered=true;
        expected(delivered&&SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT]==before,
            "queued synthetic key leaves SDL device state unchanged");
        openemperor::scene::Camera2D c;c.zoom=1.25;c.offset={100.25,80.5};
        const auto screen=c.world_to_screen({20,80});
        expected(near(screen.x,125.25)&&near(screen.y,180.5),"literal camera forward point");
        float wx=0,wy=0;
        api(SDL_RenderCoordinatesToWindow(r,500,350,&wx,&wy),"inverse literal");
        expected(near(wx,500)&&near(wy,350),"ToWindow independent literal");
        const auto world=c.screen_to_world({125.25,180.5});
        expected(near(world.x,20)&&near(world.y,80),"literal camera inverse point");
        expected(openemperor::scene::pick_cell({20,80},10,10)==openemperor::scene::Cell{2,1},"independent logical-cell literal");
        c.zoom_at({125.25,180.5},2);
        expected(near(c.zoom,2.5)&&near(c.offset.x,75.25)&&near(c.offset.y,-19.5),"literal anchored zoom result");
        // Literal tables avoid making paired forward/inverse helpers the oracle.
        const double zooms[]{.5,1.125,1.25,1.5,2,4};
        const double xs[]{110.25,122.75,125.25,130.25,140.25,180.25};
        const double ys[]{120.5,170.5,180.5,200.5,240.5,400.5};
        c.offset={100.25,80.5};
        for (int i=0;i<6;++i) {
            c.zoom=zooms[i];const auto p=c.world_to_screen({20,80});
            expected(near(p.x,xs[i])&&near(p.y,ys[i]),"camera literal zoom matrix");
        }
        expected(!openemperor::scene::pick_cell({-81,-1},10,10),"outside ground is not clamped");
        SDL_DestroyTexture(t);t=nullptr;SDL_DestroyRenderer(r);r=nullptr;SDL_DestroyWindow(w);w=nullptr;SDL_Quit();
        std::cout<<"{\"result\":\"pass\",\"native_actions\":false}\n";return 0;
    } catch(const std::exception& err) {
        std::cerr<<err.what()<<'\n';
        if(t)SDL_DestroyTexture(t);
        if(r)SDL_DestroyRenderer(r);
        if(w)SDL_DestroyWindow(w);
        SDL_Quit();return 1;
    }
}
