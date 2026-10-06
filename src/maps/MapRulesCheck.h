#pragma once

#include "simulation/World.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace openemperor::maps {
class EmperorContainer;

enum class MapRulesStatus { Unchecked, Checking, MapRulesChecked, UnsupportedForRules, InputError };

// Copy this immutable selection into a worker. Versions are explicit; zero is
// not shorthand for the current rule version. Legacy rules use policy zero.
struct MapRulesRequest {
    std::filesystem::path canonical_data_root, map_relative;
    simulation::RulesProfile profile=simulation::RulesProfile::CityV11;
    std::uint32_t rule_version=0, policy_version=0;
    bool operator==(const MapRulesRequest&) const = default;
};

struct MapRulesResult {
    MapRulesRequest request;
    MapRulesStatus status=MapRulesStatus::Unchecked;
    std::string reason, detail, input_sha256;
    std::uint64_t physical_bytes=0;
};

std::uint32_t map_rules_policy_version(simulation::RulesProfile profile,
                                       std::uint32_t rule_version);
// Hash exactly the immutable physical bytes owned by a loaded input context.
std::string map_input_sha256(const EmperorContainer& container);

// SDL-free, one selected map only. The content hash and parsers use the same
// retained container bytes. Success establishes original-map prerequisites,
// not decoded assets, legacy buildability, a starter or a working session.
// This function never creates MapPermissions, a World, textures or save files.
MapRulesResult check_map_rules(const MapRulesRequest& request);

} // namespace openemperor::maps
