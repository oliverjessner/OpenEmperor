#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/MapGraphicCandidates.h"
#include "maps/MapCatalog.h"
#include "maps/DirectGraphicCandidate.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace maps = openemperor::maps;
using Bytes = std::vector<std::uint8_t>;
void check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
template <class F> void rejects(F f) {
    try { f(); } catch (const std::exception&) { return; }
    throw std::runtime_error("expected malformed input rejection");
}
void u32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8U);
    bytes[at + 2] = static_cast<std::uint8_t>(value >> 16U);
    bytes[at + 3] = static_cast<std::uint8_t>(value >> 24U);
}
void append_u32(Bytes& bytes, std::uint32_t value) {
    const auto at = bytes.size(); bytes.resize(at + 4); u32(bytes, at, value);
}
Bytes block(const Bytes& input) {
    uLongf limit = compressBound(static_cast<uLong>(input.size()));
    Bytes compressed(static_cast<std::size_t>(limit));
    check(compress2(compressed.data(), &limit, input.data(),
                    static_cast<uLong>(input.size()), 6) == Z_OK, "synthetic zlib compression");
    compressed.resize(static_cast<std::size_t>(limit));
    Bytes result;
    append_u32(result, 0x12345678U); // Deliberately unknown synthetic header word.
    append_u32(result, static_cast<std::uint32_t>(compressed.size()));
    append_u32(result, static_cast<std::uint32_t>(input.size()));
    result.insert(result.end(), compressed.begin(), compressed.end());
    return result;
}
Bytes part(const Bytes& input, std::size_t chunk = 32768) {
    Bytes result;
    for (std::size_t at = 0; at < input.size(); at += chunk) {
        const auto stop = std::min(input.size(), at + chunk);
        const auto encoded = block(Bytes(input.begin() + static_cast<std::ptrdiff_t>(at),
                                         input.begin() + static_cast<std::ptrdiff_t>(stop)));
        result.insert(result.end(), encoded.begin(), encoded.end());
    }
    return result;
}
Bytes single(const Bytes& raw, std::size_t chunk = 32768) {
    Bytes result; append_u32(result, 0xfedcbaaaU);
    const auto p = part(raw, chunk);
    result.insert(result.end(), p.begin(), p.end());
    return result;
}
Bytes multipart(const Bytes& first, const Bytes& second) {
    Bytes result; append_u32(result, 0xfedcbaaaU);
    const auto a = part(first), b = part(second);
    result.insert(result.end(), a.begin(), a.end());
    const auto second_at = static_cast<std::uint32_t>(result.size());
    result.insert(result.end(), b.begin(), b.end());
    const auto footer_at = result.size();
    result.resize(footer_at + 68, 0);
    u32(result, footer_at, 0xaaabcdefU);
    u32(result, footer_at + 4, 0x3cU);
    u32(result, footer_at + 8, second_at);
    u32(result, footer_at + 12, static_cast<std::uint32_t>(footer_at)); // End marker, ignored.
    return result;
}
void write(const fs::path& path, const Bytes& bytes) {
    std::ofstream out{path, std::ios::binary};
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out), "write synthetic fixture");
}
Bytes map_bytes() {
    Bytes bytes(static_cast<std::size_t>(maps::objects_logical_offset + maps::grid_byte_length), 0);
    bytes.resize(static_cast<std::size_t>(maps::auxiliary_byte_logical_offset +
                                          maps::candidate_byte_byte_length),0);
    const std::array<std::uint8_t, 8> signature{5, 0, 0xfe, 0xca, 0, 0, 2, 0};
    std::copy(signature.begin(), signature.end(), bytes.begin());
    u32(bytes, 84, 112);
    u32(bytes, static_cast<std::size_t>(maps::candidate_word_logical_offset), 0x12345678U);
    u32(bytes, static_cast<std::size_t>(maps::candidate_word_logical_offset) + (2U * 228U + 3U) * 4U,
        0xfedcba98U);
    u32(bytes, static_cast<std::size_t>(maps::candidate_word_logical_offset) + (2U * 228U + 4U) * 4U,
        2U);
    bytes[static_cast<std::size_t>(maps::candidate_byte_logical_offset)] = 0xa5;
    bytes[static_cast<std::size_t>(maps::candidate_byte_logical_offset) + 2U * 228U + 3U] = 0x5a;
    bytes[static_cast<std::size_t>(maps::auxiliary_byte_logical_offset) + 2U * 228U + 3U] = 0x37;
    u32(bytes, static_cast<std::size_t>(maps::terrain_logical_offset) + 4U, 0x12345678U); // (1,0)
    u32(bytes, static_cast<std::size_t>(maps::terrain_logical_offset) + 228U * 4U, 0xdeadbeefU); // (0,1)
    u32(bytes, static_cast<std::size_t>(maps::objects_logical_offset) + (2U * 228U + 3U) * 4U, 0xaabbccddU);
    return bytes;
}
} // namespace

