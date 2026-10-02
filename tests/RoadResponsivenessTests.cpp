#include "app/RoadDrag.h"
#include "core/PerformanceDiagnostics.h"
#include "simulation/CityStartGuidance.h"
#include "simulation/World.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace sim=openemperor::simulation;
namespace road=openemperor::sandbox_ui;
namespace perf=openemperor::performance;

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

sim::World make_world(sim::RulesProfile profile,int width=228,int height=228) {
    return sim::World(width,height,std::vector<std::uint8_t>(
        static_cast<std::size_t>(width*height),1),profile);
}

void execute(sim::World& world,sim::CommandType type,sim::Cell cell) {
    const auto result=world.execute({type,cell});
    require(result.accepted,std::string("fixture command failed: ")+result.reason);
}

sim::World reference_commit(const sim::World& source,const road::RoadPlan& plan) {
    auto result=source;
    for (const auto cell:plan.cells)
        if (result.object_at(cell)!=sim::Object::Road)
            execute(result,sim::CommandType::PlaceRoad,cell);
    return result;
}

void compare_continuation(sim::World batch,sim::World reference,const char* context) {
    require(batch.snapshot()==reference.snapshot(),std::string(context)+" initial snapshot differs");
    for (int tick=0;tick<240;++tick) {
        batch.tick(); reference.tick();
        if (tick%40==0)
            require(batch.snapshot()==reference.snapshot(),std::string(context)+" continuation differs");
    }
}

void preview_has_no_side_effect_work(sim::RulesProfile profile) {
    auto world=make_world(profile);
    execute(world,sim::CommandType::PlaceClaySource,{2,2});
    execute(world,sim::CommandType::PlacePottery,{6,2});
    execute(world,sim::CommandType::PlaceWarehouse,{10,2});
    perf::reset(); perf::set_enabled(true);
    const auto before=world.snapshot();
    const auto plan=road::plan_road(world,{0,6},{29,6});
    require(plan.valid && plan.cells.size()==30 && plan.new_road_count==30,
            "30-cell preview was not valid");
    require(world.snapshot()==before,"preview changed authoritative World");
    require(perf::counter(perf::Counter::WorldCopies)==0 &&
            perf::counter(perf::Counter::WorldRestores)==0 &&
            perf::counter(perf::Counter::WorldExecutes)==0 &&
            perf::counter(perf::Counter::RouteRefreshes)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0 &&
            perf::counter(perf::Counter::FileReads)==0 &&
            perf::counter(perf::Counter::FileWrites)==0 &&
            perf::counter(perf::Counter::AssetDecodes)==0 &&
            perf::counter(perf::Counter::TextureUploads)==0,
            "preview performed copy, execute, route, I/O, decode, or upload work");
    perf::set_enabled(false);
}

void batch_lengths_and_atomicity(sim::RulesProfile profile) {
    for (const int length:{1,10,30,256}) {
        auto world=make_world(profile);
        const auto end=length==256 ? sim::Cell{127,178}:sim::Cell{length-1,50};
        const auto plan=road::plan_road(world,{0,50},end);
        require(plan.valid && plan.cells.size()==static_cast<std::size_t>(length),
                "road length plan failed");
        auto reference=reference_commit(world,plan);
        perf::reset(); perf::set_enabled(true);
        std::string reason;
        require(road::commit_road(world,plan,reason),"road batch commit failed: "+reason);
        require(perf::counter(perf::Counter::WorldCopies)==1,
                "changed batch did not use exactly one transaction copy");
        require(perf::counter(perf::Counter::RouteRefreshes)<=1,
                "changed batch refreshed routes more than once");
        perf::set_enabled(false);
        compare_continuation(std::move(world),std::move(reference),"road length batch");
    }
}

