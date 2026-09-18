#include "persistence/SandboxSave.h"
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
void check(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(16U*3U,1); }
void put(sim::World& w,sim::CommandType type,int x) {
    check(w.execute({type,{x,1}}).accepted,"synthetic placement rejected");
}
sim::World chain(bool connected=true) {
    sim::World w(16,3,mask(),sim::RulesProfile::HouseholdV3);
    put(w,sim::CommandType::PlaceClaySource,0);
    put(w,sim::CommandType::PlaceRoad,1); put(w,sim::CommandType::PlaceRoad,2);
    put(w,sim::CommandType::PlacePottery,3);
    put(w,sim::CommandType::PlaceRoad,4); put(w,sim::CommandType::PlaceRoad,5);
    put(w,sim::CommandType::PlaceWarehouse,6);
    put(w,sim::CommandType::PlaceRoad,7);
    if (connected) put(w,sim::CommandType::PlaceRoad,8);
    put(w,sim::CommandType::PlaceHousehold,9);
    return w;
}
template<class F> void until(sim::World& w,int max,F found,const char* why) {
    for (int i=0;i<max && !found();++i) w.tick();
    check(found(),why);
}
void advance(sim::World& w,std::uint64_t tick) {
    while (w.ticks()<tick) w.tick();
}
} // namespace

