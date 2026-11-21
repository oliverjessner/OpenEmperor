#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason){throw std::invalid_argument(reason);}
void add(std::uint64_t& value,std::uint64_t n){if(n>UINT64_MAX-value)invalid("Food processing history overflow");value+=n;}
template<class Rows,class Key> decltype(auto) find(Rows& rows,BuildingId id,Key key){
    auto at=std::lower_bound(rows.begin(),rows.end(),id,[&](const auto& row,BuildingId target){return key(row)<target;});
    if(at==rows.end()||key(*at)!=id)invalid("Food processing lost its exact paid owner");return *at;
}
const auto body_key=[](const auto& b){return b.id;};
const auto stock_key=[](const auto& b){return b.building_id;};
std::optional<std::uint64_t> next_food_event(std::uint64_t tick){
    std::optional<std::uint64_t> result;
    for(const std::uint64_t offset:{40,397}){
        std::uint64_t candidate=offset;
        if(tick>offset){const auto delta=tick-offset;const auto periods=delta/816+static_cast<unsigned>(delta%816!=0);
            if(periods>(UINT64_MAX-offset)/816)continue;candidate=offset+periods*816;}
        if(!result||candidate<*result)result=candidate;
    }
    return result;
}
std::uint64_t food_events(std::uint64_t tick){return (tick>=40 ? 1+(tick-40)/816:0)+(tick>=397 ? 1+(tick-397)/816:0);}
template<class Buildings>
std::uint64_t arrival_travel(const Buildings& buildings,BuildingId source,BuildingId target){
    const auto& from=find(buildings,source,body_key);const auto& to=find(buildings,target,body_key);
    const auto gap=footprint_distance(from.cell,building_footprint(RulesProfile::CityV16,kFoodProcessingRuleVersion,from.kind),
        to.cell,building_footprint(RulesProfile::CityV16,kFoodProcessingRuleVersion,to.kind));
    return static_cast<std::uint64_t>(std::max(2,gap))*5;
}
template<class Buildings>
void validate_arrival(const FoodProcessingArrival& arrival,const ProcessedFoodState& food,const Buildings& buildings,BuildingId target,std::size_t slot,std::uint64_t tick){
    const auto& owner=find(buildings,target,body_key);const auto& source=find(buildings,arrival.source,body_key);
    const auto& stock=find(food.stocks,target,stock_key);
    const auto trip=std::find_if(food.trips.begin(),food.trips.end(),[&](const auto& t){return t.transfer.courier_id==arrival.transfer.courier_id;});
    if(!arrival.tick||arrival.tick>tick||arrival.tick<=owner.placed_tick||arrival.tick<=source.placed_tick||arrival.amount<1||arrival.amount>4||
       !arrival.transfer.sequence||arrival.transfer.sequence>arrival.tick-source.placed_tick||
       static_cast<std::uint64_t>(arrival.amount)>stock.received[slot]||trip==food.trips.end()||trip->source!=arrival.source||
       arrival.transfer.sequence>trip->transfer.sequence||arrival.tick-source.placed_tick<arrival_travel(buildings,arrival.source,target)||arrival.tick-owner.placed_tick<arrival_travel(buildings,arrival.source,target))
        invalid("First Food arrival lost its exact physical supplier, positive amount or paid travel epoch");
    if(trip->transfer==arrival.transfer&&(trip->phase!=OriginalGoodsTransferPhase::Completed||trip->target!=target||trip->delivered[slot]!=arrival.amount))
        invalid("First Food arrival contradicts its retained actual transport");
    if(stock.quality_witness&&stock.quality_witness->transfer==arrival.transfer&&
       (stock.quality_witness->tick!=arrival.tick||stock.quality_witness->accepted[slot]!=arrival.amount))
        invalid("First grade arrival contradicts its retained full House witness");
}

