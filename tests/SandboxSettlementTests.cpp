#include "persistence/SandboxSave.h"
#include <nlohmann/json.hpp>

#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
void check(bool yes,const char* why) { if (!yes) throw std::runtime_error(why); }
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(16U*6U,1); }
void put(sim::World& w,sim::CommandType type,int x,int y) {
    check(w.execute({type,{x,y}}).accepted,"synthetic settlement placement failed");
}
void road(sim::World& w,int x,int y) { put(w,sim::CommandType::PlaceRoad,x,y); }
void tick_to(sim::World& w,std::uint64_t deadline) { while (w.ticks()<deadline) w.tick(); }
template<class F> void until(sim::World& w,int limit,F found,const char* why) {
    for (int i=0;i<limit && !found();++i) w.tick();
    check(found(),why);
}
sim::World network() {
    sim::World w(16,6,mask(),sim::RulesProfile::SettlementV4);
    put(w,sim::CommandType::PlaceClaySource,0,2);
    road(w,1,2); road(w,2,2);
    put(w,sim::CommandType::PlacePottery,3,2);
    road(w,4,2); road(w,5,2);
    put(w,sim::CommandType::PlaceWarehouse,6,2);
    road(w,7,2); road(w,8,2); road(w,9,2);
    put(w,sim::CommandType::PlaceHousehold,10,2); // ID 4.
    road(w,7,1); road(w,8,1); road(w,9,1);
    put(w,sim::CommandType::PlaceHousehold,9,0); // ID 5, longer route.
    road(w,7,3); road(w,8,3); road(w,10,3);
    return w;
}
} // namespace

