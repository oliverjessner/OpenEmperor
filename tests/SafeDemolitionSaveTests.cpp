// Authored fixtures and real paid commands only. The included historical test
// main is not executed; its existing source/map/model fixture stays unchanged.
#define main oe51_unused_deadline_food_save_main
#include "DeadlineFoodSaveTests.cpp"
#undef main

#include <charconv>
#include <limits>
#include <string_view>

namespace oe51_safe_save {
namespace fs=std::filesystem;
sim::World fresh(const Fixture& fixture) {
    return sim::World::create_original_safe_demolition(fixture.policy,fixture.model,fixture.terrain);
}
sim::World restore(const Fixture& fixture,const save::SaveDocument& document) {
    return save::restore_original_safe_demolition(document,fixture.root.data,fixture.mask(),
        fixture.policy,fixture.model,fixture.terrain);
}
void remove(sim::World& world,sim::BuildingId id) {
    const auto before=world.snapshot();
    const auto result=world.execute(sim::demolish_building(id));
    check(result.accepted && result.changed,"Rule19 paid fixture demolition refused: "+result.reason);
    check(world.ticks()==before.ticks && world.treasury()==before.treasury &&
        world.construction_spent_total()==before.construction_spent_total &&
        world.next_building_id()==before.next_building_id && world.next_courier_id()==before.next_courier_id,
        "Rule19 removal changed time, Funds, spending or monotone identity highwater");
}
sim::World earned(const Fixture& fixture) {
    auto world=fresh(fixture);
    for (int x=20;x<=37;++x) f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    for (int x=20;x<=29;x+=3) buy(world,sim::CommandType::PlaceHousehold,{x,22});
    const auto clay=buy(world,sim::CommandType::PlaceClaySource,{20,25});
    buy(world,sim::CommandType::PlacePottery,{25,25});
    bool departed=false;
    for (unsigned n=0;n<1000 && !departed;++n) {
        world.tick();
        const auto actor=std::find_if(world.couriers().begin(),world.couriers().end(),
            [&](const auto& c){return c.owner==clay;});
        departed=actor!=world.couriers().end() && actor->phase==sim::CourierPhase::ToWarehouse &&
            world.building(clay).output==0 && world.building(clay).progress==0;
    }
    check(departed,"Paid Clay source did not produce and dispatch real material");
    f::buy(world,sim::set_building_operation(clay,false));
    while (world.ticks()<900) world.tick();
    check(world.building(clay).clay_extracted>0 && world.building(clay).output==0,
        "Clay removal fixture was not physically drained through ordinary delivery");
    remove(world,clay);
    const auto vacant=buy(world,sim::CommandType::PlaceHousehold,{35,22});remove(world,vacant);
    // A later Inspector has a noncontiguous live actor ID. Real visits after
    // retirement must survive complete restore without ID renumbering.
    buy(world,sim::CommandType::PlaceFireWatch,{31,25});
    while (world.ticks()<1200) world.tick();
    check(world.snapshot().original_production->safe_demolition->retired.size()==2,
        "Earned fixture lost either closed purchase");
    return world;
}
void complete_bytes_and_recovery() {
    Fixture fixture;auto world=earned(fixture);
    const auto source=fixture.root.path/"earned35.json";
    save::write_save(source,fixture.document(world),fixture.root.data,fixture.mask());
    const auto original=f::read_text(source);const auto document=save::read_save(source);
    check(document.source_schema_version==35 && document.world.rule_version==19 &&
        document.world.original_production->safe_demolition &&
        document.world.original_production->development_revision==27,
        "Rule19 requires exact schema35, mandatory journal and unchanged revision27");
    const auto json=Json::parse(original);const auto& rules=json.at("rules");
    check(rules.at("safe_demolition")==sim::original_safe_demolition_contract &&
        rules.at("deadline_food_dispatch")==sim::original_deadline_food_dispatch_contract &&
        rules.at("deadline_service_dispatch")==sim::original_deadline_service_dispatch_contract &&
        rules.at("deadline_fire_dispatch")==sim::original_deadline_fire_dispatch_contract &&
        rules.at("cardinal_water")==sim::original_cardinal_water_contract &&
        rules.at("market_logistics")==sim::original_market_logistics_contract &&
        !rules.contains("inspector_coordination"),"Schema35 changed inherited exact markers");
    auto copy=restore(fixture,document);
    const auto canonical=fixture.root.path/"roundtrip35.json";
    save::write_save(canonical,fixture.document(copy),fixture.root.data,fixture.mask());
    check(copy.snapshot()==world.snapshot() && f::read_text(canonical)==original,
        "Complete post-removal schema35 canonical bytes differ");
    for (unsigned n=0;n<103;++n) {
        world.tick();copy.tick();check(world.snapshot()==copy.snapshot(),"Post-restore continuation changed complete authority");
    }
    const auto expected=fixture.root.path/"continued35.json";
    save::write_save(expected,fixture.document(world),fixture.root.data,fixture.mask());
    save::write_save(canonical,fixture.document(copy),fixture.root.data,fixture.mask());
    check(f::read_text(expected)==f::read_text(canonical) && f::read_text(source)==original,
        "Continued complete document differs or source save changed");
    save::RecoveryStore recovery(fixture.root.path/"safe-app",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(copy),fixture.mask());
    const auto entry=recovery.write_checkpoint(history,fixture.document(copy),fixture.mask());
    check(entry.loadable() && entry.rule_version==19 && entry.schema==35 &&
        f::read_text(entry.path)==f::read_text(expected) &&
        restore(fixture,save::read_save(entry.path)).snapshot()==copy.snapshot(),
        "Recovery lost exact Rule19 journal or canonical bytes");
    const auto replacement=buy(copy,sim::CommandType::PlaceClaySource,{20,25});
    const auto old=copy.snapshot().original_production->safe_demolition->retired.front();
    check(replacement>old.building_id,"Restored retired Building ID was reused");
}
void malformed_documents_and_atomic_refusals() {
    Fixture fixture;auto world=earned(fixture);
    const auto file=fixture.root.path/"safe35.json";save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());
    const auto original=f::read_text(file);const auto json=Json::parse(original);const auto snapshot=world.snapshot();
    const auto bad=fixture.root.path/"bad35.json";
    const auto reject=[&](auto mutate) {
        auto value=json;mutate(value);f::write_text(bad,value.dump());
        refuses([&]{(void)save::read_save(bad);},"Malformed schema35 admitted");
        check(f::read_text(file)==original && world.snapshot()==snapshot,"Rejected restore mutated source save or World");
    };
    reject([](auto& j){j["rules"].erase("safe_demolition");});
    reject([](auto& j){j["rules"]["safe_demolition"]="unknown";});
    reject([](auto& j){j["rules"]["safe_demolition"]=true;});
    reject([](auto& j){j["rules"]["inspector_coordination"]=sim::original_inspector_coordination_contract;});
    for (const auto* marker:{"market_logistics","cardinal_water","deadline_fire_dispatch","deadline_service_dispatch","deadline_food_dispatch"})
        reject([&](auto& j){j["rules"].erase(marker);});
    reject([](auto& j){j["world"]["original_production"].erase("safe_demolition");});
    reject([](auto& j){j["world"]["original_production"]["safe_demolition"]["retired"][0]["kind"]=2;});
    reject([](auto& j){auto& r=j["world"]["original_production"]["safe_demolition"]["retired"];r.push_back(r[0]);});
    reject([](auto& j){j["world"]["original_production"]["safe_demolition"]["retired"][0]["removed_tick"]=0;});
    reject([](auto& j){j["world"]["original_production"]["safe_demolition"]["retired"][0]["clay_trip"]["delivered"]=0;});
    reject([](auto& j){j["world"]["original_production"]["safe_demolition"]["retired"][0]["courier_id"]=2;});
    reject([](auto& j){j["world"]["treasury"]=j["world"]["treasury"].template get<std::int64_t>()+120;});
    reject([](auto& j){j["world"]["buildings"][0]["id"]=j["world"]["original_production"]["safe_demolition"]["retired"][0]["building_id"];});
    auto duplicate=original;const std::string marker="\"safe_demolition\": \"authored-safe-demolition-v1\"";
    const auto at=duplicate.find(marker);check(at!=std::string::npos,"Canonical safe marker absent");
    duplicate.replace(at,marker.size(),marker+","+marker);f::write_text(bad,duplicate);
    refuses([&]{(void)save::read_save(bad);},"Duplicate schema35 key admitted");
    auto forged=fixture.document(world);forged.world.treasury+=120;
    refuses([&]{save::write_save(file,forged,fixture.root.data,fixture.mask());},"Fake demolition refund reached publication");
    auto edge=fixture.document(world);
    auto& retired_house=edge.world.original_production->safe_demolition->retired.back();
    check(retired_house.kind==sim::OriginalRetiredKind::EmptyHouse,"Missing retired House geometry fixture");
    retired_house.origin_x=0;retired_house.origin_y=0;
    check(fixture.policy->building_allowed({0,0}) && fixture.policy->building_allowed({1,1}),
        "Authored edge must isolate cardinal-perimeter validation from map permission");
    sim::World::validate_original_production_snapshot(edge.world);
    refuses([&]{(void)restore(fixture,edge);},"Retired House acquired an impossible source-grid cardinal perimeter");
    refuses([&]{save::write_save(file,edge,fixture.root.data,fixture.mask());},"Impossible retired House geometry reached publication");
    check(f::read_text(file)==original && world.snapshot()==snapshot,"Rejected retired geometry changed source bytes or World");
    for (const auto fault:{save::WriteFault::BeforeWrite,save::WriteFault::AfterPartialWrite,save::WriteFault::BeforeRename}) {
        refuses([&]{save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask(),fault);},
            "Injected schema35 atomic write failure was ignored");
        check(f::read_text(file)==original,"Failed schema35 publication replaced source bytes");
    }
}
void historical_generation_isolation() {
    Fixture fixture;auto old=fixture.world();const auto home=buy(old,sim::CommandType::PlaceHousehold,{20,22});
    const auto oldfile=fixture.root.path/"old34.json";save::write_save(oldfile,fixture.document(old),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(oldfile);const auto state=old.snapshot();
    check(!old.execute(sim::demolish_building(home)).accepted && old.snapshot()==state,
        "Historical Rule18 acquired House demolition");
    check(!Json::parse(bytes).at("rules").contains("safe_demolition") &&
        !Json::parse(bytes).at("world").at("original_production").contains("safe_demolition"),
        "Schema34 acquired a Rule19 marker or journal");
    auto safe=fresh(fixture);const auto doc=fixture.document(safe);
    refuses([&]{(void)fixture.restore(doc);},"Historical Rule18 restore API admitted Rule19");
    refuses([&]{(void)restore(fixture,save::read_save(oldfile));},"Rule19 restore migrated historical schema34");
    auto invalid=state;invalid.original_production->safe_demolition.emplace();
    refuses([&]{sim::World::validate_original_production_snapshot(invalid);},"Historical snapshot admitted Rule19 journal");
    check(f::read_text(oldfile)==bytes,"Historical source bytes changed");
}
void retired_fire_clock_refusals() {
    Fixture fixture;auto world=fresh(fixture);
    const auto home=buy(world,sim::CommandType::PlaceHousehold,{20,22});remove(world,home);
    while (world.ticks()<2600) world.tick();
    const auto file=fixture.root.path/"closed-house35.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());
    const auto original=f::read_text(file);const auto snapshot=world.snapshot();
    auto hostile=save::read_save(file);
    auto& retirement=hostile.world.original_production->safe_demolition->retired.front();
    check(retirement.kind==sim::OriginalRetiredKind::EmptyHouse && retirement.placed_tick==0 &&
        retirement.removed_tick==0 && hostile.world.maintenance_spent_total==0 &&
        hostile.world.taxes_collected_total==0,"Closed House fire fixture changed its real paid history");
    // Only the hostile restore input changes. A live unvisited House first
    // ignites at2000 and burns until2600; no visit can justify this retirement.
    retirement.removed_tick=2000;
    refuses([&]{sim::World::validate_original_production_snapshot(hostile.world);},
        "Retired House bypassed its removal-time natural fire clock");
    refuses([&]{(void)restore(fixture,hostile);},"Burning retired House reached Rule19 restore");
    refuses([&]{save::write_save(file,hostile,fixture.root.data,fixture.mask());},
        "Burning retired House reached atomic publication");
    retirement.removed_tick=1999;
    check(restore(fixture,hostile).snapshot()==hostile.world,
        "Last nonburning pre-ignition retirement was refused");
    retirement.removed_tick=2600;
    check(restore(fixture,hostile).snapshot()==hostile.world,
        "Exact natural fire expiry was incorrectly treated as burning");
    check(f::read_text(file)==original && world.snapshot()==snapshot,
        "Retired fire-clock checks changed protected save bytes or World");
}
void reject_retired_lifetime(const Fixture& fixture,const sim::World& world,
                            save::SaveDocument hostile,std::string_view expected) {
    const auto file=fixture.root.path/"lifetime35.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(file);const auto snapshot=world.snapshot();bool rejected=false;
    try {sim::World::validate_original_production_snapshot(hostile.world);}
    catch (const std::invalid_argument& error) {
        rejected=true;check(std::string_view(error.what()).find(expected)!=std::string_view::npos,
            "Hostile retirement failed outside its intended lifetime boundary");
    }
    check(rejected,"Impossible retired purchase lifetime passed snapshot validation");
    refuses([&]{(void)restore(fixture,hostile);},"Impossible retired purchase lifetime restored");
    refuses([&]{save::write_save(file,hostile,fixture.root.data,fixture.mask());},
        "Impossible retired purchase lifetime reached atomic publication");
    check(world.snapshot()==snapshot && f::read_text(file)==bytes,
        "Rejected retired purchase lifetime changed live authority or protected save bytes");
}
void retired_purchase_lifetime_refusals() {
    for (const bool retire_older:{false,true}) {
        Fixture fixture;auto world=fresh(fixture);
        const auto a=buy(world,sim::CommandType::PlaceHousehold,{20,22});
        const auto b=buy(world,sim::CommandType::PlaceHousehold,{25,22});
        if (retire_older) world.tick();
        remove(world,retire_older ? a:b);auto hostile=fixture.document(world);
        auto& row=hostile.world.original_production->safe_demolition->retired.front();
        row.origin_x=retire_older ? 25:20;row.origin_y=22;
        reject_retired_lifetime(fixture,world,hostile,"overlaps a live paid purchase lifetime");
    }
    {
        Fixture fixture;auto world=fresh(fixture);
        const auto a=buy(world,sim::CommandType::PlaceHousehold,{20,22});
        const auto b=buy(world,sim::CommandType::PlaceHousehold,{25,22});
        world.tick();remove(world,a);remove(world,b);auto hostile=fixture.document(world);
        auto& row=hostile.world.original_production->safe_demolition->retired.back();row.origin_x=20;
        reject_retired_lifetime(fixture,world,hostile,"retired purchase footprints overlap");
    }
    for (const bool retire_older:{false,true}) {
        Fixture fixture;auto world=fresh(fixture);
        if (!retire_older) for (int n=0;n<10;++n) world.tick();
        const auto a=buy(world,sim::CommandType::PlaceHousehold,{20,22});
        if (retire_older) remove(world,a);
        const auto b=buy(world,sim::CommandType::PlaceHousehold,{25,22});
        if (!retire_older) remove(world,b);
        if (retire_older) for (int n=0;n<10;++n) world.tick();
        auto hostile=fixture.document(world);auto& row=hostile.world.original_production->safe_demolition->retired.front();
        row.placed_tick=row.removed_tick=retire_older ? 5:0;
        reject_retired_lifetime(fixture,world,hostile,"monotone paid placement order");
    }
    {
        Fixture fixture;auto world=fresh(fixture);
        const auto neutral=buy(world,sim::CommandType::PlaceOriginalInfluenceSource,{20,22});
        const auto house=buy(world,sim::CommandType::PlaceHousehold,{25,22});
        world.tick();remove(world,neutral);remove(world,house);auto hostile=fixture.document(world);
        hostile.world.original_production->safe_demolition->retired.front().origin_x=20;
        reject_retired_lifetime(fixture,world,hostile,"closed neutral source lifetime");
    }
    {
        Fixture fixture;auto world=earned(fixture);auto hostile=fixture.document(world);
        auto& clay=hostile.world.original_production->safe_demolition->retired.front();
        check(clay.kind==sim::OriginalRetiredKind::ClaySource,"Missing real drained Clay retirement");
        clay.origin_x=20;clay.origin_y=22;
        reject_retired_lifetime(fixture,world,hostile,"overlaps a live paid purchase lifetime");
    }
}
void same_tick_rebuilds_and_later_elite_extension_remain_valid() {
    {
        Fixture fixture;auto world=fresh(fixture);const sim::Cell site{20,22};
        const auto a=buy(world,sim::CommandType::PlaceHousehold,site);remove(world,a);
        const auto b=buy(world,sim::CommandType::PlaceHousehold,site);remove(world,b);
        const auto c=buy(world,sim::CommandType::PlaceHousehold,site);
        const auto neutral=buy(world,sim::CommandType::PlaceOriginalInfluenceSource,{30,22});remove(world,neutral);
        const auto after_neutral=buy(world,sim::CommandType::PlaceHousehold,{30,22});remove(world,after_neutral);
        check(a<b && b<c && world.ticks()==0 && world.original_production_state()->safe_demolition->retired.size()==3,
            "Same-tick reuse did not use distinct actual paid purchase identities");
        const auto file=fixture.root.path/"same-tick35.json";save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());
        const auto bytes=f::read_text(file);auto copy=restore(fixture,save::read_save(file));
        const auto other=fixture.root.path/"same-tick-copy35.json";save::write_save(other,fixture.document(copy),fixture.root.data,fixture.mask());
        check(copy.snapshot()==world.snapshot() && f::read_text(other)==bytes,
            "Legitimate same-tick House/neutral-source reuse lost complete canonical bytes");
    }
    {
        Fixture fixture;auto world=fresh(fixture);
        const auto elite=buy(world,sim::CommandType::PlaceOriginalElitePlot,{20,20});
        const auto spare=buy(world,sim::CommandType::PlaceHousehold,{22,20});
        for (int x=20;x<24;++x) f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
        while (world.ticks()<119) world.tick();
        check(world.building(elite).kind==sim::Object::OriginalElitePlot && world.household_population(spare)==0,
            "Later-extension fixture did not retain the real blocked2x2 plot and never-admitted spare");
        remove(world,spare);world.tick();
        check(world.ticks()==120 && world.building(elite).kind==sim::Object::OriginalEliteHouse &&
            world.original_production_state()->housing.houses.front().first_admission_tick==120,
            "Removing the real spare before120 did not permit actual atomic4x4 materialization");
        const auto document=fixture.document(world);
        check(restore(fixture,document).snapshot()==world.snapshot(),
            "Current4x4 footprint falsely rejected an earlier legitimate retired2x2 on its future extension");
        auto hostile=document;hostile.world.original_production->safe_demolition->retired.front().removed_tick=120;
        reject_retired_lifetime(fixture,world,hostile,"already materialized Elite extension");
        // Full current-body bounds are checked by subtraction before the
        // retirement helper can add any Elite extension-cell offsets.
        for (const int extreme:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
            for (const bool x_axis:{false,true}) {
                auto invalid_origin=document;
                auto& body=invalid_origin.world.buildings.front();
                check(body.id==elite && body.kind==sim::Object::OriginalEliteHouse,
                    "Extreme-origin control lacks the real materialized Elite");
                if (x_axis) body.cell.x=extreme;else body.cell.y=extreme;
                reject_retired_lifetime(fixture,world,invalid_origin,"building footprint outside saved grid");
            }
    }
}
void retired_purchase_and_infrastructure_lifetimes() {
    Fixture fixture;auto cells=fixture.policy->cells();
    for (int x=21;x<=28;++x) {
        auto& cell=cells[static_cast<std::size_t>(20*228+x)];
        cell.road_allowed=cell.building_allowed=false;cell.bridge_water=true;
        cell.road_blocker=cell.building_blocker=sim::BuildBlocker::UnsupportedBridgeWater;
    }
    cells[20*228+41].height=1;
    fixture.policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(cells),
        std::vector<sim::FixedGatePassage>{});
    auto world=fresh(fixture);
    // Both actual empty Houses precede the independently typed links. The
    // first2x2 contains bank20,20 without touching any water; the second
    // contains Ramp endpoint40,20 and stays on its original level.
    const auto bank_house=buy(world,sim::CommandType::PlaceHousehold,{19,19});remove(world,bank_house);
    const auto ramp_house=buy(world,sim::CommandType::PlaceHousehold,{39,19});remove(world,ramp_house);
    for (const auto cell:{sim::Cell{20,20},sim::Cell{29,20},sim::Cell{40,20},sim::Cell{41,20}})
        f::buy(world,{sim::CommandType::PlaceRoad,cell});
    f::buy(world,sim::place_bridge({20,20},{29,20}));f::buy(world,sim::place_ramp({40,20},{41,20}));world.tick();
    check(world.bridges().size()==1 && world.ramps().size()==1 && world.bridges().front().placed_tick==0 &&
        world.ramps().front().placed_tick==0 && world.ticks()==1,
        "Infrastructure lifetime fixture lacks actual paid same-tick links after retirement");
    const auto document=fixture.document(world);
    check(restore(fixture,document).snapshot()==world.snapshot(),
        "Legal same-tick retirement before independent Bridge/Ramp purchase was refused");
    for (std::size_t index=0;index<2;++index) {
        auto hostile=document;hostile.world.original_production->safe_demolition->retired[index].removed_tick=1;
        reject_retired_lifetime(fixture,world,hostile,index ? "permanent Ramp lifetime":"permanent Bridge lifetime");
    }
}
fs::path private_path(const char* text,bool input) {
    const auto path=fs::absolute(text).lexically_normal();
    const auto root=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/oe51";
    const auto relative=path.lexically_relative(root);
    check(!relative.empty() && *relative.begin()!=".." && path!=root,"Use an explicit private OE51 proof path");
    for (auto part=path;!part.empty() && part!=part.root_path();part=part.parent_path())
        check(!fs::is_symlink(part),"Private proof path contains symlink");
    check(input ? fs::is_regular_file(path)&&fs::file_size(path)<=8U*1024U*1024U:!fs::exists(path),
        "Preserve existing private proof inputs/outputs");
    return path;
}
int process(int argc,char** argv) {
    Fixture fixture;
    if (argc==3 && std::string_view(argv[1])=="--create") {
        const auto output=private_path(argv[2],false);fs::create_directories(output.parent_path());auto world=earned(fixture);
        save::write_save(output,fixture.document(world),fixture.root.data,fixture.mask());return 0;
    }
    check((argc==4 || argc==5) && std::string_view(argv[1])=="--restore","Use --create OUTPUT or --restore INPUT OUTPUT [TICKS]");
    const auto input=private_path(argv[2],true),output=private_path(argv[3],false);const auto bytes=f::read_text(input);
    auto world=restore(fixture,save::read_save(input));unsigned ticks=0;
    if (argc==5) {
        const std::string_view value=argv[4];const auto parsed=std::from_chars(value.data(),value.data()+value.size(),ticks);
        check(parsed.ec==std::errc{} && parsed.ptr==value.data()+value.size() && ticks<=20400,"Bounded continuation ticks required");
    }
    for (unsigned n=0;n<ticks;++n) world.tick();
    fs::create_directories(output.parent_path());save::write_save(output,fixture.document(world),fixture.root.data,fixture.mask());
    check(f::read_text(input)==bytes,"Separate-process restore changed protected input");
    if (!ticks) check(f::read_text(output)==bytes,"Separate-process complete canonical bytes differ");
    return 0;
}
} // namespace oe51_safe_save
int main(int argc,char** argv) {
    try {
        if (argc>1) return oe51_safe_save::process(argc,argv);
        oe51_safe_save::complete_bytes_and_recovery();
        oe51_safe_save::malformed_documents_and_atomic_refusals();
        oe51_safe_save::historical_generation_isolation();
        oe51_safe_save::retired_fire_clock_refusals();
        oe51_safe_save::retired_purchase_lifetime_refusals();
        oe51_safe_save::same_tick_rebuilds_and_later_elite_extension_remain_valid();
        oe51_safe_save::retired_purchase_and_infrastructure_lifetimes();
        std::cout<<"PASS Rule19/schema35 complete retired-purchase saves, Recovery, refusals and exact historical generations\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<"Safe demolition save FAIL: "<<error.what()<<'\n';return 1;}
}
