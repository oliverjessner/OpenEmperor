#include "assets/AssetCatalog.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t table_offset = 40680;

class TempDirectory {
public:
    TempDirectory() {
        path = fs::temp_directory_path() /
            ("openemperor-catalog-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(path);
    }
    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    fs::path path;
};

void u16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void u32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    u16(bytes, offset, static_cast<std::uint16_t>(value));
    u16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

Bytes archive(std::uint32_t count) {
    Bytes bytes(table_offset + static_cast<std::size_t>(count) * 72U, 0);
    u32(bytes, 0, static_cast<std::uint32_t>(bytes.size()));
    u32(bytes, 4, 214);
    u32(bytes, 12, count);
    u32(bytes, 16, count);
    u32(bytes, 20, 1);
    const std::string filename = "group.bmp";
    const std::string description = "Group sample";
    std::copy(filename.begin(), filename.end(), bytes.begin() + 680);
    std::copy(description.begin(), description.end(), bytes.begin() + 680 + 65);
    u32(bytes, 680 + 124, count);
    u32(bytes, 680 + 128, 0);
    u32(bytes, 680 + 132, count - 1U);
    return bytes;
}

void image(Bytes& bytes, std::size_t index, std::uint16_t type,
           std::uint16_t width, std::uint16_t height,
           std::uint32_t offset, std::uint32_t length,
           bool external = false) {
    const std::size_t at = table_offset + index * 72U;
    u32(bytes, at, offset);
    u32(bytes, at + 4, length);
    u16(bytes, at + 20, width);
    u16(bytes, at + 22, height);
    u16(bytes, at + 50, type);
    bytes[at + 52] = external ? 1 : 0;
}

void write(const fs::path& path, std::span<const std::uint8_t> bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write synthetic test file");
}

bool run_checks(const fs::path& parent) {
    namespace assets = openemperor::assets;
    const fs::path root = parent / "data";
    fs::create_directory(root);
    Bytes nested = archive(3);
    image(nested, 0, 13, 2, 2, 4, 8);
    u32(nested, table_offset + 64, 4095);
    u32(nested, table_offset + 68, 3); // Separate alpha range outside internal file.
    image(nested, 1, 256, 2, 2, 4, 8, true);
    image(nested, 2, 30, 78, 40, 5000, 3200);
    u32(nested, table_offset + 2U * 72U + 8U, 3200);
    nested[table_offset + 2U * 72U + 55U] = 1;
    write(root / "nested" / "a.sg3", nested);
    write(root / "nested" / "a.555", Bytes(4096, 0));
    write(root / "nested" / "group.555", Bytes(12, 0));

    Bytes upper = archive(1);
    image(upper, 0, 30, 78, 40, 4, 3200);
    u32(upper, table_offset + 8, 3200);
    upper[table_offset + 55] = 1;
    write(root / "B.SG3", upper);
    write(root / "B.555", Bytes(3204, 0));
    write(root / "broken.sg3", Bytes{1, 2, 3});
    write(parent / "outside.sg3", nested);
    std::error_code link_error;
    fs::create_symlink(parent / "outside.sg3", root / "escape.sg3", link_error);

    const assets::AssetCatalog catalog = assets::scan_asset_catalog(root);
    if (catalog.archive_count != 3 || catalog.archive_errors.size() != 1 ||
        catalog.records.size() != 4 ||
        catalog.archive_errors[0].archive_relative_path != "broken.sg3" ||
        catalog.records[0].id.archive_relative_path != "B.SG3" ||
        catalog.records[1].id.archive_relative_path != fs::path{"nested/a.sg3"} ||
        catalog.records[3].id.image_index != 2 ||
        catalog.records[1].id.archive_relative_path.is_absolute()) return false;
    if (!link_error && catalog.archive_count != 3) return false;
    for (const auto& record : catalog.records) {
        if (record.decode_attempted || record.decode_succeeded ||
            !record.metadata_supported) return false;
    }
    const auto& plain = catalog.records[1];
    const auto& sprite = catalog.records[2];
    const auto& bad_iso = catalog.records[3];
    if (plain.color_bounds != assets::AssetRangeStatus::InBounds ||
        plain.alpha_bounds != assets::AssetRangeStatus::OutOfBounds ||
        !plain.color_decoder_supported || plain.decoder_supported || !plain.payload_in_bounds ||
        sprite.color_bounds != assets::AssetRangeStatus::InBounds ||
        sprite.alpha_bounds != assets::AssetRangeStatus::NotPresent ||
        bad_iso.color_bounds != assets::AssetRangeStatus::OutOfBounds ||
        !bad_iso.color_decoder_supported || !bad_iso.decoder_supported ||
        bad_iso.payload_in_bounds) return false;

    const auto all = assets::matching_asset_indices(catalog, {});
    const auto counts = assets::count_assets(catalog, all);
    if (counts.records != 4 || counts.plain != 1 || counts.sprite != 1 ||
        counts.isometric != 2 || counts.unsupported != 0 || counts.with_alpha != 1 ||
        counts.color_out_of_bounds != 1 || counts.alpha_out_of_bounds != 1 ||
        counts.color_source_unavailable != 0 || counts.alpha_source_unavailable != 0 ||
        counts.type_counts.at(30) != 2) return false;
    assets::AssetFilter combined;
    combined.kind = assets::Sg3ImageKind::Plain;
    combined.image_type = 13;
    combined.archive_substring = "nested/";
    combined.group_substring = "Group";
    combined.with_alpha = true;
    combined.min_width = 2;
    combined.min_height = 2;
    const auto matching = assets::matching_asset_indices(catalog, combined);
    if (matching.size() != 1 || matching[0] != 1) return false;
    combined.with_alpha = false;
    if (!assets::matching_asset_indices(catalog, combined).empty()) return false;
    const assets::AssetCatalog again = assets::scan_asset_catalog(root);
    if (again.records.size() != catalog.records.size()) return false;
    for (std::size_t index = 0; index < catalog.records.size(); ++index) {
        if (!(again.records[index].id == catalog.records[index].id)) return false;
    }
    return true;
}

} // namespace

int main() {
    try {
        const TempDirectory temp;
        if (!run_checks(temp.path)) {
            std::cerr << "Asset catalog checks failed\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "Asset catalog test setup failed: " << failure.what() << '\n';
        return 1;
    }
}
