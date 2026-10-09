#include "app/SandboxView.h"
#include "maps/LandscapeProvenance.h"
#include "maps/RegeneratedMapRenderPlan.h"

#include "maps/SandboxPlacement.h"
#include "renderer/StoredCamera.h"
#include "renderer/WellFallbackRenderer.h"
#include "renderer/HealthPostFallbackRenderer.h"
#include "app/WalkerPose.h"
#include "app/SandboxVisualOrder.h"
#include "core/Version.h"
#include "simulation/CityStartGuidance.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor {
namespace {
// Presentation palette: Poor, Neutral, Good. Simulation owns category thresholds.
constexpr std::array<SDL_Color,3> housing_palette{{{210,83,75,45},
    {204,169,81,45},{78,174,128,45}}};
bool food_market_visual(std::optional<assets::WalkerVisualRole> role) {
    return role==assets::WalkerVisualRole::Supplier || role==assets::WalkerVisualRole::Distributor;
}
const char* courier_role_label(simulation::CourierRole role) {
    switch (role) {
    case simulation::CourierRole::Clay: return "Clay";
    case simulation::CourierRole::Pottery: return "Pottery";
    case simulation::CourierRole::Household: return "Household";
    case simulation::CourierRole::Food: return "Food";
    case simulation::CourierRole::Service: return "Service";
    case simulation::CourierRole::MarketPotteryInbound: return "MarketPotteryInbound";
    case simulation::CourierRole::MarketFoodInbound: return "MarketFoodInbound";
    case simulation::CourierRole::MarketPotteryDistribution: return "MarketPotteryDistribution";
    case simulation::CourierRole::MarketFoodDistribution: return "MarketFoodDistribution";
    case simulation::CourierRole::FireInspector: return "FireInspector";
    case simulation::CourierRole::HealthWorker: return "HealthWorker";
    case simulation::CourierRole::None: return "None";
    }
    return "Unknown";
}
const char* courier_good_label(simulation::Good good) {
    switch (good) {
    case simulation::Good::Clay: return "Clay";
    case simulation::Good::Pottery: return "Pottery";
    case simulation::Good::Food: return "Food";
    case simulation::Good::Goods: return "Goods";
    }
    return "Unknown";
}
const char* storage_direction_label(assets::StorageDirection direction) {
    switch (direction) {
    case assets::StorageDirection::PosX: return "pos_x";
    case assets::StorageDirection::NegX: return "neg_x";
    case assets::StorageDirection::PosY: return "pos_y";
    case assets::StorageDirection::NegY: return "neg_y";
    }
    return "unknown";
}
std::optional<assets::StorageDirection> current_storage_direction(
        const simulation::CourierState& courier) {
    if (courier.path.size()<2 || courier.path_vertex>=courier.path.size()-1) return std::nullopt;
    const auto from=courier.path[courier.path_vertex],to=courier.path[courier.path_vertex+1];
    const auto dx=std::int64_t(to.x)-from.x,dy=std::int64_t(to.y)-from.y;
    if (dx==1 && dy==0) return assets::StorageDirection::PosX;
    if (dx==-1 && dy==0) return assets::StorageDirection::NegX;
    if (dx==0 && dy==1) return assets::StorageDirection::PosY;
    if (dx==0 && dy==-1) return assets::StorageDirection::NegY;
    return std::nullopt;
}
const char* tool_name(simulation::RulesProfile rules,int tool) {
    if (simulation::production_profile(rules)) {
        switch (tool) {
        case 0: return "Market";
        case 1: return "Road";
        case 2: return "Clay source";
        case 3: return "Pottery";
        case 4: return "Warehouse";
        case 6: return "Remove road";
        case 7: return "Household";
        case 8: return "Farm";
        case 9: return "Service post";
        case 11: return "Fire Watch";
        case 12: return "Well";
        default: return "Select";
        }
    }
    switch (tool) {
    case 1: return "Road";
    case 2: return "Workshop";
    case 3: return "Warehouse";
    default: return "Select";
    }
}
const char* object_name(simulation::Object object) {
    switch (object) {
    case simulation::Object::Empty: return "Empty";
    case simulation::Object::Road: return "Road";
    case simulation::Object::Workshop: return "Workshop";
    case simulation::Object::Warehouse: return "Warehouse";
    case simulation::Object::ClaySource: return "Clay source";
    case simulation::Object::Pottery: return "Pottery";
    case simulation::Object::Household: return "Household";
    case simulation::Object::Farm: return "Farm";
    case simulation::Object::ServicePost: return "Service post";
    case simulation::Object::Market: return "Market";
    case simulation::Object::Well: return "Well";
    case simulation::Object::HealthPost: return "Health Post";
    case simulation::Object::FireWatch: return "Fire Watch";
    }
    return "Unknown";
}
simulation::CommandType command_type(simulation::RulesProfile rules,int tool) {
    if (simulation::production_profile(rules)) {
        switch (tool) {
        case 0: return simulation::CommandType::PlaceMarket;
        case 1: return simulation::CommandType::PlaceRoad;
        case 2: return simulation::CommandType::PlaceClaySource;
        case 3: return simulation::CommandType::PlacePottery;
        case 4: return simulation::CommandType::PlaceWarehouse;
        case 6: return simulation::CommandType::RemoveRoad;
        case 8: return simulation::CommandType::PlaceFarm;
        case 9: return simulation::CommandType::PlaceServicePost;
        case 11: return simulation::CommandType::PlaceFireWatch;
        case 12: return simulation::CommandType::PlaceWell;
        case 13: return simulation::CommandType::PlaceHealthPost;
        default: return simulation::CommandType::PlaceHousehold;
        }
    }
    switch (tool) {
    case 1: return simulation::CommandType::PlaceRoad;
    case 2: return simulation::CommandType::PlaceWorkshop;
    default: return simulation::CommandType::PlaceWarehouse;
    }
}
const char* profile_title(simulation::RulesProfile rules) {
    switch (rules) {
    case simulation::RulesProfile::LogisticsV1: return "Logistics v1";
    case simulation::RulesProfile::ProductionV2: return "Production v2";
    case simulation::RulesProfile::HouseholdV3: return "Household v3";
    case simulation::RulesProfile::SettlementV4: return "Settlement v4";
    case simulation::RulesProfile::IndustryV5: return "Industry v5";
    case simulation::RulesProfile::CityV6: return "City v6";
    case simulation::RulesProfile::CityV7: return "City v7";
    case simulation::RulesProfile::CityV8: return "City v8";
    case simulation::RulesProfile::CityV9: return "City v9";
    case simulation::RulesProfile::CityV10: return "City v10";
    case simulation::RulesProfile::CityV11: return "City v11";
    case simulation::RulesProfile::CityV12: return "City v12";
    case simulation::RulesProfile::CityV14: return "City v14";
    case simulation::RulesProfile::CityV16: return "City v16";
    case simulation::RulesProfile::CityV15: return "City v15";
    case simulation::RulesProfile::CityV13: return "City v13";
    }
    return "Sandbox";
}

std::string joined_buildings(const std::vector<simulation::Object>& kinds) {
    std::string result;
    for (std::size_t i=0;i<kinds.size();++i) {
        if (i) result+=i+1==kinds.size() ? " and ":", ";
        result+=simulation::starter_building_name(kinds[i]);
    }
    return result;
}
}

SandboxView::SandboxView(maps::StoredMapSession session,bool demo,simulation::RulesProfile rules,
                         std::uint32_t rule_version)
    : geometry_(session.map.declared_map_size),background_(std::move(session.plan)),demo_(demo),
      rules_(rules),requested_rule_version_(rule_version ? rule_version:
          simulation::current_rule_version(rules)),original_map_(std::move(session.map)),
      expected_map_input_sha256_(std::move(session.input_sha256)) {
    if (!geometry_.supported) throw std::invalid_argument("sandbox requires supported map geometry");
    if (!simulation::rule_version_supported(rules_,requested_rule_version_))
        throw std::invalid_argument("unsupported sandbox rule version");
    if (simulation::production_profile(rules_)) tool_=5;
}
SandboxView::~SandboxView() { shutdown(); }

void SandboxView::configure_save(std::filesystem::path root,std::filesystem::path map,
                                 std::filesystem::path save,
                                 std::optional<persistence::SaveDocument> initial) {
    data_root_=std::move(root); map_relative_=std::move(map); save_path_=std::move(save);
    initial_save_=std::move(initial);
}

void SandboxView::save_now() {
    if (save_path_.empty()) throw std::runtime_error("No sandbox save path configured");
    const auto document=capture_save_document();
    persistence::write_save(save_path_,document,data_root_,buildable_mask_);
    saved_tick_=world_->ticks(); saved_command_=world_->command_sequence();
    ++save_generation_;
    last_message_="Saved tick "+std::to_string(world_->ticks());
}

persistence::SaveDocument SandboxView::capture_save_document() const {
    performance::ScopedTimer timer(performance::Timing::AutosaveCapture);
    if (!world_ || data_root_.empty() || map_relative_.empty())
        throw std::runtime_error("sandbox has no valid save context");
    return persistence::make_document(data_root_,map_relative_,buildable_mask_,*world_);
}

bool SandboxView::recovery_safe_point() const {
    return world_ && !budget_warning_ && !pending_demolition_ && !road_start_ && !map_pressed_ && !ui_pressed_ &&
        !pending_command_ && !pending_road_;
}

void SandboxView::load_now() {
    if (save_path_.empty()) throw std::runtime_error("No sandbox save path configured");
    persistence::validate_save_target(save_path_,data_root_);
    const auto document=persistence::read_save(save_path_);
    if (document.map_relative!=map_relative_ || document.world.profile!=rules_)
        throw std::runtime_error("Save map or rules differ from current sandbox");
    auto replacement=persistence::restore_save(document,data_root_,buildable_mask_);
    world_=std::make_unique<simulation::World>(std::move(replacement));
    road_connectivity_report_.reset();
    road_connectivity_world_=nullptr;
    road_connectivity_courier_.reset();
    update_window_title();
    clear_visual_selection(); visual_hits_.clear(); visual_frame_valid_=false;
    saved_tick_=world_->ticks(); saved_command_=world_->command_sequence();
    cancel_gesture();
    if (simulation::water_profile(rules_)) {
        // A loaded branch can have the same revision and different Well positions.
        water_preview_cell_.reset();
        predicted_water_.reset();
        predicted_well_coverage_.reset();
        hover_dirty_=true;
    }
    if (selected_ && !world_->building_owner_at(*selected_)) selected_.reset();
    clock_.pause_and_reset();
    demo_origin_.reset();
    last_message_="Loaded tick "+std::to_string(world_->ticks())+" (paused)"+
        (document.migrated_from_schema1 ?
            (rules_==simulation::RulesProfile::ProductionV2 ?
                "; save schema 1 migrated to rules 2 in memory":
                "; save schema 1 migrated in memory") : "");
}

