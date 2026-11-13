#include "simulation/World.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include "simulation/OriginalMarketPolicy.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
using original_industry_goods::capacity;
using original_industry_goods::typed;
using Context=original_industry_goods::Context;
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
bool carries(Object source,CourierRole role,original::Goods good) {
    if (role==CourierRole::OriginalHempToWarehouse) return good==original::Goods::Hemp;
    if (role==CourierRole::OriginalLuxuryToWarehouse) return original_industry_goods::output(source)==good;
    return true;
}
const OriginalGoodsReservation* row(const OriginalGoodsState& state,CourierId id) {
    const auto found=std::lower_bound(state.reservations.begin(),state.reservations.end(),id,
        [](const auto& r,CourierId key){return r.transfer.courier_id<key;});
    return found==state.reservations.end() || found->transfer.courier_id!=id ? nullptr:&*found;
}
int cargo(const OriginalGoodsState& state,CourierId id) {
    const auto found=std::lower_bound(state.cargo.begin(),state.cargo.end(),id,
        [](const auto& c,CourierId key){return c.transfer.courier_id<key;});
    return found==state.cargo.end() || found->transfer.courier_id!=id ? 0:found->amount;
}
int incoming(const OriginalGoodsState& goods,BuildingId target,original::Goods good) {
    std::int64_t total=0;
    for (const auto& r:goods.reservations) if (r.target==target && r.good==good) {
        if (r.phase==OriginalGoodsTransferPhase::Reserved) total+=r.reserved;
        else if (r.phase==OriginalGoodsTransferPhase::Loaded) total+=r.loaded-r.delivered-r.returned;
    }
    if (total>INT32_MAX) invalid("typed incoming reservations overflow");return static_cast<int>(total);
}
int returning_room(const OriginalGoodsState& goods,BuildingId source,original::Goods good) {
    std::int64_t total=0;for (const auto& c:goods.cargo) if (c.source==source && c.good==good) total+=c.amount;
    if (total>INT32_MAX) invalid("typed source return-room overflow");return static_cast<int>(total);
}
template<class Buildings,class Couriers> void validate_market_bindings(const Buildings& buildings,const Couriers& couriers,
    const OriginalGoodsState& goods,std::span<const OriginalHouseState> homes,
    std::span<const OriginalHouseConsumptionState> consumption) {
    const Context context(buildings,couriers);validate_original_goods_ledger(context.view(goods,homes));
    if (consumption.size()!=homes.size()) invalid("typed House consumption lost its purchased owner");
    for (std::size_t i=0;i<homes.size();++i)
        if (consumption[i].building_id!=homes[i].building_id)
            invalid("typed House consumption is not aligned with its sole inventory");
    for (std::size_t i=0;i<goods.reservations.size();++i) {
        const auto& r=goods.reservations[i];
        if ((i && goods.reservations[i-1].transfer.courier_id==r.transfer.courier_id) ||
            static_cast<std::size_t>(r.good)>=original::goods_count || r.phase==OriginalGoodsTransferPhase::Reserved ||
            r.phase==OriginalGoodsTransferPhase::Cancelled || r.reserved>Rules::courier_capacity ||
            r.reserved!=r.loaded ||
            (r.phase!=OriginalGoodsTransferPhase::Completed && r.delivered!=0))
            invalid("typed market must retain exactly the latest real loaded trip per courier");
        const auto c=std::find_if(couriers.begin(),couriers.end(),[&](const auto& value){return value.id==r.transfer.courier_id;});
        const auto source=std::find_if(buildings.begin(),buildings.end(),[&](const auto& b){return b.id==r.source;});
        const auto target=std::find_if(buildings.begin(),buildings.end(),[&](const auto& b){return b.id==r.target;});
        if (source==buildings.end() || c==couriers.end() || !typed(c->role) || c->owner!=r.source || c->target!=r.target ||
            c->last_dispatched_target!=r.target || !carries(source->kind,c->role,r.good))
            invalid("typed transfer is not owned by its actual courier and destination");
        if (target!=buildings.end() && original_house_kind(target->kind)) {
            const auto h=std::lower_bound(homes.begin(),homes.end(),target->id,[](const auto& value,BuildingId id){return value.building_id<id;});
            if (h==homes.end() || h->building_id!=target->id || h->house.occupants<=0)
                invalid("typed targeted dispatch requires a genuinely occupied House");
        }
    }
    for (const auto& c:couriers) if (typed(c.role)) {
        if (c.cargo || c.reserved || c.good!=(c.role==CourierRole::OriginalHempToWarehouse ? Good::Goods:Good::Pottery)) invalid("typed cargo is mirrored into legacy fields");
        const auto* r=row(goods,c.id);const auto amount=cargo(goods,c.id);
        if (!r) {
            if (c.phase!=CourierPhase::IdleAtWorkshop || c.last_dispatched_target || c.target!=c.owner)
                invalid("used typed courier lost its latest trip and sequence highwater");
            continue;
        }
        if (c.phase==CourierPhase::ToWarehouse) {
            if (r->phase!=OriginalGoodsTransferPhase::Loaded || amount<=0) invalid("typed outgoing movement lost its cargo");
        } else if (c.phase==CourierPhase::Returning) {
            if (!((r->phase==OriginalGoodsTransferPhase::Completed && !amount) ||
                (r->phase==OriginalGoodsTransferPhase::Returning && amount>0))) invalid("typed return phase and cargo disagree");
        } else if (amount || (r->phase!=OriginalGoodsTransferPhase::Completed && r->phase!=OriginalGoodsTransferPhase::Returned))
            invalid("idle typed courier has unfinished goods");
    }
    // Necessary arrival provenance from bounded retained history. The first
    // trip has no pruned predecessors; a later sequence can have delivered a
    // different good/target before its old terminal row was compacted.
    const auto possible_arrival=[&](BuildingId target,original::Goods good,CourierRole role) {
        for (const auto& c:couriers) if (c.role==role) {
            if (role==CourierRole::OriginalLuxuryToWarehouse) {
                const auto source=std::find_if(buildings.begin(),buildings.end(),[&](const auto& b){return b.id==c.owner;});
                if (source==buildings.end() || original_industry_goods::output(source->kind)!=good) continue;
            }
            const auto* latest=row(goods,c.id);if (!latest) continue;
            if (latest->transfer.sequence>1 || (latest->phase==OriginalGoodsTransferPhase::Completed &&
                latest->target==target && latest->good==good)) return true;
        }
        return false;
    };
    // Every commodity must first leave its actual producing domain. Old
    // unrelated courier rotations cannot create the first source handoff.
    for (std::size_t g=0;g<original::goods_count;++g) {
        if (g==static_cast<std::size_t>(original::Goods::Ceramics)) continue;
        const auto good=static_cast<original::Goods>(g);bool outside=false,delivered=false;
        for (std::size_t i=0;i<homes.size();++i)
            outside|=original::goods_stock(homes[i].house,good)>0 || consumption[i].consumed[g]>0;
        for (const auto& b:buildings) if (b.kind==Object::Warehouse || b.kind==Object::Market || b.kind==Object::OriginalPaperMaker45) {
            for (const auto& stock:goods.stocks) if (stock.building_id==b.id && stock.amounts[g]>0) outside=true;
            for (const auto& item:goods.cargo) if (item.source==b.id && item.good==good && item.amount>0) outside=true;
        }
        if (good==original::Goods::Hemp) {
            std::uint64_t house_used=0;
            for (const auto& entry:consumption) {
                if (entry.consumed[g]>UINT64_MAX-house_used) invalid("House Hemp sink overflows");
                house_used+=entry.consumed[g];
            }
            outside|=goods.totals[g].consumed>house_used;
        }
        for (const auto& c:couriers) if (c.role==(good==original::Goods::Hemp ?
            CourierRole::OriginalHempToWarehouse:CourierRole::OriginalLuxuryToWarehouse)) {
            if (c.role==CourierRole::OriginalLuxuryToWarehouse) {
                const auto source=std::find_if(buildings.begin(),buildings.end(),[&](const auto& b){return b.id==c.owner;});
                if (source==buildings.end() || original_industry_goods::output(source->kind)!=good) continue;
            }
            const auto* latest=row(goods,c.id);
            if (latest && (latest->transfer.sequence>1 || latest->phase==OriginalGoodsTransferPhase::Completed)) delivered=true;
        }
        if (outside && !delivered) invalid("goods outside their producing domain have no possible actual source handoff");
    }
    for (std::size_t i=0;i<homes.size();++i) for (std::size_t g=0;g<original::goods_count;++g) {
        const auto good=static_cast<original::Goods>(g);
        const auto& home=homes[i];
        const auto stock=static_cast<std::uint64_t>(original::goods_stock(home.house,good));
        if (consumption[i].consumed[g]>UINT64_MAX-stock) invalid("House received-goods history overflows");
        const auto received=stock+consumption[i].consumed[g];
        if (received && !possible_arrival(home.building_id,good,CourierRole::MarketPotteryDistribution))
            invalid("House stock or consumption has no possible actual Distributor arrival");
        std::uint64_t latest_delivered=0;bool pruned_history=false;
        for (const auto& c:couriers) if (c.role==CourierRole::MarketPotteryDistribution) {
            const auto* latest=row(goods,c.id);if (!latest) continue;
            pruned_history|=latest->transfer.sequence>1;
            if (latest->phase==OriginalGoodsTransferPhase::Completed && latest->target==home.building_id && latest->good==good)
                latest_delivered+=static_cast<std::uint64_t>(latest->delivered);
        }
        // Houses have no outbound stock: received = current stock + consumed.
        // Sum all actual retained deliveries, including independent Markets.
        // Only sequence one excludes possible pruned earlier target/good trips.
        if (received<latest_delivered || (!pruned_history && received>latest_delivered))
            invalid("House stock and consumption disagree with possible actual delivery history");
    }
    for (const auto& b:buildings) if (b.kind==Object::Warehouse || b.kind==Object::Market) {
        for (std::size_t g=0;g<original::goods_count;++g) {
            const auto good=static_cast<original::Goods>(g);
            if (b.kind==Object::Warehouse && good==original::Goods::Ceramics) continue;
            bool held=false;
            for (const auto& stock:goods.stocks) if (stock.building_id==b.id && stock.amounts[static_cast<std::size_t>(good)]>0) held=true;
            for (const auto& trip:goods.reservations) if (trip.source==b.id && trip.good==good && trip.loaded>0) held=true;
            if (held && !possible_arrival(b.id,good,b.kind==Object::Warehouse ?
                (good==original::Goods::Hemp ? CourierRole::OriginalHempToWarehouse:CourierRole::OriginalLuxuryToWarehouse):CourierRole::MarketPotteryInbound))
                invalid("typed distribution stock or cargo has no possible actual supplier arrival");
        }
    }
    for (const auto& b:buildings) if (b.kind==Object::OriginalPaperMaker45) {
        const auto stock=std::find_if(goods.stocks.begin(),goods.stocks.end(),[&](const auto& entry){return entry.building_id==b.id;});
        if (stock!=goods.stocks.end() && stock->amounts[0]>0 &&
            !possible_arrival(b.id,original::Goods::Hemp,CourierRole::MarketPotteryInbound))
            invalid("Paper Hemp input lacks an actual Warehouse supplier arrival");
    }
    for (const auto& b:buildings) if (original_industry_goods::owner(b.kind) && !original_house_kind(b.kind)) {
        const auto stock=std::find_if(goods.stocks.begin(),goods.stocks.end(),[&](const auto& s){return s.building_id==b.id;});
        if (stock==goods.stocks.end()) invalid("typed physical owner has no stock");
        for (std::size_t g=0;g<original::goods_count;++g) {
            const auto good=static_cast<original::Goods>(g);
            if (std::int64_t(stock->amounts[g])+returning_room(goods,b.id,good)+incoming(goods,b.id,good)+
                (b.kind==Object::Warehouse && good==original::Goods::Ceramics ? b.reserved_incoming:0)>capacity(b.kind,good))
                invalid("typed source stock, return-room and incoming commitments exceed physical capacity");
        }
    }
}
}

