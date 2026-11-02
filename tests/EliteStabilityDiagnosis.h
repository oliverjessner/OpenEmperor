#pragma once

// Explicit, external OE44 QA only. No callback is installed in World and no
// observer value becomes gameplay or saved authority. before/after bracket one
// ordinary tick, outside the production tick/frame implementation.
#include "MarketDistributionMetrics.h"
#include "simulation/OriginalMarketPolicy.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace oe_elite_stability_diagnosis {
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace high=oe_high_tier_metrics;
namespace trace=oe_market_distribution_metrics;
using Json=nlohmann::json;
using oe_starter_fixture::check;

class Observation {
    using Rows=std::map<unsigned,Json>;
    struct Sample {
        std::uint64_t tick=0,sequence=0,taxes=0,upkeep=0,construction=0;
        std::int64_t funds=0;
        Rows bodies,actors,houses;
        std::map<std::string,Json> trips;
    };
    static constexpr std::size_t event_limit=262144;
    static constexpr std::size_t byte_limit=256U*1024U*1024U;
    static constexpr std::array<const char*,9> service_names={
        "Herbalist","Acupuncture","Music","Acrobat","Drama","Ancestor","Confucian","Daoist","Buddhist"};
public:
    Observation(const sim::World& w,const std::filesystem::path& output):
        begin_(w.ticks()),file_(output/"oe44-timeline.jsonl") {
        check(w.original_cardinal_water_enabled()&&(w.rule_version()==14||w.original_inspector_coordination_enabled()||w.original_deadline_fire_dispatch_enabled()||w.original_deadline_service_dispatch_enabled()||w.original_deadline_food_dispatch_enabled()),
            "OE44 diagnosis requires exact Rule14 or explicit Inspector coordination Rule15");
        check(!std::filesystem::exists(file_),"Preserve previous OE44 timeline");
        stream_.open(file_,std::ios::out|std::ios::binary);
        check(bool(stream_),"Cannot create OE44 timeline");
        last_=capture(w);
        emit(w.ticks(),"initial_state",{{"boundary","after initial paid commands; before first observed tick"},
            {"state",state(last_)},{"rule",w.rule_version()},{"world_mutation_by_observer",false},
            {"prepared_route_cache",route_cache(w)},
            {"timing_contract","PRE allocation is captured after player commands and before finance/tick increment. Production, dispatch, movement, fire, admission, water28, countdown35, evaluation39 and consumption follow. Post-tick pools are not claimed as exact pre-load pools."},
            {"selection_limit","UNPROVEN candidate-level explanation: private Warehouse/Market selection is not copied or replaced"}});
    }
    void before(const sim::World& w) {
        if(overflow_)return;
        before_=capture(w);
        if(before_.sequence!=last_.sequence)emit(w.ticks(),"between_tick_player_commands",
            {{"before",state(last_)},{"after",state(before_)},{"receipt_authority","commands.json"}});
        const auto allocation=staffing(before_);
        if(allocation!=last_staffing_) {
            emit(w.ticks(),"pre_tick_allocation",{{"applies_to_next_tick",w.ticks()+1},{"allocation",allocation},
                {"boundary","same allocation used by World before ticks_ increments; not post-tick reconstruction"}});
            staffing_reference_=events_;last_staffing_=allocation;
        }
    }
    void after(const sim::World& w) {
        if(overflow_)return;
        check(w.ticks()==before_.tick+1,"OE44 diagnosis must bracket exactly one ordinary World tick");
        const auto now=capture(w);const auto dispatch=orig::next_scheduler_dispatch(before_.tick);
        check(dispatch.has_value(),"OE44 diagnosis lost the actual scheduler position");
        const auto tax=now.taxes-before_.taxes,bill=now.upkeep-before_.upkeep;
        check(now.funds==before_.funds+static_cast<std::int64_t>(tax)-static_cast<std::int64_t>(bill),
            "OE44 tick finance differs from actual complete ledger");
        if(tax||bill)emit(w.ticks(),"actual_finance",{{"tax_received",tax},{"all_installed_upkeep",bill},
            {"funds_before",before_.funds},{"funds_after",now.funds},{"pre_tick_allocation_event",staffing_reference_},
            {"tax_input_boundary","pre-tick House state, occupants and fire status"}});
        changes(w,now.bodies,before_.bodies,"building_state",{"native_progress","industry_progress","fire_risk"});
        changes(w,now.actors,before_.actors,"actor_state",{"path_vertex","edge_progress","current_cell","next_cell"});
        changes(w,now.houses,before_.houses,"house_state",{});
        for(const auto& [key,value]:now.trips) {
            const auto old=before_.trips.find(key);const Json previous=old==before_.trips.end()?Json(nullptr):old->second;
            if(previous==value)continue;
            Json event{{"trip_key",key},{"before",previous},{"after",value},
                {"pre_tick",before_.tick},{"pre_tick_allocation_event",staffing_reference_},{"actual_tick_dispatch_phase",dispatch->position.phase}};
            const bool sequenced=value.contains("sequence");
            const bool fresh=previous.is_null()||(sequenced?
                previous.value("sequence",std::uint64_t{})!=value.at("sequence").get<std::uint64_t>():
                previous.value("dispatched_tick",std::uint64_t{})!=value.value("dispatched_tick",std::uint64_t{}));
            event["new_sequence_observed"]=sequenced&&fresh;
            event["new_fire_dispatch_witness"]=!sequenced&&fresh;
            event["actual_visit_arrival"]=value.contains("arrival_tick")&&!value["arrival_tick"].is_null()&&
                (previous.is_null()||previous.value("arrival_tick",Json(nullptr))!=value["arrival_tick"]);
            if(value.contains("delivered")) {
                const auto delta=delivered_delta(value["delivered"],fresh?Json(nullptr):previous.value("delivered",Json(nullptr)));
                event["new_delivered_units"]=delta;event["actual_delivery_arrival"]=delta>0;
            }
            const auto id=static_cast<sim::CourierId>(value.at("courier").get<unsigned>());
            const auto& actor=w.courier(id);
            event["actual_prepared_path"]=trace::cells(actor.path);
            event["post_tick_actor_phase"]=sim::courier_phase_name(actor.phase);
            event["path_direction_boundary"]="Current post-tick path; after arrival it may already be reversed for physical return. Not reconstructed outbound history.";
            const auto edges=actor.path.empty()?0:actor.path.size()-1;
            const auto edge=sim::profile_rules(w.profile(),w.rule_version()).courier_edge_ticks;
            event["path_edges"]=edges;event["movement_ticks_per_edge"]=edge;
            event["stable_route_elapsed_one_way_lower_bound"]=edges?Json(edges*static_cast<unsigned>(edge)-1):Json(nullptr);
            event["stable_route_dispatch_period_lower_bound"]=edges*static_cast<unsigned>(edge)*2;
            event["route_bound_is_actual_dispatch_or_arrival"]=false;
            emit(w.ticks(),"actual_trip_change",std::move(event));
        }
        for(const auto& house:w.original_economy_state()->houses) {
            const auto id=static_cast<unsigned>(house.building_id);const auto prior=before_.houses.find(id);
            if(prior!=before_.houses.end())for(unsigned role=0;role<service_names.size();++role)
                if(prior->second["timers"][role].get<unsigned>()>0&&now.houses.at(id)["timers"][role].get<unsigned>()==0) {
                    const auto availability=w.original_service_availability(house.building_id,static_cast<orig::ServiceRole>(role));
                    emit(w.ticks(),"service_timer_expiry",{{"house",id},{"service",service_names[role]},
                        {"before_tick",before_.tick},{"timer_before",prior->second["timers"][role]},{"timer_after",0},
                        {"scheduler_phase",dispatch->position.phase},{"service_countdown",dispatch->service_countdown},
                        {"last_actual_arrival",availability.last_arrival_tick?Json(*availability.last_arrival_tick):Json(nullptr)},
                        {"available_sources",availability.available_source_count},{"reachable_sources",availability.reachable_source_count},
                        {"actually_travelling_to_house",availability.travelling_to_target_count},
                        {"available_does_not_mean_actor_idle",true},{"before",prior->second},{"after",now.houses.at(id)}});
                }
            const auto& h=high::history(w,house.building_id);
            if(h.last_evaluation&&h.last_evaluation->tick==w.ticks())emit(w.ticks(),"actual_phase39_evaluation",
                {{"house",id},{"evaluation",high::evaluation(*h.last_evaluation,w.original_rules_model()->house_rules())},
                 {"post_tick_house",now.houses.at(id)},{"market_need_query_post_tick",needs(w,house.building_id)},
                 {"pre_tick_allocation_event",staffing_reference_},{"post_tick_stock_is_not_evaluation_stock",true}});
        }
        if(const auto& fire=w.original_production_state()->fire_protection;fire)
            for(auto it=fire->arrivals.rbegin();it!=fire->arrivals.rend()&&it->tick==w.ticks();++it)
                emit(w.ticks(),"actual_fire_inspector_arrival",{{"courier",static_cast<unsigned>(it->trip.courier_id)},
                    {"owner",static_cast<unsigned>(it->trip.owner)},{"target",static_cast<unsigned>(it->trip.target)},
                    {"dispatched_tick",it->trip.dispatched_tick},{"arrival_tick",it->tick},{"extinguished",it->extinguished},
                    {"travelled",trace::cells(it->trip.travelled)},{"dispatch_assigned_workers",it->trip.assigned_workers}});
        last_=now;
    }
    Json finish(const sim::World& w) {
        stream_.flush();check(bool(stream_),"Cannot flush OE44 timeline");
        return {{"format","openemperor-elite-stability-diagnosis-v1"},{"technical_status",overflow_?"PARTIAL":"PASS"},
            {"causal_gate_status","UNASSESSED; requires evidence review"},{"overflow",overflow_},
            {"overflow_reason",overflow_reason_.empty()?Json(nullptr):Json(overflow_reason_)},
            {"event_limit",event_limit},{"byte_limit",byte_limit},{"events_written",events_},{"bytes_written",bytes_},
            {"begin_tick",begin_},{"simulation_end_tick",w.ticks()},{"last_recorded_tick",last_recorded_tick_},
            {"timeline",file_.filename().string()},{"world_mutation_by_observer",false},
            {"full_world_copy_or_bfs_by_observer",false},{"frozen_staffing_boundary","PRE next tick; never reconstructed from post-tick Ready"},
            {"dispatch_status_boundary","Read-only PRE/POST query, not an internal production-time dispatch attempt. POST Ready does not prove completed-tick availability."},
            {"ordinary_risk_increment_events",false},{"risk_observation","Actual PRE/POST risk retained at semantic building events; unchanged placement-relative20-tick risk rule, threshold100, is not reimplemented as a source of events."},
            {"candidate_selection_explanation","UNPROVEN; private Warehouse/Market selector not duplicated"},
            {"historical_report_format_changed",false},{"native_human","NOT_RUN"}};
    }
    // Explicit QA checkpoint access to the same read-only prepared-cache
    // observer; no Navigator query, refresh, World copy or saved authority.
    static Json prepared_routes(const sim::World& w) {return route_cache(w);}
private:
    static Json staffing(const Sample& s) {
        Json rows=Json::array();for(const auto& [id,b]:s.bodies)rows.push_back({{"id",id},
            {"assigned",b.at("workers_assigned")},{"installed",b.at("workers_required")},
            {"priority",b.at("priority")},{"operating",b.at("operating")},{"burning",b.at("burning")}});
        return rows;
    }
    static Json state(const Sample& s) {
        return {{"tick",s.tick},{"funds",s.funds},{"taxes",s.taxes},{"upkeep",s.upkeep},
            {"construction",s.construction},{"command_sequence",s.sequence},
            {"buildings",s.bodies},{"actors",s.actors},{"houses",s.houses},{"trips",s.trips}};
    }
    static Json route_cache(const sim::World& w) {
        Json actors=Json::array();for(const auto& c:w.couriers()) {
            Json routes=Json::array();for(const auto& r:c.dynamic_target_routes)routes.push_back({
                {"target",static_cast<unsigned>(r.first)},{"prepared_path_exists",r.second.has_value()},
                {"edges",r.second&& !r.second->empty()?Json(r.second->size()-1):Json(nullptr)}});
            actors.push_back({{"courier",static_cast<unsigned>(c.id)},{"cached_revision",c.cached_revision},{"routes",routes}});
        }
        return actors;
    }
    static int delivered_delta(const Json& now,const Json& prior) {
        int result=0;if(now.is_array())for(std::size_t i=0;i<now.size();++i)
            result+=now[i].get<int>()-(prior.is_array()?prior[i].get<int>():0);
        else result=now.get<int>()-(prior.is_number()?prior.get<int>():0);
        check(result>=0,"OE44 retained transfer delivery regressed without a new sequence");return result;
    }
    static Json needs(const sim::World& w,sim::BuildingId id) {
        auto house=oe_starter_fixture::house(w,id);house.desirability=static_cast<std::int8_t>(w.original_house_influence_value(id));
        std::array<int,orig::goods_count> incoming{};
        for(const auto& r:w.original_goods_state()->reservations)if(r.target==id) {
            if(r.phase==sim::OriginalGoodsTransferPhase::Reserved)incoming[static_cast<unsigned>(r.good)]+=r.reserved;
            if(r.phase==sim::OriginalGoodsTransferPhase::Loaded)incoming[static_cast<unsigned>(r.good)]+=r.loaded-r.delivered-r.returned;
        }
        Json rows=Json::array();for(unsigned g=0;g<orig::goods_count;++g) {
            const auto need=sim::original_market_need(w.original_rules_model()->house_rules(),house,static_cast<orig::Goods>(g),incoming);
            rows.push_back({{"good",g},{"incoming",incoming[g]},
                {"shared_need",need?Json{{"tier",need->tier},{"stock",need->stock},{"demand",need->demand}}:Json(nullptr)}});
        }
        return rows;
    }
    static Sample capture(const sim::World& w) {
        check(w.buildings().size()<=82&&w.couriers().size()<=58&&w.original_economy_state()->houses.size()<=20,
            "OE44 observation exceeds retained production bounds");
        Sample s;s.tick=w.ticks();s.funds=w.treasury();s.taxes=w.taxes_collected_total();s.upkeep=w.maintenance_spent_total();
        s.construction=w.construction_spent_total();s.sequence=w.command_sequence();const auto& p=*w.original_production_state();
        for(const auto& b:w.buildings())if(b.placed) {
            auto row=trace::owner(w,b.id);row["fire_risk"]=b.fire_risk;row["fire_until_tick"]=b.fire_until_tick;
            row["fire_protection_until_tick"]=b.fire_protection_until_tick;row["placed_tick"]=b.placed_tick;
            for(const auto& f:p.food.farms)if(f.farm_id==b.id)row["food_farm"]={{"crop",static_cast<unsigned>(f.crop)},
                {"field",f.field_id?Json(static_cast<unsigned>(*f.field_id)):Json(nullptr)},{"harvested",f.harvested}};
            for(const auto& f:p.food.stocks)if(f.building_id==b.id) {
                int incoming=0,returning=0;for(const auto& t:p.food.trips)for(unsigned g=0;g<sim::original_food_crop_count;++g) {
                    const int left=t.loaded[g]-t.delivered[g]-t.returned[g];
                    if(t.target==b.id&&t.phase==sim::OriginalGoodsTransferPhase::Loaded)incoming+=left;
                    if(t.source==b.id)returning+=left;
                }
                row["food"]={{"amounts",f.amounts},{"received",f.received},{"departed",f.departed},{"returned",f.returned},
                    {"consumed",f.consumed},{"missed_demands",f.missed_demands},{"incoming_cargo",incoming},
                    {"source_return_room",returning},{"capacity",b.kind==sim::Object::Market?16:8},
                    {"headroom",(b.kind==sim::Object::Market?16:8)-f.amounts[0]-f.amounts[1]-incoming-returning},
                    {"quality_witness",high::quality_witness(f.quality_witness)}};
            }
            for(const auto& a:p.industry.auxiliary)if(a.building_id==b.id)row["auxiliary"]={{"amounts",a.amounts},
                {"received",a.received},{"departed",a.departed},{"returned",a.returned},{"consumed",a.consumed}};
            for(const auto& a:p.industry.sources)if(a.building_id==b.id) {
                row["industry"]={{"completed",a.completed},{"goods_inputs_used",a.goods_inputs_used},{"aux_inputs_used",a.aux_inputs_used},
                    {"clay_received",a.clay_received},{"clay_used",a.clay_used}};row["industry_progress"]=a.progress;
            }
            s.bodies.emplace(static_cast<unsigned>(b.id),std::move(row));
        }
        for(const auto& c:w.couriers()) {
            const auto d=w.courier_dispatch_status(c.id);Json at=nullptr,next=nullptr;
            if(c.path_vertex<c.path.size())at=trace::matrix::cell(c.path[c.path_vertex]);
            if(c.path_vertex+1<c.path.size())next=trace::matrix::cell(c.path[c.path_vertex+1]);
            s.actors.emplace(static_cast<unsigned>(c.id),Json{{"id",static_cast<unsigned>(c.id)},{"owner",static_cast<unsigned>(c.owner)},
                {"role",static_cast<unsigned>(c.role)},{"phase",sim::courier_phase_name(c.phase)},{"target",static_cast<unsigned>(c.target)},
                {"dispatch_status",sim::courier_dispatch_status_name(d.status)},
                {"dispatch_selected_target",d.selected_target?Json(static_cast<unsigned>(*d.selected_target)):Json(nullptr)},
                {"native_cargo",c.cargo},{"native_reserved",c.reserved},{"typed_or_food_aux_cargo",w.original_courier_cargo(c.id)},
                {"last_target",c.last_dispatched_target?Json(static_cast<unsigned>(*c.last_dispatched_target)):Json(nullptr)},
                {"path_edges",c.path.empty()?0:c.path.size()-1},{"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress},
                {"current_cell",at},{"next_cell",next},{"route_pending",c.route_pending},{"cached_revision",c.cached_revision}});
        }
        for(const auto& h:w.original_economy_state()->houses) {
            Json timers=Json::array(),arrivals=Json::array();for(unsigned role=0;role<service_names.size();++role) {
                timers.push_back(orig::service_timer(h.house,static_cast<orig::ServiceRole>(role)));Json witness=nullptr;
                for(const auto& v:p.services.witnesses) {
                    if(v.building_id==h.building_id&&static_cast<unsigned>(v.role)==role)
                        witness=high::service_witness(v);
                }
                arrivals.push_back(std::move(witness));
            }
            const auto tax=w.original_house_tax_status(h.building_id);
            Json row{{"state",trace::matrix::state(h.house)},
                {"timers",timers},{"service_role_order",service_names},{"last_actual_service_arrivals",arrivals},
                {"burning",w.building_on_fire(h.building_id)},{"cached_des",w.original_house_influence_value(h.building_id)},
                {"tax_if_assessed_now",tax.amount_if_assessed_now},{"tax_eligible_now",tax.eligible},{"tax_paid_total",tax.paid_total}};
            for(const auto& water:p.water.houses)if(water.building_id==h.building_id)row["water"]={
                {"normal_timer",water.normal_timer},{"special_timer",water.special_timer},
                {"current_witness",high::water_witness(water.current_witness)},
                {"evaluation_witness",high::water_witness(water.evaluation_witness)}};
            s.houses.emplace(static_cast<unsigned>(h.building_id),std::move(row));
        }
        const auto add=[&](const char* family,const auto& t,Json row) {
            row["family"]=family;row["courier"]=static_cast<unsigned>(t.transfer.courier_id);row["sequence"]=t.transfer.sequence;
            row["target"]=static_cast<unsigned>(t.target);s.trips.emplace(std::string(family)+":"+std::to_string(static_cast<unsigned>(t.transfer.courier_id)),std::move(row));
        };
        for(const auto& t:p.food.trips)add("food",t,{{"source",static_cast<unsigned>(t.source)},{"phase",static_cast<unsigned>(t.phase)},
            {"loaded",t.loaded},{"delivered",t.delivered},{"returned",t.returned}});
        for(const auto& t:w.original_goods_state()->reservations)add("goods",t,{{"source",static_cast<unsigned>(t.source)},
            {"good",static_cast<unsigned>(t.good)},{"phase",static_cast<unsigned>(t.phase)},{"reserved",t.reserved},
            {"loaded",t.loaded},{"delivered",t.delivered},{"returned",t.returned}});
        for(const auto& t:p.industry.aux_trips)add("auxiliary",t,{{"source",static_cast<unsigned>(t.source)},
            {"material",static_cast<unsigned>(t.material)},{"phase",static_cast<unsigned>(t.phase)},{"loaded",t.loaded},
            {"delivered",t.delivered},{"returned",t.returned},{"dispatched_tick",t.dispatched_tick},
            {"arrival_tick",t.arrival_tick?Json(*t.arrival_tick):Json(nullptr)}});
        for(const auto& t:p.industry.clay_trips)add("native_clay",t,{{"source",static_cast<unsigned>(t.source)},
            {"loaded",t.loaded},{"delivered",t.delivered},{"dispatched_tick",t.dispatched_tick},
            {"arrival_tick",t.arrival_tick?Json(*t.arrival_tick):Json(nullptr)}});
        const auto visits=[&](const char* family,const auto& trips) {for(const auto& t:trips)add(family,t,
            {{"dispatched_tick",t.dispatched_tick},{"arrival_tick",t.arrival_tick?Json(*t.arrival_tick):Json(nullptr)},{"visit_applied",t.visit_applied}});};
        visits("service",p.services.trips);visits("water",p.water.trips);
        if(p.fire_protection)for(const auto& t:p.fire_protection->trips)
            s.trips.emplace("fire:"+std::to_string(static_cast<unsigned>(t.courier_id)),Json{
                {"family","fire"},{"courier",static_cast<unsigned>(t.courier_id)},{"owner",static_cast<unsigned>(t.owner)},
                {"target",static_cast<unsigned>(t.target)},{"dispatched_tick",t.dispatched_tick},
                {"road_revision",t.road_revision},{"dispatch_assigned_workers",t.assigned_workers},
                {"dispatch_operating",t.operating},{"dispatch_population",t.population}});
        return s;
    }
    void changes(const sim::World& w,const Rows& now,const Rows& before,const char* kind,std::initializer_list<const char*> ignored) {
        for(const auto& [id,row]:now) {
            const auto old=before.find(id);Json a=old==before.end()?Json(nullptr):old->second,b=row;
            for(const auto key:ignored){if(a.is_object())a.erase(key);b.erase(key);}
            if(a!=b) {
                Json event{{"id",id},{"before",old==before.end()?Json(nullptr):old->second},
                    {"after",row},{"pre_tick_allocation_boundary",before_.tick}};
                if(std::string_view(kind)=="actor_state") {
                    const auto& c=w.courier(static_cast<sim::CourierId>(id));Json routes=Json::array();
                    for(const auto& r:c.dynamic_target_routes)routes.push_back({{"target",static_cast<unsigned>(r.first)},
                        {"prepared_path_exists",r.second.has_value()},{"edges",r.second&&!r.second->empty()?Json(r.second->size()-1):Json(nullptr)}});
                    event["prepared_route_cache"]=std::move(routes);
                }
                emit(w.ticks(),kind,std::move(event));
            }
        }
    }
    void emit(std::uint64_t tick,const char* kind,Json fields) {
        if(overflow_)return;
        fields["format"]="openemperor-elite-stability-event-v1";fields["sequence"]=events_+1;
        fields["tick"]=tick;fields["kind"]=kind;
        const auto line=fields.dump();
        if(events_>=event_limit||line.size()+1>byte_limit-bytes_) {
            overflow_=true;overflow_reason_=events_>=event_limit?"event limit":"byte limit";return;
        }
        stream_<<line<<'\n';check(bool(stream_),"Cannot append OE44 timeline");
        ++events_;bytes_+=line.size()+1;last_recorded_tick_=tick;
    }
    std::uint64_t begin_=0,last_recorded_tick_=0;
    std::filesystem::path file_;std::ofstream stream_;Sample before_,last_;
    std::size_t events_=0,bytes_=0,staffing_reference_=0;bool overflow_=false;std::string overflow_reason_;Json last_staffing_;
};
} // namespace oe_elite_stability_diagnosis
