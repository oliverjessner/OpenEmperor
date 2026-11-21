// Paid supplied-data proof. No fixture constructor or edited Original model is used.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "FoodNativePlaytest.h"
#include <charconv>
#include <set>

namespace l=oe_luxury_progression;
namespace p=oe_food_playtest;
namespace {
using Id=sim::BuildingId;
struct Guard {
    fs::path path;std::string bytes;fs::perms mode;fs::file_time_type modified;
    explicit Guard(fs::path file):path(std::move(file)),bytes(l::q::f::read_text(path)),mode(fs::status(path).permissions()),modified(fs::last_write_time(path)){}
    void unchanged()const{l::check(l::q::f::read_text(path)==bytes&&fs::status(path).permissions()==mode&&fs::last_write_time(path)==modified,"Supplied/replay input changed");}
};
struct Context {
    l::q::Context map;std::shared_ptr<const orig::OriginalRulesModel> model;std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
    Context(const fs::path& data,const fs::path& selected):map(oe_longplay_fixture::real_map_context(data,selected)),
        model(l::save::prepare_food_processing_rules_model(data)),terrain(l::save::prepare_playable_hunting_terrain_facts(data,selected)){}
    sim::World create()const{return sim::World::create_food_processing(map.policy,model,terrain);}
    l::save::SaveDocument document(const sim::World& world)const{return l::save::make_document(map.data,map.map_relative,map.mask,world);}
    sim::World restore(const l::save::SaveDocument& value)const{
        l::check(value.source_schema_version==50&&value.world.rule_version==34&&value.map_relative==map.map_relative,"Replay requires exact Rule34/schema50 and selected map");
        return l::save::restore_food_processing(value,map.data,map.mask,map.policy,model,terrain);
    }
};
const sim::MillProcessing& mill(const sim::World& world,Id id){for(const auto& row:world.food_processing_state()->mills)if(row.mill_id==id)return row;throw std::runtime_error("Paid Mill missing");}
bool roads(const sim::World& world,sim::Cell cell){return world.in_bounds(cell)&&l::base::initial_road(world,cell);}
bool fields_reachable(const sim::World& world){
    for(const auto& source:world.physical_food_growers_state()->farms){const auto& channel=world.courier(source.courier_id);
        for(const auto& field:world.agriculture_fields())if(field.farm_id==source.farm_id){const auto at=std::find_if(channel.dynamic_target_routes.begin(),channel.dynamic_target_routes.end(),[&](const auto& row){return row.first==field.field_id;});
            if(at==channel.dynamic_target_routes.end()||!at->second||at->second->size()<2)return false;}}
    return true;
}
bool connect(sim::World& world,p::Plan& plan,Id owner){
    if(world.find_building_route(owner,plan.market))return true;
    std::vector<sim::Cell> goals;
    for(const auto cell:world.building_cells(owner))for(const sim::Cell d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}){
        const sim::Cell c{cell.x+d.x,cell.y+d.y};if(roads(world,c)||world.validate({sim::CommandType::PlaceRoad,c}).accepted)goals.push_back(c);}
    for(const auto goal:goals){auto starts=plan.roads;std::stable_sort(starts.begin(),starts.end(),[&](auto a,auto b){return p::distance(a,goal)<p::distance(b,goal);});if(starts.size()>12)starts.resize(12);
        for(const auto start:starts)for(const auto& path:p::road_options(start,goal)){
            if(path.empty()||path.size()>192)continue;auto next=world;auto paid=plan;bool legal=true;
            for(const auto c:path)if(!roads(next,c)&&!p::buy(next,paid,{sim::CommandType::PlaceRoad,c})){legal=false;break;}
            if(legal&&next.find_building_route(owner,paid.market)&&fields_reachable(next)){world=std::move(next);plan=std::move(paid);return true;}}}
    return false;
}
bool outside_field_domain(const sim::World& world,Id farm,sim::Object kind,sim::Cell cell){const auto origin=world.building(farm).cell;
    for(const auto c:sim::building_footprint_cells(world.profile(),world.rule_version(),kind,cell)) {
        if(c.x>=origin.x-3&&c.x<=origin.x+5&&c.y>=origin.y-3&&c.y<=origin.y+5)return false;
    }
    return true;
}
std::vector<sim::Cell> nearby(const sim::World& world,const p::Plan& plan,int margin=8){
    std::vector<sim::Cell> cells;for(const auto road:plan.roads)for(int y=-margin;y<=margin;++y)for(int x=-margin;x<=margin;++x){const sim::Cell c{road.x+x,road.y+y};if(world.in_bounds(c))cells.push_back(c);}
    std::sort(cells.begin(),cells.end(),[](auto a,auto b){return a.y!=b.y?a.y<b.y:a.x<b.x;});cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
    const auto distance=[&](sim::Cell c){int best=10000;for(const auto road:plan.roads)best=std::min(best,p::distance(road,c));return best;};
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return distance(a)<distance(b);});return cells;
}
Id add_body(sim::World& world,p::Plan& plan,sim::CommandType type){const auto kind=*sim::placed_object(type);unsigned attempts=0;
    for(const auto cell:nearby(world,plan)){
        if(!outside_field_domain(world,plan.farm,kind,cell)||!world.validate({type,cell}).accepted)continue;
        if(++attempts>80)break;auto next=world;auto paid=plan;if(!p::buy(next,paid,{type,cell}))continue;const auto owner=*next.building_owner_at(cell);
        if(connect(next,paid,owner)&&fields_reachable(next)){world=std::move(next);plan=std::move(paid);return owner;}}
    throw std::runtime_error("Finite paid body planning failed for command "+std::to_string(static_cast<unsigned>(type)));
}
struct Scene{p::Plan paid;Id mill{},shop{},tent{};};
Scene choose_scene(const sim::World& base,const fs::path& map){
    auto layout=p::city_blueprints().front();std::vector<sim::Cell> origins;
    if(map=="Cities/Chengdu.map")origins.push_back({107,134});else origins=p::clear_origins(base,layout);
    if(origins.size()>48)origins.resize(48);
    constexpr std::array crop_commands{sim::CommandType::PlaceOriginalWheatField,sim::CommandType::PlaceOriginalMilletField,sim::CommandType::PlaceOriginalRiceField,sim::CommandType::PlaceOriginalCabbageField,sim::CommandType::PlaceOriginalSoybeanField};
    for(const auto origin:origins){auto world=base;Scene scene;if(!p::city(world,scene.paid,layout,origin))continue;
        const auto farm=world.building(scene.paid.farm).cell;std::vector<sim::Cell> cells;
        for(int y=farm.y-3;y<=farm.y+5;++y)for(int x=farm.x-3;x<=farm.x+5;++x)cells.push_back({x,y});
        std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return p::distance(a,farm)<p::distance(b,farm);});bool complete=true;
        for(const unsigned crop:{2U,0U,3U,4U}){bool found=false;for(const auto cell:cells){sim::Command cmd{crop_commands[crop],cell};cmd.building_id=scene.paid.farm;if(world.validate(cmd).accepted&&p::buy(world,scene.paid,cmd)){found=true;break;}}complete&=found;}
        if(!complete||!fields_reachable(world))continue;
        try{scene.shop=add_body(world,scene.paid,sim::CommandType::PlaceOriginalFoodShop);scene.mill=add_body(world,scene.paid,sim::CommandType::PlaceOriginalMill);scene.tent=add_body(world,scene.paid,sim::CommandType::PlaceOriginalHuntersTent);}catch(const std::exception&){continue;}
        struct Quay{sim::Cell cell;std::uint8_t orientation;};std::vector<Quay> candidates;
        for(int y=1;y+2<world.height();++y)for(int x=1;x+2<world.width();++x)for(std::uint8_t o=0;o<4;++o){sim::Command cmd{sim::CommandType::PlaceOriginalFishingQuay,{x,y}};cmd.orientation=o;if(world.validate(cmd).accepted)candidates.push_back({{x,y},o});}
        std::stable_sort(candidates.begin(),candidates.end(),[&](auto a,auto b){return p::distance(a.cell,farm)<p::distance(b.cell,farm);});unsigned attempts=0;
        for(const auto option:candidates){if(++attempts>64)break;if(!outside_field_domain(world,scene.paid.farm,sim::Object::OriginalFishingQuay,option.cell))continue;
            auto next=world;auto paid=scene.paid;sim::Command cmd{sim::CommandType::PlaceOriginalFishingQuay,option.cell};cmd.orientation=option.orientation;if(!p::buy(next,paid,cmd))continue;
            const auto owner=*next.building_owner_at(option.cell);const auto dock=orig::fishing_quay_geometry(option.cell,option.orientation).dock;bool reachable=false;
            for(const auto& site:next.fishing_state()->sites)if(site.cell!=dock&&next.find_fishing_route(dock,site.cell,owner)){reachable=true;break;}
            if(reachable&&connect(next,paid,owner)&&fields_reachable(next)){scene.paid=std::move(paid);scene.paid.quay=owner;scene.paid.quay_cell=option.cell;scene.paid.orientation=option.orientation;
                l::check(p::buy(next,scene.paid,sim::set_mill_recipe(scene.mill,1)),"Initial paid recipe selection refused");return scene;}}
    }
    throw std::runtime_error("Finite actual-map five-field/Mill/Shop/Tent/Quay planner found no fully paid legal city");
}
Json encode(const sim::Command& c){return {{"type",static_cast<unsigned>(c.type)},{"cell",{c.cell.x,c.cell.y}},{"owner",static_cast<unsigned>(c.building_id)},
    {"orientation",c.orientation},{"recipe_grade",c.recipe_grade},{"operating_enabled",c.operating_enabled},{"workforce_priority",static_cast<unsigned>(c.workforce_priority)}};}
