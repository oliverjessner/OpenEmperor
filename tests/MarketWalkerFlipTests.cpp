#include "assets/WalkerVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/WalkerSpriteSet.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

// The owner calls real SDL. Observe physical identity, upload count and the
// actual native/rotated submission without using draw geometry as our oracle.
namespace observer {
struct Submission { SDL_Texture* texture;SDL_FRect destination;bool flipped; };
std::set<SDL_Texture*> live;
std::map<SDL_Texture*,std::size_t> uploads;
std::vector<Submission> submissions;
SDL_Texture* create(SDL_Renderer* renderer,SDL_PixelFormat format,
                    SDL_TextureAccess access,int width,int height) {
    auto* texture=SDL_CreateTexture(renderer,format,access,width,height);
    if (texture) live.insert(texture);
    return texture;
}
bool upload(SDL_Texture* texture,const SDL_Rect* rect,const void* pixels,int pitch) {
    if (!SDL_UpdateTexture(texture,rect,pixels,pitch)) return false;
    ++uploads[texture];return true;
}
void destroy(SDL_Texture* texture) {
    live.erase(texture);uploads.erase(texture);SDL_DestroyTexture(texture);
}
bool draw(SDL_Renderer* renderer,SDL_Texture* texture,const SDL_FRect* source,
          const SDL_FRect* destination) {
    submissions.push_back({texture,*destination,false});
    return SDL_RenderTexture(renderer,texture,source,destination);
}
bool rotated(SDL_Renderer* renderer,SDL_Texture* texture,const SDL_FRect* source,
             const SDL_FRect* destination,double angle,const SDL_FPoint* center,SDL_FlipMode flip) {
    if (angle!=0 || flip!=SDL_FLIP_HORIZONTAL || center)
        throw std::runtime_error("display flip introduced rotation, vertical flip or pivot");
    submissions.push_back({texture,*destination,true});
    return SDL_RenderTextureRotated(renderer,texture,source,destination,angle,center,flip);
}
}
#define SDL_CreateTexture observer::create
#define SDL_UpdateTexture observer::upload
#define SDL_DestroyTexture observer::destroy
#define SDL_RenderTexture observer::draw
#define SDL_RenderTextureRotated observer::rotated
#include "../src/renderer/WalkerSpriteSet.cpp"
#undef SDL_CreateTexture
#undef SDL_UpdateTexture
#undef SDL_DestroyTexture
#undef SDL_RenderTexture
#undef SDL_RenderTextureRotated

