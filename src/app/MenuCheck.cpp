#include "app/MenuCheck.h"
#include "app/MenuSession.h"
#include "renderer/StoredGraphicsRenderer.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
using Menu = openemperor::menu::MenuSession;
namespace fs = std::filesystem;
class NoDialog final : public openemperor::menu::DialogAdapter {
public:
    void open_folder(SDL_Window*, Callback) override { throw std::runtime_error("dialog in menu check"); }
    void open_file(SDL_Window*, Callback) override { throw std::runtime_error("dialog in menu check"); }
    void cancel_pending() override {}
};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void click(Menu& menu, float x, float y) {
    SDL_Event event{}; event.button.button=SDL_BUTTON_LEFT;
    event.button.x=x*0.75f; event.button.y=y*0.75f;
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; menu.handle_event(event);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP; menu.handle_event(event); menu.advance();
}
void key(Menu& menu, SDL_Keycode code) {
    SDL_Event event{}; event.type=SDL_EVENT_KEY_DOWN; event.key.key=code;
    menu.handle_event(event); menu.advance();
}
void frame(Menu& menu) {
    require(menu.render(),"menu frame failed"); menu.advance();
}
openemperor::simulation::BuildingId building_by_kind(
        const openemperor::simulation::World& world,openemperor::simulation::Object kind) {
    for (const auto& building:world.buildings()) if (building.kind==kind) return building.id;
    throw std::runtime_error("required building missing");
}
nlohmann::json city_state(const openemperor::simulation::World& world) {
    const auto farm=building_by_kind(world,openemperor::simulation::Object::Farm);
    const auto market=building_by_kind(world,openemperor::simulation::Object::Market);
    std::size_t moving=0;
    std::int64_t reservations=0;
    for (const auto& courier:world.couriers()) {
        moving+=courier.phase!=openemperor::simulation::CourierPhase::IdleAtWorkshop;
        reservations+=courier.cargo;
    }
    for (const auto& building:world.buildings())
        reservations+=building.reserved_incoming+building.reserved_food_incoming;
    return {{"profile",openemperor::simulation::rules_profile_name(world.profile())},
        {"rule_version",world.rule_version()},{"tick",world.ticks()},
        {"population",world.total_population()},{"treasury",world.treasury()},
        {"farm_running",world.building(farm).operating_enabled},
        {"farm_progress",world.building(farm).progress},
        {"market_priority",openemperor::simulation::workforce_priority_name(
            world.building(market).workforce_priority)},
        {"moving_couriers",moving},{"cargo_and_reservations",reservations},
        {"clay_total",world.clay_extracted_total()},
        {"pottery_total",world.pottery_completed_total()},
        {"food_total",world.food_produced_total()}};
}
}

