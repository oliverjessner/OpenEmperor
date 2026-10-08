#include "app/SandboxView.h"
#include "app/WalkerPose.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/RoadSpriteSet.h"
#include "renderer/WalkerSpriteSet.h"
#include "EconomyStartViewFixture.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string_view>
#include <nlohmann/json.hpp>

// Same production SandboxView translation unit. This observer optionally omits
// only the old small Food cargo rectangle for an identical-frame A/B baseline;
// every other SDL call remains real. No alternative painter is implemented.
namespace cargo_observer {
bool omit=false;
std::size_t calls=0;
bool fill(SDL_Renderer* renderer,const SDL_FRect* rectangle) {
    std::uint8_t r=0,g=0,b=0,a=0;
    if (rectangle && SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a) &&
        r==153 && g==132 && b==68 && a==255 &&
        rectangle->w==rectangle->h && rectangle->w<25) {
        ++calls;
        if (omit) return true;
    }
    return SDL_RenderFillRect(renderer,rectangle);
}
}
#define SDL_RenderFillRect cargo_observer::fill
#include "../src/app/SandboxView.cpp"
#undef SDL_RenderFillRect

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace maps=oe::maps;
namespace assets=oe::assets;
namespace perf=oe::performance;
namespace authored=oe::testing::economy;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Bytes=authored::Bytes;
using Color=std::array<std::uint8_t,4>;
using Point=oe::scene::Point;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
constexpr Color road{0,0,255,255},leg{255,255,0,255},foreground{255,0,0,255};
constexpr int width=29,height=34,foot_x=10,foot_y=30;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void save_json(const fs::path& path,const Json& value) {std::ofstream out(path);out<<value.dump(2);check(bool(out),"authored JSON write");}
Bytes omega(const std::vector<std::uint16_t>& raster,int w,int h) {
    Bytes result;
    for(int y=0;y<h;++y) for(int x=0;x<w;) {
        int count=1;const bool empty=raster[static_cast<std::size_t>(y*w+x)]==0;
        while(x+count<w && (raster[static_cast<std::size_t>(y*w+x+count)]==0)==empty)++count;
        result.push_back(static_cast<std::uint8_t>(empty?255:count));
        if(empty)result.push_back(static_cast<std::uint8_t>(count));
        else for(int n=0;n<count;++n) {const auto c=raster[static_cast<std::size_t>(y*w+x+n)];
            result.push_back(static_cast<std::uint8_t>(c));result.push_back(static_cast<std::uint8_t>(c>>8U));}
        x+=count;
    }
    return result;
}
struct Fixture {
    authored::Temp map;
    fs::path data=map.root/"data",walkers=map.root/"walkers.json",roads=map.root/"roads.json";
    Fixture() {
        map.root=fs::canonical(map.root);data=map.root/"data";walkers=map.root/"walkers.json";roads=map.root/"roads.json";
        // Neutral gray historical ground, independent blue opaque road bases.
        Bytes neutral(3200);for(std::size_t p=0;p<neutral.size();p+=2)authored::u16(neutral,p,0x4210);
        authored::write(data/"DATA/China_Terrain.555",neutral);
        Bytes archive(40680+5*72,0),bitmap(4,0);
        authored::u32(archive,0,static_cast<std::uint32_t>(archive.size()));authored::u32(archive,4,214);
        authored::u32(archive,12,5);authored::u32(archive,16,5);authored::u32(archive,20,1);
        const std::string name="independently-authored.bmp";std::copy(name.begin(),name.end(),archive.begin()+680);authored::u32(archive,804,5);
        const auto image=[&](int id,int w,int h,int type,int base,const Bytes& pixels) {
            const auto at=40680+static_cast<std::size_t>(id)*72;
            authored::u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));authored::u32(archive,at+4,static_cast<std::uint32_t>(pixels.size()));
            authored::u32(archive,at+8,static_cast<std::uint32_t>(base));authored::u16(archive,at+20,static_cast<std::uint16_t>(w));
            authored::u16(archive,at+22,static_cast<std::uint16_t>(h));authored::u16(archive,at+50,static_cast<std::uint16_t>(type));
            if(base)archive[at+55]=1;
            bitmap.insert(bitmap.end(),pixels.begin(),pixels.end());
        };
        Bytes base(3200);for(std::size_t p=0;p<base.size();p+=2)authored::u16(base,p,0x001f);
        image(1,78,40,30,3200,base);
        for(int phase=0;phase<2;++phase) {
            std::vector<std::uint16_t> raster(width*height,0);
            const auto fill=[&](int x,int y,int w,int h,std::uint16_t color) {
                for(int dy=0;dy<h;++dy)for(int dx=0;dx<w;++dx)raster[static_cast<std::size_t>((y+dy)*width+x+dx)]=color;
            };
            fill(2,4,9,22,0x03ff); // Off-center opaque torso, transparent right side.
            fill(foot_x-3,foot_y-4,9,7,0x7fe0); // Broad, independently defined yellow feet.
            fill(phase?0:12,7,3,4,0x03e0); // Two genuinely different gait silhouettes.
            fill(5,12,2,3,0); // Interior hole, separate from Cargo's transparent region.
            image(phase+2,width,height,256,0,omega(raster,width,height));
        }
        // A separate high historical object: red upper column with a transparent
        // opening, on a transparent base. It retains ordinary spatial depth.
        Bytes tall_base(3200);for(std::size_t p=0;p<tall_base.size();p+=2)authored::u16(tall_base,p,0xf81f);
        std::vector<std::uint16_t> tall(78*100,0);
        for(int y=12;y<90;++y)for(int x=16;x<62;++x) tall[static_cast<std::size_t>(y*78+x)]=0x7c00;
        for(int y=32;y<39;++y)for(int x=28;x<35;++x)tall[static_cast<std::size_t>(y*78+x)]=0;
        auto overlay=omega(tall,78,100);tall_base.insert(tall_base.end(),overlay.begin(),overlay.end());image(4,78,100,30,3200,tall_base);
        authored::write(data/"DATA/authored.sg3",archive);authored::write(data/"DATA/authored.555",bitmap);
        Json role={{"ticks_per_frame",2},{"clip_id","independent-road-ground-walker"},{"evidence","Authored off-center torso, yellow feet and transparent cargo area."},
            {"frames",Json::array({{{"alias","a"},{"archive","DATA/authored.sg3"},{"image_index",2},{"foot_anchor",{foot_x,foot_y}}},
                                  {{"alias","b"},{"archive","DATA/authored.sg3"},{"image_index",3},{"foot_anchor",{foot_x,foot_y}}}})},
            {"clips",{{"pos_x",{"a","b"}},{"neg_x",{"a","b"}},{"pos_y",{"a","b"}},{"neg_y",{"a","b"}}}},{"idle","a"}};
        save_json(walkers,{{"schema_version",4},{"mode","curated_walker_preview"},{"roles",{{"supplier",role},{"distributor",role},{"fire_inspector",role}}}});
        Json tiles=Json::object();constexpr char hex[]="0123456789abcdef";
        for(int mask=0;mask<16;++mask)tiles[std::string("0x")+hex[mask]]={{"archive","DATA/authored.sg3"},{"image_index",1},{"ground_anchor",{39,20}},{"evidence","Independent opaque blue road, fixed native geometry."}};
        save_json(roads,{{"schema_version",1},{"mode","curated_road_preview"},{"replaces_ground",true},{"tiles",tiles}});
    }
    maps::StoredMapSession session(bool tall=false) const {
        auto result=maps::load_stored_map_session(data,"Cities/A.map",maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
        if(tall) {
            const auto archive=assets::scan_asset_archive(data,"DATA/authored.sg3");
            auto record=archive.records.at(4);
            maps::StoredAsset asset;asset.record=record;const auto index=result.plan.assets.size();result.plan.assets.push_back(std::move(asset));
            const auto cell_index=*result.plan.cell_by_storage[112U*228U+107U];
            auto& cell=result.plan.cells[cell_index];auto& footprint=result.plan.footprints[*cell.footprint_index];
            cell.asset_index=index;footprint.asset_index=index;
            const auto origin=maps::stored_image_origin(cell.world,78,100);cell.image_origin=origin;footprint.image_origin=origin;
        }
        return result;
    }
};
void key(oe::SandboxView& view,SDL_Keycode code) {SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;bool running=true;view.handle_event(e,running);}
void button(oe::SandboxView& view,Point p,bool down) {SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=SDL_BUTTON_LEFT;e.button.x=static_cast<float>(p.x);e.button.y=static_cast<float>(p.y);bool running=true;view.handle_event(e,running);check(running,"unexpected pointer quit");}
void click(oe::SandboxView& view,Point p) {button(view,p,true);button(view,p,false);}
void zoom(oe::SandboxView& view,Point anchor,double target) {
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(anchor.x);e.wheel.mouse_y=static_cast<float>(anchor.y);
    e.wheel.y=static_cast<float>(std::log(target/view.camera().zoom)/std::log(1.15));bool running=true;view.handle_event(e,running);
    check(std::abs(view.camera().zoom-target)<0.00001,"ordinary fractional zoom failed");
}
Point ground(const oe::SandboxView& view,sim::CourierId id) {
    const auto p=view.world().courier_position(id);check(p.has_value(),"courier has no real position");
    // Independently known map projection, no draw-key/layer oracle.
    return view.camera().world_to_screen({40*(p->x-p->y),20*(p->x+p->y-144)+20});
}
Point cell_ground(const oe::SandboxView& view,sim::Cell cell) {return view.camera().world_to_screen({40.0*(cell.x-cell.y),20.0*(cell.x+cell.y-144)+20});}
Point relative(const oe::SandboxView& view,sim::CourierId id,double x,double y) {const auto p=ground(view,id);return {p.x+x*view.camera().zoom,p.y+y*view.camera().zoom};}
void in_map(const oe::SandboxView& view,Point p) {check(view.layout().map.contains(p.x,p.y),"independent positive witness clipped by current map aperture");}
Surface pixels(SDL_Renderer* renderer) {auto* raw=SDL_RenderReadPixels(renderer,nullptr);check(raw!=nullptr,"owned pre-Present target readback");auto* result=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(raw);check(result!=nullptr,"RGBA pixel surface");return {result,SDL_DestroySurface};}
Color pixel(SDL_Surface* image,Point p) {const int x=static_cast<int>(p.x),y=static_cast<int>(p.y);check(x>=0&&y>=0&&x<image->w&&y<image->h,"witness outside target");const auto* b=static_cast<const std::uint8_t*>(image->pixels)+y*image->pitch+x*4;return {b[0],b[1],b[2],b[3]};}
void render(oe::SandboxView& view) {const auto snapshot=view.world().snapshot();perf::set_enabled(true);perf::reset();check(view.render(),"production Sandbox render failed");for(std::size_t n=0;n<static_cast<std::size_t>(perf::Counter::Count);++n)check(perf::counter(static_cast<perf::Counter>(n))==0,"frame performed asset/World/navigation work");perf::set_enabled(false);check(view.world().snapshot()==snapshot,"render changed complete World");}
void command(oe::SandboxView& view,sim::CommandType type,int x,int y) {check(view.execute({type,{x,y}}).accepted,"ordinary paid fixture command rejected");}
struct Prepared {sim::CourierId id;sim::World control;};
Prepared prepare(oe::SandboxView& view,const Fixture& fixture,SDL_Window* window,SDL_Renderer* renderer,bool early=false) {
    view.configure_save(fixture.data,"Cities/A.map",fixture.map.root/"save.json");view.set_walker_visuals(fixture.walkers);view.set_road_visuals(fixture.roads);view.initialize(window,renderer);
    sim::World control=view.world();
    const auto paid=[&](sim::CommandType type,int x,int y) {command(view,type,x,y);check(control.execute({type,{x,y}}).accepted,"ordinary control command rejected");check(view.world().snapshot()==control.snapshot(),"presentation changed complete paid-command state");};
    paid(sim::CommandType::PlaceClaySource,97,110);paid(sim::CommandType::PlacePottery,100,110);
    paid(sim::CommandType::PlaceWarehouse,103,110);paid(sim::CommandType::PlaceFarm,105,113);
    paid(sim::CommandType::PlaceHousehold,108,110);paid(sim::CommandType::PlaceHousehold,108,113);
    for(int x=97;x<=108;++x)paid(sim::CommandType::PlaceRoad,x,111);
    paid(sim::CommandType::PlaceRoad,105,112);paid(sim::CommandType::PlaceRoad,108,112);
    for(int tick=0;tick<600;++tick) {
        view.tick_once();control.tick();check(view.world().snapshot()==control.snapshot(),"presentation changed complete ordinary tick state");for(const auto& c:view.world().couriers()) {
            if(c.role==sim::CourierRole::Food&&c.phase==sim::CourierPhase::ToWarehouse&&c.path_vertex+1<c.path.size()) {
                const auto p=view.world().courier_position(c.id);
                if(c.path[c.path_vertex]==sim::Cell{105,111}&&c.path[c.path_vertex+1]==sim::Cell{106,111}&&p&&
                    (early?(p->x>105&&p->x<105.5):(p->x>105.6&&p->x<106))) {
                    zoom(view,ground(view,c.id),1.0);return {c.id,std::move(control)};
                }
            }
        }
    }
    throw std::runtime_error("ordinary Food trip never reached independent positive-depth road witness");
}
void equal_documents(oe::SandboxView& view,const sim::World& control,const Fixture& fixture,const char* label) {
    const auto a=fixture.map.root/(std::string(label)+"-drawn.json"),b=fixture.map.root/(std::string(label)+"-control.json");
    oe::persistence::write_save(a,view.capture_save_document(),fixture.data,view.buildable_mask());
    oe::persistence::write_save(b,oe::persistence::make_document(fixture.data,"Cities/A.map",view.buildable_mask(),control),fixture.data,view.buildable_mask());
    std::ifstream l(a),r(b);Json left,right;l>>left;r>>right;check(left==right,"complete serialized SaveDocuments changed with presentation");
}
void report_pixel(const char* name,const oe::SandboxView& view,sim::CourierId id,Point sample,Color actual,Color expected) {
    std::cout<<name<<" tick="<<view.world().ticks()<<" courier="<<static_cast<unsigned>(id)<<" zoom="<<view.camera().zoom<<" pixel="<<sample.x<<","<<sample.y<<" actual=";
    for(auto c:actual)std::cout<<int(c)<<",";std::cout<<" expected=";for(auto c:expected)std::cout<<int(c)<<",";std::cout<<"\n";
}
void road_case(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;oe::SandboxView view(fixture.session(),false,sim::RulesProfile::CityV7,1);auto prepared=prepare(view,fixture,window,renderer);const auto id=prepared.id;
    cargo_observer::omit=true;render(view);const auto image=pixels(renderer);const auto at=ground(view,id);
    const Point witness{at.x+2.5,at.y-1.5};in_map(view,witness);const auto actual=pixel(image.get(),witness);
    report_pixel("opaque-foot-on-adjacent-road",view,id,witness,actual,leg);
    if(actual!=leg) {
        const auto profile=assets::load_walker_visual_profile(fixture.data,fixture.walkers);
        const auto pose=oe::walker_pose(view.world().courier(id),view.world().ticks(),profile);
        const auto& frame=profile.find(*pose.role)->frames[*pose.frame];const auto& native=profile.unique_images[frame.image_index];
        std::cout<<"native-foot=";for(int y=26;y<34;++y)std::cout<<int(native.pixels[static_cast<std::size_t>((y*width+12)*4+3)])<<",";std::cout<<"\n";
        for(const auto& row:view.walker_diagnostics())if(row.id==id)std::cout<<"body submitted="<<row.submitted<<" sprite="<<row.sprite_drawn<<" bounds="<<row.image_origin.x<<","<<row.image_origin.y<<","<<row.image_width<<","<<row.image_height<<"\n";
        const auto stats=view.painter_stats();std::cout<<"groundpass="<<stats.road_ground_pass<<" roads="<<stats.road_ground_items<<" spatial="<<stats.sandbox_items<<"\n";
        for(int dy=-3;dy<=4;++dy) {for(int dx=-5;dx<=8;++dx)std::cout<<(pixel(image.get(),{at.x+dx+.5,at.y+dy+.5})==leg?'Y':pixel(image.get(),{at.x+dx+.5,at.y+dy+.5})==road?'B':'.');std::cout<<"\n";}
    }
    check(actual==leg,"adjacent opaque road overwrote the independently authored Walker foot");
    for(double z:{1.15,2.0,4.0,1.0}) {zoom(view,ground(view,id),z);render(view);const auto out=pixels(renderer);const auto p=relative(view,id,2.5,-1.5);in_map(view,p);check(pixel(out.get(),p)==leg,"fractional/integer zoom hid opaque feet behind roads");}
    key(view,SDLK_F7);render(view);const auto legacy=pixels(renderer);check(pixel(legacy.get(),relative(view,id,2.5,-1.5))==road,"F7 no longer retains the historical spatial-road comparison");
    key(view,SDLK_F7);render(view);check(pixel(pixels(renderer).get(),relative(view,id,2.5,-1.5))==leg,"F7 return did not restore corrected ground pass");
    check(view.world().snapshot()==prepared.control.snapshot(),"F7/zoom changed complete World");equal_documents(view,prepared.control,fixture,"roads");
    const auto textures=view.walker_texture_count();fs::remove(fixture.data/"DATA/authored.sg3");fs::remove(fixture.data/"DATA/authored.555");fs::remove(fixture.walkers);fs::remove(fixture.roads);
    render(view);check(view.walker_texture_count()==textures&&pixel(pixels(renderer).get(),relative(view,id,2.5,-1.5))==leg,"prepared frame changed or reloaded unavailable original-free source files");
    cargo_observer::omit=false;view.shutdown();
}
void cargo_case(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;oe::SandboxView view(fixture.session(),false,sim::RulesProfile::CityV7,1);auto prepared=prepare(view,fixture,window,renderer,true);const auto id=prepared.id;
    const auto snapshot=view.world().snapshot();const auto at=ground(view,id);const Point witness{at.x+4.5,at.y-9.5};
    cargo_observer::omit=false;cargo_observer::calls=0;render(view);const auto displayed=pixels(renderer);const auto calls=cargo_observer::calls;
    cargo_observer::omit=true;render(view);const auto without=pixels(renderer);cargo_observer::omit=false;
    const auto actual=pixel(displayed.get(),witness),expected=pixel(without.get(),witness);
    report_pixel("transparent-sprite-cargo-area",view,id,witness,actual,expected);
    std::cout<<"cargo_calls="<<calls<<" complete_World_equal="<<(view.world().snapshot()==snapshot)<<"\n";
    check(expected==road,"independent transparent Cargo witness has no opaque blue ground");
    check(actual==expected,"normal animated sprite emitted an artificial floating Cargo rectangle");
    check(calls==0,"normal animated sprite retained the extra Cargo SDL draw");
    key(view,SDLK_F1);view.set_tool(5);render(view);const auto debug=pixels(renderer);
    check(pixel(debug.get(),relative(view,id,4.5,-9.5))==Color{240,190,30,255},"F1 lost existing actual Food cargo visual diagnostic");
    click(view,relative(view,id,4.5,-9.5));check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(id),"visible F1 Cargo diagnostic lost its matching hit");
    const auto lines=view.inspection_lines();const auto cargo="Cargo: "+std::to_string(view.world().courier(id).cargo);
    check(std::find(lines.begin(),lines.end(),cargo)!=lines.end()&&std::find(lines.begin(),lines.end(),"Goods: Food")!=lines.end(),"F1 actual Cargo amount/good inspection is missing");
    key(view,SDLK_F1);key(view,SDLK_F2);render(view);check(pixel(pixels(renderer).get(),relative(view,id,15.5,-5.5))==Color{153,132,68,255},"F2 fallback marker lost its declared loaded Cargo indicator");
    check(view.world().snapshot()==snapshot,"cargo/debug selection changed complete World");equal_documents(view,prepared.control,fixture,"cargo");
    view.shutdown();
}
void foreground_case(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;oe::SandboxView view(fixture.session(true),false,sim::RulesProfile::CityV7,1);auto prepared=prepare(view,fixture,window,renderer);const auto id=prepared.id;
    for(double z:{1.0,1.15,2.0,4.0,1.0}) {
        zoom(view,ground(view,id),z);render(view);const auto image=pixels(renderer);
        const auto solid=relative(view,id,-1.5,-10.5),hole=relative(view,id,2.5,-1.5);in_map(view,solid);in_map(view,hole);
        if(pixel(image.get(),solid)!=foreground) {report_pixel("tall-solid",view,id,solid,pixel(image.get(),solid),foreground);int minx=1100,miny=700,maxx=0,maxy=0;for(int y=0;y<700;++y)for(int x=0;x<1100;++x)if(pixel(image.get(),{double(x),double(y)})==foreground){minx=std::min(x,minx);maxx=std::max(x,maxx);miny=std::min(y,miny);maxy=std::max(y,maxy);}std::cout<<"red_bounds="<<minx<<","<<miny<<","<<maxx<<","<<maxy<<"\n";}
        check(pixel(image.get(),solid)==foreground,"genuine high foreground overlay no longer occludes Walker torso");
        check(pixel(image.get(),hole)==leg,"foreground transparent opening does not reveal real opaque Walker foot");
    }
    key(view,SDLK_F1);view.set_tool(5);render(view);
    click(view,relative(view,id,-1.5,-10.5));check(view.input_diagnostic_state().selected_landscape&&!view.input_diagnostic_state().selected_walker,"foreground alpha pick did not match visible overlay");
    click(view,relative(view,id,2.5,-1.5));check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(id),"overlay hole did not select visible Walker foot");
    const auto before=view.input_diagnostic_state();click(view,{view.layout().panel.x+10.0,view.layout().panel.y+10.0});check(view.input_diagnostic_state().selected_walker==before.selected_walker,"UI click passed through into map selection");
    equal_documents(view,prepared.control,fixture,"foreground");view.shutdown();
}
void preview_case(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;oe::SandboxView view(fixture.session(),false,sim::RulesProfile::CityV7,1);
    view.configure_save(fixture.data,"Cities/A.map",fixture.map.root/"preview.json");view.set_road_visuals(fixture.roads);view.initialize(window,renderer);
    const sim::Cell cell{104,112};zoom(view,cell_ground(view,cell),1);render(view);const auto at=cell_ground(view,cell);
    const auto old=pixel(pixels(renderer).get(),at);check(old==Color{132,132,132,255},"authored neutral old-ground fixture differs");
    const auto before=view.world().snapshot();view.set_tool(1);button(view,at,true);render(view);const auto preview=pixel(pixels(renderer).get(),at);
    for(int c=0;c<3;++c) {const int wanted=(int(road[static_cast<std::size_t>(c)])*128+int(old[static_cast<std::size_t>(c)])*127)/255;check(std::abs(int(preview[static_cast<std::size_t>(c)])-wanted)<=1,"road preview is not a single alpha-128 source-over on old ground");}
    check(view.world().snapshot()==before,"road preview mutated complete World");key(view,SDLK_ESCAPE);view.set_tool(5);render(view);check(pixel(pixels(renderer).get(),at)==old,"cancelled preview retained road or replacement pixels");
    check(view.world().snapshot()==before,"preview cancellation changed complete World");
    command(view,sim::CommandType::PlaceRoad,cell.x,cell.y);view.set_tool(5);render(view);check(pixel(pixels(renderer).get(),at)==road,"paid road missing after preview cancellation");
    check(view.road_display_stats().draws==1,"one installed road has duplicate/missing sprite submissions");
    command(view,sim::CommandType::RemoveRoad,cell.x,cell.y);render(view);check(pixel(pixels(renderer).get(),at)==old,"road removal did not restore unchanged old ground");view.shutdown();
}
void route_case(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;oe::SandboxView view(fixture.session(),false,sim::RulesProfile::CityV7,1);auto prepared=prepare(view,fixture,window,renderer,true);view.set_tool(5);
    command(view,sim::CommandType::PlaceRoad,105,110);check(prepared.control.execute({sim::CommandType::PlaceRoad,{105,110}}).accepted,"paid four-way road control rejected");
    std::array<bool,4> directions{};bool empty_return=false,curve=false,cross=false;
    for(int n=0;n<600;++n) {
        view.tick_once();prepared.control.tick();check(view.world().snapshot()==prepared.control.snapshot(),"ordinary full transport/control states differ");
        const auto& c=view.world().courier(prepared.id);const auto direction=oe::current_storage_direction(c);
        if(!direction||c.path_vertex+1>=c.path.size())continue;
        const auto from=c.path[c.path_vertex],to=c.path[c.path_vertex+1];
        if(view.world().object_at(from)!=sim::Object::Road||view.world().object_at(to)!=sim::Object::Road)continue;
        render(view);const auto image=pixels(renderer);const auto p=relative(view,prepared.id,2.5,-1.5);if(!view.layout().map.contains(p.x,p.y)||pixel(image.get(),p)!=leg)continue;
        directions[assets::direction_index(*direction)]=true;
        empty_return=empty_return||(c.phase==sim::CourierPhase::Returning&&c.cargo==0);
        cross=cross||from==sim::Cell{105,111}||to==sim::Cell{105,111};
        if(c.path_vertex>0) {const auto prior=c.path[c.path_vertex-1];curve=curve||((from.x-prior.x)!=(to.x-from.x)||(from.y-prior.y)!=(to.y-from.y));}
        if(std::all_of(directions.begin(),directions.end(),[](bool d){return d;})&&empty_return&&curve&&cross)break;
    }
    check(std::all_of(directions.begin(),directions.end(),[](bool d){return d;})&&empty_return&&curve&&cross,"real loaded/empty Food route lacks positive road-foot direction/curve/crossing witnesses");
    equal_documents(view,prepared.control,fixture,"transport");view.shutdown();
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* target=nullptr;
    try {
        const bool metal=argc>1&&std::string_view(argv[1])=="metal";const std::string_view mode=argc>1?argv[1]:"all";
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal?"cocoa":"dummy")&&SDL_Init(SDL_INIT_VIDEO),"SDL fixture initialization");
        window=SDL_CreateWindow("Authored road/walker occlusion",1100,700,SDL_WINDOW_HIDDEN);check(window!=nullptr,"owned hidden window");
        renderer=SDL_CreateRenderer(window,metal?"metal":"software");check(renderer!=nullptr,"actual requested backend");
        target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1100,700);check(target&&SDL_SetRenderTarget(renderer,target),"owned same-size render target");
        if(mode=="all"||mode=="metal"||mode=="road")road_case(window,renderer);
        if(mode=="all"||mode=="metal"||mode=="cargo")cargo_case(window,renderer);
        if(mode=="all"||mode=="metal"||mode=="foreground")foreground_case(window,renderer);
        if(mode=="all"||mode=="metal"||mode=="preview")preview_case(window,renderer);
        if(mode=="all"||mode=="metal"||mode=="route")route_case(window,renderer);
        check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::RoadSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,"owned session texture leak");
        SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Production Road-ground and Cargo-overlay pixels PASS\n";return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<"\n";if(target)SDL_DestroyTexture(target);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
}
