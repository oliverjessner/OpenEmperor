#include "maps/OriginalMapEntities.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace openemperor::maps;
namespace {
using Bytes = std::vector<std::uint8_t>;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void put16(Bytes& bytes, std::size_t at, std::uint16_t value) {
    bytes.at(at) = static_cast<std::uint8_t>(value);
    bytes.at(at + 1) = static_cast<std::uint8_t>(value >> 8);
}
void put32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned n = 0; n < 4; ++n) bytes.at(at + n) = static_cast<std::uint8_t>(value >> (n * 8));
}
void append16(Bytes& bytes, std::uint16_t value) {
    const auto at = bytes.size(); bytes.resize(at + 2); put16(bytes, at, value);
}
void append32(Bytes& bytes, std::uint32_t value) {
    const auto at = bytes.size(); bytes.resize(at + 4); put32(bytes, at, value);
}
void append(Bytes& bytes, const Bytes& extra) { bytes.insert(bytes.end(), extra.begin(), extra.end()); }
Bytes header(std::uint32_t count) { Bytes bytes; append16(bytes, 1); append32(bytes, count); return bytes; }
void new_class(Bytes& bytes, std::string_view name) {
    append16(bytes, 0xffff); append16(bytes, 0); append16(bytes, static_cast<std::uint16_t>(name.size()));
    bytes.insert(bytes.end(), name.begin(), name.end());
}
Bytes base(unsigned schema, bool active = false, std::uint32_t id = 1) {
    Bytes bytes(schema == 3 ? 179 : schema == 4 ? 181 : 182, 0);
    put16(bytes, 0, static_cast<std::uint16_t>(schema));
    bytes[2] = active ? 3 : 0;
    bytes[5] = active ? 4 : 0;
    put16(bytes, 8, active ? 55 : 0xffff);
    put16(bytes, 10, active ? 32 : 0xffff);
    put32(bytes, 12, active ? 13992 : 0xffffffffU);
    put16(bytes, 16, active ? 257 : 0xffff);
    put16(bytes, 18, active ? 7 : 0xffff);
    put32(bytes, bytes.size() - 20, active ? id : 0xffffffffU);
    return bytes;
}
Bytes monument_state(unsigned schema) {
    Bytes bytes(schema == 9 ? 138 : 139, 0);
    put16(bytes, 0, static_cast<std::uint16_t>(schema));
    put32(bytes, 6, 10); put32(bytes, 87, 3); put32(bytes, 38, 0xfffffffcU); bytes[123] = 2;
    return bytes;
}
Bytes one_monument(unsigned base_schema = 4, unsigned ext_schema = 10) {
    auto bytes = header(1); new_class(bytes, "cMonumentBldg"); append(bytes, base(base_schema, true));
    append16(bytes, 1); append(bytes, monument_state(ext_schema)); return bytes;
}
Bytes gate_record(unsigned schema,unsigned wrapper,std::int32_t layout,bool active=true,std::uint32_t id=1) {
    auto bytes=base(schema,active,id);
    put32(bytes,schema==5 ? 113:112,static_cast<std::uint32_t>(layout));
    put16(bytes,16,130);bytes[5]=1;
    append16(bytes,1);append16(bytes,1);bytes.resize(bytes.size()+126);
    append16(bytes,static_cast<std::uint16_t>(wrapper));
    if(wrapper==2) {append32(bytes,0x7fffffff);append32(bytes,0xffffffff);}
    return bytes;
}
Bytes one_gate(unsigned schema,unsigned wrapper,std::int32_t layout,bool active=true) {
    auto bytes=header(1);new_class(bytes,"cGateHouse");append(bytes,gate_record(schema,wrapper,layout,active));return bytes;
}
Bytes tower_record(unsigned schema,std::uint32_t id) {
    auto bytes=base(schema,true,id);put16(bytes,16,131);bytes[5]=1;
    append16(bytes,1);append16(bytes,1);bytes.resize(bytes.size()+126);append16(bytes,0);return bytes;
}
template <class F> void rejects(F action, std::string_view expected) {
    try { action(); }
    catch (const OriginalEntityError& error) {
        check(std::string_view(error.what()).find(expected) != std::string_view::npos, "specific parser diagnostic");
        return;
    }
    throw std::runtime_error("malformed manager unexpectedly accepted");
}
Bytes all_classes() {
    auto bytes = header(5);
    new_class(bytes, "Building"); append(bytes, base(3));
    new_class(bytes, "cFillBldg"); append(bytes, base(4, true, 2)); append16(bytes, 1);
    new_class(bytes, "cIndustrialBldg"); append(bytes, base(5, true, 3)); append16(bytes, 1);
    append16(bytes, 1); bytes.resize(bytes.size() + 126);
    new_class(bytes, "cFerryBldg"); append(bytes, base(3, true, 4)); append16(bytes, 1);
    append16(bytes, 1); bytes.resize(bytes.size() + 126); append16(bytes, 1); bytes.resize(bytes.size() + 2008);
    new_class(bytes, "cMonumentBldg"); append(bytes, base(5, true, 5)); append16(bytes, 1);
    append(bytes, monument_state(9));
    return bytes;
}
Bytes compressed_container(const Bytes& raw) {
    Bytes output; append32(output, 0xfedcbaaaU);
    for (std::size_t at = 0; at < raw.size(); at += 16384) {
        const auto length = std::min<std::size_t>(16384, raw.size() - at);
        uLongf compressed_length = compressBound(static_cast<uLong>(length));
        Bytes compressed(compressed_length);
        check(compress2(compressed.data(), &compressed_length, raw.data() + at,
                        static_cast<uLong>(length), 6) == Z_OK, "synthetic container compression");
        compressed.resize(compressed_length);
        append32(output, 0); append32(output, static_cast<std::uint32_t>(compressed_length));
        append32(output, static_cast<std::uint32_t>(length)); append(output, compressed);
    }
    return output;
}
struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("openemperor-entity-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directory(path); }
    ~TemporaryDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
void write(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(output), "synthetic container write");
}
}

