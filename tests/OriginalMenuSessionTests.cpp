#include "EconomyStartViewFixture.h"
#include "OriginalProductionFixture.h"
#include "core/PerformanceDiagnostics.h"
#include "simulation/original/OriginalRulesModel.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <map>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace orig=oe::original;
namespace fs=std::filesystem;
using Menu=oe::menu::MenuSession;
using oe::testing::economy::check;

class Dialog final:public oe::menu::DialogAdapter {
public:
    Callback callback;
    void open_folder(SDL_Window*,Callback value) override { callback=std::move(value); }
    void open_file(SDL_Window*,Callback value) override { callback=std::move(value); }
    void cancel_pending() override { callback={}; }
    void answer(const fs::path& path) {
        auto value=std::move(callback);check(static_cast<bool>(value),"Expected an actual file chooser callback");
        value({oe::menu::DialogResult::Kind::Selected,path.string()});
    }
};
void route(Menu& menu,SDL_Event event) {
    check(SDL_PushEvent(&event),"Queue actual Original menu input");
    while (SDL_PollEvent(&event)) menu.handle_event(event);
}
void key(Menu& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;route(menu,e);menu.advance();
}
SDL_FPoint button_point(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const std::string& label) {
    check(menu.render(),"Draw actual menu before finding a control");
    const auto rect=menu.diagnostic_button_rect(label);
    if (!rect) throw std::runtime_error("Missing actual menu control: "+label);
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect->x+rect->w/2)*scale,
        (rect->y+rect->h/2)*scale,&x,&y),"Invert the actual menu render coordinates");
    const auto hit=menu.diagnostic_button_hit(x,y);
    check(hit.has_value(),"Rendered menu control has no matching pointer hit");
    return {x,y};
}
void mouse(Menu& menu,SDL_FPoint p,bool down) {
    SDL_Event e{};e.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;route(menu,e);
}
void click(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const std::string& label) {
    const auto p=button_point(menu,window,renderer,label);mouse(menu,p,true);mouse(menu,p,false);menu.advance();
}
void wait_check(Menu& menu) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking &&
           std::chrono::steady_clock::now()<deadline) {menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start(),"Actual map preflight did not permit Start");
}
void wait_map(Menu& menu) {
    wait_check(menu);
    const auto& request=menu.map_rules_result().request;
    check(request.profile==sim::RulesProfile::CityV16 && request.rule_version==10 && request.policy_version==3,
        "Original setup checked a legacy profile, rule or policy");
}
void finish_load(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"Expected ordinary deferred Loading state");
    check(menu.render(),"Draw actual Loading screen");menu.advance();
}
void start(Menu& menu) {wait_map(menu);key(menu,SDLK_RETURN);finish_load(menu);}
void open_legacy(Menu& menu,SDL_Window* window,SDL_Renderer* renderer) {
    check(menu.state()==Menu::State::MainMenu,"Legacy choices require the ordinary main menu");
    if (!menu.diagnostic_button_rect("O Original economy"))
        click(menu,window,renderer,"Advanced / Legacy");
    check(menu.diagnostic_button_rect("O Original economy").has_value(),"Advanced / Legacy did not expose historical choices");
}
void enter_original(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,bool by_mouse=false) {
    if (by_mouse) {open_legacy(menu,window,renderer);click(menu,window,renderer,"O Original economy");}
    else key(menu,SDLK_O);
    check(menu.state()==Menu::State::NewSandbox && menu.original_setup(),"Explicit Original entry did not select its own setup");
}
void capture(Menu& menu,SDL_Renderer* renderer,const fs::path& folder,const char* name) {
    if (folder.empty()) return;
    fs::create_directories(folder);check(menu.render(),"Draw menu capture");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> image(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(image && SDL_SavePNG(image.get(),(folder/(std::string(name)+".png")).string().c_str()),"Write actual menu PNG");
}
void same(const oe::persistence::SaveDocument& a,const oe::persistence::SaveDocument& b) {
    oe_influence_fixture::same_document(a,b);
}
std::size_t house_count(const sim::World& world) {
    return static_cast<std::size_t>(std::count_if(world.buildings().begin(),world.buildings().end(),
        [](const auto& b){return b.placed && sim::original_house_kind(b.kind);}));
}
oe::persistence::RecoveryCatalog recovery_catalog(const Menu& menu,const fs::path& app) {
    return oe::persistence::RecoveryStore(app,menu.settings().data_root,
        oe::persistence::recovery_storage_budget_bytes,
        oe::persistence::RecoveryAccess::OriginalProduction).catalog();
}
void check_original_recovery(const Menu& menu,const fs::path& app) {
    check(menu.settings().autosave_enabled,"Original mode overwrote the legacy autosave preference");
    check(menu.message().find("Autosave ON")!=std::string::npos,
        "Original mode did not explain its enabled recovery");
    const auto catalog=recovery_catalog(menu,app);
    const auto found=std::find_if(catalog.histories.begin(),catalog.histories.end(),[&](const auto& h){
        return h.history_id==menu.recovery_history_id();
    });
    check(found!=catalog.histories.end() && found->error.empty() && !found->entries.empty(),
        "Original mode did not publish its valid active recovery history");
    check(std::count_if(found->entries.begin(),found->entries.end(),[](const auto& e){return e.start_point;})==1,
        "Active Original recovery did not retain exactly one protected start point");
    for(const auto& e:found->entries)check(e.loadable() && e.rule_version==10 && e.schema==26,
        "Active Original history published an invalid or wrong-generation checkpoint");
}
void external_load(Menu& menu,Dialog& dialog,SDL_Window* window,SDL_Renderer* renderer,const fs::path& save) {
    if (menu.state()==Menu::State::Playing) key(menu,SDLK_ESCAPE);
    if (menu.state()==Menu::State::MainMenu) key(menu,SDLK_L);
    check(menu.state()==Menu::State::LoadSandbox,"Manual load screen unavailable");
    click(menu,window,renderer,"Open save file...");dialog.answer(save);menu.advance();finish_load(menu);
}
void seed_preferences(const fs::path& app,const fs::path& data) {
    oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/A.map";
    oe::menu::write_settings(app,settings);
}
void expect_legacy_preferences(const Menu& menu) {
    check(menu.settings().profile==sim::RulesProfile::CityV11 && menu.settings().prepared_starter &&
        menu.settings().autosave_enabled,"Original session changed the legacy profile/starter/autosave defaults");
}
void check_bounds(const Menu& menu,const std::string& label,float width=600,float height=400) {
    const auto r=menu.diagnostic_button_rect(label);
    check(r && r->x>=0 && r->y>=0 && r->x+r->w<=width && r->y+r->h<=height-42,
        "Original control exceeds the actual menu aperture or footer");
}
std::map<fs::path,std::string> recovery_files(const fs::path& root) {
    std::map<fs::path,std::string> result;
    if (fs::exists(root)) for (const auto& entry:fs::recursive_directory_iterator(root))
        if (entry.is_regular_file()) result.emplace(entry.path().lexically_relative(root),oe_influence_fixture::read_text(entry.path()));
    return result;
}
void capture_failure_recovery(SDL_Window* window,SDL_Renderer* renderer) {
    oe::testing::economy::Temp source;const auto data=source.root/"data",app=source.root/"capture-app";
    oe_production_fixture::write_model(data);seed_preferences(app,data);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    enter_original(menu,window,renderer);start(menu);key(menu,SDLK_F5);
    const auto manual=menu.settings().last_save;const auto manual_bytes=oe_influence_fixture::read_text(manual);
    const auto saved=oe::persistence::read_save(manual);
    const auto model=data/"Model/EmperorBuildingModels.txt",held=source.root/"held-private-model.txt";
    while (menu.sandbox()->world().ticks()<1250) menu.sandbox()->tick_once();
    const auto before=menu.sandbox()->world().snapshot();const auto id=menu.recovery_history_id();
    const auto previous=recovery_files(app/"recovery");fs::rename(model,held);
    menu.update(0);
    check(menu.message().find("Autosave failed")!=std::string::npos &&
          menu.sandbox()->hud_status_line().find("Autosave failed")!=std::string::npos,
          "Original capture failure did not produce a visible optional-recovery error");
    check(menu.sandbox()->world().snapshot()==before && menu.recovery_history_id()==id &&
          recovery_files(app/"recovery")==previous && oe_influence_fixture::read_text(manual)==manual_bytes,
          "Failed Original capture changed World, history, manual save or checkpoint bytes");
    oe::performance::set_enabled(true);oe::performance::reset();
    for (int i=0;i<100;++i) {menu.update(0);menu.advance();}
    for (std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
        check(oe::performance::counter(static_cast<oe::performance::Counter>(i))==0,
              "Original capture failure retried source work on idle frames");
    oe::performance::set_enabled(false);
    fs::rename(held,model);
    while (menu.sandbox()->world().ticks()<2449) menu.sandbox()->tick_once();
    menu.update(0);check(recovery_files(app/"recovery")==previous,"Original capture failure did not retain its1200t backoff");
    menu.sandbox()->tick_once();menu.update(0);
    const auto recovered=recovery_catalog(menu,app);
    const auto found=std::find_if(recovered.histories.begin(),recovered.histories.end(),[&](const auto& h){return h.history_id==id;});
    check(found!=recovered.histories.end() && found->entries.size()==2 && found->entries.front().tick==2450 &&
          found->entries.front().loadable() && menu.recovery_history_id()==id,
          "Original autosave did not resume its existing history at the next normal deadline");
    // F9 restores with the source available; only the optional Recovery begin loses it.
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_F9;route(menu,event);
    check(menu.sandbox()->world().snapshot()==saved.world,"Actual F9 did not restore before the Recovery begin probe");
    const auto restored=menu.sandbox()->world().snapshot();const auto before_begin=recovery_files(app/"recovery");
    fs::rename(model,held);menu.advance();
    check(menu.state()==Menu::State::Playing && menu.sandbox()->world().snapshot()==restored &&
          menu.recovery_history_id().empty() && recovery_files(app/"recovery")==before_begin &&
          menu.message().find("Autosave failed")!=std::string::npos && oe_influence_fixture::read_text(manual)==manual_bytes,
          "F9 Recovery-capture failure escaped, changed restored authority or damaged earlier sources");
    for (int i=0;i<10;++i) {menu.update(0);menu.advance();}
    key(menu,SDLK_ESCAPE);click(menu,window,renderer,"Autosave: ON");
    check(!menu.settings().autosave_enabled && menu.recovery_history_id().empty(),
          "Original autosave OFF attempted a source capture");
    click(menu,window,renderer,"Autosave: OFF");
    check(menu.settings().autosave_enabled && menu.recovery_history_id().empty() &&
          menu.message().find("Autosave failed")!=std::string::npos &&
          menu.sandbox()->hud_status_line().find("Autosave failed")!=std::string::npos &&
          menu.sandbox()->world().snapshot()==restored && recovery_files(app/"recovery")==before_begin,
          "Original Toggle ON hid its capture failure or changed prior authority/files");
    fs::rename(held,model);click(menu,window,renderer,"Autosave: ON");click(menu,window,renderer,"Autosave: OFF");
    check(!menu.recovery_history_id().empty(),"Original recovery could not be explicitly restarted after source return");
    // A real dirty-city confirmation also separates successful candidate creation from Recovery begin.
    menu.sandbox()->tick_once();enter_original(menu,window,renderer);start(menu);
    check(menu.state()==Menu::State::ConfirmLeave,"Capture-begin candidate probe lacks actual dirty-city confirmation");
    const auto before_commit=recovery_files(app/"recovery");fs::rename(model,held);
    click(menu,window,renderer,"Discard Changes");
    check(menu.state()==Menu::State::Playing && menu.sandbox()->world().ticks()==0 && house_count(menu.sandbox()->world())==4 &&
          menu.recovery_history_id().empty() && menu.message().find("Autosave failed")!=std::string::npos &&
          recovery_files(app/"recovery")==before_commit && oe_influence_fixture::read_text(manual)==manual_bytes,
          "Optional Recovery capture failure prevented candidate commit or changed prior files");
    fs::rename(held,model);menu.shutdown();
}
void fire_protection_menu(SDL_Window* window,SDL_Renderer* renderer,bool sustainable=false,bool logistics=false,bool cardinal=false,bool coordination=false,bool deadline=false,bool service_deadline=false,float width=600,float height=400,bool food_deadline=false) {
    const auto rule=food_deadline ? 18U:service_deadline ? 17U:deadline ? 16U:coordination ? 15U:cardinal ? 14U:logistics ? 13U:sustainable ? 12U:11U,
        schema=food_deadline ? 34U:service_deadline ? 33U:deadline ? 32U:coordination ? 31U:cardinal ? 30U:logistics ? 29U:sustainable ? 28U:27U;
    const auto option=food_deadline ? "U Deadline-aware food":service_deadline ? "T Deadline-aware services":deadline ? "D Deadline-aware fire dispatch":coordination ? "I Inspector coordination":cardinal ? "W Cardinal-water economy":logistics ? "M Needs-aware logistics":sustainable ? "S Sustainable economy":"F Original fire protection";
    // Independently authored map/model: normal MenuSession and production View,
    // ordinary paid World commands and actual SDL F5/F9. Real Xia acceptance
    // remains a separate original-data run.
    oe::testing::economy::Temp source;const auto data=source.root/"data",app=source.root/"fire-app";
    oe_production_fixture::write_model(data);seed_preferences(app,data);
    auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
    Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);
    expect_legacy_preferences(menu);
    check(!menu.original_setup() && !menu.original_fire_protection_setup() && !menu.original_sustainable_economy_setup() &&
        !menu.original_market_logistics_setup() && !menu.original_cardinal_water_setup() &&
        !menu.original_inspector_coordination_setup() && !menu.original_deadline_fire_dispatch_setup() &&
        !menu.original_deadline_service_dispatch_setup()&&!menu.original_deadline_food_dispatch_setup(),"Original successor became a fresh-menu default");
    open_legacy(menu,window,renderer);
    const auto bounds=[&](const std::string& label){check_bounds(menu,label,width,height);};
    bounds(option);
    bounds("M Needs-aware logistics");bounds("Autosave: ON");
    const auto logistics_rect=menu.diagnostic_button_rect("M Needs-aware logistics"),autosave_rect=menu.diagnostic_button_rect("Autosave: ON");
    check(logistics_rect && autosave_rect && !SDL_HasRectIntersectionFloat(&*logistics_rect,&*autosave_rect),
        "Explicit logistics entry overlaps Autosave at minimum window size");
    bounds("W Cardinal-water economy");
    const auto water_rect=menu.diagnostic_button_rect("W Cardinal-water economy");
    check(water_rect && autosave_rect && logistics_rect &&
        !SDL_HasRectIntersectionFloat(&*water_rect,&*autosave_rect) &&
        !SDL_HasRectIntersectionFloat(&*water_rect,&*logistics_rect),"Rule14 entry overlaps another menu action");
    bounds("I Inspector coordination");
    const auto inspector_rect=menu.diagnostic_button_rect("I Inspector coordination");
    for(const auto& label:{"O Original economy","F Original fire protection","S Sustainable economy",
                          "M Needs-aware logistics","W Cardinal-water economy","D Deadline-aware fire dispatch","Autosave: ON"}) {
        const auto other=menu.diagnostic_button_rect(label);
        check(inspector_rect && other && !SDL_HasRectIntersectionFloat(&*inspector_rect,&*other),
            "Rule15 entry overlaps an existing menu action");
    }
    bounds("D Deadline-aware fire dispatch");
    bounds("T Deadline-aware services");
    const auto service_rect=menu.diagnostic_button_rect("T Deadline-aware services");
    for(const auto& label:{"D Deadline-aware fire dispatch","O Original economy","F Original fire protection",
                          "S Sustainable economy","M Needs-aware logistics","W Cardinal-water economy","Autosave: ON"}) {
        const auto other=menu.diagnostic_button_rect(label);
        check(service_rect&&other&&!SDL_HasRectIntersectionFloat(&*service_rect,&*other),
            "Rule17 entry overlaps another action at minimum window size");
    }
    bounds("U Deadline-aware food");
    const auto food_rect=menu.diagnostic_button_rect("U Deadline-aware food");
    for(const auto& label:{"N New Sandbox","L Load Save","Original Data Folder","Quit",
                          "I Inspector coordination","T Deadline-aware services","Autosave: ON"}) {
        const auto other=menu.diagnostic_button_rect(label);
        check(food_rect&&other&&!SDL_HasRectIntersectionFloat(&*food_rect,&*other),
            "Rule18 Food entry overlaps another actual minimum-window action");
    }
    if(food_deadline) {
        key(menu,SDLK_U);wait_check(menu);
        check(menu.original_deadline_food_dispatch_setup()&&menu.map_rules_result().request.rule_version==18,
            "U key did not select the explicit Rule18 preflight");
        key(menu,SDLK_ESCAPE);
        check(menu.state()==Menu::State::MainMenu,"Rule18 setup did not return to the main menu");
        open_legacy(menu,window,renderer);click(menu,window,renderer,option);
    } else if(service_deadline) {
        key(menu,SDLK_T);wait_check(menu);
        check(menu.original_deadline_service_dispatch_setup()&&menu.map_rules_result().request.rule_version==17,
            "T key did not select the explicit Rule17 preflight");
        key(menu,SDLK_ESCAPE);
        check(menu.state()==Menu::State::MainMenu,"Rule17 setup did not return to the main menu");
        open_legacy(menu,window,renderer);click(menu,window,renderer,option);
    } else if(deadline) {
        key(menu,SDLK_D);wait_check(menu);
        check(menu.original_deadline_fire_dispatch_setup() && menu.map_rules_result().request.rule_version==16,
            "D key did not select the explicit Rule16 preflight");
        key(menu,SDLK_ESCAPE);
        check(menu.state()==Menu::State::MainMenu,"Rule16 setup did not return to the main menu");
        open_legacy(menu,window,renderer);click(menu,window,renderer,option);
    } else if(coordination) {
        key(menu,SDLK_I);wait_check(menu);
        check(menu.original_inspector_coordination_setup() && menu.map_rules_result().request.rule_version==15,
            "I key did not select the explicit Rule15 preflight");
        key(menu,SDLK_ESCAPE);
        check(menu.state()==Menu::State::MainMenu,"Rule15 setup did not return to the main menu");
        open_legacy(menu,window,renderer);click(menu,window,renderer,option); // Mouse and keyboard select the same action.
    } else if(cardinal)key(menu,SDLK_W);else if(logistics)key(menu,SDLK_M);else click(menu,window,renderer,option);
    wait_check(menu);
    check(menu.original_setup() && menu.original_fire_protection_setup()==(!sustainable&&!logistics&&!cardinal&&!coordination&&!deadline&&!service_deadline&&!food_deadline) &&
        menu.original_sustainable_economy_setup()==sustainable &&
        menu.original_market_logistics_setup()==logistics && menu.original_cardinal_water_setup()==cardinal &&
        menu.original_inspector_coordination_setup()==coordination && menu.original_deadline_fire_dispatch_setup()==deadline &&
        menu.original_deadline_service_dispatch_setup()==service_deadline &&
        menu.original_deadline_food_dispatch_setup()==food_deadline &&
        menu.map_rules_result().request.rule_version==rule && menu.map_rules_result().request.policy_version==3,
        "Explicit Rule11 menu option did not preflight its own policy3 contract");
    key(menu,SDLK_RETURN);finish_load(menu);
    check(menu.state()==Menu::State::Playing && menu.sandbox()->world().original_fire_protection_enabled() &&
        menu.sandbox()->world().rule_version()==rule && menu.sandbox()->paused() && house_count(menu.sandbox()->world())==4,
        "Rule11 menu did not buy the actual four-house plan and publish it paused");
    auto& view=*menu.sandbox();
    check(view.world().original_inspector_coordination_enabled()==coordination &&
        view.world().original_deadline_fire_dispatch_enabled()==deadline &&
        view.world().original_deadline_service_dispatch_enabled()==service_deadline &&
        view.world().original_deadline_food_dispatch_enabled()==food_deadline,
        "Menu session changed the exact Inspector coordination opt-in");
    const auto tool=std::find_if(view.layout().buttons.begin(),view.layout().buttons.end(),
        [](const auto& button){return button.action==oe::sandbox_ui::Action::FireWatch;});
    check(tool!=view.layout().buttons.end(),"Rule11 ordinary toolbar has no FireWatch action");
    key(menu,SDLK_F);check(view.tool()==11,"Rule11 F key did not choose the ordinary FireWatch tool");
    const auto first=std::find_if(view.world().buildings().begin(),view.world().buildings().end(),
        [](const auto& b){return sim::original_house_kind(b.kind);});
    check(first!=view.world().buildings().end(),"Paid starter has no actual House anchor");
    const sim::Cell watch_cell{first->cell.x+13,first->cell.y+1};
    const auto funds=view.world().treasury();
    const auto purchased=view.execute({sim::CommandType::PlaceFireWatch,watch_cell});
    check(purchased.accepted && purchased.changed && view.world().treasury()==funds-80,
        "Rule11 View did not pay the actual authored80-Funds Watch price");
    const auto watch=*view.world().building_owner_at(watch_cell);
    check(view.world().workforce_required(watch)==2,"Rule11 Watch does not require its authored2 workers");
    const auto check_watch_invoice=[&] {
        check(view.selected_building()==watch,"Paid Watch lost its actual inspection selection");
        std::string text;for(const auto& line:view.inspection_lines())text+=line+" ";
        const auto interval=sustainable||logistics||cardinal||coordination||deadline||service_deadline||food_deadline ? 816U:400U;
        const auto due=view.world().maintenance_due_in(watch);
        check(due && text.find("Maintenance: 4 every "+std::to_string(interval)+" ticks")!=std::string::npos &&
            text.find("Next due: "+std::to_string(*due ? *due:interval)+" ticks")!=std::string::npos,
            "Watch inspection misreported its selected rule's actual installed invoice or next due time");
    };
    check_watch_invoice();
    bool moving=false;
    for (int i=0;i<1500 && !moving;++i) {
        view.tick_once();
        moving=std::any_of(view.world().couriers().begin(),view.world().couriers().end(),[](const auto& c){
            return c.role==sim::CourierRole::FireInspector && c.phase!=sim::CourierPhase::IdleAtWorkshop && c.edge_progress>0;
        });
    }
    check(moving,"Rule11 menu city did not earn an actual staffed Inspector trip");
    check_watch_invoice();
    key(menu,SDLK_F5);const auto manual=menu.settings().last_save;
    const auto saved=view.capture_save_document();const auto bytes=oe_influence_fixture::read_text(manual);
    check(oe::persistence::read_save(manual).source_schema_version==schema,"Rule11 F5 wrote another save schema");
    const auto json=nlohmann::json::parse(bytes);
    check(json.at("rules").contains("inspector_coordination")==coordination,
        "F5 added or omitted the exact Rule15 contract marker");
    if(coordination)check(json.at("rules").at("inspector_coordination")==sim::original_inspector_coordination_contract,
        "F5 wrote another Inspector coordination contract");
    check(json.at("rules").contains("deadline_fire_dispatch")==bool(deadline||service_deadline||food_deadline),
        "F5 added or omitted the exact Rule16 marker");
    if(deadline||service_deadline||food_deadline)check(json.at("rules").at("deadline_fire_dispatch")==sim::original_deadline_fire_dispatch_contract,
        "F5 wrote another deadline fire-dispatch contract");
    check(json.at("rules").contains("deadline_service_dispatch")==bool(service_deadline||food_deadline),
        "F5 added or omitted the exact Rule17 marker");
    if(service_deadline||food_deadline)check(json.at("rules").at("deadline_service_dispatch")==sim::original_deadline_service_dispatch_contract,
        "F5 wrote another deadline service-dispatch contract");
    check(json.at("rules").contains("deadline_food_dispatch")==food_deadline,
        "F5 added or omitted the exact Rule18 marker");
    if(food_deadline)check(json.at("rules").at("deadline_food_dispatch")==sim::original_deadline_food_dispatch_contract,
        "F5 wrote another deadline Food-dispatch contract");
    for (int i=0;i<20;++i) view.tick_once();
    key(menu,SDLK_F9);same(saved,view.capture_save_document());
    check(oe_influence_fixture::read_text(manual)==bytes,"Rule11 midflight F9 changed its input save bytes");
    while (view.world().ticks()<saved.world.ticks+1250) view.tick_once();
    menu.update(0);
    const auto catalog=recovery_catalog(menu,app);
    const auto history=std::find_if(catalog.histories.begin(),catalog.histories.end(),[&](const auto& h){
        return h.history_id==menu.recovery_history_id();
    });
    check(history!=catalog.histories.end() && history->error.empty() && history->entries.size()>=2,
        "Rule11 autosave did not retain a protected start and due checkpoint");
    for (const auto& e:history->entries) check(e.loadable() && e.schema==schema && e.rule_version==rule,
        "Rule11 recovery history is not a valid exact schema27/rule11 tuple");
    const auto checkpoint=history->entries.front();const auto checkpoint_bytes=oe_influence_fixture::read_text(checkpoint.path);
    key(menu,SDLK_F5); // Resolve real dirty progress before the independent recovery load.
    external_load(menu,*callbacks,window,renderer,checkpoint.path);
    check(menu.state()==Menu::State::Playing && menu.sandbox()->world().rule_version()==rule,
        "Normal file chooser did not restore Rule11 recovery as Rule11");
    same(oe::persistence::read_save(checkpoint.path),menu.sandbox()->capture_save_document());
    key(menu,SDLK_F5);
    check(menu.settings().last_save!=checkpoint.path && oe_influence_fixture::read_text(checkpoint.path)==checkpoint_bytes,
        "Rule11 F5 overwrote an owned recovery checkpoint");
    expect_legacy_preferences(menu);menu.shutdown();

    auto fresh_dialog=std::make_unique<Dialog>();auto* fresh_callbacks=fresh_dialog.get();
    Menu fresh({},app,std::move(fresh_dialog));fresh.initialize(window,renderer);
    check(!fresh.original_setup() && !fresh.original_fire_protection_setup() && !fresh.original_sustainable_economy_setup() &&
        !fresh.original_market_logistics_setup() && !fresh.original_cardinal_water_setup() &&
        !fresh.original_inspector_coordination_setup() && !fresh.original_deadline_fire_dispatch_setup() &&
        !fresh.original_deadline_service_dispatch_setup()&&!fresh.original_deadline_food_dispatch_setup(),"Original successor selection persisted as a new-game default");
    external_load(fresh,*fresh_callbacks,window,renderer,manual);
    check(fresh.state()==Menu::State::Playing && fresh.sandbox()->world().rule_version()==rule &&
        house_count(fresh.sandbox()->world())==4,"Restart/reload changed Rule11 or repeated the paid starter");
    key(fresh,SDLK_ESCAPE);enter_original(fresh,window,renderer);wait_map(fresh);
    check(!fresh.original_fire_protection_setup(),"Old O Original economy menu inherited Rule11");
    key(fresh,SDLK_RETURN);finish_load(fresh);
    check(fresh.state()==Menu::State::Playing && fresh.sandbox()->world().rule_version()==10,
        "Existing Original menu changed from Rule10");
    fresh.sandbox()->set_tool(11);check(fresh.sandbox()->tool()!=11,"Rule10 UI now exposes FireWatch");
    const auto& current=fresh.sandbox()->world();const auto before=current.snapshot();
    check(!fresh.sandbox()->execute({sim::CommandType::PlaceFireWatch,watch_cell}).accepted && current.snapshot()==before,
        "Rule10 stopped atomically refusing FireWatch after a Rule11 session");
    fresh.shutdown();
}

void process_write(const fs::path& root,SDL_Window* window,SDL_Renderer* renderer) {
    check(!fs::exists(root),"Process-save evidence root must be fresh");
    oe::testing::economy::Temp source;fs::create_directories(root);
    fs::copy(source.root/"data",root/"data",fs::copy_options::recursive);
    const auto data=fs::canonical(root/"data"),app=root/"app";
    oe_production_fixture::write_model(data);seed_preferences(app,data);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    enter_original(menu,window,renderer);key(menu,SDLK_D);key(menu,SDLK_D);start(menu);
    check(menu.state()==Menu::State::Playing && house_count(menu.sandbox()->world())==4,
        "Process writer did not buy the actual four-house starter through the menu");
    for (int i=0;i<400;++i) menu.sandbox()->tick_once();
    key(menu,SDLK_F5);const auto saved=oe::persistence::read_save(menu.settings().last_save);
    same(saved,menu.sandbox()->capture_save_document());expect_legacy_preferences(menu);check_original_recovery(menu,app);
    check(saved.world.original_economy->model_source->difficulty==orig::Difficulty::VeryHard,
        "Process writer lost its explicitly chosen difficulty");
    std::cout<<"Actual menu process saved tick "<<saved.world.ticks<<" at "<<menu.settings().last_save<<'\n';
    menu.shutdown();
}
void process_load(const fs::path& root,SDL_Window* window,SDL_Renderer* renderer) {
    const auto app=root/"app";const auto settings=oe::menu::read_settings(app).value;
    const auto save=settings.last_save;const auto bytes=oe_influence_fixture::read_text(save);
    const auto saved=oe::persistence::read_save(save);
    auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
    Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);
    check(!menu.original_setup() && menu.original_difficulty()==orig::Difficulty::Normal,
        "Fresh process inherited a saved Original setup selection");
    external_load(menu,*callbacks,window,renderer,save);
    check(menu.state()==Menu::State::Playing && menu.sandbox()->paused(),"New process did not open its manual Original save paused");
    same(saved,menu.sandbox()->capture_save_document());expect_legacy_preferences(menu);check_original_recovery(menu,app);
    check(menu.sandbox()->world().original_rules_model()->difficulty()==orig::Difficulty::VeryHard &&
        house_count(menu.sandbox()->world())==4,"Restart changed saved difficulty or repeated/lost starter purchases");
    for (int i=0;i<20;++i) menu.sandbox()->tick_once();
    key(menu,SDLK_F9);same(saved,menu.sandbox()->capture_save_document());
    check(oe_influence_fixture::read_text(save)==bytes,"New-process restore/F9 changed its supplied save bytes");
    std::cout<<"Actual menu new-process restore and F9 matched full tick "<<saved.world.ticks<<" SaveDocument\n";
    menu.shutdown();
}

