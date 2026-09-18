#include "assets/WalkerVisualProfile.h"
#include "app/WalkerPose.h"
#include "renderer/WalkerSpriteSet.h"
#include "simulation/World.h"
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
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Json=nlohmann::json;
void check(bool okay,const char* reason) { if (!okay) throw std::runtime_error(reason); }
void u16(Bytes& bytes,std::size_t at,std::uint16_t value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);
    bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    u16(bytes,at,static_cast<std::uint16_t>(value)); u16(bytes,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out),"fixture write");
}
struct Fixture {
    fs::path root=fs::temp_directory_path()/
        ("openemperor-walker-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data", manifest=root/"walker.json";
    Bytes first,second;
    Fixture() {
        fs::create_directories(data/"DATA");
        // Independently constructed Type-256 Omega payloads; no game bytes.
        first={255,1,1,0x00,0x7c};
        for (int row=1;row<4;++row) first.insert(first.end(),{2,0x00,0x7c,0x00,0x7c});
        for (int row=0;row<2;++row)
            second.insert(second.end(),{4,0xe0,0x03,0xe0,0x03,0xe0,0x03,0xe0,0x03});
        Bytes sg3(40680+8*72,0);
        u32(sg3,0,static_cast<std::uint32_t>(sg3.size())); u32(sg3,4,214);
        u32(sg3,12,8); u32(sg3,16,8); u32(sg3,20,1);
        const std::string group="synthetic.bmp";
        std::copy(group.begin(),group.end(),sg3.begin()+680);
        u32(sg3,680+124,8);
        for (const auto [index,offset,length,width,height]:{
            std::array<std::uint32_t,5>{1,4,static_cast<std::uint32_t>(first.size()),2,4},
            std::array<std::uint32_t,5>{3,4+static_cast<std::uint32_t>(first.size()),
                static_cast<std::uint32_t>(second.size()),4,2}}) {
            const auto at=40680+index*72;
            u32(sg3,at,offset); u32(sg3,at+4,length);
            u16(sg3,at+20,static_cast<std::uint16_t>(width));
            u16(sg3,at+22,static_cast<std::uint16_t>(height));
            u16(sg3,at+50,256);
        }
        write(data/"DATA/walker.sg3",sg3);
        Bytes bitmap{0,0,0,0};bitmap.insert(bitmap.end(),first.begin(),first.end());
        bitmap.insert(bitmap.end(),second.begin(),second.end());
        write(data/"DATA/walker.555",bitmap);
    }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    Json valid() const {
        return {{"schema_version",1},{"mode","curated_walker_preview"},{"role","clay"},
            {"ticks_per_frame",2},{"evidence","Synthetic red and green asymmetric frames."},
            {"frames",Json::array({
                {{"alias","red"},{"archive","DATA/walker.sg3"},{"image_index",1},
                    {"foot_anchor",{1,3}}},
                {{"alias","green"},{"archive","DATA/walker.sg3"},{"image_index",3},
                    {"foot_anchor",{2,1}}},
                {{"alias","red_again"},{"archive","DATA/walker.sg3"},{"image_index",1},
                    {"foot_anchor",{1,3}}}})},
            {"clips",{{"pos_x",{"green","red","green"}},
                       {"neg_x",{"red","green"}},
                       {"pos_y",{"red"}},{"neg_y",{"green"}}}},
            {"idle","red"}};
    }
    void save(const Json& value) const { std::ofstream out(manifest);out<<value.dump(2);check(bool(out),"json write"); }
};
template<class F> void rejects(F operation,const char* reason) {
    try { operation(); } catch (const std::exception&) { return; }
    throw std::runtime_error(reason);
}
std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"software frame read");
    std::array<std::uint8_t,4> color{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&color[0],&color[1],&color[2],&color[3]);
    SDL_DestroySurface(surface);check(okay,"software pixel read");return color;
}
void profile_checks(Fixture& fixture) {
    fixture.save(fixture.valid());
    const auto profile=openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest);
    check(profile.frames.size()==3 && profile.unique_images.size()==2,"physical frame dedup");
    check(profile.frames[0].id.image_index==1 && profile.frames[1].id.image_index==3,
          "physical SG3 indices shifted");
    check(profile.frames[0].image_index==profile.frames[2].image_index,"shared asset not deduped");
    check(profile.clips[0]==std::vector<std::size_t>({1,0,1}),"explicit clip order sorted");
    auto invalid=fixture.valid();invalid["schema_version"]=2;fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"version accepted");
    invalid=fixture.valid();invalid["clips"]["pos_x"]=Json::array();fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"empty clip accepted");
    invalid=fixture.valid();invalid["ticks_per_frame"]=0;fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"zero tick accepted");
    invalid=fixture.valid();invalid["clips"]["pos_x"]={"missing"};fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"missing alias accepted");
    invalid=fixture.valid();invalid["frames"][0]["archive"]="../outside.sg3";fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"traversal accepted");
    invalid=fixture.valid();invalid["frames"][0]["foot_anchor"]={1,1e100};fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"anchor accepted");
    invalid=fixture.valid();invalid["frames"][2]["alias"]="red";fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"alias duplicate accepted");
    invalid=fixture.valid();invalid["frames"][1]["image_index"]=7;fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"empty frame accepted");
    fixture.save(fixture.valid());
    const auto bitmap=fixture.data/"DATA/walker.555";
    const auto outside=fixture.root/"outside.555";
    fs::rename(bitmap,outside);fs::create_symlink(outside,bitmap);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"symlink escape accepted");
    fs::remove(bitmap);fs::rename(outside,bitmap);
    invalid=fixture.valid();invalid["ticks_per_frame"]=1001;fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"tick limit accepted");
    invalid=fixture.valid();invalid["ticks_per_frame"]=4294967296ULL;fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"tick overflow accepted");
    invalid=fixture.valid();invalid["frames"][0]["image_index"]=4294967297ULL;fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"physical index overflow accepted");
    invalid=fixture.valid();invalid["clips"]=Json::object();fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"no moving clips accepted");
    invalid=fixture.valid();invalid["clips"]["pos_x"]=Json::array();
    for (int i=0;i<65;++i) invalid["clips"]["pos_x"].push_back("red");
    fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"clip limit accepted");
    invalid=fixture.valid();
    for (int i=0;i<257;++i) {
        auto frame=invalid["frames"][0];frame["alias"]="alias-"+std::to_string(i);
        invalid["frames"].push_back(frame);
    }
    fixture.save(invalid);
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"frame limit accepted");
    fixture.save(fixture.valid());
    const auto original=fixture.first;
    write(bitmap,Bytes{0,0,0,0,255});
    rejects([&]{ openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest); },"damaged frame accepted");
    Bytes restored{0,0,0,0};restored.insert(restored.end(),original.begin(),original.end());
    restored.insert(restored.end(),fixture.second.begin(),fixture.second.end());
    write(bitmap,restored);
    fixture.save(fixture.valid());
}
void pose_checks(const openemperor::assets::WalkerVisualProfile& profile) {
    using namespace openemperor;
    simulation::CourierState courier;
    courier.enabled=true; courier.role=simulation::CourierRole::Clay;
    courier.phase=simulation::CourierPhase::ToWarehouse;
    courier.path={{2,2},{3,2}};courier.cargo=3;
    auto pose=walker_pose(courier,0,profile);
    check(pose.moving && pose.loaded && pose.direction==assets::StorageDirection::PosX &&
          pose.frame==1,"pos_x pose");
    check(walker_pose(courier,2,profile).frame==0 && walker_pose(courier,4,profile).frame==1,
          "world-tick frame selection");
    check(walker_pose(courier,2,profile).frame==walker_pose(courier,2,profile).frame,
          "render frequency affected frame");
    courier.path={{3,2},{2,2}};
    check(walker_pose(courier,0,profile).direction==assets::StorageDirection::NegX,"neg_x pose");
    courier.path={{2,2},{2,3}};
    check(walker_pose(courier,0,profile).direction==assets::StorageDirection::PosY,"pos_y pose");
    courier.path={{2,3},{2,2}};
    check(walker_pose(courier,0,profile).direction==assets::StorageDirection::NegY,"neg_y pose");
    courier.route_pending=true;courier.edge_progress=5;
    check(walker_pose(courier,6,profile).moving,"protected edge stopped prematurely");
    courier.edge_progress=0;
    check(!walker_pose(courier,6,profile).moving && walker_pose(courier,6,profile).frame==0,
          "waiting courier animated");
    courier.route_pending=false;courier.path={{2,3}};
    check(!walker_pose(courier,6,profile).moving,"one-point path animated");
    courier.path.clear();courier.phase=simulation::CourierPhase::IdleAtWorkshop;
    check(!walker_pose(courier,6,profile).moving,"idle courier animated");
    courier.path={{2,2},{4,2}};courier.phase=simulation::CourierPhase::Returning;
    check(walker_pose(courier,6,profile).fallback==WalkerFallback::InvalidEdge,"bad edge accepted");
    auto partial=profile;partial.clips[0].clear();courier.path={{2,2},{3,2}};
    check(walker_pose(courier,6,partial).fallback==WalkerFallback::UnmappedDirection &&
          !walker_pose(courier,6,partial).frame,"unmapped direction hidden");
}
void render_checks(const openemperor::assets::WalkerVisualProfile& profile) {
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("walker pixels",80,80,SDL_WINDOW_HIDDEN,&window,&renderer),
          "software renderer");
    openemperor::WalkerSpriteSet sprites;sprites.initialize(renderer,profile);
    check(sprites.texture_count()==2,"duplicate texture uploaded");
    check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer),"clear");
    check(sprites.draw(0,{20,20},2,profile,{0,0},{80,80}),"draw red");
    check(pixel(renderer,18,14)==std::array<std::uint8_t,4>({0,0,0,255}),"transparent pixel overwritten");
    check(pixel(renderer,20,20)[0]==255,"red foot placement");
    check(SDL_RenderClear(renderer),"clear second");
    check(sprites.draw(1,{20,20},2,profile,{0,0},{80,80}),"draw green");
    check(pixel(renderer,17,19)[1]==255 && pixel(renderer,23,21)[1]==255,
          "green frame size, anchor or full image lost");
    check(SDL_RenderClear(renderer),"clear cull");
    check(sprites.draw(0,{20,81},2,profile,{0,0},{80,80}),"edge draw");
    check(pixel(renderer,20,75)[0]==255,"visible upper sprite culled with offscreen foot");
    sprites.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
