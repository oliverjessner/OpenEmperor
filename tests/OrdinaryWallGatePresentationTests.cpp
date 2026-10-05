#include "assets/AssetCatalog.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/LandscapeProvenance.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "maps/WallTopology.h"
#include "renderer/StoredGraphicsRenderer.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace maps=openemperor::maps;
namespace scene=openemperor::scene;
namespace perf=openemperor::performance;
using Bytes=std::vector<std::uint8_t>;
using Color=std::array<std::uint8_t,4>;
constexpr std::size_t cell_count=228U*228U;
constexpr maps::GridCell left{99,100},right{101,100},repeated{99,104};
constexpr maps::GridCell gate{100,100},other_gate{100,104},gate_origin{104,104};
constexpr Color red{255,0,0,255},blue{0,0,255,255},green{0,255,0,255};
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::size_t raw(maps::GridCell cell) {return std::size_t(cell.y)*228U+cell.x;}
void u16(Bytes& b,std::size_t p,unsigned n) {b.at(p)=std::uint8_t(n);b.at(p+1)=std::uint8_t(n>>8);}
void u32(Bytes& b,std::size_t p,unsigned n) {u16(b,p,n);u16(b,p+2,n>>16);}
void write(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    check(bool(out),"write authored wall/gate fixture");
}
void skip(Bytes& bytes,unsigned length) {
    while(length) {const auto n=std::min(length,254U);bytes.push_back(255);bytes.push_back(std::uint8_t(n));length-=n;}
}
Bytes patch(unsigned width,unsigned color) {
    Bytes bytes;skip(bytes,5*width+width/2-1);
    for(unsigned y=0;y<3;++y) {
        bytes.push_back(3);for(unsigned x=0;x<3;++x) {bytes.push_back(std::uint8_t(color));bytes.push_back(std::uint8_t(color>>8));}
        if(y<2) skip(bytes,width-3);
    }
    return bytes;
}
struct Temp {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("openemperor-wall-gates-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    explicit Temp(bool corrupt=false) {
        std::filesystem::create_directories(root/"DATA");
        // The first eighteen images are an authored static wall group. The
        // final three images are historical fixture images, not gate identity
        // or original pixels. A gate's model/body is deliberately not inferred.
        Bytes sg3(40680+22*72,0),bitmap(4,0);
        u32(sg3,0,unsigned(sg3.size()));u32(sg3,4,214);u32(sg3,12,22);u32(sg3,16,22);u32(sg3,20,1);
        const std::string name="authored.bmp";std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,804,22);
        for(unsigned image=1;image<=21;++image) {
            const unsigned side=image==21 ? 2:1,width=80*side-2;
            Bytes payload(3200*side*side);
            for(std::size_t p=0;p<payload.size();p+=2) u16(payload,p,0x7fff);
            const auto overlay=corrupt && image==17 ? Bytes{255}:patch(width,image<=18 ? 0x001f:image==19 ? 0x7c00:0x03e0);
            payload.insert(payload.end(),overlay.begin(),overlay.end());
            const auto p=40680+image*72;
            u32(sg3,p,unsigned(bitmap.size()));u32(sg3,p+4,unsigned(payload.size()));u32(sg3,p+8,3200*side*side);
            u16(sg3,p+20,width);u16(sg3,p+22,100);u16(sg3,p+50,30);sg3[p+55]=std::uint8_t(side);
            bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        }
        write(root/"DATA/authored.sg3",sg3);write(root/"DATA/authored.555",bitmap);
    }
    ~Temp() {std::error_code error;std::filesystem::remove_all(root,error);}
};
maps::StoredArchiveRegistrations registrations(const Temp& temp) {
    maps::StoredArchiveRegistration r;r.slot=2;r.relative_path="DATA/authored.sg3";
    r.catalog=openemperor::assets::scan_asset_archive(temp.root,r.relative_path);
    check(r.catalog->records.size()==22 && r.catalog->archive_errors.empty(),"authored archive metadata is valid");
    r.layout.emplace();auto& layout=*r.layout;
    layout.slot=2;layout.sg3_version=213;layout.verified_registration=true;
    layout.image_capacity=22;layout.runtime_image_count=18;layout.first_physical_record=1;
    layout.groups.resize(81); // Resource 0x451 is slot2/group-position80.
    for(unsigned i=0;i<layout.groups.size();++i) layout.groups[i]={i,80+2*i,0,0};
    return {{2,std::move(r)}};
}
scene::Point top(maps::GridCell cell) {
    // Independent literal projection and fixture anchor: 78x100, side1,
    // border68. This does not ask the production origin/picking helper.
    return {(int(cell.x)-int(cell.y))*40.-39.,(int(cell.x)+int(cell.y)-136)*20.-60.};
}
maps::StoredGraphicsPlan historical(const Temp& temp,const maps::StoredArchiveRegistrations& registered) {
    maps::StoredGraphicsPlan p;p.data_root=temp.root;p.border=68;p.landscape_layers_available=true;
    p.raw_terrain.assign(cell_count,0);p.raw_objects.assign(cell_count,0);p.raw_saved_ids.assign(cell_count,0);
    p.raw_candidate_bytes.assign(cell_count,0);p.draw_properties.assign(cell_count,0);p.height_bytes.assign(cell_count,0);
    p.variation_bytes.assign(cell_count,193);p.fertility_bytes.assign(cell_count,89);
    p.cell_by_storage.resize(cell_count);p.status_by_storage.assign(cell_count,maps::StoredStatus::Excluded);
    const auto& records=registered.at(2).catalog->records;
    p.assets.push_back({records[19],maps::StoredStatus::DecodePending,false,false,{}});
    p.assets.push_back({records[20],maps::StoredStatus::DecodePending,false,false,{}});
    const std::array cells{left,gate,right,repeated,other_gate,gate_origin};
    for(const auto cell:cells) {
        const bool wall=cell==left || cell==right || cell==repeated;
        maps::StoredCell c;c.storage=cell;c.cell_index=raw(cell);c.terrain_raw=wall ? 0x4080:cell==gate_origin ? 0xc080:0x8080;
        c.slot=2;c.stored_id=wall ? 0x8123:0x8321;c.objects_raw=0x12340000;c.candidate_byte=7;
        c.status=maps::StoredStatus::DecodePending;c.asset_index=wall ? 0:1;c.footprint_index=p.footprints.size();
        c.world={(int(cell.x)-int(cell.y))*40.,(int(cell.x)+int(cell.y)-136)*20.};c.image_origin=top(cell);
        maps::PlacedFootprint f;f.id=p.footprints.size();f.asset_index=*c.asset_index;f.origin=cell;
        f.cell_indices={p.cells.size()};f.image_origin=c.image_origin;f.status=c.status;
        p.raw_terrain[c.cell_index]=c.terrain_raw;p.raw_objects[c.cell_index]=c.objects_raw;
        p.raw_saved_ids[c.cell_index]=c.stored_id;p.raw_candidate_bytes[c.cell_index]=c.candidate_byte;
        p.cell_by_storage[c.cell_index]=p.cells.size();p.status_by_storage[c.cell_index]=c.status;
        p.footprints.push_back(f);p.cells.push_back(c);
    }
    return p;
}
std::size_t candidate(const maps::StoredGraphicsPlan& p,maps::GridCell cell) {return *p.cell_by_storage[raw(cell)];}
scene::Point body(maps::GridCell cell,int height=0) {const auto origin=top(cell);return {origin.x+39.5,origin.y+5.5-40.*height};}
Color pixel(SDL_Renderer* renderer,scene::Point point) {
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);check(surface,"read authored test pixels");Color color{};
    const bool read=SDL_ReadSurfacePixel(surface,int(point.x),int(point.y),&color[0],&color[1],&color[2],&color[3]);
    SDL_DestroySurface(surface);check(read,"authored sample is in viewport");return color;
}
void frame(openemperor::StoredGraphicsRenderer& view,SDL_Renderer* renderer,const scene::Camera2D& camera) {
    check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) && view.render(camera,std::nullopt),
          "draw through the production stored renderer");
}
const std::array counters{perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::AssetDecodes,
    perf::Counter::TextureUploads,perf::Counter::WorldCopies,perf::Counter::WorldExecutes,
    perf::Counter::BfsCalls,perf::Counter::RouteRefreshes};
