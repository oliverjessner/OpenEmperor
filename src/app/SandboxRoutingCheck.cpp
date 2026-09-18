#include "app/SandboxCheck.h"

#include "app/SandboxView.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor {
namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
}

int run_sandbox_routing_check(const std::filesystem::path& data_root,
                              const std::filesystem::path& map_relative) {
    namespace sim=simulation;
    namespace fs=std::filesystem;
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    struct Cleanup { fs::path path; ~Cleanup() { if (!path.empty()) { std::error_code e;
        fs::remove_all(path,e); } } } cleanup;
    try {
        auto session=maps::load_stored_map_session(data_root,map_relative,
            maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
        SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"SDL_VIDEODRIVER","dummy",1);
        SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("Sandbox routing check",1100,700,SDL_WINDOW_HIDDEN,
                                            &window,&renderer),SDL_GetError());
        cleanup.path=fs::canonical(fs::temp_directory_path())/
            ("openemperor-routing-check-"+std::to_string(std::random_device{}()));
        fs::create_directories(cleanup.path);
        const auto save_path=cleanup.path/"routing.json";
        SandboxView view(std::move(session),true,sim::RulesProfile::ProductionV2);
        view.configure_save(data_root,map_relative,save_path);
        view.initialize(window,renderer);
        const auto origin=view.demo_origin();
        require(origin.has_value(),"no buildable v2 demo row");
        const sim::Cell first_road{origin->x+1,origin->y};
        const sim::Cell later_road{origin->x+2,origin->y};
        bool edge=false;
        for (int i=0;i<300 && !edge;++i) {
            view.tick_once();
            const auto& c=view.world().courier(sim::CourierId::Clay);
            edge=c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==*origin;
        }
        require(edge,"no outbound clay edge on demo row");
        const auto revision=view.world().road_revision();
        const bool rejected=!view.execute({sim::CommandType::RemoveRoad,first_road}).accepted &&
            view.world().road_revision()==revision;
        const bool removed=view.execute({sim::CommandType::RemoveRoad,later_road}).accepted &&
            view.world().road_revision()==revision+1;
        bool waiting=false;
        for (int i=0;i<30 && !waiting;++i) {
            view.tick_once();
            const auto& c=view.world().courier(sim::CourierId::Clay);
            waiting=c.route_pending && c.edge_progress==0;
        }
        const auto& paused=view.world().courier(sim::CourierId::Clay);
        const bool goods_held=waiting && paused.cargo>0 && paused.cargo==paused.reserved &&
            view.world().building(sim::BuildingId::Pottery).reserved_incoming==paused.reserved &&
            view.world().courier_position(sim::CourierId::Clay)==
                sim::Position{static_cast<double>(first_road.x),static_cast<double>(first_road.y)};
        require(rejected && removed && goods_held,"road protection, wait or reservation failed");
        view.save_now();
        const auto document=persistence::read_save(save_path);
        auto resumed=persistence::restore_save(document,data_root,view.buildable_mask());
        const bool restored=resumed.snapshot()==view.world().snapshot();
        require(restored,"waiting state changed across save/load");
        const bool repaired_a=view.execute({sim::CommandType::PlaceRoad,later_road}).accepted;
        const bool repaired_b=resumed.execute({sim::CommandType::PlaceRoad,later_road}).accepted;
        require(repaired_a && repaired_b,"road repair failed");
        bool identical=view.world().snapshot()==resumed.snapshot();
        bool balanced=true,rendered=true,delivered=false,returned=false;
        for (int i=0;i<500;++i) {
            const auto before=view.world().courier(sim::CourierId::Clay).phase;
            view.tick_once(); resumed.tick();
            identical=identical && view.world().snapshot()==resumed.snapshot();
            balanced=balanced && view.world().production_balance_valid() &&
                view.world().navigation_valid() && resumed.production_balance_valid() &&
                resumed.navigation_valid();
            const auto after=view.world().courier(sim::CourierId::Clay).phase;
            if (before==sim::CourierPhase::ToWarehouse && after==sim::CourierPhase::Returning)
                delivered=true;
            if (delivered && before==sim::CourierPhase::Returning &&
                after==sim::CourierPhase::IdleAtWorkshop) returned=true;
            if (i%20==0 || i==499) rendered=rendered && view.render();
        }
        std::cout<<nlohmann::json{{"schema","openemperor-sandbox-routing-check-v1"},
            {"map",map_relative.generic_string()},
            {"demo_origin",nlohmann::json::array({origin->x,origin->y})},
            {"protected_rejected",rejected},{"future_removed",removed},
            {"wait_reached",waiting},{"cargo_and_reservation_preserved",goods_held},
            {"saved_reparsed_restored",restored},{"road_repaired",repaired_a && repaired_b},
            {"per_tick_equal",identical},{"balances_valid",balanced},
            {"delivery_completed",delivered},{"return_completed",returned},
            {"frames_rendered",rendered}}.dump()<<'\n';
        view.shutdown();
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        return identical && balanced && delivered && returned && rendered ? 0:1;
    } catch (const std::exception& error) {
        std::cerr<<"Sandbox routing check failed: "<<error.what()<<'\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
} // namespace openemperor
