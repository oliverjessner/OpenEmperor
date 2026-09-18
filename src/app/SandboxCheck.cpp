#include "app/SandboxCheck.h"

#include "app/SandboxView.h"
#include "maps/StoredMapSession.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace openemperor {
int run_sandbox_check(const std::filesystem::path& data_root,
                      const std::filesystem::path& map_relative,
                      simulation::RulesProfile rules) {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    try {
        auto session=maps::load_stored_map_session(data_root,map_relative,
            maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
        SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"SDL_VIDEODRIVER","dummy",1);
        SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        if (!SDL_CreateWindowAndRenderer("Sandbox check",1100,700,SDL_WINDOW_HIDDEN,
                                         &window,&renderer)) throw std::runtime_error(SDL_GetError());
        SandboxView view(std::move(session),true,rules);
        view.initialize(window,renderer);
        bool delivered=false,returning=false,returned=false,balanced=true,rendered=true;
        bool pottery_processed=false,clay_delivered=false;
        int frames_with_both=0;
        const int limit=rules==simulation::RulesProfile::ProductionV2 ? 3000 : 700;
        for (int i=0;i<limit;++i) {
            view.tick_once();
            const auto& world=view.world();
            if (rules==simulation::RulesProfile::ProductionV2) {
                balanced=balanced && world.production_balance_valid();
                const auto& p=world.building(simulation::BuildingId::Pottery);
                clay_delivered=clay_delivered || p.input_clay>0 || p.active_recipe_clay>0;
                pottery_processed=pottery_processed || world.pottery_completed_total()>0;
                delivered=delivered || world.building(simulation::BuildingId::Warehouse).pottery_stock>0;
                returning=returning || world.courier(simulation::CourierId::Pottery).phase==
                    simulation::CourierPhase::Returning;
                returned=returned || (returning && world.courier(simulation::CourierId::Pottery).phase==
                    simulation::CourierPhase::IdleAtWorkshop);
            } else {
                balanced=balanced && world.goods_balance_valid();
                if (world.warehouse_stock()>0) delivered=true;
                if (delivered && world.courier_phase()==simulation::CourierPhase::Returning) returning=true;
                if (returning && world.courier_phase()==simulation::CourierPhase::IdleAtWorkshop) returned=true;
            }
            if (i%20==0 || i==limit-1) {
                rendered=rendered && view.render();
                if (rules==simulation::RulesProfile::ProductionV2 && view.last_courier_draws()==2)
                    ++frames_with_both;
            }
        }
        const auto& world=view.world();
        const auto origin=view.demo_origin();
        if (rules==simulation::RulesProfile::ProductionV2) {
            const auto& source=world.building(simulation::BuildingId::ClaySource);
            const auto& pottery=world.building(simulation::BuildingId::Pottery);
            const auto& warehouse=world.building(simulation::BuildingId::Warehouse);
            const auto& a=world.courier(simulation::CourierId::Clay);
            const auto& b=world.courier(simulation::CourierId::Pottery);
            std::cout<<nlohmann::json{{"schema","openemperor-sandbox-check-v2"},
                {"rules",simulation::rules_profile_name(rules)},
                {"map",map_relative.generic_string()},
                {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}) : nlohmann::json()},
                {"ticks",world.ticks()},{"clay_extracted_total",world.clay_extracted_total()},
                {"pottery_completed_total",world.pottery_completed_total()},
                {"clay_source_output",source.output},{"pottery_input_clay",pottery.input_clay},
                {"pottery_active_recipe_clay",pottery.active_recipe_clay},
                {"pottery_recipe_progress",pottery.progress},{"pottery_output",pottery.output},
                {"pottery_input_reserved",pottery.reserved_incoming},
                {"warehouse_pottery",warehouse.pottery_stock},
                {"warehouse_reserved",warehouse.reserved_incoming},
                {"courier_a",{{"phase",simulation::delivery_phase_name(a.phase)},
                               {"cargo_clay",a.cargo},{"reserved",a.reserved},
                               {"edge_progress",a.edge_progress}}},
                {"courier_b",{{"phase",simulation::delivery_phase_name(b.phase)},
                               {"cargo_pottery",b.cargo},{"reserved",b.reserved},
                               {"edge_progress",b.edge_progress}}},
                {"clay_delivered",clay_delivered},{"pottery_processed",pottery_processed},
                {"pottery_delivered",delivered},{"pottery_courier_returned",returned},
                {"goods_balance_valid",balanced},{"frames_rendered",rendered},
                {"frames_with_two_couriers",frames_with_both}}.dump()<<'\n';
            view.shutdown();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return clay_delivered && pottery_processed && delivered && balanced && rendered &&
                frames_with_both>0 ? 0 : 1;
        }
        std::cout << nlohmann::json{{"schema","openemperor-sandbox-check-v1"},
            {"map",map_relative.generic_string()},
            {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}) : nlohmann::json()},
            {"ticks",world.ticks()},{"produced",world.total_produced()},
            {"workshop_stock",world.workshop_stock()},{"courier_cargo",world.courier_cargo()},
            {"warehouse_stock",world.warehouse_stock()},
            {"delivered",delivered},{"returning",returning},{"returned",returned},
            {"goods_balance_valid",balanced},{"frames_rendered",rendered}}.dump()<<'\n';
        view.shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return delivered && returned && balanced && rendered ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Sandbox check failed: " << error.what() << '\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
}
