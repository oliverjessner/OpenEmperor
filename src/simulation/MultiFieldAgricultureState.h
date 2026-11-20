#pragma once

#include "simulation/AgricultureState.h"

namespace openemperor::simulation {

inline constexpr std::uint32_t multi_field_agriculture_revision=1;
inline constexpr const char* multi_field_agriculture_contract="authored-multi-field-agriculture-v1";
inline constexpr std::size_t multi_field_per_farm_limit=8;
inline constexpr std::uint64_t multi_field_work_per_portion=80;

// No Food stock lives here. The existing seven-kind account and each existing
// AgricultureField::harvested remain the sole inventory and crop-origin owners.
struct MultiFieldFarmSchedule {
    BuildingId farm_id{};
    std::optional<BuildingId> last_field;
    bool operator==(const MultiFieldFarmSchedule&) const = default;
};
struct MultiFieldWork {
    BuildingId field_id{},farm_id{};
    std::uint64_t productive_ticks=0;
    bool operator==(const MultiFieldWork&) const = default;
};
struct MultiFieldAgricultureState {
    std::vector<MultiFieldFarmSchedule> farms;
    std::vector<MultiFieldWork> fields;
    bool operator==(const MultiFieldAgricultureState&) const = default;
};

} // namespace openemperor::simulation