sim::Command decode(const Json& j){sim::Command c{static_cast<sim::CommandType>(j.at("type").get<unsigned>()),{j.at("cell")[0].get<int>(),j.at("cell")[1].get<int>()}};
    c.building_id=static_cast<Id>(j.at("owner").get<unsigned>());c.orientation=j.at("orientation").get<std::uint8_t>();c.recipe_grade=j.at("recipe_grade").get<std::uint8_t>();
    c.operating_enabled=j.at("operating_enabled").get<bool>();c.workforce_priority=static_cast<sim::WorkforcePriority>(j.at("workforce_priority").get<unsigned>());return c;}
Json encode_scene(const Scene& s){Json commands=Json::array();for(const auto& c:s.paid.commands)commands.push_back(encode(c));return {{"format","openemperor-food-processing-plan-v1"},
    {"commands",commands},{"mill",static_cast<unsigned>(s.mill)},{"shop",static_cast<unsigned>(s.shop)},{"farm",static_cast<unsigned>(s.paid.farm)},
    {"market",static_cast<unsigned>(s.paid.market)},{"tent",static_cast<unsigned>(s.tent)},{"quay",static_cast<unsigned>(s.paid.quay)}};}
Scene decode_scene(const Json& j){l::check(j.at("format")=="openemperor-food-processing-plan-v1"&&j.at("commands").size()<=512,"Replay plan identity/bound invalid");Scene s;
    for(const auto& c:j.at("commands"))s.paid.commands.push_back(decode(c));s.mill=static_cast<Id>(j.at("mill").get<unsigned>());s.shop=static_cast<Id>(j.at("shop").get<unsigned>());
    s.paid.farm=static_cast<Id>(j.at("farm").get<unsigned>());s.paid.market=static_cast<Id>(j.at("market").get<unsigned>());s.tent=static_cast<Id>(j.at("tent").get<unsigned>());s.paid.quay=static_cast<Id>(j.at("quay").get<unsigned>());return s;}
