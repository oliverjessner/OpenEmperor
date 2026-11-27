#include "app/AgricultureVisualState.h"

#include "simulation/OriginalIndustryGoodsContext.h"
#include <algorithm>
#include <stdexcept>

namespace openemperor::agriculture_visual {
namespace {
namespace sim=simulation;
using Id=sim::BuildingId;

template<class Rows,class Key>
const auto* find_row(const Rows& rows,Id id,Key key) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return key(value)==id;});
    return row==rows.end()?nullptr:&*row;
}
bool food_field(sim::Object kind) {
    switch(kind) {
    case sim::Object::OriginalWheatField:case sim::Object::OriginalMilletField:
    case sim::Object::OriginalRiceField:case sim::Object::OriginalCabbageField:
    case sim::Object::OriginalSoybeanField:return true;
    default:return false;
    }
}
bool plantation_source(sim::Object kind) {
    return kind==sim::Object::OriginalTeaCuringShed237||kind==sim::Object::OriginalLacquerRefinery238||
        kind==sim::Object::OriginalSilkwormShed239;
}
bool supported(sim::Object kind) {
    return food_field(kind)||sim::original_industry_plant(kind)||sim::original_industry_source(kind)||
        kind==sim::Object::OriginalHempField||kind==sim::Object::OriginalHempFarm||
        kind==sim::Object::OriginalFoodFarm||kind==sim::Object::OriginalFishingQuay||
        kind==sim::Object::OriginalHuntersTent||kind==sim::Object::OriginalMill53||
        kind==sim::Object::OriginalFoodShop66||kind==sim::Object::Warehouse||
        kind==sim::Object::OriginalIrrigationPump;
}
void block(State& row,const char* reason) {
    if(!row.blocked)row.reason=reason;
    row.blocked=true;
}
void explain(State& row,const char* reason) {if(!row.blocked)row.reason=reason;}
std::uint32_t progress(int value) {
    if(value<0||value>80)throw std::logic_error("Agriculture display received invalid paid work");
    return static_cast<std::uint32_t>(value);
}
original::ModelBuilding plant_model(sim::Object kind) {
    switch(kind) {
    case sim::Object::OriginalTeaBush26:return original::ModelBuilding::TeaBush;
    case sim::Object::OriginalLacquerTree27:return original::ModelBuilding::LacquerTree;
    case sim::Object::OriginalMulberryTree28:return original::ModelBuilding::MulberryTree;
    default:throw std::logic_error("Agriculture display received an unsupported plant");
    }
}

struct Source {
    Id id{};
    int workers=0,required=0;
    bool road=false;
};
class Preparation {
public:
    Preparation(const sim::World& world,Staffing staffing):world_(world),staffing_(staffing) {}
    Cache run() {
        Cache cache;cache.tick=world_.ticks();cache.sequence=world_.command_sequence();
        if((world_.rule_version()!=sim::kAgricultureReconfigurationRuleVersion&&
            world_.rule_version()!=sim::kIrrigatedHarvestRuleVersion&&
            world_.rule_version()!=sim::kQualityAwareFoodDispatchRuleVersion)||!world_.agriculture_reconfiguration_enabled())return cache;
        for(const auto& body:world_.buildings()) {
            if(!body.placed||!supported(body.kind))continue;
            auto source=body.id;
            if(food_field(body.kind)) {
                const auto* field=find_row(world_.agriculture_fields(),body.id,[](const auto& row){return row.field_id;});
                if(!field)continue;
                source=field->farm_id;
            } else if(sim::original_industry_plant(body.kind)) {
                const auto* plant=find_row(world_.plantations_state()->plants,body.id,[](const auto& row){return row.plant_id;});
                if(!plant)continue;
                source=plant->source_id;
            } else if(body.kind==sim::Object::OriginalHempField) {
                const auto* farm=find_row(world_.original_production_state()->hemp_farms,body.id,
                    [](const auto& row){return row.field_id.value_or(Id{});});
                if(!farm)continue;
                source=farm->farm_id;
            }
            if(!world_.building(source).placed)continue;
            if(cache.count==cache.rows.size())throw std::logic_error("Agriculture display exceeds the existing paid-body bound");
            auto& row=cache.rows[cache.count++];row.id=body.id;row.source=source;row.kind=body.kind;row.cell=body.cell;
            common(row);
            if(food_field(body.kind))field(row);
            else if(sim::original_industry_plant(body.kind))plant(row);
            else if(body.kind==sim::Object::OriginalHempField||body.kind==sim::Object::OriginalHempFarm)hemp(row);
            else if(body.kind==sim::Object::OriginalFoodFarm)farm(row);
            else if(body.kind==sim::Object::OriginalMill53||body.kind==sim::Object::OriginalFoodShop66)processor(row);
            else if(body.kind==sim::Object::Warehouse)warehouse(row);
            else if(body.kind==sim::Object::OriginalIrrigationPump)pump(row);
            else if(body.kind==sim::Object::OriginalFishingQuay)quay(row);
            else if(body.kind==sim::Object::OriginalHuntersTent)tent(row);
            else industry(row);
        }
        return cache;
    }
private:
    const sim::World& world_;
    Staffing staffing_;
    std::array<Source,sim::Rules::original_production_building_limit> sources_{};
    std::size_t source_count_=0;

