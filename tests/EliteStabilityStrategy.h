#pragma once

// Explicit OE44/OE45 player experiments. The historical OE40 plan remains immutable.
// Every mutation below goes through its existing ordinary validate/execute path.
#include "EliteDesirabilityStrategy.h"
#include <algorithm>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace oe_elite_stability_strategy {
namespace base=oe_elite_desirability_strategy;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace q=oe_starter_fixture;
using Json=nlohmann::json;
using C=sim::CommandType;
using Plan=std::remove_cv_t<decltype(base::plan)>;

inline bool oe48_recipe(std::string_view recipe) {
    return recipe=="oe48-watch-north"||recipe=="oe48-watch-west"||
        recipe=="oe48-watch-balanced"||recipe=="oe48-watch-central"||
        recipe=="oe48-watch-corridor"||recipe=="oe48-watch-early"||
        recipe=="oe48-watch-phase1"||recipe=="oe48-watch-corridor-early"||
        recipe=="oe48-grid-wide"||recipe=="oe48-watch-priority"||
        recipe=="oe48-farm-swap"||recipe=="oe48-market-north"||
        recipe=="oe48-watch-phase1-early-third-food"||recipe=="oe48-watch-phase1-early-all-food"||
        recipe=="oe48-watch-phase1-early-market"||recipe=="oe48-watch-phase1-farm-swap"||
        recipe=="oe48-watch-phase1-third-food-early-market"||
        recipe=="oe48-watch-phase1-third-food-three-markets"||recipe=="oe48-watch-phase1-third-food-four-markets";
}
inline bool oe47_recipe(std::string_view recipe) {
    return recipe=="oe47-south-door-three-markets"||recipe=="oe47-south-door-four-markets"||
        recipe=="oe47-south-door-market-south"||recipe=="oe47-south-door-early-third-food"||
        recipe=="oe47-south-door-early-all-food"||recipe=="oe47-south-door-early-markets";
}
inline bool oe46_recipe(std::string_view recipe) {
    return recipe=="oe46-ancestor-music-swap"||recipe=="oe46-ancestor-acrobat-swap"||
        recipe=="oe46-ancestor-drama-swap"||recipe=="oe46-market-south"||recipe=="oe46-markets-four"||
        recipe=="oe46-ancestor-west"||recipe=="oe46-ancestor-clinic"||recipe=="oe46-ancestor-music-early"||
        recipe=="oe46-ancestor-music-four-markets"||recipe=="oe46-ancestor-music-south-door";
}
inline bool oe45_grid_watch_layout(std::string_view recipe) {
    return recipe=="oe45-grid-watch-near"||recipe=="oe45-grid-watch-north"||
        recipe=="oe45-grid-watch-west"||recipe=="oe45-grid-balanced"||recipe=="oe45-grid-south-door";
}
inline bool oe45_recipe(std::string_view recipe) {
    return recipe=="oe45-grid"||recipe=="oe45-watch-early"||recipe=="oe45-watch-central"||
        recipe=="oe45-watch-west-door"||recipe=="oe45-medical-pair"||recipe=="oe45-market-near"||
        recipe=="oe45-warehouse-swap"||recipe=="oe45-grid-central"||oe45_grid_watch_layout(recipe);
}
inline bool known(std::string_view recipe) {
    return recipe=="road-grid"||recipe=="market-near"||recipe=="medical-near"||recipe=="medical-pair"||
        recipe=="watch-near"||recipe=="paper-buffer"||recipe=="early-elite"||
        recipe=="finance-staged"||recipe=="combined"||recipe=="early-religion"||
        recipe=="combined-religion"||recipe=="combined-services"||recipe=="combined-paper-usable"||
        recipe=="early-refined"||recipe=="combined-refined"||recipe=="early-stagger-watch"||
        recipe=="early-stagger-medical"||recipe=="early-stagger-all"||recipe=="early-adaptive-watch"||
        recipe=="early-stagger-medical-four-markets"||oe45_recipe(recipe)||oe46_recipe(recipe)||oe47_recipe(recipe)||oe48_recipe(recipe);
}
inline bool timing_recipe(std::string_view recipe) {
    return recipe=="early-stagger-watch"||recipe=="early-stagger-medical"||
        recipe=="early-stagger-all"||recipe=="early-adaptive-watch"||recipe=="early-stagger-medical-four-markets";
}
inline bool stagger_watch(std::string_view recipe){return recipe=="early-stagger-watch"||recipe=="early-stagger-all"||recipe=="early-adaptive-watch";}
inline bool stagger_medical(std::string_view recipe){return recipe=="early-stagger-medical"||recipe=="early-stagger-all"||recipe=="early-adaptive-watch"||recipe=="early-stagger-medical-four-markets";}
inline bool stagger_religion(std::string_view recipe){return recipe=="early-stagger-all"||recipe=="early-adaptive-watch";}
inline bool combined_family(std::string_view recipe) {
    return recipe=="combined"||recipe=="combined-religion"||recipe=="combined-services"||
        recipe=="combined-paper-usable"||recipe=="combined-refined";
}
inline bool refined(std::string_view recipe){return recipe=="early-refined"||recipe=="combined-refined"||oe45_recipe(recipe);}
inline bool active_religion(std::string_view recipe) {
    return recipe=="early-religion"||recipe=="combined-religion"||recipe=="combined-services"||refined(recipe)||timing_recipe(recipe);
}
inline bool usable_paper(std::string_view recipe){return recipe=="combined-paper-usable"||refined(recipe)||timing_recipe(recipe);}
inline bool grid(std::string_view recipe){return recipe=="road-grid"||combined_family(recipe)||recipe=="oe45-grid"||recipe=="oe45-grid-central"||oe45_grid_watch_layout(recipe);}
inline bool medical(std::string_view recipe){return recipe=="medical-near"||recipe=="medical-pair"||combined_family(recipe);}
inline bool medical_pair(std::string_view recipe){return recipe=="medical-pair"||combined_family(recipe)||recipe=="oe45-medical-pair";}
inline bool watches(std::string_view recipe){return recipe=="watch-near"||combined_family(recipe);}
inline bool paper_buffer(std::string_view recipe){return recipe=="paper-buffer"||combined_family(recipe)||recipe=="early-refined"||timing_recipe(recipe)||oe45_recipe(recipe);}
inline bool early_elite(std::string_view recipe){return recipe=="early-elite"||recipe=="early-religion"||refined(recipe)||combined_family(recipe)||timing_recipe(recipe);}
inline bool finance(std::string_view recipe){return recipe=="finance-staged";}

