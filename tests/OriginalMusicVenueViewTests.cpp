// Production SandboxView, authored map/model/pixels, paid commands and real ticks.
// Scripted SDL captures are separate from native delivery and human acceptance.
#include "EconomyStartViewFixture.h"
#include "OriginalMusicVenueFixture.h"
#include "core/PerformanceDiagnostics.h"
#include "app/WalkerPose.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iterator>
#include <memory>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace maps=oe::maps;
namespace save=oe::persistence;
namespace perf=oe::performance;
namespace fs=std::filesystem;
namespace q=oe_music_venue_fixture;
using q::check;
using Point=oe::scene::Point;
using Json=nlohmann::json;
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Unexpected scripted quit");
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);
}
Point screen_ground(const oe::SandboxView& view,sim::Position position,unsigned border) {
    return view.camera().world_to_screen({(position.x-position.y)*40,
        (position.x+position.y-2*border)*20+20});
}
Point window_point(SDL_Renderer* renderer,Point screen) {
    SDL_FPoint point{};check(SDL_RenderCoordinatesToWindow(renderer,float(screen.x),float(screen.y),&point.x,&point.y),
        "Invert production render coordinates");return {point.x,point.y};
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point screen) {
    const auto point=window_point(renderer,screen);
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=float(point.x);e.button.y=float(point.y);event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);
}
void click_button(oe::SandboxView& view,SDL_Renderer* renderer,oe::sandbox_ui::Action action) {
    const auto& buttons=view.layout().buttons;
    const auto found=std::find_if(buttons.begin(),buttons.end(),[&](const auto& b){return b.action==action;});
    check(found!=buttons.end()&&found->enabled,"Missing/disabled actual Theatre tool button");
    click(view,renderer,{found->rect.x+found->rect.w*.5,found->rect.y+found->rect.h*.5});
}
void center(oe::SandboxView& view,Point aim) {
    for(int n=0;n<300;++n) {
        const auto map=view.layout().map;const auto actual=view.camera().world_to_screen(aim);
        const double dx=map.x+map.w*.5-actual.x,dy=map.y+map.h*.5-actual.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else{held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused ordinary camera did not converge");
}
void focus(oe::SandboxView& view,sim::Position point,unsigned border) {
    center(view,{(point.x-point.y)*40,(point.x+point.y-2*border)*20+20});
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;const auto point=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    for(int n=0;n<3&&std::abs(view.camera().zoom-wanted)>1e-9;++n) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=float(point.x);e.wheel.mouse_y=float(point.y);
        e.wheel.y=float(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0,{});
    }
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Actual zoom differs from literal matrix zoom");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    check(SDL_GetRenderTarget(renderer)==nullptr,"Capture requires normal window target first");
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Read actual output dimensions");
    const auto world=view.world().snapshot();const auto camera=view.camera();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Allocate owned same-size RGBA32 target");
    struct Restore {SDL_Renderer* renderer;~Restore(){SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    check(view.render()&&SDL_GetRenderTarget(renderer)==target.get(),"Render production frame before Present/readback");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(frame&&frame->w==width&&frame->h==height&&SDL_SavePNG(frame.get(),path.string().c_str()),"Write complete actual PNG");
    check(view.world().snapshot()==world&&view.camera().zoom==camera.zoom&&
        view.camera().offset.x==camera.offset.x&&view.camera().offset.y==camera.offset.y,"Capture mutated World/camera");
}
bool contains(const std::vector<std::string>& lines,const std::string& text) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(text)!=std::string::npos;});
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id,unsigned border) {
    view.set_tool(5);const auto& body=view.world().building(id);
    const auto size=sim::building_footprint(view.world().profile(),view.world().rule_version(),body.kind);
    focus(view,{body.cell.x+(size.width-1)*.5,body.cell.y+(size.height-1)*.5},border);
    check(view.render(),"Render actual building picking frame");
    const auto point=screen_ground(view,{double(body.cell.x+(size.width-1)/2),double(body.cell.y+(size.height-1)/2)},border);
    click(view,renderer,point);check(view.selected_building()==id,"Actual visible footprint did not select its paid body");
}
template<class P>void until(oe::SandboxView& view,P done,const std::string& message,int limit=1600) {
    for(int n=0;n<limit&&!done();++n)view.tick_once();
    check(done(),message+"; tick="+std::to_string(view.world().ticks())+"; "+view.last_message());q::invariants(view.world());
}
}

