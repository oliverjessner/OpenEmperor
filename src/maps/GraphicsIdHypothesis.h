#pragma once

#include "maps/DirectGraphicCandidate.h"

#include <cstdint>
#include <map>
#include <optional>

namespace openemperor::maps {

// Version-specific diagnostic hypothesis, not a proven map-layer interpretation.
// The examined PE's generic graphic lookup divides nonnegative IDs by 16384.
enum class GraphicsIdStatus { UnsupportedHighBit, UnregisteredSlot, UnverifiedRegistration,
                              IndexOutOfRange,
                              EmptyRecord, SourceUnavailable, UnsupportedLayout, DecodeCandidate };
const char* graphics_id_status_name(GraphicsIdStatus status);

struct GraphicsIdResolution {
    std::uint32_t raw = 0;
    std::uint32_t slot = 0;
    std::uint32_t local_index = 0;
    std::optional<std::uint32_t> physical_record_index;
    GraphicsIdStatus status = GraphicsIdStatus::UnregisteredSlot;
    const assets::AssetRecord* record = nullptr;
};

// Observed for the examined EXE's v213 Terrain/Elevation registration only:
// it skips physical record zero and copies reported_images_in_use records.
// A catalog contains physical records, including the dummy and reserved tail.
struct GraphicsArchiveRegistration {
    const assets::AssetCatalog* catalog = nullptr;
    std::uint32_t sg3_version = 0;
    std::uint32_t image_capacity = 0;
    std::uint32_t reported_images_in_use = 0;
};

// Callers provide an explicit registration snapshot. No filesystem-order or
// implicit archive fallback is allowed. Catalogs must outlive the result.
GraphicsIdResolution resolve_graphics_id_hypothesis(
    std::uint32_t raw, const std::map<std::uint32_t, GraphicsArchiveRegistration>& registrations);

} // namespace openemperor::maps
