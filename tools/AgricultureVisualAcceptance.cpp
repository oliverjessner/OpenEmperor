// Actual paid Rule36 or explicitly selected Rule37 visual-state corpus.
// Only paid World commands, ordinary work and physical arrivals create evidence.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "FoodNativePlaytest.h"
#include "../tests/CheckpointRecoveryProof.h"
#include "simulation/original/IrrigationRules.h"
#include "assets/PlantationVisualProfile.h"
#include <charconv>
#include <set>
#include <limits>
namespace l=oe_luxury_progression;
namespace p=oe_food_playtest;
namespace {
using Id=sim::BuildingId;
struct Guard{fs::path path;std::string bytes;fs::perms mode;fs::file_time_type time;
    explicit Guard(fs::path f):path(std::move(f)),bytes(l::q::f::read_text(path)),mode(fs::status(path).permissions()),time(fs::last_write_time(path)){}
    void check()const{l::check(l::q::f::read_text(path)==bytes&&fs::status(path).permissions()==mode&&fs::last_write_time(path)==time,"Read-only supplied/replay input changed");}};
struct Context{l::q::Context map;std::shared_ptr<const orig::OriginalRulesModel> model;std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    bool irrigated_harvest=false,quality_aware_food_dispatch=false;
    Context(const fs::path& data,const fs::path& selected,bool irrigated=false,bool quality_aware=false):map(oe_longplay_fixture::real_map_context(data,selected)),model(l::save::prepare_food_processing_rules_model(data)),terrain(l::save::prepare_playable_hunting_terrain_facts(data,selected)),irrigated_harvest(irrigated),quality_aware_food_dispatch(quality_aware){}
    unsigned rule()const{return quality_aware_food_dispatch?38U:irrigated_harvest?37U:36U;}
    unsigned schema()const{return quality_aware_food_dispatch?54U:irrigated_harvest?53U:52U;}
    sim::World create()const{return quality_aware_food_dispatch?sim::World::create_quality_aware_food_dispatch(map.policy,model,terrain):irrigated_harvest?sim::World::create_irrigated_harvest(map.policy,model,terrain):sim::World::create_agriculture_reconfiguration(map.policy,model,terrain);}
    l::save::SaveDocument document(const sim::World& w)const{return l::save::make_document(map.data,map.map_relative,map.mask,w);}
    sim::World restore(const l::save::SaveDocument& d)const{l::check(d.source_schema_version==schema()&&d.world.rule_version==rule(),quality_aware_food_dispatch?"Exact opt-in Rule38/schema54 required":irrigated_harvest?"Exact Rule37/schema53 required":"Exact Rule36/schema52 required");return quality_aware_food_dispatch?l::save::restore_quality_aware_food_dispatch(d,map.data,map.mask,map.policy,model,terrain):irrigated_harvest?l::save::restore_irrigated_harvest(d,map.data,map.mask,map.policy,model,terrain):l::save::restore_agriculture_reconfiguration(d,map.data,map.mask,map.policy,model,terrain);}};
Json encode(const sim::Command& c){return {{"type",static_cast<unsigned>(c.type)},{"cell",{c.cell.x,c.cell.y}},{"owner",static_cast<unsigned>(c.building_id)},{"orientation",c.orientation},{"recipe_grade",c.recipe_grade},{"operating_enabled",c.operating_enabled},{"workforce_priority",static_cast<unsigned>(c.workforce_priority)}};}
sim::Command decode(const Json& j){sim::Command c{static_cast<sim::CommandType>(j.at("type").get<unsigned>()),{j.at("cell")[0].get<int>(),j.at("cell")[1].get<int>()}};c.building_id=static_cast<Id>(j.at("owner").get<unsigned>());c.orientation=j.at("orientation").get<std::uint8_t>();c.recipe_grade=j.at("recipe_grade").get<std::uint8_t>();c.operating_enabled=j.at("operating_enabled").get<bool>();c.workforce_priority=static_cast<sim::WorkforcePriority>(j.at("workforce_priority").get<unsigned>());return c;}
bool paid(sim::World& w,std::vector<sim::Command>& commands,sim::Command c){if(!w.validate(c).accepted||!w.execute(c).accepted)return false;commands.push_back(c);return true;}
bool field_routes(const sim::World& w) {
    const auto reachable=[&](sim::CourierId courier,Id target) {
        const auto& actor=w.courier(courier);
        const auto r=std::find_if(actor.dynamic_target_routes.begin(),actor.dynamic_target_routes.end(),[&](const auto& row){return row.first==target;});
        return r!=actor.dynamic_target_routes.end()&&r->second&&r->second->size()>1;
    };
    for(const auto& farm:w.physical_food_growers_state()->farms)for(const auto& field:w.agriculture_fields())
        if(field.farm_id==farm.farm_id&&w.building(field.field_id).placed&&!reachable(farm.courier_id,field.field_id))return false;
    for(const auto& source:w.plantations_state()->sources)for(const auto& plant:w.plantations_state()->plants)
        if(plant.source_id==source.building_id&&w.building(plant.plant_id).placed&&!reachable(source.courier_id,plant.plant_id))return false;
    for(const auto& source:w.seasonal_agriculture_state()->hemp_farms)for(const auto& farm:w.original_production_state()->hemp_farms)
        if(farm.farm_id==source.farm_id&&farm.field_id&&!reachable(source.courier_id,*farm.field_id))return false;
    return true;
}
bool road(const sim::World& w,sim::Cell c){return w.in_bounds(c)&&l::base::initial_road(w,c);}
bool connect(sim::World& w,std::vector<sim::Command>& commands,Id owner,Id market){
    if(w.find_building_route(owner,market))return true;const auto existing=w.snapshot().roads;std::vector<sim::Cell> goals;
    for(const auto c:w.building_cells(owner))for(const sim::Cell d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}){const sim::Cell at{c.x+d.x,c.y+d.y};if(road(w,at)||w.validate({sim::CommandType::PlaceRoad,at}).accepted)goals.push_back(at);}
    for(const auto goal:goals){auto starts=existing;std::stable_sort(starts.begin(),starts.end(),[&](auto a,auto b){return p::distance(a,goal)<p::distance(b,goal);});if(starts.size()>8)starts.resize(8);
        for(const auto start:starts)for(const auto& path:p::road_options(start,goal)){if(path.empty()||path.size()>128)continue;auto probe=w;auto next=commands;bool legal=true;for(const auto c:path)if(!road(probe,c)&&!paid(probe,next,{sim::CommandType::PlaceRoad,c})){legal=false;break;}if(legal&&probe.find_building_route(owner,market)){w=std::move(probe);commands=std::move(next);return true;}}}return false;
}
std::pair<Id,Id> plan_plant(sim::World& w,std::vector<sim::Command>& commands,Id market,Id farm,unsigned type){
    constexpr std::array sources{sim::CommandType::PlaceOriginalTeaCuringShed,sim::CommandType::PlaceOriginalLacquerRefinery,sim::CommandType::PlaceOriginalSilkwormShed};
    constexpr std::array plants{sim::CommandType::PlaceOriginalTeaBush,sim::CommandType::PlaceOriginalLacquerTree,sim::CommandType::PlaceOriginalMulberryTree};
    const auto origin=w.building(farm).cell;std::vector<sim::Cell> candidates;
    for(int y=origin.y-18;y<=origin.y+18;++y)for(int x=origin.x-18;x<=origin.x+18;++x){const sim::Cell c{x,y};if(w.validate({sources.at(type),c}).accepted)candidates.push_back(c);}
    std::stable_sort(candidates.begin(),candidates.end(),[&](auto a,auto b){return p::distance(a,origin)<p::distance(b,origin);});if(candidates.size()>80)candidates.resize(80);
    for(const auto at:candidates){auto probe=w;auto next=commands;if(!paid(probe,next,{sources.at(type),at}))continue;const auto source=*probe.building_owner_at(at);if(!connect(probe,next,source,market))continue;
        for(int y=at.y-3;y<=at.y+4;++y)for(int x=at.x-3;x<=at.x+4;++x){sim::Command c{plants.at(type),{x,y}};c.building_id=source;if(!probe.validate(c).accepted)continue;auto positive=probe;auto chosen=next;if(!paid(positive,chosen,c))continue;const auto plant=*positive.building_owner_at(c.cell);bool reachable=false;
            for(const auto& a:positive.couriers())if(a.owner==source&&a.role==sim::CourierRole::OriginalPlantationGrower)for(const auto& r:a.dynamic_target_routes)if(r.first==plant&&r.second&&r.second->size()>1)reachable=true;
            if(reachable&&field_routes(positive)){w=std::move(positive);commands=std::move(chosen);return {source,plant};}}}
    throw std::runtime_error("Finite paid source-bound Tea source/plant planner failed");
}
struct Water{Id pump{};sim::Cell cell{};std::uint8_t orientation=0;std::vector<sim::Cell> ditches;};
Water plan_water(sim::World& w,std::vector<sim::Command>& commands,Id market,Id farm){
    struct Candidate{sim::Cell cell;std::uint8_t orientation;};std::vector<Candidate> candidates;const auto origin=w.building(farm).cell;
    for(int y=0;y+2<w.height();++y)for(int x=0;x+2<w.width();++x)for(std::uint8_t orientation=0;orientation<4;++orientation){sim::Command c{sim::CommandType::PlaceIrrigationPump,{x,y}};c.orientation=orientation;const auto shape=orig::pump_geometry(c.cell,orientation);if(std::any_of(shape.water.begin(),shape.water.end(),[&](auto at){return !w.in_bounds(at)||!w.map_permissions()->bridge_water(at);}))continue;if(w.validate(c).accepted)candidates.push_back({c.cell,orientation});}
    std::stable_sort(candidates.begin(),candidates.end(),[&](auto a,auto b){return p::distance(a.cell,origin)<p::distance(b.cell,origin);});if(candidates.size()>64)candidates.resize(64);
    std::vector<sim::Cell> goals;for(const auto& f:w.agriculture_fields())if(f.farm_id==farm)for(int dy=-5;dy<=5;++dy)for(int dx=-5;dx<=5;++dx){const sim::Cell c{w.building(f.field_id).cell.x+dx,w.building(f.field_id).cell.y+dy};if(orig::field_in_irrigation_range(w.building(f.field_id).cell,c)&&w.validate({sim::CommandType::PlaceIrrigationDitch,c}).accepted)goals.push_back(c);}
    std::sort(goals.begin(),goals.end(),[](auto a,auto b){return a.y!=b.y?a.y<b.y:a.x<b.x;});goals.erase(std::unique(goals.begin(),goals.end()),goals.end());
    for(const auto candidate:candidates){auto probe=w;auto next=commands;sim::Command pump{sim::CommandType::PlaceIrrigationPump,candidate.cell};pump.orientation=candidate.orientation;if(!paid(probe,next,pump))continue;const auto id=*probe.building_owner_at(candidate.cell);if(!connect(probe,next,id,market))continue;
        const auto shape=orig::pump_geometry(candidate.cell,candidate.orientation);auto ends=goals;std::stable_sort(ends.begin(),ends.end(),[&](auto a,auto b){return p::distance(a,candidate.cell)<p::distance(b,candidate.cell);});if(ends.size()>32)ends.resize(32);
        for(const auto start:shape.seeds)for(const auto end:ends)for(const auto& path:p::road_options(start,end)){if(path.empty()||path.size()>128)continue;auto positive=probe;auto chosen=next;bool legal=true;for(const auto c:path)if(!paid(positive,chosen,{sim::CommandType::PlaceIrrigationDitch,c})){legal=false;break;}if(legal&&field_routes(positive)){w=std::move(positive);commands=std::move(chosen);return {id,candidate.cell,candidate.orientation,path};}}}
    throw std::runtime_error("Finite complete-shore Pump/ditch planner failed");
}
// Output filenames are a tool capability boundary, independent of save schema.
// Never let untrusted replay metadata choose paths outside the fresh corpus.
bool checkpoint_label_valid(std::string_view label) {
    return !label.empty()&&label.size()<=96&&std::all_of(label.begin(),label.end(),[](char c){
        return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';
    });
}
void validate_replay_checkpoint_labels(const Json& events) {
    l::check(events.is_array(),"Replay events must be an array");std::set<std::string> labels;
    for(const auto& event:events)if(event.at("type")=="checkpoint") {
        const auto label=event.at("label").get<std::string>();
        l::check(checkpoint_label_valid(label),"Checkpoint label must be a bounded ASCII leaf identifier");
        l::check(labels.insert(label).second,"Duplicate replay checkpoint label");
    }
}
bool absent_checkpoint_path(const fs::path& path) {
    std::error_code error;const auto status=fs::symlink_status(path,error);
    return error==std::errc::no_such_file_or_directory||(!error&&status.type()==fs::file_type::not_found);
}
constexpr std::array<const char*,9> crop_names{"Wheat","Millet","Rice","Cabbage","SoyBeanCurd","Hemp","Tea","Lacquer","Mulberry"};
struct Scene {
    Id farm{},second_farm{},market{},mill{},shop{},warehouse{},hemp{},hemp_field{},recovery_watch{};
    std::array<Id,3> sources{},plants{};
    Water water;
    std::vector<Id> watches,staffing;
};
Json scene_json(const Scene& s) {
    Json ditches=Json::array();for(const auto c:s.water.ditches)ditches.push_back({c.x,c.y});
    return {{"farm",static_cast<unsigned>(s.farm)},{"second_farm",static_cast<unsigned>(s.second_farm)},
        {"market",static_cast<unsigned>(s.market)},{"mill",static_cast<unsigned>(s.mill)},
        {"shop",static_cast<unsigned>(s.shop)},{"warehouse",static_cast<unsigned>(s.warehouse)},
        {"hemp",static_cast<unsigned>(s.hemp)},{"hemp_field",static_cast<unsigned>(s.hemp_field)},
        {"sources",s.sources},{"plants",s.plants},{"watches",s.watches},{"staffing",s.staffing},
        {"recovery_watch",static_cast<unsigned>(s.recovery_watch)},{"pump",static_cast<unsigned>(s.water.pump)},
        {"pump_cell",{s.water.cell.x,s.water.cell.y}},{"orientation",s.water.orientation},{"ditches",ditches}};
}
Scene scene_from(const Json& j) {
    Scene s;
    const auto id=[&](const char* name){return static_cast<Id>(j.at(name).get<unsigned>());};
    s.farm=id("farm");s.second_farm=id("second_farm");s.market=id("market");s.mill=id("mill");
    s.shop=id("shop");s.warehouse=id("warehouse");s.hemp=id("hemp");s.hemp_field=id("hemp_field");
    s.recovery_watch=id("recovery_watch");s.water.pump=id("pump");
    s.water.cell={j.at("pump_cell")[0].get<int>(),j.at("pump_cell")[1].get<int>()};
    s.water.orientation=j.at("orientation").get<std::uint8_t>();
    for(std::size_t i=0;i<3;++i){s.sources[i]=static_cast<Id>(j.at("sources")[i].get<unsigned>());s.plants[i]=static_cast<Id>(j.at("plants")[i].get<unsigned>());}
    for(const auto& v:j.at("watches"))s.watches.push_back(static_cast<Id>(v.get<unsigned>()));
    for(const auto& v:j.at("staffing"))s.staffing.push_back(static_cast<Id>(v.get<unsigned>()));
    for(const auto& v:j.at("ditches"))s.water.ditches.push_back({v[0].get<int>(),v[1].get<int>()});
    return s;
}
std::vector<sim::Cell> candidates(const sim::World& w,sim::Cell origin,int radius=24) {
    std::vector<sim::Cell> cells;
    for(int y=origin.y-radius;y<=origin.y+radius;++y)for(int x=origin.x-radius;x<=origin.x+radius;++x)
        if(w.in_bounds({x,y}))cells.push_back({x,y});
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return p::distance(a,origin)<p::distance(b,origin);});
    return cells;
}
Id plan_body(sim::World& w,std::vector<sim::Command>& commands,Id market,sim::CommandType type,sim::Cell center) {
    unsigned tried=0;
    for(const auto cell:candidates(w,center)) {
        if(!w.validate({type,cell}).accepted)continue;
        if(++tried>96)break;
        auto next=w;auto paid_commands=commands;
        if(!paid(next,paid_commands,{type,cell}))continue;
        const auto id=*next.building_owner_at(cell);
        if(connect(next,paid_commands,id,market)&&field_routes(next)) {
            w=std::move(next);commands=std::move(paid_commands);return id;
        }
    }
    throw std::runtime_error("Finite actual paid body planner failed for command "+std::to_string(static_cast<unsigned>(type)));
}
Id plan_field(sim::World& w,std::vector<sim::Command>& commands,Id farm,sim::CommandType type) {
    const auto at=w.building(farm).cell;
    for(const auto cell:candidates(w,at,5)) {
        sim::Command c{type,cell};c.building_id=farm;
        if(!w.validate(c).accepted)continue;
        auto next=w;auto list=commands;
        if(paid(next,list,c)&&field_routes(next)) {
            const auto field=*next.building_owner_at(cell);w=std::move(next);commands=std::move(list);return field;
        }
    }
    throw std::runtime_error("Finite actual paid source/Field placement failed");
}
void valid(const sim::World& w) {
    l::check((w.rule_version()==36||w.rule_version()==37||w.rule_version()==38)&&w.processed_food_state()&&!w.hunting_food_state()&&!w.fishing_food_state()&&!w.original_agriculture_state(),"Visual proof duplicated the sole Food authority");
    l::check(w.population_valid()&&w.city_economy_valid()&&w.production_balance_valid()&&w.navigation_valid()&&w.fire_state_valid(),"Paid visual city broke a World invariant");
    const auto channels=w.couriers().size()+w.fishing_state()->quays.size()+w.hunting_state()->tents.size()+3*w.hunting_state()->habitats.size();
    l::check(w.buildings().size()<=82&&channels<=58,"Visual proof exceeded lifetime82/58 limits");
}
Json sample(const sim::World& w,const Scene& scene) {
    valid(w);
    Json crops=Json::array(),workers=Json::array(),boats=Json::array(),hunters=Json::array(),animals=Json::array(),transfers=Json::array();
    std::array<std::uint64_t,9> returned{};
    for(const auto& f:w.agriculture_fields()) {
        const auto i=static_cast<std::size_t>(f.crop);
        const auto work=std::find_if(w.multi_field_agriculture_state()->fields.begin(),w.multi_field_agriculture_state()->fields.end(),[&](const auto& a){return a.field_id==f.field_id;});
        const auto receipt=std::find_if(w.physical_food_growers_state()->fields.begin(),w.physical_food_growers_state()->fields.end(),[&](const auto& a){return a.field_id==f.field_id;});
        const auto seasonal=std::find_if(w.seasonal_agriculture_state()->fields.begin(),w.seasonal_agriculture_state()->fields.end(),[&](const auto& a){return a.field_id==f.field_id;});
        l::check(work!=w.multi_field_agriculture_state()->fields.end()&&receipt!=w.physical_food_growers_state()->fields.end()&&seasonal!=w.seasonal_agriculture_state()->fields.end(),"Lost actual Field histories");
        returned[i]+=receipt->returned;
        crops.push_back({static_cast<unsigned>(f.field_id),i,static_cast<unsigned>(f.farm_id),w.agriculture_field_progress(f.field_id),work->productive_ticks,f.harvested,receipt->returned,seasonal->harvest_progress,seasonal->ready_tick?Json(*seasonal->ready_tick):Json(nullptr),w.irrigation_field_status(w.building(f.field_id).cell).water_available});
    }
    for(const auto& f:w.seasonal_agriculture_state()->hemp_farms) {
        const auto origin=std::find_if(w.original_production_state()->hemp_farms.begin(),w.original_production_state()->hemp_farms.end(),[&](const auto& a){return a.farm_id==f.farm_id;});
        l::check(origin!=w.original_production_state()->hemp_farms.end()&&origin->field_id,"Lost paid Hemp field");returned[5]+=origin->harvested;
        crops.push_back({static_cast<unsigned>(*origin->field_id),5,static_cast<unsigned>(f.farm_id),w.building(f.farm_id).progress,f.productive_ticks,f.picked,origin->harvested,0,f.ready_tick?Json(*f.ready_tick):Json(nullptr),w.irrigation_field_status(w.building(*origin->field_id).cell).water_available});
    }
    for(const auto& f:w.plantations_state()->plants) {
        const auto where=std::find(scene.plants.begin(),scene.plants.end(),f.plant_id);l::check(where!=scene.plants.end(),"Unexpected material plant in paid visual city");
        const auto i=6+static_cast<std::size_t>(where-scene.plants.begin());returned[i]+=f.returned;
        const auto seasonal=std::find_if(w.seasonal_agriculture_state()->plants.begin(),w.seasonal_agriculture_state()->plants.end(),[&](const auto& a){return a.plant_id==f.plant_id;});
        l::check(seasonal!=w.seasonal_agriculture_state()->plants.end(),"Lost material seasonal history");
        crops.push_back({static_cast<unsigned>(f.plant_id),i,static_cast<unsigned>(f.source_id),f.growth,f.productive_ticks,f.harvested,f.returned,0,seasonal->ready_tick?Json(*seasonal->ready_tick):Json(nullptr),w.irrigation_field_status(w.building(f.plant_id).cell).water_available});
    }
    for(const auto& f:w.physical_food_growers_state()->farms)if(f.latest_trip) {
        const auto& t=*f.latest_trip;const auto& c=w.courier(f.courier_id);
        workers.push_back({static_cast<unsigned>(f.courier_id),static_cast<unsigned>(f.farm_id),static_cast<unsigned>(t.field_id),t.grower_id.value,static_cast<unsigned>(t.phase),t.work_ticks,c.edge_progress,sim::physical_food_trip_cargo(t),t.returned_tick?Json(*t.returned_tick):Json(nullptr)});
    }
    for(const auto& b:w.fishing_state()->boats)boats.push_back({b.id.value,static_cast<unsigned>(b.quay_id),static_cast<unsigned>(b.phase),b.current.x,b.current.y,b.edge_progress,b.work_progress,b.cargo});
    for(const auto& h:w.hunting_state()->hunters)hunters.push_back({h.id.value,static_cast<unsigned>(h.tent_id),static_cast<unsigned>(h.phase),h.current.x,h.current.y,h.edge_progress,h.work_progress,h.cargo,h.killed_tick?Json(*h.killed_tick):Json(nullptr)});
    for(const auto& a:w.hunting_state()->animals)animals.push_back({a.id.value,static_cast<unsigned>(a.species),a.current.x,a.current.y,a.edge_progress,a.next?Json::array({a.next->x,a.next->y}):Json(nullptr)});
    for(const auto& t:w.processed_food_state()->trips) {
        int cargo=0;for(std::size_t g=0;g<12;++g)cargo+=t.loaded[g]-t.delivered[g]-t.returned[g];
        if(cargo)transfers.push_back({static_cast<unsigned>(t.transfer.courier_id),t.transfer.sequence,static_cast<unsigned>(t.source),static_cast<unsigned>(t.target),cargo});
    }
    const auto status=w.irrigation_pump_status(scene.water.pump);
    return {{"tick",w.ticks()},{"funds",w.treasury()},{"population",w.total_population()},{"required_workers",w.active_workforce_required()},
        {"crops",crops},{"returned",returned},{"workers",workers},{"boats",boats},{"hunters",hunters},{"animals",animals},{"transfers",transfers},
        {"pump",{{"id",static_cast<unsigned>(scene.water.pump)},{"burning",status.burning},{"supplying",status.supplying},{"phase_tick",status.last_phase_tick?Json(*status.last_phase_tick):Json(nullptr)}}}};
}
struct Options {fs::path data,output,seed,replay,map="Cities/Chengdu.map";unsigned ticks=24000;bool checkpoint_recovery=false,irrigated_harvest=false,quality_aware_food_dispatch=false;};
constexpr std::array recovery_labels{"paid-city","food-grower-work-earned","food-grower-loaded","crop-Tea-growing",
    "boat-loaded-moving","hunter-loaded-moving","mill-processing","warehouse-stock","warehouse-outbound",
    "market-stock-receipt","market-house-receipt","shop-house-receipt","pump-fire","pump-extinguished","pump-recovered-wet","final"};
