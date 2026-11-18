#pragma once

#include "simulation/MapPermissions.h"
#include "simulation/OriginalGoodsState.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::simulation {

// Explicit Rule29 AUTHORED ecology, movement and portion contract. Original
// identities do not establish these clocks or convert raw50/100/400 to Food.
inline constexpr std::size_t hunting_tent_limit=2;
inline constexpr std::size_t hunting_habitat_limit=4;
inline constexpr int hunting_habitat_capacity=3;
inline constexpr int hunting_habitat_radius=3;
inline constexpr int hunting_hunter_capacity=1;
inline constexpr std::uint16_t hunting_work_ticks=60;
inline constexpr std::uint16_t hunting_unload_ticks=20;
inline constexpr std::uint16_t hunting_regeneration_ticks=720;
inline constexpr std::uint16_t hunting_roam_ticks=40;
inline constexpr std::uint8_t hunting_edge_ticks=5;

struct HuntingAnimalId {
    std::uint64_t value=0;
    auto operator<=>(const HuntingAnimalId&) const = default;
};
struct HuntingHunterId {
    std::uint64_t value=0;
    auto operator<=>(const HuntingHunterId&) const = default;
};
enum class HuntingSpecies : std::uint8_t { WildPig75=75, Pheasant76=76, Antelope77=77 };
const char* hunting_species_name(HuntingSpecies);
enum class HuntingHunterPhase : std::uint8_t { Pursuing, Hunting, Returning, Unloading };
const char* hunting_hunter_phase_name(HuntingHunterPhase);
enum class HuntingMovementResult : std::uint8_t { Moving, Arrived, Retry, Failed };
const char* hunting_movement_result_name(HuntingMovementResult);

struct HuntingHabitat {
    Cell centre{};
    HuntingSpecies species=HuntingSpecies::WildPig75;
    std::uint16_t regeneration_progress=0;
    std::uint64_t births=0,killed=0;
    bool source_marker=false;
    bool operator==(const HuntingHabitat&) const = default;
};
struct HuntingAnimal {
    HuntingAnimalId id{};
    std::size_t habitat_index=0;
    HuntingSpecies species=HuntingSpecies::WildPig75;
    Cell current{};
    std::optional<Cell> next;
    std::uint8_t edge_progress=0;
    std::uint64_t born_tick=0;
    std::optional<HuntingHunterId> reserved_by;
    bool operator==(const HuntingAnimal&) const = default;
};
struct HuntingTent {
    BuildingId tent_id=static_cast<BuildingId>(0);
    // Historical real Tent receipts; physical stock lives only in Food<7>.
    std::uint64_t unloaded=0;
    std::optional<HuntingHunterId> hunter;
    bool operator==(const HuntingTent&) const = default;
};
struct HuntingHunter {
    static constexpr std::uint8_t actor_type=40;
    HuntingHunterId id{};
    BuildingId tent_id=static_cast<BuildingId>(0);
    std::optional<HuntingAnimalId> target;
    Cell target_cell{};
    std::size_t habitat_index=0;
    HuntingHunterPhase phase=HuntingHunterPhase::Pursuing;
    HuntingMovementResult movement=HuntingMovementResult::Moving;
    Cell current{};
    std::vector<Cell> path;
    std::size_t path_vertex=0;
    std::uint8_t edge_progress=0;
    std::uint16_t work_progress=0;
    int cargo=0,reserved=0;
    std::uint64_t route_revision=0,launched_tick=0;
    std::optional<Cell> kill_cell;
    std::optional<std::uint64_t> killed_tick;
    bool return_requested=false;
    bool operator==(const HuntingHunter&) const = default;
};
struct HuntingState {
    std::vector<HuntingTent> tents;
    std::vector<HuntingHunter> hunters;
    std::vector<HuntingHabitat> habitats;
    std::vector<HuntingAnimal> animals;
    std::uint64_t next_animal_id=1,next_hunter_id=1,launched=0,retired=0;
    bool operator==(const HuntingState&) const = default;
};

} // namespace openemperor::simulation
