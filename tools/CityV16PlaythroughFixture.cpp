// On-demand technical replay of normally paid commands on the user's real Xia.
// This is scripted simulation evidence, not a human construction playthrough.
#include "app/MenuStorage.h"
#include "app/ResourceLocator.h"
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
namespace oe = openemperor;
namespace sim = oe::simulation;
using Json = nlohmann::json;
const char* role_name(sim::CourierRole role) {
    switch (role) {
    case sim::CourierRole::None: return "None";
    case sim::CourierRole::Clay: return "Clay";
    case sim::CourierRole::Pottery: return "Pottery";
    case sim::CourierRole::Household: return "Household";
    case sim::CourierRole::Food: return "Food";
    case sim::CourierRole::Service: return "Service";
    case sim::CourierRole::MarketPotteryInbound: return "MarketPotteryInbound";
    case sim::CourierRole::MarketFoodInbound: return "MarketFoodInbound";
    case sim::CourierRole::MarketPotteryDistribution: return "MarketPotteryDistribution";
    case sim::CourierRole::MarketFoodDistribution: return "MarketFoodDistribution";
    case sim::CourierRole::FireInspector: return "FireInspector";
    case sim::CourierRole::HealthWorker: return "HealthWorker";
    }
    return "Unknown";
}
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
bool within(const fs::path& child, const fs::path& parent) {
    auto c = child.begin();
    for (auto p = parent.begin(); p != parent.end(); ++p, ++c)
        if (c == child.end() || *c != *p) return false;
    return true;
}
void write(const fs::path& path, const Json& value) {
    std::ofstream out(path);
    out << value.dump(2) << '\n';
    check(bool(out), "Private playthrough report write failed.");
}
void configure(oe::SandboxView& view, const oe::VisualSelection& selected) {
    view.set_walker_visuals(selected.walker, selected.walker_source);
    view.set_building_visuals(selected.building, selected.building_source);
    view.set_road_visuals(selected.road, selected.road_source);
    view.set_fire_visuals(selected.fire, selected.fire_source, selected.fire_fallback_reason);
    view.set_fire_inspector_visuals(selected.fire_inspector, selected.fire_inspector_source,
        selected.fire_inspector_fallback_reason);
    view.set_market_walker_visuals(selected.market_walker, selected.market_walker_source,
        selected.market_walker_fallback_reason);
    view.set_service_walker_visuals(selected.service_walker, selected.service_walker_source,
        selected.service_walker_fallback_reason);
    view.set_health_walker_visuals(selected.health_walker, selected.health_walker_source,
        selected.health_walker_fallback_reason);
}
Json facts(const sim::World& w) {
    Json z = {{"tick", w.ticks()}, {"profile", sim::rules_profile_name(w.profile())},
        {"rule", w.rule_version()}, {"policy", w.map_permissions()->policy_version()},
        {"funds", w.treasury()}, {"taxes", w.taxes_collected_total()},
        {"maintenance", w.maintenance_spent_total()}, {"rate", w.current_maintenance_rate()},
        {"construction_spent", w.construction_spent_total()}, {"population", w.total_population()},
        {"available_workers", w.workforce_supply()}, {"assigned_workers", w.workforce_used()},
        {"active_demand", w.active_workforce_required()}, {"installed_demand", w.workforce_required()},
        {"goal", w.settlement_goal_reached()}, {"effective_level2", w.settlement_goal_households_ready()},
        {"clay_extracted", w.clay_extracted_total()}, {"pottery_produced", w.pottery_completed_total()},
        {"food_produced", w.food_produced_total()}, {"buildings", Json::array()},
        {"houses", Json::array()}, {"couriers", Json::array()}};
    for (const auto& b : w.buildings()) {
        z["buildings"].push_back({{"id", static_cast<unsigned>(b.id)},
            {"kind", static_cast<unsigned>(b.kind)}, {"origin", {b.cell.x, b.cell.y}},
            {"placed_tick", b.placed_tick}, {"output", b.output}, {"input_clay", b.input_clay},
            {"pottery", b.pottery_stock}, {"food", b.food_stock},
            {"nonfood_incoming", b.reserved_incoming}, {"food_incoming", b.reserved_food_incoming},
            {"workers", w.workers_assigned(b.id)}, {"required", w.workforce_required(b.id)},
            {"running", b.operating_enabled}, {"priority", static_cast<unsigned>(b.workforce_priority)},
            {"burning", w.building_on_fire(b.id)}});
        if (b.kind != sim::Object::Household) continue;
        z["houses"].push_back({{"id", static_cast<unsigned>(b.id)}, {"origin", {b.cell.x, b.cell.y}},
            {"food", b.food_stock}, {"pottery", b.pottery_stock}, {"food_incoming", b.reserved_food_incoming},
            {"pottery_incoming", b.reserved_incoming}, {"service_until", b.service_until_tick},
            {"service", w.household_service_active(b.id)}, {"water", w.household_has_water(b.id)},
            {"desirability", w.household_desirability(b.id)}, {"health_risk", b.health_risk},
            {"health_protection_until", b.health_protection_until_tick}, {"sick_until", b.sick_until_tick},
            {"sick", w.household_sick(b.id)}, {"fire_risk", b.fire_risk},
            {"fire_protection_until", b.fire_protection_until_tick}, {"fire_until", b.fire_until_tick},
            {"burning", w.building_on_fire(b.id)}, {"fulfilled", b.fulfilled_demand},
            {"missed", b.missed_demand}, {"historical_level", w.historical_household_level(b.id)},
            {"effective_level", w.household_level(b.id)}, {"population", b.population},
            {"capacity", w.household_population_capacity(b.id)}, {"actual_tax", b.taxes_paid_total}});
    }
    for (const auto& c : w.couriers()) {
        Json row = {{"id", static_cast<unsigned>(c.id)}, {"role", static_cast<unsigned>(c.role)},
            {"logical_role", role_name(c.role)},
            {"owner", static_cast<unsigned>(c.owner)}, {"target", static_cast<unsigned>(c.target)},
            {"good", static_cast<unsigned>(c.good)}, {"phase", sim::courier_phase_name(c.phase)},
            {"cargo", c.cargo}, {"reserved", c.reserved}, {"pending", c.route_pending},
            {"path_vertex", c.path_vertex}, {"edge_progress", c.edge_progress},
            {"dispatch_status", sim::courier_dispatch_status_name(w.courier_dispatch_status(c.id).status)},
            {"path", Json::array()}};
        for (auto p : c.path) row["path"].push_back({p.x, p.y});
        if (auto p = w.courier_position(c.id)) row["position"] = {p->x, p->y};
        z["couriers"].push_back(std::move(row));
    }
    z["house_count"]=z["houses"].size();
    return z;
}
void observe_logistics(Json& log, const sim::World& w) {
    const auto maximum=[&](const char* name, int value) {
        log["maximum_units"][name]=std::max(log["maximum_units"].value(name,0),value);
    };
    int clay_incoming=0,pottery_incoming=0,food_incoming=0;
    int clay_cargo=0,pottery_cargo=0,food_cargo=0;
    for (const auto& b:w.buildings()) {
        if (b.kind==sim::Object::Pottery) clay_incoming+=b.reserved_incoming;
        else pottery_incoming+=b.reserved_incoming;
        food_incoming+=b.reserved_food_incoming;
        if (b.kind==sim::Object::ClaySource) maximum("clay_source_output",b.output);
        if (b.kind==sim::Object::Pottery) {
            maximum("pottery_input_clay",b.input_clay); maximum("pottery_output",b.output);
        }
        if (b.kind==sim::Object::Warehouse) maximum("warehouse_pottery",b.pottery_stock);
        if (b.kind==sim::Object::Farm) maximum("farm_output",b.output);
        if (b.kind==sim::Object::Market) {
            maximum("market_pottery",b.pottery_stock); maximum("market_food",b.food_stock);
        }
        if (b.kind==sim::Object::Household) {
            maximum("house_pottery",b.pottery_stock); maximum("house_food",b.food_stock);
        }
    }
    for (const auto& c:w.couriers()) {
        auto& row=log["post_tick_status_exposures"][role_name(c.role)];
        if (row.is_null()) {
            row=Json::object();
            for (unsigned i=0;i<=static_cast<unsigned>(sim::CourierDispatchStatus::OnFire);++i)
                row[sim::courier_dispatch_status_name(static_cast<sim::CourierDispatchStatus>(i))]=0;
        }
        const auto status=sim::courier_dispatch_status_name(w.courier_dispatch_status(c.id).status);
        row[status]=row[status].get<std::uint64_t>()+1;
        maximum("individual_cargo",c.cargo); maximum("individual_reservation",c.reserved);
        if (c.good==sim::Good::Clay) clay_cargo+=c.cargo;
        if (c.good==sim::Good::Pottery) pottery_cargo+=c.cargo;
        if (c.good==sim::Good::Food) food_cargo+=c.cargo;
    }
    maximum("clay_incoming_total",clay_incoming); maximum("pottery_incoming_total",pottery_incoming);
    maximum("food_incoming_total",food_incoming); maximum("clay_cargo_total",clay_cargo);
    maximum("pottery_cargo_total",pottery_cargo); maximum("food_cargo_total",food_cargo);
}
void invariants(const sim::World& w) {
    check(w.production_balance_valid() && w.food_balance_valid() && w.city_economy_valid() &&
        w.navigation_valid() && w.population_valid() && w.service_state_valid() &&
        w.health_state_valid() && w.fire_state_valid(), "Playthrough invariant failure.");
}
struct Action {
    std::uint64_t tick;
    sim::CommandType type;
    sim::Cell cell;
    const char* name;
};
std::vector<Action> recipe() {
    std::vector<Action> a;
    const auto add = [&](std::uint64_t t, sim::CommandType type, int x, int y, const char* name) {
        a.push_back({t, type, {x,y}, name});
    };
    const auto road = [&](std::uint64_t t, int x, int y) { add(t,sim::CommandType::PlaceRoad,x,y,"road"); };
    add(800,sim::CommandType::PlaceWell,114,116,"well");
    add(1200,sim::CommandType::PlaceHousehold,116,113,"house");
    road(1200,118,115); road(1200,119,115);
    add(1600,sim::CommandType::PlaceHousehold,119,113,"house");
    add(2000,sim::CommandType::PlaceMarket,118,116,"market");
    road(2000,120,115);
    for (int y=115;y<=119;++y) road(2000,121,y);
    for (int x=112;x<=124;++x) if (x!=121) road(2000,x,119);
    road(2000,117,120); road(2000,117,121);
    add(2400,sim::CommandType::PlaceServicePost,117,122,"service");
    add(2400,sim::CommandType::PlaceFireWatch,111,114,"fire_watch");
    for (int y=116;y<=122;++y) road(2400,105,y);
    for (int x=103;x<=104;++x) { road(2400,x,119); road(2400,x,122); }
    add(2800,sim::CommandType::PlaceClaySource,103,120,"clay");
    add(3200,sim::CommandType::PlacePottery,103,123,"pottery");
    add(3200,sim::CommandType::PlaceWell,112,120,"well");
    add(3600,sim::CommandType::PlaceWell,122,113,"well");
    add(3600,sim::CommandType::PlaceHousehold,119,116,"house");
    add(4000,sim::CommandType::PlaceHousehold,122,116,"house");
    add(4000,sim::CommandType::PlaceHousehold,115,120,"house");
    add(4400,sim::CommandType::PlaceHousehold,118,120,"house");
    for (int y=120;y<=122;++y) road(4400,114,y);
    add(4400,sim::CommandType::PlaceHealthPost,114,123,"health");
    add(4800,sim::CommandType::PlaceWell,114,113,"well");
    return a;
}
Json events(const sim::WorldSnapshot& before, const sim::World& w) {
    Json out = Json::array();
    for (const auto& c : w.couriers()) {
        const auto prior = std::find_if(before.couriers.begin(), before.couriers.end(),
            [&](const auto& p) { return p.id==c.id; });
        if (prior==before.couriers.end() || prior->phase==c.phase) continue;
        const char* kind = c.phase==sim::CourierPhase::ToWarehouse ? "dispatch":
            c.phase==sim::CourierPhase::Returning ? "arrival":"home";
        out.push_back({{"event",kind},{"tick",w.ticks()},{"courier",static_cast<unsigned>(c.id)},
            {"role",static_cast<unsigned>(c.role)},{"owner",static_cast<unsigned>(c.owner)},
            {"logical_role",role_name(c.role)},{"good",static_cast<unsigned>(c.good)},
            {"target",static_cast<unsigned>(c.target)}, {"prior_cargo",prior->cargo},
            {"cargo",c.cargo},{"reserved",c.reserved},{"path_vertices",c.path.size()}});
    }
    for (const auto& b : w.buildings()) {
        const auto prior = std::find_if(before.buildings.begin(), before.buildings.end(),
            [&](const auto& p) { return p.id==b.id; });
        if (prior==before.buildings.end()) continue;
        if (b.kind==sim::Object::Household && prior->health_protection_until_tick!=b.health_protection_until_tick)
            out.push_back({{"event","health_arrival"},{"tick",w.ticks()},{"building",static_cast<unsigned>(b.id)},
                {"actual_cure",prior->sick_until_tick>w.ticks()},{"prior_sick_until",prior->sick_until_tick},
                {"sick_until",b.sick_until_tick},{"protection_until",b.health_protection_until_tick}});
        if (prior->fire_protection_until_tick!=b.fire_protection_until_tick)
            out.push_back({{"event","inspector_arrival"},{"tick",w.ticks()},{"building",static_cast<unsigned>(b.id)},
                {"actual_extinguish",prior->fire_until_tick>w.ticks()},
                {"prior_fire_until",prior->fire_until_tick},{"fire_until",b.fire_until_tick}});
        if (prior->fire_until_tick<=w.ticks() && b.fire_until_tick>w.ticks())
            out.push_back({{"event","natural_fire"},{"tick",w.ticks()},{"building",static_cast<unsigned>(b.id)},
                {"until",b.fire_until_tick}});
        if (b.fulfilled_demand!=prior->fulfilled_demand || b.missed_demand!=prior->missed_demand)
            out.push_back({{"event","demand"},{"tick",w.ticks()},{"house",static_cast<unsigned>(b.id)},
                {"success",b.fulfilled_demand>prior->fulfilled_demand},
                {"actual_tax",b.taxes_paid_total-prior->taxes_paid_total},
                {"prior_sick",prior->sick_until_tick>w.ticks()}, {"prior_fire",prior->fire_until_tick>w.ticks()},
                {"food_before",prior->food_stock},{"pottery_before",prior->pottery_stock},
                {"service_until_before",prior->service_until_tick}});
    }
    return out;
}
} // namespace

