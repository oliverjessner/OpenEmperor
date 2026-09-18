#include "app/SandboxCheck.h"

#include "app/SandboxView.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>

namespace openemperor {
int run_sandbox_check(const std::filesystem::path& data_root,
                      const std::filesystem::path& map_relative,
                      simulation::RulesProfile rules,bool resume_check) {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    struct TempCleanup {
        std::filesystem::path path;
        ~TempCleanup() { if (!path.empty()) { std::error_code ignored;
            std::filesystem::remove_all(path,ignored); } }
    } temporary;
    try {
        auto session=maps::load_stored_map_session(data_root,map_relative,
            maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
        SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"SDL_VIDEODRIVER","dummy",1);
        SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        if (!SDL_CreateWindowAndRenderer("Sandbox check",1100,700,SDL_WINDOW_HIDDEN,
                                         &window,&renderer)) throw std::runtime_error(SDL_GetError());
        SandboxView view(std::move(session),true,rules);
        if (resume_check) {
            temporary.path=std::filesystem::canonical(std::filesystem::temp_directory_path())/
                ("openemperor-resume-check-"+std::to_string(std::random_device{}()));
            std::filesystem::create_directories(temporary.path);
            view.configure_save(data_root,map_relative,temporary.path/"resume.json");
        }
        view.initialize(window,renderer);
        bool saved=false,reparsed=false,fresh_world=false,direct_equal=false,continued_equal=true;
        std::optional<simulation::World> resumed;
        bool delivered=false,returning=false,returned=false,balanced=true,rendered=true;
        bool pottery_processed=false,clay_delivered=false,house_delivered=false;
        int frames_with_both=0,frames_with_three=0;
        const int limit=simulation::production_profile(rules) ? 3000 : 700;
        for (int i=0;i<limit;++i) {
            view.tick_once();
            const auto& world=view.world();
            if (resume_check && i==(simulation::production_profile(rules) ? 1000:200)) {
                view.save_now(); saved=true;
                const auto parsed=persistence::read_save(temporary.path/"resume.json");
                reparsed=true;
                resumed.emplace(persistence::restore_save(parsed,data_root,view.buildable_mask()));
                fresh_world=true;
                direct_equal=resumed->snapshot()==world.snapshot();
            } else if (resumed) {
                resumed->tick();
                continued_equal=continued_equal && resumed->snapshot()==world.snapshot();
                balanced=balanced && (simulation::production_profile(rules) ?
                    resumed->production_balance_valid():resumed->goods_balance_valid());
            }
            if (simulation::production_profile(rules)) {
                balanced=balanced && world.production_balance_valid() && world.navigation_valid();
                const auto& p=world.building(simulation::BuildingId::Pottery);
                clay_delivered=clay_delivered || p.input_clay>0 || p.active_recipe_clay>0;
                pottery_processed=pottery_processed || world.pottery_completed_total()>0;
                delivered=delivered || world.building(simulation::BuildingId::Warehouse).pottery_stock>0;
                returning=returning || world.courier(simulation::CourierId::Pottery).phase==
                    simulation::CourierPhase::Returning;
                returned=returned || (returning && world.courier(simulation::CourierId::Pottery).phase==
                    simulation::CourierPhase::IdleAtWorkshop);
                if (rules==simulation::RulesProfile::HouseholdV3)
                    house_delivered=house_delivered ||
                        world.building(simulation::BuildingId::Household).pottery_stock>0;
            } else {
                balanced=balanced && world.goods_balance_valid();
                if (world.warehouse_stock()>0) delivered=true;
                if (delivered && world.courier_phase()==simulation::CourierPhase::Returning) returning=true;
                if (returning && world.courier_phase()==simulation::CourierPhase::IdleAtWorkshop) returned=true;
            }
            if (i%20==0 || i==limit-1) {
                rendered=rendered && view.render();
                if (simulation::production_profile(rules) && view.last_courier_draws()>=2)
                    ++frames_with_both;
                if (rules==simulation::RulesProfile::HouseholdV3 && view.last_courier_draws()==3)
                    ++frames_with_three;
            }
        }
        const auto& world=view.world();
        const auto origin=view.demo_origin();
        if (simulation::production_profile(rules)) {
            const auto& source=world.building(simulation::BuildingId::ClaySource);
            const auto& pottery=world.building(simulation::BuildingId::Pottery);
            const auto& warehouse=world.building(simulation::BuildingId::Warehouse);
            const auto& a=world.courier(simulation::CourierId::Clay);
            const auto& b=world.courier(simulation::CourierId::Pottery);
            const auto& home=world.building(simulation::BuildingId::Household);
            const auto& supplier=world.courier(simulation::CourierId::Household);
            if (rules==simulation::RulesProfile::HouseholdV3) {
                const bool success=clay_delivered && pottery_processed && delivered &&
                    house_delivered && home.consumed_total>0 && balanced && rendered &&
                    frames_with_three>0 && (!resume_check ||
                    (saved && reparsed && fresh_world && direct_equal && continued_equal));
                std::cout<<nlohmann::json{{"schema","openemperor-sandbox-check-v3"},
                    {"rules",simulation::rules_profile_name(rules)},
                    {"map",map_relative.generic_string()},
                    {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}) : nlohmann::json()},
                    {"ticks",world.ticks()},{"pottery_completed_total",world.pottery_completed_total()},
                    {"warehouse_pottery",warehouse.pottery_stock},
                    {"household_pottery",home.pottery_stock},
                    {"household_reserved",home.reserved_incoming},
                    {"fulfilled_demand",home.fulfilled_demand},{"missed_demand",home.missed_demand},
                    {"consumed_total",home.consumed_total},
                    {"supplier_cargo",supplier.cargo},{"house_delivered",house_delivered},
                    {"goods_balance_valid",balanced},{"frames_rendered",rendered},
                    {"frames_with_three_couriers",frames_with_three},
                    {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                               {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                               {"continued_equal",continued_equal}}}}.dump()<<'\n';
                view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return success ? 0:1;
            }
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
                {"frames_with_two_couriers",frames_with_both},
                {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                           {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                           {"continued_equal",continued_equal}}}}.dump()<<'\n';
            view.shutdown();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal)) &&
                clay_delivered && pottery_processed && delivered && balanced && rendered &&
                frames_with_both>0 ? 0 : 1;
        }
        std::cout << nlohmann::json{{"schema","openemperor-sandbox-check-v1"},
            {"map",map_relative.generic_string()},
            {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}) : nlohmann::json()},
            {"ticks",world.ticks()},{"produced",world.total_produced()},
            {"workshop_stock",world.workshop_stock()},{"courier_cargo",world.courier_cargo()},
            {"warehouse_stock",world.warehouse_stock()},
            {"delivered",delivered},{"returning",returning},{"returned",returned},
            {"goods_balance_valid",balanced},{"frames_rendered",rendered},
            {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                       {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                       {"continued_equal",continued_equal}}}}.dump()<<'\n';
        view.shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal)) &&
            delivered && returned && balanced && rendered ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Sandbox check failed: " << error.what() << '\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
}
