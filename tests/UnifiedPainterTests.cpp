#include "maps/StoredGraphicsPlan.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "scene/WorldDrawOrder.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
namespace maps = openemperor::maps;
namespace scene = openemperor::scene;
using Bytes = std::vector<std::uint8_t>;

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void u16(Bytes& b, std::size_t at, std::uint16_t value) {
    b.at(at)=static_cast<std::uint8_t>(value);
    b.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& b, std::size_t at, std::uint32_t value) {
    u16(b,at,static_cast<std::uint16_t>(value));
    u16(b,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    check(bool(out),"write independent SG3 fixture");
}
struct Temp {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("openemperor-depth-"+
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { std::filesystem::create_directories(root/"DATA"); }
    ~Temp() { std::error_code error;std::filesystem::remove_all(root,error); }
};

struct SyntheticMap {
    maps::StoredGraphicsPlan plan;
    scene::Point ground;
    scene::Point sample;
};

SyntheticMap make_map(const Temp& temp, unsigned side, bool valid=true) {
    check(side==1 || side==2 || side==4,"supported test side");
    const auto width=80U*side-2U;
    const auto height=40U*side+60U;
    const auto base=3200U*side*side;
    const auto overlay_y=40U*(side-1U)+20U;
    const auto target=(overlay_y-10U)*width+width/2U-2U;
    Bytes overlay;
    const auto skip=[&](unsigned count) {
        for (unsigned remaining=count;remaining>0;) {
            const auto step=std::min(remaining,254U);
            overlay.push_back(255);overlay.push_back(static_cast<std::uint8_t>(step));
            remaining-=step;
        }
    };
    skip(target);
    // Independently encoded 5x21 green vertical object, above the footprint.
    for (int row=0;row<21;++row) {
        overlay.push_back(5);
        for (int x=0;x<5;++x) overlay.insert(overlay.end(),{0xe0,0x03});
        if (row!=20) skip(width-5U);
    }
    const auto size=base+static_cast<unsigned>(overlay.size());
    Bytes sg3(40680U+2U*72U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,2);u32(sg3,16,2);u32(sg3,20,1);
    const std::string name="synthetic.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);
    u32(sg3,680+124,2);
    const auto at=40680U+72U;
    u32(sg3,at,4);u32(sg3,at+4,size);u32(sg3,at+8,base);
    u16(sg3,at+20,static_cast<std::uint16_t>(width));
    u16(sg3,at+22,static_cast<std::uint16_t>(height));
    u16(sg3,at+50,30);sg3[at+55]=static_cast<std::uint8_t>(side);
    write(temp.root/"DATA/occlusion.sg3",sg3);
    Bytes bitmap(4U+base,0);
    bitmap.insert(bitmap.end(),overlay.begin(),overlay.end());
    if (!valid) bitmap.resize(4); // Explicit diagnostic path.
    write(temp.root/"DATA/occlusion.555",bitmap);

    maps::StoredGraphicsPlan plan;
    plan.data_root=temp.root;plan.border=72;
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);
    plan.cell_by_storage.resize(228U*228U);
    openemperor::assets::AssetRecord record;
    record.id={"DATA/occlusion.sg3",1};
    record.width=static_cast<std::int16_t>(width);
    record.height=static_cast<std::int16_t>(height);
    record.data_length=size;record.uncompressed_length=base;
    record.image_type=30;record.isometric_size_flag=static_cast<std::uint8_t>(side);
    maps::StoredAsset asset;
    asset.record=record;
    plan.assets.push_back(std::move(asset));
    maps::PlacedFootprint footprint;
    footprint.id=0;footprint.asset_index=0;footprint.origin={100,100};
    footprint.width_cells=footprint.height_cells=side;
    const auto rear=maps::terrain_world({100,100},plan.border);
    footprint.image_origin=maps::stored_square_image_origin(rear,width,height,side);
    for (unsigned dy=0;dy<side;++dy) for (unsigned dx=0;dx<side;++dx) {
        maps::StoredCell cell;
        cell.storage={100+dx,100+dy};
        cell.cell_index=static_cast<std::size_t>(cell.storage.y)*228U+cell.storage.x;
        cell.asset_index=0;cell.footprint_index=0;
        cell.status=maps::StoredStatus::DecodePending;
        cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=footprint.image_origin;
        plan.cell_by_storage[cell.cell_index]=plan.cells.size();
        plan.status_by_storage[cell.cell_index]=cell.status;
        footprint.cell_indices.push_back(plan.cells.size());
        plan.cells.push_back(cell);
    }
    plan.footprints.push_back(std::move(footprint));
    const auto front=maps::GridCell{100+side-1U,100+side-1U};
    const auto ground=maps::terrain_ground(front,plan.border);
    return {std::move(plan),ground,{ground.x,ground.y-60.0}};
}

std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"read SDL software pixels");
    std::array<std::uint8_t,4> rgba{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&rgba[0],&rgba[1],&rgba[2],&rgba[3]);
    SDL_DestroySurface(surface);
    check(okay,"read sample pixel");
    return rgba;
}

