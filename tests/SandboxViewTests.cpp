#include "app/SandboxView.h"
#include "app/WalkerPose.h"
#include "maps/TerrainRenderPlan.h"

#include <SDL3/SDL.h>

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
}
int main() {
    try {
        {
            const auto layout=openemperor::sandbox_ui::make_layout(1100,700,1100,700,true);
            check(layout.top.h==52 && layout.map.w==796 && layout.map.h==552 &&
                  layout.toolbar.h==72 && layout.panel.w==304,
                  "1100x700 map area/layout");
            check(layout.ui_at(900,200) && !layout.ui_at(400,200) &&
                  layout.button_at(1050,20)==openemperor::sandbox_ui::Action::TogglePanel,
                  "layout hit regions");
            const auto hidpi=openemperor::sandbox_ui::make_layout(2200,1400,1100,700,true);
            check(hidpi.scale==2 && hidpi.map.w==1592 && hidpi.map.h==1104,
                  "2x display layout");
            const auto small=openemperor::sandbox_ui::make_layout(600,400,600,400,true);
            check(!small.panel_open && small.map.w==600 && small.map.h==252,
                  "small display map area");
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
        check(SDL_CreateWindowAndRenderer("sandbox test",800,600,0,&window,&renderer),"window");
        openemperor::SandboxView view(fixture(temp),false);
        view.initialize(window,renderer);
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
        bool running=true;
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

        openemperor::SandboxView production_view(fixture(temp,true),false,
            simulation::RulesProfile::ProductionV2);
        const auto save_path=temp.path.parent_path()/
            (temp.path.filename().string()+"-viewer-save.json");
        production_view.configure_save(temp.path,"Cities/Synthetic.map",save_path);
        production_view.initialize(window,renderer);
        check(std::string(SDL_GetWindowTitle(window)).find("Production Sandbox")!=std::string::npos,
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
                if (profile.frames[*pose.frame].alias=="red")
                    found_color|=color[0]>220 && color[1]<80 && color[2]<80;
                else found_color|=color[1]>220 && color[0]<80 && color[2]<80;
            }
            if (profile.frames[*pose.frame].alias=="red") {
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
        const auto prior_textures=production_view.walker_texture_count();
        bool rejected=false;
        try { production_view.set_walker_visuals(temp.path/"missing-walker.json"); }
        catch (const std::exception&) { rejected=true; }
        check(rejected && production_view.walker_texture_count()==prior_textures &&
              production_view.world().snapshot()==before_toggle && production_view.render(),
              "failed walker activation destroyed running world or textures");
        production_view.shutdown();
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
        industry_view.set_walker_visuals(walker_manifest);
        industry_view.initialize(window,renderer);
        check(industry_view.walker_texture_count()==2,"second clay courier duplicated textures");
        check(industry_view.world().building(static_cast<simulation::BuildingId>(8)).kind==
                  simulation::Object::ClaySource &&
              industry_view.world().building(static_cast<simulation::BuildingId>(9)).kind==
                  simulation::Object::Pottery && industry_view.tool()==5,
              "v5 viewer did not place two production instances");
        for (int i=0;i<700;++i) industry_view.tick_once();
        check(industry_view.render() && industry_view.last_courier_draws()==5 &&
              industry_view.world().building(static_cast<simulation::BuildingId>(9)).recipes_completed>0,
              "v5 software frame did not draw five couriers and active second pottery");
        industry_view.handle_event(key(SDLK_2),running);
        check(industry_view.tool()==5,
              "v5 full Clay tool remained selectable");
        industry_view.shutdown();

        check(SDL_SetWindowSize(window,1100,700),"resize end-to-end window");
        openemperor::SandboxView ui(fixture(temp,true,true,true),false,
            simulation::RulesProfile::IndustryV5);
        ui.configure_save(temp.path,"Cities/Synthetic.map",save_path);
        ui.initialize(window,renderer);
        check(ui.layout().map.w==796 && ui.layout().map.h==552,"end-to-end viewport");
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
        press_action(openemperor::sandbox_ui::Action::Clay); map_click(110,114);
        press_action(openemperor::sandbox_ui::Action::Pottery); map_click(113,114);
        press_action(openemperor::sandbox_ui::Action::Road);
        const auto first_start=map_point(111,114),first_end=map_point(112,114);
        const auto before_drag=ui.world().snapshot();
        ui.handle_event(click(static_cast<float>(first_start.x),static_cast<float>(first_start.y)),running);
        ui.handle_event(motion(static_cast<float>(first_end.x),static_cast<float>(first_end.y)),running);
        check(ui.world().snapshot()==before_drag && ui.road_preview().cells.size()==2,
              "road drag mutated world before release");
        check(ui.render() && ui.world().snapshot()==before_drag,
              "rendered held road changed simulation");
        ui.handle_event(release(static_cast<float>(first_end.x),static_cast<float>(first_end.y)),running);
        check(ui.world().command_sequence()==before_drag.command_sequence+2 &&
              ui.world().object_at({111,114})==simulation::Object::Road &&
              ui.world().object_at({112,114})==simulation::Object::Road,
              "road drag did not commit exactly two commands");
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
