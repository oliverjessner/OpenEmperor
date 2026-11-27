// A paid, deterministic Rule37/schema53 QA creator. The authored recipe below
// contains ordinary player commands; World alone admits, prices and runs them.
// No Original fixtures, saves, stocks, residents or work counters are imported.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "../tests/CheckpointRecoveryProof.h"
#include "../tests/CompleteCityAssessment.h"
#include "simulation/IrrigatedHarvestContract.h"
#include "simulation/QualityAwareFoodDispatchContract.h"

#include <charconv>
#include <iostream>
#include <openssl/evp.h>
#include <set>

namespace l=oe_luxury_progression;
namespace {
namespace sim=l::sim;
namespace orig=l::orig;
namespace save=l::save;
namespace fs=l::fs;
using Json=l::Json;
using Id=sim::BuildingId;
using C=sim::CommandType;
using P=sim::WorkforcePriority;
constexpr const char* creator_format="openemperor-paid-complete-city-creator-v1";
constexpr const char* plan_format="openemperor-paid-complete-city-plan-v1";
constexpr const char* recipe_id="paid-chengdu-complete-city-v7";
constexpr const char* whole_format="openemperor-paid-complete-city-whole-v2";
constexpr const char* source_map="Cities/Chengdu.map";
constexpr const char* usage="openemperor-paid-complete-city-creator --data ABS --output FRESH_.local/agri-mac14/SUBDIR --ticks SEED_END [--whole-city-ticks 100000] [--replay PRIOR_CREATOR_DIR] [--quality-aware-food-dispatch] (default explicitly creates Rule37/policy3/schema53; --quality-aware-food-dispatch separately selects Rule38/policy3/schema54; it does not select or migrate Rule36; optional whole-city QA observes the real paid lifecycle from empty tick0 and continues the same city100000 ticks beyond the unchanged seed)";

sim::Command command(C type,int x,int y,unsigned owner=0,std::uint8_t orientation=0,
                     std::uint8_t grade=0,bool operating=true,P priority=P::Normal) {
    sim::Command result{type,{x,y}};result.building_id=static_cast<Id>(owner);
    result.orientation=orientation;result.recipe_grade=grade;
    result.operating_enabled=operating;result.workforce_priority=priority;return result;
}
struct RecipeRow {sim::Command command;unsigned body_key=0,population_gate=0;std::uint64_t not_before_tick=0;};
const std::array recipe{
    RecipeRow{command(C::PlaceRoad,107,138),0,0},
    RecipeRow{command(C::PlaceRoad,108,138),0,0},
    RecipeRow{command(C::PlaceRoad,109,138),0,0},
    RecipeRow{command(C::PlaceRoad,110,138),0,0},
    RecipeRow{command(C::PlaceRoad,111,138),0,0},
    RecipeRow{command(C::PlaceRoad,112,138),0,0},
    RecipeRow{command(C::PlaceRoad,113,138),0,0},
    RecipeRow{command(C::PlaceRoad,114,138),0,0},
    RecipeRow{command(C::PlaceRoad,115,138),0,0},
    RecipeRow{command(C::PlaceRoad,116,138),0,0},
    RecipeRow{command(C::PlaceRoad,117,138),0,0},
    RecipeRow{command(C::PlaceRoad,118,138),0,0},
    RecipeRow{command(C::PlaceRoad,119,138),0,0},
    RecipeRow{command(C::PlaceRoad,120,138),0,0},
    RecipeRow{command(C::PlaceRoad,121,138),0,0},
    RecipeRow{command(C::PlaceRoad,122,138),0,0},
    RecipeRow{command(C::PlaceRoad,123,138),0,0},
    RecipeRow{command(C::PlaceRoad,124,138),0,0},
    RecipeRow{command(C::PlaceRoad,125,138),0,0},
    RecipeRow{command(C::PlaceRoad,126,138),0,0},
    RecipeRow{command(C::PlaceRoad,127,138),0,0},
    RecipeRow{command(C::PlaceRoad,128,138),0,0},
    RecipeRow{command(C::PlaceRoad,129,138),0,0},
    RecipeRow{command(C::PlaceRoad,130,138),0,0},
    RecipeRow{command(C::PlaceRoad,107,143),0,0},
    RecipeRow{command(C::PlaceRoad,108,143),0,0},
    RecipeRow{command(C::PlaceRoad,109,143),0,0},
    RecipeRow{command(C::PlaceRoad,110,143),0,0},
    RecipeRow{command(C::PlaceRoad,111,143),0,0},
    RecipeRow{command(C::PlaceRoad,112,143),0,0},
    RecipeRow{command(C::PlaceRoad,113,143),0,0},
    RecipeRow{command(C::PlaceRoad,114,143),0,0},
    RecipeRow{command(C::PlaceRoad,115,143),0,0},
    RecipeRow{command(C::PlaceRoad,116,143),0,0},
    RecipeRow{command(C::PlaceRoad,117,143),0,0},
    RecipeRow{command(C::PlaceRoad,118,143),0,0},
    RecipeRow{command(C::PlaceRoad,119,143),0,0},
    RecipeRow{command(C::PlaceRoad,120,143),0,0},
    RecipeRow{command(C::PlaceRoad,121,143),0,0},
    RecipeRow{command(C::PlaceRoad,122,143),0,0},
    RecipeRow{command(C::PlaceRoad,123,143),0,0},
    RecipeRow{command(C::PlaceRoad,124,143),0,0},
    RecipeRow{command(C::PlaceRoad,125,143),0,0},
    RecipeRow{command(C::PlaceRoad,126,143),0,0},
    RecipeRow{command(C::PlaceRoad,127,143),0,0},
    RecipeRow{command(C::PlaceRoad,128,143),0,0},
    RecipeRow{command(C::PlaceRoad,129,143),0,0},
    RecipeRow{command(C::PlaceRoad,130,143),0,0},
    RecipeRow{command(C::PlaceRoad,130,139),0,0},
    RecipeRow{command(C::PlaceRoad,130,140),0,0},
    RecipeRow{command(C::PlaceRoad,130,141),0,0},
    RecipeRow{command(C::PlaceRoad,130,142),0,0},
    RecipeRow{command(C::PlaceRoad,118,139),0,0},
    RecipeRow{command(C::PlaceRoad,118,140),0,0},
    RecipeRow{command(C::PlaceRoad,118,141),0,0},
    RecipeRow{command(C::PlaceRoad,118,142),0,0},
    RecipeRow{command(C::PlaceHousehold,107,136),1,0},
    RecipeRow{command(C::PlaceHousehold,110,136),2,0},
    RecipeRow{command(C::PlaceHousehold,113,136),3,0},
    RecipeRow{command(C::PlaceHousehold,116,136),4,0},
    RecipeRow{command(C::PlaceHousehold,119,136),5,0},
    RecipeRow{command(C::PlaceHousehold,122,139),6,0},
    RecipeRow{command(C::PlaceHousehold,126,141),7,0},
    RecipeRow{command(C::PlaceHousehold,128,141),8,0},
    RecipeRow{command(C::PlaceMarket,113,139),9,0},
    RecipeRow{command(C::PlaceOriginalFoodFarm,107,144),10,0},
    RecipeRow{command(C::PlaceOriginalMilletField,110,144,10),11,0},
    RecipeRow{command(C::PlaceFireWatch,119,139),12,0},
    RecipeRow{command(C::PlaceOriginalRiceField,106,144,10),13,0},
    RecipeRow{command(C::PlaceOriginalWheatField,107,142,10),14,0},
    RecipeRow{command(C::PlaceOriginalCabbageField,104,145,10),15,0},
    RecipeRow{command(C::PlaceOriginalSoybeanField,105,144,10),16,0},
    RecipeRow{command(C::PlaceOriginalFoodShop,114,139),17,160},
    RecipeRow{command(C::PlaceOriginalMill,131,138),18,160},
    RecipeRow{command(C::PlaceOriginalHuntersTent,116,139),19,0},
    RecipeRow{command(C::PlaceOriginalFishingQuay,107,134),20,0},
    RecipeRow{command(C::PlaceRoad,106,138),0,0},
    RecipeRow{command(C::PlaceRoad,106,137),0,0},
    RecipeRow{command(C::PlaceRoad,106,136),0,0},
    RecipeRow{command(C::PlaceRoad,106,135),0,0},
    RecipeRow{command(C::PlaceWarehouse,120,139),21,0},
    RecipeRow{command(C::PlaceRoad,106,146),0,0},
    RecipeRow{command(C::PlaceRoad,103,138),0,0},
    RecipeRow{command(C::PlaceRoad,104,138),0,0},
    RecipeRow{command(C::PlaceRoad,105,138),0,0},
    RecipeRow{command(C::PlaceOriginalFoodFarm,107,147),22,120},
    RecipeRow{command(C::PlaceRoad,106,143),0,0},
    RecipeRow{command(C::PlaceRoad,105,143),0,0},
    RecipeRow{command(C::PlaceRoad,104,143),0,0},
    RecipeRow{command(C::PlaceRoad,103,143),0,0},
    RecipeRow{command(C::PlaceRoad,102,143),0,0},
    RecipeRow{command(C::PlaceRoad,101,143),0,0},
    RecipeRow{command(C::PlaceRoad,101,144),0,0},
    RecipeRow{command(C::PlaceRoad,101,145),0,0},
    RecipeRow{command(C::PlaceRoad,101,146),0,0},
    RecipeRow{command(C::PlaceRoad,101,147),0,0},
    RecipeRow{command(C::PlaceRoad,102,147),0,0},
    RecipeRow{command(C::PlaceRoad,103,147),0,0},
    RecipeRow{command(C::PlaceRoad,104,147),0,0},
    RecipeRow{command(C::PlaceRoad,105,147),0,0},
    RecipeRow{command(C::PlaceRoad,106,147),0,0},
    RecipeRow{command(C::PlaceOriginalSoybeanField,106,148,22),23,0},
    RecipeRow{command(C::PlaceOriginalHempFarm,108,141),24,100},
    RecipeRow{command(C::PlaceOriginalHempField,107,141,24),25,0},
    RecipeRow{command(C::PlaceOriginalTeaCuringShed,111,144),26,240},
    RecipeRow{command(C::PlaceOriginalTeaBush,110,141,26),27,0},
    RecipeRow{command(C::PlaceOriginalLacquerRefinery,105,141),28,240},
    RecipeRow{command(C::PlaceOriginalLacquerTree,102,138,28),29,0},
    RecipeRow{command(C::PlaceOriginalSilkwormShed,102,144),30,240},
    RecipeRow{command(C::PlaceOriginalMulberryTree,101,141,30),31,0},
    RecipeRow{command(C::PlaceIrrigationPump,100,139,0,3),32,100},
    RecipeRow{command(C::PlaceRoad,103,139),0,0},
    RecipeRow{command(C::PlaceRoad,102,139),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,102,139),0,0},
    RecipeRow{command(C::PlaceFireWatch,119,142),33,0},
    RecipeRow{command(C::PlaceIrrigationDitch,102,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,102,141),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,103,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,102,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,103,141),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,104,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,102,143),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,103,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,101,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,104,141),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,105,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,104,139),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,104,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,100,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,106,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,105,139),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,104,138),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,104,143),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,100,143),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,107,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,106,139),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,108,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,109,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,110,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,111,140),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,111,141),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,111,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,112,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,113,142),0,0},
    RecipeRow{command(C::PlaceIrrigationDitch,114,142),0,0},
    RecipeRow{command(C::PlaceOriginalElitePlot,120,132),34,240},
    RecipeRow{command(C::PlaceOriginalAncestorShrine,118,134),35,0},
    RecipeRow{command(C::PlaceOriginalAncestorShrine,118,132),36,100},
    RecipeRow{command(C::PlaceOriginalDaoistShrine,124,134),37,100},
    RecipeRow{command(C::PlaceOriginalDaoistShrine,124,132),38,100},
    RecipeRow{command(C::PlaceOriginalBuddhistShrine,121,136),39,100},
    RecipeRow{command(C::PlaceOriginalBuddhistShrine,123,136),40,100},
    RecipeRow{command(C::PlaceOriginalInfluenceSource,125,136),41,100},
    RecipeRow{command(C::PlaceOriginalInfluenceSource,126,133),42,100},
    RecipeRow{command(C::PlaceOriginalInfluenceSource,122,130),43,100},
    RecipeRow{command(C::PlaceOriginalInfluenceSource,124,139),44,100},
    RecipeRow{command(C::PlaceOriginalWaterWell,126,139),45,0},
    RecipeRow{command(C::PlaceOriginalWaterWell,130,134),46,0},
    RecipeRow{command(C::PlaceOriginalMusicSchool,127,136),47,160},
    RecipeRow{command(C::PlaceOriginalAcrobatSchool,126,130),48,160},
    RecipeRow{command(C::PlaceRoad,115,137),0,0},
    RecipeRow{command(C::PlaceRoad,115,136),0,0},
    RecipeRow{command(C::PlaceRoad,115,135),0,0},
    RecipeRow{command(C::PlaceRoad,115,134),0,0},
    RecipeRow{command(C::PlaceRoad,115,133),0,0},
    RecipeRow{command(C::PlaceRoad,115,132),0,0},
    RecipeRow{command(C::PlaceRoad,115,131),0,0},
    RecipeRow{command(C::PlaceRoad,116,131),0,0},
    RecipeRow{command(C::PlaceRoad,117,131),0,0},
    RecipeRow{command(C::PlaceRoad,118,131),0,0},
    RecipeRow{command(C::PlaceRoad,119,131),0,0},
    RecipeRow{command(C::PlaceRoad,120,131),0,0},
    RecipeRow{command(C::PlaceRoad,118,137),0,0},
    RecipeRow{command(C::PlaceRoad,118,136),0,0},
    RecipeRow{command(C::PlaceRoad,129,137),0,0},
    RecipeRow{command(C::PlaceRoad,129,136),0,0},
    RecipeRow{command(C::PlaceRoad,129,135),0,0},
    RecipeRow{command(C::PlaceRoad,128,135),0,0},
    RecipeRow{command(C::PlaceRoad,127,135),0,0},
    RecipeRow{command(C::PlaceRoad,126,135),0,0},
    RecipeRow{command(C::PlaceRoad,120,130),0,0},
    RecipeRow{command(C::PlaceRoad,120,129),0,0},
    RecipeRow{command(C::PlaceRoad,120,128),0,0},
    RecipeRow{command(C::PlaceRoad,121,128),0,0},
    RecipeRow{command(C::PlaceRoad,122,128),0,0},
    RecipeRow{command(C::PlaceRoad,123,128),0,0},
    RecipeRow{command(C::PlaceRoad,124,128),0,0},
    RecipeRow{command(C::PlaceRoad,124,129),0,0},
    RecipeRow{command(C::PlaceRoad,124,130),0,0},
    RecipeRow{command(C::PlaceRoad,124,131),0,0},
    RecipeRow{command(C::PlaceRoad,125,131),0,0},
    RecipeRow{command(C::PlaceHealthPost,116,134),49,160},
    RecipeRow{command(C::PlaceServicePost,116,132),50,160},
    RecipeRow{command(C::PlaceOriginalLoggingShed,122,145),51,240},
    RecipeRow{command(C::PlaceRoad,122,144),0,0},
    RecipeRow{command(C::PlaceOriginalWeaver,120,145),52,240},
    RecipeRow{command(C::PlaceRoad,120,144),0,0},
    RecipeRow{command(C::PlaceOriginalLacquerwareMaker,124,145),53,240},
    RecipeRow{command(C::PlaceRoad,124,144),0,0},
    RecipeRow{command(C::PlaceClaySource,122,147),54,100},
    RecipeRow{command(C::PlaceRoad,125,144),0,0},
    RecipeRow{command(C::PlaceRoad,126,144),0,0},
    RecipeRow{command(C::PlaceRoad,127,144),0,0},
    RecipeRow{command(C::PlaceRoad,127,145),0,0},
    RecipeRow{command(C::PlaceRoad,127,146),0,0},
    RecipeRow{command(C::PlaceRoad,127,147),0,0},
    RecipeRow{command(C::PlaceRoad,126,147),0,0},
    RecipeRow{command(C::PlaceRoad,125,147),0,0},
    RecipeRow{command(C::PlaceRoad,124,147),0,0},
    RecipeRow{command(C::PlacePottery,122,141),55,100},
    RecipeRow{command(C::PlaceOriginalFoodFarm,116,144),56,180,3264},
    RecipeRow{command(C::PlaceOriginalMilletField,119,144,56),57,0},
    RecipeRow{command(C::PlaceOriginalSoybeanField,119,145,56),58,0},
    RecipeRow{command(C::PlaceOriginalMilletField,110,147,22),59,0},
    RecipeRow{command(C::PlaceOriginalHempFarm,116,147),60,180},
    RecipeRow{command(C::PlaceRoad,114,144),0,0},
    RecipeRow{command(C::PlaceRoad,114,145),0,0},
    RecipeRow{command(C::PlaceRoad,114,146),0,0},
    RecipeRow{command(C::PlaceRoad,114,147),0,0},
    RecipeRow{command(C::PlaceRoad,114,148),0,0},
    RecipeRow{command(C::PlaceRoad,115,148),0,0},
    RecipeRow{command(C::PlaceOriginalHempField,115,147,60),61,0},
    RecipeRow{command(C::SetMillRecipe,0,0,18,0,4),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,9,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,10,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,12,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,22,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,33,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,45,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,46,0,0,true,P::High),0,0},
    RecipeRow{command(C::SetBuildingWorkforcePriority,0,0,56,0,0,true,P::High),0,0},
    RecipeRow{command(C::PlaceOriginalRiceField,105,148,22),62,0,4000},
    RecipeRow{command(C::PlaceOriginalCabbageField,110,146,22),63,0,4000},
    RecipeRow{command(C::PlaceOriginalRiceField,115,144,56),64,0,4000},
    RecipeRow{command(C::PlaceOriginalCabbageField,119,146,56),65,0,4000},
};

