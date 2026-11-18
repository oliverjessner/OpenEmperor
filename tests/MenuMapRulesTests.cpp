#include "app/MenuSession.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/LandscapeProvenance.h"
#include "maps/OriginalMapEntities.h"
#include "renderer/BuildingSprite.h"
#include "renderer/RoadSpriteSet.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "renderer/WalkerSpriteSet.h"

#include <SDL3/SDL.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace perf=openemperor::performance;
using Bytes=std::vector<std::uint8_t>;
using Menu=openemperor::menu::MenuSession;
using Status=openemperor::maps::MapRulesStatus;
using Request=openemperor::maps::MapRulesRequest;
using Result=openemperor::maps::MapRulesResult;
using Profile=openemperor::simulation::RulesProfile;
void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void u16(Bytes& bytes,std::size_t at,std::uint16_t value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);
    bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t at,std::uint32_t value) {
    u16(bytes,at,static_cast<std::uint16_t>(value));
    u16(bytes,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output),"authored fixture write failed");
}
Bytes sg3(bool tile) {
    Bytes bytes(40680U+(tile?203U:202U)*64U,0);
    u32(bytes,0,static_cast<std::uint32_t>(bytes.size())); u32(bytes,4,213);
    u32(bytes,12,tile?203:202); u32(bytes,16,tile?202:201); u32(bytes,20,1);
    const std::string group="Zeus_system.bmp";
    std::copy(group.begin(),group.end(),bytes.begin()+680);
    u32(bytes,680+124,200);u32(bytes,680+128,1);u32(bytes,680+132,200);
    if (tile) {
        const auto at=40680U+201U*64U;
        u32(bytes,at+4,3200);u32(bytes,at+8,3200);u16(bytes,at+20,78);u16(bytes,at+22,40);
        u16(bytes,at+50,30);bytes[at+55]=1;
    }
    return bytes;
}
enum class Manager { Empty, UnknownClass, Truncated, ResWall, ResWallUnknownOccupancy, ResWallTruncated, ResGate, ResGateUnknownOccupancy, ResGateTruncated, River, RiverUnknownOccupancy, RiverTruncated, EditorPoints, EditorPointsUnknownOccupancy, EditorPointsTruncated, CopperPoints, CopperPointsUnknownOccupancy, CopperPointsTruncated, TerrainFlagPoints, TerrainFlagPointsUnknownOccupancy, TerrainFlagPointsTruncated, InvasionPoints, InvasionPointsUnknownOccupancy, InvasionPointsTruncated, RemainingPoints, RemainingPointsUnknownOccupancy, RemainingPointsTruncated, DirtFerry, DirtFerryUnknownOccupancy, DirtFerryTruncated, OutsidePoints, OutsidePointsUnknownOccupancy, OutsidePointsTruncated };
Bytes map_file(Manager manager=Manager::Empty,std::uint8_t height=0,int source_type=0) {
    using namespace openemperor::maps;
    const std::string unknown_class="cUnauditedBuilding";
    const auto manager_length=manager==Manager::UnknownClass ? 12U+unknown_class.size():
        manager==Manager::Truncated ? 5U:6U;
    Bytes raw(static_cast<std::size_t>(original_entities_logical_offset)+manager_length,0);
    const auto manager_at=static_cast<std::size_t>(original_entities_logical_offset);
    u16(raw,manager_at,1);
    if (manager!=Manager::Truncated) u32(raw,manager_at+2,manager==Manager::UnknownClass ? 1:0);
    if (manager==Manager::UnknownClass) {
        u16(raw,manager_at+6,0xffff);u16(raw,manager_at+8,0);
        u16(raw,manager_at+10,static_cast<std::uint16_t>(unknown_class.size()));
        std::copy(unknown_class.begin(),unknown_class.end(),raw.begin()+static_cast<std::ptrdiff_t>(manager_at+12));
    }
    if (manager==Manager::ResWall || manager==Manager::ResWallUnknownOccupancy ||
        manager==Manager::ResWallTruncated || manager==Manager::ResGate ||
        manager==Manager::ResGateUnknownOccupancy || manager==Manager::ResGateTruncated ||
        manager==Manager::River || manager==Manager::RiverUnknownOccupancy || manager==Manager::RiverTruncated ||
        manager==Manager::EditorPoints || manager==Manager::EditorPointsUnknownOccupancy || manager==Manager::EditorPointsTruncated ||
        manager==Manager::CopperPoints || manager==Manager::CopperPointsUnknownOccupancy || manager==Manager::CopperPointsTruncated ||
        manager==Manager::TerrainFlagPoints || manager==Manager::TerrainFlagPointsUnknownOccupancy || manager==Manager::TerrainFlagPointsTruncated ||
        manager==Manager::InvasionPoints || manager==Manager::InvasionPointsUnknownOccupancy || manager==Manager::InvasionPointsTruncated || manager==Manager::RemainingPoints || manager==Manager::RemainingPointsUnknownOccupancy || manager==Manager::RemainingPointsTruncated || manager==Manager::DirtFerry || manager==Manager::DirtFerryUnknownOccupancy || manager==Manager::DirtFerryTruncated || manager==Manager::OutsidePoints || manager==Manager::OutsidePointsUnknownOccupancy || manager==Manager::OutsidePointsTruncated) {
        // Authored schema-4 Building + parent1 + fixed 128-byte NonHouseInfo
        // + the selected exact ResWall1/ResGate1 wrapper. The following
        // Unsupported status2 variants of Industrial179/164 remain negative
        // fixtures after their bounded status3 point states become supported.
        raw.resize(manager_at+6);
        const bool outside=manager==Manager::OutsidePoints || manager==Manager::OutsidePointsUnknownOccupancy || manager==Manager::OutsidePointsTruncated;
        const bool river=manager==Manager::River || manager==Manager::RiverUnknownOccupancy || manager==Manager::RiverTruncated;
        const bool editor=manager==Manager::EditorPoints || manager==Manager::EditorPointsUnknownOccupancy || manager==Manager::EditorPointsTruncated;
        const bool copper=manager==Manager::CopperPoints || manager==Manager::CopperPointsUnknownOccupancy || manager==Manager::CopperPointsTruncated;
        const bool flag=manager==Manager::TerrainFlagPoints || manager==Manager::TerrainFlagPointsUnknownOccupancy || manager==Manager::TerrainFlagPointsTruncated;
        const bool association=manager==Manager::DirtFerry || manager==Manager::DirtFerryUnknownOccupancy || manager==Manager::DirtFerryTruncated;
        const bool remaining=manager==Manager::RemainingPoints || manager==Manager::RemainingPointsUnknownOccupancy || manager==Manager::RemainingPointsTruncated;
        const bool invasion=manager==Manager::InvasionPoints || manager==Manager::InvasionPointsUnknownOccupancy || manager==Manager::InvasionPointsTruncated;
        u32(raw,manager_at+2,(river || editor || copper || flag || invasion || remaining || association || outside) ? ((manager==Manager::RiverUnknownOccupancy || manager==Manager::EditorPointsUnknownOccupancy || manager==Manager::CopperPointsUnknownOccupancy || manager==Manager::TerrainFlagPointsUnknownOccupancy || manager==Manager::InvasionPointsUnknownOccupancy || manager==Manager::RemainingPointsUnknownOccupancy || manager==Manager::DirtFerryUnknownOccupancy || manager==Manager::OutsidePointsUnknownOccupancy) ? 3:2):
            (manager==Manager::ResWallUnknownOccupancy || manager==Manager::ResGateUnknownOccupancy) ? 2:1);
        const auto append=[&](const std::string& name,unsigned type,unsigned side,unsigned id,int x,std::uint8_t status=3) {
            const auto tag=raw.size();raw.resize(tag+6+name.size());
            u16(raw,tag,0xffff);u16(raw,tag+4,static_cast<std::uint16_t>(name.size()));
            std::copy(name.begin(),name.end(),raw.begin()+static_cast<std::ptrdiff_t>(tag+6));
            const auto base=raw.size();raw.resize(base+311+((name=="cResWall" || name=="cResGate") ? 2:0)+(name=="cFerryBldg"?2010:0),0);
            u16(raw,base,4);raw[base+2]=status;raw[base+5]=static_cast<std::uint8_t>(side);
            u16(raw,base+8,static_cast<std::uint16_t>(outside ? (id==7?138:71):x));u16(raw,base+10,static_cast<std::uint16_t>(outside ? (id==7?117:81):38));
            u32(raw,base+12,outside ? (id==7?33455U:25180U):110U*228U+72U+static_cast<unsigned>(x));u16(raw,base+16,static_cast<std::uint16_t>(type));
            u32(raw,base+161,id);u16(raw,base+181,1);u16(raw,base+183,1);
            if(name=="cFerryBldg")u16(raw,base+311,1);
            if ((name=="cResWall" || name=="cResGate")) u16(raw,base+311,1);
        };
        const bool resgate=manager==Manager::ResGate || manager==Manager::ResGateUnknownOccupancy || manager==Manager::ResGateTruncated;
        if(outside){append("cIndustrialBldg",163,0,7,0);append("cIndustrialBldg",163,0,8,0);}
        else if(association){append("cIndustrialBldg",162,0,7,28);append("cFerryBldg",210,2,8,28);}
        else if (remaining) {append("cIndustrialBldg",unsigned(source_type),0,7,28);append("cIndustrialBldg",unsigned(source_type),0,8,30);}
        else if (river) {append("cIndustrialBldg",177,0,7,28);append("cIndustrialBldg",178,0,8,30);}
        else if(invasion) {append("cIndustrialBldg",173,0,7,28);append("cIndustrialBldg",173,0,8,30);}
        else if(flag) {append("cIndustrialBldg",151,0,7,28);append("cIndustrialBldg",151,0,8,30);}
        else if(copper) {append("cIndustrialBldg",186,0,7,28);append("cIndustrialBldg",186,0,8,30);}
        else if(editor) {append("cIndustrialBldg",179,0,7,28);append("cIndustrialBldg",164,0,8,30);}
        else append(resgate ? "cResGate":"cResWall",resgate ? 105:231,1,7,28);
        if (manager==Manager::ResWallUnknownOccupancy || manager==Manager::ResGateUnknownOccupancy) append("cIndustrialBldg",179,0,8,30,2);
        if (manager==Manager::RiverUnknownOccupancy) append("cIndustrialBldg",164,0,9,32,2);
        if(manager==Manager::OutsidePointsUnknownOccupancy)append("cIndustrialBldg",138,0,9,0,2);
        if(manager==Manager::DirtFerryUnknownOccupancy)append("cIndustrialBldg",138,0,9,32,2);
        if (manager==Manager::RemainingPointsUnknownOccupancy) append("cIndustrialBldg",unsigned(source_type),0,9,32,2);
        if (manager==Manager::InvasionPointsUnknownOccupancy) append("cIndustrialBldg",173,0,9,32,2);
        if (manager==Manager::TerrainFlagPointsUnknownOccupancy) append("cIndustrialBldg",151,0,9,32,2);
        if (manager==Manager::CopperPointsUnknownOccupancy) append("cIndustrialBldg",186,0,9,32,2);
        if (manager==Manager::EditorPointsUnknownOccupancy) append("cIndustrialBldg",188,0,9,32,2);
        if (manager==Manager::ResWallTruncated || manager==Manager::ResGateTruncated || manager==Manager::RiverTruncated || manager==Manager::EditorPointsTruncated || manager==Manager::CopperPointsTruncated || manager==Manager::TerrainFlagPointsTruncated || manager==Manager::InvasionPointsTruncated || manager==Manager::RemainingPointsTruncated || manager==Manager::DirtFerryTruncated || manager==Manager::OutsidePointsTruncated) raw.pop_back();
    }
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());u32(raw,84,(manager==Manager::OutsidePoints || manager==Manager::OutsidePointsUnknownOccupancy || manager==Manager::OutsidePointsTruncated) ? 170:84);
    for (std::size_t i=0;i<228U*228U;++i) {
        u32(raw,static_cast<std::size_t>(candidate_word_logical_offset)+4*i,0xc000);
        raw[static_cast<std::size_t>(candidate_byte_logical_offset)+i]=64;
        u32(raw,static_cast<std::size_t>(terrain_logical_offset)+4*i,0x80);
        raw[static_cast<std::size_t>(landscape_height_offset)+i]=height;
    }
    if(manager==Manager::OutsidePoints || manager==Manager::OutsidePointsUnknownOccupancy || manager==Manager::OutsidePointsTruncated)
        u32(raw,static_cast<std::size_t>(terrain_logical_offset)+(146U*228U+167U)*4,0x80005);
    Bytes file{0xaa,0xba,0xdc,0xfe};
    for (std::size_t at=0;at<raw.size();at+=32768) {
        const auto size=std::min<std::size_t>(32768,raw.size()-at);
        uLongf capacity=compressBound(static_cast<uLong>(size));Bytes compressed(static_cast<std::size_t>(capacity));
        check(compress2(compressed.data(),&capacity,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,
              "authored map compression failed");
        compressed.resize(static_cast<std::size_t>(capacity));
        const auto offset=file.size();file.resize(offset+12);
        u32(file,offset,0x12345678);u32(file,offset+4,static_cast<std::uint32_t>(compressed.size()));
        u32(file,offset+8,static_cast<std::uint32_t>(size));
        file.insert(file.end(),compressed.begin(),compressed.end());
    }
    return file;
}
void make_data(const fs::path& root,std::uint8_t height=0) {
    fs::create_directories(root/"DATA");fs::create_directories(root/"Cities");
    write(root/"DATA/China_Terrain.sg3",sg3(true));write(root/"DATA/China_Elevation.sg3",sg3(false));
    Bytes bitmap(3200);for (std::size_t i=0;i<bitmap.size();i+=2) u16(bitmap,i,0x03e0);
    write(root/"DATA/China_Terrain.555",bitmap);write(root/"DATA/China_Elevation.555",{});
    write(root/"Cities/A.map",map_file(Manager::Empty,height));
    write(root/"Cities/B.map",map_file(Manager::UnknownClass,height));
    write(root/"Cities/C.map",map_file(Manager::Truncated,height));
}
struct Temp {
    fs::path root=fs::temp_directory_path()/("openemperor-map-menu-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { make_data(root/"data-one");make_data(root/"data-two",1); }
    ~Temp() { std::error_code error;fs::remove_all(root,error); }
    fs::path app(std::string name,Profile profile=Profile::CityV16) const {
        openemperor::menu::Settings settings;
        settings.data_root=fs::canonical(root/"data-one");settings.last_map="Cities/A.map";
        settings.profile=profile;settings.prepared_starter=false;settings.autosave_enabled=false;
        const auto path=root/name;openemperor::menu::write_settings(path,settings);return path;
    }
};
class Dialog final:public openemperor::menu::DialogAdapter {
public:
    Callback callback;
    void open_folder(SDL_Window*,Callback value) override { callback=std::move(value); }
    void open_file(SDL_Window*,Callback value) override { callback=std::move(value); }
    void cancel_pending() override { callback={}; }
    void answer(const fs::path& path) {
        auto value=std::move(callback);check(static_cast<bool>(value),"data folder dialog is not open");
        value({openemperor::menu::DialogResult::Kind::Selected,path.string()});
    }
};
class ControlledChecks {
public:
    Result run(const Request& request) {
        {
            std::unique_lock lock(mutex_);
            const auto index=started_.size();started_.push_back(request);released_.push_back(completing_all_);
            changed_.notify_all();
            check(changed_.wait_for(lock,std::chrono::seconds(10),[&]{return released_.at(index);}),
                  "menu controlled checker was not completed");
        }
        return openemperor::maps::check_map_rules(request);
    }
    void wait_started(std::size_t count) {
        std::unique_lock lock(mutex_);
        check(changed_.wait_for(lock,std::chrono::seconds(10),[&]{return started_.size()>=count;}),
              "menu did not request selected map check");
    }
    void release(std::size_t index) {
        std::lock_guard lock(mutex_);released_.at(index)=true;changed_.notify_all();
    }
    Request started(std::size_t index) const { std::lock_guard lock(mutex_);return started_.at(index); }
    std::size_t size() const { std::lock_guard lock(mutex_);return started_.size(); }
    void release_all() {
        std::lock_guard lock(mutex_);completing_all_=true;
        std::fill(released_.begin(),released_.end(),true);changed_.notify_all();
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<Request> started_;
    std::vector<bool> released_;
    bool completing_all_=false;
};
// Declared after the menu so test failures unblock the checker before the menu
// joins it. This guard is test-only; production never detaches its worker.
struct CompleteOnExit {
    std::shared_ptr<ControlledChecks> checks;
    ~CompleteOnExit() { checks->release_all(); }
};
SDL_Event mouse(Uint32 type,float logical_x,float logical_y) {
    SDL_Event event{};event.type=type;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=logical_x*1.5f;event.button.y=logical_y*1.5f;return event;
}
void click(Menu& menu,float x,float y) {
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y));
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_UP,x,y));menu.advance();
}
void click_control(Menu& menu,SDL_Window* window,SDL_Renderer* renderer,const char* label) {
    check(menu.render(),"Render current menu before choosing a control");
    const auto rect=menu.diagnostic_button_rect(label);check(rect.has_value(),"Current menu control is missing");
    const float scale=1.5F*std::max(1.0F,SDL_GetWindowPixelDensity(window));float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,(rect->x+rect->w/2)*scale,(rect->y+rect->h/2)*scale,&x,&y),
          "Transform actual menu control center");
    const auto hit=menu.diagnostic_button_hit(x,y);check(hit && hit->enabled,"Rendered menu control has no enabled hit");
    auto down=mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,0,0);down.button.windowID=SDL_GetWindowID(window);down.button.x=x;down.button.y=y;
    menu.handle_event(down);down.type=SDL_EVENT_MOUSE_BUTTON_UP;menu.handle_event(down);menu.advance();
}
void key(Menu& menu,SDL_Keycode code) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;
    menu.handle_event(event);menu.advance();
}
void accept(Menu& menu,Status expected=Status::MapRulesChecked) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (menu.map_rules_result().status==Status::Checking) {
        menu.advance();check(std::chrono::steady_clock::now()<deadline,"menu did not adopt current completion");
        std::this_thread::yield();
    }
    if(menu.map_rules_result().status!=expected)throw std::runtime_error("menu adopted wrong map rules status: "+menu.map_rules_result().detail);
}
void finish_load(Menu& menu) {
    check(menu.state()==Menu::State::Loading,"approved start did not enter loading");
    check(menu.render(),"loading render failed");menu.advance();
}
using Files=std::map<fs::path,std::string>;
Files files(const fs::path& root) {
    Files result;
    for (const auto& entry:fs::recursive_directory_iterator(root)) if (entry.is_regular_file()) {
        std::ifstream input(entry.path(),std::ios::binary);
        result.emplace(fs::relative(entry.path(),root),std::string(std::istreambuf_iterator<char>(input),{}));
    }
    return result;
}
std::size_t textures() {
    return openemperor::StoredGraphicsRenderer::live_texture_count()+
        openemperor::BuildingSprite::live_texture_count()+openemperor::RoadSpriteSet::live_texture_count()+
        openemperor::WalkerSpriteSet::live_texture_count();
}
void assert_no_preflight_effects() {
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,
         perf::Counter::WorldExecutes,perf::Counter::SimulationTicks,perf::Counter::AssetDecodes,
         perf::Counter::TextureUploads,perf::Counter::FileWrites,perf::Counter::BfsCalls,
         perf::Counter::RouteRefreshes})
        check(perf::counter(counter)==0,"map preflight mutated gameplay, decoded assets or wrote files");
    check(textures()==0,"map preflight created textures");
}

