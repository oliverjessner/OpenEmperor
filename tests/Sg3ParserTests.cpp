#include "assets/Sg3Archive.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <vector>

namespace {

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    write_u16(bytes, offset, static_cast<std::uint16_t>(value & 0xffff));
    write_u16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16));
}

std::vector<std::uint8_t> make_synthetic_archive() {
    // Independently constructed from the public layout; no game bytes are embedded.
    std::vector<std::uint8_t> bytes(40680 + 72);
    write_u32(bytes, 0, static_cast<std::uint32_t>(bytes.size()));
    write_u32(bytes, 4, 214);
    write_u32(bytes, 12, 1);
    write_u32(bytes, 16, 1);
    write_u32(bytes, 20, 1);
    write_u16(bytes, 80, 0);
    bytes[680] = 't';
    bytes[681] = 'e';
    bytes[682] = 's';
    bytes[683] = 't';
    write_u32(bytes, 680 + 124, 1);
    write_u32(bytes, 40680, 4);
    write_u32(bytes, 40680 + 4, 8);
    write_u32(bytes, 40680 + 8, 4);
    write_u16(bytes, 40680 + 20, 2);
    write_u16(bytes, 40680 + 22, 2);
    bytes[40680 + 50] = 1;
    write_u32(bytes, 40680 + 64, 12);
    write_u32(bytes, 40680 + 68, 4);
    return bytes;
}

template <typename Operation>
bool rejects(Operation operation) {
    try {
        operation();
        return false;
    } catch (const openemperor::assets::Sg3ParseError&) {
        return true;
    }
}

bool run_checks() {
    using namespace openemperor::assets;
    auto bytes = make_synthetic_archive();
    const auto archive = parse_sg3(bytes, bytes.size());
    if (archive.header.version != 214 || archive.groups.size() != 1 ||
        archive.groups[0].filename != "test" || archive.images.size() != 1 ||
        archive.images[0].data_offset != 4 || archive.images[0].data_length != 8 ||
        archive.images[0].alpha_offset != 12 || archive.images[0].alpha_length != 4) {
        return false;
    }
    if (required_sg3_table_size(std::span{bytes}.first(680), bytes.size()) != bytes.size()) {
        return false;
    }
    if (!rejects([&] { parse_sg3(std::span{bytes}.first(bytes.size() - 1), bytes.size()); })) {
        return false;
    }
    if (!rejects([&] { required_sg3_table_size(std::span{bytes}.first(680), bytes.size() - 1); })) {
        return false;
    }
    write_u32(bytes, 20, 201);
    if (!rejects([&] { parse_sg3(bytes, bytes.size()); })) {
        return false;
    }
    write_u32(bytes, 20, 1);
    write_u32(bytes, 4, 211);
    if (!rejects([&] { parse_sg3(bytes, bytes.size()); })) {
        return false;
    }
    auto version_213 = make_synthetic_archive();
    version_213.resize(40680 + 64 + 16);
    write_u32(version_213, 4, 213);
    write_u32(version_213, 0, 1); // A conflicting reported size must remain visible.
    version_213.back() = 0xa5; // Unknown suffix: accepted but left unparsed.
    const auto older = parse_sg3(version_213, version_213.size());
    if (older.header.reported_file_size != 1 || older.parsed_table_size != 40680 + 64 ||
        older.actual_file_size - older.parsed_table_size != 16 ||
        older.images.size() != 1 || older.images[0].data_length != 8) {
        return false;
    }
    if (!range_within_file(4, 8, 12) || range_within_file(4, 9, 12) ||
        !range_within_file(12, 0, 12) || range_within_file(13, 0, 12) ||
        range_within_file(std::numeric_limits<std::uint64_t>::max(), 2, 12)) {
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!run_checks()) {
        std::cerr << "SG3 parser/bounds checks failed\n";
        return 1;
    }
    return 0;
}
