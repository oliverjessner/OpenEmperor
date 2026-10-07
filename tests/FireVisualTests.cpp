#include "assets/FireVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/TextureCompatibility.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

namespace {
struct TextureState {
    SDL_TextureAccess access;
    int width,height;
    unsigned uploads=0;
};
std::map<SDL_Texture*,TextureState> observed;
unsigned create_calls=0,upload_calls=0,fail_create_at=0,fail_upload_at=0;
bool fail_next_draw=false;
SDL_Texture* recorded_create(SDL_Renderer* renderer,SDL_PixelFormat format,
                              SDL_TextureAccess access,int width,int height) {
    ++create_calls;
    if (fail_create_at==create_calls) { SDL_SetError("injected fire creation failure");return nullptr; }
    auto* texture=SDL_CreateTexture(renderer,format,access,width,height);
    if (texture) observed.emplace(texture,TextureState{access,width,height,0});
    return texture;
}
bool recorded_upload(SDL_Texture* texture,const SDL_Rect* rect,const void* pixels,int pitch) {
    ++upload_calls;
    if (fail_upload_at==upload_calls) return SDL_SetError("injected fire upload failure");
    const bool result=SDL_UpdateTexture(texture,rect,pixels,pitch);
    if (result) ++observed.at(texture).uploads;
    return result;
}
void recorded_destroy(SDL_Texture* texture) { observed.erase(texture);SDL_DestroyTexture(texture); }
bool draw_with_failure(SDL_Renderer* renderer,SDL_Texture* texture,
                         const SDL_FRect* source,const SDL_FRect* destination) {
    if (fail_next_draw) { fail_next_draw=false;return SDL_SetError("injected fire draw failure"); }
    return SDL_RenderTexture(renderer,texture,source,destination);
}
}

// Actual owner body, with only bounded recording/failure wrappers. The ordinary
// upload, draw, blend and destruction calls still use the real SDL renderer.
#define SDL_CreateTexture recorded_create
#define SDL_UpdateTexture recorded_upload
#define SDL_DestroyTexture recorded_destroy
#define SDL_RenderTexture draw_with_failure
#include "renderer/FireSpriteSet.cpp"
#undef SDL_CreateTexture
#undef SDL_UpdateTexture
#undef SDL_DestroyTexture
#undef SDL_RenderTexture

namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Json=nlohmann::json;
using namespace openemperor;

void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class Function> void rejects(const Function& operation,const char* reason) {
    try { operation(); } catch (const std::exception&) { return; }
    throw std::runtime_error(reason);
}
void u16(Bytes& bytes,std::size_t at,std::uint16_t value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);
    bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    u16(bytes,at,static_cast<std::uint16_t>(value));
    u16(bytes,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out),"fixture write failed");
}

Bytes authored_flame(int width,int height,bool second) {
    Bytes payload;
    for (int y=0;y<height;++y) {
        int left=width,right=-1;
        if (y>0) {
            if (second) { left=y==1 ? 4:y==2 ? 2:1;right=y==1 ? 4:y==2 ? 5:6; }
            else { left=y<=2 ? 2:1;right=y<=2 ? 2:y<=4 ? 3:4; }
        }
        if (left>0) payload.insert(payload.end(),{255,static_cast<std::uint8_t>(left)});
        if (right>=left) {
            payload.push_back(static_cast<std::uint8_t>(right-left+1));
            for (int x=left;x<=right;++x) {
                // Original-independent RGB555 orange/yellow geometry; no original pixels.
                const std::uint16_t color=(y==height-1 && x==left) ? 0x7c00:
                    second ? 0x7fe0:0x7da0;
                payload.push_back(static_cast<std::uint8_t>(color));
                payload.push_back(static_cast<std::uint8_t>(color>>8U));
            }
        }
        if (right+1<width && right>=0)
            payload.insert(payload.end(),{255,static_cast<std::uint8_t>(width-right-1)});
    }
    return payload;
}