void selection_and_input(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    auto controls=std::make_shared<ControlledChecks>();
    Menu menu({},temp.app("selection"),std::make_unique<Dialog>(),{},
              openemperor::maps::GreatWallPresentationMode::Automatic,
              [controls](const auto& selected){return controls->run(selected);});
    CompleteOnExit unblock{controls};menu.initialize(window,renderer);
    check(controls->size()==0,"menu eagerly parsed every catalog map");
    const auto before=files(menu.app_root());
    key(menu,SDLK_N);controls->wait_started(1);
    check(menu.state()==Menu::State::NewSandbox && !menu.map_rules_can_start(),"checking selection allowed start");
    perf::set_enabled(true);perf::reset();
    click(menu,50,325);key(menu,SDLK_RETURN);key(menu,SDLK_KP_ENTER);
    check(menu.state()==Menu::State::NewSandbox && !menu.sandbox(),"checking mouse or Enter started a session");
    const auto before_reads=perf::counter(perf::Counter::FileReads);
    for (int i=0;i<10;++i) { check(menu.render(),"checking status render failed");menu.update(0.05); }
    check(perf::counter(perf::Counter::FileReads)==before_reads && controls->size()==1,
          "render loop reread or rechecked map inputs");assert_no_preflight_effects();
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,240,325));
    check(menu.diagnostic_pressed_action()!=-1,"checking setup did not remain interactive");
    SDL_Event resized{};resized.type=SDL_EVENT_WINDOW_RESIZED;menu.handle_event(resized);
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_UP,240,325));menu.advance();
    check(menu.state()==Menu::State::NewSandbox && menu.diagnostic_pressed_action()==-1,
          "resize completed stale button press while checking");
    key(menu,SDLK_DOWN); // A -> unsupported B while A is blocked.
    controls->release(0);controls->wait_started(2);menu.advance();
    check(menu.map_rules_result().request.map_relative=="Cities/B.map" &&
          menu.map_rules_result().status==Status::Checking && !menu.map_rules_can_start(),
          "late positive A completion enabled B");
    click(menu,50,215); // City v16 -> City v15 while B's rule-3 check is blocked.
    controls->release(1);controls->wait_started(3);menu.advance();
    check(menu.map_rules_result().request.profile==Profile::CityV15 &&
          menu.map_rules_result().request.policy_version==0 &&
          menu.map_rules_result().status==Status::Checking,
          "late rule-3 unsupported result crossed profile boundary");
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,240,325));
    controls->release(2);accept(menu);
    check(menu.map_rules_can_start() && menu.diagnostic_pressed_action()==-1,
          "accepted result did not disarm old press or respect legacy rules");
    menu.handle_event(mouse(SDL_EVENT_MOUSE_BUTTON_UP,240,325));menu.advance();
    check(menu.state()==Menu::State::NewSandbox,"status change completed click-through action");
    click(menu,240,215);controls->wait_started(4);controls->release(3);accept(menu,Status::UnsupportedForRules);
    check(!menu.map_rules_can_start() && menu.map_rules_result().detail.find("cUnauditedBuilding")!=std::string::npos,
          "unsupported original class reason was hidden or start was enabled");
    click(menu,50,325);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox && !menu.sandbox() && menu.render(),
          "unsupported map mouse or Enter escaped central start condition");
    key(menu,SDLK_DOWN);controls->wait_started(5);controls->release(4);accept(menu,Status::InputError);
    check(menu.map_rules_result().detail.find("truncated")!=std::string::npos && !menu.map_rules_can_start(),
          "truncated manager was classified as permanently unsupported");
    assert_no_preflight_effects();
    check(files(menu.app_root())==before,"checking created saves or recovery histories");
    perf::set_enabled(false);menu.shutdown();
}

