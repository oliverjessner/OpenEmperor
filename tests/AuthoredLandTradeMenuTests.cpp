// Entirely authored inputs and actual queued SDL events; no original-data fixture.
#include "EconomyStartViewFixture.h"
#include "OriginalProductionFixture.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace fs=std::filesystem;
using Menu=oe::menu::MenuSession;
using oe::testing::economy::check;
class Dialog final:public oe::menu::DialogAdapter {
public:
    Callback callback;
    void open_folder(SDL_Window*,Callback value) override {callback=std::move(value);}
    void open_file(SDL_Window*,Callback value) override {callback=std::move(value);}
    void cancel_pending() override {callback={};}
    void answer(const fs::path& path) {
        auto value=std::move(callback);check(bool(value),"Expected the real chooser callback");
        value({oe::menu::DialogResult::Kind::Selected,path.string()});
    }
};
void event(Menu& menu,SDL_Event value) {
    check(SDL_PushEvent(&value),"Queue actual menu event");
    while(SDL_PollEvent(&value))menu.handle_event(value);
}
void key(Menu& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(menu,e);menu.advance();
}
void click(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const std::string& label) {
    check(menu.render(),"Render actual control");const auto rect=menu.diagnostic_button_rect(label);
    check(rect.has_value(),("Missing menu control: "+label).c_str());
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));SDL_FPoint point{};
    check(SDL_RenderCoordinatesToWindow(renderer,(rect->x+rect->w/2)*scale,
        (rect->y+rect->h/2)*scale,&point.x,&point.y),"Invert actual menu coordinates");
    const auto hit=menu.diagnostic_button_hit(point.x,point.y);check(hit && hit->enabled,"Actual button is not enabled");
    for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}) {
        SDL_Event e{};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=point.x;e.button.y=point.y;event(menu,e);
    }
    menu.advance();
}
void wait_map(Menu& menu,unsigned rule) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking &&
        std::chrono::steady_clock::now()<deadline){menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start(),"Actual authored-map preflight refused");
    check(menu.map_rules_result().request.rule_version==rule,"Preflight selected another rule");
}
void finish_load(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"Expected deferred Loading state");
    check(menu.render(),"Draw Loading before committing candidate");menu.advance();
    if(menu.state()!=Menu::State::Playing)throw std::runtime_error("Actual candidate did not enter Playing: "+menu.message());
}
void unchanged_defaults(const Menu& menu) {
    check(menu.settings().profile==sim::RulesProfile::CityV11 && menu.settings().prepared_starter &&
        sim::current_rule_version(sim::RulesProfile::CityV11)==3 && sim::current_rule_version(sim::RulesProfile::CityV16)==4,
        "Source-cell opt-in changed historical defaults");
}
void wait_source(Menu& menu,unsigned rule) {
    wait_map(menu,rule);
    check(menu.map_rules_result().request.profile==sim::RulesProfile::CityV16 &&
        menu.map_rules_result().request.policy_version==3,"Source-cell setup lost exact policy3 identity");
}
void captures(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const fs::path& output,const std::string& name) {
    for(const auto size:std::array<std::array<int,2>,3>{{{900,600},{1280,800},{1920,1080}}}) {
        check(SDL_SetWindowSize(window,size[0],size[1]),"Resize scripted menu");SDL_PumpEvents();
        SDL_Event event{};event.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;menu.handle_event(event);menu.advance();
        int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h),"Menu output dimensions");
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
        check(target && SDL_SetRenderTarget(renderer,target.get()),"Menu owned capture target");
        struct Restore{SDL_Renderer* r;~Restore(){SDL_SetRenderTarget(r,nullptr);}}restore{renderer};
        check(menu.render()&&SDL_GetRenderTarget(renderer)==target.get(),"Render menu before Present");
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
        check(frame&&frame->w==w&&frame->h==h&&SDL_SavePNG(frame.get(),(output/(name+"-"+std::to_string(size[0])+".png")).string().c_str()),"Menu PNG write");
    }
    check(SDL_SetWindowSize(window,1280,800),"Restore menu test size");SDL_PumpEvents();
    SDL_Event event{};event.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;menu.handle_event(event);menu.advance();
}
void open_save(Menu& menu,Dialog& dialog,SDL_Window* window,SDL_Renderer* renderer,const fs::path& path) {
    key(menu,SDLK_L);click(menu,window,renderer,"Open save file...");
    dialog.answer(path);menu.advance();finish_load(menu);
}
}