constexpr std::array<std::pair<const char*,SDL_Keycode>,10> historical_entries{{
    {"N New Sandbox",SDLK_N},{"O Original economy",SDLK_O},
    {"F Original fire protection",SDLK_F},{"S Sustainable economy",SDLK_S},
    {"M Needs-aware logistics",SDLK_M},{"W Cardinal-water economy",SDLK_W},
    {"I Inspector coordination",SDLK_I},{"D Deadline-aware fire dispatch",SDLK_D},
    {"T Deadline-aware services",SDLK_T},{"U Deadline-aware food",SDLK_U}
}};
void expect_compact_main(Menu& menu) {
    check(menu.state()==Menu::State::MainMenu && !menu.original_setup(),
        "Opening the main menu silently selected an Original rule");
    check(menu.diagnostic_button_rect("Play Original economy").has_value() &&
        menu.diagnostic_button_rect("Advanced / Legacy").has_value(),
        "Main menu lacks its recommended entry or explicit historical choices");
    for (const auto& [label,shortcut]:historical_entries) {
        (void)shortcut;
        check(!menu.diagnostic_button_rect(label),"Historical generation leaked into the compact main menu");
    }
    check(!menu.diagnostic_button_rect("Enter Continue"),"Fresh menu suggests Continue without an active city");
    expect_legacy_preferences(menu);
}
void enter_recommended(Menu& menu,SDL_Window* window,SDL_Renderer* renderer) {
    click(menu,window,renderer,"Play Original economy");wait_check(menu);
    const auto& selected=menu.map_rules_result().request;
    check(menu.state()==Menu::State::NewSandbox && menu.original_setup() &&
        menu.original_deadline_food_dispatch_setup() && selected.rule_version==18 &&
        selected.profile==sim::RulesProfile::CityV16 && selected.policy_version==3,
        "Confirmed recommended entry did not select the existing Rule18/Policy3 contract");
    check(menu.original_difficulty()==orig::Difficulty::Normal && menu.original_paid_starter(),
        "Fresh recommended setup changed Normal difficulty or paid-start defaults");
    expect_legacy_preferences(menu);
}
void recommended_paid_start(Menu& menu) {
    key(menu,SDLK_RETURN);finish_load(menu);
    check(menu.state()==Menu::State::Playing && menu.sandbox() && menu.sandbox()->paused(),
        "Recommended menu did not publish a paused real World");
    const auto& world=menu.sandbox()->world();
    check(world.rule_version()==18 && world.profile()==sim::RulesProfile::CityV16 &&
        world.original_deadline_food_dispatch_enabled() && world.ticks()==0 &&
        world.total_population()==0 && house_count(world)==4 && world.construction_spent_total()>0 &&
        world.construction_spent_total()<=12000 &&
        world.treasury()==12000-static_cast<std::int64_t>(world.construction_spent_total()) &&
        world.original_rules_model()->difficulty()==orig::Difficulty::Normal,
        "Recommended start did not buy the actual four-house Rule18 city from its12000-Funds budget");
}
void player_mode_menu(SDL_Window*& window,SDL_Renderer*& renderer,const fs::path& captures) {
    oe::testing::economy::Temp source;const auto data=source.root/"data";
    oe_production_fixture::write_model(data);
    constexpr std::array<std::pair<int,int>,3> sizes{{{1280,800},{1600,900},{1920,1080}}};
    for (const auto& [width,height]:sizes) {
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;
        check(SDL_CreateWindowAndRenderer("Player mode menu",width,height,SDL_WINDOW_RESIZABLE,&window,&renderer),
            "Create requested real SDL menu aperture");
        const auto name=std::to_string(width)+"x"+std::to_string(height);
        const auto app=source.root/("player-"+name);seed_preferences(app,data);
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);expect_compact_main(menu);
        const float logical_width=static_cast<float>(width)/1.5F,logical_height=static_cast<float>(height)/1.5F;
        check_bounds(menu,"Play Original economy",logical_width,logical_height);
        check_bounds(menu,"Advanced / Legacy",logical_width,logical_height);
        capture(menu,renderer,captures,("player.main."+name).c_str());
        // Every focus/layout boundary cancels the explicit Rule18 selection.
        constexpr std::array<Uint32,8> boundaries{SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_WINDOW_MOUSE_LEAVE,
            SDL_EVENT_WINDOW_HIDDEN,SDL_EVENT_WINDOW_MINIMIZED,SDL_EVENT_WINDOW_RESIZED,
            SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,SDL_EVENT_WINDOW_DISPLAY_CHANGED,SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED};
        for (const auto type:boundaries) {
            const auto p=button_point(menu,window,renderer,"Play Original economy");mouse(menu,p,true);
            SDL_Event event{};event.type=type;route(menu,event);mouse(menu,p,false);menu.advance();
            check(menu.state()==Menu::State::MainMenu && !menu.original_setup() &&
                menu.diagnostic_pressed_action()==-1 && menu.diagnostic_pending_action()==-1,
                "Canceled recommended press selected a rule or retained a pending action");
        }
        open_legacy(menu,window,renderer);
        std::vector<SDL_FRect> controls;
        for (const auto& [label,shortcut]:historical_entries) {
            (void)shortcut;check_bounds(menu,label,logical_width,logical_height);
            const auto rect=menu.diagnostic_button_rect(label);check(rect.has_value(),"Advanced menu lost a historical generation");
            for (const auto& other:controls)
                check(!SDL_HasRectIntersectionFloat(&*rect,&other),"Historical menu hitboxes overlap");
            controls.push_back(*rect);
        }
        capture(menu,renderer,captures,("player.legacy."+name).c_str());
        key(menu,SDLK_ESCAPE);expect_compact_main(menu);
        enter_recommended(menu,window,renderer);
        for (const auto* label:{"Previous map","Next map","D Difficulty: Normal","P Paid four-house starter",
                               "Start sandbox","Back","Check again"})
            check_bounds(menu,label,logical_width,logical_height);
        capture(menu,renderer,captures,("player.setup."+name).c_str());
        for (std::size_t next=0;next<orig::difficulty_count;++next) {
            if (next%2==0) key(menu,SDLK_D);
            else {
                constexpr const char* names[]={"Very Easy","Easy","Normal","Hard","Very Hard"};
                click(menu,window,renderer,std::string("D Difficulty: ")+
                    names[static_cast<std::size_t>(menu.original_difficulty())]);
            }
            check(static_cast<std::size_t>(menu.original_difficulty())==(3+next)%orig::difficulty_count,
                "Recommended difficulty click/key did not select the actual next difficulty");
        }
        menu.shutdown();
    }
    // Shortcuts remain reachable without expanding historical controls. Each
    // selection performs the real map preflight, including unchanged N Rule3.
    for (std::size_t i=0;i<historical_entries.size();++i) {
        const auto app=source.root/("shortcut-"+std::to_string(i));seed_preferences(app,data);
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);expect_compact_main(menu);
        key(menu,historical_entries[i].second);wait_check(menu);
        const auto request=menu.map_rules_result().request;
        const auto expected_rule=i==0 ? 3U:static_cast<unsigned>(i)+9U;
        check(menu.state()==Menu::State::NewSandbox && request.rule_version==expected_rule &&
            request.profile==(i==0 ? sim::RulesProfile::CityV11:sim::RulesProfile::CityV16) &&
            request.policy_version==(i==0 ? 0U:3U) && menu.original_setup()==(i!=0),
            "Collapsed-menu historical shortcut changed its rule/profile/policy contract");
        menu.shutdown();
        Menu clicked({},source.root/("clicked-"+std::to_string(i)),std::make_unique<Dialog>());
        // Give the independent click case the same historical default preferences.
        seed_preferences(clicked.app_root(),data);clicked.initialize(window,renderer);
        open_legacy(clicked,window,renderer);click(clicked,window,renderer,historical_entries[i].first);
        wait_check(clicked);
        check(clicked.map_rules_result().request==request,
            "Historical button and shortcut selected different map/rule/profile/policy identities");
        clicked.shutdown();
    }
    for (const bool paid:{true,false}) {
        const auto app=source.root/(paid ? "recommended-paid":"recommended-empty");seed_preferences(app,data);
        auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
        Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);enter_recommended(menu,window,renderer);
        click(menu,window,renderer,"Next map");wait_check(menu);
        check(menu.map_rules_result().request.map_relative==fs::path("Cities/B.map"),"Recommended map selection did not select B");
        click(menu,window,renderer,"Previous map");wait_check(menu);
        check(menu.map_rules_result().request.map_relative==fs::path("Cities/A.map"),"Recommended map selection did not return to A");
        if (paid) recommended_paid_start(menu);
        else {
            click(menu,window,renderer,"P Paid four-house starter");
            check(!menu.original_paid_starter(),"Recommended Empty choice did not change the explicit start type");
            key(menu,SDLK_RETURN);finish_load(menu);
            const auto& world=menu.sandbox()->world();
            check(menu.state()==Menu::State::Playing && menu.sandbox()->paused() && world.rule_version()==18 &&
                world.ticks()==0 && world.treasury()==12000 && world.construction_spent_total()==0 &&
                world.buildings().empty() && world.total_population()==0,
                "Recommended Empty city bought a starter or changed initial funds/rule/ticks");
        }
        expect_legacy_preferences(menu);key(menu,SDLK_F5);
        const auto manual=menu.settings().last_save;
        const auto bytes=oe_influence_fixture::read_text(manual);
        const auto saved=oe::persistence::read_save(manual);same(saved,menu.sandbox()->capture_save_document());
        check(saved.source_schema_version==34 && saved.world.rule_version==18 &&
            saved.map_permissions_policy_version==3 && saved.map_relative==fs::path("Cities/A.map") &&
            !saved.map_sha256.empty() && saved.world.original_economy->model_source->difficulty==orig::Difficulty::Normal,
            "Recommended F5 changed the existing Rule18/Schema34/source/difficulty identity");
        auto json=nlohmann::json::parse(bytes);
        check(json.at("rules").at("deadline_food_dispatch")==sim::original_deadline_food_dispatch_contract,
            "Recommended save omitted the existing Rule18 contract marker");
        for (int tick=0;tick<20;++tick) menu.sandbox()->tick_once();
        key(menu,SDLK_F9);same(saved,menu.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"Recommended F9 rewrote its source save");
        const auto retained=menu.sandbox()->world().snapshot();
        json.at("rules").erase("deadline_food_dispatch");
        const auto missing_marker=source.root/(paid ? "paid-missing-marker.json":"empty-missing-marker.json");
        oe_influence_fixture::write_text(missing_marker,json.dump());
        external_load(menu,*callbacks,window,renderer,missing_marker);
        check(menu.state()==Menu::State::LoadSandbox && menu.sandbox()->world().snapshot()==retained &&
            oe_influence_fixture::read_text(manual)==bytes,
            "A Rule18 save without its marker replaced the active city or changed historical bytes");
        const auto truncated=source.root/(paid ? "paid-truncated.json":"empty-truncated.json");
        oe_influence_fixture::write_text(truncated,bytes.substr(0,bytes.size()/2));
        external_load(menu,*callbacks,window,renderer,truncated);
        check(menu.state()==Menu::State::LoadSandbox && menu.sandbox()->world().snapshot()==retained &&
            oe_influence_fixture::read_text(manual)==bytes,
            "Truncated recommended save replaced the active city or changed historical bytes");
        menu.shutdown();
        // Ordinary load in a fresh session preserves the saved generation and
        // never repeats paid construction. Separate-process coverage is below.
        auto fresh_dialog=std::make_unique<Dialog>();auto* fresh_callbacks=fresh_dialog.get();
        Menu fresh({},app,std::move(fresh_dialog));fresh.initialize(window,renderer);expect_compact_main(fresh);
        external_load(fresh,*fresh_callbacks,window,renderer,manual);
        check(fresh.state()==Menu::State::Playing && fresh.sandbox()->paused(),"Recommended save could not load after restart");
        same(saved,fresh.sandbox()->capture_save_document());
        check(oe_influence_fixture::read_text(manual)==bytes,"Restart load rewrote the Rule18 save");fresh.shutdown();
    }
}
void player_mode_process_write(const fs::path& root,SDL_Window* window,SDL_Renderer* renderer) {
    check(!fs::exists(root),"Recommended process-save evidence root must be fresh");
    oe::testing::economy::Temp source;fs::create_directories(root);
    fs::copy(source.root/"data",root/"data",fs::copy_options::recursive);
    const auto data=fs::canonical(root/"data"),app=root/"app";
    oe_production_fixture::write_model(data);seed_preferences(app,data);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    enter_recommended(menu,window,renderer);recommended_paid_start(menu);
    for (int tick=0;tick<400;++tick) menu.sandbox()->tick_once();key(menu,SDLK_F5);
    const auto saved=oe::persistence::read_save(menu.settings().last_save);
    check(saved.source_schema_version==34 && saved.world.rule_version==18,"Recommended writer changed its save generation");
    same(saved,menu.sandbox()->capture_save_document());menu.shutdown();
}
void player_mode_process_load(const fs::path& root,SDL_Window* window,SDL_Renderer* renderer) {
    const auto app=root/"app",save=oe::menu::read_settings(app).value.last_save;
    const auto bytes=oe_influence_fixture::read_text(save);const auto saved=oe::persistence::read_save(save);
    auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
    Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);expect_compact_main(menu);
    external_load(menu,*callbacks,window,renderer,save);
    check(menu.state()==Menu::State::Playing && menu.sandbox()->paused() && saved.source_schema_version==34 &&
        menu.sandbox()->world().rule_version()==18,"Recommended fresh process changed the saved generation");
    same(saved,menu.sandbox()->capture_save_document());
    for (int tick=0;tick<20;++tick) menu.sandbox()->tick_once();key(menu,SDLK_F9);
    same(saved,menu.sandbox()->capture_save_document());
    check(oe_influence_fixture::read_text(save)==bytes,"Recommended new-process load/F9 rewrote its source save");
    menu.shutdown();
}
void player_mode_real(const fs::path& data,const fs::path& output,SDL_Window* window,SDL_Renderer* renderer) {
    check(!fs::exists(output),"Real recommended evidence output must be fresh");
    const auto source=fs::canonical(data),target=fs::weakly_canonical(output);
    const auto private_root=fs::weakly_canonical(fs::path(__FILE__).parent_path().parent_path()/".local/oe49");
    const auto relative=target.lexically_relative(private_root);
    check(!relative.empty() && relative!="." && *relative.begin()!="..",
        "Real recommended evidence must stay below ignored .local/oe49");
    const auto to_source=target.lexically_relative(source),from_source=source.lexically_relative(target);
    check(!to_source.empty() && *to_source.begin()==".." &&
        !from_source.empty() && *from_source.begin()=="..",
        "Real recommended output overlaps original files");
    const auto app=output/"app";seed_preferences(app,source);
    auto settings=oe::menu::read_settings(app).value;settings.last_map="Cities/Xia.map";
    oe::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    capture(menu,renderer,output/"captures","main");
    enter_recommended(menu,window,renderer);capture(menu,renderer,output/"captures","setup");
    recommended_paid_start(menu);
    check(menu.sandbox()->world().treasury()==10596 && menu.sandbox()->world().construction_spent_total()==1404,
        "Pinned Xia/Normal paid starter changed its actual budget");
    const auto document=menu.sandbox()->capture_save_document();key(menu,SDLK_F5);
    const auto bytes=oe_influence_fixture::read_text(menu.settings().last_save);
    for (int i=0;i<20;++i) menu.sandbox()->tick_once();key(menu,SDLK_F9);
    same(document,menu.sandbox()->capture_save_document());
    check(oe_influence_fixture::read_text(menu.settings().last_save)==bytes,"Real F9 changed manual source bytes");
    std::cout<<"Real recommended Xia/Normal Rule18 paid starter: tick0, Funds10596, construction1404, four Houses; F5/F9 complete equality\n";
    menu.shutdown();
}