void active_city_commit(sim::RulesProfile profile) {
    auto world=make_world(profile,64,16);
    execute(world,sim::CommandType::PlaceClaySource,{2,2});
    execute(world,sim::CommandType::PlacePottery,{6,2});
    execute(world,sim::CommandType::PlaceWarehouse,{10,2});
    execute(world,sim::CommandType::PlaceFarm,{14,2});
    if (sim::market_profile(profile))
        execute(world,sim::CommandType::PlaceMarket,{19,3});
    execute(world,sim::CommandType::PlaceServicePost,{21,3});
    execute(world,sim::CommandType::PlaceHousehold,{24,2});
    for (int x=2;x<=27;++x) execute(world,sim::CommandType::PlaceRoad,{x,4});
    if (sim::fire_profile(profile)) {
        execute(world,sim::CommandType::PlaceFireWatch,{27,3});
        require(world.execute(sim::set_building_workforce_priority(*world.building_owner_at({27,3}),
            sim::WorkforcePriority::High)).accepted,"Watch priority failed");
    }
    for (int i=0;i<50;++i) world.tick();
    const auto plan=road::plan_road(world,{1,9},{30,9});
    require(plan.valid,"active-city 30-cell plan failed");
    auto reference=reference_commit(world,plan);
    perf::reset(); perf::set_enabled(true);
    std::string reason;
    require(road::commit_road(world,plan,reason),"active-city batch failed");
    require(perf::counter(perf::Counter::WorldCopies)==1 &&
            perf::counter(perf::Counter::RouteRefreshes)==1,
            "active-city batch did not use one copy and one refresh");
    perf::set_enabled(false);
    compare_continuation(std::move(world),std::move(reference),
        profile==sim::RulesProfile::CityV10 ? "City-v10 active batch":"City-v11-v3 active batch");
}

void mixtures_and_failures() {
    auto mixture=make_world(sim::RulesProfile::CityV10,80,8);
    execute(mixture,sim::CommandType::PlaceRoad,{2,3});
    execute(mixture,sim::CommandType::PlaceRoad,{4,3});
    const auto plan=road::plan_road(mixture,{1,3},{5,3});
    require(plan.valid && plan.new_road_count==3 && plan.total_cost==3*sim::Rules::road_cost,
            "existing/new mixture projection wrong");
    auto reference=reference_commit(mixture,plan);
    std::string reason;
    require(road::commit_road(mixture,plan,reason) && mixture.snapshot()==reference.snapshot(),
            "mixed road batch differs from reference");

    perf::reset(); perf::set_enabled(true);
    const auto existing=road::plan_road(mixture,{1,3},{5,3});
    const auto stable=mixture.snapshot();
    require(road::commit_road(mixture,existing,reason) && mixture.snapshot()==stable &&
            perf::counter(perf::Counter::WorldCopies)==0 &&
            perf::counter(perf::Counter::RouteRefreshes)==0,
            "all-existing batch copied, refreshed, or changed World");
    perf::set_enabled(false);

    auto poor=make_world(sim::RulesProfile::CityV10);
    for (int i=0;i<245;++i) execute(poor,sim::CommandType::PlaceRoad,{i%228,i/228});
    const auto poor_before=poor.snapshot();
    const auto unaffordable=road::plan_road(poor,{0,50},{127,178});
    require(!unaffordable.valid && unaffordable.reason=="Not enough money" &&
            poor.snapshot()==poor_before,"aggregate road budget was not rejected atomically");

    auto blocked=make_world(sim::RulesProfile::CityV10,80,8);
    execute(blocked,sim::CommandType::PlaceHousehold,{30,3});
    const auto blocked_before=blocked.snapshot();
    const auto blocked_plan=road::plan_road(blocked,{1,3},{30,3});
    require(!blocked_plan.valid && blocked.snapshot()==blocked_before,
            "blocked final cell changed World");

    auto footprint=make_world(sim::RulesProfile::CityV10,80,8);
    execute(footprint,sim::CommandType::PlacePottery,{20,2});
    const auto footprint_before=footprint.snapshot();
    const auto footprint_plan=road::plan_road(footprint,{1,3},{30,3});
    require(!footprint_plan.valid && footprint.snapshot()==footprint_before,
            "2x2 foundation was not rejected by pure preview");

    auto changed=make_world(sim::RulesProfile::CityV11,80,8);
    const auto stale=road::plan_road(changed,{1,3},{10,3});
    execute(changed,sim::CommandType::PlaceHousehold,{10,3});
    const auto changed_before=changed.snapshot();
    require(!road::commit_road(changed,stale,reason) && changed.snapshot()==changed_before,
            "commit did not revalidate a changed World");
}

