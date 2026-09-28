#include "app/AutosaveController.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace recovery=openemperor::persistence;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F&& action,const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(64,1); }
std::string file_bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
sim::BuildingId by_kind(const sim::World& world,sim::Object kind) {
    for (const auto& building:world.buildings()) if (building.placed && building.kind==kind)
        return building.id;
    throw std::runtime_error("building kind missing");
}
void place(sim::World& world,sim::CommandType type,sim::Cell cell) {
    const auto result=world.execute({type,cell});
    if (!result.accepted || !result.changed)
        throw std::runtime_error(std::string("complex recovery placement failed: ")+result.reason);
}
std::size_t checkpoints(const recovery::RecoveryHistory& history) {
    return static_cast<std::size_t>(std::count_if(history.entries.begin(),history.entries.end(),
        [](const auto& entry){return !entry.start_point;}));
}
}

int main(int argc,char* argv[]) {
    try {
        if (argc==3 && std::string_view(argv[1])=="--process-write") {
            const fs::path shared=argv[2];fs::create_directories(shared/"data/Cities");
            { std::ofstream out(shared/"data/Cities/Test.map",std::ios::binary);out<<"process synthetic map"; }
            sim::World process_world(8,8,mask());
            openemperor::AutosaveController process(shared/"app",shared/"data",true);
            auto doc=recovery::make_document(shared/"data","Cities/Test.map",mask(),process_world);
            check(process.begin(doc,mask()).kind==openemperor::AutosaveResult::Kind::Saved,
                  "process A start point failed");
            for (std::uint64_t i=0;i<openemperor::autosave_interval_ticks;++i) process_world.tick();
            doc=recovery::make_document(shared/"data","Cities/Test.map",mask(),process_world);
            check(process.poll(doc,mask(),true).kind==openemperor::AutosaveResult::Kind::Saved,
                  "process A checkpoint failed");
            return 0;
        }
        if (argc==3 && std::string_view(argv[1])=="--process-read") {
            const fs::path shared=argv[2];
            recovery::RecoveryStore store(shared/"app",shared/"data");
            const auto found=store.catalog();
            check(found.histories.size()==1 && found.histories[0].entries.size()==2,
                  "process B did not find process A history");
            const auto& source=found.histories[0].entries.front();
            check(!source.start_point && source.loadable(),"process B did not select latest checkpoint");
            auto doc=recovery::read_save(source.path);
            auto restored=recovery::restore_save(doc,shared/"data",mask());
            check(restored.snapshot()==doc.world,"process B restore differed immediately");
            openemperor::AutosaveController branch(shared/"app",shared/"data",true);
            check(branch.begin(doc,mask(),recovery::RecoveryParent{source.history_id,source.sequence}).kind==
                  openemperor::AutosaveResult::Kind::Saved,"process B branch failed");
            check(branch.store().catalog().histories.size()==2,
                  "process B branch removed its source history");
            return 0;
        }
        const auto root=fs::canonical(fs::temp_directory_path())/ ("openemperor-recovery-test-"+
            std::to_string(std::random_device{}()));
        struct Cleanup { fs::path path; ~Cleanup(){std::error_code ec;fs::remove_all(path,ec);} } cleanup{root};
        fs::create_directories(root/"data/Cities");
        { std::ofstream out(root/"data/Cities/Test.map",std::ios::binary);out<<"synthetic map"; }
        const fs::path map="Cities/Test.map";
        sim::World world(8,8,mask());
        auto document=[&]{return recovery::make_document(root/"data",map,mask(),world);};

        openemperor::AutosaveController off(root/"off",root/"data",false);
        check(off.begin(document(),mask()).kind==openemperor::AutosaveResult::Kind::None,
              "disabled autosave created a start point");
        for (int i=0;i<2400;++i) world.tick();
        check(off.poll(document(),mask(),true).kind==openemperor::AutosaveResult::Kind::None &&
              !fs::exists(root/"off/recovery"/off.history_id()),"disabled autosave wrote a checkpoint");

        sim::World scheduled(8,8,mask());
        auto scheduled_document=[&]{return recovery::make_document(root/"data",map,mask(),scheduled);};
        openemperor::AutosaveController controller(root/"app",root/"data",true);
        const auto began=controller.begin(scheduled_document(),mask());
        if (began.kind!=openemperor::AutosaveResult::Kind::Saved)
            throw std::runtime_error("start point was not saved: "+began.message);
        const auto history=controller.history_id();
        const auto manual=root/"manual.json";
        recovery::write_save(manual,scheduled_document(),root/"data",mask());
        const auto manual_before=file_bytes(manual);
        for (int i=0;i<1199;++i) scheduled.tick();
        check(controller.poll(scheduled_document(),mask(),true).kind==openemperor::AutosaveResult::Kind::None,
              "checkpoint saved before interval");
        scheduled.tick();
        check(controller.poll(scheduled_document(),mask(),false).kind==openemperor::AutosaveResult::Kind::None,
              "unsafe point was saved");
        scheduled.tick();
        check(controller.poll(scheduled_document(),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved,
              "crossed interval was not saved");
        check(controller.poll(scheduled_document(),mask(),true).kind==openemperor::AutosaveResult::Kind::None,
              "same tick was saved twice");
        for (int save=0;save<6;++save) {
            for (std::uint64_t i=0;i<openemperor::autosave_interval_ticks+17;++i) scheduled.tick();
            check(controller.poll(scheduled_document(),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved,
                  "later checkpoint missing");
        }
        auto catalog=controller.store().catalog();
        check(catalog.histories.size()==1 && catalog.histories[0].history_id==history,
              "history missing from catalog");
        check(checkpoints(catalog.histories[0])==recovery::recovery_checkpoint_limit,
              "periodic checkpoint rotation failed");
        check(std::count_if(catalog.histories[0].entries.begin(),catalog.histories[0].entries.end(),
              [](const auto& e){return e.start_point;})==1,"protected start point was rotated");
        check(file_bytes(manual)==manual_before,"autosave changed an unrelated manual save file");

        const auto before=catalog.histories[0].entries;
        for (auto fault:{recovery::WriteFault::BeforeWrite,recovery::WriteFault::AfterPartialWrite,
                         recovery::WriteFault::BeforeRename}) {
            rejects([&]{controller.store().write_checkpoint(history,scheduled_document(),mask(),
                {fault,{}});},"checkpoint save fault did not fail");
            catalog=controller.store().catalog();
            check(catalog.histories[0].entries.size()==before.size(),"failed save changed rotation");
        }
        rejects([&]{controller.store().write_checkpoint(history,scheduled_document(),mask(),
            {{},openemperor::platform::AtomicWriteFault::BeforeRename});},
            "checkpoint metadata fault did not fail");
        catalog=controller.store().catalog();
        check(catalog.histories[0].entries.size()==before.size(),"metadata failure changed rotation");
        for (std::uint64_t i=0;i<openemperor::autosave_interval_ticks;++i) scheduled.tick();
        const auto failed=controller.poll(scheduled_document(),mask(),true,
            {recovery::WriteFault::BeforeRename,{}});
        check(failed.kind==openemperor::AutosaveResult::Kind::Failed &&
              failed.message.find("previous recovery points are unchanged")!=std::string::npos,
              "autosave fault was not reported");

        const recovery::RecoveryParent parent{history,catalog.histories[0].entries.front().sequence};
        openemperor::AutosaveController branch(root/"app",root/"data",true);
        check(branch.begin(scheduled_document(),mask(),parent).kind==openemperor::AutosaveResult::Kind::Saved,
              "branch start point failed");
        catalog=branch.store().catalog();
        check(catalog.histories.size()==2,"branch replaced original history");
        check(std::any_of(catalog.histories.begin(),catalog.histories.end(),[&](const auto& h) {
            return h.history_id==branch.history_id() && h.parent && h.parent->history_id==history;
        }),"branch parent was not recorded");

        recovery::RecoveryStore tiny(root/"tiny",root/"data",1);
        rejects([&]{tiny.create_history(scheduled_document(),mask());},"storage budget accepted checkpoint");
        rejects([&]{branch.store().write_checkpoint("../outside",scheduled_document(),mask());},
                "unsafe recovery history path accepted");

        recovery::RecoveryStore missing_store(root/"missing",root/"data");
        const auto missing_id=missing_store.create_history(scheduled_document(),mask());
        fs::remove(missing_store.root()/missing_id/"start.json");
        auto missing_catalog=missing_store.catalog();
        check(missing_catalog.histories.size()==1 && !missing_catalog.histories[0].entries.empty() &&
              !missing_catalog.histories[0].entries[0].error.empty(),
              "missing checkpoint was not isolated as invalid");

        recovery::RecoveryStore version_store(root/"version",root/"data");
        const auto version_id=version_store.create_history(scheduled_document(),mask());
        const auto metadata_path=version_store.root()/version_id/"history.json";
        { std::ifstream input(metadata_path);std::string value((std::istreambuf_iterator<char>(input)),{});
          const auto at=value.find("\"version\": 1");check(at!=std::string::npos,"metadata fixture version missing");
          value.replace(at,12,"\"version\": 99");std::ofstream output(metadata_path,std::ios::trunc);output<<value; }
        check(!version_store.catalog().histories[0].error.empty(),
              "unknown recovery metadata version was accepted");

        auto wrong_hash=scheduled_document();wrong_hash.map_sha256=std::string(64,'0');
        recovery::RecoveryStore hash_store(root/"hash",root/"data");
        rejects([&]{hash_store.create_history(wrong_hash,mask());},
                "recovery checkpoint with wrong map hash was accepted");

        recovery::RecoveryStore save_error_store(root/"save-errors",root/"data");
        const auto malformed_id=save_error_store.create_history(scheduled_document(),mask());
        { std::ofstream out(save_error_store.root()/malformed_id/"start.json",std::ios::trunc);out<<"{"; }
        const auto rule_id=save_error_store.create_history(scheduled_document(),mask());
        const auto rule_path=save_error_store.root()/rule_id/"start.json";
        auto rule_json=nlohmann::json::parse(file_bytes(rule_path));
        rule_json["rules"]["version"]=999;
        { std::ofstream out(rule_path,std::ios::trunc);out<<rule_json.dump(); }
        const auto save_errors=save_error_store.catalog();
        check(save_errors.histories.size()==2 &&
              std::all_of(save_errors.histories.begin(),save_errors.histories.end(),[](const auto& h) {
                  return h.entries.size()==1 && !h.entries[0].error.empty();
              }),"malformed or unknown-rule recovery save was loadable");

        std::error_code symlink_error;
        const auto link_name="history-1-abcdef12";
        fs::create_directory_symlink(root/"data",branch.store().root()/link_name,symlink_error);
        if (!symlink_error) {
            const auto symlink_catalog=branch.store().catalog();
            check(std::any_of(symlink_catalog.histories.begin(),symlink_catalog.histories.end(),
                [&](const auto& h){return h.history_id==link_name && !h.error.empty();}),
                "recovery symlink was not rejected visibly");
            fs::remove(branch.store().root()/link_name);
        }

        auto corrupt=catalog.histories.front();
        { std::ofstream out(root/"app/recovery"/corrupt.history_id/"history.json",std::ios::trunc);out<<"{"; }
        catalog=branch.store().catalog();
        check(std::any_of(catalog.histories.begin(),catalog.histories.end(),[](const auto& h){return !h.error.empty();}),
              "corrupt metadata was not visible");

        const auto process_root=root/"process-restart";
        const std::string executable=fs::absolute(argv[0]).string();
        const auto invoke=[&](std::string_view mode) {
            const std::string command="\""+executable+"\" "+std::string(mode)+" \""+
                process_root.string()+"\"";
            return std::system(command.c_str());
        };
        check(invoke("--process-write")==0,"separate process A failed");
        check(invoke("--process-read")==0,"separate process B failed");

        // Exercise RecoveryStore with a normally played schema-12 state containing operation
        // controls, population, service expiry, a recipe, cargo and reservations.
        constexpr int complex_width=32,complex_height=8;
        std::vector<std::uint8_t> complex_mask(complex_width*complex_height,1);
        sim::World complex(complex_width,complex_height,complex_mask,
                           sim::RulesProfile::CityV11,3);
        place(complex,sim::CommandType::PlaceClaySource,{0,2});
        place(complex,sim::CommandType::PlacePottery,{0,5});
        place(complex,sim::CommandType::PlaceWarehouse,{3,2});
        place(complex,sim::CommandType::PlaceFarm,{6,5});
        place(complex,sim::CommandType::PlaceMarket,{10,5});
        place(complex,sim::CommandType::PlaceServicePost,{20,2});
        for (int x=0;x<=20;++x) place(complex,sim::CommandType::PlaceRoad,{x,4});
        place(complex,sim::CommandType::PlaceRoad,{20,3});
        for (const auto cell:{sim::Cell{13,2},sim::Cell{13,5},sim::Cell{16,2}})
            place(complex,sim::CommandType::PlaceHousehold,cell);
        for (int i=0;i<900;++i) complex.tick();
        place(complex,sim::CommandType::PlaceHousehold,{16,5});
        bool rich_state=false;
        for (int i=0;i<10000 && !rich_state;++i) {
            complex.tick();
            const auto& pottery=complex.building(by_kind(complex,sim::Object::Pottery));
            bool cargo=false,service=false;
            for (const auto& courier:complex.couriers())
                cargo=cargo || (courier.cargo>0 && courier.reserved>0);
            for (const auto& building:complex.buildings())
                service=service || (building.kind==sim::Object::Household &&
                                    building.service_until_tick>complex.ticks());
            int minimum=1000,maximum=0;
            for (const auto& building:complex.buildings()) if (building.kind==sim::Object::Household) {
                minimum=std::min(minimum,building.population);maximum=std::max(maximum,building.population);
            }
            rich_state=pottery.active_recipe_clay>0 && pottery.progress>0 && cargo && service &&
                       minimum<maximum;
        }
        check(rich_state,"complex recovery state was not reached through normal ticks");
        const auto farm=by_kind(complex,sim::Object::Farm);
        const auto market=by_kind(complex,sim::Object::Market);
        check(complex.execute(sim::set_building_operation(farm,false)).changed &&
              complex.execute(sim::set_building_workforce_priority(
                  market,sim::WorkforcePriority::High)).changed,
              "complex operation controls failed");
        auto complex_document=recovery::make_document(root/"data",map,complex_mask,complex);
        recovery::RecoveryStore complex_store(root/"complex",root/"data");
        const auto complex_id=complex_store.create_history(complex_document,complex_mask);
        const auto complex_entry=complex_store.catalog().histories[0].entries[0];
        auto continued=recovery::restore_save(recovery::read_save(complex_entry.path),
                                              root/"data",complex_mask);
        check(continued.snapshot()==complex.snapshot() &&
              !continued.building(farm).operating_enabled &&
              continued.building(market).workforce_priority==sim::WorkforcePriority::High,
              "complex recovery snapshot differed immediately");
        for (int i=0;i<500;++i) {complex.tick();continued.tick();}
        check(continued.snapshot()==complex.snapshot() && !complex_id.empty(),
              "complex recovery continuation diverged");
        auto crisis=recovery::restore_save(recovery::read_save(complex_entry.path),
                                           root/"data",complex_mask);
        const auto missed_before=std::accumulate(crisis.buildings().begin(),crisis.buildings().end(),
            std::uint64_t{0},[](std::uint64_t total,const auto& building) {
                return total+building.missed_demand;
            });
        bool removed=false;
        for (int i=0;i<200 && !removed;++i) {
            const auto result=crisis.execute({sim::CommandType::RemoveRoad,{20,3}});
            removed=result.accepted && result.changed;
            if (!removed) crisis.tick();
        }
        check(removed,"player-style recovery test could not cut the Service road");
        for (int i=0;i<1600;++i) crisis.tick();
        const auto missed_after=std::accumulate(crisis.buildings().begin(),crisis.buildings().end(),
            std::uint64_t{0},[](std::uint64_t total,const auto& building) {
                return total+building.missed_demand;
            });
        check(missed_after>missed_before,"Service road cut did not produce a later supply failure");
        auto recovered=recovery::restore_save(recovery::read_save(complex_entry.path),
                                              root/"data",complex_mask);
        check(recovered.snapshot()==complex_document.world,
              "earlier player-style recovery point was not restored exactly");
        openemperor::AutosaveController alternate(root/"complex",root/"data",true);
        check(alternate.begin(recovery::make_document(root/"data",map,complex_mask,recovered),
              complex_mask,recovery::RecoveryParent{complex_id,0}).kind==
              openemperor::AutosaveResult::Kind::Saved &&
              alternate.store().catalog().histories.size()==2,
              "alternate continuation did not preserve the crisis branch source");

        std::cout<<"autosave scheduling, rotation, faults, budget, branching and process restart checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"recovery test failed: "<<error.what()<<'\n';return 1;
    }
}
