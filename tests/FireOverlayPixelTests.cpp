#include "app/SandboxView.h"
#include "assets/FireVisualProfile.h"
#include "core/PerformanceDiagnostics.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace maps=openemperor::maps;
namespace simulation=openemperor::simulation;
using Bytes=std::vector<std::uint8_t>;
using Pixel=std::array<std::uint8_t,4>;
void check(bool okay,const char* message) {
    if (!okay) throw std::runtime_error(std::string(message)+": "+SDL_GetError());
}
void u16(Bytes& bytes,std::size_t at,std::uint16_t value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);
    bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    u16(bytes,at,static_cast<std::uint16_t>(value));
    u16(bytes,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(out),"write authored fixture");
}
struct Temp {
    std::filesystem::path path=std::filesystem::canonical(std::filesystem::temp_directory_path())/
        ("openemperor-fire-pixels-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {
        std::filesystem::create_directories(path/"DATA");
        std::filesystem::create_directories(path/"Cities");
        std::ofstream out(path/"Cities/Authored.map");out<<"Independent fire renderer fixture";
    }
    ~Temp() {std::error_code error;std::filesystem::remove_all(path,error);}
};
maps::StoredMapSession fixture(const Temp& temp) {
    // Independently authored solid neutral RGB555 ground, no original game data.
    Bytes sg3(40680U+64U,0),bitmap(3200U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,213);
    u32(sg3,12,1);u32(sg3,16,1);u32(sg3,20,1);
    const std::string name="authored-fire-ground.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);
    u32(sg3,804,1);u32(sg3,808,1);
    u32(sg3,40684,3200);u32(sg3,40688,3200);
    u16(sg3,40700,78);u16(sg3,40702,40);u16(sg3,40730,30);sg3[40735]=1;
    for (std::size_t at=0;at<bitmap.size();at+=2) u16(bitmap,at,0x18c6);
    write(temp.path/"DATA/fire-ground.sg3",sg3);write(temp.path/"DATA/fire-ground.555",bitmap);
    maps::ParsedEmperorMap map;map.declared_map_size=84;
    maps::StoredGraphicsPlan plan;plan.data_root=temp.path;plan.profile=maps::StoredGraphicsProfile::Slot8;
    plan.border=maps::MapGeometry{84}.border;
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);
    plan.cell_by_storage.resize(228U*228U);
    openemperor::assets::AssetRecord record;record.id={"DATA/fire-ground.sg3",0};
    record.width=78;record.height=40;record.data_length=3200;record.uncompressed_length=3200;
    record.image_type=30;
    maps::StoredAsset asset;asset.record=record;plan.assets.push_back(asset);
    for (std::uint32_t y=110;y<=117;++y) for (std::uint32_t x=110;x<=117;++x) {
        maps::StoredCell cell;cell.storage={x,y};cell.cell_index=std::size_t(y)*228U+x;
        cell.terrain_raw=0x80;cell.status=maps::StoredStatus::DecodePending;cell.asset_index=0;
        cell.footprint_index=plan.footprints.size();cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=maps::stored_image_origin(cell.world,78,40);
        const auto index=plan.cells.size();plan.cell_by_storage[cell.cell_index]=index;
        plan.status_by_storage[cell.cell_index]=cell.status;plan.cells.push_back(cell);
        maps::PlacedFootprint footprint;footprint.id=plan.footprints.size();footprint.asset_index=0;
        footprint.origin=cell.storage;footprint.cell_indices={index};footprint.image_origin=cell.image_origin;
        footprint.status=maps::StoredStatus::DecodePending;plan.footprints.push_back(footprint);
    }
    return {std::move(map),std::move(plan)};
}
std::filesystem::path house_visual(const Temp& temp) {
    Bytes sg3(40680U+4U*72U,0),bitmap(12804U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
    const std::string name="authored-fire-house.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,804,4);
    const auto at=40680U+3U*72U;
    u32(sg3,at,4);u32(sg3,at+4,12800);u32(sg3,at+8,12800);
    u16(sg3,at+20,158);u16(sg3,at+22,90);u16(sg3,at+50,30);sg3[at+55]=2;
    for (std::size_t p=4;p<bitmap.size();p+=2) u16(bitmap,p,0x294a);
    write(temp.path/"DATA/fire-house.sg3",sg3);write(temp.path/"DATA/fire-house.555",bitmap);
    const auto manifest=temp.path/"fire-house.json";
    std::ofstream out(manifest);
    out<<nlohmann::json{{"schema_version",1},{"mode","curated_building_preview"},
        {"buildings",{{"household",{{"archive","DATA/fire-house.sg3"},{"image_index",3},
            {"ground_anchor",{79,70}},{"evidence","Independently authored neutral fire-pixel fixture"}}}}}};
    check(bool(out),"write authored House profile");return manifest;
}
std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> read_frame(SDL_Renderer* renderer) {
    return {SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface};
}
Pixel pixel(SDL_Surface* surface,int x,int y) {
    Pixel result{};
    check(SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]),"read interior pixel");
    return result;
}
struct Triangle {
    std::array<SDL_FPoint,3> points;
    std::array<std::array<double,4>,3> colors{{{{220,75,30,235}},{{245,125,35,235}},{{255,205,65,245}}}};
};
Triangle triangle(const openemperor::SandboxView& view,simulation::BuildingId id) {
    const auto& building=view.world().building(id);
    const auto front=simulation::building_front_cell(view.world().profile(),view.world().rule_version(),
        building.kind,building.cell);
    const auto ground=view.camera().world_to_screen(maps::terrain_ground(
        {static_cast<std::uint32_t>(front.x),static_cast<std::uint32_t>(front.y)},72));
    const float size=static_cast<float>(std::max(7.0,14.0*view.camera().zoom));
    const float flicker=static_cast<float>((view.world().ticks()/4U)%3U)*size*0.15F;
    const float x=static_cast<float>(ground.x),y=static_cast<float>(ground.y)-size;
    return {{{{x-size,y},{x+size,y},{x,y-size*2.5F-flicker}}}};
}
std::array<double,3> barycentric(const Triangle& tri,double x,double y) {
    const auto& p=tri.points;
    const double denominator=(p[1].y-p[2].y)*(p[0].x-p[2].x)+(p[2].x-p[1].x)*(p[0].y-p[2].y);
    const double a=((p[1].y-p[2].y)*(x-p[2].x)+(p[2].x-p[1].x)*(y-p[2].y))/denominator;
    const double b=((p[2].y-p[0].y)*(x-p[2].x)+(p[0].x-p[2].x)*(y-p[2].y))/denominator;
    return {a,b,1-a-b};
}
void verify_color(SDL_Surface* actual,SDL_Surface* background,const Triangle& tri,bool print_pixels=false) {
    // Expected channels come directly from the documented 8-bit palette and
    // independent barycentric interpolation, never the production conversion.
    for (const auto weights:std::array<std::array<double,3>,3>{{{{.25,.25,.5}},{{.4,.2,.4}},{{.2,.4,.4}}}}) {
        double x=0,y=0;for (std::size_t n=0;n<3;++n) {x+=weights[n]*tri.points[n].x;y+=weights[n]*tri.points[n].y;}
        const int px=static_cast<int>(std::floor(x)),py=static_cast<int>(std::floor(y));
        const auto w=barycentric(tri,px+.5,py+.5);
        check(*std::min_element(w.begin(),w.end())>.15,"sample must be well inside triangle");
        const auto observed=pixel(actual,px,py),back=pixel(background,px,py);
        std::array<double,4> source{};
        for (std::size_t n=0;n<3;++n) for (std::size_t c=0;c<4;++c) source[c]+=w[n]*tri.colors[n][c];
        const double alpha=source[3]/255.0;
        if (print_pixels) std::cout<<"interior("<<px<<','<<py<<")="<<unsigned(observed[0])<<','<<unsigned(observed[1])<<','
            <<unsigned(observed[2])<<','<<unsigned(observed[3])<<" background="<<unsigned(back[0])<<','
            <<unsigned(back[1])<<','<<unsigned(back[2])<<" alpha="<<alpha<<'\n';
        check(observed[0]>observed[1]+25 && observed[1]>observed[2]+35,
            "production fire interior must be orange-yellow, not white");
        for (std::size_t c=0;c<3;++c) {
            const double expected=source[c]*alpha+back[c]*(1-alpha);
            check(std::abs(observed[c]-expected)<=7,"production fire palette or alpha composition");
        }
    }
}
SDL_Event key(SDL_Keycode code) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;return event;
}
void zoom_to(openemperor::SandboxView& view,simulation::BuildingId house,double target) {
    const auto tri=triangle(view,house);
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_WHEEL;
    // Zoom around the midpoint of the two Houses, keeping both interiors in
    // the map viewport at 4x without changing the production camera seam.
    event.wheel.mouse_x=(tri.points[0].x+tri.points[1].x)/2+static_cast<float>(60*view.camera().zoom);
    event.wheel.mouse_y=tri.points[0].y+static_cast<float>(44*view.camera().zoom);
    event.wheel.y=static_cast<float>(std::log(target/view.camera().zoom)/std::log(1.15));
    bool running=true;view.handle_event(event,running);
    check(std::abs(view.camera().zoom-target)<.00001,"actual SDL wheel zoom target");
}
void verify_no_marker(SDL_Surface* actual,SDL_Surface* background,const Triangle& tri) {
    const int x=static_cast<int>((tri.points[0].x+tri.points[1].x+2*tri.points[2].x)/4);
    const int y=static_cast<int>((tri.points[0].y+tri.points[1].y+2*tri.points[2].y)/4);
    check(pixel(actual,x,y)==pixel(background,x,y),"non-burning or expired building retained fire pixels");
}
void verify_same_marker(SDL_Surface* actual,SDL_Surface* previous,const Triangle& tri) {
    for (const auto weights:std::array<std::array<double,3>,3>{{{{.25,.25,.5}},{{.4,.2,.4}},{{.2,.4,.4}}}}) {
        double x=0,y=0;for (std::size_t n=0;n<3;++n) {x+=weights[n]*tri.points[n].x;y+=weights[n]*tri.points[n].y;}
        check(pixel(actual,static_cast<int>(x),static_cast<int>(y))==
            pixel(previous,static_cast<int>(x),static_cast<int>(y)),"same-tick marker phase changed");
    }
}
void render_no_work(openemperor::SandboxView& view) {
    namespace perf=openemperor::performance;
    const auto snapshot=view.world().snapshot();
    perf::set_enabled(true);perf::reset();
    check(view.render(),"render actual production fixture");
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,perf::Counter::WorldExecutes,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,
        perf::Counter::TextureUploads,perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::SimulationTicks})
        check(perf::counter(counter)==0,"render performed simulation, I/O or asset work");
    perf::set_enabled(false);
    check(view.world().snapshot()==snapshot,"render changed authoritative World state");
}
void run(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp,save_root;
    openemperor::SandboxView view(fixture(temp),false,simulation::RulesProfile::CityV12);
    view.configure_save(temp.path,"Cities/Authored.map",save_root.path/"fire.json");
    view.initialize(window,renderer);
    view.set_fire_visuals({},openemperor::VisualProfileSource::Fallback,"Explicit marker regression fixture");
    check(view.execute({simulation::CommandType::PlaceHousehold,{112,112}}).accepted,"paid House fixture");
    check(view.execute({simulation::CommandType::PlaceHousehold,{115,112}}).accepted,"second paid House fixture");
    const auto house=*view.world().building_owner_at({112,112});
    const auto second_house=*view.world().building_owner_at({115,112});
    for (int tick=0;tick<1999;++tick) view.tick_once();
    check(!view.world().building_on_fire(house),"pre-fire natural state");
    check(view.render(),"render non-burning background");auto background=read_frame(renderer);
    check(bool(background),"read authored background");
    view.tick_once();check(view.world().building_on_fire(house),"natural fire at 2000 ticks");
    SDL_BlendMode before{};check(SDL_GetRenderDrawBlendMode(renderer,&before),"read production entry blend");
    std::cout<<"backend="<<SDL_GetRendererName(renderer)<<" runtime="<<SDL_GetVersion()
        <<" target=RGBA8888 texture beforePresent=true entryBlend="<<static_cast<unsigned>(before)<<'\n';
    const auto snapshot=view.world().snapshot();
    check(view.render(),"render actual production burning House");auto burning=read_frame(renderer);
    check(bool(burning),"read before-Present production frame");
    SDL_BlendMode after{};check(SDL_GetRenderDrawBlendMode(renderer,&after),"read production exit blend");
    check(before==after,"fire leaked renderer blend mode");
    check(view.world().snapshot()==snapshot,"render advanced fire time");
    verify_color(burning.get(),background.get(),triangle(view,house),true);
    verify_color(burning.get(),background.get(),triangle(view,second_house));

    // A separate valid renderer-test snapshot supplies the exact same tick and
    // camera without active fire. This is a synthetic fixture, not player flow.
    auto document=view.capture_save_document();auto clear_document=document;
    for (auto& building:clear_document.world.buildings) building.fire_until_tick=document.world.ticks;
    openemperor::SandboxView clear(fixture(temp),false,simulation::RulesProfile::CityV12);
    clear.configure_save(temp.path,"Cities/Authored.map",{},clear_document);clear.initialize(window,renderer);
    clear.set_fire_visuals({},openemperor::VisualProfileSource::Fallback,"Explicit no-fire marker companion");
    const auto manifest=house_visual(temp);view.set_building_visuals(manifest);clear.set_building_visuals(manifest);
    check(view.building_texture_count()==1,"eager authored House texture count");
    bool running=true;
    for (const bool visuals:{true,false}) {
        if (!visuals) {view.handle_event(key(SDLK_F4),running);clear.handle_event(key(SDLK_F4),running);}
        check(view.building_visuals_active()==visuals,"F4 actually switched configured building visuals");
        for (const bool diagnostics:{false,true}) {
            if (diagnostics) {view.handle_event(key(SDLK_F1),running);clear.handle_event(key(SDLK_F1),running);}
            for (const double zoom:{1.0,1.15,2.0,4.0,1.0}) {
                zoom_to(view,house,zoom);zoom_to(clear,house,zoom);
                check(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE),"set ordinary caller blend");
                render_no_work(clear);auto back=read_frame(renderer);check(bool(back),"read no-fire companion");
                render_no_work(view);auto pixels=read_frame(renderer);check(bool(pixels),"read zoom fire frame");
                verify_color(pixels.get(),back.get(),triangle(view,house));
                verify_color(pixels.get(),back.get(),triangle(view,second_house));
                SDL_BlendMode restored{};check(SDL_GetRenderDrawBlendMode(renderer,&restored) && restored==SDL_BLENDMODE_NONE,
                    "multiple burning buildings leaked blend mode");
                check(view.world().snapshot()==snapshot,"F1/F4/zoom changed fire authority");
            }
            if (diagnostics) {view.handle_event(key(SDLK_F1),running);clear.handle_event(key(SDLK_F1),running);}
        }
    }
    // Caller BLEND is retained as well; the effect is alpha-composed once.
    check(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND),"set alternate caller blend");
    render_no_work(clear);auto blended_back=read_frame(renderer);render_no_work(view);auto blended=read_frame(renderer);
    check(bool(blended_back) && bool(blended),"read alternate blend frames");
    verify_color(blended.get(),blended_back.get(),triangle(view,house));
    SDL_BlendMode retained{};check(SDL_GetRenderDrawBlendMode(renderer,&retained) && retained==SDL_BLENDMODE_BLEND,
        "fire failed to restore caller BLEND");
    check(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE),"restore ordinary caller state");
    // Pausing and repeated rendering keep the same phase and all pixels stable.
    view.handle_event(key(SDLK_SPACE),running);check(view.paused(),"pause fire fixture");
    render_no_work(view);auto first=read_frame(renderer);check(bool(first),"read paused frame");
    for (int frame=0;frame<25;++frame) {
        render_no_work(view);auto next=read_frame(renderer);check(bool(next),"read repeated frame");
        check(first->pitch==next->pitch && first->h==next->h &&
            std::memcmp(first->pixels,next->pixels,std::size_t(first->pitch)*static_cast<std::size_t>(first->h))==0,
            "paused repeated rendering advanced flicker or pixels");
    }
    // Save/load retains the exact snapshot, tick-driven phase and frame.
    view.save_now();const auto saved=view.world().snapshot();view.tick_once();view.load_now();
    check(view.world().snapshot()==saved && view.paused(),"burning save/load changed authoritative state");
    render_no_work(view);auto loaded=read_frame(renderer);check(bool(loaded),"read reloaded fire frame");
    verify_same_marker(loaded.get(),first.get(),triangle(view,house));
    verify_same_marker(loaded.get(),first.get(),triangle(view,second_house));
    // Actual natural expiry, with no Inspector, removes both markers.
    while (view.world().ticks()<2600) view.tick_once();
    check(!view.world().building_on_fire(house) && !view.world().building_on_fire(second_house),"natural fire expiry");
    render_no_work(clear);auto no_fire=read_frame(renderer);render_no_work(view);auto ended=read_frame(renderer);
    check(bool(no_fire) && bool(ended),"read naturally expired frame");
    verify_no_marker(ended.get(),no_fire.get(),triangle(view,house));
    verify_no_marker(ended.get(),no_fire.get(),triangle(view,second_house));
    clear.shutdown();
    view.shutdown();
    std::cout<<"A/B/D/F/G/H/I/J/K: natural fire, no-fire fixture, expiry, F4/F1, 1/1.15/2/4/1 zoom, blend, save, pause, no render work\n";
}
void inspector_checks(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp;
    openemperor::SandboxView view(fixture(temp),false,simulation::RulesProfile::CityV12);
    view.initialize(window,renderer);
    view.set_fire_visuals({},openemperor::VisualProfileSource::Fallback,"Explicit Inspector marker regression fixture");
    check(view.execute({simulation::CommandType::PlaceHousehold,{112,112}}).accepted,"Inspector paid House");
    check(view.execute({simulation::CommandType::PlaceHousehold,{115,112}}).accepted,"Inspector second paid House");
    check(view.execute({simulation::CommandType::PlaceFireWatch,{115,115}}).accepted,"Inspector paid Watch");
    const auto house=*view.world().building_owner_at({112,112});
    const auto watch=*view.world().building_owner_at({115,115});
    for (int x=112;x<=115;++x) check(view.execute({simulation::CommandType::PlaceRoad,{x,114}}).accepted,"Inspector road");
    check(view.execute(simulation::set_building_operation(watch,false)).accepted,"pause Watch for natural fire");
    for (int tick=0;tick<2000;++tick) view.tick_once();
    check(view.world().building_on_fire(house),"Inspector target natural fire");
    check(view.execute(simulation::set_building_operation(watch,true)).accepted,"resume ordinary Watch operation");
    auto previous_remaining=view.world().fire_remaining(house);
    bool dispatched=false,arrived=false;
    for (int tick=0;tick<100;++tick) {
        view.tick_once();
        const auto& courier=*std::find_if(view.world().couriers().begin(),view.world().couriers().end(),
            [](const auto& c) {return c.role==simulation::CourierRole::FireInspector;});
        if (!dispatched && courier.phase==simulation::CourierPhase::ToWarehouse) {
            check(view.world().building_on_fire(house) && !view.world().building_fire_protected(house),
                "dispatch alone extinguished fire");
            dispatched=true;
        }
        if (!view.world().building_on_fire(house)) {
            check(dispatched && previous_remaining>1 && view.world().building_fire_protected(house) &&
                view.world().building(house).fire_until_tick==view.world().ticks() &&
                view.world().building(house).fire_protection_until_tick==view.world().ticks()+2400,
                "fire cleared without actual arrival or correct protection");
            arrived=true;break;
        }
        previous_remaining=view.world().fire_remaining(house);
    }
    check(arrived,"actual Inspector did not arrive before natural expiry");
    render_no_work(view);auto actual=read_frame(renderer);check(bool(actual),"read Inspector-cleared marker");
    const auto tri=triangle(view,house);
    const int sample_x=static_cast<int>((tri.points[0].x+tri.points[1].x+2*tri.points[2].x)/4);
    const int sample_y=static_cast<int>((tri.points[0].y+tri.points[1].y+2*tri.points[2].y)/4);
    const auto color=pixel(actual.get(),sample_x,sample_y);
    check(!(color[0]>color[1]+25 && color[1]>color[2]+35),"Inspector-cleared building retained flame");
    view.shutdown();

    // No Houses means no workforce. The existing Watch remains unstaffed and
    // its existing idle Inspector cannot invent a new patrol or coverage.
    openemperor::SandboxView unstaffed(fixture(temp),false,simulation::RulesProfile::CityV12);
    unstaffed.initialize(window,renderer);
    unstaffed.set_fire_visuals({},openemperor::VisualProfileSource::Fallback,"Explicit unstaffed marker regression fixture");
    check(unstaffed.execute({simulation::CommandType::PlaceClaySource,{110,110}}).accepted,"unstaffed paid Clay");
    check(unstaffed.execute({simulation::CommandType::PlaceFireWatch,{115,115}}).accepted,"unstaffed paid Watch");
    const auto clay=*unstaffed.world().building_owner_at({110,110});
    const auto empty_watch=*unstaffed.world().building_owner_at({115,115});
    for (int x=110;x<=115;++x)
        check(unstaffed.execute({simulation::CommandType::PlaceRoad,{x,114}}).accepted,"unstaffed road");
    for (int y=112;y<114;++y)
        check(unstaffed.execute({simulation::CommandType::PlaceRoad,{110,y}}).accepted,"unstaffed Clay road");
    for (int tick=0;tick<2000;++tick) unstaffed.tick_once();
    check(unstaffed.world().workers_assigned(empty_watch)==0 && unstaffed.world().building_on_fire(clay),
        "unstaffed Watch gained workers or suppressed natural fire");
    const auto snapshot=unstaffed.world().snapshot();
    const auto inspector=std::find_if(snapshot.couriers.begin(),snapshot.couriers.end(),
        [](const auto& courier) {return courier.role==simulation::CourierRole::FireInspector;});
    check(inspector!=snapshot.couriers.end() && inspector->phase==simulation::CourierPhase::IdleAtWorkshop &&
        inspector->path.empty() && inspector->cargo==0 && inspector->reserved==0,
        "unstaffed Watch invented a patrol or payload");
    render_no_work(unstaffed);check(unstaffed.world().snapshot()==snapshot,"render staffed an empty Watch");
    unstaffed.shutdown();
    std::cout<<"C/E: ordinary dispatch kept fire until actual arrival; unstaffed 0/2 Watch stayed idle during natural fire\n";
}

