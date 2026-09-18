#include "app/Application.h"
#include "app/AssetBrowser.h"
#include "app/SceneView.h"
#include "app/MapDebugView.h"
#include "app/MapBrowser.h"
#include "app/MapRenderCheck.h"
#include "assets/AssetCatalog.h"
#include "assets/RgbaPngReader.h"
#include "assets/Sg3ImageLoader.h"
#include "scene/Scene.h"
#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/TerrainBindings.h"
#include "maps/StoredGraphicsPlan.h"
#include "maps/MapCatalog.h"
#include "maps/StoredMapSession.h"

#include <nlohmann/json.hpp>

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
              << " [--graphics-profile exe-6373328b-v213-runtime-table]"
              << " [--multi-tile-preview [--footprint-policy isolated|edge-byte]]\n";
    std::cerr << "       " << executable << " --data <directory> --browse-maps --view stored-graphics"
              << " --graphics-profile exe-6373328b-v213-runtime-table"
              << " [--multi-tile-preview [--footprint-policy isolated|edge-byte]]\n"
              << "       " << executable << " --data <directory> --list-maps --report-json\n"
              << "       " << executable << " --data <directory> --map-debug <relative.map>"
              << " --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table"
              << " [--multi-tile-preview [--footprint-policy isolated|edge-byte]]"
              << " --render-check --report-json\n";
}

} // namespace

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    bool data_supplied = false;
    bool preview_supplied = false;
    bool sg3_supplied = false;
    bool image_supplied = false;
    bool browse_assets = false;
    bool browse_maps = false;
    bool list_maps = false;
    bool render_check = false;
    bool report_json = false;
    bool ignore_alpha = false;
    bool scene_supplied = false;
    bool map_debug_supplied = false;
    bool part_supplied = false;
    bool layer_supplied = false;
    bool view_supplied = false;
    bool terrain_bindings_supplied = false;
    bool graphics_profile_supplied = false;
    bool multi_tile_preview = false;
    bool footprint_policy_supplied = false;
    auto footprint_policy = openemperor::maps::FootprintPolicy::IsolatedPreview;
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
        } else if (argument == "--browse-maps" && !browse_maps) {
            browse_maps = true;
        } else if (argument == "--list-maps" && !list_maps) {
            list_maps = true;
        } else if (argument == "--render-check" && !render_check) {
            render_check = true;
        } else if (argument == "--report-json" && !report_json) {
            report_json = true;
        } else if (argument == "--multi-tile-preview" && !multi_tile_preview) {
            multi_tile_preview = true;
        } else if (argument == "--footprint-policy" && !footprint_policy_supplied && index + 1 < argc) {
            const std::string_view value{argv[++index]};
            if (value == "isolated") footprint_policy = openemperor::maps::FootprintPolicy::IsolatedPreview;
            else if (value == "edge-byte") footprint_policy = openemperor::maps::FootprintPolicy::EdgeBytePreview;
            else { print_usage(argv[0]); return 2; }
            footprint_policy_supplied = true;
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
        ((part_supplied || layer_supplied || terrain_bindings_supplied) && !map_debug_supplied) ||
        ((view_supplied || graphics_profile_supplied) && !map_debug_supplied && !browse_maps) ||
        (map_view_mode == openemperor::maps::MapViewMode::Textured && !terrain_bindings_supplied) ||
        (terrain_bindings_supplied && map_view_mode != openemperor::maps::MapViewMode::Textured) ||
        (map_view_mode == openemperor::maps::MapViewMode::StoredGraphics && !graphics_profile_supplied) ||
        (graphics_profile_supplied && map_view_mode != openemperor::maps::MapViewMode::StoredGraphics) ||
        (footprint_policy_supplied && !multi_tile_preview) ||
        (multi_tile_preview && (!graphics_profile_supplied ||
            map_view_mode != openemperor::maps::MapViewMode::StoredGraphics)) ||
        (browse_maps && (!data_supplied || map_debug_supplied || browse_assets || scene_supplied ||
            preview_supplied || sg3_supplied || map_view_mode!=openemperor::maps::MapViewMode::StoredGraphics ||
            !graphics_profile_supplied)) ||
        (list_maps && (!data_supplied || browse_maps || map_debug_supplied || browse_assets ||
            scene_supplied || preview_supplied || sg3_supplied || view_supplied || graphics_profile_supplied)) ||
        (render_check && (!map_debug_supplied || !data_supplied || part_supplied ||
            map_view_mode!=openemperor::maps::MapViewMode::StoredGraphics || !report_json)) ||
        (report_json && !render_check && !list_maps)) {
        print_usage(argv[0]);
        return 2;
    }

    if (data_supplied) {
        std::error_code error;
        const fs::path absolute_path = fs::absolute(data_directory, error);
        if (error || !fs::is_directory(absolute_path, error) || error) {
            if (render_check)
                return openemperor::run_map_render_check(data_directory,map_debug_path,
                    multi_tile_preview ? footprint_policy : openemperor::maps::FootprintPolicy::Disabled);
            std::cerr << "Data directory does not exist or cannot be accessed: " << data_directory.string() << '\n';
            return 2;
        }
        if (!report_json)
            std::cout << "Data directory: " << absolute_path.lexically_normal().string() << '\n';
    }

    if (list_maps) {
        try {
            const auto catalog=openemperor::maps::discover_standalone_maps(data_directory);
            nlohmann::json entries=nlohmann::json::array();
            for (const auto& entry:catalog.entries)
                entries.push_back({{"relative_path",entry.relative_path.generic_string()},
                    {"file_bytes",entry.file_bytes},{"container_valid",entry.container_valid},
                    {"map_profile",entry.map_profile},{"declared_map_size",entry.declared_size},
                    {"error",entry.error},{"render_status","not_checked"}});
            std::cout<<nlohmann::json{{"schema","openemperor-map-catalog-v1"},
                {"entries",entries},{"scan_errors",catalog.scan_errors}}.dump()<<'\n';
            return catalog.scan_errors.empty() ? 0 : 1;
        } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
    }
    if (render_check)
        return openemperor::run_map_render_check(data_directory,map_debug_path,
            multi_tile_preview ? footprint_policy : openemperor::maps::FootprintPolicy::Disabled);

    std::optional<openemperor::assets::RgbaImage> preview;
    std::unique_ptr<openemperor::AssetBrowser> browser;
    std::unique_ptr<openemperor::SceneView> scene_view;
    std::unique_ptr<openemperor::MapDebugView> map_view;
    std::unique_ptr<openemperor::MapBrowser> map_browser;
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
    } else if (browse_maps) {
        try {
            map_browser=std::make_unique<openemperor::MapBrowser>(
                openemperor::maps::discover_standalone_maps(data_directory),
                multi_tile_preview ? footprint_policy : openemperor::maps::FootprintPolicy::Disabled);
        } catch (const std::exception& error) {
            std::cerr<<"Map browser scan failed: "<<error.what()<<'\n'; return 1;
        }
    } else if (map_debug_supplied) {
        try {
            if (graphics_profile_supplied && !part_supplied) {
                auto session=openemperor::maps::load_stored_map_session(data_directory,map_debug_path,
                    multi_tile_preview ? footprint_policy : openemperor::maps::FootprintPolicy::Disabled);
                std::cout << "Original map: " << map_debug_path << " part 0 storage "
                          << session.map.stored_width << 'x' << session.map.stored_height
                          << " declared size " << session.map.declared_map_size
                          << " active extent unknown\n";
                map_view=std::make_unique<openemperor::MapDebugView>(std::move(session.map),
                    map_layer,map_view_mode,std::nullopt,std::move(session.plan));
            } else {
            const fs::path map_path = openemperor::maps::resolve_map_path(data_directory, map_debug_path);
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
                    terrain_catalog,*terrain_layout,elevation_catalog,*elevation_layout,
                    multi_tile_preview ? footprint_policy : openemperor::maps::FootprintPolicy::Disabled);
            }
            map_view = std::make_unique<openemperor::MapDebugView>(
                std::move(map), map_layer, map_view_mode, std::move(bindings),std::move(stored_plan));
            }
        } catch (const std::exception& error_message) {
            std::cerr << "Map debug load failed: " << error_message.what() << '\n';
            return 1;
        }
    }

    openemperor::Application application{std::move(preview), std::move(browser),
                                       std::move(scene_view), std::move(map_view),std::move(map_browser)};
    if (!application.initialize()) {
        return 1;
    }
    const int result = application.run();
    application.shutdown();
    return result;
}