Json encode(const sim::Command& value) {
    return {{"type",static_cast<unsigned>(value.type)},{"cell",{value.cell.x,value.cell.y}},
        {"owner",static_cast<unsigned>(value.building_id)},{"orientation",value.orientation},
        {"recipe_grade",value.recipe_grade},{"operating_enabled",value.operating_enabled},
        {"workforce_priority",static_cast<unsigned>(value.workforce_priority)}};
}
std::string sha256(std::string_view bytes) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned length=0;
    l::check(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)==1&&length==32,
        "Creator SHA256 failed");
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
    for(unsigned i=0;i<length;++i){result.push_back(hex[digest[i]>>4U]);result.push_back(hex[digest[i]&15U]);}
    return result;
}
struct Input {
    fs::path path;std::string bytes;fs::perms mode;fs::file_time_type modified;
    explicit Input(fs::path value,std::uintmax_t limit=16U*1024U*1024U):path(std::move(value)) {
        l::no_symlinks(path);l::check(fs::is_regular_file(path)&&fs::file_size(path)<=limit,
            "Require a bounded regular creator input");
        bytes=l::q::f::read_text(path);mode=fs::status(path).permissions();modified=fs::last_write_time(path);
    }
    void unchanged()const {
        l::check(l::q::f::read_text(path)==bytes&&fs::status(path).permissions()==mode&&
            fs::last_write_time(path)==modified,"Creator input bytes, permissions or modification time changed");
    }
};
struct Options {fs::path data,output,replay;unsigned ticks=0,whole_city_ticks=0;bool quality_aware_food_dispatch=false;};
Options options(int argc,char** argv) {
    std::map<std::string,std::string> args;
    for(int n=1;n<argc;++n) {
        const std::string key=argv[n];
        if(key=="--quality-aware-food-dispatch"){l::check(args.emplace(key,"true").second,"Duplicate quality-aware Food selection");continue;}
        l::check(n+1<argc,usage);
        l::check(key=="--data"||key=="--output"||key=="--ticks"||key=="--replay"||key=="--whole-city-ticks",usage);
        l::check(args.emplace(key,argv[++n]).second,"Duplicate creator argument");
    }
    l::check(args.contains("--data")&&args.contains("--output")&&args.contains("--ticks"),usage);
    Options result;result.quality_aware_food_dispatch=args.contains("--quality-aware-food-dispatch");l::no_symlinks(args.at("--data"));result.data=fs::canonical(args.at("--data"));
    const auto& value=args.at("--ticks");const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result.ticks);
    l::check(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&result.ticks>=4000&&result.ticks<=100000,
        "Require an explicit4000..100000 seed endpoint; no acceptance endpoint is inferred");
    if(args.contains("--whole-city-ticks")) {
        const auto& count=args.at("--whole-city-ticks");
        const auto consumed=std::from_chars(count.data(),count.data()+count.size(),result.whole_city_ticks);
        l::check(consumed.ec==std::errc{}&&consumed.ptr==count.data()+count.size()&&result.whole_city_ticks==100000,
            "Explicit whole-city lifecycle QA requires exactly100000 additional real ticks");
    }
    l::no_symlinks(args.at("--output"));result.output=fs::weakly_canonical(fs::absolute(args.at("--output")));
    const auto allowed=fs::weakly_canonical(fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac14");
    l::check(result.output!=allowed&&l::below(result.output,allowed)&&!l::below(result.output,result.data)&&
        !l::below(result.data,result.output)&&!fs::exists(result.output),
        "Use a fresh .local/agri-mac14 subdirectory disjoint from Original data");
    save::validate_save_target(result.output/"seed.save.json",result.data);
    if(args.contains("--replay")) {
        l::no_symlinks(args.at("--replay"));result.replay=fs::canonical(args.at("--replay"));
        l::check(fs::is_directory(result.replay)&&!l::below(result.replay,result.data),"Require a prior creator directory");
    }
    return result;
}
struct Context {
    l::q::Context map;std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain;unsigned rule,schema;
    explicit Context(const fs::path& data,bool quality=false):map(oe_longplay_fixture::real_map_context(data,source_map)),
        model(save::prepare_food_processing_rules_model(data)),
        terrain(save::prepare_playable_hunting_terrain_facts(data,source_map)),rule(quality?38U:37U),schema(rule+16) {}
    sim::World create()const {return rule==38?sim::World::create_quality_aware_food_dispatch(map.policy,model,terrain):sim::World::create_irrigated_harvest(map.policy,model,terrain);}
    save::SaveDocument document(const sim::World& world)const {return save::make_document(map.data,map.map_relative,map.mask,world);}
    sim::World restore(const save::SaveDocument& document)const {
        l::check(document.source_schema_version==schema&&document.world.rule_version==rule,"Exact explicitly selected Rule/Schema required");
        if(rule==38)return save::restore_quality_aware_food_dispatch(document,map.data,map.mask,map.policy,model,terrain);
        return save::restore_irrigated_harvest(document,map.data,map.mask,map.policy,model,terrain);
    }
};
std::size_t channels(const sim::World& world) {
    return world.couriers().size()+world.fishing_state()->quays.size()+world.hunting_state()->tents.size()+
        3*world.hunting_state()->habitats.size();
}
void valid(const sim::World& world) {
    l::check((world.rule_version()==37||world.rule_version()==38)&&world.map_permissions()&&world.map_permissions()->policy_version()==3,
        "Creator lost its explicit Rule37/policy3");
    l::check(!world.has_grower_navigation_session()&&world.grower_navigation_tracks().empty(),"No research navigation authority allowed");
    l::check(world.processed_food_state()&&!world.original_agriculture_state()&&!world.fishing_food_state()&&
        !world.hunting_food_state()&&world.original_production_state()->food==sim::OriginalFoodState{},
        "Require the sole inherited ProcessedFood ledger");
    l::check(world.population_valid()&&world.city_economy_valid()&&world.production_balance_valid()&&
        world.navigation_valid()&&world.fire_state_valid(),"Paid creator World invariant failed");
    l::check(world.buildings().size()<=82&&channels(world)<=58,"Unchanged82-body/58-channel bound exceeded");
    for(const auto good:l::luxury_goods) {
        const auto balance=world.original_goods_balance(good);
        l::check(balance.supported&&balance.conserved&&!balance.totals.imported&&!balance.totals.demolished,
            "Luxury source/conservation changed");
    }
    for(std::size_t i=0;i<sim::original_aux_material_count;++i)
        l::check(world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(i)).conserved,"Auxiliary conservation failed");
}
bool fields_reachable(const sim::World& world) {
    for(const auto& farm:world.physical_food_growers_state()->farms) {
        const auto& courier=world.courier(farm.courier_id);
        for(const auto& field:world.agriculture_fields())if(field.farm_id==farm.farm_id) {
            const auto row=std::find_if(courier.dynamic_target_routes.begin(),courier.dynamic_target_routes.end(),
                [&](const auto& item){return item.first==field.field_id;});
            if(row==courier.dynamic_target_routes.end()||!row->second||row->second->size()<2)return false;
        }
    }
    return true;
}
Json influence(const sim::World& world,Id id) {
    const auto proof=world.original_influence_provenance(id);Json rows=Json::array();
    for(const auto& row:proof.sources)rows.push_back({{"source_id",static_cast<unsigned>(row.building_id)},
        {"model",static_cast<unsigned>(row.type)},{"cell",{row.origin.x,row.origin.y}},{"width",row.footprint.width},
        {"spread",{row.spread.value,row.spread.step,row.spread.step_size,row.spread.range}},{"contribution",row.contribution}});
    return {{"house_id",static_cast<unsigned>(id)},{"selected_cell",{proof.selected_cell.x,proof.selected_cell.y}},
        {"raw_des",proof.raw_value},{"water_bonus",proof.cardinal_water_bonus},{"query_des",proof.value},{"sources",rows}};
}
template<class T>Json epoch(const std::optional<T>& value) {return value?Json(*value):Json(nullptr);}
Json field_work(const sim::World& world,Id field_id) {
    const auto& fields=world.agriculture_fields();const auto field=std::find_if(fields.begin(),fields.end(),
        [&](const auto& row){return row.field_id==field_id;});l::check(field!=fields.end(),"Missing paid Food field");
    const auto& body=world.building(field_id);const auto& farm=world.building(field->farm_id);
    Json result={{"field_id",static_cast<unsigned>(field_id)},{"farm_id",static_cast<unsigned>(field->farm_id)},
        {"crop",static_cast<unsigned>(field->crop)},{"cell",{body.cell.x,body.cell.y}},{"placed_tick",body.placed_tick},
        {"age",world.ticks()-body.placed_tick},{"harvested",field->harvested},
        {"actual_staff",{{"assigned",world.workers_assigned(farm.id)},{"required",world.workforce_required(farm.id)},
            {"staffed",world.building_staffed(farm.id)},{"operating",farm.operating_enabled},{"fire_until_tick",farm.fire_until_tick}}}};
    for(const auto& row:world.multi_field_agriculture_state()->fields)if(row.field_id==field_id)
        result["base_productive_ticks"]=row.productive_ticks;
    for(const auto& row:world.irrigation_productivity_state()->work)if(row.building_id==field_id)
        result["irrigation"]={{"wet_work_ticks",row.wet_work_ticks},{"bonus_work",row.bonus_work},{"clipped_bonus",row.clipped_bonus}};
    for(const auto& row:world.seasonal_agriculture_state()->fields)if(row.field_id==field_id)
        result["seasonal"]={{"wasted_work",row.wasted_work},{"expired_ready",row.expired_ready},
            {"harvest_work",row.harvest_work},{"harvest_progress",row.harvest_progress},
            {"wasted_harvest_work",row.wasted_harvest_work},{"ready_tick",epoch(row.ready_tick)}};
    for(const auto& row:world.physical_food_growers_state()->fields)if(row.field_id==field_id)
        result["physical_returned"]=row.returned;
    for(const auto& row:world.physical_food_growers_state()->farms)if(row.farm_id==farm.id&&row.latest_trip) {
        const auto& trip=*row.latest_trip;
        result["latest_trip"]={{"grower_id",trip.grower_id.value},{"courier_id",static_cast<unsigned>(row.courier_id)},
            {"field_id",static_cast<unsigned>(trip.field_id)},{"phase",static_cast<unsigned>(trip.phase)},
            {"dispatched_tick",trip.dispatched_tick},{"arrived_tick",epoch(trip.arrived_tick)},
            {"ready_tick",epoch(trip.ready_tick)},{"harvested_tick",epoch(trip.harvested_tick)},
            {"returned_tick",epoch(trip.returned_tick)},{"work_before",trip.work_before},{"work_ticks",trip.work_ticks},
            {"irrigation_bonus_before",trip.irrigation_bonus_before},{"wasted_work_before",trip.wasted_work_before},
            {"wait_left",trip.wait_left},{"delivered",trip.delivered},{"physical_cargo",sim::physical_food_trip_cargo(trip)}};
    }
    return result;
}