struct SandboxItem { scene::WorldDrawKey key; SDL_Color color; };

bool draw_marker(SDL_Renderer* renderer,const scene::Camera2D& camera,
                 scene::Point ground,SDL_Color color) {
    const auto p=camera.world_to_screen(ground);
    const SDL_FRect rect{static_cast<float>(p.x-2),static_cast<float>(p.y-100),5,101};
    return SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255) &&
           SDL_RenderFillRect(renderer,&rect);
}

void run_case(SDL_Renderer* renderer,const Temp& temp,unsigned side) {
    auto fixture=make_map(temp,side);
    openemperor::StoredGraphicsRenderer map(std::move(fixture.plan));
    map.initialize(renderer);
    check(map.upload_count()==1 && map.draw_items().size()==1 &&
          map.stored_order_builds()==1,"static stored footprint list");
    check(map.draw_items()[0].key.depth==fixture.ground.y &&
          map.draw_items()[0].key.ground_x==fixture.ground.x &&
          map.draw_items()[0].key.layer==scene::WorldVisualLayer::StoredMap,
          "footprint front ground key");
    scene::Camera2D camera;camera.viewport_width=320;camera.viewport_height=320;
    camera.center_on(fixture.sample);
    const auto screen=camera.world_to_screen(fixture.sample);
    const auto sx=static_cast<int>(screen.x),sy=static_cast<int>(screen.y);
    const auto red=SDL_Color{255,0,0,255};
    for (int direction : {-1,0,1}) {
        // Both sprite rectangles cover the same green upper pixel. Only their
        // projected ground depth changes; their image tops are not sort keys.
        const scene::Point walker_ground{fixture.ground.x,
                                         fixture.ground.y+direction*20.0};
        const SandboxItem walker{{walker_ground.y,walker_ground.x,
                                  scene::WorldVisualLayer::SandboxWalker,1},red};
        const std::array<SandboxItem,1> sandbox{walker};
        scene::WorldMergeStats stats;
        check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer),
              "clear pixel frame");
        map.begin_frame();
        check(scene::merge_world_draw_streams(map.draw_items(),sandbox,
            [&](std::size_t i) { return map.draw_item(i,camera); },
            [&](std::size_t) { return draw_marker(renderer,camera,walker_ground,red); },stats),
            "draw merged map and walker");
        const auto actual=pixel(renderer,sx,sy);
        const bool map_in_front=direction<0;
        check(map_in_front ? actual==std::array<std::uint8_t,4>{0,255,0,255}:
                             actual==std::array<std::uint8_t,4>{255,0,0,255},
              "ground depth did not control actual SDL occlusion pixel");
        check(stats.stored_items_visited==1 && stats.sandbox_items==1 &&
              stats.stored_before_sandbox_count==static_cast<unsigned>(!map_in_front) &&
              stats.sandbox_before_stored_count==static_cast<unsigned>(map_in_front),
              "merge counters or order");
    }
    // Legacy pass always leaves the walker on top, including the case where
    // the stored item's ground should have put it in front.
    check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
          map.render(camera,std::nullopt) &&
          draw_marker(renderer,camera,{fixture.ground.x,fixture.ground.y-20},red) &&
          pixel(renderer,sx,sy)==std::array<std::uint8_t,4>{255,0,0,255},
          "legacy comparison frame");
    const auto uploads=map.upload_count();
    for (int i=0;i<100;++i) {
        map.begin_frame();
        check(map.draw_item(0,camera),"static order frame");
    }
    check(map.stored_order_builds()==1 && map.upload_count()==uploads,
          "normal frames rebuilt order or uploaded textures");
    map.shutdown();
    check(openemperor::StoredGraphicsRenderer::live_texture_count()==0,
          "stored texture released");
}