int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const auto output=fs::current_path()/".local/oe56"/("music-view-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(output);
        oe::testing::economy::Temp temp;const auto data=temp.root/"data";
        oe_production_fixture::write_model(data);q::f::write_text(data/"Model/EmperorBuildingModels.txt",q::authored_model());
        const auto map_before=q::f::read_text(data/"Cities/A.map"),model_before=q::f::read_text(data/"Model/EmperorBuildingModels.txt");
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        window=SDL_CreateWindow("Paid Music performer to Theatre",1280,800,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);check(window,"Create production window");
        renderer=SDL_CreateRenderer(window,"software");check(renderer,"Create software renderer");
        auto session=maps::load_stored_map_session(data,"Cities/A.map",maps::FootprintPolicy::EdgeByte4x4Preview,
            maps::StoredGraphicsProfile::Slot8,maps::GreatWallPresentationMode::Automatic,{},true);
        const auto border=session.plan.border;
        auto owned=oe::SandboxView::create_original_music_venues(std::move(session));auto& view=*owned;
        const auto manual=output/"active-performer.json";view.configure_save(data,"Cities/A.map",manual);view.initialize(window,renderer);
        check(view.paused()&&view.world().original_music_venues_enabled()&&view.world().ticks()==0,"Wrong/active startup generation");
        const auto buy=[&](sim::Command command) {
            const auto result=view.execute(command);check(result.accepted&&result.changed,"Actual paid view command: "+result.reason);
        };
        for(int x=110;x<=123;++x)buy({sim::CommandType::PlaceRoad,{x,114}});
        for(int x:{110,113,116})buy({sim::CommandType::PlaceHousehold,{x,112}});
        buy({sim::CommandType::PlaceOriginalMusicSchool,q::source_cell});
        const auto school=*view.world().building_owner_at(q::source_cell);
        const auto channel=q::school(view.world(),school).courier_id;
        check(view.render(),"Draw actual tool controls");
        click_button(view,renderer,oe::sandbox_ui::Action::OriginalTheatre);check(view.tool()==42,"Button lost shared Theatre action");
        view.set_tool(5);key(view,SDLK_G);check(view.tool()==42,"G lost the shared Theatre action");
        focus(view,{double(q::venue_cell.x),double(q::venue_cell.y)},border);
        const auto treasury=view.world().treasury();const auto before=view.world().snapshot();
        const auto ground=screen_ground(view,{double(q::venue_cell.x),double(q::venue_cell.y)},border);
        check(view.pick(ground)==q::venue_cell,"Actual ground input selected another construction cell");
        SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;
        const auto raw=window_point(renderer,ground);move.motion.x=float(raw.x);move.motion.y=float(raw.y);event(view,move);
        check(view.render()&&view.world().snapshot()==before,"Theatre placement preview mutated authority");
        click(view,renderer,ground);
        if(view.budget_warning_pending()) {
            check(view.render(),"Render actual optional budget warning");const auto rect=view.diagnostic_budget_build_rect();
            check(rect.w>0&&rect.h>0,"Missing actual Build anyway control");
            click(view,renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});
        }
        const auto owner=view.world().building_owner_at(q::venue_cell);
        check(owner&&view.world().building(*owner).kind==sim::Object::OriginalTheatre75,"Actual UI did not buy Theatre: "+view.last_message());
        const auto theatre=*owner;
        check(view.world().treasury()==treasury-105&&view.world().workforce_required(theatre)==7,"Paid model105/7 differs");
        for(int y=115;y<119;++y)for(int x=120;x<124;++x)
            check(view.world().building_owner_at({x,y})==theatre,"Theatre lost a complete4x4 body claim");
        zoom(view,renderer,1);select(view,renderer,theatre,border);
        // Every authored stage diamond is a real submitted picking target.
        for(int y=115;y<119;++y)for(int x=120;x<124;++x) {
            check(view.render(),"Render complete4x4 picking frame");
            const auto point=screen_ground(view,{double(x),double(y)},border);
            check(view.layout().map.contains(point.x,point.y),"1x full footprint is outside test aperture");
            click(view,renderer,point);check(view.selected_building()==theatre,"A visible Theatre cell picked another body");
        }
        until(view,[&]{const auto& c=view.world().courier(channel);return q::active(view.world(),school)&&c.edge_progress>0;},
            "Actual paid School failed to spawn and move one performer");
        const auto performer=q::school(view.world(),school).latest_trip->performer_id;
        const auto departed=q::school(view.world(),school).latest_trip->dispatched_tick;
        buy(sim::set_building_operation(school,false));
        check(q::venue(view.world(),theatre).music_timer==0&&!q::venue(view.world(),theatre).last_arrival,
            "Dispatch/proximity gave free venue duration");
        key(view,SDLK_F5);check(fs::is_regular_file(manual),"F5 failed: "+view.last_message());
        const auto active_bytes=q::f::read_text(manual);const auto active_snapshot=view.world().snapshot();
        key(view,SDLK_PERIOD);key(view,SDLK_F9);key(view,SDLK_F5);
        check(view.world().snapshot()==active_snapshot&&q::f::read_text(manual)==active_bytes,"F5/F9 changed complete active-performer bytes/state");
        const auto doc=save::read_save(manual);check(doc.source_schema_version==38,"Actual F5 wrote another schema");
        auto restored=save::restore_original_music_venues(doc,data,view.buildable_mask());
        check(restored.snapshot()==active_snapshot,"Independently prepared active restore differs");
        save::RecoveryStore recovery(output/"recovery-app",data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
        const auto history=recovery.create_history(view.capture_save_document(),view.buildable_mask());
        const auto checkpoint=recovery.write_checkpoint(history,view.capture_save_document(),view.buildable_mask());
        check(q::f::read_text(checkpoint.path)==active_bytes&&
            save::restore_original_music_venues(save::read_save(checkpoint.path),data,view.buildable_mask()).snapshot()==active_snapshot,
            "Recovery lost the active performer and source context");
        Json matrix=Json::array();
        for(const auto size:std::array{std::pair{1280,800},std::pair{1600,900},std::pair{1920,1080}}) {
            check(SDL_SetWindowSize(window,size.first,size.second),"Resize actual production window");SDL_Event e{};e.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;event(view,e);
            for(double scale:{1.,2.,4.}) {
                zoom(view,renderer,scale);select(view,renderer,theatre,border);
                const auto position=view.world().courier_position(channel);check(position.has_value(),"Active performer lost physical position");
                focus(view,*position,border);check(view.render(),"Render active performer");
                const auto diagnostics=view.walker_diagnostics();const auto row=std::find_if(diagnostics.begin(),diagnostics.end(),[&](const auto& r){return r.id==channel;});
                const auto expected=screen_ground(view,*position,border);
                check(row!=diagnostics.end()&&row->live_visible&&row->submitted&&row->direction&&!row->native_asset&&
                    std::abs(row->screen_ground.x-expected.x)<1e-9&&std::abs(row->screen_ground.y-expected.y)<1e-9,
                    "Active authored performer foot/direction/submission differs from real position");
                const auto lines=view.inspection_lines();check(contains(lines,"Incoming performer #")&&contains(lines,"House effect UNSUPPORTED"),
                    "Venue Inspector omitted actual incoming identity or unsupported House boundary");
                const auto name="active-"+std::to_string(size.first)+"-z"+std::to_string(int(scale))+".png";
                capture(view,renderer,output/name);
                matrix.push_back({{"stage","active"},{"window",{size.first,size.second}},{"zoom",scale},{"path",name},
                    {"position",{position->x,position->y}},{"screen_ground",{expected.x,expected.y}},
                    {"performer_id",static_cast<std::uint64_t>(performer)},{"direction",static_cast<unsigned>(*row->direction)},
                    {"original_sprite",false},{"full4x4_ground_pixels",{320*scale,160*scale}},
                    {"map_aperture",{view.layout().map.w,view.layout().map.h}}});
            }
        }
        const auto pure=view.world().snapshot();perf::set_enabled(true);perf::reset();
        for(int n=0;n<100;++n)check(view.render(),"Pure production frame failed");
        Json counters=Json::array();for(std::size_t n=0;n<std::size_t(perf::Counter::Count);++n) {
            const auto value=perf::counter(static_cast<perf::Counter>(n));counters.push_back(value);check(value==0,"Pure frame performed authority/preparation work");
        }
        perf::set_enabled(false);check(view.world().snapshot()==pure,"100 pure frames changed authority");
        zoom(view,renderer,1);const auto position=*view.world().courier_position(channel);focus(view,position,border);
        key(view,SDLK_F1);view.set_tool(5);check(view.render(),"Render actual performer picking");
        click(view,renderer,screen_ground(view,position,border));
        check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(channel)&&contains(view.inspection_lines(),"Performer #"),
            "Actual performer fallback pixels lost selection/typed identity");
        capture(view,renderer,output/"performer-inspector.png");key(view,SDLK_F1);
        until(view,[&]{return q::school(view.world(),school).latest_trip->arrival_tick.has_value();},"Actual one-way performer never reached Venue");
        while(restored.ticks()<view.world().ticks())restored.tick();
        check(restored.snapshot()==view.world().snapshot(),"Full active-performer continuation differs after independent restore");
        check(q::venue(view.world(),theatre).last_arrival.has_value(),
            "Performer arrived but Venue refused admission: "+view.last_message());
        const auto visit=*q::venue(view.world(),theatre).last_arrival;
        check(visit.performer_id==performer&&visit.arrival_tick>departed&&q::venue(view.world(),theatre).music_timer==q::expected_timer(view.world().ticks(),visit.arrival_tick)&&
            view.world().courier(channel).phase==sim::CourierPhase::IdleAtWorkshop,"Real arrival/retired identity/venue32 differs");
        check(!oe::walker_live_visible(view.world().courier(channel)),"Retired performer remained visible at School");
        for(const auto size:std::array{std::pair{1280,800},std::pair{1600,900},std::pair{1920,1080}}) {
            check(SDL_SetWindowSize(window,size.first,size.second),"Resize arrived scene");SDL_Event e{};e.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;event(view,e);
            for(double scale:{1.,2.,4.}) {
                zoom(view,renderer,scale);select(view,renderer,theatre,border);
                const auto lines=view.inspection_lines();check(contains(lines,"Music duration:")&&contains(lines,"Actual arrival:")&&contains(lines,"House effect UNSUPPORTED"),
                    "Actual Venue Inspector omitted duration/arrival/no-House boundary");
                const auto name="arrived-"+std::to_string(size.first)+"-z"+std::to_string(int(scale))+".png";
                capture(view,renderer,output/name);matrix.push_back({{"stage","arrived"},{"window",{size.first,size.second}},{"zoom",scale},{"path",name},
                    {"timer",q::venue(view.world(),theatre).music_timer},{"original_sprite",false},
                    {"full4x4_ground_pixels",{320*scale,160*scale}},{"map_aperture",{view.layout().map.w,view.layout().map.h}}});
            }
        }
        zoom(view,renderer,1);select(view,renderer,school,border);capture(view,renderer,output/"school-inspector.png");
        const auto expiry=q::countdowns(visit.arrival_tick-1)+32;
        const auto expires_at=36+(expiry-1)*51;
        until(view,[&]{return view.world().ticks()>=expires_at;},"Venue32 did not reach its eligible expiry",2000);
        check(q::venue(view.world(),theatre).music_timer==0&&q::venue(view.world(),theatre).last_arrival==visit&&
            q::school(view.world(),school).spawned==1&&view.world().maintenance_spent_total()>0,
            "Expiry invented arrivals, spawned through pause or omitted installed upkeep");
        const auto expired_save=output/"expired-venue.json";view.configure_save(data,"Cities/A.map",expired_save);
        key(view,SDLK_F5);const auto final_bytes=q::f::read_text(expired_save);const auto final=view.world().snapshot();
        key(view,SDLK_PERIOD);key(view,SDLK_F9);check(view.world().snapshot()==final&&q::f::read_text(expired_save)==final_bytes&&
            q::f::read_text(manual)==active_bytes,"Expired-state F5/F9 differs or overwrote active evidence");
        check(q::f::read_text(data/"Cities/A.map")==map_before&&q::f::read_text(data/"Model/EmperorBuildingModels.txt")==model_before,
            "Authored source/model bytes changed");
        std::ofstream report(output/"report.json");report<<Json{{"scope","SCRIPTED_SDL_AUTHORED_INPUTS"},{"native_human","NOT_RUN"},
            {"rule",22},{"schema",38},{"matrix",matrix},{"captures",20},{"pure_frames",100},{"all12_pure_counters",counters},
            {"performer_id",static_cast<std::uint64_t>(performer)},{"dispatch",departed},{"arrival",visit.arrival_tick},
            {"expired_tick",view.world().ticks()},{"construction",view.world().construction_spent_total()},
            {"upkeep",view.world().maintenance_spent_total()},{"funds",view.world().treasury()},
            {"no_house_effect",true},{"full_active_restore_equal",true},{"F5F9_Recovery",true},
            {"geometry_limit","4x4 full ground may exceed map aperture at4x; actual clipping retained"}}.dump(2)<<'\n';check(bool(report),"Write actual QA report");
        view.shutdown();check(oe::StoredGraphicsRenderer::live_texture_count()==0,"Production texture leak");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Paid School/actual Performer/Theatre arrival/expiry, full persistence,20 authored captures and100 pure frames PASS: "<<output<<'\n';return 0;
    }catch(const std::exception& error){perf::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
