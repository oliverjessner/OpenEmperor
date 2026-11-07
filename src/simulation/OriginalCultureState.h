#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::simulation {

enum class BuildingId : std::uint32_t;
enum class CourierId : std::uint32_t;
// Rule22 performer lifetimes are distinct from the fixed paid movement channel.
// An arrival retires this identity; a later spawn never reuses it.
enum class PerformerId : std::uint64_t {};
inline constexpr std::size_t original_music_school_limit=2;
inline constexpr std::size_t original_culture_venue_limit=2;

struct OriginalMusicTrip {
    PerformerId performer_id=static_cast<PerformerId>(0);
    BuildingId venue_id=static_cast<BuildingId>(0);
    std::uint64_t dispatched_tick=0;
    std::optional<std::uint64_t> arrival_tick=std::nullopt;
    bool accepted=false;
    bool operator==(const OriginalMusicTrip&) const = default;
};
struct OriginalMusicSchool {
    BuildingId building_id=static_cast<BuildingId>(0);
    CourierId courier_id=static_cast<CourierId>(0);
    std::uint8_t spawn_counter=0;
    // A due attempt can fail admission without creating a performer.
    std::uint64_t attempts=0,spawned=0;
    std::optional<OriginalMusicTrip> latest_trip=std::nullopt;
    bool operator==(const OriginalMusicSchool&) const = default;
};
struct OriginalMusicArrival {
    PerformerId performer_id=static_cast<PerformerId>(0);
    BuildingId school_id=static_cast<BuildingId>(0);
    CourierId courier_id=static_cast<CourierId>(0);
    std::uint64_t dispatched_tick=0,arrival_tick=0;
    bool operator==(const OriginalMusicArrival&) const = default;
};
struct OriginalCultureVenue {
    BuildingId building_id=static_cast<BuildingId>(0);
    // Sole venue timer. Its last real accepted arrival witnesses phase35 decay.
    // This is neither House96 nor a venue-owned service walker.
    std::uint8_t music_timer=0;
    std::optional<OriginalMusicArrival> last_arrival=std::nullopt;
    bool operator==(const OriginalCultureVenue&) const = default;
};
struct OriginalCultureState {
    std::uint64_t next_performer_id=1;
    std::vector<OriginalMusicSchool> schools;
    std::vector<OriginalCultureVenue> venues;
    bool operator==(const OriginalCultureState&) const = default;
};

} // namespace openemperor::simulation
