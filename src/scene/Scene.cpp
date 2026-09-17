#include "scene/Scene.h"

#include "assets/Sg3ImageLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace openemperor::scene {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

int bounded_int(const Json& value, int low, int high, const char* name) {
    if (!value.is_number_integer()) throw std::runtime_error(std::string{name} + " must be an integer");
    const auto number = value.get<std::int64_t>();
    if (number < low || number > high) throw std::runtime_error(std::string{name} + " is outside its limit");
    return static_cast<int>(number);
}
double finite_number(const Json& value, const char* name) {
    if (!value.is_number()) throw std::runtime_error(std::string{name} + " must be a number");
    const double result = value.get<double>();
    if (!std::isfinite(result) || std::abs(result) > 8192.0)
        throw std::runtime_error(std::string{name} + " must be finite and within 8192");
    return result;
}
std::string short_name(const Json& value, const char* name) {
    if (!value.is_string()) throw std::runtime_error(std::string{name} + " must be a string");
    auto result = value.get<std::string>();
    if (result.empty() || result.size() > 128) throw std::runtime_error(std::string{name} + " length is invalid");
    return result;
}
bool beneath(const fs::path& root, const fs::path& path) {
    const auto rel = path.lexically_relative(root);
    if (rel.empty() || rel.is_absolute()) return false;
    for (const auto& part : rel) if (part == "..") return false;
    return true;
}
fs::path safe_archive(const fs::path& root, const std::string& name) {
    fs::path relative{name};
    if (name.size() > 512 || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos || relative.empty() ||
        relative.is_absolute() || relative.has_root_name() ||
        relative.extension() != ".sg3") throw std::runtime_error("unsafe or non-SG3 asset path: " + name);
    for (const auto& part : relative) if (part == "..")
        throw std::runtime_error("unsafe asset path: " + name);
    std::error_code ec;
    const fs::path resolved = fs::canonical(root / relative, ec);
    if (ec || !beneath(root, resolved) || !fs::is_regular_file(resolved))
        throw std::runtime_error("asset path escapes data root or is missing: " + name);
    return resolved;
}
void validate_bitmap(const fs::path& root, const fs::path& archive_path,
                     const assets::Sg3Archive& archive, const assets::Sg3Image& image,
                     const std::string& id) {
    const auto source = assets::resolve_sg3_image_bitmap(archive_path, archive, image);
    if (source.status != assets::Sg3BitmapStatus::Resolved)
        throw std::runtime_error("unresolvable .555 for " + id);
    std::error_code ec;
    const fs::path resolved = fs::canonical(source.path, ec);
    if (ec || !beneath(root, resolved) || !fs::is_regular_file(resolved))
        throw std::runtime_error(".555 path escapes data root or is missing for " + id);
}
} // namespace