inline Plan plan_for(std::string_view recipe) {
    q::check(known(recipe),"Unknown explicit paid OE44 player strategy");
    if(oe48_recipe(recipe)) {
        auto result=plan_for("oe47-south-door-market-south");
        for(auto& node:result) {
            if(recipe=="oe48-watch-north"&&node.plan_id==131)node.cell={120,124};
            if((recipe=="oe48-watch-west"||recipe=="oe48-watch-balanced")&&node.plan_id==131)node.cell={113,128};
            if(recipe=="oe48-watch-central"&&node.plan_id==131)node.cell={124,135};
            if((recipe=="oe48-watch-balanced"||recipe=="oe48-watch-central")&&node.plan_id==132)node.cell={127,127};
            if((recipe=="oe48-watch-corridor"||recipe=="oe48-watch-corridor-early")&&node.plan_id==132)node.cell={130,132};
            if((recipe=="oe48-watch-early"||recipe=="oe48-watch-corridor-early")&&node.plan_id==132)node.phase=0;
            if((recipe=="oe48-watch-phase1"||recipe.starts_with("oe48-watch-phase1-"))&&node.plan_id==132)node.phase=1;
            if((recipe=="oe48-watch-phase1-early-third-food"||recipe.starts_with("oe48-watch-phase1-third-food-"))&&
                (node.plan_id==105||node.plan_id==106))node.phase=1;
            if(recipe=="oe48-watch-phase1-early-all-food"&&node.plan_id>=101&&node.plan_id<=106)node.phase=0;
            if((recipe=="oe48-watch-phase1-early-market"||recipe=="oe48-watch-phase1-third-food-early-market")&&node.plan_id==125)node.phase=0;
            if((recipe=="oe48-watch-phase1-third-food-three-markets"||recipe=="oe48-watch-phase1-third-food-four-markets")&&
                node.plan_id==143)node.extra_market=false;
            if(recipe=="oe48-watch-phase1-third-food-four-markets"&&node.plan_id==144)node.extra_market=false;
            // Swap the two complete existing Farm/owned Field sites, while
            // retaining each node's original phase and exact owner binding.
            if(recipe=="oe48-farm-swap"||recipe=="oe48-watch-phase1-farm-swap") {
                if(node.plan_id==101)node.cell={114,131};
                if(node.plan_id==102)node.cell={117,131};
                if(node.plan_id==105)node.cell={114,128};
                if(node.plan_id==106)node.cell={117,128};
            }
            if(recipe=="oe48-market-north"&&node.plan_id==125)node.cell={113,120};
        }
        return result;
    }
    if(oe47_recipe(recipe)) {
        auto result=plan_for("oe46-ancestor-music-south-door");
        for(auto& node:result) {
            if((recipe=="oe47-south-door-three-markets"||recipe=="oe47-south-door-four-markets"||
                recipe=="oe47-south-door-early-markets")&&node.plan_id==143)node.extra_market=false;
            if(recipe=="oe47-south-door-four-markets"&&node.plan_id==144)node.extra_market=false;
            if(recipe=="oe47-south-door-market-south"&&node.plan_id==125)node.cell={125,140};
            if(recipe=="oe47-south-door-early-third-food"&&(node.plan_id==105||node.plan_id==106))node.phase=1;
            if(recipe=="oe47-south-door-early-all-food"&&node.plan_id>=101&&node.plan_id<=106)node.phase=0;
            if(recipe=="oe47-south-door-early-markets"&&node.plan_id==125)node.phase=0;
            if(recipe=="oe47-south-door-early-markets"&&node.plan_id==143)node.phase=1;
        }
        return result;
    }
    if(oe46_recipe(recipe)) {
        auto result=plan_for(recipe=="oe46-ancestor-music-south-door"?"oe45-grid-south-door":"oe45-grid-watch-near");
        const unsigned other=recipe.starts_with("oe46-ancestor-music-")?11U:
            recipe=="oe46-ancestor-acrobat-swap"?13U:recipe=="oe46-ancestor-drama-swap"?15U:0U;
        const auto site=[&](unsigned key){for(const auto& node:result)if(node.plan_id==key)return node.cell;
            throw std::logic_error("OE46 paid site swap lost its existing plan node");};
        const auto shrine=site(17),culture=other?site(other):shrine;
        for(auto& node:result) {
            if(other&&node.plan_id==17)node.cell=culture;
            if(other&&node.plan_id==other)node.cell=shrine;
            if(recipe=="oe46-ancestor-west"&&node.plan_id==17)node.cell={110,117};
            if(recipe=="oe46-ancestor-clinic"&&node.plan_id==17)node.cell={110,122};
            if(recipe=="oe46-ancestor-music-early"&&node.plan_id==17)node.phase=2;
            if(recipe=="oe46-market-south"&&node.plan_id==125)node.cell={125,140};
            if((recipe=="oe46-markets-four"||recipe=="oe46-ancestor-music-four-markets")&&
                (node.plan_id==143||node.plan_id==144))node.extra_market=false;
        }
        return result;
    }
    auto result=base::plan;
    for(auto& node:result) {
        // Swap identical paid2x2 sites; the initially proposed126,140 site
        // was actually refused by production source admission.
        if(medical(recipe)&&node.plan_id==7)node.cell={121,136};
        if(medical(recipe)&&node.plan_id==130)node.cell={110,115};
        if(medical_pair(recipe)&&node.plan_id==8){node.lean_omit=false;node.duplicate=false;}
        if(stagger_medical(recipe)&&(node.plan_id==8||node.plan_id==10)){node.lean_omit=false;node.duplicate=false;}
        if(recipe=="early-stagger-medical-four-markets"&&(node.plan_id==143||node.plan_id==144))node.extra_market=false;
        if((recipe=="market-near"||combined_family(recipe))&&node.plan_id==125)node.cell={125,140};
        if(watches(recipe)&&node.plan_id==131)node.cell={118,134};
        if(watches(recipe)&&node.plan_id==132)node.cell={124,135};
        if(refined(recipe)&&node.plan_id==132)node.cell={130,132};
        // These independent paid contrasts inherit exactly early-refined's
        // controller, religion and early Elite purchase, then change only the
        // explicitly selected plan action. Admission remains World authority.
        if(recipe=="oe45-watch-early"&&node.plan_id==132)node.phase=0;
        if((recipe=="oe45-watch-central"||recipe=="oe45-grid-central")&&node.plan_id==131)node.cell={124,135};
        if(recipe=="oe45-watch-west-door"&&node.plan_id==132)node.cell={129,135};
        if(recipe=="oe45-medical-pair"&&node.plan_id==10){node.lean_omit=false;node.duplicate=false;}
        if(recipe=="oe45-market-near"&&node.plan_id==125)node.cell={125,140};
        if(recipe=="oe45-warehouse-swap"&&node.plan_id==124)node.cell={121,136};
        if(recipe=="oe45-warehouse-swap"&&node.plan_id==130)node.cell={118,136};
        // Bounded paid grid layouts only. New sites still require ordinary
        // production admission; there is no replacement site on refusal.
        if((recipe=="oe45-grid-watch-near"||recipe=="oe45-grid-south-door")&&node.plan_id==131)node.cell={118,134};
        if(recipe=="oe45-grid-watch-north"&&node.plan_id==131)node.cell={120,124};
        if((recipe=="oe45-grid-watch-west"||recipe=="oe45-grid-balanced")&&node.plan_id==131)node.cell={113,128};
        if(recipe=="oe45-grid-balanced"&&node.plan_id==132)node.cell={127,127};
        if(recipe=="oe45-grid-south-door"&&node.plan_id==132)node.cell={129,135};
        if(active_religion(recipe)&&(node.plan_id==20||node.plan_id==22))node.duplicate=false;
        if(recipe=="combined-services"&&(node.plan_id==11||node.plan_id==13||node.plan_id==15))node.duplicate=false;
    }
    return result;
}
inline const base::Planned& planned(const Plan& plan,unsigned id) {
    for(const auto& node:plan)if(node.plan_id==id)return node;
    throw std::runtime_error("Unknown derived OE44 owner plan ID");
}
inline bool included(const base::Planned& node){return base::included(node,"lean-med");}
inline bool medical_pair_active(const sim::World& w,const Plan& plan,std::string_view recipe) {
    if(!medical_pair(recipe))return false;
    for(const auto key:{7U,8U}) {
        const auto id=base::owner(w,planned(plan,key).cell);
        if(!id||!w.building(*id).operating_enabled)return false;
        q::check(w.building(*id).kind==sim::Object::HealthPost,"Medical pair lost a paid HealthPost");
    }
    return true;
}
inline void attachment(const sim::World& w,const Plan& plan,const base::Planned& node) {
    if(!node.owner)return;
    const auto parent=base::owner(w,planned(plan,node.owner).cell),child=base::owner(w,node.cell);
    q::check(parent&&child,"Derived paid attachment lost its actual parent or child");
    bool exact=false;
    if(node.command==C::PlaceOriginalMilletField||node.command==C::PlaceOriginalWheatField) {
        for(const auto& row:w.original_production_state()->food.farms)
            if(row.farm_id==*parent)exact=row.field_id==child;
    }else if(node.command==C::PlaceOriginalHempField) {
        for(const auto& row:w.original_production_state()->hemp_farms)
            if(row.farm_id==*parent)exact=row.field_id==child;
    }else {
        for(const auto& row:w.original_production_state()->industry.sources)
            if(row.building_id==*parent)exact=row.plant_id==child;
    }
    q::check(exact,"Derived attachment has another explicitly paid owner");
}
inline bool append(sim::World& w,sim::Command command,const char* reason,Json& commands,Json& blocked) {
    auto row=base::ordinary(w,command,reason);const bool ok=row.at("accepted");
    commands.push_back(row);if(!ok)blocked=row;return ok;
}
using Reserved=std::set<std::pair<int,int>>;
inline Reserved reserved_footprints(const sim::World& w,const Plan& plan) {
    Reserved reserved;
    for(const auto& node:plan)if(included(node)) {
        const auto kind=sim::placed_object(node.command);
        q::check(kind.has_value(),"Derived plan has an unmapped placement");
        for(const auto cell:sim::building_footprint_cells(w.profile(),w.rule_version(),*kind,node.cell))
            reserved.emplace(cell.x,cell.y);
    }
    // Reserve the future materialized body, not just the initial paid plot.
    const auto elite=sim::building_footprint_cells(w.profile(),w.rule_version(),
        sim::Object::OriginalEliteHouse,base::elite_cell);
    q::check(elite.size()==16,"OE44 roads require the complete production Elite4x4 footprint");
    for(const auto cell:elite)reserved.emplace(cell.x,cell.y);
    return reserved;
}
inline bool grid_roads(sim::World& w,const Plan& plan,Json& commands,Json& blocked) {
    if(w.ticks()!=0)return true;
    constexpr int left=108,right=136,top=110,bottom=143;
    static_assert((right-left+1)*(bottom-top+1)<=1024);
    const auto reserved=reserved_footprints(w,plan);
    for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x) {
        const sim::Cell cell{x,y};
        if(reserved.contains({x,y})||base::initial_road(w,cell))continue;
        const sim::Command command{C::PlaceRoad,cell};
        // Probe actual production legality; source/protection/height refusals
        // are not replaced by a parallel navigator or admission list.
        if(!w.validate(command).accepted)continue;
        if(!append(w,command,"OE44 paid tick0 grid road outside every future footprint",commands,blocked))return false;
    }
    return true;
}
inline bool oe48_outer_grid_roads(sim::World& w,const Plan& plan,Json& commands,Json& blocked) {
    if(w.ticks()!=0)return true;
    // Only this explicit paid control adds the bounded outer ring. Existing
    // grid/controller behavior and all production road admission stay exact.
    constexpr int left=106,right=138,top=108,bottom=145;
    static_assert((right-left+1)*(bottom-top+1)-986==268);
    const auto reserved=reserved_footprints(w,plan);
    for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x) {
        if(x>=108&&x<=136&&y>=110&&y<=143)continue;
        const sim::Cell cell{x,y};
        if(reserved.contains({x,y})||base::initial_road(w,cell))continue;
        const sim::Command command{C::PlaceRoad,cell};
        if(!w.validate(command).accepted)continue;
        if(!append(w,command,"OE48 actual paid outer grid road outside complete future footprints",commands,blocked))return false;
    }
    return true;
}
inline constexpr std::array<sim::Cell,6> watch_spur{{
    {118,135},{119,135},{120,135},{120,136},{120,137},{120,138}}};