void SandboxView::initialize(SDL_Window* window,SDL_Renderer* renderer) {
    window_=window;
    renderer_=renderer;
    update_layout(false);
    background_.set_debug_diagnostics(debug_open_);
    background_.initialize(renderer_);
    background_.set_landscape_mode(LandscapeDebugMode::Regenerated);
    std::vector<std::string> builtin_errors;
    if (!walker_manifest_.empty()) {
        const auto manifest=walker_manifest_; const auto source=walker_source_;
        walker_manifest_.clear();
        try { set_walker_visuals(manifest,source); }
        catch (const std::exception& error) {
            if (source!=VisualProfileSource::Builtin) throw;
            set_walker_visuals({});
            builtin_errors.push_back(std::string("Built-in walker preview could not be loaded; using markers: ")+error.what());
        }
    }
    if (!building_manifest_.empty()) {
        const auto manifest=building_manifest_; const auto source=building_source_;
        building_manifest_.clear();
        try { set_building_visuals(manifest,source); }
        catch (const std::exception& error) {
            if (source!=VisualProfileSource::Builtin) throw;
            set_building_visuals({});
            builtin_errors.push_back(std::string("Built-in building preview could not be loaded; using markers: ")+error.what());
        }
    }
    if (!road_manifest_.empty()) {
        const auto manifest=road_manifest_; const auto source=road_source_;
        road_manifest_.clear();
        try { set_road_visuals(manifest,source); }
        catch (const std::exception& error) {
            if (source!=VisualProfileSource::Builtin) throw;
            set_road_visuals({});
            builtin_errors.push_back(std::string("Built-in road preview could not be loaded; using fallback tiles: ")+error.what());
        }
    }
    if (!fire_manifest_.empty()) {
        const auto manifest=fire_manifest_;const auto source=fire_source_;
        fire_manifest_.clear();
        set_fire_visuals(manifest,source);
    }
    prepare_fire_inspector_extension();
    prepare_market_walker_extension();
    prepare_service_walker_extension();
    prepare_health_walker_extension();
    buildable_mask_=maps::make_sandbox_buildable_mask(background_.plan(),geometry_);
    const auto active_version=initial_save_ ? initial_save_->world.rule_version:
        requested_rule_version_;
    std::shared_ptr<const simulation::MapPermissions> permissions;
    if (rules_==simulation::RulesProfile::CityV16 && active_version==3)
        permissions=maps::load_sandbox_map_permissions(original_map_,background_.plan(),
            geometry_,buildable_mask_,initial_save_ ?
                initial_save_->map_permissions_policy_version:
                simulation::kMapPermissionsPolicyVersion,expected_map_input_sha256_);
    original_map_={};
    if (initial_save_) {
        world_=std::make_unique<simulation::World>(persistence::restore_save(*initial_save_,data_root_,
            buildable_mask_,std::move(permissions)));
        clock_.pause_and_reset();
        last_message_="Loaded tick "+std::to_string(world_->ticks())+" (paused)"+
            (initial_save_->migrated_from_schema1 ? "; save schema 1 migrated in memory":"");
        initial_save_.reset();
    } else if (permissions)
        world_=std::make_unique<simulation::World>(std::move(permissions),rules_,active_version);
    else world_=std::make_unique<simulation::World>(maps::stored_grid_width,maps::stored_grid_height,
        buildable_mask_,rules_,active_version);
    reset_camera();
    if (demo_) place_demo();
    if (initial_save_ == std::nullopt && !demo_) {
        saved_tick_=world_->ticks(); saved_command_=world_->command_sequence();
    }
    if (!builtin_errors.empty()) last_message_=builtin_errors.front();
    update_window_title();
}
void SandboxView::update_window_title() {
    if (!window_ || !world_) return;
    std::string title="OpenEmperor "+std::string(version::display)+" - "+profile_title(rules_);
    if (world_->map_permissions()) title+=" | rule "+std::to_string(world_->rule_version())+
        " | map policy "+std::to_string(world_->map_permissions()->policy_version());
    SDL_SetWindowTitle(window_,title.c_str());
}
void SandboxView::shutdown() {
    cancel_gesture();
    pending_demolition_.reset(); demolition_button_pressed_.reset();
    walker_diagnostic_open_=false;
    walker_sprites_.reset();
    walker_profile_.reset();
    fire_inspector_extension_=market_walker_extension_=service_walker_extension_=health_walker_extension_=false;
    building_sprite_.reset();
    building_profile_.reset();
    road_sprites_.reset();
    road_profile_.reset();
    fire_sprites_.reset();
    fire_profile_.reset();
    fire_frames_drawn_.fill(false);
    fire_draws_=fire_fallback_draws_=0;
    background_.shutdown();
    world_.reset();
    window_=nullptr;
    renderer_=nullptr;
}
void SandboxView::set_walker_visuals(const std::filesystem::path& manifest,
                                     VisualProfileSource source) {
    if (manifest.empty()) {
        walker_sprites_.reset(); walker_profile_.reset(); walker_manifest_.clear();
        visual_hits_.clear(); visual_frame_valid_=false;
        walker_source_=VisualProfileSource::Fallback;
        fire_inspector_extension_=market_walker_extension_=service_walker_extension_=health_walker_extension_=false;
        fire_inspector_fallback_reason_="Core walker sprites unavailable";
        market_walker_fallback_reason_="Core walker sprites unavailable";
        service_walker_fallback_reason_="Core walker sprites unavailable";
        health_walker_fallback_reason_="Core walker sprites unavailable";
        walker_diagnostic_open_=false;
        return;
    }
    if (!renderer_) { walker_manifest_=manifest; walker_source_=source; return; }
    if (!simulation::production_profile(rules_))
        throw std::runtime_error("walker visuals require a production sandbox profile");
    auto profile=assets::load_walker_visual_profile(data_root_,manifest);
    auto textures=std::make_unique<WalkerSpriteSet>();
    auto available=assets::walker_max_rgba_bytes;
    if (profile.schema_version>=3) {
        // Replacing the walker set excludes its old bytes, including when an
        // older profile already exceeded the aggregate session budget.
        const auto headroom=session_texture_headroom(false,true);
        if (!headroom) throw std::runtime_error("Session RGBA texture budget exhausted");
        available=*headroom;
    }
    textures->initialize(renderer_,profile,available);
    walker_sprites_=std::move(textures);
    walker_profile_=std::move(profile);
    visual_hits_.clear(); visual_frame_valid_=false;
    walker_manifest_=manifest;
    walker_source_=source;
    walker_core_schema_=walker_profile_->schema_version;
    fire_inspector_extension_=market_walker_extension_=service_walker_extension_=health_walker_extension_=false;
    fire_inspector_fallback_draws_=0;
    walker_visuals_enabled_=true;
    walker_role_stats_={};
    walker_moving_drawn_.fill(false);
    walker_unmapped_fallbacks_=walker_invalid_edge_fallbacks_=0;
    walker_diagnostic_role_=walker_diagnostic_direction_=walker_diagnostic_step_=0;
    last_message_="Curated walker previews active";
    enforce_fire_texture_budget();
    if (world_) { prepare_fire_inspector_extension();prepare_market_walker_extension();prepare_service_walker_extension();prepare_health_walker_extension(); }
}
void SandboxView::remove_fire_inspector_extension() {
    if (!fire_inspector_extension_) return;
    std::array<bool,assets::walker_visual_role_count> removed{};
    removed[assets::walker_role_index(assets::WalkerVisualRole::FireInspector)]=true;
    remove_walker_roles(removed);fire_inspector_extension_=false;
}
void SandboxView::remove_market_walker_extension() {
    if (!market_walker_extension_) return;
    std::array<bool,assets::walker_visual_role_count> removed{};
    removed[assets::walker_role_index(assets::WalkerVisualRole::Supplier)]=true;
    removed[assets::walker_role_index(assets::WalkerVisualRole::Distributor)]=true;
    remove_walker_roles(removed);market_walker_extension_=false;
}
void SandboxView::remove_service_walker_extension() {
    if (!service_walker_extension_) return;
    std::array<bool,assets::walker_visual_role_count> removed{};
    removed[assets::walker_role_index(assets::WalkerVisualRole::Service)]=true;
    remove_walker_roles(removed);service_walker_extension_=false;
}
void SandboxView::remove_health_walker_extension() {
    if (!health_walker_extension_) return;
    std::array<bool,assets::walker_visual_role_count> removed{};
    removed[assets::walker_role_index(assets::WalkerVisualRole::HealthWorker)]=true;
    remove_walker_roles(removed);health_walker_extension_=false;
}
void SandboxView::remove_walker_roles(
        const std::array<bool,assets::walker_visual_role_count>& removed) {
    const auto old_count=walker_profile_->unique_images.size();
    std::vector<bool> used(old_count,false);
    for (std::size_t role=0;role<removed.size();++role)
        if (!removed[role] && walker_profile_->roles[role])
            for (const auto& frame:walker_profile_->roles[role]->frames) used.at(frame.image_index)=true;
    std::vector<std::size_t> retained,remap(old_count,0);
    std::vector<assets::RgbaImage> images;
    retained.reserve(old_count);images.reserve(old_count);
    for (std::size_t i=0;i<old_count;++i) if (used[i]) {
        remap[i]=retained.size();retained.push_back(i);
    }
    // The texture set validates and allocates before touching its active data.
    // Remaining role images are moved, never copied, decoded or uploaded again.
    walker_sprites_->retain_images(retained);
    for (const auto i:retained) images.push_back(std::move(walker_profile_->unique_images[i]));
    for (std::size_t role=0;role<removed.size();++role) {
        if (removed[role]) walker_profile_->roles[role].reset();
        else if (walker_profile_->roles[role])
            for (auto& frame:walker_profile_->roles[role]->frames) frame.image_index=remap[frame.image_index];
    }
    walker_profile_->unique_images=std::move(images);
    walker_profile_->schema_version=walker_core_schema_;
    if (walker_profile_->find(assets::WalkerVisualRole::FireInspector))
        walker_profile_->schema_version=std::max(walker_profile_->schema_version,3U);
    if (walker_profile_->find(assets::WalkerVisualRole::Supplier) ||
        walker_profile_->find(assets::WalkerVisualRole::Distributor))
        walker_profile_->schema_version=std::max(walker_profile_->schema_version,4U);
    if (walker_profile_->find(assets::WalkerVisualRole::Service))
        walker_profile_->schema_version=std::max(walker_profile_->schema_version,5U);
    if (walker_profile_->find(assets::WalkerVisualRole::HealthWorker))
        walker_profile_->schema_version=std::max(walker_profile_->schema_version,6U);
    std::size_t remaining_aliases=0;
    for (const auto& role:walker_profile_->roles) if (role)
        remaining_aliases+=role->frames.size();
    // Old supplements can join a schema-6 aggregate above the historical bound.
    // Removing Health must not relabel their retained frames as a <=5 profile.
    if (remaining_aliases>assets::walker_legacy_max_frame_aliases)
        walker_profile_->schema_version=std::max(walker_profile_->schema_version,6U);
    visual_hits_.clear();visual_frame_valid_=false;
}
void SandboxView::set_fire_inspector_visuals(const std::filesystem::path& manifest,
        VisualProfileSource source,std::string fallback_reason) {
    remove_fire_inspector_extension();
    fire_inspector_manifest_=manifest;fire_inspector_source_=source;
    fire_inspector_fallback_reason_=fallback_reason.empty() ?
        "No Inspector clip selected":std::move(fallback_reason);
    fire_inspector_fallback_draws_=0;
    if (renderer_) prepare_fire_inspector_extension();
}
void SandboxView::prepare_fire_inspector_extension() {
    if (fire_inspector_manifest_.empty() || fire_inspector_extension_) return;
    if (!simulation::fire_profile(rules_)) {
        fire_inspector_fallback_reason_="Active rules have no FireInspector presentation";return;
    }
    if (!walker_profile_ || !walker_sprites_) {
        fire_inspector_fallback_reason_="Core walker sprites unavailable";return;
    }
    if (walker_profile_->find(assets::WalkerVisualRole::FireInspector)) return;
    if (walker_source_!=VisualProfileSource::Builtin ||
        fire_inspector_source_!=VisualProfileSource::Builtin) {
        fire_inspector_fallback_reason_="Explicit custom walkers are not supplemented";return;
    }
    const auto core_images=walker_profile_->unique_images.size();
    const auto core_schema=walker_profile_->schema_version;
    const auto available=session_texture_headroom(false,true);
    if (!available) {
        fire_inspector_fallback_reason_="Session RGBA texture budget exhausted";return;
    }
    try {
        assets::append_fire_inspector_visual_profile(data_root_,fire_inspector_manifest_,*walker_profile_);
        walker_sprites_->append(*walker_profile_,*available);
        fire_inspector_extension_=true;fire_inspector_fallback_reason_.clear();
        visual_hits_.clear();visual_frame_valid_=false;
    } catch (const std::exception& error) {
        walker_profile_->roles[assets::walker_role_index(assets::WalkerVisualRole::FireInspector)].reset();
        walker_profile_->unique_images.resize(core_images);
        walker_profile_->schema_version=core_schema;
        fire_inspector_fallback_reason_=std::string("Inspector preparation failed: ")+error.what();
    }
}
SandboxView::FireInspectorDisplayStats SandboxView::fire_inspector_display_stats() const {
    FireInspectorDisplayStats stats;
    const auto* visual=walker_profile_ ? walker_profile_->find(assets::WalkerVisualRole::FireInspector):nullptr;
    stats.configured=visual && walker_sprites_;
    stats.active=stats.configured && walker_visuals_enabled_;
    stats.fallback_reason=stats.configured ? (walker_visuals_enabled_ ? "":"F2 marker comparison"):
        walker_source_==VisualProfileSource::Custom ? "Custom profile has no FireInspector role":
        fire_inspector_fallback_reason_;
    if (visual) { stats.clip_id=visual->clip_id;stats.frames=visual->frames.size(); }
    if (fire_inspector_extension_) {
        std::array<bool,assets::walker_visual_role_count> selected{};
        selected[assets::walker_role_index(assets::WalkerVisualRole::FireInspector)]=true;
        const auto [assets,bytes]=exclusive_walker_images(selected);
        stats.additional_assets=assets;stats.additional_bytes=bytes;
    }
    stats.draws=walker_role_stats_[assets::walker_role_index(assets::WalkerVisualRole::FireInspector)].draws;
    stats.fallback_draws=fire_inspector_fallback_draws_;
    return stats;
}
std::pair<std::size_t,std::uint64_t> SandboxView::exclusive_walker_images(
        const std::array<bool,assets::walker_visual_role_count>& selected) const {
    if (!walker_profile_) return {};
    std::vector<bool> owned(walker_profile_->unique_images.size(),false),shared(owned.size(),false);
    for (std::size_t role=0;role<selected.size();++role) if (walker_profile_->roles[role])
        for (const auto& frame:walker_profile_->roles[role]->frames)
            (selected[role] ? owned:shared).at(frame.image_index)=true;
    std::size_t count=0;
    std::uint64_t bytes=0;
    for (std::size_t i=0;i<owned.size();++i) if (owned[i] && !shared[i]) {
        ++count;bytes+=walker_profile_->unique_images[i].pixels.size();
    }
    return {count,bytes};
}
void SandboxView::set_market_walker_visuals(const std::filesystem::path& manifest,
        VisualProfileSource source,std::string fallback_reason) {
    remove_market_walker_extension();
    market_walker_manifest_=manifest;market_walker_source_=source;
    market_walker_fallback_reason_=fallback_reason.empty() ?
        "No Food/Market clips selected":std::move(fallback_reason);
    market_walker_fallback_draws_=0;
    if (renderer_) prepare_market_walker_extension();
}
void SandboxView::prepare_market_walker_extension() {
    if (market_walker_manifest_.empty() || market_walker_extension_) return;
    if (!simulation::food_profile(rules_)) {
        market_walker_fallback_reason_="Active rules have no Food/Market presentation";return;
    }
    if (!walker_profile_ || !walker_sprites_) {
        market_walker_fallback_reason_="Core walker sprites unavailable";return;
    }
    using Role=assets::WalkerVisualRole;
    if (walker_profile_->find(Role::Supplier) || walker_profile_->find(Role::Distributor)) {
        market_walker_fallback_reason_="Food/Market roles are supplied by the selected core profile";
        return;
    }
    if (walker_source_!=VisualProfileSource::Builtin ||
        market_walker_source_!=VisualProfileSource::Builtin) {
        market_walker_fallback_reason_="Explicit custom walkers are not supplemented";return;
    }
    const auto images=walker_profile_->unique_images.size();
    const auto schema=walker_profile_->schema_version;
    const auto available=session_texture_headroom(false,true);
    if (!available) {
        market_walker_fallback_reason_="Session RGBA texture budget exhausted";return;
    }
    try {
        assets::append_market_visual_profile(data_root_,market_walker_manifest_,*walker_profile_);
        walker_sprites_->append(*walker_profile_,*available);
        market_walker_extension_=true;market_walker_fallback_reason_.clear();
        visual_hits_.clear();visual_frame_valid_=false;
    } catch (const std::exception& error) {
        // A failed loader never publishes either role. A failed eager upload
        // releases only its own new tail, preserving all previous textures.
        walker_profile_->roles[assets::walker_role_index(Role::Supplier)].reset();
        walker_profile_->roles[assets::walker_role_index(Role::Distributor)].reset();
        walker_profile_->unique_images.resize(images);
        walker_profile_->schema_version=schema;
        market_walker_fallback_reason_=std::string("Food/Market preparation failed: ")+error.what();
    }
}
SandboxView::MarketWalkerDisplayStats SandboxView::market_walker_display_stats() const {
    MarketWalkerDisplayStats stats;
    using Role=assets::WalkerVisualRole;
    constexpr std::array roles{Role::Supplier,Role::Distributor};
    stats.configured=walker_profile_ && walker_sprites_ &&
        walker_profile_->find(roles[0]) && walker_profile_->find(roles[1]);
    stats.active=stats.configured && walker_visuals_enabled_;
    stats.fallback_reason=stats.configured ? (walker_visuals_enabled_ ? "":"F2 marker comparison"):
        walker_source_==VisualProfileSource::Custom ? "Custom profile has incomplete Food/Market families":
        market_walker_fallback_reason_;
    std::array<bool,assets::walker_visual_role_count> selected{};
    for (std::size_t i=0;i<roles.size();++i) {
        const auto index=assets::walker_role_index(roles[i]);selected[index]=true;
        const auto* visual=walker_profile_ ? walker_profile_->find(roles[i]):nullptr;
        if (visual) { stats.clip_ids[i]=visual->clip_id;stats.frames+=visual->frames.size(); }
        stats.draws+=walker_role_stats_[index].draws;
    }
    if (market_walker_extension_) {
        const auto [assets,bytes]=exclusive_walker_images(selected);
        stats.additional_assets=assets;stats.additional_bytes=bytes;
    }
    stats.fallback_draws=market_walker_fallback_draws_;
    return stats;
}
void SandboxView::set_service_walker_visuals(const std::filesystem::path& manifest,
        VisualProfileSource source,std::string fallback_reason) {
    remove_service_walker_extension();
    service_walker_manifest_=manifest;service_walker_source_=source;
    service_walker_fallback_reason_=fallback_reason.empty() ?
        "No Service clip selected":std::move(fallback_reason);
    service_walker_fallback_draws_=0;
    if (renderer_) prepare_service_walker_extension();
}
void SandboxView::prepare_service_walker_extension() {
    if (service_walker_manifest_.empty() || service_walker_extension_) return;
    if (!simulation::service_profile(rules_)) {
        service_walker_fallback_reason_="Active rules have no Service presentation";return;
    }
    if (!walker_profile_ || !walker_sprites_) {
        service_walker_fallback_reason_="Core walker sprites unavailable";return;
    }
    using Role=assets::WalkerVisualRole;
    if (walker_profile_->find(Role::Service)) return;
    if (walker_source_!=VisualProfileSource::Builtin ||
        service_walker_source_!=VisualProfileSource::Builtin) {
        service_walker_fallback_reason_="Explicit custom walkers are not supplemented";return;
    }
    const auto images=walker_profile_->unique_images.size();
    const auto schema=walker_profile_->schema_version;
    const auto available=session_texture_headroom(false,true);
    if (!available) {
        service_walker_fallback_reason_="Session RGBA texture budget exhausted";return;
    }
    try {
        assets::append_service_visual_profile(data_root_,service_walker_manifest_,*walker_profile_);
        walker_sprites_->append(*walker_profile_,*available);
        service_walker_extension_=true;service_walker_fallback_reason_.clear();
        visual_hits_.clear();visual_frame_valid_=false;
    } catch (const std::exception& error) {
        walker_profile_->roles[assets::walker_role_index(Role::Service)].reset();
        walker_profile_->unique_images.resize(images);walker_profile_->schema_version=schema;
        service_walker_fallback_reason_=std::string("Service preparation failed: ")+error.what();
    }
}
SandboxView::ServiceWalkerDisplayStats SandboxView::service_walker_display_stats() const {
    ServiceWalkerDisplayStats stats;
    const auto* visual=walker_profile_ ? walker_profile_->find(assets::WalkerVisualRole::Service):nullptr;
    stats.configured=visual && walker_sprites_;
    stats.active=stats.configured && walker_visuals_enabled_;
    stats.fallback_reason=stats.configured ? (walker_visuals_enabled_ ? "":"F2 marker comparison"):
        walker_source_==VisualProfileSource::Custom ? "Custom profile has no Service role":
        service_walker_fallback_reason_;
    if (visual) { stats.clip_id=visual->clip_id;stats.frames=visual->frames.size(); }
    if (service_walker_extension_) {
        std::array<bool,assets::walker_visual_role_count> selected{};
        selected[assets::walker_role_index(assets::WalkerVisualRole::Service)]=true;
        const auto [assets,bytes]=exclusive_walker_images(selected);
        stats.additional_assets=assets;stats.additional_bytes=bytes;
    }
    stats.draws=walker_role_stats_[assets::walker_role_index(assets::WalkerVisualRole::Service)].draws;
    stats.fallback_draws=service_walker_fallback_draws_;
    return stats;
}
void SandboxView::set_health_walker_visuals(const std::filesystem::path& manifest,
        VisualProfileSource source,std::string fallback_reason) {
    remove_health_walker_extension();
    health_walker_manifest_=manifest;health_walker_source_=source;
    health_walker_fallback_reason_=fallback_reason.empty() ?
        "No HealthWorker clip selected":std::move(fallback_reason);
    health_walker_fallback_draws_=0;
    if (renderer_) prepare_health_walker_extension();
}
void SandboxView::prepare_health_walker_extension() {
    if (health_walker_manifest_.empty() || health_walker_extension_) return;
    if (!simulation::health_profile(rules_)) {
        health_walker_fallback_reason_="Active rules have no HealthWorker presentation";return;
    }
    if (!walker_profile_ || !walker_sprites_) {
        health_walker_fallback_reason_="Core walker sprites unavailable";return;
    }
    using Role=assets::WalkerVisualRole;
    if (walker_profile_->find(Role::HealthWorker)) return;
    if (walker_source_!=VisualProfileSource::Builtin ||
        health_walker_source_!=VisualProfileSource::Builtin) {
        health_walker_fallback_reason_="Explicit custom walkers are not supplemented";return;
    }
    const auto images=walker_profile_->unique_images.size();
    const auto schema=walker_profile_->schema_version;
    const auto available=session_texture_headroom(false,true);
    if (!available) {
        health_walker_fallback_reason_="Session RGBA texture budget exhausted";return;
    }
    try {
        assets::append_health_visual_profile(data_root_,health_walker_manifest_,*walker_profile_);
        walker_sprites_->append(*walker_profile_,*available);
        health_walker_extension_=true;health_walker_fallback_reason_.clear();
        visual_hits_.clear();visual_frame_valid_=false;
    } catch (const std::exception& error) {
        walker_profile_->roles[assets::walker_role_index(Role::HealthWorker)].reset();
        walker_profile_->unique_images.resize(images);walker_profile_->schema_version=schema;
        health_walker_fallback_reason_=std::string("HealthWorker preparation failed: ")+error.what();
    }
}
SandboxView::HealthWalkerDisplayStats SandboxView::health_walker_display_stats() const {
    HealthWalkerDisplayStats stats;
    const auto* visual=walker_profile_ ? walker_profile_->find(assets::WalkerVisualRole::HealthWorker):nullptr;
    stats.configured=visual && walker_sprites_;
    stats.active=stats.configured && walker_visuals_enabled_;
    stats.fallback_reason=stats.configured ? (walker_visuals_enabled_ ? "":"F2 marker comparison"):
        walker_source_==VisualProfileSource::Custom ? "Custom profile has no HealthWorker role":
        health_walker_fallback_reason_;
    if (visual) { stats.clip_id=visual->clip_id;stats.frames=visual->frames.size(); }
    if (health_walker_extension_) {
        std::array<bool,assets::walker_visual_role_count> selected{};
        selected[assets::walker_role_index(assets::WalkerVisualRole::HealthWorker)]=true;
        const auto [assets,bytes]=exclusive_walker_images(selected);
        stats.additional_assets=assets;stats.additional_bytes=bytes;
    }
    stats.draws=walker_role_stats_[assets::walker_role_index(assets::WalkerVisualRole::HealthWorker)].draws;
    stats.fallback_draws=health_walker_fallback_draws_;
    return stats;
}
SandboxView::WalkerDisplayStats SandboxView::walker_display_stats() const {
    WalkerDisplayStats stats;
    if (walker_profile_) {
        stats.schema_version=walker_profile_->schema_version;
        stats.frame_alias_limit=assets::walker_frame_alias_limit(stats.schema_version);
        stats.logical_rgba_bytes=assets::walker_rgba_bytes(*walker_profile_);
        for (std::size_t role=0;role<assets::walker_visual_role_count;++role) {
            const auto& visual=walker_profile_->roles[role];
            stats.roles[role]=walker_role_stats_[role];
            stats.roles[role].configured=visual.has_value();
            if (visual) stats.frame_aliases+=visual->frames.size();
            if (visual) for (std::size_t direction=0;direction<4;++direction)
                stats.roles[role].directions_configured[direction]=
                    !visual->clips[direction].empty();
        }
        stats.configured=stats.roles[0].directions_configured;
        stats.decoded_assets=walker_profile_->unique_images.size();
        std::vector<bool> seen(stats.decoded_assets,false);
        for (const auto& visual:walker_profile_->roles) if (visual)
            for (const auto& frame:visual->frames)
                if (!seen.at(frame.image_index)) {
                    seen[frame.image_index]=true;
                    stats.decoded_frame_ids.push_back(frame.id);
                }
    }
    stats.texture_uploads=walker_texture_count();
    stats.moving_drawn=stats.roles[0].directions_drawn;
    stats.unmapped_fallbacks=walker_unmapped_fallbacks_;
    stats.invalid_edge_fallbacks=walker_invalid_edge_fallbacks_;
    return stats;
}
void SandboxView::set_building_visuals(const std::filesystem::path& manifest,
                                       VisualProfileSource source) {
    if (manifest.empty()) {
        building_sprite_.reset();building_profile_.reset();building_manifest_.clear();
        visual_hits_.clear(); visual_frame_valid_=false;
        building_source_=VisualProfileSource::Fallback;
        return;
    }
    if (!renderer_) { building_manifest_=manifest;building_source_=source;return; }
    if (!simulation::production_profile(rules_))
        throw std::runtime_error("Pottery visuals require a production sandbox profile");
    auto profile=assets::load_building_visual_profile(data_root_,manifest);
    if (source!=VisualProfileSource::Builtin) {
        const auto version=world_ ? world_->rule_version():initial_save_ ?
            initial_save_->world.rule_version:requested_rule_version_;
        for (const auto [role,kind]:{std::pair{assets::BuildingVisualRole::Well,simulation::Object::Well},
                std::pair{assets::BuildingVisualRole::HealthPost,simulation::Object::HealthPost}})
            if (const auto* entry=profile.find(role); entry && entry->footprint_side!=
                simulation::building_footprint(rules_,version,kind).width)
                throw std::runtime_error(std::string(assets::building_role_name(role))+
                    " visual footprint differs from active rules");
    }
    auto texture=std::make_unique<BuildingSprite>();
    texture->initialize(renderer_,profile);
    building_sprite_=std::move(texture);
    building_profile_=std::move(profile);
    visual_hits_.clear(); visual_frame_valid_=false;
    building_manifest_=manifest;
    building_source_=source;
    building_enabled_=true;
    building_drawn_instances_.fill(0);building_placeholder_fallbacks_.fill(0);
    last_message_="Curated building preview active";
    enforce_fire_texture_budget();
}
SandboxView::BuildingDisplayStats SandboxView::building_display_stats() const {
    BuildingDisplayStats stats;
    stats.configured=building_profile_.has_value();
    stats.decoded_assets=building_profile_ ? building_profile_->unique_images.size():0;
    stats.texture_uploads=building_texture_count();
    stats.drawn_instances=building_drawn_instances_;
    stats.placeholder_fallbacks=building_placeholder_fallbacks_;
    if (building_profile_)
        for (const auto role:assets::building_roles)
            stats.configured_roles[assets::role_index(role)]=building_entry(role)!=nullptr;
    return stats;
}
const assets::BuildingVisualEntry* SandboxView::building_entry(assets::BuildingVisualRole role) const {
    const auto* entry=building_profile_ ? building_profile_->find(role):nullptr;
    if (!entry || !world_) return entry;
    if (role==assets::BuildingVisualRole::Well || role==assets::BuildingVisualRole::HealthPost) {
        const auto kind=role==assets::BuildingVisualRole::Well ? simulation::Object::Well:
            simulation::Object::HealthPost;
        if (entry->footprint_side!=simulation::building_footprint(rules_,world_->rule_version(),kind).width)
            return nullptr;
    }
    return entry;
}
std::optional<assets::BuildingVisualRole> SandboxView::visual_role(simulation::Cell cell,
    simulation::Object object,bool placement_preview) const {
    const auto role=building_visual_role(object);
    if (role==assets::BuildingVisualRole::Household && building_profile_ &&
        simulation::desirability_profile(rules_)) {
        const auto owner=placement_preview ? std::nullopt:world_->building_owner_at(cell);
        const auto level=owner ? static_cast<unsigned>(world_->household_level(*owner)):0U;
        return building_profile_->household_role(level);
    }
    return role;
}
void SandboxView::set_road_visuals(const std::filesystem::path& manifest,
                                   VisualProfileSource source) {
    if (manifest.empty()) {
        road_sprites_.reset();road_profile_.reset();road_manifest_.clear();
        road_source_=VisualProfileSource::Fallback;return;
    }
    if (!renderer_) { road_manifest_=manifest;road_source_=source;return; }
    auto profile=assets::load_road_visual_profile(data_root_,manifest);
    auto sprites=std::make_unique<RoadSpriteSet>();
    sprites->initialize(renderer_,profile);
    road_sprites_=std::move(sprites);
    road_profile_=std::move(profile);
    road_manifest_=manifest;
    road_source_=source;
    road_enabled_=true;
    road_masks_seen_.fill(false);road_draws_=road_fallbacks_current_=0;
    last_message_="Curated road preview active";
    enforce_fire_texture_budget();
}
std::optional<std::uint64_t> SandboxView::session_texture_headroom(
        bool include_walkers,bool include_fire) const {
    constexpr auto limit=maps::stored_max_texture_bytes;
    auto used=background_.plan().logical_texture_bytes;
    if (used>limit) return std::nullopt;
    const auto add=[&](const auto& images) {
        for (const auto& image:images) {
            const auto bytes=static_cast<std::uint64_t>(image.pixels.size());
            if (bytes>limit-used) return false;
            used+=bytes;
        }
        return true;
    };
    if ((include_walkers && walker_profile_ && !add(walker_profile_->unique_images)) ||
        (building_profile_ && !add(building_profile_->unique_images)) ||
        (road_profile_ && !add(road_profile_->unique_images))) return std::nullopt;
    if (include_fire && fire_sprites_) {
        const auto bytes=fire_sprites_->logical_bytes();
        if (bytes>limit-used) return std::nullopt;
        used+=bytes;
    }
    return limit-used;
}
void SandboxView::enforce_fire_texture_budget() {
    const auto exhausted=[&] { return !session_texture_headroom(true,true); };
    if (health_walker_extension_ && exhausted()) {
        remove_health_walker_extension();
        health_walker_fallback_reason_="Session RGBA texture budget exhausted";
    }
    if (service_walker_extension_ && exhausted()) {
        remove_service_walker_extension();
        service_walker_fallback_reason_="Session RGBA texture budget exhausted";
    }
    if (market_walker_extension_ && exhausted()) {
        remove_market_walker_extension();
        market_walker_fallback_reason_="Session RGBA texture budget exhausted";
    }
    if (fire_inspector_extension_ && exhausted()) {
        remove_fire_inspector_extension();
        fire_inspector_fallback_reason_="Session RGBA texture budget exhausted";
    }
    if (fire_sprites_ && exhausted())
        set_fire_visuals({},VisualProfileSource::Fallback,"Session RGBA texture budget exhausted");
}
void SandboxView::set_fire_visuals(const std::filesystem::path& manifest,
                                  VisualProfileSource source,std::string fallback_reason) {
    if (manifest.empty()) {
        fire_sprites_.reset();fire_profile_.reset();fire_manifest_.clear();
        fire_source_=VisualProfileSource::Fallback;
        fire_fallback_reason_=fallback_reason.empty() ? "No fire clip selected":std::move(fallback_reason);
        fire_frames_drawn_.fill(false);
        fire_draws_=fire_fallback_draws_=0;
        return;
    }
    if (!renderer_) { fire_manifest_=manifest;fire_source_=source;return; }
    if (!simulation::fire_profile(rules_)) {
        set_fire_visuals({},VisualProfileSource::Fallback,"Active rules have no fire presentation");
        return;
    }
    try {
        auto profile=assets::load_fire_visual_profile(data_root_,manifest);
        const auto bytes=assets::fire_rgba_bytes(profile);
        const auto base=session_texture_headroom(false,false);
        const auto walker_bytes=walker_profile_ ? assets::walker_rgba_bytes(*walker_profile_):0;
        std::array<bool,assets::walker_visual_role_count> removed{};
        const auto available_for=[&]() -> std::optional<std::uint64_t> {
            if (!base) return std::nullopt;
            const auto retained=walker_bytes-exclusive_walker_images(removed).second;
            if (retained>*base) return std::nullopt;
            return *base-retained;
        };
        auto available=available_for();
        using Role=assets::WalkerVisualRole;
        // Preserve the fire-priority policy only on genuine aggregate pressure.
        // Plan optional eviction without changing any existing pixels/textures.
        if ((!available || bytes>*available) && health_walker_extension_) {
            removed[assets::walker_role_index(Role::HealthWorker)]=true;
            available=available_for();
        }
        if ((!available || bytes>*available) && service_walker_extension_) {
            removed[assets::walker_role_index(Role::Service)]=true;
            available=available_for();
        }
        if ((!available || bytes>*available) && market_walker_extension_) {
            removed[assets::walker_role_index(Role::Supplier)]=true;
            removed[assets::walker_role_index(Role::Distributor)]=true;
            available=available_for();
        }
        if ((!available || bytes>*available) && fire_inspector_extension_) {
            removed[assets::walker_role_index(Role::FireInspector)]=true;
            available=available_for();
        }
        if (!available || bytes>*available)
            throw std::runtime_error("fire clip exceeds remaining session RGBA budget");
        auto sprites=std::make_unique<FireSpriteSet>();
        sprites->initialize(renderer_,profile,*available);
        // Metadata, allocation and eager upload all succeed before foreign
        // optional textures may be released. Surviving roles need no source I/O.
        if (std::any_of(removed.begin(),removed.end(),[](bool value) { return value; })) {
            remove_walker_roles(removed);
            if (removed[assets::walker_role_index(Role::HealthWorker)]) {
                health_walker_extension_=false;
                health_walker_fallback_reason_="Session RGBA texture budget exhausted";
            }
            if (removed[assets::walker_role_index(Role::Service)]) {
                service_walker_extension_=false;
                service_walker_fallback_reason_="Session RGBA texture budget exhausted";
            }
            if (removed[assets::walker_role_index(Role::Supplier)]) {
                market_walker_extension_=false;
                market_walker_fallback_reason_="Session RGBA texture budget exhausted";
            }
            if (removed[assets::walker_role_index(Role::FireInspector)]) {
                fire_inspector_extension_=false;
                fire_inspector_fallback_reason_="Session RGBA texture budget exhausted";
            }
        }
        fire_sprites_=std::move(sprites);fire_profile_=std::move(profile);
        fire_manifest_=manifest;fire_source_=source;fire_fallback_reason_.clear();
        fire_frames_drawn_.fill(false);
        fire_draws_=fire_fallback_draws_=0;
    } catch (const std::exception& error) {
        set_fire_visuals({},VisualProfileSource::Fallback,std::string("Fire clip preparation failed: ")+error.what());
    }
}
SandboxView::FireDisplayStats SandboxView::fire_display_stats() const {
    FireDisplayStats stats;
    stats.animated=fire_profile_.has_value() && bool(fire_sprites_);
    stats.fallback_reason=fire_fallback_reason_;stats.draws=fire_draws_;
    stats.fallback_draws=fire_fallback_draws_;stats.frames_drawn=fire_frames_drawn_;
    if (fire_profile_) {
        stats.clip_id=fire_profile_->clip_id;stats.frames=fire_profile_->frames.size();
        stats.unique_assets=fire_profile_->unique_images.size();
    }
    if (fire_sprites_) {
        stats.texture_uploads=fire_sprites_->texture_count();
        stats.logical_bytes=fire_sprites_->logical_bytes();
    }
    return stats;
}
std::optional<std::size_t> SandboxView::fire_frame_for(simulation::BuildingId id) const {
    return world_ && fire_profile_ ? assets::fire_frame_index(*fire_profile_,world_->ticks(),
        static_cast<std::uint64_t>(id)):std::nullopt;
}
SandboxView::RoadDisplayStats SandboxView::road_display_stats() const {
    RoadDisplayStats stats;
    stats.configured=road_profile_.has_value();
    if (road_profile_) {
        for (std::size_t i=0;i<16;++i) stats.configured_masks[i]=road_profile_->tiles[i].has_value();
        stats.unique_assets=road_profile_->unique_images.size();
    }
    stats.texture_uploads=road_sprites_ ? road_sprites_->texture_count():0;
    stats.draws=road_draws_;stats.fallback_draws=road_fallbacks_current_;
    stats.masks_seen=road_masks_seen_;
    return stats;
}
void SandboxView::reset_camera() {
    fit_stored_camera(background_.plan(),camera_);
    camera_.offset.x+=layout_.map.x;
    camera_.offset.y+=layout_.map.y;
    if (demo_origin_) {
        const auto center=maps::terrain_world({static_cast<std::uint32_t>(demo_origin_->x+
            (simulation::industry_profile(rules_) ? 6 :
             rules_==simulation::RulesProfile::SettlementV4 ? 6 :
             rules_==simulation::RulesProfile::HouseholdV3 ? 5 :
             rules_==simulation::RulesProfile::ProductionV2 ? 3 : 2)),
            static_cast<std::uint32_t>(demo_origin_->y)},geometry_.border);
        camera_.zoom=std::clamp(camera_.zoom,1.0,4.0);
        camera_.center_on({center.x,center.y+20.0});
        camera_.offset.y+=layout_.map.y+std::min(75,layout_.map.h/5);
    }
    refresh_hover();
}
void SandboxView::resize_camera() {
    update_layout(true);
}
void SandboxView::update_layout(bool preserve_center) {
    int width=0,height=0,window_width=0,window_height=0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_,&width,&height))
        throw std::runtime_error(SDL_GetError());
    if (!SDL_GetWindowSize(window_,&window_width,&window_height))
        throw std::runtime_error(SDL_GetError());
    const auto next=sandbox_ui::make_layout(width,height,window_width,window_height,panel_open_,
        simulation::fire_profile(rules_),simulation::desirability_profile(rules_),
        simulation::water_profile(rules_),simulation::health_profile(rules_),
        simulation::market_profile(rules_));
    if (preserve_center && next.map.x==layout_.map.x && next.map.y==layout_.map.y &&
        next.map.w==layout_.map.w && next.map.h==layout_.map.h) return;
    // Panel toggles and resize change the clipped viewport before another
    // frame is submitted. Its previous visible-pixel cache is then invalid.
    visual_frame_valid_=false;
    scene::Point center{};
    if (preserve_center && layout_.map.w>0 && layout_.map.h>0)
        center=camera_.screen_to_world({layout_.map.x+layout_.map.w*0.5,
                                       layout_.map.y+layout_.map.h*0.5});
    layout_=next;
    camera_.viewport_width=layout_.map.w;
    camera_.viewport_height=layout_.map.h;
    if (preserve_center && layout_.map.w>0 && layout_.map.h>0) {
        camera_.center_on(center);
        camera_.offset.x+=layout_.map.x;
        camera_.offset.y+=layout_.map.y;
    }
    panel_scroll_=0;
    income_scroll_=0;
    if (preserve_center) {
        float mouse_x=0,mouse_y=0;
        (void)SDL_GetMouseState(&mouse_x,&mouse_y);
        pointer_=render_point(mouse_x,mouse_y);
    }
    refresh_hover();
}
void SandboxView::place_demo() {
    if (simulation::market_profile(rules_)) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+14<world_->width();++x) {
            std::vector<simulation::Command> commands{
                {simulation::CommandType::PlaceClaySource,{x,y}},
                {simulation::CommandType::PlacePottery,{x,y+3}},
                {simulation::CommandType::PlaceWarehouse,{x+3,y}},
                {simulation::CommandType::PlaceFarm,{x+4,y+3}},
                {simulation::CommandType::PlaceMarket,{x+3,y+3}},
                {simulation::CommandType::PlaceServicePost,{x+5,y+3}}};
            for (int dx=0;dx<=14;++dx)
                commands.push_back({simulation::CommandType::PlaceRoad,{x+dx,y+2}});
            commands.insert(commands.end(),{
                {simulation::CommandType::PlaceHousehold,{x+6,y}},
                {simulation::CommandType::PlaceHousehold,{x+6,y+3}},
                {simulation::CommandType::PlaceHousehold,{x+9,y}},
                {simulation::CommandType::PlaceHousehold,{x+9,y+3}}});
            if (simulation::fire_profile(rules_))
                commands.push_back({simulation::CommandType::PlaceFireWatch,{x+14,y+3}});
            bool valid=true;
            for (const auto& command:commands) {
                const auto kind=command.type==simulation::CommandType::PlaceClaySource ?
                    simulation::Object::ClaySource:
                    command.type==simulation::CommandType::PlacePottery ? simulation::Object::Pottery:
                    command.type==simulation::CommandType::PlaceWarehouse ? simulation::Object::Warehouse:
                    command.type==simulation::CommandType::PlaceMarket ? simulation::Object::Market:
                    command.type==simulation::CommandType::PlaceFarm ? simulation::Object::Farm:
                    command.type==simulation::CommandType::PlaceServicePost ? simulation::Object::ServicePost:
                    command.type==simulation::CommandType::PlaceFireWatch ? simulation::Object::FireWatch:
                    command.type==simulation::CommandType::PlaceHousehold ? simulation::Object::Household:
                    simulation::Object::Road;
                const auto cells=kind==simulation::Object::Road ?
                    std::vector<simulation::Cell>{command.cell}:
                    simulation::building_footprint_cells(rules_,world_->rule_version(),kind,command.cell);
                valid=std::all_of(cells.begin(),cells.end(),[&](simulation::Cell cell) {
                    return world_->buildable(cell) && world_->object_at(cell)==simulation::Object::Empty;
                });
                if (!valid) break;
            }
            if (!valid) continue;
            for (const auto& command:commands) {
                if (!world_->execute(command).accepted)
                    throw std::logic_error("City starter command failed");
            }
            demo_origin_=simulation::Cell{x,y};
            reset_camera();
            last_message_=std::string(profile_title(rules_))+" v"+
                std::to_string(world_->rule_version())+" tick-0 starter placed: "+
                std::to_string(world_->construction_spent_total())+" spent, "+
                std::to_string(world_->treasury())+" funds remain";
            return;
        }
        throw std::runtime_error("no suitable 15x5 sandbox-buildable city starter pattern");
    }
    if (rules_==simulation::RulesProfile::CityV10) {
        for (int y=0;y+3<world_->height();++y) for (int x=0;x+16<world_->width();++x) {
            std::vector<simulation::Command> commands{
                {simulation::CommandType::PlaceClaySource,{x,y}},
                {simulation::CommandType::PlacePottery,{x+3,y}},
                {simulation::CommandType::PlaceWarehouse,{x+6,y}},
                {simulation::CommandType::PlaceHousehold,{x+9,y}},
                {simulation::CommandType::PlaceHousehold,{x+12,y}},
                {simulation::CommandType::PlaceHousehold,{x+15,y}},
                {simulation::CommandType::PlaceFarm,{x+6,y+3}},
                {simulation::CommandType::PlaceServicePost,{x+8,y+3}}};
            for (int dx=1;dx<=15;++dx)
                commands.push_back({simulation::CommandType::PlaceRoad,{x+dx,y+2}});
            bool valid=true;
            for (const auto& command:commands) {
                const auto result=world_->validate(command);
                if (!result.accepted) { valid=false; break; }
            }
            if (!valid) continue;
            for (const auto& command:commands)
                if (!world_->execute(command).accepted)
                    throw std::logic_error("City-v10 footprint demo command failed");
            demo_origin_=simulation::Cell{x,y};
            reset_camera();
            last_message_="City v10 2x2 starter placed: 980 spent, 20 funds remain";
            return;
        }
        throw std::runtime_error("no suitable 17x4 sandbox-buildable City-v10 starter pattern");
    }
    if (simulation::service_profile(rules_)) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+12<=world_->width();++x) {
            const std::array cells{
                simulation::Cell{x+0,y+2},simulation::Cell{x+1,y+2},
                simulation::Cell{x+2,y+2},simulation::Cell{x+3,y+2},
                simulation::Cell{x+4,y+2},simulation::Cell{x+5,y+2},
                simulation::Cell{x+6,y+2},simulation::Cell{x+7,y+2},
                simulation::Cell{x+8,y+2},simulation::Cell{x+9,y+2},
                simulation::Cell{x+10,y+2},simulation::Cell{x+7,y+1},
                simulation::Cell{x+8,y+1},simulation::Cell{x+9,y+1},
                simulation::Cell{x+6,y+4},simulation::Cell{x+7,y+4},
                simulation::Cell{x+8,y+4},simulation::Cell{x+8,y+3},
                simulation::Cell{x+9,y+3},simulation::Cell{x+10,y+3},
                simulation::Cell{x+9,y+4},simulation::Cell{x+10,y+4},
                simulation::Cell{x+11,y+4}};
            if (!std::all_of(cells.begin(),cells.end(),[&](simulation::Cell cell) {
                    return world_->buildable(cell); })) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("City service demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,2);
            run(simulation::CommandType::PlaceRoad,1,2);
            run(simulation::CommandType::PlaceRoad,2,2);
            run(simulation::CommandType::PlacePottery,3,2);
            run(simulation::CommandType::PlaceRoad,4,2);
            run(simulation::CommandType::PlaceRoad,5,2);
            run(simulation::CommandType::PlaceWarehouse,6,2);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,2);
            run(simulation::CommandType::PlaceHousehold,10,2);
            run(simulation::CommandType::PlaceRoad,7,1);
            run(simulation::CommandType::PlaceRoad,8,1);
            run(simulation::CommandType::PlaceHousehold,9,1);
            run(simulation::CommandType::PlaceFarm,6,4);
            run(simulation::CommandType::PlaceRoad,7,4);
            run(simulation::CommandType::PlaceRoad,8,4);
            run(simulation::CommandType::PlaceRoad,8,3);
            run(simulation::CommandType::PlaceRoad,9,3);
            run(simulation::CommandType::PlaceHousehold,10,3);
            run(simulation::CommandType::PlaceRoad,9,4);
            run(simulation::CommandType::PlaceRoad,10,4);
            run(simulation::CommandType::PlaceServicePost,11,4);
            demo_origin_=simulation::Cell{x,y+2};
            reset_camera();
            last_message_=std::string(profile_title(rules_))+
                " service starter placed: 980 spent, 20 funds remain";
            return;
        }
        throw std::runtime_error("no suitable 12x5 sandbox-buildable City service starter pattern");
    }
    if (simulation::food_profile(rules_)) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int dx=0;dx<11;++dx) valid=valid && world_->buildable({x+dx,y+2});
            for (int dx : {7,8,9}) valid=valid && world_->buildable({x+dx,y+1});
            for (const auto cell:{simulation::Cell{x+6,y+4},simulation::Cell{x+7,y+4},
                                  simulation::Cell{x+8,y+4},simulation::Cell{x+8,y+3}})
                valid=valid && world_->buildable(cell);
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("City v7 demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,2);
            run(simulation::CommandType::PlaceRoad,1,2);
            run(simulation::CommandType::PlaceRoad,2,2);
            run(simulation::CommandType::PlacePottery,3,2);
            run(simulation::CommandType::PlaceRoad,4,2);
            run(simulation::CommandType::PlaceRoad,5,2);
            run(simulation::CommandType::PlaceWarehouse,6,2);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,2);
            run(simulation::CommandType::PlaceHousehold,10,2);
            run(simulation::CommandType::PlaceRoad,7,1);
            run(simulation::CommandType::PlaceRoad,8,1);
            run(simulation::CommandType::PlaceHousehold,9,1);
            run(simulation::CommandType::PlaceFarm,6,4);
            run(simulation::CommandType::PlaceRoad,7,4);
            run(simulation::CommandType::PlaceRoad,8,4);
            run(simulation::CommandType::PlaceRoad,8,3);
            demo_origin_=simulation::Cell{x,y+2};
            reset_camera();
            last_message_="City v7 starter placed through paid commands";
            return;
        }
        throw std::runtime_error("no suitable 11x5 sandbox-buildable City v7 starter pattern");
    }
    if (rules_==simulation::RulesProfile::CityV6) {
        for (int y=0;y+2<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int dx=0;dx<11;++dx) valid=valid && world_->buildable({x+dx,y+1});
            for (int dx : {7,8,9}) valid=valid && world_->buildable({x+dx,y});
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("city demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,1);
            run(simulation::CommandType::PlaceRoad,1,1);
            run(simulation::CommandType::PlaceRoad,2,1);
            run(simulation::CommandType::PlacePottery,3,1);
            run(simulation::CommandType::PlaceRoad,4,1);
            run(simulation::CommandType::PlaceRoad,5,1);
            run(simulation::CommandType::PlaceWarehouse,6,1);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,1);
            run(simulation::CommandType::PlaceHousehold,10,1);
            run(simulation::CommandType::PlaceRoad,7,0);
            run(simulation::CommandType::PlaceRoad,8,0);
            run(simulation::CommandType::PlaceHousehold,9,0);
            demo_origin_=simulation::Cell{x,y+1};
            reset_camera();
            last_message_="City starter placed through paid commands";
            return;
        }
        throw std::runtime_error("no suitable 11x3 sandbox-buildable city starter pattern");
    }
    if (rules_==simulation::RulesProfile::IndustryV5) {
        for (int y=0;y+4<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int dx=0;dx<11;++dx) valid=valid && world_->buildable({x+dx,y+2});
            for (int dy : {1,3}) for (int dx : {7,8,9})
                valid=valid && world_->buildable({x+dx,y+dy});
            for (int dx=0;dx<=6;++dx) valid=valid && world_->buildable({x+dx,y+4});
            valid=valid && world_->buildable({x+2,y+3}) && world_->buildable({x+6,y+3});
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("industry demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,2);
            run(simulation::CommandType::PlaceRoad,1,2);
            run(simulation::CommandType::PlaceRoad,2,2);
            run(simulation::CommandType::PlacePottery,3,2);
            run(simulation::CommandType::PlaceRoad,4,2);
            run(simulation::CommandType::PlaceRoad,5,2);
            run(simulation::CommandType::PlaceWarehouse,6,2);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,2);
            run(simulation::CommandType::PlaceHousehold,10,2);
            for (int dy : {1,3}) {
                run(simulation::CommandType::PlaceRoad,7,dy);
                run(simulation::CommandType::PlaceRoad,8,dy);
                run(simulation::CommandType::PlaceHousehold,9,dy);
            }
            run(simulation::CommandType::PlaceClaySource,0,4);
            run(simulation::CommandType::PlacePottery,3,4);
            run(simulation::CommandType::PlaceRoad,1,4);
            run(simulation::CommandType::PlaceRoad,2,4);
            run(simulation::CommandType::PlaceRoad,2,3);
            for (int dx=4;dx<=6;++dx) run(simulation::CommandType::PlaceRoad,dx,4);
            run(simulation::CommandType::PlaceRoad,6,3);
            demo_origin_=simulation::Cell{x,y+2};
            reset_camera();
            last_message_="Two-source, two-pottery industry demo placed";
            return;
        }
        throw std::runtime_error("no suitable 11x5 sandbox-buildable industry branch pattern");
    }
    if (rules_==simulation::RulesProfile::SettlementV4) {
        // A fixed three-branch pattern, searched only in the actual buildability mask.
        for (int y=0;y+2<world_->height();++y) for (int x=0;x+11<=world_->width();++x) {
            bool valid=true;
            for (int i=0;i<11;++i) valid=valid && world_->buildable({x+i,y+1});
            for (int dy : {0,2}) for (int dx : {7,8})
                valid=valid && world_->buildable({x+dx,y+dy});
            if (!valid) continue;
            const auto run=[&](simulation::CommandType type,int dx,int dy) {
                if (!world_->execute({type,{x+dx,y+dy}}).accepted)
                    throw std::logic_error("settlement demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,0,1);
            run(simulation::CommandType::PlaceRoad,1,1);
            run(simulation::CommandType::PlaceRoad,2,1);
            run(simulation::CommandType::PlacePottery,3,1);
            run(simulation::CommandType::PlaceRoad,4,1);
            run(simulation::CommandType::PlaceRoad,5,1);
            run(simulation::CommandType::PlaceWarehouse,6,1);
            for (int dx=7;dx<=9;++dx) run(simulation::CommandType::PlaceRoad,dx,1);
            run(simulation::CommandType::PlaceHousehold,10,1);
            for (int dy : {0,2}) {
                run(simulation::CommandType::PlaceRoad,7,dy);
                run(simulation::CommandType::PlaceRoad,8,dy);
                run(simulation::CommandType::PlaceHousehold,9,dy);
            }
            demo_origin_=simulation::Cell{x,y+1};
            reset_camera();
            last_message_="Three-house settlement demo placed through normal commands";
            return;
        }
        throw std::runtime_error("no suitable 11x3 sandbox-buildable branch pattern for settlement demo");
    }
    const int length=rules_==simulation::RulesProfile::HouseholdV3 ? 10 :
        rules_==simulation::RulesProfile::ProductionV2 ? 7 : 6;
    for (int y=0;y<world_->height();++y) for (int x=0;x+length<=world_->width();++x) {
        bool valid=true;
        for (int i=0;i<length;++i) valid=valid && world_->buildable({x+i,y});
        if (!valid) continue;
        demo_origin_=simulation::Cell{x,y};
        if (simulation::production_profile(rules_)) {
            const auto run=[&](simulation::CommandType type,int at) {
                if (!world_->execute({type,{at,y}}).accepted)
                    throw std::logic_error("production demo command failed");
            };
            run(simulation::CommandType::PlaceClaySource,x);
            for (int i=1;i<3;++i) run(simulation::CommandType::PlaceRoad,x+i);
            run(simulation::CommandType::PlacePottery,x+3);
            for (int i=4;i<6;++i) run(simulation::CommandType::PlaceRoad,x+i);
            run(simulation::CommandType::PlaceWarehouse,x+6);
            if (rules_==simulation::RulesProfile::HouseholdV3) {
                run(simulation::CommandType::PlaceRoad,x+7);
                run(simulation::CommandType::PlaceRoad,x+8);
                run(simulation::CommandType::PlaceHousehold,x+9);
            }
            reset_camera();
            last_message_="Production demo placed through normal commands";
            return;
        }
        if (!world_->execute({simulation::CommandType::PlaceWorkshop,{x,y}}).accepted)
            throw std::logic_error("demo workshop placement failed");
        for (int i=1;i<length-1;++i)
            if (!world_->execute({simulation::CommandType::PlaceRoad,{x+i,y}}).accepted)
                throw std::logic_error("demo road placement failed");
        if (!world_->execute({simulation::CommandType::PlaceWarehouse,{x+length-1,y}}).accepted)
            throw std::logic_error("demo warehouse placement failed");
        reset_camera();
        last_message_="Demo placed through normal commands";
        return;
    }
    throw std::runtime_error("no suitable straight sandbox-buildable row for demo");
}
std::optional<simulation::Cell> SandboxView::pick(scene::Point screen) const {
    if (!layout_.map.contains(screen.x,screen.y)) return std::nullopt;
    const auto cell=maps::pick_landscape_ground(background_.plan(),camera_.screen_to_world(screen),geometry_,background_.elevated());
    if (!cell) return std::nullopt;
    return simulation::Cell{static_cast<int>(cell->x),static_cast<int>(cell->y)};
}
simulation::CommandResult SandboxView::preview(simulation::Cell cell) const {
    if (tool_==(simulation::production_profile(rules_) ? 5 : 4))
        return {false,false,"Select tool",world_->command_sequence()+1,world_->ticks()};
    if (tool_==1 && world_->object_at(cell)==simulation::Object::Road)
        return {true,false,"Existing road",world_->command_sequence()+1,world_->ticks()};
    return world_->validate({command_type(rules_,tool_),cell});
}
void SandboxView::set_tool(int tool) {
    cancel_gesture();
    sandbox_ui::Action action;
    switch (tool) {
    case 0: action=sandbox_ui::Action::Market; break;
    case 1: action=sandbox_ui::Action::Road; break;
    case 2: action=sandbox_ui::Action::Clay; break;
    case 3: action=simulation::production_profile(rules_) ? sandbox_ui::Action::Pottery :
        sandbox_ui::Action::Warehouse; break;
    case 4: action=simulation::production_profile(rules_) ? sandbox_ui::Action::Warehouse :
        sandbox_ui::Action::Select; break;
    case 5: action=sandbox_ui::Action::Select; break;
    case 6: action=sandbox_ui::Action::RemoveRoad; break;
    case 7: action=sandbox_ui::Action::Household; break;
    case 8: action=sandbox_ui::Action::Farm; break;
    case 9: action=sandbox_ui::Action::ServicePost; break;
    case 11: action=sandbox_ui::Action::FireWatch; break;
    case 12: action=sandbox_ui::Action::Well; break;
    case 13: action=sandbox_ui::Action::HealthPost; break;
    default: return;
    }
    if (action_enabled(action)) perform_action(action);
}
bool SandboxView::action_enabled(sandbox_ui::Action action) const {
    if (action==sandbox_ui::Action::ToggleIncome) return simulation::market_profile(rules_);
    const auto count_kind=[&](simulation::Object kind) {
        return std::count_if(world_->buildings().begin(),world_->buildings().end(),
            [&](const simulation::BuildingState& b) { return b.placed && b.kind==kind; });
    };
    if (action==sandbox_ui::Action::Save || action==sandbox_ui::Action::Load)
        return !save_path_.empty();
    if (simulation::city_profile(rules_)) {
        std::optional<simulation::CommandType> command;
        if (action==sandbox_ui::Action::Road) command=simulation::CommandType::PlaceRoad;
        else if (action==sandbox_ui::Action::Clay) command=simulation::CommandType::PlaceClaySource;
        else if (action==sandbox_ui::Action::Pottery) command=simulation::CommandType::PlacePottery;
        else if (action==sandbox_ui::Action::Warehouse) command=simulation::CommandType::PlaceWarehouse;
        else if (action==sandbox_ui::Action::Household) command=simulation::CommandType::PlaceHousehold;
        else if (action==sandbox_ui::Action::Farm) command=simulation::CommandType::PlaceFarm;
        else if (action==sandbox_ui::Action::ServicePost)
            command=simulation::CommandType::PlaceServicePost;
        else if (action==sandbox_ui::Action::Market)
            command=simulation::CommandType::PlaceMarket;
        else if (action==sandbox_ui::Action::FireWatch)
            command=simulation::CommandType::PlaceFireWatch;
        else if (action==sandbox_ui::Action::Well) command=simulation::CommandType::PlaceWell;
        else if (action==sandbox_ui::Action::HealthPost) command=simulation::CommandType::PlaceHealthPost;
        if (command && world_->treasury()<world_->construction_cost(*command)) return false;
    }
    if (action==sandbox_ui::Action::Health) return simulation::health_profile(rules_);
    if (action==sandbox_ui::Action::HealthPost)
        return simulation::health_profile(rules_) && count_kind(simulation::Object::HealthPost)<
            static_cast<std::ptrdiff_t>(simulation::Rules::health_post_limit);
    if (action==sandbox_ui::Action::Water) return simulation::water_profile(rules_);
    if (action==sandbox_ui::Action::Well)
        return simulation::water_profile(rules_) && count_kind(simulation::Object::Well)<
            static_cast<std::ptrdiff_t>(simulation::Rules::well_limit);
    if (action==sandbox_ui::Action::Desirability) return simulation::desirability_profile(rules_);
    if (action==sandbox_ui::Action::RemoveRoad)
        return simulation::production_profile(rules_);
    if (action==sandbox_ui::Action::Household)
        return simulation::household_profile(rules_) && count_kind(simulation::Object::Household)<
            ((simulation::scalable_profile(rules_)) ? 20:4);
    if (action==sandbox_ui::Action::Farm)
        return simulation::food_profile(rules_) &&
            count_kind(simulation::Object::Farm)<
                ((simulation::scalable_profile(rules_)) ? 2:1);
    if (action==sandbox_ui::Action::ServicePost)
        return simulation::service_profile(rules_) &&
            count_kind(simulation::Object::ServicePost)<
                ((simulation::scalable_profile(rules_)) ? 2:1);
    if (action==sandbox_ui::Action::FireWatch)
        return simulation::fire_profile(rules_) && count_kind(simulation::Object::FireWatch)<
            static_cast<std::ptrdiff_t>(simulation::Rules::fire_watch_limit);
    if (action==sandbox_ui::Action::Market)
        return simulation::market_profile(rules_) &&
            count_kind(simulation::Object::Market)<
                static_cast<std::ptrdiff_t>(simulation::Rules::city_v11_market_limit);
    if (action==sandbox_ui::Action::Clay || action==sandbox_ui::Action::Pottery ||
        action==sandbox_ui::Action::Warehouse) {
        const auto wanted=action==sandbox_ui::Action::Clay ?
            (simulation::production_profile(rules_) ? simulation::Object::ClaySource :
                simulation::Object::Workshop) :
            action==sandbox_ui::Action::Pottery ? simulation::Object::Pottery :
                simulation::Object::Warehouse;
        if (action==sandbox_ui::Action::Pottery && !simulation::production_profile(rules_)) return false;
        int count=0;
        count=static_cast<int>(count_kind(wanted));
        const int limit=(simulation::scalable_profile(rules_)) ?
            (wanted==simulation::Object::ClaySource || wanted==simulation::Object::Pottery ? 4:
             wanted==simulation::Object::Warehouse ? 2:1):
            (simulation::industry_profile(rules_) &&
            (wanted==simulation::Object::ClaySource || wanted==simulation::Object::Pottery)) ? 2:1;
        return count<limit;
    }
    return true;
}
void SandboxView::cancel_gesture() {
    pressed_button_.reset();
    pressed_building_.reset();
    pressed_operation_action_.reset();
    pressed_demolition_=false;
    ui_pressed_=false;
    map_pressed_=false;
    road_start_.reset();
    road_preview_={};
    invalidate_road_preview_cache();
    menu_pressed_=false;
    budget_button_pressed_.reset();
}

sandbox_ui::Rect SandboxView::operation_toggle_rect() const {
    return {layout_.panel.x+10*layout_.scale,
            layout_.panel.y+layout_.panel.h-(world_->demolition_supported() ? 168:92)*layout_.scale-demolition_hint_extra_height(),
            std::max(0,layout_.panel.w-20*layout_.scale),28*layout_.scale};
}

sandbox_ui::Rect SandboxView::operation_priority_rect(int index) const {
    const int gap=4*layout_.scale;
    const int width=std::max(0,(layout_.panel.w-20*layout_.scale-2*gap)/3);
    return {layout_.panel.x+10*layout_.scale+index*(width+gap),
            layout_.panel.y+layout_.panel.h-(world_->demolition_supported() ? 124:48)*layout_.scale-demolition_hint_extra_height(),width,28*layout_.scale};
}

std::optional<SandboxView::OperationAction> SandboxView::operation_action_at(double x,double y) const {
    if (income_open_ || !layout_.panel_open || !world_ || !world_->operation_controls_supported())
        return std::nullopt;
    const auto id=selected_building();
    if (!id || !simulation::World::operation_controllable(world_->building(*id).kind))
        return std::nullopt;
    if (operation_toggle_rect().contains(x,y)) return OperationAction::Toggle;
    for (int i=0;i<3;++i) if (operation_priority_rect(i).contains(x,y))
        return static_cast<OperationAction>(static_cast<int>(OperationAction::PriorityHigh)+i);
    return std::nullopt;
}

void SandboxView::perform_operation_action(OperationAction action) {
    const auto id=selected_building();
    if (!id) return;
    const auto& building=world_->building(*id);
    simulation::Command command=action==OperationAction::Toggle ?
        simulation::set_building_operation(*id,!building.operating_enabled):
        simulation::set_building_workforce_priority(*id,
            action==OperationAction::PriorityHigh ? simulation::WorkforcePriority::High:
            action==OperationAction::PriorityNormal ? simulation::WorkforcePriority::Normal:
            simulation::WorkforcePriority::Low);
    (void)request_execute(command);
}

sandbox_ui::Rect SandboxView::demolition_button_rect() const {
    return {layout_.panel.x+10*layout_.scale,
        layout_.panel.y+layout_.panel.h-78*layout_.scale-demolition_hint_extra_height(),
        std::max(0,layout_.panel.w-20*layout_.scale),28*layout_.scale};
}
void SandboxView::request_demolition() {
    const auto id=selected_building();
    if (!id || !world_->demolition_supported()) return;
    const auto status=world_->demolition_status(*id);
    last_message_=status.reason;
    if (!status.allowed) return;
    cancel_gesture(); pending_demolition_=id;
    demolition_button_pressed_.reset();
}
bool SandboxView::resolve_demolition(bool confirm) {
    if (!pending_demolition_) return false;
    const auto id=*pending_demolition_;
    pending_demolition_.reset(); demolition_button_pressed_.reset();
    cancel_gesture();
    if (!confirm) { last_message_="Demolition cancelled."; return false; }
    // Execute performs the single authoritative revalidation at approval.
    const auto result=execute(simulation::demolish_building(id));
    refresh_hover(true);
    return result.accepted && result.changed;
}

sandbox_ui::Rect SandboxView::budget_build_rect() const {
    const int w=std::min(540*layout_.scale,std::max(0,layout_.map.w-24*layout_.scale));
    const int x=layout_.map.x+(layout_.map.w-w)/2;
    const auto panel=budget_panel_rect();
    const int y=panel.y+panel.h-58*layout_.scale;
    return {x+12*layout_.scale,y,std::min(250*layout_.scale,std::max(0,(w-36*layout_.scale)/2)),34*layout_.scale};
}

sandbox_ui::Rect SandboxView::budget_cancel_rect() const {
    auto rect=budget_build_rect();
    rect.x+=rect.w+16*layout_.scale;
    return rect;
}
sandbox_ui::Rect SandboxView::budget_panel_rect() const {
    const int scale=layout_.scale;
    const int width=std::min(540*scale,std::max(0,layout_.map.w-24*scale));
    const int rows=budget_warning_ ? static_cast<int>(budget_warning_lines().size()):0;
    const int height=budget_warning_ ? std::max(260*scale,(98+rows*17)*scale):260*scale;
    return {layout_.map.x+(layout_.map.w-width)/2,
        layout_.map.y+(layout_.map.h-height)/2,width,height};
}
sandbox_ui::Rect SandboxView::menu_button_rect() const {
    return {std::max(0,layout_.top.w-96*layout_.scale),
        (simulation::market_profile(rules_) ? 44:8)*layout_.scale,88*layout_.scale,28*layout_.scale};
}
void SandboxView::perform_action(sandbox_ui::Action action) {
    using A=sandbox_ui::Action;
    if (action==A::Save || action==A::Load) cancel_gesture();
    if (!action_enabled(action)) {
        std::optional<simulation::CommandType> command;
        if (action==A::Road) command=simulation::CommandType::PlaceRoad;
        else if (action==A::Clay) command=simulation::CommandType::PlaceClaySource;
        else if (action==A::Pottery) command=simulation::CommandType::PlacePottery;
        else if (action==A::Warehouse) command=simulation::CommandType::PlaceWarehouse;
        else if (action==A::Household) command=simulation::CommandType::PlaceHousehold;
        else if (action==A::Farm) command=simulation::CommandType::PlaceFarm;
        else if (action==A::ServicePost) command=simulation::CommandType::PlaceServicePost;
        else if (action==A::Market) command=simulation::CommandType::PlaceMarket;
        else if (action==A::FireWatch) command=simulation::CommandType::PlaceFireWatch;
        else if (action==A::Well) command=simulation::CommandType::PlaceWell;
        else if (action==A::HealthPost) command=simulation::CommandType::PlaceHealthPost;
        if (simulation::city_profile(rules_) && command &&
            world_->treasury()<world_->construction_cost(*command))
            last_message_="Need "+std::to_string(world_->construction_cost(*command))+
                " funds; treasury "+std::to_string(world_->treasury());
        else last_message_=save_path_.empty() && (action==A::Save || action==A::Load) ?
            "No sandbox save path configured" : "Tool unavailable or building limit reached";
        return;
    }
    if (action==A::Select || action==A::Road || action==A::Clay || action==A::Pottery ||
        action==A::Warehouse || action==A::RemoveRoad || action==A::Household ||
        action==A::Farm || action==A::ServicePost || action==A::Market || action==A::FireWatch || action==A::Well || action==A::HealthPost) {
        cancel_gesture();
        tool_=action==A::Select ? (simulation::production_profile(rules_) ? 5:4) :
            action==A::Road ? 1 : action==A::Clay ? 2 : action==A::Pottery ? 3 :
            action==A::Warehouse ? (simulation::production_profile(rules_) ? 4:3) :
            action==A::RemoveRoad ? 6:action==A::Household ? 7:action==A::Farm ? 8:
            action==A::ServicePost ? 9:action==A::FireWatch ? 11:action==A::Well ? 12:action==A::HealthPost ? 13:0;
        last_message_=tool_name(rules_,tool_);
    } else if (action==A::Health) {
        cancel_gesture();
        health_overlay_=!health_overlay_;
        last_message_=health_overlay_ ? "Health ON: green protected, amber at risk, red sick":"Health OFF";
    } else if (action==A::Water) {
        cancel_gesture();
        water_overlay_=!water_overlay_;
        last_message_=water_overlay_ ? "Water ON: cyan covered, amber without water":"Water OFF";
    } else if (action==A::Desirability) {
        cancel_gesture();
        desirability_overlay_=!desirability_overlay_;
        last_message_=desirability_overlay_ ? "Desirability ON: green Good, amber Neutral, red Poor":
            "Desirability OFF";
    } else if (action==A::Pause) clock_.toggle_pause();
    else if (action==A::Step) { clock_.step_once(*world_); clear_hidden_walker_selection(); }
    else if (action==A::Speed1) clock_.set_speed(1);
    else if (action==A::Speed2) clock_.set_speed(2);
    else if (action==A::Speed4) clock_.set_speed(4);
    else if (action==A::Reset) { cancel_gesture(); reset_camera(); }
    else if (action==A::Save || action==A::Load) {
        cancel_gesture();
        ++io_generation_;
        try { if (action==A::Save) save_now(); else load_now(); }
        catch (const std::exception& error) {
            last_message_=(action==A::Save ? "Save failed: ":"Load failed: ")+
                std::string(error.what());
        }
    } else if (action==A::TogglePanel) {
        cancel_gesture();
        panel_open_=!panel_open_;
        update_layout(true);
    } else if (action==A::ToggleHelp) {
        cancel_gesture(); help_open_=!help_open_;
        last_message_=help_open_ ? "Help opened; press H or ? to close":"Help closed";
    } else if (action==A::ToggleIncome) {
        cancel_gesture();
        income_open_=!(income_open_ && layout_.panel_open);
        income_scroll_=0;
        if (income_open_) { panel_open_=true; update_layout(true); }
        last_message_=income_open_ ? "Income details opened; T returns to the Inspector":"Income details closed";
    }
    refresh_hover();
}
std::optional<scene::Point> SandboxView::render_point(float x,float y) const {
    int width=0,height=0;
    if (!std::isfinite(x) || !std::isfinite(y) ||
        !SDL_GetWindowSize(window_,&width,&height) || x<0 || y<0 ||
        x>=static_cast<float>(width) || y>=static_cast<float>(height))
        return std::nullopt;
    float rx=0,ry=0;
    if (!SDL_RenderCoordinatesFromWindow(renderer_,x,y,&rx,&ry) ||
        !std::isfinite(rx) || !std::isfinite(ry)) return std::nullopt;
    return scene::Point{rx,ry};
}
SandboxView::InputDiagnosticState SandboxView::input_diagnostic_state(
    std::optional<scene::Point> raw_window_position) const {
    InputDiagnosticState state;
    state.map_pressed=map_pressed_;
    state.ui_pressed=ui_pressed_ || pressed_button_.has_value() || menu_pressed_ ||
        budget_button_pressed_.has_value() || demolition_button_pressed_.has_value();
    state.road_drag=road_start_.has_value();
    state.input_focused=input_focused_;
    state.selected_cell=selected_;
    state.selected_landscape=selected_landscape_;
    state.selected_walker=selected_walker_ ? static_cast<std::uint32_t>(*selected_walker_):0;
    if (raw_window_position && std::isfinite(raw_window_position->x) &&
        std::isfinite(raw_window_position->y))
        state.render_position=render_point(static_cast<float>(raw_window_position->x),
                                           static_cast<float>(raw_window_position->y));
    if (state.render_position) {
        state.ui=layout_.ui_at(state.render_position->x,state.render_position->y) ||
            help_open_ || budget_warning_.has_value() || pending_demolition_.has_value();
        state.ui_action=layout_.button_at(state.render_position->x,state.render_position->y);
        state.ground_cell=pick(*state.render_position);
    }
    return state;
}
void SandboxView::invalidate_road_preview_cache() {
    planned_start_.reset(); planned_end_.reset();
    planned_road_revision_=UINT64_MAX; planned_command_sequence_=UINT64_MAX;
    planned_treasury_=INT64_MIN;
}
void SandboxView::refresh_hover(bool force_road_plan) {
    performance::ScopedTimer timer(performance::Timing::HoverPicking);
    hovered_=pointer_ ? pick(*pointer_) : std::nullopt;
    hover_dirty_=false;
    if (simulation::desirability_profile(rules_) && tool_==7 && hovered_) {
        if (desirability_preview_cell_!=hovered_ ||
            desirability_preview_revision_!=world_->road_revision()) {
            predicted_desirability_=world_->household_desirability_at(*hovered_);
            desirability_preview_cell_=hovered_;
            desirability_preview_revision_=world_->road_revision();
            ++desirability_preview_build_count_;
        }
    } else {
        predicted_desirability_.reset(); desirability_preview_cell_.reset();
    }
    if (simulation::water_profile(rules_) && (tool_==7 || tool_==12) && hovered_) {
        if (water_preview_cell_!=hovered_ || water_preview_tool_!=tool_ ||
            water_preview_revision_!=world_->road_revision()) {
            predicted_water_.reset(); predicted_well_coverage_.reset();
            if (tool_==7) predicted_water_=world_->household_has_water_at(*hovered_);
            else predicted_well_coverage_=world_->well_coverage_at(*hovered_);
            water_preview_cell_=hovered_; water_preview_tool_=tool_;
            water_preview_revision_=world_->road_revision();
            ++water_preview_build_count_;
        }
    } else {
        predicted_water_.reset(); predicted_well_coverage_.reset(); water_preview_cell_.reset();
    }
    if (!road_start_ || !hovered_) {
        if (road_start_) road_preview_={};
        return;
    }
    const bool stale=force_road_plan || planned_start_!=road_start_ || planned_end_!=hovered_ ||
        planned_road_revision_!=world_->road_revision() ||
        planned_command_sequence_!=world_->command_sequence() ||
        planned_treasury_!=world_->treasury();
    if (!stale) return;
    road_preview_=sandbox_ui::plan_road(*world_,*road_start_,*hovered_);
    ++road_plan_build_count_;
    planned_start_=road_start_; planned_end_=hovered_;
    planned_road_revision_=world_->road_revision();
    planned_command_sequence_=world_->command_sequence();
    planned_treasury_=world_->treasury();
}
void SandboxView::handle_event(const SDL_Event& event,bool& running) {
    if (event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (pending_demolition_) (void)resolve_demolition(false);
        cancel_gesture(); if (managed_) menu_requested_=true; else running=false; return;
    }
    // Window boundaries must disarm presses before any modal's early return.
    // Rendering also refreshes layout, but a queued release may precede it.
    if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST || event.type==SDL_EVENT_WINDOW_MINIMIZED ||
        event.type==SDL_EVENT_WINDOW_HIDDEN) {
        input_focused_=false;
        cancel_gesture(); budget_button_pressed_.reset(); demolition_button_pressed_.reset();
        if (pending_demolition_) (void)resolve_demolition(false);
        pointer_.reset(); refresh_hover(); visual_frame_valid_=false; return;
    }
    if (event.type==SDL_EVENT_WINDOW_FOCUS_GAINED) { input_focused_=true; return; }
    if (event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type==SDL_EVENT_WINDOW_RESIZED ||
        event.type==SDL_EVENT_WINDOW_DISPLAY_CHANGED || event.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
        event.type==SDL_EVENT_WINDOW_RESTORED) {
        cancel_gesture(); budget_button_pressed_.reset(); demolition_button_pressed_.reset();
        if (pending_demolition_) (void)resolve_demolition(false);
        visual_frame_valid_=false;
        resize_camera(); return;
    }
    if (pending_demolition_) {
        if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (event.key.key==SDLK_ESCAPE) { (void)resolve_demolition(false); return; }
            if (event.key.key==SDLK_RETURN || event.key.key==SDLK_KP_ENTER) {
                (void)resolve_demolition(true); return;
            }
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT) {
            demolition_button_pressed_.reset();
            const auto point=render_point(event.button.x,event.button.y);
            if (point && budget_build_rect().contains(point->x,point->y)) demolition_button_pressed_=true;
            else if (point && budget_cancel_rect().contains(point->x,point->y)) demolition_button_pressed_=false;
        } else if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
            const auto point=render_point(event.button.x,event.button.y);
            const auto pressed=demolition_button_pressed_;
            demolition_button_pressed_.reset();
            if (point && pressed && ((*pressed && budget_build_rect().contains(point->x,point->y)) ||
                (!*pressed && budget_cancel_rect().contains(point->x,point->y))))
                (void)resolve_demolition(*pressed);
        }
        return;
    }
    if (budget_warning_) {
        if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (event.key.key==SDLK_ESCAPE || event.key.key==SDLK_N ||
                event.key.key==SDLK_RETURN || event.key.key==SDLK_KP_ENTER) {
                (void)resolve_budget_warning(false); return;
            }
            if (event.key.key==SDLK_Y) {
                (void)resolve_budget_warning(true); return;
            }
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT) {
            budget_button_pressed_.reset();
            const auto point=render_point(event.button.x,event.button.y);
            if (point && budget_build_rect().contains(point->x,point->y))
                budget_button_pressed_=true;
            else if (point && budget_cancel_rect().contains(point->x,point->y))
                budget_button_pressed_=false;
            return;
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
            const auto point=render_point(event.button.x,event.button.y);
            const auto pressed=budget_button_pressed_;
            budget_button_pressed_.reset();
            if (point && pressed && *pressed && budget_build_rect().contains(point->x,point->y))
                (void)resolve_budget_warning(true);
            else if (point && pressed && !*pressed && budget_cancel_rect().contains(point->x,point->y))
                (void)resolve_budget_warning(false);
            return;
        }
        return;
    }
    if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key==SDLK_H || event.key.key==SDLK_QUESTION) {
            cancel_gesture();
            help_open_=!help_open_;
            last_message_=help_open_ ? "Help opened; press H or ? to close":"Help closed";
            return;
        }
        if (event.key.key==SDLK_ESCAPE) {
            if (help_open_) { help_open_=false; last_message_="Help closed"; return; }
            if (map_pressed_ || road_start_ || pressed_button_ || ui_pressed_ || menu_pressed_)
                cancel_gesture();
            else if (managed_) menu_requested_=true;
            else running=false;
            return;
        }
        if (help_open_) return;
        if (event.key.key==SDLK_T && simulation::market_profile(rules_)) {
            perform_action(sandbox_ui::Action::ToggleIncome); return;
        }
        if (event.key.key>=SDLK_1 && event.key.key<=
            (simulation::service_profile(rules_) ? SDLK_9 :
             rules_==simulation::RulesProfile::CityV7 ? SDLK_8 :
             simulation::household_profile(rules_) ? SDLK_7 :
             rules_==simulation::RulesProfile::ProductionV2 ? SDLK_6 : SDLK_4))
            set_tool(static_cast<int>(event.key.key-SDLK_1)+1);
        else if (event.key.key==SDLK_0 && simulation::market_profile(rules_))
            set_tool(0);
        else if (event.key.key==SDLK_F && simulation::fire_profile(rules_)) set_tool(11);
        else if (event.key.key==SDLK_J && simulation::health_profile(rules_)) set_tool(13);
        else if (event.key.key==SDLK_K && simulation::health_profile(rules_))
            perform_action(sandbox_ui::Action::Health);
        else if (event.key.key==SDLK_I && simulation::water_profile(rules_)) set_tool(12);
        else if (event.key.key==SDLK_U && simulation::water_profile(rules_))
            perform_action(sandbox_ui::Action::Water);
        else if (event.key.key==SDLK_D && simulation::desirability_profile(rules_))
            perform_action(sandbox_ui::Action::Desirability);
        else if (event.key.key==SDLK_SPACE) perform_action(sandbox_ui::Action::Pause);
        else if (event.key.key==SDLK_PERIOD) perform_action(sandbox_ui::Action::Step);
        else if (event.key.key==SDLK_PLUS || event.key.key==SDLK_EQUALS || event.key.key==SDLK_KP_PLUS)
            perform_action(clock_.speed()==1 ? sandbox_ui::Action::Speed2 : sandbox_ui::Action::Speed4);
        else if (event.key.key==SDLK_MINUS || event.key.key==SDLK_KP_MINUS)
            perform_action(clock_.speed()==4 ? sandbox_ui::Action::Speed2 : sandbox_ui::Action::Speed1);
        else if (event.key.key==SDLK_R) perform_action(sandbox_ui::Action::Reset);
        else if (event.key.key==SDLK_F5) perform_action(sandbox_ui::Action::Save);
        else if (event.key.key==SDLK_F9) perform_action(sandbox_ui::Action::Load);
        else if (event.key.key==SDLK_Z) {
            const double target=camera_.zoom<1.5 ? 2.0:camera_.zoom<3.0 ? 4.0:1.0;
            camera_.zoom_at({static_cast<double>(layout_.map.x+layout_.map.w/2),
                             static_cast<double>(layout_.map.y+layout_.map.h/2)},
                            target/camera_.zoom);
            refresh_hover();
        }
        else if (event.key.key==SDLK_TAB) perform_action(sandbox_ui::Action::TogglePanel);
        else if (event.key.key==SDLK_F1) {
            debug_open_=!debug_open_;
            visual_hits_.clear(); visual_frame_valid_=false;
            background_.set_debug_diagnostics(debug_open_);
            last_message_=debug_open_ ? "Debug diagnostics ON":"Debug diagnostics OFF";
        }
        else if (event.key.key==SDLK_G && debug_open_ && input_focused_) {
            cancel_gesture();
            request_road_connectivity_diagnosis();
        }
        else if (event.key.key==SDLK_F2 && walker_profile_) {
            walker_visuals_enabled_=!walker_visuals_enabled_;
            visual_hits_.clear(); visual_frame_valid_=false;
            last_message_=walker_visuals_enabled_ ? "Walker visuals ON" : "Walker visuals OFF";
        }
        else if (event.key.key==SDLK_F3 && walker_profile_) {
            walker_diagnostic_open_=!walker_diagnostic_open_;
            visual_hits_.clear();visual_frame_valid_=false;
            last_message_=walker_diagnostic_open_ ? "Walker clip inspection ON":"Walker clip inspection OFF";
        }
        else if (event.key.key==SDLK_F4) {
            if (building_profile_) {
                building_enabled_=!building_enabled_;
                visual_hits_.clear(); visual_frame_valid_=false;
                last_message_=building_enabled_ ? "Building visuals ON":"Building visuals OFF";
            } else last_message_="No building visuals loaded";
        }
        else if (event.key.key==SDLK_F6) {
            if (road_profile_) {
                road_enabled_=!road_enabled_;
                visual_hits_.clear(); visual_frame_valid_=false;
                last_message_=road_enabled_ ? "Road visuals ON":"Road visuals OFF";
            } else last_message_="No road visuals loaded";
        }
        else if (event.key.key==SDLK_F8 && debug_open_) {
            const auto mode=static_cast<LandscapeDebugMode>((static_cast<int>(background_.landscape_mode())+1)%8);
            background_.set_landscape_mode(mode);
            visual_hits_.clear(); visual_frame_valid_=false;
            last_message_=landscape_debug_mode_name(background_.landscape_mode());
            refresh_hover();
        }
        else if (event.key.key==SDLK_F7) {
            unified_depth_=!unified_depth_;
            visual_hits_.clear(); visual_frame_valid_=false;
            last_message_=unified_depth_ ?
                (background_.landscape_mode()==LandscapeDebugMode::Snapshot ?
                    "Depth painter: unified snapshot":"Depth painter: ground roads + spatial"):
                "Depth painter: legacy";
        }
        else if (walker_diagnostic_open_ && walker_profile_) {
            if (event.key.key==SDLK_V) {
                walker_diagnostic_role_=(walker_diagnostic_role_+1)%assets::walker_visual_role_count;
                walker_diagnostic_step_=0;
            } else if (event.key.key==SDLK_Q || event.key.key==SDLK_BACKSLASH) {
                walker_diagnostic_direction_=(walker_diagnostic_direction_+1)%4;
                walker_diagnostic_step_=0;
            } else if (event.key.key==SDLK_E || event.key.key==SDLK_RIGHTBRACKET)
                ++walker_diagnostic_step_;
            else if (event.key.key==SDLK_C || event.key.key==SDLK_LEFTBRACKET) {
                const auto& role=walker_profile_->roles[walker_diagnostic_role_];
                if (role) {
                    const auto& clip=role->clips[walker_diagnostic_direction_];
                    if (!clip.empty())
                        walker_diagnostic_step_=(walker_diagnostic_step_+clip.size()-1)%clip.size();
                }
            } else if (event.key.key==SDLK_X) walker_diagnostic_zoom4_=!walker_diagnostic_zoom4_;
            else if (event.key.key==SDLK_B) walker_diagnostic_light_=!walker_diagnostic_light_;
        }
    }
    if (help_open_) {
        if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT) {
            pressed_button_.reset();
            const auto point=render_point(event.button.x,event.button.y);
            if (point && layout_.button_at(point->x,point->y)==sandbox_ui::Action::ToggleHelp)
                pressed_button_=sandbox_ui::Action::ToggleHelp;
        } else if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
            const auto point=render_point(event.button.x,event.button.y);
            const bool matching=pressed_button_==sandbox_ui::Action::ToggleHelp && point &&
                layout_.button_at(point->x,point->y)==sandbox_ui::Action::ToggleHelp;
            cancel_gesture();
            if (matching)
                perform_action(sandbox_ui::Action::ToggleHelp);
        }
        return;
    }
    if (event.type==SDL_EVENT_MOUSE_WHEEL) {
        if (!std::isfinite(event.wheel.y)) return;
        const auto point=render_point(event.wheel.mouse_x,event.wheel.mouse_y);
        if (point) {
            pointer_=point;
            if (layout_.panel.contains(point->x,point->y)) {
                const auto rows=income_open_ ? income_lines().size():
                    placed_buildings().size()+inspection_lines().size();
                const int total=56*layout_.scale+static_cast<int>(rows)*18*layout_.scale;
                const int limit=std::max(0,total-layout_.panel.h);
                // Preserve integral-notch scrolling without narrowing an
                // untrusted float or overflowing the scaled scroll delta.
                auto& scroll=income_open_ ? income_scroll_:panel_scroll_;
                const double next=scroll-std::trunc(double(event.wheel.y))*24*layout_.scale;
                scroll=static_cast<int>(std::clamp(next,0.0,double(limit)));
            }
            else if (layout_.map.contains(point->x,point->y) && !road_start_)
                camera_.zoom_at(*point,std::pow(1.15,event.wheel.y));
            refresh_hover();
        }
    }
    if (event.type==SDL_EVENT_MOUSE_MOTION) {
        pointer_=render_point(event.motion.x,event.motion.y);
        {
            performance::ScopedTimer timer(performance::Timing::HoverPicking);
            hovered_=pointer_ ? pick(*pointer_) : std::nullopt;
        }
        hover_dirty_=true;
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button==SDL_BUTTON_RIGHT) {
            cancel_gesture(); return;
        }
        if (event.button.button!=SDL_BUTTON_LEFT) return;
        pointer_=render_point(event.button.x,event.button.y);
        if (managed_ && pointer_ && menu_button_rect().contains(pointer_->x,pointer_->y)) { menu_pressed_=true; return; }
        refresh_hover();
        if (!pointer_) return;
        if (!income_open_ && layout_.panel_open && world_->demolition_supported() && selected_building() &&
            demolition_button_rect().contains(pointer_->x,pointer_->y)) {
            pressed_demolition_=true; ui_pressed_=true; return;
        }
        if (const auto action=operation_action_at(pointer_->x,pointer_->y)) {
            pressed_operation_action_=action; ui_pressed_=true; return;
        }
        if (const auto button=layout_.button_at(pointer_->x,pointer_->y)) {
            pressed_button_=button; return;
        }
        if (layout_.ui_at(pointer_->x,pointer_->y)) {
            ui_pressed_=true;
            if (!income_open_ && layout_.panel.contains(pointer_->x,pointer_->y)) {
                const int relative=static_cast<int>(pointer_->y)-building_list_y()+panel_scroll_;
                if (relative>=0) {
                    const auto entries=placed_buildings();
                    const auto index=static_cast<std::size_t>(relative/(18*layout_.scale));
                    if (index<entries.size()) pressed_building_=entries[index];
                }
            }
            return;
        }
        const auto cell=pick(*pointer_);
        const bool landscape_hit=debug_open_ &&
            tool_==(simulation::production_profile(rules_) ? 5:4) &&
            background_.hit_test(*pointer_,camera_).has_value();
        if (!cell && !landscape_hit && !(debug_open_ &&
            tool_==(simulation::production_profile(rules_) ? 5:4))) return;
        map_pressed_=true;
        if (tool_==1) {
            road_start_=cell;
            road_preview_=sandbox_ui::plan_road(*world_,*cell,*cell);
            ++road_plan_build_count_;
            planned_start_=cell; planned_end_=cell;
            planned_road_revision_=world_->road_revision();
            planned_command_sequence_=world_->command_sequence();
            planned_treasury_=world_->treasury();
        }
    }
    if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT) {
        pointer_=render_point(event.button.x,event.button.y);
        if (menu_pressed_) {
            const bool hit=pointer_ && menu_button_rect().contains(pointer_->x,pointer_->y);
            cancel_gesture(); if (hit) menu_requested_=true; return;
        }
        refresh_hover(true);
        if (pressed_button_) {
            const auto action=*pressed_button_;
            const bool matching=pointer_ && layout_.button_at(pointer_->x,pointer_->y)==action;
            cancel_gesture();
            if (matching) perform_action(action);
            return;
        }
        if (ui_pressed_) {
            if (pressed_demolition_) {
                const bool matching=pointer_ && demolition_button_rect().contains(pointer_->x,pointer_->y);
                cancel_gesture();
                if (matching) request_demolition();
                return;
            }
            if (pressed_operation_action_) {
                const auto action=*pressed_operation_action_;
                const bool matching=pointer_ && operation_action_at(pointer_->x,pointer_->y)==action;
                cancel_gesture();
                if (matching) perform_operation_action(action);
                return;
            }
            if (pressed_building_ && pointer_ && layout_.panel.contains(pointer_->x,pointer_->y)) {
                const int relative=static_cast<int>(pointer_->y)-building_list_y()+panel_scroll_;
                const auto entries=placed_buildings();
                if (relative>=0) {
                    const auto index=static_cast<std::size_t>(relative/(18*layout_.scale));
                    if (index<entries.size() && entries[index]==*pressed_building_)
                        { clear_visual_selection(); selected_=world_->building(*pressed_building_).cell; income_open_=false; }
                    panel_scroll_=0;
                }
            }
            cancel_gesture(); return;
        }
        if (!map_pressed_) return;
        auto cell=pointer_ ? pick(*pointer_) : std::nullopt;
        const bool selecting=tool_==(simulation::production_profile(rules_) ? 5:4);
        if (selecting && pointer_ && layout_.map.contains(pointer_->x,pointer_->y)) income_open_=false;
        if (selecting && debug_open_ && pointer_ && select_visual(*pointer_)) {
            cancel_gesture(); refresh_hover(); return;
        }
        if (cell && road_start_ && tool_==1) {
            const auto plan=road_preview_;
            (void)request_road(plan);
            clear_visual_selection();
            selected_=cell;
        } else if (cell) {
            if (tool_==(simulation::production_profile(rules_) ? 5:4)) {
                if (selected_!=cell) panel_scroll_=0;
                clear_visual_selection();
                selected_=cell;
            }
            else (void)request_execute({command_type(rules_,tool_),*cell});
        }
        cancel_gesture();
        refresh_hover();
    }
}
void SandboxView::update(double seconds) {
    const bool* keys=SDL_GetKeyboardState(nullptr);
    update(seconds,{keys[SDL_SCANCODE_LEFT],keys[SDL_SCANCODE_RIGHT],
        keys[SDL_SCANCODE_UP],keys[SDL_SCANCODE_DOWN],keys[SDL_SCANCODE_A],
        keys[SDL_SCANCODE_D],keys[SDL_SCANCODE_W],keys[SDL_SCANCODE_S]});
}
void SandboxView::update(double seconds,PanKeyState keys) {
    performance::ScopedTimer timer(performance::Timing::SimulationUpdate);
    if (budget_warning_ || pending_demolition_) return;
    const double movement=400.0*std::clamp(seconds,0.0,0.05);
    if (input_focused_ && !road_start_) {
        if (keys.a || keys.left) camera_.offset.x+=movement;
        if ((!simulation::desirability_profile(rules_) && keys.d) || keys.right) camera_.offset.x-=movement;
        if (keys.w || keys.up) camera_.offset.y+=movement;
        if (keys.s || keys.down) camera_.offset.y-=movement;
        hover_dirty_=true;
    }
    if (hover_dirty_ || road_start_) refresh_hover();
    clock_.update(seconds,*world_);
    clear_hidden_walker_selection();
}
void SandboxView::tick_once() {
    if (!budget_warning_ && !pending_demolition_) world_->tick();
    clear_hidden_walker_selection();
}
simulation::CommandResult SandboxView::execute(simulation::Command command) {
    const auto result=world_->execute(command);
    last_message_=result.reason;
    if (command.type==simulation::CommandType::DemolishBuilding) {
        if (result.changed) { clear_visual_selection(); selected_.reset(); cancel_gesture(); refresh_hover(true); }
    } else if (command.type!=simulation::CommandType::SetBuildingOperation &&
        command.type!=simulation::CommandType::SetBuildingWorkforcePriority) {
        if (selected_!=command.cell) panel_scroll_=0;
        clear_visual_selection();
        selected_=command.cell;
    }
    return result;
}

