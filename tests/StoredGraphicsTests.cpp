#include "app/MapDebugView.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/StoredGraphicsPlan.h"
#include "renderer/StoredGraphicsRenderer.h"

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
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace maps = openemperor::maps;
namespace assets = openemperor::assets;
namespace scene = openemperor::scene;
using Bytes = std::vector<std::uint8_t>;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Temp {
    fs::path path = fs::temp_directory_path() /
        ("openemperor-stored-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { fs::create_directories(path/"DATA"); }
    ~Temp() { std::error_code ec; fs::remove_all(path,ec); }
};
void u16(Bytes& b,std::size_t at,std::uint16_t value) {
    b.at(at)=static_cast<std::uint8_t>(value); b.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& b,std::size_t at,std::uint32_t value) {
    u16(b,at,static_cast<std::uint16_t>(value)); u16(b,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const fs::path& path,const Bytes& b) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
    check(static_cast<bool>(out),"write synthetic fixture");
}
Bytes sg3(std::uint32_t capacity,std::uint32_t in_use) {
    Bytes b(40680U+capacity*64U,0);
    u32(b,0,static_cast<std::uint32_t>(b.size())); u32(b,4,213);
    u32(b,12,capacity); u32(b,16,in_use); u32(b,20,1);
    const std::string name="Zeus_system.bmp";
    std::copy(name.begin(),name.end(),b.begin()+680);
    u32(b,680+124,200); u32(b,680+128,1); u32(b,680+132,200);
    return b;
}
void record(Bytes& b,std::uint32_t physical,std::uint32_t offset,std::uint32_t length,
            std::uint16_t type,std::uint16_t width,std::uint16_t height,
            std::uint32_t uncompressed,std::uint32_t mirror=0,std::uint8_t size=1) {
    const auto at=40680U+physical*64U;
    u32(b,at,offset); u32(b,at+4,length); u32(b,at+8,uncompressed);
    u32(b,at+16,mirror); u16(b,at+20,width); u16(b,at+22,height);
    u16(b,at+50,type); b[at+55]=size;
}
Bytes tile(std::uint16_t color) {
    Bytes b(3200);
    for (std::size_t i=0;i<b.size();i+=2) u16(b,i,color);
    return b;
}
maps::ParsedEmperorMap map_fixture() {
    maps::ParsedEmperorMap m;
    m.declared_map_size=84;
    m.terrain_raw.logical_offset=maps::terrain_logical_offset;
    m.objects_raw.logical_offset=maps::objects_logical_offset;
    m.terrain_raw.values.assign(228U*228U,0x80);
    m.objects_raw.values.assign(228U*228U,0);
    return m;
}
maps::MapGraphicCandidates candidates_fixture() {
    maps::MapGraphicCandidates c;
    c.candidate_word_layer.assign(228U*228U,0xc000);
    c.candidate_byte_layer.assign(228U*228U,64);
    const auto set=[&](std::uint32_t x,std::uint32_t y,std::uint32_t id) {
        c.candidate_word_layer[static_cast<std::size_t>(y)*228U+x]=id;
    };
    set(114,114,0xc001); // Tall one-cell footprint.
    set(115,114,0xc001); // Same decoded asset, second instance.
    set(113,114,0xc002); // Multi-cell metadata, diagnostic behind tall tile.
    set(116,114,0xc003); // Unverified mirror.
    set(117,114,0xc004); // Empty.
    set(118,114,0xc005); // Payload outside .555.
    set(119,114,0x4c000); // Unknown slot.
    set(120,114,0xc006); // A second supported height with overlay.
    set(121,114,0xc007); // In-bounds payload with malformed overlay.
    return c;
}
std::array<std::uint8_t,4> pixel(SDL_Renderer* r,int x,int y) {
    SDL_Surface* s=SDL_RenderReadPixels(r,nullptr);
    check(s!=nullptr,"read software pixels");
    std::array<std::uint8_t,4> result{};
    const bool okay=SDL_ReadSurfacePixel(s,x,y,&result[0],&result[1],&result[2],&result[3]);
    SDL_DestroySurface(s); check(okay,"read selected software pixel");
    return result;
}
} // namespace

