#include "app/MapDebugView.h"
#include "app/SandboxView.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "renderer/WalkerSpriteSet.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
namespace maps=openemperor::maps;
namespace scene=openemperor::scene;
namespace sim=openemperor::simulation;
using Bytes=std::vector<std::uint8_t>;
using Color=std::array<std::uint8_t,4>;
constexpr Color red{255,0,0,255},blue{0,0,255,255},green{0,255,0,255};
constexpr scene::Point body{0.5,1668.5},roof{0.5,1542.5};
void check(bool b,const char* message) { if (!b) throw std::runtime_error(message); }
void u16(Bytes& b,std::size_t p,unsigned n) {b.at(p)=std::uint8_t(n);b.at(p+1)=std::uint8_t(n>>8);}
void u32(Bytes& b,std::size_t p,unsigned n) {u16(b,p,n);u16(b,p+2,n>>16);}
void write(const std::filesystem::path& p,const Bytes& b) {
    std::ofstream out(p,std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));
    check(bool(out),"write authored fixture");
}
struct Temp {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("openemperor-composition-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {std::filesystem::create_directories(root/"DATA");std::filesystem::create_directories(root/"Cities");
        std::ofstream(root/"Cities/Synthetic.map")<<"authored identity";}
    ~Temp() {std::error_code e;std::filesystem::remove_all(root,e);}
};
void skip(Bytes& b,unsigned n) {while(n) {const auto k=std::min(n,254U);b.push_back(255);b.push_back(std::uint8_t(k));n-=k;}}
Bytes omega(unsigned width,unsigned height,unsigned color) {
    Bytes out;
    for(unsigned y=0;y<height;++y) {out.push_back(std::uint8_t(width));for(unsigned x=0;x<width;++x) {
        out.push_back(std::uint8_t(color));out.push_back(std::uint8_t(color>>8));}}
    return out;
}
Bytes patch(unsigned width,unsigned x,unsigned y,unsigned color,unsigned extent=7) {
    Bytes out;skip(out,y*width+x);
    for(unsigned r=0;r<extent;++r) {out.push_back(std::uint8_t(extent));for(unsigned c=0;c<extent;++c) {
        out.push_back(std::uint8_t(color));out.push_back(std::uint8_t(color>>8));}
        if(r+1<extent) skip(out,width-extent);}
    return out;
}
struct Image {unsigned width,height,side;Bytes bytes;};
void archive(const Temp& t,const std::vector<Image>& images) {
    Bytes sg3(40680+(images.size()+1)*72,0),bitmap(4,0);
    u32(sg3,0,unsigned(sg3.size()));u32(sg3,4,214);u32(sg3,12,unsigned(images.size()+1));
    u32(sg3,16,unsigned(images.size()+1));u32(sg3,20,1);
    const std::string name="authored.bmp";std::copy(name.begin(),name.end(),sg3.begin()+680);
    u32(sg3,804,unsigned(images.size()+1));
    for(std::size_t i=0;i<images.size();++i) {
        const auto& v=images[i];const auto p=40680+(i+1)*72;
        u32(sg3,p,unsigned(bitmap.size()));u32(sg3,p+4,unsigned(v.bytes.size()));
        u32(sg3,p+8,v.side ? 3200*v.side*v.side:0);
        u16(sg3,p+20,v.width);u16(sg3,p+22,v.height);u16(sg3,p+50,v.side ? 30:256);
        sg3[p+55]=std::uint8_t(v.side);bitmap.insert(bitmap.end(),v.bytes.begin(),v.bytes.end());
    }
    write(t.root/"DATA/authored.sg3",sg3);write(t.root/"DATA/authored.555",bitmap);
}
Image tile(unsigned side,unsigned height,unsigned color,Bytes overlay={}) {
    Bytes b(3200*side*side);
    for(std::size_t p=0;p<b.size();p+=2) u16(b,p,color);
    b.insert(b.end(),overlay.begin(),overlay.end());return {80*side-2,height,side,std::move(b)};
}
Image wall_with_hole() {
    Bytes b;
    // Authored tile encoder with explicit four-cell tile origins. These are
    // fixture-format coordinates, never queried from the composition code.
    const std::array<std::pair<unsigned,unsigned>,16> origins{{{120,78},{80,98},{160,98},
        {40,118},{120,118},{200,118},{0,138},{80,138},{160,138},{240,138},
        {40,158},{120,158},{200,158},{80,178},{160,178},{120,198}}};
    for(auto [ox,oy]:origins) for(unsigned y=0;y<40;++y) {
        const unsigned start=y<20 ? 38-2*y:2*y-40;
        for(unsigned x=start;x<78-start;++x) {
            const bool hole=x+ox>=156 && x+ox<=162 && y+oy>=143 && y+oy<=149;
            const unsigned color=hole ? 0xf81f:0x7c00;
            b.push_back(std::uint8_t(color));b.push_back(std::uint8_t(color>>8));
        }
    }
    const auto overlay=patch(318,156,20,0x03e0);b.insert(b.end(),overlay.begin(),overlay.end());
    return {318,238,4,std::move(b)};
}
void images(const Temp& t) {
    archive(t,{tile(1,40,0x03ff),tile(4,238,0x7c00,patch(318,156,20,0x03e0)),
        tile(1,238,0xf81f,patch(78,36,224,0x001f)),
        tile(1,238,0xf81f,patch(78,36,24,0x001f)),
        tile(1,238,0x7c1f,omega(78,238,0x7c1f)),
        {78,238,0,omega(78,238,0x03e0)},tile(2,238,0x7c00),
        tile(5,400,0xf81f,patch(398,196,385,0x001f)),
        tile(5,400,0xf81f,patch(398,196,25,0x001f)),wall_with_hole(),
        {2,1,0,omega(2,1,0x7c00)}});
    // The existing supported Omega shadow marker converts these authored red
    // pixels to black alpha128 once during WalkerVisualProfile loading.
    auto file=t.root/"DATA/authored.sg3";
    {std::fstream sg3(file,std::ios::in|std::ios::out|std::ios::binary);sg3.seekp(40680+11*72+59);sg3.put(1);}
    std::ofstream(t.root/"shadow.json")<<R"({"schema_version":1,"mode":"curated_walker_preview","role":"clay","ticks_per_frame":1,"evidence":"Authored shadow","frames":[{"alias":"shadow","archive":"DATA/authored.sg3","image_index":11,"foot_anchor":[0,0]}],"clips":{"pos_x":["shadow"]},"idle":"shadow"})";
    std::ofstream(t.root/"building.json")<<R"({"schema_version":1,"mode":"curated_building_preview","buildings":{"farm":{"archive":"DATA/authored.sg3","image_index":5,"ground_anchor":[39,218],"evidence":"Authored pixel fixture"}}})";
    std::ofstream(t.root/"walker.json")<<R"({"schema_version":1,"mode":"curated_walker_preview","role":"clay","ticks_per_frame":1,"evidence":"Authored pixel fixture","frames":[{"alias":"green","archive":"DATA/authored.sg3","image_index":6,"foot_anchor":[39,218]}],"clips":{"pos_x":["green"],"neg_x":["green"],"pos_y":["green"],"neg_y":["green"]},"idle":"green"})";
    std::ofstream(t.root/"cargo.json")<<R"({"schema_version":1,"mode":"curated_walker_preview","role":"clay","ticks_per_frame":1,"evidence":"Authored cargo fixture","frames":[{"alias":"tiny","archive":"DATA/authored.sg3","image_index":11,"foot_anchor":[0,0]}],"clips":{"pos_x":["tiny"],"neg_x":["tiny"],"pos_y":["tiny"],"neg_y":["tiny"]},"idle":"tiny"})";
}
maps::StoredMapSession fixture(const Temp& t,int cliff=0,bool reverse=false,
    maps::LandscapeCompositionPolicy policy=maps::LandscapeCompositionPolicy::SpatialCombined,bool hole=false) {
    maps::StoredMapSession s;s.map.declared_map_size=84;
    s.map.terrain_raw.values.assign(228*228,0x80);s.map.objects_raw.values.assign(228*228,0);
    auto& p=s.plan;p.data_root=t.root;p.border=72;p.landscape_layers_available=true;
    p.raw_terrain=s.map.terrain_raw.values;p.raw_objects=s.map.objects_raw.values;
    p.height_bytes.assign(228*228,0);p.height_bytes[117*228+114]=2;
    p.cell_by_storage.resize(228*228);p.status_by_storage.assign(228*228,maps::StoredStatus::Excluded);
    const std::array<unsigned,4> indexes{1,2,3,4};
    for(auto index:indexes) {
        openemperor::assets::AssetRecord r;r.id={"DATA/authored.sg3",hole && index==2 ? 10U:index};
        r.width=index==2 ? 318:78;r.height=index==1 ? 40:238;
        r.isometric_size_flag=index==2 ? 4:1;r.image_type=30;
        r.uncompressed_length=index==2 ? 51200:3200;
        r.data_length=r.uncompressed_length+(index==1 ? 0:unsigned(patch(unsigned(r.width),index==2 ? 156:36,index==2 ? 20:index==3 ? 224:24,index==2 ? 0x03e0:0x001f).size()));
        p.assets.push_back({r,maps::StoredStatus::DecodePending,false,false,{}});
    }
    std::vector<maps::GridCell> cells;
    for(unsigned y=108;y<=123;++y) for(unsigned x=108;x<=123;++x) cells.push_back({x,y});
    if(reverse) std::reverse(cells.begin(),cells.end());
    for(const auto cell:cells) {
        maps::StoredCell c;c.storage=cell;c.cell_index=cell.y*228+cell.x;c.terrain_raw=0x80;
        c.world={(int(cell.x)-int(cell.y))*40.,(int(cell.x)+int(cell.y)-144)*20.};
        c.asset_index=0;c.footprint_index=p.footprints.size();c.status=maps::StoredStatus::DecodePending;
        const bool special=cliff && cell==maps::GridCell{unsigned(cliff<0 ? 113:118),unsigned(cliff<0 ? 113:118)};
        if(special) {c.asset_index=cliff<0 ? 2:3;c.slot=16;}
        c.image_origin={c.world.x-39,c.world.y-(special ? 198:0)};
        maps::PlacedFootprint f;f.id=p.footprints.size();f.asset_index=*c.asset_index;f.origin=cell;
        f.cell_indices={p.cells.size()};f.image_origin=c.image_origin;f.status=maps::StoredStatus::DecodePending;
        p.cell_by_storage[c.cell_index]=p.cells.size();p.status_by_storage[c.cell_index]=c.status;
        p.footprints.push_back(f);p.cells.push_back(c);
    }
    auto g=std::make_shared<maps::RegeneratedMapRenderPlan>();
    g->cells.resize(p.cells.size());g->footprint_assets.resize(p.footprints.size());
    maps::RegeneratedLandscapeInstance wall;
    wall.geometry.selection.family=maps::LandscapeFamily::GreatWall;wall.geometry.selection.evidence=maps::SelectorEvidence::Preview;
    wall.geometry.origin={114,114};wall.geometry.side=4;wall.geometry.draw_cell={114,117};
    wall.geometry.depth_cell=maps::GridCell{117,117};wall.geometry.explicit_anchor=maps::LandscapeInstanceAnchor{{117,117},159,20};
    wall.geometry.explicit_height=maps::LandscapeInstanceHeight{maps::LandscapeInstanceHeightSource::SerializedCellHeight,{114,117}};
    wall.geometry.original_entity_index=0;wall.asset_index=1;wall.composition_group=0;
    wall.great_wall_context=maps::great_wall_context_from_mode(maps::GreatWallPresentationMode::PreviewStone);
    wall.composition_policy=policy;
    for(unsigned y=114;y<=117;++y) for(unsigned x=114;x<=117;++x) {
        wall.geometry.owned_cells.push_back({x,y});const auto n=*p.cell_by_storage[y*228+x];
        wall.cell_indices.push_back(n);g->cells[n].instance_index=0;g->cells[n].asset_index=1;
        g->cells[n].selection=wall.geometry.selection;
    }
    g->instances.push_back(wall);p.regenerated=g;return s;
}
Color pixel(SDL_Renderer* r,scene::Point p) {
    auto* s=SDL_RenderReadPixels(r,nullptr);check(s,"pixel readback");
    Color c{};check(SDL_ReadSurfacePixel(s,int(p.x),int(p.y),&c[0],&c[1],&c[2],&c[3]),"sample bounds");
    SDL_DestroySurface(s);return c;
}
std::uint64_t frame_hash(SDL_Renderer* r) {
    auto* raw=SDL_RenderReadPixels(r,nullptr);check(raw,"full frame readback");
    auto* rgba=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(raw);check(rgba,"normalized frame pixels");
    std::uint64_t hash=1469598103934665603ULL;
    for(int y=0;y<rgba->h;++y) for(int x=0;x<rgba->w*4;++x) {
        hash^=static_cast<const std::uint8_t*>(rgba->pixels)[y*rgba->pitch+x];hash*=1099511628211ULL;
    }
    SDL_DestroySurface(rgba);return hash;
}
scene::Point screen(const scene::Camera2D& c,scene::Point p) {return {p.x*c.zoom+c.offset.x,p.y*c.zoom+c.offset.y};}
SDL_Event key(SDL_Keycode k) {SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=k;return e;}
void click(openemperor::SandboxView& v,scene::Point p,bool& running) {
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=float(p.x);e.button.y=float(p.y);v.handle_event(e,running);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;v.handle_event(e,running);
}
void zoom(openemperor::SandboxView& v,double target,bool& running,scene::Point shift={}) {
    const auto c=v.camera();auto at=screen(c,body);at.x+=shift.x;at.y+=shift.y;
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=float(at.x);e.wheel.mouse_y=float(at.y);
    e.wheel.y=float(std::log(target/c.zoom)/std::log(1.15));v.handle_event(e,running);
}
void renderer_cases(const Temp& t,SDL_Renderer* r) {
    std::map<std::pair<int,int>,std::uint64_t> frames;
    for(int cliff:{-1,1}) for(int z:{1,2,4}) for(bool reverse:{false,true}) {
        auto s=fixture(t,cliff,reverse);openemperor::StoredGraphicsRenderer v(std::move(s.plan));v.initialize(r);
        v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        scene::Camera2D c;c.viewport_width=1100;c.viewport_height=700;c.zoom=z;c.offset={550,600-body.y*z};
        check(SDL_SetRenderDrawColor(r,22,26,32,255)&&SDL_RenderClear(r)&&v.render(c,{}),"structural scene draw");
        check(pixel(r,screen(c,body))==(cliff<0 ? red:blue),"front/back cliff pixel oracle");
        check(pixel(r,screen(c,roof))==green,"structural Omega roof retains its independent authored pixel");
        const auto hash=frame_hash(r);
        if(reverse) check(frames.at({cliff,z})==hash,"entire composed frame is deterministic under reversed candidate insertion");
        else frames[{cliff,z}]=hash;
        const auto hit=v.hit_test(screen(c,body),c);
        check(hit==(cliff<0 ? maps::GridCell{114,117}:maps::GridCell{118,118}),"visible body and foreground cliff picking");
        const auto uploads=v.upload_count();v.set_landscape_mode(openemperor::LandscapeDebugMode::Ground);
        check(SDL_SetRenderDrawColor(r,22,26,32,255)&&SDL_RenderClear(r)&&v.render(c,{}),"hidden body draw");
        check(pixel(r,screen(c,body))!=red&&!v.hit_test(screen(c,body),c),"hidden layer leaves no wall base or hit");
        check(v.upload_count()==uploads,"layer toggle uploads nothing");
    }
    auto old=fixture(t,-1,false,maps::LandscapeCompositionPolicy::EarlyBaseSpatialOverlay);
    openemperor::StoredGraphicsRenderer v(std::move(old.plan));v.initialize(r);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
    scene::Camera2D c;c.viewport_width=1100;c.viewport_height=700;c.offset={550,350-body.y};
    check(SDL_SetRenderDrawColor(r,0,0,0,255)&&SDL_RenderClear(r)&&v.render(c,{}),"old split scene");
    check(pixel(r,screen(c,body))==blue,"fixture independently reproduces pre-fix base overwrite");
}
void append(const Temp& t,maps::StoredGraphicsPlan& p,unsigned physical,maps::GridCell origin,
    unsigned side,maps::LandscapeFamily family) {
    const auto catalog=openemperor::assets::scan_asset_archive(t.root,"DATA/authored.sg3");
    const auto asset=p.assets.size();p.assets.push_back({catalog.records.at(physical),maps::StoredStatus::DecodePending,false,false,{}});
    auto g=std::make_shared<maps::RegeneratedMapRenderPlan>(*p.regenerated);
    maps::RegeneratedLandscapeInstance i;i.asset_index=asset;i.geometry.origin=origin;i.geometry.side=side;
    i.geometry.draw_cell=origin;i.geometry.selection.family=family;i.geometry.selection.evidence=maps::SelectorEvidence::Preview;
    if(family==maps::LandscapeFamily::GreatWall) {
        const maps::GridCell marker{origin.x,origin.y+side-1},front{origin.x+side-1,origin.y+side-1};
        p.height_bytes[marker.y*228+marker.x]=2;i.geometry.draw_cell=marker;i.geometry.depth_cell=front;
        i.geometry.explicit_height=maps::LandscapeInstanceHeight{maps::LandscapeInstanceHeightSource::SerializedCellHeight,marker};
        i.geometry.explicit_anchor=maps::LandscapeInstanceAnchor{front,int(40*side-1),20};
        i.geometry.original_entity_index=g->instances.size();i.composition_group=g->instances.size();
        i.great_wall_context=maps::great_wall_context_from_mode(maps::GreatWallPresentationMode::PreviewStone);
        i.composition_policy=maps::LandscapeCompositionPolicy::SpatialCombined;
    }
    for(unsigned y=0;y<side;++y) for(unsigned x=0;x<side;++x) {
        const maps::GridCell q{origin.x+x,origin.y+y};const auto n=*p.cell_by_storage[q.y*228+q.x];
        check(!g->cells[n].instance_index,"authored complete claims never overlap");
        i.geometry.owned_cells.push_back(q);i.cell_indices.push_back(n);
        g->cells[n].instance_index=g->instances.size();g->cells[n].asset_index=asset;g->cells[n].selection=i.geometry.selection;
    }
    g->instances.push_back(i);p.regenerated=g;
}
void additional_pixels(const Temp& t,SDL_Renderer* r) {
    scene::Camera2D c;c.viewport_width=1100;c.viewport_height=700;c.offset={550,350-body.y};
    const auto clear=[&] {check(SDL_SetRenderDrawColor(r,22,26,32,255)&&SDL_RenderClear(r),"clear extra scene");};
    {
        auto s=fixture(t,-1,false,maps::LandscapeCompositionPolicy::SpatialCombined,true);
        openemperor::StoredGraphicsRenderer v(std::move(s.plan));v.initialize(r);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        clear();check(v.render(c,{})&&pixel(r,screen(c,body))==blue,"authored transparent Base hole reveals rear cliff");
        check(v.hit_test(screen(c,body),c)==maps::GridCell{113,113},"Combined alpha hole lets rear stored hit through");
        const scene::Point empty{-158,1523};
        check(!v.hit_test(screen(c,empty),c),"transparent full image rectangle never claims an object hit");
    }
    {
        auto s=fixture(t);append(t,s.plan,2,{118,114},4,maps::LandscapeFamily::GreatWall);
        append(t,s.plan,7,{122,114},2,maps::LandscapeFamily::GreatWall);
        openemperor::StoredGraphicsRenderer v(std::move(s.plan));v.initialize(r);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        clear();check(v.render(c,{})&&pixel(r,screen(c,body))==red&&
            pixel(r,screen(c,{160.5,1748.5}))==red&&pixel(r,screen(c,{320.5,1800.5}))==red,
            "adjacent wall/tower/gate-sized bodies retain authored pixels and complete claims");
        std::size_t count=0;v.begin_frame();
        for(std::size_t n=0;n<v.draw_items().size();++n) if(v.draw_items()[n].regenerated) {
            check(v.draw_ground_item(n,c)&&v.last_texture_draws()==count,"structural pieces submit no early Base");
            check(v.draw_item(n,c)&&v.last_texture_draws()==++count,"one Combined draw per complete structural piece");
        }
        check(count==3,"adjacent complete pieces are three images rather than claimed-cell draws");
    }
    for(bool front:{false,true}) {
        auto s=fixture(t);append(t,s.plan,front ? 9:8,front ? maps::GridCell{118,118}:maps::GridCell{109,109},
            5,maps::LandscapeFamily::Mountain);
        openemperor::StoredGraphicsRenderer v(std::move(s.plan));v.initialize(r);v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        clear();check(v.render(c,{})&&pixel(r,screen(c,body))==(front ? blue:red),
            "unchanged split Pinnacle overlay follows ordinary front/back painter depth");
    }
    auto s=fixture(t);openemperor::StoredGraphicsRenderer v(std::move(s.plan));v.initialize(r);
    v.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
    const auto item=std::find_if(v.draw_items().begin(),v.draw_items().end(),[](const auto& i){return i.regenerated;});
    const auto n=std::size_t(item-v.draw_items().begin());
    for(int z:{1,2,4}) for(double shift:{0.,.25}) for(double x:{1.,1098.}) {
        c.zoom=z;c.offset={x-body.x*z+shift,25-body.y*z+shift};clear();v.begin_frame();
        check(v.draw_ground_item(n,c)&&v.last_texture_draws()==0,"body has no duplicated early component");
        check(v.draw_item(n,c)&&v.last_texture_draws()==1&&pixel(r,screen(c,body))==red,
            "body-only viewport edge remains visible at integer/fractional cameras");
        check(v.hit_test(screen(c,body),c)==maps::GridCell{114,117},"body culling and alpha origin agree");
    }
    for(bool hole:{false,true}) {
        auto structural=fixture(t,0,false,maps::LandscapeCompositionPolicy::SpatialCombined,hole);
        openemperor::StoredGraphicsRenderer h(std::move(structural.plan));h.initialize(r);
        h.set_landscape_mode(openemperor::LandscapeDebugMode::Regenerated);
        const auto structural_item=std::find_if(h.draw_items().begin(),h.draw_items().end(),[](const auto& i){return i.regenerated;});
        const auto index=std::size_t(structural_item-h.draw_items().begin());
        for(int z:{1,2,4,1}) for(double pan:{0.,8.,-8.}) {
            c.zoom=z;c.offset={550+pan+.25-body.x*z,350+.25-body.y*z};
            check(SDL_SetRenderDrawColor(r,0,0,255,255)&&SDL_RenderClear(r),"known structural backdrop");
            h.begin_frame();check(h.draw_ground_item(index,c)&&h.last_texture_draws()==0,"sequential wall has no early Base");
            check(h.draw_item(index,c)&&h.last_texture_draws()==1,"sequential wall submits one complete image");
            check(pixel(r,screen(c,body))==(hole ? blue:red),"sequential zoom/pan/fractional camera retains exact opaque and transparent body pixels");
            check(bool(h.hit_test(screen(c,body),c))==!hole,"sequential camera alpha hit agrees with actual structural pixels");
        }
    }
    c.zoom=1;c.offset={550-body.x,350-body.y};
    const auto profile=openemperor::assets::load_walker_visual_profile(t.root,t.root/"shadow.json");
    check(profile.unique_images[0].pixels[3]==128,"supported Omega shadow retains alpha128");
    openemperor::WalkerSpriteSet pawn;pawn.initialize(r,profile);
    const auto& role=*profile.find(openemperor::assets::WalkerVisualRole::Clay);
    struct Pawn {scene::WorldDrawKey key;};
    for(bool front:{false,true}) {
        const std::array<Pawn,1> dynamic{{{{front ? 1840.:1800.,0,scene::WorldVisualLayer::SandboxWalker,1}}}};
        clear();v.begin_frame();scene::WorldMergeStats stats;
        check(scene::merge_world_draw_streams(v.draw_items(),dynamic,
            [&](std::size_t index){return v.draw_item(index,c);},
            [&](std::size_t){return pawn.draw(0,screen(c,body),1,role,profile,{0,0},{1100,700});},stats),
            "supported alpha pawn merged through production draw path");
        check(pixel(r,screen(c,body))==(front ? Color{127,0,0,255}:red),
            "single alpha128 shadow blends exactly once rather than twice");
    }
}
void map_debug_case(const Temp& t,SDL_Window* w,SDL_Renderer* r) {
    auto s=fixture(t,-1);openemperor::MapDebugView v(std::move(s.map),maps::RawLayer::Terrain,maps::MapViewMode::StoredGraphics,{},std::move(s.plan));
    v.initialize(w,r);
    // Independently fixed fixture bounds: x[-639,639], y[1440,2080].
    scene::Camera2D c;c.viewport_width=1100;c.viewport_height=700;
    c.zoom=std::min(1060./1278,590./640);c.offset={550,350-1760*c.zoom+43};
    bool running=true;
    for(int z:{1,2,4}) {
        const auto at=screen(c,body);const auto factor=z/c.zoom;
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=float(at.x);e.wheel.mouse_y=float(at.y);
        e.wheel.y=float(std::log(factor)/std::log(1.15));v.handle_event(e,running);
        c.offset={at.x-(at.x-c.offset.x)*factor,at.y-(at.y-c.offset.y)*factor};c.zoom=z;
        check(v.render()&&pixel(r,screen(c,body))==red,"actual MapDebug structural base pixel");
        e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=float(at.x);e.button.y=float(at.y);
        v.handle_event(e,running);check(v.selected_cell()==maps::GridCell{114,117},"actual MapDebug body inspection");
    }
    v.shutdown();
}
void sandbox_cases(const Temp& t,SDL_Window* w,SDL_Renderer* r) {
    for(bool hole:{false,true}) for(int direction:{-1,1}) {
        auto s=fixture(t,0,false,maps::LandscapeCompositionPolicy::SpatialCombined,hole);
        openemperor::SandboxView v(std::move(s),false,sim::RulesProfile::CityV10);
        v.configure_save(t.root,"Cities/Synthetic.map",{});v.set_building_visuals(t.root/"building.json");v.initialize(w,r);
        const sim::Cell farm{direction<0 ? 113:118,direction<0 ? 113:118};
        check(v.execute({sim::CommandType::PlaceFarm,farm}).accepted,"front/back farm placement");
        bool running=true;v.set_tool(5);if(!v.paused()) v.handle_event(key(SDLK_SPACE),running);
        const auto before=v.world().snapshot();
        for(int z:{1,2,4,1}) {
            zoom(v,z,running);check(v.render(),"actual Sandbox draw");
            auto p=screen(v.camera(),body);
            const auto actual=pixel(r,p);
            check(actual==((direction>0||hole) ? Color{255,0,255,255}:red),"actual Sandbox front/back building and transparent Base pixel oracle");
            for(int repeat=0;repeat<3;++repeat) {
                check(v.render()&&pixel(r,p)==actual,"repeated structural composition retains its submitted texture");
            }
            v.handle_event(key(SDLK_F1),running);check(v.render(),"diagnostic draw");
            click(v,p,running);
            check(bool(v.selected_building())==(direction>0||hole),"visible wall beats behind occupancy; alpha hole and foreground building win");
            if(direction<0 && !hole) {
                v.handle_event(key(SDLK_F7),running);check(v.render(),"legacy actual-frame order");
                check(pixel(r,p)==Color{255,0,255,255},"legacy painter visibly places building after wall");
                click(v,p,running);check(bool(v.selected_building()),"legacy submitted-frame selection follows actual painter");
                v.handle_event(key(SDLK_F7),running);
                click(v,p,running);check(bool(v.selected_building()),"F7-invalidated same-frame inspection waits for new painter submission");
                check(v.render(),"restore unified actual-frame order");
                click(v,p,running);check(!v.selected_building(),"unified picking restores wall body precedence");
            }
            if(direction>0 && !hole) {
                v.handle_event(key(SDLK_F4),running);
                click(v,p,running);check(bool(v.selected_building()),"profile-invalidated same-frame inspection preserves visible foreground selection");
                v.handle_event(key(SDLK_F4),running);check(v.render(),"profile toggle return draw");
                v.handle_event(key(SDLK_F1),running);v.handle_event(key(SDLK_F1),running);
                click(v,p,running);check(bool(v.selected_building()),"F1-invalidated same-frame inspection waits for displayed dynamic cache");
                check(v.render(),"diagnostic toggle return draw");
                if(z==4) {
                    const auto selected=v.selected_building();const auto state=v.world().snapshot();
                    // This authored opaque Base pixel extends behind the UI at
                    // 4x; only its visible map portion may receive inspection.
                    const auto covered=screen(v.camera(),{108.5,1668.5});
                    check(v.layout().panel.contains(covered.x,covered.y)&&!v.pick(covered),"oracle pixel is actually covered by panel UI");
                    SDL_Event release{};release.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
                    release.button.button=SDL_BUTTON_LEFT;release.button.x=float(p.x);release.button.y=float(p.y);
                    v.handle_event(release,running);
                    release.type=SDL_EVENT_MOUSE_BUTTON_UP;release.button.x=float(covered.x);release.button.y=float(covered.y);
                    v.handle_event(release,running);
                    check(v.selected_building()==selected&&v.world().snapshot()==state,"map press released over UI cannot inspect clipped wall pixels or mutate World");
                    v.handle_event(key(SDLK_TAB),running);
                    check(v.layout().map.contains(covered.x,covered.y),"panel toggle expands the current input viewport");
                    click(v,covered,running);
                    check(v.selected_building()==selected&&v.world().snapshot()==state,"panel toggle before redraw cannot inspect previous UI-covered pixels");
                    check(v.render(),"new panel-free submitted frame");
                    const auto visible=screen(v.camera(),{108.5,1668.5});
                    check(pixel(r,visible)==red,"previously clipped authored wall pixel becomes visible after redraw");
                    click(v,visible,running);check(!v.selected_building(),"new submitted viewport re-enables visible wall inspection");
                    v.handle_event(key(SDLK_TAB),running);check(v.render(),"restore panel viewport");
                    click(v,screen(v.camera(),body),running);check(v.selected_building()==selected,"restored viewport retains ordinary foreground selection");
                    check(v.world().snapshot()==state,"layout changes and deferred inspection preserve full World");
                    const auto help_covered=screen(v.camera(),{50.5,1668.5});
                    v.handle_event(key(SDLK_H),running);check(v.help_open()&&v.render(),"actual blocking Help frame");
                    check(pixel(r,help_covered)!=red,"Help really covers the authored wall pixel");
                    v.handle_event(key(SDLK_H),running);click(v,help_covered,running);
                    check(v.selected_building()==selected&&v.world().snapshot()==state,"closing submitted Help before redraw defers covered-pixel inspection");
                    check(v.render()&&pixel(r,help_covered)==red,"Help close redraw restores visible structural body");
                    click(v,help_covered,running);check(!v.selected_building(),"new unblocked frame restores wall inspection");
                    click(v,screen(v.camera(),body),running);check(v.selected_building()==selected,"post-Help foreground building selection");
                    check(v.world().snapshot()==state,"Help visibility and inspection preserve full World");
                }
            }
            v.handle_event(key(SDLK_F1),running);
        }
        v.handle_event(key(SDLK_F1),running);v.handle_event(key(SDLK_F8),running);v.handle_event(key(SDLK_F8),running);
        check(v.render()&&pixel(r,screen(v.camera(),body))==Color{255,0,255,255},"actual F8 Ground mode hides complete structural Base and Overlay");
        click(v,screen(v.camera(),body),running);check(bool(v.selected_building()),"F8-hidden wall never steals visible building inspection");
        for(int n=0;n<6;++n) v.handle_event(key(SDLK_F8),running);
        check(v.render()&&pixel(r,screen(v.camera(),body))==((direction>0||hole) ? Color{255,0,255,255}:red),"F8 return restores complete structural composition");
        check(v.world().snapshot()==before,"render/zoom/inspection preserve World");
        v.shutdown();
    }
    for(bool hole:{false,true}) {
    auto s=fixture(t,0,false,maps::LandscapeCompositionPolicy::SpatialCombined,hole);
    // The front walker has a higher logical depth but is lifted by an authored
    // saved-height byte. Its carried cargo can then overlap the lower wall body.
    s.plan.height_bytes[118*228+118]=3;
    openemperor::SandboxView v(std::move(s),false,sim::RulesProfile::CityV10);
    v.configure_save(t.root,"Cities/Synthetic.map",{});v.set_walker_visuals(t.root/"walker.json");v.initialize(w,r);
    for(auto [type,cell]:std::array<std::pair<sim::CommandType,sim::Cell>,4>{{
        {sim::CommandType::PlaceHousehold,{115,110}},{sim::CommandType::PlaceHousehold,{118,110}},
        {sim::CommandType::PlaceClaySource,{112,110}},{sim::CommandType::PlacePottery,{118,119}}}})
        check(v.execute({type,cell}).accepted,"moving walker town placement");
    for(auto cell:std::array<sim::Cell,12>{{{113,112},{113,113},{114,113},{114,114},{115,114},{115,115},
        {116,115},{116,116},{117,116},{117,117},{118,117},{118,118}}})
        check(v.execute({sim::CommandType::PlaceRoad,cell}).accepted,"moving walker route");
    bool running=true;v.set_tool(5);zoom(v,1,running);v.handle_event(key(SDLK_F1),running);
    bool back=false,front=false;
    for(int n=0;n<5000 && !(back&&front);++n) {
        v.tick_once();const auto& courier=v.world().couriers().front();const auto pos=v.world().courier_position(courier.id);
        if(!pos || pos->x!=pos->y || (pos->x!=113 && pos->x!=118)) continue;
        const bool is_front=pos->x==118;
        check(v.render(),"actual moving walker scene");
        const auto p=screen(v.camera(),body);check(pixel(r,p)==((is_front||hole) ? green:red),"actual moving walker depth and transparent Base pixel");
        click(v,p,running);const auto lines=v.inspection_lines();
        const bool walker=std::any_of(lines.begin(),lines.end(),[](const auto& line){return line.find("Visible walker #")!=std::string::npos;});
        check(walker==(is_front||hole),"foreground walker and alpha hole win; hidden walker does not steal wall");
        is_front ? front=true:back=true;
    }
    check(back&&front,"ordinary authoritative route exercised both sides of wall");
    const auto snapshot=v.world().snapshot();
    namespace perf=openemperor::performance;perf::set_enabled(true);perf::reset();
    for(int n=0;n<12;++n) {check(v.render(),"pure repeated scene");(void)v.inspection_lines();}
    for(auto counter:{perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::AssetDecodes,
        perf::Counter::TextureUploads,perf::Counter::WorldCopies,perf::Counter::WorldExecutes,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes})
        check(perf::counter(counter)==0,"normal rendering/inspection performs no asset or authority work");
    perf::set_enabled(false);check(v.world().snapshot()==snapshot,"normal frames preserve complete World");
    check(v.world().couriers().front().cargo>0,"ordinary front courier really carries cargo");
    v.set_walker_visuals(t.root/"cargo.json");check(v.render(),"tiny sprite and protruding loaded cargo");
    const auto cargo=screen(v.camera(),{3.4,1729.});
    check(pixel(r,cargo)==Color{25,95,255,255},"loaded sprite cargo paints outside its one-pixel body over wall");
    click(v,cargo,running);
    auto lines=v.inspection_lines();
    check(std::any_of(lines.begin(),lines.end(),[](const auto& line){return line.find("Visible walker #")!=std::string::npos;}),"actual protruding sprite cargo selects its foremost walker");
    v.set_walker_visuals({});check(v.render(),"fallback courier and protruding cargo");
    const auto marker_cargo=screen(v.camera(),{-3.5,1731.5});
    check(pixel(r,marker_cargo)==Color{25,95,255,255},"fallback loaded cargo independently paints over wall");
    click(v,marker_cargo,running);lines=v.inspection_lines();
    check(std::any_of(lines.begin(),lines.end(),[](const auto& line){return line.find("Visible walker #")!=std::string::npos;}),"actual protruding fallback cargo selects its foremost walker");
    check(v.world().snapshot()==snapshot,"profile changes and cargo inspection preserve full World");
    // Use the ordinary public road tool in a working town while its existing
    // courier moves at 4x. A held preview must leave authority to real ticks.
    v.handle_event(key(SDLK_F1),running);v.set_tool(1);zoom(v,4,running);
    v.handle_event(key(SDLK_EQUALS),running);v.handle_event(key(SDLK_EQUALS),running);
    if(v.paused()) v.handle_event(key(SDLK_SPACE),running);
    const auto road_point=[&](sim::Cell cell) {return screen(v.camera(),{(cell.x-cell.y)*40.,(cell.x+cell.y-144)*20.+20.});};
    const auto start=road_point({113,114}),end=road_point({113,115});
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=float(start.x);e.button.y=float(start.y);v.handle_event(e,running);
    check(v.road_preview().valid,"held road begins on ordinary ground mapping");
    const auto commands=v.world().command_sequence(),revision=v.world().road_revision();
    auto reference=sim::World::restore(v.world().snapshot(),std::vector<std::uint8_t>(228*228,1));
    perf::set_enabled(true);perf::reset();
    const auto plans=v.road_plan_build_count();
    for(int n=0;n<5;++n) {
        e={};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=float(end.x);e.motion.y=float(end.y);v.handle_event(e,running);
        const auto ticks=v.world().ticks();v.update(.04);
        check(v.world().ticks()-ticks>=3,"busy drag uses actual 4x clock");
        for(auto now=ticks;now<v.world().ticks();++now) reference.tick();
        check(v.render(),"4x busy road preview draw");
        check(v.world().snapshot()==reference.snapshot(),"road preview preserves full ordinary tick result");
        check(v.world().command_sequence()==commands&&v.world().road_revision()==revision,"held road never commits authority");
    }
    check(v.road_preview().new_road_count>=1&&v.road_plan_build_count()<=plans+1,"busy held road remains coalesced with actual new-road preview");
    for(auto counter:{perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::AssetDecodes,
        perf::Counter::TextureUploads,perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::RouteRefreshes})
        check(perf::counter(counter)==0,"held busy road preview has no per-frame asset or command work");
    perf::set_enabled(false);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_RIGHT;v.handle_event(e,running);
    check(v.world().snapshot()==reference.snapshot(),"road cancellation preserves ordinary World");v.shutdown();
    }
}
}
int main() {
    try {
        Temp t;images(t);check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),"SDL setup");
        const auto building=openemperor::assets::load_building_visual_profile(t.root,t.root/"building.json");
        check(building.unique_images.size()==1,"authored Farm profile eagerly decodes one physical image");
        for(std::size_t n=3;n<building.unique_images[0].pixels.size();n+=4)
            check(building.unique_images[0].pixels[n]==255,"authored full Omega makes every Farm raster pixel opaque");
        SDL_Window* w=nullptr;SDL_Renderer* r=nullptr;
        check(SDL_CreateWindowAndRenderer("authored scene composition",1100,700,0,&w,&r),"SDL window");
        auto* target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1100,700);
        check(target,"offscreen production pixel surface");
        renderer_cases(t,r);additional_pixels(t,r);map_debug_case(t,w,r);
        check(SDL_SetRenderTarget(r,target),"Sandbox offscreen production pixel surface");sandbox_cases(t,w,r);
        SDL_SetRenderTarget(r,nullptr);SDL_DestroyTexture(target);SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();
        std::cout<<"Authored scene composition pixel, picking and production-view regressions passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
