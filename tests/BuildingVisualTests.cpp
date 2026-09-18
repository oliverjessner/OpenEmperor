#include "assets/BuildingVisualProfile.h"
#include "app/SandboxVisualOrder.h"
#include "renderer/BuildingSprite.h"
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
void u16(Bytes& bytes,std::size_t offset,std::uint16_t value) {
    bytes.at(offset)=static_cast<std::uint8_t>(value);
    bytes.at(offset+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    u16(bytes,offset,static_cast<std::uint16_t>(value));
    u16(bytes,offset+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output),"synthetic file write");
}
struct Fixture {
    fs::path root=fs::temp_directory_path()/
        ("openemperor-building-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"building.json";
    Bytes sg3,bitmap;
    Fixture() {
        fs::create_directories(data/"DATA");
        sg3.assign(40680U+4U*72U,0);
        u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
        u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
        const std::string name="synthetic.bmp";
        std::copy(name.begin(),name.end(),sg3.begin()+680);
        u32(sg3,680+124,4);
        const std::size_t record=40680U+3U*72U;
        u32(sg3,record,4);u32(sg3,record+4,12800);u32(sg3,record+8,12800);
        u16(sg3,record+20,158);u16(sg3,record+22,90);
        u16(sg3,record+50,30);sg3[record+55]=2;
        write(data/"DATA/building.sg3",sg3);
        bitmap.assign(12804,0);
        for (std::size_t at=4;at<bitmap.size();at+=2) u16(bitmap,at,0x03e0);
        write(data/"DATA/building.555",bitmap);
    }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    Json valid() const {
        return {{"schema_version",1},{"mode","curated_building_preview"},
            {"buildings",{{"pottery",{{"archive","DATA/building.sg3"},
                {"image_index",3},{"ground_anchor",{79,70}},
                {"evidence","Independent synthetic Type-30 fixture"}}}}}};
    }
    void save(const Json& json) const {
        std::ofstream output(manifest);output<<json.dump();check(bool(output),"manifest write");
    }
};
template<class F> void rejects(F operation,const char* message) {
    try { operation(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"read software renderer");
    std::array<std::uint8_t,4> value{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&value[0],&value[1],&value[2],&value[3]);
    SDL_DestroySurface(surface);check(okay,"read software pixel");return value;
}
void profile_checks(Fixture& fixture) {
    fixture.save(fixture.valid());
    const auto loaded=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    const auto* pottery=loaded.find(openemperor::assets::BuildingVisualRole::Pottery);
    check(pottery && pottery->id.image_index==3 && loaded.unique_images.size()==1 &&
          loaded.unique_images[pottery->image_index].width==158 &&
          loaded.unique_images[pottery->image_index].height==90 &&
          pottery->ground_x==79 && pottery->ground_y==70,
          "physical Type-30 building load");
    auto shared=fixture.valid();
    shared["buildings"]["warehouse"]=shared["buildings"]["pottery"];
    fixture.save(shared);
    const auto dedup=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(dedup.unique_images.size()==1 &&
          dedup.find(openemperor::assets::BuildingVisualRole::Warehouse)->image_index==
          dedup.find(openemperor::assets::BuildingVisualRole::Pottery)->image_index,
          "two roles did not deduplicate one physical asset");
    const auto make_copy=[&](const char* name,std::uint16_t color) {
        fs::copy_file(fixture.data/"DATA/building.sg3",fixture.data/(std::string("DATA/")+name+".sg3"));
        auto bitmap=fixture.bitmap;
        for (std::size_t at=4;at<bitmap.size();at+=2) u16(bitmap,at,color);
        write(fixture.data/(std::string("DATA/")+name+".555"),bitmap);
    };
    make_copy("clay",0x03ff);make_copy("warehouse",0x7fe0);make_copy("house",0x03e0);
    auto all=fixture.valid();
    for (const auto& [role,name]:std::array<std::pair<const char*,const char*>,3>{{
            {"clay_source","clay"},{"warehouse","warehouse"},{"household","house"}}}) {
        all["buildings"][role]=all["buildings"]["pottery"];
        all["buildings"][role]["archive"]=std::string("DATA/")+name+".sg3";
    }
    fixture.save(all);
    const auto four=openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);
    check(four.unique_images.size()==4 &&
          std::all_of(openemperor::assets::building_roles.begin(),
                      openemperor::assets::building_roles.end(),
                      [&](auto role){return four.find(role)!=nullptr;}),
          "four distinct building assets failed");
    const auto raw_duplicate=R"({"schema_version":1,"mode":"curated_building_preview","buildings":{"pottery":{},"pottery":{}}})";
    { std::ofstream out(fixture.manifest);out<<raw_duplicate; }
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "duplicate semantic role accepted");
    auto bad=fixture.valid();bad["schema_version"]=2;fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown schema accepted");
    bad=fixture.valid();bad["buildings"]["unsupported"]=bad["buildings"]["pottery"];
    fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unknown role accepted");
    bad=fixture.valid();bad["buildings"]["pottery"]["image_index"]=4;fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "outside record accepted");
    bad=fixture.valid();bad["buildings"]["pottery"]["archive"]="../outside.sg3";
    fixture.save(bad);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "traversal accepted");
    for (const auto& value:Json::array({Json("NaN"),Json("Infinity"),Json(5000)})) {
        bad=fixture.valid();bad["buildings"]["pottery"]["ground_anchor"][0]=value;
        fixture.save(bad);
        rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
                "invalid anchor accepted");
    }
    fixture.save(fixture.valid());
    const auto outside=fixture.root/"outside.sg3";
    fs::rename(fixture.data/"DATA/building.sg3",outside);
    fs::create_symlink(outside,fixture.data/"DATA/building.sg3");
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "symlink escape accepted");
    fs::remove(fixture.data/"DATA/building.sg3");fs::rename(outside,fixture.data/"DATA/building.sg3");
    const auto outside_bitmap=fixture.root/"outside.555";
    fs::rename(fixture.data/"DATA/building.555",outside_bitmap);
    fs::create_symlink(outside_bitmap,fixture.data/"DATA/building.555");
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "bitmap symlink escape accepted");
    fs::remove(fixture.data/"DATA/building.555");
    fs::rename(outside_bitmap,fixture.data/"DATA/building.555");
    auto changed=fixture.sg3;
    u16(changed,40680U+3U*72U+50U,256);write(fixture.data/"DATA/building.sg3",changed);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "wrong image type accepted");
    changed=fixture.sg3;u32(changed,40680U+3U*72U+16U,10);
    write(fixture.data/"DATA/building.sg3",changed);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "unverified mirror accepted");
    changed=fixture.sg3;u16(changed,40680U+3U*72U+20U,5000);
    u16(changed,40680U+3U*72U+22U,5000);write(fixture.data/"DATA/building.sg3",changed);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "oversize RGBA accepted");
    write(fixture.data/"DATA/building.sg3",fixture.sg3);
    write(fixture.data/"DATA/building.555",Bytes{0,0,0,0,1});
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "truncated bitmap accepted");
    changed=fixture.sg3;u32(changed,40680U+3U*72U+4U,12801);
    write(fixture.data/"DATA/building.sg3",changed);
    auto corrupt=fixture.bitmap;corrupt.push_back(1); // Omega literal lacks RGB555 bytes.
    write(fixture.data/"DATA/building.555",corrupt);
    rejects([&]{openemperor::assets::load_building_visual_profile(fixture.data,fixture.manifest);},
            "malformed overlay accepted");
    write(fixture.data/"DATA/building.sg3",fixture.sg3);
    write(fixture.data/"DATA/building.555",fixture.bitmap);
    fixture.save(fixture.valid());
}
void pixels_and_depth() {
    using namespace openemperor;
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("building software",600,600,SDL_WINDOW_HIDDEN,
                                      &window,&renderer),"software renderer");
    assets::BuildingVisualProfile profile;
    assets::BuildingVisualEntry entry;entry.ground_x=60;entry.ground_y=70;
    profile.unique_images.emplace_back();
    auto& image=profile.unique_images.front();image.width=120;image.height=90;
    image.pixels.assign(120U*90U*4U,255);
    for (std::size_t at=0;at<image.pixels.size();at+=4) {
        image.pixels[at]=0;image.pixels[at+1]=255;image.pixels[at+2]=0;
    }
    image.pixels[0]=255;image.pixels[1]=0; // Asymmetric top-left red pixel.
    BuildingSprite sprite;sprite.initialize(renderer,profile);
    check(sprite.texture_count()==1 && BuildingSprite::live_texture_count()==1,
          "building texture upload count");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
          sprite.draw({100,100},1,profile,entry),"1x building draw");
    check(pixel(renderer,40,30)[0]==255 && pixel(renderer,100,100)[1]==255,
          "explicit 1x anchor or full image size changed");
    check(SDL_RenderClear(renderer) && sprite.draw({300,300},4,profile,entry),"4x building draw");
    check(pixel(renderer,60,20)[0]==255 && pixel(renderer,300,300)[1]==255,
          "4x anchor or nearest pixel changed");
    check(SDL_RenderClear(renderer) && sprite.draw({320,330},4,profile,entry),"panned draw");
    check(pixel(renderer,80,50)[0]==255 && pixel(renderer,320,330)[1]==255,
          "camera pan changed relative anchor");
    check(SDL_RenderClear(renderer) && sprite.draw({100,100},1,profile,entry) &&
          sprite.draw({300,300},1,profile,entry),"two building instances");
    check(sprite.texture_count()==1 && pixel(renderer,300,300)[1]==255,
          "two instances did not share one texture");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
          sprite.draw({100,100},1,profile,entry,true),"translucent placement draw");
    const auto preview_corner=pixel(renderer,40,30);
    check(preview_corner[0]>100 && preview_corner[0]<200 &&
          SDL_RenderClear(renderer) && sprite.draw({100,100},1,profile,entry) &&
          pixel(renderer,40,30)[0]==255,
          "placement preview and final image used different anchors");
    struct Layer { SandboxVisualKey key;int kind; };
    std::array<Layer,3> layers{{{{90,100,SandboxVisualKind::Walker,1},0},
                                 {{100,100,SandboxVisualKind::Building,2},1},
                                 {{110,100,SandboxVisualKind::Walker,2},2}}};
    std::sort(layers.begin(),layers.end(),[](const Layer& a,const Layer& b){return a.key<b.key;});
    check(layers[0].kind==0 && layers[1].kind==1 && layers[2].kind==2,
          "ground-depth sort order");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),"depth clear");
    for (const auto& layer:layers) {
        if (layer.kind==1) check(sprite.draw({100,100},1,profile,entry),"depth building");
        else {
            const SDL_FRect rect=layer.kind==0 ? SDL_FRect{85,70,20,20}:
                                                  SDL_FRect{100,85,20,20};
            check(SDL_SetRenderDrawColor(renderer,layer.kind==0 ? 0:255,0,
                                         layer.kind==0 ? 255:0,255) &&
                  SDL_RenderFillRect(renderer,&rect),"depth walker");
        }
    }
    check(pixel(renderer,95,85)==std::array<std::uint8_t,4>{0,255,0,255} &&
          pixel(renderer,105,90)==std::array<std::uint8_t,4>{255,0,0,255},
          "actual SDL depth pixels wrong");
    check(SandboxVisualKey{100,100,SandboxVisualKind::Building,1}<
          SandboxVisualKey{100,100,SandboxVisualKind::Walker,1},"tie breaker order");
    sprite.shutdown();check(BuildingSprite::live_texture_count()==0,"texture leaked");
    assets::BuildingVisualProfile four;
    const std::array<std::array<std::uint8_t,4>,4> colors{{
        {{0,255,255,255}},{{255,0,255,255}},{{255,255,0,255}},{{0,255,0,255}}}};
    for (std::size_t i=0;i<colors.size();++i) {
        assets::BuildingVisualEntry item;item.image_index=i;item.ground_x=10;item.ground_y=10;
        four.entries[i]=item;
        assets::RgbaImage colored;colored.width=20;colored.height=20;
        for (int p=0;p<400;++p)
            colored.pixels.insert(colored.pixels.end(),colors[i].begin(),colors[i].end());
        four.unique_images.push_back(std::move(colored));
    }
    sprite.initialize(renderer,four);
    check(sprite.texture_count()==4,"four unique assets did not upload four textures");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer) &&
          sprite.draw({100,200},1,four,*four.find(assets::BuildingVisualRole::ClaySource),true),
          "building hover preview draw");
    const auto translucent=pixel(renderer,100,200);
    check(translucent[0]<20 && translucent[1]>100 && translucent[1]<200 &&
          translucent[2]>100 && translucent[2]<200,
          "hover preview did not use translucent original sprite");
    check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),
          "role color clear");
    for (std::size_t i=0;i<colors.size();++i) {
        const auto* selected=four.find(assets::building_roles[i]);
        check(selected && sprite.draw({50.0+50.0*i,300},1,four,*selected),"role sprite draw");
        check(pixel(renderer,50+static_cast<int>(50*i),300)==colors[i],
              "role chose wrong uploaded texture");
    }
    for (std::size_t i=0;i<colors.size();++i) {
        const auto* selected=four.find(assets::building_roles[i]);
        check(SDL_SetRenderDrawColor(renderer,20,30,40,255) && SDL_RenderClear(renderer),
              "depth role clear");
        // A farther blue walker, one shared building draw, then a nearer red walker.
        const SDL_FRect back{90,90,20,20},front{100,100,20,20};
        check(SDL_SetRenderDrawColor(renderer,0,0,255,255) &&
              SDL_RenderFillRect(renderer,&back) &&
              sprite.draw({100,100},1,four,*selected) &&
              SDL_SetRenderDrawColor(renderer,255,0,0,255) &&
              SDL_RenderFillRect(renderer,&front),"role depth draw");
        check(pixel(renderer,95,95)==colors[i] &&
              pixel(renderer,105,105)==std::array<std::uint8_t,4>{255,0,0,255},
              "shared depth path did not occlude walkers by ground order");
    }
    sprite.shutdown();check(BuildingSprite::live_texture_count()==0,"four textures leaked");
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
}
int main() {
    try { Fixture fixture;profile_checks(fixture);pixels_and_depth();
        std::cout<<"building manifest, anchor and software depth pixels passed\n";return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