bool World::original_market_courier(CourierRole role) const {return original_production_enabled() && typed(role);}
int World::original_courier_cargo(CourierId id) const {
    if(original_plantations_enabled() && courier(id).role==CourierRole::OriginalPlantationGrower) return plantation_grower_cargo(id);
    if (!original_production_enabled()) return 0;
    const auto& c=courier(id);
    return original_food_courier(c.role) ? original_food_cargo(id):
        original_auxiliary_courier(c.role) ? original_auxiliary_cargo(id):cargo(*original_goods_,id);
}
std::optional<original::Goods> World::original_courier_good(CourierId id) const {
    if (!original_production_enabled()) return std::nullopt;
    (void)courier(id);for (const auto& c:original_goods_->cargo) if (c.transfer.courier_id==id) return c.good;
    return std::nullopt;
}
int World::original_market_headroom(BuildingId owner,original::Goods good) const {
    const auto& b=building(owner);
    return capacity(b.kind,good)-original_typed_stock(owner,good)-returning_room(*original_goods_,owner,good)-
        incoming(*original_goods_,owner,good)-(b.kind==Object::Warehouse && good==original::Goods::Ceramics ? b.reserved_incoming:0);
}
void World::validate_original_market_snapshot(const WorldSnapshot& s) {
    for (const auto& r:s.original_goods->reservations)
        if (r.transfer.sequence>s.ticks) invalid("typed courier history exceeds elapsed dispatch opportunities");
    validate_market_bindings(s.buildings,s.couriers,*s.original_goods,s.original_economy->houses,
        s.original_production->house_consumption);
}
void World::validate_original_market_state() const {
    for (const auto& r:original_goods_->reservations)
        if (r.transfer.sequence>ticks_) invalid("typed courier history exceeds elapsed dispatch opportunities");
    validate_market_bindings(buildings_,couriers_,*original_goods_,original_economy_->houses,
        original_production_->house_consumption);
}

