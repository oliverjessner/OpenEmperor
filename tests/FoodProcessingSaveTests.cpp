#include "FoodProcessingFixture.h"
#include "persistence/RecoveryStore.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace t=oe_food_processing_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using Json=nlohmann::json;
using f::check;
template<class F>void refuses(F action,const char* why){try{action();}catch(const std::exception&){return;}throw std::runtime_error(why);}
std::string roundtrip(const t::Fixture& fixture,const sim::World& world,const std::filesystem::path& path,int continuation=3){
    const auto before=world.snapshot();save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(path);const auto document=save::read_save(path);const auto json=Json::parse(bytes);
    check(document.source_schema_version==50&&document.world.rule_version==34&&document.world==before&&
        document.world.processed_food&&document.world.food_processing&&!document.world.hunting_food&&
        !json["world"].contains("hunting_food")&&json["rules"]["food_processing"]==sim::food_processing_contract&&
        json["rules"]["food_processing_revision"]==1&&json["world"]["food_processing"]["revision"]==1&&
        json["world"]["processed_food"]["catalog"]==sim::processed_food_catalog,
        "Schema50 lost exact identity, exclusive twelve-column Food or processing history");
    auto restored=fixture.restore(document);check(restored.snapshot()==before,"Schema50 restore changed recipe work, cargo or receipts");
    save::write_save(path,fixture.document(restored),fixture.root.data,fixture.mask());
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Schema50 canonical roundtrip changed bytes or source World");
    auto expected=world;for(int i=0;i<continuation;++i){expected.tick();restored.tick();
        check(expected.snapshot()==restored.snapshot(),"Independently restored processing continuation diverged");}
    t::valid(expected);t::valid(restored);return bytes;
}
void paid_pipeline_roundtrips_and_recovery(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    const auto path=fixture.root.path/"processing50.json";
    save::RecoveryStore recovery(fixture.root.path/"recovery",fixture.root.data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(fixture.document(world),fixture.mask());
    const auto checkpoint=[&]{roundtrip(fixture,world,path);const auto entry=recovery.write_checkpoint(history,fixture.document(world),fixture.mask());
        check(entry.loadable()&&entry.schema==50&&entry.rule_version==34&&fixture.restore(save::read_save(entry.path)).snapshot()==world.snapshot(),
            "Recovery changed sole Food or real Mill/Shop authority");};
    checkpoint();t::until(world,[&]{return t::mill(world,city.mill).active.has_value();},"No actual staffed processing job");checkpoint();
    const auto active=t::mill(world,city.mill).active;f::buy(world,sim::set_building_operation(city.mill,false));checkpoint();
    t::ticks(world,7);check(t::mill(world,city.mill).active==active,"Paused Mill changed its retained atomic recipe");
    f::buy(world,sim::set_building_operation(city.mill,true));
    t::until(world,[&]{return t::mill(world,city.mill).last_receipt.has_value();},"No actual processing completion receipt");checkpoint();
    t::until(world,[&]{
        for(const auto& trip:world.processed_food_state()->trips)for(std::size_t g=7;g<12;++g) {
            if(trip.loaded[g]>trip.delivered[g]+trip.returned[g])return true;
        }
        return false;
    },"No physical loaded meal transport");checkpoint();
    t::until(world,[&]{return !world.food_processing_state()->houses.empty()&&world.food_processing_state()->houses[0].first_consumption.has_value();},
        "No actual House meal arrival and consumption provenance");checkpoint();roundtrip(fixture,world,path,12000);
    check(recovery.catalog().histories.size()==1&&!recovery.catalog().truncated,"Processing recovery exceeded bounded history");
}
void hostile_processing_authority_is_atomic(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    t::until(world,[&]{return t::mill(world,city.mill).active.has_value()&&t::mill(world,city.mill).last_receipt.has_value()&&
        !world.food_processing_state()->houses.empty()&&world.food_processing_state()->houses[0].first_consumption.has_value();},
        "Hostile control lacks genuine active/complete/House evidence");
    const auto path=fixture.root.path/"hostile50.json";const auto bytes=roundtrip(fixture,world,path);const auto original=Json::parse(bytes);const auto before=world.snapshot();
    const auto reject=[&](auto mutate,const char* why){auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before&&f::read_text(path)==hostile,"Rejected processing restore changed paid authority or supplied bytes");};
    reject([](auto& j){j["rules"].erase("food_processing");},"Missing processing marker accepted");
    reject([](auto& j){j["rules"]["food_processing"]="other";},"Unknown processing marker accepted");
    reject([](auto& j){j["rules"]["food_processing_revision"]=2;},"Unknown processing rule revision accepted");
    reject([](auto& j){j["world"].erase("food_processing");},"Missing processing state accepted");
    reject([](auto& j){j["world"]["food_processing"]["revision"]=2;},"Unknown processing state revision accepted");
    reject([](auto& j){j["world"]["food_processing"]["meals"]=4;},"Mirrored meal authority accepted");
    reject([](auto& j){j["world"]["hunting_food"]=j["world"]["processed_food"];},"Parallel historical Food account accepted");
    reject([](auto& j){j["world"]["processed_food"]["catalog"]="seven-food";},"Wrong twelve-kind Food catalog accepted");
    reject([](auto& j){j["world"]["processed_food"]["stocks"][0]["amounts"].push_back(0);},"Wrong Food width accepted");
    reject([](auto& j){auto& rows=j["world"]["food_processing"]["mills"];rows.push_back(rows[0]);},"Duplicate Mill ID accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"].clear();},"Missing paid Mill accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["requested_grade"]=5;},"Unavailable grade5 recipe accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["productive_ticks"]=UINT64_MAX;},"Overflowing recipe work accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["ingredients_consumed"][0][0]=UINT64_MAX;},"Forged raw debit history accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["completed_by_grade"][3]=UINT64_MAX;},"Forged meal output history accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["active"]["ingredients"][0]=2;},"Repeated ingredient in one recipe accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["active"]["work"]=80;},"Completed work retained as active accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["active"]["sequence"]=0;},"Zero recipe sequence accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["last_receipt"]["completed_tick"]=UINT64_MAX;},"Future processing receipt accepted");
    reject([](auto& j){j["world"]["food_processing"]["shops"][0]["market_id"]=0;},"FoodShop lost positive parent Market accepted");
    reject([&](auto& j){j["world"]["food_processing"]["shops"][0]["market_id"]=static_cast<std::uint32_t>(city.farm);},"FoodShop bound to producer accepted");
    reject([](auto& j){auto& rows=j["world"]["food_processing"]["houses"];rows.push_back(rows[0]);},"Duplicate immutable House meal provenance accepted");
    reject([](auto& j){j["world"]["food_processing"]["houses"][0]["first_receipt"]["accepted"][11]=1;},"Unavailable grade5 first receipt accepted");
    reject([](auto& j){j["world"]["food_processing"]["houses"][0]["first_consumption"]["tick"]=UINT64_MAX;},"Future first meal consumption accepted");
    reject([](auto& j){j["world"]["food_processing"]["houses"][0]["first_consumption"]["grade"]=5;},"Unavailable grade5 consumption accepted");
    auto duplicate=bytes;const std::string marker="\"food_processing_revision\": 1";const auto at=duplicate.find(marker);
    check(at!=std::string::npos,"Duplicate-key control lost processing marker");duplicate.replace(at,marker.size(),marker+",\n    "+marker);
    f::write_text(path,duplicate);refuses([&]{(void)save::read_save(path);},"Duplicate schema50 marker accepted");
    f::write_text(path,bytes);auto bad=fixture.document(world);bad.world.food_processing->mills[0].productive_ticks=UINT64_MAX;
    refuses([&]{save::write_save(path,bad,fixture.root.data,fixture.mask());},"Writer published forged processing work");
    check(f::read_text(path)==bytes&&world.snapshot()==before,"Rejected processing writer replaced valid save");
    refuses([&]{save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask(),save::WriteFault::BeforeRename);},"Atomic publication fault ignored");
    check(f::read_text(path)==bytes,"Failed processing publication changed old target bytes");
}
void immutable_first_arrivals_work_and_consumption_cannot_borrow_future_history(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    f::buy(world,sim::set_mill_recipe(city.mill,1));
    const auto path=fixture.root.path/"causal-first50.json";std::string bytes;Json original;sim::WorldSnapshot before;
    const auto capture=[&]{bytes=roundtrip(fixture,world,path);original=Json::parse(bytes);before=world.snapshot();};
    const auto reject=[&](auto mutate,const char* why){auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before&&f::read_text(path)==hostile,"Rejected causal provenance changed actual authority or supplied bytes");
        f::write_text(path,bytes);};
    t::until(world,[&]{return t::mill(world,city.mill).active.has_value();},"First-job control lacks actual input and work");capture();
    check(t::mill(world,city.mill).started==1,"First-job negative accidentally borrowed older completed work");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["active"]["started_tick"]=1;},
        "A first Mill job borrowed ingredients before their actual arrival");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0].erase("first_ingredients");},"Missing required first ingredient provenance accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["first_ingredients"].push_back(nullptr);},"Wrong first ingredient history width accepted");
    reject([](auto& j){auto& rows=j["world"]["food_processing"]["mills"][0]["first_ingredients"];for(auto& row:rows)if(!row.is_null()){row["amount"]=0;break;}},
        "Zero-amount physical first ingredient receipt accepted");
    reject([](auto& j){auto& rows=j["world"]["food_processing"]["mills"][0]["first_ingredients"];for(auto& row:rows)if(!row.is_null()){row["tick"]=UINT64_MAX;break;}},
        "Future first ingredient receipt accepted");
    reject([](auto& j){auto& rows=j["world"]["food_processing"]["mills"][0]["first_ingredients"];for(auto& row:rows)if(!row.is_null()){row["teleport"]=true;break;}},
        "Unknown first ingredient receipt authority accepted");
    t::until(world,[&]{return t::mill(world,city.mill).first_completions[0].has_value();},"No first real grade completion");capture();
    reject([](auto& j){auto& mill=j["world"]["food_processing"]["mills"][0];auto& first=mill["first_completions"][0];
        first["batch"]["started_tick"]=1;first["completed_tick"]=20;
        if(mill["last_receipt"]["batch"]["sequence"]==first["batch"]["sequence"])mill["last_receipt"]=first;},
        "A coherent first/last completion pair predating its real ingredient arrival was accepted");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["first_completions"][0]=nullptr;},"Completed grade lost its actual first completion");
    t::until(world,[&]{return world.food_processing_state()->shops[0].first_meals[0].has_value();},"No first actual Shop grade receipt");capture();
    reject([](auto& j){auto& state=j["world"]["food_processing"];state["shops"][0]["first_meals"][0]["tick"]=state["mills"][0]["first_completions"][0]["completed_tick"];},
        "Shop first receipt borrowed its grade before physical movement after completion");
    reject([](auto& j){j["world"]["food_processing"]["shops"][0]["first_meals"][0]=nullptr;},"Shop meal receipts lost first grade provenance");
    t::until(world,[&]{return !world.food_processing_state()->houses.empty();},"No first actual House meal receipt");capture();
    const auto& current_first=world.food_processing_state()->houses[0];
    check(t::stock(world,current_first.house_id).quality_witness==current_first.first_receipt,"First House arrival control lacks an exact retained current receipt");
    reject([](auto& j){j["world"]["food_processing"]["houses"][0]["first_receipt"]["previous_quality"]=255;},"First meal receipt retained impossible previous quality");
    reject([](auto& j){auto& quality=j["world"]["food_processing"]["houses"][0]["first_receipt"]["previous_quality"];
        quality=quality.template get<unsigned>()==20?21:20;},"Same current/first transfer retained contradictory bounded previous quality");
    reject([](auto& j){auto& state=j["world"]["food_processing"];auto& house=state["houses"][0];const auto impossible=state["shops"][0]["first_meals"][0]["tick"];
        house["first_receipt"]["tick"]=impossible;house["first_by_grade"][0]["tick"]=impossible;},
        "House first grade receipt borrowed Shop stock before real outward arrival");
    reject([](auto& j){j["world"]["food_processing"]["houses"][0].erase("first_by_grade");},"House lost mandatory per-grade first provenance");
    f::buy(world,sim::set_mill_recipe(city.mill,4));
    t::until(world,[&]{const auto& first=world.food_processing_state()->houses[0];return first.first_consumption&&
        world.ticks()>=first.first_consumption->tick+816&&t::stock(world,first.house_id).consumed[10]>0;},
        "First-consumption control lacks later valid events and actual grade4 use");capture();
    check(world.food_processing_state()->houses[0].first_consumption->grade==1,"First grade control did not actually consume grade1 first");
    reject([](auto& j){auto& tick=j["world"]["food_processing"]["houses"][0]["first_consumption"]["tick"];
        tick=tick.template get<std::uint64_t>()+816;},"Immutable first consumption moved to a later valid Food event");
    reject([](auto& j){j["world"]["food_processing"]["houses"][0]["first_consumption"]["grade"]=4;},
        "Immutable first consumption borrowed a later actually consumed meal grade");
    reject([](auto& j){auto& house=j["world"]["food_processing"]["houses"][0];
        house["first_receipt"]["tick"]=120;house["first_by_grade"][0]["tick"]=120;house["first_consumption"]["tick"]=397;},
        "A coherent early receipt/Food-event pair borrowed later completed and transported meals");
}
void first_transfer_sequences_and_known_recipe_prefixes_are_jointly_consistent(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build(world);
    f::buy(world,sim::set_mill_recipe(city.mill,1));
    t::until(world,[&]{const auto& m=t::mill(world,city.mill);return m.completed_by_grade[0]>=6&&
        std::all_of(m.first_ingredients.begin(),m.first_ingredients.begin()+5,[](const auto& value){return value.has_value();});},
        "Joint identity/prefix control lacks real first ingredients and completed jobs");
    const auto pair=[&]()->std::optional<std::array<std::size_t,2>>{
        const auto& houses=world.food_processing_state()->houses;
        for(std::size_t b=0;b<houses.size();++b){const auto& later=houses[b];const auto& quality=t::stock(world,later.house_id).quality_witness;
            if(!quality||quality->transfer==later.first_receipt.transfer)continue;
            std::uint64_t maximum=0,latest=0;
            for(const auto& house:houses)if(house.first_receipt.transfer.courier_id==later.first_receipt.transfer.courier_id)
                maximum=std::max(maximum,house.first_receipt.transfer.sequence);
            for(const auto& trip:world.processed_food_state()->trips)if(trip.transfer.courier_id==later.first_receipt.transfer.courier_id)latest=trip.transfer.sequence;
            if(maximum==UINT64_MAX||latest<=maximum+1||later.first_receipt.transfer.sequence>=maximum)continue;
            for(std::size_t a=0;a<houses.size();++a)if(houses[a].first_receipt.transfer.courier_id==later.first_receipt.transfer.courier_id&&
                houses[a].first_receipt.transfer.sequence<later.first_receipt.transfer.sequence)return std::array<std::size_t,2>{a,b};
        }
        return {};
    };
    t::until(world,[&]{return pair().has_value();},"Joint transfer control lacks pruned same-courier first arrivals and later retained sequences");
    const auto path=fixture.root.path/"joint-first-identities50.json";std::string bytes;Json original;sim::WorldSnapshot before;
    const auto capture=[&]{bytes=roundtrip(fixture,world,path);original=Json::parse(bytes);before=world.snapshot();};
    const auto reject=[&](auto mutate,const char* why){auto json=original;mutate(json);const auto hostile=json.dump();f::write_text(path,hostile);
        refuses([&]{(void)fixture.restore(save::read_save(path));},why);
        check(world.snapshot()==before&&f::read_text(path)==hostile,"Rejected joint first identity changed live authority or supplied bytes");f::write_text(path,bytes);};
    capture();const auto selected=*pair();
    const auto replace_house_transfer=[](Json& house,const Json& key){const auto previous=house["first_receipt"]["transfer"];
        house["first_receipt"]["transfer"]=key;
        for(auto& first:house["first_by_grade"])if(!first.is_null()&&first["transfer"]==previous)first["transfer"]=key;};
    reject([&](auto& j){auto& houses=j["world"]["food_processing"]["houses"];
        replace_house_transfer(houses[selected[1]],houses[selected[0]]["first_receipt"]["transfer"]);},
        "Two different first House arrivals reused one immutable physical transfer key");
    reject([&](auto& j){auto& houses=j["world"]["food_processing"]["houses"];auto key=houses[selected[1]]["first_receipt"]["transfer"];
        std::uint64_t maximum=0;for(const auto& house:houses)if(house["first_receipt"]["transfer"]["courier_id"]==key["courier_id"])
            maximum=std::max(maximum,house["first_receipt"]["transfer"]["sequence"].template get<std::uint64_t>());
        key["sequence"]=maximum+1;replace_house_transfer(houses[selected[1]],key);},
        "An early first House arrival borrowed a later same-courier sequence while individual bounds remained valid");
    check(t::mill(world,city.mill).first_completions[0]->batch.sequence==1&&
        !t::mill(world,city.mill).first_completions[1]&&!t::mill(world,city.mill).first_completions[2]&&!t::mill(world,city.mill).first_completions[3],
        "Recipe prefix control already admitted another grade before its first grade1 completion");
    reject([](auto& j){j["world"]["food_processing"]["mills"][0]["first_completions"][0]["batch"]["sequence"]=5;},
        "First recipe sequence5 left four earlier completed jobs with no available grade");
    f::buy(world,sim::set_mill_recipe(city.mill,2));
    const auto reused_input=[&]()->std::optional<std::size_t>{const auto& m=t::mill(world,city.mill);
        if(!m.first_completions[1]||!m.last_receipt||m.last_receipt->batch.grade!=2||
            m.first_completions[1]->batch.sequence==m.last_receipt->batch.sequence)return {};
        for(std::size_t g=0;g<7;++g)if(m.ingredients_consumed[1][g]==1&&m.last_receipt->batch.ingredients[g]&&
            !m.first_completions[1]->batch.ingredients[g]&&m.first_ingredients[g]&&
            m.first_ingredients[g]->tick<m.first_completions[1]->batch.started_tick)return g;
        return {};
    };
    t::until(world,[&]{return reused_input().has_value();},"Known-job quota control lacks an earlier-available ingredient used by exactly one later grade2 job");capture();
    const auto ingredient=*reused_input();
    reject([&](auto& j){auto& flags=j["world"]["food_processing"]["mills"][0]["first_completions"][1]["batch"]["ingredients"];
        bool replaced=false;for(std::size_t g=0;g<flags.size();++g)if(flags[g].template get<unsigned>()){
            flags[g]=0;replaced=true;break;
        }
        check(replaced,"Known input negative lacks a retained distinct recipe flag");flags[ingredient]=1;},
        "Distinct first and last jobs reused the same single lifetime ingredient debit");
}
void historical49_and_cross_generation_targets_stay_exact(){
    t::Fixture fixture;const auto current=fixture.world();const auto path=fixture.root.path/"current50.json";const auto bytes=roundtrip(fixture,current,path);
    auto old=sim::World::create_irrigation_productivity(fixture.policy,fixture.model,fixture.terrain);const auto old_path=fixture.root.path/"historical49.json";
    save::write_save(old_path,fixture.document(old),fixture.root.data,fixture.mask());const auto old_bytes=f::read_text(old_path);const auto document=save::read_save(old_path);
    check(document.source_schema_version==49&&!document.world.processed_food&&!document.world.food_processing&&
        sim::World::restore_irrigation_productivity(document.world,fixture.policy,fixture.model,fixture.terrain).snapshot()==old.snapshot(),"Historical49 acquired processing authority");
    refuses([&]{(void)save::restore_food_processing(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);},"Schema49 automatically migrated");
    refuses([&]{save::write_save(old_path,fixture.document(current),fixture.root.data,fixture.mask());},"Schema50 writer overwrote schema49");
    refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"Historical writer overwrote schema50");
    check(f::read_text(path)==bytes&&f::read_text(old_path)==old_bytes,"Cross-generation refusal changed target bytes");
    auto hidden=fixture.document(old);hidden.world.food_processing.emplace();
    refuses([&]{save::write_save(old_path,hidden,fixture.root.data,fixture.mask());},"Historical writer silently dropped processing state");
    for(const char* damaged:{"{\"schema_version\":50,broken","{\"schema_version\":\"50\",broken"}){
        f::write_text(path,damaged);refuses([&]{save::write_save(path,fixture.document(old),fixture.root.data,fixture.mask());},"Historical writer replaced damaged50 target");
        check(f::read_text(path)==damaged,"Damaged target protection changed bytes");}
}
}
template<class F>void run(const char* name,F action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}}
int main(){try{run("paid pipeline/recovery/12k",paid_pipeline_roundtrips_and_recovery);run("hostile50",hostile_processing_authority_is_atomic);
    run("causal first histories",immutable_first_arrivals_work_and_consumption_cannot_borrow_future_history);
    run("joint first identities/known recipes",first_transfer_sequences_and_known_recipe_prefixes_are_jointly_consistent);
    run("historical49/cross-generation",historical49_and_cross_generation_targets_stay_exact);
    std::cout<<"Food processing strict Save/Recovery/12k replay tests passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
