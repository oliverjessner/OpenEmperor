#pragma once

#include "maps/DirectGraphicCandidate.h"

#include <cstdint>
#include <map>

namespace openemperor::maps {

// Version-specific diagnostic hypothesis, not a proven map-layer interpretation.
// The examined PE's generic graphic lookup divides nonnegative IDs by 16384.
enum class GraphicsIdStatus { UnsupportedHighBit, UnregisteredSlot, IndexOutOfRange,
                              EmptyRecord, SourceUnavailable, UnsupportedLayout, DecodeCandidate };
const char* graphics_id_status_name(GraphicsIdStatus status);

struct GraphicsIdResolution {
    std::uint32_t raw = 0;
    std::uint32_t slot = 0;
    std::uint32_t local_index = 0;
    GraphicsIdStatus status = GraphicsIdStatus::UnregisteredSlot;
    const assets::AssetRecord* record = nullptr;
};

// Callers provide an explicit registration snapshot. No filesystem-order or
// implicit archive fallback is allowed. Catalogs must outlive the result.
GraphicsIdResolution resolve_graphics_id_hypothesis(
    std::uint32_t raw, const std::map<std::uint32_t, const assets::AssetCatalog*>& registrations);

} // namespace openemperor::maps
