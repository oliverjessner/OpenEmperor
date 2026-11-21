// Independent paid SDL controls and processing receipts; native/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "FoodProcessingFixture.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace n=oe_food_processing_fixture;namespace fs=std::filesystem;
using f::check;using Point=oe::scene::Point;
void key(oe::menu::MenuSession& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;
    menu.handle_event(e);menu.advance();
}
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Input closed Food processing");
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
    throw std::runtime_error("Paused Food processing camera did not converge");
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
    throw std::runtime_error("Visible paid body could not be selected independently of physical carriers: wanted "+std::to_string(static_cast<unsigned>(*wanted))+" cell "+std::to_string(cell.x)+","+std::to_string(cell.y)+" selected "+std::to_string(static_cast<unsigned>(view.selected_building().value_or(sim::BuildingId{}))));
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
    check(view.render(),"Food processing render");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Private capture output");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore render target");
}


void menu_path(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    check(menu.state()==oe::menu::MenuSession::State::MainMenu&&!menu.food_processing_setup(),"General menu default changed");
    for(const char* label:{"Q Food processing (34)","X Irrigation yields (33)","Z Seasons (32)","E Physical Growers (31)",
            "G Agriculture Irrigation (rule 26)","P Playable Agriculture (rule 27)","F Playable Fishing (rule 28)","Y Playable Hunting (rule 29)"}) {
        const auto rect=menu.diagnostic_button_rect(label);
        check(rect&&rect->x>=0&&rect->x+rect->w<=600&&rect->y>=0&&rect->y+rect->h<=358,"Explicit regular selector escapes minimum menu aperture");
    }
    check(menu.render(),"Render regular food processing selector");
    const auto rect=*menu.diagnostic_button_rect("Q Food processing (34)");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect.x+rect.w/2)*scale,(rect.y+rect.h/2)*scale,&x,&y),"Q pointer coordinates");
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;menu.handle_event(e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;menu.handle_event(e);menu.advance();
    check(menu.food_processing_setup()&&menu.original_agriculture_setup(),"Q pointer selected a historical generation");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_X);check(menu.irrigation_productivity_setup()&&!menu.food_processing_setup(),"Historical X changed");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_Q);check(menu.food_processing_setup(),"Q keyboard and button disagree");menu.shutdown();
}
void pure_frames(oe::SandboxView& view) {
    const auto before=view.world().snapshot();const auto prepared=view.agriculture_inspection_prepare_count();
    oe::performance::set_enabled(true);oe::performance::reset();
    for(unsigned i=0;i<5;++i){view.update(0);check(view.render(),"Submit pure processing frame");(void)view.inspection_lines();}
    for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Processing frames performed World/resource/navigation work");
    oe::performance::set_enabled(false);
    check(view.world().snapshot()==before&&view.agriculture_inspection_prepare_count()==prepared,"Processing frames mutated World or rebuilt cached inspection");
}
void show_house_row(oe::SandboxView& view,SDL_Renderer* renderer,std::string_view wanted) {
    const auto lines=view.original_house_section_lines();
    const auto found=std::find_if(lines.begin(),lines.end(),[&](const auto& line){return line.find(wanted)!=std::string::npos;});
    check(found!=lines.end(),"House Goods inspector omitted its actual meal witness");
    const auto index=static_cast<std::size_t>(found-lines.begin()),shown=std::min<std::size_t>(3,lines.size()-index);
    for(unsigned step=0;step<512;++step) {
        const auto content=view.original_house_content_rect();const auto row=content.y+static_cast<int>(index)*17*view.layout().scale-view.original_house_scroll();
        if(row>=content.y&&row+static_cast<int>(shown)*17*view.layout().scale<=content.y+content.h)break;
        float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,float(content.x+content.w/2),float(content.y+content.h/2),&x,&y),"House scroll coordinates");
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=row<content.y?1:-1;wheel.wheel.mouse_x=x;wheel.wheel.mouse_y=y;event(view,wheel);
    }
    const auto content=view.original_house_content_rect();const auto row=content.y+static_cast<int>(index)*17*view.layout().scale-view.original_house_scroll();
    check(row>=content.y&&row+static_cast<int>(shown)*17*view.layout().scale<=content.y+content.h,"Complete House meal witness is not reachable by normal scrolling");
    check(view.render(),"Submit visible actual House meal witness");
}
void processing_inspector(oe::SandboxView& view,sim::BuildingId id) {
    const auto actual=view.world().food_processing_status(id);const auto lines=view.inspection_lines();
    check(view.selected_building()==id&&contains(lines,"AUTHORED recipes")&&contains(lines,"Workers ")&&contains(lines,"Status:"),"Normal processing inspector lacks paid authority");
    for(std::size_t grade=0;grade<5;++grade)
        check(contains(lines,"Dinners28 grade "+std::to_string(grade+1)+" stock "+std::to_string(actual.meals[grade])),"Normal meal grade differs from its sole inventory");
    check(contains(lines,"Grade5 unavailable"),"Unavailable Salt/Spices recipe is not labelled");
    if(actual.parent_market)check(contains(lines,"Bound paid Market #"+std::to_string(static_cast<unsigned>(*actual.parent_market))),"Shop inspector lost exact parent Market");
    else check(contains(lines,"Requested recipe grade "+std::to_string(actual.requested_grade)),"Mill inspector differs from actual requested recipe");
}
} // namespace
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        n::Fixture source;f::Temp art;art.root=fs::canonical(art.root);
        fs::copy(art.root/"data/DATA",source.root.data/"DATA",fs::copy_options::recursive);
        f::write(source.root.data/oe_influence_fixture::map_relative,
            oe_fishing_fixture::encoded_map(source.raw,source.fertility,true));
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");check(SDL_Init(SDL_INIT_VIDEO),"Initialize processing SDL");
        check(SDL_CreateWindowAndRenderer("Food processing scripted SDL",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create processing renderer");
        menu_path(source.root.data,art.root/"menu",window,renderer);
        check(SDL_SetWindowSize(window,1440,1000),"Resize processing inspector");
        auto session=oe::maps::load_stored_map_session(source.root.data,oe_influence_fixture::map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_food_processing(std::move(session),std::nullopt,false);
        const auto save_path=art.root/"processing50.save.json";view->configure_save(source.root.data,oe_influence_fixture::map_relative,save_path);view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        for(unsigned i=0;i<3&&view->camera().zoom!=1.;++i)key(*view,SDLK_Z);
        check(view->world().rule_version()==34&&view->world().processed_food_state()&&!view->world().hunting_food_state()&&view->world().ticks()==0&&!view->world().total_population(),"New processing UI gained historical inventory/time/population");
        const auto buy=[&](sim::CommandType type,sim::Cell cell,sim::BuildingId parent=sim::BuildingId{}) {
            sim::Command command{type,cell};command.building_id=parent;const auto result=view->execute(command);
            if(!result.accepted||!result.changed)throw std::runtime_error("Paid processing UI construction: "+result.reason);
            return view->world().building_owner_at(cell).value_or(sim::BuildingId{});
        };
        for(int x=89;x<=132;++x)buy(sim::CommandType::PlaceRoad,{x,109});
        for(int x=90;x<126;x+=3)buy(sim::CommandType::PlaceHousehold,{x,107});
        const auto farm=buy(sim::CommandType::PlaceOriginalFoodFarm,{90,110});
        for(std::size_t crop=0;crop<5;++crop)buy(oe_playable_agriculture_fixture::crop_commands[crop],{93,110+static_cast<int>(crop)},farm);
        const auto market=buy(sim::CommandType::PlaceMarket,{97,110});
        buy(sim::CommandType::PlaceOriginalWaterWell,{101,110});buy(sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
        for(int x:{109,112})buy(sim::CommandType::PlaceFireWatch,{x,110});
        const auto construct=[&](SDL_Keycode shortcut,int tool,sim::Cell cell,sim::Object kind,sim::CommandType command) {
            key(*view,shortcut,SDL_KMOD_SHIFT);check(view->tool()==tool&&view->preview(cell).accepted,"Processing shortcut omitted validated placement");
            const auto before=view->world().snapshot();pure_frames(*view);
            const auto ground=oe::maps::terrain_ground({static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},1);center(*view,ground);
            check(view->render(),"Submit validated processing preview");click(*view,renderer,view->camera().world_to_screen(ground));
            const auto id=view->world().building_owner_at(cell);check(id&&view->world().building(*id).kind==kind,"Normal pointer did not construct the requested paid source");
            check(view->world().treasury()==before.treasury-view->world().construction_cost(command),"Processing UI construction lost source-model debit");return *id;
        };
        const auto mill=construct(SDLK_M,51,{119,110},sim::Object::OriginalMill53,sim::CommandType::PlaceOriginalMill);
        const auto shop=construct(SDLK_K,52,{98,110},sim::Object::OriginalFoodShop66,sim::CommandType::PlaceOriginalFoodShop);
        check(view->world().building_cells(mill).size()==25&&view->world().building_cells(shop).size()==4&&view->world().food_processing_state()->shops.front().market_id==market,
            "Paid Mill/Shop complete geometry or stable parent binding changed");
        select(*view,renderer,view->world().building(mill).cell);processing_inspector(*view,mill);
        const auto recipe=std::find_if(view->layout().buttons.begin(),view->layout().buttons.end(),[](const auto& button){return button.action==oe::sandbox_ui::Action::CycleMillRecipe;});
        check(recipe!=view->layout().buttons.end(),"Normal toolbar omits recipe control");
        click(*view,renderer,{recipe->rect.x+recipe->rect.w*.5,recipe->rect.y+recipe->rect.h*.5});
        check(n::mill(view->world(),mill).requested_grade==3,"Recipe button did not use the shared action");
        key(*view,SDLK_G,SDL_KMOD_SHIFT);check(n::mill(view->world(),mill).requested_grade==4,"Recipe key and button disagree");
        key(*view,SDLK_G,SDL_KMOD_SHIFT);check(n::mill(view->world(),mill).requested_grade==1,"Recipe cycle admitted unavailable grade5");
        const auto output=fs::current_path()/".local/agri-mac09/presentation";fs::create_directories(output);
        processing_inspector(*view,mill);pure_frames(*view);capture(*view,renderer,output/"processing-paid-empty-mill.png");
        key(*view,SDLK_F5);const auto empty=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);check(view->world().snapshot()==empty,"Empty processing F5/F9 changed paid identity");
        bool active_seen=false,meal_cargo_seen=false;
        for(unsigned i=0;i<14000&&!n::meals_consumed(view->world());++i) {
            view->tick_once();const auto& work=n::mill(view->world(),mill);
            if(!active_seen&&work.active&&work.active->work>0) {
                active_seen=true;select(*view,renderer,view->world().building(mill).cell);processing_inspector(*view,mill);show_row(*view,renderer,"Active grade");pure_frames(*view);
                capture(*view,renderer,output/"processing-active-paid-input.png");key(*view,SDLK_F5);const auto active=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
                check(view->world().snapshot()==active,"Active processing F5/F9 changed committed inputs or work");
            }
            if (!meal_cargo_seen) {
                std::optional<sim::CourierId> loaded;
                for (const auto& actor:view->world().couriers())
                    if ((actor.role==sim::CourierRole::OriginalMealsToShop || actor.role==sim::CourierRole::OriginalMealsToHouse) &&
                        view->world().original_food_cargo(actor.id)>0) {loaded=actor.id;break;}
                if (loaded) {
                    const auto ground=view->courier_ground_position(*loaded);check(ground.has_value(),"Loaded meal carrier lost actual position");
                    view->set_tool(5);center(*view,*ground);check(view->render(),"Submit actual meal carrier marker");
                    const auto point=view->camera().world_to_screen(*ground);bool picked=false;
                    for(const int dy:{0,-18,-15,-12})for(const int dx:{0,8,16,24,32,40}) {
                        if(picked)break;click(*view,renderer,{point.x+dx,point.y+dy});
                        picked=view->input_diagnostic_state().selected_walker==static_cast<unsigned>(*loaded);
                    }
                    check(picked&&contains(view->inspection_lines(),"Actual Food cargo ")&&contains(view->inspection_lines(),"Dinners grade"),
                        "Normal meal marker does not expose its actual typed physical cargo");
                    check(!contains(view->inspection_lines(),"To warehouse"),"Meal delivery inspector mislabeled a nonexistent Warehouse Food leg");
                    meal_cargo_seen=true;pure_frames(*view);capture(*view,renderer,output/"processing-actual-meal-cargo.png");
                    key(*view,SDLK_F5);const auto cargo=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);
                    check(view->world().snapshot()==cargo,"Meal cargo F5/F9 changed physical loaded transfer");
                }
            }
        }
        check(active_seen&&meal_cargo_seen&&n::meals_consumed(view->world())>0,"Actual paid UI never completed processing, physical Shop/House delivery and consumption");
        select(*view,renderer,view->world().building(shop).cell);processing_inspector(*view,shop);pure_frames(*view);capture(*view,renderer,output/"processing-food-shop.png");
        const auto witness=std::find_if(view->world().food_processing_state()->houses.begin(),view->world().food_processing_state()->houses.end(),[](const auto& row){return row.first_consumption.has_value();});
        check(witness!=view->world().food_processing_state()->houses.end(),"Actual meal consumption lacks a House witness");
        select(*view,renderer,view->world().building(witness->house_id).cell);
        for(unsigned i=0;i<4&&view->original_house_section()!=oe::SandboxView::OriginalHouseSection::Goods;++i)key(*view,SDLK_PAGEDOWN);
        const auto goods=view->original_house_section_lines();check(contains(goods,"First physical Dinners")&&contains(goods,"First actual Dinners"),"Normal House inspector hides earned meal witnesses");
        show_house_row(*view,renderer,"First actual Dinners");pure_frames(*view);
        capture(*view,renderer,output/"processing-house-consumption.png");
        key(*view,SDLK_F5);const auto consumed=view->world().snapshot();view->tick_once();key(*view,SDLK_F9);check(view->world().snapshot()==consumed,"Consumed meal F5/F9 changed a sole Food history");
        for(const auto size:{SDL_Point{600,400},SDL_Point{900,600},SDL_Point{1280,800}}) {
            check(SDL_SetWindowSize(window,size.x,size.y),"Resize processing UI");view->update(0);check(view->render(),"Render resized processing UI");
            if(view->layout().panel_open){select(*view,renderer,view->world().building(mill).cell);processing_inspector(*view,mill);
                check(view->inspector_layout().content.h>=3*17*view->layout().scale+6*view->layout().scale,"Normal processing inspector lost three complete readable rows");
                show_row(*view,renderer,"Grade5 unavailable");}
            pure_frames(*view);capture(*view,renderer,output/("processing-"+std::to_string(size.x)+".png"));
        }
        sim::CourierState meal;meal.role=sim::CourierRole::OriginalMealsToShop;
        check(!oe::walker_live_visible(meal)&&!oe::walker_visual_role(meal.role),"Idle meal channel gained visible actor or unqualified art");meal.phase=sim::CourierPhase::Returning;
        check(oe::walker_live_visible(meal),"Actual returning meal carrier is hidden");
        const auto historical=sim::World::create_irrigation_productivity(view->world().map_permissions(),
            view->world().original_rules_model(),view->world().original_terrain_facts());
        const auto historical_path=art.root/"historical49.save.json";
        const auto old_document=oe::persistence::make_document(source.root.data,oe_influence_fixture::map_relative,
            view->world().map_permissions()->building_mask(),historical);
        oe::persistence::write_save(historical_path,old_document,source.root.data,view->world().map_permissions()->building_mask());
        const auto old_bytes=oe_influence_fixture::read_text(historical_path);const auto old_modified=fs::last_write_time(historical_path);
        const auto retained=view->world().snapshot();const auto loads=view->load_generation(),saves=view->save_generation();
        view->configure_save(source.root.data,oe_influence_fixture::map_relative,historical_path);key(*view,SDLK_F9);
        check(view->world().snapshot()==retained&&view->load_generation()==loads,"Rule34 F9 migrated a historical schema49 target");
        key(*view,SDLK_F5);
        check(view->world().snapshot()==retained&&view->save_generation()==saves&&oe_influence_fixture::read_text(historical_path)==old_bytes&&
            fs::last_write_time(historical_path)==old_modified,"Rule34 F5 changed a historical schema49 target");
        view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Food processing Q/legacy selectors, paid full Mill/Shop controls, recipe action, active input/work, actual House meal consumption, exact F5/F9 and pure frames PASS\n";return 0;
    }catch(const std::exception& error){oe::performance::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
