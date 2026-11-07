// Production SandboxView on independently authored land-trade geography.
// No original pixels/data; scripted SDL and software captures are not human QA.
#include "EconomyStartViewFixture.h"
#include "OriginalProductionFixture.h"
#include <cstdlib>
#include "core/PerformanceDiagnostics.h"
#include "maps/LandscapeProvenance.h"
#include "maps/SandboxPlacement.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iterator>
#include <memory>
#include <source_location>
namespace {
namespace oe=openemperor;namespace sim=oe::simulation;namespace maps=oe::maps;
namespace p=oe::persistence;namespace perf=oe::performance;namespace fs=std::filesystem;
namespace fixture=oe::testing::economy;using fixture::check;using Json=nlohmann::json;
using Point=oe::scene::Point;using Bytes=fixture::Bytes;
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Missing private bytes");return {std::istreambuf_iterator<char>(in),{}};}
void event(oe::SandboxView& view,SDL_Event e){bool running=true;view.handle_event(e,running);check(running,"Unexpected quit");}
void key(oe::SandboxView& view,SDL_Keycode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);}
void center(oe::SandboxView& view,Point aim){
    for(int n=0;n<300;++n){const auto map=view.layout().map;const auto actual=view.camera().world_to_screen(aim);
        const double dx=map.x+map.w*.5-actual.x,dy=map.y+map.h*.5-actual.y;if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else{held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}}
    throw std::runtime_error("Paused ordinary camera did not converge");
}
Point window_point(SDL_Renderer* renderer,Point p){SDL_FPoint value{};check(SDL_RenderCoordinatesToWindow(renderer,float(p.x),float(p.y),&value.x,&value.y),"Window inverse failed");return {value.x,value.y};}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted){
    const auto m=view.layout().map;const auto q=window_point(renderer,{m.x+m.w*.5,m.y+m.h*.5});
    for(int n=0;n<3&&std::abs(view.camera().zoom-wanted)>1e-9;++n){SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=float(q.x);e.wheel.mouse_y=float(q.y);e.wheel.y=float(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0,{});}
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Literal zoom failed");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path){
    int w=0,h=0;check(SDL_GetRenderOutputSize(renderer,&w,&h),"Output size failed");
    auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h);check(target,"Owned target failed");
    const auto snapshot=view.world().snapshot();const auto camera=view.camera();
    check(SDL_SetRenderTarget(renderer,target),"Set owned target failed");check(view.render(),"Production capture render failed");
    auto* image=SDL_RenderReadPixels(renderer,nullptr);check(image,"Pre-Present readback failed");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore default target failed");SDL_DestroyTexture(target);
    check(SDL_SavePNG(image,path.c_str()),"Private PNG write failed");SDL_DestroySurface(image);
    check(view.world().snapshot()==snapshot&&view.camera().zoom==camera.zoom&&view.camera().offset.x==camera.offset.x&&view.camera().offset.y==camera.offset.y,"Capture mutated World/camera");
}

