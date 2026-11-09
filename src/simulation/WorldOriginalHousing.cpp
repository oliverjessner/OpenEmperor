#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
template<class Rows> const auto& by_id(const Rows& rows,BuildingId id) {
    const auto it=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key) {
        if constexpr (requires {row.building_id;}) return row.building_id<key;
        else return row.id<key;
    });
    if (it==rows.end()) invalid("Housing history refers to missing paid identity");
    if constexpr (requires {it->building_id;}) {if (it->building_id!=id) invalid("Housing history has orphan identity");}
    else {if (it->id!=id) invalid("Housing history has orphan body");}
    return *it;
}
constexpr std::array roles{original::ServiceRole::Herbalist,original::ServiceRole::Acupuncture,original::ServiceRole::Ancestor,
    original::ServiceRole::Confucian,original::ServiceRole::Daoist,original::ServiceRole::Buddhist,
    original::ServiceRole::Music,original::ServiceRole::Acrobat,original::ServiceRole::Drama};
constexpr std::array service_kinds{Object::HealthPost,Object::ServicePost,Object::OriginalAncestorShrine214,
    Object::OriginalConfucianAcademy219,Object::OriginalDaoistShrine215,Object::OriginalBuddhistShrine217,
    Object::OriginalMusicSchool211,Object::OriginalAcrobatSchool212,Object::OriginalDramaSchool213};
constexpr std::array service_actor_roles{CourierRole::HealthWorker,CourierRole::Service,CourierRole::OriginalAncestor,
    CourierRole::OriginalConfucian,CourierRole::OriginalDaoist,CourierRole::OriginalBuddhist,
    CourierRole::OriginalMusic,CourierRole::OriginalAcrobat,CourierRole::OriginalDrama};