// Receipt JSON is lifted verbatim from the canonical, validated DTO just written
// by production SaveIO. These QA observations are never supplied to a World.
Json source_returns(const Json& world) {
    constexpr std::array names{"wheat","millet","rice","cabbage","soy","hemp","tea","lacquer","mulberry"};
    std::array<Json,9> result;
    for(std::size_t i=0;i<result.size();++i)result[i]={{"kind",names[i]},{"returned",0U},{"bindings",Json::array()}};
    const auto add=[&](std::size_t kind,unsigned source,unsigned leaf,std::uint64_t returned,const Json& receipt) {
        auto& row=result.at(kind);row["returned"]=row.at("returned").get<std::uint64_t>()+returned;
        row["bindings"].push_back({{"source_id",source},{"leaf_id",leaf},{"returned",returned},{"receipt",receipt}});
    };
    for(const auto& field:world.at("processed_food").at("fields")) {
        const auto leaf=field.at("field_id").get<unsigned>();const auto& physical=world.at("physical_food_growers").at("fields");
        const auto row=std::find_if(physical.begin(),physical.end(),[&](const auto& item){return item.at("field_id")==leaf;});
        l::check(row!=physical.end(),"Canonical Food DTO lost its actual physical return row");
        add(field.at("crop").get<std::size_t>(),field.at("farm_id").get<unsigned>(),leaf,
            row->at("returned").template get<std::uint64_t>(),row->at("last_receipt"));
    }
    for(const auto& farm:world.at("original_production").at("hemp_farms")) {
        const auto source=farm.at("farm_id").get<unsigned>();const auto& seasonal=world.at("seasonal_agriculture").at("hemp_farms");
        const auto row=std::find_if(seasonal.begin(),seasonal.end(),[&](const auto& item){return item.at("farm_id")==source;});
        l::check(row!=seasonal.end()&&!farm.at("field_id").is_null(),"Canonical Hemp DTO lost actual source/leaf");
        add(5,source,farm.at("field_id").get<unsigned>(),farm.at("harvested").get<std::uint64_t>(),row->at("last_receipt"));
    }
    for(const auto& plant:world.at("plantations").at("plants")) {
        const auto leaf=plant.at("plant_id").get<unsigned>();const auto& bodies=world.at("buildings");
        const auto body=std::find_if(bodies.begin(),bodies.end(),[&](const auto& item){return item.at("id")==leaf;});
        l::check(body!=bodies.end(),"Canonical plantation DTO lost its paid leaf");
        const auto kind=body->at("kind").template get<unsigned>();l::check(kind>=36&&kind<=38,"Unexpected plantation leaf kind");
        add(6+kind-36,plant.at("source_id").get<unsigned>(),leaf,plant.at("returned").get<std::uint64_t>(),plant.at("last_receipt"));
    }
    Json rows=Json::array();
    for(auto& row:result) {
        auto& bindings=row["bindings"];std::sort(bindings.begin(),bindings.end(),[](const auto& a,const auto& b) {
            return std::pair{a.at("source_id").template get<unsigned>(),a.at("leaf_id").template get<unsigned>()}<
                std::pair{b.at("source_id").template get<unsigned>(),b.at("leaf_id").template get<unsigned>()};
        });rows.push_back(std::move(row));
    }
    return rows;
}

