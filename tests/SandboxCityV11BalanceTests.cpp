#include "persistence/SandboxSave.h"
#include "simulation/World.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;

constexpr int width=32;

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

struct SaveFixture {
    fs::path root,relative="Cities/Synthetic.map",target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-v11-balance-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities");
        fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)
            <<"independently authored City-v11 balance map";
        target=root/"saves/checkpoint.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
};

void roundtrip_checkpoint(const sim::World& source,int height,SaveFixture& files,
                          const char* label) {
    const std::vector<std::uint8_t> buildable(
        static_cast<std::size_t>(width)*static_cast<std::size_t>(height),1);
    save::write_save(files.target,
        save::make_document(files.root/"data",files.relative,buildable,source),
        files.root/"data",buildable);
    const auto document=save::read_save(files.target);
    require(document.source_schema_version==11 && document.world.rule_version==2,
        std::string(label)+" did not persist schema 11 / rules v2");
    auto original=sim::World::restore(source.snapshot(),buildable);
    auto restored=save::restore_save(document,files.root/"data",buildable);
    require(restored.snapshot()==source.snapshot(),
        std::string(label)+" changed across immediate restore");
    for (int tick=1;tick<=200;++tick) {
        original.tick(); restored.tick();
        if (tick%50==0)
            require(original.snapshot()==restored.snapshot(),
                std::string(label)+" diverged after restore");
    }
}

void put(sim::World& world,sim::CommandType type,int x,int y) {
    const auto result=world.execute({type,{x,y}});
    require(result.accepted,"command rejected at "+std::to_string(x)+","+
        std::to_string(y)+": "+result.reason);
}

void build_starter(sim::World& world,bool staged,bool houses_first,bool service_first) {
    const auto houses=[&] {
        for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}}) {
            put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
            if (staged && cell!=sim::Cell{9,5})
                for (int tick=0;tick<100;++tick) world.tick();
        }
    };
    if (houses_first) houses();
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,4,5);
    if (service_first) put(world,sim::CommandType::PlaceServicePost,5,5);
    put(world,sim::CommandType::PlaceMarket,3,5);
    if (!service_first) put(world,sim::CommandType::PlaceServicePost,5,5);
    for (int x=0;x<=14;++x) put(world,sim::CommandType::PlaceRoad,x,4);
    if (!houses_first) houses();
}

sim::World starter(bool staged,bool houses_first=false,bool service_first=false,
                   std::uint32_t rule_version=1) {
    sim::World world(width,8,std::vector<std::uint8_t>(width*8,1),
        sim::RulesProfile::CityV11,rule_version);
    build_starter(world,staged,houses_first,service_first);
    return world;
}