// First proofs can share one atomic multi-kind arrival. Publication checks
// their bounded keys together; ordinary ticks create these proofs only through
// the real monotone courier path and do not sort or rebuild this index.
void validate_first_transfer_keys(const FoodProcessingState& state){
    struct Binding {OriginalGoodsTransferKey transfer;BuildingId source{},target{};std::uint64_t tick=0;std::size_t slot=0;std::int32_t amount=0;};
    constexpr std::size_t bound=food_processing_mill_limit*7+food_processing_shop_limit*4+Rules::city_v10_household_limit*(4+processed_food_count);
    std::array<Binding,bound> rows{};std::size_t count=0;
    const auto append=[&](const FoodProcessingArrival& a,BuildingId target,std::size_t slot){
        if(count==rows.size())invalid("Bounded first transfer proof index exhausted");
        rows[count++]={a.transfer,a.source,target,a.tick,slot,a.amount};
    };
    for(const auto& m:state.mills)for(std::size_t g=0;g<7;++g)if(m.first_ingredients[g])append(*m.first_ingredients[g],m.mill_id,g);
    for(const auto& shop:state.shops)for(std::size_t g=0;g<4;++g)if(shop.first_meals[g])append(*shop.first_meals[g],shop.shop_id,7+g);
    for(const auto& house:state.houses){
        for(std::size_t g=0;g<4;++g)if(house.first_by_grade[g])append(*house.first_by_grade[g],house.house_id,7+g);
        // The full immutable first witness asserts zero slots as well.
        const auto first=std::find_if(house.first_by_grade.begin(),house.first_by_grade.end(),[&](const auto& a){return a&&a->transfer==house.first_receipt.transfer;});
        if(first==house.first_by_grade.end())invalid("Full first meal witness lost its actual grade supplier");
        for(std::size_t g=0;g<processed_food_count;++g)append(FoodProcessingArrival{house.first_receipt.transfer,(*first)->source,house.first_receipt.tick,house.first_receipt.accepted[g]},house.house_id,g);
    }
    const auto key=[](const auto& r){return std::pair{r.transfer.courier_id,r.transfer.sequence};};
    std::sort(rows.begin(),rows.begin()+static_cast<std::ptrdiff_t>(count),[&](const auto& a,const auto& b){return key(a)<key(b);});
    std::optional<Binding> previous;
    for(std::size_t begin=0;begin<count;){
        const auto& first=rows[begin];std::array<std::optional<std::int32_t>,processed_food_count> amounts{};std::size_t end=begin;
        while(end<count&&key(rows[end])==key(first)){
            const auto& row=rows[end++];
            if(row.source!=first.source||row.target!=first.target||row.tick!=first.tick)
                invalid("One atomic Food transfer key cannot certify different owners or arrival epochs");
            if(amounts[row.slot]&&*amounts[row.slot]!=row.amount)invalid("One atomic Food transfer contradicts its retained per-kind amounts");
            amounts[row.slot]=row.amount;
        }
        int volume=0;for(const auto& n:amounts)if(n)volume+=*n;
        if(volume<=0||volume>4)invalid("One atomic first Food arrival exceeds its actual Cart volume");
        // There is at most one dispatch per courier/World tick. A canceled
        // trip may turn back after its begun edge, so it cannot be assigned
        // the duration of a complete outward/return visit.
        if(previous&&previous->transfer.courier_id==first.transfer.courier_id&&
           (previous->source!=first.source||first.tick<=previous->tick||
            first.transfer.sequence-previous->transfer.sequence>first.tick-previous->tick))
            invalid("Retained first Food arrivals contradict their monotone physical courier sequence");
        previous=first;begin=end;
    }
}
template<class Buildings>
void validate_processing(const FoodProcessingState& state,const ProcessedFoodState& food,const Buildings& buildings,std::uint64_t tick,bool publication){
    if(buildings.size()>Rules::original_production_building_limit||food.stocks.size()>original_food_stock_limit(RulesProfile::CityV16,kFoodProcessingRuleVersion)||
       food.trips.size()>original_food_trip_limit(RulesProfile::CityV16,kFoodProcessingRuleVersion))invalid("Food processing publication exceeds its unchanged World bounds");
    if(state.mills.size()>food_processing_mill_limit||state.shops.size()>food_processing_shop_limit ||
       std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return b.kind==Object::OriginalMill53;})!=static_cast<std::ptrdiff_t>(state.mills.size()) ||
       std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return b.kind==Object::OriginalFoodShop66;})!=static_cast<std::ptrdiff_t>(state.shops.size()))
        invalid("Food processing must bind every paid Mill and FoodShop exactly once");
    for(std::size_t i=0;i<state.mills.size();++i){
        const auto& m=state.mills[i];const auto& b=find(buildings,m.mill_id,body_key);const auto& stock=find(food.stocks,m.mill_id,stock_key);
        if((i&&!(state.mills[i-1].mill_id<m.mill_id))||b.kind!=Object::OriginalMill53||!b.placed||b.placed_tick>tick||
           m.requested_grade<1||m.requested_grade>4||m.completed>m.started||m.started-m.completed!=static_cast<unsigned>(m.active.has_value()) ||
           m.productive_ticks>tick-b.placed_tick || (m.started && m.started>m.productive_ticks))
            invalid("Mill recipe identity, finite sequence or staffed-work age invalid");
        for(std::size_t g=0;g<7;++g){
            if(bool(stock.received[g])!=m.first_ingredients[g].has_value())invalid("Every actual Mill ingredient requires its immutable first physical arrival");
            if(!m.first_ingredients[g])continue;
            const auto& arrival=*m.first_ingredients[g];validate_arrival(arrival,food,buildings,m.mill_id,g,tick);
            const auto& source=find(buildings,arrival.source,body_key);const auto travel=arrival_travel(buildings,arrival.source,m.mill_id);
            if(arrival.amount>2)invalid("First raw Mill arrival exceeds its per-kind physical admission");
            if(g<5){
                if(source.kind!=Object::OriginalFoodFarm)invalid("Mill crop has no actual Farm supplier");
                bool possible=false;for(const auto& field:food.fields)if(field.farm_id==source.id&&static_cast<std::size_t>(field.crop)==g){
                    const auto placed=find(buildings,field.field_id,body_key).placed_tick;
                    // First crop origin needs at least one outward/return edge,
                    // wait5, wet growth64 and harvest5. Its Farm Cart dispatch
                    // follows actual return on the next tick (minimum83).
                    possible|=arrival.tick>placed&&arrival.tick-placed>=83+travel;}
                if(!possible)invalid("First Mill crop receipt predates any paid compatible physical harvest");
            }else {
                const auto expected=g==5?Object::OriginalFishingQuay:Object::OriginalHuntersTent;
                const std::uint64_t minimum_work=g==5?60:80; // real work + actual dock/tent unload
                if(source.kind!=expected||arrival.tick-source.placed_tick<minimum_work+travel-1)
                    invalid("First Fish/Meat ingredient predates real producer work, unload and land transport");
            }
        }
        const auto validate_batch=[&](const FoodProcessingBatch& batch,bool finished,std::uint64_t through){
            if(!batch.sequence||batch.sequence>m.started||batch.started_tick<=b.placed_tick||batch.started_tick>through||batch.grade<1||batch.grade>4)
                invalid("Mill batch lost its paid start, grade or monotone recipe identity");
            unsigned count=0;for(auto n:batch.ingredients){if(n>1)invalid("Mill recipe requires distinct physical ingredients");count+=n;}
            const auto required=food_processing_work_per_ingredient*batch.grade;
            if(count!=batch.grade||(finished ? batch.work!=required:batch.work>=required)||!batch.work||batch.work>through-batch.started_tick+1)
                invalid("Mill recipe work, actual ingredients or completion opportunity invalid");
            for(std::size_t g=0;g<7;++g)if(batch.ingredients[g]){
                if(batch.ingredients[g]>m.ingredients_consumed[batch.grade-1][g]||!m.first_ingredients[g]||batch.started_tick<=m.first_ingredients[g]->tick)
                    invalid("Mill batch input debit predates its actual physical ingredient arrival");
            }
            if(batch.sequence>UINT64_MAX/20||(batch.sequence-1)*20>batch.started_tick-b.placed_tick-1)
                invalid("Mill batch sequence cannot precede its own prior staffed jobs");
        };
        if(m.active){validate_batch(*m.active,false,tick);if(m.active->sequence!=m.started)invalid("Mill active recipe sequence is not its exact next job");}
        if(m.last_receipt){
            const auto& receipt=*m.last_receipt;validate_batch(receipt.batch,true,receipt.completed_tick);
            if(!m.completed||receipt.batch.sequence!=m.completed||receipt.completed_tick>tick||
               !m.completed_by_grade[receipt.batch.grade-1]||
               (m.active && (m.active->sequence!=receipt.batch.sequence+1 || m.active->started_tick<=receipt.completed_tick)))
                invalid("Mill completion receipt contradicts its actual latest batch");
        }else if(m.completed)invalid("Mill output requires its actual completed-work receipt");
        for(std::size_t g=0;g<4;++g){
            if(bool(m.completed_by_grade[g])!=m.first_completions[g].has_value())invalid("Every produced grade requires its immutable first completed recipe");
            if(!m.first_completions[g])continue;
            const auto& receipt=*m.first_completions[g];validate_batch(receipt.batch,true,receipt.completed_tick);
            if(receipt.batch.grade!=g+1||receipt.batch.sequence>m.completed||receipt.completed_tick>tick||
               (m.last_receipt&&(receipt.batch.sequence>m.last_receipt->batch.sequence||receipt.completed_tick>m.last_receipt->completed_tick))||
               (m.last_receipt&&receipt.batch.sequence==m.last_receipt->batch.sequence&&receipt!=*m.last_receipt))
                invalid("First grade completion contradicts actual Mill sequence, grade or last receipt");
            for(std::size_t p=0;p<g;++p)if(m.first_completions[p]){
                const auto& peer=*m.first_completions[p];
                if(peer.batch.sequence==receipt.batch.sequence||
                   (peer.batch.sequence<receipt.batch.sequence ? peer.completed_tick>=receipt.batch.started_tick:receipt.completed_tick>=peer.batch.started_tick))
                    invalid("First grade recipes overlap their one physical Mill work channel");
            }
        }
        // Each grade is unavailable to the recipe prefix before its own
        // retained first sequence. Four release-time suffix quotas close the
        // completed history without an unbounded per-job journal.
        for(const auto& first:m.first_completions)if(first){std::uint64_t suffix=0;
            for(std::size_t g=0;g<4;++g)if(m.first_completions[g]&&m.first_completions[g]->batch.sequence>=first->batch.sequence)add(suffix,m.completed_by_grade[g]);
            if(suffix>m.completed-first->batch.sequence+1)invalid("Completed Mill recipe prefix has no possible available grade");}
        std::array<const FoodProcessingBatch*,6> known{};std::size_t known_count=0;
        for(const auto& first:m.first_completions)if(first)known[known_count++]=&first->batch;
        if(m.last_receipt)known[known_count++]=&m.last_receipt->batch;
        if(m.active)known[known_count++]=&*m.active;
        std::sort(known.begin(),known.begin()+static_cast<std::ptrdiff_t>(known_count),[](const auto* a,const auto* b){return a->sequence<b->sequence;});
        std::array<std::array<std::uint64_t,7>,4> proved_inputs{};std::array<std::uint64_t,4> proved_jobs{};
        for(std::size_t k=0;k<known_count;++k){const auto& batch=*known[k];
            if(k&&known[k-1]->sequence==batch.sequence){if(*known[k-1]!=batch)invalid("One Mill recipe sequence has contradictory immutable batches");continue;}
            ++proved_jobs[batch.grade-1];for(std::size_t g=0;g<7;++g)proved_inputs[batch.grade-1][g]+=batch.ingredients[g];}
        for(std::size_t g=0;g<4;++g){const auto starts=m.completed_by_grade[g]+static_cast<unsigned>(m.active&&m.active->grade==g+1);
            if(proved_jobs[g]>starts)invalid("Known Mill jobs exceed their actual grade count");
            for(std::size_t input=0;input<7;++input)if(proved_inputs[g][input]>m.ingredients_consumed[g][input])invalid("Distinct retained Mill jobs reused one ingredient debit");}
        std::uint64_t completed=0,work=0;
        for(std::size_t grade=0;grade<4;++grade){
            const auto cycles=m.completed_by_grade[grade];add(completed,cycles);
            if(cycles>UINT64_MAX/(food_processing_work_per_ingredient*(grade+1)))invalid("Mill work history overflows");
            add(work,cycles*food_processing_work_per_ingredient*(grade+1));
            auto starts=cycles;if(m.active&&m.active->grade==grade+1)add(starts,1);
            if(starts>UINT64_MAX/(grade+1))invalid("Mill ingredient history overflows");
            std::uint64_t input=0;
            for(auto n:m.ingredients_consumed[grade]){if(n>starts)invalid("Mill ingredient category was debited twice in one recipe");add(input,n);}
            if(input!=starts*(grade+1))invalid("Mill consumed ingredients do not equal committed recipes");
        }
        if(m.last_receipt){const auto& receipt=*m.last_receipt;const auto required=std::uint64_t(food_processing_work_per_ingredient)*receipt.batch.grade;
            if(work>receipt.completed_tick-b.placed_tick || work<required || work-required>receipt.batch.started_tick-b.placed_tick-1)
                invalid("Latest Mill receipt borrowed staffed work from after its actual completion epoch");}
        if(m.active){if(work>m.active->started_tick-b.placed_tick-1)invalid("Active Mill recipe borrowed prior completed work from its own future");add(work,m.active->work);}
        if(completed!=m.completed||work!=m.productive_ticks)invalid("Mill work/output/active batch equation invalid");
        for(std::size_t g=0;g<7;++g){std::uint64_t input=0;for(const auto& row:m.ingredients_consumed)add(input,row[g]);
            if(stock.consumed[g]!=input)invalid("Mill inputs must be debited once in the sole Food authority");}
        for(std::size_t g=7;g<processed_food_count;++g)if(stock.consumed[g]||stock.received[g])
            invalid("Mill cannot consume or receive its own processed output");
    }
    for(std::size_t i=0;i<state.shops.size();++i){
        const auto& shop=state.shops[i];const auto& b=find(buildings,shop.shop_id,body_key);const auto& market=find(buildings,shop.market_id,body_key);
        if((i&&!(state.shops[i-1].shop_id<shop.shop_id))||b.kind!=Object::OriginalFoodShop66||!b.placed||market.kind!=Object::Market||!market.placed||
           !(market.id<b.id)||market.placed_tick>b.placed_tick||b.placed_tick>tick||
           footprint_distance(b.cell,{2,2},market.cell,{1,1})!=1)
            invalid("FoodShop requires its exact earlier paid cardinal-adjacent parent Market");
        for(std::size_t p=0;p<i;++p)if(state.shops[p].market_id==shop.market_id)invalid("A Market can bind only one paid FoodShop");
        const auto& stock=find(food.stocks,b.id,stock_key);
        for(std::size_t g=0;g<4;++g){
            if(bool(stock.received[7+g])!=shop.first_meals[g].has_value())invalid("Every Shop meal grade requires its first actual physical Mill receipt");
            if(!shop.first_meals[g])continue;
            const auto& arrival=*shop.first_meals[g];validate_arrival(arrival,food,buildings,b.id,7+g,tick);
            const auto& producer=find(state.mills,arrival.source,[](const auto& m){return m.mill_id;});
            if(!producer.first_completions[g]||arrival.tick<=producer.first_completions[g]->completed_tick||
               arrival.tick-producer.first_completions[g]->completed_tick<arrival_travel(buildings,arrival.source,b.id)-1)
                invalid("First Shop grade arrived before its actual Mill completion and physical transport");
        }
        std::optional<BuildingId> admitted_parent;
        for(const auto& candidate:buildings)if(candidate.placed&&candidate.kind==Object::Market&&candidate.id<b.id&&
            footprint_distance(b.cell,{2,2},candidate.cell,{1,1})==1){
            bool bound=false;for(std::size_t p=0;p<i;++p)bound|=state.shops[p].market_id==candidate.id;
            if(bound)continue;
            if(admitted_parent)invalid("Saved FoodShop had ambiguous unbound parents at its actual paid placement");
            admitted_parent=candidate.id;
        }
        if(admitted_parent!=shop.market_id)invalid("Saved FoodShop parent differs from its actual paid placement admission");
    }
    if(state.houses.size()>Rules::city_v10_household_limit)invalid("Meal recipient history exceeds the existing House bound");
    for(std::size_t i=0;i<state.houses.size();++i){
        const auto& row=state.houses[i];const auto& b=find(buildings,row.house_id,body_key);const auto& stock=find(food.stocks,row.house_id,stock_key);const auto& witness=row.first_receipt;
        if((i&&!(state.houses[i-1].house_id<row.house_id))||!original_house_kind(b.kind)||!b.placed||!witness.tick||witness.tick>tick||
           witness.tick<b.placed_tick||witness.tick-b.placed_tick<120||!witness.transfer.sequence||witness.transfer.sequence>witness.tick)
            invalid("First meal receipt lacks its sorted actual occupied-House identity and arrival time");
        if(!stock.quality_witness||witness.tick>stock.quality_witness->tick||
           (stock.quality_witness->transfer==witness.transfer&&*stock.quality_witness!=witness))
            invalid("Immutable first meal witness contradicts the retained full House arrival");
        std::uint8_t minimum=UINT8_MAX,maximum=0;std::uint64_t raw_received=0,raw_used_before=0;
        for(std::size_t g=0;g<7;++g){add(raw_received,stock.received[g]);add(raw_used_before,witness.consumed_before[g]);
            if(witness.consumed_before[g]>stock.received[g])invalid("First meal witness consumes raw Food before any physical reception");
            if(stock.received[g]){ProcessedFoodAmounts single{};single[g]=1;const auto score=agriculture_batch_quality(single).value;minimum=std::min(minimum,score);maximum=std::max(maximum,score);}}
        if(witness.previous_quality>90||(witness.previous_quality&&(raw_received<=raw_used_before||witness.previous_quality<minimum||witness.previous_quality>maximum))||
           raw_used_before>food_events(witness.tick-1)-food_events(b.placed_tick+119))
            invalid("First meal previous quality or consumed prefix has no possible actual raw Food origin");
        const auto first_event=next_food_event(witness.tick);
        std::uint8_t first_consumed_grade=0;
        for(std::size_t g=0;g<4;++g){
            if(bool(stock.received[7+g])!=row.first_by_grade[g].has_value())invalid("Every House meal grade requires its first actual Shop arrival");
            if(!row.first_by_grade[g])continue;
            const auto& arrival=*row.first_by_grade[g];validate_arrival(arrival,food,buildings,b.id,7+g,tick);
            const auto& shop=find(state.shops,arrival.source,[](const auto& value){return value.shop_id;});
            if(!shop.first_meals[g]||arrival.tick<=shop.first_meals[g]->tick||
               arrival.tick-shop.first_meals[g]->tick<arrival_travel(buildings,arrival.source,b.id)||arrival.tick<witness.tick)
                invalid("First House grade predates actual Shop input or physical outward transport");
            if(witness.accepted[7+g]&&(arrival.transfer!=witness.transfer||arrival.tick!=witness.tick||arrival.amount!=witness.accepted[7+g]))
                invalid("First meal full witness disagrees with its exact per-grade arrival");
            if(first_event&&arrival.tick<=*first_event&&!first_consumed_grade)first_consumed_grade=static_cast<std::uint8_t>(g+1);
        }
        if(first_event&&*first_event<=tick&&(!first_consumed_grade||!row.first_consumption))invalid("Occupied House skipped its first actual meal consumption event");
        std::uint64_t accepted=0,consumed=0;
        for(std::size_t g=0;g<processed_food_count;++g){
            if(witness.accepted[g]<0||witness.accepted[g]>4||static_cast<std::uint64_t>(witness.accepted[g])>stock.received[g]||witness.consumed_before[g]>stock.consumed[g])
                invalid("First meal arrival exceeds actual Food histories");
            if(g<7){if(witness.accepted[g])invalid("FoodShop cannot deliver raw ingredients as meals");}
            else {
                if(witness.consumed_before[g])invalid("First meal arrival follows invented earlier meal consumption");
                accepted+=static_cast<unsigned>(witness.accepted[g]);add(consumed,stock.consumed[g]);
            }
        }
        const auto trip=std::find_if(food.trips.begin(),food.trips.end(),[&](const auto& t){return t.transfer.courier_id==witness.transfer.courier_id;});
        if(!accepted||accepted>4||witness.accepted[11]||trip==food.trips.end()||witness.transfer.sequence>trip->transfer.sequence||
           find(buildings,trip->source,body_key).kind!=Object::OriginalFoodShop66)
            invalid("First meal receipt has no actual paid FoodShop supplier sequence");
        const auto& supplier=find(buildings,trip->source,body_key);
        if(witness.tick<=supplier.placed_tick||witness.transfer.sequence>witness.tick-supplier.placed_tick)
            invalid("First meal receipt predates its paid FoodShop supplier or possible real sequence");
        if(trip->transfer==witness.transfer&&(trip->target!=b.id||trip->phase!=OriginalGoodsTransferPhase::Completed||trip->delivered!=witness.accepted))
            invalid("First meal receipt contradicts its retained actual arrival");
        if(bool(consumed)!=row.first_consumption.has_value())invalid("Actual meal consumption must retain its immutable first witness");
        if(row.first_consumption){const auto& use=*row.first_consumption;
            if(!first_event||use.tick!=*first_event||use.tick>tick||use.grade!=first_consumed_grade||use.grade<1||use.grade>4||!stock.consumed[6+use.grade]||
               !((use.tick>=40&&(use.tick-40)%816==0)||(use.tick>=397&&(use.tick-397)%816==0)))
                invalid("First meal consumption lost its actual grade, sole event or arrival chronology");}
    }
    for(const auto& stock:food.stocks)if(original_house_kind(find(buildings,stock.building_id,body_key).kind)){
        bool received=false;for(std::size_t g=7;g<processed_food_count;++g)received|=stock.received[g]!=0;
        const auto at=std::lower_bound(state.houses.begin(),state.houses.end(),stock.building_id,[](const auto& row,BuildingId id){return row.house_id<id;});
        if(received!=(at!=state.houses.end()&&at->house_id==stock.building_id))invalid("House meal histories require exactly one actual first receipt");
    }

    if(publication)validate_first_transfer_keys(state);
}
}
Command set_mill_recipe(BuildingId id,std::uint8_t grade){Command c{CommandType::SetMillRecipe};c.building_id=id;c.recipe_grade=grade;return c;}
World World::create_food_processing(std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    auto w=create_irrigation_generation(kFoodProcessingRuleVersion,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.initialize_fishing_sites();w.initialize_hunting_habitats();w.validate_fishing_context();w.validate_hunting_context();return w;
}
World World::restore_food_processing(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> p,std::shared_ptr<const original::OriginalRulesModel> m,std::shared_ptr<const OriginalTerrainFacts> t){
    if(!simulation::food_processing_enabled(s.profile,s.rule_version))invalid("Food processing restore requires exact Rule34");
    validate_food_processing_snapshot(s);
    auto w=restore_irrigation_generation(kFoodProcessingRuleVersion,s,std::move(p),std::move(m),std::move(t));
    w.validate_food_processing_model();w.validate_fishing_context();w.validate_hunting_context();return w;
}
void World::validate_food_processing_model() const{
    if(!original_rules_model_)invalid("Food processing requires an explicitly prepared source model");
    for(const auto kind:{original::ModelBuilding::Mill,original::ModelBuilding::FoodShop}){
        if(!original_rules_model_->supports_building(kind)||original_rules_model_->building_value(kind,original::BuildingField::Cost)<=0)
            invalid("Mill and FoodShop require separately prepared positive source prices");
        const auto workers=original_rules_model_->building_value(kind,original::BuildingField::Employees);
        if(workers<=0||workers>INT16_MAX)invalid("Mill/FoodShop require bounded positive source staffing");
    }
}
std::optional<BuildingId> World::food_shop_parent(Cell cell) const{
    if(!food_processing_enabled()||!food_processing_)return {};
    std::optional<BuildingId> result;
    for(const auto& b:buildings_)if(b.placed&&b.kind==Object::Market&&footprint_distance(cell,{2,2},b.cell,{1,1})==1){
        if(std::any_of(food_processing_->shops.begin(),food_processing_->shops.end(),[&](const auto& shop){return shop.market_id==b.id;}))continue;
        if(result)return {};result=b.id;
    }
    return result;
}
int World::food_processing_output_reservation(BuildingId id) const{
    if(!food_processing_)return 0;
    const auto at=std::lower_bound(food_processing_->mills.begin(),food_processing_->mills.end(),id,[](const auto& m,BuildingId key){return m.mill_id<key;});
    return at!=food_processing_->mills.end()&&at->mill_id==id&&at->active ? at->active->grade:0;
}
void World::advance_food_processing(){
    for(auto& m:food_processing_->mills){
        const auto& b=building(m.mill_id);
        if(!b.operating_enabled||building_on_fire(b.id)||!building_staffed_for_tick(b.id)||building_entrances(b.id).empty())continue;
        auto& stock=find(processed_food_->stocks,b.id,stock_key);
        if(!m.active){
            if(m.started==UINT64_MAX||m.productive_ticks==UINT64_MAX)continue;
            FoodProcessingBatch batch;batch.sequence=m.started+1;batch.started_tick=ticks_;batch.grade=m.requested_grade;
            unsigned count=0;
            for(std::size_t g=0;g<7&&count<batch.grade;++g)if(stock.amounts[g]>0){batch.ingredients[g]=1;++count;}
            if(count!=batch.grade)continue;
            // All bounded job/output admission precedes the one physical debit.
            // Consumed input volume becomes reserved output volume immediately.
            for(std::size_t g=0;g<7;++g)if(batch.ingredients[g]&&
                (stock.consumed[g]==UINT64_MAX||m.ingredients_consumed[batch.grade-1][g]==UINT64_MAX))
                throw std::overflow_error("Mill ingredient debit history exhausted");
            m.active=batch;++m.started;
            for(std::size_t g=0;g<7;++g)if(batch.ingredients[g]){--stock.amounts[g];++stock.consumed[g];++m.ingredients_consumed[batch.grade-1][g];}
        }
        if(m.productive_ticks==UINT64_MAX)throw std::overflow_error("Mill productive work history exhausted");
        auto& batch=*m.active;++batch.work;++m.productive_ticks;
        if(batch.work==food_processing_work_per_ingredient*batch.grade){
            const auto index=7+batch.grade-1;
            if(m.completed==UINT64_MAX||m.completed_by_grade[batch.grade-1]==UINT64_MAX)throw std::overflow_error("Mill output history exhausted");
            stock.amounts[index]+=batch.grade;
            ++m.completed;++m.completed_by_grade[batch.grade-1];
            m.last_receipt=FoodProcessingReceipt{batch,ticks_};
            if(!m.first_completions[batch.grade-1])m.first_completions[batch.grade-1]=m.last_receipt;
            m.active.reset();
        }
    }
}
void World::validate_food_processing_snapshot(const WorldSnapshot& s){
    if(!simulation::food_processing_enabled(s.profile,s.rule_version)){
        if(s.processed_food||s.food_processing)invalid("Historical generations reject processed Food authority");return;
    }
    if(!s.processed_food||!s.food_processing||s.hunting_food||s.fishing_food||s.agriculture)invalid("Rule34 requires its sole graded Food authority");
    validate_processing(*s.food_processing,*s.processed_food,s.buildings,s.ticks,true);
}
void World::validate_food_processing_state() const{
    if(food_processing_enabled())validate_processing(*food_processing_,*processed_food_,buildings_,ticks_,false);
    else if(food_processing_||processed_food_)invalid("Historical World rejects Food processing state");
}
FoodProcessingStatus World::food_processing_status(BuildingId id) const{
    FoodProcessingStatus result;result.building_id=id;
    if(!food_processing_||!processed_food_)return result;
    const auto& b=building(id);
    if(b.kind!=Object::OriginalMill53&&b.kind!=Object::OriginalFoodShop66)return result;
    const auto& stock=find(processed_food_->stocks,id,stock_key);
    std::copy_n(stock.amounts.begin(),7,result.ingredients.begin());std::copy_n(stock.amounts.begin()+7,5,result.meals.begin());
    for(const auto& trip:processed_food_->trips){int remaining=0;for(std::size_t g=0;g<12;++g)remaining+=trip.loaded[g]-trip.delivered[g]-trip.returned[g];
        if(trip.target==id&&trip.phase==OriginalGoodsTransferPhase::Loaded)result.incoming+=remaining;
        if(trip.source==id)result.return_reserved+=remaining;}
    if(b.kind==Object::OriginalMill53){
        const auto& m=find(food_processing_->mills,id,[](const auto& row){return row.mill_id;});
        result.requested_grade=m.requested_grade;result.started=m.started;result.completed=m.completed;result.productive_ticks=m.productive_ticks;
        result.required_work=food_processing_work_per_ingredient*m.requested_grade;
        if(m.active){result.active_grade=m.active->grade;result.work=m.active->work;result.required_work=food_processing_work_per_ingredient*m.active->grade;result.processing_reserved=m.active->grade;}
    }else result.parent_market=find(food_processing_->shops,id,[](const auto& row){return row.shop_id;}).market_id;
    if(!b.operating_enabled)result.blocker="Operation paused";
    else if(building_on_fire(id))result.blocker="On fire";
    else if(!building_staffed_for_tick(id))result.blocker="Full source staffing required";
    else if(building_entrances(id).empty())result.blocker="No road access";
    else if(b.kind==Object::OriginalMill53&&!result.active_grade){unsigned distinct=0;for(auto n:result.ingredients)distinct+=n>0;
        int held=0;for(auto n:stock.amounts)held+=n;
        result.blocker=distinct<result.requested_grade ?
            (held+result.incoming+result.return_reserved>=16 ? "Full shared storage; waiting for physical output or return":"Waiting for distinct physical ingredients"):
            "Ready for next recipe";}
    else result.blocker=b.kind==Object::OriginalMill53 ? "Processing actual debited ingredients":"Physical meal distribution";
    return result;
}
} // namespace openemperor::simulation