int main() {
    try {
        const fs::path root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-household-test-"+std::to_string(std::random_device{}()));
        struct Cleanup { fs::path path; ~Cleanup(){ std::error_code e; fs::remove_all(path,e); } } cleanup{root};
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        { std::ofstream out(root/"data/Cities/Synthetic.map",std::ios::binary);
          out<<"independently authored household map identity"; }
        const fs::path map="Cities/Synthetic.map",target=root/"saves/household.json";

        sim::World lone(16,3,mask(),sim::RulesProfile::HouseholdV3);
        advance(lone,800);
        check(lone.building(sim::BuildingId::Household).missed_demand==0,
              "unplaced household consumed");
        put(lone,sim::CommandType::PlaceHousehold,9);
        check(lone.building(sim::BuildingId::Household).placed_tick==800 &&
              lone.building(sim::BuildingId::Household).demand_progress==0,
              "placement advanced demand clock");
        advance(lone,1199);
        check(lone.building(sim::BuildingId::Household).missed_demand==0 &&
              lone.building(sim::BuildingId::Household).last_demand_status==0,
              "need occurred before first deadline");
        lone.tick();
        check(lone.building(sim::BuildingId::Household).missed_demand==1 &&
              lone.building(sim::BuildingId::Household).last_demand_status==2,
              "empty household missed wrong first deadline");
        sim::World v2(16,3,mask(),sim::RulesProfile::ProductionV2);
        check(!v2.execute({sim::CommandType::PlaceHousehold,{9,1}}).accepted &&
              !v2.courier(sim::CourierId::Household).enabled,"v2 enabled household");
        sim::World v1(16,3,mask());
        check(!v1.execute({sim::CommandType::PlaceHousehold,{9,1}}).accepted,
              "v1 enabled household");
        // A v3 chain without its receiver keeps the old finite warehouse
        // behavior and has no demand clock or supplier activity.
        // Build the same production half without placing a household.
        auto no_receiver=sim::World(16,3,mask(),sim::RulesProfile::HouseholdV3);
        put(no_receiver,sim::CommandType::PlaceClaySource,0);
        put(no_receiver,sim::CommandType::PlaceRoad,1); put(no_receiver,sim::CommandType::PlaceRoad,2);
        put(no_receiver,sim::CommandType::PlacePottery,3);
        put(no_receiver,sim::CommandType::PlaceRoad,4); put(no_receiver,sim::CommandType::PlaceRoad,5);
        put(no_receiver,sim::CommandType::PlaceWarehouse,6);
        advance(no_receiver,12000);
        check(no_receiver.building(sim::BuildingId::Warehouse).pottery_stock==
                  sim::Rules::warehouse_capacity &&
              no_receiver.building(sim::BuildingId::Household).missed_demand==0 &&
              !no_receiver.courier(sim::CourierId::Household).enabled &&
              no_receiver.production_balance_valid(),"absent household changed finite production");

        auto cut=chain(false);
        advance(cut,1000);
        const auto& empty=cut.building(sim::BuildingId::Household);
        check(cut.building(sim::BuildingId::Warehouse).pottery_stock>0 &&
              empty.pottery_stock==0 && empty.missed_demand==2 &&
              cut.courier(sim::CourierId::Household).cargo==0 &&
              cut.production_balance_valid(),"unconnected house received or consumed warehouse goods");
        put(cut,sim::CommandType::PlaceRoad,8);
        until(cut,40,[&]{ return cut.courier(sim::CourierId::Household).cargo>0; },
              "warehouse did not dispatch physical pottery");
        const auto shipment=cut.courier(sim::CourierId::Household).cargo;
        check(cut.building(sim::BuildingId::Household).pottery_stock==0 &&
              cut.building(sim::BuildingId::Household).reserved_incoming==shipment &&
              cut.production_balance_valid(),"reserved transit goods appeared in house stock");
        until(cut,60,[&]{ return cut.building(sim::BuildingId::Household).pottery_stock>0; },
              "household delivery did not arrive");
        check(cut.building(sim::BuildingId::Household).missed_demand==2,
              "delivery retroactively fulfilled missing demand");
        advance(cut,1199);
        const auto before_demand=cut.building(sim::BuildingId::Household).pottery_stock;
        cut.tick();
        check(cut.building(sim::BuildingId::Household).pottery_stock==before_demand-1 &&
              cut.building(sim::BuildingId::Household).fulfilled_demand==1 &&
              cut.building(sim::BuildingId::Household).consumed_total==1 &&
              cut.building(sim::BuildingId::Household).last_demand_status==1,
              "next need did not consume exactly one delivered Pottery");

        // Three deadline cases. A delivery at the deadline is too late for
        // that tick because consumption precedes movement and arrival.
        for (int delta:{-1,0,1}) {
            auto deadline=chain(false);
            advance(deadline,static_cast<std::uint64_t>(870+delta));
            // Placement on tick zero means the third deadline is tick 1200.
            // Opening at 1170 + delta yields arrival at 1200 + delta.
            advance(deadline,static_cast<std::uint64_t>(1170+delta));
            put(deadline,sim::CommandType::PlaceRoad,8);
            advance(deadline,1200);
            const auto& h=deadline.building(sim::BuildingId::Household);
            check(h.fulfilled_demand==(delta<0 ? 1U:0U) &&
                  h.missed_demand==(delta<0 ? 2U:3U),
                  "arrival/deadline tick order changed");
        }

        // A third courier on a begun edge protects both endpoints. A road
        // farther ahead may be cut; it then waits with real cargo/reservation.
        auto broken=chain();
        until(broken,2000,[&] {
            const auto& c=broken.courier(sim::CourierId::Household);
            return broken.building(sim::BuildingId::Household).pottery_stock>0 &&
                c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==sim::Cell{6,1};
        },"no loaded household courier on first edge");
        const auto revision=broken.road_revision();
        check(!broken.execute({sim::CommandType::RemoveRoad,{7,1}}).accepted &&
              broken.road_revision()==revision,"third courier edge endpoint unprotected");
        check(broken.execute({sim::CommandType::RemoveRoad,{8,1}}).accepted,
              "future household road removal rejected");
        until(broken,40,[&] {
            const auto& c=broken.courier(sim::CourierId::Household);
            return c.route_pending && c.edge_progress==0;
        },"household courier did not wait");
        const auto wait_position=broken.courier_position(sim::CourierId::Household);
        const auto held=broken.courier(sim::CourierId::Household).cargo;
        check(wait_position==sim::Position{7.0,1.0} && held>0 &&
              broken.courier(sim::CourierId::Household).reserved==held &&
              broken.building(sim::BuildingId::Household).reserved_incoming==held,
              "waiting household shipment lost goods or teleported");
        const auto old_fulfilled=broken.building(sim::BuildingId::Household).fulfilled_demand;
        const auto old_missed=broken.building(sim::BuildingId::Household).missed_demand;
        broken.tick();
        const auto failed_attempts=broken.courier(sim::CourierId::Household).reroute_attempts;
        for (int i=0;i<4000;++i) broken.tick();
        check(broken.building(sim::BuildingId::Household).pottery_stock==0 &&
              broken.building(sim::BuildingId::Household).fulfilled_demand>old_fulfilled &&
              broken.building(sim::BuildingId::Household).missed_demand>old_missed &&
              broken.courier(sim::CourierId::Household).cargo==held &&
              broken.courier(sim::CourierId::Household).reroute_attempts==failed_attempts &&
              broken.building(sim::BuildingId::Household).reserved_incoming==held &&
              broken.production_balance_valid(),
              "stock did not drain into unmet demand while delivery waited");
        const auto before_save=broken.snapshot();
        auto document=save::make_document(root/"data",map,mask(),broken);
        save::write_save(target,document,root/"data",mask());
        auto restored=save::restore_save(save::read_save(target),root/"data",mask());
        check(restored.snapshot()==before_save,"waiting household JSON roundtrip differs");
        { std::ifstream in(target); nlohmann::json valid; in>>valid;
          const auto reject=[&](const auto& change) {
              auto invalid=valid; change(invalid);
              { std::ofstream out(target,std::ios::trunc); out<<invalid.dump(); }
              try { (void)save::restore_save(save::read_save(target),root/"data",mask()); }
              catch (const std::exception&) { return; }
              throw std::runtime_error("tampered household save accepted");
          };
          reject([](auto& j){ j["schema_version"]=2; });
          reject([](auto& j){ j["rules"]["version"]=2; });
          reject([](auto& j){ j["world"]["buildings"][3]["last_demand_status"]=3; });
          reject([](auto& j){ auto& n=j["world"]["buildings"][3]["consumed_total"];
                                n=n.template get<std::uint64_t>()+1; });
          reject([](auto& j){ j["world"]["buildings"][3]["placed_tick"]=999999; });
          reject([](auto& j){ j["world"]["buildings"][3]["demand_progress"]=399; });
          reject([](auto& j){ j["world"]["buildings"][3]["reserved_incoming"]=0; });
          reject([](auto& j){ j["world"]["couriers"][2]["target"]=3; });
          reject([](auto& j){ j["world"]["couriers"][2]["owner"]=2; });
        }
        put(broken,sim::CommandType::PlaceRoad,8);
        put(restored,sim::CommandType::PlaceRoad,8);
        int arrivals=0,returns=0;
        for (int i=0;i<1200;++i) {
            const auto old=broken.courier(sim::CourierId::Household).phase;
            broken.tick(); restored.tick();
            check(broken.snapshot()==restored.snapshot() && broken.production_balance_valid() &&
                  broken.navigation_valid(),"household repair continuation diverged");
            const auto now=broken.courier(sim::CourierId::Household).phase;
            if (old==sim::CourierPhase::ToWarehouse && now==sim::CourierPhase::Returning) ++arrivals;
            if (old==sim::CourierPhase::Returning && now==sim::CourierPhase::IdleAtWorkshop) ++returns;
        }
        check(arrivals>0 && returns>0 && broken.building(sim::BuildingId::Household).consumed_total>0,
              "repaired delivery/return/consumption missing");

        auto returner=chain();
        until(returner,3000,[&] {
            const auto& c=returner.courier(sim::CourierId::Household);
            return c.phase==sim::CourierPhase::Returning && c.edge_progress>0 &&
                c.path[c.path_vertex]==sim::Cell{9,1};
        },"no empty household returner on house edge");
        check(returner.execute({sim::CommandType::RemoveRoad,{7,1}}).accepted,
              "future return road could not be removed");
        until(returner,40,[&] {
            const auto& c=returner.courier(sim::CourierId::Household);
            return c.route_pending && c.edge_progress==0;
        },"empty household returner did not wait");
        check(returner.courier(sim::CourierId::Household).cargo==0 &&
              returner.courier_position(sim::CourierId::Household)==sim::Position{8.0,1.0},
              "empty household returner teleported or held goods");
        put(returner,sim::CommandType::PlaceRoad,7);
        until(returner,80,[&] {
            return returner.courier(sim::CourierId::Household).phase==
                sim::CourierPhase::IdleAtWorkshop;
        },"empty returner did not resume");

        auto full=chain();
        until(full,15000,[&] {
            const auto& h=full.building(sim::BuildingId::Household);
            return h.pottery_stock==sim::Rules::household_capacity && h.demand_progress<390 &&
                full.building(sim::BuildingId::Warehouse).pottery_stock>0 &&
                full.courier(sim::CourierId::Household).phase==sim::CourierPhase::IdleAtWorkshop;
        },"no full household with waiting warehouse stock");
        const auto stock_before=full.building(sim::BuildingId::Warehouse).pottery_stock;
        full.tick();
        check(full.courier(sim::CourierId::Household).cargo==0 &&
              full.building(sim::BuildingId::Warehouse).pottery_stock==stock_before &&
              full.building(sim::BuildingId::Household).reserved_incoming==0,
              "full household dispatched or reserved another shipment");
        until(full,450,[&] { return full.courier(sim::CourierId::Household).cargo>0; },
              "new free household space did not permit later dispatch");

        auto long_run=chain();
        bool saw_full=false,produced_after_full=false,saw_full_home=false;
        bool resumed_home=false,simultaneous_store=false;
        std::uint64_t at_full=0;
        for (int i=0;i<30000;++i) {
            long_run.tick();
            const auto& p=long_run.building(sim::BuildingId::Pottery);
            const auto& h=long_run.building(sim::BuildingId::Household);
            if (!saw_full && p.output==sim::Rules::pottery_output_capacity) {
                saw_full=true; at_full=long_run.pottery_completed_total();
            }
            if (saw_full && long_run.pottery_completed_total()>at_full) produced_after_full=true;
            if (h.pottery_stock==sim::Rules::household_capacity) saw_full_home=true;
            if (saw_full_home && long_run.courier(sim::CourierId::Household).cargo>0)
                resumed_home=true;
            if (long_run.building(sim::BuildingId::Warehouse).reserved_incoming>0 &&
                long_run.courier(sim::CourierId::Household).cargo>0) simultaneous_store=true;
            check(long_run.production_balance_valid() && long_run.navigation_valid() &&
                  h.pottery_stock+h.reserved_incoming<=sim::Rules::household_capacity,
                  "long household run violated goods or capacity invariant");
        }
        check(long_run.pottery_completed_total()>40 &&
              long_run.building(sim::BuildingId::Household).consumed_total>40 &&
              saw_full && produced_after_full && saw_full_home && resumed_home &&
              simultaneous_store,"long run did not exercise buffers or concurrent store flow");
        std::cout<<"household: delivered=true consumed="<<long_run.building(sim::BuildingId::Household).consumed_total
                 <<" completed="<<long_run.pottery_completed_total()<<" repaired_arrivals="<<arrivals<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"household test failed: "<<error.what()<<'\n'; return 1;
    }
}