int main() {
    try {
        const fs::path root = fs::temp_directory_path() /
            ("openemperor-map-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(root);
        const auto file = root / "synthetic.map";
        write(file, single(Bytes{'a', 'b', 'c'}));
        auto container = maps::EmperorContainer::open(file);
        check(!container.multipart() && container.parts().size() == 1 &&
              container.parts()[0].blocks.size() == 1 &&
              container.read_part(0) == Bytes({'a', 'b', 'c'}), "single block");
        check(container.parts()[0].blocks[0].physical_header_offset == 4 &&
              container.parts()[0].blocks[0].logical_offset == 0, "offset domains");
        rejects([&] { container.read_range(0, 3, 1); });
        rejects([&] { container.read_range(0, UINT64_MAX, 1); });
        rejects([&] { container.read_part(1); });

        write(file, single(Bytes{'a','b','c','d','e','f','g'}, 3));
        container = maps::EmperorContainer::open(file);
        check(container.parts()[0].blocks.size() == 3 &&
              container.read_range(0, 2, 4) == Bytes({'c','d','e','f'}), "cross-block logical read");

        auto data = map_bytes();
        fs::create_directory(root/"catalog");
        write(root/"catalog/Zed.MAP",single(data));
        write(root/"catalog/Alpha.map",single(data));
        write(root/"catalog/Invalid.map",Bytes{'n','o'});
        write(root/"catalog/Unknown.map",single(Bytes{'a','b','c'}));
        const auto discovered=maps::discover_standalone_maps(root/"catalog");
        check(discovered.entries.size()==4 && discovered.entries[0].relative_path=="Alpha.map" &&
              discovered.entries[1].relative_path=="Invalid.map" &&
              discovered.entries[2].relative_path=="Unknown.map" &&
              discovered.entries[3].relative_path=="Zed.MAP" &&
              discovered.entries[0].map_profile && discovered.entries[0].declared_size==112 &&
              !discovered.entries[1].container_valid && !discovered.entries[1].error.empty() &&
              discovered.entries[2].container_valid && !discovered.entries[2].map_profile &&
              discovered.entries[3].map_profile,
              "deterministic case-insensitive map discovery keeps bad entries independently");
        check(maps::resolve_map_path(root/"catalog","Alpha.map")==
              fs::canonical(root/"catalog/Alpha.map") &&
              maps::resolve_map_path(root/"catalog",fs::canonical(root/"catalog/Alpha.map"))==
              fs::canonical(root/"catalog/Alpha.map"),
              "safe relative and in-root absolute map resolution");
        rejects([&]{ (void)maps::discover_standalone_maps(root/"missing"); });
        rejects([&]{ (void)maps::resolve_map_path(root/"catalog","../synthetic.map"); });
        std::error_code symlink_error;
        fs::create_symlink(root/"synthetic.map",root/"catalog/Outside.map",symlink_error);
        if (!symlink_error) {
            check(maps::discover_standalone_maps(root/"catalog").entries.size()==4,
                  "map discovery does not follow symlink files");
            rejects([&]{ (void)maps::resolve_map_path(root/"catalog","Outside.map"); });
        }
        write(file, single(data));
        container = maps::EmperorContainer::open(file);
        check(maps::probe_map_part(container, 0).profile == maps::PartProfile::Map, "map profile probe");
        const auto parsed = maps::read_emperor_map(container, 0);
        check(parsed.stored_width == 228 && parsed.declared_map_size == 112 &&
              parsed.terrain_raw.values.size() == 51984 && parsed.objects_raw.values.size() == 51984,
              "complete map layer lengths");
        check(parsed.terrain_at(1,0) == 0x12345678U &&
              parsed.terrain_at(0,1) == 0xdeadbeefU &&
              parsed.object_at(3,2) == 0xaabbccddU, "row-major little-endian raw words");
        check(parsed.terrain_cell_offset(1,0) == maps::terrain_logical_offset + 4 &&
              parsed.object_cell_offset(3,2) == maps::objects_logical_offset + (2 * 228 + 3) * 4,
              "cell logical offsets");
        const auto diagnostic = maps::read_map_graphic_candidates(container, 0);
        check(maps::read_auxiliary_map_byte(container,0,3,2) == 0x37 &&
              diagnostic.byte_at(3,2) == 0x5a &&
              maps::auxiliary_byte_logical_offset != maps::candidate_byte_logical_offset,
              "separate bounded auxiliary and candidate byte layers");
        rejects([&] { (void)maps::read_auxiliary_map_byte(container,0,228,2); });
        check(diagnostic.candidate_word_layer.size() == 51984 &&
              diagnostic.candidate_byte_layer.size() == 51984 &&
              diagnostic.word_at(0,0) == 0x12345678U &&
              diagnostic.word_at(3,2) == 0xfedcba98U &&
              diagnostic.byte_at(0,0) == 0xa5 && diagnostic.byte_at(3,2) == 0x5a,
              "candidate layer little-endian and y*228+x without truncation");
        check(diagnostic.word_offset(3,2) == maps::candidate_word_logical_offset + (2U*228U+3U)*4U &&
              diagnostic.byte_offset(3,2) == maps::candidate_byte_logical_offset + 2U*228U+3U &&
              parsed.terrain_at(1,0) == 0x12345678U &&
              parsed.object_at(3,2) == 0xaabbccddU,
              "candidate reads preserve raw production layers and exact offsets");
        openemperor::assets::AssetCatalog synthetic_catalog;
        synthetic_catalog.records.resize(3);
        synthetic_catalog.records[2].width=1;
        synthetic_catalog.records[2].height=1;
        synthetic_catalog.records[2].data_length=2;
        synthetic_catalog.records[2].color_bounds=openemperor::assets::AssetRangeStatus::InBounds;
        synthetic_catalog.records[2].alpha_bounds=openemperor::assets::AssetRangeStatus::NotPresent;
        synthetic_catalog.records[2].decoder_supported=true;
        check(diagnostic.word_at(4,2)==2 &&
              maps::resolve_direct_candidate(synthetic_catalog,diagnostic.word_at(4,2)).status==
                  maps::DirectCandidateStatus::DecodeCandidate &&
              maps::resolve_direct_candidate(synthetic_catalog,diagnostic.word_at(3,2)).status==
                  maps::DirectCandidateStatus::IndexOutOfRange &&
              diagnostic.word_offset(4,2)==maps::candidate_word_logical_offset+(2U*228U+4U)*4U,
              "selected image index belongs to original storage coordinate without truncation");
        rejects([&] { diagnostic.word_at(228,0); });
        rejects([&] { diagnostic.byte_at(0,228); });
        rejects([&] { (void)maps::decode_map_graphic_candidates(
            std::span{data}.subspan(static_cast<std::size_t>(maps::candidate_word_logical_offset),
                                    static_cast<std::size_t>(maps::candidate_word_byte_length)-1U),
            std::span{data}.subspan(static_cast<std::size_t>(maps::candidate_byte_logical_offset),
                                    static_cast<std::size_t>(maps::candidate_byte_byte_length))); });
        rejects([&] { (void)maps::decode_map_graphic_candidates(
            std::span{data}.subspan(static_cast<std::size_t>(maps::candidate_word_logical_offset),
                                    static_cast<std::size_t>(maps::candidate_word_byte_length)),
            std::span{data}.subspan(static_cast<std::size_t>(maps::candidate_byte_logical_offset),
                                    static_cast<std::size_t>(maps::candidate_byte_byte_length)-1U)); });
        write(file, single(data, 1537)); // First 32-bit word crosses a block boundary.
        const auto crossed = maps::read_map_graphic_candidates(maps::EmperorContainer::open(file), 0);
        check(crossed.word_at(0,0) == 0x12345678U &&
              crossed.word_at(3,2) == 0xfedcba98U && crossed.byte_at(3,2) == 0x5a,
              "candidate reads across compressed block boundaries");
        rejects([&] { parsed.terrain_at(228,0); });
        rejects([&] { parsed.object_at(0,228); });

        const auto multi_file = root / "synthetic.pak";
        write(multi_file, multipart(Bytes{'x','y','z'}, data));
        auto multi = maps::EmperorContainer::open(multi_file);
        check(multi.multipart() && multi.parts().size() == 2 &&
              maps::probe_map_part(multi, 0).profile == maps::PartProfile::Unknown &&
              maps::read_emperor_map(multi, 1).terrain_at(1,0) == 0x12345678U,
              "selected multipart map profile");
        rejects([&] { maps::read_emperor_map(multi, 0); });
        auto broken_table = multipart(Bytes{'x'}, Bytes{'y'});
        u32(broken_table, broken_table.size() - 60, 3); // Part start before first physical block.
        write(multi_file, broken_table);
        rejects([&] { maps::EmperorContainer::open(multi_file); });

        auto invalid_size = data; u32(invalid_size, 84, 229);
        write(file, single(invalid_size));
        container = maps::EmperorContainer::open(file);
        rejects([&] { maps::read_emperor_map(container, 0); });
        rejects([&] { maps::read_map_graphic_candidates(container, 0); });
        auto invalid_signature = data; invalid_signature[0] = 0;
        write(file, single(invalid_signature));
        container = maps::EmperorContainer::open(file);
        rejects([&] { maps::read_emperor_map(container, 0); });
        auto short_map = data; short_map.resize(500000);
        write(file, single(short_map));
        container = maps::EmperorContainer::open(file);
        rejects([&] { maps::read_emperor_map(container, 0); });

        auto truncated = single(Bytes{'a','b'});
        truncated.resize(14); write(file, truncated);
        rejects([&] { maps::EmperorContainer::open(file); });
        auto bad_length = single(Bytes{'a','b'});
        u32(bad_length, 8, 0xffffffffU); write(file, bad_length);
        rejects([&] { maps::EmperorContainer::open(file); });
        bad_length = single(Bytes{'a','b'});
        u32(bad_length, 12, 32769); write(file, bad_length);
        rejects([&] { maps::EmperorContainer::open(file); });
        auto extra_input = single(Bytes{'a','b'});
        extra_input.push_back(0);
        u32(extra_input, 8, static_cast<std::uint32_t>(extra_input.size() - 16));
        write(file, extra_input);
        container = maps::EmperorContainer::open(file);
        rejects([&] { container.read_part(0); });
        auto wrong_output = single(Bytes{'a','b'});
        u32(wrong_output, 12, 3); write(file, wrong_output);
        container = maps::EmperorContainer::open(file);
        rejects([&] { container.read_part(0); });
        auto corrupt = single(Bytes{'a','b'}); corrupt[16] ^= 0xffU;
        write(file, corrupt); container = maps::EmperorContainer::open(file);
        rejects([&] { container.read_part(0); });
        auto short_payload = single(Bytes{'a','b'}); short_payload.pop_back();
        write(file, short_payload);
        rejects([&] { maps::EmperorContainer::open(file); });

        {
            std::ofstream oversized{file, std::ios::binary | std::ios::trunc};
            oversized.seekp(64 * 1024 * 1024);
            oversized.put('\0');
        }
        rejects([&] { maps::EmperorContainer::open(file); });

        fs::remove_all(root);
        std::cout << "synthetic container and map checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