void roots_and_setup_lifetime(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    auto controls=std::make_shared<ControlledChecks>();
    auto dialog=std::make_unique<Dialog>();auto* chooser=dialog.get();
    Menu menu({},temp.app("roots"),std::move(dialog),{},openemperor::maps::GreatWallPresentationMode::Automatic,
              [controls](const auto& selected){return controls->run(selected);});
    CompleteOnExit unblock{controls};menu.initialize(window,renderer);
    key(menu,SDLK_N);controls->wait_started(1);key(menu,SDLK_ESCAPE);
    check(menu.state()==Menu::State::MainMenu && menu.map_rules_result().status==Status::Unchecked,
          "leaving setup retained pending authorization");
    click_control(menu,window,renderer,"Original Data Folder");
    check(menu.state()==Menu::State::DataSetup,"data root change did not open setup");
    click(menu,50,145);chooser->answer(temp.root/"data-two");menu.advance();
    check(menu.settings().data_root==fs::canonical(temp.root/"data-two"),"second data root not accepted");
    key(menu,SDLK_N);controls->release(0);controls->wait_started(2);menu.advance();
    check(menu.map_rules_result().request.canonical_data_root==fs::canonical(temp.root/"data-two") &&
          !menu.map_rules_can_start() && menu.map_rules_result().status==Status::Checking,
          "old-root result authorized same-name map in new root");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);
    controls->release(1);controls->wait_started(3);menu.advance();
    check(menu.map_rules_result().status==Status::Checking && !menu.map_rules_can_start(),
          "same selection from closed setup crossed new setup lifetime");
    controls->release(2);accept(menu);
    check(menu.map_rules_can_start(),"reopened setup did not accept fresh root-specific check");
    click(menu,450,325);controls->wait_started(4);
    controls->release(3);menu.shutdown();
    check(!menu.map_rules_can_start() && menu.map_rules_result().status==Status::Unchecked && !menu.sandbox(),
          "result after menu shutdown changed menu authorization or session");
}

