#include "assets/Sg3ImageLoader.h"

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
    0x1f, 0x00, 0xe0, 0x03, 0x00, 0x7c, 0x1f, 0xf8,
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
        2, 0x1f, 0x00, 0xe0, 0x03, // Red and green across the row boundary.
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

    const fs::path sprite_short = root / "sprite-short.sg3";
    write_file(sprite_short, synthetic_sg3("", false, 257,
                                           static_cast<std::uint32_t>(sprite_stream.size())));
    write_file(root / "sprite-short.555", std::span{sprite_bitmap}.first(12));
    if (!rejects_with([&] { load_sg3_image({sprite_short, 0}); }, "exceeds")) return false;

    const fs::path isometric = root / "isometric.sg3";
    write_file(isometric, synthetic_sg3("", false, 30));
    if (!rejects_with([&] { load_sg3_image({isometric, 0}); }, "isometric")) return false;
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
