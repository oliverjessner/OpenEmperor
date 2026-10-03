#include "app/SandboxView.h"
#include "app/AutosaveController.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include "app/SandboxVisualOrder.h"
#include "app/RoadTopology.h"
#include "maps/TerrainRenderPlan.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace maps=openemperor::maps;
namespace simulation=openemperor::simulation;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void u16(Bytes& b,std::size_t at,std::uint16_t value) {
    b.at(at)=static_cast<std::uint8_t>(value);
    b.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& b,std::size_t at,std::uint32_t value) {
    u16(b,at,static_cast<std::uint16_t>(value));
    u16(b,at+2,static_cast<std::uint16_t>(value>>16U));
}
void write(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out),"synthetic write");
}
struct Temp {
    std::filesystem::path path=std::filesystem::canonical(std::filesystem::temp_directory_path())/
        ("openemperor-sandbox-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {
        std::filesystem::create_directories(path/"DATA");
        std::filesystem::create_directories(path/"Cities");
        std::ofstream out(path/"Cities/Synthetic.map",std::ios::binary);
        out<<"synthetic viewer map identity";
    }
    ~Temp() { std::error_code error; std::filesystem::remove_all(path,error);
        std::filesystem::remove(path.parent_path()/(path.filename().string()+"-viewer-save.json"),error); }
};
std::filesystem::path walker_fixture(const Temp& temp) {
    Bytes sg3(40680U+4U*72U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
    const std::string name="synthetic.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);
    u32(sg3,680+124,4);
    const Bytes red{2,0x00,0x7c,0x00,0x7c,2,0x00,0x7c,0x00,0x7c};
    const Bytes green{2,0xe0,0x03,0xe0,0x03,2,0xe0,0x03,0xe0,0x03};
    for (const auto [index,offset]:{std::pair{1U,4U},std::pair{3U,14U}}) {
        const auto at=40680U+index*72U;
        u32(sg3,at,offset);u32(sg3,at+4,10);
        u16(sg3,at+20,2);u16(sg3,at+22,2);u16(sg3,at+50,256);
    }
    write(temp.path/"DATA/walker.sg3",sg3);
    Bytes bitmap{0,0,0,0};bitmap.insert(bitmap.end(),red.begin(),red.end());
    bitmap.insert(bitmap.end(),green.begin(),green.end());
    write(temp.path/"DATA/walker.555",bitmap);
    const auto path=temp.path/"walker.json";
    std::ofstream out(path);
    out<<R"({"schema_version":1,"mode":"curated_walker_preview","role":"clay",
"ticks_per_frame":1,"evidence":"Synthetic software viewer frames",
"frames":[
{"alias":"red","archive":"DATA/walker.sg3","image_index":1,"foot_anchor":[1,1]},
{"alias":"green","archive":"DATA/walker.sg3","image_index":3,"foot_anchor":[1,1]}],
"clips":{"pos_x":["red","green"],"neg_x":["green","red"]},"idle":"red"})";
    check(static_cast<bool>(out),"walker manifest write");
    return path;
}
std::filesystem::path multi_role_walker_fixture(const Temp& temp) {
    Bytes sg3(40680U+6U*72U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,6);u32(sg3,16,6);u32(sg3,20,1);
    const std::string name="synthetic.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,680+124,6);
    Bytes bitmap{0,0,0,0};
    for (const auto [index,color]:{std::pair{1U,std::uint16_t{0x03ff}},
                                  std::pair{3U,std::uint16_t{0x7c1f}},
                                  std::pair{5U,std::uint16_t{0x03e0}}}) {
        const auto at=40680U+index*72U;
        u32(sg3,at,static_cast<std::uint32_t>(bitmap.size()));u32(sg3,at+4,10);
        u16(sg3,at+20,2);u16(sg3,at+22,2);u16(sg3,at+50,256);
        for (int row=0;row<2;++row) {
            bitmap.push_back(2);
            for (int column=0;column<2;++column) {
                bitmap.push_back(static_cast<std::uint8_t>(color));
                bitmap.push_back(static_cast<std::uint8_t>(color>>8U));
            }
        }
    }
    write(temp.path/"DATA/multi-walker.sg3",sg3);
    write(temp.path/"DATA/multi-walker.555",bitmap);
    nlohmann::json roles=nlohmann::json::object();
    for (const auto [role,index]:{std::pair{"clay",1},std::pair{"pottery",3},
                                 std::pair{"household",5}}) {
        roles[role]={{"ticks_per_frame",4},{"evidence","Synthetic role color"},
            {"frames",nlohmann::json::array({{{"alias","only"},
                {"archive","DATA/multi-walker.sg3"},{"image_index",index},
                {"foot_anchor",{1,1}}}})},
            {"clips",{{"pos_x",{"only"}},{"neg_x",{"only"}},
                {"pos_y",{"only"}},{"neg_y",{"only"}}}},
            {"idle","only"}};
    }
    const auto path=temp.path/"multi-walker.json";
    std::ofstream out(path);
    out<<nlohmann::json{{"schema_version",2},{"mode","curated_walker_preview"},
                         {"roles",roles}}.dump();
    check(bool(out),"multi-role manifest write");return path;
}
std::filesystem::path building_fixture(const Temp& temp) {
    Bytes sg3(40680U+4U*72U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
    const std::string name="synthetic.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);
    u32(sg3,680+124,4);
    const auto at=40680U+3U*72U;
    u32(sg3,at,4);u32(sg3,at+4,12800);u32(sg3,at+8,12800);
    u16(sg3,at+20,158);u16(sg3,at+22,90);u16(sg3,at+50,30);sg3[at+55]=2;
    for (const auto& [asset_name,color]:std::array<std::pair<const char*,std::uint16_t>,4>{{
            {"clay",0x03ff},{"pottery",0x7c1f},{"warehouse",0x7fe0},{"house",0x03e0}}}) {
        write(temp.path/(std::string("DATA/")+asset_name+".sg3"),sg3);
        Bytes bitmap(12804,0);
        for (std::size_t i=4;i<bitmap.size();i+=2) u16(bitmap,i,color);
        write(temp.path/(std::string("DATA/")+asset_name+".555"),bitmap);
    }
    const auto path=temp.path/"building.json";
    std::ofstream output(path);
    output<<R"({"schema_version":1,"mode":"curated_building_preview",
"buildings":{
"clay_source":{"archive":"DATA/clay.sg3","image_index":3,"ground_anchor":[79,70],"evidence":"Synthetic cyan"},
"pottery":{"archive":"DATA/pottery.sg3","image_index":3,"ground_anchor":[79,70],"evidence":"Synthetic magenta"},
"warehouse":{"archive":"DATA/warehouse.sg3","image_index":3,"ground_anchor":[79,70],"evidence":"Synthetic yellow"},
"household":{"archive":"DATA/house.sg3","image_index":3,"ground_anchor":[79,70],"evidence":"Synthetic green"}}})";
    check(static_cast<bool>(output),"building manifest write");
    return path;
}
std::filesystem::path fire_watch_fixture(const Temp& temp) {
    Bytes sg3(40680U+4U*72U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
    const std::string name="synthetic.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,680+124,4);
    const auto at=40680U+3U*72U;
    u32(sg3,at,4);u32(sg3,at+4,3200);u32(sg3,at+8,3200);
    u16(sg3,at+20,78);u16(sg3,at+22,104);u16(sg3,at+50,30);sg3[at+55]=1;
    write(temp.path/"DATA/watch.sg3",sg3);
    Bytes bitmap(3204,0);
    for (std::size_t i=4;i<bitmap.size();i+=2) u16(bitmap,i,0x7c00);
    write(temp.path/"DATA/watch.555",bitmap);
    const auto path=temp.path/"watch.json";
    std::ofstream out(path);
    out<<nlohmann::json{{"schema_version",1},{"mode","curated_building_preview"},
        {"buildings",{{"fire_watch",{{"archive","DATA/watch.sg3"},{"image_index",3},
        {"ground_anchor",{39,84}},{"evidence","Independently authored one-cell base"}}}}}}.dump();
    check(bool(out),"Watch fixture write");return path;
}
std::filesystem::path road_fixture(const Temp& temp) {
    Bytes sg3(40680U+17U*72U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,17);u32(sg3,16,17);u32(sg3,20,1);
    const std::string name="synthetic.bmp";
    std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,680+124,17);
    Bytes bitmap(4U+16U*3200U,0);
    for (int mask=0;mask<16;++mask) {
        const auto at=40680U+static_cast<unsigned>(mask+1)*72U;
        const auto offset=4U+static_cast<unsigned>(mask)*3200U;
        u32(sg3,at,offset);u32(sg3,at+4,3200);u32(sg3,at+8,3200);
        u16(sg3,at+20,78);u16(sg3,at+22,40);u16(sg3,at+50,30);sg3[at+55]=1;
        const auto color=static_cast<std::uint16_t>(((mask+1)<<10)|((mask+1)<<5));
        for (unsigned p=0;p<3200;p+=2) u16(bitmap,offset+p,color);
    }
    write(temp.path/"DATA/roads.sg3",sg3);write(temp.path/"DATA/roads.555",bitmap);
    nlohmann::json tiles=nlohmann::json::object();
    constexpr char hex[]="0123456789abcdef";
    for (int mask=0;mask<16;++mask)
        tiles[std::string("0x")+hex[mask]]={{"archive","DATA/roads.sg3"},
            {"image_index",mask+1},{"ground_anchor",{39,20}},
            {"evidence","Synthetic road viewer pixel"}};
    const auto path=temp.path/"roads.json";
    std::ofstream out(path);
    out<<nlohmann::json{{"schema_version",1},{"mode","curated_road_preview"},
                        {"tiles",tiles}}.dump();
    check(bool(out),"road fixture manifest write");return path;
}
openemperor::maps::StoredMapSession fixture(const Temp& temp,bool production=false,
                                           bool household=false,bool industry=false) {
    Bytes sg3(40680U+64U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));
    u32(sg3,4,213); u32(sg3,12,1); u32(sg3,16,1); u32(sg3,20,1);
    const std::string group_name="Zeus_system.bmp";
    std::copy(group_name.begin(),group_name.end(),sg3.begin()+680);
    u32(sg3,680+124,1); u32(sg3,680+128,1);
    u32(sg3,40680+4,3200); u32(sg3,40680+8,3200);
    u16(sg3,40680+20,78); u16(sg3,40680+22,40);
    u16(sg3,40680+50,30); sg3[40680+55]=1;
    Bytes bitmap(3200,0);
    for (std::size_t i=0;i<bitmap.size();i+=2) u16(bitmap,i,0x03e0);
    write(temp.path/"DATA/test.sg3",sg3);
    write(temp.path/"DATA/test.555",bitmap);
    maps::ParsedEmperorMap map;
    map.declared_map_size=84;
    maps::StoredGraphicsPlan plan;
    plan.data_root=temp.path;
    plan.profile=maps::StoredGraphicsProfile::Slot8;
    plan.border=maps::MapGeometry{84}.border;
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);
    plan.cell_by_storage.resize(228U*228U);
    openemperor::assets::AssetRecord record;
    record.id={"DATA/test.sg3",0};
    record.width=78; record.height=40; record.data_length=3200;
    record.uncompressed_length=3200; record.image_type=30;
    maps::StoredAsset asset;
    asset.record=record;
    plan.assets.push_back(std::move(asset));
    const std::uint32_t blocked=industry ? 121U : household ? 120U :
        production ? 117U : 116U;
    for (std::uint32_t y=industry ? 112U:114U;y<=(industry ? 116U:114U);++y)
    for (std::uint32_t x=110;x<=blocked;++x) {
        maps::StoredCell cell;
        cell.storage={x,y};
        cell.cell_index=static_cast<std::size_t>(y)*228+x;
        cell.terrain_raw=x==blocked ? 0 : 0x80;
        cell.objects_raw=0;
        cell.status=maps::StoredStatus::DecodePending;
        cell.asset_index=0;
        cell.footprint_index=plan.footprints.size();
        cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=maps::stored_image_origin(cell.world,78,40);
        const auto cell_index=plan.cells.size();
        plan.cell_by_storage[cell.cell_index]=cell_index;
        plan.status_by_storage[cell.cell_index]=cell.status;
        plan.cells.push_back(cell);
        maps::PlacedFootprint footprint;
        footprint.id=plan.footprints.size();
        footprint.asset_index=0;
        footprint.origin=cell.storage;
        footprint.cell_indices={cell_index};
        footprint.image_origin=cell.image_origin;
        footprint.status=maps::StoredStatus::DecodePending;
        plan.footprints.push_back(footprint);
    }
    return {std::move(map),std::move(plan)};
}
openemperor::maps::StoredMapSession city_v10_fixture(
    const Temp& temp,std::vector<std::uint8_t>& buildable) {
    auto session=fixture(temp);
    auto& plan=session.plan;
    plan.cells.clear();
    plan.footprints.clear();
    plan.cell_by_storage.assign(228U*228U,std::nullopt);
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);
    buildable.assign(228U*228U,0);
    const auto add=[&](std::uint32_t x,std::uint32_t y) {
        const auto storage_index=static_cast<std::size_t>(y)*228U+x;
        if (buildable.at(storage_index)) return;
        buildable[storage_index]=1;
        maps::StoredCell cell;
        cell.storage={x,y};
        cell.cell_index=storage_index;
        cell.terrain_raw=0x80;
        cell.objects_raw=0;
        cell.status=maps::StoredStatus::DecodePending;
        cell.asset_index=0;
        cell.footprint_index=plan.footprints.size();
        cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=maps::stored_image_origin(cell.world,78,40);
        const auto cell_index=plan.cells.size();
        plan.cell_by_storage[storage_index]=cell_index;
        plan.status_by_storage[storage_index]=cell.status;
        plan.cells.push_back(cell);
        maps::PlacedFootprint footprint;
        footprint.id=plan.footprints.size();
        footprint.asset_index=0;
        footprint.origin=cell.storage;
        footprint.cell_indices={cell_index};
        footprint.image_origin=cell.image_origin;
        footprint.status=maps::StoredStatus::DecodePending;
        plan.footprints.push_back(footprint);
    };
    for (std::uint32_t y=101;y<=112;++y)
        for (std::uint32_t x=100;x<=127;++x) add(x,y);
    return session;
}
simulation::World city_v10_world(const std::vector<std::uint8_t>& buildable) {
    simulation::World world(228,228,buildable,simulation::RulesProfile::CityV10);
    const auto put=[&](simulation::CommandType type,int x,int y) {
        const auto result=world.execute({type,{x,y}});
        if (!result.accepted)
            throw std::runtime_error(std::string("City-v10 UI fixture: ")+result.reason);
    };
    put(simulation::CommandType::PlaceClaySource,100,101);
    put(simulation::CommandType::PlacePottery,103,101);
    put(simulation::CommandType::PlaceWarehouse,100,104);
    put(simulation::CommandType::PlaceFarm,103,104);
    put(simulation::CommandType::PlaceServicePost,104,104);
    put(simulation::CommandType::PlaceHousehold,106,101);
    put(simulation::CommandType::PlaceHousehold,109,101);
    put(simulation::CommandType::PlaceHousehold,112,101);
    for (int x=100;x<=114;++x) put(simulation::CommandType::PlaceRoad,x,103);
    for (int ticks=0;ticks<60000 && world.treasury()<4000;++ticks) world.tick();
    check(world.treasury()>=4000,"City-v10 UI fixture did not earn expansion funds");
    for (int x=115;x<=125;++x) put(simulation::CommandType::PlaceRoad,x,103);
    for (int x=100;x<=125;++x) put(simulation::CommandType::PlaceRoad,x,110);
    put(simulation::CommandType::PlaceClaySource,106,104);
    put(simulation::CommandType::PlacePottery,109,104);
    for (const auto cell:{simulation::Cell{115,101},simulation::Cell{118,101},
                          simulation::Cell{112,104},simulation::Cell{115,104},
                          simulation::Cell{118,104},simulation::Cell{121,104},
                          simulation::Cell{124,104}})
        put(simulation::CommandType::PlaceHousehold,cell.x,cell.y);
    put(simulation::CommandType::PlaceWarehouse,100,111);
    put(simulation::CommandType::PlaceFarm,103,111);
    put(simulation::CommandType::PlaceServicePost,104,111);
    put(simulation::CommandType::PlaceClaySource,100,108);
    put(simulation::CommandType::PlacePottery,103,108);
    put(simulation::CommandType::PlaceClaySource,106,111);
    put(simulation::CommandType::PlacePottery,109,111);
    for (const auto cell:{simulation::Cell{106,108},simulation::Cell{109,108},
                          simulation::Cell{112,108},simulation::Cell{115,108},
                          simulation::Cell{118,108},simulation::Cell{112,111},
                          simulation::Cell{115,111},simulation::Cell{118,111},
                          simulation::Cell{121,111},simulation::Cell{124,111}})
        put(simulation::CommandType::PlaceHousehold,cell.x,cell.y);
    check(world.buildings().size()==34 && world.couriers().size()==14,
          "City-v10 UI fixture did not reach full entity counts");
    return world;
}
std::array<std::uint8_t,4> pixel(SDL_Renderer* renderer,int x,int y) {
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
    check(surface!=nullptr,"read software frame");
    std::array<std::uint8_t,4> result{};
    const bool okay=SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]);
    SDL_DestroySurface(surface);
    check(okay,"read pixel");
    return result;
}
SDL_Event click(float x,float y) {
    SDL_Event event{};
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;
    event.button.x=x;
    event.button.y=y;
    return event;
}
SDL_Event release(float x,float y) {
    auto event=click(x,y);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    return event;
}
SDL_Event motion(float x,float y) {
    SDL_Event event{};
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=x; event.motion.y=y;
    return event;
}
void mouse_click(openemperor::SandboxView& view,float x,float y,bool& running) {
    view.handle_event(click(x,y),running);
    view.handle_event(release(x,y),running);
}
SDL_Event key(SDL_Keycode code) {
    SDL_Event event{};
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.key=code;
    return event;
}
void household_stage_checks(const Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    namespace perf=openemperor::performance;
    namespace persistence=openemperor::persistence;
    using Role=openemperor::assets::BuildingVisualRole;
    // Three independently authored RGB555 diamonds; no original game bytes.
    Bytes sg3(40680U+4U*72U,0),bitmap(4U+3U*12800U,0);
    u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,214);
    u32(sg3,12,4);u32(sg3,16,4);u32(sg3,20,1);
    const std::string group="synthetic-housing.bmp";
    std::copy(group.begin(),group.end(),sg3.begin()+680);u32(sg3,680+124,4);
    const std::array<std::uint16_t,3> colors{0x7c00,0x03e0,0x001f};
    nlohmann::json entries=nlohmann::json::object();
    for (std::size_t stage=0;stage<3;++stage) {
        const auto at=40680U+(stage+1U)*72U,offset=4U+stage*12800U;
        u32(sg3,at,static_cast<std::uint32_t>(offset));u32(sg3,at+4,12800);u32(sg3,at+8,12800);
        u16(sg3,at+20,158);u16(sg3,at+22,90);u16(sg3,at+50,30);sg3[at+55]=2;
        for (std::size_t p=offset;p<offset+12800U;p+=2) u16(bitmap,p,colors[stage]);
        entries["household_level_"+std::to_string(stage)]={{"archive","DATA/house-stages.sg3"},
            {"image_index",stage+1},{"ground_anchor",{79,70}},
            {"evidence","Independent synthetic 2x2 stage color"}};
    }
    entries["household"]=entries["household_level_0"];
    write(temp.path/"DATA/house-stages.sg3",sg3);write(temp.path/"DATA/house-stages.555",bitmap);
    const auto manifest=temp.path/"house-stages.json";
    const auto save_profile=[&](const nlohmann::json& roles) {
        std::ofstream out(manifest);
        out<<nlohmann::json{{"schema_version",1},{"mode","curated_building_preview"},{"buildings",roles}};
        check(bool(out),"synthetic stage manifest");
    };
    save_profile(entries);
    Temp app_temp; // Separate temporary preference/save root outside the original-data root.
    const auto manual=app_temp.path/"manual.json";
    std::vector<std::uint8_t> mask;
    auto session=city_v10_fixture(temp,mask);const auto border=session.plan.border;
    openemperor::SandboxView view(std::move(session),false,simulation::RulesProfile::CityV13);
    view.configure_save(temp.path,"Cities/Synthetic.map",manual);
    view.set_building_visuals(manifest);
    perf::set_enabled(true);perf::reset();
    view.initialize(window,renderer);
    // The background has one additional synthetic image/texture.
    check(view.building_texture_count()==3 && view.building_display_stats().decoded_assets==3 &&
          perf::counter(perf::Counter::AssetDecodes)==4 &&
          perf::counter(perf::Counter::TextureUploads)==4,"stage assets were not eagerly deduplicated");
    perf::set_enabled(false);
    const auto put=[&](simulation::CommandType type,simulation::Cell cell) {
        const auto result=view.execute({type,cell});
        if (!result.accepted || !result.changed) throw std::runtime_error("stage fixture: "+result.reason);
        return *view.world().building_owner_at(cell);
    };
    put(simulation::CommandType::PlaceClaySource,{100,101});
    put(simulation::CommandType::PlacePottery,{100,104});
    put(simulation::CommandType::PlaceWarehouse,{103,101});
    put(simulation::CommandType::PlaceFarm,{104,104});
    put(simulation::CommandType::PlaceMarket,{103,104});
    put(simulation::CommandType::PlaceServicePost,{105,104});
    for (int x=100;x<=114;++x)
        check(view.execute({simulation::CommandType::PlaceRoad,{x,103}}).accepted,"starter road");
    for (const auto cell:{simulation::Cell{106,101},simulation::Cell{106,104},
                          simulation::Cell{109,101},simulation::Cell{109,104}})
        put(simulation::CommandType::PlaceHousehold,cell);
    const auto watch=put(simulation::CommandType::PlaceFireWatch,{114,104});
    const auto house=*view.world().building_owner_at({109,104});
    check(view.world().ticks()==0 && view.world().treasury()==20 &&
          view.world().household_desirability(house)==10,"paid stage starter changed");
    const auto ground=view.camera().world_to_screen(maps::terrain_ground({110,105},border));
    const auto pixels=[&]{return pixel(renderer,static_cast<int>(ground.x),static_cast<int>(ground.y));};
    const std::array<std::array<std::uint8_t,4>,3> rgba{{{255,0,0,255},{0,255,0,255},{0,0,255,255}}};
    bool running=true;
    const auto check_stage=[&](int expected,bool overlay=false) {
        const auto before=view.world().snapshot();const auto refresh=view.world().route_refresh_count();
        perf::set_enabled(true);perf::reset();
        check(view.world().household_level(house)==expected && view.render(),"effective stage render");
        const auto color=pixels();
        check(overlay ? color!=rgba.at(static_cast<std::size_t>(expected)):
                        color==rgba.at(static_cast<std::size_t>(expected)),"wrong stage/overlay pixels");
        if (overlay) check(color.at(static_cast<std::size_t>(expected))>150,
                          "desirability overlay hid the stage instead of blending");
        for (const auto counter:{perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
            perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::WorldCopies,
            perf::Counter::WorldRestores,perf::Counter::WorldExecutes,perf::Counter::SimulationTicks,
            perf::Counter::FileReads,perf::Counter::FileWrites})
            check(perf::counter(counter)==0,"stage selection/render did IO, routes or mutation");
        check(view.world().snapshot()==before && view.world().route_refresh_count()==refresh &&
              view.building_texture_count()==3,"stage render changed World or textures");
        perf::set_enabled(false);
    };
    check_stage(0);view.save_now();const auto level0=view.capture_save_document();
    openemperor::AutosaveController recovery(app_temp.path,temp.path,true);
    check(recovery.begin(level0,mask).kind==openemperor::AutosaveResult::Kind::Saved,"stage recovery start");
    for (int i=0;i<1200;++i) view.tick_once();
    check_stage(1);const auto level1=view.capture_save_document();
    check(recovery.poll(level1,mask,true).kind==openemperor::AutosaveResult::Kind::Saved,"L1 checkpoint");
    for (int i=0;i<1200;++i) view.tick_once();
    check_stage(2);const auto level2=view.capture_save_document();
    check(recovery.poll(level2,mask,true).kind==openemperor::AutosaveResult::Kind::Saved,"L2 checkpoint");
    check(view.save_path()==manual && view.save_generation()==1 &&
          persistence::read_save(manual).world==level0.world,"autosave altered manual state");
    const auto catalog=recovery.store().catalog();
    check(catalog.histories.size()==1 && catalog.histories[0].entries.size()==3,"stage recovery history");
    for (const auto& entry:catalog.histories[0].entries) {
        const auto document=persistence::read_save(entry.path);
        const auto restored=persistence::restore_save(document,temp.path,mask);
        check(document.source_schema_version==15 && restored.household_level(house)==
              (entry.tick==0 ? 0:entry.tick==1200 ? 1:2),"recovery reconstructed wrong effective stage");
        persistence::write_save(manual,document,temp.path,mask);view.load_now();
        check_stage(restored.household_level(house));
    }
    persistence::write_save(manual,level2,temp.path,mask);view.load_now();check_stage(2);
    // Earn every later construction cost through actual production/demand/tax.
    while (view.world().ticks()<60000 && view.world().treasury()<8100) view.tick_once();
    check(view.world().treasury()>=8100 && view.world().household_level(house)==2,
          "stage fixture did not earn construction funds");
    const auto stable_house=view.world().building(house);
    for (int repeat=0;repeat<25;++repeat) {
        const auto pottery=put(simulation::CommandType::PlacePottery,{112,104});
        check_stage(1);
        const auto clay=put(simulation::CommandType::PlaceClaySource,{112,101});
        check_stage(0);
        check(view.execute(simulation::demolish_building(clay)).accepted,"empty Clay removal");
        check_stage(1);
        check(view.execute(simulation::demolish_building(pottery)).accepted,"empty Pottery removal");
        check_stage(2);
        check(view.world().building(house)==stable_house,"geometry-driven stages recreated/changed House");
        for (const auto cell:simulation::building_footprint_cells(view.world().profile(),view.world().rule_version(),
            simulation::Object::Household,stable_house.cell)) {
            const auto p=view.camera().world_to_screen(maps::terrain_ground(
                {static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},border));
            view.set_tool(5);mouse_click(view,static_cast<float>(p.x),static_cast<float>(p.y),running);
            check(view.selected_building()==house,"stage changed footprint picking");
        }
        view.handle_event(key(SDLK_F4),running);const auto before=view.world().snapshot();
        check(view.render() && view.world().snapshot()==before,"F4 mutated stage authority");
        view.handle_event(key(SDLK_F4),running);
    }
    // Overlay blends on all reachable stages; poor/neutral/good caps follow the World.
    view.handle_event(key(SDLK_D),running);check_stage(2,true);
    const auto pottery=put(simulation::CommandType::PlacePottery,{112,104});check_stage(1,true);
    const auto clay=put(simulation::CommandType::PlaceClaySource,{112,101});check_stage(0,true);
    check(view.execute(simulation::demolish_building(clay)).accepted &&
          view.execute(simulation::demolish_building(pottery)).accepted,"overlay fixture removal");
    view.handle_event(key(SDLK_D),running);
    view.save_now();const auto saved=view.world().snapshot();
    put(simulation::CommandType::PlacePottery,{112,104});check_stage(1);
    view.load_now();check(view.world().snapshot()==saved && view.paused(),"stage exact manual reload");
    check_stage(2);
    check(view.execute(simulation::set_building_operation(watch,false)).accepted,"pause Watch");
    for (int ticks=0;ticks<10000 && !view.world().building_on_fire(house);++ticks) view.tick_once();
    check(view.world().building_on_fire(house),"House fire visual fixture");
    check_stage(2);
    check(!view.world().demolition_status(house).allowed,"stage bypassed burning demolition safety");
    view.load_now();check_stage(2);
    save_profile({{"household",entries["household"]}});view.set_building_visuals(manifest);
    check(view.render() && pixels()==rgba[0],"legacy-only custom profile did not supply L2");
    save_profile({{"household",entries["household"]},{"household_level_0",entries["household_level_0"]}});
    view.set_building_visuals(manifest);
    check(view.render() && pixels()==rgba[0],"partial profile did not prefer legacy L2 fallback");
    save_profile({{"household_level_0",entries["household_level_0"]}});view.set_building_visuals(manifest);
    check(view.render() && pixels()!=rgba[2] && view.building_display_stats().placeholder_fallbacks[
        openemperor::assets::role_index(Role::Household)]>0,"missing stage did not retain diagnostic fallback");
    save_profile(entries);view.set_building_visuals(manifest);check_stage(2);
    auto broken=entries;broken["household_level_2"]["image_index"]=99;save_profile(broken);
    bool rejected=false;
    try { view.set_building_visuals(manifest); } catch (const std::exception&) { rejected=true; }
    check(rejected,"bad staged profile replacement accepted");check_stage(2);save_profile(entries);
    view.shutdown();
    // Earlier profiles use only the legacy image, even with stage keys configured.
    std::vector<std::uint8_t> old_mask;
    openemperor::SandboxView old(city_v10_fixture(temp,old_mask),false,simulation::RulesProfile::CityV12);
    old.configure_save(temp.path,"Cities/Synthetic.map",{});old.set_building_visuals(manifest);
    old.initialize(window,renderer);
    check(old.execute({simulation::CommandType::PlaceHousehold,{109,104}}).accepted,"legacy House");
    check(old.render() && pixel(renderer,static_cast<int>(ground.x),static_cast<int>(ground.y))==rgba[0] &&
          old.building_display_stats().drawn_instances[openemperor::assets::role_index(Role::Household)]==1 &&
          old.building_display_stats().drawn_instances[openemperor::assets::role_index(Role::HouseholdLevel0)]==0,
          "City-v12 unexpectedly adopted staged visuals");
    old.shutdown();
    std::cout<<"House stages: actual 0/1/2 supply, 100 command-driven reversals, pixels, picking, overlay, fire, save/recovery, legacy\n";

}
void city_v14_water_checks(const Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    namespace perf=openemperor::performance;
    namespace persistence=openemperor::persistence;
    Bytes stage_archive(40680U+4U*72U,0),stage_bitmap(38404U,0);
    u32(stage_archive,0,static_cast<std::uint32_t>(stage_archive.size()));u32(stage_archive,4,214);
    u32(stage_archive,12,4);u32(stage_archive,16,4);u32(stage_archive,20,1);
    const std::string group="synthetic-water-stages.bmp";
    std::copy(group.begin(),group.end(),stage_archive.begin()+680);u32(stage_archive,804,4);
    const std::array<std::uint16_t,3> colors{0x7c00,0x03e0,0x001f};
    for (std::size_t n=0;n<3;++n) {
        const auto at=40680U+(n+1U)*72U,offset=4U+n*12800U;
        u32(stage_archive,at,static_cast<std::uint32_t>(offset));u32(stage_archive,at+4,12800);u32(stage_archive,at+8,12800);
        u16(stage_archive,at+20,158);u16(stage_archive,at+22,90);u16(stage_archive,at+50,30);stage_archive[at+55]=2;
        for (std::size_t q=offset;q<offset+12800U;q+=2) u16(stage_bitmap,q,colors[n]);
    }
    write(temp.path/"DATA/house-stages.sg3",stage_archive);write(temp.path/"DATA/house-stages.555",stage_bitmap);
    nlohmann::json entries=nlohmann::json::object();
    for (unsigned stage=0;stage<3;++stage)
        entries["household_level_"+std::to_string(stage)]={{"archive","DATA/house-stages.sg3"},
            {"image_index",stage+1},{"ground_anchor",{79,70}},{"evidence","Independent synthetic stage"}};
    const auto manifest=temp.path/"water-stages.json";
    {std::ofstream out(manifest);out<<nlohmann::json{{"schema_version",1},{"mode","curated_building_preview"},{"buildings",entries}};}
    std::vector<std::uint8_t> mask;
    auto session=city_v10_fixture(temp,mask);const auto border=session.plan.border;
    openemperor::SandboxView view(std::move(session),false,simulation::RulesProfile::CityV14);
    Temp outside;view.configure_save(temp.path,"Cities/Synthetic.map",outside.path/"water-save.json");
    view.set_building_visuals(manifest);view.initialize(window,renderer);
    const auto put=[&](simulation::CommandType type,simulation::Cell cell) {
        const auto result=view.execute({type,cell});check(result.accepted && result.changed,"water view placement");
        return *view.world().building_owner_at(cell);
    };
    put(simulation::CommandType::PlaceClaySource,{100,101});put(simulation::CommandType::PlacePottery,{100,104});
    put(simulation::CommandType::PlaceWarehouse,{103,101});put(simulation::CommandType::PlaceFarm,{104,104});
    put(simulation::CommandType::PlaceMarket,{103,104});put(simulation::CommandType::PlaceServicePost,{105,104});
    for (int x=100;x<=114;++x) check(view.execute({simulation::CommandType::PlaceRoad,{x,103}}).accepted,"water starter road");
    for (const auto cell:{simulation::Cell{106,101},simulation::Cell{106,104},simulation::Cell{109,101},simulation::Cell{109,104}})
        put(simulation::CommandType::PlaceHousehold,cell);
    put(simulation::CommandType::PlaceFireWatch,{114,104});
    const auto house=*view.world().building_owner_at({109,104});
    check(view.world().treasury()==20 && view.world().water_covered_households()==0,"water starter cost");
    for (int i=0;i<2400;++i) view.tick_once();
    check(view.world().historical_household_level(house)==2 && view.world().household_level(house)==0,"dry historic stage");
    const auto ground=view.camera().world_to_screen(maps::terrain_ground({110,105},border));
    const auto screen=[&](simulation::Cell cell) {return view.camera().world_to_screen(maps::terrain_ground(
        {static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},border));};
    bool running=true;
    const auto house_screen=screen({110,105});mouse_click(view,static_cast<float>(house_screen.x),static_cast<float>(house_screen.y),running);
    const auto contains=[&](const char* text) {const auto lines=view.inspection_lines();return std::any_of(lines.begin(),lines.end(),
        [&](const auto& line) {return line.find(text)!=std::string::npos;});};
    check(contains("No water access") && contains("Water cap: 0") && contains("Historical development:"),"dry inspector caps");
    const auto check_stage=[&](unsigned expected) {
        const auto before=view.world().snapshot();perf::set_enabled(true);perf::reset();
        check(view.render(),"water stage rendering");
        const auto color=pixel(renderer,static_cast<int>(ground.x),static_cast<int>(ground.y));
        check(color[expected]==255 && view.world().household_level(house)==static_cast<int>(expected),"water stage pixel");
        for (const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
            perf::Counter::AssetDecodes,perf::Counter::TextureUploads,perf::Counter::BfsCalls,
            perf::Counter::RouteRefreshes,perf::Counter::FileReads,perf::Counter::FileWrites})
            check(perf::counter(c)==0,"water stage reload/mutation");
        check(view.world().snapshot()==before && view.building_texture_count()==3,"water stage authority changed");
        perf::set_enabled(false);
    };
    check_stage(0);
    view.handle_event(key(SDLK_I),running);check(view.tool()==12,"I Well shortcut");
    const auto position=screen({108,106});
    view.handle_event(motion(static_cast<float>(position.x),static_cast<float>(position.y)),running);
    view.update(0);
    check(view.predicted_well_coverage() && view.predicted_well_coverage()->households==4 &&
        view.predicted_well_coverage()->currently_dry==4,"Well preview counts");
    const auto count=view.water_preview_build_count();const auto before=view.world().snapshot();
    perf::set_enabled(true);perf::reset();
    for (int n=0;n<200;++n) view.handle_event(motion(static_cast<float>(position.x)+0.001F*static_cast<float>(n%10),
        static_cast<float>(position.y)),running);
    view.update(0);
    check(view.water_preview_build_count()==count && view.world().snapshot()==before,"200 motions repeated water projection");
    for (const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads})
        check(perf::counter(c)==0,"water preview expensive work");
    perf::set_enabled(false);
    const auto well=put(simulation::CommandType::PlaceWell,{108,106});
    view.handle_event(motion(static_cast<float>(position.x),static_cast<float>(position.y)),running);
    view.update(0);
    check(view.predicted_well_coverage() && view.predicted_well_coverage()->currently_dry==0 &&
        view.water_preview_build_count()==count+1,"building revision did not refresh preview");
    view.handle_event(key(SDLK_5),running);check_stage(2);
    mouse_click(view,static_cast<float>(position.x),static_cast<float>(position.y),running);
    check(contains("Well #") && contains("Covered Houses 4") && !contains("Worker") && !contains("Operation") &&
        !contains("Priority") && !contains("Courier"),"Well inspector acquired operation UI");
    const auto stable=view.world().snapshot();perf::set_enabled(true);perf::reset();
    for (int n=0;n<100;++n) {view.handle_event(key(SDLK_U),running);check(view.render(),"water overlay render");}
    check(!view.water_overlay() && view.world().snapshot()==stable,"100 Water toggles mutated World");
    for (const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites}) check(perf::counter(c)==0,"water overlay did work");
    perf::set_enabled(false);
    view.handle_event(key(SDLK_7),running);
    const auto future=screen({112,104});view.handle_event(motion(static_cast<float>(future.x),static_cast<float>(future.y)),running);
    view.update(0);
    check(view.predicted_water()==true && view.predicted_desirability().has_value(),"House preview missing water/quality");
    view.save_now();check(openemperor::persistence::read_save(view.save_path()).source_schema_version==16,"view save schema16");
    check(view.execute(simulation::demolish_building(well)).accepted,"view Well removal");check_stage(0);
    view.load_now();check(view.paused() && view.world().snapshot()==stable,"water load did not restore exact paused World");check_stage(2);
    const auto stable_document=persistence::read_save(view.save_path());
    check(view.execute(simulation::demolish_building(well)).accepted,"branch Well removal");
    put(simulation::CommandType::PlaceWell,{120,112});view.save_now();
    const auto far_document=persistence::read_save(view.save_path());
    persistence::write_save(view.save_path(),stable_document,temp.path,mask);view.load_now();
    check(view.execute(simulation::demolish_building(well)).accepted,"central branch removal");
    put(simulation::CommandType::PlaceWell,{108,106});
    check(view.world().road_revision()==far_document.world.road_revision,"branch revisions must match");
    view.handle_event(key(SDLK_I),running);
    view.handle_event(motion(static_cast<float>(position.x),static_cast<float>(position.y)),running);view.update(0);
    check(view.predicted_well_coverage()->currently_dry==0,"central branch water");
    const auto branch_count=view.water_preview_build_count();
    persistence::write_save(view.save_path(),far_document,temp.path,mask);view.load_now();view.update(0);
    check(view.predicted_well_coverage() && view.predicted_well_coverage()->currently_dry==4 &&
        view.water_preview_build_count()==branch_count+1,"same-revision load retained stale water preview");
    const auto close_well=put(simulation::CommandType::PlaceWell,{108,106});
    check(view.world().building(close_well).kind==simulation::Object::Well && view.paused(),
        "Well presentation setup");
    // Anchored zoom pairs translate the camera through ordinary view events.
    // This exercises real offset changes without relying on OS keyboard state.
    const auto wheel=[&](openemperor::scene::Point at,double factor) {
        SDL_Event event{};event.type=SDL_EVENT_MOUSE_WHEEL;
        event.wheel.mouse_x=static_cast<float>(at.x);event.wheel.mouse_y=static_cast<float>(at.y);
        event.wheel.y=static_cast<float>(std::log(factor)/std::log(1.15));view.handle_event(event,running);
    };
    const auto focus=[&](int zoom) {
        const auto map=view.layout().map;
        const openemperor::scene::Point center{map.x+map.w*.5,map.y+map.h*.5};
        wheel(center,zoom/view.camera().zoom);
        for (int i=0;i<20;++i) {
            const auto p=screen({108,106});
            const openemperor::scene::Point delta{std::clamp(center.x-p.x,-200.0,200.0),
                std::clamp(center.y-p.y,-100.0,100.0)};
            if (std::abs(delta.x)+std::abs(delta.y)<.01) break;
            wheel({center.x+delta.x*.5,center.y+delta.y*.5},.5);
            wheel({center.x-delta.x*.5,center.y-delta.y*.5},2);
        }
    };
    // Prove the integration draws the same geometry for a valid, unplaced Well.
    // The sample lies inside the dark water and away from the footprint outline.
    const simulation::Cell ghost_cell{108,108};
    check(view.world().validate({simulation::CommandType::PlaceWell,ghost_cell}).accepted,
        "Well preview fixture not buildable");
    focus(2);view.set_tool(5);check(view.render(),"bare preview terrain");
    const auto ghost_ground=screen(ghost_cell);
    const int ghost_x=static_cast<int>(ghost_ground.x-4),ghost_y=static_cast<int>(ghost_ground.y-12);
    const auto bare=pixel(renderer,ghost_x,ghost_y);
    view.set_tool(12);view.handle_event(motion(static_cast<float>(ghost_ground.x),
        static_cast<float>(ghost_ground.y)),running);view.update(0);
    check(view.render() && pixel(renderer,ghost_x,ghost_y)!=bare,"Well fallback placement preview is missing");
    const auto presentation_snapshot=view.world().snapshot();
    const auto texture_count=view.building_texture_count();
    const auto start_camera=view.camera();
    perf::set_enabled(true);perf::reset();
    for (int frame=0;frame<100;++frame) {
        focus(frame%3==0 ? 1:frame%3==1 ? 2:4);
        const auto map=view.layout().map;
        const openemperor::scene::Point center{map.x+map.w*.5,map.y+map.h*.5};
        wheel({center.x+4,center.y+2},.5);wheel({center.x-4,center.y-2},2);
        view.set_tool(5);const auto selected=screen({108,106});
        mouse_click(view,static_cast<float>(selected.x),static_cast<float>(selected.y),running);
        check(view.selected_building()==close_well,"Well selection during camera changes");
        view.handle_event(key(SDLK_U),running);
        if (frame%2) {
            view.set_tool(12);const auto ghost=screen(ghost_cell);
            view.handle_event(motion(static_cast<float>(ghost.x),static_cast<float>(ghost.y)),running);
        }
        view.update(.016);check(view.render(),"Well camera/zoom/overlay/selection/preview frame");
    }
    check(view.world().snapshot()==presentation_snapshot && view.building_texture_count()==texture_count &&
        !view.water_overlay(),"100 Well presentation frames changed authority/assets");
    check(view.camera().offset.x!=start_camera.offset.x || view.camera().offset.y!=start_camera.offset.y,
        "presentation stress did not move camera");
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,
        perf::Counter::TextureUploads,perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::SimulationTicks})
        check(perf::counter(counter)==0,"Well presentation did expensive work");
    perf::set_enabled(false);
    view.shutdown();std::cout<<"City-v14 view: stages,100 pure Water toggles,200 coalesced motions,inspector,save/load; "
        "100 pure Well frames with actual camera motion,1x/2x/4x,selection,overlay and visible preview\n";
}
}
void city_v15_health_checks(const Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    namespace perf=openemperor::performance;
    std::vector<std::uint8_t> mask;
    auto session=city_v10_fixture(temp,mask);const auto border=session.plan.border;
    openemperor::SandboxView view(std::move(session),false,simulation::RulesProfile::CityV15);
    Temp outside;view.configure_save(temp.path,"Cities/Synthetic.map",outside.path/"health-save.json");
    view.initialize(window,renderer);bool running=true;
    const auto put=[&](simulation::CommandType type,simulation::Cell cell) {
        const auto r=view.execute({type,cell});check(r.accepted && r.changed,"health UI paid placement");
        return *view.world().building_owner_at(cell);
    };
    put(simulation::CommandType::PlaceClaySource,{100,101});put(simulation::CommandType::PlacePottery,{100,104});
    put(simulation::CommandType::PlaceWarehouse,{103,101});put(simulation::CommandType::PlaceFarm,{104,104});
    put(simulation::CommandType::PlaceMarket,{103,104});put(simulation::CommandType::PlaceServicePost,{105,104});
    for(int x=100;x<=114;++x)check(view.execute({simulation::CommandType::PlaceRoad,{x,103}}).accepted,"health UI road");
    for(auto cell:{simulation::Cell{106,101},simulation::Cell{106,104},simulation::Cell{109,101},simulation::Cell{109,104}})
        put(simulation::CommandType::PlaceHousehold,cell);
    put(simulation::CommandType::PlaceFireWatch,{114,104});
    for(int i=0;i<3400;++i)view.tick_once();
    const auto home=*view.world().building_owner_at({109,104});
    check(view.world().household_sick(home),"live illness UI fixture");
    const auto screen=[&](simulation::Cell cell) {return view.camera().world_to_screen(maps::terrain_ground(
        {static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},border));};
    view.set_tool(5);auto pos=screen({109,104});mouse_click(view,static_cast<float>(pos.x),static_cast<float>(pos.y),running);
    const auto contains=[&](const char* text){const auto lines=view.inspection_lines();return std::ranges::any_of(lines,
        [&](const auto& line){return line.find(text)!=std::string::npos;});};
    check(contains("Health: Sick") && contains("Health risk: 0 / 100") && contains("Illness remaining:") &&
        contains("No water access"),"sick House inspector");
    const auto sick=view.world().snapshot();const auto level=view.world().household_level(home);
    perf::set_enabled(true);perf::reset();
    for(int i=0;i<100;++i){view.handle_event(key(SDLK_K),running);check(view.render(),"health overlay render");}
    check(!view.health_overlay() && view.world().snapshot()==sick && view.world().household_level(home)==level,"100 health toggles mutated World/stage");
    for(const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites})check(perf::counter(c)==0,"health overlay did expensive work");
    perf::set_enabled(false);
    view.save_now();check(openemperor::persistence::read_save(view.save_path()).source_schema_version==17,"view schema17");
    view.tick_once();view.load_now();check(view.paused() && view.world().snapshot()==sick &&
        view.world().household_sick(home),"live sick recovery/load auto-cure");
    view.handle_event(key(SDLK_J),running);check(view.tool()==13,"J HealthPost shortcut");
    pos=screen({112,104});view.handle_event(motion(static_cast<float>(pos.x),static_cast<float>(pos.y)),running);
    perf::set_enabled(true);perf::reset();
    for(int i=0;i<200;++i)view.handle_event(motion(static_cast<float>(pos.x)+0.001F*static_cast<float>(i%10),static_cast<float>(pos.y)),running);
    view.update(0);check(view.render(),"health fallback preview");
    for(const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites})check(perf::counter(c)==0,"health preview did expensive work");
    check(view.world().snapshot()==sick,"Health preview changed World");perf::set_enabled(false);
    const auto post=put(simulation::CommandType::PlaceHealthPost,{112,104});
    view.execute(simulation::set_building_workforce_priority(post,simulation::WorkforcePriority::High));
    view.set_tool(5);mouse_click(view,static_cast<float>(pos.x),static_cast<float>(pos.y),running);
    check(contains("Health Post #") && contains("Workers assigned") && contains("Priority: High") &&
        contains("Worker phase:") && contains("Route status:") && contains("City-wide") && !contains("stock"),"Health inspector diagnostics");
    const auto stable=view.world().snapshot();
    view.handle_event(key(SDLK_H),running);check(view.tool()==5 && view.render(),"H lost Help binding");view.handle_event(key(SDLK_H),running);
    view.handle_event(key(SDLK_1),running);auto start=screen({116,103});auto end=screen({120,103});
    view.handle_event(click(static_cast<float>(start.x),static_cast<float>(start.y)),running);
    perf::set_enabled(true);perf::reset();
    for(int i=0;i<100;++i)view.handle_event(motion(static_cast<float>(end.x),static_cast<float>(end.y)),running);
    view.update(0);check(view.render(),"health city road preview");
    for(const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,
        perf::Counter::AssetDecodes,perf::Counter::TextureUploads,perf::Counter::FileReads,perf::Counter::FileWrites})
        check(perf::counter(c)==0,"Health couriers regressed road preview");
    check(view.world().snapshot()==stable,"road preview changed health city");perf::set_enabled(false);
    view.handle_event(key(SDLK_ESCAPE),running);
    // Exercise the overlay at its full House bound, using genuine earned funds.
    put(simulation::CommandType::PlaceWell,{108,106});
    for(int i=0;i<20000 && view.world().treasury()<16*80;++i)view.tick_once();
    check(view.world().treasury()>=16*80,"Health UI expansion did not earn funds");
    for(int y:{107,110})for(int x=100;x<=121;x+=3)
        put(simulation::CommandType::PlaceHousehold,{x,y});
    check(std::ranges::count_if(view.world().buildings(),[](const auto& b){
        return b.kind==simulation::Object::Household;})==20,"Health overlay max House fixture");
    const auto maximum=view.world().snapshot();perf::set_enabled(true);perf::reset();
    for(int i=0;i<100;++i){view.handle_event(key(SDLK_K),running);check(view.render(),"maximum Health overlay render");}
    check(!view.health_overlay() && view.world().snapshot()==maximum,"maximum Health overlay changed World");
    for(const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::SimulationTicks})
        check(perf::counter(c)==0,"maximum Health overlay did expensive work");
    perf::set_enabled(false);
    view.shutdown();std::cout<<"City-v15 UI: sick/save/load paused, inspectors, J/K/H, 100 overlay toggles at 4 and 20 Houses, 200 hover events, road preview pure\n";
}

