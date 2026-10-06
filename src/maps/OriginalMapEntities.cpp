#include "maps/OriginalMapEntities.h"

#include "maps/EmperorMap.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <string>
#include <string_view>

namespace openemperor::maps {
namespace {

// EXE-OBSERVED in the pinned 6373328b...53c0e Emperor.exe. These are serialized
// widths, not sizeof the original runtime objects. Building::Serialize 427430
// loads schema 3 at 42856f, 4 at 428112, 5 at 427cc2; common ID tail is 4289bd.
constexpr std::uint32_t base_length(std::uint16_t schema) {
    switch (schema) {
    case 3: return 179;
    case 4: return 181;
    case 5: return 182;
    default: return 0;
    }
}
constexpr std::uint64_t maximum_manager_byte_length =
    6 + static_cast<std::uint64_t>(maximum_original_entity_records) * (6 + 63 + 182 + 2 + 128 + 2010);

class Reader {
public:
    Reader(std::span<const std::uint8_t> bytes, std::uint64_t logical_offset)
        : bytes_(bytes), logical_offset_(logical_offset) {}
    std::size_t position() const { return position_; }
    void record(std::uint32_t index) { record_ = index; }
    [[noreturn]] void fail(std::string_view reason, std::size_t at, bool unsupported=false) const {
        std::string message = "original entity manager";
        if (record_) message += " record " + std::to_string(*record_);
        message += " at logical " + std::to_string(logical_offset_ + at) + ": ";
        message += reason;
        if (unsupported) throw OriginalEntityUnsupported(message);
        throw OriginalEntityError(message);
    }
    [[noreturn]] void unsupported(std::string_view reason, std::size_t at) const {
        fail(reason,at,true);
    }
    void require(std::size_t count, std::string_view purpose) const {
        if (position_ > bytes_.size() || count > bytes_.size() - position_)
            fail(std::string("truncated ") + std::string(purpose), position_);
    }
    std::span<const std::uint8_t> take(std::size_t count, std::string_view purpose) {
        require(count, purpose);
        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        if (position_ > maximum_manager_byte_length) fail("manager exceeds bounded byte length", position_);
        return result;
    }
    std::uint16_t u16(std::string_view purpose) {
        const auto b = take(2, purpose);
        return static_cast<std::uint16_t>(b[0] | (static_cast<std::uint16_t>(b[1]) << 8));
    }
    std::int32_t i32(std::string_view purpose) {
        const auto b = take(4, purpose);
        return std::bit_cast<std::int32_t>(read_u32(b, 0));
    }
    static std::uint32_t read_u32(std::span<const std::uint8_t> b, std::size_t at) {
        return static_cast<std::uint32_t>(b[at]) |
               (static_cast<std::uint32_t>(b[at + 1]) << 8) |
               (static_cast<std::uint32_t>(b[at + 2]) << 16) |
               (static_cast<std::uint32_t>(b[at + 3]) << 24);
    }
private:
    std::span<const std::uint8_t> bytes_;
    std::uint64_t logical_offset_;
    std::size_t position_ = 0;
    std::optional<std::uint32_t> record_;
};

std::int16_t i16_at(std::span<const std::uint8_t> b, std::size_t at) {
    const auto value = static_cast<std::uint16_t>(b[at] | (static_cast<std::uint16_t>(b[at + 1]) << 8));
    return std::bit_cast<std::int16_t>(value);
}
std::int32_t i32_at(std::span<const std::uint8_t> b, std::size_t at) {
    return std::bit_cast<std::int32_t>(Reader::read_u32(b, at));
}
std::optional<OriginalEntityClass> known_class(std::string_view name) {
    if (name == "Building") return OriginalEntityClass::Building;
    if (name == "cMonumentBldg") return OriginalEntityClass::Monument;
    if (name == "cFillBldg") return OriginalEntityClass::Fill;
    if (name == "cIndustrialBldg") return OriginalEntityClass::Industrial;
    if (name == "cFerryBldg") return OriginalEntityClass::Ferry;
    if (name == "cGateHouse") return OriginalEntityClass::GateHouse;
    if (name == "cTower") return OriginalEntityClass::Tower;
    return std::nullopt;
}
enum class ReferenceKind { Null, Class, Object };
struct Reference {
    ReferenceKind kind = ReferenceKind::Null;
    OriginalEntityClass entity_class = OriginalEntityClass::Building;
};
void expect_schema(Reader& reader, std::uint16_t expected, std::string_view purpose) {
    const auto at = reader.position();
    const auto schema = reader.u16(purpose);
    if (schema != expected)
        reader.unsupported(std::string("unsupported ") + std::string(purpose) + " " + std::to_string(schema), at);
}

} // namespace

const char* original_entity_class_name(OriginalEntityClass entity_class) {
    switch (entity_class) {
    case OriginalEntityClass::Building: return "Building";
    case OriginalEntityClass::Monument: return "cMonumentBldg";
    case OriginalEntityClass::Fill: return "cFillBldg";
    case OriginalEntityClass::Industrial: return "cIndustrialBldg";
    case OriginalEntityClass::Ferry: return "cFerryBldg";
    case OriginalEntityClass::GateHouse: return "cGateHouse";
    case OriginalEntityClass::Tower: return "cTower";
    }
    return "unknown";
}

std::optional<OriginalEntityFieldSource> OriginalEntityRecord::field_source(OriginalEntityField field) const {
    std::uint64_t origin = provenance.logical_base_offset;
    std::uint32_t offset = 0;
    std::uint8_t width = 0;
    bool is_signed = false;
    // Runtime +04/+07/+0a/+0c/+10/+14/+16 become these stream offsets;
    // in particular runtime coordinate offsets are NOT serialized offsets.
    switch (field) {
    case OriginalEntityField::Status: offset = 2; width = 1; break;
    case OriginalEntityField::FootprintSide: offset = 5; width = 1; break;
    case OriginalEntityField::LocalX: offset = 8; width = 2; is_signed = true; break;
    case OriginalEntityField::LocalY: offset = 10; width = 2; is_signed = true; break;
    case OriginalEntityField::SerializedCellReference: offset = 12; width = 4; is_signed = true; break;
    case OriginalEntityField::Type: offset = 16; width = 2; is_signed = true; break;
    case OriginalEntityField::Subindex: offset = 18; width = 2; is_signed = true; break;
    case OriginalEntityField::OriginalId:
        if (base_length(provenance.base_schema) == 0) return std::nullopt;
        offset = base_length(provenance.base_schema) - 20; width = 4; is_signed = true; break;
    case OriginalEntityField::GateLayout:
        if (!gate_house || base_length(provenance.base_schema) == 0) return std::nullopt;
        // Building runtime +80: read4288df/428482/428025, serialized widths
        // summed independently for Base3/4/5. Schema5 adds one earlier byte.
        offset = provenance.base_schema == 5 ? 113 : 112;
        width = 4; is_signed = true; break;
    case OriginalEntityField::MonumentPhase:
    case OriginalEntityField::SerializedMaterial:
    case OriginalEntityField::MonumentHeight:
    case OriginalEntityField::MonumentOrientation:
        if (!monument || !provenance.logical_extended_offset) return std::nullopt;
        origin = *provenance.logical_extended_offset;
        if (field == OriginalEntityField::MonumentPhase) offset = 6;
        if (field == OriginalEntityField::SerializedMaterial) offset = 87;
        if (field == OriginalEntityField::MonumentHeight) offset = 38;
        if (field == OriginalEntityField::MonumentOrientation) offset = 123;
        width = field == OriginalEntityField::MonumentOrientation ? 1 : 4;
        is_signed = width == 4;
        break;
    }
    if (origin > std::numeric_limits<std::uint64_t>::max() - offset ||
        origin < provenance.logical_record_offset ||
        origin - provenance.logical_record_offset > std::numeric_limits<std::uint32_t>::max() - offset)
        return std::nullopt;
    return OriginalEntityFieldSource{origin + offset,
        static_cast<std::uint32_t>(origin - provenance.logical_record_offset) + offset, width, is_signed};
}

OriginalMapEntities parse_original_map_entities(std::span<const std::uint8_t> bytes,
                                               std::uint32_t declared_map_size,
                                               std::uint64_t logical_offset) {
    if (declared_map_size == 0 || declared_map_size > stored_grid_width)
        throw OriginalEntityError("original entity manager: invalid declared map size");
    if (bytes.size() > std::numeric_limits<std::uint64_t>::max() - logical_offset)
        throw OriginalEntityError("original entity manager: logical range overflow");
    Reader reader(bytes, logical_offset);
    OriginalMapEntities result;
    result.logical_offset = logical_offset;
    result.declared_map_size = declared_map_size;
    result.manager_schema = reader.u16("manager schema");
    if (result.manager_schema != 1) reader.unsupported("unsupported manager schema", 0);
    const auto count = reader.i32("record count");
    if (count < 0 || count > static_cast<std::int32_t>(maximum_original_entity_records))
        reader.fail("record count outside 0..4000", 2);
    result.records.reserve(static_cast<std::size_t>(count));
    std::vector<Reference> references(1); // MFC index zero is the null reference.
    references.reserve(static_cast<std::size_t>(count) * 2 + 1);
    std::array<bool, maximum_original_entity_records + 1> active_ids{};

    // 42d790 schema1 count/objects, 42d0e0 -> MFC77fd90/7802fe: class and object
    // references share one index table. Only fresh object records are admitted;
    // aliases/nulls/extended tags cannot establish a distinct manager record.
    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(count); ++index) {
        reader.record(index);
        const auto record_at = reader.position();
        const auto tag = reader.u16("MFC object/class tag");
        OriginalEntityClass entity_class;
        if (tag == 0xffff) {
            expect_schema(reader, 0, "MFC class schema");
            const auto length_at = reader.position();
            const auto name_length = reader.u16("MFC class name length");
            if (name_length == 0 || name_length >= 64) reader.fail("invalid MFC class name length", length_at);
            const auto name_bytes = reader.take(name_length, "MFC class name");
            const std::string_view name(reinterpret_cast<const char*>(name_bytes.data()), name_bytes.size());
            const auto recognized = known_class(name);
            if (!recognized) reader.unsupported(std::string("unsupported MFC class '") + std::string(name) + "'", record_at);
            entity_class = *recognized;
            references.push_back({ReferenceKind::Class, entity_class});
        } else if ((tag & 0x8000) != 0) {
            const auto reference = static_cast<std::size_t>(tag & 0x7fff);
            if (reference == 0 || reference >= references.size() ||
                references[reference].kind != ReferenceKind::Class)
                reader.fail("invalid MFC class reference", record_at);
            entity_class = references[reference].entity_class;
        } else {
            if (tag == 0x7fff) reader.fail("unsupported extended MFC reference", record_at);
            if (tag == 0 || tag >= references.size() || references[tag].kind != ReferenceKind::Object)
                reader.fail("invalid MFC object reference", record_at);
            reader.fail("aliased MFC object record is unsupported", record_at);
        }
        OriginalEntityRecord record;
        record.manager_index = index;
        record.entity_class = entity_class;
        record.provenance.logical_record_offset = logical_offset + record_at;
        record.provenance.mfc_object_reference = static_cast<std::uint32_t>(references.size());
        references.push_back({ReferenceKind::Object, entity_class});
        const auto base_at = reader.position();
        const auto schema = reader.u16("Building schema");
        const auto length = base_length(schema);
        if (length == 0) reader.unsupported("unsupported Building schema " + std::to_string(schema), base_at);
        // The schema has already been consumed; obtain the full proven base slice.
        reader.take(length - 2, "Building record");
        const auto base = bytes.subspan(base_at, length);
        record.provenance.logical_base_offset = logical_offset + base_at;
        record.provenance.base_schema = schema;
        record.status = base[2];
        record.footprint_side = base[5];
        record.local_x = i16_at(base, 8);
        record.local_y = i16_at(base, 10);
        record.serialized_cell_reference = i32_at(base, 12);
        record.type = i16_at(base, 16);
        record.subindex = i16_at(base, 18);
        record.serialized_original_id = i32_at(base, length - 20);
        if (entity_class == OriginalEntityClass::GateHouse)
            record.gate_house = OriginalGateHouseState{i32_at(base, schema == 5 ? 113 : 112)};
        if (record.active()) {
            if (record.serialized_original_id <= 0 ||
                record.serialized_original_id > static_cast<std::int32_t>(maximum_original_entity_records))
                reader.fail("active original ID outside 1..4000", base_at + length - 20);
            const auto id = static_cast<std::uint32_t>(record.serialized_original_id);
            if (active_ids[id]) reader.fail("duplicate active original ID", base_at + length - 20);
            active_ids[id] = true;
            record.original_id = OriginalEntityId{id};
            if (record.local_x < 0 || record.local_y < 0 ||
                record.local_x >= static_cast<std::int32_t>(declared_map_size) ||
                record.local_y >= static_cast<std::int32_t>(declared_map_size))
                reader.fail("active local coordinates outside declared map", base_at + 8);
            if (record.serialized_cell_reference < 0 ||
                record.serialized_cell_reference >= static_cast<std::int32_t>(stored_grid_width * stored_grid_height))
                reader.fail("active serialized cell reference outside stored grid", base_at + 12);
            if (record.gate_house && (record.gate_house->layout < 0 || record.gate_house->layout > 1))
                reader.fail("unsupported active GateHouse layout outside 0..1", base_at + (schema == 5 ? 113 : 112));
        }
        if (entity_class != OriginalEntityClass::Building) {
            // Monument5631b0, Fill56fc30, NonHouse/Industrial51ce00 wrappers.
            expect_schema(reader, 1, "entity wrapper schema");
            record.provenance.wrapper_schema = 1;
        }
        if (entity_class == OriginalEntityClass::Monument) {
            const auto ext_at = reader.position();
            const auto ext_schema = reader.u16("monument state schema");
            // cMonInfo561e30: schema9 read562218..562393/common562a1b,
            // schema10 read562078..562213. Both have the same selected offsets.
            if (ext_schema != 9 && ext_schema != 10)
                reader.unsupported("unsupported monument state schema " + std::to_string(ext_schema), ext_at);
            const std::size_t ext_length = ext_schema == 9 ? 138 : 139;
            reader.take(ext_length - 2, "monument state");
            const auto ext = bytes.subspan(ext_at, ext_length);
            record.monument = OriginalMonumentState{i32_at(ext, 6), i32_at(ext, 87),
                                                    i32_at(ext, 38), ext[123]};
            record.provenance.logical_extended_offset = logical_offset + ext_at;
            record.provenance.extended_schema = ext_schema;
            // Bounded Great Wall state domain only. Other monument families
            // retain raw fields; their semantic stages have not been audited.
            // Zero material is constructor-valid (561ddd), and is still not
            // the material chosen by the external-context restore operation.
            if (record.active() && record.type >= 253 && record.type <= 268) {
                if (record.monument->phase < 0 || record.monument->phase > 11)
                    reader.fail("unsupported active Great Wall phase outside 0..11", ext_at + 6);
                if (record.monument->serialized_material < 0 || record.monument->serialized_material > 3)
                    reader.fail("unsupported active Great Wall serialized material outside 0..3", ext_at + 87);
                if (record.monument->orientation > 3)
                    reader.fail("unsupported active Great Wall orientation outside 0..3", ext_at + 123);
            }
        } else if (entity_class == OriginalEntityClass::Industrial ||
                   entity_class == OriginalEntityClass::Ferry ||
                   entity_class == OriginalEntityClass::GateHouse ||
                   entity_class == OriginalEntityClass::Tower) {
            // Saved cIndustrialBldg descriptor854438 uses ctor51c140→51c9a0,
            // cNonHouseInfo51c3a0. It is distinct from runtime industrial42d480.
            const auto ext_at = reader.position();
            expect_schema(reader, 1, "non-house state schema");
            reader.take(126, "non-house state");
            record.provenance.logical_extended_offset = logical_offset + ext_at;
            record.provenance.extended_schema = 1;
            if (entity_class == OriginalEntityClass::Ferry) {
                // Ferry4c6b90 calls51ce00 then schema1 +4 +4 +0x7d0 bytes.
                expect_schema(reader, 1, "ferry state schema");
                reader.take(2008, "ferry state");
            }
        }
        if (entity_class == OriginalEntityClass::GateHouse) {
            // cGateHouse4f9f20 calls the existing 51ce00 parent serializer.
            // Its own schema1 has no payload; schema2 adds two i32 attached-unit
            // references at runtime +150/+154. Neither chooses the static body.
            const auto class_at = reader.position();
            const auto class_schema = reader.u16("GateHouse wrapper schema");
            if (class_schema != 1 && class_schema != 2)
                reader.unsupported("unsupported GateHouse wrapper schema " + std::to_string(class_schema), class_at);
            if (class_schema == 2) reader.take(8, "GateHouse attached-unit state");
            record.provenance.logical_class_wrapper_offset = logical_offset + class_at;
            record.provenance.class_wrapper_schema = class_schema;
        } else if (entity_class == OriginalEntityClass::Tower) {
            // cTower5da880 uses the same saved NonHouse parent, then only its
            // schema0 word. No additional tower state or body is inferred.
            const auto class_at = reader.position();
            expect_schema(reader, 0, "Tower wrapper schema");
            record.provenance.logical_class_wrapper_offset = logical_offset + class_at;
            record.provenance.class_wrapper_schema = 0;
        }
        record.provenance.record_byte_length = static_cast<std::uint32_t>(reader.position() - record_at);
        result.records.push_back(record);
    }
    result.byte_length = reader.position();
    return result;
}

OriginalMapEntities read_original_map_entities(const EmperorContainer& container, std::size_t part) {
    const auto probe = probe_map_part(container, part);
    if (probe.profile != PartProfile::Map)
        throw OriginalEntityError("original entity manager requires a supported standalone map: " + probe.reason);
    const auto& source = container.parts().at(part);
    if (source.uncompressed_size < original_entities_logical_offset)
        throw OriginalEntityError("original entity manager: map is truncated before logical 1093607");
    const auto length = std::min(source.uncompressed_size - original_entities_logical_offset,
                                 maximum_manager_byte_length);
    const auto bytes = container.read_range(part, original_entities_logical_offset, length);
    return parse_original_map_entities(bytes, probe.declared_map_size);
}

} // namespace openemperor::maps
