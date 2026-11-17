#pragma once

#include "simulation/MapPermissions.h"
#include "simulation/OriginalGoodsState.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::simulation {

// Explicit Rule28 AUTHORED ecology/portion/time contract. None of these
// portions reinterpret Original raw100/400 or native callback counters.
inline constexpr std::size_t fishing_quay_limit=2;
inline constexpr std::size_t fishing_site_limit=16;
inline constexpr int fishing_site_capacity=8;
inline constexpr int fishing_boat_capacity=4;
inline constexpr std::uint16_t fishing_catch_ticks=40;
inline constexpr std::uint16_t fishing_unload_ticks=20;
inline constexpr std::uint16_t fishing_regeneration_ticks=240;
inline constexpr std::uint8_t fishing_edge_ticks=5;

struct FishingBoatId {
    std::uint64_t value=0;
    auto operator<=>(const FishingBoatId&) const = default;
};
enum class FishingBoatPhase : std::uint8_t { Outbound, Fishing, Returning, Unloading };
const char* fishing_boat_phase_name(FishingBoatPhase);
enum class FishingMovementResult : std::uint8_t { Moving, Arrived, Retry, Failed };
const char* fishing_movement_result_name(FishingMovementResult);
struct FishingSite {
    Cell cell{};
    int stock=fishing_site_capacity;
    std::uint16_t regeneration_progress=0;
    std::uint64_t regenerated=0,caught=0;
    bool source_marker=false;
    bool operator==(const FishingSite&) const = default;
};
struct FishingQuay {
    BuildingId quay_id=static_cast<BuildingId>(0);
    std::uint8_t orientation=0;
    // Historical dock receipts, never a second physical Food inventory.
    std::uint64_t unloaded=0;
    std::optional<FishingBoatId> boat;
    bool operator==(const FishingQuay&) const = default;
};
struct FishingBoat {
    FishingBoatId id{};
    BuildingId quay_id=static_cast<BuildingId>(0);
    std::size_t site_index=0;
    FishingBoatPhase phase=FishingBoatPhase::Outbound;
    FishingMovementResult movement=FishingMovementResult::Moving;
    Cell current{};
    std::vector<Cell> path;
    std::size_t path_vertex=0;
    std::uint8_t edge_progress=0;
    std::uint16_t work_progress=0;
    int cargo=0,reserved=0;
    std::uint64_t route_revision=0,launched_tick=0;
    bool return_requested=false;
    bool operator==(const FishingBoat&) const = default;
};
struct FishingState {
    std::vector<FishingQuay> quays;
    std::vector<FishingBoat> boats;
    std::vector<FishingSite> sites;
    std::uint64_t next_boat_id=1,launched=0,retired=0;
    bool operator==(const FishingState&) const = default;
};

} // namespace openemperor::simulation