// A report's storage filename is not an event identity. Before comparing two
// reports, prove every name against its own publication metadata and canonical
// manual payload. This QA object never supplies a document to gameplay.
struct PriorRecovery {
    using Manuals=std::vector<std::pair<std::string,fs::path>>;
    std::vector<Input> files;Json comparison;
    fs::path recovery_root,history_path;std::set<std::string> history_files;
    static Json evidence_json(const std::string& bytes) {
        std::vector<std::set<std::string>> keys;bool duplicate=false;
        const auto result=Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json& parsed) {
            l::check(depth<=32,"Recovery QA evidence nesting is unbounded");
            if(event==Json::parse_event_t::object_start)keys.emplace_back();
            else if(event==Json::parse_event_t::key) {
                l::check(!keys.empty(),"Recovery QA evidence has invalid object nesting");
                if(!keys.back().insert(parsed.get<std::string>()).second)duplicate=true;
            } else if(event==Json::parse_event_t::object_end)keys.pop_back();
            return true;
        });
        l::check(!duplicate,"Recovery QA evidence contains a duplicate key");return result;
    }
    PriorRecovery(const fs::path& directory,const fs::path& data,const Context& context,
                  const Json& proof,const Manuals& manuals,bool whole) {
        const auto& events=proof.at("verified_events");
        l::check(proof.at("contract")==oe_checkpoint_recovery::contract&&proof.at("history_count")==1&&
            proof.at("checkpoint_limit")==32&&events.is_array()&&!events.empty()&&
            events.size()==manuals.size()&&events.size()<=32,"Require bounded actual Recovery event provenance");
        const auto base=directory/(whole?"whole-city/checkpoint-recovery":"checkpoint-recovery");
        const auto root=base/"recovery";l::no_symlinks(root);
        recovery_root=root;
        l::check(fs::is_directory(root),"Prior Recovery root is missing");
        fs::path history;
        for(const auto& entry:fs::directory_iterator(root)) {
            l::check(history.empty()&&!entry.is_symlink()&&entry.is_directory(),
                "Require exactly one ordinary Recovery history");history=entry.path();
        }
        l::check(!history.empty(),"Prior Recovery history is missing");
        history_path=history;
        files.emplace_back(history/"history.json",1024U*1024U);
        const auto final_metadata=evidence_json(files.back().bytes);
        // Production catalog validation retains ownership of the actual
        // Recovery format, canonical metadata and source-bound payload rules.
        save::RecoveryStore store(base,data,save::recovery_storage_budget_bytes,save::RecoveryAccess::OriginalProduction);
        const auto catalog=store.catalog();
        l::check(!catalog.truncated&&catalog.histories.size()==1&&catalog.histories.front().error.empty()&&
            catalog.histories.front().history_id==history.filename().string()&&
            !catalog.histories.front().parent,"Prior Recovery catalog is not one valid unbranched history");
        for(const auto& entry:catalog.histories.front().entries)
            l::check(entry.loadable(),"Prior Recovery retained a nonloadable payload");

        std::vector<std::string> canonical;std::vector<Json> summaries;
        comparison=proof;
        for(std::size_t i=0;i<events.size();++i) {
            const auto& event=events.at(i);const auto& [label,path]=manuals.at(i);
            const auto filename=event.at("filename").get<std::string>();
            const auto prefix="checkpoint-"+std::to_string(i+1)+"-";
            std::uint32_t nonce=0;
            const auto suffix=filename.ends_with(".json")&&filename.size()>prefix.size()+5?
                std::string_view(filename).substr(prefix.size(),filename.size()-prefix.size()-5):std::string_view(filename).substr(0,0);
            const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),nonce);
            l::check(filename.size()<=128&&filename.starts_with(prefix)&&!suffix.empty()&&
                parsed.ec==std::errc{}&&parsed.ptr==suffix.data()+suffix.size()&&std::to_string(nonce)==suffix&&
                fs::path(filename).filename()==fs::path(filename),"Recovery event filename is not its safe exact sequence");
            files.emplace_back(directory/path);canonical.push_back(files.back().bytes);
            const auto saved=save::read_save(files.back().path);const auto restored=context.restore(saved);
            l::check(event==Json{{"label",label},{"tick",restored.ticks()},{"schema",context.schema},
                {"sequence",i+1},{"filename",filename},{"canonical_payload_exact",true},{"fresh_restore_exact",true}},
                "Recovery event differs from its exact source-bound manual checkpoint");
            summaries.push_back({{"tick",restored.ticks()},{"map",saved.map_relative.generic_string()},
                {"profile",sim::city_v16_profile_name},{"rule_version",context.rule},{"schema",context.schema},
                {"population",restored.total_population()},{"treasury",restored.treasury()}});
            comparison["verified_events"][i]["filename"]={{"canonical_payload_sha256",sha256(canonical.back())},
                {"event_identity",{{"label",label},{"tick",restored.ticks()},{"sequence",i+1},
                    {"schema",context.schema},{"rule",context.rule}}}};
        }
        std::map<std::uint64_t,Json> published;
        const auto validate_metadata=[&](const Json& metadata,std::size_t count) {
            const auto& entries=metadata.at("checkpoints");
            l::check(metadata==Json{{"version",1},{"history_id",history.filename().string()},{"parent",nullptr},
                {"next_sequence",count+1},{"checkpoints",entries}}&&entries.is_array()&&
                entries.size()==1+std::min(count,save::recovery_checkpoint_limit),
                "Publication history metadata is not one ordinary bounded sequence");
            std::set<std::uint64_t> sequences;std::set<std::string> filenames;
            for(const auto& entry:entries) {
                const auto sequence=entry.at("sequence").get<std::uint64_t>();
                l::check(sequence==0||(sequence<=count&&sequence+save::recovery_checkpoint_limit>count),
                    "Publication history does not retain exactly start plus last five");
                const auto index=sequence?sequence-1:0;
                const auto filename=sequence?events.at(index).at("filename").get<std::string>():"start.json";
                auto expected=summaries.at(index);
                const auto created=entry.at("created_local").get<std::string>();
                l::check(!created.empty()&&created.size()<=64,"Publication time metadata is unbounded");
                expected.update(Json{{"sequence",sequence},{"kind",sequence?"autosave":"start"},
                    {"filename",filename},{"created_local",created}});
                l::check(entry==expected&&sequences.insert(sequence).second&&filenames.insert(filename).second,
                    "Publication metadata differs from its exact canonical event/state or repeats an identity");
                const auto [found,inserted]=published.emplace(sequence,entry);
                l::check(inserted||found->second==entry,"A published Recovery metadata row changed before rotation");
            }
            l::check(sequences.contains(0)&&sequences.contains(count),"Publication history lost its start or current event");
        };
        if(whole) {
            // Old whole-city reports without publication evidence cannot prove
            // filenames already evicted by ordinary rotation; refuse honestly.
            files.emplace_back(base/"publication-ledger.json",1024U*1024U);
            const auto ledger=evidence_json(files.back().bytes);const auto& rows=ledger.at("publications");
            l::check(ledger==Json{{"contract",oe_checkpoint_recovery::publication_ledger_contract},{"publications",rows}}&&
                rows.is_array()&&rows.size()==events.size(),"Whole-city Recovery publication evidence is missing or incomplete");
            for(std::size_t i=0;i<rows.size();++i) {
                const auto& row=rows.at(i);const auto& metadata=row.at("history_metadata");
                l::check(row==Json{{"event",events.at(i)},{"canonical_payload_sha256",sha256(canonical.at(i))},
                    {"history_metadata",metadata}},"Publication ledger is not bound to its exact canonical manual payload/event");
                validate_metadata(metadata,i+1);
            }
            l::check(rows.back().at("history_metadata")==final_metadata,
                "Final publication metadata differs from the actual current history");
        } else l::check(events.size()<=save::recovery_checkpoint_limit,
            "Seed Recovery proof unexpectedly requires rotated publication evidence");
        validate_metadata(final_metadata,events.size());
        std::set<std::string> expected_files{"history.json"};
        for(const auto& entry:final_metadata.at("checkpoints")) {
            const auto sequence=entry.at("sequence").get<std::uint64_t>();
            const auto filename=entry.at("filename").get<std::string>();expected_files.insert(filename);
            files.emplace_back(history/filename);
            l::check(files.back().bytes==canonical.at(sequence?sequence-1:0),
                "Actual retained Recovery payload differs from its source-bound canonical manual save");
        }
        std::set<std::string> actual_files;
        for(const auto& entry:fs::directory_iterator(history)) {
            l::check(!entry.is_symlink()&&entry.is_regular_file(),"Recovery history contains unsafe or unlisted storage");
            actual_files.insert(entry.path().filename().string());
        }
        l::check(actual_files==expected_files,"Recovery history contains an unlisted payload");
        history_files=std::move(expected_files);
    }
    void unchanged()const {
        for(const auto& file:files)file.unchanged();
        std::set<fs::path> histories;
        for(const auto& entry:fs::directory_iterator(recovery_root)) {
            l::check(!entry.is_symlink()&&entry.is_directory(),"Recovery input history storage changed");
            histories.insert(entry.path());
        }
        l::check(histories==std::set<fs::path>{history_path},"Recovery input history inventory changed");
        std::set<std::string> names;
        for(const auto& entry:fs::directory_iterator(history_path)) {
            l::check(!entry.is_symlink()&&entry.is_regular_file(),"Recovery input payload storage changed");
            names.insert(entry.path().filename().string());
        }
        l::check(names==history_files,"Recovery input payload inventory changed");
    }
};

