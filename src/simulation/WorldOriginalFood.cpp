#include "simulation/World.h"
#include "simulation/FoodBatchArithmetic.h"
#include "simulation/original/OriginalRulesModel.h"
#include "simulation/original/OriginalScheduler.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
bool meal_role(CourierRole role) {return role==CourierRole::OriginalMealsToShop || role==CourierRole::OriginalMealsToHouse;}
bool food_role(CourierRole role) {return role==CourierRole::OriginalFoodToMarket || role==CourierRole::OriginalFoodToHouse || meal_role(role) || role==CourierRole::OriginalWarehouseFood;}
bool food_source(Object kind,bool fishing=false,bool hunting=false) {return kind==Object::OriginalFoodFarm || (fishing && kind==Object::OriginalFishingQuay) || (hunting && kind==Object::OriginalHuntersTent);}
bool food_owner(Object kind,bool fishing=false,bool hunting=false,bool processing=false,bool warehouses=false) {return food_source(kind,fishing,hunting) || kind==Object::Market || original_house_kind(kind) || (warehouses && kind==Object::Warehouse) || (processing && (kind==Object::OriginalMill53 || kind==Object::OriginalFoodShop66));}
int capacity(Object kind) {return kind==Object::Warehouse ? food_warehouse_capacity:kind==Object::Market || kind==Object::OriginalMill53 || kind==Object::OriginalFoodShop66 ? 16:8;}
std::size_t crop_index(OriginalFoodCrop crop) {
    const auto result=static_cast<std::size_t>(crop);
    if (result>=original_food_crop_count) invalid("unsupported authored Food crop");return result;
}
template<class Amounts> int volume(const Amounts& amounts) {
    return food_batch::volume(amounts);
}
void add(std::uint64_t& target,std::uint64_t n) {
    if (n>UINT64_MAX-target) invalid("Food history sum overflows");target+=n;
}
template<class Trip> auto remaining(const Trip& trip) {
    std::remove_cvref_t<decltype(trip.loaded)> result{};
    for (std::size_t g=0;g<result.size();++g) {
        const auto n=std::int64_t(trip.loaded[g])-trip.delivered[g]-trip.returned[g];
        if (n<0 || n>4) invalid("Food trip remaining cargo outside Cart capacity");result[g]=static_cast<std::int32_t>(n);
    }
    return result;
}
template<class Rows> decltype(auto) account(Rows& rows,BuildingId owner) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),owner,[](const auto& row,BuildingId id){return row.building_id<id;});
    if (at==rows.end() || at->building_id!=owner) invalid("missing sole Food owner account");return *at;
}
template<class Rows> auto trip_for(Rows& rows,CourierId actor) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),actor,[](const auto& row,CourierId id){return row.transfer.courier_id<id;});
    return at==rows.end() || at->transfer.courier_id!=actor ? nullptr:&*at;
}
template<class Rows> decltype(auto) body(Rows& rows,BuildingId id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.id<key;});
    if (at==rows.end() || at->id!=id) invalid("Food reference has no paid building");return *at;
}
template<class Rows> decltype(auto) home(Rows& rows,BuildingId id) {return account(rows,id);}
template<class Food> int incoming(const Food& food,BuildingId target) {
    int result=0;for (const auto& trip:food.trips) if (trip.target==target && trip.phase==OriginalGoodsTransferPhase::Loaded) result+=volume(remaining(trip));
    return result;
}
template<class Food> int return_room(const Food& food,BuildingId source) {
    int result=0;for (const auto& trip:food.trips) if (trip.source==source) result+=volume(remaining(trip));return result;
}
std::uint64_t events(std::uint64_t tick) {
    return (tick>=40 ? 1+(tick-40)/816:0)+(tick>=397 ? 1+(tick-397)/816:0);
}
bool adjacent(Cell farm,Cell field) {
    const std::int64_t x=farm.x,y=farm.y,fx=field.x,fy=field.y;
    return std::max({x-fx,std::int64_t{0},fx-(x+2)})+std::max({y-fy,std::int64_t{0},fy-(y+2)})==1;
}
template<class Amounts> Amounts portions(const Amounts& source,int count) {
    if (count<0) invalid("negative Food load");
    return food_batch::mixed_portions(source,static_cast<std::uint64_t>(count));
}
template<class Amounts> Amounts consumed_portions(const Amounts& source,std::uint64_t count) {
    if constexpr(std::tuple_size_v<Amounts>!=processed_food_count)return food_batch::prefix_portions(source,count);
    else {
        Amounts result{};
        const auto take=[&](std::size_t slot){const auto amount=std::min(count,static_cast<std::uint64_t>(source[slot]));
            result[slot]=static_cast<std::int32_t>(amount);count-=amount;};
        for(std::size_t slot=processed_food_first_meal;slot<source.size();++slot)take(slot);
        for(std::size_t slot=0;slot<processed_food_first_meal;++slot)take(slot);
        return result;
    }
}
template<class Food> int incoming_raw(const Food& food,BuildingId target) {
    int result=0;
    for(const auto& trip:food.trips)if(trip.target==target&&trip.phase==OriginalGoodsTransferPhase::Loaded) {
        const auto cargo=remaining(trip);for(std::size_t g=0;g<std::min(cargo.size(),processed_food_first_meal);++g)result+=cargo[g];
    }
    return result;
}
template<class Food> auto available_load(const Food& food,BuildingId owner,BuildingId target,Object target_kind,CourierRole role,bool warehouses=false) {
    auto result=account(food.stocks,owner).amounts;
    if constexpr(std::is_same_v<Food,ProcessedFoodState>) {
        for(std::size_t g=0;g<result.size();++g) {
            if(role==CourierRole::OriginalWarehouseFood) {
                if(target_kind!=Object::Empty && ((g>=processed_food_first_meal)!=(target_kind==Object::OriginalFoodShop66)))result[g]=0;
            } else if((g>=processed_food_first_meal)!=meal_role(role))result[g]=0;
        }
        if(target_kind==Object::OriginalMill53)for(std::size_t g=0;g<processed_food_first_meal;++g) {
            int reserved=0;for(const auto& trip:food.trips)if(trip.target==target&&trip.phase==OriginalGoodsTransferPhase::Loaded)reserved+=remaining(trip)[g];
            result[g]=std::min(result[g],std::max(0,2-account(food.stocks,target).amounts[g]-reserved));
        }
        if(warehouses && target_kind==Object::Warehouse)for(std::size_t g=0;g<result.size();++g) {
            int reserved=0;
            for(const auto& trip:food.trips) {
                if(trip.target==target && trip.phase==OriginalGoodsTransferPhase::Loaded)reserved+=remaining(trip)[g];
                if(trip.source==target)reserved+=remaining(trip)[g];
            }
            result[g]=std::min(result[g],std::max(0,food_warehouse_kind_intake-account(food.stocks,target).amounts[g]-reserved));
        }
    }
    return result;
}
const MillProcessing& mill_record(const FoodProcessingState* processing,BuildingId id) {
    if(!processing)invalid("Meal origin lost its sole paid processing history");
    const auto at=std::lower_bound(processing->mills.begin(),processing->mills.end(),id,[](const auto& row,BuildingId key){return row.mill_id<key;});
    if(at==processing->mills.end()||at->mill_id!=id)invalid("Meal origin has no exact paid Mill");return *at;
}
std::uint64_t meal_origin(const FoodProcessingState* processing,BuildingId id,std::size_t slot) {
    if(slot<processed_food_first_meal||slot>=processed_food_first_meal+4)return 0;
    const auto grade=slot-processed_food_first_meal;const auto count=mill_record(processing,id).completed_by_grade[grade];
    if(count>UINT64_MAX/(grade+1))invalid("Meal completion origin overflows");return count*(grade+1);
}
int processing_reservation(const FoodProcessingState* processing,BuildingId id) {
    if(!processing)return 0;
    for(const auto& row:processing->mills)if(row.mill_id==id)return row.active?row.active->grade:0;
    return 0;
}
template<class Food,class Trip> void return_load(Food& food,Trip& trip) {
    const auto cargo=remaining(trip);auto& source=account(food.stocks,trip.source);
    for (std::size_t g=0;g<cargo.size();++g) {
        source.amounts[g]+=cargo[g];add(source.returned[g],static_cast<std::uint64_t>(cargo[g]));trip.returned[g]+=cargo[g];
    }
    trip.phase=OriginalGoodsTransferPhase::Returned;
}
bool agriculture_field(Object kind) {
    return kind==Object::OriginalWheatField || kind==Object::OriginalMilletField ||
        kind==Object::OriginalRiceField || kind==Object::OriginalCabbageField || kind==Object::OriginalSoybeanField;
}
Object field_kind(AgricultureCrop crop) {
    constexpr std::array kinds{Object::OriginalWheatField,Object::OriginalMilletField,Object::OriginalRiceField,Object::OriginalCabbageField,Object::OriginalSoybeanField};
    const auto g=static_cast<std::size_t>(crop);if(g>=kinds.size()) invalid("unknown Agriculture crop");return kinds[g];
}
bool work_area(Cell farm,Cell field) {
    return std::int64_t(field.x)>=std::int64_t(farm.x)-3 && std::int64_t(field.x)<=std::int64_t(farm.x)+5 &&
        std::int64_t(field.y)>=std::int64_t(farm.y)-3 && std::int64_t(field.y)<=std::int64_t(farm.y)+5;
}
FoodBatchQuality batch_quality(const OriginalFoodAmounts& a) {return original_food_batch_quality(a);}
FoodBatchQuality batch_quality(const AgricultureAmounts& a) {return agriculture_batch_quality(a);}
FoodBatchQuality batch_quality(const FishingFoodAmounts& a) {return agriculture_batch_quality(a);}
FoodBatchQuality batch_quality(const HuntingFoodAmounts& a) {return agriculture_batch_quality(a);}
FoodBatchQuality batch_quality(const ProcessedFoodAmounts& a) {return agriculture_batch_quality(a);}
template<class Amounts> Amounts quality_aware_target_raw_load(const Amounts& available,int space,
        const original::HouseState& target,const original::HouseEvolutionRules& rules) {
    const auto limit=std::min({4,volume(available),space});
    if constexpr(std::is_same_v<Amounts,ProcessedFoodAmounts>)
        return quality_aware_raw_food_load(available,limit,rules.value(original::house_row(target.type),original::HouseField::FoodQuality));
    else return portions(available,limit);
}
std::uint64_t fish_origin(const FishingState* state,BuildingId quay) {
    if(!state) invalid("Fish origin requires its sole Fishing authority");
    const auto row=std::find_if(state->quays.begin(),state->quays.end(),[&](const auto& value){return value.quay_id==quay;});
    if(row==state->quays.end()) invalid("Fish source lost its paid Quay identity");
    return row->unloaded;
}
int boat_return_room(const FishingState* state,BuildingId quay) {
    int result=0;
    if(state) for(const auto& boat:state->boats) if(boat.quay_id==quay) {
        if(boat.reserved<0 || boat.reserved>fishing_boat_capacity || result>8-boat.reserved)
            invalid("Boat return reservation exceeds finite Quay capacity");
        result+=boat.reserved;
    }
    return result;
}
std::uint64_t meat_origin(const HuntingState* state,BuildingId tent) {
    if(!state) invalid("Meat origin requires its sole Hunting authority");
    const auto row=std::find_if(state->tents.begin(),state->tents.end(),[&](const auto& value){return value.tent_id==tent;});
    if(row==state->tents.end()) invalid("Meat source lost its paid Tent identity");
    return row->unloaded;
}
int hunter_return_room(const HuntingState* state,BuildingId tent) {
    int result=0;
    if(state) for(const auto& actor:state->hunters) if(actor.tent_id==tent) {
        if(actor.reserved<0 || actor.reserved>1 || result>8-actor.reserved)
            invalid("Hunter return reservation exceeds finite Tent capacity");
        result+=actor.reserved;
    }
    return result;
}
template<class Buildings,class Food> void validate_multi_field_work(const MultiFieldAgricultureState& state,const Food& food,
                              const Buildings& buildings,std::uint64_t ticks,const SeasonalAgricultureState* seasonal=nullptr,const IrrigationProductivityState* irrigation=nullptr,const AgricultureReconfigurationState* reconfiguration=nullptr) {
    if(state.farms.size()!=food.farms.size() || state.fields.size()!=food.fields.size())
        invalid("Multi-field work must bind every paid Farm and Field exactly once");
    for(std::size_t i=0;i<state.fields.size();++i) {
        const auto& work=state.fields[i];const auto& field=food.fields[i];
        const auto& paid=body(buildings,field.field_id);
        const auto through=agriculture_retirement_tick(reconfiguration,field.field_id,ticks);
        const auto* retired=agriculture_reconfiguration_entry(reconfiguration,field.field_id);
        if(through>ticks||through<paid.placed_tick||(!paid.placed&&(!retired||!retired->removed_tick)))invalid("Field work lacks closed retirement chronology");
        if(work.field_id!=field.field_id || work.farm_id!=field.farm_id || paid.placed_tick>through ||
           work.productive_ticks>through-paid.placed_tick || (!seasonal && field.harvested!=work.productive_ticks/multi_field_work_per_portion))
            invalid("Multi-field work lost paid identity, age or sole harvest provenance");
        if(seasonal) {
            const auto at=std::lower_bound(seasonal->fields.begin(),seasonal->fields.end(),work.field_id,[](const auto& value,BuildingId id){return value.field_id<id;});
            if(at==seasonal->fields.end()||at->field_id!=work.field_id||at->wasted_work>irrigation_earned_work(work.productive_ticks,irrigation,work.field_id))
                invalid("Seasonal Field lost its exact wasted-work history");
            const auto net=irrigation_earned_work(work.productive_ticks,irrigation,work.field_id)-at->wasted_work;
            if(field.harvested>net/80||net-field.harvested*80>80||(!paid.placed&&(net!=field.harvested*80||at->ready_tick||at->harvest_progress)))invalid("Seasonal work/harvest/pending-cycle equation invalid");
        }
    }
    for(std::size_t i=0;i<state.farms.size();++i) {
        const auto& schedule=state.farms[i];
        if(schedule.farm_id!=food.farms[i].farm_id || body(buildings,schedule.farm_id).progress)
            invalid("Multi-field Farm schedule identity or legacy progress invalid");
        std::size_t count=0;bool worked=false,cursor=false;
        for(const auto& work:state.fields) if(work.farm_id==schedule.farm_id) {
            count+=!reconfiguration||body(buildings,work.field_id).placed;worked|=work.productive_ticks!=0;
            cursor|=schedule.last_field==work.field_id && work.productive_ticks!=0;
            // Work released by a later Field cannot borrow the Farm's earlier
            // idle age. Every release-time suffix shares one work unit/tick.
            const auto epoch=body(buildings,work.field_id).placed_tick;
            std::uint64_t suffix=0;
            for(const auto& peer:state.fields) if(peer.farm_id==schedule.farm_id &&
                body(buildings,peer.field_id).placed_tick>=epoch) add(suffix,peer.productive_ticks);
            if(suffix>ticks-epoch) invalid("Multi-field work exceeds shared Farm labor after paid Field release");
        }
        if(count>multi_field_per_farm_limit || (worked ? !cursor:schedule.last_field.has_value()))
            invalid("Multi-field Farm quota or real last-work cursor invalid");
    }
}
std::uint64_t crop_origin(const AgricultureField& field,const PhysicalFoodGrowersState* physical) {
    if(!physical)return field.harvested;
    const auto at=std::lower_bound(physical->fields.begin(),physical->fields.end(),field.field_id,[](const auto& row,BuildingId id){return row.field_id<id;});
    if(at==physical->fields.end()||at->field_id!=field.field_id||at->farm_id!=field.farm_id)invalid("Food origin lost its actual Farm return identity");
    return at->returned;
}
template<class Food,class Buildings,class Couriers,class Homes>
void validate_food(const Food& food,const Buildings& buildings,const Couriers& couriers,const Homes& houses,
              std::uint64_t ticks,RulesProfile profile,std::uint32_t rule,bool bind_movement=true,bool check_progress=true,
              const FishingState* fishing_context=nullptr,const HuntingState* hunting_context=nullptr,
              const MultiFieldAgricultureState* multi_context=nullptr,const PhysicalFoodGrowersState* physical_context=nullptr,const SeasonalAgricultureState* seasonal_context=nullptr,const IrrigationProductivityState* irrigation_context=nullptr,const FoodProcessingState* processing_context=nullptr,const AgricultureReconfigurationState* reconfiguration_context=nullptr) {
    if(simulation::physical_food_growers_enabled(profile,rule) ? !physical_context:physical_context!=nullptr)invalid("Physical Food return authority requires exact Rule31");
    using Amounts=std::remove_cvref_t<decltype(food.stocks.front().amounts)>;
    using Counters=std::remove_cvref_t<decltype(food.stocks.front().received)>;
    constexpr bool agriculture=!std::is_same_v<Food,OriginalFoodState>;
    constexpr bool processing=std::is_same_v<Food,ProcessedFoodState>;
    const bool warehouses=food_warehouses_enabled(profile,rule);
    constexpr bool hunting=std::is_same_v<Food,HuntingFoodState> || processing;
    constexpr bool fishing=std::is_same_v<Food,FishingFoodState> || hunting;
    if(simulation::multi_field_agriculture_enabled(profile,rule)) {
        if constexpr(hunting) {
            if(!multi_context) invalid("Multi-field generation requires its independent work authority");
            validate_multi_field_work(*multi_context,food,buildings,ticks,seasonal_context,irrigation_context,reconfiguration_context);
        } else invalid("Multi-field generation requires sole seven-kind Food authority");
    } else if(multi_context) invalid("Historical Food rejects multi-field work authority");
    if constexpr(hunting) {
        if(!playable_hunting_enabled(profile,rule) || !hunting_context) invalid("Seven-kind Food requires exact Hunting generation");
    } else if(hunting_context) invalid("Historical Food cannot bind Hunting authority");
    if constexpr(fishing) {
        if(!playable_fishing_enabled(profile,rule) || !fishing_context) invalid("Six-kind Food requires exact Fishing generation");
    } else if(fishing_context) invalid("Historical Food cannot bind Fishing authority");
    if(processing ? !food_processing_enabled(profile,rule)||!processing_context:processing_context!=nullptr)
        invalid("Processed Food requires its exact sole Rule34 authority");
    const auto farm_limit=original_food_farm_limit(profile,rule);
    const auto trip_limit=original_food_trip_limit(profile,rule);
    if (food.farms.size()>farm_limit || food.stocks.size()>original_food_stock_limit(profile,rule) || food.trips.size()>trip_limit)
        invalid("bounded Food authority exceeded");
    std::size_t owners=0,farms=0,fields=0,actors=0;
    for (const auto& b:buildings) {owners+=food_owner(b.kind,fishing,hunting,processing,warehouses);farms+=b.kind==Object::OriginalFoodFarm;
        fields+=agriculture ? agriculture_field(b.kind):(b.kind==Object::OriginalWheatField || b.kind==Object::OriginalMilletField);}
    for (const auto& c:couriers) actors+=food_role(c.role);
    if (owners!=food.stocks.size() || farms!=food.farms.size() || fields>(agriculture ? agriculture_field_limit:farm_limit) || actors>trip_limit) invalid("missing Food purchase authority");
    std::array<BuildingId,3> assigned_fields{};std::size_t assigned=0;
    Counters harvest{},all_stock{},all_consumed{},all_departed{},all_received{},all_returned{},all_cargo{};
    if constexpr(fishing) for(const auto& quay:fishing_context->quays) add(harvest[fishing_food_index],quay.unloaded);
    if constexpr(hunting) for(const auto& tent:hunting_context->tents) add(harvest[hunting_food_index],tent.unloaded);
    if constexpr(processing)for(const auto& mill:processing_context->mills) {
        if(body(buildings,mill.mill_id).kind!=Object::OriginalMill53)invalid("Meal origin is not a paid Mill");
        for(std::size_t g=processed_food_first_meal;g<harvest.size();++g)add(harvest[g],meal_origin(processing_context,mill.mill_id,g));
    }
    if constexpr(processing)for(const auto& row:food.housing_evaluations) {
        if(row.received[processed_food_count-1]||row.consumed[processed_food_count-1]||
           (row.quality_witness&&(row.quality_witness->accepted[processed_food_count-1]||row.quality_witness->consumed_before[processed_food_count-1])))
            invalid("Historical House evaluation contains unavailable grade5 meals");
    }
    if constexpr (agriculture) {
        const bool authored_food=simulation::original_plantations_enabled(profile,rule);
        if(food.fields.size()!=fields) invalid("Agriculture field count differs from paid bodies");
        for(std::size_t i=0;i<food.farms.size();++i) {
            const auto& row=food.farms[i];const auto& b=body(buildings,row.farm_id);
            if(b.kind!=Object::OriginalFoodFarm || b.progress<0 || b.progress>=80 || (!authored_food && b.progress) ||
                (i && !(food.farms[i-1].farm_id<row.farm_id)))
                invalid("Agriculture Farm identity/progress invalid");
            const auto productive=std::count_if(food.fields.begin(),food.fields.end(),[&](const auto& field) {
                return field.farm_id==row.farm_id && (!reconfiguration_context||body(buildings,field.field_id).placed) && authored_agriculture_crop_enabled(profile,rule,field.crop);
            });
            if(simulation::multi_field_agriculture_enabled(profile,rule) ?
                (productive>static_cast<std::ptrdiff_t>(multi_field_per_farm_limit) || b.progress) :
                (authored_food && (productive>1 || (!productive && b.progress))))
                invalid("AUTHORED adapter requires one paid productive Field per Farm");
        }
        for(std::size_t i=0;i<food.fields.size();++i) {
            const auto& field=food.fields[i];const auto& b=body(buildings,field.field_id);const auto& farm=body(buildings,field.farm_id);
            const auto g=static_cast<std::size_t>(field.crop);
            if(g>=agriculture_crop_count || b.kind!=field_kind(field.crop) || farm.kind!=Object::OriginalFoodFarm ||
                !(farm.id<b.id) || farm.placed_tick>b.placed_tick || b.placed_tick>ticks || !work_area(farm.cell,b.cell) ||
                (i && !(food.fields[i-1].field_id<field.field_id)) ||
                std::none_of(food.farms.begin(),food.farms.end(),[&](const auto& f){return f.farm_id==farm.id;}))
                invalid("Agriculture field lost its paid compatible Farm or work area");
            const bool productive=authored_agriculture_crop_enabled(profile,rule,field.crop);
            if(field.quantity || field.productive_events || (!productive && field.harvested) || field.last_harvest_cycle)
                invalid("Agriculture production requires fully verified Original tending/harvest timing");
            if(productive) {
                const auto through=agriculture_retirement_tick(reconfiguration_context,field.field_id,ticks);
                if(through>ticks||through<b.placed_tick)invalid("Food Field retirement age invalid");
                const auto age=through-b.placed_tick;
                if(irrigated_harvest_enabled(profile,rule)||quality_aware_food_dispatch_enabled(profile,rule)) {
                    // Rule37 admits only growth actually earned by the existing
                    // paid Field histories. Wall-clock potential is no authority.
                    if(!multi_context||!seasonal_context||!irrigation_context)
                        invalid("Irrigated harvest requires complete actual earned-work histories");
                    const auto work=std::lower_bound(multi_context->fields.begin(),multi_context->fields.end(),field.field_id,
                        [](const auto& value,BuildingId id){return value.field_id<id;});
                    const auto season=std::lower_bound(seasonal_context->fields.begin(),seasonal_context->fields.end(),field.field_id,
                        [](const auto& value,BuildingId id){return value.field_id<id;});
                    if(work==multi_context->fields.end()||work->field_id!=field.field_id||work->farm_id!=field.farm_id||
                        season==seasonal_context->fields.end()||season->field_id!=field.field_id||work->productive_ticks>age)
                        invalid("Irrigated harvest lost its paid actual Field work");
                    const auto earned=irrigation_earned_work(work->productive_ticks,irrigation_context,field.field_id);
                    if(season->wasted_work>earned)invalid("Irrigated harvest waste exceeds actual earned growth");
                    const auto net=earned-season->wasted_work;
                    if(field.harvested>net/80||net-field.harvested*80>80||(check_progress&&farm.progress))
                        invalid("Irrigated harvest exceeds actual earned growth");
                } else if(field.harvested>age/80 || (check_progress && field.harvested==age/80 &&
                          static_cast<std::uint64_t>(farm.progress)>age%80))
                    invalid("AUTHORED harvest exceeds its paid80-tick opportunities");
            }
            add(harvest[g],crop_origin(field,physical_context));
        }
    } else {
    for (std::size_t i=0;i<food.farms.size();++i) {
        const auto& f=food.farms[i];const auto g=crop_index(f.crop);const auto& b=body(buildings,f.farm_id);
        if (b.kind!=Object::OriginalFoodFarm || (i && !(food.farms[i-1].farm_id<f.farm_id)) || b.placed_tick>ticks)
            invalid("Food Farm must bind its exact sorted paid body");
        if (b.cell.x<0 || b.cell.y<0 || b.cell.x>228-3 || b.cell.y>228-3) invalid("Food Farm coordinates outside bounded domain");
        if (b.progress<0 || b.progress>=80) invalid("Food crop progress outside80 productive ticks");
        if (!f.field_id) {
            if (f.harvested || b.progress || f.crop!=OriginalFoodCrop::Wheat) invalid("Food origin or crop selected before separately paid Field");
        } else {
            const auto& field=body(buildings,*f.field_id);
            const auto expected=g==0 ? Object::OriginalWheatField:Object::OriginalMilletField;
            if (field.kind!=expected || !(field.id>b.id) || field.placed_tick<b.placed_tick || field.placed_tick>ticks ||
                !adjacent(b.cell,field.cell) || std::find(assigned_fields.begin(),assigned_fields.begin()+static_cast<std::ptrdiff_t>(assigned),field.id)!=assigned_fields.begin()+static_cast<std::ptrdiff_t>(assigned))
                invalid("Food Field must be uniquely paid, adjacent and match its selected crop");
            assigned_fields[assigned++]=field.id;
            const auto age=ticks-field.placed_tick;
            if (f.harvested>age/80 || (check_progress && f.harvested==age/80 && static_cast<std::uint64_t>(b.progress)>age%80))
                invalid("Food harvest exceeds productive opportunities after paid Field");
        }
        add(harvest[g],f.harvested);
    }
    if (assigned!=fields) invalid("orphan Food Field");
    }
    for (std::size_t i=0;i<food.trips.size();++i) {
        const auto& trip=food.trips[i];
        if ((i && !(food.trips[i-1].transfer.courier_id<trip.transfer.courier_id)) || !trip.transfer.sequence || trip.transfer.sequence>ticks)
            invalid("Food trips must retain one bounded latest sequence per actor");
        const auto c=std::find_if(couriers.begin(),couriers.end(),[&](const auto& value){return value.id==trip.transfer.courier_id;});
        if (c==couriers.end() || !food_role(c->role) || c->owner!=trip.source || trip.source==trip.target ||
            (bind_movement && (c->target!=trip.target || c->last_dispatched_target!=trip.target)))
            invalid("Food transaction lost actual actor/source/target binding");
        const auto& source=body(buildings,trip.source);const auto& target=body(buildings,trip.target);
        const bool raw_to_market=c->role==CourierRole::OriginalFoodToMarket&&food_source(source.kind,fishing,hunting)&&
            (target.kind==Object::Market||(processing&&target.kind==Object::OriginalMill53)||(warehouses&&target.kind==Object::Warehouse));
        const bool raw_to_house=c->role==CourierRole::OriginalFoodToHouse&&source.kind==Object::Market&&original_house_kind(target.kind);
        const bool meals_to_shop=processing&&c->role==CourierRole::OriginalMealsToShop&&source.kind==Object::OriginalMill53&&
            (target.kind==Object::OriginalFoodShop66||(warehouses&&target.kind==Object::Warehouse));
        const bool meals_to_house=processing&&c->role==CourierRole::OriginalMealsToHouse&&source.kind==Object::OriginalFoodShop66&&original_house_kind(target.kind);
        const bool warehouse_leg=warehouses&&c->role==CourierRole::OriginalWarehouseFood&&source.kind==Object::Warehouse&&
            (target.kind==Object::OriginalMill53||target.kind==Object::Market||target.kind==Object::OriginalFoodShop66);
        if(!raw_to_market&&!raw_to_house&&!meals_to_shop&&!meals_to_house&&!warehouse_leg)invalid("Food actor has incompatible owner or target");
        if constexpr(processing)for(std::size_t g=0;g<trip.loaded.size();++g) {
            const bool meals=warehouse_leg ? target.kind==Object::OriginalFoodShop66:meal_role(c->role);
            if(((g>=processed_food_first_meal)!=meals)&&
                (trip.loaded[g]||trip.delivered[g]||trip.returned[g]))invalid("Food cargo changed ingredient/graded-meal identity");
            if(g==processed_food_count-1&&(trip.loaded[g]||trip.delivered[g]||trip.returned[g]))invalid("Grade5 meals require an unavailable real supplement contract");
        }
        if (original_house_kind(target.kind) && home(houses,target.id).house.occupants<=0)
            invalid("Food targeted dispatch requires a genuinely occupied House");
        const auto loaded=volume(trip.loaded);(void)volume(trip.delivered);(void)volume(trip.returned);
        if (loaded<=0 || loaded>4) invalid("Food trip must load positive actual Cart volume");
        const bool complete=trip.phase==OriginalGoodsTransferPhase::Completed,returned=trip.phase==OriginalGoodsTransferPhase::Returned;
        const bool active=trip.phase==OriginalGoodsTransferPhase::Loaded || trip.phase==OriginalGoodsTransferPhase::Returning;
        if (!complete && !returned && !active) invalid("unsupported Food transaction phase");
        if (trip.delivered!=(complete ? trip.loaded:Amounts{}) || trip.returned!=(returned ? trip.loaded:Amounts{}))
            invalid("Food World arrivals and returns must book the complete actual load atomically");
        const auto cargo=remaining(trip);for (std::size_t g=0;g<cargo.size();++g) add(all_cargo[g],static_cast<std::uint64_t>(cargo[g]));
        if (bind_movement) {
            if (c->phase==CourierPhase::ToWarehouse ? trip.phase!=OriginalGoodsTransferPhase::Loaded:
                c->phase==CourierPhase::Returning ? !(complete || trip.phase==OriginalGoodsTransferPhase::Returning):!(complete || returned))
                invalid("Food remaining Cargo and real movement phase disagree");
        }
        const auto& from=account(food.stocks,trip.source);const auto& to=account(food.stocks,trip.target);
        for (std::size_t g=0;g<cargo.size();++g)
            if (from.departed[g]<static_cast<std::uint64_t>(trip.loaded[g]) || from.returned[g]<static_cast<std::uint64_t>(trip.returned[g]) || to.received[g]<static_cast<std::uint64_t>(trip.delivered[g]))
                invalid("Food latest trip exceeds actual cumulative owner history");
        if constexpr (!agriculture) {
        if (source.kind==Object::OriginalFoodFarm) {
            const auto farm=std::find_if(food.farms.begin(),food.farms.end(),[&](const auto& f){return f.farm_id==source.id;});
            if (farm==food.farms.end()) invalid("Food source trip has no paid Farm");
            const auto crop=crop_index(farm->crop);
            if (static_cast<std::uint64_t>(trip.loaded[crop])>farm->harvested)
                invalid("Food Farm latest load exceeds its own selected crop harvest");
            for (std::size_t g=0;g<trip.loaded.size();++g) if (g!=crop && trip.loaded[g])
                invalid("Food Farm latest load exceeds its own selected crop harvest");
        } else for (std::size_t g=0;g<cargo.size();++g)
            if (static_cast<std::uint64_t>(trip.loaded[g])>from.received[g]) invalid("Market Food load has no possible actual received origin");
        } else for(std::size_t g=0;g<cargo.size();++g) {
            std::uint64_t origin=0;
            if(processing&&source.kind==Object::OriginalMill53)origin=meal_origin(processing_context,source.id,g);
            else if(source.kind==Object::OriginalFoodFarm) {
                for(const auto& field:food.fields) if(field.farm_id==source.id && static_cast<std::size_t>(field.crop)==g) add(origin,crop_origin(field,physical_context));
            } else if constexpr(fishing) {
                if(source.kind==Object::OriginalFishingQuay) origin=g==fishing_food_index ? fish_origin(fishing_context,source.id):0;
                else if constexpr(hunting) origin=source.kind==Object::OriginalHuntersTent ? (g==hunting_food_index ? meat_origin(hunting_context,source.id):0):from.received[g];
                else origin=from.received[g];
            } else origin=from.received[g];
            if(static_cast<std::uint64_t>(trip.loaded[g])>origin) invalid("Agriculture load exceeds physical crop origin");
        }

    }
    for (const auto& c:couriers) if (food_role(c.role)) {
        if (c.cargo || c.reserved || c.good!=Good::Food) invalid("Food cargo mirrored into legacy actor fields");
        const auto* trip=trip_for(food.trips,c.id);
        if (!trip && (c.phase!=CourierPhase::IdleAtWorkshop || c.last_dispatched_target || c.target!=c.owner)) invalid("used Food actor lost its sequence highwater");
    }
    for (std::size_t i=0;i<food.stocks.size();++i) {
        const auto& stock=food.stocks[i];const auto& b=body(buildings,stock.building_id);
        if (!food_owner(b.kind,fishing,hunting,processing,warehouses) || (i && !(food.stocks[i-1].building_id<stock.building_id))) invalid("Food accounts require sorted unique purchased owners");
        const auto amount=volume(stock.amounts);
        const auto boat_reserved=(fishing ? boat_return_room(fishing_context,b.id):0)+(hunting ? hunter_return_room(hunting_context,b.id):0)+physical_food_grower_reservations(physical_context,b.id);
        if (std::int64_t(amount)+incoming(food,b.id)+return_room(food,b.id)+boat_reserved+processing_reservation(processing_context,b.id)>capacity(b.kind)) invalid("Food stock, incoming and source return-room exceed physical volume");
        if(warehouses && b.kind==Object::Warehouse)for(std::size_t g=0;g<stock.amounts.size();++g) {
            int claimed=stock.amounts[g];
            for(const auto& trip:food.trips) {
                if(trip.target==b.id && trip.phase==OriginalGoodsTransferPhase::Loaded)claimed+=remaining(trip)[g];
                if(trip.source==b.id)claimed+=remaining(trip)[g];
            }
            if(claimed>food_warehouse_kind_intake)invalid("Warehouse per-kind Food intake and return room exceeded");
            if(stock.consumed[g])invalid("Warehouse cannot consume Food or create a House witness");
        }
        Counters own_harvest{};
        if (b.kind==Object::OriginalFoodFarm) {
            if constexpr (agriculture) {
                for(const auto& field:food.fields) if(field.farm_id==b.id) add(own_harvest[static_cast<std::size_t>(field.crop)],crop_origin(field,physical_context));
            } else {
            const auto farm=std::find_if(food.farms.begin(),food.farms.end(),[&](const auto& f){return f.farm_id==b.id;});
            if (farm==food.farms.end()) invalid("Food stock has no paid Farm record");own_harvest[crop_index(farm->crop)]=farm->harvested;
            for (std::size_t g=0;g<stock.amounts.size();++g) if (g!=crop_index(farm->crop) &&
                    (stock.amounts[g] || stock.departed[g] || stock.returned[g]))
                invalid("Food Farm never owned the other immutable paid Field crop");
            if (check_progress && std::int64_t(amount)+return_room(food,b.id)==8 && b.progress) invalid("full Food source retains crop progress");
            }

        }
        if constexpr(fishing) if(b.kind==Object::OriginalFishingQuay)
            own_harvest[fishing_food_index]=fish_origin(fishing_context,b.id);
        if constexpr(hunting) if(b.kind==Object::OriginalHuntersTent)
            own_harvest[hunting_food_index]=meat_origin(hunting_context,b.id);
        if constexpr(processing) {
            if(b.kind==Object::OriginalMill53)for(std::size_t g=processed_food_first_meal;g<own_harvest.size();++g)
                own_harvest[g]=meal_origin(processing_context,b.id,g);
            int raw_held=0;for(std::size_t g=0;g<processed_food_first_meal;++g)raw_held+=stock.amounts[g];
            if(original_house_kind(b.kind)&&raw_held+incoming_raw(food,b.id)>4)invalid("Raw Food displaced reserved House meal capacity");
            if(b.kind==Object::OriginalMill53)for(std::size_t g=0;g<processed_food_first_meal;++g) {
                int pending=0;for(const auto& trip:food.trips)if(trip.target==b.id&&trip.phase==OriginalGoodsTransferPhase::Loaded)pending+=remaining(trip)[g];
                if(stock.amounts[g]+pending>2)invalid("Mill ingredient reservations exceeded their finite per-kind intake");
            }
        }
        for (std::size_t g=0;g<stock.amounts.size();++g) {
            auto input=own_harvest[g];add(input,stock.received[g]);add(input,stock.returned[g]);
            auto output=static_cast<std::uint64_t>(stock.amounts[g]);add(output,stock.departed[g]);add(output,stock.consumed[g]);
            if (input!=output || stock.returned[g]>stock.departed[g]) invalid("Food owner harvest+received+returned differs from stock+departed+consumed");
            if (food_source(b.kind,fishing,hunting) && (stock.received[g] || stock.consumed[g])) invalid("Food Farm received or consumed unearned crop");
            if (b.kind==Object::Market && stock.consumed[g]) invalid("Market cannot consume household Food");
            if constexpr(processing) {
                const bool meals=g>=processed_food_first_meal;
                if((g==processed_food_count-1 || (food_source(b.kind,fishing,hunting)&&meals) || (b.kind==Object::Market&&meals) ||
                    (b.kind==Object::OriginalFoodShop66&&!meals)) &&
                   (stock.amounts[g]||stock.received[g]||stock.departed[g]||stock.returned[g]||stock.consumed[g]))
                    invalid("Processed Food account admits a foreign or unavailable grade identity");
                if(b.kind==Object::OriginalFoodShop66&&stock.consumed[g])invalid("Food Shop cannot consume House meals");
                if(b.kind==Object::OriginalMill53) {
                    if(meals&&(stock.received[g]||stock.consumed[g]))invalid("Mill meals require actual processing rather than receipt or reprocessing");
                    if(!meals) {
                        const auto& mill=mill_record(processing_context,b.id);std::uint64_t debited=0;
                        for(const auto& grade:mill.ingredients_consumed)add(debited,grade[g]);
                        if(stock.consumed[g]!=debited||stock.departed[g]||stock.returned[g])invalid("Mill raw debits contradict the sole recipe history");
                    }
                }
            }
            if (original_house_kind(b.kind) && (stock.departed[g] || stock.returned[g])) invalid("House Food cannot depart or return");
            add(all_stock[g],static_cast<std::uint64_t>(stock.amounts[g]));add(all_consumed[g],stock.consumed[g]);
            add(all_departed[g],stock.departed[g]);add(all_received[g],stock.received[g]);add(all_returned[g],stock.returned[g]);
        }
        if (!original_house_kind(b.kind)) {
            if (stock.missed_demands || stock.quality_witness) invalid("only actual Houses own Food demand and quality witnesses");
            const auto c=std::find_if(couriers.begin(),couriers.end(),[&](const auto& actor){return actor.owner==b.id && food_role(actor.role);});
            if (c==couriers.end()) invalid("paid Food source lost its real courier");const auto* trip=trip_for(food.trips,c->id);
            const auto departed=food_batch::history_total(stock.departed);
            if (!trip ? departed!=0:departed<trip->transfer.sequence || departed/4+(departed%4!=0)>trip->transfer.sequence)
                invalid("Food dispatch history exceeds its actual positive Cart sequences");
        } else {
            const auto& h=home(houses,b.id);const auto age=ticks-b.placed_tick;
            const auto opportunities=age<120 ? 0:events(ticks)-events(b.placed_tick+119);
            auto used=food_batch::history_total(stock.consumed);add(used,stock.missed_demands);
            if (used>opportunities || (!h.house.occupants && used)) invalid("Food demand exceeds actual paid occupied-House clock opportunities");
            if (!stock.quality_witness) {
                if (stock.received!=Counters{} || stock.consumed!=Counters{} || amount || h.house.food_quality)
                    invalid("Food quality or historical reception lacks an actual House arrival witness");
            } else {
                const auto& witness=*stock.quality_witness;const auto accepted=volume(witness.accepted);
                if constexpr(processing)if(witness.accepted[processed_food_count-1]||witness.consumed_before[processed_food_count-1])invalid("House witness contains unavailable grade5 meals");
                const auto c=std::find_if(couriers.begin(),couriers.end(),[&](const auto& actor){return actor.id==witness.transfer.courier_id;});
                const auto* latest=trip_for(food.trips,witness.transfer.courier_id);
                if (c==couriers.end() || (c->role!=CourierRole::OriginalFoodToHouse&&!(processing&&c->role==CourierRole::OriginalMealsToHouse)) || !latest || !witness.transfer.sequence ||
                    witness.transfer.sequence>latest->transfer.sequence || !witness.tick || witness.tick>ticks || witness.transfer.sequence>witness.tick ||
                    witness.tick<b.placed_tick || witness.tick-b.placed_tick<120 || accepted<=0 || accepted>4)
                    invalid("Food quality witness has no possible paid Distributor arrival");
                if (witness.tick<body(buildings,c->owner).placed_tick) invalid("Food quality witness predates its paid Distributor owner");
                if (latest->transfer==witness.transfer && (latest->phase!=OriginalGoodsTransferPhase::Completed || latest->target!=b.id || latest->delivered!=witness.accepted))
                    invalid("Food witness disagrees with its retained actual arrival");
                Amounts previous{},available{};std::uint64_t used_since=0,used_before=0;
                for (std::size_t g=0;g<previous.size();++g) {
                    if (witness.consumed_before[g]>stock.consumed[g] || stock.received[g]<static_cast<std::uint64_t>(witness.accepted[g]) ||
                        stock.received[g]-static_cast<std::uint64_t>(witness.accepted[g])<witness.consumed_before[g]) invalid("Food witness invents previous portions or consumed history");
                    const auto n=stock.received[g]-static_cast<std::uint64_t>(witness.accepted[g])-witness.consumed_before[g];
                    if (n>8) invalid("Food witness previous stock outside House capacity");previous[g]=static_cast<std::int32_t>(n);
                    available[g]=previous[g]+witness.accepted[g];
                    const auto delta=stock.consumed[g]-witness.consumed_before[g];
                    if (delta>static_cast<std::uint64_t>(available[g]) || available[g]-static_cast<std::int64_t>(delta)!=stock.amounts[g])
                        invalid("Food witness portions do not lead to sole current stock");
                    add(used_since,delta);add(used_before,witness.consumed_before[g]);
                }
                const auto before_opportunities=events(witness.tick-1)-events(b.placed_tick+119);
                if (volume(available)>8 || used_before>before_opportunities || used_since>events(ticks)-events(witness.tick-1))
                    invalid("Food witness violates capacity, consumption timing or Wheat-first sink");
                const auto expected_sink=consumed_portions(available,used_since);
                for(std::size_t g=0;g<available.size();++g)
                    if(stock.consumed[g]-witness.consumed_before[g]!=static_cast<std::uint64_t>(expected_sink[g])) invalid("Food sink changed crop identity/order");
                const auto prior=volume(previous);
                if ((!prior && witness.previous_quality) || (prior && (witness.previous_quality<20 || witness.previous_quality>(processing ? 100:agriculture ? 90:80))))
                    invalid("Food witness previous quality has no possible positive batch origin");
                if (!agriculture && prior && ((stock.received[1]==static_cast<std::uint64_t>(witness.accepted[1]) && witness.previous_quality!=20) ||
                    (stock.received[0]==static_cast<std::uint64_t>(witness.accepted[0]) && witness.previous_quality!=80)))
                    invalid("Food witness previous quality contradicts exclusive crop history");
                if constexpr (agriculture) {
                    std::uint8_t minimum=UINT8_MAX,maximum=0;
                    for(std::size_t g=0;g<stock.amounts.size();++g) if(stock.received[g]>static_cast<std::uint64_t>(witness.accepted[g])) {
                        Amounts single{};single[g]=1;const auto score=batch_quality(single).value;
                        minimum=std::min(minimum,score);maximum=std::max(maximum,score);
                    }
                    if(prior && (witness.previous_quality<minimum || witness.previous_quality>maximum))
                        invalid("Agriculture previous quality has no actual received crop origin");
                }
                const auto expected=original::blend_food_quality(witness.previous_quality,batch_quality(witness.accepted).value,accepted,prior);
                if (h.house.food_quality!=(amount ? expected:0)) invalid("sole House Food quality disagrees with witnessed original blend or empty reset");
            }
        }
        if (!food_source(b.kind,fishing,hunting)) {
            bool pruned=false;Counters delivered{};
            for (const auto& c:couriers) if (warehouses ?
                (b.kind==Object::Warehouse ? c.role==CourierRole::OriginalFoodToMarket||c.role==CourierRole::OriginalMealsToShop:
                 b.kind==Object::Market||b.kind==Object::OriginalMill53 ? c.role==CourierRole::OriginalFoodToMarket||c.role==CourierRole::OriginalWarehouseFood:
                 b.kind==Object::OriginalFoodShop66 ? c.role==CourierRole::OriginalMealsToShop||c.role==CourierRole::OriginalWarehouseFood:
                 c.role==CourierRole::OriginalFoodToHouse||c.role==CourierRole::OriginalMealsToHouse):
                (b.kind==Object::Market||b.kind==Object::OriginalMill53 ? c.role==CourierRole::OriginalFoodToMarket:
                 b.kind==Object::OriginalFoodShop66 ? c.role==CourierRole::OriginalMealsToShop:
                 c.role==CourierRole::OriginalFoodToHouse||(processing&&c.role==CourierRole::OriginalMealsToHouse))) {
                const auto* trip=trip_for(food.trips,c.id);if (!trip) continue;pruned|=trip->transfer.sequence>1;
                if (trip->phase==OriginalGoodsTransferPhase::Completed && trip->target==b.id)
                    for (std::size_t g=0;g<delivered.size();++g) add(delivered[g],static_cast<std::uint64_t>(trip->delivered[g]));
            }
            for (std::size_t g=0;g<delivered.size();++g)
                if (stock.received[g]<delivered[g] || (!pruned && stock.received[g]!=delivered[g])) invalid("Food owner received counter has no possible actual supplier arrival");
        }
    }
    for (std::size_t g=0;g<harvest.size();++g) {
        auto held=all_stock[g];add(held,all_cargo[g]);add(held,all_consumed[g]);
        if (harvest[g]!=held) invalid("Food per-crop global harvest conservation mismatch");
        auto arrived=all_received[g];add(arrived,all_returned[g]);add(arrived,all_cargo[g]);
        if (all_departed[g]!=arrived) invalid("Food dispatched/received/returned/remaining mass mismatch");
    }
}
}

