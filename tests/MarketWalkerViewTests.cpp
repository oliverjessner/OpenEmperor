#include "app/SandboxView.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include "renderer/FireSpriteSet.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "renderer/WalkerSpriteSet.h"
#include "EconomyStartViewFixture.h"
#include "FireInspectorWalkerFixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

// Production SandboxView pixels and actual ordinary transports, using only
// independently authored map/container bytes and native/display-flipped schema-4 images.
namespace {
namespace fs=std::filesystem;
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace assets=oe::assets;
namespace perf=oe::performance;
namespace authored=oe::testing::inspector;
using View=oe::SandboxView;
using Json=nlohmann::json;
using Pixel=std::array<std::uint8_t,4>;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
constexpr std::array market_roles{sim::CourierRole::MarketFoodInbound,
    sim::CourierRole::MarketPotteryInbound,sim::CourierRole::MarketFoodDistribution,
    sim::CourierRole::MarketPotteryDistribution};
constexpr std::array<const char*,4> market_names{"MarketFoodInbound","MarketPotteryInbound",
    "MarketFoodDistribution","MarketPotteryDistribution"};
void check(bool yes,const char* why) { if (!yes) throw std::runtime_error(why); }
Surface read(SDL_Renderer* renderer) {
    Surface result{SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface};
    check(bool(result),"private pre-Present target readback failed");return result;
}
Pixel pixel(SDL_Surface* surface,int x,int y) {
    Pixel result{};check(SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]),"pixel read failed");return result;
}
void key(View& view,SDL_Keycode code) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;bool running=true;
    view.handle_event(event,running);check(running,"unexpected test quit");
}
void click(View& view,oe::scene::Point point) {
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=static_cast<float>(point.x);event.button.y=static_cast<float>(point.y);bool running=true;
    view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(event,running);
    check(running,"unexpected click quit");
}
void pure_render(View& view) {
    const auto before=view.world().snapshot();perf::set_enabled(true);perf::reset();
    check(view.render(),"actual Sandbox render failed");
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,"render performed IO/decode/upload/World/navigation work");
    perf::set_enabled(false);check(view.world().snapshot()==before,"render changed complete World state");
}
oe::scene::Point ground(const View& view,sim::CourierId id) {
    const auto position=view.world().courier_position(id);check(position.has_value(),"actual courier position missing");
    // Independent flat-map projection. No renderer pose/anchor is used here.
    return view.camera().world_to_screen({40*(position->x-position->y),20*(position->x+position->y-144)+20});
}
void zoom_to(View& view,sim::CourierId id,double zoom) {
    const auto point=ground(view,id);SDL_Event event{};event.type=SDL_EVENT_MOUSE_WHEEL;
    event.wheel.mouse_x=static_cast<float>(point.x);event.wheel.mouse_y=static_cast<float>(point.y);
    event.wheel.y=static_cast<float>(std::log(zoom/view.camera().zoom)/std::log(1.15));bool running=true;
    view.handle_event(event,running);check(std::abs(view.camera().zoom-zoom)<0.00001,"ordinary wheel zoom failed");
}
struct Fixture {
    oe::testing::economy::Temp map;
    authored::Fixture images;
    fs::path data=map.root/"data",profile=map.root/"market.json";
    Fixture() {
        map.root=fs::canonical(map.root);data=map.root/"data";profile=map.root/"market.json";
        fs::copy_file(images.data/"DATA/walker.sg3",data/"DATA/walker.sg3");
        fs::copy_file(images.data/"DATA/walker.555",data/"DATA/walker.555");
        auto full=authored::Fixture::legacy();full["schema_version"]=4;
        auto role=authored::Fixture::inspector()["roles"]["fire_inspector"];
        role["clip_id"]="authored-supplier";full["roles"]["supplier"]=role;
        role["clip_id"]="authored-distributor";full["roles"]["distributor"]=role;
        authored::Fixture::save(profile,full);
    }
    oe::maps::StoredMapSession session() const {
        return oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8);
    }
    void configure(View& view,bool sprites,const char* save) const {
        view.configure_save(data,"Cities/A.map",map.root/save);
        if (sprites) view.set_walker_visuals(profile,oe::VisualProfileSource::Custom);
    }
};
void equal_documents(View&,View&,const Fixture&,const char*);
// Two native, unequal canvases. Opposing aliases reflect only at display time.
// These authored bytes deliberately have noncenter feet, extreme-column color,
// a transparent corner/interior hole and the supported shadow marker.
void asymmetric_profile(Fixture& fixture) {
    authored::Bytes archive(40680+6*72,0),bitmap{0,0,0,0};
    authored::u32(archive,0,static_cast<std::uint32_t>(archive.size()));
    authored::u32(archive,4,214);authored::u32(archive,12,6);authored::u32(archive,16,6);authored::u32(archive,20,1);
    const std::string group="authored-asymmetric.bmp";
    std::copy(group.begin(),group.end(),archive.begin()+680);authored::u32(archive,680+124,6);
    Json frames=Json::array();
    for(int phase=0;phase<2;++phase) {
        const int w=9+4*phase,h=15+4*phase,fx=3+phase,fy=h-1;
        std::vector<std::uint16_t> raster(static_cast<std::size_t>(w*h),0);
        for(int y=fy-11;y<=fy-5;++y)for(int x=0;x<w;++x)
            raster[static_cast<std::size_t>(y*w+x)]=x<w/2?0x001f:0x03e0;
        for(int y=fy-9;y<=fy-7;++y)for(int x=fx;x<=fx+1;++x)raster[static_cast<std::size_t>(y*w+x)]=0;
        raster[static_cast<std::size_t>((fy-11)*w)]=0; // Asymmetric corner.
        for(int x=fx-1;x<=fx;++x)raster[static_cast<std::size_t>((fy-1)*w+x)]=0x7fff;
        raster[static_cast<std::size_t>((fy-2)*w+w-2)]=0x7c00;
        raster[static_cast<std::size_t>((fy-13)*w+(phase?w-2:1))]=0x7fe0;
        authored::Bytes payload;
        for(int y=0;y<h;++y)for(int x=0;x<w;) {
            if(!raster[static_cast<std::size_t>(y*w+x)]) {
                int n=1;while(x+n<w&&!raster[static_cast<std::size_t>(y*w+x+n)])++n;
                payload.push_back(255);payload.push_back(static_cast<std::uint8_t>(n));x+=n;
            }else {
                int n=1;while(x+n<w&&raster[static_cast<std::size_t>(y*w+x+n)])++n;
                payload.push_back(static_cast<std::uint8_t>(n));
                for(int i=0;i<n;++i){const auto c=raster[static_cast<std::size_t>(y*w+x+i)];payload.push_back(static_cast<std::uint8_t>(c));payload.push_back(static_cast<std::uint8_t>(c>>8U));}x+=n;
            }
        }
        const auto at=40680+static_cast<std::size_t>(1+2*phase)*72;
        authored::u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));authored::u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
        authored::u16(archive,at+20,static_cast<std::uint16_t>(w));authored::u16(archive,at+22,static_cast<std::uint16_t>(h));authored::u16(archive,at+50,256);archive[at+59]=1;
        bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        for(const bool flip:{false,true})frames.push_back({{"alias",std::string(flip?"reflected-":"native-")+std::to_string(phase)},
            {"archive","DATA/walker.sg3"},{"image_index",1+2*phase},{"foot_anchor",{flip?w-fx:fx,fy}},{"flip_x",flip}});
    }
    authored::write(fixture.data/"DATA/walker.sg3",archive);authored::write(fixture.data/"DATA/walker.555",bitmap);
    auto full=authored::Fixture::legacy();full["schema_version"]=4;
    for(auto& [name,role]:full["roles"].items())role["frames"][0]["foot_anchor"]={3,14};
    Json role={{"ticks_per_frame",2},{"clip_id","authored-asymmetric-flip"},{"evidence","Independent native bytes and explicit reflected aliases."},
        {"frames",frames},{"idle","native-0"},{"clips",{{"pos_x",{"native-0","native-1"}},{"neg_y",{"native-0","native-1"}},
          {"neg_x",{"reflected-0","reflected-1"}},{"pos_y",{"reflected-0","reflected-1"}}}}};
    full["roles"]["supplier"]=role;full["roles"]["distributor"]=role;authored::Fixture::save(fixture.profile,full);
}
void reflected_view_checks(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;asymmetric_profile(fixture);const auto profile=assets::load_walker_visual_profile(fixture.data,fixture.profile);
    View view(fixture.session(),true,sim::RulesProfile::CityV16,3),plain(fixture.session(),true,sim::RulesProfile::CityV16,3);
    fixture.configure(view,true,"reflected.json");fixture.configure(plain,false,"reflected-control.json");view.initialize(window,renderer);plain.initialize(window,renderer);
    check(view.walker_texture_count()==2,"display aliases duplicated native textures");
    zoom_to(view,view.world().couriers().back().id,1.0);key(view,SDLK_F1);key(view,SDLK_5);
    bool native=false,reflected=false,holes=false,zoomed=false,preview=false,same_frame=false;
    std::array<bool,2> canvas{};
    for(int tick=0;tick<650;++tick) {
        view.tick_once();plain.tick_once();check(view.world().snapshot()==plain.world().snapshot(),"reflected presentation changed complete tick state");
        pure_render(view);auto output=read(renderer);bool submitted_native=false,submitted_flip=false;
        for(const auto& row:view.walker_diagnostics()) {
            if(!row.sprite_drawn||(row.family!="supplier"&&row.family!="distributor"))continue;
            check(row.profile_source=="custom","configured custom family diagnostic lost provenance");
            submitted_native=submitted_native||!row.flip_x;submitted_flip=submitted_flip||row.flip_x;
            const auto& c=view.world().courier(row.id);const auto pose=oe::walker_pose(c,view.world().ticks(),profile);
            if(!pose.frame||!pose.role)continue;
            const auto& frame=profile.find(*pose.role)->frames[*pose.frame];const int phase=frame.id.image_index==3?1:0;
            const int w=9+4*phase,fy=14+4*phase,fx=3+phase;
            const auto at=ground(view,row.id);const auto sample=[&](int sx,int sy) {
                const int dx=row.flip_x?w-1-sx:sx;const double ax=row.flip_x?w-fx:fx,z=view.camera().zoom;
                return oe::scene::Point{at.x+(dx+0.5-ax)*z,at.y+(sy+0.5-fy)*z};
            };
            const auto left=sample(0,fy-6),right=sample(w-1,fy-6);
            const auto visible=[&](oe::scene::Point p,Pixel color) {return p.x>=view.layout().map.x&&p.x<view.layout().map.x+view.layout().map.w&&p.y>=view.layout().map.y&&p.y<view.layout().map.y+view.layout().map.h&&pixel(output.get(),static_cast<int>(p.x),static_cast<int>(p.y))==color;};
            if(!visible(left,{0,0,255,255})||!visible(right,{0,255,0,255}))continue;
            // Shared silhouettes can overlap another courier. Require both
            // independently colored edge hits to identify this actual instance
            // before accepting it as an unobscured alpha/pixel witness.
            click(view,left);if(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(row.id))continue;
            click(view,right);if(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(row.id))continue;
            native=native||!row.flip_x;reflected=reflected||row.flip_x;canvas[static_cast<std::size_t>(phase)]=true;
            const auto hole=sample(fx,fy-8);click(view,hole);check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(row.id),"reflected transparent interior hole retained a body hit");holes=true;
            if(!zoomed&&row.flip_x) {
                for(const double z:{1.15,2.0,4.0,1.0}) {zoom_to(view,row.id,z);pure_render(view);output=read(renderer);
                    check(visible(sample(0,fy-6),{0,0,255,255})&&visible(sample(w-1,fy-6),{0,255,0,255}),"zoom changed reflected foot or extreme source column");
                    click(view,sample(0,fy-6));check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(row.id),"zoom reflected alpha hit mismatch");}
                zoomed=true;
            }
            if(!preview&&row.flip_x) {
                key(view,SDLK_F3);for(int i=0;i<4;++i)key(view,SDLK_V);key(view,SDLK_Q);pure_render(view);const auto diagnostic=read(renderer);
                // F3 uses panel ground (map+8+470-105,map+8+300) and a native 9x15 image.
                const double gx=view.layout().map.x+373,gy=view.layout().map.y+308;
                check(pixel(diagnostic.get(),static_cast<int>(gx+6),static_cast<int>(gy-24))==Pixel{0,0,255,255},"F3 4x preview did not display reflected blue side");
                check(pixel(diagnostic.get(),static_cast<int>(gx-18),static_cast<int>(gy-24))==Pixel{0,255,0,255},"F3 4x preview used native orientation");
                key(view,SDLK_X);pure_render(view);const auto preview_one=read(renderer);
                check(pixel(preview_one.get(),static_cast<int>(gx+1),static_cast<int>(gy-6))==Pixel{0,0,255,255}&&
                    pixel(preview_one.get(),static_cast<int>(gx-5),static_cast<int>(gy-6))==Pixel{0,255,0,255},"F3 1x reflected orientation mismatch");
                key(view,SDLK_F3);const auto stale=view.walker_diagnostics();check(std::none_of(stale.begin(),stale.end(),[](const auto& r){return r.submitted;}),"closing F3 retained stale body hits before redraw");
                pure_render(view);preview=true;
            }
        }
        same_frame=same_frame||(submitted_native&&submitted_flip);
        if(native&&reflected&&holes&&zoomed&&preview&&same_frame&&canvas[0]&&canvas[1])break;
    }
    check(native&&reflected&&holes&&zoomed&&preview&&same_frame&&canvas[0]&&canvas[1],"actual native/reflected variable-canvas route witnesses incomplete");
    equal_documents(view,plain,fixture,"reflected-final");view.shutdown();plain.shutdown();
    check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,"reflected session leaked textures");
}
void equal_documents(View& drawn,View& plain,const Fixture& fixture,const char* tag) {
    const auto a=fixture.map.root/(std::string(tag)+"-drawn.json"),b=fixture.map.root/(std::string(tag)+"-plain.json");
    oe::persistence::write_save(a,drawn.capture_save_document(),fixture.data,drawn.buildable_mask());
    oe::persistence::write_save(b,plain.capture_save_document(),fixture.data,plain.buildable_mask());
    std::ifstream left(a),right(b);Json l,r;left>>l;right>>r;
    check(l==r,"full serialized SaveDocuments differ with graphics enabled/disabled");
    check(l.at("schema_version")==19,"paid rule-3 save did not retain schema 19");
}
std::size_t live_market(const View& view) {
    return static_cast<std::size_t>(std::count_if(view.world().couriers().begin(),view.world().couriers().end(),
        [](const auto& c){return std::find(market_roles.begin(),market_roles.end(),c.role)!=market_roles.end()&&oe::walker_live_visible(c);}));
}
Pixel color(assets::StorageDirection direction) {
    constexpr std::array<Pixel,4> colors{{{0,0,255,255},{0,255,0,255},{255,255,0,255},{0,255,255,255}}};
    return colors[assets::direction_index(direction)];
}
bool torso(SDL_Surface* output,const View& view,sim::CourierId id,Pixel expected) {
    const auto at=ground(view,id);const auto zoom=view.camera().zoom;
    return pixel(output,static_cast<int>(at.x),static_cast<int>(at.y-8*zoom))==expected;
}
double inherited_marker_shift(sim::CourierId id) {
    // F2 keeps the pre-existing diagnostic-marker offsets. Family selection
    // and animated sprite placement never use this numeric-ID convention.
    const auto number=static_cast<unsigned>(id);
    return number==1?-5:number==2?5:number==3?0:number==4?-10:number==5?10:14;
}
void idle_checks(View& view) {
    const auto before=view.market_walker_display_stats();pure_render(view);const auto after=view.market_walker_display_stats();
    check(live_market(view)==0&&after.draws==before.draws&&after.fallback_draws==before.fallback_draws,
        "idle Market transports submitted sprites/markers");
    for (const auto& c:view.world().couriers()) if (std::find(market_roles.begin(),market_roles.end(),c.role)!=market_roles.end()) {
        check(c.phase==sim::CourierPhase::IdleAtWorkshop&&!oe::walker_live_visible(c),"fixture is not actually idle");
        const auto at=ground(view,c.id);click(view,{at.x,at.y-8});
        check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(c.id),"hidden idle retained a sprite hit");
        click(view,{at.x+inherited_marker_shift(c.id),at.y});
        check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(c.id),"hidden idle retained a marker hit");
    }
}
void production_checks(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto profile=assets::load_walker_visual_profile(fixture.data,fixture.profile);
    View view(fixture.session(),true,sim::RulesProfile::CityV16,3),plain(fixture.session(),true,sim::RulesProfile::CityV16,3);
    fixture.configure(view,true,"moving.json");fixture.configure(plain,false,"plain.json");
    view.initialize(window,renderer);plain.initialize(window,renderer);
    const auto equal=[&] { check(view.world().snapshot()==plain.world().snapshot(),"graphics changed complete authoritative snapshot"); };
    const auto step=[&] { view.tick_once();plain.tick_once();equal(); };
    const auto command=[&](sim::Command input) {
        const auto a=view.execute(input),b=plain.execute(input);
        check(a.accepted&&b.accepted&&a.changed==b.changed,"paired ordinary paid command rejected");equal();
    };
    equal();check(view.world().ticks()==0&&view.world().treasury()==20&&view.world().construction_spent_total()==1280&&
        view.world().workforce_supply()==24&&view.world().workforce_used()==24,"normal paid starter changed");
    check(view.market_walker_display_stats().active&&view.walker_texture_count()==8,"complete schema-4 families not active/deduplicated");
    equal_documents(view,plain,fixture,"initial");
    const auto any=view.world().couriers().back().id;zoom_to(view,any,1.0);zoom_to(plain,any,1.0);
    idle_checks(view);key(view,SDLK_F2);idle_checks(view);key(view,SDLK_F2);idle_checks(plain);equal();
    // Every subsequent render uses already prepared bytes. Removing source
    // files here is an explicit load-time vs per-frame regression guard.
    fs::remove(fixture.data/"DATA/walker.sg3");fs::remove(fixture.data/"DATA/walker.555");fs::remove(fixture.profile);
    std::array<bool,4> loaded{},returned{};
    std::array<std::array<bool,4>,2> pixel_directions{};
    std::array<std::array<bool,2>,2> gait{};
    bool simultaneous=false,picked=false,marker_picked=false,transparent=false,zoomed=false,saved_active=false,paused_owner=false;
    std::optional<sim::BuildingId> market;std::uint64_t resume_at=0;
    std::optional<sim::CourierId> interrupted;std::optional<sim::Cell> removed;
    bool begun_wait=false,static_wait=false,repaired=false;int waiting_ticks=0;
    std::vector<Pixel> waiting_pixels;
    for (int n=0;n<1200;++n) {
        step();const auto before=view.market_walker_display_stats();pure_render(view);
        const auto after=view.market_walker_display_stats();
        check(after.draws-before.draws==live_market(view)&&after.fallback_draws==before.fallback_draws,
            "active Food/Market transport drew a fallback square or duplicate instance");
        const auto output=read(renderer);bool food=false,pottery=false;
        for (const auto& c:view.world().couriers()) {
            const auto found=std::find(market_roles.begin(),market_roles.end(),c.role);if(found==market_roles.end())continue;
            const auto r=static_cast<std::size_t>(found-market_roles.begin());
            check(oe::walker_live_visible(c)==(c.phase!=sim::CourierPhase::IdleAtWorkshop),"actual transport visibility changed");
            if(c.phase==sim::CourierPhase::IdleAtWorkshop)continue;
            const auto pose=oe::walker_pose(c,view.world().ticks(),profile);check(pose.frame.has_value(),"real route has no authored frame");
            if(c.phase==sim::CourierPhase::ToWarehouse) { check(c.cargo>0&&pose.loaded,"actual outbound cargo lost");loaded[r]=true; }
            if(c.phase==sim::CourierPhase::Returning) { check(c.cargo==0&&!pose.loaded,"empty return invented cargo");returned[r]=true; }
            food=food||(r==2&&c.phase==sim::CourierPhase::ToWarehouse);pottery=pottery||(r==3&&c.phase==sim::CourierPhase::ToWarehouse);
            if(!pose.moving||!pose.direction)continue;
            const auto expected=color(*pose.direction);if(!torso(output.get(),view,c.id,expected))continue;
            const std::size_t family=*pose.role==assets::WalkerVisualRole::Supplier?0:1;
            pixel_directions[family][assets::direction_index(*pose.direction)]=true;
            const auto at=ground(view,c.id);const auto& visual=*profile.find(*pose.role);
            const auto& frame=visual.frames[*pose.frame];
            const bool phase1=frame.id.image_index%4==3;
            const double limb=phase1?4.0:-3.0;
            if(pixel(output.get(),static_cast<int>(at.x+limb),static_cast<int>(at.y-9))==expected)gait[family][phase1?1:0]=true;
            if(!saved_active) {
                const auto saved=view.world().snapshot();const auto phase=*pose.frame;const auto id=c.id;
                key(view,SDLK_F5);key(plain,SDLK_F5);
                equal_documents(view,plain,fixture,"active");step();step();key(view,SDLK_F9);
                key(plain,SDLK_F9);
                check(view.world().snapshot()==saved&&oe::walker_pose(view.world().courier(id),view.world().ticks(),profile).frame==phase,
                    "active normal F5/F9 lost complete route/cargo/reservations/tick/frame");
                saved_active=true;equal();break;
            }
            const bool clear_road=c.path_vertex>=2&&c.path_vertex<c.path.size()&&
                view.world().object_at(c.path[c.path_vertex])==sim::Object::Road;
            if(!picked||(!marker_picked&&clear_road)) {
                key(view,SDLK_F1);pure_render(view);click(view,{at.x,at.y-8});
                const bool actual_hit=view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(c.id);
                if(actual_hit) {
                    std::string inspection;for(const auto& line:view.inspection_lines())inspection+=line+" ";
                    check(inspection.find(std::string("Role: ")+market_names[r])!=std::string::npos&&
                        inspection.find("Owner: "+std::to_string(static_cast<unsigned>(c.owner)))!=std::string::npos&&
                        inspection.find("target: "+std::to_string(static_cast<unsigned>(c.target)))!=std::string::npos&&
                        inspection.find("Cargo: "+std::to_string(c.cargo))!=std::string::npos&&
                        inspection.find(r==0||r==2?"Goods: Food":"Goods: Pottery")!=std::string::npos&&
                        inspection.find("Curated clip: authored-")!=std::string::npos,
                        "selected visible transport lost authoritative logical role/owner/target/goods/cargo/clip");
                }
                picked=picked||actual_hit;
                pure_render(view);click(view,{at.x-5,at.y-15});
                transparent=transparent||view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(c.id);
                if(!marker_picked&&clear_road) {
                    const auto old=view.market_walker_display_stats();key(view,SDLK_F2);pure_render(view);
                    const auto markers=view.market_walker_display_stats();
                    check(markers.configured&&!markers.active&&markers.draws==old.draws&&
                        markers.fallback_draws-old.fallback_draws==live_market(view),
                        "F2 comparison lost active transport or exposed idle instances");
                    click(view,{at.x+inherited_marker_shift(c.id),at.y});
                    check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(c.id),
                        "visible active comparison marker did not retain its hit");
                    marker_picked=true;
                    key(view,SDLK_F2);pure_render(view);
                }
                key(view,SDLK_F1);equal();
            }
            if(!zoomed&&picked) {
                for(const double zoom:{1.15,2.0,4.0,1.0}) {
                    zoom_to(view,c.id,zoom);pure_render(view);const auto scaled=read(renderer);
                    check(torso(scaled.get(),view,c.id,expected),"camera zoom lost actual sprite torso/anchor");
                }
                const auto saved=view.world().snapshot();pure_render(view);const auto still=read(renderer);const auto point=ground(view,c.id);
                view.update(0);pure_render(view);const auto again=read(renderer);
                for(int y=-12;y<0;++y)for(int x=-5;x<5;++x)
                    check(pixel(still.get(),static_cast<int>(point.x)+x,static_cast<int>(point.y)+y)==
                        pixel(again.get(),static_cast<int>(point.x)+x,static_cast<int>(point.y)+y),"paused render changed gait pixels");
                check(view.world().snapshot()==saved,"paused render/update changed full state");zoomed=true;
            }
        }
        simultaneous=simultaneous||(food&&pottery);
        if(!paused_owner&&food&&pottery) {
            for(const auto& c:view.world().couriers())if(c.role==sim::CourierRole::MarketFoodDistribution)market=c.owner;
            command(sim::set_building_operation(*market,false));resume_at=view.world().ticks()+20;paused_owner=true;
            check(std::any_of(view.world().couriers().begin(),view.world().couriers().end(),[&](const auto& c) {
                return c.owner==*market&&c.phase!=sim::CourierPhase::IdleAtWorkshop&&oe::walker_live_visible(c);
            }),"pausing the owner hid an already dispatched distributor");
        }
        if(market&&!view.world().building(*market).operating_enabled&&view.world().ticks()>=resume_at)
            command(sim::set_building_operation(*market,true));
        if(saved_active&&!interrupted)for(const auto& c:view.world().couriers()) {
            if(c.role!=sim::CourierRole::MarketFoodInbound||c.phase!=sim::CourierPhase::ToWarehouse||c.edge_progress!=1)continue;
            for(std::size_t i=c.path_vertex+2;i<c.path.size();++i) {
                const auto cell=c.path[i];if(!view.world().validate({sim::CommandType::RemoveRoad,cell}).accepted)continue;
                const auto id=c.id;const auto at=view.world().courier_position(id);
                command({sim::CommandType::RemoveRoad,cell});interrupted=id;removed=cell;
                const auto& waiting=view.world().courier(id);const auto pose=oe::walker_pose(waiting,view.world().ticks(),profile);
                check(waiting.route_pending&&waiting.edge_progress==1&&pose.moving&&
                    oe::walker_live_visible(waiting)&&view.world().courier_position(id)==at,
                    "allowed road break discarded begun edge/cargo/visible movement");
                begun_wait=true;break;
            }
            if(interrupted)break;
        }
        if(interrupted&&!repaired) {
            const auto& waiting=view.world().courier(*interrupted);
            if(waiting.route_pending&&waiting.edge_progress==0) {
                const auto pose=oe::walker_pose(waiting,view.world().ticks(),profile);
                check(oe::walker_live_visible(waiting)&&!pose.moving&&pose.frame.has_value(),
                    "one-cell waiting transport vanished or animated in place");
                // Its actual retained position and a ground-relative crop stay
                // fixed while World ticks continue. Other transports remain real.
                pure_render(view);const auto image=read(renderer);const auto at=ground(view,*interrupted);
                std::vector<Pixel> crop;
                for(int y=-12;y<-5;++y)for(int x=-4;x<3;++x)
                    crop.push_back(pixel(image.get(),static_cast<int>(at.x)+x,static_cast<int>(at.y)+y));
                if(waiting_pixels.empty())waiting_pixels=crop;
                else check(crop==waiting_pixels,"stopped real transport changed gait pixels as World tick advanced");
                check(waiting.cargo>0&&waiting.reserved==waiting.cargo,"waiting lost actual cargo/reservation");
                static_wait=true;++waiting_ticks;
                if(waiting_ticks==1) {
                    const auto saved=view.world().snapshot();key(view,SDLK_F5);key(plain,SDLK_F5);
                    equal_documents(view,plain,fixture,"waiting");key(view,SDLK_F9);key(plain,SDLK_F9);
                    check(view.world().snapshot()==saved,"normal waiting save/load lost complete state");equal();
                }
                if(waiting_ticks==12) { command({sim::CommandType::PlaceRoad,*removed});repaired=true; }
            }
        }
        if(view.world().ticks()==400||view.world().ticks()==800)equal_documents(view,plain,fixture,"boundary");
    }
    check(std::all_of(loaded.begin(),loaded.end(),[](bool x){return x;})&&
        std::all_of(returned.begin(),returned.end(),[](bool x){return x;})&&
        std::all_of(pixel_directions.begin(),pixel_directions.end(),[](const auto& row) {
            return std::all_of(row.begin(),row.end(),[](bool x){return x;});
        })&&std::all_of(gait.begin(),gait.end(),[](const auto& row){return row[0]&&row[1];})&&
        simultaneous&&picked&&marker_picked&&transparent&&zoomed&&saved_active&&paused_owner&&begun_wait&&static_wait&&repaired&&
        view.world().taxes_collected_total()>0,
        "production transport pixel/direction/return/picking/pause/save/tax acceptance incomplete");
    equal_documents(view,plain,fixture,"final");
    view.shutdown();plain.shutdown();
    check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::FireSpriteSet::live_texture_count()==0&&
        oe::StoredGraphicsRenderer::live_texture_count()==0,"normal sessions leaked owned textures");
}
void actual_understaffed_trip(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto profile=assets::load_walker_visual_profile(fixture.data,fixture.profile);
    View view(fixture.session(),false,sim::RulesProfile::CityV11,3);fixture.configure(view,true,"understaffed.json");
    view.initialize(window,renderer);auto control=sim::World::restore(view.world().snapshot(),view.buildable_mask());
    const auto execute=[&](sim::Command command) {
        const auto actual=view.execute(command),plain=control.execute(command);
        check(actual.accepted&&plain.accepted&&actual.changed==plain.changed&&view.world().snapshot()==control.snapshot(),
            "paid under-staffing fixture command changed complete state");
    };
    const auto put=[&](sim::CommandType type,int x,int y) {
        const sim::Cell cell{94+x,106+y};execute({type,cell});return *view.world().building_owner_at(cell);
    };
    for(const int x:{1,4,7})put(sim::CommandType::PlaceHousehold,x,5);
    const auto clay=put(sim::CommandType::PlaceClaySource,0,1),pot=put(sim::CommandType::PlacePottery,3,1);
    put(sim::CommandType::PlaceWarehouse,6,1);put(sim::CommandType::PlaceFarm,9,4);
    put(sim::CommandType::PlaceServicePost,18,1);const auto market=put(sim::CommandType::PlaceMarket,14,4);
    for(int x=0;x<=18;++x)execute({sim::CommandType::PlaceRoad,{94+x,109}});
    for(const int x:{1,4,7})execute({sim::CommandType::PlaceRoad,{94+x,110}});
    execute({sim::CommandType::PlaceRoad,{112,108}});
    check(view.world().workforce_supply()==18&&!view.world().building_staffed(market),"fixture did not genuinely exhaust workforce");
    const auto step=[&] { view.tick_once();control.tick();check(view.world().snapshot()==control.snapshot(),"understaffed pixels changed actual tick"); };
    int budget=400;while(view.world().building(market).food_stock==0&&budget-->0)step();
    check(budget>0,"ordinary Farm did not supply genuinely unstaffed Market");
    execute(sim::set_building_operation(clay,false));execute(sim::set_building_operation(pot,false));
    check(view.world().building_staffed(market),"ordinary operation priority did not staff Market");
    std::optional<sim::CourierId> id;budget=200;
    while(!id&&budget-->0) {
        step();for(const auto& c:view.world().couriers())if(c.role==sim::CourierRole::MarketFoodDistribution&&
            c.phase==sim::CourierPhase::ToWarehouse&&c.path_vertex>=2&&c.edge_progress==1)id=c.id;
    }
    check(id.has_value(),"ordinary staffed Market did not dispatch a real distributor");
    execute(sim::set_building_operation(clay,true));execute(sim::set_building_operation(pot,true));
    check(!view.world().building_staffed(market)&&view.world().workers_assigned(market)==0,
        "normal priority failed to remove actual Market workers");
    zoom_to(view,*id,1.0);pure_render(view);const auto pose=oe::walker_pose(view.world().courier(*id),view.world().ticks(),profile);
    check(pose.moving&&pose.direction&&oe::walker_live_visible(view.world().courier(*id)),
        "genuinely understaffed dispatched transport disappeared/froze");
    bool empty_return=false,unobscured_outbound_pixel=false;budget=300;
    while(view.world().courier(*id).phase!=sim::CourierPhase::IdleAtWorkshop&&budget-->0) {
        step();const auto& c=view.world().courier(*id);const auto before=view.market_walker_display_stats();
        const auto distributor_before=view.walker_display_stats().roles[assets::walker_role_index(assets::WalkerVisualRole::Distributor)].draws;
        pure_render(view);
        const auto after=view.market_walker_display_stats();
        check(after.draws>=before.draws&&after.draws-before.draws<=live_market(view)&&after.fallback_draws==before.fallback_draws,
            "understaffed active trip lost actual family draw");
        const auto at=ground(view,*id);const auto& map=view.layout().map;
        if(oe::walker_live_visible(c)&&at.x>=map.x+10&&at.x<map.x+map.w-10&&at.y>=map.y+20&&at.y<map.y+map.h)
            check(view.walker_display_stats().roles[assets::walker_role_index(assets::WalkerVisualRole::Distributor)].draws>
                distributor_before,"on-screen genuinely understaffed distributor did not submit a sprite");
        const auto actual_pose=oe::walker_pose(c,view.world().ticks(),profile);
        if(c.phase==sim::CourierPhase::ToWarehouse&&actual_pose.moving&&actual_pose.direction) {
            const auto output=read(renderer);
            unobscured_outbound_pixel=unobscured_outbound_pixel||torso(output.get(),view,*id,color(*actual_pose.direction));
        }
        if(c.phase==sim::CourierPhase::Returning) { empty_return=true;check(c.cargo==0&&oe::walker_live_visible(c),"understaffed empty return vanished"); }
    }
    check(budget>0&&empty_return&&unobscured_outbound_pixel&&!oe::walker_live_visible(view.world().courier(*id)),
        "understaffed actual outbound pixels/arrival/empty return/home-idle visibility incomplete");
    view.shutdown();check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,
        "understaffed session leaked textures");
}
void actual_legacy_food(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto profile=assets::load_walker_visual_profile(fixture.data,fixture.profile);
    View view(fixture.session(),false,sim::RulesProfile::CityV7,1);fixture.configure(view,true,"legacy-food.json");
    view.initialize(window,renderer);auto control=sim::World::restore(view.world().snapshot(),view.buildable_mask());
    const auto command=[&](sim::Command input) {
        const auto a=view.execute(input),b=control.execute(input);
        check(a.accepted&&b.accepted&&a.changed==b.changed&&view.world().snapshot()==control.snapshot(),
            "legacy paid commands differ with presentation");
    };
    const auto put=[&](sim::CommandType type,int x,int y){command({type,{96+x,108+y}});};
    put(sim::CommandType::PlaceClaySource,1,2);put(sim::CommandType::PlacePottery,4,2);
    put(sim::CommandType::PlaceWarehouse,7,2);put(sim::CommandType::PlaceFarm,9,5);
    put(sim::CommandType::PlaceHousehold,12,2);put(sim::CommandType::PlaceHousehold,12,5);
    for(int x=1;x<=12;++x)put(sim::CommandType::PlaceRoad,x,3);
    put(sim::CommandType::PlaceRoad,9,4);put(sim::CommandType::PlaceRoad,12,4);
    std::optional<sim::CourierId> id;
    for(const auto& c:view.world().couriers())if(c.role==sim::CourierRole::Food)id=c.id;
    check(id&&view.world().courier(*id).phase==sim::CourierPhase::IdleAtWorkshop&&
        oe::walker_live_visible(view.world().courier(*id)),"legacy Food idle visibility was changed");
    zoom_to(view,*id,1.0);
    bool loaded=false,returned=false,pixels=false,saved=false;std::array<bool,4> directions{};
    for(int i=0;i<1800;++i) {
        view.tick_once();control.tick();check(view.world().snapshot()==control.snapshot(),"legacy Food full tick state changed");
        const auto& c=view.world().courier(*id);const auto pose=oe::walker_pose(c,view.world().ticks(),profile);
        check(pose.role==assets::WalkerVisualRole::Supplier&&pose.frame&&oe::walker_live_visible(c),"legacy direct Food lost its family/live instance");
        if(c.phase==sim::CourierPhase::ToWarehouse) {
            check(c.cargo>0&&view.world().building(c.owner).kind==sim::Object::Farm&&
                view.world().building(c.target).kind==sim::Object::Household,"legacy Food gained a Market target/owner or invented cargo");loaded=true;
        }
        if(c.phase==sim::CourierPhase::Returning) { check(c.cargo==0,"legacy empty return retained cargo");returned=true; }
        if(i%5==0) {
            pure_render(view);const auto output=read(renderer);
            if(pose.moving&&pose.direction) {
                directions[assets::direction_index(*pose.direction)]=true;
                pixels=pixels||torso(output.get(),view,*id,color(*pose.direction));
            }
        }
        if(!saved&&c.phase==sim::CourierPhase::ToWarehouse&&c.edge_progress==2) {
            const auto snapshot=view.world().snapshot();const auto frame=pose.frame;key(view,SDLK_F5);
            key(view,SDLK_F9);control=sim::World::restore(snapshot,view.buildable_mask());
            check(view.world().snapshot()==snapshot&&oe::walker_pose(view.world().courier(*id),view.world().ticks(),profile).frame==frame,
                "legacy active normal save/load changed state or World-tick phase");saved=true;
        }
    }
    check(loaded&&returned&&pixels&&saved&&view.world().taxes_collected_total()>0&&
        std::all_of(directions.begin(),directions.end(),[](bool x){return x;}),"actual legacy direct Food delivery/return/pixels/save/tax acceptance incomplete");
    const auto drawn=fixture.map.root/"legacy-drawn.json",plain=fixture.map.root/"legacy-control.json";
    oe::persistence::write_save(drawn,view.capture_save_document(),fixture.data,view.buildable_mask());
    oe::persistence::write_save(plain,oe::persistence::make_document(fixture.data,"Cities/A.map",view.buildable_mask(),control),
        fixture.data,view.buildable_mask());
    std::ifstream a(drawn),b(plain);Json actual,expected;a>>actual;b>>expected;
    check(actual==expected,"legacy full serialized SaveDocument changed with Supplier presentation");
    view.shutdown();check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,
        "legacy Food session leaked textures");
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const bool metal=argc==2&&std::string_view(argv[1])=="metal";check(argc==1||metal,"usage: Market view tests [metal]");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal?"cocoa":"dummy")&&
            SDL_SetHint(SDL_HINT_RENDER_DRIVER,metal?"metal":"software")&&SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
        check(SDL_CreateWindowAndRenderer("Authored Food/Market production pixels",1280,720,SDL_WINDOW_HIDDEN,&window,&renderer),"SDL renderer");
        check(std::string_view(SDL_GetRendererName(renderer))==(metal?"metal":"software"),"actual renderer mismatch");
        auto target=std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)>(
            SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,720),SDL_DestroyTexture);
        check(bool(target)&&SDL_SetRenderTarget(renderer,target.get()),"private before-Present target");
        production_checks(window,renderer);reflected_view_checks(window,renderer);actual_understaffed_trip(window,renderer);actual_legacy_food(window,renderer);
        check(SDL_SetRenderTarget(renderer,nullptr),"release private target");target.reset();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Production Food/Market paid routes, shared alpha painter, pixels, hidden idle, pause and full SaveDocument neutrality PASS\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
