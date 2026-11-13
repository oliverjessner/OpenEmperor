// Paid Rule25 behavior. Every map/model input is independently authored;
// all residents, workers, growth, harvests and goods arise through World.
#include "PlantationsFixture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <set>

namespace {
namespace q=oe_plantations_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using q::check;

int stock(const sim::World& world,sim::BuildingId owner,orig::Goods good) {
    for(const auto& row:world.original_goods_state()->stocks)
        if(row.building_id==owner)return row.amounts[static_cast<std::size_t>(good)];
    return 0;
}
void pause_other_industry(sim::World& world,sim::BuildingId kept) {
    std::vector<sim::BuildingId> ids;
    for(const auto& row:world.original_production_state()->industry.sources)
        if(row.building_id!=kept)ids.push_back(row.building_id);
    for(const auto id:ids)f::buy(world,sim::set_building_operation(id,false));
}
bool moving(const sim::World& world,sim::BuildingId source,bool picked) {
    const auto& row=q::source(world,source);
    if(!row.latest_trip || row.latest_trip->returned_tick ||
       row.latest_trip->picked_tick.has_value()!=picked)return false;
    const auto& actor=world.courier(row.courier_id);
    return actor.phase!=sim::CourierPhase::IdleAtWorkshop && actor.edge_progress>0;
}
void actual_path(const sim::World& world,sim::CourierId id) {
    const auto& actor=world.courier(id);
    check(actor.role==sim::CourierRole::OriginalPlantationGrower && actor.path.size()>=2,
          "Paid plantation grower lacks its actual shared movement path");
    for(std::size_t i=1;i<actor.path.size();++i)
        check(std::abs(actor.path[i].x-actor.path[i-1].x)+
              std::abs(actor.path[i].y-actor.path[i-1].y)==1,
              "Plantation grower skipped a physical cardinal land edge");
    const auto position=world.courier_position(id);
    check(position && std::isfinite(position->x) && std::isfinite(position->y),
          "Actual moving plantation worker has no finite physical position");
}

void paid_geometry_binding_and_limits() {
    q::Fixture fixture;auto world=fixture.world();
    check(world.rule_version()==sim::kOriginalPlantationsRuleVersion && world.plantations_state() &&
          world.plantations_state()->plants.empty() && world.plantations_state()->sources.empty(),
          "Explicit plantation factory did not create its own empty generation");
    q::denied(world,{sim::CommandType::PlaceOriginalTeaBush,{30,30}});
    const auto before=world.treasury();
    const auto tea=q::purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,{30,30});
    check(world.treasury()==before-40 && world.workforce_required(tea)==12 &&
          sim::building_footprint(world.profile(),world.rule_version(),world.building(tea).kind)==sim::BuildingFootprint{2,2},
          "Paid plantation source lost its independent model bill, staff or full footprint");
    sim::Command field{sim::CommandType::PlaceOriginalTeaBush,{34,34}};field.building_id=tea;
    auto invalid=field;invalid.building_id=static_cast<sim::BuildingId>(999999);q::denied(world,invalid);
    invalid=field;invalid.type=sim::CommandType::PlaceOriginalLacquerTree;q::denied(world,invalid);
    invalid=field;invalid.cell={35,34};q::denied(world,invalid);
    invalid=field;invalid.cell={26,27};q::denied(world,invalid);
    invalid=field;invalid.cell={31,31};q::denied(world,invalid);
    const auto funds=world.treasury();f::buy(world,field);const auto first=*world.building_owner_at(field.cell);
    const auto second=q::plant(world,sim::CommandType::PlaceOriginalTeaBush,tea,{27,27});
    check(first!=second && world.treasury()==funds-12 &&
          q::plant(world,first).source_id==tea && q::plant(world,second).source_id==tea &&
          sim::building_footprint(world.profile(),world.rule_version(),world.building(first).kind)==sim::BuildingFootprint{1,1},
          "Separately paid range-boundary plants were collapsed or misbound");
    q::denied(world,sim::demolish_building(tea));q::denied(world,sim::demolish_building(first));
    (void)q::purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,{37,30});
    invalid=field;invalid.cell={34,33};invalid.building_id=static_cast<sim::BuildingId>(0);
    q::denied(world,invalid); // Two actual matching sources require a choice.
    q::denied(world,{sim::CommandType::PlaceOriginalTeaCuringShed,{90,30}});
    for(int y=27;y<=34 && world.plantations_state()->plants.size()<sim::plantation_plant_limit;++y)
        for(int x=27;x<=34 && world.plantations_state()->plants.size()<sim::plantation_plant_limit;++x) {
            field.cell={x,y};if(world.validate(field).accepted)f::buy(world,field);
        }
    check(world.plantations_state()->plants.size()==32,
          "Could not exercise the bounded32 distinct paid plants");
    bool capacity_refusal=false;
    for(int y=27;y<=34 && !capacity_refusal;++y)for(int x=27;x<=34 && !capacity_refusal;++x)
        if(world.object_at({x,y})==sim::Object::Empty) {
            field.cell={x,y};q::denied(world,field);capacity_refusal=true;
        }
    check(capacity_refusal,"Plant capacity test had no legal empty range cell");
    q::ticks(world,200);
    for(const auto& plant:world.plantations_state()->plants)
        check(!plant.growth && !plant.grown && !plant.harvested && !plant.returned,
              "No-worker/no-road plantation generated growth or a harvest");
}