void city_v16_maintenance_checks(const Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    namespace perf=openemperor::performance;
    std::vector<std::uint8_t> mask;auto session=city_v10_fixture(temp,mask);const auto border=session.plan.border;
    openemperor::SandboxView view(std::move(session),false,simulation::RulesProfile::CityV16);
    Temp outside;view.configure_save(temp.path,"Cities/Synthetic.map",outside.path/"budget-save.json");
    view.initialize(window,renderer);bool running=true;
    const auto put=[&](simulation::CommandType type,simulation::Cell cell) {
        const auto result=view.execute({type,cell});check(result.accepted && result.changed,"budget UI paid building");
        return *view.world().building_owner_at(cell);
    };
    view.set_tool(3);check(view.tool()==3,"funded Pottery preview disabled");
    const auto cost_before=view.world().snapshot();perf::set_enabled(true);perf::reset();
    const auto cost_point=view.camera().world_to_screen(maps::terrain_ground({115,108},border));
    for(int i=0;i<200;++i)view.handle_event(motion(static_cast<float>(cost_point.x),static_cast<float>(cost_point.y)),running);
    view.update(0);check(view.render() && view.world().snapshot()==cost_before,"paid cost/upkeep preview mutation");
    for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::BfsCalls,
        perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites})check(perf::counter(counter)==0,"cost/upkeep preview did work");
    perf::set_enabled(false);
    for(int x:{100,103,106,109})put(simulation::CommandType::PlacePottery,{x,104});
    for(int x:{100,103,106})put(simulation::CommandType::PlaceClaySource,{x,101});
    const auto post=put(simulation::CommandType::PlaceHealthPost,{112,104});
    put(simulation::CommandType::PlaceWell,{120,104});
    for(int x=100;x<110;++x)check(view.execute({simulation::CommandType::PlaceRoad,{x,103}}).accepted,"budget UI roads");
    const auto screen=[&](simulation::Cell cell){return view.camera().world_to_screen(maps::terrain_ground(
        {static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},border));};
    const auto select=[&](simulation::Cell cell){view.set_tool(5);const auto point=screen(cell);
        mouse_click(view,static_cast<float>(point.x),static_cast<float>(point.y),running);};
    const auto contains=[&](const char* wanted){std::string text;for(const auto& line:view.inspection_lines())text+=line+" ";return text.find(wanted)!=std::string::npos;};
    view.set_tool(1);check(view.tool()==1,"funded Road tool disabled");
    const auto road_start=screen({115,107}),road_end=screen({120,107});
    const auto before_road=view.world().snapshot();perf::set_enabled(true);perf::reset();
    view.handle_event(click(static_cast<float>(road_start.x),static_cast<float>(road_start.y)),running);
    for(int i=0;i<100;++i)view.handle_event(motion(static_cast<float>(road_end.x),static_cast<float>(road_end.y)),running);
    view.update(0);check(view.road_preview().cells.size()==6 && view.render(),"actual upkeep city road preview");
    for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::BfsCalls,
        perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites})check(perf::counter(counter)==0,"upkeep road preview did work");
    check(view.world().snapshot()==before_road,"upkeep road preview mutation");perf::set_enabled(false);
    view.handle_event(key(SDLK_ESCAPE),running);
    for(int i=0;i<399;++i)view.tick_once();
    select({100,104});check(contains("Maintenance: 12 every 400 ticks") && contains("Next due: 1 ticks"),"Pottery maintenance inspector");
    const auto pottery=*view.world().building_owner_at({100,104});view.execute(simulation::set_building_operation(pottery,false));
    check(contains("Maintenance continues while paused."),"paused upkeep explanation");
    select({120,104});check(contains("Maintenance: 2 every 400 ticks") && contains("Covered Houses 0") && !contains("Workers"),"Well upkeep inspector");
    select({112,104});view.execute(simulation::set_building_operation(post,false));
    check(contains("Maintenance: 4 every 400 ticks") && contains("Worker phase:") && contains("Maintenance continues while paused."),"Health Post upkeep inspector");
    view.tick_once();check(view.world().treasury()==-58 && view.world().maintenance_spent_total()==78,"negative Funds UI fixture");
    check(contains("Due this tick"),"due-at-current-tick inspector");
    const auto snapshot=view.world().snapshot();view.save_now();view.tick_once();view.load_now();
    check(view.paused() && view.world().snapshot()==snapshot && openemperor::persistence::read_save(view.save_path()).source_schema_version==18,"negative schema18 load paused");
    perf::set_enabled(true);perf::reset();
    for(int i=0;i<100;++i) {
        view.handle_event(key(SDLK_F1),running);view.handle_event(key(SDLK_H),running);check(view.render(),"maintenance help/HUD render");
        view.handle_event(key(SDLK_H),running);
        const auto point=screen({115,108});view.handle_event(motion(static_cast<float>(point.x),static_cast<float>(point.y)),running);
        view.update(0);check(view.render(),"maintenance cost preview");
    }
    view.set_tool(1);check(view.tool()==5,"paid Road tool enabled in debt");
    for(const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
        perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::SimulationTicks})check(perf::counter(counter)==0,"upkeep presentation/preview did work");
    check(view.world().snapshot()==snapshot,"budget UI mutated World");perf::set_enabled(false);
    // The complete HUD/guidance path must also accept the signed endpoint.
    auto extreme=view.capture_save_document();extreme.world.treasury=INT64_MIN;
    extreme.world.maintenance_spent_total=static_cast<std::uint64_t>(INT64_MAX)+1+20;
    openemperor::persistence::write_save(view.save_path(),extreme,temp.path,view.buildable_mask());
    view.load_now();check(view.paused() && view.world().treasury()==INT64_MIN && view.render(),"signed endpoint HUD/guidance");
    view.shutdown();std::cout<<"City-v16 SDL: debt, placement cost/upkeep, paused/Well/Health inspectors, schema18 paused load, pure Help/HUD/road preview\n";
}

