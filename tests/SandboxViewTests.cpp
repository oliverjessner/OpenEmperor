#include "app/SandboxView.h"
#include "maps/TerrainRenderPlan.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace maps=openemperor::maps;
namespace simulation=openemperor::simulation;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void u16(Bytes& b,std::size_t at,std::uint16_t value) {
    b.at(at)=static_cast<std::uint8_t>(value);
    b.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& b,std::size_t at,std::uint32_t value) {
    u16(b,at,static_cast<std::uint16_t>(value));
    u16(b,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out),"synthetic write");
}
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("openemperor-sandbox-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { std::filesystem::create_directories(path/"DATA"); }
    ~Temp() { std::error_code error; std::filesystem::remove_all(path,error); }
};
openemperor::maps::StoredMapSession fixture(const Temp& temp) {
    Bytes sg3(40680U+64U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));
    u32(sg3,4,213); u32(sg3,12,1); u32(sg3,16,1); u32(sg3,20,1);
    const std::string group_name="Zeus_system.bmp";
    std::copy(group_name.begin(),group_name.end(),sg3.begin()+680);
    u32(sg3,680+124,1); u32(sg3,680+128,1);
    u32(sg3,40680+4,3200); u32(sg3,40680+8,3200);
    u16(sg3,40680+20,78); u16(sg3,40680+22,40);
    u16(sg3,40680+50,30); sg3[40680+55]=1;
    Bytes bitmap(3200,0);
    for (std::size_t i=0;i<bitmap.size();i+=2) u16(bitmap,i,0x03e0);
    write(temp.path/"DATA/test.sg3",sg3);
    write(temp.path/"DATA/test.555",bitmap);
    maps::ParsedEmperorMap map;
    map.declared_map_size=84;
    maps::StoredGraphicsPlan plan;
    plan.data_root=temp.path;
    plan.profile=maps::StoredGraphicsProfile::Slot8;
    plan.border=maps::MapGeometry{84}.border;
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);
    plan.cell_by_storage.resize(228U*228U);
    openemperor::assets::AssetRecord record;
    record.id={"DATA/test.sg3",0};
    record.width=78; record.height=40; record.data_length=3200;
    record.uncompressed_length=3200; record.image_type=30;
    maps::StoredAsset asset;
    asset.record=record;
    plan.assets.push_back(std::move(asset));
    for (std::uint32_t x=110;x<=116;++x) {
        maps::StoredCell cell;
        cell.storage={x,114};
        cell.cell_index=static_cast<std::size_t>(114)*228+x;
        cell.terrain_raw=x==116 ? 0 : 0x80;
        cell.objects_raw=0;
        cell.status=maps::StoredStatus::DecodePending;
        cell.asset_index=0;
        cell.footprint_index=plan.footprints.size();
        cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=maps::stored_image_origin(cell.world,78,40);
        const auto cell_index=plan.cells.size();
        plan.cell_by_storage[cell.cell_index]=cell_index;
        plan.status_by_storage[cell.cell_index]=cell.status;
        plan.cells.push_back(cell);
        maps::PlacedFootprint footprint;
        footprint.id=plan.footprints.size();
        footprint.asset_index=0;
        footprint.origin=cell.storage;
        footprint.cell_indices={cell_index};
        footprint.image_origin=cell.image_origin;
        footprint.status=maps::StoredStatus::DecodePending;
        plan.footprints.push_back(footprint);
    }
    return {std::move(map),std::move(plan)};
}
std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"read software frame");
    std::array<std::uint8_t,4> result{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]);
    SDL_DestroySurface(surface);
    check(okay,"read pixel");
    return result;
}
SDL_Event click(float x,float y) {
    SDL_Event event{};
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;
    event.button.x=x;
    event.button.y=y;
    return event;
}
}
int main() {
    try {
        Temp temp;
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),"dummy driver");
        check(SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software"),"software renderer");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL init");
        SDL_Window* window=nullptr;
        SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("sandbox test",800,600,0,&window,&renderer),"window");
        openemperor::SandboxView view(fixture(temp),false);
        view.initialize(window,renderer);
        const auto screen_for=[&](int x) {
            auto p=maps::terrain_world({static_cast<std::uint32_t>(x),114},72);
            p.y+=20;
            return view.camera().world_to_screen(p);
        };
        const auto workshop=screen_for(110);
        check(view.pick(workshop)==simulation::Cell{110,114},"projected pick");
        view.set_tool(2);
        check(view.preview({110,114}).accepted,"valid preview");
        check(!view.preview({116,114}).accepted,"invalid preview");
        bool running=true;
        view.handle_event(click(static_cast<float>(workshop.x),static_cast<float>(workshop.y)),running);
        check(view.world().workshop()==simulation::Cell{110,114},"click did not place workshop");
        const auto before_zoom=view.camera().screen_to_world(workshop);
        SDL_Event wheel{}; wheel.type=SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.mouse_x=static_cast<float>(workshop.x);
        wheel.wheel.mouse_y=static_cast<float>(workshop.y);
        wheel.wheel.y=1;
        view.handle_event(wheel,running);
        const auto after_zoom=view.camera().screen_to_world(workshop);
        check(std::abs(before_zoom.x-after_zoom.x)<1e-5 &&
              std::abs(before_zoom.y-after_zoom.y)<1e-5,"pointer zoom moved picked world point");
        for (int x=111;x<=114;++x) {
            view.set_tool(1);
            const auto at=screen_for(x);
            check(view.pick(at)==simulation::Cell{x,114},"pick after zoom");
            check(view.preview({x,114}).accepted,"road preview differs from validation");
            view.handle_event(click(static_cast<float>(at.x),static_cast<float>(at.y)),running);
        }
        view.set_tool(3);
        const auto warehouse=screen_for(115);
        view.handle_event(click(static_cast<float>(warehouse.x),static_cast<float>(warehouse.y)),running);
        check(view.world().warehouse()==simulation::Cell{115,114},"warehouse click");
        const auto status_before=view.world().warehouse_stock();
        check(view.render(),"render frame with overlay");
        const auto p=pixel(renderer,static_cast<int>(workshop.x),
            static_cast<int>(workshop.y-11*view.camera().zoom));
        check(p[2]>p[1] && p[1]>p[0],"workshop overlay was not drawn before present");
        for (int i=0;i<120;++i) view.tick_once();
        check(view.world().courier_phase()==simulation::CourierPhase::ToWarehouse,
              "courier did not move in viewer");
        check(view.world().warehouse_stock()==status_before,"premature delivery in viewer");
        check(view.render(),"render moving marker");
        const auto moving=*view.world().courier_position();
        check(moving.x>110 && moving.x<115,"moving marker position");
        auto marker_world=maps::terrain_world({static_cast<std::uint32_t>(moving.x),114},72);
        marker_world.y+=20;
        const auto marker_screen=view.camera().world_to_screen(marker_world);
        const auto marker_pixel=pixel(renderer,static_cast<int>(marker_screen.x),
            static_cast<int>(marker_screen.y));
        check(marker_pixel[0]>235 && marker_pixel[1]>235 && marker_pixel[2]>235,
              "moving courier marker absent from composed frame");
        view.shutdown();
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::cout << "Sandbox software view checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Sandbox software view failed: " << error.what() << '\n'; return 1;
    }
}