struct Prior {
    std::vector<Input> files;Json plan;bool selected=false;
    std::optional<PriorRecovery> recovery;
    Prior(const Options& options,const Context& context) {
        if(options.replay.empty())return;selected=true;
        for(const auto* name:{"initial.save.json","seed.save.json","player-plan.json","report.json","early-harvest.save.json","early-harvest.json","early-return.save.json"})
            files.emplace_back(options.replay/name);
        plan=Json::parse(files[2].bytes);const auto report=Json::parse(files[3].bytes);
        l::check(plan.at("format")==plan_format&&plan.at("recipe")==recipe_id&&plan.at("rule")==context.rule&&
            plan.at("schema")==context.schema&&plan.at("policy")==3&&plan.at("map")==source_map&&
            plan.at("initial_tick")==0&&plan.at("end_tick")==options.ticks&&plan.at("events").is_array()&&
            plan.at("events").size()<=4096,"Prior plan does not match this explicit Rule37 creator");
        l::check(report.at("format")==creator_format&&report.at("recipe")==recipe_id&&report.at("status")=="PASS"&&
            report.at("rule")==context.rule&&report.at("schema")==context.schema&&report.at("policy")==3&&
            report.at("seed_tick")==options.ticks&&report.at("model_sha256")==context.model->provenance().sha256&&
            report.at("map_sha256")==context.terrain->source_map_sha256(),"Prior creator provenance does not match supplied inputs");
        for(const auto& file:files)if(file.path.filename()!="report.json")
            l::check(report.at("artifacts_sha256").at(file.path.filename().string())==sha256(file.bytes),
                "Prior creator artifact bytes differ from its recorded SHA256");
        (void)context.restore(save::read_save(files[0].path));(void)context.restore(save::read_save(files[1].path));
        (void)context.restore(save::read_save(files[4].path));(void)context.restore(save::read_save(files[6].path));
        recovery.emplace(options.replay,options.data,context,report.at("checkpoint_recovery"),PriorRecovery::Manuals{
            {"earned-early-harvest","early-harvest.save.json"},{"earned-early-source-return","early-return.save.json"},
            {"paid-seed","seed.save.json"}},false);
    }
    void command(std::size_t index,const Json& event)const {
        if(selected)l::check(index<plan.at("events").size()&&plan.at("events")[index]==event,
            "Independent paid command/tick replay differs");
    }
    void finish(const fs::path& output,std::size_t commands)const {
        if(!selected)return;l::check(commands==plan.at("events").size(),"Independent replay command count differs");
        for(const auto& file:files) {
            file.unchanged();if(file.path.filename()!="report.json")
                l::check(l::q::f::read_text(output/file.path.filename())==file.bytes,"Independent creator artifact bytes differ");
        }
        recovery->unchanged();
    }
};

// Additional QA provenance binds only this explicitly selected fresh0 audit.
// Saved World authority remains production SaveIO; prior city stock is never
// used to initialize either the fresh paid replay or its assessment.
struct WholePrior {
    std::vector<Input> files;Json report;bool selected=false;
    std::optional<PriorRecovery> recovery;PriorRecovery::Manuals manuals;
    const Context& context;fs::path data;
    WholePrior(const Options& options,const Context& value,const Prior& seed):context(value),data(options.data) {
        if(!options.whole_city_ticks||options.replay.empty())return;
        selected=true;files.emplace_back(options.replay/"whole-city.json");report=Json::parse(files.front().bytes);
        l::check(report.at("format")==whole_format&&report.at("recipe")==recipe_id&&
            report.at("rule")==context.rule&&report.at("schema")==context.schema&&report.at("policy")==3&&
            report.at("map")==source_map&&report.at("model_sha256")==context.model->provenance().sha256&&
            report.at("map_sha256")==context.terrain->source_map_sha256()&&
            report.at("begin_tick")==0&&report.at("seed_tick")==options.ticks&&
            report.at("continued_ticks")==options.whole_city_ticks&&
            report.at("end_tick")==std::uint64_t(options.ticks)+options.whole_city_ticks&&
            report.at("whole_city").at("contract")==oe_complete_city::fresh_city_contract,
            "Prior whole-city provenance differs from the explicit fresh0 QA contract");
        l::check(seed.selected&&report.at("initial_sha256")==sha256(seed.files[0].bytes)&&
            report.at("seed_sha256")==sha256(seed.files[1].bytes)&&
            report.at("player_plan_sha256")==sha256(seed.files[2].bytes),
            "Prior whole-city report does not bind the exact independently earned paid seed artifacts");
        const auto& checkpoints=report.at("checkpoints");
        l::check(checkpoints.is_array()&&!checkpoints.empty()&&checkpoints.size()<=32,
            "Require a bounded explicit whole-city checkpoint manifest");
        std::uint64_t previous=0;unsigned next_window=1;bool maturity=false,final=false;
        for(const auto& row:checkpoints) {
            const auto label=row.at("label").get<std::string>();const auto tick=row.at("tick").get<std::uint64_t>();
            if(label=="maturity") {
                l::check(!maturity&&!final&&next_window==1&&tick==report.at("whole_city").at("maturity_tick"),
                    "Repeated or misplaced maturity checkpoint");maturity=true;
            } else if(label=="final") {
                l::check(!final&&tick==report.at("end_tick"),"Repeated or misplaced final checkpoint");final=true;
            } else {
                const auto& windows=report.at("whole_city").at("complete_post_maturity_windows");
                l::check(maturity&&!final&&label=="window-"+std::to_string(next_window)&&next_window<=windows.size()&&
                    tick==windows.at(next_window-1).at("end_tick"),"Unexpected whole-city checkpoint label or epoch");++next_window;
            }
            l::check(tick>=previous&&tick>0&&tick<=report.at("end_tick").get<std::uint64_t>()&&
                row.at("canonical_roundtrip_exact")==true&&row.at("fresh_restore_exact")==true,
                "Whole-city checkpoint order or canonical identity is invalid");
            previous=tick;
            const auto manual=label=="final"?fs::path("whole-city-final.save.json"):fs::path("whole-city")/(label+".save.json");
            const auto roundtrip=fs::path("whole-city")/(label+".roundtrip.json");
            manuals.emplace_back(label,manual);
            l::check(row.at("file")==manual.generic_string()&&row.at("roundtrip_file")==roundtrip.generic_string(),
                "Whole-city checkpoint path is not the exact bounded artifact path");
            files.emplace_back(options.replay/manual);const auto& saved=files.back();
            l::check(row.at("sha256")==sha256(saved.bytes),"Prior whole-city checkpoint SHA256 differs");
            files.emplace_back(options.replay/roundtrip);
            l::check(row.at("roundtrip_sha256")==sha256(files.back().bytes)&&files.back().bytes==files[files.size()-2].bytes,
                "Prior whole-city roundtrip differs from its canonical checkpoint");
            if(label=="final")l::check(report.at("final_sha256")==row.at("sha256"),"Prior whole-city final SHA256 differs");
        }
        l::check(final&&maturity==!report.at("whole_city").at("maturity_tick").is_null()&&
            next_window-1==report.at("whole_city").at("complete_post_maturity_windows").size(),
            "Whole-city provenance lacks its exact maturity, window or final artifacts");
        recovery.emplace(options.replay,options.data,context,report.at("checkpoint_recovery"),manuals,true);
    }
    void finish(const fs::path& output,const Json& completed_report)const {
        if(!selected)return;
        recovery->unchanged();
        const PriorRecovery actual_recovery(output,data,context,completed_report.at("checkpoint_recovery"),manuals,true);
        for(const auto& file:files) {
            file.unchanged();const auto relative=fs::relative(file.path,files.front().path.parent_path());
            if(relative=="whole-city.json") {
                auto expected=report;auto actual=completed_report;
                expected.erase("separate_process_replay_exact");actual.erase("separate_process_replay_exact");
                expected["checkpoint_recovery"]=recovery->comparison;
                actual["checkpoint_recovery"]=actual_recovery.comparison;
                l::check(actual==expected,"Separate fresh0 whole-city canonical report differs");
            } else l::check(l::q::f::read_text(output/relative)==file.bytes,
                "Separate fresh0 whole-city canonical checkpoint bytes differ");
        }
        recovery->unchanged();actual_recovery.unchanged();
    }
};