openemperor::assets::WalkerVisualProfile four_direction_profile() {
    using namespace openemperor::assets;
    WalkerVisualProfile profile;
    profile.ticks_per_frame=3;profile.idle_frame=0;
    constexpr std::array<std::array<std::uint8_t,4>,4> colors{{
        {{255,0,0,255}},{{0,255,0,255}},{{0,0,255,255}},{{255,255,0,255}}}};
    constexpr std::array<std::array<int,4>,4> geometry{{
        {{2,3,1,2}},{{3,2,2,1}},{{4,2,1,1}},{{2,4,1,3}}}};
    for (std::size_t i=0;i<4;++i) {
        const auto [width,height,foot_x,foot_y]=geometry[i];
        RgbaImage image;image.width=static_cast<std::uint16_t>(width);
        image.height=static_cast<std::uint16_t>(height);
        for (int j=0;j<width*height;++j)
            image.pixels.insert(image.pixels.end(),colors[i].begin(),colors[i].end());
        image.pixels[3]=0; // Distinct transparent top-left corner.
        if (i==3) {
            const std::size_t mixed=static_cast<std::size_t>(width+1)*4;
            image.pixels[mixed]=100;image.pixels[mixed+1]=0;
            image.pixels[mixed+2]=200;image.pixels[mixed+3]=128;
        }
        profile.unique_images.push_back(std::move(image));
        WalkerFrame frame;frame.alias="direction-"+std::to_string(i);
        frame.id={"DATA/synthetic.sg3",static_cast<std::uint32_t>(1+2*i)};
        frame.foot_x=foot_x;frame.foot_y=foot_y;frame.image_index=i;
        profile.frames.push_back(frame);profile.clips[i]={i};
    }
    return profile;
}
void four_direction_pixels(const openemperor::assets::WalkerVisualProfile& profile) {
    using namespace openemperor;
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("four direction pixels",100,100,SDL_WINDOW_HIDDEN,
                                      &window,&renderer),"software window");
    WalkerSpriteSet sprites;sprites.initialize(renderer,profile);
    check(sprites.texture_count()==4,"four directions did not upload four distinct textures");
    constexpr std::array<std::array<std::uint8_t,3>,4> expected{{
        {{255,0,0}},{{0,255,0}},{{0,0,255}},{{255,255,0}}}};
    for (std::size_t i=0;i<4;++i) {
        check(SDL_SetRenderDrawColor(renderer,20,40,60,255) && SDL_RenderClear(renderer),"dark clear");
        check(sprites.draw(i,{40,40},4,profile,{0,0},{100,100}),"direction draw");
        const auto& frame=profile.frames[i];
        const int left=40-static_cast<int>(frame.foot_x)*4;
        const int top=40-static_cast<int>(frame.foot_y)*4;
        check(pixel(renderer,left,top)==std::array<std::uint8_t,4>({20,40,60,255}),
              "transparent corner did not show dark background");
        const auto solid=pixel(renderer,left+5,top+1);
        check(solid[0]==expected[i][0] && solid[1]==expected[i][1] &&
              solid[2]==expected[i][2],"direction color/anchor/texture modulation mismatch");
    }
    const auto& half=profile.frames[3];
    const int half_x=40-static_cast<int>(half.foot_x)*4+5;
    const int half_y=40-static_cast<int>(half.foot_y)*4+5;
    const auto dark=pixel(renderer,half_x,half_y);
    const auto near=[](int actual,int expected){return std::abs(actual-expected)<=1;};
    check(near(dark[0],60) && near(dark[1],20) && near(dark[2],130),
          "half-alpha dark blend differs from straight-alpha expectation");
    check(SDL_SetRenderDrawColor(renderer,200,220,240,255) && SDL_RenderClear(renderer) &&
          sprites.draw(3,{40,40},4,profile,{0,0},{100,100}),"light blend render");
    const auto light=pixel(renderer,half_x,half_y);
    check(near(light[0],150) && near(light[1],110) && near(light[2],220),
          "half-alpha light blend or texture modulation mismatch");
    sprites.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