simulation::CommandResult SandboxView::request_execute(simulation::Command command) {
    const auto validated=world_->validate(command);
    if (!validated.accepted || !validated.changed) return execute(command);
    std::optional<simulation::StarterBudgetWarning> warning;
    try {
        performance::ScopedTimer timer(performance::Timing::BudgetCheck);
        warning=simulation::starter_budget_warning(*world_,command);
    } catch (const std::exception& error) {
        // This read-only preflight has not executed a source command. Reject
        // the purchase here; mutation/commit failures stay outside this guard.
        cancel_gesture();
        budget_warning_.reset(); pending_command_.reset(); pending_road_.reset();
        budget_button_pressed_.reset();
        last_message_=std::string("Purchase check failed: ")+error.what();
        return {false,false,last_message_,world_->command_sequence()+1,world_->ticks()};
    }
    if (warning) {
        cancel_gesture();
        budget_warning_=std::move(warning);
        pending_command_=command;
        pending_road_.reset();
        last_message_="Starter budget confirmation required.";
        return {false,false,"Budget confirmation required",world_->command_sequence()+1,
                world_->ticks()};
    }
    return execute(command);
}

bool SandboxView::request_road(const sandbox_ui::RoadPlan& plan) {
    std::optional<simulation::StarterBudgetWarning> warning;
    {
        performance::ScopedTimer timer(performance::Timing::BudgetCheck);
        if (plan.valid) warning=simulation::starter_budget_warning_for_road_purchase(
            *world_,plan.total_cost);
    }
    if (warning) {
        cancel_gesture();
        budget_warning_=std::move(warning);
        pending_road_=plan;
        pending_command_.reset();
        last_message_="Starter budget confirmation required.";
        return false;
    }
    std::string reason;
    const bool okay=sandbox_ui::commit_road(*world_,plan,reason);
    last_message_=reason;
    return okay;
}

