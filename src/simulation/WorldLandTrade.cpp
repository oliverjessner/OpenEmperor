#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
bool sha(const std::string& value) {
    return value.size()==64 && std::ranges::all_of(value,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
OriginalModelSource trade_model_source(const original::OriginalRulesModel& model) {
    const auto& source=model.provenance();
    return {source.format_version,source.sha256,std::uint64_t(source.byte_count),model.difficulty()};
}
bool trade_command(CommandType type) {
    return type==CommandType::ConfigureLandTrade || type==CommandType::ImportPottery || type==CommandType::ExportPottery;
}
}
LandTradeBoundaryFacts::LandTradeBoundaryFacts(int width,int height,std::span<const std::uint8_t> geometry,std::string source)
    :width_(width),height_(height),source_map_sha256_(std::move(source)) {
    if(width<=0||height<=0||width>512||height>512||geometry.size()!=std::size_t(width)*std::size_t(height)||!sha(source_map_sha256_)||
       !std::ranges::all_of(geometry,[](auto n){return n<=1;})||std::ranges::none_of(geometry,[](auto n){return n==1;}))
        invalid("land trade boundary requires complete common source geometry and map SHA");
    geometry_.assign(geometry.begin(),geometry.end());
}
bool LandTradeBoundaryFacts::contains(Cell p) const noexcept {
    return p.x>=0&&p.y>=0&&p.x<width_&&p.y<height_&&geometry_[std::size_t(p.y)*std::size_t(width_)+std::size_t(p.x)]!=0;
}
bool LandTradeBoundaryFacts::boundary_footprint(Cell origin,int width,int height) const noexcept {
    if(width<=0||height<=0||width>4||height>4||origin.x<0||origin.y<0||origin.x>width_-width||origin.y>height_-height)return false;
    bool edge=false;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const Cell p{origin.x+x,origin.y+y};if(!contains(p))return false;
        for(const auto d:std::array<Cell,4>{{{1,0},{-1,0},{0,1},{0,-1}}})if(!contains({p.x+d.x,p.y+d.y}))edge=true;
    }
    return edge;
}
LandTradeBalance inspect_land_trade(const AuthoredLandTradeState& state) {
    if(!sha(state.source_map_sha256)||state.model_source.format_version!=1||!sha(state.model_source.sha256)||
       !state.model_source.byte_count||state.model_source.byte_count>original::max_model_bytes||static_cast<unsigned>(state.model_source.difficulty)>4U||
       state.orders.size()>land_trade_order_limit||state.next_order_id!=state.orders.size()+1)
        invalid("invalid land trade identity, finite journal or next ID");
    LandTradeBalance result;
    if(!state.binding) {if(!state.orders.empty())invalid("land trade orders require a paid binding");return result;}
    const auto& b=*state.binding;
    if(!static_cast<std::uint32_t>(b.warehouse)||!static_cast<std::uint32_t>(b.gateway)||b.warehouse==b.gateway||!static_cast<std::uint32_t>(b.courier))
        invalid("invalid land trade binding identities");
    std::uint64_t previous=b.configured_tick;
    for(std::size_t i=0;i<state.orders.size();++i) {
        const auto& order=state.orders[i];
        if(static_cast<std::uint32_t>(order.id)!=i+1||static_cast<unsigned>(order.direction)>1U||order.ordered_tick<previous||
           order.ordered_tick>UINT64_MAX-20||
           (order.boundary_tick&&(*order.boundary_tick<order.ordered_tick+10||*order.boundary_tick>UINT64_MAX-10))||
           (order.completed_tick&&(!order.boundary_tick||*order.completed_tick<*order.boundary_tick+10))||
           (!order.completed_tick&&i+1!=state.orders.size()))invalid("invalid land trade transaction order or physical event clocks");
        if(order.direction==LandTradeDirection::Import) {
            result.import_offer_remaining-=land_trade_portion;
            result.imports_paid+=std::uint64_t(land_trade_portion*land_trade_import_price);
            if(order.boundary_tick)result.imported+=land_trade_portion;
        } else {
            result.export_offer_remaining-=land_trade_portion;
            if(order.boundary_tick){result.exported+=land_trade_portion;result.exports_received+=std::uint64_t(land_trade_portion*land_trade_export_price);}
        }
        result.busy=!order.completed_tick;
        previous=order.completed_tick.value_or(order.ordered_tick);
    }
    if(result.import_offer_remaining<0||result.export_offer_remaining<0||result.exports_received>24)
        invalid("finite land trade offer or counterparty budget exceeded");
    return result;
}
Command configure_land_trade(BuildingId warehouse,BuildingId gateway) {
    Command c{CommandType::ConfigureLandTrade};c.building_id=warehouse;c.trade_gateway=gateway;return c;
}
Command import_pottery() {return {CommandType::ImportPottery};}
Command export_pottery() {return {CommandType::ExportPottery};}
World World::create_authored_land_trade(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const LandTradeBoundaryFacts> boundary) {
    if(!permissions||!model||!boundary)invalid("land trade create requires prepared source, model and boundary");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    return World(width,height,std::move(mask),RulesProfile::CityV16,kAuthoredLandTradeRuleVersion,std::move(permissions),false,std::move(model),{},std::move(boundary));
}
World World::restore_authored_land_trade(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const LandTradeBoundaryFacts> boundary) {
    if(s.profile!=RulesProfile::CityV16||s.rule_version!=kAuthoredLandTradeRuleVersion||!model||!boundary||!permissions||
       !s.land_trade||s.land_trade->model_source!=trade_model_source(*model)||s.land_trade->source_map_sha256!=boundary->source_map_sha256())
        invalid("land trade restore requires exact21, model/difficulty and source geometry");
    validate_authored_land_trade_snapshot(s);
    return restore_impl(s,{},std::move(permissions),false,std::move(model),{},std::move(boundary));
}
void World::validate_authored_land_trade_snapshot(const WorldSnapshot& s) {
    const bool enabled=s.profile==RulesProfile::CityV16&&s.rule_version==kAuthoredLandTradeRuleVersion;
    if(!enabled){if(s.land_trade)invalid("land trade authority requires exact Rule21");return;}
    if(!s.land_trade||s.original_economy||s.original_influence||s.original_goods||s.original_production)
        invalid("Rule21 requires its own trade contract and classic economy");
    (void)inspect_land_trade(*s.land_trade);
    if(s.land_trade->binding&&s.land_trade->binding->configured_tick>s.ticks)invalid("land trade binding is in the future");
    for(const auto& order:s.land_trade->orders)
        if(order.ordered_tick>s.ticks||(order.boundary_tick&&*order.boundary_tick>s.ticks)||(order.completed_tick&&*order.completed_tick>s.ticks))
            invalid("land trade event is in the future");
}
LandTradeBalance World::land_trade_balance() const {
    if(!authored_land_trade_enabled()||!land_trade_)invalid("land trade balance requires exact21");
    return inspect_land_trade(*land_trade_);
}
bool World::land_trade_protected_building(BuildingId id) const {
    return authored_land_trade_enabled()&&land_trade_&&land_trade_->binding&&
        (land_trade_->binding->warehouse==id||land_trade_->binding->gateway==id);
}
CommandResult World::validate_land_trade_command(Command command) const {
    const auto refuse=[&](const char* reason){return CommandResult{false,false,reason,command_sequence_,ticks_};};
    if(!trade_command(command.type)||!authored_land_trade_enabled()||!land_trade_||!land_trade_boundary_)
        return refuse("Authored land trade requires explicit rule21");
    if(command_sequence_==UINT64_MAX||ticks_>UINT64_MAX-20)return refuse("Land trade counters exhausted");
    try {
        const auto balance=land_trade_balance();
        if(command.type==CommandType::ConfigureLandTrade) {
            if(land_trade_->binding)return refuse("Land trade is already bound; no reassignment or refund");
            if(couriers_.size()>=courier_collection_limit(profile_)||next_courier_id_==UINT32_MAX)return refuse("Caravan identity or courier budget exhausted");
            const auto& warehouse=building(command.building_id);const auto& gateway=building(command.trade_gateway);
            if(warehouse.id==gateway.id||warehouse.kind!=Object::Warehouse||gateway.kind!=Object::Warehouse)
                return refuse("Choose two distinct paid Warehouses");
            const auto footprint=building_footprint(profile_,rule_version_,gateway.kind);
            if(!land_trade_boundary_->boundary_footprint(gateway.cell,footprint.width,footprint.height))
                return refuse("Gateway Warehouse must touch the common source geometry boundary");
            const auto low=permissions_->cell_height(gateway.cell);
            for(const auto p:building_footprint_cells(profile_,rule_version_,gateway.kind,gateway.cell))
                if(!permissions_->building_allowed(p)||permissions_->protected_original(p)||permissions_->cell_height(p)!=low)
                    return refuse("Gateway requires complete permitted level land");
            if(treasury_<land_trade_license_cost||construction_spent_total_>UINT64_MAX-std::uint64_t(land_trade_license_cost))return refuse("Not enough money for the authored trade license");
            if(!find_building_route(warehouse.id,gateway.id))return refuse("Gateway has no physical legal road route");
        } else {
            if(!land_trade_->binding)return refuse("Bind a boundary Warehouse first");
            if(balance.busy)return refuse("Caravan is busy; competing orders are refused");
            const auto& b=*land_trade_->binding;const auto& c=courier(b.courier);const auto& warehouse=building(b.warehouse);
            if(c.phase!=CourierPhase::IdleAtWorkshop)return refuse("Caravan has not returned");
            if(land_trade_->orders.size()>=land_trade_order_limit)return refuse("Trade journal is full");
            if(command.type==CommandType::ImportPottery) {
                if(balance.import_offer_remaining<land_trade_portion)return refuse("Finite authored import offer exhausted");
                if(treasury_<land_trade_portion*land_trade_import_price)return refuse("Not enough money for prepaid import");
                if(warehouse.pottery_stock+warehouse.reserved_incoming>Rules::warehouse_capacity-land_trade_portion)return refuse("Warehouse has no reserved import headroom");
            } else {
                if(balance.export_offer_remaining<land_trade_portion)return refuse("Finite authored export offer or budget exhausted");
                if(warehouse.pottery_stock<land_trade_portion)return refuse("Warehouse has no physical export stock");
            }
            const auto route=std::find_if(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),[&](const auto& r){return r.first==b.gateway;});
            if(route==c.dynamic_target_routes.end()||!route->second)return refuse("Gateway has no physical legal road route");
        }
        const auto warehouse=command.type==CommandType::ConfigureLandTrade?command.building_id:land_trade_->binding->warehouse;
        const auto gateway=command.type==CommandType::ConfigureLandTrade?command.trade_gateway:land_trade_->binding->gateway;
        for(const auto id:{warehouse,gateway}) {
            if(!building(id).operating_enabled)return refuse("Trade Warehouse operation is paused");
            if(building_on_fire(id))return refuse("Trade Warehouse is on fire");
            if(!building_staffed(id))return refuse("Trade Warehouse is unstaffed");
        }
        return {true,true,"Authored finite pottery trade ready",command_sequence_+1,ticks_};
    } catch(const std::out_of_range&){return refuse("Trade references a missing paid Warehouse");}
}
CommandResult World::execute_land_trade_command(Command command) {
    const auto checked=validate_land_trade_command(command);if(!checked.accepted)return checked;
    World candidate=*this;auto& state=*candidate.land_trade_;
    if(command.type==CommandType::ConfigureLandTrade) {
        const auto id=static_cast<CourierId>(candidate.next_courier_id_++);
        CourierState c;c.id=id;c.role=CourierRole::AuthoredLandTrader;c.owner=command.building_id;c.target=c.owner;c.good=Good::Pottery;c.enabled=true;
        candidate.couriers_.push_back(std::move(c));
        state.binding=LandTradeBinding{command.building_id,command.trade_gateway,id,ticks_};
        candidate.treasury_-=land_trade_license_cost;candidate.construction_spent_total_+=std::uint64_t(land_trade_license_cost);
        candidate.refresh_routes();
    } else {
        const auto& binding=*state.binding;auto& c=candidate.mutable_courier(binding.courier);auto& warehouse=candidate.mutable_building(binding.warehouse);
        const bool importing=command.type==CommandType::ImportPottery;
        state.orders.push_back({static_cast<LandTradeOrderId>(state.next_order_id++),importing?LandTradeDirection::Import:LandTradeDirection::Export,ticks_,{}, {}});
        const auto route=std::find_if(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),[&](const auto& r){return r.first==binding.gateway;});
        c.path=*route->second;c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();
        c.target=binding.gateway;c.last_dispatched_target=binding.gateway;c.phase=CourierPhase::ToWarehouse;
        if(importing) {candidate.treasury_-=land_trade_portion*land_trade_import_price;warehouse.reserved_incoming+=land_trade_portion;c.reserved=land_trade_portion;}
        else {warehouse.pottery_stock-=land_trade_portion;c.cargo=land_trade_portion;}
    }
    ++candidate.command_sequence_;performance::increment(performance::Counter::WorldExecutes);*this=std::move(candidate);return {true,true,"Authored pottery order accepted; receipt requires physical arrival",command_sequence_,ticks_};
}
void World::arrive_land_trader(CourierState& c) {
    if(!authored_land_trade_enabled()||!land_trade_||!land_trade_->binding||land_trade_->orders.empty())throw std::logic_error("orphan land trader arrival");
    auto& order=land_trade_->orders.back();
    if(order.completed_tick)throw std::logic_error("duplicate completed trade arrival");
    if(c.phase==CourierPhase::ToWarehouse) {
        if(order.boundary_tick)throw std::logic_error("duplicate boundary trade settlement");
        if(order.direction==LandTradeDirection::Import)c.cargo=land_trade_portion;
        else {if(c.cargo!=land_trade_portion||treasury_>INT64_MAX-land_trade_portion*land_trade_export_price)throw std::logic_error("invalid export settlement");
            c.cargo=0;treasury_+=land_trade_portion*land_trade_export_price;}
        order.boundary_tick=ticks_;c.phase=CourierPhase::Returning;
        if(valid_return_path(c)){std::reverse(c.path.begin(),c.path.end());c.path_vertex=0;}
        else {c.path={c.path.back()};c.path_vertex=0;c.route_pending=true;c.route_checked_revision.reset();reroute_v2(c);}
    } else {
        if(!order.boundary_tick)throw std::logic_error("land trader returned without boundary arrival");
        if(order.direction==LandTradeDirection::Import) {
            auto& warehouse=mutable_building(c.owner);
            if(c.cargo!=land_trade_portion||c.reserved!=land_trade_portion||warehouse.reserved_incoming<land_trade_portion||warehouse.pottery_stock>Rules::warehouse_capacity-land_trade_portion)
                throw std::logic_error("lost reserved import headroom");
            warehouse.pottery_stock+=land_trade_portion;warehouse.reserved_incoming-=land_trade_portion;c.cargo=0;c.reserved=0;
        }
        order.completed_tick=ticks_;c.phase=CourierPhase::IdleAtWorkshop;c.path.clear();c.path_vertex=0;c.route_pending=false;c.route_checked_revision.reset();
    }
}
bool World::land_trade_state_valid() const {
    try {
        const bool enabled=authored_land_trade_enabled();if(!enabled)return !land_trade_&&!land_trade_boundary_;
        if(!land_trade_||!land_trade_boundary_||!original_rules_model_||land_trade_->model_source!=trade_model_source(*original_rules_model_)||
           land_trade_->source_map_sha256!=land_trade_boundary_->source_map_sha256())return false;
        const auto balance=land_trade_balance();
        const auto count=std::count_if(couriers_.begin(),couriers_.end(),[](const auto& c){return c.role==CourierRole::AuthoredLandTrader;});
        if(!land_trade_->binding)return !count&&land_trade_->orders.empty();
        if(count!=1||command_sequence_<road_revision_||command_sequence_-road_revision_<1+land_trade_->orders.size())return false;
        const auto& b=*land_trade_->binding;
        for(const auto& prior:couriers_)if((prior.owner==b.warehouse||prior.owner==b.gateway)&&prior.role==CourierRole::MarketPotteryInbound&&prior.id>=b.courier)return false;
const auto& warehouse=building(b.warehouse);const auto& gateway=building(b.gateway);const auto& c=courier(b.courier);
        if(b.configured_tick>ticks_||warehouse.placed_tick>b.configured_tick||gateway.placed_tick>b.configured_tick||
           warehouse.kind!=Object::Warehouse||gateway.kind!=Object::Warehouse||c.owner!=b.warehouse||c.last_dispatched_pottery||c.role!=CourierRole::AuthoredLandTrader||c.good!=Good::Pottery||
           !land_trade_boundary_->boundary_footprint(gateway.cell,2,2))return false;
        const auto low=permissions_->cell_height(gateway.cell);
        for(const auto p:building_footprint_cells(profile_,rule_version_,gateway.kind,gateway.cell))
            if(!permissions_->building_allowed(p)||permissions_->protected_original(p)||permissions_->cell_height(p)!=low)return false;
        const auto wf=building_footprint(profile_,rule_version_,warehouse.kind);
        const auto gf=building_footprint(profile_,rule_version_,gateway.kind);
        const auto distance=[](Cell a,BuildingFootprint af,Cell z,BuildingFootprint zf) {
            const auto dx=std::max({std::int64_t(a.x)-z.x-zf.width+1,std::int64_t(z.x)-a.x-af.width+1,std::int64_t{0}});
            const auto dy=std::max({std::int64_t(a.y)-z.y-zf.height+1,std::int64_t(z.y)-a.y-af.height+1,std::int64_t{0}});
            return std::uint64_t(dx+dy);
        };
        const auto minimum_leg=std::max(std::uint64_t{2},distance(warehouse.cell,wf,gateway.cell,gf))*std::uint64_t(active_rules().courier_edge_ticks);
        for(const auto& prior:land_trade_->orders) {
            if(prior.ordered_tick>ticks_||(prior.boundary_tick&&(*prior.boundary_tick>ticks_||*prior.boundary_tick-prior.ordered_tick<minimum_leg))||
               (prior.completed_tick&&(*prior.completed_tick>ticks_||*prior.completed_tick-*prior.boundary_tick<minimum_leg)))return false;
        }
        if(!balance.busy)return c.phase==CourierPhase::IdleAtWorkshop&&!c.cargo&&!c.reserved&&
            (land_trade_->orders.empty()?(c.target==c.owner&&!c.last_dispatched_target):(c.target==b.gateway&&c.last_dispatched_target==b.gateway));
        const auto& order=land_trade_->orders.back();
        if(c.path.empty()||c.path_vertex>=c.path.size()||c.edge_progress<0)return false;
        const auto origin=order.boundary_tick?gateway.cell:warehouse.cell;
        const auto origin_footprint=order.boundary_tick?gf:wf;
        const auto since=order.boundary_tick.value_or(order.ordered_tick);
        const auto travelled=distance(origin,origin_footprint,c.path[c.path_vertex],{1,1})*std::uint64_t(active_rules().courier_edge_ticks)+std::uint64_t(c.edge_progress);
        if(ticks_-since<travelled)return false;
        if(order.ordered_tick>ticks_||(order.boundary_tick&&*order.boundary_tick>ticks_)||c.target!=b.gateway||c.last_dispatched_target!=b.gateway||
           c.phase!=(order.boundary_tick?CourierPhase::Returning:CourierPhase::ToWarehouse))return false;
        const bool importing=order.direction==LandTradeDirection::Import;
        const auto cargo=(importing?bool(order.boundary_tick):!order.boundary_tick)?land_trade_portion:0;
        return c.cargo==cargo&&c.reserved==(importing?land_trade_portion:0);
    } catch(const std::exception&){return false;}
}
} // namespace openemperor::simulation