struct WholeArtifacts {
    const Context& context;fs::path output;oe_checkpoint_recovery::Proof recovery;
    Json checkpoints=Json::array();
    WholeArtifacts(const Context& value,const fs::path& directory,const fs::path& data):
        context(value),output(directory),recovery(directory/"whole-city",data,sha256) {
        fs::create_directories(output/"whole-city");
    }
    void capture(const std::string& label,const sim::World& world) {
        l::check(checkpoints.size()<32,"Whole-city selected checkpoint allowance exceeded32");valid(world);
        const auto manual=label=="final"?fs::path("whole-city-final.save.json"):fs::path("whole-city")/(label+".save.json");
        const auto roundtrip=fs::path("whole-city")/(label+".roundtrip.json");
        save::write_save(output/manual,context.document(world),context.map.data,context.map.mask);
        const auto saved=save::read_save(output/manual);auto restored=context.restore(saved);
        l::check(restored.snapshot()==world.snapshot(),"Fresh whole-city checkpoint restore differs");
        save::write_save(output/roundtrip,context.document(restored),context.map.data,context.map.mask);
        const auto canonical=l::q::f::read_text(output/manual);const auto repeated=l::q::f::read_text(output/roundtrip);
        l::check(canonical==repeated,"Whole-city canonical manual roundtrip differs");
        recovery.capture(label,saved,context.map.mask,world,output/manual,
            [&](const auto& document){return context.restore(document);});
        checkpoints.push_back({{"label",label},{"tick",world.ticks()},{"file",manual.generic_string()},
            {"sha256",sha256(canonical)},{"roundtrip_file",roundtrip.generic_string()},
            {"roundtrip_sha256",sha256(repeated)},{"canonical_roundtrip_exact",true},{"fresh_restore_exact",true}});
    }
};