std::pair<CourierDispatchDecision,std::optional<original::Goods>> World::original_needs_market_selection(const CourierState& c) const {
    const auto* previous=row(*original_goods_,c.id);
    if(previous&&previous->transfer.sequence==UINT64_MAX)return {{CourierDispatchStatus::NoStock,{}},{}};
    bool stocked=false;
    for(unsigned g=0;g<original::goods_count;++g)stocked|=original_typed_stock(c.owner,static_cast<original::Goods>(g))>0;
    if(!stocked)return {{CourierDispatchStatus::NoStock,{}},{}};
    std::array<const BuildingState*,Rules::city_v10_household_limit> targets{};std::size_t count=0;
    for(const auto& b:buildings_)if(courier_can_target(c.role,b)) {
        if(count==targets.size())invalid("needs-aware target bound exceeded");targets[count++]=&b;
    }
    if(!count)return {{CourierDispatchStatus::NoTarget,{}},{}};
    const auto pairs=count*original::goods_count;std::size_t start=0;
    if(previous) {
        const auto found=std::find_if(targets.begin(),targets.begin()+static_cast<std::ptrdiff_t>(count),
            [&](const auto* b){return b->id==previous->target;});
        if(found==targets.begin()+static_cast<std::ptrdiff_t>(count))invalid("needs-aware cursor lost its purchased target");
        start=(static_cast<std::size_t>(found-targets.begin())*original::goods_count+static_cast<std::size_t>(previous->good)+1)%pairs;
    }
    constexpr auto bound=Rules::city_v10_household_limit;
    std::array<std::array<std::optional<MarketNeed>,original::goods_count>,bound> needs{};
    const auto& homes=original_economy_->houses;
    if(homes.size()>bound)invalid("needs-aware House bound exceeded");
    for(std::size_t h=0;h<homes.size();++h) {
        const auto& body=building(homes[h].building_id);
        if(!body.operating_enabled||building_on_fire(body.id)||homes[h].house.occupants<=0)continue;
        auto actual=homes[h].house;
        actual.desirability=static_cast<std::int8_t>(original_house_influence_value(body.id));
        std::array<int,original::goods_count> commitments{};
        for(unsigned g=0;g<original::goods_count;++g)commitments[g]=incoming(*original_goods_,body.id,static_cast<original::Goods>(g));
        for(unsigned g=0;g<original::goods_count;++g)
            needs[h][g]=original_market_need(original_rules_model_->house_rules(),actual,static_cast<original::Goods>(g),commitments);
    }
    const auto has_route=[](const CourierState& actor,BuildingId target) {
        const auto route=std::lower_bound(actor.dynamic_target_routes.begin(),actor.dynamic_target_routes.end(),target,
            [](const auto& value,BuildingId id){return value.first<id;});
        return route!=actor.dynamic_target_routes.end()&&route->first==target&&route->second.has_value();
    };
    std::array<MarketChoice,bound*original::goods_count> choices{};std::size_t used=0;
    bool demand=false,room=false;
    for(std::size_t t=0;t<count;++t) {
        const auto& target=*targets[t];
        if(!target.operating_enabled||building_on_fire(target.id))continue;
        for(unsigned g=0;g<original::goods_count;++g) {
            const auto good=static_cast<original::Goods>(g);std::optional<MarketNeed> need;
            if(c.role==CourierRole::MarketPotteryDistribution) {
                const auto h=std::lower_bound(homes.begin(),homes.end(),target.id,[](const auto& home,BuildingId id){return home.building_id<id;});
                if(h!=homes.end()&&h->building_id==target.id)need=needs[static_cast<std::size_t>(h-homes.begin())][g];
            } else if(target.kind==Object::Market) {
                // The Warehouse supplier prepares only goods usable by a real
                // reachable household of this paid Market. Both transport legs
                // use the same need query and actual incoming commitments.
                const auto outbound=std::find_if(couriers_.begin(),couriers_.end(),[&](const auto& actor){
                    return actor.owner==target.id&&actor.role==CourierRole::MarketPotteryDistribution;});
                if(outbound==couriers_.end()||workers_assigned(target.id)<workforce_required(target.id))continue;
                const int pool=original_typed_stock(target.id,good)+incoming(*original_goods_,target.id,good);
                if(pool>Rules::courier_capacity)continue;
                for(std::size_t h=0;h<homes.size();++h)if(needs[h][g]&&has_route(*outbound,homes[h].building_id)) {
                    const auto& wanted=*needs[h][g];
                    if(!need||wanted.tier<need->tier)need=MarketNeed{wanted.tier,pool+wanted.stock,wanted.demand};
                    else if(wanted.tier==need->tier){need->stock+=wanted.stock;need->demand+=wanted.demand;}
                }
            } else if(target.kind==Object::OriginalPaperMaker45&&good==original::Goods::Hemp&&
                      workers_assigned(target.id)==workforce_required(target.id)) {
                // Keep the existing paid Paper input chain possible. It has a
                // real staffed producer; the two-unit input threshold is an
                // authored buffer, not a changed recipe quantity. No Paper
                // or Confucian service is granted by this supply decision.
                const int pool=original_typed_stock(target.id,good)+incoming(*original_goods_,target.id,good);
                if(pool<=Rules::courier_capacity)need=MarketNeed{pool<2?0U:1U,pool,2};
            }
            if(!need)continue;demand=true;
            if(capacity(target.kind,good)<=0||original_typed_stock(c.owner,good)<=0||original_market_headroom(target.id,good)<=0)continue;
            room=true;if(!has_route(c,target.id))continue;
            choices[used++]={t*original::goods_count+g,*need};
        }
    }
    const auto chosen=choose_original_market_pair(std::span(choices).first(used),start,pairs);
    if(!chosen)return {{!demand?CourierDispatchStatus::NoTarget:room?CourierDispatchStatus::NoRoad:CourierDispatchStatus::TargetFull,{}},{}};
    return {{CourierDispatchStatus::Ready,targets[*chosen/original::goods_count]->id},static_cast<original::Goods>(*chosen%original::goods_count)};
}

