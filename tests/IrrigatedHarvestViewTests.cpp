// Independently authored inputs and scripted SDL. This does not claim native
// pointer delivery, Original artwork parity or human acceptance.
#include "EconomyStartViewFixture.h"
#include "IrrigatedHarvestFixture.h"
#include "core/PerformanceDiagnostics.h"

#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace t=oe_irrigated_harvest_fixture;namespace art=oe::testing::economy;
namespace fs=std::filesystem;using Menu=oe::menu::MenuSession;using t::check;
using Point=oe::scene::Point;using Diagnostic=oe::SandboxView::AgricultureVisualDiagnostic;

void event(Menu& menu,SDL_Event value){check(SDL_PushEvent(&value),"Queue ordinary menu event");
    while(SDL_PollEvent(&value))menu.handle_event(value);}
void key(Menu& menu,SDL_Keycode code){SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(menu,value);menu.advance();}
void click(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const char* label){
    check(menu.render(),"Render ordinary menu before its control");const auto r=menu.diagnostic_button_rect(label);
    check(r.has_value(),"Expected ordinary menu control is absent");
    const float scale=1.5F*std::max(1.F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(r->x+r->w/2)*scale,(r->y+r->h/2)*scale,&x,&y),"Invert actual menu coordinates");
    const auto hit=menu.diagnostic_button_hit(x,y);check(hit&&hit->enabled,"Visible menu control is not an enabled hit");
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.x=x;value.button.y=y;
    event(menu,value);value.type=SDL_EVENT_MOUSE_BUTTON_UP;event(menu,value);menu.advance();
}
void wait_map(Menu& menu,unsigned version){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking&&std::chrono::steady_clock::now()<deadline){menu.advance();std::this_thread::yield();}
    const auto& request=menu.map_rules_result().request;
    check(menu.map_rules_can_start()&&request.profile==sim::RulesProfile::CityV16&&request.rule_version==version&&request.policy_version==3,
        "Explicit agriculture menu setup lost exact generation or source-bound policy3");
}
void bounds(Menu& menu,bool retained){
    constexpr std::array labels{"Play Original economy","X Irrigation yields (33)","L Load Save","Advanced / Legacy",
        "Original Data Folder","R Reconfigure (36)","Quit","Autosave: ON","Q Food processing (34)",
        "G Agriculture Irrigation (rule 26)","V Food warehouses (35)","P Playable Agriculture (rule 27)",
        "E Physical Growers (31)","F Playable Fishing (rule 28)","Z Seasons (32)","Y Playable Hunting (rule 29)",
        "C Multi-field Agriculture (30)","J Irrigated harvest (37)"};
    std::vector<SDL_FRect> all;
    const auto add=[&](const char* label){const auto r=menu.diagnostic_button_rect(label);
        check(r&&r->x>=0&&r->y>=0&&r->x+r->w<=600&&r->y+r->h<=358,
            "Regular menu control escapes its600x400 aperture or explanatory footer");
        for(const auto& other:all)check(!SDL_HasRectIntersectionFloat(&*r,&other),"Regular menu controls overlap");all.push_back(*r);};
    for(const char* label:labels)add(label);
    if(retained){add("Enter Continue");check(all.back().y>=100,"Continue overlaps retained-city status text");}
    else check(!menu.diagnostic_button_rect("Enter Continue"),"Fresh menu invented a retained city");
}
void unchanged_defaults(const Menu& menu){
    check(menu.settings().profile==sim::RulesProfile::CityV11&&menu.settings().prepared_starter&&menu.settings().autosave_enabled&&
        sim::current_rule_version(sim::RulesProfile::CityV11)==3&&sim::current_rule_version(sim::RulesProfile::CityV16)==4,
        "Explicit37 changed general profile, starter, autosave or rule defaults");
}
void start(Menu& menu){key(menu,SDLK_RETURN);check(menu.state()==Menu::State::Loading,"Start bypassed deferred Loading");
    check(menu.render(),"Render normal Loading boundary");menu.advance();check(menu.state()==Menu::State::Playing,"Exact-generation candidate did not commit");}