void city_v16_geometry_checks(const Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    namespace perf=openemperor::performance;
    using Role=openemperor::assets::BuildingVisualRole;
    const auto base=building_fixture(temp);
    nlohmann::json source;{std::ifstream in(base);in>>source;}
    const auto manifest=temp.path/"safety.json";
    auto entries=nlohmann::json::object();
    for (const char* role:{"well","health_post"}) {
        entries[role]=source["buildings"]["pottery"];
        entries[role]["footprint_side"]=2;
    }
    const auto save_profile=[&] {std::ofstream out(manifest);out<<nlohmann::json{
        {"schema_version",1},{"mode","curated_building_preview"},{"buildings",entries}};};
    save_profile();std::vector<std::uint8_t> mask;
    auto session=city_v10_fixture(temp,mask);const auto border=session.plan.border;
    openemperor::SandboxView view(std::move(session),false,simulation::RulesProfile::CityV16);
    Temp outside;view.configure_save(temp.path,"Cities/Synthetic.map",outside.path/"safety.json");
    view.set_building_visuals(manifest,openemperor::VisualProfileSource::Builtin);
    view.initialize(window,renderer);bool running=true;
    const auto put=[&](simulation::CommandType type,simulation::Cell c) {
        check(view.execute({type,c}).accepted,"rule2 SDL placement");return *view.world().building_owner_at(c);};
    const auto well=put(simulation::CommandType::PlaceWell,{112,106});
    const auto post=put(simulation::CommandType::PlaceHealthPost,{116,106});
    check(view.world().rule_version()==2 && view.building_texture_count()==1,"rule2 eager dedupe");
    const auto exact=view.world().snapshot();
    for (const int zoom:{1,2,4}) for (const auto id:{well,post}) {
        view.handle_event(key(SDLK_R),running);
        const auto cell=view.world().building(id).cell;
        const auto& map=view.layout().map;const openemperor::scene::Point q{map.x+map.w*.5,map.y+map.h*.5};
        const auto wheel=[&](openemperor::scene::Point p,double factor) {
            SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);
            e.wheel.mouse_y=static_cast<float>(p.y);e.wheel.y=static_cast<float>(std::log(factor)/std::log(1.15));view.handle_event(e,running);};
        wheel(q,zoom/view.camera().zoom);
        for (int n=0;n<25;++n) {
            const auto p=view.camera().world_to_screen(openemperor::maps::terrain_ground(
                {static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border));
            const openemperor::scene::Point d{std::clamp(q.x-p.x,-100.,100.),std::clamp(q.y-p.y,-100.,100.)};
            if (std::abs(d.x)+std::abs(d.y)<.01)break;
            wheel({q.x+d.x*.5,q.y+d.y*.5},.5);wheel({q.x-d.x*.5,q.y-d.y*.5},2.);
        }
        view.set_tool(5);
        for (const auto c:simulation::building_footprint_cells(view.world().profile(),2,
                view.world().building(id).kind,cell)) {
            const auto p=view.camera().world_to_screen(openemperor::maps::terrain_ground(
                {static_cast<unsigned>(c.x),static_cast<unsigned>(c.y)},border));
            mouse_click(view,static_cast<float>(p.x),static_cast<float>(p.y),running);
            check(view.selected_building()==id,"rule2 SDL four-cell picking at zoom");
        }
        perf::set_enabled(true);perf::reset();
        check(view.render(),"rule2 Safety sprite render");
        const auto stats=view.building_display_stats();
        for (const auto role:{Role::Well,Role::HealthPost}) check(stats.drawn_instances[openemperor::assets::role_index(role)]>0 &&
            stats.placeholder_fallbacks[openemperor::assets::role_index(role)]==0,"rule2 selected profile used fallback");
        for (const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::WorldRestores,
            perf::Counter::BfsCalls,perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,
            perf::Counter::TextureUploads,perf::Counter::FileReads,perf::Counter::FileWrites})
            check(perf::counter(c)==0,"rule2 render performed work");
        perf::set_enabled(false);check(view.world().snapshot()==exact,"rule2 presentation mutation");
    }
    view.set_tool(12);
    const auto p=view.camera().world_to_screen(openemperor::maps::terrain_ground({116,104},border));
    view.handle_event(motion(static_cast<float>(p.x),static_cast<float>(p.y)),running);view.update(0);
    const auto builds=view.water_preview_build_count();perf::set_enabled(true);perf::reset();
    for (int n=0;n<200;++n)view.handle_event(motion(static_cast<float>(p.x),static_cast<float>(p.y)),running);
    view.update(0);check(view.render() && view.water_preview_build_count()==builds,"rule2 coalesced preview");
    for(const auto c:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::BfsCalls,
        perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads})
        check(perf::counter(c)==0,"rule2 preview performed work");
    perf::set_enabled(false);check(view.world().snapshot()==exact,"rule2 preview changed World");
    // A legacy-sized explicit custom profile must fail before replacing textures.
    const auto watch=fire_watch_fixture(temp);nlohmann::json old;{std::ifstream in(watch);in>>old;}
    auto legacy_entry=old["buildings"]["fire_watch"];
    entries["well"]=legacy_entry;save_profile();bool rejected=false;
    try{view.set_building_visuals(manifest);}catch(const std::exception&){rejected=true;}
    check(rejected && view.building_texture_count()==1 && view.render(),"mismatched custom profile replaced v2");
    // Missing/unknown visuals retain a foundation-aware authored fallback.
    view.set_building_visuals({});check(view.render(),"unknown v2 fallback render");
    const auto fallbacks=view.building_display_stats();
    for(const auto role:{Role::Well,Role::HealthPost})check(fallbacks.placeholder_fallbacks[openemperor::assets::role_index(role)]>0,"unknown Safety fallback count");
    view.save_now();auto legacy=openemperor::persistence::read_save(view.save_path());legacy.world.rule_version=1;
    view.shutdown();
    auto old_session=city_v10_fixture(temp,mask);
    openemperor::SandboxView old_view(std::move(old_session),false,simulation::RulesProfile::CityV16);
    old_view.configure_save(temp.path,"Cities/Synthetic.map",outside.path/"legacy.json",legacy);
    entries["well"]=source["buildings"]["pottery"];entries["well"]["footprint_side"]=2;save_profile();
    old_view.set_building_visuals(manifest,openemperor::VisualProfileSource::Builtin);
    old_view.initialize(window,renderer);check(old_view.render(),"legacy built-in fallback");
    for(const auto role:{Role::Well,Role::HealthPost})check(!old_view.building_display_stats().configured_roles[openemperor::assets::role_index(role)],"2x2 sprite exposed in legacy");
    entries["well"]=legacy_entry;entries["health_post"]=legacy_entry;save_profile();old_view.set_building_visuals(manifest);
    check(old_view.render() && old_view.world().rule_version()==1 && old_view.building_texture_count()==1,"legacy custom1x1 rejected");
    old_view.shutdown();std::cout<<"Safety rule2 SDL: 1x/2x/4x picking, full footprints, eager dedupe, pure preview, legacy/unknown fallback\n";
}

