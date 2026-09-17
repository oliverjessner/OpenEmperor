#include "app/Application.h"
#include "app/AssetBrowser.h"
#include "app/SceneView.h"
#include "app/MapDebugView.h"
#include "assets/AssetCatalog.h"
#include "assets/RgbaPngReader.h"
#include "assets/Sg3ImageLoader.h"
#include "scene/Scene.h"
#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/TerrainBindings.h"
#include "maps/StoredGraphicsPlan.h"

#include <SDL3/SDL_main.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {

void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable << " [--data <directory>] [--preview <exported.png>]\n"
              << "       " << executable << " [--data <directory>] --sg3 <file.sg3> --image <index> [--ignore-alpha]\n"
              << "       " << executable << " --sg3 <file.sg3> --image <index>"
              << " --alpha-addressing spec|contiguous|legacy (diagnostic)\n"
              << "       " << executable << " --data <directory> --browse-assets"
              << " [--kind plain|sprite|isometric] [--ignore-alpha]\n"
              << "       " << executable << " --data <directory> --scene <scene.json>\n"
              << "       " << executable << " --data <directory> --map-debug <relative.map>"
              << " [--part <index>] [--layer terrain_raw|objects_raw]"
              << " [--view storage|semantic|projected|textured|stored-graphics]"
              << " [--terrain-bindings <preview.json>]"
              << " [--graphics-profile exe-6373328b-v213-runtime-table]\n";
}

