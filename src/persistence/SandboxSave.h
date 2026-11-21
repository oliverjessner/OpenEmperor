#pragma once

#include "simulation/World.h"
#include "simulation/OriginalTerrainFacts.h"
#include "simulation/GrowerSourceFacts.h"
#include "simulation/original/OriginalRulesModel.h"
#include "platform/AtomicReplace.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace openemperor::persistence {

struct SaveDocument {
    std::filesystem::path map_relative;
    std::string map_sha256;
    std::string buildable_sha256;
    simulation::WorldSnapshot world;
    bool migrated_from_schema1=false; // Diagnostic only; never serialized.
    std::uint32_t source_schema_version=0; // Diagnostic only; set when reading.
    // Bound schema/rule/policy identity. Original-road claims and prepared
    // topology are immutable inputs, never replacement save authority.
    std::uint32_t map_permissions_policy_version=0;
    std::string map_permissions_sha256;
    // Prepared immutable input for manual/autosave validation; never serialized.
    std::shared_ptr<const simulation::MapPermissions> prepared_permissions;
    // Session dependency only; never serialized or restored from JSON.
    std::shared_ptr<const original::OriginalRulesModel> prepared_original_rules_model;
    // Rule14–17 immutable source-map dependency only; never serialized authority.
    std::shared_ptr<const simulation::OriginalTerrainFacts> prepared_original_terrain_facts;
    // Rule21 source-bound geometry only; never serialized or inferred from roads.
    std::shared_ptr<const simulation::LandTradeBoundaryFacts> prepared_land_trade_boundary_facts;
};

// Fault injection is used only by tests; production always uses None.
using WriteFault=platform::AtomicWriteFault;

SaveDocument make_document(const std::filesystem::path& data_root,
                           const std::filesystem::path& map_relative,
                           const std::vector<std::uint8_t>& buildable,
                           const simulation::World& world);