struct Fixture {
    fs::path root=fs::temp_directory_path()/
        ("openemperor-fire-visual-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"fire.json";
    Bytes metadata,bitmap;
    Fixture() {
        fs::create_directories(data/"DATA");
        const auto first=authored_flame(6,8,false),second=authored_flame(8,6,true);
        metadata.resize(40680U+8U*72U);
        u32(metadata,0,static_cast<std::uint32_t>(metadata.size()));u32(metadata,4,214);
        u32(metadata,12,8);u32(metadata,16,8);u32(metadata,20,1);
        const std::string group="authored-fire.bmp";
        std::copy(group.begin(),group.end(),metadata.begin()+680);u32(metadata,680+124,8);
        bitmap={0,0,0,0};
        for (const auto& [index,width,height,payload]:{
                 std::tuple{1U,6,8,first},std::tuple{3U,8,6,second}}) {
            const auto at=40680U+index*72U;
            u32(metadata,at,static_cast<std::uint32_t>(bitmap.size()));
            u32(metadata,at+4,static_cast<std::uint32_t>(payload.size()));
            u16(metadata,at+20,static_cast<std::uint16_t>(width));
            u16(metadata,at+22,static_cast<std::uint16_t>(height));u16(metadata,at+50,256);
            bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        }
        write(data/"DATA/fire.sg3",metadata);write(data/"DATA/fire.555",bitmap);
        save(valid());
    }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    Json valid() const {
        return {{"schema_version",1},{"mode","curated_fire_presentation"},
            {"clip_id","authored-asymmetric-flames"},{"evidence","Independent synthetic flames; authored 2-tick timing and bottom-center anchors."},
            {"ticks_per_frame",2},{"frames",Json::array({
                {{"archive","DATA/fire.sg3"},{"image_index",1},{"anchor",{3,8}}},
                {{"archive","DATA/fire.sg3"},{"image_index",3},{"anchor",{4,6}}},
                {{"archive","DATA/fire.sg3"},{"image_index",1},{"anchor",{3,8}}}})}};
    }
    void save(const Json& value) const { std::ofstream out(manifest);out<<value.dump(2);check(bool(out),"manifest write"); }
};

void profile_checks(Fixture& fixture) {
    const auto load=[&]{return assets::load_fire_visual_profile(fixture.data,fixture.manifest);};
    const auto profile=load();
    check(profile.frames.size()==3 && profile.unique_images.size()==2 &&
          profile.frames[0].id.image_index==1 && profile.frames[1].id.image_index==3 &&
          profile.frames[2].image_index==0,"explicit physical order/dedup changed");
    check(assets::fire_rgba_bytes(profile)==384,"logical fire image bytes wrong");
    const auto& first=profile.unique_images[0];
    const auto red=(static_cast<std::size_t>(7)*first.width+1U)*4U;
    check(first.pixels[red]==255 && first.pixels[red+1]==0 && first.pixels[red+2]==0 &&
          first.pixels[red+3]==255,"fire loader introduced an unevidenced red/shadow color key");
    auto invalid=fixture.valid();invalid["schema_version"]=2;fixture.save(invalid);
    rejects(load,"unknown schema accepted");
    invalid=fixture.valid();invalid["mode"]="walker";fixture.save(invalid);
    rejects(load,"walker mode accepted as fire");
    invalid=fixture.valid();invalid["frames"]=Json::array();fixture.save(invalid);
    rejects(load,"empty clip accepted");
    invalid=fixture.valid();invalid["frames"]=Json::array({invalid["frames"][0]});fixture.save(invalid);
    rejects(load,"static one-frame clip accepted");
    invalid=fixture.valid();invalid["frames"][1]=invalid["frames"][0];fixture.save(invalid);
    rejects(load,"same physical frame clip accepted");
    invalid=fixture.valid();for (int i=0;i<65;++i) invalid["frames"].push_back(invalid["frames"][0]);
    fixture.save(invalid);rejects(load,"unbounded frame clip accepted");
    for (const Json& value:{Json(0),Json(-1),Json(1001),Json(4294967296ULL)}) {
        invalid=fixture.valid();invalid["ticks_per_frame"]=value;fixture.save(invalid);
        rejects(load,"invalid frame duration accepted");
    }
    for (const Json& value:{Json(0),Json(-1),Json(4294967297ULL),Json(7)}) {
        invalid=fixture.valid();invalid["frames"][1]["image_index"]=value;fixture.save(invalid);
        rejects(load,"invalid physical image reference accepted");
    }
    for (const auto& path:{"../outside.sg3","/outside.sg3","DATA/../fire.sg3","DATA\\fire.sg3"}) {
        invalid=fixture.valid();invalid["frames"][0]["archive"]=path;fixture.save(invalid);
        rejects(load,"unsafe archive path accepted");
    }
    for (const Json& value:{Json::array({1,1e100}),Json::array({1,nullptr}),Json::array({1})}) {
        invalid=fixture.valid();invalid["frames"][0]["anchor"]=value;fixture.save(invalid);
        rejects(load,"invalid anchor accepted");
    }
    invalid=fixture.valid();invalid["frames"][0]["duration"]=0;fixture.save(invalid);
    rejects(load,"unknown per-frame duration accepted");
    invalid=fixture.valid();invalid["evidence"]="";fixture.save(invalid);
    rejects(load,"missing evidence accepted");
    fixture.save(fixture.valid());
    auto metadata=fixture.metadata;u16(metadata,40680U+3U*72U+20U,257);
    write(fixture.data/"DATA/fire.sg3",metadata);rejects(load,"oversized dimensions accepted");
    write(fixture.data/"DATA/fire.sg3",fixture.metadata);
    metadata=fixture.metadata;u32(metadata,40680U+3U*72U+16U,1);
    write(fixture.data/"DATA/fire.sg3",metadata);rejects(load,"mirrored fire frame accepted");
    write(fixture.data/"DATA/fire.sg3",fixture.metadata);
    write(fixture.data/"DATA/fire.555",Bytes{0,0,0,0,255});rejects(load,"broken second frame accepted");
    write(fixture.data/"DATA/fire.555",fixture.bitmap);
    const auto bitmap=fixture.data/"DATA/fire.555",outside=fixture.root/"outside.555";
    fs::rename(bitmap,outside);fs::create_symlink(outside,bitmap);
    rejects(load,"bitmap symlink escape accepted");fs::remove(bitmap);fs::rename(outside,bitmap);
    {
        std::ofstream out(fixture.manifest);
        out<<R"({"schema_version":1,"schema_version":1,"mode":"curated_fire_presentation"})";
    }
    rejects(load,"duplicate manifest key accepted");
    { std::ofstream out(fixture.manifest);out<<std::string(65537,' '); }
    rejects(load,"oversized manifest accepted");
    fixture.save(fixture.valid());
    auto constructed=profile;constructed.frames[0].anchor_y=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{assets::validate_fire_visual_profile(constructed);},"NaN authored anchor accepted");
    constructed=profile;constructed.unique_images[1]=constructed.unique_images[0];
    rejects([&]{assets::validate_fire_visual_profile(constructed);},"identical decoded frames accepted");
    constructed=profile;constructed.unique_images[1].pixels[3]=0;
    for (auto& value:constructed.unique_images[1].pixels) value=0;
    rejects([&]{assets::validate_fire_visual_profile(constructed);},"invisible frame accepted");
    constructed=profile;constructed.frames[2].image_index=1;
    rejects([&]{assets::validate_fire_visual_profile(constructed);},"und deduplicated physical asset accepted");
    constructed=profile;constructed.unique_images[0].pixels.pop_back();
    rejects([&]{assets::validate_fire_visual_profile(constructed);},"bad pixel buffer accepted");
}