void counter_overflow() {
    auto world=make_world(sim::RulesProfile::CityV10,20,5);
    auto snapshot=world.snapshot();
    snapshot.command_sequence=std::numeric_limits<std::uint64_t>::max()-1;
    auto near_limit=sim::World::restore(snapshot,std::vector<std::uint8_t>(100,1));
    const std::array cells{sim::Cell{1,1},sim::Cell{2,1}};
    const auto result=near_limit.validate_road_batch(cells);
    require(!result.accepted && std::string(result.reason)=="Sandbox command counter exhausted",
            "aggregate command counter overflow was not rejected");
}

void city_road_warning_is_pure(sim::RulesProfile profile) {
    auto world=make_world(profile,80,8);
    std::vector<sim::Command> commands;
    for (int y=0;y<3;++y) for (int x=0;x<80 && commands.size()<200;++x)
        commands.push_back({sim::CommandType::PlaceRoad,{x,y}});
    const auto reference=sim::starter_budget_warning(world,commands);
    perf::reset(); perf::set_enabled(true);
    const auto warning=sim::starter_budget_warning_for_road_purchase(world,400);
    require(warning.has_value(),"City-v11 starter road purchase should warn");
    require(reference.has_value() && warning->purchase_cost==reference->purchase_cost &&
            warning->funds_after_purchase==reference->funds_after_purchase &&
            warning->minimum_remaining_start_cost==reference->minimum_remaining_start_cost &&
            warning->additional_houses_needed==reference->additional_houses_needed &&
            warning->remaining_missing_supply_buildings==reference->remaining_missing_supply_buildings,
            "pure road warning differs from the prior hypothetical-command result");
    require(perf::counter(perf::Counter::WorldCopies)==0 &&
            perf::counter(perf::Counter::WorldRestores)==0 &&
            perf::counter(perf::Counter::WorldExecutes)==0 &&
            perf::counter(perf::Counter::RouteRefreshes)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0,
            "City-v11 road warning used a hypothetical World or routing");
    perf::set_enabled(false);
}

void report_preview_timing() {
    auto city10=make_world(sim::RulesProfile::CityV10,228,228);
    auto city11=make_world(sim::RulesProfile::CityV11,228,228);
    const auto measure=[](const sim::World& world) {
        std::uint64_t total=0;
        for (int i=0;i<100;++i) {
            const auto before=std::chrono::steady_clock::now();
            const auto plan=road::plan_road(world,{90,120},{119,120});
            require(plan.valid,"timing preview invalid");
            total+=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now()-before).count());
        }
        return static_cast<double>(total)/100.0/1'000'000.0;
    };
    std::cout<<"pure 30-cell preview mean: City-v10 "<<measure(city10)
             <<" ms, City-v11-v3 "<<measure(city11)<<" ms\n";
}
}

int main() {
    try {
        preview_has_no_side_effect_work(sim::RulesProfile::CityV10);
        preview_has_no_side_effect_work(sim::RulesProfile::CityV11);
        preview_has_no_side_effect_work(sim::RulesProfile::CityV12);
        batch_lengths_and_atomicity(sim::RulesProfile::CityV10);
        batch_lengths_and_atomicity(sim::RulesProfile::CityV11);
        batch_lengths_and_atomicity(sim::RulesProfile::CityV12);
        active_city_commit(sim::RulesProfile::CityV10);
        active_city_commit(sim::RulesProfile::CityV11);
        active_city_commit(sim::RulesProfile::CityV12);
        mixtures_and_failures();
        counter_overflow();
        city_road_warning_is_pure(sim::RulesProfile::CityV11);
        city_road_warning_is_pure(sim::RulesProfile::CityV12);
        report_preview_timing();
        std::cout<<"responsive road preview and transactional batches passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
