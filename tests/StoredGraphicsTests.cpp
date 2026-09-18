#include "app/MapDebugView.h"
#include "app/MapBrowser.h"
#include "app/MapRenderCheck.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/StoredGraphicsPlan.h"
#include "renderer/StoredGraphicsRenderer.h"

#include <SDL3/SDL.h>
#include <zlib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <tuple>
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
Bytes stored_map_file(std::uint32_t marker,bool partial=false,std::uint32_t declared_size=84,
                      bool slot8=false,bool wall4=false) {
    Bytes raw(static_cast<std::size_t>(maps::objects_logical_offset+maps::grid_byte_length),0);
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());
    u32(raw,84,declared_size);
    for (std::size_t i=0;i<228U*228U;++i) {
        u32(raw,static_cast<std::size_t>(maps::candidate_word_logical_offset)+4*i,0xc000);
        raw[static_cast<std::size_t>(maps::candidate_byte_logical_offset)+i]=64;
        u32(raw,static_cast<std::size_t>(maps::terrain_logical_offset)+4*i,marker);
    }
    if (partial) u32(raw,static_cast<std::size_t>(maps::candidate_word_logical_offset)+
                         4U*(114U*228U+114U),0xc002);
    if (slot8) u32(raw,static_cast<std::size_t>(maps::candidate_word_logical_offset)+
                       4U*(114U*228U+114U),0x20000);
    if (wall4) for (std::uint32_t dy=0;dy<4;++dy) for (std::uint32_t dx=0;dx<4;++dx) {
        const auto index=static_cast<std::size_t>(100U+dy)*228U+100U+dx;
        u32(raw,static_cast<std::size_t>(maps::candidate_word_logical_offset)+4U*index,0x20001);
        raw[static_cast<std::size_t>(maps::candidate_byte_logical_offset)+index]=
            static_cast<std::uint8_t>((dy<<3U)|dx|((dx==0 && dy==3) ? 0x40U : 0U));
    }
    Bytes file{0xaa,0xba,0xdc,0xfe};
    for (std::size_t at=0;at<raw.size();at+=32768) {
        const auto length=std::min<std::size_t>(32768,raw.size()-at);
        uLongf capacity=compressBound(static_cast<uLong>(length));
        Bytes zipped(static_cast<std::size_t>(capacity));
        check(compress2(zipped.data(),&capacity,raw.data()+at,static_cast<uLong>(length),6)==Z_OK,
              "synthetic map zlib compression");
        zipped.resize(static_cast<std::size_t>(capacity));
        const auto start=file.size(); file.resize(start+12);
        u32(file,start,0x12345678); u32(file,start+4,static_cast<std::uint32_t>(zipped.size()));
        u32(file,start+8,static_cast<std::uint32_t>(length));
        file.insert(file.end(),zipped.begin(),zipped.end());
    }
    return file;
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
void set_id(maps::MapGraphicCandidates& candidates,std::uint32_t x,std::uint32_t y,
            std::uint32_t id,std::uint8_t marker=64) {
    const auto at=static_cast<std::size_t>(y)*228U+x;
    candidates.candidate_word_layer.at(at)=id;
    candidates.candidate_byte_layer.at(at)=marker;
}
maps::MapGeometry sparse_geometry(std::initializer_list<maps::GridCell> cells) {
    maps::MapGeometry geometry{84};
    std::fill(geometry.candidate.begin(),geometry.candidate.end(),0);
    for (const auto cell:cells)
        geometry.candidate.at(static_cast<std::size_t>(cell.y)*228U+cell.x)=1;
    return geometry;
}
maps::MapGeometry sparse_geometry(const std::vector<maps::GridCell>& cells) {
    maps::MapGeometry geometry{84};
    std::fill(geometry.candidate.begin(),geometry.candidate.end(),0);
    for (const auto cell:cells)
        geometry.candidate.at(static_cast<std::size_t>(cell.y)*228U+cell.x)=1;
    return geometry;
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
        auto terrain=sg3(212,211);
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
        const auto multi_offset=static_cast<std::uint32_t>(bitmap.size());
        for (const auto color : {std::uint16_t{0x7c00},std::uint16_t{0x03e0},
                                 std::uint16_t{0x001f},std::uint16_t{0x7fe0}}) {
            const auto part=tile(color);
            bitmap.insert(bitmap.end(),part.begin(),part.end());
        }
        const auto front_offset=static_cast<std::uint32_t>(bitmap.size());
        bitmap.insert(bitmap.end(),green.begin(),green.end());
        const Bytes front_overlay{255,38,1,0x00,0x7c}; // One red pixel above a green tile.
        bitmap.insert(bitmap.end(),front_overlay.begin(),front_overlay.end());
        record(terrain,201,0,3200,30,78,40,3200);
        record(terrain,202,green_offset,3205,30,78,46,3200);
        record(terrain,203,multi_offset,12800,30,158,80,12800,0,2);
        record(terrain,204,0,3200,30,78,54,3200,8);
        record(terrain,206,100000,3200,30,78,40,3200);
        record(terrain,207,high_offset,3205,30,78,54,3200);
        record(terrain,208,malformed_offset,3201,30,78,40,3200);
        record(terrain,209,multi_offset,12800,30,158,125,12800,0,2);
        record(terrain,210,multi_offset,12800,30,160,80,12800,0,2);
        record(terrain,211,front_offset,3205,30,78,54,3200);
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
        auto extra_candidates=candidates_fixture();
        set_id(extra_candidates,110,110,0x20000);
        set_id(extra_candidates,111,110,0x20001);
        set_id(extra_candidates,112,110,0xc000);
        const auto extra_geometry=sparse_geometry({{110,110},{111,110},{112,110}});
        const auto base_registrations=maps::load_stored_archive_registrations(
            temp.path,extra_candidates,extra_geometry,maps::StoredGraphicsProfile::Base);
        check(!base_registrations.contains(8U),"base profile retains its two registrations");
        const auto missing_registrations=maps::load_stored_archive_registrations(
            temp.path,extra_candidates,extra_geometry,maps::StoredGraphicsProfile::Slot8);
        check(missing_registrations.at(8U).archive_missing &&
              !missing_registrations.at(8U).catalog,"known optional archive is missing");
        const auto missing_plan=maps::make_stored_graphics_plan(
            map,extra_candidates,extra_geometry,missing_registrations,
            maps::FootprintPolicy::EdgeBytePreview,maps::StoredGraphicsProfile::Slot8);
        check(missing_plan.at({110,110})->status==maps::StoredStatus::ArchiveMissing &&
              missing_plan.at({112,110})->physical_record==201,
              "missing optional archive does not block required terrain");
        const auto unused_registrations=maps::load_stored_archive_registrations(
            temp.path,candidates,geometry,maps::StoredGraphicsProfile::Slot8);
        check(!unused_registrations.contains(8U),"unused optional archive is not loaded");
        auto slot8_sg3=sg3(205,202);
        record(slot8_sg3,201,0,3200,30,78,40,3200);
        record(slot8_sg3,202,0,3200,30,318,160,51200,0,4);
        write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",slot8_sg3);
        write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.555",red);
        const auto extra_registrations=maps::load_stored_archive_registrations(
            temp.path,extra_candidates,extra_geometry,maps::StoredGraphicsProfile::Slot8);
        check(extra_registrations.at(8U).layout &&
              extra_registrations.at(8U).layout->first_physical_record==201 &&
              extra_registrations.at(8U).catalog,
              "extra archive owns metadata, catalog and verified layout");
        auto extra_plan=maps::make_stored_graphics_plan(
            map,extra_candidates,extra_geometry,extra_registrations,
            maps::FootprintPolicy::EdgeBytePreview,maps::StoredGraphicsProfile::Slot8);
        check(extra_plan.profile==maps::StoredGraphicsProfile::Slot8 &&
              extra_plan.at({110,110})->physical_record==201 &&
              extra_plan.at({110,110})->status==maps::StoredStatus::DecodePending &&
              extra_plan.at({111,110})->status==maps::StoredStatus::UnsupportedFootprintSize &&
              extra_plan.at({112,110})->physical_record==201 &&
              extra_plan.at({110,110})->asset_index!=extra_plan.at({112,110})->asset_index,
              "slot8 and terrain keep separate physical contexts and unchanged footprint rules");
        const auto extra_image=assets::load_sg3_image(
            {temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",201});
        check(extra_image.width==78 && extra_image.height==40 &&
              extra_image.pixels[38U*4U]==255,
              "resolved synthetic slot8 image decodes through the normal loader");
        auto unsupported_sg3=slot8_sg3;
        const std::string other_name="Other_system.bmp";
        std::fill(unsupported_sg3.begin()+680,unsupported_sg3.begin()+744,0);
        std::copy(other_name.begin(),other_name.end(),unsupported_sg3.begin()+680);
        write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",unsupported_sg3);
        const auto unsupported_registrations=maps::load_stored_archive_registrations(
            temp.path,extra_candidates,extra_geometry,maps::StoredGraphicsProfile::Slot8);
        check(!unsupported_registrations.at(8U).layout &&
              maps::make_stored_graphics_plan(map,extra_candidates,extra_geometry,
                  unsupported_registrations,maps::FootprintPolicy::EdgeBytePreview,
                  maps::StoredGraphicsProfile::Slot8).at({110,110})->status==
                  maps::StoredStatus::UnverifiedRegistration,
              "present archive with unsupported layout remains unverified");
        write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",slot8_sg3);
        {
            Temp outside;
            write(outside.path/"DATA/escape.sg3",slot8_sg3);
            fs::remove(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3");
            fs::create_symlink(outside.path/"DATA/escape.sg3",
                               temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3");
            bool rejected=false;
            try { (void)maps::load_stored_archive_registrations(
                    temp.path,extra_candidates,extra_geometry,maps::StoredGraphicsProfile::Slot8); }
            catch (const std::exception&) { rejected=true; }
            check(rejected,"optional archive symlink cannot escape data root");
            fs::remove(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3");
            write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",slot8_sg3);
            write(outside.path/"DATA/escape.555",red);
            fs::remove(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.555");
            fs::create_symlink(outside.path/"DATA/escape.555",
                               temp.path/"DATA/China_Mon_Earthen_Greatwall_1.555");
            rejected=false;
            try { (void)maps::load_stored_archive_registrations(
                    temp.path,extra_candidates,extra_geometry,maps::StoredGraphicsProfile::Slot8); }
            catch (const std::exception&) { rejected=true; }
            check(rejected,"optional bitmap symlink cannot escape data root");
            fs::remove(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.555");
            write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.555",red);
        }
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
        check(maps::stored_two_by_two_image_origin({100,100},158,80).x==21 &&
              maps::stored_two_by_two_image_origin({100,100},158,80).y==100 &&
              maps::stored_two_by_two_image_origin({100,100},158,125).x==21 &&
              maps::stored_two_by_two_image_origin({100,100},158,125).y==55,
              "fixed two-by-two anchors preserve the full upper image");
        check(maps::stored_square_image_origin({100,100},318,160,4).x==-59 &&
              maps::stored_square_image_origin({100,100},318,160,4).y==100 &&
              maps::stored_square_image_origin({100,100},318,167,4).x==-59 &&
              maps::stored_square_image_origin({100,100},318,167,4).y==93 &&
              maps::stored_square_image_origin({100,100},318,200,4).x==-59 &&
              maps::stored_square_image_origin({100,100},318,200,4).y==60,
              "fixed 4x4 anchors preserve full tall images");
        scene::Camera2D tall_clip; tall_clip.viewport_width=100; tall_clip.viewport_height=100;
        check(maps::stored_rect_visible({0,80},318,200,tall_clip),
              "visible upper 4x4 image survives culling with offscreen footprint point");
        const auto multi_rgba=assets::load_sg3_image({terrain_path,203});
        const auto multi_pixel=[&](std::size_t x,std::size_t y) {
            const auto at=(y*multi_rgba.width+x)*4U;
            return std::array<std::uint8_t,4>{multi_rgba.pixels.at(at),multi_rgba.pixels.at(at+1),
                multi_rgba.pixels.at(at+2),multi_rgba.pixels.at(at+3)};
        };
        check(multi_pixel(79,0)==std::array<std::uint8_t,4>{255,0,0,255} &&
              multi_pixel(39,20)==std::array<std::uint8_t,4>{0,255,0,255} &&
              multi_pixel(119,20)==std::array<std::uint8_t,4>{0,0,255,255} &&
              multi_pixel(79,40)==std::array<std::uint8_t,4>{255,255,0,255},
              "asymmetric independent four-tile payload preserves orientation");
        auto multi_candidates=candidates_fixture();
        const auto set_square=[&](std::uint32_t x,std::uint32_t y,std::uint32_t id) {
            set_id(multi_candidates,x,y,id,0);
            set_id(multi_candidates,x+1,y,id,1);
            set_id(multi_candidates,x,y+1,id,72);
            set_id(multi_candidates,x+1,y+1,id,9);
        };
        set_square(100,100,0xc002);
        set_square(104,100,0xc002);
        set_square(108,100,0xc008); // Physical 209, 158x125.
        set_id(multi_candidates,99,99,0xc000);
        set_id(multi_candidates,102,102,0xc000);
        const auto sparse=sparse_geometry({{100,100},{101,100},{100,101},{101,101},
            {104,100},{105,100},{104,101},{105,101},
            {108,100},{109,100},{108,101},{109,101},{99,99},{102,102}});
        const auto default_plan=maps::make_stored_graphics_plan(map,multi_candidates,
            sparse,terrain_catalog,*terrain_layout,elevation_catalog,*elevation_layout);
        check(default_plan.footprint_count(2)==0 &&
              default_plan.status_counts().at("multi_tile_placement_unverified")==12,
              "default snapshot keeps complete multi-cell images diagnostic without opt-in");
        auto two_plan=maps::make_stored_graphics_plan(map,multi_candidates,sparse,terrain_catalog,
            *terrain_layout,elevation_catalog,*elevation_layout,true);
        check(two_plan.cells.size()==14 && two_plan.footprint_count(2)==3 &&
              two_plan.footprint_count(1)==2 && two_plan.covered_cells()==14 &&
              two_plan.at({100,100})->footprint_index==two_plan.at({101,101})->footprint_index &&
              two_plan.at({100,100})->footprint_index!=two_plan.at({104,100})->footprint_index &&
              two_plan.at({100,100})->asset_index==two_plan.at({104,100})->asset_index,
              "separate exact squares create two instances sharing one physical texture");
        for (const auto where : {maps::GridCell{100,100},maps::GridCell{101,100},
                                 maps::GridCell{100,101},maps::GridCell{101,101}}) {
            const auto* cell=two_plan.at(where);
            check(cell && cell->stored_id==0xc002 && cell->footprint_index &&
                  cell->terrain_raw==0x80 && cell->logical_offset==
                  1535U+4U*(static_cast<std::size_t>(where.y)*228U+where.x),
                  "each owned storage cell retains its own saved values and offset");
        }
        set_square(112,100,0xc002);
        auto ambiguous_candidates=multi_candidates;
        set_id(ambiguous_candidates,114,100,0xc002);
        set_id(ambiguous_candidates,114,101,0xc002);
        const auto ambiguous_geometry=sparse_geometry({{112,100},{113,100},{114,100},
            {112,101},{113,101},{114,101}});
        const auto ambiguous=maps::make_stored_graphics_plan(map,ambiguous_candidates,
            ambiguous_geometry,terrain_catalog,*terrain_layout,elevation_catalog,*elevation_layout,true);
        check(ambiguous.footprint_count(2)==0 &&
              ambiguous.status_counts().at("ambiguous_footprint")==6,
              "touching same-image footprints are not partitioned arbitrarily");
        auto conflict_candidates=multi_candidates;
        set_id(conflict_candidates,101,101,0xc000);
        const auto conflict=maps::make_stored_graphics_plan(map,conflict_candidates,
            sparse_geometry({{100,100},{101,100},{100,101},{101,101}}),terrain_catalog,
            *terrain_layout,elevation_catalog,*elevation_layout,true);
        check(conflict.footprint_count(2)==0 &&
              conflict.status_counts().at("incomplete_footprint")==3 &&
              conflict.at({101,101})->stored_id==0xc000 && conflict.at({101,101})->footprint_index,
              "conflicting reference is never taken into a neighboring footprint");
        const auto boundary=maps::make_stored_graphics_plan(map,multi_candidates,
            sparse_geometry({{100,100},{101,100},{100,101}}),terrain_catalog,*terrain_layout,
            elevation_catalog,*elevation_layout,true);
        check(boundary.footprint_count(2)==0 && !boundary.at({101,101}) &&
              boundary.status_counts().at("incomplete_footprint")==3,
              "candidate mask boundary cannot complete a footprint by clipping");
        const auto metadata_zero=maps::decode_map_subtile_byte(0);
        const auto metadata_one=maps::decode_map_subtile_byte(1);
        const auto metadata_72=maps::decode_map_subtile_byte(72);
        const auto metadata_nine=maps::decode_map_subtile_byte(9);
        check(metadata_zero.part_x==0 && metadata_zero.part_y==0 && !metadata_zero.draw_marker_candidate &&
              metadata_one.part_x==1 && metadata_one.part_y==0 &&
              metadata_72.part_x==0 && metadata_72.part_y==1 && metadata_72.draw_marker_candidate &&
              metadata_nine.part_x==1 && metadata_nine.part_y==1 &&
              maps::decode_map_subtile_byte(0x08).part_y==maps::decode_map_subtile_byte(0x48).part_y &&
              !maps::decode_map_subtile_byte(0x08).draw_marker_candidate &&
              maps::decode_map_subtile_byte(0x48).draw_marker_candidate &&
              maps::decode_map_subtile_byte(0x80).unknown_bits==0x80 &&
              !maps::map_subtile_origin({0,0},metadata_one),
              "reference-derived bits preserve position, marker and unknown top bit independently");
        const auto edge_plan=[&](const maps::MapGraphicCandidates& c,const std::vector<maps::GridCell>& cells) {
            return maps::make_stored_graphics_plan(map,c,sparse_geometry(cells),terrain_catalog,
                *terrain_layout,elevation_catalog,*elevation_layout,maps::FootprintPolicy::EdgeBytePreview);
        };
        const auto squares=[&](std::initializer_list<maps::GridCell> origins) {
            auto c=candidates_fixture();
            std::vector<maps::GridCell> cells;
            for (const auto origin:origins) {
                const std::array<std::uint8_t,4> bytes{0,1,72,9};
                std::size_t part=0;
                for (std::uint32_t dy=0;dy<2;++dy) for (std::uint32_t dx=0;dx<2;++dx) {
                    set_id(c,origin.x+dx,origin.y+dy,0xc002,bytes[part++]);
                    cells.push_back({origin.x+dx,origin.y+dy});
                }
            }
            return std::pair{c,cells};
        };
        for (const auto& origins : {std::vector<maps::GridCell>{{100,100}},
                                   std::vector<maps::GridCell>{{100,100},{100,102}},
                                   std::vector<maps::GridCell>{{100,100},{102,100}},
                                   std::vector<maps::GridCell>{{100,100},{102,100},{100,102},{102,102}}}) {
            auto c=candidates_fixture();
            std::vector<maps::GridCell> cells;
            for (const auto origin:origins) {
                const std::array<std::uint8_t,4> bytes{0,1,72,9};
                std::size_t part=0;
                for (std::uint32_t dy=0;dy<2;++dy) for (std::uint32_t dx=0;dx<2;++dx) {
                    set_id(c,origin.x+dx,origin.y+dy,0xc002,bytes[part++]);
                    cells.push_back({origin.x+dx,origin.y+dy});
                }
            }
            const auto grouped=edge_plan(c,cells);
            check(grouped.footprint_count(2)==origins.size() &&
                  grouped.covered_cells()==origins.size()*4 && grouped.marker_deviations==0 &&
                  grouped.status_counts().at("decode_pending")==cells.size(),
                  "adjacent edge-byte squares are independent complete instances");
            for (const auto& footprint:grouped.footprints) {
                check(footprint.draw_cell_candidate &&
                      footprint.draw_cell_candidate->x==footprint.origin.x &&
                      footprint.draw_cell_candidate->y==footprint.origin.y+1 &&
                      footprint.image_origin.x==maps::stored_two_by_two_image_origin(
                          maps::terrain_world(footprint.origin,grouped.border),158,80).x,
                      "draw candidate differs from projected image origin");
            }
        }
        auto [vertical_candidates,vertical_cells]=squares({{100,100},{100,102}});
        const auto old_vertical=maps::make_stored_graphics_plan(map,vertical_candidates,
            sparse_geometry(vertical_cells),terrain_catalog,*terrain_layout,elevation_catalog,*elevation_layout,true);
        check(old_vertical.footprint_count(2)==0 &&
              old_vertical.status_counts().at("ambiguous_footprint")==8,
              "isolated policy remains separately ambiguous for touching squares");
        const auto edge_vertical=edge_plan(vertical_candidates,vertical_cells);
        auto reversed_cells=vertical_cells;
        std::reverse(reversed_cells.begin(),reversed_cells.end());
        const auto reversed=edge_plan(vertical_candidates,reversed_cells);
        check(edge_vertical.at({100,100})->footprint_index!=edge_vertical.at({100,102})->footprint_index &&
              edge_vertical.at({100,100})->asset_index==edge_vertical.at({100,102})->asset_index &&
              edge_vertical.at({100,101})->candidate_byte==72 &&
              reversed.at({100,100})->footprint_index==edge_vertical.at({100,100})->footprint_index &&
              reversed.at({100,102})->footprint_index==edge_vertical.at({100,102})->footprint_index,
              "touching instances retain their own raw bytes and share asset identity");
        auto zero_candidates=vertical_candidates;
        for (const auto cell:vertical_cells)
            set_id(zero_candidates,cell.x,cell.y,0xc002,0);
        const auto zero=edge_plan(zero_candidates,vertical_cells);
        check(zero.footprint_count(2)==0 && zero.covered_cells()==0,
              "four zero bytes do not acquire geometry by spatial fallback");
        auto wrong_candidates=vertical_candidates;
        set_id(wrong_candidates,101,101,0xc002,0x1a);
        const auto wrong=edge_plan(wrong_candidates,vertical_cells);
        check(wrong.footprint_count(2)==1 &&
              wrong.at({101,101})->status==maps::StoredStatus::SubtilePositionInvalid &&
              wrong.at({100,100})->status==maps::StoredStatus::ConflictingFootprint,
              "invalid part position diagnoses its group without isolated fallback");
        auto different_candidates=vertical_candidates;
        set_id(different_candidates,101,101,0xc008,9);
        const auto different=edge_plan(different_candidates,vertical_cells);
        check(different.footprint_count(2)==1 &&
              different.at({100,100})->status==maps::StoredStatus::ConflictingFootprint,
              "different saved ID and physical asset cannot complete first group");
        auto missing_cells=vertical_cells;
        missing_cells.erase(std::remove_if(missing_cells.begin(),missing_cells.end(),
            [](maps::GridCell p){ return p.x==101 && p.y==101; }),missing_cells.end());
        const auto missing=edge_plan(vertical_candidates,missing_cells);
        check(missing.footprint_count(2)==1 &&
              missing.at({100,100})->status==maps::StoredStatus::AnchorUnresolved,
              "preview mask boundary rejects incomplete footprint");
        auto marker_candidates=vertical_candidates;
        set_id(marker_candidates,100,101,0xc002,8);
        set_id(marker_candidates,101,103,0xc002,73);
        const auto marker=edge_plan(marker_candidates,vertical_cells);
        check(marker.footprint_count(2)==2 && marker.marker_deviations==2,
              "missing and duplicate marker candidates are diagnostic, not hidden ownership rules");
        auto unknown_candidates=vertical_candidates;
        set_id(unknown_candidates,100,100,0xc002,0x80);
        const auto unknown=edge_plan(unknown_candidates,vertical_cells);
        check(unknown.footprint_count(2)==2 && unknown.unknown_bit_cells==1 &&
              unknown.at({100,100})->candidate_byte==0x80,
              "unknown top bit is retained without altering grouping");
        auto edge_candidates=multi_candidates;
        set_id(edge_candidates,102,100,0xc002); // Same saved ID immediately outside the mask.
        const auto edge=maps::make_stored_graphics_plan(map,edge_candidates,
            sparse_geometry({{100,100},{101,100},{100,101},{101,101}}),terrain_catalog,
            *terrain_layout,elevation_catalog,*elevation_layout,true);
        check(edge.footprint_count(2)==0 &&
              edge.status_counts().at("anchor_unresolved")==4,
              "a fifth matching storage cell outside the candidate mask is not silently trimmed");
        set_id(multi_candidates,110,100,0xc009);
        const auto unsupported=maps::make_stored_graphics_plan(map,multi_candidates,
            sparse_geometry({{110,100}}),terrain_catalog,*terrain_layout,elevation_catalog,
            *elevation_layout,true);
        check(unsupported.at({110,100})->status==maps::StoredStatus::UnsupportedFootprintSize,
              "inconsistent wider metadata stays a separate diagnostic");
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
        fs::create_directories(temp.path/"Cities");
        write(temp.path/"Cities/A.map",stored_map_file(0x80));
        write(temp.path/"Cities/B.MAP",stored_map_file(0x82));
        write(temp.path/"Cities/Broken.map",Bytes{'b','a','d'});
        {
            auto catalog=maps::discover_standalone_maps(temp.path);
            check(catalog.entries.size()==3 && catalog.entries[0].map_profile &&
                  catalog.entries[1].map_profile && !catalog.entries[2].map_profile,
                  "browser catalog validates synthetic maps independently");
            openemperor::MapBrowser browser{std::move(catalog),maps::FootprintPolicy::EdgeBytePreview};
            browser.initialize(window,renderer);
            bool running=true;
            SDL_Event down{}; down.type=SDL_EVENT_KEY_DOWN; down.key.key=SDLK_DOWN;
            SDL_Event up{}; up.type=SDL_EVENT_KEY_DOWN; up.key.key=SDLK_UP;
            SDL_Event escape{}; escape.type=SDL_EVENT_KEY_DOWN; escape.key.key=SDLK_ESCAPE;
            check(browser.open_selected() && browser.map_open() && browser.render() &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==1,
                  "first synthetic map opens and uploads one shared texture");
            browser.handle_event(escape,running);
            check(running && !browser.map_open() &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==0,
                  "Escape returns to list and releases all owned map textures");
            browser.handle_event(down,running);
            check(browser.selected_index()==1 && browser.open_selected() && browser.render() &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==1,
                  "second synthetic map opens in same renderer");
            browser.handle_event(escape,running);
            browser.handle_event(down,running);
            check(browser.selected_index()==2 && !browser.open_selected() &&
                  !browser.map_open() && browser.statuses()[2]=="load_failed" &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==0,
                  "bad map reports error without ending browser session");
            browser.handle_event(up,running); browser.handle_event(up,running);
            check(browser.selected_index()==0 && browser.open_selected() && browser.render() &&
                  browser.statuses()[0]=="snapshot_complete" &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==1,
                  "first map reopens after second map and failed file");
            browser.handle_event(escape,running);
            check(!browser.map_open() && openemperor::StoredGraphicsRenderer::live_texture_count()==0,
                  "A-B-error-A sequence leaves no owned map textures");
            SDL_Event page_down{}; page_down.type=SDL_EVENT_KEY_DOWN; page_down.key.key=SDLK_PAGEDOWN;
            SDL_Event page_up{}; page_up.type=SDL_EVENT_KEY_DOWN; page_up.key.key=SDLK_PAGEUP;
            browser.handle_event(page_down,running);
            check(browser.selected_index()==2,"Page Down moves to end of short list");
            browser.handle_event(page_up,running);
            check(browser.selected_index()==0,"Page Up returns to first page");
            SDL_Event double_click{}; double_click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
            double_click.button.button=SDL_BUTTON_LEFT; double_click.button.clicks=2;
            double_click.button.x=20; double_click.button.y=128;
            browser.handle_event(double_click,running);
            check(browser.selected_index()==1 && browser.map_open() && browser.render(),
                  "double click opens selected row");
            browser.handle_event(escape,running);
            check(openemperor::StoredGraphicsRenderer::live_texture_count()==0,
                  "double-clicked map also releases textures");
            browser.handle_event(escape,running);
            check(!running,"Escape in list quits application");
            browser.shutdown();
        }
        write(temp.path/"Cities/Fail.map",stored_map_file(0x80,false,85));
        {
            openemperor::MapBrowser browser{maps::discover_standalone_maps(temp.path),
                                            maps::FootprintPolicy::EdgeBytePreview};
            browser.initialize(window,renderer);
            bool running=true;
            SDL_Event down{}; down.type=SDL_EVENT_KEY_DOWN; down.key.key=SDLK_DOWN;
            for (int i=0;i<3;++i) browser.handle_event(down,running);
            check(browser.selected_index()==3 && !browser.open_selected() &&
                  browser.statuses()[3]=="load_failed" && !browser.map_open(),
                  "valid-format unsupported geometry fails session loading without exiting browser");
            SDL_Event up{}; up.type=SDL_EVENT_KEY_DOWN; up.key.key=SDLK_UP;
            for (int i=0;i<3;++i) browser.handle_event(up,running);
            check(browser.open_selected() && browser.render() &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==1,
                  "a good map opens after actual session-load failure");
            browser.shutdown();
            check(openemperor::StoredGraphicsRenderer::live_texture_count()==0,
                  "failed-session recovery releases good map resources");
        }
        write(temp.path/"Cities/Slot8.map",stored_map_file(0x80,false,84,true));
        {
            openemperor::MapBrowser browser{maps::discover_standalone_maps(temp.path),
                maps::FootprintPolicy::EdgeBytePreview,maps::StoredGraphicsProfile::Slot8};
            browser.initialize(window,renderer);
            bool running=true;
            SDL_Event down{}; down.type=SDL_EVENT_KEY_DOWN; down.key.key=SDLK_DOWN;
            for (int i=0;i<4;++i) browser.handle_event(down,running);
            check(browser.open_selected() && browser.render() &&
                  browser.statuses()[4]=="snapshot_complete" &&
                  openemperor::StoredGraphicsRenderer::live_texture_count()==2,
                  "extended browser resolves and decodes synthetic slot8 image");
            browser.shutdown();
            check(openemperor::StoredGraphicsRenderer::live_texture_count()==0,
                  "extended browser releases both slot textures");
        }
        write(temp.path/"Cities/C.map",stored_map_file(0x80,true));
        {
            auto adjacent=edge_plan(vertical_candidates,vertical_cells);
            openemperor::StoredGraphicsRenderer preview{std::move(adjacent)};
            preview.initialize(renderer);
            check(preview.upload_count()==1 && preview.plan().footprint_count(2)==2 &&
                  preview.plan().covered_cells()==8 &&
                  preview.plan().status_counts().at("rendered")==8,
                  "two touching metadata instances share one decoded SDL texture");
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            camera.center_on(maps::terrain_world({100,101},sparse.border));
            check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
                  preview.render(camera,std::nullopt) && preview.last_texture_draws()==2 &&
                  preview.last_diagnostic_draws()==0,
                  "touching instances draw twice without diagnostic overpaint");
            camera.zoom_at({200,150},1.4);
            check(preview.render(camera,std::nullopt) && preview.upload_count()==1,
                  "zoom does not re-upload shared texture");
            const auto adjacent_geometry=sparse_geometry(vertical_cells);
            for (const auto where:vertical_cells) {
                const auto ground=maps::terrain_world(where,adjacent_geometry.border);
                const scene::Point inside{ground.x,ground.y+20};
                check(maps::pick_terrain_cell(camera.screen_to_world(camera.world_to_screen(inside)),
                      adjacent_geometry)==where,
                      "each edge-byte part remains selectable after zoom");
            }
            preview.shutdown();
        }
        {
            auto isolated=maps::make_stored_graphics_plan(map,multi_candidates,
                sparse_geometry({{100,100},{101,100},{100,101},{101,101}}),terrain_catalog,
                *terrain_layout,elevation_catalog,*elevation_layout,true);
            openemperor::StoredGraphicsRenderer preview{std::move(isolated)};
            preview.initialize(renderer);
            check(preview.upload_count()==1 && preview.plan().covered_cells()==4 &&
                  preview.plan().footprint_count(2)==1 &&
                  preview.plan().status_counts().at("rendered")==4,
                  "one decoded texture covers four retained storage cells");
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            camera.center_on(maps::terrain_world({100,100},sparse.border));
            check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
                  preview.render(camera,std::nullopt) &&
                  preview.last_texture_draws()==1 && preview.last_diagnostic_draws()==0,
                  "four cells cause exactly one SDL texture draw and no covering diagnostics");
            check(pixel(renderer,200,150)==std::array<std::uint8_t,4>{255,0,0,255} &&
                  pixel(renderer,160,170)==std::array<std::uint8_t,4>{0,255,0,255} &&
                  pixel(renderer,240,170)==std::array<std::uint8_t,4>{0,0,255,255} &&
                  pixel(renderer,200,190)==std::array<std::uint8_t,4>{255,255,0,255} &&
                  pixel(renderer,239,196)==std::array<std::uint8_t,4>{0,0,255,255},
                  "software-rendered asymmetric footprint has correct orientation");
            for (const auto where : {maps::GridCell{100,100},maps::GridCell{101,100},
                                     maps::GridCell{100,101},maps::GridCell{101,101}}) {
                const auto ground=maps::terrain_world(where,sparse.border);
                const scene::Point inside{ground.x,ground.y+20};
                const auto screen=camera.world_to_screen(inside);
                check(maps::pick_terrain_cell(camera.screen_to_world(screen),sparse)==where,
                      "each of the four source cells remains independently selectable");
                camera.zoom_at(screen,1.4);
                check(maps::pick_terrain_cell(camera.screen_to_world(camera.world_to_screen(inside)),
                      sparse)==where,"selection survives camera zoom and reprojection");
            }
            preview.shutdown();
        }
        {
            auto layered_candidates=multi_candidates;
            set_id(layered_candidates,102,101,0xc00a);
            auto layered=maps::make_stored_graphics_plan(map,layered_candidates,
                sparse_geometry({{100,100},{101,100},{100,101},{101,101},{102,101}}),
                terrain_catalog,*terrain_layout,elevation_catalog,*elevation_layout,true);
            openemperor::StoredGraphicsRenderer preview{std::move(layered)};
            preview.initialize(renderer);
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            camera.center_on(maps::terrain_world({100,100},sparse.border));
            check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
                  preview.render(camera,std::nullopt) && preview.last_texture_draws()==2 &&
                  pixel(renderer,239,196)==std::array<std::uint8_t,4>{255,0,0,255},
                  "front one-cell overlay paints over the two-cell image at its ground depth");
            preview.shutdown();
        }
        {
            openemperor::StoredGraphicsRenderer preview{std::move(two_plan)};
            preview.initialize(renderer);
            check(preview.upload_count()==3 && preview.plan().footprint_count(2)==3 &&
                  preview.plan().footprint_count(1)==2 && preview.plan().covered_cells()==14,
                  "two same-asset footprints remain two placements but share one uploaded texture");
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            const auto first=maps::terrain_world({100,100},sparse.border);
            camera.zoom=0.5; camera.center_on({first.x+160,first.y+80});
            check(preview.render(camera,std::nullopt) && preview.last_texture_draws()==5 &&
                  preview.last_diagnostic_draws()==0,
                  "all five supported instances draw once in a wide viewport");
            preview.shutdown();
        }
        {
            auto tall_plan=maps::make_stored_graphics_plan(map,multi_candidates,
                sparse_geometry({{108,100},{109,100},{108,101},{109,101}}),terrain_catalog,
                *terrain_layout,elevation_catalog,*elevation_layout,true);
            openemperor::StoredGraphicsRenderer preview{std::move(tall_plan)};
            preview.initialize(renderer);
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            camera.center_on(maps::terrain_world({108,100},sparse.border));
            camera.offset.y+=170; // Ground anchor is below the viewport; upper image rect intersects it.
            check(preview.render(camera,std::nullopt) && preview.last_texture_draws()==1,
                  "culling uses the whole 158x125 image despite an offscreen ground anchor");
            preview.shutdown();
        }
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
        {
            auto fresh=maps::make_stored_graphics_plan(map,multi_candidates,
                sparse_geometry({{100,100},{101,100},{100,101},{101,101}}),terrain_catalog,
                *terrain_layout,elevation_catalog,*elevation_layout,true);
            openemperor::MapDebugView view{map,maps::RawLayer::Terrain,
                maps::MapViewMode::StoredGraphics,std::nullopt,std::move(fresh)};
            view.initialize(window,renderer);
            bool running=true;
            // Fixed 400x300 fit: rear/top ground (200,128), zoom 2; click
            // inside each distinct 80x40 logical diamond, not its image pixels.
            for (const auto [where,sx,sy] : std::array<std::tuple<maps::GridCell,float,float>,4>{
                std::tuple{maps::GridCell{100,100},200.0F,168.0F},
                std::tuple{maps::GridCell{101,100},280.0F,208.0F},
                std::tuple{maps::GridCell{100,101},120.0F,208.0F},
                std::tuple{maps::GridCell{101,101},200.0F,248.0F}}) {
                SDL_Event click{}; click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
                click.button.button=SDL_BUTTON_LEFT; click.button.x=sx; click.button.y=sy;
                view.handle_event(click,running);
                check(view.selected_cell()==where && view.render(),
                      "SDL click selects the precise owned storage cell, not only the image instance");
            }
            SDL_Event wheel{}; wheel.type=SDL_EVENT_MOUSE_WHEEL;
            wheel.wheel.mouse_x=200; wheel.wheel.mouse_y=150; wheel.wheel.y=2;
            view.handle_event(wheel,running);
            check(view.selected_cell()==maps::GridCell{101,101} && view.render(),
                  "selected member persists after opt-in preview zoom");
            check(SDL_SetWindowSize(window,500,350) && view.render() &&
                  view.selected_cell()==maps::GridCell{101,101},
                  "selected member persists after opt-in preview resize");
            view.shutdown();
            SDL_SetWindowSize(window,400,300);
        }
        {
            Bytes wall_bitmap=red;
            for (std::uint32_t part=0;part<16;++part) {
                const auto pixels=tile(part==15 ? 0x001f : 0x7c00);
                wall_bitmap.insert(wall_bitmap.end(),pixels.begin(),pixels.end());
            }
            const Bytes wall_overlay{255,158,1,0xe0,0x03};
            wall_bitmap.insert(wall_bitmap.end(),wall_overlay.begin(),wall_overlay.end());
            record(slot8_sg3,202,3200,51205,30,318,167,51200,0,4);
            write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",slot8_sg3);
            write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.555",wall_bitmap);
            auto make_squares=[&](const std::vector<maps::GridCell>& origins) {
                auto c=candidates_fixture();
                std::vector<maps::GridCell> cells;
                for (const auto origin:origins)
                    for (std::uint32_t dy=0;dy<4;++dy) for (std::uint32_t dx=0;dx<4;++dx) {
                        const auto x=origin.x+dx, y=origin.y+dy;
                        const auto byte=static_cast<std::uint8_t>((dy<<3U)|dx|
                            ((dx==0 && dy==3) ? 0x40U : 0U));
                        set_id(c,x,y,0x20001,byte);
                        cells.push_back({x,y});
                    }
                return std::pair{c,cells};
            };
            const auto make_wall_plan=[&](const maps::MapGraphicCandidates& c,
                                          const std::vector<maps::GridCell>& cells,
                                          maps::FootprintPolicy policy) {
                const auto mask=sparse_geometry(cells);
                const auto registrations=maps::load_stored_archive_registrations(
                    temp.path,c,mask,maps::StoredGraphicsProfile::Slot8);
                return maps::make_stored_graphics_plan(map,c,mask,registrations,policy,
                    maps::StoredGraphicsProfile::Slot8);
            };
            auto [wall_candidates,wall_cells]=make_squares({{100,100},{104,100},{100,104}});
            auto baseline=make_wall_plan(wall_candidates,wall_cells,maps::FootprintPolicy::EdgeBytePreview);
            check(baseline.footprint_count(4)==0 && baseline.covered_cells()==0 &&
                  baseline.status_counts().at("unsupported_footprint_size")==48,
                  "unchanged edge-byte policy rejects 4x4 records");
            auto walls=make_wall_plan(wall_candidates,wall_cells,maps::FootprintPolicy::EdgeByte4x4Preview);
            check(walls.footprint_count(4)==3 && walls.covered_cells()==48 &&
                  walls.marker_deviations==0 && walls.footprint_histogram().at(4)==3 &&
                  walls.at({100,100})->footprint_index!=walls.at({104,100})->footprint_index &&
                  walls.at({100,100})->asset_index==walls.at({100,104})->asset_index,
                  "adjacent horizontal and vertical 4x4 groups share only asset identity");
            std::reverse(wall_cells.begin(),wall_cells.end());
            auto reverse=make_wall_plan(wall_candidates,wall_cells,maps::FootprintPolicy::EdgeByte4x4Preview);
            check(reverse.footprint_count(4)==3 && reverse.covered_cells()==48 &&
                  reverse.at({100,100})->footprint_index==walls.at({100,100})->footprint_index,
                  "input order cannot change 4x4 ownership");
            for (std::uint32_t dy=0;dy<4;++dy) for (std::uint32_t dx=0;dx<4;++dx) {
                const auto* cell=walls.at({100+dx,100+dy});
                check(cell && cell->subtile && cell->subtile->part_x==dx &&
                      cell->subtile->part_y==dy && cell->subtile_origin==maps::GridCell{100,100} &&
                      cell->physical_record==202 && cell->footprint_index==walls.at({100,100})->footprint_index,
                      "all 16 original cells retain metadata and one footprint owner");
            }
            auto missing_wall_cells=wall_cells;
            missing_wall_cells.erase(std::remove(missing_wall_cells.begin(),missing_wall_cells.end(),
                maps::GridCell{103,103}),missing_wall_cells.end());
            auto incomplete=make_wall_plan(wall_candidates,missing_wall_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(incomplete.footprint_count(4)==2 && incomplete.at({100,100})->status==
                  maps::StoredStatus::AnchorUnresolved && incomplete.covered_cells()==32,
                  "mask boundary never manufactures a missing sixteenth part");
            auto wrong_wall_candidates=wall_candidates;
            set_id(wrong_wall_candidates,103,103,0x20001,0x1c);
            auto bad=make_wall_plan(wrong_wall_candidates,wall_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(bad.footprint_count(4)==2 && bad.at({100,100})->status==
                  maps::StoredStatus::ConflictingFootprint && bad.covered_cells()==32,
                  "wrong 4x4 part does not unlock the otherwise valid shared asset");
            openemperor::StoredGraphicsRenderer partial_preview{std::move(bad)};
            partial_preview.initialize(renderer);
            check(partial_preview.upload_count()==1 && partial_preview.plan().covered_cells()==32 &&
                  partial_preview.plan().at({100,100})->status==maps::StoredStatus::ConflictingFootprint &&
                  partial_preview.plan().at({104,100})->status==maps::StoredStatus::Rendered,
                  "one shared decoded texture cannot unlock an invalid 4x4 group");
            partial_preview.shutdown();
            auto different_wall_candidates=wall_candidates;
            set_id(different_wall_candidates,103,103,0x20000,0x1b);
            auto different_wall=make_wall_plan(different_wall_candidates,wall_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(different_wall.footprint_count(4)==2 &&
                  different_wall.at({100,100})->status==maps::StoredStatus::ConflictingFootprint &&
                  different_wall.at({103,103})->physical_record==201,
                  "different saved ID and physical AssetId cannot complete a 4x4 group");
            auto high_wall_candidates=wall_candidates;
            set_id(high_wall_candidates,100,100,0x20001,0x80);
            auto diagnostic=make_wall_plan(high_wall_candidates,wall_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(diagnostic.footprint_count(4)==3 && diagnostic.unknown_bit_cells==1 &&
                  diagnostic.marker_deviations==0 && diagnostic.at({100,100})->candidate_byte==0x80,
                  "unknown high bit is retained without changing placement");
            auto marker_wall_candidates=wall_candidates;
            set_id(marker_wall_candidates,100,103,0x20001,0x18);
            auto marker_plan=make_wall_plan(marker_wall_candidates,wall_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(marker_plan.footprint_count(4)==3 && marker_plan.marker_deviations==1,
                  "marker deviation stays visible without suppressing a complete group");
            auto negative_candidates=wall_candidates;
            set_id(negative_candidates,0,0,0x20001,3);
            auto negative=make_wall_plan(negative_candidates,{{0,0}},
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(negative.at({0,0})->status==maps::StoredStatus::SubtilePositionInvalid &&
                  !negative.at({0,0})->subtile_origin,
                  "negative 4x4 origin is rejected before unsigned conversion");
            auto mixed_candidates=wall_candidates;
            auto mixed_cells=wall_cells;
            for (std::uint32_t dy=0;dy<2;++dy) for (std::uint32_t dx=0;dx<2;++dx) {
                const auto x=110+dx,y=100+dy;
                set_id(mixed_candidates,x,y,0xc002,static_cast<std::uint8_t>((dy<<3U)|dx|
                    ((dx==0 && dy==1) ? 0x40U : 0U)));
                mixed_cells.push_back({x,y});
            }
            set_id(mixed_candidates,114,100,0xc000);
            mixed_cells.push_back({114,100});
            auto mixed=make_wall_plan(mixed_candidates,mixed_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(mixed.footprint_count(1)==1 && mixed.footprint_count(2)==1 &&
                  mixed.footprint_count(4)==3 && mixed.covered_cells()==53,
                  "one-, two-, and four-cell-sided previews coexist without fallback");
            auto collision_candidates=wall_candidates;
            std::vector<maps::GridCell> collision_cells;
            for (std::uint32_t dy=0;dy<4;++dy) for (std::uint32_t dx=0;dx<4;++dx)
                collision_cells.push_back({100+dx,100+dy});
            for (std::uint32_t dy=0;dy<2;++dy) for (std::uint32_t dx=0;dx<2;++dx) {
                const auto x=103+dx,y=103+dy;
                set_id(collision_candidates,x,y,0xc002,
                    static_cast<std::uint8_t>((dy<<3U)|dx|
                        ((dx==0 && dy==1) ? 0x40U : 0U)));
                if (x>103 || y>103) collision_cells.push_back({x,y});
            }
            auto collision=make_wall_plan(collision_candidates,collision_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            check(collision.footprint_count(4)==0 && collision.footprint_count(2)==0 &&
                  collision.covered_cells()==0,
                  "overlapping 2x2/4x4 claims invalidate both groups");
            for (const auto [width,height,base,flag]:
                 std::array<std::tuple<std::uint16_t,std::uint16_t,std::uint32_t,std::uint8_t>,3>{
                     std::tuple{318,159,51200,4},std::tuple{318,167,51198,4},
                     std::tuple{318,167,51200,3}}) {
                auto invalid_sg3=slot8_sg3;
                record(invalid_sg3,202,3200,51205,30,width,height,base,0,flag);
                write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",invalid_sg3);
                auto invalid=make_wall_plan(wall_candidates,wall_cells,
                    maps::FootprintPolicy::EdgeByte4x4Preview);
                check(invalid.footprint_count(4)==0 && invalid.covered_cells()==0,
                      "bad 4x4 base, height, or size flag remains diagnostic");
            }
            write(temp.path/"DATA/China_Mon_Earthen_Greatwall_1.sg3",slot8_sg3);
            auto [single_wall_candidates,single_wall_cells]=make_squares({{100,100}});
            auto single_wall=make_wall_plan(single_wall_candidates,single_wall_cells,
                maps::FootprintPolicy::EdgeByte4x4Preview);
            openemperor::StoredGraphicsRenderer single_preview{std::move(single_wall)};
            single_preview.initialize(renderer);
            scene::Camera2D single_camera;
            single_camera.viewport_width=400; single_camera.viewport_height=300;
            single_camera.center_on(maps::terrain_world({100,100},single_preview.plan().border));
            check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
                  single_preview.render(single_camera,std::nullopt) &&
                  single_preview.last_texture_draws()==1 &&
                  single_preview.last_diagnostic_draws()==0 &&
                  pixel(renderer,199,143)==std::array<std::uint8_t,4>{0,255,0,255} &&
                  pixel(renderer,199,150)==std::array<std::uint8_t,4>{255,0,0,255} &&
                  pixel(renderer,199,270)==std::array<std::uint8_t,4>{0,0,255,255},
                  "full 318x167 image includes top Omega overlay and asymmetric base orientation");
            single_preview.shutdown();
            openemperor::StoredGraphicsRenderer preview{std::move(walls)};
            preview.initialize(renderer);
            check(preview.upload_count()==1 && preview.plan().covered_cells()==48 &&
                  preview.plan().status_counts().at("rendered")==48,
                  "three 4x4 placements use one decoded and uploaded texture");
            scene::Camera2D camera; camera.viewport_width=400; camera.viewport_height=300;
            camera.zoom=0.6; camera.center_on(maps::terrain_world({102,102},preview.plan().border));
            check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
                  preview.render(camera,std::nullopt) && preview.last_texture_draws()==3 &&
                  preview.last_diagnostic_draws()==0,
                  "4x4 previews draw once per instance with no owned-cell overpaint");
            camera.zoom_at({200,150},1.1);
            check(preview.render(camera,std::nullopt) && preview.upload_count()==1,
                  "camera movement does not upload another large texture");
            const auto mask=sparse_geometry(wall_cells);
            for (std::uint32_t dy=0;dy<4;++dy) for (std::uint32_t dx=0;dx<4;++dx) {
                const maps::GridCell at{100+dx,100+dy};
                const auto ground=maps::terrain_world(at,mask.border);
                const scene::Point inside{ground.x,ground.y+20};
                check(maps::pick_terrain_cell(camera.screen_to_world(camera.world_to_screen(inside)),mask)==at,
                      "each 4x4 member remains independently selectable");
            }
            preview.shutdown();
            write(temp.path/"Cities/Wall4.map",stored_map_file(0x80,false,84,false,true));
            auto wall_catalog=maps::discover_standalone_maps(temp.path);
            std::erase_if(wall_catalog.entries,[](const auto& entry) {
                return entry.relative_path!=fs::path{"Cities/Wall4.map"};
            });
            check(wall_catalog.entries.size()==1,"synthetic browser wall map discovered");
            openemperor::MapBrowser wall_browser{std::move(wall_catalog),
                maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8};
            wall_browser.initialize(window,renderer);
            check(wall_browser.open_selected() && wall_browser.render() &&
                  wall_browser.statuses().front()=="snapshot_complete",
                  "browser forwards 4x4 policy and resolves all synthetic wall parts");
            wall_browser.shutdown();
        }
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        {
            std::ostringstream output;
            auto* previous=std::cout.rdbuf(output.rdbuf());
            const auto code=openemperor::run_map_render_check(temp.path,"Cities/Wall4.map",
                maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
            std::cout.rdbuf(previous);
            const auto report=nlohmann::json::parse(output.str());
            check(code==0 && report.at("schema")=="openemperor-map-render-check-v2" &&
                  report.at("footprint_policy")=="edge-byte-4x4" &&
                  report.at("footprint_size_histogram").at("4")==1 &&
                  report.at("footprint_instances_planned")==report.at("footprint_instances_rendered") &&
                  report.at("covered_cells")==report.at("candidate_cells"),
                  "render-check report accounts for the synthetic 16-cell wall instance");
        }
        const auto check_report=[&](const fs::path& relative) {
            std::ostringstream output;
            auto* previous=std::cout.rdbuf(output.rdbuf());
            const auto code=openemperor::run_map_render_check(temp.path,relative,
                maps::FootprintPolicy::EdgeBytePreview);
            std::cout.rdbuf(previous);
            check(code==0,"headless render check returns without event loop");
            return nlohmann::json::parse(output.str());
        };
        const auto full_report=check_report("Cities/A.map");
        const auto partial_report=check_report("Cities/C.map");
        {
            std::ostringstream output;
            auto* previous=std::cout.rdbuf(output.rdbuf());
            const auto code=openemperor::run_map_render_check(temp.path,"Cities/Slot8.map",
                maps::FootprintPolicy::EdgeBytePreview,maps::StoredGraphicsProfile::Slot8);
            std::cout.rdbuf(previous);
            const auto selected=nlohmann::json::parse(output.str());
            check(code==0 && selected.at("graphics_profile")==maps::stored_graphics_slot8_profile &&
                  selected.at("status")=="snapshot_complete" &&
                  selected.at("decoded_assets")==2 && selected.at("texture_uploads")==2,
                  "headless check uses same extended registration and actual decode");
        }
        check(full_report.at("status")=="snapshot_complete" &&
              full_report.at("stages").at("render_frames")==true &&
              full_report.at("decoded_assets")==1 && full_report.at("texture_uploads")==1 &&
              full_report.at("candidate_cells")==full_report.at("covered_cells") &&
              partial_report.at("status")=="snapshot_partial" &&
              partial_report.at("diagnostic_cells")==1 &&
              partial_report.at("stages").at("render_frames")==true,
              "render-check JSON distinguishes full and partial actual decoder/renderer runs");
        {
            std::ostringstream output;
            auto* previous=std::cout.rdbuf(output.rdbuf());
            const auto code=openemperor::run_map_render_check(temp.path,"Cities/A.map",
                maps::FootprintPolicy::EdgeBytePreview,maps::StoredGraphicsProfile::Slot8);
            std::cout.rdbuf(previous);
            const auto selected=nlohmann::json::parse(output.str());
            check(code==0 && selected.at("graphics_profile")==maps::stored_graphics_slot8_profile &&
                  selected.at("covered_cells")==full_report.at("covered_cells"),
                  "headless report states the selected profile and unused extra archive is harmless");
        }
        {
            std::ostringstream output;
            auto* previous=std::cout.rdbuf(output.rdbuf());
            const auto code=openemperor::run_map_render_check(temp.path,"Cities/Broken.map",
                maps::FootprintPolicy::EdgeBytePreview);
            std::cout.rdbuf(previous);
            const auto failed=nlohmann::json::parse(output.str());
            check(code==1 && failed.at("status")=="load_failed" &&
                  failed.at("stages").at("container_valid")==false,
                  "headless failure still emits one valid JSON report");
        }
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