int run_menu_check(int argc, char* argv[]) {
    // Private, finite packaging check. An explicit root prevents reads or writes to real preferences.
    if ((argc!=4 && argc!=7) || std::string_view(argv[1])!="--menu-check" ||
        std::string_view(argv[2])!="--app-root" ||
        (argc==7 && (std::string_view(argv[4])!="--data" ||
            (std::string_view(argv[6])!="--industry" && std::string_view(argv[6])!="--city" &&
             std::string_view(argv[6])!="--city-save-controls" &&
             std::string_view(argv[6])!="--city-load-controls")))) {
        std::cerr << "Usage: openemperor --menu-check --app-root <temporary-directory> "
                     "[--data <directory> (--industry|--city|--city-save-controls|"
                     "--city-load-controls)]\n";
        return 2;
    }
    SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr;
    try {
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy init failed");
        require(SDL_CreateWindowAndRenderer("OpenEmperor check",1100,700,SDL_WINDOW_RESIZABLE,
                                            &window,&renderer),"SDL window/renderer failed");
        const fs::path app_root=fs::absolute(argv[3]);
        const auto mode=argc==7 ? std::string_view(argv[6]):std::string_view{};
        const bool controls_save=mode=="--city-save-controls";
        const bool controls_load=mode=="--city-load-controls";
        require(controls_load ? fs::is_directory(app_root):!fs::exists(app_root),
                controls_load ? "control-load app root missing":"check app root must be fresh");
        const fs::path data=argc==7 ? fs::absolute(argv[5]) : fs::path{};
        const bool city=mode=="--city" || controls_save || controls_load;
        if (!data.empty() && !city) {
            openemperor::menu::Settings settings;
            settings.data_root=data; settings.last_map="Cities/Xia.map";
            settings.profile=openemperor::simulation::RulesProfile::IndustryV5;
            settings.prepared_starter=false;
            openemperor::menu::write_settings(app_root,settings);
        }
        {
            Menu menu(data,app_root,std::make_unique<NoDialog>());
            menu.initialize(window,renderer); frame(menu); frame(menu);
            if (data.empty()) {
                require(menu.state()==Menu::State::DataSetup,"fresh menu did not show data setup");
            } else if (controls_load) {
                require(menu.state()==Menu::State::MainMenu,"control-load menu did not initialize");
                click(menu,90,310); click(menu,90,560); frame(menu);
                require(menu.state()==Menu::State::Playing && menu.sandbox() && menu.sandbox()->paused(),
                        "saved City-v11 control state did not load after process restart");
                const auto& world=menu.sandbox()->world();
                const auto farm=building_by_kind(world,openemperor::simulation::Object::Farm);
                const auto market=building_by_kind(world,openemperor::simulation::Object::Market);
                require(world.rule_version()==3 && !world.building(farm).operating_enabled &&
                        world.building(market).workforce_priority==
                            openemperor::simulation::WorkforcePriority::High,
                        "operation state or priority changed across process restart");
                const auto loaded=city_state(world);
                const auto progress=world.building(farm).progress;
                require(menu.sandbox()->execute(
                            openemperor::simulation::set_building_operation(farm,true)).changed,
                        "loaded Farm could not resume");
                menu.sandbox()->tick_once();
                require(world.building(farm).progress==progress+1,"resumed Farm did not continue");
                std::cout << nlohmann::json{{"menu_city_v11_process_load",true},
                    {"loaded",loaded},{"resumed_progress",world.building(farm).progress}}.dump()<<'\n';
            } else {
                require(menu.state()==Menu::State::MainMenu,"data menu did not initialize");
                click(menu,90,220); click(menu,90,650); frame(menu);
                require(menu.state()==Menu::State::Playing && menu.sandbox(),"sandbox did not open");
                if (city) {
                    const auto& world=menu.sandbox()->world();
                    require(world.profile()==openemperor::simulation::RulesProfile::CityV11,
                            "fresh settings did not select City-v11");
                    require(world.rule_version()==openemperor::simulation::current_rule_version(
                                openemperor::simulation::RulesProfile::CityV11),
                            "fresh City-v11 did not use the current rule version");
                    require(world.ticks()==0 && world.treasury()==100 &&
                            world.construction_spent_total()==1200,
                            "prepared starter did not use the paid tick-zero layout");
                }
                const int ticks=controls_save ? 1000:20;
                for (int i=0;i<ticks;++i) menu.sandbox()->tick_once();
                require(menu.sandbox()->world().ticks()>=20,"simulation did not advance");
                if (controls_save) {
                    const auto& world=menu.sandbox()->world();
                    const auto farm=building_by_kind(world,openemperor::simulation::Object::Farm);
                    const auto market=building_by_kind(world,openemperor::simulation::Object::Market);
                    require(menu.sandbox()->execute(
                                openemperor::simulation::set_building_operation(farm,false)).changed &&
                            menu.sandbox()->execute(openemperor::simulation::set_building_workforce_priority(market,
                                openemperor::simulation::WorkforcePriority::High)).changed,
                            "operation controls could not be prepared for process restart");
                    key(menu,SDLK_F5);
                    require(fs::is_regular_file(menu.sandbox()->save_path()),"control save not written");
                    std::cout << nlohmann::json{{"menu_city_v11_process_save",true},
                        {"saved",city_state(world)}}.dump()<<'\n';
                    menu.shutdown();
                    require(openemperor::StoredGraphicsRenderer::live_texture_count()==0,"texture leak");
                    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                    return 0;
                }
                key(menu,SDLK_F5);
                require(fs::is_regular_file(menu.sandbox()->save_path()),"save not written");
                const auto snapshot=menu.sandbox()->world().snapshot();
                key(menu,SDLK_ESCAPE); click(menu,90,400); click(menu,90,560); frame(menu);
                require(menu.state()==Menu::State::Playing && menu.sandbox()->paused() &&
                        menu.sandbox()->world().snapshot()==snapshot,"saved world did not resume");
                menu.sandbox()->tick_once();
                require(menu.sandbox()->world().ticks()>snapshot.ticks,"resumed world did not advance");
            }
            menu.shutdown();
        }
        require(openemperor::StoredGraphicsRenderer::live_texture_count()==0,"texture leak");
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        if (!controls_load) std::cout << (data.empty() ? "{\"menu_setup\":true}" : city ?
                                  "{\"menu_city_v11_save_resume\":true}" :
                                  "{\"menu_industry_save_resume\":true}") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Menu check failed: " << error.what() << '\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }
}