original::HouseState evolved(const original::HouseEvolutionRules& rules,const original::HouseState& input) {
    return original::evolve_step(rules,input);
}
original::HouseType empty_type(Object kind) {
    return original_elite_house_kind(kind) ? original::HouseType::EliteUnoccupied:original::HouseType::Vacant;
}
original::HouseType admitted_type(Object kind) {
    return original_elite_house_kind(kind) ? original::HouseType::ModestSiheyuan:original::HouseType::Shelter;
}
std::size_t row_index(original::HouseType type) {return original::model_row(original::house_row(type)).value;}
bool occupied_type(Object kind,original::HouseType type) {
    return original_elite_house_kind(kind) ? type>=original::HouseType::EliteAbandoned && type<=original::HouseType::HeavenlyCompound:
        type>=original::HouseType::Shelter && type<=original::HouseType::LuxuriousApartment;
}
std::uint64_t evaluations(std::uint64_t through) {return through<40 ? 0:1+(through-40)/51;}
void add(std::uint64_t& total,std::uint64_t value) {
    if (value>UINT64_MAX-total) invalid("Housing historical counter overflow");total+=value;
}
std::uint64_t admission_opportunities(std::uint64_t first,std::uint64_t through) {
    return through<first ? 0:1+(through-first)/40;
}
void basic_input(const original::HouseState& input,bool cardinal_water) {
    if (!original::is_house_type(input.type) || input.type<original::HouseType::Shelter ||
        input.type==original::HouseType::EliteUnoccupied || input.occupants<=0 || input.devolve_counter>original::devolve_limit(input.type) ||
        input.desirability<-100 || input.desirability>(cardinal_water ? 110:100) || input.food_quality>80)
        invalid("unsupported occupied Housing evaluation input");
    for (std::size_t g=0;g<original::goods_count;++g) {
        const auto stock=original::goods_stock(input,static_cast<original::Goods>(g));
        if (stock<0 || stock>Rules::household_capacity) invalid("Housing input invents unsupported goods stock");
    }
}
void historical_service(const WorldSnapshot& s,BuildingId id,std::uint64_t first,const OriginalHousingEvaluation& proof,std::size_t index) {
    const auto role=roles[index];const auto& value=proof.service_witnesses[index];std::uint8_t expected=0;
    if (value) {
        const auto& w=*value;const auto& source=by_id(s.buildings,w.provider_id);const auto& target=by_id(s.buildings,id);
        const auto actor=std::find_if(s.couriers.begin(),s.couriers.end(),[&](const auto& c){return c.id==w.transfer.courier_id;});
        const auto trip=std::find_if(s.original_production->services.trips.begin(),s.original_production->services.trips.end(),
            [&](const auto& row){return row.transfer.courier_id==w.transfer.courier_id;});
        const bool venue=simulation::original_venue_house_service_enabled(s.profile,s.rule_version) && role==original::ServiceRole::Music;
        const auto kind=venue ? Object::OriginalTheatre75:service_kinds[index];
        const auto actor_role=venue ? CourierRole::OriginalVenueMusic:service_actor_roles[index];
        if(w.venue_walker.has_value()!=venue) invalid("Housing Music history requires its exact generation's real provider evidence");
        const auto from=building_footprint(s.profile,s.rule_version,source.kind),to=building_footprint(s.profile,s.rule_version,target.kind);
        const auto gap=[](int a,int a_size,int b,int b_size) {
            const auto first=static_cast<std::int64_t>(a),second=static_cast<std::int64_t>(b);
            return std::max({std::int64_t{0},first-(second+b_size-1),second-(first+a_size-1)});
        };
        const auto travel=static_cast<std::uint64_t>(std::max<std::int64_t>(2,
            gap(source.cell.x,from.width,target.cell.x,to.width)+gap(source.cell.y,from.height,target.cell.y,to.height)))*
            static_cast<std::uint64_t>(profile_rules(s.profile,s.rule_version).courier_edge_ticks);
        if (w.building_id!=id || w.role!=role || source.kind!=kind || actor==s.couriers.end() || actor->owner!=w.provider_id ||
            actor->role!=actor_role || trip==s.original_production->services.trips.end() || !w.transfer.sequence ||
            w.transfer.sequence>trip->transfer.sequence || w.transfer.sequence>w.arrival_tick || w.arrival_tick>proof.tick ||
            w.arrival_tick<=source.placed_tick || w.arrival_tick-source.placed_tick<travel ||
            w.arrival_tick<first || w.arrival_tick-first<travel)
            invalid("Housing service input lacks a possible actual paid visit");
        if (trip->transfer==w.transfer) {
            if (!trip->visit_applied || trip->target!=id || trip->arrival_tick!=w.arrival_tick)
                invalid("Housing historical service contradicts retained arrival");
        } else if (w.arrival_tick>=trip->dispatched_tick) invalid("Housing service witness postdates later trip");
        const auto current=std::find_if(s.original_production->services.witnesses.begin(),s.original_production->services.witnesses.end(),
            [&](const auto& row){return row.building_id==id && row.role==role;});
        if (current==s.original_production->services.witnesses.end() || current->arrival_tick<w.arrival_tick ||
            (current->arrival_tick==w.arrival_tick && *current!=w)) invalid("Housing discarded its latest historical service origin");
        const auto n=original_water_countdowns(proof.tick)-original_water_countdowns(w.arrival_tick-1);
        const auto duration=original::service_duration(role);expected=n>=duration ? 0:static_cast<std::uint8_t>(duration-n);
    }
    const auto require_known=[&](const OriginalServiceWitness& known) {
        if (known.arrival_tick>proof.tick) return;
        const auto key=[](const auto& w){return std::pair{w.arrival_tick,w.transfer.courier_id};};
        if (!value || key(*value)<key(known) || (key(*value)==key(known) && *value!=known))
            invalid("Housing service input discarded an arrival already known at evaluation");
    };
    for (const auto& known:s.original_production->services.witnesses)
        if (known.building_id==id && known.role==role) require_known(known);
    for (const auto& trip:s.original_production->services.trips) if (trip.visit_applied && trip.target==id && trip.arrival_tick) {
        const auto actor=std::find_if(s.couriers.begin(),s.couriers.end(),[&](const auto& c){return c.id==trip.transfer.courier_id;});
        const auto actor_role=simulation::original_venue_house_service_enabled(s.profile,s.rule_version) && role==original::ServiceRole::Music ?
            CourierRole::OriginalVenueMusic:service_actor_roles[index];
        if (actor!=s.couriers.end() && actor->role==actor_role) require_known({id,role,actor->owner,trip.transfer,*trip.arrival_tick,trip.venue_walker});
    }
    if (original::service_timer(proof.input,role)!=expected) invalid("Housing input service timer differs from actual visit phase");
}
void historical_food(const WorldSnapshot& s,BuildingId id,std::uint64_t first,const OriginalHousingEvaluation& proof) {
    const auto& current=by_id(s.original_production->food.stocks,id);
    std::uint64_t received=0,consumed=0;
    for (std::size_t g=0;g<original_food_crop_count;++g) {
        if (proof.food_received[g]>current.received[g] || proof.food_consumed[g]>current.consumed[g] || proof.food_consumed[g]>proof.food_received[g])
            invalid("Housing Food input exceeds actual physical reception/sinks");
        add(received,proof.food_received[g]);add(consumed,proof.food_consumed[g]);
    }
    if (received-consumed>8 || consumed>original_housing_consumption_events(proof.tick-1)-original_housing_consumption_events(first-1))
        invalid("Housing Food input exceeds House capacity or occupied clock");
    if (current.quality_witness && current.quality_witness->tick<=proof.tick && (proof.food_quality_witness!=current.quality_witness || proof.food_received!=current.received))
        invalid("Housing Food input discarded a known last quality arrival");
    if (!proof.food_quality_witness) {
        if (received || consumed || proof.input.food_quality) invalid("Housing Food quality has no actual delivery witness");return;
    }
    const auto& w=*proof.food_quality_witness;
    const auto actor=std::find_if(s.couriers.begin(),s.couriers.end(),[&](const auto& c){return c.id==w.transfer.courier_id;});
    const auto trip=std::find_if(s.original_production->food.trips.begin(),s.original_production->food.trips.end(),
        [&](const auto& row){return row.transfer.courier_id==w.transfer.courier_id;});
    if (actor==s.couriers.end() || actor->role!=CourierRole::OriginalFoodToHouse || trip==s.original_production->food.trips.end() ||
        !w.transfer.sequence || w.transfer.sequence>trip->transfer.sequence || w.transfer.sequence>w.tick || !w.tick ||
        w.tick<first || w.tick>proof.tick || !current.quality_witness || current.quality_witness->tick<w.tick ||
        (current.quality_witness->tick==w.tick && *current.quality_witness!=w))
        invalid("Housing Food quality witness has no possible paid Distributor arrival");
    const auto& market=by_id(s.buildings,actor->owner);const auto& target=by_id(s.buildings,id);
    const auto gap=[](int a,int a_size,int b,int b_size) {
        const std::int64_t first=a,second=b;
        return std::max({std::int64_t{0},first-(second+b_size-1),second-(first+a_size-1)});
    };
    // Market is currently1x1; a conservative2x2 source lower bound remains
    // necessary without pretending to retain the complete road history.
    const auto target_size=building_footprint(s.profile,s.rule_version,target.kind);
    const auto travel=static_cast<std::uint64_t>(std::max<std::int64_t>(2,
        gap(market.cell.x,2,target.cell.x,target_size.width)+gap(market.cell.y,2,target.cell.y,target_size.height)))*
        static_cast<std::uint64_t>(profile_rules(s.profile,s.rule_version).courier_edge_ticks);
    if (market.kind!=Object::Market || w.tick<=market.placed_tick || w.tick-market.placed_tick<travel || w.tick-first<travel)
        invalid("Housing Food witness predates its actual paid Market/occupied target travel");
    if (trip->transfer==w.transfer && (trip->phase!=OriginalGoodsTransferPhase::Completed || trip->target!=id || trip->delivered!=w.accepted))
        invalid("Housing Food witness contradicts retained delivery");
    std::int32_t previous=0,accepted=0;std::uint64_t used_since=0;
    for (std::size_t g=0;g<original_food_crop_count;++g) {
        if (w.accepted[g]<0 || w.accepted[g]>4 || w.consumed_before[g]>proof.food_consumed[g] ||
            proof.food_received[g]<static_cast<std::uint64_t>(w.accepted[g]) ||
            proof.food_received[g]-static_cast<std::uint64_t>(w.accepted[g])<w.consumed_before[g])
            invalid("Housing Food witness invents portions");
        const auto prior=proof.food_received[g]-static_cast<std::uint64_t>(w.accepted[g])-w.consumed_before[g];
        const auto delta=proof.food_consumed[g]-w.consumed_before[g];
        if (prior>8 || delta>prior+static_cast<std::uint64_t>(w.accepted[g])) invalid("Housing Food witness exceeds historical stock");
        previous+=static_cast<std::int32_t>(prior);accepted+=w.accepted[g];add(used_since,delta);
    }
    if (accepted<=0 || accepted>4 || previous+accepted>8 || (!previous && w.previous_quality) ||
        (previous && (w.previous_quality<20 || w.previous_quality>80)) ||
        (previous && ((proof.food_received[1]==static_cast<std::uint64_t>(w.accepted[1]) && w.previous_quality!=20) ||
         (proof.food_received[0]==static_cast<std::uint64_t>(w.accepted[0]) && w.previous_quality!=80))) ||
        used_since>original_housing_consumption_events(proof.tick-1)-original_housing_consumption_events(w.tick-1))
        invalid("Housing Food witness violates real crop quality/capacity/clock");
    const auto expected=original::blend_food_quality(w.previous_quality,original_food_batch_quality(w.accepted).value,accepted,previous);
    if (proof.input.food_quality!=(received>consumed ? expected:0)) invalid("Housing Food input differs from witnessed blend/empty reset");
}
}

