#include "assets/AssetCatalog.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/LandscapeProvenance.h"
#include "maps/OrdinaryGateMapPresentation.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "scene/WorldDrawOrder.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
namespace maps=openemperor::maps;
namespace scene=openemperor::scene;
namespace perf=openemperor::performance;
using Bytes=std::vector<std::uint8_t>;
using Color=std::array<std::uint8_t,4>;
constexpr std::size_t count=228U*228U;
constexpr maps::GridCell origin{99,109};
constexpr Color red{255,0,0,255},green{0,255,0,255};
constexpr std::array<maps::GridCell,15> rows0{{{0,0},{1,0},{2,0},{3,0},{4,0},
    {0,1},{1,1},{2,1},{3,1},{4,1},{0,2},{1,2},{2,2},{3,2},{4,2}}};
constexpr std::array<maps::GridCell,15> rows1{{{2,0},{2,1},{2,2},{2,3},{2,4},
    {1,0},{1,1},{1,2},{1,3},{1,4},{0,0},{0,1},{0,2},{0,3},{0,4}}};
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::size_t raw(maps::GridCell cell) {return std::size_t(cell.y)*228U+cell.x;}
maps::GridCell add(maps::GridCell a,maps::GridCell b) {return {a.x+b.x,a.y+b.y};}
void u16(Bytes& b,std::size_t p,unsigned n) {b.at(p)=std::uint8_t(n);b.at(p+1)=std::uint8_t(n>>8);}
void u32(Bytes& b,std::size_t p,unsigned n) {u16(b,p,n);u16(b,p+2,n>>16);}
void write(const std::filesystem::path& p,const Bytes& b) {
    std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));
    check(bool(out),"write authored gate fixture");
}
void skip(Bytes& b,unsigned n) {while(n) {const auto k=std::min(n,254U);b.push_back(255);b.push_back(std::uint8_t(k));n-=k;}}
Bytes patch(unsigned width,unsigned color) {
    Bytes b;skip(b,5*width+width/2-1);
    for(unsigned y=0;y<3;++y) {b.push_back(3);for(unsigned x=0;x<3;++x) {b.push_back(std::uint8_t(color));b.push_back(std::uint8_t(color>>8));}
        if(y<2) skip(b,width-3);}
    return b;
}
Color variant_color(unsigned v) {const auto r=v+1;return {std::uint8_t((r<<3)|(r>>2)),0,255,255};}
struct Temp {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("openemperor-gate-body-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    explicit Temp(int malformed=-1) {
        std::filesystem::create_directories(root/"DATA");
        // Authored RGB555 markers identify thirty physical component records.
        // These pixels and the historical red images establish no Emperor
        // appearance, gate identity from pixels, or whole-scene parity.
        Bytes sg3(40680+33*72,0),bitmap(4,0);
        u32(sg3,0,unsigned(sg3.size()));u32(sg3,4,214);u32(sg3,12,33);u32(sg3,16,33);u32(sg3,20,1);
        const std::string name="authored.bmp";std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,804,33);
        for(unsigned i=1;i<=32;++i) {
            const unsigned side=i==32 ? 2:1,width=80*side-2;
            Bytes image(3200*side*side);for(std::size_t p=0;p<image.size();p+=2) u16(image,p,0x7fff);
            const auto overlay=int(i)-1==malformed ? Bytes{255}:patch(width,i<=30 ? (i<<10)|31U:0x7c00);
            image.insert(image.end(),overlay.begin(),overlay.end());const auto p=40680+i*72;
            u32(sg3,p,unsigned(bitmap.size()));u32(sg3,p+4,unsigned(image.size()));u32(sg3,p+8,3200*side*side);
            u16(sg3,p+20,width);u16(sg3,p+22,100);u16(sg3,p+50,30);sg3[p+55]=std::uint8_t(side);
            bitmap.insert(bitmap.end(),image.begin(),image.end());
        }
        write(root/"DATA/authored.sg3",sg3);write(root/"DATA/authored.555",bitmap);
    }
    ~Temp() {std::error_code e;std::filesystem::remove_all(root,e);}
};
maps::StoredArchiveRegistrations registrations(const Temp& t) {
    maps::StoredArchiveRegistration r;r.slot=2;r.relative_path="DATA/authored.sg3";
    r.catalog=openemperor::assets::scan_asset_archive(t.root,r.relative_path);
    check(r.catalog->records.size()==33 && r.catalog->archive_errors.empty(),"authored Type30 archive metadata");
    r.layout.emplace();auto& layout=*r.layout;layout.slot=2;layout.sg3_version=213;layout.verified_registration=true;
    layout.first_physical_record=1;layout.image_capacity=33;layout.runtime_image_count=30;layout.groups.resize(175);
    for(unsigned i=0;i<175;++i) layout.groups[i]={i,80+2*i,0,0};
    return {{2,std::move(r)}};
}
maps::OriginalEntityRecord source(unsigned layout,maps::GridCell start=origin,unsigned index=11,unsigned id=7) {
    maps::OriginalEntityRecord e;e.manager_index=index;e.entity_class=maps::OriginalEntityClass::GateHouse;e.status=3;
    e.footprint_side=1;e.type=130;e.subindex=0;e.local_x=std::int16_t(start.x-29);e.local_y=std::int16_t(start.y-29);
    e.serialized_cell_reference=std::int32_t(raw(start));e.serialized_original_id=std::int32_t(id);e.original_id=maps::OriginalEntityId{id};
    e.gate_house=maps::OriginalGateHouseState{std::int32_t(layout)};e.provenance.logical_record_offset=9984;
    e.provenance.logical_base_offset=10000;e.provenance.base_schema=5;e.provenance.wrapper_schema=1;
    e.provenance.logical_extended_offset=10184;e.provenance.extended_schema=1;
    e.provenance.logical_class_wrapper_offset=10312;e.provenance.class_wrapper_schema=2;return e;
}
struct Input {
    maps::OriginalMapEntities entities;
    std::vector<std::uint32_t> terrain=std::vector<std::uint32_t>(count,0);
    Input(unsigned layout,bool second=false,bool conflict=false) {
        entities.declared_map_size=170;entities.records.push_back(source(layout));
        if(second) entities.records.push_back(source(layout,conflict ? maps::GridCell{100,109}:maps::GridCell{120,130},12,8));
        const auto& rows=layout ? rows1:rows0;
        for(const auto& e:entities.records) for(const auto offset:rows)
            terrain[raw(add({unsigned(e.local_x)+29,unsigned(e.local_y)+29},offset))]=0x8088;
    }
    maps::OrdinaryGateMapPresentation prepare() const {return maps::prepare_ordinary_gate_presentation(entities,terrain);}
};
scene::Point top(maps::GridCell cell) {
    // Literal fixture projection/anchor, not the renderer or gate selector.
    return {(int(cell.x)-int(cell.y))*40.-39.,(int(cell.x)+int(cell.y)-58)*20.-60.};
}
scene::Point body(maps::GridCell cell,int height=0) {const auto p=top(cell);return {p.x+39.5,p.y+5.5-40.*height};}
maps::StoredGraphicsPlan historical(const Temp& t,const maps::StoredArchiveRegistrations& r,const Input& input,bool overlap=false) {
    maps::StoredGraphicsPlan p;p.data_root=t.root;p.border=29;p.landscape_layers_available=true;
    p.raw_terrain=input.terrain;p.raw_objects.assign(count,0xaabb0000);p.raw_saved_ids.assign(count,0xffffffff);
    p.raw_candidate_bytes.assign(count,255);p.draw_properties.assign(count,0);p.height_bytes.assign(count,0);
    p.variation_bytes.assign(count,213);p.fertility_bytes.assign(count,97);p.cell_by_storage.resize(count);
    p.status_by_storage.assign(count,maps::StoredStatus::Excluded);
    p.assets.push_back({r.at(2).catalog->records[31],maps::StoredStatus::DecodePending,false,false,{}});
    if(overlap) p.assets.push_back({r.at(2).catalog->records[32],maps::StoredStatus::DecodePending,false,false,{}});
    if(overlap) {p.raw_terrain[raw({98,109})]=8;p.raw_terrain[raw({98,110})]=8;}
    for(unsigned y=0;y<228;++y) for(unsigned x=0;x<228;++x) {
        const maps::GridCell cell{x,y};const auto at=raw(cell);if(!p.raw_terrain[at]) continue;
        maps::StoredCell c;c.storage=cell;c.cell_index=at;c.terrain_raw=p.raw_terrain[at];c.objects_raw=p.raw_objects[at];
        c.stored_id=0xffffffff;c.slot=2;c.asset_index=0;c.status=maps::StoredStatus::DecodePending;
        c.world={(int(x)-int(y))*40.,(int(x)+int(y)-58)*20.};c.image_origin=top(cell);
        p.cell_by_storage[at]=p.cells.size();p.status_by_storage[at]=c.status;p.cells.push_back(c);
    }
    std::set<std::size_t> grouped;
    if(overlap) {
        maps::PlacedFootprint f;f.id=0;f.asset_index=1;f.origin={98,109};f.width_cells=2;f.height_cells=2;
        f.image_origin={-519,2960};f.status=maps::StoredStatus::DecodePending;
        for(const auto cell:std::array<maps::GridCell,4>{{{98,109},{99,109},{98,110},{99,110}}}) {
            const auto n=*p.cell_by_storage[raw(cell)];f.cell_indices.push_back(n);grouped.insert(n);p.cells[n].asset_index=1;p.cells[n].footprint_index=0;
        }
        p.footprints.push_back(f);
    }
    for(std::size_t i=0;i<p.cells.size();++i) {
        if(grouped.contains(i)) continue;auto& c=p.cells[i];maps::PlacedFootprint f;f.id=p.footprints.size();
        f.asset_index=0;f.origin=c.storage;f.cell_indices={i};f.image_origin=c.image_origin;f.status=c.status;
        c.footprint_index=f.id;p.footprints.push_back(f);
    }
    p.original_ordinary_gates=std::make_shared<maps::OrdinaryGateMapPresentation>(input.prepare());return p;
}
void frame(openemperor::StoredGraphicsRenderer& v,SDL_Renderer* r,const scene::Camera2D& c) {
    check(SDL_SetRenderDrawColor(r,0,0,0,255) && SDL_RenderClear(r) && v.render(c,std::nullopt),"production GateHouse render");
}
Color pixel(SDL_Renderer* r,scene::Point p) {
    auto* s=SDL_RenderReadPixels(r,nullptr);check(s,"authored pixel readback");Color c{};
    const auto ok=SDL_ReadSurfacePixel(s,int(p.x),int(p.y),&c[0],&c[1],&c[2],&c[3]);SDL_DestroySurface(s);check(ok,"pixel within fixture viewport");return c;
}
nlohmann::json report(const maps::StoredGraphicsPlan& p) {return maps::landscape_fidelity_report(p).at("regeneration").at("ordinary_gate_rendering");}
void stats(const maps::StoredGraphicsPlan& p,unsigned selected,unsigned prepared,unsigned active,unsigned sources=1) {
    const auto j=report(p);check(j.at("source_gate_objects")==sources && j.at("selected_gate_objects")==selected &&
        j.at("prepared_components")==prepared && j.at("active_complete_gate_objects")==active &&
        j.at("active_components")==15*active && j.at("active_cells")==15*active &&
        j.at("full_original_composition_verified")==0 && j.at("tower_bodies_reproduced")==0,"report keeps complete gates and original parity separate");
}
void preparation() {
    for(unsigned layout=0;layout<2;++layout) {
        const Input input(layout);const auto p=input.prepare();const auto& g=p.gates.front();const auto& expected=layout ? rows1:rows0;
        check(p.error.empty() && p.manager_records==1 && g.origin==origin && g.width==(layout ? 3U:5U) &&
            g.height==(layout ? 5U:3U) && g.components.size()==15 && g.source.footprint_side==1,"GateHouse layout establishes fifteen cells independently of raw base footprint_side");
        std::set<std::size_t> claims;
        for(unsigned row=0;row<15;++row) {
            const auto cell=add(origin,expected[row]);const auto& s=g.components[row];
            check(s.origin==cell && s.draw_cell==cell && s.owned_cells==std::vector{cell} && s.side==1 &&
                s.selection.family==maps::LandscapeFamily::OrdinaryGate && s.selection.group.value==0x4af &&
                s.selection.variant==row+15*layout && s.explicit_height && s.explicit_height->cell==cell &&
                !s.explicit_anchor && !s.depth_cell && !s.original_entity_index && claims.insert(raw(cell)).second &&
                p.gate_by_storage[raw(cell)]==0,"literal EXE row expectations preserve orientation and singleton ownership");
        }
        for(unsigned failure=0;failure<7;++failure) {
            auto bad=input;
            if(failure==0) ++bad.entities.records[0].serialized_cell_reference;
            if(failure==1) bad.terrain[raw(add(origin,expected[14]))]&=~8U;
            if(failure==2) bad.terrain[raw(add(origin,expected[14]))]|=0x4000;
            if(failure==3) bad.entities.records[0].type=131;
            if(failure==4) bad.entities.records[0].gate_house->layout=2;
            if(failure==5) bad.entities.records[0].local_x=-1;
            if(failure==6) bad.entities.records[0].gate_house.reset();
            const auto rejected=bad.prepare();check(rejected.gates.size()==1 && rejected.gates[0].components.empty() &&
                !rejected.gates[0].fallback.empty() && std::none_of(rejected.gate_by_storage.begin(),rejected.gate_by_storage.end(),
                    [](const auto& x){return bool(x);}),"any raw claim/state failure publishes zero partial gate claims");
        }
        auto inactive=input;inactive.entities.records[0].status=0;check(inactive.prepare().gates.empty(),"inactive GateHouse has no presentation claims");
        auto wrong_class=input;wrong_class.entities.records[0].entity_class=maps::OriginalEntityClass::Tower;
        check(wrong_class.prepare().gates.empty(),"Tower traversal is not GateHouse body authority");
        const Input conflict(layout,true,true);const auto p2=conflict.prepare();
        check(p2.gates.size()==2 && p2.gates[0].components.size()==15 && p2.gates[1].components.empty() && !p2.gates[1].fallback.empty(),
            "overlapping source gates cannot publish competing partial ownership");
    }
    Input bad(0);bad.terrain.pop_back();check(!bad.prepare().error.empty(),"incomplete global terrain rejects preparation");
    bad=Input(0);bad.entities.declared_map_size=169;check(!bad.prepare().error.empty(),"unsupported map geometry rejects preparation");
}
void positive(SDL_Renderer* renderer,unsigned layout) {
    Temp temp;const auto r=registrations(temp);Input input(layout);auto p=historical(temp,r,input);const auto before=p;
    maps::build_regenerated_map_render_plan(p,r);check(p.regenerated->instances.size()==15 && p.assets.size()==16,"complete metadata preflight publishes fifteen deduplicated component assets");
    for(unsigned row=0;row<15;++row) {
        const auto& i=p.regenerated->instances[row];check(i.graphic.value==0x8000+row+15*layout &&
            p.assets[i.asset_index].record.id.image_index==1+row+15*layout && i.composition_group==4011 &&
            !i.great_wall_context && i.composition_policy==maps::LandscapeCompositionPolicy::EarlyBaseSpatialOverlay,
            "group4af physical translation, disjoint composition domain and split policy are explicit");
    }
    check(p.raw_terrain==before.raw_terrain && p.raw_objects==before.raw_objects && p.raw_saved_ids==before.raw_saved_ids &&
        p.raw_candidate_bytes==before.raw_candidate_bytes && p.height_bytes==before.height_bytes && p.status_by_storage==before.status_by_storage,
        "gate presentation does not mutate source layers or buildability");
    auto other=before;other.raw_saved_ids.assign(count,123);other.raw_objects.assign(count,0);other.raw_candidate_bytes.assign(count,0);
    other.variation_bytes.assign(count,0);other.height_bytes.assign(count,255);for(auto& c:other.cells) {c.stored_id=123;c.objects_raw=0;}
    maps::build_regenerated_map_render_plan(other,r);
    for(unsigned row=0;row<15;++row) check(other.regenerated->instances[row].graphic.value==p.regenerated->instances[row].graphic.value &&
        other.regenerated->instances[row].geometry.owned_cells==p.regenerated->instances[row].geometry.owned_cells,"saved graphics, objects, variation and height cannot choose gate component identity");
    const auto cached=p.regenerated;maps::build_regenerated_map_render_plan(p,r);check(p.regenerated==cached && p.assets.size()==16,"GateHouse preparation is immutable");
    openemperor::StoredGraphicsRenderer v{std::move(p)};v.initialize(renderer);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
    stats(v.plan(),1,15,1);check(v.upload_count()==48,"each physical historical/component image eagerly uploads exactly three textures");
    scene::Camera2D camera;camera.viewport_width=1100;camera.viewport_height=800;
    const auto& expected=layout ? rows1:rows0;
    for(double zoom:{1.,2.,4.}) {
        camera.zoom=zoom;camera.center_on({layout ? -439.5:-359.5,3005.5});frame(v,renderer,camera);
        check(v.last_texture_draws()==30,"fifteen split component pairs draw once without any historical double draw");
        for(unsigned row=0;row<15;++row) {const auto cell=add(origin,expected[row]);const auto sample=camera.world_to_screen(body(cell));
            check(pixel(renderer,sample)==variant_color(row+15*layout) && v.hit_test(sample,camera)==cell,"each authored marker and alpha hit follows its independent layout row");}
        const auto o=top(origin);check(!v.hit_test(camera.world_to_screen({o.x+30.5,o.y+5.5}),camera),"transparent gate-component rectangle remains click-through");
    }
    camera.zoom=1;camera.center_on({-359.5,3005.5});
    const auto f1=maps::landscape_provenance(v.plan(),origin,true,&camera);const auto& gate=f1.at("ordinary_gate");
    check(gate.at("original_id")==7 && gate.at("manager_index")==11 && gate.at("original_class")=="cGateHouse" &&
        gate.at("layout_raw")==layout && gate.at("selected_components")==15 && gate.at("active_components")==15 &&
        gate.at("preparation_status")=="active" && gate.at("layout_source").at("logical_offset")==10113 &&
        gate.at("layout_source").at("record_relative_offset")==129 && gate.at("layout_source").at("signed")==true,
        "typed F1 original identity/layout/source and whole-gate readiness remain distinct");
    v.set_landscape_mode(openemperor::LandscapeDebugMode::Ground);frame(v,renderer,camera);
    check(!v.hit_test(camera.world_to_screen(body(origin)),camera),"F8 Ground hides GateHouse overlays and their alpha hits");
    v.set_landscape_mode(openemperor::LandscapeDebugMode::WallsMonuments);frame(v,renderer,camera);
    check(v.hit_test(camera.world_to_screen(body(origin)),camera)==origin,"F8 Walls/Monuments includes the ordinary gate family");
    v.set_landscape_mode(openemperor::LandscapeDebugMode::Snapshot);frame(v,renderer,camera);
    check(v.last_texture_draws()==15 && pixel(renderer,camera.world_to_screen(body(origin)))==red,"Snapshot retains the fifteen old images only");
    v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);frame(v,renderer,camera);
    const auto hit=v.hit_test_item(camera.world_to_screen(body(origin)),camera);check(bool(hit),"stored alpha supplies gate draw key");
    check(hit->key.depth==3020 && hit->key.ground_x==-400,"gate component painter uses the existing logical ground key");
    for(bool front:{false,true}) {
        auto key=hit->key;key.depth+=front ? 1:-1;key.layer=scene::WorldVisualLayer::SandboxBuilding;
        struct Dynamic {scene::WorldDrawKey key;};const std::array<Dynamic,1> dynamic{{{key}}};
        check(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer),"clear painter oracle");v.begin_frame();
        for(std::size_t i=0;i<v.draw_items().size();++i) check(v.draw_ground_item(i,camera),"ordinary split ground pass");
        scene::WorldMergeStats merge;const auto sample=camera.world_to_screen(body(origin));
        check(scene::merge_world_draw_streams(v.draw_items(),dynamic,[&](std::size_t i){return v.draw_item(i,camera);},[&](std::size_t){
            const SDL_FRect box{float(sample.x),float(sample.y),1,1};return SDL_SetRenderDrawColor(renderer,0,255,0,255) && SDL_RenderFillRect(renderer,&box);},merge),"merge gate components with an independent dynamic item");
        check(pixel(renderer,sample)==(front ? green:variant_color(layout ? 25:0)),"a rear object yields to gate alpha and a front object can occlude it");
    }
    perf::set_enabled(true);perf::reset();for(unsigned frame_index=0;frame_index<12;++frame_index) {frame(v,renderer,camera);
        (void)v.hit_test(camera.world_to_screen(body(origin)),camera);(void)maps::landscape_provenance(v.plan(),origin,true,&camera);(void)report(v.plan());}
    for(const auto counter:{perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::BfsCalls,perf::Counter::RouteRefreshes})
        check(perf::counter(counter)==0,"gate render/F1/picking makes no asset, World or route work");
    perf::set_enabled(false);
}
void failures(SDL_Renderer* renderer) {
    for(unsigned failure=0;failure<5;++failure) {
        Temp temp{failure==3 ? 7:-1};auto r=registrations(temp);const Input input(0);auto p=historical(temp,r,input);
        if(failure==0) r.clear();if(failure==1) r.at(2).layout->runtime_image_count=14;
        if(failure==2) r.at(2).catalog->records[8].width=158;if(failure==4) p.cells[7].slot=16;
        maps::build_regenerated_map_render_plan(p,r);check(p.regenerated->instances.size()==(failure==3 ? 15U:0U) && p.assets.size()==(failure==3 ? 16U:1U),
            "one missing resource/geometry/claim preflight cannot publish the other fourteen components");
        openemperor::StoredGraphicsRenderer v{std::move(p)};v.initialize(renderer);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        stats(v.plan(),1,failure==3 ? 15:0,0);scene::Camera2D c;c.viewport_width=1100;c.viewport_height=800;c.center_on({-359.5,3005.5});frame(v,renderer,c);
        check(v.last_texture_draws()==30 && pixel(renderer,c.world_to_screen(body(origin)))==red,"unavailable whole gate retains every complete historical base/overlay pair");
        const auto f1=maps::landscape_provenance(v.plan(),origin,true);check(f1.at("ordinary_gate").at("preparation_status")==
            (failure==3 ? "decode_failed":"complete_preparation_unavailable") && f1.at("render_source")=="historical saved-ID preview",
            "F1 distinguishes decode failure from preflight refusal without claiming active components");
        if(failure==3) check(std::all_of(v.plan().regenerated_instance_active.begin(),v.plan().regenerated_instance_active.end(),
            [](auto active){return !active;}),"one failed decode disables all fifteen siblings");
    }
    Temp temp;const auto r=registrations(temp);const Input input(0);auto budget=historical(temp,r,input);
    while(budget.assets.size()<2034) {openemperor::assets::AssetRecord record;record.id={"DATA/budget-"+std::to_string(budget.assets.size())+".sg3",0};
        budget.assets.push_back({record,maps::StoredStatus::UnsupportedLayout,false,false,{}});}
    maps::build_regenerated_map_render_plan(budget,r);check(budget.assets.size()==2034 && budget.regenerated->instances.empty() &&
        budget.regenerated->ordinary_gate_preparation.at(0).fallback.find("budget")!=std::string::npos,"aggregate physical budget refuses all fifteen before asset publication");
    auto bad=input;++bad.entities.records[0].serialized_cell_reference;auto refused=historical(temp,r,bad);
    maps::build_regenerated_map_render_plan(refused,r);const auto source_status=report(refused).at("source_gate_statuses").at(0);
    check(refused.regenerated->instances.empty() && source_status.at("preparation_status")=="complete_claim_unavailable" &&
        !source_status.at("fallback").get<std::string>().empty() && !maps::landscape_provenance(refused,origin,true).contains("ordinary_gate"),
        "report names invalid source claims while F1 never grants invalid cells an object claim");
    auto overlap=historical(temp,r,input,true);maps::build_regenerated_map_render_plan(overlap,r);
    openemperor::StoredGraphicsRenderer v{std::move(overlap)};v.initialize(renderer);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);stats(v.plan(),1,15,0);
    check(std::all_of(v.plan().regenerated_instance_active.begin(),v.plan().regenerated_instance_active.end(),[](auto x){return !x;}) &&
        maps::landscape_provenance(v.plan(),origin,true).at("ordinary_gate").at("preparation_status")=="atomic_activation_unavailable",
        "partial overlap with one complete old 2x2 footprint disables the entire gate composition");
}
void sharing_and_height(SDL_Renderer* renderer) {
    Temp temp;const auto r=registrations(temp);const Input both(1,true);auto p=historical(temp,r,both);maps::build_regenerated_map_render_plan(p,r);
    check(p.regenerated->instances.size()==30 && p.assets.size()==16,"two complete gates deduplicate fifteen physical records");
    for(unsigned row=0;row<15;++row) check(p.regenerated->instances[row].asset_index==p.regenerated->instances[row+15].asset_index &&
        p.regenerated->instances[row].composition_group==4011 && p.regenerated->instances[row+15].composition_group==4012,
        "physical sharing leaves independent per-source atomic composition keys");
    openemperor::StoredGraphicsRenderer shared{std::move(p)};shared.initialize(renderer);stats(shared.plan(),2,30,2,2);check(shared.upload_count()==48,"shared gates upload each physical component only once");
    // The front marker remains visible at both signed heights. A lowered rear
    // marker can legitimately be occluded by a different front component.
    const auto marker=add(origin,rows0[14]);
    for(int height:{-1,2}) {const Input input(0);auto raised=historical(temp,r,input);raised.height_bytes[raw(marker)]=std::uint8_t(height);
        maps::build_regenerated_map_render_plan(raised,r);openemperor::StoredGraphicsRenderer v{std::move(raised)};v.initialize(renderer);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        scene::Camera2D c;c.viewport_width=1100;c.viewport_height=800;c.center_on(body(marker,height));frame(v,renderer,c);
        const auto hit=v.hit_test_item(c.world_to_screen(body(marker,height)),c);
        check(pixel(renderer,c.world_to_screen(body(marker,height)))==variant_color(14) && hit && hit->cell==marker && hit->key.depth==3140,
            "signed per-component height times forty affects render/alpha once and leaves logical painter depth unchanged");
    }
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        preparation();check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"initialize isolated software test");
        window=SDL_CreateWindow("authored ordinary GateHouse",1100,800,SDL_WINDOW_HIDDEN);check(window,"create fixture window");
        renderer=SDL_CreateRenderer(window,"software");check(renderer && std::string(SDL_GetRendererName(renderer))=="software","verify software renderer");
        const auto before=openemperor::StoredGraphicsRenderer::live_texture_count();positive(renderer,0);positive(renderer,1);failures(renderer);sharing_and_height(renderer);
        check(openemperor::StoredGraphicsRenderer::live_texture_count()==before,"all component textures shut down without leaks");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"Ordinary GateHouse presentation checks passed\n";return 0;
    } catch(const std::exception& error) {perf::set_enabled(false);if(renderer) SDL_DestroyRenderer(renderer);if(window) SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<error.what()<<'\n';return 1;}
}