void click(oe::SandboxView& view,SDL_Renderer* renderer,oe::sandbox_ui::Rect rect,bool release=true,
        const std::source_location where=std::source_location::current()) {
    if(rect.w<=0||rect.h<=0)throw std::runtime_error("Empty actual control at line "+std::to_string(where.line())+
        "; rect="+std::to_string(rect.x)+","+std::to_string(rect.y)+","+std::to_string(rect.w)+","+std::to_string(rect.h)+
        "; selected="+std::to_string(unsigned(view.selected_building().value_or(sim::BuildingId{})))+
        "; budget="+std::to_string(view.budget_warning_pending())+"; panel="+std::to_string(view.layout().panel_open)+
        "; message="+view.last_message());
    const auto q=window_point(renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=float(q.x);e.button.y=float(q.y);event(view,e);
    if(release){e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);}
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id,unsigned border) {
    if(!view.layout().panel_open)key(view,SDLK_TAB);view.set_tool(5);
    const auto& b=view.world().building(id);
    const auto f=sim::building_front_cell(sim::RulesProfile::CityV16,21,b.kind,b.cell);
    const Point aim{double(f.x-f.y)*40,(double(f.x+f.y)-2*border)*20+20};center(view,aim);
    check(view.render(),"Selection render failed");
    // Real production picking and actual SDL release; no selection override.
    bool chosen=false;
    for(int y=-60;y<=15&&!chosen;y+=4)for(int x=-50;x<=50&&!chosen;x+=4) {
        const auto screen=view.camera().world_to_screen({aim.x+x+.5,aim.y+y+.5});
        if(!view.layout().map.contains(screen.x,screen.y))continue;
        const auto cell=view.pick(screen);if(!cell || view.world().building_owner_at(*cell)!=id)continue;
        const auto q=window_point(renderer,screen);SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=float(q.x);e.button.y=float(q.y);event(view,e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);
        chosen=view.selected_building()==id;
    }
    check(chosen,"Actual paid Warehouse could not be selected");
}
int child(const fs::path& data,const fs::path& input,const fs::path& output) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"Independent SDL init failed");
    struct Quit{~Quit(){SDL_Quit();}}quit;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Independent authored trade restore",1280,800,SDL_WINDOW_HIDDEN),SDL_DestroyWindow);
    check(window.get(),"Independent window failed");
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateRenderer(window.get(),"software"),SDL_DestroyRenderer);
    check(renderer.get(),"Independent renderer failed");
    const auto doc=p::read_save(input);
    auto session=maps::load_stored_map_session(data,doc.map_relative,maps::FootprintPolicy::EdgeByte4x4Preview,
        maps::StoredGraphicsProfile::Slot8,maps::GreatWallPresentationMode::Automatic,{},true);
    auto view=oe::SandboxView::create_authored_land_trade(std::move(session));
    view->configure_save(data,doc.map_relative,output/"child-restored.json",doc);view->initialize(window.get(),renderer.get());
    check(view->paused(),"Independent restore did not start paused");view->save_now();
    for(int n=0;n<103;++n)view->tick_once();
    view->configure_save(data,doc.map_relative,output/"child-continued.json");view->save_now();view->shutdown();return 0;
}
}
int main(int argc,char** argv){SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;try {
    if(argc==5&&std::string_view(argv[1])=="--independent")return child(argv[2],argv[3],argv[4]);
    const auto output=fs::current_path()/".local/oe54"/("view-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(output);
    fixture::Temp temp;const auto data=temp.root/"data";oe_production_fixture::write_model(data);
    const auto source_before=bytes(data/"Cities/A.map"),model_before=bytes(data/"Model/EmperorBuildingModels.txt");
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"SDL video init failed");
    window=SDL_CreateWindow("Authored finite land trade production QA",1280,800,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);check(window,"Window failed");renderer=SDL_CreateRenderer(window,"software");check(renderer,"Software renderer failed");
    auto session=maps::load_stored_map_session(data,"Cities/A.map",maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8,maps::GreatWallPresentationMode::Automatic,{},true);const auto border=session.plan.border;
    auto owned=oe::SandboxView::create_authored_land_trade(std::move(session));auto& view=*owned;
    const auto manual=output/"manual.json";view.configure_save(data,"Cities/A.map",manual);view.initialize(window,renderer);
    check(view.paused()&&view.world().authored_land_trade_enabled()&&!view.world().original_economy_enabled(),"Wrong/active trade generation");
    unsigned budget_confirmations=0;
    const auto confirm_budget=[&] {
        if(!view.budget_warning_pending())return;
        const auto before=view.world().snapshot();check(view.render(),"Render actual budget warning");
        click(view,renderer,view.diagnostic_budget_build_rect());
        check(!view.budget_warning_pending()&&view.world().command_sequence()==before.command_sequence+1,
            "Actual Build anyway mouse action did not commit one revalidated command");++budget_confirmations;
    };
    const auto buy=[&](sim::Command c){
        const auto before=view.world().snapshot();const auto r=view.request_execute(c);
        if(view.budget_warning_pending()) {
            check(view.world().snapshot()==before,"Budget preflight changed paid World before confirmation");confirm_budget();
        } else if(!r.accepted)throw std::runtime_error("Paid view fixture: "+r.reason);
    };
    for(const sim::Cell home:std::array<sim::Cell,5>{{{82,104},{85,104},{88,104},{91,104},{94,104}}})buy({sim::CommandType::PlaceHousehold,home});
    buy({sim::CommandType::PlaceWarehouse,{79,110}});const auto city=*view.world().building_owner_at({79,110});
    buy({sim::CommandType::PlaceWarehouse,{75,110}});const auto gateway=*view.world().building_owner_at({75,110});
    buy({sim::CommandType::PlaceClaySource,{83,110}});
    buy({sim::CommandType::PlacePottery,{87,110}});
    for(int x=75;x<=88;++x)buy({sim::CommandType::PlaceRoad,{x,112}});
    select(view,renderer,city,border);const auto before_bind=view.world().snapshot();
    click(view,renderer,view.diagnostic_land_trade_button_rect(0),false);
    SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;event(view,lost);
    SDL_Event gained{};gained.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,gained);
    const auto bind=view.diagnostic_land_trade_button_rect(0);const auto q=window_point(renderer,{bind.x+bind.w*.5,bind.y+bind.h*.5});
    SDL_Event up{};up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.button=SDL_BUTTON_LEFT;up.button.x=float(q.x);up.button.y=float(q.y);event(view,up);
    check(view.world().snapshot()==before_bind,"Focus boundary committed an abandoned binding press");
    const auto binding_check=view.world().validate(sim::configure_land_trade(city,gateway));
    const auto boundary=p::prepare_land_trade_boundary_facts(data,"Cities/A.map");
    key(view,SDLK_G);
    check(view.budget_warning_pending()&&view.world().snapshot()==before_bind,
        "G did not retain the optional classic starter warning without mutation");
    click(view,renderer,view.diagnostic_budget_cancel_rect());
    check(!view.budget_warning_pending()&&view.world().snapshot()==before_bind,"Actual Cancel committed trade license");
    click(view,renderer,view.diagnostic_land_trade_button_rect(0));confirm_budget();
    if(!view.world().land_trade_state()->binding) {
        const auto& gateway_body=view.world().building(gateway);
        std::cerr<<"Trade binding diagnostic: selected="<<unsigned(view.selected_building().value_or(sim::BuildingId{}))
            <<" city="<<unsigned(city)<<" gateway="<<unsigned(gateway)
            <<" accepted="<<binding_check.accepted<<" reason="<<binding_check.reason
            <<" boundary="<<boundary->boundary_footprint(gateway_body.cell,2,2)
            <<" gateway-origin="<<gateway_body.cell.x<<','<<gateway_body.cell.y
            <<" city-workers="<<view.world().workers_assigned(city)<<'/'<<view.world().workforce_required(city)
            <<" gateway-workers="<<view.world().workers_assigned(gateway)<<'/'<<view.world().workforce_required(gateway)
            <<" last_message="<<view.last_message()<<'\n';
    }
    check(view.world().land_trade_state()->binding&&view.world().land_trade_state()->binding->warehouse==city&&
        view.world().land_trade_state()->binding->gateway==gateway&&view.world().treasury()==before_bind.treasury-40,"G did not share paid binding action");
    check(view.selected_building()==city,"Accepted binding lost selected city Warehouse");
    const auto bound=view.world().snapshot();click(view,renderer,view.diagnostic_land_trade_button_rect(0));
    check(view.world().snapshot()==bound&&view.selected_building()==city,"Repeated mouse binding charged again or lost selection");
    click(view,renderer,view.diagnostic_land_trade_button_rect(3));
    check(!view.demolition_pending() && view.world().snapshot()==bound,"Bound Warehouse demolition widened World authority");
    // Earn CITY export stock through the actual paid Clay/Pottery chain.
    // Both Warehouses may physically receive production; do not stop Clay at
    // the first four total units or change any stock/cursor/workforce authority.
    for(int n=0;n<5000&&view.world().building(city).pottery_stock<4;++n)view.tick_once();
    check(view.world().pottery_completed_total()>=4&&view.world().building(city).pottery_stock>=4,
        "Bounded actual paid production did not earn four city-Warehouse Pottery");
    const auto production_tick=view.world().ticks();const auto produced=view.world().pottery_completed_total();
    select(view,renderer,city,border);const auto before_export=view.world().snapshot();
    click(view,renderer,view.diagnostic_land_trade_button_rect(2));
    check(view.world().land_trade_balance().busy&&view.world().building(city).pottery_stock==before_export.buildings[5].pottery_stock-4&&
        view.world().land_trade_balance().exports_received==0,"Mouse export failed physical load or paid early");
    check(view.selected_building()==city,"Accepted export lost selected city Warehouse");
    const auto courier_id=view.world().land_trade_state()->binding->courier;
    check(view.world().courier(courier_id).cargo==4,"Export cargo is not actual owner stock");
    capture(view,renderer,output/"export-outbound.png");
    const auto busy=view.world().snapshot();key(view,SDLK_I);check(view.world().snapshot()==busy&&view.selected_building()==city,"Busy import changed state or lost selection");
    for(int n=0;n<500&&view.world().land_trade_balance().busy;++n)view.tick_once();
    check(!view.world().land_trade_balance().busy&&view.world().land_trade_balance().exported==4&&
        view.world().land_trade_balance().exports_received==12&&view.world().courier(courier_id).phase==sim::CourierPhase::IdleAtWorkshop,
        "Actual boundary sale/empty return did not finish");
    const auto before_import=view.world().snapshot();key(view,SDLK_I);confirm_budget();
    check(view.world().treasury()==before_import.treasury-24&&view.world().building(city).reserved_incoming>=4&&
        view.world().courier(courier_id).cargo==0&&view.world().courier(courier_id).reserved==4,"I did not prepay/reserve empty outbound import");
    check(view.selected_building()==city,"Accepted import lost selected city Warehouse");
    bool loaded=false;for(int n=0;n<500;++n){view.tick_once();const auto& c=view.world().courier(courier_id);
        if(c.phase==sim::CourierPhase::Returning&&c.cargo==4){loaded=true;break;}}
    check(loaded&&view.world().land_trade_balance().imported==4,"Actual source-boundary import load absent");
    const auto saved=view.world().snapshot();key(view,SDLK_F5);check(fs::exists(manual),view.last_message().c_str());const auto full=bytes(manual);
    key(view,SDLK_PERIOD);key(view,SDLK_F9);key(view,SDLK_F5);
    check(view.world().snapshot()==saved&&bytes(manual)==full,"F5/Step/F9/F5 lost full loaded import SaveDocument bytes");
    p::RecoveryStore recovery(output/"app",data);const auto doc=view.capture_save_document();const auto history=recovery.create_history(doc,view.buildable_mask());const auto checkpoint=recovery.write_checkpoint(history,doc,view.buildable_mask());
    check(p::restore_authored_land_trade(p::read_save(checkpoint.path),data,view.buildable_mask()).snapshot()==saved,"Recovery lost physical trade cargo/escrow/reservation");
    const auto executable=fs::absolute(argv[0]).string(),child_data=data.string(),child_input=manual.string(),child_output=output.string();
    const std::array<const char*,6> arguments{executable.c_str(),"--independent",child_data.c_str(),child_input.c_str(),child_output.c_str(),nullptr};
    auto* process=SDL_CreateProcess(arguments.data(),false);check(process,"Independent-process start failed");
    int child_status=-1;const bool waited=SDL_WaitProcess(process,true,&child_status);SDL_DestroyProcess(process);
    check(waited&&child_status==0,"Independent-process trade restore failed");
    check(bytes(output/"child-restored.json")==full,"Independent-process full SaveDocument bytes differ");
    Json captures=Json::array();
    const auto position=*view.world().courier_position(courier_id);const Point aim{(position.x-position.y)*40,(position.x+position.y-2*border)*20+20};
    for(const auto size:std::array<std::array<int,2>,3>{{{1280,800},{1600,1000},{1920,1080}}}) {
        check(SDL_SetWindowSize(window,size[0],size[1]),"Window size failed");SDL_PumpEvents();SDL_Event resize{};resize.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;event(view,resize);
        for(const double z:{1.,2.,4.}){zoom(view,renderer,z);center(view,aim);check(view.render(),"Trade render failed");
            const auto path=output/("import-return-"+std::to_string(size[0])+"-z"+std::to_string(int(z))+".png");capture(view,renderer,path);
            for(int i=0;i<4;++i){const auto r=view.diagnostic_land_trade_button_rect(i);check(r.h>0&&view.layout().panel.contains(r.x,r.y)&&view.layout().panel.contains(r.x+r.w-1,r.y+r.h-1),"Trade footer control escaped panel");}
            const auto actors=view.walker_diagnostics();const auto actor=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.id==courier_id;});
            check(actor!=actors.end()&&actor->cargo==4&&actor->submitted,"Actual loaded caravan was not submitted to production render");
            captures.push_back({{"file",path.filename().string()},{"window",size},{"zoom",z},{"cargo",actor->cargo},{"foot",{actor->screen_ground.x,actor->screen_ground.y}},{"original_art",false}});
        }
    }
    perf::set_enabled(true);perf::reset();for(int n=0;n<100;++n)check(view.render(),"Pure trade frame failed");
    Json counters=Json::array();for(std::size_t i=0;i<std::size_t(perf::Counter::Count);++i){const auto value=perf::counter(static_cast<perf::Counter>(i));counters.push_back(value);check(value==0,"Trade frame performed forbidden preparation/authority work");}perf::set_enabled(false);
    check(view.world().snapshot()==saved,"Visual matrix or pure frames changed World");
    for(int n=0;n<103;++n)view.tick_once();key(view,SDLK_F5);
    check(bytes(manual)==bytes(output/"child-continued.json"),"Independent-process 103-tick full continuation differs");
    for(int n=0;n<500&&view.world().land_trade_balance().busy;++n)view.tick_once();
    check(!view.world().land_trade_balance().busy&&view.world().courier(courier_id).cargo==0&&view.world().courier(courier_id).reserved==0&&
        view.world().land_trade_balance().imported==4&&view.world().production_balance_valid(),"Import physical arrival/owner accounting failed");
    check(bytes(data/"Cities/A.map")==source_before&&bytes(data/"Model/EmperorBuildingModels.txt")==model_before,"Authored source input changed");
    std::ofstream report(output/"report.json");report<<Json{{"scope","authored map + scripted SDL; native/human NOT_RUN; no original trade parity"},{"rule",21},{"schema",37},{"production_tick",production_tick},{"pottery_completed",produced},{"city",unsigned(city)},{"gateway",unsigned(gateway)},{"saved_tick",saved.ticks},{"final_tick",view.world().ticks()},{"imported",4},{"exported",4},{"imports_paid",24},{"exports_received",12},{"captures",captures},{"pure_frames",100},{"performance_counters",counters},{"independent_process_ticks",103},{"actual_budget_confirmations",budget_confirmations},{"actual_binding_budget_cancel",true}}.dump(2)<<'\n';check(bool(report),"Report write failed");
    view.shutdown();check(oe::StoredGraphicsRenderer::live_texture_count()==0,"Trade view texture leak");SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    std::cout<<"Authored Rule21 production/export/import/SDL/F5F9/Recovery/independent-process/9 captures/pure frames PASS: "<<output<<'\n';return 0;
}catch(const std::exception& e){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<e.what()<<'\n';return 1;}}
