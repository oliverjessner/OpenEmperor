#pragma once

#include "maps/StoredGraphicsPlan.h"
#include "maps/MapGeometry.h"
#include "maps/OriginalMapEntities.h"
#include "simulation/MapPermissions.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace openemperor::maps {

inline constexpr const char* sandbox_buildable_profile = "sandbox_buildable_v1";

// Conservative, diagnostic placement mask. Call after the stored renderer has
// updated decode statuses. Neither the map nor its plan is mutated here.
std::vector<std::uint8_t> make_sandbox_buildable_mask(const StoredGraphicsPlan& plan,
                                                      const MapGeometry& geometry);

// City-v16 rule 3 only. Road and fixed-passage authority comes from complete
// original raw data, never renderer readiness, saved graphics or alpha. The
// conservative legacy mask remains the upper bound for building placement.
std::shared_ptr<const simulation::MapPermissions> prepare_sandbox_map_permissions(
    const ParsedEmperorMap& map, const MapGeometry& geometry,
    std::span<const std::uint8_t> legacy_mask, const OriginalMapEntities& entities,
    std::span<const std::uint8_t> height_bytes,
    std::uint32_t policy_version = simulation::kMapPermissionsPolicyVersion);
std::shared_ptr<const simulation::MapPermissions> make_sandbox_map_permissions(
    const ParsedEmperorMap& map, const StoredGraphicsPlan& plan, const MapGeometry& geometry,
    std::span<const std::uint8_t> legacy_mask, const OriginalMapEntities& entities,
    std::uint32_t policy_version = simulation::kMapPermissionsPolicyVersion);
std::shared_ptr<const simulation::MapPermissions> load_sandbox_map_permissions(
    const ParsedEmperorMap& map, const StoredGraphicsPlan& plan, const MapGeometry& geometry,
    std::span<const std::uint8_t> legacy_mask,
    std::uint32_t policy_version = simulation::kMapPermissionsPolicyVersion);
// Persistence reconstructs the same immutable contract without a renderer or
// a presentation plan. Unknown policy/source data fails before publication.
std::shared_ptr<const simulation::MapPermissions> read_sandbox_map_permissions(
    const std::filesystem::path& data_root, const std::filesystem::path& map_relative,
    std::span<const std::uint8_t> legacy_mask,
    std::uint32_t policy_version = simulation::kMapPermissionsPolicyVersion);

} // namespace openemperor::maps
