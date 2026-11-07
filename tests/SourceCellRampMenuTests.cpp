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
    check(menu.state()==Menu::State::Playing,"Actual candidate did not enter Playing");
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
void source_bounds(Menu& menu) {
    const auto source=menu.diagnostic_button_rect("Source-cell ramps (rule 20)");
    check(source.has_value(),"City-v16 lost the explicit source-cell choice");
    for (const auto* label:{"Start sandbox","Back","Check again","Enable Ramps + Bridges (rule 6)"}) {
        const auto other=menu.diagnostic_button_rect(label);
        check(other && !SDL_HasRectIntersectionFloat(&*source,&*other),"Source-cell choice overlaps an existing control");
    }
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
        check(SDL_CreateWindowAndRenderer("Authored source-cell menu",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create menu window");
        oe::testing::economy::Temp source;const auto data=source.root/"data",app=source.root/"app";
        oe_production_fixture::write_model(data);
        oe::menu::Settings preferences;preferences.data_root=data;preferences.last_map="Cities/A.map";
        oe::menu::write_settings(app,preferences);
        auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
        Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);unchanged_defaults(menu);
        click(menu,window,renderer,"Play Original economy");wait_map(menu,18);
        check(menu.original_deadline_food_dispatch_setup(),"Source-cell addition changed recommended Rule18");
        key(menu,SDLK_ESCAPE);key(menu,SDLK_N);wait_map(menu,3);
        check(!menu.original_setup(),"Ordinary N inherited an Original generation");
        for (unsigned n=0;menu.settings().profile!=sim::RulesProfile::CityV16 && n<16;++n)
            click(menu,window,renderer,"Next rules");
        wait_map(menu,4);source_bounds(menu);
        click(menu,window,renderer,"Prepared starter settlement");
        check(menu.diagnostic_button_rect("Empty city").has_value(),"Could not select an empty authored city");
        click(menu,window,renderer,"Source-cell ramps (rule 20)");wait_source(menu,20);
        check(!menu.original_setup(),"Rule20 acquired Original-production authority");
        click(menu,window,renderer,"Source-cell ramps ON (rule 20)");wait_map(menu,4);
        click(menu,window,renderer,"Enable Ramps + Bridges (rule 6)");wait_source(menu,6);
        click(menu,window,renderer,"Source-cell ramps (rule 20)");wait_source(menu,20);
        key(menu,SDLK_RETURN);finish_load(menu);
        std::cout<<"Actual source-cell start: rule="<<menu.sandbox()->world().rule_version()
            <<" supported="<<menu.sandbox()->world().source_cell_ramps_supported()
            <<" original_production="<<menu.sandbox()->world().original_production_enabled()
            <<" paused="<<menu.sandbox()->paused()<<" ticks="<<menu.sandbox()->world().ticks()
            <<" source_cell_ramps="<<menu.sandbox()->world().source_cell_ramps().size()<<'\n';
        check(menu.sandbox()->world().rule_version()==20 && menu.sandbox()->world().source_cell_ramps_supported() &&
            !menu.sandbox()->world().original_production_enabled() && !menu.sandbox()->paused() &&
            menu.sandbox()->world().ticks()==0 && menu.sandbox()->world().source_cell_ramps().empty(),
            "Explicit source-cell start lost generation, classic running default, tick or empty-state identity");
        const auto before_pause=menu.sandbox()->world().snapshot();key(menu,SDLK_SPACE);
        check(menu.sandbox()->paused() && menu.sandbox()->world().snapshot()==before_pause,
            "Actual Space did not pause the classic sandbox without changing World authority");
        key(menu,SDLK_F5);const auto manual=menu.settings().last_save;
        const auto bytes=oe_influence_fixture::read_text(manual);
        const auto document=menu.sandbox()->capture_save_document();
        const auto json=nlohmann::json::parse(bytes);
        check(json.at("schema_version")==36 && json.at("rules").at("version")==20 &&
            json.at("rules").at("source_cell_ramps")==sim::authored_source_cell_ramp_contract &&
            json.at("world").at("source_cell_ramps").empty(),"Rule20 F5 lost schema36 exact empty-state marker");
        menu.sandbox()->tick_once();key(menu,SDLK_F9);
        oe_influence_fixture::same_document(document,menu.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"Rule20 F9 changed the supplied save");
        const auto catalog=oe::persistence::RecoveryStore(app,data).catalog();
        fs::path protected_checkpoint;std::string checkpoint_bytes;
        for (const auto& history:catalog.histories) for (const auto& entry:history.entries) {
            check(entry.loadable() && entry.schema==36 && entry.rule_version==20,"Rule20 LegacyOnly Recovery is not exact/loadable");
            if (entry.start_point) {protected_checkpoint=entry.path;checkpoint_bytes=oe_influence_fixture::read_text(entry.path);}
        }
        check(!protected_checkpoint.empty(),"Rule20 has no protected Recovery start point");
        menu.shutdown();

        auto fresh_dialog=std::make_unique<Dialog>();callbacks=fresh_dialog.get();
        Menu fresh({},app,std::move(fresh_dialog));fresh.initialize(window,renderer);
        key(fresh,SDLK_N);wait_map(fresh,4);key(fresh,SDLK_ESCAPE);
        open_save(fresh,*callbacks,window,renderer,manual);
        check(fresh.sandbox()->world().rule_version()==20 && fresh.sandbox()->paused(),"Fresh load did not retain exact Rule20 paused");
        oe_influence_fixture::same_document(document,fresh.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"Fresh menu changed schema36 input bytes");
        fresh.shutdown();

        auto managed_dialog=std::make_unique<Dialog>();callbacks=managed_dialog.get();
        Menu managed({},app,std::move(managed_dialog));managed.initialize(window,renderer);
        open_save(managed,*callbacks,window,renderer,protected_checkpoint);
        check(managed.sandbox()->world().rule_version()==20 && managed.sandbox()->paused(),"Recovery chooser lost exact Rule20");
        key(managed,SDLK_F5);
        check(managed.settings().last_save!=protected_checkpoint &&
            oe_influence_fixture::read_text(protected_checkpoint)==checkpoint_bytes,
            "F5 overwrote an externally selected managed schema36 Recovery payload");
        managed.shutdown();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Rule20 explicit menu/preflight/schema36/F5/F9/Recovery/fresh load and unchanged recommended18/legacy3/4 PASS; scripted SDL, native/human NOT_RUN\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';if(renderer) SDL_DestroyRenderer(renderer);if(window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