bool SandboxView::resolve_budget_warning(bool build_anyway) {
    if (!budget_warning_) return false;
    const auto command=pending_command_;
    const auto road=pending_road_;
    budget_warning_.reset(); pending_command_.reset(); pending_road_.reset();
    budget_button_pressed_.reset();
    if (!build_anyway) {
        last_message_="Purchase cancelled";
        return true;
    }
    if (command) {
        const auto result=execute(*command); // World::execute revalidates against current state.
        return result.accepted;
    }
    if (road) {
        std::string reason;
        const bool okay=sandbox_ui::commit_road(*world_,*road,reason);
        last_message_=reason;
        return okay;
    }
    return false;
}
scene::Point SandboxView::world_for(simulation::Position cell) const {
    const double u=cell.x-static_cast<double>(geometry_.border);
    const double v=cell.y-static_cast<double>(geometry_.border);
    double height=0;
    if (background_.elevated()) {
        const int x=static_cast<int>(std::floor(cell.x)), y=static_cast<int>(std::floor(cell.y));
        const double fx=cell.x-x, fy=cell.y-y;
        const auto h=[&](int px,int py) {
            if (px<0 || py<0) return 0.0;
            return static_cast<double>(maps::landscape_height(background_.plan(),
                {static_cast<unsigned>(px),static_cast<unsigned>(py)}));
        };
        height=(h(x,y)*(1-fx)+h(x+1,y)*fx)*(1-fy)+(h(x,y+1)*(1-fx)+h(x+1,y+1)*fx)*fy;
    }
    return {(u-v)*40.0,(u+v)*20.0+20.0-height*maps::landscape_height_step};
}
scene::Point SandboxView::building_visual_ground(simulation::Cell origin,
                                                  simulation::Object kind) const {
    const auto front=simulation::building_front_cell(rules_,world_->rule_version(),kind,origin);
    return world_for({static_cast<double>(front.x),static_cast<double>(front.y)});
}
void SandboxView::clear_visual_selection() {
    selected_landscape_=false; selected_walker_.reset();
}
void SandboxView::clear_hidden_walker_selection() {
    if (simulation::production_profile(rules_) && selected_walker_ &&
        !walker_live_visible(world_->courier(*selected_walker_))) {
        clear_visual_selection(); selected_.reset(); panel_scroll_=0;
    }
}
void SandboxView::record_visual_hit(const DrawInstance& instance) {
    if (instance.placement_preview || instance.object==simulation::Object::Road) return;
    const auto append=[&](const VisualHit& next) {
        const auto top=camera_.world_to_screen(next.origin);
        if (top.x+next.width*camera_.zoom<=layout_.map.x ||
            top.y+next.height*camera_.zoom<=layout_.map.y ||
            top.x>=layout_.map.x+layout_.map.w || top.y>=layout_.map.y+layout_.map.h) return;
        visual_hits_.push_back(next);
    };
    VisualHit hit; hit.key=instance.key; hit.cell=instance.cell;
    if (instance.key.layer==scene::WorldVisualLayer::SandboxWalker) {
        hit.walker=instance.courier;
        hit.cell={static_cast<int>(std::floor(instance.position.x)),
                  static_cast<int>(std::floor(instance.position.y))};
        const auto ground=world_for(instance.position);
        if (simulation::production_profile(rules_)) {
            const auto& courier=world_->courier(instance.courier);
            const auto role=walker_visual_role(courier.role);
            const auto* visual=role && walker_profile_ ? walker_profile_->find(*role):nullptr;
            if (walker_visuals_active() && walker_sprites_ && visual) {
                const auto pose=walker_pose(courier,world_->ticks(),*visual);
                if (pose.frame) {
                    const auto& frame=visual->frames[*pose.frame];
                    const auto& image=walker_profile_->unique_images[frame.image_index];
                    hit.image=frame.image_index; hit.origin={ground.x-frame.foot_x,ground.y-frame.foot_y};
                    hit.flip_x=frame.flip_x;
                    hit.width=image.width; hit.height=image.height;
                    append(hit);
                    if (pose.loaded && debug_open_) {
                        const double size=std::max(5.0,8.0*camera_.zoom)/camera_.zoom;
                        hit.image.reset();hit.origin={ground.x+size*.3,ground.y-size*1.5};
                        hit.width=hit.height=size*.6;append(hit);
                    }
                    return;
                }
            }
        }
        const auto id=static_cast<unsigned>(instance.courier);
        const int shift=id==1 ? -5:id==2 ? 5:id==3 ? 0:id==4 ? -10:id==5 ? 10:14;
        const double size=std::max(debug_open_ ? 5.0:4.0,(debug_open_ ? 10.0:7.0)*camera_.zoom)/camera_.zoom;
        hit.origin={ground.x-size/2+(simulation::production_profile(rules_) ? shift/camera_.zoom:0),ground.y-size/2};
        hit.width=hit.height=size; append(hit);
        const auto cargo=simulation::production_profile(rules_) ?
            world_->courier(instance.courier).cargo:world_->courier_cargo();
        if (cargo>0) {
            hit.origin={hit.origin.x+size*.5,hit.origin.y-size*.5};
            hit.width=hit.height=size*.5;append(hit);
        }
        return;
    }
    const auto ground=building_visual_ground(instance.cell,instance.object);
    const auto role=visual_role(instance.cell,instance.object);
    const auto* entry=role && building_profile_ ? building_entry(*role):nullptr;
    if (entry && building_visuals_active() && building_sprite_) {
        const auto& image=building_profile_->unique_images[entry->image_index];
        hit.image=entry->image_index; hit.origin={ground.x-entry->ground_x,ground.y-entry->ground_y};
        hit.width=image.width; hit.height=image.height;
        append(hit); return;
    }
    if (instance.object==simulation::Object::Well || instance.object==simulation::Object::HealthPost) {
        const int side=simulation::building_footprint(rules_,world_->rule_version(),instance.object).width;
        hit.mesh=instance.object;hit.mesh_ground=ground;hit.footprint_side=side;
        hit.origin={ground.x-30*side,ground.y-48*side-20*(side-1)};
        hit.width=60*side;hit.height=62*side;append(hit);return;
    }
    hit.origin={ground.x-40,ground.y-20}; hit.width=80;hit.height=40;hit.diamond=true;
    append(hit);
    const double size=std::max(5.0,11.0*camera_.zoom)/camera_.zoom;
    hit.origin={ground.x-size/2,ground.y-size*1.5}; hit.width=hit.height=size;hit.diamond=false;
    append(hit);
}
bool SandboxView::select_visual(scene::Point screen) {
    // A map press can be released over UI. Clipped sprite pixels cannot be
    // inspected through the panel or toolbar that covers them.
    if (!layout_.map.contains(screen.x,screen.y)) return true;
    // Input inspects the submitted frame, including between a camera/update
    // event and the next draw. Mode/profile switches invalidate its descriptors.
    if (!visual_frame_valid_ || visual_hit_mode_!=background_.landscape_mode()) return true;
    const auto world=visual_hit_camera_.screen_to_world(screen);
    const VisualHit* dynamic=nullptr;
    for (auto it=visual_hits_.rbegin();it!=visual_hits_.rend();++it) {
        // A submitted frame can precede the tick that returns an Inspector home.
        if (simulation::production_profile(rules_) && it->walker) {
            const auto& couriers=world_->couriers();
            const auto current=std::lower_bound(couriers.begin(),couriers.end(),*it->walker,
                [](const simulation::CourierState& courier,simulation::CourierId id) {
                    return courier.id<id;
                });
            if (current==couriers.end() || current->id!=*it->walker ||
                !walker_live_visible(*current)) continue;
        }
        const double x=world.x-it->origin.x,y=world.y-it->origin.y;
        if (x<0 || y<0 || x>=it->width || y>=it->height) continue;
        if (it->diamond && std::abs(x-40)/40+std::abs(y-20)/20>1) continue;
        if (it->mesh!=simulation::Object::Empty) {
            const auto ground=visual_hit_camera_.world_to_screen(it->mesh_ground);
            const bool hit=it->mesh==simulation::Object::Well ?
                hit_well_fallback(screen,ground,visual_hit_camera_.zoom,it->footprint_side):
                hit_health_post_fallback(screen,ground,visual_hit_camera_.zoom,it->footprint_side);
            if (!hit) continue;
        }
        if (it->image) {
            const auto* images=it->walker ? (walker_profile_ ? &walker_profile_->unique_images:nullptr):
                (building_profile_ ? &building_profile_->unique_images:nullptr);
            if (!images || *it->image>=images->size()) continue;
            const auto& image=(*images)[*it->image];
            const auto display_x=std::size_t(std::floor(x));
            const auto source_x=it->flip_x ? image.width-1-display_x:display_x;
            const auto offset=(std::size_t(std::floor(y))*image.width+source_x)*4+3;
            if (offset>=image.pixels.size() || image.pixels[offset]==0) continue;
        }
        dynamic=&*it;break;
    }
    const auto stored=background_.hit_test_item(screen,visual_hit_camera_,
        road_ground_replacements_,static_cast<std::size_t>(world_->width()));
    if (!dynamic && !stored) return false;
    clear_visual_selection(); panel_scroll_=0;
    if (dynamic && (!stored || !visual_hit_unified_ || stored->key<dynamic->key)) {
        selected_=dynamic->cell; selected_walker_=dynamic->walker;
    } else {
        selected_=simulation::Cell{static_cast<int>(stored->cell.x),static_cast<int>(stored->cell.y)};
        selected_landscape_=true;
    }
    return true;
}
bool SandboxView::draw_diamond(scene::Point world,std::uint8_t r,std::uint8_t g,std::uint8_t b,bool fill,float alpha) {
    const auto p=camera_.world_to_screen(world);
    const float x=static_cast<float>(p.x),y=static_cast<float>(p.y);
    const float w=static_cast<float>(40*camera_.zoom),h=static_cast<float>(20*camera_.zoom);
    const SDL_FPoint points[]={{x,y},{x+w,y+h},{x,y+2*h},{x-w,y+h}};
    if (fill) {
        const SDL_FColor color{r/255.0F,g/255.0F,b/255.0F,alpha};
        const SDL_Vertex vertices[]={{points[0],color,{}},{points[1],color,{}},
                                     {points[2],color,{}},{points[3],color,{}}};
        const int indices[]={0,1,2,0,2,3};
        if (!SDL_RenderGeometry(renderer_,nullptr,vertices,4,indices,6)) return false;
    }
    if (!SDL_SetRenderDrawColor(renderer_,r,g,b,255)) return false;
    for (int i=0;i<4;++i)
        if (!SDL_RenderLine(renderer_,points[i].x,points[i].y,
                            points[(i+1)%4].x,points[(i+1)%4].y)) return false;
    return true;
}
bool SandboxView::draw_world(const scene::Camera2D& render_camera) {
    clear_hidden_walker_selection();
    last_courier_draws_=0;
    fire_draws_=fire_fallback_draws_=0;
    road_fallbacks_current_=0;
    auto& instances=draw_instances_;
    instances.clear();
    visual_hits_.clear(); visual_frame_valid_=false;
    visual_hit_camera_=camera_;visual_hit_mode_=background_.landscape_mode();visual_hit_unified_=unified_depth_;
    const auto hit_capacity=3U*(world_->buildings().size()+world_->couriers().size()+2U);
    if (visual_hits_.capacity()<hit_capacity) visual_hits_.reserve(hit_capacity);
    const auto maximum=static_cast<std::size_t>(world_->width())*
        static_cast<std::size_t>(world_->height())+road_preview_.cells.size()+6U;
    if (instances.capacity()<maximum) instances.reserve(maximum);
    for (int y=0;y<world_->height();++y) for (int x=0;x<world_->width();++x) {
        const simulation::Cell cell{x,y};
        const auto object=world_->object_at(cell);
        if (object==simulation::Object::Empty) continue;
        const auto owner=world_->building_owner_at(cell);
        simulation::Cell visual_cell=cell;
        if (object!=simulation::Object::Road && owner) {
            const auto& building=world_->building(*owner);
            if (cell!=building.cell) continue;
            visual_cell=simulation::building_front_cell(rules_,world_->rule_version(),building.kind,building.cell);
        }
        const auto ground=object==simulation::Object::Road ?
            world_for({static_cast<double>(visual_cell.x),static_cast<double>(visual_cell.y)}):
            building_visual_ground(cell,object);
        const auto id=owner ? static_cast<unsigned>(*owner):
            static_cast<unsigned>(y*world_->width()+x);
        instances.push_back({{ground.y,ground.x,object==simulation::Object::Road ?
            scene::WorldVisualLayer::SandboxRoad:scene::WorldVisualLayer::SandboxBuilding,id},
            cell,object});
    }
    if (road_start_ && road_preview_.valid) {
        for (const auto cell:road_preview_.cells) {
            if (world_->object_at(cell)==simulation::Object::Road || world_->fixed_passage(cell)) continue;
            const auto ground=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
            const auto id=static_cast<unsigned>(cell.y*world_->width()+cell.x);
            instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxRoad,id},cell,
                                 simulation::Object::Road});
        }
    }
    road_ground_replacements_.resize(static_cast<std::size_t>(world_->width())*
                                     static_cast<std::size_t>(world_->height()));
    std::fill(road_ground_replacements_.begin(),road_ground_replacements_.end(),0);
    if (road_visuals_active() && road_sprites_ && road_profile_->replaces_ground) {
        for (const auto& instance:instances) if (instance.object==simulation::Object::Road) {
            const auto cell=instance.cell;
            const auto mask=road_start_ && road_preview_.valid ?
                sandbox_ui::road_neighbor_mask_for_preview(*world_,road_preview_.cells,cell):
                sandbox_ui::road_neighbor_mask(*world_,cell);
            if (road_profile_->find(mask))
                road_ground_replacements_[static_cast<std::size_t>(cell.y)*
                    static_cast<std::size_t>(world_->width())+static_cast<std::size_t>(cell.x)]=
                    world_->object_at(cell)==simulation::Object::Road ? 1:2;
        }
    }
    const auto draw_fire_overlay=[&](simulation::Cell cell,scene::Point center,
                                     bool placement_preview)->bool {
        if (placement_preview || !simulation::fire_profile(rules_)) return true;
        const auto owner=world_->building_owner_at(cell);
        if (!owner || !simulation::fire_eligible(world_->building(*owner).kind)) return true;
        const bool burning=world_->building_on_fire(*owner);
        if (burning && fire_profile_ && fire_sprites_) {
            const auto frame=fire_frame_for(*owner);
            if (!frame) return false; // Complete preparation guarantees a valid selection.
            const auto& building=world_->building(*owner);
            const auto side=simulation::building_footprint(rules_,world_->rule_version(),building.kind).width;
            const auto role=visual_role(cell,building.kind);
            const auto* entry=role ? building_entry(*role):nullptr;
            // Curated roof attachment, independent of F4. Original timing/pivots
            // are not claimed: center the footprint and lift into its visible body.
            const double rise=entry ? std::clamp(entry->ground_y*0.3,12.0,36.0):12.0;
            const scene::Point attachment{center.x,center.y-(20.0*(side-1)+rise)*camera_.zoom};
            const double scale=camera_.zoom*(side==1 ? 0.75:1.0);
            const scene::Point clip_min{static_cast<double>(layout_.map.x),static_cast<double>(layout_.map.y)};
            const scene::Point clip_max{static_cast<double>(layout_.map.x+layout_.map.w),
                static_cast<double>(layout_.map.y+layout_.map.h)};
            // Owner centers are never a culling oracle for a protruding flame.
            const auto bounds=fire_sprites_->clip_bounds(attachment,scale);
            if (bounds && bounds->max.x>clip_min.x && bounds->max.y>clip_min.y &&
                bounds->min.x<clip_max.x && bounds->min.y<clip_max.y) {
                const auto selected=FireSpriteSet::bounds(*frame,attachment,scale,*fire_profile_);
                if (!selected) return false;
                if (selected->max.x>clip_min.x && selected->max.y>clip_min.y &&
                    selected->min.x<clip_max.x && selected->min.y<clip_max.y) {
                    if (!fire_sprites_->draw(*frame,attachment,scale,*fire_profile_,clip_min,clip_max)) return false;
                    ++fire_draws_;fire_frames_drawn_[*frame]=true;
                }
            }
        } else if (burning) {
            ++fire_fallback_draws_;
            const float size=static_cast<float>(std::max(7.0,14.0*camera_.zoom));
            const float flicker=static_cast<float>((world_->ticks()/4U)%3U)*size*0.15F;
            const float x=static_cast<float>(center.x),y=static_cast<float>(center.y)-size;
            const SDL_Vertex flame[]={{{x-size,y},{220.F/255.F,75.F/255.F,30.F/255.F,235.F/255.F},{}},
                {{x+size,y},{245.F/255.F,125.F/255.F,35.F/255.F,235.F/255.F},{}},
                {{x,y-size*2.5F-flicker},{1.F,205.F/255.F,65.F/255.F,245.F/255.F},{}}};
            SDL_BlendMode previous;
            if (!SDL_GetRenderDrawBlendMode(renderer_,&previous)) return false;
            const bool drawn=SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND) &&
                SDL_RenderGeometry(renderer_,nullptr,flame,3,nullptr,0);
            const bool restored=SDL_SetRenderDrawBlendMode(renderer_,previous);
            if (!drawn || !restored) return false;
        }
        if (debug_open_) {
            const auto text=std::string(burning ? "FIRE ":"Risk ")+
                std::to_string(world_->building(*owner).fire_risk);
            if (!SDL_SetRenderDrawColor(renderer_,burning ? 255:220,burning ? 170:230,95,255) ||
                !SDL_RenderDebugText(renderer_,static_cast<float>(center.x)+8,
                    static_cast<float>(center.y)-38,text.c_str())) return false;
        }
        return true;
    };
    const auto draw_sickness=[&](simulation::Cell cell,scene::Point center,bool preview)->bool {
        if (preview || !simulation::health_profile(rules_)) return true;
        const auto owner=world_->building_owner_at(cell);
        if (!owner || !world_->household_sick(*owner)) return true;
        const float z=static_cast<float>(camera_.zoom);
        const SDL_FRect marker{static_cast<float>(center.x)+22*z,
            static_cast<float>(center.y)-17*z,4*z,6*z};
        return SDL_SetRenderDrawColor(renderer_,166,141,72,255) &&
            SDL_RenderFillRect(renderer_,&marker) && SDL_SetRenderDrawColor(renderer_,62,59,44,255) &&
            SDL_RenderLine(renderer_,marker.x+2*z,marker.y+z,marker.x+2*z,marker.y+5*z);
    };
    const auto draw_object=[&](simulation::Cell cell,simulation::Object object,
                               bool placement_preview)->bool {
        const int x=cell.x,y=cell.y;
        const auto visual_cell=object==simulation::Object::Road ? cell:
            simulation::building_front_cell(rules_,world_->rule_version(),object,cell);
        const auto top=object==simulation::Object::Road ?
            world_for({static_cast<double>(visual_cell.x),static_cast<double>(visual_cell.y)}):
            building_visual_ground(cell,object);
        const auto center=camera_.world_to_screen(top);
        if (object==simulation::Object::Road) {
            const bool new_preview=road_start_ && road_preview_.valid &&
                world_->object_at(cell)!=simulation::Object::Road;
            const auto mask=road_start_ && road_preview_.valid ?
                sandbox_ui::road_neighbor_mask_for_preview(*world_,road_preview_.cells,cell):
                sandbox_ui::road_neighbor_mask(*world_,cell);
            road_masks_seen_[mask]=true;
            const auto* entry=road_profile_ ? road_profile_->find(mask):nullptr;
            if (entry && road_visuals_active() && road_sprites_) {
                if (!road_sprites_->draw(center,camera_.zoom,*road_profile_,*entry,new_preview))
                    return false;
                if (!new_preview) ++road_draws_;
                return true;
            }
            if (!new_preview) ++road_fallbacks_current_;
            if (new_preview) return draw_diamond({top.x,top.y-20},45,180,100,true);
            const SDL_Color road_color=debug_open_ ? SDL_Color{225,174,65,255}:
                SDL_Color{137,109,72,255};
            if (!draw_diamond({top.x,top.y-20},road_color.r,road_color.g,road_color.b,true)) return false;
            for (const simulation::Cell next : {simulation::Cell{x+1,y},simulation::Cell{x,y+1},
                                                simulation::Cell{x-1,y},simulation::Cell{x,y-1}}) {
                const auto other=world_->object_at(next);
                if (other==simulation::Object::Empty ||
                    (other==simulation::Object::Road && (next.x<x || next.y<y))) continue;
                const auto screen=camera_.world_to_screen(world_for({static_cast<double>(next.x),
                    static_cast<double>(next.y)}));
                const SDL_Color line=debug_open_ ? SDL_Color{255,215,94,255}:
                    SDL_Color{161,132,88,255};
                if (!SDL_SetRenderDrawColor(renderer_,line.r,line.g,line.b,255) ||
                    !SDL_RenderLine(renderer_,static_cast<float>(center.x),static_cast<float>(center.y),
                        static_cast<float>(screen.x),static_cast<float>(screen.y))) return false;
            }
        } else {
            if (debug_open_) for (const auto footprint_cell:
                simulation::building_footprint_cells(rules_,world_->rule_version(),object,cell)) {
                const auto logical=world_for({static_cast<double>(footprint_cell.x),
                                              static_cast<double>(footprint_cell.y)});
                if (!draw_diamond({logical.x,logical.y-20},80,210,245,false)) return false;
            }
            const auto role=visual_role(cell,object,placement_preview);
            if (role && building_profile_) {
                const auto* entry=building_entry(*role);
                if (entry && building_visuals_active() && building_sprite_) {
                    if (!building_sprite_->draw(center,camera_.zoom,*building_profile_,*entry,
                                                placement_preview))
                        return false;
                    if (!placement_preview) ++building_drawn_instances_[assets::role_index(*role)];
                    return draw_fire_overlay(cell,center,placement_preview) && draw_sickness(cell,center,placement_preview);
                }
                if (placement_preview && object!=simulation::Object::Well && object!=simulation::Object::HealthPost) return true;

            }
            if (role && !placement_preview) ++building_placeholder_fallbacks_[assets::role_index(*role)];
            const auto side=simulation::building_footprint(rules_,world_->rule_version(),object).width;
            if (object==simulation::Object::Well)
                return draw_well_fallback(renderer_,center,camera_.zoom,placement_preview,side);
            if (object==simulation::Object::HealthPost)
                return draw_health_post_fallback(renderer_,center,camera_.zoom,placement_preview,side) &&
                    draw_fire_overlay(cell,center,placement_preview);
            SDL_Color color=debug_open_ ? SDL_Color{255,105,100,255}:SDL_Color{137,101,85,255};
            if (object==simulation::Object::Workshop || object==simulation::Object::ClaySource)
                color=debug_open_ ? SDL_Color{50,210,245,255}:SDL_Color{104,124,120,255};
            if (object==simulation::Object::Pottery)
                color=debug_open_ ? SDL_Color{220,95,245,255}:SDL_Color{135,101,125,255};
            if (object==simulation::Object::Household)
                color=debug_open_ ? SDL_Color{90,245,125,255}:SDL_Color{101,128,100,255};
            if (object==simulation::Object::Farm)
                color=debug_open_ ? SDL_Color{220,190,55,255}:SDL_Color{126,119,70,255};
            if (object==simulation::Object::ServicePost)
                color=debug_open_ ? SDL_Color{80,205,255,255}:SDL_Color{91,119,132,255};
            if (object==simulation::Object::FireWatch)
                color=debug_open_ ? SDL_Color{215,140,80,255}:SDL_Color{124,104,84,255};
            if (object==simulation::Object::Market)
                color=debug_open_ ? SDL_Color{225,170,65,255}:SDL_Color{133,104,65,255};
            if (!draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true)) return false;
            const float size=static_cast<float>(std::max(5.0,11.0*camera_.zoom));
            const SDL_FRect rect{static_cast<float>(center.x)-size/2,
                static_cast<float>(center.y)-size*1.5F,size,size};
            if (!SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) ||
                !SDL_RenderFillRect(renderer_,&rect)) return false;
            if (debug_open_ && (object==simulation::Object::Household || object==simulation::Object::Farm ||
                object==simulation::Object::ServicePost || object==simulation::Object::Market ||
                object==simulation::Object::FireWatch ||
                (simulation::industry_profile(rules_) &&
                 (object==simulation::Object::ClaySource || object==simulation::Object::Pottery)))) {
                const auto id=world_->building_owner_at({x,y});
                const char prefix=object==simulation::Object::Household ? 'H':
                    object==simulation::Object::Farm ? 'F':
                    object==simulation::Object::ServicePost ? 'S':
                    object==simulation::Object::Market ? 'M':
                    object==simulation::Object::ClaySource ? 'C':'P';
                const std::string label=(object==simulation::Object::FireWatch ?
                    std::string("FW"):std::string(1,prefix))+
                    std::to_string(static_cast<unsigned>(*id));
                if (!SDL_SetRenderDrawColor(renderer_,255,255,255,255) ||
                    !SDL_RenderDebugText(renderer_,static_cast<float>(center.x)+7,
                        static_cast<float>(center.y)-16,label.c_str())) return false;
            }
        }
        return draw_fire_overlay(cell,center,placement_preview) && draw_sickness(cell,center,placement_preview);
    };
    const auto draw_agent=[&](simulation::Position position,SDL_Color body,SDL_Color cargo_color,
                              int cargo,int shift)->bool {
        const auto screen=camera_.world_to_screen(world_for(position));
        const double base=debug_open_ ? 10.0:7.0;
        const float size=static_cast<float>(std::max(debug_open_ ? 5.0:4.0,base*camera_.zoom));
        const SDL_FRect marker{static_cast<float>(screen.x)-size/2+shift,
            static_cast<float>(screen.y)-size/2,size,size};
        if (!SDL_SetRenderDrawColor(renderer_,body.r,body.g,body.b,255) ||
            !SDL_RenderFillRect(renderer_,&marker)) return false;
        if (cargo>0) {
            const SDL_FRect cargo_rect{marker.x+size*.5F,marker.y-size*.5F,size*.5F,size*.5F};
            if (!SDL_SetRenderDrawColor(renderer_,cargo_color.r,cargo_color.g,cargo_color.b,255) ||
                !SDL_RenderFillRect(renderer_,&cargo_rect)) return false;
        }
        ++last_courier_draws_;
        return true;
    };
    const auto draw_courier=[&](simulation::CourierId id,simulation::Position position,
                               SDL_Color marker_color,SDL_Color cargo_color,
                               int marker_shift)->bool {
        const auto& courier=world_->courier(id);
        const auto visual_role=walker_visual_role(courier.role);
        const auto* visual=visual_role && walker_profile_ ?
            walker_profile_->find(*visual_role):nullptr;
        if (walker_visuals_active() && walker_sprites_ && visual) {
            const auto role_index=assets::walker_role_index(*visual_role);
            const auto pose=walker_pose(courier,world_->ticks(),*visual);
            const auto ground=camera_.world_to_screen(world_for(position));
            if (pose.frame) {
                if (!walker_sprites_->draw(*pose.frame,ground,camera_.zoom,*visual,*walker_profile_,
                    {static_cast<double>(layout_.map.x),static_cast<double>(layout_.map.y)},
                    {static_cast<double>(layout_.map.x+layout_.map.w),
                     static_cast<double>(layout_.map.y+layout_.map.h)})) return false;
                const auto& frame=visual->frames[*pose.frame];
                const auto& image=walker_profile_->unique_images[frame.image_index];
                const double left=ground.x-frame.foot_x*camera_.zoom;
                const double top=ground.y-frame.foot_y*camera_.zoom;
                const bool visible=left+image.width*camera_.zoom>layout_.map.x &&
                    top+image.height*camera_.zoom>layout_.map.y &&
                    left<layout_.map.x+layout_.map.w &&
                    top<layout_.map.y+layout_.map.h;
                if (visible) {
                    ++walker_role_stats_[role_index].draws;
                    if (pose.moving && pose.direction)
                        walker_role_stats_[role_index].directions_drawn[
                            assets::direction_index(*pose.direction)]=true;
                }
                if (pose.loaded && debug_open_) {
                    const float size=static_cast<float>(std::max(5.0,8.0*camera_.zoom));
                    const SDL_FRect cargo{static_cast<float>(ground.x)+size*.3F,
                        static_cast<float>(ground.y)-size*1.5F,size*.6F,size*.6F};
                    if (!SDL_SetRenderDrawColor(renderer_,cargo_color.r,cargo_color.g,
                                                cargo_color.b,255) ||
                        !SDL_RenderFillRect(renderer_,&cargo)) return false;
                }
                ++last_courier_draws_;
                return true;
            }
            if (pose.fallback==WalkerFallback::UnmappedDirection) {
                ++walker_unmapped_fallbacks_;
                ++walker_role_stats_[role_index].fallback_unmapped;
            } else if (pose.fallback==WalkerFallback::InvalidEdge) {
                ++walker_invalid_edge_fallbacks_;
                ++walker_role_stats_[role_index].fallback_invalid_edge;
            }
            const SDL_Color missing=debug_open_ ? SDL_Color{255,90,60,255}:
                SDL_Color{126,126,116,255};
            if (courier.role==simulation::CourierRole::FireInspector) ++fire_inspector_fallback_draws_;
            if (food_market_visual(visual_role)) ++market_walker_fallback_draws_;
            if (visual_role==assets::WalkerVisualRole::Service) ++service_walker_fallback_draws_;
            if (visual_role==assets::WalkerVisualRole::HealthWorker) ++health_walker_fallback_draws_;
            if (!draw_agent(position,missing,cargo_color,courier.cargo,
                            marker_shift)) return false;
            return !debug_open_ || SDL_RenderDebugText(renderer_,static_cast<float>(ground.x)+5,
                static_cast<float>(ground.y)-20,"W?");
        }
        if (courier.role==simulation::CourierRole::FireInspector) ++fire_inspector_fallback_draws_;
        if (food_market_visual(visual_role)) ++market_walker_fallback_draws_;
        if (visual_role==assets::WalkerVisualRole::Service) ++service_walker_fallback_draws_;
        if (visual_role==assets::WalkerVisualRole::HealthWorker) ++health_walker_fallback_draws_;
        if (!draw_agent(position,marker_color,cargo_color,courier.cargo,marker_shift)) return false;
        if (debug_open_ && (courier.role==simulation::CourierRole::Food ||
                            courier.role==simulation::CourierRole::Service ||
                            courier.role==simulation::CourierRole::FireInspector ||
                            courier.role==simulation::CourierRole::MarketPotteryInbound ||
                            courier.role==simulation::CourierRole::MarketFoodInbound ||
                            courier.role==simulation::CourierRole::MarketPotteryDistribution ||
                            courier.role==simulation::CourierRole::MarketFoodDistribution)) {
            const auto ground=camera_.world_to_screen(world_for(position));
            const char* label=courier.role==simulation::CourierRole::Food ? "F":
                courier.role==simulation::CourierRole::Service ? "S":
                courier.role==simulation::CourierRole::FireInspector ? "FI":
                courier.role==simulation::CourierRole::MarketPotteryInbound ? "WP":
                courier.role==simulation::CourierRole::MarketFoodInbound ? "FF":
                courier.role==simulation::CourierRole::MarketPotteryDistribution ? "MP":"MF";
            return SDL_RenderDebugText(renderer_,static_cast<float>(ground.x)+5,
                                       static_cast<float>(ground.y)-20,label);
        }
        return true;
    };
    if (!simulation::production_profile(rules_)) {
        const auto position=world_->courier_position();
        if (position) {
            const auto ground=world_for(*position);
            instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxWalker,1},{},
                                 simulation::Object::Empty,simulation::CourierId::Clay,*position});
        }
    } else for (const auto& state:world_->couriers()) {
        if (!walker_live_visible(state)) continue;
        const auto position=world_->courier_position(state.id);
        if (!position) continue;
        const auto ground=world_for(*position);
        instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxWalker,
                              static_cast<std::uint32_t>(state.id)},{},
                             simulation::Object::Empty,state.id,*position});
    }
    if (!road_start_ && hovered_ && tool_!=(simulation::production_profile(rules_) ? 5:4)) {
        const auto result=preview(*hovered_);
        const auto object=tool_==2 ? simulation::Object::ClaySource:
            tool_==3 ? simulation::Object::Pottery:
            tool_==4 ? simulation::Object::Warehouse:
            tool_==7 ? simulation::Object::Household:
            tool_==8 ? simulation::Object::Farm:
            tool_==9 ? simulation::Object::ServicePost:
            tool_==13 ? simulation::Object::HealthPost:
            tool_==12 ? simulation::Object::Well:
            tool_==11 ? simulation::Object::FireWatch:
            tool_==0 ? simulation::Object::Market:simulation::Object::Empty;
        const auto role=simulation::production_profile(rules_) ?
            visual_role(*hovered_,object,true):std::nullopt;
        if (result.accepted && (object==simulation::Object::Well || object==simulation::Object::HealthPost ||
            (role && building_visuals_active() && building_sprite_ &&
             building_profile_ && building_entry(*role)))) {
            const auto ground=building_visual_ground(*hovered_,object);
            instances.push_back({{ground.y,ground.x,scene::WorldVisualLayer::SandboxBuilding,
                                  std::numeric_limits<unsigned>::max()},*hovered_,object,
                                 simulation::CourierId::Clay,{},true});
        }
    }
    // Keep the existing logical front-cell order while lifting presentation.
    if (background_.elevated()) for (auto& instance:instances) {
        auto position=instance.position;
        if (instance.key.layer!=scene::WorldVisualLayer::SandboxWalker) {
            auto cell=instance.cell;
            if (instance.key.layer==scene::WorldVisualLayer::SandboxBuilding)
                cell=simulation::building_front_cell(rules_,world_->rule_version(),instance.object,cell);
            position={static_cast<double>(cell.x),static_cast<double>(cell.y)};
        }
        const double flat_y=(position.x+position.y-2*geometry_.border)*20.0+20.0;
        instance.key.depth+=flat_y-world_for(position).y;
    }
    // Sandbox roads are flat surfaces. Retain the historical Snapshot and F7
    // comparison paths, including their old shared road/object ordering.
    const bool road_ground_pass=unified_depth_ &&
        background_.landscape_mode()!=LandscapeDebugMode::Snapshot;
    const auto spatial_begin=road_ground_pass ?
        std::partition(instances.begin(),instances.end(),[](const DrawInstance& instance) {
            return instance.object==simulation::Object::Road;
        }):instances.begin();
    const auto road_ground_count=static_cast<std::size_t>(spatial_begin-instances.begin());
    const auto less=[](const DrawInstance& a,const DrawInstance& b) {
        return a.key<b.key;
    };
    std::sort(instances.begin(),spatial_begin,less);
    std::sort(spatial_begin,instances.end(),less);
    const auto draw_instance_body=[&](std::size_t index)->bool {
        const auto& instance=instances[index];
        if (instance.key.layer!=scene::WorldVisualLayer::SandboxWalker)
            return draw_object(instance.cell,instance.object,instance.placement_preview);
        const auto id=static_cast<unsigned>(instance.courier);
        const auto position=instance.position;
        if (!simulation::production_profile(rules_)) {
            return draw_agent(position,{255,255,255,255},{255,215,20,255},
                              world_->courier_cargo(),0);
        }
        const auto& courier=world_->courier(instance.courier);
        const auto pending=courier.route_pending;
        const SDL_Color color=debug_open_ ? (pending ? SDL_Color{255,120,40,255}:
            courier.role==simulation::CourierRole::Clay ?
                (id==1 ? SDL_Color{100,240,255,255}:SDL_Color{50,175,255,255}):
            id==2 ? SDL_Color{255,225,130,255}:
            id==3 ? SDL_Color{110,255,130,255}:
            courier.role==simulation::CourierRole::Food ? SDL_Color{235,200,70,255}:
            courier.role==simulation::CourierRole::Service ? SDL_Color{120,205,220,255}:
            courier.role==simulation::CourierRole::FireInspector ? SDL_Color{200,150,95,255}:
            courier.role==simulation::CourierRole::MarketPotteryInbound ? SDL_Color{205,145,90,255}:
            courier.role==simulation::CourierRole::MarketFoodInbound ? SDL_Color{180,165,70,255}:
            courier.role==simulation::CourierRole::MarketPotteryDistribution ? SDL_Color{210,125,120,255}:
            courier.role==simulation::CourierRole::MarketFoodDistribution ? SDL_Color{160,150,85,255}:
            SDL_Color{225,145,255,255}) :
            (pending ? SDL_Color{151,104,74,255}:
             courier.role==simulation::CourierRole::Clay ? SDL_Color{104,126,132,255}:
             courier.role==simulation::CourierRole::Pottery ? SDL_Color{143,121,95,255}:
             courier.role==simulation::CourierRole::Food ? SDL_Color{137,125,77,255}:
             courier.role==simulation::CourierRole::Service ? SDL_Color{105,120,126,255}:
             courier.role==simulation::CourierRole::FireInspector ? SDL_Color{132,110,85,255}:
             courier.role==simulation::CourierRole::MarketPotteryInbound ? SDL_Color{128,104,79,255}:
             courier.role==simulation::CourierRole::MarketFoodInbound ? SDL_Color{121,112,73,255}:
             courier.role==simulation::CourierRole::MarketPotteryDistribution ? SDL_Color{132,91,87,255}:
             courier.role==simulation::CourierRole::MarketFoodDistribution ? SDL_Color{112,105,72,255}:
             SDL_Color{112,132,108,255});
        const bool food_cargo=courier.role==simulation::CourierRole::Food ||
            courier.role==simulation::CourierRole::MarketFoodInbound ||
            courier.role==simulation::CourierRole::MarketFoodDistribution;
        const SDL_Color cargo=debug_open_ ? (courier.role==simulation::CourierRole::Clay ?
            SDL_Color{25,95,255,255}:food_cargo ?
            SDL_Color{240,190,30,255}:courier.role==simulation::CourierRole::Household ?
            SDL_Color{255,90,150,255}:SDL_Color{240,45,190,255}) :
            (courier.role==simulation::CourierRole::Clay ? SDL_Color{72,94,130,255}:
             food_cargo ? SDL_Color{153,132,68,255}:
             courier.role==simulation::CourierRole::Household ? SDL_Color{148,92,104,255}:
             SDL_Color{133,86,119,255});
        const int shift=id==1 ? -5:id==2 ? 5:id==3 ? 0:id==4 ? -10:id==5 ? 10:14;
        return draw_courier(instance.courier,position,color,cargo,shift);
    };
    const auto draw_instance=[&](std::size_t index) {
        if (!draw_instance_body(index)) return false;
        record_visual_hit(instances[index]); return true;
    };
    painter_stats_={};
    painter_stats_.stored_order_builds=background_.stored_order_builds();
    painter_stats_.road_ground_pass=road_ground_pass;
    painter_stats_.road_ground_items=road_ground_count;
    const auto replacement_for=[&](std::size_t i)->std::uint8_t {
        const auto& item=background_.draw_items()[i];
        if (item.regenerated) return 0; // Multi-cell landscape never replaces road ground.
        const auto& plan=background_.plan();
        maps::GridCell cell;
        if (item.footprint) {
            const auto& footprint=plan.footprints[item.plan_index];
            if (footprint.width_cells!=1 || footprint.height_cells!=1) return 0;
            cell=footprint.origin;
        } else cell=plan.cells[item.plan_index].storage;
        if (cell.x>=static_cast<unsigned>(world_->width()) ||
            cell.y>=static_cast<unsigned>(world_->height())) return 0;
        return road_ground_replacements_[static_cast<std::size_t>(cell.y)*
            static_cast<std::size_t>(world_->width())+cell.x];
    };
    background_.begin_frame();
    for (std::size_t i=0;i<background_.draw_items().size();++i)
        if (replacement_for(i)!=1 && !background_.draw_ground_item(i,render_camera)) return false;
    // Valid new roads stay alpha-128 over their old ground. Draw that ground once
    // before the painter so its raised grass cannot overwrite an earlier road.
    if (road_start_ && road_preview_.valid && road_preview_.new_road_count)
        for (std::size_t i=0;i<background_.draw_items().size();++i)
            if (replacement_for(i)==2 && !background_.draw_item(i,render_camera)) return false;
    const auto draw_stored=[&](std::size_t i) {
        return replacement_for(i)!=0 || background_.draw_item(i,render_camera);
    };
    // The same prepared road raster/preview draws once, after the existing
    // Ground and preview backdrop, before any spatial object or walker.
    for (std::size_t i=0;i<road_ground_count;++i)
        if (!draw_instance(i)) return false;
    if (unified_depth_) {
        scene::WorldMergeStats stats;
        const auto spatial=std::span<const DrawInstance>{instances}.subspan(road_ground_count);
        if (!scene::merge_world_draw_streams(background_.draw_items(),spatial,
            draw_stored,
            [&](std::size_t i){return draw_instance(road_ground_count+i);},stats)) return false;
        static_cast<scene::WorldMergeStats&>(painter_stats_)=stats;
    } else {
        for (std::size_t i=0;i<background_.draw_items().size();++i)
            if (!draw_stored(i)) return false;
        painter_stats_.sandbox_items=instances.size();
        for (std::size_t i=0;i<instances.size();++i)
            if (!draw_instance(i)) return false;
    }
    if (desirability_overlay_ && simulation::desirability_profile(rules_)) {
        SDL_BlendMode previous_blend=SDL_BLENDMODE_NONE;
        if (!SDL_GetRenderDrawBlendMode(renderer_,&previous_blend) ||
            !SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND)) return false;
        struct RestoreBlend {
            SDL_Renderer* renderer;
            SDL_BlendMode mode;
            ~RestoreBlend() { (void)SDL_SetRenderDrawBlendMode(renderer,mode); }
        } restore_blend{renderer_,previous_blend};
        for (const auto& b:world_->buildings()) if (b.placed && b.kind==simulation::Object::Household) {
            const auto score=world_->household_desirability(b.id);
            const auto color=housing_palette[static_cast<std::size_t>(simulation::desirability_level_cap(score))];
            for (const auto cell:simulation::building_footprint_cells(rules_,world_->rule_version(),b.kind,b.cell)) {
                const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
                if (!draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true,color.a/255.0F)) return false;
            }
            if (debug_open_) {
                const auto ground=camera_.world_to_screen(building_visual_ground(b.cell,b.kind));
                if (!SDL_SetRenderDrawColor(renderer_,240,244,250,255) ||
                    !draw_text(ground.x,ground.y,std::to_string(score),80*layout_.scale)) return false;
            }
        }
    }
    if (water_overlay_ && simulation::water_profile(rules_)) {
        SDL_BlendMode previous=SDL_BLENDMODE_NONE;
        if (!SDL_GetRenderDrawBlendMode(renderer_,&previous) ||
            !SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND)) return false;
        bool ok=true;
        for (const auto& b:world_->buildings()) if (b.placed && b.kind==simulation::Object::Household) {
            const auto color=world_->household_has_water(b.id) ? SDL_Color{45,195,230,65}:SDL_Color{220,160,60,65};
            for (const auto cell:simulation::building_footprint_cells(rules_,world_->rule_version(),b.kind,b.cell)) {
                const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
                ok=draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true,color.a/255.0F) && ok;
            }
        }
        if (!SDL_SetRenderDrawBlendMode(renderer_,previous) || !ok) return false;
    }
    if (health_overlay_ && simulation::health_profile(rules_)) {
        SDL_BlendMode previous;
        if (!SDL_GetRenderDrawBlendMode(renderer_,&previous) ||
            !SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND)) return false;
        bool ok=true;
        for (const auto& b:world_->buildings()) if (b.placed && b.kind==simulation::Object::Household) {
            const auto color=world_->household_sick(b.id) ? SDL_Color{196,90,70,65}:
                world_->household_health_protected(b.id) ? SDL_Color{90,155,130,55}:SDL_Color{200,156,69,55};
            for (const auto cell:simulation::building_footprint_cells(rules_,world_->rule_version(),b.kind,b.cell)) {
                const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
                ok=draw_diamond({top.x,top.y-20},color.r,color.g,color.b,true,color.a/255.0F) && ok;
            }
            if (debug_open_) {
                const auto ground=camera_.world_to_screen(building_visual_ground(b.cell,b.kind));
                ok=SDL_SetRenderDrawColor(renderer_,240,244,250,255) &&
                    draw_text(ground.x,ground.y,"Health "+std::to_string(world_->household_health_risk(b.id)),100*layout_.scale) && ok;
            }
        }
        if (!SDL_SetRenderDrawBlendMode(renderer_,previous) || !ok) return false;
    }
    if (selected_) {
        const auto owner=world_->building_owner_at(*selected_);
        if (owner) {
            const auto& building=world_->building(*owner);
            for (const auto cell:simulation::building_footprint_cells(rules_,world_->rule_version(),building.kind,
                                                                       building.cell)) {
                const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
                if (!draw_diamond({top.x,top.y-20},255,255,255,false)) return false;
            }
        } else {
            const auto top=world_for({static_cast<double>(selected_->x),
                                      static_cast<double>(selected_->y)});
            if (!draw_diamond({top.x,top.y-20},255,255,255,false)) return false;
        }
    }
    if (road_start_) {
        if (!road_preview_.valid) for (const auto cell:road_preview_.cells) {
            const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
            const bool blocked=sandbox_ui::road_cell_blocked(road_preview_,cell);
            if (!draw_diamond({top.x,top.y-20},blocked ? 245:151,
                blocked ? 60:161,blocked ? 65:121,true,blocked ? 0.65F:0.25F)) return false;
        }
        return true;
    }
    if (hovered_ && tool_!=(simulation::production_profile(rules_) ? 5 : 4)) {
        const auto result=preview(*hovered_);
        const auto object=tool_==2 ? simulation::Object::ClaySource:
            tool_==3 ? simulation::Object::Pottery:
            tool_==4 ? simulation::Object::Warehouse:
            tool_==7 ? simulation::Object::Household:
            tool_==8 ? simulation::Object::Farm:
            tool_==9 ? simulation::Object::ServicePost:
            tool_==13 ? simulation::Object::HealthPost:
            tool_==12 ? simulation::Object::Well:
            tool_==11 ? simulation::Object::FireWatch:
            tool_==0 ? simulation::Object::Market:simulation::Object::Empty;
        const auto role=simulation::production_profile(rules_) ?
            visual_role(*hovered_,object,true):std::nullopt;
        const auto* entry=role && building_profile_ ? building_entry(*role):nullptr;
        for (const auto cell:simulation::building_footprint_cells(rules_,world_->rule_version(),object,*hovered_)) {
            const auto top=world_for({static_cast<double>(cell.x),static_cast<double>(cell.y)});
            const bool outline=result.accepted && (object==simulation::Object::Well || object==simulation::Object::HealthPost ||
                (entry && building_visuals_active() && building_sprite_));
            if (!draw_diamond({top.x,top.y-20},result.accepted?70:255,
                result.accepted?245:65,result.accepted?100:65,!outline)) return false;
        }
    }
    return true;
}
std::vector<simulation::BuildingId> SandboxView::placed_buildings() const {
    std::vector<simulation::BuildingId> result;
    for (const auto& b:world_->buildings()) if (b.placed) result.push_back(b.id);
    return result;
}
std::optional<simulation::BuildingId> SandboxView::selected_building() const {
    if (selected_landscape_ || selected_walker_) return {};
    return selected_ ? world_->building_owner_at(*selected_) : std::nullopt;
}
const simulation::RoadConnectivityReport* SandboxView::road_connectivity_report() const {
    if (!world_ || !road_connectivity_report_ || road_connectivity_world_!=world_.get() ||
        road_connectivity_report_->road_revision!=world_->road_revision() ||
        road_connectivity_report_->command_sequence!=world_->command_sequence() ||
        selected_building()!=road_connectivity_report_->source.id) return nullptr;
    return &*road_connectivity_report_;
}
void SandboxView::request_road_connectivity_diagnosis() {
    const auto source=selected_building();
    if (!source) { last_message_="Select a courier's building, then press G"; return; }
    struct Pair { simulation::CourierId courier; simulation::BuildingId target; bool candidate; };
    std::vector<Pair> pairs;
    for (const auto& courier:world_->couriers()) {
        if (!courier.enabled || courier.owner!=*source) continue;
        auto targets=simulation::road_connectivity_targets(*world_,courier.id);
        const auto dispatch=world_->courier_dispatch_status(courier.id);
        const auto actual=dispatch.selected_target ? dispatch.selected_target:
            courier.phase!=simulation::CourierPhase::IdleAtWorkshop ?
                std::optional{courier.target}:std::nullopt;
        if (actual) {
            const auto found=std::find(targets.begin(),targets.end(),*actual);
            if (found!=targets.end()) std::rotate(targets.begin(),found,found+1);
        }
        for (const auto target:targets)
            pairs.push_back({courier.id,target,!actual || target!=*actual});
    }
    if (pairs.empty()) {
        road_connectivity_report_.reset();road_connectivity_courier_.reset();
        last_message_="No compatible target building; no road query performed";
        return;
    }
    auto chosen=pairs.begin();
    if (const auto* previous=road_connectivity_report()) {
        const auto found=std::find_if(pairs.begin(),pairs.end(),[&](const auto& pair) {
            return road_connectivity_courier_==pair.courier && previous->target.id==pair.target;
        });
        if (found!=pairs.end()) chosen=std::next(found)==pairs.end() ? pairs.begin():std::next(found);
    }
    auto report=simulation::diagnose_road_connectivity(*world_,*source,chosen->target);
    road_connectivity_report_=std::move(report);road_connectivity_world_=world_.get();
    road_connectivity_courier_=chosen->courier;
    road_connectivity_target_candidate_=chosen->candidate;
    ++road_connectivity_check_count_;
    panel_scroll_=0;income_open_=false;panel_open_=true;update_layout(true);
    last_message_=std::string("Road check: ")+
        simulation::road_connectivity_category_name(road_connectivity_report_->category)+
        "; G checks the next compatible target";
}
std::vector<std::string> SandboxView::road_connectivity_lines() const {
    const auto source=selected_building();
    if (!source) return {};
    const bool owns_courier=std::any_of(world_->couriers().begin(),world_->couriers().end(),
        [&](const auto& courier) { return courier.enabled && courier.owner==*source; });
    if (!owns_courier) return {};
    const auto* report=road_connectivity_report();
    if (!debug_open_) {
        const bool no_road=std::any_of(world_->couriers().begin(),world_->couriers().end(),[&](const auto& courier) {
            return courier.enabled && courier.owner==*source &&
                world_->courier_dispatch_status(courier.id).status==simulation::CourierDispatchStatus::NoRoad;
        });
        return no_road ? std::vector<std::string>{"F1, then G: check road connections"}:std::vector<std::string>{};
    }
    if (!report) return {"G: check roads to a compatible target","Checks run only when G is pressed."};
    const auto cell=[](simulation::Cell point) {
        return "("+std::to_string(point.x)+","+std::to_string(point.y)+")";
    };
    std::vector<std::string> lines{
        std::string(object_name(report->source.kind))+" #"+
            std::to_string(static_cast<unsigned>(report->source.id))+" -> "+
            object_name(report->target.kind)+" #"+std::to_string(static_cast<unsigned>(report->target.id)),
        road_connectivity_target_candidate_ ? "Candidate target; not a dispatch choice.":"Actual selected/current courier target.",
        std::string("Road check: ")+simulation::road_connectivity_category_name(report->category),
        "Source entrances: "+std::to_string(report->source.entrances.size())+
            " | Target: "+std::to_string(report->target.entrances.size()),
        "Transport components: "+std::to_string(report->components.size()),
        "Measured tick "+std::to_string(report->measured_tick)+" | road rev "+std::to_string(report->road_revision),
        "G: check next compatible target"};
    if (report->route) lines.push_back("Authoritative route: "+std::to_string(report->route->size())+" cells");
    if (!report->unsupported_reason.empty()) lines.push_back(report->unsupported_reason);
    const auto add_endpoint=[&](const auto& endpoint,const char* label) {
        std::string footprint=std::string(label)+" footprint";
        for (const auto point:endpoint.footprint) footprint+=" "+cell(point);
        lines.push_back(std::move(footprint));
        for (const auto& entrance:endpoint.entrances)
            lines.push_back(std::string(label)+" entrance "+cell(entrance.building_cell)+" <-> "+cell(entrance.road_cell));
        for (std::size_t i=0;i<std::min<std::size_t>(2,endpoint.rejected_entrances.size());++i) {
            const auto& edge=endpoint.rejected_entrances[i];
            lines.push_back(std::string(label)+" rejected "+cell(edge.from.cell)+" -> "+cell(edge.to.cell));
            lines.push_back(std::string(simulation::road_connectivity_category_name(edge.category))+" | height "+
                std::to_string(edge.from.height)+" -> "+std::to_string(edge.to.height));
            if (!edge.reason.empty()) lines.push_back(edge.reason);
        }
        if (!endpoint.rejected_entrances.empty()) lines.push_back(std::string(label)+" rejected entrances: "+
            std::to_string(endpoint.rejected_entrances.size())+"; showing at most 2.");
    };
    add_endpoint(report->source,"Source");add_endpoint(report->target,"Target");
    const auto ids=[](const auto& components) {
        std::string text;
        for (const auto id:components) text+=(text.empty() ? "":" ")+std::to_string(id);
        return text.empty() ? "none":text;
    };
    lines.push_back("Source components: "+ids(report->source_components));
    lines.push_back("Target components: "+ids(report->target_components));
    for (std::size_t i=0;!report->route && i<std::min<std::size_t>(3,report->blocked_edges.size());++i) {
        const auto& edge=report->blocked_edges[i];
        lines.push_back(std::string(edge.candidate ? "Candidate edge ":"Blocked edge ")+
            cell(edge.from.cell)+" -> "+cell(edge.to.cell));
        lines.push_back(std::string(simulation::road_connectivity_category_name(edge.category))+" | height "+
            std::to_string(edge.from.height)+" -> "+std::to_string(edge.to.height));
        if (!edge.reason.empty()) lines.push_back(edge.reason);
        if (edge.from.protected_original || edge.to.protected_original)
            lines.push_back("Protected original cells on this edge.");
        if (edge.from.fixed_gate || edge.to.fixed_gate) lines.push_back("Fixed gate edge; existing passage rule applies.");
        if (edge.candidate_cell) lines.push_back("Candidate Road "+cell(*edge.candidate_cell)+(edge.place_road_allowed ?
            ": purchase allowed; full repair unproved.":": purchase rejected."));
    }
    if (!report->route && report->blocked_edge_count) lines.push_back("Blocked edges measured: "+
        std::to_string(report->blocked_edge_count)+"; showing at most 3. No unique repair claimed.");
    for (const auto& courier:report->couriers) if (road_connectivity_courier_==courier.id) {
        lines.push_back("Dispatch at check: "+std::string(simulation::courier_dispatch_status_name(courier.dispatch_status)));
        lines.push_back("Workers "+std::to_string(courier.workers_assigned)+"/"+std::to_string(courier.workers_required)+
            " | output stock "+std::to_string(courier.output_stock));
        lines.push_back(std::string("Phase ")+simulation::courier_phase_name(courier.phase)+
            " | cargo "+std::to_string(courier.cargo)+" reserved "+std::to_string(courier.reserved));
        lines.push_back(courier.route_pending ? "Route pending: yes":"Route pending: no");
        lines.push_back("Selected dispatch target: "+(courier.selected_target ?
            std::to_string(static_cast<unsigned>(*courier.selected_target)):std::string("none")));
        lines.push_back("Route cache rev "+(courier.cached_revision==UINT64_MAX ? std::string("not prepared"):
            std::to_string(courier.cached_revision))+" | road rev "+std::to_string(courier.road_revision));
    }
    return lines;
}
bool SandboxView::fire_watch_selected() const {
    const auto id=selected_building();
    return id && world_->building(*id).kind==simulation::Object::FireWatch;
}
bool SandboxView::status_first_selected() const {
    const auto id=selected_building();
    return (id && world_->building(*id).kind==simulation::Object::Well) ||
        fire_watch_selected() || (simulation::desirability_profile(rules_) && id &&
        world_->building(*id).kind==simulation::Object::Household);
}
std::vector<std::string> SandboxView::wrap_panel_lines(const std::vector<std::string>& lines) const {
    const auto columns=static_cast<std::size_t>(std::max(1,
        (layout_.panel.w-20*layout_.scale)/(10*layout_.scale)));
    return sandbox_ui::wrap_text(lines,columns);
}
int SandboxView::building_list_y() const {
    return layout_.panel.y+(status_first_selected() ?
        52+static_cast<int>(inspection_lines().size())*17:46)*layout_.scale;
}
std::vector<std::string> SandboxView::demolition_hint_lines() const {
    const auto id=selected_building();
    if (!id) return {};
    const auto status=world_->demolition_status(*id);
    std::string hint=status.allowed ? "Demolition gives no refund.":status.reason;
    if (fire_watch_selected() && status.blocker==simulation::DemolitionBlocker::OwnedActiveCourier) {
        const auto found=std::find_if(world_->couriers().begin(),world_->couriers().end(),
            [&](const auto& c) { return c.owner==*id &&
                c.phase!=simulation::CourierPhase::IdleAtWorkshop; });
        if (found!=world_->couriers().end()) hint=found->route_pending ?
            "Inspector route interrupted; reconnect roads.":
            found->phase==simulation::CourierPhase::Returning ?
            "Waiting for the Inspector to return.":"Inspector is still on patrol.";
    }
    return wrap_panel_lines({hint});
}
int SandboxView::demolition_hint_extra_height() const {
    if (!selected_building() || !world_->demolition_supported()) return 0;
    return std::max(0,static_cast<int>(demolition_hint_lines().size())-2)*17*layout_.scale;
}
std::vector<std::string> SandboxView::inspection_lines() const {
    std::vector<std::string> lines;
    if (selected_ && !selected_walker_ && world_->map_permissions()) {
        const auto& permissions=*world_->map_permissions();
        if (const auto* gate=permissions.fixed_gate(*selected_)) {
            lines={"Original gate #"+std::to_string(gate->id.value),
                permissions.fixed_passage(*selected_) ? "Fixed passage":"Protected gate structure",
                "OpenEmperor open passage rule"};
            for (std::size_t side=0;side<2;++side) {
                const auto corridor=side==0 ? gate->corridor.front():gate->corridor.back();
                lines.push_back(std::string("Opening ")+(side==0 ? "A":"B")+
                    (world_->transport_edge_allowed(corridor,gate->openings[side]) ?
                        " connected":" not connected"));
            }
            if (debug_open_) {
                const auto provenance=maps::landscape_inspection_lines(background_.plan(),
                    {static_cast<unsigned>(selected_->x),static_cast<unsigned>(selected_->y)},
                    background_.elevated(),&camera_);
                lines.insert(lines.end(),provenance.begin(),provenance.end());
            }
            return wrap_panel_lines(lines);
        }
    }
    if (debug_open_ && selected_walker_) {
        const auto found=std::find_if(world_->couriers().begin(),world_->couriers().end(),
            [&](const auto& courier){return courier.id==*selected_walker_;});
        if (found!=world_->couriers().end()) {
            std::vector<std::string> walker_lines{
                "Visible walker #"+std::to_string(static_cast<unsigned>(found->id)),
                std::string("Role: ")+courier_role_label(found->role),
                "Owner: "+std::to_string(static_cast<unsigned>(found->owner))+
                    " target: "+std::to_string(static_cast<unsigned>(found->target)),
                std::string("Phase: ")+simulation::courier_phase_name(found->phase),
                "Cargo: "+std::to_string(found->cargo),
                std::string("Goods: ")+courier_good_label(found->good)};
            const auto role=walker_visual_role(found->role);
            const bool configured=role && walker_profile_ && walker_profile_->find(*role);
            const auto source=walker_source_==VisualProfileSource::Custom ? walker_source_:
                food_market_visual(role) && (market_walker_extension_ || !configured) ? market_walker_source_:
                role==assets::WalkerVisualRole::FireInspector && (fire_inspector_extension_ || !configured) ?
                    fire_inspector_source_:
                role==assets::WalkerVisualRole::Service && (service_walker_extension_ || !configured) ?
                    service_walker_source_:
                role==assets::WalkerVisualRole::HealthWorker && (health_walker_extension_ || !configured) ?
                    health_walker_source_:walker_source_;
            walker_lines.push_back(std::string("Family: ")+(role ? assets::walker_role_name(*role):"unassigned")+
                " | source: "+visual_profile_source_name(source));
            if (found->path.size()>1 && found->path_vertex<found->path.size()-1) {
                const auto from=found->path[found->path_vertex],to=found->path[found->path_vertex+1];
                const auto direction=current_storage_direction(*found);
                walker_lines.push_back("Edge: ("+std::to_string(from.x)+","+std::to_string(from.y)+
                    ")->("+std::to_string(to.x)+","+std::to_string(to.y)+") "+
                    (direction ? storage_direction_label(*direction):"invalid"));
            } else {
                walker_lines.push_back("Edge: none | static pose");
            }
            walker_lines.push_back(walker_clip_status(*found));
            return wrap_panel_lines(walker_lines);
        }
    }
    if (debug_open_ && selected_ && (selected_landscape_ || world_->object_at(*selected_)==simulation::Object::Empty))
        return wrap_panel_lines(maps::landscape_inspection_lines(background_.plan(),
            {static_cast<unsigned>(selected_->x),static_cast<unsigned>(selected_->y)},background_.elevated(),&camera_));
    const auto add_maintenance=[&](simulation::BuildingId id) {
        const auto& b=world_->building(id);
        const auto cost=simulation::maintenance_cost(rules_,b.kind);
        if (!cost) {
            if (debug_open_ && simulation::maintenance_profile(rules_) && b.kind==simulation::Object::Household)
                lines.push_back("Maintenance none.");
            return;
        }
        lines.push_back("Maintenance: "+std::to_string(cost)+" every "+
            std::to_string(simulation::Rules::maintenance_interval_ticks)+" ticks");
        const auto due=world_->maintenance_due_in(id);
        if (due) lines.push_back("Next due: "+std::to_string(*due ? *due:
            simulation::Rules::maintenance_interval_ticks)+" ticks");
        if (!b.operating_enabled) lines.push_back("Maintenance continues while paused.");
    };
    const auto add_house_income=[&](simulation::BuildingId id) {
        const auto& home=world_->building(id);
        const auto demand=world_->household_demand_status(id);
        lines.push_back(std::string("Food: ")+(demand.food_available ? "available":"missing")+
            " ("+std::to_string(home.food_stock)+")");
        lines.push_back(std::string("Pottery: ")+(demand.pottery_available ? "available":"missing")+
            " ("+std::to_string(home.pottery_stock)+")");
        lines.push_back(demand.service_available ? "Service active: "+
            std::to_string(world_->household_service_remaining(id))+" ticks left":"Service: missing");
        if (demand.burning) lines.push_back("Fire blocks demand now.");
        if (demand.sick) lines.push_back("Sickness blocks demand now.");
        lines.push_back("Next demand: "+std::to_string(simulation::Rules::household_demand_ticks-
            home.demand_progress)+" ticks");
        lines.push_back(home.last_demand_status==0 ? "Last demand: none yet":
            home.last_demand_status==1 ? "Last demand: supplied; tax paid":"Last demand: unmet; no tax");
        lines.push_back("Taxes contributed: "+std::to_string(world_->household_tax_contributed(id)));
        lines.push_back("Payment depends on conditions at the demand deadline.");
    };
    const auto add_connectivity=[&]() {
        const auto diagnostic=road_connectivity_lines();
        lines.insert(lines.end(),diagnostic.begin(),diagnostic.end());
    };
    if (fire_watch_selected()) {
        const auto id=*selected_building();
        const auto& b=world_->building(id);
        lines.push_back("Fire Watch #"+std::to_string(static_cast<std::uint32_t>(id)));
        lines.push_back("Workers assigned "+std::to_string(world_->workers_assigned(id))+"/"+
            std::to_string(world_->workforce_required(id)));
        lines.push_back(std::string("Operation: ")+(b.operating_enabled ? "Running":"Paused"));
        lines.push_back(std::string("Priority: ")+simulation::workforce_priority_name(b.workforce_priority));
        for (const auto& c:world_->couriers()) if (c.owner==id) {
            lines.push_back(std::string("Inspector phase: ")+simulation::delivery_phase_name(c.phase));
            lines.push_back("Current target: "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
                std::string("-"):std::to_string(static_cast<std::uint32_t>(c.target))));
            const auto decision=world_->courier_dispatch_status(c.id);
            lines.push_back(std::string("Route status: ")+simulation::courier_dispatch_status_name(decision.status));
            if (c.phase!=simulation::CourierPhase::IdleAtWorkshop && b.operating_enabled)
                lines.push_back("Pause operation to prevent a new patrol.");
        }
        add_connectivity();
        add_maintenance(id);
        lines.push_back("City-wide protected "+std::to_string(world_->protected_buildings())+"/"+
            std::to_string(world_->fire_eligible_buildings()));
        lines.push_back("City-wide burning "+std::to_string(world_->burning_buildings()));
        if (debug_open_) {
            lines.push_back("Fireproof; footprint 1x1");
            const auto inspector=fire_inspector_display_stats();
            lines.push_back(inspector.active ? "Inspector: curated original walk clip":
                "Inspector marker: "+inspector.fallback_reason);
            const auto* entry=building_profile_ ? building_profile_->find(assets::BuildingVisualRole::FireWatch):nullptr;
            lines.push_back(std::string("Building visuals ")+(building_enabled_ ? "ON":"OFF"));
            lines.push_back(entry ? "SG3 "+entry->id.archive_relative_path.generic_string()+
                " #"+std::to_string(entry->id.image_index):"Fire Watch visual: fallback");
        }
        return wrap_panel_lines(lines);
    }
    const auto house_id=selected_building();
    if (house_id && world_->building(*house_id).kind==simulation::Object::HealthPost) {
        const auto id=*house_id;
        const auto& b=world_->building(id);
        lines.push_back("Health Post #"+std::to_string(static_cast<std::uint32_t>(id)));
        lines.push_back("Workers assigned "+std::to_string(world_->workers_assigned(id))+"/2");
        lines.push_back(std::string("Operation: ")+(b.operating_enabled ? "Running":"Paused"));
        lines.push_back(std::string("Priority: ")+simulation::workforce_priority_name(b.workforce_priority));
        for (const auto& c:world_->couriers()) if (c.owner==id) {
            lines.push_back(std::string("Worker phase: ")+simulation::delivery_phase_name(c.phase));
            lines.push_back("Current target: "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
                std::string("-"):std::to_string(static_cast<std::uint32_t>(c.target))));
            lines.push_back(std::string("Route status: ")+simulation::courier_dispatch_status_name(
                world_->courier_dispatch_status(c.id).status));
        }
        add_connectivity();
        add_maintenance(id);
        int houses=0,protected_houses=0,sick=0;
        for (const auto& h:world_->buildings()) if (h.kind==simulation::Object::Household) {
            ++houses; protected_houses+=world_->household_health_protected(h.id); sick+=world_->household_sick(h.id);
        }
        lines.push_back("City-wide protected Houses "+std::to_string(protected_houses)+"/"+std::to_string(houses));
        lines.push_back("City-wide sick Houses "+std::to_string(sick));
        lines.push_back(world_->building_on_fire(id) ? "ON FIRE: new visits suspended.":"Road connection required; no goods.");
        lines.push_back("Arrival cures illness and grants 2400 ticks protection.");
        return wrap_panel_lines(lines);
    }
    if (house_id && world_->building(*house_id).kind==simulation::Object::Well) {
        const auto& well=world_->building(*house_id);
        lines.push_back("Well #"+std::to_string(static_cast<std::uint32_t>(*house_id)));
        lines.push_back("Footprint 1x1; water radius "+std::to_string(simulation::Rules::water_radius));
        lines.push_back("Covered Houses "+std::to_string(world_->well_coverage_at(well.cell).households));
        add_maintenance(*house_id);
        lines.push_back("Local infrastructure; roads not required.");
        lines.push_back("Fireproof; desirability impact 0.");
        if (debug_open_) lines.push_back("Origin "+std::to_string(well.cell.x)+","+std::to_string(well.cell.y));
        return wrap_panel_lines(lines);
    }
    if (simulation::desirability_profile(rules_) && house_id &&
        world_->building(*house_id).kind==simulation::Object::Household) {
        const auto id=*house_id;
        const auto& b=world_->building(id);
        const int score=world_->household_desirability(id);
        lines.push_back("House #"+std::to_string(static_cast<std::uint32_t>(id)));
        add_house_income(id);
        add_maintenance(id);
        if (simulation::health_profile(rules_)) {
            lines.push_back(std::string("Health: ")+(world_->household_sick(id) ? "Sick":
                world_->household_health_protected(id) ? "Protected":"At risk"));
            lines.push_back("Health risk: "+std::to_string(world_->household_health_risk(id))+" / 100");
            if (world_->household_sick(id))
                lines.push_back("Illness remaining: "+std::to_string(world_->household_sickness_remaining(id))+" ticks");
            else if (world_->household_health_protected(id))
                lines.push_back("Protection remaining: "+std::to_string(world_->household_health_protection_remaining(id))+" ticks");
        }
        if (simulation::water_profile(rules_)) {
            const auto well=world_->nearest_water_source(id);
            lines.push_back(well ? "Water: Available":"No water access");
            if (well) {
                lines.push_back("Nearest Well #"+std::to_string(static_cast<std::uint32_t>(*well)));
                lines.push_back("Distance "+std::to_string(world_->building_distance(id,*well))+"/"+
                    std::to_string(simulation::Rules::water_radius));
            }
            lines.push_back("Water cap: "+std::to_string(well ? 2:0));
        }
        lines.push_back("House Level: "+std::to_string(world_->household_level(id)));
        lines.push_back("Historical development: "+std::to_string(world_->historical_household_level(id)));
        lines.push_back("Desirability: "+std::to_string(score));
        const int cap=simulation::desirability_level_cap(score);
        lines.push_back(std::string(simulation::water_profile(rules_) ? "Desirability cap: ":"Level cap: ")+std::to_string(cap));
        lines.push_back("Population: "+std::to_string(b.population)+" / "+
            std::to_string(world_->household_population_capacity(id)));
        if (b.population>world_->household_population_capacity(id))
            lines.push_back("Over capacity: one resident leaves per demand.");
        lines.push_back("Desirability sources:");
        int negative=0,positive=0;
        const auto sources=world_->household_desirability_sources(id);
        for (const auto& source:sources) {
            auto& count=source.contribution<0 ? negative:positive;
            if (count++>=3) continue;
            lines.push_back(object_name(source.kind)+std::string(" #")+
                std::to_string(static_cast<std::uint32_t>(source.id))+" "+
                (source.contribution>0 ? "+":"")+std::to_string(source.contribution));
        }
        if (sources.empty()) lines.push_back("None in radius 8.");
        lines.push_back("Fire: "+std::string(world_->building_on_fire(id) ? "ON FIRE":
            world_->building_fire_protected(id) ? "protected":"unprotected"));
        lines.push_back("Fulfilled "+std::to_string(b.fulfilled_demand)+"; missed "+std::to_string(b.missed_demand));
        lines.push_back("Full supply still required; score gives no goods or Service.");
        if (debug_open_) lines.push_back("Origin "+std::to_string(b.cell.x)+","+std::to_string(b.cell.y));
        return wrap_panel_lines(lines);
    }
    if (selected_ && world_->object_at(*selected_)==simulation::Object::Road) {
        constexpr const char* names[]{"neg_y","pos_x","pos_y","neg_x"};
        constexpr char hex[]="0123456789abcdef";
        const auto roads=sandbox_ui::road_neighbor_mask(*world_,*selected_);
        const auto entrances=sandbox_ui::entrance_mask(*world_,*selected_);
        lines.push_back("Road cell "+std::to_string(selected_->x)+", "+
                        std::to_string(selected_->y));
        lines.push_back(std::string("Road mask 0x")+hex[roads & 0x0fU]);
        lines.push_back("Road neighbors");
        for (unsigned bit=0;bit<4;++bit)
            lines.push_back(std::string(names[bit])+" "+
                ((roads & (1U<<bit))!=0 ? "yes":"no"));
        lines.push_back("Entrances");
        for (unsigned bit=0;bit<4;++bit)
            lines.push_back(std::string(names[bit])+" "+
                ((entrances & (1U<<bit))!=0 ? "building":"none"));
        const auto* entry=road_profile_ ? road_profile_->find(roads):nullptr;
        lines.push_back(std::string("Road asset configured ")+(entry ? "yes":"no"));
        lines.push_back(std::string("Road visuals ")+(road_visuals_active() ? "ON":"OFF"));
        if (entry) lines.push_back("SG3 "+entry->id.archive_relative_path.generic_string()+
                                  " #"+std::to_string(entry->id.image_index));
        return lines;
    }
    const auto id=selected_building();
    if (!id) {
        lines.push_back("Select a building or list entry");
        lines.push_back("Placed: "+std::to_string(placed_buildings().size()));
        if (simulation::market_profile(rules_)) lines.push_back("Income details: T / Income");
        return lines;
    }
    const auto& b=world_->building(*id);
        if (world_->demolition_supported()) {
            const auto status=world_->demolition_status(*id);
            lines.push_back(status.reason);
            if (!status.stored_goods_summary.empty()) lines.push_back("Stored: "+status.stored_goods_summary);
        }
    const auto number=std::to_string(static_cast<unsigned>(*id));
    lines.push_back(object_name(b.kind)+std::string(" #")+number);
    add_connectivity();
    if (simulation::market_profile(rules_) && b.kind==simulation::Object::Household) add_house_income(*id);
    add_maintenance(*id);
    if (simulation::fire_profile(rules_) && simulation::fire_eligible(b.kind)) {
        lines.push_back("Fire risk: "+std::to_string(b.fire_risk)+"/"+
            std::to_string(simulation::Rules::fire_risk_threshold));
        lines.push_back(world_->building_on_fire(*id) ?
            "ON FIRE - "+std::to_string(world_->fire_remaining(*id))+" ticks remaining":
            world_->building_fire_protected(*id) ?
            "Protection active - "+std::to_string(world_->fire_protection_remaining(*id))+" ticks":
            "Unprotected");
    }
    const auto footprint=simulation::building_footprint(rules_,world_->rule_version(),b.kind);
    const auto front=simulation::building_front_cell(rules_,world_->rule_version(),b.kind,b.cell);
    lines.push_back("Footprint "+std::to_string(footprint.width)+"x"+
                    std::to_string(footprint.height));
    lines.push_back("Origin "+std::to_string(b.cell.x)+", "+std::to_string(b.cell.y));
    lines.push_back("Front "+std::to_string(front.x)+", "+std::to_string(front.y));
    if (debug_open_) {
        std::string occupied="Occupied";
        for (const auto cell:simulation::building_footprint_cells(rules_,world_->rule_version(),b.kind,b.cell))
            occupied+=" "+std::to_string(cell.x)+","+std::to_string(cell.y);
        lines.push_back(std::move(occupied));
        const auto entrances=world_->building_entrances(*id);
        lines.push_back("Active entrances "+std::to_string(entrances.size()));
        for (const auto& entrance:entrances)
            lines.push_back("Road "+std::to_string(entrance.road_cell.x)+","+
                std::to_string(entrance.road_cell.y)+" -> cell "+
                std::to_string(entrance.building_cell.x)+","+
                std::to_string(entrance.building_cell.y));
    }
    if (simulation::city_profile(rules_)) {
        if (b.kind==simulation::Object::Household)
            lines.push_back("Workers supplied "+std::to_string(
                simulation::population_profile(rules_) ? b.population:
                    simulation::Rules::household_workers));
        else {
            const auto required=world_->workforce_required(*id);
            lines.push_back("Workers assigned "+std::to_string(world_->workers_assigned(*id))+
                "/"+std::to_string(required));
            if (world_->operation_controls_supported() &&
                simulation::World::operation_controllable(b.kind)) {
                lines.push_back(std::string("Operation: ")+
                    (b.operating_enabled ? "Running":"Paused by player"));
                lines.push_back(std::string("Worker priority: ")+
                    simulation::workforce_priority_name(b.workforce_priority));
                if (world_->building_on_fire(*id))
                    lines.push_back("On fire - operation suspended.");
                else if (!b.operating_enabled) {
                    const bool moving=std::any_of(world_->couriers().begin(),world_->couriers().end(),
                        [&](const simulation::CourierState& courier) {
                            return courier.owner==b.id &&
                                courier.phase!=simulation::CourierPhase::IdleAtWorkshop;
                        });
                    lines.push_back(moving ? "Operation paused; current delivery is finishing.":
                                           "Paused by player; production and new dispatch stopped.");
                } else if (!world_->building_staffed(*id)) lines.push_back("Status: Unstaffed");
            } else if (!world_->building_staffed(*id)) lines.push_back("Status: Unstaffed");
        }
    }
    if (b.kind==simulation::Object::Workshop) {
        lines.push_back("Output "+std::to_string(world_->workshop_stock())+"/8");
        lines.push_back("Progress "+std::to_string(world_->production_progress())+"/100");
        lines.push_back("Produced "+std::to_string(world_->total_produced()));
        lines.push_back(world_->workshop_stock()==simulation::Rules::workshop_capacity ?
            "Output full" : "Workshop active");
    } else if (b.kind==simulation::Object::ClaySource) {
        lines.push_back("Clay output "+std::to_string(b.output)+"/8");
        lines.push_back("Progress "+std::to_string(b.progress)+"/"+
            std::to_string(world_->active_rules().clay_ticks));
        lines.push_back("Produced total "+std::to_string(b.clay_extracted));
        if (b.operating_enabled && !world_->building_on_fire(*id))
            lines.push_back(b.output==simulation::Rules::clay_output_capacity ? "Output full" :
                "Extraction active");
    } else if (b.kind==simulation::Object::Pottery) {
        lines.push_back("Clay input "+std::to_string(b.input_clay)+"/8");
        lines.push_back("Reserved input "+std::to_string(b.reserved_incoming));
        lines.push_back("Recipe Clay "+std::to_string(b.active_recipe_clay));
        lines.push_back("Recipe "+std::to_string(b.progress)+"/"+
            std::to_string(world_->active_rules().pottery_recipe_ticks));
        lines.push_back("Pottery output "+std::to_string(b.output)+"/8");
        lines.push_back("Produced total "+std::to_string(b.recipes_completed));
        if (b.operating_enabled && !world_->building_on_fire(*id)) lines.push_back(b.active_recipe_clay>0 ? "Processing" :
            b.output>=simulation::Rules::pottery_output_capacity ? "Output full" :
            b.input_clay<simulation::Rules::pottery_recipe_clay ? "Awaiting input" : "Ready");
    } else if (b.kind==simulation::Object::Warehouse) {
        const int stock=simulation::production_profile(rules_) ? b.pottery_stock :
            world_->warehouse_stock();
        lines.push_back("Pottery stock "+std::to_string(stock)+"/32");
        lines.push_back("Reserved input "+std::to_string(b.reserved_incoming));
        lines.push_back("Free "+std::to_string(simulation::Rules::warehouse_capacity-stock-
            b.reserved_incoming));
    } else if (b.kind==simulation::Object::Household) {
        lines.push_back("Pottery stock "+std::to_string(b.pottery_stock)+"/8");
        lines.push_back("Reserved "+std::to_string(b.reserved_incoming));
        lines.push_back("Demand clock "+std::to_string(b.demand_progress)+"/400");
        lines.push_back("Fulfilled "+std::to_string(b.fulfilled_demand)+
            " Missed "+std::to_string(b.missed_demand));
        lines.push_back("Consumed "+std::to_string(b.consumed_total));
        if (simulation::market_profile(rules_)) {
            lines.push_back("House level "+std::to_string(world_->household_level(*id)));
            if (simulation::population_profile(rules_)) lines.push_back("Population "+
                std::to_string(b.population)+"/"+std::to_string(world_->household_population_capacity(*id)));
            lines.push_back("Move-in grace "+std::to_string(world_->household_move_in_grace_remaining(*id))+" ticks");
            lines.push_back("Food reserved "+std::to_string(b.reserved_food_incoming));
            lines.push_back("Food consumed "+std::to_string(b.food_consumed_total));
        }
        if (simulation::food_profile(rules_) && !simulation::market_profile(rules_)) {
            lines.push_back("Tax requires Food, Pottery and Service at the demand deadline.");
            const auto next=b.fulfilled_demand+1;
            const auto next_tax=next>=simulation::Rules::city_v7_level2_demands ? 60:
                next>=simulation::Rules::city_v7_level1_demands ? 40:25;
            lines.push_back("House level "+std::to_string(world_->household_level(*id)));
            if (simulation::population_profile(rules_))
                lines.push_back("Population "+std::to_string(b.population)+"/"+
                    std::to_string(world_->household_population_capacity(*id)));
            lines.push_back("Next demand in "+
                std::to_string(std::max(0,simulation::Rules::household_demand_ticks-
                    b.demand_progress))+" ticks");
            lines.push_back("Move-in grace "+
                std::to_string(world_->household_move_in_grace_remaining(*id))+" ticks");
            lines.push_back("Food stock "+std::to_string(b.food_stock)+"/8");
            lines.push_back("Food reserved "+std::to_string(b.reserved_food_incoming));
            lines.push_back("Food consumed "+std::to_string(b.food_consumed_total));
            lines.push_back("Next fulfilled tax "+std::to_string(next_tax));
            lines.push_back("Taxes earned "+std::to_string(world_->household_tax_contributed(*id)));
            if (simulation::service_profile(rules_)) {
                lines.push_back(std::string("Service ")+
                    (world_->household_service_active(*id) ? "active":"expired"));
                lines.push_back("Coverage remaining "+
                    std::to_string(world_->household_service_remaining(*id))+" ticks");
                std::string missing;
                const auto add_missing=[&](const char* value) {
                    if (!missing.empty()) missing+=" + ";
                    missing+=value;
                };
                if (b.pottery_stock==0) add_missing("Pottery");
                if (b.food_stock==0) add_missing("Food");
                if (!world_->household_service_active(*id)) add_missing("Service");
                lines.push_back(world_->building_on_fire(*id) ? "On fire - demand cannot be fulfilled":
                    missing.empty() ? "Demand ready":"Missing "+missing);
            }
        } else if (rules_==simulation::RulesProfile::CityV6) {
            lines.push_back("Tax per supplied demand 25");
            lines.push_back("Taxes earned "+std::to_string(world_->household_tax_contributed(*id)));
        }
        lines.push_back(b.last_demand_status==0 ? "Last demand: none yet" :
            b.last_demand_status==1 ? "Last demand: supplied; tax paid" :
                                      "Last demand: unmet; no tax");
    } else if (b.kind==simulation::Object::Farm) {
        lines.push_back("Food output "+std::to_string(b.output)+"/12");
        lines.push_back("Progress "+std::to_string(b.progress)+"/"+
            std::to_string(world_->active_rules().farm_ticks));
        lines.push_back("Produced total "+std::to_string(b.food_produced));
    } else if (b.kind==simulation::Object::Market) {
        if (b.operating_enabled && !world_->building_on_fire(*id)) lines.push_back(std::string("Market ")+
            (world_->building_staffed(*id) ? "staffed":"unstaffed"));
        lines.push_back("Pottery stock "+std::to_string(b.pottery_stock)+"/16");
        lines.push_back("Pottery incoming "+std::to_string(b.reserved_incoming));
        lines.push_back("Food stock "+std::to_string(b.food_stock)+"/16");
        lines.push_back("Food incoming "+std::to_string(b.reserved_food_incoming));
    } else if (b.kind==simulation::Object::FireWatch) {
        lines.push_back("Fire Watch - fireproof");
        lines.push_back(std::string("Staffed ")+(world_->building_staffed(*id) ? "yes":"no"));
        lines.push_back("Protected "+std::to_string(world_->protected_buildings())+"/"+
            std::to_string(world_->fire_eligible_buildings()));
        lines.push_back("Burning "+std::to_string(world_->burning_buildings()));
    } else if (b.kind==simulation::Object::ServicePost) {
        const auto found=std::find_if(world_->couriers().begin(),world_->couriers().end(),
            [&](const simulation::CourierState& c) { return c.owner==b.id; });
        if (found==world_->couriers().end()) return lines;
        const auto& c=*found;
        lines.push_back("Covered houses "+std::to_string(world_->covered_households())+"/"+
            std::to_string(std::count_if(world_->buildings().begin(),world_->buildings().end(),
                [](const simulation::BuildingState& value) {
                    return value.placed && value.kind==simulation::Object::Household;
                })));
        lines.push_back(std::string("Walker phase ")+simulation::delivery_phase_name(c.phase));
        lines.push_back("Current target "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
            std::string("-"):std::to_string(static_cast<unsigned>(c.target))));
        const auto decision=world_->courier_dispatch_status(c.id);
        lines.push_back(std::string("Road status ")+
            simulation::courier_dispatch_status_name(decision.status));
        if (world_->last_dispatched_service_household())
            lines.push_back("Last target "+std::to_string(static_cast<unsigned>(
                *world_->last_dispatched_service_household())));
    }
    if (const auto role=visual_role(b.cell,b.kind)) {
        lines.push_back(std::string("Building visuals ")+(building_enabled_ ? "ON":"OFF"));
        lines.push_back(std::string("Visual role ")+assets::building_role_name(*role));
        const auto* entry=building_profile_ ? building_entry(*role):nullptr;
        lines.push_back(std::string("Visual configured ")+(entry ? "yes":"no"));
        if (entry) {
            const auto& image=building_profile_->unique_images.at(entry->image_index);
            lines.push_back("SG3 "+entry->id.archive_relative_path.generic_string()+
                " #"+std::to_string(entry->id.image_index));
            lines.push_back("Image "+std::to_string(image.width)+"x"+
                std::to_string(image.height));
            lines.push_back("Anchor "+std::to_string(entry->ground_x)+","+
                std::to_string(entry->ground_y));
            lines.push_back("Evidence: curated preview");
        }
    }
    if (simulation::production_profile(rules_)) {
        for (const auto& c:world_->couriers()) {
            if (!c.enabled || c.owner!=*id) continue;
            const char* role_name=c.role==simulation::CourierRole::Clay ? "Clay":
                c.role==simulation::CourierRole::Pottery ? "Pottery":
                c.role==simulation::CourierRole::Food ? "Food":
                c.role==simulation::CourierRole::Service ? "Service":
                c.role==simulation::CourierRole::FireInspector ? "Fire Inspector":
                c.role==simulation::CourierRole::MarketPotteryInbound ? "Pottery Market Supplier":
                c.role==simulation::CourierRole::MarketFoodInbound ? "Market Food Supplier":
                c.role==simulation::CourierRole::MarketPotteryDistribution ? "Pottery distributor":
                c.role==simulation::CourierRole::MarketFoodDistribution ? "Food distributor":
                "House supply";
            lines.push_back("Courier #"+std::to_string(static_cast<std::uint32_t>(c.id))+" "+
                role_name);
            lines.push_back("Target "+(c.phase==simulation::CourierPhase::IdleAtWorkshop ?
                std::string("-"):std::to_string(static_cast<unsigned>(c.target)))+
                " cargo "+std::to_string(c.cargo));
            lines.push_back(std::string("Phase ")+simulation::delivery_phase_name(c.phase));
            if (food_market_visual(walker_visual_role(c.role))) {
                lines.push_back(std::string("Role ")+courier_role_label(c.role)+" | Owner "+
                    std::to_string(static_cast<std::uint32_t>(c.owner)));
                lines.push_back(std::string("Goods ")+courier_good_label(c.good));
                lines.push_back(walker_clip_status(c));
            }
            const auto decision=world_->courier_dispatch_status(c.id);
            lines.push_back(std::string("Status ")+
                simulation::courier_dispatch_status_name(decision.status));
            if (decision.selected_target)
                lines.push_back("Next target "+
                    std::to_string(static_cast<unsigned>(*decision.selected_target)));
            if (b.kind!=simulation::Object::Market) break;
        }
    } else if (b.kind==simulation::Object::Warehouse || b.kind==simulation::Object::Workshop) {
        lines.push_back(std::string("Courier ")+simulation::courier_phase_name(world_->courier_phase()));
        lines.push_back("Cargo "+std::to_string(world_->courier_cargo()));
    }
    return simulation::maintenance_profile(rules_) ? wrap_panel_lines(lines):lines;
}
std::vector<std::string> SandboxView::income_summary_lines() const {
    const auto status=simulation::inspect_city_start(*world_);
    if (!status.applicable) return {};
    std::string funds="Funds "+std::to_string(status.funds_current)+" | ";
    funds+=status.taxes_received_total ? "Taxes received: "+std::to_string(status.taxes_received_total):
        "Taxes: none yet";
    if (status.maintenance.applicable) funds+=" | Upkeep "+
        std::to_string(status.maintenance.installed_rate)+"/"+
        std::to_string(simulation::Rules::maintenance_interval_ticks)+"t";
    std::string houses="Current Houses: ";
    if (status.households.empty()) houses+="none; no household tax";
    else {
        std::vector<std::string> blockers;
        if (status.households_burning) blockers.push_back("burning "+std::to_string(status.households_burning));
        if (status.households_sick) blockers.push_back("sick "+std::to_string(status.households_sick));
        if (status.households_missing_service) blockers.push_back("Service missing "+std::to_string(status.households_missing_service));
        if (status.households_missing_food) blockers.push_back("Food missing "+std::to_string(status.households_missing_food));
        if (status.households_missing_pottery) blockers.push_back("Pottery missing "+std::to_string(status.households_missing_pottery));
        for (const auto& blocker:blockers) { if (houses!="Current Houses: ") houses+=", "; houses+=blocker; }
        if (blockers.empty()) houses+="supplied now; payment depends on the demand deadline";
    }
    std::string next;
    const auto unstaffed=std::find_if(status.facility_instances.begin(),status.facility_instances.end(),
        [](const auto& facility) { return facility.kind==simulation::Object::Farm &&
            facility.condition==simulation::StarterSupplyCondition::Unstaffed; });
    const auto bottleneck=unstaffed!=status.facility_instances.end() ? unstaffed:
        std::find_if(status.facility_instances.begin(),status.facility_instances.end(),[](const auto& facility) {
            return facility.condition==simulation::StarterSupplyCondition::Unstaffed;
        });
    if (bottleneck!=status.facility_instances.end()) next=
        std::string(simulation::starter_building_name(bottleneck->kind))+" #"+
        std::to_string(static_cast<std::uint32_t>(bottleneck->id))+" unstaffed "+
        std::to_string(bottleneck->workers_assigned)+"/"+std::to_string(bottleneck->workers_required);
    if (!status.missing_supply_buildings.empty()) {
        if (!next.empty()) next+=" | ";
        next+=status.missing_supply_buildings.size()==1 ?
            std::string(simulation::starter_building_name(status.missing_supply_buildings.front())):
            "Missing supply buildings";
        next+=" cost "+std::to_string(status.minimum_missing_building_funds);
        if (!status.missing_building_funds_gap) next+="; gap exceeds display range";
        else if (*status.missing_building_funds_gap>0) next+="; short "+std::to_string(*status.missing_building_funds_gap);
        else next+="; affordable before roads";
    }
    if (next.empty()) next="Income details: T | Current stocks and coverage can change";
    return {std::move(funds),std::move(houses),std::move(next)};
}

