#include "app/SandboxCheck.h"

#include "app/SandboxView.h"
#include "maps/StoredMapSession.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <stdexcept>

namespace openemperor {
int run_sandbox_check(const std::filesystem::path& data_root,
                      const std::filesystem::path& map_relative) {
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
        SandboxView view(std::move(session),true);
        view.initialize(window,renderer);
        bool delivered=false,returning=false,returned=false,balanced=true,rendered=true;
        for (int i=0;i<700;++i) {
            view.tick_once();
            const auto& world=view.world();
            balanced=balanced && world.goods_balance_valid();
            if (world.warehouse_stock()>0) delivered=true;
            if (delivered && world.courier_phase()==simulation::CourierPhase::Returning) returning=true;
            if (returning && world.courier_phase()==simulation::CourierPhase::IdleAtWorkshop) returned=true;
            if (i%20==0 || i==699) rendered=rendered && view.render();
        }
        const auto& world=view.world();
        const auto origin=view.demo_origin();
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
