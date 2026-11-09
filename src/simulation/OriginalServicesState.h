#pragma once

#include "simulation/OriginalGoodsState.h"
#include "simulation/OriginalCultureState.h"
#include "simulation/original/ServiceRules.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace openemperor::simulation {

// Explicit Rule10 purchase bindings; old generic Posts acquire no identity.
enum class OriginalServiceProviderType : std::uint8_t { Herbalist207=0, Acupuncture208=1, Ancestor214=2, Daoist215=3, Buddhist217=4, Confucian219=5, Music211=6, Acrobat212=7, Drama213=8, Theatre75=9 };
inline constexpr std::size_t original_service_provider_limit=18;
inline constexpr std::size_t original_service_trip_limit=18;
inline constexpr std::size_t original_service_witness_limit=180;
struct OriginalServiceProvider {
    BuildingId building_id=static_cast<BuildingId>(0);
    OriginalServiceProviderType type=OriginalServiceProviderType::Herbalist207;
    bool operator==(const OriginalServiceProvider&) const = default;
};
// Latest real trip per owned cargo-free courier, including sequence highwater.
struct OriginalServiceTrip {
    OriginalGoodsTransferKey transfer;
    BuildingId target=static_cast<BuildingId>(0);
    std::uint64_t dispatched_tick=0;
    std::optional<std::uint64_t> arrival_tick=std::nullopt;
    bool visit_applied=false;
    std::optional<OriginalVenueWalkerEvidence> venue_walker=std::nullopt;
    bool operator==(const OriginalServiceTrip&) const = default;
};
// Last successful visit per House/category. HouseState owns the only timer.
struct OriginalServiceWitness {
    BuildingId building_id=static_cast<BuildingId>(0);
    original::ServiceRole role=original::ServiceRole::Herbalist;
    BuildingId provider_id=static_cast<BuildingId>(0);
    OriginalGoodsTransferKey transfer;
    std::uint64_t arrival_tick=0;
    std::optional<OriginalVenueWalkerEvidence> venue_walker=std::nullopt;
    bool operator==(const OriginalServiceWitness&) const = default;
};
// Derived on-demand inspection only. Never serialized or used to grant visits.
struct OriginalServiceAvailability {
    bool eligible_target=false;
    std::uint8_t delivered_timer=0;
    std::size_t provider_count=0,available_source_count=0,reachable_source_count=0;
    std::size_t travelling_to_target_count=0;
    std::optional<std::uint64_t> last_arrival_tick;
    bool operator==(const OriginalServiceAvailability&) const = default;
};
struct OriginalServicesState {
    std::vector<OriginalServiceProvider> providers;
    std::vector<OriginalServiceTrip> trips;
    std::vector<OriginalServiceWitness> witnesses;
    bool operator==(const OriginalServicesState&) const = default;
};

} // namespace openemperor::simulation
