#include "simulation/World.h"
#include <algorithm>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason){throw std::invalid_argument(reason);}
void add(std::uint64_t& n,std::uint64_t v){if(v>UINT64_MAX-n)invalid("Seasonal history exhausted");n+=v;}
template<class Rows,class Id,class Key> decltype(auto) row(Rows& rows,Id id,Key key){
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[&](const auto& v,Id target){return key(v)<target;});
    if(at==rows.end()||key(*at)!=id)invalid("Seasonal state lost exact paid identity");return *at;
}
const auto body_id=[](const auto& r){return r.id;};
const auto food_id=[](const auto& r){return r.field_id;};
const auto plant_id=[](const auto& r){return r.plant_id;};
original::ModelBuilding food_model(AgricultureCrop crop){return static_cast<original::ModelBuilding>(195+static_cast<unsigned>(crop));}
original::ModelBuilding plant_model(Object kind){
    switch(kind){case Object::OriginalTeaBush26:return original::ModelBuilding::TeaBush;
    case Object::OriginalLacquerTree27:return original::ModelBuilding::LacquerTree;
    case Object::OriginalMulberryTree28:return original::ModelBuilding::MulberryTree;
    default:invalid("Unsupported seasonal plant identity");}
}
void ready_valid(original::ModelBuilding model,std::optional<std::uint64_t> ready,std::uint64_t tick,std::uint64_t placed){
    if(ready&&(*ready<=placed||!seasonal_growing(model,*ready)||!seasonal_harvest_allowed(model,tick,*ready)))
        invalid("Seasonal readiness lacks a live source-bound growing epoch");
}
template<class Buildings>
void validate_food_plants(const SeasonalAgricultureState& s,const HuntingFoodState& food,const MultiFieldAgricultureState& work,
    const PlantationsState& plants,const Buildings& bodies,std::uint64_t tick){
    if(s.fields.size()!=food.fields.size()||s.plants.size()!=plants.plants.size()||s.fields.size()>agriculture_field_limit||s.plants.size()>plantation_plant_limit)
        invalid("Seasonal authority must bind every paid Field and plant once");
    for(std::size_t i=0;i<s.fields.size();++i){
        const auto& f=s.fields[i];const auto& base=food.fields[i];const auto& paid=row(bodies,f.field_id,body_id);
        const auto& source=row(bodies,f.farm_id,body_id);const auto& w=row(work.fields,f.field_id,food_id);
        if(f.field_id!=base.field_id||f.farm_id!=base.farm_id||w.farm_id!=f.farm_id||f.wasted_work>w.productive_ticks)
            invalid("Seasonal Food identity or work loss invalid");
        const auto net=w.productive_ticks-f.wasted_work;
        if(base.harvested>net/80||net-base.harvested*80>80||f.expired_ready>f.wasted_work/80||
           w.productive_ticks>seasonal_work_opportunities(food_model(base.crop),std::max(paid.placed_tick,source.placed_tick),tick))
            invalid("Seasonal Food work exceeds its paid growing opportunities");
        const auto progress=net-base.harvested*80;
        if(f.ready_tick.has_value()!=(progress==80)||f.harvest_progress>=seasonal_harvest_work_ticks||
           (!f.ready_tick&&f.harvest_progress)||(!seasonal_growing(food_model(base.crop),tick)&&progress>0&&progress<80)||
           base.harvested>f.harvest_work/seasonal_harvest_work_ticks||f.harvest_work>tick-paid.placed_tick||
           f.harvest_work-base.harvested*seasonal_harvest_work_ticks<f.wasted_harvest_work||
           f.harvest_work-base.harvested*seasonal_harvest_work_ticks-f.wasted_harvest_work!=f.harvest_progress)
            invalid("Seasonal Food readiness or actual harvest work equation invalid");
        ready_valid(food_model(base.crop),f.ready_tick,tick,std::max(paid.placed_tick,source.placed_tick));
        if(f.ready_tick&&w.productive_ticks>seasonal_work_opportunities(food_model(base.crop),std::max(paid.placed_tick,source.placed_tick),*f.ready_tick))
            invalid("Food readiness predates its earned growing work");
        if(f.ready_tick&&tick-*f.ready_tick<f.harvest_progress)invalid("Seasonal harvest work predates physical readiness");
    }
    for(std::size_t i=0;i<s.plants.size();++i){
        const auto& f=s.plants[i];const auto& p=plants.plants[i];const auto& paid=row(bodies,f.plant_id,body_id);
        if(f.plant_id!=p.plant_id||f.source_id!=p.source_id||p.growth>80||f.wasted_work>p.productive_ticks)
            invalid("Seasonal plant lost paid source or work");
        const auto net=p.productive_ticks-f.wasted_work;
        if(p.harvested>net/80||net-p.harvested*80!=p.growth||f.expired_ready>f.wasted_work/80||
           p.grown<p.harvested||p.grown-p.harvested<f.expired_ready||p.grown-p.harvested-f.expired_ready!=static_cast<unsigned>(p.growth==80)||
           f.ready_tick.has_value()!=(p.growth==80)||(!seasonal_growing(plant_model(paid.kind),tick)&&p.growth>0&&p.growth<80)||
           p.productive_ticks>seasonal_work_opportunities(plant_model(paid.kind),paid.placed_tick,tick))
            invalid("Seasonal plant work/readiness/expiry equation invalid");
        ready_valid(plant_model(paid.kind),f.ready_tick,tick,paid.placed_tick);
        if(f.ready_tick&&p.productive_ticks>seasonal_work_opportunities(plant_model(paid.kind),paid.placed_tick,*f.ready_tick))
            invalid("Plant readiness predates its earned growing work");
        if(p.last_receipt&&(!p.last_receipt->ready_tick||seasonal_work_opportunities(plant_model(paid.kind),paid.placed_tick,p.last_receipt->ready_tick)<80||
           !seasonal_harvest_allowed(plant_model(paid.kind),p.last_receipt->picked_tick,p.last_receipt->ready_tick)))
            invalid("Seasonal plant receipt picked an expired ready cycle");
    }
    for(const auto& source:plants.sources)if(source.latest_trip){
        const auto& t=*source.latest_trip;const auto& paid=row(bodies,t.plant_id,body_id);
        if(!t.ready_tick||t.ready_tick>t.dispatched_tick||seasonal_work_opportunities(plant_model(paid.kind),paid.placed_tick,t.ready_tick)<80||
           !seasonal_harvest_allowed(plant_model(paid.kind),t.dispatched_tick,t.ready_tick)||
           (t.picked_tick&&!seasonal_harvest_allowed(plant_model(paid.kind),*t.picked_tick,t.ready_tick)))
            invalid("Seasonal plant trip lacks its exact live ready-cycle commitment");
        if(!t.returned_tick&&!t.picked_tick&&seasonal_harvest_allowed(plant_model(paid.kind),tick,t.ready_tick)&&
           row(s.plants,t.plant_id,plant_id).ready_tick!=t.ready_tick)
            invalid("Unexpired plant assignment contradicts its actual ready crop");
    }
}
}
World World::create_seasonal_agriculture(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kSeasonalAgricultureRuleVersion,std::move(p),std::move(m),std::move(t));
    w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_seasonal_agriculture(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    validate_seasonal_agriculture_snapshot(s);
    auto w=restore_irrigation_generation(kSeasonalAgricultureRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_fishing_context();w.validate_hunting_context();return w;
}
std::uint32_t World::seasonal_food_progress(BuildingId id)const{
    if(!seasonal_agriculture_enabled())return 0;
    const auto& f=row(hunting_food_->fields,id,food_id);const auto& season=row(seasonal_agriculture_->fields,id,food_id);
    const auto total=row(multi_field_agriculture_->fields,id,food_id).productive_ticks;
    if(total<season.wasted_work||f.harvested>(total-season.wasted_work)/80)invalid("Seasonal crop progress underflow");
    const auto progress=total-season.wasted_work-f.harvested*80;if(progress>80)invalid("Seasonal crop cycle exceeded80 work units");
    return static_cast<std::uint32_t>(progress);
}
void World::advance_seasonal_agriculture(){
    for(auto& f:seasonal_agriculture_->fields){
        const auto& base=row(hunting_food_->fields,f.field_id,food_id);const auto model=food_model(base.crop);
        const auto progress=seasonal_food_progress(f.field_id);
        if(f.ready_tick&&!seasonal_harvest_allowed(model,ticks_,*f.ready_tick)){
            add(f.wasted_work,80);add(f.expired_ready,1);add(f.wasted_harvest_work,f.harvest_progress);f.harvest_progress=0;f.ready_tick.reset();
        }else if(progress&&progress<80&&!seasonal_growing(model,ticks_))add(f.wasted_work,progress);
    }
    for(auto& f:seasonal_agriculture_->plants){
        auto& p=row(plantations_->plants,f.plant_id,plant_id);const auto model=plant_model(building(f.plant_id).kind);
        if(f.ready_tick&&!seasonal_harvest_allowed(model,ticks_,*f.ready_tick)){
            add(f.wasted_work,p.growth);add(f.expired_ready,1);p.growth=0;f.ready_tick.reset();
        }else if(p.growth&&p.growth<80&&!seasonal_growing(model,ticks_)){add(f.wasted_work,p.growth);p.growth=0;}
    }
    for(auto& f:seasonal_agriculture_->hemp_farms){
        auto& b=mutable_building(f.farm_id);
        if(f.ready_tick&&!seasonal_harvest_allowed(original::ModelBuilding::HempField,ticks_,*f.ready_tick)){
            add(f.wasted_work,static_cast<std::uint64_t>(b.progress));add(f.expired_ready,1);b.progress=0;f.ready_tick.reset();
        }else if(b.progress>0&&b.progress<80&&!seasonal_growing(original::ModelBuilding::HempField,ticks_)){
            add(f.wasted_work,static_cast<std::uint64_t>(b.progress));b.progress=0;
        }
    }
}
void World::validate_seasonal_agriculture_snapshot(const WorldSnapshot& s){
    if(!simulation::seasonal_agriculture_enabled(s.profile,s.rule_version)){
        if(s.seasonal_agriculture||std::any_of(s.couriers.begin(),s.couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalSeasonalHempGrower;}))
            invalid("Seasonal authority requires exact Rule32/schema48");
        return;
    }
    if(!s.seasonal_agriculture||!s.hunting_food||!s.multi_field_agriculture||!s.plantations||!s.original_production||!s.original_goods)
        invalid("Seasonal mode lacks its exact inherited authorities");
    validate_food_plants(*s.seasonal_agriculture,*s.hunting_food,*s.multi_field_agriculture,*s.plantations,s.buildings,s.ticks);
    validate_seasonal_hemp_snapshot(s);
}
void World::validate_seasonal_agriculture_state()const{
    if(!seasonal_agriculture_enabled())return;
    validate_food_plants(*seasonal_agriculture_,*hunting_food_,*multi_field_agriculture_,*plantations_,buildings_,ticks_);
    validate_seasonal_hemp_state();
}
}