void frame_checks(const assets::FireVisualProfile& profile) {
    check(assets::fire_frame_index(profile,0)==0 && assets::fire_frame_index(profile,1)==0 &&
          assets::fire_frame_index(profile,2)==1 && assets::fire_frame_index(profile,3)==1 &&
          assets::fire_frame_index(profile,4)==2 && assets::fire_frame_index(profile,6)==0,
          "frame boundary/wrap selection wrong");
    check(assets::fire_frame_index(profile,0,1)==1 && assets::fire_frame_index(profile,0,6)==0 &&
          assets::fire_frame_index(profile,0,7)==0 && assets::fire_frame_index(profile,4,1)==0,
          "bounded stable BuildingId phase wrong");
    constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
    const auto expected=(maximum/2U%3U+maximum%7U%3U)%3U;
    check(assets::fire_frame_index(profile,maximum,maximum)==expected,"maximum tick/id arithmetic overflow");
    for (std::uint64_t tick=0;tick<1000;++tick)
        for (int render=0;render<13;++render)
            check(assets::fire_frame_index(profile,tick,11)==(tick/2U%3U+11U%7U%3U)%3U,
                  "render frequency mutated frame phase");
    auto invalid=profile;invalid.ticks_per_frame=0;
    check(!assets::fire_frame_index(invalid,maximum,maximum),"zero duration queried");
    invalid=profile;invalid.frames.clear();
    check(!assets::fire_frame_index(invalid,maximum,maximum),"empty clip queried");
    invalid=profile;invalid.frames.resize(65);
    check(!assets::fire_frame_index(invalid,maximum,maximum),"oversized clip queried");
}