    const Source& source(Id id) {
        for(std::size_t i=0;i<source_count_;++i)if(sources_[i].id==id)return sources_[i];
        if(source_count_==sources_.size())throw std::logic_error("Agriculture display source bound exceeded");
        auto& value=sources_[source_count_++];value.id=id;value.required=world_.workforce_required(id);
        const auto assigned=std::find_if(staffing_.begin(),staffing_.end(),[&](const auto& entry){return entry.first==id;});
        value.workers=assigned!=staffing_.end()?assigned->second:value.required?world_.workers_assigned(id):0;
        if(value.workers<0||value.workers>value.required)throw std::logic_error("Agriculture display staffing cache is invalid");
        value.road=!world_.building_entrances(id).empty();return value;
    }
    void common(State& row) {
        const auto& body=world_.building(row.id);const auto& owner=world_.building(row.source);const auto& workers=source(row.source);
        row.operating=body.operating_enabled&&owner.operating_enabled;
        row.burning=world_.building_on_fire(row.id)||world_.building_on_fire(row.source);
        row.workers=workers.workers;row.required_workers=workers.required;row.staffed=row.workers>=row.required_workers;
        row.draining=world_.agriculture_draining(row.id);
        if(row.burning)block(row,"Actual source or plot is on fire");
        else if(!row.operating)block(row,"Actual source or plot operation is paused");
        else if(!row.staffed)block(row,"Full actual source staffing required");
        else if(row.kind!=sim::Object::OriginalIrrigationPump&&!workers.road)block(row,"No paid source road access");
        row.reason=row.blocked?row.reason:"AUTHORED display of actual paid World state";
    }
    void water(State& row) {row.wet=world_.irrigation_field_status(row.cell).water_available;}
    void season(State& row,original::ModelBuilding model,bool ready) {
        if(!ready&&!sim::seasonal_growing(model,world_.ticks()))block(row,"Actual seasonal rest; no new growth work");
        if(row.draining&&row.progress==0&&!row.cargo)block(row,"Drain settled; no new crop cycle");
    }
    void food_account(State& row) {
        const auto* stock=find_row(world_.processed_food_state()->stocks,row.id,[](const auto& value){return value.building_id;});
        if(stock)for(std::size_t i=0;i<stock->amounts.size();++i) {
            if((row.kind==sim::Object::OriginalMill53||row.kind==sim::Object::Warehouse)&&i<sim::processed_food_first_meal)row.input+=stock->amounts[i];
            else row.output+=stock->amounts[i];
        }
        for(const auto& trip:world_.processed_food_state()->trips) {
            int amount=0;for(std::size_t i=0;i<trip.loaded.size();++i)amount+=trip.loaded[i]-trip.delivered[i]-trip.returned[i];
            if(trip.target==row.id&&trip.phase==sim::OriginalGoodsTransferPhase::Loaded)row.incoming+=amount;
            if(trip.source==row.id)row.return_reserved+=amount;
        }
    }
    void field(State& row) {
        const auto* paid=find_row(world_.agriculture_fields(),row.id,[](const auto& value){return value.field_id;});
        const auto* seasonal=find_row(world_.seasonal_agriculture_state()->fields,row.id,[](const auto& value){return value.field_id;});
        row.progress=progress(world_.agriculture_field_progress(row.id));row.target=80;water(row);
        const bool ready=seasonal&&seasonal->ready_tick.has_value();
        if(ready){row.harvest_progress=seasonal->harvest_progress;row.harvest_target=sim::seasonal_harvest_work_ticks;}
        row.phase=ready?(seasonal->harvest_progress?Phase::Harvesting:Phase::Ready):
            row.progress?(paid->harvested?Phase::Regrowing:Phase::Growing):paid->harvested?Phase::Resting:Phase::Initial;
        for(const auto& owner:world_.physical_food_growers_state()->farms)if(owner.farm_id==row.source&&owner.latest_trip&&
            owner.latest_trip->field_id==row.id&&!owner.latest_trip->returned_tick) {
            const auto& trip=*owner.latest_trip;const auto& actor=world_.courier(owner.courier_id);
            row.worker=owner.courier_id;row.worker_lifetime=trip.grower_id.value;
            row.cargo=sim::physical_food_trip_cargo(trip);row.returning=trip.phase==sim::PhysicalFoodGrowerPhase::Returning;
            if(row.cargo)row.phase=Phase::Picked;
            if(actor.route_pending)block(row,"Actual Grower route interrupted; cargo and begun edge retained");
            row.working=trip.phase==sim::PhysicalFoodGrowerPhase::Working&&!row.blocked&&!trip.cancel_requested;
        }
        season(row,static_cast<original::ModelBuilding>(195U+static_cast<unsigned>(paid->crop)),ready);
        if(row.blocked)row.working=false;
        explain(row,row.cargo?"Actual harvest on Grower; Farm not yet credited":ready?"Actual ready crop; physical harvest work required":
            row.working?"Actual assigned Grower working at this Field":"Actual per-field work; no time-derived growth");
    }
    void plant(State& row) {
        const auto* paid=find_row(world_.plantations_state()->plants,row.id,[](const auto& value){return value.plant_id;});
        row.progress=paid->growth;row.target=sim::plantation_growth_ticks;water(row);
        const bool ready=paid->growth==sim::plantation_growth_ticks;
        row.phase=ready?Phase::Ready:paid->growth?(paid->harvested?Phase::Regrowing:Phase::Growing):
            paid->harvested?Phase::Resting:Phase::Initial;
        for(const auto& owner:world_.plantations_state()->sources)if(owner.building_id==row.source&&owner.latest_trip&&
            owner.latest_trip->plant_id==row.id&&!owner.latest_trip->returned_tick) {
            const auto& trip=*owner.latest_trip;const auto& actor=world_.courier(owner.courier_id);
            row.worker=owner.courier_id;row.worker_lifetime=static_cast<std::uint64_t>(trip.grower_id);
            row.cargo=world_.plantation_grower_cargo(owner.courier_id);row.returning=actor.phase==sim::CourierPhase::Returning;
            if(row.cargo&&!row.progress)row.phase=Phase::Picked;
            if(actor.route_pending)block(row,"Actual plantation return route interrupted; cargo retained");
        }
        season(row,plant_model(row.kind),ready);
        explain(row,row.cargo?"Actual picked material remains on Grower; regrowth is separate":ready?
            "Actual ready plant; physical Grower pickup required":"Actual saved plant growth; no invented tending animation");
    }
    void hemp(State& row) {
        const auto id=row.source;row.progress=progress(world_.building(id).progress);row.target=80;
        if(row.kind==sim::Object::OriginalHempField)water(row);
        const auto* owner=find_row(world_.seasonal_agriculture_state()->hemp_farms,id,[](const auto& value){return value.farm_id;});
        const bool ready=owner&&owner->ready_tick.has_value();
        row.phase=ready?Phase::Ready:row.progress?(owner&&owner->picked?Phase::Regrowing:Phase::Growing):
            owner&&owner->picked?Phase::Resting:Phase::Initial;
        if(owner&&owner->latest_trip&&!owner->latest_trip->returned_tick) {
            const auto& actor=world_.courier(owner->courier_id);const auto& trip=*owner->latest_trip;
            row.worker=owner->courier_id;row.worker_lifetime=trip.grower_id.value;row.cargo=sim::seasonal_hemp_cargo(trip);
            row.returning=actor.phase==sim::CourierPhase::Returning;
            if(row.cargo&&!row.progress)row.phase=Phase::Picked;
            if(actor.route_pending)block(row,"Actual Hemp return route interrupted; cargo retained");
        }
        if(row.kind==sim::Object::OriginalHempFarm)for(const auto& account:world_.original_goods_state()->stocks)
            if(account.building_id==row.id)row.output=account.amounts[static_cast<std::size_t>(original::Goods::Hemp)];
        season(row,original::ModelBuilding::HempField,ready);
        explain(row,row.cargo?"Actual Hemp remains on worker until Farm return":ready?"Actual ready Hemp; physical pickup required":
            "Actual Hemp growth; no invented field work loop");
    }
    void farm(State& row) {
        food_account(row);bool field_work=false,active_grower=false;
        for(const auto& owner:world_.physical_food_growers_state()->farms)if(owner.farm_id==row.id&&owner.latest_trip&&!owner.latest_trip->returned_tick) {
            active_grower=true;
            const auto& trip=*owner.latest_trip;row.worker=owner.courier_id;row.worker_lifetime=trip.grower_id.value;
            row.cargo=sim::physical_food_trip_cargo(trip);row.returning=trip.phase==sim::PhysicalFoodGrowerPhase::Returning;
            row.phase=row.returning?Phase::Returning:Phase::Waiting;
            if(trip.phase==sim::PhysicalFoodGrowerPhase::Working) {
                row.phase=Phase::Processing;row.progress=progress(world_.agriculture_field_progress(trip.field_id));row.target=80;field_work=true;
            }
            if(world_.courier(owner.courier_id).route_pending)block(row,"Actual Farm Grower route interrupted; cargo retained");
        }
        if(!active_grower) {
            distribution_phase(row);
            for(const auto& owner:world_.physical_food_growers_state()->farms)if(owner.farm_id==row.id) {
                const auto actual=world_.courier_dispatch_status(owner.courier_id);
                if(actual.status!=sim::CourierDispatchStatus::Ready&&actual.status!=sim::CourierDispatchStatus::AlreadyMoving) {
                    const bool had_blocker=row.blocked;block(row,"Actual Farm Grower dispatch blocked");
                    if(!had_blocker)row.reason+="; "+std::string(sim::courier_dispatch_status_name(actual.status));
                }
            }
        }
        row.working=field_work&&!row.blocked;
        explain(row,row.cargo?"Actual Grower returning crop; source stock is separate":row.working?
            "Paid Farm's actual Grower is working a bound Field":"Paid Farm; actual Field work and physical returns only");
    }
    void processor(State& row) {
        food_account(row);
        if(row.kind==sim::Object::OriginalMill53) {
            const auto* mill=find_row(world_.food_processing_state()->mills,row.id,[](const auto& value){return value.mill_id;});
            if(mill&&mill->active) {
                row.phase=Phase::Processing;row.progress=mill->active->work;
                row.target=sim::food_processing_work_per_ingredient*mill->active->grade;row.working=!row.blocked;
                explain(row,"Actual committed Mill recipe; only completed work creates meals");
            } else {
                const auto* stock=find_row(world_.processed_food_state()->stocks,row.id,[](const auto& value){return value.building_id;});
                unsigned kinds=0;if(stock)for(std::size_t i=0;i<sim::processed_food_first_meal;++i)kinds+=stock->amounts[i]>0;
                row.phase=mill&&kinds>=mill->requested_grade?Phase::Ready:Phase::Waiting;
                if(row.phase==Phase::Waiting)block(row,"Waiting for distinct physical Mill ingredients");
                else explain(row,"Actual ingredients available; no recipe committed yet");
            }
        } else {
            distribution_phase(row);if(row.phase==Phase::Idle)row.phase=Phase::Waiting;
            if(!row.output&&!row.incoming&&!row.return_reserved)block(row,"Waiting for physical Dinners arrival");
            explain(row,"Actual Food Shop stock and physical House deliveries");
        }
    }
    void distribution_phase(State& row) {
        bool outbound=false,returning=false;
        for(const auto& actor:world_.couriers())if(actor.owner==row.id&&world_.original_food_cargo(actor.id)>0) {
            returning=returning||actor.phase==sim::CourierPhase::Returning;
            outbound=outbound||actor.phase==sim::CourierPhase::ToWarehouse;
        }
        row.returning=returning;
        row.phase=outbound?Phase::Distributing:returning?Phase::Returning:row.input||row.output?Phase::Ready:
            row.incoming?Phase::Waiting:Phase::Idle;
    }
    void warehouse(State& row) {
        food_account(row);distribution_phase(row);
        if(row.input+row.output+row.incoming+row.return_reserved>=sim::food_warehouse_capacity)
            block(row,"Full shared Food compartment; awaiting physical delivery or return");
        explain(row,"Actual sole Food account; Goods and Auxiliary compartments remain separate");
    }
    void pump(State& row) {
        // Pump eligibility is positive staffing, unlike production's full
        // staffing. Supply/intake/fire/refill all belong to this World query.
        const auto actual=world_.irrigation_pump_status(row.id);
        row.operating=actual.operating;row.burning=actual.burning;
        row.workers=actual.assigned_workers;row.staffed=actual.staffed;
        row.blocked=!actual.supported||!actual.placed||!actual.intake_valid||!actual.operating||actual.burning||!actual.staffed;
        row.wet=actual.supplying;row.working=actual.supplying&&!row.blocked;
        row.phase=row.working?Phase::Distributing:Phase::Waiting;row.reason=actual.reason;
    }
    void quay(State& row) {
        food_account(row);row.phase=Phase::Waiting;
        for(const auto& boat:world_.fishing_state()->boats)if(boat.quay_id==row.id) {
            row.worker_lifetime=boat.id.value;row.cargo=boat.cargo;
            row.returning=boat.phase==sim::FishingBoatPhase::Returning||boat.phase==sim::FishingBoatPhase::Unloading;
            row.progress=boat.work_progress;row.target=boat.phase==sim::FishingBoatPhase::Unloading?sim::fishing_unload_ticks:sim::fishing_catch_ticks;
            row.phase=boat.phase==sim::FishingBoatPhase::Fishing||boat.phase==sim::FishingBoatPhase::Unloading?Phase::Processing:Phase::Distributing;
            row.working=row.phase==Phase::Processing&&!row.blocked;
            if(boat.movement==sim::FishingMovementResult::Retry||boat.movement==sim::FishingMovementResult::Failed) {
                block(row,"Actual Boat waiting for a valid physical return route");row.working=false;
            }
        }
        explain(row,"Actual Boat work/cargo; only real dock unload credits Fish");
    }
    void tent(State& row) {
        food_account(row);row.phase=Phase::Waiting;
        for(const auto& hunter:world_.hunting_state()->hunters)if(hunter.tent_id==row.id) {
            row.worker_lifetime=hunter.id.value;row.cargo=hunter.cargo;
            row.returning=hunter.phase==sim::HuntingHunterPhase::Returning||hunter.phase==sim::HuntingHunterPhase::Unloading;
            row.progress=hunter.work_progress;row.target=hunter.phase==sim::HuntingHunterPhase::Unloading?sim::hunting_unload_ticks:sim::hunting_work_ticks;
            row.phase=hunter.phase==sim::HuntingHunterPhase::Hunting||hunter.phase==sim::HuntingHunterPhase::Unloading?Phase::Processing:Phase::Distributing;
            row.working=row.phase==Phase::Processing&&!row.blocked;
            if(hunter.movement==sim::HuntingMovementResult::Retry||hunter.movement==sim::HuntingMovementResult::Failed) {
                block(row,"Actual Hunter waiting for a valid physical return route");row.working=false;
            }
        }
        explain(row,"Actual Hunter work/cargo; only real Tent unload credits Meat");
    }
    void industry(State& row) {
        const auto& industry=world_.original_production_state()->industry;
        const auto* actual=find_row(industry.sources,row.id,[](const auto& value){return value.building_id;});
        if(actual){row.progress=progress(actual->progress);row.target=80;}
        const auto good=sim::original_industry_goods::output(row.kind);
        for(const auto& stock:world_.original_goods_state()->stocks)if(stock.building_id==row.id)
            for(std::size_t i=0;i<stock.amounts.size();++i) {
                if(good&&i==static_cast<std::size_t>(*good))row.output+=stock.amounts[i];else row.input+=stock.amounts[i];
            }
        const auto material=row.kind==sim::Object::OriginalLoggingShed38?std::optional{sim::OriginalAuxMaterial::Wood}:
            row.kind==sim::Object::OriginalBronzeSmelter39?std::optional{sim::OriginalAuxMaterial::Bronze}:
            row.kind==sim::Object::OriginalPaperMaker45?std::optional{sim::OriginalAuxMaterial::Paper}:
            row.kind==sim::Object::OriginalLacquerRefinery238?std::optional{sim::OriginalAuxMaterial::Lacquer}:
            row.kind==sim::Object::OriginalSilkwormShed239?std::optional{sim::OriginalAuxMaterial::RawSilk}:std::nullopt;
        for(const auto& stock:industry.auxiliary)if(stock.building_id==row.id)
            for(std::size_t i=0;i<stock.amounts.size();++i) {
                if(material&&i==static_cast<std::size_t>(*material))row.output+=stock.amounts[i];else row.input+=stock.amounts[i];
            }
        row.input+=world_.building(row.id).input_clay;
        row.phase=row.progress?Phase::Processing:row.output?Phase::Ready:Phase::Waiting;
        row.working=row.progress>0&&!row.blocked;
        if(plantation_source(row.kind)) {
            row.working=false;bool bound=false;
            for(const auto& paid:world_.plantations_state()->plants)if(paid.source_id==row.id&&world_.building(paid.plant_id).placed)bound=true;
            if(!bound)block(row,"Waiting for an actual paid bound plant");
            for(const auto& owner:world_.plantations_state()->sources)if(owner.building_id==row.id&&owner.latest_trip&&!owner.latest_trip->returned_tick) {
                row.worker=owner.courier_id;row.worker_lifetime=static_cast<std::uint64_t>(owner.latest_trip->grower_id);
                row.cargo=world_.plantation_grower_cargo(owner.courier_id);row.returning=world_.courier(owner.courier_id).phase==sim::CourierPhase::Returning;
                row.phase=row.returning?Phase::Returning:Phase::Waiting;
                if(world_.courier(owner.courier_id).route_pending)block(row,"Actual plantation Grower route interrupted; cargo retained");
            }
        }
        explain(row,row.working?"Actual saved recipe work; source inputs and output are separate":
            "Actual physical material/source accounts; no invented working loop");
    }
};
} // namespace

const State* Cache::find(simulation::BuildingId id) const noexcept {
    if(count>rows.size())return nullptr;
    const auto end=rows.begin()+static_cast<std::ptrdiff_t>(count);
    const auto row=std::lower_bound(rows.begin(),end,id,[](const State& value,auto key){return value.id<key;});
    return row==end||row->id!=id?nullptr:&*row;
}
Cache prepare(const simulation::World& world,Staffing staffing) {return Preparation(world,staffing).run();}
const char* phase_name(Phase phase) noexcept {
    switch(phase) {
    case Phase::Initial:return "Initial";case Phase::Growing:return "Growing";case Phase::Ready:return "Ready";
    case Phase::Harvesting:return "Harvesting";case Phase::Picked:return "Picked";case Phase::Regrowing:return "Regrowing";
    case Phase::Resting:return "Resting";case Phase::Idle:return "Idle";case Phase::Processing:return "Processing";
    case Phase::Distributing:return "Distributing";case Phase::Waiting:return "Waiting";
    case Phase::Returning:return "Returning";
    }
    return "Unsupported";
}
} // namespace openemperor::agriculture_visual
