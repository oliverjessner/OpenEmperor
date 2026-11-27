#include "simulation/World.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* why){throw std::invalid_argument(why);}
bool leaf(Object kind){return kind==Object::OriginalWheatField||kind==Object::OriginalMilletField||kind==Object::OriginalRiceField||kind==Object::OriginalCabbageField||kind==Object::OriginalSoybeanField||original_industry_plant(kind)||kind==Object::OriginalIrrigationPump;}
template<class Rows> const auto& body(const Rows& rows,BuildingId id){
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& b,BuildingId key){return b.id<key;});
    if(at==rows.end()||at->id!=id)invalid("Agriculture retirement lost its paid identity");return *at;
}
template<class Rows,class Key> const auto& exact(const Rows& rows,BuildingId id,Key key){
    const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return key(row)==id;});
    if(at==rows.end())invalid("Agriculture drain lost existing typed work authority");return *at;
}
std::optional<AgricultureDrainWork> current_work(BuildingId id,const ProcessedFoodState& food,const MultiFieldAgricultureState& multi,
    const SeasonalAgricultureState& season,const PlantationsState& plants,const IrrigationProductivityState& irrigation){
    AgricultureDrainWork result;
    const auto f=std::find_if(food.fields.begin(),food.fields.end(),[&](const auto& row){return row.field_id==id;});
    if(f!=food.fields.end()){
        result.work.base_work=exact(multi.fields,id,[](const auto& r){return r.field_id;}).productive_ticks;
        result.work.wasted_work=exact(season.fields,id,[](const auto& r){return r.field_id;}).wasted_work;result.harvested=f->harvested;
    }else{
        const auto p=std::find_if(plants.plants.begin(),plants.plants.end(),[&](const auto& row){return row.plant_id==id;});
        if(p==plants.plants.end())return {};
        result.work.base_work=p->productive_ticks;result.harvested=p->harvested;
        result.work.wasted_work=exact(season.plants,id,[](const auto& r){return r.plant_id;}).wasted_work;
    }
    const auto& wet=exact(irrigation.work,id,[](const auto& r){return r.building_id;});
    result.work.wet_work_ticks=wet.wet_work_ticks;result.work.bonus_work=wet.bonus_work;result.work.clipped_bonus=wet.clipped_bonus;return result;
}
std::uint64_t progress(const AgricultureDrainWork& w){
    if(w.work.bonus_work>UINT64_MAX-w.work.base_work)invalid("Drain growth observation overflow");
    const auto earned=w.work.base_work+w.work.bonus_work;
    if(w.work.wasted_work>earned||w.harvested>(earned-w.work.wasted_work)/80)invalid("Drain observation borrowed unearned work");
    const auto pending=earned-w.work.wasted_work-80*w.harvested;
    if(pending>80)invalid("Drain observation exceeds one actual crop cycle");return pending;
}
template<class Buildings>
void validate_drain_work(const AgricultureReconfigurationState& state,const Buildings& buildings,const ProcessedFoodState& food,
    const MultiFieldAgricultureState& multi,const SeasonalAgricultureState& season,const PlantationsState& plants,const IrrigationProductivityState& irrigation,
    const PhysicalFoodGrowersState& physical,std::uint64_t tick){
    for(const auto& entry:state.entries){
        const auto& paid=body(buildings,entry.building_id);const auto current=current_work(entry.building_id,food,multi,season,plants,irrigation);
        if(current.has_value()!=entry.initial_work.has_value() || (paid.kind==Object::OriginalIrrigationPump)==current.has_value())invalid("Drain requires its exact initial crop observation, or null for a Pump");
        if(!current)continue;
        const auto& initial=*entry.initial_work;const auto& a=initial.work;const auto& b=current->work;const auto pending=progress(initial);const auto now=progress(*current);
        original::ModelBuilding model;
        const auto field=std::find_if(food.fields.begin(),food.fields.end(),[&](const auto& f){return f.field_id==paid.id;});
        if(field!=food.fields.end())model=static_cast<original::ModelBuilding>(195+static_cast<unsigned>(field->crop));
        else model=paid.kind==Object::OriginalTeaBush26?original::ModelBuilding::TeaBush:paid.kind==Object::OriginalLacquerTree27?original::ModelBuilding::LacquerTree:original::ModelBuilding::MulberryTree;
        if(a.base_work>seasonal_work_opportunities(model,paid.placed_tick,entry.started_tick)||a.wet_work_ticks>a.base_work||a.bonus_work>a.wet_work_ticks/4||a.clipped_bonus!=a.wet_work_ticks/4-a.bonus_work||a.clipped_bonus>(a.base_work+a.bonus_work)/80||
            a.base_work>b.base_work||a.wet_work_ticks>b.wet_work_ticks||a.bonus_work>b.bonus_work||a.clipped_bonus>b.clipped_bonus||a.wasted_work>b.wasted_work||initial.harvested>current->harvested)
            invalid("Initial drain work was not earned before its real request");
        const auto earned=a.base_work+a.bonus_work;const auto current_earned=b.base_work+b.bonus_work;
        const auto through=agriculture_retirement_tick(&state,paid.id,tick);
        if(b.base_work-a.base_work>seasonal_work_opportunities(model,entry.started_tick,through)||
            (through==entry.started_tick&&(current->harvested!=initial.harvested||a.wasted_work!=b.wasted_work)))
            invalid("Drain continuation borrowed work or a crop event from a future tick");
        if(field!=food.fields.end()){
            const auto& receipt=exact(physical.fields,paid.id,[](const auto& r){return r.field_id;});
            if(receipt.last_receipt&&receipt.last_receipt->harvested_tick<=entry.started_tick&&initial.harvested<receipt.returned)
                invalid("Drain omitted an actual earlier Field harvest receipt");
            const auto& worker=exact(physical.farms,field->farm_id,[](const auto& r){return r.farm_id;});
            if(worker.latest_trip&&worker.latest_trip->field_id==paid.id&&worker.latest_trip->harvested_tick&&
                *worker.latest_trip->harvested_tick<=entry.started_tick&&initial.harvested<current->harvested)
                invalid("Drain omitted its actual earlier loaded Field harvest");
        }else{
            const auto& plant=exact(plants.plants,paid.id,[](const auto& r){return r.plant_id;});
            if(plant.last_receipt&&plant.last_receipt->picked_tick<=entry.started_tick&&initial.harvested<plant.returned)
                invalid("Drain omitted an actual earlier plant pickup receipt");
            const auto& worker=exact(plants.sources,plant.source_id,[](const auto& r){return r.building_id;});
            if(worker.latest_trip&&worker.latest_trip->plant_id==paid.id&&worker.latest_trip->picked_tick&&
                *worker.latest_trip->picked_tick<=entry.started_tick&&initial.harvested<current->harvested)
                invalid("Drain omitted its actual earlier loaded plant pickup");
        }
        if(current_earned-earned>(pending?80-pending:0)||current->harvested-initial.harvested>static_cast<std::uint64_t>(pending!=0)||
            (b.wasted_work>a.wasted_work&&(current->harvested!=initial.harvested||now)))invalid("Draining crop started a new cycle or harvested after ordinary expiry");
    }
    // Retained start observations share the Farm's single labor timeline. An
    // earlier completed closure contributes its full frozen base work.
    for(const auto& epoch:state.entries){
        const auto f=std::find_if(food.fields.begin(),food.fields.end(),[&](const auto& row){return row.field_id==epoch.building_id;});
        if(f==food.fields.end())continue;
        const auto placed=body(buildings,f->farm_id).placed_tick;std::uint64_t base=0;
        for(const auto& peer:state.entries){
            const auto field=std::find_if(food.fields.begin(),food.fields.end(),[&](const auto& row){return row.field_id==peer.building_id;});
            if(field==food.fields.end()||field->farm_id!=f->farm_id||peer.started_tick>epoch.started_tick)continue;
            const auto retained=peer.removed_tick&&*peer.removed_tick<=epoch.started_tick?
                current_work(peer.building_id,food,multi,season,plants,irrigation)->work.base_work:peer.initial_work->work.base_work;
            if(retained>UINT64_MAX-base)invalid("Drain Farm labor observation overflow");base+=retained;
        }
        if(epoch.started_tick<placed||base>epoch.started_tick-placed)invalid("Drain observations borrowed parallel or future Farm labor");
    }
}
template<class Buildings,class Couriers>
void validate_entries(const AgricultureReconfigurationState* state,const Buildings& buildings,const Couriers& couriers,
    RulesProfile profile,std::uint32_t rule,std::uint64_t tick,std::uint64_t sequence){
    if(!agriculture_reconfiguration_enabled(profile,rule)){
        if(state)invalid("Historical generation rejects Agriculture reconfiguration authority");return;
    }
    if(!state||state->entries.size()>agriculture_reconfiguration_limit)invalid("Rule36 requires its bounded reconfiguration journal");
    struct Event {std::uint64_t sequence,tick;};
    std::array<Event,2*agriculture_reconfiguration_limit> sequences{};std::size_t n=0;
    for(std::size_t i=0;i<state->entries.size();++i){
        const auto& e=state->entries[i];const auto& b=body(buildings,e.building_id);
        if((i&&!(state->entries[i-1].building_id<e.building_id))||!leaf(b.kind)||e.started_tick<b.placed_tick||e.started_tick>tick||e.started_sequence<=static_cast<std::uint32_t>(b.id)||e.started_sequence>sequence||
            e.removed_tick.has_value()!=e.removed_sequence.has_value()||b.placed==e.removed_tick.has_value())invalid("Agriculture journal identity, purchase, phase or chronology invalid");
        sequences[n++]={e.started_sequence,e.started_tick};
        if(e.removed_tick){
            if(*e.removed_tick<e.started_tick||*e.removed_tick>tick||*e.removed_sequence<e.started_sequence||*e.removed_sequence>sequence||
                (*e.removed_sequence==e.started_sequence&&*e.removed_tick!=e.started_tick))invalid("Agriculture removal precedes its paid drain or command");
            if(*e.removed_sequence!=e.started_sequence)sequences[n++]={*e.removed_sequence,*e.removed_tick};
            if(b.input_clay||b.output||b.pottery_stock||b.reserved_incoming||b.progress||b.active_recipe_clay||b.recipes_completed||b.food_stock||b.reserved_food_incoming||b.population||b.fire_until_tick>*e.removed_tick)
                invalid("Retired Agriculture leaf owns live payload, work or fire");
            for(const auto& c:couriers)if(c.owner==b.id||(c.target==b.id&&c.phase!=CourierPhase::IdleAtWorkshop))invalid("Retired Agriculture leaf retains an actual worker commitment");
        }
    }
    std::sort(sequences.begin(),sequences.begin()+static_cast<std::ptrdiff_t>(n),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
    for(std::size_t i=1;i<n;++i)if(sequences[i-1].sequence==sequences[i].sequence || sequences[i-1].tick>sequences[i].tick)invalid("Agriculture journal reused a command sequence or reversed the real command clock");
    for(const auto& b:buildings)if(!b.placed){const auto* e=agriculture_reconfiguration_entry(state,b.id);if(!e||!e->removed_tick||!leaf(b.kind))invalid("Unplaced paid body lacks exact closed Agriculture provenance");}
}
// Publication-only lifetime geometry. Current occupancy cannot establish the
// historical permission to overlap a closed purchase.
void validate_retired_geometry(const WorldSnapshot& s){
    if(s.buildings.size()>Rules::original_production_building_limit||s.bridges.size()>bridge_max_spans||s.ramps.size()>ramp_max_links||
        (s.fishing&&s.fishing->quays.size()>fishing_quay_limit)||s.irrigation->ditches.size()>irrigation_ditch_limit||
        s.original_production->housing.houses.size()>Rules::city_v10_household_limit||s.original_production->housing.neutral_source_origins.size()>original_influence_purchase_limit||
        (s.original_influence&&s.original_influence->removed_sources.size()>original_influence_purchase_limit))
        invalid("Retired Agriculture geometry exceeds existing collection bounds");
    for(const auto& b:s.buildings){
        const auto shape=building_footprint(s.profile,s.rule_version,b.kind);
        if(b.cell.x<0||b.cell.y<0||b.cell.x>s.width-shape.width||b.cell.y>s.height-shape.height)invalid("Retired Agriculture geometry leaves the bound source grid");
        if(b.kind==Object::OriginalFishingQuay){
            if(!s.fishing)invalid("Retirement geometry requires complete Quay claims");
            const auto q=std::find_if(s.fishing->quays.begin(),s.fishing->quays.end(),[&](const auto& row){return row.quay_id==b.id;});
            if(q==s.fishing->quays.end()||q->orientation>3)invalid("Retirement geometry lost a complete oriented Quay");
        }
    }
    for(const auto& e:s.agriculture_reconfiguration->entries)if(e.removed_tick){
        const auto& closed=body(s.buildings,e.building_id);const auto cells=building_footprint_cells(s.profile,s.rule_version,closed.kind,closed.cell);
        for(const auto& peer:s.buildings){
            if(peer.id==closed.id)continue;
            if(closed.id<peer.id?closed.placed_tick>peer.placed_tick:peer.placed_tick>closed.placed_tick)invalid("Closed Agriculture purchase contradicts monotone placement time");
            const auto initial=peer.kind==Object::OriginalEliteHouse?Object::OriginalElitePlot:peer.kind;
            for(const auto cell:cells){
                const bool overlap=peer.kind==Object::OriginalFishingQuay?building_footprint_contains(s,peer.id,cell):
                    building_footprint_contains(s.profile,s.rule_version,initial,peer.cell,cell);
                if(overlap){
                    const auto* older=agriculture_reconfiguration_entry(&*s.agriculture_reconfiguration,peer.id);
                    const bool separated=closed.id<peer.id?*e.removed_tick<=peer.placed_tick:
                        older&&older->removed_tick&&*older->removed_tick<=closed.placed_tick;
                    if(!separated)invalid("Agriculture purchase footprints overlap during retained paid lifetimes");
                }
                if(peer.kind==Object::OriginalEliteHouse&&building_footprint_contains(s.profile,s.rule_version,peer.kind,peer.cell,cell)&&
                    !building_footprint_contains(s.profile,s.rule_version,initial,peer.cell,cell)){
                    const auto& history=exact(s.original_production->housing.houses,peer.id,[](const auto& row){return row.building_id;});
                    if(!history.first_admission_tick||*e.removed_tick>=*history.first_admission_tick)invalid("Retired leaf overlaps an already materialized Elite extension");
                }
            }
        }
        const auto touching=[&](Cell cell){return std::find(cells.begin(),cells.end(),cell)!=cells.end();};
        for(const auto& ditch:s.irrigation->ditches)if(touching(ditch.cell)&&*e.removed_tick>ditch.placed_tick)invalid("Retired Agriculture body overlaps an earlier paid ditch");
        for(const auto& bridge:s.bridges){
            if(bridge.water_cells.size()>bridge_max_water_cells)invalid("Retired Agriculture geometry exceeds bounded Bridge claims");
            bool overlap=touching(bridge.bank_a)||touching(bridge.bank_b);for(const auto cell:bridge.water_cells)overlap|=touching(cell);
            if(overlap&&*e.removed_tick>bridge.placed_tick)invalid("Retired Agriculture body overlaps a permanent Bridge lifetime");
        }
        for(const auto& ramp:s.ramps)if((touching(ramp.endpoint_a)||touching(ramp.endpoint_b))&&*e.removed_tick>ramp.placed_tick)invalid("Retired Agriculture body overlaps a permanent Ramp lifetime");
        if(s.original_influence)for(const auto& source:s.original_influence->removed_sources){
            const auto& origin=exact(s.original_production->housing.neutral_source_origins,source.building_id,[](const auto& row){return row.building_id;});
            for(const auto cell:cells)if(building_footprint_contains(s.profile,s.rule_version,Object::OriginalInfluenceSource,origin.origin,cell)){
                const bool separated=closed.id<source.building_id?*e.removed_tick<=source.placed_tick:source.removed_tick<=closed.placed_tick;
                if(!separated)invalid("Retired Agriculture body overlaps a closed neutral source lifetime");
            }
        }
    }
}
} // namespace
Command begin_agriculture_drain(BuildingId id){Command command{CommandType::BeginAgricultureDrain};command.building_id=id;return command;}
World World::create_agriculture_reconfiguration(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kAgricultureReconfigurationRuleVersion,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_agriculture_reconfiguration(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    if(s.profile!=RulesProfile::CityV16||s.rule_version!=kAgricultureReconfigurationRuleVersion)invalid("Agriculture reconfiguration restore requires exact Rule36");
    validate_agriculture_reconfiguration_snapshot(s);
    auto w=restore_irrigation_generation(kAgricultureReconfigurationRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::create_irrigated_harvest(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kIrrigatedHarvestRuleVersion,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_irrigated_harvest(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    if(!simulation::irrigated_harvest_enabled(s.profile,s.rule_version))invalid("Irrigated harvest restore requires exact Rule37");
    validate_agriculture_reconfiguration_snapshot(s);
    auto w=restore_irrigation_generation(kIrrigatedHarvestRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::create_quality_aware_food_dispatch(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kQualityAwareFoodDispatchRuleVersion,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_quality_aware_food_dispatch(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    if(!simulation::quality_aware_food_dispatch_enabled(s.profile,s.rule_version))invalid("Quality-aware Food dispatch restore requires exact Rule38");
    validate_agriculture_reconfiguration_snapshot(s);
    auto w=restore_irrigation_generation(kQualityAwareFoodDispatchRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.validate_fishing_context();w.validate_hunting_context();return w;
}
bool World::agriculture_draining(BuildingId id)const{const auto* e=agriculture_reconfiguration_entry(agriculture_reconfiguration_?&*agriculture_reconfiguration_:nullptr,id);return e&&!e->removed_tick;}
bool World::agriculture_retired(BuildingId id)const{const auto* e=agriculture_reconfiguration_entry(agriculture_reconfiguration_?&*agriculture_reconfiguration_:nullptr,id);return e&&e->removed_tick.has_value();}
void World::validate_agriculture_reconfiguration_snapshot(const WorldSnapshot& s){
    validate_entries(s.agriculture_reconfiguration?&*s.agriculture_reconfiguration:nullptr,s.buildings,s.couriers,s.profile,s.rule_version,s.ticks,s.command_sequence);
    if(!s.agriculture_reconfiguration)return;
    if(s.width<1||s.width>228||s.height<1||s.height>228||!s.original_production||!s.processed_food||!s.plantations||!s.irrigation||!s.physical_food_growers||!s.multi_field_agriculture||!s.seasonal_agriculture||!s.irrigation_productivity)invalid("Rule36 requires complete inherited work authority");
    validate_retired_geometry(s);
    validate_drain_work(*s.agriculture_reconfiguration,s.buildings,*s.processed_food,*s.multi_field_agriculture,*s.seasonal_agriculture,*s.plantations,*s.irrigation_productivity,*s.physical_food_growers,s.ticks);
    for(const auto& e:s.agriculture_reconfiguration->entries)if(e.removed_tick){
        for(const auto& p:s.plantations->plants)if(p.plant_id==e.building_id&&(p.growth||p.harvested!=p.returned))invalid("Retired plantation retains work or carried output");
        for(const auto& source:s.plantations->sources)if(source.latest_trip&&source.latest_trip->plant_id==e.building_id&&!source.latest_trip->returned_tick)invalid("Retired plant retains a live Grower");
        for(const auto& source:s.physical_food_growers->farms)if(source.latest_trip&&source.latest_trip->field_id==e.building_id&&!source.latest_trip->returned_tick)invalid("Retired Field retains a live Grower");
        if(s.original_production&&s.original_production->fire_protection)for(const auto& a:s.original_production->fire_protection->arrivals)if(a.trip.target==e.building_id&&a.tick>*e.removed_tick)invalid("Retired Pump received a later Inspector arrival");
    }
}
void World::validate_agriculture_reconfiguration_state()const{
    validate_entries(agriculture_reconfiguration_?&*agriculture_reconfiguration_:nullptr,buildings_,couriers_,profile_,rule_version_,ticks_,command_sequence_);
    if(agriculture_reconfiguration_enabled())validate_drain_work(*agriculture_reconfiguration_,buildings_,*processed_food_,*multi_field_agriculture_,*seasonal_agriculture_,*plantations_,*irrigation_productivity_,*physical_food_growers_,ticks_);
}
DemolitionStatus World::agriculture_demolition_status(BuildingId id)const{
    DemolitionStatus s;
    const auto at=std::lower_bound(buildings_.begin(),buildings_.end(),id,[](const auto& b,BuildingId key){return b.id<key;});
    const auto blocked=[&](DemolitionBlocker why,std::string reason){s.blocker=why;s.reason=std::move(reason);return s;};
    if(at==buildings_.end()||at->id!=id||!at->placed)return blocked(DemolitionBlocker::MissingBuilding,"Paid building is absent or already retired");
    const auto& b=*at;
    if(b.kind==Object::OriginalInfluenceSource){
        const auto checked=validate_original_influence_command(demolish_building(id));
        s.allowed=checked.accepted;s.reason=checked.reason;s.blocker=s.allowed?DemolitionBlocker::None:DemolitionBlocker::HistoricalReference;return s;
    }
    for(const auto& c:couriers_){if(c.owner==id&&c.phase!=CourierPhase::IdleAtWorkshop)++s.active_couriers;else if(c.target==id&&c.phase!=CourierPhase::IdleAtWorkshop)++s.incoming_deliveries;}
    if(!leaf(b.kind)){
        if(s.active_couriers)return blocked(DemolitionBlocker::OwnedActiveCourier,"Owned Worker/Food courier is active or returning; preserve cargo and repair its route. Source retirement is unavailable");
        if(fishing_)for(const auto& boat:fishing_->boats)if(boat.quay_id==id)return blocked(DemolitionBlocker::OwnedPayload,"Quay owns a real Boat/cargo/dock reservation; let it return and unload. Quay retirement is unavailable");
        if(hunting_)for(const auto& hunter:hunting_->hunters)if(hunter.tent_id==id)return blocked(DemolitionBlocker::OwnedPayload,"Tent owns a real Hunter/cargo/prey reservation; let it return and unload. Tent retirement is unavailable");
        if(s.incoming_deliveries||b.reserved_incoming||b.reserved_food_incoming)return blocked(DemolitionBlocker::IncomingReservation,"Actual incoming or returning delivery retains this source; no cancellation or stock discard");
        if(processed_food_)for(const auto& stock:processed_food_->stocks)if(stock.building_id==id&&std::any_of(stock.amounts.begin(),stock.amounts.end(),[](auto a){return a!=0;}))return blocked(DemolitionBlocker::StoredGoods,"Stored Food requires physical evacuation; stock-owner retirement is unavailable");
        if(food_processing_)for(const auto& mill:food_processing_->mills)if(mill.mill_id==id&&mill.active)return blocked(DemolitionBlocker::ActiveRecipe,"Mill retains physical recipe inputs and active work; finish production. Mill retirement is unavailable");
        return blocked(DemolitionBlocker::UnsupportedKind,"Source/House retirement is unavailable: retained stocks, channels, children and histories stay authoritative. Reconfigure its supported Fields/plants/Pumps instead");
    }
    s.draining=agriculture_draining(id);s.drain_supported=true;
    s.drain_reason=s.draining?"Draining: no new cycles; existing work and loaded returns finish physically":"Drain stops new cycles; existing work and real returns finish. No refund";
    const auto* entry=agriculture_reconfiguration_entry(&*agriculture_reconfiguration_,id);
    if(!entry&&agriculture_reconfiguration_->entries.size()>=agriculture_reconfiguration_limit){s.drain_supported=false;return blocked(DemolitionBlocker::HistoryCapacity,"Eight-row lifetime reconfiguration journal is full");}
    if(command_sequence_>=UINT64_MAX-1||road_revision_==UINT64_MAX){s.drain_supported=false;return blocked(DemolitionBlocker::ExhaustedCounter,"Command/topology counter exhausted");}
    if(building_on_fire(id))return blocked(DemolitionBlocker::OnFire,"Wait for the real fire to expire; no removal during fire");
    if(s.incoming_deliveries)return blocked(DemolitionBlocker::IncomingDelivery,"Actual Worker/Inspector is travelling, working or returning; preserve its begun edge/cargo and repair its route");
    for(const auto& f:agriculture_fields())if(f.field_id==id&&agriculture_field_progress(id))return blocked(DemolitionBlocker::ActiveProduction,"Field retains partial/ready crop work; drain through actual harvest/return or ordinary seasonal expiry");
    for(const auto& p:plantations_->plants)if(p.plant_id==id&&(p.growth||p.harvested!=p.returned))return blocked(DemolitionBlocker::ActiveProduction,"Plant retains partial/ready growth or physical cargo; drain through actual pickup and return");
    const auto cells=building_cells(id);const auto claimed=[&](Cell c){return std::find(cells.begin(),cells.end(),c)!=cells.end();};
    for(const auto& c:couriers_)if(c.phase!=CourierPhase::IdleAtWorkshop&&!c.path.empty()&&(claimed(c.path.at(c.path_vertex))||(c.edge_progress&&claimed(c.path.at(c.path_vertex+1)))))return blocked(DemolitionBlocker::OwnedActiveCourier,"Another real actor occupies the footprint or its begun edge; wait for physical clearance");
    if(hunting_){for(const auto& h:hunting_->hunters)if(claimed(h.current)||(h.edge_progress&&h.path_vertex+1<h.path.size()&&claimed(h.path[h.path_vertex+1])))return blocked(DemolitionBlocker::OwnedActiveCourier,"Hunter occupies the footprint or begun edge; wait for physical clearance");for(const auto& a:hunting_->animals)if(claimed(a.current)||(a.next&&claimed(*a.next)))return blocked(DemolitionBlocker::OwnedActiveCourier,"Living prey occupies the footprint or begun edge; no animal deletion");}
    if(fishing_)for(const auto& boat:fishing_->boats)if(claimed(boat.current)||(boat.edge_progress&&boat.path_vertex+1<boat.path.size()&&claimed(boat.path[boat.path_vertex+1])))return blocked(DemolitionBlocker::OwnedActiveCourier,"Boat occupies the footprint or begun edge; preserve its real water return");
    s.allowed=true;s.reason="Ready: complete paid footprint retires; all work/receipts remain. No refund";return s;
}
CommandResult World::validate_agriculture_reconfiguration_command(Command c)const{
    if(!agriculture_reconfiguration_enabled())return {false,false,"Requires explicit Agriculture reconfiguration Rule36",command_sequence_,ticks_};
    const auto s=agriculture_demolition_status(c.building_id);
    if(c.type==CommandType::BeginAgricultureDrain){const bool okay=s.drain_supported&&!s.draining;return {okay,okay,okay?s.drain_reason:s.reason,okay?command_sequence_+1:command_sequence_,ticks_};}
    return {s.allowed,s.allowed,s.reason,s.allowed?command_sequence_+1:command_sequence_,ticks_};
}
CommandResult World::execute_agriculture_reconfiguration_command(Command c,bool refresh_after){
    auto result=validate_agriculture_reconfiguration_command(c);if(!result.accepted)return result;
    if(building(c.building_id).kind==Object::OriginalInfluenceSource)return execute_original_influence_command(c,refresh_after);
    World next=*this;auto& rows=next.agriculture_reconfiguration_->entries;
    auto entry=std::lower_bound(rows.begin(),rows.end(),c.building_id,[](const auto& e,BuildingId id){return e.building_id<id;});
    ++next.command_sequence_;
    if(entry==rows.end()||entry->building_id!=c.building_id){
        entry=rows.insert(entry,{c.building_id,ticks_,next.command_sequence_,{}, {},{}});
        entry->initial_work=current_work(c.building_id,*processed_food_,*multi_field_agriculture_,*seasonal_agriculture_,*plantations_,*irrigation_productivity_);
    }
    if(c.type==CommandType::DemolishBuilding){
        const auto cells=next.building_cells(c.building_id);auto& b=next.mutable_building(c.building_id);
        for(const auto cell:cells){next.objects_[next.index(cell)]=Object::Empty;next.owners_[next.index(cell)]=0;}
        b.placed=false;entry->removed_tick=ticks_;entry->removed_sequence=next.command_sequence_;++next.road_revision_;
        next.cached_route_.reset();next.tick_staffed_.clear();next.tick_staffing_active_=false;
        if(refresh_after)next.refresh_routes();result.reason="Agriculture leaf retired. No refund; histories and lifetime IDs retained";
    }else result.reason="Draining started. Finish work and real returns, then explicitly demolish when ready";
    next.refresh_irrigation();next.validate_agriculture_reconfiguration_state();
    performance::increment(performance::Counter::WorldExecutes);*this=std::move(next);return result;
}
} // namespace openemperor::simulation
