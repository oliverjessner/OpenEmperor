#pragma once

#include "maps/ResourceGroupLookup.h"
#include "maps/MapGeometry.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace openemperor::maps {
// Storage directions, not screen directions. No saved graphic or road input.
struct WaterNeighborhood {
    bool n=false, ne=false, e=false, se=false, s=false, sw=false, w=false, nw=false;
    std::uint8_t mask() const;
};
WaterNeighborhood terrain_neighborhood(std::span<const std::uint32_t> terrain,
    GridCell cell, std::uint32_t mask);
struct WaterMatch {
    std::uint8_t row=0, orientation_offset=0, variant_count=0, variant=0;
    bool operator==(const WaterMatch&) const = default;
};
// First matching semantic row; orientation is 0..3. The water caller matches
// absent-water neighbours (4bc100), not the water bits themselves.
std::optional<WaterMatch> match_water(WaterNeighborhood water,
    unsigned orientation, std::uint8_t variation);
enum class LandscapeFamily { Ground, Water, Decoration, Rock, Mountain, Wall, GreatWall, Preserved };
enum class SelectorEvidence { Verified, Preview, Unresolved };
struct LandscapeSelection {
    LandscapeFamily family=LandscapeFamily::Preserved;
    SelectorEvidence evidence=SelectorEvidence::Unresolved;
    const char* selector="unreconstructed";
    const char* reason="selector not reconstructed";
    ResourceGroupKey group{};
    unsigned variant=0;
    std::optional<WaterMatch> water_match;
    bool operator==(const LandscapeSelection& rhs) const;
};
struct LandscapeSelectorInput {
    std::span<const std::uint32_t> terrain, objects;
    std::span<const std::uint8_t> variation, fertility;
    // Original transient 425250 context is not a saved object word. Unknown
    // inputs stay explicit; there is no map-name or historic-ID inference.
    std::optional<bool> flood_context;
    unsigned orientation=0;
};
LandscapeSelection select_landscape(const LandscapeSelectorInput& input, GridCell cell);
const char* selector_evidence_name(SelectorEvidence evidence);
const char* landscape_family_name(LandscapeFamily family);
} // namespace openemperor::maps