Bytes read_pixels(SDL_Renderer* renderer,int width,int height) {
    auto* source=SDL_RenderReadPixels(renderer,nullptr);check(source!=nullptr,"read before Present");
    auto* surface=SDL_ConvertSurface(source,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(source);
    check(surface!=nullptr && surface->w==width && surface->h==height,"read dimensions");
    Bytes result(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*4U);
    const auto* bytes=static_cast<const std::uint8_t*>(surface->pixels);
    for (int y=0;y<height;++y)
        std::copy_n(bytes+static_cast<std::size_t>(y)*static_cast<std::size_t>(surface->pitch),
                    static_cast<std::size_t>(width)*4U,
                    result.data()+static_cast<std::size_t>(y)*static_cast<std::size_t>(width)*4U);
    SDL_DestroySurface(surface);return result;
}
std::array<std::uint8_t,4> pixel(const Bytes& bytes,int x,int y) {
    const auto at=(static_cast<std::size_t>(y)*96U+static_cast<std::size_t>(x))*4U;
    return {bytes.at(at),bytes.at(at+1),bytes.at(at+2),bytes.at(at+3)};
}

void render_checks(Fixture& fixture,bool metal) {
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal ? "cocoa":"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=SDL_CreateWindow("fire clip synthetic pixels",96,96,SDL_WINDOW_HIDDEN);
    check(window!=nullptr,"window creation");
    SDL_Renderer* renderer=SDL_CreateRenderer(window,metal ? "metal":"software");
    check(renderer!=nullptr,"renderer creation");
    check(std::string_view(SDL_GetRendererName(renderer))==(metal ? "metal":"software"),"actual renderer name");
    auto profile=assets::load_fire_visual_profile(fixture.data,fixture.manifest);
    // Independent straight-alpha witness, still sharing the same prepared frames.
    const auto half=(static_cast<std::size_t>(6)*6U+3U)*4U;
    profile.unique_images[0].pixels[half]=200;profile.unique_images[0].pixels[half+1]=100;
    profile.unique_images[0].pixels[half+2]=40;profile.unique_images[0].pixels[half+3]=128;
    {
        FireSpriteSet sprites;
        rejects([&]{sprites.initialize(renderer,profile,383);},"remaining aggregate budget ignored");
        check(sprites.texture_count()==0 && observed.empty(),"budget failure published textures");
        fail_create_at=create_calls+2;
        rejects([&]{sprites.initialize(renderer,profile);},"partial creation failure hidden");
        fail_create_at=0;SDL_ClearError();
        check(sprites.texture_count()==0 && sprites.logical_bytes()==0 &&
              FireSpriteSet::live_texture_count()==0 && observed.empty(),"partial creation leaked");
        fail_upload_at=upload_calls+2;
        rejects([&]{sprites.initialize(renderer,profile);},"partial upload failure hidden");
        fail_upload_at=0;SDL_ClearError();
        check(sprites.texture_count()==0 && FireSpriteSet::live_texture_count()==0 &&
              observed.empty(),"partial upload leaked");
        performance::set_enabled(true);performance::reset();
        sprites.initialize(renderer,profile,384);
        check(sprites.texture_count()==2 && sprites.logical_bytes()==384 &&
              performance::counter(performance::Counter::TextureUploads)==2,"eager physical texture dedup failed");
        for (const auto& [texture,state]:observed) {
            const auto properties=SDL_GetTextureProperties(texture);
            const auto access=static_cast<SDL_TextureAccess>(SDL_GetNumberProperty(properties,SDL_PROP_TEXTURE_ACCESS_NUMBER,-1));
            SDL_BlendMode blend=SDL_BLENDMODE_NONE;SDL_ScaleMode scale=SDL_SCALEMODE_LINEAR;
            Uint8 r=0,g=0,b=0,a=0;
            check(access==texture_compatibility::eager_rgba_access(renderer) && state.access==access &&
                  state.uploads==1 && SDL_GetTextureBlendMode(texture,&blend) && blend==SDL_BLENDMODE_BLEND &&
                  SDL_GetTextureScaleMode(texture,&scale) && scale==SDL_SCALEMODE_NEAREST &&
                  SDL_GetTextureColorMod(texture,&r,&g,&b) && r==255 && g==255 && b==255 &&
                  SDL_GetTextureAlphaMod(texture,&a) && a==255,"prepared texture policy wrong");
        }
        const auto union_box=sprites.clip_bounds({40,40},1);
        check(union_box && union_box->min.x==36 && union_box->min.y==32 &&
              union_box->max.x==44 && union_box->max.y==40,"all-frame clip bounds not cached correctly");
        check(!FireSpriteSet::bounds(3,{40,40},1,profile) &&
              !FireSpriteSet::bounds(0,{40,40},1e300,profile) &&
              !sprites.clip_bounds({40,40},std::numeric_limits<double>::infinity()),"invalid bounds accepted");
        check(SDL_SetRenderDrawColor(renderer,20,40,60,255) &&
              SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_ADD),"backing/state setup");
        const auto draw_frame=[&](std::uint64_t tick,double zoom=1.0,scene::Point attachment={40,40}) {
            check(SDL_RenderClear(renderer),"clear backing");
            const auto frame=assets::fire_frame_index(profile,tick,7);
            check(frame && sprites.draw(*frame,attachment,zoom,profile,{0,0},{96,96}),"frame draw");
            SDL_BlendMode mode=SDL_BLENDMODE_NONE;Uint8 r=0,g=0,b=0,a=0;
            check(SDL_GetRenderDrawBlendMode(renderer,&mode) && mode==SDL_BLENDMODE_ADD &&
                  SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a) && r==20 && g==40 && b==60 && a==255,
                  "effect leaked renderer blend/color state");
            return read_pixels(renderer,96,96);
        };
        const auto first=draw_frame(0),second=draw_frame(2);
        check(first!=second,"different timepoints rendered the same flame silhouette");
        const auto backing=std::array<std::uint8_t,4>{20,40,60,255};
        check(pixel(first,37,32)==backing && pixel(second,36,34)==backing,
              "transparent flame background blocked backing");
        check(pixel(first,40,39)!=backing && pixel(second,40,39)!=backing,
              "common bottom attachment jumped between frames");
        const auto blended=pixel(first,40,38);
        check(std::abs(static_cast<int>(blended[0])-110)<=1 &&
              std::abs(static_cast<int>(blended[1])-70)<=1 &&
              std::abs(static_cast<int>(blended[2])-50)<=1,"original straight alpha not source-over");
        performance::reset();
        // Asset sources genuinely unavailable after preparation; all subsequent
        // renders use only the shared textures, bounds and immutable metadata.
        fs::rename(fixture.data,fixture.root/"data-unavailable");
        for (int repeat=0;repeat<24;++repeat) {
            check(draw_frame(0)==first && draw_frame(2)==second,"paused/repeated scene pixels changed");
            for (const double zoom:{1.0,1.125,2.0,4.0,1.0}) {
                const auto frame=draw_frame(2,zoom);
                if (pixel(frame,40,38)==backing)
                    throw std::runtime_error("scaled flame disappeared at zoom "+std::to_string(zoom));
            }
        }
        check(draw_frame(0)==first,"zoom return changed paused pixels");
        const auto clipped=draw_frame(0,1.0,{40,100});
        check(pixel(clipped,39,95)!=backing,"offscreen attachment culled entering flame tip");
        check(SDL_RenderClear(renderer) && sprites.draw(0,{40,120},1,profile,{0,0},{96,96}),"outside cull");
        const auto outside=read_pixels(renderer,96,96);
        check(pixel(outside,40,95)==backing,"fully outside fire wrote pixels");
        check(SDL_RenderClear(renderer),"multi-instance backing");
        for (int i=0;i<20;++i)
            check(sprites.draw(static_cast<std::size_t>(i%3),{10.0+(i%5)*17.0,12.0+(i/5)*20.0},
                               1,profile,{0,0},{96,96}),"shared multi-fire draw");
        fail_next_draw=true;
        check(!sprites.draw(0,{40,40},1,profile,{0,0},{96,96}) && !fail_next_draw,"injected draw failure ignored");
        SDL_ClearError();check(draw_frame(0)==first,"failed effect changed later shared texture state");
        for (std::size_t counter=0;counter<static_cast<std::size_t>(performance::Counter::Count);++counter)
            check(performance::counter(static_cast<performance::Counter>(counter))==0,
                  "rendering performed I/O/decode/upload/World/navigation work");
        for (const auto& [texture,state]:observed) {
            Uint8 alpha=0;SDL_BlendMode blend=SDL_BLENDMODE_NONE;
            check(state.uploads==1 && SDL_GetTextureAlphaMod(texture,&alpha) && alpha==255 &&
                  SDL_GetTextureBlendMode(texture,&blend) && blend==SDL_BLENDMODE_BLEND,"shared effect state leaked");
        }
        fs::rename(fixture.root/"data-unavailable",fixture.data);
        performance::set_enabled(false);
        sprites.shutdown();
        check(sprites.texture_count()==0 && sprites.logical_bytes()==0 && !sprites.clip_bounds({40,40},1) &&
              observed.empty() && FireSpriteSet::live_texture_count()==0,"session shutdown leaked effects");
        sprites.initialize(renderer,profile);sprites.shutdown();
        check(observed.empty() && FireSpriteSet::live_texture_count()==0,"reinitialization leaked effects");
    }
    std::cout<<Json{{"evidence","independently authored synthetic fire frames; actual production sprite owner"},
        {"renderer",SDL_GetRendererName(renderer)},{"runtime",SDL_GetVersion()},
        {"texture_access",metal ? "STATIC":"runtime-bounded"},{"frames",3},{"unique_textures",2},
        {"logical_rgba_bytes",384},{"distinct_pixel_silhouettes",true},
        {"readback","before Present"},{"partial_init_cleanup",true}}.dump()<<'\n';
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
} // namespace

int main(int argc,char* argv[]) {
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        if (argc>1 && !metal) throw std::runtime_error("usage: fire-visual-tests [metal]");
        Fixture fixture;profile_checks(fixture);
        const auto profile=assets::load_fire_visual_profile(fixture.data,fixture.manifest);
        frame_checks(profile);render_checks(fixture,metal);
        std::cout<<"Fire profile, tick phase, animated pixels, shared eager resources and atomic cleanup passed\n";
        return 0;
    } catch (const std::exception& error) {
        performance::set_enabled(false);std::cerr<<error.what()<<'\n';SDL_Quit();return 1;
    }
}