CourierDispatchDecision World::original_market_dispatch_status(const CourierState& c) const {
    if(original_market_selection_enabled()&&(c.role==CourierRole::MarketPotteryInbound||c.role==CourierRole::MarketPotteryDistribution))
        return original_needs_market_selection(c).first;
    const auto& source=building(c.owner);
    const auto* previous=row(*original_goods_,c.id);
    if (previous && previous->transfer.sequence==UINT64_MAX) return {CourierDispatchStatus::NoStock,{}};
    bool stocked=false;
    for (std::size_t g=0;g<original::goods_count;++g) {
        const auto good=static_cast<original::Goods>(g);
        stocked|=carries(source.kind,c.role,good) && original_typed_stock(c.owner,good)>0;
    }
    if (!stocked) return {CourierDispatchStatus::NoStock,{}};
    // Keep the complete canonical ID list stable while fire, staffing, stock or
    // access changes. The actual latest trip is the sole saved cursor over
    // (target, commodity); no second clock or per-frame mutation is needed.
    std::array<const BuildingState*,Rules::city_v10_household_limit> targets{};std::size_t count=0;
    for (const auto& b:buildings_) if (courier_can_target(c.role,b)) {
        if (count==targets.size()) invalid("typed target bound exceeded");
        targets[count++]=&b;
    }
    if (!count) return {CourierDispatchStatus::NoTarget,{}};
    const auto pairs=count*original::goods_count;std::size_t start=0;
    if (previous) {
        const auto at=std::find_if(targets.begin(),targets.begin()+static_cast<std::ptrdiff_t>(count),
            [&](const auto* b){return b->id==previous->target;});
        if (at==targets.begin()+static_cast<std::ptrdiff_t>(count)) invalid("typed cursor lost its purchased target");
        start=(static_cast<std::size_t>(at-targets.begin())*original::goods_count+
            static_cast<std::size_t>(previous->good)+1)%pairs;
    }
    bool eligible=false,room=false;
    for (std::size_t offset=0;offset<pairs;++offset) {
        const auto index=(start+offset)%pairs;const auto& target=*targets[index/original::goods_count];
        const auto good=static_cast<original::Goods>(index%original::goods_count);
        if (!carries(source.kind,c.role,good) || capacity(target.kind,good)<=0) continue;
        if (original_house_kind(target.kind)) {
            const auto& homes=original_economy_->houses;
            const auto h=std::lower_bound(homes.begin(),homes.end(),target.id,
                [](const auto& value,BuildingId id){return value.building_id<id;});
            if (h==homes.end() || h->building_id!=target.id || h->house.occupants<=0) continue;
        }
        // Reserved Paper input may arrive while its recipe is stopped/burning.
        if (target.kind!=Object::OriginalPaperMaker45 && (!target.operating_enabled || building_on_fire(target.id))) continue;
        eligible=true;
        if (original_typed_stock(c.owner,good)<=0 || original_market_headroom(target.id,good)<=0) continue;
        room=true;
        const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target.id,
            [](const auto& entry,BuildingId id){return entry.first<id;});
        if (route!=c.dynamic_target_routes.end() && route->first==target.id && route->second)
            return {CourierDispatchStatus::Ready,target.id};
    }
    return {!eligible ? CourierDispatchStatus::NoTarget:room ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::TargetFull,{}};
}

