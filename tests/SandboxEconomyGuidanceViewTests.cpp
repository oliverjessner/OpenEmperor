#include "EconomyStartViewFixture.h"
#include "core/PerformanceDiagnostics.h"
#include <limits>
#include <memory>
#include <thread>
namespace {
namespace f=openemperor::testing::economy;
namespace oe=openemperor;
namespace sim=oe::simulation;
using Menu=oe::menu::MenuSession;
using f::check;
class NoDialog final:public oe::menu::DialogAdapter {
public:
    void open_folder(SDL_Window*,Callback) override { throw std::runtime_error("Unexpected folder dialog."); }
    void open_file(SDL_Window*,Callback) override { throw std::runtime_error("Unexpected file dialog."); }
    void cancel_pending() override {}
};
SDL_Event key(SDL_Keycode value) { SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=value;return event; }
void send(Menu& menu,SDL_Keycode value) { menu.handle_event(key(value));menu.advance(); }
void load_ready(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"Normal menu did not enter Loading.");
    check(menu.render(),"Normal loading render failed.");menu.advance();
    check(menu.state()==Menu::State::Playing&&menu.sandbox(),"Normal menu did not publish its city.");
}
void start(Menu& menu) {
    send(menu,SDLK_N);check(menu.state()==Menu::State::NewSandbox,"Normal New Sandbox key.");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (!menu.map_rules_can_start()&&std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start(),"Authored selected-map authority failed.");
    send(menu,SDLK_RETURN);load_ready(menu);
}
std::string text(const std::vector<std::string>& lines) {
    std::string result;for (const auto& line:lines) { result+=line;result+=' '; }return result;
}
void contains(const std::vector<std::string>& lines,const std::string& value,const char* message) {
    if (text(lines).find(value)==std::string::npos) throw std::runtime_error(std::string(message)+": "+text(lines));
}
void paid(Menu& menu,sim::Command command) {
    auto* view=menu.sandbox();const auto before=view->world().command_sequence();
    const auto result=view->request_execute(command);
    if (view->budget_warning_pending()) send(menu,SDLK_Y);
    else check(result.accepted&&result.changed,"Ordinary UI purchase rejected.");
    check(!view->budget_warning_pending()&&view->world().command_sequence()==before+1,
        "Fixture UI confirmation did not buy exactly once.");
}
std::array<sim::Command,8> critical_buildings() { return {{
    {sim::CommandType::PlaceHousehold,{106,101}},
    {sim::CommandType::PlaceHousehold,{106,104}},
    {sim::CommandType::PlaceHousehold,{109,101}},
    {sim::CommandType::PlaceClaySource,{100,101}},
    {sim::CommandType::PlacePottery,{100,104}},
    {sim::CommandType::PlaceWarehouse,{103,101}},
    {sim::CommandType::PlaceMarket,{103,104}},
    {sim::CommandType::PlaceFarm,{104,104}}}}; }
