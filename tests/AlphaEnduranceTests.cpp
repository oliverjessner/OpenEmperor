#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace sim = openemperor::simulation;
namespace save = openemperor::persistence;

constexpr int width = 20;
constexpr int height = 8;
constexpr std::uint64_t tick_limit = 100'000;

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

std::vector<std::uint8_t> buildable() {
    return std::vector<std::uint8_t>(static_cast<std::size_t>(width * height), 1);
}

void require_command(sim::World& world, sim::CommandType type, int x, int y) {
    const auto result = world.execute({type, {x, y}});
    check(result.accepted, "initial command rejected at " + std::to_string(x) + "," +
        std::to_string(y) + ": " + result.reason);
}

sim::World make_world() {
    sim::World world(width, height, buildable(), sim::RulesProfile::IndustryV5);
    const auto road = [&](int x, int y) { require_command(world, sim::CommandType::PlaceRoad, x, y); };
    require_command(world, sim::CommandType::PlaceClaySource, 0, 3);
    road(1, 3); road(2, 3);
    require_command(world, sim::CommandType::PlacePottery, 3, 3);
    road(4, 3); road(5, 3);
    require_command(world, sim::CommandType::PlaceWarehouse, 6, 3);
    road(7, 3); road(8, 3);
    require_command(world, sim::CommandType::PlaceHousehold, 9, 3);
    road(7, 2); road(8, 2);
    require_command(world, sim::CommandType::PlaceHousehold, 8, 1);
    road(9, 2); road(10, 2);
    require_command(world, sim::CommandType::PlaceHousehold, 10, 1);
    road(7, 4); road(8, 4); road(8, 5);
    require_command(world, sim::CommandType::PlaceHousehold, 9, 5);

    require_command(world, sim::CommandType::PlaceClaySource, 0, 5);
    road(1, 5); road(2, 5); road(2, 4); road(1, 4);
    require_command(world, sim::CommandType::PlacePottery, 3, 5);
    road(4, 5); road(5, 5); road(6, 5); road(6, 4); road(5, 4);
    return world;
}

