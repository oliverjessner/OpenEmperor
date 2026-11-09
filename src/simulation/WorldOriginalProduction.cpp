#include "simulation/World.h"
#include "simulation/OriginalGoodsLedger.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include "simulation/original/OriginalRulesModel.h"
#include "simulation/original/OriginalScheduler.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
constexpr auto ceramics=static_cast<std::size_t>(original::Goods::Ceramics);
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
using GoodsContext=original_industry_goods::Context;
std::uint64_t retired_spending(const OriginalProductionState& state) {
    std::uint64_t result=0;
    if (state.safe_demolition) for (const auto& entry:state.safe_demolition->retired)
        result+=static_cast<std::uint64_t>(entry.kind==OriginalRetiredKind::ClaySource ?
            Rules::clay_source_cost:Rules::household_cost);
    return result;
}
// Called only for Rule19 after complete purchase, bounds and Housing witness
// validation. Historical roads have no per-cell placement clock; they are not
// guessed here. New retirement rows must agree with every retained purchase.
void validate_retired_purchase_lifetimes(const WorldSnapshot& s) {
    const auto& production=*s.original_production;
    const auto& retired=production.safe_demolition->retired;
    if (s.bridges.size()>bridge_max_spans || s.ramps.size()>ramp_max_links)
        invalid("Rule19 retired geometry exceeds existing infrastructure bounds");
    const auto overlaps=[](Cell a,BuildingFootprint as,Cell b,BuildingFootprint bs) {
        const auto ax=static_cast<std::int64_t>(a.x),ay=static_cast<std::int64_t>(a.y);
        const auto bx=static_cast<std::int64_t>(b.x),by=static_cast<std::int64_t>(b.y);
        return ax<bx+bs.width && bx<ax+as.width && ay<by+bs.height && by<ay+as.height;
    };
    const auto chronological=[](BuildingId a,std::uint64_t placed_a,BuildingId b,std::uint64_t placed_b) {
        if (a<b ? placed_a>placed_b:placed_b>placed_a)
            invalid("Rule19 retired purchase contradicts monotone paid placement order");
    };
    const BuildingFootprint closed{2,2};
    for (std::size_t i=0;i<retired.size();++i) {
        const auto& entry=retired[i];const Cell origin{entry.origin_x,entry.origin_y};
        for (std::size_t j=0;j<i;++j) {
            const auto& earlier=retired[j];
            chronological(earlier.building_id,earlier.placed_tick,entry.building_id,entry.placed_tick);
            if (overlaps(origin,closed,{earlier.origin_x,earlier.origin_y},closed) && earlier.removed_tick>entry.placed_tick)
                invalid("Rule19 retired purchase footprints overlap during their paid lifetimes");
        }
        for (const auto& body:s.buildings) {
            chronological(entry.building_id,entry.placed_tick,body.id,body.placed_tick);
            const auto current=building_footprint(s.profile,s.rule_version,body.kind);
            const auto initial=body.kind==Object::OriginalEliteHouse ?
                building_footprint(s.profile,s.rule_version,Object::OriginalElitePlot):current;
            if (overlaps(origin,closed,body.cell,initial) &&
                (body.id<entry.building_id || entry.removed_tick>body.placed_tick))
                invalid("Rule19 retired purchase overlaps a live paid purchase lifetime");
            if (body.kind!=Object::OriginalEliteHouse || !overlaps(origin,closed,body.cell,current)) continue;
            const auto& houses=production.housing.houses;
            const auto house=std::lower_bound(houses.begin(),houses.end(),body.id,
                [](const auto& row,BuildingId id){return row.building_id<id;});
            if (house==houses.end() || house->building_id!=body.id || !house->first_admission_tick)
                invalid("Rule19 retired geometry requires the actual Elite materialization witness");
            // The plot owned its initial2x2 from purchase, but only acquired
            // the extension during first admission. Commands at that tick run
            // after materialization, so removal must be strictly earlier.
            for (int y=0;y<current.height;++y) for (int x=0;x<current.width;++x) {
                if (x<initial.width && y<initial.height) continue;
                const Cell cell{body.cell.x+x,body.cell.y+y};
                if (overlaps(origin,closed,cell,{1,1}) && entry.removed_tick>=*house->first_admission_tick)
                    invalid("Rule19 retirement overlaps an already materialized Elite extension");
            }
        }
        for (const auto& source:s.original_influence->removed_sources) {
            chronological(entry.building_id,entry.placed_tick,source.building_id,source.placed_tick);
            const auto& origins=production.housing.neutral_source_origins;
            const auto source_origin=std::lower_bound(origins.begin(),origins.end(),source.building_id,
                [](const auto& row,BuildingId id){return row.building_id<id;});
            if (source_origin==origins.end() || source_origin->building_id!=source.building_id)
                invalid("Rule19 retired geometry requires the retained neutral source origin");
            const bool separated=entry.building_id<source.building_id ?
                entry.removed_tick<=source.placed_tick:source.removed_tick<=entry.placed_tick;
            if (overlaps(origin,closed,source_origin->origin,closed) && !separated)
                invalid("Rule19 retired purchase overlaps a closed neutral source lifetime");
        }
        // Bridge banks/decks and Ramp endpoints remain reserved after purchase
        // even if a land road is later disconnected. Their independent IDs do
        // not order Building commands within a tick; equality permits a real
        // removal followed by paid infrastructure construction that same tick.
        for (const auto& bridge:s.bridges) {
            if (bridge.water_cells.size()>bridge_max_water_cells)
                invalid("Rule19 retired geometry exceeds the existing Bridge span bound");
            bool touching=overlaps(origin,closed,bridge.bank_a,{1,1}) || overlaps(origin,closed,bridge.bank_b,{1,1});
            for (const auto cell:bridge.water_cells) touching|=overlaps(origin,closed,cell,{1,1});
            if (touching && entry.removed_tick>bridge.placed_tick)
                invalid("Rule19 retired purchase overlaps a permanent Bridge lifetime");
        }
        for (const auto& ramp:s.ramps)
            if ((overlaps(origin,closed,ramp.endpoint_a,{1,1}) || overlaps(origin,closed,ramp.endpoint_b,{1,1})) &&
                entry.removed_tick>ramp.placed_tick)
                invalid("Rule19 retired purchase overlaps a permanent Ramp lifetime");
    }
}
std::uint64_t paid_cost(Object kind) {
    switch (kind) {
    case Object::Household:
    case Object::OriginalElitePlot:
    case Object::OriginalEliteHouse:return Rules::household_cost;
    case Object::FireWatch:return Rules::fire_watch_cost;
    case Object::ClaySource:return Rules::clay_source_cost;
    case Object::Pottery:return Rules::pottery_cost;
    case Object::Warehouse:return Rules::warehouse_cost;
    case Object::Market:return Rules::market_cost;
    case Object::OriginalLoggingShed38:
    case Object::OriginalBronzeSmelter39:
    case Object::OriginalBronzewareMaker42:
    case Object::OriginalLacquerwareMaker44:
    case Object::OriginalPaperMaker45:
    case Object::OriginalWeaver47:
    case Object::OriginalTeaCuringShed237:
    case Object::OriginalLacquerRefinery238:
    case Object::OriginalSilkwormShed239:
    case Object::OriginalTeaBush26:
    case Object::OriginalLacquerTree27:
    case Object::OriginalMulberryTree28:
    case Object::OriginalHempFarm:
    case Object::OriginalHempField:
    case Object::OriginalFoodFarm:
    case Object::OriginalWheatField:
    case Object::OriginalMilletField:
    case Object::HealthPost:
    case Object::OriginalDaoistShrine215:
    case Object::OriginalBuddhistShrine217:
    case Object::OriginalTheatre75:
    case Object::OriginalMusicSchool211:
    case Object::OriginalAcrobatSchool212:
    case Object::OriginalDramaSchool213:
    case Object::OriginalConfucianAcademy219:
    case Object::OriginalAncestorShrine214:
    case Object::OriginalWaterWell72:
    case Object::ServicePost:return 0;
    case Object::OriginalInfluenceSource:return original_influence_source_cost;
    default:invalid("unsupported original production building kind");
    }
}
std::optional<original::ModelBuilding> model_priced(Object kind) {
    using B=original::ModelBuilding;
    switch (kind) {
    case Object::OriginalHempFarm:return B::HempFarm;
    case Object::OriginalHempField:return B::HempField;
    case Object::OriginalFoodFarm:return B::Farmhouse;
    case Object::OriginalWheatField:return B::WheatField;
    case Object::OriginalMilletField:return B::MilletField;
    case Object::HealthPost:return B::HerbalistsStall;
    case Object::ServicePost:return B::AcupuncturistsClinic;
    case Object::OriginalAncestorShrine214:return B::AncestralShrine;
    case Object::OriginalWaterWell72:return B::Well;
    case Object::OriginalDaoistShrine215:return B::DaoistShrine;
    case Object::OriginalBuddhistShrine217:return B::BuddhistShrine;
    case Object::OriginalConfucianAcademy219:return B::ConfucianAcademy;
    case Object::OriginalTheatre75:return B::TheatrePavilion;
    case Object::OriginalMusicSchool211:return B::MusicSchool;
    case Object::OriginalAcrobatSchool212:return B::AcrobatSchool;
    case Object::OriginalDramaSchool213:return B::DramaSchool;
    default:return original_industry_model(kind);
    }
}
template<class Buildings,class Bridges> std::uint64_t spending(const Buildings& buildings,
    const Bridges& bridges,std::size_t ramps,std::uint64_t roads,std::size_t removed,
    const original::OriginalRulesModel* model=nullptr) {
    std::uint64_t result=0;
    const auto add=[&](std::uint64_t count,std::uint64_t cost) {
        if (!cost) return;
        if (count>UINT64_MAX/cost || result>UINT64_MAX-count*cost) invalid("original production spending counter exhausted");
        result+=count*cost;
    };
    add(roads,Rules::road_cost);add(ramps,ramp_cost);add(removed,original_influence_source_cost);
    for (const auto& b:buildings) {
        const auto priced=model_priced(b.kind);
        const auto prepared=priced && model ? model->building_value(*priced,original::BuildingField::Cost):0;
        if (priced && model && prepared<=0) invalid("original production requires positive prepared price");
        const auto cost=priced ? static_cast<std::uint64_t>(prepared):paid_cost(b.kind);
        if (cost) add(1,cost);
    }
    for (const auto& bridge:bridges) {
        if (bridge.paid_cost<=0) invalid("invalid original production bridge price");
        add(1,static_cast<std::uint64_t>(bridge.paid_cost));
    }
    return result;
}
template<class Building> void validate_neutral_fields(const Building& b) {
    BuildingSnapshot expected{b.id,b.kind,b.cell,true};expected.placed_tick=b.placed_tick;
    const bool field=b.kind==Object::OriginalHempField || b.kind==Object::OriginalWheatField || b.kind==Object::OriginalMilletField;
    if (b.kind!=Object::OriginalInfluenceSource && !field) {
        expected.fire_risk=b.fire_risk;expected.fire_protection_until_tick=b.fire_protection_until_tick;
        expected.fire_until_tick=b.fire_until_tick;
    }
    if (!original_house_kind(b.kind) && b.kind!=Object::OriginalInfluenceSource && !field) {
        expected.operating_enabled=b.operating_enabled;expected.workforce_priority=b.workforce_priority;
    }
    if (original_house_kind(b.kind)) expected.taxes_paid_total=b.taxes_paid_total;
    if (b.kind==Object::ClaySource) {
        expected.output=b.output;expected.progress=b.progress;expected.clay_extracted=b.clay_extracted;
    } else if (b.kind==Object::Pottery) {
        expected.input_clay=b.input_clay;expected.output=b.output;expected.reserved_incoming=b.reserved_incoming;
        expected.progress=b.progress;expected.active_recipe_clay=b.active_recipe_clay;
        expected.recipes_completed=b.recipes_completed;
    } else if (b.kind==Object::Warehouse) expected.reserved_incoming=b.reserved_incoming;
    else if (b.kind==Object::OriginalBronzewareMaker42) {expected.input_clay=b.input_clay;expected.reserved_incoming=b.reserved_incoming;}
    else if (b.kind==Object::OriginalHempFarm || b.kind==Object::OriginalFoodFarm) expected.progress=b.progress;
    if (b!=expected) invalid("unsupported original production building authority");
}
}

