#include "persistence/SandboxSave.h"
#include "simulation/World.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace sim=openemperor::simulation;
namespace persistence=openemperor::persistence;
namespace fs=std::filesystem;

namespace {
constexpr int width=32;
constexpr int height=32;

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

sim::CommandResult place(sim::World& world,sim::CommandType type,sim::Cell cell) {
    const auto result=world.execute({type,cell});
    require(result.accepted && result.changed,"placement failed: "+std::string(result.reason));
    return result;
}

sim::World city(std::uint32_t version) {
    sim::World world(width,height,std::vector<std::uint8_t>(width*height,1),
                     sim::RulesProfile::CityV11,version);
    place(world,sim::CommandType::PlaceClaySource,{1,1});
    place(world,sim::CommandType::PlacePottery,{4,1});
    place(world,sim::CommandType::PlaceWarehouse,{7,1});
    place(world,sim::CommandType::PlaceFarm,{10,1});
    place(world,sim::CommandType::PlaceServicePost,{12,1});
    place(world,sim::CommandType::PlaceMarket,{14,1});
    place(world,sim::CommandType::PlaceHousehold,{1,5});
    place(world,sim::CommandType::PlaceHousehold,{4,5});
    place(world,sim::CommandType::PlaceHousehold,{7,5});
    return world;
}

sim::World connected_city() {
    sim::World world(width,8,std::vector<std::uint8_t>(width*8,1),
                     sim::RulesProfile::CityV11,3);
    place(world,sim::CommandType::PlaceClaySource,{0,2});
    place(world,sim::CommandType::PlacePottery,{0,5});
    place(world,sim::CommandType::PlaceWarehouse,{3,2});
    place(world,sim::CommandType::PlaceFarm,{6,5});
    place(world,sim::CommandType::PlaceMarket,{10,5});
    place(world,sim::CommandType::PlaceServicePost,{20,2});
    for (int x=0;x<=20;++x) place(world,sim::CommandType::PlaceRoad,{x,4});
    place(world,sim::CommandType::PlaceRoad,{20,3});
    for (const auto cell:{sim::Cell{13,2},sim::Cell{13,5},sim::Cell{16,2},sim::Cell{16,5}})
        place(world,sim::CommandType::PlaceHousehold,cell);
    return world;
}

sim::BuildingId by_kind(const sim::World& world,sim::Object kind) {
    for (const auto& building:world.buildings()) if (building.kind==kind) return building.id;
    throw std::runtime_error("building kind not found");
}

void staffing_and_commands() {
    auto world=city(3);
    const auto clay=by_kind(world,sim::Object::ClaySource);
    const auto pottery=by_kind(world,sim::Object::Pottery);
    const auto warehouse=by_kind(world,sim::Object::Warehouse);
    const auto farm=by_kind(world,sim::Object::Farm);
    const auto service=by_kind(world,sim::Object::ServicePost);
    const auto market=by_kind(world,sim::Object::Market);
    require(world.workforce_supply()==18 && world.workforce_required()==22 &&
            world.active_workforce_required()==22 && world.workforce_used()==18,
            "initial workforce totals differ");
    require(world.building_staffed(clay) && world.building_staffed(pottery) &&
            world.building_staffed(warehouse) && world.building_staffed(farm) &&
            world.building_staffed(service) && !world.building_staffed(market),
            "normal priority did not preserve stable ID order");

    const auto before_road=world.road_revision();
    const auto before_refresh=world.route_refresh_count();
    const auto before_funds=world.treasury();
    const auto before_tick=world.ticks();
    const auto before_next=world.next_building_id();
    auto changed=world.execute(sim::set_building_workforce_priority(market,
        sim::WorkforcePriority::High));
    require(changed.accepted && changed.changed && world.building_staffed(market) &&
            !world.building_staffed(farm) && world.building_staffed(service),
            "priority did not outrank stable ID while retaining skip allocation");
    require(world.road_revision()==before_road && world.route_refresh_count()==before_refresh &&
            world.treasury()==before_funds && world.ticks()==before_tick &&
            world.next_building_id()==before_next,"priority command changed unrelated state");

    require(world.execute(sim::set_building_operation(clay,false)).changed &&
            world.execute(sim::set_building_operation(pottery,false)).changed,
            "pause commands failed");
    require(world.workers_assigned(clay)==0 && world.workers_assigned(pottery)==0 &&
            world.active_workforce_required()==12 && world.workforce_used()==12 &&
            world.building_staffed(farm) && world.building_staffed(service) &&
            world.building_staffed(market),"paused allocation totals differ");
    const auto unchanged=world.execute(sim::set_building_operation(clay,false));
    require(unchanged.accepted && !unchanged.changed,"identical operation command was not unchanged");

    const auto invalid_before=world.canonical_state();
    const auto invalid=world.execute(sim::set_building_workforce_priority(
        static_cast<sim::BuildingId>(999999),sim::WorkforcePriority::High));
    require(!invalid.accepted && world.canonical_state()==invalid_before,
            "invalid building operation command mutated the World");
    const auto bad=world.execute(sim::set_building_workforce_priority(farm,
        static_cast<sim::WorkforcePriority>(99)));
    require(!bad.accepted,"invalid priority was accepted");

    auto old=city(2);
    require(!old.execute(sim::set_building_operation(by_kind(old,sim::Object::Farm),false)).accepted,
            "City-v11-v2 silently enabled operation controls");
}

void paused_progress_and_resume() {
    auto world=city(3);
    const auto clay=by_kind(world,sim::Object::ClaySource);
    const auto farm=by_kind(world,sim::Object::Farm);
    for (int i=0;i<11;++i) world.tick();
    const int clay_progress=world.building(clay).progress;
    const int farm_progress=world.building(farm).progress;
    require(world.execute(sim::set_building_operation(clay,false)).changed &&
            world.execute(sim::set_building_operation(farm,false)).changed,"pause failed");
    for (int i=0;i<20;++i) world.tick();
    require(world.building(clay).progress==clay_progress &&
            world.building(farm).progress==farm_progress,
            "paused source progress changed");
    require(world.execute(sim::set_building_operation(clay,true)).changed &&
            world.execute(sim::set_building_operation(farm,true)).changed,"resume failed");
    world.tick();
    require(world.building(clay).progress==clay_progress+1 &&
            world.building(farm).progress==farm_progress+1,
            "resumed source did not continue from stored progress");
}

void recipes_and_couriers_finish() {
    auto world=connected_city();
    const auto pottery=by_kind(world,sim::Object::Pottery);
    const auto market=by_kind(world,sim::Object::Market);
    const auto service=by_kind(world,sim::Object::ServicePost);
    const auto recipe_deadline=world.ticks()+3000;
    while ((world.building(pottery).active_recipe_clay==0 ||
            world.building(pottery).progress==0) && world.ticks()<recipe_deadline) world.tick();
    require(world.building(pottery).active_recipe_clay==sim::Rules::pottery_recipe_clay,
            "connected fixture did not start a Pottery recipe");
    const int recipe_progress=world.building(pottery).progress;
    const int recipe_clay=world.building(pottery).active_recipe_clay;
    const auto completed=world.building(pottery).recipes_completed;
    require(world.execute(sim::set_building_operation(pottery,false)).changed,
            "Pottery pause failed");
    for (int i=0;i<100;++i) world.tick();
    require(world.building(pottery).progress==recipe_progress &&
            world.building(pottery).active_recipe_clay==recipe_clay &&
            world.building(pottery).recipes_completed==completed,
            "paused active recipe changed or lost input");
    auto restored=sim::World::restore(world.snapshot(),std::vector<std::uint8_t>(width*8,1));
    require(restored.snapshot()==world.snapshot(),"paused recipe snapshot did not restore exactly");
    require(restored.execute(sim::set_building_operation(pottery,true)).changed,
            "Pottery resume failed");
    const auto completion_deadline=restored.ticks()+200;
    while (restored.building(pottery).recipes_completed==completed &&
           restored.ticks()<completion_deadline) restored.tick();
    require(restored.building(pottery).recipes_completed==completed+1,
            "resumed Pottery recipe did not complete");

    sim::CourierId distributor{};
    bool moving=false;
    const auto distributor_deadline=restored.ticks()+3000;
    while (!moving && restored.ticks()<distributor_deadline) {
        for (const auto& courier:restored.couriers())
            if (courier.owner==market && courier.phase!=sim::CourierPhase::IdleAtWorkshop) {
                distributor=courier.id; moving=true; break;
            }
        if (!moving) restored.tick();
    }
    require(moving,"Market distributor never started");
    require(restored.execute(sim::set_building_operation(market,false)).changed,
            "Market pause failed");
    require(restored.courier_dispatch_status(distributor).status==
                sim::CourierDispatchStatus::AlreadyMoving,
            "paused Market erased the active trip status");
    const auto finish_deadline=restored.ticks()+1000;
    while (restored.courier(distributor).phase!=sim::CourierPhase::IdleAtWorkshop &&
           restored.ticks()<finish_deadline) restored.tick();
    require(restored.courier(distributor).phase==sim::CourierPhase::IdleAtWorkshop,
            "paused Market trip did not finish its return");
    for (int i=0;i<100;++i) restored.tick();
    require(restored.courier(distributor).phase==sim::CourierPhase::IdleAtWorkshop &&
            restored.courier_dispatch_status(distributor).status==
                sim::CourierDispatchStatus::OperationPaused,
            "paused Market dispatched again after returning");

    sim::CourierId inbound{};
    int cargo=0;
    const auto inbound_deadline=restored.ticks()+3000;
    while (cargo==0 && restored.ticks()<inbound_deadline) {
        for (const auto& courier:restored.couriers())
            if ((courier.role==sim::CourierRole::MarketPotteryInbound ||
                 courier.role==sim::CourierRole::MarketFoodInbound) &&
                courier.target==market && courier.phase==sim::CourierPhase::ToWarehouse &&
                courier.cargo>0) {
                inbound=courier.id; cargo=courier.cargo; break;
            }
        if (cargo==0) restored.tick();
    }
    require(cargo>0,"no inbound delivery targeted the paused Market");
    const auto good=restored.courier(inbound).good;
    const int stock_before=good==sim::Good::Food ? restored.building(market).food_stock:
        restored.building(market).pottery_stock;
    const auto arrival_deadline=restored.ticks()+1000;
    while (restored.courier(inbound).phase==sim::CourierPhase::ToWarehouse &&
           restored.ticks()<arrival_deadline) restored.tick();
    const int stock_after=good==sim::Good::Food ? restored.building(market).food_stock:
        restored.building(market).pottery_stock;
    require(stock_after==stock_before+cargo,
            "paused Market rejected or lost an inbound delivery");

    sim::CourierId service_courier{};
    bool service_moving=false;
    for (const auto& courier:restored.couriers()) if (courier.owner==service) {
        service_courier=courier.id; break;
    }
    const auto service_deadline=restored.ticks()+3000;
    while (!service_moving && restored.ticks()<service_deadline) {
        service_moving=restored.courier(service_courier).phase==sim::CourierPhase::ToWarehouse;
        if (!service_moving) restored.tick();
    }
    require(service_moving && restored.execute(sim::set_building_operation(service,false)).changed,
            "Service pause fixture did not capture an active visit");
    const auto target=restored.courier(service_courier).target;
    const auto coverage_before=restored.building(target).service_until_tick;
    const auto service_arrival_deadline=restored.ticks()+1000;
    while (restored.courier(service_courier).phase==sim::CourierPhase::ToWarehouse &&
           restored.ticks()<service_arrival_deadline) restored.tick();
    require(restored.building(target).service_until_tick>coverage_before,
            "finishing paused Service visit did not extend coverage on actual arrival");
}

void persistence_and_upgrade() {
    const auto root=fs::canonical(fs::temp_directory_path())/
        ("openemperor-operations-"+std::to_string(std::random_device{}()));
    fs::create_directories(root/"data/Cities");
    const auto map=fs::path("Cities/Synthetic.map");
    { std::ofstream out(root/"data"/map,std::ios::binary); out<<"synthetic-map"; }
    const std::vector<std::uint8_t> mask(width*height,1);
    try {
        auto world=city(3);
        const auto farm=by_kind(world,sim::Object::Farm);
        const auto market=by_kind(world,sim::Object::Market);
        require(world.execute(sim::set_building_operation(farm,false)).changed,"save pause failed");
        require(world.execute(sim::set_building_workforce_priority(market,
            sim::WorkforcePriority::High)).changed,"save priority failed");
        const auto document=persistence::make_document(root/"data",map,mask,world);
        const auto path=root/"city-v11-v3.json";
        persistence::write_save(path,document,root/"data",mask);
        const auto read=persistence::read_save(path);
        require(read.source_schema_version==12 && read.world.rule_version==3,
                "City-v11-v3 did not use schema 12");
        const auto restored=persistence::restore_save(read,root/"data",mask);
        require(restored.snapshot()==world.snapshot(),"schema-12 continuation state differs");

        auto v2=city(2);
        for (int i=0;i<17;++i) v2.tick();
        const auto v2_doc=persistence::make_document(root/"data",map,mask,v2);
        const auto v2_before=v2_doc.world;
        auto upgraded=persistence::upgrade_city_v11_v2_to_v3(v2_doc);
        require(v2_doc.world==v2_before && upgraded.world.rule_version==3,
                "explicit upgrade mutated its source");
        auto common=upgraded.world;
        common.rule_version=2;
        require(common==v2_before,"upgrade changed authoritative state beyond defaults and version");
        const auto copy=root/"city-v11-v3-copy.json";
        persistence::write_save(copy,upgraded,root/"data",mask);
        require(persistence::read_save(copy).source_schema_version==12,
                "upgraded copy was not readable schema 12");
    } catch (...) { fs::remove_all(root); throw; }
    fs::remove_all(root);
}

void defaults_and_determinism() {
    auto v2=city(2);
    auto v3=city(3);
    for (int tick=0;tick<2000;++tick) { v2.tick(); v3.tick(); }
    auto a=v2.snapshot(),b=v3.snapshot();
    b.rule_version=2;
    require(a==b,"defaults-only City-v11-v3 behavior differs from v2");

    auto first=city(3),second=city(3);
    const auto first_farm=by_kind(first,sim::Object::Farm);
    const auto second_farm=by_kind(second,sim::Object::Farm);
    for (int tick=0;tick<20000;++tick) {
        if (tick==500 || tick==2500) {
            const bool enabled=tick!=500;
            require(first.execute(sim::set_building_operation(first_farm,enabled)).accepted &&
                    second.execute(sim::set_building_operation(second_farm,enabled)).accepted,
                    "deterministic operation plan failed");
        }
        first.tick(); second.tick();
        if (tick%257==0) {
            require(first.snapshot()==second.snapshot(),"v3 worlds diverged");
            require(first.workforce_used()<=first.workforce_supply(),"staffing exceeded population");
            if (!first.building(first_farm).operating_enabled)
                require(first.workers_assigned(first_farm)==0,"paused farm received workers");
        }
    }
}
}

int main() {
    try {
        require(sim::current_rule_version(sim::RulesProfile::CityV11)==3,
                "new City-v11 Worlds enabled v4 before native acceptance");
        staffing_and_commands();
        paused_progress_and_resume();
        recipes_and_couriers_finish();
        persistence_and_upgrade();
        defaults_and_determinism();
        std::cout<<"City-v11 operation controls: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
