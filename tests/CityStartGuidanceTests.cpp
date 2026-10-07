#include "simulation/CityStartGuidance.h"
#include "core/PerformanceDiagnostics.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <climits>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace sim=openemperor::simulation;

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

void put(sim::World& world,sim::CommandType type,int x,int y) {
    const auto result=world.execute({type,{x,y}});
    require(result.accepted,"command rejected at "+std::to_string(x)+","+
        std::to_string(y)+": "+result.reason);
}

sim::World make_world() {
    constexpr int width=48,height=24;
    return {width,height,std::vector<std::uint8_t>(width*height,1),
            sim::RulesProfile::CityV11,2};
}

void road_only_permission_budget_copy() {
    constexpr int width=24,height=16;
    std::vector<sim::MapCellPermission> cells(width*height,{true,true,false,0,
        sim::BuildBlocker::None,sim::BuildBlocker::None});
    const sim::Cell road_only{4,4};
    const auto road_index=static_cast<std::size_t>(road_only.y*width+road_only.x);
    cells[road_index].building_allowed=false;
    cells[road_index].building_blocker=
        sim::BuildBlocker::LegacyBuildabilityRestriction;
    const auto permissions=std::make_shared<const sim::MapPermissions>(width,height,
        sim::kMapPermissionsPolicyVersion,std::move(cells),
        std::vector<sim::FixedGatePassage>{});
    sim::World world(permissions,sim::RulesProfile::CityV16,3);
    require(world.road_buildable(road_only) && !world.buildable(road_only),
        "fixture did not separate road and building permission");
    put(world,sim::CommandType::PlaceRoad,road_only.x,road_only.y);
    const sim::Command purchase{sim::CommandType::PlaceClaySource,{10,4}};
    require(world.taxes_collected_total()==0 && world.validate(purchase).accepted,
        "road-only budget fixture has tax or an invalid purchase");
    const auto before=world.snapshot();
    const auto policy_before=permissions->canonical_state();
    require(!sim::starter_budget_warning(world,purchase),
        "adequately funded road-only fixture emitted a budget warning");
    require(world.snapshot()==before && world.map_permissions()==permissions &&
            permissions->canonical_state()==policy_before,
        "road-only budget check mutated source World or map policy");

    for (int i=0;i<5;++i) put(world,sim::CommandType::PlaceHousehold,i*3,12);
    const sim::Command sixth{sim::CommandType::PlaceHousehold,{15,12}};
    const auto cancelled=world.snapshot();
    const auto warning=sim::starter_budget_warning(world,sixth);
    require(warning && warning->purchase_cost==80 && warning->funds_after_purchase==818 &&
            warning->minimum_remaining_building_funds==850 &&
            warning->minimum_remaining_house_funds==0,
        "road-only policy did not retain the ordinary starter reserve warning");
    require(world.snapshot()==cancelled && permissions->canonical_state()==policy_before,
        "cancelled road-only budget purchase changed source state");
    const auto committed=world.execute(sixth);
    require(committed.accepted && committed.changed && world.treasury()==818 &&
            world.command_sequence()==cancelled.command_sequence+1,
        "approved road-only budget purchase did not run one paid command");
}

constexpr int gate_size=24;
sim::Cell gate_axis(sim::Cell p,bool horizontal) {
    return horizontal ? sim::Cell{p.y,p.x}:p;
}

std::shared_ptr<const sim::MapPermissions> guidance_gate_policy(bool horizontal) {
    std::vector<sim::MapCellPermission> cells(gate_size*gate_size,{true,true,false,1,
        sim::BuildBlocker::None,sim::BuildBlocker::None});
    const auto at=[&](sim::Cell p)->sim::MapCellPermission& {
        p=gate_axis(p,horizontal);
        return cells[static_cast<std::size_t>(p.y*gate_size+p.x)];
    };
    sim::FixedGatePassage gate;gate.id={101};
    for (int y=7;y<=9;++y) for (int x=5;x<=9;++x) {
        gate.protected_footprint.push_back(gate_axis({x,y},horizontal));
        at({x,y})={false,false,true,1,sim::BuildBlocker::GateSolidPart,
            sim::BuildBlocker::OriginalStructure};
    }
    for (int y=7;y<=9;++y) {
        gate.corridor.push_back(gate_axis({7,y},horizontal));
        at({7,y}).road_allowed=true;
    }
    gate.openings={gate_axis({7,6},horizontal),gate_axis({7,10},horizontal)};
    for (int x=0;x<gate_size;++x) if (x<5 || x>9)
        at({x,8})={false,false,true,1,sim::BuildBlocker::OriginalStructure,
            sim::BuildBlocker::OriginalStructure};
    at({4,5}).building_allowed=false;
    at({4,5}).building_blocker=sim::BuildBlocker::LegacyBuildabilityRestriction;
    at({22,22}).height=-2;
    return std::make_shared<const sim::MapPermissions>(gate_size,gate_size,
        sim::kMapPermissionsPolicyVersion,std::move(cells),
        std::vector<sim::FixedGatePassage>{std::move(gate)});
}

