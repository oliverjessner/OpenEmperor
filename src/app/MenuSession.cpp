#include "app/MenuSession.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include "core/Version.h"
#include "app/ResourceLocator.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace openemperor::menu {
namespace fs=std::filesystem;
namespace {
enum Action { ChooseFolder=1, NewGame, LoadGame, Resume, DataFolder, Quit,
    MapPrev, MapNext, ProfilePrev, ProfileNext, Demo, Start, Back,
    SavePrev, SaveNext, OpenSave, ExternalSave, ConfirmSave, ConfirmDiscard, ConfirmCancel,
    ResetSettings, OpenMenu, VisualsFile, VisualsClear, BuildingVisualsFile, BuildingVisualsClear,
    RoadVisualsFile, RoadVisualsClear, AdvancedVisuals,
    MapSelectBase=1000, SaveSelectBase=2000 };
constexpr simulation::RulesProfile profiles[]={simulation::RulesProfile::LogisticsV1,
    simulation::RulesProfile::ProductionV2,simulation::RulesProfile::HouseholdV3,
    simulation::RulesProfile::SettlementV4,simulation::RulesProfile::IndustryV5,
    simulation::RulesProfile::CityV6};
const char* description(simulation::RulesProfile p) {
    switch (p) {
    case simulation::RulesProfile::LogisticsV1: return "Legacy prototype: goods delivery";
    case simulation::RulesProfile::ProductionV2: return "Legacy prototype: clay and pottery";
    case simulation::RulesProfile::HouseholdV3: return "Legacy prototype: one household";
    case simulation::RulesProfile::SettlementV4: return "Legacy prototype: four households";
    case simulation::RulesProfile::IndustryV5: return "Legacy alpha sandbox";
    case simulation::RulesProfile::CityV6: return "Playable city loop: money, workers and taxes";
    }
    return "";
}
std::string selection_label(const char* kind,const fs::path& path) {
    std::string value=path.empty()?"AUTO":path.filename().string();
    if (value.size()>18) value=value.substr(0,15)+"...";
    return std::string(kind)+": "+value;
}
std::string data_error(const std::exception& error) {
    return "That folder is not an installed or extracted Emperor data folder. "
           "Select the game folder, not the GOG installer file. Details: "+std::string(error.what());
}
}
MenuSession::MenuSession(fs::path explicit_data,fs::path app_root,std::unique_ptr<DialogAdapter> dialog,
                         fs::path resource_root)
    : explicit_data_(std::move(explicit_data)),app_root_(std::move(app_root)),
      resource_root_(std::move(resource_root)),dialog_(std::move(dialog)) {}