void World::validate_original_housing_snapshot(const WorldSnapshot& s) {
    const auto& state=s.original_production->housing;const auto& homes=s.original_economy->houses;
    if (state.houses.size()!=homes.size() || s.original_production->house_consumption.size()!=homes.size() || state.houses.size()>20 || state.neutral_source_origins.size()>37 ||
        state.neutral_source_origins.size()!=s.original_influence->sources.size()+s.original_influence->removed_sources.size())
        invalid("Housing authority requires exact bounded paid House/source histories");
    for (std::size_t i=0;i<state.neutral_source_origins.size();++i) {
        const auto& origin=state.neutral_source_origins[i];
        if ((i && !(state.neutral_source_origins[i-1].building_id<origin.building_id)) ||
            origin.origin.x<0 || origin.origin.y<0 || origin.origin.x>s.width-2 || origin.origin.y>s.height-2)
            invalid("Housing neutral source origins must be unique sorted on-map footprints");
        const auto live=std::find_if(s.original_influence->sources.begin(),s.original_influence->sources.end(),[&](const auto& p){return p.building_id==origin.building_id;});
        if (live!=s.original_influence->sources.end()) {
            if (by_id(s.buildings,origin.building_id).cell!=origin.origin) invalid("Housing source origin differs from actual paid body");
        } else if (std::none_of(s.original_influence->removed_sources.begin(),s.original_influence->removed_sources.end(),
            [&](const auto& row){return row.building_id==origin.building_id;})) invalid("Housing origin has no ever-paid neutral source");
    }
    for (std::size_t i=0;i<homes.size();++i) {
        const auto& h=homes[i];const auto& row=state.houses[i];const auto& b=by_id(s.buildings,h.building_id);
        if (row.building_id!=h.building_id || !original_house_kind(b.kind)) invalid("Housing rows must match exact paid House order/family");
        if ((b.kind==Object::OriginalElitePlot && row.first_admission_tick) ||
            (b.kind==Object::OriginalEliteHouse && !row.first_admission_tick))
            invalid("Elite materialization requires exactly its real first admission");
        original::HouseState empty;empty.type=empty_type(b.kind);
        if (!row.first_admission_tick) {
            if (h.house!=empty || h.original_status || h.access_enabled || h.cycle_phase || row.last_evaluation ||
                row.first_transitions!=decltype(row.first_transitions){} || row.consumption_rows!=decltype(row.consumption_rows){})
                invalid("Empty paid House has no admission but owns Housing authority");
            continue;
        }
        const auto first=*row.first_admission_tick;
        if (first>s.ticks || first<b.placed_tick || first-b.placed_tick<120 || (first-b.placed_tick-120)%40 ||
            !occupied_type(b.kind,h.house.type) || h.house.occupants<=0 || h.original_status!=3 || !h.access_enabled || h.cycle_phase)
            invalid("Housing admission must preserve its paid family and real120+40k opportunity");
        basic_input(h.house,simulation::original_cardinal_terrain_enabled(s.profile,s.rule_version));
        const auto initial_index=row_index(admitted_type(b.kind));const auto& initial=row.first_transitions[initial_index];
        OriginalHousingEvaluation expected_admission;expected_admission.tick=first;expected_admission.input=empty;
        if (!initial || initial->kind!=OriginalHousingTransitionKind::Admission || initial->evaluation!=expected_admission ||
            initial->admitted<=0 || initial->admitted>2 || initial->admitted>h.house.occupants)
            invalid("Housing first admission lacks the exact empty paid origin and actual residents");
        const auto latest=original_housing_evaluation_tick(s.ticks);
        if (static_cast<bool>(row.last_evaluation)!=(latest>=first)) invalid("Housing last evaluation is missing or prematurely injected");
        std::uint64_t events=0;std::array<std::uint64_t,original::goods_count> consumed{};
        for (std::size_t r=0;r<row.consumption_rows.size();++r) {
            const auto& c=row.consumption_rows[r];add(events,c.events);
            const auto& entered=row.first_transitions[r];
            const auto opportunities=entered ? original_housing_consumption_events(s.ticks)-original_housing_consumption_events(entered->evaluation.tick-1):0;
            if (c.events>opportunities) invalid("Housing consumption row predates its first actual entry");
            for (std::size_t g=0;g<original::goods_count;++g) {
                if (c.consumed[g]>c.events*std::uint64_t{8}) invalid("Housing consumption row exceeds physical stock/events");
                add(consumed[g],c.consumed[g]);
            }
        }
        if (events!=original_housing_consumption_events(s.ticks)-original_housing_consumption_events(first-1) ||
            consumed!=s.original_production->house_consumption[i].consumed) invalid("Housing row sinks/events disagree with occupied clock/ledger");
        std::vector<const OriginalHousingEvaluation*> proofs;
        if (row.last_evaluation) proofs.push_back(&*row.last_evaluation);
        std::uint64_t first_change=s.ticks+1;
        for (std::size_t r=0;r<row.first_transitions.size();++r) if (const auto& transition=row.first_transitions[r]) {
            const auto target=static_cast<original::HouseType>(r+3);const auto& e=transition->evaluation;
            if (!occupied_type(b.kind,target) || e.tick<first || e.tick>s.ticks) invalid("Housing transition crosses family or paid lifetime");
            if (transition->kind==OriginalHousingTransitionKind::Admission) {
                if (r!=initial_index || *transition!=*initial) invalid("Housing owns a second or misplaced admission event");
                continue;
            }
            if (transition->kind!=OriginalHousingTransitionKind::Evolution || transition->admitted || target==e.input.type ||
                !occupied_type(b.kind,e.input.type) || original_housing_evaluation_tick(e.tick)!=e.tick)
                invalid("Housing transition must be a changed pure phase39 decision");
            for (std::size_t earlier=0;earlier<r;++earlier) if (row.first_transitions[earlier] &&
                row.first_transitions[earlier]->kind==OriginalHousingTransitionKind::Evolution &&
                row.first_transitions[earlier]->evaluation.tick==e.tick) invalid("One House evaluated into two different types in one tick");
            if (row.last_evaluation && row.last_evaluation->tick==e.tick && *row.last_evaluation!=e)
                invalid("same House phase39 has conflicting first/latest inputs");
            first_change=std::min(first_change,e.tick);proofs.push_back(&e);
        }
        const auto initial_until=first_change<=s.ticks ? first_change-1:s.ticks;
        if (row.consumption_rows[initial_index].events<original_housing_consumption_events(initial_until)-original_housing_consumption_events(first-1))
            invalid("Housing consumption omitted mandatory initial-type events");
        for (const auto* proof:proofs) {
            const auto& e=*proof;basic_input(e.input,simulation::original_cardinal_terrain_enabled(s.profile,s.rule_version));
            if (!occupied_type(b.kind,e.input.type) || e.tick<first || e.tick>s.ticks ||
                original_housing_evaluation_tick(e.tick)!=e.tick || e.input.occupants>h.house.occupants)
                invalid("Housing evaluation has impossible paid family/occupied phase");
            const auto& source=row.first_transitions[row_index(e.input.type)];
            if (!source || source->evaluation.tick>e.tick ||
                (source->evaluation.tick==e.tick && source->kind!=OriginalHousingTransitionKind::Admission))
                invalid("Housing input predates its own first actual type entry");
            const auto after=source->evaluation.tick-(source->kind==OriginalHousingTransitionKind::Admission ? 1:0);
            if (e.input.devolve_counter>evaluations(e.tick-1)-evaluations(after))
                invalid("Housing input counter exceeds earlier actual evaluations after entry");
            for (std::size_t g=0;g<original::goods_count;++g) {
                const auto before=e.goods_consumed_before[g],total=consumed[g];
                const auto input=static_cast<std::uint64_t>(original::goods_stock(e.input,static_cast<original::Goods>(g)));
                const auto current=static_cast<std::uint64_t>(original::goods_stock(h.house,static_cast<original::Goods>(g)));
                if (before>total || before>UINT64_MAX-input || total>UINT64_MAX-current || input+before>current+total)
                    invalid("Housing evaluation invents goods without actual House delivery");
            }
            for (std::size_t r=0;r<roles.size();++r) historical_service(s,h.building_id,first,e,r);
            validate_original_water_evaluation_provenance(s,h.building_id,e.tick,e.water_evaluation_witness,e.input.water);
            historical_food(s,h.building_id,first,e);
        }
        if (row.last_evaluation && row.last_evaluation->tick!=latest) invalid("Housing last evaluation skipped an occupied phase39");
        if (!row.first_transitions[row_index(h.house.type)]) invalid("current Housing type has no permanent first entry");
    }
}

