#include "app/MapRenderCheck.h"

#include "app/MapDebugView.h"
#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/MapCatalog.h"
#include "maps/MapGeometry.h"
#include "maps/StoredMapSession.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
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
                         maps::FootprintPolicy policy, maps::StoredGraphicsProfile profile) {
    Json report{
        {"schema","openemperor-map-render-check-v2"},
        {"relative_path",relative_map.generic_string()},
        {"status","not_checked"},
        {"graphics_profile",maps::stored_graphics_profile_name(profile)},
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
        auto session=maps::load_stored_map_session(data_root,relative_map,policy,profile);
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
            const auto histogram=plan.footprint_histogram();
            std::size_t counted_footprints=0, rendered_instances=0;
            std::set<std::size_t> owned_cells;
            for (const auto& [side,count]:histogram) { (void)side; counted_footprints+=count; }
            for (const auto& footprint:plan.footprints) {
                if (footprint.width_cells!=footprint.height_cells ||
                    footprint.cell_indices.size()!=static_cast<std::size_t>(footprint.width_cells)*
                                                   footprint.height_cells)
                    throw std::runtime_error("stored graphics footprint shape disagrees");
                if (footprint.status!=maps::StoredStatus::Rendered) continue;
                ++rendered_instances;
                for (const auto index:footprint.cell_indices)
                    if (index>=plan.cells.size() || !owned_cells.insert(index).second ||
                        plan.cells[index].footprint_index!=footprint.id ||
                        plan.cells[index].status!=maps::StoredStatus::Rendered)
                        throw std::runtime_error("stored graphics cell ownership disagrees");
            }
            if (accounted!=plan.cells.size() || counted_footprints!=plan.footprints.size() ||
                rendered!=owned_cells.size() || rendered!=plan.covered_cells())
                throw std::runtime_error("stored graphics report counters disagree");
            report["candidate_cells"]=plan.cells.size();
            report["covered_cells"]=plan.covered_cells();
            report["diagnostic_cells"]=plan.cells.size()-plan.covered_cells();
            report["status_counts"]=counts;
            struct SlotEvidence {
                std::size_t cells=0;
                std::set<std::uint32_t> ids;
                std::uint32_t min_local=std::numeric_limits<std::uint32_t>::max();
                std::uint32_t max_local=0;
                Json examples=Json::array();
            };
            std::map<std::uint32_t,SlotEvidence> missing_slots;
            for (const auto& cell:plan.cells) {
                if (cell.lookup_status!=maps::GraphicsIdStatus::UnregisteredSlot) continue;
                auto& evidence=missing_slots[cell.slot];
                ++evidence.cells;
                evidence.ids.insert(cell.stored_id);
                evidence.min_local=std::min(evidence.min_local,cell.local_index);
                evidence.max_local=std::max(evidence.max_local,cell.local_index);
                if (evidence.examples.size()<4)
                    evidence.examples.push_back({{"x",cell.storage.x},{"y",cell.storage.y},
                                                 {"raw",cell.stored_id}});
            }
            report["unregistered_slots"]=Json::array();
            for (const auto& [slot,evidence]:missing_slots) {
                Json ids=Json::array();
                for (const auto id:evidence.ids) {
                    if (ids.size()>=256) break;
                    ids.push_back(id);
                }
                report["unregistered_slots"].push_back({{"slot",slot},{"candidate_cells",evidence.cells},
                    {"distinct_stored_ids",evidence.ids.size()},{"min_local_index",evidence.min_local},
                    {"max_local_index",evidence.max_local},{"stored_ids",ids},
                    {"stored_ids_truncated",evidence.ids.size()>ids.size()},
                    {"examples",evidence.examples}});
            }
            report["one_by_one_instances"]=plan.footprint_count(1);
            report["two_by_two_instances"]=plan.footprint_count(2);
            report["footprint_instances_planned"]=plan.footprints.size();
            report["footprint_instances_rendered"]=rendered_instances;
            Json size_histogram=Json::object();
            for (const auto& [side,count]:histogram)
                size_histogram[std::to_string(side)]=count;
            report["footprint_size_histogram"]=std::move(size_histogram);
            report["supported_footprint_sides"]=policy==maps::FootprintPolicy::EdgeByte4x4Preview ?
                Json::array({1,2,4}) :
                policy==maps::FootprintPolicy::Disabled ? Json::array({1}) : Json::array({1,2});
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
