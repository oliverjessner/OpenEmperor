#pragma once

#include "simulation/MapPermissions.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::simulation {
enum class BuildingId : std::uint32_t;

// Explicit non-production acceptance policy. One World tick drives one native
// movement microstep; this authored binding is not an Actor26 speed grant.
enum class GrowerNavigationPolicy : std::uint8_t { Mode17MicrostepV1=1 };
enum class GrowerNavigationResult : std::uint8_t {
    Moving=0,Arrived=8,Retry=9,Failed=10,Unsupported=11
};
enum class GrowerNavigationInteraction : std::uint8_t {
    None,PeerState5,Suspension13c,Ferry48,Linked66,Exceptional14,
    Exceptional15,Exceptional16,GlobalGate88ec38
};
struct GrowerNavigationId {
    std::uint64_t value=0;
    auto operator<=>(const GrowerNavigationId&) const = default;
};
struct GrowerNavigationRequest {
    GrowerNavigationPolicy policy=GrowerNavigationPolicy::Mode17MicrostepV1;
    BuildingId farm_id{},field_id{};
    Cell start{},target{};
    bool returning=false;
    GrowerNavigationInteraction interaction=GrowerNavigationInteraction::None;
    bool operator==(const GrowerNavigationRequest&) const = default;
};
// Session navigation state only: no Actor26 identity, field claim, work,
// resource, reservation or saved authority is created by this track.
struct GrowerNavigationTrack {
    GrowerNavigationId id{};
    GrowerNavigationRequest request;
    Cell current{};
    std::vector<Cell> path;
    // Count of directions accepted at phase20, not completed edges. During
    // an edge, path[cursor-1] -> path[cursor] is the begun segment.
    std::size_t cursor=0;
    std::uint8_t phase=20;
    bool edge_started=false;
    bool unsupported_context_seen=false;
    // An autonomous acceptance lifecycle owns and reuses this one channel;
    // the ordinary explicit-session wrapper must not advance it a second time.
    bool lifecycle_owned=false;
    GrowerNavigationResult result=GrowerNavigationResult::Moving;
    std::uint64_t route_revision=0,microsteps=0,reroutes=0;
    std::string blocker;
    bool operator==(const GrowerNavigationTrack&) const = default;
};
struct GrowerNavigationAdmission {
    bool accepted=false;
    std::optional<GrowerNavigationId> id;
    GrowerNavigationResult result=GrowerNavigationResult::Failed;
    std::string reason;
};
} // namespace openemperor::simulation