void finite_growth_real_pick_return_and_regrowth() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    q::until(world,[&]{const auto& plant=q::plant(world,scene.tea_plant);return plant.growth>0 && plant.growth<80;},
             "Actual paid residents never staffed a growing Tea plant");
    check(world.building_staffed(scene.tea_source) &&
          !q::industry(world,scene.tea_source).completed && !stock(world,scene.tea_source,orig::Goods::Tea),
          "Growing plant credited Tea before an actual harvested return");
    q::until(world,[&]{return q::plant(world,scene.tea_plant).growth==80;},
             "Tea plant never reached one finite ready unit");
    const auto ready=q::plant(world,scene.tea_plant);
    check(ready.grown==1 && !ready.harvested && !ready.returned &&
          !world.original_goods_balance(orig::Goods::Tea).totals.produced,
          "Readiness invented source stock or skipped the first paid harvest");
    q::until(world,[&]{return moving(world,scene.tea_source,false);},
             "Ready paid Tea never dispatched an actual outward grower");
    const auto channel=q::source(world,scene.tea_source).courier_id;
    const auto first_grower=q::source(world,scene.tea_source).latest_trip->grower_id;
    actual_path(world,channel);
    check(!world.original_courier_cargo(channel) && q::plant(world,scene.tea_plant).growth==80 &&
          !world.original_goods_balance(orig::Goods::Tea).totals.produced,
          "An outward grower loaded cargo or credited output before plant arrival");
    q::until(world,[&]{return moving(world,scene.tea_source,true);},
             "Grower never physically picked the finite ready Tea unit");
    actual_path(world,channel);
    check(q::plant(world,scene.tea_plant).harvested==1 && q::plant(world,scene.tea_plant).growth<80 &&
          world.original_courier_cargo(channel)==1 && !q::plant(world,scene.tea_plant).returned &&
          !q::industry(world,scene.tea_source).completed &&
          !world.original_goods_balance(orig::Goods::Tea).totals.produced,
          "Picked Tea did not remain the sole real worker cargo until source return");
    q::denied(world,sim::demolish_building(scene.tea_plant));
    q::denied(world,sim::demolish_building(scene.tea_source));
    q::until(world,[&]{return q::plant(world,scene.tea_plant).returned==1;},
             "Picked Tea never reached its actual paid source");
    const auto returned=q::plant(world,scene.tea_plant);
    check(returned.last_receipt && returned.last_receipt->grower_id==first_grower &&
          returned.last_receipt->picked_tick<returned.last_receipt->returned_tick &&
          q::industry(world,scene.tea_source).completed==1 &&
          world.original_goods_balance(orig::Goods::Tea).totals.produced==1,
          "Source did not credit exactly one returned Tea unit with actual arrival evidence");
    q::until(world,[&]{return q::plant(world,scene.tea_plant).returned>=2;},
             "Retained Tea plant never regrew and completed a second real harvest");
    check(world.building(scene.tea_plant).kind==sim::Object::OriginalTeaBush26 &&
          q::plant(world,scene.tea_plant).last_receipt->grower_id!=first_grower &&
          q::source(world,scene.tea_source).courier_id==channel,
          "Regrowth repurchased a plant, reused grower lifetime identity or replaced its movement channel");
    q::valid(world);
}