sim::World played_goal() {
    sim::World world(width,16,std::vector<std::uint8_t>(width*16,1),
        sim::RulesProfile::CityV11,2);
    build_starter(world,false,false,false);
    while (world.treasury()<1'800 && world.ticks()<10'000) world.tick();
    require(world.treasury()>=1'800,"v2 starter did not finance goal expansion");
    put(world,sim::CommandType::PlaceRoad,15,4);
    put(world,sim::CommandType::PlaceHousehold,12,2);
    put(world,sim::CommandType::PlaceMarket,14,5);
    put(world,sim::CommandType::PlaceClaySource,0,9);
    put(world,sim::CommandType::PlacePottery,0,12);
    put(world,sim::CommandType::PlaceWarehouse,3,9);
    put(world,sim::CommandType::PlaceFarm,4,12);
    put(world,sim::CommandType::PlaceMarket,3,12);
    put(world,sim::CommandType::PlaceServicePost,5,12);
    put(world,sim::CommandType::PlaceMarket,14,12);
    for (int x=0;x<=15;++x) put(world,sim::CommandType::PlaceRoad,x,11);
    for (const auto cell:{sim::Cell{6,9},sim::Cell{6,12},sim::Cell{9,9},
                             sim::Cell{9,12},sim::Cell{12,9}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    const auto expansion_tick=world.ticks();
    while (!world.settlement_goal_reached() && world.ticks()<expansion_tick+30'000) world.tick();
    require(world.settlement_goal_reached(),"paid v2 city did not reach its goal");
    std::cout<<"V2 GOAL tick="<<world.ticks()<<" population="<<world.total_population()
             <<" treasury="<<world.treasury()<<" spent="<<world.construction_spent_total()<<'\n';
    return world;
}

void wait_for_funds(sim::World& world,std::int64_t funds,std::uint64_t limit,const char* label) {
    const auto deadline=world.ticks()+limit;
    while (world.treasury()<funds && world.ticks()<deadline) world.tick();
    require(world.treasury()>=funds,std::string(label)+" did not earn required funds");
}

void build_full_district(sim::World& world,int base_y,bool initial_starter) {
    const int road_y=base_y+8;
    if (initial_starter) {
        put(world,sim::CommandType::PlaceClaySource,12,base_y+12);
        put(world,sim::CommandType::PlacePottery,15,base_y+12);
        put(world,sim::CommandType::PlaceWarehouse,18,base_y+12);
        put(world,sim::CommandType::PlaceFarm,9,base_y+12);
        put(world,sim::CommandType::PlaceMarket,9,base_y+9);
        put(world,sim::CommandType::PlaceServicePost,9,base_y+7);
        for (int x=0;x<=16;++x) put(world,sim::CommandType::PlaceRoad,x,road_y);
        put(world,sim::CommandType::PlaceRoad,11,base_y+9);
        put(world,sim::CommandType::PlaceRoad,11,base_y+10);
        for (int x=9;x<=20;++x) put(world,sim::CommandType::PlaceRoad,x,base_y+11);
        for (int x:{3,6,12,15}) put(world,sim::CommandType::PlaceHousehold,x,base_y+6);
        return;
    }
    put(world,sim::CommandType::PlaceClaySource,12,base_y+12);
    put(world,sim::CommandType::PlacePottery,15,base_y+12);
    put(world,sim::CommandType::PlaceWarehouse,18,base_y+12);
    put(world,sim::CommandType::PlaceFarm,9,base_y+12);
    put(world,sim::CommandType::PlaceMarket,9,base_y+9);
    put(world,sim::CommandType::PlaceMarket,10,base_y+9);
    put(world,sim::CommandType::PlaceServicePost,9,base_y+7);
    put(world,sim::CommandType::PlaceClaySource,21,base_y+12);
    put(world,sim::CommandType::PlacePottery,24,base_y+12);
    for (int x=0;x<=16;++x) put(world,sim::CommandType::PlaceRoad,x,road_y);
    put(world,sim::CommandType::PlaceRoad,11,base_y+9);
    put(world,sim::CommandType::PlaceRoad,11,base_y+10);
    for (int x=9;x<=26;++x) put(world,sim::CommandType::PlaceRoad,x,base_y+11);
    for (int x:{0,3,6,12,15}) {
        put(world,sim::CommandType::PlaceHousehold,x,base_y+6);
        put(world,sim::CommandType::PlaceHousehold,x,base_y+9);
    }
}

sim::World played_maximum() {
    constexpr int full_height=32;
    sim::World world(width,full_height,std::vector<std::uint8_t>(width*full_height,1),
        sim::RulesProfile::CityV11,2);
    build_full_district(world,0,true);
    require(world.ticks()==0 && world.construction_spent_total()==1'232,
        "maximum-city starter was not built at tick zero with paid commands");
    wait_for_funds(world,1'000,10'000,"first full district");
    put(world,sim::CommandType::PlaceClaySource,21,12);
    put(world,sim::CommandType::PlacePottery,24,12);
    put(world,sim::CommandType::PlaceMarket,10,9);
    for (int x=21;x<=26;++x) put(world,sim::CommandType::PlaceRoad,x,11);
    put(world,sim::CommandType::PlaceHousehold,0,6);
    for (int x:{0,3,6,12,15}) put(world,sim::CommandType::PlaceHousehold,x,9);
    wait_for_funds(world,2'164,20'000,"second full district");
    build_full_district(world,16,false);
    require(world.buildings().size()==38 && world.couriers().size()==22 &&
            world.construction_spent_total()==4'328,
        "paid maximum city did not reach 38 buildings, 22 couriers, or expected cost");
    return world;
}

struct WindowHouse {
    sim::BuildingId id{};
    std::uint64_t fulfilled=0,missed=0,pottery_arrivals=0,food_arrivals=0;
    std::uint64_t inactive_service_ticks=0;
};

std::vector<WindowHouse> measure_window(sim::World& world,std::uint64_t ticks) {
    struct Start {
        sim::BuildingId id{};
        std::uint64_t fulfilled=0,missed=0;
        std::uint64_t pottery_total=0,food_total=0;
    };
    std::vector<Start> starts;
    for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household)
        starts.push_back({b.id,b.fulfilled_demand,b.missed_demand,
            static_cast<std::uint64_t>(b.pottery_stock)+b.consumed_total,
            static_cast<std::uint64_t>(b.food_stock)+b.food_consumed_total});
    std::vector<std::uint64_t> service_gaps(starts.size());
    for (std::uint64_t tick=0;tick<ticks;++tick) {
        world.tick();
        for (std::size_t i=0;i<starts.size();++i)
            if (!world.household_service_active(starts[i].id)) ++service_gaps[i];
    }
    std::vector<WindowHouse> result;
    for (std::size_t i=0;i<starts.size();++i) {
        const auto& start=starts[i];
        const auto& home=world.building(start.id);
        const auto pottery=static_cast<std::uint64_t>(home.pottery_stock)+home.consumed_total;
        const auto food=static_cast<std::uint64_t>(home.food_stock)+home.food_consumed_total;
        result.push_back({home.id,home.fulfilled_demand-start.fulfilled,
            home.missed_demand-start.missed,pottery-start.pottery_total,
            food-start.food_total,service_gaps[i]});
    }
    return result;
}

void require_sustained(const std::vector<WindowHouse>& window,std::uint64_t demands,
                       const char* label) {
    for (const auto& home:window)
        require(home.fulfilled==demands && home.missed==0 && home.inactive_service_ticks==0,
            std::string(label)+" house "+std::to_string(static_cast<unsigned>(home.id))+
            " was not continuously supplied: fulfilled="+std::to_string(home.fulfilled)+
            " missed="+std::to_string(home.missed)+" arrivals="+
            std::to_string(home.pottery_arrivals)+"/"+std::to_string(home.food_arrivals)+
            " service_gap_ticks="+std::to_string(home.inactive_service_ticks));
}

sim::World recovery_city() {
    sim::World world(width,8,std::vector<std::uint8_t>(width*8,1),
        sim::RulesProfile::CityV11,2);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    put(world,sim::CommandType::PlacePottery,0,5);
    put(world,sim::CommandType::PlaceWarehouse,3,2);
    put(world,sim::CommandType::PlaceFarm,6,5);
    put(world,sim::CommandType::PlaceMarket,10,5);
    put(world,sim::CommandType::PlaceServicePost,20,2);
    for (int x=0;x<=20;++x) put(world,sim::CommandType::PlaceRoad,x,4);
    put(world,sim::CommandType::PlaceRoad,20,3);
    for (const auto cell:{sim::Cell{13,2},sim::Cell{13,5},sim::Cell{16,2},sim::Cell{16,5}})
        put(world,sim::CommandType::PlaceHousehold,cell.x,cell.y);
    for (int tick=0;tick<3'000;++tick) world.tick();
    require_sustained(measure_window(world,1'600),4,"recovery control");
    return world;
}

void remove_when_free(sim::World& world,sim::Cell cell) {
    for (int tick=0;tick<2'000;++tick) {
        const auto result=world.execute({sim::CommandType::RemoveRoad,cell});
        if (result.accepted) return;
        world.tick();
    }
    throw std::runtime_error("road remained protected during bounded removal wait");
}

std::uint64_t total_fulfilled(const sim::World& world) {
    std::uint64_t result=0;
    for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household)
        result+=b.fulfilled_demand;
    return result;
}

std::uint64_t total_missed(const sim::World& world) {
    std::uint64_t result=0;
    for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household)
        result+=b.missed_demand;
    return result;
}

void check_recovery(sim::Cell removed,const char* label,int mode,SaveFixture& files) {
    auto world=recovery_city();
    remove_when_free(world,removed);
    const auto missed=total_missed(world);
    bool buffer_drained=false;
    for (int tick=0;tick<12'000 && total_missed(world)==missed;++tick) {
        world.tick();
        const auto& market=world.building(static_cast<sim::BuildingId>(5));
        bool empty_house=false;
        for (const auto& b:world.buildings()) if (b.kind==sim::Object::Household)
            empty_house=empty_house || b.pottery_stock==0 || b.food_stock==0;
        buffer_drained=buffer_drained || (mode==2 ? world.covered_households()==0:
            mode==1 ? empty_house &&
                (market.pottery_stock==sim::Rules::market_pottery_capacity ||
                 market.food_stock==sim::Rules::market_food_capacity):
                market.pottery_stock==0 || market.food_stock==0);
    }
    require(total_missed(world)>missed,std::string(label)+" outage never reached a failed demand");
    roundtrip_checkpoint(world,8,files,(std::string(label)+" outage").c_str());
    put(world,sim::CommandType::PlaceRoad,removed.x,removed.y);
    const auto fulfilled=total_fulfilled(world);
    bool real_recovery=false;
    for (int tick=0;tick<6'000 && !real_recovery;++tick) {
        world.tick();
        real_recovery=total_fulfilled(world)>fulfilled;
    }
    require(buffer_drained && real_recovery && world.production_balance_valid() &&
            world.navigation_valid() && world.population_valid(),
        std::string(label)+" did not drain its affected buffer and recover after repair");
    roundtrip_checkpoint(world,8,files,(std::string(label)+" recovery").c_str());
    std::cout<<"recovery="<<label<<" tick="<<world.ticks()
             <<" population="<<world.total_population()<<'\n';
}

sim::BuildingId building_of(const sim::World& world,sim::Object kind) {
    for (const auto& building:world.buildings()) if (building.kind==kind) return building.id;
    throw std::runtime_error("recovery building kind missing");
}

void operation_recovery() {
    auto crisis=recovery_city();
    constexpr sim::Cell farm_entrance{6,4};
    remove_when_free(crisis,farm_entrance);
    const auto missed_before=total_missed(crisis);
    const auto crisis_deadline=crisis.ticks()+20'000;
    while ((crisis.total_population()>15 || total_missed(crisis)==missed_before) &&
           crisis.ticks()<crisis_deadline) crisis.tick();
    require(crisis.total_population()<=15 && total_missed(crisis)>missed_before,
            "natural food interruption did not produce the bounded workforce crisis");
    const auto crisis_population=crisis.total_population();
    const auto crisis_taxes=crisis.taxes_collected_total();
    const auto crisis_fulfilled=total_fulfilled(crisis);
    const auto crisis_inventory=crisis.resource_inventory();

    auto control=sim::World::restore(crisis.snapshot(),std::vector<std::uint8_t>(width*8,1));
    save::SaveDocument source;
    source.world=crisis.snapshot();
    auto upgraded=save::upgrade_city_v11_v2_to_v3(source);
    auto rescued=sim::World::restore(upgraded.world,std::vector<std::uint8_t>(width*8,1));
    put(control,sim::CommandType::PlaceRoad,farm_entrance.x,farm_entrance.y);
    put(rescued,sim::CommandType::PlaceRoad,farm_entrance.x,farm_entrance.y);

    const auto clay=building_of(rescued,sim::Object::ClaySource);
    const auto pottery=building_of(rescued,sim::Object::Pottery);
    const auto farm=building_of(rescued,sim::Object::Farm);
    const auto market=building_of(rescued,sim::Object::Market);
    const auto service=building_of(rescued,sim::Object::ServicePost);
    for (const auto command:{sim::set_building_operation(clay,false),
                             sim::set_building_operation(pottery,false),
                             sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High),
                             sim::set_building_workforce_priority(market,sim::WorkforcePriority::High),
                             sim::set_building_workforce_priority(service,sim::WorkforcePriority::High)})
        require(rescued.execute(command).accepted,"recovery operation command failed");
    require(rescued.active_workforce_required()==12 && rescued.workforce_used()==12,
            "recovery allocation did not reserve the 12-worker supply chain");

    bool first_tax=false,pottery_restarted=false,clay_restarted=false,later_growth=false;
    std::uint64_t first_tax_tick=0;
    int first_tax_population=0;
    std::uint64_t first_tax_fulfilled=0;
    std::uint64_t first_tax_delta=0;
    const auto rescue_deadline=rescued.ticks()+12'000;
    while (rescued.ticks()<rescue_deadline && !later_growth) {
        rescued.tick();
        if (!first_tax && rescued.taxes_collected_total()>crisis_taxes) {
            first_tax=true;
            first_tax_tick=rescued.ticks();
            first_tax_population=rescued.total_population();
            first_tax_fulfilled=total_fulfilled(rescued);
            first_tax_delta=rescued.taxes_collected_total()-crisis_taxes;
            require(rescued.total_population()>crisis_population,
                    "first recovered demand paid tax without population growth");
            require(rescued.execute(sim::set_building_operation(pottery,true)).accepted &&
                    rescued.execute(sim::set_building_workforce_priority(
                        pottery,sim::WorkforcePriority::High)).accepted,
                    "pottery restart decisions failed");
            pottery_restarted=true;
        }
        if (pottery_restarted && !clay_restarted && rescued.total_population()>=22) {
            require(rescued.execute(sim::set_building_operation(clay,true)).accepted,
                    "clay restart decision failed");
            clay_restarted=true;
        }
        later_growth=clay_restarted && rescued.total_population()>=crisis_population+8 &&
            rescued.taxes_collected_total()>crisis_taxes+100;
    }
    for (int tick=0;tick<12'000;++tick) control.tick();
    require(first_tax && pottery_restarted && clay_restarted && later_growth &&
            rescued.production_balance_valid() && rescued.food_balance_valid() &&
            rescued.service_state_valid() && rescued.population_valid(),
            "operation-control crisis did not achieve sustained measured recovery");
    require(control.total_population()<=crisis_population &&
            control.taxes_collected_total()==crisis_taxes,
            "unchanged v2 control unexpectedly recovered from the same staffing crisis");
    std::cout<<"operation_crisis tick="<<crisis.ticks()<<" population="<<crisis_population
             <<" stocks clay="<<(crisis_inventory.clay_source_output+
                                  crisis_inventory.clay_courier_cargo+
                                  crisis_inventory.pottery_clay_input+
                                  crisis_inventory.recipe_clay)
             <<" pottery="<<(crisis_inventory.pottery_producer_output+
                              crisis_inventory.pottery_warehouse_stock+
                              crisis_inventory.pottery_market_stock+
                              crisis_inventory.pottery_household_stock+
                              crisis_inventory.pottery_courier_cargo)
             <<" food="<<(crisis_inventory.food_farm_output+
                           crisis_inventory.food_market_stock+
                           crisis_inventory.food_household_stock+
                           crisis_inventory.food_courier_cargo)
             <<" first_tax_tick="<<first_tax_tick
             <<" first_tax_population="<<first_tax_population
             <<" first_tax_fulfilled_delta="<<(first_tax_fulfilled-crisis_fulfilled)
             <<" first_tax_delta="<<first_tax_delta
             <<" recovered_population="<<rescued.total_population()
             <<" recovered_fulfilled_delta="<<(total_fulfilled(rescued)-crisis_fulfilled)
             <<" tax_delta="<<(rescued.taxes_collected_total()-crisis_taxes)<<'\n';
}

} // namespace

int main() {
    try {
        SaveFixture save_files;
        const auto& v1=sim::profile_rules(sim::RulesProfile::CityV11,1);
        const auto& v2=sim::profile_rules(sim::RulesProfile::CityV11,2);
        const auto& v3=sim::profile_rules(sim::RulesProfile::CityV11,3);
        require(sim::current_rule_version(sim::RulesProfile::CityV11)==3,
            "new City-v11 Worlds enabled v4 before native acceptance");
        require(v3.clay_ticks==v2.clay_ticks &&
                v3.pottery_recipe_ticks==v2.pottery_recipe_ticks &&
                v3.farm_ticks==v2.farm_ticks &&
                v3.courier_edge_ticks==v2.courier_edge_ticks &&
                v3.household_move_in_grace_ticks==v2.household_move_in_grace_ticks,
            "City-v11 v3 changed v2 economic timing");
        require(v1.clay_ticks==100 && v1.pottery_recipe_ticks==150 && v1.farm_ticks==80 &&
                v1.courier_edge_ticks==10 && v1.household_move_in_grace_ticks==0,
            "City-v11 v1 rules changed");
        const int clay_capacity=4*sim::Rules::household_demand_ticks/v2.clay_ticks;
        const int raw_pottery=clay_capacity/sim::Rules::pottery_recipe_clay;
        const int process_pottery=4*sim::Rules::household_demand_ticks/
            (v2.pottery_recipe_ticks+1);
        const int food_capacity=2*sim::Rules::household_demand_ticks/v2.farm_ticks;
        require(v2.clay_ticks==32 && v2.pottery_recipe_ticks==64 && v2.farm_ticks==32 &&
                v2.courier_edge_ticks==5 && v2.household_move_in_grace_ticks==800 &&
                clay_capacity==50 && raw_pottery==25 && process_pottery==24 &&
                food_capacity==25,
            "City-v11 v2 capacity calculation changed");
        std::cout<<"capacity_per_400 clay="<<clay_capacity<<" raw_pottery="<<raw_pottery
                 <<" processed_pottery="<<process_pottery<<" food="<<food_capacity<<'\n';

        auto checkpoint=starter(false,false,false,2);
        roundtrip_checkpoint(checkpoint,8,save_files,"paid tick-zero starter");
        for (int tick=0;tick<799;++tick) checkpoint.tick();
        require(checkpoint.household_move_in_grace_remaining(
                    checkpoint.buildings().back().id)==1,
            "move-in grace boundary before expiry changed");
        roundtrip_checkpoint(checkpoint,8,save_files,"grace before expiry");
        checkpoint.tick();
        require(checkpoint.household_move_in_grace_remaining(
                    checkpoint.buildings().back().id)==0,
            "move-in grace did not expire at tick 800");
        roundtrip_checkpoint(checkpoint,8,save_files,"grace after expiry and first taxes");
        bool active_market_flow=false;
        for (int tick=0;tick<5'000 && !active_market_flow;++tick) {
            checkpoint.tick();
            bool inbound=false,outbound=false;
            for (const auto& courier:checkpoint.couriers()) {
                const bool moving=courier.phase==sim::CourierPhase::ToWarehouse;
                inbound=inbound || (moving &&
                    (courier.role==sim::CourierRole::MarketPotteryInbound ||
                     courier.role==sim::CourierRole::MarketFoodInbound));
                outbound=outbound || (moving &&
                    (courier.role==sim::CourierRole::MarketPotteryDistribution ||
                     courier.role==sim::CourierRole::MarketFoodDistribution));
            }
            active_market_flow=inbound && outbound;
        }
        require(active_market_flow,"active v2 Market checkpoint was not reached");
        roundtrip_checkpoint(checkpoint,8,save_files,"active Market flows");

        for (const auto variant:{std::pair{false,false},std::pair{false,true},
                                 std::pair{true,false},std::pair{true,true}}) {
            auto world=starter(false,variant.first,variant.second,2);
            require(world.ticks()==0 && world.treasury()==100 && world.total_population()==24,
                "v2 build-order variant was not a paid tick-zero starter");
            for (int tick=0;tick<400;++tick) world.tick();
            require(world.total_population()==24,
                "move-in grace did not protect tick-zero workforce at first demand");
            for (int tick=400;tick<2'000;++tick) world.tick();
            require_sustained(measure_window(world,2'400),6,"build-order variant");
        }

        auto sustained=starter(false,false,false,2);
        for (int tick=0;tick<2'000;++tick) sustained.tick();
        const auto starter_tax=sustained.taxes_collected_total();
        const auto starter_population=sustained.total_population();
        const auto starter_window=measure_window(sustained,4'000);
        require_sustained(starter_window,10,"four-house starter");
        require(sustained.taxes_collected_total()>starter_tax &&
                sustained.total_population()>=starter_population,
            "sustained starter did not preserve population and earn window taxes");

        auto goal=played_goal();
        const auto goal_tick=goal.ticks();
        const auto goal_population=goal.total_population();
        require(goal.settlement_goal_reached() && goal_population>=100,
            "played run did not reach all goal conditions together");
        roundtrip_checkpoint(goal,16,save_files,"played goal");
        require_sustained(measure_window(goal,2'400),6,"played goal follow-up");
        require(goal.settlement_goal_reached() && goal.total_population()>=goal_population,
            "played goal city collapsed in its follow-up window");

        auto maximum=played_maximum();
        for (int tick=0;tick<6'000;++tick) maximum.tick();
        const auto maximum_window=measure_window(maximum,2'400);
        require_sustained(maximum_window,6,"paid 20-house city");
        require(maximum.buildings().size()==38 && maximum.couriers().size()==22,
            "paid maximum city entity limits changed");

        check_recovery({8,4},"market-inbound",0,save_files);
        check_recovery({12,4},"market-outbound",1,save_files);
        check_recovery({20,3},"service",2,save_files);
        operation_recovery();

        auto v1_progress=sim::World(width,8,std::vector<std::uint8_t>(width*8,1),
            sim::RulesProfile::CityV11,1);
        for (int x:{0,3,6,9}) put(v1_progress,sim::CommandType::PlaceHousehold,x,2);
        put(v1_progress,sim::CommandType::PlaceClaySource,15,2);
        for (int tick=0;tick<70;++tick) v1_progress.tick();
        require(v1_progress.buildings().back().progress==70,
            "v1 compatibility fixture did not retain old production progress");
        const auto v1_snapshot=v1_progress.snapshot();
        auto v1_restored=sim::World::restore(v1_snapshot,
            std::vector<std::uint8_t>(width*8,1));
        require(v1_restored.snapshot()==v1_snapshot && v1_restored.rule_version()==1,
            "City-v11 v1 restore changed state or rules");
        const auto v2_snapshot=maximum.snapshot();
        auto v2_restored=sim::World::restore(v2_snapshot,
            std::vector<std::uint8_t>(width*32,1));
        require(v2_restored.snapshot()==v2_snapshot && v2_restored.rule_version()==2,
            "City-v11 v2 restore changed state or rules");
        auto unknown=v2_snapshot; unknown.rule_version=5;
        bool rejected=false;
        try { (void)sim::World::restore(unknown,std::vector<std::uint8_t>(width*32,1)); }
        catch (const std::invalid_argument&) { rejected=true; }
        require(rejected,"unknown City-v11 rule version was accepted");

        auto deterministic=goal;
        auto comparison=sim::World::restore(goal.snapshot(),
            std::vector<std::uint8_t>(width*16,1));
        for (int tick=1;tick<=20'000;++tick) {
            deterministic.tick(); comparison.tick();
            if (tick%1'000==0)
                require(deterministic.snapshot()==comparison.snapshot(),
                    "v2 deterministic snapshots diverged during 20k run");
        }
        std::cout<<"goal_tick="<<goal_tick<<" goal_population="<<goal_population
                 <<" followup_population="<<goal.total_population()
                 <<" maximum_population="<<maximum.total_population()
                 <<" maximum_tax="<<maximum.taxes_collected_total()<<'\n';
        std::cout<<"City-v11 sustainable balance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