void explicit_bridge_setup(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    const auto app=temp.app("explicit-bridges");
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    key(menu,SDLK_N);accept(menu);
    check(menu.map_rules_result().request.rule_version==4 &&
          menu.map_rules_result().request.policy_version==2,"new menu default no longer uses rule4/policy2");
    click(menu,440,215);accept(menu);
    check(menu.map_rules_result().request.rule_version==5 &&
          menu.map_rules_result().request.policy_version==3 && menu.map_rules_can_start(),
          "explicit Bridge choice did not check rule5/policy3");
    check(menu.render(),"Bridge setup did not render");
    click(menu,440,215);accept(menu);
    check(menu.map_rules_result().request.rule_version==4 &&
          menu.map_rules_result().request.policy_version==2,"disabling Bridge choice did not restore rule4/policy2");
    click(menu,440,215);accept(menu);key(menu,SDLK_RETURN);finish_load(menu);
    check(menu.sandbox() && menu.sandbox()->world().bridges_supported() &&
          menu.sandbox()->world().rule_version()==5 && menu.sandbox()->world().map_permissions()->policy_version()==3,
          "checked Bridge setup did not create the requested rule5 World");
    key(menu,SDLK_F5);
    const auto save=menu.sandbox()->save_path();
    const auto document=openemperor::persistence::read_save(save);
    check(document.source_schema_version==21 && document.world.rule_version==5 &&
          document.map_permissions_policy_version==3,"normal menu F5 did not preserve Bridge version authority");
    menu.shutdown();check(textures()==0,"Bridge setup leaked textures");
    Menu restarted({},app,std::make_unique<Dialog>());restarted.initialize(window,renderer);
    key(restarted,SDLK_N);accept(restarted);
    check(restarted.map_rules_result().request.rule_version==4 &&
          restarted.map_rules_result().request.policy_version==2,"session-only Bridge choice became a stored default");
    key(restarted,SDLK_ESCAPE);key(restarted,SDLK_L);key(restarted,SDLK_RETURN);finish_load(restarted);
    check(restarted.sandbox() && restarted.sandbox()->world().snapshot()==document.world &&
          restarted.sandbox()->world().bridges_supported(),"normal menu load downgraded a rule5 save");
    restarted.shutdown();check(textures()==0,"Bridge load leaked textures");
}