inline bool watch_roads(sim::World& w,Json& commands,Json& blocked) {
    if(w.ticks()!=0)return true;
    for(const auto cell:watch_spur)if(!base::initial_road(w,cell))
        if(!append(w,{C::PlaceRoad,cell},"OE44 actual paid road spur for relocated Watch",commands,blocked))return false;
    return true;
}
inline unsigned actual_elite_type(const sim::World& w) {
    const auto target=base::elite(w);
    return target?static_cast<unsigned>(q::house(w,*target).type):0U;
}
struct WatchArrivals {
    std::optional<sim::BuildingId> owner;
    std::optional<std::uint64_t> first,last;
};
inline WatchArrivals actual_watch_arrivals(const sim::World& w,const Plan& plan,unsigned key) {
    WatchArrivals result;result.owner=base::owner(w,planned(plan,key).cell);
    const auto target=base::elite(w);const auto& fire=w.original_production_state()->fire_protection;
    if(!result.owner||!target||!fire)return result;
    q::check(w.building(*result.owner).kind==sim::Object::FireWatch,"Timing source is not its paid Watch");
    for(const auto& arrival:fire->arrivals)if(arrival.trip.owner==*result.owner&&arrival.trip.target==*target) {
        if(!result.first||arrival.tick<*result.first)result.first=arrival.tick;
        if(!result.last||arrival.tick>*result.last)result.last=arrival.tick;
    }
    return result;
}
inline Json watch_arrival_status(const WatchArrivals& value) {
    return {{"purchased_owner",value.owner?Json(static_cast<unsigned>(*value.owner)):Json(nullptr)},
        {"first_actual_elite_arrival",value.first?Json(*value.first):Json(nullptr)},
        {"latest_actual_elite_arrival",value.last?Json(*value.last):Json(nullptr)}};
}
inline std::optional<sim::OriginalServiceWitness> primary_service_witness(const sim::World& w,const Plan& plan,
    unsigned primary,orig::ServiceRole role) {
    const auto source=base::owner(w,planned(plan,primary).cell),target=base::elite(w);
    if(!source||!target)return {};
    for(const auto& witness:w.original_production_state()->services.witnesses)
        if(witness.provider_id==*source&&witness.building_id==*target&&witness.role==role)return witness;
    return {};
}
inline Json primary_cached_tour(const sim::World& w,const Plan& plan,unsigned primary,orig::ServiceRole role) {
    q::check(role==orig::ServiceRole::Daoist||role==orig::ServiceRole::Buddhist,"Cached religion tour requires an exact supported ServiceRole");
    const auto owner=base::owner(w,planned(plan,primary).cell);
    const auto wanted=role==orig::ServiceRole::Daoist?sim::CourierRole::OriginalDaoist:sim::CourierRole::OriginalBuddhist;
    const sim::CourierState* actor=nullptr;
    if(owner)for(const auto& c:w.couriers())if(c.owner==*owner&&c.role==wanted) {
        q::check(!actor,"Primary religion source has more than one matching actor");
        actor=&c;
    }
    const auto edge=sim::profile_rules(w.profile(),w.rule_version()).courier_edge_ticks;
    q::check(edge>0,"Cached physical tour has no positive existing edge clock");
    const bool current=actor&&actor->cached_revision==w.road_revision();
    Json routes=Json::array();bool complete=current;std::uint64_t edges=0;
    for(const auto& house:w.original_economy_state()->houses)if(house.house.occupants>0) {
        std::optional<std::uint64_t> length;
        if(actor) {
            const auto route=std::lower_bound(actor->dynamic_target_routes.begin(),actor->dynamic_target_routes.end(),house.building_id,
                [](const auto& entry,sim::BuildingId id){return entry.first<id;});
            if(route!=actor->dynamic_target_routes.end()&&route->first==house.building_id&&route->second&&!route->second->empty())
                length=route->second->size()-1;
        }
        complete=complete&&length.has_value();
        if(length)edges+=*length;
        routes.push_back({{"house",static_cast<unsigned>(house.building_id)},{"occupants",house.house.occupants},
            {"burning_included",w.building_on_fire(house.building_id)},{"prepared_edges",length?Json(*length):Json(nullptr)}});
    }
    complete=complete&&!routes.empty();
    const auto half=edges*static_cast<std::uint64_t>(edge);
    return {{"proven",complete},{"primary_owner",owner?Json(static_cast<unsigned>(*owner)):Json(nullptr)},
        {"primary_courier",actor?Json(static_cast<unsigned>(actor->id)):Json(nullptr)},{"service_role",static_cast<unsigned>(role)},
        {"cached_revision",actor?Json(actor->cached_revision):Json(nullptr)},{"world_road_revision",w.road_revision()},
        {"cache_current",current},{"routes_to_all_positive_occupant_houses",routes},{"route_count",routes.size()},
        {"prepared_edges_sum",complete?Json(edges):Json(nullptr)},{"existing_ticks_per_edge",edge},
        {"full_tour_movement_lower_bound_ticks",complete?Json(half*2):Json(nullptr)},
        {"half_tour_purchase_delay_ticks",complete?Json(half):Json(nullptr)},
        {"actual_eligibility_policy_not_reimplemented",true},{"no_BFS_or_route_refresh",true},
        {"movement_bound_is_not_an_actual_sweep_or_visit_guarantee",true}};
}
inline std::optional<Json> purchase_timing(const sim::World& w,const Plan& plan,unsigned key,std::string_view recipe) {
    unsigned primary=0;std::optional<std::uint64_t> delay,arrival;
    Json witness=nullptr,tour=nullptr;const char* authority="";
    if(stagger_watch(recipe)&&key==132) {
        primary=131;delay=1200;const auto visits=actual_watch_arrivals(w,plan,primary);arrival=visits.first;
        witness=watch_arrival_status(visits);authority="saved fire arrivals: exact first Watch owner and Elite target";
    }else {
        orig::ServiceRole role=orig::ServiceRole::Herbalist;
        if(stagger_medical(recipe)&&key==8){primary=7;delay=2448;role=orig::ServiceRole::Herbalist;}
        if(stagger_medical(recipe)&&key==10){primary=9;delay=2448;role=orig::ServiceRole::Acupuncture;}
        if(stagger_religion(recipe)&&key==20){primary=19;role=orig::ServiceRole::Daoist;}
        if(stagger_religion(recipe)&&key==22){primary=21;role=orig::ServiceRole::Buddhist;}
        if(!primary)return {};
        if(stagger_religion(recipe)&&(key==20||key==22)) {
            tour=primary_cached_tour(w,plan,primary,role);
            if(tour.at("proven").get<bool>())delay=tour.at("half_tour_purchase_delay_ticks").get<std::uint64_t>();
        }
        if(const auto actual=primary_service_witness(w,plan,primary,role)) {
            arrival=actual->arrival_tick;witness={{"provider",static_cast<unsigned>(actual->provider_id)},
                {"house",static_cast<unsigned>(actual->building_id)},{"role",static_cast<unsigned>(actual->role)},
                {"actual_successful_arrival",actual->arrival_tick}};
        }
        authority="saved successful service witness: exact primary provider, Elite target and ServiceRole";
    }
    q::check(!arrival||*arrival<=w.ticks(),"Actual saved timing visit lies after the current World tick");
    const auto elapsed=arrival?std::optional{w.ticks()-*arrival}:std::nullopt;
    const bool ready=elapsed&&delay&&*elapsed>=*delay;
    return Json{{"plan_id",key},{"primary_plan_id",primary},{"required_elapsed_ticks",delay?Json(*delay):Json(nullptr)},
        {"actual_arrival_tick",arrival?Json(*arrival):Json(nullptr)},{"elapsed_ticks",elapsed?Json(*elapsed):Json(nullptr)},
        {"ready_from_current_saved_witness",ready},{"purchased",base::owner(w,planned(plan,key).cell).has_value()},
        {"population_stage_unlocked",base::phase(w)>=planned(plan,key).phase},{"witness",witness},{"authority",authority},
        {"physical_tour_timing",tour},{"timing_delay_proven",delay.has_value()},
        {"missing_or_overwritten_primary_witness_is_not_zero",true},{"no_unsaved_deadline_cache",true}};
}
inline bool timing_purchases_complete(const sim::World& w,std::string_view recipe) {
    if(!timing_recipe(recipe))return true;
    const auto plan=plan_for(recipe);
    for(const auto& node:plan)if(included(node)&&purchase_timing(w,plan,node.plan_id,recipe)&&!base::owner(w,node.cell))return false;
    return true;
}
inline bool active_pair(const sim::World& w,const Plan& plan,unsigned primary,unsigned duplicate) {
    for(const auto key:{primary,duplicate}) {
        const auto id=base::owner(w,planned(plan,key).cell);
        if(!id||!w.building(*id).operating_enabled)return false;
    }
    return true;
}
inline bool stage_ready(const sim::World& w,const base::Planned& node,std::string_view recipe) {
    if(timing_recipe(recipe))if(const auto gate=purchase_timing(w,plan_for(recipe),node.plan_id,recipe))
        return gate->at("ready_from_current_saved_witness").get<bool>();
    if(!finance(recipe)||node.phase<3)return true;
    const auto type=actual_elite_type(w);
    // The unchanged model already requires Silk for13->14 and Lacquer for
    //14->15. Delaying either chain until15 would deliberately deadlock it.
    if(node.plan_id>=134&&node.plan_id<=136)return type>=13;
    if(node.plan_id==133||(node.plan_id>=138&&node.plan_id<=140))return type>=14;
    if(node.plan_id==137||node.plan_id==23||node.plan_id==24)return type>=15;
    if(node.plan_id==141||node.plan_id==142)return type>=16;
    if(type>=15)return true;
    const auto target=base::elite(w);
    if(!target)return false;
    const auto diagnosis=w.original_housing_diagnosis(*target);
    // Buy an earlier DES source only when the current stage actually needs
    // its evolution threshold; never grant or project a DES change.
    return diagnosis.cached_desirability<diagnosis.current.high;
}
inline std::int64_t building_reserve(const sim::World& w,std::string_view recipe) {
    const auto installed=static_cast<std::int64_t>(w.workforce_required())*sim::original_worker_upkeep_funds;
    return finance(recipe)?std::max<std::int64_t>(1000,installed):installed;
}
inline bool door_watch_cell(const Plan& plan,sim::Cell cell,std::string_view recipe) {
    if(oe45_grid_watch_layout(recipe))return cell==planned(plan,131).cell||cell==planned(plan,132).cell;
    return refined(recipe)&&cell==planned(plan,132).cell;
}
inline void validate_oe45_reservations(const sim::World& w,const Plan& plan,std::string_view recipe) {
    if(!oe45_recipe(recipe)||w.ticks()!=0)return;
    Reserved reserved;
    const auto reserve=[&](sim::Object kind,sim::Cell origin) {
        for(const auto cell:sim::building_footprint_cells(w.profile(),w.rule_version(),kind,origin))
            q::check(reserved.emplace(cell.x,cell.y).second,"OE45 future paid production footprints overlap");
    };
    for(const auto& node:plan)if(included(node)) {
        const auto kind=sim::placed_object(node.command);
        q::check(kind.has_value(),"OE45 future paid plan has an unmapped placement");
        reserve(*kind,node.cell);
    }
    reserve(sim::Object::OriginalEliteHouse,base::elite_cell);
    for(const auto cell:base::roads)if(!door_watch_cell(plan,cell,recipe))
        q::check(!reserved.contains({cell.x,cell.y}),"OE45 basic paid road overlaps a future complete footprint");
}
inline bool apply(sim::World& w,std::string_view recipe,Json& commands,Json& blocked) {
    q::check(known(recipe)&&w.original_cardinal_water_enabled()&&
        (w.rule_version()==14||w.original_inspector_coordination_enabled()||w.original_deadline_fire_dispatch_enabled()||w.original_deadline_service_dispatch_enabled()||w.original_deadline_food_dispatch_enabled()),"Paid OE44 strategy requires exact Rule14 or explicit Inspector coordination Rule15");
    const auto selected=recipe;const auto plan=plan_for(recipe);
    // Explicit OE46 paid site contrasts reuse the unchanged OE45 controller.
    // Site changes and extra paid Markets still use production commands,
    // model prices, complete footprint reservations and ordinary admission.
    if(oe46_recipe(recipe)||oe47_recipe(recipe)||oe48_recipe(recipe))recipe="oe45-grid-watch-near";
    validate_oe45_reservations(w,plan,recipe);
    for(const auto cell:base::roads)if(!door_watch_cell(plan,cell,recipe)&&!base::initial_road(w,cell))
        if(!append(w,{C::PlaceRoad,cell},"OE44 original basic paid roads",commands,blocked))return false;
    if(early_elite(recipe)&&w.ticks()==0&&!base::elite(w))
        if(!append(w,{C::PlaceOriginalElitePlot,base::elite_cell},"OE44 buy separate Elite plot before every other building",commands,blocked))return false;
    if(grid(recipe)&&!grid_roads(w,plan,commands,blocked))return false;
    if(selected=="oe48-grid-wide"&&!oe48_outer_grid_roads(w,plan,commands,blocked))return false;
    if(watches(recipe)&&!watch_roads(w,commands,blocked))return false;
    const auto unlocked=base::phase(w);
    for(const auto& node:plan) {
        if(!included(node)||node.phase>unlocked)continue;
        const auto kind=sim::placed_object(node.command);
        q::check(kind.has_value(),"Derived player command has no production body");
        if(const auto id=base::owner(w,node.cell)) {
            q::check(w.building(*id).kind==*kind,"Derived paid site contains another body");
            attachment(w,plan,node);continue;
        }
        if(!stage_ready(w,node,recipe))continue;
        sim::Command command{node.command,node.cell};
        if(node.owner) {
            const auto parent=base::owner(w,planned(plan,node.owner).cell);
            if(!parent)continue;
            command.building_id=*parent;
        }
        if(node.phase>=3&&w.treasury()<w.construction_cost(node.command)+building_reserve(w,recipe))continue;
        const auto timing=timing_recipe(recipe)?purchase_timing(w,plan,node.plan_id,recipe):std::optional<Json>{};
        if(!append(w,command,"OE44 actual paid population/needs stage purchase",commands,blocked))return false;
        if(timing)commands.back()["actual_arrival_purchase_gate"]=*timing;
        attachment(w,plan,node);
        if(node.owner)commands.back()["exact_owner_binding_checked"]=true;
    }
    if(unlocked>=2&&!base::elite(w))
        if(!append(w,{C::PlaceOriginalElitePlot,base::elite_cell},"OE44 separate paid Elite family plot",commands,blocked))return false;
    for(const auto& node:plan)if(included(node)&&node.duplicate) {
        if(node.command==C::PlaceOriginalWaterWell||node.command==C::PlaceOriginalAncestorShrine)continue;
        if(const auto id=base::owner(w,node.cell);id&&w.building(*id).operating_enabled)
            if(!append(w,sim::set_building_operation(*id,false),"OE44 redundant provider pause retains installed upkeep",commands,blocked))return false;
    }
    const bool prioritize_medical=medical_pair_active(w,plan,recipe);
    for(const auto& b:w.buildings()) {
        const bool critical=b.kind==sim::Object::OriginalWaterWell72||b.kind==sim::Object::OriginalFoodFarm||
            b.kind==sim::Object::Market||b.kind==sim::Object::FireWatch||
            (prioritize_medical&&b.operating_enabled&&(b.kind==sim::Object::HealthPost||b.kind==sim::Object::ServicePost))||
            (stagger_medical(recipe)&&b.operating_enabled&&
             ((b.kind==sim::Object::HealthPost&&active_pair(w,plan,7,8))||
              (b.kind==sim::Object::ServicePost&&active_pair(w,plan,9,10))));
        const auto wanted=selected=="oe48-watch-priority"&&b.kind!=sim::Object::FireWatch ?
            sim::WorkforcePriority::Normal:sim::WorkforcePriority::High;
        if(critical&&b.workforce_priority!=wanted) {
            auto command=sim::set_building_workforce_priority(b.id,wanted);command.cell=b.cell;
            if(!append(w,command,wanted==sim::WorkforcePriority::Normal ?
                "OE48 actual non-Watch critical sources retain normal workforce priority":
                "OE44 actual critical-source workforce priority",commands,blocked))return false;
        }
    }
    return true;
}
struct UsablePaper {
    sim::OriginalAuxBalance global;
    std::uint64_t downstream_stock=0,downstream_cargo=0,maker_stock=0,maker_cargo=0;
    bool active_academies_buffered=false,active_academy_empty_without_incoming=false;
    Json academies=Json::array();
};
inline UsablePaper usable_paper_state(const sim::World& w,sim::BuildingId maker) {
    UsablePaper result;result.global=w.original_auxiliary_balance(sim::OriginalAuxMaterial::Paper);
    q::check(result.global.conserved,"Usable Paper controller observed a broken global auxiliary ledger");
    constexpr auto paper=static_cast<unsigned>(sim::OriginalAuxMaterial::Paper);
    const auto& industry=w.original_production_state()->industry;
    const auto downstream=[&](sim::BuildingId id) {
        const auto& b=w.building(id);
        return b.kind==sim::Object::Warehouse||
            (b.kind==sim::Object::OriginalConfucianAcademy219&&b.operating_enabled);
    };
    for(const auto& stock:industry.auxiliary) {
        const auto amount=static_cast<std::uint64_t>(stock.amounts[paper]);
        if(stock.building_id==maker)result.maker_stock+=amount;
        if(downstream(stock.building_id))result.downstream_stock+=amount;
    }
    for(const auto& trip:industry.aux_trips)if(trip.material==sim::OriginalAuxMaterial::Paper) {
        const auto amount=trip.loaded-trip.delivered-trip.returned;
        q::check(amount>=0,"Usable Paper controller observed negative actual cargo");
        if(trip.source==maker)result.maker_cargo+=static_cast<std::uint64_t>(amount);
        if(!amount)continue;
        q::check(trip.phase==sim::OriginalGoodsTransferPhase::Loaded||trip.phase==sim::OriginalGoodsTransferPhase::Returning,
            "Outstanding Paper cargo has no actual outbound or return phase");
        if(trip.phase==sim::OriginalGoodsTransferPhase::Loaded&&downstream(trip.target))
            result.downstream_cargo+=static_cast<std::uint64_t>(amount);
    }
    result.active_academies_buffered=true;
    for(const auto& b:w.buildings())if(b.placed&&b.kind==sim::Object::OriginalConfucianAcademy219&&b.operating_enabled) {
        std::uint64_t stock=0,incoming=0;
        for(const auto& row:industry.auxiliary)if(row.building_id==b.id)stock=static_cast<std::uint64_t>(row.amounts[paper]);
        for(const auto& trip:industry.aux_trips)if(trip.material==sim::OriginalAuxMaterial::Paper&&
            trip.target==b.id&&trip.phase==sim::OriginalGoodsTransferPhase::Loaded)
            incoming+=static_cast<std::uint64_t>(trip.loaded-trip.delivered-trip.returned);
        result.active_academies_buffered=result.active_academies_buffered&&stock>=4;
        result.active_academy_empty_without_incoming=result.active_academy_empty_without_incoming||(stock<=1&&incoming==0);
        result.academies.push_back({{"owner",static_cast<unsigned>(b.id)},{"physical_stock",stock},{"actual_loaded_incoming",incoming},
            {"burning",w.building_on_fire(b.id)},{"workers_assigned_post_query",w.workers_assigned(b.id)}});
    }
    result.active_academies_buffered=result.active_academies_buffered&&!result.academies.empty();
    q::check(result.downstream_stock+result.downstream_cargo<=result.global.stock+result.global.cargo,
        "Usable Paper observation double-counted physical inventory");
    return result;
}
inline Json usable_paper_status(const UsablePaper& paper) {
    const auto physical=paper.global.stock+paper.global.cargo,downstream=paper.downstream_stock+paper.downstream_cargo;
    return {{"controller","downstream8/16-active-academy-floor4"},{"global_physical_stock",paper.global.stock},
        {"global_actual_cargo",paper.global.cargo},{"global_physical_total",physical},{"global_conserved",paper.global.conserved},
        {"downstream_stock",paper.downstream_stock},{"downstream_actual_cargo",paper.downstream_cargo},{"downstream_total",downstream},
        {"excluded_from_control_but_retained_in_global_balance",physical-downstream},{"maker_physical_stock",paper.maker_stock},
        {"maker_outstanding_cargo",paper.maker_cargo},{"active_academies",paper.academies},
        {"all_active_academy_stock_at_least4",paper.active_academies_buffered},
        {"active_academy_stock_at_most1_without_loaded_incoming",paper.active_academy_empty_without_incoming},
        {"pause_downstream_at_or_above",16},{"resume_downstream_at_or_below",8},{"pause_requires_drained_maker",false},
        {"returning_cargo_excluded_from_control_not_global_balance",true},
        {"downstream_is_not_a_route_or_immediate_service_grant",true},{"installed_upkeep_retained",true}};
}
inline Json adaptive_watch_status(const sim::World& w,const Plan& plan) {
    const auto first=actual_watch_arrivals(w,plan,131),second=actual_watch_arrivals(w,plan,132);
    const bool both=first.last&&second.last;
    std::optional<std::uint64_t> gap,elapsed;
    if(both) {
        const auto latest=std::max(*first.last,*second.last),earliest=std::min(*first.last,*second.last);
        q::check(latest<=w.ticks(),"Adaptive Watch observed an arrival after the current World tick");
        gap=latest-earliest;elapsed=w.ticks()-latest;
    }
    const bool pause=both&&*gap<=600&&*elapsed<1200;
    return {{"tick",w.ticks()},{"first_watch",watch_arrival_status(first)},{"second_watch",watch_arrival_status(second)},
        {"both_exact_watch_owners_have_actual_elite_arrivals",both},{"latest_actual_arrival_gap",gap?Json(*gap):Json(nullptr)},
        {"elapsed_since_latest_actual_arrival",elapsed?Json(*elapsed):Json(nullptr)},{"close_gap_at_most",600},
        {"pause_only_while_elapsed_below",1200},{"pause_requested",pause},{"requested_operating",!pause},
        {"ordinary_operation_only_begun_movement_remains_World_authority",true},{"no_visit_timer_or_upkeep_grant",true},
        {"state_reconstructed_only_from_saved_fire_arrivals",true}};
}
inline bool maintenance(sim::World& w,std::string_view recipe,Json& commands,Json& blocked) {
    q::check(known(recipe),"Unknown paid strategy maintenance");
    if(oe46_recipe(recipe)||oe47_recipe(recipe)||oe48_recipe(recipe))recipe="oe45-grid-watch-near";
    if(recipe=="early-adaptive-watch") {
        const auto plan=plan_for(recipe);const auto id=base::owner(w,planned(plan,132).cell);
        if(id) {
            const auto observation=adaptive_watch_status(w,plan);
            const bool enabled=observation.at("requested_operating");
            if(w.building(*id).operating_enabled!=enabled) {
                auto command=sim::set_building_operation(*id,enabled);command.cell=w.building(*id).cell;
                const bool accepted=append(w,command,enabled?"OE44 adaptive Watch2 resume from saved actual arrival timing":
                    "OE44 adaptive Watch2 pause after close actual arrivals; installed upkeep retained",commands,blocked);
                commands.back()["watch_controller_observation"]=observation;
                if(!accepted)return false;
            }
        }
    }
    if(!paper_buffer(recipe))return true;
    const auto plan=plan_for(recipe);const auto& node=planned(plan,137);
    const auto id=base::owner(w,node.cell);if(!id)return true;
    q::check(w.building(*id).kind==sim::Object::OriginalPaperMaker45,"Paper controller lost its actual paid producer");
    if(usable_paper(recipe)) {
        const auto paper=usable_paper_state(w,*id);const auto downstream=paper.downstream_stock+paper.downstream_cargo;
        const bool enabled=w.building(*id).operating_enabled;
        const bool pause=enabled&&downstream>=16&&paper.active_academies_buffered;
        const bool resume=!enabled&&(downstream<=8||paper.active_academy_empty_without_incoming);
        if(pause||resume) {
            auto command=sim::set_building_operation(*id,resume);command.cell=w.building(*id).cell;
            const bool accepted=append(w,command,pause?"OE44 usable Paper pause: downstream16 and active Academy stock4":
                "OE44 usable Paper resume: downstream8 or Academy low without actual incoming",commands,blocked);
            commands.back()["paper_controller_observation"]=usable_paper_status(paper);
            return accepted;
        }
        return true;
    }
    const auto balance=w.original_auxiliary_balance(sim::OriginalAuxMaterial::Paper);
    q::check(balance.conserved,"Paper controller observed a broken physical auxiliary ledger");
    const auto physical=balance.stock+balance.cargo;const bool enabled=w.building(*id).operating_enabled;
    if((enabled&&physical>=16)||(!enabled&&physical<=8)) {
        auto command=sim::set_building_operation(*id,!enabled);command.cell=w.building(*id).cell;
        return append(w,command,enabled?"OE44 physical Paper buffer>=16 pause; upkeep retained":
            "OE44 physical Paper buffer<=8 resume; no stock grant",commands,blocked);
    }
    return true;
}
inline Json status(const sim::World& w,std::string_view recipe) {
    q::check(known(recipe),"Unknown paid strategy status");
    const auto selected=recipe;const auto plan=plan_for(recipe);auto result=base::status(w,"lean-med");
    if(oe46_recipe(recipe)||oe47_recipe(recipe)||oe48_recipe(recipe))recipe="oe45-grid-watch-near";
    Json pending=Json::array(),positions=Json::array();
    for(const auto& node:plan)if(included(node)) {
        positions.push_back({{"plan_id",node.plan_id},{"cell",{node.cell.x,node.cell.y}},
            {"owner_plan_id",node.owner},{"phase",node.phase},{"duplicate_paused",node.duplicate&&
             node.command!=C::PlaceOriginalWaterWell&&node.command!=C::PlaceOriginalAncestorShrine}});
        if(!base::owner(w,node.cell))pending.push_back({{"plan_id",node.plan_id},{"phase",node.phase},
            {"command",static_cast<unsigned>(node.command)},{"cell",{node.cell.x,node.cell.y}},
            {"actual_need_stage_ready",stage_ready(w,node,recipe)}});
    }
    result["pending_plan_nodes"]=std::move(pending);result["paid_strategy"]=std::string(selected);
    if(oe48_recipe(selected))result["oe48_paid_contrast"]={{"base_strategy","oe47-south-door-market-south"},
        {"watch1_north",selected=="oe48-watch-north"},{"watch1_west",selected=="oe48-watch-west"||selected=="oe48-watch-balanced"},
        {"watch1_central",selected=="oe48-watch-central"},
        {"watch2_original_site",selected=="oe48-watch-balanced"||selected=="oe48-watch-central"},
        {"watch2_north_door",selected=="oe48-watch-corridor"||selected=="oe48-watch-corridor-early"},
        {"watch2_phase0",selected=="oe48-watch-early"||selected=="oe48-watch-corridor-early"},
        {"watch2_phase1",selected=="oe48-watch-phase1"||selected.starts_with("oe48-watch-phase1-")},
        {"combined_from_phase1_watch_plan",selected.starts_with("oe48-watch-phase1-")},
        {"earlier_third_food_chain",selected=="oe48-watch-phase1-early-third-food"||selected.starts_with("oe48-watch-phase1-third-food-")},
        {"all_food_chains_phase0",selected=="oe48-watch-phase1-early-all-food"},
        {"market125_phase0",selected=="oe48-watch-phase1-early-market"||selected=="oe48-watch-phase1-third-food-early-market"},
        {"extra_paid_markets",selected=="oe48-watch-phase1-third-food-four-markets" ? 2:
            selected=="oe48-watch-phase1-third-food-three-markets" ? 1:0},
        {"only_watches_high_priority",selected=="oe48-watch-priority"},
        {"farm101_105_complete_site_swap",selected=="oe48-farm-swap"||selected=="oe48-watch-phase1-farm-swap"},
        {"market125_historical_north_site",selected=="oe48-market-north"},
        {"outer_paid_grid",selected=="oe48-grid-wide" ? Json{{"x_min",106},{"x_max",138},{"y_min",108},{"y_max",145},
            {"additional_candidate_cells",268},{"complete_future_footprints_reserved",true},
            {"only_production_admitted_cells_bought",true}}:Json(nullptr)},
        {"two_paid_watches_and_all13_house_sites_retained",true},
        {"ordinary_admission_workers_prices_upkeep_and_transport",true},
        {"no_house_id_targeting",true},{"no_engine_rule_change",true}};
    if(oe47_recipe(selected))result["oe47_paid_contrast"]={{"base_strategy","oe46-ancestor-music-south-door"},
        {"extra_paid_markets",selected=="oe47-south-door-four-markets"?2:
            selected=="oe47-south-door-three-markets"||selected=="oe47-south-door-early-markets"?1:0},
        {"relocated_market",selected=="oe47-south-door-market-south"},
        {"earlier_third_food_chain",selected=="oe47-south-door-early-third-food"},
        {"all_food_chains_bought_at_tick0",selected=="oe47-south-door-early-all-food"},
        {"earlier_paid_markets",selected=="oe47-south-door-early-markets"},
        {"same_watch_and_ancestor_sites",true},{"ordinary_workers_prices_upkeep_and_transport",true},
        {"no_house_id_targeting",true},{"no_engine_rule_change",true}};
    if(oe46_recipe(selected))result["oe46_paid_contrast"]={{"base_strategy","oe45-grid-watch-near"},
        {"ancestor_site_change",selected.starts_with("oe46-ancestor-")},
        {"ancestor_site_swap",selected.starts_with("oe46-ancestor-music-")||selected=="oe46-ancestor-acrobat-swap"||selected=="oe46-ancestor-drama-swap"},
        {"earlier_ancestor_purchase",selected=="oe46-ancestor-music-early"},
        {"south_door_second_watch",selected=="oe46-ancestor-music-south-door"},
        {"relocated_market",selected=="oe46-market-south"},{"extra_paid_markets",selected=="oe46-markets-four"||selected=="oe46-ancestor-music-four-markets"?2:0},
        {"ordinary_model_workers_and_prices",true},{"no_house_id_targeting",true},{"no_rule_change",true}};
    result["derived_plan"]=std::move(positions);result["building_reserve"]=building_reserve(w,recipe);
    result["grid_region"]=grid(recipe)?Json{{"x_min",108},{"x_max",136},{"y_min",110},{"y_max",143},
        {"candidate_cell_bound",986},{"production_footprints_reserved",true},{"tick0_only",true}}:Json(nullptr);
    result["early_elite"]=early_elite(recipe);result["financial_staging"]=finance(recipe);
    result["medical_site_swap"]=medical(recipe);
    result["medical_high_priority_when_pair_active"]=medical_pair(recipe);
    result["medical_pair_active_now"]=medical_pair_active(w,plan,recipe);
    if(oe45_recipe(recipe))result["oe45_paid_contrast"]={{"base_strategy","early-refined"},
        {"complete_future_footprints_reserved",true},{"admission_and_routes","ordinary production World"},
        {"early_second_watch",recipe=="oe45-watch-early"},
        {"central_first_watch",recipe=="oe45-watch-central"||recipe=="oe45-grid-central"},
        {"west_door_second_watch",recipe=="oe45-watch-west-door"},
        {"medical_pairs_enabled",recipe=="oe45-medical-pair"},
        {"near_market",recipe=="oe45-market-near"},{"warehouse_pottery_site_swap",recipe=="oe45-warehouse-swap"},
        {"saved_state_or_resource_grants",false}};
    if(watches(recipe)) {
        Json spur=Json::array();for(const auto cell:watch_spur)spur.push_back({cell.x,cell.y});
        result["paid_watch_road_spur"]=std::move(spur);
    }
    if(paper_buffer(recipe)) {
        const auto b=w.original_auxiliary_balance(sim::OriginalAuxMaterial::Paper);
        const auto id=base::owner(w,planned(plan,137).cell);
        if(usable_paper(recipe)) {
            if(id) {
                result["paper_buffer"]=usable_paper_status(usable_paper_state(w,*id));
                result["paper_buffer"]["operating"]=w.building(*id).operating_enabled;
            }else result["paper_buffer"]={{"controller","downstream8/16-active-academy-floor4"},
                {"maker_not_yet_purchased",true},{"global_physical_total",b.stock+b.cargo},{"global_conserved",b.conserved}};
        }else result["paper_buffer"]={{"physical_stock",b.stock},{"actual_in_flight",b.cargo},
            {"physical_total",b.stock+b.cargo},{"pause_at_or_above",16},{"resume_at_or_below",8},
            {"operating",id?Json(w.building(*id).operating_enabled):Json(nullptr)},
            {"installed_upkeep_retained",true},{"authority","World::original_auxiliary_balance(Paper)"}};
    }
    if(active_religion(recipe)) {
        Json sources=Json::array();
        for(const auto key:{11U,13U,15U,20U,22U,24U}) {
            const auto id=base::owner(w,planned(plan,key).cell);
            sources.push_back({{"plan_id",key},{"purchased_owner",id?Json(static_cast<unsigned>(*id)):Json(nullptr)},
                {"operating_now",id?Json(w.building(*id).operating_enabled):Json(nullptr)},
                {"policy_pause",planned(plan,key).duplicate}});
        }
        result["existing_duplicate_service_policy"]={{"religion20_22_operating",true},
            {"culture11_13_15_operating",recipe=="combined-services"},{"academy24_remains_paused",true},{"actual_sources",sources}};
    }
    if(oe45_grid_watch_layout(recipe)) {
        Json sites=Json::array();for(const auto key:{131U,132U}) {
            const auto& node=planned(plan,key);
            sites.push_back({{"plan_id",key},{"cell",{node.cell.x,node.cell.y}},{"phase",node.phase}});
        }
        result["grid_watch_layout"]={{"watch_sites",std::move(sites)},
            {"both_watch_cells_excluded_from_basic_road_purchases",true},
            {"complete_future_footprints_reserved_from_grid_roads",true},
            {"source_admission","ordinary production World; no fallback site"},{"road_removed",false}};
    } else if(refined(recipe))result["door_watch"]={{"plan_id",132},{"cell",{planned(plan,132).cell.x,planned(plan,132).cell.y}},
        {"exact_unbought_basic_road_cell_omitted",true},{"road_removed",false}};
    if(timing_recipe(recipe)) {
        Json gates=Json::array();for(const auto& node:plan)if(const auto gate=purchase_timing(w,plan,node.plan_id,recipe))gates.push_back(*gate);
        result["saved_arrival_purchase_timing"]={{"gates",gates},{"all_timing_purchases_complete",timing_purchases_complete(w,recipe)},
            {"keep_testing_actual_purchase_gates_after_first17",true},{"missing_arrival_never_has_a_fallback",true},
            {"bought_nodes_do_not_reconstruct_overwritten_primary_witnesses",true},{"installed_upkeep_not_discounted",true}};
        if(stagger_medical(recipe))result["staggered_medical_priority"]={{"normal_High_for_active_own_category_pair",true},
            {"health_pair_active",active_pair(w,plan,7,8)},{"clinic_pair_active",active_pair(w,plan,9,10)}};
        if(recipe=="early-adaptive-watch")result["adaptive_watch"]=adaptive_watch_status(w,plan);
    }
    return result;
}
} // namespace oe_elite_stability_strategy
