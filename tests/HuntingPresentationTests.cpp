// Independent paid SDL scenario; native-window/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "FishingFixture.h"
#include "app/OriginalHouseInspection.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace q=oe_irrigation_fixture;
namespace z=oe_fishing_fixture;namespace fs=std::filesystem;
using f::check;using Point=oe::scene::Point;
void key(oe::menu::MenuSession& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;
    menu.handle_event(e);menu.advance();
}
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Input closed Playable Fishing");
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
    throw std::runtime_error("Paused Playable Fishing camera did not converge");
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
std::string document(const std::vector<std::string>& lines) {
    std::string text;for(const auto& line:lines){if(!text.empty())text+=' ';text+=line;}return text;
}
std::size_t row(const std::vector<std::string>& lines,std::string_view fragment) {
    const auto found=std::find_if(lines.begin(),lines.end(),[&](const auto& line){return line.find(fragment)!=std::string::npos;});
    if(found==lines.end())throw std::runtime_error("Missing priority inspector line: "+std::string(fragment));
    return static_cast<std::size_t>(found-lines.begin());
}
void visible(oe::SandboxView& view,const std::vector<std::string>& lines,std::string_view fragment) {
    check(view.inspector_layout().detail_row_rect(row(lines,fragment)).h>0,
        "Actual crop/Food status is below the visible inspector viewport");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h),"Capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Private capture target");
    check(view.render(),"Playable Fishing render");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Private capture output");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore render target");
}

