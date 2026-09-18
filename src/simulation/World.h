#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::simulation {

inline constexpr const char* profile_name = "sandbox-logistics-v1";
struct Rules {
    static constexpr int ticks_per_second = 20;
    static constexpr int production_ticks = 100;
    static constexpr int workshop_capacity = 8;
    static constexpr int warehouse_capacity = 32;
    static constexpr int courier_capacity = 4;
    static constexpr int edge_ticks = 10;
};

struct Cell {
    int x=0;
    int y=0;
    bool operator==(const Cell&) const = default;
};
enum class Object : std::uint8_t { Empty, Road, Workshop, Warehouse };
enum class CommandType { PlaceRoad, PlaceWorkshop, PlaceWarehouse };
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
struct Position { double x=0; double y=0; };

class World {
public:
    World(int width,int height,std::vector<std::uint8_t> buildable);
    int width() const { return width_; }
    int height() const { return height_; }
    bool in_bounds(Cell cell) const;
    bool buildable(Cell cell) const;
    Object object_at(Cell cell) const;
    CommandResult validate(Command command) const;
    CommandResult execute(Command command);
    void tick();
    std::optional<std::vector<Cell>> find_route() const;
    std::optional<Position> courier_position() const;
    const std::vector<Cell>& courier_path() const { return path_; }
    std::optional<Cell> workshop() const { return workshop_; }
    std::optional<Cell> warehouse() const { return warehouse_; }
    CourierPhase courier_phase() const { return phase_; }
    const char* blockage();
    std::uint64_t ticks() const { return ticks_; }
    std::uint64_t command_sequence() const { return command_sequence_; }
    std::uint64_t road_revision() const { return road_revision_; }
    std::uint64_t total_produced() const { return total_produced_; }
    int workshop_stock() const { return workshop_stock_; }
    int production_progress() const { return production_progress_; }
    int courier_cargo() const { return courier_cargo_; }
    int warehouse_stock() const { return warehouse_stock_; }
    bool goods_balance_valid() const;
private:
    std::size_t index(Cell cell) const;
    const std::vector<Cell>* route_for_revision();
    void move_courier();
    int width_;
    int height_;
    std::vector<std::uint8_t> buildable_;
    std::vector<Object> objects_;
    std::optional<Cell> workshop_;
    std::optional<Cell> warehouse_;
    std::uint64_t ticks_=0;
    std::uint64_t command_sequence_=0;
    std::uint64_t road_revision_=0;
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