void command(sim::World& world,const sim::Command& cmd,Json& log,const char* reason){const auto tick=world.ticks(),sequence=world.command_sequence();const auto funds=world.treasury();
    const auto valid=world.validate(cmd),result=world.execute(cmd);l::check(valid.accepted&&result.accepted,"Actual command refused: "+result.reason);
    log.push_back({{"tick",tick},{"command",encode(cmd)},{"reason",reason},{"sequence_before",sequence},{"sequence_after",world.command_sequence()},{"funds_before",funds},{"funds_after",world.treasury()}});}
Json balances(const sim::World& world){
    l::check(world.food_processing_enabled()&&world.processed_food_state()&&!world.hunting_food_state()&&!world.fishing_food_state()&&!world.original_agriculture_state()&&
        world.original_production_state()->food==sim::OriginalFoodState{},"Food processing lost sole twelve-column authority");
    p::invariant(world);sim::ProcessedFoodCounters origin{},held{},cargo{},used{},house_used{},house_received{},shop_received{},departed{},received{},returned{};
    for(const auto& field:world.physical_food_growers_state()->fields){const auto at=std::find_if(world.agriculture_fields().begin(),world.agriculture_fields().end(),[&](const auto& row){return row.field_id==field.field_id;});
        l::check(at!=world.agriculture_fields().end(),"Actual Farm return lost crop");origin[static_cast<unsigned>(at->crop)]+=field.returned;}
    for(const auto& quay:world.fishing_state()->quays)origin[5]+=quay.unloaded;for(const auto& tent:world.hunting_state()->tents)origin[6]+=tent.unloaded;
    for(const auto& m:world.food_processing_state()->mills)for(std::size_t grade=0;grade<4;++grade)origin[7+grade]+=m.completed_by_grade[grade]*(grade+1);
    for(const auto& row:world.processed_food_state()->stocks)for(std::size_t g=0;g<12;++g){l::check(row.amounts[g]>=0,"Negative Food stock");held[g]+=static_cast<unsigned>(row.amounts[g]);used[g]+=row.consumed[g];departed[g]+=row.departed[g];received[g]+=row.received[g];returned[g]+=row.returned[g];
        const auto kind=world.building(row.building_id).kind;if(sim::original_house_kind(kind)){house_used[g]+=row.consumed[g];house_received[g]+=row.received[g];}if(kind==sim::Object::OriginalFoodShop66)shop_received[g]+=row.received[g];}
    for(const auto& trip:world.processed_food_state()->trips)for(std::size_t g=0;g<12;++g){const auto amount=std::int64_t(trip.loaded[g])-trip.delivered[g]-trip.returned[g];l::check(amount>=0&&amount<=4,"Invalid physical Food cargo");cargo[g]+=static_cast<unsigned>(amount);}
    for(std::size_t g=0;g<12;++g){l::check(origin[g]==held[g]+cargo[g]+used[g],"Physical origin differs from held+cargo+actual use");l::check(departed[g]==received[g]+returned[g]+cargo[g],"Food departures differ from arrivals+returns+cargo");}
    l::check(!origin[11]&&!used[11],"Unavailable grade5 invented");sim::World::validate_food_processing_snapshot(world.snapshot());
    return {{"tick",world.ticks()},{"origin",origin},{"held",held},{"cargo",cargo},{"used",used},{"house_consumed",house_used},{"house_received",house_received},{"shop_received",shop_received}};
}
Json sample(const sim::World& world){auto result=balances(world);Json mills=Json::array(),recipients=Json::array(),transfers=Json::array();
    for(const auto& stock:world.processed_food_state()->stocks)if(sim::original_house_kind(world.building(stock.building_id).kind)) {
        std::array<std::uint64_t,4> received{},consumed{};
        for(std::size_t grade=0;grade<4;++grade){received[grade]=stock.received[7+grade];consumed[grade]=stock.consumed[7+grade];}
        recipients.push_back({{"house_id",static_cast<unsigned>(stock.building_id)},{"received",received},{"consumed",consumed}});
    }
    for(const auto& trip:world.processed_food_state()->trips)transfers.push_back({
        {"transfer",{{"courier_id",static_cast<unsigned>(trip.transfer.courier_id)},{"sequence",trip.transfer.sequence}}},
        {"source",static_cast<unsigned>(trip.source)},{"target",static_cast<unsigned>(trip.target)},{"delivered",trip.delivered}});
    for(const auto& row:world.food_processing_state()->mills){Json active=nullptr;if(row.active)active={{"sequence",row.active->sequence},{"started_tick",row.active->started_tick},{"grade",row.active->grade},{"work",row.active->work},{"ingredients",row.active->ingredients}};
        mills.push_back({{"mill_id",static_cast<unsigned>(row.mill_id)},{"started",row.started},{"completed",row.completed},{"productive_ticks",row.productive_ticks},
            {"ingredients_consumed",row.ingredients_consumed},{"completed_by_grade",row.completed_by_grade},{"active",active},{"workers",world.workers_assigned(row.mill_id)},{"required",world.workforce_required(row.mill_id)},
            {"operating",world.building(row.mill_id).operating_enabled},{"burning",world.building_on_fire(row.mill_id)}});}
    result["mills"]=mills;result["meal_recipients"]=recipients;result["transfers"]=transfers;return result;
}
void checkpoint(const Context& context,const sim::World& world,const fs::path& output,const std::string& label,Json& manifest,l::save::RecoveryStore* recovery=nullptr,const std::string& history={}){
    const auto path=output/(label+".save.json");l::save::write_save(path,context.document(world),context.map.data,context.map.mask);const auto document=l::save::read_save(path);auto restored=context.restore(document);
    l::check(restored.snapshot()==world.snapshot(),"Schema50 checkpoint changed actual authority");const auto again=output/(label+".roundtrip.json");l::save::write_save(again,context.document(restored),context.map.data,context.map.mask);
    l::check(l::q::f::read_text(path)==l::q::f::read_text(again),"Canonical schema50 roundtrip differs");
    if(recovery){const auto entry=recovery->write_checkpoint(history,document,context.map.mask);l::check(entry.loadable()&&entry.schema==50&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery changed processing/cargo authority");}
    manifest.push_back({{"label",label},{"tick",world.ticks()},{"file",path.filename().string()},{"exact_roundtrip",true},{"exact_recovery",recovery!=nullptr}});
}
struct Options{fs::path data,output,map="Cities/Chengdu.map",replay;unsigned ticks=12000;};
constexpr const char* usage="openemperor-food-processing-acceptance --data ABS --output FRESH_.local/agri-mac09/SUBDIR [--map Cities/Chengdu.map] [--ticks 12000] [--replay CREATOR_DIRECTORY]";
Options options(int argc,char** argv){Options o;std::map<std::string,std::string> args;for(int i=1;i<argc;i+=2){l::check(i+1<argc,usage);const std::string key=argv[i];l::check(key=="--data"||key=="--output"||key=="--map"||key=="--ticks"||key=="--replay",usage);l::check(args.emplace(key,argv[i+1]).second,"Duplicate argument");}
    l::check(args.contains("--data")&&args.contains("--output"),usage);l::no_symlinks(args.at("--data"));o.data=fs::canonical(args.at("--data"));if(args.contains("--map"))o.map=args.at("--map");
    l::check(!o.map.is_absolute()&&o.map==o.map.lexically_normal()&&o.map.parent_path()=="Cities"&&o.map.extension()==".map","Select explicit supplied Cities/name.map");l::no_symlinks(o.data/o.map);
    if(args.contains("--ticks")){const auto& v=args.at("--ticks");const auto parsed=std::from_chars(v.data(),v.data()+v.size(),o.ticks);l::check(parsed.ec==std::errc{}&&parsed.ptr==v.data()+v.size(),"Invalid ticks");}
    l::check(o.ticks>=12000&&o.ticks<=50000,"Require12000..50000 actual ticks");o.output=p::output_root(args.at("--output"),o.data,".local/agri-mac09");
    if(args.contains("--replay")){l::no_symlinks(args.at("--replay"));o.replay=fs::canonical(args.at("--replay"));l::check(fs::is_directory(o.replay)&&!l::below(o.replay,o.data),"Replay must be explicit private directory");}return o;
}
}
int main(int argc,char** argv){fs::path output;std::uint64_t completed=0;try{
    if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}const auto o=options(argc,argv);output=o.output;
    Guard model(o.data/"Model/EmperorBuildingModels.txt"),map_input(o.data/o.map);Context context(o.data,o.map);
    l::check(context.model->provenance().sha256==l::base::model_sha,"Unchanged supplied Normal model required");std::vector<Guard> inputs;
    if(!o.replay.empty())for(const char* file:{"plan.json","seed.save.json","final.save.json"}){
        l::no_symlinks(o.replay/file);
        l::check(fs::is_regular_file(o.replay/file)&&fs::file_size(o.replay/file)<=8U*1024U*1024U,"Replay input exceeds explicit bounded evidence");
        inputs.emplace_back(o.replay/file);
    }
    const auto scene=o.replay.empty()?choose_scene(context.create(),o.map):decode_scene(Json::parse(inputs[0].bytes));l::write(output/"plan.json",encode_scene(scene));
    auto world=context.create();Json commands=Json::array(),manifest=Json::array();for(const auto& cmd:scene.paid.commands)command(world,cmd,commands,"Ordinary paid source-model construction");
    l::check(world.ticks()==0&&world.total_population()==0,"Planner transferred residents or work to seed");for(const auto& stock:world.processed_food_state()->stocks)for(const auto n:stock.amounts)l::check(!n,"Seed injected Food");
    if(!o.replay.empty()){const auto seed=l::save::read_save(o.replay/"seed.save.json");l::check(context.restore(seed).snapshot()==world.snapshot(),"Independently reconstructed paid seed differs");}
    checkpoint(context,world,output,"seed",manifest);auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
    l::save::RecoveryStore recovery(output/"recovery",o.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
    const auto history=recovery.create_history(context.document(world),context.map.mask);std::set<std::string> captured;std::ofstream trace(output/"work.jsonl");l::check(bool(trace),"Cannot open work trace");
    std::optional<sim::Cell> cut;std::optional<std::uint64_t> cut_tick,pause_tick;bool repaired=false,paused=false,resumed=false;Json events=Json::array();auto minimum=world.treasury();
    const auto capture=[&](const std::string& name){if(captured.insert(name).second)checkpoint(context,world,output,name,manifest,&recovery,history);};
    const auto apply=[&](sim::Command cmd,const char* why){Json repeated=Json::array();command(world,cmd,commands,why);command(replay,cmd,repeated,why);l::check(commands.back()==repeated.back(),"Restored paid command differs");};
    while(world.ticks()<o.ticks){const auto before=sample(world);const auto m=mill(world,scene.mill);
        if(!paused&&m.active){capture("processing-active");apply(sim::set_building_operation(scene.mill,false),"Pause real committed recipe without refund/discard");paused=true;pause_tick=world.ticks();capture("processing-paused");}
        if(paused&&!resumed&&world.ticks()>=*pause_tick+17){apply(sim::set_building_operation(scene.mill,true),"Resume real retained processing job");resumed=true;capture("processing-resumed");}
        if(m.requested_grade<4&&before["house_consumed"][6+m.requested_grade].get<std::uint64_t>()>0)
            apply(sim::set_mill_recipe(scene.mill,static_cast<std::uint8_t>(m.requested_grade+1)),"Select next authored recipe only after actual current-grade House consumption");
        bool loaded=false;for(const auto& trip:world.processed_food_state()->trips)for(std::size_t g=7;g<12;++g)if(trip.loaded[g]>trip.delivered[g]+trip.returned[g])loaded=true;
        if(loaded)capture("meals-loaded");
        if(loaded&&!cut&&world.ticks()>=3000) {
            for(const auto& trip:world.processed_food_state()->trips) {
                bool meals=false;for(std::size_t g=7;g<12;++g)meals|=trip.loaded[g]>trip.delivered[g]+trip.returned[g];
                if(!meals)continue;
                const auto& actor=world.courier(trip.transfer.courier_id);
                for(std::size_t index=actor.path_vertex+1;index<actor.path.size();++index) {
                    const auto cell=actor.path[index];const sim::Command remove{sim::CommandType::RemoveRoad,cell};
                    if(!world.validate(remove).accepted)continue;
                    auto probe=world;if(!probe.execute(remove).accepted||probe.find_building_route(trip.source,trip.target))continue;
                    const auto source=trip.source,target=trip.target;const auto courier=trip.transfer.courier_id;
                    const auto sequence=trip.transfer.sequence;const auto cargo=world.original_food_cargo(courier);
                    cut=cell;cut_tick=world.ticks();apply(remove,"Cut the actual loaded meal trip's remaining route; retain cargo and begun edges");
                    l::check(world.original_food_cargo(courier)==cargo,"Road cut discarded actual meal cargo");
                    capture("route-cut-loaded");events.push_back({{"type","paid-road-cut"},{"tick",world.ticks()},{"cell",{cell.x,cell.y}},
                        {"source",static_cast<unsigned>(source)},{"target",static_cast<unsigned>(target)},
                        {"courier",static_cast<unsigned>(courier)},{"sequence",sequence},{"actual_meal_cargo_preserved",cargo},{"source_target_disconnected",true}});
                    break;
                }
                if(cut)break;
            }
        }
        if(cut&&!repaired&&world.ticks()>=*cut_tick+31){apply({sim::CommandType::PlaceRoad,*cut},"Pay to reconnect actual road after loaded-cargo wait");repaired=true;capture("route-repaired");events.push_back({{"type","paid-road-repair"},{"tick",world.ticks()}});}
        const auto tick_before=sample(world);world.tick();replay.tick();completed=world.ticks();minimum=std::min(minimum,world.treasury());const auto after=sample(world);trace<<Json({{"tick",completed},{"before",tick_before},{"after",after}}).dump()<<'\n';
        if(mill(world,scene.mill).last_receipt)capture("processing-completed");if(!world.food_processing_state()->houses.empty())capture("house-meal-arrived");
        if(std::any_of(world.food_processing_state()->houses.begin(),world.food_processing_state()->houses.end(),[](const auto& row){return row.first_consumption.has_value();}))capture("house-meal-consumed");
        if(completed%816==0){l::check(world.snapshot()==replay.snapshot(),"Independent restored continuation diverged");model.unchanged();map_input.unchanged();}
    }
    trace.close();l::check(bool(trace),"Work trace write failed");capture("final");l::check(world.snapshot()==replay.snapshot(),"Final restored continuation differs");bool independent=!o.replay.empty();
    if(independent){l::check(context.restore(l::save::read_save(o.replay/"final.save.json")).snapshot()==world.snapshot(),"Separate-process final authority differs");l::check(l::q::f::read_text(o.replay/"final.save.json")==l::q::f::read_text(output/"final.save.json"),"Separate-process canonical final bytes differ");}
    const auto final=sample(world);std::array<std::uint64_t,7> mill_received{},ingredients{};for(const auto& stock:world.processed_food_state()->stocks)if(stock.building_id==scene.mill)for(std::size_t g=0;g<7;++g)mill_received[g]+=stock.received[g];
    for(const auto& row:world.food_processing_state()->mills)for(const auto& grade:row.ingredients_consumed)for(std::size_t g=0;g<7;++g)ingredients[g]+=grade[g];
    bool positive=minimum>=0&&paused&&resumed&&repaired;for(unsigned g=0;g<7;++g)positive&=final["origin"][g].get<std::uint64_t>()>0&&mill_received[g]>0&&ingredients[g]>0;
    for(unsigned g=7;g<11;++g)positive&=final["origin"][g].get<std::uint64_t>()>0&&final["shop_received"][g].get<std::uint64_t>()>0&&final["house_consumed"][g].get<std::uint64_t>()>0;
    model.unchanged();map_input.unchanged();for(const auto& input:inputs)input.unchanged();l::write(output/"commands.json",commands);
    const auto status=positive?"PASS":"PARTIAL";l::write(output/"report.json",{{"format","openemperor-food-processing-acceptance-v1"},{"status",status},{"rule",34},{"schema",50},{"real_ticks",completed},
        {"map",o.map.generic_string()},{"map_sha256",context.terrain->source_map_sha256()},{"model_sha256",context.model->provenance().sha256},{"unchanged_supplied_model",true},{"source_bytes_mode_mtime_unchanged",true},
        {"snapshot_stock_or_population_injection",false},{"minimum_funds",minimum},{"mill_ingredient_arrivals",mill_received},{"actual_recipe_ingredients_consumed",ingredients},{"final",final},
        {"actual_route_events",events},{"checkpoints",manifest},{"work_trace","work.jsonl"},{"recovery_exact",true},{"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",independent},
        {"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"claim_scope","AUTHORED distinct-ingredient unit recipes, staffed work and physical meals. Source Model identity/cost/staff and observed dimensions do not establish native runtime reproduction."}});
    std::cout<<status<<" Rule34/schema50 seven raw sources and physical graded meals; ticks="<<completed<<" report="<<output/"report.json"<<'\n';return positive?0:2;
}catch(const std::exception& error){if(!output.empty()&&!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()},{"last_completed_tick",completed}});}catch(...){}
    std::cerr<<"Food processing acceptance FAIL: "<<error.what()<<'\n';return 1;}}
