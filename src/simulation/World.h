#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::simulation {

inline constexpr const char* profile_name = "sandbox-logistics-v1";
inline constexpr const char* production_profile_name = "sandbox-production-v2";
enum class RulesProfile { LogisticsV1, ProductionV2 };
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
};

struct Cell {
    int x=0;
    int y=0;
    bool operator==(const Cell&) const = default;
};
enum class Object : std::uint8_t { Empty, Road, Workshop, Warehouse, ClaySource, Pottery };
enum class CommandType { PlaceRoad, PlaceWorkshop, PlaceWarehouse, PlaceClaySource, PlacePottery,
                         RemoveRoad };
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
enum class Good { Goods, Clay, Pottery };
enum class BuildingId : std::uint8_t { ClaySource=1, Pottery=2, Warehouse=3 };
enum class CourierId : std::uint8_t { Clay=1, Pottery=2 };
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
};
struct CourierState {
    CourierId id=CourierId::Clay;
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
};
// Only authoritative state. Occupancy and future route caches are rebuilt.
struct CourierSnapshot {
    CourierId id=CourierId::Clay;
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
    std::array<BuildingSnapshot,3> buildings{};
    std::array<CourierSnapshot,2> couriers{};
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
    const CourierState& courier(CourierId id) const;
    std::optional<Position> courier_position(CourierId id) const;
    const char* courier_blockage(CourierId id) const;
    const char* pottery_blockage() const;
    std::uint64_t clay_extracted_total() const { return clay_extracted_total_; }
    std::uint64_t pottery_completed_total() const { return pottery_completed_total_; }
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
    std::optional<Position> courier_position() const;
    const std::vector<Cell>& courier_path() const { return path_; }
    std::optional<Cell> workshop() const { return workshop_; }
    std::optional<Cell> warehouse() const { return warehouse_; }
    CourierPhase courier_phase() const { return phase_; }
    const char* blockage();
    std::uint64_t ticks() const { return ticks_; }
    std::uint64_t command_sequence() const { return command_sequence_; }
    std::uint64_t road_revision() const { return road_revision_; }
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
    const std::vector<Cell>* route_for_revision();
    void refresh_routes();
    void tick_production_v2();
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
    std::vector<std::uint8_t> owners_;
    std::array<BuildingState,3> buildings_;
    std::array<CourierState,2> couriers_;
    std::uint64_t clay_extracted_total_=0;
    std::uint64_t pottery_completed_total_=0;
    std::optional<Cell> workshop_;
    std::optional<Cell> warehouse_;
    std::uint64_t ticks_=0;
    std::uint64_t command_sequence_=0;
    std::uint64_t road_revision_=0;
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
