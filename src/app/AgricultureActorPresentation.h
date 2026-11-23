#pragma once

#include "simulation/World.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace openemperor {

enum class AgricultureActorKind : std::uint8_t {
    FoodGrower, HempGrower, PlantationGrower, FoodCourier,
    Boat, Hunter, WildPig, Pheasant, Antelope
};
enum class AgricultureActorAction : std::uint8_t {
    Standing, Walking, Waiting, Working, Returning, Unloading
};
// AUTHORED display only. Phase is an actual saved edge/work counter modulo4;
// neither frame time nor World tick advances a stationary actor's pose.
// Copy this value into the hit submitted with its drawn silhouette.
struct AgricultureActorPose {
    AgricultureActorKind kind=AgricultureActorKind::FoodGrower;
    AgricultureActorAction action=AgricultureActorAction::Standing;
    std::int8_t dx=0,dy=1;
    std::uint8_t phase=0;
    bool moving=false,working=false;
    int cargo=0;
    bool operator==(const AgricultureActorPose&) const=default;
};
enum class AgricultureActorDomain : std::uint8_t { Courier, Boat, Hunter, Animal };
struct AgricultureActorVisual {
    AgricultureActorDomain domain=AgricultureActorDomain::Courier;
    std::uint64_t id=0,lifetime_id=0;
    simulation::BuildingId owner=static_cast<simulation::BuildingId>(0);
    std::optional<simulation::BuildingId> target;
    simulation::Position position{};
    AgricultureActorPose pose{};
    bool operator==(const AgricultureActorVisual&) const=default;
};

// Preparation-only bounded read of real World actors. Call after command/tick
// or restore, never from render/hover. It owns no World, routes or inventory.
// Identical saved states produce identical poses, including a fresh F9 cache.
class AgricultureActorPresentationCache {
public:
    static constexpr std::size_t capacity=simulation::Rules::original_production_courier_limit;
    void prepare(const simulation::World&);
    void clear() noexcept { count_=0; }
    std::span<const AgricultureActorVisual> entries() const noexcept { return {entries_.data(),count_}; }
    const AgricultureActorVisual* find_courier(simulation::CourierId) const noexcept;
    const AgricultureActorVisual* find_boat(simulation::FishingBoatId) const noexcept;
    const AgricultureActorVisual* find_hunter(simulation::HuntingHunterId) const noexcept;
    const AgricultureActorVisual* find_animal(simulation::HuntingAnimalId) const noexcept;
private:
    const AgricultureActorVisual* find(AgricultureActorDomain,std::uint64_t) const noexcept;
    std::array<AgricultureActorVisual,capacity> entries_{};
    std::size_t count_=0;
};

} // namespace openemperor