FoodBatchQuality original_food_batch_quality(const OriginalFoodAmounts& amounts) {
    // Frozen authored Rules10–23 scores; no additional crop is admitted.
    constexpr std::array<std::uint8_t,original_food_crop_count> scores{20,80};
    return {food_batch::weighted_quality(amounts,scores)};
}

void World::validate_original_food_model() const {
    if (!original_rules_model_) invalid("Food requires a bound prepared model");
    for (const auto kind:{original::ModelBuilding::Farmhouse,original::ModelBuilding::WheatField,original::ModelBuilding::MilletField}) {
        if (!original_rules_model_->supports_building(kind) || original_rules_model_->building_value(kind,original::BuildingField::Cost)<=0)
            invalid("Food Farmhouse/Field requires positive prepared model capability and price");
        const auto need=original_rules_model_->building_value(kind,original::BuildingField::Employees);
        if (kind==original::ModelBuilding::Farmhouse ? need<=0 || need>INT16_MAX:need!=0)
            invalid("Food Farmhouse requires bounded model workforce and Fields zero workers");
    }
}
CommandResult World::validate_original_food_command(Command command) const {
    if(original_agriculture_enabled()) return validate_agriculture_command(command);
    CommandResult result{false,false,"Food construction requires explicit revision18",command_sequence_,ticks_};
    if (!original_production_enabled() || !original_production_) return result;
    if (!in_bounds(command.cell)) {result.reason="Food construction outside permitted map bounds";return result;}
    const auto& food=original_production_->food;
    if (command.type==CommandType::PlaceOriginalFoodFarm) {
        if (food.farms.size()>=original_food_farm_limit(profile_,rule_version_)) {result.reason="Food Farmhouse limit reached";return result;}
    } else if (command.type==CommandType::PlaceOriginalWheatField || command.type==CommandType::PlaceOriginalMilletField) {
        const auto f=std::find_if(food.farms.begin(),food.farms.end(),[&](const auto& row){return row.farm_id==command.building_id;});
        if (f==food.farms.end() || f->field_id || !adjacent(building(f->farm_id).cell,command.cell)) {
            result.reason="Food Field requires its explicitly selected paid Farm, one free field slot and direct adjacency";return result;
        }
    } else return result;
    result.accepted=true;result.changed=true;result.reason="Food purchase facts ready";return result;
}
void World::validate_multi_field_agriculture_snapshot(const WorldSnapshot& snapshot) {
    if(!simulation::multi_field_agriculture_enabled(snapshot.profile,snapshot.rule_version)) {
        if(snapshot.multi_field_agriculture) invalid("Historical snapshots reject multi-field work authority");
        return;
    }
    if(!snapshot.multi_field_agriculture || (simulation::food_processing_enabled(snapshot.profile,snapshot.rule_version)?(!snapshot.processed_food||snapshot.hunting_food):(!snapshot.hunting_food||snapshot.processed_food))) invalid("Multi-field snapshot requires work and sole seven-kind Food authority");
    const auto validate=[&](const auto& food){validate_multi_field_work(*snapshot.multi_field_agriculture,food,snapshot.buildings,snapshot.ticks,snapshot.seasonal_agriculture?&*snapshot.seasonal_agriculture:nullptr,snapshot.irrigation_productivity?&*snapshot.irrigation_productivity:nullptr,simulation::agriculture_reconfiguration_enabled(snapshot.profile,snapshot.rule_version)&&snapshot.agriculture_reconfiguration?&*snapshot.agriculture_reconfiguration:nullptr);};
    if(snapshot.processed_food)validate(*snapshot.processed_food);else validate(*snapshot.hunting_food);
}
void World::validate_original_food_snapshot(const WorldSnapshot& snapshot) {
    validate_multi_field_agriculture_snapshot(snapshot);
    if (!snapshot.original_production || !snapshot.original_economy) invalid("Food snapshot has no original production authority");
    if(simulation::food_processing_enabled(snapshot.profile,snapshot.rule_version)) {
        if(!snapshot.processed_food || !snapshot.food_processing || snapshot.hunting_food || snapshot.fishing_food || snapshot.agriculture ||
           !snapshot.hunting || !snapshot.fishing || snapshot.original_production->food!=OriginalFoodState{})
            invalid("Food processing requires its sole full-width graded Food authority");
        validate_food(*snapshot.processed_food,snapshot.buildings,snapshot.couriers,snapshot.original_economy->houses,
            snapshot.ticks,snapshot.profile,snapshot.rule_version,true,true,&*snapshot.fishing,&*snapshot.hunting,
            snapshot.multi_field_agriculture?&*snapshot.multi_field_agriculture:nullptr,snapshot.physical_food_growers?&*snapshot.physical_food_growers:nullptr,
            snapshot.seasonal_agriculture?&*snapshot.seasonal_agriculture:nullptr,snapshot.irrigation_productivity?&*snapshot.irrigation_productivity:nullptr,&*snapshot.food_processing,simulation::agriculture_reconfiguration_enabled(snapshot.profile,snapshot.rule_version)&&snapshot.agriculture_reconfiguration?&*snapshot.agriculture_reconfiguration:nullptr);
        return;
    }
    if(snapshot.processed_food||snapshot.food_processing)invalid("Historical Food rejects processing authority");
    if(simulation::playable_hunting_enabled(snapshot.profile,snapshot.rule_version)) {
        if(!snapshot.hunting_food || !snapshot.hunting || !snapshot.fishing || snapshot.fishing_food || snapshot.agriculture || snapshot.original_production->food!=OriginalFoodState{})
            invalid("Hunting requires sole seven-kind Food authority without historical projections");
        validate_food(*snapshot.hunting_food,snapshot.buildings,snapshot.couriers,snapshot.original_economy->houses,
            snapshot.ticks,snapshot.profile,snapshot.rule_version,true,true,&*snapshot.fishing,&*snapshot.hunting,snapshot.multi_field_agriculture ? &*snapshot.multi_field_agriculture:nullptr,snapshot.physical_food_growers ? &*snapshot.physical_food_growers:nullptr,snapshot.seasonal_agriculture?&*snapshot.seasonal_agriculture:nullptr,snapshot.irrigation_productivity?&*snapshot.irrigation_productivity:nullptr);return;
    }
    if(snapshot.hunting_food || snapshot.hunting) invalid("Historical Food rejects Hunting authority");
    if(simulation::playable_fishing_enabled(snapshot.profile,snapshot.rule_version)) {
        if(!snapshot.fishing_food || !snapshot.fishing || snapshot.agriculture || snapshot.original_production->food!=OriginalFoodState{})
            invalid("Fishing requires sole six-kind Food authority without historical projections");
        validate_food(*snapshot.fishing_food,snapshot.buildings,snapshot.couriers,snapshot.original_economy->houses,
            snapshot.ticks,snapshot.profile,snapshot.rule_version,true,true,&*snapshot.fishing);return;
    }
    if(snapshot.fishing_food || snapshot.fishing) invalid("Historical Food rejects Fishing authority");
    if(simulation::original_agriculture_family_enabled(snapshot.profile,snapshot.rule_version)) {
        if(!snapshot.agriculture || snapshot.original_production->food!=OriginalFoodState{}) invalid("Agriculture requires sole five-kind Food authority");
        validate_food(*snapshot.agriculture,snapshot.buildings,snapshot.couriers,snapshot.original_economy->houses,snapshot.ticks,snapshot.profile,snapshot.rule_version);return;
    }
    if(snapshot.agriculture) invalid("historical Food rejects Agriculture state");
    validate_food(snapshot.original_production->food,snapshot.buildings,snapshot.couriers,snapshot.original_economy->houses,
        snapshot.ticks,snapshot.profile,snapshot.rule_version);
}
void World::validate_original_food_state() const {
    with_original_food([&](const auto& food){validate_food(food,buildings_,couriers_,original_economy_->houses,ticks_,profile_,rule_version_,true,true,fishing_ ? &*fishing_:nullptr,hunting_ ? &*hunting_:nullptr,multi_field_agriculture_ ? &*multi_field_agriculture_:nullptr,physical_food_growers_ ? &*physical_food_growers_:nullptr,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,food_processing_?&*food_processing_:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);});
}
bool World::original_food_courier(CourierRole role) const {return original_production_enabled() && food_role(role) && (!meal_role(role)||food_processing_enabled()) && (role!=CourierRole::OriginalWarehouseFood||food_warehouses_enabled());}
int World::original_food_cargo(CourierId id) const {
    if (!original_production_enabled()) return 0;
    return with_original_food([&](auto& food) {

    (void)courier(id);
    const auto* trip=trip_for(food.trips,id);return trip ? volume(remaining(*trip)):0;

    });
}
int World::original_food_stock(BuildingId owner) const {
    return with_original_food([&](auto& food) {
return volume(account(food.stocks,owner).amounts);
    });
}
int World::original_food_headroom(BuildingId owner) const {
    return with_original_food([&](auto& food) {


    return capacity(building(owner).kind)-original_food_stock(owner)-incoming(food,owner)-return_room(food,owner)-
        boat_return_room(fishing_ ? &*fishing_:nullptr,owner)-hunter_return_room(hunting_ ? &*hunting_:nullptr,owner)-physical_food_grower_reservations(physical_food_growers_ ? &*physical_food_growers_:nullptr,owner)-food_processing_output_reservation(owner);

    });
}
void World::credit_fishing_return(BuildingId quay,int amount) {
    if(!playable_fishing_enabled() || (!fishing_food_ && !hunting_food_ && !processed_food_) || !fishing_ ||
        building(quay).kind!=Object::OriginalFishingQuay || amount<=0 || amount>original_food_headroom(quay))
        invalid("Fish credit requires actual reserved dock return and positive finite headroom");
    with_agriculture_food([&](auto& food){account(food.stocks,quay).amounts[fishing_food_index]+=amount;});
    validate_original_food_state();
}
void World::credit_hunting_return(BuildingId tent,int amount) {
    if(!playable_hunting_enabled() || (!hunting_food_ && !processed_food_) || !hunting_ ||
        building(tent).kind!=Object::OriginalHuntersTent || amount<=0 || amount>original_food_headroom(tent))
        invalid("Meat credit requires actual reserved Tent return and positive finite headroom");
    with_agriculture_food([&](auto& food){account(food.stocks,tent).amounts[hunting_food_index]+=amount;});
    validate_original_food_state();
}
int World::agriculture_field_progress(BuildingId id) const {
    if(seasonal_agriculture_enabled()) return static_cast<int>(seasonal_food_progress(id));
    if(multi_field_agriculture_) {
        const auto& rows=multi_field_agriculture_->fields;
        const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.field_id<key;});
        return at!=rows.end() && at->field_id==id ? static_cast<int>(at->productive_ticks%multi_field_work_per_portion):0;
    }
    if(!original_agriculture_enabled()) return 0;
    for(const auto& field:agriculture_fields()) if(field.field_id==id) return building(field.farm_id).progress;
    return 0;
}
void World::advance_multi_field_agriculture() {
    with_agriculture_food([&](auto& food){
    for(auto& schedule:multi_field_agriculture_->farms) {
        const auto& farm=building(schedule.farm_id);
        if(!farm.operating_enabled || building_on_fire(farm.id) || !building_staffed_for_tick(farm.id) ||
           building_entrances(farm.id).empty() || original_food_headroom(farm.id)<=0) continue;
        auto& rows=multi_field_agriculture_->fields;
        const auto after=schedule.last_field ? std::upper_bound(rows.begin(),rows.end(),*schedule.last_field,
            [](BuildingId id,const auto& row){return id<row.field_id;}):rows.begin();
        const auto start=static_cast<std::size_t>(after-rows.begin());
        for(std::size_t n=0;n<rows.size();++n) {
            auto& work=rows[(start+n)%rows.size()];
            if(work.farm_id!=farm.id) continue;
            const auto& field=building(work.field_id);
            if(!field.placed || !field.operating_enabled || building_on_fire(field.id)) continue;
            auto at=std::lower_bound(food.fields.begin(),food.fields.end(),field.id,
                [](const auto& row,BuildingId id){return row.field_id<id;});
            if(at==food.fields.end() || at->field_id!=field.id) throw std::logic_error("Multi-field work lost paid Food Field");
            add(work.productive_ticks,1);schedule.last_field=field.id;
            if(work.productive_ticks%multi_field_work_per_portion==0) {
                add(at->harvested,1);++account(food.stocks,farm.id).amounts[static_cast<std::size_t>(at->crop)];
            }
            break;
        }
    }
    });
}
void World::advance_original_food() {
    if(physical_food_growers_enabled()) {advance_physical_food_growers();return;}
    if(multi_field_agriculture_enabled()) {advance_multi_field_agriculture();return;}
    if(original_plantations_enabled()) {
        // Explicit retained AUTHORED adapter, not Original Grower/calendar
        // reconstruction. Rule25/26 retain Wheat/Millet only; Rule27 admits
        // all five typed crops under a separate explicit contract.
        with_agriculture_food([&](auto& agriculture) {
        for(auto& record:agriculture.fields) {
            const auto g=static_cast<std::size_t>(record.crop);
            if(!authored_agriculture_crop_enabled(profile_,rule_version_,record.crop)) continue;
            auto& farm=mutable_building(record.farm_id);
            if(!farm.operating_enabled || building_on_fire(farm.id) || !building_staffed_for_tick(farm.id) ||
                building_entrances(farm.id).empty()) continue;
            if(playable_agriculture_enabled()) {
                const auto& field=building(record.field_id);
                if(!field.placed || !field.operating_enabled || building_on_fire(field.id)) continue;
            }
            if(original_food_headroom(farm.id)<=0) {farm.progress=0;continue;}
            if(farm.progress<79) {++farm.progress;continue;}
            add(record.harvested,1);++account(agriculture.stocks,farm.id).amounts[g];farm.progress=0;
        }
        });
        return;
    }
    // Rule24 deliberately has no active Food origin. Source phase20 is known,
    // but actual Grower work/harvest countdown and arrival timing is unclosed.
    if(original_agriculture_enabled()) return;
    for (std::size_t i=0;i<original_production_->food.farms.size();++i) {
        const auto& record=original_production_->food.farms[i];auto& farm=mutable_building(record.farm_id);
        if (!record.field_id || !building_staffed_for_tick(farm.id)) continue;
        if (original_food_headroom(farm.id)<=0) {farm.progress=0;continue;}
        if (farm.progress<79) {++farm.progress;continue;}
        auto candidate=original_production_->food;auto& next=candidate.farms[i];auto& stock=account(candidate.stocks,farm.id);
        const auto g=crop_index(next.crop);add(next.harvested,1);++stock.amounts[g];
        validate_food(candidate,buildings_,couriers_,original_economy_->houses,ticks_,profile_,rule_version_,false,false);
        original_production_->food=std::move(candidate);farm.progress=0;
    }
}
CourierDispatchDecision World::original_food_dispatch_status(const CourierState& c) const {
    return with_original_food([&](auto& food) -> CourierDispatchDecision {

    const auto carried_kind=available_load(food,c.owner,c.owner,Object::Empty,c.role,food_warehouses_enabled());
    if (!volume(carried_kind)) return {CourierDispatchStatus::NoStock,{}};
    const auto target_space=[&](BuildingId target) {
        auto space=original_food_headroom(target);
        if(food_processing_enabled()&&c.role==CourierRole::OriginalFoodToHouse) {
            int raw=0;const auto& held=account(food.stocks,target).amounts;
            for(std::size_t g=0;g<processed_food_first_meal;++g)raw+=held[g];
            space=std::min(space,std::max(0,4-raw-incoming_raw(food,target)));
        }
        return space;
    };
    if(food_warehouses_enabled() && (c.role==CourierRole::OriginalFoodToMarket || c.role==CourierRole::OriginalMealsToShop || c.role==CourierRole::OriginalWarehouseFood)) {
        // One complete cursor over the existing cached physical routes. Nearby
        // destinations begin each new cursor first; every eligible receiver
        // retains its turn while the topology remains unchanged.
        struct Target {const BuildingState* body=nullptr;std::size_t distance=SIZE_MAX;};
        std::array<Target,Rules::city_v10_household_limit> canonical{};
        std::size_t count=0,start=0;
        for(const auto& b:buildings_)if(courier_can_target(c.role,b,profile_,rule_version_)) {
            if(count==canonical.size())throw std::logic_error("Food routing exceeded its bounded receiver catalog");
            const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),b.id,[](const auto& entry,BuildingId key){return entry.first<key;});
            const auto distance=route!=c.dynamic_target_routes.end()&&route->first==b.id&&route->second ? route->second->size():SIZE_MAX;
            canonical[count++]={&b,distance};
        }
        std::sort(canonical.begin(),canonical.begin()+count,[](const auto& a,const auto& b){
            return std::pair{a.distance,a.body->id}<std::pair{b.distance,b.body->id};
        });
        for(std::size_t i=0;i<count;++i)if(c.last_dispatched_target==canonical[i].body->id)start=(i+1)%count;
        bool eligible=false,room=false;
        for(std::size_t i=0;i<count;++i) {
            const auto& entry=canonical[(start+i)%count];const auto& target=*entry.body;
            if(!target.operating_enabled || building_on_fire(target.id) ||
               (target.kind==Object::Warehouse&&!building_staffed_for_tick(target.id)))continue;
            eligible=true;
            if(target_space(target.id)<=0 || !volume(available_load(food,c.owner,target.id,target.kind,c.role,true)))continue;
            room=true;
            if(entry.distance!=SIZE_MAX)return {CourierDispatchStatus::Ready,target.id};
        }
        return {room ? CourierDispatchStatus::NoRoad:eligible ? CourierDispatchStatus::TargetFull:CourierDispatchStatus::NoTarget,{}};
    }
    if (original_deadline_food_dispatch_enabled() && (c.role==CourierRole::OriginalFoodToHouse ||
        (food_processing_enabled()&&c.role==CourierRole::OriginalMealsToHouse))) {
        const auto add=[](std::uint64_t a,std::uint64_t b) {return b>UINT64_MAX-a ? UINT64_MAX:a+b;};
        // Actual dispatch precedes this tick's movement and consumption. A
        // public query sees a completed tick and predicts the next departure.
        const auto dispatch_tick=tick_staffing_active_ ? ticks_:add(ticks_,1);
        const auto through=tick_staffing_active_ ? ticks_-1:ticks_;
        const auto edge=static_cast<std::uint64_t>(active_rules().courier_edge_ticks);
        const auto consumption_tick=[&](std::uint64_t index) {
            const auto offset=index%2 ? std::uint64_t{397}:std::uint64_t{40};
            return index/2>(UINT64_MAX-offset)/816 ? UINT64_MAX:offset+(index/2)*816;
        };
        struct Incoming {BuildingId target{};std::uint64_t arrival=0;int amount=0;CourierId actor{};};
        constexpr auto extra_suppliers=std::is_same_v<std::remove_cvref_t<decltype(food)>,ProcessedFoodState> ? food_processing_shop_limit:0;
        std::array<Incoming,Rules::city_v11_market_limit+extra_suppliers> incoming_rows{};std::size_t incoming_count=0;
        for (const auto& trip:food.trips) {
            if (trip.transfer.courier_id==c.id || trip.phase!=OriginalGoodsTransferPhase::Loaded) continue;
            const auto actor=std::find_if(couriers_.begin(),couriers_.end(),[&](const auto& other) {return other.id==trip.transfer.courier_id;});
            if (actor==couriers_.end() || (actor->role!=CourierRole::OriginalFoodToHouse&&
                !(food_processing_enabled()&&actor->role==CourierRole::OriginalMealsToHouse)) || !actor->enabled ||
                actor->phase!=CourierPhase::ToWarehouse || actor->route_pending || actor->target!=trip.target ||
                actor->path.empty() || actor->path_vertex+1>=actor->path.size() || !edge ||
                static_cast<std::uint64_t>(actor->edge_progress)>=edge) continue;
            const auto edges=static_cast<std::uint64_t>(actor->path.size()-1-actor->path_vertex);
            if (edges>UINT64_MAX/edge) continue;
            const auto movement=edges*edge-static_cast<std::uint64_t>(actor->edge_progress);
            const auto amount=volume(remaining(trip));
            // Source fire or lost staffing does not stop begun movement.
            // An explicit pause has already changed the trip to Returning.
            if (movement && amount>0 && incoming_count<incoming_rows.size())
                incoming_rows[incoming_count++]={trip.target,add(dispatch_tick,movement-1),amount,actor->id};
        }
        std::sort(incoming_rows.begin(),incoming_rows.begin()+incoming_count,[](const auto& a,const auto& b) {
            return std::pair{a.arrival,a.actor}<std::pair{b.arrival,b.actor};
        });
        // Preserve rotation in all paid compatible identities, including a
        // currently burning/paused destination. Temporary eligibility cannot
        // erase the saved cursor and restart at the first healthy House.
        std::array<const BuildingState*,Rules::city_v10_household_limit> canonical{};
        std::size_t count=0,start=0;
        for (const auto& b:buildings_) if (b.placed && courier_can_target(c.role,b,profile_,rule_version_)) canonical[count++]=&b;
        for (std::size_t i=0;i<count;++i) if (c.last_dispatched_target==canonical[i]->id) start=(i+1)%count;
        struct Choice {BuildingId target{};std::uint64_t deadline=0,arrival=0,next_departure=0;};
        std::optional<Choice> selected;
        const auto earlier=[](const Choice& a,const Choice& b) {
            if (a.deadline!=b.deadline) return a.deadline<b.deadline;
            // Equal empty recipients need the earliest physical rescue, not
            // the farthest recipient produced by negative-laxity ranking.
            if (a.arrival!=b.arrival) return a.arrival<b.arrival;
            if (a.next_departure!=b.next_departure) return a.next_departure<b.next_departure;
            return false; // Exact ties retain canonical cyclic order.
        };
        bool eligible=false,room=false,missing_road=false,quality_blocked=false,qualified_raw=false;
        for (std::size_t i=0;i<count;++i) {
            const auto& target=*canonical[(start+i)%count];
            if (!target.operating_enabled || building_on_fire(target.id) || home(original_economy_->houses,target.id).house.occupants<=0) continue;
            eligible=true;
            // Capacity still reserves every real incoming load, including a
            // road-waiting load that is too uncertain to predict as coverage.
            if (target_space(target.id)<=0 || (food_processing_enabled()&&
                !volume(available_load(food,c.owner,target.id,target.kind,c.role,food_warehouses_enabled())))) continue;
            room=true;
            if(quality_aware_food_dispatch_enabled()&&c.role==CourierRole::OriginalFoodToHouse) {
                if(!volume(quality_aware_target_raw_load(available_load(food,c.owner,target.id,target.kind,c.role,food_warehouses_enabled()),
                    target_space(target.id),home(original_economy_->houses,target.id).house,original_rules_model_->house_rules()))) {
                    quality_blocked=true;continue;
                }
                qualified_raw=true;
            }
            const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target.id,
                [](const auto& entry,BuildingId key){return entry.first<key;});
            if (route==c.dynamic_target_routes.end() || route->first!=target.id || !route->second || route->second->size()<2) {
                missing_road=true;continue;
            }
            const auto edges=static_cast<std::uint64_t>(route->second->size()-1);
            if (!edge || edges>UINT64_MAX/edge/2) {missing_road=true;continue;}
            const auto movement=edges*edge,arrival=add(dispatch_tick,movement-1),next_departure=add(dispatch_tick,movement*2);
            const auto stock=original_food_stock(target.id);
            // Before the first depletion, only incoming quantities that
            // physically arrive no later than that consumption can extend
            // coverage. There are at most eight reserved House portions.
            const auto deadline_from=[&](int portions,std::uint64_t consumed_through,std::uint64_t incoming_after) {
                if (!portions) return dispatch_tick;
                auto deadline=consumption_tick(add(events(consumed_through),static_cast<std::uint64_t>(portions)-1));
                for (std::size_t n=0;n<incoming_count;++n) {
                    const auto& load=incoming_rows[n];
                    if (load.target!=target.id || load.arrival<=incoming_after || load.arrival>deadline) continue;
                    portions+=load.amount;
                    deadline=consumption_tick(add(events(consumed_through),static_cast<std::uint64_t>(portions)-1));
                }
                return deadline;
            };
            auto deadline=deadline_from(stock,through,through);
            // A real earlier arrival may already restore an unavoidable gap
            // before this actor could get there. Forecast its actual scalar
            // portions at our arrival, never a future dispatch or production.
            int at_arrival=stock;auto consumed=events(through);bool earlier_incoming=false;
            for (std::size_t n=0;n<incoming_count;++n) {
                const auto& load=incoming_rows[n];if (load.target!=target.id || load.arrival>arrival) continue;
                const auto before=events(load.arrival-1),used=before-consumed;
                at_arrival=used>=static_cast<std::uint64_t>(at_arrival) ? 0:at_arrival-static_cast<int>(used);
                at_arrival+=load.amount;consumed=before;earlier_incoming=true;
            }
            const auto before_arrival=events(arrival-1),used=before_arrival-consumed;
            at_arrival=used>=static_cast<std::uint64_t>(at_arrival) ? 0:at_arrival-static_cast<int>(used);
            if (earlier_incoming && at_arrival>0) {
                deadline=deadline_from(at_arrival,arrival-1,arrival);
                // Coalesce only enough real incoming quantity to cover our
                // full physical visit/return opportunity. A single portion
                // expiring before that boundary still permits a real refill.
                if (deadline>=next_departure) continue;
            }
            const Choice candidate{target.id,deadline,arrival,next_departure};
            if (!selected || earlier(candidate,*selected)) selected=candidate;
        }
        if (selected) return {CourierDispatchStatus::Ready,selected->target};
        if (!eligible) return {CourierDispatchStatus::NoTarget,{}};
        if (!room) return {CourierDispatchStatus::TargetFull,{}};
        if (missing_road) return {CourierDispatchStatus::NoRoad,{}};
        if (quality_blocked&&!qualified_raw) return {CourierDispatchStatus::NoQualifiedFood,{}};
        return {CourierDispatchStatus::NoTarget,{}};
    }
    std::array<const BuildingState*,Rules::city_v10_household_limit> targets{};std::size_t count=0;
    for (const auto& b:buildings_) if (courier_can_target(c.role,b,profile_,rule_version_) && b.operating_enabled && !building_on_fire(b.id)) {
        if (original_house_kind(b.kind) && home(original_economy_->houses,b.id).house.occupants<=0) continue;
        targets[count++]=&b;
    }
    if (!count) return {CourierDispatchStatus::NoTarget,{}};std::size_t start=0;
    for (std::size_t i=0;i<count;++i) if (c.last_dispatched_target==targets[i]->id) start=(i+1)%count;
    bool room=false;
    const int passes=food_processing_enabled()&&c.role==CourierRole::OriginalFoodToMarket?2:1;
    for(int pass=0;pass<passes;++pass)for (std::size_t i=0;i<count;++i) {
        const auto& target=*targets[(start+i)%count];
        if(passes==2&&((target.kind==Object::OriginalMill53)!=(pass==0)))continue;
        const auto space=target_space(target.id);
        if(space<=0||!volume(available_load(food,c.owner,target.id,target.kind,c.role,food_warehouses_enabled())))continue;room=true;
        const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target.id,[](const auto& entry,BuildingId key){return entry.first<key;});
        if (route!=c.dynamic_target_routes.end() && route->first==target.id && route->second) return {CourierDispatchStatus::Ready,target.id};
    }
    return {room ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::TargetFull,{}};

    });
}
void World::dispatch_original_food(CourierState& c) {
    return with_original_food([&](auto& food) {

    const auto decision=courier_dispatch_status(c.id);if (decision.status!=CourierDispatchStatus::Ready || !decision.selected_target) return;
    const auto target=*decision.selected_target;const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target,[](const auto& entry,BuildingId key){return entry.first<key;});
    if (route==c.dynamic_target_routes.end() || !route->second) throw std::logic_error("Food dispatch lost its shared cached route");
    auto candidate=food;auto& source=account(candidate.stocks,c.owner);const auto* previous=trip_for(candidate.trips,c.id);
    if (previous && previous->transfer.sequence==UINT64_MAX) return;
    const auto& destination=building(target);const auto available=available_load(food,c.owner,target,destination.kind,c.role,food_warehouses_enabled());
    auto headroom=original_food_headroom(target);
    if(food_processing_enabled()&&c.role==CourierRole::OriginalFoodToHouse) {
        int raw=0;const auto& held=account(candidate.stocks,target).amounts;
        for(std::size_t g=0;g<processed_food_first_meal;++g)raw+=held[g];
        headroom=std::min(headroom,std::max(0,4-raw-incoming_raw(candidate,target)));
    }
    const auto amount=std::min({4,volume(available),headroom});if (!amount) return;
    typename std::remove_cvref_t<decltype(food.trips)>::value_type next{{c.id,previous ? previous->transfer.sequence+1:1},c.owner,target};
    next.loaded=quality_aware_food_dispatch_enabled()&&c.role==CourierRole::OriginalFoodToHouse ?
        quality_aware_target_raw_load(available,headroom,home(original_economy_->houses,target).house,original_rules_model_->house_rules()):
        portions(available,amount);
    if(!volume(next.loaded))return;
    for (std::size_t g=0;g<next.loaded.size();++g) {source.amounts[g]-=next.loaded[g];add(source.departed[g],static_cast<std::uint64_t>(next.loaded[g]));}
    if (auto* prior=trip_for(candidate.trips,c.id)) *prior=next;
    else candidate.trips.insert(std::lower_bound(candidate.trips.begin(),candidate.trips.end(),c.id,[](const auto& trip,CourierId key){return trip.transfer.courier_id<key;}),next);
    auto path=*route->second;
    // No actual movement fields are changed until every bounded DTO check and
    // the route allocation have completed successfully.
    validate_food(candidate,buildings_,couriers_,original_economy_->houses,ticks_,profile_,rule_version_,false,true,fishing_ ? &*fishing_:nullptr,hunting_ ? &*hunting_:nullptr,multi_field_agriculture_ ? &*multi_field_agriculture_:nullptr,physical_food_growers_ ? &*physical_food_growers_:nullptr,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,food_processing_?&*food_processing_:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);
    food=std::move(candidate);c.path=std::move(path);c.target=target;c.last_dispatched_target=target;c.path_vertex=0;c.edge_progress=0;
    c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;

    });
}
void World::cancel_original_food(BuildingId owner) {
    return with_original_food([&](auto& food) {

    for (auto& c:couriers_) if (c.owner==owner && original_food_courier(c.role) && c.phase==CourierPhase::ToWarehouse) {
        auto candidate=food;auto* trip=trip_for(candidate.trips,c.id);
        if (!trip) throw std::logic_error("loaded Food actor lost its trip");trip->phase=OriginalGoodsTransferPhase::Returning;
        const auto here=c.path.at(c.path_vertex);const auto& source=building(owner);
        const bool already_home=!c.edge_progress && building_contains(source.id,here);
        if (already_home) return_load(candidate,*trip);
        validate_food(candidate,buildings_,couriers_,original_economy_->houses,ticks_,profile_,rule_version_,false,true,fishing_ ? &*fishing_:nullptr,hunting_ ? &*hunting_:nullptr,multi_field_agriculture_ ? &*multi_field_agriculture_:nullptr,physical_food_growers_ ? &*physical_food_growers_:nullptr,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,food_processing_?&*food_processing_:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);
        food=std::move(candidate);
        if (already_home) {c.path.clear();c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::IdleAtWorkshop;}
        else {c.phase=CourierPhase::Returning;mark_route_pending(c);if (!c.edge_progress) reroute_v2(c);}
    }

    });
}
void World::arrive_original_food(CourierState& c) {
    return with_original_food([&](auto& food) {

    auto candidate=food;auto houses=original_economy_->houses;auto processing=food_processing_;auto warehouses=food_warehouses_;auto* trip=trip_for(candidate.trips,c.id);
    if (!trip) throw std::logic_error("Food arrival lost its latest trip");
    if (c.phase==CourierPhase::ToWarehouse) {
        const auto& target=building(c.target);const auto cargo=remaining(*trip);
        if (!target.operating_enabled || building_on_fire(target.id) ||
            (food_warehouses_enabled() && target.kind==Object::Warehouse && !building_staffed_for_tick(target.id)) ||
            (original_house_kind(target.kind) && home(houses,target.id).house.occupants<=0)) trip->phase=OriginalGoodsTransferPhase::Returning;
        else {
            auto& stock=account(candidate.stocks,target.id);const auto before=volume(stock.amounts),accepted=volume(cargo);
            if (!accepted || before+accepted>capacity(target.kind)) throw std::logic_error("reserved Food arrival no longer fits its physical target");
            if (original_house_kind(target.kind)) {
                auto& h=home(houses,target.id);
                stock.quality_witness=typename std::remove_cvref_t<decltype(stock.quality_witness)>::value_type{trip->transfer,ticks_,cargo,stock.consumed,h.house.food_quality};
                h.house.food_quality=original::blend_food_quality(h.house.food_quality,batch_quality(cargo).value,accepted,before);
                if constexpr(std::is_same_v<std::remove_cvref_t<decltype(food)>,ProcessedFoodState>) {
                    bool meal=false;for(std::size_t g=processed_food_first_meal;g<cargo.size();++g)meal|=cargo[g]>0;
                    if(meal) {
                        if(!processing)invalid("Meal arrival lost its paid processing history");
                        auto& rows=processing->houses;auto at=std::lower_bound(rows.begin(),rows.end(),target.id,
                            [](const auto& row,BuildingId id){return row.house_id<id;});
                        if(at==rows.end()||at->house_id!=target.id)at=rows.insert(at,MealHouseHistory{target.id,*stock.quality_witness,{},{}});
                        for(std::size_t grade=0;grade<at->first_by_grade.size();++grade)if(cargo[processed_food_first_meal+grade]>0&&!at->first_by_grade[grade])
                            at->first_by_grade[grade]=FoodProcessingArrival{trip->transfer,trip->source,ticks_,cargo[processed_food_first_meal+grade]};
                    }
                }
            }
            if constexpr(std::is_same_v<std::remove_cvref_t<decltype(food)>,ProcessedFoodState>) {
                if(!processing)invalid("Physical Food processing arrival lost its sole causal history");
                if(warehouses) {
                    if(target.kind==Object::Warehouse) {
                        auto& rows=warehouses->warehouses;
                        const auto at=std::lower_bound(rows.begin(),rows.end(),target.id,[](const auto& row,BuildingId id){return row.warehouse_id<id;});
                        if(at==rows.end()||at->warehouse_id!=target.id)invalid("Food arrival lost its exact paid Warehouse");
                        for(std::size_t g=0;g<cargo.size();++g)if(cargo[g]>0&&!at->first_received[g])
                            at->first_received[g]=FoodProcessingArrival{trip->transfer,trip->source,ticks_,cargo[g]};
                    } else if(target.kind==Object::Market) {
                        auto& rows=warehouses->markets;
                        const auto at=std::lower_bound(rows.begin(),rows.end(),target.id,[](const auto& row,BuildingId id){return row.market_id<id;});
                        if(at==rows.end()||at->market_id!=target.id)invalid("Food arrival lost its exact paid Market");
                        for(std::size_t g=0;g<processed_food_first_meal;++g)if(cargo[g]>0&&!at->first_received[g])
                            at->first_received[g]=FoodProcessingArrival{trip->transfer,trip->source,ticks_,cargo[g]};
                    }
                    if(c.role==CourierRole::OriginalWarehouseFood) {
                        auto& rows=warehouses->warehouses;
                        const auto at=std::lower_bound(rows.begin(),rows.end(),trip->source,[](const auto& row,BuildingId id){return row.warehouse_id<id;});
                        if(at==rows.end()||at->warehouse_id!=trip->source)invalid("Food departure lost its exact paid Warehouse");
                        for(std::size_t g=0;g<cargo.size();++g)if(cargo[g]>0&&!at->first_delivered[g])
                            at->first_delivered[g]=FoodWarehouseDelivery{trip->transfer,target.id,ticks_,cargo[g]};
                    }
                }
                if(target.kind==Object::OriginalMill53) {
                    auto& rows=processing->mills;const auto at=std::lower_bound(rows.begin(),rows.end(),target.id,
                        [](const auto& row,BuildingId id){return row.mill_id<id;});
                    if(at==rows.end()||at->mill_id!=target.id)invalid("Actual ingredient arrival lost its paid Mill");
                    for(std::size_t g=0;g<at->first_ingredients.size();++g)if(cargo[g]>0&&!at->first_ingredients[g])
                        at->first_ingredients[g]=FoodProcessingArrival{trip->transfer,trip->source,ticks_,cargo[g]};
                } else if(target.kind==Object::OriginalFoodShop66) {
                    auto& rows=processing->shops;const auto at=std::lower_bound(rows.begin(),rows.end(),target.id,
                        [](const auto& row,BuildingId id){return row.shop_id<id;});
                    if(at==rows.end()||at->shop_id!=target.id)invalid("Actual meal arrival lost its paid FoodShop");
                    for(std::size_t grade=0;grade<at->first_meals.size();++grade)if(cargo[processed_food_first_meal+grade]>0&&!at->first_meals[grade])
                        at->first_meals[grade]=FoodProcessingArrival{trip->transfer,trip->source,ticks_,cargo[processed_food_first_meal+grade]};
                }
            }
            for (std::size_t g=0;g<cargo.size();++g) {stock.amounts[g]+=cargo[g];add(stock.received[g],static_cast<std::uint64_t>(cargo[g]));trip->delivered[g]=cargo[g];}
            trip->phase=OriginalGoodsTransferPhase::Completed;
        }
        validate_food(candidate,buildings_,couriers_,houses,ticks_,profile_,rule_version_,false,true,fishing_ ? &*fishing_:nullptr,hunting_ ? &*hunting_:nullptr,multi_field_agriculture_ ? &*multi_field_agriculture_:nullptr,physical_food_growers_ ? &*physical_food_growers_:nullptr,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,processing?&*processing:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);
        food=std::move(candidate);original_economy_->houses=std::move(houses);food_processing_=std::move(processing);food_warehouses_=std::move(warehouses);c.phase=CourierPhase::Returning;
        if (valid_return_path(c)) {std::reverse(c.path.begin(),c.path.end());c.path_vertex=0;}
        else {c.path={c.path.back()};c.path_vertex=0;c.route_pending=true;c.route_checked_revision.reset();reroute_v2(c);}
    } else {
        if (volume(remaining(*trip))) return_load(candidate,*trip);
        validate_food(candidate,buildings_,couriers_,houses,ticks_,profile_,rule_version_,false,true,fishing_ ? &*fishing_:nullptr,hunting_ ? &*hunting_:nullptr,multi_field_agriculture_ ? &*multi_field_agriculture_:nullptr,physical_food_growers_ ? &*physical_food_growers_:nullptr,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,processing?&*processing:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);
        food=std::move(candidate);food_processing_=std::move(processing);c.path.clear();c.path_vertex=0;c.phase=CourierPhase::IdleAtWorkshop;
        c.route_pending=false;c.route_checked_revision.reset();
    }

    });
}
void World::consume_original_food() {
    return with_original_food([&](auto& food) {

    auto candidate=food;auto houses=original_economy_->houses;auto processing=food_processing_;
    for (auto& h:houses) {
        if (!h.house.occupants) continue;auto& stock=account(candidate.stocks,h.building_id);
        if (!volume(stock.amounts)) {add(stock.missed_demands,1);continue;}
        const auto consumed=consumed_portions(stock.amounts,1);
        for (std::size_t crop=0;crop<consumed.size();++crop) if (consumed[crop]) {
            stock.amounts[crop]-=consumed[crop];
            add(stock.consumed[crop],static_cast<std::uint64_t>(consumed[crop]));
            if constexpr(std::is_same_v<std::remove_cvref_t<decltype(food)>,ProcessedFoodState>)if(crop>=processed_food_first_meal) {
                if(!processing)invalid("Meal consumption lost its sole actual receipt history");
                const auto at=std::lower_bound(processing->houses.begin(),processing->houses.end(),h.building_id,
                    [](const auto& row,BuildingId id){return row.house_id<id;});
                if(at==processing->houses.end()||at->house_id!=h.building_id)invalid("House consumed a meal without an actual first receipt");
                if(!at->first_consumption)at->first_consumption=MealConsumptionReceipt{ticks_,static_cast<std::uint8_t>(crop-processed_food_first_meal+1)};
            }
        }
        if (!volume(stock.amounts)) h.house.food_quality=0;
    }
    validate_food(candidate,buildings_,couriers_,houses,ticks_,profile_,rule_version_,true,true,fishing_ ? &*fishing_:nullptr,hunting_ ? &*hunting_:nullptr,multi_field_agriculture_ ? &*multi_field_agriculture_:nullptr,physical_food_growers_ ? &*physical_food_growers_:nullptr,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,processing?&*processing:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);
    food=std::move(candidate);original_economy_->houses=std::move(houses);food_processing_=std::move(processing);

    });
}

