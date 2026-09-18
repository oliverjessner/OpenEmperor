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
#include <initializer_list>
#include <iostream>
#include <stdexcept>
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
