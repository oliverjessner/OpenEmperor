#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {

struct OriginalInfluenceCache {
    struct Source {
        OriginalInfluenceSourceState identity;
        Cell origin;
        BuildingFootprint footprint;
        original::SpreadSpec spread;
    };
    std::vector<Source> sources;
    // Prefix0 is the zero map; each following prefix comes from the unchanged
    // numerical core. Inspector values therefore include actual per-add clamps.
    std::vector<original::DesirabilityMap> prefixes;
};

std::optional<original::ModelBuilding> original_influence_model(Object kind) {
    using B=original::ModelBuilding;
    switch (kind) {
    case Object::OriginalInfluenceSource:
    case Object::OriginalWaterWell72:return B::Well;
    case Object::HealthPost:return B::HerbalistsStall;
    case Object::ServicePost:return B::AcupuncturistsClinic;
    case Object::OriginalMusicSchool211:return B::MusicSchool;
    case Object::OriginalAcrobatSchool212:return B::AcrobatSchool;
    case Object::OriginalDramaSchool213:return B::DramaSchool;
    case Object::OriginalAncestorShrine214:return B::AncestralShrine;
    case Object::OriginalDaoistShrine215:return B::DaoistShrine;
    case Object::OriginalBuddhistShrine217:return B::BuddhistShrine;
    case Object::OriginalConfucianAcademy219:return B::ConfucianAcademy;
    default:return std::nullopt;
    }
}

namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
bool source_type_valid(OriginalInfluenceSourceType type) {
    return type==OriginalInfluenceSourceType::Well72;
}
int contribution_at(const OriginalInfluenceCache::Source& source,Cell cell) {
    const auto dx=std::max({source.origin.x-cell.x,0,cell.x-(source.origin.x+source.footprint.width-1)});
    const auto dy=std::max({source.origin.y-cell.y,0,cell.y-(source.origin.y+source.footprint.height-1)});
    const auto ring=std::max(dx,dy);
    if (!ring || ring>std::clamp(source.spread.range,0,original::desirability_max_range) ||
        source.spread.value==0) return 0;
    // Explanation only: authoritative before/after values are read from the
    // actual core's prepared prefix maps. At most ten small steps are needed.
    std::int64_t value=source.spread.value;
    const auto period=std::max(source.spread.step,std::int32_t{1});
    for (int r=1;r<ring;++r) if (r%period==0 && value!=0) {
        value+=source.spread.step_size;
        value=source.spread.value<0 ? std::min(value,std::int64_t{0}):
            std::max(value,std::int64_t{0});
    }
    return static_cast<int>(value);
}
}

World World::create_original_influence_development(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original influence development requires complete policy-3 permissions and a prepared model");
    const auto width=permissions->width(),height=permissions->height();
    auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,
        kOriginalInfluenceRuleVersion,std::move(permissions),true,std::move(model));
    world.rebuild_original_influence_cache();
    return world;
}

