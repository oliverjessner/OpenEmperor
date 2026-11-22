#pragma once
#include "simulation/OriginalIndustryState.h"
#include "simulation/IrrigationProductivityState.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace openemperor::simulation {
inline constexpr const char* agriculture_reconfiguration_contract="authored-agriculture-reconfiguration-v1";
inline constexpr std::uint32_t agriculture_reconfiguration_revision=1;
inline constexpr std::size_t agriculture_reconfiguration_limit=8;
// Closed provenance only: physical work, receipts and inventories retain their
// existing typed owners. A retired paid leaf remains in the lifetime body budget.
struct AgricultureDrainWork {
    IrrigationWorkWitness work;
    std::uint64_t harvested=0;
    bool operator==(const AgricultureDrainWork&) const = default;
};
struct AgricultureReconfigurationEntry {
    BuildingId building_id{};
    std::uint64_t started_tick=0,started_sequence=0;
    std::optional<std::uint64_t> removed_tick,removed_sequence;
    std::optional<AgricultureDrainWork> initial_work;
    bool operator==(const AgricultureReconfigurationEntry&) const = default;
};
struct AgricultureReconfigurationState {
    std::vector<AgricultureReconfigurationEntry> entries;
    bool operator==(const AgricultureReconfigurationState&) const = default;
};
inline const AgricultureReconfigurationEntry* agriculture_reconfiguration_entry(const AgricultureReconfigurationState* state,BuildingId id) {
    if(state) for(const auto& entry:state->entries) if(entry.building_id==id) return &entry;
    return nullptr;
}
inline std::uint64_t agriculture_retirement_tick(const AgricultureReconfigurationState* state,BuildingId id,std::uint64_t now) {
    const auto* entry=agriculture_reconfiguration_entry(state,id);
    return entry && entry->removed_tick ? *entry->removed_tick:now;
}
} // namespace openemperor::simulation