void build_critical(Menu& menu) {
    // The first road is road_allowed but never building_allowed. Every later
    // budget preflight must retain its exact immutable policy and content.
    paid(menu,{sim::CommandType::PlaceRoad,{110,110}});
    const auto policy=menu.sandbox()->world().map_permissions();
    check(policy&&policy->road_allowed({110,110})&&!policy->building_allowed({110,110}),
        "Authored road-only source was lost.");
    for (const auto command:critical_buildings()) paid(menu,command);
    int roads=1;
    for (int y=117;y<=126&&roads<102;++y) for (int x=100;x<=115&&roads<102;++x) {
        paid(menu,{sim::CommandType::PlaceRoad,{x,y}});++roads;
    }
    auto* view=menu.sandbox();check(view->world().treasury()==106&&view->world().workforce_supply()==18&&
        view->world().workforce_required()==20&&view->world().workers_assigned(sim::BuildingId{8})==0&&
        view->world().map_permissions()==policy,"Independent critical UI fixture arithmetic/policy changed.");
}
void income_body_pixels(Menu& menu,SDL_Window* window,SDL_Renderer* renderer) {
    // A separate positive glyph raster establishes actual pixel visibility;
    // complete DebugText arguments alone do not prove readable output.
    check(menu.render(),"Income positive pixel render failed.");
    using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
    Surface actual(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(static_cast<bool>(actual),"Income production pixel readback failed.");
    int output_w=0,output_h=0,window_w=0,window_h=0;
    check(SDL_GetCurrentRenderOutputSize(renderer,&output_w,&output_h)&&
        SDL_GetWindowSize(window,&window_w,&window_h),"Income pixel dimensions failed.");
    const auto layout=oe::sandbox_ui::make_layout(output_w,output_h,window_w,window_h,
        true,true,true,true,true,true);
    const int x=layout.panel.x+10*layout.scale;
    const int y=layout.panel.y+46*layout.scale;
    const float scale=1.25F*static_cast<float>(layout.scale);
    check(SDL_SetRenderClipRect(renderer,nullptr)&&SDL_SetRenderScale(renderer,1,1)&&
        SDL_SetRenderDrawColor(renderer,0,0,0,255)&&SDL_RenderClear(renderer)&&
        SDL_SetRenderDrawColor(renderer,205,225,238,255)&&
        SDL_SetRenderScale(renderer,scale,scale)&&
        SDL_RenderDebugText(renderer,static_cast<float>(x)/scale,
            static_cast<float>(y)/scale,"Funds: 106")&&SDL_SetRenderScale(renderer,1,1),
        "Independent unclipped Income glyph control failed.");
    Surface control(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(static_cast<bool>(control),"Income glyph control readback failed.");
    std::size_t expected=0,visible=0;
    for (int py=y;py<y+12*layout.scale;++py) for (int px=x;px<x+100*layout.scale;++px) {
        Uint8 r=0,g=0,b=0,a=0;
        check(SDL_ReadSurfacePixel(control.get(),px,py,&r,&g,&b,&a),"Income control pixel failed.");
        if (r!=205||g!=225||b!=238||a!=255) continue;
        ++expected;
        check(SDL_ReadSurfacePixel(actual.get(),px,py,&r,&g,&b,&a),"Income actual pixel failed.");
        if (r==205&&g==225&&b==238&&a==255) ++visible;
    }
    check(expected>100,"Independent Income glyph control had no positive pixels.");
    if (visible!=expected) throw std::runtime_error("Income body Funds glyphs clipped at intended panel origin: "+
        std::to_string(visible)+"/"+std::to_string(expected)+" positive pixels.");
    check(menu.render(),"Income redraw after private pixel control failed.");
}
void main_paths(SDL_Window* window,SDL_Renderer* renderer) {
    f::Temp temp;const auto root=temp.root/"app";
    oe::menu::Settings settings;settings.data_root=temp.root/"data";
    settings.profile=sim::RulesProfile::CityV16;settings.prepared_starter=false;
    settings.last_map="Cities/A.map";oe::menu::write_settings(root,settings);
    Menu menu({},root,std::make_unique<NoDialog>());menu.initialize(window,renderer);start(menu);build_critical(menu);
    auto* view=menu.sandbox();const auto before=view->world().snapshot();
    const auto policy=view->world().map_permissions();const auto canonical=policy->canonical_state();
    contains(view->income_summary_lines(),"Farm #8 unstaffed 0/4","Concrete unstaffed Farm missing.");
    contains(view->income_summary_lines(),"Service Post cost 100","Infrastructure price missing.");
    contains(view->income_summary_lines(),"Service missing 3","Actual House service condition missing.");
    send(menu,SDLK_T);check(view->income_open(),"Normal T did not open Income.");
    contains(view->income_lines(),"Funds: 106","Current Funds missing.");
    contains(view->income_lines(),"Taxes: none received yet","Historical tax status missing.");
    contains(view->income_lines(),"Extra Houses estimate: 1 / 80 Funds","Full-plan House estimate missing.");
    contains(view->income_lines(),"Next future bill: 40 in 400 ticks","Placement-relative future bill missing.");
    contains(view->income_lines(),"Funds after that upkeep: 66","Explicit no-income assumption missing.");
    income_body_pixels(menu,window,renderer);
    check(view->world().snapshot()==before,"Income mutated the World.");
    namespace perf=oe::performance;perf::set_enabled(true);perf::reset();
    for (int i=0;i<100;++i) { (void)view->income_summary_lines();(void)view->income_lines();check(menu.render(),"Income render failed."); }
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,
        perf::Counter::WorldExecutes,perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,
        perf::Counter::AssetDecodes,perf::Counter::TextureUploads,perf::Counter::FileReads,
        perf::Counter::FileWrites,perf::Counter::SimulationTicks}) check(perf::counter(counter)==0,
            "Read-only Income queried assets/World/navigation/files.");
    perf::set_enabled(false);check(view->world().snapshot()==before,"Read-only Income changed snapshot.");
    send(menu,SDLK_T);check(!view->income_open(),"T did not restore the ordinary inspector.");
    const sim::Command fourth{sim::CommandType::PlaceHousehold,{109,104}};
    const auto open=[&] { const auto requested=view->request_execute(fourth);
        check(!requested.accepted&&view->budget_warning_pending()&&view->world().snapshot()==before,
            "Concrete warning committed before explicit consent.");
        contains(view->budget_warning_lines(),"Purchase: 80 Funds; afterwards: 26","Purchase consequence missing.");
        contains(view->budget_warning_lines(),"Short 74","Concrete building shortfall missing.");
        contains(view->budget_warning_lines(),"Service Post","Remaining missing building missing.");
        contains(view->budget_warning_lines(),"+0 Houses / 0 Funds","Purchased House counted twice.");
        check(menu.render(),"Concrete budget warning render failed.");
    };
    for (const auto cancel:{SDLK_RETURN,SDLK_KP_ENTER,SDLK_ESCAPE,SDLK_N}) {
        open();send(menu,cancel);check(!view->budget_warning_pending()&&view->world().snapshot()==before,
            "Default Cancel changed tick, Funds, IDs, roads or economy.");
    }
    open();send(menu,SDLK_Y);check(!view->budget_warning_pending()&&view->world().treasury()==26&&
        view->world().command_sequence()==before.command_sequence+1&&view->world().workforce_supply()==24,
        "Build anyway did not buy exactly one ordinary House.");
    send(menu,SDLK_Y);check(view->world().command_sequence()==before.command_sequence+1,
        "Repeated unrelated Y bought twice.");
    check(view->world().map_permissions()==policy&&policy->canonical_state()==canonical&&
        !policy->building_allowed({110,110}),"Budget copied only the old building mask.");
    send(menu,SDLK_T);contains(view->income_lines(),"Short 74 for these buildings alone.","Current post-purchase gap stale.");
    const auto committed=view->world().snapshot();send(menu,SDLK_F5);const auto save_path=menu.settings().last_save;
    check(!save_path.empty()&&oe::persistence::read_save(save_path).world==committed,"Normal F5 changed save authority.");
    view->tick_once();send(menu,SDLK_F9);check(view->paused()&&view->world().snapshot()==committed,
        "Normal F9 failed exact restore.");
    contains(view->income_lines(),"Funds: 26","Income stale after normal Load.");menu.shutdown();
    Menu restarted({},root,std::make_unique<NoDialog>());restarted.initialize(window,renderer);
    send(restarted,SDLK_L);send(restarted,SDLK_RETURN);load_ready(restarted);
    check(restarted.sandbox()->world().snapshot()==committed&&restarted.sandbox()->world().map_permissions()&&
        restarted.sandbox()->world().map_permissions()->canonical_state()==canonical,
        "Fresh MenuSession restart changed snapshot or policy.");
    contains(restarted.sandbox()->income_summary_lines(),"short 74","Restart retained stale income guidance.");
    restarted.shutdown();
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software")&&
            SDL_Init(SDL_INIT_VIDEO),"SDL setup.");
        check(SDL_CreateWindowAndRenderer("Authored economy guidance UI",1100,700,SDL_WINDOW_HIDDEN,
            &window,&renderer),"SDL window.");main_paths(window,renderer);
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Normal menu/budget/Income: exact Cancel + one paid confirmation, pure diagnostics, full road-only policy, F5/F9 and fresh-session restore PASS\n";return 0;
    } catch (const std::exception& error) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
