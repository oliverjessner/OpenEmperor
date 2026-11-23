#include "simulation/World.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason){throw std::invalid_argument(reason);}
template<class Rows,class Key> decltype(auto) exact(Rows& rows,BuildingId id,Key key){
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[&](const auto& row,BuildingId target){return key(row)<target;});
    if(at==rows.end()||key(*at)!=id)invalid("Food Warehouse provenance lost its exact paid owner");
    return *at;
}
const auto body_key=[](const auto& b){return b.id;};
const auto stock_key=[](const auto& s){return s.building_id;};
template<class Buildings> std::uint64_t travel(const Buildings& buildings,BuildingId source,BuildingId target){
    const auto& from=exact(buildings,source,body_key);const auto& to=exact(buildings,target,body_key);
    const auto gap=footprint_distance(from.cell,building_footprint(RulesProfile::CityV16,kFoodWarehousesRuleVersion,from.kind),
        to.cell,building_footprint(RulesProfile::CityV16,kFoodWarehousesRuleVersion,to.kind));
    return static_cast<std::uint64_t>(std::max(2,gap))*5;
}
template<class Buildings,class Couriers>
void arrival(const FoodProcessingArrival& a,BuildingId target,std::size_t slot,const ProcessedFoodState& food,
             const Buildings& buildings,const Couriers& couriers,std::uint64_t tick){
    const auto& source=exact(buildings,a.source,body_key);const auto& owner=exact(buildings,target,body_key);
    const auto& stock=exact(food.stocks,target,stock_key);
    const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==a.transfer.courier_id;});
    const auto trip=std::find_if(food.trips.begin(),food.trips.end(),[&](const auto& t){return t.transfer.courier_id==a.transfer.courier_id;});
    const auto expected=source.kind==Object::Warehouse ? CourierRole::OriginalWarehouseFood:
        source.kind==Object::OriginalMill53 ? CourierRole::OriginalMealsToShop:CourierRole::OriginalFoodToMarket;
    if(a.source==target||!a.tick||a.tick>tick||a.tick<=source.placed_tick||a.tick<=owner.placed_tick||a.amount<1||a.amount>4||
       !a.transfer.sequence||a.transfer.sequence>a.tick-source.placed_tick||static_cast<std::uint64_t>(a.amount)>stock.received[slot]||
       a.tick-source.placed_tick<travel(buildings,a.source,target)||a.tick-owner.placed_tick<travel(buildings,a.source,target)||
       actor==couriers.end()||actor->owner!=a.source||actor->role!=expected||actor->good!=Good::Food||
       trip==food.trips.end()||trip->source!=a.source||a.transfer.sequence>trip->transfer.sequence)
        invalid("First Warehouse Food arrival lost its actual courier, paid epoch, amount or physical travel");
    if(trip->transfer==a.transfer&&(trip->phase!=OriginalGoodsTransferPhase::Completed||trip->target!=target||trip->delivered[slot]!=a.amount))
        invalid("First Warehouse Food receipt contradicts its retained accepted trip");
}
template<class Buildings>
void raw_origin(const FoodProcessingArrival& a,BuildingId target,std::size_t g,const ProcessedFoodState& food,const Buildings& buildings){
    const auto& source=exact(buildings,a.source,body_key);const auto distance=travel(buildings,a.source,target);
    if(g<5){
        if(source.kind!=Object::OriginalFoodFarm)invalid("Warehouse raw crop requires its actual paid Farm source");
        bool possible=false;
        for(const auto& field:food.fields)if(field.farm_id==source.id&&static_cast<std::size_t>(field.crop)==g){
            const auto placed=exact(buildings,field.field_id,body_key).placed_tick;
            possible|=a.tick>placed&&a.tick-placed>=83+distance;
        }
        if(!possible)invalid("Warehouse raw receipt predates a compatible paid physical crop harvest");
    }else{
        const auto expected=g==5?Object::OriginalFishingQuay:Object::OriginalHuntersTent;
        const std::uint64_t minimum=g==5?60:80;
        if(source.kind!=expected||a.tick-source.placed_tick<minimum+distance-1)
            invalid("Warehouse Fish or Meat receipt predates real producer work, unload and land movement");
    }
}
template<class Buildings,class Couriers>
void validate_warehouses(const FoodWarehousesState& state,const FoodProcessingState& processing,const ProcessedFoodState& food,
              const Buildings& buildings,const Couriers& couriers,std::uint64_t tick){
    if(state.warehouses.size()>Rules::city_v10_warehouse_limit||state.markets.size()>Rules::city_v11_market_limit||
       std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return b.placed&&b.kind==Object::Warehouse;})!=static_cast<std::ptrdiff_t>(state.warehouses.size())||
       std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return b.placed&&b.kind==Object::Market;})!=static_cast<std::ptrdiff_t>(state.markets.size()))
        invalid("Rule35 must retain exactly one bounded provenance row per paid Warehouse and Market");
    for(std::size_t i=0;i<state.warehouses.size();++i){
        const auto& row=state.warehouses[i];const auto& b=exact(buildings,row.warehouse_id,body_key);const auto& stock=exact(food.stocks,b.id,stock_key);
        if((i&&!(state.warehouses[i-1].warehouse_id<b.id))||!b.placed||b.kind!=Object::Warehouse||b.placed_tick>tick||stock.quality_witness||stock.missed_demands)
            invalid("Food Warehouse history requires a sorted paid non-House owner");
        const auto actors=std::count_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.owner==b.id&&c.role==CourierRole::OriginalWarehouseFood;});
        if(actors!=1)invalid("Every Food Warehouse requires exactly one counted physical Food courier");
        std::int64_t total=0;
        for(std::size_t g=0;g<processed_food_count;++g){
            std::int64_t incoming=0,returning=0;
            for(const auto& t:food.trips){
                const auto remaining=std::int64_t(t.loaded[g])-t.delivered[g]-t.returned[g];
                if(remaining<0)invalid("Warehouse Food cargo history became negative");
                if(t.source==b.id)returning+=remaining;
                if(t.target==b.id&&t.phase==OriginalGoodsTransferPhase::Loaded)incoming+=remaining;
            }
            if(stock.amounts[g]<0||stock.consumed[g]||stock.received[g]>UINT64_MAX-stock.returned[g]||
               stock.departed[g]>UINT64_MAX-static_cast<std::uint64_t>(stock.amounts[g])||
               stock.received[g]+stock.returned[g]!=stock.departed[g]+static_cast<std::uint64_t>(stock.amounts[g])||
               std::int64_t(stock.amounts[g])+incoming+returning>food_warehouse_kind_intake)
                invalid("Warehouse sole Food conservation or per-kind incoming and return capacity invalid");
            total+=stock.amounts[g]+incoming+returning;
            if(bool(stock.received[g])!=row.first_received[g].has_value())invalid("Warehouse reception requires its immutable first actual arrival");
            if(stock.departed[g]<stock.returned[g]||stock.departed[g]-stock.returned[g]<static_cast<std::uint64_t>(returning))
                invalid("Warehouse actual accepted departures cannot be negative");
            const auto accepted=stock.departed[g]-stock.returned[g]-static_cast<std::uint64_t>(returning);
            if(bool(accepted)!=row.first_delivered[g].has_value())invalid("Warehouse accepted outbound Food requires its immutable actual destination receipt");
            if(g==11&&(stock.amounts[g]||stock.received[g]||stock.departed[g]||stock.returned[g]||incoming||returning||row.first_received[g]||row.first_delivered[g]))
                invalid("Unsupported grade5 cannot enter a Warehouse history or compartment");
            if(row.first_received[g]){
                const auto& a=*row.first_received[g];arrival(a,b.id,g,food,buildings,couriers,tick);
                if(g<7)raw_origin(a,b.id,g,food,buildings);
                else{
                    const auto& mill=exact(processing.mills,a.source,[](const auto& m){return m.mill_id;});
                    if(!mill.first_completions[g-7]||a.tick<=mill.first_completions[g-7]->completed_tick||
                       a.tick-mill.first_completions[g-7]->completed_tick<travel(buildings,a.source,b.id)-1)
                        invalid("Warehouse meal arrival predates its actual paid Mill grade completion");
                }
            }
            if(row.first_delivered[g]){
                const auto& a=*row.first_delivered[g];const auto& target=exact(buildings,a.target,body_key);
                if(static_cast<std::uint64_t>(a.amount)>accepted||!row.first_received[g]||a.tick<=row.first_received[g]->tick||
                   a.tick-row.first_received[g]->tick<travel(buildings,b.id,a.target)||
                   (g<7 ? target.kind!=Object::Market&&target.kind!=Object::OriginalMill53:target.kind!=Object::OriginalFoodShop66))
                    invalid("Warehouse accepted delivery lost its legal typed destination or earlier physical input");
                arrival(FoodProcessingArrival{a.transfer,b.id,a.tick,a.amount},a.target,g,food,buildings,couriers,tick);
            }
        }
        if(total>food_warehouse_capacity)invalid("Warehouse shared Food compartment exceeds32 including incoming and physical return reservations");
    }
    for(std::size_t i=0;i<state.markets.size();++i){
        const auto& row=state.markets[i];const auto& b=exact(buildings,row.market_id,body_key);const auto& stock=exact(food.stocks,b.id,stock_key);
        if((i&&!(state.markets[i-1].market_id<b.id))||!b.placed||b.kind!=Object::Market||b.placed_tick>tick)
            invalid("Food Market first receipts require every sorted paid Market owner");
        for(std::size_t g=0;g<7;++g){
            if(bool(stock.received[g])!=row.first_received[g].has_value())invalid("Rule35 raw Market input requires its first actual physical arrival");
            if(!row.first_received[g])continue;
            const auto& a=*row.first_received[g];arrival(a,b.id,g,food,buildings,couriers,tick);
            if(exact(buildings,a.source,body_key).kind==Object::Warehouse){
                const auto& warehouse=exact(state.warehouses,a.source,[](const auto& w){return w.warehouse_id;});
                if(!warehouse.first_received[g]||!warehouse.first_delivered[g]||a.tick<=warehouse.first_received[g]->tick||
                   a.tick-warehouse.first_received[g]->tick<travel(buildings,a.source,b.id)||a.tick<warehouse.first_delivered[g]->tick)
                    invalid("Market raw receipt predates its Warehouse input and actual forwarding");
            }else raw_origin(a,b.id,g,food,buildings);
        }
        for(std::size_t g=7;g<processed_food_count;++g)if(stock.amounts[g]||stock.received[g]||stock.departed[g]||stock.returned[g]||stock.consumed[g])
            invalid("A Market cannot receive, store or distribute processed meals");
    }
    // Current and retained housing-evaluation witnesses still name the real
    // Market courier. Its first input is now an immutable causal lower bound.
    const auto house_witness=[&](BuildingId house,const auto& witness){
        if(!witness)return;
        bool raw=false;for(std::size_t g=0;g<7;++g)raw|=witness->accepted[g]>0;
        if(!raw)return;
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==witness->transfer.courier_id;});
        if(actor==couriers.end()||actor->role!=CourierRole::OriginalFoodToHouse)invalid("Raw House Food requires its actual Market distributor");
        const auto& market=exact(state.markets,actor->owner,[](const auto& m){return m.market_id;});
        for(std::size_t g=0;g<7;++g)if(witness->accepted[g]>0&&(!market.first_received[g]||witness->tick<=market.first_received[g]->tick||
            witness->tick-market.first_received[g]->tick<travel(buildings,market.market_id,house)))
            invalid("House raw Food witness predates the actual Market input and physical outward hop");
    };
    for(const auto& stock:food.stocks)if(original_house_kind(exact(buildings,stock.building_id,body_key).kind))house_witness(stock.building_id,stock.quality_witness);
    for(const auto& evaluation:food.housing_evaluations)house_witness(evaluation.building_id,evaluation.quality_witness);
}
}
World World::create_food_warehouses(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kFoodWarehousesRuleVersion,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_food_warehouses(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    if(s.profile!=RulesProfile::CityV16 || s.rule_version!=kFoodWarehousesRuleVersion)invalid("Food Warehouse restore requires exact Rule35");
    validate_food_warehouses_snapshot(s);validate_food_processing_snapshot(s);
    auto w=restore_irrigation_generation(kFoodWarehousesRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.validate_fishing_context();w.validate_hunting_context();return w;
}
void World::validate_food_warehouses_snapshot(const WorldSnapshot& s){
    if(!simulation::food_warehouses_enabled(s.profile,s.rule_version)){
        if(s.food_warehouses)invalid("Historical generations reject Food Warehouse authority");return;
    }
    if(!s.food_warehouses||!s.food_processing||!s.processed_food)invalid("Rule35 requires exclusive Food Warehouse and processed Food authority");
    validate_warehouses(*s.food_warehouses,*s.food_processing,*s.processed_food,s.buildings,s.couriers,s.ticks);
}
void World::validate_food_warehouses_state() const{
    if(food_warehouses_enabled()){
        if(!food_warehouses_||!food_processing_||!processed_food_)invalid("Rule35 lost its sole Food authority");
        validate_warehouses(*food_warehouses_,*food_processing_,*processed_food_,buildings_,couriers_,ticks_);
    }else if(food_warehouses_)invalid("Historical World rejects Food Warehouse state");
}
FoodWarehouseStatus World::food_warehouse_status(BuildingId id) const{
    FoodWarehouseStatus result;result.building_id=id;
    if(!food_warehouses_||!processed_food_)return result;
    const auto& b=building(id);if(!b.placed||b.kind!=Object::Warehouse)return result;
    result.amounts=exact(processed_food_->stocks,id,stock_key).amounts;
    for(const auto& t:processed_food_->trips){int remaining=0;for(std::size_t g=0;g<processed_food_count;++g)remaining+=t.loaded[g]-t.delivered[g]-t.returned[g];
        if(t.target==id&&t.phase==OriginalGoodsTransferPhase::Loaded)result.incoming+=remaining;
        if(t.source==id)result.return_reserved+=remaining;}
    for(const auto& c:couriers_)if(c.owner==id&&c.role==CourierRole::OriginalWarehouseFood)result.courier=c.id;
    if(!b.operating_enabled)result.blocker="Operation paused";
    else if(building_on_fire(id))result.blocker="On fire";
    else if(!building_staffed_for_tick(id))result.blocker="Full source staffing required";
    else if(building_entrances(id).empty())result.blocker="No road access";
    else{int held=result.incoming+result.return_reserved;for(auto amount:result.amounts)held+=amount;
        result.blocker=held>=result.capacity ? "Full shared Food compartment; waiting for physical delivery or return":"Physical raw and meal distribution";}
    return result;
}
} // namespace openemperor::simulation