void World::validate_original_housing_model() {
    const auto& rules=original_rules_model_->house_rules();
    original::HouseState minimum_abandoned;
    minimum_abandoned.type=original::HouseType::EliteAbandoned;
    minimum_abandoned.occupants=1;
    const bool abandoned_needs_always_met=original::needs_met(rules,original::ModelHouse::EliteAbandoned,minimum_abandoned);
    for (std::size_t i=0;i<original::house_row_count;++i) {
        const auto cap=rules.row_capacity(static_cast<original::ModelHouse>(i));
        if (cap<0 || cap>INT16_MAX || (i!=original::model_row(original::ModelHouse::EliteUnoccupied).value && !cap))
            invalid("Housing requires bounded capacities with only the unoccupied model row allowed zero");
    }
    for (const auto& source:original_production_->housing.neutral_source_origins)
        for (int y=0;y<2;++y) for (int x=0;x<2;++x)
            if (!permissions_->building_allowed({source.origin.x+x,source.origin.y+y})) invalid("historical neutral source origin violates immutable permissions");
    std::map<std::uint64_t,original::DesirabilityMap> maps;
    const auto desirability=[&](std::uint64_t tick,BuildingId recipient) {
        const auto& body=building(recipient);
        const auto cell=body.cell;
        const auto side=building_footprint(profile_,rule_version_,body.kind).width;
        auto found=maps.find(tick);
        if (found==maps.end()) {
            struct Source {BuildingId id;Cell origin;BuildingFootprint footprint;original::ModelBuilding model;};
            std::vector<Source> sources;
            for (const auto& origin:original_production_->housing.neutral_source_origins) {
                const auto live=std::find_if(buildings_.begin(),buildings_.end(),[&](const auto& b){return b.id==origin.building_id;});
                bool present=live!=buildings_.end() ? live->placed_tick<tick:false;
                if (live==buildings_.end()) {const auto& old=by_id(original_influence_->removed_sources,origin.building_id);present=old.placed_tick<tick && old.removed_tick>=tick;}
                if (present) sources.push_back({origin.building_id,origin.origin,{2,2},original::ModelBuilding::Well});
            }
            for (const auto& b:buildings_) if (b.kind!=Object::OriginalInfluenceSource && b.placed_tick<tick)
                if (const auto model=original_influence_model(b.kind)) sources.push_back({b.id,b.cell,building_footprint(profile_,rule_version_,b.kind),*model});
            std::sort(sources.begin(),sources.end(),[](const auto& a,const auto& b){return a.id<b.id;});
            original::DesirabilityMap map(width_,height_);
            for (const auto& source:sources) {
                const original::SpreadSpec spread{
                    original_rules_model_->building_value(source.model,original::BuildingField::Desirability),
                    original_rules_model_->building_value(source.model,original::BuildingField::DesirabilityStep),
                    original_rules_model_->building_value(source.model,original::BuildingField::DesirabilityStepSize),
                    original_rules_model_->building_value(source.model,original::BuildingField::DesirabilityRange)};
                map.apply_source({{source.origin.x,source.origin.y,source.footprint.width},spread});
            }
            found=maps.emplace(tick,std::move(map)).first;
        }
        return found->second.building_value({cell.x,cell.y,side},original_house_cardinal_water_bonus(recipient));
    };
    for (std::size_t i=0;i<original_economy_->houses.size();++i) {
        const auto& home=original_economy_->houses[i];const auto& row=original_production_->housing.houses[i];
        if (!row.first_admission_tick) continue;
        const auto first=*row.first_admission_tick;const auto& b=building(home.building_id);
        const auto& admission=*row.first_transitions[row_index(admitted_type(b.kind))];
        const auto initial_cap=rules.row_capacity(original::admission_capacity_row(empty_type(b.kind)));
        if (admission.admitted!=std::min(2,initial_cap)) invalid("Housing initial residents differ from the pre-event admission selector");
        std::vector<std::pair<original::HouseType,const OriginalHousingTransition*>> entries;
        for (std::size_t r=0;r<row.first_transitions.size();++r) if (const auto& entry=row.first_transitions[r])
            entries.emplace_back(static_cast<original::HouseType>(r+3),&*entry);
        std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second->evaluation.tick<b.second->evaluation.tick;});
        const auto maximum_population=[&](std::uint64_t through) {
            std::uint64_t population=static_cast<std::uint64_t>(admission.admitted),previous=first+1;
            auto cap=rules.row_capacity(original::admission_capacity_row(admitted_type(b.kind)));
            const auto accumulate=[&](std::uint64_t until) {
                if (until<previous) return;
                const auto count=admission_opportunities(first,until)-admission_opportunities(first,previous-1);
                if (population<static_cast<std::uint64_t>(cap)) population=std::min<std::uint64_t>(static_cast<std::uint64_t>(cap),population+2*count);
                previous=until+1;
            };
            for (const auto& [type,entry]:entries) if (entry->kind==OriginalHousingTransitionKind::Evolution && entry->evaluation.tick<through) {
                accumulate(entry->evaluation.tick);
                cap=std::max(cap,rules.row_capacity(original::admission_capacity_row(type)));
            }
            accumulate(through);return population;
        };
        const auto parity_reachable=[&](int population,std::uint64_t through) {
            if (population%2==0) return true;
            if (admission.admitted%2 && admission.admitted<=population) return true;
            for (const auto& [type,entry]:entries) {
                const auto cap=rules.row_capacity(original::admission_capacity_row(type));
                if (entry->evaluation.tick<through && cap%2 && cap>entry->evaluation.input.occupants && cap<=population) return true;
            }
            return false;
        };
        const auto check_population=[&](int population,std::uint64_t through) {
            if (!parity_reachable(population,through) || static_cast<std::uint64_t>(population)>maximum_population(through))
                invalid("Housing residents exceed earlier paid admissions/type capacities/odd saturation");
        };
        check_population(home.house.occupants,ticks_);
        for (std::size_t r=0;r<row.consumption_rows.size();++r) for (std::size_t g=0;g<original::goods_count;++g) {
            const auto need=std::clamp(rules.value(static_cast<original::ModelHouse>(r),original::goods_field(static_cast<original::Goods>(g))),0,8);
            if (row.consumption_rows[r].consumed[g]>row.consumption_rows[r].events*static_cast<std::uint64_t>(need))
                invalid("Housing row consumption exceeds its actual model need");
        }
        const auto check_evaluation=[&](const OriginalHousingEvaluation& e) {
            check_population(e.input.occupants,e.tick);
            if (abandoned_needs_always_met && e.input.type==original::HouseType::EliteAbandoned && e.input.devolve_counter)
                invalid("Housing Abandoned counter cannot accumulate when its model needs are always met");
            if (e.input.desirability!=desirability(e.tick,b.id))
                invalid("Housing historical desire differs from actual paid sources at phase39");
        };
        if (row.last_evaluation) check_evaluation(*row.last_evaluation);
        for (std::size_t r=0;r<row.first_transitions.size();++r) if (const auto& entry=row.first_transitions[r])
            if (entry->kind==OriginalHousingTransitionKind::Evolution) {
                check_evaluation(entry->evaluation);
                if (evolved(rules,entry->evaluation.input).type!=static_cast<original::HouseType>(r+3))
                    invalid("Housing first transition fails unchanged pure evolution");
            }
    }
    validate_original_housing_state();
}

