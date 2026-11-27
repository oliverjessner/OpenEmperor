#pragma once

#include "simulation/World.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace openemperor::agriculture_visual {

// AUTHORED presentation vocabulary. These are observations of the current paid
// World, never a crop clock, asset selector, inventory or saved authority.
enum class Phase : std::uint8_t {
    Initial, Growing, Ready, Harvesting, Picked, Regrowing, Resting,
    Idle, Processing, Distributing, Waiting, Returning
};

struct State {
    simulation::BuildingId id{},source{};
    simulation::Object kind{};
    simulation::Cell cell{};
    Phase phase=Phase::Idle;
    std::uint32_t progress=0,target=0;
    std::uint8_t harvest_progress=0,harvest_target=0;
    bool wet=false,working=false,blocked=false;
    bool operating=false,burning=false,staffed=false,draining=false;
    int workers=0,required_workers=0;
    int input=0,output=0,incoming=0,return_reserved=0;
    // Actual associated physical worker only. Cargo remains on that worker;
    // these diagnostic numbers must never be added to an owner's stock.
    std::optional<simulation::CourierId> worker;
    std::uint64_t worker_lifetime=0;
    int cargo=0;
    bool returning=false;
    std::string reason;
    bool operator==(const State&) const=default;
};

struct Cache {
    std::array<State,simulation::Rules::original_production_building_limit> rows{};
    std::size_t count=0;
    std::uint64_t tick=0,sequence=0;
    const State* find(simulation::BuildingId) const noexcept;
    bool operator==(const Cache&) const=default;
};

using Staffing=std::span<const std::pair<simulation::BuildingId,int>>;

// Call only at World publication/tick/command/load boundaries. Production may
// reuse its existing workers_assigned cache for this exact World epoch. Missing
// entries delegate once per unique source to the existing public World query.
// Explicit Rules36/37/38 only; other generations return an empty cache. Retired
// bodies are absent. Drawing/hovering uses find() and these copied observations.
Cache prepare(const simulation::World&,Staffing staffing={});
const char* phase_name(Phase) noexcept;

} // namespace openemperor::agriculture_visual
