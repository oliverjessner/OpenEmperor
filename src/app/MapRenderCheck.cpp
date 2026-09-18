#include "app/MapRenderCheck.h"

#include "app/MapDebugView.h"
#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/MapCatalog.h"
#include "maps/MapGeometry.h"
#include "maps/StoredMapSession.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace openemperor {
namespace {
using Json=nlohmann::json;
struct QuietStdout {
    std::streambuf* previous=std::cout.rdbuf(std::cerr.rdbuf());
    ~QuietStdout() { std::cout.rdbuf(previous); }
};
struct SdlCheck {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    bool initialized=false;
    ~SdlCheck() {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        if (initialized) SDL_Quit();
    }
};
}

int run_map_render_check(const std::filesystem::path& data_root,
                         const std::filesystem::path& relative_map,
                         maps::FootprintPolicy policy) {
    Json report{
        {"schema","openemperor-map-render-check-v1"},
        {"relative_path",relative_map.generic_string()},
        {"status","not_checked"},
        {"graphics_profile",maps::stored_graphics_profile},
        {"footprint_policy",maps::footprint_policy_name(policy)},
        {"original_game_comparison",false},
        {"stages",{{"file_discovered",false},{"container_valid",false},
                   {"map_profile_read",false},{"geometry_supported",false},
                   {"graphics_plan_created",false},{"assets_decoded",false},
                   {"textures_uploaded",false},{"render_frames",false}}}
    };
    try {
        const auto path=maps::resolve_map_path(data_root,relative_map);
        report["stages"]["file_discovered"]=true;
        const auto container=maps::EmperorContainer::open(path);
        report["stages"]["container_valid"]=true;
        if (container.multipart()) throw std::runtime_error("render check requires standalone map");
        const auto probed=maps::read_emperor_map(container,0);
        report["stages"]["map_profile_read"]=true;
        report["declared_map_size"]=probed.declared_map_size;
        const maps::MapGeometry geometry{probed.declared_map_size};
        report["stages"]["geometry_supported"]=geometry.supported;
        if (!geometry.supported) {
            report["status"]="unsupported_profile";
            throw std::runtime_error("unsupported map geometry");
        }
        auto session=maps::load_stored_map_session(data_root,relative_map,policy);
        report["stages"]["graphics_plan_created"]=true;
        SdlCheck sdl;
        if (!SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE))
            throw std::runtime_error("cannot select SDL dummy video driver");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        sdl.initialized=true;
        sdl.window=SDL_CreateWindow("OpenEmperor render check",1280,720,SDL_WINDOW_HIDDEN);
        if (!sdl.window) throw std::runtime_error(SDL_GetError());
        sdl.renderer=SDL_CreateRenderer(sdl.window,"software");
        if (!sdl.renderer) throw std::runtime_error(SDL_GetError());
        report["render_backend"]=SDL_GetRendererName(sdl.renderer);
        report["render_width"]=1280;
        report["render_height"]=720;
        MapDebugView view{std::move(session.map),maps::RawLayer::Terrain,
            maps::MapViewMode::StoredGraphics,std::nullopt,std::move(session.plan)};
        {
            QuietStdout quiet;
            view.initialize(sdl.window,sdl.renderer);
            const auto& plan=*view.stored_plan();
            const auto counts=plan.status_counts();
            std::size_t accounted=0;
            for (const auto& [status,count]:counts) { (void)status; accounted+=count; }
            const auto rendered=counts.contains("rendered") ? counts.at("rendered") : 0;
            if (accounted!=plan.cells.size() || plan.covered_cells()>plan.cells.size() ||
                rendered!=plan.covered_cells() ||
                plan.footprint_count(1)+plan.footprint_count(2)!=plan.footprints.size())
                throw std::runtime_error("stored graphics report counters disagree");
            report["candidate_cells"]=plan.cells.size();
            report["covered_cells"]=plan.covered_cells();
            report["diagnostic_cells"]=plan.cells.size()-plan.covered_cells();
            report["status_counts"]=counts;
            report["one_by_one_instances"]=plan.footprint_count(1);
            report["two_by_two_instances"]=plan.footprint_count(2);
            report["distinct_referenced_assets"]=plan.assets.size();
            std::set<std::size_t> required_assets;
            for (const auto& footprint:plan.footprints) required_assets.insert(footprint.asset_index);
            report["distinct_required_assets"]=required_assets.size();
            report["decoded_assets"]=plan.decoded_assets;
            report["texture_uploads"]=plan.texture_uploads;
            report["logical_rgba_texture_bytes"]=plan.logical_texture_bytes;
            report["marker_deviations"]=plan.marker_deviations;
            report["unknown_bit_cells"]=plan.unknown_bit_cells;
            report["mask_mismatches"]=plan.mask_comparison.mismatches();
            report["stages"]["assets_decoded"]=plan.decoded_assets==required_assets.size();
            report["stages"]["textures_uploaded"]=plan.texture_uploads==required_assets.size();
            if (!view.render()) throw std::runtime_error(SDL_GetError());
            report["overview_texture_draws"]=view.stored_texture_draws();
            report["overview_diagnostic_draws"]=view.stored_diagnostic_draws();
            SDL_Event wheel{};
            wheel.type=SDL_EVENT_MOUSE_WHEEL;
            wheel.wheel.mouse_x=640; wheel.wheel.mouse_y=360; wheel.wheel.y=6;
            bool running=true;
            view.handle_event(wheel,running);
            if (!running || !view.render()) throw std::runtime_error(SDL_GetError());
            report["zoom_texture_draws"]=view.stored_texture_draws();
            report["zoom_diagnostic_draws"]=view.stored_diagnostic_draws();
            report["stages"]["render_frames"]=true;
            report["status"]=plan.covered_cells()==plan.cells.size() ?
                "snapshot_complete" : "snapshot_partial";
            view.shutdown();
        }
    } catch (const std::exception& error) {
        if (report["status"]=="not_checked")
            report["status"]=report["stages"]["graphics_plan_created"].get<bool>() ?
                "render_failed" :
                report["stages"]["container_valid"].get<bool>() &&
                !report["stages"]["map_profile_read"].get<bool>() ?
                    "unsupported_profile" : "load_failed";
        report["error"]=error.what();
        std::cerr<<"Map render check: "<<error.what()<<'\n';
    }
    std::cout<<report.dump()<<'\n';
    const auto status=report["status"].get<std::string>();
    return status=="snapshot_complete" || status=="snapshot_partial" ? 0 : 1;
}
} // namespace openemperor
