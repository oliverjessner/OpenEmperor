#include "assets/RoadVisualProfile.h"
#include "renderer/RoadSpriteSet.h"
#include "app/SandboxVisualOrder.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Json=nlohmann::json;
void check(bool okay,const char* message) { if (!okay) throw std::runtime_error(message); }
void u16(Bytes& b,std::size_t p,std::uint16_t x) {
    b.at(p)=static_cast<std::uint8_t>(x);b.at(p+1)=static_cast<std::uint8_t>(x>>8U);
}
void u32(Bytes& b,std::size_t p,std::uint32_t x) {
    u16(b,p,static_cast<std::uint16_t>(x));u16(b,p+2,static_cast<std::uint16_t>(x>>16U));
}
void write(const fs::path& p,const Bytes& b) {
    std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),
        static_cast<std::streamsize>(b.size()));check(bool(out),"synthetic write");
}
template<class F> void rejects(F f,const char* message) {
    try { f(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
std::string key(int mask) {
    constexpr char hex[]="0123456789abcdef";
    return std::string("0x")+hex[mask];
}
struct Fixture {
    fs::path root=fs::temp_directory_path()/
        ("openemperor-road-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"roads.json";
    Bytes sg3,bitmap;
    Fixture() {
        fs::create_directories(data/"DATA");
        sg3.assign(40680U+17U*72U,0);
        u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
        u32(sg3,12,17);u32(sg3,16,17);u32(sg3,20,1);
        const std::string name="synthetic.bmp";
        std::copy(name.begin(),name.end(),sg3.begin()+680);
        u32(sg3,680+124,17);
        bitmap.assign(4U+16U*3200U,0);
        for (int i=1;i<=16;++i) {
            const auto record=40680U+static_cast<std::size_t>(i)*72U;
            const auto offset=4U+static_cast<unsigned>(i-1)*3200U;
            u32(sg3,record,offset);u32(sg3,record+4,3200);u32(sg3,record+8,3200);
            u16(sg3,record+20,78);u16(sg3,record+22,40);
            u16(sg3,record+50,30);sg3[record+55]=1;
            // Independently constructed, distinct RGB555 red/green combinations.
            const auto color=static_cast<std::uint16_t>(((i & 31)<<10)|((i & 15)<<5));
            for (unsigned p=0;p<3200;p+=2) u16(bitmap,offset+p,color);
        }
        write(data/"DATA/roads.sg3",sg3);write(data/"DATA/roads.555",bitmap);
    }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    Json entry(int image=1) const {
        return {{"archive","DATA/roads.sg3"},{"image_index",image},
                {"ground_anchor",{39,20}},{"evidence","Synthetic original-independent tile"}};
    }
    Json profile() const {
        return {{"schema_version",1},{"mode","curated_road_preview"},
                {"tiles",{{"0x0",entry()}}}};
    }
    void save(const Json& data_json) const {
        std::ofstream out(manifest);out<<data_json.dump();check(bool(out),"manifest write");
    }
};
std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"read pixels");
    std::array<std::uint8_t,4> result{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]);
    SDL_DestroySurface(surface);check(okay,"pixel read");return result;
}
void profile_tests(Fixture& f) {
    f.save(f.profile());
    auto loaded=openemperor::assets::load_road_visual_profile(f.data,f.manifest);
    check(loaded.configured_count()==1 && loaded.unique_images.size()==1 &&
          loaded.find(0) && !loaded.find(1),"single mask profile");
    auto two=f.profile();two["tiles"]["0x5"]=f.entry();f.save(two);
    loaded=openemperor::assets::load_road_visual_profile(f.data,f.manifest);
    check(loaded.configured_count()==2 && loaded.unique_images.size()==1 &&
          loaded.find(0)->image_index==loaded.find(5)->image_index,"AssetId deduplication");
    two["tiles"]["0x5"]=f.entry(2);f.save(two);
    loaded=openemperor::assets::load_road_visual_profile(f.data,f.manifest);
    check(loaded.unique_images.size()==2,"distinct road assets");
    auto bad=f.profile();bad["tiles"]["0x10"]=f.entry();f.save(bad);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"bad mask");
    { std::ofstream out(f.manifest);out<<R"({"schema_version":1,"mode":"curated_road_preview","tiles":{"0x0":{},"0x0":{}}})"; }
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"duplicate mask");
    bad=f.profile();bad["tiles"]["0x0"]["archive"]="../escape.sg3";f.save(bad);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"unsafe path");
    for (const auto& value:Json::array({Json("NaN"),Json("Infinity"),Json(5000)})) {
        bad=f.profile();bad["tiles"]["0x0"]["ground_anchor"][0]=value;f.save(bad);
        rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},
                "nonfinite/outside anchor");
    }
    f.save(f.profile());
    auto changed=f.sg3;u16(changed,40680U+72U+50U,256);
    write(f.data/"DATA/roads.sg3",changed);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"wrong type");
    changed=f.sg3;u32(changed,40680U+72U+16U,5);write(f.data/"DATA/roads.sg3",changed);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"mirror");
    changed=f.sg3;u16(changed,40680U+72U+30U,1);write(f.data/"DATA/roads.sg3",changed);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"animation");
    changed=f.sg3;u32(changed,40680U+72U+68U,1);write(f.data/"DATA/roads.sg3",changed);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"alpha");
    write(f.data/"DATA/roads.sg3",f.sg3);
    write(f.data/"DATA/roads.555",Bytes{0,0,0,0});
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"payload bounds");
    write(f.data/"DATA/roads.555",f.bitmap);
    changed=f.sg3;u16(changed,40680U+72U+20U,5000);
    u16(changed,40680U+72U+22U,5000);write(f.data/"DATA/roads.sg3",changed);
    rejects([&]{openemperor::assets::load_road_visual_profile(f.data,f.manifest);},"RGBA budget");
    write(f.data/"DATA/roads.sg3",f.sg3);
}
void render_tests(Fixture& f) {
    using namespace openemperor;
    Json all=f.profile();all["tiles"]=Json::object();
    for (int mask=0;mask<16;++mask) all["tiles"][key(mask)]=f.entry(mask+1);
    f.save(all);
    const auto profile=assets::load_road_visual_profile(f.data,f.manifest);
    check(profile.configured_count()==16 && profile.unique_images.size()==16,"16 fixture images");
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("roads",256,128,SDL_WINDOW_HIDDEN,&window,&renderer),
          "software renderer");
    RoadSpriteSet sprites;sprites.initialize(renderer,profile);
    check(sprites.texture_count()==16,"16 texture uploads");
    for (int mask=0;mask<16;++mask) {
        check(SDL_SetRenderDrawColor(renderer,10,10,10,255) && SDL_RenderClear(renderer) &&
              sprites.draw({100,60},1,profile,*profile.find(static_cast<std::uint8_t>(mask))),
              "road pixel draw");
        const auto actual=pixel(renderer,100,60);
        const auto& image=profile.unique_images.at(profile.find(static_cast<std::uint8_t>(mask))->image_index);
        const std::size_t p=(20U*78U+39U)*4U;
        check(actual[0]==image.pixels[p] && actual[1]==image.pixels[p+1] &&
              actual[2]==image.pixels[p+2],"wrong actual SDL mask pixel");
    }
    check(SDL_SetRenderDrawColor(renderer,10,10,10,255) && SDL_RenderClear(renderer) &&
          sprites.draw({100,60},2,profile,*profile.find(0)),"2x road anchor");
    check(pixel(renderer,100,60)[0]==profile.unique_images[0].pixels[(20U*78U+39U)*4U],
          "zoom moved ground anchor");
    check(SDL_RenderClear(renderer) && sprites.draw({150,100},4,profile,*profile.find(0)),
          "4x road draw");
    check(pixel(renderer,150,100)[0]==profile.unique_images[0].pixels[(20U*78U+39U)*4U],
          "4x zoom shifted the shared ground anchor");
    // Same projected ground: road, then building, then walker. The final pixel is the walker.
    std::array<SandboxVisualKind,3> order{{SandboxVisualKind::Walker,
        SandboxVisualKind::Road,SandboxVisualKind::Building}};
    std::sort(order.begin(),order.end());
    check(order==std::array<SandboxVisualKind,3>{{SandboxVisualKind::Road,
        SandboxVisualKind::Building,SandboxVisualKind::Walker}},"depth kind ordering");
    check(SDL_RenderClear(renderer),"depth clear");
    for (auto kind:order) {
        if (kind==SandboxVisualKind::Road)
            check(sprites.draw({100,60},1,profile,*profile.find(0)),"depth road");
        else {
            const SDL_FRect rect{95,55,10,10};
            check(SDL_SetRenderDrawColor(renderer,kind==SandboxVisualKind::Walker ? 255:0,
                kind==SandboxVisualKind::Building ? 255:0,0,255) &&
                SDL_RenderFillRect(renderer,&rect),"depth overlay");
        }
    }
    check(pixel(renderer,100,60)==std::array<std::uint8_t,4>{255,0,0,255},"walker over road");
    sprites.shutdown();check(RoadSpriteSet::live_texture_count()==0,"road texture leak");
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
}
int main() {
    try { Fixture fixture;profile_tests(fixture);render_tests(fixture);
          std::cout<<"road profile and SDL pixels passed\n";return 0; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