struct Result {
    Json events=Json::array(),refusals=Json::array(),early=nullptr;
    Json attempted_command=nullptr;
    std::map<unsigned,Id> ids;std::vector<bool> done=std::vector<bool>(recipe.size(),false);
    std::int64_t minimum_funds=12000;std::optional<std::uint64_t> construction_complete_tick;
    std::optional<Id> early_field;std::optional<sim::World> early_continuation;
    std::uint64_t early_capture_tick=0,early_continuation_ticks=0;bool early_return_exact=false;
};
Json cell_facts(const sim::World& world,sim::Cell cell) {
    Json result={{"cell",{cell.x,cell.y}},{"in_bounds",world.in_bounds(cell)}};
    if(!world.in_bounds(cell))return result;
    const auto owner=world.building_owner_at(cell);const auto permissions=world.map_permissions();
    const auto terrain=world.original_terrain_facts();
    result["object"]=static_cast<unsigned>(world.object_at(cell));
    result["building_owner"]=owner?Json(static_cast<unsigned>(*owner)):Json(nullptr);
    if(owner)result["owner_kind"]=static_cast<unsigned>(world.building(*owner).kind);
    result["road_allowed"]=permissions->road_allowed(cell);result["building_allowed"]=permissions->building_allowed(cell);
    result["protected_original"]=permissions->protected_original(cell);result["fixed_passage"]=permissions->fixed_passage(cell);
    result["height"]=permissions->cell_height(cell);result["raw_terrain"]=terrain->raw_terrain(cell);
    result["irrigation_boundary_allowed"]=terrain->irrigation_boundary_allowed(cell);
    result["ditch_overlay"]=world.irrigation_state()&&std::any_of(world.irrigation_state()->ditches.begin(),
        world.irrigation_state()->ditches.end(),[&](const auto& row){return row.cell==cell;});
    result["bridge_claim"]=world.bridge_at(cell)||world.bridge_bank_at(cell);
    result["ramp_claim"]=world.ramp_endpoint(cell)||world.source_cell_ramp_claim(cell);return result;
}
void failure_context(const char* stage,const sim::World& world,const Result& result,
                     const fs::path& output,const std::exception& error) {
    // Observe the failing authority in place. Never repair, retry, restore or
    // turn an exceptional command/tick into an accepted QA result.
    const auto path=output/"actual-creator-failure.json";
    if(!fs::is_directory(output)||fs::exists(path))return;
    Json hunters=Json::array(),fields=Json::array();
    if(world.hunting_state())for(const auto& hunter:world.hunting_state()->hunters) {
        Json route=Json::array();for(const auto cell:hunter.path)route.push_back({cell.x,cell.y});
        Json row={{"id",hunter.id.value},{"tent_id",static_cast<unsigned>(hunter.tent_id)},
            {"phase",static_cast<unsigned>(hunter.phase)},{"movement",static_cast<unsigned>(hunter.movement)},
            {"current",cell_facts(world,hunter.current)},{"path",route},{"path_vertex",hunter.path_vertex},
            {"edge_progress",hunter.edge_progress},{"work_progress",hunter.work_progress},
            {"cargo",hunter.cargo},{"reserved",hunter.reserved},{"route_revision",hunter.route_revision},
            {"launched_tick",hunter.launched_tick},{"return_requested",hunter.return_requested}};
        const auto& tent=world.building(hunter.tent_id);
        row["tent"]={{"cell",{tent.cell.x,tent.cell.y}},{"kind",static_cast<unsigned>(tent.kind)},
            {"placed",tent.placed},{"operating",tent.operating_enabled}};
        if(hunter.path_vertex+1<hunter.path.size()) {
            const auto next=hunter.path[hunter.path_vertex+1];row["next_path_cell"]=cell_facts(world,next);
            row["next_edge_blocker"]=static_cast<unsigned>(world.transport_edge_blocker(hunter.current,next));
        }
        hunters.push_back(std::move(row));
    }
    for(const auto& field:world.agriculture_fields())try{fields.push_back(field_work(world,field.field_id));}
        catch(const std::exception& detail){fields.push_back({{"field_id",static_cast<unsigned>(field.field_id)},{"observation_error",detail.what()}});}
    l::write(path,{{"format","openemperor-paid-creator-failure-context-v1"},{"stage",stage},{"error",error.what()},
        {"rule",world.rule_version()},{"tick",world.ticks()},{"command_sequence",world.command_sequence()},
        {"funds",world.treasury()},{"population",world.total_population()},
        {"attempted_command",result.attempted_command},{"actual_commands",result.events},{"hunters",hunters},{"fields",fields}});
}
template<class Action>void observed(const char* stage,const sim::World& world,const Result& result,
                                   const fs::path& output,Action action) {
    try{action();}catch(const std::exception& error){
        try{failure_context(stage,world,result,output,error);}catch(...){}throw;
    }
}
void purchases(sim::World& world,Result& result,const Prior& prior) {
    for(std::size_t i=0;i<recipe.size();++i) {
        if(result.done[i])continue;const auto& row=recipe[i];auto value=row.command;
        if(world.ticks()<row.not_before_tick)continue;
        if(world.total_population()<static_cast<int>(row.population_gate))continue;
        if(static_cast<unsigned>(value.building_id)) {
            const auto parent=result.ids.find(static_cast<unsigned>(value.building_id));
            if(parent==result.ids.end())continue;value.building_id=parent->second;
        }
        const auto price=world.construction_cost(value.type);
        if(price>0&&world.treasury()<price+(row.population_gate?world.workforce_required()*2+200:0))continue;
        result.attempted_command={{"recipe_index",i},{"tick",world.ticks()},{"command",encode(value)},
            {"price",price},{"funds_before",world.treasury()},{"population_gate",row.population_gate}};
        const auto decision=world.validate(value);
        if(!decision.accepted) {
            if(std::none_of(result.refusals.begin(),result.refusals.end(),[&](const auto& item){return item.at("recipe_index")==i;}))
                result.refusals.push_back({{"recipe_index",i},{"tick",world.ticks()},{"command",encode(value)},{"reason",decision.reason}});
            result.attempted_command=nullptr;
            continue;
        }
        const Json event={{"tick",world.ticks()},{"command",encode(value)}};prior.command(result.events.size(),event);
        const auto before_funds=world.treasury();const auto before_construction=world.construction_spent_total();
        const auto before_sequence=world.command_sequence();const auto applied=world.execute(value);
        l::check(applied.accepted&&applied.changed&&world.command_sequence()==before_sequence+1&&
            world.treasury()==before_funds-price&&world.construction_spent_total()==before_construction+static_cast<std::uint64_t>(price),
            "Ordinary paid command did not preserve exact price/sequence/history");
        if(result.early_continuation) {
            const auto repeated=result.early_continuation->execute(value);
            l::check(repeated.accepted&&repeated.changed,"Restored early checkpoint command diverged");
        }
        result.events.push_back(event);result.done[i]=true;result.minimum_funds=std::min(result.minimum_funds,world.treasury());
        if(row.body_key) {
            const auto id=world.building_owner_at(value.cell);const auto kind=sim::placed_object(value.type);
            l::check(id&&kind&&world.building(*id).kind==*kind&&result.ids.emplace(row.body_key,*id).second,
                "Paid recipe lost its actual monotone source/leaf identity");
        }
        result.attempted_command=nullptr;
    }
    if(!result.construction_complete_tick&&std::all_of(result.done.begin(),result.done.end(),[](bool value){return value;}))
        result.construction_complete_tick=world.ticks();
}
Json purchase_schedule(const Result& result) {
    Json scheduled=Json::array();
    for(std::size_t i=0;i<recipe.size();++i)if(recipe[i].not_before_tick) {
        const auto& row=recipe[i];auto actual=row.command;bool parent_bound=true;
        if(const auto parent=static_cast<unsigned>(actual.building_id)) {
            const auto found=result.ids.find(parent);parent_bound=found!=result.ids.end();
            if(parent_bound)actual.building_id=found->second;
        }
        const auto selected=encode(actual);
        const auto event=std::find_if(result.events.begin(),result.events.end(),[&](const auto& item){return parent_bound&&item.at("command")==selected;});
        if(event!=result.events.end())l::check(event->at("tick").template get<std::uint64_t>()>=row.not_before_tick,
            "Actual paid command predates the declared player purchase schedule");
        scheduled.push_back({{"recipe_index",i},{"authored_parent_key",static_cast<unsigned>(row.command.building_id)},
            {"command",parent_bound?selected:Json(nullptr)},{"not_before_tick",row.not_before_tick},
            {"population_gate",row.population_gate},{"actual_paid_tick",event==result.events.end()?Json(nullptr):event->at("tick")}});
    }
    return {{"scope","Authored player purchase timing using completed World ticks, not an Original runtime calendar claim or saved authority"},
        {"basis","The third Food Farm is bought no earlier than the existing World Soy growing slot4 opening (4*816=3264); four additional Rice/Cabbage fields are bought no earlier than completed tick4000. Actual paid parent, population and Funds gates remain mandatory; every execution tick is recorded."},
        {"commands",scheduled}};
}
void early_checkpoint(const Context& context,sim::World& world,Result& result,
                      oe_checkpoint_recovery::Proof& recovery,const fs::path& output) {
    if(result.early_field)return;
    for(const auto& farm:world.physical_food_growers_state()->farms)if(farm.latest_trip) {
        const auto& trip=*farm.latest_trip;
        if(!trip.harvested_tick||*trip.harvested_tick!=world.ticks()||trip.returned_tick||
            world.ticks()-world.building(trip.field_id).placed_tick>=80)continue;
        result.early_field=trip.field_id;result.early_capture_tick=world.ticks();
        result.early={{"format","openemperor-paid-early-irrigated-harvest-v1"},{"rule",context.rule},{"schema",context.schema},
            {"tick",world.ticks()},{"actual",field_work(world,trip.field_id)}};
        const auto& earned=result.early.at("actual");
        l::check(earned.at("physical_returned")==0&&earned.at("latest_trip").at("physical_cargo")==1&&
            earned.at("latest_trip").at("returned_tick").is_null(),
            "Early checkpoint must precede actual source return and stock credit");
        const auto base=earned.at("base_productive_ticks").get<std::uint64_t>();
        const auto bonus=earned.at("irrigation").at("bonus_work").get<std::uint64_t>();
        const auto waste=earned.at("seasonal").at("wasted_work").get<std::uint64_t>();
        l::check(base+bonus>=waste&&base+bonus-waste==80*earned.at("harvested").get<std::uint64_t>()&&
            earned.at("irrigation").at("wet_work_ticks").get<std::uint64_t>()/4==
                bonus+earned.at("irrigation").at("clipped_bonus").get<std::uint64_t>(),
            "Early harvest lacks its actual conserved base/wet/bonus/waste observations");
        const auto path=output/"early-harvest.save.json";save::write_save(path,context.document(world),context.map.data,context.map.mask);
        const auto document=save::read_save(path);result.early_continuation=context.restore(document);
        l::check(result.early_continuation->snapshot()==world.snapshot(),"Early manual save/restore differs");
        recovery.capture("earned-early-harvest",document,context.map.mask,world,path,
            [&](const auto& saved){return context.restore(saved);});
        return;
    }
}
void continue_early(const Context& context,sim::World& world,Result& result,
                    oe_checkpoint_recovery::Proof& recovery,const fs::path& output) {
    if(!result.early_continuation)return;
    result.early_continuation->tick();++result.early_continuation_ticks;
    l::check(result.early_continuation->snapshot()==world.snapshot(),"Early manual save real continuation differs");
    const auto row=std::find_if(world.physical_food_growers_state()->fields.begin(),world.physical_food_growers_state()->fields.end(),
        [&](const auto& item){return item.field_id==*result.early_field;});
    l::check(row!=world.physical_food_growers_state()->fields.end(),"Early Field return row vanished");
    if(row->returned&&!result.early.contains("first_source_return_tick")) {
        l::check(row->last_receipt&&row->last_receipt->grower_id.value==
            result.early.at("actual").at("latest_trip").at("grower_id").get<std::uint64_t>(),
            "Early earned harvest returned with a different physical identity");
        result.early["first_source_return_tick"]=row->last_receipt->returned_tick;
        const auto path=output/"early-return.save.json";
        save::write_save(path,context.document(world),context.map.data,context.map.mask);
        const auto saved=Json::parse(l::q::f::read_text(path));
        const auto& fields=saved.at("world").at("physical_food_growers").at("fields");
        const auto actual=std::find_if(fields.begin(),fields.end(),[&](const auto& item) {
            return item.at("field_id")==static_cast<unsigned>(*result.early_field);
        });
        l::check(actual!=fields.end()&&!actual->at("last_receipt").is_null(),"Actual first return DTO receipt missing");
        result.early["first_source_return_count"]=actual->at("returned");
        result.early["first_source_return_receipt"]=actual->at("last_receipt");
        recovery.capture("earned-early-source-return",save::read_save(path),context.map.mask,world,path,
            [&](const auto& document){return context.restore(document);});
    }
    if(result.early_continuation_ticks>=64&&result.early.contains("first_source_return_tick")) {
        result.early_return_exact=true;result.early["manual_save_return_continuation_exact"]=true;
        result.early["manual_save_continuation_ticks"]=result.early_continuation_ticks;result.early_continuation.reset();
    }
}
}

