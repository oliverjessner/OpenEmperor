#pragma once
#include "app/MenuDialog.h"
#include "app/MenuStorage.h"
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "app/AutosaveController.h"
#include <SDL3/SDL.h>
#include <deque>
#include <memory>
#include <mutex>

namespace openemperor::menu {
class MenuSession {
public:
    enum class State { DataSetup, MainMenu, NewSandbox, LoadSandbox, Loading, Playing,
                       ConfirmLeave, ConfirmUpgrade, ConfirmDeleteRecovery };
    MenuSession(std::filesystem::path explicit_data={},std::filesystem::path app_root={},
                std::unique_ptr<DialogAdapter> dialog=std::make_unique<NativeDialog>(),
                std::filesystem::path resource_root={},
                maps::GreatWallPresentationMode great_wall=maps::GreatWallPresentationMode::Automatic);
    ~MenuSession();
    void initialize(SDL_Window* window,SDL_Renderer* renderer);
    void shutdown();
    void handle_event(const SDL_Event& event);
    void advance();
    bool render();
    void update(double seconds);
    bool running() const { return running_; }
    State state() const { return state_; }
    const SandboxView* sandbox() const { return sandbox_.get(); }
    SandboxView* sandbox() { return sandbox_.get(); }
    const Settings& settings() const { return settings_; }
    const std::string& message() const { return message_; }
    const std::filesystem::path& app_root() const { return app_root_; }
    const assets::CompatibilityResult& compatibility() const { return compatibility_; }
    VisualSelection visual_selection() const;
    struct DiagnosticButtonHit {
        int action;
        bool enabled;
        SDL_FPoint logical_point;
    };
    std::optional<SDL_FPoint> diagnostic_render_point(float window_x,float window_y) const;
    std::optional<DiagnosticButtonHit> diagnostic_button_hit(float window_x,float window_y) const;
    int diagnostic_pressed_action() const { return pressed_action_; }
    int diagnostic_pending_action() const { return pending_action_.value_or(-1); }
private:
    struct Inbox { std::mutex mutex; std::deque<std::pair<std::uint64_t,DialogResult>> results; };
    enum class DialogKind { None, Folder, SaveFile, VisualsFile, BuildingVisualsFile, RoadVisualsFile };
    enum class AfterConfirm { None, Quit, Replace, ChangeData };
    struct Button { SDL_FRect rect; std::string label; int action; bool enabled=true; };
    void open_dialog(DialogKind kind);
    void accept_data(const std::filesystem::path& path);
    void refresh_saves();
    void start_new();
    void start_load(const std::filesystem::path& path);
    void finish_loading();
    void commit_candidate();
    void confirm_or(AfterConfirm next);
    void perform(int action);
    void persist_settings();
    void record_save();
    void report_autosave(const AutosaveResult& result);
    void activate_button(int action);
    void rebuild_buttons();
    void set_state(State state);
    std::optional<SDL_FPoint> point(float x,float y) const;
    float menu_scale() const;
    std::filesystem::path explicit_data_,app_root_,pending_save_path_,resource_root_;
    std::filesystem::path candidate_map_;
    std::filesystem::path visual_profile_path_; // Session-only, never saved.
    std::filesystem::path building_profile_path_; // Session-only, never saved.
    std::filesystem::path road_profile_path_; // Session-only, never saved.
    simulation::RulesProfile candidate_profile_=simulation::RulesProfile::CityV11;
    std::unique_ptr<DialogAdapter> dialog_;
    std::shared_ptr<Inbox> inbox_=std::make_shared<Inbox>();
    std::uint64_t dialog_generation_=0,seen_save_generation_=0;
    DialogKind dialog_kind_=DialogKind::None;
    State state_=State::DataSetup,return_state_=State::MainMenu;
    State confirm_return_state_=State::MainMenu;
    AfterConfirm after_confirm_=AfterConfirm::None;
    Settings settings_;
    assets::CompatibilityResult compatibility_;
    maps::MapCatalog catalog_;
    SaveList saves_;
    persistence::RecoveryCatalog recoveries_;
    std::vector<persistence::RecoveryEntry> recovery_entries_;
    std::unique_ptr<SandboxView> sandbox_,candidate_;
    std::unique_ptr<AutosaveController> autosave_,candidate_autosave_;
    std::optional<persistence::RecoveryParent> pending_recovery_parent_;
    std::size_t map_index_=0,save_index_=0,map_scroll_=0,save_scroll_=0;
    bool demo_=true,running_=true,initialized_=false,loading_drawn_=false,settings_reset_required_=false;
    bool upgrade_copy_=false;
    std::uint32_t upgrade_target_version_=3;
    bool recovery_view_=false,candidate_sets_last_save_=false;
    bool advanced_visuals_open_=false;
    maps::GreatWallPresentationMode great_wall_mode_=maps::GreatWallPresentationMode::Automatic;
    int pressed_action_=-1;
    std::optional<int> pending_action_;
    std::vector<Button> buttons_;
    std::string message_;
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
};
}
