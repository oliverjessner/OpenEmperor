// Authored map/model integration, earned solely with normal paid commands and
// World ticks. These checks are not original-data progression or human evidence.
#include "OriginalFinanceFixture.h"
#include "persistence/RecoveryStore.h"
#include "maps/EmperorMap.h"
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <iostream>

namespace {
namespace f=oe_influence_fixture;
namespace q=oe_finance_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace maps=openemperor::maps;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
using f::check;
template<class F> void refuses(F action,const char* reason) {
    try {action();} catch(const std::exception&) {return;}
    throw std::runtime_error(reason);
}
void u32(Bytes& data,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)data.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
Bytes encoded_map(const std::vector<std::uint32_t>& terrain) {
    Bytes raw(static_cast<std::size_t>(maps::objects_logical_offset+maps::grid_byte_length));
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),raw.begin());u32(raw,84,226);
    for(std::size_t i=0;i<terrain.size();++i)
        u32(raw,static_cast<std::size_t>(maps::terrain_logical_offset)+4*i,terrain[i]);
    Bytes bytes(4);u32(bytes,0,0xfedcbaaaU);
    for(std::size_t at=0;at<raw.size();at+=32768) {
        const auto size=std::min<std::size_t>(32768,raw.size()-at);
        uLongf length=compressBound(static_cast<uLong>(size));Bytes block(length);
        check(compress2(block.data(),&length,raw.data()+at,static_cast<uLong>(size),6)==Z_OK,
              "Authored map compression failed");
        const auto header=bytes.size();bytes.resize(header+12);u32(bytes,header,0x12345678U);
        u32(bytes,header+4,static_cast<std::uint32_t>(length));u32(bytes,header+8,static_cast<std::uint32_t>(size));
        bytes.insert(bytes.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(length));
    }
    return bytes;
}
struct Fixture {
    f::Root root;
    std::vector<std::uint32_t> raw=std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count);
    std::shared_ptr<const sim::MapPermissions> policy;
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    Fixture() {
        raw[22*228+19]=4;raw[22*228+46]=4;
        std::vector<sim::MapCellPermission> cells(raw.size(),
            {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
        for(const auto i:{22*228+19,22*228+46})
            cells[static_cast<std::size_t>(i)]={false,false,false,0,sim::BuildBlocker::LegacyBuildabilityRestriction,
                sim::BuildBlocker::LegacyBuildabilityRestriction};
        policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",q::authored_model());
        model=save::prepare_original_production_rules_model(root.data);
        write_map(raw);terrain=save::prepare_original_terrain_facts(root.data,f::map_relative);
    }
    void write_map(const std::vector<std::uint32_t>& values) const {
        const auto bytes=encoded_map(values);
        f::write_text(root.data/f::map_relative,std::string(bytes.begin(),bytes.end()));
    }
    std::vector<std::uint8_t> mask() const {return policy->building_mask();}
    sim::World world() const {return sim::World::create_original_deadline_fire_dispatch(policy,model,terrain);}
    save::SaveDocument document(const sim::World& w) const {
        return save::make_document(root.data,f::map_relative,mask(),w);
    }
    sim::World restore(const save::SaveDocument& d) const {
        return save::restore_original_deadline_fire_dispatch(d,root.data,mask(),policy,model,terrain);
    }
};
sim::BuildingId buy(sim::World& w,sim::CommandType command,sim::Cell cell) {
    f::buy(w,{command,cell});return *w.building_owner_at(cell);
}
void accounting(const sim::World& w) {
    std::uint64_t upkeep=0,tax=0;
    for(const auto& b:w.buildings()) {
        upkeep+=(w.ticks()-b.placed_tick)/816U*2U*static_cast<std::uint64_t>(w.workforce_required(b.id));
        tax+=b.taxes_paid_total;
    }
    check(upkeep==w.maintenance_spent_total()&&tax==w.taxes_collected_total()&&
        w.treasury()==12000+static_cast<std::int64_t>(tax)-static_cast<std::int64_t>(upkeep)-
            static_cast<std::int64_t>(w.construction_spent_total()),
        "Rule16 changed installed upkeep or complete actual Funds accounting");
}
void build(sim::World& w) {
    for(int x=20;x<=51;++x)f::buy(w,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<44;x+=3)buy(w,sim::CommandType::PlaceHousehold,{x,22});
    const auto farm=buy(w,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{23,25}};field.building_id=farm;f::buy(w,field);
    buy(w,sim::CommandType::PlaceMarket,{25,25});
    buy(w,sim::CommandType::PlaceOriginalWaterWell,{29,25});
    buy(w,sim::CommandType::PlaceOriginalAncestorShrine,{33,25});
    buy(w,sim::CommandType::PlaceFireWatch,{40,25});buy(w,sim::CommandType::PlaceFireWatch,{43,25});
    buy(w,sim::CommandType::PlaceOriginalElitePlot,{47,20});
    check(w.total_population()==0&&w.taxes_collected_total()==0,"Paid fixture granted residents or taxes");
}
void identity_and_marker_isolation() {
    Fixture fixture;auto w=fixture.world();
    check(w.rule_version()==16&&w.active_rules().version==16&&w.original_deadline_fire_dispatch_enabled()&&!w.original_inspector_coordination_enabled()&&
        w.original_cardinal_water_enabled()&&w.original_market_selection_enabled()&&w.original_sustainable_economy_enabled()&&
        w.original_fire_protection_enabled()&&w.original_worker_upkeep_interval()==816&&
        w.original_production_state()->development_revision==27,"Rule16 did not retain its exact inherited contracts");
    refuses([&]{(void)sim::World(fixture.policy,sim::RulesProfile::CityV16,16);},"Ordinary constructor opted into Rule16");
    refuses([&]{(void)sim::World::create_original_deadline_fire_dispatch(fixture.policy,fixture.model,nullptr);},
            "Rule16 factory accepted missing immutable terrain");
    const auto original=w.snapshot();
    const auto command=w.execute({sim::CommandType::PlaceHousehold,{-1,20}});
    check(!command.accepted&&!command.changed&&w.snapshot()==original,"Refused Rule16 command changed state/IDs/Funds");
    const auto file=fixture.root.path/"empty32.json";save::write_save(file,fixture.document(w),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(file);const auto json=Json::parse(bytes);const auto parsed=save::read_save(file);
    check(parsed.source_schema_version==32&&parsed.world==original&&!parsed.prepared_original_terrain_facts&&
        json.at("rules").at("deadline_fire_dispatch")==sim::original_deadline_fire_dispatch_contract&&
        json.at("rules").at("cardinal_water")==sim::original_cardinal_water_contract&&
        json.at("rules").at("market_logistics")==sim::original_market_logistics_contract,
        "Schema32 lost an exact marker or acquired JSON terrain authority");
    check(!json.at("rules").contains("inspector_coordination") &&
        !json.at("world").contains("deadline_fire_dispatch"),
        "Rule16 acquired the historical marker or new saved dispatch authority");
    const auto bad=fixture.root.path/"hostile32.json";
    const auto rejects_json=[&](auto edit) {
        auto value=json;edit(value);f::write_text(bad,value.dump());
        refuses([&]{(void)save::read_save(bad);},"Malformed Schema32 JSON accepted");
        check(f::read_text(file)==bytes&&w.snapshot()==original,"Rejected JSON changed prior bytes or World");
    };
    rejects_json([](auto& j){j["rules"].erase("deadline_fire_dispatch");});
    rejects_json([](auto& j){j["rules"]["deadline_fire_dispatch"]="unknown";});
    rejects_json([](auto& j){j["rules"]["deadline_fire_dispatch"]=true;});
    rejects_json([](auto& j){j["rules"].erase("market_logistics");});
    rejects_json([](auto& j){j["rules"].erase("cardinal_water");});
    rejects_json([](auto& j){j["schema_version"]=31;});
    rejects_json([](auto& j){j["rules"]["version"]=15;});
    rejects_json([](auto& j){j["rules"]["inspector_coordination"]=sim::original_inspector_coordination_contract;});
    rejects_json([](auto& j){j["schema_version"]=30;});
    rejects_json([](auto& j){j["rules"]["version"]=14;});
    rejects_json([](auto& j){j["rules"]["unknown"]=1;});
    rejects_json([](auto& j){j["world"]["deadline_fire_dispatch"]={{"reserved_target",1}};});
    rejects_json([](auto& j){j["profiles"]["terrain_flags"]=Json::array({4});});
    auto duplicated=bytes;const auto at=duplicated.find("\"deadline_fire_dispatch\":");
    check(at!=std::string::npos,"Missing canonical Schema32 marker");
    duplicated.insert(at,"\"deadline_fire_dispatch\":\"authored-deadline-fire-dispatch-v1\",");
    f::write_text(bad,duplicated);refuses([&]{(void)save::read_save(bad);},"Duplicate identical Schema32 marker accepted");
    auto nested=Json::array();for(int depth=0;depth<34;++depth)nested=Json::array({nested});
    auto deep=json;deep["untrusted"]=std::move(nested);f::write_text(bad,deep.dump());
    refuses([&]{(void)save::read_save(bad);},"Schema32 raised the depth32 bound");
    f::write_text(bad,std::string(8U*1024U*1024U+1U,' '));
    refuses([&]{(void)save::read_save(bad);},"Schema32 raised the8MiB bound");
    refuses([&]{(void)save::restore_save(parsed,fixture.root.data,fixture.mask(),fixture.policy);},"Normal restore silently enabled Rule16");
    refuses([&]{(void)save::restore_original_production_development(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model);},"Rule10 restore accepted31");
    refuses([&]{(void)save::restore_original_fire_protection(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model);},"Rule11 restore accepted31");
    refuses([&]{(void)save::restore_original_sustainable_economy(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model);},"Rule12 restore accepted31");
    refuses([&]{(void)save::restore_original_market_logistics(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model);},"Rule13 restore accepted31");
    refuses([&]{(void)save::restore_original_cardinal_water(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},"Rule14 restore accepted31");
    refuses([&]{(void)save::restore_original_inspector_coordination(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},"Rule15 restore accepted32");
    auto previous=sim::World::create_original_inspector_coordination(fixture.policy,fixture.model,fixture.terrain);
    const auto previous_file=fixture.root.path/"historical31.json";
    save::write_save(previous_file,fixture.document(previous),fixture.root.data,fixture.mask());
    const auto previous_bytes=f::read_text(previous_file);const auto previous_doc=save::read_save(previous_file);
    refuses([&]{(void)fixture.restore(previous_doc);},"Schema31 implicitly migrated to32");
    refuses([&]{(void)sim::World::restore_original_deadline_fire_dispatch(previous.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "World Rule15 implicitly migrated to16");
    refuses([&]{(void)sim::World::restore_original_inspector_coordination(w.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "World Rule15 restore accepted16");
    auto previous_copy=save::restore_original_inspector_coordination(previous_doc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);
    save::write_save(previous_file,fixture.document(previous_copy),fixture.root.data,fixture.mask());
    check(f::read_text(previous_file)==previous_bytes,"Historical Rule15 canonical bytes changed");
    auto old=sim::World::create_original_cardinal_water(fixture.policy,fixture.model,fixture.terrain);
    const auto oldfile=fixture.root.path/"historical30.json";save::write_save(oldfile,fixture.document(old),fixture.root.data,fixture.mask());
    const auto oldbytes=f::read_text(oldfile);auto olddoc=save::read_save(oldfile);
    refuses([&]{(void)fixture.restore(olddoc);},"Schema30 implicitly migrated to31");
    auto relabel=Json::parse(oldbytes);relabel["schema_version"]=32;relabel["rules"]["version"]=16;
    f::write_text(bad,relabel.dump());refuses([&]{(void)save::read_save(bad);},"Header-only Rule14 relabel bypassed the new contract marker");
    auto unknown=Json::parse(oldbytes);unknown["rules"]["deadline_fire_dispatch"]=sim::original_deadline_fire_dispatch_contract;
    f::write_text(bad,unknown.dump());refuses([&]{(void)save::read_save(bad);},"Schema30 ignored a new-generation marker");
    auto restored=save::restore_original_cardinal_water(olddoc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);
    save::write_save(oldfile,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(oldfile)==oldbytes,"Historical Rule14 canonical bytes changed");
    sim::World legacy(228,228,fixture.mask());const auto legacyfile=fixture.root.path/"legacy4.json";
    save::write_save(legacyfile,fixture.document(legacy),fixture.root.data,fixture.mask());
    auto legacy_json=Json::parse(f::read_text(legacyfile));
    check(legacy_json.at("schema_version")==4,"Legacy marker probe has the wrong historical generation");
    legacy_json["rules"]["deadline_fire_dispatch"]=sim::original_deadline_fire_dispatch_contract;
    f::write_text(bad,legacy_json.dump());
    refuses([&]{(void)save::read_save(bad);},"Historical noncanonical parser ignored the new reserved marker");
}
void paid_authority_roundtrip_recovery_and_hostiles() {
    Fixture fixture;auto w=fixture.world();build(w);
    const auto empty_paid=w.snapshot();
    const auto third=w.execute({sim::CommandType::PlaceFireWatch,{60,25}});
    check(!third.accepted&&!third.changed&&w.snapshot()==empty_paid,"Third Watch refusal changed paid state or limits");
    sim::Command bad_owner{sim::CommandType::PlaceOriginalHempField,{62,25}};
    bad_owner.building_id=*w.building_owner_at({40,25});const auto field=w.execute(bad_owner);
    check(!field.accepted&&!field.changed&&w.snapshot()==empty_paid,"Wrong field owner acquired paid authority");
    for(int i=0;i<1800;++i)w.tick();
    accounting(w);
    check(w.taxes_collected_total()>0&&w.maintenance_spent_total()>0&&
        !w.original_production_state()->fire_protection->arrivals.empty(),"Paid Rule16 fixture earned no taxes/upkeep/Inspector arrivals");
    const auto file=fixture.root.path/"paid32.json";const auto document=fixture.document(w);
    save::write_save(file,document,fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);const auto snapshot=w.snapshot();
    const auto parsed=save::read_save(file);auto copy=fixture.restore(parsed);
    check(copy.snapshot()==snapshot&&*copy.original_terrain_facts()==*fixture.terrain,"Full Schema32 restore changed authority/context");
    save::write_save(file,fixture.document(copy),fixture.root.data,fixture.mask());
    check(f::read_text(file)==bytes,"Complete Schema32 canonical SaveDocument changed");
    const auto rejects=[&](auto edit) {
        auto bad=parsed;edit(bad);refuses([&]{(void)fixture.restore(bad);},"Hostile Rule16 restore accepted");
        refuses([&]{save::write_save(file,bad,fixture.root.data,fixture.mask());},"Hostile Rule16 reached atomic publication");
        check(w.snapshot()==snapshot&&f::read_text(file)==bytes,"Rejected restore/publication changed prior World/save");
    };
    rejects([](auto& d){d.source_schema_version=31;});
    rejects([](auto& d){d.world.rule_version=15;});
    rejects([](auto& d){d.source_schema_version=30;});
    rejects([](auto& d){d.world.rule_version=14;});
    rejects([](auto& d){++d.world.original_production->development_revision;});
    rejects([](auto& d){d.map_sha256=std::string(64,'0');});
    rejects([](auto& d){d.buildable_sha256=std::string(64,'0');});
    rejects([](auto& d){d.map_permissions_policy_version=2;});
    rejects([](auto& d){d.map_permissions_sha256=std::string(64,'0');});
    rejects([](auto& d){d.world.original_economy->model_source->sha256=std::string(64,'0');});
    rejects([](auto& d){++d.world.original_economy->model_source->byte_count;});
    rejects([](auto& d){d.world.original_economy->model_source->difficulty=static_cast<orig::Difficulty>(0);});
    rejects([](auto& d){++d.world.treasury;});
    rejects([](auto& d){--d.world.construction_spent_total;++d.world.treasury;});
    rejects([](auto& d){d.world.couriers.back().role=sim::CourierRole::Clay;});
    rejects([](auto& d){d.world.couriers.back().path={{-1,-1}};});
    rejects([](auto& d){d.world.original_goods->totals[0].imported=1;});
    rejects([](auto& d){d.world.original_production->fire_protection->arrivals.front().trip.owner=static_cast<sim::BuildingId>(999);});
    rejects([](auto& d){d.world.original_production->fire_protection->arrivals.front().trip.assigned_workers=0;});
    rejects([](auto& d){d.world.original_production->fire_protection->arrivals.front().trip.travelled.back()={-1,-1};});
    rejects([](auto& d){d.world.original_production->fire_protection->arrivals.clear();});
    rejects([](auto& d){++d.world.original_production->fire_protection->travelled_cells;});
    rejects([](auto& d){d.world.original_production->fire_protection->taxes.clear();});
    auto forged_raw=fixture.raw;forged_raw[22*228+19]=0;
    const auto forged=std::make_shared<const sim::OriginalTerrainFacts>(forged_raw,fixture.terrain->source_map_sha256());
    refuses([&]{(void)save::restore_original_deadline_fire_dispatch(parsed,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,forged);},
            "Prepared same-SHA terrain forgery bypassed live raw rederivation");
    auto bad_document=document;bad_document.prepared_original_terrain_facts=forged;
    refuses([&]{save::write_save(file,bad_document,fixture.root.data,fixture.mask());},"Forged prepared document reached publication");
    for(const auto fault:{save::WriteFault::BeforeWrite,save::WriteFault::AfterPartialWrite,save::WriteFault::BeforeRename}) {
        refuses([&]{save::write_save(file,document,fixture.root.data,fixture.mask(),fault);},"Injected atomic write fault was ignored");
        check(f::read_text(file)==bytes,"Atomic write fault overwrote the good Schema32 file");
    }
    const auto source=fixture.root.data/"Model/EmperorBuildingModels.txt";const auto source_bytes=f::read_text(source);
    f::write_text(source,source_bytes+"\n");
    refuses([&]{(void)fixture.restore(parsed);},"Prepared model accepted changed live source suffix");
    refuses([&]{save::write_save(file,document,fixture.root.data,fixture.mask());},"Changed model suffix reached publication");
    refuses([&]{(void)fixture.document(w);},"Document factory ignored changed live model bytes");
    check(f::read_text(file)==bytes,"Failed source-bound publication changed prior save");f::write_text(source,source_bytes);
    const auto map=fixture.root.data/f::map_relative;const auto map_bytes=f::read_text(map);
    fixture.write_map(forged_raw);
    refuses([&]{(void)fixture.restore(parsed);},"Prepared context bypassed changed full raw map SHA");
    refuses([&]{save::write_save(file,document,fixture.root.data,fixture.mask());},"Changed raw map reached publication");
    refuses([&]{(void)fixture.document(w);},"Document factory accepted changed raw map bytes");
    check(f::read_text(file)==bytes,"Changed source map overwrote good save");f::write_text(map,map_bytes);
    save::RecoveryStore legacy(fixture.root.path/"legacy-app",fixture.root.data);
    refuses([&]{(void)legacy.create_history(document,fixture.mask());},"LegacyOnly Recovery opted into Schema32");
    save::RecoveryStore recovery(fixture.root.path/"deadline-app",fixture.root.data,save::recovery_storage_budget_bytes,
        save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(document,fixture.mask());
    for(int i=0;i<250;++i){w.tick();copy.tick();check(w.snapshot()==copy.snapshot(),"Full Rule16 continuation diverged");}
    const auto checkpoint=recovery.write_checkpoint(history,fixture.document(w),fixture.mask());
    check(checkpoint.loadable()&&checkpoint.rule_version==16&&checkpoint.schema==32,"Recovery lost exact Rule16/Schema32");
    auto recovered=fixture.restore(save::read_save(checkpoint.path));
    const auto expected=fixture.root.path/"expected-recovery.json";save::write_save(expected,fixture.document(w),fixture.root.data,fixture.mask());
    check(f::read_text(expected)==f::read_text(checkpoint.path)&&recovered.snapshot()==w.snapshot(),"Recovery lost complete canonical bytes/World");
    const auto metadata=recovery.root()/history/"history.json";const auto metadata_bytes=f::read_text(metadata);
    const auto historical=sim::World::create_original_cardinal_water(fixture.policy,fixture.model,fixture.terrain);
    refuses([&]{(void)recovery.write_checkpoint(history,fixture.document(historical),fixture.mask());},
            "Recovery appended historical Rule14 to a Rule16 history");
    check(f::read_text(metadata)==metadata_bytes&&f::read_text(checkpoint.path)==f::read_text(expected),
        "Rejected mixed-generation Recovery changed metadata or saved authority");
    const auto catalog=recovery.catalog();check(catalog.histories.size()==1&&catalog.histories.front().error.empty(),"Exact Schema32 recovery catalog failed");
    for(int i=0;i<120;++i){w.tick();recovered.tick();check(w.snapshot()==recovered.snapshot(),"Recovered Rule16 continuation diverged");}
    accounting(w);
}
void legitimate_duplicate_after_road_repair() {
    Fixture fixture;auto w=fixture.world();
    for(int x=10;x<=22;++x)f::buy(w,{sim::CommandType::PlaceRoad,{x,12}});
    const auto house=buy(w,sim::CommandType::PlaceHousehold,{12,10});
    while(w.ticks()<240)w.tick();
    const auto first=buy(w,sim::CommandType::PlaceFireWatch,{10,11});
    const auto second=buy(w,sim::CommandType::PlaceFireWatch,{18,11});
    f::buy(w,sim::set_building_operation(second,false));w.tick();
    const auto inspector=[&](sim::BuildingId owner) {
        for(const auto& c:w.couriers())if(c.owner==owner&&c.role==sim::CourierRole::FireInspector)return c.id;
        throw std::runtime_error("Paid Watch has no actual Inspector");
    };
    const auto a=inspector(first),b=inspector(second);
    check(w.courier(a).target==house&&w.courier(a).edge_progress==1,"Repair fixture lacks real begun incoming Inspector");
    f::buy(w,{sim::CommandType::RemoveRoad,{11,12}});
    for(int i=0;i<4;++i)w.tick();
    check(w.courier(a).route_pending&&!w.courier(a).edge_progress,"Cut incoming Inspector did not actually wait");
    f::buy(w,sim::set_building_operation(second,true));w.tick();
    check(w.courier(b).target==house&&w.courier(b).phase==sim::CourierPhase::ToWarehouse,
        "Waiting actor incorrectly forbade a real replacement Inspector");
    f::buy(w,{sim::CommandType::PlaceRoad,{11,12}});w.tick();
    check(w.courier(a).target==house&&w.courier(b).target==house&&
        w.courier(a).phase==sim::CourierPhase::ToWarehouse&&w.courier(b).phase==sim::CourierPhase::ToWarehouse&&
        !w.courier(a).route_pending&&!w.courier(b).route_pending,
        "Ordinary road repair failed to produce two legitimate active incoming trips");
    const auto snapshot=w.snapshot();const auto file=fixture.root.path/"duplicate-repaired32.json";
    save::write_save(file,fixture.document(w),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    auto restored=fixture.restore(save::read_save(file));check(restored.snapshot()==snapshot,"Legitimate duplicate targets were rejected or rewritten");
    save::write_save(file,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(file)==bytes,"Duplicate-repair canonical bytes changed");
    save::RecoveryStore recovery(fixture.root.path/"repair-app",fixture.root.data,save::recovery_storage_budget_bytes,
        save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(w),fixture.mask());
    const auto entry=recovery.write_checkpoint(history,fixture.document(w),fixture.mask());
    check(f::read_text(entry.path)==bytes&&fixture.restore(save::read_save(entry.path)).snapshot()==snapshot,
        "Recovery imposed a new global duplicate-target prohibition");
    for(int i=0;i<100;++i){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Duplicate-repair continuation diverged");}
}
}
int main() {
    try {
        identity_and_marker_isolation();paid_authority_roundtrip_recovery_and_hostiles();legitimate_duplicate_after_road_repair();
        std::cout<<"PASS: authored paid Rule16, strict Schema32/live source/atomic Recovery, legitimate repaired duplicates; no original-data retention claim\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"deadline fire dispatch save FAIL: "<<error.what()<<'\n';return 1;}
}