void World::validate_original_housing_state() const {
    for (std::size_t i=0;i<original_economy_->houses.size();++i) {
        const auto& home=original_economy_->houses[i];const auto& row=original_production_->housing.houses[i];
        const auto kind=building(home.building_id).kind;
        if ((kind==Object::OriginalElitePlot && row.first_admission_tick) ||
            (kind==Object::OriginalEliteHouse && !row.first_admission_tick))
            invalid("live Elite geometry contradicts its first admission");
        if (row.last_evaluation) {
            const auto expected=evolved(original_rules_model_->house_rules(),row.last_evaluation->input);
            if (home.house.type!=expected.type || home.house.devolve_counter!=expected.devolve_counter || home.house.desirability!=expected.desirability)
                invalid("current Housing type/counter differs from last pure evaluation");
        } else if (row.first_admission_tick && (home.house.type!=admitted_type(building(home.building_id).kind) || home.house.devolve_counter || home.house.desirability))
            invalid("newly occupied House changed before its first phase39");
    }
}

void World::evaluate_original_houses() {
    for (std::size_t i=0;i<original_economy_->houses.size();++i) {
        auto& home=original_economy_->houses[i];if (!home.house.occupants) continue;
        auto& row=original_production_->housing.houses[i];OriginalHousingEvaluation proof;
        home.house.desirability=static_cast<std::int8_t>(original_house_influence_value(home.building_id));
        proof.tick=ticks_;proof.input=home.house;
        for (std::size_t r=0;r<roles.size();++r) {
            const auto w=std::find_if(original_production_->services.witnesses.begin(),original_production_->services.witnesses.end(),
                [&](const auto& witness){return witness.building_id==home.building_id && witness.role==roles[r];});
            if (w!=original_production_->services.witnesses.end()) proof.service_witnesses[r]=*w;
        }
        proof.water_evaluation_witness=original_production_->water.houses[i].evaluation_witness;
        const auto& food=by_id(original_production_->food.stocks,home.building_id);
        proof.food_quality_witness=food.quality_witness;proof.food_received=food.received;proof.food_consumed=food.consumed;
        proof.goods_consumed_before=original_production_->house_consumption[i].consumed;
        home.house=evolved(original_rules_model_->house_rules(),home.house);
        row.last_evaluation=proof;
        if (home.house.type!=proof.input.type) {
            auto& first=row.first_transitions[row_index(home.house.type)];
            if (!first) first=OriginalHousingTransition{OriginalHousingTransitionKind::Evolution,proof,0};
        }
    }
}

} // namespace openemperor::simulation
