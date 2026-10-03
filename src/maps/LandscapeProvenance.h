#pragma once

#include "maps/StoredGraphicsPlan.h"
#include <nlohmann/json_fwd.hpp>

namespace openemperor::maps {
// Pinned standalone-map serializer + signed-byte draw operand, not sprite height.
inline constexpr std::uint64_t landscape_draw_properties_offset = 677327;
inline constexpr std::uint64_t landscape_height_offset = 989275;
inline constexpr int landscape_height_step = 40;
void read_landscape_layers(StoredGraphicsPlan& plan, const EmperorContainer& container,
                           std::size_t part);
int landscape_height(const StoredGraphicsPlan& plan, GridCell cell);
scene::Point landscape_ground(const StoredGraphicsPlan& plan, GridCell cell);
std::optional<GridCell> pick_landscape_ground(const StoredGraphicsPlan& plan,
    scene::Point world, const MapGeometry& geometry, bool elevated);
nlohmann::json landscape_provenance(const StoredGraphicsPlan& plan, GridCell cell,
    bool elevated, const scene::Camera2D* camera = nullptr);
std::vector<std::string> landscape_inspection_lines(const StoredGraphicsPlan& plan,
    GridCell cell, bool elevated, const scene::Camera2D* camera = nullptr);
nlohmann::json landscape_fidelity_report(const StoredGraphicsPlan& plan);
} // namespace openemperor::maps
