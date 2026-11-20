// Independent paid SDL scenario; native-window/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "PhysicalFoodGrowersFixture.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace q=oe_irrigation_fixture;
namespace n=oe_physical_food_growers_fixture;namespace fs=std::filesystem;
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
    check(menu.state()==oe::menu::MenuSession::State::MainMenu && !menu.physical_food_growers_setup(),
        "Physical Growers mode changed general defaults");
    for(const char* label:{"G Agriculture Irrigation (rule 26)","P Playable Agriculture (rule 27)",
            "F Playable Fishing (rule 28)","Y Playable Hunting (rule 29)","E Physical Growers (31)"}) {
        const auto rect=menu.diagnostic_button_rect(label);
        check(rect && rect->x>=0 && rect->x+rect->w<=600 && rect->y>=0 && rect->y+rect->h<=358,
            "Regular agriculture selector escapes minimum menu aperture");
    }
    check(menu.render(),"Render explicit Physical Growers menu");
    const auto rect=*menu.diagnostic_button_rect("E Physical Growers (31)");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"E button transform");
    const auto mouse=[&](Uint32 type) {
        SDL_Event e{};e.type=type;e.button.windowID=SDL_GetWindowID(window);e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
        check(SDL_PushEvent(&e),"Queue E menu pointer");while(SDL_PollEvent(&e))menu.handle_event(e);
    };
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN);mouse(SDL_EVENT_MOUSE_BUTTON_UP);menu.advance();
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.physical_food_growers_setup(),"E button selected another generation");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_Y);
    check(menu.playable_hunting_setup() && !menu.physical_food_growers_setup(),"Historical Y selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_C);
    check(menu.multi_field_agriculture_setup() && !menu.physical_food_growers_setup(),"Historical C selector changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_E);
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.physical_food_growers_setup() && menu.original_agriculture_setup(),
        "E shortcut did not select explicit Physical Growers setup");
    key(menu,SDLK_P);check(!menu.original_paid_starter(),"Physical Growers Empty city selection failed");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking && std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==31,"Physical Growers preflight did not bind31");
    key(menu,SDLK_RETURN);check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Physical Growers loading frame");menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox())throw std::runtime_error("Physical Growers menu Start: "+menu.message());
    check(menu.sandbox()->world().physical_food_growers_enabled() && menu.sandbox()->world().rule_version()==31 && menu.sandbox()->paused(),
        "Menu did not create paused Rule31 World");
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
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted Physical Growers SDL");
        check(SDL_CreateWindowAndRenderer("Physical Growers scripted acceptance",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create Physical Growers renderer");
        menu_path(temp.root/"data",temp.root/"menu",window,renderer);
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,
            oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_physical_food_growers(std::move(session));
        view->configure_save(temp.root/"data","Cities/A.map",temp.root/"physical-growers.save.json");view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        // Inspect actual Field/worker pixels at the ordinary1x action. The
        // full-map fit makes a1x1 plot only a few screen pixels at900x600.
        for(unsigned attempt=0;attempt<3 && view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
        check(view->camera().zoom==1.,"Ordinary zoom action did not reach1x before Field picking");
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view->execute({type,cell});
            if(!result.accepted || !result.changed)throw std::runtime_error("Paid Physical Growers UI construction "+std::to_string(static_cast<unsigned>(type))+
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
        // Keep the first paid work plot in front of the Farm painter body.
        fields.push_back(place(farm,0,{102,121}));fields.push_back(place(farm,1,{101,119}));fields.push_back(place(farm,2,{101,120}));
        // Overlapping work areas require an explicit paid owner. Both keyboard
        // and pointer construction pass through the same placement command.
        select(*view,renderer,second_cell);check(view->selected_building()==second,"Select second paid Farm");
        key(*view,SDLK_W);check(view->tool()==19,"Inherited Wheat action");
        const auto overlap=oe::maps::terrain_ground({103,118},1);center(*view,overlap);check(view->render(),"Submit overlap placement camera");
        click(*view,renderer,view->camera().world_to_screen(overlap));
        const auto second_field=view->world().building_owner_at({103,118});
        check(second_field && n::field(view->world(),*second_field).farm_id==second,"Explicit selected Farm lost overlap ownership");
        select(*view,renderer,{102,121});check(view->selected_building()==fields[0],"Select paid field");
        key(*view,SDLK_C,SDL_KMOD_SHIFT);check(view->tool()==44,"Inherited Cabbage action");
        const auto target=oe::maps::terrain_ground({101,121},1);center(*view,target);check(view->render(),"Submit parent-bound field camera");
        click(*view,renderer,view->camera().world_to_screen(target));
        const auto cabbage=view->world().building_owner_at({101,121});
        check(cabbage && n::field(view->world(),*cabbage).farm_id==farm,"Selected field did not bind its existing parent");fields.push_back(*cabbage);
        fields.push_back(place(farm,4,{102,118}));fields.push_back(place(farm,0,{102,119}));
        fields.push_back(place(farm,1,{102,120}));fields.push_back(place(farm,4,{101,118}));
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
        std::array<bool,4> checkpoints{};bool picked_worker=false;
        const auto observe=[&]() {
            const auto source=n::farm(view->world(),farm);if(!source.latest_trip || source.latest_trip->returned_tick)return;
            const auto trip=*source.latest_trip;const auto actor=view->world().courier(source.courier_id);
            int which=-1;
            if(trip.phase==sim::PhysicalFoodGrowerPhase::Outward && actor.edge_progress)which=0;
            else if(trip.phase==sim::PhysicalFoodGrowerPhase::Waiting && trip.wait_left)which=1;
            else if(trip.phase==sim::PhysicalFoodGrowerPhase::Working && trip.work_ticks)which=2;
            else if(trip.phase==sim::PhysicalFoodGrowerPhase::Returning && actor.edge_progress && view->world().physical_food_grower_cargo(actor.id)==1)which=3;
            if(which>=0 && !checkpoints[static_cast<std::size_t>(which)]) {
                const auto saved=view->world().snapshot();key(*view,SDLK_F5);
                check(oe::persistence::read_save(view->save_path()).source_schema_version==47,"Production Grower F5 omitted schema47");
                key(*view,SDLK_F9);check(saved==view->world().snapshot(),"F9 changed actual outward/wait/tending/loaded Grower phase");
                checkpoints[static_cast<std::size_t>(which)]=true;
            }
            if(!picked_worker && oe::walker_live_visible(actor)) {
                const auto ground=view->courier_ground_position(actor.id);check(ground.has_value(),"Active Food Grower has no prepared World projection");
                view->set_tool(5);center(*view,*ground);check(view->render(),"Submit actual Food Grower marker");
                const auto saved=view->world().snapshot();const auto screen=view->camera().world_to_screen(*ground);
                // Real Farm/Field art may occlude an active channel. Probe only
                // submitted marker body/label pixels through ordinary clicks;
                // final success is still mandatory on an actual unoccluded state.
                for(const int dy:{0,-2,2,-18,-15,-12}) {
                    if(picked_worker)break;
                    for(const int dx:{0,-2,2,8,16,24,32,40,48}) {
                        const Point point{screen.x+dx,screen.y+dy};if(!view->layout().map.contains(point.x,point.y))continue;
                        click(*view,renderer,point);
                        if(view->selected_building()==farm && view->input_diagnostic_state().selected_walker==static_cast<unsigned>(actor.id)) {
                            picked_worker=true;break;
                        }
                    }
                }
                check(saved==view->world().snapshot(),"Worker pixel picking granted gameplay");
                if(picked_worker)check(contains(view->inspection_lines(),"AUTHORED marker") && contains(view->inspection_lines(),"Grower lifetime"),
                    "Normal Grower inspection hides authored identity");
            }
        };
        for(unsigned tick=0;tick<4000 && n::farm_work(view->world(),farm)<40;++tick){view->tick_once();observe();}
        check(n::farm_work(view->world(),farm)>=40,"Actual immigration did not earn shared Farm work");
        const auto paused=fields[0];select(*view,renderer,{102,121});
        auto toggle=view->diagnostic_operation_toggle_rect();check(toggle.h>0,"Rule31 field has no normal operation toggle");
        click(*view,renderer,{double(toggle.x+toggle.w/2),double(toggle.y+toggle.h/2)});
        check(!view->world().building(paused).operating_enabled,"Normal field operation button failed to pause");
        const auto stopped=n::work(view->world(),paused).productive_ticks,all_before=n::farm_work(view->world(),farm);
        const auto retired_before=n::farm(view->world(),farm).retired;
        for(unsigned tick=0;tick<40;++tick){view->tick_once();observe();}
        check(n::farm(view->world(),farm).retired>retired_before && !n::receipt_field(view->world(),paused).returned && !n::field(view->world(),paused).harvested,
            "Paused fractional field failed its real empty return/retirement or granted a harvest receipt");
        check(n::work(view->world(),paused).productive_ticks==stopped && n::farm_work(view->world(),farm)>all_before,
            "Paused field received work or blocked all eligible peers");
        select(*view,renderer,{102,121});lines=view->inspection_lines();
        check(contains(lines,"Field paused") && contains(lines,"Work total "+std::to_string(stopped)) && contains(lines,"Bound Farmhouse #"),
            "Field inspector hides independent progress or paused reason");
        toggle=view->diagnostic_operation_toggle_rect();check(toggle.h>0,"Paused field lost normal operation toggle");
        click(*view,renderer,{double(toggle.x+toggle.w/2),double(toggle.y+toggle.h/2)});
        check(view->world().building(paused).operating_enabled,"Normal field operation button failed to resume");
        bool produced=false;
        for(unsigned tick=0;tick<12000 && !produced;++tick) {
            view->tick_once();observe();produced=std::all_of(fields.begin(),fields.end(),[&](auto id){return n::receipt_field(view->world(),id).returned>0;});
        }
        check(produced,"Paid eight-field UI Farm never physically returned every bound crop");
        check(picked_worker && std::all_of(checkpoints.begin(),checkpoints.end(),[](bool value){return value;}),"Actual Grower phase/save/picking coverage incomplete");
        std::optional<sim::CourierId> loaded;
        for(unsigned tick=0;tick<6000 && !loaded;++tick) {
            for(const auto& courier:view->world().couriers())
                if(courier.owner==farm && view->world().original_food_cargo(courier.id)>0) {loaded=courier.id;break;}
            if(!loaded)view->tick_once();
        }
        check(loaded.has_value(),"Paid physical-growers Farm never exposed a real typed Food cart");
        const auto typed_cargo=view->world().original_food_cargo(*loaded);
        check(typed_cargo>0 && view->world().courier(*loaded).cargo==0,"Food cart lost sole typed cargo authority");
        select(*view,renderer,farm_cell);lines=view->inspection_lines();
        check(contains(lines,"cargo "+std::to_string(typed_cargo)),"Farm inspector displays an empty legacy cargo mirror");
        if(!contains(lines,"Wheat") || !contains(lines,"Millet") || !contains(lines,"Rice") || !contains(lines,"Cabbage") || !contains(lines,"Bean Curd (Soy)") ||
            !contains(lines,"Actually harvested") || !contains(lines,"Last worked field") || (!contains(lines,"Next dispatch") && !contains(lines,"Next reachable field"))) {
            std::string detail;for(const auto& line:lines)detail+="["+line+"]";
            throw std::runtime_error("Farm inspector lost typed crop work or fair cursor: "+detail);
        }
        check(view->world().building(farm).progress==0 && view->world().hunting_food_state() && !view->world().fishing_food_state() && !view->world().original_agriculture_state(),
            "Rule31 UI duplicated historical Farm clocks or Food authority");
        const auto output=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/agri-mac06/presentation"/temp.root.filename();fs::create_directories(output);
        capture(*view,renderer,output/"physical-growers-farm-inspector.png");
        const auto snapshot=view->world().snapshot();const auto prepared=view->agriculture_inspection_prepare_count();
        oe::performance::set_enabled(true);oe::performance::reset();
        for(unsigned frame=0;frame<10;++frame){check(view->render(),"Prepared Physical Growers frame");(void)view->inspection_lines();}
        for(std::size_t c=0;c<static_cast<std::size_t>(oe::performance::Counter::Count);++c)
            check(!oe::performance::counter(static_cast<oe::performance::Counter>(c)),"Physical Growers frame performed authority, source or route work");
        oe::performance::set_enabled(false);
        check(snapshot==view->world().snapshot() && prepared==view->agriculture_inspection_prepare_count(),"Render rebuilt Farm work or inspection preparation");
        key(*view,SDLK_F5);check(oe::persistence::read_save(view->save_path()).source_schema_version==47,"F5 did not write exact schema47");
        key(*view,SDLK_F9);check(snapshot==view->world().snapshot(),"F9 changed field work/cursor or physical Food state");
        // Drain the actual in-flight Food cart and Grower before stopping its
        // destination. A legitimate four-portion cart return may otherwise jump
        // stock directly to8 and skip the distinct seven-plus-one state.
        check(view->execute(sim::set_building_operation(farm,false)).accepted,"Pause Farm before real channel drain");
        bool drained=false;for(unsigned tick=0;tick<5000 && !drained;++tick) {
            view->tick_once();drained=std::all_of(view->world().couriers().begin(),view->world().couriers().end(),[&](const auto& courier) {
                return courier.owner!=farm || courier.phase==sim::CourierPhase::IdleAtWorkshop;
            });
        }
        check(drained && !n::active(view->world(),farm) && !sim::physical_food_grower_reservations(&*view->world().physical_food_growers_state(),farm),
            "Paused Farm did not physically drain its cart and Grower channels");
        check(view->execute(sim::set_building_operation(market,false)).accepted,"Pause Market to expose finite Farm headroom");
        check(view->execute(sim::set_building_operation(farm,true)).accepted,"Resume Farm after actual channel drain");
        bool full=false,reserved_seven=false;for(unsigned tick=0;tick<5000 && !full;++tick) {
            view->tick_once();for(const auto& stock:view->world().hunting_food_state()->stocks)if(stock.building_id==farm) {
                const auto amount=std::accumulate(stock.amounts.begin(),stock.amounts.end(),0);full=amount==8;
                if(amount==7 && n::active(view->world(),farm) && !reserved_seven) {
                    select(*view,renderer,farm_cell);const auto state=view->inspection_lines();
                    check(!contains(state,"Stock/reservations full") && contains(state,"Farm return reserved 1"),
                        "Own7+1 Grower reservation falsely blocks the already reserved field work");reserved_seven=true;
                }
            }
        }
        check(reserved_seven,"UI did not exercise actual seven stock plus one live Grower reservation");
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
        capture(*view,renderer,output/"physical-growers-road-blocked.png");
        n::valid(view->world());view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Physical Growers E menu generation, explicit Farm binding, eight-field progress/blockers, pure SDL frames and F5/F9 PASS\n";return 0;
    } catch(const std::exception& error) {SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
