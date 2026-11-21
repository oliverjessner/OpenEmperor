#include "simulation/World.h"
#include <algorithm>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason){throw std::invalid_argument(reason);}
template<class Rows,class Key> decltype(auto) find(Rows& rows,BuildingId id,Key key){
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[&](const auto& value,BuildingId target){return key(value)<target;});
    if(at==rows.end()||key(*at)!=id)invalid("Irrigation work lost an exact paid crop/source binding");
    return *at;
}
const auto body_key=[](const auto& b){return b.id;};
const auto field_key=[](const auto& b){return b.field_id;};
const auto plant_key=[](const auto& b){return b.plant_id;};
const auto farm_key=[](const auto& b){return b.farm_id;};
const auto work_key=[](const auto& b){return b.building_id;};
std::optional<original::ModelBuilding> crop_model(Object kind){
    switch(kind){
    case Object::OriginalWheatField:return original::ModelBuilding::WheatField;
    case Object::OriginalMilletField:return original::ModelBuilding::MilletField;
    case Object::OriginalRiceField:return original::ModelBuilding::RiceField;
    case Object::OriginalCabbageField:return original::ModelBuilding::CabbageField;
    case Object::OriginalSoybeanField:return original::ModelBuilding::SoybeanField;
    case Object::OriginalHempField:return original::ModelBuilding::HempField;
    case Object::OriginalTeaBush26:return original::ModelBuilding::TeaBush;
    case Object::OriginalLacquerTree27:return original::ModelBuilding::LacquerTree;
    case Object::OriginalMulberryTree28:return original::ModelBuilding::MulberryTree;
    default:return {};
    }
}
struct WorkContext {BuildingId source{};std::uint64_t base=0;std::uint32_t progress=0;
    std::optional<std::uint64_t> ready_tick;std::uint64_t wasted_work=0;};