int main() {
    try {
        Temp temp;
        auto terrain=sg3(212,209);
        auto red=tile(0x7c00), green=tile(0x03e0);
        const Bytes overlay{255,38,1,0x1f,0};
        Bytes bitmap=red;
        const auto green_offset=static_cast<std::uint32_t>(bitmap.size());
        bitmap.insert(bitmap.end(),green.begin(),green.end());
        bitmap.insert(bitmap.end(),overlay.begin(),overlay.end());
        const auto malformed_offset=static_cast<std::uint32_t>(bitmap.size());
        bitmap.insert(bitmap.end(),red.begin(),red.end());
        bitmap.push_back(1); // Omega literal without two color bytes.
        bitmap.resize(12800,0); // Valid range for the deliberately unsupported 2x2 record.
        const auto high_offset=static_cast<std::uint32_t>(bitmap.size());
        bitmap.insert(bitmap.end(),green.begin(),green.end());
        bitmap.insert(bitmap.end(),overlay.begin(),overlay.end());
        record(terrain,201,0,3200,30,78,40,3200);
        record(terrain,202,green_offset,3205,30,78,46,3200);
        record(terrain,203,0,12800,30,158,80,12800,0,2);
        record(terrain,204,0,3200,30,78,54,3200,8);
        record(terrain,206,100000,3200,30,78,40,3200);
        record(terrain,207,high_offset,3205,30,78,54,3200);
        record(terrain,208,malformed_offset,3201,30,78,40,3200);
        write(temp.path/"DATA/China_Terrain.sg3",terrain);
        write(temp.path/"DATA/China_Terrain.555",bitmap);
        auto elevation=sg3(205,201);
        write(temp.path/"DATA/China_Elevation.sg3",elevation);
        write(temp.path/"DATA/China_Elevation.555",Bytes{});
        const auto terrain_path=maps::validate_stored_archive_sources(temp.path,"DATA/China_Terrain.sg3");
        maps::validate_stored_archive_sources(temp.path,"DATA/China_Elevation.sg3");
        const auto terrain_layout=maps::build_runtime_archive_layout(3,assets::read_sg3_archive(terrain_path));
        const auto elevation_layout=maps::build_runtime_archive_layout(16,
            assets::read_sg3_archive(temp.path/"DATA/China_Elevation.sg3"));
        check(terrain_layout && elevation_layout && terrain_layout->system_record_skip==200 &&
              terrain_layout->physical_record_for_local(1)==202,"shared 200-plus-dummy layout");
        const auto terrain_catalog=assets::scan_asset_archive(temp.path,"DATA/China_Terrain.sg3");
        const auto elevation_catalog=assets::scan_asset_archive(temp.path,"DATA/China_Elevation.sg3");
        const auto map=map_fixture();
        const auto candidates=candidates_fixture();
        const maps::MapGeometry geometry{map.declared_map_size};
        auto plan=maps::make_stored_graphics_plan(map,candidates,geometry,terrain_catalog,*terrain_layout,
                                                  elevation_catalog,*elevation_layout);
        check(plan.cells.size()==3612 && plan.excluded==48372 &&
              plan.mask_comparison.outside_and_onmap==48372 &&
              plan.cells.size()+plan.excluded==228U*228U,"one entry per candidate, no minimap duplication");
        const auto* tall=plan.at({114,114});
        check(tall && tall->stored_id==0xc001 && tall->local_index==1 &&
              tall->physical_record==202 && tall->logical_offset==
                  1535U+4U*(114U*228U+114U) &&
              tall->status==maps::StoredStatus::DecodePending && tall->record_present &&
              tall->source_ranges_valid && tall->footprint_supported &&
              tall->lookup_status==maps::GraphicsIdStatus::DecodeCandidate,
              "saved ID resolves once to physical 202 with separate metadata gates");
        check(plan.at({115,114})->asset_index==tall->asset_index &&
              plan.at({113,114})->status==maps::StoredStatus::MultiTilePlacementUnverified &&
              plan.at({116,114})->status==maps::StoredStatus::MirrorUnverified &&
              plan.at({117,114})->status==maps::StoredStatus::EmptyRecord &&
              plan.at({118,114})->status==maps::StoredStatus::SourceUnavailable &&
              plan.at({118,114})->record_present && !plan.at({118,114})->source_ranges_valid &&
              plan.at({119,114})->status==maps::StoredStatus::UnregisteredSlot &&
              plan.at({120,114})->status==maps::StoredStatus::DecodePending &&
              plan.at({121,114})->status==maps::StoredStatus::DecodePending,
              "unsupported placements, mirror, empty, missing range and unknown slot stay diagnostic");
        const auto high=assets::load_sg3_image({terrain_path,207});
        const auto flat=assets::load_sg3_image({terrain_path,201});
        check(flat.width==78 && flat.height==40 &&
              flat.pixels[(38U*4U)]==255 && flat.pixels[(38U*4U)+1U]==0,
              "78x40 synthetic base decodes without overlay");
        check(high.width==78 && high.height==54 &&
              high.pixels[(38U*4U)+2U]==255 &&
              high.pixels[(14U*78U+38U)*4U+1U]==255,
              "78x54 synthetic Omega overlay and base decode independently");
        check(!plan.at({0,0}) && plan.status_by_storage[0]==maps::StoredStatus::Excluded,
              "outside storage retained as excluded");
        check(maps::stored_image_origin({100,100},78,40).x==61 &&
              maps::stored_image_origin({100,100},78,40).y==100 &&
              maps::stored_image_origin({100,100},78,46).y==94 &&
              maps::stored_image_origin({100,100},78,48).y==92 &&
              maps::stored_image_origin({100,100},78,54).y==86,
              "independent fixed height anchors");
        check(tall->world.y==plan.at({115,114})->world.y-20 &&
              tall->image_origin.y==tall->world.y-6,
              "neighbor ground position stays independent of image height");
        scene::Camera2D clip; clip.viewport_width=100; clip.viewport_height=100;
        check(maps::stored_rect_visible({0,-50},78,54,clip),
              "upper image remains visible with offscreen ground anchor");
        const auto tall_world=tall->world;
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy init");
        SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("stored test",400,300,0,&window,&renderer),"software renderer");
        {
            openemperor::StoredGraphicsRenderer preview{std::move(plan)};
            preview.initialize(renderer);
            check(preview.upload_count()==3 && preview.plan().logical_texture_bytes==
                  78U*40U*4U+78U*46U*4U+78U*54U*4U &&
                  preview.plan().at({114,114})->status==maps::StoredStatus::Rendered &&
                  preview.plan().at({115,114})->status==maps::StoredStatus::Rendered &&
                  preview.plan().at({121,114})->status==maps::StoredStatus::DecodeFailed &&
                  preview.plan().assets[*preview.plan().at({121,114})->asset_index].decode_attempted,
                  "three distinct assets upload once and share tall texture");
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            camera.center_on(tall_world);
            check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
                  preview.render(camera,std::nullopt) && SDL_RenderPresent(renderer),"render saved image plan");
            check(pixel(renderer,199,144)==std::array<std::uint8_t,4>{0,0,255,255},
                  "synthetic Omega overlay retains top pixel at variable-height anchor");
            check(pixel(renderer,179,160)==std::array<std::uint8_t,4>{0,255,0,255},
                  "earlier diagnostic diamond does not cover later original image");
            const auto count=preview.last_drawn_instances();
            camera.offset.x+=15;
            check(preview.render(camera,std::nullopt) && preview.upload_count()==3 &&
                  preview.last_drawn_instances()>0 && count>0,"camera move never uploads textures");
            preview.shutdown();
        }
        {
            maps::StoredGraphicsPlan over_budget;
            assets::AssetRecord huge;
            huge.width=30000; huge.height=30000;
            huge.data_length=3200;
            over_budget.assets.push_back({huge,maps::StoredStatus::DecodePending,false,false,{}});
            openemperor::StoredGraphicsRenderer guarded{std::move(over_budget)};
            bool rejected=false;
            try { guarded.initialize(renderer); }
            catch (const std::runtime_error&) { rejected=true; }
            check(rejected && guarded.upload_count()==0,"oversized image rejected before decode/allocation");
        }
        {
            auto fresh=maps::make_stored_graphics_plan(map,candidates,geometry,terrain_catalog,*terrain_layout,
                                                       elevation_catalog,*elevation_layout);
            openemperor::MapDebugView view{map,maps::RawLayer::Terrain,
                maps::MapViewMode::StoredGraphics,std::nullopt,std::move(fresh)};
            view.initialize(window,renderer);
            check(view.render() && view.view()==maps::MapViewMode::StoredGraphics,
                  "stored view renders without terrain bindings");
            bool running=true;
            SDL_Event select{}; select.type=SDL_EVENT_KEY_DOWN; select.key.key=SDLK_RETURN;
            view.handle_event(select,running);
            const auto chosen=view.selected_cell();
            SDL_Event wheel{}; wheel.type=SDL_EVENT_MOUSE_WHEEL;
            wheel.wheel.mouse_x=200; wheel.wheel.mouse_y=150; wheel.wheel.y=2;
            view.handle_event(wheel,running);
            view.handle_event(select,running);
            check(view.selected_cell()==chosen && view.render(),"selection remains same after zoom");
            check(SDL_SetWindowSize(window,500,350) && view.render(),"stored view survives resize");
            view.handle_event(select,running);
            check(view.selected_cell()==chosen,"stored logical selection survives resize");
            view.shutdown();
        }
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        {
            Temp outside;
            write(outside.path/"DATA/foreign.555",Bytes{1,2});
            write(temp.path/"DATA/escape.sg3",sg3(205,201));
            fs::create_symlink(outside.path/"DATA/foreign.555",temp.path/"DATA/escape.555");
            bool rejected=false;
            try { maps::validate_stored_archive_sources(temp.path,"DATA/escape.sg3"); }
            catch (const std::runtime_error&) { rejected=true; }
            check(rejected,"bitmap symlink escaping data root rejected before catalog scan");
        }
        std::cout << "stored graphics synthetic checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "stored graphics test: " << error.what() << '\n'; return 1;
    }
}
