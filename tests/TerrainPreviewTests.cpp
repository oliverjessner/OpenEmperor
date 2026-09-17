#include "maps/TerrainBindings.h"
#include "maps/TerrainRenderPlan.h"
#include "renderer/TerrainPreviewRenderer.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <set>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace maps = openemperor::maps;
using Json = nlohmann::json;

void check(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); }
void u16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}
void u32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    u16(bytes, at, static_cast<std::uint16_t>(value));
    u16(bytes, at + 2, static_cast<std::uint16_t>(value >> 16));
}
void write(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out), "write fixture");
}
void synthetic_tile(const fs::path& root, const std::string& name, std::uint16_t rgb555) {
    constexpr std::size_t at = 40680;
    std::vector<std::uint8_t> sg3(at + 72, 0), bitmap(3200, 0);
    u32(sg3, 0, static_cast<std::uint32_t>(sg3.size()));
    u32(sg3, 4, 214); u32(sg3, 12, 1); u32(sg3, 16, 1); u32(sg3, 20, 1);
    u32(sg3, 680 + 124, 1);
    u32(sg3, at + 4, 3200); u32(sg3, at + 8, 3200);
    u16(sg3, at + 20, 78); u16(sg3, at + 22, 40);
    u16(sg3, at + 50, 30); sg3[at + 55] = 1;
    for (std::size_t i = 0; i < bitmap.size(); i += 2) u16(bitmap, i, rgb555);
    write(root / (name + ".sg3"), sg3);
    write(root / (name + ".555"), bitmap);
}
Json fixture_json() {
    return {
        {"schema_version", 1}, {"map_profile", "emperor_map_v1_storage_grid"},
        {"mode", "curated_preview"},
        {"assets", {
            {"red", {{"archive", "red.sg3"}, {"image_index", 0}, {"provenance", "synthetic red"}}},
            {"same_red", {{"archive", "red.sg3"}, {"image_index", 0}, {"provenance", "same id"}}},
            {"green", {{"archive", "green.sg3"}, {"image_index", 0}, {"provenance", "synthetic green"}}}
        }},
        {"bindings", Json::array({
            {{"id", "fertile"}, {"terrain_raw", 128}, {"objects_raw", 0}, {"asset", "red"},
             {"expected_category", "fertile_hint"}, {"expected_rule", "fertility_bit_without_value"},
             {"provenance", "synthetic exact pair"}},
            {{"id", "empty"}, {"terrain_raw", 0}, {"objects_raw", 0}, {"asset", "same_red"},
             {"expected_category", "empty_by_reference"}, {"expected_rule", "no_terrain_flags"},
             {"provenance", "synthetic second pair"}},
            {{"id", "water"}, {"terrain_raw", 4}, {"objects_raw", 0}, {"asset", "green"},
             {"expected_category", "water"}, {"expected_rule", "water_without_road_or_with_flood"},
             {"provenance", "synthetic color distinction"}}
        })}
    };
}
void json_file(const fs::path& path, const Json& content) {
    std::ofstream out(path); out << content.dump(2);
    check(static_cast<bool>(out), "write JSON");
}
template <typename F> void rejects(F function, const char* message) {
    try { function(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
std::size_t index(maps::GridCell cell) {
    return static_cast<std::size_t>(cell.y) * maps::stored_grid_width + cell.x;
}
void pixel(SDL_Surface* surface, int x, int y, std::uint8_t& r, std::uint8_t& g,
           std::uint8_t& b, std::uint8_t& a) {
    check(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a), "sample pixel");
}
} // namespace

int main() {
    try {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path root = fs::temp_directory_path() / ("openemperor-terrain-preview-" + std::to_string(unique));
        fs::create_directory(root);
        struct Cleanup { fs::path path; ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); } } cleanup{root};
        synthetic_tile(root, "red", 0x7c00);
        synthetic_tile(root, "green", 0x03e0);
        const auto json_path = root / "preview.json";
        Json document = fixture_json();
        json_file(json_path, document);
        auto bindings = maps::load_terrain_bindings(root, json_path);
        check(bindings.exact.size() == 3 && bindings.assets.size() == 3, "exact bindings loaded");
        check(!bindings.exact.contains({2, 0}), "unknown pair stays unmapped");
        auto reordered = document;
        std::reverse(reordered["bindings"].begin(), reordered["bindings"].end());
        json_file(json_path, reordered);
        const auto reordered_bindings = maps::load_terrain_bindings(root, json_path);
        for (const auto& [pair, binding] : bindings.exact)
            check(reordered_bindings.exact.at(pair).asset_alias == binding.asset_alias,
                  "JSON ordering cannot change mapping");

        auto invalid = document;
        invalid["bindings"].push_back(invalid["bindings"][0]);
        json_file(json_path, invalid);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "duplicate accepted");
        invalid["bindings"][3]["id"] = "contradictory";
        invalid["bindings"][3]["asset"] = "green";
        json_file(json_path, invalid);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "contradictory pair accepted");
        invalid = document;
        invalid["bindings"][0]["expected_rule"] = "wrong";
        json_file(json_path, invalid);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "rule mismatch accepted");
        invalid = document;
        invalid["assets"]["red"]["archive"] = "../red.sg3";
        json_file(json_path, invalid);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "traversal accepted");
        invalid["assets"]["red"]["archive"] = (root / "red.sg3").string();
        json_file(json_path, invalid);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "absolute asset path accepted");
        invalid = document;
        invalid["assets"]["red"]["archive"] = "missing.sg3";
        json_file(json_path, invalid);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "missing SG3 accepted");
        std::error_code link_error;
        fs::create_symlink(fs::temp_directory_path() / "outside.sg3", root / "escape.sg3", link_error);
        if (!link_error) {
            invalid = document;
            invalid["assets"]["red"]["archive"] = "escape.sg3";
            json_file(json_path, invalid);
            rejects([&] { maps::load_terrain_bindings(root, json_path); }, "symlink escape accepted");
        }
        fs::remove(root / "green.555");
        json_file(json_path, document);
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "missing .555 accepted");
        synthetic_tile(root, "green", 0x03e0);
        write(root / "green.555", {0, 0});
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "short .555 accepted");
        synthetic_tile(root, "green", 0x03e0);
        write(root / "green.sg3", {0, 0});
        rejects([&] { maps::load_terrain_bindings(root, json_path); }, "corrupt SG3 accepted");
        synthetic_tile(root, "green", 0x03e0);
        const auto outside_bitmap = root.parent_path() / (root.filename().string() + "-outside.555");
        fs::rename(root / "green.555", outside_bitmap);
        fs::create_symlink(outside_bitmap, root / "green.555", link_error);
        if (!link_error)
            rejects([&] { maps::load_terrain_bindings(root, json_path); }, ".555 symlink escape accepted");
        fs::remove(root / "green.555");
        fs::remove(outside_bitmap);
        synthetic_tile(root, "green", 0x03e0);
        bindings = maps::load_terrain_bindings(root, json_path);

        maps::ParsedEmperorMap map;
        map.declared_map_size = 84;
        constexpr auto cells = static_cast<std::size_t>(maps::stored_grid_width) * maps::stored_grid_height;
        map.terrain_raw.values.assign(cells, 128);
        map.objects_raw.values.assign(cells, 0);
        const maps::GridCell red{114,114}, green{115,114}, unmapped{114,115}, empty{113,114};
        map.terrain_raw.values[index(green)] = 4;
        map.terrain_raw.values[index(unmapped)] = 2;
        map.terrain_raw.values[index(empty)] = 0;
        const maps::MapGeometry geometry{84};
        check(geometry.contains(red) && geometry.contains(green) && geometry.contains(unmapped), "test cells in mask");
        auto plan = maps::make_terrain_render_plan(map, geometry, bindings);
        check(plan.counts.candidate == 3612 && plan.instances.size() == 3612 &&
              plan.counts.bound == 3611 && plan.counts.unmapped == 1 &&
              plan.counts.excluded == cells - 3612, "candidate count and exact mapping");
        check(plan.counts.by_binding.at("fertile") == 3609 &&
              plan.counts.by_binding.at("water") == 1 && plan.counts.by_binding.at("empty") == 1,
              "binding counts");
        check(plan.status_by_storage[index(unmapped)] == maps::PreviewStatus::Unmapped &&
              plan.status_by_storage[0] == maps::PreviewStatus::ExcludedByPreviewMask, "status coverage");
        std::set<std::size_t> unique_cells;
        bool clipped_minimap_edge_retained = false;
        for (const auto& instance : plan.instances) {
            check(unique_cells.insert(index(instance.storage)).second, "duplicate terrain instance");
            if (!geometry.projected_origin(instance.storage)) clipped_minimap_edge_retained = true;
        }
        check(clipped_minimap_edge_retained, "minimap-clipped edge cell retained in textured plan");
        auto discrepant = map;
        discrepant.terrain_raw.values[index(red)] = 0x80000U;
        const auto discrepant_plan = maps::make_terrain_render_plan(discrepant, geometry, bindings);
        check(discrepant_plan.counts.candidate == 3612 &&
              discrepant_plan.status_by_storage[index(red)] == maps::PreviewStatus::Unmapped &&
              maps::pick_terrain_cell({0, 1700}, geometry) == red,
              "candidate/off-map disagreement is retained and selectable");
        const auto at = maps::terrain_world(red, geometry.border);
        const auto east = maps::terrain_world(green, geometry.border);
        const auto south = maps::terrain_world(unmapped, geometry.border);
        const auto origin = maps::terrain_image_origin(at);
        check(at.x == 0 && at.y == 1680 && east.x - at.x == 40 && east.y - at.y == 20 &&
              south.x - at.x == -40 && south.y - at.y == 20 && origin.x == -39 && origin.y == 1680,
              "fixed isometric reference positions and anchor");
        check(maps::pick_terrain_cell({at.x, at.y + 20}, geometry) == red &&
              maps::pick_terrain_cell({east.x, east.y + 20}, geometry) == green,
              "world picking");
        openemperor::scene::Camera2D camera;
        camera.viewport_width = 200; camera.viewport_height = 160;
        camera.center_on({0, 1700});
        auto screen = camera.world_to_screen({east.x, east.y + 20});
        check(maps::pick_terrain_cell(camera.screen_to_world(screen), geometry) == green,
              "camera picking");
        camera.zoom = 2; camera.center_on({0,1700});
        screen = camera.world_to_screen({east.x, east.y + 20});
        check(maps::pick_terrain_cell(camera.screen_to_world(screen), geometry) == green,
              "zoom picking");
        check(!maps::pick_terrain_cell({-100000, -100000}, geometry), "outside pick rejected");
        check(openemperor::terrain_rect_visible(origin, camera), "viewport hit");
        camera.center_on({100000, 100000});
        check(!openemperor::terrain_rect_visible(origin, camera), "viewport culling");

        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy") && SDL_Init(SDL_INIT_VIDEO), "SDL dummy init");
        SDL_Surface* surface = SDL_CreateSurface(200, 160, SDL_PIXELFORMAT_RGBA32);
        check(surface != nullptr, "software surface");
        SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
        check(sdl != nullptr, "software renderer");
        auto failure_plan = plan;
        auto failure_bindings = bindings;
        fs::remove(root / "green.555");
        openemperor::TerrainPreviewRenderer failure_renderer{std::move(failure_plan),
                                                              std::move(failure_bindings)};
        rejects([&] { failure_renderer.initialize(sdl); }, "runtime missing asset accepted");
        check(failure_renderer.plan().counts.asset_error == 1 &&
              failure_renderer.plan().status_by_storage[index(green)] == maps::PreviewStatus::AssetError,
              "asset_error is distinct from unmapped");
        failure_renderer.shutdown();
        synthetic_tile(root, "green", 0x03e0);
        openemperor::TerrainPreviewRenderer renderer{std::move(plan), std::move(bindings)};
        renderer.initialize(sdl);
        check(renderer.texture_count() == 2 && renderer.upload_count() == 2, "deduplicated source uploads");
        camera.zoom = 1; camera.center_on({0, 1700});
        check(SDL_SetRenderDrawColor(sdl, 22,26,32,255) && SDL_RenderClear(sdl), "clear");
        check(renderer.render(camera, std::nullopt), "render terrain");
        check(SDL_RenderPresent(sdl), "present software frame");
        check(renderer.last_drawn_instances() < renderer.plan().counts.candidate,
              "viewport culls instances");
        std::uint8_t r=0,g=0,b=0,a=0;
        auto sample = [&](maps::GridCell cell) {
            const auto world = maps::terrain_world(cell, geometry.border);
            const auto point = camera.world_to_screen({world.x, world.y + 20});
            pixel(surface, static_cast<int>(point.x), static_cast<int>(point.y), r,g,b,a);
        };
        sample(red); if (!(r == 255 && g == 0 && b == 0 && a == 255))
            std::cerr << "red sample=" << int(r) << ',' << int(g) << ',' << int(b) << ',' << int(a) << '\n';
        check(r == 255 && g == 0 && b == 0 && a == 255, "red tile at correct cell");
        sample(green); check(r == 0 && g == 255 && b == 0 && a == 255, "green tile at correct cell");
        sample(unmapped); check(r > 60 && b > 60 && a == 255, "unmapped diagnostic visible");
        const auto uploads = renderer.upload_count();
        camera.offset.x += 10;
        check(renderer.render(camera, std::nullopt) && renderer.upload_count() == uploads,
              "camera motion does not upload textures");
        camera.center_on({0, 1700});
        auto isolated = renderer.plan();
        isolated.instances.erase(std::remove_if(isolated.instances.begin(), isolated.instances.end(),
            [&](const auto& instance) { return instance.storage != red; }), isolated.instances.end());
        const auto single_asset = openemperor::assets::AssetId{"red.sg3", 0};
        maps::TerrainBindings isolated_bindings;
        isolated_bindings.assets.emplace("red", maps::TerrainPreviewAsset{single_asset, root / "red.sg3", "synthetic"});
        openemperor::TerrainPreviewRenderer isolated_renderer{std::move(isolated), std::move(isolated_bindings)};
        isolated_renderer.initialize(sdl);
        check(SDL_SetRenderDrawColor(sdl, 22,26,32,255) && SDL_RenderClear(sdl) &&
              isolated_renderer.render(camera, std::nullopt) && SDL_RenderPresent(sdl),
              "isolated alpha tile render");
        const auto corner = camera.world_to_screen(maps::terrain_image_origin(at));
        pixel(surface, static_cast<int>(corner.x), static_cast<int>(corner.y), r,g,b,a);
        check(r == 22 && g == 26 && b == 32, "transparent tile corner remains background");
        isolated_renderer.shutdown();
        renderer.shutdown();
        SDL_DestroyRenderer(sdl); SDL_DestroySurface(surface); SDL_Quit();
    } catch (const std::exception& error) {
        std::cerr << "terrain preview test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
