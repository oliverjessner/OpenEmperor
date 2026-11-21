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
    sim::World create()const{return sim::World::create_food_warehouses(map.policy,model,terrain);}
    l::save::SaveDocument document(const sim::World& world)const{return l::save::make_document(map.data,map.map_relative,map.mask,world);}
    sim::World restore(const l::save::SaveDocument& value)const{
        l::check(value.source_schema_version==51&&value.world.rule_version==35&&value.map_relative==map.map_relative,"Replay requires exact Rule35/schema51 and selected map");
        return l::save::restore_food_warehouses(value,map.data,map.mask,map.policy,model,terrain);
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
struct Scene{p::Plan paid;Id mill{},shop{},tent{},warehouse{},second_mill{},second_market{};};
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
                try{scene.warehouse=add_body(next,scene.paid,sim::CommandType::PlaceWarehouse);
                    scene.second_mill=add_body(next,scene.paid,sim::CommandType::PlaceOriginalMill);
                    scene.second_market=add_body(next,scene.paid,sim::CommandType::PlaceMarket);
                    for(const auto id:{scene.mill,scene.shop,scene.paid.market,scene.second_mill,scene.second_market})
                        l::check(p::buy(next,scene.paid,sim::set_building_operation(id,false)),"Initial paid outlet pause refused");
                    l::check(p::buy(next,scene.paid,sim::set_mill_recipe(scene.second_mill,3)),"Explicit grade3 selection refused");
                }catch(const std::exception&){continue;}
                return scene;}}
    }
    throw std::runtime_error("Finite actual-map five-field/Mill/Shop/Tent/Quay/Warehouse planner found no fully paid legal city");
}
Json encode(const sim::Command& c){return {{"type",static_cast<unsigned>(c.type)},{"cell",{c.cell.x,c.cell.y}},{"owner",static_cast<unsigned>(c.building_id)},
    {"orientation",c.orientation},{"recipe_grade",c.recipe_grade},{"operating_enabled",c.operating_enabled},{"workforce_priority",static_cast<unsigned>(c.workforce_priority)}};}
sim::Command decode(const Json& j){sim::Command c{static_cast<sim::CommandType>(j.at("type").get<unsigned>()),{j.at("cell")[0].get<int>(),j.at("cell")[1].get<int>()}};
    c.building_id=static_cast<Id>(j.at("owner").get<unsigned>());c.orientation=j.at("orientation").get<std::uint8_t>();c.recipe_grade=j.at("recipe_grade").get<std::uint8_t>();
    c.operating_enabled=j.at("operating_enabled").get<bool>();c.workforce_priority=static_cast<sim::WorkforcePriority>(j.at("workforce_priority").get<unsigned>());return c;}
Json encode_scene(const Scene& s){Json commands=Json::array();for(const auto& c:s.paid.commands)commands.push_back(encode(c));return {{"format","openemperor-food-warehouses-plan-v1"},
    {"commands",commands},{"warehouse",static_cast<unsigned>(s.warehouse)},{"second_mill",static_cast<unsigned>(s.second_mill)},{"second_market",static_cast<unsigned>(s.second_market)},
    {"mill",static_cast<unsigned>(s.mill)},{"shop",static_cast<unsigned>(s.shop)},{"farm",static_cast<unsigned>(s.paid.farm)},
    {"market",static_cast<unsigned>(s.paid.market)},{"tent",static_cast<unsigned>(s.tent)},{"quay",static_cast<unsigned>(s.paid.quay)}};}
Scene decode_scene(const Json& j){l::check(j.at("format")=="openemperor-food-warehouses-plan-v1"&&j.at("commands").size()<=512,"Exact bounded Warehouse replay plan required");Scene s;
    for(const auto& c:j.at("commands"))s.paid.commands.push_back(decode(c));
    s.warehouse=static_cast<Id>(j.at("warehouse").get<unsigned>());s.second_mill=static_cast<Id>(j.at("second_mill").get<unsigned>());s.second_market=static_cast<Id>(j.at("second_market").get<unsigned>());
    s.mill=static_cast<Id>(j.at("mill").get<unsigned>());s.shop=static_cast<Id>(j.at("shop").get<unsigned>());s.paid.farm=static_cast<Id>(j.at("farm").get<unsigned>());
    s.paid.market=static_cast<Id>(j.at("market").get<unsigned>());s.tent=static_cast<Id>(j.at("tent").get<unsigned>());s.paid.quay=static_cast<Id>(j.at("quay").get<unsigned>());return s;}
