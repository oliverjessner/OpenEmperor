#include "maps/TerrainBindings.h"

#include "assets/Sg3ImageLoader.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <stdexcept>

namespace openemperor::maps {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::uintmax_t max_json_bytes = 256U * 1024U;
constexpr std::size_t max_assets = 16;
constexpr std::size_t max_bindings = 256;

bool under(const fs::path& root, const fs::path& path) {
    const auto relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& part : relative) if (part == "..") return false;
    return true;
}
fs::path contained_file(const fs::path& root, const fs::path& path, const char* label) {
    std::error_code ec;
    const auto resolved = fs::canonical(path, ec);
    if (ec || !under(root, resolved) || !fs::is_regular_file(resolved))
        throw std::runtime_error(std::string(label) + " is missing or escapes data directory: " + path.string());
    return resolved;
}
fs::path asset_path(const fs::path& root, const std::string& name) {
    const fs::path relative{name};
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.extension() != ".sg3" || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos)
        throw std::runtime_error("asset archive must be a relative .sg3 path");
    for (const auto& part : relative)
        if (part == ".." || part == ".") throw std::runtime_error("unsafe asset archive path");
    return contained_file(root, root / relative, "asset archive");
}
void exact_keys(const Json& value, std::initializer_list<const char*> keys) {
    if (!value.is_object() || value.size() != keys.size())
        throw std::runtime_error("terrain binding object has unexpected or missing keys");
    for (const char* key : keys) if (!value.contains(key))
        throw std::runtime_error(std::string("terrain binding key missing: ") + key);
}
std::string short_text(const Json& object, const char* key) {
    const auto value = object.at(key).get<std::string>();
    if (value.empty() || value.size() > 256) throw std::runtime_error(std::string("invalid ") + key);
    return value;
}
} // namespace

TerrainBindings load_terrain_bindings(const fs::path& data_root, const fs::path& json_path) {
    std::error_code ec;
    const auto root = fs::canonical(data_root, ec);
    if (ec || !fs::is_directory(root)) throw std::runtime_error("invalid data directory");
    const auto size = fs::file_size(json_path, ec);
    if (ec || size > max_json_bytes) throw std::runtime_error("terrain binding file missing or exceeds 256 KiB");
    std::ifstream input(json_path);
    if (!input) throw std::runtime_error("cannot open terrain binding file");
    const Json source = Json::parse(input);
    exact_keys(source, {"schema_version", "map_profile", "mode", "assets", "bindings"});
    if (source.at("schema_version") != 1 || source.at("mode") != "curated_preview" ||
        source.at("map_profile") != "emperor_map_v1_storage_grid")
        throw std::runtime_error("unsupported terrain binding schema, mode, or map profile");
    TerrainBindings result;
    result.profile = "emperor_map_v1_storage_grid";
    const auto& aliases = source.at("assets");
    const auto& entries = source.at("bindings");
    if (!aliases.is_object() || aliases.empty() || aliases.size() > max_assets ||
        !entries.is_array() || entries.size() > max_bindings)
        throw std::runtime_error("terrain binding asset/count budget exceeded or invalid");
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        if (it.key().empty() || it.key().size() > 64) throw std::runtime_error("invalid asset alias");
        exact_keys(it.value(), {"archive", "image_index", "provenance"});
        const auto name = short_text(it.value(), "archive");
        const auto index = it.value().at("image_index").get<std::uint32_t>();
        const auto archive_path = asset_path(root, name);
        const auto archive = assets::read_sg3_archive(archive_path);
        if (index >= archive.images.size()) throw std::runtime_error("asset image index outside SG3 table: " + it.key());
        const auto& image = archive.images[index];
        if (image.image_type != 30 || image.width != 78 || image.height != 40 ||
            image.uncompressed_length != 3200 || image.isometric_size_flag != 1 ||
            image.horizontal_mirror_offset != 0 || image.alpha_length != 0 ||
            image.data_length != 3200)
            throw std::runtime_error("asset is not a supported flat 78x40 Type-30 tile: " + it.key());
        const auto bitmap = assets::resolve_sg3_image_bitmap(archive_path, archive, image);
        if (bitmap.status != assets::Sg3BitmapStatus::Resolved)
            throw std::runtime_error("unsafe or unresolved .555 source for asset: " + it.key());
        const auto bitmap_path = contained_file(root, bitmap.path, "asset .555 source");
        const auto bitmap_size = fs::file_size(bitmap_path);
        if (!assets::range_within_file(image.data_offset, image.data_length, bitmap_size))
            throw std::runtime_error("asset .555 payload exceeds file bounds: " + it.key());
        result.assets.emplace(it.key(), TerrainPreviewAsset{{fs::path{name}, index}, archive_path,
                                                            short_text(it.value(), "provenance")});
    }
    std::set<std::string> binding_ids;
    for (const auto& entry : entries) {
        exact_keys(entry, {"id", "terrain_raw", "objects_raw", "asset", "expected_category",
                           "expected_rule", "provenance"});
        const auto terrain = entry.at("terrain_raw").get<std::uint32_t>();
        const auto objects = entry.at("objects_raw").get<std::uint32_t>();
        const auto alias = short_text(entry, "asset");
        if (!result.assets.contains(alias)) throw std::runtime_error("binding references unknown asset: " + alias);
        const auto category = short_text(entry, "expected_category");
        const auto rule = short_text(entry, "expected_rule");
        const auto interpretation = interpret_terrain(terrain, objects);
        if (category != category_name(interpretation.category) || rule != interpretation.rule)
            throw std::runtime_error("binding category/rule mismatch: " + short_text(entry, "id"));
        const auto id = short_text(entry, "id");
        if (!binding_ids.insert(id).second) throw std::runtime_error("duplicate terrain binding id");
        if (!result.exact.emplace(std::pair{terrain, objects}, TerrainBinding{
                id, alias, category, rule, short_text(entry, "provenance")}).second)
            throw std::runtime_error("duplicate terrain/object binding pair");
    }
    return result;
}
} // namespace openemperor::maps
