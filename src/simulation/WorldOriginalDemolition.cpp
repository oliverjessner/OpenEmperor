#include "simulation/World.h"
#include "simulation/OriginalSafeDemolitionState.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
template<class Rows> const auto* owned_row(const Rows& rows,BuildingId id) {
    const auto found=std::lower_bound(rows.begin(),rows.end(),id,
        [](const auto& row,BuildingId key){return row.building_id<key;});
    return found==rows.end() || found->building_id!=id ? nullptr:&*found;
}
bool proof_references(const OriginalHousingEvaluation& proof,BuildingId id,
                      std::optional<CourierId> actor) {
    for (const auto& witness:proof.service_witnesses)
        if (witness && (witness->building_id==id || witness->provider_id==id ||
            (actor && witness->transfer.courier_id==*actor))) return true;
    if (const auto& witness=proof.water_evaluation_witness;witness &&
        (witness->building_id==id || witness->provider_id==id ||
         (actor && witness->transfer.courier_id==*actor))) return true;
    return actor && proof.food_quality_witness && proof.food_quality_witness->transfer.courier_id==*actor;
}
}

DemolitionStatus World::original_safe_demolition_status(BuildingId id) const {
    DemolitionStatus status;
    const auto blocked=[&](DemolitionBlocker blocker,std::string reason) {
        status.blocker=blocker;status.reason=std::move(reason);return status;
    };
    if (!original_safe_demolition_enabled() || !original_production_ || !original_economy_ ||
        !original_goods_ || !original_production_->safe_demolition)
        return blocked(DemolitionBlocker::UnsupportedProfile,"Safe demolition requires explicit Rule19 authority.");
    const auto found=std::lower_bound(buildings_.begin(),buildings_.end(),id,
        [](const auto& body,BuildingId key){return body.id<key;});
    if (found==buildings_.end() || found->id!=id || !found->placed)
        return blocked(DemolitionBlocker::MissingBuilding,"Building does not exist.");
    const auto& body=*found;
    if (body.kind==Object::OriginalInfluenceSource) {
        const auto checked=validate_original_influence_command(demolish_building(id));
        status.allowed=checked.accepted;status.reason=checked.reason;
        status.blocker=checked.accepted ? DemolitionBlocker::None:DemolitionBlocker::UnsupportedKind;
        return status;
    }
    const auto& production=*original_production_;
    for (const auto& farm:production.hemp_farms) if (farm.farm_id==id || farm.field_id==id)
        return blocked(DemolitionBlocker::DependentField,"Hemp Farm/Field ownership must remain intact; demolition is unavailable in V1.");
    for (const auto& farm:production.food.farms) if (farm.farm_id==id || farm.field_id==id)
        return blocked(DemolitionBlocker::DependentField,"Food Farm/Field ownership must remain intact; demolition is unavailable in V1.");
    for (const auto& source:production.industry.sources) if (source.plant_id==id || (source.building_id==id && source.plant_id))
        return blocked(DemolitionBlocker::DependentField,"Production plant ownership must remain intact; demolition is unavailable in V1.");
    if (body.kind!=Object::Household && body.kind!=Object::ClaySource)
        return blocked(DemolitionBlocker::UnsupportedKind,"Rule19 V1 permits only never-occupied Common Houses and settled empty Clay Sources.");
    if (building_on_fire(id)) return blocked(DemolitionBlocker::OnFire,"Cannot demolish: building is on fire.");
    if (production.safe_demolition->retired.size()>=original_safe_demolition_retired_limit)
        return blocked(DemolitionBlocker::HistoryCapacity,"Cannot demolish: the bounded retirement history is full.");
    if (command_sequence_>=UINT64_MAX-1 || road_revision_==UINT64_MAX)
        return blocked(DemolitionBlocker::ExhaustedCounter,"Cannot demolish: command or topology counter exhausted.");
    for (const auto cell:building_footprint_cells(profile_,rule_version_,body.kind,body.cell))
        if (!in_bounds(cell) || !buildable(cell) || object_at(cell)!=body.kind || building_owner_at(cell)!=id)
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: paid footprint ownership is inconsistent.");

    std::optional<CourierId> owned_actor;
    const CourierState* clay_actor=nullptr;
    bool owned_payload=false,physical_overlap=false;
    for (const auto& actor:couriers_) {
        if (actor.owner==id) {
            if (owned_actor || body.kind!=Object::ClaySource || actor.role!=CourierRole::Clay)
                return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: unexpected owned courier authority.");
            owned_actor=actor.id;clay_actor=&actor;
            if (actor.phase!=CourierPhase::IdleAtWorkshop) ++status.active_couriers;
            owned_payload=owned_payload || actor.cargo || actor.reserved;
        } else if (actor.target==id && actor.phase!=CourierPhase::IdleAtWorkshop) ++status.incoming_deliveries;
        if (actor.phase!=CourierPhase::IdleAtWorkshop) {
            if (actor.path_vertex>=actor.path.size() || (actor.edge_progress>0 && actor.path_vertex+1>=actor.path.size()))
                return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: active courier path is inconsistent.");
            physical_overlap|=building_footprint_contains(profile_,rule_version_,body.kind,body.cell,actor.path[actor.path_vertex]);
            if (actor.edge_progress>0)
                physical_overlap|=building_footprint_contains(profile_,rule_version_,body.kind,body.cell,actor.path[actor.path_vertex+1]);
        }
    }
    const auto stock=[&](int count,const char* name) {
        if (!count) return;
        if (!status.stored_goods_summary.empty()) status.stored_goods_summary+=", ";
        status.stored_goods_summary+=std::to_string(count)+" "+name;
    };
    stock(body.input_clay,"input Clay");stock(body.output,"Clay");
    stock(body.active_recipe_clay,"recipe Clay");stock(body.pottery_stock,"legacy Pottery");stock(body.food_stock,"legacy Food");
    if (status.active_couriers)
        return blocked(DemolitionBlocker::OwnedActiveCourier,"Cannot demolish: owned courier is delivering, returning or waiting for a road.");
    if (status.incoming_deliveries || physical_overlap)
        return blocked(DemolitionBlocker::IncomingDelivery,"Cannot demolish: a courier retains this active target or occupies its footprint.");
    if (body.reserved_incoming || body.reserved_food_incoming)
        return blocked(DemolitionBlocker::IncomingReservation,"Cannot demolish: incoming delivery reserved.");
    if (owned_payload) return blocked(DemolitionBlocker::OwnedPayload,"Cannot demolish: owned courier still carries cargo or a reservation.");
    if (body.active_recipe_clay) return blocked(DemolitionBlocker::ActiveRecipe,"Cannot demolish: an active recipe still owns material.");
    if (!status.stored_goods_summary.empty())
        return blocked(DemolitionBlocker::StoredGoods,"Cannot demolish: still contains "+status.stored_goods_summary+".");

    if (body.kind==Object::Household) {
        const auto* house=owned_row(original_economy_->houses,id);
        const auto* consumption=owned_row(production.house_consumption,id);
        const auto* food=owned_row(production.food.stocks,id);
        const auto* housing=owned_row(production.housing.houses,id);
        const auto* finance=owned_row(production.finance.houses,id);
        const auto* water=owned_row(production.water.houses,id);
        if (!house || !consumption || !food || !housing || !finance || !water)
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: House owner records are incomplete.");
        if (house->house.occupants>0 || housing->first_admission_tick)
            return blocked(DemolitionBlocker::OccupiedHouse,"Cannot demolish: occupied or previously admitted Houses are unsupported in V1.");
        for (std::size_t good=0;good<original::goods_count;++good)
            stock(original::goods_stock(house->house,static_cast<original::Goods>(good)),"House goods");
        for (const auto amount:food->amounts) stock(amount,"Food");
        if (!status.stored_goods_summary.empty())
            return blocked(DemolitionBlocker::StoredGoods,"Cannot demolish: still contains "+status.stored_goods_summary+".");
        OriginalHousingHouse empty_housing;empty_housing.building_id=id;
        OriginalFinanceHouse empty_finance;empty_finance.building_id=id;
        OriginalWaterHouseState empty_water;empty_water.building_id=id;
        if (*house!=OriginalHouseState{id} || *consumption!=OriginalHouseConsumptionState{id} ||
            *food!=OriginalFoodStock{id} || *housing!=empty_housing ||
            *finance!=empty_finance || *water!=empty_water || body.taxes_paid_total)
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: House retains needs, consumption, tax or arrival history.");
    } else {
        if (!clay_actor || !owned_actor || !clay_actor->enabled || !clay_actor->path.empty() || clay_actor->path_vertex ||
            clay_actor->edge_progress || clay_actor->route_pending || clay_actor->route_checked_revision || clay_actor->last_dispatched_pottery)
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: Clay courier has not completed its physical return.");
        if (body.operating_enabled)
            return blocked(DemolitionBlocker::OperationEnabled,"Pause Clay Source before demolition; existing deliveries must finish returning.");
        if (body.progress)
            return blocked(DemolitionBlocker::ActiveProduction,"Cannot demolish: Clay extraction is partly completed.");
    }

    // Retained terminal rows and cursors are proof, not disposable caches.
    // The sole exception is this source's exact completed Clay trip, archived
    // below together with its never-reused actor identity.
    const auto actor_reference=[&](CourierId actor){return owned_actor && actor==*owned_actor;};
    for (const auto& actor:couriers_) if (actor.owner!=id &&
        (actor.target==id || actor.last_dispatched_target==id || actor.last_dispatched_pottery==id))
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: another courier retains this historical target.");
    if (last_dispatched_household_==id || last_dispatched_food_household_==id || last_dispatched_service_household_==id)
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a household cursor still references this purchase.");
    for (const auto& row:original_goods_->stocks) if (row.building_id==id)
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a typed goods owner record remains.");
    for (const auto& row:original_goods_->cargo)
        if (row.source==id || row.target==id || actor_reference(row.transfer.courier_id))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: typed goods cargo still references this purchase.");
    for (const auto& row:original_goods_->reservations)
        if (row.source==id || row.target==id || actor_reference(row.transfer.courier_id))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: typed goods transfer history references this purchase.");
    for (const auto& row:production.food.trips)
        if (row.source==id || row.target==id || actor_reference(row.transfer.courier_id))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: Food transfer history references this purchase.");
    for (const auto& row:production.food.stocks)
        if (row.quality_witness && actor_reference(row.quality_witness->transfer.courier_id))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a Food quality witness retains this actor.");
    for (const auto& row:production.services.trips) if (row.target==id || actor_reference(row.transfer.courier_id))
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a service trip retains this purchase.");
    for (const auto& row:production.services.witnesses)
        if (row.building_id==id || row.provider_id==id || actor_reference(row.transfer.courier_id))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a service arrival retains this purchase.");
    for (const auto& row:production.water.trips) if (row.target==id || actor_reference(row.transfer.courier_id))
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a Water trip retains this purchase.");
    for (const auto& row:production.water.houses)
        for (const auto* witness:{&row.current_witness,&row.evaluation_witness})
            if (*witness && ((*witness)->building_id==id || (*witness)->provider_id==id || actor_reference((*witness)->transfer.courier_id)))
                return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a Water arrival retains this purchase.");
    for (const auto& row:production.housing.houses) {
        if (row.last_evaluation && proof_references(*row.last_evaluation,id,owned_actor))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a Housing evaluation retains this purchase.");
        for (const auto& entry:row.first_transitions)
            if (entry && proof_references(entry->evaluation,id,owned_actor))
                return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a Housing transition retains this purchase.");
    }
    for (const auto& row:production.industry.sources)
        if (row.building_id==id || row.plant_id==id || (row.last_clay_arrival &&
            (row.last_clay_arrival->source==id || actor_reference(row.last_clay_arrival->transfer.courier_id))))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: production ownership or a Bronze Clay receipt retains this purchase.");
    for (const auto& row:production.industry.auxiliary) if (row.building_id==id)
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: an auxiliary stock owner remains.");
    for (const auto& row:production.industry.managed_ore) if (row.building_id==id)
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a managed ore owner remains.");
    for (const auto& row:production.industry.aux_trips)
        if (row.source==id || row.target==id || actor_reference(row.transfer.courier_id))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: an auxiliary trip retains this purchase.");
    const OriginalClayTrip* settled=nullptr;
    for (const auto& trip:production.industry.clay_trips) {
        if (actor_reference(trip.transfer.courier_id)) {
            const auto target=std::lower_bound(buildings_.begin(),buildings_.end(),trip.target,
                [](const auto& value,BuildingId key){return value.id<key;});
            if (settled || !clay_actor || trip.source!=id || !trip.arrival_tick || *trip.arrival_tick>ticks_ ||
                !trip.transfer.sequence || trip.loaded<=0 || trip.delivered!=trip.loaded ||
                target==buildings_.end() || target->id!=trip.target || target->kind!=Object::Pottery ||
                clay_actor->target!=trip.target || clay_actor->last_dispatched_target!=trip.target)
                return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: Clay trip has not settled at its actual destination.");
            settled=&trip;
        } else if (trip.source==id || trip.target==id)
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: another Clay trip retains this purchase.");
    }
    if (clay_actor && !settled && (body.clay_extracted || clay_actor->target!=id || clay_actor->last_dispatched_target || clay_actor->last_dispatched_pottery))
        return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: Clay production or courier history lacks its settled trip.");
    if (production.fire_protection) {
        const auto references=[&](const OriginalFireTrip& trip) {
            return trip.owner==id || trip.target==id || actor_reference(trip.courier_id);
        };
        for (const auto& trip:production.fire_protection->trips) if (references(trip))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: an Inspector trip retains this purchase.");
        for (const auto& arrival:production.fire_protection->arrivals) if (references(arrival.trip))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: an Inspector arrival retains this purchase.");
        for (const auto& tax:production.fire_protection->taxes) if (tax.building_id==id)
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: a tax witness retains this House.");
    }
    for (const auto& retired:production.safe_demolition->retired)
        if (retired.building_id==id || (retired.clay_trip && (retired.clay_trip->source==id || retired.clay_trip->target==id ||
            actor_reference(retired.clay_trip->transfer.courier_id))))
            return blocked(DemolitionBlocker::HistoricalReference,"Cannot demolish: retirement provenance still references this purchase.");
    status.allowed=true;status.reason="Ready to demolish. No refund; paid history and retired IDs remain preserved.";
    return status;
}