const sim::ProcessedFoodStock& stock(const sim::World& w,Id id){for(const auto& s:w.processed_food_state()->stocks)if(s.building_id==id)return s;throw std::runtime_error("Lost sole paid Food owner");}
const sim::ProcessedFoodTrip* trip(const sim::World& w,sim::CourierId id){for(const auto& t:w.processed_food_state()->trips)if(t.transfer.courier_id==id)return &t;return nullptr;}
int volume(const sim::ProcessedFoodAmounts& a){int n=0;for(auto v:a)n+=v;return n;}
int cargo(const sim::ProcessedFoodTrip& t){int n=0;for(std::size_t g=0;g<12;++g)n+=t.loaded[g]-t.delivered[g]-t.returned[g];return n;}
Json sample(const sim::World& w){
    l::check(w.rule_version()==35&&w.processed_food_state()&&w.food_warehouses_state()&&!w.hunting_food_state()&&!w.fishing_food_state()&&!w.original_agriculture_state(),"Warehouse duplicated Food authority");
    l::check(w.population_valid()&&w.city_economy_valid()&&w.production_balance_valid()&&w.navigation_valid()&&w.fire_state_valid(),"Paid Warehouse broke World invariants");
    const auto channels=w.couriers().size()+w.fishing_state()->quays.size()+w.hunting_state()->tents.size()+3*w.hunting_state()->habitats.size();
    l::check(w.buildings().size()<=82&&channels<=58,"Raised paid body/channel limits");
    sim::ProcessedFoodCounters origin{},held{},carried{},used{},departed{},received{},returned{},house_used{},house_received{};
    for(const auto& f:w.physical_food_growers_state()->fields){const auto at=std::find_if(w.agriculture_fields().begin(),w.agriculture_fields().end(),[&](const auto& a){return a.field_id==f.field_id;});l::check(at!=w.agriculture_fields().end(),"Paid crop identity lost");origin[static_cast<std::size_t>(at->crop)]+=f.returned;}
    for(const auto& q:w.fishing_state()->quays)origin[5]+=q.unloaded;for(const auto& h:w.hunting_state()->tents)origin[6]+=h.unloaded;
    Json mills=Json::array(),warehouses=Json::array(),transfers=Json::array(),houses=Json::array();
    for(const auto& m:w.food_processing_state()->mills){for(std::size_t g=0;g<4;++g)origin[7+g]+=m.completed_by_grade[g]*(g+1);
        Json active=nullptr;if(m.active)active={{"sequence",m.active->sequence},{"started_tick",m.active->started_tick},{"grade",m.active->grade},{"ingredients",m.active->ingredients},{"work",m.active->work}};
        mills.push_back({{"mill_id",static_cast<unsigned>(m.mill_id)},{"started",m.started},{"completed",m.completed},{"completed_by_grade",m.completed_by_grade},
            {"productive_ticks",m.productive_ticks},{"ingredients_consumed",m.ingredients_consumed},{"active",active},
            {"workers",w.workers_assigned(m.mill_id)},{"required",w.workforce_required(m.mill_id)},{"operating",w.building(m.mill_id).operating_enabled},{"burning",w.building_on_fire(m.mill_id)}});}
    for(const auto& s:w.processed_food_state()->stocks)for(std::size_t g=0;g<12;++g){l::check(s.amounts[g]>=0,"Negative real Food stock");held[g]+=static_cast<unsigned>(s.amounts[g]);used[g]+=s.consumed[g];departed[g]+=s.departed[g];received[g]+=s.received[g];returned[g]+=s.returned[g];
        if(sim::original_house_kind(w.building(s.building_id).kind)){house_used[g]+=s.consumed[g];house_received[g]+=s.received[g];}}
    for(const auto& s:w.processed_food_state()->stocks)if(sim::original_house_kind(w.building(s.building_id).kind))houses.push_back({{"house_id",static_cast<unsigned>(s.building_id)},{"consumed",s.consumed}});
    for(const auto& t:w.processed_food_state()->trips){for(std::size_t g=0;g<12;++g){const auto n=t.loaded[g]-t.delivered[g]-t.returned[g];l::check(n>=0&&n<=4,"Invalid physical Food cargo");carried[g]+=static_cast<unsigned>(n);}
        transfers.push_back({{"transfer",{{"courier_id",static_cast<unsigned>(t.transfer.courier_id)},{"sequence",t.transfer.sequence}}},{"role",static_cast<unsigned>(w.courier(t.transfer.courier_id).role)},
            {"source",static_cast<unsigned>(t.source)},{"target",static_cast<unsigned>(t.target)},{"phase",static_cast<unsigned>(t.phase)},{"loaded",t.loaded},{"delivered",t.delivered},{"returned",t.returned}});}
    for(std::size_t g=0;g<12;++g){l::check(origin[g]==held[g]+carried[g]+used[g],"Food physical origin conservation failed");l::check(departed[g]==received[g]+returned[g]+carried[g],"Food physical transfer conservation failed");}
    for(const auto& row:w.food_warehouses_state()->warehouses){const auto& s=stock(w,row.warehouse_id);const auto status=w.food_warehouse_status(row.warehouse_id);
        l::check(volume(s.amounts)+status.incoming+status.return_reserved<=32,"Warehouse shared Food capacity exceeded");
        for(std::size_t g=0;g<12;++g){int claims=0;for(const auto& t:w.processed_food_state()->trips){const auto n=t.loaded[g]-t.delivered[g]-t.returned[g];if(t.source==row.warehouse_id||(t.target==row.warehouse_id&&t.phase==sim::OriginalGoodsTransferPhase::Loaded))claims+=n;}
            l::check(s.amounts[g]+claims<=4&&!s.consumed[g],"Warehouse per-kind return quota or no-consumption failed");}
        warehouses.push_back({{"warehouse_id",static_cast<unsigned>(row.warehouse_id)},{"amounts",s.amounts},{"received",s.received},{"departed",s.departed},{"returned",s.returned},
            {"incoming",status.incoming},{"return_reserved",status.return_reserved},{"workers",w.workers_assigned(row.warehouse_id)},{"required",w.workforce_required(row.warehouse_id)},
            {"operating",w.building(row.warehouse_id).operating_enabled},{"burning",w.building_on_fire(row.warehouse_id)}});}
    for(std::size_t g=0;g<orig::goods_count;++g)l::check(w.original_goods_balance(static_cast<orig::Goods>(g)).conserved,"Food changed Goods conservation");
    for(std::size_t g=0;g<sim::original_aux_material_count;++g)l::check(w.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(g)).conserved,"Food changed Aux conservation");
    l::check(!origin[11]&&!used[11],"Unavailable grade5 invented");
    sim::World::validate_food_processing_snapshot(w.snapshot());sim::World::validate_food_warehouses_snapshot(w.snapshot());
    return {{"tick",w.ticks()},{"origin",origin},{"held",held},{"cargo",carried},{"used",used},{"departed",departed},{"received",received},{"returned",returned},
        {"house_consumed",house_used},{"house_received",house_received},{"house_uses",houses},{"mills",mills},{"warehouses",warehouses},{"transfers",transfers},{"bodies",w.buildings().size()},{"channels",channels}};
}
void command(sim::World& w,sim::Command c,Json& log,const char* why){const auto funds=w.treasury();const auto result=w.execute(c);l::check(result.accepted,std::string(why)+": "+result.reason);
    log.push_back({{"tick",w.ticks()},{"command",encode(c)},{"reason",why},{"funds_before",funds},{"funds_after",w.treasury()},{"changed",result.changed}});}
