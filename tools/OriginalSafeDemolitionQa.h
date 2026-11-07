#pragma once

// Explicit OE51 paid acceptance only. All mutation goes through the existing
// World command path. This helper never advances time or assigns saved state.
#include "../tests/EliteStabilityStrategy.h"

#include <cmath>

namespace oe_safe_demolition_qa {
namespace sim=openemperor::simulation;
namespace q=oe_starter_fixture;
namespace paid=oe_elite_stability_strategy;
using Json=nlohmann::json;
class Journey {
public:
    bool complete() const {return stage_==Stage::Done;}
    std::optional<std::uint64_t> finished_tick() const {return finished_;}
    Json report() const {return {{"format","openemperor-oe51-paid-demolition-v1"},{"status",complete()?"PASS":"PARTIAL"},
        {"begin_tick",begin_?Json(*begin_):Json(nullptr)},{"finished_tick",finished_?Json(*finished_):Json(nullptr)},
        {"snapshots",observations_},{"events",events_},{"retirements",retirements_},{"journey_retirement_limit",4},
        {"world_retirement_limit",sim::original_safe_demolition_retired_limit},
        {"world_commands_only",true},{"stock_population_or_finance_injection",false},
        {"empty_house_population_change","0 -> 0; occupied House removal remains refused"},
        {"native_human","NOT_RUN"}};}
    // Called once after each ordinary World tick. The caller brackets that
    // tick with its existing observers and checkpoints the final command state.
    void after_tick(sim::World& world,Json& commands,Json& blocked) {
        if (complete()) return;
        q::check(world.original_safe_demolition_enabled(),"OE51 journey requires exact Rule19");
        if (!begin_) {
            begin_=world.ticks();
            q::check(world.original_production_state()->safe_demolition->retired.empty(),"OE51 creator must start without retired purchases");
            for (const auto& body:world.buildings()) original_bodies_.push_back(body.id);
            q::check(world.original_economy_state()->houses.size()==13,"OE51 paid entry requires the actual thirteen-House city");
            observations_.push_back(facts(world,"before-demolition"));
            const auto house_cell=empty_site(world,sim::CommandType::PlaceHousehold,{});
            const auto house=purchase(world,sim::CommandType::PlaceHousehold,house_cell,commands,blocked);
            retire(world,house,commands,blocked);
            const auto rebuilt=purchase(world,sim::CommandType::PlaceHousehold,house_cell,commands,blocked);
            q::check(rebuilt>house,"OE51 House rebuild recycled its purchase identity");retire(world,rebuilt,commands,blocked);
            for (const auto& body:world.buildings()) if (body.kind==sim::Object::ClaySource) {
                original_clay_.push_back(body.id);if(body.operating_enabled)paused_.push_back(body.id);
            }
            for (const auto id:paused_) command(world,sim::set_building_operation(id,false),"OE51 stop existing paid Clay production while real deliveries settle",commands,blocked);
            stage_=Stage::DrainExisting;
        }
        q::check(world.ticks()-*begin_<=2048,"OE51 bounded Clay drain/return did not settle; preserve the actual state for diagnosis");
        if (stage_==Stage::DrainExisting) {
            for (const auto& actor:world.couriers()) if (std::find(original_clay_.begin(),original_clay_.end(),actor.owner)!=original_clay_.end() &&
                (actor.phase!=sim::CourierPhase::IdleAtWorkshop || actor.cargo || actor.reserved)) return;
            std::optional<sim::Cell> kiln;
            for (const auto& body:world.buildings()) if (body.kind==sim::Object::Pottery && body.operating_enabled &&
                !world.building_on_fire(body.id) && world.workers_assigned(body.id)==world.workforce_required(body.id) &&
                body.input_clay+body.reserved_incoming<=sim::Rules::pottery_input_capacity-2) {kiln=body.cell;break;}
            if (!kiln) return;
            clay_cell_=empty_site(world,sim::CommandType::PlaceClaySource,kiln);
            clay_=purchase(world,sim::CommandType::PlaceClaySource,clay_cell_,commands,blocked);
            command(world,sim::set_building_workforce_priority(*clay_,sim::WorkforcePriority::High),
                "OE51 actual High-priority spare Clay uses the ordinary worker budget",commands,blocked);
            for (const auto& actor:world.couriers()) if (actor.owner==*clay_) courier_=actor.id;
            q::check(courier_.has_value(),"Paid spare Clay has no actual actor");stage_=Stage::AwaitLoad;
            events_.push_back({{"event","spare-clay-purchased"},{"tick",world.ticks()},{"body",static_cast<unsigned>(*clay_)},
                {"courier",static_cast<unsigned>(*courier_)},{"cell",{clay_cell_.x,clay_cell_.y}}});
            return;
        }
        if (stage_==Stage::AwaitLoad) {
            const auto actor=world.courier(*courier_);
            if (actor.phase!=sim::CourierPhase::ToWarehouse) return;
            const auto source=world.building(*clay_);
            q::check(world.building(actor.target).kind==sim::Object::Pottery && source.output==0 && source.progress==0 &&
                source.clay_extracted>0 && actor.cargo>0 && actor.reserved==actor.cargo,
                "Spare Clay did not physically load its earned output for ordinary Pottery");
            command(world,sim::set_building_operation(*clay_,false),"OE51 pause spare only after its actual earned Clay departure",commands,blocked);
            assert_refused(world,*clay_,"outbound-courier");
            events_.push_back({{"event","earned-clay-loaded"},{"tick",world.ticks()},{"extracted",source.clay_extracted},
                {"cargo",actor.cargo},{"reserved",actor.reserved},{"target",static_cast<unsigned>(actor.target)}});
            stage_=Stage::AwaitReturn;return;
        }
        if (stage_==Stage::AwaitReturn) {
            const auto& actor=world.courier(*courier_);
            if (actor.phase!=sim::CourierPhase::IdleAtWorkshop) {
                if (actor.phase==sim::CourierPhase::Returning && !return_checked_) {assert_refused(world,*clay_,"returning-courier");return_checked_=true;}
                return;
            }
            q::check(return_checked_,"Spare Clay skipped an actually observed physical return");
            const auto extracted=world.building(*clay_).clay_extracted;
            retire(world,*clay_,commands,blocked);
            const auto rebuilt=purchase(world,sim::CommandType::PlaceClaySource,clay_cell_,commands,blocked);
            q::check(rebuilt>*clay_ && world.building(rebuilt).clay_extracted==0,"Rebuilt spare inherited Clay production or reused ID");
            command(world,sim::set_building_operation(rebuilt,false),"OE51 pause newly bought empty spare before any production tick",commands,blocked);
            retire(world,rebuilt,commands,blocked);
            for (const auto id:paused_) command(world,sim::set_building_operation(id,true),"OE51 restore original paid Clay operation after complete retirement",commands,blocked);
            std::vector<sim::BuildingId> current;for (const auto& body:world.buildings()) current.push_back(body.id);
            q::check(current==original_bodies_ && world.original_economy_state()->houses.size()==13 && retirements_==4 &&
                world.original_production_state()->safe_demolition->retired.size()==4,
                "OE51 journey left an extra live purchase or changed the original thirteen-House identity set");
            q::authority(world);q::worker_oracle(world);
            events_.push_back({{"event","complete"},{"tick",world.ticks()},{"earned_clay_extracted",extracted},
                {"original_live_purchase_ids_preserved",true},{"all_original_clay_operation_restored",true}});
            observations_.push_back(facts(world,"after-demolition-and-rebuild"));
            finished_=world.ticks();stage_=Stage::Done;
        }
    }
private:
    enum class Stage {NotStarted,DrainExisting,AwaitLoad,AwaitReturn,Done};
    static Json facts(const sim::World& w,const char* label) {
        Json goods=Json::array();for (const auto& row:w.original_goods_balances()) goods.push_back({
            {"good",static_cast<unsigned>(row.good)},{"produced",row.totals.produced},{"consumed",row.totals.consumed},
            {"stock",row.stock},{"cargo",row.cargo},{"reserved",row.incoming_reserved},{"conserved",row.conserved}});
        const auto food=q::food_facts(w);
        return {{"label",label},{"tick",w.ticks()},{"funds",w.treasury()},{"construction",w.construction_spent_total()},
            {"taxes",w.taxes_collected_total()},{"upkeep",w.maintenance_spent_total()},{"population",w.total_population()},
            {"workers_supplied",w.workforce_supply()},{"workers_installed",w.workforce_required()},{"workers_used",w.workforce_used()},
            {"live_bodies",w.buildings().size()},{"live_couriers",w.couriers().size()},{"houses",w.original_economy_state()->houses.size()},
            {"clay_extracted",w.clay_extracted_total()},{"pottery_completed",w.pottery_completed_total()},
            {"six_goods",goods},{"food",{{"harvested",food.produced},{"consumed",food.consumed},
                {"house_received",food.house_received},{"house_consumed",food.house_consumed}}}};
    }
    sim::Cell empty_site(const sim::World& world,sim::CommandType type,std::optional<sim::Cell> kiln) {
        // A diagnostic placement search reuses World validation. The spare
        // source attaches only to an already bought road; no navigator or
        // source permission is reimplemented here.
        const auto center=kiln.value_or(oe_elite_desirability_strategy::elite_cell);
        constexpr int radius=48;
        Json refused=Json::object();std::uint64_t examined=0,no_road=0;
        const auto diagnostic=[&](std::optional<sim::Cell> selected) {
            events_.push_back({{"event","bounded-spare-site-search"},{"tick",world.ticks()},{"command",static_cast<unsigned>(type)},
                {"center",{center.x,center.y}},{"manhattan_radius",radius},{"examined",examined},
                {"world_refusal_reasons",refused},{"permitted_without_existing_road",no_road},
                {"selected",selected?Json::array({selected->x,selected->y}):Json(nullptr)},
                {"authority","World::validate; original permissions and all costs/limits unchanged"}});
        };
        for (int distance=0;distance<=radius;++distance) for (int dy=-distance;dy<=distance;++dy)
            for (int dx=-distance;dx<=distance;++dx) {
                if (std::abs(dx)+std::abs(dy)!=distance) continue;
                const sim::Cell at{center.x+dx,center.y+dy};
                ++examined;const auto validation=world.validate({type,at});
                if (!validation.accepted) {
                    auto& count=refused[validation.reason];if(count.is_null())count=0U;
                    count=count.get<std::uint64_t>()+1;continue;
                }
                if (kiln) {
                    bool road=false;
                    for (int y=0;y<2;++y) for (int x=0;x<2;++x)
                        for (const auto edge:{sim::Cell{-1,0},sim::Cell{1,0},sim::Cell{0,-1},sim::Cell{0,1}}) {
                            const sim::Cell next{at.x+x+edge.x,at.y+y+edge.y};
                            road|=world.object_at(next)==sim::Object::Road || world.original_road(next);
                        }
                    if (!road) {++no_road;continue;}
                }
                diagnostic(at);return at;
            }
        diagnostic(std::nullopt);
        throw std::runtime_error("No normally permitted, affordable empty OE51 spare footprint within bounded city search");
    }
    static void command(sim::World& w,sim::Command value,const char* reason,Json& commands,Json& blocked) {
        q::check(paid::append(w,value,reason,commands,blocked),"OE51 ordinary command refused; see commands.json");
    }
    static sim::BuildingId purchase(sim::World& w,sim::CommandType type,sim::Cell at,Json& commands,Json& blocked) {
        command(w,{type,at},"OE51 extra paid purchase for safe-demolition/rebuild acceptance",commands,blocked);
        return *w.building_owner_at(at);
    }
    void assert_refused(sim::World& world,sim::BuildingId id,const char* boundary) {
        const auto before=world.snapshot();const auto canonical=world.canonical_state();
        const auto status=world.demolition_status(id);const auto result=world.execute(sim::demolish_building(id));
        q::check(!status.allowed && !result.accepted && world.snapshot()==before && world.canonical_state()==canonical,
            "OE51 active-courier refusal changed paid authority");
        events_.push_back({{"event","atomic-refusal"},{"boundary",boundary},{"tick",world.ticks()},{"reason",status.reason}});
    }
    void retire(sim::World& world,sim::BuildingId id,Json& commands,Json& blocked) {
        const auto body=world.building(id);const auto funds=world.treasury();const auto sequence=world.command_sequence();
        const auto building_id=world.next_building_id();const auto courier_id=world.next_courier_id();
        const auto status=world.demolition_status(id);
        q::check(status.allowed,"OE51 real spare cannot safely retire: "+status.reason);
        auto value=sim::demolish_building(id);value.cell=body.cell;
        command(world,value,"OE51 confirmed actual safe demolition; no refund",commands,blocked);
        q::check(++retirements_<=4 && world.treasury()==funds && world.command_sequence()==sequence+1 &&
            world.next_building_id()==building_id && world.next_courier_id()==courier_id,"OE51 retirement refunded or reused IDs");
        for (const auto cell:sim::building_footprint_cells(world.profile(),world.rule_version(),body.kind,body.cell))
            q::check(!world.building_owner_at(cell) && world.object_at(cell)==sim::Object::Empty,"OE51 retirement failed full footprint release");
    }
    Stage stage_=Stage::NotStarted;
    std::optional<std::uint64_t> begin_,finished_;
    std::optional<sim::BuildingId> clay_;std::optional<sim::CourierId> courier_;
    sim::Cell clay_cell_{};std::vector<sim::BuildingId> paused_,original_clay_,original_bodies_;
    std::size_t retirements_=0;bool return_checked_=false;
    Json observations_=Json::array(),events_=Json::array();
};
} // namespace oe_safe_demolition_qa