std::vector<std::string> SandboxView::income_lines() const {
    const auto status=simulation::inspect_city_start(*world_);
    if (!status.applicable) return {};
    std::vector<std::string> lines{
        "Funds: "+std::to_string(status.funds_current),
        status.taxes_received_total ? "Taxes received total: "+std::to_string(status.taxes_received_total):"Taxes: none received yet",
        "Houses pay for actual supplied demand; Market creates no Food or Service."};
    if (status.maintenance.applicable) lines.push_back("Installed upkeep: "+
        std::to_string(status.maintenance.installed_rate)+" / "+
        std::to_string(simulation::Rules::maintenance_interval_ticks)+" ticks");
    if (status.households_missing_service) lines.push_back("Service missing in "+std::to_string(status.households_missing_service)+" Houses now.");
    if (status.households_burning) lines.push_back(std::to_string(status.households_burning)+" Houses burning: demand blocked.");
    if (status.households_sick) lines.push_back(std::to_string(status.households_sick)+" Houses sick: demand blocked.");
    if (status.missing_supply_buildings.empty()) lines.push_back("Basic buildings installed. This does not prove House supply.");
    else {
        lines.push_back("Missing for ongoing supply:");
        for (const auto& missing:status.missing_supply_costs)
            lines.push_back(std::string(simulation::starter_building_name(missing.kind))+": "+std::to_string(missing.cost)+" Funds");
        if (!status.missing_building_funds_gap) lines.push_back("Building funds gap exceeds supported range.");
        else if (*status.missing_building_funds_gap>0)
            lines.push_back("Short "+std::to_string(*status.missing_building_funds_gap)+" for these buildings alone.");
        else lines.push_back("These buildings are affordable before roads.");
        lines.push_back("Existing stocks / Service can remain after a supply building is removed.");
    }
    lines.push_back("Workers available: "+std::to_string(status.workforce_supply)+
        "; assigned: "+std::to_string(status.workforce_used));
    lines.push_back("Active demand: "+std::to_string(status.workforce_active_demand)+
        "; installed demand: "+std::to_string(status.workforce_installed_demand));
    lines.push_back("Active excludes paused / burning operations; installed includes them.");
    bool staffing_issue=false;
    for (const auto& facility:status.facility_instances) {
        if (facility.condition!=simulation::StarterSupplyCondition::Unstaffed) continue;
        staffing_issue=true;
        lines.push_back(std::string(simulation::starter_building_name(facility.kind))+" #"+
            std::to_string(static_cast<std::uint32_t>(facility.id))+" unstaffed: "+
            std::to_string(facility.workers_assigned)+"/"+std::to_string(facility.workers_required));
    }
    lines.push_back("NEXT STEPS");
    if (staffing_issue) lines.push_back("Review staffing of the named business; changing priority can take workers from another.");
    if (!status.missing_supply_buildings.empty()) {
        if (status.missing_building_funds_gap && *status.missing_building_funds_gap==0)
            lines.push_back("Build missing supply and connect it by road.");
        else lines.push_back("Building funds are short. Review existing supply and staffing before spending more.");
    } else if (!status.household_count)
        lines.push_back("No House is installed. Check the House construction estimate.");
    else if (status.households_ready_now<status.household_count)
        lines.push_back("Check House stocks, Service and business delivery status below.");
    else lines.push_back("Houses are supplied now. Wait for actual demand; conditions may change.");
    lines.push_back("CONSTRUCTION PLAN (estimate)");
    lines.push_back("Missing buildings: "+std::to_string(status.minimum_missing_building_funds));
    lines.push_back("Full-staff starter demand: "+std::to_string(status.workforce_required_for_starter));
    lines.push_back("Extra Houses estimate: "+std::to_string(status.houses_needed_for_shortfall)+
        " / "+std::to_string(status.minimum_house_funds_for_starter)+" Funds");
    const auto construction_reserve=status.minimum_missing_building_funds+
        status.minimum_house_funds_for_starter;
    lines.push_back("Construction reserve: "+std::to_string(construction_reserve)+" (estimate)");
    if (construction_reserve>0) {
        if (!status.construction_funds_gap) lines.push_back("Plan funds gap exceeds supported range.");
        else if (*status.construction_funds_gap>0) lines.push_back("Plan funds gap: "+
            std::to_string(*status.construction_funds_gap)+" (estimate)");
    }
    lines.push_back("Assumes simultaneous full staffing and "+
        std::to_string(simulation::Rules::household_initial_population)+" initial residents per new House.");
    if (!status.starter_workforce_within_house_limit) lines.push_back("Estimated House count exceeds the remaining House limit.");
    lines.push_back("Other staffing / population sequences may work. This is not a minimum solution.");
    lines.push_back("Future roads excluded; land and reachability are not proven.");
    lines.push_back("Construction reserve does not guarantee funds until first tax.");
    if (status.maintenance.applicable) {
        const auto& upkeep=status.maintenance;
        lines.push_back("UPKEEP (separate assumption)");
        lines.push_back("Current buildings; no new income or purchases.");
        if (upkeep.next_bill_tick) lines.push_back("Next future bill: "+std::to_string(upkeep.next_bill_cost)+
            " in "+std::to_string(*upkeep.next_bill_tick-world_->ticks())+" ticks");
        else lines.push_back(upkeep.installed_rate ? "Next bill exceeds supported tick range.":"No future bill for current buildings.");
        if (upkeep.interval_cost) lines.push_back("Next "+std::to_string(upkeep.horizon_ticks)+
            " ticks upkeep: "+std::to_string(*upkeep.interval_cost));
        else lines.push_back("Upkeep horizon exceeds supported range.");
        if (upkeep.funds_after_interval) lines.push_back("Funds after that upkeep: "+std::to_string(*upkeep.funds_after_interval));
        else lines.push_back("Funds after upkeep exceed supported range.");
        lines.push_back("Paused / unstaffed buildings still pay. Demolition gives no refund.");
    }
    lines.push_back("CURRENT BUSINESSES");
    for (const auto& facility:status.facility_instances) {
        lines.push_back(std::string(simulation::starter_building_name(facility.kind))+" #"+
            std::to_string(static_cast<std::uint32_t>(facility.id))+": "+
            std::to_string(facility.workers_assigned)+"/"+std::to_string(facility.workers_required)+" workers");
        using C=simulation::StarterSupplyCondition;
        const auto state=facility.condition==C::Paused ? "Paused by player":
            facility.condition==C::OnFire ? "Burning; new work suspended":
            facility.condition==C::Unstaffed ? "Unstaffed; new work suspended":
            facility.condition==C::NoReachableTarget ? "No reachable delivery target":
            facility.condition==C::AwaitingGoods ? "Awaiting goods / production":"Staffed; work enabled";
        lines.push_back(state);
        if (facility.delivery_active) lines.push_back("Existing delivery trip in progress.");
    }
    lines.push_back("CURRENT HOUSE CONDITIONS");
    lines.push_back("Next demand is not a promised tax payment. Conditions are tested then.");
    for (const auto& house:status.households) {
        lines.push_back("House #"+std::to_string(static_cast<std::uint32_t>(house.id)));
        lines.push_back(std::string("Food: ")+(house.food_available ? "available":"missing")+
            "; Pottery: "+(house.pottery_available ? "available":"missing"));
        lines.push_back(house.service_available ? "Service active: "+std::to_string(house.service_ticks_remaining)+" ticks left":"Service: missing");
        if (house.burning) lines.push_back("Fire blocks demand now.");
        if (house.sick) lines.push_back("Sickness blocks demand now.");
        if (house.food_inbound || house.pottery_inbound) lines.push_back("Incoming: Food "+
            std::to_string(house.food_inbound)+", Pottery "+std::to_string(house.pottery_inbound));
        lines.push_back("Next demand: "+std::to_string(house.demand_ticks_remaining)+" ticks");
        lines.push_back(house.last_demand_status==0 ? "Last demand: none yet":
            house.last_demand_status==1 ? "Last demand: supplied; tax paid":"Last demand: unmet; no tax");
        lines.push_back("Taxes contributed: "+std::to_string(house.taxes_contributed));
    }
    return wrap_panel_lines(lines);
}

