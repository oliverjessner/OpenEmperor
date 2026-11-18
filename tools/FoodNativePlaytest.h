#pragma once

// Explicit private technical playtest with supplied Original data and normal
// production SandboxView commands/ticks. Scripted SDL software evidence only;
// it is never a claim of native-window or human acceptance.
#include "app/MenuStorage.h"
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "assets/RgbaPngEncoder.h"
#include "maps/TerrainRenderPlan.h"
#include "simulation/original/FishingRules.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace orig=oe::original;
using Json=nlohmann::json;
namespace oe_food_playtest {
void require(bool ok,const std::string& message) {if(!ok)throw std::runtime_error(message);}
int distance(sim::Cell a,sim::Cell b) {return std::abs(a.x-b.x)+std::abs(a.y-b.y);}
bool below(const fs::path& child,const fs::path& parent) {
    auto c=child.begin();for(auto p=parent.begin();p!=parent.end();++p,++c)
        if(c==child.end() || *c!=*p)return false;
    return true;
}
fs::path output_root(const fs::path& requested,const fs::path& data,
                     const fs::path& private_scope=".local/agri-mac02") {
    const auto repo=fs::canonical(fs::path(__FILE__).parent_path().parent_path());
    const auto allowed=fs::weakly_canonical(repo/private_scope);
    const auto output=fs::absolute(requested).lexically_normal();
    fs::path ancestry;
    for(const auto& part:output) {
        ancestry/=part;std::error_code error;const auto status=fs::symlink_status(ancestry,error);
        require(error==std::errc::no_such_file_or_directory || !error,"Cannot inspect output ancestry");
        require(error || !fs::is_symlink(status),"Private playtest output must not traverse symlinks");
    }
    const auto resolved=fs::weakly_canonical(output);
    require(resolved!=allowed && below(resolved,allowed) && !below(resolved,data) && !below(data,resolved),
            "Output must be a fresh private playtest subdirectory disjoint from Original data");
    require(!fs::exists(resolved),"Playtest output already exists; choose a fresh directory");
    oe::persistence::validate_save_target(resolved/"saves/manual.save.json",data);
    fs::create_directories(resolved/"saves");return resolved;
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;bool running=true;
    view.handle_event(event,running);require(running,"Scripted key unexpectedly closed the view");
}
void configure(oe::SandboxView& view,const oe::VisualSelection& visuals) {
    view.set_compatibility(visuals.compatibility.compatible() ? visuals.compatibility.profile->id:"unknown");
    if(!visuals.walker.empty())view.set_walker_visuals(visuals.walker,visuals.walker_source);
    if(!visuals.building.empty())view.set_building_visuals(visuals.building,visuals.building_source);
    if(!visuals.road.empty())view.set_road_visuals(visuals.road,visuals.road_source);
    view.set_fire_visuals(visuals.fire,visuals.fire_source,visuals.fire_fallback_reason);
    view.set_fire_inspector_visuals(visuals.fire_inspector,visuals.fire_inspector_source,visuals.fire_inspector_fallback_reason);
    view.set_market_walker_visuals(visuals.market_walker,visuals.market_walker_source,visuals.market_walker_fallback_reason);
    view.set_service_walker_visuals(visuals.service_walker,visuals.service_walker_source,visuals.service_walker_fallback_reason);
    view.set_health_walker_visuals(visuals.health_walker,visuals.health_walker_source,visuals.health_walker_fallback_reason);
    view.set_walker_animation_visuals(visuals.walker_animation,visuals.walker_animation_source,visuals.walker_animation_fallback_reason);
    view.set_original_house_visuals(visuals.original_house,visuals.original_house_source,visuals.original_house_fallback_reason);
    view.set_original_building_visuals(visuals.original_building,visuals.original_building_source,visuals.original_building_fallback_reason);
}
struct QuayCandidate {sim::Cell cell{};std::uint8_t orientation=0;};
struct Plan {
    std::string layout;
    sim::Cell origin{},quay_cell{};
    std::uint8_t orientation=0;
    sim::BuildingId quay{},farm{},market{};
    std::vector<sim::BuildingId> homes;
    std::vector<sim::Cell> roads;
    std::vector<sim::Command> commands;
};
Json planning_statistics=Json::object();
struct CityLayout {std::string name;std::vector<sim::Command> commands;};
std::vector<CityLayout> city_blueprints() {
    std::vector<CityLayout> layouts;
    const auto add_body=[](auto& commands,sim::CommandType type,int x,int y){commands.push_back({type,{x,y}});};
    CityLayout original{"linear8",{}};
    for(const int y:{4,9})for(int x=0;x<24;++x)add_body(original.commands,sim::CommandType::PlaceRoad,x,y);
    for(int y=5;y<9;++y)add_body(original.commands,sim::CommandType::PlaceRoad,23,y);
    for(int x=0;x<=21;x+=3)add_body(original.commands,sim::CommandType::PlaceHousehold,x,2);
    add_body(original.commands,sim::CommandType::PlaceOriginalWaterWell,0,5);
    add_body(original.commands,sim::CommandType::PlaceOriginalAncestorShrine,3,5);
    add_body(original.commands,sim::CommandType::PlaceMarket,6,5);
    add_body(original.commands,sim::CommandType::PlaceOriginalFoodFarm,0,10);
    add_body(original.commands,sim::CommandType::PlaceOriginalMilletField,3,10);
    add_body(original.commands,sim::CommandType::PlaceFireWatch,12,5);
    add_body(original.commands,sim::CommandType::PlaceFireWatch,16,5);
    layouts.push_back(std::move(original));
    for(const int homes:{8,6}) {
        CityLayout layout{"compact"+std::to_string(homes),{}};const int half=homes/2;
        const int right=half*3+2;
        for(const int y:{4,9})for(int x=0;x<=right;++x)add_body(layout.commands,sim::CommandType::PlaceRoad,x,y);
        for(int y=5;y<9;++y)add_body(layout.commands,sim::CommandType::PlaceRoad,right,y);
        for(const int y:{2,5})for(int i=0;i<half;++i)add_body(layout.commands,sim::CommandType::PlaceHousehold,3*i,y);
        add_body(layout.commands,sim::CommandType::PlaceOriginalWaterWell,0,10);
        add_body(layout.commands,sim::CommandType::PlaceOriginalAncestorShrine,3,10);
        add_body(layout.commands,sim::CommandType::PlaceMarket,6,10);
        add_body(layout.commands,sim::CommandType::PlaceOriginalFoodFarm,9,10);
        add_body(layout.commands,sim::CommandType::PlaceOriginalMilletField,12,10);
        add_body(layout.commands,sim::CommandType::PlaceFireWatch,right-2,5);
        add_body(layout.commands,sim::CommandType::PlaceFireWatch,right-2,8);
        layouts.push_back(std::move(layout));
    }
    CityLayout compact{"compact4",{}};
    for(const int y:{4,7})for(int x=0;x<12;++x)add_body(compact.commands,sim::CommandType::PlaceRoad,x,y);
    for(int y=5;y<7;++y)add_body(compact.commands,sim::CommandType::PlaceRoad,11,y);
    for(int x=0;x<=9;x+=3)add_body(compact.commands,sim::CommandType::PlaceHousehold,x,2);
    add_body(compact.commands,sim::CommandType::PlaceOriginalWaterWell,0,5);
    add_body(compact.commands,sim::CommandType::PlaceOriginalAncestorShrine,3,5);
    add_body(compact.commands,sim::CommandType::PlaceMarket,6,5);
    add_body(compact.commands,sim::CommandType::PlaceOriginalFoodFarm,0,8);
    add_body(compact.commands,sim::CommandType::PlaceOriginalMilletField,3,8);
    add_body(compact.commands,sim::CommandType::PlaceFireWatch,9,5);
    add_body(compact.commands,sim::CommandType::PlaceFireWatch,9,6);
    layouts.push_back(std::move(compact));
    return layouts;
}
const char* command_name(sim::CommandType type) {
    switch(type) {
    case sim::CommandType::PlaceRoad:return "Road";
    case sim::CommandType::PlaceHousehold:return "House";
    case sim::CommandType::PlaceOriginalWaterWell:return "WaterWell";
    case sim::CommandType::PlaceOriginalAncestorShrine:return "Ancestor";
    case sim::CommandType::PlaceMarket:return "Market";
    case sim::CommandType::PlaceOriginalFoodFarm:return "FoodFarm";
    case sim::CommandType::PlaceOriginalMilletField:return "Millet";
    case sim::CommandType::PlaceFireWatch:return "FireWatch";
    default:return "Other";
    }
}
void rejection(Json& diagnostics,const std::string& reason,sim::Cell cell) {
    if(!diagnostics.contains(reason))diagnostics[reason]={{"count",0},{"example_cell",{cell.x,cell.y}}};
    diagnostics[reason]["count"]=diagnostics[reason]["count"].get<std::uint64_t>()+1;
}
void diagnose_permissions(const sim::World& world,const fs::path& output) {
    // Private request-time inspection of the same immutable permissions and
    // production validators used by the view; this grants no construction.
    auto& result=planning_statistics["source_permissions"];
    result={{"width",world.width()},{"height",world.height()},{"buildable_cells",0},
        {"road_buildable_cells",0},{"water_cells",0},{"protected_cells",0},
        {"raw_terrain_counts",Json::object()},{"legal_placements",Json::object()},
        {"raster","permissions.png"},{"raster_scale",4},
        {"raster_legend",{{"green","building and paid-road cell"},{"yellow","road-only cell"},
            {"blue","source-qualified water"},{"red","protected Original cell"},
            {"gray","in-map unsupported terrain"},{"black","outside source geometry"}}}};
    oe::assets::RgbaImage image;
    image.width=static_cast<std::uint16_t>(world.width()*4);
    image.height=static_cast<std::uint16_t>(world.height()*4);
    image.pixels.resize(static_cast<std::size_t>(image.width)*image.height*4);
    constexpr std::array types{sim::CommandType::PlaceRoad,sim::CommandType::PlaceHousehold,
        sim::CommandType::PlaceOriginalFoodFarm,sim::CommandType::PlaceOriginalWaterWell,
        sim::CommandType::PlaceOriginalAncestorShrine,sim::CommandType::PlaceMarket,
        sim::CommandType::PlaceFireWatch};
    for(const auto type:types) result["legal_placements"][command_name(type)]={
        {"count",0},{"first_cells",Json::array()},{"rejections",Json::object()}};
    for(int y=0;y<world.height();++y)for(int x=0;x<world.width();++x) {
        const sim::Cell cell{x,y};const auto& permissions=*world.map_permissions();
        const auto increment=[&](const char* name){result[name]=result[name].get<std::uint64_t>()+1;};
        if(world.buildable(cell))increment("buildable_cells");
        if(world.road_buildable(cell))increment("road_buildable_cells");
        if(permissions.bridge_water(cell))increment("water_cells");
        if(permissions.protected_original(cell))increment("protected_cells");
        const auto raw=std::to_string(world.original_terrain_facts()->raw_terrain(cell));
        auto& count=result["raw_terrain_counts"][raw];count=count.is_null()?1:count.get<std::uint64_t>()+1;
        for(const auto type:types) {
            const auto accepted=world.validate({type,cell});auto& row=result["legal_placements"][command_name(type)];
            if(accepted.accepted) {
                row["count"]=row["count"].get<std::uint64_t>()+1;
                if(row["first_cells"].size()<64)row["first_cells"].push_back({x,y});
            } else rejection(row["rejections"],accepted.reason,accepted.diagnostic.cell.value_or(cell));
        }
        std::array<std::uint8_t,4> color{35,35,35,255};
        if(permissions.building_blocker(cell)==sim::BuildBlocker::OutsideMap)color={0,0,0,255};
        if(permissions.bridge_water(cell))color={20,90,225,255};
        if(world.road_buildable(cell))color={230,190,30,255};
        if(world.buildable(cell))color={30,190,60,255};
        if(permissions.protected_original(cell))color={220,40,40,255};
        for(int dy=0;dy<4;++dy)for(int dx=0;dx<4;++dx) {
            const auto at=(static_cast<std::size_t>(4*y+dy)*image.width+static_cast<std::size_t>(4*x+dx))*4;
            std::copy(color.begin(),color.end(),image.pixels.begin()+static_cast<std::ptrdiff_t>(at));
        }
    }
    const auto png=oe::assets::encode_rgba_png(image);std::ofstream file(output/"permissions.png",std::ios::binary);
    file.write(reinterpret_cast<const char*>(png.data()),static_cast<std::streamsize>(png.size()));
    require(bool(file),"Could not write private source-permission raster");
}
std::vector<sim::Cell> clear_origins(const sim::World& world,const CityLayout& layout) {
    std::vector<sim::Cell> road_offsets;
    int width=0,height=0;
    for(const auto& command:layout.commands) {
        if(command.type==sim::CommandType::PlaceRoad) {
            road_offsets.push_back(command.cell);
            width=std::max(width,command.cell.x+1);height=std::max(height,command.cell.y+1);
            continue;
        }
        const auto kind=sim::placed_object(command.type);require(kind.has_value(),"Starter command has no production object identity");
        for(const auto cell:sim::building_footprint_cells(world.profile(),world.rule_version(),*kind,command.cell)) {
            width=std::max(width,cell.x+1);height=std::max(height,cell.y+1);
        }
    }
    auto& statistics=planning_statistics["layouts"][layout.name];statistics={{"width",width},{"height",height},{"tested_origins",0},{"first_rejections",Json::object()}};
    std::vector<sim::Cell> result;
    for(int y=0;y+height<=world.height();++y)for(int x=0;x+width<=world.width();++x) {
        statistics["tested_origins"]=statistics["tested_origins"].get<std::uint64_t>()+1;
        bool legal=true;
        for(auto command:layout.commands) {
            command.cell={x+command.cell.x,y+command.cell.y};
            if(command.type==sim::CommandType::PlaceOriginalMilletField) {
                for(const auto cell:sim::building_footprint_cells(world.profile(),world.rule_version(),sim::Object::OriginalMilletField,command.cell))
                    if(!world.buildable(cell) || world.object_at(cell)!=sim::Object::Empty) {
                        rejection(statistics["first_rejections"],"Millet occupied terrain",cell);legal=false;break;
                    }
            } else {
                const auto accepted=world.validate(command);
                if(!accepted.accepted) {
                    rejection(statistics["first_rejections"],std::string(command_name(command.type))+": "+accepted.reason,
                        accepted.diagnostic.cell.value_or(command.cell));legal=false;
                }
            }
            if(!legal)break;
        }
        if(!legal)continue;
        for(std::size_t i=0;i<road_offsets.size() && legal;++i)for(std::size_t j=0;j<i;++j) {
            if(distance(road_offsets[i],road_offsets[j])!=1)continue;
            const sim::Cell a{x+road_offsets[i].x,y+road_offsets[i].y},b{x+road_offsets[j].x,y+road_offsets[j].y};
            const auto blocker=world.transport_edge_blocker(a,b);
            if(blocker!=sim::BuildBlocker::None) {
                rejection(statistics["first_rejections"],std::string("Road edge: ")+sim::build_blocker_reason(blocker),a);legal=false;break;
            }
        }
        if(legal)result.push_back({x,y});
    }
    statistics["legal_origins"]=result.size();return result;
}
bool buy(sim::World& world,Plan& plan,sim::Command command) {
    const auto result=world.execute(command);
    if(!result.accepted)return false;
    plan.commands.push_back(command);
    if(command.type==sim::CommandType::PlaceRoad)plan.roads.push_back(command.cell);
    return true;
}
bool city(sim::World& world,Plan& plan,const CityLayout& layout,sim::Cell origin) {
    plan.origin=origin;plan.layout=layout.name;
    for(auto command:layout.commands) {
        command.cell={origin.x+command.cell.x,origin.y+command.cell.y};
        if(command.type==sim::CommandType::PlaceOriginalMilletField)command.building_id=plan.farm;
        if(!buy(world,plan,command))return false;
        if(command.type==sim::CommandType::PlaceHousehold)plan.homes.push_back(*world.building_owner_at(command.cell));
        if(command.type==sim::CommandType::PlaceOriginalFoodFarm)plan.farm=*world.building_owner_at(command.cell);
        if(command.type==sim::CommandType::PlaceMarket)plan.market=*world.building_owner_at(command.cell);
    }
    return true;
}
std::vector<sim::Cell> line(std::vector<sim::Cell> points) {
    std::vector<sim::Cell> result;
    for(std::size_t i=0;i<points.size();++i) {
        if(i && points[i-1].x!=points[i].x && points[i-1].y!=points[i].y)return {};
        if(!i) {result.push_back(points[i]);continue;}
        auto c=points[i-1];const sim::Cell delta{(points[i].x>c.x)-(points[i].x<c.x),(points[i].y>c.y)-(points[i].y<c.y)};
        while(c!=points[i]) {c={c.x+delta.x,c.y+delta.y};result.push_back(c);if(result.size()>512)return {};}
    }
    return result;
}
std::vector<std::vector<sim::Cell>> road_options(sim::Cell from,sim::Cell to) {
    std::vector<std::vector<sim::Cell>> result;
    result.push_back(line({from,{to.x,from.y},to}));result.push_back(line({from,{from.x,to.y},to}));
    for(const int offset:{-3,3,-6,6,-12,12,-24,24}) {
        result.push_back(line({from,{from.x,from.y+offset},{to.x,from.y+offset},to}));
        result.push_back(line({from,{from.x+offset,from.y},{from.x+offset,to.y},to}));
    }
    return result;
}
Plan choose_plan(const sim::World& base) {
    require(base.fishing_state() && !base.fishing_state()->sites.empty(),"Map has no source-qualified fishing ground");
    const auto layouts=city_blueprints();
    struct Origin {std::size_t layout;sim::Cell cell;};std::vector<Origin> origins;
    for(std::size_t i=0;i<layouts.size();++i) {
        const auto selected=clear_origins(base,layouts[i]);for(const auto cell:selected)origins.push_back({i,cell});
    }
    require(!origins.empty(),"No legal used-cell starter among compact4/6/8 layouts; inspect planning_statistics first_rejections");
    std::vector<QuayCandidate> quays;
    for(int y=1;y+2<base.height();++y)for(int x=1;x+2<base.width();++x)for(std::uint8_t orientation=0;orientation<4;++orientation) {
        sim::Command command{sim::CommandType::PlaceOriginalFishingQuay,{x,y}};command.orientation=orientation;
        if(base.validate(command).accepted)quays.push_back({{x,y},orientation});
    }
    require(!quays.empty(),"Map has no complete legal Quay shoreline under production admission");
    struct Pair {Origin origin;QuayCandidate quay;int score;};std::vector<Pair> pairs;
    for(const auto quay:quays) {
        sim::World probe=base;sim::Command command{sim::CommandType::PlaceOriginalFishingQuay,quay.cell};command.orientation=quay.orientation;
        require(probe.execute(command).accepted,"Quay preflight changed before paid candidate construction");
        const auto id=*probe.building_owner_at(quay.cell);const auto dock=orig::fishing_quay_geometry(quay.cell,quay.orientation).dock;
        bool reachable=false;
        for(const auto& site:probe.fishing_state()->sites)if(site.cell!=dock) {
            const auto route=probe.find_fishing_route(dock,site.cell,id);
            if(route && route->size()>1) {reachable=true;break;}
        }
        if(!reachable)continue;
        auto ordered=origins;
        std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b){return distance(a.cell,quay.cell)<distance(b.cell,quay.cell);});
        for(std::size_t i=0;i<std::min<std::size_t>(ordered.size(),12);++i)
            pairs.push_back({ordered[i],quay,distance(ordered[i].cell,quay.cell)});
    }
    std::stable_sort(pairs.begin(),pairs.end(),[](const auto& a,const auto& b){return a.score<b.score;});
    require(!pairs.empty(),"No legal Quay has a real shared-navigator route to a fishing ground");
    std::size_t attempts=0;
    for(const auto& pair:pairs) {
        if(++attempts>256)break;
        sim::World candidate=base;Plan plan;
        if(!city(candidate,plan,layouts[pair.origin.layout],pair.origin.cell))continue;
        sim::Command quay{sim::CommandType::PlaceOriginalFishingQuay,pair.quay.cell};quay.orientation=pair.quay.orientation;
        if(!buy(candidate,plan,quay))continue;
        plan.quay=*candidate.building_owner_at(pair.quay.cell);plan.quay_cell=pair.quay.cell;plan.orientation=pair.quay.orientation;
        const auto shape=orig::fishing_quay_geometry(pair.quay.cell,pair.quay.orientation);
        std::vector<sim::Cell> goals;
        for(const auto land:shape.land)for(const sim::Cell delta:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}) {
            const sim::Cell c{land.x+delta.x,land.y+delta.y};
            if(std::find(shape.occupied.begin(),shape.occupied.end(),c)==shape.occupied.end() &&
                candidate.validate({sim::CommandType::PlaceRoad,c}).accepted &&
                std::find(goals.begin(),goals.end(),c)==goals.end())goals.push_back(c);
        }
        for(const auto goal:goals) {
            auto starts=plan.roads;
            std::stable_sort(starts.begin(),starts.end(),[&](auto a,auto b){return distance(a,goal)<distance(b,goal);});
            if(starts.size()>12)starts.resize(12);
            for(const auto start:starts)for(const auto& cells:road_options(start,goal)) {
            if(cells.empty() || !std::all_of(cells.begin(),cells.end(),[&](auto c){return candidate.validate({sim::CommandType::PlaceRoad,c}).accepted;}))continue;
            sim::World connected=candidate;Plan complete=plan;bool legal=true;
            for(const auto c:cells)if(!buy(connected,complete,{sim::CommandType::PlaceRoad,c})) {legal=false;break;}
            if(legal && connected.find_building_route(complete.quay,complete.market)) {
                planning_statistics["paid_layout_attempts"]=attempts;return complete;
            }
            }
        }
    }
    throw std::runtime_error("Bounded paid placement planning found no legal road-connected Quay/starter combination; no admission was widened");
}
void invariant(const sim::World& world) {
    require(world.population_valid() && world.city_economy_valid() && world.production_balance_valid() &&
        world.navigation_valid() && world.fire_state_valid(),"Actual-data paid World invariant failed");
}
Json observe(const sim::World& world) {
    std::uint64_t caught=0,unloaded=0,market=0,houses=0,consumed=0,millet=0;
    for(const auto& site:world.fishing_state()->sites)caught+=site.caught;
    for(const auto& quay:world.fishing_state()->quays)unloaded+=quay.unloaded;
    for(const auto& stock:world.fishing_food_state()->stocks) {
        const auto kind=world.building(stock.building_id).kind;
        if(kind==sim::Object::Market)market+=stock.received[5];
        if(sim::original_house_kind(kind)) {houses+=stock.received[5];consumed+=stock.consumed[5];millet+=stock.consumed[1];}
    }
    return {{"tick",world.ticks()},{"population",world.total_population()},{"funds",world.treasury()},
        {"caught",caught},{"quay_unloaded",unloaded},{"market_received",market},{"house_received",houses},
        {"fish_consumed",consumed},{"millet_consumed",millet},{"active_boats",world.fishing_state()->boats.size()},
        {"boat_launches",world.fishing_state()->launched},{"boat_retirements",world.fishing_state()->retired}};
}
Json checkpoint(oe::SandboxView& view,const fs::path& data,const fs::path& map,const fs::path& root,const std::string& name) {
    const auto path=root/"saves"/(name+".save.json");view.configure_save(data,map,path);
    const auto before=view.world().snapshot();const auto save_generation=view.save_generation();key(view,SDLK_F5);
    require(view.save_generation()==save_generation+1,"F5 failed: "+view.last_message());
    const auto saved=oe::persistence::read_save(path);
    require(saved.source_schema_version==44 && saved.world==before,"F5 schema44 changed actual World authority");
    const auto load_generation=view.load_generation();key(view,SDLK_F9);
    require(view.load_generation()==load_generation+1 && view.world().snapshot()==before,"F9 changed actual-data World authority: "+view.last_message());
    return {{"path",path.string()},{"tick",before.ticks},{"f5",true},{"f9_exact",true},{"schema",44}};
}
void center(oe::SandboxView& view,oe::scene::Point target) {
    for(int i=0;i<800;++i) {
        const auto point=view.camera().world_to_screen(target);const auto box=view.layout().map;
        const double dx=box.x+box.w*.5-point.x,dy=box.y+box.h*.5-point.y;
        if(std::abs(dx)<.1 && std::abs(dy)<.1)return;
        oe::SandboxView::PanKeyState keys;keys.left=dx>0;keys.right=dx<0;
        if(std::abs(dx)>.1)view.update(std::min(std::abs(dx)/400.,.05),keys);
        keys={};keys.up=dy>0;keys.down=dy<0;
        if(std::abs(dy)>.1)view.update(std::min(std::abs(dy)/400.,.05),keys);
    }
    throw std::runtime_error("Production camera pan did not reach the Quay capture target");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& output) {
    const auto before=view.world().snapshot();require(view.render(),SDL_GetError());
    auto* source=SDL_RenderReadPixels(renderer,nullptr);require(source,SDL_GetError());
    auto* rgba=SDL_ConvertSurface(source,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(source);require(rgba,SDL_GetError());
    oe::assets::RgbaImage image;image.width=static_cast<std::uint16_t>(rgba->w);image.height=static_cast<std::uint16_t>(rgba->h);
    image.pixels.resize(static_cast<std::size_t>(rgba->w)*static_cast<std::size_t>(rgba->h)*4);
    for(int y=0;y<rgba->h;++y)std::copy_n(static_cast<const std::uint8_t*>(rgba->pixels)+y*rgba->pitch,rgba->w*4,
        image.pixels.data()+static_cast<std::size_t>(y)*static_cast<std::size_t>(rgba->w)*4);
    SDL_DestroySurface(rgba);const auto png=oe::assets::encode_rgba_png(image);
    std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char*>(png.data()),static_cast<std::streamsize>(png.size()));
    require(bool(file),"Screenshot write failed");require(view.world().snapshot()==before,"Rendering changed actual World authority");
}
bool select_quay(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId quay,std::uint32_t border) {
    view.set_tool(5);
    for(const auto cell:view.world().fishing_quay_cells(quay)) {
        const auto ground=oe::maps::terrain_ground({static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border);
        const auto screen=view.camera().world_to_screen(ground);
        float x=0,y=0;
        if(!SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(screen.x),static_cast<float>(screen.y),&x,&y))continue;
        bool running=true;SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button=SDL_BUTTON_LEFT;event.button.down=true;event.button.x=x;event.button.y=y;
        view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;view.handle_event(event,running);
        if(view.selected_building()==quay)return true;
    }
    return false;
}
} // namespace