void content_retry_and_resume(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    auto controls=std::make_shared<ControlledChecks>();
    Menu menu({},temp.app("resume"),std::make_unique<Dialog>(),{},openemperor::maps::GreatWallPresentationMode::Automatic,
              [controls](const auto& selected){return controls->run(selected);});
    CompleteOnExit unblock{controls};menu.initialize(window,renderer);
    key(menu,SDLK_N);controls->wait_started(1);controls->release(0);accept(menu);
    const auto original_hash=menu.map_rules_result().input_sha256;
    fs::remove(temp.root/"data-one/Cities/A.map");
    click(menu,450,325);controls->wait_started(2);controls->release(1);accept(menu,Status::InputError);
    check(!menu.map_rules_can_start() && !menu.sandbox() && menu.render(),
          "disappeared source was authorized or its retry status was not reachable");
    write(temp.root/"data-one/Cities/A.map",map_file(Manager::Empty,1));
    click(menu,450,325);controls->wait_started(3);
    check(!menu.map_rules_can_start(),"content recheck reused earlier approval");
    controls->release(2);accept(menu);
    check(menu.map_rules_result().input_sha256!=original_hash,"changed source content retained old hash");
    key(menu,SDLK_RETURN);finish_load(menu);
    check(menu.state()==Menu::State::Playing && menu.sandbox() &&
          menu.sandbox()->world().rule_version()==4 && menu.sandbox()->world().map_permissions() &&
          menu.sandbox()->world().map_permissions()->policy_version()==2,
          "positive map-rules result did not allow normal rule-4/policy-2 initialization");
    auto* const retained=menu.sandbox();
    const auto before=retained->world().snapshot();
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);controls->wait_started(4);
    key(menu,SDLK_DOWN);controls->release(3);controls->wait_started(5);
    controls->release(4);accept(menu,Status::UnsupportedForRules);
    check(menu.sandbox()==retained && retained->world().snapshot()==before,
          "unsupported selection replaced or mutated retained city");
    key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox,"unsupported start entered initialization");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==before,"Resume lost retained session after blocked selection");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);controls->wait_started(6);
    // New setup initially reselects A, the last actually started map.
    if (controls->started(5).map_relative!="Cities/A.map") {
        key(menu,SDLK_UP);controls->release(5);controls->wait_started(7);
        controls->release(6);accept(menu);
    } else { controls->release(5);accept(menu); }
    write(temp.root/"data-one/Cities/A.map",map_file(Manager::Empty,2));
    const auto started_checks=controls->size();
    key(menu,SDLK_RETURN);perf::set_enabled(true);perf::reset();finish_load(menu);
    check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
          retained->world().snapshot()==before && menu.message().find("changed")!=std::string::npos,
          "changed-after-check source did not reject start while retaining city");
    check(perf::counter(perf::Counter::AssetDecodes)==0 && perf::counter(perf::Counter::TextureUploads)==0 &&
          perf::counter(perf::Counter::WorldExecutes)==0 && perf::counter(perf::Counter::FileWrites)==0,
          "changed-before-start input reached expensive or side-effecting initialization");
    perf::set_enabled(false);
    controls->wait_started(started_checks+1);controls->release(started_checks);accept(menu);
    key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==before,"Resume lost retained city after later start failure");
    menu.shutdown();check(textures()==0,"menu map-rules session leaked textures");
}

