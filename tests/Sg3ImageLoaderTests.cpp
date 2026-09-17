#include "assets/Sg3ImageLoader.h"

#include <algorithm>
#include <array>
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
constexpr std::size_t image_offset = 40680;

class TempDirectory {
public:
    TempDirectory() {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("openemperor-sg3-loader-" + std::to_string(unique));
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

void u16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void u32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    u16(bytes, at, static_cast<std::uint16_t>(value));
    u16(bytes, at + 2, static_cast<std::uint16_t>(value >> 16));
}

std::vector<std::uint8_t> synthetic_sg3(const std::string& group_filename = "",
                                         bool external = false, std::uint16_t image_type = 13,
                                         std::uint32_t data_length = 8) {
    // Independently authored public SG3 metadata layout, never copied game data.
    std::vector<std::uint8_t> bytes(image_offset + 72, 0);
    u32(bytes, 0, static_cast<std::uint32_t>(bytes.size()));
    u32(bytes, 4, 214);
    u32(bytes, 12, 1);
    u32(bytes, 16, 1);
    u32(bytes, 20, 1);
    if (group_filename.size() >= 65) throw std::runtime_error("synthetic group filename is too long");
    for (std::size_t index = 0; index < group_filename.size(); ++index) {
        bytes[680 + index] = static_cast<std::uint8_t>(group_filename[index]);
    }
    u32(bytes, 680 + 124, 1);
    u32(bytes, image_offset, 4);
    u32(bytes, image_offset + 4, data_length);
    u16(bytes, image_offset + 20, 2);
    u16(bytes, image_offset + 22, 2);
    u16(bytes, image_offset + 50, image_type);
    bytes[image_offset + 52] = external ? 1 : 0;
    return bytes;
}

void write_file(const fs::path& path, std::span<const std::uint8_t> bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write synthetic test file");
}

const std::vector<std::uint8_t> bitmap{
    0xaa, 0xbb, 0xcc, 0xdd, // Payload starts at offset four.
    0x00, 0x7c, 0xe0, 0x03, 0x1f, 0x00, 0x1f, 0xf8,
    0xee, 0xff, // Trailing bytes must not become image pixels.
};
const std::vector<std::uint8_t> expected{
    255, 0, 0, 255, 0, 255, 0, 255,
    0, 0, 255, 255, 0, 0, 0, 0,
};

template <typename Operation>
bool rejects_with(Operation operation, const std::string& fragment) {
    try {
        operation();
        return false;
    } catch (const std::runtime_error& error) {
        return std::string{error.what()}.find(fragment) != std::string::npos;
    }
}

bool run_checks(const fs::path& root) {
    using openemperor::assets::AlphaAddressing;
    using openemperor::assets::Sg3ImageRequest;
    using openemperor::assets::load_sg3_image;
    using openemperor::assets::load_sg3_image_with_source;

    const fs::path internal = root / "internal.sg3";
    write_file(internal, synthetic_sg3());
    write_file(root / "internal.555", bitmap);
    const auto loaded = load_sg3_image_with_source({internal, 0});
    if (loaded.rgba.width != 2 || loaded.rgba.height != 2 || loaded.rgba.pixels != expected ||
        loaded.bitmap.ref != "internal" || loaded.bitmap.path != root / "internal.555") {
        return false;
    }
    if (!rejects_with([&] { load_sg3_image({internal, 1}); }, "index") ||
        !rejects_with([&] { load_sg3_image({internal, 0xffffffffU}); }, "index")) {
        return false;
    }
    auto invalid_group = synthetic_sg3();
    invalid_group[image_offset + 56] = 1;
    const fs::path bad_group = root / "bad-group.sg3";
    write_file(bad_group, invalid_group);
    if (!rejects_with([&] { load_sg3_image({bad_group, 0}); }, "group ID")) return false;

    const fs::path missing = root / "missing.sg3";
    write_file(missing, synthetic_sg3());
    if (!rejects_with([&] { load_sg3_image({missing, 0}); }, "missing")) return false;

    const fs::path short_file = root / "short.sg3";
    write_file(short_file, synthetic_sg3());
    write_file(root / "short.555", std::span{bitmap}.first(11));
    if (!rejects_with([&] { load_sg3_image({short_file, 0}); }, "exceeds")) return false;
    auto large_offset = synthetic_sg3();
    u32(large_offset, image_offset, 0xffffffffU);
    const fs::path overflow = root / "overflow.sg3";
    write_file(overflow, large_offset);
    write_file(root / "overflow.555", bitmap);
    if (!rejects_with([&] { load_sg3_image({overflow, 0}); }, "exceeds")) return false;

    const fs::path external = root / "external.sg3";
    write_file(external, synthetic_sg3("nested\\sprites.bmp", true));
    write_file(root / "nested" / "sprites.555", bitmap);
    const auto external_image = load_sg3_image_with_source({external, 0});
    if (external_image.rgba.pixels != expected || external_image.bitmap.ref != "group:0" ||
        external_image.bitmap.path != root / "nested" / "sprites.555") {
        return false;
    }

    const fs::path unsafe = root / "unsafe.sg3";
    write_file(unsafe, synthetic_sg3("../escape.bmp", true));
    if (!rejects_with([&] { load_sg3_image({unsafe, 0}); }, "unsafe_relative_name")) return false;
    const fs::path rooted = root / "rooted.sg3";
    write_file(rooted, synthetic_sg3("/outside.bmp", true));
    if (!rejects_with([&] { load_sg3_image({rooted, 0}); }, "unsafe_relative_name")) return false;
    const fs::path foreign_root = root / "foreign-root.sg3";
    write_file(foreign_root, synthetic_sg3("C:\\outside.bmp", true));
    if (!rejects_with([&] { load_sg3_image({foreign_root, 0}); }, "unsafe_relative_name")) return false;
    const fs::path linked_archive = root / "linked" / "image.sg3";
    write_file(linked_archive, synthetic_sg3("alias\\sprites.bmp", true));
    write_file(root / "outside" / "sprites.555", bitmap);
    std::error_code link_error;
    fs::create_directory_symlink(root / "outside", root / "linked" / "alias", link_error);
    if (!link_error &&
        !rejects_with([&] { load_sg3_image({linked_archive, 0}); }, "unsafe_relative_name")) {
        return false;
    }

    const std::vector<std::uint8_t> sprite_stream{
        255, 1, // First pixel stays transparent.
        2, 0x00, 0x7c, 0xe0, 0x03, // Red and green across the row boundary.
        255, 1, // Last pixel stays transparent.
    };
    std::vector<std::uint8_t> sprite_bitmap{0xaa, 0xbb, 0xcc, 0xdd};
    sprite_bitmap.insert(sprite_bitmap.end(), sprite_stream.begin(), sprite_stream.end());
    sprite_bitmap.push_back(0xee);
    const fs::path sprite = root / "sprite.sg3";
    write_file(sprite, synthetic_sg3("", false, 256, static_cast<std::uint32_t>(sprite_stream.size())));
    write_file(root / "sprite.555", sprite_bitmap);
    const std::vector<std::uint8_t> sprite_expected{
        0, 0, 0, 0, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 0, 0, 0,
    };
    if (load_sg3_image({sprite, 0}).pixels != sprite_expected) return false;

    auto mixed_alpha_record = synthetic_sg3("", false, 256, 3);
    u32(mixed_alpha_record, image_offset + 4, 3);
    u16(mixed_alpha_record, image_offset + 20, 1);
    u16(mixed_alpha_record, image_offset + 22, 1);
    u32(mixed_alpha_record, image_offset + 64, 10); // Profile marker: 4 + 2 * 3.
    u32(mixed_alpha_record, image_offset + 68, 2);
    const fs::path mixed_alpha_path = root / "mixed-alpha.sg3";
    write_file(mixed_alpha_path, mixed_alpha_record);
    const std::array<std::uint8_t, 12> mixed_alpha_bitmap{
        0, 0, 0, 0, 1, 0x23, 0x4a, 1, 16, 0, 0, 0};
    write_file(root / "mixed-alpha.555", mixed_alpha_bitmap);
    if (load_sg3_image({mixed_alpha_path, 0}).pixels !=
            std::vector<std::uint8_t>{148, 140, 24, 132} ||
        load_sg3_image({mixed_alpha_path, 0, true}).pixels !=
            std::vector<std::uint8_t>{148, 140, 24, 255}) return false;

    const fs::path sprite_short = root / "sprite-short.sg3";
    write_file(sprite_short, synthetic_sg3("", false, 257,
                                           static_cast<std::uint32_t>(sprite_stream.size())));
    write_file(root / "sprite-short.555", std::span{sprite_bitmap}.first(12));
    if (!rejects_with([&] { load_sg3_image({sprite_short, 0}); }, "exceeds")) return false;

    const fs::path isometric = root / "isometric.sg3";
    write_file(isometric, synthetic_sg3("", false, 30));
    if (!rejects_with([&] { load_sg3_image({isometric, 0}); }, "isometric")) return false;

    auto tile_record = synthetic_sg3("", false, 30, 3200);
    u32(tile_record, image_offset + 8, 3200); // Documented base/overlay boundary.
    u16(tile_record, image_offset + 20, 78);
    u16(tile_record, image_offset + 22, 40);
    tile_record[image_offset + 55] = 1;
    const fs::path type30 = root / "type30.sg3";
    write_file(type30, tile_record);
    std::vector<std::uint8_t> tile_bitmap(4, 0xaa);
    for (std::size_t offset = 0; offset < 3200; offset += 2) {
        tile_bitmap.push_back(0xe0);
        tile_bitmap.push_back(0x03); // Synthetic green RGB555 pixel.
    }
    tile_bitmap.push_back(0xee); // Outside the selected payload.
    write_file(root / "type30.555", tile_bitmap);
    const auto type30_result = load_sg3_image_with_source({type30, 0});
    if (type30_result.rgba.width != 78 || type30_result.rgba.height != 40 ||
        type30_result.bitmap.ref != "internal" ||
        type30_result.bitmap.path != root / "type30.555" ||
        type30_result.rgba.pixels.size() != 78U * 40U * 4U) return false;
    const auto& pixels = type30_result.rgba.pixels;
    const std::size_t apex = (38U * 4U);
    if (pixels[3] != 0 || pixels[apex] != 0 || pixels[apex + 1] != 255 ||
        pixels[apex + 2] != 0 || pixels[apex + 3] != 255) return false;

    const fs::path type30_short = root / "type30-short.sg3";
    write_file(type30_short, tile_record);
    write_file(root / "type30-short.555", std::span{tile_bitmap}.first(3203));
    if (!rejects_with([&] { load_sg3_image({type30_short, 0}); }, "exceeds")) return false;
    auto type30_alpha = tile_record;
    u32(type30_alpha, image_offset + 68, 1);
    const fs::path type30_alpha_path = root / "type30-alpha.sg3";
    u32(type30_alpha, image_offset + 64, 5000);
    u32(type30_alpha, image_offset + 68, 4);
    write_file(type30_alpha_path, type30_alpha);
    auto type30_alpha_bitmap = tile_bitmap;
    type30_alpha_bitmap.resize(5004, 0xaa);
    const std::array<std::uint8_t, 4> type30_alpha_stream{255, 38, 1, 0};
    std::copy(type30_alpha_stream.begin(), type30_alpha_stream.end(),
              type30_alpha_bitmap.begin() + 5000);
    write_file(root / "type30-alpha.555", type30_alpha_bitmap);
    if (!rejects_with([&] { load_sg3_image({type30_alpha_path, 0}); }, "unverified")) return false;
    const auto type30_masked = load_sg3_image({type30_alpha_path, 0, false, AlphaAddressing::Spec});
    if (type30_masked.pixels[apex + 1] != 255 || type30_masked.pixels[apex + 3] != 0 ||
        type30_masked.pixels[(39U * 4U) + 3U] != 255) return false;

    auto separated_record = synthetic_sg3();
    u32(separated_record, image_offset, 100);
    u32(separated_record, image_offset + 64, 500);
    const std::array<std::uint8_t, 7> plain_alpha_stream{255, 1, 2, 0, 16, 255, 1};
    u32(separated_record, image_offset + 68,
        static_cast<std::uint32_t>(plain_alpha_stream.size()));
    auto separated_bitmap = std::vector<std::uint8_t>(512, 0xaa);
    std::copy(bitmap.begin() + 4, bitmap.begin() + 12, separated_bitmap.begin() + 100);
    std::copy(plain_alpha_stream.begin(), plain_alpha_stream.end(),
              separated_bitmap.begin() + 500);
    const fs::path plain_alpha = root / "plain-alpha.sg3";
    write_file(plain_alpha, separated_record);
    write_file(root / "plain-alpha.555", separated_bitmap);
    if (!rejects_with([&] { load_sg3_image({plain_alpha, 0}); }, "unverified")) return false;
    const auto plain_masked = load_sg3_image({plain_alpha, 0, false, AlphaAddressing::Spec});
    const std::vector<std::uint8_t> masked_expected{
        255, 0, 0, 255, 0, 255, 0, 0,
        0, 0, 255, 132, 0, 0, 0, 0,
    };
    if (plain_masked.pixels != masked_expected) return false;
    if (load_sg3_image({plain_alpha, 0, true}).pixels != expected) return false;

    auto alpha_at_zero = separated_record;
    u32(alpha_at_zero, image_offset + 64, 0);
    const fs::path alpha_zero = root / "alpha-zero.sg3";
    write_file(alpha_zero, alpha_at_zero);
    std::copy(plain_alpha_stream.begin(), plain_alpha_stream.end(), separated_bitmap.begin());
    write_file(root / "alpha-zero.555", separated_bitmap);
    if (load_sg3_image({alpha_zero, 0, false, AlphaAddressing::Spec}).pixels != masked_expected) return false;

    auto no_alpha = separated_record;
    u32(no_alpha, image_offset + 64, 0xffffffffU);
    u32(no_alpha, image_offset + 68, 0);
    const fs::path no_alpha_path = root / "no-alpha.sg3";
    write_file(no_alpha_path, no_alpha);
    write_file(root / "no-alpha.555", separated_bitmap);
    if (load_sg3_image({no_alpha_path, 0}).pixels != expected) return false;

    auto bad_alpha_range = separated_record;
    u32(bad_alpha_range, image_offset + 64, 510);
    const fs::path bad_alpha_path = root / "bad-alpha.sg3";
    write_file(bad_alpha_path, bad_alpha_range);
    write_file(root / "bad-alpha.555", separated_bitmap);
    if (!rejects_with([&] { load_sg3_image({bad_alpha_path, 0, false, AlphaAddressing::Spec}); }, "alpha range exceeds")) {
        return false;
    }
    if (load_sg3_image({bad_alpha_path, 0, true}).pixels != expected) return false;
    auto bad_color_range = separated_record;
    u32(bad_color_range, image_offset, 510);
    const fs::path bad_color_path = root / "bad-color.sg3";
    write_file(bad_color_path, bad_color_range);
    write_file(root / "bad-color.555", separated_bitmap);
    if (!rejects_with([&] { load_sg3_image({bad_color_path, 0, false, AlphaAddressing::Spec}); }, "data range exceeds")) {
        return false;
    }

    auto sprite_alpha_record = synthetic_sg3("", false, 256,
                                              static_cast<std::uint32_t>(sprite_stream.size()));
    u32(sprite_alpha_record, image_offset, 100);
    u32(sprite_alpha_record, image_offset + 64, 500);
    const std::array<std::uint8_t, 7> sprite_alpha_stream{255, 1, 2, 31, 16, 255, 1};
    u32(sprite_alpha_record, image_offset + 68,
        static_cast<std::uint32_t>(sprite_alpha_stream.size()));
    auto sprite_alpha_bitmap = std::vector<std::uint8_t>(512, 0xaa);
    std::copy(sprite_stream.begin(), sprite_stream.end(), sprite_alpha_bitmap.begin() + 100);
    std::copy(sprite_alpha_stream.begin(), sprite_alpha_stream.end(),
              sprite_alpha_bitmap.begin() + 500);
    const fs::path sprite_alpha_path = root / "sprite-alpha.sg3";
    write_file(sprite_alpha_path, sprite_alpha_record);
    write_file(root / "sprite-alpha.555", sprite_alpha_bitmap);
    const std::vector<std::uint8_t> sprite_alpha_expected{
        0, 0, 0, 0, 255, 0, 0, 255,
        0, 255, 0, 132, 0, 0, 0, 0,
    };
    if (load_sg3_image({sprite_alpha_path, 0, false, AlphaAddressing::Spec}).pixels != sprite_alpha_expected) return false;

    // Observed Emperor profile: raw metadata is deliberately not the alpha payload start.
    auto contiguous_record = sprite_alpha_record;
    constexpr std::uint32_t color_offset = 100;
    const std::uint32_t color_length = static_cast<std::uint32_t>(sprite_stream.size());
    const std::uint32_t effective_alpha = color_offset + color_length;
    const std::uint32_t raw_alpha = color_offset + 2U * color_length;
    u32(contiguous_record, image_offset + 64, raw_alpha);
    auto contiguous_bitmap = std::vector<std::uint8_t>(128, 0xaa);
    std::copy(sprite_stream.begin(), sprite_stream.end(), contiguous_bitmap.begin() + color_offset);
    std::copy(sprite_alpha_stream.begin(), sprite_alpha_stream.end(),
              contiguous_bitmap.begin() + effective_alpha);
    const std::array<std::uint8_t, 7> different_raw_stream{255, 1, 2, 0, 0, 255, 1};
    std::copy(different_raw_stream.begin(), different_raw_stream.end(),
              contiguous_bitmap.begin() + raw_alpha);
    const fs::path contiguous_path = root / "contiguous-alpha.sg3";
    write_file(contiguous_path, contiguous_record);
    write_file(root / "contiguous-alpha.555", contiguous_bitmap);
    const auto production = load_sg3_image({contiguous_path, 0});
    if (production.pixels != sprite_alpha_expected ||
        production.pixels != load_sg3_image({contiguous_path, 0, false,
                                             AlphaAddressing::Contiguous}).pixels ||
        production.pixels == load_sg3_image({contiguous_path, 0, false,
                                             AlphaAddressing::Spec}).pixels) return false;

    // The raw offset can be beyond EOF while the selected effective range is valid.
    const fs::path raw_outside_path = root / "raw-outside-alpha.sg3";
    write_file(raw_outside_path, contiguous_record);
    write_file(root / "raw-outside-alpha.555",
               std::span{contiguous_bitmap}.first(effective_alpha + sprite_alpha_stream.size()));
    if (load_sg3_image({raw_outside_path, 0}).pixels != sprite_alpha_expected ||
        !rejects_with([&] { load_sg3_image({raw_outside_path, 0, false,
                                             AlphaAddressing::Spec}); }, "alpha range exceeds")) return false;

    // A valid raw range cannot imply a valid effective range for an unverified profile.
    auto unverified_record = contiguous_record;
    u32(unverified_record, image_offset + 64, 0);
    const fs::path unverified_path = root / "unverified-alpha.sg3";
    write_file(unverified_path, unverified_record);
    write_file(root / "unverified-alpha.555",
               std::span{contiguous_bitmap}.first(effective_alpha + sprite_alpha_stream.size() - 1U));
    if (!rejects_with([&] { load_sg3_image({unverified_path, 0}); }, "unverified") ||
        !rejects_with([&] { load_sg3_image({unverified_path, 0, false,
                                             AlphaAddressing::Contiguous}); }, "alpha range exceeds")) return false;

    auto malformed_bitmap = contiguous_bitmap;
    malformed_bitmap[effective_alpha] = 9; // Literal exceeds the four-pixel image.
    const fs::path malformed_path = root / "malformed-contiguous.sg3";
    write_file(malformed_path, contiguous_record);
    write_file(root / "malformed-contiguous.555", malformed_bitmap);
    if (!rejects_with([&] { load_sg3_image({malformed_path, 0}); }, "alpha literal run")) return false;

    auto external_alpha_record = synthetic_sg3("external.bmp", true, 256, color_length);
    u32(external_alpha_record, image_offset, color_offset);
    u32(external_alpha_record, image_offset + 64, raw_alpha);
    u32(external_alpha_record, image_offset + 68,
        static_cast<std::uint32_t>(sprite_alpha_stream.size()));
    const fs::path external_alpha_path = root / "external-alpha.sg3";
    write_file(external_alpha_path, external_alpha_record);
    write_file(root / "external.555", contiguous_bitmap);
    if (!rejects_with([&] { load_sg3_image({external_alpha_path, 0}); }, "unverified") ||
        load_sg3_image({external_alpha_path, 0, false,
                        AlphaAddressing::Contiguous}).pixels != sprite_alpha_expected) return false;

    const fs::path unknown = root / "unknown.sg3";
    write_file(unknown, synthetic_sg3("", false, 999));
    return rejects_with([&] { load_sg3_image({unknown, 0}); }, "unsupported SG3 image type");
}

} // namespace

int main() {
    try {
        const TempDirectory temp;
        if (!run_checks(temp.path)) {
            std::cerr << "SG3 image loader checks failed\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SG3 image loader test setup failed: " << error.what() << '\n';
        return 1;
    }
}
