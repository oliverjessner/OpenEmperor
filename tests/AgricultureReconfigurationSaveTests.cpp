#include "AgricultureReconfigurationFixture.h"
#include "persistence/RecoveryStore.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace t=oe_agriculture_reconfiguration_fixture;namespace sim=t::sim;namespace save=t::save;namespace f=t::f;
using Json=nlohmann::json;using t::check;
template<class F>void refuses(F action,const char* reason){try{action();}catch(const std::exception&){return;}throw std::runtime_error(reason);}
std::string roundtrip(const t::Fixture& fixture,const sim::World& world,const std::filesystem::path& path,int continuation=3){
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto doc=save::read_save(path);const auto json=Json::parse(bytes);
    check(doc.source_schema_version==52&&doc.world.rule_version==36&&doc.world==before&&doc.world.agriculture_reconfiguration&&doc.world.processed_food&&
        !doc.world.hunting_food&&json["rules"]["agriculture_reconfiguration"]==sim::agriculture_reconfiguration_contract&&
        json["rules"]["agriculture_reconfiguration_revision"]==1&&json["world"]["agriculture_reconfiguration"]["revision"]==1,
        "Schema52 lost its exact marker, journal or sole typed Food authority");
    auto restored=fixture.restore(doc);check(restored.snapshot()==before,"Schema52 changed work, cargo, closed history or drain state");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());check(f::read_text(path)==bytes&&world.snapshot()==before,"Schema52 canonical bytes or source World changed");
    auto expected=world;for(int n=0;n<continuation;++n){expected.tick();restored.tick();check(expected.snapshot()==restored.snapshot(),"Independent schema52 continuation diverged");}
    t::valid(expected);t::valid(restored);return bytes;
}
struct Hostile {
    const t::Fixture& fixture;const sim::World& world;std::filesystem::path path;std::string bytes;Json original;sim::WorldSnapshot before;
    Hostile(const t::Fixture& f,const sim::World& w,const char* name):fixture(f),world(w),path(f.root.path/name),bytes(roundtrip(f,w,path)),original(Json::parse(bytes)),before(w.snapshot()){}
    template<class F>void reject(F mutate,const char* reason){
        try{
            auto json=original;mutate(json);const auto input=json.dump();f::write_text(path,input);
            refuses([&]{(void)fixture.restore(save::read_save(path));},reason);
            check(world.snapshot()==before&&f::read_text(path)==input,"Rejected52 restore changed live authority or supplied bytes");f::write_text(path,bytes);
        }catch(const std::exception& error){throw std::runtime_error(path.filename().string()+": "+reason+": "+error.what());}
    }
};
void pending_loaded_retired_rebuilt_roundtrip_and_recovery(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,2> kinds{0,4};const auto city=t::q::build(world,kinds);const auto target=city.fields.front();
    const auto path=fixture.root.path/"reconfiguration52.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=[&]{roundtrip(fixture,world,path);const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==52&&entry.rule_version==36&&fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery lost exact52 pending/removal identity");};
    checkpoint();t::until(world,[&]{const auto s=t::worker(world,city.farm);return s.latest_trip&&!s.latest_trip->returned_tick&&s.latest_trip->field_id==target&&world.agriculture_field_progress(target)>0&&world.agriculture_field_progress(target)<80;},"Save control lacks actual partial Field work");
    f::buy(world,sim::begin_agriculture_drain(target));checkpoint();
    const auto pending=Json::parse(f::read_text(path))["world"]["agriculture_reconfiguration"]["entries"][0];
    check(pending["removed_tick"].is_null()&&pending["removed_sequence"].is_null(),"Pending drain did not serialize exact paired nulls");
    t::until(world,[&]{const auto s=t::worker(world,city.farm);return s.latest_trip&&s.latest_trip->field_id==target&&sim::physical_food_trip_cargo(*s.latest_trip)>0;},"Pending Field never earned real loaded harvest");checkpoint();
    auto restored=fixture.restore(save::read_save(path));for(int n=0;n<2000&&!world.demolition_status(target).allowed;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Loaded pending drain failed actual return replay");}
    check(world.demolition_status(target).allowed&&t::receipt(world,target).returned>0,"Pending Field never physically returned its earned Food");
    f::buy(world,sim::demolish_building(target));checkpoint();const auto old_work=t::work(world,target);const auto old_receipt=t::receipt(world,target);
    const auto replacement=t::crop(world,city.farm,1,world.building(target).cell);check(replacement>target,"Rebuilt paid Field reused retired ID");checkpoint();
    Hostile rebuilt(fixture,world,"hostile-rebuilt-lifetime52.json");
    check(world.building(replacement).placed_tick>0,"Rebuild lifetime control lacks a genuinely later paid purchase");
    rebuilt.reject([&](auto& j){for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<unsigned>(replacement))b["placed_tick"]=0;},
        "Same-site replacement was paid before its predecessor's known physical lifetime closed");
    roundtrip(fixture,world,path,1500);check(t::work(world,target)==old_work&&t::receipt(world,target)==old_receipt,"Save replay changed closed Field history");
    check(recovery.catalog().histories.size()==1&&!recovery.catalog().truncated,"Schema52 Recovery exceeded its bounded history");
}
void strict_journal_and_loaded_orphans_are_atomic(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,2> kinds{0,4};const auto city=t::q::build(world,kinds);const auto target=city.fields.front();
    t::until(world,[&]{const auto s=t::worker(world,city.farm);return s.latest_trip&&!s.latest_trip->returned_tick&&s.latest_trip->field_id==target&&world.agriculture_field_progress(target)>=8&&world.agriculture_field_progress(target)<80;},"Hostile control lacks actual partial work");
    f::buy(world,sim::begin_agriculture_drain(target));
    Hostile same_tick(fixture,world,"hostile-same-tick52.json");
    same_tick.reject([](auto& j){auto& base=j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["base_work"];
        check(base.template get<std::uint64_t>()>1,"Same-tick control lacks positive partial work");base=base.template get<std::uint64_t>()-1;},
        "Initial crop work understated the actual prefix without any elapsed World tick");
    t::ticks(world,1);
    f::buy(world,sim::begin_agriculture_drain(city.fields[1]));Hostile pending(fixture,world,"hostile-pending52.json");
    pending.reject([](auto& j){j["rules"].erase("agriculture_reconfiguration");},"Missing52 contract accepted");
    pending.reject([](auto& j){j["rules"]["agriculture_reconfiguration"]="other";},"Unknown52 contract accepted");
    pending.reject([](auto& j){j["rules"]["agriculture_reconfiguration_revision"]=2;},"Unknown52 rule revision accepted");
    pending.reject([](auto& j){j["world"].erase("agriculture_reconfiguration");},"Missing52 state accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["revision"]=2;},"Unknown52 state revision accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["stock"]=1;},"Parallel retirement inventory accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["discard"]=true;},"Unknown entry authority accepted");
    check(!pending.original["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"].is_null(),"Actual partial Field drain lost its immutable initial work");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0].erase("initial_work");},"Missing required initial drain-work key accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]=nullptr;},"Field drain lost mandatory initial-work provenance");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["inventory"]=1;},"Unknown initial drain authority accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["inventory"]=1;},"Unknown initial work witness key accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"].erase("clipped_bonus");},"Incomplete five-counter initial work accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["base_work"]=UINT64_MAX;},"Initial work borrowed future base ticks");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["wet_work_ticks"]=UINT64_MAX;},"Initial work borrowed future wet ticks");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["bonus_work"]=UINT64_MAX;},"Initial earned bonus overflow accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["clipped_bonus"]=UINT64_MAX;},"Initial clipped bonus overflow accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["work"]["wasted_work"]=UINT64_MAX;},"Initial wasted-work underflow accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]["harvested"]=UINT64_MAX;},"Initial harvest count borrowed future production");
    pending.reject([&](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["started_tick"]=world.building(target).placed_tick;},"Drain at purchase epoch borrowed later actual crop work");
    pending.reject([](auto& j){auto& initial=j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"];initial["work"]={{"base_work",0},{"wet_work_ticks",0},{"bonus_work",0},{"clipped_bonus",0},{"wasted_work",0}};initial["harvested"]=0;},"Zero-work drain started a later crop cycle");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0].erase("removed_tick");},"Missing required pending null accepted");
    pending.reject([](auto& j){auto& e=j["world"]["agriculture_reconfiguration"]["entries"][0];e["removed_tick"]=e["started_tick"];},"Unpaired removal tick accepted");
    pending.reject([](auto& j){auto& e=j["world"]["agriculture_reconfiguration"]["entries"][0];e["removed_sequence"]=e["started_sequence"];},"Unpaired removal sequence accepted");
    pending.reject([](auto& j){auto& rows=j["world"]["agriculture_reconfiguration"]["entries"];rows.push_back(rows[0]);},"Duplicate52 paid identity accepted");
    pending.reject([](auto& j){auto& rows=j["world"]["agriculture_reconfiguration"]["entries"];while(rows.size()<9)rows.push_back(rows[0]);},"Overbound52 journal accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["started_sequence"]=0;},"Zero drain command sequence accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["started_sequence"]=1;},"Drain command predating its paid building identity accepted");
    pending.reject([](auto& j){auto& rows=j["world"]["agriculture_reconfiguration"]["entries"];check(rows.size()==2,"Cross-order control lacks two real requests");
        const auto first=rows[0]["started_tick"],second=rows[1]["started_tick"];check(first<second,"Cross-order control lacks distinct real ticks");
        rows[0]["started_tick"]=second;rows[1]["started_tick"]=first;},"Retained drain command sequences contradicted the sole monotone World clock");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["started_sequence"]=UINT64_MAX;},"Future drain command sequence accepted");
    pending.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["started_tick"]=UINT64_MAX;},"Future drain tick accepted");
    pending.reject([&](auto& j){for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<unsigned>(target))b["placed"]=false;},"Pending entry silently retired an active paid Field");
    pending.reject([&](auto& j){auto& e=j["world"]["agriculture_reconfiguration"]["entries"][0];e["building_id"]=static_cast<unsigned>(city.farm);},"Unsupported Farm acquired drain authority");
    pending.reject([](auto& j){j["world"]["multi_field_agriculture"]["fields"][0]["productive_ticks"]=UINT64_MAX;},"Overflowing saved crop work accepted");
    pending.reject([](auto& j){j["world"]["processed_food"]["fields"].erase(0);},"Orphaned typed Field history accepted");
    pending.reject([](auto& j){j["world"]["hunting_food"]=j["world"]["processed_food"];},"Parallel historical Food authority accepted");
    t::until(world,[&]{const auto s=t::worker(world,city.farm);return s.latest_trip&&s.latest_trip->field_id==target&&sim::physical_food_trip_cargo(*s.latest_trip)>0;},"Hostile control lacks actual loaded Grower");
    Hostile loaded(fixture,world,"hostile-loaded52.json");
    loaded.reject([&](auto& j){auto& e=j["world"]["agriculture_reconfiguration"]["entries"][0];e["removed_tick"]=j["world"]["ticks"];e["removed_sequence"]=j["world"]["command_sequence"];for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<unsigned>(target))b["placed"]=false;},"Loaded work/cargo was closed without physical return");
    loaded.reject([](auto& j){j["world"]["physical_food_growers"]["farms"][0]["latest_trip"]["field_id"]=0;},"Loaded Grower lost its exact pending Field identity");
    loaded.reject([](auto& j){j["world"]["physical_food_growers"]["fields"][0]["returned"]=UINT64_MAX;},"Loaded harvest was forged into returned Food");
    t::until(world,[&]{return world.demolition_status(target).allowed;},"Real loaded cargo never physically returned");f::buy(world,sim::demolish_building(target));
    Hostile retired(fixture,world,"hostile-retired52.json");
    retired.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"].clear();},"Unplaced paid Field lost required tombstone provenance");
    retired.reject([&](auto& j){for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<unsigned>(target))b["placed"]=true;},"Closed retirement resurrected paid occupancy");
    retired.reject([](auto& j){auto& e=j["world"]["agriculture_reconfiguration"]["entries"][0];e["removed_tick"]=e["started_tick"].template get<std::uint64_t>()-1;},"Removal predating actual drain accepted");
    retired.reject([](auto& j){auto& e=j["world"]["agriculture_reconfiguration"]["entries"][0];e["removed_sequence"]=e["started_sequence"].template get<std::uint64_t>()-1;},"Removal command predating drain accepted");
    retired.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["removed_tick"]=UINT64_MAX;},"Future retirement accepted");
    retired.reject([&](auto& j){for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<unsigned>(target))b["kind"]=static_cast<unsigned>(sim::Object::Warehouse);},"Unsupported Warehouse tombstone accepted");
    auto duplicate=retired.bytes;const std::string marker="\"agriculture_reconfiguration_revision\": 1";const auto at=duplicate.find(marker);check(at!=std::string::npos,"Duplicate-key control lost52 marker");duplicate.replace(at,marker.size(),marker+",\n    "+marker);f::write_text(retired.path,duplicate);refuses([&]{(void)save::read_save(retired.path);},"Duplicate52 marker accepted");f::write_text(retired.path,retired.bytes);
    auto bad=fixture.document(world);bad.world.agriculture_reconfiguration->entries[0].removed_tick=UINT64_MAX;refuses([&]{save::write_save(retired.path,bad,fixture.root.data,fixture.mask());},"Writer published a forged tombstone");
    check(f::read_text(retired.path)==retired.bytes&&world.snapshot()==retired.before,"Rejected writer changed paid authority or target bytes");
    refuses([&]{save::write_save(retired.path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Atomic52 publication fault ignored");check(f::read_text(retired.path)==retired.bytes,"Failed52 publication replaced valid bytes");
}
void closed_pump_fire_and_upkeep_survive_later_ticks(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat,false);const auto network=t::i::network(world);
    t::purchase(world,sim::CommandType::PlaceFireWatch,{86,96});t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});
    t::until(world,[&]{return world.ticks()>=900&&t::i::work(world,city.fields.front()).wet_work_ticks>0&&std::any_of(world.original_production_state()->fire_protection->arrivals.begin(),world.original_production_state()->fire_protection->arrivals.end(),[&](const auto& a){return a.trip.target==network.pump;});},"Closed Pump control lacks real paid upkeep, wet work and Inspector receipt");
    const auto rate=static_cast<std::uint64_t>(world.workforce_required(network.pump))*static_cast<std::uint64_t>(sim::original_worker_upkeep_funds);check(rate>0,"Paid Pump has no source-model upkeep");
    f::buy(world,sim::begin_agriculture_drain(network.pump));
    Hostile pending_pump(fixture,world,"hostile-pending-pump52.json");
    pending_pump.reject([&](auto& j){for(auto& pump:j["world"]["irrigation"]["pumps"])if(pump["building_id"]==static_cast<unsigned>(network.pump))pump["eligible_since_tick"]=world.building(network.pump).placed_tick+1;},
        "Pending Pump retained a forged current supply epoch");
    t::until(world,[&]{return world.demolition_status(network.pump).allowed;},"Pump Inspector did not actually return before retirement");
    f::buy(world,sim::demolish_building(network.pump));const auto closed=world.building(network.pump);const auto path=fixture.root.path/"closed-pump52.json";roundtrip(fixture,world,path);
    t::ticks(world,1000);check(world.building(network.pump)==closed&&!world.building_fire_protected(network.pump)&&!world.building_on_fire(network.pump),"Retired Pump clock or active fire authority changed");roundtrip(fixture,world,path,1000);
    Hostile hostile(fixture,world,"hostile-pump52.json");
    check(hostile.original["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"].is_null(),"Pump drain acquired crop work authority");
    hostile.reject([](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"]={{"work",{{"base_work",0},{"wet_work_ticks",0},{"bonus_work",0},{"clipped_bonus",0},{"wasted_work",0}}},{"harvested",0}};},"Pump drain acquired an unrelated crop-work witness");
    hostile.reject([&](auto& j){j["world"]["maintenance_spent_total"]=j["world"]["maintenance_spent_total"].template get<std::uint64_t>()-rate;j["world"]["treasury"]=j["world"]["treasury"].template get<std::int64_t>()+static_cast<std::int64_t>(rate);},"Coherent Funds forgery erased an already incurred retired-Pump invoice");
    hostile.reject([&](auto& j){const auto ended=j["world"]["agriculture_reconfiguration"]["entries"][0]["removed_tick"].template get<std::uint64_t>();for(auto& b:j["world"]["buildings"])if(b["id"]==static_cast<unsigned>(network.pump))b["fire_protection_until_tick"]=ended+sim::Rules::fire_protection_ticks+1;},"Retired Pump borrowed later fire protection");
    hostile.reject([&](auto& j){
        const auto ended=j["world"]["agriculture_reconfiguration"]["entries"][0]["removed_tick"].template get<std::uint64_t>();bool matched_pump_arrival=false;
        // Fire arrivals retain the frozen tuple [tick,extinguished,trip],
        // where trip[2] is the actual paid target identity.
        for(auto& arrival:j["world"]["original_production"]["fire_protection"]["arrivals"]){
            check(arrival.is_array()&&arrival.size()==3&&arrival[2].is_array()&&arrival[2].size()==9,"Inspector hostile lost the exact historical tuple syntax");
            if(arrival[2][2]!=static_cast<unsigned>(network.pump))continue;
            arrival[0]=ended+1;matched_pump_arrival=true;break;
        }
        check(matched_pump_arrival,"Inspector hostile lacks its actual paid Pump arrival");
    },"Retired Pump received a forged later Inspector arrival");
}
void retired_pump_gap_cannot_certify_later_dry_work_as_wet(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat);
    const auto network=t::i::network(world);f::buy(world,sim::begin_agriculture_drain(network.pump));f::buy(world,sim::demolish_building(network.pump));
    check(world.ticks()==0&&!world.building(network.pump).placed,"Closed-gap control missed real retirement before first possible water phase");
    const auto target=city.fields.front();t::until(world,[&]{return t::receipt(world,target).returned==1;},"Closed-gap control lacks genuine dry80 work and physical source return");
    f::buy(world,sim::set_building_operation(city.farm,false));
    check(t::work(world,target).productive_ticks==80&&t::i::work(world,target).wet_work_ticks==0,"Closed-gap control borrowed water or later work");
    Hostile hostile(fixture,world,"hostile-closed-pump-gap52.json");
    // A clipped quarter adds no growth: every earned-work, actual harvest,
    // cargo, source receipt and sole inventory equation stays unchanged.
    hostile.reject([&](auto& j){
        bool matched_crop_work=false;
        for(auto& row:j["world"]["irrigation_productivity"]["work"]){
            if(row["building_id"]!=static_cast<unsigned>(target))continue;
            row["wet_work_ticks"]=4;row["clipped_bonus"]=1;matched_crop_work=true;break;
        }
        check(matched_crop_work,"Closed-gap hostile lost exact crop work row");
    },
        "Retired Pump borrowed future wet ticks from after its only paid supply lifetime closed");
}
void initial_clip_quota_cannot_borrow_a_later_real_cap(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat);
    const auto network=t::i::network(world);const auto target=city.fields.front();
    f::buy(world,sim::set_building_operation(city.farm,false));
    t::until(world,[&]{return world.irrigation_field_status(world.building(target).cell).water_available;},"Initial clip control never reached actual paid water supply");
    f::buy(world,sim::set_building_operation(city.farm,true));
    t::until(world,[&]{return t::i::work(world,target).wet_work_ticks==4;},"Initial clip control lacks four real wet work ticks");
    check(t::work(world,target).productive_ticks==4&&t::i::work(world,target).bonus_work==1&&world.agriculture_field_progress(target)==5,"Initial clip control lacks its real first earned quarter");
    f::buy(world,sim::begin_agriculture_drain(target));f::buy(world,sim::set_building_operation(network.pump,false));
    t::until(world,[&]{return world.agriculture_field_progress(target)==76;},"Pending crop never earned its real dry partial cycle");
    f::buy(world,sim::set_building_operation(city.farm,false));f::buy(world,sim::set_building_operation(network.pump,true));
    t::until(world,[&]{return world.irrigation_field_status(world.building(target).cell).water_available;},"Initial clip control never received a real later refill");
    f::buy(world,sim::set_building_operation(city.farm,true));
    t::until(world,[&]{return t::i::work(world,target).clipped_bonus==1;},"Actual final wet quarter did not clip at the eighty-unit cap");
    check(t::i::work(world,target).wet_work_ticks==8&&t::i::work(world,target).bonus_work==1&&world.agriculture_field_progress(target)==80,"Clip quota control lacks its genuine complete physical crop");
    Hostile hostile(fixture,world,"hostile-initial-clipped52.json");
    // Both counters remain below their now-real totals and wet/4 remains
    // bonus+clipped. The cap did not exist at the immutable request epoch.
    hostile.reject([](auto& j){auto& initial=j["world"]["agriculture_reconfiguration"]["entries"][0]["initial_work"];
        check(initial["work"]["base_work"]==4&&initial["work"]["wet_work_ticks"]==4&&initial["work"]["bonus_work"]==1&&initial["work"]["clipped_bonus"]==0,"Clip hostile lost its actual initial quarter");
        initial["work"]["bonus_work"]=0;initial["work"]["clipped_bonus"]=1;},
        "Initial drain witness borrowed a later real clipped quarter before any eighty-unit cycle existed");
}
void pump_drain_preserves_incoming_inspector_but_rejects_later_dispatch(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat,false);
    f::buy(world,sim::set_building_operation(city.farm,false));const auto network=t::i::network(world);
    t::purchase(world,sim::CommandType::PlaceFireWatch,{86,96});t::purchase(world,sim::CommandType::PlaceFireWatch,{109,110});
    t::until(world,[&]{
        for(const auto& trip:world.original_production_state()->fire_protection->trips){
            if(trip.target==network.pump&&trip.dispatched_tick==world.ticks())return true;
        }
        return false;
    },
        "Pump dispatch control lacks a real newly begun physical Inspector visit");
    const auto dispatched=world.ticks();check(dispatched>world.building(network.pump).placed_tick,"Inspector control predates its paid Pump");
    f::buy(world,sim::begin_agriculture_drain(network.pump));
    check(world.agriculture_reconfiguration_state()->entries[0].started_tick==dispatched&&t::work(world,city.fields.front()).productive_ticks==0,
        "Pump same-tick drain control changed actual dispatch epoch or earned crop work");
    Hostile active(fixture,world,"hostile-pump-active-inspector52.json");
    const auto backdate=[&](auto& j){j["world"]["agriculture_reconfiguration"]["entries"][0]["started_tick"]=dispatched-1;};
    active.reject(backdate,"Pending Pump admitted an active Inspector dispatched after its recorded drain start");
    t::until(world,[&]{
        for(const auto& arrival:world.original_production_state()->fire_protection->arrivals){
            if(arrival.trip.target==network.pump&&arrival.trip.dispatched_tick==dispatched)return true;
        }
        return false;
    },
        "Pump drain erased a physically incoming Inspector instead of allowing its actual arrival");
    Hostile arrived(fixture,world,"hostile-pump-arrived-inspector52.json");
    arrived.reject(backdate,"Pending Pump admitted a retained Inspector arrival whose trip began after its recorded drain start");
}
void historical51_and_strict_own_target_protection(){
    t::Fixture fixture;auto current=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(current,wheat);f::buy(current,sim::demolish_building(city.fields.front()));
    const auto path=fixture.root.path/"own52.json";const auto bytes=roundtrip(fixture,current,path);
    auto old=sim::World::create_food_warehouses(fixture.policy,fixture.model,fixture.terrain);t::w::build(old);const auto old_path=fixture.root.path/"old51.json";save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());const auto old_bytes=f::read_text(old_path);const auto doc=save::read_save(old_path);const auto json=Json::parse(old_bytes);
    check(doc.source_schema_version==51&&!doc.world.agriculture_reconfiguration&&!json["world"].contains("agriculture_reconfiguration")&&!json["rules"].contains("agriculture_reconfiguration"),"Historical51 acquired52 authority");
    auto restored=save::restore_food_warehouses(doc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);save::write_save(old_path,fixture.document(restored),fixture.root.data,fixture.mask());check(f::read_text(old_path)==old_bytes,"Historical51 bytes changed on exact F5");
    refuses([&]{(void)save::restore_agriculture_reconfiguration(doc,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},"Historical51 automatically migrated to52");
    refuses([&]{save::write_save(old_path,fixture.document(current),fixture.root.data,fixture.mask());},"52 writer overwrote historical51");refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"51 writer overwrote52");
    check(f::read_text(path)==bytes&&f::read_text(old_path)==old_bytes,"Cross-generation refusal changed target bytes");
    auto hidden=fixture.document(old);hidden.world.agriculture_reconfiguration.emplace();refuses([&]{save::write_save(old_path,hidden,fixture.root.data,fixture.mask());},"Historical writer silently dropped52 authority");
    for(const char* damaged:{"{\"schema_version\":52,broken","{\"schema_version\":\"52\",broken"}){f::write_text(path,damaged);refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"51 writer replaced damaged52 target");check(f::read_text(path)==damaged,"Damaged52 protection changed supplied bytes");}
}
}
template<class F>void run_case(const char* name,F action){try{action();}catch(const std::exception& error){throw std::runtime_error(std::string(name)+": "+error.what());}}
int main(){try{
    run_case("pending/loaded/retired/rebuilt Recovery",pending_loaded_retired_rebuilt_roundtrip_and_recovery);
    run_case("strict journal and loaded orphans",strict_journal_and_loaded_orphans_are_atomic);
    run_case("closed Pump fire and upkeep",closed_pump_fire_and_upkeep_survive_later_ticks);
    run_case("closed Pump wet gap",retired_pump_gap_cannot_certify_later_dry_work_as_wet);
    run_case("initial clipped-quarter quota",initial_clip_quota_cannot_borrow_a_later_real_cap);
    run_case("Pump Inspector drain dispatch epochs",pump_drain_preserves_incoming_inspector_but_rejects_later_dispatch);
    run_case("historical51/strict own targets",historical51_and_strict_own_target_protection);
    std::cout<<"Agriculture reconfiguration strict52 Save/Recovery, pending cargo, closed Pump clocks and historical51 passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