std::optional<original::Goods> World::original_market_dispatch_good(const CourierState& c,BuildingId target) const {
    if(original_market_selection_enabled()&&(c.role==CourierRole::MarketPotteryInbound||c.role==CourierRole::MarketPotteryDistribution)) {
        const auto selection=original_needs_market_selection(c);
        return selection.first.selected_target==target?selection.second:std::nullopt;
    }
    const auto* previous=row(*original_goods_,c.id);
    // A different selected target is entered at column0. Returning to the
    // previous target continues after its previous good, including full wrap.
    const auto first=previous && previous->target==target ?
        (static_cast<std::size_t>(previous->good)+1)%original::goods_count:0;
    for (std::size_t offset=0;offset<original::goods_count;++offset) {
        const auto good=static_cast<original::Goods>((first+offset)%original::goods_count);
        if (carries(building(c.owner).kind,c.role,good) && capacity(building(target).kind,good)>0 &&
            original_typed_stock(c.owner,good)>0 && original_market_headroom(target,good)>0) return good;
    }
    return std::nullopt;
}

void World::dispatch_original_market(CourierState& c) {
    const auto decision=courier_dispatch_status(c.id);
    if (decision.status!=CourierDispatchStatus::Ready || !decision.selected_target) return;
    const auto target=*decision.selected_target;
    const auto route=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target,
        [](const auto& entry,BuildingId id){return entry.first<id;});
    if (route==c.dynamic_target_routes.end() || !route->second) throw std::logic_error("typed dispatch lost cached route");
    const auto selected=original_market_dispatch_good(c,target);if (!selected) return;const auto good=*selected;
    const auto amount=std::min({Rules::courier_capacity,original_typed_stock(c.owner,good),original_market_headroom(target,good)});
    const auto* previous=row(*original_goods_,c.id);
    if (previous && previous->transfer.sequence==UINT64_MAX) return;
    const OriginalGoodsTransferKey key{c.id,previous ? previous->transfer.sequence+1:1};
    const Context context(buildings_,couriers_);
    // Sixteen real typed actors and the temporary successor fit the unchanged
    // pure ledger. The old highwater is replaced only
    // after its successor exists, and stale handles can never name that trip.
    auto candidate=original_goods_reserve(context.view(*original_goods_,original_economy_->houses),key,
        c.owner,target,good,amount);
    std::erase_if(candidate.goods.reservations,[&](const auto& r){return r.transfer.courier_id==c.id && r.transfer!=key;});
    candidate=original_goods_load(context.view(candidate.goods,candidate.houses),key,amount);
    auto path=*route->second; // Allocate before authoritative publication.
    original_goods_=std::move(candidate.goods);
    c.path=std::move(path);c.target=target;c.last_dispatched_target=target;
    c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();
    c.phase=CourierPhase::ToWarehouse;
}

