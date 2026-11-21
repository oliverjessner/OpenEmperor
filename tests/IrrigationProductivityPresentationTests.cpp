// Independent paid SDL scenario; native-window/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "IrrigationProductivityFixture.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace q=oe_irrigation_fixture;
namespace n=oe_irrigation_productivity_fixture;namespace seasons=n::s;namespace fs=std::filesystem;
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
void show_row(oe::SandboxView& view,SDL_Renderer* renderer,std::string_view wanted) {
    const auto lines=view.inspection_lines();const auto found=std::find_if(lines.begin(),lines.end(),
        [&](const auto& line){return line.find(wanted)!=std::string::npos;});
    check(found!=lines.end(),"Requested inspection evidence is absent");
    const auto index=static_cast<std::size_t>(found-lines.begin());
    for(unsigned step=0;step<512 && view.inspector_layout().detail_row_rect(index).h<=0;++step) {
        const auto content=view.inspector_layout().content;float x=0,y=0;
        check(SDL_RenderCoordinatesToWindow(renderer,float(content.x+content.w/2),float(content.y+content.h/2),&x,&y),
            "Inspector scroll coordinate conversion failed");
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=-1;wheel.wheel.mouse_x=x;wheel.wheel.mouse_y=y;
        event(view,wheel);
    }
    check(view.inspector_layout().detail_row_rect(index).h>0,"Normal scrolling cannot expose actual work evidence");
    check(view.render(),"Submit actual inspection evidence");
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
    if(menu.state()!=oe::menu::MenuSession::State::MainMenu || menu.irrigation_productivity_setup())
        throw std::runtime_error("Seasons initial menu state "+std::to_string(static_cast<unsigned>(menu.state()))+": "+menu.message());
    for(const char* label:{"G Agriculture Irrigation (rule 26)","P Playable Agriculture (rule 27)",
            "F Playable Fishing (rule 28)","Y Playable Hunting (rule 29)","E Physical Growers (31)","Z Seasons (32)","X Irrigation yields (33)"}) {
        const auto rect=menu.diagnostic_button_rect(label);
        check(rect && rect->x>=0 && rect->x+rect->w<=600 && rect->y>=0 && rect->y+rect->h<=358,
            "Regular seasonal selector escapes the minimum menu aperture");
    }
    check(menu.render(),"Render regular Seasons selector");
    const auto rect=*menu.diagnostic_button_rect("X Irrigation yields (33)");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"X pointer transform");
    const auto mouse=[&](Uint32 type) {
        SDL_Event e{};e.type=type;e.button.windowID=SDL_GetWindowID(window);e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
        menu.handle_event(e);
    };
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN);mouse(SDL_EVENT_MOUSE_BUTTON_UP);menu.advance();
    check(menu.irrigation_productivity_setup(),"X pointer selected another generation");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_E);
    check(menu.physical_food_growers_setup() && !menu.irrigation_productivity_setup(),"Historical E selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_C);
    check(menu.multi_field_agriculture_setup() && !menu.irrigation_productivity_setup(),"Historical C selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_X);
    check(menu.original_agriculture_setup() && menu.irrigation_productivity_setup(),"X shortcut lost the explicit lineage");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_Z);
    check(menu.seasonal_agriculture_setup() && !menu.irrigation_productivity_setup(),"Historical Z selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_X);
    key(menu,SDLK_P);check(!menu.original_paid_starter(),"Productivity Empty city selection failed");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking && std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==33,"Productivity preflight did not bind33");
    key(menu,SDLK_RETURN);check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Productivity loading frame");menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox())throw std::runtime_error("Productivity menu Start: "+menu.message());
    check(menu.sandbox()->world().irrigation_productivity_enabled() && menu.sandbox()->world().rule_version()==33 && menu.sandbox()->paused(),
        "Menu did not create a paused Rule33 World");
    menu.shutdown();
}
std::string joined(const std::vector<std::string>& lines) {
    std::string value;for(const auto& line:lines){value+=line;value+=' ';}return value;
}
void productivity_inspector(const oe::SandboxView& view,sim::BuildingId subject) {
    const auto text=joined(view.inspection_lines());const auto tick=view.world().ticks();
    for(const auto& fragment:{"Elapsed cycle "+std::to_string(tick/9792),"Outer slot "+std::to_string((tick/816)%12),
            "Subslot "+std::to_string((tick/51)%16),"Phase "+std::to_string(tick%51),
            std::string("Growing slots"),std::string("Next growing opening"),std::string("Wasted work"),
            std::string("expired ready"),std::string("Irrigation productivity"),std::string("AUTHORED")})
        if(text.find(fragment)==std::string::npos)
            throw std::runtime_error("Normal seasonal inspector missing ["+fragment+"] at tick "+std::to_string(tick)+": "+text);
    const auto status=view.world().irrigation_productivity_status(subject);
    check(status.supported,"Productivity inspector subject is not an actual supported paid crop");
    for(const auto& required:{"Current growth increment "+std::to_string(status.next_increment),
            "Wet carry "+std::to_string(status.carry)+"/4", "Bonus earned "+std::to_string(status.bonus_work),
            "clipped at80 "+std::to_string(status.clipped_bonus),std::string("Work blocker:"),
            std::string("Next productive opportunity:"),std::string("Supplying Ditch"),
            std::string("Actual water ")+(status.water_available ? "WET":"DRY")})
        if(text.find(required)==std::string::npos)
            throw std::runtime_error("Normal productivity inspector differs from actual World status: "+required);
    check(text.find("Base yield; no irrigation bonus")==std::string::npos && text.find("yield inactive")==std::string::npos,
        "New productivity inspector retained a historical inactive-yield claim");
    check(text.find("Only real eligible wet growth advances carry")!=std::string::npos,"Inspector implies passive carry credit");
    check(text.find("Original period query is not its quantity callback veto")!=std::string::npos,
        "Productivity inspector conflates observed queries with authored work gates");
    for(const auto name:{"January","February","March","April","May","June","July","August",
            "September","October","November","December"})
        check(text.find(name)==std::string::npos,"Inspector invented Original month names");
}
void pure_frames(oe::SandboxView& view) {
    const auto before=view.world().snapshot();const auto prepared=view.agriculture_inspection_prepare_count();
    oe::performance::set_enabled(true);oe::performance::reset();
    for(unsigned i=0;i<9;++i)check(view.render(),"Productivity pure frame failed");
    for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Productivity render performed World/resource/navigation work");
    oe::performance::set_enabled(false);
    check(view.world().snapshot()==before && view.agriculture_inspection_prepare_count()==prepared,
        "Productivity frames mutated paid state or rebuilt prepared diagnosis");
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
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize isolated productivity SDL");
        check(SDL_CreateWindowAndRenderer("Irrigation Productivity scripted acceptance",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),
            "Create productivity SDL renderer");
        menu_path(source.root.data,temp.root/"menu",window,renderer);
        check(SDL_SetWindowSize(window,1440,1000),"Resize normal productivity evidence window");
        auto session=oe::maps::load_stored_map_session(source.root.data,oe_influence_fixture::map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_irrigation_productivity(std::move(session),std::nullopt,false);
        const auto save_path=temp.root/"irrigation49.save.json";
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
        seasons::Scene scene;auto& materials=scene.materials;
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
            scene.fields[i]=buy(n::q::m::crop_commands[i],{x+(i==0?5:3),97},scene.farms[i]);
        }
        buy(sim::CommandType::PlaceRoad,{93,97});buy(sim::CommandType::PlaceRoad,{94,97});
        scene.hemp_farm=buy(sim::CommandType::PlaceOriginalHempFarm,{132,98});
        scene.hemp_field=buy(sim::CommandType::PlaceOriginalHempField,{134,98},scene.hemp_farm);
        const auto pump=buy(sim::CommandType::PlaceIrrigationPump,n::pump_origin);
        for(int y=93;y<=104;++y)buy(sim::CommandType::PlaceRoad,{87,y});
        buy(sim::CommandType::PlaceRoad,{88,104});
        std::vector<sim::Cell> ditches;
        for(int x=85;x<=149;++x)ditches.push_back({x,94});
        for(int y=95;y<=114;++y)ditches.push_back({85,y});
        for(int x=86;x<=149;++x)ditches.push_back({x,114});
        for(const auto cell:ditches)buy(sim::CommandType::PlaceIrrigationDitch,cell);
        check(ditches.size()>=21 && !view->world().irrigation_ditch_wet(ditches.front()),
            "Paid unstaffed Pump supplied construction-time water");
        std::vector<sim::BuildingId> subjects(scene.fields.begin(),scene.fields.end());
        subjects.insert(subjects.end(),{scene.hemp_field,scene.materials.tea_plant,scene.materials.lacquer_plant,scene.materials.mulberry_plant});
        for(const auto id:subjects) {
            select(*view,renderer,view->world().building(id).cell);productivity_inspector(*view,id);pure_frames(*view);
            check(contains(view->inspection_lines(),"Off-season"),"Initial off-season inspector hides its productive veto");
        }
        const auto output=fs::current_path()/".local/agri-mac08/presentation";fs::create_directories(output);
        select(*view,renderer,view->world().building(scene.fields[0]).cell);
        capture(*view,renderer,output/"productivity-wheat-offseason.png");
        const auto paused=view->world().snapshot();view->update(.2);check(view->world().snapshot()==paused,"Paused UI advanced the sole World clock");
        while(view->world().ticks()<1631)view->tick_once();productivity_inspector(*view,scene.fields[0]);
        check(contains(view->inspection_lines(),"Off-season"),"Wheat opened before exact start slot");
        key(*view,SDLK_F5);const auto before_open=view->world().snapshot();check(fs::exists(save_path),"F5 omitted schema49 save");
        view->tick_once();check(view->world().ticks()==1632,"Productivity single step used another clock");productivity_inspector(*view,scene.fields[0]);
        check(contains(view->inspection_lines(),"Actual growing window OPEN"),"Exact Wheat opening is absent from the normal inspector");
        key(*view,SDLK_F9);check(view->world().snapshot()==before_open && view->paused(),"F9 changed exact productivity boundary state");
        view->tick_once();
        bool wet_carry=false;
        for(unsigned i=0;i<400 && !wet_carry;++i) {
            const auto status=view->world().irrigation_productivity_status(scene.fields[0]);
            wet_carry=status.work_eligible && status.water_available && status.carry==3 && status.progress<78;
            if(!wet_carry)view->tick_once();
        }
        check(wet_carry,"Actual paid wet Wheat never reached a pending quarter increment");
        select(*view,renderer,view->world().building(scene.fields[0]).cell);
        productivity_inspector(*view,scene.fields[0]);
        const auto wet_before=n::work(view->world(),scene.fields[0]);
        const auto wet_base=n::base_work(view->world(),scene.fields[0]);
        check(view->world().irrigation_productivity_status(scene.fields[0]).next_increment==2,
            "Current eligible wet quarter does not advertise its actual two-unit increment");
        key(*view,SDLK_F5);const auto wet_checkpoint=view->world().snapshot();view->tick_once();
        const auto wet_after=n::work(view->world(),scene.fields[0]);
        check(wet_after.wet_work_ticks==wet_before.wet_work_ticks+1 && wet_after.bonus_work==wet_before.bonus_work+1 &&
            n::base_work(view->world(),scene.fields[0])==wet_base+1,"Wet UI Step did not earn one actual base plus one rational bonus unit");
        key(*view,SDLK_F9);check(view->world().snapshot()==wet_checkpoint,"F9 changed wet carry before its actual bonus tick");
        check(view->execute(sim::set_building_operation(pump,false)).accepted,"Paid Pump pause refused");
        const auto dry_status=view->world().irrigation_productivity_status(scene.fields[0]);
        check(!dry_status.water_available && dry_status.carry==3 && dry_status.next_increment==1,
            "Pump pause did not immediately remove bonus while retaining actual carry/base work");
        select(*view,renderer,view->world().building(scene.fields[0]).cell);productivity_inspector(*view,scene.fields[0]);
        show_row(*view,renderer,"Wet carry 3/4");
        pure_frames(*view);capture(*view,renderer,output/"productivity-dry-retained-carry.png");
        key(*view,SDLK_F5);const auto dry_checkpoint=view->world().snapshot();view->tick_once();
        const auto dry_after=n::work(view->world(),scene.fields[0]);
        check(dry_after.wet_work_ticks==wet_before.wet_work_ticks && dry_after.bonus_work==wet_before.bonus_work &&
            n::base_work(view->world(),scene.fields[0])==wet_base+1,"Dry UI Step granted wet work or stopped valid base work");
        key(*view,SDLK_F9);check(view->world().snapshot()==dry_checkpoint,"F9 changed dry carry or authority");
        check(view->execute(sim::set_building_operation(pump,true)).accepted &&
            !view->world().irrigation_productivity_status(scene.fields[0]).water_available,
            "Pump restart skipped its genuine next irrigation update");
        bool wet_again=false;for(unsigned i=0;i<100 && !wet_again;++i) {
            view->tick_once();wet_again=view->world().irrigation_productivity_status(scene.fields[0]).water_available;
        }
        check(wet_again,"Paid Pump restart never supplied its later real irrigation phase");
        for(const auto subject:subjects) {
            check(view->world().irrigation_productivity_status(subject).water_available,
                "The actual supplied network does not reach every supported UI crop");
            select(*view,renderer,view->world().building(subject).cell);productivity_inspector(*view,subject);pure_frames(*view);
        }
        key(*view,SDLK_V,SDL_KMOD_SHIFT);pure_frames(*view);
        capture(*view,renderer,output/"productivity-water-overlay.png");
        key(*view,SDLK_V,SDL_KMOD_SHIFT);
        select(*view,renderer,view->world().building(scene.fields[0]).cell);
        bool ready=false;
        for(unsigned i=0;i<1800 && !ready;++i) {
            ready=seasons::field(view->world(),scene.fields[0]).ready_tick.has_value();
            if(!ready)view->tick_once();
        }
        check(ready,"Paid physical Wheat never reached seasonal readiness");productivity_inspector(*view,scene.fields[0]);
        const auto pending_text=joined(view->inspection_lines());
        check(pending_text.find("Ready crop expires before tick")!=std::string::npos && pending_text.find("At-field harvest work")!=std::string::npos,
            "Pending crop inspector hides expiry or real harvest work");
        key(*view,SDLK_F5);const auto pending=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==pending,"F5/F9 changed pending ready/harvest identities");
        select(*view,renderer,view->world().building(scene.fields[0]).cell);
        show_row(*view,renderer,"Actual work 80/80");
        capture(*view,renderer,output/"productivity-wheat-ready.png");
        for(const auto size:{SDL_Point{600,400},SDL_Point{900,600},SDL_Point{1280,800}}) {
            check(SDL_SetWindowSize(window,size.x,size.y),"Resize seasonal inspector");view->update(0);
            check(view->render(),"Submit resized seasonal layout");
            if(view->layout().map.w>0 && view->layout().map.h>0)
                select(*view,renderer,view->world().building(scene.fields[0]).cell);
            if(view->layout().panel_open) {
                productivity_inspector(*view,scene.fields[0]);show_row(*view,renderer,"Current growth increment");
            }
            else check(size.x==600 && size.y==400,"Productivity inspector disappeared above the existing compact-layout threshold");
            pure_frames(*view);
            capture(*view,renderer,output/("productivity-ready-"+std::to_string(size.x)+".png"));
        }
        check(SDL_SetWindowSize(window,1440,1000),"Restore normal cargo evidence window");view->update(0);
        sim::CourierId food_channel{};
        for(const auto& farm:view->world().physical_food_growers_state()->farms)
            if(farm.farm_id==scene.farms[0])food_channel=farm.courier_id;
        check(static_cast<unsigned>(food_channel)>0,"Paid Wheat Farm lost its counted physical Grower channel");
        bool loaded_on_road=false;
        for(unsigned attempt=0;attempt<400 && !loaded_on_road;++attempt) {
            const auto& actor=view->world().courier(food_channel);
            loaded_on_road=view->world().physical_food_grower_cargo(food_channel)==1 && !actor.path.empty() &&
                view->world().road_cell(actor.path.at(actor.path_vertex));
            if(!loaded_on_road)view->tick_once();
        }
        check(loaded_on_road,"Paid Food Grower never physically returned over its two paid road cells");
        const auto food_ground=view->courier_ground_position(food_channel);check(food_ground.has_value(),"Loaded Food Grower lost its position");
        view->set_tool(5);center(*view,*food_ground);check(view->render(),"Submit actual Food marker on its road");
        click(*view,renderer,view->camera().world_to_screen(*food_ground));
        check(view->input_diagnostic_state().selected_walker==static_cast<unsigned>(food_channel) &&
            view->selected_building()==scene.farms[0],"Normal Food marker on Road did not select its true Farm");
        check(contains(view->inspection_lines(),"Grower crop cargo 1/1") &&
            !contains(view->inspection_lines(),"Road cell"),"Road inspection hid the selected physical Grower's true cargo");
        show_row(*view,renderer,"Grower crop cargo 1/1");pure_frames(*view);
        capture(*view,renderer,output/"productivity-food-road-cargo.png");
        key(*view,SDLK_F5);const auto road_cargo=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==road_cargo,"F5/F9 changed real Food road cargo or its earned irrigation work");
        const auto hemp_channel=seasons::hemp(view->world(),scene.hemp_farm).courier_id;
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
        productivity_inspector(*view,scene.hemp_field);show_row(*view,renderer,"Hemp cargo 1/1");
        pure_frames(*view);capture(*view,renderer,output/"productivity-hemp-cargo.png");
        key(*view,SDLK_F5);const auto hemp_save=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==hemp_save,"F5/F9 changed a real Hemp pickup/return commitment");
        sim::CourierState hemp;hemp.role=sim::CourierRole::OriginalSeasonalHempGrower;
        check(!oe::walker_live_visible(hemp) && !oe::walker_visual_role(hemp.role),"Idle Hemp gained a body or unqualified Original art");
        hemp.phase=sim::CourierPhase::Returning;check(oe::walker_live_visible(hemp),"Real returning Hemp marker is hidden");
        const auto historical=sim::World::create_seasonal_agriculture(view->world().map_permissions(),
            view->world().original_rules_model(),view->world().original_terrain_facts());
        const auto historical_path=temp.root/"historical48.save.json";
        const auto historical_document=openemperor::persistence::make_document(source.root.data,oe_influence_fixture::map_relative,
            view->world().map_permissions()->building_mask(),historical);
        openemperor::persistence::write_save(historical_path,historical_document,source.root.data,
            view->world().map_permissions()->building_mask());
        const auto retained=view->world().snapshot();const auto loads=view->load_generation(),saves=view->save_generation();
        view->configure_save(source.root.data,oe_influence_fixture::map_relative,historical_path);
        key(*view,SDLK_F9);check(view->world().snapshot()==retained && view->load_generation()==loads,
            "Rule33 F9 silently migrated a historical Rule32 checkpoint");
        key(*view,SDLK_F5);check(view->world().snapshot()==retained && view->save_generation()==saves &&
            openemperor::persistence::read_save(historical_path).world==historical.snapshot(),
            "Rule33 F5 overwrote a historical schema48 target");
        view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Irrigation menu X/legacy selectors, nine normal wet/dry inspectors, actual quarter work, retained carry/restart, exact wet/dry F5/F9, historical32 refusal, real Hemp cargo/picking and pure frames PASS\n";
        return 0;
    } catch(const std::exception& error) {
        oe::performance::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
