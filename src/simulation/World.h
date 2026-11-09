#pragma once
#include "simulation/MapPermissions.h"
#include "simulation/Bridge.h"
#include "simulation/Ramp.h"
#include "simulation/SourceCellRamp.h"
#include "simulation/AuthoredLandTrade.h"
#include "simulation/OriginalEconomyState.h"
#include "simulation/OriginalInfluenceState.h"
#include "simulation/OriginalTerrainFacts.h"
#include "simulation/OriginalGoodsState.h"
#include "simulation/OriginalProductionState.h"
#include "simulation/OriginalHousingDiagnosis.h"
#include "simulation/original/Desirability.h"

#include "core/PerformanceDiagnostics.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace openemperor::original { class OriginalRulesModel; }

namespace openemperor::simulation {

inline constexpr const char* profile_name = "sandbox-logistics-v1";
inline constexpr const char* production_profile_name = "sandbox-production-v2";
enum class RulesProfile {
    LogisticsV1, ProductionV2, HouseholdV3, SettlementV4, IndustryV5, CityV6, CityV7, CityV8,
    CityV9, CityV10, CityV11, CityV12, CityV13, CityV14, CityV15, CityV16
};
inline constexpr const char* household_profile_name = "sandbox-household-v3";
inline constexpr const char* settlement_profile_name = "sandbox-settlement-v4";
inline constexpr const char* industry_profile_name = "sandbox-industry-v5";
inline constexpr const char* city_profile_name = "sandbox-city-v6";
inline constexpr const char* city_v7_profile_name = "sandbox-city-v7";
inline constexpr const char* city_v8_profile_name = "sandbox-city-v8";
inline constexpr const char* city_v9_profile_name = "sandbox-city-v9";
inline constexpr const char* city_v10_profile_name = "sandbox-city-v10";
inline constexpr const char* city_v11_profile_name = "sandbox-city-v11";
inline constexpr const char* city_v12_profile_name = "sandbox-city-v12";
inline constexpr const char* city_v13_profile_name = "sandbox-city-v13";
inline constexpr const char* city_v14_profile_name = "sandbox-city-v14";
inline constexpr const char* city_v15_profile_name = "sandbox-city-v15";
inline constexpr const char* city_v16_profile_name = "sandbox-city-v16";
constexpr bool maintenance_profile(RulesProfile profile) { return profile==RulesProfile::CityV16; }
constexpr bool health_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV15 || maintenance_profile(profile);
}
constexpr bool water_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV14 || health_profile(profile);
}
constexpr bool desirability_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV13 || water_profile(profile);
}
constexpr bool fire_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV12 || desirability_profile(profile);
}
constexpr bool market_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV11 || fire_profile(profile);
}
constexpr bool scalable_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV10 || market_profile(profile);
}
inline constexpr std::size_t legacy_max_buildings=9;
inline constexpr std::size_t legacy_max_couriers=5;
inline constexpr std::size_t city_v7_max_buildings=10;
inline constexpr std::size_t city_v7_max_couriers=6;
inline constexpr std::size_t max_buildings=11;
inline constexpr std::size_t max_couriers=7;
inline constexpr std::uint8_t first_household_id=4;
inline constexpr std::uint8_t household_limit=4;
inline constexpr std::uint8_t household_id_end=first_household_id+household_limit;
constexpr bool household_profile(RulesProfile profile) {
    return profile==RulesProfile::HouseholdV3 || profile==RulesProfile::SettlementV4 ||
           profile==RulesProfile::IndustryV5 || profile==RulesProfile::CityV6 ||
           profile==RulesProfile::CityV7 || profile==RulesProfile::CityV8 ||
           profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10 ||
           market_profile(profile);
}
constexpr bool production_profile(RulesProfile profile) {
    return profile==RulesProfile::ProductionV2 || household_profile(profile);
}
constexpr bool industry_profile(RulesProfile profile) {
    return profile==RulesProfile::IndustryV5 || profile==RulesProfile::CityV6 ||
           profile==RulesProfile::CityV7 || profile==RulesProfile::CityV8 ||
           profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10 ||
           market_profile(profile);
}
constexpr bool city_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV6 || profile==RulesProfile::CityV7 ||
           profile==RulesProfile::CityV8 || profile==RulesProfile::CityV9 ||
           scalable_profile(profile);
}
constexpr bool food_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV7 || profile==RulesProfile::CityV8 ||
           profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10 ||
           market_profile(profile);
}
constexpr bool service_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV8 || profile==RulesProfile::CityV9 ||
           scalable_profile(profile);
}
constexpr bool population_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10 ||
           market_profile(profile);
}
const char* rules_profile_name(RulesProfile profile);
struct Rules {
    static constexpr int ticks_per_second = 20;
    static constexpr int production_ticks = 100;
    static constexpr int workshop_capacity = 8;
    static constexpr int warehouse_capacity = 32;
    static constexpr int courier_capacity = 4;
    static constexpr int edge_ticks = 10;
    static constexpr int clay_ticks = 100;
    static constexpr int clay_output_capacity = 8;
    static constexpr int pottery_input_capacity = 8;
    static constexpr int pottery_output_capacity = 8;
    static constexpr int pottery_recipe_clay = 2;
    static constexpr int pottery_recipe_ticks = 150;
    static constexpr int household_capacity = 8;
    static constexpr int household_demand_ticks = 400;
    static constexpr std::uint64_t maintenance_interval_ticks = 400;
    static constexpr std::int64_t starting_treasury = 1000;
    static constexpr std::int64_t road_cost = 2;
    static constexpr std::int64_t clay_source_cost = 120;
    static constexpr std::int64_t pottery_cost = 180;
    static constexpr std::int64_t warehouse_cost = 150;
    static constexpr std::int64_t household_cost = 80;
    static constexpr int household_workers = 8;
    static constexpr int clay_source_workers = 4;
    static constexpr int pottery_workers = 6;
    static constexpr int warehouse_workers = 2;
    static constexpr std::int64_t tax_income_per_fulfilled_demand = 25;
    static constexpr int settlement_goal_fulfilled_demands = 3;
    static constexpr std::int64_t farm_cost = 160;
    static constexpr int farm_workers = 4;
    static constexpr int farm_ticks = 80;
    static constexpr int farm_output_capacity = 12;
    static constexpr int household_food_capacity = 8;
    static constexpr int city_v7_level1_demands = 2;
    static constexpr int city_v7_level2_demands = 5;
    static constexpr std::int64_t city_v7_level0_tax = 25;
    static constexpr std::int64_t city_v7_level1_tax = 40;
    static constexpr std::int64_t city_v7_level2_tax = 60;
    static constexpr std::int64_t service_post_cost = 100;
    static constexpr int service_post_workers = 2;
    static constexpr std::uint64_t service_coverage_ticks = 1200;
    static constexpr int household_initial_population = 6;
    static constexpr int household_min_population = 2;
    static constexpr int household_level0_capacity = 6;
    static constexpr int household_level1_capacity = 10;
    static constexpr int household_level2_capacity = 16;
    static constexpr int city_v9_population_goal = 48;
    static constexpr std::size_t city_v10_household_limit = 20;
    static constexpr std::size_t city_v10_clay_source_limit = 4;
    static constexpr std::size_t city_v10_pottery_limit = 4;
    static constexpr std::size_t city_v10_warehouse_limit = 2;
    static constexpr std::size_t city_v10_farm_limit = 2;
    static constexpr std::size_t city_v10_service_post_limit = 2;
    static constexpr std::size_t city_v10_building_limit = 34;
    static constexpr std::size_t city_v10_courier_limit = 14;
    static constexpr int city_v10_goal_households = 10;
    static constexpr int city_v10_goal_level2_households = 8;
    static constexpr int city_v10_population_goal = 100;
    static constexpr std::int64_t city_v11_starting_treasury = 1300;
    static constexpr std::int64_t market_cost = 140;
    static constexpr int market_workers = 4;
    static constexpr int market_pottery_capacity = 16;
    static constexpr int market_food_capacity = 16;
    static constexpr std::size_t city_v11_market_limit = 4;
    static constexpr std::size_t city_v11_building_limit = 38;
    static constexpr std::size_t city_v11_courier_limit = 22;
    static constexpr std::int64_t well_cost = 60;
    static constexpr std::size_t well_limit = 4;
    static constexpr int water_radius = 5;
    static constexpr std::size_t city_v14_building_limit = 44;
    static constexpr std::int64_t health_post_cost = 120;
    static constexpr int health_post_workers = 2;
    static constexpr std::size_t health_post_limit = 2;
    static constexpr std::size_t city_v15_building_limit = 46;
    static constexpr std::size_t city_v15_courier_limit = 26;
    static constexpr std::size_t original_production_building_limit = 82;
    static constexpr std::size_t original_production_courier_limit = 58;
    static constexpr std::uint64_t health_risk_step_ticks = 100;
    static constexpr int health_risk_threshold = 100;
    static constexpr std::uint64_t health_protection_ticks = 2400;
    static constexpr std::uint64_t illness_ticks = 1200;
    static constexpr std::int64_t fire_watch_cost = 80;
    static constexpr int fire_watch_workers = 2;
    static constexpr std::size_t fire_watch_limit = 2;
    static constexpr std::size_t city_v12_building_limit = 40;
    static constexpr std::size_t city_v12_courier_limit = 24;
    static constexpr std::uint64_t fire_risk_step_ticks = 20;
    static constexpr int fire_risk_threshold = 100;
    static constexpr std::uint64_t fire_protection_ticks = 2400;
    static constexpr std::uint64_t fire_incident_ticks = 600;
};
constexpr std::size_t building_collection_limit(RulesProfile profile) {
    return health_profile(profile) ? Rules::city_v15_building_limit:
        water_profile(profile) ? Rules::city_v14_building_limit:
        fire_profile(profile) ? Rules::city_v12_building_limit:
        market_profile(profile) ? Rules::city_v11_building_limit:Rules::city_v10_building_limit;
}
constexpr std::size_t courier_collection_limit(RulesProfile profile) {
    return health_profile(profile) ? Rules::city_v15_courier_limit:
        fire_profile(profile) ? Rules::city_v12_courier_limit:
        market_profile(profile) ? Rules::city_v11_courier_limit:Rules::city_v10_courier_limit;
}