std::filesystem::path animated_visual(const Temp& temp) {
    // Two independently authored Omega silhouettes, with different canvas
    // dimensions and a shared bottom-center attachment; no original pixels.
    Bytes sg3(40680U+5U*72U,0),bitmap{0,0,0,0};
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,5);u32(sg3,16,5);u32(sg3,20,1);
    const std::string name="authored-animated-flame.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,804,5);
    for (const auto dimensions:std::array<std::array<int,3>,2>{{{{1,30,50}},{{3,38,44}}}}) {
        const auto [index,width,height]=dimensions;Bytes payload;
        const bool second=index==3;
        for (int y=0;y<height;++y) {
            int left=width,right=-1;
            if (y>4) {
                left=second ? (y<15 ? 24:y<28 ? 7:3):(y<13 ? 12:y<30 ? 6:2);
                right=second ? (y<15 ? 26:y<28 ? 30:34):(y<13 ? 15:y<30 ? 22:27);
            }
            if (left>0) payload.insert(payload.end(),{255,static_cast<std::uint8_t>(left)});
            if (right>=left) {
                payload.push_back(static_cast<std::uint8_t>(right-left+1));
                for (int x=left;x<=right;++x) {
                    const std::uint16_t color=second ? 0x7fe0:0x7da0;
                    payload.push_back(static_cast<std::uint8_t>(color));
                    payload.push_back(static_cast<std::uint8_t>(color>>8U));
                }
            }
            if (right>=0 && right+1<width)
                payload.insert(payload.end(),{255,static_cast<std::uint8_t>(width-right-1)});
        }
        const auto at=40680U+static_cast<std::size_t>(index)*72U;
        u32(sg3,at,static_cast<std::uint32_t>(bitmap.size()));u32(sg3,at+4,static_cast<std::uint32_t>(payload.size()));
        u16(sg3,at+20,static_cast<std::uint16_t>(width));u16(sg3,at+22,static_cast<std::uint16_t>(height));
        u16(sg3,at+50,256);bitmap.insert(bitmap.end(),payload.begin(),payload.end());
    }
    write(temp.path/"DATA/animated-flame.sg3",sg3);write(temp.path/"DATA/animated-flame.555",bitmap);
    const auto manifest=temp.path/"animated-fire.json";
    std::ofstream out(manifest);
    out<<nlohmann::json{{"schema_version",1},{"mode","curated_fire_presentation"},
        {"clip_id","authored-production-animation"},{"ticks_per_frame",2},
        {"evidence","Independent asymmetric flame silhouettes; authored timing and common bottom-center attachment."},
        {"frames",nlohmann::json::array({
            {{"archive","DATA/animated-flame.sg3"},{"image_index",1},{"anchor",{15,50}}},
            {{"archive","DATA/animated-flame.sg3"},{"image_index",3},{"anchor",{19,44}}}})}};
    check(bool(out),"write authored animated profile");return manifest;
}

