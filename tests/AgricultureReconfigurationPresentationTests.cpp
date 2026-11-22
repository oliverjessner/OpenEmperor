// Independent authored inputs and the paid production view. No injected stock,
// residents, work or removal permission. Scripted SDL is not human acceptance.
#include "EconomyStartViewFixture.h"
#include "AgricultureReconfigurationFixture.h"
#include "core/PerformanceDiagnostics.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;namespace t=oe_agriculture_reconfiguration_fixture;
namespace art=oe::testing::economy;namespace fs=std::filesystem;
using t::check;using Point=oe::scene::Point;
void event(oe::SandboxView& view,SDL_Event value){bool running=true;view.handle_event(value,running);check(running,"Reconfiguration event closed the view");}
void key(oe::SandboxView& view,SDL_Keycode code){SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(view,value);}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point){
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,float(point.x),float(point.y),&x,&y),"Invert reconfiguration pointer coordinates");
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_MOTION;value.motion.x=x;value.motion.y=y;event(view,value);view.update(0);
    value={};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.x=x;value.button.y=y;event(view,value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,value);view.update(0);
}
void center(oe::SandboxView& view,Point point){
    const auto map=view.layout().map;const auto zoom=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-point.x*zoom,map.y+map.h*.5-point.y*zoom};
    for(unsigned step=0;step<1000;++step){const auto offset=view.camera().offset;const auto dx=wanted.x-offset.x,dy=wanted.y-offset.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else{held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}}
    throw std::runtime_error("Paused reconfiguration camera did not converge");
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id){
    const auto cell=view.world().building(id).cell;const auto ground=oe::maps::terrain_ground({unsigned(cell.x),unsigned(cell.y)},1);
    view.set_tool(5);center(view,ground);check(view.render(),"Submit paid leaf before normal picking");
    const auto screen=view.camera().world_to_screen(ground);
    for(const int dy:{0,-8,-16,-24,-32,-48,-64,8,16})for(const int dx:{0,-4,4,-8,8,-12,12,-20,20}){
        const Point point{screen.x+dx*view.camera().zoom,screen.y+dy*view.camera().zoom};
        const auto picked=view.pick(point);if(!picked||view.world().building_owner_at(*picked)!=id)continue;
        click(view,renderer,point);if(view.selected_building()==id&&!view.input_diagnostic_state().selected_walker)return;}
    throw std::runtime_error("Normal pixel selection missed the paid leaf");
}
void action(oe::SandboxView& view,SDL_Renderer* renderer){const auto rect=view.inspector_layout().demolition;
    check(rect.w>0&&rect.h>0,"Shared reconfiguration footer has no visible action");click(view,renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});}
void select_worker(oe::SandboxView& view,SDL_Renderer* renderer,sim::CourierId id){
    const auto ground=view.courier_ground_position(id);check(ground.has_value(),"Loaded plantation worker has no visible position");
    view.set_tool(5);center(view,*ground);check(view.render(),"Submit actual plantation worker marker");const auto screen=view.camera().world_to_screen(*ground);
    for(const int dy:{0,-2,2,-18,-15,-12})for(const int dx:{0,-2,2,8,16,24,32,40,48}){
        const Point point{screen.x+dx,screen.y+dy};if(!view.layout().map.contains(point.x,point.y))continue;
        click(view,renderer,point);if(view.input_diagnostic_state().selected_walker==static_cast<unsigned>(id))return;}
    throw std::runtime_error("Normal pixel selection missed loaded plantation worker");
}
void pure(oe::SandboxView& view){
    const auto before=view.world().snapshot();const auto count=view.agriculture_inspection_prepare_count();
    oe::performance::set_enabled(true);oe::performance::reset();
    for(unsigned frame=0;frame<3;++frame){view.update(0);check(view.render(),"Submit pure reconfiguration frame");(void)view.inspection_lines();}
    for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Reconfiguration frame performed forbidden World/resource/navigation work");
    oe::performance::set_enabled(false);check(view.world().snapshot()==before&&view.agriculture_inspection_prepare_count()==count,"Reconfiguration frames mutated World or rebuilt status");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path){
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Read capture size");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get())&&view.render(),"Render owned reconfiguration capture");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> pixels(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(pixels&&SDL_SavePNG(pixels.get(),path.string().c_str()),"Write private reconfiguration PNG");check(SDL_SetRenderTarget(renderer,nullptr),"Restore window target");
}
void roundtrip(oe::SandboxView& view){key(view,SDLK_F5);const auto before=view.world().snapshot();const auto saved=view.save_generation();
    view.tick_once();key(view,SDLK_F9);check(saved>0&&view.world().snapshot()==before,"F5/F9 changed pending drain, history or physical return");}