World World::create_original_production_development(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original production development requires policy3 and a prepared model");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("original production requires a positive signed-word Shelter capacity");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,kOriginalProductionRuleVersion,
        std::move(permissions),true,std::move(model));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();
    world.rebuild_original_influence_cache();return world;
}

World World::create_original_fire_protection(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    auto world=create_original_production_development(std::move(permissions),std::move(model));
    world.rule_version_=kOriginalFireProtectionRuleVersion;
    world.original_production_->fire_protection.emplace();return world;
}
World World::create_original_sustainable_economy(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    auto world=create_original_fire_protection(std::move(permissions),std::move(model));
    world.rule_version_=kOriginalSustainableRuleVersion;return world;
}
World World::create_original_market_logistics(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    auto world=create_original_sustainable_economy(std::move(permissions),std::move(model));
    world.rule_version_=kOriginalMarketLogisticsRuleVersion;return world;
}
World World::create_original_cardinal_water(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original cardinal-water creation requires policy3, prepared model and immutable terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original cardinal-water Shelter capacity");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,kOriginalCardinalWaterRuleVersion,
        std::move(permissions),true,std::move(model),std::move(terrain));
    world.original_production_->fire_protection.emplace();
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();world.rebuild_original_influence_cache();return world;
}
World World::restore_original_cardinal_water(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!simulation::original_cardinal_water_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original cardinal-water restore requires exact rule14, policy3, model and terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original cardinal-water Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::create_original_inspector_coordination(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original inspector coordination creation requires policy3, prepared model and immutable terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original inspector coordination Shelter capacity");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,kOriginalInspectorCoordinationRuleVersion,
        std::move(permissions),true,std::move(model),std::move(terrain));
    world.original_production_->fire_protection.emplace();
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();world.rebuild_original_influence_cache();return world;
}
World World::restore_original_inspector_coordination(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!simulation::original_inspector_coordination_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original inspector coordination restore requires exact rule15, policy3, model and terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original inspector coordination Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::create_original_deadline_fire_dispatch(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original deadline fire dispatch creation requires policy3, prepared model and immutable terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original deadline fire dispatch Shelter capacity");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,kOriginalDeadlineFireDispatchRuleVersion,
        std::move(permissions),true,std::move(model),std::move(terrain));
    world.original_production_->fire_protection.emplace();
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();world.rebuild_original_influence_cache();return world;
}
World World::restore_original_deadline_fire_dispatch(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!simulation::original_deadline_fire_dispatch_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original deadline fire dispatch restore requires exact rule16, policy3, model and terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original deadline fire dispatch Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::create_original_deadline_service_dispatch(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original deadline service dispatch creation requires policy3, prepared model and immutable terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original deadline service dispatch Shelter capacity");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,kOriginalDeadlineServiceDispatchRuleVersion,
        std::move(permissions),true,std::move(model),std::move(terrain));
    world.original_production_->fire_protection.emplace();
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();world.rebuild_original_influence_cache();return world;
}
World World::restore_original_deadline_service_dispatch(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!simulation::original_deadline_service_dispatch_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original deadline service dispatch restore requires exact rule17, policy3, model and terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original deadline service dispatch Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::create_original_deadline_food_dispatch(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original deadline food dispatch creation requires policy3, prepared model and immutable terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original deadline food dispatch Shelter capacity");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,kOriginalDeadlineFoodDispatchRuleVersion,
        std::move(permissions),true,std::move(model),std::move(terrain));
    world.original_production_->fire_protection.emplace();
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();world.rebuild_original_influence_cache();return world;
}
World World::restore_original_deadline_food_dispatch(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!simulation::original_deadline_food_dispatch_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original deadline food dispatch restore requires exact rule18, policy3, model and terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original deadline food dispatch Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::create_original_safe_demolition(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=create_original_deadline_food_dispatch(std::move(permissions),std::move(model),std::move(terrain));
    world.rule_version_=kOriginalSafeDemolitionRuleVersion;
    world.original_production_->safe_demolition.emplace();
    return world;
}
World World::restore_original_safe_demolition(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,
    std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if (!simulation::original_safe_demolition_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain)
        invalid("original safe demolition restore requires exact rule19, policy3, model and terrain facts");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original safe demolition Shelter capacity");
    validate_original_production_snapshot(snapshot);
    for (const auto& entry:snapshot.original_production->safe_demolition->retired) {
        const auto kind=entry.kind==OriginalRetiredKind::ClaySource ? Object::ClaySource:Object::Household;
        if (original_house_kind(kind) && !terrain->supports_cardinal_footprint(
            {entry.origin_x,entry.origin_y},building_footprint(snapshot.profile,snapshot.rule_version,kind).width))
            invalid("retired original House cardinal perimeter exceeds its bound original terrain");
        for (const auto cell:building_footprint_cells(snapshot.profile,snapshot.rule_version,kind,{entry.origin_x,entry.origin_y}))
            if (!permissions->building_allowed(cell) || permissions->protected_original(cell))
                invalid("retired original purchase lacks source-bound building permission");
    }
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::restore_original_fire_protection(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (snapshot.profile!=RulesProfile::CityV16 || snapshot.rule_version!=kOriginalFireProtectionRuleVersion ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original production restore requires explicit rule11, policy3 and prepared model");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original production Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}
World World::restore_original_sustainable_economy(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (snapshot.profile!=RulesProfile::CityV16 || snapshot.rule_version!=kOriginalSustainableRuleVersion ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original sustainable restore requires explicit rule12, policy3 and prepared model");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original sustainable Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}

World World::restore_original_market_logistics(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (!simulation::original_market_logistics_enabled(snapshot.profile,snapshot.rule_version) ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original market logistics restore requires explicit rule13, policy3 and prepared model");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original logistics Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}

World World::restore_original_production_development(const WorldSnapshot& snapshot,
    std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model) {
    if (snapshot.profile!=RulesProfile::CityV16 || snapshot.rule_version!=kOriginalProductionRuleVersion ||
        !permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model)
        invalid("original production restore requires explicit rule10, policy3 and prepared model");
    const auto capacity=model->house_rules().row_capacity(original::ModelHouse::Shelter);
    if (capacity<=0 || capacity>INT16_MAX) invalid("invalid original production Shelter capacity");
    validate_original_production_snapshot(snapshot);
    auto world=restore_impl(snapshot,{},std::move(permissions),true,std::move(model));
    world.validate_original_hemp_model();world.validate_original_food_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();world.validate_original_industry_model();
    world.validate_original_finance_model();return world;
}

void World::validate_original_production_snapshot(const WorldSnapshot& s) {
    validate_original_music_venues_snapshot(s);
    if (!simulation::original_production_enabled(s.profile,s.rule_version)) {
        if (s.original_production) invalid("original production requires City-v16 rule10");
        return;
    }
    if (!s.original_production || s.original_production->development_revision!=kOriginalProductionDevelopmentRevision ||
        !s.original_economy || !s.original_economy->model_source || !s.original_influence || !s.original_goods ||
        s.width<1 || s.height<1 || s.width>228 || s.height>228)
        invalid("invalid original production development revision or required authority");
    const bool safe=simulation::original_safe_demolition_enabled(s.profile,s.rule_version);
    if (static_cast<bool>(s.original_production->safe_demolition)!=safe)
        invalid("safe demolition journal requires exact Rule19 and is mandatory there");
    const auto* retired=safe ? &s.original_production->safe_demolition->retired:nullptr;
    const auto retired_count=retired ? retired->size():0;
    if (retired_count>original_safe_demolition_retired_limit ||
        (safe && s.buildings.size()+retired_count>Rules::original_production_building_limit))
        invalid("safe demolition exceeds its bounded lifetime purchase authority");
    const auto& source=*s.original_economy->model_source;
    if (source.format_version!=1 || source.sha256.size()!=64 || !source.byte_count ||
        source.byte_count>original::max_model_bytes || static_cast<std::size_t>(source.difficulty)>=original::difficulty_count ||
        !std::all_of(source.sha256.begin(),source.sha256.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
        invalid("invalid original production model source identity");
    if (s.ticks>original::scheduler_max_tick || s.workshop || s.warehouse || s.total_produced ||
        s.workshop_stock || s.production_progress || s.courier_cargo || s.warehouse_stock ||
        s.phase!=CourierPhase::IdleAtWorkshop || !s.path.empty() || s.path_vertex || s.edge_progress ||
        s.next_household_id!=first_household_id || s.next_production_id!=8 ||
        s.last_dispatched_household || s.last_dispatched_food_household || s.last_dispatched_service_household ||
        s.food_produced_total ||
        s.demolition_history!=DemolitionHistory{} || s.buildings.size()>Rules::original_production_building_limit ||
        s.original_economy->houses.size()>Rules::city_v10_household_limit ||
        s.original_influence->sources.size()>original_influence_source_limit ||
        s.original_influence->removed_sources.size()>original_influence_purchase_limit)
        invalid("unsupported original production runtime authority");
    std::array<bool,Rules::original_production_building_limit+original_influence_purchase_limit+1> seen{};
    const auto mark=[&](BuildingId id) {
        const auto value=static_cast<std::uint32_t>(id);
        if (!value || value>=s.next_building_id || value>=seen.size() || seen[value])
            invalid("original production purchase identities must be disjoint and contiguous");
        seen[value]=true;
    };
    std::size_t retired_couriers=0;
    if (retired) for (std::size_t i=0;i<retired->size();++i) {
        const auto& entry=(*retired)[i];mark(entry.building_id);
        if ((i && !((*retired)[i-1].building_id<entry.building_id)) ||
            (entry.kind!=OriginalRetiredKind::EmptyHouse && entry.kind!=OriginalRetiredKind::ClaySource) ||
            entry.placed_tick>entry.removed_tick || entry.removed_tick>s.ticks)
            invalid("invalid sorted retired purchase identity or lifetime");
        const auto kind=entry.kind==OriginalRetiredKind::ClaySource ? Object::ClaySource:Object::Household;
        const auto size=building_footprint(s.profile,s.rule_version,kind);
        if (entry.origin_x<0 || entry.origin_y<0 || entry.origin_x>s.width-size.width || entry.origin_y>s.height-size.height)
            invalid("retired purchase footprint outside saved grid");
        if (entry.kind==OriginalRetiredKind::EmptyHouse) {
            if (entry.courier_id || entry.clay_trip || entry.clay_extracted)
                invalid("retired empty House contains unsupported production authority");
        } else {
            if (!entry.courier_id || entry.clay_extracted>(entry.removed_tick-entry.placed_tick)/32 ||
                static_cast<bool>(entry.clay_trip)!=(entry.clay_extracted!=0))
                invalid("retired Clay source lacks its bounded settled production history");
            const auto active_before=std::count_if(s.couriers.begin(),s.couriers.end(),[&](const auto& actor){
                return actor.owner<entry.building_id;
            });
            if (static_cast<std::uint32_t>(*entry.courier_id)!=static_cast<std::uint32_t>(active_before)+retired_couriers+1)
                invalid("retired Clay courier differs from paid purchase order");
            ++retired_couriers;
        }
    }
    if (safe && s.couriers.size()+retired_couriers>Rules::original_production_courier_limit)
        invalid("safe demolition exceeds the existing lifetime courier budget");
    std::size_t house_index=0,warehouse_index=0,influence_index=0,courier_index=0;
    std::uint64_t residents=0;
    for (std::size_t i=0;i<s.buildings.size();++i) {
        const auto& b=s.buildings[i];mark(b.id);
        const auto retired_before=retired ? static_cast<std::size_t>(std::count_if(retired->begin(),retired->end(),[&](const auto& entry){
            return entry.kind==OriginalRetiredKind::ClaySource && entry.building_id<b.id;
        })):0;
        if ((i && !(s.buildings[i-1].id<b.id)) || b.placed_tick>s.ticks)
            invalid("original production buildings must be sorted with valid placement time");
        if (b.kind==Object::FireWatch && !simulation::original_fire_protection_enabled(s.profile,s.rule_version))
            invalid("Rule10 does not admit FireWatch");
        (void)paid_cost(b.kind);validate_neutral_fields(b);
        const auto footprint=building_footprint(s.profile,s.rule_version,b.kind);
        // Check by subtraction before any helper adds footprint offsets to an
        // untrusted origin. In particular, INT_MAX must never reach cell+1.
        if (b.cell.x<0 || b.cell.y<0 || b.cell.x>s.width-footprint.width || b.cell.y>s.height-footprint.height)
            invalid("original production building footprint outside saved grid");
        if (b.kind==Object::FireWatch || b.kind==Object::ClaySource || b.kind==Object::Pottery || b.kind==Object::Warehouse || b.kind==Object::Market || b.kind==Object::OriginalHempFarm || b.kind==Object::OriginalFoodFarm ||
            b.kind==Object::HealthPost || b.kind==Object::ServicePost || b.kind==Object::OriginalAncestorShrine214 || b.kind==Object::OriginalWaterWell72 ||
            b.kind==Object::OriginalDaoistShrine215 || b.kind==Object::OriginalBuddhistShrine217 || b.kind==Object::OriginalConfucianAcademy219 ||
            (b.kind==Object::OriginalTheatre75 && simulation::original_venue_house_service_enabled(s.profile,s.rule_version)) || b.kind==Object::OriginalMusicSchool211 ||
            b.kind==Object::OriginalAcrobatSchool212 || b.kind==Object::OriginalDramaSchool213 || original_industry_source(b.kind)) {
            if (courier_index>=s.couriers.size()) invalid("missing purchased original production courier");
            const auto& c=s.couriers[courier_index];
            const auto role=b.kind==Object::OriginalTheatre75 ? CourierRole::OriginalVenueMusic:b.kind==Object::FireWatch ? CourierRole::FireInspector:original_industry_source(b.kind) ? (original_industry_goods::output(b.kind) ? CourierRole::OriginalLuxuryToWarehouse:CourierRole::OriginalAuxToWarehouse):
                b.kind==Object::OriginalWaterWell72 ? CourierRole::OriginalWater:b.kind==Object::HealthPost ? CourierRole::HealthWorker:
                b.kind==Object::ServicePost ? CourierRole::Service:
                b.kind==Object::OriginalMusicSchool211 ? (simulation::original_music_venues_enabled(s.profile,s.rule_version) ? CourierRole::OriginalMusicPerformer:CourierRole::OriginalMusic):
                b.kind==Object::OriginalAcrobatSchool212 ? CourierRole::OriginalAcrobat:
                b.kind==Object::OriginalDramaSchool213 ? CourierRole::OriginalDrama:
                b.kind==Object::OriginalDaoistShrine215 ? CourierRole::OriginalDaoist:
                b.kind==Object::OriginalBuddhistShrine217 ? CourierRole::OriginalBuddhist:
                b.kind==Object::OriginalConfucianAcademy219 ? CourierRole::OriginalConfucian:
                b.kind==Object::OriginalAncestorShrine214 ? CourierRole::OriginalAncestor:
                b.kind==Object::OriginalFoodFarm ? CourierRole::OriginalFoodToMarket:
                b.kind==Object::OriginalHempFarm ? CourierRole::OriginalHempToWarehouse:
                b.kind==Object::ClaySource ? CourierRole::Clay:
                b.kind==Object::Pottery ? CourierRole::Pottery:b.kind==Object::Market ?
                CourierRole::MarketPotteryDistribution:CourierRole::MarketPotteryInbound;
            if (static_cast<std::uint32_t>(c.id)!=courier_index+retired_before+1 || c.owner!=b.id || c.role!=role)
                invalid("original production courier IDs must follow purchased owners");
            ++courier_index;
            if (b.kind==Object::Warehouse) {
                if (courier_index>=s.couriers.size()) invalid("missing paid Warehouse Aux supplier");
                const auto& supplier=s.couriers[courier_index];
                if (static_cast<std::uint32_t>(supplier.id)!=courier_index+retired_before+1 || supplier.owner!=b.id || supplier.role!=CourierRole::OriginalAuxSupplier)
                    invalid("Warehouse Aux supplier must follow the paid Goods courier");
                ++courier_index;
            }
            if (b.kind==Object::Market) {
                if (courier_index>=s.couriers.size()) invalid("missing purchased Food Distributor");
                const auto& food=s.couriers[courier_index];
                if (static_cast<std::uint32_t>(food.id)!=courier_index+retired_before+1 || food.owner!=b.id || food.role!=CourierRole::OriginalFoodToHouse)
                    invalid("Food Distributor IDs must follow their paid Market");
                ++courier_index;
            }
        }
        const auto age=s.ticks-b.placed_tick;
        if (original_house_kind(b.kind)) {
            if (house_index>=s.original_economy->houses.size()) invalid("missing original production house");
            const auto& h=s.original_economy->houses[house_index++];
            const auto occupants=h.house.occupants;
            if ((b.kind==Object::OriginalElitePlot && occupants) ||
                (b.kind==Object::OriginalEliteHouse && occupants<=0))
                invalid("Elite geometry must match empty Plot or materialized residents");
            const auto maximum=age<120 ? 0:std::min<std::uint64_t>(INT16_MAX,2*std::min<std::uint64_t>(INT16_MAX,(age-120)/40+1));
            if (h.building_id!=b.id || occupants<0 || static_cast<std::uint64_t>(occupants)>maximum)
                invalid("original residents exceed paid house immigration history");
            OriginalHouseState empty{b.id};
            if (original_elite_house_kind(b.kind)) empty.house.type=original::HouseType::EliteUnoccupied;
            if (!occupants && h!=empty) invalid("empty original House owns unsupported authority");
            const bool family=original_elite_house_kind(b.kind) ?
                h.house.type>=original::HouseType::EliteAbandoned && h.house.type<=original::HouseType::HeavenlyCompound:
                h.house.type>=original::HouseType::Shelter && h.house.type<=original::HouseType::LuxuriousApartment;
            if (occupants && (!family || h.original_status!=3 || !h.access_enabled || h.cycle_phase ||
                h.house.devolve_counter>original::devolve_limit(h.house.type))) invalid("unsupported original House family/state");
            residents+=static_cast<std::uint64_t>(occupants);
        } else if (b.kind==Object::Warehouse || b.kind==Object::Market || b.kind==Object::OriginalHempFarm ||
                   b.kind==Object::OriginalPaperMaker45 || original_industry_goods::output(b.kind)) {
            if (warehouse_index>=s.original_goods->stocks.size() ||
                s.original_goods->stocks[warehouse_index++].building_id!=b.id)
                invalid("original production requires exactly one typed stock per Warehouse");
        } else if (b.kind==Object::OriginalInfluenceSource) {
            if (influence_index>=s.original_influence->sources.size()) invalid("missing original influence source");
            const auto& typed=s.original_influence->sources[influence_index++];
            if (typed.building_id!=b.id || typed.type!=OriginalInfluenceSourceType::Well72)
                invalid("invalid original production influence identity");
        } else if (b.kind==Object::ClaySource) {
            if (b.clay_extracted>age/32) invalid("Clay history exceeds actual recipe clock");
        } else if (b.kind==Object::Pottery && b.recipes_completed>age/65) invalid("Pottery history exceeds actual recipe clock");
    }
    if (house_index!=s.original_economy->houses.size() || warehouse_index!=s.original_goods->stocks.size() ||
        influence_index!=s.original_influence->sources.size() || residents!=s.original_production->residents_admitted ||
        courier_index!=s.couriers.size() || s.next_courier_id!=courier_index+retired_couriers+1)
        invalid("orphan original production house, stock, source or resident history");
    const auto& removed=s.original_influence->removed_sources;
    for (std::size_t i=0;i<removed.size();++i) {
        const auto& entry=removed[i];mark(entry.building_id);
        if ((i && !(removed[i-1].building_id<entry.building_id)) || entry.type!=OriginalInfluenceSourceType::Well72 ||
            entry.placed_tick>entry.removed_tick || entry.removed_tick>s.ticks)
            invalid("invalid original production source removal history");
    }
    if (s.next_building_id!=s.buildings.size()+removed.size()+retired_count+1)
        invalid("invalid original production next building identity");
    for (std::uint32_t id=1;id<s.next_building_id;++id) if (!seen[id]) invalid("missing paid original production purchase");
    const auto spent=spending(s.buildings,s.bridges,s.ramps.size(),s.roads_placed_total,removed.size())+retired_spending(*s.original_production);
    const auto model_priced_count=std::count_if(s.buildings.begin(),s.buildings.end(),[](const auto& body){return model_priced(body.kind).has_value();});
    const auto materialized=static_cast<std::uint64_t>(std::count_if(s.buildings.begin(),s.buildings.end(),
        [](const auto& b){return b.kind==Object::OriginalEliteHouse;}));
    if (s.construction_spent_total<spent || s.construction_spent_total-spent<static_cast<std::uint64_t>(model_priced_count) ||
        (!model_priced_count && spent!=s.construction_spent_total) ||
        !original_finance_accounting_valid(s.treasury,s.taxes_collected_total,s.construction_spent_total,s.maintenance_spent_total) ||
        s.roads_removed_total>s.roads_placed_total || s.roads_placed_total-s.roads_removed_total!=s.roads.size() ||
        s.road_revision!=s.roads_placed_total+s.roads_removed_total+s.buildings.size()+2*removed.size()+2*retired_count+s.bridges.size()+s.ramps.size()+materialized)
        invalid("original production finances or placement revision mismatch");
    validate_original_hemp_snapshot(s);
    validate_original_food_snapshot(s);
    validate_original_services_snapshot(s);
    validate_original_water_snapshot(s);
    validate_original_housing_snapshot(s);
    if (safe) validate_retired_purchase_lifetimes(s);
    validate_original_fire_protection_snapshot(s);
    validate_original_finance_snapshot(s);
    validate_original_industry_snapshot(s);
    std::uint64_t hemp=0;for (const auto& farm:s.original_production->hemp_farms) hemp+=farm.harvested;
    for (std::size_t g=0;g<original::goods_count;++g) {
        const auto& totals=s.original_goods->totals[g];
        std::uint64_t luxury=0;
        for (const auto& producer:s.original_production->industry.sources) {
            const auto body=std::lower_bound(s.buildings.begin(),s.buildings.end(),producer.building_id,[](const auto& value,BuildingId id){return value.id<id;});
            if (body==s.buildings.end() || body->id!=producer.building_id) invalid("orphan paid industry source");
            if (const auto output=original_industry_goods::output(body->kind); output && static_cast<std::size_t>(*output)==g) {
                if (producer.completed>UINT64_MAX-luxury) invalid("luxury production sum overflow");
                luxury+=producer.completed;
            }
        }
        if (totals.imported || totals.demolished ||
            totals.produced!=(g==ceramics ? s.original_production->pottery_handed_off:g==0 ? hemp:luxury))
            invalid("unsupported original production goods origin");
    }
    if (s.original_production->pottery_handed_off>s.pottery_completed_total)
        invalid("Ceramics handoff exceeds actual completed Pottery");
    const GoodsContext owners(s.buildings,s.couriers);
    validate_original_goods_ledger({*s.original_goods,s.original_economy->houses,owners.owners(),owners.actors()});
    validate_original_consumption_snapshot(s);
    validate_original_market_snapshot(s);
    for (const auto& c:s.couriers) if (c.role!=CourierRole::Clay && c.role!=CourierRole::Pottery &&
        c.role!=CourierRole::MarketPotteryInbound && c.role!=CourierRole::MarketPotteryDistribution &&
        c.role!=CourierRole::OriginalHempToWarehouse && c.role!=CourierRole::OriginalFoodToMarket && c.role!=CourierRole::OriginalFoodToHouse &&
        c.role!=CourierRole::HealthWorker && c.role!=CourierRole::Service && c.role!=CourierRole::OriginalAncestor && c.role!=CourierRole::OriginalWater &&
        c.role!=CourierRole::OriginalDaoist && c.role!=CourierRole::OriginalBuddhist && c.role!=CourierRole::OriginalConfucian && c.role!=CourierRole::OriginalMusic &&
        c.role!=CourierRole::OriginalAcrobat && c.role!=CourierRole::OriginalDrama &&
        !(simulation::original_music_venues_enabled(s.profile,s.rule_version) && c.role==CourierRole::OriginalMusicPerformer) &&
        !(simulation::original_venue_house_service_enabled(s.profile,s.rule_version) && c.role==CourierRole::OriginalVenueMusic) &&
        c.role!=CourierRole::OriginalLuxuryToWarehouse && c.role!=CourierRole::OriginalAuxToWarehouse && c.role!=CourierRole::OriginalAuxSupplier &&
        !(simulation::original_fire_protection_enabled(s.profile,s.rule_version) && c.role==CourierRole::FireInspector))
        invalid("unsupported original production courier role");
}

bool World::original_production_finance_valid() const {
    if (!original_production_ || !original_influence_ || !original_economy_ || !original_goods_ ||
        !original_rules_model_ || demolition_history_!=DemolitionHistory{}) return false;
    try {
        const auto spent=spending(buildings_,bridges_,ramps_.size(),roads_placed_total_,original_influence_->removed_sources.size(),original_rules_model_.get())+retired_spending(*original_production_);
        return construction_spent_total_==spent && original_finance_history_valid();
    } catch (const std::invalid_argument&) {return false;}
}

std::array<OriginalGoodsBalance,original::goods_count> World::original_production_goods_balances() const {
    if (!original_goods_ || !original_economy_) throw std::logic_error("missing original production goods authority");
    const GoodsContext owners(buildings_,couriers_);
    validate_original_market_state();
    auto balances=inspect_original_goods_ledger({*original_goods_,original_economy_->houses,owners.owners(),owners.actors()});
    for (auto& balance:balances) balance.supported=true;return balances;
}

int World::original_ceramics_stock(BuildingId owner) const {return original_typed_stock(owner,original::Goods::Ceramics);}

int World::original_typed_stock(BuildingId owner,original::Goods good) const {
    if (original_house_kind(building(owner).kind)) {
        const auto& homes=original_economy_->houses;
        const auto found=std::lower_bound(homes.begin(),homes.end(),owner,
            [](const OriginalHouseState& home,BuildingId id){return home.building_id<id;});
        if (found==homes.end() || found->building_id!=owner) throw std::logic_error("missing original House stock");
        return original::goods_stock(found->house,good);
    }
    const auto& stocks=original_goods_->stocks;
    const auto found=std::lower_bound(stocks.begin(),stocks.end(),owner,
        [](const OriginalGoodsStock& row,BuildingId id) {return row.building_id<id;});
    if (found==stocks.end() || found->building_id!=owner) throw std::logic_error("missing typed Warehouse stock authority");
    return found->amounts[static_cast<std::size_t>(good)];
}

void World::receive_original_ceramics(BuildingId warehouse,int amount) {
    if (amount<=0 || static_cast<std::uint64_t>(amount)>UINT64_MAX-original_production_->pottery_handed_off)
        throw std::overflow_error("original Ceramics handoff exhausted");
    const GoodsContext owners(buildings_,couriers_);
    auto candidate=original_goods_produce({*original_goods_,original_economy_->houses,owners.owners(),owners.actors()},warehouse,
        original::Goods::Ceramics,amount);
    // All arithmetic, capacity and conservation checks precede the handoff.
    // HouseState is unchanged by the producer; only the typed Warehouse owns it.
    original_goods_=std::move(candidate.goods);
    original_production_->pottery_handed_off+=static_cast<std::uint64_t>(amount);
}

OriginalEliteMaterializationStatus World::original_elite_materialization_status(BuildingId id) const {
    OriginalEliteMaterializationStatus status;
    if (!original_production_enabled()) return status;
    const auto found=std::lower_bound(buildings_.begin(),buildings_.end(),id,
        [](const auto& b,BuildingId key){return b.id<key;});
    if (found==buildings_.end() || found->id!=id || !found->placed || found->kind!=Object::OriginalElitePlot)
        return status;
    status.pending=true;
    const auto blocked=[&](BuildBlocker blocker,Cell cell) {
        status.diagnostic={blocker,cell,{}};return status;
    };
    const auto& b=*found;
    if (original_cardinal_water_enabled() && !original_terrain_facts_->supports_cardinal_footprint(b.cell,4))
        return blocked(BuildBlocker::OutsideMap,b.cell);
    if (road_revision_==UINT64_MAX) return blocked(BuildBlocker::CounterExhausted,b.cell);
    // Guard untrusted bounds before adding any offsets. The geometry query
    // neither reserves cells nor advances a preparation/immigration phase.
    if (!in_bounds(b.cell))
        return blocked(BuildBlocker::OutsideMap,b.cell);
    const auto foundation=permissions_ ? permissions_->cell_height(b.cell):0;
    for (int y=0;y<4;++y) for (int x=0;x<4;++x) {
        const Cell cell{b.cell.x+x,b.cell.y+y};
        if (!in_bounds(cell)) return blocked(BuildBlocker::OutsideMap,cell);
        if (bridge_at(cell) || bridge_bank_at(cell)) return blocked(BuildBlocker::ProtectedBridge,cell);
        if (ramp_endpoint(cell)) return blocked(BuildBlocker::SandboxOccupied,cell);
        if (fixed_passage(cell)) return blocked(BuildBlocker::ProtectedGatePassage,cell);
        if (permissions_ && permissions_->fixed_gate(cell)) return blocked(BuildBlocker::GateSolidPart,cell);
        if (original_road(cell)) return blocked(BuildBlocker::ProtectedOriginalRoad,cell);
        if (permissions_ && permissions_->protected_original(cell)) return blocked(BuildBlocker::OriginalStructure,cell);
        if (!buildable(cell)) {
            const auto blocker=permissions_ ? permissions_->building_blocker(cell):BuildBlocker::LegacyBuildabilityRestriction;
            return blocked(blocker==BuildBlocker::None ? BuildBlocker::LegacyBuildabilityRestriction:blocker,cell);
        }
        if (permissions_ && permissions_->cell_height(cell)!=foundation)
            return blocked(BuildBlocker::UnsupportedHeightTransition,cell);
        const bool old=x<2 && y<2;
        if (old ? object_at(cell)!=Object::OriginalElitePlot || building_owner_at(cell)!=id:
                  object_at(cell)!=Object::Empty || building_owner_at(cell).has_value())
            return blocked(BuildBlocker::SandboxOccupied,cell);
        // An actor's already begun edge remains physical authority. This is
        // defense in depth even when its road/body already blocked the cell.
        for (const auto& actor:couriers_) {
            if (actor.phase==CourierPhase::IdleAtWorkshop) continue;
            if (actor.path_vertex>=actor.path.size()) return blocked(BuildBlocker::SandboxOccupied,cell);
            if (actor.path[actor.path_vertex]==cell ||
                (actor.edge_progress>0 && actor.path_vertex+1<actor.path.size() && actor.path[actor.path_vertex+1]==cell))
                return blocked(BuildBlocker::SandboxOccupied,cell);
        }
    }
    constexpr std::array<Cell,4> adjacent{{{1,0},{0,1},{-1,0},{0,-1}}};
    for (int y=0;y<4;++y) for (int x=0;x<4;++x) {
        const Cell cell{b.cell.x+x,b.cell.y+y};
        for (const auto delta:adjacent) {
            const Cell road{cell.x+delta.x,cell.y+delta.y};
            if (road_cell(road) && permission_edge_allowed(cell,road)) {
                status.ready=true;return status;
            }
        }
    }
    return blocked(BuildBlocker::InvalidEdge,b.cell);
}

void World::admit_original_residents() {
    bool geometry_changed=false;
    for (std::size_t i=0;i<original_economy_->houses.size();++i) {
        auto& home=original_economy_->houses[i];
        const auto capacity=original_rules_model_->house_rules().row_capacity(original::admission_capacity_row(home.house.type));
        const auto& b=building(home.building_id);
        const auto age=ticks_-b.placed_tick;
        if (age<120 || (age-120)%40 || home.house.occupants>=capacity || building_on_fire(b.id)) continue;
        const bool materialize=b.kind==Object::OriginalElitePlot;
        if (materialize ? !original_elite_materialization_status(b.id).ready:building_entrances(b.id).empty()) continue;
        const auto admitted=std::min(2,capacity-home.house.occupants);
        if (static_cast<std::uint64_t>(admitted)>UINT64_MAX-original_production_->residents_admitted)
            throw std::overflow_error("original admitted residents exhausted");
        if (!original_production_->housing.houses[i].first_admission_tick) {
            auto& history=original_production_->housing.houses[i];
            OriginalHousingEvaluation origin;origin.tick=ticks_;origin.input=home.house;
            // All prospective claims, actors, signed heights, entrances and
            // arithmetic passed above. Keep the existing identity, finance,
            // population/stock owners, timers and every active courier path.
            if (materialize) {
                auto& body=mutable_building(b.id);body.kind=Object::OriginalEliteHouse;
                for (int y=0;y<4;++y) for (int x=0;x<4;++x) {
                    const auto at=index({body.cell.x+x,body.cell.y+y});
                    objects_[at]=body.kind;owners_[at]=static_cast<std::uint32_t>(body.id);
                }
                ++road_revision_;geometry_changed=true;cached_route_.reset();
            }
            home.house.type=original_elite_house_kind(b.kind) ? original::HouseType::ModestSiheyuan:original::HouseType::Shelter;
            history.first_admission_tick=ticks_;
            history.first_transitions[original::model_row(original::house_row(home.house.type)).value]=
                OriginalHousingTransition{OriginalHousingTransitionKind::Admission,origin,static_cast<std::int16_t>(admitted)};
        }
        home.house.occupants=static_cast<std::int16_t>(home.house.occupants+admitted);
        home.original_status=3;home.access_enabled=true;
        original_production_->residents_admitted+=static_cast<std::uint64_t>(admitted);
    }
    // Candidate routes reflect the final new bodies once. This existing cache
    // refresh never rewrites a dispatched path, begun edge, cargo or reserve.
    if (geometry_changed) refresh_routes();
}

void World::tick_original_production() {
    const auto next=original::advance_scheduler_tick(ticks_);
    const auto dispatch=*original::next_scheduler_dispatch(ticks_);
    if (next>UINT64_MAX-Rules::fire_incident_ticks) throw std::overflow_error("original production Fire clock exhausted");
    if (original_fire_protection_enabled()) preflight_original_fire_tick();
    book_original_finance_tick(next);
    capture_tick_staffing();ticks_=next;
    // Staffing belongs to the pre-tick population. Immigration cannot staff a
    // producer until the next ordinary tick. No 400t legacy demand is run.
    advance_clay_pottery_production();advance_original_hemp();advance_original_food();update_original_industry();
    for (auto& c:couriers_) dispatch_v2(c);
    for (auto& c:couriers_) move_v2(c);
    update_fire();admit_original_residents();tick_staffing_active_=false;
    if (dispatch.position.phase==28) evaluate_original_water();
    if (dispatch.service_countdown) {decay_original_services();decay_original_water();decay_original_music_venues();}
    if (dispatch.evolution) evaluate_original_houses();
    if (dispatch.consumption) {consume_original_house_goods();consume_original_food();}
    validate_original_food_state();validate_original_industry_state();
    validate_original_services_state();validate_original_water_state();validate_original_housing_state();validate_original_music_venues_state();validate_original_venue_house_service_state();
    const auto balances=original_production_goods_balances();
    if (!population_valid() || !fire_state_valid() || !production_balance_valid() || !navigation_valid() ||
        !city_economy_valid() || !std::all_of(balances.begin(),balances.end(),[](const auto& b){return b.conserved;}))
        throw std::logic_error("original production, transport, population or finance invariant violated");
}

} // namespace openemperor::simulation