void pause_roads_full_stocks_and_natural_fire() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    pause_other_industry(world,scene.tea_source);
    q::until(world,[&]{return q::plant(world,scene.tea_plant).growth>=10;},
             "Paid pause scenario never earned productive plant work");
    f::buy(world,sim::set_building_operation(scene.tea_source,false));
    const auto paused=q::plant(world,scene.tea_plant);q::ticks(world,180);
    check(q::plant(world,scene.tea_plant).growth==paused.growth &&
          q::plant(world,scene.tea_plant).productive_ticks==paused.productive_ticks,
          "Paused plantation advanced plant work");
    f::buy(world,sim::set_building_operation(scene.tea_source,true));
    q::until(world,[&]{return q::plant(world,scene.tea_plant).returned>0;},
             "Resumed plantation never completed its preserved actual cycle");
    const auto entrances=world.building_entrances(scene.tea_source);
    std::vector<sim::Cell> removed;
    for(const auto& entrance:entrances) {
        // Both road rows touch the shed; duplicate entrance cells are harmless
        // only when removed once through the real command path.
        const auto cell=entrance.road_cell;
        if(std::find(removed.begin(),removed.end(),cell)==removed.end()) {
            f::buy(world,{sim::CommandType::RemoveRoad,cell});removed.push_back(cell);
        }
    }
    check(!removed.empty() && world.building_entrances(scene.tea_source).empty(),
          "Road-cut scenario retained a real source entrance");
    const auto disconnected=q::plant(world,scene.tea_plant);q::ticks(world,180);
    check(q::plant(world,scene.tea_plant).productive_ticks==disconnected.productive_ticks,
          "Disconnected plantation grew without its required actual road access");
    for(const auto cell:removed)f::buy(world,{sim::CommandType::PlaceRoad,cell});
    f::buy(world,sim::set_building_operation(scene.market,false));
    f::buy(world,sim::set_building_operation(scene.warehouse,false));
    q::until(world,[&]{return stock(world,scene.tea_source,orig::Goods::Tea)==8 &&
                                  q::plant(world,scene.tea_plant).growth==80 &&
                                  world.courier(q::source(world,scene.tea_source).courier_id).phase==sim::CourierPhase::IdleAtWorkshop;},
             "Actual paid Tea failed to fill bounded output storage and retain a ready plant");
    const auto full=q::plant(world,scene.tea_plant);const auto completed=q::industry(world,scene.tea_source).completed;
    q::ticks(world,240);
    check(q::plant(world,scene.tea_plant).growth==80 && q::plant(world,scene.tea_plant).grown==full.grown &&
          q::plant(world,scene.tea_plant).harvested==full.harvested &&
          q::industry(world,scene.tea_source).completed==completed,
          "Full storage discarded a ready plant, duplicated harvest or exceeded source capacity");
    f::buy(world,sim::set_building_operation(scene.warehouse,true));
    f::buy(world,sim::set_building_operation(scene.market,true));
    q::until(world,[&]{return q::industry(world,scene.tea_source).completed>completed;},
             "Unblocked physical goods storage never resumed the retained harvest");

    auto burning=fixture.world();const auto fire_scene=q::build(burning);
    for(const auto id:fire_scene.watches)f::buy(burning,sim::set_building_operation(id,false));
    q::until(burning,[&]{return burning.building_on_fire(fire_scene.tea_source);},
             "Paid plantation never reached a genuine natural source fire",12000);
    const auto stopped=q::plant(burning,fire_scene.tea_plant).productive_ticks;
    bool observed=false;
    for(int tick=0;tick<20 && burning.building_on_fire(fire_scene.tea_source);++tick) {
        burning.tick();
        if(burning.building_on_fire(fire_scene.tea_source)) {
            check(q::plant(burning,fire_scene.tea_plant).productive_ticks==stopped,
                  "Burning source advanced plantation growth");observed=true;
        }
    }
    check(observed,"Natural fire test did not observe an actual stopped production interval");q::valid(burning);
}