void menu_path(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    check(menu.state()==oe::menu::MenuSession::State::MainMenu && !menu.playable_hunting_setup(),"Hunting changed defaults");
    check(menu.diagnostic_button_rect("Y Playable Hunting (rule 29)").has_value(),"Hunting missing from regular menu");
    check(menu.render(),"Render Hunting menu before pointer boundary test");
    const auto rect=*menu.diagnostic_button_rect("Y Playable Hunting (rule 29)");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"Hunting menu button transform");
    const auto mouse=[&](Uint32 type,float px,float py) {
        SDL_Event e{};e.type=type;e.button.windowID=SDL_GetWindowID(window);e.button.button=SDL_BUTTON_LEFT;e.button.x=px;e.button.y=py;
        check(SDL_PushEvent(&e),"Queue Hunting menu pointer");while(SDL_PollEvent(&e))menu.handle_event(e);
    };
    // Native automation delivered a correct Down and a stale edge-position Up.
    // Test the product boundary using queued SDL events; this is no native claim.
    int width=0,height=0;check(SDL_GetWindowSize(window,&width,&height),"Hunting menu dimensions");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_UP,float(width)-.5F,float(height)*.75F);menu.advance();
    check(menu.state()==oe::menu::MenuSession::State::MainMenu && !menu.original_setup() &&
        menu.diagnostic_pressed_action()==-1 && menu.diagnostic_pending_action()==-1,"Mismatched Down/Up selected Hunting or retained a press");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_UP,x,y);menu.advance();
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.playable_hunting_setup(),"Correct click did not recover after mismatched release");
    key(menu,SDLK_ESCAPE);
    key(menu,SDLK_Y);
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.playable_hunting_setup() && menu.original_agriculture_setup(),"Y selection is not Hunting setup");
    key(menu,SDLK_P);check(!menu.original_paid_starter(),"Actual Hunting setup did not select Empty city");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking && std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==29,"Hunting preflight did not bind29");
    key(menu,SDLK_RETURN);
    check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Hunting Loading frame");
    menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox()) throw std::runtime_error("Hunting menu Start: "+menu.message());
    check(menu.sandbox()->world().playable_hunting_enabled() && menu.sandbox()->world().rule_version()==29 && menu.sandbox()->paused(),"Wrong Hunting World");
    menu.shutdown();
}

}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        f::Temp temp;temp.root=fs::canonical(temp.root);
        z::Fixture source;
        auto model=z::f::read_text(source.root.data/"Model/EmperorBuildingModels.txt");
        const auto marker=model.find("HOUSE MODS\n");check(marker!=std::string::npos,"Hunting test model marker");
        model.insert(marker,"33,Hunter's Tent,{,32,0,0,0,0,15,0,0,0,0,0,0,0,},,,\n");
        fs::create_directories(temp.root/"data/Model");q::f::write_text(temp.root/"data/Model/EmperorBuildingModels.txt",model);
        std::vector<std::uint32_t> raw(sim::OriginalTerrainFacts::cell_count,0x80U);
        std::vector<std::uint8_t> fertility(raw.size(),100);
        for(int y=94;y<=105;++y)for(int x=100;x<=132;++x)raw[std::size_t(y)*228U+std::size_t(x)]=4U;
        f::write(temp.root/"data/Cities/A.map",z::encoded_map(raw,fertility,true));
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted Hunting SDL");
        check(SDL_CreateWindowAndRenderer("Hunting scripted acceptance",1280,900,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create Hunting renderer");
        menu_path(temp.root/"data",temp.root/"menu",window,renderer);
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,
            oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_playable_hunting(std::move(session));
        view->configure_save(temp.root/"data","Cities/A.map",temp.root/"hunting.save.json");
        view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view->execute({type,cell});
            if (!result.accepted || !result.changed) throw std::runtime_error("Paid UI construction "+
                std::to_string(static_cast<unsigned>(type))+" at "+std::to_string(cell.x)+","+
                std::to_string(cell.y)+": "+result.reason);
            return view->world().building_owner_at(cell);
        };
        for(int x=89;x<=153;++x) for(const int y:{114,117}) buy(sim::CommandType::PlaceRoad,{x,y});
        for(int y=115;y<=116;++y)for(const int x:{89,153})buy(sim::CommandType::PlaceRoad,{x,y});
        for(int x=90;x<150;x+=3)buy(sim::CommandType::PlaceHousehold,{x,112});
        for(int x=114;x<=119;++x)buy(sim::CommandType::PlaceRoad,{x,107});
        for(int y=108;y<=113;++y)buy(sim::CommandType::PlaceRoad,{119,y});
        const auto market=*buy(sim::CommandType::PlaceMarket,{134,118});
        buy(sim::CommandType::PlaceOriginalWaterWell,{138,118});buy(sim::CommandType::PlaceOriginalAncestorShrine,{142,118});
        buy(sim::CommandType::PlaceFireWatch,{110,118});buy(sim::CommandType::PlaceFireWatch,{146,118});
        // This paid UI city supplies Millet, Fish and Meat. The independent
        // World suite proves all five crops/seven Foods together; five fast
        // Farms here would continuously reserve the Market's finite16 slots.
        const auto farm=*buy(sim::CommandType::PlaceOriginalFoodFarm,{98,118});
        sim::Command field_command{sim::CommandType::PlaceOriginalMilletField,{101,118}};field_command.building_id=farm;
        check(view->execute(field_command).accepted,"Paid inherited Millet field");
        const auto field_id=*view->world().building_owner_at(field_command.cell);
        key(*view,SDLK_F,SDL_KMOD_SHIFT);check(view->tool()==49,"Inherited Quay keyboard action");
        const sim::Cell quay_cell{114,105};center(*view,oe::maps::terrain_ground({114,105},1));
        click(*view,renderer,view->camera().world_to_screen(oe::maps::terrain_ground({114,105},1)));
        const auto quay=view->world().building_owner_at(quay_cell);check(quay.has_value(),"Paid inherited Quay");
        for(int y=118;y<=120;++y)buy(sim::CommandType::PlaceRoad,{129,y});
        key(*view,SDLK_H,SDL_KMOD_SHIFT);check(view->tool()==50 && !view->help_open(),"HuntersTent action collides with Help");
        const sim::Cell tent_cell{128,121};center(*view,oe::maps::terrain_ground({128,121},1));
        const auto funds=view->world().treasury();const auto untouched=view->world().snapshot();
        SDL_Event zero{};zero.type=SDL_EVENT_MOUSE_BUTTON_DOWN;zero.button.button=SDL_BUTTON_LEFT;event(*view,zero);
        zero.type=SDL_EVENT_MOUSE_BUTTON_UP;event(*view,zero);view->update(0);
        check(untouched==view->world().snapshot(),"Raw zero button coordinates were replaced with stale map position");
        // A stale zero motion cannot replace a later button's actual position.
        SDL_Event stale{};stale.type=SDL_EVENT_MOUSE_MOTION;stale.motion.x=stale.motion.y=0;event(*view,stale);
        const auto screen=view->camera().world_to_screen(oe::maps::terrain_ground({128,121},1));float wx=0,wy=0;
        check(SDL_RenderCoordinatesToWindow(renderer,float(screen.x),float(screen.y),&wx,&wy),"Tent button transform");
        SDL_Event button{};button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;button.button.button=SDL_BUTTON_LEFT;button.button.x=wx;button.button.y=wy;event(*view,button);
        SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;event(*view,lost);
        button.type=SDL_EVENT_MOUSE_BUTTON_UP;event(*view,button);view->update(0);
        check(untouched==view->world().snapshot(),"Focus-cancelled Tent press reached paid construction");
        SDL_Event regained{};regained.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,regained);
        button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event(*view,button);
        button.type=SDL_EVENT_MOUSE_BUTTON_UP;event(*view,button);view->update(0);
        const auto tent=view->world().building_owner_at(tent_cell);
        check(tent && view->world().building(*tent).kind==sim::Object::OriginalHuntersTent && view->world().treasury()<funds,"Actual Tent click did not buy paid source");
        check(view->world().find_building_route(*tent,market).has_value(),"Paid Tent has no physical Market road route");
        {
            const auto front=sim::building_front_cell(view->world().profile(),view->world().rule_version(),sim::Object::OriginalHuntersTent,tent_cell);
            const auto anchor=oe::maps::terrain_ground({static_cast<unsigned>(front.x),static_cast<unsigned>(front.y)},1);
            view->set_tool(5);center(*view,anchor);check(view->render(),"Authored canvas Tent frame");
            click(*view,renderer,view->camera().world_to_screen({anchor.x-20,anchor.y-45}));
            check(view->selected_building()==tent,"Visible Tent canvas does not select its paid source");
        }
        check(view->world().hunting_food_state() && !view->world().fishing_food_state() && !view->world().original_agriculture_state(),"UI duplicated versioned Food authority");
        check(!view->world().hunting_state()->animals.empty(),"Actual authored habitat has no living animals");
        const auto output=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/agri-mac03/presentation"/temp.root.filename();fs::create_directories(output);
        {
            const auto id=view->world().hunting_state()->animals.front().id;const auto p=view->world().hunting_animal_position(id);
            const Point ground{(p.x-p.y)*40,(p.x+p.y-2)*20+20};view->set_tool(5);center(*view,ground);check(view->render(),"Actual animal pixels");
            const auto before=view->world().snapshot();click(*view,renderer,view->camera().world_to_screen(ground));
            capture(*view,renderer,output/"wildlife-inspector.png");
            const auto details=document(view->inspection_lines());
            if(view->selected_building() || details.find("Actual live animal")==std::string::npos ||
                details.find("Animal art: AUTHORED fallback")==std::string::npos)
                throw std::runtime_error("Actual animal pixels do not expose wildlife authority: "+details);
            check(before==view->world().snapshot(),"Wildlife inspection changed gameplay");
        }
        bool walked=false,loaded=false,consumed=false,staffed=false,burned=false;
        for(unsigned tick=0;tick<24000 && !consumed;++tick) {
            view->tick_once();
            staffed|=view->world().workers_assigned(*tent)==view->world().workforce_required(*tent);
            burned|=view->world().building_on_fire(*tent);
            for(const auto& hunter:view->world().hunting_state()->hunters) {
                if (!walked && hunter.edge_progress) {
                    walked=true;const auto p=view->world().hunting_hunter_position(hunter.id);
                    const Point ground{(p.x-p.y)*40,(p.x+p.y-2)*20+20};view->set_tool(5);center(*view,ground);check(view->render(),"Actual Hunter frame");
                    click(*view,renderer,view->camera().world_to_screen(ground));
                    check(view->selected_building()==tent && contains(view->inspection_lines(),"Hunter #"),"Actual Hunter pixels do not select paid Tent");
                    capture(*view,renderer,output/"moving-hunter.png");
                    const auto saved=view->world().snapshot();oe::performance::set_enabled(true);oe::performance::reset();
                    for(unsigned frame=0;frame<10;++frame)check(view->render(),"Pure hunting frame");
                    for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
                        check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Hunting frame performed I/O, authority or route work");
                    oe::performance::set_enabled(false);check(saved==view->world().snapshot(),"Hunting frame mutated World");
                    view->save_now();check(oe::persistence::read_save(view->save_path()).source_schema_version==45,"F5 omitted schema45");
                    view->load_now();check(saved==view->world().snapshot(),"F9 changed actual Hunter movement");
                    break;
                }
                if (!loaded && hunter.cargo>0 && hunter.edge_progress) {
                    loaded=true;const auto saved=view->world().snapshot();view->save_now();view->load_now();
                    check(saved==view->world().snapshot(),"F5/F9 changed actual loaded Hunter return");break;
                }
            }
            for(const auto& stock:view->world().hunting_food_state()->stocks)consumed|=stock.consumed[sim::hunting_food_index]>0;
        }
        if(!walked || !loaded || !consumed) {
            select(*view,renderer,tent_cell);capture(*view,renderer,output/"hunt-failure.png");
            view->save_now();fs::copy_file(view->save_path(),output/"hunt-failure.save.json");
            const auto& hunting=*view->world().hunting_state();
            std::string info="Paid Hunter E2E failed: walked="+std::to_string(walked)+" loaded="+std::to_string(loaded)+
                " consumed="+std::to_string(consumed)+" ever_staffed="+std::to_string(staffed)+" ever_burned="+std::to_string(burned)+
                " tick="+std::to_string(view->world().ticks())+" pop="+std::to_string(view->world().total_population())+
                " launched="+std::to_string(hunting.launched)+" retired="+std::to_string(hunting.retired)+
                " entrances="+std::to_string(view->world().building_entrances(*tent).size())+" | "+document(view->inspection_lines());
            throw std::runtime_error(info);
        }
        std::uint64_t unloaded=0;for(const auto& source:view->world().hunting_state()->tents)if(source.tent_id==*tent)unloaded=source.unloaded;
        check(unloaded>0,"Tent never received physically returned Meat");
        check(z::source(view->world(),*quay).unloaded>0,"Hunting lost inherited physical Fishing");
        const auto field=std::find_if(view->world().agriculture_fields().begin(),view->world().agriculture_fields().end(),[&](const auto& row){return row.field_id==field_id;});
        check(field!=view->world().agriculture_fields().end() && field->harvested>0,"Hunting lost inherited Millet");
        select(*view,renderer,tent_cell);auto lines=view->inspection_lines();
        check(contains(lines,"Hunters Tent33") && contains(lines,"Meat stock") && contains(lines,"Workers ") && contains(lines,"Active hunters") && contains(lines,"AUTHORED fallback"),"Tent inspector omits actual state or visual boundary");
        visible(*view,lines,"Meat stock");capture(*view,renderer,output/"tent-inspector.png");
        select(*view,renderer,view->world().building(market).cell);lines=view->inspection_lines();
        capture(*view,renderer,output/"market-seven-foods.png");
        if(view->selected_building()!=market)throw std::runtime_error("Market click selected another submitted visual: "+document(lines));
        check(contains(lines,"Meat ") && row(lines,"Meat ")<row(lines,"Ceramics stock"),"Market lost seventh Food priority");
        visible(*view,lines,"Meat ");
        bool home=false;for(const auto& stock:view->world().hunting_food_state()->stocks)if(stock.consumed[sim::hunting_food_index]) {
            const auto result=oe::original_house_ui::inspect_house(view->world(),stock.building_id,oe::original_house_ui::Section::Goods);
            check(result.hunting_food && !result.fishing_food && !result.agriculture_food && result.hunting_food->consumed[6]==stock.consumed[6],"House lost sole seven-Food observation");
            check(contains(result.sections[std::size_t(oe::original_house_ui::Section::Goods)],"Meat House stock"),"House hides actual Meat stock/consumption");home=true;break;
        }
        check(home,"No actual Meat-consuming House observed");
        view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Hunting menu, paid SDL-button Tent, actual prey/hunter/Meat consumption, seven-Food inspectors and F5/F9 PASS\n";return 0;
    } catch(const std::exception& error) {SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