MenuSession::~MenuSession() { shutdown(); }
void MenuSession::initialize(SDL_Window* window,SDL_Renderer* renderer) {
    window_=window; renderer_=renderer;
    if (resource_root_.empty()) {
        const char* base=SDL_GetBasePath();
        if (base) resource_root_=locate_resource_root(base);
    }
    if (app_root_.empty()) {
        char* pref=SDL_GetPrefPath("OpenEmperor","OpenEmperor");
        if (!pref) throw std::runtime_error(std::string("SDL_GetPrefPath: ")+SDL_GetError());
        app_root_=pref; SDL_free(pref);
    }
    const auto read=read_settings(app_root_);
    settings_=read.value; settings_reset_required_=read.needs_reset;
    if (read.needs_reset) message_="Settings unavailable: "+read.message+". Confirm reset to save new settings.";
    const auto wanted=explicit_data_.empty()?settings_.data_root:explicit_data_;
    if (!wanted.empty()) {
        try { accept_data(wanted); }
        catch (const std::exception& e) { state_=State::DataSetup; message_=data_error(e); }
    }
    rebuild_buttons(); initialized_=true;
}
void MenuSession::shutdown() {
    if (dialog_) dialog_->cancel_pending();
    ++dialog_generation_; dialog_kind_=DialogKind::None;
    inbox_.reset();
    if (candidate_) { candidate_->shutdown(); candidate_.reset(); }
    if (sandbox_) { sandbox_->shutdown(); sandbox_.reset(); }
    window_=nullptr; renderer_=nullptr; initialized_=false;
}
void MenuSession::set_state(State state) {
    if (state_!=state && dialog_kind_!=DialogKind::None) {
        ++dialog_generation_; dialog_->cancel_pending(); dialog_kind_=DialogKind::None;
    }
    state_=state; pressed_action_=-1; pending_action_.reset(); rebuild_buttons();
}
void MenuSession::persist_settings() {
    if (settings_reset_required_) { message_="Confirm settings reset before saving preferences"; return; }
    try { write_settings(app_root_,settings_); }
    catch (const std::exception& e) { message_=std::string("Preferences could not be saved: ")+e.what(); }
}
void MenuSession::accept_data(const fs::path& path) {
    const auto found=validate_data_root(path);
    if (!settings_.data_root.empty() && settings_.data_root!=found.data_root) {
        visual_profile_path_.clear();building_profile_path_.clear();road_profile_path_.clear();
    }
    catalog_=found; settings_.data_root=found.data_root;
    compatibility_=assets::detect_compatibility(settings_.data_root,
        resource_root_.empty() ? fs::path{} : resource_root_/"Compatibility");
    auto it=std::find_if(catalog_.entries.begin(),catalog_.entries.end(),[&](const auto& e){
        return e.map_profile && e.relative_path==settings_.last_map; });
    if (it==catalog_.entries.end()) it=std::find_if(catalog_.entries.begin(),catalog_.entries.end(),
        [](const auto& e){return e.map_profile;});
    map_index_=static_cast<std::size_t>(it-catalog_.entries.begin());
    map_scroll_=0; set_state(State::MainMenu);
    const auto invalid=std::count_if(catalog_.entries.begin(),catalog_.entries.end(),
        [](const auto& e){return !e.map_profile;});
    message_=invalid ? std::to_string(invalid)+" map entries unreadable or unsupported" :
        compatibility_.compatible() ? "Compatible original data detected" :
        "Built-in visual preview unavailable for this data version";
    persist_settings();
}
VisualSelection MenuSession::visual_selection() const {
    return select_visual_profiles(compatibility_,visual_profile_path_,building_profile_path_,
                                  road_profile_path_);
}
void MenuSession::refresh_saves() {
    try { saves_=list_saves(app_root_); save_index_=0; save_scroll_=0; }
    catch (const std::exception& e) { saves_={}; message_="Save list could not be read: "+std::string(e.what()); }
}
void MenuSession::open_dialog(DialogKind kind) {
    if (dialog_kind_!=DialogKind::None) return;
    dialog_kind_=kind; const auto id=++dialog_generation_;
    std::weak_ptr<Inbox> weak=inbox_;
    auto callback=[weak,id](DialogResult result) {
        if (const auto inbox=weak.lock()) {
            std::lock_guard lock(inbox->mutex); inbox->results.emplace_back(id,std::move(result));
        }
    };
    try {
        if (kind==DialogKind::Folder) dialog_->open_folder(window_,std::move(callback));
        else if (kind==DialogKind::VisualsFile || kind==DialogKind::BuildingVisualsFile ||
                 kind==DialogKind::RoadVisualsFile)
            dialog_->open_visual_profile(window_,std::move(callback));
        else dialog_->open_file(window_,std::move(callback));
    } catch (const std::exception& e) { dialog_kind_=DialogKind::None; message_="File chooser could not open: "+std::string(e.what()); }
}
void MenuSession::start_new() {
    const auto& map=catalog_.entries.at(map_index_);
    if (!map.map_profile) { message_="Selected map is unsupported"; return; }
    pending_save_path_.clear();
    return_state_=State::NewSandbox; set_state(State::Loading); loading_drawn_=false;
}
void MenuSession::start_load(const fs::path& path) {
    std::error_code error;
    const auto resolved=fs::canonical(path,error);
    pending_save_path_=error ? path : resolved;
    return_state_=State::LoadSandbox; set_state(State::Loading); loading_drawn_=false;
}
void MenuSession::finish_loading() {
    if (state_!=State::Loading || !loading_drawn_) return;
    try {
        fs::path map; simulation::RulesProfile profile=settings_.profile;
        std::optional<persistence::SaveDocument> document;
        fs::path save_path=pending_save_path_;
        if (!save_path.empty()) {
            persistence::validate_save_target(save_path,settings_.data_root);
            document=persistence::read_save(save_path);
            map=document->map_relative; profile=document->world.profile;
        } else {
            map=catalog_.entries.at(map_index_).relative_path;
            save_path=new_save_target(app_root_,settings_.data_root);
        }
        auto loaded=maps::load_stored_map_session(settings_.data_root,map,
            maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
        auto view=std::make_unique<SandboxView>(std::move(loaded),document ? false:demo_,profile);
        view->set_managed(true);
        view->configure_save(settings_.data_root,map,save_path,std::move(document));
        const auto visuals=visual_selection();
        view->set_compatibility(visuals.compatibility.compatible() ?
            visuals.compatibility.profile->id:"unknown");
        if (!visuals.walker.empty()) view->set_walker_visuals(visuals.walker,visuals.walker_source);
        if (!visuals.building.empty()) view->set_building_visuals(visuals.building,visuals.building_source);
        if (!visuals.road.empty()) view->set_road_visuals(visuals.road,visuals.road_source);
        view->initialize(window_,renderer_);
        candidate_=std::move(view); candidate_map_=map; candidate_profile_=profile;
        if (sandbox_ && sandbox_->dirty()) confirm_or(AfterConfirm::Replace);
        else commit_candidate();
    } catch (const std::exception& e) {
        candidate_.reset();
        const auto subject=pending_save_path_.empty()?"Map or optional visual preview":"Save";
        message_=std::string(subject)+" could not be loaded: "+e.what()+". Check the selected file and try again.";
        set_state(return_state_);
    }
}
void MenuSession::commit_candidate() {
    if (!candidate_) return;
    if (sandbox_) { sandbox_->shutdown(); sandbox_.reset(); }
    sandbox_=std::move(candidate_); seen_save_generation_=sandbox_->save_generation();
    settings_.last_map=candidate_map_; settings_.profile=candidate_profile_;
    if (!pending_save_path_.empty()) settings_.last_save=sandbox_->save_path();
    persist_settings();
    set_state(State::Playing);
}
void MenuSession::confirm_or(AfterConfirm next) {
    if (sandbox_ && (sandbox_->dirty() || next==AfterConfirm::ChangeData)) {
        confirm_return_state_=state_==State::Playing ? State::Playing:State::MainMenu;
        after_confirm_=next; set_state(State::ConfirmLeave);
        message_=sandbox_->dirty() ? "Unsaved world progress. Save, discard, or cancel." :
            "Changing data folder discards this session. Save, discard, or cancel.";
    } else {
        after_confirm_=next; perform(ConfirmDiscard);
    }
}
void MenuSession::record_save() {
    if (!sandbox_) return;
    settings_.last_save=sandbox_->save_path();
    message_="Saved tick "+std::to_string(sandbox_->world().ticks());
    if (!settings_reset_required_) {
        try { write_settings(app_root_,settings_); }
        catch (const std::exception& e) { message_+="; preferences update failed: "+std::string(e.what()); }
    } else message_+="; preferences not saved until reset";
    seen_save_generation_=sandbox_->save_generation();
}
void MenuSession::perform(int action) {
    try {
        if (action>=SaveSelectBase) {
            const auto index=save_scroll_+static_cast<std::size_t>(action-SaveSelectBase);
            if (index<saves_.entries.size()) { save_index_=index; rebuild_buttons(); }
            return;
        }
        if (action>=MapSelectBase) {
            const auto index=map_scroll_+static_cast<std::size_t>(action-MapSelectBase);
            if (index<catalog_.entries.size()) { map_index_=index; rebuild_buttons(); }
            return;
        }
        switch (action) {
        case ChooseFolder: open_dialog(DialogKind::Folder); break;
        case NewGame: advanced_visuals_open_=false; set_state(State::NewSandbox); break;
        case LoadGame: advanced_visuals_open_=false; refresh_saves(); set_state(State::LoadSandbox); break;
        case Resume: if (sandbox_) set_state(State::Playing); break;
        case DataFolder: confirm_or(AfterConfirm::ChangeData); break;
        case Quit: confirm_or(AfterConfirm::Quit); break;
        case MapPrev: if (map_index_>0) --map_index_; rebuild_buttons(); break;
        case MapNext: if (map_index_+1<catalog_.entries.size()) ++map_index_; rebuild_buttons(); break;
        case ProfilePrev: case ProfileNext: {
            auto it=std::find(std::begin(profiles),std::end(profiles),settings_.profile);
            int index=static_cast<int>(it-std::begin(profiles));
            index=std::clamp(index+(action==ProfileNext?1:-1),0,
                static_cast<int>(std::size(profiles))-1);
            settings_.profile=profiles[index]; rebuild_buttons(); break;
        }
        case Demo: demo_=!demo_; rebuild_buttons(); break;
        case Start: start_new(); break;
        case Back: set_state(State::MainMenu); break;
        case SavePrev: if (save_index_>0) --save_index_; rebuild_buttons(); break;
        case SaveNext: if (save_index_+1<saves_.entries.size()) ++save_index_; rebuild_buttons(); break;
        case OpenSave: if (!saves_.entries.empty()) start_load(saves_.entries.at(save_index_).path); break;
        case ExternalSave: open_dialog(DialogKind::SaveFile); break;
        case VisualsFile: open_dialog(DialogKind::VisualsFile); break;
        case VisualsClear: visual_profile_path_.clear(); message_="Walker override cleared; automatic visuals selected";
            rebuild_buttons(); break;
        case BuildingVisualsFile: open_dialog(DialogKind::BuildingVisualsFile); break;
        case BuildingVisualsClear: building_profile_path_.clear();
            message_="Building override cleared; automatic visuals selected";rebuild_buttons();break;
        case RoadVisualsFile: open_dialog(DialogKind::RoadVisualsFile);break;
        case RoadVisualsClear: road_profile_path_.clear();
            message_="Road override cleared; automatic visuals selected";rebuild_buttons();break;
        case AdvancedVisuals: advanced_visuals_open_=!advanced_visuals_open_;rebuild_buttons();break;
        case ConfirmSave:
            if (sandbox_) { sandbox_->save_now(); record_save(); }
            [[fallthrough]];
        case ConfirmDiscard:
            if (after_confirm_==AfterConfirm::Quit) running_=false;
            else if (after_confirm_==AfterConfirm::Replace) commit_candidate();
            else if (after_confirm_==AfterConfirm::ChangeData) {
                if (sandbox_) { sandbox_->shutdown(); sandbox_.reset(); }
                set_state(State::DataSetup); message_="Choose your original data directory";
            }
            after_confirm_=AfterConfirm::None; break;
        case ConfirmCancel:
            if (candidate_) { candidate_->shutdown(); candidate_.reset(); }
            after_confirm_=AfterConfirm::None; set_state(sandbox_?confirm_return_state_:State::DataSetup); break;
        case ResetSettings:
            settings_reset_required_=false; write_settings(app_root_,settings_);
            message_="Preferences reset"; rebuild_buttons(); break;
        case OpenMenu: set_state(State::MainMenu); message_="Session retained in memory"; break;
        }
    } catch (const std::exception& e) {
        message_=(action==ConfirmSave ? "Save failed: ":"Action failed: ")+std::string(e.what());
    }
}
void MenuSession::activate_button(int action) { pending_action_=action; }
std::optional<SDL_FPoint> MenuSession::point(float x,float y) const {
    float rx=0,ry=0;
    if (!SDL_RenderCoordinatesFromWindow(renderer_,x,y,&rx,&ry)) return std::nullopt;
    const auto scale=menu_scale();
    return SDL_FPoint{rx/scale,ry/scale};
}
float MenuSession::menu_scale() const {
    return 1.5f*std::max(1.0f,SDL_GetWindowPixelDensity(window_));
}
void MenuSession::handle_event(const SDL_Event& event) {
    if (state_==State::Playing && sandbox_) {
        bool direct_running=true; sandbox_->handle_event(event,direct_running);
        if (sandbox_->take_menu_request()) {
            if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                pending_action_=Quit;
            else pending_action_=OpenMenu;
        }
        return;
    }
    if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        pending_action_=Quit; return;
    }
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key==SDLK_ESCAPE) {
            if (state_==State::ConfirmLeave) activate_button(ConfirmCancel);
            else if (state_==State::NewSandbox || state_==State::LoadSandbox) activate_button(Back);
            else if (state_!=State::Loading) activate_button(Quit);
        } else if (event.key.key==SDLK_RETURN || event.key.key==SDLK_KP_ENTER) {
            if (state_==State::NewSandbox) activate_button(Start);
            else if (state_==State::LoadSandbox) activate_button(OpenSave);
        } else if (event.key.key==SDLK_UP) {
            if (state_==State::NewSandbox) activate_button(MapPrev);
            else if (state_==State::LoadSandbox) activate_button(SavePrev);
        } else if (event.key.key==SDLK_DOWN) {
            if (state_==State::NewSandbox) activate_button(MapNext);
            else if (state_==State::LoadSandbox) activate_button(SaveNext);
        }
    }
    if (event.type==SDL_EVENT_MOUSE_WHEEL) {
        if (state_==State::NewSandbox) activate_button(event.wheel.y>0?MapPrev:MapNext);
        else if (state_==State::LoadSandbox) activate_button(event.wheel.y>0?SavePrev:SaveNext);
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT) {
        pressed_action_=-1;
        if (const auto p=point(event.button.x,event.button.y))
            for (const auto& b:buttons_) if (b.enabled && SDL_PointInRectFloat(&*p,&b.rect)) {
                pressed_action_=b.action; break;
            }
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
        const int pressed=pressed_action_; pressed_action_=-1;
        if (pressed<0) return;
        if (const auto p=point(event.button.x,event.button.y))
            for (const auto& b:buttons_) if (b.enabled && b.action==pressed &&
                SDL_PointInRectFloat(&*p,&b.rect)) { activate_button(pressed); break; }
    }
}
void MenuSession::advance() {
    if (!inbox_) return;
    std::deque<std::pair<std::uint64_t,DialogResult>> results;
    { std::lock_guard lock(inbox_->mutex); results.swap(inbox_->results); }
    for (auto& [id,result]:results) {
        if (id!=dialog_generation_ || dialog_kind_==DialogKind::None) continue;
        const auto kind=dialog_kind_; dialog_kind_=DialogKind::None;
        if (result.kind==DialogResult::Kind::Cancelled) { message_="Selection cancelled"; continue; }
        if (result.kind==DialogResult::Kind::Error) { message_="File chooser failed: "+result.path; continue; }
        try {
            if (kind==DialogKind::Folder) accept_data(result.path);
            else if (kind==DialogKind::VisualsFile || kind==DialogKind::BuildingVisualsFile ||
                     kind==DialogKind::RoadVisualsFile) {
                if (fs::path(result.path).extension()!=".json")
                    throw std::runtime_error("visual profile must be a .json file");
                if (kind==DialogKind::VisualsFile) visual_profile_path_=result.path;
                else if (kind==DialogKind::BuildingVisualsFile) building_profile_path_=result.path;
                else road_profile_path_=result.path;
                message_=kind==DialogKind::VisualsFile ?
                    "Walker profile selected for this session":
                    kind==DialogKind::BuildingVisualsFile ?
                    "Building profile selected for this session":"Road profile selected for this session";
                rebuild_buttons();
            } else start_load(result.path);
        } catch (const std::exception& e) {
            message_=kind==DialogKind::Folder ? data_error(e) :
                "Selected file could not be used: "+std::string(e.what());
        }
    }
    if (pending_action_) { const auto action=*pending_action_; pending_action_.reset(); perform(action); }
    if (sandbox_ && sandbox_->save_generation()!=seen_save_generation_) record_save();
    finish_loading();
}
void MenuSession::update(double seconds) {
    if (state_==State::Playing && sandbox_) sandbox_->update(seconds);
}
void MenuSession::rebuild_buttons() {
    buttons_.clear();
    auto add=[&](int action,std::string label,int x,int y,int w=180) {
        buttons_.push_back({SDL_FRect{static_cast<float>(x),static_cast<float>(y),
            static_cast<float>(w),32.0f},std::move(label),action});
    };
    if (state_==State::DataSetup) {
        add(ChooseFolder,"Choose Game Data Folder",40,135,230);
        if (settings_reset_required_) add(ResetSettings,"Reset preferences",40,185,210);
        add(Quit,"Quit",40,235);
        if (!catalog_.entries.empty()) add(Back,"Back",230,235);
    } else if (state_==State::MainMenu) {
        if (sandbox_) add(Resume,"Continue",40,100,210);
        add(NewGame,"New Sandbox",40,sandbox_?145:100,210);
        add(LoadGame,"Load Save",40,sandbox_?190:145,210);
        add(DataFolder,"Data folder",40,sandbox_?235:190,210);
        buttons_.back().label="Original Data Folder";
        add(Quit,"Quit",40,sandbox_?280:235,210);
        if (settings_reset_required_) add(ResetSettings,"Reset preferences",280,235,210);
    } else if (state_==State::NewSandbox) {
        add(MapPrev,"Previous map",40,120); add(MapNext,"Next map",230,120);
        add(ProfilePrev,"Previous rules",40,205); add(ProfileNext,"Next rules",230,205);
        add(Demo,demo_?"Demo: ON":"Demo: OFF",40,270);
        add(Start,"Start sandbox",40,315); add(Back,"Back",230,315);
        add(AdvancedVisuals,advanced_visuals_open_?"Hide advanced visuals":"Advanced...",40,360,210);
        if (advanced_visuals_open_) {
            add(VisualsFile,"Choose walker JSON...",40,400); add(VisualsClear,selection_label("Walker",visual_profile_path_),230,400);
            add(BuildingVisualsFile,"Building JSON...",40,440);
            add(BuildingVisualsClear,selection_label("Buildings",building_profile_path_),230,440);
            add(RoadVisualsFile,"Road JSON...",40,480);
            add(RoadVisualsClear,selection_label("Roads",road_profile_path_),230,480);
        }
        const auto begin=map_index_>4?map_index_-4:0;
        map_scroll_=begin;
        for (std::size_t i=begin;i<catalog_.entries.size() && i<begin+11;++i) {
            const auto& entry=catalog_.entries[i];
            std::string name=entry.relative_path.generic_string();
            if (name.size()>32) name=name.substr(0,29)+"...";
            buttons_.push_back({SDL_FRect{430.0f,95.0f+22.0f*static_cast<float>(i-begin),
                280.0f,20.0f},(i==map_index_?"> ":"  ")+name,
                MapSelectBase+static_cast<int>(i-begin),
                entry.map_profile});
        }
    } else if (state_==State::LoadSandbox) {
        add(SavePrev,"Previous save",40,120); add(SaveNext,"Next save",230,120);
        add(OpenSave,"Open selected",40,270); add(ExternalSave,"Open save file...",230,270);
        add(Back,"Back",40,315);
        add(AdvancedVisuals,advanced_visuals_open_?"Hide advanced visuals":"Advanced...",40,360,210);
        if (advanced_visuals_open_) {
            add(VisualsFile,"Choose walker JSON...",40,400); add(VisualsClear,selection_label("Walker",visual_profile_path_),230,400);
            add(BuildingVisualsFile,"Building JSON...",40,440);
            add(BuildingVisualsClear,selection_label("Buildings",building_profile_path_),230,440);
            add(RoadVisualsFile,"Road JSON...",40,480);
            add(RoadVisualsClear,selection_label("Roads",road_profile_path_),230,480);
        }
        const auto begin=save_index_>4?save_index_-4:0;
        save_scroll_=begin;
        for (std::size_t i=begin;i<saves_.entries.size() && i<begin+11;++i) {
            std::string name=saves_.entries[i].name;
            if (name.size()>32) name=name.substr(0,29)+"...";
            buttons_.push_back({SDL_FRect{430.0f,95.0f+22.0f*static_cast<float>(i-begin),
                280.0f,20.0f},(i==save_index_?"> ":"  ")+name,
                SaveSelectBase+static_cast<int>(i-begin)});
        }
    } else if (state_==State::ConfirmLeave) {
        const bool quitting=after_confirm_==AfterConfirm::Quit;
        add(ConfirmSave,quitting?"Save and Quit":"Save and Continue",40,160,210);
        add(ConfirmDiscard,quitting?"Quit Without Saving":"Discard Changes",40,205,210);
        add(ConfirmCancel,"Cancel",40,250,210);
    }
}
bool MenuSession::render() {
    if (state_==State::Playing && sandbox_) return sandbox_->render();
    int out_w=0,out_h=0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&out_w,&out_h)) return false;
    const auto scale=menu_scale();
    const int width=static_cast<int>(static_cast<float>(out_w)/scale);
    const int height=static_cast<int>(static_cast<float>(out_h)/scale);
    if (!SDL_SetRenderDrawColor(renderer_,18,27,40,255) || !SDL_RenderClear(renderer_) ||
        !SDL_SetRenderScale(renderer_,scale,scale)) return false;
    auto label=[&](int x,int y,const std::string& s)->bool {
        return SDL_SetRenderDrawColor(renderer_,230,237,244,255) &&
            SDL_RenderDebugText(renderer_,static_cast<float>(x),static_cast<float>(y),s.c_str());
    };
    const auto heading=state_==State::DataSetup?"OpenEmperor":
        state_==State::MainMenu?"OpenEmperor":state_==State::NewSandbox?"New sandbox":
        state_==State::LoadSandbox?"Load sandbox":state_==State::Loading?"Loading...":
        "Unsaved progress";
    if (!label(40,40,heading)) return false;
    if (state_==State::DataSetup) {
        if (!label(40,56,std::string(version::display)+" | Clean-room reimplementation")) return false;
        if (!label(40,72,"Original game data setup")) return false;
        if (!label(40,88,"OpenEmperor requires files from a legally obtained Emperor installation.")) return false;
        if (!label(40,103,"Do not select the GOG installer file itself.")) return false;
    } else if (state_==State::MainMenu) {
        if (!label(40,56,std::string(version::display))) return false;
        if (!label(40,70,"Experimental sandbox using your own Emperor game data")) return false;
        if (!label(280,105,"Original data: Ready")) return false;
        if (sandbox_ && !label(280,125,"Retained tick "+std::to_string(sandbox_->world().ticks())+
            (sandbox_->dirty()?" (unsaved)":" (saved)"))) return false;
    } else if (state_==State::NewSandbox && map_index_<catalog_.entries.size()) {
        const auto& e=catalog_.entries[map_index_];
        if (!label(40,85,"Map "+std::to_string(map_index_+1)+" / "+std::to_string(catalog_.entries.size())+
            ": "+e.relative_path.generic_string())) return false;
        if (!label(40,165,"Declared size: "+(e.declared_size?std::to_string(*e.declared_size):"unsupported")+
            (e.error.empty()?"":" - "+e.error))) return false;
        if (!label(40,245,std::string(simulation::rules_profile_name(settings_.profile))+" - "+description(settings_.profile))) return false;
        if (!label(40,345,compatibility_.compatible() ?
            "Visuals: Compatible preview detected":"Visuals: Diagnostic fallback")) return false;
        if (advanced_visuals_open_ && !label(40,385,"Advanced visual previews (developer overrides)")) return false;
    } else if (state_==State::LoadSandbox) {
        if (!label(40,85,"Saves: "+std::to_string(saves_.entries.size())+(saves_.truncated?" (list limited)":""))) return false;
        if (!saves_.entries.empty()) {
            const auto& e=saves_.entries[save_index_];
            if (!label(40,165,e.name+" | "+(e.error.empty()?e.map:e.error))) return false;
            if (e.error.empty() && !label(40,185,e.profile+" | tick "+std::to_string(e.tick)+
                " | schema "+std::to_string(e.schema)+" rule "+std::to_string(e.rule_version))) return false;
        }
        if (!label(40,345,compatibility_.compatible() ?
            "Visuals: Compatible preview detected":"Visuals: Diagnostic fallback")) return false;
        if (advanced_visuals_open_ && !label(40,385,"Advanced visual previews (developer overrides)")) return false;
    } else if (state_==State::Loading) {
        const auto item=pending_save_path_.empty() && map_index_<catalog_.entries.size()
            ? catalog_.entries[map_index_].relative_path.filename().string()
            : pending_save_path_.filename().string();
        if (!label(40,72,"Loading "+(item.empty()?std::string("sandbox"):item)+"...")) return false;
        if (!label(40,88,"Reading map and original graphics. This may take a moment.")) return false;
    } else if (state_==State::ConfirmLeave) {
        if (!label(40,80,"This sandbox has unsaved changes.")) return false;
        if (!label(40,96,"Choose whether to save before leaving the current session.")) return false;
    }
    for (const auto& b:buttons_) {
        if (!SDL_SetRenderDrawColor(renderer_,b.enabled?45:42,b.enabled?84:45,b.enabled?104:50,255) ||
            !SDL_RenderFillRect(renderer_,&b.rect) ||
            !label(static_cast<int>(b.rect.x)+8,static_cast<int>(b.rect.y)+11,b.label)) return false;
    }
    if (!message_.empty() && !label(40,std::max(25,height-34),message_.substr(0,static_cast<std::size_t>(std::max(0,width/8-12))))) return false;
    const std::string footer="OpenEmperor "+std::string(version::display)+" | "+std::string(version::target)+
        " | Clean-room reimplementation | rev "+std::string(version::revision)+
        (version::dirty?" dirty":"")+" | "+std::string(version::build_type);
    if (!label(40,std::max(12,height-18),footer.substr(0,static_cast<std::size_t>(std::max(0,width/8-12))))) return false;
    if (!SDL_SetRenderScale(renderer_,1,1)) return false;
    const bool okay=SDL_RenderPresent(renderer_);
    if (okay && state_==State::Loading) loading_drawn_=true;
    return okay;
}
}
