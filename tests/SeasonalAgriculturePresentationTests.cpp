// Independent paid SDL scenario; native-window/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "SeasonalAgricultureFixture.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace q=oe_irrigation_fixture;
namespace n=oe_seasonal_agriculture_fixture;namespace fs=std::filesystem;
using f::check;using Point=oe::scene::Point;
void key(oe::menu::MenuSession& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;
    menu.handle_event(e);menu.advance();
}
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Input closed Physical Food Growers");
}
void key(oe::SandboxView& view,SDL_Keycode code,SDL_Keymod mod=SDL_KMOD_NONE) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;e.key.mod=mod;event(view,e);
}
void center(oe::SandboxView& view,Point ground) {
    const auto map=view.layout().map;const auto z=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-ground.x*z,map.y+map.h*.5-ground.y*z};
    for(unsigned step=0;step<1000;++step) {
        const auto now=view.camera().offset;const auto dx=wanted.x-now.x,dy=wanted.y-now.y;
        if(std::abs(dx)<1e-6 && std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6) {held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused Physical Food Growers camera did not converge");
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point render) {
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,float(render.x),float(render.y),&x,&y),"Pointer transform");
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=x;e.motion.y=y;event(view,e);view.update(0);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);view.update(0);
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::Cell cell) {
    const auto ground=oe::maps::terrain_ground({std::uint32_t(cell.x),std::uint32_t(cell.y)},1);
    view.set_tool(5);center(view,ground);
    // Ordinary mesh picking intentionally follows the last submitted frame.
    // Display the new camera before clicking the building now visible there.
    check(view.render(),"Submit camera before building selection");
    const auto wanted=view.world().building_owner_at(cell);check(wanted.has_value(),"Selection cell has no paid body");
    const auto screen=view.camera().world_to_screen(ground);
    for(const int dy:{0,-8,-16,-24,-32,-48,-64,8,16}) for(const int dx:{0,-4,4,-8,8,-12,12,-20,20}) {
        const Point point{screen.x+dx*view.camera().zoom,screen.y+dy*view.camera().zoom};
        const auto picked=view.pick(point);
        if(!picked || view.world().building_owner_at(*picked)!=wanted)continue;
        click(view,renderer,point);
        if(view.selected_building()==wanted && !view.input_diagnostic_state().selected_walker)return;
    }
    throw std::runtime_error("Visible paid body could not be selected independently of its live Grower: wanted "+std::to_string(static_cast<unsigned>(*wanted))+" cell "+std::to_string(cell.x)+","+std::to_string(cell.y)+" selected "+std::to_string(static_cast<unsigned>(view.selected_building().value_or(sim::BuildingId{}))));
}
bool contains(const std::vector<std::string>& lines,std::string_view fragment) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(fragment)!=std::string::npos;});
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h),"Capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Private capture target");
    check(view.render(),"Physical Food Growers render");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Private capture output");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore render target");
}