struct AnimatedGeometry {
    openemperor::scene::Point attachment;
    double scale=1;
};
AnimatedGeometry animated_geometry(const openemperor::SandboxView& view,simulation::BuildingId id,
                                     bool house_metadata=true) {
    const auto& building=view.world().building(id);
    const auto front=simulation::building_front_cell(view.world().profile(),view.world().rule_version(),
        building.kind,building.cell);
    const auto ground=view.camera().world_to_screen(maps::terrain_ground(
        {static_cast<std::uint32_t>(front.x),static_cast<std::uint32_t>(front.y)},72));
    const auto side=simulation::building_footprint(view.world().profile(),view.world().rule_version(),building.kind).width;
    // Independent oracle for the declared curated attachment. The configured
    // authored House has ground anchor 70; F4 changes artwork, not this anchor.
    const double rise=house_metadata && building.kind==simulation::Object::Household ? 21.0:12.0;
    return {{ground.x,ground.y-(20.0*(side-1)+rise)*view.camera().zoom},
            view.camera().zoom*(side==1 ? .75:1.0)};
}
void zoom_to_animated(openemperor::SandboxView& view,simulation::BuildingId house,double target) {
    const auto geometry=animated_geometry(view,house);
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_WHEEL;
    // Keep the independently authored attachment fixed vertically. Zooming
    // around the old triangle's lower pivot clipped every opaque witness at 4x
    // when the income header reduced the map aperture.
    event.wheel.mouse_x=static_cast<float>(geometry.attachment.x+60*view.camera().zoom);
    event.wheel.mouse_y=static_cast<float>(geometry.attachment.y);
    event.wheel.y=static_cast<float>(std::log(target/view.camera().zoom)/std::log(1.15));
    bool running=true;view.handle_event(event,running);
    check(std::abs(view.camera().zoom-target)<.00001,"actual SDL wheel animated zoom target");
}
Pixel image_pixel(const openemperor::assets::RgbaImage& image,int x,int y) {
    const auto at=(static_cast<std::size_t>(y)*image.width+static_cast<std::size_t>(x))*4U;
    return {image.pixels.at(at),image.pixels.at(at+1),image.pixels.at(at+2),image.pixels.at(at+3)};
}
void verify_animated_pixels(SDL_Surface* frame,const openemperor::SandboxView& view,simulation::BuildingId id,
                            const openemperor::assets::FireVisualProfile& profile,bool house_metadata=true) {
    const auto selected=view.fire_frame_for(id);
    if (!selected) throw std::runtime_error("prepared clip did not select frame: "+view.fire_display_stats().fallback_reason);
    const auto& entry=profile.frames.at(*selected);const auto& image=profile.unique_images.at(entry.image_index);
    const auto geometry=animated_geometry(view,id,house_metadata);const auto& map=view.layout().map;
    int matches=0,in_map_witnesses=0;
    // Source witnesses have a uniform 3x3 interior, so fractional nearest
    // rasterization cannot turn a border rounding difference into a failure.
    for (int y=1;y+1<image.height;y+=3) for (int x=1;x+1<image.width;x+=3) {
        const auto source=image_pixel(image,x,y);if (source[3]!=255) continue;
        bool interior=true;
        for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
            interior=interior && image_pixel(image,x+dx,y+dy)==source;
        if (!interior) continue;
        const int px=static_cast<int>(std::floor(geometry.attachment.x+(x+.5-entry.anchor_x)*geometry.scale));
        const int py=static_cast<int>(std::floor(geometry.attachment.y+(y+.5-entry.anchor_y)*geometry.scale));
        if (!map.contains(px,py)) continue;
        ++in_map_witnesses;
        if (pixel(frame,px,py)==source) ++matches;
    }
    check(in_map_witnesses>=3,"animated test fixture clipped its opaque pixel witnesses");
    check(matches>=3,"actual Sandbox did not render the selected prepared flame texture");
}
bool same_effect_region(SDL_Surface* first,SDL_Surface* second,const AnimatedGeometry& geometry,
                          const openemperor::sandbox_ui::Rect& map) {
    // Union of both independently declared canvases relative to their pivot.
    const int left=static_cast<int>(std::floor(geometry.attachment.x-19*geometry.scale));
    const int top=static_cast<int>(std::floor(geometry.attachment.y-50*geometry.scale));
    const int right=static_cast<int>(std::ceil(geometry.attachment.x+19*geometry.scale));
    const int bottom=static_cast<int>(std::ceil(geometry.attachment.y));
    for (int y=top;y<bottom;++y) for (int x=left;x<right;++x)
        if (map.contains(x,y) && pixel(first,x,y)!=pixel(second,x,y)) return false;
    return true;
}
void pan_y(openemperor::SandboxView& view,double delta) {
    const double target=view.camera().offset.y+delta;
    for (int step=0;step<1000 && std::abs(view.camera().offset.y-target)>.0000001;++step) {
        const double remaining=target-view.camera().offset.y;
        openemperor::SandboxView::PanKeyState keys;
        keys.up=remaining>0;keys.down=remaining<0;
        view.update(std::min(.05,std::abs(remaining)/400.0),keys);
    }
    check(std::abs(view.camera().offset.y-target)<.00001,"held-device seam did not pan to requested test view");
}

