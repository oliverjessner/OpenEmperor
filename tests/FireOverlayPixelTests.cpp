#include "app/SandboxView.h"
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
#include <stdexcept>
#include <string>
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
}
int main() {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),"select isolated dummy video");
        check(SDL_Init(SDL_INIT_VIDEO),"initialize SDL");
        auto window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>(
            SDL_CreateWindow("Authored fire pixel regression",960,720,SDL_WINDOW_HIDDEN),SDL_DestroyWindow);
        check(bool(window),"create isolated window");
        auto renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>(
            SDL_CreateRenderer(window.get(),"software"),SDL_DestroyRenderer);
        check(bool(renderer),"create software renderer");
        auto target=std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)>(
            SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,960,720),SDL_DestroyTexture);
        check(bool(target) && SDL_SetRenderTarget(renderer.get(),target.get()),"own pre-Present render target");
        run(window.get(),renderer.get());
        inspector_checks(window.get(),renderer.get());
        check(SDL_SetRenderTarget(renderer.get(),nullptr),"release target");
        target.reset();renderer.reset();window.reset();SDL_Quit();
        std::cout<<"Production fire pixel regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Fire pixel regression: "<<error.what()<<'\n';SDL_Quit();return 1;
    }
}