int main() {
    try {
        for(const auto bs:{3U,4U,5U}) for(const auto wrapper:{1U,2U}) for(const auto layout:{0,1}) {
            auto bytes=one_gate(bs,wrapper,layout);const auto unchanged=bytes;
            const auto parsed_gate=parse_original_map_entities(bytes,170,1000);const auto& gate=parsed_gate.records.front();
            const unsigned base_length=bs==3 ? 179:bs==4 ? 181:182;
            const unsigned layout_offset=bs==5 ? 113:112;
            check(bytes==unchanged && parsed_gate.byte_length==bytes.size() && parsed_gate.records.size()==1 &&
                  gate.entity_class==OriginalEntityClass::GateHouse && gate.gate_house && gate.gate_house->layout==layout &&
                  gate.original_id==OriginalEntityId{1} && gate.serialized_original_id==1 && gate.footprint_side==1 && !gate.monument,
                  "GateHouse preserves typed identity and raw layout without attached-unit authority");
            check(gate.provenance.base_schema==bs && gate.provenance.wrapper_schema==1 && gate.provenance.extended_schema==1 &&
                  gate.provenance.logical_extended_offset==gate.provenance.logical_base_offset+base_length+2 &&
                  gate.provenance.logical_class_wrapper_offset==gate.provenance.logical_base_offset+base_length+130 &&
                  gate.provenance.class_wrapper_schema==wrapper && gate.provenance.record_byte_length==bytes.size()-6,
                  "parent, NonHouse and GateHouse wrappers retain separate exact provenance");
            const auto source=gate.field_source(OriginalEntityField::GateLayout);
            check(source && source->logical_offset==gate.provenance.logical_base_offset+layout_offset &&
                  source->record_relative_offset==16+layout_offset && source->byte_width==4 && source->signed_value &&
                  !gate.field_source(OriginalEntityField::MonumentPhase),
                  "GateHouse layout exposes signed i32 Base3/4/5 byte sources, not runtime offsets");
            for(std::size_t length=0;length<bytes.size();++length)
                rejects([&]{parse_original_map_entities(std::span(bytes).first(length),170);},"truncated");
            bytes.insert(bytes.end(),{0xde,0xad,0xbe,0xef});
            check(parse_original_map_entities(bytes,170).byte_length==unchanged.size(),"GateHouse wrapper consumes no subsequent map bytes");
            for(const auto invalid:{-1,2,std::numeric_limits<std::int32_t>::max()})
                rejects([&]{parse_original_map_entities(one_gate(bs,wrapper,invalid),170);},"GateHouse layout");
            const auto inactive=parse_original_map_entities(one_gate(bs,wrapper,-123,false),170).records.front();
            check(!inactive.active() && !inactive.original_id && inactive.serialized_original_id==-1 &&
                  inactive.gate_house->layout==-123 && inactive.field_source(OriginalEntityField::GateLayout),
                  "inactive raw GateHouse layout remains a fact without active body-selection authority");
            bytes=one_gate(bs,wrapper,layout);
            const auto parent=22+base_length;
            put16(bytes,parent,2);rejects([&]{parse_original_map_entities(bytes,170);},"entity wrapper schema");
            put16(bytes,parent,1);put16(bytes,parent+2,2);
            rejects([&]{parse_original_map_entities(bytes,170);},"non-house state schema");
            put16(bytes,parent+2,1);put16(bytes,parent+130,3);
            rejects([&]{parse_original_map_entities(bytes,170);},"GateHouse wrapper schema");
        }
        for(const auto bs:{3U,4U,5U}) {
            auto bytes=header(1);new_class(bytes,"cTower");append(bytes,tower_record(bs,1));
            const auto parsed_tower=parse_original_map_entities(bytes,170);const auto& tower=parsed_tower.records.front();
            check(tower.entity_class==OriginalEntityClass::Tower && tower.type==131 && tower.original_id==OriginalEntityId{1} &&
                  !tower.gate_house && !tower.monument && !tower.field_source(OriginalEntityField::GateLayout) &&
                  tower.provenance.wrapper_schema==1 && tower.provenance.extended_schema==1 &&
                  tower.provenance.logical_class_wrapper_offset && tower.provenance.class_wrapper_schema==0 &&
                  parsed_tower.byte_length==bytes.size(),"Tower admits only the proven parent state and payload-free schema0 wrapper");
            for(std::size_t length=0;length<bytes.size();++length)
                rejects([&]{parse_original_map_entities(std::span(bytes).first(length),170);},"truncated");
            put16(bytes,bytes.size()-2,1);rejects([&]{parse_original_map_entities(bytes,170);},"Tower wrapper schema");
        }
        {
            auto bytes=header(4);new_class(bytes,"cGateHouse");append(bytes,gate_record(3,2,0,true,1));
            append16(bytes,0x8001);append(bytes,gate_record(4,1,1,true,2));
            new_class(bytes,"cTower");append(bytes,tower_record(5,3));
            append16(bytes,0x8004);append(bytes,tower_record(3,4));
            const auto parsed_gates=parse_original_map_entities(bytes,170);
            check(parsed_gates.records.size()==4 && parsed_gates.byte_length==bytes.size() &&
                  parsed_gates.records[1].gate_house->layout==1 && parsed_gates.records[3].entity_class==OriginalEntityClass::Tower &&
                  parsed_gates.records[3].provenance.mfc_object_reference==6,
                  "new GateHouse/Tower objects share the existing MFC class/object reference sequence");
            put16(bytes,bytes.size()-2,1);
            rejects([&]{parse_original_map_entities(bytes,170);},"Tower wrapper schema");
            auto unknown=header(1);new_class(unknown,"cResWall");append(unknown,base(4));
            rejects([&]{parse_original_map_entities(unknown,170);},"unsupported MFC class");
        }
        for (const auto bs : {3U, 4U, 5U}) for (const auto es : {9U, 10U}) {
            auto bytes = one_monument(bs, es);
            const auto unchanged = bytes;
            const auto parsed = parse_original_map_entities(bytes, 170, 1000);
            check(bytes == unchanged && parsed.byte_length == bytes.size() && parsed.records.size() == 1,
                  "pure parser consumes exact manager prefix");
            const auto& record = parsed.records.front();
            check(record.active() && record.status == 3 && record.original_id == OriginalEntityId{1} &&
                  record.type == 257 && record.subindex == 7 && record.local_x == 55 && record.local_y == 32 &&
                  record.serialized_cell_reference == 13992 && record.footprint_side == 4,
                  "typed base state is decoded independently from graphics");
            check(record.monument && record.monument->phase == 10 && record.monument->serialized_material == 3 &&
                  record.monument->height == -4 && record.monument->orientation == 2,
                  "raw saved material and signed monument height retain their own meanings");
            const auto coordinates = record.field_source(OriginalEntityField::LocalX);
            const auto material = record.field_source(OriginalEntityField::SerializedMaterial);
            const auto identity = record.field_source(OriginalEntityField::OriginalId);
            const auto orientation = record.field_source(OriginalEntityField::MonumentOrientation);
            check(coordinates && coordinates->logical_offset == record.provenance.logical_base_offset + 8 &&
                  coordinates->record_relative_offset == 6 + 13 + 8 && coordinates->byte_width == 2 && coordinates->signed_value,
                  "coordinates expose proven serialized offsets, not runtime offsets");
            check(material && material->logical_offset == *record.provenance.logical_extended_offset + 87 &&
                  material->byte_width == 4 && material->signed_value && orientation &&
                  orientation->byte_width == 1 && !orientation->signed_value && identity &&
                  identity->logical_offset == record.provenance.logical_base_offset + (bs == 3 ? 159 : bs == 4 ? 161 : 162),
                  "field provenance retains schema-specific widths and identity tail");
            for (std::size_t size = 0; size < bytes.size(); ++size)
                rejects([&] { parse_original_map_entities(std::span(bytes).first(size), 170); }, "truncated");
            bytes.insert(bytes.end(), {0xde, 0xad, 0xbe, 0xef});
            check(parse_original_map_entities(bytes, 170).byte_length == unchanged.size(),
                  "subsequent map data is outside the entity manager");
        }
        auto bytes = all_classes();
        auto parsed = parse_original_map_entities(bytes, 170);
        check(parsed.records.size() == 5 && parsed.records[1].entity_class == OriginalEntityClass::Fill &&
              parsed.records[2].entity_class == OriginalEntityClass::Industrial &&
              parsed.records[3].entity_class == OriginalEntityClass::Ferry &&
              parsed.records[4].monument && parsed.byte_length == bytes.size(),
              "every admitted ancillary class has its own exact schema length");
        check(!parsed.records[0].original_id && parsed.records[0].serialized_original_id == -1 &&
              !parsed.records[0].field_source(OriginalEntityField::MonumentPhase),
              "empty records retain raw sentinels without inventing active identity/state");
        for (std::size_t size = 0; size < bytes.size(); ++size)
            rejects([&] { parse_original_map_entities(std::span(bytes).first(size), 170); }, "truncated");
        const auto industrial_ext = static_cast<std::size_t>(*parsed.records[2].provenance.logical_extended_offset -
                                                           original_entities_logical_offset);
        const auto ferry_state = static_cast<std::size_t>(*parsed.records[3].provenance.logical_extended_offset -
                                                        original_entities_logical_offset) + 128;
        put16(bytes, industrial_ext, 2);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "non-house state schema");
        put16(bytes, industrial_ext, 1); put16(bytes, ferry_state, 2);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "ferry state schema");
        bytes = header(4000); new_class(bytes, "Building"); append(bytes, base(4));
        for (unsigned index = 1; index < 4000; ++index) { append16(bytes, 0x8001); append(bytes, base(4)); }
        parsed = parse_original_map_entities(bytes, 170);
        check(parsed.records.size() == 4000 && !parsed.records.back().active() &&
              parsed.records.back().provenance.mfc_object_reference == 4001 && parsed.byte_length == bytes.size(),
              "entire bounded 4000-record no-wall manager validates shared MFC reference numbering");
        check(parse_original_map_entities(header(0), 170).records.empty(), "empty bounded manager has no walls");

        rejects([&] { parse_original_map_entities(header(4001), 170); }, "record count");
        rejects([&] { parse_original_map_entities(header(0xffffffffU), 170); }, "record count");
        bytes = header(1); append16(bytes, 0x8001);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "class reference");
        bytes = header(2); new_class(bytes, "Building"); append(bytes, base(4)); append16(bytes, 0x8002);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "class reference");
        put16(bytes, bytes.size() - 2, 2);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "aliased");
        put16(bytes, bytes.size() - 2, 7);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "object reference");
        put16(bytes, bytes.size() - 2, 0x7fff);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "extended MFC");
        bytes = header(1); new_class(bytes, "UnknownBldg");
        rejects([&] { parse_original_map_entities(bytes, 170); }, "unsupported MFC class");
        bytes = one_monument(); put16(bytes, 8, 1);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "class schema");
        bytes = header(1); append16(bytes, 0xffff); append16(bytes, 0); append16(bytes, 64);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "class name length");
        bytes = one_monument(); const std::size_t base_at = 6 + 6 + 13;
        put16(bytes, base_at, 2);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "Building schema");
        bytes = one_monument(); put16(bytes, base_at + 181, 2);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "wrapper schema");
        bytes = one_monument(); put16(bytes, base_at + 183, 8);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "monument state schema");
        bytes = one_monument(); put32(bytes, base_at + 183 + 6, 0xffffffffU);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "Great Wall phase");
        put32(bytes, base_at + 183 + 6, 12);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "Great Wall phase");
        bytes = one_monument(); put32(bytes, base_at + 183 + 87, 99);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "serialized material");
        bytes = one_monument(); bytes[base_at + 183 + 123] = 255;
        rejects([&] { parse_original_map_entities(bytes, 170); }, "Great Wall orientation");
        bytes = one_monument(); put32(bytes, base_at + 183 + 87, 0);
        check(parse_original_map_entities(bytes, 170).records[0].monument->serialized_material == 0,
              "constructor-valid raw zero material remains raw and cannot establish restored context");
        bytes[base_at + 2] = 0; put32(bytes, base_at + 183 + 6, 999);
        check(!parse_original_map_entities(bytes, 170).records[0].active(),
              "inactive monument state is retained without active Great Wall authority");
        bytes[base_at + 2] = 3; put16(bytes, base_at + 16, 83);
        check(parse_original_map_entities(bytes, 170).records[0].monument->phase == 999,
              "other monument families retain state without guessed Great Wall phase semantics");
        bytes = one_monument(); put16(bytes, base_at + 8, 0xffff);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "local coordinates");
        bytes = one_monument(); put32(bytes, base_at + 12, 51984);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "cell reference");
        bytes = one_monument(); put32(bytes, base_at + 161, 0);
        rejects([&] { parse_original_map_entities(bytes, 170); }, "original ID");
        bytes = header(2); new_class(bytes, "Building"); append(bytes, base(4, true));
        append16(bytes, 0x8001); append(bytes, base(4, true));
        rejects([&] { parse_original_map_entities(bytes, 170); }, "duplicate active original ID");
        rejects([&] { parse_original_map_entities(header(0), 0); }, "map size");
        rejects([&] { parse_original_map_entities(header(0), 229); }, "map size");
        rejects([&] { parse_original_map_entities(header(0), 170, std::numeric_limits<std::uint64_t>::max()); }, "overflow");

        TemporaryDirectory directory;
        Bytes map(static_cast<std::size_t>(original_entities_logical_offset), 0);
        const std::array<std::uint8_t, 8> signature{5, 0, 0xfe, 0xca, 0, 0, 2, 0};
        std::copy(signature.begin(), signature.end(), map.begin()); put32(map, 84, 170);
        bytes = all_classes(); append(map, bytes);
        const auto path = directory.path / "synthetic.map";
        write(path, compressed_container(map));
        parsed = read_original_map_entities(EmperorContainer::open(path), 0);
        check(parsed.records.size() == 5 && parsed.byte_length == bytes.size() &&
              parsed.logical_offset == original_entities_logical_offset,
              "container wrapper uses cross-block logical ranges and standalone profile");
        map[0] = 4; write(path, compressed_container(map));
        rejects([&] { read_original_map_entities(EmperorContainer::open(path), 0); }, "standalone map");
        std::cout << "Original map entity checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
