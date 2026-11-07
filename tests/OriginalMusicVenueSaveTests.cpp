#include "OriginalMusicVenueFixture.h"
#include "persistence/RecoveryStore.h"

#include <charconv>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace q=oe_music_venue_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using q::check;

sim::World earned(const q::Fixture& fixture) {
    auto world=fixture.world();const auto s=q::scene(world);
    q::until(world,[&]{return q::active(world,s.source)&&world.courier(s.channel).edge_progress>0;},
        "Separate-process fixture did not earn a begun physical performer edge");
    f::buy(world,sim::set_building_operation(s.source,false));return world;
}
void checked_model_capability() {
    q::Fixture fixture;
    check(fixture.model->supports_building(orig::ModelBuilding::TheatrePavilion)&&
          fixture.model->building_value(orig::ModelBuilding::TheatrePavilion,orig::BuildingField::Cost)==105&&
          fixture.model->building_value(orig::ModelBuilding::TheatrePavilion,orig::BuildingField::Employees)==7,
          "New culture preparation lost the actual typed Theatre row");
    const auto old=save::prepare_original_production_rules_model(fixture.root.data);
    check(!old->supports_building(orig::ModelBuilding::TheatrePavilion),"Old production preparation silently gained75 capability");
    for(int variant=0;variant<3;++variant) {
        f::Root root;auto text=q::authored_model();const std::string marker="75,Theatre Pavilion,{,105,";
        const auto at=text.find(marker);check(at!=std::string::npos,"Authored model capability row missing");
        if(variant==0)text.erase(at,text.find('\n',at)-at+1);
        else if(variant==1)text.replace(at,marker.size(),"75,Wrong Theatre Label,{,105,");
        else {
            text.replace(at,marker.size(),"75,Theatre Pavilion,{,2147483647,");
            const std::string modifier="2,Normal Modifier,{,100,";const auto mod=text.find(modifier);
            check(mod!=std::string::npos,"Authored checked-overflow modifier missing");
            text.replace(mod,modifier.size(),"2,Normal Modifier,{,200,");
        }
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",text);
        check(!save::prepare_original_production_rules_model(root.data)->supports_building(orig::ModelBuilding::TheatrePavilion),
              "An unused75 field changed historical production preparation");
        q::refuses([&]{(void)save::prepare_original_culture_rules_model(root.data);},
            "Missing/wrong-label/checked-overflow Theatre row entered new capability");
    }
}
void canonical_active_arrived_expired_and_recovery() {
    q::Fixture fixture;auto world=earned(fixture);const auto source=q::state(world).schools.front().building_id;
    const auto target=q::state(world).venues.front().building_id;
    const auto canonical=fixture.root.path/"canonical38.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,
        save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    for(int phase=0;phase<3;++phase) {
        if(phase==1)q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},
            "Save checkpoint did not reach actual one-way arrival");
        if(phase==2)q::until(world,[&]{return q::venue(world,target).music_timer==0;},
            "Save checkpoint did not reach earned Venue expiry",2000);
        save::write_save(canonical,fixture.document(world),fixture.root.data,fixture.mask());
        const auto bytes=f::read_text(canonical);const auto snapshot=world.snapshot();
        const auto parsed=save::read_save(canonical);const auto json=Json::parse(bytes);
        check(parsed.source_schema_version==38&&parsed.world.rule_version==22&&parsed.world==snapshot&&
              json.at("rules").at("music_venues")==sim::original_music_venues_contract&&
              json.at("world").at("original_production").contains("culture")&&!parsed.migrated_from_schema1,
              "Schema38 lost exact marker, complete authority or explicit generation");
        auto restored=fixture.restore(parsed);check(restored.snapshot()==snapshot,"Independent active/retired performer restore differs");
        save::write_save(canonical,fixture.document(restored),fixture.root.data,fixture.mask());
        check(f::read_text(canonical)==bytes,"Complete canonical Schema38 bytes changed on rewrite");
        const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==38&&entry.rule_version==22&&f::read_text(entry.path)==bytes&&
              fixture.restore(save::read_save(entry.path)).snapshot()==snapshot,
              "Recovery lost literal complete Schema38 performer/timer authority");
        // Independent in-process continuation also compares every complete
        // snapshot, rather than only the visible timer or a chosen digest.
        auto continued=fixture.restore(parsed);auto expected=world;
        for(int tick=0;tick<103;++tick){continued.tick();expected.tick();check(continued.snapshot()==expected.snapshot(),
            "Complete103-tick active/timer continuation diverged");}
        q::invariants(continued);
    }
}
void hostile_world_and_atomic_publication() {
    q::Fixture fixture;auto world=earned(fixture);const auto baseline=world.snapshot();
    const auto file=fixture.root.path/"preserved38.json";save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(file);const auto original=save::read_save(file);
    const auto reject=[&](auto mutate) {
        auto hostile=original;mutate(hostile.world);
        q::refuses([&]{(void)fixture.restore(hostile);},"Hostile performer/venue authority reached restore");
        q::refuses([&]{save::write_save(file,hostile,fixture.root.data,fixture.mask());},
            "Hostile performer/venue authority reached atomic save publication");
        check(world.snapshot()==baseline&&f::read_text(file)==bytes,"Rejected restore/write changed live authority or preserved bytes");
    };
    reject([](auto& s){s.original_production->culture.reset();});
    reject([](auto& s){s.original_production->safe_demolition.emplace();});
    reject([](auto& s){s.original_production->culture->schools.clear();});
    reject([](auto& s){s.original_production->culture->venues.clear();});
    reject([](auto& s){auto& rows=s.original_production->culture->schools;rows.push_back(rows.front());});
    reject([](auto& s){auto& rows=s.original_production->culture->venues;rows.push_back(rows.front());});
    reject([](auto& s){s.original_production->culture->next_performer_id=0;});
    reject([](auto& s){++s.original_production->culture->next_performer_id;});
    reject([](auto& s){s.original_production->culture->schools.front().courier_id=static_cast<sim::CourierId>(999);});
    reject([](auto& s){s.original_production->culture->schools.front().building_id=s.buildings.front().id;});
    reject([](auto& s){s.original_production->culture->schools.front().spawn_counter=255;});
    reject([](auto& s){s.original_production->culture->schools.front().attempts=0;});
    reject([](auto& s){s.original_production->culture->schools.front().spawned=0;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip.reset();});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->performer_id=static_cast<sim::PerformerId>(0);});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->venue_id=s.buildings.front().id;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=s.ticks+1;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=36;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->dispatched_tick=189;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->accepted=true;});
    reject([](auto& s){s.original_production->culture->schools.front().latest_trip->arrival_tick=s.ticks;});
    reject([](auto& s){s.original_production->culture->venues.front().music_timer=32;});
    reject([](auto& s){s.couriers.front().role=sim::CourierRole::OriginalMusic;});
    reject([](auto& s){s.couriers.front().owner=s.buildings.front().id;});
    reject([](auto& s){s.couriers.front().phase=sim::CourierPhase::Returning;});
    reject([](auto& s){s.couriers.front().target=s.buildings.front().id;});
    reject([](auto& s){s.couriers.front().cargo=1;});
    reject([](auto& s){s.couriers.front().reserved=1;});
    reject([](auto& s){s.couriers.front().last_dispatched_pottery=s.buildings.front().id;});
    reject([](auto& s){s.couriers.front().path.clear();});
    reject([](auto& s){s.command_sequence=UINT64_MAX;});
    reject([](auto& s){++s.treasury;});
    reject([](auto& s){s.original_economy->model_source.reset();});
    reject([](auto& s){s.original_economy->model_source->sha256=std::string(64,'0');});
    const auto source=q::state(world).schools.front().building_id,target=q::state(world).venues.front().building_id;
    q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},"Hostile-arrival baseline missing");
    const auto arrived=fixture.document(world);const auto arrived_bytes_file=fixture.root.path/"arrived38.json";
    save::write_save(arrived_bytes_file,arrived,fixture.root.data,fixture.mask());const auto arrived_bytes=f::read_text(arrived_bytes_file);
    for(int variant=0;variant<5;++variant) {
        auto bad=arrived;auto& venue=bad.world.original_production->culture->venues.front();
        if(variant==0)venue.last_arrival.reset();
        if(variant==1)++venue.music_timer;
        if(variant==2)venue.last_arrival->arrival_tick=bad.world.ticks+1;
        if(variant==3)venue.last_arrival->performer_id=static_cast<sim::PerformerId>(0);
        if(variant==4)venue.last_arrival->school_id=target;
        q::refuses([&]{(void)fixture.restore(bad);},"Duplicate/phantom/clock-shifted Venue arrival restored");
        q::refuses([&]{save::write_save(arrived_bytes_file,bad,fixture.root.data,fixture.mask());},"Forged arrival overwrote canonical save");
        check(f::read_text(arrived_bytes_file)==arrived_bytes,"Rejected actual-arrival forgery changed preserved bytes");
    }
}
void unused_channel_and_actual_population_clock() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world,false);
    const auto snapshot=world.snapshot();check(!q::school(world,s.source).latest_trip&&!world.total_population(),
        "Unspawned channel fixture unexpectedly acquired residents or a lifetime");
    for(int variant=0;variant<4;++variant) {
        auto hostile=snapshot;auto& school=hostile.original_production->culture->schools.front();
        if(variant==0)school.spawn_counter=1;
        if(variant==1)school.attempts=1;
        if(variant==2)hostile.couriers.front().route_checked_revision=hostile.road_revision;
        if(variant==3)hostile.couriers.front().reroute_attempts=1;
        q::refuses([&]{(void)fixture.restore(hostile);},"Unadmitted population or never-created actor supplied training/route history");
    }
    // This later pre-admission snapshot has sufficient source-age cycles for
    // forged training, but still has no real House admission or workers.
    q::through(world,119);auto no_people=world.snapshot();
    no_people.original_production->culture->schools.front().spawn_counter=1;
    q::refuses([&]{(void)fixture.restore(no_people);},"Prepopulation cycles trained a performer without any admitted residents");
}
void duplicate_actual_incoming_and_source_ownership() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    const auto second=q::buy(world,sim::CommandType::PlaceOriginalMusicSchool,{114,115});
    for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    (void)q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    q::until(world,[&]{return q::active(world,s.source)&&q::active(world,second);},
        "Two paid Schools never earned simultaneous distinct physical performers");
    auto hostile=world.snapshot();auto& schools=hostile.original_production->culture->schools;
    check(schools.size()==2&&schools[0].latest_trip->venue_id!=schools[1].latest_trip->venue_id,
          "Two actual incoming performers were not independently admitted");
    const auto target=schools[0].latest_trip->venue_id;schools[1].latest_trip->venue_id=target;
    for(auto& channel:hostile.couriers)if(channel.id==schools[1].courier_id){channel.target=target;channel.last_dispatched_target=target;}
    q::refuses([&]{sim::World::validate_original_music_venues_snapshot(hostile);},
        "Two active performers reserved one actual empty slot");
    hostile=world.snapshot();auto& duplicate=hostile.original_production->culture->schools;
    duplicate[1].latest_trip->performer_id=duplicate[0].latest_trip->performer_id;
    q::refuses([&]{(void)fixture.restore(hostile);},"One actual performer identity changed paid School ownership");
}
void accepted_arrival_cannot_be_moved_into_a_natural_fire_interval() {
    q::Fixture fixture;auto world=earned(fixture);const auto source=q::state(world).schools.front().building_id;
    q::until(world,[&]{return q::school(world,source).latest_trip->arrival_tick.has_value();},"Fire-provenance fixture lacks actual accepted arrival");
    check(q::school(world,source).latest_trip->accepted,"Fire-provenance baseline was not actually accepted");
    q::through(world,2600);auto bad=fixture.document(world);
    auto& culture=*bad.world.original_production->culture;culture.schools.front().latest_trip->arrival_tick=2001;
    culture.venues.front().last_arrival->arrival_tick=2001;
    culture.venues.front().music_timer=q::expected_timer(world.ticks(),2001);
    q::refuses([&]{(void)fixture.restore(bad);},"Accepted Venue arrival was forged into its natural burning interval");
    const auto file=fixture.root.path/"fire-provenance38.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    q::refuses([&]{save::write_save(file,bad,fixture.root.data,fixture.mask());},"Burning-interval receipt reached atomic publication");
    check(f::read_text(file)==bytes,"Rejected burning-interval receipt changed prior canonical bytes");
}
void one_retired_performer_cannot_witness_two_venues() {
    q::Fixture fixture;auto world=fixture.world();const auto s=q::scene(world);
    q::until(world,[&]{return q::venue(world,s.target).last_arrival.has_value();},"Compacted witness fixture lacks first actual arrival");
    for(int x=124;x<=129;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,114}});
    const auto other=q::buy(world,sim::CommandType::PlaceOriginalTheatre,{126,115});
    q::until(world,[&]{return q::school(world,s.source).spawned==2&&q::active(world,s.source);},
        "Compacted witness fixture lacks second actually active performer");
    check(q::school(world,s.source).latest_trip->venue_id==other&&!q::venue(world,other).last_arrival,
          "Second actual trip did not retain its empty physical destination");
    f::buy(world,sim::set_building_operation(s.source,false));
    const auto file=fixture.root.path/"two-venues38.json";
    save::write_save(file,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(file);
    auto hostile=save::read_save(file);auto& rows=hostile.world.original_production->culture->venues;
    check(rows.size()==2&&rows[0].last_arrival&&rows[0].music_timer&&!rows[1].last_arrival,
          "Actual compacted arrival baseline did not preserve both distinct Venue states");
    rows[1].last_arrival=rows[0].last_arrival;rows[1].music_timer=rows[0].music_timer;
    q::refuses([&]{(void)fixture.restore(hostile);},"One retired one-way performer witnessed two Venues");
    q::refuses([&]{save::write_save(file,hostile,fixture.root.data,fixture.mask());},"Duplicate historical performer reached atomic publication");
    check(f::read_text(file)==bytes,"Rejected duplicate historical performer changed saved bytes");
}
void strict_json_source_and_generation_isolation() {
    q::Fixture fixture;auto world=earned(fixture);const auto path=fixture.root.path/"canonical.json",bad=fixture.root.path/"hostile.json";
    save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());const auto bytes=f::read_text(path);const auto json=Json::parse(bytes);
    const auto reject=[&](auto mutate){auto hostile=json;mutate(hostile);f::write_text(bad,hostile.dump());
        q::refuses([&]{(void)save::read_save(bad);},"Hostile Schema38 JSON shape/version was accepted");
        check(f::read_text(path)==bytes,"Hostile JSON modified its canonical input");};
    reject([](auto& j){j["rules"].erase("music_venues");});
    reject([](auto& j){j["rules"]["music_venues"]="unverified";});
    reject([](auto& j){j["world"]["original_production"].erase("culture");});
    reject([](auto& j){j["world"]["original_production"]["culture"]["unsupported"]=0;});
    reject([](auto& j){j["world"]["original_production"]["culture"]["schools"][0]["trainees"]=1;});
    reject([](auto& j){j["world"]["original_production"]["culture"]["venues"][0]["house_bonus"]=96;});
    reject([](auto& j){j["schema_version"]=34;});
    reject([](auto& j){j["rules"]["version"]=18;});
    reject([](auto& j){j["world"]["couriers"][0]["role"]=28;});
    auto duplicate=bytes;const auto marker=duplicate.find("\"music_venues\":");check(marker!=std::string::npos,"Canonical marker missing");
    duplicate.insert(marker,"\"music_venues\":\"authored-music-venue-v1\",");f::write_text(bad,duplicate);
    q::refuses([&]{(void)save::read_save(bad);},"Duplicate identical Schema38 marker accepted");
    const auto document=save::read_save(path);q::refuses([&]{(void)sim::World::restore_original_deadline_food_dispatch(document.world,
        fixture.policy,fixture.model,fixture.terrain);},"Historical18 restore acquired new22 state");
    q::refuses([&]{(void)save::restore_original_safe_demolition(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},
        "Rule19 restore accepted a relabelled music generation");
    auto wrong=document;wrong.map_sha256=std::string(64,'0');
    q::refuses([&]{(void)fixture.restore(wrong);},"Missing full map fingerprint entered Rule22");
    const auto raw=fixture.raw;auto forged=raw;forged[114*228+110]=4;
    const auto context=std::make_shared<const sim::OriginalTerrainFacts>(forged,fixture.terrain->source_map_sha256());
    q::refuses([&]{(void)save::restore_original_music_venues(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,context);},
        "Prepared same-SHA terrain forgery bypassed live full-source comparison");
    const auto model_path=fixture.root.data/"Model/EmperorBuildingModels.txt";const auto original_model=f::read_text(model_path);
    f::write_text(model_path,original_model+"\n");q::refuses([&]{(void)fixture.restore(document);},
        "Retained prepared model bypassed changed live file provenance");f::write_text(model_path,original_model);
    check(f::read_text(path)==bytes,"Source-refusal checks mutated a prior complete save");
    const auto old_model=save::prepare_original_production_rules_model(fixture.root.data);
    auto old=sim::World::create_original_deadline_food_dispatch(fixture.policy,old_model,fixture.terrain);
    const auto old_path=fixture.root.path/"historical34.json";save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());
    const auto old_bytes=f::read_text(old_path);const auto old_doc=save::read_save(old_path);
    check(!old_doc.world.original_production->culture,"Historical34 serialization acquired an empty new authority");
    q::refuses([&]{(void)fixture.restore(old_doc);},"Historical34 silently migrated to38");
    auto injected=Json::parse(old_bytes);injected["rules"]["music_venues"]=sim::original_music_venues_contract;
    injected["world"]["original_production"]["culture"]=json["world"]["original_production"]["culture"];
    f::write_text(bad,injected.dump());q::refuses([&]{(void)save::read_save(bad);},"Old schema accepted injected performer authority");
    const auto restored=save::restore_original_deadline_food_dispatch(old_doc,fixture.root.data,fixture.mask(),fixture.policy,old_model,fixture.terrain);
    save::write_save(old_path,fixture.document(restored),fixture.root.data,fixture.mask());check(f::read_text(old_path)==old_bytes,
        "Historical Rule18 complete canonical bytes changed");
}
fs::path private_path(const char* raw,bool input) {
    const auto allowed=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/oe56";
    std::error_code error;const auto path=fs::weakly_canonical(fs::absolute(raw),error);
    check(!error&&path!=allowed&&std::mismatch(allowed.begin(),allowed.end(),path.begin(),path.end()).first==allowed.end(),
        "Use a strict explicit private .local/oe56 path");
    for(auto p=fs::absolute(raw);!p.empty();p=p.parent_path()) {
        check(!fs::is_symlink(p),"Linked proof paths are forbidden");if(p==p.root_path())break;
    }
    check(input?fs::is_regular_file(path):!fs::exists(path),"Preserve existing process inputs and outputs");return path;
}
unsigned ticks(const char* raw) {
    unsigned result=0;const std::string_view text=raw;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&result<=20400,"Use bounded continuation ticks0..20400");return result;
}
int process(int argc,char** argv) {
    q::Fixture fixture;
    if((argc==3||argc==4)&&std::string_view(argv[1])=="--create") {
        const auto output=private_path(argv[2],false);auto world=earned(fixture);
        const auto count=argc==4?ticks(argv[3]):0;for(unsigned i=0;i<count;++i)world.tick();q::invariants(world);
        fs::create_directories(output.parent_path());save::write_save(output,fixture.document(world),fixture.root.data,fixture.mask());return 0;
    }
    check((argc==4||argc==5)&&std::string_view(argv[1])=="--restore","Use --create OUTPUT [TICKS] or --restore INPUT OUTPUT [TICKS]");
    const auto input=private_path(argv[2],true),output=private_path(argv[3],false);const auto bytes=f::read_text(input);
    auto world=fixture.restore(save::read_save(input));const auto count=argc==5?ticks(argv[4]):0;
    for(unsigned i=0;i<count;++i)world.tick();q::invariants(world);
    fs::create_directories(output.parent_path());save::write_save(output,fixture.document(world),fixture.root.data,fixture.mask());
    check(f::read_text(input)==bytes,"Independent restore changed its input bytes");
    if(!count)check(f::read_text(output)==bytes,"Independent complete Schema38 rewrite differs");return 0;
}
}
int main(int argc,char** argv){try{
    if(argc>1)return process(argc,argv);
    checked_model_capability();canonical_active_arrived_expired_and_recovery();hostile_world_and_atomic_publication();
    unused_channel_and_actual_population_clock();duplicate_actual_incoming_and_source_ownership();
    accepted_arrival_cannot_be_moved_into_a_natural_fire_interval();
    one_retired_performer_cannot_witness_two_venues();
    strict_json_source_and_generation_isolation();
    std::cout<<"Rule22/schema38 full active/arrived/expired Performer saves, strict source/model/hostile/Recovery and historical18 bytes PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<"Music Venue Save FAIL: "<<error.what()<<'\n';return 1;}}