std::optional<BuildingId> World::agriculture_farm_for_field(Cell cell,BuildingId selected) const {
    std::optional<BuildingId> result;std::int64_t distance=INT64_MAX;
    for(const auto& row:agriculture_farms()) {
        const auto& farm=building(row.farm_id);
        if((static_cast<std::uint32_t>(selected) && selected!=farm.id) || !work_area(farm.cell,cell)) continue;
        if(playable_agriculture_enabled() && !static_cast<std::uint32_t>(selected) &&
            std::count_if(agriculture_fields().begin(),agriculture_fields().end(),[&](const auto& field) {
                return field.farm_id==farm.id && (!agriculture_reconfiguration_enabled()||building(field.field_id).placed);
            }) >= static_cast<std::ptrdiff_t>(multi_field_agriculture_enabled() ? multi_field_per_farm_limit:1)) continue;
        const auto d=std::abs(std::int64_t(cell.x)-farm.cell.x-1)+std::abs(std::int64_t(cell.y)-farm.cell.y-1);
        if(d<distance) {result=farm.id;distance=d;}
    }
    return result;
}
CommandResult World::validate_agriculture_command(Command command) const {
    CommandResult result{false,false,"Agriculture requires separately paid Farm and Field",command_sequence_,ticks_};
    if(!original_agriculture_enabled() || !in_bounds(command.cell)) return result;
    if(command.type==CommandType::PlaceOriginalFoodFarm) {
        if(agriculture_farms().size()>=original_food_farm_limit(profile_,rule_version_)) {result.reason="Agriculture Farm limit reached";return result;}
    } else {
        if(agriculture_fields().size()>=agriculture_field_limit) {result.reason="Agriculture field limit reached";return result;}
        const auto parent=agriculture_farm_for_field(command.cell,command.building_id);
        if(!parent) {result.reason="Field requires an existing paid Farmhouse within its source work area";return result;}
        if(multi_field_agriculture_enabled() && std::count_if(agriculture_fields().begin(),agriculture_fields().end(),
            [&](const auto& f){return f.farm_id==*parent&&(!agriculture_reconfiguration_enabled()||building(f.field_id).placed);})>=static_cast<std::ptrdiff_t>(multi_field_per_farm_limit)) {
            result.reason="Multi-field Farm permits at most eight paid Fields";return result;
        }
        if(!multi_field_agriculture_enabled() && original_plantations_enabled() && (playable_agriculture_enabled() || command.type==CommandType::PlaceOriginalWheatField || command.type==CommandType::PlaceOriginalMilletField) &&
            std::any_of(agriculture_fields().begin(),agriculture_fields().end(),[&](const auto& f) {
                return f.farm_id==*parent && authored_agriculture_crop_enabled(profile_,rule_version_,f.crop);
            })) {result.reason=playable_agriculture_enabled() ? "AUTHORED adapter permits one productive Field per Farm":
                "AUTHORED Wheat/Millet adapter permits one productive Field per Farm";return result;}
        if(command.type==CommandType::PlaceOriginalRiceField && original_terrain_facts_->fertility(command.cell)!=100) {
            result.reason="Rice requires source fertility100; irrigation cannot bypass this prerequisite";return result;
        }
    }
    result.accepted=true;result.changed=true;result.reason="Agriculture purchase facts ready";return result;
}
void World::validate_agriculture_model() const {
    validate_original_food_model();
    for(const auto kind:{original::ModelBuilding::RiceField,original::ModelBuilding::CabbageField,original::ModelBuilding::SoybeanField})
        if(!original_rules_model_->supports_building(kind) || original_rules_model_->building_value(kind,original::BuildingField::Cost)<=0 ||
            original_rules_model_->building_value(kind,original::BuildingField::Employees)!=0)
            invalid("Agriculture requires separately prepared positive-price zero-worker crop models");
}
void World::validate_original_agriculture_snapshot(const WorldSnapshot& snapshot) {
    if(!simulation::original_agriculture_family_enabled(snapshot.profile,snapshot.rule_version)) {
        if(snapshot.agriculture || snapshot.fishing_food || snapshot.hunting_food || snapshot.processed_food) invalid("Agriculture authority requires its explicit generation");return;
    }
    validate_original_food_snapshot(snapshot);
}

} // namespace openemperor::simulation
