// Authored inputs and scripted SDL delivery; no original data or human claim.
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
        auto value=std::move(callback);check(static_cast<bool>(value),"Expected real file-chooser callback");
        value({oe::menu::DialogResult::Kind::Selected,path.string()});
    }
};
void event(Menu& menu,SDL_Event value) {
    check(SDL_PushEvent(&value),"Queue actual menu event");
    while (SDL_PollEvent(&value)) menu.handle_event(value);
}
void key(Menu& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(menu,e);menu.advance();
}
SDL_FPoint point(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const std::string& label) {
    check(menu.render(),"Render menu control");const auto rect=menu.diagnostic_button_rect(label);
    check(rect.has_value(),("Missing menu control: "+label).c_str());
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));
    SDL_FPoint result{};
    check(SDL_RenderCoordinatesToWindow(renderer,(rect->x+rect->w/2)*scale,
        (rect->y+rect->h/2)*scale,&result.x,&result.y),"Invert actual menu coordinates");
    const auto hit=menu.diagnostic_button_hit(result.x,result.y);
    check(hit && hit->enabled,"Actual visible menu button has no enabled hit");return result;
}
void mouse(Menu& menu,SDL_FPoint p,bool down) {
    SDL_Event e{};e.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;event(menu,e);
}
void click(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const std::string& label) {
    const auto p=point(menu,window,renderer,label);mouse(menu,p,true);mouse(menu,p,false);menu.advance();
}
void wait_map(Menu& menu,unsigned rule) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking &&
        std::chrono::steady_clock::now()<deadline) {menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start(),"Actual map preflight refused authored input");
    const auto& request=menu.map_rules_result().request;
    check(request.rule_version==rule,"Setup preflight selected another rule");
    if (rule==19) check(request.profile==sim::RulesProfile::CityV16 && request.policy_version==3,
        "Rule19 replaced source-bound policy3");
}
void finish_load(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"Expected deferred Loading state");
    check(menu.render(),"Draw Loading before candidate commit");menu.advance();
    check(menu.state()==Menu::State::Playing,"Actual candidate did not enter Playing");
}
constexpr std::array options{"N New Sandbox","L Load Save","Original Data Folder","Quit",
    "O Original economy","F Original fire protection","S Sustainable economy","M Needs-aware logistics",
    "W Cardinal-water economy","I Inspector coordination","D Deadline-aware fire dispatch",
    "T Deadline-aware services","U Deadline-aware food","V Safe demolition economy","Autosave: ON","Back to main menu"};
