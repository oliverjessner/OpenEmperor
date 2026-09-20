#include "app/SandboxCheck.h"

#include "app/SandboxView.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <array>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>

namespace openemperor {
int run_sandbox_check(const std::filesystem::path& data_root,
                      const std::filesystem::path& map_relative,
                      simulation::RulesProfile rules,bool resume_check,
                      const VisualSelection& visuals) {
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
        view.set_compatibility(visuals.compatibility.compatible() ?
            visuals.compatibility.profile->id:"unknown");
        if (!visuals.walker.empty()) view.set_walker_visuals(visuals.walker,visuals.walker_source);
        if (!visuals.building.empty()) view.set_building_visuals(visuals.building,visuals.building_source);
        if (!visuals.road.empty()) view.set_road_visuals(visuals.road,visuals.road_source);
        if (resume_check) {
            temporary.path=std::filesystem::canonical(std::filesystem::temp_directory_path())/
                ("openemperor-resume-check-"+std::to_string(std::random_device{}()));
            std::filesystem::create_directories(temporary.path);
            view.configure_save(data_root,map_relative,temporary.path/"resume.json");
        }
        view.initialize(window,renderer);
        std::optional<simulation::World> walker_control;
        if (!visuals.walker.empty() || !visuals.building.empty() || !visuals.road.empty())
            walker_control.emplace(simulation::World::restore(view.world().snapshot(),
                                                               view.buildable_mask()));
        bool simulation_neutral=true;
        bool saved=false,reparsed=false,fresh_world=false,direct_equal=false,continued_equal=true;
        std::optional<simulation::World> resumed;
        bool delivered=false,returning=false,returned=false,balanced=true,rendered=true;
        bool pottery_processed=false,clay_delivered=false,house_delivered=false;
        std::array<int,4> house_arrivals{};
        std::array<int,5> courier_arrivals{};
        int frames_with_both=0,frames_with_three=0,frames_with_five=0;
        const int limit=rules==simulation::RulesProfile::IndustryV5 ? 8000 :
            rules==simulation::RulesProfile::SettlementV4 ? 6000 :
            simulation::production_profile(rules) ? 3000 : 700;
        for (int i=0;i<limit;++i) {
            const auto before=simulation::household_profile(rules) ?
                view.world().courier(simulation::CourierId::Household):simulation::CourierState{};
            std::array<simulation::CourierPhase,5> previous{};
            if (rules==simulation::RulesProfile::IndustryV5)
                for (unsigned id=1;id<=5;++id)
                    previous[id-1]=view.world().courier(static_cast<simulation::CourierId>(id)).phase;
            view.tick_once();
            const auto& world=view.world();
            if (walker_control) {
                walker_control->tick();
                simulation_neutral=simulation_neutral &&
                    walker_control->snapshot()==world.snapshot();
            }
            if (rules==simulation::RulesProfile::IndustryV5)
                for (unsigned id=1;id<=5;++id)
                    if (previous[id-1]==simulation::CourierPhase::ToWarehouse &&
                        world.courier(static_cast<simulation::CourierId>(id)).phase==
                            simulation::CourierPhase::Returning)
                        ++courier_arrivals[id-1];
            if (simulation::household_profile(rules) &&
                before.phase==simulation::CourierPhase::ToWarehouse &&
                world.courier(simulation::CourierId::Household).phase==simulation::CourierPhase::Returning) {
                const auto id=static_cast<unsigned>(before.target);
                if (id>=4 && id<8) ++house_arrivals[id-4];
            }
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
                if (simulation::household_profile(rules) && view.last_courier_draws()==3)
                    ++frames_with_three;
                if (rules==simulation::RulesProfile::IndustryV5 && view.last_courier_draws()==5)
                    ++frames_with_five;
            }
        }
        const auto walker_report=[&]() {
            const auto stats=view.walker_display_stats();
            constexpr const char* directions[]={"pos_x","neg_x","pos_y","neg_y"};
            nlohmann::json configured=nlohmann::json::array();
            nlohmann::json drawn=nlohmann::json::array();
            nlohmann::json missing=nlohmann::json::array();
            nlohmann::json decoded=nlohmann::json::array();
            for (std::size_t d=0;d<4;++d) {
                if (stats.configured[d]) configured.push_back(directions[d]);
                else missing.push_back(directions[d]);
                if (stats.moving_drawn[d]) drawn.push_back(directions[d]);
            }
            for (const auto& id:stats.decoded_frame_ids)
                decoded.push_back({{"archive",id.archive_relative_path.generic_string()},
                                   {"physical_image_index",id.image_index}});
            return nlohmann::json{{"configured_directions",configured},
                {"moving_directions_drawn",drawn},{"unmapped_directions",missing},
                {"decoded_frame_assets",decoded},{"decoded_asset_count",stats.decoded_assets},
                {"texture_uploads",stats.texture_uploads},
                {"unmapped_fallbacks",stats.unmapped_fallbacks},
                {"invalid_edge_fallbacks",stats.invalid_edge_fallbacks},
                {"decode_errors",0},{"manual_visual_review",false},
                {"simulation_neutral",simulation_neutral},
                {"save_resume_equal",resume_check ? nlohmann::json(direct_equal && continued_equal):
                    nlohmann::json()}};
        };
        const auto walker_visuals_report=[&]() {
            const auto stats=view.walker_display_stats();
            constexpr const char* directions[]={"pos_x","neg_x","pos_y","neg_y"};
            nlohmann::json configured=nlohmann::json::array();
            nlohmann::json roles=nlohmann::json::object();
            for (std::size_t r=0;r<3;++r) {
                const auto name=assets::walker_role_name(static_cast<assets::WalkerVisualRole>(r));
                const auto& role=stats.roles[r];
                if (role.configured) configured.push_back(name);
                nlohmann::json mapped=nlohmann::json::array(),drawn=nlohmann::json::array();
                for (std::size_t d=0;d<4;++d) {
                    if (role.directions_configured[d]) mapped.push_back(directions[d]);
                    if (role.directions_drawn[d]) drawn.push_back(directions[d]);
                }
                roles[name]={{"configured",role.configured},{"directions_configured",mapped},
                    {"directions_drawn",drawn},{"draws",role.draws},
                    {"fallback_unmapped",role.fallback_unmapped},
                    {"fallback_invalid_edge",role.fallback_invalid_edge}};
            }
            return nlohmann::json{{"schema_version",stats.schema_version},
                {"configured_roles",configured},{"unique_assets",stats.decoded_assets},
                {"texture_uploads",stats.texture_uploads},{"roles",roles},
                {"simulation_equal_to_control",simulation_neutral},
                {"manual_visual_review",false},
                {"save_resume_equal",resume_check ? nlohmann::json(direct_equal && continued_equal):
                    nlohmann::json()}};
        };
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
            if (rules==simulation::RulesProfile::IndustryV5) {
                nlohmann::json producers=nlohmann::json::array(),houses=nlohmann::json::array();
                int sources=0,potteries=0,supplied=0,consumed=0;
                for (unsigned id=1;id<=9;++id) {
                    const auto& instance=world.building(static_cast<simulation::BuildingId>(id));
                    if (!instance.placed) continue;
                    if (instance.kind==simulation::Object::ClaySource) ++sources;
                    if (instance.kind==simulation::Object::Pottery) ++potteries;
                    if (instance.kind==simulation::Object::ClaySource ||
                        instance.kind==simulation::Object::Pottery)
                        producers.push_back({{"id",id},{"kind",instance.kind==
                            simulation::Object::ClaySource ? "ClaySource":"Pottery"},
                            {"output",instance.output},{"input_clay",instance.input_clay},
                            {"clay_extracted",instance.clay_extracted},
                            {"recipes_completed",instance.recipes_completed}});
                    if (instance.kind==simulation::Object::Household) {
                        if (house_arrivals[id-4]>0) ++supplied;
                        if (instance.consumed_total>0) ++consumed;
                        houses.push_back({{"id",id},{"arrivals",house_arrivals[id-4]},
                            {"fulfilled",instance.fulfilled_demand},{"missed",instance.missed_demand},
                            {"consumed",instance.consumed_total}});
                    }
                }
                const bool success=sources==2 && potteries==2 && houses.size()>=3 &&
                    world.building(simulation::BuildingId::ClaySource).clay_extracted>0 &&
                    world.building(static_cast<simulation::BuildingId>(8)).clay_extracted>0 &&
                    world.building(simulation::BuildingId::Pottery).recipes_completed>0 &&
                    world.building(static_cast<simulation::BuildingId>(9)).recipes_completed>0 &&
                    courier_arrivals[1]>0 && courier_arrivals[4]>0 &&
                    supplied>=2 && consumed>=2 && balanced && rendered && frames_with_five>0 &&
                    (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal)) &&
                    (!walker_control || simulation_neutral);
                nlohmann::json report={{"schema","openemperor-sandbox-check-v5"},
                    {"rules",simulation::rules_profile_name(rules)},
                    {"map",map_relative.generic_string()},{"ticks",world.ticks()},
                    {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}):nlohmann::json()},
                    {"producers",producers},{"houses",houses},
                    {"courier_arrivals",courier_arrivals},
                    {"clay_extracted_total",world.clay_extracted_total()},
                    {"pottery_completed_total",world.pottery_completed_total()},
                    {"warehouse_pottery",warehouse.pottery_stock},
                    {"warehouse_reserved",warehouse.reserved_incoming},
                    {"distinct_supplied",supplied},{"distinct_consumed",consumed},
                    {"goods_balance_valid",balanced},{"frames_rendered",rendered},
                    {"frames_with_five_couriers",frames_with_five},
                    {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                               {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                               {"continued_equal",continued_equal}}}};
                report["compatibility"]={{"detected",visuals.compatibility.compatible()},
                    {"id",visuals.compatibility.compatible() ? visuals.compatibility.profile->id:"unknown"},
                    {"status",assets::compatibility_status_name(visuals.compatibility.status)},
                    {"walker",visual_profile_source_name(view.walker_visual_source())},
                    {"building",visual_profile_source_name(view.building_visual_source())},
                    {"road",visual_profile_source_name(view.road_visual_source())}};
                const auto painter=view.painter_stats();
                report["depth_painter"]={{"mode",view.unified_depth() ? "unified":"legacy"},
                    {"stored_items_visited",painter.stored_items_visited},
                    {"sandbox_items",painter.sandbox_items},
                    {"stored_before_sandbox_count",painter.stored_before_sandbox_count},
                    {"sandbox_before_stored_count",painter.sandbox_before_stored_count},
                    {"stored_order_builds",painter.stored_order_builds},
                    {"manual_visual_review",false}};
                if (view.walker_visual_source()!=VisualProfileSource::Fallback) {
                    report["walker"]=walker_report(); // Schema-1 check compatibility.
                    report["walker_visuals"]=walker_visuals_report();
                }
                if (view.building_visual_source()!=VisualProfileSource::Fallback) {
                    const auto stats=view.building_display_stats();
                    nlohmann::json roles=nlohmann::json::array();
                    nlohmann::json draws=nlohmann::json::object(),fallbacks=nlohmann::json::object();
                    for (const auto role:assets::building_roles) {
                        const auto index=assets::role_index(role);
                        const auto name=assets::building_role_name(role);
                        if (stats.configured_roles[index]) roles.push_back(name);
                        draws[name]=stats.drawn_instances[index];
                        fallbacks[name]=stats.placeholder_fallbacks[index];
                    }
                    report["building_visuals"]={{"configured_roles",roles},
                        {"decoded_unique_assets",stats.decoded_assets},
                        {"texture_uploads",stats.texture_uploads},
                        {"draws_by_role",draws},
                        {"placeholder_fallbacks_by_role",fallbacks},
                        {"simulation_equal_to_control",simulation_neutral},
                        {"manual_visual_review",false}};
                }
                if (view.road_visual_source()!=VisualProfileSource::Fallback) {
                    const auto stats=view.road_display_stats();
                    nlohmann::json masks=nlohmann::json::array(),seen=nlohmann::json::array();
                    for (std::size_t mask=0;mask<16;++mask) {
                        if (stats.configured_masks[mask]) masks.push_back(mask);
                        if (stats.masks_seen[mask]) seen.push_back(mask);
                    }
                    report["road_visuals"]={{"configured",stats.configured},
                        {"configured_masks",masks},{"unique_assets",stats.unique_assets},
                        {"texture_uploads",stats.texture_uploads},{"draws",stats.draws},
                        {"fallback_draws",stats.fallback_draws},{"masks_seen",seen},
                        {"simulation_equal_to_control",simulation_neutral},
                        {"manual_visual_review",false}};
                }
                std::cout<<report.dump()<<'\n';
                view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return success ? 0:1;
            }
            if (rules==simulation::RulesProfile::SettlementV4) {
                nlohmann::json houses=nlohmann::json::array();
                int supplied=0,consumed=0;
                for (unsigned id=4;id<8;++id) {
                    const auto& h=world.building(static_cast<simulation::BuildingId>(id));
                    if (!h.placed) continue;
                    if (house_arrivals[id-4]>0) ++supplied;
                    if (h.consumed_total>0) ++consumed;
                    houses.push_back({{"id",id},{"position",{h.cell.x,h.cell.y}},
                        {"arrivals",house_arrivals[id-4]},{"pottery",h.pottery_stock},
                        {"reserved",h.reserved_incoming},{"demand_progress",h.demand_progress},
                        {"fulfilled",h.fulfilled_demand},{"missed",h.missed_demand},
                        {"consumed",h.consumed_total}});
                }
                const bool success=houses.size()>=3 && supplied>=2 && consumed>=2 &&
                    pottery_processed && delivered && balanced && rendered && frames_with_three>0 &&
                    (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal));
                std::cout<<nlohmann::json{{"schema","openemperor-sandbox-check-v4"},
                    {"rules",simulation::rules_profile_name(rules)},
                    {"map",map_relative.generic_string()},{"ticks",world.ticks()},
                    {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}) : nlohmann::json()},
                    {"houses",houses},{"distinct_supplied",supplied},{"distinct_consumed",consumed},
                    {"last_dispatched_household",world.last_dispatched_household() ?
                        nlohmann::json(static_cast<unsigned>(*world.last_dispatched_household())):
                        nlohmann::json()},
                    {"supplier_target",supplier.phase==simulation::CourierPhase::IdleAtWorkshop ?
                        nlohmann::json():nlohmann::json(static_cast<unsigned>(supplier.target))},
                    {"pottery_completed_total",world.pottery_completed_total()},
                    {"goods_balance_valid",balanced},{"frames_rendered",rendered},
                    {"frames_with_three_couriers",frames_with_three},
                    {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                               {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                               {"continued_equal",continued_equal}}}}.dump()<<'\n';
                view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return success ? 0:1;
            }
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