namespace {
namespace assets=openemperor::assets;
namespace perf=openemperor::performance;
using openemperor::scene::Point;
using Color=std::array<std::uint8_t,4>;
constexpr Color background{24,48,80,255};
constexpr int canvas_width=512,canvas_height=320;
void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
Color pixel(const assets::RgbaImage& image,int x,int y) {
    const auto at=(static_cast<std::size_t>(y)*image.width+static_cast<unsigned>(x))*4;
    return {image.pixels.at(at),image.pixels.at(at+1),image.pixels.at(at+2),image.pixels.at(at+3)};
}
void put(assets::RgbaImage& image,int x,int y,Color color) {
    const auto at=(static_cast<std::size_t>(y)*image.width+static_cast<unsigned>(x))*4;
    std::copy(color.begin(),color.end(),image.pixels.begin()+static_cast<std::ptrdiff_t>(at));
}
assets::RgbaImage authored(int phase) {
    assets::RgbaImage image;image.width=static_cast<std::uint16_t>(11+2*phase);
    image.height=static_cast<std::uint16_t>(13+2*phase);
    image.pixels.assign(static_cast<std::size_t>(image.width)*image.height*4,0);
    const int width=static_cast<int>(image.width),height=static_cast<int>(image.height);
    // Unequal left/right silhouettes and colors, including the extreme columns.
    for (int y=2;y<height-3;++y) for (int x=0;x<width;++x) {
        if (y==2 && x<3) continue; // Transparent asymmetric corner.
        put(image,x,y,x<width/2 ? Color{255,0,0,255}:Color{0,0,255,255});
    }
    for (int y=5;y<=7;++y) for (int x=3;x<=5;++x) put(image,x,y,{0,0,0,0}); // Interior hole.
    for (int y=height-3;y<height-1;++y) for (int x=1;x<=4;++x) put(image,x,y,{0,255,0,255});
    for (int y=height-3;y<height;++y) for (int x=width-3;x<width;++x)
        put(image,x,y,{0,0,0,128}); // Independently authored prepared shadow.
    for (int y=0;y<2;++y) for (int x=phase ? width-4:1;x<(phase ? width-1:4);++x)
        put(image,x,y,{255,255,0,255}); // A genuinely different arm/head gait.
    return image;
}
assets::WalkerVisualProfile profile() {
    assets::WalkerVisualProfile result;result.schema_version=4;
    result.unique_images={authored(0),authored(1)};
    assets::WalkerRoleVisual role;role.ticks_per_frame=2;
    role.clip_id="independently-authored-asymmetric-flip";
    role.evidence="Original-free RGBA silhouette, continuous feet and discrete alpha oracle.";
    for (int phase=0;phase<2;++phase) {
        const double foot_x=phase ? 3.75:2.25;
        const double foot_y=static_cast<double>(result.unique_images[static_cast<std::size_t>(phase)].height)-2.5;
        for (const bool flipped:{false,true}) {
            assets::WalkerFrame frame;
            frame.alias=std::string(flipped ? "reflected-":"native-")+std::to_string(phase);
            frame.id={"authored-native.sg3",static_cast<std::uint32_t>(phase+1)};
            frame.image_index=static_cast<std::size_t>(phase);
            frame.foot_x=flipped ? (phase ? 9.25:8.75):foot_x;
            frame.foot_y=foot_y;frame.flip_x=flipped;role.frames.push_back(frame);
        }
    }
    role.clips[0]={0,2};role.clips[1]={1,3};role.clips[2]={0,2};role.clips[3]={1,3};
    result.roles[assets::walker_role_index(assets::WalkerVisualRole::Supplier)]=std::move(role);
    return result;
}
struct Pixels {
    SDL_Surface* surface=nullptr;
    explicit Pixels(SDL_Renderer* renderer) {
        auto* raw=SDL_RenderReadPixels(renderer,nullptr);check(raw!=nullptr,"pre-Present target readback");
        surface=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(raw);
        check(surface && surface->w==canvas_width && surface->h==canvas_height,"same-size RGBA target");
    }
    ~Pixels() { SDL_DestroySurface(surface); }
    Pixels(const Pixels&)=delete;
    Color at(int x,int y) const {
        const auto* bytes=static_cast<const std::uint8_t*>(surface->pixels)+y*surface->pitch+x*4;
        return {bytes[0],bytes[1],bytes[2],bytes[3]};
    }
};
void state() {
    for (auto* texture:observer::live) {
        std::uint8_t alpha=0,r=0,g=0,b=0;SDL_BlendMode blend=SDL_BLENDMODE_INVALID;
        SDL_ScaleMode scale=SDL_SCALEMODE_INVALID;
        check(SDL_GetTextureAlphaMod(texture,&alpha) && alpha==255,"texture alpha state leaked");
        check(SDL_GetTextureColorMod(texture,&r,&g,&b) && r==255 && g==255 && b==255,"texture color state leaked");
        check(SDL_GetTextureBlendMode(texture,&blend) && blend==SDL_BLENDMODE_BLEND,"texture blend state leaked");
        check(SDL_GetTextureScaleMode(texture,&scale) && scale==SDL_SCALEMODE_NEAREST,"texture scale state changed");
        check(observer::uploads.at(texture)==1,"a native source texture was uploaded again");
    }
}
void pure_draw(openemperor::WalkerSpriteSet& sprites,std::size_t selected,Point ground,double zoom,
        const assets::WalkerRoleVisual& role,const assets::WalkerVisualProfile& prepared,SDL_Rect clip) {
    perf::set_enabled(true);perf::reset();
    check(sprites.draw(selected,ground,zoom,role,prepared,
                      {static_cast<double>(clip.x),static_cast<double>(clip.y)},
                      {static_cast<double>(clip.x+clip.w),static_cast<double>(clip.y+clip.h)}),"production Walker draw");
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,"draw performed asset/World/navigation work");
    perf::set_enabled(false);state();
}
void clear(SDL_Renderer* renderer,SDL_Rect clip) {
    check(SDL_SetRenderClipRect(renderer,nullptr),"clear clip reset");
    check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255) && SDL_RenderClear(renderer),"target clear");
    check(SDL_SetRenderClipRect(renderer,&clip),"target viewport clip");observer::submissions.clear();
}
struct OracleCounts { std::size_t opaque=0,hole=0,inner_hole=0,shadow=0,first=0,last=0; };
OracleCounts oracle(const Pixels& actual,const assets::RgbaImage& source,Point ground,
                    double native_foot_x,double foot_y,double zoom,bool reflected,SDL_Rect clip) {
    // Independent explicit arithmetic: a continuous reflected point is W-ax;
    // an integer source-column lookup is W-1-i. No production pose/hit helper.
    const int width=static_cast<int>(source.width),height=static_cast<int>(source.height);
    const double displayed_foot=reflected ? width-native_foot_x:native_foot_x;
    const double left=ground.x-displayed_foot*zoom,top=ground.y-foot_y*zoom;
    OracleCounts counts;
    for (int y=std::max(clip.y,static_cast<int>(std::floor(top)));y<std::min(clip.y+clip.h,static_cast<int>(std::ceil(top+height*zoom)));++y)
        for (int x=std::max(clip.x,static_cast<int>(std::floor(left)));x<std::min(clip.x+clip.w,static_cast<int>(std::ceil(left+width*zoom)));++x) {
            const double qx=(x+0.5-left)/zoom,qy=(y+0.5-top)/zoom;
            if (qx<0 || qx>=width || qy<0 || qy>=height) continue;
            // Backend raster-edge rounding is not a transformed-alpha oracle.
            // Inspect stable sample interiors and count extreme columns too.
            const double fx=qx-std::floor(qx),fy=qy-std::floor(qy);
            if (zoom==std::floor(zoom) && (fx<0.13 || fx>0.87 || fy<0.13 || fy>0.87)) continue;
            if (zoom!=std::floor(zoom) &&
                (x+0.5>=std::floor(left+width*zoom) || y+0.5>=std::floor(top+height*zoom))) continue;
            const int displayed_column=static_cast<int>(std::floor(qx));
            const int source_column=reflected ? width-1-displayed_column:displayed_column;
            const int source_row=static_cast<int>(std::floor(qy));
            const auto rgba=pixel(source,source_column,source_row),observed=actual.at(x,y);
            if (zoom!=std::floor(zoom)) {
                // Fractional nearest rasterization has different edge rounding
                // on SDL's software copy and GPU paths. A homogeneous native
                // neighborhood gives an independent, positive color/alpha
                // witness on either path, including extreme source columns.
                bool homogeneous=true;
                for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
                    if (pixel(source,std::clamp(source_column+dx,0,width-1),
                                     std::clamp(source_row+dy,0,height-1))!=rgba) homogeneous=false;
                if (!homogeneous) continue;
            }
            if (rgba[3]==255) {
                if (observed!=rgba) {
                    std::cerr<<"opaque x/y="<<x<<'/'<<y<<" source="<<source_column<<'/'<<source_row<<" zoom="<<zoom<<" flip="<<reflected<<'\n';
                    throw std::runtime_error("native/reflected opaque pixel differs from independent source-column oracle");
                }
                ++counts.opaque;
                if (source_column==0) ++counts.first;
                if (source_column==width-1) ++counts.last;
            } else if (!rgba[3]) {
                if (observed!=background) {
                    std::cerr<<"hole x/y="<<x<<'/'<<y<<" source="<<source_column<<'/'<<source_row<<" zoom="<<zoom<<" flip="<<reflected<<'\n';
                    throw std::runtime_error("transparent corner/hole became a visible or enlarged pixel");
                }
                ++counts.hole;
                if (source_column>=3 && source_column<=5 && source_row>=5 && source_row<=7)
                    ++counts.inner_hole;
            } else {
                for (std::size_t channel=0;channel<3;++channel) {
                    const int expected=(background[channel]*127)/255;
                    check(std::abs(static_cast<int>(observed[channel])-expected)<=1,"shadow alpha/flip composition differs");
                }
                check(observed[3]==255,"prepared shadow changed target alpha");++counts.shadow;
            }
        }
    return counts;
}
void tests(SDL_Renderer* renderer) {
    auto prepared=profile();const auto unchanged=prepared.unique_images;
    const auto& role=*prepared.find(assets::WalkerVisualRole::Supplier);
    openemperor::WalkerSpriteSet sprites;sprites.initialize(renderer,prepared);
    check(sprites.texture_count()==2 && observer::live.size()==2 && sprites.logical_bytes()==1352,
          "native and reflected aliases created duplicate physical textures or bytes");
    auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,canvas_width,canvas_height);
    check(target && SDL_SetRenderTarget(renderer,target),"explicit same-size render target");
    const SDL_Rect full{0,0,canvas_width,canvas_height};
    for (const int phase:{0,1}) for (const double zoom:{1.0,1.5,2.0,4.0,1.0}) {
        clear(renderer,full);
        const auto& source=prepared.unique_images[static_cast<std::size_t>(phase)];
        const double foot_x=phase ? 3.75:2.25,foot_y=source.height-2.5;
        const std::array<Point,3> grounds{{{40+foot_x*zoom,110+foot_y*zoom},
            {205+(source.width-foot_x)*zoom,110+foot_y*zoom},{370+foot_x*zoom,110+foot_y*zoom}}};
        const auto native=static_cast<std::size_t>(phase*2),reflected=native+1;
        pure_draw(sprites,native,grounds[0],zoom,role,prepared,full);
        pure_draw(sprites,reflected,grounds[1],zoom,role,prepared,full);
        pure_draw(sprites,native,grounds[2],zoom,role,prepared,full);
        check(observer::submissions.size()==3,"simultaneous native/flip/native draw count");
        check(observer::submissions[0].texture==observer::submissions[1].texture &&
              observer::submissions[1].texture==observer::submissions[2].texture,"simultaneous reflected alias used another texture");
        const Pixels pixels(renderer);
        for (std::size_t i=0;i<grounds.size();++i) {
            const bool flip=i==1;
            const auto counts=oracle(pixels,source,grounds[i],foot_x,foot_y,zoom,flip,full);
            if (!(counts.opaque>20 && counts.hole>4 && counts.inner_hole>0 && counts.shadow>1 && counts.first>1 && counts.last>1)) {
                std::cerr<<"phase="<<phase<<" zoom="<<zoom<<" reflected="<<flip
                    <<" opaque/hole/inner/shadow/first/last="<<counts.opaque<<'/'<<counts.hole<<'/'<<counts.inner_hole<<'/'
                    <<counts.shadow<<'/'<<counts.first<<'/'<<counts.last<<'\n';
                throw std::runtime_error("pixel oracle lacks meaningful asymmetric, hole, shadow or extreme-column witnesses");
            }
            const auto& rectangle=observer::submissions[i].destination;
            const double expected_anchor=flip ? source.width-foot_x:foot_x;
            check(std::abs(rectangle.x+expected_anchor*zoom-grounds[i].x)<0.0001 &&
                  std::abs(rectangle.y+foot_y*zoom-grounds[i].y)<0.0001,"continuous feet transformed twice or used discrete W-1-ax");
        }
        check(!observer::submissions[0].flipped && observer::submissions[1].flipped &&
              !observer::submissions[2].flipped,"native submission path changed or flip leaked to next draw");
    }
    // The same ground reference, unequal frame sizes and both transformations.
    for (const int phase:{0,1}) for (const bool flip:{false,true}) {
        clear(renderer,full);const Point ground{210,155};
        pure_draw(sprites,static_cast<std::size_t>(phase*2+(flip ? 1:0)),ground,2,role,prepared,full);
        const auto& source=prepared.unique_images[static_cast<std::size_t>(phase)];
        const auto& rectangle=observer::submissions.at(0).destination;
        check(std::abs(rectangle.x+(flip ? source.width-(phase ? 3.75:2.25):(phase ? 3.75:2.25))*2-ground.x)<0.0001,
              "frame size or orientation shifted common ground registration");
    }
    // Actual renderer clipping, in addition to the owner's coarse viewport cull.
    const SDL_Rect clipped{70,65,160,120};
    for (const double zoom:{1.5,2.0,4.0}) for (const bool flip:{false,true}) {
        clear(renderer,clipped);const Point ground{68+(flip ? 8.75:2.25)*zoom,63+10.5*zoom};
        pure_draw(sprites,static_cast<std::size_t>(flip ? 1:0),ground,zoom,role,prepared,clipped);
        const Pixels pixels(renderer);const auto counts=oracle(pixels,prepared.unique_images[0],ground,2.25,10.5,zoom,flip,clipped);
        check(counts.opaque>5 && counts.hole>1,"partially clipped positive draw was not inspected");
        check(pixels.at(clipped.x-1,90)==background && pixels.at(clipped.x,clipped.y-1)==background,
              "walker pixels escaped actual viewport clipping");
        const auto draws=observer::submissions.size();
        pure_draw(sprites,static_cast<std::size_t>(flip ? 1:0),{10000,10000},zoom,role,prepared,clipped);
        check(observer::submissions.size()==draws,"fully culled alias submitted a draw");
    }
    for (std::size_t i=0;i<unchanged.size();++i)
        check(prepared.unique_images[i].pixels==unchanged[i].pixels,"display reflection changed or copied prepared CPU pixels");
    state();check(SDL_SetRenderTarget(renderer,nullptr),"restore ordinary render target");
    SDL_DestroyTexture(target);sprites.shutdown();
    check(observer::live.empty() && openemperor::WalkerSpriteSet::live_texture_count()==0,"display flip leaked physical textures");
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        if (!metal) SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL video init");
        window=SDL_CreateWindow("Asymmetric Walker display-flip tests",canvas_width,canvas_height,SDL_WINDOW_HIDDEN);
        check(window!=nullptr,"hidden window");renderer=SDL_CreateRenderer(window,metal ? "metal":"software");
        check(renderer && std::string_view(SDL_GetRendererName(renderer))==(metal ? "metal":"software"),"actual renderer backend");
        tests(renderer);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"PASS independent native/flip RGBA, continuous feet, discrete alpha, shared texture, clipping, state and cleanup\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
