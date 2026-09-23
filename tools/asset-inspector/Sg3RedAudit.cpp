#include "Sg3RedAudit.h"

#include "assets/Sg3Archive.h"
#include "assets/Sg3ImageLoader.h"
#include "assets/Sg3IsometricDecoder.h"
#include "assets/Sg3OmegaDecoder.h"
#include "assets/Sg3PayloadLayout.h"
#include "assets/Sg3RgbaDecoder.h"
#include "assets/Sg3ShadowComposition.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace assets = openemperor::assets;
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::uint16_t red_555 = 0x7c00;
constexpr std::uint16_t transparent_555 = 0xf81f;

struct LayerAudit {
    std::uint64_t stored_red_count = 0;
    assets::RgbaImage red_image;
};

std::uint64_t regular_file_size(const fs::path& path) {
    std::error_code error;
    if (!fs::is_regular_file(path, error) || error) {
        throw std::runtime_error("selected .555 bitmap is missing or is not a regular file");
    }
    const auto size = fs::file_size(path, error);
    if (error || size > std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("cannot determine selected .555 bitmap size");
    }
    return static_cast<std::uint64_t>(size);
}

std::vector<std::uint8_t> read_range(const fs::path& path, std::uint64_t offset,
                                     std::uint64_t length) {
    const std::uint64_t size = regular_file_size(path);
    if (!assets::range_within_file(offset, length, size)) {
        throw std::runtime_error("selected color payload exceeds the actual .555 file size");
    }
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
        length > std::numeric_limits<std::size_t>::max() ||
        length > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("selected color payload cannot be read on this platform");
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) throw std::runtime_error("cannot open selected .555 bitmap");
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw std::runtime_error("cannot seek to selected color payload");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("cannot read complete selected color payload");
    }
    return bytes;
}

std::uint16_t word_at(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

void store_word(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

LayerAudit filter_plain(std::span<const std::uint8_t> bytes,
                        const assets::Sg3Image& image) {
    if (bytes.size() % 2U != 0) throw std::runtime_error("plain RGB555 payload has odd length");
    std::vector<std::uint8_t> filtered(bytes.begin(), bytes.end());
    LayerAudit result;
    for (std::size_t offset = 0; offset < filtered.size(); offset += 2U) {
        if (word_at(filtered, offset) == red_555) ++result.stored_red_count;
        else store_word(filtered, offset, transparent_555);
    }
    result.red_image = assets::decode_uncompressed_rgba(image, filtered);
    return result;
}

LayerAudit filter_omega(std::span<const std::uint8_t> stream,
                        std::uint16_t width, std::uint16_t height) {
    std::vector<std::uint8_t> filtered(stream.begin(), stream.end());
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    std::uint64_t cursor = 0;
    std::size_t input = 0;
    LayerAudit result;
    while (input < filtered.size()) {
        if (cursor == pixel_count) {
            throw std::runtime_error("Omega stream has trailing commands after the final pixel");
        }
        const std::uint8_t command = filtered[input++];
        if (command == 255) {
            if (input == filtered.size()) throw std::runtime_error("Omega skip is truncated");
            const std::uint8_t skip = filtered[input++];
            if (skip > pixel_count - cursor) throw std::runtime_error("Omega skip exceeds image");
            cursor += skip;
            continue;
        }
        const std::size_t count = command;
        if (count > pixel_count - cursor || count * 2U > filtered.size() - input) {
            throw std::runtime_error("Omega literal run exceeds stream or image bounds");
        }
        for (std::size_t index = 0; index < count; ++index) {
            if (word_at(filtered, input) == red_555) ++result.stored_red_count;
            else store_word(filtered, input, transparent_555);
            input += 2U;
            ++cursor;
        }
    }
    result.red_image = assets::decode_omega_color_rgba(filtered, width, height);
    return result;
}

std::vector<std::uint8_t> red_mask(const assets::RgbaImage& image, bool require_opaque) {
    const std::size_t pixels = image.pixels.size() / 4U;
    std::vector<std::uint8_t> mask(pixels);
    for (std::size_t index = 0; index < pixels; ++index) {
        const std::size_t at = index * 4U;
        mask[index] = image.pixels[at] == 255 && image.pixels[at + 1] == 0 &&
            image.pixels[at + 2] == 0 && (!require_opaque || image.pixels[at + 3] == 255);
    }
    return mask;
}

Json bounding_box(const std::vector<std::uint8_t>& mask, std::uint16_t width,
                  std::uint16_t height) {
    std::uint32_t min_x = width;
    std::uint32_t min_y = height;
    std::uint32_t max_x = 0;
    std::uint32_t max_y = 0;
    bool found = false;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            if (mask[static_cast<std::size_t>(y) * width + x] == 0) continue;
            found = true;
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
    }
    if (!found) return nullptr;
    return {{"x", min_x}, {"y", min_y}, {"width", max_x - min_x + 1U},
            {"height", max_y - min_y + 1U}};
}

Json connected_components(const std::vector<std::uint8_t>& mask, std::uint16_t width,
                          std::uint16_t height) {
    std::vector<std::uint8_t> seen(mask.size());
    Json components = Json::array();
    for (std::uint32_t seed_y = 0; seed_y < height; ++seed_y) {
        for (std::uint32_t seed_x = 0; seed_x < width; ++seed_x) {
            const std::size_t seed = static_cast<std::size_t>(seed_y) * width + seed_x;
            if (mask[seed] == 0 || seen[seed] != 0) continue;
            std::queue<std::pair<std::uint32_t, std::uint32_t>> pending;
            pending.emplace(seed_x, seed_y);
            seen[seed] = 1;
            std::uint64_t count = 0;
            std::uint32_t min_x = seed_x, max_x = seed_x, min_y = seed_y, max_y = seed_y;
            while (!pending.empty()) {
                const auto [x, y] = pending.front();
                pending.pop();
                ++count;
                min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                min_y = std::min(min_y, y); max_y = std::max(max_y, y);
                const auto visit = [&](std::uint32_t next_x, std::uint32_t next_y) {
                    const std::size_t at = static_cast<std::size_t>(next_y) * width + next_x;
                    if (mask[at] != 0 && seen[at] == 0) {
                        seen[at] = 1;
                        pending.emplace(next_x, next_y);
                    }
                };
                if (x != 0) visit(x - 1U, y);
                if (x + 1U < width) visit(x + 1U, y);
                if (y != 0) visit(x, y - 1U);
                if (y + 1U < height) visit(x, y + 1U);
            }
            components.push_back({{"pixels", count}, {"bbox", {{"x", min_x}, {"y", min_y},
                {"width", max_x - min_x + 1U}, {"height", max_y - min_y + 1U}}}});
        }
    }
    return components;
}

Json image_context(const assets::Sg3Image& image, std::uint32_t index) {
    return {{"physical_record", index}, {"image_type", image.image_type},
            {"width", image.width}, {"height", image.height},
            {"data_offset", image.data_offset}, {"data_length", image.data_length},
            {"uncompressed_length", image.uncompressed_length}};
}

} // namespace