void full_policy_budget_copy(bool horizontal) {
    const auto permissions=guidance_gate_policy(horizontal);
    const auto policy_before=permissions->canonical_state();
    sim::World world(permissions,sim::RulesProfile::CityV16,3);
    for (const auto p:{sim::Cell{0,0},sim::Cell{3,0},sim::Cell{10,0},sim::Cell{13,0}}) {
        const auto q=gate_axis(p,horizontal);
        put(world,sim::CommandType::PlaceHousehold,q.x,q.y);
    }
    const auto source=gate_axis({3,3},horizontal);
    const auto destination=gate_axis({7,12},horizontal);
    put(world,sim::CommandType::PlaceClaySource,source.x,source.y);
    put(world,sim::CommandType::PlacePottery,destination.x,destination.y);
    for (const auto p:{sim::Cell{4,5},sim::Cell{5,5},sim::Cell{6,5},sim::Cell{7,5},
            sim::Cell{7,6},sim::Cell{7,10},sim::Cell{7,11}}) {
        const auto q=gate_axis(p,horizontal);
        put(world,sim::CommandType::PlaceRoad,q.x,q.y);
    }
    const auto owner=*world.building_owner_at(source);
    const auto courier=std::ranges::find_if(world.couriers(),[&](const auto& c) {
        return c.owner==owner;
    });
    require(courier!=world.couriers().end(),"gate guidance fixture has no courier");
    const auto id=courier->id;
    const auto inside=[&] {
        const auto& c=world.courier(id);
        return c.phase==sim::CourierPhase::ToWarehouse && !c.path.empty() &&
            world.fixed_passage(c.path[c.path_vertex]) && c.edge_progress==1;
    };
    for (int i=0;i<1'000 && !inside();++i) world.tick();
    require(inside() && world.taxes_collected_total()==0,
        "gate budget fixture did not reach active untaxed passage travel");
    const auto before=world.snapshot();
    const auto canonical_before=world.canonical_state();
    auto hypothetical=sim::World::restore(before,permissions);
    require(hypothetical.map_permissions()==permissions && hypothetical.snapshot()==before &&
            hypothetical.fixed_passage(gate_axis({7,8},horizontal)) &&
            permissions->gates()[0].protected_footprint.size()==15 &&
            permissions->cell_height(gate_axis({22,22},horizontal))==-2 &&
            hypothetical.transport_edge_allowed(gate_axis({7,6},horizontal),
                gate_axis({7,7},horizontal)) &&
            hypothetical.map_permissions()->transport_edge_blocker(
                gate_axis({7,8},horizontal),gate_axis({6,8},horizontal))==
                    sim::BuildBlocker::GateSideEntry &&
            hypothetical.map_permissions()->transport_edge_blocker(
                gate_axis({22,21},horizontal),gate_axis({22,22},horizontal))==
                    sim::BuildBlocker::UnsupportedHeightTransition,
        "hypothetical restore lost shared policy, gate, opening or signed heights");
    const sim::Command purchase{sim::CommandType::PlaceFarm,{18,18}};
    require(world.validate(purchase).accepted && !sim::starter_budget_warning(world,purchase),
        "active passage courier prevented a valid adequately funded budget check");
    require(hypothetical.execute(purchase).accepted && hypothetical.snapshot()!=before &&
            world.snapshot()==before && world.canonical_state()==canonical_before &&
            world.map_permissions()==permissions && permissions->canonical_state()==policy_before,
        "hypothetical purchase changed source World or immutable policy");
    hypothetical.tick();world.tick();
    const auto& copied=hypothetical.courier(id);
    const auto& actual=world.courier(id);
    require(copied.phase==actual.phase && copied.path==actual.path &&
            copied.path_vertex==actual.path_vertex && copied.edge_progress==actual.edge_progress &&
            copied.cargo==actual.cargo && copied.reserved==actual.reserved &&
            hypothetical.courier_position(id)==world.courier_position(id) &&
            permissions->canonical_state()==policy_before,
        "hypothetical purchase lost gate-courier continuation context");

    auto invalid=before;
    invalid.roads.push_back(gate_axis({6,8},horizontal));
    bool rejected=false;
    try { (void)sim::World::restore(invalid,permissions); }
    catch (const std::invalid_argument&) { rejected=true; }
    require(rejected,"invalid road on protected gate solid part was accepted");
}

void legacy_budget_copy_versions() {
    constexpr int width=24,height=16;
    for (const auto profile:{sim::RulesProfile::CityV10,sim::RulesProfile::CityV16})
        for (const auto version:{1U,2U}) {
            if (profile==sim::RulesProfile::CityV10 && version==2) continue;
            sim::World world(width,height,std::vector<std::uint8_t>(width*height,1),profile,version);
            require(!world.map_permissions(),"legacy profile acquired map permissions");
            for (int i=0;i<5;++i) put(world,sim::CommandType::PlaceHousehold,i*3,12);
            const sim::Command sixth{sim::CommandType::PlaceHousehold,{15,12}};
            const auto before=world.snapshot();
            const auto warning=sim::starter_budget_warning(world,sixth);
            if (profile==sim::RulesProfile::CityV10)
                require(!warning,"City-v10 acquired a City-v11 budget warning");
            else require(warning && warning->purchase_cost==80 &&
                    warning->funds_after_purchase==820 &&
                    warning->minimum_remaining_building_funds==850,
                "legacy City-v16 budget behavior changed");
            require(world.snapshot()==before,"legacy budget copy changed source World");
        }
}

void put_house(sim::World& world,int ordinal) {
    const int x=(ordinal%10)*3;
    const int y=12+(ordinal/10)*3;
    put(world,sim::CommandType::PlaceHousehold,x,y);
}

void screenshot_budget_blockade() {
    auto world=make_world();
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,3,2);
    put(world,sim::CommandType::PlaceWarehouse,6,2);
    for (int i=0;i<9;++i) put_house(world,i);
    for (int x=0;x<48;++x) put(world,sim::CommandType::PlaceRoad,x,8);
    for (int x=0;x<17;++x) put(world,sim::CommandType::PlaceRoad,x,9);
    require(world.treasury()==0 && world.workforce_supply()==54 &&
            world.workforce_required()==12,"screenshot-like treasury/workforce setup differs");
    const auto before=world.snapshot();
    const auto status=sim::inspect_city_start(world);
    require(status.missing_supply_buildings==std::vector<sim::Object>{
                sim::Object::Farm,sim::Object::Market,sim::Object::ServicePost} &&
            status.minimum_missing_building_funds==400 &&
            status.workforce_required_now==12 &&
            status.workforce_required_for_starter==22 &&
            status.starter_workforce_shortfall==0 &&
            status.suggested_additional_houses==0,
        "missing infrastructure was misreported as a worker shortage");
    for (int i=0;i<1600;++i) world.tick();
    require(world.taxes_collected_total()==0,
        "incomplete Food/Market/Service chain produced tax");
    auto restored=sim::World::restore(before,std::vector<std::uint8_t>(48*24,1));
    const auto restored_before=restored.snapshot();
    require(sim::inspect_city_start(restored).minimum_missing_building_funds==400 &&
            restored.snapshot()==restored_before,
        "read-only start inspection changed the World");
}