void World::cancel_original_market(BuildingId owner) {
    for (auto& c:couriers_) if (c.owner==owner && original_market_courier(c.role) && c.phase==CourierPhase::ToWarehouse) {
        const auto* trip=row(*original_goods_,c.id);if (!trip) throw std::logic_error("loaded typed actor has no trip");
        const auto key=trip->transfer;const Context context(buildings_,couriers_);
        auto candidate=original_goods_cancel(context.view(*original_goods_,original_economy_->houses),key);
        const auto here=c.path.at(c.path_vertex);
        const auto& source=building(owner);
        if (!c.edge_progress && building_footprint_contains(profile_,rule_version_,source.kind,source.cell,here)) {
            candidate=original_goods_return(context.view(candidate.goods,candidate.houses),key,cargo(candidate.goods,c.id));
            original_goods_=std::move(candidate.goods);c.path.clear();c.path_vertex=0;c.edge_progress=0;
            c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::IdleAtWorkshop;
        } else {
            original_goods_=std::move(candidate.goods);c.phase=CourierPhase::Returning;
            mark_route_pending(c); // A begun edge remains exact until its next vertex.
            if (!c.edge_progress) reroute_v2(c);
        }
    }
}

void World::arrive_original_market(CourierState& c) {
    const auto* trip=row(*original_goods_,c.id);if (!trip) throw std::logic_error("typed arrival has no latest trip");
    const auto key=trip->transfer;const Context context(buildings_,couriers_);
    if (c.phase==CourierPhase::ToWarehouse) {
        const auto& target=building(c.target);
        const auto amount=cargo(*original_goods_,c.id);
        OriginalGoodsLedgerSnapshot candidate;
        if (target.kind!=Object::OriginalPaperMaker45 && (!target.operating_enabled || building_on_fire(target.id)))
            candidate=original_goods_cancel(context.view(*original_goods_,original_economy_->houses),key);
        else candidate=original_goods_deliver(context.view(*original_goods_,original_economy_->houses),key,amount);
        original_goods_=std::move(candidate.goods);original_economy_->houses=std::move(candidate.houses);
        c.phase=CourierPhase::Returning;
        if (valid_return_path(c)) {std::reverse(c.path.begin(),c.path.end());c.path_vertex=0;}
        else {c.path={c.path.back()};c.path_vertex=0;c.route_pending=true;c.route_checked_revision.reset();reroute_v2(c);}
    } else {
        const auto amount=cargo(*original_goods_,c.id);
        if (amount) {
            auto candidate=original_goods_return(context.view(*original_goods_,original_economy_->houses),key,amount);
            original_goods_=std::move(candidate.goods);
        }
        c.path.clear();c.path_vertex=0;c.phase=CourierPhase::IdleAtWorkshop;c.route_pending=false;c.route_checked_revision.reset();
    }
}

} // namespace openemperor::simulation