std::filesystem::path resolve_map_path(const std::filesystem::path& root_path,
                                       const std::filesystem::path& requested) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = fs::canonical(root_path, ec);
    if (ec) throw std::runtime_error("cannot resolve data directory");
    const fs::path candidate = fs::canonical(requested.is_absolute() ? requested : root / requested, ec);
    if (ec || !fs::is_regular_file(candidate))
        throw std::runtime_error("map file is missing or cannot be resolved");
    const fs::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) throw std::runtime_error("map file escapes data directory");
    for (const auto& component : relative)
        if (component == "..") throw std::runtime_error("map file escapes data directory");
    return candidate;
}

} // namespace

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    bool data_supplied = false;
    bool preview_supplied = false;
    bool sg3_supplied = false;
    bool image_supplied = false;
    bool browse_assets = false;
    bool ignore_alpha = false;
    bool scene_supplied = false;
    bool map_debug_supplied = false;
    bool part_supplied = false;
    bool layer_supplied = false;
    bool view_supplied = false;
    bool terrain_bindings_supplied = false;
    bool graphics_profile_supplied = false;
    std::optional<openemperor::assets::AlphaAddressing> diagnostic_alpha_addressing;
    std::optional<openemperor::assets::Sg3ImageKind> browser_kind;
    fs::path data_directory;
    fs::path preview_path;
    fs::path sg3_path;
    fs::path scene_path;
    fs::path map_debug_path;
    fs::path terrain_bindings_path;
    std::uint32_t image_index = 0;
    std::uint32_t map_part = 0;
    openemperor::maps::RawLayer map_layer = openemperor::maps::RawLayer::Terrain;
    openemperor::maps::MapViewMode map_view_mode = openemperor::maps::MapViewMode::Storage;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--browse-assets" && !browse_assets) {
            browse_assets = true;
        } else if (argument == "--ignore-alpha" && !ignore_alpha) {
            ignore_alpha = true;
        } else if (argument == "--alpha-addressing" && !diagnostic_alpha_addressing && index + 1 < argc) {
            const std::string_view value{argv[++index]};
            if (value == "spec") diagnostic_alpha_addressing = openemperor::assets::AlphaAddressing::Spec;
            else if (value == "contiguous") diagnostic_alpha_addressing = openemperor::assets::AlphaAddressing::Contiguous;
            else if (value == "legacy") diagnostic_alpha_addressing = openemperor::assets::AlphaAddressing::Legacy;
            else { print_usage(argv[0]); return 2; }
        } else if (index + 1 >= argc) {
            print_usage(argv[0]);
            return 2;
        } else if (argument == "--data" && !data_supplied) {
            data_directory = argv[++index];
            data_supplied = true;
        } else if (argument == "--preview" && !preview_supplied) {
            preview_path = argv[++index];
            preview_supplied = true;
        } else if (argument == "--sg3" && !sg3_supplied) {
            sg3_path = argv[++index];
            sg3_supplied = true;
        } else if (argument == "--scene" && !scene_supplied) {
            scene_path = argv[++index];
            scene_supplied = true;
        } else if (argument == "--map-debug" && !map_debug_supplied) {
            map_debug_path = argv[++index];
            map_debug_supplied = true;
        } else if (argument == "--terrain-bindings" && !terrain_bindings_supplied) {
            terrain_bindings_path = argv[++index];
            terrain_bindings_supplied = true;
        } else if (argument == "--graphics-profile" && !graphics_profile_supplied) {
            if (std::string_view{argv[++index]} != openemperor::maps::stored_graphics_profile) {
                std::cerr << "Unsupported stored graphics profile\n";
                return 2;
            }
            graphics_profile_supplied = true;
        } else if (argument == "--part" && !part_supplied) {
            const std::string_view value{argv[++index]};
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), map_part);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                std::cerr << "Map part must be a nonnegative integer\n";
                return 2;
            }
            part_supplied = true;
        } else if (argument == "--layer" && !layer_supplied) {
            const std::string_view value{argv[++index]};
            if (value == "terrain_raw") map_layer = openemperor::maps::RawLayer::Terrain;
            else if (value == "objects_raw") map_layer = openemperor::maps::RawLayer::Objects;
            else { print_usage(argv[0]); return 2; }
            layer_supplied = true;
        } else if (argument == "--view" && !view_supplied) {
            const std::string_view value{argv[++index]};
            if (value == "storage") map_view_mode = openemperor::maps::MapViewMode::Storage;
            else if (value == "semantic") map_view_mode = openemperor::maps::MapViewMode::Semantic;
            else if (value == "projected") map_view_mode = openemperor::maps::MapViewMode::Projected;
            else if (value == "textured") map_view_mode = openemperor::maps::MapViewMode::Textured;
            else if (value == "stored-graphics") map_view_mode = openemperor::maps::MapViewMode::StoredGraphics;
            else { print_usage(argv[0]); return 2; }
            view_supplied = true;
        } else if (argument == "--image" && !image_supplied) {
            const std::string_view text{argv[++index]};
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), image_index);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
                std::cerr << "Image index must be a nonnegative integer\n";
                return 2;
            }
            image_supplied = true;
        } else if (argument == "--kind" && !browser_kind) {
            const std::string_view kind{argv[++index]};
            if (kind == "plain") browser_kind = openemperor::assets::Sg3ImageKind::Plain;
            else if (kind == "sprite") browser_kind = openemperor::assets::Sg3ImageKind::Sprite;
            else if (kind == "isometric") browser_kind = openemperor::assets::Sg3ImageKind::Isometric;
            else { print_usage(argv[0]); return 2; }
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }
    if ((sg3_supplied != image_supplied) || (preview_supplied && sg3_supplied) ||
        (browse_assets && (!data_supplied || preview_supplied || sg3_supplied)) ||
        (browser_kind && !browse_assets) || (ignore_alpha && !browse_assets && !sg3_supplied) ||
        (diagnostic_alpha_addressing && (!sg3_supplied || ignore_alpha || browse_assets)) ||
        (scene_supplied && (!data_supplied || preview_supplied || sg3_supplied || browse_assets ||
                            ignore_alpha || diagnostic_alpha_addressing || browser_kind || map_debug_supplied)) ||
        (map_debug_supplied && (!data_supplied || preview_supplied || sg3_supplied || browse_assets ||
                                scene_supplied || ignore_alpha || diagnostic_alpha_addressing || browser_kind)) ||
        ((part_supplied || layer_supplied || view_supplied || terrain_bindings_supplied ||
          graphics_profile_supplied) && !map_debug_supplied) ||
        (map_view_mode == openemperor::maps::MapViewMode::Textured && !terrain_bindings_supplied) ||
        (terrain_bindings_supplied && map_view_mode != openemperor::maps::MapViewMode::Textured) ||
        (map_view_mode == openemperor::maps::MapViewMode::StoredGraphics && !graphics_profile_supplied) ||
        (graphics_profile_supplied && map_view_mode != openemperor::maps::MapViewMode::StoredGraphics)) {
        print_usage(argv[0]);
        return 2;
    }

    if (data_supplied) {
        std::error_code error;
        const fs::path absolute_path = fs::absolute(data_directory, error);
        if (error || !fs::is_directory(absolute_path, error) || error) {
            std::cerr << "Data directory does not exist or cannot be accessed: " << data_directory.string() << '\n';
            return 2;
        }
        std::cout << "Data directory: " << absolute_path.lexically_normal().string() << '\n';
    }

    std::optional<openemperor::assets::RgbaImage> preview;
    std::unique_ptr<openemperor::AssetBrowser> browser;
    std::unique_ptr<openemperor::SceneView> scene_view;
    std::unique_ptr<openemperor::MapDebugView> map_view;
    if (preview_supplied) {
        try {
            preview = openemperor::assets::read_exported_png(preview_path);
        } catch (const std::exception& error_message) {
            std::cerr << "PNG preview failed: " << error_message.what() << '\n';
            return 1;
        }
        std::cout << "Preview image: " << fs::absolute(preview_path).lexically_normal().string()
                  << " (" << preview->width << 'x' << preview->height << ")\n";
    } else if (sg3_supplied) {
        try {
            preview = openemperor::assets::load_sg3_image(
                {sg3_path, image_index, ignore_alpha, diagnostic_alpha_addressing});
        } catch (const std::exception& error_message) {
            std::cerr << "SG3 image load failed: " << error_message.what() << '\n';
            return 1;
        }
        std::error_code error;
        const fs::path absolute_path = fs::absolute(sg3_path, error);
        std::cout << "SG3 image: " << (error ? sg3_path : absolute_path).lexically_normal().string()
                  << " index " << image_index << " (" << preview->width << 'x' << preview->height << ")\n";
    } else if (browse_assets) {
        try {
            auto catalog = openemperor::assets::scan_asset_catalog(data_directory);
            std::cout << "Asset catalog: " << catalog.archive_count << " SG3 archives, "
                      << catalog.records.size() << " image records, "
                      << catalog.archive_errors.size() << " archive errors\n";
            browser = std::make_unique<openemperor::AssetBrowser>(
                std::move(catalog), ignore_alpha, browser_kind);
        } catch (const std::exception& error_message) {
            std::cerr << "Asset browser scan failed: " << error_message.what() << '\n';
            return 1;
        }
    } else if (scene_supplied) {
        try {
            scene_view = std::make_unique<openemperor::SceneView>(
                openemperor::scene::load_scene(data_directory, scene_path));
        } catch (const std::exception& error_message) {
            std::cerr << "Scene load failed: " << error_message.what() << '\n';
            return 1;
        }
    } else if (map_debug_supplied) {
        try {
            const fs::path map_path = resolve_map_path(data_directory, map_debug_path);
            auto container = openemperor::maps::EmperorContainer::open(map_path);
            if (container.multipart() && !part_supplied)
                throw std::runtime_error("multipart container requires --part <index>");
            auto map = openemperor::maps::read_emperor_map(container, map_part);
            std::cout << "Original map: " << map_path << " part " << map_part << " storage "
                      << map.stored_width << 'x' << map.stored_height << " declared size "
                      << map.declared_map_size << " active extent unknown\n";
            std::optional<openemperor::maps::TerrainBindings> bindings;
            if (terrain_bindings_supplied)
                bindings = openemperor::maps::load_terrain_bindings(data_directory, terrain_bindings_path);
            std::optional<openemperor::maps::StoredGraphicsPlan> stored_plan;
            if (graphics_profile_supplied) {
                namespace maps = openemperor::maps;
                namespace assets = openemperor::assets;
                const fs::path terrain_relative = "DATA/China_Terrain.sg3";
                const fs::path elevation_relative = "DATA/China_Elevation.sg3";
                const auto terrain_path = maps::validate_stored_archive_sources(data_directory,terrain_relative);
                const auto elevation_path = maps::validate_stored_archive_sources(data_directory,elevation_relative);
                const auto terrain_archive = assets::read_sg3_archive(terrain_path);
                const auto elevation_archive = assets::read_sg3_archive(elevation_path);
                const auto terrain_layout = maps::build_runtime_archive_layout(3,terrain_archive);
                const auto elevation_layout = maps::build_runtime_archive_layout(16,elevation_archive);
                if (!terrain_layout || !elevation_layout)
                    throw std::runtime_error("unsupported v213 Terrain/Elevation runtime layout");
                const auto terrain_catalog = assets::scan_asset_archive(data_directory,terrain_relative);
                const auto elevation_catalog = assets::scan_asset_archive(data_directory,elevation_relative);
                const auto candidates = maps::read_map_graphic_candidates(container,map_part);
                const maps::MapGeometry geometry{map.declared_map_size};
                stored_plan = maps::make_stored_graphics_plan(map,candidates,geometry,
                    terrain_catalog,*terrain_layout,elevation_catalog,*elevation_layout);
            }
            map_view = std::make_unique<openemperor::MapDebugView>(
                std::move(map), map_layer, map_view_mode, std::move(bindings),std::move(stored_plan));
        } catch (const std::exception& error_message) {
            std::cerr << "Map debug load failed: " << error_message.what() << '\n';
            return 1;
        }
    }

    openemperor::Application application{std::move(preview), std::move(browser),
                                       std::move(scene_view), std::move(map_view)};
    if (!application.initialize()) {
        return 1;
    }
    const int result = application.run();
    application.shutdown();
    return result;
}