void loaded_output_road_cut_and_three_real_house_consumers() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    constexpr std::array goods{orig::Goods::Tea,orig::Goods::Silk,orig::Goods::Lacquerware};
    std::array<bool,goods.size()> actual_house_arrivals{};
    const auto observe_arrivals=[&] {
        // The production ledger intentionally retains only each courier's
        // latest transaction. Observe actual completed events before the next
        // good/target replaces that bounded row; do not require an unbounded
        // production history or infer an arrival from stock/dispatch.
        for(const auto& row:world.original_goods_state()->reservations) {
            if(row.source!=scene.market || row.phase!=sim::OriginalGoodsTransferPhase::Completed ||
               row.delivered<=0 || std::find(scene.homes.begin(),scene.homes.end(),row.target)==scene.homes.end())continue;
            const auto& actor=world.courier(row.transfer.courier_id);
            check(actor.role==sim::CourierRole::MarketPotteryDistribution && actor.owner==scene.market &&
                  actor.target==row.target,"Observed luxury arrival has no actual Market-to-House courier");
            for(std::size_t i=0;i<goods.size();++i)if(row.good==goods[i])actual_house_arrivals[i]=true;
        }
    };
    const auto observed_until=[&](auto done,const char* reason,int limit=16000) {
        for(int tick=0;tick<limit && !done();++tick) {world.tick();observe_arrivals();}
        check(done(),reason);q::valid(world);
    };
    sim::CourierId output{};
    observed_until([&]{
        for(const auto& actor:world.couriers())if(actor.owner==scene.tea_source &&
           actor.role==sim::CourierRole::OriginalLuxuryToWarehouse &&
           world.original_courier_cargo(actor.id)>0 && actor.edge_progress>0 &&
           actor.path.size()>actor.path_vertex+2) {output=actor.id;return true;}
        return false;
    },"Actual Tea never reached a loaded physical output courier");
    const auto actor=world.courier(output);const auto cut=actor.path[actor.path_vertex+2];
    sim::OriginalGoodsCargo loaded;bool found=false;
    for(const auto& row:world.original_goods_state()->cargo)if(row.transfer.courier_id==output) {loaded=row;found=true;}
    check(found && loaded.good==orig::Goods::Tea && loaded.amount>0,
          "Moving output courier had no sole typed Tea cargo");
    f::buy(world,{sim::CommandType::RemoveRoad,cut});
    check(world.courier(output).edge_progress==actor.edge_progress,
          "Road removal rewound the output courier's begun physical edge");
    for(int tick=0;tick<80;++tick) {world.tick();observe_arrivals();}
    // Existing road repair/return behavior owns this cargo; the cut never
    // becomes a grant, deletion or a substitute arrival in the new plant state.
    q::valid(world);f::buy(world,{sim::CommandType::PlaceRoad,cut});
    observed_until([&]{
        for(const auto& row:world.original_goods_state()->reservations)
            if(row.transfer==loaded.transfer && (row.phase==sim::OriginalGoodsTransferPhase::Completed ||
                                                row.phase==sim::OriginalGoodsTransferPhase::Returned))
                return row.delivered+row.returned==loaded.amount;
        return false;
    },"Repaired output trip failed to deliver or return exactly its retained Tea cargo");
    observed_until([&]{
        return world.original_goods_balance(orig::Goods::Tea).totals.consumed>0 &&
               world.original_goods_balance(orig::Goods::Silk).totals.consumed>0 &&
               world.original_goods_balance(orig::Goods::Lacquerware).totals.consumed>0;
    },"Three paid plantation chains failed to reach actual House consumption");
    for(std::size_t i=0;i<goods.size();++i) {
        const auto good=goods[i];bool consumed=false;
        for(const auto& home:world.original_production_state()->housing.houses)
            for(const auto& row:home.consumption_rows)
                consumed|=row.consumed[static_cast<std::size_t>(good)]>0;
        check(actual_house_arrivals[i] && consumed,
              "Luxury stock or dispatch was substituted for actual House arrival and consumption");
    }
    const auto& lacquer=q::industry(world,scene.lacquerware);
    const auto& silk=q::industry(world,scene.weaver);
    check(lacquer.completed>0 && lacquer.aux_inputs_used[static_cast<std::size_t>(sim::OriginalAuxMaterial::Lacquer)]==lacquer.completed &&
          lacquer.aux_inputs_used[static_cast<std::size_t>(sim::OriginalAuxMaterial::Wood)]==lacquer.completed &&
          silk.completed>0 && silk.aux_inputs_used[static_cast<std::size_t>(sim::OriginalAuxMaterial::RawSilk)]==silk.completed,
          "Downstream recipes bypassed genuine returned Lacquer/Wood/RawSilk inputs");
    std::set<sim::CourierId> channels;
    for(const auto id:{scene.tea_source,scene.lacquer_source,scene.mulberry_source}) {
        const auto& source=q::source(world,id);channels.insert(source.courier_id);
        check(source.spawned>0 && q::industry(world,id).completed>0,
              "One of the three paid plantation kinds never completed real harvest returns");
    }
    check(channels.size()==3 && orig::goods_count==6 && sim::original_aux_material_count==5,
          "Plantations shared a worker channel or widened the historical goods/material dimensions");
    q::valid(world);
}

