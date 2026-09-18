#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& action,const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
void command(sim::World& world,sim::CommandType type,int x) {
    check(world.execute({type,{x,1}}).accepted,"synthetic placement failed");
}
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(64,1); }
sim::World production(int first_roads=2,int second_roads=2) {
    sim::World w(16,4,mask(),sim::RulesProfile::ProductionV2);
    command(w,sim::CommandType::PlaceClaySource,0);
    for (int x=1;x<=first_roads;++x) command(w,sim::CommandType::PlaceRoad,x);
    const int pottery=first_roads+1;
    command(w,sim::CommandType::PlacePottery,pottery);
    for (int x=pottery+1;x<=pottery+second_roads;++x)
        command(w,sim::CommandType::PlaceRoad,x);
    command(w,sim::CommandType::PlaceWarehouse,pottery+second_roads+1);
    return w;
}
sim::World logistics() {
    sim::World w(16,4,mask());
    command(w,sim::CommandType::PlaceWorkshop,0);
    command(w,sim::CommandType::PlaceRoad,1); command(w,sim::CommandType::PlaceRoad,2);
    command(w,sim::CommandType::PlaceWarehouse,3);
    return w;
}
std::string bytes(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
void overwrite(const fs::path& path,const nlohmann::json& json) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc); out<<json.dump();
    check(out.good(),"test fixture write failed");
}
} // namespace