template<class Buildings>
WorkContext context(BuildingId id,const Buildings& bodies,const HuntingFoodState& food,
    const MultiFieldAgricultureState& multi,const SeasonalAgricultureState& season,
    const PlantationsState& plants,const OriginalProductionState& production,const IrrigationProductivityState& water){
    const auto& b=find(bodies,id,body_key);
    if(b.kind==Object::OriginalHempField){
        const auto at=std::find_if(production.hemp_farms.begin(),production.hemp_farms.end(),[&](const auto& f){return f.field_id==id;});
        if(at==production.hemp_farms.end())invalid("Irrigation Hemp work lacks its paid Farm");
        const auto& cycle=find(season.hemp_farms,at->farm_id,farm_key);
        return {at->farm_id,cycle.productive_ticks,
            static_cast<std::uint32_t>(find(bodies,at->farm_id,body_key).progress),cycle.ready_tick,cycle.wasted_work};
    }
    if(b.kind==Object::OriginalTeaBush26||b.kind==Object::OriginalLacquerTree27||b.kind==Object::OriginalMulberryTree28){
        const auto& p=find(plants.plants,id,plant_key);const auto& cycle=find(season.plants,id,plant_key);
        return {p.source_id,p.productive_ticks,p.growth,cycle.ready_tick,cycle.wasted_work};
    }
    const auto& f=find(food.fields,id,field_key);const auto& w=find(multi.fields,id,field_key);
    const auto& cycle=find(season.fields,id,field_key);
    const auto earned=irrigation_earned_work(w.productive_ticks,&water,id);
    if(cycle.wasted_work>earned||f.harvested>(earned-cycle.wasted_work)/80)invalid("Irrigation Food work underflow");
    const auto pending=earned-cycle.wasted_work-f.harvested*80;
    if(pending>80)invalid("Irrigation Food work exceeded one bounded cycle");
    return {f.farm_id,w.productive_ticks,static_cast<std::uint32_t>(pending),cycle.ready_tick,cycle.wasted_work};
}
template<class Buildings>
void validate_work(const IrrigationProductivityState& water,const Buildings& bodies,const HuntingFoodState& food,
    const MultiFieldAgricultureState& multi,const SeasonalAgricultureState& season,
    const PlantationsState& plants,const OriginalProductionState& production,const PhysicalFoodGrowersState& physical,
    const IrrigationState& irrigation,std::uint64_t tick,bool inspect_retained_ditch_epochs){
    const auto expected=std::count_if(bodies.begin(),bodies.end(),[](const auto& b){return b.placed&&crop_model(b.kind).has_value();});
    if(water.work.size()>66||static_cast<std::ptrdiff_t>(water.work.size())!=expected)
        invalid("Irrigation work must bind every paid eligible crop exactly once");
    std::optional<std::uint64_t> first_possible_supply;
    for(const auto& pump:irrigation.pumps){
        const auto placed=find(bodies,pump.building_id,body_key).placed_tick;
        std::uint64_t phase=28;
        if(placed>=28){
            const auto periods=(placed-28)/51+1;
            if(periods>(UINT64_MAX-28)/51)continue;
            phase=28+periods*51;
        }
        if(!first_possible_supply||phase<*first_possible_supply)first_possible_supply=phase;
    }
    // Full retained-ditch chronology is checked at snapshot publication/restore,
    // never scanned in normal tick validation. Runtime work uses the sole actual
    // water cache; legal commands cannot backdate installation.
    if(inspect_retained_ditch_epochs&&first_possible_supply&&!irrigation.ditches_removed_total){
        if(irrigation.ditches.empty())first_possible_supply.reset();
        else {
            const auto first=std::min_element(irrigation.ditches.begin(),irrigation.ditches.end(),
                [](const auto& a,const auto& b){return a.placed_tick<b.placed_tick;})->placed_tick;
            std::uint64_t phase=28;
            if(first>=28){
                const auto periods=(first-28)/51+1;
                if(periods>(UINT64_MAX-28)/51)first_possible_supply.reset();
                else phase=28+periods*51;
            }
            if(first_possible_supply)*first_possible_supply=std::max(*first_possible_supply,phase);
        }
    }
    const auto wet_capacity=[&](BuildingId id,std::uint64_t through){
        if(!irrigation.ditches_placed_total||!first_possible_supply||*first_possible_supply>=through)return std::uint64_t{0};
        const auto& b=find(bodies,id,body_key);
        return seasonal_work_opportunities(*crop_model(b.kind),std::max(b.placed_tick,*first_possible_supply),through);
    };
    for(std::size_t i=0;i<water.work.size();++i){
        const auto& w=water.work[i];const auto& b=find(bodies,w.building_id,body_key);
        if((i&&!(water.work[i-1].building_id<w.building_id))||!b.placed||!crop_model(b.kind)||b.placed_tick>tick)
            invalid("Irrigation work lost its sorted complete paid crop identity");
        const auto c=context(w.building_id,bodies,food,multi,season,plants,production,water);
        if(w.wet_work_ticks>c.base||w.bonus_work>w.wet_work_ticks/4||w.clipped_bonus!=w.wet_work_ticks/4-w.bonus_work||
           c.base>seasonal_work_opportunities(*crop_model(b.kind),b.placed_tick,tick))
            invalid("Irrigation rational carry or actual eligible work history invalid");
        if(w.wet_work_ticks>wet_capacity(w.building_id,tick))
            invalid("Wet work predates possible paid Pump/ditch supply");
        if(c.ready_tick&&(*c.ready_tick>tick||w.wet_work_ticks>wet_capacity(w.building_id,*c.ready_tick)))
            invalid("Ready crop borrowed wet work from after its immutable ready epoch");
        // Cap80 may discard at most one bonus per completed/expired cycle.
        const auto earned=irrigation_earned_work(c.base,&water,w.building_id);
        if(w.clipped_bonus>earned/80)invalid("Irrigation clipped bonus exceeds real bounded crop cycles");
    }
    const auto witness=[&](const std::optional<IrrigationWorkWitness>& value,BuildingId id,std::uint64_t ready,std::uint64_t ordinal){
        if(!value)invalid("Material trip/receipt lacks its immutable irrigation readiness work");
        const auto& v=*value;const auto& w=find(water.work,id,work_key);const auto& b=find(bodies,id,body_key);
        const auto c=context(id,bodies,food,multi,season,plants,production,water);
        if(!ordinal||ready<=b.placed_tick||ready>tick||v.base_work>c.base||v.wet_work_ticks>w.wet_work_ticks||
           v.bonus_work>w.bonus_work||v.clipped_bonus>w.clipped_bonus||v.wasted_work>c.wasted_work||
           v.wet_work_ticks>v.base_work||v.bonus_work>v.wet_work_ticks/4||v.clipped_bonus!=v.wet_work_ticks/4-v.bonus_work||
           v.bonus_work>UINT64_MAX-v.base_work||v.base_work>seasonal_work_opportunities(*crop_model(b.kind),b.placed_tick,ready)||
           v.wet_work_ticks>wet_capacity(id,ready))
            invalid("Material readiness work is foreign, future or impossible before its ready epoch");
        if(c.ready_tick&&*c.ready_tick==ready&&
           (v.base_work!=c.base||v.wet_work_ticks!=w.wet_work_ticks||v.bonus_work!=w.bonus_work||
            v.clipped_bonus!=w.clipped_bonus||v.wasted_work!=c.wasted_work))
            invalid("Material witness contradicts its still-current frozen ready cycle");
        const auto earned=v.base_work+v.bonus_work;
        if(v.wasted_work>earned||(earned-v.wasted_work)%80||(earned-v.wasted_work)/80!=ordinal||
           v.clipped_bonus>earned/80||c.base-v.base_work>seasonal_work_opportunities(*crop_model(b.kind),ready,tick)||
           w.wet_work_ticks-v.wet_work_ticks>wet_capacity(id,tick)-wet_capacity(id,ready))
            invalid("Material readiness work lost its exact crop ordinal or borrowed later regrowth");
    };
    for(const auto& source:plants.sources)if(source.latest_trip){
        const auto& t=*source.latest_trip;const auto& p=find(plants.plants,t.plant_id,plant_key);
        if(!t.picked_tick&&p.harvested==UINT64_MAX)invalid("Material crop ordinal exhausted");
        witness(t.irrigation_ready_work,t.plant_id,t.ready_tick,p.harvested+static_cast<unsigned>(!t.picked_tick));
    }
    for(const auto& p:plants.plants)if(p.last_receipt){
        const auto& r=*p.last_receipt;witness(r.irrigation_ready_work,p.plant_id,r.ready_tick,p.returned);
        const auto& source=find(plants.sources,p.source_id,work_key);
        if(source.latest_trip&&source.latest_trip->grower_id==r.grower_id&&source.latest_trip->irrigation_ready_work!=r.irrigation_ready_work)
            invalid("Material receipt contradicts its actual trip readiness work");
    }
    for(const auto& f:season.hemp_farms){
        const auto& base=find(production.hemp_farms,f.farm_id,farm_key);
        if(f.latest_trip){const auto& t=*f.latest_trip;
            if(!t.picked_tick&&f.picked==UINT64_MAX)invalid("Hemp crop ordinal exhausted");
            witness(t.irrigation_ready_work,t.field_id,t.ready_tick,f.picked+static_cast<unsigned>(!t.picked_tick));}
        if(f.last_receipt){const auto& r=*f.last_receipt;
            witness(r.irrigation_ready_work,r.field_id,r.ready_tick,base.harvested);
            if(f.latest_trip&&f.latest_trip->grower_id==r.grower_id&&f.latest_trip->irrigation_ready_work!=r.irrigation_ready_work)
                invalid("Hemp receipt contradicts its actual trip readiness work");}
    }
    for(const auto& farm:physical.farms)if(farm.latest_trip){
        const auto& t=*farm.latest_trip;const auto& w=find(water.work,t.field_id,work_key);
        if(t.irrigation_bonus_before>w.bonus_work||w.bonus_work-t.irrigation_bonus_before>(std::uint64_t(t.work_ticks)+3)/4||
           t.irrigation_bonus_before>t.work_before/4||t.irrigation_bonus_before>wet_capacity(t.field_id,t.dispatched_tick)/4)
            invalid("Physical Food trip lost its earned irrigation bonus provenance");
        if(t.ready_tick&&w.wet_work_ticks>wet_capacity(t.field_id,*t.ready_tick))
            invalid("Food trip readiness borrowed later wet work");
        const auto before=irrigation_earned_work(t.work_before,nullptr,t.field_id);
        if(t.irrigation_bonus_before>UINT64_MAX-before||t.wasted_work_before>before+t.irrigation_bonus_before)
            invalid("Physical Food trip irrigation work overflow or waste underflow");
        const auto pending=(before+t.irrigation_bonus_before-t.wasted_work_before)%80;
        if(t.work_ticks+w.bonus_work-t.irrigation_bonus_before>80-pending)
            invalid("Physical Food trip overfilled its original crop cycle");
    }
    for(const auto& f:physical.fields)if(f.last_receipt){
        const auto& r=*f.last_receipt;const auto& w=find(water.work,f.field_id,work_key);
        if(r.irrigation_bonus_at_harvest>w.bonus_work||r.irrigation_bonus_at_harvest>r.work_at_harvest/4||
           r.irrigation_bonus_at_harvest>UINT64_MAX-r.work_at_harvest||
           r.irrigation_bonus_at_harvest>wet_capacity(f.field_id,r.ready_tick)/4)
            invalid("Food receipt lost its actual earned irrigation work");
        const auto& source=find(physical.farms,f.farm_id,farm_key);
        if(source.latest_trip&&source.latest_trip->grower_id==r.grower_id&&r.irrigation_bonus_at_harvest!=w.bonus_work)
            invalid("Food receipt irrigation work contradicts its actual latest return");
    }
}
}
World World::create_irrigation_productivity(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kIrrigationProductivityRuleVersion,std::move(p),std::move(m),std::move(t));
    w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_irrigation_productivity(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    validate_irrigation_productivity_snapshot(s);
    auto w=restore_irrigation_generation(kIrrigationProductivityRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_fishing_context();w.validate_hunting_context();return w;
}
IrrigationWorkWitness World::irrigation_ready_work(BuildingId id)const{
    if(!irrigation_productivity_enabled())invalid("Irrigation readiness work requires Rule33");
    const auto& w=find(irrigation_productivity_->work,id,work_key);
    const auto c=context(id,buildings_,*hunting_food_,*multi_field_agriculture_,*seasonal_agriculture_,*plantations_,*original_production_,*irrigation_productivity_);
    if(c.progress!=80||!c.ready_tick)invalid("Irrigation readiness work cannot be captured before actual crop readiness");
    return {c.base,w.wet_work_ticks,w.bonus_work,w.clipped_bonus,c.wasted_work};
}
std::uint32_t World::irrigation_growth_increment(BuildingId id,std::uint32_t remaining){
    if(!remaining||remaining>80)invalid("Irrigation work requires a real unfinished crop cycle");
    if(!irrigation_productivity_enabled())return 1;
    auto& w=find(irrigation_productivity_->work,id,work_key);
    if(!irrigation_field_status(building(id).cell).water_available)return 1;
    if(w.wet_work_ticks==UINT64_MAX)invalid("Irrigation wet-work history exhausted");
    ++w.wet_work_ticks;
    if(w.wet_work_ticks%4)return 1;
    if(remaining==1){if(w.clipped_bonus==UINT64_MAX)invalid("Irrigation clipped history exhausted");++w.clipped_bonus;return 1;}
    if(w.bonus_work==UINT64_MAX)invalid("Irrigation bonus history exhausted");++w.bonus_work;return 2;
}
void World::validate_irrigation_productivity_snapshot(const WorldSnapshot& s){
    if(!simulation::irrigation_productivity_enabled(s.profile,s.rule_version)){
        if(s.irrigation_productivity)invalid("Irrigation productivity requires exact Rule33/schema49");
        if(s.plantations){
            for(const auto& p:s.plantations->plants)if(p.last_receipt&&p.last_receipt->irrigation_ready_work)
                invalid("Historical plantation receipt cannot own irrigation readiness work");
            for(const auto& p:s.plantations->sources)if(p.latest_trip&&p.latest_trip->irrigation_ready_work)
                invalid("Historical plantation trip cannot own irrigation readiness work");
        }
        if(s.seasonal_agriculture)for(const auto& f:s.seasonal_agriculture->hemp_farms){
            if((f.latest_trip&&f.latest_trip->irrigation_ready_work)||(f.last_receipt&&f.last_receipt->irrigation_ready_work))
                invalid("Historical Hemp cannot own irrigation readiness work");
        }
        if(s.physical_food_growers){
            for(const auto& f:s.physical_food_growers->farms)if(f.latest_trip&&f.latest_trip->irrigation_bonus_before)
                invalid("Historical Food trip cannot own irrigation productivity");
            for(const auto& f:s.physical_food_growers->fields)if(f.last_receipt&&f.last_receipt->irrigation_bonus_at_harvest)
                invalid("Historical Food receipt cannot own irrigation productivity");
        }
        return;
    }
    if(!s.irrigation_productivity||!s.irrigation||!s.hunting_food||!s.multi_field_agriculture||!s.seasonal_agriculture||
       !s.plantations||!s.original_production||!s.physical_food_growers)
        invalid("Irrigation productivity lacks its exact inherited physical authorities");
    validate_work(*s.irrigation_productivity,s.buildings,*s.hunting_food,*s.multi_field_agriculture,*s.seasonal_agriculture,
        *s.plantations,*s.original_production,*s.physical_food_growers,*s.irrigation,s.ticks,true);
}
void World::validate_irrigation_productivity_state()const{
    if(irrigation_productivity_enabled())validate_work(*irrigation_productivity_,buildings_,*hunting_food_,*multi_field_agriculture_,
        *seasonal_agriculture_,*plantations_,*original_production_,*physical_food_growers_,*irrigation_,ticks_,false);
}
IrrigationProductivityStatus World::irrigation_productivity_status(BuildingId id)const{
    IrrigationProductivityStatus r;r.building_id=id;
    if(!irrigation_productivity_enabled()){r.reason="Irrigation productivity requires Rule33";return r;}
    const auto it=std::lower_bound(irrigation_productivity_->work.begin(),irrigation_productivity_->work.end(),id,
        [](const auto& w,BuildingId key){return w.building_id<key;});
    if(it==irrigation_productivity_->work.end()||it->building_id!=id){r.reason="This paid body is not a crop work recipient";return r;}
    const auto& b=building(id);const auto model=*crop_model(b.kind);
    const auto c=context(id,buildings_,*hunting_food_,*multi_field_agriculture_,*seasonal_agriculture_,*plantations_,*original_production_,*irrigation_productivity_);
    const auto& source=building(c.source);const auto water=irrigation_field_status(b.cell);
    r.supported=true;r.water_available=water.water_available;r.wet_ditch=water.wet_ditch;r.supplying_pump=water.supplying_pump;
    r.base_work_ticks=c.base;r.wet_work_ticks=it->wet_work_ticks;r.bonus_work=it->bonus_work;r.clipped_bonus=it->clipped_bonus;
    r.progress=c.progress;r.carry=static_cast<std::uint8_t>(it->wet_work_ticks%4);r.next_opening_tick=seasonal_next_opening(model,ticks_);
    if(c.progress==80)r.reason="Ready crop; physical harvest/pickup earns no irrigation bonus";
    else if(!seasonal_growing(model,ticks_))r.reason="Outside the crop growing window";
    else if(!source.operating_enabled||!b.operating_enabled)r.reason="Source or crop paused";
    else if(building_on_fire(source.id)||(b.kind!=Object::OriginalTeaBush26&&b.kind!=Object::OriginalLacquerTree27&&b.kind!=Object::OriginalMulberryTree28&&building_on_fire(id)))r.reason="Source or field on fire";
    else if(workers_assigned(source.id)<workforce_required(source.id))r.reason="Source has no complete assigned workforce";
    else if(building_entrances(source.id).empty())r.reason="Source has no road access";
    else {
        bool at_work=true;
        if(source.kind==Object::OriginalFoodFarm){
            const auto& f=find(physical_food_growers_->farms,source.id,farm_key);
            at_work=f.latest_trip&&!f.latest_trip->returned_tick&&f.latest_trip->field_id==id&&
                f.latest_trip->phase==PhysicalFoodGrowerPhase::Working&&!f.latest_trip->cancel_requested;
        }
        if(!at_work)r.reason="Waiting for actual Food Grower field work";
        else {r.work_eligible=true;r.bonus_active=r.water_available;
            r.next_increment=static_cast<std::uint8_t>(r.water_available&&r.carry==3&&r.progress<79?2:1);
            r.reason=r.water_available?"Actual supplied water; +1 growth on every fourth eligible wet work tick":"Dry; base work only";}
    }
    return r;
}
}