void road_continuity_checks(const Temp& temp,SDL_Window* window,SDL_Renderer* renderer) {
    namespace scene=openemperor::scene;
    namespace perf=openemperor::performance;
    using Cell=simulation::Cell;
    auto manifest=road_fixture(temp);
    nlohmann::json profile;
    { std::ifstream in(manifest);in>>profile; }
    // Uniform authored paving, independent of Emperor's pixels or mask table.
    Bytes paving(4U+16U*3200U,0);
    for (std::size_t i=4;i<paving.size();i+=2) u16(paving,i,0x6a8a);
    write(temp.path/"DATA/roads.555",paving);
    const auto save_profile=[&](bool replaces) {
        profile["replaces_ground"]=replaces;
        std::ofstream out(manifest);out<<profile;check(bool(out),"contact profile write");
    };
    const auto raised_session=[&] {
        std::vector<std::uint8_t> mask;
        auto session=city_v10_fixture(temp,mask);
        // A complete green base with a raised opaque Omega strip. The front
        // cell's old overlay reaches back across the road behind it.
        Bytes sg3(40680U+64U,0),terrain(3200U,0);
        u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));u32(sg3,4,213);
        u32(sg3,12,1);u32(sg3,16,1);u32(sg3,20,1);
        const std::string name="synthetic-raised.bmp";
        std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,680+124,1);
        for (std::size_t i=0;i<terrain.size();i+=2) u16(terrain,i,0x03e0);
        unsigned skip=78U*20U;
        while (skip) { const auto n=std::min(skip,254U);terrain.push_back(255);
            terrain.push_back(static_cast<std::uint8_t>(n));skip-=n; }
        for (int row=0;row<8;++row) {
            terrain.push_back(78);
            for (int x=0;x<78;++x) {terrain.push_back(0xe0);terrain.push_back(0x03);}
        }
        u32(sg3,40680+4,static_cast<std::uint32_t>(terrain.size()));u32(sg3,40680+8,3200);
        u16(sg3,40680+20,78);u16(sg3,40680+22,60);u16(sg3,40680+50,30);sg3[40680+55]=1;
        write(temp.path/"DATA/test.sg3",sg3);write(temp.path/"DATA/test.555",terrain);
        session.plan.assets[0].record.height=60;
        session.plan.assets[0].record.data_length=static_cast<std::uint32_t>(terrain.size());
        for (auto& cell:session.plan.cells) cell.image_origin=maps::stored_image_origin(cell.world,78,60);
        for (auto& f:session.plan.footprints) f.image_origin=session.plan.cells[f.cell_indices[0]].image_origin;
        return session;
    };
    std::vector<std::vector<Cell>> patterns;
    constexpr std::array<Cell,4> neighbors{{{0,-1},{1,0},{0,1},{-1,0}}};
    for (int mask=0;mask<16;++mask) {
        std::vector<Cell> cells{{0,0}};
        for (unsigned i=0;i<4;++i) if (static_cast<unsigned>(mask)&(1U<<i)) cells.push_back(neighbors[i]);
        patterns.push_back(std::move(cells));
    }
    std::vector<Cell> x,y,l,t,c,grid;
    for (int i=0;i<8;++i) {
        x.push_back({i,0});y.push_back({0,i});l.push_back({i,0});t.push_back({i,0});c.push_back({i,0});
        if (i) {l.push_back({7,i});t.push_back({4,i});c.push_back({0,i});}
    }
    for (int v=0;v<5;++v) for (int u=0;u<5;++u) grid.push_back({u,v});
    patterns.insert(patterns.end(),{x,y,l,t,c,grid});
    std::size_t reproduced=0,contacts=0;
    for (const auto& pattern:patterns) {
        save_profile(false);
        auto session=raised_session();const auto border=session.plan.border;
        openemperor::SandboxView view(std::move(session),false,simulation::RulesProfile::CityV16);
        view.configure_save(temp.path,"Cities/Synthetic.map",temp.path/"unused.json");
        view.set_road_visuals(manifest);view.initialize(window,renderer);bool running=true;
        std::vector<Cell> cells;
        for (auto d:pattern) {
            Cell p{110+d.x,104+d.y};cells.push_back(p);
            check(view.execute({simulation::CommandType::PlaceRoad,p}).accepted,"contact road placement");
        }
        if (!view.paused()) view.handle_event(key(SDLK_SPACE),running);
        view.set_tool(5);
        const auto screen=[&](Cell cell) { return view.camera().world_to_screen(maps::terrain_ground(
            {static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border)); };
        const auto focus=[&](int zoom) {
            view.handle_event(key(SDLK_R),running);const auto map=view.layout().map;
            scene::Point q{map.x+map.w*.5,map.y+map.h*.5};
            const auto wheel=[&](scene::Point at,double factor) {
                SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(at.x);
                e.wheel.mouse_y=static_cast<float>(at.y);e.wheel.y=static_cast<float>(std::log(factor)/std::log(1.15));
                view.handle_event(e,running);
            };
            wheel(q,zoom/view.camera().zoom);
            double u=0,v=0;for (auto p:cells) {u+=p.x;v+=p.y;}u/=static_cast<double>(cells.size());v/=static_cast<double>(cells.size());
            const auto base=maps::terrain_ground({110,104},border);
            const scene::Point center{base.x+(u-110-v+104)*40,base.y+(u-110+v-104)*20};
            for (int i=0;i<25;++i) {
                auto p=view.camera().world_to_screen(center);
                scene::Point d{std::clamp(q.x-p.x,-300.,300.),std::clamp(q.y-p.y,-250.,250.)};
                if (std::abs(d.x)+std::abs(d.y)<.01) break;
                wheel({q.x+d.x*.5,q.y+d.y*.5},.5);wheel({q.x-d.x*.5,q.y-d.y*.5},2);
            }
            // Exercise the exact player zoom presets after pointer-based focusing.
            for (int i=0;i<4 && view.camera().zoom!=zoom;++i) view.handle_event(key(SDLK_Z),running);
            check(view.camera().zoom==zoom,"exact contact zoom preset");
        };
        const auto gaps=[&](int zoom,bool preview=false) {
            check(view.render(),"contact production render");
            auto* surface=SDL_RenderReadPixels(renderer,nullptr);check(surface,"contact readback");
            std::size_t missing=0;
            for (auto a:cells) for (auto d:{Cell{1,0},Cell{0,1}}) {
                Cell b{a.x+d.x,a.y+d.y};
                if (std::find(cells.begin(),cells.end(),b)==cells.end()) continue;
                ++contacts;const auto p=screen(a),q=screen(b);const double sign=q.x>p.x ? 1:-1;
                // Every pixel of a five-lane road core, spanning the shared
                // edge and both interiors, must remain paving at all zooms.
                for (int row=3*zoom;row<18*zoom;++row) for (int lane=-2*zoom;lane<=2*zoom;++lane) {
                    const int px=static_cast<int>(std::floor(p.x+sign*2*row+lane));
                    const int py=static_cast<int>(std::floor(p.y+row));
                    std::uint8_t r,g,blue,alpha;
                    check(SDL_ReadSurfacePixel(surface,px,py,&r,&g,&blue,&alpha),"contact pixel bounds");
                    // The existing white hover outline is an intentional UI overlay.
                    const bool outline=r==255 && g==255 && blue==255;
                    const bool bad=!outline && (preview ? (r<70 || blue<25):(r<=g || g<=blue));
                    missing+=bad;
                }
            }
            SDL_DestroySurface(surface);return missing;
        };
        focus(1);reproduced+=gaps(1);
        save_profile(true);view.set_road_visuals(manifest);
        const auto snapshot=view.world().snapshot();
        for (int zoom:{1,2,4}) {
            focus(zoom);check(gaps(zoom)==0,"raised stored terrain interrupted a connected road");
            view.handle_event(key(SDLK_F7),running);
            check(gaps(zoom)==0,"map-first replacement interrupted a connected road");
            view.handle_event(key(SDLK_F7),running);
        }
        view.handle_event(key(SDLK_F6),running);check(view.render(),"contact F6 off");
        check(!view.road_visuals_active(),"contact F6 fallback");
        view.handle_event(key(SDLK_F6),running);check(view.world().snapshot()==snapshot,"contact toggles changed World");
        // Preview the same pattern's long horizontal L path after ordinary road removal.
        if (pattern==l) {
            for (auto p:cells) check(view.execute({simulation::CommandType::RemoveRoad,p}).accepted,"clear preview roads");
            focus(1);view.set_tool(1);const auto start=screen(cells.front()),end=screen(cells.back());
            const auto before=view.world().snapshot();
            perf::set_enabled(true);perf::reset();
            view.handle_event(click(static_cast<float>(start.x),static_cast<float>(start.y)),running);
            view.handle_event(motion(static_cast<float>(end.x),static_cast<float>(end.y)),running);view.update(0);
            check(view.road_preview().valid && view.road_preview().cells.size()==15,"L contact preview");
            check(gaps(1,true)==0,"preview ground interrupted connected alpha paving");
            check(view.world().snapshot()==before,"preview contact mutated World");
            for (auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldExecutes,perf::Counter::BfsCalls,
                perf::Counter::RouteRefreshes,perf::Counter::AssetDecodes,perf::Counter::TextureUploads,
                perf::Counter::FileReads,perf::Counter::FileWrites})
                check(perf::counter(counter)==0,"contact preview did expensive or authoritative work");
            perf::set_enabled(false);view.handle_event(key(SDLK_ESCAPE),running);
        }
        view.shutdown();
    }
    check(reproduced>0 && contacts>500,"contact fixture did not expose the old composition failure");
    std::cout<<"Road continuity: 16 neighborhoods + 8-cell x/y, L/T/cross/grid; actual 1x/2x/4x pixels, alpha preview, zero authority/work\n";
}