int main() {
    try {
        const auto root=fs::canonical(fs::temp_directory_path())/fs::path("openemperor-save-test-"+
            std::to_string(std::random_device{}()));
        struct Cleanup { fs::path path; ~Cleanup() { std::error_code e; fs::remove_all(path,e); } } cleanup{root};
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        const auto map=root/"data/Cities/Synthetic.map";
        { std::ofstream out(map,std::ios::binary); out<<"synthetic map identity only"; }
        const fs::path relative="Cities/Synthetic.map";
        const auto target=root/"saves/resume.json";
        const auto roundtrip=[&](sim::World& w) {
            auto state=w.snapshot();
            auto document=save::make_document(root/"data",relative,mask(),w);
            save::write_save(target,document,root/"data",mask());
            auto restored=save::restore_save(save::read_save(target),root/"data",mask());
            check(restored.snapshot()==state,"boundary roundtrip mismatch");
        };
        { sim::World empty(16,4,mask()); roundtrip(empty);
          sim::World empty_v2(16,4,mask(),sim::RulesProfile::ProductionV2); roundtrip(empty_v2);
          command(empty,sim::CommandType::PlaceWorkshop,0);
          command(empty,sim::CommandType::PlaceWarehouse,3);
          for (int i=0;i<300;++i) empty.tick();
          check(empty.workshop_stock()>0 && empty.courier_cargo()==0,"v1 no-route expected state");
          roundtrip(empty);
          command(empty_v2,sim::CommandType::PlaceClaySource,0);
          command(empty_v2,sim::CommandType::PlacePottery,3);
          for (int i=0;i<120;++i) empty_v2.tick();
          check(empty_v2.building(sim::BuildingId::ClaySource).output>0 &&
                empty_v2.courier(sim::CourierId::Clay).cargo==0,"v2 no-route expected state");
          roundtrip(empty_v2);
        }
        auto world=production();
        // Establish the resume point solely through ordinary commands and ticks.
        bool reached=false;
        for (int a=2;a<=6 && !reached;++a) for (int b=2;b<=6 && !reached;++b) {
            world=production(a,b);
            for (int i=0;i<10000;++i) {
                world.tick();
                const auto& clay=world.courier(sim::CourierId::Clay);
                const auto& pottery=world.courier(sim::CourierId::Pottery);
                if (world.building(sim::BuildingId::Pottery).active_recipe_clay==2 &&
                    clay.phase==sim::CourierPhase::ToWarehouse && clay.cargo>0 &&
                    clay.edge_progress>0 && pottery.phase==sim::CourierPhase::Returning) {
                    reached=true; break;
                }
            }
        }
        check(reached,"no simultaneous active recipe, outbound cargo and returner found");
        check(world.production_balance_valid(),"pre-save production balance invalid");
        const auto expected=world.snapshot();
        check(expected.ticks==500 && expected.buildings[1].active_recipe_clay==2 &&
              expected.couriers[0].cargo>0 &&
              expected.couriers[0].reserved==expected.couriers[0].cargo &&
              expected.couriers[0].edge_progress>0 && expected.couriers[1].cargo==0,
              "independent mid-flight expected values missing");
        auto doc=save::make_document(root/"data",relative,mask(),world);
        save::write_save(target,doc,root/"data",mask());
        auto parsed=save::read_save(target);
        auto resumed=save::restore_save(parsed,root/"data",mask());
        check(resumed.snapshot()==expected,"immediate authoritative snapshot differs");
        for (int i=0;i<1200;++i) {
            if (i==11 || i==311) {
                // Rejected commands still advance the command sequence identically.
                const auto a=world.execute({sim::CommandType::PlaceRoad,{0,1}});
                const auto b=resumed.execute({sim::CommandType::PlaceRoad,{0,1}});
                check(a.accepted==b.accepted && a.sequence==b.sequence,"post-resume command differs");
            }
            world.tick(); resumed.tick();
            check(world.snapshot()==resumed.snapshot(),"tick-by-tick resume mismatch");
            check(world.production_balance_valid() && resumed.production_balance_valid(),
                  "post-resume production balance invalid");
        }
        check(world.pottery_completed_total()>expected.pottery_completed_total &&
              world.building(sim::BuildingId::Warehouse).pottery_stock>0,
              "recipe or delivery did not complete after resume");
        { auto full=production();
          for (int i=0;i<12000;++i) full.tick();
          check(full.building(sim::BuildingId::Warehouse).pottery_stock==sim::Rules::warehouse_capacity,
                "synthetic warehouse did not reach full backpressure");
          roundtrip(full); }

        auto v1=logistics(); for (int i=0;i<137;++i) v1.tick();
        auto old=save::make_document(root/"data",relative,mask(),v1);
        save::write_save(target,old,root/"data",mask());
        const auto previous=bytes(target);
        for (auto fault:{save::WriteFault::BeforeWrite,save::WriteFault::AfterPartialWrite,
                         save::WriteFault::BeforeRename}) {
            rejects([&]{ save::write_save(target,doc,root/"data",mask(),fault); },"fault injection did not fail");
            check(bytes(target)==previous,"failed write changed old save");
            check(std::distance(fs::directory_iterator(target.parent_path()),fs::directory_iterator{})==1,
                  "failed write left temporary save behind");
        }
        save::write_save(target,doc,root/"data",mask());
        check(bytes(target)!=previous,"successful replace kept old save");
        const auto once=bytes(target);
        save::write_save(target,doc,root/"data",mask());
        check(bytes(target)==once,"repeat save changed bytes");
        auto v1doc=save::make_document(root/"data",relative,mask(),v1);
        save::write_save(target,v1doc,root/"data",mask());
        auto v1resumed=save::restore_save(save::read_save(target),root/"data",mask());
        check(v1resumed.snapshot()==v1.snapshot(),"v1 roundtrip mismatch");
        for (int i=0;i<1000;++i) { v1.tick(); v1resumed.tick();
            check(v1.snapshot()==v1resumed.snapshot(),"v1 continuation mismatch"); }
        sim::World untouched=logistics();
        auto invalid=untouched.snapshot(); invalid.roads.push_back({0,1});
        const auto before=untouched.snapshot();
        rejects([&]{ untouched.import_snapshot(invalid); },"invalid import accepted");
        check(untouched.snapshot()==before,"invalid import mutated world");

        save::write_save(target,doc,root/"data",mask());
        const auto valid=nlohmann::json::parse(bytes(target));
        auto mutate=[&](const auto& edit) {
            auto j=valid; edit(j); overwrite(target,j);
            rejects([&]{ auto parsed_bad=save::read_save(target);
                (void)save::restore_save(parsed_bad,root/"data",mask()); },
                "tampered save accepted");
        };
        mutate([](auto& j){ j["schema_version"]=2; });
        mutate([](auto& j){ j["rules"]["id"]="unknown"; });
        mutate([](auto& j){ j["world"]["ticks"]=1.0; });
        mutate([](auto& j){ j["world"]["command_sequence"]=nlohmann::json::number_unsigned_t(-1); });
        mutate([](auto& j){ j["map"]["sha256"]=std::string(64,'0'); });
        mutate([](auto& j){ j["profiles"]["buildable_sha256"]=std::string(64,'0'); });
        mutate([](auto& j){ j["map"]["relative_path"]="../escape.map"; });
        mutate([](auto& j){ j["world"]["buildings"][1]["id"]=1; });
        mutate([](auto& j){ j["world"]["roads"].push_back(j["world"]["buildings"][0]["cell"]); });
        mutate([](auto& j){ j["world"]["couriers"][0]["path_vertex"]=100000; });
        mutate([](auto& j){ j["world"]["couriers"][0]["path"][1]=nlohmann::json::array({15,3}); });
        mutate([](auto& j){ j["world"]["couriers"][0]["edge_progress"]=10; });
        mutate([](auto& j){ j["world"]["couriers"][0]["good"]=2; });
        mutate([](auto& j){ j["world"]["couriers"][1]["id"]=1; });
        mutate([](auto& j){ j["world"]["buildings"][1]["active_recipe_clay"]=0; });
        mutate([](auto& j){ j["world"]["buildings"][1]["reserved_incoming"]=0; });
        mutate([](auto& j){ j["world"]["clay_extracted_total"]=0; });
        { std::ofstream out(target); out<<"{\"format\":"; }
        rejects([&]{ (void)save::read_save(target); },"truncated JSON accepted");
        { std::ofstream out(target,std::ios::trunc); out<<std::string(8U*1024U*1024U+1,' '); }
        rejects([&]{ (void)save::read_save(target); },"oversized JSON accepted");
        rejects([&]{ save::validate_save_target(root/"data/forbidden.json",root/"data"); },
                "save inside original data accepted");
        fs::create_directory_symlink(root/"data",root/"saves/link");
        rejects([&]{ save::validate_save_target(root/"saves/link/forbidden.json",root/"data"); },
                "symlink target accepted");
        std::cout<<"sandbox save/resume tests passed at tick "<<expected.ticks<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"sandbox save/resume tests failed: "<<error.what()<<'\n'; return 1;
    }
}
