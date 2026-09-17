#pragma once

#include "assets/AssetCatalog.h"
#include "scene/IsoProjection.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace openemperor::scene {

struct Asset {
    assets::AssetId id;
    std::filesystem::path archive_path;
    Point anchor;
    int footprint = 1;
    int width = 0;
    int height = 0;
    bool emperor_type30 = false;
};
struct TerrainOverride { Cell cell; std::string asset; };
struct Object { std::string id; std::string asset; Point tile; };
struct Scene {
    int width = 0;
    int height = 0;
    std::map<std::string, Asset> assets;
    std::string default_terrain;
    std::vector<TerrainOverride> terrain_overrides;
    std::vector<Object> objects;
};

Scene load_scene(const std::filesystem::path& data_root,
                 const std::filesystem::path& manifest_path);
Point image_origin(Point tile, const Asset& asset);
std::vector<Object> sorted_objects(const Scene& scene);

} // namespace openemperor::scene