struct ProfileRules {
    std::uint32_t version=1;
    int clay_ticks=Rules::clay_ticks;
    int pottery_recipe_ticks=Rules::pottery_recipe_ticks;
    int farm_ticks=Rules::farm_ticks;
    int courier_edge_ticks=Rules::edge_ticks;
    std::uint64_t household_move_in_grace_ticks=0;
    bool operator==(const ProfileRules&) const = default;
};

std::uint32_t current_rule_version(RulesProfile profile);
bool rule_version_supported(RulesProfile profile,std::uint32_t version);
const ProfileRules& profile_rules(RulesProfile profile,std::uint32_t version);

inline constexpr std::uint32_t kOriginalEconomyRuleVersion=7;
inline constexpr std::uint32_t kOriginalInfluenceRuleVersion=8;
inline constexpr std::uint32_t kOriginalGoodsRuleVersion=9;
inline constexpr std::uint32_t kOriginalProductionRuleVersion=10;
inline constexpr std::uint32_t kOriginalFireProtectionRuleVersion=11;
inline constexpr std::uint32_t kOriginalSustainableRuleVersion=12;
inline constexpr std::uint32_t kOriginalMarketLogisticsRuleVersion=13;
inline constexpr std::uint32_t kOriginalCardinalWaterRuleVersion=14;
inline constexpr std::uint32_t kOriginalInspectorCoordinationRuleVersion=15;
inline constexpr std::uint32_t kOriginalDeadlineFireDispatchRuleVersion=16;
inline constexpr std::uint32_t kOriginalDeadlineServiceDispatchRuleVersion=17;
inline constexpr std::uint32_t kOriginalDeadlineFoodDispatchRuleVersion=18;
inline constexpr std::uint32_t kOriginalSafeDemolitionRuleVersion=19;
inline constexpr std::uint32_t kAuthoredSourceCellRampRuleVersion=20;
inline constexpr std::uint32_t kOriginalMusicVenuesRuleVersion=22;
inline constexpr std::uint32_t kOriginalVenueHouseServiceRuleVersion=23;
inline constexpr const char* original_venue_house_service_contract="authored-venue-house-service-v1";
constexpr bool original_venue_house_service_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalVenueHouseServiceRuleVersion;
}
inline constexpr const char* original_music_venues_contract="authored-music-venue-v1";
constexpr bool original_music_venues_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && (version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
constexpr bool source_cell_ramps_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kAuthoredSourceCellRampRuleVersion;
}
inline constexpr const char* original_market_logistics_contract="authored-needs-aware-market-v1";
inline constexpr const char* original_cardinal_water_contract="verified-cardinal-water-desirability-v1";
inline constexpr const char* original_inspector_coordination_contract="authored-inspector-coordination-v1";
inline constexpr const char* original_deadline_fire_dispatch_contract="authored-deadline-fire-dispatch-v1";
inline constexpr const char* original_deadline_service_dispatch_contract="authored-deadline-service-dispatch-v1";
inline constexpr const char* original_deadline_food_dispatch_contract="authored-deadline-food-dispatch-v1";
inline constexpr const char* original_safe_demolition_contract="authored-safe-demolition-v1";
constexpr bool original_safe_demolition_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalSafeDemolitionRuleVersion;
}
constexpr bool original_deadline_food_dispatch_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalDeadlineFoodDispatchRuleVersion;
}
constexpr bool original_deadline_service_dispatch_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalDeadlineServiceDispatchRuleVersion;
}
constexpr bool original_deadline_fire_dispatch_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalDeadlineFireDispatchRuleVersion;
}
constexpr bool original_inspector_coordination_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalInspectorCoordinationRuleVersion;
}
constexpr bool original_cardinal_water_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalCardinalWaterRuleVersion;
}
constexpr bool original_cardinal_terrain_enabled(RulesProfile profile,std::uint32_t version) {
    return original_cardinal_water_enabled(profile,version) || original_inspector_coordination_enabled(profile,version) ||
        original_deadline_fire_dispatch_enabled(profile,version) || original_deadline_service_dispatch_enabled(profile,version) ||
        original_deadline_food_dispatch_enabled(profile,version) || original_safe_demolition_enabled(profile,version) ||
        original_music_venues_enabled(profile,version);
}
constexpr bool original_market_logistics_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && version==kOriginalMarketLogisticsRuleVersion;
}
constexpr bool original_market_selection_enabled(RulesProfile profile,std::uint32_t version) {
    return original_market_logistics_enabled(profile,version) || original_cardinal_terrain_enabled(profile,version);
}
constexpr bool original_sustainable_economy_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && (version==kOriginalSustainableRuleVersion || version==kOriginalMarketLogisticsRuleVersion || version==kOriginalCardinalWaterRuleVersion || version==kOriginalInspectorCoordinationRuleVersion || version==kOriginalDeadlineFireDispatchRuleVersion || version==kOriginalDeadlineServiceDispatchRuleVersion || version==kOriginalDeadlineFoodDispatchRuleVersion || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
constexpr bool original_fire_protection_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && (version==kOriginalFireProtectionRuleVersion || version==kOriginalSustainableRuleVersion || version==kOriginalMarketLogisticsRuleVersion || version==kOriginalCardinalWaterRuleVersion || version==kOriginalInspectorCoordinationRuleVersion || version==kOriginalDeadlineFireDispatchRuleVersion || version==kOriginalDeadlineServiceDispatchRuleVersion || version==kOriginalDeadlineFoodDispatchRuleVersion || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
constexpr std::size_t original_food_farm_limit(RulesProfile profile,std::uint32_t version) {
    return original_sustainable_economy_enabled(profile,version) ? 3U:2U;
}
constexpr std::size_t original_food_stock_limit(RulesProfile profile,std::uint32_t version) {
    return 24U+original_food_farm_limit(profile,version);
}
constexpr std::size_t original_food_trip_limit(RulesProfile profile,std::uint32_t version) {
    return 4U+original_food_farm_limit(profile,version);
}
inline constexpr std::int64_t original_production_starting_treasury=12000;
constexpr bool original_production_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && (version==kOriginalProductionRuleVersion ||
        version==kOriginalFireProtectionRuleVersion || version==kOriginalSustainableRuleVersion || version==kOriginalMarketLogisticsRuleVersion || version==kOriginalCardinalWaterRuleVersion || version==kOriginalInspectorCoordinationRuleVersion || version==kOriginalDeadlineFireDispatchRuleVersion || version==kOriginalDeadlineServiceDispatchRuleVersion || version==kOriginalDeadlineFoodDispatchRuleVersion || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
inline constexpr std::int64_t original_influence_starting_treasury=2000;
inline constexpr std::int64_t original_influence_source_cost=60;
inline constexpr std::size_t original_influence_source_limit=4;
inline constexpr std::size_t original_influence_purchase_limit=33;
constexpr bool original_influence_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 &&
        (version==kOriginalInfluenceRuleVersion || version==kOriginalGoodsRuleVersion ||
         version==kOriginalProductionRuleVersion || version==kOriginalFireProtectionRuleVersion || version==kOriginalSustainableRuleVersion || version==kOriginalMarketLogisticsRuleVersion || version==kOriginalCardinalWaterRuleVersion || version==kOriginalInspectorCoordinationRuleVersion || version==kOriginalDeadlineFireDispatchRuleVersion || version==kOriginalDeadlineServiceDispatchRuleVersion || version==kOriginalDeadlineFoodDispatchRuleVersion || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
constexpr bool original_goods_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 && (version==kOriginalGoodsRuleVersion ||
        version==kOriginalProductionRuleVersion || version==kOriginalFireProtectionRuleVersion || version==kOriginalSustainableRuleVersion || version==kOriginalMarketLogisticsRuleVersion || version==kOriginalCardinalWaterRuleVersion || version==kOriginalInspectorCoordinationRuleVersion || version==kOriginalDeadlineFireDispatchRuleVersion || version==kOriginalDeadlineServiceDispatchRuleVersion || version==kOriginalDeadlineFoodDispatchRuleVersion || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
constexpr bool original_economy_enabled(RulesProfile profile,std::uint32_t version) {
    return profile==RulesProfile::CityV16 &&
        (version==kOriginalEconomyRuleVersion || version==kOriginalInfluenceRuleVersion ||
         version==kOriginalGoodsRuleVersion || version==kOriginalProductionRuleVersion || version==kOriginalFireProtectionRuleVersion || version==kOriginalSustainableRuleVersion || version==kOriginalMarketLogisticsRuleVersion || version==kOriginalCardinalWaterRuleVersion || version==kOriginalInspectorCoordinationRuleVersion || version==kOriginalDeadlineFireDispatchRuleVersion || version==kOriginalDeadlineServiceDispatchRuleVersion || version==kOriginalDeadlineFoodDispatchRuleVersion || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion);
}
constexpr bool bridges_rule_enabled(RulesProfile profile,std::uint32_t version) {
    return source_cell_ramps_enabled(profile,version) || (profile==RulesProfile::CityV16&&version==kAuthoredLandTradeRuleVersion) || (profile==RulesProfile::CityV16 && (version==5 || version==6 || version==7 || version==8 || version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion));
}
constexpr bool ramps_rule_enabled(RulesProfile profile,std::uint32_t version) {
    return source_cell_ramps_enabled(profile,version) || (profile==RulesProfile::CityV16&&version==kAuthoredLandTradeRuleVersion) || (profile==RulesProfile::CityV16 && (version==6 || version==7 || version==8 || version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==kOriginalSafeDemolitionRuleVersion || version==kOriginalMusicVenuesRuleVersion || version==kOriginalVenueHouseServiceRuleVersion));
}

constexpr std::int64_t starting_treasury_for(RulesProfile profile) {
    return market_profile(profile) ? Rules::city_v11_starting_treasury:
        Rules::starting_treasury;
}
constexpr std::int64_t starting_treasury_for(RulesProfile profile,std::uint32_t version) {
    return original_production_enabled(profile,version) ? original_production_starting_treasury:
        original_influence_enabled(profile,version) ? original_influence_starting_treasury:
        starting_treasury_for(profile);
}

enum class Object : std::uint8_t {
    Empty, Road, Workshop, Warehouse, ClaySource, Pottery, Household, Farm, ServicePost, Market,
    FireWatch, Well, HealthPost, OriginalInfluenceSource, OriginalHempFarm, OriginalHempField,
    OriginalFoodFarm, OriginalWheatField, OriginalMilletField, OriginalAncestorShrine214, OriginalWaterWell72,
    OriginalDaoistShrine215, OriginalBuddhistShrine217, OriginalConfucianAcademy219,
    OriginalMusicSchool211, OriginalAcrobatSchool212, OriginalDramaSchool213,
    OriginalLoggingShed38, OriginalBronzeSmelter39, OriginalBronzewareMaker42, OriginalLacquerwareMaker44, OriginalPaperMaker45, OriginalWeaver47, OriginalTeaCuringShed237, OriginalLacquerRefinery238, OriginalSilkwormShed239, OriginalTeaBush26, OriginalLacquerTree27, OriginalMulberryTree28,
    OriginalElitePlot, OriginalEliteHouse, OriginalTheatre75
};
// Classification for the opt-in original production adapter. Older rule
// admission continues to reject the appended Elite body/command identities.
constexpr bool original_elite_house_kind(Object kind) noexcept {
    return kind==Object::OriginalElitePlot || kind==Object::OriginalEliteHouse;
}
constexpr bool original_house_kind(Object kind) noexcept {
    return kind==Object::Household || original_elite_house_kind(kind);
}
std::optional<original::ModelBuilding> original_influence_model(Object kind);
enum class TransportCellSource : std::uint8_t {
    None, SandboxRoad, OriginalMapRoad, FixedGatePassage, SandboxBridge
};
const char* transport_cell_source_name(TransportCellSource source);
std::int64_t maintenance_cost(RulesProfile profile,Object kind);
struct BuildingFootprint {
    int width=1;
    int height=1;
    bool operator==(const BuildingFootprint&) const = default;
};
struct OriginalEliteMaterializationStatus {
    bool pending=false,ready=false;
    BuildDiagnostic diagnostic;
};
struct OriginalInfluenceContribution {
    BuildingId building_id=static_cast<BuildingId>(0);
    OriginalInfluenceSourceType type=OriginalInfluenceSourceType::Well72;
    Cell origin{};
    BuildingFootprint footprint{2,2};
    original::SpreadSpec spread{};
    int before=0,contribution=0,after=0;
};
struct OriginalInfluenceProvenance {
    BuildingId household_id=static_cast<BuildingId>(0);
    Cell selected_cell{};
    int value=0;
    original::Difficulty difficulty=original::Difficulty::Normal;
    std::string model_sha256;
    std::vector<OriginalInfluenceContribution> sources;
    int raw_value=0,cardinal_water_bonus=0;
    std::string source_map_sha256;
};
struct OriginalInfluenceCache;
struct OriginalInfluenceInspection {
    Cell selected_cell{};
    int value=0;
    std::vector<OriginalInfluenceContribution> entries;
    int raw_value=0,cardinal_water_bonus=0;
    std::string source_map_sha256;
};
BuildingFootprint building_footprint(RulesProfile profile,std::uint32_t rule_version,Object kind);
std::vector<Cell> building_footprint_cells(RulesProfile profile,std::uint32_t rule_version,Object kind,Cell origin);
Cell building_front_cell(RulesProfile profile,std::uint32_t rule_version,Object kind,Cell origin);
bool building_footprint_contains(RulesProfile profile,std::uint32_t rule_version,Object kind,Cell origin,Cell cell);
// Pure authored City-v13 geometry/scoring helpers. No road or map semantics.
inline constexpr int desirability_radius=8;
enum class BuildingId : std::uint32_t;
struct DesirabilitySource {
    BuildingId id;
    Object kind=Object::Empty;
    int distance=0, contribution=0;
};
int footprint_distance(Cell a,BuildingFootprint af,Cell b,BuildingFootprint bf);
int desirability_impact(Object kind);
int desirability_contribution(Object kind,int distance);
int desirability_level_cap(int score);
int clamp_desirability(int score);
struct BuildingEntrance {
    Cell road_cell{};
    Cell building_cell{};
    bool operator==(const BuildingEntrance&) const = default;
};
enum class BuildingId : std::uint32_t;
enum class WorkforcePriority : std::uint8_t { High=0, Normal=1, Low=2 };
const char* workforce_priority_name(WorkforcePriority priority);
enum class CommandType { PlaceRoad, PlaceWorkshop, PlaceWarehouse, PlaceClaySource, PlacePottery,
                         RemoveRoad, PlaceHousehold, PlaceFarm, PlaceServicePost, PlaceMarket,
                         SetBuildingOperation, SetBuildingWorkforcePriority, DemolishBuilding,
                         PlaceFireWatch, PlaceWell, PlaceHealthPost, PlaceBridge, RemoveBridge, PlaceRamp, RemoveRamp,
                         PlaceOriginalInfluenceSource, PlaceOriginalHempFarm, PlaceOriginalHempField,
                         PlaceOriginalFoodFarm, PlaceOriginalWheatField, PlaceOriginalMilletField, PlaceOriginalAncestorShrine, PlaceOriginalWaterWell,
                         PlaceOriginalDaoistShrine, PlaceOriginalBuddhistShrine, PlaceOriginalConfucianAcademy,
                         PlaceOriginalMusicSchool, PlaceOriginalAcrobatSchool, PlaceOriginalDramaSchool,
                         PlaceOriginalLoggingShed, PlaceOriginalBronzeSmelter, PlaceOriginalBronzewareMaker, PlaceOriginalLacquerwareMaker, PlaceOriginalPaperMaker, PlaceOriginalWeaver, PlaceOriginalTeaCuringShed, PlaceOriginalLacquerRefinery, PlaceOriginalSilkwormShed, PlaceOriginalTeaBush, PlaceOriginalLacquerTree, PlaceOriginalMulberryTree, PlaceOriginalElitePlot, PlaceSourceCellRamp, ConfigureLandTrade, ImportPottery, ExportPottery, PlaceOriginalTheatre };
// Pure construction-command identity shared by validation and presentation.
std::optional<Object> placed_object(CommandType type);
// Fixed identities of active paid industry bodies; no file/model lookup here.
std::optional<original::ModelBuilding> original_industry_model(Object kind);
bool original_industry_source(Object kind);
bool original_industry_plant(Object kind);
struct Command {
    CommandType type;
    Cell cell{};
    BuildingId building_id=static_cast<BuildingId>(0);
    bool operating_enabled=true;
    WorkforcePriority workforce_priority=WorkforcePriority::Normal;
    Cell bridge_end{};
    BridgeId bridge_id{};
    Cell ramp_end{};
    RampId ramp_id{};
    OriginalInfluenceSourceType source_type=OriginalInfluenceSourceType::Well72;
    BuildingId trade_gateway=static_cast<BuildingId>(0);
};
Command place_bridge(Cell bank_a,Cell bank_b);
Command place_ramp(Cell endpoint_a,Cell endpoint_b);
Command place_source_cell_ramp(Cell source_cell,Cell high_neighbor);
Command configure_land_trade(BuildingId warehouse,BuildingId gateway);
Command import_pottery();
Command export_pottery();
Command demolish_building(BuildingId id);
Command set_building_operation(BuildingId id,bool enabled);
Command set_building_workforce_priority(BuildingId id,WorkforcePriority priority);
struct CommandResult {
    bool accepted=false;
    bool changed=false;
    std::string reason;
    std::uint64_t sequence=0;
    std::uint64_t tick=0;
    BuildDiagnostic diagnostic{};
};
struct RoadBatchValidation {
    bool accepted=false;
    std::string reason;
    std::size_t new_road_count=0;
    std::int64_t total_cost=0;
    BuildDiagnostic diagnostic{};
    std::vector<BuildDiagnostic> additional_blockers{};
    std::vector<RoadCellDiagnostic> cell_diagnostics{};
};
enum class CourierPhase { IdleAtWorkshop, ToWarehouse, Returning };
const char* courier_phase_name(CourierPhase phase);
struct Position { double x=0; double y=0; bool operator==(const Position&) const = default; };
enum class Good { Goods, Clay, Pottery, Food };
enum class BuildingId : std::uint32_t {
    ClaySource=1, Pottery=2, Warehouse=3, Household=4, Farm=10, ServicePost=11
};
enum class CourierId : std::uint32_t { Clay=1, Pottery=2, Household=3, Food=6, Service=7 };
enum class CourierRole : std::uint8_t {
    None=0, Clay=1, Pottery=2, Household=3, Food=4, Service=5,
    MarketPotteryInbound=6, MarketFoodInbound=7,
    MarketPotteryDistribution=8, MarketFoodDistribution=9, FireInspector=10, HealthWorker=11,
    OriginalHempToWarehouse=12, OriginalFoodToMarket=13, OriginalFoodToHouse=14, OriginalAncestor=15, OriginalWater=16,
    OriginalDaoist=17, OriginalBuddhist=18, OriginalConfucian=19,
    OriginalMusic=20, OriginalAcrobat=21, OriginalDrama=22,
    OriginalLuxuryToWarehouse=23, OriginalAuxToWarehouse=24, OriginalAuxSupplier=25, AuthoredLandTrader=26,
    OriginalMusicPerformer=27, OriginalVenueMusic=28
};
enum class CourierDispatchStatus : std::uint8_t {
    Ready,
    NoStock,
    NoRoad,
    TargetFull,
    NoTarget,
    AlreadyMoving,
    WaitingForRoadRevision,
    Unstaffed,
    OperationPaused,
    Disabled,
    OnFire
};
const char* courier_dispatch_status_name(CourierDispatchStatus status);
struct CourierDispatchDecision {
    CourierDispatchStatus status=CourierDispatchStatus::Disabled;
    std::optional<BuildingId> selected_target;
};
struct BuildingState {
    BuildingId id=BuildingId::ClaySource;
    Object kind=Object::Empty;
    Cell cell{};
    bool placed=false;
    int input_clay=0;
    int output=0; // Clay at source; Pottery at pottery.
    int pottery_stock=0; // Warehouse, Household, or Market according to kind.
    int reserved_incoming=0;
    int progress=0;
    int active_recipe_clay=0;
    std::uint64_t recipes_completed=0;
    std::uint64_t placed_tick=0;
    int demand_progress=0;
    std::uint64_t fulfilled_demand=0, missed_demand=0, consumed_total=0;
    int last_demand_status=0; // 0 none, 1 fulfilled, 2 missed.
    std::uint64_t clay_extracted=0;
    int food_stock=0;
    int reserved_food_incoming=0;
    std::uint64_t food_consumed_total=0;
    std::uint64_t food_produced=0;
    std::uint64_t service_until_tick=0;
    int population=0;
    bool operating_enabled=true;
    WorkforcePriority workforce_priority=WorkforcePriority::Normal;
    int fire_risk=0;
    std::uint64_t fire_protection_until_tick=0, fire_until_tick=0;
    std::uint64_t taxes_paid_total=0; // City-v13+: actual payments, never inferred from level.
    int health_risk=0;
    std::uint64_t health_protection_until_tick=0, sick_until_tick=0;
    bool operator==(const BuildingState&) const = default;
};
// Current demand inputs, not a promise of payment at a future deadline.
struct HouseholdDemandStatus {
    bool pottery_available=false,food_available=false,service_available=false;
    bool food_required=false,service_required=false;
    bool burning=false,sick=false,ready=false;
};
// Ownership costs in (current tick, current tick + horizon], with no new
// construction, demolition or income. Missing values denote arithmetic limits.
struct MaintenanceProjection {
    bool applicable=false;
    std::uint64_t horizon_ticks=Rules::maintenance_interval_ticks;
    std::optional<std::uint64_t> horizon_end_tick,next_bill_tick;
    std::int64_t installed_rate=0,next_bill_cost=0;
    std::optional<std::int64_t> interval_cost,funds_after_interval;
};
bool fire_eligible(Object kind);
// The authored Watch becomes a combustible target only in the explicit Rule11 contract.
bool fire_eligible(Object kind,RulesProfile profile,std::uint32_t rule_version);
bool courier_can_target(CourierRole role,const BuildingState& target);
bool courier_can_target(CourierRole role,const BuildingState& target,
                        RulesProfile profile,std::uint32_t rule_version);
struct CourierState {
    CourierId id=CourierId::Clay;
    CourierRole role=CourierRole::None;
    BuildingId owner=BuildingId::ClaySource;
    BuildingId target=BuildingId::Pottery;
    Good good=Good::Clay;
    bool enabled=false;
    CourierPhase phase=CourierPhase::IdleAtWorkshop;
    int cargo=0;
    int reserved=0;
    std::vector<Cell> path;
    std::size_t path_vertex=0;
    int edge_progress=0;
    bool route_pending=false;
    std::optional<std::uint64_t> route_checked_revision;
    std::uint64_t reroute_attempts=0;
    std::uint64_t cached_revision=UINT64_MAX;
    std::optional<std::vector<Cell>> cached_route;
    std::optional<BuildingId> last_dispatched_pottery;
    std::array<std::optional<std::vector<Cell>>,max_buildings> target_routes{};
    std::optional<BuildingId> last_dispatched_target;
    std::vector<std::pair<BuildingId,std::optional<std::vector<Cell>>>> dynamic_target_routes;
};
// Only authoritative state. Occupancy and future route caches are rebuilt.
struct CourierSnapshot {
    CourierId id=CourierId::Clay;
    CourierRole role=CourierRole::None;
    BuildingId owner=BuildingId::ClaySource, target=BuildingId::Pottery;
    Good good=Good::Clay;
    bool enabled=false;
    CourierPhase phase=CourierPhase::IdleAtWorkshop;
    int cargo=0, reserved=0;
    std::vector<Cell> path;
    std::size_t path_vertex=0;
    int edge_progress=0;
    bool route_pending=false;
    std::optional<std::uint64_t> route_checked_revision;
    std::uint64_t reroute_attempts=0;
    std::optional<BuildingId> last_dispatched_pottery;
    std::optional<BuildingId> last_dispatched_target;
    bool operator==(const CourierSnapshot&) const = default;
};
struct BuildingSnapshot {
    BuildingId id=BuildingId::ClaySource;
    Object kind=Object::Empty;
    Cell cell{};
    bool placed=false;
    int input_clay=0, output=0, pottery_stock=0, reserved_incoming=0;
    int progress=0, active_recipe_clay=0;
    std::uint64_t recipes_completed=0;
    std::uint64_t placed_tick=0;
    int demand_progress=0;
    std::uint64_t fulfilled_demand=0, missed_demand=0, consumed_total=0;
    int last_demand_status=0;
    std::uint64_t clay_extracted=0;
    int food_stock=0, reserved_food_incoming=0;
    std::uint64_t food_consumed_total=0, food_produced=0;
    std::uint64_t service_until_tick=0;
    int population=0;
    bool operating_enabled=true;
    WorkforcePriority workforce_priority=WorkforcePriority::Normal;
    int fire_risk=0;
    std::uint64_t fire_protection_until_tick=0, fire_until_tick=0;
    std::uint64_t taxes_paid_total=0;
    int health_risk=0;
    std::uint64_t health_protection_until_tick=0, sick_until_tick=0;
    bool operator==(const BuildingSnapshot&) const = default;
};
// Historical accounting only. No removed entities, goods, population or UI state.
struct DemolitionHistory {
    std::uint64_t buildings=0;
    std::uint64_t clay_extracted=0, pottery_completed=0, food_produced=0;
    std::uint64_t pottery_consumed=0, food_consumed=0;
    std::uint64_t taxes=0, construction_spent=0;
    bool operator==(const DemolitionHistory&) const = default;
};
enum class DemolitionBlocker {
    None, UnsupportedProfile, MissingBuilding, UnsupportedKind, OnFire,
    OwnedActiveCourier, IncomingDelivery, IncomingReservation, OwnedPayload,
    ActiveRecipe, StoredGoods, ExhaustedCounter,
    OccupiedHouse, HistoricalReference, HistoryCapacity, DependentField,
    OperationEnabled, ActiveProduction
};
struct DemolitionStatus {
    bool allowed=false;
    DemolitionBlocker blocker=DemolitionBlocker::None; // Derived diagnostic, never persisted.
    std::string reason;
    std::string stored_goods_summary;
    std::size_t active_couriers=0, incoming_deliveries=0;
};
struct WorldSnapshot {
    int width=0, height=0;
    RulesProfile profile=RulesProfile::LogisticsV1;
    std::uint32_t rule_version=1;
    std::uint64_t ticks=0, command_sequence=0, road_revision=0;
    std::uint64_t roads_placed_total=0, roads_removed_total=0;
    std::vector<Cell> roads;
    std::optional<Cell> workshop, warehouse;
    std::uint64_t total_produced=0, clay_extracted_total=0, pottery_completed_total=0;
    int workshop_stock=0, production_progress=0, courier_cargo=0, warehouse_stock=0;
    CourierPhase phase=CourierPhase::IdleAtWorkshop;
    std::vector<Cell> path;
    std::size_t path_vertex=0;
    int edge_progress=0;
    std::vector<BuildingSnapshot> buildings;
    std::vector<CourierSnapshot> couriers;
    std::uint32_t next_household_id=first_household_id;
    std::uint32_t next_production_id=8;
    std::uint32_t next_courier_id=4;
    std::uint32_t next_building_id=1;
    std::optional<BuildingId> last_dispatched_household;
    std::optional<BuildingId> last_dispatched_food_household;
    std::optional<BuildingId> last_dispatched_service_household;
    std::uint64_t food_produced_total=0;
    std::int64_t treasury=0;
    std::uint64_t taxes_collected_total=0;
    std::uint64_t construction_spent_total=0;
    std::uint64_t maintenance_spent_total=0;
    DemolitionHistory demolition_history;
    std::vector<BridgeSpan> bridges;
    std::uint32_t next_bridge_id=1;
    std::vector<RampLink> ramps;
    std::uint32_t next_ramp_id=1;
    std::vector<SourceCellRamp> source_cell_ramps;
    std::uint32_t next_source_cell_ramp_id=1;
    std::optional<OriginalEconomyState> original_economy;
    std::optional<OriginalInfluenceState> original_influence;
    std::optional<OriginalGoodsState> original_goods;
    std::optional<OriginalProductionState> original_production;
    std::optional<AuthoredLandTradeState> land_trade;
    bool operator==(const WorldSnapshot&) const = default;
};
struct ResourceInventory {
    int clay_source_output=0,clay_courier_cargo=0,pottery_clay_input=0,recipe_clay=0;
    int pottery_producer_output=0,pottery_warehouse_stock=0,pottery_market_stock=0,
        pottery_household_stock=0,pottery_courier_cargo=0;
    int food_farm_output=0,food_market_stock=0,food_household_stock=0,food_courier_cargo=0;
    int pottery_reservations=0,food_reservations=0;
    bool operator==(const ResourceInventory&) const = default;
};
const char* delivery_phase_name(CourierPhase phase);

class World {
public:
    bool authored_land_trade_enabled() const {return profile_==RulesProfile::CityV16 && rule_version_==kAuthoredLandTradeRuleVersion;}
    const std::optional<AuthoredLandTradeState>& land_trade_state() const {return land_trade_;}
    LandTradeBalance land_trade_balance() const;
    static World create_authored_land_trade(std::shared_ptr<const MapPermissions>,
        std::shared_ptr<const original::OriginalRulesModel>,std::shared_ptr<const LandTradeBoundaryFacts>);
    static World restore_authored_land_trade(const WorldSnapshot&,std::shared_ptr<const MapPermissions>,
        std::shared_ptr<const original::OriginalRulesModel>,std::shared_ptr<const LandTradeBoundaryFacts>);
    static void validate_authored_land_trade_snapshot(const WorldSnapshot&);
    bool original_economy_enabled() const { return simulation::original_economy_enabled(profile_,rule_version_); }
    bool original_venue_house_service_enabled() const { return simulation::original_venue_house_service_enabled(profile_,rule_version_); }
    bool original_music_venues_enabled() const { return simulation::original_music_venues_enabled(profile_,rule_version_); }
    bool original_influence_enabled() const { return simulation::original_influence_enabled(profile_,rule_version_); }
    bool original_goods_enabled() const { return simulation::original_goods_enabled(profile_,rule_version_); }
    bool original_production_enabled() const { return simulation::original_production_enabled(profile_,rule_version_); }
    bool original_fire_protection_enabled() const { return simulation::original_fire_protection_enabled(profile_,rule_version_); }
    bool original_sustainable_economy_enabled() const { return simulation::original_sustainable_economy_enabled(profile_,rule_version_); }
    bool original_market_logistics_enabled() const { return simulation::original_market_logistics_enabled(profile_,rule_version_); }
    bool original_cardinal_water_enabled() const { return simulation::original_cardinal_terrain_enabled(profile_,rule_version_); }
    bool original_deadline_service_dispatch_enabled() const { return simulation::original_deadline_service_dispatch_enabled(profile_,rule_version_); }
    bool original_deadline_food_dispatch_enabled() const {
        return simulation::original_deadline_food_dispatch_enabled(profile_,rule_version_) || original_safe_demolition_enabled() || original_music_venues_enabled();
    }
    bool original_safe_demolition_enabled() const { return simulation::original_safe_demolition_enabled(profile_,rule_version_); }
    bool original_deadline_fire_dispatch_enabled() const { return simulation::original_deadline_fire_dispatch_enabled(profile_,rule_version_); }
    bool original_inspector_coordination_enabled() const { return simulation::original_inspector_coordination_enabled(profile_,rule_version_); }
    bool original_market_selection_enabled() const { return simulation::original_market_selection_enabled(profile_,rule_version_); }
    // Authored installed liability: Rule12 uses one existing 816t economy
    // month; historical generations retain their exact 400t invoice clock.
    std::uint64_t original_worker_upkeep_interval() const {
        return original_sustainable_economy_enabled() ? original_tax_interval_ticks:original_worker_upkeep_interval_ticks;
    }
    const std::optional<OriginalEconomyState>& original_economy_state() const { return original_economy_; }
    const std::optional<OriginalInfluenceState>& original_influence_state() const { return original_influence_; }
    const std::optional<OriginalGoodsState>& original_goods_state() const { return original_goods_; }
    const std::optional<OriginalProductionState>& original_production_state() const { return original_production_; }
    OriginalEliteMaterializationStatus original_elite_materialization_status(BuildingId house) const;
    OriginalServiceAvailability original_service_availability(BuildingId house,original::ServiceRole role) const;
    OriginalHousingDiagnosis original_housing_diagnosis(BuildingId house) const;
    std::array<OriginalHousingRowDiagnosis,original::house_row_count> original_housing_decision_matrix(BuildingId house) const;
    OriginalAuxBalance original_auxiliary_balance(OriginalAuxMaterial material) const;
    int original_auxiliary_cargo(CourierId id) const;
    OriginalGoodsBalance original_goods_balance(original::Goods good) const;
    std::array<OriginalGoodsBalance,original::goods_count> original_goods_balances() const;
    int original_courier_cargo(CourierId courier) const;
    int original_food_cargo(CourierId courier) const;
    std::optional<original::Goods> original_courier_good(CourierId courier) const;
    // Transient on-demand test/diagnostic observation, never save authority.
    std::uint64_t original_influence_cache_build_count() const { return original_influence_cache_build_count_; }
    OriginalInfluenceProvenance original_influence_provenance(BuildingId household) const;
    OriginalInfluenceInspection original_influence_inspection(BuildingId household) const;
    int original_house_influence_value(BuildingId household) const;
    int original_house_cardinal_water_bonus(BuildingId household) const;
    std::shared_ptr<const OriginalTerrainFacts> original_terrain_facts() const { return original_terrain_facts_; }
    std::shared_ptr<const original::OriginalRulesModel> original_rules_model() const { return original_rules_model_; }
    bool bridges_supported() const { return bridges_rule_enabled(profile_,rule_version_); }
    bool ramps_supported() const { return ramps_rule_enabled(profile_,rule_version_); }
    bool source_cell_ramps_supported() const { return source_cell_ramps_enabled(profile_,rule_version_); }
    static World create_authored_source_cell_ramps(std::shared_ptr<const MapPermissions> permissions);
    std::span<const SourceCellRamp> source_cell_ramps() const { return source_cell_ramps_; }
    const SourceCellRamp* source_cell_ramp_source_at(Cell cell) const;
    const SourceCellRamp* source_cell_ramp_claim(Cell cell) const;
    SourceCellRampValidation validate_source_cell_ramp(Cell source_cell,Cell high_neighbor) const;
    std::optional<double> source_cell_ramp_surface_height(Position position) const;
    std::span<const RampLink> ramps() const { return ramps_; }
    const RampLink* ramp_endpoint(Cell cell) const;
    const RampLink* ramp_between(Cell a,Cell b) const;
    RampValidation validate_ramp(Cell a,Cell b) const;
    std::optional<double> ramp_surface_height(Position position) const;
    std::span<const BridgeSpan> bridges() const { return bridges_; }
    const BridgeSpan* bridge_at(Cell cell) const;
    const BridgeSpan* bridge_bank_at(Cell cell) const;
    BridgeValidation validate_bridge(Cell bank_a,Cell bank_b) const;
    BuildBlocker transport_edge_blocker(Cell from,Cell to) const;
    std::int8_t transport_height(Cell cell) const;
    World(int width,int height,std::vector<std::uint8_t> buildable,
          RulesProfile profile=RulesProfile::LogisticsV1,std::uint32_t rule_version=0);
    World(std::shared_ptr<const MapPermissions> permissions,
          RulesProfile profile=RulesProfile::CityV16,std::uint32_t rule_version=0);
    // Explicit development fixture for infrastructure and paid vacant containers.
    // Bound models are explicit immutable session inputs. Unbound infrastructure
    // and clock fixtures remain possible, but never acquire save authority.
    static World create_original_economy_development(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
    static World restore_original_economy_development(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model=nullptr);
    // Map-independent authority validation shared by schema-23 parsing and
    // explicit development restore. Geometry/accounting still require restore.
    static void validate_original_economy_snapshot(const WorldSnapshot& snapshot);
    static World create_original_influence_development(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World restore_original_influence_development(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static void validate_original_influence_snapshot(const WorldSnapshot& snapshot);
    // Explicit Rule9/Schema25 accounting foundation; no active goods capability.
    static World create_original_goods_development(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World restore_original_goods_development(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static void validate_original_goods_snapshot(const WorldSnapshot& snapshot);
    static World create_original_production_development(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World restore_original_production_development(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static void validate_original_production_snapshot(const WorldSnapshot& snapshot);
    static World create_original_music_venues(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_music_venues(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static void validate_original_music_venues_snapshot(const WorldSnapshot& snapshot);
    static World create_original_venue_house_service(std::shared_ptr<const MapPermissions>,
        std::shared_ptr<const original::OriginalRulesModel>,std::shared_ptr<const OriginalTerrainFacts>);
    static World restore_original_venue_house_service(const WorldSnapshot&,
        std::shared_ptr<const MapPermissions>,std::shared_ptr<const original::OriginalRulesModel>,
        std::shared_ptr<const OriginalTerrainFacts>);
    static void validate_original_venue_house_service_snapshot(const WorldSnapshot&);
    static void validate_original_fire_protection_snapshot(const WorldSnapshot& snapshot);
    // Explicit authored paid fire protection; historical Rule10 factories remain Rule10-only.
    static World create_original_fire_protection(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World restore_original_fire_protection(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    // Explicit authored capacity/finance generation; historical factories never migrate.
    static World create_original_sustainable_economy(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World restore_original_sustainable_economy(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    // Separate authored needs-aware logistics; no migration of historical cities.
    static World create_original_market_logistics(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World restore_original_market_logistics(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model);
    static World create_original_cardinal_water(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_cardinal_water(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World create_original_inspector_coordination(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_inspector_coordination(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    // Separate authored deadline-aware dispatch; no new saved mutable authority.
    static World create_original_deadline_fire_dispatch(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_deadline_fire_dispatch(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    // Separate authored service ordering; inherits Rule16 fire dispatch unchanged.
    static World create_original_deadline_service_dispatch(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_deadline_service_dispatch(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World create_original_deadline_food_dispatch(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_deadline_food_dispatch(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    // Explicit authored, bounded empty-House/settled-Clay retirement. Historical
    // generation factories never admit or migrate this authority.
    static World create_original_safe_demolition(std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    static World restore_original_safe_demolition(const WorldSnapshot& snapshot,
        std::shared_ptr<const MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const OriginalTerrainFacts> terrain);
    RulesProfile profile() const { return profile_; }
    std::uint32_t rule_version() const { return rule_version_; }
    const ProfileRules& active_rules() const { return profile_rules(profile_,rule_version_); }
    int width() const { return width_; }
    int height() const { return height_; }
    bool in_bounds(Cell cell) const;
    bool buildable(Cell cell) const;
    bool road_buildable(Cell cell) const;
    bool fixed_passage(Cell cell) const;
    bool original_road(Cell cell) const;
    bool road_cell(Cell cell) const;
    bool transport_cell(Cell cell) const;
    TransportCellSource transport_source(Cell cell) const;
    bool transport_edge_allowed(Cell from,Cell to) const;
    const std::shared_ptr<const MapPermissions>& map_permissions() const { return permissions_; }
    Object object_at(Cell cell) const;
    std::optional<BuildingId> building_owner_at(Cell cell) const;
    const BuildingState& building(BuildingId id) const;
    std::span<const BuildingState> buildings() const { return buildings_; }
    std::span<const CourierState> couriers() const { return couriers_; }
    std::uint32_t next_household_id() const { return next_household_id_; }
    std::uint32_t next_production_id() const { return next_production_id_; }
    std::uint32_t next_courier_id() const { return next_courier_id_; }
    std::uint32_t next_building_id() const { return next_building_id_; }
    std::optional<BuildingId> last_dispatched_household() const { return last_dispatched_household_; }
    std::optional<BuildingId> last_dispatched_food_household() const { return last_dispatched_food_household_; }
    std::optional<BuildingId> last_dispatched_service_household() const {
        return last_dispatched_service_household_;
    }
    std::optional<BuildingId> next_household_candidate() const;
    std::optional<BuildingId> next_pottery_candidate(CourierId id) const;
    bool household_route_available(BuildingId id) const;
    // Known geometric reachability from existing current-revision Service caches.
    // Paused/unstaffed posts may have a route; no search or prediction is performed.
    std::optional<bool> service_route_available(BuildingId household) const;
    const CourierState& courier(CourierId id) const;
    std::optional<Position> courier_position(CourierId id) const;
    CourierDispatchDecision courier_dispatch_status(CourierId id) const;
    const char* courier_blockage(CourierId id) const;
    const char* pottery_blockage() const;
    std::uint64_t clay_extracted_total() const { return clay_extracted_total_; }
    std::uint64_t pottery_completed_total() const { return pottery_completed_total_; }
    std::int64_t treasury() const { return treasury_; }
    std::uint64_t taxes_collected_total() const { return taxes_collected_total_; }
    std::uint64_t construction_spent_total() const { return construction_spent_total_; }
    std::uint64_t maintenance_spent_total() const { return maintenance_spent_total_; }
    OriginalHouseTaxStatus original_house_tax_status(BuildingId id) const;
    OriginalGoalStatus original_goal_status() const;
    std::int64_t current_maintenance_rate() const;
    std::optional<std::uint64_t> maintenance_due_in(BuildingId id) const;
    MaintenanceProjection maintenance_projection(
        std::uint64_t horizon=Rules::maintenance_interval_ticks) const;
    std::uint64_t food_produced_total() const { return food_produced_total_; }
    ResourceInventory resource_inventory() const;
    int workforce_supply() const;
    int workforce_required() const;
    int active_workforce_required() const;
    int workforce_used() const;
    int workers_assigned(BuildingId id) const;
    int workforce_required(BuildingId id) const;
    bool building_staffed(BuildingId id) const;
    bool operation_controls_supported() const {
        return fire_profile(profile_) || (profile_==RulesProfile::CityV11 && rule_version_>=3);
    }
    static bool operation_controllable(Object kind);
    bool settlement_goal_reached() const;
    int settlement_goal_households_ready() const;
    bool city_economy_valid() const;
    bool food_balance_valid() const;
    bool service_state_valid() const;
    int historical_household_level(BuildingId id) const;
    int household_level(BuildingId id) const;
    int building_distance(BuildingId a,BuildingId b) const;
    int household_desirability(BuildingId id) const;
    int household_desirability_at(Cell origin) const;
    std::vector<DesirabilitySource> household_desirability_sources(BuildingId id) const;
    // Pure, road-independent infrastructure queries; no coverage is persisted.
    bool household_has_water(BuildingId id) const;
    bool household_has_water_at(Cell origin) const;
    std::optional<BuildingId> nearest_water_source(BuildingId id) const;
    std::optional<BuildingId> nearest_water_source_at(Cell origin) const;
    int water_covered_households() const;
    struct WellCoverage { int households=0, currently_dry=0; };
    WellCoverage well_coverage_at(Cell origin) const;
    bool household_service_active(BuildingId id) const;
    HouseholdDemandStatus household_demand_status(BuildingId id) const;
    std::uint64_t household_service_remaining(BuildingId id) const;
    std::uint64_t household_move_in_grace_remaining(BuildingId id) const;
    int covered_households() const;
    int household_population(BuildingId id) const;
    int household_population_capacity(BuildingId id) const;
    int total_population() const;
    int total_population_capacity() const;
    int unemployed_workers() const;
    bool population_valid() const;
    std::uint64_t household_tax_contributed(BuildingId id) const;
    std::int64_t construction_cost(CommandType type) const;
    bool production_balance_valid() const;
    bool demolition_supported() const {
        return fire_profile(profile_) || (profile_==RulesProfile::CityV11 && rule_version_==4);
    }
    bool building_fire_protected(BuildingId id) const;
    bool building_on_fire(BuildingId id) const;
    std::uint64_t fire_protection_remaining(BuildingId id) const;
    std::uint64_t fire_remaining(BuildingId id) const;
    std::size_t fire_eligible_buildings() const;
    std::size_t protected_buildings() const;
    std::size_t burning_buildings() const;
    bool fire_state_valid() const;
    int household_health_risk(BuildingId id) const;
    bool household_health_protected(BuildingId id) const;
    bool household_sick(BuildingId id) const;
    std::uint64_t household_health_protection_remaining(BuildingId id) const;
    std::uint64_t household_sickness_remaining(BuildingId id) const;
    bool health_state_valid() const;
    DemolitionStatus demolition_status(BuildingId id) const;
    std::string canonical_state() const;
    WorldSnapshot snapshot() const;
    static World restore(const WorldSnapshot& snapshot,std::vector<std::uint8_t> buildable);
    static World restore(const WorldSnapshot& snapshot,std::shared_ptr<const MapPermissions> permissions);
    void import_snapshot(const WorldSnapshot& snapshot);
    CommandResult validate(Command command) const;
    CommandResult execute(Command command);
    RoadBatchValidation validate_road_batch(std::span<const Cell> cells) const;
    CommandResult execute_road_batch(std::span<const Cell> cells);
    void tick();
    std::optional<std::vector<Cell>> find_route() const;
    std::optional<std::vector<Cell>> find_route(Cell start,Cell goal) const;
    std::vector<BuildingEntrance> building_entrances(BuildingId id) const;
    std::optional<std::vector<Cell>> find_building_route(BuildingId source,BuildingId target) const;
    std::optional<Position> courier_position() const;
    const std::vector<Cell>& courier_path() const { return path_; }
    std::optional<Cell> workshop() const { return workshop_; }
    std::optional<Cell> warehouse() const { return warehouse_; }
    CourierPhase courier_phase() const { return phase_; }
    const char* blockage();
    std::uint64_t ticks() const { return ticks_; }
    std::uint64_t command_sequence() const { return command_sequence_; }
    std::uint64_t road_revision() const { return road_revision_; }
    std::uint64_t route_refresh_count() const { return route_refresh_count_; }
    std::size_t route_cache_entries() const;
    std::uint64_t roads_placed_total() const { return roads_placed_total_; }
    std::uint64_t roads_removed_total() const { return roads_removed_total_; }
    bool navigation_valid() const;
    std::uint64_t total_produced() const { return total_produced_; }
    int workshop_stock() const { return workshop_stock_; }
    int production_progress() const { return production_progress_; }
    int courier_cargo() const { return courier_cargo_; }
    int warehouse_stock() const { return warehouse_stock_; }
    bool goods_balance_valid() const;
private:
    World(int width,int height,std::vector<std::uint8_t> buildable,
          RulesProfile profile,std::uint32_t rule_version,
          std::shared_ptr<const MapPermissions> permissions,bool original_development=false,
          std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
          std::shared_ptr<const OriginalTerrainFacts> terrain=nullptr,
          std::shared_ptr<const LandTradeBoundaryFacts> trade_boundary=nullptr);
    void update_health();
    void update_fire();
    CommandResult execute_impl(Command command,bool refresh_after,bool influence_prepared=false);
    CommandResult validate_original_influence_command(Command command) const;
    CommandResult execute_original_influence_command(Command command,bool refresh_after);
    bool original_influence_finance_valid() const;
    void rebuild_original_influence_cache();
    int original_influence_value_at(Cell origin,int side=2) const;
    std::size_t index(Cell cell) const;
    bool permission_edge_allowed(Cell from,Cell to) const;
    BuildBlocker ramp_land_blocker(Cell cell) const;
    void restore_ramps(const WorldSnapshot& snapshot);
    SourceCellRampValidation validate_source_cell_ramp_geometry(Cell source,Cell high,bool require_roads) const;
    void index_source_cell_ramp(const SourceCellRamp& ramp);
    void restore_source_cell_ramps(const WorldSnapshot& snapshot);
    void index_ramp(const RampLink& link);
    void restore_bridges(const WorldSnapshot& snapshot);
    void index_bridge(const BridgeSpan& span);
    static World restore_impl(const WorldSnapshot& snapshot,std::vector<std::uint8_t> buildable,
                              std::shared_ptr<const MapPermissions> permissions,
                              bool original_development=false,
                              std::shared_ptr<const original::OriginalRulesModel> model=nullptr,
                              std::shared_ptr<const OriginalTerrainFacts> terrain=nullptr,
          std::shared_ptr<const LandTradeBoundaryFacts> trade_boundary=nullptr);
    std::optional<std::vector<Cell>> find_road_route(Cell start,Cell goal) const;
    std::optional<std::vector<Cell>> find_route_to_building(Cell start,BuildingId target) const;
    const std::vector<Cell>* route_for_revision();
    void refresh_routes();
    void tick_production_v2();
    void advance_clay_pottery_production();
    void tick_original_production();
    void preflight_original_fire_tick() const;
    void record_original_fire_dispatch(const CourierState& courier);
    void record_original_fire_arrival(const CourierState& courier);
    void record_original_fire_step(const CourierState& courier);
    void validate_original_industry_model() const;
    static void validate_original_industry_snapshot(const WorldSnapshot& snapshot);
    void validate_original_industry_state() const;
    void update_original_industry();
    CommandResult validate_original_industry_command(Command command) const;
    bool original_auxiliary_courier(CourierRole role) const;
    CourierDispatchDecision original_auxiliary_dispatch_status(const CourierState& courier) const;
    void dispatch_original_auxiliary(CourierState& courier);
    void arrive_original_auxiliary(CourierState& courier);
    void cancel_original_auxiliary(BuildingId owner);
    bool original_clay_dispatch_available(CourierId id) const;
    void record_original_clay_dispatch(const CourierState& courier);
    void record_original_clay_arrival(const CourierState& courier);
    bool original_academy_paper_available(BuildingId provider) const;
    void consume_original_academy_paper(BuildingId provider);
    void consume_original_house_goods();
    void validate_original_consumption_model() const;
    static void validate_original_consumption_snapshot(const WorldSnapshot& snapshot);
    void admit_original_residents();
    bool original_production_finance_valid() const;
    int original_ceramics_stock(BuildingId owner) const;
    int original_typed_stock(BuildingId owner,original::Goods good) const;
    void receive_original_ceramics(BuildingId warehouse,int amount);
    std::array<OriginalGoodsBalance,original::goods_count> original_production_goods_balances() const;
    bool original_market_courier(CourierRole role) const;
    int original_market_headroom(BuildingId owner,original::Goods good=original::Goods::Ceramics) const;
    std::optional<original::Goods> original_market_dispatch_good(const CourierState& courier,BuildingId target) const;
    CourierDispatchDecision original_market_dispatch_status(const CourierState& courier) const;
    std::pair<CourierDispatchDecision,std::optional<original::Goods>> original_needs_market_selection(const CourierState&) const;
    void dispatch_original_market(CourierState& courier);
    void arrive_original_market(CourierState& courier);
    void cancel_original_market(BuildingId owner);
    static void validate_original_market_snapshot(const WorldSnapshot& snapshot);
    void validate_original_market_state() const;
    CommandResult validate_original_hemp_command(Command command) const;
    void advance_original_hemp();
    void validate_original_hemp_model() const;
    static void validate_original_hemp_snapshot(const WorldSnapshot& snapshot);
    CommandResult validate_original_food_command(Command command) const;
    void validate_original_food_model() const;
    static void validate_original_food_snapshot(const WorldSnapshot& snapshot);
    void validate_original_food_state() const;
    void advance_original_food();
    void consume_original_food();
    bool original_food_courier(CourierRole role) const;
    int original_food_stock(BuildingId owner) const;
    int original_food_headroom(BuildingId owner) const;
    CourierDispatchDecision original_food_dispatch_status(const CourierState& courier) const;
    void dispatch_original_food(CourierState& courier);
    void arrive_original_food(CourierState& courier);
    void cancel_original_food(BuildingId owner);
    void validate_original_services_model() const;
    static void validate_original_services_snapshot(const WorldSnapshot& snapshot);
    void validate_original_services_state() const;
    bool original_service_courier(CourierRole role) const;
    CourierDispatchDecision original_service_dispatch_status(const CourierState& courier) const;
    void dispatch_original_service(CourierState& courier);
    void arrive_original_service(CourierState& courier);
    void decay_original_services();
    void validate_original_music_venues_state() const;
    void validate_original_music_venues_model() const;
    CourierDispatchDecision original_music_dispatch_status(const CourierState&) const;
    void dispatch_original_music_performer(CourierState&);
    void arrive_original_music_performer(CourierState&);
    void decay_original_music_venues();
    void validate_original_venue_house_service_state() const;
    void validate_original_venue_house_service_model() const;
    bool original_venue_music_available(BuildingId) const;
    void validate_original_water_model() const;
    static void validate_original_water_snapshot(const WorldSnapshot&);
    void validate_original_water_state() const;
    bool original_water_courier(CourierRole) const;
    CourierDispatchDecision original_water_dispatch_status(const CourierState&) const;
    void dispatch_original_water(CourierState&);
    void arrive_original_water(CourierState&);
    void evaluate_original_water();
    void decay_original_water();
    static void validate_original_water_evaluation_provenance(const WorldSnapshot&,BuildingId,std::uint64_t,
        const std::optional<OriginalWaterWitness>&,bool);
    void validate_original_water_evaluation_provenance(BuildingId,std::uint64_t,
        const std::optional<OriginalWaterWitness>&,bool) const;
    static void validate_original_housing_snapshot(const WorldSnapshot&);
    void validate_original_housing_model();
    void validate_original_housing_state() const;
    void evaluate_original_houses();
    static void validate_original_finance_snapshot(const WorldSnapshot&);
    void validate_original_finance_model() const;
    bool original_finance_history_valid() const;
    void book_original_finance_tick(std::uint64_t next);
    std::int64_t original_worker_upkeep(BuildingId id) const;
    std::int64_t household_demand_tax(const BuildingState& home,std::uint64_t resulting_count) const;
    HouseholdDemandStatus household_demand_status(
        const BuildingState& home,std::uint64_t evaluation_tick) const;
    void preflight_maintenance_tick() const;
    void bill_maintenance();
    bool maintenance_economy_valid() const;
    void capture_tick_staffing();
    std::vector<bool> workforce_allocation() const;
    bool building_staffed_for_tick(BuildingId id) const;
    bool industry_balance_valid() const;
    CommandResult validate_land_trade_command(Command) const;
    CommandResult execute_land_trade_command(Command);
    bool land_trade_state_valid() const;
    void arrive_land_trader(CourierState&);
    bool land_trade_protected_building(BuildingId) const;

    void dispatch_v2(CourierState& courier);
    void move_v2(CourierState& courier);
    bool valid_return_path(const CourierState& courier) const;
    void mark_route_pending(CourierState& courier);
    void reroute_v2(CourierState& courier);
    BuildingState& mutable_building(BuildingId id);
    CourierState& mutable_courier(CourierId id);
    DemolitionStatus original_safe_demolition_status(BuildingId id) const;
    CommandResult execute_original_safe_demolition(Command command,bool refresh_after);
    void move_courier();
    int width_;
    int height_;
    RulesProfile profile_;
    std::uint32_t rule_version_=1;
    std::vector<std::uint8_t> buildable_;
    std::shared_ptr<const MapPermissions> permissions_;
    std::optional<OriginalEconomyState> original_economy_;
    std::optional<OriginalInfluenceState> original_influence_;
    std::optional<OriginalGoodsState> original_goods_;
    std::optional<OriginalProductionState> original_production_;
    std::optional<AuthoredLandTradeState> land_trade_;
    std::shared_ptr<const LandTradeBoundaryFacts> land_trade_boundary_;
    std::shared_ptr<const OriginalInfluenceCache> original_influence_cache_;
    std::uint64_t original_influence_cache_build_count_=0;
    std::shared_ptr<const original::OriginalRulesModel> original_rules_model_;
    std::shared_ptr<const OriginalTerrainFacts> original_terrain_facts_;
    std::vector<Object> objects_;
    std::vector<std::uint32_t> owners_;
    std::vector<BuildingState> buildings_;
    std::vector<CourierState> couriers_;
    std::vector<BridgeSpan> bridges_;
    std::uint32_t next_bridge_id_=1;
    std::vector<RampLink> ramps_;
    std::uint32_t next_ramp_id_=1;
    std::vector<std::uint8_t> ramp_index_; // Rule6-only derived endpoint lookup, never persisted.
    std::vector<SourceCellRamp> source_cell_ramps_;
    std::uint32_t next_source_cell_ramp_id_=1;
    std::vector<std::uint8_t> source_cell_ramp_index_; // Three bounded axial claims per link; derived only.
    std::vector<std::uint8_t> bridge_index_,bridge_bank_index_; // Rule5-only derived lookups, never persisted.
    std::uint32_t next_household_id_=first_household_id;
    std::uint32_t next_production_id_=8;
    std::uint32_t next_courier_id_=4;
    std::uint32_t next_building_id_=1;
    std::optional<BuildingId> last_dispatched_household_;
    std::optional<BuildingId> last_dispatched_food_household_;
    std::optional<BuildingId> last_dispatched_service_household_;
    std::array<std::optional<std::vector<Cell>>,household_limit> household_routes_{};
    std::uint64_t household_routes_revision_=UINT64_MAX;
    std::array<std::optional<std::vector<Cell>>,household_limit> food_household_routes_{};
    std::uint64_t food_routes_revision_=UINT64_MAX;
    std::array<std::optional<std::vector<Cell>>,household_limit> service_household_routes_{};
    std::uint64_t service_routes_revision_=UINT64_MAX;
    std::uint64_t clay_extracted_total_=0;
    std::uint64_t pottery_completed_total_=0;
    std::int64_t treasury_=0;
    std::uint64_t taxes_collected_total_=0;
    std::uint64_t construction_spent_total_=0;
    std::uint64_t maintenance_spent_total_=0;
    std::uint64_t food_produced_total_=0;
    DemolitionHistory demolition_history_;
    std::vector<bool> tick_staffed_;
    bool tick_staffing_active_=false;
    std::optional<Cell> workshop_;
    std::optional<Cell> warehouse_;
    std::uint64_t ticks_=0;
    std::uint64_t command_sequence_=0;
    std::uint64_t road_revision_=0;
    std::uint64_t route_refresh_count_=0;
    std::uint64_t roads_placed_total_=0;
    std::uint64_t roads_removed_total_=0;
    std::uint64_t cached_revision_=UINT64_MAX;
    std::optional<std::vector<Cell>> cached_route_;
    std::uint64_t total_produced_=0;
    int workshop_stock_=0;
    int production_progress_=0;
    int courier_cargo_=0;
    int warehouse_stock_=0;
    CourierPhase phase_=CourierPhase::IdleAtWorkshop;
    std::vector<Cell> path_;
    std::size_t path_vertex_=0;
    int edge_progress_=0;
    performance::WorldCopyProbe performance_copy_probe_;
};

// Application adapter only: explicit fixed ticks, bounded catch-up, no wall clock.
class TickDriver {
public:
    void update(double frame_seconds,World& world);
    void toggle_pause();
    void pause_and_reset() { paused_=true; accumulator_=0; }
    void set_speed(int speed);
    void step_once(World& world);
    bool paused() const { return paused_; }
    int speed() const { return speed_; }
    double interpolation() const { return accumulator_ * Rules::ticks_per_second; }
private:
    double accumulator_=0;
    bool paused_=false;
    int speed_=1;
};

} // namespace openemperor::simulation
