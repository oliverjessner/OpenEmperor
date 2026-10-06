#include "app/MenuSession.h"
#include "app/ResourceLocator.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "maps/StoredGraphicsPlan.h"
#include "maps/OriginalMapEntities.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
Bytes map_file(bool valid_manager=true) {
    using namespace openemperor::maps;
    Bytes raw(static_cast<std::size_t>(original_entities_logical_offset+6),0);
    // Complete empty authored manager for the rule-3 immutable map policy.
    u16(raw,static_cast<std::size_t>(original_entities_logical_offset),valid_manager ? 1:99);
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
    static int load_number=0; ++load_number;
    if (menu.state()!=Menu::State::Loading)
        throw std::runtime_error("expected loading state at load "+std::to_string(load_number)+
                                 ", state "+std::to_string(static_cast<int>(menu.state()))+
                                 ", message "+menu.message());
    check(menu.render(),"loading frame"); menu.advance();
}
}
int main(int argc,char* argv[]) {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy init");
        SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr;
        // Advanced visual rows reach output y=768 at the menu's 1.5x scale.
        check(SDL_CreateWindowAndRenderer("menu test",1100,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"window");
        if (argc==3 && std::string_view(argv[1])=="--local-data") {
            const auto app_root=fs::temp_directory_path()/("openemperor-local-menu-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path,ec); } } cleanup{app_root};
            const auto data=fs::canonical(argv[2]);
            openemperor::menu::Settings settings;
            settings.data_root=data; settings.last_map="Cities/Xia.map";
            settings.profile=openemperor::simulation::RulesProfile::IndustryV5;
            settings.prepared_starter=false;
            openemperor::menu::write_settings(app_root,settings);
            Menu local(data,app_root,std::make_unique<FakeDialog>());
            local.initialize(window,renderer);
            check(local.state()==Menu::State::MainMenu &&
                  local.settings().profile==openemperor::simulation::RulesProfile::IndustryV5 &&
                  !local.settings().prepared_starter,
                  "existing explicit profile or starter preference was not preserved");
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
        fs::path v2_source,v3_source;
        fs::path upgrade_root;
        std::optional<openemperor::simulation::WorldSnapshot> v2_expected;
        {
            const auto legacy_root=t.root/"legacy-settings";
            fs::create_directories(legacy_root);
            std::ofstream(legacy_root/"settings.json") <<
                "{\"version\":1,\"data_root\":\"\",\"last_map\":\"\","
                "\"last_save\":\"\",\"profile\":\"sandbox-city-v10\"}";
            const auto legacy=openemperor::menu::read_settings(legacy_root);
            check(!legacy.needs_reset &&
                  legacy.value.profile==openemperor::simulation::RulesProfile::CityV10 &&
                  !legacy.value.prepared_starter && legacy.value.autosave_enabled,
                  "alpha.1 settings did not retain the empty-starter choice");
            auto explicit_settings=legacy.value;
            explicit_settings.profile=openemperor::simulation::RulesProfile::IndustryV5;
            explicit_settings.prepared_starter=false;
            explicit_settings.autosave_enabled=false;
            openemperor::menu::write_settings(t.root/"roundtrip-settings",explicit_settings);
            const auto roundtrip=openemperor::menu::read_settings(t.root/"roundtrip-settings");
            check(!roundtrip.needs_reset &&
                  roundtrip.value.profile==openemperor::simulation::RulesProfile::IndustryV5 &&
                  !roundtrip.value.prepared_starter && !roundtrip.value.autosave_enabled,
                  "explicit profile and starter choice did not survive settings restart");
        }
        {
            auto settings=openemperor::menu::Settings{};
            settings.data_root=t.root/"data";settings.profile=openemperor::simulation::RulesProfile::CityV16;
            const auto root=t.root/"maintenance-app";openemperor::menu::write_settings(root,settings);
            Menu maintenance_menu({},root,std::make_unique<FakeDialog>());
            maintenance_menu.initialize(window,renderer);
            check(maintenance_menu.settings().profile==openemperor::simulation::RulesProfile::CityV16,"stored City-v16 reset");
            click(maintenance_menu,90,220);check(maintenance_menu.render(),"City-v16 description render");
            click(maintenance_menu,90,650);finish_load(maintenance_menu);
            check(maintenance_menu.state()==Menu::State::Playing && maintenance_menu.sandbox() &&
                maintenance_menu.sandbox()->world().profile()==openemperor::simulation::RulesProfile::CityV16 &&
                maintenance_menu.sandbox()->world().rule_version()==3 &&
                maintenance_menu.sandbox()->world().map_permissions() &&
                maintenance_menu.sandbox()->world().treasury()==20 &&
                maintenance_menu.sandbox()->world().current_maintenance_rate()==48 &&
                maintenance_menu.sandbox()->world().maintenance_spent_total()==0,"menu paid maintenance starter");
            maintenance_menu.sandbox()->save_now();const auto entries=openemperor::menu::list_saves(root);
            check(entries.entries.size()==1 && entries.entries[0].schema==19 &&
                entries.entries[0].profile==openemperor::simulation::city_v16_profile_name,"City-v16 save list identity");
            maintenance_menu.shutdown();
        }
        {
            auto fire_settings=openemperor::menu::Settings{};
            fire_settings.data_root=t.root/"data";
            fire_settings.profile=openemperor::simulation::RulesProfile::CityV12;
            const auto fire_root=t.root/"fire-app";
            openemperor::menu::write_settings(fire_root,fire_settings);
            Menu fire_menu({},fire_root,std::make_unique<FakeDialog>());
            fire_menu.initialize(window,renderer);
            check(fire_menu.settings().profile==openemperor::simulation::RulesProfile::CityV12,
                "explicit stored City-v12 profile was reset");
            click(fire_menu,90,220);click(fire_menu,90,650);finish_load(fire_menu);
            check(fire_menu.state()==Menu::State::Playing && fire_menu.sandbox() &&
                fire_menu.sandbox()->world().profile()==openemperor::simulation::RulesProfile::CityV12 &&
                fire_menu.sandbox()->world().rule_version()==1 &&
                fire_menu.sandbox()->world().ticks()==0 && fire_menu.sandbox()->world().treasury()==20 &&
                fire_menu.sandbox()->world().workforce_used()==24,"City-v12 menu starter failed");
            key(fire_menu,SDLK_F5);
            const auto state=fire_menu.sandbox()->world().snapshot();
            const auto entries=openemperor::menu::list_saves(fire_root);
            check(entries.entries.size()==1 && entries.entries[0].schema==14 &&
                entries.entries[0].rule_version==1 && entries.entries[0].profile==
                    openemperor::simulation::city_v12_profile_name,"City-v12 ordinary save list identity");
            key(fire_menu,SDLK_ESCAPE);click(fire_menu,90,400);click(fire_menu,90,560);finish_load(fire_menu);
            check(fire_menu.state()==Menu::State::Playing && fire_menu.sandbox()->paused() &&
                fire_menu.sandbox()->world().snapshot()==state,"City-v12 menu restore changed World");
            fire_menu.shutdown();
        }

        {
            auto quality_settings=openemperor::menu::Settings{};
            quality_settings.data_root=t.root/"data";
            quality_settings.profile=openemperor::simulation::RulesProfile::CityV13;
            const auto quality_root=t.root/"quality-app";
            openemperor::menu::write_settings(quality_root,quality_settings);
            Menu quality_menu({},quality_root,std::make_unique<FakeDialog>());
            quality_menu.initialize(window,renderer);
            check(quality_menu.settings().profile==openemperor::simulation::RulesProfile::CityV13,
                "explicit stored City-v13 profile was reset");
            click(quality_menu,90,220);click(quality_menu,90,650);finish_load(quality_menu);
            check(quality_menu.state()==Menu::State::Playing && quality_menu.sandbox() &&
                quality_menu.sandbox()->world().profile()==openemperor::simulation::RulesProfile::CityV13 &&
                quality_menu.sandbox()->world().rule_version()==1 &&
                quality_menu.sandbox()->world().ticks()==0 && quality_menu.sandbox()->world().treasury()==20 &&
                quality_menu.sandbox()->world().workforce_used()==24,"City-v13 menu starter failed");
            key(quality_menu,SDLK_F5);
            const auto state=quality_menu.sandbox()->world().snapshot();
            const auto entries=openemperor::menu::list_saves(quality_root);
            check(entries.entries.size()==1 && entries.entries[0].schema==15 &&
                entries.entries[0].rule_version==1 && entries.entries[0].profile==
                    openemperor::simulation::city_v13_profile_name,"City-v13 ordinary save list identity");
            key(quality_menu,SDLK_ESCAPE);click(quality_menu,90,400);click(quality_menu,90,560);finish_load(quality_menu);
            check(quality_menu.state()==Menu::State::Playing && quality_menu.sandbox()->paused() &&
                quality_menu.sandbox()->world().snapshot()==state,"City-v13 menu restore changed World");
            quality_menu.shutdown();
        }
        fs::create_directories(t.root/"plain-bin/resources");
        fs::create_directories(t.root/"OpenEmperor.app/Contents/MacOS");
        fs::create_directories(t.root/"OpenEmperor.app/Contents/Resources");
        check(openemperor::locate_resource_root(t.root/"plain-bin")==
                  fs::canonical(t.root/"plain-bin/resources") &&
              openemperor::locate_resource_root(t.root/"OpenEmperor.app/Contents/MacOS")==
                  fs::canonical(t.root/"OpenEmperor.app/Contents/Resources") &&
              openemperor::locate_resource_root(t.root/"missing",t.root/"plain-bin/resources")==
                  fs::canonical(t.root/"plain-bin/resources"),
              "resource locator depended on cwd or missed build/bundle roots");
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
            check(menu.settings().profile==openemperor::simulation::RulesProfile::CityV11 &&
                  menu.settings().prepared_starter,
                  "fresh settings did not recommend City-v11 with the prepared starter");
            check(!menu.compatibility().compatible() &&
                  menu.visual_selection().walker_source==openemperor::VisualProfileSource::Fallback &&
                  menu.visual_selection().building_source==openemperor::VisualProfileSource::Fallback &&
                  menu.visual_selection().road_source==openemperor::VisualProfileSource::Fallback,
                  "unknown synthetic data did not select atomic visual fallbacks");
            const auto detection_count=openemperor::assets::compatibility_detection_count();
            for (int frame=0;frame<1000;++frame) check(menu.render(),"menu frame failed");
            check(openemperor::assets::compatibility_detection_count()==detection_count,
                  "compatibility files were hashed in the frame loop");
            check(fs::exists(t.root/"app/settings.json"),"settings not stored");
            click(menu,90,220); // New sandbox
            check(menu.state()==Menu::State::NewSandbox,"new sandbox menu");
            check(menu.render(),"City-v11 recommended profile description did not render");
            click(menu,900,750); // Open Advanced visual previews.
            click(menu,90,830); // Walker JSON through the existing dialog adapter.
            check(static_cast<bool>(dialog->callback),"walker dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"missing-walker.json").string()}); menu.advance();
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("walker manifest")!=std::string::npos,
                  "missing walker profile did not reject activation");
            click(menu,500,830); // Clear the session-only profile selection.
            click(menu,90,910); // Building JSON through the same session-only dialog.
            check(static_cast<bool>(dialog->callback),"building dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"missing-building.json").string()}); menu.advance();
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("building manifest")!=std::string::npos,
                  "missing building profile did not reject activation");
            click(menu,500,910); // Clear the building profile without persisting it.
            click(menu,90,990); // Road JSON via the same visual-profile dialog adapter.
            check(static_cast<bool>(dialog->callback),"road dialog not opened");
            dialog->answer({openemperor::menu::DialogResult::Kind::Selected,
                (t.root/"missing-roads.json").string()}); menu.advance();
            click(menu,90,650); finish_load(menu);
            check(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("road manifest")!=std::string::npos,
                  "missing road profile did not reject activation");
            click(menu,500,990); // Clear the independent road selection.
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
            check(menu.sandbox()->world().profile()==openemperor::simulation::RulesProfile::CityV11 &&
                  menu.sandbox()->world().rule_version()==
                      openemperor::simulation::current_rule_version(
                          openemperor::simulation::RulesProfile::CityV11) &&
                  menu.sandbox()->world().ticks()==0 && menu.sandbox()->world().treasury()==100 &&
                  menu.sandbox()->world().construction_spent_total()==1200,
                  "prepared starter did not use the current City-v11 rules and normal budget");
            check(!menu.sandbox()->walker_visuals_active() &&
                  !menu.sandbox()->building_visuals_active() &&
                  !menu.sandbox()->road_visuals_active(),
                  "new sandbox unexpectedly required optional visual profiles");
            check(menu.sandbox()->walker_visual_source()==openemperor::VisualProfileSource::Fallback &&
                  menu.sandbox()->building_visual_source()==openemperor::VisualProfileSource::Fallback &&
                  menu.sandbox()->road_visual_source()==openemperor::VisualProfileSource::Fallback,
                  "unknown data sandbox did not keep category fallbacks");
            key(menu,SDLK_F2); key(menu,SDLK_F4); key(menu,SDLK_F6); key(menu,SDLK_W);
            check(openemperor::assets::compatibility_detection_count()==detection_count,
                  "visual toggles or camera input recomputed compatibility fingerprints");
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
            check(openemperor::assets::compatibility_detection_count()==detection_count,
                  "saving recomputed compatibility fingerprints");
            const auto saved=menu.sandbox()->world().snapshot();
            key(menu,SDLK_ESCAPE); key(menu,SDLK_N);
            for (int choice=0;choice<4;++choice) key(menu,SDLK_G);
            check(menu.render(),"Great Wall preview setup did not render");
            check(menu.sandbox()->world().snapshot()==saved && menu.sandbox()->save_path()==first_save,
                  "session-only Great Wall choice changed the retained World or save target");
            // Deliberately unsupported source manager tests failed preview
            // preparation, independently of the valid rule-3 fixture above.
            write(t.root/"data/Cities/A.map",map_file(false));
            write(t.root/"data/Cities/B.map",map_file(false));
            key(menu,SDLK_RETURN); finish_load(menu);
            if (!(menu.state()==Menu::State::NewSandbox &&
                  menu.message().find("Great Wall preview preparation failed")!=std::string::npos &&
                  menu.sandbox()->world().snapshot()==saved && menu.sandbox()->save_path()==first_save))
                throw std::runtime_error("failed Great Wall preparation retained state="+
                    std::to_string(static_cast<int>(menu.state()))+", message="+menu.message());
            write(t.root/"data/Cities/A.map",map_file());
            write(t.root/"data/Cities/B.map",map_file());
            key(menu,SDLK_G); // Return to Automatic without persisting the preview choice.
            key(menu,SDLK_ESCAPE); key(menu,SDLK_RETURN);
            {
                std::ifstream stored(t.root/"app/settings.json");
                const std::string settings_text((std::istreambuf_iterator<char>(stored)),{});
                check(settings_text.find("great_wall")==std::string::npos &&
                      settings_text.find("preview-stone")==std::string::npos,
                      "Great Wall presentation choice leaked into persistent settings");
            }
            {
                auto document=openemperor::persistence::read_save(first_save);
                document.world.rule_version=2;
                const auto old_world=openemperor::simulation::World::restore(
                    document.world,menu.sandbox()->buildable_mask());
                upgrade_root=fs::canonical(t.root)/"upgrade-app";
                fs::create_directories(upgrade_root/"saves");
                v2_source=upgrade_root/"saves/city-v11-v2.json";
                openemperor::persistence::write_save(v2_source,document,t.root/"data",
                                                     menu.sandbox()->buildable_mask());
                openemperor::menu::Settings upgrade_settings;
                upgrade_settings.data_root=t.root/"data";
                upgrade_settings.last_map="Cities/A.map";
                upgrade_settings.last_save=v2_source;
                upgrade_settings.profile=openemperor::simulation::RulesProfile::CityV11;
                upgrade_settings.prepared_starter=true;
                openemperor::menu::write_settings(upgrade_root,upgrade_settings);
                v2_expected=old_world.snapshot();
            }
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
            openemperor::persistence::RecoveryStore recovery_store(t.root/"app",t.root/"data");
            const auto recovery_catalog=recovery_store.catalog();
            check(!recovery_catalog.histories.empty() &&
                  !recovery_catalog.histories.front().entries.empty(),
                  "menu sessions did not persist a recovery start point");
            const auto source=recovery_catalog.histories.front().entries.front().path;
            std::ifstream source_before_file(source,std::ios::binary);
            const std::string source_before((std::istreambuf_iterator<char>(source_before_file)),{});
            const auto prior_settings=openemperor::menu::read_settings(t.root/"app").value;
            Menu recovered({},t.root/"app",std::make_unique<FakeDialog>());
            recovered.initialize(window,renderer);
            click(recovered,90,310); // Load Save.
            click(recovered,90,210); // Switch from Manual saves to Recovery history.
            click(recovered,90,560); finish_load(recovered);
            if (!(recovered.state()==Menu::State::Playing && recovered.sandbox() &&
                  recovered.sandbox()->paused() && recovered.sandbox()->save_path()!=source &&
                  recovered.settings().last_save==prior_settings.last_save))
                throw std::runtime_error("recovery load did not pause, branch, or preserve manual last_save: "+
                                         recovered.message());
            const auto new_manual_target=recovered.sandbox()->save_path();
            recovered.sandbox()->tick_once();key(recovered,SDLK_F5);
            std::ifstream source_after_file(source,std::ios::binary);
            const std::string source_after((std::istreambuf_iterator<char>(source_after_file)),{});
            check(fs::exists(new_manual_target) && new_manual_target!=source &&
                  source_after==source_before && recovered.settings().last_save==new_manual_target,
                  "F5 after recovery did not use a fresh manual target or changed its source");
            recovered.shutdown();
            fs::remove(new_manual_target);
            openemperor::menu::write_settings(t.root/"app",prior_settings);
        }
        {
            std::ifstream before_file(v2_source,std::ios::binary);
            const std::string before((std::istreambuf_iterator<char>(before_file)),{});
            Menu upgrade({},upgrade_root,std::make_unique<FakeDialog>());
            upgrade.initialize(window,renderer);
            check(upgrade.state()==Menu::State::MainMenu,"upgrade menu did not initialize");
            click(upgrade,90,310);
            check(upgrade.state()==Menu::State::LoadSandbox,"upgrade save list did not open");
            click(upgrade,480,650);
            check(upgrade.state()==Menu::State::ConfirmUpgrade &&
                  upgrade.message().find("original save stays unchanged")!=std::string::npos,
                  "v2 upgrade did not require explicit confirmation");
            click(upgrade,90,335); finish_load(upgrade);
            if (!(upgrade.state()==Menu::State::Playing && upgrade.sandbox() &&
                  upgrade.sandbox()->paused() && upgrade.sandbox()->world().rule_version()==3 &&
                  upgrade.sandbox()->save_path()!=v2_source))
                throw std::runtime_error("confirmed v2 upgrade did not create a separate v3 session: "+
                    upgrade.message()+" state="+std::to_string(static_cast<int>(upgrade.state())));
            auto comparable=upgrade.sandbox()->world().snapshot();
            comparable.rule_version=2;
            check(v2_expected && comparable==*v2_expected,
                  "v2 upgrade changed authoritative state beyond control defaults and version");
            std::ifstream after_file(v2_source,std::ios::binary);
            const std::string after((std::istreambuf_iterator<char>(after_file)),{});
            check(before==after &&
                  openemperor::persistence::read_save(upgrade.sandbox()->save_path()).source_schema_version==12,
                  "v2 upgrade modified its source or did not write schema 12");
            v3_source=upgrade.sandbox()->save_path();
            upgrade.shutdown();
        }
        {
            const auto original=openemperor::persistence::read_save(v3_source);
            std::ifstream source_file(v3_source,std::ios::binary);
            const std::string source_bytes((std::istreambuf_iterator<char>(source_file)),{});
            Menu upgrade({},upgrade_root,std::make_unique<FakeDialog>());
            upgrade.initialize(window,renderer);click(upgrade,90,310);
            click(upgrade,900,234); // Select the second (v3 copy) entry, not the historical v2 source.
            click(upgrade,480,650);
            check(upgrade.state()==Menu::State::ConfirmUpgrade,"v3 demolition copy needs confirmation");
            key(upgrade,SDLK_ESCAPE);
            check(upgrade.state()==Menu::State::LoadSandbox,"v3 upgrade cancel failed");
            click(upgrade,480,650);click(upgrade,90,335);finish_load(upgrade);
            check(upgrade.state()==Menu::State::Playing && upgrade.sandbox() &&
                upgrade.sandbox()->paused() && upgrade.sandbox()->world().rule_version()==4 &&
                upgrade.sandbox()->save_path()!=v3_source,"v3 copy did not enable v4 demolition separately");
            auto common=upgrade.sandbox()->world().snapshot();common.rule_version=3;
            check(common==original.world &&
                openemperor::persistence::read_save(upgrade.sandbox()->save_path()).source_schema_version==13,
                "v3 copy lost authoritative state or schema13");
            std::ifstream unchanged(v3_source,std::ios::binary);
            check(std::string((std::istreambuf_iterator<char>(unchanged)),{})==source_bytes,
                "demolition copy overwrote v3 source");
            upgrade.shutdown();
        }
        {
            auto fake_restart=std::make_unique<FakeDialog>(); auto* restart_dialog=fake_restart.get();
            Menu restarted({},t.root/"app",std::move(fake_restart));
            restarted.initialize(window,renderer);
            check(restarted.state()==Menu::State::MainMenu &&
                  restarted.settings().profile==openemperor::simulation::RulesProfile::CityV11 &&
                  restarted.settings().prepared_starter,
                  "restart did not retain the selected profile and starter choice");
            click(restarted,90,310);
            click(restarted,90,560); finish_load(restarted);
            check(restarted.state()==Menu::State::Playing && restarted.sandbox()->paused() &&
                  !restarted.sandbox()->walker_visuals_active() &&
                  !restarted.sandbox()->building_visuals_active() &&
                  !restarted.sandbox()->road_visuals_active(),
                  "restart load failed or depended on a deleted optional visual profile");
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