void reswall_atomic_preflight(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    write(temp.root/"data-one/Cities/D.map",map_file(Manager::ResWall));
    write(temp.root/"data-one/Cities/E.map",map_file(Manager::ResWallUnknownOccupancy));
    write(temp.root/"data-one/Cities/F.map",map_file(Manager::ResWallTruncated));
    const auto app=temp.app("reswall");
    auto settings=openemperor::menu::read_settings(app).value;
    settings.last_map="Cities/D.map";openemperor::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    key(menu,SDLK_N);accept(menu);check(menu.map_rules_can_start(),"complete authored ResWall policy cannot start");
    key(menu,SDLK_RETURN);finish_load(menu);
    auto* const retained=menu.sandbox();check(retained,"authored ResWall session absent");
    check(retained->world().map_permissions()->protected_original({100,110}) &&
          !retained->world().map_permissions()->road_allowed({100,110}),"typed ResWall singleton not protected in session");
    const auto snapshot=retained->world().snapshot();const auto policy=retained->world().map_permissions();
    const auto persisted=files(app);
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);accept(menu);key(menu,SDLK_DOWN);accept(menu,Status::UnsupportedForRules);
    check(menu.map_rules_result().detail.find("type 179")!=std::string::npos && !menu.map_rules_can_start(),
          "complete ResWall prefix hid later unknown occupancy");
    key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && retained->world().map_permissions()==policy,
          "failed ResWall map replaced full working session or policy");
    key(menu,SDLK_DOWN);accept(menu,Status::InputError);
    check(menu.map_rules_result().detail.find("truncated")!=std::string::npos && !menu.map_rules_can_start(),
          "truncated ResWall wrapper was accepted or misclassified");
    key(menu,SDLK_RETURN);key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && files(app)==persisted,
          "Resume or rejected ResWall selection changed city or persistent files");
    menu.shutdown();check(textures()==0,"ResWall menu test leaked textures");
}
void resgate_atomic_preflight(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    write(temp.root/"data-one/Cities/D.map",map_file(Manager::ResGate));
    write(temp.root/"data-one/Cities/E.map",map_file(Manager::ResGateUnknownOccupancy));
    write(temp.root/"data-one/Cities/F.map",map_file(Manager::ResGateTruncated));
    const auto app=temp.app("resgate");
    auto settings=openemperor::menu::read_settings(app).value;
    settings.last_map="Cities/D.map";openemperor::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    key(menu,SDLK_N);accept(menu);check(menu.map_rules_can_start(),"complete authored ResGate policy cannot start");
    key(menu,SDLK_RETURN);finish_load(menu);
    auto* const retained=menu.sandbox();check(retained,"authored ResGate session absent");
    check(retained->world().map_permissions()->protected_original({100,110}) &&
          !retained->world().map_permissions()->road_allowed({100,110}),"typed ResGate singleton not protected in session");
    const auto snapshot=retained->world().snapshot();const auto policy=retained->world().map_permissions();
    const auto persisted=files(app);
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);accept(menu);key(menu,SDLK_DOWN);accept(menu,Status::UnsupportedForRules);
    check(menu.map_rules_result().detail.find("type 179")!=std::string::npos && !menu.map_rules_can_start(),
          "complete ResGate prefix hid later unknown occupancy");
    key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && retained->world().map_permissions()==policy,
          "failed ResGate map replaced full working session or policy");
    key(menu,SDLK_DOWN);accept(menu,Status::InputError);
    check(menu.map_rules_result().detail.find("truncated")!=std::string::npos && !menu.map_rules_can_start(),
          "truncated ResGate wrapper was accepted or misclassified");
    key(menu,SDLK_RETURN);key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && files(app)==persisted,
          "Resume or rejected ResGate selection changed city or persistent files");
    menu.shutdown();check(textures()==0,"ResGate menu test leaked textures");
}
void point_atomic_preflight(Temp& temp,SDL_Window* window,SDL_Renderer* renderer,bool editor,bool copper=false,bool flag=false,bool invasion=false,int source_type=0) {
    write(temp.root/"data-one/Cities/D.map",map_file(source_type ? Manager::RemainingPoints:invasion ? Manager::InvasionPoints:flag ? Manager::TerrainFlagPoints:copper ? Manager::CopperPoints:editor ? Manager::EditorPoints:Manager::River,0,source_type));
    write(temp.root/"data-one/Cities/E.map",map_file(source_type ? Manager::RemainingPointsUnknownOccupancy:invasion ? Manager::InvasionPointsUnknownOccupancy:flag ? Manager::TerrainFlagPointsUnknownOccupancy:copper ? Manager::CopperPointsUnknownOccupancy:editor ? Manager::EditorPointsUnknownOccupancy:Manager::RiverUnknownOccupancy,0,source_type));
    write(temp.root/"data-one/Cities/F.map",map_file(source_type ? Manager::RemainingPointsTruncated:invasion ? Manager::InvasionPointsTruncated:flag ? Manager::TerrainFlagPointsTruncated:copper ? Manager::CopperPointsTruncated:editor ? Manager::EditorPointsTruncated:Manager::RiverTruncated,0,source_type));
    const auto app=temp.app(source_type ? "remaining-"+std::to_string(source_type):invasion ? "invasion-points":flag ? "terrain-flag-points":copper ? "copper-points":editor ? "editor-points":"river");
    auto settings=openemperor::menu::read_settings(app).value;
    settings.last_map="Cities/D.map";openemperor::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    key(menu,SDLK_N);accept(menu);check(menu.map_rules_can_start(),"complete river marker map cannot start");
    key(menu,SDLK_RETURN);finish_load(menu);auto* const retained=menu.sandbox();
    check(retained && retained->world().rule_version()==4 && retained->world().map_permissions()->policy_version()==2,
          "river marker session must use current rule4/policy2");
    for (const openemperor::simulation::Cell cell:std::array{openemperor::simulation::Cell{100,110},openemperor::simulation::Cell{102,110}})
        check(retained->world().map_permissions()->protected_original(cell) &&
              !retained->world().map_permissions()->road_allowed(cell),"both river origins must remain protected");
    const auto snapshot=retained->world().snapshot();const auto policy=retained->world().map_permissions();
    const auto persisted=files(app);
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);accept(menu);key(menu,SDLK_DOWN);accept(menu,Status::UnsupportedForRules);
    check(menu.map_rules_result().detail.find(source_type ? "type "+std::to_string(source_type):invasion ? "type 173":flag ? "type 151":copper ? "type 186":editor ? "type 188":"type 164")!=std::string::npos && !menu.map_rules_can_start(),
          "valid river markers must not hide a later unknown source");
    key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && retained->world().map_permissions()==policy,
          "failed river candidate replaced retained complete session");
    key(menu,SDLK_DOWN);accept(menu,Status::InputError);key(menu,SDLK_RETURN);
    check(menu.map_rules_result().detail.find("truncated")!=std::string::npos && !menu.map_rules_can_start(),
          "truncated river state must remain an input error");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && files(app)==persisted,"river rejection/Resume changed previous World or files");
    menu.shutdown();check(textures()==0,"river menu test leaked textures");
}
void association_atomic_preflight(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    write(temp.root/"data-one/Cities/D.map",map_file(Manager::DirtFerry));
    write(temp.root/"data-one/Cities/E.map",map_file(Manager::DirtFerryUnknownOccupancy));
    write(temp.root/"data-one/Cities/F.map",map_file(Manager::DirtFerryTruncated));
    const auto app=temp.app("dirt-ferry-association");
    auto settings=openemperor::menu::read_settings(app).value;
    settings.last_map="Cities/D.map";openemperor::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    key(menu,SDLK_N);accept(menu);check(menu.map_rules_can_start(),"complete river marker map cannot start");
    key(menu,SDLK_RETURN);finish_load(menu);auto* const retained=menu.sandbox();
    check(retained && retained->world().rule_version()==4 && retained->world().map_permissions()->policy_version()==2,
          "river marker session must use current rule4/policy2");
    for (const openemperor::simulation::Cell cell:std::array{openemperor::simulation::Cell{100,110},openemperor::simulation::Cell{101,111}})
        check(retained->world().map_permissions()->protected_original(cell) &&
              !retained->world().map_permissions()->road_allowed(cell),"both river origins must remain protected");
    const auto snapshot=retained->world().snapshot();const auto policy=retained->world().map_permissions();
    const auto persisted=files(app);
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);accept(menu);key(menu,SDLK_DOWN);accept(menu,Status::UnsupportedForRules);
    check(menu.map_rules_result().detail.find("type 138")!=std::string::npos && !menu.map_rules_can_start(),
          "valid river markers must not hide a later unknown source");
    key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && retained->world().map_permissions()==policy,
          "failed river candidate replaced retained complete session");
    key(menu,SDLK_DOWN);accept(menu,Status::InputError);key(menu,SDLK_RETURN);
    check(menu.map_rules_result().detail.find("truncated")!=std::string::npos && !menu.map_rules_can_start(),
          "truncated river state must remain an input error");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && files(app)==persisted,"river rejection/Resume changed previous World or files");
    menu.shutdown();check(textures()==0,"river menu test leaked textures");
}
void outside_atomic_preflight(Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    write(temp.root/"data-one/Cities/D.map",map_file(Manager::OutsidePoints));
    write(temp.root/"data-one/Cities/E.map",map_file(Manager::OutsidePointsUnknownOccupancy));
    write(temp.root/"data-one/Cities/F.map",map_file(Manager::OutsidePointsTruncated));
    const auto app=temp.app("outside-sources");
    auto settings=openemperor::menu::read_settings(app).value;
    settings.last_map="Cities/D.map";openemperor::menu::write_settings(app,settings);
    Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
    key(menu,SDLK_N);accept(menu);check(menu.map_rules_can_start(),"complete river marker map cannot start");
    key(menu,SDLK_RETURN);finish_load(menu);auto* const retained=menu.sandbox();
    check(retained && retained->world().rule_version()==4 && retained->world().map_permissions()->policy_version()==2,
          "river marker session must use current rule4/policy2");
    const auto permissions=retained->world().map_permissions();
    check(permissions->protected_original({100,110}) && !permissions->road_allowed({100,110}),
          "inside marker retains its exact conservative protection");
    check(!permissions->protected_original({167,146}) && !permissions->road_allowed({167,146}) &&
          !permissions->building_allowed({167,146}) && permissions->road_blocker({167,146})==openemperor::simulation::BuildBlocker::OutsideMap,
          "normal menu must never create playable outside occupancy");
    check(retained->world().snapshot().buildings.empty() && retained->world().snapshot().couriers.empty(),
          "historical sources add no editable World entities");
    const auto snapshot=retained->world().snapshot();const auto policy=retained->world().map_permissions();
    const auto persisted=files(app);
    key(menu,SDLK_ESCAPE);key(menu,SDLK_N);accept(menu);key(menu,SDLK_DOWN);accept(menu,Status::UnsupportedForRules);
    check(menu.map_rules_result().detail.find("type 138")!=std::string::npos && !menu.map_rules_can_start(),
          "valid river markers must not hide a later unknown source");
    key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::NewSandbox && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && retained->world().map_permissions()==policy,
          "failed river candidate replaced retained complete session");
    key(menu,SDLK_DOWN);accept(menu,Status::InputError);key(menu,SDLK_RETURN);
    check(menu.map_rules_result().detail.find("truncated")!=std::string::npos && !menu.map_rules_can_start(),
          "truncated river state must remain an input error");
    key(menu,SDLK_ESCAPE);key(menu,SDLK_RETURN);
    check(menu.state()==Menu::State::Playing && menu.sandbox()==retained &&
          retained->world().snapshot()==snapshot && files(app)==persisted,"river rejection/Resume changed previous World or files");
    menu.shutdown();check(textures()==0,"river menu test leaked textures");
}
}

int main() {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"SDL dummy initialization failed");
        SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("map selection test",1100,800,SDL_WINDOW_RESIZABLE,&window,&renderer),
              "SDL authored test window failed");
        Temp temp;
        selection_and_input(temp,window,renderer);
        roots_and_setup_lifetime(temp,window,renderer);
        explicit_bridge_setup(temp,window,renderer);
        content_retry_and_resume(temp,window,renderer);
        reswall_atomic_preflight(temp,window,renderer);
        resgate_atomic_preflight(temp,window,renderer);
        point_atomic_preflight(temp,window,renderer,false);
        point_atomic_preflight(temp,window,renderer,true);
        point_atomic_preflight(temp,window,renderer,false,true);
        point_atomic_preflight(temp,window,renderer,false,false,true);
        point_atomic_preflight(temp,window,renderer,false,false,false,true);
        for(int source_type:{187,189,138,181,188,167})
            point_atomic_preflight(temp,window,renderer,false,false,false,false,source_type);
        association_atomic_preflight(temp,window,renderer);
        outside_atomic_preflight(temp,window,renderer);
        check(textures()==0,"map selection tests left prepared textures alive");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"production menu map-rules selection, concurrency, purity, input, retry and Resume checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