void layer_case(SDL_Renderer* renderer,const Temp& temp) {
    auto fixture=make_map(temp,1);
    openemperor::StoredGraphicsRenderer map(std::move(fixture.plan));
    map.initialize(renderer);
    scene::Camera2D camera;camera.viewport_width=320;camera.viewport_height=320;
    camera.center_on(fixture.sample);
    const auto screen=camera.world_to_screen(fixture.sample);
    const auto sx=static_cast<int>(screen.x),sy=static_cast<int>(screen.y);
    const std::array<scene::WorldVisualLayer,3> layers{
        scene::WorldVisualLayer::SandboxRoad,scene::WorldVisualLayer::SandboxBuilding,
        scene::WorldVisualLayer::SandboxWalker};
    const std::array<SDL_Color,3> colors{
        SDL_Color{255,255,0,255},SDL_Color{0,0,255,255},SDL_Color{255,0,0,255}};
    for (std::size_t n=0;n<layers.size();++n) {
        const SandboxItem item{{fixture.ground.y,fixture.ground.x,layers[n],n},colors[n]};
        const std::array<SandboxItem,1> sandbox{item};
        scene::WorldMergeStats stats;
        check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer),
              "clear layer frame");
        map.begin_frame();
        check(scene::merge_world_draw_streams(map.draw_items(),sandbox,
            [&](std::size_t i) { return map.draw_item(i,camera); },
            [&](std::size_t) { return draw_marker(renderer,camera,fixture.ground,colors[n]); },
            stats) && pixel(renderer,sx,sy)==
                std::array<std::uint8_t,4>{colors[n].r,colors[n].g,colors[n].b,255},
              "equal-ground map/road/building/walker layer pixel");
    }
    map.shutdown();
}

void diagnostic_case(SDL_Renderer* renderer,const Temp& temp) {
    auto fixture=make_map(temp,1,false);
    openemperor::StoredGraphicsRenderer map(std::move(fixture.plan));
    map.initialize(renderer);
    check(map.plan().assets[0].status==maps::StoredStatus::DecodeFailed &&
          map.draw_items().size()==1,"malformed image did not retain one diagnostic item");
    const auto diagnostic=maps::terrain_ground({100,100},72);
    scene::Camera2D camera;camera.viewport_width=320;camera.viewport_height=320;
    camera.center_on(diagnostic);
    const auto blue=SDL_Color{0,0,255,255};
    for (int direction : {-1,1}) {
        const SandboxItem item{{diagnostic.y+direction*20.0,diagnostic.x,
                                scene::WorldVisualLayer::SandboxWalker,1},blue};
        const std::array<SandboxItem,1> sandbox{item};
        scene::WorldMergeStats stats;
        check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer),
              "clear diagnostic frame");
        map.begin_frame();
        check(scene::merge_world_draw_streams(map.draw_items(),sandbox,
            [&](std::size_t i) { return map.draw_item(i,camera); },
            [&](std::size_t) {
                const SDL_FRect rect{158,158,5,5};
                return SDL_SetRenderDrawColor(renderer,blue.r,blue.g,blue.b,255) &&
                       SDL_RenderFillRect(renderer,&rect);
            },stats) && map.last_diagnostic_draws()==1,
            "diagnostic marker lost in item-by-item drawing");
        const auto actual=pixel(renderer,160,160);
        if (direction<0)
            check(actual[0]>70 && actual[2]>70 && actual[1]<80,
                  "front stored diagnostic did not cover sandbox marker");
        else check(actual==std::array<std::uint8_t,4>{0,0,255,255},
                   "front sandbox marker did not cover stored diagnostic");
    }
    map.shutdown();
}
} // namespace

int main() {
    try {
        for (unsigned border : {0U,72U,101U}) for (const auto cell : {
            maps::GridCell{0,0},maps::GridCell{100,105},maps::GridCell{227,227}}) {
            const auto top=maps::terrain_world(cell,border);
            const auto ground=maps::terrain_ground(cell,border);
            const double u=static_cast<double>(cell.x)-border;
            const double v=static_cast<double>(cell.y)-border;
            check(ground.x==top.x && ground.y==top.y+20.0 &&
                  ground.x==(u-v)*40.0 && ground.y==(u+v)*20.0+20.0,
                  "terrain/sandbox ground convention");
        }
        for (unsigned border : {0U,72U,101U}) {
            const std::array<maps::GridCell,5> cells{{{100,100},{101,100},{100,101},
                                                       {101,101},{104,98}}};
            for (const auto a:cells) for (const auto b:cells) {
                const auto ta=maps::terrain_world(a,border),tb=maps::terrain_world(b,border);
                const auto ga=maps::terrain_ground(a,border),gb=maps::terrain_ground(b,border);
                check((std::tie(ta.y,ta.x)<std::tie(tb.y,tb.x)) ==
                      (std::tie(ga.y,ga.x)<std::tie(gb.y,gb.x)),
                      "+20 normalization changed stored-only relative order");
            }
        }
        Temp temp;
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") &&
              SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") && SDL_Init(SDL_INIT_VIDEO),
              "software SDL init");
        SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("depth painter",320,320,SDL_WINDOW_HIDDEN,
                                         &window,&renderer),"software window");
        run_case(renderer,temp,1);
        run_case(renderer,temp,2);
        run_case(renderer,temp,4);
        layer_case(renderer,temp);
        diagnostic_case(renderer,temp);
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"unified painter synthetic SDL pixels passed\n";
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
