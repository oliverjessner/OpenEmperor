#include "renderer/TextureCompatibility.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Color=std::array<std::uint8_t,4>;
constexpr Color background{17,91,137,255},red{220,40,80,255},blue{35,185,230,255};
constexpr int width=800,height=600;
constexpr SDL_Rect clip{12,19,776,562};
template<class Function> auto api(const char* operation,Function function) {
    SDL_ClearError();auto result=function();
    if(!result) throw std::runtime_error(std::string(operation)+": "+SDL_GetError());
    return result;
}
void expect(bool result,const std::string& message) {
    if(!result) throw std::runtime_error(message); // Pixel failures are not SDL error events.
}
struct Context {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    explicit Context(bool metal) {
        const char* driver=metal ? "cocoa":"dummy",*backend=metal ? "metal":"software";
        api("video hint",[&]{return SDL_SetHint(SDL_HINT_VIDEO_DRIVER,driver);});
        api("SDL_Init",[]{return SDL_Init(SDL_INIT_VIDEO);});
        window=api("window",[]{return SDL_CreateWindow("authored SDL zoom",width,height,SDL_WINDOW_HIDDEN);});
        renderer=api("renderer",[&]{return SDL_CreateRenderer(window,backend);});
        expect(std::strcmp(SDL_GetRendererName(renderer),backend)==0,"requested renderer must be actual renderer");
        expect(std::strcmp(SDL_GetCurrentVideoDriver(),driver)==0,"requested video driver must be actual driver");
        int w=0,h=0;api("render size",[&]{return SDL_GetRenderOutputSize(renderer,&w,&h);});
        expect(w==width&&h==height,"test output size");
        std::cout<<"headers="<<SDL_VERSION<<" runtime="<<SDL_GetVersion()<<" revision="<<SDL_GetRevision()
            <<" renderer="<<SDL_GetRendererName(renderer)<<" driver="<<SDL_GetCurrentVideoDriver()
            <<" size="<<w<<'x'<<h<<" clip=12,19,776,562 BLEND/NEAREST"
#if defined(__aarch64__) || defined(_M_ARM64)
            <<" arch=arm64"
#endif
#ifdef NDEBUG
            <<" build=Release\n";
#else
            <<" build=Debug\n";
#endif
    }
    ~Context(){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();}
};
struct Image {
    int w,h;std::vector<std::uint8_t> pixels;
    Color at(int x,int y) const {
        const auto n=static_cast<std::size_t>((y*w+x)*4);
        return {pixels[n],pixels[n+1],pixels[n+2],pixels[n+3]};
    }
    bool interior(int x,int y) const {
        if(x<2||y<2||x+2>=w||y+2>=h)return false;
        const auto color=at(x,y);
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
            if(at(x+dx,y+dy)!=color)return false;
        return true;
    }
};
Image image(char pattern,bool large) {
    Image result{large ? 318:23,large ? 238:31,{}};
    result.pixels.resize(static_cast<std::size_t>(result.w*result.h*4));
    for(int y=0;y<result.h;++y)for(int x=0;x<result.w;++x) {
        Color color=red;
        if(pattern!='A'&&(x<3||y<3||x>=result.w-3||y>=result.h-3))color[3]=0;
        if(pattern=='C'&&std::abs(x-result.w/2)<=4&&std::abs(y-result.h/2)<=4)color[3]=0;
        if(pattern=='D'&&color[3]&&x<result.w/2)color[3]=128;
        if(pattern=='E') {
            if(x>=result.w*61/100&&y>=result.h*64/100)color[3]=0;
            else if(color[3]&&x>=result.w*60/100&&x<result.w*87/100&&
                    y>=result.h*17/100&&y<result.h*43/100)color=blue;
        }
        std::copy(color.begin(),color.end(),result.pixels.begin()+static_cast<std::ptrdiff_t>((y*result.w+x)*4));
    }
    return result;
}
struct Texture {
    SDL_Texture* value=nullptr;
    Texture(SDL_Renderer* renderer,const Image& source) {
        const auto access=openemperor::texture_compatibility::eager_rgba_access(renderer);
        value=api("create one texture",[&]{return SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,access,source.w,source.h);});
        api("one eager upload",[&]{return SDL_UpdateTexture(value,nullptr,source.pixels.data(),source.w*4);});
        api("blend",[&]{return SDL_SetTextureBlendMode(value,SDL_BLENDMODE_BLEND);});
        api("nearest",[&]{return SDL_SetTextureScaleMode(value,SDL_SCALEMODE_NEAREST);});
        const auto properties=SDL_GetTextureProperties(value);
        expect(SDL_GetNumberProperty(properties,SDL_PROP_TEXTURE_ACCESS_NUMBER,-1)==access,"actual access matches policy");
    }
    ~Texture(){SDL_DestroyTexture(value);}
};
struct Instance {SDL_FRect rect;std::uint8_t alpha=255;};
std::vector<Instance> instances(const Image& source,double zoom,const std::string& location) {
    const double w=source.w*zoom,h=source.h*zoom;
    double x=(width-w)/2+.25,y=(height-h)/2+.375;
    if(location=="left")x=clip.x-w/2+.25;
    if(location=="right")x=clip.x+clip.w-w/2+.25;
    if(location=="top")y=clip.y-h/2+.375;
    if(location=="bottom")y=clip.y+clip.h-h/2+.375;
    if(location=="outside")x=-w-40.25;
    if(location=="shared")return {{{24.25f,170.375f,float(source.w),float(source.h)},128},
                                 {{444.25f,170.375f,float(source.w),float(source.h)},255}};
    return {{{float(x),float(y),float(w),float(h)},std::uint8_t(location=="alpha128" ? 128:255)}};
}
Color blend(Color source,Color target,std::uint8_t modulation) {
    const int alpha=int(source[3])*modulation/255;
    for(std::size_t n=0;n<3;++n)target[n]=std::uint8_t((int(source[n])*alpha+int(target[n])*(255-alpha)+127)/255);
    target[3]=255;return target;
}
bool close(Color actual,Color expected) {
    for(std::size_t n=0;n<4;++n)if(std::abs(int(actual[n])-int(expected[n]))>2)return false;
    return true;
}
struct Surface {
    SDL_Surface* value=nullptr;
    explicit Surface(SDL_Surface* surface):value(surface){}
    ~Surface(){SDL_DestroySurface(value);}
};
std::uint64_t inspect(Context& context,const Image& source,const std::vector<Instance>& draws,
                      const std::string& label) {
    Surface read(api("before-Present readback",[&]{return SDL_RenderReadPixels(context.renderer,nullptr);}));
    Surface rgba(api("RGBA readback",[&]{return SDL_ConvertSurface(read.value,SDL_PIXELFORMAT_RGBA32);}));
    const auto pixel=[&](int x,int y) {
        const auto* p=static_cast<const std::uint8_t*>(rgba.value->pixels)+y*rgba.value->pitch+x*4;
        return Color{p[0],p[1],p[2],p[3]};
    };
    constexpr std::array<std::array<double,2>,8> points{{{.25,.5},{.75,.5},{.5,.5},{.27,.5},
                                                       {.73,.28},{.75,.75},{.5,.25},{.5,.75}}};
    for(const auto& item:draws)for(const auto& point:points) {
        const int sx=int(point[0]*source.w),sy=int(point[1]*source.h);
        if(!source.interior(sx,sy))continue; // Exclude fractional raster boundaries uniformly.
        const int x=int(std::floor(item.rect.x+(sx+.5)*item.rect.w/source.w));
        const int y=int(std::floor(item.rect.y+(sy+.5)*item.rect.h/source.h));
        if(x<clip.x||x>=clip.x+clip.w||y<clip.y||y>=clip.y+clip.h)continue;
        Color expected=background;bool safe=true;
        for(const auto& d:draws) {
            const double u=(x+.5-d.rect.x)*source.w/d.rect.w,v=(y+.5-d.rect.y)*source.h/d.rect.h;
            if(u<0||v<0||u>=source.w||v>=source.h)continue;
            if(!source.interior(int(u),int(v))){safe=false;break;}
            expected=blend(source.at(int(u),int(v)),expected,d.alpha);
        }
        if(safe)expect(close(pixel(x,y),expected),label+" authored opaque/hole/half-alpha pixel");
    }
    if(label.find("outside")!=std::string::npos)
        expect(pixel(width/2,height/2)==background,"fully outside leaves background");
    std::uint64_t hash=1469598103934665603ULL;
    for(int y=0;y<rgba.value->h;++y)for(int x=0;x<rgba.value->w*4;++x) {
        hash^=static_cast<const std::uint8_t*>(rgba.value->pixels)[y*rgba.value->pitch+x];hash*=1099511628211ULL;
    }
    return hash;
}
std::uint64_t frame(Context& context,Texture& texture,const Image& source,double zoom,
                    const std::string& location,bool readback) {
    auto* renderer=context.renderer;
    api("clear clip",[&]{return SDL_SetRenderClipRect(renderer,nullptr);});
    api("background",[&]{return SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255);});
    api("clear",[&]{return SDL_RenderClear(renderer);});
    api("map clip",[&]{return SDL_SetRenderClipRect(renderer,&clip);});
    const auto draws=instances(source,zoom,location);
    for(const auto& draw:draws) {
        api("alpha modulation",[&]{return SDL_SetTextureAlphaMod(texture.value,draw.alpha);});
        api("RenderTexture",[&]{return SDL_RenderTexture(renderer,texture.value,nullptr,&draw.rect);});
    }
    api("restore alpha255",[&]{return SDL_SetTextureAlphaMod(texture.value,255);});
    std::uint8_t alpha=0;api("query restored alpha",[&]{return SDL_GetTextureAlphaMod(texture.value,&alpha);});
    expect(alpha==255,"shared texture alpha is restored");
    const auto hash=readback ? inspect(context,source,draws,location+" zoom="+std::to_string(zoom)):0;
    api("Present",[&]{return SDL_RenderPresent(renderer);});return hash;
}
void exercise(Context& context,char pattern,bool large) {
    const auto source=image(pattern,large);Texture texture(context.renderer,source);
    const auto home=frame(context,texture,source,1,"center",true);
    const auto clipped=frame(context,texture,source,1,"left",true);
    for(int repeat=0;repeat<100;++repeat)for(double zoom:{1.,1.125,1.}) {
        const auto hash=frame(context,texture,source,zoom,"left",true);
        if(zoom==1)expect(hash==clipped,"same clipped camera/zoom returns to identical frame hash");
    }
    for(const auto& sequence:std::vector<std::vector<double>>{{1,1.25,1.5,1},{1,2,4,1}})
        for(const auto& location:std::array<std::string,8>{"center","left","right","top","bottom","outside","alpha128","shared"})
            for(double zoom:sequence)for(int repeat=0;repeat<3;++repeat)
                frame(context,texture,source,zoom,location,true);
    // No readback, target switch, Flush, or extra draw in these normal frames.
    for(int repeat=0;repeat<100;++repeat)for(double zoom:{1.,1.125,1.})
        frame(context,texture,source,zoom,"left",false);
    expect(frame(context,texture,source,1,"center",true)==home,"fresh terminal before-Present frame equals baseline");
    std::cout<<pattern<<' '<<source.w<<'x'<<source.h<<" stable: one texture/upload,100 read and100 Present-only sequences\n";
}
}
int main(int argc,char** argv) {
    try {
        expect(argc==1||(argc==2&&std::string(argv[1])=="metal"),"optional argument is metal");
        Context context(argc==2);
        for(char pattern:{'A','B','C','D','E'})for(bool large:{false,true})exercise(context,pattern,large);
        std::cout<<"Authored SDL texture zoom stability passed\n";return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
