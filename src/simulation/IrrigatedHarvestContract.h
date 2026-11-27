#pragma once
#include <cstdint>

namespace openemperor::simulation {
// Admission is based on retained actual work, never potential irrigation time.
// Inventory, physical Grower and seasonal histories retain their existing owners.
inline constexpr const char* irrigated_harvest_contract="authored-irrigated-harvest-v1";
inline constexpr std::uint32_t irrigated_harvest_revision=1;
} // namespace openemperor::simulation