void build_one_house_chain(sim::World& world) {
    // The reviewed order deliberately makes stable staffing order observable.
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlaceHousehold,6,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<=14;++x) put(world,sim::CommandType::PlaceRoad,x,4);
}

void workforce_recovery_and_real_tax() {
    auto world=make_world();
    build_one_house_chain(world);
    const auto status=sim::inspect_city_start(world);
    require(status.complete_supply_chain && status.workforce_supply==6 &&
            status.workforce_required_for_starter==22 &&
            status.starter_workforce_shortfall==16 &&
            status.suggested_additional_houses==3 && status.suggested_house_cost==240,
        "fresh one-house worker recommendation differs");
    const auto clay=world.buildings()[0].id;
    require(world.building_staffed(clay),"stable ID staffing did not staff Clay Source");
    bool pottery_unstaffed=false,market_unstaffed=false,warehouse_staffed=false,
         farm_unstaffed=false,service_unstaffed=false;
    for (const auto& building:world.buildings()) {
        if (building.kind==sim::Object::Pottery) pottery_unstaffed=!world.building_staffed(building.id);
        if (building.kind==sim::Object::Market) market_unstaffed=!world.building_staffed(building.id);
        if (building.kind==sim::Object::Warehouse) warehouse_staffed=world.building_staffed(building.id);
        if (building.kind==sim::Object::Farm) farm_unstaffed=!world.building_staffed(building.id);
        if (building.kind==sim::Object::ServicePost) service_unstaffed=!world.building_staffed(building.id);
    }
    require(pottery_unstaffed && market_unstaffed && warehouse_staffed && farm_unstaffed &&
            service_unstaffed,"one-house stable ID staffing allocation differs");
    for (const auto& courier:world.couriers()) if (courier.enabled && courier.owner!=clay)
        require(world.courier_dispatch_status(courier.id).status==
                    sim::CourierDispatchStatus::Unstaffed ||
                world.building(courier.owner).kind==sim::Object::Warehouse,
            "unstaffed owner did not report unstaffed dispatch");

    for (const auto cell:{sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    require(world.ticks()==0 && world.workforce_supply()==24 &&
            world.workforce_required()==22,
        "three paused house commands did not recover workforce immediately");
    for (const auto& building:world.buildings())
        if (building.kind!=sim::Object::Household)
            require(world.building_staffed(building.id),"starter facility remained unstaffed");

    for (int x=0;x<=14;++x)
        put(world,sim::CommandType::RemoveRoad,x,4);
    for (int i=0;i<200;++i) world.tick();
    const auto disconnected=sim::inspect_city_start(world);
    require(std::ranges::any_of(disconnected.facilities,[](const auto& facility) {
                return facility.condition==sim::StarterSupplyCondition::NoReachableTarget;
            }) && std::ranges::any_of(disconnected.facilities,[](const auto& facility) {
                return facility.condition==sim::StarterSupplyCondition::AwaitingGoods;
            }),"routing and not-yet-produced supply states were not distinguished");
    for (int x=0;x<=14;++x) put(world,sim::CommandType::PlaceRoad,x,4);

    bool market_pottery_seen=false,market_food_seen=false,house_pottery_seen=false,
         house_food_seen=false,service_seen=false;
    const auto deadline=world.ticks()+8'000;
    while (world.taxes_collected_total()==0 && world.ticks()<deadline) {
        world.tick();
        for (const auto& building:world.buildings()) {
            if (building.kind==sim::Object::Market) {
                market_pottery_seen=market_pottery_seen || building.pottery_stock>0;
                market_food_seen=market_food_seen || building.food_stock>0;
            }
            if (building.kind==sim::Object::Household) {
                house_pottery_seen=house_pottery_seen || building.pottery_stock>0;
                house_food_seen=house_food_seen || building.food_stock>0;
                service_seen=service_seen || world.household_service_active(building.id);
            }
        }
    }
    require(world.taxes_collected_total()>0,"complete recovered chain earned no real tax");
    require(world.clay_extracted_total()>0 && world.pottery_completed_total()>0 &&
            world.food_produced_total()>0 && world.covered_households()>0,
        "recovery did not run Clay, Pottery, Food and Service");
    require(market_pottery_seen && market_food_seen && house_pottery_seen &&
            house_food_seen && service_seen,
        "Warehouse/Farm -> Market -> House or Service arrival was not observed");
    std::cout<<"Recovered four-house starter paid its first tax at tick "
             <<world.ticks()<<'\n';
}

void shrunken_house_recommendation() {
    auto world=make_world();
    put(world,sim::CommandType::PlaceHousehold,6,2);
    for (int i=0;i<2600;++i) world.tick();
    require(world.total_population()==sim::Rules::household_min_population,
        "isolated first House did not shrink through ordinary unmet demands");
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<48;++x) put(world,sim::CommandType::PlaceRoad,x,20);
    for (int x=0;x<12;++x) put(world,sim::CommandType::PlaceRoad,x,21);
    const auto status=sim::inspect_city_start(world);
    require(status.starter_workforce_shortfall==20 &&
            status.suggested_additional_houses==4 &&
            status.affordable_suggested_houses==3 &&
            status.suggested_house_cost==320 && status.suggested_house_funds_missing==70,
        "recommendation was fixed at three Houses instead of using actual population");
}

void budget_confirmation_contract() {
    auto world=make_world();
    for (int i=0;i<5;++i) put_house(world,i);
    const sim::Command sixth{sim::CommandType::PlaceHousehold,{15,12}};
    const auto warning=sim::starter_budget_warning(world,sixth);
    require(warning && warning->purchase_cost==sim::Rules::household_cost &&
            warning->funds_after_purchase==820 &&
            warning->minimum_remaining_building_funds==850,
        "budget warning did not preserve the starter-building reserve");
    const auto cancelled=world.snapshot();
    require(world.snapshot()==cancelled,"cancel path changed World before a command");
    const auto built=world.execute(sixth);
    require(built.accepted && built.changed && world.command_sequence()==
            cancelled.command_sequence+1 && world.treasury()==820 &&
            world.next_building_id()==cancelled.next_building_id+1,
        "Build anyway did not run exactly one normally validated command");

    auto complete=make_world();
    build_one_house_chain(complete);
    require(!sim::starter_budget_warning(complete,
                {sim::CommandType::PlaceRoad,{20,4}}),
        "complete adequately funded supply emitted an irrelevant reserve warning");
}

void workforce_reserve_is_guarded() {
    auto world=make_world();
    build_one_house_chain(world);
    std::vector<sim::Command> purchase;
    // The complete one-House chain leaves 340 funds. Fifty-one ordinary,
    // distinct road cells cost 102 and leave 238, two below the 240 needed
    // for the three fresh Houses that supply the missing 16 workers.
    for (int x=16;x<48;++x) purchase.push_back({sim::CommandType::PlaceRoad,{x,4}});
    for (int x=16;x<35;++x) purchase.push_back({sim::CommandType::PlaceRoad,{x,5}});
    require(purchase.size()==51,"workforce reserve purchase fixture differs");
    const auto before=world.snapshot();
    const auto warning=sim::starter_budget_warning(world,purchase);
    require(warning && warning->purchase_cost==102 && warning->funds_after_purchase==238 &&
            warning->minimum_remaining_building_funds==0 &&
            warning->minimum_remaining_house_funds==240 &&
            warning->minimum_remaining_start_cost==240 &&
            warning->additional_houses_needed==3 &&
            warning->starter_workforce_within_house_limit,
        "complete infrastructure did not reserve the three required Houses");
    require(world.snapshot()==before,"workforce reserve inspection changed World");
}

std::vector<sim::Command> road_purchase(std::size_t count) {
    std::vector<sim::Command> result;
    for (int y=4;y<12 && result.size()<count;++y)
        for (int x=16;x<48 && result.size()<count;++x)
            result.push_back({sim::CommandType::PlaceRoad,{x,y}});
    require(result.size()==count,"road purchase fixture too small");
    return result;
}

void post_purchase_reserve_cases() {
    {
        auto empty=make_world();
        const auto purchase=road_purchase(66);
        const auto warning=sim::starter_budget_warning(empty,purchase);
        require(warning && warning->funds_after_purchase==1'168 &&
                warning->minimum_remaining_building_funds==850 &&
                warning->additional_houses_needed==4 &&
                warning->minimum_remaining_house_funds==320 &&
                warning->minimum_remaining_start_cost==1'170,
            "combined facility and workforce reserves were omitted or double-counted");
    }
    {
        auto world=make_world();
        build_one_house_chain(world);
        const sim::Command extra_industry{sim::CommandType::PlaceClaySource,{20,12}};
        const auto warning=sim::starter_budget_warning(world,extra_industry);
        require(warning && warning->purchase_cost==sim::Rules::clay_source_cost &&
                warning->funds_after_purchase==220 && warning->additional_houses_needed==4 &&
                warning->minimum_remaining_house_funds==320 &&
                warning->minimum_remaining_start_cost==320,
            "post-purchase industry demand did not increase the workforce reserve");
    }
    {
        auto world=make_world();
        build_one_house_chain(world);
        std::vector<sim::Command> purchase{{sim::CommandType::PlaceHousehold,{9,2}}};
        auto roads=road_purchase(51);
        purchase.insert(purchase.end(),roads.begin(),roads.end());
        const auto warning=sim::starter_budget_warning(world,purchase);
        require(warning && warning->purchase_cost==182 && warning->funds_after_purchase==158 &&
                warning->additional_houses_needed==2 &&
                warning->minimum_remaining_house_funds==160,
            "purchased House was charged or reserved twice");
    }
    {
        auto exact=make_world();
        build_one_house_chain(exact);
        const auto roads=road_purchase(50);
        require(!sim::starter_budget_warning(exact,roads),
            "funds exactly equal to the starter reserve emitted a warning");
        for (const auto command:roads) put(exact,command.type,command.cell.x,command.cell.y);
        require(exact.treasury()==240,"exact reserve fixture differs");
        const sim::Command one_new_road{sim::CommandType::PlaceRoad,{16,11}};
        const std::array duplicate{one_new_road,one_new_road};
        const auto warning=sim::starter_budget_warning(exact,duplicate);
        require(warning && warning->purchase_cost==sim::Rules::road_cost &&
                warning->funds_after_purchase==238 &&
                warning->minimum_remaining_start_cost==240,
            "underfunded reserve or duplicate-road accounting differs");
    }
    {
        auto complete=make_world();
        build_one_house_chain(complete);
        for (const auto cell:{sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
            put(complete,sim::CommandType::PlaceHousehold,cell.x,cell.y);
        require(complete.treasury()==100 &&
                !sim::starter_budget_warning(complete,
                    {sim::CommandType::PlaceRoad,{20,4}}),
            "paid four-House starter emitted an unnecessary reserve warning");
    }
}

void exhausted_house_limit_is_explicit() {
    auto world=make_world();
    build_one_house_chain(world);
    for (const auto cell:{sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    while (world.treasury()<3'200 && world.ticks()<20'000) world.tick();
    require(world.treasury()>=3'200,"ordinary tax income did not fund house-limit fixture");
    for (const auto cell:{sim::Cell{20,2},sim::Cell{23,2},sim::Cell{26,2}})
        put(world,sim::CommandType::PlaceClaySource,cell.x,cell.y);
    for (const auto cell:{sim::Cell{20,5},sim::Cell{23,5},sim::Cell{26,5}})
        put(world,sim::CommandType::PlacePottery,cell.x,cell.y);
    put(world,sim::CommandType::PlaceWarehouse,29,2);
    put(world,sim::CommandType::PlaceFarm,29,5);
    for (const auto cell:{sim::Cell{31,5},sim::Cell{32,5},sim::Cell{33,5}})
        put(world,sim::CommandType::PlaceMarket,cell.x,cell.y);
    put(world,sim::CommandType::PlaceServicePost,34,5);
    for (int i=0;i<16;++i) put_house(world,i);
    require(world.buildings().size()==sim::Rules::city_v11_building_limit &&
            world.workforce_required()==72,
        "house-limit fixture did not reach the authored City-v11 limits");

    // Disconnect the compact starter through ordinary removals, then allow
    // unmet demands to reduce population. A protected road is retried after
    // normal ticks; no World field is injected.
    for (int x=0;x<=14;++x) {
        bool removed=false;
        for (int attempt=0;attempt<2'000 && !removed;++attempt) {
            const auto result=world.execute({sim::CommandType::RemoveRoad,{x,4}});
            removed=result.accepted;
            if (!removed) world.tick();
        }
    }
    for (int i=0;i<7'000;++i) world.tick();
    const auto status=sim::inspect_city_start(world);
    require(status.household_count==sim::Rules::city_v10_household_limit &&
            status.household_slots_remaining==0 && status.starter_workforce_shortfall>0 &&
            status.houses_needed_for_shortfall>0 &&
            !status.starter_workforce_within_house_limit,
        "exhausted House limit falsely cleared an impossible workforce completion");
}

sim::World guidance_world(sim::RulesProfile profile=sim::RulesProfile::CityV16,
                          std::uint32_t version=1) {
    return {48,24,std::vector<std::uint8_t>(48*24,1),profile,version};
}

sim::BuildingId building_kind(const sim::World& world,sim::Object kind) {
    for (const auto& b:world.buildings()) if (b.placed && b.kind==kind) return b.id;
    throw std::runtime_error("missing fixture building");
}

const sim::StarterFacilityInstance& instance(const sim::CityStartGuidance& status,
                                             sim::BuildingId id) {
    const auto found=std::ranges::find(status.facility_instances,id,
        &sim::StarterFacilityInstance::id);
    require(found!=status.facility_instances.end(),"missing concrete facility ID");
    return *found;
}

const sim::HouseholdStartStatus& household(const sim::CityStartGuidance& status,
                                           sim::BuildingId id) {
    const auto found=std::ranges::find(status.households,id,&sim::HouseholdStartStatus::id);
    require(found!=status.households.end(),"missing concrete House ID");
    return *found;
}

sim::BuildingSnapshot& snapshot_home(sim::WorldSnapshot& snapshot,sim::BuildingId id) {
    const auto found=std::ranges::find(snapshot.buildings,id,&sim::BuildingSnapshot::id);
    require(found!=snapshot.buildings.end(),"missing fixture snapshot building");
    return *found;
}

sim::World paid_guidance_starter(sim::RulesProfile profile=sim::RulesProfile::CityV16) {
    auto world=guidance_world(profile);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<=14;++x) put(world,sim::CommandType::PlaceRoad,x,4);
    for (const auto p:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(world,sim::CommandType::PlaceHousehold,p.x,p.y);
    if (sim::fire_profile(profile)) put(world,sim::CommandType::PlaceFireWatch,14,5);
    return world;
}

void corresponding_failed_city_and_staffing() {
    // Independently authored valid city, not a reconstruction of Oliver's save.
    // Its initial population is 18, rather than the screenshot's reported 16.
    auto world=guidance_world();
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceMarket,3,5);
    put(world,sim::CommandType::PlaceFireWatch,14,5);
    put(world,sim::CommandType::PlaceWell,11,3);
    put(world,sim::CommandType::PlaceFarm,4,5);
    for (const auto p:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2}})
        put(world,sim::CommandType::PlaceHousehold,p.x,p.y);
    for (int x=0;x<48;++x) put(world,sim::CommandType::PlaceRoad,x,16);
    for (int x=0;x<24;++x) put(world,sim::CommandType::PlaceRoad,x,17);
    const auto before=world.snapshot();
    const auto farm=building_kind(world,sim::Object::Farm);
    const auto status=sim::inspect_city_start(world);
    require(world.treasury()==26 && world.construction_spent_total()==1274 &&
            status.funds_current==26 && status.taxes_received_total==0 &&
            status.missing_supply_buildings==std::vector{sim::Object::ServicePost} &&
            status.missing_supply_costs.size()==1 &&
            status.missing_supply_costs[0].kind==sim::Object::ServicePost &&
            status.missing_supply_costs[0].cost==100 &&
            status.missing_building_funds_gap==74,
        "corresponding failed city lost its exact Service-only 74-Funds gap");
    require(status.workforce_supply==18 && status.workforce_used==18 &&
            status.workforce_active_demand==22 && status.workforce_installed_demand==22 &&
            status.workforce_required_for_starter==24 && status.houses_needed_for_shortfall==1 &&
            status.minimum_house_funds_for_starter==80 && status.construction_funds_gap==154 &&
            instance(status,farm).workers_assigned==0 && instance(status,farm).workers_required==4 &&
            instance(status,farm).condition==sim::StarterSupplyCondition::Unstaffed,
        "current allocation and simultaneous-full-staffing estimate were conflated");
    require(status.households_ready_now==0 && status.households_missing_service==3 &&
            status.households_missing_food==3 && status.households_missing_pottery==3 &&
            status.maintenance.installed_rate==46 && status.maintenance.next_bill_tick==400 &&
            status.maintenance.next_bill_cost==46 && status.maintenance.interval_cost==46 &&
            status.maintenance.funds_after_interval==-20 && world.snapshot()==before,
        "failed-city supply/upkeep diagnosis mutated or misreported current facts");
    const auto priority=world.execute(sim::set_building_workforce_priority(
        farm,sim::WorkforcePriority::High));
    require(priority.accepted,"ordinary Farm priority command rejected");
    const auto changed=sim::inspect_city_start(world);
    require(instance(changed,farm).workers_assigned==4 &&
            std::ranges::any_of(changed.facility_instances,[&](const auto& f) {
                return f.id!=farm && f.condition==sim::StarterSupplyCondition::Unstaffed;
            }) && changed.workforce_supply==18 && changed.workforce_active_demand==22,
        "priority shift was falsely described as creating enough workers for every facility");
    std::cout<<"Corresponding synthetic failed city: population18 (screenshot16), Funds26, "
        "Farm0/4, demand22/full starter24, Service100/gap74, upkeep46/400t\n";
}

void infrastructure_is_not_delivered_supply() {
    auto world=paid_guidance_starter();
    const auto before=world.snapshot();
    const auto status=sim::inspect_city_start(world);
    require(status.complete_supply_chain && status.missing_supply_buildings.empty() &&
            status.households.size()==4 && status.households_ready_now==0 &&
            status.households_missing_food==4 && status.households_missing_pottery==4 &&
            status.households_missing_service==4 && status.workforce_used==24,
        "complete paid infrastructure was claimed to prove actual House supply");
    for (const auto& home:status.households)
        require(!home.demand_ready_now && home.demand_ticks_remaining==400 &&
                home.last_demand_status==0 && home.taxes_contributed==0,
            "new House was granted delivered goods, a tax or elapsed demand");
    require(status.maintenance.funds_after_interval==-28 &&
            !sim::starter_budget_warning_for_road_purchase(world,2) && world.snapshot()==before,
        "advisory upkeep turned the legitimate 20-Funds starter into a mandatory warning");
    // Query and road-price projections remain bounded reads at every iteration.
    namespace perf=openemperor::performance;
    perf::reset();perf::set_enabled(true);
    for (int i=0;i<100;++i) {
        (void)sim::inspect_city_start(world);
        (void)world.maintenance_projection();
        (void)sim::starter_budget_warning_for_road_purchase(world,2);
    }
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        require(perf::counter(static_cast<perf::Counter>(i))==0,
            "read-only guidance/road query performed "+std::string(perf::counter_names[i]));
    perf::set_enabled(false);
    require(world.snapshot()==before,"repeated diagnosis changed the prepared starter");
    while (world.ticks()<399) world.tick();
    require(world.taxes_collected_total()==0 && world.maintenance_spent_total()==0 &&
            world.treasury()==20,"read-only guidance advanced or funded the starter");
    world.tick();
    require(world.taxes_collected_total()==75 && world.maintenance_spent_total()==48 &&
            world.treasury()==47,"ordinary tick400 tax-before-upkeep cash flow changed");
    while (world.ticks()<800) world.tick();
    require(world.taxes_collected_total()==175 && world.maintenance_spent_total()==96 &&
            world.treasury()==99,"ordinary tick800 cash flow changed");
    std::cout<<"Paid synthetic starter unchanged: 1280/1300,24/24,tax400=75/upkeep48/Funds47, "
        "taxes800=175/upkeep96/Funds99; 100 reads all12 counters zero\n";
}

void removed_service_retains_current_supply_and_history() {
    auto world=paid_guidance_starter();
    while (world.ticks()<799) world.tick();
    const auto post=building_kind(world,sim::Object::ServicePost);
    for (const auto& b:world.buildings()) if (sim::World::operation_controllable(b.kind))
        require(world.execute(sim::set_building_operation(b.id,false)).accepted,
            "pause before safe service demolition rejected");
    const auto limit=world.ticks()+600;
    while (!world.demolition_status(post).allowed && world.ticks()<limit) world.tick();
    require(world.demolition_status(post).allowed,"ordinary service trip did not return");
    std::optional<sim::BuildingId> ready;
    for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household &&
        world.household_demand_status(b.id).ready) { ready=b.id;break; }
    require(ready.has_value(),"natural retained-goods service fixture has no supplied House");
    const auto coverage=world.building(*ready).service_until_tick;
    const auto funds=world.treasury();
    require(world.execute(sim::demolish_building(post)).accepted &&
            world.treasury()==funds && world.building(*ready).service_until_tick==coverage,
        "safe demolition refunded Funds or removed existing service coverage");
    const auto status=sim::inspect_city_start(world);
    require(status.taxes_have_been_collected && status.taxes_received_total>0 &&
            status.missing_supply_buildings==std::vector{sim::Object::ServicePost} &&
            !status.complete_supply_chain && household(status,*ready).demand_ready_now &&
            household(status,*ready).service_ticks_remaining==coverage-world.ticks() &&
            household(status,*ready).taxes_contributed>0,
        "missing Service Post incorrectly erased retained supply or actual tax history");
    const auto tax_before=world.building(*ready).taxes_paid_total;
    const auto remaining=400-world.building(*ready).demand_progress;
    for (int i=0;i<remaining;++i) world.tick();
    require(world.building(*ready).taxes_paid_total==tax_before+25,
        "retained actual Food/Pottery/Service failed an ordinary dry-House demand");
    // Paused all supply eventually expires; prior taxes never disable diagnosis.
    while (world.ticks()<=coverage) world.tick();
    const auto blocked=sim::inspect_city_start(world);
    require(blocked.taxes_have_been_collected && blocked.households_missing_service>0 &&
            !household(blocked,*ready).service_available &&
            !household(blocked,*ready).demand_ready_now &&
            !sim::starter_budget_warning_for_road_purchase(world,2),
        "historical taxes hid current supply blockers or re-enabled startup modals");
}

void current_demand_matches_real_demand() {
    auto source=paid_guidance_starter();
    while (source.ticks()<1199) source.tick();
    const auto id=building_kind(source,sim::Object::Household);
    require(source.household_demand_status(id).ready,"natural demand fixture lacks supplied goods");
    const auto original=source.snapshot();
    // Valid synthetic incident deadlines isolate the three unchanged demand
    // blockers. Goods, histories and money are all naturally produced.
    for (int variant=0;variant<7;++variant) {
        auto snapshot=original;
        auto& b=snapshot_home(snapshot,id);
        if (variant==1 || variant==3 || variant==4) {
            b.fire_risk=0;b.fire_protection_until_tick=0;
            b.fire_until_tick=snapshot.ticks+(variant==4 ? 1:100);
        }
        if (variant==2 || variant==3 || variant==5) {
            b.health_risk=0;b.health_protection_until_tick=0;
            b.sick_until_tick=snapshot.ticks+(variant==5 ? 1:100);
        }
        if (variant==6) b.service_until_tick=snapshot.ticks+1;
        auto world=sim::World::restore(snapshot,std::vector<std::uint8_t>(48*24,1));
        const auto before=world.building(id);
        const auto report=sim::inspect_city_start(world);
        const bool current_ready=variant==0 || variant==6;
        require(household(report,id).demand_ready_now==current_ready &&
                household(report,id).burning==(variant==1 || variant==3 || variant==4) &&
                household(report,id).sick==(variant==2 || variant==3 || variant==5) &&
                household(report,id).demand_ticks_remaining==1 &&
                world.snapshot()==snapshot,
            "current House condition or read-only incident diagnosis diverged");
        world.tick();
        const bool paid=variant==0 || variant==4 || variant==5;
        const auto& after=world.building(id);
        require(after.taxes_paid_total==before.taxes_paid_total+(paid ? 25:0) &&
                after.consumed_total==before.consumed_total+(paid ? 1:0) &&
                after.food_consumed_total==before.food_consumed_total+(paid ? 1:0) &&
                after.last_demand_status==(paid ? 1:2),
            "shared demand predicate changed actual tax/consumption at incident/service boundary");
    }
    require(source.snapshot()==original,"incident diagnostics changed the source starter");
}

void future_maintenance_boundaries_and_purchase() {
    auto world=guidance_world();
    put(world,sim::CommandType::PlaceWell,10,10);
    const auto well=building_kind(world,sim::Object::Well);
    auto projection=world.maintenance_projection();
    require(projection.horizon_end_tick==400 && projection.next_bill_tick==400 &&
            projection.next_bill_cost==2 && projection.interval_cost==2 &&
            projection.funds_after_interval==1238,"placement created immediate/prorated upkeep");
    while (world.ticks()<100) world.tick();
    put(world,sim::CommandType::PlaceClaySource,14,10);
    const auto clay=building_kind(world,sim::Object::ClaySource);
    require(world.execute(sim::set_building_operation(clay,false)).accepted,
        "maintenance pause fixture rejected");
    require(world.maintenance_projection(299).interval_cost==0 &&
            world.maintenance_projection(300).interval_cost==2,
        "strict future horizon included an out-of-range bill");
    projection=world.maintenance_projection();
    require(projection.installed_rate==10 && projection.next_bill_tick==400 &&
            projection.next_bill_cost==2 && projection.interval_cost==10 &&
            projection.funds_after_interval==1110,"staggered buildings were billed together");
    while (world.ticks()<399) world.tick();
    require(world.maintenance_projection().next_bill_tick==400,"next positive-age bill off by one");
    world.tick();
    projection=world.maintenance_projection();
    require(world.treasury()==1118 && world.maintenance_due_in(well)==0 &&
            projection.next_bill_tick==500 && projection.next_bill_cost==8 &&
            projection.interval_cost==10 && projection.funds_after_interval==1108,
        "already booked bill was counted twice or pause exempted ownership upkeep");
    const auto retained=world.snapshot();
    auto debt=retained;
    debt.treasury=-5;debt.maintenance_spent_total+=1123;
    auto negative=sim::World::restore(debt,std::vector<std::uint8_t>(48*24,1));
    require(negative.maintenance_projection().funds_after_interval==-15 &&
            negative.snapshot()==debt,"debt query wrapped or altered ownership history");
    require(world.execute(sim::demolish_building(clay)).accepted && world.treasury()==1118 &&
            world.maintenance_projection().interval_cost==2,
        "demolition refunded money or retained removed future bills");

    auto purchase=guidance_world();
    for (int i=0;i<6;++i) put_house(purchase,i);
    const auto source=purchase.snapshot();
    const auto warning=sim::starter_budget_warning(purchase,
        {sim::CommandType::PlaceClaySource,{20,2}});
    require(warning && warning->funds_after_purchase==700 &&
            warning->minimum_remaining_building_funds==730 &&
            warning->minimum_remaining_house_funds==0 &&
            warning->maintenance.installed_rate==8 && warning->maintenance.next_bill_tick==400 &&
            warning->maintenance.next_bill_cost==8 && warning->maintenance.interval_cost==8 &&
            warning->maintenance.funds_after_interval==692 &&
            std::ranges::none_of(warning->missing_supply_costs,[](const auto& cost) {
                return cost.kind==sim::Object::ClaySource;
            }) && purchase.snapshot()==source,
        "planned purchase omitted its normal first bill or counted the bought facility twice");
    // A smaller cost-based road preview has identical ownership but lower cash.
    const auto road=sim::starter_budget_warning_for_road_purchase(purchase,52);
    require(road && road->funds_after_purchase==768 && road->missing_building_funds_gap==82 &&
            road->maintenance.installed_rate==0 && road->maintenance.interval_cost==0 &&
            road->maintenance.funds_after_interval==768 && purchase.snapshot()==source,
        "cost-based road warning invented upkeep or changed the World");
}

void arithmetic_limits_and_legacy_maintenance() {
    auto world=guidance_world();
    put(world,sim::CommandType::PlaceWell,10,10);
    auto minimum=world.snapshot();
    minimum.treasury=INT64_MIN;
    minimum.maintenance_spent_total=static_cast<std::uint64_t>(INT64_MAX)+1+1240;
    auto low=sim::World::restore(minimum,std::vector<std::uint8_t>(48*24,1));
    const auto status=sim::inspect_city_start(low);
    require(status.maintenance.interval_cost==2 && !status.maintenance.funds_after_interval &&
            !status.missing_building_funds_gap && !status.construction_funds_gap &&
            low.snapshot()==minimum,"INT64_MIN query wrapped or concealed an arithmetic limit");
    minimum.treasury+=2;minimum.maintenance_spent_total-=2;
    auto boundary=sim::World::restore(minimum,std::vector<std::uint8_t>(48*24,1));
    require(boundary.maintenance_projection().funds_after_interval==INT64_MIN,
        "representable signed-minimum upkeep boundary was rejected");

    auto high=guidance_world();
    put(high,sim::CommandType::PlaceHousehold,0,0);
    const auto removed=building_kind(high,sim::Object::Household);
    require(high.execute(sim::demolish_building(removed)).accepted,"synthetic maximum fixture rejected");
    put(high,sim::CommandType::PlaceWell,10,10);
    auto maximum=high.snapshot();
    const auto credit=static_cast<std::uint64_t>(INT64_MAX-maximum.treasury);
    maximum.treasury=INT64_MAX;maximum.taxes_collected_total+=credit;
    maximum.demolition_history.taxes+=credit;
    high=sim::World::restore(maximum,std::vector<std::uint8_t>(48*24,1));
    require(sim::inspect_city_start(high).missing_building_funds_gap==0 &&
            high.maintenance_projection().funds_after_interval==INT64_MAX-2,
        "INT64_MAX query overflowed subtractive projection");

    auto late=world.snapshot();late.ticks=UINT64_MAX-2400;
    auto end=sim::World::restore(late,std::vector<std::uint8_t>(48*24,1));
    const auto future=end.maintenance_projection();
    require(future.horizon_end_tick==UINT64_MAX-2000 && future.next_bill_tick &&
            *future.next_bill_tick>end.ticks() && future.interval_cost==2 &&
            future.funds_after_interval==1238,"valid late tick lost its bounded future bill");
    const auto exhausted=end.maintenance_projection(2401);
    require(!exhausted.horizon_end_tick && !exhausted.interval_cost &&
            !exhausted.funds_after_interval && end.snapshot()==late,
        "unrepresentable late horizon wrapped or changed World state");
    require(!end.maintenance_projection(UINT64_MAX).horizon_end_tick,
        "unrepresentable query horizon silently wrapped");

    for (const auto profile:{sim::RulesProfile::CityV11,sim::RulesProfile::CityV12,
            sim::RulesProfile::CityV13,sim::RulesProfile::CityV14,sim::RulesProfile::CityV15}) {
        auto legacy=guidance_world(profile);
        put(legacy,sim::CommandType::PlaceClaySource,0,2);
        const auto current=legacy.snapshot();
        const auto p=legacy.maintenance_projection();
        require(!p.applicable && p.installed_rate==0 && !p.next_bill_tick &&
                p.interval_cost==0 && p.funds_after_interval==1180 &&
                !sim::inspect_city_start(legacy).maintenance.applicable &&
                legacy.snapshot()==current,"legacy profile acquired invented ownership costs");
    }
}

} // namespace

int main() {
    try {
        road_only_permission_budget_copy();
        full_policy_budget_copy(false);
        full_policy_budget_copy(true);
        legacy_budget_copy_versions();
        screenshot_budget_blockade();
        workforce_recovery_and_real_tax();
        shrunken_house_recommendation();
        budget_confirmation_contract();
        workforce_reserve_is_guarded();
        post_purchase_reserve_cases();
        exhausted_house_limit_is_explicit();
        corresponding_failed_city_and_staffing();
        infrastructure_is_not_delivered_supply();
        removed_service_retains_current_supply_and_history();
        current_demand_matches_real_demand();
        future_maintenance_boundaries_and_purchase();
        arithmetic_limits_and_legacy_maintenance();
        std::cout<<"City-v11 start diagnosis, recovery, tax and budget warning passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