CommandResult World::execute_original_safe_demolition(Command command,bool refresh_after) {
    const auto status=original_safe_demolition_status(command.building_id);
    if (!status.allowed) return {false,false,status.reason,command_sequence_,ticks_};
    const auto body=building(command.building_id);
    if (body.kind==Object::OriginalInfluenceSource) return execute_original_influence_command(command,refresh_after);
    const auto id=body.id;
    OriginalRetiredPurchase retired;
    retired.building_id=id;
    retired.kind=body.kind==Object::Household ? OriginalRetiredKind::EmptyHouse:OriginalRetiredKind::ClaySource;
    retired.origin_x=body.cell.x;retired.origin_y=body.cell.y;
    retired.placed_tick=body.placed_tick;retired.removed_tick=ticks_;retired.clay_extracted=body.clay_extracted;
    if (body.kind==Object::ClaySource) {
        const auto actor=std::find_if(couriers_.begin(),couriers_.end(),[&](const auto& value){return value.owner==id;});
        retired.courier_id=actor->id;
        const auto trip=std::find_if(original_production_->industry.clay_trips.begin(),original_production_->industry.clay_trips.end(),
            [&](const auto& value){return value.transfer.courier_id==actor->id;});
        if (trip!=original_production_->industry.clay_trips.end()) retired.clay_trip=*trip;
    }
    auto& journal=original_production_->safe_demolition->retired;
    journal.insert(std::lower_bound(journal.begin(),journal.end(),id,
        [](const auto& row,BuildingId key){return row.building_id<key;}),retired);
    if (body.kind==Object::Household) {
        const auto erase=[&](auto& rows){std::erase_if(rows,[&](const auto& row){return row.building_id==id;});};
        erase(original_economy_->houses);erase(original_production_->house_consumption);
        erase(original_production_->food.stocks);erase(original_production_->housing.houses);
        erase(original_production_->finance.houses);erase(original_production_->water.houses);
    } else {
        std::erase_if(original_production_->industry.clay_trips,
            [&](const auto& trip){return trip.transfer.courier_id==*retired.courier_id;});
        std::erase_if(couriers_,[&](const auto& actor){return actor.owner==id;});
    }
    for (const auto cell:building_footprint_cells(profile_,rule_version_,body.kind,body.cell)) {
        objects_[index(cell)]=Object::Empty;owners_[index(cell)]=0;
    }
    std::erase_if(buildings_,[&](const auto& value){return value.id==id;});
    tick_staffed_.clear();tick_staffing_active_=false;
    ++command_sequence_;++road_revision_;cached_route_.reset();
    if (refresh_after) refresh_routes();
    performance::increment(performance::Counter::WorldExecutes);
    return {true,true,"Building demolished. No refund; retired purchase identity preserved.",command_sequence_,ticks_};
}

} // namespace openemperor::simulation
