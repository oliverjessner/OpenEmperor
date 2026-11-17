// Paid scripted SDL acceptance. This is independent authored map/model data;
// native-window and human acceptance are separate validation steps.
#include "EconomyStartViewFixture.h"
#include "IrrigationFixture.h"
#include "app/OriginalHouseInspection.h"
#include "core/PerformanceDiagnostics.h"

#include <cmath>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace f=oe::testing::economy;
namespace q=oe_irrigation_fixture;
namespace fs=std::filesystem;
using f::check;
using Point=oe::scene::Point;

void key(oe::menu::MenuSession& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;
    menu.handle_event(e);menu.advance();
}
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Input closed Playable Agriculture");
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
    throw std::runtime_error("Paused Playable Agriculture camera did not converge");
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point render) {
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,float(render.x),float(render.y),&x,&y),"Pointer transform");
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=x;e.motion.y=y;event(view,e);view.update(0);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);view.update(0);
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::Cell cell) {
    const auto ground=oe::maps::terrain_ground({std::uint32_t(cell.x),std::uint32_t(cell.y)},72);
    view.set_tool(5);center(view,ground);click(view,renderer,view.camera().world_to_screen(ground));
}
bool contains(const std::vector<std::string>& lines,std::string_view fragment) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(fragment)!=std::string::npos;});
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
    check(view.render(),"Playable Agriculture render");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Private capture output");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore render target");
}
void menu_path(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    check(menu.state()==oe::menu::MenuSession::State::MainMenu && !menu.playable_agriculture_setup(),
        "Playable Agriculture changed general menu defaults");
    check(menu.diagnostic_button_rect("P Playable Agriculture (rule 27)").has_value(),
        "Playable Agriculture is not available in the regular main menu");
    key(menu,SDLK_P);
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.playable_agriculture_setup() &&
        menu.original_agriculture_setup(),"P did not select explicit Playable Agriculture setup");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking &&
        std::chrono::steady_clock::now()<deadline) {menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==27,
        "Playable Agriculture map preflight did not bind Rule27");
    key(menu,SDLK_RETURN);
    check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Playable Agriculture Loading screen");
    menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox())
        throw std::runtime_error("Regular Playable Agriculture Start failed: "+menu.message());
    check(menu.sandbox()->world().playable_agriculture_enabled() && menu.sandbox()->world().rule_version()==27 &&
        menu.sandbox()->world().irrigation_enabled() && menu.sandbox()->paused(),"Wrong menu World generation");
    check(menu.sandbox()->last_message().find("AUTHORED")!=std::string::npos &&
        menu.sandbox()->last_message().find("locked")==std::string::npos,"Active new mode reports a locked production path");
    menu.shutdown();
}
} // namespace

