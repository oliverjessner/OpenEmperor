#include "app/MenuCheck.h"
#include "app/MenuSession.h"
#include "renderer/StoredGraphicsRenderer.h"
#include <SDL3/SDL.h>
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
}

int run_menu_check(int argc, char* argv[]) {
    // Private, finite packaging check. An explicit root prevents reads or writes to real preferences.
    if ((argc!=4 && argc!=7) || std::string_view(argv[1])!="--menu-check" ||
        std::string_view(argv[2])!="--app-root" ||
        (argc==7 && (std::string_view(argv[4])!="--data" || std::string_view(argv[6])!="--industry"))) {
        std::cerr << "Usage: openemperor --menu-check --app-root <temporary-directory> [--data <directory> --industry]\n";
        return 2;
    }
    SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr;
    try {
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy init failed");
        require(SDL_CreateWindowAndRenderer("OpenEmperor check",1100,700,SDL_WINDOW_RESIZABLE,
                                            &window,&renderer),"SDL window/renderer failed");
        const fs::path app_root=fs::absolute(argv[3]);
        require(!fs::exists(app_root),"check app root must be fresh");
        const fs::path data=argc==7 ? fs::absolute(argv[5]) : fs::path{};
        if (!data.empty()) {
            openemperor::menu::Settings settings;
            settings.data_root=data; settings.last_map="Cities/Xia.map";
            settings.profile=openemperor::simulation::RulesProfile::IndustryV5;
            openemperor::menu::write_settings(app_root,settings);
        }
        {
            Menu menu(data,app_root,std::make_unique<NoDialog>());
            menu.initialize(window,renderer); frame(menu); frame(menu);
            if (data.empty()) {
                require(menu.state()==Menu::State::DataSetup,"fresh menu did not show data setup");
            } else {
                require(menu.state()==Menu::State::MainMenu,"data menu did not initialize");
                click(menu,90,220); click(menu,90,650); frame(menu);
                require(menu.state()==Menu::State::Playing && menu.sandbox(),"Industry sandbox did not open");
                for (int i=0;i<20;++i) menu.sandbox()->tick_once();
                require(menu.sandbox()->world().ticks()>=20,"simulation did not advance");
                key(menu,SDLK_F5);
                require(fs::is_regular_file(menu.sandbox()->save_path()),"save not written");
                const auto snapshot=menu.sandbox()->world().snapshot();
                key(menu,SDLK_ESCAPE); click(menu,90,310); click(menu,90,560); frame(menu);
                require(menu.state()==Menu::State::Playing && menu.sandbox()->paused() &&
                        menu.sandbox()->world().snapshot()==snapshot,"saved world did not resume");
                menu.sandbox()->tick_once();
                require(menu.sandbox()->world().ticks()>snapshot.ticks,"resumed world did not advance");
            }
            menu.shutdown();
        }
        require(openemperor::StoredGraphicsRenderer::live_texture_count()==0,"texture leak");
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::cout << (data.empty() ? "{\"menu_setup\":true}" :
                                  "{\"menu_industry_save_resume\":true}") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Menu check failed: " << error.what() << '\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }
}