void no_work() {for(const auto counter:counters) check(perf::counter(counter)==0,"frame/F1/picking has no asset or authoritative work");}
void counts(const maps::StoredGraphicsPlan& p,unsigned active,unsigned fallback) {
    const auto report=maps::landscape_fidelity_report(p).at("regeneration");
    const auto& gates=report.at("normal_wall_gate_connections");
    check(gates.at("inspected_wall_cells")==4 && gates.at("gate_adjacent_cells")==3 &&
          gates.at("selected_static_connections")==3 && gates.at("active_static_connections")==active &&
          gates.at("selected_fallback_cells")==fallback && gates.at("gate_origin_composition_required")==1 &&
          gates.at("gate_bodies_reproduced")==0 && gates.at("optional_model_components_reproduced")==0,
          "report separates static selection, readiness, fallback and unreproduced gate bodies");
    check(report.at("normal_wall_selector_verified")==active && report.at("great_wall_instances")==0 &&
          report.at("full_original_composition_verified")==0,"active wall counts do not claim gates or complete original composition");
}
void positive(SDL_Renderer* renderer) {
    Temp temp;const auto registered=registrations(temp);auto p=historical(temp,registered);const auto before=p;
    maps::build_regenerated_map_render_plan(p,registered);
    check(p.regenerated->instances.size()==3 && p.regenerated->normal_wall_topology.size()==4,
          "load publishes three static singleton connections and caches the rejected gate origin");
    const auto& a=p.regenerated->instances[*p.regenerated->cells[candidate(p,left)].instance_index];
    const auto& b=p.regenerated->instances[*p.regenerated->cells[candidate(p,right)].instance_index];
    const auto& c=p.regenerated->instances[*p.regenerated->cells[candidate(p,repeated)].instance_index];
    check(a.geometry.selection.variant==16 && b.geometry.selection.variant==14 && c.geometry.selection.variant==16 &&
          a.graphic.value==0x8010 && b.graphic.value==0x800e && a.asset_index==c.asset_index && p.assets.size()==4,
          "independent east/west endpoints resolve bounded group451 and dedupe the repeated physical record");
    check(p.assets[a.asset_index].record.id.image_index==17 && p.assets[b.asset_index].record.id.image_index==15,
          "the runtime-local wall record translates to physical position exactly once");
    const std::map<std::uint32_t,maps::GroupRegistration> groups{{2,{&*registered.at(2).layout}}};
    const auto last=maps::resolve_landscape_variant({0x451},17,groups);
    check(last && last->value==0x8011 &&
          !maps::resolve_landscape_variant({0x451},18,groups),
          "the selected static group cannot borrow an adjacent physical record");
    for(const auto& instance:p.regenerated->instances)
        check(instance.geometry.side==1 && instance.geometry.owned_cells==std::vector{instance.geometry.origin} &&
              instance.geometry.draw_cell==instance.geometry.origin && !instance.geometry.explicit_anchor &&
              !instance.geometry.explicit_height && !instance.geometry.depth_cell && !instance.geometry.original_entity_index &&
              !instance.great_wall_context && instance.composition_policy==maps::LandscapeCompositionPolicy::EarlyBaseSpatialOverlay,
              "only ordinary one-cell walls own cells using the unchanged split composition and height contract");
    for(const auto cell:{gate,other_gate,gate_origin})
        check(!p.regenerated->cells[candidate(p,cell)].instance_index,"no historical gate cell receives a new wall owner");
    check(p.raw_terrain==before.raw_terrain && p.raw_objects==before.raw_objects && p.raw_saved_ids==before.raw_saved_ids &&
          p.raw_candidate_bytes==before.raw_candidate_bytes && p.height_bytes==before.height_bytes &&
          p.status_by_storage==before.status_by_storage,"preparation preserves all raw and buildability layers");
    for(std::size_t i=0;i<p.footprints.size();++i) {
        const auto& f=p.footprints[i];const auto& old=before.footprints[i];
        check(f.asset_index==old.asset_index && f.origin==old.origin && f.cell_indices==old.cell_indices &&
              f.width_cells==old.width_cells && f.height_cells==old.height_cells &&
              f.image_origin.x==old.image_origin.x && f.image_origin.y==old.image_origin.y &&
              p.cells[i].stored_id==before.cells[i].stored_id && p.cells[i].status==before.cells[i].status,
              "new claims preserve the old gate/wall partition, images, positions and source statuses");
    }
    auto independent=before;independent.raw_saved_ids.assign(cell_count,0xffffffff);independent.raw_objects.assign(cell_count,0);
    independent.variation_bytes.assign(cell_count,0);independent.height_bytes.assign(cell_count,255);
    for(auto& cell:independent.cells) {cell.stored_id=0xffffffff;cell.objects_raw=0;}
    maps::build_regenerated_map_render_plan(independent,registered);
    for(std::size_t i=0;i<p.regenerated->instances.size();++i)
        check(independent.regenerated->instances[i].graphic.value==p.regenerated->instances[i].graphic.value &&
              independent.regenerated->instances[i].geometry.owned_cells==p.regenerated->instances[i].geometry.owned_cells,
              "saved IDs, object fields, variation and height do not choose wall/gate topology");
    const auto selected=p.regenerated;maps::build_regenerated_map_render_plan(p,registered);
    check(p.regenerated==selected && p.assets.size()==4,"preparation remains immutable and physically deduplicated");
    openemperor::StoredGraphicsRenderer view{std::move(p)};view.initialize(renderer);view.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
    counts(view.plan(),3,0);check(view.upload_count()==12,"four physical images upload three components each once");
    scene::Camera2D camera;camera.viewport_width=600;camera.viewport_height=500;camera.center_on({0,1320});
    frame(view,renderer,camera);check(view.last_texture_draws()==12,"one base/overlay pair per wall or historical gate, with no old-wall double draw");
    for(double zoom:{1.,2.,4.}) {
        camera.zoom=zoom;camera.center_on({0,1260});frame(view,renderer,camera);
        check(pixel(renderer,camera.world_to_screen(body(left)))==blue && pixel(renderer,camera.world_to_screen(body(gate)))==green,
              "new wall and unchanged historical gate coexist at 1x/2x/4x");
        check(view.hit_test(camera.world_to_screen(body(left)),camera)==left && view.hit_test(camera.world_to_screen(body(gate)),camera)==gate,
              "each visible alpha pixel selects its own historical or regenerated cell");
        const auto origin=top(left);check(!view.hit_test(camera.world_to_screen({origin.x+30.5,origin.y+5.5}),camera),
              "a transparent overlay hole cannot claim the whole wall image rectangle");
    }
    camera.zoom=1;camera.center_on({0,1320});
    const auto f1=maps::landscape_provenance(view.plan(),left,true,&camera).at("regenerated");
    const auto& topology=f1.at("normal_wall_topology");
    check(topology.at("gate_connection")=="gate_adjacent_static_selected" && topology.at("wall_neighbor_mask")=="0x00" &&
          topology.at("gate_neighbor_mask")=="0x04" && topology.at("combined_neighbor_mask")=="0x04" &&
          topology.at("semantic_row")==13 && topology.at("selected_variant")==16 &&
          topology.at("gate_body_reproduced")==false && f1.at("atomic_instance_active")==true,
          "F1 uses cached neighbor evidence and independently reports atomic activation");
    check(maps::landscape_provenance(view.plan(),gate_origin,true).at("regenerated").at("normal_wall_topology").at("gate_connection")==
          "gate_origin_composition_required","gate-origin F1 retains a named separate-composition limitation");
    view.set_landscape_mode(openemperor::LandscapeDebugMode::Ground);frame(view,renderer,camera);
    check(pixel(renderer,camera.world_to_screen(body(left)))!=blue && !view.hit_test(camera.world_to_screen(body(left)),camera),
          "ordinary F8 Ground keeps the established overlay visibility and picking convention");
    view.set_landscape_mode(openemperor::LandscapeDebugMode::Snapshot);frame(view,renderer,camera);
    check(view.last_texture_draws()==6 && pixel(renderer,camera.world_to_screen(body(left)))==red &&
          pixel(renderer,camera.world_to_screen(body(gate)))==green,"Snapshot keeps exactly the six historical images");
    view.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
    perf::set_enabled(true);perf::reset();
    for(unsigned i=0;i<16;++i) {frame(view,renderer,camera);(void)view.hit_test(camera.world_to_screen(body(left)),camera);
        (void)maps::landscape_provenance(view.plan(),left,true,&camera);(void)maps::landscape_fidelity_report(view.plan());}
    no_work();perf::set_enabled(false);
}
void failures(SDL_Renderer* renderer) {
    for(unsigned failure=0;failure<3;++failure) {
        Temp temp{failure==2};auto registered=registrations(temp);auto p=historical(temp,registered);
        if(failure==0) registered.clear();
        if(failure==1) registered.at(2).catalog->records[17].width=158;
        maps::build_regenerated_map_render_plan(p,registered);
        check(p.regenerated->instances.size()==(failure==0 ? 0U:failure==1 ? 1U:3U),
              "missing registration and unsupported geometry publish no invalid owner, decode remains eager");
        openemperor::StoredGraphicsRenderer view{std::move(p)};view.initialize(renderer);
        view.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);counts(view.plan(),failure==0 ? 0:1,failure==0 ? 3:2);
        scene::Camera2D camera;camera.viewport_width=600;camera.viewport_height=500;camera.center_on({0,1320});frame(view,renderer,camera);
        check(pixel(renderer,camera.world_to_screen(body(left)))==red && view.hit_test(camera.world_to_screen(body(left)),camera)==left &&
              pixel(renderer,camera.world_to_screen(body(gate)))==green,"unavailable connection retains its complete old wall and unchanged gate image");
        const auto f1=maps::landscape_provenance(view.plan(),left,true,&camera);
        check(f1.at("render_source")=="historical saved-ID preview" &&
              f1.at("regenerated").at("normal_wall_topology").at("gate_connection")=="gate_adjacent_static_selected",
              "a supported static selection is separate from historical activation fallback");
        if(failure==2) {
            const auto& generated=view.plan().regenerated->cells[candidate(view.plan(),left)];
            check(view.plan().assets[*generated.asset_index].status==maps::StoredStatus::DecodeFailed &&
                  f1.at("regenerated").at("atomic_instance_active")==false,"failed shared decode activates neither repeated connection");
        }
    }
}
void height_and_overlap(SDL_Renderer* renderer) {
    Temp temp;const auto registered=registrations(temp);
    for(int height:{-1,2}) {
        auto p=historical(temp,registered);p.height_bytes[raw(left)]=std::uint8_t(height);
        maps::build_regenerated_map_render_plan(p,registered);openemperor::StoredGraphicsRenderer view{std::move(p)};
        view.initialize(renderer);view.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        scene::Camera2D camera;camera.viewport_width=600;camera.viewport_height=500;camera.center_on(body(left,height));frame(view,renderer,camera);
        check(pixel(renderer,camera.world_to_screen(body(left,height)))==blue &&
              view.hit_test(camera.world_to_screen(body(left,height)),camera)==left,"signed cell height times forty moves render and alpha together exactly once");
        const auto instance=maps::landscape_provenance(view.plan(),left,true).at("regenerated").at("instance");
        check(instance.at("height")==height && instance.at("height_cell")==nlohmann::json({{"x",99},{"y",100}}),
              "height provenance remains the wall cell rather than an adjacent gate");
    }
    auto p=historical(temp,registered);const auto member=candidate(p,left);
    p.assets.push_back({registered.at(2).catalog->records[21],maps::StoredStatus::DecodePending,false,false,{}});
    auto& old=p.footprints[member];old.origin={98,99};old.width_cells=2;old.height_cells=2;old.asset_index=2;
    old.image_origin={-119,1200}; // Literal side2 158x100 anchor at rear(98,99).
    for(const maps::GridCell cell:std::array<maps::GridCell,3>{{{98,99},{99,99},{98,100}}}) {
        maps::StoredCell c;c.storage=cell;c.cell_index=raw(cell);c.terrain_raw=8;c.slot=2;c.asset_index=2;c.footprint_index=old.id;
        c.status=maps::StoredStatus::DecodePending;c.world={(int(cell.x)-int(cell.y))*40.,(int(cell.x)+int(cell.y)-136)*20.};
        p.raw_terrain[c.cell_index]=8;p.cell_by_storage[c.cell_index]=p.cells.size();p.status_by_storage[c.cell_index]=c.status;
        old.cell_indices.push_back(p.cells.size());p.cells.push_back(c);
    }
    p.cells[member].asset_index=2;maps::build_regenerated_map_render_plan(p,registered);
    check(p.regenerated->instances.size()==3,"selector can prepare a singleton before historical overlap readiness");
    openemperor::StoredGraphicsRenderer view{std::move(p)};view.initialize(renderer);view.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
    counts(view.plan(),2,1);const auto f1=maps::landscape_provenance(view.plan(),left,true);
    check(f1.at("regenerated").at("atomic_instance_active")==false && f1.at("render_source")=="historical saved-ID preview",
          "one wall cannot split a complete historical multi-cell image");
    scene::Camera2D camera;camera.viewport_width=600;camera.viewport_height=500;camera.center_on({0,1320});frame(view,renderer,camera);
    const scene::Point old_pixel{-39.5,1205.5};
    check(pixel(renderer,camera.world_to_screen(old_pixel))==green &&
          view.hit_test(camera.world_to_screen(old_pixel),camera)==maps::GridCell{98,99},
          "whole historical alpha and pixels remain available while the overlapping new wall is inactive");
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"initialize isolated SDL software test");
        window=SDL_CreateWindow("authored ordinary-wall gate connections",600,500,SDL_WINDOW_HIDDEN);check(window,"create test window");
        renderer=SDL_CreateRenderer(window,"software");check(renderer && std::string(SDL_GetRendererName(renderer))=="software","verify actual software renderer");
        const auto textures=openemperor::StoredGraphicsRenderer::live_texture_count();
        positive(renderer);failures(renderer);height_and_overlap(renderer);
        check(openemperor::StoredGraphicsRenderer::live_texture_count()==textures,"all eager renderer textures shut down without leaks");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Ordinary wall gate presentation checks passed\n";return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);if(renderer) SDL_DestroyRenderer(renderer);if(window) SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