void save_restore(Menu& menu,unsigned version,unsigned schema){
    const auto before=menu.sandbox()->world().snapshot();const auto saved=menu.sandbox()->save_generation();key(menu,SDLK_F5);
    check(menu.sandbox()->save_generation()==saved+1,"Menu F5 failed ordinary save publication");
    const auto path=menu.settings().last_save;const auto bytes=t::f::read_text(path);const auto doc=oe::persistence::read_save(path);
    check(doc.source_schema_version==schema&&doc.world==before&&doc.world.rule_version==version,
        "Menu save selected another generation or altered the paid World");
    menu.sandbox()->tick_once();const auto loaded=menu.sandbox()->load_generation();key(menu,SDLK_F9);
    check(menu.sandbox()->load_generation()==loaded+1&&menu.sandbox()->world().snapshot()==before&&t::f::read_text(path)==bytes,
        "Menu F9 changed exact generation, source bytes or immutable paid state");
}
void menu_generations_and_minimum_retained_layout(t::Fixture& source,SDL_Window* window,SDL_Renderer* renderer){
    const auto app=source.root.path/"menu-app";
    oe::menu::Settings preferences;preferences.data_root=source.root.data;preferences.last_map=t::f::map_relative;
    oe::menu::write_settings(app,preferences);Menu menu({},app);menu.initialize(window,renderer);
    check(menu.state()==Menu::State::MainMenu,"Authored sources did not reach normal main menu");bounds(menu,false);unchanged_defaults(menu);
    click(menu,window,renderer,"Play Original economy");wait_map(menu,18);
    check(menu.original_deadline_food_dispatch_setup()&&!menu.irrigated_harvest_setup(),"Recommended play silently changed to37");key(menu,SDLK_ESCAPE);
    key(menu,SDLK_N);check(!menu.original_setup()&&menu.settings().profile==sim::RulesProfile::CityV11,"N changed default create generation");key(menu,SDLK_ESCAPE);
    // Both actual pointer choice and keyboard shortcut must choose the same37.
    click(menu,window,renderer,"J Irrigated harvest (37)");wait_map(menu,37);
    check(menu.irrigated_harvest_setup()&&!menu.agriculture_reconfiguration_setup(),"J pointer did not select explicit37");key(menu,SDLK_ESCAPE);
    key(menu,SDLK_J);wait_map(menu,37);start(menu);
    check(menu.sandbox()->world().irrigated_harvest_enabled()&&menu.sandbox()->world().ticks()==0&&
        menu.sandbox()->world().total_population()==0&&menu.sandbox()->world().construction_spent_total()>0,
        "J did not create an ordinary paid, empty-of-residents Rule37 starter");
    check(std::string(SDL_GetWindowTitle(window)).find("save schema 53")!=std::string::npos,"Rule37 View title advertises another save schema");
    save_restore(menu,37,53);key(menu,SDLK_ESCAPE);bounds(menu,true);unchanged_defaults(menu);
    key(menu,SDLK_R);wait_map(menu,36);
    check(menu.agriculture_reconfiguration_setup()&&!menu.irrigated_harvest_setup(),"R silently selected37");start(menu);
    check(menu.sandbox()->world().rule_version()==36&&!menu.sandbox()->world().irrigated_harvest_enabled(),"Historical R creation changed generation");
    save_restore(menu,36,52);key(menu,SDLK_ESCAPE);bounds(menu,true);unchanged_defaults(menu);menu.shutdown();
}

