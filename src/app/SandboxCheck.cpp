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
        if (resume_check) {
            temporary.path=std::filesystem::canonical(std::filesystem::temp_directory_path())/
                ("openemperor-resume-check-"+std::to_string(std::random_device{}()));
            std::filesystem::create_directories(temporary.path);
        }
        view.configure_save(data_root,map_relative,
            resume_check ? temporary.path/"resume.json":std::filesystem::path{});
        view.set_compatibility(visuals.compatibility.compatible() ?
            visuals.compatibility.profile->id:"unknown");
        if (!visuals.walker.empty()) view.set_walker_visuals(visuals.walker,visuals.walker_source);
        if (!visuals.building.empty()) view.set_building_visuals(visuals.building,visuals.building_source);
        if (!visuals.road.empty()) view.set_road_visuals(visuals.road,visuals.road_source);
        view.initialize(window,renderer);
        const auto initial_ticks=view.world().ticks();
        const auto initial_treasury=view.world().treasury();
        const auto initial_taxes=view.world().taxes_collected_total();
        const auto initial_construction_spent=view.world().construction_spent_total();
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
        std::array<int,7> courier_arrivals{};
        std::uint64_t service_visits=0;
        int frames_with_both=0,frames_with_three=0,frames_with_five=0,frames_with_food=0;
        int frames_with_service=0;
        std::uint64_t first_service_visit_tick=0;
        std::uint64_t first_tax_tick=0;
        std::uint64_t first_tax_total=0;
        std::int64_t treasury_at_first_tax=initial_treasury;
        bool service_walker_activated=false;
        const int limit=simulation::fire_profile(rules) ? 1200 :
            rules==simulation::RulesProfile::IndustryV5 ? 8000 :
            simulation::food_profile(rules) ? 8000 :
            rules==simulation::RulesProfile::CityV6 ? 4000 :
            rules==simulation::RulesProfile::SettlementV4 ? 6000 :
            simulation::production_profile(rules) ? 3000 : 700;
        for (int i=0;i<limit;++i) {
            const auto before=simulation::household_profile(rules) &&
                !simulation::scalable_profile(rules) ?
                view.world().courier(simulation::CourierId::Household):simulation::CourierState{};
            std::vector<std::pair<simulation::CourierId,simulation::CourierPhase>> previous;
            const unsigned courier_count=(simulation::scalable_profile(rules)) ?
                static_cast<unsigned>(view.world().couriers().size()):
                simulation::service_profile(rules) ? 7U:
                simulation::food_profile(rules) ? 6U:
                simulation::industry_profile(rules) ? 5U:0U;
            if (simulation::scalable_profile(rules)) {
                for (const auto& c:view.world().couriers()) previous.emplace_back(c.id,c.phase);
            } else if (courier_count)
                for (unsigned id=1;id<=courier_count;++id)
                    previous.emplace_back(static_cast<simulation::CourierId>(id),
                        view.world().courier(static_cast<simulation::CourierId>(id)).phase);
            view.tick_once();
            const auto& world=view.world();
            if (first_tax_tick==0 && world.taxes_collected_total()>initial_taxes) {
                first_tax_tick=world.ticks();
                first_tax_total=world.taxes_collected_total()-initial_taxes;
                treasury_at_first_tax=world.treasury();
            }
            if (simulation::service_profile(rules)) for (const auto& c:world.couriers())
                if (c.role==simulation::CourierRole::Service &&
                    c.phase!=simulation::CourierPhase::IdleAtWorkshop) service_walker_activated=true;
            if (walker_control) {
                walker_control->tick();
                simulation_neutral=simulation_neutral &&
                    walker_control->snapshot()==world.snapshot();
            }
            if (courier_count)
                for (const auto& [id,phase]:previous)
                    if (phase==simulation::CourierPhase::ToWarehouse &&
                        world.courier(id).phase==
                            simulation::CourierPhase::Returning) {
                        const auto numeric=static_cast<std::uint32_t>(id);
                        if (numeric>=1 && numeric<=courier_arrivals.size())
                            ++courier_arrivals[numeric-1];
                        if (world.courier(id).role==simulation::CourierRole::Service) {
                            ++service_visits;
                            if (first_service_visit_tick==0) first_service_visit_tick=world.ticks();
                        }
                    }
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
                balanced=balanced && world.production_balance_valid() && world.navigation_valid() &&
                    (!simulation::city_profile(rules) || world.city_economy_valid()) &&
                    (!simulation::food_profile(rules) || world.food_balance_valid()) &&
                    (!simulation::service_profile(rules) || world.service_state_valid()) &&
                    (!simulation::population_profile(rules) || world.population_valid());
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
                if (rules==simulation::RulesProfile::CityV7 && view.last_courier_draws()>=4)
                    ++frames_with_food;
                if (simulation::service_profile(rules) && view.last_courier_draws()>=5)
                    ++frames_with_service;
            }
        }
        // Bounded City-v12 presentation smoke: a normally paid second instance
        // in the already validated starter area. Both controls receive the same
        // command; no new tick or decoded texture is needed to display it.
        bool second_watch_loaded=false;
        if (simulation::fire_profile(rules)) {
            const auto origin=view.demo_origin();
            if (!origin) throw std::runtime_error("Fire Watch check has no starter origin");
            const simulation::Command command{simulation::CommandType::PlaceFireWatch,
                {origin->x+14,origin->y+1}};
            const auto textures=view.building_texture_count();
            if (!view.execute(command).accepted) throw std::runtime_error("second Fire Watch placement failed");
            if (walker_control) {
                if (!walker_control->execute(command).accepted) throw std::runtime_error("Watch control command failed");
                simulation_neutral=simulation_neutral && walker_control->snapshot()==view.world().snapshot();
            }
            if (resumed) {
                if (!resumed->execute(command).accepted) throw std::runtime_error("resumed Watch command failed");
                continued_equal=continued_equal && resumed->snapshot()==view.world().snapshot();
            }
            rendered=rendered && view.render();
            second_watch_loaded=view.building_texture_count()==textures;
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
        if (simulation::scalable_profile(rules)) {
            const bool v11=simulation::market_profile(rules);
            const bool fire=simulation::fire_profile(rules);
            const auto building_stats=view.building_display_stats();
            const auto visual_role_ok=[&](assets::BuildingVisualRole role) {
                const auto index=assets::role_index(role);
                return building_stats.configured_roles[index] &&
                    building_stats.drawn_instances[index]>0 &&
                    building_stats.placeholder_fallbacks[index]==0;
            };
            const bool expected_builtin=visuals.building_source!=VisualProfileSource::Fallback;
            const bool require_watch_visual=fire &&
                (visuals.building_source==VisualProfileSource::Builtin ||
                 building_stats.configured_roles[assets::role_index(assets::BuildingVisualRole::FireWatch)]);
            const bool visual_ok=!expected_builtin ||
                (visual_role_ok(assets::BuildingVisualRole::Farm) &&
                 visual_role_ok(assets::BuildingVisualRole::ServicePost) &&
                 (!v11 || visual_role_ok(assets::BuildingVisualRole::Market)) &&
                 (!require_watch_visual || visual_role_ok(assets::BuildingVisualRole::FireWatch)));
            std::size_t houses=0,warehouses=0,farms=0,posts=0,markets=0;
            std::uint64_t fulfilled=0;
            nlohmann::json household_states=nlohmann::json::array();
            nlohmann::json operation_states=nlohmann::json::array();
            for (const auto& b:world.buildings()) {
                houses+=b.kind==simulation::Object::Household;
                warehouses+=b.kind==simulation::Object::Warehouse;
                farms+=b.kind==simulation::Object::Farm;
                posts+=b.kind==simulation::Object::ServicePost;
                markets+=b.kind==simulation::Object::Market;
                if (b.kind==simulation::Object::Household) {
                    fulfilled+=b.fulfilled_demand;
                    household_states.push_back({{"id",static_cast<std::uint32_t>(b.id)},
                        {"fulfilled",b.fulfilled_demand},{"missed",b.missed_demand},
                        {"pottery",b.pottery_stock},{"food",b.food_stock},
                        {"service_active",world.household_service_active(b.id)},
                        {"population",world.household_population(b.id)}});
                    if (simulation::desirability_profile(rules)) {
                        auto& house=household_states.back();
                        house["desirability"]=world.household_desirability(b.id);
                        house["historical_level"]=world.historical_household_level(b.id);
                        house["effective_level"]=world.household_level(b.id);
                        house["population_capacity"]=world.household_population_capacity(b.id);
                        house["taxes_paid_total"]=b.taxes_paid_total;
                    }
                }
                if (v11 && simulation::World::operation_controllable(b.kind))
                    operation_states.push_back({{"id",static_cast<std::uint32_t>(b.id)},
                        {"running",b.operating_enabled},
                        {"priority",simulation::workforce_priority_name(b.workforce_priority)}});
            }
            const bool success=houses==(v11 ? 4U:3U) && warehouses==1 && farms==1 && posts==1 &&
                markets==(v11 ? 1U:0U) && world.buildings().size()==(fire ? 12U:v11 ? 10U:8U) &&
                world.couriers().size()==(fire ? 9U:v11 ? 7U:5U) && (!fire || second_watch_loaded) && service_visits>0 &&
                (!v11 || (fulfilled>0 && world.taxes_collected_total()>0)) &&
                balanced && world.fire_state_valid() && rendered && visual_ok && (!resume_check ||
                    (saved && reparsed && fresh_world && direct_equal && continued_equal));
            nlohmann::json configured=nlohmann::json::array();
            nlohmann::json draws=nlohmann::json::object(),fallbacks=nlohmann::json::object();
            for (const auto role:assets::building_roles) {
                const auto index=assets::role_index(role);
                const auto name=assets::building_role_name(role);
                if (building_stats.configured_roles[index]) configured.push_back(name);
                draws[name]=building_stats.drawn_instances[index];
                fallbacks[name]=building_stats.placeholder_fallbacks[index];
            }
            std::cout<<nlohmann::json{{"schema",simulation::desirability_profile(rules) ? "openemperor-sandbox-check-v13":fire ? "openemperor-sandbox-check-v12":v11 ? "openemperor-sandbox-check-v11":
                                                    "openemperor-sandbox-check-v10"},
                {"rules",simulation::rules_profile_name(rules)},
                {"rule_version",world.rule_version()},
                {"map",map_relative.generic_string()},{"ticks_before",initial_ticks},
                {"ticks",world.ticks()},
                {"building_count",world.buildings().size()},{"courier_count",world.couriers().size()},
                {"house_count",houses},{"population",world.total_population()},
                {"warehouses",warehouses},{"farms",farms},{"service_posts",posts},
                {"markets",markets},{"fire_protected",world.protected_buildings()},
                {"fire_burning",world.burning_buildings()},{"fulfilled_demands",fulfilled},
                {"households",household_states},{"operation_states",operation_states},
                {"treasury_before",initial_treasury},{"treasury",world.treasury()},
                {"treasury_delta",world.treasury()-initial_treasury},
                {"taxes_before",initial_taxes},
                {"taxes_collected_total",world.taxes_collected_total()},
                {"construction_spent_before",initial_construction_spent},
                {"construction_spent_total",world.construction_spent_total()},
                {"first_tax_tick",first_tax_tick},{"first_tax_total",first_tax_total},
                {"treasury_at_first_tax",treasury_at_first_tax},
                {"district_independent_deliveries",nullptr},
                {"district_delivery_scope","starter_single_district"},
                {"service_visits",service_visits},{"production_balance_valid",world.production_balance_valid()},
                {"economy_valid",world.city_economy_valid()},
                {"compatibility",{{"detected",visuals.compatibility.compatible()},
                    {"id",visuals.compatibility.compatible() ? visuals.compatibility.profile->id:"unknown"},
                    {"building",visual_profile_source_name(view.building_visual_source())}}},
                {"building_visuals",{{"configured_roles",configured},
                    {"two_fire_watch_instances_shared_texture",fire &&
                        building_stats.configured_roles[assets::role_index(assets::BuildingVisualRole::FireWatch)] ?
                        nlohmann::json(second_watch_loaded):nlohmann::json()},
                    {"decoded_unique_assets",building_stats.decoded_assets},
                    {"texture_uploads",building_stats.texture_uploads},
                    {"draws_by_role",draws},{"placeholder_fallbacks_by_role",fallbacks},
                    {"simulation_equal_to_control",simulation_neutral},
                    {"manual_visual_review",false}}},
                {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                    {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                    {"continued_equal",continued_equal}}},
                {"original_emperor_fidelity_claim",false}}
                .dump()<<'\n';
            view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
            return success ? 0:1;
        }
        if (simulation::production_profile(rules)) {
            const auto& source=world.building(simulation::BuildingId::ClaySource);
            const auto& pottery=world.building(simulation::BuildingId::Pottery);
            const auto& warehouse=world.building(simulation::BuildingId::Warehouse);
            const auto& a=world.courier(simulation::CourierId::Clay);
            const auto& b=world.courier(simulation::CourierId::Pottery);
            const auto& home=world.building(simulation::BuildingId::Household);
            const auto& supplier=world.courier(simulation::CourierId::Household);
            if (simulation::service_profile(rules)) {
                nlohmann::json houses=nlohmann::json::array();
                int placed_houses=0;
                for (unsigned id=4;id<8;++id) {
                    const auto key=static_cast<simulation::BuildingId>(id);
                    const auto& house=world.building(key);
                    if (!house.placed) continue;
                    ++placed_houses;
                    houses.push_back({{"id",id},{"level",world.household_level(key)},
                        {"fulfilled",house.fulfilled_demand},{"missed",house.missed_demand},
                        {"pottery",house.pottery_stock},{"food",house.food_stock},
                        {"service_active",world.household_service_active(key)},
                        {"service_remaining",world.household_service_remaining(key)},
                        {"population",world.household_population(key)},
                        {"population_capacity",world.household_population_capacity(key)}});
                }
                const bool success=placed_houses==3 &&
                    world.building(simulation::BuildingId::ServicePost).placed &&
                    world.building_staffed(simulation::BuildingId::ServicePost) &&
                    courier_arrivals[6]>0 && first_service_visit_tick>0 &&
                    world.covered_households()>0 && world.taxes_collected_total()>0 &&
                    world.workforce_supply()==(simulation::population_profile(rules) ?
                        world.total_population():24) && world.workforce_used()==18 &&
                    (!simulation::population_profile(rules) ||
                        (world.total_population()>18 && world.population_valid())) &&
                    balanced && rendered && frames_with_service>0 &&
                    (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal));
                std::cout<<nlohmann::json{{"schema",simulation::population_profile(rules) ?
                        "openemperor-sandbox-check-v9":"openemperor-sandbox-check-v8"},
                    {"rules",simulation::rules_profile_name(rules)},
                    {"map",map_relative.generic_string()},{"ticks",world.ticks()},
                    {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}):nlohmann::json()},
                    {"houses",houses},{"service_post_placed",true},
                    {"service_staffed",world.building_staffed(simulation::BuildingId::ServicePost)},
                    {"service_walker_active",service_walker_activated},
                    {"service_visits",courier_arrivals[6]},
                    {"covered_households",world.covered_households()},
                    {"expired_households",placed_houses-world.covered_households()},
                    {"first_service_visit_tick",first_service_visit_tick},
                    {"treasury",world.treasury()},{"taxes_collected_total",world.taxes_collected_total()},
                    {"construction_spent_total",world.construction_spent_total()},
                    {"workforce_supply",world.workforce_supply()},
                    {"workforce_required",world.workforce_required()},
                    {"workforce_used",world.workforce_used()},
                    {"population",world.total_population()},
                    {"population_capacity",world.total_population_capacity()},
                    {"population_valid",world.population_valid()},
                    {"goal_households_ready",world.settlement_goal_households_ready()},
                    {"goal_reached",world.settlement_goal_reached()},
                    {"production_balance_valid",world.production_balance_valid()},
                    {"food_balance_valid",world.food_balance_valid()},
                    {"service_state_valid",world.service_state_valid()},
                    {"economy_valid",world.city_economy_valid()},
                    {"frames_rendered",rendered},{"frames_with_service_courier",frames_with_service},
                    {"original_emperor_fidelity_claim",false},
                    {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                               {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                               {"continued_equal",continued_equal}}}}.dump()<<'\n';
                view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return success ? 0:1;
            }
            if (rules==simulation::RulesProfile::CityV7) {
                nlohmann::json houses=nlohmann::json::array();
                std::uint64_t delivered_food=0,consumed_food=0;
                for (unsigned id=4;id<8;++id) {
                    const auto key=static_cast<simulation::BuildingId>(id);
                    const auto& house=world.building(key);
                    if (!house.placed) continue;
                    delivered_food+=static_cast<std::uint64_t>(house.food_stock)+
                        house.food_consumed_total;
                    consumed_food+=house.food_consumed_total;
                    houses.push_back({{"id",id},{"level",world.household_level(key)},
                        {"fulfilled",house.fulfilled_demand},{"missed",house.missed_demand},
                        {"pottery",house.pottery_stock},{"food",house.food_stock},
                        {"food_consumed",house.food_consumed_total},
                        {"tax_contributed",world.household_tax_contributed(key)}});
                }
                const bool success=houses.size()==2 && delivered_food>0 && consumed_food>0 &&
                    world.food_produced_total()>0 && world.taxes_collected_total()>0 &&
                    world.workforce_supply()==16 && world.workforce_used()==16 &&
                    balanced && rendered && frames_with_food>0 &&
                    (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal));
                std::cout<<nlohmann::json{{"schema","openemperor-sandbox-check-v7"},
                    {"rules",simulation::rules_profile_name(rules)},
                    {"map",map_relative.generic_string()},{"ticks",world.ticks()},
                    {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}):nlohmann::json()},
                    {"houses",houses},{"food_produced_total",world.food_produced_total()},
                    {"food_delivered",delivered_food},{"food_consumed",consumed_food},
                    {"treasury",world.treasury()},{"taxes_collected_total",world.taxes_collected_total()},
                    {"construction_spent_total",world.construction_spent_total()},
                    {"workforce_supply",world.workforce_supply()},
                    {"workforce_required",world.workforce_required()},
                    {"workforce_used",world.workforce_used()},
                    {"goal_households_ready",world.settlement_goal_households_ready()},
                    {"goal_reached",world.settlement_goal_reached()},
                    {"production_balance_valid",world.production_balance_valid()},
                    {"food_balance_valid",world.food_balance_valid()},
                    {"economy_valid",world.city_economy_valid()},
                    {"frames_rendered",rendered},{"frames_with_food_courier",frames_with_food},
                    {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                               {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                               {"continued_equal",continued_equal}}}}.dump()<<'\n';
                view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return success ? 0:1;
            }
            if (rules==simulation::RulesProfile::CityV6) {
                nlohmann::json houses=nlohmann::json::array();
                int supplied=0;
                for (unsigned id=4;id<8;++id) {
                    const auto& house=world.building(static_cast<simulation::BuildingId>(id));
                    if (!house.placed) continue;
                    if (house.fulfilled_demand>0) ++supplied;
                    houses.push_back({{"id",id},{"fulfilled",house.fulfilled_demand},
                        {"missed",house.missed_demand},{"pottery",house.pottery_stock},
                        {"tax_contributed",house.fulfilled_demand*25U}});
                }
                const bool success=houses.size()==2 && supplied==2 &&
                    world.taxes_collected_total()>0 && world.workforce_supply()==16 &&
                    world.workforce_used()==12 && balanced && rendered && frames_with_three>0 &&
                    (!resume_check || (saved && reparsed && fresh_world && direct_equal && continued_equal));
                std::cout<<nlohmann::json{{"schema","openemperor-sandbox-check-v6"},
                    {"rules",simulation::rules_profile_name(rules)},
                    {"map",map_relative.generic_string()},{"ticks",world.ticks()},
                    {"demo_origin",origin ? nlohmann::json::array({origin->x,origin->y}):nlohmann::json()},
                    {"houses",houses},{"distinct_supplied",supplied},
                    {"treasury",world.treasury()},{"taxes_collected_total",world.taxes_collected_total()},
                    {"construction_spent_total",world.construction_spent_total()},
                    {"workforce_supply",world.workforce_supply()},
                    {"workforce_required",world.workforce_required()},
                    {"workforce_used",world.workforce_used()},
                    {"goal_households_ready",world.settlement_goal_households_ready()},
                    {"goal_reached",world.settlement_goal_reached()},
                    {"goods_balance_valid",balanced},{"economy_valid",world.city_economy_valid()},
                    {"frames_rendered",rendered},{"frames_with_three_couriers",frames_with_three},
                    {"resume",{{"requested",resume_check},{"saved",saved},{"reparsed",reparsed},
                               {"fresh_world",fresh_world},{"direct_equal",direct_equal},
                               {"continued_equal",continued_equal}}}}.dump()<<'\n';
                view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
                return success ? 0:1;
            }
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
