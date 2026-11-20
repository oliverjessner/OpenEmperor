// Supplied-data proof through paid production World commands only. The shared
// helpers' synthetic Fixture types are never instantiated by this executable.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "FoodNativePlaytest.h"

#include <charconv>
#include <chrono>
#include <map>
#include <set>

namespace l=oe_luxury_progression;
namespace p=oe_food_playtest;
namespace {
using Id=sim::BuildingId;
constexpr const char* usage="openemperor-physical-food-growers-acceptance --data ABS --output FRESH_.local/agri-mac06/SUBDIR [--map Cities/Xia.map] [--ticks 14000] [--replay EXACT47_SAVE --expected CREATOR_FINAL_SAVE]";
constexpr std::array crop_commands{sim::CommandType::PlaceOriginalWheatField,sim::CommandType::PlaceOriginalMilletField,
    sim::CommandType::PlaceOriginalRiceField,sim::CommandType::PlaceOriginalCabbageField,sim::CommandType::PlaceOriginalSoybeanField};
struct Options {fs::path data,output,replay,expected,map="Cities/Xia.map";unsigned ticks=14000;};
Options options(int argc,char** argv) {
    Options result;std::map<std::string,std::string> arguments;
    for(int i=1;i<argc;i+=2) {
        l::check(i+1<argc,usage);const std::string key=argv[i];
        l::check(key=="--data"||key=="--output"||key=="--map"||key=="--ticks"||key=="--replay"||key=="--expected",usage);
        l::check(arguments.emplace(key,argv[i+1]).second,"Duplicate acceptance argument");
    }
    l::check(arguments.contains("--data")&&arguments.contains("--output"),usage);
    l::no_symlinks(arguments.at("--data"));result.data=fs::canonical(arguments.at("--data"));
    if(arguments.contains("--map"))result.map=arguments.at("--map");
    l::check(!result.map.is_absolute()&&result.map==result.map.lexically_normal()&&result.map.parent_path()=="Cities"&&
        result.map.extension()==".map"&&!result.map.stem().empty(),"Map must be one explicit Cities/name.map without traversal");
    const auto map_path=result.data/result.map;l::no_symlinks(map_path);
    l::check(fs::is_regular_file(map_path)&&fs::file_size(map_path)<=64U*1024U*1024U&&l::below(fs::canonical(map_path),result.data),
        "Selected Original map must be a bounded regular input inside the data root");
    const auto requested=fs::absolute(arguments.at("--output"));l::no_symlinks(requested);result.output=fs::weakly_canonical(requested);
    const auto allowed=fs::weakly_canonical(fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac06");
    l::check(result.output!=allowed&&l::below(result.output,allowed)&&!l::below(result.output,result.data)&&
        !l::below(result.data,result.output)&&!fs::exists(result.output),"Use a fresh private .local/agri-mac06 subdirectory disjoint from Original data");
    l::save::validate_save_target(result.output/"final.save.json",result.data);
    for(const auto& [key,destination]:{std::pair{"--replay",&result.replay},{"--expected",&result.expected}})if(arguments.contains(key)) {
        *destination=fs::absolute(arguments.at(key));l::no_symlinks(*destination);
        l::check(fs::is_regular_file(*destination)&&fs::file_size(*destination)<=8U*1024U*1024U&&!l::below(*destination,result.data),
            "Replay requires an explicit bounded save outside Original data");
    }
    l::check(result.replay.empty()==result.expected.empty(),"Replay requires both --replay and --expected");
    if(arguments.contains("--ticks")) {
        const auto& text=arguments.at("--ticks");const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result.ticks);
        l::check(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size(),"Invalid tick count");
    }
    l::check(result.ticks>=12000&&result.ticks<=100000,"Acceptance requires12000..100000 real continuation ticks");return result;
}
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
    sim::World create()const{return sim::World::create_physical_food_growers(source.map.policy,source.model,source.terrain);}
    l::save::SaveDocument document(const sim::World& world)const{return source.document(world);}
    sim::World restore(const l::save::SaveDocument& document)const {
        l::check(document.source_schema_version==47&&document.world.rule_version==31,"Exact Rule31/schema47 input required");
        l::check(document.map_relative==source.map.map_relative,"Replay must retain the explicitly selected source map");
        return l::save::restore_physical_food_growers(document,source.map.data,source.map.mask,source.map.policy,source.model,source.terrain);
    }
};
const sim::PhysicalFoodGrowerFarm& worker_source(const sim::World& world,Id farm) {
    for(const auto& row:world.physical_food_growers_state()->farms)if(row.farm_id==farm)return row;
    throw std::runtime_error("Paid Farm lost its physical Grower source");
}
const sim::HuntingFoodStock& farm_stock(const sim::World& world,Id farm) {
    for(const auto& row:world.hunting_food_state()->stocks)if(row.building_id==farm)return row;
    throw std::runtime_error("Paid Farm lost its sole Food account");
}
void paid_command(sim::World& world,sim::Command command,Json& commands,const char* reason) {
    const auto before=world.snapshot();const auto quote=world.construction_cost(command.type);const auto preview=world.validate(command);
    const auto result=world.execute(command);
    commands.push_back({{"tick",before.ticks},{"command",static_cast<unsigned>(command.type)},{"cell",{command.cell.x,command.cell.y}},
        {"owner_argument",static_cast<unsigned>(command.building_id)},{"reason",reason},{"quoted_cost",quote},
        {"preview_accepted",preview.accepted},{"accepted",result.accepted},{"changed",result.changed},{"result",result.reason},
        {"funds_before",before.treasury},{"funds_after",world.treasury()},
        {"construction_before",before.construction_spent_total},{"construction_after",world.construction_spent_total()},
        {"sequence_before",before.command_sequence},{"sequence_after",world.command_sequence()}});
    const auto detail="command="+std::to_string(static_cast<unsigned>(command.type))+" cell="+std::to_string(command.cell.x)+","+
        std::to_string(command.cell.y)+" quoted="+std::to_string(quote)+" actual_debit="+std::to_string(before.treasury-world.treasury())+" result="+result.reason;
    l::check(preview.accepted==result.accepted,"Paid scene preflight changed: "+detail);
    if(!result.accepted){l::check(world.snapshot()==before,"Refused scene command changed authority: "+detail);throw std::runtime_error("Paid scene refused: "+detail);}
    if(!result.changed) {
        // The common native blueprint also records already present Original
        // roads. An accepted no-op is not a purchase and must change nothing.
        l::check(world.snapshot()==before,"Accepted scene no-op changed authority: "+detail);return;
    }
    l::check(world.ticks()==before.ticks&&world.treasury()==before.treasury-quote&&
        world.construction_spent_total()==before.construction_spent_total+static_cast<std::uint64_t>(quote),
        "Scene construction did not pay its actual source quote: "+detail);
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
            if(legal&&candidate.find_building_route(body,plan.market)&&routes_to_all_fields(candidate,plan.farm)&&routes_to_all_plants(candidate)) {
                world=std::move(candidate);plan=std::move(proposed);return true;
            }
        }
    }
    return false;
}
struct Scene {p::Plan paid;std::optional<Id> quay,tent,plantation;Json joint_planning=Json::object();};
std::vector<sim::Cell> nearby(const sim::World& world,const p::Plan& plan) {
    std::vector<sim::Cell> cells;
    for(const auto road:plan.roads)for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx) {
        const sim::Cell cell{road.x+dx,road.y+dy};if(world.in_bounds(cell))cells.push_back(cell);
    }
    std::sort(cells.begin(),cells.end(),[](auto a,auto b){return a.y!=b.y?a.y<b.y:a.x<b.x;});cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return road_distance(plan,a)<road_distance(plan,b);});return cells;
}
void add_plantation(sim::World& world,Scene& scene) {
    unsigned attempts=0;
    for(const auto cell:nearby(world,scene.paid)) {
        sim::Command command{sim::CommandType::PlaceOriginalTeaCuringShed,cell};
        if(!outside_food_domain(world,scene.paid.farm,sim::Object::OriginalTeaCuringShed237,cell)||!world.validate(command).accepted)continue;
        if(++attempts>32)break;
        auto candidate=world;auto proposed=scene.paid;if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(cell);bool plant=false;
        for(int y=cell.y-3;y<=cell.y+4&&!plant;++y)for(int x=cell.x-3;x<=cell.x+4&&!plant;++x) {
            sim::Command bush{sim::CommandType::PlaceOriginalTeaBush,{x,y}};bush.building_id=source;
            if(!outside_food_domain(candidate,proposed.farm,sim::Object::OriginalTeaBush26,bush.cell)||!candidate.validate(bush).accepted)continue;
            auto planted=candidate;auto paid=proposed;
            if(p::buy(planted,paid,bush)&&routes_to_all_plants(planted)&&connect(planted,paid,source)) {
                candidate=std::move(planted);proposed=std::move(paid);plant=true;
            }
        }
        if(plant) {world=std::move(candidate);scene.paid=std::move(proposed);scene.plantation=source;break;}
    }
    scene.joint_planning["plantation"]={{"attempts",attempts},{"planned",scene.plantation.has_value()}};
}
void add_hunting(sim::World& world,Scene& scene) {
    unsigned attempts=0;auto cells=nearby(world,scene.paid);
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b) {
        const auto score=[&](sim::Cell cell){int nearest=1000;for(const auto& animal:world.hunting_state()->animals)nearest=std::min(nearest,p::distance(cell,animal.current));return nearest+8*road_distance(scene.paid,cell);};
        return score(a)<score(b);
    });
    for(const auto cell:cells) {
        sim::Command command{sim::CommandType::PlaceOriginalHuntersTent,cell};
        if(!outside_food_domain(world,scene.paid.farm,sim::Object::OriginalHuntersTent,cell)||!world.validate(command).accepted)continue;
        if(++attempts>32)break;
        auto candidate=world;auto proposed=scene.paid;if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(cell);bool prey=false;
        for(const auto start:candidate.building_cells(source)) {
            for(const auto& animal:candidate.hunting_state()->animals)if(candidate.find_hunting_route(start,animal.current,source)){prey=true;break;}
            if(prey)break;
        }
        if(prey&&connect(candidate,proposed,source)){world=std::move(candidate);scene.paid=std::move(proposed);scene.tent=source;break;}
    }
    scene.joint_planning["hunting"]={{"attempts",attempts},{"planned",scene.tent.has_value()}};
}
std::vector<p::QuayCandidate> qualified_quays(const sim::World& world) {
    std::vector<p::QuayCandidate> result;
    for(int y=1;y+2<world.height();++y)for(int x=1;x+2<world.width();++x) {
        for(std::uint8_t orientation=0;orientation<4;++orientation) {
            sim::Command command{sim::CommandType::PlaceOriginalFishingQuay,{x,y}};command.orientation=orientation;
            if(world.validate(command).accepted)result.push_back({command.cell,orientation});
        }
    }
    return result;
}
void add_fishing(sim::World& world,Scene& scene,std::vector<p::QuayCandidate> options) {
    std::stable_sort(options.begin(),options.end(),[&](const auto& a,const auto& b){return road_distance(scene.paid,a.cell)<road_distance(scene.paid,b.cell);});
    unsigned attempts=0;
    for(const auto& option:options) {
        if(++attempts>32)break;
        auto candidate=world;auto proposed=scene.paid;sim::Command command{sim::CommandType::PlaceOriginalFishingQuay,option.cell};command.orientation=option.orientation;
        if(!outside_food_domain(candidate,proposed.farm,sim::Object::OriginalFishingQuay,command.cell)||!candidate.validate(command).accepted)continue;
        if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(option.cell);const auto dock=orig::fishing_quay_geometry(option.cell,option.orientation).dock;bool water=false;
        for(const auto& site:candidate.fishing_state()->sites)if(site.cell!=dock&&candidate.find_fishing_route(dock,site.cell,source)){water=true;break;}
        if(water&&connect(candidate,proposed,source)){world=std::move(candidate);scene.paid=std::move(proposed);scene.quay=source;break;}
    }
    scene.joint_planning["fishing"]={{"attempts",attempts},{"source_qualified_global_quays",options.size()},{"planned",scene.quay.has_value()}};
}
Scene choose_scene(const sim::World& base) {
    auto layouts=p::city_blueprints();std::stable_sort(layouts.begin(),layouts.end(),[](const auto& a,const auto& b){return a.name=="compact4"&&b.name!="compact4";});
    const auto quays=qualified_quays(base);std::optional<Scene> fallback;
    unsigned attempts=0;
    for(const auto& layout:layouts) {
        const auto farm=std::find_if(layout.commands.begin(),layout.commands.end(),[](const auto& c){return c.type==sim::CommandType::PlaceOriginalFoodFarm;});
        l::check(farm!=layout.commands.end(),"Shared city blueprint lacks its paid Farm");
        struct Candidate {sim::Cell origin;unsigned rice;int shore_distance;};std::vector<Candidate> origins;
        for(const auto origin:p::clear_origins(base,layout)) {
            const sim::Cell source{origin.x+farm->cell.x,origin.y+farm->cell.y};unsigned rice=0;
            for(int y=source.y-3;y<=source.y+5;++y)for(int x=source.x-3;x<=source.x+5;++x)
                if(base.in_bounds({x,y})&&base.original_terrain_facts()->fertility({x,y})==100)++rice;
            int shore=1000;for(const auto& quay:quays)shore=std::min(shore,p::distance(origin,quay.cell));
            if(rice)origins.push_back({origin,rice,shore});
        }
        std::stable_sort(origins.begin(),origins.end(),[](const auto& a,const auto& b){return a.shore_distance!=b.shore_distance?a.shore_distance<b.shore_distance:a.rice>b.rice;});
        for(const auto& option:origins) {
            if(++attempts>256)break;
            auto candidate=base;Scene scene;
            if(!p::city(candidate,scene.paid,layout,option.origin)||!five_fields(candidate,scene.paid))continue;
            add_fishing(candidate,scene,quays);add_plantation(candidate,scene);add_hunting(candidate,scene);
            l::check(routes_to_all_fields(candidate,scene.paid.farm)&&routes_to_all_plants(candidate),"Optional paid producer blocked a required Grower route");
            p::planning_statistics["physical_grower_scene_attempts"]=attempts;
            if((scene.quay||quays.empty())&&scene.tent&&scene.plantation)return scene;
            if(!fallback)fallback=scene;
        }
        if(attempts>256)break;
    }
    if(fallback)return *fallback;
    throw std::runtime_error("No bounded source-admitted five-crop city with Rice fertility100 and physical Grower routes");
}
Id primary_farm(const sim::World& world) {
    l::check(world.physical_food_growers_state()->farms.size()==1,"This proof requires one actual five-crop Farm as the unique crop contributor");
    const auto farm=world.physical_food_growers_state()->farms.front().farm_id;std::set<sim::AgricultureCrop> crops;
    for(const auto& field:world.hunting_food_state()->fields)if(field.farm_id==farm)crops.insert(field.crop);
    l::check(crops.size()==5,"This proof requires all five actual paid crop kinds on its Farm");return farm;
}
Json balances(const sim::World& world) {
    l::check(world.rule_version()==31&&world.physical_food_growers_state()&&world.hunting_food_state()&&
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
    l::check(restored.snapshot()==world.snapshot(),"Exact schema47 checkpoint changed physical authority");
    const auto roundtrip=output/(name+".roundtrip.json");l::save::write_save(roundtrip,context.document(restored),context.source.map.data,context.source.map.mask);
    l::check(l::q::f::read_text(path)==l::q::f::read_text(roundtrip),"Canonical schema47 roundtrip bytes differ");
    if(recovery) {
        const auto entry=recovery->write_checkpoint(history,document,context.source.map.mask);
        l::check(entry.loadable()&&entry.schema==47&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery changed physical Grower authority");
    }
    manifest.push_back({{"label",name},{"tick",world.ticks()},{"file",path.filename().string()},{"exact_roundtrip",true},{"exact_recovery",recovery!=nullptr}});
}
struct Observation {
    std::array<std::array<bool,5>,5> stages{}; // outward,wait,work,loaded-return,received
    std::map<Id,std::uint64_t> work,returns,harvested;
    std::map<std::uint64_t,std::tuple<unsigned,bool,bool>> previous_phase;
    std::map<Id,sim::HuntingFoodCounters> previous_received,previous_consumed;
    std::optional<sim::HuntingFoodStock> previous_stock;
    std::optional<sim::PhysicalFoodGrowerTrip> previous_trip;
    std::optional<sim::Cell> previous_return_arrival;
    std::uint64_t previous_tick=0;
    sim::HuntingFoodCounters previous_returned{};
    Json actors=Json::array(),farm_receipts=Json::array(),house_meals=Json::array(),food_arrivals=Json::array();
    std::vector<Id> assignments;
    std::uint64_t last_assignment=0,observed_work=0,fully_staffed_ticks=0;
    void remember_worker(const sim::World& world,Id farm) {
        const auto& source=worker_source(world,farm);const auto& actor=world.courier(source.courier_id);
        previous_trip=source.latest_trip;previous_tick=world.ticks();previous_return_arrival.reset();
        if(actor.phase==sim::CourierPhase::Returning&&actor.edge_progress==4&&actor.path_vertex+1<actor.path.size())
            previous_return_arrival=actor.path.at(actor.path_vertex+1);
    }
    void seed(const sim::World& world,Id farm) {
        for(const auto& field:world.multi_field_agriculture_state()->fields)work[field.field_id]=field.productive_ticks;
        for(const auto& field:world.hunting_food_state()->fields)harvested[field.field_id]=field.harvested;
        for(const auto& field:world.physical_food_growers_state()->fields)returns[field.field_id]=field.returned;
        for(const auto& stock:world.hunting_food_state()->stocks){previous_received[stock.building_id]=stock.received;previous_consumed[stock.building_id]=stock.consumed;}
        previous_stock=farm_stock(world,farm);
        for(const auto& field:world.physical_food_growers_state()->fields)if(field.farm_id==farm) {
            const auto crop=std::find_if(world.hunting_food_state()->fields.begin(),world.hunting_food_state()->fields.end(),[&](const auto& row){return row.field_id==field.field_id;})->crop;
            previous_returned[static_cast<unsigned>(crop)]+=field.returned;
        }
        remember_worker(world,farm);
    }
    void after(const sim::World& world,Id farm) {
        const auto& source=worker_source(world,farm);const auto& actor=world.courier(source.courier_id);
        const auto required=world.workforce_required(farm);if(required>0&&world.workers_assigned(farm)==required)++fully_staffed_ticks;
        std::uint64_t total_delta=0;
        for(const auto& field:world.multi_field_agriculture_state()->fields) {
            auto& before=work[field.field_id];l::check(field.productive_ticks>=before,"Field work moved backwards");
            const auto delta=field.productive_ticks-before;if(field.farm_id==farm) {
                total_delta+=delta;
                if(delta)l::check(source.latest_trip&&source.latest_trip->field_id==field.field_id&&source.latest_trip->arrived_tick&&
                    (source.latest_trip->phase==sim::PhysicalFoodGrowerPhase::Working||source.latest_trip->harvested_tick==world.ticks())&&
                    !actor.path.empty()&&actor.path.at(actor.path_vertex)==world.building(field.field_id).cell,
                    "Crop work was earned without this actual arrived worker");
                if(delta)l::check(world.ticks()-*source.latest_trip->arrived_tick>=5U+source.latest_trip->work_ticks,
                    "Observed crop work skipped actual field waiting or elapsed tending");
            }
            before=field.productive_ticks;
        }
        l::check(total_delta<=1,"One physical Farm created more than one work unit/tick");observed_work+=total_delta;
        for(const auto& field:world.hunting_food_state()->fields) {
            auto& before=harvested[field.field_id];l::check(field.harvested>=before&&field.harvested-before<=1,"Field harvest history jumped");
            if(field.harvested>before)l::check(field.farm_id==farm&&source.latest_trip&&source.latest_trip->field_id==field.field_id&&
                source.latest_trip->harvested_tick==world.ticks()&&total_delta==1&&sim::physical_food_trip_cargo(*source.latest_trip)==1,
                "A field harvest appeared without this tick's actual worker tending and retained typed cargo");
            before=field.harvested;
        }
        if(source.latest_trip) {
            const auto& trip=*source.latest_trip;const auto g=static_cast<unsigned>(trip.crop);const bool live=!trip.returned_tick;
            if(trip.grower_id.value!=last_assignment){assignments.push_back(trip.field_id);last_assignment=trip.grower_id.value;}
            stages[g][0]|=live&&trip.phase==sim::PhysicalFoodGrowerPhase::Outward&&actor.edge_progress>0;
            stages[g][1]|=live&&trip.phase==sim::PhysicalFoodGrowerPhase::Waiting&&trip.wait_left>0;
            stages[g][2]|=live&&trip.phase==sim::PhysicalFoodGrowerPhase::Working&&trip.work_ticks>0;
            stages[g][3]|=live&&sim::physical_food_trip_cargo(trip)==1&&actor.edge_progress>0;
            const auto key=std::tuple{static_cast<unsigned>(trip.phase),trip.work_ticks>0,trip.returned_tick.has_value()};
            const auto previous=previous_phase.find(trip.grower_id.value);
            if(previous==previous_phase.end()||previous->second!=key) {
                actors.push_back({{"tick",world.ticks()},{"grower_id",trip.grower_id.value},{"courier_id",static_cast<unsigned>(source.courier_id)},
                    {"farm",static_cast<unsigned>(farm)},{"field",static_cast<unsigned>(trip.field_id)},{"crop",g},
                    {"phase",static_cast<unsigned>(trip.phase)},{"wait_left",trip.wait_left},{"work_ticks",trip.work_ticks},
                    {"cargo",sim::physical_food_trip_cargo(trip)},{"edge_progress",actor.edge_progress},{"route_pending",actor.route_pending},
                    {"arrived_tick",trip.arrived_tick?Json(*trip.arrived_tick):Json(nullptr)},
                    {"harvested_tick",trip.harvested_tick?Json(*trip.harvested_tick):Json(nullptr)},
                    {"returned_tick",trip.returned_tick?Json(*trip.returned_tick):Json(nullptr)}});
                previous_phase[trip.grower_id.value]=key;
            }
        }
        sim::HuntingFoodCounters returned_now{};
        for(const auto& field:world.physical_food_growers_state()->fields)if(field.farm_id==farm) {
            const auto crop=std::find_if(world.hunting_food_state()->fields.begin(),world.hunting_food_state()->fields.end(),[&](const auto& row){return row.field_id==field.field_id;})->crop;
            const auto g=static_cast<unsigned>(crop);returned_now[g]+=field.returned;auto& prior=returns[field.field_id];
            l::check(field.returned>=prior&&field.returned-prior<=1,"Actual Field return jumped or went backwards");
            if(field.returned>prior) {
                l::check(field.last_receipt&&field.last_receipt->returned_tick==world.ticks()&&field.last_receipt->crop==crop,
                    "Farm stock credit lacks its actual current worker receipt");stages[g][4]=true;
                const auto& receipt=*field.last_receipt;farm_receipts.push_back({{"field",static_cast<unsigned>(field.field_id)},{"crop",g},
                    {"grower_id",receipt.grower_id.value},{"courier_id",static_cast<unsigned>(receipt.courier_id)},
                    {"dispatched_tick",receipt.dispatched_tick},{"arrived_tick",receipt.arrived_tick},{"harvested_tick",receipt.harvested_tick},
                    {"returned_tick",receipt.returned_tick},{"work_at_harvest",receipt.work_at_harvest}});
                l::check(previous_tick+1==world.ticks()&&previous_trip&&previous_trip->grower_id==receipt.grower_id&&
                    previous_trip->field_id==field.field_id&&previous_trip->crop==crop&&sim::physical_food_trip_cargo(*previous_trip)==1&&
                    previous_trip->phase==sim::PhysicalFoodGrowerPhase::Returning&&previous_return_arrival&&
                    world.building_contains(farm,*previous_return_arrival)&&source.latest_trip&&
                    source.latest_trip->grower_id==receipt.grower_id&&source.latest_trip->returned_tick==world.ticks()&&source.latest_trip->delivered==1,
                    "Farm receipt lacks the preceding loaded worker's real final5-tick edge into its owning Farm");
            }
            prior=field.returned;
        }
        const auto& stock=farm_stock(world,farm);
        for(std::size_t g=0;g<7;++g) {
            const auto earned=returned_now[g]-previous_returned[g];
            l::check(stock.returned[g]>=previous_stock->returned[g]&&stock.departed[g]>=previous_stock->departed[g],
                "Farm Cart transfer history moved backwards");
            const auto input=stock.returned[g]-previous_stock->returned[g];const auto output=stock.departed[g]-previous_stock->departed[g];
            l::check(std::int64_t(stock.amounts[g])-previous_stock->amounts[g]==static_cast<std::int64_t>(earned+input)-static_cast<std::int64_t>(output),
                "Farm stock changed without exactly one real crop return or physical Cart transfer");
        }
        previous_stock=stock;previous_returned=returned_now;
        for(const auto& account:world.hunting_food_state()->stocks) {
            const auto kind=world.building(account.building_id).kind;
            for(std::size_t g=0;g<7;++g) {
                auto& arrived=previous_received[account.building_id][g];auto& eaten=previous_consumed[account.building_id][g];
                if(account.received[g]>arrived)food_arrivals.push_back({{"tick",world.ticks()},{"owner",static_cast<unsigned>(account.building_id)},
                    {"house",sim::original_house_kind(kind)},{"crop",g},{"accepted",account.received[g]-arrived}});
                if(account.consumed[g]>eaten&&sim::original_house_kind(kind))house_meals.push_back({{"tick",world.ticks()},
                    {"house",static_cast<unsigned>(account.building_id)},{"crop",g},{"portions",account.consumed[g]-eaten}});
                arrived=account.received[g];eaten=account.consumed[g];
            }
        }
        remember_worker(world,farm);
    }
};
Json scene_report(const sim::World& world,Id farm) {
    Json fields=Json::array();sim::HuntingFoodCounters market{},house{},used{};
    for(const auto& field:world.physical_food_growers_state()->fields)if(field.farm_id==farm) {
        const auto& crops=world.hunting_food_state()->fields;
        const auto crop=std::find_if(crops.begin(),crops.end(),[&](const auto& row){return row.field_id==field.field_id;});
        const auto& body=world.building(field.field_id);fields.push_back({{"field",static_cast<unsigned>(field.field_id)},
            {"cell",{body.cell.x,body.cell.y}},{"crop",static_cast<unsigned>(crop->crop)},{"harvested",crop->harvested},{"returned",field.returned},
            {"source_fertility",world.original_terrain_facts()->fertility(body.cell)},{"progress",world.agriculture_field_progress(field.field_id)}});
    }
    for(const auto& stock:world.hunting_food_state()->stocks)for(std::size_t g=0;g<7;++g) {
        const auto kind=world.building(stock.building_id).kind;
        if(kind==sim::Object::Market)market[g]+=stock.received[g];
        if(sim::original_house_kind(kind)){house[g]+=stock.received[g];used[g]+=stock.consumed[g];}
    }
    std::uint64_t fish=0,meat=0,plantation=0;
    for(const auto& quay:world.fishing_state()->quays)fish+=quay.unloaded;
    for(const auto& tent:world.hunting_state()->tents)meat+=tent.unloaded;
    for(const auto& plant:world.plantations_state()->plants)plantation+=plant.returned;
    const auto& body=world.building(farm);
    return {{"farm",static_cast<unsigned>(farm)},{"cell",{body.cell.x,body.cell.y}},{"fields",fields},
        {"assigned_workers",world.workers_assigned(farm)},{"required_workers",world.workforce_required(farm)},
        {"spawned",worker_source(world,farm).spawned},{"retired",worker_source(world,farm).retired},
        {"market_received",market},{"house_received",house},{"house_consumed",used},
        {"joint",{{"paid_quays",world.fishing_state()->quays.size()},{"actual_fish_unloaded",fish},
            {"paid_tents",world.hunting_state()->tents.size()},{"actual_meat_unloaded",meat},
            {"paid_plantation_sources",world.plantations_state()->sources.size()},{"actual_plantation_returns",plantation}}},
        {"buildings",world.buildings().size()},{"courier_channels",world.couriers().size()+world.fishing_state()->quays.size()+
            world.hunting_state()->tents.size()+3*world.hunting_state()->habitats.size()},
        {"population",world.total_population()},{"treasury",world.treasury()}};
}
std::optional<std::string> current_checkpoint(const sim::World& world,Id farm,const std::set<std::string>& saved) {
    const auto& source=worker_source(world,farm);const auto& actor=world.courier(source.courier_id);
    if(source.latest_trip&&!source.latest_trip->returned_tick) {
        const auto& trip=*source.latest_trip;
        if(!saved.contains("grower-outward")&&trip.phase==sim::PhysicalFoodGrowerPhase::Outward&&actor.edge_progress>0)return "grower-outward";
        if(!saved.contains("grower-waiting")&&trip.phase==sim::PhysicalFoodGrowerPhase::Waiting&&trip.wait_left>0)return "grower-waiting";
        if(!saved.contains("grower-working")&&trip.phase==sim::PhysicalFoodGrowerPhase::Working&&trip.work_ticks>=8)return "grower-working";
        if(!saved.contains("grower-loaded")&&sim::physical_food_trip_cargo(trip)==1&&actor.edge_progress>0)return "grower-loaded";
    }
    if(!saved.contains("food-cart"))for(const auto& trip:world.hunting_food_state()->trips)if(trip.source==farm) {
        const auto& cart=world.courier(trip.transfer.courier_id);
        if(cart.role==sim::CourierRole::OriginalFoodToMarket&&world.original_food_cargo(cart.id)>0&&cart.edge_progress>0)return "food-cart";
    }
    return {};
}
}

int main(int argc,char** argv) {
    fs::path output;Json commands=Json::array();
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto selected=options(argc,argv);l::SourceGuard sources(selected.data);
        const ReplayGuard map_input(selected.data/selected.map),input(selected.replay),expected_input(selected.expected);
        Context context(selected.data,selected.map);
        l::check(context.source.model->provenance().sha256==l::base::model_sha&&
            (selected.map!="Cities/Xia.map"||context.source.terrain->source_map_sha256()==l::base::map_sha),
            "Positive proof requires the unchanged supplied Normal model and exact selected map");
        auto world=selected.replay.empty()?context.create():context.restore(l::save::read_save(selected.replay));
        fs::create_directories(selected.output);output=selected.output;Json planning=Json::object();
        if(selected.replay.empty()) {
            const auto scene=choose_scene(world);planning=scene.joint_planning;
            for(const auto command:scene.paid.commands)paid_command(world,command,commands,"MAC06 actual paid source-bound scene");
            std::vector<Id> critical;
            for(const auto& body:world.buildings())if(body.kind==sim::Object::OriginalFoodFarm||body.kind==sim::Object::Market||
                body.kind==sim::Object::OriginalWaterWell72||body.kind==sim::Object::OriginalAncestorShrine214||body.kind==sim::Object::FireWatch)critical.push_back(body.id);
            for(const auto id:critical)paid_command(world,sim::set_building_workforce_priority(id,sim::WorkforcePriority::High),commands,"MAC06 actual critical staffing priority");
            l::check(world.ticks()==0&&!world.total_population(),"Paid setup created elapsed time or residents");
            for(const auto& stock:world.hunting_food_state()->stocks)for(const auto amount:stock.amounts)l::check(!amount,"Paid setup injected Food");
        }
        const auto farm=primary_farm(world);const auto begin=world.ticks();
        l::check(begin<=UINT64_MAX-selected.ticks,"Continuation clock would overflow");std::uint64_t end=begin+selected.ticks;
        std::optional<l::save::SaveDocument> expected;
        if(!selected.expected.empty()) {
            expected=l::save::read_save(selected.expected);(void)context.restore(*expected);end=expected->world.ticks;
            l::check(end>=begin&&end-begin>=12000&&end-begin<=100000,"Separate replay requires12000..100000 real continuation ticks");
        }
        Json manifest=Json::array();checkpoint(context,world,output,"seed",manifest);
        auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
        l::save::RecoveryStore recovery(output/"recovery",selected.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
        const auto history=recovery.create_history(l::save::read_save(output/"seed.save.json"),context.source.map.mask);
        Observation observed;observed.seed(world,farm);std::set<std::string> saved;
        while(world.ticks()<end) {
            world.tick();replay.tick();observed.after(world,farm);
            if(const auto name=current_checkpoint(world,farm,saved)) {
                checkpoint(context,world,output,*name,manifest,&recovery,history);saved.insert(*name);
            }
            if(world.ticks()%816==0) {
                (void)balances(world);l::check(world.snapshot()==replay.snapshot(),"Independent restored continuation diverged");
                sources.unchanged();map_input.unchanged();input.unchanged();expected_input.unchanged();
            }
        }
        const auto mass=balances(world);l::check(world.snapshot()==replay.snapshot(),"Terminal restored continuation diverged");
        checkpoint(context,world,output,"final",manifest,&recovery,history);bool independent=false;
        if(expected) {
            l::check(expected->world==world.snapshot(),"Separate creator/replay endpoint differs");
            l::check(l::q::f::read_text(selected.expected)==l::q::f::read_text(output/"final.save.json"),"Separate creator/replay canonical bytes differ");independent=true;
        }
        const auto actual=scene_report(world,farm);bool positive=world.ticks()-begin>=12000&&saved.size()==5&&observed.assignments.size()>=5;
        Json stages=Json::array();
        for(std::size_t crop=0;crop<5;++crop) {
            const bool complete=std::all_of(observed.stages[crop].begin(),observed.stages[crop].end(),[](bool value){return value;});
            const auto consumed=actual.at("house_consumed")[crop].get<std::uint64_t>();positive&=complete&&consumed>0&&actual.at("market_received")[crop].get<std::uint64_t>()>0;
            stages.push_back({{"crop",crop},{"outward_wait_work_loaded_return_receipt",observed.stages[crop]},
                {"all_observed",complete},{"actual_house_consumed",consumed}});
        }
        const auto& joint=actual.at("joint");
        const bool joint_positive=joint.at("actual_fish_unloaded").get<std::uint64_t>()>0&&
            joint.at("actual_meat_unloaded").get<std::uint64_t>()>0&&joint.at("actual_plantation_returns").get<std::uint64_t>()>0;
        sources.unchanged();map_input.unchanged();input.unchanged();expected_input.unchanged();const auto status=positive?"PASS":"PARTIAL";
        Json report={{"format","openemperor-physical-food-growers-acceptance-v1"},{"status",status},{"rule",31},{"schema",47},
            {"map",selected.map.generic_string()},{"map_sha256",context.source.terrain->source_map_sha256()},
            {"model_sha256",context.source.model->provenance().sha256},{"unchanged_supplied_model",true},
            {"source_bytes_mode_mtime_unchanged",true},{"snapshot_stock_or_population_injection",false},
            {"begin_tick",begin},{"end_tick",world.ticks()},{"real_ticks",world.ticks()-begin},{"farm",static_cast<unsigned>(farm)},
            {"five_crop_actual_stages",stages},{"actual_scene",actual},{"joint_actual_status",joint_positive?"PASS":"PARTIAL"},
            {"joint_planning",planning},{"planning_diagnostics",p::planning_statistics},
            {"physical_worker_transitions",observed.actors},{"actual_farm_receipts",observed.farm_receipts},
            {"actual_food_arrivals",observed.food_arrivals},{"actual_house_consumption",observed.house_meals},
            {"actual_field_assignments",observed.assignments},{"observed_work",observed.observed_work},{"fully_staffed_ticks",observed.fully_staffed_ticks},
            {"physical_conservation",mass},{"checkpoints",manifest},{"recovery_exact",true},
            {"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",independent},
            {"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},
            {"claim_scope","One paid Farm; five physical crop Grower chains through real Farm receipt, Market/House arrival and actual House consumption. AUTHORED work and units; no Original raw/x87 yield claim."}};
        l::write(output/"commands.json",commands);l::write(output/"report.json",report);
        std::cout<<status<<" Rule31/schema47 paid physical five-crop Growers; ticks="<<world.ticks()-begin<<" report="<<output/"report.json"<<'\n';return positive?0:2;
    } catch(const std::exception& error) {
        if(!output.empty()) {
            if(!fs::exists(output/"commands.json"))try{l::write(output/"commands.json",commands);}catch(...){}
            if(!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()},{"planning_diagnostics",p::planning_statistics}});}catch(...){}
        }
        std::cerr<<"Physical Food Growers acceptance FAIL: "<<error.what()<<'\n';return 1;
    }
}
