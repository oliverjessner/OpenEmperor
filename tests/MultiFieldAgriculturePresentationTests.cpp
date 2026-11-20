// Independent paid SDL scenario; native-window/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "MultiFieldAgricultureFixture.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace q=oe_irrigation_fixture;
namespace n=oe_multi_field_fixture;namespace fs=std::filesystem;
using f::check;using Point=oe::scene::Point;
void key(oe::menu::MenuSession& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;
    menu.handle_event(e);menu.advance();
}
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Input closed Multi-field Agriculture");
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
    throw std::runtime_error("Paused Multi-field Agriculture camera did not converge");
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
    check(view.render(),"Submit camera before building selection");click(view,renderer,view.camera().world_to_screen(ground));
}
bool contains(const std::vector<std::string>& lines,std::string_view fragment) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(fragment)!=std::string::npos;});
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h),"Capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Private capture target");
    check(view.render(),"Multi-field Agriculture render");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Private capture output");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore render target");
}

void menu_path(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    check(menu.state()==oe::menu::MenuSession::State::MainMenu && !menu.multi_field_agriculture_setup(),
        "Multi-field mode changed general defaults");
    for(const char* label:{"G Agriculture Irrigation (rule 26)","P Playable Agriculture (rule 27)",
            "F Playable Fishing (rule 28)","Y Playable Hunting (rule 29)","C Multi-field Agriculture (30)"}) {
        const auto rect=menu.diagnostic_button_rect(label);
        check(rect && rect->x>=0 && rect->x+rect->w<=600 && rect->y>=0 && rect->y+rect->h<=358,
            "Regular agriculture selector escapes minimum menu aperture");
    }
    check(menu.render(),"Render explicit Multi-field menu");
    const auto rect=*menu.diagnostic_button_rect("C Multi-field Agriculture (30)");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"C button transform");
    const auto mouse=[&](Uint32 type) {
        SDL_Event e{};e.type=type;e.button.windowID=SDL_GetWindowID(window);e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
        check(SDL_PushEvent(&e),"Queue C menu pointer");while(SDL_PollEvent(&e))menu.handle_event(e);
    };
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN);mouse(SDL_EVENT_MOUSE_BUTTON_UP);menu.advance();
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.multi_field_agriculture_setup(),"C button selected another generation");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_Y);
    check(menu.playable_hunting_setup() && !menu.multi_field_agriculture_setup(),"Historical Y selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_C);
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.multi_field_agriculture_setup() && menu.original_agriculture_setup(),
        "C shortcut did not select explicit Multi-field setup");
    key(menu,SDLK_P);check(!menu.original_paid_starter(),"Multi-field Empty city selection failed");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking && std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==30,"Multi-field preflight did not bind30");
    key(menu,SDLK_RETURN);check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Multi-field loading frame");menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox())throw std::runtime_error("Multi-field menu Start: "+menu.message());
    check(menu.sandbox()->world().multi_field_agriculture_enabled() && menu.sandbox()->world().rule_version()==30 && menu.sandbox()->paused(),
        "Menu did not create paused Rule30 World");
    menu.shutdown();
}

} // namespace
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        f::Temp temp;temp.root=fs::canonical(temp.root);n::Fixture source;
        fs::create_directories(temp.root/"data/Model");
        q::f::write_text(temp.root/"data/Model/EmperorBuildingModels.txt",q::f::read_text(source.root.data/"Model/EmperorBuildingModels.txt"));
        // The paid city below uses source-grid coordinates through153. Bind
        // the same full226-cell authored geography as Hunting presentation,
        // rather than the older84-cell starter-map helper.
        f::write(temp.root/"data/Cities/A.map",oe_fishing_fixture::encoded_map(source.raw,source.fertility,true));
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted Multi-field SDL");
        check(SDL_CreateWindowAndRenderer("Multi-field scripted acceptance",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create Multi-field renderer");
        menu_path(temp.root/"data",temp.root/"menu",window,renderer);
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,
            oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_multi_field_agriculture(std::move(session));
        view->configure_save(temp.root/"data","Cities/A.map",temp.root/"multi-field.save.json");view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view->execute({type,cell});
            if(!result.accepted || !result.changed)throw std::runtime_error("Paid Multi-field UI construction "+std::to_string(static_cast<unsigned>(type))+
                " at "+std::to_string(cell.x)+","+std::to_string(cell.y)+": "+result.reason);
            return view->world().building_owner_at(cell);
        };
        for(int x=89;x<=153;++x)for(const int y:{114,117})buy(sim::CommandType::PlaceRoad,{x,y});
        for(int y=115;y<=116;++y)for(const int x:{89,153})buy(sim::CommandType::PlaceRoad,{x,y});
        for(int x=90;x<150;x+=3)buy(sim::CommandType::PlaceHousehold,{x,112});
        const auto market=*buy(sim::CommandType::PlaceMarket,{134,118});
        buy(sim::CommandType::PlaceOriginalWaterWell,{138,118});buy(sim::CommandType::PlaceOriginalAncestorShrine,{142,118});
        buy(sim::CommandType::PlaceFireWatch,{110,118});buy(sim::CommandType::PlaceFireWatch,{146,118});
        const sim::Cell farm_cell{98,118},second_cell{105,118};
        const auto farm=*buy(sim::CommandType::PlaceOriginalFoodFarm,farm_cell);
        const auto second=*buy(sim::CommandType::PlaceOriginalFoodFarm,second_cell);
        const auto place=[&](sim::BuildingId parent,std::size_t crop,sim::Cell cell) {
            sim::Command command{n::m::crop_commands.at(crop),cell};command.building_id=parent;
            check(view->execute(command).accepted,"Paid bound UI field");return *view->world().building_owner_at(cell);
        };
        std::vector<sim::BuildingId> fields;
        fields.push_back(place(farm,0,{101,118}));fields.push_back(place(farm,1,{101,119}));fields.push_back(place(farm,2,{101,120}));
        // Overlapping work areas require an explicit paid owner. Both keyboard
        // and pointer construction pass through the same placement command.
        select(*view,renderer,second_cell);check(view->selected_building()==second,"Select second paid Farm");
        key(*view,SDLK_W);check(view->tool()==19,"Inherited Wheat action");
        const auto overlap=oe::maps::terrain_ground({103,118},1);center(*view,overlap);check(view->render(),"Submit overlap placement camera");
        click(*view,renderer,view->camera().world_to_screen(overlap));
        const auto second_field=view->world().building_owner_at({103,118});
        check(second_field && n::field(view->world(),*second_field).farm_id==second,"Explicit selected Farm lost overlap ownership");
        select(*view,renderer,{101,118});check(view->selected_building()==fields[0],"Select paid field");
        key(*view,SDLK_C,SDL_KMOD_SHIFT);check(view->tool()==44,"Inherited Cabbage action");
        const auto target=oe::maps::terrain_ground({101,121},1);center(*view,target);check(view->render(),"Submit parent-bound field camera");
        click(*view,renderer,view->camera().world_to_screen(target));
        const auto cabbage=view->world().building_owner_at({101,121});
        check(cabbage && n::field(view->world(),*cabbage).farm_id==farm,"Selected field did not bind its existing parent");fields.push_back(*cabbage);
        fields.push_back(place(farm,4,{102,118}));fields.push_back(place(farm,0,{102,119}));
        fields.push_back(place(farm,1,{102,120}));fields.push_back(place(farm,4,{102,121}));
        select(*view,renderer,farm_cell);auto lines=view->inspection_lines();
        if(!contains(lines,"Farm workers") || !contains(lines,"Bound fields 8/8") || !contains(lines,"Farm understaffed") || !contains(lines,"Food courier:")) {
            std::string detail;for(const auto& line:lines)detail+="["+line+"]";
            throw std::runtime_error("Farm inspector hides paid bindings, workforce, blockers or courier: "+detail);
        }
        for(const auto field:fields)check(contains(lines,"Field #"+std::to_string(static_cast<unsigned>(field))),"Farm inspector omits a bound field");
        {
            const auto before=view->world().snapshot();const auto fixed=view->diagnostic_operation_toggle_rect();
            const auto wanted="Field #"+std::to_string(static_cast<unsigned>(fields.back()));
            const auto last=std::find_if(lines.begin(),lines.end(),[&](const auto& line){return line.find(wanted)!=std::string::npos;});
            check(last!=lines.end(),"Eighth field is absent from scroll document");
            const auto index=static_cast<std::size_t>(last-lines.begin());
            check(view->inspector_layout().scroll_limit>0,"Eight-field inspector has no bounded scroll range");
            for(unsigned step=0;step<200 && view->inspector_layout().detail_row_rect(index).h<=0;++step) {
                const auto content=view->inspector_layout().content;float wx=0,wy=0;
                check(SDL_RenderCoordinatesToWindow(renderer,float(content.x+content.w/2),float(content.y+content.h/2),&wx,&wy),"Inspector scroll transform");
                SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=-1;wheel.wheel.mouse_x=wx;wheel.wheel.mouse_y=wy;event(*view,wheel);
            }
            const auto scrolled=view->inspector_layout();const auto after=view->diagnostic_operation_toggle_rect();
            check(scrolled.scroll>0 && scrolled.scroll<=scrolled.scroll_limit && scrolled.detail_row_rect(index).h>0,
                "Eighth field cannot be reached through ordinary panel scrolling");
            check(fixed.x==after.x && fixed.y==after.y && fixed.w==after.w && fixed.h==after.h && fixed.h>0,
                "Scrolling field history displaced fixed Farm operation controls");
            check(before==view->world().snapshot() && view->render(),"Inspector scrolling mutated gameplay or failed render");
        }
        for(unsigned tick=0;tick<4000 && n::farm_work(view->world(),farm)<40;++tick)view->tick_once();
        check(n::farm_work(view->world(),farm)>=40,"Actual immigration did not earn shared Farm work");
        const auto paused=fields[0];select(*view,renderer,{101,118});
        auto toggle=view->diagnostic_operation_toggle_rect();check(toggle.h>0,"Rule30 field has no normal operation toggle");
        click(*view,renderer,{double(toggle.x+toggle.w/2),double(toggle.y+toggle.h/2)});
        check(!view->world().building(paused).operating_enabled,"Normal field operation button failed to pause");
        const auto stopped=n::work(view->world(),paused).productive_ticks,all_before=n::farm_work(view->world(),farm);
        for(unsigned tick=0;tick<40;++tick)view->tick_once();
        check(n::work(view->world(),paused).productive_ticks==stopped && n::farm_work(view->world(),farm)>all_before,
            "Paused field received work or blocked all eligible peers");
        select(*view,renderer,{101,118});lines=view->inspection_lines();
        check(contains(lines,"Field paused") && contains(lines,"Work total "+std::to_string(stopped)) && contains(lines,"Bound Farmhouse #"),
            "Field inspector hides independent progress or paused reason");
        toggle=view->diagnostic_operation_toggle_rect();check(toggle.h>0,"Paused field lost normal operation toggle");
        click(*view,renderer,{double(toggle.x+toggle.w/2),double(toggle.y+toggle.h/2)});
        check(view->world().building(paused).operating_enabled,"Normal field operation button failed to resume");
        bool produced=false;
        for(unsigned tick=0;tick<12000 && !produced;++tick) {
            view->tick_once();produced=std::all_of(fields.begin(),fields.end(),[&](auto id){return n::field(view->world(),id).harvested>0;});
        }
        check(produced,"Paid eight-field UI Farm never harvested every bound crop");
        std::optional<sim::CourierId> loaded;
        for(unsigned tick=0;tick<6000 && !loaded;++tick) {
            for(const auto& courier:view->world().couriers())
                if(courier.owner==farm && view->world().original_food_cargo(courier.id)>0) {loaded=courier.id;break;}
            if(!loaded)view->tick_once();
        }
        check(loaded.has_value(),"Paid multi-field Farm never exposed a real typed Food cart");
        const auto typed_cargo=view->world().original_food_cargo(*loaded);
        check(typed_cargo>0 && view->world().courier(*loaded).cargo==0,"Food cart lost sole typed cargo authority");
        select(*view,renderer,farm_cell);lines=view->inspection_lines();
        check(contains(lines,"cargo "+std::to_string(typed_cargo)),"Farm inspector displays an empty legacy cargo mirror");
        if(!contains(lines,"Wheat") || !contains(lines,"Millet") || !contains(lines,"Rice") || !contains(lines,"Cabbage") || !contains(lines,"Bean Curd (Soy)") ||
            !contains(lines,"Actually harvested") || !contains(lines,"Last worked field") || !contains(lines,"Next eligible field")) {
            std::string detail;for(const auto& line:lines)detail+="["+line+"]";
            throw std::runtime_error("Farm inspector lost typed crop work or fair cursor: "+detail);
        }
        check(view->world().building(farm).progress==0 && view->world().hunting_food_state() && !view->world().fishing_food_state() && !view->world().original_agriculture_state(),
            "Rule30 UI duplicated historical Farm clocks or Food authority");
        const auto output=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/agri-mac05/presentation"/temp.root.filename();fs::create_directories(output);
        capture(*view,renderer,output/"multi-field-farm-inspector.png");
        const auto snapshot=view->world().snapshot();const auto prepared=view->agriculture_inspection_prepare_count();
        oe::performance::set_enabled(true);oe::performance::reset();
        for(unsigned frame=0;frame<10;++frame){check(view->render(),"Prepared Multi-field frame");(void)view->inspection_lines();}
        for(std::size_t c=0;c<static_cast<std::size_t>(oe::performance::Counter::Count);++c)
            check(!oe::performance::counter(static_cast<oe::performance::Counter>(c)),"Multi-field frame performed authority, source or route work");
        oe::performance::set_enabled(false);
        check(snapshot==view->world().snapshot() && prepared==view->agriculture_inspection_prepare_count(),"Render rebuilt Farm work or inspection preparation");
        key(*view,SDLK_F5);check(oe::persistence::read_save(view->save_path()).source_schema_version==46,"F5 did not write exact schema46");
        key(*view,SDLK_F9);check(snapshot==view->world().snapshot(),"F9 changed field work/cursor or physical Food state");
        check(view->execute(sim::set_building_operation(market,false)).accepted,"Pause Market to expose finite Farm headroom");
        bool full=false;for(unsigned tick=0;tick<5000 && !full;++tick) {
            view->tick_once();for(const auto& stock:view->world().hunting_food_state()->stocks)if(stock.building_id==farm)
                full=std::accumulate(stock.amounts.begin(),stock.amounts.end(),0)==8;
        }
        check(full,"Paused Market did not expose real Farm capacity");select(*view,renderer,farm_cell);
        check(contains(view->inspection_lines(),"Stock/reservations full"),"Farm inspector hides finite capacity blocker");
        std::vector<sim::Cell> entrances;for(const auto& entrance:view->world().building_entrances(farm))
            if(std::find(entrances.begin(),entrances.end(),entrance.road_cell)==entrances.end())entrances.push_back(entrance.road_cell);
        for(const auto cell:entrances)buy(sim::CommandType::RemoveRoad,cell);
        check(!entrances.empty() && view->world().building_entrances(farm).empty(),"Farm road cut left an entrance");
        // Paid road removal selects its affected source cell. Reopen the paid
        // Farm through the real pointer path before reading its own inspector.
        select(*view,renderer,farm_cell);check(view->selected_building()==farm,"Reselect disconnected Farm");
        check(contains(view->inspection_lines(),"Road entrance missing"),"Farm inspector hides disconnected source");
        capture(*view,renderer,output/"multi-field-road-blocked.png");
        n::valid(view->world());view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Multi-field C menu generation, explicit Farm binding, eight-field progress/blockers, pure SDL frames and F5/F9 PASS\n";return 0;
    } catch(const std::exception& error) {SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