void animated_checks(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp,save_root;
    const auto manifest=animated_visual(temp),buildings=house_visual(temp);
    const auto profile=openemperor::assets::load_fire_visual_profile(temp.path,manifest);
    openemperor::SandboxView view(fixture(temp),false,simulation::RulesProfile::CityV12);
    view.configure_save(temp.path,"Cities/Authored.map",save_root.path/"animated-save.json");
    view.initialize(window,renderer);view.set_building_visuals(buildings);view.set_fire_visuals(manifest);
    const auto put=[&](simulation::CommandType type,simulation::Cell cell) {
        check(view.execute({type,cell}).accepted,"animated paid fixture command");
    };
    put(simulation::CommandType::PlaceHousehold,{112,112});
    put(simulation::CommandType::PlaceHousehold,{115,112});
    put(simulation::CommandType::PlacePottery,{110,115});
    put(simulation::CommandType::PlaceWarehouse,{113,115});
    put(simulation::CommandType::PlaceServicePost,{117,110});
    put(simulation::CommandType::PlaceFireWatch,{117,115});
    const auto house=*view.world().building_owner_at({112,112});
    const auto pottery=*view.world().building_owner_at({110,115});
    const auto small=*view.world().building_owner_at({117,110});
    const auto watch=*view.world().building_owner_at({117,115});
    check(view.execute(simulation::set_building_operation(watch,false)).accepted,"ordinary Watch pause in animation fixture");
    for (int tick=0;tick<1999;++tick) view.tick_once();
    render_no_work(view);check(view.fire_display_stats().draws==0,"non-burning scene gained sprite fire");
    view.tick_once();check(view.world().burning_buildings()==5 && !view.world().building_on_fire(watch),
        "normal fixture fire count or excluded Watch changed");
    render_no_work(view);auto first=read_frame(renderer);check(bool(first),"first animated production frame");
    const auto initial_stats=view.fire_display_stats();
    check(initial_stats.animated && initial_stats.frames==2 && initial_stats.unique_assets==2 &&
          initial_stats.texture_uploads==2 && initial_stats.draws==5 && initial_stats.fallback_draws==0 &&
          initial_stats.logical_bytes==12688,"animated complete activation/draw dedup budget wrong");
    verify_animated_pixels(first.get(),view,house,profile);
    verify_animated_pixels(first.get(),view,pottery,profile,false);
    verify_animated_pixels(first.get(),view,small,profile,false);
    const auto original_frame=view.fire_frame_for(house);
    view.tick_once();view.tick_once();
    render_no_work(view);auto second=read_frame(renderer);check(bool(second),"second animated production frame");
    check(view.fire_frame_for(house)!=original_frame &&
        !same_effect_region(first.get(),second.get(),animated_geometry(view,house),view.layout().map),
        "production different ticks rendered an unchanged flame silhouette");
    verify_animated_pixels(second.get(),view,house,profile);
    check(view.fire_display_stats().frames_drawn[0] && view.fire_display_stats().frames_drawn[1],
          "prepared different frames were never actually submitted");

    auto document=view.capture_save_document(),clear_document=document;
    for (auto& building:clear_document.world.buildings)
        if (simulation::fire_eligible(building.kind)) building.fire_until_tick=clear_document.world.ticks;
    openemperor::SandboxView clear(fixture(temp),false,simulation::RulesProfile::CityV12);
    clear.configure_save(temp.path,"Cities/Authored.map",{},clear_document);clear.initialize(window,renderer);
    clear.set_building_visuals(buildings);clear.set_fire_visuals(manifest);
    render_no_work(clear);auto background=read_frame(renderer);render_no_work(view);auto actual=read_frame(renderer);
    check(bool(background) && bool(actual),"animated transparent/background comparison");
    verify_no_marker(actual.get(),background.get(),triangle(view,house));
    // Transparent upper-left canvas texels preserve the existing scene.
    const auto selected=*view.fire_frame_for(house);const auto& entry=profile.frames[selected];
    const auto geometry=animated_geometry(view,house);
    const int hole_x=static_cast<int>(geometry.attachment.x-entry.anchor_x*geometry.scale+1);
    const int hole_y=static_cast<int>(geometry.attachment.y-entry.anchor_y*geometry.scale+1);
    check(pixel(actual.get(),hole_x,hole_y)==pixel(background.get(),hole_x,hole_y),
          "transparent effect canvas blocked or recolored the scene");
    const auto authoritative=view.world().snapshot();bool running=true;
    for (const bool visuals:{true,false}) {
        if (!visuals) {view.handle_event(key(SDLK_F4),running);clear.handle_event(key(SDLK_F4),running);}
        for (const bool diagnostics:{false,true}) {
            if (diagnostics) {view.handle_event(key(SDLK_F1),running);clear.handle_event(key(SDLK_F1),running);}
            for (const double zoom:{1.0,1.15,2.0,4.0,1.0}) {
                zoom_to_animated(view,house,zoom);zoom_to_animated(clear,house,zoom);
                render_no_work(view);auto pixels=read_frame(renderer);check(bool(pixels),"zoom animated pixels");
                verify_animated_pixels(pixels.get(),view,house,profile);
                check(view.fire_display_stats().fallback_draws==0 && view.world().snapshot()==authoritative,
                      "F1/F4/zoom changed animation authority or selected marker fallback");
            }
            if (diagnostics) {view.handle_event(key(SDLK_F1),running);clear.handle_event(key(SDLK_F1),running);}
        }
    }
    view.handle_event(key(SDLK_SPACE),running);check(view.paused(),"pause animated production view");
    render_no_work(view);auto paused=read_frame(renderer);check(bool(paused),"read paused animated frame");
    for (int repeat=0;repeat<20;++repeat) {
        render_no_work(view);auto next=read_frame(renderer);check(bool(next),"read repeated animated frame");
        check(paused->pitch==next->pitch && paused->h==next->h &&
            std::memcmp(paused->pixels,next->pixels,std::size_t(paused->pitch)*static_cast<std::size_t>(paused->h))==0,
            "pause/render frequency changed full animated scene pixels");
    }
    view.save_now();const auto saved=view.world().snapshot();const auto saved_frame=view.fire_frame_for(house);
    view.tick_once();view.load_now();check(view.world().snapshot()==saved && view.paused() &&
        view.fire_frame_for(house)==saved_frame && view.fire_display_stats().animated,"save/load changed clip phase or World");
    render_no_work(view);auto restored=read_frame(renderer);check(bool(restored),"read restored animated frame");
    check(same_effect_region(paused.get(),restored.get(),animated_geometry(view,house),view.layout().map),
          "save/load changed actual flame pixels or attachment");

    // Fault the later explicit frame: preparation must select the entire named
    // marker fallback, never retain the first decoded frame as a half clip.
    auto invalid=nlohmann::json::parse(std::ifstream(manifest));invalid["frames"][1]["image_index"]=4;
    const auto bad=temp.path/"bad-fire.json";{std::ofstream out(bad);out<<invalid;}
    view.set_fire_visuals(bad);check(!view.fire_display_stats().animated &&
        view.fire_display_stats().texture_uploads==0 && !view.fire_display_stats().fallback_reason.empty(),
        "broken frame published a partial animation or unnamed fallback");
    render_no_work(clear);auto no_fire=read_frame(renderer);render_no_work(view);auto fallback=read_frame(renderer);
    check(bool(no_fire) && bool(fallback),"read atomic fallback");
    verify_color(fallback.get(),no_fire.get(),triangle(view,house));
    view.set_fire_visuals(manifest);check(view.fire_display_stats().animated,"valid clip failed after optional preparation failure");

    // Constructing a preview cannot create another fire instance or mutate the
    // occupied building's state. The paid scene has five logical burning owners.
    if (!view.building_visuals_active()) view.handle_event(key(SDLK_F4),running);
    view.handle_event(key(SDLK_7),running);
    const auto empty=view.camera().world_to_screen(maps::terrain_ground({110,110},72));
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=static_cast<float>(empty.x);
    motion.motion.y=static_cast<float>(empty.y);view.handle_event(motion,running);
    check(view.hovered_cell()==simulation::Cell{110,110} && view.preview({110,110}).accepted,
          "animated placement preview fixture was not a valid empty House target");
    render_no_work(view);check(view.fire_display_stats().draws==5 &&
        view.fire_display_stats().fallback_draws==0 && view.world().snapshot()==saved,
        "placement preview invented fire or advanced authority");
    view.handle_event(key(SDLK_5),running);
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;view.handle_event(focus,running);
    const auto prior_camera=view.camera();
    const auto center=triangle(view,house).points[0].y+14.F;
    const double target_center=view.layout().map.y+view.layout().map.h+70.0;
    pan_y(view,target_center-center);
    render_no_work(view);auto clipped=read_frame(renderer);check(bool(clipped),"read partially clipped animated fire");
    const auto offscreen_ground=triangle(view,house).points[0].y+14.F;
    check(static_cast<double>(offscreen_ground)>view.layout().map.y+view.layout().map.h && view.fire_display_stats().draws>0,
          "offscreen owner center rejected an entering flame tip");
    verify_animated_pixels(clipped.get(),view,house,profile);
    pan_y(view,prior_camera.offset.y-view.camera().offset.y);
    render_no_work(view);check(view.world().snapshot()==saved,"camera motion changed fire time or authority");
    // End by genuine deadline expiry, without any Inspector arrival.
    while (view.world().ticks()<2600) view.tick_once();
    check(view.world().burning_buildings()==0,"animated natural deadline expiry changed");
    render_no_work(view);check(view.fire_display_stats().draws==0 && view.fire_display_stats().fallback_draws==0,
        "naturally expired owner retained sprite or marker fire");
    clear.shutdown();view.shutdown();
    check(openemperor::FireSpriteSet::live_texture_count()==0,"animated production session leaked textures");
    std::cout<<"Animated production: two pixel silhouettes, 5 logical owners/2 textures, House/Pottery/ServicePost, alpha/no triangle, F1/F4, zoom, pause, save/load, atomic fallback, preview, natural expiry\n";
}