int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        f::Temp temp;
        // Darwin's temporary root can be reached through /var -> /private/var.
        // Preserve the production save writer's strict no-symlink boundary.
        temp.root=fs::canonical(temp.root);
        f::write(temp.root/"data/Cities/A.map",f::map_file(true,true,true));
        fs::create_directories(temp.root/"data/Model");
        q::f::write_text(temp.root/"data/Model/EmperorBuildingModels.txt",q::authored_model());
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize isolated SDL");
        check(SDL_CreateWindowAndRenderer("Playable Agriculture scripted acceptance",1280,800,SDL_WINDOW_RESIZABLE,
            &window,&renderer),"Create isolated SDL renderer");
        menu_path(temp.root/"data",temp.root/"menu",window,renderer);
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,
            oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto owner=oe::SandboxView::create_playable_agriculture(std::move(session),std::nullopt,false);
        auto& view=*owner;
        view.configure_save(temp.root/"data","Cities/A.map",temp.root/"playable.save.json");
        view.initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,focus);
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view.execute({type,cell});
            if(!result.accepted || !result.changed)throw std::runtime_error("Paid Playable Agriculture UI scene command "+
                std::to_string(static_cast<unsigned>(type))+" at "+std::to_string(cell.x)+","+
                std::to_string(cell.y)+": "+result.reason);
            return view.world().building_owner_at(cell);
        };
        // The authored84-side map has a narrow diamond at y99. Use the
        // existing central y110 corridor; every complete footprint remains in it.
        for(int x=90;x<=151;++x)if(!view.world().road_cell({x,110}))buy(sim::CommandType::PlaceRoad,{x,110});
        for(int x=90;x<150;x+=3)buy(sim::CommandType::PlaceHousehold,{x,108});
        constexpr std::array commands{sim::CommandType::PlaceOriginalWheatField,sim::CommandType::PlaceOriginalMilletField,
            sim::CommandType::PlaceOriginalRiceField,sim::CommandType::PlaceOriginalCabbageField,sim::CommandType::PlaceOriginalSoybeanField};
        constexpr std::array names{"Wheat","Millet","Rice","Cabbage","Bean Curd (Soy)"};
        std::array<sim::Cell,5> cells{};
        for(std::size_t crop=0;crop<commands.size();++crop) {
            const int x=90+8*static_cast<int>(crop);
            (void)buy(sim::CommandType::PlaceOriginalFoodFarm,{x,111});
            if(crop==0) {
                // A free paid Farm makes every crop action eligible. Once all
                // five Farms own productive fields the toolbar correctly disables them.
                key(view,SDLK_R,SDL_KMOD_SHIFT);check(view.tool()==43,"Rice keyboard tool");
                key(view,SDLK_C,SDL_KMOD_SHIFT);check(view.tool()==44,"Cabbage keyboard tool");
                key(view,SDLK_S,SDL_KMOD_SHIFT);check(view.tool()==45,"Soy keyboard tool");
            }
            cells[crop]={x+3,111};(void)buy(commands[crop],cells[crop]);
        }
        const auto market=buy(sim::CommandType::PlaceMarket,{132,111});
        (void)buy(sim::CommandType::PlaceOriginalWaterWell,{136,111});
        (void)buy(sim::CommandType::PlaceOriginalAncestorShrine,{140,111});
        (void)buy(sim::CommandType::PlaceFireWatch,{144,111});
        (void)buy(sim::CommandType::PlaceFireWatch,{147,111});
        check(view.world().original_agriculture_state()->farms.size()==5 && view.world().original_agriculture_state()->fields.size()==5,
            "Regular new mode cannot place all five paid Food chains");
        bool produced=false;
        for(unsigned tick=0;tick<6000 && !produced;++tick) {
            view.tick_once();
            const auto& fields=view.world().original_agriculture_state()->fields;
            produced=std::all_of(fields.begin(),fields.end(),[](const auto& field){return field.harvested>0;});
        }
        check(produced,"Paid UI scene did not produce all five Food crops");
        for(std::size_t crop=0;crop<cells.size();++crop) {
            select(view,renderer,cells[crop]);
            const auto lines=view.inspection_lines();
            check(contains(lines,std::string("Output: ")+names[crop]) && contains(lines,"Harvest progress") &&
                contains(lines,"AUTHORED") && contains(lines,"Actually harvested"),"Crop inspector hides actual authored production");
            check(!contains(lines,"Harvest locked"),"Playable crop inspector reports historical harvest lock");
            check(row(lines,"Output:")<row(lines,"Harvest progress") && row(lines,"Harvest progress")<row(lines,"Farm workers") &&
                !contains(lines,"Reference numeric slots") && !contains(lines,"Outer slot"),
                "Reference/debug calendar displaced actual playable crop information");
            visible(view,lines,"Output:");visible(view,lines,"Harvest progress");visible(view,lines,"Farm workers");
            visible(view,lines,"Farm crop stock");visible(view,lines,"Farm crop dispatched");
        }
        const auto output=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/agri-mac01/presentation"/temp.root.filename();
        fs::create_directories(output);
        capture(view,renderer,output/"playable-beancurd-inspector.png");
        select(view,renderer,view.world().building(*market).cell);
        const auto market_lines=view.inspection_lines();
        check(contains(market_lines,"Bean Curd (Soy)") && contains(market_lines,"Physical Food deliveries") &&
            contains(market_lines,"Arrived"),"Market inspector hides physical typed Food delivery history");
        check(row(market_lines,"Food total")<row(market_lines,"Ceramics stock"),
            "Legacy Goods hide new-mode Food stocks at the top of the Market inspector");
        for(const auto name:names)visible(view,market_lines,name);
        capture(view,renderer,output/"playable-market-inspector.png");
        const auto before=view.world().snapshot();
        oe::performance::set_enabled(true);oe::performance::reset();
        for(unsigned frame=0;frame<10;++frame)check(view.render(),"Prepared Playable Agriculture frame");
        for(std::size_t c=0;c<static_cast<std::size_t>(oe::performance::Counter::Count);++c)
            check(!oe::performance::counter(static_cast<oe::performance::Counter>(c)),"Playable Agriculture render performed authority/resource work");
        oe::performance::set_enabled(false);
        check(before==view.world().snapshot(),"Playable Agriculture rendering mutated World");
        view.save_now();
        check(oe::persistence::read_save(view.save_path()).source_schema_version==43,"Regular F5 did not write schema43");
        view.tick_once();view.load_now();
        check(view.world().snapshot()==before,"Regular F9 did not restore exact playable state");
        owner.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Playable Agriculture regular menu, five paid crops, authored inspectors, pure SDL rendering and exact F5/F9 PASS\n";
        return 0;
    } catch(const std::exception& error) {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
