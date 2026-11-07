#include "simulation/World.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* why) {throw std::invalid_argument(why);}
std::size_t column(OriginalAuxMaterial material) {
    const auto g=static_cast<std::size_t>(material);
    if (g>=original_aux_material_count) invalid("unknown auxiliary material");
    return g;
}
void add(std::uint64_t& value,std::uint64_t amount) {
    if (amount>UINT64_MAX-value) invalid("industry counter overflow");
    value+=amount;
}
template<class Rows> decltype(auto) account(Rows& rows,BuildingId id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.building_id<key;});
    if (at==rows.end() || at->building_id!=id) invalid("industry owner account missing");
    return *at;
}
template<class Rows> decltype(auto) body(Rows& rows,BuildingId id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.id<key;});
    if (at==rows.end() || at->id!=id) invalid("industry reference has no paid body");
    return *at;
}
template<class Rows> auto trip_for(Rows& rows,CourierId actor) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),actor,[](const auto& row,CourierId key){return row.transfer.courier_id<key;});
    return at==rows.end() || at->transfer.courier_id!=actor ? nullptr:&*at;
}
std::optional<OriginalAuxMaterial> output(Object kind) {
    switch (kind) {
    case Object::OriginalLoggingShed38:return OriginalAuxMaterial::Wood;
    case Object::OriginalBronzeSmelter39:return OriginalAuxMaterial::Bronze;
    case Object::OriginalPaperMaker45:return OriginalAuxMaterial::Paper;
    case Object::OriginalLacquerRefinery238:return OriginalAuxMaterial::Lacquer;
    case Object::OriginalSilkwormShed239:return OriginalAuxMaterial::RawSilk;
    default:return std::nullopt;
    }
}
bool input(Object kind,OriginalAuxMaterial material) {
    return (kind==Object::OriginalBronzeSmelter39 && material==OriginalAuxMaterial::Wood) ||
        (kind==Object::OriginalBronzewareMaker42 && material==OriginalAuxMaterial::Bronze) ||
        (kind==Object::OriginalLacquerwareMaker44 && (material==OriginalAuxMaterial::Wood || material==OriginalAuxMaterial::Lacquer)) ||
        (kind==Object::OriginalWeaver47 && material==OriginalAuxMaterial::RawSilk) ||
        (kind==Object::OriginalConfucianAcademy219 && material==OriginalAuxMaterial::Paper);
}
bool auxiliary_owner(Object kind) {
    return kind==Object::Warehouse || kind==Object::OriginalConfucianAcademy219 ||
        (original_industry_source(kind) && kind!=Object::OriginalTeaCuringShed237);
}
int capacity(Object kind,OriginalAuxMaterial material) {
    return kind==Object::Warehouse ? 32:(input(kind,material) || output(kind)==material ? 8:0);
}
int remaining(const OriginalAuxTrip& trip) {
    const auto n=std::int64_t(trip.loaded)-trip.delivered-trip.returned;
    if (n<0 || n>Rules::courier_capacity) invalid("auxiliary trip cargo outside cart capacity");
    return static_cast<int>(n);
}
int incoming(const OriginalIndustryState& state,BuildingId id,OriginalAuxMaterial material) {
    int amount=0;
    for (const auto& trip:state.aux_trips)
        if (trip.target==id && trip.material==material && trip.phase==OriginalGoodsTransferPhase::Loaded) amount+=remaining(trip);
    return amount;
}
int return_room(const OriginalIndustryState& state,BuildingId id,OriginalAuxMaterial material) {
    int amount=0;
    for (const auto& trip:state.aux_trips) if (trip.source==id && trip.material==material) amount+=remaining(trip);
    return amount;
}
int headroom(const OriginalIndustryState& state,BuildingId id,Object kind,OriginalAuxMaterial material) {
    return capacity(kind,material)-account(state.auxiliary,id).amounts[column(material)]-
        incoming(state,id,material)-return_room(state,id,material);
}
Object plant_for(Object kind) {
    if (kind==Object::OriginalTeaCuringShed237) return Object::OriginalTeaBush26;
    if (kind==Object::OriginalLacquerRefinery238) return Object::OriginalLacquerTree27;
    if (kind==Object::OriginalSilkwormShed239) return Object::OriginalMulberryTree28;
    return Object::Empty;
}
bool adjacent(Cell origin,Cell plant) {
    const std::int64_t x=origin.x,y=origin.y,px=plant.x,py=plant.y;
    return std::max({x-px,std::int64_t{0},px-(x+1)})+
        std::max({y-py,std::int64_t{0},py-(y+1)})==1;
}
template<class Building> std::uint64_t travel(const Building& a,const Building& b) {
    const auto from=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,a.kind);
    const auto to=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,b.kind);
    const auto gap=[](int a0,int as,int b0,int bs) {
        const std::int64_t x=a0,y=b0;
        return std::max({std::int64_t{0},x-(y+bs-1),y-(x+as-1)});
    };
    return static_cast<std::uint64_t>(std::max<std::int64_t>(2,
        gap(a.cell.x,from.width,b.cell.x,to.width)+gap(a.cell.y,from.height,b.cell.y,to.height)))*
        static_cast<std::uint64_t>(profile_rules(RulesProfile::CityV16,kOriginalProductionRuleVersion).courier_edge_ticks);
}
template<class Building,class Courier> std::uint64_t moved(const Building& start,const Courier& actor) {
    const auto edge=profile_rules(RulesProfile::CityV16,kOriginalProductionRuleVersion).courier_edge_ticks;
    if (actor.path_vertex>=actor.path.size() || actor.edge_progress<0 || actor.edge_progress>=edge)
        invalid("industry movement has no physical vertex");
    const auto at=actor.path[actor.path_vertex];
    const auto size=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,start.kind);
    const auto gap=[](int a,int n,int b) {
        const auto d=static_cast<std::int64_t>(b)-a;
        return d<0 ? -d:std::max<std::int64_t>(0,d-n+1);
    };
    return static_cast<std::uint64_t>(gap(start.cell.x,size.width,at.x)+gap(start.cell.y,size.height,at.y))*static_cast<std::uint64_t>(edge)+
        static_cast<std::uint64_t>(actor.edge_progress);
}
template<class Buildings,class Couriers>
void validate_retired_clay(const OriginalSafeDemolitionState& history,const Buildings& buildings,
    const Couriers& couriers,std::uint64_t tick) {
    for (const auto& entry:history.retired) {
        if (entry.kind!=OriginalRetiredKind::ClaySource) continue;
        if (!entry.courier_id || entry.removed_tick>tick ||
            static_cast<bool>(entry.clay_trip)!=(entry.clay_extracted!=0) ||
            std::any_of(couriers.begin(),couriers.end(),[&](const auto& actor){return actor.id==*entry.courier_id;}))
            invalid("retired Clay source lost its exclusive closed actor history");
        if (!entry.clay_trip) continue;
        const auto& trip=*entry.clay_trip;
        const auto& target=body(buildings,trip.target);
        auto source=target;source.id=entry.building_id;source.kind=Object::ClaySource;
        source.cell={entry.origin_x,entry.origin_y};
        if (target.kind!=Object::Pottery || trip.source!=entry.building_id ||
            trip.transfer.courier_id!=*entry.courier_id || !trip.transfer.sequence ||
            trip.transfer.sequence>trip.dispatched_tick || trip.dispatched_tick<=entry.placed_tick ||
            trip.dispatched_tick<target.placed_tick || !trip.arrival_tick ||
            *trip.arrival_tick<=trip.dispatched_tick || *trip.arrival_tick>entry.removed_tick ||
            trip.loaded<=0 || trip.loaded>Rules::courier_capacity || trip.delivered!=trip.loaded ||
            static_cast<std::uint64_t>(trip.loaded)>entry.clay_extracted)
            invalid("retired Clay trip is not a completely settled ordinary Pottery delivery");
        const auto earlier=trip.transfer.sequence-1;
        const auto previous_units=entry.clay_extracted-static_cast<std::uint64_t>(trip.loaded);
        if (earlier>previous_units || previous_units/Rules::courier_capacity+
            (previous_units%Rules::courier_capacity!=0)>earlier ||
            *trip.arrival_tick-trip.dispatched_tick<travel(source,target)-1 ||
            entry.removed_tick-*trip.arrival_tick<travel(target,source))
            invalid("retired Clay sequence, extraction or physical return history differs");
    }
}
template<class Building> void bounded_body(const Building& b,int width,int height) {
    const auto size=building_footprint(RulesProfile::CityV16,kOriginalProductionRuleVersion,b.kind);
    if (!b.placed || b.cell.x<0 || b.cell.y<0 || std::int64_t(b.cell.x)+size.width>width ||
        std::int64_t(b.cell.y)+size.height>height) invalid("industry footprint exceeds map bounds");
}
template<class Buildings,class Couriers>
void validate_industry(const OriginalIndustryState& state,const Buildings& buildings,const Couriers& couriers,
    const OriginalGoodsState& goods,const OriginalServicesState& services,int width,int height,std::uint64_t tick) {
    if (state.sources.size()>original_industry_source_limit || state.managed_ore.size()>original_managed_ore_limit ||
        state.auxiliary.size()>original_aux_owner_limit || state.aux_trips.size()>original_aux_courier_limit ||
        state.clay_trips.size()>original_clay_trip_limit) invalid("industry finite authority exceeded");
    std::size_t sources=0,plants=0,owners=0,actors=0,smelters=0;
    for (const auto& b:buildings) {
        sources+=original_industry_source(b.kind);plants+=original_industry_plant(b.kind);
        owners+=auxiliary_owner(b.kind);smelters+=b.kind==Object::OriginalBronzeSmelter39;
        if (original_industry_source(b.kind) || original_industry_plant(b.kind)) bounded_body(b,width,height);
    }
    for (const auto& c:couriers) actors+=c.role==CourierRole::OriginalAuxToWarehouse || c.role==CourierRole::OriginalAuxSupplier;
    if (sources!=state.sources.size() || owners!=state.auxiliary.size() || smelters!=state.managed_ore.size() ||
        actors>original_aux_courier_limit) invalid("industry purchased bodies/accounts are incomplete");
    std::array<unsigned,9> counts{};std::array<BuildingId,6> assigned{};std::size_t linked=0;
    for (std::size_t i=0;i<state.sources.size();++i) {
        const auto& s=state.sources[i];const auto& b=body(buildings,s.building_id);
        if ((i && !(state.sources[i-1].building_id<s.building_id)) || !original_industry_source(b.kind) ||
            b.placed_tick>tick || s.progress<0 || s.progress>=80) invalid("industry source identity/progress invalid");
        const auto index=static_cast<std::size_t>(b.kind)-static_cast<std::size_t>(Object::OriginalLoggingShed38);
        if (index>=counts.size() || ++counts[index]>2) invalid("industry producer kind limit exceeded");
        const auto role=output(b.kind) ? CourierRole::OriginalAuxToWarehouse:CourierRole::OriginalLuxuryToWarehouse;
        if (std::count_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.owner==b.id && c.role==role;})!=1)
            invalid("industry source requires its own fixed-domain output actor");
        std::uint64_t start=b.placed_tick;
        if (s.plant_id) {
            const auto& p=body(buildings,*s.plant_id);
            if (plant_for(b.kind)==Object::Empty || p.kind!=plant_for(b.kind) || p.id<=b.id ||
                p.placed_tick<b.placed_tick || p.placed_tick>tick || !adjacent(b.cell,p.cell) ||
                linked==assigned.size() || std::find(assigned.begin(),assigned.begin()+static_cast<std::ptrdiff_t>(linked),p.id)!=assigned.begin()+static_cast<std::ptrdiff_t>(linked))
                invalid("industry plant must uniquely adjoin its paid matching orchard");
            assigned[linked++]=p.id;start=p.placed_tick;
        } else if (plant_for(b.kind)!=Object::Empty && (s.completed || s.progress)) invalid("orchard has no purchased plant origin");
        const auto age=tick-start;
        if (s.completed>age/80 || (s.completed==age/80 && static_cast<std::uint64_t>(s.progress)>age%80))
            invalid("industry completed output exceeds productive paid age");
        for (std::size_t g=0;g<original::goods_count;++g)
            if (s.goods_inputs_used[g]!=(b.kind==Object::OriginalPaperMaker45 && g==0 ? s.completed:0))
                invalid("industry six-goods recipe input counter differs from completed recipe");
        for (std::size_t g=0;g<original_aux_material_count;++g)
            if (s.aux_inputs_used[g]!=(input(b.kind,static_cast<OriginalAuxMaterial>(g)) ? s.completed:0))
                invalid("industry auxiliary input counter differs from completed recipe");
        if (s.clay_used!=(b.kind==Object::OriginalBronzewareMaker42 ? s.completed:0)) invalid("industry Clay recipe sink mismatch");
        if (b.kind==Object::OriginalBronzewareMaker42) {
            std::uint64_t received=s.clay_used;add(received,static_cast<std::uint64_t>(b.input_clay));
            if (b.input_clay<0 || b.input_clay>8 || b.reserved_incoming<0 || b.input_clay+b.reserved_incoming>8 || received!=s.clay_received)
                invalid("Bronzeware sole Clay input and received/used history disagree");
            if (static_cast<bool>(s.last_clay_arrival)!=(s.clay_received>0)) invalid("Bronzeware received Clay lacks actual arrival witness");
            if (s.last_clay_arrival) {
                const auto& w=*s.last_clay_arrival;const auto* trip=trip_for(state.clay_trips,w.transfer.courier_id);
                const auto& from=body(buildings,w.source);
                if (!trip || trip->source!=w.source || !w.transfer.sequence || w.transfer.sequence>trip->transfer.sequence || from.kind!=Object::ClaySource ||
                    w.amount<=0 || w.amount>4 || static_cast<std::uint64_t>(w.amount)>s.clay_received || w.arrival_tick>tick ||
                    w.arrival_tick<=from.placed_tick || w.arrival_tick<b.placed_tick ||
                    w.arrival_tick-from.placed_tick<travel(from,b) || w.transfer.sequence>w.arrival_tick)
                    invalid("Bronzeware Clay witness is not a possible actual purchased-source trip");
                if (w.transfer==trip->transfer && (trip->target!=b.id || trip->source!=w.source ||
                    trip->arrival_tick!=w.arrival_tick || trip->delivered!=w.amount)) invalid("Bronzeware latest Clay witness disagrees with retained trip");
                if (w.transfer.sequence<trip->transfer.sequence && w.arrival_tick>=trip->dispatched_tick)
                    invalid("retained Bronze Clay receipt postdates its actor's later trip");
            }
            std::uint64_t latest=0;bool pruned=false;
            for (const auto& trip:state.clay_trips) {
                pruned|=trip.transfer.sequence>1;
                if (trip.target!=b.id || !trip.arrival_tick) continue;
                add(latest,static_cast<std::uint64_t>(trip.delivered));
                if (!s.last_clay_arrival || *trip.arrival_tick>s.last_clay_arrival->arrival_tick)
                    invalid("Bronzeware lost its latest actual native Clay receipt");
            }
            if (s.clay_received<latest || (!pruned && s.clay_received!=latest))
                invalid("Bronzeware Clay balance exceeds possible actual supplier arrivals");
        } else if (s.clay_received || s.last_clay_arrival) invalid("non-Clay recipe invented native Clay receipt");
        if (b.kind==Object::OriginalPaperMaker45) {
            const auto stock=std::find_if(goods.stocks.begin(),goods.stocks.end(),[&](const auto& row){return row.building_id==b.id;});
            if (stock==goods.stocks.end() || stock->amounts[0]<0) invalid("Paper recipe lost its sole Hemp input");
            std::uint64_t received=s.goods_inputs_used[0];add(received,static_cast<std::uint64_t>(stock->amounts[0]));
            std::uint64_t latest=0;bool pruned=false;
            for (const auto& trip:goods.reservations) {
                const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==trip.transfer.courier_id;});
                if (actor==couriers.end() || actor->role!=CourierRole::MarketPotteryInbound) continue;
                pruned|=trip.transfer.sequence>1;
                if (trip.target==b.id && trip.good==original::Goods::Hemp) add(latest,static_cast<std::uint64_t>(trip.delivered));
            }
            if (received<latest || (!pruned && received!=latest)) invalid("Paper Hemp sink has no possible actual Warehouse receipt");
        }
        if (const auto good=original_industry_goods::output(b.kind)) {
            const auto g=static_cast<std::size_t>(*good);std::uint64_t held=0;
            for (const auto& stock:goods.stocks) if (stock.building_id==b.id) add(held,static_cast<std::uint64_t>(stock.amounts[g]));
            for (const auto& cargo:goods.cargo) if (cargo.source==b.id) {
                if (cargo.good!=*good) invalid("luxury output actor changed its source commodity");
                add(held,static_cast<std::uint64_t>(cargo.amount));
            }
            if (held>s.completed) invalid("luxury producer inventory exceeds its own completed origin");
            for (const auto& trip:goods.reservations) if (trip.source==b.id) {
                if (trip.good!=*good || trip.loaded<0 || static_cast<std::uint64_t>(trip.loaded)>s.completed ||
                    static_cast<std::uint64_t>(trip.delivered)>s.completed-held)
                    invalid("luxury output history exceeds its own completed origin");
            }
        }
    }
    if (linked!=plants) invalid("orphan industry plant");
    for (std::size_t i=0;i<state.managed_ore.size();++i) {
        const auto& ore=state.managed_ore[i];const auto& b=body(buildings,ore.building_id);const auto& source=account(state.sources,b.id);
        if ((i && !(state.managed_ore[i-1].building_id<ore.building_id)) || b.kind!=Object::OriginalBronzeSmelter39 ||
            ore.stock<0 || ore.stock>8 || ore.progress<0 || ore.progress>=80 || (ore.stock==8 && ore.progress) ||
            ore.used!=source.completed || ore.extracted<ore.used || ore.extracted-ore.used!=static_cast<std::uint64_t>(ore.stock))
            invalid("managed Ore is not bound to its sole paid Smelter and recipe sink");
        std::uint64_t cycles=ore.extracted;add(cycles,source.completed);
        const auto age=tick-b.placed_tick;
        if (cycles>age/80 || (cycles==age/80 && static_cast<std::uint64_t>(ore.progress+source.progress)>age%80))
            invalid("managed Ore plus recipe work exceeds actual paid age");
    }
    OriginalAuxCounters produced{},stocks{},cargo{},consumed{},received{},departed{},returned{};
    for (const auto& s:state.sources) if (const auto material=output(body(buildings,s.building_id).kind)) add(produced[column(*material)],s.completed);
    for (std::size_t i=0;i<state.auxiliary.size();++i) {
        const auto& stock=state.auxiliary[i];const auto& b=body(buildings,stock.building_id);
        if ((i && !(state.auxiliary[i-1].building_id<stock.building_id)) || !auxiliary_owner(b.kind)) invalid("auxiliary owner identity invalid");
        for (std::size_t g=0;g<original_aux_material_count;++g) {
            const auto material=static_cast<OriginalAuxMaterial>(g);
            if (stock.amounts[g]<0 || stock.amounts[g]>capacity(b.kind,material) ||
                std::int64_t(stock.amounts[g])+incoming(state,b.id,material)+return_room(state,b.id,material)>capacity(b.kind,material))
                invalid("auxiliary inventory/incoming/return room exceed physical compartment");
            std::uint64_t own=0,used=0;
            if (original_industry_source(b.kind)) {
                const auto& source=account(state.sources,b.id);
                own=output(b.kind)==material ? source.completed:0;used=source.aux_inputs_used[g];
            }
            if (b.kind==Object::OriginalConfucianAcademy219 && material==OriginalAuxMaterial::Paper) {
                const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.owner==b.id && c.role==CourierRole::OriginalConfucian;});
                if (actor==couriers.end()) invalid("Academy Paper sink lost its paid visitor");
                const auto* trip=trip_for(services.trips,actor->id);used=trip ? trip->transfer.sequence:0;
            }
            if (stock.consumed[g]!=used) invalid("auxiliary sink differs from actual completed recipes or Academy dispatches");
            std::uint64_t lhs=own;add(lhs,stock.received[g]);add(lhs,stock.returned[g]);
            std::uint64_t rhs=static_cast<std::uint64_t>(stock.amounts[g]);add(rhs,stock.departed[g]);add(rhs,stock.consumed[g]);
            if (lhs!=rhs) invalid("auxiliary owner origin/receipt/return balance violated");
            if ((!capacity(b.kind,material) && (lhs || rhs)) ||
                (b.kind!=Object::Warehouse && !input(b.kind,material) && stock.received[g]) ||
                (b.kind!=Object::Warehouse && output(b.kind)!=material && (stock.departed[g] || stock.returned[g])))
                invalid("auxiliary material entered an unsupported owner compartment or direction");
            add(stocks[g],static_cast<std::uint64_t>(stock.amounts[g]));add(consumed[g],stock.consumed[g]);
            add(received[g],stock.received[g]);add(departed[g],stock.departed[g]);add(returned[g],stock.returned[g]);
        }
    }
    for (std::size_t i=0;i<state.aux_trips.size();++i) {
        const auto& trip=state.aux_trips[i];const auto g=column(trip.material);
        const auto& source=body(buildings,trip.source);const auto& target=body(buildings,trip.target);
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==trip.transfer.courier_id;});
        if ((i && !(state.aux_trips[i-1].transfer.courier_id<trip.transfer.courier_id)) || actor==couriers.end() ||
            !trip.transfer.sequence || trip.transfer.sequence>trip.dispatched_tick || trip.dispatched_tick<=source.placed_tick ||
            trip.dispatched_tick<target.placed_tick || trip.dispatched_tick>tick || actor->owner!=source.id || actor->target!=target.id ||
            actor->last_dispatched_target!=target.id || actor->cargo || actor->reserved || actor->good!=Good::Goods ||
            trip.loaded<=0 || trip.loaded>4 || trip.delivered<0 || trip.returned<0 || trip.delivered>trip.loaded || trip.returned>trip.loaded ||
            (actor->role==CourierRole::OriginalAuxToWarehouse ? output(source.kind)!=trip.material || target.kind!=Object::Warehouse:
                actor->role!=CourierRole::OriginalAuxSupplier || source.kind!=Object::Warehouse || !input(target.kind,trip.material)))
            invalid("auxiliary latest trip is not its actual fixed-domain loaded actor");
        const auto amount=remaining(trip);
        if (trip.phase==OriginalGoodsTransferPhase::Loaded) {
            if (trip.delivered || trip.returned || trip.arrival_tick || actor->phase!=CourierPhase::ToWarehouse)
                invalid("auxiliary outbound trip has impossible delivery/cancel phase");
            if (tick-trip.dispatched_tick+1<moved(source,*actor)) invalid("auxiliary movement predates real departure");
        } else if (trip.phase==OriginalGoodsTransferPhase::Returning) {
            if (trip.delivered || trip.returned || trip.arrival_tick || actor->phase!=CourierPhase::Returning)
                invalid("auxiliary cancelled trip changed its sole cargo");
        } else if (trip.phase==OriginalGoodsTransferPhase::Completed) {
            if (trip.delivered!=trip.loaded || trip.returned || amount || !trip.arrival_tick || actor->phase==CourierPhase::ToWarehouse)
                invalid("auxiliary completed trip lost its actual arrival");
        } else if (trip.phase==OriginalGoodsTransferPhase::Returned) {
            if (trip.returned!=trip.loaded || trip.delivered || amount || trip.arrival_tick || actor->phase!=CourierPhase::IdleAtWorkshop)
                invalid("auxiliary returned trip does not conserve its full load");
        } else invalid("auxiliary intermediate reservation/partial delivery is unsupported");
        if (trip.arrival_tick) {
            if (*trip.arrival_tick<=trip.dispatched_tick || *trip.arrival_tick>tick ||
                *trip.arrival_tick-trip.dispatched_tick<travel(source,target)-1)
                invalid("auxiliary arrival predates physical paid trip");
            const auto required=actor->phase==CourierPhase::IdleAtWorkshop ? travel(target,source):moved(target,*actor);
            if (tick-*trip.arrival_tick<required) invalid("auxiliary return predates physical arrival");
        }
        if (account(state.auxiliary,source.id).departed[g]<static_cast<std::uint64_t>(trip.loaded) ||
            account(state.auxiliary,target.id).received[g]<static_cast<std::uint64_t>(trip.delivered) ||
            account(state.auxiliary,source.id).returned[g]<static_cast<std::uint64_t>(trip.returned))
            invalid("auxiliary trip exceeds its owner's actual departure/receipt/return counters");
        add(cargo[g],static_cast<std::uint64_t>(amount));
    }
    for (const auto& c:couriers) if (c.role==CourierRole::OriginalAuxSupplier || c.role==CourierRole::OriginalAuxToWarehouse) {
        const auto& source=body(buildings,c.owner);
        if (c.cargo || c.reserved || c.good!=Good::Goods ||
            (c.role==CourierRole::OriginalAuxSupplier ? source.kind!=Object::Warehouse:!output(source.kind)))
            invalid("auxiliary actor has a mirrored cargo or incompatible owner");
        const auto* trip=trip_for(state.aux_trips,c.id);
        if (!trip && (c.phase!=CourierPhase::IdleAtWorkshop || c.last_dispatched_target || c.target!=c.owner))
            invalid("used auxiliary actor lost its monotonic trip highwater");
        const auto& stock=account(state.auxiliary,c.owner);std::uint64_t departures=0;
        for (const auto n:stock.departed) add(departures,n);
        const auto seq=trip ? trip->transfer.sequence:0;
        if (departures<seq || (seq<=UINT64_MAX/4 && departures>seq*4)) invalid("auxiliary departure history exceeds actual full-cart trip opportunities");
    }
    // Necessary receipt authority from retained latest actual arrivals. A first
    // trip has no compacted predecessor; later trips may have served another
    // target/material. This is a bound, not an invented full travel history.
    for (const auto& stock:state.auxiliary) for (std::size_t g=0;g<original_aux_material_count;++g) {
        const auto& target=body(buildings,stock.building_id);const auto material=static_cast<OriginalAuxMaterial>(g);
        std::uint64_t latest=0;bool pruned=false;
        for (const auto& trip:state.aux_trips) {
            const auto& source=body(buildings,trip.source);
            if (target.kind==Object::Warehouse ? output(source.kind)!=material:source.kind!=Object::Warehouse) continue;
            pruned|=trip.transfer.sequence>1;
            if (trip.target==target.id && trip.material==material) add(latest,static_cast<std::uint64_t>(trip.delivered));
        }
        if (stock.received[g]<latest || (!pruned && stock.received[g]!=latest)) invalid("auxiliary owner receipt has no possible actual supplier arrival");
    }
    for (std::size_t g=0;g<original_aux_material_count;++g) {
        std::uint64_t rhs=stocks[g];add(rhs,cargo[g]);add(rhs,consumed[g]);
        std::uint64_t outbound=received[g];add(outbound,returned[g]);add(outbound,cargo[g]);
        if (produced[g]!=rhs || departed[g]!=outbound) invalid("auxiliary global mass/transfer balance violated");
    }
    std::size_t clay_actors=0;
    for (const auto& c:couriers) if (c.role==CourierRole::Clay) {
        ++clay_actors;const auto* trip=trip_for(state.clay_trips,c.id);
        if (!trip && (c.phase!=CourierPhase::IdleAtWorkshop || c.last_dispatched_target || c.target!=c.owner))
            invalid("used native Clay actor lost its all-trip sequence highwater");
    }
    if (state.clay_trips.size()>clay_actors) invalid("native Clay metadata actor count exceeded");
    for (std::size_t i=0;i<state.clay_trips.size();++i) {
        const auto& trip=state.clay_trips[i];const auto& source=body(buildings,trip.source);const auto& target=body(buildings,trip.target);
        const auto actor=std::find_if(couriers.begin(),couriers.end(),[&](const auto& c){return c.id==trip.transfer.courier_id;});
        if ((i && !(state.clay_trips[i-1].transfer.courier_id<trip.transfer.courier_id)) || actor==couriers.end() || actor->role!=CourierRole::Clay ||
            actor->owner!=source.id || actor->target!=target.id || actor->last_dispatched_target!=target.id || source.kind!=Object::ClaySource ||
            (target.kind!=Object::Pottery && target.kind!=Object::OriginalBronzewareMaker42) || !trip.transfer.sequence ||
            trip.transfer.sequence>trip.dispatched_tick || trip.dispatched_tick<=source.placed_tick || trip.dispatched_tick<target.placed_tick ||
            trip.dispatched_tick>tick || trip.loaded<=0 || trip.loaded>4 || trip.delivered<0 || trip.delivered>trip.loaded)
            invalid("native Clay latest trip is not its actual paid source/target actor");
        if (trip.arrival_tick) {
            if (trip.delivered!=trip.loaded || actor->cargo || actor->reserved || actor->phase==CourierPhase::ToWarehouse ||
                *trip.arrival_tick<=trip.dispatched_tick || *trip.arrival_tick>tick || *trip.arrival_tick-trip.dispatched_tick<travel(source,target)-1)
                invalid("native Clay arrived metadata disagrees with sole physical cargo");
            const auto required=actor->phase==CourierPhase::IdleAtWorkshop ? travel(target,source):moved(target,*actor);
            if (tick-*trip.arrival_tick<required) invalid("native Clay return predates physical arrival");
        } else if (trip.delivered || actor->phase!=CourierPhase::ToWarehouse || actor->cargo!=trip.loaded || actor->reserved!=trip.loaded ||
            tick-trip.dispatched_tick+1<moved(source,*actor)) invalid("native Clay outbound metadata disagrees with sole physical cargo");
    }
}
void return_load(OriginalIndustryState& state,OriginalAuxTrip& trip) {
    const auto n=remaining(trip);auto& source=account(state.auxiliary,trip.source);const auto g=column(trip.material);
    source.amounts[g]+=n;add(source.returned[g],static_cast<std::uint64_t>(n));trip.returned+=n;
    trip.phase=OriginalGoodsTransferPhase::Returned;
}
}