void audit_sg3_red_pixels(const fs::path& path, std::uint32_t image_index,
                          std::ostream& output) {
    const assets::Sg3Archive archive = assets::read_sg3_archive(path);
    if (image_index >= archive.images.size()) throw std::runtime_error("image index is out of range");
    const assets::Sg3Image& image = archive.images[image_index];
    if (image.width <= 0 || image.height <= 0) throw std::runtime_error("image dimensions are invalid");
    const auto bitmap = assets::resolve_sg3_image_bitmap(path, archive, image);
    if (bitmap.status != assets::Sg3BitmapStatus::Resolved) {
        throw std::runtime_error("selected .555 path cannot be resolved: " +
            std::string{assets::sg3_bitmap_status_name(bitmap.status)});
    }
    const auto layout = assets::sg3_payload_layout(archive.header.version, image);
    std::vector<std::uint8_t> payload = read_range(bitmap.path, layout.color.offset, layout.color.length);
    const assets::RgbaImage final_image = assets::load_sg3_image({path, image_index});
    const auto kind = assets::classify_sg3_image_type(image.image_type);

    LayerAudit base;
    LayerAudit overlay;
    LayerAudit sprite;
    if (kind == assets::Sg3ImageKind::Plain) {
        sprite = filter_plain(payload, image);
    } else if (kind == assets::Sg3ImageKind::Sprite) {
        sprite = filter_omega(payload, final_image.width, final_image.height);
    } else if (kind == assets::Sg3ImageKind::Isometric) {
        if (image.uncompressed_length > payload.size()) {
            throw std::runtime_error("Type-30 base length exceeds color payload");
        }
        assets::Sg3Image base_image = image;
        base_image.data_length = image.uncompressed_length;
        std::vector<std::uint8_t> filtered_base(payload.begin(),
            payload.begin() + static_cast<std::ptrdiff_t>(image.uncompressed_length));
        for (std::size_t at = 0; at < filtered_base.size(); at += 2U) {
            if (at + 1U >= filtered_base.size()) throw std::runtime_error("Type-30 base has odd length");
            if (word_at(filtered_base, at) == red_555) ++base.stored_red_count;
            else store_word(filtered_base, at, transparent_555);
        }
        base.red_image = assets::decode_isometric_rgba(base_image, filtered_base);
        overlay = filter_omega(std::span<const std::uint8_t>{payload}.subspan(image.uncompressed_length),
                               final_image.width, final_image.height);
    } else {
        throw std::runtime_error("red audit does not support this image type");
    }

    auto source_mask = std::vector<std::uint8_t>(static_cast<std::size_t>(final_image.width) * final_image.height);
    const auto merge_layer = [&](const LayerAudit& layer) {
        if (layer.red_image.pixels.empty()) return;
        const auto mask = red_mask(layer.red_image, false);
        for (std::size_t index = 0; index < mask.size(); ++index) source_mask[index] |= mask[index];
    };
    merge_layer(base); merge_layer(overlay); merge_layer(sprite);
    const auto final_red_mask = red_mask(final_image, true);
    const std::uint64_t pure_red = static_cast<std::uint64_t>(
        std::count(final_red_mask.begin(), final_red_mask.end(), std::uint8_t{1}));
    assets::RgbaImage presentation = final_image;
    const std::size_t composed_shadow_pixels =
        assets::prepare_omega_shadow_composition(image, presentation);
    const auto presented_red_mask = red_mask(presentation, true);
    const std::uint64_t presented_red = static_cast<std::uint64_t>(
        std::count(presented_red_mask.begin(), presented_red_mask.end(), std::uint8_t{1}));
    std::uint64_t visible = 0;
    for (std::size_t at = 3; at < final_image.pixels.size(); at += 4U)
        if (final_image.pixels[at] != 0) ++visible;

    Json group = nullptr;
    if (image.group_id < archive.groups.size()) {
        const auto& record = archive.groups[image.group_id];
        Json local_position = nullptr;
        if (image_index >= record.first_image_index && image_index <= record.last_image_index)
            local_position = image_index - record.first_image_index;
        group = {{"id", image.group_id}, {"filename", record.filename},
                 {"description", record.description}, {"first_image_index", record.first_image_index},
                 {"last_image_index", record.last_image_index}, {"local_position", local_position}};
    }
    Json neighbors = Json::array();
    if (image_index != 0) neighbors.push_back(image_context(archive.images[image_index - 1U], image_index - 1U));
    if (static_cast<std::size_t>(image_index) + 1U < archive.images.size())
        neighbors.push_back(image_context(archive.images[image_index + 1U], image_index + 1U));

    const std::uint64_t total_stored_red = base.stored_red_count + overlay.stored_red_count +
                                           sprite.stored_red_count;
    Json result = {
        {"format", "OpenEmperor SG3 red audit v1"},
        {"archive", path.lexically_normal().string()},
        {"bitmap", bitmap.path.lexically_normal().string()},
        {"physical_record", image_index},
        {"image", {{"type", image.image_type},
                    {"kind", assets::sg3_image_kind_name(kind)},
                    {"width", image.width}, {"height", image.height},
                    {"total_pixels", static_cast<std::uint64_t>(final_image.width) * final_image.height},
                    {"visible_pixels", visible}}},
        {"red", {{"exact_rgb555_0x7c00_count", total_stored_red},
                 {"rgba_255_0_0_255_count", pure_red},
                 {"source_bbox", bounding_box(source_mask, final_image.width, final_image.height)},
                 {"rgba_connected_components", connected_components(final_red_mask,
                                                                       final_image.width,
                                                                       final_image.height)}}},
        {"source_attribution", {{"plain_or_sprite_omega_count", sprite.stored_red_count},
                                {"type30_base_count", base.stored_red_count},
                                {"type30_omega_overlay_count", overlay.stored_red_count}}},
        {"color_payload", {{"offset", layout.color.offset}, {"length", layout.color.length},
                           {"type30_base_offset", kind == assets::Sg3ImageKind::Isometric ?
                                Json(layout.color.offset) : Json(nullptr)},
                           {"type30_base_length", kind == assets::Sg3ImageKind::Isometric ?
                                Json(image.uncompressed_length) : Json(nullptr)},
                           {"type30_overlay_offset", kind == assets::Sg3ImageKind::Isometric ?
                                Json(layout.color.offset + image.uncompressed_length) : Json(nullptr)},
                           {"type30_overlay_length", kind == assets::Sg3ImageKind::Isometric ?
                                Json(image.data_length - image.uncompressed_length) : Json(nullptr)}}},
        {"alpha", {{"present", image.alpha_length != 0}, {"stored_offset", image.alpha_offset},
                   {"length", image.alpha_length},
                   {"production_profile_supported", layout.alpha_profile_supported}}},
        {"mirror", {{"horizontal_mirror_offset", image.horizontal_mirror_offset},
                    {"applied", false}}},
        {"composition", {{"shadow_marker_flag", image.shadow_marker_flag},
                         {"record_byte", 59},
                         {"shadow_pixels_prepared", composed_shadow_pixels},
                         {"rgba_pure_red_after_presentation", presented_red},
                         {"verified_effect", image.shadow_marker_flag != 0 ?
                             "RGB555 0x7c00 darkens the existing destination in the studied runtime" :
                             "not enabled by this record"}}},
        {"animation", {{"sprites", image.animation_sprites},
                       {"x_offset", image.animation_x_offset},
                       {"y_offset", image.animation_y_offset},
                       {"speed_id", image.animation_speed_id},
                       {"reversible_flag", image.reversible_animation_flag}}},
        {"group", group},
        {"physical_neighbors", neighbors}
    };
    output << result.dump(2) << '\n';
}