void animated_inspector_checks(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp;const auto manifest=animated_visual(temp);
    const auto profile=openemperor::assets::load_fire_visual_profile(temp.path,manifest);
    openemperor::SandboxView view(fixture(temp),false,simulation::RulesProfile::CityV12);
    view.configure_save(temp.path,"Cities/Authored.map",{});
    view.initialize(window,renderer);view.set_fire_visuals(manifest);
    const auto put=[&](simulation::CommandType type,simulation::Cell cell) {
        check(view.execute({type,cell}).accepted,"animated Inspector paid fixture command");
    };
    put(simulation::CommandType::PlaceHousehold,{112,112});put(simulation::CommandType::PlaceHousehold,{115,112});
    put(simulation::CommandType::PlaceFireWatch,{115,115});
    const auto house=*view.world().building_owner_at({112,112}),watch=*view.world().building_owner_at({115,115});
    for (int x=112;x<=115;++x) put(simulation::CommandType::PlaceRoad,{x,114});
    check(view.execute(simulation::set_building_operation(watch,false)).accepted,"pause animated Watch normally");
    for (int tick=0;tick<2000;++tick) view.tick_once();
    check(view.world().building_on_fire(house),"animated Inspector target did not ignite");
    render_no_work(view);auto burning=read_frame(renderer);check(bool(burning),"read animation before actual arrival");
    verify_animated_pixels(burning.get(),view,house,profile,false);
    check(view.execute(simulation::set_building_operation(watch,true)).accepted,"resume animated Watch normally");
    bool dispatched=false,arrived=false;auto previous_remaining=view.world().fire_remaining(house);
    for (int tick=0;tick<100;++tick) {
        view.tick_once();
        const auto& courier=*std::find_if(view.world().couriers().begin(),view.world().couriers().end(),
            [](const auto& state){return state.role==simulation::CourierRole::FireInspector;});
        if (courier.phase==simulation::CourierPhase::ToWarehouse) {
            dispatched=true;check(view.world().building_on_fire(house),"dispatch alone removed sprite fire");
        }
        if (!view.world().building_on_fire(house)) {
            check(dispatched && previous_remaining>1 && view.world().building_fire_protected(house) &&
                view.world().building(house).fire_until_tick==view.world().ticks() &&
                view.world().building(house).fire_protection_until_tick==view.world().ticks()+2400,
                "animated path changed real Inspector arrival/deadlines");arrived=true;break;
        }
        previous_remaining=view.world().fire_remaining(house);
    }
    check(arrived,"animated Inspector never arrived before expiry");
    auto document=view.capture_save_document();
    for (auto& building:document.world.buildings)
        if (simulation::fire_eligible(building.kind)) building.fire_until_tick=document.world.ticks;
    openemperor::SandboxView clear(fixture(temp),false,simulation::RulesProfile::CityV12);
    clear.configure_save(temp.path,"Cities/Authored.map",{},document);clear.initialize(window,renderer);clear.set_fire_visuals(manifest);
    render_no_work(clear);auto background=read_frame(renderer);render_no_work(view);auto extinguished=read_frame(renderer);
    check(bool(background) && bool(extinguished) &&
        same_effect_region(background.get(),extinguished.get(),animated_geometry(view,house,false),view.layout().map),
        "actual Inspector arrival retained animated flame pixels");
    clear.shutdown();view.shutdown();

    // Real zero-workforce city: original idle Inspector remains idle, even with
    // an activated clip and an ordinary burning Clay Source.
    openemperor::SandboxView unstaffed(fixture(temp),false,simulation::RulesProfile::CityV12);
    unstaffed.configure_save(temp.path,"Cities/Authored.map",{});
    unstaffed.initialize(window,renderer);unstaffed.set_fire_visuals(manifest);
    check(unstaffed.execute({simulation::CommandType::PlaceClaySource,{110,110}}).accepted &&
        unstaffed.execute({simulation::CommandType::PlaceFireWatch,{115,115}}).accepted,"animated unstaffed fixture");
    const auto clay=*unstaffed.world().building_owner_at({110,110}),empty_watch=*unstaffed.world().building_owner_at({115,115});
    for (int tick=0;tick<2000;++tick) unstaffed.tick_once();
    check(unstaffed.world().workers_assigned(empty_watch)==0 && unstaffed.world().building_on_fire(clay),
        "animation staffed a Watch or suppressed ordinary unprotected fire");
    const auto before=unstaffed.world().snapshot();
    const auto& inspector=*std::find_if(before.couriers.begin(),before.couriers.end(),
        [](const auto& state){return state.role==simulation::CourierRole::FireInspector;});
    check(inspector.phase==simulation::CourierPhase::IdleAtWorkshop && inspector.path.empty() &&
        inspector.cargo==0 && inspector.reserved==0,"animation invented an unstaffed Inspector patrol");
    render_no_work(unstaffed);auto pixels=read_frame(renderer);check(bool(pixels),"unstaffed animated frame");
    verify_animated_pixels(pixels.get(),unstaffed,clay,profile,false);
    check(unstaffed.world().snapshot()==before,"render changed unstaffed city authority");
    unstaffed.shutdown();check(openemperor::FireSpriteSet::live_texture_count()==0,"Inspector animation session leaked effects");
    std::cout<<"Animated lifecycle: dispatch preserves fire, actual arrival removes pixels, and zero-workforce Watch remains idle\n";
}