sim::World make_city_world() {
    sim::World world(width,height,buildable(),sim::RulesProfile::CityV6);
    const auto road=[&](int x,int y) { require_command(world,sim::CommandType::PlaceRoad,x,y); };
    require_command(world,sim::CommandType::PlaceClaySource,0,2);
    road(1,2); road(2,2);
    require_command(world,sim::CommandType::PlacePottery,3,2);
    road(4,2); road(5,2);
    require_command(world,sim::CommandType::PlaceWarehouse,6,2);
    road(7,2); road(8,2); road(9,2);
    require_command(world,sim::CommandType::PlaceHousehold,10,2);
    road(7,1); road(8,1);
    require_command(world,sim::CommandType::PlaceHousehold,9,1);
    while (world.treasury()<482 && world.ticks()<20'000) world.tick();
    check(world.treasury()>=482,"City endurance starter did not earn expansion funds");
    require_command(world,sim::CommandType::PlaceClaySource,0,4);
    road(1,4); road(2,4); road(2,3);
    require_command(world,sim::CommandType::PlacePottery,3,4);
    road(4,4); road(5,4); road(6,4); road(6,3);
    road(7,3); road(8,3);
    require_command(world,sim::CommandType::PlaceHousehold,9,3);
    road(7,4); road(8,4);
    require_command(world,sim::CommandType::PlaceHousehold,9,4);
    check(world.city_economy_valid() && world.production_balance_valid() &&
          world.navigation_valid(),"expanded City endurance world invalid");
    return world;
}

struct ScheduledCommand {
    std::uint64_t before_tick;
    sim::Command command;
};

const std::array schedule{
    ScheduledCommand{2'000, {sim::CommandType::RemoveRoad, {1, 4}}},
    ScheduledCommand{2'400, {sim::CommandType::PlaceRoad, {1, 4}}},
    ScheduledCommand{5'000, {sim::CommandType::RemoveRoad, {2, 4}}},
    ScheduledCommand{5'400, {sim::CommandType::PlaceRoad, {2, 4}}},
    ScheduledCommand{12'000, {sim::CommandType::RemoveRoad, {8, 2}}},
    ScheduledCommand{12'400, {sim::CommandType::PlaceRoad, {8, 2}}},
    ScheduledCommand{20'000, {sim::CommandType::RemoveRoad, {6, 4}}},
    ScheduledCommand{20'400, {sim::CommandType::PlaceRoad, {6, 4}}},
    ScheduledCommand{30'000, {sim::CommandType::RemoveRoad, {5, 4}}},
    ScheduledCommand{30'001, {sim::CommandType::RemoveRoad, {6, 4}}},
    ScheduledCommand{30'500, {sim::CommandType::PlaceRoad, {5, 4}}},
    ScheduledCommand{30'501, {sim::CommandType::PlaceRoad, {6, 4}}},
    ScheduledCommand{60'000, {sim::CommandType::RemoveRoad, {7, 3}}},
    ScheduledCommand{60'300, {sim::CommandType::PlaceRoad, {7, 3}}},
    ScheduledCommand{80'000, {sim::CommandType::RemoveRoad, {4, 3}}},
    ScheduledCommand{80'250, {sim::CommandType::PlaceRoad, {4, 3}}},
};

constexpr std::array checkpoints{1ULL, 99ULL, 100ULL, 149ULL, 150ULL, 399ULL,
    400ULL, 999ULL, 5'000ULL, 20'000ULL, 50'000ULL, 99'999ULL};

void validate_world(const sim::World& world) {
    check(world.profile() == sim::RulesProfile::IndustryV5, "rules profile changed");
    check(world.production_balance_valid(), "production conservation failed");
    check(world.navigation_valid(), "navigation invariant failed");
    const auto snapshot = world.snapshot();
    std::array<int, 9> reservations{};
    for (std::size_t index = 0; index < snapshot.buildings.size(); ++index) {
        const auto& building = snapshot.buildings[index];
        check(static_cast<unsigned>(building.id) == index + 1U, "building ID changed");
        check(building.input_clay >= 0 && building.output >= 0 && building.pottery_stock >= 0 &&
              building.reserved_incoming >= 0 && building.progress >= 0 &&
              building.active_recipe_clay >= 0 && building.demand_progress >= 0,
              "negative building inventory or progress");
        if (!building.placed) continue;
        check(world.in_bounds(building.cell), "placed building outside world");
        if (building.kind == sim::Object::ClaySource)
            check(building.output <= sim::Rules::clay_output_capacity &&
                  building.progress <= sim::Rules::clay_ticks, "ClaySource capacity exceeded");
        if (building.kind == sim::Object::Pottery)
            check(building.input_clay + building.reserved_incoming <= sim::Rules::pottery_input_capacity &&
                  building.output <= sim::Rules::pottery_output_capacity &&
                  building.progress <= sim::Rules::pottery_recipe_ticks &&
                  (building.active_recipe_clay == 0 ||
                   building.active_recipe_clay == sim::Rules::pottery_recipe_clay),
                  "Pottery capacity or recipe state invalid");
        if (building.kind == sim::Object::Warehouse)
            check(building.pottery_stock + building.reserved_incoming <=
                  sim::Rules::warehouse_capacity, "warehouse capacity exceeded");
        if (building.kind == sim::Object::Household)
            check(building.pottery_stock + building.reserved_incoming <=
                  sim::Rules::household_capacity &&
                  building.demand_progress <= sim::Rules::household_demand_ticks,
                  "household capacity or demand clock invalid");
    }
    for (std::size_t index = 0; index < snapshot.couriers.size(); ++index) {
        const auto& courier = snapshot.couriers[index];
        check(static_cast<unsigned>(courier.id) == index + 1U, "courier ID changed");
        check(courier.cargo >= 0 && courier.reserved >= 0 &&
              courier.edge_progress >= 0 && courier.edge_progress < sim::Rules::edge_ticks,
              "courier cargo, reservation or edge progress invalid");
        check(courier.path.empty() ? courier.path_vertex == 0 : courier.path_vertex < courier.path.size(),
              "courier path cursor outside route");
        for (std::size_t step = 0; step < courier.path.size(); ++step) {
            check(world.in_bounds(courier.path[step]), "courier path outside world");
            if (step != 0) {
                const auto dx = std::abs(courier.path[step].x - courier.path[step - 1].x);
                const auto dy = std::abs(courier.path[step].y - courier.path[step - 1].y);
                check(dx + dy == 1, "courier path contains non-adjacent cells");
            }
        }
        if (courier.enabled) {
            const auto owner = static_cast<std::size_t>(static_cast<unsigned>(courier.owner) - 1U);
            const auto target = static_cast<std::size_t>(static_cast<unsigned>(courier.target) - 1U);
            check(owner < snapshot.buildings.size() && snapshot.buildings[owner].placed,
                  "courier owner invalid");
            check(target < snapshot.buildings.size() && snapshot.buildings[target].placed,
                  "courier target invalid");
            if (courier.phase == sim::CourierPhase::ToWarehouse) {
                check(courier.reserved == courier.cargo && courier.cargo > 0,
                      "outbound courier reservation does not match cargo");
                reservations[target] += courier.reserved;
            }
        }
        if (const auto position = world.courier_position(courier.id))
            check(std::isfinite(position->x) && std::isfinite(position->y) &&
                  position->x >= 0.0 && position->y >= 0.0 &&
                  position->x < world.width() && position->y < world.height(),
                  "courier position is non-finite or outside world");
    }
    for (std::size_t i = 0; i < reservations.size(); ++i)
        check(snapshot.buildings[i].reserved_incoming == reservations[i],
              "building reservation does not equal active couriers");
}

bool same_result(const sim::CommandResult& a, const sim::CommandResult& b) {
    return a.accepted == b.accepted && a.changed == b.changed && a.sequence == b.sequence &&
        a.tick == b.tick && std::string(a.reason) == b.reason;
}

std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct TemporaryRoot {
    fs::path path = fs::canonical(fs::temp_directory_path()) /
        ("openemperor-alpha-endurance-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryRoot() {
        fs::create_directories(path / "data/Cities");
        fs::create_directories(path / "saves");
        std::ofstream output(path / "data/Cities/Synthetic.map", std::ios::binary);
        output << "independently authored alpha endurance map identity";
    }
    ~TemporaryRoot() { std::error_code error; fs::remove_all(path, error); }
};

} // namespace

int main() {
    try {
        TemporaryRoot temporary;
        const fs::path map_relative = "Cities/Synthetic.map";
        const auto mask = buildable();
        auto world_a = make_world();
        auto world_b = make_world();
        check(world_a.snapshot() == world_b.snapshot(), "initial worlds differ");
        std::optional<sim::World> snapshot_branch;
        std::optional<sim::World> json_branch;
        std::map<std::string, bool> representative{{"idle", false}, {"loaded_transit", false},
            {"edge_progress", false}, {"route_pending", false}, {"household_delivery", false},
            {"active_recipe", false}, {"multiple_reservations", false}};
        std::size_t schedule_index = 0;
        std::size_t checkpoint_count = 0;
        std::size_t json_roundtrips = 0;
        std::size_t accepted_topology_changes = 0;
        const auto save_a = temporary.path / "saves/a.json";
        const auto save_b = temporary.path / "saves/b.json";

        const auto roundtrip = [&](const sim::World& world) {
            const auto document = save::make_document(temporary.path / "data", map_relative, mask, world);
            save::write_save(save_a, document, temporary.path / "data", mask);
            save::write_save(save_b, document, temporary.path / "data", mask);
            check(bytes(save_a) == bytes(save_b), "identical snapshot produced different save bytes");
            auto restored = save::restore_save(save::read_save(save_a), temporary.path / "data", mask);
            check(restored.snapshot() == world.snapshot(), "JSON roundtrip changed snapshot");
            ++json_roundtrips;
            return restored;
        };

        for (std::uint64_t next_tick = 1; next_tick <= tick_limit; ++next_tick) {
            while (schedule_index < schedule.size() &&
                   schedule[schedule_index].before_tick == next_tick) {
                const auto& command = schedule[schedule_index].command;
                const auto a = world_a.execute(command);
                const auto b = world_b.execute(command);
                check(same_result(a, b), "scheduled command result diverged");
                if (snapshot_branch) {
                    const auto result = snapshot_branch->execute(command);
                    check(same_result(a, result), "snapshot branch command result diverged");
                }
                if (json_branch) {
                    const auto result = json_branch->execute(command);
                    check(same_result(a, result), "JSON branch command result diverged");
                }
                if (a.accepted && a.changed) ++accepted_topology_changes;
                ++schedule_index;
            }
            world_a.tick();
            world_b.tick();
            if (snapshot_branch) snapshot_branch->tick();
            if (json_branch) json_branch->tick();
            check(world_a.snapshot() == world_b.snapshot(), "parallel worlds diverged at tick " +
                std::to_string(next_tick));
            if (snapshot_branch && next_tick % 20 == 0)
                check(snapshot_branch->snapshot() == world_a.snapshot(),
                      "snapshot branch diverged at tick " + std::to_string(next_tick));
            if (json_branch && next_tick % 20 == 0)
                check(json_branch->snapshot() == world_a.snapshot(),
                      "JSON branch diverged at tick " + std::to_string(next_tick));
            if (next_tick % 25 == 0 || next_tick == 1) validate_world(world_a);

            const auto snapshot = world_a.snapshot();
            bool multiple = false;
            std::array<int, 9> active_reservations{};
            for (const auto& courier : snapshot.couriers) {
                representative["idle"] = representative["idle"] ||
                    (courier.enabled && courier.phase == sim::CourierPhase::IdleAtWorkshop);
                representative["loaded_transit"] = representative["loaded_transit"] ||
                    (courier.phase == sim::CourierPhase::ToWarehouse && courier.cargo > 0);
                representative["edge_progress"] = representative["edge_progress"] ||
                    courier.edge_progress > 0;
                representative["route_pending"] = representative["route_pending"] ||
                    courier.route_pending;
                if (courier.phase == sim::CourierPhase::ToWarehouse) {
                    const auto target = static_cast<std::size_t>(static_cast<unsigned>(courier.target) - 1U);
                    if (++active_reservations[target] > 1) multiple = true;
                }
            }
            representative["multiple_reservations"] = representative["multiple_reservations"] || multiple;
            for (const auto& building : snapshot.buildings) {
                representative["active_recipe"] = representative["active_recipe"] ||
                    building.active_recipe_clay > 0;
                representative["household_delivery"] = representative["household_delivery"] ||
                    (building.kind == sim::Object::Household && building.pottery_stock > 0);
            }

            if (std::find(checkpoints.begin(), checkpoints.end(), next_tick) != checkpoints.end()) {
                snapshot_branch.emplace(sim::World::restore(snapshot, mask));
                check(snapshot_branch->snapshot() == snapshot, "snapshot restore changed state");
                json_branch.emplace(roundtrip(world_a));
                ++checkpoint_count;
            }
        }
        validate_world(world_a);
        check(schedule_index == schedule.size() && accepted_topology_changes >= 10,
              "topology schedule was not fully exercised");
        for (const auto& [name, observed] : representative)
            check(observed, "representative save state not observed: " + name);
        check(world_a.pottery_completed_total() > 0 && world_a.clay_extracted_total() > 0,
              "long run did not produce goods");
        unsigned supplied_households = 0;
        for (unsigned id = 4; id <= 7; ++id)
            if (world_a.building(static_cast<sim::BuildingId>(id)).consumed_total > 0)
                ++supplied_households;
        check(supplied_households == 4, "not all four households consumed pottery");

        auto city_a=make_city_world();
        auto city_b=city_a;
        const auto city_start_tick=city_a.ticks();
        for (std::uint64_t tick=0;tick<tick_limit;++tick) {
            city_a.tick(); city_b.tick();
            if (tick%25==0) {
                check(city_a.snapshot()==city_b.snapshot(),"City endurance worlds diverged");
                check(city_a.city_economy_valid() && city_a.production_balance_valid() &&
                      city_a.navigation_valid(),"City endurance invariant failed");
            }
        }
        check(city_a.ticks()-city_start_tick==tick_limit && city_a.settlement_goal_reached(),
              "City endurance did not complete 100k ticks or retain its goal");

        nlohmann::json states = nlohmann::json::object();
        for (const auto& [name, observed] : representative) states[name] = observed;
        std::cout << nlohmann::json{{"schema", "openemperor-alpha-endurance-v1"},
            {"ticks", world_a.ticks()}, {"parallel_worlds_equal", true},
            {"checkpoint_count", checkpoint_count}, {"json_roundtrips", json_roundtrips},
            {"deterministic_save_bytes", true},
            {"accepted_topology_changes", accepted_topology_changes},
            {"representative_states", states}, {"production_balance_valid", true},
            {"navigation_valid", true}, {"supplied_households", supplied_households},
            {"clay_extracted_total", world_a.clay_extracted_total()},
            {"pottery_completed_total", world_a.pottery_completed_total()},
            {"city_ticks", tick_limit}, {"city_goal_reached",city_a.settlement_goal_reached()},
            {"city_funds",city_a.treasury()},
            {"city_taxes_collected_total",city_a.taxes_collected_total()},
            {"city_economy_valid",city_a.city_economy_valid()},
            {"result", "pass"}}.dump() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "alpha endurance failed: " << error.what() << '\n';
        return 1;
    }
}