SaveDocument read_save(const std::filesystem::path& path);
// Source identity only. Does not prepare or activate original runtime rules.
simulation::OriginalModelSource original_model_source(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
std::shared_ptr<const original::OriginalRulesModel> prepare_original_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
// Active Rule10 preparation additionally requires the named paid-production
// model categories. Frozen Rule7/8/9 preparation remains the seven-row path.
std::shared_ptr<const original::OriginalRulesModel> prepare_original_production_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
// Rule22 separately prepares the production model and admitted Theatre75 row;
// neither historical model factory requires or exposes this capability.
std::shared_ptr<const original::OriginalRulesModel> prepare_original_culture_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
// Rule24 separately prepares the five-crop model capability. Historical
// production and culture factories retain their exact admitted model sets.
std::shared_ptr<const original::OriginalRulesModel> prepare_original_agriculture_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
std::shared_ptr<const original::OriginalRulesModel> prepare_original_irrigation_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
// Explicit Rule28 additionally admits the source Fishing Quay31 model row.
std::shared_ptr<const original::OriginalRulesModel> prepare_playable_fishing_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
std::shared_ptr<const original::OriginalRulesModel> prepare_playable_hunting_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
// Rule34 separately admits source-priced Mill53 and FoodShop66 capability.
std::shared_ptr<const original::OriginalRulesModel> prepare_food_processing_rules_model(
    const std::filesystem::path& data_root,
    original::Difficulty difficulty=original::Difficulty::Normal);
// Complete supported original map, shared reader and full-file SHA binding.
// Raw bit4 facts never substitute for MapPermissions or bridge eligibility.
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_terrain_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
// Rule24 additionally binds the complete serialized fertility lane through
// the shared original-map reader; no saved fertility array grants authority.
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_agriculture_terrain_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_irrigation_terrain_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
// Shared complete map/manager/permission preparation binds marker179 geography;
// it never imports a native stock or regenerates resources from a source label.
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_playable_fishing_terrain_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_playable_hunting_terrain_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
// Transient Grower session input only. The source-bound native entry operand
// grants no spawn, path, field claim, production or saved authority.
std::shared_ptr<const simulation::GrowerSourceFacts> prepare_original_grower_source_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
std::shared_ptr<const simulation::LandTradeBoundaryFacts> prepare_land_trade_boundary_facts(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative);
// Schema 23 remains an explicit development boundary while normal Rule-7
// sessions, ticking and restore_save are unavailable.
simulation::World restore_original_economy_development(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema24/Rule8 has its own explicit source-development restore boundary.
// Normal restore and recovery never opt an existing save into this contract.
simulation::World restore_original_influence_development(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema25/Rule9 adds an explicitly inactive goods ledger; no migration or
// normal/recovery restore grants a goods-production or transport capability.
simulation::World restore_original_goods_development(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema26/Rule10 is an explicit paid-production development generation.
// Its mandatory development revision rejects incompatible in-progress saves.
simulation::World restore_original_production_development(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema27/Rule11 is a separate explicit paid fire-protection contract.
simulation::World restore_original_fire_protection(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema28/Rule12 explicitly selects the authored sustainable-economy contract.
// Existing schema26/27 documents retain their exact identities and rules.
simulation::World restore_original_sustainable_economy(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema29/Rule13 retains existing inventories and monotone courier cursors;
// its authored needs-aware selection never relabels historical saves.
simulation::World restore_original_market_logistics(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
// Schema30/Rule14 always rederives immutable terrain from the current source
// map; an optional prepared context must match all source facts exactly.
simulation::World restore_original_cardinal_water(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema31/Rule15 is a separate explicit authored Inspector dispatch contract.
// It inherits Rule14's immutable live-map terrain binding, never its generation.
simulation::World restore_original_inspector_coordination(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema32/Rule16 is a separate explicit authored deadline-aware Inspector dispatch contract.
// It inherits Rule14's immutable live-map terrain binding, never its generation.
simulation::World restore_original_deadline_fire_dispatch(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema33/Rule17 explicitly selects authored deadline-aware service dispatch.
// It retains Rule16's terrain/fire contracts and existing mutable save authority.
simulation::World restore_original_deadline_service_dispatch(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema34/Rule18 explicitly selects authored deadline-aware Food dispatch.
// It retains Rule17's service and Rule16's terrain/fire contracts and existing mutable save authority.
simulation::World restore_original_deadline_food_dispatch(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema35/Rule19 explicitly adds bounded paid-purchase retirement; no migration.
simulation::World restore_original_safe_demolition(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema38/Rule22 explicitly inherits Rule18 and adds Music School-to-Venue
// authority. It does not inherit safe demolition, source ramps or land trade.
simulation::World restore_original_music_venues(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema39/Rule23 adds witnessed physical Venue-to-House Music visits. It never
// relabels or migrates the separate Rule22/schema38 School-to-Venue contract.
simulation::World restore_original_venue_house_service(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema40/Rule24 explicitly selects Agriculture revision28 and its exact
// five-crop catalog. No historical Food state or save is automatically migrated.
simulation::World restore_original_agriculture(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema41/Rule25 adds physical plantation work without reinterpreting the
// frozen Rule24/schema40 crop contract or changing commodity dimensions.
simulation::World restore_original_plantations(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
simulation::World restore_original_irrigation(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema43/Rule27 is the explicitly AUTHORED five-crop playable successor.
// Its exact contract never migrates or reinterprets Rule26/schema42 saves.
simulation::World restore_playable_agriculture(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema44/Rule28 uses a sole six-kind Food authority and physical Fishing state.
simulation::World restore_playable_fishing(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema45/Rule29 retains one seven-kind Food authority plus real prey/Hunters.
simulation::World restore_playable_hunting(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema46/Rule30 adds bounded per-field work to the existing seven-kind Food authority.
simulation::World restore_multi_field_agriculture(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema47/Rule31 retains physical Grower work/cargo/return receipts separately.
simulation::World restore_physical_food_growers(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema48/Rule32 retains seasonal pending work and physical material receipts.
simulation::World restore_seasonal_agriculture(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema49/Rule33 retains actual wet-work carry and bounded earned bonus.
simulation::World restore_irrigation_productivity(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema50/Rule34 retains the sole twelve-kind Food ledger and real processing histories.
simulation::World restore_food_processing(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr);
// Schema36 is an explicit authored classic Rule6 successor. It does not
// inherit Original-economy rules or migrate an existing transport save.
simulation::World restore_authored_source_cell_ramps(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr);
// Schema37/Rule21 binds finite authored trade to classic Rule6 infrastructure;
// the prepared model supplies identity only and activates no Original economy.
simulation::World restore_authored_land_trade(
    const SaveDocument& document,const std::filesystem::path& data_root,
    std::vector<std::uint8_t> buildable,
    std::shared_ptr<const simulation::MapPermissions> permissions=nullptr,
    std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
    std::shared_ptr<const simulation::LandTradeBoundaryFacts> boundary=nullptr);
SaveDocument upgrade_city_v11_v2_to_v3(const SaveDocument& document);
SaveDocument upgrade_city_v11_v3_to_v4(const SaveDocument& document);
simulation::World restore_save(const SaveDocument& document,
                               const std::filesystem::path& data_root,
                               std::vector<std::uint8_t> buildable);
simulation::World restore_save(const SaveDocument& document,
                               const std::filesystem::path& data_root,
                               std::vector<std::uint8_t> buildable,
                               std::shared_ptr<const simulation::MapPermissions> permissions);
// Explicit domain/version/map-hash framing over canonical permission/topology
// bytes. Does not modify the original-map or legacy-buildability fingerprints.
std::string map_permissions_fingerprint(const simulation::MapPermissions& permissions,
                                        const std::string& map_sha256);
void write_save(const std::filesystem::path& path,const SaveDocument& document,
                const std::filesystem::path& data_root,
                const std::vector<std::uint8_t>& buildable,WriteFault fault=WriteFault::None);
void validate_save_target(const std::filesystem::path& path,const std::filesystem::path& data_root);

} // namespace openemperor::persistence
