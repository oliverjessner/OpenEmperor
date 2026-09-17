#include "assets/Sg3AlphaAudit.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace assets = openemperor::assets;
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t table_offset = 40680;

struct TempDirectory {
    fs::path path = fs::temp_directory_path() /
        ("openemperor-alpha-audit-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    TempDirectory() { fs::create_directories(path); }
    ~TempDirectory() { std::error_code ignored; fs::remove_all(path, ignored); }
};

void u16(Bytes& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void u32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    u16(bytes, at, static_cast<std::uint16_t>(value));
    u16(bytes, at + 2, static_cast<std::uint16_t>(value >> 16U));
}

void write(const fs::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("synthetic write failed");
}

bool checks() {
    using assets::AlphaAddressing;
    using assets::AuditRangeStatus;
    const auto overflow = assets::evaluate_alpha_candidate(
        std::numeric_limits<std::uint64_t>::max() - 1, 3, 0, 2, 0,
        std::numeric_limits<std::uint64_t>::max(), true, AlphaAddressing::Contiguous);
    if (overflow.color != AuditRangeStatus::Overflow ||
        overflow.alpha != AuditRangeStatus::Overflow) return false;
    const auto negative = assets::evaluate_alpha_candidate(
        0, 2, 0, 2, 1, 16, true, AlphaAddressing::Legacy);
    if (negative.color != AuditRangeStatus::NegativeOffset ||
        negative.alpha != AuditRangeStatus::NegativeOffset) return false;
    const auto missing = assets::evaluate_alpha_candidate(
        0, 2, 2, 2, 0, 0, false, AlphaAddressing::Spec);
    if (missing.color != AuditRangeStatus::SourceUnavailable ||
        missing.alpha != AuditRangeStatus::SourceUnavailable) return false;

    const TempDirectory temp;
    Bytes archive(table_offset + 4U * 72U, 0);
    u32(archive, 0, static_cast<std::uint32_t>(archive.size()));
    u32(archive, 4, 214);
    u32(archive, 12, 4);
    u32(archive, 16, 4);
    u32(archive, 20, 1);
    const char group[] = "external.bmp";
    std::copy(group, group + sizeof(group) - 1, archive.begin() + 680);
    u32(archive, 680 + 124, 4);
    u32(archive, 680 + 132, 3);
    auto record = [&](std::size_t index, std::uint32_t color,
                      std::uint32_t alpha, bool external) {
        const std::size_t at = table_offset + index * 72U;
        u32(archive, at, color);
        u32(archive, at + 4, 2);
        u16(archive, at + 20, 1);
        u16(archive, at + 22, 1);
        u16(archive, at + 50, 256);
        archive[at + 52] = external ? 1 : 0;
        u32(archive, at + 64, alpha);
        u32(archive, at + 68, 2);
    };
    record(0, 0, 8, false);  // separate alpha: SPEC valid, adjacent malformed
    record(1, 16, 18, false); // contiguous: all valid
    record(2, 3, 4, true);   // external -1: SPEC/LEGACY valid, adjacent malformed
    record(3, 0, 4, true);   // legacy negative offset
    write(temp.path / "fixture.sg3", archive);
    Bytes internal(20, 0);
    internal[2] = 2; internal[3] = 0;
    internal[8] = 1; internal[9] = 31;
    internal[18] = 1; internal[19] = 31;
    write(temp.path / "fixture.555", internal);
    Bytes external(8, 0);
    external[4] = 1; external[5] = 31; external[6] = 0;
    write(temp.path / "external.555", external);
    const auto catalog = assets::scan_asset_catalog(temp.path);
    const auto audit = assets::audit_alpha_catalog(catalog);
    if (audit.total.records != 4 || audit.by_source.at("internal").records != 2 ||
        audit.by_source.at("external").records != 2 ||
        audit.by_type.at(256).records != 4 || audit.by_archive.at("fixture.sg3").records != 4 ||
        audit.total.valid != std::array<std::uint64_t, 3>{4, 2, 2} ||
        audit.total.complete != std::array<std::uint64_t, 3>{4, 1, 2} ||
        audit.total.malformed != std::array<std::uint64_t, 3>{0, 2, 1} ||
        audit.total.negative[2] != 1 ||
        audit.alpha_offset_equals_data_plus_twice_length != 1 ||
        audit.delta_from_contiguous.at(0) != 1 ||
        audit.delta_from_legacy_alpha.at(0) != 1) return false;
    if (audit.records[0].candidates[1].syntax.failure !=
        "alpha literal run advances beyond image bounds" ||
        audit.records[0].candidates[1].syntax.bytes_consumed != 1 ||
        audit.records[0].candidates[1].syntax.final_pixel_cursor != 0 ||
        audit.records[0].candidates[0].syntax.image_pixel_count != 1) return false;
    const auto again = assets::audit_alpha_catalog(catalog);
    return again.total.valid == audit.total.valid &&
        again.delta_from_contiguous == audit.delta_from_contiguous;
}

} // namespace

int main() {
    try {
        if (checks()) return 0;
        std::cerr << "SG3 alpha audit checks failed\n";
    } catch (const std::exception& error) {
        std::cerr << "SG3 alpha audit setup failed: " << error.what() << '\n';
    }
    return 1;
}