int main(int argc, char** argv) {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    try {
        check(argc==3 || argc==4, "Usage: openemperor-city-v16-playthrough-fixture <original-data> <fresh-app-root> [resource-root]");
        const auto data=fs::canonical(argv[1]), root=fs::canonical(argv[2]);
        check(fs::is_directory(root) && fs::is_empty(root) && !within(root,data),
            "Fresh empty isolated app root outside original data required.");
        const auto resources=argc==4 ? fs::canonical(argv[3]):
            oe::locate_resource_root(fs::canonical(argv[0]).parent_path());
        const auto selected=oe::detect_and_select_visual_profiles(data,resources);
        auto stored=oe::maps::load_stored_map_session(data,"Cities/Xia.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);
        const auto border=stored.plan.border;
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        check(SDL_CreateWindowAndRenderer("Technical paid City-v16 playthrough",1100,700,
            SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        oe::SandboxView view(std::move(stored),true,sim::RulesProfile::CityV16,3);
        fs::create_directory(root/"saves");
        view.configure_save(data,"Cities/Xia.map",root/"saves/A-paid-starter.json");
        configure(view,selected); view.initialize(window,renderer);
        check(view.world().ticks()==0 && view.world().construction_spent_total()==1280 &&
            view.world().treasury()==20 && view.world().workforce_supply()==24 &&
            view.world().workforce_used()==24 && view.world().water_covered_households()==0,
            "Exact dry paid1280/1300 and24/24 starter changed.");
        auto replay=sim::World::restore(view.world().snapshot(),view.world().map_permissions());
        Json report={{"evidence","Technical original-Xia paid command replay; not human UI acceptance"},
            {"starter",facts(view.world())},{"commands",Json::array()},{"checkpoints",Json::array()},
            {"main_events",Json::array()},{"stability",Json::object()}};
        report["logistics"]={{"observation","One post-tick query per existing Courier after that tick's commands; current allocation, not the captured tick-start staffing or a predicted dispatch"},
            {"post_tick_status_exposures",Json::object()},{"maximum_units",Json::object()},
            {"actual_phase_transition_counts",Json::object()}};
        Json schedule={{"border",border},{"actions",Json::array()},{"checkpoints",Json::array()},
            {"end_tick",9200}};
        const auto save_world=[&](const sim::World& w, const std::string& label) {
            auto doc=view.capture_save_document(); doc.world=w.snapshot();
            const auto path=root/"saves"/(label+".json");
            oe::persistence::write_save(path,doc,data,view.buildable_mask());
            const auto read=oe::persistence::read_save(path);
            auto restored=oe::persistence::restore_save(read,data,view.buildable_mask(),w.map_permissions());
            check(read.source_schema_version==19 && read.world==doc.world && restored.snapshot()==w.snapshot() &&
                read.map_permissions_policy_version==1 && read.map_permissions_sha256==doc.map_permissions_sha256 &&
                read.map_sha256==doc.map_sha256 && read.buildable_sha256==doc.buildable_sha256,
                "Full schema19/WorldSnapshot/map-policy checkpoint mismatch.");
            auto direct=w;
            for (int i=0;i<20;++i) {
                direct.tick(); restored.tick();
                check(direct.snapshot()==restored.snapshot(),"Checkpoint continuation changed full snapshot.");
            }
            return path;
        };
        const auto checkpoint=[&](const std::string& label) {
            const auto path=save_world(view.world(),label);
            schedule["checkpoints"].push_back({{"label",label},{"tick",view.world().ticks()},
                {"expected",path.string()}});
            report["checkpoints"].push_back({{"label",label},{"facts",facts(view.world())},
                {"full_snapshot_equal",true},{"schema",19},{"policy",1},{"continuation_ticks",20}});
        };
        checkpoint("A-paid-starter");
        const auto actions=recipe(); std::size_t next=0;
        std::ofstream trace(root/"main-facts.jsonl");
        trace<<facts(view.world()).dump()<<'\n';
        std::optional<sim::World> e_world, goal_world;
        bool actual_cure=false, prevention=false, service_arrival=false, inspector_arrival=false;
        int minimum_population=1000000;
        for (std::uint64_t tick=1;tick<=9200;++tick) {
            const auto before=view.world().snapshot();
            view.tick_once(); replay.tick();
            check(view.world().snapshot()==replay.snapshot(),"Identical paid ticks diverged in full WorldSnapshot.");
            for (const auto& event:events(before,view.world())) {
                report["main_events"].push_back(event);
                if (event["event"]=="dispatch" || event["event"]=="arrival" || event["event"]=="home") {
                    auto& count=report["logistics"]["actual_phase_transition_counts"][event["logical_role"].get<std::string>()]
                        [event["event"].get<std::string>()];
                    count=count.is_null() ? 1:count.get<std::uint64_t>()+1;
                }
                if (event["event"]=="health_arrival") {
                    actual_cure=actual_cure || event["actual_cure"].get<bool>();
                    prevention=prevention || !event["actual_cure"].get<bool>();
                }
                if (event["event"]=="inspector_arrival") inspector_arrival=true;
                if (event["event"]=="arrival" && event["role"]==static_cast<unsigned>(sim::CourierRole::Service))
                    service_arrival=true;
            }
            while (next<actions.size() && actions[next].tick==tick) {
                const auto& a=actions[next++]; const auto prior=view.world().treasury();
                const auto first=view.execute({a.type,a.cell}), second=replay.execute({a.type,a.cell});
                check(first.accepted && first.changed && second.accepted && second.changed,
                    "Paid recipe rejected at tick"+std::to_string(tick)+" ("+a.name+"): "+first.reason);
                check(view.world().snapshot()==replay.snapshot(),"Identical paid commands diverged in full snapshot.");
                Json action={{"tick",tick},{"type",a.name},{"x",a.cell.x},{"y",a.cell.y},
                    {"cost",prior-view.world().treasury()},{"funds_after",view.world().treasury()}};
                schedule["actions"].push_back(action); action["actual_after"]=facts(view.world());
                report["commands"].push_back(std::move(action));
            }
            observe_logistics(report["logistics"],view.world());
            if (tick==400) checkpoint("B-first-revenue");
            if (tick==1200) checkpoint("C-first-house");
            if (tick==1600) checkpoint("D-six-houses");
            if (tick==4406) {
                checkpoint("E-active-transport"); e_world=view.world();
                check(std::any_of(e_world->couriers().begin(),e_world->couriers().end(),[](const auto& c) {
                    return c.role==sim::CourierRole::HealthWorker && c.phase==sim::CourierPhase::ToWarehouse &&
                        c.edge_progress>0;
                }),"E does not contain an actual active Health trip.");
            }
            if (tick==4426) {
                const auto path=save_world(view.world(),"E-plus20");
                schedule["restart_continuation"]={{"from","E-active-transport"},{"ticks",20},
                    {"tick",tick},{"expected",path.string()}};
            }
            if (view.world().settlement_goal_reached() && !goal_world) {
                check(tick==6800,"First full goal changed from measured paid recipe.");
                goal_world=view.world(); checkpoint("F-goal"); report["first_goal"]=facts(view.world());
            }
            if (goal_world) {
                check(view.world().settlement_goal_reached(),"Goal lost during fixed-building stability window.");
                minimum_population=std::min(minimum_population,view.world().total_population());
            }
            if (tick%100==0) trace<<facts(view.world()).dump()<<'\n';
        }
        check(next==actions.size() && goal_world && actual_cure && prevention && service_arrival && inspector_arrival,
            "Paid goal/shared actual Service/Fire/Health evidence incomplete.");
        invariants(view.world()); checkpoint("G-stability");
        const auto& final=view.world();
        check(final.ticks()-goal_world->ticks()==2400 &&
            final.construction_spent_total()==goal_world->construction_spent_total() &&
            final.snapshot().command_sequence==goal_world->snapshot().command_sequence,
            "Stability window received construction or other commands.");
        report["stability"]={{"ticks",2400},{"no_construction",final.construction_spent_total()==goal_world->construction_spent_total()},
            {"goal_every_tick",true},{"minimum_population",minimum_population},
            {"tax_delta",final.taxes_collected_total()-goal_world->taxes_collected_total()},
            {"maintenance_delta",final.maintenance_spent_total()-goal_world->maintenance_spent_total()},
            {"funds_delta",final.treasury()-goal_world->treasury()},{"per_house",Json::array()}};
        for (const auto& b:final.buildings()) if (b.kind==sim::Object::Household) {
            const auto& prior=goal_world->building(b.id);
            check(b.fulfilled_demand-prior.fulfilled_demand==6 && b.missed_demand==prior.missed_demand,
                "House demand failed in the2400-tick stability window.");
            report["stability"]["per_house"].push_back({{"id",static_cast<unsigned>(b.id)},
                {"success_delta",6},{"miss_delta",0},{"actual_tax_delta",b.taxes_paid_total-prior.taxes_paid_total}});
        }
        report["full_snapshot_determinism"]={{"ticks",9200},{"after_every_tick_and_command",true}};
        report["shared_systems"]={{"actual_natural_sickness_cured",actual_cure},
            {"actual_health_prevention",prevention},{"actual_service_arrival",service_arrival},
            {"actual_inspector_arrival",inspector_arrival}};
        report["final"]=facts(final);
        // Separate ordinary-command road interruption; the main goal city is untouched.
        auto cut=*e_world;
        const sim::Cell bridge{121,118};
        std::optional<sim::CourierId> trip;
        for (int i=0;i<2400 && !trip;++i) {
            for (const auto& c:cut.couriers()) {
                if (c.phase!=sim::CourierPhase::ToWarehouse || c.cargo<=0 || c.edge_progress==0 ||
                    c.path_vertex+2>=c.path.size()) continue;
                if (std::find(c.path.begin()+static_cast<std::ptrdiff_t>(c.path_vertex+2),c.path.end(),bridge)==c.path.end()) continue;
                if (cut.validate({sim::CommandType::RemoveRoad,bridge}).accepted) { trip=c.id; break; }
            }
            if (!trip) cut.tick();
        }
        check(trip.has_value(),"No real loaded future-edge Road cut candidate.");
        const auto courier_id=*trip;
        const auto original=cut.courier(courier_id);
        const auto start_tick=cut.ticks();
        const auto before_cut=save_world(cut,"road-before-cut");
        check(cut.execute({sim::CommandType::RemoveRoad,bridge}).accepted,"Legal future Road cut rejected.");
        const auto begun_to=original.path[original.path_vertex+1];
        bool completed_edge=false;
        for (int i=0;i<1000 && !(cut.courier(courier_id).route_pending &&
                cut.courier(courier_id).edge_progress==0);++i) {
            cut.tick(); const auto& c=cut.courier(courier_id);
            completed_edge=completed_edge || (c.path[c.path_vertex]==begun_to && c.edge_progress==0);
            check(c.cargo==original.cargo && c.reserved==original.reserved,"Interrupted goods/reservation changed before arrival.");
        }
        check(completed_edge && cut.courier(courier_id).route_pending && cut.courier(courier_id).edge_progress==0,
            "Road cut did not complete begun edge and wait at real waypoint.");
        const auto wait_tick=cut.ticks(); const auto waiting=cut.courier(courier_id);
        const auto waiting_save=save_world(cut,"road-waiting");
        for (int i=0;i<30;++i) {
            cut.tick(); const auto& c=cut.courier(courier_id);
            check(c.cargo==waiting.cargo && c.reserved==waiting.reserved && c.path_vertex==waiting.path_vertex &&
                c.edge_progress==0 && c.route_pending,"Waiting shipment moved or lost goods.");
        }
        const auto repair_tick=cut.ticks();
        const auto funds=cut.treasury();
        check(cut.execute({sim::CommandType::PlaceRoad,bridge}).accepted && cut.treasury()==funds-2,
            "Road repair was not normally paid.");
        bool delivered=false,home=false; std::uint64_t arrival_tick=0,home_tick=0;
        for (int i=0;i<2000 && !home;++i) {
            cut.tick(); const auto& c=cut.courier(courier_id);
            if (!delivered && c.phase==sim::CourierPhase::Returning && c.cargo==0 && c.reserved==0) {
                delivered=true; arrival_tick=cut.ticks(); save_world(cut,"road-delivered");
            }
            if (delivered && c.phase==sim::CourierPhase::IdleAtWorkshop) { home=true; home_tick=cut.ticks(); }
        }
        check(delivered && home,"Repaired shipment did not deliver and return normally.");
        save_world(cut,"road-home"); invariants(cut);
        report["road_branch"]={{"start_tick",start_tick},{"courier",static_cast<unsigned>(courier_id)},
            {"role",static_cast<unsigned>(original.role)},{"cargo",original.cargo},{"reserved",original.reserved},
            {"bridge",{bridge.x,bridge.y}},{"completed_begun_edge",completed_edge}, {"wait_tick",wait_tick},
            {"wait_ticks",30},{"repair_tick",repair_tick},{"repair_cost",2},
            {"arrival_tick",arrival_tick},{"home_tick",home_tick},{"goods_and_reservations_retained",true},
            {"before",before_cut.string()},{"waiting",waiting_save.string()}};
        // Pause both real Watches in a separate earned city; natural risk creates a genuine incident.
        auto incident=final; std::vector<sim::BuildingId> watches;
        for (const auto& b:incident.buildings()) if (b.kind==sim::Object::FireWatch) watches.push_back(b.id);
        for (const auto id:watches) check(incident.execute(sim::set_building_operation(id,false)).accepted,"Watch pause rejected.");
        Json fire_events=Json::array(); bool burning_house=false, burning_miss=false, extinguished=false;
        std::uint64_t fire_tick=0,resume_tick=0;
        for (int i=0;i<10000 && !burning_miss;++i) {
            const auto prior=incident.snapshot(); incident.tick();
            for (const auto& event:events(prior,incident)) {
                fire_events.push_back(event);
                if (event["event"]=="natural_fire" &&
                    incident.building(static_cast<sim::BuildingId>(event["building"].get<unsigned>())).kind==sim::Object::Household) {
                    if (!burning_house) { fire_tick=incident.ticks(); save_world(incident,"fire-natural-incident"); }
                    burning_house=true;
                }
                if (event["event"]=="demand" && event["prior_fire"].get<bool>() && !event["success"].get<bool>())
                    burning_miss=true;
            }
        }
        check(burning_house && burning_miss,"Paused-Watch branch did not create a genuine fire/demand miss.");
        resume_tick=incident.ticks();
        for (const auto id:watches) check(incident.execute(sim::set_building_operation(id,true)).accepted,"Watch resume rejected.");
        for (int i=0;i<1200 && !extinguished;++i) {
            const auto prior=incident.snapshot(); incident.tick();
            for (const auto& event:events(prior,incident)) {
                fire_events.push_back(event);
                if (event["event"]=="inspector_arrival" && event["actual_extinguish"].get<bool>()) extinguished=true;
            }
        }
        check(extinguished,"Actual Inspector arrival did not extinguish a still-burning building.");
        save_world(incident,"fire-actual-extinguished"); invariants(incident);
        report["fire_branch"]={{"kind","Separate normally earned city; legal Watch pause/resume and natural risk, no injected incident"},
            {"start_tick",final.ticks()},{"first_house_fire_tick",fire_tick},{"resume_tick",resume_tick},
            {"actual_extinguish_tick",incident.ticks()},{"burning_house_demand_missed",burning_miss},
            {"events",fire_events},{"final",facts(incident)}};
        check(final.snapshot()==replay.snapshot(),"Separate branches changed the main paid city.");
        check(bool(trace),"Main trace write failed.");
        write(root/"application-schedule.json",schedule); write(root/"fixture-report.json",report);
        oe::menu::Settings settings; settings.data_root=data; settings.last_map="Cities/Xia.map";
        settings.profile=sim::RulesProfile::CityV16; settings.prepared_starter=true;
        settings.autosave_enabled=true; settings.last_save=root/"saves/A-paid-starter.json";
        oe::menu::write_settings(root,settings);
        view.shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::cout<<"TECHNICAL CITY V16: paid Xia goal at 6800, 10 Houses / 8 effective L2 / 126 residents; sustained through 9200. "
            <<"Load Sandbox -> Load selected save starts the unchanged paid tick-zero starter.\n";
        return 0;
    } catch (const std::exception& e) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit(); std::cerr<<"City-v16 playthrough fixture: "<<e.what()<<'\n'; return 1;
    }
}
