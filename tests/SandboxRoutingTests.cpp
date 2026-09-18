#include "persistence/SandboxSave.h"

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
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(15U*5U,1); }
void place(sim::World& w,sim::CommandType type,int x,int y) {
    check(w.execute({type,{x,y}}).accepted,"synthetic routing placement failed");
}
sim::World chain() {
    sim::World w(15,5,mask(),sim::RulesProfile::ProductionV2);
    place(w,sim::CommandType::PlaceClaySource,0,2);
    for (int x=1;x<=5;++x) place(w,sim::CommandType::PlaceRoad,x,2);
    place(w,sim::CommandType::PlacePottery,6,2);
    for (int x=7;x<=11;++x) place(w,sim::CommandType::PlaceRoad,x,2);
    place(w,sim::CommandType::PlaceWarehouse,12,2);
    // The upper branch is a real, equal-cost-free detour around (3,2).
    for (int x=2;x<=4;++x) place(w,sim::CommandType::PlaceRoad,x,1);
    return w;
}
template<class F> void until(sim::World& w,int limit,F condition,const char* failure) {
    for (int i=0;i<limit;++i) {
        if (condition()) return;
        w.tick();
    }
    check(condition(),failure);
}
bool contains(const std::vector<sim::Cell>& path,sim::Cell cell) {
    for (auto point:path) if (point==cell) return true;
    return false;
}
} // namespace