void menu_path(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    if(menu.state()!=oe::menu::MenuSession::State::MainMenu || menu.seasonal_agriculture_setup())
        throw std::runtime_error("Seasons initial menu state "+std::to_string(static_cast<unsigned>(menu.state()))+": "+menu.message());
    for(const char* label:{"G Agriculture Irrigation (rule 26)","P Playable Agriculture (rule 27)",
            "F Playable Fishing (rule 28)","Y Playable Hunting (rule 29)","E Physical Growers (31)","Z Seasons (32)"}) {
        const auto rect=menu.diagnostic_button_rect(label);
        check(rect && rect->x>=0 && rect->x+rect->w<=600 && rect->y>=0 && rect->y+rect->h<=358,
            "Regular seasonal selector escapes the minimum menu aperture");
    }
    check(menu.render(),"Render regular Seasons selector");
    const auto rect=*menu.diagnostic_button_rect("Z Seasons (32)");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"Z pointer transform");
    const auto mouse=[&](Uint32 type) {
        SDL_Event e{};e.type=type;e.button.windowID=SDL_GetWindowID(window);e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
        menu.handle_event(e);
    };
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN);mouse(SDL_EVENT_MOUSE_BUTTON_UP);menu.advance();
    check(menu.seasonal_agriculture_setup(),"Z pointer selected another generation");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_E);
    check(menu.physical_food_growers_setup() && !menu.seasonal_agriculture_setup(),"Historical E selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_C);
    check(menu.multi_field_agriculture_setup() && !menu.seasonal_agriculture_setup(),"Historical C selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_Z);
    check(menu.original_agriculture_setup() && menu.seasonal_agriculture_setup(),"Z shortcut lost the explicit lineage");
    key(menu,SDLK_P);check(!menu.original_paid_starter(),"Seasonal Empty city selection failed");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking && std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==32,"Seasonal preflight did not bind32");
    key(menu,SDLK_RETURN);check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Seasonal loading frame");menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox())throw std::runtime_error("Seasonal menu Start: "+menu.message());
    check(menu.sandbox()->world().seasonal_agriculture_enabled() && menu.sandbox()->world().rule_version()==32 && menu.sandbox()->paused(),
        "Menu did not create a paused Rule32 World");
    menu.shutdown();
}
std::string joined(const std::vector<std::string>& lines) {
    std::string value;for(const auto& line:lines){value+=line;value+=' ';}return value;
}
void seasonal_inspector(const oe::SandboxView& view) {
    const auto text=joined(view.inspection_lines());const auto tick=view.world().ticks();
    for(const auto& fragment:{"Elapsed cycle "+std::to_string(tick/9792),"Outer slot "+std::to_string((tick/816)%12),
            "Subslot "+std::to_string((tick/51)%16),"Phase "+std::to_string(tick%51),
            std::string("Growing slots"),std::string("Next growing opening"),std::string("Wasted work"),
            std::string("expired ready"),std::string("yield inactive"),std::string("AUTHORED")})
        if(text.find(fragment)==std::string::npos)
            throw std::runtime_error("Normal seasonal inspector missing ["+fragment+"] at tick "+std::to_string(tick)+": "+text);
    check(text.find("Original period query is not its quantity callback veto")!=std::string::npos,
        "Seasonal inspector conflates observed queries with authored work gates");
    for(const auto name:{"January","February","March","April","May","June","July","August",
            "September","October","November","December"})
        check(text.find(name)==std::string::npos,"Inspector invented Original month names");
}
void pure_frames(oe::SandboxView& view) {
    const auto before=view.world().snapshot();const auto prepared=view.agriculture_inspection_prepare_count();
    oe::performance::set_enabled(true);oe::performance::reset();
    for(unsigned i=0;i<9;++i)check(view.render(),"Seasonal pure frame failed");
    for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Seasonal render performed World/resource/navigation work");
    oe::performance::set_enabled(false);
    check(view.world().snapshot()==before && view.agriculture_inspection_prepare_count()==prepared,
        "Seasonal frames mutated paid state or rebuilt prepared diagnosis");
}
} // namespace
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        n::Fixture source;f::Temp temp;temp.root=fs::canonical(temp.root);
        // These independently authored UI inputs add terrain-art references
        // to the same raw/fertility geography before the real app binds it.
        fs::copy(temp.root/"data/DATA",source.root.data/"DATA",fs::copy_options::recursive);
        f::write(source.root.data/oe_influence_fixture::map_relative,
            oe_fishing_fixture::encoded_map(source.raw,source.fertility,true));
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize isolated seasonal SDL");
        check(SDL_CreateWindowAndRenderer("Seasonal Agriculture scripted acceptance",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),
            "Create seasonal SDL renderer");
        menu_path(source.root.data,temp.root/"menu",window,renderer);
        auto session=oe::maps::load_stored_map_session(source.root.data,oe_influence_fixture::map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_seasonal_agriculture(std::move(session),std::nullopt,false);
        const auto save_path=temp.root/"seasons48.save.json";
        view->configure_save(source.root.data,oe_influence_fixture::map_relative,save_path);view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        for(unsigned attempt=0;attempt<3 && view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
        check(view->world().ticks()==0 && !view->world().total_population() && !view->debug_diagnostics(),
            "New seasonal UI granted time/residents or opened debug inspection");
        const auto buy=[&](sim::CommandType type,sim::Cell cell,sim::BuildingId parent=sim::BuildingId{}) {
            sim::Command command{type,cell};command.building_id=parent;const auto result=view->execute(command);
            if(!result.accepted || !result.changed)throw std::runtime_error("Paid seasonal UI construction at "+
                std::to_string(cell.x)+","+std::to_string(cell.y)+": "+result.reason);
            return view->world().building_owner_at(cell).value_or(sim::BuildingId{});
        };
        n::Scene scene;auto& materials=scene.materials;
        for(const int y:{104,107})for(int x=89;x<=149;++x)buy(sim::CommandType::PlaceRoad,{x,y});
        for(const int x:{89,149})for(const int y:{105,106})buy(sim::CommandType::PlaceRoad,{x,y});
        for(int x=90;x<150;x+=3)materials.homes.push_back(buy(sim::CommandType::PlaceHousehold,{x,102}));
        materials.tea_source=buy(sim::CommandType::PlaceOriginalTeaCuringShed,{92,105});
        materials.lacquer_source=buy(sim::CommandType::PlaceOriginalLacquerRefinery,{102,105});
        materials.mulberry_source=buy(sim::CommandType::PlaceOriginalSilkwormShed,{112,105});
        materials.tea_plant=buy(sim::CommandType::PlaceOriginalTeaBush,{96,109},materials.tea_source);
        materials.lacquer_plant=buy(sim::CommandType::PlaceOriginalLacquerTree,{106,109},materials.lacquer_source);
        materials.mulberry_plant=buy(sim::CommandType::PlaceOriginalMulberryTree,{116,109},materials.mulberry_source);
        materials.logging=buy(sim::CommandType::PlaceOriginalLoggingShed,{122,105});
        materials.warehouse=buy(sim::CommandType::PlaceWarehouse,{128,105});
        materials.market=buy(sim::CommandType::PlaceMarket,{134,105});
        materials.water=buy(sim::CommandType::PlaceOriginalWaterWell,{138,105});
        materials.ancestor=buy(sim::CommandType::PlaceOriginalAncestorShrine,{142,105});
        materials.lacquerware=buy(sim::CommandType::PlaceOriginalLacquerwareMaker,{124,108});
        materials.weaver=buy(sim::CommandType::PlaceOriginalWeaver,{134,108});
        for(const int x:{100,130})materials.watches.push_back(buy(sim::CommandType::PlaceFireWatch,{x,108}));
        for(int x=89;x<=149;++x)buy(sim::CommandType::PlaceRoad,{x,100});
        for(int y=101;y<104;++y)buy(sim::CommandType::PlaceRoad,{89,y});
        for(std::size_t i=0;i<5;++i) {
            const int x=90+7*static_cast<int>(i);
            scene.farms[i]=buy(sim::CommandType::PlaceOriginalFoodFarm,{x,97});
            scene.fields[i]=buy(n::q::m::crop_commands[i],{x+3,97},scene.farms[i]);
        }
        scene.hemp_farm=buy(sim::CommandType::PlaceOriginalHempFarm,{132,98});
        scene.hemp_field=buy(sim::CommandType::PlaceOriginalHempField,{134,98},scene.hemp_farm);
        std::vector<sim::BuildingId> subjects(scene.fields.begin(),scene.fields.end());
        subjects.insert(subjects.end(),{scene.hemp_field,scene.materials.tea_plant,scene.materials.lacquer_plant,scene.materials.mulberry_plant});
        for(const auto id:subjects) {
            select(*view,renderer,view->world().building(id).cell);seasonal_inspector(*view);pure_frames(*view);
            check(contains(view->inspection_lines(),"Off-season"),"Initial off-season inspector hides its productive veto");
        }
        const auto output=fs::current_path()/".local/agri-mac07/presentation";fs::create_directories(output);
        select(*view,renderer,view->world().building(scene.fields[0]).cell);
        capture(*view,renderer,output/"seasonal-wheat-offseason.png");
        const auto paused=view->world().snapshot();view->update(.2);check(view->world().snapshot()==paused,"Paused UI advanced the seasonal clock");
        while(view->world().ticks()<1631)view->tick_once();seasonal_inspector(*view);
        check(contains(view->inspection_lines(),"Off-season"),"Wheat opened before exact start slot");
        key(*view,SDLK_F5);const auto before_open=view->world().snapshot();check(fs::exists(save_path),"F5 omitted schema48 save");
        view->tick_once();check(view->world().ticks()==1632,"Seasonal single step used another clock");seasonal_inspector(*view);
        check(contains(view->inspection_lines(),"Actual growing window OPEN"),"Exact Wheat opening is absent from the normal inspector");
        key(*view,SDLK_F9);check(view->world().snapshot()==before_open && view->paused(),"F9 changed exact seasonal boundary state");
        view->tick_once();
        bool ready=false;
        for(unsigned i=0;i<1800 && !ready;++i) {
            ready=n::field(view->world(),scene.fields[0]).ready_tick.has_value();
            if(!ready)view->tick_once();
        }
        check(ready,"Paid physical Wheat never reached seasonal readiness");seasonal_inspector(*view);
        const auto pending_text=joined(view->inspection_lines());
        check(pending_text.find("Ready crop expires before tick")!=std::string::npos && pending_text.find("At-field harvest work")!=std::string::npos,
            "Pending crop inspector hides expiry or real harvest work");
        key(*view,SDLK_F5);const auto pending=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==pending,"F5/F9 changed pending ready/harvest identities");
        select(*view,renderer,view->world().building(scene.fields[0]).cell);
        capture(*view,renderer,output/"seasonal-wheat-ready.png");
        for(const auto size:{SDL_Point{600,400},SDL_Point{900,600},SDL_Point{1280,800}}) {
            check(SDL_SetWindowSize(window,size.x,size.y),"Resize seasonal inspector");view->update(0);
            check(view->render(),"Submit resized seasonal layout");
            if(view->layout().map.w>0 && view->layout().map.h>0)
                select(*view,renderer,view->world().building(scene.fields[0]).cell);
            if(view->layout().panel_open)seasonal_inspector(*view);
            else check(size.x==600 && size.y==400,"Seasonal inspector disappeared above the existing compact-layout threshold");
            pure_frames(*view);
            capture(*view,renderer,output/("seasonal-ready-"+std::to_string(size.x)+".png"));
        }
        const auto hemp_channel=n::hemp(view->world(),scene.hemp_farm).courier_id;
        bool loaded_hemp=false;
        for(unsigned i=0;i<3000 && !loaded_hemp;++i) {
            loaded_hemp=view->world().seasonal_hemp_grower_cargo(hemp_channel)==1;
            if(!loaded_hemp)view->tick_once();
        }
        check(loaded_hemp,"Paid seasonal Hemp never made real returning Grower cargo");
        const auto hemp_ground=view->courier_ground_position(hemp_channel);
        check(hemp_ground && oe::walker_live_visible(view->world().courier(hemp_channel)),"Real Hemp Grower lost its physical visible position");
        view->set_tool(5);center(*view,*hemp_ground);check(view->render(),"Submit real Hemp marker frame");
        click(*view,renderer,view->camera().world_to_screen(*hemp_ground));
        check(view->input_diagnostic_state().selected_walker==static_cast<unsigned>(hemp_channel) && view->selected_building()==scene.hemp_farm,
            "Normal Hemp marker pixels did not select their real source");
        const auto hemp_text=joined(view->inspection_lines());
        check(hemp_text.find("Hemp Grower")!=std::string::npos && hemp_text.find("Hemp cargo 1/1")!=std::string::npos,
            "Selected Hemp Grower inspector lost actual cargo or authored identity");
        seasonal_inspector(*view);pure_frames(*view);capture(*view,renderer,output/"seasonal-hemp-cargo.png");
        key(*view,SDLK_F5);const auto hemp_save=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==hemp_save,"F5/F9 changed a real Hemp pickup/return commitment");
        sim::CourierState hemp;hemp.role=sim::CourierRole::OriginalSeasonalHempGrower;
        check(!oe::walker_live_visible(hemp) && !oe::walker_visual_role(hemp.role),"Idle Hemp gained a body or unqualified Original art");
        hemp.phase=sim::CourierPhase::Returning;check(oe::walker_live_visible(hemp),"Real returning Hemp marker is hidden");
        view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Seasonal menu Z/legacy selectors, nine normal inspectors, boundary/ready F5/F9, real Hemp cargo/pixel picking/F5/F9, pure frames and viewport captures PASS\n";
        return 0;
    } catch(const std::exception& error) {
        oe::performance::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
