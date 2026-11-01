#pragma once

// Explicit OE45 external observation. Never installed in World or a frame loop.
#include "EliteStabilityDiagnosis.h"

namespace oe_elite_fire_coverage_metrics {
namespace sim=openemperor::simulation;
using Json=nlohmann::json;
using oe_starter_fixture::check;

inline Json prepared_capacity(const sim::World& w) {
    Json actors=Json::array(),targets=Json::array();
    std::uint64_t optimistic_edges=0;unsigned unreachable=0,inspectors=0;
    bool current=true;
    for(const auto& c:w.couriers())if(c.role==sim::CourierRole::FireInspector) {
        ++inspectors;current=current&&c.cached_revision==w.road_revision();
        Json routes=Json::array();
        for(const auto& [target,path]:c.dynamic_target_routes)
            routes.push_back({{"target",static_cast<unsigned>(target)},
                {"edges",path&&!path->empty()?Json(path->size()-1):Json(nullptr)}});
        actors.push_back({{"courier",static_cast<unsigned>(c.id)},{"owner",static_cast<unsigned>(c.owner)},
            {"cache_current",c.cached_revision==w.road_revision()},{"routes",routes}});
    }
    for(const auto& b:w.buildings())if(b.placed&&sim::fire_eligible(b.kind,w.profile(),w.rule_version())) {
        std::optional<std::uint64_t> best;
        for(const auto& c:w.couriers())if(c.role==sim::CourierRole::FireInspector&&c.owner!=b.id)
            for(const auto& [target,path]:c.dynamic_target_routes)if(target==b.id&&path&&!path->empty()) {
                const auto edges=static_cast<std::uint64_t>(path->size()-1);
                if(!best||edges<*best)best=edges;
            }
        if(best)optimistic_edges+=*best;else ++unreachable;
        targets.push_back({{"target",static_cast<unsigned>(b.id)},
            {"shortest_prepared_edges",best?Json(*best):Json(nullptr)},
            {"operating",b.operating_enabled},{"burning",w.building_on_fire(b.id)}});
    }
    const auto edge=static_cast<unsigned>(sim::profile_rules(w.profile(),w.rule_version()).courier_edge_ticks);
    const auto maximum_gap=sim::Rules::fire_protection_ticks+
        sim::Rules::fire_risk_step_ticks*sim::Rules::fire_risk_threshold;
    return {{"tick",w.ticks()},{"road_revision",w.road_revision()},{"all_caches_current",current},
        {"inspectors",inspectors},{"targets",targets},{"prepared_routes",actors},
        {"targets_without_prepared_route",unreachable},{"optimistic_roundtrip_work_ticks",optimistic_edges*edge*2},
        {"optimistic_all_actor_capacity_ticks",maximum_gap*inspectors},
        {"scope","Necessary relaxed all-body no-fire workload only; ignores assignment, return asymmetry, waits, dispatch overhead, source fire and staffing. Not a feasibility or fairness proof."},
        {"no_BFS_or_route_refresh",true}};
}

class Observation {
    struct Body {std::uint64_t burning=0,starts=0;bool previous=false;};
    struct Actor {std::uint64_t outbound=0,returning=0,idle=0,pending=0,unstaffed=0;};
public:
    Observation(const sim::World& w,const std::filesystem::path& out):begin_(w.ticks()),trace_begin_(w.ticks()) {
        const auto path=out/"oe45-inspectors.csv";check(!std::filesystem::exists(path),"Preserve OE45 Inspector trace");
        stream_.open(path,std::ios::out|std::ios::binary);check(bool(stream_),"Cannot open OE45 Inspector trace");
        stream_<<"tick,courier,owner,target,phase,vertex,edge_progress,path_edges,route_pending,reroutes,pre_workers,pre_required,pre_operating,pre_source_fire,target_risk,protection_until,fire_until\n";
        reset(w);
    }
    void before(const sim::World& w) {
        before_=w.ticks();pre_.clear();
        for(const auto& c:w.couriers())if(c.role==sim::CourierRole::FireInspector) {
            const auto& b=w.building(c.owner);
            pre_[c.id]={w.workers_assigned(c.owner),w.workforce_required(c.owner),b.operating_enabled,w.building_on_fire(c.owner)};
        }
    }
    void after(const sim::World& w) {
        check(w.ticks()==before_+1,"OE45 observer requires exactly one real tick");
        for(const auto& b:w.buildings())if(b.placed) {
            auto& row=bodies_[b.id];const bool fire=w.building_on_fire(b.id);
            row.burning+=fire;row.starts+=fire&&!row.previous;row.previous=fire;
        }
        std::optional<sim::BuildingId> incoming;
        for(const auto& c:w.couriers())if(c.role==sim::CourierRole::FireInspector) {
            const auto& b=w.building(c.target);const auto found=pre_.find(c.id);
            const Pre pre=found==pre_.end()?Pre{}:found->second;
            auto& row=actors_[c.id];
            row.outbound+=c.phase==sim::CourierPhase::ToWarehouse;
            row.returning+=c.phase==sim::CourierPhase::Returning;
            row.idle+=c.phase==sim::CourierPhase::IdleAtWorkshop;
            row.pending+=c.route_pending;row.unstaffed+=pre.assigned<pre.required;
            if(c.enabled&&c.phase==sim::CourierPhase::ToWarehouse&&!c.route_pending) {
                if(incoming==c.target)++duplicate_ticks_;incoming=c.target;
            }
            if(overflow_)continue;
            if(rows_>=262144||stream_.tellp()>=64*1024*1024) {overflow_=true;continue;}
            stream_<<w.ticks()<<','<<static_cast<unsigned>(c.id)<<','<<static_cast<unsigned>(c.owner)<<','
                <<static_cast<unsigned>(c.target)<<','<<static_cast<unsigned>(c.phase)<<','<<c.path_vertex<<','
                <<c.edge_progress<<','<<(c.path.empty()?0:c.path.size()-1)<<','<<c.route_pending<<','<<c.reroute_attempts<<','
                <<pre.assigned<<','<<pre.required<<','<<pre.operating<<','<<pre.fire<<','<<b.fire_risk<<','
                <<b.fire_protection_until_tick<<','<<b.fire_until_tick<<'\n';++rows_;
        }
    }
    void begin_retention(const sim::World& w) {reset(w);entry_capacity_=prepared_capacity(w);}
    Json finish(const sim::World& w) {
        stream_.flush();check(bool(stream_),"Cannot flush OE45 Inspector trace");
        Json bodies=Json::array(),actors=Json::array();
        for(const auto& [id,row]:bodies_)bodies.push_back({{"id",static_cast<unsigned>(id)},
            {"burning_ticks",row.burning},{"fire_starts",row.starts}});
        for(const auto& [id,row]:actors_)actors.push_back({{"courier",static_cast<unsigned>(id)},
            {"outbound_ticks",row.outbound},{"return_ticks",row.returning},{"idle_ticks",row.idle},
            {"route_pending_ticks",row.pending},{"pre_understaffed_ticks",row.unstaffed}});
        return {{"format","openemperor-oe45-fire-observation-v1"},{"begin_tick",begin_},{"end_tick",w.ticks()},
            {"per_tick_trace","oe45-inspectors.csv"},{"trace_begin_tick",trace_begin_},{"trace_rows",rows_},{"trace_overflow",overflow_},
            {"trace_scope","Whole process trace, including pre-entry ticks. POST physical actor and target; PRE frozen source staffing/operation/fire. Actual routes and arrivals are in oe44-timeline.jsonl. Window statistics begin at begin_tick."},
            {"window_buildings",bodies},{"window_inspectors",actors},{"duplicate_incoming_ticks",duplicate_ticks_},
            {"entry_capacity",entry_capacity_},{"final_capacity",prepared_capacity(w)},
            {"world_mutation_by_observer",false},{"native_human","NOT_RUN"}};
    }
private:
    struct Pre {int assigned=0,required=0;bool operating=false,fire=false;};
    void reset(const sim::World& w) {
        begin_=w.ticks();bodies_.clear();actors_.clear();duplicate_ticks_=0;
        for(const auto& b:w.buildings())if(b.placed)bodies_[b.id].previous=w.building_on_fire(b.id);
    }
    std::uint64_t begin_=0,trace_begin_=0,before_=0,rows_=0,duplicate_ticks_=0;bool overflow_=false;
    std::ofstream stream_;std::map<sim::BuildingId,Body> bodies_;
    std::map<sim::CourierId,Actor> actors_;std::map<sim::CourierId,Pre> pre_;
    Json entry_capacity_=nullptr;
};
}