std::vector<std::string> SandboxView::budget_warning_lines() const {
    if (!budget_warning_) return {};
    const auto& warning=*budget_warning_;
    std::vector<std::string> lines{
        "Purchase: "+std::to_string(warning.purchase_cost)+" Funds; afterwards: "+
            std::to_string(warning.funds_after_purchase),
        warning.remaining_missing_supply_buildings.empty() ? "Basic supply buildings remain installed.":
            "Still missing: "+joined_buildings(warning.remaining_missing_supply_buildings),
        "Missing buildings cost: "+std::to_string(warning.minimum_remaining_building_funds)};
    if (warning.funds_after_purchase<warning.minimum_remaining_building_funds)
        lines.push_back("Short "+std::to_string(warning.minimum_remaining_building_funds-warning.funds_after_purchase)+
            " for those buildings alone, excluding roads.");
    lines.push_back("Full-staff estimate: +"+std::to_string(warning.additional_houses_needed)+
        " Houses / "+std::to_string(warning.minimum_remaining_house_funds)+" Funds");
    if (!warning.starter_workforce_within_house_limit) lines.push_back("Estimated House count exceeds remaining slots.");
    lines.push_back("Construction reserve: "+std::to_string(warning.minimum_remaining_start_cost)+
        "; future roads excluded.");
    lines.push_back("Estimate assumes all starter jobs staffed together.");
    if (warning.maintenance.applicable) {
        lines.push_back("Separate upkeep: "+std::to_string(warning.maintenance.installed_rate)+" / "+
            std::to_string(simulation::Rules::maintenance_interval_ticks)+" ticks");
        lines.push_back("Paused / unstaffed buildings still pay upkeep.");
    }
    lines.push_back("This reserve does not guarantee funds until first tax.");
    const int width=std::min(540*layout_.scale,std::max(0,layout_.map.w-24*layout_.scale));
    return sandbox_ui::wrap_text(lines,static_cast<std::size_t>(std::max(1,(width-28*layout_.scale)/(10*layout_.scale))));
}
bool SandboxView::draw_text(double x,double y,const std::string& value,int max_width) {
    if (max_width<=0) return true;
    const float scale=1.25F*static_cast<float>(layout_.scale);
    const int max_chars=static_cast<int>(static_cast<float>(max_width)/(8.0F*scale));
    if (max_chars<=0) return true;
    std::string text=value;
    if (static_cast<int>(text.size())>max_chars) {
        text.resize(static_cast<std::size_t>(max_chars));
        if (max_chars>=3) text.replace(text.size()-3,3,"...");
    }
    if (!SDL_SetRenderScale(renderer_,scale,scale)) return false;
    const bool okay=SDL_RenderDebugText(renderer_,static_cast<float>(x/scale),
                                        static_cast<float>(y/scale),text.c_str());
    return SDL_SetRenderScale(renderer_,1,1) && okay;
}
bool SandboxView::draw_hud() {
    using A=sandbox_ui::Action;
    const auto fill=[&](sandbox_ui::Rect rect,SDL_Color color)->bool {
        if (rect.w<=0 || rect.h<=0) return true;
        const SDL_FRect area{static_cast<float>(rect.x),static_cast<float>(rect.y),
            static_cast<float>(rect.w),static_cast<float>(rect.h)};
        return SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,color.a) &&
            SDL_RenderFillRect(renderer_,&area);
    };
    if (!fill(layout_.top,{14,22,34,255}) || !fill(layout_.toolbar,{16,24,36,255}) ||
        !fill(layout_.status,{13,21,31,255}) ||
        !fill(layout_.panel,{19,29,43,250})) return false;
    if (!SDL_SetRenderDrawColor(renderer_,230,236,244,255)) return false;
    const std::string profile=debug_open_ ? simulation::rules_profile_name(rules_):profile_title(rules_);
    std::string overview=profile;
    if (simulation::market_profile(rules_))
        overview+=" v"+std::to_string(world_->rule_version());
    if (world_->map_permissions())
        overview+=" | Map policy "+std::to_string(world_->map_permissions()->policy_version());
    overview+=" | Tick "+std::to_string(world_->ticks())+" | "+
        (clock_.paused()?"Paused ":"Running ")+std::to_string(clock_.speed())+"x";
    if (simulation::water_profile(rules_)) {
        const auto houses=std::count_if(world_->buildings().begin(),world_->buildings().end(),
            [](const auto& b) { return b.kind==simulation::Object::Household; });
        overview+=" | Funds "+std::to_string(world_->treasury());
        if (simulation::maintenance_profile(rules_))
            overview+=" | Maint "+std::to_string(world_->current_maintenance_rate())+"/"+
                std::to_string(simulation::Rules::maintenance_interval_ticks)+"t";
        overview+=" | Water "+
            std::to_string(world_->water_covered_households())+"/"+std::to_string(houses)+
            " | Pop "+std::to_string(world_->total_population());
    }
    if (simulation::desirability_profile(rules_)) {
        int good=0,houses=0;
        for (const auto& b:world_->buildings()) if (b.kind==simulation::Object::Household) {
            ++houses; if (world_->household_desirability(b.id)>=10) ++good;
        }
        overview+=" | Desirable homes "+std::to_string(good)+"/"+std::to_string(houses);
    }
    if (simulation::fire_profile(rules_))
        overview+=" | Fire "+std::to_string(world_->protected_buildings())+"/"+
            std::to_string(world_->fire_eligible_buildings())+" protected, "+
            std::to_string(world_->burning_buildings())+" BURNING";
    if (simulation::city_profile(rules_) && !simulation::water_profile(rules_)) {
        overview+=" | Funds "+std::to_string(world_->treasury())+" | Workers ";
        if (simulation::population_profile(rules_))
            overview+="avail "+std::to_string(world_->workforce_supply())+
                " assigned "+std::to_string(world_->workforce_used())+
                " active "+std::to_string(world_->active_workforce_required())+
                " installed "+std::to_string(world_->workforce_required());
        else
            overview+=std::to_string(world_->workforce_used())+"/"+
                std::to_string(world_->workforce_supply());
    }
    if (simulation::population_profile(rules_) && !simulation::water_profile(rules_))
        overview+=" | Pop "+std::to_string(world_->total_population());
    if (simulation::food_profile(rules_)) {
        int food=0; std::size_t houses=0;
        for (const auto& b:world_->buildings()) {
            if (b.kind==simulation::Object::Farm) food+=b.output;
            if (b.kind==simulation::Object::Market || b.kind==simulation::Object::Household)
                food+=b.food_stock;
            if (b.kind==simulation::Object::Household) ++houses;
        }
        for (const auto& c:world_->couriers()) if (c.good==simulation::Good::Food) food+=c.cargo;
        overview+=" | Food stock "+std::to_string(food);
        if (simulation::service_profile(rules_))
            overview+=" | Service "+std::to_string(world_->covered_households())+"/"+
                std::to_string(houses);
        overview+=" | Goal "+std::to_string(world_->settlement_goal_households_ready())+"/"+
            std::to_string((simulation::scalable_profile(rules_)) ?
                           simulation::Rules::city_v10_goal_level2_households:4);
        if (simulation::population_profile(rules_))
            overview+=" Pop "+std::to_string(world_->total_population())+"/"+
                std::to_string((simulation::scalable_profile(rules_)) ?
                    simulation::Rules::city_v10_population_goal:
                    simulation::Rules::city_v9_population_goal);
    }
    if (simulation::production_profile(rules_)) {
        int stored=0; for (const auto& b:world_->buildings())
            if (b.kind==simulation::Object::Warehouse) stored+=b.pottery_stock;
        overview+=" | Clay produced total "+std::to_string(world_->clay_extracted_total())+
            " | Pottery produced total "+std::to_string(world_->pottery_completed_total())+
            " | Warehouse stock "+std::to_string(stored);
    }
    else overview+=" | Goods "+std::to_string(world_->total_produced());
    if (world_->burning_buildings()>0 && !SDL_SetRenderDrawColor(renderer_,255,174,80,255)) return false;
    if (simulation::market_profile(rules_)) {
        overview=profile+" v"+std::to_string(world_->rule_version())+" | Tick "+
            std::to_string(world_->ticks())+" | "+(clock_.paused() ? "Paused ":"Running ")+
            std::to_string(clock_.speed())+"x";
        if (!draw_text(8*layout_.scale,10*layout_.scale,overview,layout_.top.w-310*layout_.scale)) return false;
        const auto income=income_summary_lines();
        for (std::size_t i=0;i<income.size();++i)
            if (!draw_text(8*layout_.scale,(28+static_cast<int>(i)*18)*layout_.scale,income[i],
                layout_.top.w-(managed_ && i>0 ? 112:16)*layout_.scale)) return false;
    } else if (!draw_text(8*layout_.scale,18*layout_.scale,overview,
        layout_.top.w-200*layout_.scale)) return false;
    if (!SDL_SetRenderDrawColor(renderer_,230,236,244,255)) return false;
    const auto menu=menu_button_rect();
    if (managed_ && !draw_text(menu.x+6*layout_.scale,menu.y+10*layout_.scale,
                               "[ MENU ]",menu.w-8*layout_.scale)) return false;
    std::string status=road_start_ && !road_preview_.valid ? sandbox_ui::road_plan_status(road_preview_) :
        last_message_.empty() ? "OpenEmperor sandbox | Select a tool, then click the map" :
        last_message_;
    if (simulation::city_profile(rules_) && road_start_ &&
        !road_preview_.valid && road_preview_.reason=="Not enough money") {
        const auto count=world_->map_permissions() ? road_preview_.new_road_count:
            static_cast<std::size_t>(std::count_if(road_preview_.cells.begin(),road_preview_.cells.end(),
                [&](simulation::Cell cell) { return world_->object_at(cell)!=simulation::Object::Road; }));
        status="Need "+std::to_string(count*simulation::Rules::road_cost)+
            " funds; treasury "+std::to_string(world_->treasury());
    }
    if (simulation::city_profile(rules_) && !road_start_ && hovered_ &&
        tool_!=(simulation::production_profile(rules_) ? 5:4)) {
        const auto result=preview(*hovered_);
        if (!result.accepted && std::string_view(result.reason)=="Not enough money") {
            const auto cost=world_->construction_cost(command_type(rules_,tool_));
            status="Need "+std::to_string(cost)+" funds; treasury "+
                std::to_string(world_->treasury());
        }
    }
    if (simulation::maintenance_profile(rules_) && hovered_ && !road_start_ && tool_!=5 && tool_!=6) {
        const auto type=command_type(rules_,tool_);
        const auto cost=world_->construction_cost(type);
        const auto kind=simulation::placed_object(type);
        if (cost>0 && kind)
            status="$"+std::to_string(cost)+" | upkeep $"+std::to_string(simulation::maintenance_cost(rules_,*kind))+"/400t | "+status;
    }
    if (tool_==13 && simulation::health_profile(rules_)) status="Road connection required. | "+status;
    if (simulation::water_profile(rules_)) {
        if (world_->taxes_collected_total()>0 && world_->water_covered_households()==0 &&
            std::all_of(world_->buildings().begin(),world_->buildings().end(),[&](const auto& b) {
                return b.kind!=simulation::Object::Household || (b.pottery_stock>0 && b.food_stock>0 &&
                    world_->household_service_active(b.id) && !world_->building_on_fire(b.id) && !world_->household_sick(b.id));
            }))
            status="Your city is supplied. Add water to improve housing. | "+status;
        if (predicted_water_) status=std::string("Water: ")+(*predicted_water_ ? "Yes":"No")+" | "+status;
        if (predicted_well_coverage_) status="Would supply water to "+
            std::to_string(predicted_well_coverage_->households)+" Houses ("+
            std::to_string(predicted_well_coverage_->currently_dry)+" currently without water) | "+status;
    }
    if (predicted_desirability_) status="Predicted desirability at this location: "+
        std::to_string(*predicted_desirability_)+" | "+status;
    if (!recovery_status_.empty() && !budget_warning_ && !pending_demolition_ && !road_start_)
        status+=" | "+recovery_status_;
    if (simulation::city_profile(rules_))
        status+=world_->settlement_goal_reached() ?
            (simulation::population_profile(rules_) ?
                " | GOAL REACHED - houses developed and population stable":
             simulation::food_profile(rules_) ?
                " | GOAL REACHED - all houses level 2":" | GOAL REACHED - settlement supplied") :
            " | Goal "+std::to_string(world_->settlement_goal_households_ready())+
                (simulation::food_profile(rules_) ?
                    ((simulation::scalable_profile(rules_)) ? "/8 houses at level 2":
                     "/4 houses at level 2"):"/4 households ready");
    if (simulation::population_profile(rules_) && !world_->settlement_goal_reached())
        status+=" | Population "+std::to_string(world_->total_population())+"/"+
            std::to_string((simulation::scalable_profile(rules_)) ?
                simulation::Rules::city_v10_population_goal:simulation::Rules::city_v9_population_goal);
    if (debug_open_ && walker_profile_) {
        status+=" | Walkers ";
        status+=walker_visuals_enabled_ ? "ON ":"OFF ";
        for (std::size_t r=0;r<assets::walker_visual_role_count;++r) {
            if (r) status+=' ';
            status+=assets::walker_role_name(static_cast<assets::WalkerVisualRole>(r));
            status+=walker_profile_->roles[r] ? ":yes":":no";
        }
    }
    const auto& generated=background_.plan().regenerated;
    const bool preview_notice=generated && !generated->great_wall_preview_notice.empty();
    if (preview_notice && !draw_text(8*layout_.scale,layout_.status.y+2*layout_.scale,
        generated->great_wall_preview_notice,layout_.status.w-16*layout_.scale)) return false;
    if (!draw_text(8*layout_.scale,layout_.status.y+(preview_notice ? 13:7)*layout_.scale,status,
                   layout_.status.w-16*layout_.scale)) return false;
    const bool scalable=simulation::scalable_profile(rules_);
    const auto label=[&](A action)->std::string {
        const auto count=[&](simulation::Object kind) {
            return std::count_if(world_->buildings().begin(),world_->buildings().end(),
                [&](const simulation::BuildingState& b) { return b.placed && b.kind==kind; });
        };
        switch (action) {
        case A::Select: return simulation::production_profile(rules_) ? "5 Select":"4 Select";
        case A::Road: return simulation::city_profile(rules_) ? "1 Road $2":"1 Road";
        case A::Clay: return scalable ? "2 Clay "+std::to_string(count(simulation::Object::ClaySource))+
            "/4 $120":simulation::city_profile(rules_) ? "2 Clay $120":
            simulation::production_profile(rules_) ? "2 Clay" : "2 Workshop";
        case A::Pottery: return scalable ? "3 Pottery "+std::to_string(count(simulation::Object::Pottery))+
            "/4 $180":simulation::city_profile(rules_) ? "3 Pottery $180":"3 Pottery";
        case A::Warehouse: return scalable ? "4 Store "+std::to_string(count(simulation::Object::Warehouse))+
            "/2 $150":simulation::city_profile(rules_) ? "4 Store $150":
            simulation::production_profile(rules_) ? "4 Store" : "3 Store";
        case A::RemoveRoad: return "6 Remove";
        case A::Household: return scalable ? "7 House "+std::to_string(count(simulation::Object::Household))+
            "/20 $80":simulation::city_profile(rules_) ? "7 House $80":"7 House";
        case A::Farm: return scalable ? "8 Farm "+std::to_string(count(simulation::Object::Farm))+
            "/2 $160":"8 Farm $160";
        case A::ServicePost: return scalable ? "9 Service "+
            std::to_string(count(simulation::Object::ServicePost))+"/2 $100":"9 Service $100";
        case A::Market: return "0 Market $140";
        case A::HealthPost: return "J Health Post $120";
        case A::Health: return health_overlay_ ? "K Health ON":"K Health";
        case A::Well: return "I Well $60";
        case A::Water: return water_overlay_ ? "U Water ON":"U Water";
        case A::Desirability: return desirability_overlay_ ? "D Desirability ON":"D Desirability";
        case A::FireWatch: return "F Fire Watch $"+
            std::to_string(simulation::Rules::fire_watch_cost);
        case A::Pause: return clock_.paused()?"Continue":"Pause";
        case A::Step: return "Step";
        case A::Speed1: return "1x";
        case A::Speed2: return "2x";
        case A::Speed4: return "4x";
        case A::Reset: return "Reset";
        case A::Save: return "Save F5";
        case A::Load: return "Load F9";
        case A::TogglePanel: return layout_.panel_open ? "Hide info":"Show info";
        case A::ToggleHelp: return help_open_ ? "Hide help":"Help";
        case A::ToggleIncome: return income_open_ ? "Inspect T":"Income T";
        }
        return "";
    };
    for (const auto& button:layout_.buttons) {
        bool active=false;
        switch (button.action) {
        case A::Select: active=tool_==(simulation::production_profile(rules_) ? 5:4); break;
        case A::Road: active=tool_==1; break;
        case A::Clay: active=tool_==2; break;
        case A::Pottery: active=simulation::production_profile(rules_) && tool_==3; break;
        case A::Warehouse: active=tool_==(simulation::production_profile(rules_) ? 4:3); break;
        case A::RemoveRoad: active=tool_==6; break;
        case A::Household: active=tool_==7; break;
        case A::Farm: active=tool_==8; break;
        case A::ServicePost: active=tool_==9; break;
        case A::Market: active=tool_==0; break;
        case A::FireWatch: active=tool_==11; break;
        case A::HealthPost: active=tool_==13; break;
        case A::Health: active=health_overlay_; break;
        case A::Well: active=tool_==12; break;
        case A::Water: active=water_overlay_; break;
        case A::ToggleIncome: active=income_open_; break;
        case A::Desirability: active=desirability_overlay_; break;
        default: break;
        }
        const bool enabled=action_enabled(button.action);
        if (!fill(button.rect,!enabled ? SDL_Color{43,47,55,255}:
            active ? SDL_Color{38,100,125,255}:SDL_Color{38,58,77,255})) return false;
        if (!SDL_SetRenderDrawColor(renderer_,enabled ? 245:135,enabled ? 247:140,
                                    enabled ? 250:145,255)) return false;
        std::string text=label(button.action);
        if (button.action==A::Clay || button.action==A::Pottery ||
            button.action==A::Warehouse || button.action==A::Household ||
            button.action==A::Farm || button.action==A::ServicePost ||
            button.action==A::Market || button.action==A::FireWatch || button.action==A::Well || button.action==A::HealthPost) {
            const auto wanted=button.action==A::Clay ?
                (simulation::production_profile(rules_) ? simulation::Object::ClaySource:
                 simulation::Object::Workshop) :
                button.action==A::Pottery ? simulation::Object::Pottery :
                button.action==A::Warehouse ? simulation::Object::Warehouse:
                button.action==A::Household ? simulation::Object::Household:
                button.action==A::Farm ? simulation::Object::Farm:
                button.action==A::ServicePost ? simulation::Object::ServicePost:
                button.action==A::HealthPost ? simulation::Object::HealthPost:
                button.action==A::Well ? simulation::Object::Well:
                button.action==A::FireWatch ? simulation::Object::FireWatch:
                simulation::Object::Market;
            int count=0;
            for (const auto id:placed_buildings()) if (world_->building(id).kind==wanted) ++count;
            const int limit=button.action==A::Well ? 4:(button.action==A::FireWatch || button.action==A::HealthPost) ? 2:button.action==A::Market ? 4:
                scalable ? (button.action==A::Household ? 20:
                    (button.action==A::Clay || button.action==A::Pottery) ? 4:
                    (button.action==A::Warehouse || button.action==A::Farm ||
                     button.action==A::ServicePost) ? 2:1):
                button.action==A::Household ?
                (simulation::industry_profile(rules_) ||
                 rules_==simulation::RulesProfile::SettlementV4 ? 4:1) :
                simulation::industry_profile(rules_) &&
                (button.action==A::Clay || button.action==A::Pottery) ? 2:1;
            if (button.action!=A::HealthPost) text+=" "+std::to_string(count)+"/"+std::to_string(limit);
        }
        if (!draw_text(button.rect.x+5*layout_.scale,button.rect.y+10*layout_.scale,
                       text,button.rect.w-8*layout_.scale)) return false;
    }
    if (layout_.panel_open && income_open_) {
        if (!SDL_SetRenderDrawColor(renderer_,240,244,250,255)) return false;
        if (!draw_text(layout_.panel.x+10*layout_.scale,layout_.panel.y+14*layout_.scale,
            "INCOME - T / SELECT RETURNS",layout_.panel.w-20*layout_.scale)) return false;
        const auto details=income_lines();
        const SDL_Rect clip{layout_.panel.x,layout_.panel.y+40*layout_.scale,
            layout_.panel.w,std::max(0,layout_.panel.h-40*layout_.scale)};
        // Draw only complete wrapped rows. draw_text temporarily scales SDL's
        // coordinates, which would also scale an absolute-pixel clip here.
        for (std::size_t i=0;i<details.size();++i) {
            const int y=layout_.panel.y+(46+static_cast<int>(i)*17)*layout_.scale-income_scroll_;
            if (y<clip.y || y+10*layout_.scale>=clip.y+clip.h) continue;
            if (!SDL_SetRenderDrawColor(renderer_,205,225,238,255) ||
                !draw_text(layout_.panel.x+10*layout_.scale,y,details[i],
                    layout_.panel.w-20*layout_.scale)) return false;
        }
    }
    if (layout_.panel_open && !income_open_) {
        if (!SDL_SetRenderDrawColor(renderer_,240,244,250,255)) return false;
        if (!draw_text(layout_.panel.x+10*layout_.scale,layout_.panel.y+14*layout_.scale,
                       selected_building() && world_->building(*selected_building()).kind==simulation::Object::Well ? "WELL INSPECTOR":
                       fire_watch_selected() ? "FIRE WATCH INSPECTOR":
                       status_first_selected() ? "HOUSE INSPECTOR":"BUILDINGS",
                       layout_.panel.w-20*layout_.scale)) return false;
        const auto entries=placed_buildings();
        for (std::size_t i=0;i<entries.size();++i) {
            const auto& b=world_->building(entries[i]);
            const int y=building_list_y()+static_cast<int>(i)*18*layout_.scale-panel_scroll_;
            const int list_bottom=layout_.panel.y+layout_.panel.h-
                (status_first_selected() ? 180*layout_.scale+demolition_hint_extra_height():0);
            if (y<layout_.panel.y+40*layout_.scale || y+10*layout_.scale>=list_bottom) continue;
            const std::string text=std::to_string(static_cast<unsigned>(b.id))+" "+
                object_name(b.kind)+" ("+std::to_string(b.cell.x)+","+
                std::to_string(b.cell.y)+")";
            if (selected_building()==entries[i]) {
                if (!fill({layout_.panel.x+4*layout_.scale,y-2*layout_.scale,
                    layout_.panel.w-8*layout_.scale,16*layout_.scale},{34,92,113,255})) return false;
            }
            if (!SDL_SetRenderDrawColor(renderer_,235,240,250,255) ||
                !draw_text(layout_.panel.x+10*layout_.scale,y,text,
                           layout_.panel.w-20*layout_.scale)) return false;
        }
        const int detail_y=(status_first_selected() ? layout_.panel.y+46*layout_.scale:
            layout_.panel.y+52*layout_.scale+static_cast<int>(entries.size())*18*layout_.scale)-panel_scroll_;
        const auto details=inspection_lines();
        const auto controls_id=selected_building();
        const bool show_operation_controls=controls_id && world_->operation_controls_supported() &&
            simulation::World::operation_controllable(world_->building(*controls_id).kind);
        const bool show_demolition=controls_id && world_->demolition_supported();
        const int detail_bottom=layout_.panel.y+layout_.panel.h-
            (show_demolition ? (show_operation_controls ? 180:90)*layout_.scale:
             show_operation_controls ? 104*layout_.scale:0)-demolition_hint_extra_height();
        for (std::size_t i=0;i<details.size();++i) {
            const int y=detail_y+static_cast<int>(i)*17*layout_.scale;
            if (y<layout_.panel.y+(status_first_selected() ? 40*layout_.scale:0) ||
                y+10*layout_.scale>=detail_bottom) continue;
            if (!SDL_SetRenderDrawColor(renderer_,205,225,238,255) ||
                !draw_text(layout_.panel.x+10*layout_.scale,y,details[i],
                           layout_.panel.w-20*layout_.scale)) return false;
        }
        if (show_demolition) {
            const auto demolition=world_->demolition_status(*controls_id);
            const auto rect=demolition_button_rect();
            if (!fill(rect,demolition.allowed ? SDL_Color{117,65,45,255}:SDL_Color{55,55,61,255}) ||
                !SDL_SetRenderDrawColor(renderer_,245,247,250,255) ||
                !draw_text(rect.x+6*layout_.scale,rect.y+9*layout_.scale,"Demolish",rect.w-12*layout_.scale)) return false;
            const auto hints=demolition_hint_lines();
            for (std::size_t i=0;i<hints.size();++i)
                if (!draw_text(rect.x,rect.y+(36+static_cast<int>(i)*17)*layout_.scale,
                    hints[i],rect.w)) return false;
        }
        if (show_operation_controls) {
            const auto& selected=world_->building(*controls_id);
            if (simulation::World::operation_controllable(selected.kind)) {
                const auto toggle=operation_toggle_rect();
                if (!fill(toggle,{38,82,105,255}) ||
                    !SDL_SetRenderDrawColor(renderer_,245,247,250,255) ||
                    !draw_text(toggle.x+6*layout_.scale,toggle.y+9*layout_.scale,
                        selected.operating_enabled ? "Pause operation":"Resume operation",
                        toggle.w-12*layout_.scale)) return false;
                constexpr std::array<const char*,3> labels{"High","Normal","Low"};
                for (int i=0;i<3;++i) {
                    const auto priority=static_cast<simulation::WorkforcePriority>(i);
                    const auto rect=operation_priority_rect(i);
                    if (!fill(rect,selected.workforce_priority==priority ?
                        SDL_Color{38,100,125,255}:SDL_Color{38,58,77,255}) ||
                        !SDL_SetRenderDrawColor(renderer_,245,247,250,255) ||
                        !draw_text(rect.x+5*layout_.scale,rect.y+9*layout_.scale,
                                   labels[static_cast<std::size_t>(i)],
                                   rect.w-10*layout_.scale)) return false;
                }
            }
        }
    }
    if (debug_open_) {
        if (!SDL_SetRenderDrawColor(renderer_,255,230,150,255)) return false;
        const bool balance_valid=simulation::production_profile(rules_) ?
            world_->production_balance_valid():world_->goods_balance_valid();
        std::string debug="Road revision "+std::to_string(world_->road_revision())+
            " | Commands "+std::to_string(world_->command_sequence())+
            " | Balance "+(balance_valid?"OK":"ERROR");
        if (simulation::city_profile(rules_))
            debug+=" | Taxes "+std::to_string(world_->taxes_collected_total())+
                " | Spent "+std::to_string(world_->construction_spent_total())+
                " | Worker need "+std::to_string(world_->workforce_required())+
                " | Goal "+std::to_string(world_->settlement_goal_households_ready())+"/"+
                std::to_string((simulation::scalable_profile(rules_)) ? 8:4);
        if (simulation::food_profile(rules_))
            debug+=" | Food produced "+std::to_string(world_->food_produced_total())+
                " | Food balance "+(world_->food_balance_valid()?"OK":"ERROR");
        if (simulation::service_profile(rules_))
            debug+=" | Service "+std::to_string(world_->covered_households())+"/"+
                std::to_string(std::count_if(world_->buildings().begin(),world_->buildings().end(),
                    [](const simulation::BuildingState& b) {
                        return b.placed && b.kind==simulation::Object::Household;
                    }))+
                " | Service state "+(world_->service_state_valid()?"OK":"ERROR");
        if (!draw_text(8*layout_.scale,layout_.map.y+8*layout_.scale,debug,
                       layout_.map.w-16*layout_.scale)) return false;
        int debug_row=22;
        const auto fire=fire_display_stats();
        const std::string fire_line=fire.animated ?
            "Fire visual: Original animated clip | "+fire.clip_id+" | frames "+
                std::to_string(fire.frames)+" | shared textures "+std::to_string(fire.texture_uploads)+
                " | RGBA "+std::to_string(fire.logical_bytes)+" B | draws "+std::to_string(fire.draws):
            "Fire visual: Fallback - "+fire.fallback_reason;
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,fire_line,
                       layout_.map.w-16*layout_.scale)) return false;
        debug_row+=14;
        if (simulation::fire_profile(rules_)) {
            const auto inspector=fire_inspector_display_stats();
            const auto line=inspector.active ? "FireInspector: Curated original walk clip":
                "FireInspector: Marker - "+inspector.fallback_reason;
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,line,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
            if (inspector.configured) {
                if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,
                    inspector.clip_id+" | "+std::to_string(inspector.frames)+" frames | +"+
                    std::to_string(inspector.additional_assets)+" textures / "+
                    std::to_string(inspector.additional_bytes)+" B | draws "+std::to_string(inspector.draws),
                    layout_.map.w-16*layout_.scale)) return false;
                debug_row+=14;
            }
        }
        if (simulation::food_profile(rules_)) {
            const auto market=market_walker_display_stats();
            const bool any_family=!market.clip_ids[0].empty() || !market.clip_ids[1].empty();
            const auto line=any_family ? (walker_visuals_enabled_ ?
                "Food/Market: Selected curated family clips":"Food/Market: F2 marker comparison"):
                "Food/Market: Marker - "+market.fallback_reason;
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,line,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
            if (any_family) {
                const auto family=[&](std::size_t index) {
                    return market.clip_ids[index].empty() ? std::string("Marker (unconfigured)"):
                        market.clip_ids[index];
                };
                if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,
                    "Supplier: "+family(0)+" / Distributor: "+family(1)+" | "+
                    std::to_string(market.frames)+" aliases | +"+
                    std::to_string(market.additional_assets)+" textures / "+
                    std::to_string(market.additional_bytes)+" B | draws "+std::to_string(market.draws),
                    layout_.map.w-16*layout_.scale)) return false;
                debug_row+=14;
            }
            std::size_t logical=0,live_submitted=0,supplier=0,distributor=0,foreign_markers=0;
            std::array<bool,static_cast<std::size_t>(simulation::CourierRole::HealthWorker)+1> logical_roles{};
            for (const auto& courier:world_->couriers()) {
                const auto role=walker_visual_role(courier.role);
                const auto hit=std::find_if(visual_hits_.begin(),visual_hits_.end(),
                    [&](const VisualHit& value){return value.walker==courier.id;});
                const bool submitted=walker_live_visible(courier) && hit!=visual_hits_.end() &&
                    !help_open_ && !budget_warning_ && !pending_demolition_ && !walker_diagnostic_open_;
                if (food_market_visual(role)) {
                    ++logical;
                    logical_roles[static_cast<std::size_t>(courier.role)]=true;
                    if (submitted) {
                        ++live_submitted;
                        if (hit->image) {
                            if (role==assets::WalkerVisualRole::Supplier) ++supplier;
                            else ++distributor;
                        }
                    }
                } else if (submitted && !hit->image &&
                    (courier.role==simulation::CourierRole::Service ||
                     courier.role==simulation::CourierRole::HealthWorker)) ++foreign_markers;
            }
            if (logical>0) {
                if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,
                    "Food/Market roles "+std::to_string(std::count(logical_roles.begin(),logical_roles.end(),true))+
                    " | instances "+std::to_string(logical)+" | submitted "+std::to_string(live_submitted)+
                    " | families "+std::to_string(int(!market.clip_ids[0].empty())+int(!market.clip_ids[1].empty()))+
                    " | S/D figures "+std::to_string(supplier)+"/"+
                    std::to_string(distributor)+" | Service/Health markers "+
                    std::to_string(foreign_markers),layout_.map.w-16*layout_.scale)) return false;
                debug_row+=14;
            }
        }
        if (simulation::population_profile(rules_)) {
            std::string staffing="Staffing order:";
            for (const auto& b:world_->buildings()) {
                if (!b.placed || world_->workforce_required(b.id)==0) continue;
                staffing+=" "+std::to_string(static_cast<std::uint32_t>(b.id))+" "+object_name(b.kind)+
                    (world_->building_staffed(b.id)?" yes":" no");
            }
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,staffing,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
        }
        if (simulation::market_profile(rules_)) {
            const auto stock=world_->resource_inventory();
            const std::string clay="Current Clay: source "+std::to_string(stock.clay_source_output)+
                " courier "+std::to_string(stock.clay_courier_cargo)+" input "+
                std::to_string(stock.pottery_clay_input)+" recipe "+
                std::to_string(stock.recipe_clay)+" | Produced total "+
                std::to_string(world_->clay_extracted_total());
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,clay,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
            const std::string pottery="Current Pottery: producer "+
                std::to_string(stock.pottery_producer_output)+" warehouse "+
                std::to_string(stock.pottery_warehouse_stock)+" market "+
                std::to_string(stock.pottery_market_stock)+" houses "+
                std::to_string(stock.pottery_household_stock)+" courier "+
                std::to_string(stock.pottery_courier_cargo)+" reservations "+
                std::to_string(stock.pottery_reservations);
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,pottery,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
            const std::string food="Current Food: farm "+std::to_string(stock.food_farm_output)+
                " market "+std::to_string(stock.food_market_stock)+" houses "+
                std::to_string(stock.food_household_stock)+" courier "+
                std::to_string(stock.food_courier_cargo)+" reservations "+
                std::to_string(stock.food_reservations)+" | Produced total "+
                std::to_string(world_->food_produced_total());
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,food,
                           layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
        }
        if (simulation::maintenance_profile(rules_)) {
            std::size_t due=0;
            for (const auto& b:world_->buildings()) {
                const auto next=world_->maintenance_due_in(b.id);
                if (next && *next>0 && *next<=100) ++due;
            }
            const auto line="Maintenance spent "+std::to_string(world_->maintenance_spent_total())+
                " | Installed "+std::to_string(world_->current_maintenance_rate())+"/400t | Due next 100t "+std::to_string(due);
            if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,line,layout_.map.w-16*layout_.scale)) return false;
            debug_row+=14;
        }
        const auto roads=road_display_stats();
        const std::string road_line=std::string("Road visuals ")+
            (road_visuals_active()?"ON":"OFF")+" | masks "+
            std::to_string(road_profile_ ? road_profile_->configured_count():0)+
            "/16 | assets "+std::to_string(roads.unique_assets)+
            " | fallbacks "+std::to_string(roads.fallback_draws);
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,road_line,
                       layout_.map.w-16*layout_.scale)) return false;
        debug_row+=14;
        const std::string depth_line=std::string("Depth painter: ")+
            (painter_stats_.road_ground_pass ? "ground+spatial":unified_depth_ ? "snapshot":"legacy")+
            " | roads "+std::to_string(painter_stats_.road_ground_items)+" | stored "+
            std::to_string(painter_stats_.stored_items_visited)+
            (painter_stats_.road_ground_pass ? " | spatial ":" | shared ")+
            std::to_string(painter_stats_.sandbox_items);
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,depth_line,
                       layout_.map.w-16*layout_.scale)) return false;
        debug_row+=14;
        const auto debug_source=[](VisualProfileSource source) {
            return source==VisualProfileSource::Builtin ? "auto":
                visual_profile_source_name(source);
        };
        const std::string visual_line="Compatibility: "+compatibility_id_+
            " | Walker: "+debug_source(walker_source_)+
            " | Buildings: "+debug_source(building_source_)+
            " | Roads: "+debug_source(road_source_);
        if (!draw_text(8*layout_.scale,layout_.map.y+debug_row*layout_.scale,visual_line,
                       layout_.map.w-16*layout_.scale)) return false;
        debug_row+=14;

    }
    return true;
}
std::string SandboxView::walker_clip_status(const simulation::CourierState& courier) const {
    const auto role=walker_visual_role(courier.role);
    const auto* visual=role && walker_profile_ ? walker_profile_->find(*role):nullptr;
    if (!walker_live_visible(courier)) return "Hidden live instance: "+
        std::string(simulation::courier_phase_name(courier.phase));
    std::string reason;
    if (!role) reason=std::string(courier_role_label(courier.role))+
        " has no assigned visual family (outside Food/Market)";
    else if (!walker_visuals_enabled_) reason="F2 marker comparison";
    else if (!visual) {
        if (walker_source_==VisualProfileSource::Custom)
            reason="Custom profile has no configured "+std::string(assets::walker_role_name(*role));
        else if (food_market_visual(role)) reason=market_walker_fallback_reason_;
        else if (role==assets::WalkerVisualRole::FireInspector) reason=fire_inspector_fallback_reason_;
        else if (role==assets::WalkerVisualRole::Service) reason=service_walker_fallback_reason_;
        else if (role==assets::WalkerVisualRole::HealthWorker) reason=health_walker_fallback_reason_;
        else reason="Core profile has no configured "+std::string(assets::walker_role_name(*role));
    } else if (!walker_sprites_) reason="Walker asset preparation unavailable";
    else {
        const auto pose=walker_pose(courier,world_->ticks(),*visual);
        if (pose.fallback==WalkerFallback::InvalidEdge) reason="Invalid current path edge";
        else if (pose.fallback==WalkerFallback::UnmappedDirection) reason="Unmapped path direction";
    }
    if (!visual && reason.empty()) reason="No configured visual family";
    if (!reason.empty()) return "Marker fallback: "+reason;
    const auto pose=walker_pose(courier,world_->ticks(),*visual);
    const auto* frame=pose.frame ? &visual->frames[*pose.frame]:nullptr;
    return "Curated clip: "+(visual->clip_id.empty() ? std::string(assets::walker_role_name(*role)):visual->clip_id)+
        (frame ? " | #"+std::to_string(frame->id.image_index)+(frame->flip_x ? " flip_x":" native"):"");
}
std::vector<SandboxView::WalkerDiagnostic> SandboxView::walker_diagnostics() const {
    std::vector<WalkerDiagnostic> rows;
    if (!world_ || !simulation::production_profile(rules_)) return rows;
    rows.reserve(world_->couriers().size());
    for (const auto& courier:world_->couriers()) {
        WalkerDiagnostic row;
        row.id=courier.id;row.role=courier.role;row.owner=courier.owner;row.target=courier.target;
        row.phase=courier.phase;row.good=courier.good;row.cargo=courier.cargo;
        row.route_pending=courier.route_pending;row.marker_comparison=!walker_visuals_enabled_;
        row.live_visible=walker_live_visible(courier);row.direction=current_storage_direction(courier);
        if (courier.path.size()>1 && courier.path_vertex<courier.path.size()-1) {
            row.edge_from=courier.path[courier.path_vertex];row.edge_to=courier.path[courier.path_vertex+1];
        }
        const auto role=walker_visual_role(courier.role);
        row.family=role ? assets::walker_role_name(*role):"unassigned";
        const bool configured=role && walker_profile_ && walker_profile_->find(*role);
        const auto source=walker_source_==VisualProfileSource::Custom ? walker_source_:
            food_market_visual(role) && (market_walker_extension_ || !configured) ? market_walker_source_:
            role==assets::WalkerVisualRole::FireInspector && (fire_inspector_extension_ || !configured) ?
                fire_inspector_source_:
            role==assets::WalkerVisualRole::Service && (service_walker_extension_ || !configured) ?
                service_walker_source_:
                role==assets::WalkerVisualRole::HealthWorker && (health_walker_extension_ || !configured) ?
                    health_walker_source_:walker_source_;
        row.profile_source=role ? visual_profile_source_name(source):"unassigned";
        row.status=walker_clip_status(courier);
        const auto* visual=role && walker_profile_ ? walker_profile_->find(*role):nullptr;
        if (visual) {
            row.clip_id=visual->clip_id;
            const auto pose=walker_pose(courier,world_->ticks(),*visual);
            if (pose.frame) row.native_asset=visual->frames[*pose.frame].id;
        }
        if (const auto position=world_->courier_position(courier.id))
            row.screen_ground=camera_.world_to_screen(world_for(*position));
        if (row.live_visible && visual_frame_valid_) {
            const auto hit=std::find_if(visual_hits_.begin(),visual_hits_.end(),
                [&](const VisualHit& value){return value.walker==courier.id;});
            if (hit!=visual_hits_.end()) {
                row.submitted=true;row.sprite_drawn=hit->image.has_value();row.flip_x=hit->flip_x;
                row.image_origin=visual_hit_camera_.world_to_screen(hit->origin);
                row.image_width=hit->width*visual_hit_camera_.zoom;
                row.image_height=hit->height*visual_hit_camera_.zoom;
            }
        }
        rows.push_back(std::move(row));
    }
    return rows;
}
bool SandboxView::draw_walker_diagnostic() {
    if (!walker_diagnostic_open_ || !walker_profile_ || !walker_sprites_) return true;
    const float scale=static_cast<float>(layout_.scale);
    const SDL_FRect panel{static_cast<float>(layout_.map.x)+8*scale,
        static_cast<float>(layout_.map.y)+8*scale,
        std::min(470*scale,static_cast<float>(layout_.map.w)-16*scale),320*scale};
    if (!SDL_SetRenderDrawColor(renderer_,walker_diagnostic_light_ ? 232:22,
        walker_diagnostic_light_ ? 232:26,walker_diagnostic_light_ ? 220:34,255) ||
        !SDL_RenderFillRect(renderer_,&panel)) return false;
    if (!SDL_SetRenderDrawColor(renderer_,walker_diagnostic_light_ ? 15:245,
        walker_diagnostic_light_ ? 20:245,walker_diagnostic_light_ ? 30:245,255)) return false;
    constexpr const char* directions[]={"pos_x  right/down","neg_x  left/up",
        "pos_y  left/down","neg_y  right/up"};
    const auto label=[&](int row,const std::string& value)->bool {
        return draw_text(panel.x+8*scale,panel.y+static_cast<float>(12+row*18)*scale,value,
                         static_cast<int>(std::min(255*scale,panel.w-16*scale)));
    };
    const auto role=static_cast<assets::WalkerVisualRole>(walker_diagnostic_role_);
    const auto* visual=walker_profile_->find(role);
    if (!label(0,std::string("WALKER ")+assets::walker_role_name(role)+" | F3 close") ||
        !label(1,std::string(directions[walker_diagnostic_direction_])+
            (walker_diagnostic_zoom4_ ? "  4x":"  1x")) ||
        !label(2,"V role Q dir C/E X zoom B bg")) return false;
    if (!visual) return label(3,"UNCONFIGURED - no original series");
    const auto& clip=visual->clips[walker_diagnostic_direction_];
    if (clip.empty()) return label(3,"UNMAPPED - no image selected");
    const auto step=walker_diagnostic_step_%clip.size();
    const auto& frame=visual->frames[clip[step]];
    const auto& image=walker_profile_->unique_images[frame.image_index];
    const auto compact=[](double value) {
        auto text=std::to_string(value);
        while (!text.empty() && text.back()=='0') text.pop_back();
        if (!text.empty() && text.back()=='.') text.pop_back();
        return text;
    };
    if (!label(3,"Physical #"+std::to_string(frame.id.image_index)+"  step "+
        std::to_string(step+1)+"/"+std::to_string(clip.size())) ||
        !label(4,"Size "+std::to_string(image.width)+"x"+std::to_string(image.height)+
            "  foot "+compact(frame.foot_x)+","+compact(frame.foot_y)) ||
        !label(5,frame.id.archive_relative_path.generic_string()) ||
        !label(6,"tick/frame "+std::to_string(visual->ticks_per_frame)+" | "+visual->evidence) ||
        !label(7,"Clip: "+visual->clip_id+(frame.flip_x ? " | flip_x":" | native")))
        return false;
    const double zoom=(walker_diagnostic_zoom4_ ? 4.0:1.0)*layout_.scale;
    const scene::Point ground{panel.x+panel.w-105*scale,panel.y+300*scale};
    const SDL_FRect bounds{static_cast<float>(ground.x-frame.foot_x*zoom),
        static_cast<float>(ground.y-frame.foot_y*zoom),
        static_cast<float>(image.width*zoom),static_cast<float>(image.height*zoom)};
    if (!walker_sprites_->draw(clip[step],ground,zoom,*visual,*walker_profile_,
            {panel.x,panel.y},{panel.x+panel.w,panel.y+panel.h}) ||
        !SDL_SetRenderDrawColor(renderer_,20,210,245,255) ||
        !SDL_RenderRect(renderer_,&bounds)) return false;
    const float gx=static_cast<float>(ground.x),gy=static_cast<float>(ground.y);
    return SDL_SetRenderDrawColor(renderer_,245,225,40,255) &&
        SDL_RenderLine(renderer_,gx-5*scale,gy,gx+5*scale,gy) &&
        SDL_RenderLine(renderer_,gx,gy-5*scale,gx,gy+5*scale);
}
bool SandboxView::draw_help_overlay() {
    if (!help_open_) return true;
    const int margin=12*layout_.scale;
    const int x=layout_.map.x+margin;
    const int y=layout_.map.y+margin;
    const int width=std::max(0,layout_.map.w-2*margin);
    const int height=std::max(0,std::min(layout_.map.h-2*margin,(simulation::maintenance_profile(rules_) ? 350:250)*layout_.scale));
    if (width<=0 || height<=0) return true;
    const SDL_FRect panel{static_cast<float>(x),static_cast<float>(y),
        static_cast<float>(width),static_cast<float>(height)};
    if (!SDL_SetRenderDrawColor(renderer_,12,19,30,248) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,235,241,248,255)) return false;
    const int pad=12*layout_.scale;
    const int line=16*layout_.scale;
    if (simulation::maintenance_profile(rules_) && width<650*layout_.scale) {
        constexpr std::array<const char*,20> compact{
            "HELP - H / ? closes", "1-0 Build | F Watch | I Well | J Health",
            "Arrows Move | Wheel Zoom | 5 Select", "Space Pause | . Step | + / - Speed",
            "F5 Save | F9 Load | Esc Menu", "D Desirability | U Water | K Health",
            "F1 + G Road check | F2/F4/F6", "Houses pay tax after full supply",
            "Food, Pottery and Service enable tax", "Residents provide workers",
            "Select a business: Pause / Resume", "High / Normal / Low controls staffing",
            "Wells slow risk; road Health visits cure", "BUILDING MAINTENANCE",
            "Upkeep starts 400 ticks after building", "Then every 400 ticks per building",
            "Maint HUD = installed rates", "Funds may go negative; taxes repay debt",
            "Paused buildings still pay upkeep", "Demolition stops future upkeep"};
        for (std::size_t row=0;row<compact.size();++row)
            if (!draw_text(x+pad,y+pad+static_cast<int>(row)*line,compact[row],width-2*pad)) return false;
        return true;
    }
    const int column=std::max(160*layout_.scale,(width-3*pad)/2);
    const auto text=[&](int col,int row,const std::string& value)->bool {
        return draw_text(x+pad+col*column,y+pad+row*line,value,column-pad);
    };
    if (simulation::maintenance_profile(rules_)) {
        if (!text(0,16,"BUILDING MAINTENANCE") ||
            !text(0,17,"Upkeep starts 400t after building") ||
            !text(0,18,"Each building has its own cycle") ||
            !text(0,19,"Maint HUD = installed rates") ||
            !text(0,20,"Houses and roads have no upkeep") ||
            !text(1,16,"Upkeep can push Funds below zero") ||
            !text(1,17,"Taxes repay debt automatically") ||
            !text(1,18,"Paused buildings still pay upkeep") ||
            !text(1,19,"Demolition stops future upkeep") ||
            !text(1,20,"No interest or bankruptcy")) return false;
    }
    if (simulation::health_profile(rules_)) {
        if (!text(0,14,"J Health Post | K Health") ||
            !text(1,11,"Wells slow risk: +1 vs +3") ||
            !text(1,12,"Health Workers visit by road") ||
            !text(1,13,"Sick blocks demand. Visits cure.")) return false;
    }
    return text(0,0,"HELP - H / ? closes") &&
        text(0,2,"BUILD AND NAVIGATE") && text(0,3,"1 Road | 2 Clay | 3 Pottery") &&
        text(0,4,"4 Store | 5 Select | 6 Remove") &&
        text(0,5,"7 House | 8 Farm | 9 Service") &&
        text(0,6,simulation::water_profile(rules_) ? "0 Market | F Watch | I Well | U Water":"0 Market | F Fire Watch (v12/v13)") &&
        text(0,7,"Space Pause | . Step | + / - Speed") &&
        text(0,8,simulation::desirability_profile(rules_) ? "Arrows Move | D Desirability | Wheel Zoom":"WASD / Arrows Move | Wheel Zoom") &&
        text(0,9,"F5 Save | F9 Load | Esc Menu") &&
        text(0,11,"DIAGNOSTICS") &&
        text(0,12,"F1 Runtime | F2/F4/F6 Visuals") &&
        text(0,13,"F1 + G checks selected building roads") &&
        text(1,2,"ECONOMY AND WORKFORCE") &&
        text(1,3,"Houses pay tax after full supply") &&
        text(1,4,"T Income: supply / costs / upkeep") &&
        text(1,5,"Residents provide Available workers") &&
        text(1,6,"Assigned is actual staffed workforce") &&
        text(1,7,"Active excludes paused / burning work") &&
        text(1,8,"Select a business in the Inspector") &&
        text(1,9,"Pause/Resume preserves goods/building") &&
        text(1,10,"High/Normal/Low controls staffing") &&
        (simulation::health_profile(rules_) || (text(1,11,"Current deliveries finish when paused") &&
        text(1,13,"Paused work can resume without reset")));
}