void bounds(Menu& menu,bool retained) {
    std::vector<SDL_FRect> rectangles;
    for (const auto* label:options) {
        const auto r=menu.diagnostic_button_rect(label);check(r.has_value(),"Advanced menu lost an explicit choice");
        check(r->x>=0 && r->y>=0 && r->x+r->w<=600 && r->y+r->h<=358,
            "Advanced button escapes minimum900x600 aperture or footer");
        for (const auto& other:rectangles) check(!SDL_HasRectIntersectionFloat(&*r,&other),"Advanced controls overlap");
        rectangles.push_back(*r);
    }
    if (retained) {
        const auto r=menu.diagnostic_button_rect("Enter Continue");check(r.has_value(),"Retained city lost Continue");
        for (const auto& other:rectangles) check(!SDL_HasRectIntersectionFloat(&*r,&other),"Continue overlaps an explicit mode");
    }
}
void capture(Menu& menu,SDL_Renderer* renderer,const fs::path& output,const std::string& name) {
    if (output.empty()) return;
    check(SDL_GetRenderTarget(renderer)==nullptr,"Menu capture requires the ordinary window target");
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height) && width>0 && height>0,"Read actual menu output dimensions");
    const auto state=menu.state();
    const auto before=menu.sandbox() ? std::optional{menu.sandbox()->world().snapshot()}:std::nullopt;
    const auto camera=menu.sandbox() ? std::optional{menu.sandbox()->camera()}:std::nullopt;
    fs::create_directories(output);
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Create own menu capture target");
    struct Restore {SDL_Renderer* renderer;~Restore(){SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    check(menu.render(),"Render menu screenshot into owned target");
    check(SDL_GetRenderTarget(renderer)==target.get(),"Menu render changed the owned readback target");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(frame && frame->w==width && frame->h==height && SDL_SavePNG(frame.get(),(output/(name+".png")).string().c_str()),
        "Write same-output-size actual menu screenshot");
    check(menu.state()==state && (!before || (menu.sandbox() && menu.sandbox()->world().snapshot()==*before)),
        "Menu capture mutated authoritative World or menu state");
    if (camera) {
        const auto after=menu.sandbox()->camera();
        check(after.zoom==camera->zoom && after.offset.x==camera->offset.x && after.offset.y==camera->offset.y &&
            after.viewport_width==camera->viewport_width && after.viewport_height==camera->viewport_height,
            "Same-output-size menu capture changed the retained camera");
    }
}
void unchanged_defaults(const Menu& menu) {
    check(menu.settings().profile==sim::RulesProfile::CityV11 && menu.settings().prepared_starter &&
        sim::current_rule_version(sim::RulesProfile::CityV11)==3 && sim::current_rule_version(sim::RulesProfile::CityV16)==4,
        "Rule19 changed historical defaults");
}
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==1 || (argc==3 && std::string_view(argv[1])=="--capture"),"Unknown safe-demolition menu arguments");
        const fs::path output=argc==3 ? fs::absolute(argv[2]):fs::path{};
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        check(SDL_CreateWindowAndRenderer("Safe demolition menu",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create minimum menu");
        oe::testing::economy::Temp source;const auto data=source.root/"data",app=source.root/"app";
        oe_production_fixture::write_model(data);
        oe::menu::Settings preferences;preferences.data_root=data;preferences.last_map="Cities/A.map";
        oe::menu::write_settings(app,preferences);
        auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
        Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);unchanged_defaults(menu);
        check(!menu.original_safe_demolition_setup() && !menu.diagnostic_button_rect("V Safe demolition economy"),
            "Rule19 became a default or compact main-menu recommendation");
        click(menu,window,renderer,"Play Original economy");wait_map(menu,18);
        check(menu.original_deadline_food_dispatch_setup() && !menu.original_safe_demolition_setup(),"Recommended mode silently changed Rule18");
        key(menu,SDLK_ESCAPE);key(menu,SDLK_N);wait_map(menu,3);
        check(!menu.original_setup(),"N inherited an Original-economy version");key(menu,SDLK_ESCAPE);
        click(menu,window,renderer,"Advanced / Legacy");bounds(menu,false);
        const auto v=point(menu,window,renderer,"V Safe demolition economy");mouse(menu,v,true);
        SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;event(menu,lost);mouse(menu,v,false);menu.advance();
        check(menu.state()==Menu::State::MainMenu,"Focus boundary activated an armed Rule19 choice");
        capture(menu,renderer,output,"advanced.minimum");
        for (const auto& [code,rule]:std::array{std::pair{SDLK_O,10U},std::pair{SDLK_F,11U},std::pair{SDLK_S,12U},
            std::pair{SDLK_M,13U},std::pair{SDLK_W,14U},std::pair{SDLK_I,15U},std::pair{SDLK_D,16U},
            std::pair{SDLK_T,17U},std::pair{SDLK_U,18U}}) {
            key(menu,code);wait_map(menu,rule);check(!menu.original_safe_demolition_setup(),"Historical shortcut selected Rule19");key(menu,SDLK_ESCAPE);
        }
        click(menu,window,renderer,"Advanced / Legacy");bounds(menu,false);
        click(menu,window,renderer,"V Safe demolition economy");wait_map(menu,19);
        check(menu.original_safe_demolition_setup() && !menu.original_deadline_food_dispatch_setup() &&
            menu.original_difficulty()==oe::original::Difficulty::Normal && menu.original_paid_starter(),"Explicit Rule19 setup lost exact defaults");
        capture(menu,renderer,output,"setup.minimum");key(menu,SDLK_RETURN);finish_load(menu);
        const auto& world=menu.sandbox()->world();
        check(world.rule_version()==19 && world.original_safe_demolition_enabled() &&
            !sim::original_deadline_food_dispatch_enabled(world.profile(),world.rule_version()) &&
            world.ticks()==0 && world.total_population()==0 && world.construction_spent_total()>0 &&
            world.treasury()==sim::original_production_starting_treasury-static_cast<std::int64_t>(world.construction_spent_total()),
            "Rule19 starter changed generation, payment, tick or population authority");
        const auto houses=static_cast<std::size_t>(std::count_if(world.buildings().begin(),world.buildings().end(),
            [](const auto& b){return sim::original_house_kind(b.kind);}));check(houses==4,"Rule19 paid starter did not buy four Houses");
        key(menu,SDLK_F5);const auto manual=menu.settings().last_save;
        const auto bytes=oe_influence_fixture::read_text(manual);const auto document=menu.sandbox()->capture_save_document();
        const auto json=nlohmann::json::parse(bytes);
        check(json.at("schema_version")==35 && json.at("rules").at("safe_demolition")==sim::original_safe_demolition_contract &&
            json.at("rules").at("deadline_food_dispatch")==sim::original_deadline_food_dispatch_contract,
            "Rule19 did not write schema35 and exact independent/inherited markers");
        menu.sandbox()->tick_once();key(menu,SDLK_F9);oe_influence_fixture::same_document(document,menu.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"F9 rewrote source save bytes");
        const auto recovery=oe::persistence::RecoveryStore(app,data,oe::persistence::recovery_storage_budget_bytes,
            oe::persistence::RecoveryAccess::OriginalProduction).catalog();
        check(!recovery.histories.empty(),"Rule19 has no protected Recovery history");
        for (const auto& history:recovery.histories) for (const auto& entry:history.entries)
            check(entry.loadable() && entry.schema==35 && entry.rule_version==19,"Rule19 Recovery changed generation or is not loadable");
        key(menu,SDLK_ESCAPE);click(menu,window,renderer,"Advanced / Legacy");bounds(menu,true);
        for (const auto size:std::array{std::pair{1280,720},std::pair{1600,900},std::pair{1920,1080}}) {
            check(SDL_SetWindowSize(window,size.first,size.second),"Resize actual menu");SDL_Event resized{};resized.type=SDL_EVENT_WINDOW_RESIZED;
            event(menu,resized);check(menu.render(),"Draw resized Advanced menu");bounds(menu,true);
            capture(menu,renderer,output,"advanced.retained."+std::to_string(size.first));
        }
        unchanged_defaults(menu);menu.shutdown();
        Menu fresh({},app,std::make_unique<Dialog>());fresh.initialize(window,renderer);
        unchanged_defaults(fresh);check(!fresh.original_safe_demolition_setup(),"Restart persisted session-only Rule19 setup");
        fresh.shutdown();
        auto load_dialog=std::make_unique<Dialog>();callbacks=load_dialog.get();
        Menu loaded({},app,std::move(load_dialog));loaded.initialize(window,renderer);key(loaded,SDLK_L);
        click(loaded,window,renderer,"Open save file...");callbacks->answer(manual);loaded.advance();finish_load(loaded);
        check(loaded.sandbox()->world().rule_version()==19 && loaded.sandbox()->paused(),"Fresh menu did not restore Rule19 paused");
        oe_influence_fixture::same_document(document,loaded.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"Fresh menu changed supplied save bytes");loaded.shutdown();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Rule19 explicit V/menu/schema35/Recovery/fresh load and unchanged recommended18/legacy defaults PASS; scripted SDL, native/human NOT_RUN\n";return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';if (renderer) SDL_DestroyRenderer(renderer);if (window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
