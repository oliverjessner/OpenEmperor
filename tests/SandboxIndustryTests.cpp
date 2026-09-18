#include "persistence/SandboxSave.h"
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
void check(bool yes,const char* why) { if (!yes) throw std::runtime_error(why); }
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(20U*8U,1); }
void put(sim::World& w,sim::CommandType type,int x,int y) {
    check(w.execute({type,{x,y}}).accepted,"industry placement failed");
}
void road(sim::World& w,int x,int y) { put(w,sim::CommandType::PlaceRoad,x,y); }
sim::World base(sim::RulesProfile profile=sim::RulesProfile::IndustryV5) {
    sim::World w(20,8,mask(),profile);
    put(w,sim::CommandType::PlaceClaySource,0,3);
    road(w,1,3); road(w,2,3);
    put(w,sim::CommandType::PlacePottery,3,3);
    road(w,4,3); road(w,5,3);
    put(w,sim::CommandType::PlaceWarehouse,6,3);
    road(w,7,3); road(w,8,3);
    put(w,sim::CommandType::PlaceHousehold,9,3);
    road(w,7,2); road(w,8,2);
    put(w,sim::CommandType::PlaceHousehold,8,1);
    road(w,7,4); road(w,8,4); road(w,8,5);
    put(w,sim::CommandType::PlaceHousehold,9,5);
    return w;
}
void grow(sim::World& w) {
    road(w,1,5); road(w,2,5); road(w,2,4);
    road(w,4,5); road(w,5,5); road(w,6,5); road(w,6,4);
}
}
int main() {
    try {
        auto control=base(),expanded=base();
        auto legacy=base(sim::RulesProfile::SettlementV4);
        auto single_industry=base();
        check(!legacy.execute({sim::CommandType::PlaceClaySource,{0,5}}).accepted &&
              !legacy.execute({sim::CommandType::PlacePottery,{3,5}}).accepted,
              "v4 accepted an additional production building");
        for (int i=0;i<5000;++i) {
            legacy.tick(); single_industry.tick();
            const auto older=legacy.snapshot(),newer=single_industry.snapshot();
            for (std::size_t slot=0;slot<7;++slot)
                check(older.buildings[slot]==newer.buildings[slot],
                      "single-industry building timing changed");
            for (std::size_t slot=0;slot<3;++slot) {
                auto expected=older.couriers[slot],actual=newer.couriers[slot];
                if (slot==0) actual.last_dispatched_pottery.reset();
                check(expected==actual,"single-industry courier timing changed");
            }
        }
        sim::World reverse_order(20,8,mask(),sim::RulesProfile::IndustryV5);
        put(reverse_order,sim::CommandType::PlacePottery,3,3);
        put(reverse_order,sim::CommandType::PlacePottery,3,5);
        put(reverse_order,sim::CommandType::PlaceClaySource,0,3);
        put(reverse_order,sim::CommandType::PlaceClaySource,0,5);
        check(reverse_order.building(static_cast<sim::BuildingId>(8)).kind==sim::Object::Pottery &&
              reverse_order.building(static_cast<sim::BuildingId>(9)).kind==sim::Object::ClaySource &&
              reverse_order.courier(static_cast<sim::CourierId>(4)).role==sim::CourierRole::Pottery &&
              reverse_order.courier(static_cast<sim::CourierId>(5)).role==sim::CourierRole::Clay &&
              reverse_order.production_balance_valid(),
              "industry ID or courier role was inferred from position");
        // A validated synthetic boundary state has three free Clay input
        // slots. Both independent dispatches in one tick must share them.
        sim::World capacity(20,8,mask(),sim::RulesProfile::IndustryV5);
        put(capacity,sim::CommandType::PlaceClaySource,0,3);
        put(capacity,sim::CommandType::PlacePottery,3,3);
        put(capacity,sim::CommandType::PlaceClaySource,0,5);
        for (int i=0;i<900;++i) capacity.tick();
        road(capacity,1,3); road(capacity,2,3);
        road(capacity,1,5); road(capacity,2,5); road(capacity,2,4);
        auto boundary=capacity.snapshot();
        boundary.buildings[0].output=1;
        boundary.buildings[1].input_clay=7;
        capacity=sim::World::restore(boundary,mask());
        check(capacity.production_balance_valid(),"constructed capacity state failed validation");
        capacity.tick(); // Pottery starts a 2-Clay recipe; three input slots remain.
        check(capacity.building(sim::BuildingId::Pottery).input_clay==5 &&
              capacity.courier(sim::CourierId::Clay).cargo==1 &&
              capacity.courier(static_cast<sim::CourierId>(4)).cargo==2 &&
              capacity.building(sim::BuildingId::Pottery).reserved_incoming==3 &&
              capacity.production_balance_valid(),
              "two Clay dispatches overbooked three shared input slots");
        for (int i=0;i<3000;++i) { control.tick(); expanded.tick(); }
        check(control.snapshot()==expanded.snapshot(),"control fork differs");
        const auto before_growth=expanded.snapshot();
        put(expanded,sim::CommandType::PlaceClaySource,0,5); // ID 8, courier 4.
        put(expanded,sim::CommandType::PlacePottery,3,5); // ID 9, courier 5.
        for (std::size_t slot=0;slot<3;++slot)
            check(before_growth.couriers[slot]==expanded.snapshot().couriers[slot],
                  "new production changed an active legacy courier");
        check(expanded.next_production_id()==10 && expanded.next_courier_id()==6 &&
              expanded.building_owner_at({0,5})==static_cast<sim::BuildingId>(8) &&
              expanded.building_owner_at({3,5})==static_cast<sim::BuildingId>(9),
              "stable industry allocation failed");
        check(!expanded.execute({sim::CommandType::PlaceClaySource,{0,6}}).accepted &&
              !expanded.execute({sim::CommandType::PlacePottery,{3,6}}).accepted &&
              expanded.next_production_id()==10 && expanded.next_courier_id()==6,
              "third industry was accepted or consumed IDs");
        for (int i=0;i<1000;++i) { control.tick(); expanded.tick(); }
        check(expanded.building(static_cast<sim::BuildingId>(9)).input_clay==0 &&
              expanded.building(static_cast<sim::BuildingId>(9)).recipes_completed==0 &&
              expanded.pottery_completed_total()==control.pottery_completed_total(),
              "disconnected pottery created goods");
        grow(expanded);
        const auto initial_control=control.pottery_completed_total();
        const auto initial_expanded=expanded.pottery_completed_total();
        std::array<int,5> arrivals{};
        bool shared_clay=false,shared_pottery=false,retained_reservation=false;
        std::optional<sim::World> checkpoint_world;
        std::optional<sim::World> interruption_world;
        for (int i=0;i<5000;++i) {
            std::array<sim::CourierPhase,5> before{};
            for (unsigned id=1;id<=5;++id)
                before[id-1]=expanded.courier(static_cast<sim::CourierId>(id)).phase;
            control.tick(); expanded.tick();
            check(control.production_balance_valid() && expanded.production_balance_valid() &&
                  expanded.navigation_valid(),"industry balance/navigation failed");
            for (unsigned id=1;id<=5;++id)
                if (before[id-1]==sim::CourierPhase::ToWarehouse &&
                    expanded.courier(static_cast<sim::CourierId>(id)).phase==
                        sim::CourierPhase::Returning) ++arrivals[id-1];
            const auto& a=expanded.courier(sim::CourierId::Clay);
            const auto& other_clay=expanded.courier(static_cast<sim::CourierId>(4));
            if (a.phase==sim::CourierPhase::ToWarehouse &&
                other_clay.phase==sim::CourierPhase::ToWarehouse &&
                a.target==other_clay.target) {
                shared_clay=true;
                check(expanded.building(a.target).reserved_incoming==a.cargo+other_clay.cargo,
                      "two Clay couriers did not share target reservation");
            }
            const auto& b=expanded.courier(sim::CourierId::Pottery);
            const auto& other_pot=expanded.courier(static_cast<sim::CourierId>(5));
            if (b.phase==sim::CourierPhase::ToWarehouse &&
                other_pot.phase==sim::CourierPhase::ToWarehouse) {
                shared_pottery=true;
                if (!interruption_world && !other_pot.route_pending)
                    for (std::size_t step=other_pot.path_vertex+2;
                         step<other_pot.path.size();++step)
                        if (other_pot.path[step]==sim::Cell{6,4}) interruption_world=expanded;
                check(expanded.building(sim::BuildingId::Warehouse).reserved_incoming==
                    b.cargo+other_pot.cargo,"two Pottery couriers overbooked warehouse");
            }
            if ((before[1]==sim::CourierPhase::ToWarehouse &&
                    b.phase==sim::CourierPhase::Returning &&
                    other_pot.phase==sim::CourierPhase::ToWarehouse) ||
                (before[4]==sim::CourierPhase::ToWarehouse &&
                    other_pot.phase==sim::CourierPhase::Returning &&
                    b.phase==sim::CourierPhase::ToWarehouse)) {
                retained_reservation=true;
                check(expanded.building(sim::BuildingId::Warehouse).reserved_incoming==
                    (b.phase==sim::CourierPhase::ToWarehouse ? b.cargo:other_pot.cargo),
                    "one arrival released the other Pottery reservation");
            }
            if (!checkpoint_world && (shared_clay || shared_pottery) &&
                (expanded.building(sim::BuildingId::Pottery).active_recipe_clay>0 ||
                 expanded.building(static_cast<sim::BuildingId>(9)).active_recipe_clay>0)) {
                unsigned active=0;
                for (unsigned id=1;id<=5;++id)
                    if (expanded.courier(static_cast<sim::CourierId>(id)).phase==
                        sim::CourierPhase::ToWarehouse) ++active;
                if (active>=2) checkpoint_world=expanded;
            }
        }
        check(expanded.building(static_cast<sim::BuildingId>(9)).recipes_completed>0 &&
              expanded.building(static_cast<sim::BuildingId>(9)).input_clay>=0 &&
              arrivals[3]>0 && arrivals[4]>0 &&
              expanded.pottery_completed_total()-initial_expanded>
                  control.pottery_completed_total()-initial_control,
              "expanded production did not outperform control via real deliveries");
        check(shared_clay && shared_pottery && retained_reservation,
              "concurrent shared-target deliveries were not exercised");
        std::uint64_t control_needs=0,expanded_needs=0;
        for (unsigned id=4;id<=6;++id) {
            control_needs+=control.building(static_cast<sim::BuildingId>(id)).fulfilled_demand;
            expanded_needs+=expanded.building(static_cast<sim::BuildingId>(id)).fulfilled_demand;
        }
        check(expanded_needs>control_needs,
              "expanded industry did not improve actually fulfilled household needs");
        check(checkpoint_world.has_value(),"no two active deliveries with recipe found");
        auto reference=*checkpoint_world;
        const auto root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-industry-"+std::to_string(std::random_device{}()));
        struct Cleanup { fs::path path; ~Cleanup(){ std::error_code error;
            fs::remove_all(path,error); } } cleanup{root};
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        { std::ofstream out(root/"data/Cities/Synthetic.map",std::ios::binary);
          out<<"independently authored industry map identity"; }
        const fs::path map="Cities/Synthetic.map",save_path=root/"saves/industry.json";
        save::write_save(save_path,save::make_document(root/"data",map,mask(),reference),
            root/"data",mask());
        auto loaded=save::restore_save(save::read_save(save_path),root/"data",mask());
        check(loaded.snapshot()==reference.snapshot(),"industry JSON roundtrip changed state");
        { const auto before=reference.snapshot();
          auto corrupt=before;
          corrupt.couriers[4].target=sim::BuildingId::Household;
          bool rejected=false;
          try { reference.import_snapshot(corrupt); }
          catch (const std::exception&) { rejected=true; }
          check(rejected && reference.snapshot()==before,
                "invalid import partially replaced the running industry world"); }
        { std::ifstream in(save_path); nlohmann::json j; in>>j; in.close();
          std::reverse(j["world"]["buildings"].begin(),j["world"]["buildings"].end());
          std::reverse(j["world"]["couriers"].begin(),j["world"]["couriers"].end());
          { std::ofstream out(save_path,std::ios::trunc); out<<j.dump(); }
          check(save::restore_save(save::read_save(save_path),root/"data",mask()).snapshot()==
              reference.snapshot(),"JSON array order changed industry identity");
          const auto rejects=[&](const char* why,const auto& edit) {
              auto bad=j; edit(bad);
              { std::ofstream out(save_path,std::ios::trunc); out<<bad.dump(); }
              bool failed=false;
              try { (void)save::restore_save(save::read_save(save_path),root/"data",mask()); }
              catch (const std::exception&) { failed=true; }
              check(failed,why);
          };
          rejects("forged delivery reservation accepted",[](auto& bad) {
              int target=0;
              for (const auto& c:bad["world"]["couriers"])
                  if (c["phase"]==1) { target=c["target"].template get<int>(); break; }
              for (auto& b:bad["world"]["buildings"])
                  if (b["id"]==target) b["reserved_incoming"]=0;
          });
          rejects("forged courier target accepted",[](auto& bad) {
              for (auto& c:bad["world"]["couriers"])
                  if (c["id"]==5) c["target"]=4;
          });
          rejects("forged Clay cursor accepted",[](auto& bad) {
              for (auto& c:bad["world"]["couriers"])
                  if (c["id"]==1) c["last_dispatched_pottery"]=3;
          });
          rejects("colliding next production ID accepted",[](auto& bad) {
              bad["world"]["next_production_id"]=9;
          });
        }
        for (int i=0;i<2000;++i) {
            if (i==111) {
                check(reference.execute({sim::CommandType::PlaceRoad,{12,6}}).accepted &&
                      loaded.execute({sim::CommandType::PlaceRoad,{12,6}}).accepted,
                      "equal continuation road command failed");
            }
            reference.tick(); loaded.tick();
            check(reference.snapshot()==loaded.snapshot() &&
                  reference.production_balance_valid() && reference.navigation_valid(),
                  "industry JSON continuation diverged");
        }
        // Two independent Pottery couriers approach one warehouse. Cutting a
        // future edge of the second route must leave the first delivery intact.
        check(interruption_world.has_value(),
              "no simultaneous warehouse flights with cuttable future edge");
        auto interrupted=*interruption_world;
        check(interrupted.execute({sim::CommandType::RemoveRoad,{6,4}}).accepted,
              "future Pottery route could not be cut");
        bool first_arrived=false,second_waiting=false;
        for (int i=0;i<200;++i) {
            interrupted.tick();
            const auto& first=interrupted.courier(sim::CourierId::Pottery);
            const auto& second=interrupted.courier(static_cast<sim::CourierId>(5));
            first_arrived=first_arrived || first.phase==sim::CourierPhase::Returning;
            second_waiting=second_waiting || (second.route_pending && second.edge_progress==0);
            if (first_arrived && second_waiting) break;
        }
        const auto& waiting=interrupted.courier(static_cast<sim::CourierId>(5));
        check(first_arrived && second_waiting && waiting.target==sim::BuildingId::Warehouse &&
              waiting.cargo>0 &&
              interrupted.building(sim::BuildingId::Warehouse).reserved_incoming==waiting.cargo,
              "first warehouse arrival lost the second waiting reservation");
        save::write_save(save_path,save::make_document(root/"data",map,mask(),interrupted),
            root/"data",mask());
        auto repaired=save::restore_save(save::read_save(save_path),root/"data",mask());
        check(repaired.snapshot()==interrupted.snapshot(),"waiting industry save changed state");
        road(interrupted,6,4); road(repaired,6,4);
        for (int i=0;i<2000;++i) {
            interrupted.tick(); repaired.tick();
            check(interrupted.snapshot()==repaired.snapshot() &&
                  interrupted.production_balance_valid() && interrupted.navigation_valid(),
                  "waiting industry repair diverged after save");
        }
        auto clay_wait=expanded;
        bool clay_cuttable=false;
        for (int i=0;i<10000 && !clay_cuttable;++i) {
            clay_wait.tick();
            const auto& c=clay_wait.courier(static_cast<sim::CourierId>(4));
            if (c.phase!=sim::CourierPhase::ToWarehouse ||
                c.target!=sim::BuildingId::Pottery || c.route_pending ||
                !clay_wait.validate({sim::CommandType::RemoveRoad,{2,4}}).accepted) continue;
            for (std::size_t step=c.path_vertex+2;step<c.path.size();++step)
                if (c.path[step]==sim::Cell{2,4}) clay_cuttable=true;
        }
        check(clay_cuttable,"second Clay courier never exposed a removable future road");
        const auto booked=clay_wait.courier(static_cast<sim::CourierId>(4));
        check(clay_wait.execute({sim::CommandType::RemoveRoad,{2,4}}).accepted,
              "second Clay future road removal failed");
        bool clay_waiting=false;
        for (int i=0;i<100 && !clay_waiting;++i) {
            clay_wait.tick();
            const auto& c=clay_wait.courier(static_cast<sim::CourierId>(4));
            clay_waiting=c.route_pending && c.edge_progress==0;
        }
        check(clay_waiting &&
              clay_wait.courier(static_cast<sim::CourierId>(4)).target==booked.target &&
              clay_wait.courier(static_cast<sim::CourierId>(4)).cargo==booked.cargo &&
              clay_wait.building(booked.target).reserved_incoming>=booked.cargo,
              "second Clay courier changed target or reservation while waiting");
        road(clay_wait,2,4);
        bool clay_arrived=false;
        for (int i=0;i<200 && !clay_arrived;++i) {
            clay_wait.tick();
            clay_arrived=clay_wait.courier(static_cast<sim::CourierId>(4)).phase==
                sim::CourierPhase::Returning;
        }
        check(clay_arrived && clay_wait.production_balance_valid() &&
              clay_wait.navigation_valid(),"second Clay courier did not resume after repair");
        auto long_run=base();
        put(long_run,sim::CommandType::PlaceClaySource,0,5);
        put(long_run,sim::CommandType::PlacePottery,3,5);
        grow(long_run);
        std::optional<sim::WorldSnapshot> same_tick_fixture;
        for (int i=0;i<30000;++i) {
            if (!same_tick_fixture &&
                long_run.courier(sim::CourierId::Household).phase==sim::CourierPhase::Returning &&
                long_run.building(sim::BuildingId::Warehouse).pottery_stock>0) {
                bool receiver=false,arriving=false;
                for (unsigned id=4;id<=6;++id) {
                    const auto& h=long_run.building(static_cast<sim::BuildingId>(id));
                    receiver=receiver || h.pottery_stock+h.reserved_incoming<
                        sim::Rules::household_capacity;
                }
                for (unsigned id : {2U,5U}) {
                    const auto& c=long_run.courier(static_cast<sim::CourierId>(id));
                    arriving=arriving || (c.phase==sim::CourierPhase::ToWarehouse &&
                        !c.route_pending && c.edge_progress==sim::Rules::edge_ticks-1 &&
                        c.path_vertex+2==c.path.size());
                }
                if (receiver && arriving) same_tick_fixture=long_run.snapshot();
            }
            long_run.tick();
            check(long_run.production_balance_valid() && long_run.navigation_valid(),
                  "30,000-tick industry balance or navigation failed");
        }
        check(same_tick_fixture.has_value(),
              "no bounded fixture with approaching Pottery and available stock");
        auto& idle_supplier=same_tick_fixture->couriers[2];
        idle_supplier.phase=sim::CourierPhase::IdleAtWorkshop;
        idle_supplier.path.clear(); idle_supplier.path_vertex=0;
        idle_supplier.edge_progress=0; idle_supplier.route_pending=false;
        idle_supplier.route_checked_revision.reset();
        auto simultaneous=sim::World::restore(*same_tick_fixture,mask());
        const auto before_stock=simultaneous.building(sim::BuildingId::Warehouse).pottery_stock;
        int incoming=0;
        for (unsigned id : {2U,5U}) {
            const auto& c=simultaneous.courier(static_cast<sim::CourierId>(id));
            if (c.phase==sim::CourierPhase::ToWarehouse && !c.route_pending &&
                c.edge_progress==sim::Rules::edge_ticks-1 &&
                c.path_vertex+2==c.path.size()) incoming+=c.cargo;
        }
        simultaneous.tick();
        check(simultaneous.courier(sim::CourierId::Household).phase==
                  sim::CourierPhase::ToWarehouse &&
              simultaneous.building(sim::BuildingId::Warehouse).pottery_stock==
                  before_stock+incoming-
                  simultaneous.courier(sim::CourierId::Household).cargo &&
              simultaneous.production_balance_valid(),
              "same-tick warehouse receipt and household shipment lost goods");
        for (unsigned id=4;id<=6;++id)
            check(long_run.building(static_cast<sim::BuildingId>(id)).consumed_total>1,
                  "industry long run did not repeatedly supply each house");
        check(long_run.building(sim::BuildingId::ClaySource).clay_extracted>100 &&
              long_run.building(static_cast<sim::BuildingId>(8)).clay_extracted>100 &&
              long_run.building(sim::BuildingId::Pottery).recipes_completed>50 &&
              long_run.building(static_cast<sim::BuildingId>(9)).recipes_completed>50,
              "industry long run did not keep both branches productive");
        std::cout<<"industry control="<<control.pottery_completed_total()-initial_control
                 <<" expanded="<<expanded.pottery_completed_total()-initial_expanded
                 <<" arrivals="<<arrivals[0]<<','<<arrivals[1]<<','<<arrivals[2]<<','
                 <<arrivals[3]<<','<<arrivals[4]
                 <<" fulfilled="<<control_needs<<','<<expanded_needs<<'\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"industry test failed: "<<e.what()<<'\n'; return 1;
    }
}
