#pragma once

#include "maps/EmperorContainer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace openemperor::maps {

// A saved Emperor entity identity, never a simulation::BuildingId or an asset ID.
struct OriginalEntityId {
    std::uint32_t value = 0;
    bool operator==(const OriginalEntityId&) const = default;
};

constexpr std::uint64_t original_entities_logical_offset = 1093607;
constexpr std::uint32_t maximum_original_entity_records = 4000;

class OriginalEntityError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class OriginalEntityClass { Building, Monument, Fill, Industrial, Ferry, GateHouse, Tower };
const char* original_entity_class_name(OriginalEntityClass entity_class);

enum class OriginalEntityField {
    Status, FootprintSide, LocalX, LocalY, SerializedCellReference,
    Type, Subindex, OriginalId, MonumentPhase, SerializedMaterial,
    MonumentHeight, MonumentOrientation, GateLayout
};

struct OriginalEntityFieldSource {
    std::uint64_t logical_offset = 0;
    std::uint32_t record_relative_offset = 0;
    std::uint8_t byte_width = 0;
    bool signed_value = false;
};

struct OriginalEntityRecordProvenance {
    std::uint64_t logical_record_offset = 0; // Starts at the MFC object/class tag.
    std::uint64_t logical_base_offset = 0;   // Starts at the Building schema u16.
    std::optional<std::uint64_t> logical_extended_offset;
    std::uint32_t record_byte_length = 0;
    std::uint32_t mfc_object_reference = 0;
    std::uint16_t base_schema = 0;
    std::uint16_t wrapper_schema = 0;
    std::uint16_t extended_schema = 0;
    // Class-specific wrapper after the existing NonHouse state, where present.
    std::optional<std::uint64_t> logical_class_wrapper_offset;
    std::uint16_t class_wrapper_schema = 0;
};

struct OriginalMonumentState {
    std::int32_t phase = 0;
    // The original loader overwrites this using external mission/player goals.
    // This raw saved value therefore does not establish the restored material.
    std::int32_t serialized_material = 0;
    std::int32_t height = 0;
    std::uint8_t orientation = 0;
};

struct OriginalGateHouseState {
    // Raw Building +80 layout selector, independent of the attached units,
    // historical graphics and sandbox buildings. Active GateHouse admits 0/1.
    std::int32_t layout = 0;
};

struct OriginalEntityRecord {
    std::uint32_t manager_index = 0;
    OriginalEntityClass entity_class = OriginalEntityClass::Building;
    std::uint8_t status = 0;
    std::uint8_t footprint_side = 0;
    std::int16_t local_x = 0;
    std::int16_t local_y = 0;
    std::int32_t serialized_cell_reference = 0;
    std::int16_t type = 0;
    std::int16_t subindex = 0;
    std::int32_t serialized_original_id = -1;
    std::optional<OriginalEntityId> original_id;
    std::optional<OriginalMonumentState> monument;
    std::optional<OriginalGateHouseState> gate_house;
    OriginalEntityRecordProvenance provenance;

    bool active() const { return status != 0; }
    std::optional<OriginalEntityFieldSource> field_source(OriginalEntityField field) const;
};

struct OriginalMapEntities {
    std::uint64_t logical_offset = original_entities_logical_offset;
    std::uint64_t byte_length = 0; // The manager only; subsequent map data is not consumed.
    std::uint32_t declared_map_size = 0;
    std::uint16_t manager_schema = 0;
    std::vector<OriginalEntityRecord> records;
};

// Pure bounded parser of a manager prefix. All fields are little-endian. Unknown
// classes/schemas, references, counts, active IDs/coordinates and truncation fail
// with OriginalEntityError; no partially parsed collection is returned. Active
// monument types 253..268 additionally admit only phase 0..11, raw material
// 0..3 and orientation 0..3. Active GateHouse admits layout 0/1. Other monument
// types and inactive fields retain uninterpreted state.
OriginalMapEntities parse_original_map_entities(
    std::span<const std::uint8_t> bytes, std::uint32_t declared_map_size,
    std::uint64_t logical_offset = original_entities_logical_offset);

// Reads only a bounded range in a supported standalone-map part.
OriginalMapEntities read_original_map_entities(const EmperorContainer& container,
                                               std::size_t part);

} // namespace openemperor::maps
