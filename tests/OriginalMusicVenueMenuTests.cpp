// Authored data and scripted SDL only; no native or human acceptance claim.
#include "EconomyStartViewFixture.h"
#include "OriginalMusicVenueFixture.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>
#include <memory>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace fs=std::filesystem;
namespace q=oe_music_venue_fixture;
using Menu=oe::menu::MenuSession;
using q::check;
class Dialog final:public oe::menu::DialogAdapter {
public:
    void open_folder(SDL_Window*,Callback) override {}
    void open_file(SDL_Window*,Callback) override {}
    void cancel_pending() override {}
};
void event(Menu& menu,SDL_Event value) {
    check(SDL_PushEvent(&value),"Queue actual menu event");
    while(SDL_PollEvent(&value))menu.handle_event(value);
}
void key(Menu& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(menu,e);menu.advance();
}
void click(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const std::string& label) {
    check(menu.render(),"Render actual menu controls");
    const auto rect=menu.diagnostic_button_rect(label);
    check(rect.has_value(),"Missing menu control: "+label);
    const float scale=1.5F*std::max(1.F,SDL_GetWindowPixelDensity(window));
    SDL_FPoint point{};
    check(SDL_RenderCoordinatesToWindow(renderer,(rect->x+rect->w/2)*scale,
        (rect->y+rect->h/2)*scale,&point.x,&point.y),"Invert menu coordinates");
    const auto hit=menu.diagnostic_button_hit(point.x,point.y);
    check(hit&&hit->enabled,"Visible menu control lost its enabled hit");
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=point.x;e.button.y=point.y;event(menu,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(menu,e);menu.advance();
}
void wait_map(Menu& menu,unsigned rule) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking&&
        std::chrono::steady_clock::now()<deadline){menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start(),"Actual map preflight refused: "+menu.message());
    check(menu.map_rules_result().request.rule_version==rule,"Preflight changed requested rule");
}
void finish(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"Expected deferred Loading state");
    check(menu.render(),"Present Loading before commit");menu.advance();
    check(menu.state()==Menu::State::Playing,"Candidate failed: "+menu.message());
}
void bounded(Menu& menu) {
    const std::array labels{"B Music venues (rule 22)","Back to main menu","N New Sandbox",
        "L Load Save","Original Data Folder","Quit","O Original economy",
        "F Original fire protection","S Sustainable economy","M Needs-aware logistics",
        "W Cardinal-water economy","I Inspector coordination","D Deadline-aware fire dispatch",
        "T Deadline-aware services","U Deadline-aware food","V Safe demolition economy",
        "H Land trade (rule 21)","Autosave: ON"};
    std::vector<SDL_FRect> rectangles;
    if(const auto retained=menu.diagnostic_button_rect("Enter Continue"))rectangles.push_back(*retained);
    for(const auto* label:labels) {
        const auto r=menu.diagnostic_button_rect(label);check(r.has_value(),"Missing Advanced choice: "+std::string(label));
        check(r->x>=0&&r->y>=0&&r->x+r->w<=600&&r->y+r->h<=358,"Advanced choice escapes minimum menu");
        const SDL_FRect heading{40,40,230,40};
        check(!SDL_HasRectIntersectionFloat(&*r,&heading),"Advanced control overlaps heading/version/help");
        for(const auto& other:rectangles)check(!SDL_HasRectIntersectionFloat(&*r,&other),"Advanced choices overlap");
        rectangles.push_back(*r);
    }
}
void capture(Menu& menu,SDL_Renderer* renderer,const fs::path& path) {
    check(SDL_GetRenderTarget(renderer)==nullptr,"Capture starts at normal window target");
    int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h),"Read actual output size");
    const auto state=menu.state();
    const auto world=menu.sandbox()?std::optional{menu.sandbox()->world().snapshot()}:std::nullopt;
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Own RGBA32 capture target");
    struct Restore {SDL_Renderer* renderer;~Restore(){SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    check(menu.render()&&SDL_GetRenderTarget(renderer)==target.get(),"Render into owned target before readback");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> image(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(image&&image->w==w&&image->h==h&&SDL_SavePNG(image.get(),path.string().c_str()),"Save complete pre-Present capture");
    check(menu.state()==state&&(!world||menu.sandbox()->world().snapshot()==*world),"Capture changed menu/World");
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const auto output=fs::current_path()/".local/oe56"/("music-menu-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(output);
        oe::testing::economy::Temp temp;const auto data=temp.root/"data",app=temp.root/"app";
        oe_production_fixture::write_model(data);q::f::write_text(data/"Model/EmperorBuildingModels.txt",q::authored_model());
        const auto source=q::f::read_text(data/"Cities/A.map"),model=q::f::read_text(data/"Model/EmperorBuildingModels.txt");
        oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/A.map";
        oe::menu::write_settings(app,settings);
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        check(SDL_CreateWindowAndRenderer("Music venues menu",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create actual menu");
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
        check(!menu.original_music_venues_setup()&&!menu.diagnostic_button_rect("B Music venues (rule 22)"),"Rule22 became a default");
        click(menu,window,renderer,"Play Original economy");wait_map(menu,18);
        check(!menu.original_music_venues_setup(),"Recommendation changed from Rule18");key(menu,SDLK_ESCAPE);
        key(menu,SDLK_N);wait_map(menu,3);check(!menu.original_setup(),"N changed its ordinary profile");key(menu,SDLK_ESCAPE);
        click(menu,window,renderer,"Advanced / Legacy");check(menu.render(),"Render Advanced");bounded(menu);
        capture(menu,renderer,output/"advanced-900.png");
        click(menu,window,renderer,"B Music venues (rule 22)");wait_map(menu,22);
        check(menu.original_music_venues_setup()&&!menu.original_safe_demolition_setup()&&
            menu.original_difficulty()==oe::original::Difficulty::Normal,"Explicit Rule22 lost its setup identity");
        capture(menu,renderer,output/"setup-900.png");key(menu,SDLK_ESCAPE);key(menu,SDLK_B);wait_map(menu,22);
        for(const auto size:std::array{std::pair{1280,800},std::pair{1600,900},std::pair{1920,1080}}) {
            check(SDL_SetWindowSize(window,size.first,size.second),"Resize setup");SDL_Event e{};e.type=SDL_EVENT_WINDOW_RESIZED;event(menu,e);
            capture(menu,renderer,output/("setup-"+std::to_string(size.first)+".png"));
        }
        key(menu,SDLK_RETURN);finish(menu);
        const auto& world=menu.sandbox()->world();
        check(world.rule_version()==22&&world.original_music_venues_enabled()&&world.original_production_state()->culture&&
            !world.original_production_state()->safe_demolition&&!world.source_cell_ramps_supported()&&!world.authored_land_trade_enabled(),
            "Rule22 inherited unrelated authority");
        check(world.ticks()==0&&world.total_population()==0&&world.construction_spent_total()>0&&
            world.treasury()==12000-static_cast<std::int64_t>(world.construction_spent_total()),"Starter granted free state");
        key(menu,SDLK_F5);const auto manual=menu.settings().last_save;const auto bytes=q::f::read_text(manual);
        const auto document=menu.sandbox()->capture_save_document();const auto json=nlohmann::json::parse(bytes);
        check(json.at("schema_version")==38&&json.at("rules").at("music_venues")==sim::original_music_venues_contract&&
            json.at("rules").at("deadline_food_dispatch")==sim::original_deadline_food_dispatch_contract&&
            !json.at("rules").contains("safe_demolition"),"Save38 marker/inheritance differs");
        menu.sandbox()->tick_once();key(menu,SDLK_F9);key(menu,SDLK_F5);q::f::same_document(document,menu.sandbox()->capture_save_document());
        check(q::f::read_text(manual)==bytes,"F9 rewrote saved source bytes");
        const auto recovery=oe::persistence::RecoveryStore(app,data,oe::persistence::recovery_storage_budget_bytes,
            oe::persistence::RecoveryAccess::OriginalProduction).catalog();
        check(!recovery.histories.empty(),"Rule22 has no protected Recovery history");
        for(const auto& history:recovery.histories)for(const auto& entry:history.entries)
            check(entry.loadable()&&entry.schema==38&&entry.rule_version==22,"Recovery changed rule/schema");
        key(menu,SDLK_ESCAPE);click(menu,window,renderer,"Advanced / Legacy");check(menu.render(),"Retained Advanced");bounded(menu);
        capture(menu,renderer,output/"advanced-retained-1920.png");
        check(menu.settings().profile==sim::RulesProfile::CityV11&&sim::current_rule_version(sim::RulesProfile::CityV11)==3&&
            sim::current_rule_version(sim::RulesProfile::CityV16)==4,"Default profiles changed");
        menu.shutdown();
        Menu fresh({},app,std::make_unique<Dialog>());fresh.initialize(window,renderer);
        key(fresh,SDLK_L);key(fresh,SDLK_RETURN);finish(fresh);
        q::f::same_document(document,fresh.sandbox()->capture_save_document());fresh.shutdown();
        check(q::f::read_text(data/"Cities/A.map")==source&&q::f::read_text(data/"Model/EmperorBuildingModels.txt")==model,
            "Authored input bytes changed");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Rule22 menu opt-in/defaults/full F5F9/Recovery/load and scripted captures PASS: "<<output<<'\n';return 0;
    }catch(const std::exception& error){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