int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        check(SDL_CreateWindowAndRenderer("Authored land trade menu",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create menu window");
        const auto output=fs::current_path()/".local/oe54"/("menu-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(output);
        oe::testing::economy::Temp source;const auto data=source.root/"data",app=source.root/"app";
        oe_production_fixture::write_model(data);
        oe::menu::Settings preferences;preferences.data_root=data;preferences.last_map="Cities/A.map";
        oe::menu::write_settings(app,preferences);
        auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
        Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);unchanged_defaults(menu);
        click(menu,window,renderer,"Play Original economy");wait_map(menu,18);
        check(menu.original_deadline_food_dispatch_setup(),"Trade addition changed recommended Rule18");
        key(menu,SDLK_ESCAPE);key(menu,SDLK_N);wait_map(menu,3);
        for(unsigned n=0;menu.settings().profile!=sim::RulesProfile::CityV16 && n<16;++n)
            click(menu,window,renderer,"Next rules");
        wait_map(menu,4);key(menu,SDLK_ESCAPE);
        click(menu,window,renderer,"Advanced / Legacy");
        captures(menu,window,renderer,output,"advanced");
        click(menu,window,renderer,"H Land trade (rule 21)");wait_source(menu,21);
        captures(menu,window,renderer,output,"trade-setup");
        check(menu.authored_land_trade_setup() && !menu.original_setup(),"Trade acquired Original setup authority");
        check(!menu.diagnostic_button_rect("P Paid four-house starter"),"Trade exposed an Original starter");
        key(menu,SDLK_D);check(menu.original_difficulty()==oe::original::Difficulty::Hard,"Trade model difficulty choice lost");
        key(menu,SDLK_ESCAPE);key(menu,SDLK_H);wait_source(menu,21);
        key(menu,SDLK_RETURN);finish_load(menu);
        const auto& world=menu.sandbox()->world();
        check(world.rule_version()==21 && world.authored_land_trade_enabled() && menu.sandbox()->paused() &&
            world.ticks()==0 && world.buildings().empty() && world.land_trade_state() && !world.land_trade_state()->binding &&
            !world.original_economy_enabled() && !world.original_production_enabled() &&
            world.land_trade_state()->model_source.difficulty==oe::original::Difficulty::Hard,
            "Explicit trade start lost paused/empty/exact/classic/model identity");
        key(menu,SDLK_F5);const auto manual=menu.settings().last_save;
        const auto bytes=oe_influence_fixture::read_text(manual);const auto document=menu.sandbox()->capture_save_document();
        const auto json=nlohmann::json::parse(bytes);
        check(json.at("schema_version")==37 && json.at("rules").at("version")==21,
            "Trade F5 lost exact schema37/rule21");
        menu.sandbox()->tick_once();key(menu,SDLK_F9);
        oe_influence_fixture::same_document(document,menu.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"F9 changed source save bytes");
        const auto catalog=oe::persistence::RecoveryStore(app,data).catalog();
        fs::path checkpoint;std::string checkpoint_bytes;
        for(const auto& history:catalog.histories)for(const auto& entry:history.entries) {
            check(entry.loadable() && entry.schema==37 && entry.rule_version==21,"Trade Recovery lost exact schema37");
            if(entry.start_point){checkpoint=entry.path;checkpoint_bytes=oe_influence_fixture::read_text(entry.path);}
        }
        check(!checkpoint.empty(),"Trade has no Recovery start point");menu.shutdown();
        auto fresh_dialog=std::make_unique<Dialog>();callbacks=fresh_dialog.get();
        Menu fresh({},app,std::move(fresh_dialog));fresh.initialize(window,renderer);
        key(fresh,SDLK_N);wait_map(fresh,4);key(fresh,SDLK_ESCAPE);
        open_save(fresh,*callbacks,window,renderer,manual);
        check(fresh.sandbox()->world().authored_land_trade_enabled() && fresh.sandbox()->paused(),"Fresh menu failed exact trade load");
        oe_influence_fixture::same_document(document,fresh.sandbox()->capture_save_document());fresh.shutdown();
        auto managed_dialog=std::make_unique<Dialog>();callbacks=managed_dialog.get();
        Menu managed({},app,std::move(managed_dialog));managed.initialize(window,renderer);
        open_save(managed,*callbacks,window,renderer,checkpoint);key(managed,SDLK_F5);
        check(managed.settings().last_save!=checkpoint && oe_influence_fixture::read_text(checkpoint)==checkpoint_bytes,
            "F5 overwrote managed trade Recovery input");managed.shutdown();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Rule21 explicit menu/preflight/model/schema37/F5F9/Recovery/fresh load/defaults PASS; scripted SDL only; native/human NOT_RUN\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;}
}
