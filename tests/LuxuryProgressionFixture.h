#pragma once

// On-demand supplied-data QA only. Every purchase uses ordinary World commands;
// telemetry and replay checks confer no saved gameplay authority.
#include "EliteStabilityStrategy.h"
#include "MarketDistributionMetrics.h"
#include "HuntingFixture.h"
#include "persistence/RecoveryStore.h"
#include "simulation/OriginalMarketPolicy.h"

#include <fstream>
#include <map>
#include <set>

namespace oe_luxury_progression {
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
namespace q=oe_starter_fixture;
namespace base=oe_elite_desirability_strategy;
namespace paid=oe_elite_stability_strategy;
namespace matrix=oe_high_tier_matrix;
namespace trace=oe_market_distribution_metrics;
using Json=nlohmann::json;
using q::check;
inline constexpr std::array luxury_goods{orig::Goods::Tea,orig::Goods::Silk,orig::Goods::Lacquerware};
inline constexpr std::string_view recipe="oe48-watch-phase1-third-food-three-markets";
// These two far-west, paused duplicate schools confer no new service and are
// outside the Elite's DES stencil. Retaining their unused channels would make
// the actual Rule29 wildlife plus Tea producer exceed the historical58 budget.
inline bool included(const base::Planned& node) {return paid::included(node)&&node.plan_id!=13&&node.plan_id!=15;}

inline void write(const fs::path& path,const Json& value) {
    check(!fs::exists(path),"Preserve existing QA output");
    std::ofstream file(path);file<<value.dump(2)<<'\n';file.close();check(bool(file),"Cannot publish private QA output");
}
inline bool below(const fs::path& child,const fs::path& parent) {
    auto c=child.begin();for(auto p=parent.begin();p!=parent.end();++p,++c)if(c==child.end()||*c!=*p)return false;
    return true;
}
inline void no_symlinks(const fs::path& path) {
    fs::path cursor;
    for(const auto& part:fs::absolute(path)) {
        cursor/=part;std::error_code error;const auto status=fs::symlink_status(cursor,error);
        check(error==std::errc::no_such_file_or_directory||!error,"Cannot inspect QA path ancestry");
        check(error||!fs::is_symlink(status),"QA paths must not traverse symlinks");
    }
}
inline fs::path output_root(const fs::path& requested,const fs::path& data) {
    no_symlinks(requested);const auto path=fs::weakly_canonical(fs::absolute(requested));
    const auto allowed=fs::weakly_canonical(fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac04");
    check(path!=allowed&&below(path,allowed)&&!below(path,data)&&!below(data,path)&&!fs::exists(path),
        "Use a fresh .local/agri-mac04 subdirectory disjoint from Original data");
    save::validate_save_target(path/"final.save.json",data);return path;
}
struct SourceGuard {
    struct Input {fs::path path;std::string bytes;fs::perms mode;fs::file_time_type modified;};
    std::vector<Input> inputs;
    explicit SourceGuard(const fs::path& data) {
        for(const auto* name:{"Model/EmperorBuildingModels.txt","Cities/Xia.map"}) {
            const auto path=data/name;no_symlinks(path);
            check(fs::is_regular_file(path)&&fs::file_size(path)<=64U*1024U*1024U,"Bounded supplied source input required");
            inputs.push_back({path,q::f::read_text(path),fs::status(path).permissions(),fs::last_write_time(path)});
        }
    }
    void unchanged()const {
        for(const auto& file:inputs)check(q::f::read_text(file.path)==file.bytes&&
            fs::status(file.path).permissions()==file.mode&&fs::last_write_time(file.path)==file.modified,
            "Original source bytes, permissions or modification time changed");
    }
};
struct Context {
    q::Context map;
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    explicit Context(const fs::path& data):map(q::Context::real(data)),
        model(save::prepare_playable_hunting_rules_model(data,orig::Difficulty::Normal)),
        terrain(save::prepare_playable_hunting_terrain_facts(data,map.map_relative)) {}
    sim::World world()const{return sim::World::create_playable_hunting(map.policy,model,terrain);}
    save::SaveDocument document(const sim::World& world)const{return save::make_document(map.data,map.map_relative,map.mask,world);}
    sim::World restore(const save::SaveDocument& document)const {
        check(document.source_schema_version==45&&document.world.rule_version==29,"Exact Rule29/schema45 input required");
        return save::restore_playable_hunting(document,map.data,map.mask,map.policy,model,terrain);
    }
};
inline void valid(const sim::World& world) {
    check(world.rule_version()==29,"Luxury QA changed gameplay generation");
    oe_hunting_fixture::valid(world);
    for(const auto good:luxury_goods) {
        const auto value=world.original_goods_balance(good);
        check(value.supported&&value.conserved&&!value.totals.imported&&!value.totals.demolished,"Luxury conservation/origin failed");
    }
    for(std::size_t i=0;i<sim::original_aux_material_count;++i)
        check(world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(i)).conserved,"Auxiliary conservation failed");
}
inline void attachment(const sim::World& world,const paid::Plan& plan,const base::Planned& node) {
    if(!node.owner)return;
    const auto parent=base::owner(world,paid::planned(plan,node.owner).cell),child=base::owner(world,node.cell);
    check(parent&&child,"Paid attachment lost actual owners");bool found=false;
    if(node.command==sim::CommandType::PlaceOriginalMilletField||node.command==sim::CommandType::PlaceOriginalWheatField) {
        for(const auto& row:world.hunting_food_state()->fields)found|=row.farm_id==*parent&&row.field_id==*child;
    } else if(node.command==sim::CommandType::PlaceOriginalHempField) {
        for(const auto& row:world.original_production_state()->hemp_farms)found|=row.farm_id==*parent&&row.field_id==child;
    } else {
        for(const auto& row:world.plantations_state()->plants)found|=row.source_id==*parent&&row.plant_id==*child;
    }
    check(found,"Rule29 attachment did not retain the selected paid source");
}
inline bool purchase_or_wait(sim::World& world,sim::Command command,const char* reason,Json& commands,Json& blocked) {
    const auto decision=world.validate(command);
    if(!decision.accepted&&decision.reason.find("overlaps actual wildlife, a Hunter or its begun edge")!=std::string::npos) {
        // The actual animal and begun section retain their claims. This is a
        // finite player retry on a later real tick, never an admission bypass.
        const auto value=static_cast<unsigned>(command.type);const Json cell={command.cell.x,command.cell.y};
        const bool logged=std::any_of(commands.begin(),commands.end(),[&](const Json& entry){return entry.at("command")==value&&
            entry.at("cell")==cell&&!entry.at("accepted").get<bool>();});
        if(!logged) {
            auto record=base::ordinary(world,command,reason);record["retry_after_real_ticks"]=true;commands.push_back(std::move(record));
        }
        return true;
    }
    return paid::append(world,command,reason,commands,blocked);
}
inline bool apply_recipe(sim::World& world,Json& commands,Json& blocked) {
    const auto plan=paid::plan_for(recipe);
    // Retain the historical player's finite road geometry and purchases while
    // checking attachments against the current generation's actual authority.
    for(const auto cell:base::roads)if(!paid::door_watch_cell(plan,cell,"oe45-grid-watch-near")&&!base::initial_road(world,cell))
        if(!purchase_or_wait(world,{sim::CommandType::PlaceRoad,cell},"MAC04 paid baseline road",commands,blocked))return false;
    if(!paid::grid_roads(world,plan,commands,blocked))return false;
    const auto unlocked=base::phase(world);
    for(const auto& node:plan) {
        if(!included(node)||node.phase>unlocked)continue;
        const auto kind=sim::placed_object(node.command);check(kind.has_value(),"Unmapped paid recipe command");
        if(const auto owner=base::owner(world,node.cell)) {
            check(world.building(*owner).kind==*kind,"Paid recipe site occupied by another body");attachment(world,plan,node);continue;
        }
        sim::Command command{node.command,node.cell};
        if(node.owner) {const auto parent=base::owner(world,paid::planned(plan,node.owner).cell);if(!parent)continue;command.building_id=*parent;}
        if(node.phase>=3&&world.treasury()<world.construction_cost(node.command)+paid::building_reserve(world,"oe45-grid-watch-near"))continue;
        if(!purchase_or_wait(world,command,"MAC04 actual population-gated purchase",commands,blocked))return false;
        if(base::owner(world,node.cell))attachment(world,plan,node);
    }
    if(unlocked>=2&&!base::elite(world))
        if(!purchase_or_wait(world,{sim::CommandType::PlaceOriginalElitePlot,base::elite_cell},"MAC04 separate paid Elite plot",commands,blocked))return false;
    for(const auto& node:plan)if(included(node)&&node.duplicate&&
        node.command!=sim::CommandType::PlaceOriginalWaterWell&&node.command!=sim::CommandType::PlaceOriginalAncestorShrine)
        if(const auto owner=base::owner(world,node.cell);owner&&world.building(*owner).operating_enabled)
            if(!paid::append(world,sim::set_building_operation(*owner,false),"MAC04 pause redundant paid provider",commands,blocked))return false;
    // Transactional commands can replace World's complete storage. Retain only
    // stable identities across execute(), then reacquire each current body.
    std::vector<sim::BuildingId> building_ids;
    for(const auto& building:world.buildings())building_ids.push_back(building.id);
    for(const auto id:building_ids) {
        const auto building=world.building(id);
        const bool critical=building.kind==sim::Object::OriginalWaterWell72||building.kind==sim::Object::OriginalFoodFarm||
            building.kind==sim::Object::Market||building.kind==sim::Object::FireWatch;
        if(critical&&building.workforce_priority!=sim::WorkforcePriority::High)
            if(!paid::append(world,sim::set_building_workforce_priority(building.id,sim::WorkforcePriority::High),
                "MAC04 paid critical workforce priority",commands,blocked))return false;
    }
    return true;
}
inline Json remaining_plan(const sim::World& world) {
    const auto plan=paid::plan_for(recipe);Json result=Json::array();
    for(const auto& node:plan)if(included(node)&&!base::owner(world,node.cell)) {
        sim::Command command{node.command,node.cell};bool owner_ready=!node.owner;
        if(node.owner)if(const auto parent=base::owner(world,paid::planned(plan,node.owner).cell)) {
            command.building_id=*parent;owner_ready=true;
        }
        const auto decision=world.validate(command);
        result.push_back({{"plan_id",node.plan_id},{"command",static_cast<unsigned>(node.command)},
            {"cell",{node.cell.x,node.cell.y}},{"required_population_phase",node.phase},{"current_population_phase",base::phase(world)},
            {"selected_parent_present",owner_ready},{"currently_legal",decision.accepted},{"current_validator_reason",decision.reason},
            {"quoted_cost",world.construction_cost(node.command)}});
    }
    return result;
}
inline Json house_report(const sim::World& world,const sim::OriginalHouseState& home) {
    const auto diagnosis=world.original_housing_diagnosis(home.building_id);auto query=diagnosis.actual;
    query.desirability=static_cast<std::int8_t>(diagnosis.cached_desirability);
    Json incoming=Json::array(),choices=Json::array();std::array<int,orig::goods_count> booked{};
    for(const auto& reservation:world.original_goods_state()->reservations)if(reservation.target==home.building_id) {
        if(reservation.phase==sim::OriginalGoodsTransferPhase::Reserved)booked[static_cast<unsigned>(reservation.good)]+=reservation.reserved;
        if(reservation.phase==sim::OriginalGoodsTransferPhase::Loaded)booked[static_cast<unsigned>(reservation.good)]+=reservation.loaded-reservation.delivered-reservation.returned;
        incoming.push_back(trace::goods_row(reservation));
    }
    for(const auto good:luxury_goods) {
        const auto need=sim::original_market_need(world.original_rules_model()->house_rules(),query,good,booked);
        choices.push_back({{"good",static_cast<unsigned>(good)},{"stock",orig::goods_stock(query,good)},
            {"booked_incoming",booked[static_cast<unsigned>(good)]},{"eligible_need",need?Json{{"tier",need->tier},{"stock",need->stock},{"demand",need->demand}}:Json(nullptr)}});
    }
    Json food=nullptr;for(const auto& row:world.hunting_food_state()->stocks)if(row.building_id==home.building_id)
        food={{"stock",row.amounts},{"received",row.received},{"consumed",row.consumed}};
    Json history=Json::array();
    for(const auto& row:world.original_production_state()->housing.houses)if(row.building_id==home.building_id)
        for(std::size_t i=0;i<row.first_transitions.size();++i)if(const auto& first=row.first_transitions[i])
            history.push_back({{"type",i+3},{"tick",first->evaluation.tick},{"kind",static_cast<unsigned>(first->kind)},
                {"admitted",first->admitted},{"actual_input",matrix::state(first->evaluation.input)},
                {"row_consumption_events",row.consumption_rows[i].events},{"row_consumed",row.consumption_rows[i].consumed}});
    return {{"house",static_cast<unsigned>(home.building_id)},{"actual",matrix::state(home.house)},
        {"raw_des",diagnosis.raw_desirability},{"cardinal_water_bonus",diagnosis.cardinal_water_bonus},{"query_des",diagnosis.cached_desirability},
        {"current",matrix::row(diagnosis.current,query,world.original_rules_model()->house_rules())},
        {"next",diagnosis.common_terminal||diagnosis.elite_terminal?Json(nullptr):matrix::row(diagnosis.next,query,world.original_rules_model()->house_rules())},
        {"terminal_in_own_family",diagnosis.common_terminal||diagnosis.elite_terminal},
        {"food",food},{"luxury_demand",choices},{"incoming",incoming},{"natural_first_transitions",history},
        {"ledger_and_staff",trace::owner(world,home.building_id)}};
}
inline Json city_report(const sim::World& world) {
    Json houses=Json::array(),sources=Json::array(),actors=Json::array(),plantations=Json::array(),industry=Json::array(),auxiliary=Json::array();
    for(const auto& home:world.original_economy_state()->houses)houses.push_back(house_report(world,home));
    for(const auto& body:world.buildings())if(!sim::original_house_kind(body.kind))sources.push_back(trace::owner(world,body.id));
    for(const auto& actor:world.couriers())if(trace::traced(actor.role))actors.push_back(trace::actor(world,actor));
    for(const auto& plant:world.plantations_state()->plants) {
        Json receipt=nullptr;if(plant.last_receipt)receipt={{"grower",static_cast<std::uint64_t>(plant.last_receipt->grower_id)},
            {"dispatch_tick",plant.last_receipt->dispatched_tick},{"picked_tick",plant.last_receipt->picked_tick},{"returned_tick",plant.last_receipt->returned_tick}};
        plantations.push_back({{"plant",static_cast<unsigned>(plant.plant_id)},{"source",static_cast<unsigned>(plant.source_id)},
            {"growth",plant.growth},{"productive_ticks",plant.productive_ticks},{"grown",plant.grown},{"harvested",plant.harvested},{"returned",plant.returned},{"last_receipt",receipt}});
    }
    for(const auto& source:world.original_production_state()->industry.sources)industry.push_back({
        {"source",static_cast<unsigned>(source.building_id)},{"kind",static_cast<unsigned>(world.building(source.building_id).kind)},
        {"completed",source.completed},{"goods_inputs_used",source.goods_inputs_used},{"aux_inputs_used",source.aux_inputs_used},
        {"clay_received",source.clay_received},{"clay_used",source.clay_used}});
    for(std::size_t i=0;i<sim::original_aux_material_count;++i) {
        const auto balance=world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(i));
        auxiliary.push_back({{"material",i},{"produced",balance.produced},{"stock",balance.stock},{"cargo",balance.cargo},
            {"consumed",balance.consumed},{"conserved",balance.conserved}});
    }
    return {{"tick",world.ticks()},{"population",world.total_population()},{"funds",world.treasury()},
        {"installed_workers",world.workforce_required()},{"active_workers",world.active_workforce_required()},{"workers_used",world.workforce_used()},
        {"building_count",world.buildings().size()},{"courier_count",world.couriers().size()},
        {"construction",world.construction_spent_total()},{"taxes",world.taxes_collected_total()},{"upkeep",world.maintenance_spent_total()},
        {"houses",houses},{"sources",sources},{"actors",actors},{"plantations",plantations},{"industry_recipe_inputs",industry},
        {"goods_balances",trace::balances(world)},{"auxiliary_balances",auxiliary}};
}
inline void checkpoint(const Context& context,const sim::World& world,const fs::path& output,const std::string& label,Json& manifest) {
    valid(world);const auto path=output/(label+".save.json");check(!fs::exists(path),"Preserve previous checkpoint");
    save::write_save(path,context.document(world),context.map.data,context.map.mask);
    const auto document=save::read_save(path);auto restored=context.restore(document);
    check(restored.snapshot()==world.snapshot(),"Exact schema45 checkpoint differs after restore");
    const auto roundtrip=output/(label+".roundtrip.json");save::write_save(roundtrip,context.document(restored),context.map.data,context.map.mask);
    check(q::f::read_text(roundtrip)==q::f::read_text(path),"Canonical schema45 checkpoint bytes differ");
    manifest.push_back({{"label",label},{"tick",world.ticks()},{"file",path.filename().string()},{"exact_roundtrip",true}});
}
class Observation {
    struct HousePrior {orig::HouseState house;std::array<std::uint64_t,orig::goods_count> consumed{};
        std::array<int,orig::goods_count> incoming{};std::int32_t des=0;};
    struct PostMaturityHouse {
        orig::HouseType initial_type=orig::HouseType::Vacant,final_type=orig::HouseType::Vacant;
        std::array<std::uint64_t,16> type_ticks{}; // Actual types2..17 only.
        std::uint64_t ticks=0,devolutions=0,burning_ticks=0,evaluations=0,current_unmet=0,next_applicable=0,next_unmet=0;
        std::int16_t minimum_occupancy=0;
    };
    std::map<sim::BuildingId,HousePrior> houses_;
    std::map<sim::BuildingId,PostMaturityHouse> post_maturity_houses_;
    std::map<std::pair<unsigned,std::uint64_t>,sim::OriginalGoodsReservation> transfers_;
    std::array<std::uint64_t,orig::goods_count> global_{};
    std::map<sim::BuildingId,sim::PlantationPlant> plants_;
    std::map<sim::BuildingId,sim::OriginalIndustrySource> processors_;
    bool initialized_=false;
public:
    Json transitions=Json::array(),consumption=Json::array(),deliveries=Json::array(),dispatches=Json::array(),upstream=Json::array();
    std::array<bool,3> earned{};std::optional<std::uint64_t> maturity;
    std::uint64_t devolutions=0,burning_house_ticks=0;std::int64_t minimum_funds=12000;
    std::uint64_t post_maturity_devolutions=0,post_maturity_burning_house_ticks=0;
    std::optional<std::int64_t> maturity_funds;std::uint64_t maturity_taxes=0,maturity_upkeep=0;
    Json post_maturity_houses()const {
        Json result=Json::array();
        for(const auto& [id,summary]:post_maturity_houses_) {
            Json durations=Json::array();
            for(std::size_t i=0;i<summary.type_ticks.size();++i)if(summary.type_ticks[i])
                durations.push_back({{"type",i+2},{"post_tick_count",summary.type_ticks[i]}});
            result.push_back({{"house",static_cast<unsigned>(id)},{"elite_family",orig::is_elite(summary.final_type)},
                {"begin_tick",*maturity},{"post_maturity_ticks",summary.ticks},
                {"initial_type",static_cast<unsigned>(summary.initial_type)},{"final_type",static_cast<unsigned>(summary.final_type)},
                {"minimum_occupancy",summary.minimum_occupancy},{"type_tick_counts",durations},
                {"devolutions",summary.devolutions},{"burning_ticks",summary.burning_ticks},
                {"actual_phase39_evaluations",summary.evaluations},{"current_unmet_evaluations",summary.current_unmet},
                {"next_applicable_evaluations",summary.next_applicable},{"next_unmet_evaluations",summary.next_unmet}});
        }
        return result;
    }
    void before(const sim::World& world) {
        if(!initialized_) {
            for(const auto& plant:world.plantations_state()->plants)plants_[plant.plant_id]=plant;
            for(const auto& source:world.original_production_state()->industry.sources)processors_[source.building_id]=source;
            for(const auto& row:world.original_goods_state()->reservations)
                transfers_[{static_cast<unsigned>(row.transfer.courier_id),row.transfer.sequence}]=row;
            initialized_=true;
        }
        houses_.clear();for(const auto& home:world.original_economy_state()->houses) {
            auto& prior=houses_[home.building_id];prior.house=home.house;
            prior.des=world.original_housing_diagnosis(home.building_id).cached_desirability;
            for(const auto& row:world.original_production_state()->house_consumption)if(row.building_id==home.building_id)prior.consumed=row.consumed;
            for(const auto& row:world.original_goods_state()->reservations)if(row.target==home.building_id) {
                if(row.phase==sim::OriginalGoodsTransferPhase::Reserved)prior.incoming[static_cast<unsigned>(row.good)]+=row.reserved;
                if(row.phase==sim::OriginalGoodsTransferPhase::Loaded)prior.incoming[static_cast<unsigned>(row.good)]+=row.loaded-row.delivered-row.returned;
            }
        }
        for(std::size_t g=0;g<orig::goods_count;++g)global_[g]=world.original_goods_balance(static_cast<orig::Goods>(g)).totals.consumed;
    }
    void after(const sim::World& world) {
        minimum_funds=std::min(minimum_funds,world.treasury());
        for(const auto& plant:world.plantations_state()->plants) {
            const auto previous=plants_.find(plant.plant_id);const auto old=previous==plants_.end()?sim::PlantationPlant{}:previous->second;
            if(plant.harvested>old.harvested||plant.returned>old.returned||(!old.productive_ticks&&plant.productive_ticks)) {
                Json receipt=nullptr;if(plant.last_receipt)receipt={{"dispatch",plant.last_receipt->dispatched_tick},
                    {"pickup",plant.last_receipt->picked_tick},{"return",plant.last_receipt->returned_tick},
                    {"grower",static_cast<std::uint64_t>(plant.last_receipt->grower_id)}};
                upstream.push_back({{"tick",world.ticks()},{"event","plantation_actual_work_pickup_or_return"},
                    {"plant",static_cast<unsigned>(plant.plant_id)},{"source",static_cast<unsigned>(plant.source_id)},
                    {"source_kind",static_cast<unsigned>(world.building(plant.source_id).kind)},
                    {"productive_ticks",plant.productive_ticks},{"harvested",plant.harvested},{"returned",plant.returned},{"last_receipt",receipt}});
            }
            plants_[plant.plant_id]=plant;
        }
        for(const auto& source:world.original_production_state()->industry.sources) {
            const auto previous=processors_.find(source.building_id);const auto old=previous==processors_.end()?sim::OriginalIndustrySource{}:previous->second;
            const auto kind=world.building(source.building_id).kind;
            if(source.completed>old.completed&&(kind==sim::Object::OriginalWeaver47||kind==sim::Object::OriginalLacquerwareMaker44)) {
                auto delta=source.aux_inputs_used;for(std::size_t i=0;i<delta.size();++i)delta[i]-=old.aux_inputs_used[i];
                upstream.push_back({{"tick",world.ticks()},{"event","processor_actual_completion_and_input_debit"},
                    {"source",static_cast<unsigned>(source.building_id)},{"source_kind",static_cast<unsigned>(kind)},
                    {"completed",source.completed},{"completed_delta",source.completed-old.completed},{"aux_debit",delta},
                    {"aux_inputs_cumulative",source.aux_inputs_used},{"goods_inputs_cumulative",source.goods_inputs_used}});
            }
            processors_[source.building_id]=source;
        }
        for(const auto& home:world.original_economy_state()->houses) {
            const auto& prior=houses_.at(home.building_id);burning_house_ticks+=world.building_on_fire(home.building_id);
            if(maturity) {
                post_maturity_burning_house_ticks+=world.building_on_fire(home.building_id);
                const auto [at,inserted]=post_maturity_houses_.try_emplace(home.building_id);auto& summary=at->second;
                check(post_maturity_houses_.size()<=sim::Rules::city_v10_household_limit,"Bounded per-House observation exceeded20 identities");
                if(inserted) {summary.initial_type=prior.house.type;summary.minimum_occupancy=prior.house.occupants;}
                summary.final_type=home.house.type;summary.minimum_occupancy=std::min(summary.minimum_occupancy,home.house.occupants);
                ++summary.ticks;++summary.type_ticks.at(static_cast<unsigned>(home.house.type)-2);
                summary.devolutions+=home.house.type<prior.house.type;summary.burning_ticks+=world.building_on_fire(home.building_id);
                for(const auto& history:world.original_production_state()->housing.houses)
                    if(history.building_id==home.building_id&&history.last_evaluation&&history.last_evaluation->tick==world.ticks()) {
                        const auto& input=history.last_evaluation->input;const auto& rules=world.original_rules_model()->house_rules();
                        ++summary.evaluations;summary.current_unmet+=!orig::needs_met(rules,orig::house_row(input.type),input);
                        // A terminal House has no next evolution within its
                        // family; a table neighbour is not a playable next row.
                        if(const auto next=orig::evolve_target(input.type);next&&orig::is_elite(*next)==orig::is_elite(input.type)) {
                            ++summary.next_applicable;summary.next_unmet+=!orig::needs_met(rules,orig::house_row(*next),input);
                        }
                    }
            }
            if(home.house.type!=prior.house.type) {
                if(home.house.type<prior.house.type) {++devolutions;if(maturity)++post_maturity_devolutions;}
                transitions.push_back({{"tick",world.ticks()},{"house",static_cast<unsigned>(home.building_id)},
                    {"from",static_cast<unsigned>(prior.house.type)},{"to",static_cast<unsigned>(home.house.type)},
                    {"occupants",home.house.occupants},{"diagnosis",house_report(world,home)}});
            }
            for(const auto& row:world.original_production_state()->house_consumption)if(row.building_id==home.building_id)
                for(std::size_t i=0;i<luxury_goods.size();++i) {
                    const auto good=luxury_goods[i];const auto g=static_cast<unsigned>(good);
                    check(row.consumed[g]>=prior.consumed[g],"House luxury sink regressed");const auto delta=row.consumed[g]-prior.consumed[g];
                    if(!delta)continue;
                    check(world.original_goods_balance(good).totals.consumed>=global_[g]+delta,"House debit lacks matching global consumption");
                    const auto& histories=world.original_production_state()->housing.houses;
                    const auto history=std::find_if(histories.begin(),histories.end(),[&](const auto& h){return h.building_id==home.building_id;});
                    const auto current=orig::house_row(home.house.type);const auto model_index=orig::model_row(current).value;
                    check(history!=histories.end()&&history->first_transitions[static_cast<unsigned>(home.house.type)-3]&&
                        world.original_rules_model()->house_rules().value(current,orig::goods_field(good))>0&&
                        history->consumption_rows[model_index].consumed[g]>0&&home.house.occupants>0,
                        "Luxury debit lacks a natural current-row transition and consumption witness");
                    earned[i]=true;consumption.push_back({{"tick",world.ticks()},{"house",static_cast<unsigned>(home.building_id)},
                        {"type",static_cast<unsigned>(home.house.type)},{"good",g},{"amount",delta},{"house_cumulative",row.consumed[g]},
                        {"before_tick",matrix::state(prior.house)},{"after_tick",matrix::state(home.house)},
                        {"global_cumulative",world.original_goods_balance(good).totals.consumed}});
                }
        }
        for(const auto& r:world.original_goods_state()->reservations) {
            if(std::find(luxury_goods.begin(),luxury_goods.end(),r.good)==luxury_goods.end())continue;
            const auto key=std::pair{static_cast<unsigned>(r.transfer.courier_id),r.transfer.sequence};const auto old=transfers_.find(key);
            if((old==transfers_.end()||old->second.phase!=sim::OriginalGoodsTransferPhase::Loaded)&&r.phase==sim::OriginalGoodsTransferPhase::Loaded) {
                auto event=trace::goods_row(r);event["tick"]=world.ticks();event["courier"]=key.first;
                event["source_kind"]=static_cast<unsigned>(world.building(r.source).kind);event["target_kind"]=static_cast<unsigned>(world.building(r.target).kind);
                event["actual_path_vertices"]=world.courier(r.transfer.courier_id).path.size();
                event["actual_dispatch_path"]=trace::cells(world.courier(r.transfer.courier_id).path);
                if(const auto home=houses_.find(r.target);home!=houses_.end()) {
                    auto query=home->second.house;query.desirability=static_cast<std::int8_t>(home->second.des);
                    const auto need=sim::original_market_need(world.original_rules_model()->house_rules(),query,r.good,home->second.incoming);
                    event["pre_tick_house"]=matrix::state(home->second.house);event["pre_tick_des"]=home->second.des;
                    event["pre_tick_incoming"]=home->second.incoming;
                    event["pre_tick_need"]=need?Json{{"tier",need->tier},{"stock_with_incoming",need->stock},{"demand",need->demand}}:Json(nullptr);
                    event["need_scope"]="Read-only pre-tick physical House/incoming query; actual reservation is the dispatch witness.";
                }
                dispatches.push_back(std::move(event));
            }
            const int previous=old==transfers_.end()?0:old->second.delivered;
            if(r.delivered>previous) {auto event=trace::goods_row(r);event["tick"]=world.ticks();event["courier"]=key.first;
                event["arrived_units"]=r.delivered-previous;event["source_kind"]=static_cast<unsigned>(world.building(r.source).kind);
                event["target_kind"]=static_cast<unsigned>(world.building(r.target).kind);deliveries.push_back(std::move(event));}
            transfers_[key]=r;
        }
        if(!maturity&&std::all_of(earned.begin(),earned.end(),[](bool value){return value;})) {
            maturity=world.ticks();maturity_funds=world.treasury();maturity_taxes=world.taxes_collected_total();maturity_upkeep=world.maintenance_spent_total();
        }
    }
};
} // namespace oe_luxury_progression
