// Supplied-data proof through paid production World commands only. The shared
// helpers' synthetic Fixture types are never instantiated by this executable.
#include "../tests/LuxuryProgressionFixture.h"
#include "../tests/OriginalLongplayFixture.h"
#include "FoodNativePlaytest.h"

#include <charconv>
#include <chrono>
#include <limits>
#include <map>
#include <set>

namespace l=oe_luxury_progression;
namespace p=oe_food_playtest;
namespace {
using Id=sim::BuildingId;
constexpr const char* usage="openemperor-seasonal-agriculture-acceptance --data ABS --output FRESH_.local/agri-mac07/SUBDIR [--map Cities/Chengdu.map] [--ticks 80000] [--replay EXACT48_SAVE --expected CREATOR_FINAL_SAVE]";
constexpr std::array crop_commands{sim::CommandType::PlaceOriginalWheatField,sim::CommandType::PlaceOriginalMilletField,
    sim::CommandType::PlaceOriginalRiceField,sim::CommandType::PlaceOriginalCabbageField,sim::CommandType::PlaceOriginalSoybeanField};
struct Options {fs::path data,output,replay,expected,map="Cities/Chengdu.map";unsigned ticks=80000;};
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
    const auto allowed=fs::weakly_canonical(fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac07");
    l::check(result.output!=allowed&&l::below(result.output,allowed)&&!l::below(result.output,result.data)&&
        !l::below(result.data,result.output)&&!fs::exists(result.output),"Use a fresh private .local/agri-mac07 subdirectory disjoint from Original data");
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
    sim::World create()const{return sim::World::create_seasonal_agriculture(source.map.policy,source.model,source.terrain);}
    l::save::SaveDocument document(const sim::World& world)const{return source.document(world);}
    sim::World restore(const l::save::SaveDocument& document)const {
        l::check(document.source_schema_version==48&&document.world.rule_version==32,"Exact Rule32/schema48 input required");
        l::check(document.map_relative==source.map.map_relative,"Replay must retain the explicitly selected source map");
        return l::save::restore_seasonal_agriculture(document,source.map.data,source.map.mask,source.map.policy,source.model,source.terrain);
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
    p::Plan paid;std::optional<Id> quay,tent,warehouse,hemp,elite,common_recipient;
    std::array<std::optional<Id>,3> plantations;
    Json joint_planning=Json::object();
};
bool future_elite_clear(const sim::World&,const Scene&);
bool outside_elite(const sim::World&,const Scene&,sim::Object,sim::Cell);
bool paused_des_provider(sim::Object kind) {
    return kind==sim::Object::OriginalConfucianAcademy219||kind==sim::Object::OriginalDramaSchool213||
        kind==sim::Object::OriginalBuddhistShrine217;
}
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
bool add_second_tea_bush(sim::World& world,Scene& scene) {
    if(!scene.plantations[0])return false;
    const auto source=*scene.plantations[0];const auto origin=world.building(source).cell;
    for(int y=origin.y-3;y<=origin.y+4;++y)for(int x=origin.x-3;x<=origin.x+4;++x) {
        sim::Command bush{sim::CommandType::PlaceOriginalTeaBush,{x,y}};bush.building_id=source;
        if(!outside_elite(world,scene,sim::Object::OriginalTeaBush26,bush.cell)||!world.validate(bush).accepted)continue;
        auto candidate=world;auto proposed=scene.paid;
        if(p::buy(candidate,proposed,bush)&&routes_to_all_food(candidate)&&routes_to_all_plants(candidate)&&future_elite_clear(candidate,scene)) {
            world=std::move(candidate);scene.paid=std::move(proposed);return true;
        }
    }
    return false;
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
bool future_elite_clear(const sim::World& world,const Scene& scene) {
    return !scene.elite || world.building(*scene.elite).kind!=sim::Object::OriginalElitePlot ||
        world.original_elite_materialization_status(*scene.elite).ready;
}
bool outside_elite(const sim::World& world,const Scene& scene,sim::Object kind,sim::Cell cell) {
    if(!scene.elite)return true;
    const auto origin=world.building(*scene.elite).cell;
    for(const auto occupied:sim::building_footprint_cells(world.profile(),world.rule_version(),kind,cell))
        if(occupied.x>=origin.x&&occupied.x<origin.x+4&&occupied.y>=origin.y&&occupied.y<origin.y+4)return false;
    return true;
}
std::optional<Id> add_city_body(sim::World& world,Scene& scene,sim::CommandType type,bool far_from_core=false) {
    const auto kind=sim::placed_object(type);l::check(kind.has_value(),"Paid seasonal extension has an unknown construction command");
    auto cells=nearby(world,scene.paid);
    {
        const auto center=world.building(scene.elite.value_or(scene.paid.market)).cell;
        std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){
            return far_from_core ? p::distance(a,center)>p::distance(b,center):p::distance(a,center)<p::distance(b,center);
        });
    }
    const bool rank_desirability=scene.elite&&sim::original_influence_model(*kind).has_value();
    std::optional<sim::World> best;std::optional<p::Plan> best_plan;std::optional<Id> best_id;
    int best_score=std::numeric_limits<int>::min();unsigned attempts=0;
    for(const auto cell:cells) {
        const sim::Command command{type,cell};
        if(!outside_elite(world,scene,*kind,cell)||!world.validate(command).accepted)continue;
        if(++attempts>(rank_desirability?48U:128U))break;
        auto candidate=world;auto proposed=scene.paid;
        if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(cell);
        int score=0;
        if(rank_desirability) {
            const int elite=std::min(60,candidate.original_housing_diagnosis(*scene.elite).cached_desirability);
            const int common=scene.common_recipient ? std::min(40,candidate.original_housing_diagnosis(*scene.common_recipient).cached_desirability):0;
            score=4*elite+3*common;
            // Roads are not influence sources. Once a better legal layout is
            // retained, an equal/lower DES candidate needs no road construction.
            if(best&&score<=best_score)continue;
        }
        if(type==sim::CommandType::PlaceOriginalElitePlot && !candidate.original_elite_materialization_status(source).ready)continue;
        if(type!=sim::CommandType::PlaceOriginalInfluenceSource&&!paused_des_provider(*kind)&&!connect(candidate,proposed,source))continue;
        if(type==sim::CommandType::PlaceOriginalElitePlot && !candidate.original_elite_materialization_status(source).ready)continue;
        if(!future_elite_clear(candidate,scene)||!routes_to_all_food(candidate)||!routes_to_all_plants(candidate))continue;
        if(!rank_desirability) {
            world=std::move(candidate);scene.paid=std::move(proposed);
            if(type==sim::CommandType::PlaceHousehold)scene.paid.homes.push_back(source);
            return source;
        }
        // The actual prepared World cache scores this finite paid layout.
        // No copied stencil, synthetic DES assignment or source admission is used.
        if(score>best_score) {
            best_score=score;best=std::move(candidate);best_plan=std::move(proposed);best_id=source;
        }
    }
    if(best){world=std::move(*best);scene.paid=std::move(*best_plan);return best_id;}
    return {};
}
bool add_supplemental_food(sim::World& world,Scene& scene) {
    auto candidate=world;auto proposed=scene;
    const auto farm=add_city_body(candidate,proposed,sim::CommandType::PlaceOriginalFoodFarm);
    if(!farm)return false;
    const auto origin=candidate.building(*farm).cell;
    std::vector<sim::Cell> cells;
    for(int y=origin.y-3;y<=origin.y+5;++y)for(int x=origin.x-3;x<=origin.x+5;++x)cells.push_back({x,y});
    std::stable_sort(cells.begin(),cells.end(),[&](auto a,auto b){return p::distance(a,origin)<p::distance(b,origin);});
    for(const auto type:std::array{sim::CommandType::PlaceOriginalSoybeanField,
        sim::CommandType::PlaceOriginalMilletField,sim::CommandType::PlaceOriginalCabbageField}) {
        bool planted=false;
        for(const auto cell:cells) {
            sim::Command field{type,cell};field.building_id=*farm;
            const auto kind=*sim::placed_object(type);
            if(!outside_food_domain(candidate,proposed.paid.farm,kind,cell)||!outside_elite(candidate,proposed,kind,cell)||
               !candidate.validate(field).accepted)continue;
            auto planted_world=candidate;auto paid=proposed.paid;
            if(p::buy(planted_world,paid,field)&&routes_to_all_food(planted_world)&&routes_to_all_plants(planted_world)&&future_elite_clear(planted_world,proposed)) {
                candidate=std::move(planted_world);proposed.paid=std::move(paid);planted=true;break;
            }
        }
        if(!planted)return false;
    }
    world=std::move(candidate);scene=std::move(proposed);return true;
}
void add_luxury_city(sim::World& world,Scene& scene) {
    // Finite authored player geometry, independently proved with the actual
    // Chengdu permission/terrain/model context. Every command revalidates below.
    const auto buy=[&](sim::CommandType type,sim::Cell cell) {
        l::check(p::buy(world,scene.paid,{type,cell}),"Source-bound seasonal core purchase refused");
        return world.building_owner_at(cell).value_or(Id{});
    };
    scene.elite=buy(sim::CommandType::PlaceOriginalElitePlot,{157,149});
    scene.common_recipient=buy(sim::CommandType::PlaceHousehold,{154,150});
    scene.paid.homes.push_back(*scene.common_recipient);
    constexpr std::array<sim::Cell,41> first_roads{{
        {157,148},{158,148},{159,148},{160,148},{161,148},{161,149},{161,150},{161,151},
        {157,153},{158,153},{159,153},{131,143},{132,143},{133,143},{134,143},{135,143},
        {136,143},{137,143},{138,143},{139,143},{140,143},{141,143},{142,143},{143,143},
        {144,143},{145,143},{146,143},{147,143},{148,143},{149,143},{150,143},{151,143},
        {152,143},{153,143},{154,143},{155,143},{157,143},{157,144},{157,145},{157,146},{157,147}}};
    for(const auto cell:first_roads)buy(sim::CommandType::PlaceRoad,cell);
    using C=sim::CommandType;
    struct Provider {C command;sim::Cell cell;};
    constexpr std::array providers{
        Provider{C::PlaceOriginalDaoistShrine,{154,147}},Provider{C::PlaceOriginalDaoistShrine,{154,152}},
        Provider{C::PlaceOriginalBuddhistShrine,{152,147}},Provider{C::PlaceOriginalBuddhistShrine,{152,149}},
        Provider{C::PlaceOriginalAncestorShrine,{154,145}},Provider{C::PlaceOriginalConfucianAcademy,{151,144}},
        Provider{C::PlaceOriginalConfucianAcademy,{158,144}},Provider{C::PlaceOriginalAcrobatSchool,{161,145}},
        Provider{C::PlaceOriginalDramaSchool,{151,151}},Provider{C::PlaceOriginalMusicSchool,{154,154}},
        Provider{C::PlaceOriginalWaterWell,{158,142}},Provider{C::PlaceHealthPost,{162,148}},
        Provider{C::PlaceServicePost,{154,156}},Provider{C::PlaceOriginalInfluenceSource,{152,154}},
        Provider{C::PlaceOriginalInfluenceSource,{160,142}},Provider{C::PlaceOriginalInfluenceSource,{162,142}},
        Provider{C::PlaceOriginalInfluenceSource,{164,142}}};
    std::vector<Id> connected;
    for(const auto& provider:providers) {
        const auto id=buy(provider.command,provider.cell);
        if(provider.command!=C::PlaceOriginalInfluenceSource&&!paused_des_provider(world.building(id).kind))connected.push_back(id);
    }
    for(const auto id:connected)l::check(connect(world,scene.paid,id),"Active source-bound provider lacks a real route");
    const auto second=buy(C::PlaceHousehold,{149,149});scene.paid.homes.push_back(second);
    for(int y=144;y<=148;++y)buy(C::PlaceRoad,{149,y});
    const auto third=buy(C::PlaceHousehold,{150,147});scene.paid.homes.push_back(third);
    for(const auto id:scene.paid.homes)l::check(connect(world,scene.paid,id),"Actual Common source-bound route is absent");
    l::check(connect(world,scene.paid,*scene.elite)&&future_elite_clear(world,scene)&&routes_to_all_food(world),
        "Paid core lost the real Elite geometry or physical Food routes");
    Json common=Json::array();for(const auto id:scene.paid.homes)
        common.push_back({{"id",static_cast<unsigned>(id)},{"query_des",world.original_housing_diagnosis(id).cached_desirability}});
    scene.joint_planning["luxury_progression"]={{"geometry","authored-chengdu-waterfront-core-v1"},
        {"elite",static_cast<unsigned>(*scene.elite)},{"pending_elite_desirability",world.original_housing_diagnosis(*scene.elite).cached_desirability},
        {"actual_common_desirability",common},{"all_actual_routes",true},{"background_houses",0}};
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
        if(!outside_food_domain(candidate,proposed.farm,sim::Object::OriginalFishingQuay,command.cell)||
            !outside_elite(candidate,scene,sim::Object::OriginalFishingQuay,command.cell)||!candidate.validate(command).accepted)continue;
        if(!p::buy(candidate,proposed,command))continue;
        const auto source=*candidate.building_owner_at(option.cell);const auto dock=orig::fishing_quay_geometry(option.cell,option.orientation).dock;bool water=false;
        for(const auto& site:candidate.fishing_state()->sites)if(site.cell!=dock&&candidate.find_fishing_route(dock,site.cell,source)){water=true;break;}
        if(water&&connect(candidate,proposed,source)&&future_elite_clear(candidate,scene)){world=std::move(candidate);scene.paid=std::move(proposed);scene.quay=source;break;}
    }
    scene.joint_planning["fishing"]={{"attempts",attempts},{"source_qualified_global_quays",options.size()},{"planned",scene.quay.has_value()}};
}
Scene choose_scene(const sim::World& base) {
    auto candidate=base;Scene scene;
    const auto blueprints=p::city_blueprints();const auto selected=std::find_if(blueprints.begin(),blueprints.end(),
        [](const auto& layout){return layout.name=="linear8";});
    l::check(selected!=blueprints.end(),"Shared native blueprint is absent");auto layout=*selected;
    // Only the three deliberately supplied core Commons become consumers.
    std::erase_if(layout.commands,[](const auto& command) {
        return command.type==sim::CommandType::PlaceHousehold||
            (command.type==sim::CommandType::PlaceFireWatch&&command.cell.x==16);
    });
    layout.name="seasonal-three-common-core";
    l::check(p::city(candidate,scene.paid,layout,{107,134})&&five_fields(candidate,scene.paid),
        "Source-bound paid primary five-crop Farm blueprint refused");
    add_luxury_city(candidate,scene);
    const auto core_market=add_city_body(candidate,scene,sim::CommandType::PlaceMarket);
    l::check(core_market.has_value(),"Paid core Market has no admitted connected site");scene.paid.market=*core_market;
    l::check(add_city_body(candidate,scene,sim::CommandType::PlaceFireWatch).has_value(),"Paid core FireWatch is absent");
    for(const auto command:std::array{sim::CommandType::PlaceClaySource,sim::CommandType::PlacePottery,
        sim::CommandType::PlaceOriginalLoggingShed,sim::CommandType::PlaceOriginalWeaver,
        sim::CommandType::PlaceOriginalLacquerwareMaker})
        l::check(add_city_body(candidate,scene,command).has_value(),"A paid downstream processor has no real admitted route");
    l::check(add_supplemental_food(candidate,scene),"Paid high-quality supplemental Farm is absent");
    scene.warehouse=add_city_body(candidate,scene,sim::CommandType::PlaceWarehouse);
    add_hemp(candidate,scene);add_hemp(candidate,scene);
    for(unsigned kind=0;kind<3;++kind)add_plantation(candidate,scene,kind);
    scene.joint_planning["second_tea_bush"]=add_second_tea_bush(candidate,scene);
    add_fishing(candidate,scene,qualified_quays(candidate));
    l::check(scene.warehouse&&scene.hemp&&candidate.seasonal_agriculture_state()->hemp_farms.size()==2&&
        std::all_of(scene.plantations.begin(),scene.plantations.end(),[](const auto& id){return id.has_value();})&&
        scene.joint_planning["second_tea_bush"].get<bool>(),"The paid nine-plant source chains are incomplete");
    l::check(scene.paid.homes.size()==3&&routes_to_all_food(candidate)&&routes_to_all_plants(candidate)&&future_elite_clear(candidate,scene),
        "The four-House city lost actual identities, routes or future geometry");
    return scene;
}
Id primary_farm(const sim::World& world) {
    std::optional<Id> selected;
    for(const auto& farm:world.physical_food_growers_state()->farms) {
        std::set<sim::AgricultureCrop> crops;
        for(const auto& field:world.hunting_food_state()->fields)if(field.farm_id==farm.farm_id)crops.insert(field.crop);
        if(crops.size()!=5)continue;
        l::check(!selected,"This proof requires one uniquely identified primary five-crop Farm");selected=farm.farm_id;
    }
    l::check(selected.has_value(),"This proof requires all five actual paid crop kinds on its primary Farm");return *selected;
}
Json balances(const sim::World& world) {
    l::check(world.rule_version()==32&&world.physical_food_growers_state()&&world.hunting_food_state()&&
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
    l::check(restored.snapshot()==world.snapshot(),"Exact schema48 checkpoint changed physical authority");
    const auto roundtrip=output/(name+".roundtrip.json");l::save::write_save(roundtrip,context.document(restored),context.source.map.data,context.source.map.mask);
    l::check(l::q::f::read_text(path)==l::q::f::read_text(roundtrip),"Canonical schema48 roundtrip bytes differ");
    if(recovery) {
        const auto entry=recovery->write_checkpoint(history,document,context.source.map.mask);
        l::check(entry.loadable()&&entry.schema==48&&context.restore(l::save::read_save(entry.path)).snapshot()==world.snapshot(),"Recovery changed physical Grower authority");
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
            if(field.harvested>before) {
                const auto& actual=worker_source(world,field.farm_id);
                l::check(actual.latest_trip&&actual.latest_trip->field_id==field.field_id&&
                    actual.latest_trip->harvested_tick==world.ticks()&&sim::physical_food_trip_cargo(*actual.latest_trip)==1,
                    "A field harvest appeared without this tick's actual worker tending and retained typed cargo");
            }
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
                    {"returned_tick",receipt.returned_tick},{"work_at_harvest",receipt.work_at_harvest},
                    {"wasted_work_at_harvest",receipt.wasted_work_at_harvest},{"ready_tick",receipt.ready_tick}});
                l::check(receipt.ready_tick<=receipt.harvested_tick&&
                    receipt.harvested_tick-receipt.ready_tick>=sim::seasonal_harvest_work_ticks,
                    "Seasonal Field harvest skipped its actual post-readiness work interval");
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
struct SeasonalObservation {
    struct Previous {std::uint64_t work=0,returned=0,expired=0,wasted=0;bool ready=false;};
    std::map<Id,Previous> previous;
    std::array<std::uint64_t,9> actual_work{},returns{},expired{},wasted{};
    std::array<bool,9> ready{},offseason_wait{};
    std::array<std::uint64_t,4> material_arrived{}; // Hemp, Tea, Lacquer, RawSilk.
    std::map<std::pair<sim::CourierId,std::uint64_t>,std::int32_t> seen_goods,seen_aux;
    Json events=Json::array(),material_transfers=Json::array();
    void row(const sim::World& world,Id id,unsigned kind,orig::ModelBuilding model,std::uint64_t work,
             std::uint64_t returned,std::uint64_t expired_count,std::uint64_t wasted_count,bool is_ready,bool observe) {
        auto& before=previous[id];
        l::check(work>=before.work&&returned>=before.returned&&expired_count>=before.expired&&wasted_count>=before.wasted,
            "Seasonal lifetime work/return/expiration accounting moved backwards");
        if(observe) {
            const auto delta=work-before.work;
            l::check(!delta||sim::seasonal_growing(model,world.ticks()),"Seasonal productive work appeared outside its authored growing interval");
            actual_work.at(kind)+=delta;returns.at(kind)+=returned-before.returned;
            expired.at(kind)+=expired_count-before.expired;wasted.at(kind)+=wasted_count-before.wasted;
            ready.at(kind)|=is_ready;offseason_wait.at(kind)|=!sim::seasonal_growing(model,world.ticks())&&!delta;
            if(is_ready!=before.ready||returned!=before.returned||expired_count!=before.expired||wasted_count!=before.wasted)
                events.push_back({{"tick",world.ticks()},{"year",world.ticks()/sim::seasonal_year_ticks},
                    {"subject",static_cast<unsigned>(id)},{"kind",kind},{"ready",is_ready},
                    {"returned_delta",returned-before.returned},{"expired_delta",expired_count-before.expired},
                    {"wasted_work_delta",wasted_count-before.wasted}});
        }
        before={work,returned,expired_count,wasted_count,is_ready};
    }
    void after(const sim::World& world,bool observe=true) {
        const auto& state=*world.seasonal_agriculture_state();
        for(const auto& crop:world.hunting_food_state()->fields) {
            const auto& season=*std::find_if(state.fields.begin(),state.fields.end(),[&](const auto& f){return f.field_id==crop.field_id;});
            const auto& work=*std::find_if(world.multi_field_agriculture_state()->fields.begin(),world.multi_field_agriculture_state()->fields.end(),
                [&](const auto& f){return f.field_id==crop.field_id;});
            const auto& receipt=*std::find_if(world.physical_food_growers_state()->fields.begin(),world.physical_food_growers_state()->fields.end(),
                [&](const auto& f){return f.field_id==crop.field_id;});
            const auto kind=static_cast<unsigned>(crop.crop);
            row(world,crop.field_id,kind,static_cast<orig::ModelBuilding>(195+kind),work.productive_ticks,receipt.returned,
                season.expired_ready,season.wasted_work,season.ready_tick.has_value(),observe);
        }
        for(const auto& farm:state.hemp_farms) {
            const auto& source=*std::find_if(world.original_production_state()->hemp_farms.begin(),world.original_production_state()->hemp_farms.end(),
                [&](const auto& f){return f.farm_id==farm.farm_id;});
            if(source.field_id)row(world,*source.field_id,5,orig::ModelBuilding::HempField,farm.productive_ticks,source.harvested,
                farm.expired_ready,farm.wasted_work,farm.ready_tick.has_value(),observe);
        }
        for(const auto& plant:world.plantations_state()->plants) {
            const auto& season=*std::find_if(state.plants.begin(),state.plants.end(),[&](const auto& p){return p.plant_id==plant.plant_id;});
            const auto body=world.building(plant.plant_id).kind;
            const unsigned kind=body==sim::Object::OriginalTeaBush26 ? 6:body==sim::Object::OriginalLacquerTree27 ? 7:8;
            const auto model=kind==6?orig::ModelBuilding::TeaBush:kind==7?orig::ModelBuilding::LacquerTree:orig::ModelBuilding::MulberryTree;
            row(world,plant.plant_id,kind,model,plant.productive_ticks,plant.returned,season.expired_ready,
                season.wasted_work,season.ready_tick.has_value(),observe);
        }
        for(const auto& trip:world.original_goods_state()->reservations) {
            if(world.building(trip.target).kind!=sim::Object::Warehouse)continue;
            const unsigned kind=trip.good==orig::Goods::Hemp?0:trip.good==orig::Goods::Tea?1:4;
            if(kind==4)continue;
            auto& old=seen_goods[{trip.transfer.courier_id,trip.transfer.sequence}];
            l::check(trip.delivered>=old,"Material Goods delivery counter moved backwards");
            if(observe&&trip.delivered>old) {
                material_arrived[kind]+=static_cast<std::uint64_t>(trip.delivered-old);
                material_transfers.push_back({{"tick",world.ticks()},{"kind",kind},{"source",static_cast<unsigned>(trip.source)},
                    {"target",static_cast<unsigned>(trip.target)},{"courier",static_cast<unsigned>(trip.transfer.courier_id)},
                    {"sequence",trip.transfer.sequence},{"actual_arrived",trip.delivered-old}});
            }
            old=trip.delivered;
        }
        for(const auto& trip:world.original_production_state()->industry.aux_trips) {
            if(world.building(trip.target).kind!=sim::Object::Warehouse)continue;
            const unsigned kind=trip.material==sim::OriginalAuxMaterial::Lacquer?2:trip.material==sim::OriginalAuxMaterial::RawSilk?3:4;
            if(kind==4)continue;
            auto& old=seen_aux[{trip.transfer.courier_id,trip.transfer.sequence}];
            l::check(trip.delivered>=old,"Material Aux delivery counter moved backwards");
            if(observe&&trip.delivered>old) {
                material_arrived[kind]+=static_cast<std::uint64_t>(trip.delivered-old);
                material_transfers.push_back({{"tick",world.ticks()},{"kind",kind},{"source",static_cast<unsigned>(trip.source)},
                    {"target",static_cast<unsigned>(trip.target)},{"courier",static_cast<unsigned>(trip.transfer.courier_id)},
                    {"sequence",trip.transfer.sequence},{"actual_arrived",trip.delivered-old}});
            }
            old=trip.delivered;
        }
    }
    Json report()const {
        Json rows=Json::array();
        constexpr std::array names{"Wheat","Millet","Rice","Cabbage","SoyBeanCurd","Hemp","Tea","Lacquer","Mulberry"};
        for(unsigned kind=0;kind<9;++kind)rows.push_back({{"kind",names[kind]},{"actual_productive_work",actual_work[kind]},
            {"actual_source_returns",returns[kind]},{"ready_observed",ready[kind]},
            {"offseason_work_veto_observed",offseason_wait[kind]},{"expired_ready",expired[kind]},{"wasted_work",wasted[kind]}});
        return rows;
    }
    bool complete()const {
        for(unsigned kind=0;kind<9;++kind)if(!actual_work[kind]||!returns[kind]||!ready[kind]||!offseason_wait[kind])return false;
        return std::all_of(material_arrived.begin(),material_arrived.end(),[](auto amount){return amount>0;});
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
    if(!saved.contains("crop-ready"))for(const auto& field:world.seasonal_agriculture_state()->fields)
        if(field.ready_tick)return "crop-ready";
    if(!saved.contains("hemp-grower-loaded"))for(const auto& hemp_source:world.seasonal_agriculture_state()->hemp_farms)
        if(hemp_source.latest_trip&&sim::seasonal_hemp_cargo(*hemp_source.latest_trip)>0&&world.courier(hemp_source.courier_id).edge_progress>0)
            return "hemp-grower-loaded";
    if(!saved.contains("plantation-grower-loaded"))for(const auto& plant_source:world.plantations_state()->sources)
        if(plant_source.latest_trip&&plant_source.latest_trip->picked_tick&&!plant_source.latest_trip->returned_tick&&world.courier(plant_source.courier_id).edge_progress>0)
            return "plantation-grower-loaded";
    return {};
}
struct CityStaffing {std::vector<Id> initial,enable;};
CityStaffing city_staffing(const sim::World& world) {
    CityStaffing plan;std::vector<sim::Cell> houses;
    for(const auto& body:world.buildings())if(body.kind==sim::Object::Household)houses.push_back(body.cell);
    l::check(houses.size()==3,"The seasonal proof requires exactly three actual Common consumers");
    const auto all=[&](sim::Object kind) {
        std::vector<Id> ids;for(const auto& body:world.buildings())if(body.kind==kind)ids.push_back(body.id);return ids;
    };
    const auto closest=[&](sim::Object kind) {
        const auto ids=all(kind);l::check(!ids.empty(),"The staged city lacks a paid source");
        const auto distance=[&](Id id){int sum=0;for(const auto cell:houses)sum+=p::distance(world.building(id).cell,cell);return sum;};
        return *std::min_element(ids.begin(),ids.end(),[&](Id a,Id b){return distance(a)<distance(b);});
    };
    const auto primary=primary_farm(world);
    plan.initial={closest(sim::Object::Market),closest(sim::Object::OriginalWaterWell72),closest(sim::Object::OriginalAncestorShrine214)};
    for(const auto id:all(sim::Object::FireWatch))plan.initial.push_back(id);
    // Three initial Shelters plus the Elite admit only 26 residents. Continuous
    // physical Fish fits that first population and unlocks Food growth before
    // the 22-worker Farm and the seasonal material chains can be staffed.
    for(const auto id:all(sim::Object::OriginalFishingQuay))plan.enable.push_back(id);
    plan.enable.push_back(primary);
    plan.enable.push_back(closest(sim::Object::Warehouse));
    const auto hemp=all(sim::Object::OriginalHempFarm);
    l::check(hemp.size()==2,"The seasonal consumer proof requires two paid Hemp Farms");
    plan.enable.push_back(hemp.front());
    plan.enable.push_back(closest(sim::Object::HealthPost));
    plan.enable.push_back(closest(sim::Object::OriginalMusicSchool211));
    plan.enable.push_back(hemp.back());
    for(const auto kind:std::array{sim::Object::ClaySource,sim::Object::Pottery,sim::Object::ServicePost,sim::Object::OriginalAcrobatSchool212,
        sim::Object::OriginalDaoistShrine215,sim::Object::OriginalTeaCuringShed237,
        sim::Object::OriginalSilkwormShed239,sim::Object::OriginalWeaver47,sim::Object::OriginalLoggingShed38,
        sim::Object::OriginalLacquerRefinery238,sim::Object::OriginalLacquerwareMaker44})plan.enable.push_back(closest(kind));
    for(const auto id:all(sim::Object::OriginalFoodFarm))if(id!=primary)plan.enable.push_back(id);
    return plan;
}
void initialize_staffing(sim::World& world,const CityStaffing& plan,Json& commands) {
    std::vector<Id> ids;for(const auto& body:world.buildings())if(world.workforce_required(body.id)>0)ids.push_back(body.id);
    for(const auto id:ids) {
        const bool initial=std::find(plan.initial.begin(),plan.initial.end(),id)!=plan.initial.end();
        const bool wanted=initial||std::find(plan.enable.begin(),plan.enable.end(),id)!=plan.enable.end();
        if(world.building(id).operating_enabled!=initial)paid_command(world,sim::set_building_operation(id,initial),commands,
            "MAC07 paid staged startup; inactive physical DES retains installed upkeep");
        const auto priority=wanted?sim::WorkforcePriority::High:sim::WorkforcePriority::Low;
        if(world.building(id).workforce_priority!=priority)paid_command(world,sim::set_building_workforce_priority(id,priority),commands,
            "MAC07 staged existing source workforce priority");
    }
}
void advance_staffing(sim::World& world,const CityStaffing& plan,Json& commands) {
    std::uint64_t enabled=0;
    for(const auto& body:world.buildings())if(body.operating_enabled)enabled+=static_cast<std::uint64_t>(world.workforce_required(body.id));
    for(const auto id:plan.enable) {
        if(world.building(id).operating_enabled)continue;
        const auto required=static_cast<std::uint64_t>(world.workforce_required(id));
        if(enabled+required>static_cast<std::uint64_t>(world.total_population()))break;
        paid_command(world,sim::set_building_operation(id,true),commands,
            "MAC07 enable actual paid chain after naturally admitted residents cover its complete model staff");
        enabled+=required;
    }
}
}

int main(int argc,char** argv) {
    fs::path output;Json commands=Json::array();std::uint64_t last_completed_tick=0;
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
            for(const auto command:scene.paid.commands)paid_command(world,command,commands,"MAC07 actual paid source-bound scene");
            initialize_staffing(world,city_staffing(world),commands);
            l::check(world.ticks()==0&&!world.total_population(),"Paid setup created elapsed time or residents");
            for(const auto& stock:world.hunting_food_state()->stocks)for(const auto amount:stock.amounts)l::check(!amount,"Paid setup injected Food");
            l::write(output/"planning.json",planning);
        }
        const auto farm=primary_farm(world);const auto begin=world.ticks();
        l::check(begin<=UINT64_MAX-selected.ticks,"Continuation clock would overflow");std::uint64_t end=begin+selected.ticks;
        std::optional<l::save::SaveDocument> expected;
        if(!selected.expected.empty()) {
            expected=l::save::read_save(selected.expected);(void)context.restore(*expected);end=expected->world.ticks;
            l::check(end>=begin&&end-begin>=12000&&end-begin<=100000,"Separate replay requires12000..100000 real continuation ticks");
        }
        const auto staffing=city_staffing(world);
        Json manifest=Json::array();checkpoint(context,world,output,"seed",manifest);
        {
            const auto unchanged=world.snapshot();auto geometry=world;
            for(unsigned tick=0;tick<240;++tick)geometry.tick();
            int common_des=100,elite_des=-100;unsigned commons=0;Json subjects=Json::array();
            for(const auto& home:geometry.original_economy_state()->houses)if(home.house.occupants>0) {
                const auto& body=geometry.building(home.building_id);const auto diagnosis=geometry.original_housing_diagnosis(home.building_id);
                subjects.push_back({{"owner",static_cast<unsigned>(home.building_id)},{"cell",{body.cell.x,body.cell.y}},
                    {"raw_des",diagnosis.raw_desirability},{"actual_cardinal_bonus",diagnosis.cardinal_water_bonus},
                    {"query_des",diagnosis.cached_desirability}});
                if(body.kind==sim::Object::OriginalEliteHouse)elite_des=std::max(elite_des,diagnosis.cached_desirability);
                else if(body.kind==sim::Object::Household){common_des=std::min(common_des,diagnosis.cached_desirability);++commons;}
            }
            l::check(world.snapshot()==unchanged,"Copied geometry probe changed the real creator");
            l::write(output/"geometry-probe.json",{{"scope","Separate normally paid World copy; no state transfer"},
                {"real_probe_ticks",240},{"creator_tick",world.ticks()},{"creator_population",world.total_population()},
                {"subjects",subjects},{"creator_unchanged",true}});
            l::check(commons==3&&common_des>=40&&elite_des>=60,"Paid occupied core misses actual DES gates after240 real copied-World ticks: Common="+
                std::to_string(common_des)+" Elite="+std::to_string(elite_des));
        }
        auto replay=context.restore(l::save::read_save(output/"seed.save.json"));
        l::save::RecoveryStore recovery(output/"recovery",selected.data,l::save::recovery_storage_budget_bytes,l::save::RecoveryAccess::OriginalProduction);
        const auto history=recovery.create_history(l::save::read_save(output/"seed.save.json"),context.source.map.mask);
        Observation observed;observed.seed(world,farm);SeasonalObservation seasonal;seasonal.after(world,false);std::set<std::string> saved;
        l::Observation luxury;std::array<std::uint64_t,9> previous_annual_returns{};
        std::array<std::uint64_t,3> previous_annual_luxury{};unsigned ongoing_years=0,longest_ongoing_years=0;
        auto minimum_funds=world.treasury();Json annual=Json::array();
        while(world.ticks()<end) {
            Json issued=Json::array(),repeated=Json::array();
            advance_staffing(world,staffing,issued);advance_staffing(replay,staffing,repeated);
            l::check(issued==repeated,"Independent restored staged paid commands diverged");
            for(const auto& command:issued)commands.push_back(command);
            luxury.before(world);world.tick();replay.tick();observed.after(world,farm);seasonal.after(world);luxury.after(world);
            last_completed_tick=world.ticks();
            minimum_funds=std::min(minimum_funds,world.treasury());
            if(world.ticks()%sim::seasonal_year_ticks==0) {
                std::array<std::uint64_t,9> returned_delta{};std::array<std::uint64_t,3> luxury_total{},luxury_delta{};
                bool continued=true;
                for(unsigned kind=0;kind<9;++kind) {
                    returned_delta[kind]=seasonal.returns[kind]-previous_annual_returns[kind];
                    continued&=returned_delta[kind]>0;previous_annual_returns[kind]=seasonal.returns[kind];
                }
                for(unsigned good=0;good<3;++good) {
                    luxury_total[good]=world.original_goods_balance(l::luxury_goods[good]).totals.consumed;
                    luxury_delta[good]=luxury_total[good]-previous_annual_luxury[good];
                    continued&=luxury_delta[good]>0;previous_annual_luxury[good]=luxury_total[good];
                }
                ongoing_years=continued?ongoing_years+1:0;longest_ongoing_years=std::max(longest_ongoing_years,ongoing_years);
                annual.push_back({{"tick",world.ticks()},{"year",world.ticks()/sim::seasonal_year_ticks},
                    {"population",world.total_population()},{"treasury",world.treasury()},{"source_returns",seasonal.returns},
                    {"actual_source_returns_this_year",returned_delta},{"actual_luxury_consumed_this_year",luxury_delta},
                    {"all_nine_and_luxuries_active_this_year",continued},{"material_warehouse_arrivals",seasonal.material_arrived},
                    {"scene",scene_report(world,farm)}});
            }
            if(world.ticks()<=sim::seasonal_year_ticks &&
               (world.ticks()%sim::seasonal_month_ticks==sim::seasonal_month_ticks-1 ||
                world.ticks()%sim::seasonal_month_ticks==0)) {
                const auto boundary=(world.ticks()+1)/sim::seasonal_month_ticks;
                const auto side=world.ticks()%sim::seasonal_month_ticks ? "before":"after";
                checkpoint(context,world,output,"slot-"+std::to_string(boundary)+"-"+side,manifest,&recovery,history);
            }
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
        const auto actual=scene_report(world,farm);bool positive=world.ticks()-begin>=5*sim::seasonal_year_ticks&&observed.assignments.size()>=5;
        for(const auto* required:{"grower-outward","grower-waiting","grower-working","grower-loaded","food-cart",
            "crop-ready","hemp-grower-loaded","plantation-grower-loaded"})positive&=saved.contains(required);
        Json stages=Json::array();
        for(std::size_t crop=0;crop<5;++crop) {
            const bool complete=std::all_of(observed.stages[crop].begin(),observed.stages[crop].end(),[](bool value){return value;});
            const auto consumed=actual.at("house_consumed")[crop].get<std::uint64_t>();positive&=complete&&consumed>0&&actual.at("market_received")[crop].get<std::uint64_t>()>0;
            stages.push_back({{"crop",crop},{"outward_wait_work_loaded_return_receipt",observed.stages[crop]},
                {"all_observed",complete},{"actual_house_consumed",consumed}});
        }
        positive&=seasonal.complete()&&longest_ongoing_years>=5&&minimum_funds>=0;
        const auto& joint=actual.at("joint");
        const bool joint_positive=joint.at("actual_fish_unloaded").get<std::uint64_t>()>0&&
            joint.at("actual_meat_unloaded").get<std::uint64_t>()>0&&joint.at("actual_plantation_returns").get<std::uint64_t>()>0;
        sources.unchanged();map_input.unchanged();input.unchanged();expected_input.unchanged();const auto status=positive?"PASS":"PARTIAL";
        Json report={{"format","openemperor-seasonal-agriculture-acceptance-v1"},{"status",status},{"rule",32},{"schema",48},
            {"map",selected.map.generic_string()},{"map_sha256",context.source.terrain->source_map_sha256()},
            {"model_sha256",context.source.model->provenance().sha256},{"unchanged_supplied_model",true},
            {"source_bytes_mode_mtime_unchanged",true},{"snapshot_stock_or_population_injection",false},
            {"begin_tick",begin},{"end_tick",world.ticks()},{"real_ticks",world.ticks()-begin},{"farm",static_cast<unsigned>(farm)},
            {"five_crop_actual_stages",stages},{"actual_scene",actual},{"joint_actual_status",joint_positive?"PASS":"PARTIAL"},
            {"joint_planning",planning},{"planning_diagnostics",p::planning_statistics},
            {"physical_worker_transitions",observed.actors},{"actual_farm_receipts",observed.farm_receipts},
            {"actual_food_arrivals",observed.food_arrivals},{"actual_house_consumption",observed.house_meals},
            {"actual_field_assignments",observed.assignments},{"observed_work",observed.observed_work},{"fully_staffed_ticks",observed.fully_staffed_ticks},
            {"complete_calendar_years",(world.ticks()-begin)/sim::seasonal_year_ticks},
            {"minimum_funds",minimum_funds},{"annual_physical_production_and_consumption",annual},
            {"longest_consecutive_all_nine_and_luxury_years",longest_ongoing_years},
            {"luxury_maturity_tick",luxury.maturity ? Json(*luxury.maturity):Json(nullptr)},
            {"actual_luxury_consumption",luxury.consumption},{"actual_luxury_arrivals",luxury.deliveries},
            {"actual_luxury_dispatches",luxury.dispatches},{"actual_luxury_upstream",luxury.upstream},
            {"luxury_city",l::city_report(world)},
            {"nine_type_seasonal_production",seasonal.report()},{"seasonal_transitions",seasonal.events},
            {"actual_material_warehouse_arrivals",seasonal.material_arrived},{"actual_material_transfers",seasonal.material_transfers},
            {"physical_conservation",mass},{"checkpoints",manifest},{"recovery_exact",true},
            {"fresh_restore_continuation_exact",true},{"separate_process_replay_exact",independent},
            {"native_metal","NOT_RUN"},{"human_acceptance","NOT_RUN"},
            {"claim_scope","Paid seasonal five-crop Farm and Hemp/Tea/Lacquer/Mulberry sources; real field work, physical source returns and downstream Food/Material transfers. AUTHORED calendar/work/units; no Original raw/x87 yield claim."}};
        l::write(output/"commands.json",commands);l::write(output/"report.json",report);
        std::cout<<status<<" Rule32/schema48 paid physical five-crop Growers; ticks="<<world.ticks()-begin<<" report="<<output/"report.json"<<'\n';return positive?0:2;
    } catch(const std::exception& error) {
        if(!output.empty()) {
            if(!fs::exists(output/"commands.json"))try{l::write(output/"commands.json",commands);}catch(...){}
            if(!fs::exists(output/"report.json"))try{l::write(output/"report.json",{{"status","FAIL"},{"reason",error.what()},
                {"last_completed_tick",last_completed_tick},{"planning_diagnostics",p::planning_statistics}});}catch(...){}
        }
        std::cerr<<"Seasonal Agriculture acceptance FAIL: "<<error.what()<<'\n';return 1;
    }
}