int main(int argc,char** argv) {
    try {
        const bool road_only=argc==2 && std::string(argv[1])=="--road-continuity-only";
        const bool geometry_only=argc==2 && std::string(argv[1])=="--geometry16-only";
        const bool maintenance_only=argc==2 && std::string(argv[1])=="--maintenance-only";
        const bool health_only=argc==2 && std::string(argv[1])=="--health-only";
        const bool water_only=argc==2 && std::string(argv[1])=="--water-only";
        const bool alpha_stress=argc==2 && std::string(argv[1])=="--alpha-stress";
        check(argc==1 || alpha_stress || water_only || health_only || maintenance_only || geometry_only || road_only,"usage: openemperor-sandbox-view-tests [--alpha-stress]");
        {
            const auto layout=openemperor::sandbox_ui::make_layout(1100,700,1100,700,true);
            check(layout.top.h==52 && layout.map.w==796 && layout.map.h==516 &&
                  layout.toolbar.h==108 && layout.panel.w==304,
                  "1100x700 map area/layout");
            check(layout.ui_at(900,200) && !layout.ui_at(400,200) &&
                  layout.button_at(1050,20)==openemperor::sandbox_ui::Action::TogglePanel,
                  "layout hit regions");
            const auto hidpi=openemperor::sandbox_ui::make_layout(2200,1400,1100,700,true);
            check(hidpi.scale==2 && hidpi.map.w==1592 && hidpi.map.h==1032,
                  "2x display layout");
            const auto farm_button=std::find_if(hidpi.buttons.begin(),hidpi.buttons.end(),
                [](const auto& button) {
                    return button.action==openemperor::sandbox_ui::Action::Farm;
                });
            check(farm_button!=hidpi.buttons.end() && farm_button->rect.w>0 &&
                  farm_button->rect.x+farm_button->rect.w<=hidpi.toolbar.w,
                  "City v7 Farm tool is not readable at 2x layout");
            const auto small=openemperor::sandbox_ui::make_layout(600,400,600,400,true);
            check(!small.panel_open && small.map.w==600 && small.map.h==216,
                  "small display map area");
            const auto small_farm=std::find_if(small.buttons.begin(),small.buttons.end(),
                [](const auto& button) {
                    return button.action==openemperor::sandbox_ui::Action::Farm;
                });
            check(small_farm!=small.buttons.end() && small_farm->rect.w>0 &&
                  small_farm->rect.x+small_farm->rect.w<=small.toolbar.w,
                  "City v7 Farm tool is not readable in small layout");
            for (const int width:{1440,1728,1920}) {
                const auto responsive=openemperor::sandbox_ui::make_layout(
                    width,900,width,900,true);
                const auto service=std::find_if(responsive.buttons.begin(),responsive.buttons.end(),
                    [](const auto& button) {
                        return button.action==openemperor::sandbox_ui::Action::ServicePost;
                    });
                check(service!=responsive.buttons.end() && service->rect.w>=180 &&
                      service->rect.x>=0 && service->rect.x+service->rect.w<=width &&
                      service->rect.y>=responsive.toolbar.y &&
                      service->rect.y+service->rect.h<=responsive.toolbar.y+responsive.toolbar.h,
                      "City v8 Service tool does not fit responsive toolbar");
                const auto market=std::find_if(responsive.buttons.begin(),responsive.buttons.end(),
                    [](const auto& button) {
                        return button.action==openemperor::sandbox_ui::Action::Market;
                    });
                check(market!=responsive.buttons.end() && market->rect.w>0 &&
                      market->rect.x>=0 && market->rect.x+market->rect.w<=width &&
                      market->rect.y>=responsive.toolbar.y &&
                      market->rect.y+market->rect.h<=responsive.toolbar.y+responsive.toolbar.h,
                      "City v11 Market tool does not fit responsive toolbar");
            }
            const auto closed=openemperor::sandbox_ui::make_layout(1100,700,1100,700,false);
            check(!closed.ui_at(900,200) && closed.map.w==1100,
                  "closed panel still blocks map");
            simulation::World world(20,20,std::vector<std::uint8_t>(400,1),
                                    simulation::RulesProfile::ProductionV2);
            auto path=openemperor::sandbox_ui::plan_road(world,{2,5},{6,7});
            check(path.valid && path.cells.size()==7 && path.cells[4]==simulation::Cell{6,5} &&
                  path.cells.back()==simulation::Cell{6,7},"X-then-Y road layout");
            const auto before=world.snapshot();
            std::string reason;
            check(openemperor::sandbox_ui::commit_road(world,path,reason),"road commit");
            check(openemperor::sandbox_ui::plan_road(world,{2,5},{6,7}).valid,
                  "existing roads not allowed in preview");
            simulation::World individual=simulation::World::restore(before,std::vector<std::uint8_t>(400,1));
            for (const auto cell:path.cells)
                check(individual.execute({simulation::CommandType::PlaceRoad,cell}).accepted,
                      "individual road command");
            check(world.snapshot()==individual.snapshot(),"drag differs from individual commands");
            const auto unchanged=world.snapshot();
            check(openemperor::sandbox_ui::commit_road(world,path,reason) &&
                  world.snapshot()==unchanged,"existing roads changed command sequence");
            check(!openemperor::sandbox_ui::plan_road(world,{0,0},{300,0}).valid,
                  "road length cap");
            const auto single=openemperor::sandbox_ui::plan_road(world,{9,9},{9,9});
            const auto old_sequence=world.command_sequence();
            check(single.valid && single.cells.size()==1 &&
                  openemperor::sandbox_ui::commit_road(world,single,reason) &&
                  world.command_sequence()==old_sequence+1,
                  "single-cell road click");
            check(world.execute({simulation::CommandType::PlacePottery,{10,5}}).accepted,
                  "road obstacle placement");
            const auto blocked=world.snapshot();
            const auto bad=openemperor::sandbox_ui::plan_road(world,{8,5},{12,5});
            check(!bad.valid && !openemperor::sandbox_ui::commit_road(world,bad,reason) &&
                  world.snapshot()==blocked,"obstacle partially committed road");
        }
        Temp temp;
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),"dummy driver");
        check(SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software"),"software renderer");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL init");
        SDL_Window* window=nullptr;
        SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("sandbox test",road_only ? 3000:800,road_only ? 2200:600,0,&window,&renderer),"window");
        if (road_only) {
            road_continuity_checks(temp,window,renderer);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        if (geometry_only) {
            city_v16_geometry_checks(temp,window,renderer);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        if (maintenance_only) {
            city_v16_maintenance_checks(temp,window,renderer);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        if (health_only) {
            city_v15_health_checks(temp,window,renderer);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        if (water_only) {
            city_v14_water_checks(temp,window,renderer);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
        }
        household_stage_checks(temp,window,renderer);
        openemperor::SandboxView view(fixture(temp),false);
        view.initialize(window,renderer);
        bool running=true;
        check(!view.debug_diagnostics(),"presentation diagnostics were not off by default");
        const auto f1_world=view.world().snapshot();
        const auto f1_dirty=view.dirty();
        for (int i=0;i<50;++i) view.handle_event(key(SDLK_F1),running);
        check(!view.debug_diagnostics() && view.world().snapshot()==f1_world &&
              view.dirty()==f1_dirty,"50 F1 toggles changed World or dirty state");
        view.handle_event(key(SDLK_F1),running);
        check(view.debug_diagnostics(),"F1 did not enable technical diagnostics");
        const auto screen_for=[&](int x) {
            auto p=maps::terrain_world({static_cast<std::uint32_t>(x),114},72);
            p.y+=20;
            return view.camera().world_to_screen(p);
        };
        const auto workshop=screen_for(110);
        check(view.pick(workshop)==simulation::Cell{110,114},"projected pick");
        view.set_tool(2);
        check(view.preview({110,114}).accepted,"valid preview");
        check(!view.preview({116,114}).accepted,"invalid preview");
        mouse_click(view,static_cast<float>(workshop.x),static_cast<float>(workshop.y),running);
        check(view.world().workshop()==simulation::Cell{110,114},"click did not place workshop");
        const auto before_zoom=view.camera().screen_to_world(workshop);
        SDL_Event wheel{}; wheel.type=SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.mouse_x=static_cast<float>(workshop.x);
        wheel.wheel.mouse_y=static_cast<float>(workshop.y);
        wheel.wheel.y=1;
        view.handle_event(wheel,running);
        const auto after_zoom=view.camera().screen_to_world(workshop);
        check(std::abs(before_zoom.x-after_zoom.x)<1e-5 &&
              std::abs(before_zoom.y-after_zoom.y)<1e-5,"pointer zoom moved picked world point");
        for (int x=111;x<=114;++x) {
            view.set_tool(1);
            const auto at=screen_for(x);
            check(view.pick(at)==simulation::Cell{x,114},"pick after zoom");
            check(view.preview({x,114}).accepted,"road preview differs from validation");
            mouse_click(view,static_cast<float>(at.x),static_cast<float>(at.y),running);
        }
        view.set_tool(3);
        const auto warehouse=screen_for(115);
        mouse_click(view,static_cast<float>(warehouse.x),static_cast<float>(warehouse.y),running);
        check(view.world().warehouse()==simulation::Cell{115,114},"warehouse click");
        const auto status_before=view.world().warehouse_stock();
        check(view.render(),"render frame with overlay");
        const auto p=pixel(renderer,static_cast<int>(workshop.x),
            static_cast<int>(workshop.y-11*view.camera().zoom));
        check(p[2]>p[1] && p[1]>p[0],"workshop overlay was not drawn before present");
        for (int i=0;i<120;++i) view.tick_once();
        check(view.world().courier_phase()==simulation::CourierPhase::ToWarehouse,
              "courier did not move in viewer");
        check(view.world().warehouse_stock()==status_before,"premature delivery in viewer");
        check(view.render(),"render moving marker");
        const auto moving=*view.world().courier_position();
        check(moving.x>110 && moving.x<115,"moving marker position");
        auto marker_world=maps::terrain_world({static_cast<std::uint32_t>(moving.x),114},72);
        marker_world.y+=20;
        const auto marker_screen=view.camera().world_to_screen(marker_world);
        const auto marker_pixel=pixel(renderer,static_cast<int>(marker_screen.x),
            static_cast<int>(marker_screen.y));
        check(marker_pixel[0]>235 && marker_pixel[1]>235 && marker_pixel[2]>235,
              "moving courier marker absent from composed frame");
        view.handle_event(key(SDLK_F5),running);
        check(view.last_message().find("No sandbox save path configured")!=std::string::npos,
              "unconfigured F5 did not report missing path");
        const auto before_disabled_v1=view.world().snapshot();
        view.handle_event(key(SDLK_F4),running);
        check(view.world().snapshot()==before_disabled_v1 &&
              view.last_message()=="No building visuals loaded",
              "F4 without a building profile changed World or hid missing profile");
        for (const auto& item:view.layout().buttons)
            if (item.action==openemperor::sandbox_ui::Action::Household)
                mouse_click(view,static_cast<float>(item.rect.x+item.rect.w/2),
                            static_cast<float>(item.rect.y+item.rect.h/2),running);
        check(view.world().snapshot()==before_disabled_v1,
              "disabled old-profile house button placed a building");
        for (const auto& item:view.layout().buttons)
            if (item.action==openemperor::sandbox_ui::Action::Save)
                mouse_click(view,static_cast<float>(item.rect.x+item.rect.w/2),
                            static_cast<float>(item.rect.y+item.rect.h/2),running);
        check(view.world().snapshot()==before_disabled_v1 &&
              view.last_message().find("No sandbox save path configured")!=std::string::npos,
              "disabled Save button gave no explanation");
        view.shutdown();

        openemperor::SandboxView builtin_failure_view(fixture(temp,true),false,
            simulation::RulesProfile::ProductionV2);
        builtin_failure_view.set_walker_visuals(temp.path/"missing-builtin-walkers.json",
            openemperor::VisualProfileSource::Builtin);
        builtin_failure_view.set_building_visuals(temp.path/"missing-builtin-buildings.json",
            openemperor::VisualProfileSource::Builtin);
        builtin_failure_view.set_road_visuals(temp.path/"missing-builtin-roads.json",
            openemperor::VisualProfileSource::Builtin);
        builtin_failure_view.initialize(window,renderer);
        check(builtin_failure_view.walker_visual_source()==openemperor::VisualProfileSource::Fallback &&
              builtin_failure_view.building_visual_source()==openemperor::VisualProfileSource::Fallback &&
              builtin_failure_view.road_visual_source()==openemperor::VisualProfileSource::Fallback &&
              builtin_failure_view.render(),
              "broken built-in visual profiles blocked the sandbox or failed to fall back");
        builtin_failure_view.shutdown();

        openemperor::SandboxView production_view(fixture(temp,true),false,
            simulation::RulesProfile::ProductionV2);
        const auto save_path=temp.path.parent_path()/
            (temp.path.filename().string()+"-viewer-save.json");
        production_view.configure_save(temp.path,"Cities/Synthetic.map",save_path);
        production_view.initialize(window,renderer);
        production_view.handle_event(key(SDLK_F1),running);
        check(production_view.debug_diagnostics(),"v2 diagnostics did not enable");
        check(std::string(SDL_GetWindowTitle(window)).find("OpenEmperor 0.1.0-alpha.2 - Production v2")!=std::string::npos,
              "v2 window profile label");
        const auto v2_screen=[&](int x) {
            auto point=maps::terrain_world({static_cast<std::uint32_t>(x),114},72);
            point.y+=20;
            return production_view.camera().world_to_screen(point);
        };
        auto v2_click=[&](SDL_Keycode tool,int x) {
            production_view.handle_event(key(tool),running);
            const auto at=v2_screen(x);
            check(production_view.pick(at)==simulation::Cell{x,114},"v2 pick");
            check(production_view.preview({x,114}).accepted,"v2 preview");
            mouse_click(production_view,static_cast<float>(at.x),static_cast<float>(at.y),running);
        };
        v2_click(SDLK_2,110);
        v2_click(SDLK_1,111);
        v2_click(SDLK_1,112);
        v2_click(SDLK_3,113);
        v2_click(SDLK_1,114);
        v2_click(SDLK_1,115);
        v2_click(SDLK_4,116);
        check(production_view.world().building(simulation::BuildingId::ClaySource).placed &&
              production_view.world().building(simulation::BuildingId::Pottery).placed &&
              production_view.world().building(simulation::BuildingId::Warehouse).placed,
              "v2 keyboard tools did not place all buildings");
        check(!production_view.preview({117,114}).accepted,"v2 invalid preview");
        production_view.handle_event(key(SDLK_5),running);
        check(production_view.tool()==5,"v2 select key");
        for (int i=0;i<400;++i) production_view.tick_once();
        check(production_view.world().pottery_completed_total()>0 &&
              production_view.world().courier(simulation::CourierId::Pottery).cargo>0,
              "v2 recipe/courier did not run in view");
        check(production_view.render() && production_view.last_courier_draws()==2,
              "software frame omitted a courier");
        const auto sample_courier=[&](simulation::CourierId id,int shift) {
            const auto position=*production_view.world().courier_position(id);
            const double u=position.x-72,v=position.y-72;
            const auto screen=production_view.camera().world_to_screen({(u-v)*40,(u+v)*20+20});
            return pixel(renderer,static_cast<int>(screen.x+shift),static_cast<int>(screen.y));
        };
        const auto a_color=sample_courier(simulation::CourierId::Clay,-5);
        const auto b_color=sample_courier(simulation::CourierId::Pottery,5);
        check(a_color[2]>a_color[0] && b_color[0]>b_color[2],
              "distinct courier body colors absent from composed frame");
        const auto before_save=production_view.world().snapshot();
        const auto before_position=production_view.world().courier_position(simulation::CourierId::Pottery);
        const auto manual_generation=production_view.save_generation();
        const auto manual_target=production_view.save_path();
        const auto was_dirty=production_view.dirty();
        const auto captured=production_view.capture_save_document();
        check(captured.world==before_save && production_view.world().snapshot()==before_save &&
              production_view.save_generation()==manual_generation &&
              production_view.save_path()==manual_target && production_view.dirty()==was_dirty &&
              !std::filesystem::exists(manual_target),
              "recovery capture changed manual save state, target, dirty markers, or World");
        production_view.handle_event(key(SDLK_F5),running);
        check(production_view.last_message().find("Saved tick ")!=std::string::npos,
              "F5 did not report saved tick in HUD state");
        check(production_view.world().snapshot()==before_save,"F5 advanced simulation");
        for (int i=0;i<13;++i) production_view.tick_once();
        production_view.handle_event(key(SDLK_F9),running);
        check(production_view.world().snapshot()==before_save && production_view.paused(),
              "F9 did not restore and pause world");
        check(production_view.last_message().find("Loaded tick ")!=std::string::npos,
              "F9 did not report loaded tick in HUD state");
        check(production_view.world().courier_position(simulation::CourierId::Pottery)==before_position,
              "loaded courier marker position changed");
        check(production_view.render() && production_view.last_courier_draws()==2,
              "loaded software frame omitted couriers");
        const auto loaded_b=sample_courier(simulation::CourierId::Pottery,5);
        const auto position=*production_view.world().courier_position(simulation::CourierId::Pottery);
        const double u=position.x-72,v=position.y-72;
        const auto center=production_view.camera().world_to_screen({(u-v)*40,(u+v)*20+20});
        const double marker_size=std::max(5.0,10.0*production_view.camera().zoom);
        const auto cargo_pixel=pixel(renderer,
            static_cast<int>(center.x+5+marker_size*0.25),
            static_cast<int>(center.y-marker_size*0.75));
        check(loaded_b[0]>loaded_b[2] && cargo_pixel[0]>200 && cargo_pixel[1]<100 &&
              cargo_pixel[2]>150 && production_view.world().courier(simulation::CourierId::Pottery).cargo>0,
              "loaded courier marker or cargo indicator absent");
        bool outbound_edge=false;
        for (int i=0;i<500 && !outbound_edge;++i) {
            const auto& c=production_view.world().courier(simulation::CourierId::Clay);
            outbound_edge=c.phase==simulation::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==simulation::Cell{110,114};
            if (!outbound_edge) production_view.tick_once();
        }
        check(outbound_edge,"viewer did not reach loaded outbound edge");
        check(production_view.paused(),"viewer removal test was not paused");
        production_view.handle_event(key(SDLK_6),running);
        check(production_view.tool()==6 &&
              !production_view.preview({111,114}).accepted &&
              std::string(production_view.preview({111,114}).reason).find("occupied")!=std::string::npos &&
              production_view.preview({112,114}).accepted,
              "remove-road preview ignored protected or future cell");
        const auto pre_remove_position=production_view.world().courier_position(simulation::CourierId::Clay);
        v2_click(SDLK_6,112);
        check(production_view.world().courier_position(simulation::CourierId::Clay)==pre_remove_position &&
              production_view.world().courier(simulation::CourierId::Clay).route_pending &&
              production_view.last_message()=="Road removed",
              "viewer removal moved courier or hid command result");
        bool waiting=false;
        for (int i=0;i<20 && !waiting;++i) {
            production_view.tick_once();
            const auto& c=production_view.world().courier(simulation::CourierId::Clay);
            waiting=c.route_pending && c.edge_progress==0;
        }
        check(waiting && production_view.world().courier_position(simulation::CourierId::Clay)==
              simulation::Position{111.0,114.0} &&
              std::string(production_view.world().courier_blockage(simulation::CourierId::Clay))==
                  "Waiting for road connection", "viewer courier did not wait at reached road");
        check(production_view.render(),"viewer waiting frame failed");
        const auto orange=sample_courier(simulation::CourierId::Clay,-5);
        check(orange[0]>220 && orange[1]>80 && orange[1]<180 && orange[2]<100,
              "waiting courier marker was not orange");
        v2_click(SDLK_1,112);
        production_view.tick_once();
        const auto resumed_position=production_view.world().courier_position(simulation::CourierId::Clay);
        check(resumed_position && resumed_position->x>111.0 && resumed_position->x<112.0 &&
              !production_view.world().courier(simulation::CourierId::Clay).route_pending,
              "viewer courier did not continue from saved waiting point");
        check(production_view.render(),"viewer resumed frame failed");
        const auto cyan=sample_courier(simulation::CourierId::Clay,-5);
        check(cyan[1]>200 && cyan[2]>200,"resumed courier marker did not return to moving color");
        const auto walker_manifest=walker_fixture(temp);
        production_view.set_walker_visuals(walker_manifest);
        check(production_view.walker_visuals_active() && production_view.walker_texture_count()==2,
              "walker textures were not shared by physical image");
        const auto profile=openemperor::assets::load_walker_visual_profile(temp.path,walker_manifest);
        bool saw_red=false,saw_green=false;
        for (int i=0;i<80 && !(saw_red && saw_green);++i) {
            production_view.tick_once();
            const auto& clay=production_view.world().courier(simulation::CourierId::Clay);
            const auto pose=openemperor::walker_pose(clay,production_view.world().ticks(),profile);
            if (!pose.moving || !pose.frame || !pose.direction ||
                (*pose.direction!=openemperor::assets::StorageDirection::PosX &&
                 *pose.direction!=openemperor::assets::StorageDirection::NegX)) continue;
            const auto clay_position=*production_view.world().courier_position(simulation::CourierId::Clay);
            const double clay_u=clay_position.x-72.0,clay_v=clay_position.y-72.0;
            const auto screen=production_view.camera().world_to_screen(
                {(clay_u-clay_v)*40,(clay_u+clay_v)*20+20});
            check(production_view.render(),"walker software frame failed");
            bool found_color=false;
            for (int yy=-4;yy<=4;++yy) for (int xx=-4;xx<=4;++xx) {
                const auto color=pixel(renderer,static_cast<int>(screen.x)+xx,
                                        static_cast<int>(screen.y)+yy);
                if (profile.find(openemperor::assets::WalkerVisualRole::Clay)->frames[*pose.frame].alias=="red")
                    found_color|=color[0]>220 && color[1]<80 && color[2]<80;
                else found_color|=color[1]>220 && color[0]<80 && color[2]<80;
            }
            if (profile.find(openemperor::assets::WalkerVisualRole::Clay)->frames[*pose.frame].alias=="red") {
                check(found_color,"red walker pixel missing");
                saw_red=true;
            } else {
                check(found_color,"green walker pixel missing");
                saw_green=true;
            }
        }
        check(saw_red && saw_green,"real courier path did not animate both synthetic frames");
        const auto before_toggle=production_view.world().snapshot();
        const auto paused_pose=openemperor::walker_pose(
            production_view.world().courier(simulation::CourierId::Clay),
            production_view.world().ticks(),profile);
        check(production_view.paused(),"viewer should remain paused during explicit tick test");
        production_view.update(1.0);
        check(production_view.render() && production_view.render() &&
              production_view.world().snapshot()==before_toggle &&
              openemperor::walker_pose(production_view.world().courier(simulation::CourierId::Clay),
                                       production_view.world().ticks(),profile).frame==paused_pose.frame,
              "paused or repeated rendering advanced walker animation");
        production_view.handle_event(key(SDLK_F2),running);
        check(!production_view.walker_visuals_active() &&
              production_view.world().snapshot()==before_toggle && production_view.render(),
              "F2 marker toggle changed simulation");
        production_view.handle_event(key(SDLK_F2),running);
        check(production_view.walker_visuals_active() &&
              production_view.world().snapshot()==before_toggle && production_view.render(),
              "F2 sprite toggle changed simulation");
        production_view.handle_event(key(SDLK_F3),running);
        check(production_view.render(),"walker diagnostic frame failed");
        const auto diagnostic_x=production_view.layout().map.x+8*production_view.layout().scale;
        const auto diagnostic_y=production_view.layout().map.y+8*production_view.layout().scale;
        const auto scale=production_view.layout().scale;
        const auto diagnostic_ground_x=diagnostic_x+
            std::min(470*scale,production_view.layout().map.w-16*scale)-105*scale;
        const auto diagnostic_pixel=[&] {
            return pixel(renderer,diagnostic_ground_x-2*scale,diagnostic_y+300*scale-2*scale);
        };
        const auto first_preview=diagnostic_pixel();
        check(first_preview[0]>220 && first_preview[1]<80,
              "4x diagnostic did not draw first physical frame");
        production_view.handle_event(key(SDLK_RIGHTBRACKET),running);
        check(production_view.render(),"diagnostic frame advance failed");
        const auto second_preview=diagnostic_pixel();
        check(second_preview[1]>220 && second_preview[0]<80 &&
              production_view.world().snapshot()==before_toggle,
              "visual-only frame advance changed World or missed next frame");
        production_view.handle_event(key(SDLK_B),running);
        check(production_view.render(),"light diagnostic background failed");
        const auto light_background=pixel(renderer,diagnostic_x+15*scale,diagnostic_y+150*scale);
        check(light_background[0]>200 && production_view.world().snapshot()==before_toggle,
              "diagnostic background switch changed World");
        production_view.handle_event(key(SDLK_X),running);
        production_view.handle_event(key(SDLK_BACKSLASH),running);
        check(production_view.render() && production_view.world().snapshot()==before_toggle,
              "diagnostic zoom or direction switch changed World");
        production_view.handle_event(key(SDLK_F3),running);
        const auto prior_textures=production_view.walker_texture_count();
        bool rejected=false;
        try { production_view.set_walker_visuals(temp.path/"missing-walker.json"); }
        catch (const std::exception&) { rejected=true; }
        check(rejected && production_view.walker_texture_count()==prior_textures &&
              production_view.world().snapshot()==before_toggle && production_view.render(),
              "failed walker activation destroyed running world or textures");
        production_view.set_walker_visuals(walker_manifest);
        check(production_view.walker_texture_count()==2 &&
              openemperor::WalkerSpriteSet::live_texture_count()==2,
              "repeated profile activation leaked textures");
        production_view.shutdown();
        check(openemperor::WalkerSpriteSet::live_texture_count()==0,
              "walker textures survived production session shutdown");
        openemperor::SandboxView fresh_view(fixture(temp,true),false,
            simulation::RulesProfile::ProductionV2);
        fresh_view.configure_save(temp.path,"Cities/Synthetic.map",save_path,
                                  openemperor::persistence::read_save(save_path));
        fresh_view.initialize(window,renderer);
        check(fresh_view.world().snapshot()==before_save && fresh_view.paused(),
              "startup load did not publish exact paused world");
        check(fresh_view.render() && fresh_view.last_courier_draws()==2,
              "startup loaded world did not render both couriers");
        { std::ofstream bad(save_path,std::ios::trunc); bad<<"{broken"; }
        const auto before_failed_load=fresh_view.world().snapshot();
        fresh_view.handle_event(key(SDLK_F9),running);
        check(fresh_view.world().snapshot()==before_failed_load &&
              fresh_view.last_message().find("parse error")!=std::string::npos,
              "failed F9 replaced world or concealed error");
        fresh_view.shutdown();
        openemperor::SandboxView household_view(fixture(temp,true,true),true,
            simulation::RulesProfile::HouseholdV3);
        household_view.configure_save(temp.path,"Cities/Synthetic.map",save_path);
        household_view.initialize(window,renderer);
        household_view.handle_event(key(SDLK_F1),running);
        check(household_view.debug_diagnostics(),"v3 diagnostics did not enable");
        check(household_view.demo_origin()==simulation::Cell{110,114} &&
              household_view.world().building(simulation::BuildingId::Household).placed &&
              household_view.tool()==5 && household_view.preview({119,114}).accepted==false,
              "v3 demo, household placement or select tool failed");
        household_view.handle_event(key(SDLK_7),running);
        check(household_view.tool()==7 &&
              !household_view.preview({119,114}).accepted &&
              std::string(household_view.preview({119,114}).reason)=="Cell already occupied",
              "v3 seventh tool did not use ordinary validation");
        household_view.handle_event(key(SDLK_5),running);
        for (int i=0;i<399;++i) household_view.tick_once();
        const auto before_need=household_view.world().snapshot();
        household_view.handle_event(key(SDLK_F5),running);
        check(household_view.last_message().find("Saved tick 399")!=std::string::npos,
              "v3 F5 did not save one tick before demand");
        household_view.handle_event(key(SDLK_SPACE),running);
        check(household_view.paused(),"v3 pause did not stop clock");
        household_view.update(1.0);
        check(household_view.world().snapshot()==before_need,
              "paused v3 simulation advanced demand");
        household_view.handle_event(key(SDLK_PERIOD),running);
        check(household_view.world().ticks()==400 &&
              household_view.world().building(simulation::BuildingId::Household).missed_demand+
              household_view.world().building(simulation::BuildingId::Household).fulfilled_demand==1,
              "v3 single-step did not process one demand tick");
        household_view.handle_event(key(SDLK_F9),running);
        check(household_view.paused() && household_view.world().snapshot()==before_need,
              "v3 F9 did not restore tick-399 state transactionally");
        for (int i=0;i<401;++i) household_view.tick_once();
        check(household_view.render() && household_view.last_courier_draws()==3 &&
              household_view.world().building(simulation::BuildingId::Household).consumed_total>0,
              "v3 software frame did not render three couriers and real consumption");
        bool third_edge=false;
        for (int i=0;i<2000 && !third_edge;++i) {
            const auto& c=household_view.world().courier(simulation::CourierId::Household);
            third_edge=c.phase==simulation::CourierPhase::ToWarehouse && c.edge_progress>0 &&
                c.path[c.path_vertex]==simulation::Cell{116,114};
            if (!third_edge) household_view.tick_once();
        }
        check(third_edge,"v3 viewer did not reach household supplier edge");
        household_view.handle_event(key(SDLK_6),running);
        check(!household_view.preview({117,114}).accepted &&
              household_view.preview({118,114}).accepted,
              "v3 removal preview ignored third courier's protected edge");
        check(household_view.execute({simulation::CommandType::RemoveRoad,{118,114}}).accepted,
              "v3 viewer could not remove future household road");
        bool third_waiting=false;
        for (int i=0;i<40 && !third_waiting;++i) {
            household_view.tick_once();
            const auto& c=household_view.world().courier(simulation::CourierId::Household);
            third_waiting=c.route_pending && c.edge_progress==0;
        }
        check(third_waiting && household_view.render(),"v3 supplier did not wait or render");
        const auto supplier_position=*household_view.world().courier_position(
            simulation::CourierId::Household);
        const auto supplier_u=supplier_position.x-72.0,supplier_v=supplier_position.y-72.0;
        const auto supplier_screen=household_view.camera().world_to_screen(
            {(supplier_u-supplier_v)*40.0,(supplier_u+supplier_v)*20.0+20.0});
        const auto waiting_pixel=pixel(renderer,static_cast<int>(supplier_screen.x),
                                       static_cast<int>(supplier_screen.y));
        check(waiting_pixel[0]>220 && waiting_pixel[1]>80 && waiting_pixel[1]<180 &&
              waiting_pixel[2]<100,"v3 waiting courier lacked orange marker");
        check(household_view.execute({simulation::CommandType::PlaceRoad,{118,114}}).accepted,
              "v3 viewer could not repair household road");
        household_view.tick_once();
        check(!household_view.world().courier(simulation::CourierId::Household).route_pending &&
              household_view.render(),"v3 supplier did not resume after repair");
        household_view.shutdown();
        openemperor::SandboxView industry_view(fixture(temp,true,true,true),true,
            simulation::RulesProfile::IndustryV5);
        industry_view.configure_save(temp.path,"Cities/Synthetic.map",save_path);
        industry_view.set_walker_visuals(multi_role_walker_fixture(temp));
        const auto building_manifest=building_fixture(temp);
        const auto road_manifest=road_fixture(temp);
        industry_view.set_building_visuals(building_manifest);
        industry_view.set_road_visuals(road_manifest);
        industry_view.initialize(window,renderer);
        const auto help_baseline=industry_view.world().snapshot();
        industry_view.handle_event(key(SDLK_H),running);
        check(industry_view.help_open() && industry_view.render() &&
              industry_view.world().snapshot()==help_baseline,
              "opening and rendering Help changed the Industry World");
        industry_view.handle_event(key(SDLK_H),running);
        check(!industry_view.help_open() && industry_view.world().snapshot()==help_baseline,
              "closing Help changed the Industry World");
        check(industry_view.walker_texture_count()==3,"industry role textures missing or duplicated");
        check(industry_view.building_texture_count()==4 &&
              openemperor::BuildingSprite::live_texture_count()==4,
              "four roles did not share four textures");
        openemperor::SandboxView marker_control(fixture(temp,true,true,true),true,
            simulation::RulesProfile::IndustryV5);
        marker_control.initialize(window,renderer);
        check(industry_view.world().building(static_cast<simulation::BuildingId>(8)).kind==
                  simulation::Object::ClaySource &&
              industry_view.world().building(static_cast<simulation::BuildingId>(9)).kind==
                  simulation::Object::Pottery && industry_view.tool()==5,
              "v5 viewer did not place two production instances");
        for (int i=0;i<700;++i) {
            industry_view.tick_once();marker_control.tick_once();
            if (i%25==0) {
                industry_view.handle_event(key(SDLK_F2),running);
                industry_view.handle_event(key(SDLK_F4),running);
                industry_view.handle_event(key(SDLK_F6),running);
                check(industry_view.render() && industry_view.render() && marker_control.render(),
                      "frequent sprite/marker/building render failed");
            }
            check(industry_view.world().snapshot()==marker_control.world().snapshot(),
                  "visual preview or F2/F4/F6 changed an authoritative Industry World tick");
        }
        if (!industry_view.walker_visuals_active()) industry_view.handle_event(key(SDLK_F2),running);
        if (!industry_view.building_visuals_active()) industry_view.handle_event(key(SDLK_F4),running);
        if (!industry_view.road_visuals_active()) industry_view.handle_event(key(SDLK_F6),running);
        check(industry_view.render() && industry_view.last_courier_draws()==5 &&
              industry_view.world().building(static_cast<simulation::BuildingId>(9)).recipes_completed>0,
              "v5 software frame did not draw five couriers and active second pottery");
        std::array<bool,3> role_pixels{};
        for (int step=0;step<300 && !std::all_of(role_pixels.begin(),role_pixels.end(),
                [](bool seen){return seen;});++step) {
            industry_view.tick_once();marker_control.tick_once();
            check(industry_view.world().snapshot()==marker_control.world().snapshot() &&
                  industry_view.render(),"multi-role render changed Industry World");
            for (int id=1;id<=5;++id) {
                const auto courier_id=static_cast<simulation::CourierId>(id);
                const auto& courier=industry_view.world().courier(courier_id);
                const auto visual_role=openemperor::walker_visual_role(courier.role);
                if (!visual_role) continue;
                const auto courier_position=industry_view.world().courier_position(courier_id);
                if (!courier_position) continue;
                const double grid_u=courier_position->x-72,grid_v=courier_position->y-72;
                const auto ground=industry_view.camera().world_to_screen(
                    {(grid_u-grid_v)*40,(grid_u+grid_v)*20+20});
                int render_width=0,render_height=0;
                check(SDL_GetRenderOutputSize(renderer,&render_width,&render_height),
                      "renderer output size");
                if (ground.x<1 || ground.y<1 || ground.x>=render_width-1 ||
                    ground.y>=render_height-1) continue;
                const auto sample=pixel(renderer,static_cast<int>(ground.x),
                                        static_cast<int>(ground.y));
                const auto role_index=openemperor::assets::walker_role_index(*visual_role);
                role_pixels[role_index]=role_pixels[role_index] ||
                    (role_index==0 ? sample[1]>220 && sample[2]>220 && sample[0]<80:
                     role_index==1 ? sample[0]>220 && sample[2]>220 && sample[1]<80:
                                     sample[1]>220 && sample[0]<80 && sample[2]<80);
            }
        }
        check(std::all_of(role_pixels.begin(),role_pixels.end(),[](bool seen){return seen;}),
              "one CourierRole lacked its synthetic color in SDL pixels");
        const auto walker_roles=industry_view.walker_display_stats();
        check(walker_roles.schema_version==2 && walker_roles.decoded_assets==3 &&
              walker_roles.texture_uploads==3 && walker_roles.roles[0].draws>0 &&
              walker_roles.roles[1].draws>0 && walker_roles.roles[2].draws>0,
              "five Industry couriers did not share three role textures");
        const auto building_stats=industry_view.building_display_stats();
        check(building_stats.configured && building_stats.decoded_assets==4 &&
              building_stats.texture_uploads==4 &&
              building_stats.drawn_instances[openemperor::assets::role_index(
                  openemperor::assets::BuildingVisualRole::Pottery)]>=2,
              "both Pottery instances not drawn with one texture");
        const auto& draws=building_stats.drawn_instances;
        check(draws[0]>=2 && draws[1]>=2 && draws[2]>=1 && draws[3]>=4,
              "all Industry-v5 roles were not drawn through the shared path");
        check(openemperor::building_visual_role(industry_view.world().building(
                  static_cast<simulation::BuildingId>(8)).kind)==
                  openemperor::assets::BuildingVisualRole::ClaySource &&
              openemperor::building_visual_role(industry_view.world().building(
                  static_cast<simulation::BuildingId>(9)).kind)==
                  openemperor::assets::BuildingVisualRole::Pottery &&
              !openemperor::building_visual_role(simulation::Object::Workshop) &&
              !openemperor::building_visual_role(simulation::Object::Road),
              "second producer mapped from ID rather than Object kind");
        std::optional<simulation::Cell> fourth_house;
        for (int y=0;y<industry_view.world().height() && !fourth_house;++y)
            for (int x=0;x<industry_view.world().width();++x)
                if (industry_view.world().validate({simulation::CommandType::PlaceHousehold,{x,y}}).accepted) {
                    fourth_house=simulation::Cell{x,y};break;
                }
        check(fourth_house.has_value(),"no valid fourth-house test cell");
        check(industry_view.execute({simulation::CommandType::PlaceHousehold,*fourth_house}).accepted &&
              marker_control.execute({simulation::CommandType::PlaceHousehold,*fourth_house}).accepted &&
              industry_view.world().snapshot()==marker_control.world().snapshot() &&
              industry_view.render() && industry_view.building_texture_count()==4 &&
              industry_view.building_display_stats().drawn_instances[3]>=4,
              "fourth Household uploaded another texture or altered World");
        const auto before_building_toggle=industry_view.world().snapshot();
        const auto before_dirty=industry_view.dirty();
        industry_view.handle_event(key(SDLK_F4),running);
        check(!industry_view.building_visuals_active() && industry_view.render() &&
              industry_view.world().snapshot()==before_building_toggle &&
              industry_view.dirty()==before_dirty,"F4 changed authoritative World or dirty state");
        industry_view.handle_event(key(SDLK_F4),running);
        check(industry_view.building_visuals_active() && industry_view.render(),
              "F4 did not restore selected building visual");
        bool bad_building_rejected=false;
        try { industry_view.set_building_visuals(temp.path/"missing-building.json"); }
        catch (const std::exception&) { bad_building_rejected=true; }
        check(bad_building_rejected && industry_view.building_visuals_active() &&
              industry_view.building_texture_count()==4 && industry_view.render(),
              "failed building profile replaced working texture");
        industry_view.set_building_visuals(building_manifest);
        check(openemperor::BuildingSprite::live_texture_count()==4,
              "building profile reload leaked texture");
        industry_view.save_now();
        const auto saved_building_world=industry_view.world().snapshot();
        for (int i=0;i<7;++i) industry_view.tick_once();
        industry_view.load_now();
        check(industry_view.world().snapshot()==saved_building_world &&
              industry_view.building_visuals_active() && industry_view.render(),
              "building preview altered save/load World or lost session visual");
        check(industry_view.unified_depth() &&
              industry_view.painter_stats().stored_items_visited>0 &&
              industry_view.painter_stats().sandbox_items>0 &&
              industry_view.painter_stats().stored_order_builds==1,
              "unified view did not merge static map and dynamic sandbox items");
        for (int i=0;i<2001;++i) {
            if (i%200==0) {
                const auto snapshot=industry_view.world().snapshot();
                const auto dirty=industry_view.dirty();
                industry_view.handle_event(key(SDLK_F7),running);
                check(industry_view.world().snapshot()==snapshot &&
                      industry_view.dirty()==dirty && industry_view.render(),
                      "F7 changed authoritative state or failed to render");
                if (industry_view.unified_depth())
                    check(industry_view.painter_stats().stored_items_visited>0 &&
                          industry_view.painter_stats().stored_order_builds==1,
                          "unified painter rebuilt static order");
                else check(industry_view.painter_stats().stored_items_visited==0,
                           "legacy painter visited merged items");
            }
            industry_view.tick_once();marker_control.tick_once();
            check(industry_view.world().snapshot()==marker_control.world().snapshot(),
                  "legacy/unified painter changed Industry World during 2001 ticks");
        }
        industry_view.handle_event(key(SDLK_F7),running);
        check(industry_view.unified_depth() && industry_view.render(),
              "F7 did not restore unified default after regression run");
        industry_view.handle_event(key(SDLK_2),running);
        check(industry_view.tool()==5,
              "v5 full Clay tool remained selectable");
        if (alpha_stress) {
            const auto stable_world=industry_view.world().snapshot();
            const auto walker_before=industry_view.walker_display_stats();
            const auto building_before=industry_view.building_display_stats();
            const auto road_before=industry_view.road_display_stats();
            const auto order_before=industry_view.painter_stats().stored_order_builds;
            struct HiddenFiles {
                std::vector<std::pair<std::filesystem::path,std::filesystem::path>> paths;
                void restore() {
                    for (auto it=paths.rbegin();it!=paths.rend();++it)
                        std::filesystem::rename(it->second,it->first);
                    paths.clear();
                }
                ~HiddenFiles() {
                    for (auto it=paths.rbegin();it!=paths.rend();++it) {
                        std::error_code error;
                        std::filesystem::rename(it->second,it->first,error);
                    }
                }
            } hidden;
            std::vector<std::filesystem::path> loaded_files;
            for (const auto& entry:std::filesystem::recursive_directory_iterator(temp.path))
                if (entry.is_regular_file() && (entry.path().extension()==".json" ||
                    entry.path().extension()==".map" || entry.path().extension()==".sg3" ||
                    entry.path().extension()==".555")) loaded_files.push_back(entry.path());
            for (const auto& original:loaded_files) {
                auto unavailable=original;unavailable += ".alpha-hidden";
                std::filesystem::rename(original,unavailable);
                hidden.paths.emplace_back(original,unavailable);
            }
            for (int frame=0;frame<3000;++frame) {
                if (frame%100==0) {
                    industry_view.handle_event(key(SDLK_F2),running);
                    industry_view.handle_event(key(SDLK_F4),running);
                    industry_view.handle_event(key(SDLK_F6),running);
                    industry_view.handle_event(key(SDLK_F7),running);
                    industry_view.handle_event(key(SDLK_F3),running);
                    industry_view.handle_event(key(SDLK_V),running);
                    SDL_Event stress_wheel{};stress_wheel.type=SDL_EVENT_MOUSE_WHEEL;
                    stress_wheel.wheel.mouse_x=400;stress_wheel.wheel.mouse_y=300;
                    stress_wheel.wheel.y=(frame/100)%2==0 ? 1.0F:-1.0F;
                    industry_view.handle_event(stress_wheel,running);
                    industry_view.handle_event(motion(350.0F+static_cast<float>(frame%200),300.0F),running);
                }
                check(industry_view.render(),"alpha pure render frame failed");
            }
            hidden.restore();
            const auto walker_after=industry_view.walker_display_stats();
            const auto building_after=industry_view.building_display_stats();
            const auto road_after=industry_view.road_display_stats();
            check(industry_view.world().snapshot()==stable_world,
                  "3000 pure render frames changed Industry World");
            check(walker_after.decoded_assets==walker_before.decoded_assets &&
                  walker_after.texture_uploads==walker_before.texture_uploads &&
                  building_after.decoded_assets==building_before.decoded_assets &&
                  building_after.texture_uploads==building_before.texture_uploads &&
                  road_after.unique_assets==road_before.unique_assets &&
                  road_after.texture_uploads==road_before.texture_uploads,
                  "pure rendering decoded or uploaded visual assets");
            check(industry_view.painter_stats().stored_order_builds==order_before,
                  "pure rendering rebuilt stored draw order");
            std::cout<<"alpha render stress: 3000 frames, zero decode/upload/order rebuilds\n";
        }
        industry_view.shutdown();
        marker_control.shutdown();
        check(openemperor::WalkerSpriteSet::live_texture_count()==0,
              "walker textures survived industry session shutdown");
        check(openemperor::BuildingSprite::live_texture_count()==0,
              "building texture survived industry session shutdown");
        check(openemperor::RoadSpriteSet::live_texture_count()==0,
              "road texture survived industry session shutdown");

        check(SDL_SetWindowSize(window,1100,700),"resize end-to-end window");
        openemperor::SandboxView ui(fixture(temp,true,true,true),false,
            simulation::RulesProfile::IndustryV5);
        ui.configure_save(temp.path,"Cities/Synthetic.map",save_path);
        ui.set_building_visuals(building_manifest);
        ui.set_road_visuals(road_manifest);
        ui.initialize(window,renderer);
        check(ui.road_display_stats().texture_uploads==16,"road set upload");
        check(ui.layout().map.w==796 && ui.layout().map.h==516,"end-to-end viewport");
        const auto button_point=[&](openemperor::sandbox_ui::Action action) {
            for (const auto& button:ui.layout().buttons) if (button.action==action)
                return openemperor::scene::Point{button.rect.x+button.rect.w/2.0,
                                                  button.rect.y+button.rect.h/2.0};
            throw std::runtime_error("missing UI button");
        };
        const auto press_action=[&](openemperor::sandbox_ui::Action action) {
            const auto p=button_point(action);
            mouse_click(ui,static_cast<float>(p.x),static_cast<float>(p.y),running);
        };
        const auto map_point=[&](int x,int y) {
            auto p=maps::terrain_world({static_cast<std::uint32_t>(x),
                                        static_cast<std::uint32_t>(y)},72);
            p.y+=20;
            return ui.camera().world_to_screen(p);
        };
        const auto map_click=[&](int x,int y) {
            const auto p=map_point(x,y);
            check(ui.pick(p)==simulation::Cell{x,y},"end-to-end map point");
            mouse_click(ui,static_cast<float>(p.x),static_cast<float>(p.y),running);
        };
        const auto drag=[&](int x0,int y0,int x1,int y1) {
            const auto start=map_point(x0,y0),end=map_point(x1,y1);
            ui.handle_event(click(static_cast<float>(start.x),static_cast<float>(start.y)),running);
            ui.handle_event(motion(static_cast<float>(end.x),static_cast<float>(end.y)),running);
            ui.handle_event(release(static_cast<float>(end.x),static_cast<float>(end.y)),running);
        };
        press_action(openemperor::sandbox_ui::Action::Clay);
        const auto clay_hover=map_point(110,114);
        const auto before_hover=ui.world().snapshot();
        ui.handle_event(motion(static_cast<float>(clay_hover.x),
                               static_cast<float>(clay_hover.y)),running);
        check(ui.hovered_cell()==simulation::Cell{110,114} &&
              ui.preview({110,114}).accepted && ui.render() &&
              ui.world().snapshot()==before_hover && ui.building_texture_count()==4,
              "valid original building hover changed World or uploaded texture");
        map_click(110,114);
        const auto after_clay=ui.world().snapshot();
        ui.handle_event(motion(static_cast<float>(clay_hover.x),
                               static_cast<float>(clay_hover.y)),running);
        check(!ui.preview({110,114}).accepted && ui.render() &&
              ui.world().snapshot()==after_clay,
              "invalid building hover changed World");
        press_action(openemperor::sandbox_ui::Action::Pottery); map_click(113,114);
        press_action(openemperor::sandbox_ui::Action::Road);
        const auto invalid_road_hover=map_point(121,114);
        ui.handle_event(motion(static_cast<float>(invalid_road_hover.x),
                               static_cast<float>(invalid_road_hover.y)),running);
        check(!ui.preview({121,114}).accepted && ui.render(),
              "invalid road hover was not rendered");
        const auto invalid_hover_pixel=pixel(renderer,static_cast<int>(invalid_road_hover.x),
                                                       static_cast<int>(invalid_road_hover.y));
        check(invalid_hover_pixel[0]==255 && invalid_hover_pixel[1]==65 &&
              invalid_hover_pixel[2]==65,
              "invalid road hover did not produce the red diagnostic diamond");
        press_action(openemperor::sandbox_ui::Action::Select);
        check(ui.render() && pixel(renderer,static_cast<int>(invalid_road_hover.x),
                                  static_cast<int>(invalid_road_hover.y))!=invalid_hover_pixel,
              "red diagnostic diamond survived switching to the Select tool");
        press_action(openemperor::sandbox_ui::Action::Road);
        const auto first_start=map_point(111,114),first_end=map_point(112,114);
        const auto before_drag=ui.world().snapshot();
        ui.handle_event(click(static_cast<float>(first_start.x),static_cast<float>(first_start.y)),running);
        ui.handle_event(motion(static_cast<float>(first_end.x),static_cast<float>(first_end.y)),running);
        ui.update(0.0);
        check(ui.world().snapshot()==before_drag && ui.road_preview().cells.size()==2,
              "road drag mutated world before release");
        const auto plan_builds=ui.road_plan_build_count();
        for (int i=0;i<500;++i)
            ui.handle_event(motion(static_cast<float>(first_end.x),
                                   static_cast<float>(first_end.y)),running);
        check(ui.road_plan_build_count()==plan_builds,
              "mouse-motion burst rebuilt a RoadPlan inside event handling");
        ui.update(0.0);
        check(ui.road_plan_build_count()==plan_builds,
              "unchanged start/end cell rebuilt the cached RoadPlan");
        check(ui.render() && ui.world().snapshot()==before_drag,
              "rendered held road changed simulation");
        check(ui.road_display_stats().draws==0 &&
              openemperor::sandbox_ui::road_neighbor_mask_for_preview(ui.world(),ui.road_preview().cells,
                                                             {111,114})==0x2 &&
              openemperor::sandbox_ui::entrance_mask_for_preview(
                  ui.world(),ui.road_preview().cells,{111,114})==0x8,
              "drag preview did not separate hypothetical roads from building entrances");
        ui.handle_event(release(static_cast<float>(first_end.x),static_cast<float>(first_end.y)),running);
        check(ui.world().command_sequence()==before_drag.command_sequence+2 &&
              ui.world().object_at({111,114})==simulation::Object::Road &&
              ui.world().object_at({112,114})==simulation::Object::Road,
              "road drag did not commit exactly two commands");
        check(ui.render() && ui.road_display_stats().draws>=2 &&
              ui.road_display_stats().masks_seen[0x2],"committed road visual did not render");
        const auto road_center=map_point(111,114);
        const auto road_pixel=pixel(renderer,static_cast<int>(road_center.x),
                                            static_cast<int>(road_center.y));
        const auto before_road_toggle=ui.world().snapshot();
        const auto road_dirty=ui.dirty();
        ui.handle_event(key(SDLK_F6),running);
        check(!ui.road_visuals_active() && ui.render() &&
              ui.world().snapshot()==before_road_toggle && ui.dirty()==road_dirty &&
              pixel(renderer,static_cast<int>(road_center.x),static_cast<int>(road_center.y))!=road_pixel,
              "F6 did not change only road pixels");
        ui.handle_event(key(SDLK_F6),running);
        check(ui.road_visuals_active() && ui.render(),"F6 did not restore road tiles");
        drag(114,114,115,114);
        press_action(openemperor::sandbox_ui::Action::Warehouse); map_click(116,114);
        press_action(openemperor::sandbox_ui::Action::Road); drag(117,114,119,114);
        press_action(openemperor::sandbox_ui::Action::Household); map_click(120,114);
        check(ui.world().building(simulation::BuildingId::Household).placed,
              "house UI placement");
        const auto before_disabled=ui.world().snapshot();
        press_action(openemperor::sandbox_ui::Action::Warehouse);
        check(ui.world().snapshot()==before_disabled &&
              ui.last_message().find("limit")!=std::string::npos,
              "disabled building button changed world");
        const auto before_rejected=ui.world().snapshot();
        press_action(openemperor::sandbox_ui::Action::Road);
        drag(111,114,115,114); // The Pottery at 113 is an obstacle.
        check(ui.world().snapshot()==before_rejected &&
              ui.last_message().find("occupied")!=std::string::npos,
              "obstructed road drag partially changed world");
        const auto start_cancel=map_point(117,115);
        const auto end_cancel=map_point(119,115);
        ui.handle_event(click(static_cast<float>(start_cancel.x),
                              static_cast<float>(start_cancel.y)),running);
        ui.handle_event(motion(static_cast<float>(end_cancel.x),
                               static_cast<float>(end_cancel.y)),running);
        check(ui.world().snapshot()==before_rejected,"held road changed world");
        SDL_Event focus{}; focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;
        ui.handle_event(focus,running);
        ui.handle_event(release(static_cast<float>(end_cancel.x),
                                static_cast<float>(end_cancel.y)),running);
        check(ui.world().snapshot()==before_rejected,"focus loss committed road");
        const auto button=button_point(openemperor::sandbox_ui::Action::Clay);
        ui.handle_event(click(static_cast<float>(button.x),static_cast<float>(button.y)),running);
        ui.handle_event(release(static_cast<float>(end_cancel.x),
                                static_cast<float>(end_cancel.y)),running);
        check(ui.tool()==1 && ui.world().snapshot()==before_rejected,
              "button press/map release leaked action");
        ui.handle_event(click(static_cast<float>(start_cancel.x),
                              static_cast<float>(start_cancel.y)),running);
        ui.handle_event(release(static_cast<float>(button.x),
                                static_cast<float>(button.y)),running);
        check(ui.world().snapshot()==before_rejected,"map press/UI release committed road");
        ui.handle_event(click(static_cast<float>(start_cancel.x),
                              static_cast<float>(start_cancel.y)),running);
        SDL_Event right{}; right.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
        right.button.button=SDL_BUTTON_RIGHT;
        ui.handle_event(right,running);
        ui.handle_event(release(static_cast<float>(end_cancel.x),
                                static_cast<float>(end_cancel.y)),running);
        check(ui.world().snapshot()==before_rejected,"right-click did not cancel road");
        ui.handle_event(click(static_cast<float>(start_cancel.x),
                              static_cast<float>(start_cancel.y)),running);
        ui.handle_event(key(SDLK_ESCAPE),running);
        ui.handle_event(release(static_cast<float>(end_cancel.x),
                                static_cast<float>(end_cancel.y)),running);
        check(running && ui.world().snapshot()==before_rejected,
              "Escape did not cancel road before exit");
        ui.handle_event(click(static_cast<float>(start_cancel.x),
                              static_cast<float>(start_cancel.y)),running);
        SDL_Event resize{}; resize.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
        ui.handle_event(resize,running);
        ui.handle_event(release(static_cast<float>(end_cancel.x),
                                static_cast<float>(end_cancel.y)),running);
        check(ui.world().snapshot()==before_rejected,"resize did not cancel road");
        ui.handle_event(click(static_cast<float>(start_cancel.x),
                              static_cast<float>(start_cancel.y)),running);
        ui.handle_event(key(SDLK_5),running);
        ui.handle_event(release(static_cast<float>(end_cancel.x),
                                static_cast<float>(end_cancel.y)),running);
        check(ui.tool()==5 && ui.world().snapshot()==before_rejected,
              "tool switch did not cancel road");
        // Additional producers use ordinary Commands; their UI selection remains instance-specific.
        check(ui.execute({simulation::CommandType::PlaceClaySource,{110,116}}).accepted &&
              ui.execute({simulation::CommandType::PlacePottery,{113,116}}).accepted,
              "second branch normal commands");
        press_action(openemperor::sandbox_ui::Action::Road);
        drag(111,116,112,116); drag(112,115,112,115);
        drag(114,116,116,116); drag(116,115,116,115);
        for (int i=0;i<2500;++i) ui.tick_once();
        check(ui.world().building(simulation::BuildingId::Pottery).recipes_completed>0 &&
              ui.world().building(static_cast<simulation::BuildingId>(9)).recipes_completed>0 &&
              ui.world().building(simulation::BuildingId::Household).consumed_total>0,
              "UI-built chain did not produce, deliver and consume");
        const auto capture=[&](const char* name) {
            if (const char* folder=std::getenv("OPENEMPEROR_UI_CAPTURE_DIR")) {
                std::filesystem::create_directories(folder);
                SDL_Surface* image=SDL_RenderReadPixels(renderer,nullptr);
                check(image!=nullptr,"capture UI frame");
                const auto path=std::filesystem::path(folder)/name;
                const bool saved=SDL_SaveBMP(image,path.string().c_str());
                SDL_DestroySurface(image);
                check(saved,"write UI capture");
            }
        };
        check(ui.render(),"UI overview frame");
        capture("overview.bmp");
        press_action(openemperor::sandbox_ui::Action::Select); map_click(113,116);
        check(ui.selected_building()==static_cast<simulation::BuildingId>(9),
              "second Pottery selection resolved first instance");
        const auto details=ui.inspection_lines();
        check(std::find(details.begin(),details.end(),"Pottery #9")!=details.end(),
              "second Pottery inspection missing own ID");
        check(ui.render(),"UI inspector frame");
        capture("inspector.bmp");
        const auto panel_x=ui.layout().panel.x+20;
        const auto source_row_y=ui.layout().panel.y+46+4*18+4;
        mouse_click(ui,static_cast<float>(panel_x),static_cast<float>(source_row_y),running);
        check(ui.selected_building()==static_cast<simulation::BuildingId>(8),
              "building list did not select second Clay instance");
        map_click(113,116);
        const auto zoom_before_panel=ui.camera().zoom;
        SDL_Event panel_wheel{}; panel_wheel.type=SDL_EVENT_MOUSE_WHEEL;
        panel_wheel.wheel.mouse_x=static_cast<float>(panel_x);
        panel_wheel.wheel.mouse_y=static_cast<float>(source_row_y);
        panel_wheel.wheel.y=-1;
        ui.handle_event(panel_wheel,running);
        check(ui.camera().zoom==zoom_before_panel,"panel wheel zoomed map");
        const auto stable=ui.world().snapshot();
        for (int i=0;i<10;++i) {
            ui.handle_event(motion(static_cast<float>(map_point(113,116).x),
                                   static_cast<float>(map_point(113,116).y)),running);
            check(ui.render(),"UI interaction frame");
            (void)ui.inspection_lines();
        }
        check(ui.world().snapshot()==stable,"hover/panel/render changed simulation");
        press_action(openemperor::sandbox_ui::Action::Pause);
        check(ui.paused(),"pause button");
        const auto tick_before=ui.world().ticks();
        press_action(openemperor::sandbox_ui::Action::Step);
        check(ui.world().ticks()==tick_before+1 && ui.paused(),"step button");
        press_action(openemperor::sandbox_ui::Action::Pause);
        check(!ui.paused(),"continue button");
        const auto saved=ui.world().snapshot();
        press_action(openemperor::sandbox_ui::Action::Save);
        press_action(openemperor::sandbox_ui::Action::Road);
        const auto future=map_point(117,115);
        ui.handle_event(click(static_cast<float>(future.x),static_cast<float>(future.y)),running);
        ui.handle_event(key(SDLK_F5),running);
        ui.handle_event(release(static_cast<float>(future.x),static_cast<float>(future.y)),running);
        check(ui.world().snapshot()==saved,"F5 committed unfinished road");
        ui.tick_once();
        press_action(openemperor::sandbox_ui::Action::Load);
        check(ui.world().snapshot()==saved && ui.paused() &&
              ui.selected_building()==static_cast<simulation::BuildingId>(9),
              "button save/load or instance selection");
        check(ui.render(),"saved UI software frame");
        press_action(openemperor::sandbox_ui::Action::Road);
        const auto l_start=map_point(117,112),l_end=map_point(119,113);
        const auto before_l=ui.world().snapshot();
        ui.handle_event(click(static_cast<float>(l_start.x),static_cast<float>(l_start.y)),running);
        ui.handle_event(motion(static_cast<float>(l_end.x),static_cast<float>(l_end.y)),running);
        ui.update(0.0);
        check(ui.road_preview().valid && ui.road_preview().cells.size()==4 &&
              openemperor::sandbox_ui::road_neighbor_mask_for_preview(ui.world(),ui.road_preview().cells,
                                                             {119,112})==0xc &&
              ui.world().snapshot()==before_l && ui.render(),
              "L drag preview missing future corner or changed World");
        const auto corner_screen=map_point(119,112);
        const auto preview_corner_pixel=pixel(renderer,static_cast<int>(corner_screen.x),
                                               static_cast<int>(corner_screen.y));
        ui.handle_event(release(static_cast<float>(l_end.x),static_cast<float>(l_end.y)),running);
        check(ui.world().command_sequence()==before_l.command_sequence+4 &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{119,112})==0xc &&
              ui.render() &&
              pixel(renderer,static_cast<int>(corner_screen.x),static_cast<int>(corner_screen.y))!=
                  preview_corner_pixel,
              "L drag corner did not commit its opaque road pixel");
        const auto committed_corner_pixel=pixel(renderer,static_cast<int>(corner_screen.x),
                                                 static_cast<int>(corner_screen.y));
        check(ui.execute({simulation::CommandType::PlaceRoad,{120,112}}).accepted &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{119,112})==0xe &&
              ui.render() &&
              pixel(renderer,static_cast<int>(corner_screen.x),static_cast<int>(corner_screen.y))!=
                  committed_corner_pixel,"new arm did not change live road pixel to T");
        check(ui.execute({simulation::CommandType::RemoveRoad,{120,112}}).accepted &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{119,112})==0xc &&
              ui.render() &&
              pixel(renderer,static_cast<int>(corner_screen.x),static_cast<int>(corner_screen.y))==
                  committed_corner_pixel,"road removal did not restore corner pixel");
        const auto junction=map_point(118,113);
        const auto junction_pixel=[&]{return pixel(renderer,static_cast<int>(junction.x),
                                                    static_cast<int>(junction.y));};
        check(ui.execute({simulation::CommandType::PlaceRoad,{118,113}}).accepted &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{118,113})==0x7 &&
              ui.render(),"new road did not form T");
        const auto t_pixel=junction_pixel();
        check(ui.execute({simulation::CommandType::PlaceRoad,{117,113}}).accepted &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{118,113})==0xf &&
              ui.render() && junction_pixel()!=t_pixel,
              "added arm did not render a crossing pixel");
        check(ui.execute({simulation::CommandType::RemoveRoad,{117,113}}).accepted &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{118,113})==0x7 &&
              ui.render() && junction_pixel()==t_pixel,
              "removed arm did not restore T pixel");
        check(ui.execute({simulation::CommandType::RemoveRoad,{119,113}}).accepted &&
              openemperor::sandbox_ui::road_neighbor_mask(ui.world(),{118,113})==0x5 &&
              ui.render() && junction_pixel()!=t_pixel,
              "second removal did not render straight pixel");
        check(ui.execute({simulation::CommandType::PlaceRoad,{118,113}}).accepted,
              "road selection no-op failed");
        const auto road_details=ui.inspection_lines();
        check(std::find(road_details.begin(),road_details.end(),"Road mask 0x5")!=road_details.end() &&
              std::find(road_details.begin(),road_details.end(),"Road asset configured yes")!=
                  road_details.end() &&
              std::find(road_details.begin(),road_details.end(),"Entrances")!=road_details.end(),
              "selected road F1 diagnostics missing mask, entrances, or asset state");
        const auto before_bad_profile=ui.road_display_stats().texture_uploads;
        bool bad_road_rejected=false;
        try { ui.set_road_visuals(temp.path/"missing-roads.json"); }
        catch (const std::exception&) { bad_road_rejected=true; }
        check(bad_road_rejected && ui.road_visuals_active() &&
              ui.road_display_stats().texture_uploads==before_bad_profile,
              "invalid road profile partially replaced active sprites");
        const auto previous_zoom=ui.camera().zoom;
        const auto at=map_point(113,116);
        ui.handle_event(motion(static_cast<float>(at.x),static_cast<float>(at.y)),running);
        SDL_Event extreme{}; extreme.type=SDL_EVENT_MOUSE_WHEEL;
        extreme.wheel.mouse_x=static_cast<float>(at.x);
        extreme.wheel.mouse_y=static_cast<float>(at.y);
        extreme.wheel.y=100;
        const auto anchored=ui.camera().screen_to_world(at);
        ui.handle_event(extreme,running);
        const auto after=ui.camera().screen_to_world(at);
        check(ui.camera().zoom==4 && std::abs(anchored.x-after.x)<1e-5 &&
              std::abs(anchored.y-after.y)<1e-5 && previous_zoom<=4,
              "zoom max changed pointer anchor");
        extreme.wheel.y=-100;
        ui.handle_event(extreme,running);
        check(ui.camera().zoom==0.5,"zoom minimum clamp");
        ui.handle_event(key(SDLK_R),running);
        check(ui.hovered_cell()==ui.pick(at),"hover not recomputed after camera reset");
        const auto panel_button=button_point(openemperor::sandbox_ui::Action::TogglePanel);
        mouse_click(ui,static_cast<float>(panel_button.x),
                    static_cast<float>(panel_button.y),running);
        check(!ui.layout().panel_open && ui.layout().map.w==1100 &&
              !ui.layout().ui_at(900,200),"collapsed panel retained hidden hit area");
        check(ui.render(),"collapsed panel frame");
        SDL_Rect viewport{},clip{};
        float render_scale_x=0,render_scale_y=0;
        check(SDL_GetRenderViewport(renderer,&viewport) &&
              SDL_GetRenderClipRect(renderer,&clip) &&
              SDL_GetRenderScale(renderer,&render_scale_x,&render_scale_y) &&
              !SDL_RenderClipEnabled(renderer) &&
              viewport.x==0 && viewport.y==0 && viewport.w==1100 && viewport.h==700 &&
              render_scale_x==1 && render_scale_y==1,
              "renderer viewport/clip/scale leaked after passes");
        ui.shutdown();
        check(openemperor::RoadSpriteSet::live_texture_count()==0,"road textures survived session");

        std::vector<std::uint8_t> city_v10_buildable;
        auto city_v10_session=city_v10_fixture(temp,city_v10_buildable);
        auto city_v10_state=city_v10_world(city_v10_buildable);
        auto city_v10_document=openemperor::persistence::make_document(
            temp.path,"Cities/Synthetic.map",city_v10_buildable,city_v10_state);
        openemperor::SandboxView city_v10_view(std::move(city_v10_session),false,
            simulation::RulesProfile::CityV10);
        city_v10_view.configure_save(temp.path,"Cities/Synthetic.map",
            temp.path.parent_path()/(temp.path.filename().string()+"-city-v10-save.json"),
            std::move(city_v10_document));
        city_v10_view.initialize(window,renderer);
        check(city_v10_view.world().buildings().size()==34 &&
              city_v10_view.world().couriers().size()==14,
              "large City-v10 view did not restore all entities");
        const auto large_panel=city_v10_view.layout().panel;
        const auto large_scale=city_v10_view.layout().scale;
        const auto large_panel_x=large_panel.x+20*large_scale;
        const auto row_y=[&](std::size_t index,int scroll) {
            return large_panel.y+46*large_scale+static_cast<int>(index)*18*large_scale-
                scroll+4*large_scale;
        };
        mouse_click(city_v10_view,static_cast<float>(large_panel_x),
                    static_cast<float>(row_y(0,0)),running);
        check(city_v10_view.selected_building()==static_cast<simulation::BuildingId>(1),
              "large building list did not select first stable ID");
        SDL_Event large_wheel{};
        large_wheel.type=SDL_EVENT_MOUSE_WHEEL;
        large_wheel.wheel.mouse_x=static_cast<float>(large_panel_x);
        large_wheel.wheel.mouse_y=static_cast<float>(large_panel.y+100*large_scale);
        large_wheel.wheel.y=-5;
        city_v10_view.handle_event(large_wheel,running);
        constexpr int middle_scroll=5*24;
        mouse_click(city_v10_view,static_cast<float>(large_panel_x),
                    static_cast<float>(row_y(17,middle_scroll*large_scale)),running);
        check(city_v10_view.selected_building()==static_cast<simulation::BuildingId>(18),
              "large building list confused middle stable ID with vector index");
        const int row_count=34+static_cast<int>(city_v10_view.inspection_lines().size());
        const int final_scroll=std::max(0,56*large_scale+
            row_count*18*large_scale-large_panel.h);
        large_wheel.wheel.y=-100;
        city_v10_view.handle_event(large_wheel,running);
        mouse_click(city_v10_view,static_cast<float>(large_panel_x),
                    static_cast<float>(row_y(33,final_scroll)),running);
        check(city_v10_view.selected_building()==static_cast<simulation::BuildingId>(34),
              "large building list did not select final stable ID after scrolling");
        check(city_v10_view.render(),"large City-v10 building panel frame");
        city_v10_view.shutdown();

        std::vector<std::uint8_t> warning_buildable;
        auto warning_session=city_v10_fixture(temp,warning_buildable);
        simulation::World warning_world(228,228,warning_buildable,
            simulation::RulesProfile::CityV11,2);
        for (const auto cell:{simulation::Cell{100,101},simulation::Cell{103,101},
                              simulation::Cell{106,101},simulation::Cell{109,101},
                              simulation::Cell{112,101}})
            check(warning_world.execute({simulation::CommandType::PlaceHousehold,cell}).accepted,
                  "budget-warning House fixture");
        auto warning_document=openemperor::persistence::make_document(
            temp.path,"Cities/Synthetic.map",warning_buildable,warning_world);
        openemperor::SandboxView warning_view(std::move(warning_session),false,
            simulation::RulesProfile::CityV11);
        warning_view.configure_save(temp.path,"Cities/Synthetic.map",
            temp.path.parent_path()/(temp.path.filename().string()+"-warning-save.json"),
            std::move(warning_document));
        warning_view.initialize(window,renderer);
        const simulation::Command warned{simulation::CommandType::PlaceHousehold,{115,101}};
        const auto before_warning=warning_view.world().snapshot();
        const auto requested=warning_view.request_execute(warned);
        check(!requested.accepted && warning_view.budget_warning_pending() &&
              warning_view.world().snapshot()==before_warning,
              "starter reserve warning executed before confirmation");
        warning_view.tick_once();
        warning_view.update(1.0);
        check(warning_view.world().snapshot()==before_warning && warning_view.render(),
              "budget confirmation advanced or failed to render");
        warning_view.handle_event(key(SDLK_ESCAPE),running);
        check(!warning_view.budget_warning_pending() &&
              warning_view.world().snapshot()==before_warning,
              "budget-warning Escape/Cancel event changed funds, IDs, commands, ticks or World");
        check(!warning_view.request_execute(warned).accepted &&
              warning_view.budget_warning_pending(),"second warning was not opened");
        warning_view.handle_event(key(SDLK_RETURN),running);
        check(!warning_view.budget_warning_pending(),
              "Build anyway Return event did not close confirmation");
        check(warning_view.world().command_sequence()==before_warning.command_sequence+1 &&
              warning_view.world().treasury()==before_warning.treasury-
                  simulation::Rules::household_cost &&
              warning_view.world().next_building_id()==before_warning.next_building_id+1,
              "Build anyway executed anything other than one revalidated command");
        warning_view.shutdown();

        std::vector<std::uint8_t> operations_buildable;
        auto operations_session=city_v10_fixture(temp,operations_buildable);
        openemperor::SandboxView operations_view(std::move(operations_session),false,
            simulation::RulesProfile::CityV11);
        // v4 is explicit until native acceptance permits changing new-game defaults.
        const simulation::World operations_world(maps::stored_grid_width,maps::stored_grid_height,
            operations_buildable,simulation::RulesProfile::CityV11,4);
        operations_view.configure_save(temp.path,"Cities/Synthetic.map",save_path,
            openemperor::persistence::make_document(temp.path,"Cities/Synthetic.map",
                operations_buildable,operations_world));
        operations_view.set_building_visuals(building_manifest);
        operations_view.initialize(window,renderer);
        const auto placed_operation=operations_view.execute(
            {simulation::CommandType::PlaceClaySource,{100,101}});
        check(placed_operation.accepted && operations_view.world().rule_version()==4,
              "City-v11-v4 operation UI fixture failed");
        const auto operation_id=*operations_view.selected_building();
        const auto operation_panel=operations_view.layout().panel;
        const int operation_scale=operations_view.layout().scale;
        const auto operation_before=operations_view.world().snapshot();
        const float toggle_x=static_cast<float>(operation_panel.x+operation_panel.w/2);
        const float toggle_y=static_cast<float>(operation_panel.y+operation_panel.h-
                                                154*operation_scale);
        mouse_click(operations_view,toggle_x,toggle_y,running);
        check(!operations_view.world().building(operation_id).operating_enabled &&
              operations_view.world().workers_assigned(operation_id)==0 &&
              operations_view.world().road_revision()==operation_before.road_revision &&
              operations_view.world().command_sequence()==operation_before.command_sequence+1 &&
              operations_view.dirty(),
              "real SDL Pause operation event did not issue exactly one typed command");
        const int priority_gap=4*operation_scale;
        const int priority_width=(operation_panel.w-20*operation_scale-2*priority_gap)/3;
        const float high_x=static_cast<float>(operation_panel.x+10*operation_scale+
                                              priority_width/2);
        const float priority_y=static_cast<float>(operation_panel.y+operation_panel.h-
                                                  110*operation_scale);
        mouse_click(operations_view,high_x,priority_y,running);
        check(operations_view.world().building(operation_id).workforce_priority==
                  simulation::WorkforcePriority::High &&
              operations_view.world().command_sequence()==operation_before.command_sequence+2,
              "real SDL priority event targeted the wrong building or command");
        const auto after_controls=operations_view.world().snapshot();
        mouse_click(operations_view,static_cast<float>(operation_panel.x+8*operation_scale),
                    static_cast<float>(operation_panel.y+20*operation_scale),running);
        check(operations_view.world().snapshot()==after_controls,
              "operation inspector click-through issued a map command");
        check(operations_view.render(),"operation controls did not render inside inspector");
        // Real SDL events share the production inspector and modal command path.
        const float demolish_x=toggle_x;
        const float demolish_y=static_cast<float>(operation_panel.y+operation_panel.h-64*operation_scale);
        mouse_click(operations_view,static_cast<float>(operation_panel.x+30*operation_scale),
            static_cast<float>(operation_panel.y+55*operation_scale),running);
        check(operations_view.selected_building()==operation_id,"building list picking failed");
        check(operations_view.render(),"pre-demolition sprite render failed");
        auto clay_ground=maps::terrain_world({101,102},72);clay_ground.y+=20;
        const auto clay_screen=operations_view.camera().world_to_screen(clay_ground);
        const auto sprite_pixel=pixel(renderer,static_cast<int>(clay_screen.x),static_cast<int>(clay_screen.y));
        const auto texture_count=operations_view.building_texture_count();
        check(texture_count==4,"synthetic building sprite set was not loaded");
        operations_view.handle_event(key(SDLK_F5),running);
        check(!operations_view.dirty(),"pre-demolition F5 did not establish a clean save");
        const auto demolition_save_generation=operations_view.save_generation();
        const auto before_demolition=operations_view.world().snapshot();
        mouse_click(operations_view,demolish_x,demolish_y,running);
        check(operations_view.demolition_pending() && !operations_view.recovery_safe_point(),
            "Demolish did not open an unsafe-for-recovery confirmation");
        operations_view.tick_once();operations_view.update(1);
        check(operations_view.world().snapshot()==before_demolition && operations_view.render(),
            "demolition modal advanced ticks or failed rendering");
        operations_view.handle_event(key(SDLK_ESCAPE),running);
        check(!operations_view.demolition_pending() && operations_view.world().snapshot()==before_demolition &&
            !operations_view.dirty() && operations_view.save_generation()==demolition_save_generation,
            "demolition Cancel mutated World or manual save state");
        mouse_click(operations_view,demolish_x,demolish_y,running);
        SDL_Event demolition_focus{};demolition_focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;
        operations_view.handle_event(demolition_focus,running);
        operations_view.handle_event(release(demolish_x,demolish_y),running);
        check(!operations_view.demolition_pending() && operations_view.world().snapshot()==before_demolition,
            "focus loss or stray MouseUp confirmed demolition");
        mouse_click(operations_view,demolish_x,demolish_y,running);
        const auto map_rect=operations_view.layout().map;
        const int modal_w=std::min(540*operation_scale,map_rect.w-24*operation_scale);
        const int modal_x=map_rect.x+(map_rect.w-modal_w)/2;
        const int modal_y=map_rect.y+(map_rect.h-260*operation_scale)/2;
        const float confirm_x=static_cast<float>(modal_x+40*operation_scale);
        const float confirm_y=static_cast<float>(modal_y+216*operation_scale);
        operations_view.handle_event(click(confirm_x,confirm_y),running);
        operations_view.handle_event(release(30,80),running);
        check(operations_view.demolition_pending() && operations_view.world().snapshot()==before_demolition,
            "modal press released on map leaked a command");
        // Cancel button is distinct from confirmation, including revalidation.
        mouse_click(operations_view,static_cast<float>(modal_x+290*operation_scale),confirm_y,running);
        check(!operations_view.demolition_pending() && operations_view.world().snapshot()==before_demolition,
            "Cancel button changed authoritative state");
        mouse_click(operations_view,demolish_x,demolish_y,running);
        namespace perf=openemperor::performance;
        perf::reset();perf::set_enabled(true);
        mouse_click(operations_view,confirm_x,confirm_y,running);
        check(!operations_view.demolition_pending() && !operations_view.selected_building() &&
            operations_view.world().buildings().empty() && operations_view.world().couriers().empty() &&
            operations_view.world().command_sequence()==before_demolition.command_sequence+1 &&
            operations_view.dirty() && operations_view.save_generation()==demolition_save_generation &&
            operations_view.render(),"confirmed demolition left an entity or failed to mark the view dirty");
        check(perf::counter(perf::Counter::AssetDecodes)==0 &&
            perf::counter(perf::Counter::TextureUploads)==0 &&
            perf::counter(perf::Counter::WorldCopies)==0 &&
            perf::counter(perf::Counter::RouteRefreshes)==1,"demolition decoded/uploaded/copied or refreshed repeatedly");
        perf::set_enabled(false);
        check(operations_view.building_texture_count()==texture_count &&
            pixel(renderer,static_cast<int>(clay_screen.x),static_cast<int>(clay_screen.y))!=sprite_pixel,
            "demolished sprite remained visible or shared textures were reloaded");
        for (const auto cell:simulation::building_footprint_cells(simulation::RulesProfile::CityV11, 1,
            simulation::Object::ClaySource,{100,101}))
            check(!operations_view.world().building_owner_at(cell),"SDL demolition retained footprint owner");
        for (const auto& line:operations_view.inspection_lines())
            check(line.find("ClaySource #1")==std::string::npos,"old entity ID stayed in inspector");
        const auto demolished_saved_world=operations_view.world().snapshot();
        operations_view.handle_event(key(SDLK_F5),running);
        check(!operations_view.dirty() &&
            operations_view.save_generation()==demolition_save_generation+1,
            "post-demolition F5 did not save exactly once");
        check(operations_view.execute({simulation::CommandType::PlaceHousehold,{106,101}}).accepted &&
            operations_view.execute({simulation::CommandType::PlaceClaySource,{100,101}}).accepted,
            "rebuild after SDL demolition failed");
        const auto rebuilt_id=*operations_view.selected_building();
        for (int i=0;i<32;++i) operations_view.tick_once();
        check(operations_view.world().building(rebuilt_id).output>0,"blocked UI fixture has no stock");
        const auto blocked_before=operations_view.world().snapshot();
        mouse_click(operations_view,demolish_x,demolish_y,running);
        check(!operations_view.demolition_pending() && operations_view.world().snapshot()==blocked_before &&
            operations_view.last_message().find("Clay")!=std::string::npos && operations_view.render(),
            "blocked stock demolition opened modal or mutated World");
        operations_view.handle_event(key(SDLK_F9),running);
        check(operations_view.world().snapshot()==demolished_saved_world && operations_view.paused() &&
            !operations_view.dirty() && !operations_view.selected_building() &&
            operations_view.building_texture_count()==texture_count && operations_view.render(),
            "F9 did not restore the exact demolished World paused without ghost entities");
        operations_view.shutdown();

        // City-v13: presentation/placement projections never execute or copy a World.
        std::vector<std::uint8_t> quality_mask;
        auto quality_session=city_v10_fixture(temp,quality_mask);
        const auto quality_border=quality_session.plan.border;
        openemperor::SandboxView quality_view(std::move(quality_session),false,
            simulation::RulesProfile::CityV13);
        quality_view.configure_save(temp.path,"Cities/Synthetic.map",{});
        quality_view.initialize(window,renderer);
        check(quality_view.execute({simulation::CommandType::PlaceHousehold,{110,104}}).accepted &&
            quality_view.execute({simulation::CommandType::PlacePottery,{112,104}}).accepted &&
            quality_view.execute({simulation::CommandType::PlaceMarket,{114,104}}).accepted,
            "City-v13 UI fixture commands failed");
        const auto quality_house=*quality_view.world().building_owner_at({110,104});
        quality_view.set_tool(5);
        const auto house_screen=quality_view.camera().world_to_screen(maps::terrain_ground({110,104},quality_border));
        mouse_click(quality_view,static_cast<float>(house_screen.x),static_cast<float>(house_screen.y),running);
        check(quality_view.selected_building()==quality_house,"City-v13 House selection");
        const auto quality_details=quality_view.inspection_lines();
        const auto has_detail=[&](const std::string& value) {
            return std::find(quality_details.begin(),quality_details.end(),value)!=quality_details.end();
        };
        check(has_detail("House Level: 0") && has_detail("Historical development: 0") &&
            has_detail("Desirability: -13") && has_detail("Level cap: 1") &&
            has_detail("Population: 6 / 6"),"City-v13 quality inspector missing authority/derived distinction");
        const auto quality_before=quality_view.world().snapshot();
        const auto quality_refresh=quality_view.world().route_refresh_count();
        perf::set_enabled(true);perf::reset();
        for (int i=0;i<100;++i) {
            quality_view.handle_event(key(SDLK_D),running);
            check(quality_view.desirability_overlay()==(i%2==0) && quality_view.render(),
                "Desirability key/overlay failed");
        }
        check(quality_view.world().snapshot()==quality_before &&
            quality_view.world().route_refresh_count()==quality_refresh &&
            perf::counter(perf::Counter::WorldCopies)==0 &&
            perf::counter(perf::Counter::WorldRestores)==0 &&
            perf::counter(perf::Counter::WorldExecutes)==0 &&
            perf::counter(perf::Counter::SimulationTicks)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0 &&
            perf::counter(perf::Counter::RouteRefreshes)==0 &&
            perf::counter(perf::Counter::AssetDecodes)==0 &&
            perf::counter(perf::Counter::TextureUploads)==0 &&
            perf::counter(perf::Counter::FileWrites)==0,"overlay changed World / did asset or route work");
        const auto untouched=quality_view.camera().world_to_screen(maps::terrain_ground({107,104},quality_border));
        const auto house_pixel_before=pixel(renderer,static_cast<int>(house_screen.x),static_cast<int>(house_screen.y));
        const auto empty_pixel_before=pixel(renderer,static_cast<int>(untouched.x),static_cast<int>(untouched.y));
        const auto quality_button=std::find_if(quality_view.layout().buttons.begin(),quality_view.layout().buttons.end(),
            [](const auto& button){return button.action==openemperor::sandbox_ui::Action::Desirability;});
        check(quality_button!=quality_view.layout().buttons.end(),"Desirability toolbar missing");
        mouse_click(quality_view,static_cast<float>(quality_button->rect.x+10),
            static_cast<float>(quality_button->rect.y+10),running);
        check(quality_view.desirability_overlay() && quality_view.world().snapshot()==quality_before,
            "Desirability toolbar changed authority");
        check(quality_view.render() &&
            pixel(renderer,static_cast<int>(house_screen.x),static_cast<int>(house_screen.y))!=house_pixel_before &&
            pixel(renderer,static_cast<int>(untouched.x),static_cast<int>(untouched.y))==empty_pixel_before,
            "overlay did not tint House / tinted a non-House cell");
        const auto blended_house=pixel(renderer,static_cast<int>(house_screen.x),static_cast<int>(house_screen.y));
        check(blended_house!=std::array<std::uint8_t,4>{204,169,81,255},
            "House overlay was opaque instead of preserving the building below");
        SDL_BlendMode after_overlay=SDL_BLENDMODE_BLEND;
        check(SDL_GetRenderDrawBlendMode(renderer,&after_overlay) && after_overlay==SDL_BLENDMODE_NONE,
            "overlay leaked blend mode into ordinary World/HUD rendering");
        quality_view.set_tool(7);
        const auto location=quality_view.camera().world_to_screen(maps::terrain_ground({107,104},quality_border));
        quality_view.handle_event(motion(static_cast<float>(location.x),static_cast<float>(location.y)),running);
        quality_view.update(0.0); // Pointer projections are coalesced at the frame boundary.
        check(quality_view.predicted_desirability()==quality_view.world().household_desirability_at({107,104}),
            "House preview score differs");
        const auto quality_plans=quality_view.desirability_preview_build_count();
        for (int i=0;i<200;++i) {
            quality_view.handle_event(motion(static_cast<float>(location.x)+static_cast<float>(i%3),
                static_cast<float>(location.y)),running);
            quality_view.update(0.0);
        }
        check(quality_view.desirability_preview_build_count()==quality_plans &&
            quality_view.world().snapshot()==quality_before &&
            perf::counter(perf::Counter::WorldCopies)==0 && perf::counter(perf::Counter::BfsCalls)==0 &&
            perf::counter(perf::Counter::WorldExecutes)==0,"per-pixel preview rebuilt or mutated");
        perf::set_enabled(false);
        check(quality_view.execute(simulation::demolish_building(
            *quality_view.world().building_owner_at({112,104}))).accepted,"preview fixture safe demolition");
        quality_view.handle_event(motion(static_cast<float>(location.x),static_cast<float>(location.y)),running);
        quality_view.update(0.0); // Pointer projections are coalesced at the frame boundary.
        check(quality_view.desirability_preview_build_count()==quality_plans+1 &&
            quality_view.predicted_desirability()==quality_view.world().household_desirability_at({107,104}),
            "building topology did not invalidate preview");
        quality_view.shutdown();

        // City-v12 keeps fire presentation read-only, including active incidents.
        std::vector<std::uint8_t> fire_buildable;
        auto fire_session=city_v10_fixture(temp,fire_buildable);
        openemperor::SandboxView fire_view(std::move(fire_session),true,
            simulation::RulesProfile::CityV12);
        fire_view.configure_save(temp.path,"Cities/Synthetic.map",{});
        fire_view.set_building_visuals(building_fixture(temp));
        fire_view.set_walker_visuals(multi_role_walker_fixture(temp));
        fire_view.initialize(window,renderer);
        check(fire_view.world().ticks()==0 && fire_view.world().treasury()==20 &&
            fire_view.world().workforce_used()==24 && fire_view.world().workforce_required()==24 &&
            fire_view.world().buildings().size()==11 && fire_view.world().couriers().size()==8,
            "City-v12 SDL starter is not paid or fully staffed");
        const auto fire_button=std::find_if(fire_view.layout().buttons.begin(),fire_view.layout().buttons.end(),
            [](const auto& button){return button.action==openemperor::sandbox_ui::Action::FireWatch;});
        check(fire_button!=fire_view.layout().buttons.end() &&
            fire_view.layout().button_at(fire_button->rect.x+1,fire_button->rect.y+1)==
                openemperor::sandbox_ui::Action::FireWatch,"Fire Watch toolbar missing");
        // The affordable tool is tested in a separate empty profile-specific view below.
        const auto watch=std::find_if(fire_view.world().buildings().begin(),fire_view.world().buildings().end(),
            [](const auto& b){return b.kind==simulation::Object::FireWatch;})->id;
        check(fire_view.execute(simulation::set_building_operation(watch,false)).accepted,
            "Watch pause command unavailable");
        for (int i=0;i<2000;++i) fire_view.tick_once();
        check(fire_view.world().burning_buildings()==10 && fire_view.world().fire_state_valid(),
            "UI fixture natural incidents absent");
        mouse_click(fire_view,static_cast<float>(fire_view.layout().panel.x+20),
            static_cast<float>(fire_view.layout().panel.y+50),running);
        check(fire_view.selected_building()==fire_view.world().buildings().front().id,
            "fire inspector selection failed");
        const auto fire_details=fire_view.inspection_lines();
        check(std::find(fire_details.begin(),fire_details.end(),"On fire - operation suspended.")!=fire_details.end() &&
            std::find(fire_details.begin(),fire_details.end(),"Status: Unstaffed")==fire_details.end(),
            "burning building mislabeled unstaffed");
        const auto before_fire_render=fire_view.world().snapshot();
        perf::set_enabled(true);perf::reset();
        for (int i=0;i<50;++i) {
            for (const auto code:{SDLK_F1,SDLK_F2,SDLK_F4,SDLK_F6,SDLK_TAB})
                fire_view.handle_event(key(code),running);
            SDL_Event zoom{};zoom.type=SDL_EVENT_MOUSE_WHEEL;
            zoom.wheel.mouse_x=300;zoom.wheel.mouse_y=240;zoom.wheel.y=i%2 ? -1.0F:1.0F;
            fire_view.handle_event(zoom,running);
            fire_view.handle_event(key(i%2 ? SDLK_LEFT:SDLK_RIGHT),running);
            check(fire_view.render(),"fire primitive overlay render failed");
        }
        check(fire_view.world().snapshot()==before_fire_render &&
            perf::counter(perf::Counter::AssetDecodes)==0 &&
            perf::counter(perf::Counter::TextureUploads)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0 &&
            perf::counter(perf::Counter::RouteRefreshes)==0 &&
            perf::counter(perf::Counter::WorldCopies)==0,"fire render changed World or loaded assets/routes");
        perf::set_enabled(false);fire_view.shutdown();
        auto empty_fire_session=city_v10_fixture(temp,fire_buildable);
        openemperor::SandboxView empty_fire(std::move(empty_fire_session),false,
            simulation::RulesProfile::CityV12);
        empty_fire.initialize(window,renderer);empty_fire.handle_event(key(SDLK_F),running);
        check(empty_fire.tool()==11 && empty_fire.render(),"F does not select Fire Watch");
        const auto watch_manifest=fire_watch_fixture(temp);
        Temp watch_saves;
        empty_fire.configure_save(temp.path,"Cities/Synthetic.map",watch_saves.path/"watch-save.json");
        perf::set_enabled(true);perf::reset();
        empty_fire.set_building_visuals(watch_manifest);
        check(empty_fire.building_texture_count()==1 &&
            perf::counter(perf::Counter::AssetDecodes)==1 &&
            perf::counter(perf::Counter::TextureUploads)==1,"Watch profile did not decode/upload once");
        for (const auto command:std::vector<simulation::Command>{
            {simulation::CommandType::PlaceHousehold,{100,101}},
            {simulation::CommandType::PlaceFireWatch,{103,101}},
            {simulation::CommandType::PlaceHousehold,{106,101}},
            {simulation::CommandType::PlaceFireWatch,{109,101}}})
            check(empty_fire.execute(command).accepted,"dynamic Watch fixture placement");
        const auto second_watch=*empty_fire.selected_building();
        std::vector<simulation::BuildingId> watches;
        for (const auto& b:empty_fire.world().buildings())
            if (b.kind==simulation::Object::FireWatch) watches.push_back(b.id);
        check(watches.size()==2 && watches[0]!=watches[1] &&
            second_watch==watches[1],"Watch kind was confused with ID");
        for (int x=101;x<=109;++x)
            check(empty_fire.execute({simulation::CommandType::PlaceRoad,{x,103}}).accepted,"Watch road");
        for (const int x:{103,109})
            check(empty_fire.execute({simulation::CommandType::PlaceRoad,{x,102}}).accepted,"Watch entrance");
        // A no-op road selects no new entity; select the Watch by its occupied map cell.
        empty_fire.set_tool(5);
        const auto select_watch=[&]() {
            const auto ground=empty_fire.camera().world_to_screen(
                maps::terrain_ground({109,101},72));
            mouse_click(empty_fire,static_cast<float>(ground.x),static_cast<float>(ground.y),running);
            check(empty_fire.selected_building()==second_watch,"Watch map selection");
        };
        select_watch();
        empty_fire.tick_once();
        auto join=[](const auto& lines) {
            std::string result;for (const auto& line:lines) { if (!result.empty()) result+=' ';result+=line; }
            return result;
        };
        const auto watch_details=join(empty_fire.inspection_lines());
        check(watch_details.starts_with("Fire Watch #") && watch_details.find("Workers assigned 2/2")!=std::string::npos &&
            watch_details.find("Inspector phase:")!=std::string::npos && watch_details.find("Route status:")!=std::string::npos &&
            watch_details.find("City-wide protected")!=std::string::npos && watch_details.find("...")==std::string::npos,
            "Watch inspector priority, wrapping or city-wide attribution");
        check(join(empty_fire.demolition_hint_lines())=="Inspector is still on patrol.",
            "Watch demolition showed false empty-stock advice");
        const auto before_watch_draw=empty_fire.world().snapshot();
        const auto before_watch_stats=empty_fire.building_display_stats();
        perf::reset();
        for (int frame=0;frame<4;++frame) {
            check(empty_fire.render(),"two Watch render");
            empty_fire.handle_event(key(SDLK_F4),running);
        }
        const auto after_watch_stats=empty_fire.building_display_stats();
        const auto role=openemperor::assets::role_index(openemperor::assets::BuildingVisualRole::FireWatch);
        check(after_watch_stats.drawn_instances[role]-before_watch_stats.drawn_instances[role]==4 &&
            after_watch_stats.placeholder_fallbacks[role]-before_watch_stats.placeholder_fallbacks[role]==4 &&
            empty_fire.world().snapshot()==before_watch_draw && empty_fire.building_texture_count()==1 &&
            perf::counter(perf::Counter::AssetDecodes)==0 && perf::counter(perf::Counter::TextureUploads)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0 && perf::counter(perf::Counter::RouteRefreshes)==0,
            "F4 did not toggle both Watches with shared texture and unchanged World");
        const auto preview_world=empty_fire.world().snapshot();
        const auto point=[&](std::uint32_t x,std::uint32_t y) {
            return empty_fire.camera().world_to_screen(maps::terrain_ground({x,y},72));
        };
        perf::reset();
        empty_fire.set_tool(11);
        const auto watch_hover=point(111,105);
        empty_fire.handle_event(motion(static_cast<float>(watch_hover.x),static_cast<float>(watch_hover.y)),running);
        check(empty_fire.render(),"Watch placement preview render");
        empty_fire.set_tool(1);
        const auto road_start=point(112,105),road_end=point(115,105);
        empty_fire.handle_event(click(static_cast<float>(road_start.x),
            static_cast<float>(road_start.y)),running);
        empty_fire.handle_event(motion(static_cast<float>(road_end.x),static_cast<float>(road_end.y)),running);
        check(empty_fire.render() && empty_fire.road_preview().valid,"road preview beside selected Watch");
        empty_fire.handle_event(key(SDLK_ESCAPE),running);
        check(empty_fire.world().snapshot()==preview_world && perf::counter(perf::Counter::WorldCopies)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0 && perf::counter(perf::Counter::RouteRefreshes)==0 &&
            perf::counter(perf::Counter::AssetDecodes)==0 && perf::counter(perf::Counter::TextureUploads)==0,
            "Watch/road preview did work or changed fire, population, funds or commands");
        select_watch();
        bool bad_profile_rejected=false;
        try { empty_fire.set_building_visuals(temp.path/"missing-watch.json"); }
        catch (const std::exception&) {bad_profile_rejected=true;}
        check(bad_profile_rejected && empty_fire.building_texture_count()==1 &&
            empty_fire.building_display_stats().configured_roles[role],"failed Watch change lost old profile");
        // Existing operation hit path must not issue a map placement on mouse-up.
        const auto sequence=empty_fire.world().command_sequence();
        const auto& layout=empty_fire.layout();
        mouse_click(empty_fire,static_cast<float>(layout.panel.x+30*layout.scale),
            static_cast<float>(layout.panel.y+layout.panel.h-154*layout.scale),running);
        check(!empty_fire.world().building(second_watch).operating_enabled &&
            empty_fire.world().command_sequence()==sequence+1 && empty_fire.world().buildings().size()==4,
            "Watch Pause clicked through into map or changed multiple commands");
        check(empty_fire.execute({simulation::CommandType::RemoveRoad,{108,103}}).accepted,
            "Inspector future-road interruption fixture");
        select_watch();
        check(join(empty_fire.demolition_hint_lines())=="Inspector route interrupted; reconnect roads.",
            "interrupted Inspector hint falsely promised waiting alone");
        check(empty_fire.execute({simulation::CommandType::PlaceRoad,{108,103}}).accepted,
            "Inspector road reconnect fixture");
        select_watch();
        bool returned=false;
        for (int tick=0;tick<600;++tick) {
            empty_fire.tick_once();
            const auto& c=*std::find_if(empty_fire.world().couriers().begin(),empty_fire.world().couriers().end(),
                [&](const auto& value){return value.owner==second_watch;});
            if (c.phase==simulation::CourierPhase::Returning)
                check(join(empty_fire.demolition_hint_lines())=="Waiting for the Inspector to return.",
                    "returning Watch demolition hint incorrect");
            if (c.phase==simulation::CourierPhase::IdleAtWorkshop) {returned=true;break;}
        }
        check(returned && empty_fire.world().demolition_status(second_watch).allowed,
            "paused Inspector did not finish with existing demolition semantics");
        empty_fire.save_now();
        const auto exact_watch_save=empty_fire.world().snapshot();
        for (int tick=0;tick<20;++tick) empty_fire.tick_once();
        empty_fire.load_now();
        check(empty_fire.world().snapshot()==exact_watch_save && empty_fire.paused() &&
            empty_fire.world().rule_version()==1 && empty_fire.world().buildings().size()==4,
            "schema14 Watch visual load changed entities, paths, controls or fire deadlines");
        // Empty, paused Watch can be demolished without changing the visual pipeline.
        select_watch();
        const auto before_watch_demolish=empty_fire.world().snapshot();
        mouse_click(empty_fire,static_cast<float>(layout.panel.x+30*layout.scale),
            static_cast<float>(layout.panel.y+layout.panel.h-64*layout.scale),running);
        check(empty_fire.demolition_pending() && empty_fire.world().snapshot()==before_watch_demolish,
            "Watch demolition button clicked through or mutated before confirmation");
        check(empty_fire.resolve_demolition(true) && !empty_fire.world().building_owner_at({109,101}),
            "safe Watch demolition failed");
        const auto preview_before=empty_fire.world().snapshot();
        perf::reset();empty_fire.set_tool(11);
        const auto rebuilt_point=empty_fire.camera().world_to_screen(maps::terrain_ground({109,101},72));
        empty_fire.handle_event(motion(static_cast<float>(rebuilt_point.x),static_cast<float>(rebuilt_point.y)),running);
        check(empty_fire.preview({109,101}).accepted && empty_fire.render() &&
            empty_fire.world().snapshot()==preview_before && perf::counter(perf::Counter::AssetDecodes)==0 &&
            perf::counter(perf::Counter::BfsCalls)==0 && perf::counter(perf::Counter::WorldCopies)==0,
            "valid partial-alpha Watch preview modified World or loaded per frame");
        check(empty_fire.execute({simulation::CommandType::PlaceFireWatch,{109,101}}).accepted &&
            *empty_fire.selected_building()!=second_watch && empty_fire.building_texture_count()==1,
            "Watch rebuild reused IDs or uploaded another shared asset");
        perf::set_enabled(false);
        empty_fire.shutdown();

        const auto scaled=openemperor::sandbox_ui::make_layout(2200,1400,1100,700,true);
        const auto scaled_button=std::find_if(scaled.buttons.begin(),scaled.buttons.end(),
            [](const auto& item) { return item.action==openemperor::sandbox_ui::Action::Clay; });
        check(scaled_button!=scaled.buttons.end(),"2x button present");
        const auto button_window_x=(scaled_button->rect.x+scaled_button->rect.w/2.0)/2.0;
        const auto button_window_y=(scaled_button->rect.y+scaled_button->rect.h/2.0)/2.0;
        check(scaled.button_at(button_window_x*2,button_window_y*2)==
                  openemperor::sandbox_ui::Action::Clay,
              "2x window-to-render UI hit");
        auto camera_2x=ui.camera();
        camera_2x.viewport_width=scaled.map.w;
        camera_2x.viewport_height=scaled.map.h;
        auto storage_world=maps::terrain_world({110,114},72);
        storage_world.y+=20;
        camera_2x.center_on(storage_world);
        camera_2x.offset.y+=scaled.map.y;
        const auto physical=camera_2x.world_to_screen(storage_world);
        const openemperor::scene::Point window_point{physical.x/2,physical.y/2};
        const openemperor::scene::Point back_to_render{window_point.x*2,window_point.y*2};
        const auto picked=maps::pick_terrain_cell(camera_2x.screen_to_world(back_to_render),
                                                  maps::MapGeometry{84});
        check(scaled.map.contains(back_to_render.x,back_to_render.y) && picked &&
              *picked==maps::GridCell{110,114},"2x window-to-render map picking");
        SDL_Texture* target_2x=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,2200,1400);
        check(target_2x && SDL_SetRenderTarget(renderer,target_2x),"2x software target");
        openemperor::SandboxView scaled_view(fixture(temp,true,true,true),true,
            simulation::RulesProfile::IndustryV5);
        scaled_view.initialize(window,renderer);
        check(scaled_view.layout().scale==2 && scaled_view.render(),"2x UI render");
        const auto panel=scaled_view.layout().panel;
        const SDL_Rect text_sample{panel.x+12,panel.y+18,260,38};
        SDL_Surface* sample=SDL_RenderReadPixels(renderer,&text_sample);
        check(sample!=nullptr,"2x panel readback");
        bool found_text=false;
        for (int y=0;y<sample->h && !found_text;++y)
            for (int x=0;x<sample->w && !found_text;++x) {
                std::uint8_t r=0,g=0,b=0,a=0;
                check(SDL_ReadSurfacePixel(sample,x,y,&r,&g,&b,&a),"2x panel pixel");
                found_text=r>180 && g>180 && b>180;
            }
        SDL_DestroySurface(sample);
        check(found_text,"2x inspector text clipped away");
        scaled_view.shutdown();
        check(SDL_SetRenderTarget(renderer,nullptr),"restore software target");
        SDL_DestroyTexture(target_2x);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::cout << "Sandbox software view checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Sandbox software view failed: " << error.what() << '\n'; return 1;
    }
}