void starter_availability(SDL_Window*& window,SDL_Renderer*& renderer,const fs::path& captures) {
    oe::testing::economy::Temp source;const auto data=source.root/"data";
    oe_production_fixture::write_model(data);
    // A valid authored map with alternating signed source heights, rather than
    // a name-based exception or an injected placement verdict.
    fs::remove(data/"Cities/Broken.map");
    oe::testing::economy::write(data/"Cities/C.map",oe::testing::economy::map_file(true,false));
    constexpr std::array<std::pair<int,int>,4> sizes{{{900,600},{1280,800},{1600,900},{1920,1080}}};
    for (const auto& [width,height]:sizes) {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);renderer=nullptr;window=nullptr;
        check(SDL_CreateWindowAndRenderer("Starter availability",width,height,SDL_WINDOW_RESIZABLE,&window,&renderer),
            "Create starter error test aperture");
        const auto name=std::to_string(width)+"x"+std::to_string(height);
        const auto app=source.root/("starter-"+name);seed_preferences(app,data);
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
        enter_recommended(menu,window,renderer);recommended_paid_start(menu);key(menu,SDLK_F5);
        const auto retained=menu.sandbox()->capture_save_document();
        key(menu,SDLK_ESCAPE);enter_recommended(menu,window,renderer);
        click(menu,window,renderer,"Next map");wait_check(menu);
        click(menu,window,renderer,"Next map");wait_check(menu);
        check(menu.map_rules_result().request.map_relative==fs::path("Cities/C.map"),
            "Select the actual no-layout authored map");
        const auto files=recovery_files(app);
        key(menu,SDLK_RETURN);finish_load(menu);
        check(menu.state()==Menu::State::StarterUnavailable && menu.original_paid_starter() &&
            menu.message().find("24x13")!=std::string::npos,
            "A passed map without a starter area lacks the distinct placement error");
        same(retained,menu.sandbox()->capture_save_document());
        check(recovery_files(app)==files,"Refused starter changed settings, manual save or Recovery files");
        check_bounds(menu,"P Use Empty city",static_cast<float>(width)/1.5F,static_cast<float>(height)/1.5F);
        check_bounds(menu,"Choose another map",static_cast<float>(width)/1.5F,static_cast<float>(height)/1.5F);
        capture(menu,renderer,captures,("starter.unavailable."+name).c_str());
        // UI press/release boundaries retain the previous city and cannot turn
        // a canceled error-dialog action into an Empty selection.
        constexpr std::array<Uint32,3> boundaries{SDL_EVENT_WINDOW_FOCUS_LOST,
            SDL_EVENT_WINDOW_RESIZED,SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED};
        for (const auto type:boundaries) {
            const auto p=button_point(menu,window,renderer,"P Use Empty city");mouse(menu,p,true);
            SDL_Event event{};event.type=type;route(menu,event);mouse(menu,p,false);menu.advance();
            check(menu.state()==Menu::State::StarterUnavailable && menu.original_paid_starter(),
                "Canceled error-dialog click silently selected Empty city");
        }
        key(menu,SDLK_RETURN);
        check(menu.state()==Menu::State::StarterUnavailable,
            "Enter silently accepted a different construction mode after failed placement");
        click(menu,window,renderer,"Choose another map");wait_check(menu);
        check(menu.map_rules_result().request.map_relative==fs::path("Cities/C.map") && menu.original_paid_starter(),
            "Returning to map selection silently switched the map or starter preference");
        key(menu,SDLK_RETURN);finish_load(menu);
        check(menu.state()==Menu::State::StarterUnavailable,"Repeated refusal lost the placement explanation");
        if (width==1280) key(menu,SDLK_P);
        else click(menu,window,renderer,"P Use Empty city");
        wait_check(menu);
        check(!menu.original_paid_starter() && menu.map_rules_result().request.map_relative==fs::path("Cities/C.map"),
            "Explicit Empty selection changed the chosen map");
        same(retained,menu.sandbox()->capture_save_document());
        key(menu,SDLK_RETURN);finish_load(menu);
        check(menu.state()==Menu::State::Playing && menu.sandbox()->paused() &&
            menu.sandbox()->world().ticks()==0 && menu.sandbox()->world().treasury()==12000 &&
            menu.sandbox()->world().buildings().empty() && menu.sandbox()->world().rule_version()==18,
            "Explicit Empty start bypassed normal preparation or bought a starter");
        menu.shutdown();
    }
    // Empty is a layout choice only; an unsupported manager must still fail
    // the ordinary map-rules preflight and cannot enter Loading.
    oe::testing::economy::write(data/"Cities/C.map",oe::testing::economy::map_file(false));
    const auto app=source.root/"unsupported";seed_preferences(app,data);
    auto settings=oe::menu::read_settings(app).value;settings.last_map="Cities/C.map";
    oe::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    click(menu,window,renderer,"Play Original economy");key(menu,SDLK_P);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking &&
        std::chrono::steady_clock::now()<deadline) {menu.advance();std::this_thread::yield();}
    check(menu.map_rules_result().status==oe::maps::MapRulesStatus::UnsupportedForRules ||
        menu.map_rules_result().status==oe::maps::MapRulesStatus::InputError,
        "Unsupported Empty control did not finish its actual map rules check");
    key(menu,SDLK_RETURN);
    check(!menu.map_rules_can_start() && menu.state()==Menu::State::NewSandbox && !menu.sandbox(),
        "Empty city bypassed unsupported original map rules");
    menu.shutdown();
    std::cout<<"Starter rules/placement separation, complete error dialog, explicit Empty/map choice, "
        "retained World/files and pointer cancellation PASS\n";
}