World World::restore_original_influence_development(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (snapshot.profile!=RulesProfile::CityV16 || snapshot.rule_version!=kOriginalInfluenceRuleVersion ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original influence restore requires explicit rule 8, policy 3 and prepared model");
    validate_original_influence_snapshot(snapshot);
    return restore_impl(snapshot,{},std::move(permissions),true,std::move(model));
}

void World::validate_original_influence_snapshot(const WorldSnapshot& s) {
    validate_original_economy_snapshot(s);
    if (simulation::original_production_enabled(s.profile,s.rule_version)) return;
    if (!simulation::original_influence_enabled(s.profile,s.rule_version)) {
        if (s.original_influence) invalid("influence authority requires rule 8");
        return;
    }
    if (!s.original_influence || !s.original_economy || !s.original_economy->model_source ||
        s.width<1 || s.height<1 || s.width>228 || s.height>228)
        invalid("invalid bounded original influence authority");
    const auto& state=*s.original_influence;
    const auto houses=s.original_economy->houses.size();
    const auto live=state.sources.size(),removed=state.removed_sources.size();
    if (live>original_influence_source_limit || removed>original_influence_purchase_limit ||
        houses+live+removed>original_influence_purchase_limit ||
        s.buildings.size()!=houses+live || s.next_building_id!=houses+live+removed+1)
        invalid("invalid original influence purchase counts or next ID");
    std::array<bool,original_influence_purchase_limit+1> seen{};
    const auto mark=[&](BuildingId id) {
        const auto value=static_cast<std::uint32_t>(id);
        if (!value || value>=s.next_building_id || value>=seen.size() || seen[value])
            invalid("original influence IDs must be disjoint and contiguous");
        seen[value]=true;
    };
    for (const auto& house:s.original_economy->houses) mark(house.building_id);
    for (std::size_t i=0;i<live;++i) {
        const auto& source=state.sources[i];
        if (!source_type_valid(source.type) || (i && !(state.sources[i-1].building_id<source.building_id)))
            invalid("invalid original influence type or source order");
        mark(source.building_id);
        const auto b=std::lower_bound(s.buildings.begin(),s.buildings.end(),source.building_id,
            [](const BuildingSnapshot& value,BuildingId id) { return value.id<id; });
        if (b==s.buildings.end() || b->id!=source.building_id) invalid("orphan original influence source");
        BuildingSnapshot neutral{source.building_id,Object::OriginalInfluenceSource,b->cell,true};
        neutral.placed_tick=b->placed_tick;
        if (*b!=neutral || b->placed_tick>s.ticks)
            invalid("original influence container must have neutral fields and valid placement time");
    }
    for (std::size_t i=0;i<removed;++i) {
        const auto& entry=state.removed_sources[i];
        if (!source_type_valid(entry.type) || (i && !(state.removed_sources[i-1].building_id<entry.building_id)) ||
            entry.placed_tick>entry.removed_tick || entry.removed_tick>s.ticks)
            invalid("invalid original influence removal history");
        mark(entry.building_id);
    }
    for (std::uint32_t id=1;id<s.next_building_id;++id)
        if (!seen[id]) invalid("missing original influence purchase identity");
    for (const auto& b:s.buildings) if (b.kind==Object::OriginalInfluenceSource) {
        const auto r=std::lower_bound(state.sources.begin(),state.sources.end(),b.id,
            [](const OriginalInfluenceSourceState& value,BuildingId id) { return value.building_id<id; });
        if (r==state.sources.end() || r->building_id!=b.id) invalid("unbound influence container");
    }
    std::uint64_t spent=0;
    const auto add=[&](std::uint64_t count,std::uint64_t cost) {
        if (!cost || count>std::uint64_t(original_influence_starting_treasury)/cost ||
            spent>std::uint64_t(original_influence_starting_treasury)-count*cost)
            invalid("original influence spending exceeds fixed funds");
        spent+=count*cost;
    };
    add(s.roads_placed_total,std::uint64_t(Rules::road_cost));
    add(houses,std::uint64_t(Rules::household_cost));
    add(live+removed,std::uint64_t(original_influence_source_cost));
    add(s.ramps.size(),std::uint64_t(ramp_cost));
    for (const auto& bridge:s.bridges) {
        if (bridge.paid_cost<=0) invalid("invalid paid bridge cost");
        add(1,static_cast<std::uint64_t>(bridge.paid_cost));
    }
    if (s.construction_spent_total!=spent ||
        s.treasury!=original_influence_starting_treasury-static_cast<std::int64_t>(spent) ||
        s.roads_removed_total>s.roads_placed_total ||
        s.road_revision!=s.roads_placed_total+s.roads_removed_total+s.buildings.size()+
            2*removed+s.bridges.size()+s.ramps.size())
        invalid("original influence accounting or placement revision mismatch");
}

bool World::original_influence_finance_valid() const {
    if (original_production_enabled()) return original_production_finance_valid();
    if (!original_influence_ || !original_economy_ || !original_rules_model_ ||
        demolition_history_!=DemolitionHistory{} || taxes_collected_total_ || maintenance_spent_total_)
        return false;
    std::uint64_t spent=0;
    const auto add=[&](std::uint64_t count,std::uint64_t cost) {
        const auto budget=static_cast<std::uint64_t>(original_influence_starting_treasury);
        if (!cost || count>budget/cost || spent>budget-count*cost) return false;
        spent+=count*cost; return true;
    };
    if (!add(roads_placed_total_,std::uint64_t(Rules::road_cost)) ||
        !add(original_economy_->houses.size(),std::uint64_t(Rules::household_cost)) ||
        !add(original_influence_->sources.size()+original_influence_->removed_sources.size(),
            std::uint64_t(original_influence_source_cost)) ||
        !add(ramps_.size(),std::uint64_t(ramp_cost))) return false;
    for (const auto& bridge:bridges_)
        if (bridge.paid_cost<=0 || !add(1,static_cast<std::uint64_t>(bridge.paid_cost))) return false;
    return construction_spent_total_==spent &&
        treasury_==original_influence_starting_treasury-static_cast<std::int64_t>(spent);
}

CommandResult World::validate_original_influence_command(Command command) const {
    CommandResult result{false,false,"Original influence command requires rule 8",command_sequence_,ticks_};
    if (!original_influence_enabled() || !original_influence_ || !original_rules_model_) return result;
    const auto reject=[&](const char* reason) { result.reason=reason; return result; };
    if (command_sequence_>=UINT64_MAX-1 || road_revision_==UINT64_MAX ||
        original_influence_cache_build_count_==UINT64_MAX) return reject("Original influence counter exhausted");
    result.sequence=command_sequence_+1;
    if (command.type==CommandType::PlaceOriginalInfluenceSource) {
        if (!source_type_valid(command.source_type)) return reject("Unsupported original influence source type");
        if (original_influence_->sources.size()>=original_influence_source_limit)
            return reject("Original influence source limit reached");
        const auto retired=original_safe_demolition_enabled() && original_production_ &&
            original_production_->safe_demolition ? original_production_->safe_demolition->retired.size():0;
        if (original_production_enabled() && buildings_.size()+retired>=Rules::original_production_building_limit)
            return reject("Original production building collection limit reached");
        if (treasury_<original_influence_source_cost) return reject("Not enough money");
        if (original_production_enabled() && construction_spent_total_>UINT64_MAX-static_cast<std::uint64_t>(original_influence_source_cost))
            return reject("Original production construction counter exhausted");
        if (!next_building_id_ || next_building_id_>(original_production_enabled() ?
                Rules::original_production_building_limit+original_influence_purchase_limit:original_influence_purchase_limit))
            return reject("Original influence purchase ID exhausted");
        if (!in_bounds(command.cell)) return reject("Source footprint outside sandbox grid");
        for (const auto cell:building_footprint_cells(profile_,rule_version_,Object::OriginalInfluenceSource,command.cell)) {
            if (!in_bounds(cell)) return reject("Source footprint outside sandbox grid");
            if (bridge_at(cell) || bridge_bank_at(cell) || ramp_endpoint(cell))
                return reject("Source footprint overlaps protected infrastructure");
            if (!buildable(cell) || object_at(cell)!=Object::Empty)
                return reject("Source footprint is not empty permitted building land");
        }
    } else if (command.type==CommandType::DemolishBuilding) {
        const auto source=std::lower_bound(original_influence_->sources.begin(),original_influence_->sources.end(),command.building_id,
            [](const OriginalInfluenceSourceState& value,BuildingId id) { return value.building_id<id; });
        if (source==original_influence_->sources.end() || source->building_id!=command.building_id ||
            !source_type_valid(source->type)) return reject("Only a living typed Well72 influence source can be removed");
        const auto b=std::lower_bound(buildings_.begin(),buildings_.end(),command.building_id,
            [](const BuildingState& value,BuildingId id) { return value.id<id; });
        if (b==buildings_.end() || b->id!=command.building_id || !b->placed || b->kind!=Object::OriginalInfluenceSource)
            return reject("Influence source container is missing");
        if ((!original_production_enabled() && !couriers_.empty()) ||
            original_influence_->removed_sources.size()>=original_influence_purchase_limit)
            return reject("Influence removal has unsafe actor state or exhausted history");
        for (const auto cell:building_footprint_cells(profile_,rule_version_,b->kind,b->cell))
            if (!in_bounds(cell) || object_at(cell)!=b->kind || owners_[index(cell)]!=static_cast<std::uint32_t>(b->id))
                return reject("Influence source owner cells are inconsistent");
    } else return reject("Unsupported original influence command");
    result.accepted=true; result.changed=true; result.reason="Original influence command ready";
    return result;
}

CommandResult World::execute_original_influence_command(Command command,bool refresh_after) {
    auto result=validate_original_influence_command(command);
    if (!result.accepted) return result;
    performance::increment(performance::Counter::WorldExecutes);
    if (command.type==CommandType::PlaceOriginalInfluenceSource) {
        const auto id=static_cast<BuildingId>(next_building_id_++);
        BuildingState source{id,Object::OriginalInfluenceSource,command.cell,true};
        source.placed_tick=ticks_; buildings_.push_back(source);
        original_influence_->sources.push_back({id,command.source_type});
        if (original_production_enabled()) original_production_->housing.neutral_source_origins.push_back({id,command.cell});
        for (const auto cell:building_footprint_cells(profile_,rule_version_,source.kind,source.cell)) {
            objects_[index(cell)]=source.kind; owners_[index(cell)]=static_cast<std::uint32_t>(id);
        }
        treasury_-=original_influence_source_cost;
        construction_spent_total_+=std::uint64_t(original_influence_source_cost);
        result.reason="Influence source built. Authored60; no water, upkeep or workers.";
    } else {
        const auto id=command.building_id;
        const auto b=building(id);
        OriginalInfluenceRemovedSource removed{id,OriginalInfluenceSourceType::Well72,b.placed_tick,ticks_};
        const auto position=std::lower_bound(original_influence_->removed_sources.begin(),original_influence_->removed_sources.end(),id,
            [](const OriginalInfluenceRemovedSource& value,BuildingId key) { return value.building_id<key; });
        original_influence_->removed_sources.insert(position,removed);
        for (const auto cell:building_footprint_cells(profile_,rule_version_,b.kind,b.cell)) {
            objects_[index(cell)]=Object::Empty; owners_[index(cell)]=0;
        }
        std::erase_if(original_influence_->sources,[&](const OriginalInfluenceSourceState& value) { return value.building_id==id; });
        std::erase_if(buildings_,[&](const BuildingState& value) { return value.id==id; });
        result.reason="Influence source removed. No refund.";
    }
    ++command_sequence_; ++road_revision_; result.sequence=command_sequence_;
    rebuild_original_influence_cache();
    cached_route_.reset(); if (refresh_after) refresh_routes();
    return result;
}

void World::rebuild_original_influence_cache() {
    if (!original_influence_enabled() || !original_influence_ || !original_rules_model_)
        throw std::logic_error("influence cache requires explicit bound rule 8");
    if (original_influence_cache_build_count_==UINT64_MAX)
        throw std::overflow_error("influence cache counter exhausted");
    auto candidate=std::make_shared<OriginalInfluenceCache>();
    auto sources=original_influence_->sources;
    if (original_production_enabled()) {
        for (const auto& p:original_production_->water.providers) sources.push_back({p.building_id,OriginalInfluenceSourceType::Well72});
        for (const auto& p:original_production_->services.providers) {
            // Rule23 adds a physical visitor channel, not a new desirability
            // source. Preserve Rule22's explicit absence of Theatre DES.
            if (original_venue_house_service_enabled() && p.type==OriginalServiceProviderType::Theatre75 &&
                building(p.building_id).kind==Object::OriginalTheatre75) continue;
            const auto model=original_influence_model(building(p.building_id).kind);
            if (!model) invalid("paid Provider lost its own named desirability model");
            sources.push_back({p.building_id,static_cast<OriginalInfluenceSourceType>(*model)});
        }
        if (original_music_venues_enabled()) {
            if (!original_production_->culture) invalid("Music influence requires its exact culture state");
            // Rule22 changes the School's visitor owner, not its existing
            // physical model influence. The Theatre supplies no new DES source.
            for (const auto& school:original_production_->culture->schools) {
                const auto kind=building(school.building_id).kind;
                const auto model=original_influence_model(kind);
                if (kind!=Object::OriginalMusicSchool211 || !model)
                    invalid("paid Music School lost its own named desirability model");
                sources.push_back({school.building_id,static_cast<OriginalInfluenceSourceType>(*model)});
            }
        }
        std::sort(sources.begin(),sources.end(),[](const auto& a,const auto& b){return a.building_id<b.building_id;});
    }
    candidate->sources.reserve(sources.size());candidate->prefixes.reserve(sources.size()+1);
    candidate->prefixes.emplace_back(width_,height_);
    for (const auto& source:sources) {
        const auto& b=building(source.building_id);
        const auto model=original_influence_model(b.kind);
        if (!model || source.type!=static_cast<OriginalInfluenceSourceType>(*model) || !b.placed ||
            (!original_production_enabled() && b.kind!=Object::OriginalInfluenceSource)) invalid("influence cache source is not its own placed model");
        const auto footprint=building_footprint(profile_,rule_version_,b.kind);
        const original::SpreadSpec spread{
            original_rules_model_->building_value(*model,original::BuildingField::Desirability),
            original_rules_model_->building_value(*model,original::BuildingField::DesirabilityStep),
            original_rules_model_->building_value(*model,original::BuildingField::DesirabilityStepSize),
            original_rules_model_->building_value(*model,original::BuildingField::DesirabilityRange)};
        candidate->prefixes.push_back(candidate->prefixes.back());
        candidate->prefixes.back().apply_source({{b.cell.x,b.cell.y,footprint.width},spread},original::WallBlocking::None);
        candidate->sources.push_back({source,b.cell,footprint,spread});
    }
    original_influence_cache_=std::move(candidate);
    ++original_influence_cache_build_count_;
}

int World::original_influence_value_at(Cell origin,int side) const {
    if (!original_influence_enabled() || !original_influence_cache_)
        throw std::logic_error("original influence map is unavailable");
    return original_influence_cache_->prefixes.back().building_value({origin.x,origin.y,side},0);
}

int World::original_house_cardinal_water_bonus(BuildingId household) const {
    const auto& home=building(household);
    if (!original_influence_enabled() || !home.placed || !original_house_kind(home.kind))
        throw std::invalid_argument("original House query requires an actual paid House");
    if (!original_cardinal_water_enabled() || home.kind==Object::OriginalElitePlot) return 0;
    if (!original_terrain_facts_) throw std::logic_error("Rule14 lost immutable terrain facts");
    return original_terrain_facts_->cardinal_water_bonus(home.cell,
        building_footprint(profile_,rule_version_,home.kind).width);
}

int World::original_house_influence_value(BuildingId household) const {
    const auto& home=building(household);
    const auto bonus=original_house_cardinal_water_bonus(household);
    return original_influence_value_at(home.cell,
        building_footprint(profile_,rule_version_,home.kind).width)+bonus;
}

OriginalInfluenceProvenance World::original_influence_provenance(BuildingId household) const {
    const auto& home=building(household);
    if (!original_influence_enabled() || !original_influence_cache_ || !original_house_kind(home.kind) || !home.placed)
        throw std::invalid_argument("original influence provenance requires a placed rule-8 Household");
    OriginalInfluenceProvenance result;
    result.household_id=household;result.value=-101;
    result.difficulty=original_rules_model_->difficulty();
    result.model_sha256=original_rules_model_->provenance().sha256;
    const auto& cache=*original_influence_cache_;
    const auto footprint=building_footprint(profile_,rule_version_,home.kind);
    for (int y=home.cell.y;y<home.cell.y+footprint.height;++y) for (int x=home.cell.x;x<home.cell.x+footprint.width;++x) {
        const auto value=cache.prefixes.back().at(x,y);
        if (value>result.value) { result.value=value; result.selected_cell={x,y}; }
    }
    result.sources.reserve(cache.sources.size());
    for (std::size_t i=0;i<cache.sources.size();++i) {
        const auto& source=cache.sources[i];
        const auto before=cache.prefixes[i].at(result.selected_cell.x,result.selected_cell.y);
        const auto after=cache.prefixes[i+1].at(result.selected_cell.x,result.selected_cell.y);
        const auto contribution=contribution_at(source,result.selected_cell);
        if (std::clamp(std::int64_t(before)+contribution,std::int64_t{-100},std::int64_t{100})!=after)
            throw std::logic_error("influence provenance disagrees with authoritative core prefix");
        result.sources.push_back({source.identity.building_id,source.identity.type,source.origin,source.footprint,source.spread,
            before,contribution,after});
    }
    result.raw_value=result.value;
    result.cardinal_water_bonus=original_house_cardinal_water_bonus(household);
    result.value+=result.cardinal_water_bonus;
    if (original_cardinal_water_enabled()) result.source_map_sha256=original_terrain_facts_->source_map_sha256();
    return result;
}

OriginalInfluenceInspection World::original_influence_inspection(BuildingId household) const {
    auto provenance=original_influence_provenance(household);
    return {provenance.selected_cell,provenance.value,std::move(provenance.sources),
        provenance.raw_value,provenance.cardinal_water_bonus,std::move(provenance.source_map_sha256)};
}

} // namespace openemperor::simulation
