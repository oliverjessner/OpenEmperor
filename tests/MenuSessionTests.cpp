#include "app/MenuSession.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "maps/StoredGraphicsPlan.h"
#include <SDL3/SDL.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
void check(bool yes,const char* message) { if (!yes) throw std::runtime_error(message); }
void u16(Bytes& b,std::size_t at,std::uint16_t v) { b.at(at)=static_cast<std::uint8_t>(v); b.at(at+1)=static_cast<std::uint8_t>(v>>8U); }
void u32(Bytes& b,std::size_t at,std::uint32_t v) { u16(b,at,static_cast<std::uint16_t>(v)); u16(b,at+2,static_cast<std::uint16_t>(v>>16U)); }
void write(const fs::path& p,const Bytes& bytes) {
    std::ofstream out(p,std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out),"fixture write failed");
}
Bytes sg3(bool tile) {
    Bytes b(40680U+(tile?203U:202U)*64U,0);
    u32(b,0,static_cast<std::uint32_t>(b.size())); u32(b,4,213);
    u32(b,12,tile?203:202); u32(b,16,tile?202:201); u32(b,20,1);
    const std::string group="Zeus_system.bmp";
    std::copy(group.begin(),group.end(),b.begin()+680);
    u32(b,680+124,200); u32(b,680+128,1); u32(b,680+132,200);
    if (tile) { const auto at=40680U+201U*64U;
        u32(b,at+4,3200); u32(b,at+8,3200); u16(b,at+20,78); u16(b,at+22,40);
        u16(b,at+50,30); b[at+55]=1;
    }
    return b;
}
Bytes map_file() {
    using namespace openemperor::maps;
    Bytes raw(static_cast<std::size_t>(objects_logical_offset+grid_byte_length),0);
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin()); u32(raw,84,84);
    for (std::size_t i=0;i<228U*228U;++i) {
        u32(raw,static_cast<std::size_t>(candidate_word_logical_offset)+4*i,0xc000);
        raw[static_cast<std::size_t>(candidate_byte_logical_offset)+i]=64;
        u32(raw,static_cast<std::size_t>(terrain_logical_offset)+4*i,0x80);
    }
    Bytes file{0xaa,0xba,0xdc,0xfe};
    for (std::size_t at=0;at<raw.size();at+=32768) {
        const auto n=std::min<std::size_t>(32768,raw.size()-at);
        uLongf capacity=compressBound(static_cast<uLong>(n)); Bytes compressed(static_cast<std::size_t>(capacity));
        check(compress2(compressed.data(),&capacity,raw.data()+at,static_cast<uLong>(n),6)==Z_OK,"zlib fixture");
        compressed.resize(static_cast<std::size_t>(capacity));
        const auto start=file.size(); file.resize(start+12);
        u32(file,start,0x12345678); u32(file,start+4,static_cast<std::uint32_t>(compressed.size()));
        u32(file,start+8,static_cast<std::uint32_t>(n));
        file.insert(file.end(),compressed.begin(),compressed.end());
    }
    return file;
}
struct Temp {
    fs::path root=fs::temp_directory_path()/ ("openemperor-menu-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {
        fs::create_directories(root/"data/DATA"); fs::create_directories(root/"data/Cities");
        write(root/"data/DATA/China_Terrain.sg3",sg3(true));
        write(root/"data/DATA/China_Elevation.sg3",sg3(false));
        Bytes bitmap(3200); for (std::size_t i=0;i<bitmap.size();i+=2) u16(bitmap,i,0x03e0);
        write(root/"data/DATA/China_Terrain.555",bitmap);
        write(root/"data/DATA/China_Elevation.555",{});
        write(root/"data/Cities/A.map",map_file());
        write(root/"data/Cities/B.map",map_file());
        write(root/"data/Cities/Broken.map",{'b','a','d'});
    }
    ~Temp() { std::error_code ec; fs::remove_all(root,ec); }
};
class FakeDialog final: public openemperor::menu::DialogAdapter {
public:
    Callback callback;
    void open_folder(SDL_Window*,Callback cb) override { callback=std::move(cb); }
    void open_file(SDL_Window*,Callback cb) override { callback=std::move(cb); }
    void cancel_pending() override { callback={}; }
    void answer(openemperor::menu::DialogResult value) { auto cb=std::move(callback); check(static_cast<bool>(cb),"dialog callback missing"); cb(std::move(value)); }
};
using Menu=openemperor::menu::MenuSession;
SDL_Event mouse(Uint32 type,float x,float y) {
    SDL_Event e{}; e.type=type; e.button.button=SDL_BUTTON_LEFT; e.button.x=x; e.button.y=y; return e;
}
void click(Menu& menu,float x,float y) {
    x*=0.75f; y*=0.75f; // Fixture coordinates below were written at 2x logical scale.
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y));
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_UP,x,y));
    menu.advance();
}
void key(Menu& menu,SDL_Keycode keycode) {
    SDL_Event e{}; e.type=SDL_EVENT_KEY_DOWN; e.key.key=keycode;
    menu.handle_event(e); menu.advance();
}
void finish_load(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"expected loading state");
    check(menu.render(),"loading frame"); menu.advance();
}
}
int main(int argc,char* argv[]) {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy init");
        SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("menu test",1100,700,SDL_WINDOW_RESIZABLE,&window,&renderer),"window");
        if (argc==3 && std::string_view(argv[1])=="--local-data") {
            const auto app_root=fs::temp_directory_path()/("openemperor-local-menu-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path,ec); } } cleanup{app_root};
            const auto data=fs::canonical(argv[2]);
            openemperor::menu::Settings settings;
            settings.data_root=data; settings.last_map="Cities/Xia.map";
            settings.profile=openemperor::simulation::RulesProfile::IndustryV5;
            openemperor::menu::write_settings(app_root,settings);
            Menu local(data,app_root,std::make_unique<FakeDialog>());
            local.initialize(window,renderer);
            check(local.state()==Menu::State::MainMenu,"local data rejected");
            click(local,90,220); click(local,90,650); finish_load(local);
            if (local.state()!=Menu::State::Playing) throw std::runtime_error("local Xia load: "+local.message());
            local.sandbox()->tick_once(); key(local,SDLK_F5);
            check(fs::exists(local.sandbox()->save_path()),"local save not written");
            const auto expected=local.sandbox()->world().snapshot();
            key(local,SDLK_ESCAPE); click(local,90,310); click(local,480,250);
            click(local,90,650); finish_load(local);
            if (local.state()!=Menu::State::Playing) throw std::runtime_error("second local map load: "+local.message());
            check(local.settings().last_map!="Cities/Xia.map","local map selection did not change");
            key(local,SDLK_ESCAPE); click(local,90,400); click(local,90,560); finish_load(local);
            check(local.state()==Menu::State::Playing && local.sandbox()->paused() &&
                  local.sandbox()->world().snapshot()==expected,"local saved Xia world not restored");
            local.shutdown();
            check(openemperor::StoredGraphicsRenderer::live_texture_count()==0,"local texture leak");
            SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
            std::cout<<"local menu Xia/second-map/save/reload checks passed\n";
            return 0;
        }
        Temp t;
        auto fake=std::make_unique<FakeDialog>(); auto* dialog=fake.get();
        {
            Menu menu({},t.root/"app",std::move(fake)); menu.initialize(window,renderer);
            check(menu.state()==Menu::State::DataSetup,"fresh start should require data setup");
            check(menu.render(),"setup render");
            click(menu,90,286); // logical y 143 at 2x output scale
            check(static_cast<bool>(dialog->callback),"folder dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Cancelled,{}}); menu.advance();
            check(menu.state()==Menu::State::DataSetup,"cancel changed setup");
            click(menu,90,286);
            std::thread worker([&]{ dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"invalid").string()}); }); worker.join(); menu.advance();
            check(menu.state()==Menu::State::DataSetup &&
                  menu.message().find("installed or extracted")!=std::string::npos &&
                  menu.message().find("GOG installer")!=std::string::npos,
                  "invalid root not reported");
            click(menu,90,286);
            std::thread valid([&]{ dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"data").string()}); }); valid.join(); menu.advance();
            check(menu.state()==Menu::State::MainMenu,"valid root not accepted");
            check(fs::exists(t.root/"app/settings.json"),"settings not stored");
            click(menu,90,220); // New sandbox
            check(menu.state()==Menu::State::NewSandbox,"new sandbox menu");
            click(menu,90,750); // Walker JSON through the existing dialog adapter.
            check(static_cast<bool>(dialog->callback),"walker dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"missing-walker.json").string()}); menu.advance();
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("walker manifest")!=std::string::npos,
                  "missing walker profile did not reject activation");
            click(menu,500,750); // Clear the session-only profile selection.
            click(menu,90,830); // Building JSON through the same session-only dialog.
            check(static_cast<bool>(dialog->callback),"building dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"missing-building.json").string()}); menu.advance();
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("building manifest")!=std::string::npos,
                  "missing building profile did not reject activation");
            click(menu,500,830); // Clear the building profile without persisting it.
            click(menu,90,910); // Road JSON via the same visual-profile dialog adapter.
            check(static_cast<bool>(dialog->callback),"road dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"missing-roads.json").string()}); menu.advance();
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("road manifest")!=std::string::npos,
                  "missing road profile did not reject activation");
            click(menu,500,910); // Clear the independent road selection.
            {
                std::ifstream stored(t.root/"app/settings.json");
                const std::string settings_text((std::istreambuf_iterator<char>(stored)),{});
                check(settings_text.find("missing-walker") == std::string::npos &&
                      settings_text.find("missing-building") == std::string::npos &&
                      settings_text.find("missing-roads") == std::string::npos,
                      "session-only visual profile leaked into settings");
            }
            click(menu,90,650); // Start
            finish_load(menu);
            if (!(menu.state()==Menu::State::Playing && menu.sandbox()))
                throw std::runtime_error("sandbox not opened: "+menu.message());
            check(!menu.sandbox()->walker_visuals_active() &&
                  !menu.sandbox()->building_visuals_active() &&
                  !menu.sandbox()->road_visuals_active(),
                  "new sandbox unexpectedly required optional visual profiles");
            const auto first_save=menu.sandbox()->save_path();
            check(!fs::exists(first_save),"new sandbox wrote save prematurely");
            const auto& mask=menu.sandbox()->buildable_mask();
            const auto cell=std::find(mask.begin(),mask.end(),1);
            check(cell!=mask.end(),"synthetic map has no buildable cell");
            const auto index=static_cast<std::size_t>(cell-mask.begin());
            check(menu.sandbox()->execute({openemperor::simulation::CommandType::PlaceRoad,
                {static_cast<int>(index%228),static_cast<int>(index/228)}}).changed,
                "menu sandbox command failed");
            menu.update(0.5); const auto tick=menu.sandbox()->world().ticks();
            check(tick>0 && menu.sandbox()->dirty(),"world did not tick or mark dirty");
            const auto& layout=menu.sandbox()->layout();
            const auto save_button=std::find_if(layout.buttons.begin(),layout.buttons.end(),[](const auto& b){
                return b.action==openemperor::sandbox_ui::Action::Save; });
            check(save_button!=layout.buttons.end(),"save button missing");
            menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,
                static_cast<float>(save_button->rect.x+4),static_cast<float>(save_button->rect.y+4)));
            key(menu,SDLK_ESCAPE);
            check(menu.state()==Menu::State::Playing,"Escape did not cancel active gesture first");
            key(menu,SDLK_ESCAPE);
            check(menu.state()==Menu::State::MainMenu,"escape did not open menu");
            menu.update(10.0); check(menu.sandbox()->world().ticks()==tick,"menu advanced world");
            click(menu,90,220); // Continue
            check(menu.state()==Menu::State::Playing && menu.sandbox()->world().ticks()==tick,
                  "resume changed world");
            key(menu,SDLK_F5);
            check(fs::exists(first_save) && !menu.sandbox()->dirty(),"explicit save failed");
            const auto saved=menu.sandbox()->world().snapshot();
            key(menu,SDLK_ESCAPE); click(menu,90,310); // Main, New
            click(menu,480,250); // Next map, Cities/B.map
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::Playing && menu.sandbox()->save_path()!=first_save,
                  "second session reused save target");
            check(menu.settings().last_map==fs::path("Cities/B.map"),"second map not selected");
            const auto second_save=menu.sandbox()->save_path();
            check(!fs::exists(second_save),"unsaved second session created file");
            menu.update(0.5); const auto second_tick=menu.sandbox()->world().ticks();
            key(menu,SDLK_ESCAPE); click(menu,90,400); // Main, Load
            check(menu.state()==Menu::State::LoadSandbox,"load list not opened");
            click(menu,90,560); finish_load(menu); // Open selected
            check(menu.state()==Menu::State::ConfirmLeave &&
                  menu.sandbox()->world().ticks()==second_tick,"dirty session was replaced before confirmation");
            click(menu,90,515); // Cancel
            check(menu.state()==Menu::State::MainMenu && menu.sandbox()->world().ticks()==second_tick,
                  "cancel lost prior session");
            click(menu,90,400); click(menu,90,560); finish_load(menu);
            click(menu,90,425); // Without saving
            if (!(menu.state()==Menu::State::Playing && menu.sandbox() && menu.sandbox()->paused() &&
                  menu.sandbox()->world().snapshot()==saved))
                throw std::runtime_error("saved world not restored: "+menu.message()+
                    " state="+std::to_string(static_cast<int>(menu.state())));
            menu.shutdown();
        }
        check(openemperor::StoredGraphicsRenderer::live_texture_count()==0,"texture leak after menu shutdown");
        {
            auto fake_restart=std::make_unique<FakeDialog>(); auto* restart_dialog=fake_restart.get();
            Menu restarted({},t.root/"app",std::move(fake_restart));
            restarted.initialize(window,renderer);
            check(restarted.state()==Menu::State::MainMenu,"restart did not read settings");
            click(restarted,90,220); click(restarted,90,650); finish_load(restarted);
            check(restarted.state()==Menu::State::Playing &&
                  !restarted.sandbox()->walker_visuals_active() &&
                  !restarted.sandbox()->building_visuals_active() &&
                  !restarted.sandbox()->road_visuals_active(),
                  "restart depended on a deleted optional visual profile");
            key(restarted,SDLK_ESCAPE); click(restarted,90,400);
            click(restarted,90,560); finish_load(restarted);
            check(restarted.state()==Menu::State::Playing && restarted.sandbox()->paused(),"restart load failed");
            const auto baseline=restarted.sandbox()->world().snapshot();
            const auto external=t.root/"external.json";
            fs::copy_file(restarted.sandbox()->save_path(),external);
            key(restarted,SDLK_ESCAPE); click(restarted,90,400);
            click(restarted,480,555); // Open external save dialog
            check(static_cast<bool>(restart_dialog->callback),"external save dialog missing");
            restart_dialog->answer({openemperor::menu::DialogResult::Kind::Selected,external.string()});
            restarted.advance(); finish_load(restarted);
            check(restarted.state()==Menu::State::Playing &&
                  restarted.sandbox()->world().snapshot()==baseline &&
                  restarted.sandbox()->save_path()==fs::canonical(external),
                  "external save was not restored without copying");
            restarted.sandbox()->tick_once();
            const auto preferences=t.root/"app/settings.json";
            fs::rename(preferences,t.root/"app/settings-backup.json");
            fs::create_directory(preferences);
            key(restarted,SDLK_F5);
            check(!restarted.sandbox()->dirty() &&
                  restarted.message().find("preferences update failed")!=std::string::npos,
                  "successful save and failed preference write were conflated");
            fs::remove(preferences); fs::rename(t.root/"app/settings-backup.json",preferences);
            restarted.sandbox()->tick_once();
            fs::remove(external); fs::create_directory(external);
            SDL_Event close{}; close.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            restarted.handle_event(close); restarted.advance();
            check(restarted.state()==Menu::State::ConfirmLeave,"dirty close did not confirm");
            click(restarted,90,335); // Save and continue
            check(restarted.state()==Menu::State::ConfirmLeave && restarted.running() &&
                  restarted.sandbox()->dirty(),"failed save discarded dirty session");
            click(restarted,90,515); // Cancel
            check(restarted.state()==Menu::State::Playing,"cancel close did not resume session");
            restarted.shutdown();
        }
        {
            const auto broken=t.root/"app/saves/bad.json";
            std::ofstream out(broken); out<<"{\"schema_version\":999}"; out.close();
            const auto list=openemperor::menu::list_saves(t.root/"app");
            check(list.entries.size()==2 &&
                  std::count_if(list.entries.begin(),list.entries.end(),[](const auto& e){return !e.error.empty();})==1,
                  "malformed save broke save list");
            Menu malformed({},t.root/"app",std::make_unique<FakeDialog>());
            malformed.initialize(window,renderer);
            click(malformed,90,310); key(malformed,SDLK_DOWN); click(malformed,90,560);
            finish_load(malformed);
            check(malformed.state()==Menu::State::Playing,"valid save could not load beside corrupt one");
            const auto retained=malformed.sandbox()->world().snapshot();
            key(malformed,SDLK_ESCAPE); click(malformed,90,400); click(malformed,90,560);
            finish_load(malformed);
            check(malformed.state()==Menu::State::LoadSandbox && malformed.sandbox() &&
                  malformed.sandbox()->world().snapshot()==retained &&
                  malformed.message().find("Save could not be loaded")!=std::string::npos,
                  "corrupt save replaced retained session");
            malformed.shutdown();
            const auto preferences=t.root/"app/settings.json";
            { std::ofstream file(preferences,std::ios::trunc); file<<"{bad"; }
            check(openemperor::menu::read_settings(t.root/"app").needs_reset,"corrupt settings accepted");
            { std::ofstream file(preferences,std::ios::trunc); file<<"{\"version\":99}"; }
            auto newer=openemperor::menu::read_settings(t.root/"app");
            check(newer.needs_reset,"future settings silently accepted");
            auto fake_reset=std::make_unique<FakeDialog>();
            Menu reset(t.root/"data",t.root/"app",std::move(fake_reset)); reset.initialize(window,renderer);
            check(reset.state()==Menu::State::MainMenu &&
                  openemperor::menu::read_settings(t.root/"app").needs_reset,
                  "unknown settings overwritten at startup");
            click(reset,600,490); // Explicit Reset preferences button
            check(!openemperor::menu::read_settings(t.root/"app").needs_reset,"settings reset not confirmed");
            reset.shutdown();
            fs::remove(t.root/"data/DATA/China_Elevation.555");
            bool missing_rejected=false;
            try { (void)openemperor::menu::validate_data_root(t.root/"data"); }
            catch (const std::exception& e) { missing_rejected=std::string(e.what()).find("China_Elevation.555")!=std::string::npos; }
            check(missing_rejected,"missing required bitmap not identified");
            bool nested_rejected=false;
            try { (void)openemperor::menu::new_save_target(t.root/"data/.app",t.root/"data"); }
            catch (const std::exception&) { nested_rejected=true; }
            check(nested_rejected && !fs::exists(t.root/"data/.app"),
                  "app storage inside original data was created");
            Menu missing(t.root/"removed-data",t.root/"app",std::make_unique<FakeDialog>());
            missing.initialize(window,renderer);
            check(missing.state()==Menu::State::DataSetup,"missing explicit root did not show setup");
            click(missing,480,490);
            check(missing.state()==Menu::State::DataSetup,"missing root allowed invalid Back action");
            missing.shutdown();
        }
        {
            auto fake_late=std::make_unique<FakeDialog>(); auto* raw=fake_late.get();
            Menu late({},t.root/"fresh",std::move(fake_late)); late.initialize(window,renderer);
            click(late,90,286);
            auto callback=raw->callback;
            late.shutdown();
            std::thread delayed([callback]{ callback({openemperor::menu::DialogResult::Kind::Selected,"late"}); });
            delayed.join();
        }
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::cout<<"menu session synthetic checks passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; SDL_Quit(); return 1; }
}
