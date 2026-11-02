#pragma once
#include "app/MenuDialog.h"
#include "app/MenuStorage.h"
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "app/AutosaveController.h"
#include "app/MapRulesPreflight.h"
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
                maps::GreatWallPresentationMode great_wall=maps::GreatWallPresentationMode::Automatic,
                MapRulesPreflight::Checker map_checker=maps::check_map_rules);
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
    const maps::MapRulesResult& map_rules_result() const { return map_preflight_.result(); }
    bool map_rules_can_start() const;
    bool original_setup() const { return original_new_game_; }
    bool original_fire_protection_setup() const { return original_fire_protection_new_game_; }
    bool original_sustainable_economy_setup() const { return original_sustainable_economy_new_game_; }
    bool original_cardinal_water_setup() const { return original_cardinal_water_new_game_; }
    bool original_deadline_service_dispatch_setup() const { return original_deadline_service_dispatch_new_game_; }
    bool original_deadline_food_dispatch_setup() const { return original_deadline_food_dispatch_new_game_; }
    bool original_deadline_fire_dispatch_setup() const { return original_deadline_fire_dispatch_new_game_; }
    bool original_inspector_coordination_setup() const { return original_inspector_coordination_new_game_; }
    bool original_market_logistics_setup() const { return original_market_logistics_new_game_; }
    original::Difficulty original_difficulty() const { return original_difficulty_; }
    bool original_paid_starter() const { return original_paid_starter_; }
    std::string recovery_history_id() const { return autosave_ ? autosave_->history_id():std::string{}; }
    struct DiagnosticButtonHit {
        int action;
        bool enabled;
        SDL_FPoint logical_point;
    };
    std::optional<SDL_FPoint> diagnostic_render_point(float window_x,float window_y) const;
    std::optional<DiagnosticButtonHit> diagnostic_button_hit(float window_x,float window_y) const;
    std::optional<SDL_FRect> diagnostic_button_rect(const std::string& label) const;
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
    std::optional<maps::MapRulesRequest> selected_map_request() const;
    void check_selected_map();
    void start_new();
    void start_load(const std::filesystem::path& path);
    void finish_loading();
    void commit_candidate();
    void confirm_or(AfterConfirm next);
    void perform(int action);
    void persist_settings();
    void record_save();
    void record_load();
    AutosaveResult begin_original_autosave(std::optional<persistence::RecoveryParent> parent=std::nullopt);
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
    std::uint64_t seen_load_generation_=0;
    DialogKind dialog_kind_=DialogKind::None;
    State state_=State::DataSetup,return_state_=State::MainMenu;
    State confirm_return_state_=State::MainMenu;
    AfterConfirm after_confirm_=AfterConfirm::None;
    Settings settings_;
    bool ramps_new_game_=false; // Explicit rule6 setup, never a saved default or migration.
    bool bridges_new_game_=false; // Explicit setup choice, never a default/save upgrade.
    bool original_new_game_=false; // Session-only opt-in; never a RulesProfile/settings default.
    bool original_fire_protection_new_game_=false; // Explicit Rule11 session choice, never a default.
    bool original_sustainable_economy_new_game_=false; // Explicit Rule12, never persisted as a default.
    bool original_cardinal_water_new_game_=false; // Explicit Rule14, never a saved default.
    bool original_deadline_service_dispatch_new_game_=false; // Explicit Rule17, never a default or migration.
    bool original_deadline_food_dispatch_new_game_=false; // Explicit Rule18, never a default or migration.
    bool original_deadline_fire_dispatch_new_game_=false; // Explicit Rule16, never a default or migration.
    bool original_inspector_coordination_new_game_=false; // Explicit Rule15, never a default or migration.
    bool original_market_logistics_new_game_=false; // Explicit authored Rule13, never a default.
    original::Difficulty original_difficulty_=original::Difficulty::Normal;
    bool original_paid_starter_=true;
    MapRulesPreflight map_preflight_;
    std::optional<maps::MapRulesResult> starting_map_check_;
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