void starter_availability_real(const fs::path& data,const fs::path& output,
                               SDL_Window*& window,SDL_Renderer*& renderer) {
    check(!fs::exists(output),"Real starter evidence output must be fresh");
    const auto source=fs::canonical(data),target=fs::weakly_canonical(output);
    const auto to_source=target.lexically_relative(source),from_source=source.lexically_relative(target);
    check(!to_source.empty() && *to_source.begin()==".." &&
        !from_source.empty() && *from_source.begin()=="..","Real starter output overlaps original files");
    fs::create_directories(output);
    nlohmann::json rows=nlohmann::json::array();
    constexpr std::array<std::pair<int,int>,3> sizes{{{1280,800},{1600,900},{1920,1080}}};
    for (const auto& [width,height]:sizes) {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);renderer=nullptr;window=nullptr;
        check(SDL_CreateWindowAndRenderer("Real starter availability",width,height,SDL_WINDOW_RESIZABLE,&window,&renderer),
            "Create real starter test aperture");
        const auto name=std::to_string(width)+"x"+std::to_string(height);
        const auto app=output/name;seed_preferences(app,source);
        auto settings=oe::menu::read_settings(app).value;settings.last_map="Cities/ASSR-Loulan.map";
        oe::menu::write_settings(app,settings);
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
        enter_recommended(menu,window,renderer);
        const auto request=menu.map_rules_result().request;
        check(request.map_relative==fs::path("Cities/ASSR-Loulan.map"),"Real control selected a different map");
        const auto files=recovery_files(app);
        key(menu,SDLK_RETURN);finish_load(menu);
        check(menu.state()==Menu::State::StarterUnavailable && !menu.sandbox() && recovery_files(app)==files,
            "Real no-layout starter did not refuse without publishing files or a World");
        capture(menu,renderer,output/"captures",("loulan.unavailable."+name).c_str());
        const auto reason=menu.message();click(menu,window,renderer,"P Use Empty city");wait_check(menu);
        check(menu.map_rules_result().request==request && !menu.original_paid_starter(),
            "Real explicit Empty choice changed the map, rules or source identity");
        key(menu,SDLK_RETURN);finish_load(menu);
        check(menu.state()==Menu::State::Playing && menu.sandbox()->paused() &&
            menu.sandbox()->world().ticks()==0 && menu.sandbox()->world().treasury()==12000 &&
            menu.sandbox()->world().buildings().empty(),"Real map could not start an ordinary checked Empty city");
        const auto saved=menu.sandbox()->capture_save_document();key(menu,SDLK_F5);
        const auto persisted=oe::persistence::read_save(menu.settings().last_save);
        check(persisted.source_schema_version==34,"Real Empty city changed its historical save schema");
        for (int i=0;i<20;++i) menu.sandbox()->tick_once();key(menu,SDLK_F9);
        same(saved,menu.sandbox()->capture_save_document());
        rows.push_back({{"map",request.map_relative.generic_string()},{"resolution",name},
            {"map_rules","AUTOMATED_PASS"},{"starter","REFUSED"},{"reason",reason},
            {"empty_city","AUTOMATED_PASS"},{"f5_f9_full_document","AUTOMATED_PASS"},
            {"source_sha256",saved.map_sha256},{"rule",saved.world.rule_version},
            {"schema",persisted.source_schema_version},{"funds",saved.world.treasury}});
        menu.shutdown();
    }
    const auto app=output/"xia";seed_preferences(app,source);
    auto settings=oe::menu::read_settings(app).value;settings.last_map="Cities/Xia.map";
    oe::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    enter_recommended(menu,window,renderer);recommended_paid_start(menu);
    const auto saved=menu.sandbox()->capture_save_document();
    check(saved.world.treasury==10596 && saved.world.construction_spent_total==1404,
        "Real Xia starter changed paid accounting");
    key(menu,SDLK_F5);const auto persisted=oe::persistence::read_save(menu.settings().last_save);
    check(persisted.source_schema_version==34,"Real Xia starter changed its historical save schema");
    rows.push_back({{"map","Cities/Xia.map"},{"map_rules","AUTOMATED_PASS"},{"starter","AUTOMATED_PASS"},
        {"source_sha256",saved.map_sha256},{"rule",saved.world.rule_version},{"schema",persisted.source_schema_version},
        {"tick",saved.world.ticks},{"funds",saved.world.treasury},{"houses",house_count(menu.sandbox()->world())}});
    menu.shutdown();
    oe_influence_fixture::write_text(output/"report.json",nlohmann::json{
        {"status","AUTOMATED_PASS"},{"scope","Actual production MenuSession, software SDL; no native human acceptance"},
        {"cases",rows}}.dump(2));
    std::cout<<"Real Loulan map-rules PASS/starter refusal/explicit checked Empty, "
        "three readable menu apertures and unchanged Xia paid starter PASS\n";
}
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"Initialize dummy SDL");
        check(SDL_CreateWindowAndRenderer("Original menu",900,600,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create minimum-size actual menu");
        if ((argc==2 && std::string_view(argv[1])=="--starter-availability") ||
            (argc==3 && std::string_view(argv[1])=="--starter-availability-capture")) {
            starter_availability(window,renderer,argc==3 ? fs::absolute(argv[2]):fs::path{});
        } else if (argc==4 && std::string_view(argv[1])=="--starter-availability-real") {
            starter_availability_real(fs::absolute(argv[2]),fs::absolute(argv[3]),window,renderer);
        } else if (argc==4 && std::string_view(argv[1])=="--player-mode-real") {
            player_mode_real(fs::absolute(argv[2]),fs::absolute(argv[3]),window,renderer);
        } else if ((argc==2 && std::string_view(argv[1])=="--player-mode-menu") ||
            (argc==3 && std::string_view(argv[1])=="--player-mode-capture")) {
            player_mode_menu(window,renderer,argc==3 ? fs::absolute(argv[2]):fs::path{});
        } else if (argc==3 && std::string_view(argv[1])=="--player-mode-process-save") {
            player_mode_process_write(fs::absolute(argv[2]),window,renderer);
        } else if (argc==3 && std::string_view(argv[1])=="--player-mode-process-load") {
            player_mode_process_load(fs::absolute(argv[2]),window,renderer);
        } else if (argc==3 && std::string_view(argv[1])=="--process-save") {
            process_write(fs::absolute(argv[2]),window,renderer);
        } else if (argc==3 && std::string_view(argv[1])=="--process-load") {
            process_load(fs::absolute(argv[2]),window,renderer);
        } else if (argc==2 && std::string_view(argv[1])=="--rule18-menu") {
            fire_protection_menu(window,renderer,false,false,false,false,false,false,600,400,true);
            SDL_DestroyRenderer(renderer);renderer=nullptr;
            SDL_DestroyWindow(window);window=nullptr;
            check(SDL_CreateWindowAndRenderer("Original Rule18 wide menu",1920,1080,
                SDL_WINDOW_RESIZABLE,&window,&renderer),"Create actual1280x720 logical Food menu");
            fire_protection_menu(window,renderer,false,false,false,false,false,false,1280,720,true);
        } else if (argc==2 && std::string_view(argv[1])=="--rule17-menu") {
            // Keep the complete historical menu case within its unchanged
            // deadline, while independently exercising every new Rule17 check
            // at both actual production UI apertures.
            fire_protection_menu(window,renderer,false,false,false,false,false,true);
            SDL_DestroyRenderer(renderer);renderer=nullptr;
            SDL_DestroyWindow(window);window=nullptr;
            check(SDL_CreateWindowAndRenderer("Original Rule17 wide menu",1920,1080,
                SDL_WINDOW_RESIZABLE,&window,&renderer),"Create actual1280x720 logical menu");
            fire_protection_menu(window,renderer,false,false,false,false,false,true,1280,720);
        } else {
            const bool core_only=argc==2 && std::string_view(argv[1])=="--core-menu";
            const bool successor_only=argc==2 && std::string_view(argv[1])=="--legacy-successor-menu";
            check(argc==1 || core_only || successor_only ||
                (argc==3 && std::string_view(argv[1])=="--capture"),"Unknown Original menu test arguments");
            // The no-argument lifecycle retains its complete historical order.
            // CTest runs these same assertions as separately bounded groups.
            if (!core_only) {
                fire_protection_menu(window,renderer);
                fire_protection_menu(window,renderer,true);
                fire_protection_menu(window,renderer,false,true);
                fire_protection_menu(window,renderer,false,false,true);
                fire_protection_menu(window,renderer,false,false,false,true);
                fire_protection_menu(window,renderer,false,false,false,false,true);
            }
            if (!successor_only) {
            capture_failure_recovery(window,renderer);
            const fs::path captures=argc==3 ? fs::absolute(argv[2]):fs::path{};
            oe::testing::economy::Temp temp;const auto root=fs::canonical(temp.root),data=root/"data",app=root/"app";
            oe_production_fixture::write_model(data);seed_preferences(app,data);
            auto dialog=std::make_unique<Dialog>();auto* callbacks=dialog.get();
            Menu menu({},app,std::move(dialog));menu.initialize(window,renderer);
            expect_legacy_preferences(menu);check(!menu.original_setup(),"Original became the fresh default");
            capture(menu,renderer,captures,"menu.main.minimum");
            open_legacy(menu,window,renderer);check_bounds(menu,"O Original economy");
            capture(menu,renderer,captures,"menu.main.legacy.minimum");
            enter_original(menu,window,renderer,true);wait_map(menu);
            check(menu.original_difficulty()==orig::Difficulty::Normal && menu.original_paid_starter(),
                "Original setup did not default to Normal and the explicit paid plan");
            for (const auto& label:{"Previous map","Next map","D Difficulty: Normal","P Paid four-house starter","Start sandbox","Back","Check again"})
                check_bounds(menu,label);
            capture(menu,renderer,captures,"menu.original.minimum");

            // A release after focus/resize cancellation cannot change difficulty.
            const std::array<Uint32,8> boundaries{SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_WINDOW_MOUSE_LEAVE,
                SDL_EVENT_WINDOW_HIDDEN,SDL_EVENT_WINDOW_MINIMIZED,SDL_EVENT_WINDOW_RESIZED,
                SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,SDL_EVENT_WINDOW_DISPLAY_CHANGED,SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED};
            for (const auto type:boundaries) {
                const auto p=button_point(menu,window,renderer,"D Difficulty: Normal");mouse(menu,p,true);
                SDL_Event e{};e.type=type;route(menu,e);mouse(menu,p,false);menu.advance();
                check(menu.original_difficulty()==orig::Difficulty::Normal && menu.diagnostic_pressed_action()==-1,
                    "A canceled Original control press changed difficulty");
            }
            click(menu,window,renderer,"P Paid four-house starter");
            check(!menu.original_paid_starter(),"Actual setup click did not choose an empty Original city");
            constexpr std::array<orig::Difficulty,5> choices{orig::Difficulty::Normal,orig::Difficulty::Hard,
                orig::Difficulty::VeryHard,orig::Difficulty::VeryEasy,orig::Difficulty::Easy};
            fs::path manual;
            for (const auto wanted:choices) {
                check(menu.original_difficulty()==wanted,"Difficulty cycle did not match the selected typed model");
                start(menu);
                check(menu.state()==Menu::State::Playing && menu.sandbox()->paused(),"Original menu did not publish a paused session");
                const auto& world=menu.sandbox()->world();
                check(world.rule_version()==10 && world.original_production_enabled() && world.ticks()==0 &&
                    world.treasury()==12000 && world.buildings().empty() && world.total_population()==0 &&
                    world.original_rules_model()->difficulty()==wanted,"New empty Original city used wrong rules, funds or difficulty");
                expect_legacy_preferences(menu);check_original_recovery(menu,app);
                key(menu,SDLK_F5);manual=menu.settings().last_save;const auto saved=menu.sandbox()->capture_save_document();
                const auto before_f9_history=menu.recovery_history_id();
                const auto before_f9_count=recovery_catalog(menu,app).histories.size();
                menu.sandbox()->tick_once();key(menu,SDLK_F9);same(saved,menu.sandbox()->capture_save_document());
                check(menu.recovery_history_id()!=before_f9_history &&
                    recovery_catalog(menu,app).histories.size()==before_f9_count+1,
                    "Successful Original F9 did not branch its recovery without deleting the previous history");
                key(menu,SDLK_ESCAPE);check(menu.state()==Menu::State::MainMenu,"Escape did not retain Original session in menu");
                const auto auto_point=button_point(menu,window,renderer,"Autosave: ON");
                const auto enabled=menu.diagnostic_button_hit(auto_point.x,auto_point.y);
                check(enabled && enabled->enabled,"Original menu hid its autosave choice");
                mouse(menu,auto_point,true);mouse(menu,auto_point,false);menu.advance();
                check(!menu.settings().autosave_enabled && menu.recovery_history_id().empty(),
                    "Original autosave OFF retained an active writer");
                click(menu,window,renderer,"Autosave: OFF");expect_legacy_preferences(menu);
                enter_original(menu,window,renderer);key(menu,SDLK_D);
            }
            // Selecting old New Sandbox remains the old profile and prepared starter.
            key(menu,SDLK_ESCAPE);key(menu,SDLK_N);wait_check(menu);
            check(!menu.original_setup() && menu.map_rules_result().request.profile==sim::RulesProfile::CityV11,
                "Legacy N entry inherited the Original selection");
            key(menu,SDLK_RETURN);finish_load(menu);
            check(menu.state()==Menu::State::Playing && menu.sandbox()->world().profile()==sim::RulesProfile::CityV11 &&
                !menu.sandbox()->world().original_production_enabled(),"Legacy N start opened Original economy");
            const auto retained=menu.sandbox();const auto retained_world=retained->world().snapshot();
            const auto model_path=data/"Model/EmperorBuildingModels.txt";
            key(menu,SDLK_ESCAPE);enter_original(menu,window,renderer);key(menu,SDLK_P);
            check(menu.original_paid_starter(),"Paid-plan toggle did not remain separate from legacy starter preference");
            fs::rename(model_path,model_path.string()+".missing");start(menu);
            check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
                menu.sandbox()->world().snapshot()==retained_world &&
                (menu.message().find("model")!=std::string::npos || menu.message().find("Model")!=std::string::npos),
                "Missing Original model replaced the existing city or hid its actual cause");
            fs::rename(model_path.string()+".missing",model_path);
            start(menu);
            check(menu.state()==Menu::State::ConfirmLeave && menu.sandbox()==retained &&
                menu.sandbox()->world().snapshot()==retained_world,"Original candidate bypassed the dirty-city confirmation");
            click(menu,window,renderer,"Cancel");
            check(menu.state()==Menu::State::MainMenu && menu.sandbox()==retained &&
                menu.sandbox()->world().snapshot()==retained_world,"Cancel did not retain the complete previous city");
            enter_original(menu,window,renderer);start(menu);click(menu,window,renderer,"Discard Changes");
            check(menu.state()==Menu::State::Playing && house_count(menu.sandbox()->world())==4 &&
                menu.sandbox()->world().ticks()==0 && menu.sandbox()->world().total_population()==0 &&
                menu.sandbox()->world().construction_spent_total()>0,"Confirmed menu start did not buy four empty Houses at tick0");
            expect_legacy_preferences(menu);check_original_recovery(menu,app);
            capture(menu,renderer,captures,"menu.original.paid");
            key(menu,SDLK_F5);const auto paid_save=menu.settings().last_save;
            const auto paid_bytes=oe_influence_fixture::read_text(paid_save);
            const auto paid_document=menu.sandbox()->capture_save_document();
            // Crossing the ordinary safe deadline publishes one checkpoint.
            const auto paid_history=menu.recovery_history_id();
            const auto before_periodic=recovery_catalog(menu,app).histories.size();
            for (int i=0;i<1201;++i) menu.sandbox()->tick_once();
            menu.update(0);
            const auto periodic=recovery_catalog(menu,app);
            const auto current=std::find_if(periodic.histories.begin(),periodic.histories.end(),[&](const auto& h){return h.history_id==paid_history;});
            check(current!=periodic.histories.end() && current->entries.size()==2 &&
                current->entries.front().tick==1201 && !current->entries.front().start_point,
                "Original safe1200t deadline did not create exactly one actual checkpoint");
            key(menu,SDLK_F9);same(paid_document,menu.sandbox()->capture_save_document());
            check(recovery_catalog(menu,app).histories.size()==before_periodic+1 && menu.recovery_history_id()!=paid_history,
                "Original F9 overwrote its previous active checkpoint history");
            check(oe_influence_fixture::read_text(paid_save)==paid_bytes,"F9 changed the manual paid source save");
            // The normal Recovery menu loads a protected source into a new branch.
            const auto selected_history=menu.recovery_history_id();
            const auto selection_catalog=recovery_catalog(menu,app);
            const auto selection=std::find_if(selection_catalog.histories.begin(),selection_catalog.histories.end(),
                [&](const auto& h){return h.history_id==selected_history;});
            check(selection!=selection_catalog.histories.end(),"Missing selected Original recovery history");
            const auto recovery_source=selection->entries.front();
            const auto recovery_bytes=oe_influence_fixture::read_text(recovery_source.path);
            key(menu,SDLK_ESCAPE);key(menu,SDLK_L);
            click(menu,window,renderer,"Show recovery history");
            click(menu,window,renderer,"Open selected");finish_load(menu);
            check(menu.state()==Menu::State::Playing && menu.sandbox()->paused(),
                "Recovery menu did not restore a paused Original city");
            same(paid_document,menu.sandbox()->capture_save_document());
            const auto branch_catalog=recovery_catalog(menu,app);
            const auto branch=std::find_if(branch_catalog.histories.begin(),branch_catalog.histories.end(),
                [&](const auto& h){return h.history_id==menu.recovery_history_id();});
            check(branch!=branch_catalog.histories.end() && branch->parent &&
                branch->parent->history_id==recovery_source.history_id && branch->parent->sequence==recovery_source.sequence,
                "Recovery selection did not create a separate real-parent branch");
            key(menu,SDLK_F5);
            check(menu.settings().last_save!=recovery_source.path &&
                oe_influence_fixture::read_text(recovery_source.path)==recovery_bytes,
                "F5 after Recovery menu load overwrote its source checkpoint");
            // Opening the same owned checkpoint through the file chooser has the same protection.
            external_load(menu,*callbacks,window,renderer,recovery_source.path);
            check(menu.state()==Menu::State::Playing,"File chooser could not open a valid owned Original recovery point");
            key(menu,SDLK_F5);
            check(menu.settings().last_save!=recovery_source.path &&
                oe_influence_fixture::read_text(recovery_source.path)==recovery_bytes,
                "File chooser/F5 treated a managed checkpoint as a writable manual target");
            const auto failed_history=menu.recovery_history_id();
            const auto before_failed=menu.sandbox()->capture_save_document();
            const auto before_failed_world=menu.sandbox()->world().snapshot();
            const auto before_failed_generation=menu.sandbox()->load_generation();
            fs::rename(model_path,model_path.string()+".missing");key(menu,SDLK_F9);
            check(menu.sandbox()->world().snapshot()==before_failed_world,
                "Failed F9 with absent model changed the retained complete World");
            check(menu.recovery_history_id()==failed_history &&
                menu.sandbox()->load_generation()==before_failed_generation,
                "Failed F9 replaced a valid World or recovery history");
            fs::rename(model_path.string()+".missing",model_path);
            same(before_failed,menu.sandbox()->capture_save_document());
            const auto pure=menu.sandbox()->world().snapshot();
            oe::performance::set_enabled(true);oe::performance::reset();
            for (int i=0;i<100;++i) {menu.update(0);check(menu.render(),"Draw pure Original menu-managed frame");menu.advance();}
            for (std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
                check(oe::performance::counter(static_cast<oe::performance::Counter>(i))==0,
                    "Idle Original menu frames performed authority, preparation, navigation or files");
            oe::performance::set_enabled(false);
            check(menu.sandbox()->world().snapshot()==pure,"Pure menu-managed Original frames changed authoritative World");

            auto bad=nlohmann::json::parse(paid_bytes);bad["world"]["original_production"]["development_revision"]=26;
            const auto hostile=root/"unsupported-revision.json";oe_influence_fixture::write_text(hostile,bad.dump());
            const auto active=menu.sandbox();external_load(menu,*callbacks,window,renderer,hostile);
            check(menu.state()==Menu::State::LoadSandbox && menu.sandbox()==active &&
                menu.sandbox()->world().snapshot()==pure && menu.message().find("revision")!=std::string::npos,
                "Manual load migrated an old revision or replaced the retained Original city");
            fs::rename(model_path,model_path.string()+".missing");external_load(menu,*callbacks,window,renderer,paid_save);
            check(menu.state()==Menu::State::LoadSandbox && menu.sandbox()==active &&
                menu.sandbox()->world().snapshot()==pure,"Missing saved model replaced an existing city");
            fs::rename(model_path.string()+".missing",model_path);
            // The old developer generations are valid saves but remain unavailable to the normal app.
            const auto old_model=oe::persistence::prepare_original_rules_model(data);
            for (const unsigned rule:{7U,8U,9U}) {
                const auto permissions=menu.sandbox()->world().map_permissions();
                auto old=rule==7 ? sim::World::create_original_economy_development(permissions,old_model):
                    rule==8 ? sim::World::create_original_influence_development(permissions,old_model):
                    sim::World::create_original_goods_development(permissions,old_model);
                const auto doc=oe::persistence::make_document(data,"Cities/A.map",menu.sandbox()->buildable_mask(),old);
                const auto path=root/("old-rule"+std::to_string(rule)+".json");
                oe::persistence::write_save(path,doc,data,menu.sandbox()->buildable_mask());
                external_load(menu,*callbacks,window,renderer,path);
                check(menu.state()==Menu::State::LoadSandbox && menu.sandbox()==active &&
                    menu.sandbox()->world().snapshot()==pure,"Normal menu silently opened a frozen developer generation");
            }
            menu.shutdown();
            // A new MenuSession starts with Normal/paid defaults but explicitly loads saved Easy unchanged.
            auto fresh_dialog=std::make_unique<Dialog>();auto* fresh_callbacks=fresh_dialog.get();
            Menu fresh({},app,std::move(fresh_dialog));fresh.initialize(window,renderer);
            expect_legacy_preferences(fresh);
            check(!fresh.original_setup() && fresh.original_difficulty()==orig::Difficulty::Normal && fresh.original_paid_starter(),
                "A restart persisted the session-only Original controls");
            external_load(fresh,*fresh_callbacks,window,renderer,manual);
            check(fresh.state()==Menu::State::Playing && fresh.sandbox()->world().original_rules_model()->difficulty()==orig::Difficulty::Easy &&
                fresh.sandbox()->world().buildings().empty(),"Manual load overrode saved difficulty or repeated starter purchases");
            same(oe::persistence::read_save(manual),fresh.sandbox()->capture_save_document());expect_legacy_preferences(fresh);
            fresh.shutdown();
            }
        }
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Original menu controls, defaults and manual persistence checks passed\n";return 0;
    } catch (const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();return 1;
    }
}