int main() {
    try {
        const auto root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-routing-"+std::to_string(std::random_device{}()));
        struct Cleanup { fs::path p; ~Cleanup(){ std::error_code e; fs::remove_all(p,e); } } cleanup{root};
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        { std::ofstream out(root/"data/Cities/Synthetic.map",std::ios::binary);
          out<<"independently authored synthetic map identity"; }
        const fs::path relative="Cities/Synthetic.map",target=root/"saves/routing.json";
        auto world=chain();
        check(world.snapshot().rule_version==2,"new production rule version is not 2");
        sim::World v1(3,1,{1,1,1});
        place(v1,sim::CommandType::PlaceRoad,1,0);
        check(!v1.execute({sim::CommandType::RemoveRoad,{1,0}}).accepted &&
              v1.object_at({1,0})==sim::Object::Road,"v1 accepted road removal");
        check(!world.execute({sim::CommandType::RemoveRoad,{14,4}}).accepted,
              "empty road removal accepted");
        check(!world.execute({sim::CommandType::RemoveRoad,{6,2}}).accepted,
              "building removal accepted");
        const auto before_unused=world.road_revision();
        place(world,sim::CommandType::PlaceRoad,14,4);
        check(world.execute({sim::CommandType::RemoveRoad,{14,4}}).accepted &&
              world.road_revision()==before_unused+2,"unused road removal revision mismatch");
        until(world,300,[&] {
            const auto& c=world.courier(sim::CourierId::Clay);
            return c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==sim::Cell{1,2};
        },"no outbound loaded protected edge found");
        const auto before=world.snapshot();
        const auto position=world.courier_position(sim::CourierId::Clay);
        const auto rejected=world.execute({sim::CommandType::RemoveRoad,{2,2}});
        check(!rejected.accepted && world.road_revision()==before.road_revision &&
              world.courier_position(sim::CourierId::Clay)==position,
              "protected edge endpoint was removed or moved");
        const auto accepted=world.execute({sim::CommandType::RemoveRoad,{3,2}});
        check(accepted.accepted && world.road_revision()==before.road_revision+1 &&
              world.courier_position(sim::CourierId::Clay)==position &&
              world.courier(sim::CourierId::Clay).cargo==before.couriers[0].cargo &&
              world.courier(sim::CourierId::Clay).reserved==before.couriers[0].reserved,
              "future road removal moved courier or changed shipment");
        check(world.navigation_valid(),"state immediately after removal is invalid");
        auto doc=save::make_document(root/"data",relative,mask(),world);
        save::write_save(target,doc,root/"data",mask());
        auto after_command=save::restore_save(save::read_save(target),root/"data",mask());
        check(after_command.snapshot()==world.snapshot(),"immediate post-command save mismatch");
        const auto edge_remaining=sim::Rules::edge_ticks-before.couriers[0].edge_progress;
        for (int i=0;i<edge_remaining;++i) world.tick();
        check(world.courier_position(sim::CourierId::Clay)==sim::Position{2.0,2.0} &&
              world.courier(sim::CourierId::Clay).route_pending,
              "begun edge did not complete at protected endpoint");
        world.tick();
        check(!world.courier(sim::CourierId::Clay).route_pending &&
              contains(world.courier(sim::CourierId::Clay).path,{3,1}) &&
              world.courier(sim::CourierId::Clay).edge_progress==1,
              "courier did not choose available upper detour");
        until(world,1000,[&] {
            return world.courier(sim::CourierId::Clay).phase==sim::CourierPhase::IdleAtWorkshop &&
                world.building(sim::BuildingId::Pottery).input_clay>0;
        },"detoured first delivery did not arrive and return");
        const auto delivered_first=world.building(sim::BuildingId::Pottery).input_clay;
        check(delivered_first>0 && world.production_balance_valid(),"detoured delivery balance invalid");

        until(world,1000,[&] {
            const auto& c=world.courier(sim::CourierId::Clay);
            return c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==sim::Cell{1,2} && contains(c.path,{3,1});
        },"no second loaded outbound route through detour found");
        const auto reserved=world.courier(sim::CourierId::Clay).reserved;
        check(world.execute({sim::CommandType::RemoveRoad,{3,1}}).accepted,
              "removing future detour failed");
        until(world,30,[&] {
            const auto& c=world.courier(sim::CourierId::Clay);
            return c.route_pending && c.edge_progress==0;
        },"courier did not wait after both routes were cut");
        const auto waiting_position=world.courier_position(sim::CourierId::Clay);
        const auto& waiting=world.courier(sim::CourierId::Clay);
        check(waiting_position==sim::Position{2.0,2.0} && waiting.cargo==reserved &&
              waiting.reserved==reserved &&
              world.building(sim::BuildingId::Pottery).reserved_incoming==reserved &&
              world.navigation_valid(),"waiting courier lost position, cargo or reservation");
        world.tick(); // One failed BFS for this road revision.
        const auto attempts=world.courier(sim::CourierId::Clay).reroute_attempts;
        const auto goods_before=world.building(sim::BuildingId::Pottery).input_clay;
        for (int i=0;i<35;++i) world.tick();
        check(world.courier(sim::CourierId::Clay).reroute_attempts==attempts &&
              world.courier_position(sim::CourierId::Clay)==waiting_position &&
              world.building(sim::BuildingId::Pottery).input_clay==goods_before &&
              world.courier(sim::CourierId::Clay).cargo==reserved,
              "blocked topology reran BFS, moved or delivered prematurely");
        doc=save::make_document(root/"data",relative,mask(),world);
        save::write_save(target,doc,root/"data",mask());
        const auto parsed=save::read_save(target);
        auto resumed=save::restore_save(parsed,root/"data",mask());
        check(resumed.snapshot()==world.snapshot(),"waiting JSON restore changed authoritative state");
        const auto safe_state=world.snapshot();
        auto invalid_navigation=safe_state;
        invalid_navigation.couriers[0].path={{14,4}};
        try { world.import_snapshot(invalid_navigation);
              throw std::runtime_error("invalid navigation import was accepted"); }
        catch (const std::invalid_argument&) {}
        check(world.snapshot()==safe_state,"invalid navigation import mutated active world");
        const auto repaired_a=world.execute({sim::CommandType::PlaceRoad,{3,2}});
        const auto repaired_b=resumed.execute({sim::CommandType::PlaceRoad,{3,2}});
        check(repaired_a.accepted && repaired_b.accepted &&
              world.snapshot()==resumed.snapshot(),"repair command diverged after load");
        int arrivals=0,returns=0;
        for (int i=0;i<1100;++i) {
            const auto old_phase=world.courier(sim::CourierId::Clay).phase;
            const auto old_reserved=world.courier(sim::CourierId::Clay).reserved;
            world.tick(); resumed.tick();
            check(world.snapshot()==resumed.snapshot() && world.production_balance_valid() &&
                  world.navigation_valid(),"routing continuation diverged or violated balance");
            const auto new_phase=world.courier(sim::CourierId::Clay).phase;
            if (old_phase==sim::CourierPhase::ToWarehouse &&
                new_phase==sim::CourierPhase::Returning) {
                ++arrivals;
                check(old_reserved>0 && world.courier(sim::CourierId::Clay).cargo==0 &&
                      world.building(sim::BuildingId::Pottery).reserved_incoming==0,
                      "arrival did not release exactly one reservation");
            }
            if (old_phase==sim::CourierPhase::Returning &&
                new_phase==sim::CourierPhase::IdleAtWorkshop) ++returns;
        }
        check(arrivals>=1 && returns>=1,"repaired courier did not deliver and return");
        check(world.roads_removed_total()==3 &&
              world.roads_placed_total()-world.roads_removed_total()==world.snapshot().roads.size(),
              "road placement/removal counters differ from live graph");

        // A road added during a valid flight must not redirect that flight.
        auto shorter=chain();
        check(shorter.execute({sim::CommandType::RemoveRoad,{3,2}}).accepted,
              "pre-dispatch detour setup failed");
        until(shorter,300,[&] {
            const auto& c=shorter.courier(sim::CourierId::Clay);
            return c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                contains(c.path,{3,1});
        },"no active detour for shorter-road test");
        const auto old_path=shorter.courier(sim::CourierId::Clay).path;
        place(shorter,sim::CommandType::PlaceRoad,3,2);
        check(shorter.courier(sim::CourierId::Clay).path==old_path &&
              !shorter.courier(sim::CourierId::Clay).route_pending,
              "new shorter road replaced a valid active path");
        until(shorter,50,[&] {
            const auto& c=shorter.courier(sim::CourierId::Clay);
            return c.phase==sim::CourierPhase::ToWarehouse && c.path_vertex>2 &&
                c.path[c.path_vertex]==sim::Cell{3,1};
        },"courier never passed historical detour segment");
        check(shorter.execute({sim::CommandType::RemoveRoad,{2,1}}).accepted,
              "historical road removal rejected");
        check(!shorter.courier(sim::CourierId::Clay).route_pending && shorter.navigation_valid(),
              "historical removal incorrectly invalidated future route");
        auto historical_doc=save::make_document(root/"data",relative,mask(),shorter);
        save::write_save(target,historical_doc,root/"data",mask());
        auto historical_loaded=save::restore_save(save::read_save(target),root/"data",mask());
        check(historical_loaded.snapshot()==shorter.snapshot(),
              "removed historical waypoint made save unloadable");

        // At a vertex, an unbegun next edge is removable; the courier waits
        // on the current placed road, not at its source building.
        auto vertex=chain();
        until(vertex,300,[&] {
            const auto& c=vertex.courier(sim::CourierId::Clay);
            return c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress==0 &&
                c.path[c.path_vertex]==sim::Cell{1,2};
        },"no stationary waypoint before next edge");
        check(vertex.execute({sim::CommandType::RemoveRoad,{2,2}}).accepted,
              "unbegun edge endpoint remained protected");
        vertex.tick();
        check(vertex.courier(sim::CourierId::Clay).route_pending &&
              vertex.courier_position(sim::CourierId::Clay)==sim::Position{1.0,2.0},
              "unbegun edge removal teleported courier");

        // Returners carry nothing. A missing road ahead leaves them at their
        // reached waypoint until the same road is rebuilt.
        auto returning=chain();
        until(returning,500,[&] {
            const auto& c=returning.courier(sim::CourierId::Clay);
            return c.phase==sim::CourierPhase::Returning && c.edge_progress>0 &&
                c.path[c.path_vertex]==sim::Cell{3,2};
        },"no empty returning courier on a protected edge");
        check(returning.execute({sim::CommandType::RemoveRoad,{1,2}}).accepted,
              "future road removal on return failed");
        until(returning,50,[&] {
            const auto& c=returning.courier(sim::CourierId::Clay);
            return c.route_pending && c.edge_progress==0;
        },"returning courier did not stop at missing route");
        const auto return_position=returning.courier_position(sim::CourierId::Clay);
        check(returning.courier(sim::CourierId::Clay).cargo==0 &&
              returning.courier(sim::CourierId::Clay).reserved==0 &&
              return_position && *return_position!=sim::Position{0.0,2.0},
              "empty returner teleported or retained cargo");
        place(returning,sim::CommandType::PlaceRoad,1,2);
        until(returning,100,[&] {
            return returning.courier(sim::CourierId::Clay).phase==sim::CourierPhase::IdleAtWorkshop;
        },"empty returner did not resume after repair");

        // The other courier's edge receives the same protection.
        auto other=chain();
        until(other,1000,[&] {
            const auto& c=other.courier(sim::CourierId::Pottery);
            return c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==sim::Cell{7,2};
        },"no pottery courier on protected edge");
        const auto other_revision=other.road_revision();
        check(!other.execute({sim::CommandType::RemoveRoad,{8,2}}).accepted &&
              other.road_revision()==other_revision,
              "pottery courier's protected endpoint was removed");

        sim::World tie(8,5,std::vector<std::uint8_t>(40,1),sim::RulesProfile::ProductionV2);
        place(tie,sim::CommandType::PlacePottery,6,2);
        place(tie,sim::CommandType::PlaceRoad,2,2);
        for (int x=2;x<=5;++x) {
            place(tie,sim::CommandType::PlaceRoad,x,1);
            place(tie,sim::CommandType::PlaceRoad,x,3);
        }
        place(tie,sim::CommandType::PlaceRoad,5,2);
        const auto equal=tie.find_route({2,2},{6,2});
        check(equal && equal->at(1)==sim::Cell{2,1},
              "equal-cost reroute ignored fixed up-first tie-break");
        const auto two=tie.find_route({5,2},{6,2});
        check(two && two->size()==2 && two->front()==sim::Cell{5,2},
              "road-to-goal two-point remaining route was rejected");
        std::cout<<"routing check: protected_rejection=true detour=true wait=true saved=true "
                 <<"repaired=true arrivals="<<arrivals<<" returns="<<returns<<"\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"routing check failed: "<<error.what()<<'\n'; return 1;
    }
}
