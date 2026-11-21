// Supplied-data matched irrigation proof. All state originates in paid World
// commands and ordinary ticks; synthetic Fixture classes are never instantiated.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "FoodNativePlaytest.h"
#include "simulation/original/IrrigationRules.h"

#include <charconv>
#include <limits>
#include <set>

namespace l=oe_luxury_progression;
namespace p=oe_food_playtest;
namespace {
using Id=sim::BuildingId;
constexpr std::array crop_commands{sim::CommandType::PlaceOriginalWheatField,sim::CommandType::PlaceOriginalMilletField,
    sim::CommandType::PlaceOriginalRiceField,sim::CommandType::PlaceOriginalCabbageField,sim::CommandType::PlaceOriginalSoybeanField};
constexpr std::array<const char*,9> kind_names{"Wheat","Millet","Rice","Cabbage","SoyBeanCurd","Hemp","Tea","Lacquer","Mulberry"};
struct ReplayGuard {
    fs::path path;std::string bytes;fs::perms permissions{};fs::file_time_type modified{};
    explicit ReplayGuard(const fs::path& value):path(value) {
        if(!path.empty()){bytes=l::q::f::read_text(path);permissions=fs::status(path).permissions();modified=fs::last_write_time(path);}
    }
    void unchanged()const {if(!path.empty())l::check(l::q::f::read_text(path)==bytes&&fs::status(path).permissions()==permissions&&
        fs::last_write_time(path)==modified,"Replay input bytes, permissions or modification time changed");}
};
struct Context {
    struct Source {
        l::q::Context map;
        std::shared_ptr<const orig::OriginalRulesModel> model;
        std::shared_ptr<const sim::OriginalTerrainFacts> terrain;
        Source(const fs::path& data,const fs::path& selected):map(oe_longplay_fixture::real_map_context(data,selected)),
            model(l::save::prepare_playable_hunting_rules_model(data,orig::Difficulty::Normal)),
            terrain(l::save::prepare_playable_hunting_terrain_facts(data,selected)) {}
        l::save::SaveDocument document(const sim::World& world)const {
            return l::save::make_document(map.data,map.map_relative,map.mask,world);
        }
    } source;
    Context(const fs::path& data,const fs::path& map):source(data,map) {}
    sim::World create()const{return sim::World::create_irrigation_productivity(source.map.policy,source.model,source.terrain);}
    l::save::SaveDocument document(const sim::World& world)const{return source.document(world);}
    sim::World restore(const l::save::SaveDocument& document)const {
        l::check(document.source_schema_version==49&&document.world.rule_version==33,"Exact Rule33/schema49 input required");
        l::check(document.map_relative==source.map.map_relative,"Replay must retain the explicitly selected source map");
        return l::save::restore_irrigation_productivity(document,source.map.data,source.map.mask,source.map.policy,source.model,source.terrain);
    }
};
const sim::PhysicalFoodGrowerFarm& worker_source(const sim::World& world,Id farm) {
    for(const auto& row:world.physical_food_growers_state()->farms)if(row.farm_id==farm)return row;
    throw std::runtime_error("Paid Farm lost its physical Grower source");
}
bool food_domain(sim::Cell farm,sim::Cell cell) {
    return cell.x>=farm.x-3&&cell.x<=farm.x+5&&cell.y>=farm.y-3&&cell.y<=farm.y+5;
}
bool outside_food_domain(const sim::World& world,Id farm,sim::Object kind,sim::Cell cell) {
    const auto origin=world.building(farm).cell;
    for(const auto occupied:sim::building_footprint_cells(world.profile(),world.rule_version(),kind,cell))
        if(food_domain(origin,occupied))return false;
    return true;
}
int road_distance(const p::Plan& plan,sim::Cell cell) {
    int result=1000;for(const auto road:plan.roads)result=std::min(result,p::distance(road,cell));return result;
}
bool existing_road(const sim::World& world,sim::Cell cell) {return world.in_bounds(cell)&&l::base::initial_road(world,cell);}
bool routes_to_all_fields(const sim::World& world,Id farm) {
    const auto& channel=world.courier(worker_source(world,farm).courier_id);
    for(const auto& field:world.hunting_food_state()->fields)if(field.farm_id==farm) {
        const auto path=std::find_if(channel.dynamic_target_routes.begin(),channel.dynamic_target_routes.end(),
            [&](const auto& row){return row.first==field.field_id;});
        if(path==channel.dynamic_target_routes.end()||!path->second||path->second->size()<2)return false;
    }
    return true;
}
bool routes_to_all_food(const sim::World& world) {
    for(const auto& farm:world.physical_food_growers_state()->farms)
        if(!routes_to_all_fields(world,farm.farm_id))return false;
    return true;
}
bool routes_to_all_plants(const sim::World& world) {
    for(const auto& source:world.plantations_state()->sources) {
        const auto& channel=world.courier(source.courier_id);
        for(const auto& plant:world.plantations_state()->plants)if(plant.source_id==source.building_id) {
            const auto path=std::find_if(channel.dynamic_target_routes.begin(),channel.dynamic_target_routes.end(),
                [&](const auto& row){return row.first==plant.plant_id;});
            if(path==channel.dynamic_target_routes.end()||!path->second||path->second->size()<2)return false;
        }
    }
    return true;
}
bool five_fields(sim::World& world,p::Plan& plan) {
    const auto origin=world.building(plan.farm).cell;
    std::vector<sim::Cell> cells;
    for(int y=origin.y-3;y<=origin.y+5;++y)for(int x=origin.x-3;x<=origin.x+5;++x)if(world.in_bounds({x,y}))cells.push_back({x,y});
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return p::distance(a,origin)<p::distance(b,origin);});
    // Rice is scarce source geography. Its normal World purchase must first
    // accept actual fertility100; no Pump, terrain or permission is authored.
    for(const unsigned crop:{2U,0U,3U,4U}) {
        bool placed=false;
        for(const auto cell:cells) {
            sim::Command command{crop_commands.at(crop),cell};command.building_id=plan.farm;
            if(!world.validate(command).accepted)continue;
            if(p::buy(world,plan,command)){placed=true;break;}
        }
        if(!placed)return false;
    }
    return world.find_building_route(plan.farm,plan.market).has_value()&&routes_to_all_fields(world,plan.farm);
}
bool connect(sim::World& world,p::Plan& plan,Id body) {
    if(world.find_building_route(body,plan.market))return true;
    std::vector<sim::Cell> goals;
    for(const auto cell:world.building_cells(body))for(const sim::Cell delta:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}) {
        const sim::Cell goal{cell.x+delta.x,cell.y+delta.y};
        if((existing_road(world,goal)||world.validate({sim::CommandType::PlaceRoad,goal}).accepted)&&
            std::find(goals.begin(),goals.end(),goal)==goals.end())goals.push_back(goal);
    }
    for(const auto goal:goals) {
        auto starts=plan.roads;std::stable_sort(starts.begin(),starts.end(),[&](auto a,auto b){return p::distance(a,goal)<p::distance(b,goal);});
        if(starts.size()>12)starts.resize(12);
        for(const auto start:starts)for(const auto& cells:p::road_options(start,goal)) {
            if(cells.empty()||cells.size()>192)continue;
            auto candidate=world;auto proposed=plan;bool legal=true;
            for(const auto cell:cells)if(!existing_road(candidate,cell)&&!p::buy(candidate,proposed,{sim::CommandType::PlaceRoad,cell})) {legal=false;break;}
            if(legal&&candidate.find_building_route(body,plan.market)&&routes_to_all_food(candidate)&&routes_to_all_plants(candidate)) {
                world=std::move(candidate);plan=std::move(proposed);return true;
            }
        }
    }
    return false;
}
struct Scene {
    p::Plan paid;std::optional<Id> hemp,warehouse,elite;
    std::array<std::optional<Id>,3> plantations;
    Id pump{};std::uint8_t orientation=0;
    sim::Cell control_cut{},event_cut{};
    std::vector<sim::Cell> network;
    std::vector<sim::Command> recovery;
    Json joint_planning=Json::object();
};
bool future_elite_clear(const sim::World&,const Scene&) {return true;}
bool outside_elite(const sim::World&,const Scene&,sim::Object,sim::Cell) {return true;}
std::vector<sim::Cell> nearby(const sim::World& world,const p::Plan& plan) {
    std::vector<sim::Cell> cells;
    for(const auto road:plan.roads)for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx) {
        const sim::Cell cell{road.x+dx,road.y+dy};if(world.in_bounds(cell))cells.push_back(cell);
    }
    std::sort(cells.begin(),cells.end(),[](auto a,auto b){return a.y!=b.y?a.y<b.y:a.x<b.x;});cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return road_distance(plan,a)<road_distance(plan,b);});return cells;
}
void add_plantation(sim::World& world,Scene& scene,unsigned kind) {
    constexpr std::array source_commands{sim::CommandType::PlaceOriginalTeaCuringShed,
        sim::CommandType::PlaceOriginalLacquerRefinery,sim::CommandType::PlaceOriginalSilkwormShed};
    constexpr std::array source_kinds{sim::Object::OriginalTeaCuringShed237,
        sim::Object::OriginalLacquerRefinery238,sim::Object::OriginalSilkwormShed239};
    constexpr std::array plant_commands{sim::CommandType::PlaceOriginalTeaBush,
        sim::CommandType::PlaceOriginalLacquerTree,sim::CommandType::PlaceOriginalMulberryTree};
    constexpr std::array plant_kinds{sim::Object::OriginalTeaBush26,
        sim::Object::OriginalLacquerTree27,sim::Object::OriginalMulberryTree28};
    unsigned attempts=0;auto cells=nearby(world,scene.paid);
    if(scene.elite) {const auto center=world.building(*scene.elite).cell;
        std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return p::distance(a,center)<p::distance(b,center);});}
    for(const auto cell:cells) {
        sim::Command command{source_commands.at(kind),cell};
        if(!outside_food_domain(world,scene.paid.farm,source_kinds.at(kind),cell)||
            !outside_elite(world,scene,source_kinds.at(kind),cell)||!world.validate(command).accepted)continue;
        if(++attempts>32)break;
        auto candidate=world;auto proposed=scene.paid;if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(cell);bool plant=false;
        for(int y=cell.y-3;y<=cell.y+4&&!plant;++y)for(int x=cell.x-3;x<=cell.x+4&&!plant;++x) {
            sim::Command bush{plant_commands.at(kind),{x,y}};bush.building_id=source;
            if(!outside_food_domain(candidate,proposed.farm,plant_kinds.at(kind),bush.cell)||
                !outside_elite(candidate,scene,plant_kinds.at(kind),bush.cell)||!candidate.validate(bush).accepted)continue;
            auto planted=candidate;auto paid=proposed;
            if(p::buy(planted,paid,bush)&&routes_to_all_plants(planted)&&connect(planted,paid,source)&&future_elite_clear(planted,scene)) {
                candidate=std::move(planted);proposed=std::move(paid);plant=true;
            }
        }
        if(plant) {world=std::move(candidate);scene.paid=std::move(proposed);scene.plantations.at(kind)=source;break;}
    }
    scene.joint_planning["plantations"][kind]={{"attempts",attempts},{"planned",scene.plantations.at(kind).has_value()}};
}
void add_hemp(sim::World& world,Scene& scene) {
    unsigned attempts=0;auto cells=nearby(world,scene.paid);
    if(scene.elite) {const auto center=world.building(*scene.elite).cell;
        std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return p::distance(a,center)<p::distance(b,center);});}
    for(const auto cell:cells) {
        const sim::Command command{sim::CommandType::PlaceOriginalHempFarm,cell};
        if(!outside_food_domain(world,scene.paid.farm,sim::Object::OriginalHempFarm,cell)||
            !outside_elite(world,scene,sim::Object::OriginalHempFarm,cell)||!world.validate(command).accepted)continue;
        if(++attempts>32)break;
        auto candidate=world;auto proposed=scene.paid;
        if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(cell);bool planted=false;
        for(int y=cell.y-1;y<=cell.y+2&&!planted;++y)for(int x=cell.x-1;x<=cell.x+2&&!planted;++x) {
            sim::Command field{sim::CommandType::PlaceOriginalHempField,{x,y}};field.building_id=source;
            if(!outside_food_domain(candidate,proposed.farm,sim::Object::OriginalHempField,field.cell)||
                !outside_elite(candidate,scene,sim::Object::OriginalHempField,field.cell)||!candidate.validate(field).accepted)continue;
            auto planted_world=candidate;auto paid=proposed;
            if(p::buy(planted_world,paid,field)&&connect(planted_world,paid,source)&&future_elite_clear(planted_world,scene)) {
                candidate=std::move(planted_world);proposed=std::move(paid);planted=true;
            }
        }
        if(planted) {world=std::move(candidate);scene.paid=std::move(proposed);scene.hemp=source;break;}
    }
    scene.joint_planning["hemp"]={{"attempts",attempts},{"planned",scene.hemp.has_value()}};
}
Json balances(const sim::World& world) {
    l::check(world.rule_version()==33&&world.physical_food_growers_state()&&world.hunting_food_state()&&
        !world.original_agriculture_state()&&!world.fishing_food_state()&&world.original_production_state()->food==sim::OriginalFoodState{},
        "Physical Grower proof lost sole seven-kind Food authority");
    p::invariant(world);sim::HuntingFoodCounters harvested{},returned_crop{},grower{},held{},cart{},used{},departed{},received{},returned_cart{},origin{};
    for(const auto& field:world.hunting_food_state()->fields)harvested[static_cast<unsigned>(field.crop)]+=field.harvested;
    for(const auto& field:world.physical_food_growers_state()->fields) {
        const auto actual=std::find_if(world.hunting_food_state()->fields.begin(),world.hunting_food_state()->fields.end(),[&](const auto& row){return row.field_id==field.field_id;});
        l::check(actual!=world.hunting_food_state()->fields.end(),"Receipt lost paid crop identity");returned_crop[static_cast<unsigned>(actual->crop)]+=field.returned;
    }
    for(const auto& farm:world.physical_food_growers_state()->farms)if(farm.latest_trip) {
        const auto amount=sim::physical_food_trip_cargo(*farm.latest_trip);
        l::check(world.physical_food_grower_cargo(farm.courier_id)==amount,"Physical cargo query differs from sole trip authority");
        grower[static_cast<unsigned>(farm.latest_trip->crop)]+=static_cast<std::uint64_t>(amount);
    }
    origin=returned_crop;
    for(const auto& quay:world.fishing_state()->quays)origin[5]+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)origin[6]+=tent.unloaded;
    for(const auto& stock:world.hunting_food_state()->stocks)for(std::size_t g=0;g<7;++g) {
        l::check(stock.amounts[g]>=0,"Negative sole Food stock");held[g]+=static_cast<std::uint64_t>(stock.amounts[g]);used[g]+=stock.consumed[g];
        departed[g]+=stock.departed[g];received[g]+=stock.received[g];returned_cart[g]+=stock.returned[g];
    }
    for(const auto& trip:world.hunting_food_state()->trips)for(std::size_t g=0;g<7;++g) {
        const auto amount=std::int64_t(trip.loaded[g])-trip.delivered[g]-trip.returned[g];l::check(amount>=0&&amount<=4,"Impossible Food Cart cargo");cart[g]+=static_cast<std::uint64_t>(amount);
    }
    for(std::size_t g=0;g<7;++g) {
        l::check(harvested[g]==returned_crop[g]+grower[g],"Field harvest != actual returned crop + live Grower cargo");
        l::check(origin[g]==held[g]+cart[g]+used[g],"Actual Farm/Fish/Meat receipts != Food held + Cart cargo + consumed");
        l::check(departed[g]==received[g]+returned_cart[g]+cart[g],"Food departures != arrivals + Cart returns + cargo");
    }
    sim::World::validate_irrigation_productivity_snapshot(world.snapshot());
    sim::World::validate_seasonal_agriculture_snapshot(world.snapshot());
    sim::World::validate_physical_food_growers_snapshot(world.snapshot());
    sim::World::validate_hunting_snapshot(world.snapshot());
    for(std::size_t g=0;g<orig::goods_count;++g) {
        const auto ledger=world.original_goods_balance(static_cast<orig::Goods>(g));
        l::check(ledger.supported&&ledger.conserved&&!ledger.totals.imported&&!ledger.totals.demolished,
            "Joint material ledger lost its sole paid physical origin or conservation");
    }
    for(std::size_t g=0;g<sim::original_aux_material_count;++g)
        l::check(world.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(g)).conserved,
            "Joint auxiliary material conservation failed");
    return {{"field_harvested",harvested},{"physical_farm_returned",returned_crop},{"grower_cargo",grower},
        {"accepted_food_origin",origin},{"held",held},{"cart_cargo",cart},{"consumed",used},{"conserved",true}};
}
void checkpoint(const Context& context,const sim::World& world,const fs::path& output,const std::string& name,Json& manifest,
                l::save::RecoveryStore* recovery=nullptr,const std::string& history={}) {
    (void)balances(world);const auto path=output/(name+".save.json");l::check(!fs::exists(path),"Preserve existing checkpoint");
    l::save::write_save(path,context.document(world),context.source.map.data,context.source.map.mask);
    const auto document=l::save::read_save(path);auto restored=context.restore(document);
    l::check(restored.snapshot()==world.snapshot(),"Exact schema49 checkpoint changed physical authority");
    const auto roundtrip=output/(name+".roundtrip.json");l::save::write_save(roundtrip,context.document(restored),context.source.map.data,context.source.map.mask);
    l::check(l::q::f::read_text(path)==l::q::f::read_text(roundtrip),"Canonical schema49 roundtrip bytes differ");
    if(recovery) {
        const auto entry=recovery->write_checkpoint(history,document,context.source.map.mask);
        l::check(entry.loadable()&&entry.schema==49&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery changed physical Grower authority");
    }
    manifest.push_back({{"label",name},{"tick",world.ticks()},{"file",path.filename().string()},{"exact_roundtrip",true},{"exact_recovery",recovery!=nullptr}});
}
struct Options {fs::path data,output,map="Cities/Chengdu.map",replay;unsigned ticks=24000;};
constexpr const char* usage="openemperor-irrigation-productivity-acceptance --data ABS --output FRESH_.local/agri-mac08/SUBDIR [--map Cities/Chengdu.map] [--ticks 24000] [--replay CREATOR_DIRECTORY]";
Options options(int argc,char** argv) {
    Options result;std::map<std::string,std::string> args;
    for(int i=1;i<argc;i+=2) {
        l::check(i+1<argc,usage);const std::string key=argv[i];
        l::check(key=="--data"||key=="--output"||key=="--map"||key=="--ticks"||key=="--replay",usage);
        l::check(args.emplace(key,argv[i+1]).second,"Duplicate acceptance argument");
    }
    l::check(args.contains("--data")&&args.contains("--output"),usage);
    l::no_symlinks(args.at("--data"));result.data=fs::canonical(args.at("--data"));
    if(args.contains("--map"))result.map=args.at("--map");
    l::check(!result.map.is_absolute()&&result.map==result.map.lexically_normal()&&result.map.parent_path()=="Cities"&&result.map.extension()==".map",
        "Select one explicit supplied Cities/name.map without traversal");
    l::no_symlinks(result.data/result.map);
    l::check(fs::is_regular_file(result.data/result.map)&&fs::file_size(result.data/result.map)<=64U*1024U*1024U,
        "A bounded regular supplied map is required");
    result.output=p::output_root(args.at("--output"),result.data,".local/agri-mac08");
    if(args.contains("--ticks")) {
        const auto& text=args.at("--ticks");const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result.ticks);
        l::check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size(),"Invalid tick count");
    }
    l::check(result.ticks>=20000&&result.ticks<=50000,"Require20000..50000 real matched continuation ticks");
    if(args.contains("--replay")) {
        l::no_symlinks(args.at("--replay"));result.replay=fs::canonical(args.at("--replay"));
        l::check(fs::is_directory(result.replay)&&!l::below(result.replay,result.data),"Replay must be an explicit private creator directory");
    }
    return result;
}
void command(sim::World& world,sim::Command cmd,Json& log,const char* reason) {
    const auto tick=world.ticks(),sequence=world.command_sequence();const auto funds=world.treasury();
    const auto decision=world.validate(cmd);const auto result=world.execute(cmd);
    l::check(decision.accepted&&result.accepted,"Actual paid command refused: "+result.reason);
    log.push_back({{"tick",tick},{"command",static_cast<unsigned>(cmd.type)},{"cell",{cmd.cell.x,cmd.cell.y}},
        {"owner",static_cast<unsigned>(cmd.building_id)},{"orientation",cmd.orientation},{"reason",reason},
        {"sequence_before",sequence},{"sequence_after",world.command_sequence()},
        {"funds_before",funds},{"funds_after",world.treasury()},{"changed",result.changed}});
}
std::optional<Id> add_body(sim::World& world,Scene& scene,sim::CommandType type) {
    const auto kind=sim::placed_object(type);l::check(kind.has_value(),"Unknown paid body type");
    unsigned attempts=0;
    for(const auto cell:nearby(world,scene.paid)) {
        if(!outside_food_domain(world,scene.paid.farm,*kind,cell)||!world.validate({type,cell}).accepted)continue;
        if(++attempts>64)break;
        auto next=world;auto plan=scene.paid;
        if(!p::buy(next,plan,{type,cell}))continue;
        const auto id=*next.building_owner_at(cell);
        if(!connect(next,plan,id)||!routes_to_all_food(next)||!routes_to_all_plants(next))continue;
        world=std::move(next);scene.paid=std::move(plan);return id;
    }
    return {};
}
std::vector<sim::Cell> crop_cells(const sim::World& world) {
    std::vector<sim::Cell> result;
    for(const auto& row:world.irrigation_productivity_state()->work)result.push_back(world.building(row.building_id).cell);
    return result;
}
bool in_field_range(sim::Cell field,sim::Cell ditch) {return orig::field_in_irrigation_range(field,ditch);}
bool connect_ditches(sim::World& world,Scene& scene,sim::Cell goal) {
    auto starts=scene.network;
    std::stable_sort(starts.begin(),starts.end(),[&](auto a,auto b){return p::distance(a,goal)<p::distance(b,goal);});
    if(starts.size()>12)starts.resize(12);
    for(const auto start:starts)for(const auto& path:p::road_options(start,goal)) {
        if(path.empty()||path.size()>sim::irrigation_gesture_limit)continue;
        if(!world.validate_irrigation_batch(path).accepted)continue;
        auto next=world;const auto placed=next.execute_irrigation_batch(path);
        if(!placed.accepted||!routes_to_all_food(next)||!routes_to_all_plants(next))continue;
        world=std::move(next);
        for(const auto cell:path)if(std::find(scene.network.begin(),scene.network.end(),cell)==scene.network.end())scene.network.push_back(cell);
        return true;
    }
    return false;
}
bool network_to_crops(sim::World& world,Scene& scene) {
    const auto shape=orig::pump_geometry(world.building(scene.pump).cell,scene.orientation);
    for(const auto seed:shape.seeds)if(world.validate({sim::CommandType::PlaceIrrigationDitch,seed}).accepted) {
        const std::array<sim::Cell,1> first{seed};
        if(world.execute_irrigation_batch(first).accepted){scene.network.push_back(seed);break;}
    }
    if(scene.network.empty())return false;
    auto targets=crop_cells(world);const auto pump_cell=world.building(scene.pump).cell;
    std::stable_sort(targets.begin(),targets.end(),[&](auto a,auto b){return p::distance(a,pump_cell)<p::distance(b,pump_cell);});
    for(const auto field:targets) {
        if(std::any_of(scene.network.begin(),scene.network.end(),[&](auto c){return in_field_range(field,c);}))continue;
        std::vector<sim::Cell> goals;
        for(int y=field.y-5;y<=field.y+5;++y)for(int x=field.x-5;x<=field.x+5;++x) {
            const sim::Cell c{x,y};if(!in_field_range(field,c)||!world.validate({sim::CommandType::PlaceIrrigationDitch,c}).accepted)continue;
            goals.push_back(c);
        }
        std::stable_sort(goals.begin(),goals.end(),[&](auto a,auto b) {
            const auto distance=[&](auto c){int best=10000;for(const auto at:scene.network)best=std::min(best,p::distance(c,at));return best;};
            return distance(a)<distance(b);
        });
        bool connected=false;
        for(const auto goal:goals)if(connect_ditches(world,scene,goal)){connected=true;break;}
        if(!connected)return false;
    }
    if(scene.network.size()<21) {
        const auto first=scene.network.back();bool extended=false;
        for(const sim::Cell d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}) {
            const sim::Cell goal{first.x+24*d.x,first.y+24*d.y};
            if(connect_ditches(world,scene,goal)&&scene.network.size()>=21){extended=true;break;}
        }
        if(!extended)return false;
    }
    return scene.network.size()<=sim::irrigation_gesture_limit;
}
void initialize_staffing(sim::World& world,Json& commands) {
    std::vector<Id> ids;for(const auto& body:world.buildings())if(world.workforce_required(body.id)>0)ids.push_back(body.id);
    for(const auto id:ids) {
        const auto kind=world.building(id).kind;
        const bool basic=kind==sim::Object::Market||kind==sim::Object::OriginalWaterWell72||
            kind==sim::Object::OriginalAncestorShrine214||kind==sim::Object::FireWatch;
        command(world,sim::set_building_operation(id,basic),commands,"Initial paid Water/Market/Ancestor/fire staffing");
        command(world,sim::set_building_workforce_priority(id,sim::WorkforcePriority::High),commands,"Explicit bounded matched staff priority");
    }
}
void advance_staffing(sim::World& world,Json& commands,bool hold_pump=false) {
    std::uint64_t active=0;
    for(const auto& b:world.buildings())if(b.operating_enabled)active+=static_cast<std::uint64_t>(world.workforce_required(b.id));
    constexpr std::array order{sim::Object::OriginalIrrigationPump,sim::Object::OriginalFoodFarm,sim::Object::Warehouse,
        sim::Object::OriginalHempFarm,sim::Object::OriginalTeaCuringShed237,sim::Object::OriginalLacquerRefinery238,sim::Object::OriginalSilkwormShed239};
    std::vector<Id> pending;
    for(const auto kind:order)if(!hold_pump||kind!=sim::Object::OriginalIrrigationPump)
        for(const auto& b:world.buildings())if(b.kind==kind&&!b.operating_enabled)pending.push_back(b.id);
    for(const auto id:pending) {
        const auto staff=static_cast<std::uint64_t>(world.workforce_required(id));
        if(active+staff>static_cast<std::uint64_t>(world.total_population()))return;
        command(world,sim::set_building_operation(id,true),commands,"Enable only after real population covers full source staff");active+=staff;
    }
}
struct PumpCandidate {sim::Cell cell{};std::uint8_t direction=0;};
std::vector<PumpCandidate> pump_candidates(const sim::World& world,sim::Cell center) {
    std::vector<PumpCandidate> result;
    for(int y=1;y+2<world.height();++y)for(int x=1;x+2<world.width();++x)for(std::uint8_t direction=0;direction<4;++direction) {
        const sim::Cell cell{x,y};const auto shape=orig::pump_geometry(cell,direction);
        if(std::any_of(shape.water.begin(),shape.water.end(),[&](auto c){return !world.in_bounds(c)||!world.map_permissions()->bridge_water(c);}))continue;
        sim::Command cmd{sim::CommandType::PlaceIrrigationPump,cell};cmd.orientation=direction;
        if(world.validate(cmd).accepted)result.push_back({cell,direction});
    }
    std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return p::distance(a.cell,center)<p::distance(b.cell,center);});
    return result;
}
bool recovery_plan(const sim::World& world,Scene& scene) {
    const auto origin=world.building(scene.pump).cell;
    for(int radius=2;radius<=8;++radius)for(int y=origin.y-radius;y<=origin.y+radius;++y)for(int x=origin.x-radius;x<=origin.x+radius;++x) {
        const sim::Cell cell{x,y};if(!world.validate({sim::CommandType::PlaceFireWatch,cell}).accepted)continue;
        auto next=world;std::vector<sim::Command> commands{{sim::CommandType::PlaceFireWatch,cell}};
        if(!next.execute(commands.front()).accepted)continue;
        const auto watch=*next.building_owner_at(cell);
        std::vector<sim::Cell> from,to;
        for(const auto body:next.building_cells(watch))for(const sim::Cell d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}) {
            const sim::Cell c{body.x+d.x,body.y+d.y};if(next.validate({sim::CommandType::PlaceRoad,c}).accepted)from.push_back(c);
        }
        for(const auto body:next.building_cells(scene.pump))for(const sim::Cell d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}) {
            const sim::Cell c{body.x+d.x,body.y+d.y};if(next.validate({sim::CommandType::PlaceRoad,c}).accepted)to.push_back(c);
        }
        for(const auto start:from)for(const auto goal:to)for(const auto& path:p::road_options(start,goal)) {
            if(path.empty()||path.size()>24)continue;
            auto connected=next;auto proposed=commands;bool valid=true;
            for(const auto c:path) {
                const sim::Command road{sim::CommandType::PlaceRoad,c};const auto decision=connected.execute(road);
                if(!decision.accepted){valid=false;break;}proposed.push_back(road);
            }
            if(valid&&connected.find_building_route(watch,scene.pump)&&routes_to_all_food(connected)&&routes_to_all_plants(connected)) {
                scene.recovery=std::move(proposed);return true;
            }
        }
    }
    return false;
}
bool probe_network(const sim::World& candidate,Scene& scene) {
    auto wet=candidate;Json discard=Json::array();initialize_staffing(wet,discard);
    for(unsigned i=0;i<1500;++i){advance_staffing(wet,discard);wet.tick();}
    const auto targets=crop_cells(wet);
    if(targets.size()!=9||std::any_of(targets.begin(),targets.end(),[&](auto c){return !wet.irrigation_field_status(c).water_available;}))return false;
    std::vector<sim::Cell> cuts=scene.network;const auto at=wet.building(scene.pump).cell;
    std::stable_sort(cuts.begin(),cuts.end(),[&](auto a,auto b){return p::distance(a,at)<p::distance(b,at);});
    std::vector<sim::Cell> isolated;
    for(const auto cell:cuts) {
        auto dry=wet;if(!dry.execute({sim::CommandType::RemoveIrrigationDitch,cell}).accepted)continue;
        if(std::none_of(targets.begin(),targets.end(),[&](auto c){return dry.irrigation_field_status(c).water_available;}))isolated.push_back(cell);
        if(isolated.size()==2)break;
    }
    if(isolated.size()!=2)return false;
    scene.control_cut=isolated[0];scene.event_cut=isolated[1];
    scene.joint_planning["discarded_geometry_probe"]={{"real_ticks",wet.ticks()},{"population",wet.total_population()},
        {"nine_actual_wet_queries",true},{"dry_control_cut",{scene.control_cut.x,scene.control_cut.y}},
        {"event_cut",{scene.event_cut.x,scene.event_cut.y}},{"creator_tick",candidate.ticks()},
        {"scope","Normally paid copied World only; no resident/work/water transfer"}};
    return true;
}
Scene choose_scene(const sim::World& base,const fs::path& map) {
    auto layout=p::city_blueprints().front();
    std::erase_if(layout.commands,[](const auto& cmd){return cmd.type==sim::CommandType::PlaceFireWatch&&cmd.cell.x==16;});
    std::vector<sim::Cell> origins;
    if(map=="Cities/Chengdu.map")origins.push_back({107,134});
    else origins=p::clear_origins(base,layout);
    if(origins.size()>48)origins.resize(48);
    unsigned attempted_pumps=0;
    for(const auto origin:origins) {
        auto city=base;Scene built;
        if(!p::city(city,built.paid,layout,origin)||!five_fields(city,built.paid))continue;
        built.warehouse=add_body(city,built,sim::CommandType::PlaceWarehouse);if(!built.warehouse)continue;
        add_hemp(city,built);if(!built.hemp)continue;
        bool complete=true;for(unsigned kind=0;kind<3;++kind){add_plantation(city,built,kind);complete&=built.plantations[kind].has_value();}
        if(!complete)continue;
        for(const auto& pump:pump_candidates(city,city.building(built.paid.farm).cell)) {
            if(++attempted_pumps>128)break;
            auto candidate=city;auto scene=built;
            sim::Command cmd{sim::CommandType::PlaceIrrigationPump,pump.cell};cmd.orientation=pump.direction;
            if(!p::buy(candidate,scene.paid,cmd))continue;
            scene.pump=*candidate.building_owner_at(pump.cell);scene.orientation=pump.direction;
            // Keep the natural-fire subject physically unreachable by the
            // existing Watch. The later paid Watch supplies the real recovery.
            bool protected_route=false;
            for(const auto& b:candidate.buildings())if(b.kind==sim::Object::FireWatch)
                protected_route|=candidate.find_building_route(b.id,scene.pump).has_value();
            if(protected_route||!network_to_crops(candidate,scene))continue;
            if(!probe_network(candidate,scene)||!recovery_plan(candidate,scene))continue;
            scene.joint_planning["pump_candidates_attempted"]=attempted_pumps;
            scene.joint_planning["network_cells"]=scene.network.size();
            return scene;
        }
    }
    throw std::runtime_error("Finite paid source geometry could not connect nine crop queries and a natural-fire Pump; no source admission was widened");
}
Json encode_command(const sim::Command& cmd) {
    return {{"type",static_cast<unsigned>(cmd.type)},{"cell",{cmd.cell.x,cmd.cell.y}},
        {"building_id",static_cast<unsigned>(cmd.building_id)},{"orientation",cmd.orientation}};
}
sim::Command decode_command(const Json& j) {
    sim::Command result{static_cast<sim::CommandType>(j.at("type").get<unsigned>()),{j.at("cell")[0].get<int>(),j.at("cell")[1].get<int>()}};
    result.building_id=static_cast<Id>(j.at("building_id").get<unsigned>());result.orientation=j.at("orientation").get<std::uint8_t>();return result;
}
Json encode_scene(const Scene& scene) {
    Json paid=Json::array(),recovery=Json::array(),network=Json::array();
    for(const auto& cmd:scene.paid.commands)paid.push_back(encode_command(cmd));
    for(const auto& cmd:scene.recovery)recovery.push_back(encode_command(cmd));
    for(const auto cell:scene.network)network.push_back({cell.x,cell.y});
    return {{"format","openemperor-irrigation-productivity-plan-v1"},{"paid_commands",paid},{"recovery_commands",recovery},
        {"network",network},{"pump",static_cast<unsigned>(scene.pump)},
        {"control_cut",{scene.control_cut.x,scene.control_cut.y}},{"event_cut",{scene.event_cut.x,scene.event_cut.y}},
        {"diagnostics",scene.joint_planning}};
}
Scene decode_scene(const Json& j) {
    l::check(j.at("format")=="openemperor-irrigation-productivity-plan-v1","Unknown replay plan");
    Scene scene;scene.pump=static_cast<Id>(j.at("pump").get<unsigned>());
    const auto cell=[](const Json& v){return sim::Cell{v.at(0).get<int>(),v.at(1).get<int>()};};
    scene.control_cut=cell(j.at("control_cut"));scene.event_cut=cell(j.at("event_cut"));
    l::check(j.at("paid_commands").size()<=512&&j.at("recovery_commands").size()<=32&&j.at("network").size()<=sim::irrigation_gesture_limit,
        "Replay plan exceeds bounded paid construction");
    for(const auto& c:j.at("paid_commands"))scene.paid.commands.push_back(decode_command(c));
    for(const auto& c:j.at("recovery_commands"))scene.recovery.push_back(decode_command(c));
    for(const auto& c:j.at("network"))scene.network.push_back(cell(c));
    scene.joint_planning=j.at("diagnostics");return scene;
}
void install_network(sim::World& world,const Scene& scene,Json& log) {
    const auto before=world.treasury();const auto result=world.execute_irrigation_batch(scene.network);
    l::check(result.accepted&&result.changed,"Real complete paid ditch gesture was refused: "+result.reason);
    log.push_back({{"tick",world.ticks()},{"kind","ditch_batch"},{"cells",scene.network.size()},
        {"funds_before",before},{"funds_after",world.treasury()},{"reason",result.reason}});
}
struct Sample {
    Id id{},source{};unsigned kind=0;std::uint64_t base=0,wet=0,bonus=0,clip=0,progress=0,picked=0,returned=0,wasted=0,harvest_work=0;
    std::optional<std::uint64_t> ready;int cargo=0,stock=0,workers=0,required=0;bool water=false;
};
std::array<Sample,9> samples(const sim::World& world) {
    std::array<Sample,9> result{};std::array<bool,9> seen{};
    for(const auto& row:world.irrigation_productivity_state()->work) {
        Sample v;v.id=row.building_id;const auto& body=world.building(v.id);
        const auto status=world.irrigation_productivity_status(v.id);
        v.base=status.base_work_ticks;v.wet=row.wet_work_ticks;v.bonus=row.bonus_work;v.clip=row.clipped_bonus;
        v.progress=status.progress;v.water=status.water_available;
        if(const auto food=std::find_if(world.hunting_food_state()->fields.begin(),world.hunting_food_state()->fields.end(),[&](const auto& f){return f.field_id==v.id;});food!=world.hunting_food_state()->fields.end()) {
            v.kind=static_cast<unsigned>(food->crop);v.source=food->farm_id;v.picked=food->harvested;
            for(const auto& f:world.physical_food_growers_state()->fields)if(f.field_id==v.id)v.returned=f.returned;
            for(const auto& f:world.seasonal_agriculture_state()->fields)if(f.field_id==v.id){v.ready=f.ready_tick;v.wasted=f.wasted_work;v.harvest_work=f.harvest_work;}
            const auto& source=worker_source(world,v.source);
            if(source.latest_trip&&source.latest_trip->field_id==v.id)v.cargo=sim::physical_food_trip_cargo(*source.latest_trip);
            for(const auto& stock:world.hunting_food_state()->stocks)if(stock.building_id==v.source)v.stock=stock.amounts.at(v.kind);
        } else if(body.kind==sim::Object::OriginalHempField) {
            v.kind=5;
            for(const auto& f:world.original_production_state()->hemp_farms)if(f.field_id==v.id){v.source=f.farm_id;v.returned=f.harvested;}
            for(const auto& f:world.seasonal_agriculture_state()->hemp_farms)if(f.farm_id==v.source) {
                v.picked=f.picked;v.ready=f.ready_tick;v.wasted=f.wasted_work;
                if(f.latest_trip)v.cargo=sim::seasonal_hemp_cargo(*f.latest_trip);
            }
            for(const auto& stock:world.original_goods_state()->stocks)if(stock.building_id==v.source)v.stock=stock.amounts[static_cast<unsigned>(orig::Goods::Hemp)];
        } else {
            v.kind=body.kind==sim::Object::OriginalTeaBush26?6U:body.kind==sim::Object::OriginalLacquerTree27?7U:8U;
            for(const auto& plant:world.plantations_state()->plants)if(plant.plant_id==v.id) {
                v.source=plant.source_id;v.picked=plant.harvested;v.returned=plant.returned;
                for(const auto& source:world.plantations_state()->sources)if(source.building_id==v.source&&source.latest_trip&&source.latest_trip->plant_id==v.id)
                    v.cargo=source.latest_trip->picked_tick&&!source.latest_trip->returned_tick?1:0;
            }
            for(const auto& plant:world.seasonal_agriculture_state()->plants)if(plant.plant_id==v.id){v.ready=plant.ready_tick;v.wasted=plant.wasted_work;}
            if(v.kind==6)for(const auto& stock:world.original_goods_state()->stocks)if(stock.building_id==v.source)v.stock=stock.amounts[static_cast<unsigned>(orig::Goods::Tea)];
            if(v.kind>=7)for(const auto& stock:world.original_production_state()->industry.auxiliary)if(stock.building_id==v.source)
                v.stock=stock.amounts[static_cast<unsigned>(v.kind==7?sim::OriginalAuxMaterial::Lacquer:sim::OriginalAuxMaterial::RawSilk)];
        }
        l::check(v.kind<9&&!seen[v.kind]&&v.source!=Id{},"Matched cohort requires exactly one paid identity for each of nine crop types");
        seen[v.kind]=true;v.workers=world.workers_assigned(v.source);v.required=world.workforce_required(v.source);result[v.kind]=v;
    }
    l::check(std::all_of(seen.begin(),seen.end(),[](bool v){return v;}),"Matched crop cohort is incomplete");return result;
}
Json sample_json(const std::array<Sample,9>& rows) {
    Json result=Json::array();
    for(const auto& v:rows)result.push_back(Json::array({static_cast<unsigned>(v.id),v.kind,v.base,v.wet,v.bonus,v.clip,v.progress,
        v.ready?Json(*v.ready):Json(nullptr),v.picked,v.returned,v.cargo,v.water,v.workers,v.required,v.stock,v.wasted,v.harvest_work}));
    return result;
}
struct Observation {
    std::array<std::optional<std::uint64_t>,9> first_ready_tick{},first_ready_base{},first_return_tick{};
    std::array<std::uint64_t,9> useful_wet_ticks{},shared_base_ticks{},increased_increment_ticks{};
    Json receipts=Json::array();
    void after(const std::array<Sample,9>& before,const std::array<Sample,9>& after,std::uint64_t tick,bool dry) {
        for(unsigned k=0;k<9;++k) {
            const auto& a=before[k];const auto& b=after[k];
            l::check(a.id==b.id&&b.base>=a.base&&b.base-a.base<=1&&b.wet>=a.wet&&b.bonus>=a.bonus&&b.clip>=a.clip,
                "Actual base/wet/bonus chronology is not monotone bounded work");
            const auto base=b.base-a.base,wet=b.wet-a.wet,bonus=b.bonus-a.bonus,clip=b.clip-a.clip;
            l::check(wet==(a.water?base:0),"Wet work disagrees with actual pre-tick water and real base work");
            l::check(b.bonus+b.clip==b.wet/4&&bonus+clip==(b.wet/4-a.wet/4),"Integer carry emitted a doubled or missing bonus");
            l::check(!dry||(b.wet==0&&b.bonus==0&&b.clip==0),"Disconnected dry control earned wet work");
            useful_wet_ticks[k]+=wet;
            if(!first_ready_tick[k]&&b.ready){first_ready_tick[k]=tick;first_ready_base[k]=b.base;}
            if(b.returned>a.returned) {
                if(!first_return_tick[k])first_return_tick[k]=tick;
                receipts.push_back({{"tick",tick},{"kind",k},{"building_id",static_cast<unsigned>(b.id)},
                    {"amount",b.returned-a.returned},{"cumulative",b.returned}});
            }
        }
    }
};
std::size_t supplied(const sim::World& world) {
    return static_cast<std::size_t>(std::count_if(world.irrigation_state()->ditches.begin(),world.irrigation_state()->ditches.end(),[&](const auto& d){return world.irrigation_ditch_wet(d.cell);}));
}
bool all_returned(const std::array<Sample,9>& rows) {return std::all_of(rows.begin(),rows.end(),[](const auto& v){return v.returned>0;});}
bool cargo_present(const std::array<Sample,9>& rows) {return std::any_of(rows.begin(),rows.end(),[](const auto& v){return v.cargo>0;});}
struct Controller {
    bool recovery_paid=false,extinguished=false,fire_rewet=false,cut=false,reconnected=false,reconnected_wet=false,paused=false,resumed=false,resumed_wet=false;
    std::optional<std::uint64_t> fire_tick,cut_tick,reconnected_tick,pause_tick,resume_tick;
    Json events=Json::array();
};
Json scenario_state(const sim::World& world,Id pump) {
    const auto status=world.irrigation_pump_status(pump);
    return {{"tick",world.ticks()},{"population",world.total_population()},{"funds",world.treasury()},
        {"buildings",world.buildings().size()},{"movement_channels",world.couriers().size()+world.fishing_state()->quays.size()+
            world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size()},
        {"supplied_ditch_cells",supplied(world)},{"pump_burning",world.building_on_fire(pump)},
        {"pump_workers",status.assigned_workers},{"pump_supplying",status.supplying},
        {"pump_phase_tick",status.last_phase_tick?Json(*status.last_phase_tick):Json(nullptr)},
        {"pump_eligible_since_tick",status.eligible_since_tick?Json(*status.eligible_since_tick):Json(nullptr)}};
}
} // namespace
int main(int argc,char** argv) {
    fs::path output;std::uint64_t completed=0;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto selected=options(argc,argv);output=selected.output;
        l::SourceGuard sources(selected.data);ReplayGuard map_input(selected.data/selected.map);
        Context context(selected.data,selected.map);
        l::check(context.source.model->provenance().sha256==l::base::model_sha,
            "Matched proof requires the unchanged supplied Normal model");
        std::vector<ReplayGuard> inputs;
        if(!selected.replay.empty())for(const auto* name:{"plan.json","dry-seed.save.json","wet-seed.save.json","dry-final.save.json","wet-final.save.json"}) {
            const auto path=selected.replay/name;l::no_symlinks(path);
            l::check(fs::is_regular_file(path)&&fs::file_size(path)<=8U*1024U*1024U,"Replay requires bounded explicit paid evidence files");
            inputs.emplace_back(path);
        }
        const auto scene=selected.replay.empty()?choose_scene(context.create(),selected.map):decode_scene(Json::parse(inputs[0].bytes));
        const auto plan=encode_scene(scene);l::write(output/"plan.json",plan);
        std::array<sim::World,2> worlds{context.create(),context.create()};
        std::array<Json,2> commands{Json::array(),Json::array()};
        constexpr std::array<const char*,2> labels{"dry","wet"};Json manifest=Json::array();
        for(unsigned mode=0;mode<2;++mode) {
            for(const auto& cmd:scene.paid.commands)command(worlds[mode],cmd,commands[mode],"Replay every ordinary paid source-model construction");
            install_network(worlds[mode],scene,commands[mode]);initialize_staffing(worlds[mode],commands[mode]);
            if(mode==0)command(worlds[mode],{sim::CommandType::RemoveIrrigationDitch,scene.control_cut},commands[mode],"Matched dry control: remove one real upstream paid ditch");
            l::check(worlds[mode].ticks()==0&&worlds[mode].total_population()==0,"Geometry probe transferred work or residents into the real seed");
            if(!selected.replay.empty()) {
                auto restored=context.restore(l::save::read_save(selected.replay/(std::string(labels[mode])+"-seed.save.json")));
                l::check(restored.snapshot()==worlds[mode].snapshot(),"Replay seed differs from independently reconstructed paid commands");
                worlds[mode]=std::move(restored);
            }
            checkpoint(context,worlds[mode],output,std::string(labels[mode])+"-seed",manifest);
        }
        std::array<sim::World,2> restored{context.restore(l::save::read_save(output/"dry-seed.save.json")),context.restore(l::save::read_save(output/"wet-seed.save.json"))};
        l::save::RecoveryStore recovery(output/"recovery",selected.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
        std::array<std::string,2> histories{
            recovery.create_history(l::save::read_save(output/"dry-seed.save.json"),context.source.map.mask),
            recovery.create_history(l::save::read_save(output/"wet-seed.save.json"),context.source.map.mask)};
        std::ofstream trace(output/"paired-work.jsonl");l::check(bool(trace),"Cannot open bounded tick-by-tick audit trace");
        std::array<Observation,2> observed;Controller control;std::set<std::string> captured;
        auto minimum_funds=std::min(worlds[0].treasury(),worlds[1].treasury());std::size_t maximum_supplied=0;
        const auto capture=[&](const std::string& name) {
            if(!captured.insert(name).second)return;
            for(unsigned mode=0;mode<2;++mode) {
                checkpoint(context,worlds[mode],output,std::string(labels[mode])+"-"+name,manifest,&recovery,histories[mode]);
                auto checked=context.restore(l::save::read_save(output/(std::string(labels[mode])+"-"+name+".save.json")));
                l::check(checked.snapshot()==worlds[mode].snapshot(),"Transition restore changed authority");
                restored[mode]=std::move(checked);
            }
        };
        const auto apply=[&](sim::Command cmd,const char* reason) {
            for(unsigned mode=0;mode<2;++mode) {
                Json repeat=Json::array();command(worlds[mode],cmd,commands[mode],reason);command(restored[mode],cmd,repeat,reason);
                l::check(commands[mode].back()==repeat.back(),"Restored transition command diverged");
            }
        };
        while(worlds[1].ticks()<selected.ticks) {
            const auto tick=worlds[1].ticks();const auto current=samples(worlds[1]);
            if(!control.recovery_paid&&worlds[1].building_on_fire(scene.pump)) {
                control.fire_tick=tick;control.events.push_back({{"type","natural-pump-fire"},{"state",scenario_state(worlds[1],scene.pump)}});
                capture("pump-burning");
                for(const auto& cmd:scene.recovery)apply(cmd,"Pay a new Watch and physical recovery roads after the genuine Pump fire");
                control.recovery_paid=true;capture("recovery-paid");
            }
            if(control.recovery_paid&&!control.extinguished)for(const auto& arrival:worlds[1].original_production_state()->fire_protection->arrivals)
                if(arrival.trip.target==scene.pump&&arrival.extinguished&&arrival.tick>*control.fire_tick) {
                    control.extinguished=true;control.events.push_back({{"type","actual-inspector-extinguish"},{"arrival_tick",arrival.tick},
                        {"courier",static_cast<unsigned>(arrival.trip.courier_id)},{"paid_watch",static_cast<unsigned>(arrival.trip.owner)},
                        {"state",scenario_state(worlds[1],scene.pump)}});capture("pump-extinguished-before-phase");break;
                }
            if(control.extinguished&&!control.fire_rewet&&supplied(worlds[1])>=21) {
                control.fire_rewet=true;control.events.push_back({{"type","post-fire-phase27-supply"},{"state",scenario_state(worlds[1],scene.pump)}});capture("post-fire-wet");
            }
            if(control.fire_rewet&&!control.cut&&tick>=6000&&all_returned(current)&&cargo_present(current)) {
                capture("before-cut-with-cargo");apply({sim::CommandType::RemoveIrrigationDitch,scene.event_cut},"Cut real supplied network while genuine Grower cargo remains owned");
                control.cut=true;control.cut_tick=tick;control.events.push_back({{"type","paid-ditch-cut"},{"state",scenario_state(worlds[1],scene.pump)}});capture("network-cut-dry");
            }
            if(control.cut&&!control.reconnected&&tick>=*control.cut_tick+99) {
                apply({sim::CommandType::PlaceIrrigationDitch,scene.event_cut},"Pay for the missing ditch; renewed water must await the existing phase27");
                control.reconnected=true;control.reconnected_tick=tick;
                control.events.push_back({{"type","paid-ditch-reconnect"},{"state",scenario_state(worlds[1],scene.pump)}});capture("reconnected-before-phase");
            }
            if(control.reconnected&&!control.reconnected_wet&&supplied(worlds[1])>=21) {
                control.reconnected_wet=true;control.events.push_back({{"type","reconnected-phase27-supply"},{"state",scenario_state(worlds[1],scene.pump)}});capture("reconnected-wet");
            }
            if(control.reconnected_wet&&!control.paused&&tick>=*control.reconnected_tick+200) {
                apply(sim::set_building_operation(scene.pump,false),"Real Pump operation-off immediately ends new wet work");
                control.paused=true;control.pause_tick=tick;control.events.push_back({{"type","pump-paused"},{"state",scenario_state(worlds[1],scene.pump)}});capture("pump-paused-dry");
            }
            if(control.paused&&!control.resumed&&tick>=*control.pause_tick+50) {
                apply(sim::set_building_operation(scene.pump,true),"Restart paid Pump; no water before a later existing phase27");
                control.resumed=true;control.resume_tick=tick;control.events.push_back({{"type","pump-restarted"},{"state",scenario_state(worlds[1],scene.pump)}});capture("restarted-before-phase");
            }
            if(control.resumed&&!control.resumed_wet&&supplied(worlds[1])>=21) {
                control.resumed_wet=true;control.events.push_back({{"type","restarted-phase27-supply"},{"state",scenario_state(worlds[1],scene.pump)}});capture("restarted-wet");
            }
            for(unsigned mode=0;mode<2;++mode) {
                Json issued=Json::array(),repeated=Json::array();
                advance_staffing(worlds[mode],issued,control.paused&&!control.resumed);
                advance_staffing(restored[mode],repeated,control.paused&&!control.resumed);
                l::check(issued==repeated,"Restored natural staffing commands diverged");
                for(const auto& entry:issued)commands[mode].push_back(entry);
            }
            const std::array before{samples(worlds[0]),samples(worlds[1])};
            for(unsigned mode=0;mode<2;++mode){worlds[mode].tick();restored[mode].tick();}
            const std::array after{samples(worlds[0]),samples(worlds[1])};completed=worlds[1].ticks();
            for(unsigned mode=0;mode<2;++mode) {
                observed[mode].after(before[mode],after[mode],completed,mode==0);
                minimum_funds=std::min(minimum_funds,worlds[mode].treasury());
            }
            for(unsigned k=0;k<9;++k)if(after[0][k].base>before[0][k].base&&after[1][k].base>before[1][k].base&&
                before[0][k].workers==before[0][k].required&&before[1][k].workers==before[1][k].required) {
                ++observed[1].shared_base_ticks[k];
                if(after[1][k].bonus>before[1][k].bonus)++observed[1].increased_increment_ticks[k];
            }
            maximum_supplied=std::max(maximum_supplied,supplied(worlds[1]));
            trace<<Json({{"tick",completed},{"dry_before",sample_json(before[0])},{"wet_before",sample_json(before[1])},
                {"dry_after",sample_json(after[0])},{"wet_after",sample_json(after[1])}}).dump()<<'\n';
            if(maximum_supplied>=21)capture("supplied-network");
            if(std::any_of(after[1].begin(),after[1].end(),[](const auto& v){return v.wet%4==3&&v.progress>0&&v.progress<80;}))capture("wet-carry-three");
            if(std::any_of(after[1].begin(),after[1].begin()+5,[](const auto& v){return v.cargo>0;}))capture("food-grower-loaded");
            if(after[1][5].cargo>0)capture("hemp-grower-loaded");
            if(std::any_of(after[1].begin()+6,after[1].end(),[](const auto& v){return v.cargo>0;}))capture("plantation-grower-loaded");
            if(completed==sim::seasonal_year_ticks-1)capture("year-before");
            if(completed==sim::seasonal_year_ticks)capture("year-after");
            if(completed%816==0) {
                for(unsigned mode=0;mode<2;++mode){(void)balances(worlds[mode]);l::check(worlds[mode].snapshot()==restored[mode].snapshot(),"Independent restored continuation diverged");}
                sources.unchanged();map_input.unchanged();for(const auto& input:inputs)input.unchanged();
            }
        }
        trace.close();l::check(bool(trace),"Tick-by-tick progress/inventory trace did not close successfully");
        capture("final");bool independent=!selected.replay.empty();
        for(unsigned mode=0;mode<2;++mode)if(independent) {
            const auto name=std::string(labels[mode])+"-final.save.json";
            l::check(context.restore(l::save::read_save(selected.replay/name)).snapshot()==worlds[mode].snapshot(),"Separate process endpoint differs");
            l::check(l::q::f::read_text(selected.replay/name)==l::q::f::read_text(output/name),"Separate process canonical final bytes differ");
        }
        bool positive=minimum_funds>=0&&maximum_supplied>=21&&control.extinguished&&control.fire_rewet&&control.reconnected_wet&&control.resumed_wet;
        const auto dry=samples(worlds[0]),wet=samples(worlds[1]);Json rows=Json::array();
        for(unsigned k=0;k<9;++k) {
            positive&=dry[k].returned>0&&wet[k].returned>0&&wet[k].wet>0&&wet[k].bonus>0&&
                observed[1].shared_base_ticks[k]>0&&observed[1].increased_increment_ticks[k]>0;
            positive&=observed[0].first_ready_base[k]&&observed[1].first_ready_base[k]&&
                *observed[1].first_ready_base[k]<*observed[0].first_ready_base[k];
            rows.push_back({{"kind",kind_names[k]},{"building_id",static_cast<unsigned>(wet[k].id)},
                {"actual_returns_dry",dry[k].returned},{"actual_returns_wet",wet[k].returned},{"actual_base_work_dry",dry[k].base},
                {"actual_base_work_wet",wet[k].base},{"wet_work_ticks",wet[k].wet},{"bonus_work",wet[k].bonus},{"clipped_bonus",wet[k].clip},
                {"matched_fully_staffed_base_ticks",observed[1].shared_base_ticks[k]},
                {"measured_increased_work_increment_ticks",observed[1].increased_increment_ticks[k]},
                {"first_ready_base_dry",observed[0].first_ready_base[k]?Json(*observed[0].first_ready_base[k]):Json(nullptr)},
                {"first_ready_base_wet",observed[1].first_ready_base[k]?Json(*observed[1].first_ready_base[k]):Json(nullptr)},
                {"first_return_tick_dry",observed[0].first_return_tick[k]?Json(*observed[0].first_return_tick[k]):Json(nullptr)},
                {"first_return_tick_wet",observed[1].first_return_tick[k]?Json(*observed[1].first_return_tick[k]):Json(nullptr)}});
        }
        for(const auto* required:{"supplied-network","wet-carry-three","food-grower-loaded","hemp-grower-loaded","plantation-grower-loaded",
            "year-before","year-after","pump-burning","recovery-paid","pump-extinguished-before-phase","post-fire-wet","before-cut-with-cargo",
            "network-cut-dry","reconnected-before-phase","reconnected-wet","pump-paused-dry","restarted-before-phase","restarted-wet"})positive&=captured.contains(required);
        sources.unchanged();map_input.unchanged();for(const auto& input:inputs)input.unchanged();
        Json conservation=Json::array();for(const auto& world:worlds)conservation.push_back(balances(world));
        const auto status=positive?"PASS":"PARTIAL";
        Json report={{"format","openemperor-irrigation-productivity-acceptance-v1"},{"status",status},{"rule",33},{"schema",49},
            {"map",selected.map.generic_string()},{"map_sha256",context.source.terrain->source_map_sha256()},
            {"model_sha256",context.source.model->provenance().sha256},{"unchanged_supplied_model",true},
            {"source_bytes_mode_mtime_unchanged",true},{"snapshot_stock_or_population_injection",false},
            {"real_ticks",completed},{"maximum_supplied_ditch_cells",maximum_supplied},{"minimum_funds",minimum_funds},
            {"nine_type_comparison",rows},{"pump_transition_events",control.events},{"actual_dry_returns",observed[0].receipts},
            {"actual_wet_returns",observed[1].receipts},{"physical_conservation",conservation},{"checkpoints",manifest},
            {"final_scenes",Json::array({scenario_state(worlds[0],scene.pump),scenario_state(worlds[1],scene.pump)})},
            {"work_trace","paired-work.jsonl"},{"trace_columns",Json::array({"building_id","kind","base","wet","bonus","clipped","progress","ready_tick","picked","returned","cargo","water_available","source_workers","source_required","source_stock","wasted","harvest_work"})},
            {"recovery_exact",true},{"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",independent},
            {"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},
            {"claim_scope","AUTHORED rational work bonus from actual pre-tick supplied water only. Physical crop work, harvest labor, cargo and real source returns remain separate; no Original yield/runtime claim."}};
        for(unsigned mode=0;mode<2;++mode)l::write(output/(std::string(labels[mode])+"-commands.json"),commands[mode]);
        l::write(output/"report.json",report);
        std::cout<<status<<" Rule33/schema49 matched paid dry/wet work; ticks="<<completed<<" report="<<output/"report.json"<<'\n';return positive?0:2;
    } catch(const std::exception& error) {
        if(!output.empty()&&!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()},{"last_completed_tick",completed}});}catch(...){}
        std::cerr<<"Irrigation productivity acceptance FAIL: "<<error.what()<<'\n';return 1;
    }
}