void simulation_neutrality(const openemperor::assets::WalkerVisualProfile& profile) {
    using namespace openemperor::simulation;
    World plain(11,5,std::vector<std::uint8_t>(55,1),RulesProfile::ProductionV2);
    World decorated(11,5,std::vector<std::uint8_t>(55,1),RulesProfile::ProductionV2);
    const auto place=[&](Command command) {
        check(plain.execute(command).accepted && decorated.execute(command).accepted,"placement");
    };
    place({CommandType::PlaceClaySource,{0,0}});
    place({CommandType::PlacePottery,{4,3}});
    place({CommandType::PlaceWarehouse,{10,3}});
    for (const Cell cell:{Cell{1,0},Cell{2,0},Cell{3,0},Cell{4,0},
                          Cell{4,1},Cell{4,2},Cell{5,3},Cell{6,3},
                          Cell{7,3},Cell{8,3},Cell{9,3}})
        place({CommandType::PlaceRoad,cell});
    std::array<bool,4> directions{};
    for (int tick=0;tick<1600;++tick) {
        plain.tick();decorated.tick();
        for (int draw=0;draw<5;++draw) {
            const auto pose=openemperor::walker_pose(decorated.courier(CourierId::Clay),
                                                     decorated.ticks(),profile);
            if (pose.moving && pose.direction) {
                check(pose.frame && pose.fallback==openemperor::WalkerFallback::None,
                      "real bent route used walker fallback");
                directions[openemperor::assets::direction_index(*pose.direction)]=true;
            }
        }
        check(plain.snapshot()==decorated.snapshot(),"walker query changed authoritative World");
        if (tick==150 || tick==250) {
            const auto saved=decorated.snapshot();
            auto resumed=World::restore(saved,std::vector<std::uint8_t>(55,1));
            check(resumed.snapshot()==saved &&
                  openemperor::walker_pose(resumed.courier(CourierId::Clay),resumed.ticks(),profile).frame==
                  openemperor::walker_pose(decorated.courier(CourierId::Clay),decorated.ticks(),profile).frame,
                  "restored pose differs");
        }
    }
    check(std::all_of(directions.begin(),directions.end(),[](bool seen){return seen;}) &&
          plain.production_balance_valid() && decorated.production_balance_valid(),
          "bent transport did not traverse all four storage directions");
}
}
void local_red_check(const fs::path& root,const fs::path& manifest) {
    const auto profile=openemperor::assets::load_walker_visual_profile(root,manifest);
    const auto frame=std::find_if(profile.frames.begin(),profile.frames.end(),[](const auto& candidate) {
        return candidate.id.archive_relative_path==fs::path("DATA/SprMain.sg3") &&
            candidate.id.image_index==109;
    });
    check(frame!=profile.frames.end(),"local red check requires physical SprMain record 109");
    const auto frame_index=static_cast<std::size_t>(frame-profile.frames.begin());
    const auto& image=profile.unique_images.at(frame->image_index);
    check(image.width>24 && image.height>30,"local red sample outside image");
    const auto source=static_cast<std::size_t>(30*image.width+24)*4;
    const std::array<std::uint8_t,4> decoded{image.pixels[source],image.pixels[source+1],
        image.pixels[source+2],image.pixels[source+3]};
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("local walker pixel",100,100,SDL_WINDOW_HIDDEN,
                                      &window,&renderer),"local software window");
    openemperor::WalkerSpriteSet sprites;sprites.initialize(renderer,profile);
    const auto sample=[&](std::array<std::uint8_t,3> background) {
        check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255) &&
              SDL_RenderClear(renderer),"local background clear");
        check(sprites.draw(frame_index,{frame->foot_x,frame->foot_y},1,profile,{0,0},{100,100}),
              "local sprite draw");
        return std::array{pixel(renderer,24,30),pixel(renderer,0,0)};
    };
    const auto dark=sample({20,40,60});const auto light=sample({200,220,240});
    std::cout<<Json{{"archive","DATA/SprMain.sg3"},{"physical_image_index",109},
        {"image_pixel",{24,30}},{"decoded_rgba",decoded},
        {"dark_sdl_rgba",dark[0]},{"light_sdl_rgba",light[0]},
        {"transparent_decoded_alpha",image.pixels[3]},
        {"dark_transparent_sdl_rgba",dark[1]},
        {"light_transparent_sdl_rgba",light[1]}}.dump()<<'\n';
    sprites.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
int main(int argc,char* argv[]) {
    try {
        if (argc==4 && std::string_view(argv[1])=="--local-red-check") {
            local_red_check(argv[2],argv[3]);return 0;
        }
        Fixture fixture;profile_checks(fixture);
        auto profile=openemperor::assets::load_walker_visual_profile(fixture.data,fixture.manifest);
        pose_checks(profile);render_checks(profile);
        const auto four=four_direction_profile();four_direction_pixels(four);simulation_neutrality(four);
        std::cout<<"walker profile, pose, software pixels and neutral World checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