constexpr const char* usage="openemperor-agriculture-visual-acceptance --data ABS --output FRESH_PRIVATE_SUBDIR [--seed-plan MAC10_PAID_COMMAND_PLAN | --replay CREATOR_DIR] [--map Cities/Chengdu.map] [--ticks 24000] [--checkpoint-recovery] [--irrigated-harvest | --quality-aware-food-dispatch] (default Rule36/schema52: .local/agri-mac12; explicit Rule37/schema53 or Rule38/schema54: .local/agri-mac14)";
Options options(int argc,char** argv) {
    Options o;std::map<std::string,std::string> args;
    for(int n=1;n<argc;++n){const std::string key=argv[n];if(key=="--checkpoint-recovery"){l::check(!o.checkpoint_recovery,"Duplicate argument");o.checkpoint_recovery=true;continue;}if(key=="--quality-aware-food-dispatch"){l::check(!o.quality_aware_food_dispatch,"Duplicate argument");o.quality_aware_food_dispatch=true;continue;}if(key=="--irrigated-harvest"){l::check(!o.irrigated_harvest,"Duplicate argument");o.irrigated_harvest=true;continue;}l::check(n+1<argc,usage);l::check(key=="--data"||key=="--output"||key=="--seed-plan"||key=="--replay"||key=="--map"||key=="--ticks",usage);l::check(args.emplace(key,argv[++n]).second,"Duplicate argument");}
    l::check(args.contains("--data")&&args.contains("--output")&&(args.contains("--seed-plan")!=args.contains("--replay")),usage);l::no_symlinks(args.at("--data"));o.data=fs::canonical(args.at("--data"));
    l::check(fs::path(args.at("--data")).is_absolute()&&fs::path(args.at("--output")).is_absolute(),"Explicit absolute data/output paths required");
    if(args.contains("--map"))o.map=args.at("--map");l::check(!o.map.is_absolute()&&o.map==o.map.lexically_normal()&&o.map.parent_path()=="Cities"&&o.map.extension()==".map","Explicit supplied Cities map required");l::no_symlinks(o.data/o.map);
    if(args.contains("--ticks")){const auto& s=args.at("--ticks");const auto r=std::from_chars(s.data(),s.data()+s.size(),o.ticks);l::check(r.ec==std::errc{}&&r.ptr==s.data()+s.size(),"Invalid tick count");}l::check(o.ticks>=12000&&o.ticks<=50000,"Require12000..50000 actual ticks");
    l::check(!(o.irrigated_harvest&&o.quality_aware_food_dispatch),"Choose one explicit Agriculture successor");o.output=p::output_root(args.at("--output"),o.data,(o.irrigated_harvest||o.quality_aware_food_dispatch)?".local/agri-mac14":".local/agri-mac12");
    if(args.contains("--seed-plan")){l::no_symlinks(args.at("--seed-plan"));o.seed=fs::canonical(args.at("--seed-plan"));}
    if(args.contains("--replay")){l::no_symlinks(args.at("--replay"));o.replay=fs::canonical(args.at("--replay"));}return o;
}
} // namespace