Scene load_scene(const fs::path& data_root, const fs::path& manifest_path) {
    std::error_code ec;
    const fs::path root = fs::canonical(data_root, ec);
    if (ec || !fs::is_directory(root)) throw std::runtime_error("data root is not a directory");
    const auto manifest_size = fs::file_size(manifest_path, ec);
    if (ec || manifest_size > 1024U * 1024U) throw std::runtime_error("scene manifest is missing or exceeds 1 MiB");
    std::ifstream input{manifest_path};
    if (!input) throw std::runtime_error("cannot open scene manifest");
    Json json = Json::parse(input);
    if (!json.is_object() || bounded_int(json.at("schema_version"), 1, 1, "schema_version") != 1)
        throw std::runtime_error("unsupported scene schema");
    const auto& grid = json.at("grid");
    if (bounded_int(grid.at("cell_width"), 80, 80, "cell_width") != 80 ||
        bounded_int(grid.at("cell_height"), 40, 40, "cell_height") != 40)
        throw std::runtime_error("unsupported cell geometry");
    Scene result;
    result.width = bounded_int(grid.at("width"), 1, 32, "grid width");
    result.height = bounded_int(grid.at("height"), 1, 32, "grid height");
    const auto& aliases = json.at("assets");
    if (!aliases.is_object() || aliases.empty() || aliases.size() > 64)
        throw std::runtime_error("scene needs 1..64 asset aliases");
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        const std::string alias = it.key();
        if (alias.empty() || alias.size() > 128) throw std::runtime_error("invalid asset alias");
        const Json& value = it.value();
        Asset asset;
        const std::string relative = short_name(value.at("sg3"), "sg3");
        asset.archive_path = safe_archive(root, relative);
        asset.id.archive_relative_path = fs::path{relative}.lexically_normal();
        asset.id.image_index = static_cast<std::uint32_t>(bounded_int(value.at("image_index"), 0, 1000000, "image_index"));
        const auto& anchor = value.at("anchor");
        if (!anchor.is_array() || anchor.size() != 2) throw std::runtime_error("anchor must have two numbers");
        asset.anchor = {finite_number(anchor[0], "anchor x"), finite_number(anchor[1], "anchor y")};
        asset.footprint = bounded_int(value.value("footprint", Json(1)), 1, 4, "footprint");
        const std::string id = relative + "#" + std::to_string(asset.id.image_index);
        try {
            const auto archive = assets::read_sg3_archive(asset.archive_path);
            if (asset.id.image_index >= archive.images.size()) throw std::runtime_error("image index out of range");
            const auto& image = archive.images[asset.id.image_index];
            validate_bitmap(root, asset.archive_path, archive, image, id);
            asset.width = image.width;
            asset.height = image.height;
            if (image.image_type == 30) {
                asset.emperor_type30 = true;
                const int n = asset.footprint;
                if (asset.width != 80 * n - 2 || asset.height < 40 * n ||
                    image.uncompressed_length != static_cast<std::uint32_t>(3200 * n * n))
                    throw std::runtime_error("Type-30 asset lacks declared Emperor footprint geometry");
                if (asset.anchor.x != asset.width / 2.0 || asset.anchor.y != asset.height - 40.0 * n)
                    throw std::runtime_error("Type-30 anchor does not match footprint corner");
            } else if (value.contains("footprint")) {
                throw std::runtime_error("footprint is only declared for Type-30 assets");
            }
            if (asset.width <= 0 || asset.height <= 0 ||
                static_cast<std::uint64_t>(asset.width) * static_cast<std::uint64_t>(asset.height) * 4U >
                    16U * 1024U * 1024U)
                throw std::runtime_error("image exceeds 16 MiB texture budget");
        } catch (const std::exception& error) {
            throw std::runtime_error("asset " + id + ": " + error.what());
        }
        result.assets.emplace(alias, std::move(asset));
    }
    const auto& terrain = json.at("terrain");
    result.default_terrain = short_name(terrain.at("default"), "terrain default");
    const auto default_it = result.assets.find(result.default_terrain);
    if (default_it == result.assets.end() || !default_it->second.emperor_type30 ||
        default_it->second.footprint != 1 ||
        default_it->second.width != 78 || default_it->second.height != 40)
        throw std::runtime_error("default terrain must be a declared 1x1 Emperor tile");
    const auto overrides = terrain.value("overrides", Json::array());
    if (!overrides.is_array() || overrides.size() > 1024) throw std::runtime_error("too many terrain overrides");
    std::set<std::pair<int,int>> terrain_cells;
    for (const auto& value : overrides) {
        TerrainOverride item{{bounded_int(value.at("x"), 0, result.width - 1, "terrain x"),
                              bounded_int(value.at("y"), 0, result.height - 1, "terrain y")},
                             short_name(value.at("asset"), "terrain asset")};
        const auto found = result.assets.find(item.asset);
        if (found == result.assets.end() || !found->second.emperor_type30 ||
            found->second.width != 78 || found->second.height != 40 ||
            found->second.footprint != 1 || !terrain_cells.emplace(item.cell.x, item.cell.y).second)
            throw std::runtime_error("invalid or duplicate terrain override");
        result.terrain_overrides.push_back(std::move(item));
    }
    const auto& objects = json.at("objects");
    if (!objects.is_array() || objects.size() > 256) throw std::runtime_error("too many objects");
    std::set<std::string> ids;
    for (const auto& value : objects) {
        Object item{short_name(value.at("id"), "object id"),
                    short_name(value.at("asset"), "object asset"),
                    {finite_number(value.at("x"), "object x"), finite_number(value.at("y"), "object y")}};
        if (!ids.insert(item.id).second) throw std::runtime_error("duplicate object instance ID: " + item.id);
        const auto found = result.assets.find(item.asset);
        if (found == result.assets.end()) throw std::runtime_error("unknown object asset: " + item.asset);
        const auto& asset = found->second;
        const bool invalid = asset.emperor_type30
            ? (std::floor(item.tile.x) != item.tile.x || std::floor(item.tile.y) != item.tile.y ||
               item.tile.x < 0 || item.tile.y < 0 ||
               item.tile.x + asset.footprint > result.width ||
               item.tile.y + asset.footprint > result.height)
            : (item.tile.x < 0 || item.tile.y < 0 || item.tile.x >= result.width ||
               item.tile.y >= result.height);
        if (invalid)
            throw std::runtime_error("object outside grid: " + item.id);
        result.objects.push_back(std::move(item));
    }
    return result;
}

Point image_origin(Point tile, const Asset& asset) {
    const auto point = project(tile);
    return {point.x - asset.anchor.x, point.y - asset.anchor.y};
}
std::vector<Object> sorted_objects(const Scene& scene) {
    auto objects = scene.objects;
    std::stable_sort(objects.begin(), objects.end(), [&](const Object& a, const Object& b) {
        const auto& aa = scene.assets.at(a.asset);
        const auto& bb = scene.assets.at(b.asset);
        const double da = a.tile.x + a.tile.y + 2.0 * (aa.footprint - 1);
        const double db = b.tile.x + b.tile.y + 2.0 * (bb.footprint - 1);
        if (da != db) return da < db;
        return a.id < b.id;
    });
    return objects;
}

} // namespace openemperor::scene