std::filesystem::path tall_building_visual(const Temp& temp) {
    Bytes sg3(40680U+2U*72U,0),bitmap(12804U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,2);u32(sg3,16,2);u32(sg3,20,1);
    const std::string name="authored-fire-front-body.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,804,2);
    for (std::size_t at=4;at<bitmap.size();at+=2) u16(bitmap,at,0x001f);
    // Whole-image blue authored foreground body. Type30 foundation/decoder
    // geometry is unchanged; only the fixture supplies its explicit Omega body.
    for (int y=0;y<170;++y) {
        bitmap.push_back(158);
        for (int x=0;x<158;++x) bitmap.insert(bitmap.end(),{0x1f,0});
    }
    const auto at=40680U+72U;u32(sg3,at,4);u32(sg3,at+4,static_cast<std::uint32_t>(bitmap.size()-4U));
    u32(sg3,at+8,12800);u16(sg3,at+20,158);u16(sg3,at+22,170);u16(sg3,at+50,30);sg3[at+55]=2;
    write(temp.path/"DATA/fire-front-body.sg3",sg3);write(temp.path/"DATA/fire-front-body.555",bitmap);
    const auto manifest=temp.path/"fire-front-body.json";std::ofstream out(manifest);
    out<<nlohmann::json{{"schema_version",1},{"mode","curated_building_preview"},
        {"buildings",{{"household",{{"archive","DATA/fire-front-body.sg3"},{"image_index",1},
            {"ground_anchor",{79,150}},{"evidence","Independent tall opaque foreground painter fixture."}}}}}};
    check(bool(out),"write foreground building manifest");return manifest;
}
void click(openemperor::SandboxView& view,openemperor::scene::Point press,
             openemperor::scene::Point release) {
    bool running=true;SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;event.button.x=static_cast<float>(press.x);event.button.y=static_cast<float>(press.y);
    view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.x=static_cast<float>(release.x);event.button.y=static_cast<float>(release.y);view.handle_event(event,running);
}
void animated_composition_checks(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp;const auto fire=animated_visual(temp),building=tall_building_visual(temp);
    openemperor::SandboxView view(fixture(temp),false,simulation::RulesProfile::CityV12);
    view.configure_save(temp.path,"Cities/Authored.map",{});
    view.initialize(window,renderer);view.set_building_visuals(building);view.set_fire_visuals(fire);
    check(view.execute({simulation::CommandType::PlaceHousehold,{112,112}}).accepted,"rear burning paid House");
    const auto rear=*view.world().building_owner_at({112,112});
    for (int tick=0;tick<2000;++tick) view.tick_once();
    zoom_to(view,rear,1.0);
    const auto ground=view.camera().world_to_screen(maps::terrain_ground({113,113},72));
    // At tick2000/BuildingId1 the independent clip selects frame1, source(27,17).
    const int px=static_cast<int>(ground.x+8),py=static_cast<int>(ground.y-83);
    render_no_work(view);auto unobstructed=read_frame(renderer);check(bool(unobstructed),"read owner-visible flame");
    if (pixel(unobstructed.get(),px,py)!=Pixel({255,255,0,255}))
        std::cout<<"composition sample "<<px<<','<<py<<" ground "<<ground.x<<','<<ground.y
                 <<" frame "<<(view.fire_frame_for(rear) ? std::to_string(*view.fire_frame_for(rear)):"missing")
                 <<" rgba "<<nlohmann::json(pixel(unobstructed.get(),px,py)).dump()
                 <<" fallback "<<view.fire_display_stats().fallback_reason<<'\n';
    check(pixel(unobstructed.get(),px,py)==Pixel({255,255,0,255}),"flame hidden inside its own building body");
    check(view.execute({simulation::CommandType::PlaceHousehold,{114,112}}).accepted,"ordinary new foreground House");
    const auto front=*view.world().building_owner_at({114,112});
    render_no_work(view);auto occluded=read_frame(renderer);check(bool(occluded),"read spatial foreground occlusion");
    check(pixel(occluded.get(),px,py)==Pixel({0,0,255,255}),"rear flame painted over genuinely foreground building");
    check(view.world().building_on_fire(rear) && !view.world().building_on_fire(front),"composition test changed fire lifecycle");
    bool running=true;view.handle_event(key(SDLK_F1),running);render_no_work(view);
    const openemperor::scene::Point rear_body{ground.x-60,ground.y-100};
    click(view,rear_body,rear_body);check(view.selected_building()==rear,"rear body could not be selected before effect pick check");
    render_no_work(view);
    const auto snapshot=view.world().snapshot();click(view,{double(px),double(py)},{double(px),double(py)});
    check(view.selected_building()==front && view.world().snapshot()==snapshot,"effect blocked foreground alpha picking or mutated World");
    render_no_work(view);
    const auto selected=view.selected_building();const auto panel=view.layout().panel;
    click(view,{double(px),double(py)},{double(panel.x+panel.w/2),double(panel.y+panel.h/2)});
    check(view.selected_building()==selected && view.world().snapshot()==snapshot,"map press released over UI clicked through effect/panel");
    view.shutdown();check(openemperor::FireSpriteSet::live_texture_count()==0,"composition effect textures leaked");
    std::cout<<"Animated composition: owner-visible flame, foreground body occlusion, existing alpha picking and UI press-release consumption\n";
}
}
int main(int argc,char* argv[]) {
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        check(argc==1 || metal,"usage: fire-overlay-pixel-tests [metal]");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal ? "cocoa":"dummy"),"select isolated video backend");
        check(SDL_Init(SDL_INIT_VIDEO),"initialize SDL");
        auto window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>(
            SDL_CreateWindow("Authored fire pixel regression",960,720,SDL_WINDOW_HIDDEN),SDL_DestroyWindow);
        check(bool(window),"create isolated window");
        auto renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>(
            SDL_CreateRenderer(window.get(),metal ? "metal":"software"),SDL_DestroyRenderer);
        check(bool(renderer),"create requested renderer");
        check(std::string_view(SDL_GetRendererName(renderer.get()))==(metal ? "metal":"software"),"actual requested renderer");
        auto target=std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)>(
            SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,960,720),SDL_DestroyTexture);
        check(bool(target) && SDL_SetRenderTarget(renderer.get(),target.get()),"own pre-Present render target");
        run(window.get(),renderer.get());
        inspector_checks(window.get(),renderer.get());
        animated_checks(window.get(),renderer.get());
        animated_inspector_checks(window.get(),renderer.get());
        animated_composition_checks(window.get(),renderer.get());
        check(SDL_SetRenderTarget(renderer.get(),nullptr),"release target");
        target.reset();renderer.reset();window.reset();SDL_Quit();
        std::cout<<"Production fire pixel regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Fire pixel regression: "<<error.what()<<'\n';SDL_Quit();return 1;
    }
}
