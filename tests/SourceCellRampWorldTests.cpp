#include "simulation/World.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace openemperor::simulation;
namespace {
void check(bool okay,const char* message) {if(!okay)throw std::runtime_error(message);}
Cell add(Cell a,Cell d,int scale=1) {return {a.x+d.x*scale,a.y+d.y*scale};}
std::shared_ptr<const MapPermissions> step_policy(Cell source={8,5},Cell direction={1,0},int base=0,
                                                 int width=32,int height=24,bool bridge=false) {
    std::vector<MapCellPermission> cells(std::size_t(width*height),
        {true,true,false,0,BuildBlocker::None,BuildBlocker::None});
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        auto& cell=cells[std::size_t(y*width+x)];
        cell.height=std::int8_t(base+((x-source.x)*direction.x+(y-source.y)*direction.y>0?1:0));
        if(bridge&&x==11) {
            cell.road_allowed=cell.building_allowed=false;cell.bridge_water=true;
            cell.road_blocker=BuildBlocker::UnsupportedTerrain;
            cell.building_blocker=BuildBlocker::LegacyBuildabilityRestriction;
        }
    }
    return std::make_shared<const MapPermissions>(width,height,3,std::move(cells),std::vector<FixedGatePassage>{});
}
void road(World& world,Cell cell) {check(world.execute({CommandType::PlaceRoad,cell}).accepted,"paid road refused");}
void three_roads(World& world,Cell source,Cell direction) {
    for(int step=-1;step<=1;++step)road(world,add(source,direction,step));
}
void refused(World& world,Command command,BuildBlocker expected) {
    const auto snapshot=world.snapshot();const auto canonical=world.canonical_state();
    const auto refreshes=world.route_refresh_count();
    const auto preview=world.validate(command);
    check(!preview.accepted&&preview.diagnostic.blocker==expected,"negative preview blocker changed");
    const auto result=world.execute(command);
    check(!result.accepted&&!result.changed&&result.diagnostic.blocker==expected,"negative command accepted or wrong blocker");
    check(world.snapshot()==snapshot&&world.canonical_state()==canonical&&world.route_refresh_count()==refreshes,
        "refused command mutated snapshot, derived routes, counters or funds");
}
void assert_balances(const World& world) {
    check(world.navigation_valid()&&world.production_balance_valid()&&world.city_economy_valid()&&
        world.population_valid()&&world.service_state_valid(),"paid source-cell World invariant failed");
}
World paid_city(const std::shared_ptr<const MapPermissions>& policy,bool bridge) {
    auto world=World::create_authored_source_cell_ramps(policy);
    for(const Command command:std::array<Command,4>{{
        {CommandType::PlaceHousehold,{1,1}},{CommandType::PlaceHousehold,{1,3}},
        {CommandType::PlaceClaySource,{4,2}},{CommandType::PlacePottery,{14,2}}}})
        check(world.execute(command).accepted,"ordinary paid city purchase failed");
    for(int x=4;x<=15;++x)if(!bridge||x!=11)road(world,{x,5});
    road(world,{5,4});road(world,{14,4});
    if(bridge)check(world.execute(place_bridge({10,5},{12,5})).accepted,"paid inherited Bridge failed");
    return world;
}
void paid_transport_and_repair(bool bridge) {
    const auto policy=step_policy({8,5},{1,0},0,32,24,bridge);
    auto world=paid_city(policy,bridge);
    const auto source=static_cast<BuildingId>(3),target=static_cast<BuildingId>(4);
    check(!world.find_building_route(source,target),"unbuilt source-cell step granted transport");
    const auto original_policy=policy->canonical_state();
    const auto before=world.snapshot();const auto refreshes=world.route_refresh_count();
    check(world.execute(place_source_cell_ramp({8,5},{9,5})).accepted,"paid productive source-cell ramp failed");
    check(world.treasury()==before.treasury-40&&world.command_sequence()==before.command_sequence+1&&
        world.road_revision()==before.road_revision+1&&world.construction_spent_total()==before.construction_spent_total+40&&
        world.route_refresh_count()==refreshes+1,"source-cell purchase cost/revision/refresh not exactly once");
    check(policy->canonical_state()==original_policy&&world.find_building_route(source,target).has_value(),
        "source-cell purchase mutated immutable policy or failed shared navigation");
    bool cut=false,waited=false,delivered=false,returned=false,mid_out=false,mid_back=false;
    for(int tick=0;tick<600;++tick) {
        world.tick();assert_balances(world);
        const auto actor=world.couriers().front();
        if(actor.path_vertex+1<actor.path.size()&&actor.edge_progress>0) {
            const auto from=actor.path[actor.path_vertex],to=actor.path[actor.path_vertex+1];
            if(world.source_cell_ramp_source_at(from)||world.source_cell_ramp_source_at(to)) {
                const auto position=*world.courier_position(actor.id);
                const auto height=world.source_cell_ramp_surface_height(position);
                check(height&&std::abs(*height-std::clamp(position.x-7.5,0.,1.))<1e-12,
                    "actual courier foot did not follow flat/slope/flat source-cell surface");
                mid_out|=actor.phase==CourierPhase::ToWarehouse&&actor.cargo>0&&actor.reserved==actor.cargo;
                mid_back|=actor.phase==CourierPhase::Returning&&actor.cargo==0&&actor.reserved==0;
                check(World::restore(world.snapshot(),policy).snapshot()==world.snapshot(),"mid-source-cell restore changed authority");
                if(!cut&&actor.phase==CourierPhase::ToWarehouse&&from==Cell{8,5}&&to==Cell{9,5}) {
                    // Both cells of the real begun edge remain protected.
                    const auto stable=world.snapshot();
                    check(!world.execute({CommandType::RemoveRoad,from}).accepted&&
                        !world.execute({CommandType::RemoveRoad,to}).accepted&&world.snapshot()==stable,
                        "occupied source-cell begun edge could be removed");
                    const auto cargo=actor.cargo,reservation=world.building(target).reserved_incoming;
                    const auto position_before=world.courier_position(actor.id);
                    check(world.execute({CommandType::RemoveRoad,{13,5}}).accepted,"future loaded road cut failed");
                    const auto pending=world.courier(actor.id);
                    check(pending.route_pending&&pending.edge_progress==actor.edge_progress&&
                        pending.path==std::vector<Cell>{from,to}&&pending.cargo==cargo&&pending.reserved==actor.reserved&&
                        world.building(target).reserved_incoming==reservation&&world.courier_position(actor.id)==position_before,
                        "road cut changed begun edge, cargo, reservation or foot position");
                    check(World::restore(world.snapshot(),policy).snapshot()==world.snapshot(),"mid-edge cut was not restorable");
                    cut=true;
                }
            }
        }
        const auto current=world.couriers().front();
        if(cut&&!waited&&current.route_pending&&current.edge_progress==0) {
            const auto position=world.courier_position(current.id);
            const auto cargo=current.cargo,reserved=current.reserved;
            for(int wait=0;wait<10;++wait)world.tick();
            check(world.courier_position(current.id)==position&&world.courier(current.id).cargo==cargo&&
                world.courier(current.id).reserved==reserved&&cargo>0,
                "disconnected source-cell courier teleported or lost paid cargo");
            check(World::restore(world.snapshot(),policy).snapshot()==world.snapshot(),"disconnected wait restore changed state");
            const auto funds=world.treasury();const auto revision=world.road_revision();
            check(world.execute({CommandType::PlaceRoad,{13,5}}).accepted&&world.treasury()==funds-2&&
                world.road_revision()==revision+1,"normal paid road repair failed");
            waited=true;
        }
        const auto after=world.couriers().front();
        delivered|=waited&&after.phase==CourierPhase::Returning&&after.cargo==0&&
            (world.building(target).input_clay>0||world.building(target).active_recipe_clay>0||world.pottery_completed_total()>0);
        returned|=delivered&&after.phase==CourierPhase::IdleAtWorkshop;
    }
    check(cut&&waited&&delivered&&returned&&mid_out&&mid_back&&world.pottery_completed_total()>0&&
        world.maintenance_spent_total()>0,"paid crossing, cut/wait/repair, arrival, recipe, return or upkeep missing");
    auto reload=World::restore(world.snapshot(),policy);
    for(int tick=0;tick<50;++tick){world.tick();reload.tick();}
    check(world.snapshot()==reload.snapshot()&&world.canonical_state()==reload.canonical_state(),
        "continued complete source-cell state differs after independent restore");
}
void surface_and_legacy_controls() {
    check(current_rule_version(RulesProfile::CityV16)==4&&current_rule_version(RulesProfile::CityV11)==3,
        "historical defaults changed");
    check(source_cell_ramps_enabled(RulesProfile::CityV16,20)&&!source_cell_ramps_enabled(RulesProfile::CityV16,19)&&
        !source_cell_ramps_enabled(RulesProfile::CityV11,20)&&!original_economy_enabled(RulesProfile::CityV16,20)&&
        !original_safe_demolition_enabled(RulesProfile::CityV16,20),"Rule20 inherited original economy or old opt-ins");
    const auto classic=profile_rules(RulesProfile::CityV16,6),selected=profile_rules(RulesProfile::CityV16,20);
    check(selected.version==20&&selected.courier_edge_ticks==5&&selected.clay_ticks==classic.clay_ticks&&
        selected.pottery_recipe_ticks==classic.pottery_recipe_ticks&&selected.farm_ticks==classic.farm_ticks&&
        selected.household_move_in_grace_ticks==classic.household_move_in_grace_ticks,"Rule20 classic economy/timing changed");
    constexpr Cell source{10,10};
    for(const Cell direction:std::array<Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}})
        for(const int base:{-128,-2,0,126}) {
            const auto policy=step_policy(source,direction,base);
            auto world=World::create_authored_source_cell_ramps(policy);three_roads(world,source,direction);
            check(world.execute(place_source_cell_ramp(source,add(source,direction))).accepted,"signed cardinal source-cell purchase failed");
            check(world.source_cell_ramp_source_at(source)&&!world.source_cell_ramp_source_at(add(source,direction))&&
                world.source_cell_ramp_claim(add(source,direction,-1))&&world.source_cell_ramp_claim(add(source,direction)),
                "derived source/three-claim index mismatch");
            for(int sample=0;sample<=20;++sample) {
                const double along=-1+sample*.1;
                const Position position{source.x+direction.x*along,source.y+direction.y*along};
                const auto height=world.source_cell_ramp_surface_height(position);
                check(height&&std::abs(*height-(base+std::clamp(along+.5,0.,1.)))<1e-12,
                    "signed cardinal flat/slope/flat surface discontinuity");
            }
            const Cell lateral{-direction.y,direction.x};road(world,add(source,lateral));
            check(!world.transport_edge_allowed(source,add(source,lateral))&&
                world.transport_edge_allowed(source,add(source,direction))&&
                world.transport_edge_allowed(source,add(source,direction,-1)),"source-cell axis restriction failed");
            check(!world.source_cell_ramp_surface_height({source.x+lateral.x*.1,source.y+lateral.y*.1})&&
                !world.source_cell_ramp_surface_height({std::numeric_limits<double>::infinity(),0}),
                "surface helper accepted lateral or nonfinite actor position");
            const auto saved=world.snapshot();check(World::restore(saved,policy).snapshot()==saved,"signed source-cell restore failed");
        }
    for(const auto version:{5U,6U}) {
        const auto policy=step_policy();World world(policy,RulesProfile::CityV16,version);three_roads(world,{8,5},{1,0});
        refused(world,place_source_cell_ramp({8,5},{9,5}),BuildBlocker::UnsupportedTerrain);
        check(world.transport_edge_blocker({8,5},{9,5})==BuildBlocker::UnsupportedHeightTransition,"historical height edge opened");
        if(version==6)check(world.execute(place_ramp({8,5},{9,5})).accepted,"historical paid RampLink changed");
    }
}
void negative_geometry_and_restore() {
    constexpr Cell source{8,5},high{9,5};const auto policy=step_policy();
    auto world=World::create_authored_source_cell_ramps(policy);three_roads(world,source,{1,0});
    for(const auto ends:std::array<std::pair<Cell,Cell>,6>{{{source,source},{source,{9,6}},{source,{10,5}},
        {{-1,5},{0,5}},{source,{INT32_MAX,5}},{source,{7,5}}}}) {
        const auto before=world.snapshot();check(!world.execute(place_source_cell_ramp(ends.first,ends.second)).accepted&&
            world.snapshot()==before,"invalid direction/bounds/multilevel mutated World");
    }
    for(int negative=0;negative<7;++negative) {
        auto cells=policy->cells();auto& neighbor=cells[4U*32+9U];
        if(negative==0)neighbor.height=0;
        if(negative==1)neighbor.height=2;
        if(negative==2){neighbor.road_allowed=neighbor.building_allowed=false;neighbor.protected_original=true;
            neighbor.road_blocker=neighbor.building_blocker=BuildBlocker::OriginalStructure;}
        if(negative==3){neighbor.road_allowed=neighbor.building_allowed=false;neighbor.bridge_water=true;
            neighbor.road_blocker=BuildBlocker::UnsupportedTerrain;}
        if(negative==4){neighbor.road_allowed=neighbor.building_allowed=false;neighbor.road_blocker=BuildBlocker::OutsideMap;}
        if(negative==5){neighbor.road_allowed=false;neighbor.road_blocker=BuildBlocker::UnsupportedTerrain;}
        if(negative==6)cells[5U*32+7U].height=-1;
        auto changed=std::make_shared<const MapPermissions>(32,24,3,std::move(cells),std::vector<FixedGatePassage>{});
        auto candidate=World::create_authored_source_cell_ramps(changed);three_roads(candidate,source,{1,0});
        const auto stable=candidate.snapshot();check(!candidate.execute(place_source_cell_ramp(source,high)).accepted&&
            candidate.snapshot()==stable,"unsupported neighborhood protection/height accepted");
    }
    check(world.execute({CommandType::RemoveRoad,{7,5}}).accepted,"missing low road setup failed");
    refused(world,place_source_cell_ramp(source,high),BuildBlocker::BridgeBankRoadRequired);road(world,{7,5});
    auto counter=world.snapshot();counter.command_sequence=UINT64_MAX-1;
    auto exhausted=World::restore(counter,policy);
    refused(exhausted,place_source_cell_ramp(source,high),BuildBlocker::CounterExhausted);
    check(world.execute(place_source_cell_ramp(source,high)).accepted,"restore fixture ramp failed");
    refused(world,place_source_cell_ramp(source,high),BuildBlocker::SandboxOccupied);
    refused(world,place_ramp(source,high),BuildBlocker::SandboxOccupied);
    const auto stable=world.snapshot();
    for(int variant=0;variant<20;++variant) {
        auto bad=stable;auto& ramp=bad.source_cell_ramps[0];
        switch(variant) {
        case 0:ramp.id.value=0;break;case 1:ramp.id.value=2;break;
        case 2:bad.next_source_cell_ramp_id=1;break;case 3:ramp.source_cell={INT32_MIN,5};break;
        case 4:ramp.high_neighbor={INT32_MAX,5};break;case 5:ramp.high_neighbor=ramp.source_cell;break;
        case 6:ramp.high_neighbor={9,6};break;case 7:ramp.high_neighbor={10,5};break;
        case 8:ramp.low_height=-1;break;case 9:ramp.high_height=0;break;case 10:ramp.paid_cost=0;break;
        case 11:ramp.placed_tick=bad.ticks+1;break;
        case 12:bad.source_cell_ramps.push_back(ramp);bad.next_source_cell_ramp_id=3;break;
        case 13:bad.rule_version=6;break;case 14:--bad.construction_spent_total;break;
        case 15:--bad.road_revision;break;case 16:bad.source_cell_ramps.resize(65,ramp);bad.next_source_cell_ramp_id=66;break;
        case 17:bad.original_economy.emplace();break;case 18:bad.next_source_cell_ramp_id=UINT32_MAX;break;
        case 19:bad.ramps.push_back({{1},{8,15},{9,15},0,1,40,0});bad.next_ramp_id=2;
            bad.source_cell_ramps.resize(64,ramp);bad.next_source_cell_ramp_id=65;break;
        }
        bool rejected=false;try{(void)World::restore(bad,policy);}catch(const std::exception&){rejected=true;}
        check(rejected&&world.snapshot()==stable,"hostile source-cell save accepted or mutated source");
    }
    for(const Cell cell:{Cell{7,5},source,high})check(world.execute({CommandType::RemoveRoad,cell}).accepted,
        "empty source-cell road removal failed");
    check(World::restore(world.snapshot(),policy).snapshot()==world.snapshot(),"disconnected axial claims failed restore");
    refused(world,{CommandType::PlaceHousehold,{7,4}},BuildBlocker::SandboxOccupied);
    for(const Cell cell:{Cell{7,5},source,high})road(world,cell);
    check(world.source_cell_ramps().front().id.value==1&&world.transport_edge_allowed(source,high),
        "paid road repair lost ramp identity");
    // A valid high-bank Bridge would use an already reserved axial claim.
    auto bridge_cells=policy->cells();auto& water=bridge_cells[5U*32+10U];
    water.road_allowed=water.building_allowed=false;water.bridge_water=true;water.road_blocker=BuildBlocker::UnsupportedTerrain;
    auto bridge_policy=std::make_shared<const MapPermissions>(32,24,3,std::move(bridge_cells),std::vector<FixedGatePassage>{});
    auto overlap=World::create_authored_source_cell_ramps(bridge_policy);three_roads(overlap,source,{1,0});road(overlap,{11,5});
    check(overlap.execute(place_source_cell_ramp(source,high)).accepted,"bridge-overlap source fixture failed");
    refused(overlap,place_bridge(high,{11,5}),BuildBlocker::SandboxOccupied);
    auto bridge_first=World::create_authored_source_cell_ramps(bridge_policy);
    three_roads(bridge_first,source,{1,0});road(bridge_first,{11,5});
    check(bridge_first.execute(place_bridge(high,{11,5})).accepted,"reverse bridge overlap fixture failed");
    refused(bridge_first,place_source_cell_ramp(source,high),BuildBlocker::ProtectedBridge);
    auto legacy_first=World::create_authored_source_cell_ramps(policy);three_roads(legacy_first,source,{1,0});
    check(legacy_first.execute(place_ramp(source,high)).accepted,"reverse legacy overlap fixture failed");
    refused(legacy_first,place_source_cell_ramp(source,high),BuildBlocker::SandboxOccupied);
    // Real paid budget exhaustion, without a Snapshot treasury override.
    const auto large=step_policy(source,{1,0},0,64,32);auto poor=World::create_authored_source_cell_ramps(large);
    three_roads(poor,source,{1,0});
    for(int y=0;y<32&&poor.treasury()>=40;++y)for(int x=0;x<64&&poor.treasury()>=40;++x)
        if(!poor.road_cell({x,y}))road(poor,{x,y});
    const auto preview=poor.validate_source_cell_ramp(source,high);
    check(preview.geometry_valid&&!preview.accepted&&poor.treasury()==38,"paid funds-only source-cell setup failed");
    refused(poor,place_source_cell_ramp(source,high),BuildBlocker::InsufficientFunds);
}
void history_and_building_entrances() {
    const auto policy=step_policy();
    auto world=World::create_authored_source_cell_ramps(policy);
    // All fields except the absent prerequisite paid road history describe a
    // superficially balanced first40-fund infrastructure purchase.
    auto forged=world.snapshot();
    forged.source_cell_ramps.push_back({{1},{8,5},{9,5},0,1,40,0});
    forged.next_source_cell_ramp_id=2;forged.command_sequence=forged.road_revision=1;
    forged.construction_spent_total=40;forged.treasury-=40;
    bool refused_history=false;
    try{(void)World::restore(forged,policy);}catch(const std::invalid_argument& error) {
        refused_history=std::string(error.what()).find("paid road construction/removal history")!=std::string::npos;
    }
    check(refused_history,"source-cell save invented unpaid prerequisite roads");
    three_roads(world,{8,5},{1,0});
    check(world.execute(place_source_cell_ramp({8,5},{9,5})).accepted,"history fixture purchase failed");
    for(const Cell cell:{Cell{7,5},Cell{8,5},Cell{9,5}})
        check(world.execute({CommandType::RemoveRoad,cell}).accepted,"history fixture road removal failed");
    const auto disconnected=world.snapshot();
    check(World::restore(disconnected,policy).snapshot()==disconnected,"real paid removed claims rejected");
    auto absent_removals=disconnected;absent_removals.roads_removed_total=0;
    refused_history=false;
    try{(void)World::restore(absent_removals,policy);}catch(const std::invalid_argument& error) {
        refused_history=std::string(error.what()).find("paid road construction/removal history")!=std::string::npos;
    }
    check(refused_history,"source-cell save invented missing roads without any removal");
    auto ordered=World::create_authored_source_cell_ramps(policy);three_roads(ordered,{8,5},{1,0});
    check(ordered.execute(place_source_cell_ramp({8,5},{9,5})).accepted,"ordered first ramp failed");
    ordered.tick();ordered.tick();three_roads(ordered,{8,10},{1,0});
    check(ordered.execute(place_source_cell_ramp({8,10},{9,10})).accepted,"ordered second ramp failed");
    check(World::restore(ordered.snapshot(),policy).snapshot()==ordered.snapshot(),"ordered two-ramp history failed restore");
    auto bad_order=ordered.snapshot();bad_order.source_cell_ramps[0].placed_tick=1;
    bad_order.source_cell_ramps[1].placed_tick=0;
    bool rejected=false;try{(void)World::restore(bad_order,policy);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"monotone source-cell IDs accepted reversed purchase ticks");
    auto entrances=World::create_authored_source_cell_ramps(policy);three_roads(entrances,{8,5},{1,0});
    check(entrances.execute(place_source_cell_ramp({8,5},{9,5})).accepted&&
        entrances.execute({CommandType::PlaceHousehold,{8,3}}).accepted,"adjacent paid House fixture failed");
    const auto entries=entrances.building_entrances(static_cast<BuildingId>(1));
    check(!entries.empty()&&std::none_of(entries.begin(),entries.end(),[](const auto& entry) {
        return entry.road_cell==Cell{8,5};
    }),"source-cell half-height centre granted a direct building entrance");
    check(World::restore(entrances.snapshot(),policy).snapshot()==entrances.snapshot(),"adjacent legal paid House failed restore");
    // Explicit synthetic immutable GroundRoad controls: paid prerequisites
    // exclude exactly imported road cells, never every road-looking cell.
    for(const bool all_original:{false,true}) {
        auto cells=policy->cells();
        for(const Cell cell:{Cell{7,5},Cell{8,5},Cell{9,5}}) {
            if(!all_original&&cell!=Cell{8,5})continue;
            auto& permission=cells[std::size_t(cell.y*32+cell.x)];
            permission.original_road=true;permission.original_road_classification=OriginalRoadClassification::GroundRoad;
            permission.building_allowed=false;permission.building_blocker=BuildBlocker::ProtectedOriginalRoad;
        }
        const auto imported=std::make_shared<const MapPermissions>(32,24,3,std::move(cells),std::vector<FixedGatePassage>{});
        auto mixed=World::create_authored_source_cell_ramps(imported);
        if(!all_original){road(mixed,{7,5});road(mixed,{9,5});}
        check(mixed.execute(place_source_cell_ramp({8,5},{9,5})).accepted,"immutable road prerequisite fixture refused");
        check(mixed.roads_placed_total()==(all_original?0U:2U)&&
            World::restore(mixed.snapshot(),imported).snapshot()==mixed.snapshot(),
            "paid road history bound charged or rejected validated immutable GroundRoads");
    }
}
void referenced_surface_refusal() {
    const auto policy=step_policy();auto world=World::create_authored_source_cell_ramps(policy);
    for(const Command command:std::array<Command,4>{{{CommandType::PlaceHousehold,{1,1}},
        {CommandType::PlaceHousehold,{1,3}},{CommandType::PlaceClaySource,{4,2}},{CommandType::PlacePottery,{6,9}}}})
        check(world.execute(command).accepted,"active surface paid fixture purchase failed");
    for(int x=4;x<=9;++x)road(world,{x,5});
    road(world,{5,4});
    for(int y=6;y<=10;++y)road(world,{8,y});
    bool live=false,past=false;
    for(int tick=0;tick<150;++tick) {
        world.tick();const auto actor=world.couriers().front();
        const auto point=std::find(actor.path.begin(),actor.path.end(),Cell{8,5});
        if(point==actor.path.end())continue;
        refused(world,place_source_cell_ramp({8,5},{9,5}),BuildBlocker::SandboxOccupied);live=true;
        if(std::size_t(point-actor.path.begin())<actor.path_vertex){past=true;break;}
    }
    check(live&&past,"active and historical-prefix courier surface refusals were not physically exercised");
}
}
int main() {try {
    surface_and_legacy_controls();negative_geometry_and_restore();history_and_building_entrances();referenced_surface_refusal();
    paid_transport_and_repair(false);paid_transport_and_repair(true);
    std::cout<<"SourceCellRampWorldTests PASS: paid axial cargo/arrival/return, begun-edge cut/repair, signed surfaces and hostile restores\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"SOURCE CELL RAMP: "<<error.what()<<'\n';return 1;}}