void historical_rule24_keeps_its_own_plant_contract() {
    q::Fixture fixture;
    auto legacy=sim::World::create_original_agriculture(fixture.policy,fixture.model,fixture.terrain);
    const auto source=q::purchase(legacy,sim::CommandType::PlaceOriginalTeaCuringShed,{30,30});
    (void)q::plant(legacy,sim::CommandType::PlaceOriginalTeaBush,source,{32,30});
    sim::Command additional{sim::CommandType::PlaceOriginalTeaBush,{30,32}};additional.building_id=source;
    q::denied(legacy,additional);
    check(legacy.rule_version()==sim::kOriginalAgricultureRuleVersion && !legacy.plantations_state() &&
          legacy.original_production_state()->industry.sources.front().plant_id.has_value(),
          "Rule25 changed historical Rule24's single adjacent plant or mutable state");
}

void foreign_plant_cannot_occupy_a_real_grower_edge() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    const auto foreign=q::purchase(world,sim::CommandType::PlaceOriginalTeaCuringShed,{24,31});
    const auto origin=world.building(foreign).cell;
    bool saw_current=false,saw_begun_next=false;
    for(int tick=0;tick<5000 && !(saw_current && saw_begun_next);++tick) {
        world.tick();const auto& source=q::source(world,scene.tea_source);
        const auto& actor=world.courier(source.courier_id);
        if(actor.phase!=sim::CourierPhase::ToWarehouse || actor.edge_progress<=0 ||
           actor.path_vertex+1>=actor.path.size())continue;
        const std::array candidates{actor.path[actor.path_vertex],actor.path[actor.path_vertex+1]};
        for(std::size_t side=0;side<candidates.size();++side) {
            if(side==0 ? saw_current:saw_begun_next)continue;
            const auto cell=candidates[side];
            if(world.object_at(cell)!=sim::Object::Empty || cell.x<origin.x-3 || cell.x>origin.x+4 ||
               cell.y<origin.y-3 || cell.y>origin.y+4)continue;
            sim::Command placement{sim::CommandType::PlaceOriginalTeaBush,cell};placement.building_id=foreign;
            q::denied(world,placement);
            if(side==0)saw_current=true;else saw_begun_next=true;
        }
    }
    check(saw_current && saw_begun_next,
          "Foreign-plant regression did not observe both an actual worker cell and begun next land cell");
    q::until(world,[&]{return q::plant(world,scene.tea_plant).returned>0;},
             "Rejected foreign plants damaged the genuine grower's retained route or harvest return");
    q::valid(world);
}
} // namespace

int main() {
    try {
        paid_geometry_binding_and_limits();finite_growth_real_pick_return_and_regrowth();
        pause_roads_full_stocks_and_natural_fire();loaded_output_road_cut_and_three_real_house_consumers();
        historical_rule24_keeps_its_own_plant_contract();foreign_plant_cannot_occupy_a_real_grower_edge();
        std::cout<<"Paid plantations: finite growth, actual grower pickup/return, blocked storage, three House consumers and historical boundary PASS\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Plantation World FAIL: "<<error.what()<<'\n';return 1;
    }
}