void remove(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId target){
    select(view,renderer,target);const auto before=view.world().snapshot();check(view.world().demolition_status(target).allowed,"UI removal lacks World authority");
    action(view,renderer);check(view.demolition_pending()&&!view.recovery_safe_point(),"Ready action bypassed shared blocking confirmation");
    view.tick_once();check(view.world().snapshot()==before,"Pending confirmation advanced paid gameplay");key(view,SDLK_ESCAPE);
    check(view.world().snapshot()==before&&!view.demolition_pending(),"Cancel changed paid authority");
    action(view,renderer);SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;event(view,lost);
    check(!view.demolition_pending()&&view.world().snapshot()==before,"Focus loss confirmed removal");lost.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,lost);
    action(view,renderer);key(view,SDLK_RETURN);
    check(!view.world().building(target).placed&&view.world().agriculture_retired(target)&&!view.selected_building()&&
        view.world().treasury()==before.treasury&&view.world().ticks()==before.ticks&&view.world().next_building_id()==before.next_building_id,
        "Confirmed removal lost tombstone, refunded money, advanced time or allocated identity");
    for(const auto cell:sim::building_footprint_cells(view.world().profile(),view.world().rule_version(),view.world().building(target).kind,view.world().building(target).cell)) {
        check(view.world().building_owner_at(cell)!=target,"Retired leaf retains a physical claim");
    }
    pure(view);
}
void drain_if_needed(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId target){
    select(view,renderer,target);const auto status=view.world().demolition_status(target);
    if(!status.allowed&&!status.draining){check(status.drain_supported,"Leaf has neither drain nor removal authority");action(view,renderer);
        check(!view.demolition_pending()&&view.selected_building()==target&&(view.world().agriculture_draining(target)||view.world().demolition_status(target).allowed),
            "Drain did not use the shared action or changed selection");}
}
void menu(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer){
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    for(const char* label:{"R Reconfigure (36)","V Food warehouses (35)","Q Food processing (34)","Quit"}){const auto rect=menu.diagnostic_button_rect(label);
        check(rect&&rect->x>=0&&rect->x+rect->w<=600&&rect->y+rect->h<=358,"Regular mode or Quit escaped minimum aperture");}
    SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=SDLK_R;menu.handle_event(value);menu.advance();
    check(menu.agriculture_reconfiguration_setup()&&menu.original_agriculture_setup()&&!menu.food_warehouses_setup(),"R selected an inherited generation");
    value.key.key=SDLK_ESCAPE;menu.handle_event(value);menu.advance();check(menu.render(),"Render regular R selector");
    const auto rect=*menu.diagnostic_button_rect("R Reconfigure (36)");const auto scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"R pointer transform");
    value={};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.x=x;value.button.y=y;menu.handle_event(value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;menu.handle_event(value);menu.advance();check(menu.agriculture_reconfiguration_setup(),"R button and key disagree");menu.shutdown();
}
}
int main(){SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;try{
    t::Fixture source;art::Temp visuals;visuals.root=fs::canonical(visuals.root);
    fs::copy(visuals.root/"data/DATA",source.root.data/"DATA",fs::copy_options::recursive);
    art::write(source.root.data/t::f::map_relative,oe_fishing_fixture::encoded_map(source.raw,source.fertility,true));
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");check(SDL_Init(SDL_INIT_VIDEO),"Initialize reconfiguration SDL");
    check(SDL_CreateWindowAndRenderer("Agriculture reconfiguration scripted SDL",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create reconfiguration view");
    menu(source.root.data,visuals.root/"menu",window,renderer);check(SDL_SetWindowSize(window,1440,1000),"Resize paid view");
    auto session=oe::maps::load_stored_map_session(source.root.data,t::f::map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
    auto view=oe::SandboxView::create_agriculture_reconfiguration(std::move(session),std::nullopt,false);
    view->configure_save(source.root.data,t::f::map_relative,visuals.root/"reconfiguration52.save.json");view->initialize(window,renderer);
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);for(unsigned step=0;step<3&&view->camera().zoom!=1.;++step)key(*view,SDLK_Z);
    const auto buy=[&](sim::CommandType type,sim::Cell cell,sim::BuildingId parent=sim::BuildingId{}){sim::Command command{type,cell};command.building_id=parent;
        const auto result=view->execute(command);if(!result.accepted||!result.changed)throw std::runtime_error("Paid UI command: "+result.reason);
        return view->world().building_owner_at(cell).value_or(sim::BuildingId{});};
    for(int x=89;x<=132;++x)buy(sim::CommandType::PlaceRoad,{x,109});for(int x=90;x<126;x+=3)buy(sim::CommandType::PlaceHousehold,{x,107});
    const auto farm=buy(sim::CommandType::PlaceOriginalFoodFarm,{90,110});const auto idle=buy(sim::CommandType::PlaceOriginalWheatField,{93,110},farm);
    buy(sim::CommandType::PlaceMarket,{97,110});buy(sim::CommandType::PlaceOriginalWaterWell,{101,110});buy(sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
    for(int x:{109,112})buy(sim::CommandType::PlaceFireWatch,{x,110});
    const auto output=fs::current_path()/".local/agri-mac11/presentation";fs::create_directories(output);
    select(*view,renderer,idle);pure(*view);capture(*view,renderer,output/"field-ready.png");remove(*view,renderer,idle);
    const auto funds=view->world().treasury();const auto price=view->world().construction_cost(sim::CommandType::PlaceOriginalSoybeanField);
    const auto replacement=buy(sim::CommandType::PlaceOriginalSoybeanField,{93,110},farm);
    check(replacement>idle&&view->world().treasury()==funds-price&&view->world().building(idle).placed==false,"Rebuild evaded price or reused paid identity");
    select(*view,renderer,replacement);capture(*view,renderer,output/"field-rebuilt.png");roundtrip(*view);
    const auto target=buy(sim::CommandType::PlaceOriginalWheatField,{93,111},farm);
    const auto shed=buy(sim::CommandType::PlaceOriginalTeaCuringShed,{115,110});const auto plant=buy(sim::CommandType::PlaceOriginalTeaBush,{115,114},shed);
    drain_if_needed(*view,renderer,plant);check(view->world().demolition_status(plant).allowed,"Idle plant did not settle");remove(*view,renderer,plant);
    const auto live_plant=buy(sim::CommandType::PlaceOriginalTeaBush,{115,114},shed);
    const auto pump=buy(sim::CommandType::PlaceIrrigationPump,t::i::pump_origin);drain_if_needed(*view,renderer,pump);
    check(view->world().demolition_status(pump).allowed,"Idle Pump did not settle");remove(*view,renderer,pump);
    const auto store=buy(sim::CommandType::PlaceWarehouse,{127,110});select(*view,renderer,store);const auto blocked=view->world().snapshot();
    action(*view,renderer);check(!view->demolition_pending()&&view->world().snapshot()==blocked,"Unsupported Warehouse acquired drain/removal");capture(*view,renderer,output/"warehouse-blocked.png");
    bool partial=false;for(unsigned tick=0;tick<14000&&!partial;++tick){view->tick_once();const auto progress=view->world().agriculture_field_progress(target);partial=progress>0&&progress<80;}
    check(partial,"No genuinely worked Field for UI drain");drain_if_needed(*view,renderer,target);check(view->world().agriculture_draining(target),"Partial Field drain was not saved");
    pure(*view);capture(*view,renderer,output/"field-draining.png");roundtrip(*view);
    bool loaded=false;for(unsigned tick=0;tick<14000&&!loaded;++tick){view->tick_once();const auto& worker=t::worker(view->world(),farm);
        loaded=worker.latest_trip&&worker.latest_trip->field_id==target&&sim::physical_food_trip_cargo(*worker.latest_trip)>0;}
    check(loaded&&!view->world().demolition_status(target).allowed,"Loaded Grower was erased or no physical harvest occurred");
    select(*view,renderer,target);capture(*view,renderer,output/"field-loaded-return.png");roundtrip(*view);
    for(unsigned tick=0;tick<2000&&!view->world().demolition_status(target).allowed;++tick)view->tick_once();
    check(view->world().demolition_status(target).allowed&&t::receipt(view->world(),target).returned>0,"Field never settled through actual Farm return");
    const auto receipt=t::receipt(view->world(),target);select(*view,renderer,target);capture(*view,renderer,output/"field-settled.png");remove(*view,renderer,target);
    check(t::receipt(view->world(),target)==receipt,"Removal erased earned physical return history");roundtrip(*view);
    std::optional<sim::CourierId> plantation_worker;
    for(unsigned tick=0;tick<14000&&!plantation_worker;++tick){
        for(const auto& plant_source:view->world().plantations_state()->sources)if(plant_source.building_id==shed&&plant_source.latest_trip&&
            plant_source.latest_trip->plant_id==live_plant&&view->world().plantation_grower_cargo(plant_source.courier_id)>0)plantation_worker=plant_source.courier_id;
        if(!plantation_worker)view->tick_once();
    }
    check(plantation_worker.has_value(),"No actual loaded plantation worker for normal selection regression");
    select_worker(*view,renderer,*plantation_worker);check(!view->debug_diagnostics()&&view->selected_building()==shed,"Normal plantation worker inspector lost its paid source");
    const auto worker_lines=view->inspection_lines();check(std::any_of(worker_lines.begin(),worker_lines.end(),[](const auto& line){return line.find("cargo 1")!=std::string::npos;}),"Normal plantation inspector hides actual cargo");
    pure(*view);capture(*view,renderer,output/"plant-loaded-worker.png");
    for(const auto size:{SDL_Point{900,600},SDL_Point{1280,800}}){check(SDL_SetWindowSize(window,size.x,size.y),"Resize compact reconfiguration footer");view->update(0);select(*view,renderer,replacement);
        const auto layout=view->inspector_layout();check(layout.detail_row_rect(0).h>0&&layout.detail_row_rect(1).h>0&&layout.detail_row_rect(2).h>0,"Compact footer hides three complete details");
        pure(*view);capture(*view,renderer,output/("field-"+std::to_string(size.x)+".png"));}
    t::valid(view->world());view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    std::cout<<"Rule36 R/CLI mode, shared drain/confirm/refusal, real Grower return, normal loaded plantation selection, paid monotone rebuild, tombstones, exact F5/F9 and compact pure frames PASS\n";return 0;
}catch(const std::exception& error){oe::performance::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}}