void event(oe::SandboxView& view,SDL_Event value){bool running=true;view.handle_event(value,running);check(running,"Ordinary View event unexpectedly closed its session");}
void key(oe::SandboxView& view,SDL_Keycode code){SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(view,value);}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point){
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,float(point.x),float(point.y),&x,&y),"Invert production View pointer coordinates");
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_MOTION;value.motion.x=x;value.motion.y=y;event(view,value);view.update(0);
    value={};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.x=x;value.button.y=y;event(view,value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,value);view.update(0);
}
void center(oe::SandboxView& view,Point ground){
    const auto map=view.layout().map;const auto zoom=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-ground.x*zoom,map.y+map.h*.5-ground.y*zoom};
    for(unsigned n=0;n<1000;++n){const auto now=view.camera().offset;const auto dx=wanted.x-now.x,dy=wanted.y-now.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else{held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}}
    throw std::runtime_error("Paused normal camera did not reach paid construction");
}
Diagnostic diagnostic(const oe::SandboxView& view,Diagnostic::Domain domain,std::uint64_t id){
    const auto rows=view.agriculture_visual_diagnostics();const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.domain==domain&&row.id==id;});
    check(at!=rows.end(),"Rule37 paid entity lost cached agriculture presentation");return *at;
}
void view_paid_early_harvest_and_loaded_f9(t::Fixture& source,SDL_Window* window,SDL_Renderer* renderer){
    check(SDL_SetWindowSize(window,1440,1000),"Resize owned synthetic View");
    auto session=oe::maps::load_stored_map_session(source.root.data,t::f::map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
        oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
    auto view=oe::SandboxView::create_irrigated_harvest(std::move(session),std::nullopt,false);
    const auto path=source.root.path/"view53.save.json";view->configure_save(source.root.data,t::f::map_relative,path);
    view->initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
    for(unsigned n=0;n<3&&view->camera().zoom!=1.;++n)key(*view,SDLK_Z);
    const auto buy=[&](sim::CommandType type,sim::Cell cell,sim::BuildingId owner=sim::BuildingId{}){
        sim::Command command{type,cell};command.building_id=owner;const auto result=view->execute(command);
        if(!result.accepted||!result.changed)throw std::runtime_error("Paid Rule37 View placement: "+result.reason);
        return view->world().building_owner_at(cell).value_or(sim::BuildingId{});};
    // One genuine pointer-paid road before the remaining ordinary command path.
    const sim::Cell road{89,109};const auto ground=oe::maps::terrain_ground({unsigned(road.x),unsigned(road.y)},1);
    view->set_tool(1);center(*view,ground);check(view->render(),"Render pending real road input");
    const auto before_road=view->world().snapshot();click(*view,renderer,view->camera().world_to_screen(ground));
    check(view->world().road_cell(road)&&view->world().ticks()==before_road.ticks&&
        view->world().command_sequence()==before_road.command_sequence+1&&view->world().treasury()==before_road.treasury-2,
        "Actual SDL road input failed sole paid World authority");
    for(int x=90;x<=132;++x)buy(sim::CommandType::PlaceRoad,{x,109});
    for(int x=90;x<126;x+=3)buy(sim::CommandType::PlaceHousehold,{x,107});
    const auto farm=buy(sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    buy(sim::CommandType::PlaceMarket,{97,110});buy(sim::CommandType::PlaceOriginalWaterWell,{101,110});
    buy(sim::CommandType::PlaceOriginalAncestorShrine,{105,110});for(int x:{109,112})buy(sim::CommandType::PlaceFireWatch,{x,110});
    sim::Command pump{sim::CommandType::PlaceIrrigationPump,t::i::pump_origin};pump.orientation=0;
    const auto paid_pump=view->execute(pump);check(paid_pump.accepted&&paid_pump.changed,"Paid Rule37 Pump was refused");
    const auto road_if_needed=[&](sim::Cell cell){if(!view->world().road_cell(cell))buy(sim::CommandType::PlaceRoad,cell);};
    for(int y=93;y<=109;++y)road_if_needed({87,y});for(int y:{104,109})for(int x=88;x<=89;++x)road_if_needed({x,y});
    for(int x=85;x<=149;++x)buy(sim::CommandType::PlaceIrrigationDitch,{x,94});
    for(int y=95;y<=114;++y)buy(sim::CommandType::PlaceIrrigationDitch,{85,y});
    for(int x=86;x<=149;++x)buy(sim::CommandType::PlaceIrrigationDitch,{x,114});
    for(int n=0;n<1700;++n)view->tick_once();const auto field=buy(sim::CommandType::PlaceOriginalWheatField,{93,110},farm);
    for(int n=0;n<160&&!t::s::field(view->world(),field).ready_tick;++n)view->tick_once();
    check(t::s::field(view->world(),field).ready_tick&&t::work(view->world(),field).productive_ticks==64&&
        t::i::work(view->world(),field).wet_work_ticks==64&&t::i::work(view->world(),field).bonus_work==16,
        "Ordinary View failed actual64+16 early readiness");
    const auto ready=diagnostic(*view,Diagnostic::Domain::Building,static_cast<unsigned>(field));
    check(ready.state=="Ready"&&ready.progress==80&&ready.target_progress==80&&ready.wet&&view->agriculture_display_stats().active,
        "Rule37 crop presentation lost actual ready growth or real supplied water");
    for(int n=0;n<5;++n)view->tick_once();
    const auto worker=t::q::farm(view->world(),farm);const auto actor=diagnostic(*view,Diagnostic::Domain::Courier,static_cast<unsigned>(worker.courier_id));
    check(t::field(view->world(),field).harvested==1&&view->world().ticks()-view->world().building(field).placed_tick<80&&
        !t::receipt(view->world(),field).returned&&actor.source==farm&&actor.lifetime_id==worker.latest_trip->grower_id.value&&actor.cargo==1,
        "Early loaded Grower presentation lost real cargo/source/lifetime or credited Food early");
    const auto before=view->world().snapshot();const auto save_generation=view->save_generation();key(*view,SDLK_F5);
    check(view->save_generation()==save_generation+1&&oe::persistence::read_save(path).source_schema_version==53,
        "Loaded Rule37 View F5 selected another schema");const auto bytes=t::f::read_text(path);
    key(*view,SDLK_PERIOD);const auto load_generation=view->load_generation();key(*view,SDLK_F9);
    check(view->load_generation()==load_generation+1&&view->world().snapshot()==before&&t::f::read_text(path)==bytes&&
        diagnostic(*view,Diagnostic::Domain::Courier,static_cast<unsigned>(worker.courier_id)).cargo==1,
        "Loaded Rule37 F9 lost exact cargo, paid state or source save bytes");
    check(view->render(),"Submit freshly restored loaded37 presentation");
    const auto stats=view->agriculture_display_stats();oe::performance::set_enabled(true);oe::performance::reset();
    for(int n=0;n<4;++n){view->update(.017);check(view->render(),"Render paused loaded37 View");}
    for(std::size_t n=0;n<static_cast<std::size_t>(oe::performance::Counter::Count);++n)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(n)),"Paused37 frame performed source, World or navigation work");
    oe::performance::set_enabled(false);
    check(view->world().snapshot()==before&&view->agriculture_display_stats().prepare_count==stats.prepare_count,
        "Paused37 drawing changed saved work/cargo or rebuilt presentation");
    view->shutdown();
}
} // namespace

int main(){SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;try{
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),"Initialize scripted dummy SDL");
    check(SDL_CreateWindowAndRenderer("Irrigated harvest scripted View",900,600,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE,&window,&renderer),"Create minimum owned menu aperture");
    t::Fixture source;art::Temp graphics;
    fs::copy(graphics.root/"data/DATA",source.root.data/"DATA",fs::copy_options::recursive);
    art::write(source.root.data/t::f::map_relative,oe_fishing_fixture::encoded_map(source.raw,source.fertility,true));
    menu_generations_and_minimum_retained_layout(source,window,renderer);
    view_paid_early_harvest_and_loaded_f9(source,window,renderer);
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    std::cout<<"Rule37 scripted menu/create/load, minimum retained layout, paid input and early-harvest presentation passed\n";return 0;
}catch(const std::exception& e){oe::performance::set_enabled(false);std::cerr<<e.what()<<'\n';
    if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}}
