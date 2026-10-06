#include "app/AutosaveController.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"
#include "maps/OriginalMapEntities.h"
#include "maps/SandboxPlacement.h"
#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <locale>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace maps=openemperor::maps;
namespace save=openemperor::persistence;
using Bytes=std::vector<std::uint8_t>;
constexpr int width=228;
constexpr sim::Cell origin{112,112};
const fs::path relative="Cities/Authored.map";
void check(bool ok,const char* why) { if(!ok)throw std::runtime_error(why); }
template<class F> void rejects(F action,std::string_view why) {
    try {action();} catch(const std::exception& e) {
        check(std::string_view(e.what()).find(why)!=std::string_view::npos,"wrong persistence failure diagnosis");return;
    }
    throw std::runtime_error("invalid gate save accepted");
}
void put(Bytes& bytes,std::size_t at,std::uint32_t value,unsigned n=4) {
    for(unsigned i=0;i<n;++i)bytes.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
void append(Bytes& bytes,std::uint32_t value,unsigned n=4) {
    const auto at=bytes.size();bytes.resize(at+n);put(bytes,at,value,n);
}
std::size_t index(sim::Cell p) {return static_cast<std::size_t>(p.y)*width+static_cast<unsigned>(p.x);}
Bytes legacy_mask(unsigned layout) {
    const maps::MapGeometry geometry{84};Bytes mask(width*width);
    for(unsigned y=0;y<width;++y)for(unsigned x=0;x<width;++x)
        mask[y*width+x]=static_cast<std::uint8_t>(geometry.contains({x,y}));
    for(int y=0;y<(layout ? 5:3);++y)for(int x=0;x<(layout ? 3:5);++x)
        mask[index({origin.x+x,origin.y+y})]=0;
    return mask;
}
// Entirely authored compressed map; no original bytes, assets or cached links.
void write_map(const fs::path& data,unsigned layout) {
    Bytes manager;append(manager,1,2);append(manager,1);
    append(manager,0xffff,2);append(manager,0,2);append(manager,10,2);
    const std::string_view name="cGateHouse";manager.insert(manager.end(),name.begin(),name.end());
    Bytes base(181);put(base,0,4,2);base[2]=3;base[5]=1;
    put(base,8,40,2);put(base,10,40,2);put(base,12,112*228+112);
    put(base,16,130,2);put(base,112,layout);put(base,161,7);
    manager.insert(manager.end(),base.begin(),base.end());
    append(manager,1,2);append(manager,1,2);manager.resize(manager.size()+126);append(manager,1,2);
    Bytes raw(maps::original_entities_logical_offset+manager.size());
    const Bytes signature{5,0,0xfe,0xca,0,0,2,0};std::copy(signature.begin(),signature.end(),raw.begin());
    put(raw,84,84);
    const auto mask=legacy_mask(layout);
    for(std::size_t i=0;i<mask.size();++i)
        put(raw,maps::terrain_logical_offset+4*i,mask[i] ? 0x80:0x80000);
    for(int y=0;y<(layout ? 5:3);++y)for(int x=0;x<(layout ? 3:5);++x)
        put(raw,maps::terrain_logical_offset+4*index({origin.x+x,origin.y+y}),0x8008);
    std::copy(manager.begin(),manager.end(),raw.begin()+maps::original_entities_logical_offset);
    Bytes container;append(container,0xfedcbaaa);
    for(std::size_t at=0;at<raw.size();at+=16384) {
        const auto n=std::min<std::size_t>(16384,raw.size()-at);
        uLongf size=compressBound(static_cast<uLong>(n));Bytes compressed(size);
        check(compress2(compressed.data(),&size,raw.data()+at,static_cast<uLong>(n),6)==Z_OK,"map compression failed");
        append(container,0);append(container,static_cast<std::uint32_t>(size));append(container,static_cast<std::uint32_t>(n));
        container.insert(container.end(),compressed.begin(),compressed.begin()+static_cast<std::ptrdiff_t>(size));
    }
    fs::create_directories(data/"Cities");std::ofstream out(data/relative,std::ios::binary);
    out.write(reinterpret_cast<const char*>(container.data()),static_cast<std::streamsize>(container.size()));
    check(out.good(),"authored map write failed");
}
void place(sim::World& w,sim::CommandType type,sim::Cell cell) {
    const auto result=w.execute({type,cell});
    if(!result.accepted || !result.changed)throw std::runtime_error("gate save fixture command: "+result.reason);
}
sim::World playing(const std::shared_ptr<const sim::MapPermissions>& permissions,unsigned layout) {
    sim::World w(permissions,sim::RulesProfile::CityV16,3);
    place(w,sim::CommandType::PlaceHousehold,{106,104});place(w,sim::CommandType::PlaceHousehold,{109,104});
    place(w,sim::CommandType::PlaceClaySource,layout ? sim::Cell{106,113}:sim::Cell{113,106});
    place(w,sim::CommandType::PlacePottery,layout ? sim::Cell{119,113}:sim::Cell{113,119});
    for(int coordinate=108;coordinate<=118;++coordinate) {
        const sim::Cell cell=layout ? sim::Cell{coordinate,114}:sim::Cell{114,coordinate};
        const auto result=w.execute({sim::CommandType::PlaceRoad,cell});
        check(result.accepted && result.changed!=permissions->fixed_passage(cell),"road/passsage fixture command failed");
    }
    return w;
}
const sim::CourierState& clay(const sim::World& w) {
    const auto found=std::find_if(w.couriers().begin(),w.couriers().end(),[](const auto& c){return c.role==sim::CourierRole::Clay;});
    check(found!=w.couriers().end(),"clay courier absent");return *found;
}
template<class P> void until(sim::World& w,P ready) {
    for(int i=0;i<1500 && !ready();++i)w.tick();check(ready(),"gate courier boundary not reached");
}
void equal_continuation(sim::World a,sim::World b,int ticks=250) {
    check(a.snapshot()==b.snapshot(),"gate restore changed paused authoritative state");
    for(int i=0;i<ticks;++i) {a.tick();b.tick();check(a.snapshot()==b.snapshot(),"gate save continuation diverged");}
}
void overwrite(const fs::path& path,const nlohmann::json& j) {std::ofstream out(path);out<<j.dump();check(out.good(),"tamper write failed");}
nlohmann::json json_file(const fs::path& path) {std::ifstream in(path);return nlohmann::json::parse(in);}
void boundary(const fs::path& root,const Bytes& mask,sim::World& w,std::string_view name) {
    const auto before=w.snapshot();const auto file=root/(std::string(name)+".json");
    const auto doc=save::make_document(root/"data",relative,mask,w);
    save::write_save(file,doc,root/"data",mask);
    const auto parsed=save::read_save(file);
    check(parsed.source_schema_version==19 && parsed.map_permissions_policy_version==1 &&
          !parsed.prepared_permissions && parsed.map_permissions_sha256==doc.map_permissions_sha256,
          "schema19 did not retain bounded policy identity");
    auto loaded=save::restore_save(parsed,root/"data",mask);
    check(loaded.map_permissions()->canonical_state()==w.map_permissions()->canonical_state(),"map policy reconstruction differed");
    equal_continuation(sim::World::restore(before,w.map_permissions()),std::move(loaded));
    equal_continuation(sim::World::restore(before,w.map_permissions()),save::restore_save(parsed,root/"data",mask,w.map_permissions()));
    check(w.snapshot()==before,"save/restore advanced or mutated source world");
}
void process_boundary(const fs::path& root,const Bytes& mask,const sim::World& w,unsigned layout,
                      const fs::path& executable,std::string_view name) {
    auto reference=w;
    for(int i=0;i<500;++i)reference.tick();
    const auto prefix=std::string(name);
    save::write_save(root/(prefix+"-expected.json"),save::make_document(root/"data",relative,mask,reference),root/"data",mask);
    const std::string command="\""+executable.string()+"\" --process-read-file \""+root.string()+"\" "+
        std::to_string(layout)+" "+prefix;
    check(std::system(command.c_str())==0,"fresh process post-repair gate restore failed");
    check(json_file(root/(prefix+"-process.json"))==json_file(root/(prefix+"-expected.json")),
          "fresh process post-repair continuation differs from live reference");
}
void run_layout(const fs::path& root,unsigned layout,const fs::path& executable) {
    fs::create_directories(root);write_map(root/"data",layout);const auto mask=legacy_mask(layout);
    const auto permissions=maps::read_sandbox_map_permissions(root/"data",relative,mask);
    check(permissions->gates().size()==1 && permissions->gates()[0].id.value==7,"typed GateHouse fixture did not reconstruct");
    auto w=playing(permissions,layout);const auto& gate=permissions->gates()[0];
    const auto at=[&](sim::Cell cell,sim::CourierPhase phase) {const auto& c=clay(w);return
        c.phase==phase && c.path_vertex<c.path.size() && c.path[c.path_vertex]==cell && c.edge_progress>0;};
    until(w,[&]{return at(gate.openings[0],sim::CourierPhase::ToWarehouse);});boundary(root,mask,w,"before");
    until(w,[&]{return at(gate.corridor[1],sim::CourierPhase::ToWarehouse);});boundary(root,mask,w,"inside");
    // The recovery path captures the actual partial edge/cargo/reservation state.
    openemperor::AutosaveController autosave(root/"app",root/"data",true);
    auto doc=save::make_document(root/"data",relative,mask,w);
    check(autosave.begin(doc,mask).kind==openemperor::AutosaveResult::Kind::Saved,"gate recovery start failed");
    const std::string command="\""+executable.string()+"\" --process-read \""+root.string()+"\" "+std::to_string(layout);
    check(std::system(command.c_str())==0,"fresh process gate reconstruction failed");
    auto interrupted=sim::World::restore(w.snapshot(),permissions);
    check(interrupted.execute({sim::CommandType::RemoveRoad,gate.openings[1]}).accepted,"gate exit removal failed");
    for(int i=0;i<25;++i)interrupted.tick();boundary(root,mask,interrupted,"interrupted");
    auto resumed=save::restore_save(save::read_save(root/"interrupted.json"),root/"data",mask);
    place(interrupted,sim::CommandType::PlaceRoad,gate.openings[1]);place(resumed,sim::CommandType::PlaceRoad,gate.openings[1]);
    equal_continuation(interrupted,resumed,500);
    boundary(root,mask,interrupted,"repaired");
    const auto repaired_at=[&](sim::Cell cell,sim::CourierPhase phase) {const auto& c=clay(interrupted);return
        c.phase==phase && c.path_vertex<c.path.size() && c.path[c.path_vertex]==cell && c.edge_progress>0;};
    until(interrupted,[&]{return repaired_at(gate.openings[1],sim::CourierPhase::ToWarehouse);});
    check(permissions->fixed_passage(clay(interrupted).path.front()),"post-repair fixture did not reroute from inside Gate");
    boundary(root,mask,interrupted,"repaired-exited");process_boundary(root,mask,interrupted,layout,executable,"repaired-exited");
    until(interrupted,[&]{return repaired_at(gate.corridor[1],sim::CourierPhase::Returning);});
    boundary(root,mask,interrupted,"repaired-returning");
    until(interrupted,[&]{return repaired_at(gate.openings[0],sim::CourierPhase::Returning);});
    boundary(root,mask,interrupted,"repaired-return-exited");
    process_boundary(root,mask,interrupted,layout,executable,"repaired-return-exited");
    until(w,[&]{return at(gate.openings[1],sim::CourierPhase::ToWarehouse);});boundary(root,mask,w,"after");
    until(w,[&]{return at(gate.corridor[1],sim::CourierPhase::Returning);});boundary(root,mask,w,"returning");
    while(w.ticks()<doc.world.ticks+openemperor::autosave_interval_ticks)w.tick();
    doc=save::make_document(root/"data",relative,mask,w);
    check(autosave.poll(doc,mask,true).kind==openemperor::AutosaveResult::Kind::Saved,"schema19 periodic autosave failed");
    const auto catalog=autosave.store().catalog();check(catalog.histories.size()==2,"recovery child did not preserve its parent");
    for(const auto& history:catalog.histories)for(const auto& entry:history.entries) {
        check(entry.loadable() && entry.schema==19,"schema19 recovery metadata invalid");
        const auto recovered=save::read_save(entry.path);equal_continuation(
            sim::World::restore(recovered.world,permissions),save::restore_save(recovered,root/"data",mask));
    }
    const auto path=root/"inside.json";const auto valid=json_file(path);
    auto changed=valid;changed["profiles"]["map_permissions"]["version"]=2;overwrite(path,changed);
    rejects([&]{save::read_save(path);},"policy version");
    changed=valid;changed["profiles"]["map_permissions"]["sha256"]=std::string(64,'0');overwrite(path,changed);
    rejects([&]{save::restore_save(save::read_save(path),root/"data",mask);},"map-permissions SHA-256");
    changed=valid;changed["profiles"]["map_permissions"]["links"]={{1,2}};overwrite(path,changed);
    rejects([&]{save::read_save(path);},"not transit links");
    changed=valid;changed["schema_version"]=18;overwrite(path,changed);
    rejects([&]{save::read_save(path);},"schema 18 requires");
    changed=valid;changed["map"]["sha256"]=std::string(64,'0');overwrite(path,changed);
    rejects([&]{save::restore_save(save::read_save(path),root/"data",mask);},"original map SHA-256");
    changed=valid;
    for(auto& courier:changed["world"]["couriers"]) if(courier["role"]==static_cast<unsigned>(sim::CourierRole::Clay)) {
        const auto vertex=courier["path_vertex"].get<std::size_t>();
        courier["path"][vertex]=nlohmann::json{113,113};
    }
    overwrite(path,changed);
    rejects([&]{save::restore_save(save::read_save(path),root/"data",mask);},"");
    overwrite(path,valid);const auto parsed=save::read_save(path);
    auto wrong_policy=parsed;wrong_policy.map_permissions_sha256=std::string(64,'0');
    rejects([&]{save::write_save(path,wrong_policy,root/"data",mask);},"map-permissions SHA-256");
    check(json_file(path)==valid,"rejected policy overwrote the manual save");
    const auto metadata_path=autosave.store().root()/autosave.history_id()/"history.json";
    const auto metadata_before=json_file(metadata_path);
    rejects([&]{autosave.store().write_checkpoint(autosave.history_id(),wrong_policy,mask);},"map-permissions SHA-256");
    check(json_file(metadata_path)==metadata_before,"rejected policy changed recovery metadata/rotation");
    rejects([&]{save::write_save(path,parsed,root/"data",mask,save::WriteFault::BeforeRename);},"");
    check(json_file(path)==valid,"interrupted schema19 write replaced the previous manual save");
    rejects([&]{autosave.store().write_checkpoint(autosave.history_id(),doc,mask,
        {save::WriteFault::BeforeRename,{}});},"");
    check(json_file(metadata_path)==metadata_before,"interrupted schema19 recovery write changed metadata/rotation");
    auto reordered=permissions->gates();
    std::reverse(reordered[0].protected_footprint.begin(),reordered[0].protected_footprint.end());
    std::reverse(reordered[0].corridor.begin(),reordered[0].corridor.end());
    std::swap(reordered[0].openings[0],reordered[0].openings[1]);
    const sim::MapPermissions normalized(width,width,1,permissions->cells(),reordered);
    check(save::map_permissions_fingerprint(normalized,parsed.map_sha256)==parsed.map_permissions_sha256,
          "canonical fingerprint depended on authored collection ordering");
    auto cells=permissions->cells();auto gates=permissions->gates();gates[0].id.value=8;
    auto topology=std::make_shared<const sim::MapPermissions>(width,width,1,cells,gates);
    rejects([&]{save::restore_save(parsed,root/"data",mask,topology);},"map-permissions SHA-256");
    cells[index({108,108})].height=1;
    auto height=std::make_shared<const sim::MapPermissions>(width,width,1,cells,permissions->gates());
    rejects([&]{save::restore_save(parsed,root/"data",mask,height);},"map-permissions SHA-256");
    auto wrong_mask=mask;wrong_mask[index({108,108})]^=1;
    rejects([&]{save::restore_save(parsed,root/"data",wrong_mask);},"buildable mask SHA-256");
    check(save::map_permissions_fingerprint(*permissions,std::string(64,'a'))!=parsed.map_permissions_sha256,
          "permission fingerprint omitted original map identity");
    for(const auto version:{1U,2U}) {
        sim::World legacy(width,width,mask,sim::RulesProfile::CityV16,version);
        const auto old=save::make_document(root/"data",relative,mask,legacy);const auto file=root/("legacy"+std::to_string(version)+".json");
        save::write_save(file,old,root/"data",mask);const auto j=json_file(file);const auto loaded=save::read_save(file);
        check(j["schema_version"]==18 && !j["profiles"].contains("map_permissions") &&
            loaded.map_permissions_policy_version==0 && loaded.map_permissions_sha256.empty() &&
            loaded.buildable_sha256==parsed.buildable_sha256,"legacy schema18/hash was reinterpreted");
        check(!save::restore_save(loaded,root/"data",mask).map_permissions(),"legacy save gained a new map policy");
        equal_continuation(legacy,save::restore_save(loaded,root/"data",mask));
        auto forbidden=loaded;forbidden.world.rule_version=3;
        forbidden.map_permissions_policy_version=parsed.map_permissions_policy_version;
        forbidden.map_permissions_sha256=parsed.map_permissions_sha256;
        rejects([&]{save::restore_save(forbidden,root/"data",mask,permissions);},"legacy saves are not migrated");
    }
    sim::World city10(width,width,mask,sim::RulesProfile::CityV10,1);
    save::write_save(root/"city10.json",save::make_document(root/"data",relative,mask,city10),root/"data",mask);
    const auto old10=save::read_save(root/"city10.json");check(old10.source_schema_version==10 && !old10.map_permissions_policy_version,"legacy City10 policy changed");
    equal_continuation(city10,save::restore_save(old10,root/"data",mask));
    if(layout==0) {
        auto debt=sim::World::restore(doc.world,permissions);
        for(int y=0;y<width && debt.treasury()>=2;++y)for(int x=0;x<width && debt.treasury()>=2;++x) {
            const sim::Cell cell{x,y};
            if(permissions->road_allowed(cell) && debt.object_at(cell)==sim::Object::Empty)
                place(debt,sim::CommandType::PlaceRoad,cell);
        }
        for(int ticks=0;ticks<400 && debt.treasury()>=0;++ticks)debt.tick();
        check(debt.treasury()<0,"ordinary paid roads/maintenance did not establish schema19 debt");
        boundary(root,mask,debt,"signed-debt");
    }
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc==5 && std::string_view(argv[1])=="--process-read-file") {
            const fs::path root=argv[2];const auto mask=legacy_mask(static_cast<unsigned>(std::stoul(argv[3])));
            const std::string name=argv[4];const auto doc=save::read_save(root/(name+".json"));
            auto restored=save::restore_save(doc,root/"data",mask);
            check(restored.snapshot()==doc.world,"fresh process changed repaired gate state");
            for(int i=0;i<500;++i)restored.tick();
            save::write_save(root/(name+"-process.json"),save::make_document(root/"data",relative,mask,restored),root/"data",mask);
            return 0;
        }
        if(argc==4 && std::string_view(argv[1])=="--process-read") {
            const fs::path root=argv[2];const auto mask=legacy_mask(static_cast<unsigned>(std::stoul(argv[3])));
            save::RecoveryStore store(root/"app",root/"data");const auto catalog=store.catalog();
            check(catalog.histories.size()==1 && catalog.histories[0].entries.size()==1,"new process could not read recovery start");
            const auto entry=catalog.histories[0].entries[0];const auto doc=save::read_save(entry.path);
            auto restored=save::restore_save(doc,root/"data",mask);check(restored.snapshot()==doc.world,"process restore changed partial gate edge");
            auto manual=save::restore_save(save::read_save(root/"inside.json"),root/"data",mask);
            equal_continuation(restored,manual,500);
            openemperor::AutosaveController branch(root/"app",root/"data",true);
            check(branch.begin(save::make_document(root/"data",relative,mask,restored),mask,
                save::RecoveryParent{entry.history_id,entry.sequence}).kind==openemperor::AutosaveResult::Kind::Saved,"new process recovery branch failed");
            return 0;
        }
        const auto tiny=sim::authored_simple_permissions(2,2,{1,0,1,1});
        check(save::map_permissions_fingerprint(*tiny,std::string(64,'a'))==
            "c98d688227469b04a9746e5cb78f568f40ae5f53d5c8c9920ab2368b8b369039",
            "SHA-256 canonical framing changed from independently calculated literal bytes");
        struct Grouped : std::numpunct<char> {
            char do_thousands_sep() const override{return '_';}
            std::string do_grouping() const override{return "\1";}
        };
        const auto wider=sim::authored_simple_permissions(12,2,Bytes(24,1));
        const auto wider_hash=save::map_permissions_fingerprint(*wider,std::string(64,'a'));
        const auto previous=std::locale::global(std::locale(std::locale::classic(),new Grouped));
        const auto locale_hash=save::map_permissions_fingerprint(*wider,std::string(64,'a'));
        std::locale::global(previous);
        check(locale_hash==wider_hash,
              "canonical fingerprint depended on process locale");
        const auto root=fs::canonical(fs::temp_directory_path())/("openemperor-gate-save-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup {fs::path root;~Cleanup(){std::error_code ec;fs::remove_all(root,ec);}} cleanup{root};
        for(unsigned layout=0;layout<2;++layout)run_layout(root/std::to_string(layout),layout,fs::absolute(argv[0]));
        std::cout<<"Gate passage schema19 save/recovery: both layouts, partial edges, restart, tamper and legacy passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