void checkpoint(const Context& context,const sim::World& w,const fs::path& out,const std::string& label,Json& manifest,l::save::RecoveryStore* recovery=nullptr,const std::string& history={}){
    const auto path=out/(label+".save.json");l::save::write_save(path,context.document(w),context.map.data,context.map.mask);const auto doc=l::save::read_save(path);auto restored=context.restore(doc);
    l::check(restored.snapshot()==w.snapshot(),"Schema51 checkpoint changed actual Warehouse authority");const auto again=out/(label+".roundtrip.json");l::save::write_save(again,context.document(restored),context.map.data,context.map.mask);
    l::check(l::q::f::read_text(path)==l::q::f::read_text(again),"Canonical schema51 bytes changed");
    if(recovery){const auto row=recovery->write_checkpoint(history,doc,context.map.mask);l::check(row.loadable()&&row.schema==51&&context.restore(l::save::read_save(row.path)).snapshot()==w.snapshot(),"Recovery changed Warehouse cargo or provenance");}
    manifest.push_back({{"label",label},{"tick",w.ticks()},{"file",path.filename().string()},{"exact_roundtrip",true},{"exact_recovery",recovery!=nullptr}});
}
struct Options{fs::path data,output,map="Cities/Chengdu.map",replay;unsigned ticks=24000;};
constexpr const char* usage="openemperor-food-warehouses-acceptance --data ABS --output FRESH_.local/agri-mac10/SUBDIR [--map Cities/Chengdu.map] [--ticks 24000] [--replay CREATOR_DIRECTORY]";
Options options(int argc,char** argv){Options o;std::map<std::string,std::string> args;for(int i=1;i<argc;i+=2){l::check(i+1<argc,usage);const std::string key=argv[i];l::check(key=="--data"||key=="--output"||key=="--map"||key=="--ticks"||key=="--replay",usage);l::check(args.emplace(key,argv[i+1]).second,"Duplicate argument");}
    l::check(args.contains("--data")&&args.contains("--output"),usage);l::no_symlinks(args.at("--data"));o.data=fs::canonical(args.at("--data"));if(args.contains("--map"))o.map=args.at("--map");
    l::check(!o.map.is_absolute()&&o.map==o.map.lexically_normal()&&o.map.parent_path()=="Cities"&&o.map.extension()==".map","Explicit supplied Cities/name.map required");l::no_symlinks(o.data/o.map);
    if(args.contains("--ticks")){const auto& text=args.at("--ticks");const auto parsed=std::from_chars(text.data(),text.data()+text.size(),o.ticks);l::check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size(),"Invalid tick count");}
    l::check(o.ticks>=12000&&o.ticks<=50000,"Require12000..50000 actual ticks");o.output=p::output_root(args.at("--output"),o.data,".local/agri-mac10");
    if(args.contains("--replay")){l::no_symlinks(args.at("--replay"));o.replay=fs::canonical(args.at("--replay"));l::check(fs::is_directory(o.replay)&&!l::below(o.replay,o.data),"Explicit private replay directory required");}return o;}
}
int main(int argc,char** argv){fs::path output;std::uint64_t completed=0;try{
    if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}const auto o=options(argc,argv);output=o.output;
    Guard model(o.data/"Model/EmperorBuildingModels.txt"),map(o.data/o.map);Context context(o.data,o.map);
    l::check(context.model->provenance().sha256==l::base::model_sha,"Unchanged supplied Normal model required");std::vector<Guard> inputs;
    if(!o.replay.empty())for(const char* name:{"plan.json","seed.save.json","final.save.json"}){l::no_symlinks(o.replay/name);l::check(fs::is_regular_file(o.replay/name)&&fs::file_size(o.replay/name)<=8U*1024U*1024U,"Bounded explicit replay input required");inputs.emplace_back(o.replay/name);}
    const auto scene=o.replay.empty()?choose_scene(context.create(),o.map):decode_scene(Json::parse(inputs[0].bytes));l::write(output/"plan.json",encode_scene(scene));
    auto w=context.create();Json commands=Json::array(),manifest=Json::array(),events=Json::array();for(const auto& c:scene.paid.commands)command(w,c,commands,"Ordinary paid source-bound construction and explicit outlet selection");
    l::check(!w.ticks()&&!w.total_population(),"Paid seed injected work or residents");for(const auto& s:w.processed_food_state()->stocks)l::check(!volume(s.amounts),"Paid construction created Food");
    if(!o.replay.empty())l::check(context.restore(l::save::read_save(o.replay/"seed.save.json")).snapshot()==w.snapshot(),"Separate process did not independently rebuild the paid seed");
    checkpoint(context,w,output,"seed",manifest);auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
    l::save::RecoveryStore recovery(output/"recovery",o.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);const auto history=recovery.create_history(context.document(w),context.map.mask);
    std::set<std::string> captured;std::ofstream trace(output/"work.jsonl");l::check(bool(trace),"Cannot open actual work trace");
    const auto capture=[&](const std::string& name){if(captured.insert(name).second)checkpoint(context,w,output,name,manifest,&recovery,history);};
    const auto apply=[&](sim::Command c,const char* why){Json repeated=Json::array();command(w,c,commands,why);command(replay,c,repeated,why);l::check(commands.back()==repeated.back(),"Restored paid command differs");};
    const auto operate=[&](Id id,bool enabled,const char* why){apply(sim::set_building_operation(id,enabled),why);};
    sim::CourierId actor{};for(const auto& c:w.couriers())if(c.owner==scene.warehouse&&c.role==sim::CourierRole::OriginalWarehouseFood)actor=c.id;
    l::check(static_cast<unsigned>(actor)>0,"Paid Warehouse has no counted Food courier");
    unsigned phase=0;std::uint64_t three_tick=0,cut_tick=0;bool cut_done=false,repaired=false,returned=false;std::vector<sim::Cell> cut_cells;sim::ProcessedFoodAmounts cut_load{};
    auto minimum=w.treasury();std::size_t simultaneous=0;bool producers[4]{};Json raw_consumption=nullptr,meal_consumption=nullptr;
    while(w.ticks()<o.ticks){
        const auto held=stock(w,scene.warehouse);std::size_t kinds=0;for(std::size_t g=0;g<7;++g)kinds+=held.amounts[g]>0;simultaneous=std::max(simultaneous,kinds);
        for(const auto& h:w.food_warehouses_state()->warehouses)if(h.warehouse_id==scene.warehouse)for(const auto& first:h.first_received)if(first){
            producers[0]|=first->source==scene.paid.farm;producers[1]|=first->source==scene.paid.quay;producers[2]|=first->source==scene.tent;producers[3]|=first->source==scene.second_mill;}
        if(phase==0&&kinds>=3&&producers[0]&&producers[1]&&producers[2]){capture("warehouse-three-kinds");three_tick=w.ticks();
            for(const auto id:{scene.paid.farm,scene.paid.quay,scene.tent})operate(id,false,"Pause actual raw sources; later fresh outlets can receive only stored Warehouse Food");phase=1;}
        if(phase==1){bool drained=true;for(const auto& c:w.couriers())if(c.role==sim::CourierRole::OriginalFoodToMarket&&(c.owner==scene.paid.farm||c.owner==scene.paid.quay||c.owner==scene.tent))drained&=c.phase==sim::CourierPhase::IdleAtWorkshop;
            if(drained){operate(scene.second_mill,true,"Enable the fresh empty Mill after all direct raw carts have actually returned");phase=2;}}
        const auto* current=trip(w,actor);
        if(phase==2&&current&&current->target==scene.second_mill&&current->phase==sim::OriginalGoodsTransferPhase::Loaded&&cargo(*current)>0)capture("warehouse-raw-loaded");
        if(phase==2&&held.received[9]>0){capture("warehouse-meals-held");operate(scene.second_mill,false,"Retain actual Warehouse meals and stop the only active Mill");
            operate(scene.second_market,true,"Enable fresh empty Market whose only possible raw supplier is Warehouse");operate(scene.shop,true,"Enable real FoodShop for physically stored Warehouse meals");phase=3;}
        current=trip(w,actor);
        if(phase>=3&&current&&current->target==scene.second_market&&current->phase==sim::OriginalGoodsTransferPhase::Loaded&&cargo(*current)>0)capture("warehouse-market-loaded");
        if(phase>=3&&!cut_done&&current&&current->phase==sim::OriginalGoodsTransferPhase::Loaded&&cargo(*current)>0){
            const auto& c=w.courier(actor);if(c.phase==sim::CourierPhase::ToWarehouse&&c.edge_progress>0&&c.path_vertex>=2){
                std::set<std::pair<int,int>> access;for(const auto cell:w.building_cells(scene.warehouse))for(const sim::Cell d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}){const sim::Cell at{cell.x+d.x,cell.y+d.y};if(roads(w,at))access.insert({at.x,at.y});}
                auto probe=w;bool legal=!access.empty();for(const auto [x,y]:access){const sim::Command remove{sim::CommandType::RemoveRoad,{x,y}};if(!probe.validate(remove).accepted||!probe.execute(remove).accepted){legal=false;break;}}
                if(legal&&!probe.find_building_route(scene.warehouse,current->target)){
                    const auto target=current->target,source=current->source;const auto sequence=current->transfer.sequence;cut_load=current->loaded;const auto before=c;
                    for(const auto [x,y]:access){cut_cells.push_back({x,y});apply({sim::CommandType::RemoveRoad,{x,y}},"Cut paid Warehouse access only after the real loaded courier has left its begun cells");}
                    operate(scene.warehouse,false,"Cancel loaded dispatch through the ordinary source pause; retain cargo on physical return");
                    l::check(w.courier(actor).edge_progress==before.edge_progress&&trip(w,actor)->loaded==cut_load,"Cut/cancel rewound a begun edge or discarded Food");
                    cut_tick=w.ticks();cut_done=true;capture("route-cut-loaded");Json cells=Json::array();for(const auto at:cut_cells)cells.push_back({at.x,at.y});
                    events.push_back({{"type","paid-road-cut"},{"tick",cut_tick},{"cells",cells},{"source",static_cast<unsigned>(source)},{"target",static_cast<unsigned>(target)},
                        {"courier",static_cast<unsigned>(actor)},{"sequence",sequence},{"loaded",cut_load},{"actual_cargo",cargo(*trip(w,actor))},{"source_target_disconnected",true}});
                }
            }
        }
        if(cut_done&&!repaired&&w.ticks()>=cut_tick+31){l::check(trip(w,actor)&&cargo(*trip(w,actor))==volume(cut_load),"Blocked loaded return lost actual Food before repair");
            for(const auto at:cut_cells)apply({sim::CommandType::PlaceRoad,at},"Pay to repair actual Warehouse road access");repaired=true;capture("route-repaired");events.push_back({{"type","paid-road-repair"},{"tick",w.ticks()}});}
        if(repaired&&!returned&&trip(w,actor)&&trip(w,actor)->phase==sim::OriginalGoodsTransferPhase::Returned){l::check(trip(w,actor)->returned==cut_load&&!cargo(*trip(w,actor)),"Real Warehouse return did not settle original typed cargo exactly once");
            returned=true;capture("warehouse-returned");events.push_back({{"type","actual-warehouse-return"},{"tick",w.ticks()},{"returned",trip(w,actor)->returned}});operate(scene.warehouse,true,"Resume stocked Warehouse only after its cancelled courier physically returned");}
        const auto before=sample(w);w.tick();replay.tick();completed=w.ticks();minimum=std::min(minimum,w.treasury());const auto after=sample(w);trace<<Json({{"tick",completed},{"before",before},{"after",after}}).dump()<<'\n';
        for(const auto& h:after["house_uses"]){const auto old=std::find_if(before["house_uses"].begin(),before["house_uses"].end(),[&](const auto& value){return value["house_id"]==h["house_id"];});
            l::check(old!=before["house_uses"].end(),"Actual House consumption lost its pre-tick account");const auto id=static_cast<Id>(h["house_id"].get<unsigned>());const auto& account=stock(w,id);
            for(std::size_t g=0;g<12;++g)if(h["consumed"][g].get<std::uint64_t>()>(*old)["consumed"][g].get<std::uint64_t>()){
                if(g<7&&raw_consumption.is_null()&&account.quality_witness&&w.courier(account.quality_witness->transfer.courier_id).owner==scene.second_market&&
                   account.quality_witness->accepted[g]>0&&account.consumed[g]>account.quality_witness->consumed_before[g])
                    raw_consumption={{"house_id",static_cast<unsigned>(id)},{"slot",g},{"tick",completed},{"market_id",static_cast<unsigned>(scene.second_market)},
                        {"arrival_tick",account.quality_witness->tick},{"transfer",{{"courier_id",static_cast<unsigned>(account.quality_witness->transfer.courier_id)},{"sequence",account.quality_witness->transfer.sequence}}}};
                if(g>=7&&meal_consumption.is_null())meal_consumption={{"house_id",static_cast<unsigned>(id)},{"grade",g-6},{"tick",completed}};
            }}
        if(!raw_consumption.is_null())capture("warehouse-house-raw-consumed");if(!meal_consumption.is_null())capture("warehouse-house-meal-consumed");
        if(completed%816==0){l::check(w.snapshot()==replay.snapshot(),"Fresh-restored Warehouse continuation diverged");model.unchanged();map.unchanged();}
    }
    trace.close();l::check(bool(trace),"Actual trace publication failed");capture("final");l::check(w.snapshot()==replay.snapshot(),"Terminal restored Warehouse continuation differs");
    const bool independent=!o.replay.empty();if(independent){l::check(context.restore(l::save::read_save(o.replay/"final.save.json")).snapshot()==w.snapshot(),"Separate process final Warehouse authority differs");
        l::check(l::q::f::read_text(o.replay/"final.save.json")==l::q::f::read_text(output/"final.save.json"),"Separate process canonical final bytes differ");}
    const auto final=sample(w);const auto& fresh=mill(w,scene.second_mill);bool warehouse_only=true;
    for(const auto& first:fresh.first_ingredients)if(first)warehouse_only&=first->source==scene.warehouse;
    for(const auto& h:w.food_warehouses_state()->markets)if(h.market_id==scene.second_market)for(const auto& first:h.first_received)if(first)warehouse_only&=first->source==scene.warehouse;
    const auto& shop=*std::find_if(w.food_processing_state()->shops.begin(),w.food_processing_state()->shops.end(),[&](const auto& s){return s.shop_id==scene.shop;});
    warehouse_only&=shop.first_meals[2]&&shop.first_meals[2]->source==scene.warehouse;
    std::uint64_t raw_market_received=0;for(std::size_t g=0;g<7;++g)raw_market_received+=stock(w,scene.second_market).received[g];
    bool positive=phase==3&&simultaneous>=3&&std::all_of(std::begin(producers),std::end(producers),[](bool x){return x;})&&warehouse_only&&fresh.completed_by_grade[2]>0&&
        raw_market_received>0&&cut_done&&repaired&&returned&&minimum>=0&&captured.contains("warehouse-house-raw-consumed")&&captured.contains("warehouse-house-meal-consumed");
    l::check(std::all_of(stock(w,scene.paid.market).received.begin(),stock(w,scene.paid.market).received.end(),[](auto n){return n==0;})&&!mill(w,scene.mill).started,"Paused historical outlet contaminated the Warehouse-only proof");
    model.unchanged();map.unchanged();for(const auto& g:inputs)g.unchanged();l::write(output/"commands.json",commands);const auto status=positive?"PASS":"PARTIAL";
    l::write(output/"report.json",{{"format","openemperor-food-warehouses-acceptance-v1"},{"status",status},{"rule",35},{"schema",51},{"real_ticks",completed},
        {"map",o.map.generic_string()},{"map_sha256",context.terrain->source_map_sha256()},{"model_sha256",context.model->provenance().sha256},
        {"unchanged_supplied_model",true},{"source_bytes_mode_mtime_unchanged",true},{"snapshot_stock_or_population_injection",false},{"minimum_funds",minimum},
        {"source_classes_received",{{"Farm",producers[0]},{"Quay",producers[1]},{"Tent",producers[2]},{"Mill",producers[3]}}},{"maximum_simultaneous_raw_kinds",simultaneous},
        {"first_three_kind_tick",three_tick},{"fresh_outlets_only_warehouse_origin",warehouse_only},{"raw_consumption_witness",raw_consumption},{"meal_consumption_witness",meal_consumption},
        {"actual_route_events",events},{"checkpoints",manifest},{"work_trace","work.jsonl"},{"final",final},
        {"recovery_exact",true},{"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",independent},{"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},
        {"claim_scope","AUTHORED bounded Warehouse Food compartment and deterministic physical redistribution; no Original storage/courier reproduction or Warehouse model geometry change."}});
    std::cout<<status<<" Rule35/schema51 actual Farm/Quay/Tent/Mill storage, independent Warehouse redistribution and House consumption; ticks="<<completed<<" report="<<output/"report.json"<<'\n';return positive?0:2;
}catch(const std::exception& e){if(!output.empty()&&!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",e.what()},{"last_completed_tick",completed}});}catch(...){}
    std::cerr<<"Food Warehouses acceptance FAIL: "<<e.what()<<'\n';return 1;}}