int main(int argc,char** argv) {
    fs::path output;std::vector<Input> sources;unsigned selected_rule=37;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto selected=options(argc,argv);selected_rule=selected.quality_aware_food_dispatch?38U:37U;
        sources.emplace_back(selected.data/"Model/EmperorBuildingModels.txt",64U*1024U*1024U);
        sources.emplace_back(selected.data/source_map,64U*1024U*1024U);
        Context context(selected.data,selected.quality_aware_food_dispatch);Prior prior(selected,context);WholePrior whole_prior(selected,context,prior);
        auto world=context.create();valid(world);
        l::check(world.ticks()==0&&world.command_sequence()==0&&world.treasury()==12000&&world.total_population()==0&&
            world.buildings().empty()&&world.couriers().empty()&&world.processed_food_state()->stocks.empty(),
            "Creator must begin with an empty, unearned World at tick0");
        const auto& rows=context.model->house_rules();
        l::check(rows.value(orig::ModelHouse::LuxuriousApartment,orig::HouseField::Drama)==0&&
            rows.value(orig::ModelHouse::LuxuriousApartment,orig::HouseField::Confucian)==0&&
            rows.value(orig::ModelHouse::HumbleCompound,orig::HouseField::Drama)==0&&
            rows.value(orig::ModelHouse::HumbleCompound,orig::HouseField::Confucian)==0,
            "Authored paid recipe omission must match the actual Common10/Elite15 source requirements");
        fs::create_directories(selected.output);output=selected.output;
        save::write_save(output/"initial.save.json",context.document(world),context.map.data,context.map.mask);
        oe_checkpoint_recovery::Proof recovery(output,selected.data);Result result;
        std::optional<oe_complete_city::Assessment> whole;std::optional<WholeArtifacts> whole_artifacts;
        if(selected.whole_city_ticks) {
            whole.emplace(oe_complete_city::Assessment::fresh_from_zero(world));
            whole_artifacts.emplace(context,output,selected.data);
        }
        const auto observe_whole=[&] {
            if(!whole)return;
            whole->after(world);
            const auto maturity=whole->maturity_tick();
            if(maturity&&*maturity==world.ticks())whole_artifacts->capture("maturity",world);
            else if(const auto window=whole->last_window_end_tick();window&&*window==world.ticks())
                whole_artifacts->capture("window-"+std::to_string(whole->complete_window_count()),world);
        };
        while(world.ticks()<selected.ticks) {
            observed("paid-purchases",world,result,output,[&]{purchases(world,result,prior);});const auto before_tick=world.ticks();
            observed("world-tick",world,result,output,[&]{try{world.tick();}catch(const std::exception& error) {
                Json fields=Json::array();for(const auto& field:world.agriculture_fields())fields.push_back(field_work(world,field.field_id));
                l::write(output/"actual-world-tick-failure.json",{{"tick_before",before_tick},{"tick_after_throw",world.ticks()},
                    {"error",error.what()},{"actual_fields",fields},{"actual_commands",result.events}});throw;
            }});
            observed("early-manual-continuation",world,result,output,[&]{continue_early(context,world,result,recovery,output);});
            result.minimum_funds=std::min(result.minimum_funds,world.treasury());
            observed("early-harvest-checkpoint",world,result,output,[&]{early_checkpoint(context,world,result,recovery,output);});
            observed("whole-city-from-zero-observation",world,result,output,observe_whole);
            if(world.ticks()%816==0)observed("periodic-world-validation",world,result,output,[&]{valid(world);});
        }
        observed("final-world-validation",world,result,output,[&]{valid(world);});
        l::check(fields_reachable(world),"Paid plan lacks an actual prepared Food-field route");
        observed("seed-save-publication",world,result,output,[&]{save::write_save(output/"seed.save.json",context.document(world),context.map.data,context.map.mask);});
        observed("seed-recovery",world,result,output,[&]{recovery.capture("paid-seed",save::read_save(output/"seed.save.json"),context.map.mask,world,output/"seed.save.json",
            [&](const auto& saved){return context.restore(saved);});});
        l::write(output/"player-plan.json",{{"format",plan_format},{"recipe",recipe_id},{"rule",context.rule},{"policy",3},{"schema",context.schema},
            {"map",source_map},{"initial_tick",0},{"end_tick",selected.ticks},{"purchase_schedule",purchase_schedule(result)},{"events",result.events}});
        l::write(output/"early-harvest.json",result.early);
        const auto seed_json=Json::parse(l::q::f::read_text(output/"seed.save.json"));const auto returns=source_returns(seed_json.at("world"));
        std::uint64_t fish=0,meat=0;for(const auto& row:world.fishing_state()->quays)fish+=row.unloaded;
        for(const auto& row:world.hunting_state()->tents)meat+=row.unloaded;
        const bool complete=result.construction_complete_tick.has_value();
        const bool returned=std::all_of(returns.begin(),returns.end(),[](const auto& row){return row.at("returned").template get<std::uint64_t>()>0;});
        const bool passed=complete&&returned&&fish&&meat&&result.minimum_funds>=0&&result.early_return_exact;
        if(prior.selected)l::check(passed,"Replay failed its independently earned creator requirements");
        prior.finish(output,result.events.size());for(const auto& source:sources)source.unchanged();
        Json artifacts=Json::object();for(const auto* name:{"initial.save.json","seed.save.json","player-plan.json","early-harvest.save.json","early-harvest.json","early-return.save.json"})
            if(fs::is_regular_file(output/name))artifacts[name]=sha256(l::q::f::read_text(output/name));
        Json homes=Json::array();for(const auto& row:world.original_economy_state()->houses)
            homes.push_back({{"id",static_cast<unsigned>(row.building_id)},{"type",static_cast<unsigned>(row.house.type)},
                {"occupants",row.house.occupants},{"food_quality",row.house.food_quality},{"influence",influence(world,row.building_id)}});
        l::write(output/"report.json",{{"format",creator_format},{"recipe",recipe_id},{"status",passed?"PASS":"FAIL"},
            {"rule",context.rule},{"policy",3},{"schema",context.schema},{"contract",context.rule==38?sim::quality_aware_food_dispatch_contract:sim::irrigated_harvest_contract},
            {"contract_revision",context.rule==38?sim::quality_aware_food_dispatch_revision:sim::irrigated_harvest_revision},{"map",source_map},
            {"model_sha256",context.model->provenance().sha256},{"map_sha256",context.terrain->source_map_sha256()},
            {"seed_tick",world.ticks()},{"construction_complete",complete},
            {"construction_complete_tick",result.construction_complete_tick?Json(*result.construction_complete_tick):Json(nullptr)},
            {"minimum_funds",result.minimum_funds},{"final_funds",world.treasury()},
            {"construction",world.construction_spent_total()},{"taxes",world.taxes_collected_total()},{"upkeep",world.maintenance_spent_total()},
            {"source_bytes_mode_mtime_unchanged",true},{"snapshot_stock_resident_level_or_arrival_injection",false},
            {"separate_process_replay_exact",prior.selected},{"source_returns",returns},{"fish_unloaded",fish},{"meat_unloaded",meat},
            {"bodies",world.buildings().size()},{"channels",channels(world)},{"installed_workers",world.workforce_required()},
            {"population",world.total_population()},{"paid_commands",result.events.size()},{"actual_refusals",result.refusals},
            {"purchase_schedule",purchase_schedule(result)},
            {"artifacts_sha256",artifacts},{"early_harvest",result.early},{"checkpoint_recovery",recovery.report()},{"houses",homes},
            {"complete_city_acceptance","Requires the separate read-only100k complete-city observer; creator PASS proves only this paid seed"}});
        l::check(passed,"Paid creator did not complete all commands/nine source returns/early Recovery/nonnegative Funds");
        if(whole) {
            // Continue the same paid primary World. Only this independent paired
            // continuation starts from its exact, production-validated seed;
            // the lifecycle assessment above retains its genuine tick0 history.
            auto paired=context.restore(save::read_save(output/"seed.save.json"));
            l::check(paired.snapshot()==world.snapshot(),"Whole-city seed restore differs before continuation");
            const auto end=std::uint64_t(selected.ticks)+selected.whole_city_ticks;
            while(world.ticks()<end) {
                observed("whole-city-primary-tick",world,result,output,[&]{world.tick();});
                observed("whole-city-independent-restored-tick",world,result,output,[&]{
                    paired.tick();l::check(paired.snapshot()==world.snapshot(),
                        "Independent restored whole-city continuation differs at an actual tick");
                });
                observed("whole-city-from-zero-observation",world,result,output,observe_whole);
                if(world.ticks()%816==0)observed("whole-city-periodic-validation",world,result,output,[&]{valid(world);});
            }
            l::check(world.command_sequence()==result.events.size(),"Whole-city continuation must execute no new player commands");
            observed("whole-city-final-checkpoint",world,result,output,[&]{whole_artifacts->capture("final",world);});
            const auto assessment=whole->report(world);const bool whole_passed=assessment.at("status")=="PASS";
            const auto final_bytes=l::q::f::read_text(output/"whole-city-final.save.json");
            for(const auto& source:sources)source.unchanged();prior.finish(output,result.events.size());
            const Json whole_report={{"format",whole_format},{"recipe",recipe_id},
                {"status",whole_passed?"PASS":"PARTIAL"},{"technical_status","PASS"},
                {"rule",context.rule},{"policy",3},{"schema",context.schema},{"map",source_map},
                {"model_sha256",context.model->provenance().sha256},{"map_sha256",context.terrain->source_map_sha256()},
                {"begin_tick",0},{"seed_tick",selected.ticks},{"end_tick",world.ticks()},
                {"continued_ticks",selected.whole_city_ticks},{"initial_sha256",artifacts.at("initial.save.json")},
                {"seed_sha256",artifacts.at("seed.save.json")},{"player_plan_sha256",artifacts.at("player-plan.json")},
                {"final_sha256",sha256(final_bytes)},{"whole_city",assessment},
                {"checkpoints",whole_artifacts->checkpoints},{"checkpoint_recovery",whole_artifacts->recovery.report()},
                {"fresh_restored_world_continuation_exact",true},{"separate_process_replay_exact",prior.selected},
                {"source_bytes_mode_mtime_unchanged",true},{"stock_population_model_or_arrival_injection",false},
                {"commands_after_seed",0}};
            observed("whole-city-independent-process-replay",world,result,output,[&]{whole_prior.finish(output,whole_report);});
            l::write(output/"whole-city.json",whole_report);
            std::cout<<(whole_passed?"PASS":"PARTIAL")<<": explicit fresh0 Rule"<<context.rule<<"/schema"<<context.schema<<" whole-city QA at tick"<<world.ticks()<<'\n';
            return whole_passed?0:2;
        }
        std::cout<<"PASS: explicitly earned Rule"<<context.rule<<"/schema"<<context.schema<<" paid seed at tick"<<world.ticks()<<'\n';return 0;
    } catch(const std::exception& error) {
        bool unchanged=true;for(const auto& source:sources)try{source.unchanged();}catch(...){unchanged=false;}
        if(!output.empty()&&!fs::exists(output/"report.json"))try {
            l::write(output/"report.json",{{"format",creator_format},{"status","FAIL"},{"rule",selected_rule},{"schema",selected_rule+16},
                {"reason",error.what()},{"source_bytes_mode_mtime_unchanged",unchanged}});
        }catch(...){}
        std::cerr<<error.what()<<'\n';return 1;
    }
}
