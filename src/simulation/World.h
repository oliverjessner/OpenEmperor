#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace openemperor::simulation {

inline constexpr const char* profile_name = "sandbox-logistics-v1";
inline constexpr const char* production_profile_name = "sandbox-production-v2";
enum class RulesProfile {
    LogisticsV1, ProductionV2, HouseholdV3, SettlementV4, IndustryV5, CityV6, CityV7, CityV8,
    CityV9, CityV10
};
inline constexpr const char* household_profile_name = "sandbox-household-v3";
inline constexpr const char* settlement_profile_name = "sandbox-settlement-v4";
inline constexpr const char* industry_profile_name = "sandbox-industry-v5";
inline constexpr const char* city_profile_name = "sandbox-city-v6";
inline constexpr const char* city_v7_profile_name = "sandbox-city-v7";
inline constexpr const char* city_v8_profile_name = "sandbox-city-v8";
inline constexpr const char* city_v9_profile_name = "sandbox-city-v9";
inline constexpr const char* city_v10_profile_name = "sandbox-city-v10";
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
           profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10;
}
constexpr bool production_profile(RulesProfile profile) {
    return profile==RulesProfile::ProductionV2 || household_profile(profile);
}
constexpr bool industry_profile(RulesProfile profile) {
    return profile==RulesProfile::IndustryV5 || profile==RulesProfile::CityV6 ||
           profile==RulesProfile::CityV7 || profile==RulesProfile::CityV8 ||
           profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10;
}
constexpr bool city_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV6 || profile==RulesProfile::CityV7 ||
           profile==RulesProfile::CityV8 || profile==RulesProfile::CityV9 ||
           profile==RulesProfile::CityV10;
}
constexpr bool food_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV7 || profile==RulesProfile::CityV8 ||
           profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10;
}
constexpr bool service_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV8 || profile==RulesProfile::CityV9 ||
           profile==RulesProfile::CityV10;
}
constexpr bool population_profile(RulesProfile profile) {
    return profile==RulesProfile::CityV9 || profile==RulesProfile::CityV10;
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
};

struct Cell {
    int x=0;
    int y=0;
    bool operator==(const Cell&) const = default;
};
enum class Object : std::uint8_t {
    Empty, Road, Workshop, Warehouse, ClaySource, Pottery, Household, Farm, ServicePost
};
struct BuildingFootprint {
    int width=1;
    int height=1;
    bool operator==(const BuildingFootprint&) const = default;
};
BuildingFootprint building_footprint(RulesProfile profile,Object kind);
std::vector<Cell> building_footprint_cells(RulesProfile profile,Object kind,Cell origin);
Cell building_front_cell(RulesProfile profile,Object kind,Cell origin);
bool building_footprint_contains(RulesProfile profile,Object kind,Cell origin,Cell cell);
struct BuildingEntrance {
    Cell road_cell{};
    Cell building_cell{};
    bool operator==(const BuildingEntrance&) const = default;
};
enum class CommandType { PlaceRoad, PlaceWorkshop, PlaceWarehouse, PlaceClaySource, PlacePottery,
                         RemoveRoad, PlaceHousehold, PlaceFarm, PlaceServicePost };
struct Command { CommandType type; Cell cell; };
struct CommandResult {
    bool accepted=false;
    bool changed=false;
    const char* reason="";
    std::uint64_t sequence=0;
    std::uint64_t tick=0;
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
    None=0, Clay=1, Pottery=2, Household=3, Food=4, Service=5
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
    Disabled
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
    int pottery_stock=0; // Warehouse only.
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
    bool operator==(const BuildingState&) const = default;
};
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
    bool operator==(const BuildingSnapshot&) const = default;
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
    bool operator==(const WorldSnapshot&) const = default;
};
const char* delivery_phase_name(CourierPhase phase);

class World {
public:
    World(int width,int height,std::vector<std::uint8_t> buildable,
          RulesProfile profile=RulesProfile::LogisticsV1);
    RulesProfile profile() const { return profile_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool in_bounds(Cell cell) const;
    bool buildable(Cell cell) const;
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
    std::uint64_t food_produced_total() const { return food_produced_total_; }
    int workforce_supply() const;
    int workforce_required() const;
    int workforce_used() const;
    int workforce_required(BuildingId id) const;
    bool building_staffed(BuildingId id) const;
    bool settlement_goal_reached() const;
    int settlement_goal_households_ready() const;
    bool city_economy_valid() const;
    bool food_balance_valid() const;
    bool service_state_valid() const;
    int household_level(BuildingId id) const;
    bool household_service_active(BuildingId id) const;
    std::uint64_t household_service_remaining(BuildingId id) const;
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
    std::string canonical_state() const;
    WorldSnapshot snapshot() const;
    static World restore(const WorldSnapshot& snapshot,std::vector<std::uint8_t> buildable);
    void import_snapshot(const WorldSnapshot& snapshot);
    CommandResult validate(Command command) const;
    CommandResult execute(Command command);
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
    std::size_t index(Cell cell) const;
    std::optional<std::vector<Cell>> find_road_route(Cell start,Cell goal) const;
    std::optional<std::vector<Cell>> find_route_to_building(Cell start,BuildingId target) const;
    const std::vector<Cell>* route_for_revision();
    void refresh_routes();
    void tick_production_v2();
    void capture_tick_staffing();
    bool building_staffed_for_tick(BuildingId id) const;
    bool industry_balance_valid() const;
    void dispatch_v2(CourierState& courier);
    void move_v2(CourierState& courier);
    bool valid_return_path(const CourierState& courier) const;
    void mark_route_pending(CourierState& courier);
    void reroute_v2(CourierState& courier);
    BuildingState& mutable_building(BuildingId id);
    CourierState& mutable_courier(CourierId id);
    void move_courier();
    int width_;
    int height_;
    RulesProfile profile_;
    std::vector<std::uint8_t> buildable_;
    std::vector<Object> objects_;
    std::vector<std::uint32_t> owners_;
    std::vector<BuildingState> buildings_;
    std::vector<CourierState> couriers_;
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
    std::uint64_t food_produced_total_=0;
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