void World::validate_original_industry_model() const {
    if (!original_rules_model_) invalid("industry requires a prepared model");
    for (unsigned value=static_cast<unsigned>(Object::OriginalLoggingShed38);value<=static_cast<unsigned>(Object::OriginalMulberryTree28);++value) {
        const auto kind=static_cast<Object>(value);const auto model=original_industry_model(kind);
        if (!model || !original_rules_model_->supports_building(*model) ||
            original_rules_model_->building_value(*model,original::BuildingField::Cost)<=0)
            invalid("industry requires positive actual prepared model cost");
        const auto employees=original_rules_model_->building_value(*model,original::BuildingField::Employees);
        if (original_industry_plant(kind) ? employees!=0:employees<=0 || employees>INT16_MAX)
            invalid("industry requires bounded actual prepared model staffing");
    }
}
CommandResult World::validate_original_industry_command(Command command) const {
    CommandResult result{false,false,"industry requires active original production",command_sequence_,ticks_};
    if (!original_production_enabled() || !original_production_) return result;
    if (!in_bounds(command.cell)) {result.reason="industry construction outside map";return result;}
    const auto command_value=static_cast<unsigned>(command.type);
    const auto first=static_cast<unsigned>(CommandType::PlaceOriginalLoggingShed);
    if (command_value<first || command_value>static_cast<unsigned>(CommandType::PlaceOriginalMulberryTree)) return result;
    const auto kind=static_cast<Object>(static_cast<unsigned>(Object::OriginalLoggingShed38)+command_value-first);
    if (original_industry_source(kind)) {
        if (std::count_if(buildings_.begin(),buildings_.end(),[&](const auto& b){return b.kind==kind;})>=2) {
            result.reason="industry source type limit reached";return result;
        }
    } else {
        const auto at=std::find_if(original_production_->industry.sources.begin(),original_production_->industry.sources.end(),
            [&](const auto& s){return s.building_id==command.building_id;});
        if (at==original_production_->industry.sources.end() || at->plant_id ||
            plant_for(building(at->building_id).kind)!=kind || !adjacent(building(at->building_id).cell,command.cell)) {
            result.reason="plant requires its selected paid matching adjacent orchard";return result;
        }
    }
    result.accepted=true;result.changed=true;result.reason="industry construction facts ready";return result;
}
void World::validate_original_industry_snapshot(const WorldSnapshot& s) {
    if (!s.original_production || !s.original_goods) invalid("industry requires active sole production/goods authority");
    validate_industry(s.original_production->industry,s.buildings,s.couriers,*s.original_goods,s.original_production->services,s.width,s.height,s.ticks);
    if (simulation::original_safe_demolition_enabled(s.profile,s.rule_version)) {
        if (!s.original_production->safe_demolition) invalid("Rule19 industry requires its retired purchase journal");
        validate_retired_clay(*s.original_production->safe_demolition,s.buildings,s.couriers,s.ticks);
    }
}
void World::validate_original_industry_state() const {
    validate_industry(original_production_->industry,buildings_,couriers_,*original_goods_,original_production_->services,width_,height_,ticks_);
    if (original_safe_demolition_enabled()) {
        if (!original_production_->safe_demolition) invalid("Rule19 industry requires its retired purchase journal");
        validate_retired_clay(*original_production_->safe_demolition,buildings_,couriers_,ticks_);
    }
}
OriginalAuxBalance World::original_auxiliary_balance(OriginalAuxMaterial material) const {
    const auto g=column(material);OriginalAuxBalance result;
    if (!original_production_enabled()) return result;
    const auto& state=original_production_->industry;
    for (const auto& s:state.sources) if (output(building(s.building_id).kind)==material) add(result.produced,s.completed);
    for (const auto& stock:state.auxiliary) {
        add(result.stock,static_cast<std::uint64_t>(stock.amounts[g]));add(result.consumed,stock.consumed[g]);
    }
    for (const auto& trip:state.aux_trips) if (trip.material==material) {
        const auto amount=static_cast<std::uint64_t>(remaining(trip));add(result.cargo,amount);add(result.source_reserved,amount);
        if (trip.phase==OriginalGoodsTransferPhase::Loaded) add(result.incoming_reserved,amount);
    }
    auto rhs=result.stock;add(rhs,result.cargo);add(rhs,result.consumed);result.conserved=result.produced==rhs;return result;
}
bool World::original_auxiliary_courier(CourierRole role) const {
    return original_production_enabled() && (role==CourierRole::OriginalAuxToWarehouse || role==CourierRole::OriginalAuxSupplier);
}
int World::original_auxiliary_cargo(CourierId id) const {
    if (!original_production_enabled()) return 0;
    (void)courier(id);const auto* trip=trip_for(original_production_->industry.aux_trips,id);return trip ? remaining(*trip):0;
}
CourierDispatchDecision World::original_auxiliary_dispatch_status(const CourierState& c) const {
    const auto& state=original_production_->industry;const auto& stock=account(state.auxiliary,c.owner);
    const auto* previous=trip_for(state.aux_trips,c.id);
    if (previous && previous->transfer.sequence==UINT64_MAX) return {CourierDispatchStatus::NoStock,{}};
    const auto fixed=output(building(c.owner).kind);const auto first=previous ? (column(previous->material)+1)%original_aux_material_count:0;
    bool stocked=false,room=false,target_exists=false;
    for (std::size_t offset=0;offset<original_aux_material_count;++offset) {
        const auto material=static_cast<OriginalAuxMaterial>((first+offset)%original_aux_material_count);
        if ((c.role==CourierRole::OriginalAuxToWarehouse && fixed!=material) || stock.amounts[column(material)]<=0) continue;
        stocked=true;
        std::array<const BuildingState*,20> targets{};std::size_t count=0;
        for (const auto& b:buildings_) if (b.id!=c.owner &&
            (c.role==CourierRole::OriginalAuxToWarehouse ? b.kind==Object::Warehouse:input(b.kind,material))) {
            if (count==targets.size()) invalid("auxiliary target bound exceeded");
            targets[count++]=&b;
        }
        target_exists|=count>0;std::size_t start=0;
        for (std::size_t i=0;i<count;++i) if (c.last_dispatched_target==targets[i]->id) start=(i+1)%count;
        for (std::size_t i=0;i<count;++i) {
            const auto& target=*targets[(start+i)%count];
            if (headroom(state,target.id,target.kind,material)<=0) continue;
            room=true;
            const auto at=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target.id,
                [](const auto& row,BuildingId id){return row.first<id;});
            if (at!=c.dynamic_target_routes.end() && at->first==target.id && at->second) return {CourierDispatchStatus::Ready,target.id};
        }
    }
    return {!stocked ? CourierDispatchStatus::NoStock:!target_exists ? CourierDispatchStatus::NoTarget:
        room ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::TargetFull,{}};
}
void World::dispatch_original_auxiliary(CourierState& c) {
    const auto decision=courier_dispatch_status(c.id);
    if (decision.status!=CourierDispatchStatus::Ready || !decision.selected_target) return;
    const auto target=*decision.selected_target;const auto& target_body=building(target);
    const auto at=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),target,
        [](const auto& row,BuildingId id){return row.first<id;});
    if (at==c.dynamic_target_routes.end() || !at->second) throw std::logic_error("auxiliary dispatch lost cached route");
    auto candidate=original_production_->industry;const auto* old=trip_for(candidate.aux_trips,c.id);
    if (old && old->transfer.sequence==UINT64_MAX) return;
    const auto first=old ? (column(old->material)+1)%original_aux_material_count:0;
    const auto fixed=output(building(c.owner).kind);auto& stock=account(candidate.auxiliary,c.owner);
    std::optional<OriginalAuxMaterial> selected;
    for (std::size_t offset=0;offset<original_aux_material_count;++offset) {
        const auto material=static_cast<OriginalAuxMaterial>((first+offset)%original_aux_material_count);
        if ((c.role==CourierRole::OriginalAuxToWarehouse ? fixed==material && target_body.kind==Object::Warehouse:input(target_body.kind,material)) &&
            stock.amounts[column(material)]>0 && headroom(candidate,target,target_body.kind,material)>0) {selected=material;break;}
    }
    if (!selected) throw std::logic_error("auxiliary dispatch lost selected material");
    const auto material=*selected;const auto g=column(material);
    const auto amount=std::min({4,stock.amounts[g],headroom(candidate,target,target_body.kind,material)});
    OriginalAuxTrip next{{c.id,old ? old->transfer.sequence+1:1},c.owner,target,material,OriginalGoodsTransferPhase::Loaded,amount,0,0,ticks_,{}};
    stock.amounts[g]-=amount;add(stock.departed[g],static_cast<std::uint64_t>(amount));
    if (auto* current=trip_for(candidate.aux_trips,c.id)) *current=next;
    else candidate.aux_trips.insert(std::lower_bound(candidate.aux_trips.begin(),candidate.aux_trips.end(),c.id,
        [](const auto& row,CourierId id){return row.transfer.courier_id<id;}),next);
    auto path=*at->second;
    original_production_->industry=std::move(candidate);c.path=std::move(path);c.target=target;c.last_dispatched_target=target;
    c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;
}
void World::arrive_original_auxiliary(CourierState& c) {
    auto candidate=original_production_->industry;auto* trip=trip_for(candidate.aux_trips,c.id);
    if (!trip) throw std::logic_error("auxiliary arrival lost latest actual trip");
    if (c.phase==CourierPhase::ToWarehouse) {
        const auto amount=remaining(*trip);const auto g=column(trip->material);auto& target=account(candidate.auxiliary,trip->target);
        // Reserved physical inputs remain receivable while their recipe or
        // Academy is paused/burning; actual production and new tours stay gated.
        target.amounts[g]+=amount;add(target.received[g],static_cast<std::uint64_t>(amount));
        trip->delivered=amount;trip->arrival_tick=ticks_;trip->phase=OriginalGoodsTransferPhase::Completed;
        original_production_->industry=std::move(candidate);c.phase=CourierPhase::Returning;
        if (valid_return_path(c)) {std::reverse(c.path.begin(),c.path.end());c.path_vertex=0;}
        else {c.path={c.path.back()};c.path_vertex=0;c.route_pending=true;c.route_checked_revision.reset();reroute_v2(c);}
    } else {
        if (remaining(*trip)) return_load(candidate,*trip);
        original_production_->industry=std::move(candidate);c.path.clear();c.path_vertex=0;c.phase=CourierPhase::IdleAtWorkshop;
        c.route_pending=false;c.route_checked_revision.reset();
    }
}
void World::cancel_original_auxiliary(BuildingId owner) {
    for (auto& c:couriers_) if (c.owner==owner && original_auxiliary_courier(c.role) && c.phase==CourierPhase::ToWarehouse) {
        auto candidate=original_production_->industry;auto* trip=trip_for(candidate.aux_trips,c.id);
        if (!trip) throw std::logic_error("cancelled auxiliary actor lost its real load");
        trip->phase=OriginalGoodsTransferPhase::Returning;
        if (!c.edge_progress && building_footprint_contains(profile_,rule_version_,building(owner).kind,building(owner).cell,c.path.at(c.path_vertex))) {
            return_load(candidate,*trip);original_production_->industry=std::move(candidate);
            c.path.clear();c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::IdleAtWorkshop;
        } else {
            original_production_->industry=std::move(candidate);c.phase=CourierPhase::Returning;mark_route_pending(c);
            if (!c.edge_progress) reroute_v2(c);
        }
    }
}
bool World::original_clay_dispatch_available(CourierId id) const {
    if (!original_production_enabled()) return true;
    const auto* trip=trip_for(original_production_->industry.clay_trips,id);
    return !trip || trip->transfer.sequence<UINT64_MAX;
}
void World::record_original_clay_dispatch(const CourierState& c) {
    if (!original_production_enabled()) return;
    auto candidate=original_production_->industry;const auto* old=trip_for(candidate.clay_trips,c.id);
    if (old && old->transfer.sequence==UINT64_MAX) throw std::overflow_error("native Clay dispatch sequence exhausted");
    OriginalClayTrip next{{c.id,old ? old->transfer.sequence+1:1},c.owner,c.target,c.cargo,0,ticks_,{}};
    if (auto* current=trip_for(candidate.clay_trips,c.id)) *current=next;
    else candidate.clay_trips.insert(std::lower_bound(candidate.clay_trips.begin(),candidate.clay_trips.end(),c.id,
        [](const auto& row,CourierId id){return row.transfer.courier_id<id;}),next);
    original_production_->industry=std::move(candidate);
}
void World::record_original_clay_arrival(const CourierState& c) {
    if (!original_production_enabled()) return;
    auto candidate=original_production_->industry;auto* trip=trip_for(candidate.clay_trips,c.id);
    if (!trip || trip->arrival_tick || trip->loaded!=c.cargo || trip->target!=c.target)
        throw std::logic_error("native Clay arrival lost its actual sole cargo trip");
    trip->delivered=c.cargo;trip->arrival_tick=ticks_;
    if (building(c.target).kind==Object::OriginalBronzewareMaker42) {
        auto& source=account(candidate.sources,c.target);add(source.clay_received,static_cast<std::uint64_t>(c.cargo));
        source.last_clay_arrival=OriginalClayReceipt{trip->transfer,c.owner,c.cargo,ticks_};
    }
    original_production_->industry=std::move(candidate);
}
bool World::original_academy_paper_available(BuildingId id) const {
    return original_production_enabled() && account(original_production_->industry.auxiliary,id).amounts[column(OriginalAuxMaterial::Paper)]>0;
}
void World::consume_original_academy_paper(BuildingId id) {
    auto candidate=original_production_->industry;auto& stock=account(candidate.auxiliary,id);const auto g=column(OriginalAuxMaterial::Paper);
    if (stock.amounts[g]<=0) throw std::logic_error("Academy dispatch cannot create its Paper input");
    --stock.amounts[g];add(stock.consumed[g],1);original_production_->industry=std::move(candidate);
}
void World::update_original_industry() {
    for (std::size_t i=0;i<original_production_->industry.sources.size();++i) {
        auto& live=original_production_->industry.sources[i];const auto& source_body=building(live.building_id);
        if (!source_body.operating_enabled || building_on_fire(source_body.id) || !building_staffed_for_tick(source_body.id) ||
            building_entrances(source_body.id).empty() || (plant_for(source_body.kind)!=Object::Empty && !live.plant_id)) continue;
        const auto aux=output(source_body.kind);const auto good=original_industry_goods::output(source_body.kind);
        const bool room=aux ? headroom(original_production_->industry,source_body.id,source_body.kind,*aux)>0:
            original_market_headroom(source_body.id,*good)>0;
        bool inputs=true;
        for (std::size_t g=0;g<original_aux_material_count;++g)
            if (input(source_body.kind,static_cast<OriginalAuxMaterial>(g)) && account(original_production_->industry.auxiliary,source_body.id).amounts[g]<=0) inputs=false;
        if (source_body.kind==Object::OriginalPaperMaker45 && original_typed_stock(source_body.id,original::Goods::Hemp)<=0) inputs=false;
        if (source_body.kind==Object::OriginalBronzewareMaker42 && source_body.input_clay<=0) inputs=false;
        if (source_body.kind==Object::OriginalBronzeSmelter39) {
            auto& ore=account(original_production_->industry.managed_ore,source_body.id);
            if (ore.stock<=0 || !inputs || !room) {
                if (ore.stock==8) {ore.progress=0;continue;}
                if (ore.progress<79) {++ore.progress;continue;}
                if (ore.extracted==UINT64_MAX) throw std::overflow_error("managed Ore extraction exhausted");
                ++ore.stock;++ore.extracted;ore.progress=0;continue;
            }
        }
        if (!room) {live.progress=0;continue;}
        if (!inputs) continue;
        if (live.progress<79) {++live.progress;continue;}
        auto candidate=original_production_->industry;auto& recipe=candidate.sources[i];
        OriginalGoodsLedgerSnapshot goods{*original_goods_,original_economy_->houses};
        const original_industry_goods::Context context(buildings_,couriers_);
        if (source_body.kind==Object::OriginalPaperMaker45) {
            goods=original_goods_consume(context.view(goods.goods,goods.houses),source_body.id,original::Goods::Hemp,1);
            add(recipe.goods_inputs_used[0],1);
        }
        for (std::size_t g=0;g<original_aux_material_count;++g) if (input(source_body.kind,static_cast<OriginalAuxMaterial>(g))) {
            auto& stock=account(candidate.auxiliary,source_body.id);--stock.amounts[g];add(stock.consumed[g],1);add(recipe.aux_inputs_used[g],1);
        }
        if (source_body.kind==Object::OriginalBronzeSmelter39) {auto& ore=account(candidate.managed_ore,source_body.id);--ore.stock;add(ore.used,1);}
        if (source_body.kind==Object::OriginalBronzewareMaker42) add(recipe.clay_used,1);
        if (aux) ++account(candidate.auxiliary,source_body.id).amounts[column(*aux)];
        else goods=original_goods_produce(context.view(goods.goods,goods.houses),source_body.id,*good,1);
        add(recipe.completed,1);recipe.progress=0;
        original_production_->industry=std::move(candidate);original_goods_=std::move(goods.goods);
        if (source_body.kind==Object::OriginalBronzewareMaker42) --mutable_building(source_body.id).input_clay;
    }
}

} // namespace openemperor::simulation