bool SandboxView::draw_budget_warning_overlay() {
    if (!budget_warning_) return true;
    const int scale=layout_.scale;
    const auto bounds=budget_panel_rect();
    const int width=bounds.w;
    const int x=bounds.x,y=bounds.y;
    if (width<=0) return true;
    const SDL_FRect panel{static_cast<float>(x),static_cast<float>(y),
                          static_cast<float>(width),static_cast<float>(bounds.h)};
    if (!SDL_SetRenderDrawColor(renderer_,18,25,36,250) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,244,238,220,255) ||
        !draw_text(x+14*scale,y+16*scale,"STARTER BUDGET WARNING",width-28*scale)) return false;
    const auto lines=budget_warning_lines();
    for (std::size_t i=0;i<lines.size();++i)
        if (!draw_text(x+14*scale,y+(43+17*static_cast<int>(i))*scale,
                       lines[i],width-28*scale)) return false;
    const auto draw_button=[&](sandbox_ui::Rect rect,SDL_Color color,const char* label) {
        const SDL_FRect area{static_cast<float>(rect.x),static_cast<float>(rect.y),
                             static_cast<float>(rect.w),static_cast<float>(rect.h)};
        return SDL_SetRenderDrawColor(renderer_,color.r,color.g,color.b,255) &&
            SDL_RenderFillRect(renderer_,&area) &&
            SDL_SetRenderDrawColor(renderer_,245,247,250,255) &&
            draw_text(rect.x+10*scale,rect.y+11*scale,label,rect.w-20*scale);
    };
    return draw_button(budget_build_rect(),{117,74,48,255},"Build anyway (Y)") &&
        draw_button(budget_cancel_rect(),{45,76,94,255},"Cancel (Enter / Esc)");
}
bool SandboxView::draw_demolition_overlay() {
    if (!pending_demolition_) return true;
    const auto& b=world_->building(*pending_demolition_);
    const int scale=layout_.scale;
    const int width=std::min(540*scale,std::max(0,layout_.map.w-24*scale));
    const int height=260*scale;
    const int x=layout_.map.x+(layout_.map.w-width)/2;
    const int y=layout_.map.y+(layout_.map.h-height)/2;
    const SDL_FRect panel{static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)};
    if (!SDL_SetRenderDrawColor(renderer_,18,25,36,250) || !SDL_RenderFillRect(renderer_,&panel) ||
        !SDL_SetRenderDrawColor(renderer_,244,238,220,255) ||
        !draw_text(x+14*scale,y+24*scale,"Demolish "+std::string(object_name(b.kind))+" #"+
            std::to_string(static_cast<std::uint32_t>(b.id))+"?",width-28*scale) ||
        !draw_text(x+14*scale,y+60*scale,"No refund. This cannot be undone.",width-28*scale) ||
        !draw_text(x+14*scale,y+88*scale,b.kind==simulation::Object::Household ?
            "Residents leave immediately.":"Only an empty building can be removed.",width-28*scale)) return false;
    for (const bool confirm:{true,false}) {
        const auto rect=confirm ? budget_build_rect():budget_cancel_rect();
        const SDL_FRect area{static_cast<float>(rect.x),static_cast<float>(rect.y),static_cast<float>(rect.w),static_cast<float>(rect.h)};
        if (!SDL_SetRenderDrawColor(renderer_,confirm ? 117:45,confirm ? 65:76,confirm ? 45:94,255) ||
            !SDL_RenderFillRect(renderer_,&area) || !SDL_SetRenderDrawColor(renderer_,245,247,250,255) ||
            !draw_text(rect.x+10*scale,rect.y+11*scale,confirm ? "Demolish (Enter)":"Cancel (Esc)",rect.w-20*scale)) return false;
    }
    return true;
}
bool SandboxView::render() {
    if (hover_dirty_) refresh_hover();
    resize_camera();
    if (!SDL_SetRenderViewport(renderer_,nullptr) ||
        !SDL_SetRenderClipRect(renderer_,nullptr) ||
        !SDL_SetRenderScale(renderer_,1,1) ||
        !SDL_SetRenderDrawColor(renderer_,22,26,32,255) || !SDL_RenderClear(renderer_)) return false;
    const SDL_Rect map_clip{layout_.map.x,layout_.map.y,layout_.map.w,layout_.map.h};
    if (layout_.map.w>0 && layout_.map.h>0) {
        if (!SDL_SetRenderClipRect(renderer_,&map_clip)) return false;
        auto render_camera=camera_;
        render_camera.viewport_width=layout_.map.x+layout_.map.w;
        render_camera.viewport_height=layout_.map.y+layout_.map.h;
        bool map_ok=false;
        {
            performance::ScopedTimer timer(performance::Timing::WorldRender);
            map_ok=draw_world(render_camera);
        }
        if (!SDL_SetRenderClipRect(renderer_,nullptr) || !map_ok) return false;
        // A submitted blocking overlay hides the map until another frame is
        // drawn, including after any Help/budget/demolition close branch.
        visual_frame_valid_=!help_open_ && !budget_warning_ && !pending_demolition_ &&
            !walker_diagnostic_open_;
    }
    bool ui_ok=false;
    {
        performance::ScopedTimer timer(performance::Timing::HudRender);
        ui_ok=draw_hud() && draw_walker_diagnostic() && draw_help_overlay() &&
            draw_budget_warning_overlay() && draw_demolition_overlay();
    }
    const bool reset=SDL_SetRenderClipRect(renderer_,nullptr) &&
        SDL_SetRenderViewport(renderer_,nullptr) && SDL_SetRenderScale(renderer_,1,1);
    if (!ui_ok || !reset) return false;
    if (SDL_GetRenderTarget(renderer_)) return true;
    performance::ScopedTimer timer(performance::Timing::Present);
    return SDL_RenderPresent(renderer_);
}

} // namespace openemperor