int main() {
    try {
        const auto root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-settlement-"+std::to_string(std::random_device{}()));
        struct Cleanup { fs::path p; ~Cleanup(){ std::error_code e; fs::remove_all(p,e); } } cleanup{root};
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        { std::ofstream out(root/"data/Cities/Synthetic.map",std::ios::binary);
          out<<"independently authored settlement identity"; }
        const fs::path map="Cities/Synthetic.map",target=root/"saves/settlement.json";
        auto w=network();
        check(w.next_household_id()==6 &&
              w.building_owner_at({10,2})==sim::BuildingId::Household &&
              w.building_owner_at({9,0})==static_cast<sim::BuildingId>(5) &&
              w.building(static_cast<sim::BuildingId>(5)).kind==sim::Object::Household,
              "household instance IDs or occupancy are wrong");
        const auto failed=w.execute({sim::CommandType::PlaceHousehold,{10,2}});
        check(!failed.accepted && w.next_household_id()==6,"rejected placement consumed an ID");
        tick_to(w,401);
        check(w.building(sim::BuildingId::Household).missed_demand==1 &&
              w.building(static_cast<sim::BuildingId>(5)).missed_demand==1,
              "initial independent household clocks failed");
        // Both homes have routes and capacity. The first eligible ID is H1.
        until(w,800,[&]{ return w.building(sim::BuildingId::Warehouse).pottery_stock>0 &&
            w.courier(sim::CourierId::Household).phase==sim::CourierPhase::IdleAtWorkshop; },
            "no available warehouse stock for first selection");
        const auto cursor_before=w.last_dispatched_household();
        const auto candidate=w.next_household_candidate();
        for (int i=0;i<20;++i) {
            check(w.next_household_candidate()==candidate &&
                  w.last_dispatched_household()==cursor_before &&
                  w.courier_blockage(sim::CourierId::Household)!=nullptr,
                  "read-only selection diagnostics advanced cursor");
        }
        check(candidate==sim::BuildingId::Household,"round robin did not start with ID 4");
        w.tick();
        check(w.courier(sim::CourierId::Household).target==sim::BuildingId::Household &&
              w.last_dispatched_household()==sim::BuildingId::Household,
              "first dispatch did not select H1");
        until(w,200,[&]{ return w.courier(sim::CourierId::Household).phase==
            sim::CourierPhase::IdleAtWorkshop; },"first supplier did not return");
        // The cursor survives a save made between trips, without an active reservation.
        auto idle_doc=save::make_document(root/"data",map,mask(),w);
        save::write_save(target,idle_doc,root/"data",mask());
        auto idle_loaded=save::restore_save(save::read_save(target),root/"data",mask());
        check(idle_loaded.snapshot()==w.snapshot() &&
              idle_loaded.last_dispatched_household()==sim::BuildingId::Household,
              "idle cursor was not restored");
        for (int i=0;i<600;++i) {
            w.tick(); idle_loaded.tick();
            check(w.snapshot()==idle_loaded.snapshot(),"idle cursor continuation diverged");
            if (w.courier(sim::CourierId::Household).phase==sim::CourierPhase::ToWarehouse &&
                w.courier(sim::CourierId::Household).target==static_cast<sim::BuildingId>(5)) break;
        }
        check(w.last_dispatched_household()==static_cast<sim::BuildingId>(5) &&
              w.courier(sim::CourierId::Household).phase==sim::CourierPhase::ToWarehouse,
              "idle cursor did not advance to H2");
        until(w,200,[&]{ return w.courier(sim::CourierId::Household).phase==
            sim::CourierPhase::IdleAtWorkshop; },"H2 supplier did not return");
        check(w.building(sim::BuildingId::Household).pottery_stock>0 &&
              w.building(static_cast<sim::BuildingId>(5)).pottery_stock>0 &&
              w.production_balance_valid(),"two reachable houses did not receive real goods");

        // H3 begins later, with no connected final road. Its missed needs do
        // not keep H1/H2 from continuing to receive supplies.
        put(w,sim::CommandType::PlaceHousehold,10,4);
        const auto id3=static_cast<sim::BuildingId>(6);
        const auto h3_placed=w.building(id3).placed_tick;
        const auto old_h1=w.building(sim::BuildingId::Household).fulfilled_demand;
        tick_to(w,h3_placed+850);
        check(w.building(id3).missed_demand==2 && w.building(id3).pottery_stock==0 &&
              w.building(sim::BuildingId::Household).fulfilled_demand>old_h1,
              "disconnected H3 blocked H1/H2 or received goods");
        road(w,9,3); // H3's unique final approach.
        until(w,2000,[&]{ return w.last_dispatched_household()==id3; },
              "newly connected H3 was never selected");
        const auto active=w.courier(sim::CourierId::Household);
        check(active.phase==sim::CourierPhase::ToWarehouse && active.target==id3 && active.cargo>0,
              "H3 target not active after selection");
        road(w,10,1);
        put(w,sim::CommandType::PlaceHousehold,11,1); // ID 7, during H3 flight.
        const auto id4=static_cast<sim::BuildingId>(7);
        check(w.building_owner_at({11,1})==id4 && w.next_household_id()==8 &&
              w.courier(sim::CourierId::Household).target==id3 &&
              w.courier(sim::CourierId::Household).cargo==active.cargo &&
              w.courier(sim::CourierId::Household).reserved==active.reserved,
              "fourth placement changed active H3 delivery");
        check(!w.execute({sim::CommandType::PlaceHousehold,{12,1}}).accepted &&
              w.next_household_id()==8,"fifth house was accepted or consumed an ID");
        // A unique future H3 road can be cut after the supplier begins the
        // first edge. Its booked target and reservation cannot switch to H4.
        until(w,70,[&]{ const auto& c=w.courier(sim::CourierId::Household);
            return c.phase==sim::CourierPhase::ToWarehouse && c.target==id3 &&
                c.edge_progress>0 && c.path[c.path_vertex]==sim::Cell{6,2}; },
            "H3 courier did not begin protected edge");
        check(!w.execute({sim::CommandType::RemoveRoad,{7,2}}).accepted &&
              w.execute({sim::CommandType::RemoveRoad,{9,3}}).accepted,
              "H3 edge protection or future removal failed");
        until(w,100,[&]{ const auto& c=w.courier(sim::CourierId::Household);
            return c.route_pending && c.edge_progress==0; },
            "H3 supplier did not wait at a real waypoint");
        const auto waiting=w.snapshot();
        check(w.courier(sim::CourierId::Household).target==id3 &&
              w.building(id3).reserved_incoming==w.courier(sim::CourierId::Household).cargo &&
              w.building(id4).reserved_incoming==0 && w.production_balance_valid(),
              "waiting shipment changed target or duplicated reservation");
        auto doc=save::make_document(root/"data",map,mask(),w);
        save::write_save(target,doc,root/"data",mask());
        auto loaded=save::restore_save(save::read_save(target),root/"data",mask());
        check(loaded.snapshot()==waiting,"waiting multi-house JSON state differs");
        road(w,9,3); road(loaded,9,3);
        std::array<int,4> arrivals{};
        std::array<int,4> fulfilled_before{};
        for (int i=0;i<4;++i)
            fulfilled_before[static_cast<std::size_t>(i)]=
                static_cast<int>(w.building(static_cast<sim::BuildingId>(4+i)).fulfilled_demand);
        for (int i=0;i<2000;++i) {
            const auto prior=w.courier(sim::CourierId::Household);
            w.tick(); loaded.tick();
            check(w.snapshot()==loaded.snapshot() && w.production_balance_valid() &&
                  w.navigation_valid(),"multi-house save continuation diverged");
            const auto& now=w.courier(sim::CourierId::Household);
            if (prior.phase==sim::CourierPhase::ToWarehouse && now.phase==sim::CourierPhase::Returning)
                ++arrivals[static_cast<std::size_t>(static_cast<unsigned>(prior.target)-4U)];
        }
        check(arrivals[2]>0 && arrivals[3]>0 &&
              w.building(id3).fulfilled_demand>static_cast<std::uint64_t>(fulfilled_before[2]),
              "repaired H3 delivery or later H4 selection missing");

        // Reparse a save with the building array in reverse order. Stable
        // IDs, not its JSON order, determine the reconstructed world.
        save::write_save(target,save::make_document(root/"data",map,mask(),w),root/"data",mask());
        { std::ifstream in(target); nlohmann::json j; in>>j;
          auto& bs=j["world"]["buildings"];
          std::reverse(bs.begin(),bs.end());
          { std::ofstream out(target,std::ios::trunc); out<<j.dump(); }
          check(save::restore_save(save::read_save(target),root/"data",mask()).snapshot()==w.snapshot(),
                "JSON building order affected instance IDs");
          bs[0]["id"]=6;
          { std::ofstream out(target,std::ios::trunc); out<<j.dump(); }
          bool rejected=false;
          try { (void)save::restore_save(save::read_save(target),root/"data",mask()); }
          catch (const std::exception&) { rejected=true; }
          check(rejected,"duplicate household ID was accepted"); }
        const auto rejects_json=[&](const char* why,const auto& edit) {
            save::write_save(target,save::make_document(root/"data",map,mask(),w),
                root/"data",mask());
            std::ifstream in(target); nlohmann::json j; in>>j; in.close();
            edit(j);
            { std::ofstream out(target,std::ios::trunc); out<<j.dump(); }
            bool rejected=false;
            try { (void)save::restore_save(save::read_save(target),root/"data",mask()); }
            catch (const std::exception&) { rejected=true; }
            check(rejected,why);
        };
        rejects_json("invalid next household ID accepted",[](auto& j) {
            j["world"]["next_household_id"]=7;
        });
        rejects_json("cursor on warehouse accepted",[](auto& j) {
            j["world"]["last_dispatched_household"]=3;
        });
        rejects_json("supplier target on warehouse accepted",[](auto& j) {
            j["world"]["couriers"][2]["target"]=3;
        });
        rejects_json("wrong household reservation accepted",[](auto& j) {
            j["world"]["buildings"][4]["reserved_incoming"]=1;
        });

        auto long_run=network();
        put(long_run,sim::CommandType::PlaceHousehold,10,4);
        road(long_run,9,3);
        std::array<int,4> delivered{};
        for (int i=0;i<30000;++i) {
            const auto before=long_run.courier(sim::CourierId::Household);
            long_run.tick();
            const auto& after=long_run.courier(sim::CourierId::Household);
            if (before.phase==sim::CourierPhase::ToWarehouse &&
                after.phase==sim::CourierPhase::Returning)
                ++delivered[static_cast<std::size_t>(static_cast<unsigned>(before.target)-4U)];
            check(long_run.production_balance_valid() && long_run.navigation_valid(),
                  "30,000-tick settlement invariant failed");
        }
        for (int i=0;i<3;++i)
            check(delivered[static_cast<std::size_t>(i)]>1 &&
                  long_run.building(static_cast<sim::BuildingId>(4+i)).consumed_total>1,
                  "reachable household did not repeatedly receive and consume");
        // One-house v4 uses the same rates, deadlines and shipment timing as v3.
        const auto single=[](sim::RulesProfile profile) {
            sim::World s(16,6,mask(),profile);
            put(s,sim::CommandType::PlaceClaySource,0,2);
            road(s,1,2); road(s,2,2);
            put(s,sim::CommandType::PlacePottery,3,2);
            road(s,4,2); road(s,5,2);
            put(s,sim::CommandType::PlaceWarehouse,6,2);
            road(s,7,2); road(s,8,2); road(s,9,2);
            put(s,sim::CommandType::PlaceHousehold,10,2);
            return s;
        };
        auto one_v3=single(sim::RulesProfile::HouseholdV3);
        auto one_v4=single(sim::RulesProfile::SettlementV4);
        check(!one_v4.next_household_candidate() && !one_v4.last_dispatched_household(),
              "empty warehouse changed the round-robin cursor");
        check(!one_v3.execute({sim::CommandType::PlaceHousehold,{11,2}}).accepted,
              "v3 accepted a second house");
        for (int i=0;i<5000;++i) {
            one_v3.tick(); one_v4.tick();
            const auto a=one_v3.snapshot(),b=one_v4.snapshot();
            check(a.buildings[3]==b.buildings[3] && a.buildings[2]==b.buildings[2] &&
                  a.couriers[2]==b.couriers[2],
                  "single-home v4 timing differs from v3");
        }
        until(one_v4,80000,[&]{ return
            one_v4.building(sim::BuildingId::Household).pottery_stock==
                sim::Rules::household_capacity &&
            one_v4.building(sim::BuildingId::Warehouse).pottery_stock>0 &&
            one_v4.courier(sim::CourierId::Household).phase==sim::CourierPhase::IdleAtWorkshop;
        },"single house never reached full-buffer selection state");
        const auto full_cursor=one_v4.last_dispatched_household();
        check(!one_v4.next_household_candidate() &&
              one_v4.last_dispatched_household()==full_cursor &&
              one_v4.building(sim::BuildingId::Household).reserved_incoming==0,
              "full household changed cursor or gained reservation");
        road(one_v4,7,1); road(one_v4,8,1);
        put(one_v4,sim::CommandType::PlaceHousehold,8,0);
        check(one_v4.next_household_candidate()==static_cast<sim::BuildingId>(5),
              "new eligible house was skipped after full house");
        one_v4.tick();
        check(one_v4.courier(sim::CourierId::Household).target==static_cast<sim::BuildingId>(5) &&
              one_v4.last_dispatched_household()==static_cast<sim::BuildingId>(5) &&
              one_v4.building(static_cast<sim::BuildingId>(5)).reserved_incoming>0,
              "full household was not skipped on actual dispatch");
        // Here H4 is farther than H5. The first dispatch still uses ID order.
        sim::World distance(16,6,mask(),sim::RulesProfile::SettlementV4);
        put(distance,sim::CommandType::PlaceClaySource,0,2);
        road(distance,1,2); road(distance,2,2);
        put(distance,sim::CommandType::PlacePottery,3,2);
        road(distance,4,2); road(distance,5,2);
        put(distance,sim::CommandType::PlaceWarehouse,6,2);
        for (int x=7;x<=10;++x) road(distance,x,2);
        put(distance,sim::CommandType::PlaceHousehold,11,2); // 5-edge route.
        road(distance,7,1); road(distance,8,1);
        put(distance,sim::CommandType::PlaceHousehold,8,0); // 4-edge route.
        std::array<int,2> first_targets{};
        int dispatches=0;
        for (int i=0;i<1500 && dispatches<2;++i) {
            const auto was=distance.courier(sim::CourierId::Household).phase;
            distance.tick();
            const auto& now=distance.courier(sim::CourierId::Household);
            if (was==sim::CourierPhase::IdleAtWorkshop &&
                now.phase==sim::CourierPhase::ToWarehouse)
                first_targets[static_cast<std::size_t>(dispatches++)]=
                    static_cast<int>(now.target);
        }
        check(dispatches==2 && first_targets==std::array<int,2>{4,5},
              "nearest house displaced stable-ID cyclic dispatch");
        std::cout<<"settlement long: delivered="<<delivered[0]<<','<<delivered[1]<<','<<delivered[2]
                 <<" consumed="<<long_run.building(sim::BuildingId::Household).consumed_total<<','
                 <<long_run.building(static_cast<sim::BuildingId>(5)).consumed_total<<','
                 <<long_run.building(static_cast<sim::BuildingId>(6)).consumed_total<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"settlement test failed: "<<error.what()<<'\n'; return 1;
    }
}