int main(int argc,char** argv) {
    fs::path output;std::uint64_t completed=0;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto o=options(argc,argv);output=o.output;Guard model(o.data/"Model/EmperorBuildingModels.txt"),map(o.data/o.map);Context context(o.data,o.map,o.irrigated_harvest,o.quality_aware_food_dispatch);auto world=context.create();Scene scene;
        std::optional<sim::World> restored;Json events=Json::array(),checkpoints=Json::array(),coverage=Json::object();std::vector<Guard> inputs;
        std::unique_ptr<oe_checkpoint_recovery::Proof> recovery;
        if(o.checkpoint_recovery)recovery=std::make_unique<oe_checkpoint_recovery::Proof>(output,o.data);
        Json replay_plan;std::size_t event_at=0;std::set<std::string> checkpoint_labels;
        const auto apply=[&](const sim::Command& c,const std::string& reason){const auto result=world.execute(c);l::check(result.accepted,reason+": "+result.reason);if(restored)l::check(restored->execute(c).accepted&&restored->snapshot()==world.snapshot(),"Restored paid command diverged");events.push_back({{"type","command"},{"tick",world.ticks()},{"command",encode(c)},{"reason",reason}});};
        const auto capture=[&](const std::string& label,const std::string& domain,std::uint64_t id,const std::string& state){
            l::check(checkpoint_label_valid(label),"Checkpoint label must be a bounded ASCII leaf identifier");
            l::check(checkpoint_labels.insert(label).second,"Duplicate visual checkpoint label");
            if(recovery&&(label=="paid-city"||label=="final"))
                l::check(world.ticks()==(label=="paid-city"?0:o.ticks)&&domain=="Building"&&id==static_cast<unsigned>(scene.farm)&&state==(label=="paid-city"?"initial":"final"),
                    "Event Recovery initial/final checkpoint differs from its required epoch/identity");
            const auto path=output/(label+".save.json"),roundtrip=output/(label+".roundtrip.json");
            l::check(absent_checkpoint_path(path)&&absent_checkpoint_path(roundtrip),"Both visual checkpoint targets must be fresh");
            const auto document=context.document(world);l::save::write_save(path,document,context.map.data,context.map.mask);const auto loaded=l::save::read_save(path);auto fresh=context.restore(loaded);l::check(fresh.snapshot()==world.snapshot(),"Visual checkpoint restore changed authority");
            l::save::write_save(roundtrip,context.document(fresh),context.map.data,context.map.mask);l::check(l::q::f::read_text(path)==l::q::f::read_text(roundtrip),"Visual checkpoint canonical bytes differ");
            if(recovery) {
                if(label.starts_with("year-boundary-")||std::find(recovery_labels.begin(),recovery_labels.end(),label)!=recovery_labels.end())
                    recovery->capture(label,loaded,context.map.mask,world,path,[&](const auto& saved){return context.restore(saved);});
            }
            if(!restored)restored=std::move(fresh);
            const Json item{{"label",label},{"domain",domain},{"id",id},{"state",state},{"tick",world.ticks()},{"file",path.filename().string()},{"actual",sample(world,scene)}};
            checkpoints.push_back(item);events.push_back({{"type","checkpoint"},{"tick",world.ticks()},{"label",label},{"domain",domain},{"id",id},{"state",state}});
        };
        const auto mark=[&](const std::string& label,const std::string& domain,std::uint64_t id,const std::string& state){if(coverage.contains(label))return;coverage[label]={{"tick",world.ticks()},{"domain",domain},{"id",id},{"state",state}};if(o.replay.empty())capture(label,domain,id,state);};
        if(!o.replay.empty()) {
            for(const char* name:{"plan.json","final.save.json","work.jsonl"}){l::no_symlinks(o.replay/name);inputs.emplace_back(o.replay/name);}
            replay_plan=Json::parse(inputs.front().bytes);l::check(replay_plan.at("format")=="openemperor-agriculture-visual-plan-v1"&&replay_plan.at("events").size()<=2048,"Bounded exact visual replay plan required");
            const bool explicit_generation=replay_plan.contains("rule")||replay_plan.contains("schema")||replay_plan.contains("irrigated_harvest")||replay_plan.contains("irrigated_harvest_revision")||replay_plan.contains("quality_aware_food_dispatch")||replay_plan.contains("quality_aware_food_dispatch_revision");
            l::check((o.irrigated_harvest||o.quality_aware_food_dispatch)==explicit_generation,"Rule37 visual replay requires the creator's explicit --irrigated-harvest selection");
            l::check(o.quality_aware_food_dispatch==(replay_plan.contains("quality_aware_food_dispatch")||replay_plan.contains("quality_aware_food_dispatch_revision")),
                "Rule38 visual replay requires its explicit --quality-aware-food-dispatch selection");
            if(o.quality_aware_food_dispatch)l::check(replay_plan.at("quality_aware_food_dispatch")==sim::quality_aware_food_dispatch_contract&&
                replay_plan.at("quality_aware_food_dispatch_revision")==sim::quality_aware_food_dispatch_revision,"Unknown Rule38 visual replay contract");
            if(o.irrigated_harvest||o.quality_aware_food_dispatch)l::check(replay_plan.at("rule")==context.rule()&&replay_plan.at("policy")==3&&replay_plan.at("schema")==context.schema()&&
                replay_plan.at("irrigated_harvest")==sim::irrigated_harvest_contract&&replay_plan.at("irrigated_harvest_revision")==sim::irrigated_harvest_revision,
                "Unknown Rule37 visual replay generation contract");
            l::check(o.checkpoint_recovery==replay_plan.contains("checkpoint_recovery"),"Event Recovery replay requires the creator's explicit --checkpoint-recovery selection");
            if(o.checkpoint_recovery)l::check(replay_plan.at("checkpoint_recovery")==oe_checkpoint_recovery::contract,"Unknown event Recovery QA contract");
            validate_replay_checkpoint_labels(replay_plan.at("events"));scene=scene_from(replay_plan.at("scene"));
        } else {
            inputs.emplace_back(o.seed);const auto seed=Json::parse(inputs.back().bytes);l::check(seed.at("format")=="openemperor-food-warehouses-plan-v1"&&seed.at("commands").size()<=512,"Normal paid MAC10 command plan required, never a save");
            std::map<unsigned,Id> actual_ids;unsigned old_id=0,watch_purchases=0;
            for(const auto& encoded:seed.at("commands")) {
                auto command=decode(encoded);const auto object=sim::placed_object(command.type);
                if(object)++old_id;
                // A new city may reserve its second bounded Watch for recovery.
                // No paid identity is removed or reused: this purchase never occurs.
                if(object==sim::Object::FireWatch&&++watch_purchases==2)continue;
                if(static_cast<unsigned>(command.building_id))command.building_id=actual_ids.at(static_cast<unsigned>(command.building_id));
                apply(command,o.quality_aware_food_dispatch?"Fresh actual Rule38 paid construction with a reserved recovery Watch slot":o.irrigated_harvest?"Fresh actual Rule37 paid construction with a reserved recovery Watch slot":"Fresh actual Rule36 paid construction with a reserved recovery Watch slot");
                if(object)actual_ids.emplace(old_id,*world.building_owner_at(command.cell));
            }
            l::check(watch_purchases==2,"Expected the normal two-Watch paid seed plan");
            const auto seed_id=[&](const char* name){return actual_ids.at(seed.at(name).get<unsigned>());};
            scene.farm=seed_id("farm");scene.market=seed_id("market");scene.mill=seed_id("mill");scene.shop=seed_id("shop");scene.warehouse=seed_id("warehouse");
            l::check(!world.ticks()&&!world.total_population(),"Paid seed imported residents or elapsed time");
            auto probe=world;std::vector<sim::Command> additions;
            scene.second_farm=plan_body(probe,additions,scene.market,sim::CommandType::PlaceOriginalFoodFarm,world.building(scene.farm).cell);
            plan_field(probe,additions,scene.second_farm,sim::CommandType::PlaceOriginalSoybeanField);
            scene.hemp=plan_body(probe,additions,scene.market,sim::CommandType::PlaceOriginalHempFarm,world.building(scene.farm).cell);
            scene.hemp_field=plan_field(probe,additions,scene.hemp,sim::CommandType::PlaceOriginalHempField);
            for(unsigned kind=0;kind<3;++kind){const auto [source,plant]=plan_plant(probe,additions,scene.market,scene.farm,kind);scene.sources[kind]=source;scene.plants[kind]=plant;}
            scene.water=plan_water(probe,additions,scene.market,scene.farm);
            scene.recovery_watch=plan_body(probe,additions,scene.market,sim::CommandType::PlaceFireWatch,scene.water.cell);
            for(const auto& command:additions)apply(command,"Normal source-admitted paid visual city extension");
            for(const auto& b:world.buildings())if(b.kind==sim::Object::FireWatch)scene.watches.push_back(b.id);
            for(const char* name:{"market","mill","shop","second_market"})apply(sim::set_building_operation(seed_id(name),true),"Open actual finite Food outlets");
            apply(sim::set_building_operation(seed_id("second_mill"),false),"Keep redundant paid Mill paused while new crop sources earn staff");
            apply(sim::set_building_operation(scene.recovery_watch,false),"Reserve an honestly paid recovery Watch without early protection");
            // Transactional World commands replace its storage. Retain values,
            // never Building references or span iterators across execute().
            std::vector<std::pair<Id,sim::Object>> staffing_controls;
            for(const auto& b:world.buildings())staffing_controls.emplace_back(b.id,b.kind);
            for(const auto [id,kind]:staffing_controls)if(world.workforce_required(id)>0) {
                const bool essential=id==scene.market||kind==sim::Object::OriginalWaterWell72||kind==sim::Object::OriginalAncestorShrine214||(kind==sim::Object::FireWatch&&id!=scene.recovery_watch);
                apply(sim::set_building_operation(id,essential),"Stage actual staff demand behind real resident admission");
                if(essential)apply(sim::set_building_workforce_priority(id,sim::WorkforcePriority::High),"Keep real Water, Market, Ancestor and fire staffing before new production");
            }
            for(const auto& q:world.fishing_state()->quays)scene.staffing.push_back(q.quay_id);
            scene.staffing.insert(scene.staffing.end(),{scene.farm,scene.water.pump,scene.warehouse,scene.hemp,scene.shop,scene.mill});
            for(const auto& t:world.hunting_state()->tents)scene.staffing.push_back(t.tent_id);
            scene.staffing.insert(scene.staffing.end(),{scene.sources[0],scene.sources[1],scene.sources[2],scene.second_farm});
            capture("paid-city","Building",static_cast<unsigned>(scene.farm),"initial");
        }
        std::ofstream trace(output/"work.jsonl");l::check(bool(trace),"Cannot create visual state trace");
        bool watches_paused=false,recovery_started=false,extinguished=false;std::optional<std::uint64_t> fire_tick,extinguished_tick;std::int64_t minimum_funds=std::numeric_limits<std::int64_t>::max();
        const auto observe=[&](){
            const auto current=sample(world,scene);
            for(const auto& b:world.buildings())if(b.placed&&(b.kind==sim::Object::OriginalFoodFarm||b.kind==sim::Object::OriginalHempFarm||b.kind==sim::Object::OriginalFishingQuay||b.kind==sim::Object::OriginalHuntersTent||b.kind==sim::Object::OriginalMill53||b.kind==sim::Object::OriginalFoodShop66||b.kind==sim::Object::Warehouse||b.kind==sim::Object::OriginalIrrigationPump||b.kind==sim::Object::OriginalTeaCuringShed237||b.kind==sim::Object::OriginalLacquerRefinery238||b.kind==sim::Object::OriginalSilkwormShed239))
                mark("body-"+std::to_string(static_cast<unsigned>(b.id))+"-source","Building",static_cast<unsigned>(b.id),"source");
            for(const auto& row:current.at("crops")) {
                const auto index=row[1].get<std::size_t>();const auto progress=row[3].get<unsigned>();const auto harvested=row[5].get<std::uint64_t>();const auto id=row[0].get<unsigned>();const std::string name=crop_names.at(index);
                if(progress==0&&harvested==0)mark("crop-"+name+"-initial","Building",id,"initial");
                if(progress>0&&progress<80)mark("crop-"+name+"-growing","Building",id,"growing");
                if(progress>0&&progress<80&&harvested>0)mark("crop-"+name+"-regrowing","Building",id,"regrowing");
                // Lacquer's qualified final image first exists at actual ready80.
                // Keep that frame distinct from pickup; never invent it at79.
                if(progress<80||(index>=6&&progress==80)) {
                    const auto band=index<6 ? oe::assets::authored_farm_progress_stage(static_cast<int>(progress)):
                        oe::assets::plantation_growth_visual_stage(static_cast<std::uint16_t>(26+index-6),progress);
                    l::check(band.has_value(),"Invalid actual growth sampling band");
                    mark("crop-"+name+"-growth-band-"+std::to_string(*band),"Building",id,"growth-band");
                }
                if(progress==80)mark("crop-"+name+"-ready","Building",id,"ready");
                if(progress==0&&harvested>0)mark("crop-"+name+"-picked","Building",id,"picked");
                if(row[6].get<std::uint64_t>()>0)mark("crop-"+name+"-returned","Building",id,"returned");
                if(row[7].get<unsigned>()>0)mark("crop-"+name+"-harvesting","Building",id,"harvesting");
            }
            for(const auto& f:world.physical_food_growers_state()->farms)if(f.latest_trip&&!f.latest_trip->returned_tick) {
                const auto& t=*f.latest_trip;const auto& c=world.courier(f.courier_id);const auto name=std::string("food-grower-")+std::to_string(static_cast<unsigned>(t.phase));
                mark(name,"Courier",static_cast<unsigned>(f.courier_id),"phase-"+std::to_string(static_cast<unsigned>(t.phase)));
                if(o.checkpoint_recovery&&t.phase==sim::PhysicalFoodGrowerPhase::Working&&t.work_ticks>0)
                    mark("food-grower-work-earned","Courier",static_cast<unsigned>(f.courier_id),"actual-work");
                if(sim::physical_food_trip_cargo(t)&&c.edge_progress)mark("food-grower-loaded","Courier",static_cast<unsigned>(f.courier_id),"loaded");
            }
            for(const auto& f:world.seasonal_agriculture_state()->hemp_farms)if(f.latest_trip&&sim::seasonal_hemp_cargo(*f.latest_trip)&&world.courier(f.courier_id).edge_progress)mark("hemp-grower-loaded","Courier",static_cast<unsigned>(f.courier_id),"loaded");
            for(const auto& s:world.plantations_state()->sources)if(s.latest_trip&&s.latest_trip->picked_tick&&!s.latest_trip->returned_tick&&world.courier(s.courier_id).edge_progress)mark("plantation-grower-loaded","Courier",static_cast<unsigned>(s.courier_id),"loaded");
            for(const auto& c:world.couriers())if(c.edge_progress&&c.path_vertex+1<c.path.size()&&(c.role==sim::CourierRole::OriginalFoodGrower||c.role==sim::CourierRole::OriginalSeasonalHempGrower||c.role==sim::CourierRole::OriginalPlantationGrower||c.role==sim::CourierRole::OriginalFoodToMarket||c.role==sim::CourierRole::OriginalFoodToHouse||c.role==sim::CourierRole::OriginalMealsToShop||c.role==sim::CourierRole::OriginalMealsToHouse||c.role==sim::CourierRole::OriginalWarehouseFood)) {
                const auto a=c.path[c.path_vertex],b=c.path[c.path_vertex+1];
                mark("direction-"+std::to_string(b.x-a.x)+"-"+std::to_string(b.y-a.y),"Courier",static_cast<unsigned>(c.id),"moving-direction");
            }
            for(const auto& b:world.fishing_state()->boats)if(b.edge_progress)mark("boat-moving","Boat",b.id.value,"moving");
            for(const auto& h:world.hunting_state()->hunters)if(h.edge_progress)mark("hunter-moving","Hunter",h.id.value,"moving");
            for(const auto& b:world.fishing_state()->boats)if(static_cast<unsigned>(b.phase)<2||b.cargo>0)mark("boat-"+std::to_string(static_cast<unsigned>(b.phase)),"Boat",b.id.value,sim::fishing_boat_phase_name(b.phase));
            for(const auto& h:world.hunting_state()->hunters){if(static_cast<unsigned>(h.phase)<2||h.cargo>0)mark("hunter-"+std::to_string(static_cast<unsigned>(h.phase)),"Hunter",h.id.value,sim::hunting_hunter_phase_name(h.phase));if(h.killed_tick&&*h.killed_tick==world.ticks())mark("hunter-kill","Hunter",h.id.value,"killed");}
            if(o.checkpoint_recovery) {
                for(const auto& boat:world.fishing_state()->boats)if(boat.cargo>0&&boat.edge_progress>0)
                    mark("boat-loaded-moving","Boat",boat.id.value,"loaded-moving");
                for(const auto& hunter:world.hunting_state()->hunters)if(hunter.cargo>0&&hunter.edge_progress>0)
                    mark("hunter-loaded-moving","Hunter",hunter.id.value,"loaded-moving");
                for(const auto& market:world.food_warehouses_state()->markets)for(const auto& receipt:market.first_received)
                    if(receipt&&receipt->tick==world.ticks()&&receipt->amount>0)
                        mark("market-stock-receipt","Building",static_cast<unsigned>(market.market_id),"actual-receipt");
                for(const auto& stock:world.processed_food_state()->stocks)if(sim::original_house_kind(world.building(stock.building_id).kind)&&
                    stock.quality_witness&&stock.quality_witness->tick==world.ticks()) {
                    const auto role=world.courier(stock.quality_witness->transfer.courier_id).role;
                    if(role==sim::CourierRole::OriginalFoodToHouse)mark("market-house-receipt","Building",static_cast<unsigned>(stock.building_id),"actual-receipt");
                    if(role==sim::CourierRole::OriginalMealsToHouse)mark("shop-house-receipt","Building",static_cast<unsigned>(stock.building_id),"actual-receipt");
                }
                if(world.ticks()>0&&world.ticks()%sim::seasonal_year_ticks==0)
                    mark("year-boundary-"+std::to_string(world.ticks()),"Building",static_cast<unsigned>(scene.farm),"year-boundary");
                // Replay earns the natural-fire epochs from World facts too;
                // checkpoint labels in its input plan are not event evidence.
                if(!o.replay.empty()) {
                    const auto returned=current.at("returned").get<std::array<std::uint64_t,9>>();
                    if(std::all_of(returned.begin(),returned.end(),[](auto n){return n>0;})&&world.building_on_fire(scene.water.pump))
                        mark("pump-fire","Building",static_cast<unsigned>(scene.water.pump),"burning");
                    if(coverage.contains("pump-fire")) {
                        const auto burning_tick=coverage.at("pump-fire").at("tick").get<std::uint64_t>();
                        for(const auto& arrival:world.original_production_state()->fire_protection->arrivals)
                            if(arrival.trip.target==scene.water.pump&&arrival.extinguished&&arrival.tick==world.ticks()&&arrival.tick>=burning_tick)
                                mark("pump-extinguished","Building",static_cast<unsigned>(scene.water.pump),"extinguished");
                    }
                    if(coverage.contains("pump-extinguished")&&world.ticks()>coverage.at("pump-extinguished").at("tick").get<std::uint64_t>()&&world.irrigation_pump_status(scene.water.pump).supplying)
                        mark("pump-recovered-wet","Building",static_cast<unsigned>(scene.water.pump),"wet");
                }
            }
            for(const auto& a:world.hunting_state()->animals)mark("animal-"+std::to_string(static_cast<unsigned>(a.species))+(a.edge_progress?"-moving":"-standing"),"Animal",a.id.value,a.edge_progress?"moving":"standing");
            for(const auto& m:world.food_processing_state()->mills)if(m.active&&m.active->work>0)mark("mill-processing","Building",static_cast<unsigned>(m.mill_id),"processing");
            for(const auto& t:world.processed_food_state()->trips) {
                const auto& c=world.courier(t.transfer.courier_id);if(world.original_food_cargo(c.id)<=0||!c.edge_progress)continue;
                mark("food-courier-loaded","Courier",static_cast<unsigned>(c.id),"loaded");
                if(world.building(t.target).kind==sim::Object::Warehouse)mark("warehouse-inbound","Courier",static_cast<unsigned>(c.id),"loaded");
                if(c.role==sim::CourierRole::OriginalWarehouseFood)mark("warehouse-outbound","Courier",static_cast<unsigned>(c.id),"loaded");
                if(c.role==sim::CourierRole::OriginalMealsToHouse)mark("shop-meals-loaded","Courier",static_cast<unsigned>(c.id),"loaded");
            }
            for(const auto& s:world.processed_food_state()->stocks)if(s.building_id==scene.warehouse&&std::any_of(s.amounts.begin(),s.amounts.end(),[](auto n){return n>0;}))mark("warehouse-stock","Building",static_cast<unsigned>(scene.warehouse),"stocked");
            const auto pump=world.irrigation_pump_status(scene.water.pump);mark(pump.supplying?"water-wet":"water-dry","Building",static_cast<unsigned>(scene.water.pump),pump.supplying?"wet":"dry");
        };
        while(world.ticks()<=o.ticks) {
            if(!o.replay.empty()) {
                const auto& recorded=replay_plan.at("events");while(event_at<recorded.size()&&recorded[event_at].at("tick").get<std::uint64_t>()==world.ticks()) {
                    const auto& event=recorded[event_at++];if(event.at("type")=="command")apply(decode(event.at("command")),event.at("reason").get<std::string>());
                    else if(event.at("type")=="checkpoint")capture(event.at("label").get<std::string>(),event.at("domain").get<std::string>(),event.at("id").get<std::uint64_t>(),event.at("state").get<std::string>());
                    else throw std::runtime_error("Unsupported visual replay event");
                }
            } else {
                for(auto it=scene.staffing.rbegin();it!=scene.staffing.rend()&&world.active_workforce_required()>world.total_population();++it)
                    if(world.building(*it).operating_enabled)apply(sim::set_building_operation(*it,false),"Reverse real staffing commitments after actual population loss");
                for(const auto id:scene.staffing)if(!world.building(id).operating_enabled) {
                    if(world.active_workforce_required()+world.workforce_required(id)>world.total_population())break;
                    apply(sim::set_building_operation(id,true),"Enable only when actual admitted residents cover full model staff");
                }
                const auto current=sample(world,scene);const auto counts=current.at("returned").get<std::array<std::uint64_t,9>>();
                if(!watches_paused&&std::all_of(counts.begin(),counts.end(),[](auto n){return n>0;})) {
                    for(const auto id:scene.watches)apply(sim::set_building_operation(id,false),"Wait for genuine natural Pump fire after all nine crops physically returned");watches_paused=true;
                }
                if(watches_paused&&!recovery_started&&world.building_on_fire(scene.water.pump)) {
                    fire_tick=world.ticks();
                    for(const auto id:scene.watches)apply(sim::set_building_operation(id,true),"Restart paid Watch; only a physical Inspector arrival may extinguish");
                    l::check(world.building_on_fire(scene.water.pump),"Watch restart passively extinguished the Pump");
                    // Capture the same post-command epoch that the tick trace observes.
                    mark("pump-fire","Building",static_cast<unsigned>(scene.water.pump),"burning");recovery_started=true;
                }
                if(recovery_started&&!extinguished)for(const auto& arrival:world.original_production_state()->fire_protection->arrivals)if(arrival.trip.target==scene.water.pump&&arrival.extinguished&&arrival.tick>=*fire_tick) {
                    extinguished=true;extinguished_tick=arrival.tick;mark("pump-extinguished","Building",static_cast<unsigned>(scene.water.pump),"extinguished");break;
                }
                if(extinguished&&world.ticks()>*extinguished_tick&&world.irrigation_pump_status(scene.water.pump).supplying)mark("pump-recovered-wet","Building",static_cast<unsigned>(scene.water.pump),"wet");
            }
            observe();minimum_funds=std::min(minimum_funds,world.treasury());trace<<sample(world,scene).dump()<<'\n';l::check(bool(trace),"Visual physical trace write failed");
            if(world.ticks()==o.ticks)break;
            world.tick();if(restored){restored->tick();l::check(restored->snapshot()==world.snapshot(),"Fresh restored visual city continuation diverged");}completed=world.ticks();
        }
        trace.close();if(o.replay.empty())capture("final","Building",static_cast<unsigned>(scene.farm),"final");
        else {
            l::check(event_at==replay_plan.at("events").size()&&events==replay_plan.at("events"),"Separate process did not replay every paid command/checkpoint");
            // Preserve the historical report path; opt-in event Recovery uses
            // the independent World observations above instead of plan labels.
            if(!o.checkpoint_recovery)for(const auto& item:checkpoints)if(item.at("label")=="pump-fire"||item.at("label")=="pump-extinguished"||item.at("label")=="pump-recovered-wet")coverage[item.at("label").get<std::string>()]={{"tick",item.at("tick")},{"domain",item.at("domain")},{"id",item.at("id")},{"state",item.at("state")}};
            l::check(l::q::f::read_text(output/"final.save.json")==inputs[1].bytes&&l::q::f::read_text(output/"work.jsonl")==inputs[2].bytes,"Separate process final canonical bytes or complete trace differ");
        }
        std::vector<std::string> required;
        for(const auto* name:crop_names)for(const auto* phase:{"initial","growing","ready","picked","returned","regrowing"})required.push_back(std::string("crop-")+name+"-"+phase);
        for(std::size_t kind=0;kind<9;++kind)for(unsigned band=0;band<(kind==6?4U:6U);++band)required.push_back(std::string("crop-")+crop_names[kind]+"-growth-band-"+std::to_string(band));
        for(std::size_t kind=0;kind<5;++kind)required.push_back(std::string("crop-")+crop_names[kind]+"-harvesting");
        for(const auto* direction:{"direction-0-1","direction-0--1","direction-1-0","direction--1-0","boat-moving","hunter-moving"})required.emplace_back(direction);
        for(unsigned phase=1;phase<=4;++phase)required.push_back("food-grower-"+std::to_string(phase));
        for(unsigned phase=0;phase<4;++phase){required.push_back("boat-"+std::to_string(phase));required.push_back("hunter-"+std::to_string(phase));}
        for(const unsigned species:{75U,76U,77U})for(const auto* state:{"moving","standing"})required.push_back("animal-"+std::to_string(species)+"-"+state);
        for(const char* label:{"food-grower-loaded","hemp-grower-loaded","plantation-grower-loaded","hunter-kill","mill-processing","food-courier-loaded","warehouse-inbound","warehouse-outbound","warehouse-stock","shop-meals-loaded","water-dry","water-wet","pump-fire","pump-extinguished","pump-recovered-wet"})required.emplace_back(label);
        if(o.checkpoint_recovery)for(const char* label:{"food-grower-work-earned","boat-loaded-moving","hunter-loaded-moving","market-stock-receipt","market-house-receipt","shop-house-receipt","year-boundary-9792"})required.emplace_back(label);
        if(recovery) {
          const auto recovery_report=recovery->report();
          std::set<std::string> published;
          for(const auto& event:recovery_report.at("verified_events"))published.insert(event.at("label").get<std::string>());
          for(const char* label:recovery_labels)
            l::check(published.contains(label),"Required event Recovery checkpoint was not published");
          for(std::uint64_t tick=sim::seasonal_year_ticks;tick<=o.ticks;tick+=sim::seasonal_year_ticks)
            l::check(published.contains("year-boundary-"+std::to_string(tick)),"Required year-boundary Recovery checkpoint was not published");
          for(const auto& event:recovery_report.at("verified_events")) {
            const auto label=event.at("label").get<std::string>();
            if(label=="paid-city"||label=="final")continue;
            const auto saved=std::find_if(checkpoints.begin(),checkpoints.end(),[&](const auto& row){return row.at("label")==label;});
            l::check(saved!=checkpoints.end()&&coverage.contains(label),"Selected event Recovery checkpoint lacks independently earned coverage");
            const auto& actual=coverage.at(label);
            l::check(saved->at("tick")==actual.at("tick")&&saved->at("domain")==actual.at("domain")&&
                saved->at("id")==actual.at("id")&&saved->at("state")==actual.at("state"),
                "Event Recovery checkpoint differs from independently earned epoch/identity");
          }
        }
        Json missing=Json::array();for(const auto& label:required)if(!coverage.contains(label))missing.push_back(label);
        const bool positive=missing.empty()&&minimum_funds>=0;for(const auto& input:inputs)input.check();model.check();map.check();
        Json plan={{"format","openemperor-agriculture-visual-plan-v1"},{"scene",scene_json(scene)},{"events",events}};
        Json report={{"format","openemperor-agriculture-visual-acceptance-v1"},{"status",positive?"PASS":"PARTIAL"},{"rule",context.rule()},{"schema",context.schema()},{"real_ticks",completed},{"minimum_funds",minimum_funds},{"coverage",coverage},{"missing",missing},{"checkpoints",checkpoints},{"final",sample(world,scene)},{"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",!o.replay.empty()},{"source_bytes_mode_mtime_unchanged",true},{"snapshot_stock_or_population_injection",false},{"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"}};
        if(o.irrigated_harvest||o.quality_aware_food_dispatch) {
            for(auto* value:{&plan,&report}) {
                (*value)["policy"]=3;
                (*value)["irrigated_harvest"]=sim::irrigated_harvest_contract;
                (*value)["irrigated_harvest_revision"]=sim::irrigated_harvest_revision;
            }
            plan["rule"]=context.rule();plan["schema"]=context.schema();
        }
        if(o.quality_aware_food_dispatch)for(auto* value:{&plan,&report}) {
            (*value)["quality_aware_food_dispatch"]=sim::quality_aware_food_dispatch_contract;
            (*value)["quality_aware_food_dispatch_revision"]=sim::quality_aware_food_dispatch_revision;
        }
        if(recovery){plan["checkpoint_recovery"]=oe_checkpoint_recovery::contract;report["checkpoint_recovery"]=recovery->report();}
        l::write(output/"plan.json",plan);l::write(output/"report.json",report);
        std::cout<<(positive?"PASS":"PARTIAL")<<" paid Agriculture visual corpus ticks="<<completed<<" missing="<<missing.dump()<<'\n';return positive?0:2;
    } catch(const std::exception& e) {
        if(!output.empty()&&!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",e.what()},{"last_completed_tick",completed}});}catch(...){}
        std::cerr<<"Agriculture visual acceptance FAIL: "<<e.what()<<'\n';return 1;
    }
}
