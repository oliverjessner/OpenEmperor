// Production SandboxView, authored map/model/pixels, paid commands and real ticks.
// Scripted SDL captures are separate from native delivery and human acceptance.
#include "EconomyStartViewFixture.h"
#include "OriginalVenueHouseServiceFixture.h"
#include "core/PerformanceDiagnostics.h"
#include "app/WalkerPose.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iterator>
#include <memory>
#include <sstream>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace maps=oe::maps;
namespace save=oe::persistence;
namespace perf=oe::performance;
namespace fs=std::filesystem;
namespace q=oe_venue_house_fixture;
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
std::string document_words(const std::vector<std::string>& lines) {
    std::string result;
    for(const auto& line:lines) {
        std::istringstream input(line);std::string word;
        while(input>>word) {if(!result.empty())result+=' ';result+=word;}
    }
    return result;
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
        const auto output=fs::current_path()/".local/oe57"/("venue-house-view-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(output);
        oe::testing::economy::Temp temp;const auto data=temp.root/"data";
        oe_production_fixture::write_model(data);
        q::f::write_text(data/"Model/EmperorBuildingModels.txt",q::q::authored_model());
        const auto map_before=q::f::read_text(data/"Cities/A.map");
        const auto model_before=q::f::read_text(data/"Model/EmperorBuildingModels.txt");
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        window=SDL_CreateWindow("Paid Theatre House Music",1280,800,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        check(window,"Create actual production window");renderer=SDL_CreateRenderer(window,"software");
        check(renderer,"Create software renderer");
        auto session=maps::load_stored_map_session(data,"Cities/A.map",maps::FootprintPolicy::EdgeByte4x4Preview,
            maps::StoredGraphicsProfile::Slot8,maps::GreatWallPresentationMode::Automatic,{},true);
        const auto border=session.plan.border;
        auto owned=oe::SandboxView::create_original_venue_house_service(std::move(session));auto& view=*owned;
        const auto manual=output/"active-venue-walker.json";
        view.configure_save(data,"Cities/A.map",manual);view.initialize(window,renderer);
        check(view.paused()&&view.world().rule_version()==23&&view.world().original_venue_house_service_enabled()&&
            view.world().ticks()==0,"Wrong/active Rule23 startup");
        const auto buy=[&](sim::Command command) {
            const auto result=view.execute(command);check(result.accepted&&result.changed,"Actual paid view command: "+result.reason);
        };
        for(int x=110;x<=123;++x)buy({sim::CommandType::PlaceRoad,{x,114}});
        for(int x:{110,113,116})buy({sim::CommandType::PlaceHousehold,{x,112}});
        buy({sim::CommandType::PlaceOriginalMusicSchool,q::q::source_cell});
        const auto school=*view.world().building_owner_at(q::q::source_cell);
        check(view.render(),"Draw actual Theatre tool controls");
        click_button(view,renderer,oe::sandbox_ui::Action::OriginalTheatre);check(view.tool()==42,"Button lost Theatre action");
        view.set_tool(5);key(view,SDLK_G);check(view.tool()==42,"G lost shared Theatre action");
        focus(view,{double(q::q::venue_cell.x),double(q::q::venue_cell.y)},border);
        const auto ground=screen_ground(view,{double(q::q::venue_cell.x),double(q::q::venue_cell.y)},border);
        const auto treasury=view.world().treasury();const auto preview=view.world().snapshot();
        const auto raw=window_point(renderer,ground);SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;
        motion.motion.x=float(raw.x);motion.motion.y=float(raw.y);event(view,motion);
        check(view.render()&&view.world().snapshot()==preview,"Theatre preview changed paid authority");
        click(view,renderer,ground);
        if(view.budget_warning_pending()) {
            check(view.render(),"Render optional budget warning");const auto rect=view.diagnostic_budget_build_rect();
            check(rect.w>0&&rect.h>0,"Missing actual Build anyway button");
            click(view,renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});
        }
        const auto id=view.world().building_owner_at(q::q::venue_cell);
        check(id&&view.world().building(*id).kind==sim::Object::OriginalTheatre75,"Actual Theatre purchase failed: "+view.last_message());
        const auto theatre=*id;const auto channel=q::source(view.world(),theatre).courier_id;
        check(view.world().treasury()==treasury-105&&view.world().workforce_required(theatre)==7,"Theatre lost paid105/7 model contract");
        zoom(view,renderer,1);select(view,renderer,theatre,border);
        for(int y=115;y<119;++y)for(int x=120;x<124;++x) {
            check(view.world().building_owner_at({x,y})==theatre,"Theatre lost full4x4 authority");
            check(view.render(),"Render actual full-footprint picking frame");
            click(view,renderer,screen_ground(view,{double(x),double(y)},border));
            check(view.selected_building()==theatre,"A visible Theatre footprint cell picked another body");
        }
        until(view,[&]{const auto& c=view.world().courier(channel);
            // The first begun edge starts inside the real4x4 stage. Its
            // painter-correct occlusion is not a visible-pixel picking oracle.
            // Wait for an actual road edge beyond that paid footprint.
            return c.phase==sim::CourierPhase::ToWarehouse&&c.path_vertex>=1&&c.edge_progress>0;
        },"Paid supplied Theatre failed actual Walker movement onto its road",2400);
        const auto trip=[&]() -> const sim::OriginalServiceTrip& {
            for(const auto& row:view.world().original_production_state()->services.trips)
                if(row.transfer.courier_id==channel)return row;
            throw std::runtime_error("Actual venue walker has no retained trip");
        };
        check(trip().venue_walker.has_value()&&!trip().arrival_tick&&!trip().visit_applied,"Dispatch manufactured arrival evidence");
        const auto walker_id=trip().venue_walker->walker_id;const auto dispatch=trip().dispatched_tick;
        const auto home=trip().target;
        for(const auto& h:view.world().original_economy_state()->houses)check(h.house.music==0,"School/dispatch gave free HouseMusic");
        check(q::venue(view.world(),theatre).music_timer>0&&q::venue(view.world(),theatre).last_arrival,
            "Venue dispatch lacks actual performer Music input");
        buy(sim::set_building_operation(school,false));
        key(view,SDLK_F5);check(fs::is_regular_file(manual),"F5 failed: "+view.last_message());
        const auto active_bytes=q::f::read_text(manual);const auto active=view.world().snapshot();
        key(view,SDLK_PERIOD);key(view,SDLK_F9);key(view,SDLK_F5);
        check(view.world().snapshot()==active&&q::f::read_text(manual)==active_bytes,"F5/F9 altered complete active Walker state/bytes");
        const auto document=save::read_save(manual);check(document.source_schema_version==39,"Actual F5 wrote another schema");
        auto independent=save::restore_original_venue_house_service(document,data,view.buildable_mask());
        check(independent.snapshot()==active,"Independent active Walker restore differs");
        save::RecoveryStore recovery(output/"recovery-app",data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
        const auto history=recovery.create_history(view.capture_save_document(),view.buildable_mask());
        const auto checkpoint=recovery.write_checkpoint(history,view.capture_save_document(),view.buildable_mask());
        check(q::f::read_text(checkpoint.path)==active_bytes&&
            save::restore_original_venue_house_service(save::read_save(checkpoint.path),data,view.buildable_mask()).snapshot()==active,
            "Recovery lost actual Walker or Music provenance");
        Json matrix=Json::array();
        const auto capture_matrix=[&](const std::string& stage) {
            for(const auto size:std::array{std::pair{1280,800},std::pair{1600,900},std::pair{1920,1080}}) {
                check(SDL_SetWindowSize(window,size.first,size.second),"Resize actual window");
                SDL_Event resize{};resize.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;event(view,resize);
                for(double scale:{1.,2.,4.}) {
                    zoom(view,renderer,scale);select(view,renderer,theatre,border);
                    const auto position=view.world().courier_position(channel);check(position.has_value(),"Live Walker lost physical position");
                    focus(view,*position,border);check(view.render(),"Render actual moving Walker");
                    const auto rows=view.walker_diagnostics();const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.id==channel;});
                    const auto foot=screen_ground(view,*position,border);
                    check(row!=rows.end()&&row->live_visible&&row->submitted&&row->direction&&!row->native_asset&&
                        std::abs(row->screen_ground.x-foot.x)<1e-9&&std::abs(row->screen_ground.y-foot.y)<1e-9,
                        "Walker fallback direction/foot differs from real movement");
                    const auto lines=view.inspection_lines();
                    check(contains(lines,"Venue walker #")&&contains(lines,"Music96")&&contains(lines,"Authored roundtrip"),
                        "Theatre Inspector omitted actual typed Walker/currentMusic boundary");
                    const auto name=stage+"-"+std::to_string(size.first)+"-z"+std::to_string(int(scale))+".png";
                    capture(view,renderer,output/name);
                    matrix.push_back({{"stage",stage},{"window",{size.first,size.second}},{"zoom",scale},{"path",name},
                        {"walker_id",static_cast<std::uint64_t>(walker_id)},{"position",{position->x,position->y}},
                        {"screen_ground",{foot.x,foot.y}},{"direction",static_cast<unsigned>(*row->direction)},
                        {"original_sprite",false},{"map_aperture",{view.layout().map.w,view.layout().map.h}}});
                }
            }
        };
        capture_matrix("outward");
        const auto pure=view.world().snapshot();perf::set_enabled(true);perf::reset();
        for(int n=0;n<100;++n)check(view.render(),"Pure production frame failed");
        Json counters=Json::array();for(std::size_t n=0;n<std::size_t(perf::Counter::Count);++n) {
            const auto count=perf::counter(static_cast<perf::Counter>(n));counters.push_back(count);check(count==0,"Pure frame did authority/preparation work");
        }
        perf::set_enabled(false);check(view.world().snapshot()==pure,"Pure renders changed World");
        zoom(view,renderer,1);focus(view,*view.world().courier_position(channel),border);
        key(view,SDLK_F1);view.set_tool(5);check(view.render(),"Render actual Walker picking frame");
        click(view,renderer,screen_ground(view,*view.world().courier_position(channel),border));
        check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(channel)&&
            contains(view.inspection_lines(),"Venue walker #"),"Visible Walker lost actual picking/typed identity");
        capture(view,renderer,output/"walker-inspector.png");key(view,SDLK_F1);
        until(view,[&]{return trip().arrival_tick.has_value();},"Actual Walker never reached House");
        check(trip().visit_applied&&trip().venue_walker->arrival_music&&
            view.world().courier(channel).phase==sim::CourierPhase::Returning,"Real House visit did not grant Music and start physical return");
        const auto arrival=*trip().arrival_tick;
        const auto witness=std::find_if(view.world().original_production_state()->services.witnesses.begin(),
            view.world().original_production_state()->services.witnesses.end(),[&](const auto& w){return w.building_id==home&&w.role==oe::original::ServiceRole::Music;});
        check(witness!=view.world().original_production_state()->services.witnesses.end()&&witness->arrival_tick==arrival&&
            witness->venue_walker&&witness->venue_walker->walker_id==walker_id&&witness->venue_walker->arrival_music,
            "House lacks exact actual Walker receipt");
        const auto music_input_id=witness->venue_walker->arrival_music->performer_id;
        const auto house_music=[&] {
            for(const auto& h:view.world().original_economy_state()->houses)
                if(h.building_id==home)return h.house.music;
            throw std::runtime_error("Lost House state");
        };
        const auto elapsed=q::q::countdowns(view.world().ticks())-q::q::countdowns(arrival-1);
        check(elapsed<96&&house_music()==96-elapsed,"HouseMusic differs from actual96 arrival/countdown");
        while(independent.ticks()<view.world().ticks())independent.tick();
        check(independent.snapshot()==view.world().snapshot(),"Independent active restore continuation differs");
        buy(sim::set_building_operation(theatre,false));
        check(independent.execute(sim::set_building_operation(theatre,false)).accepted,"Pause independent returned scene");
        capture_matrix("returning");
        until(view,[&]{return view.world().courier(channel).phase==sim::CourierPhase::IdleAtWorkshop;},"Paused source prevented actual physical return");
        while(independent.ticks()<view.world().ticks())independent.tick();
        check(independent.snapshot()==view.world().snapshot(),"Independent physical return continuation differs");
        check(!oe::walker_live_visible(view.world().courier(channel))&&!view.world().courier_position(channel)&&
            q::source(view.world(),theatre).spawned==1&&q::walkers(view.world()).next_walker_id==2,
            "Returned Walker remained physical, respawned through pause or reused identity");
        zoom(view,renderer,1);select(view,renderer,home,border);
        const auto visits=view.original_house_section_rect(oe::SandboxView::OriginalHouseSection::Visits);
        check(visits.w>0&&visits.h>0,"Missing actual House Visits tab");click(view,renderer,{visits.x+visits.w*.5,visits.y+visits.h*.5});
        const auto visits_words=document_words(view.original_house_section_lines());
        check(view.original_house_section()==oe::SandboxView::OriginalHouseSection::Visits&&
            visits_words.find("Theatre #"+std::to_string(static_cast<std::uint32_t>(theatre))+" | venue walker #"+
                std::to_string(static_cast<std::uint64_t>(walker_id)))!=std::string::npos&&
            visits_words.find("Actual Music input performer #"+std::to_string(static_cast<std::uint64_t>(music_input_id)))!=std::string::npos,
            "House Visits omitted actual Venue/Performer provenance; document: "+visits_words);
        check(view.render(),"Render actual House Visits document");
        const auto visit_lines=view.original_house_section_lines();
        const auto music=std::find_if(visit_lines.begin(),visit_lines.end(),[](const auto& line){
            return line.find("Music actual timer")!=std::string::npos;
        });
        check(music!=visit_lines.end(),"Missing current House Music timer row");
        const auto content=view.original_house_content_rect();
        const auto panel_point=window_point(renderer,{content.x+content.w*.5,content.y+content.h*.5});
        const auto row=static_cast<int>(music-visit_lines.begin());
        SDL_Event scroll{};scroll.type=SDL_EVENT_MOUSE_WHEEL;
        scroll.wheel.mouse_x=float(panel_point.x);scroll.wheel.mouse_y=float(panel_point.y);
        scroll.wheel.y=-float(std::floor(double(row*17*view.layout().scale)/double(24*view.layout().scale)));
        event(view,scroll);check(view.render()&&view.original_house_scroll()>0,"Actual House Visits wheel did not scroll");
        const auto music_y=row*17*view.layout().scale-view.original_house_scroll();
        check(music_y>=0&&music_y+17*view.layout().scale<=content.h,"Music row was clipped out of actual Visits capture");
        capture(view,renderer,output/"house-music-visits.png");
        const auto completed=output/"returned-walker.json";view.configure_save(data,"Cities/A.map",completed);
        key(view,SDLK_F5);const auto returned_bytes=q::f::read_text(completed);const auto returned=view.world().snapshot();
        key(view,SDLK_PERIOD);key(view,SDLK_F9);key(view,SDLK_F5);
        check(view.world().snapshot()==returned&&q::f::read_text(completed)==returned_bytes&&q::f::read_text(manual)==active_bytes,
            "Returned-state F5/F9 differs or rewrote active evidence");
        select(view,renderer,theatre,border);capture(view,renderer,output/"returned-theatre.png");
        check(q::f::read_text(data/"Cities/A.map")==map_before&&q::f::read_text(data/"Model/EmperorBuildingModels.txt")==model_before,
            "Authored input bytes changed");
        std::ofstream report(output/"report.json");report<<Json{{"scope","SCRIPTED_SDL_AUTHORED_INPUTS"},{"native_human","NOT_RUN"},
            {"rule",23},{"schema",39},{"matrix",matrix},{"captures",21},{"pure_frames",100},{"all_pure_counters",counters},
            {"walker_id",static_cast<std::uint64_t>(walker_id)},{"dispatch",dispatch},{"house_arrival",arrival},
            {"returned_tick",view.world().ticks()},{"house_music",house_music()},{"construction",view.world().construction_spent_total()},
            {"upkeep",view.world().maintenance_spent_total()},{"funds",view.world().treasury()},{"F5F9_Recovery_full_equal",true},
            {"original_art",false},{"limits","Authored4x4/fallback markers; aperture may clip at4x; no Original roaming or native parity"}}.dump(2)<<'\n';
        check(bool(report),"Write actual bounded QA report");view.shutdown();
        check(oe::StoredGraphicsRenderer::live_texture_count()==0,"Production texture leak");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Paid physical Venue/House visit and return, full persistence,21 scripted captures and100 pure frames PASS: "<<output<<'\n';return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
