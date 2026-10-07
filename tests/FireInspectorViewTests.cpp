#include "app/SandboxView.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/FireSpriteSet.h"
#include "renderer/WalkerSpriteSet.h"
#include "FireInspectorWalkerFixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace maps=openemperor::maps;
namespace perf=openemperor::performance;
namespace authored=openemperor::testing::inspector;
using View=openemperor::SandboxView;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
using Pixel=std::array<std::uint8_t,4>;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
Surface read(SDL_Renderer* renderer) {
    Surface result{SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface};
    check(bool(result),"production readback");return result;
}
Pixel pixel(SDL_Surface* surface,int x,int y) {
    Pixel result{};
    check(SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]),"production pixel");return result;
}
void toggle(View& view,SDL_Keycode key) {
    bool running=true;SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;
    view.handle_event(event,running);check(running,"unexpected quit");
}
void click(View& view,openemperor::scene::Point point) {
    bool running=true;SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=static_cast<float>(point.x);event.button.y=static_cast<float>(point.y);
    view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(event,running);
}
void render_pure(View& view) {
    const auto before=view.world().snapshot();perf::set_enabled(true);perf::reset();
    check(view.render(),"normal Sandbox render");
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,"render performed simulation/navigation/IO/decode/upload work");
    perf::set_enabled(false);check(view.world().snapshot()==before,"render changed full World snapshot");
}
fs::path fire_profile(const authored::Fixture& fixture) {
    const auto path=fixture.root/"fire.json";
    authored::Fixture::save(path,{{"schema_version",1},{"mode","curated_fire_presentation"},
        {"clip_id","authored-effect-budget-fixture"},{"evidence","Independent effect resource accounting fixture."},
        {"ticks_per_frame",2},{"frames",Json::array({
            {{"archive","DATA/walker.sg3"},{"image_index",1},{"anchor",{5,15}}},
            {{"archive","DATA/walker.sg3"},{"image_index",3},{"anchor",{6,17}}}})}});
    return path;
}
maps::StoredMapSession session(const authored::Fixture& fixture,bool full_budget=false) {
    fs::create_directories(fixture.data/"Cities");
    { std::ofstream out(fixture.data/"Cities/Authored.map");out<<"Independent Inspector presentation fixture"; }
    maps::ParsedEmperorMap map;map.declared_map_size=84;
    maps::StoredGraphicsPlan plan;plan.data_root=fixture.data;plan.profile=maps::StoredGraphicsProfile::Slot8;
    plan.border=maps::MapGeometry{84}.border;
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);plan.cell_by_storage.resize(228U*228U);
    const auto add_asset=[&](std::uint32_t record,int width,int height,std::uint32_t length,
                             std::uint16_t type,const char* archive) {
        openemperor::assets::AssetRecord metadata;metadata.id={archive,record};
        metadata.width=static_cast<std::int16_t>(width);metadata.height=static_cast<std::int16_t>(height);
        metadata.data_length=length;metadata.image_type=type;
        maps::StoredAsset asset;asset.record=metadata;plan.assets.push_back(std::move(asset));
    };
    if (full_budget) {
        // Real eagerly decoded/uploaded synthetic images use 64MiB - 4096 bytes.
        // Core and fire fit; optional Inspector must not displace either owner.
        Bytes archive(40680+5*72,0),bitmap{0,0,0,0};
        authored::u32(archive,0,static_cast<std::uint32_t>(archive.size()));authored::u32(archive,4,214);
        authored::u32(archive,12,5);authored::u32(archive,16,5);authored::u32(archive,20,1);
        const std::string group="authored-budget.bmp";std::copy(group.begin(),group.end(),archive.begin()+680);
        authored::u32(archive,804,5);
        for (std::uint32_t record=1;record<=4;++record) {
            const int height=record==4 ? 4095:4096;
            Bytes payload;
            for (int y=0;y<height;++y) payload.insert(payload.end(),{255,255,255,255,255,255,255,255,255,4});
            const auto at=40680+record*72;authored::u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));
            authored::u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
            authored::u16(archive,at+20,1024);authored::u16(archive,at+22,static_cast<std::uint16_t>(height));
            authored::u16(archive,at+50,256);bitmap.insert(bitmap.end(),payload.begin(),payload.end());
            add_asset(record,1024,height,static_cast<std::uint32_t>(payload.size()),256,"DATA/budget.sg3");
        }
        authored::write(fixture.data/"DATA/budget.sg3",archive);authored::write(fixture.data/"DATA/budget.555",bitmap);
    } else {
        Bytes archive(40680+64,0),bitmap(3200,0);
        authored::u32(archive,0,static_cast<std::uint32_t>(archive.size()));authored::u32(archive,4,213);
        authored::u32(archive,12,1);authored::u32(archive,16,1);authored::u32(archive,20,1);
        const std::string group="authored-ground.bmp";std::copy(group.begin(),group.end(),archive.begin()+680);
        authored::u32(archive,804,1);authored::u32(archive,808,1);
        authored::u32(archive,40684,3200);authored::u32(archive,40688,3200);
        authored::u16(archive,40700,78);authored::u16(archive,40702,40);authored::u16(archive,40730,30);archive[40735]=1;
        for (std::size_t at=0;at<bitmap.size();at+=2) authored::u16(bitmap,at,0x18c6);
        authored::write(fixture.data/"DATA/ground.sg3",archive);authored::write(fixture.data/"DATA/ground.555",bitmap);
        add_asset(0,78,40,3200,30,"DATA/ground.sg3");plan.assets[0].record.uncompressed_length=3200;
    }
    for (std::uint32_t y=110;y<=122;++y) for (std::uint32_t x=110;x<=122;++x) {
        const auto asset=full_budget ? static_cast<std::size_t>((x+y)%4):0;
        const auto& record=plan.assets[asset].record;
        maps::StoredCell cell;cell.storage={x,y};cell.cell_index=std::size_t(y)*228U+x;
        cell.terrain_raw=0x80;cell.status=maps::StoredStatus::DecodePending;cell.asset_index=asset;
        cell.footprint_index=plan.footprints.size();cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=maps::stored_image_origin(cell.world,static_cast<unsigned>(record.width),static_cast<unsigned>(record.height));
        const auto index=plan.cells.size();plan.cell_by_storage[cell.cell_index]=index;
        plan.status_by_storage[cell.cell_index]=cell.status;plan.cells.push_back(cell);
        maps::PlacedFootprint footprint;footprint.id=plan.footprints.size();footprint.asset_index=asset;
        footprint.origin=cell.storage;footprint.cell_indices={index};footprint.image_origin=cell.image_origin;
        footprint.status=maps::StoredStatus::DecodePending;plan.footprints.push_back(footprint);
    }
    return {std::move(map),std::move(plan)};
}
void configure(View& view,const authored::Fixture& fixture,const fs::path& inspector,
               openemperor::VisualProfileSource source=openemperor::VisualProfileSource::Builtin) {
    view.configure_save(fixture.data,"Cities/Authored.map",fixture.root/"save.json");
    view.set_walker_visuals(fixture.core,source);view.set_fire_visuals(fire_profile(fixture));
    view.set_fire_inspector_visuals(inspector);
}
sim::BuildingId put(View& view,sim::CommandType type,sim::Cell cell) {
    const auto result=view.execute({type,cell});check(result.accepted && result.changed,"ordinary paid scene command");
    return *view.world().building_owner_at(cell);
}
sim::CourierId inspector_id(const View& view,sim::BuildingId watch) {
    for (const auto& courier:view.world().couriers())
        if (courier.owner==watch && courier.role==sim::CourierRole::FireInspector) return courier.id;
    throw std::runtime_error("existing Inspector missing");
}
openemperor::scene::Point ground(const View& view,sim::CourierId id) {
    const auto position=view.world().courier_position(id);check(position.has_value(),"Inspector position missing");
    // Independent storage projection, with position supplied solely by World.
    return view.camera().world_to_screen({40*(position->x-position->y),20*(position->x+position->y-144)+20});
}
void zoom_to(View& view,sim::CourierId id,double target) {
    const auto anchor=ground(view,id);SDL_Event event{};event.type=SDL_EVENT_MOUSE_WHEEL;
    event.wheel.mouse_x=static_cast<float>(anchor.x);event.wheel.mouse_y=static_cast<float>(anchor.y);
    event.wheel.y=static_cast<float>(std::log(target/view.camera().zoom)/std::log(1.15));
    bool running=true;view.handle_event(event,running);check(std::abs(view.camera().zoom-target)<0.00001,"wheel zoom");
}
bool contains(const std::vector<std::string>& lines,std::string_view needle) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(needle)!=std::string::npos;});
}
void optional_failure_checks(SDL_Window* window,SDL_Renderer* renderer) {
    authored::Fixture fixture;auto invalid=authored::Fixture::inspector();
    invalid["roles"]["fire_inspector"]["frames"][7]["image_index"]=17;
    const auto broken=fixture.root/"broken.json";authored::Fixture::save(broken,invalid);
    for (const bool budget:{false,true}) {
        View view(session(fixture,budget),false,sim::RulesProfile::CityV12);
        configure(view,fixture,budget ? fixture.supplement:broken);view.initialize(window,renderer);
        const auto stats=view.fire_inspector_display_stats();
        check(!stats.configured && !stats.active && !stats.fallback_reason.empty() &&
            stats.additional_assets==0 && stats.additional_bytes==0,"bad optional role partially activated");
        const auto core=view.walker_display_stats();
        check(core.schema_version==2 && core.decoded_assets==1 && core.texture_uploads==1 &&
            core.roles[0].configured && core.roles[1].configured && core.roles[2].configured && !core.roles[3].configured,
            "optional failure disabled/reuploaded/reindexed valid core roles");
        const auto fire=view.fire_display_stats();
        check(fire.animated && fire.unique_assets==2 && fire.texture_uploads==2 && fire.logical_bytes==1504,
            "optional failure displaced valid fire textures/bytes");
        if (budget) check(stats.fallback_reason.find("budget")!=std::string::npos,"aggregate budget fixture did not reject by bytes");
        view.shutdown();check(openemperor::WalkerSpriteSet::live_texture_count()==0 &&
            openemperor::FireSpriteSet::live_texture_count()==0,"optional failure session leaked textures");
    }
    View custom(session(fixture),false,sim::RulesProfile::CityV12);
    configure(custom,fixture,fixture.supplement,openemperor::VisualProfileSource::Custom);custom.initialize(window,renderer);
    check(!custom.fire_inspector_display_stats().configured && custom.walker_texture_count()==1 &&
        custom.fire_inspector_display_stats().fallback_reason.find("Custom")!=std::string::npos,
        "explicit legacy custom override secretly gained builtin Inspector");custom.shutdown();
}
void live_hit(View& view,sim::CourierId id) {
    const auto before=view.fire_inspector_display_stats();render_pure(view);
    const auto after=view.fire_inspector_display_stats();
    check(openemperor::walker_live_visible(view.world().courier(id)) &&
        (before.active ? after.draws==before.draws+1:after.fallback_draws==before.fallback_draws+1),
        "active trip did not submit its sprite/marker exactly once");
    auto hit=ground(view,id);
    if (before.active) hit.y-=8;
    else { const auto number=static_cast<unsigned>(id);hit.x+=number==1 ? -5:number==2 ? 5:number==3 ? 0:number==4 ? -10:number==5 ? 10:14; }
    click(view,hit);
    check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(id),
        "drawn active sprite/marker did not retain its live Walker hit");
}
void hidden_home(View& view,sim::CourierId id,sim::BuildingId watch,SDL_Renderer* renderer) {
    const auto before=view.fire_inspector_display_stats();render_pure(view);
    const auto after=view.fire_inspector_display_stats();
    check(!openemperor::walker_live_visible(view.world().courier(id)) &&
        after.draws==before.draws && after.fallback_draws==before.fallback_draws &&
        view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(id),
        "home Idle Inspector retained a live draw/counter/selection");
    const auto output=read(renderer);const auto point=ground(view,id);
    check(pixel(output.get(),static_cast<int>(point.x),static_cast<int>(point.y-8))!=Pixel{0,0,255,255},
        "Idle Inspector torso remained outside its Watch");
    for (const auto offset:{openemperor::scene::Point{0,-8},openemperor::scene::Point{-5,0}}) {
        click(view,{point.x+offset.x,point.y+offset.y});
        check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(id),
            "hidden sprite/marker retained an invisible Walker hit");
    }
    click(view,point);
    std::string inspection;
    for (const auto& line:view.inspection_lines()) { inspection+=line;inspection+=' '; }
    check(view.input_diagnostic_state().selected_walker==0 &&
        view.input_diagnostic_state().selected_cell==view.world().building(watch).cell &&
        inspection.find("Fire Watch #")!=std::string::npos &&
        inspection.find("Inspector phase: Idle at source")!=std::string::npos &&
        inspection.find("Workers assigned")!=std::string::npos,
        "hidden logical Inspector made Watch selection/diagnostics unavailable");
}
void visibility_lifecycle_checks(SDL_Window* window,SDL_Renderer* renderer,bool inspector_assets) {
    authored::Fixture fixture;
    View view(session(fixture),false,sim::RulesProfile::CityV12);
    configure(view,fixture,inspector_assets ? fixture.supplement:fs::path{});view.initialize(window,renderer);
    auto control=sim::World::restore(view.world().snapshot(),view.buildable_mask());
    const auto command=[&](sim::Command input) {
        const auto actual=view.execute(input),plain=control.execute(input);
        check(actual.accepted && plain.accepted && actual.changed==plain.changed &&
            view.world().snapshot()==control.snapshot(),"visibility changed paid command/full World state");
    };
    const auto step=[&] { view.tick_once();control.tick();check(view.world().snapshot()==control.snapshot(),"visibility changed full tick outcome"); };
    command({sim::CommandType::PlaceFireWatch,{111,111}});
    const auto watch=*view.world().building_owner_at({111,111});const auto id=inspector_id(view,watch);
    check(view.world().workers_assigned(watch)==0 && view.world().courier_position(id) &&
        view.world().courier(id).enabled && view.world().courier(id).phase==sim::CourierPhase::IdleAtWorkshop,
        "unstaffed Watch changed existing logical Courier/position");
    zoom_to(view,id,1.0);toggle(view,SDLK_F1);hidden_home(view,id,watch,renderer);
    check(view.last_courier_draws()==0,"new unstaffed Watch submitted a live Inspector");
    toggle(view,SDLK_F2);hidden_home(view,id,watch,renderer);
    check(view.last_courier_draws()==0,"F2 exposed home Idle fallback marker");
    toggle(view,SDLK_F2);
    if (inspector_assets) {
        const auto stable=view.world().snapshot();toggle(view,SDLK_F3);
        for (int role=0;role<3;++role) toggle(view,SDLK_V);
        render_pure(view);
        check(view.last_courier_draws()==0 && view.fire_inspector_display_stats().draws==0 &&
            view.world().snapshot()==stable,"separate Inspector idle asset preview became a live World instance");
        toggle(view,SDLK_F3);
    }
    const auto idle_saved=view.world().snapshot();toggle(view,SDLK_F5);
    check(fs::is_regular_file(fixture.root/"save.json"),"Idle save missing");
    command({sim::CommandType::PlaceHousehold,{119,115}});
    check(view.world().workers_assigned(watch)==2 && view.world().courier(id).phase==sim::CourierPhase::IdleAtWorkshop,
        "paid population did not staff Watch before actual dispatch");
    hidden_home(view,id,watch,renderer);
    toggle(view,SDLK_F9);control=sim::World::restore(idle_saved,view.buildable_mask());
    check(view.world().snapshot()==control.snapshot(),"Idle reload changed logical state");
    hidden_home(view,id,watch,renderer);
    command({sim::CommandType::PlaceHousehold,{119,115}});
    const auto target=*view.world().building_owner_at({119,115});
    for (int x=111;x<=118;++x) command({sim::CommandType::PlaceRoad,{x,112}});
    for (int y=113;y<=115;++y) command({sim::CommandType::PlaceRoad,{118,y}});
    hidden_home(view,id,watch,renderer);step();
    check(view.world().ticks()==1 && view.world().burning_buildings()==0 &&
        view.world().courier(id).phase==sim::CourierPhase::ToWarehouse &&
        view.world().building(target).fire_protection_until_tick==0,
        "preventive dispatch without fire was changed or protection granted early");
    int budget=30;
    while (budget-- && !(view.world().courier(id).path_vertex>=2 && view.world().courier(id).edge_progress==0)) step();
    check(budget>=0,"preventive patrol failed to reach clear road waypoint");
    live_hit(view,id);
    if (inspector_assets) { toggle(view,SDLK_F2);live_hit(view,id);toggle(view,SDLK_F2);live_hit(view,id); }
    command(sim::set_building_operation(watch,false));
    check(view.world().workers_assigned(watch)==0,"paused underway Watch retained assigned workers");live_hit(view,id);
    command(sim::set_building_operation(watch,true));
    command({sim::CommandType::PlacePottery,{110,118}});const auto pottery=*view.world().building_owner_at({110,118});
    command(sim::set_building_workforce_priority(pottery,sim::WorkforcePriority::High));
    check(view.world().building(watch).operating_enabled && view.world().workers_assigned(watch)==0,
        "ordinary priority did not remove underway Watch staffing");
    const auto old_role_before=view.walker_display_stats().roles[1].draws;live_hit(view,id);
    check(view.walker_display_stats().roles[1].draws==old_role_before+1,
        "Inspector visibility change hid an older idle Pottery sprite");
    command({sim::CommandType::RemoveRoad,{118,114}});
    check(view.world().courier(id).route_pending && view.world().courier(id).path.size()==1 &&
        view.world().courier(id).phase==sim::CourierPhase::ToWarehouse,
        "ordinary road interruption did not create a real one-cell waiting trip");
    const auto waiting_position=view.world().courier_position(id);step();live_hit(view,id);
    render_pure(view);const auto waiting_pixels=read(renderer);const auto waiting_ground=ground(view,id);
    const auto same_waiting_pixels=[&] {
        render_pure(view);const auto actual=read(renderer);
        for (int y=-14;y<=3;++y) for (int x=-6;x<=6;++x)
            check(pixel(actual.get(),static_cast<int>(waiting_ground.x)+x,static_cast<int>(waiting_ground.y)+y)==
                pixel(waiting_pixels.get(),static_cast<int>(waiting_ground.x)+x,static_cast<int>(waiting_ground.y)+y),
                "one-cell waiting/reloaded Inspector changed its static visible pixels");
    };
    const auto waiting_saved=view.world().snapshot();toggle(view,SDLK_F5);
    for (int repeat=0;repeat<5;++repeat) {
        step();live_hit(view,id);same_waiting_pixels();
        check(view.world().courier_position(id)==waiting_position,"waiting figure moved");
    }
    toggle(view,SDLK_F9);control=sim::World::restore(waiting_saved,view.buildable_mask());
    check(view.world().snapshot()==control.snapshot() && view.world().courier_position(id)==waiting_position &&
        view.input_diagnostic_state().selected_walker==0,"waiting reload changed state/position or retained stale selection");
    live_hit(view,id);same_waiting_pixels();
    command({sim::CommandType::PlaceRoad,{118,114}});live_hit(view,id);
    bool arrived=false;budget=200;openemperor::scene::Point stale_hit{};
    while (budget-- && view.world().courier(id).phase!=sim::CourierPhase::IdleAtWorkshop) {
        const auto phase=view.world().courier(id).phase;
        step();const auto& courier=view.world().courier(id);
        if (phase==sim::CourierPhase::ToWarehouse && courier.phase==sim::CourierPhase::Returning) {
            arrived=true;
            check(view.world().burning_buildings()==0 && view.world().building(target).fire_protection_until_tick==view.world().ticks()+2400,
                "preventive arrival failed existing protection semantics");
            command(sim::set_building_operation(watch,false));
        }
        if (courier.phase!=sim::CourierPhase::IdleAtWorkshop) {
            const auto before=view.fire_inspector_display_stats();render_pure(view);const auto after=view.fire_inspector_display_stats();
            check(openemperor::walker_live_visible(courier) &&
                (before.active ? after.draws==before.draws+1:after.fallback_draws==before.fallback_draws+1),
                "unassigned/paused/outgoing/Returning trip disappeared");
            stale_hit=ground(view,id);if (before.active) stale_hit.y-=8;else stale_hit.x-=5;
        }
    }
    check(arrived && budget>=0 && view.input_diagnostic_state().selected_walker==0 &&
        !view.input_diagnostic_state().selected_cell,"completed return retained a selected Walker/cell before redraw");
    click(view,stale_hit);
    check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(id),
        "pre-redraw cached hit resurrected returned Idle Inspector");
    hidden_home(view,id,watch,renderer);
    std::cout<<(inspector_assets ? "sprite":"missing-assets marker")
             <<" visibility: unstaffed/staffed Idle hidden, preventive dispatch1, real single-cell wait/reload, paused+unassigned trip, protection/return neutral\n";
    view.shutdown();check(openemperor::WalkerSpriteSet::live_texture_count()==0 &&
        openemperor::FireSpriteSet::live_texture_count()==0,"visibility lifecycle leaked prepared resources");
}
void last_fire_return_checks(SDL_Window* window,SDL_Renderer* renderer) {
    authored::Fixture fixture;View view(session(fixture),false,sim::RulesProfile::CityV12);
    configure(view,fixture,fixture.supplement);view.initialize(window,renderer);
    const auto target=put(view,sim::CommandType::PlaceHousehold,{119,115});
    const auto watch=put(view,sim::CommandType::PlaceFireWatch,{111,111});
    check(view.execute(sim::set_building_operation(watch,false)).accepted,"pause sole-fire fixture Watch");
    for (int x=111;x<=118;++x) check(view.execute({sim::CommandType::PlaceRoad,{x,112}}).accepted,"sole-fire horizontal road");
    for (int y=113;y<=115;++y) check(view.execute({sim::CommandType::PlaceRoad,{118,y}}).accepted,"sole-fire curved road");
    const auto id=inspector_id(view,watch);auto control=sim::World::restore(view.world().snapshot(),view.buildable_mask());
    const auto step=[&] { view.tick_once();control.tick();check(view.world().snapshot()==control.snapshot(),"visibility changed sole-fire command/tick state"); };
    while (view.world().ticks()<2000) step();
    check(view.world().burning_buildings()==1 && view.world().building_on_fire(target),"sole natural fire fixture");
    check(view.execute(sim::set_building_operation(watch,true)).accepted && control.execute(sim::set_building_operation(watch,true)).accepted,"resume sole-fire Watch");
    step();check(view.world().building_on_fire(target) && view.world().courier(id).phase==sim::CourierPhase::ToWarehouse,
        "dispatch changed sole-fire extinguishing semantics");
    zoom_to(view,id,1.0);toggle(view,SDLK_F1);
    int budget=30;while (budget-- && !(view.world().courier(id).path_vertex>=2 && view.world().courier(id).edge_progress==0)) step();
    check(budget>=0,"fire trip failed clear waypoint");live_hit(view,id);
    budget=150;while (budget-- && view.world().courier(id).phase==sim::CourierPhase::ToWarehouse) step();
    check(budget>=0 && view.world().courier(id).phase==sim::CourierPhase::Returning && view.world().burning_buildings()==0 &&
        view.world().building(target).fire_until_tick==view.world().ticks() &&
        view.world().building(target).fire_protection_until_tick==view.world().ticks()+2400,"last fire did not end on actual unchanged arrival");
    const auto arrival_tick=view.world().ticks();
    check(view.execute(sim::set_building_operation(watch,false)).accepted && control.execute(sim::set_building_operation(watch,false)).accepted,"pause next sole-fire patrol");
    budget=150;openemperor::scene::Point last_hit{};
    while (budget-- && view.world().courier(id).phase!=sim::CourierPhase::IdleAtWorkshop) {
        check(view.world().burning_buildings()==0 && openemperor::walker_live_visible(view.world().courier(id)),"Returning hidden after last fire ended");
        const auto before=view.fire_inspector_display_stats();render_pure(view);
        check(view.fire_inspector_display_stats().draws==before.draws+1,"last-fire return did not draw its figure");
        last_hit=ground(view,id);last_hit.y-=8;step();
    }
    check(budget>=0 && view.input_diagnostic_state().selected_walker==0,"sole-fire return left stale selection");
    check(view.execute(sim::demolish_building(watch)).accepted && control.execute(sim::demolish_building(watch)).accepted &&
        view.world().snapshot()==control.snapshot(),"ordinary idle Watch demolition changed control state");
    // The submitted frame still contains the now-erased CourierId. Inspect it
    // before any redraw to verify safe lookup, not only ordinary hit rebuilding.
    click(view,last_hit);check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(id),
        "cached erased Courier hit resurrected demolished Watch Inspector");
    render_pure(view);
    std::cout<<"sole natural fire: dispatch2001, unchanged arrival"<<arrival_tick<<", burning0 return visible, cached erased hit safe\n";
    view.shutdown();
}
void production_checks(SDL_Window* window,SDL_Renderer* renderer) {
    authored::Fixture fixture;
    const auto profile=openemperor::assets::load_walker_visual_profile(fixture.data,fixture.supplement);
    View view(session(fixture),false,sim::RulesProfile::CityV12);
    configure(view,fixture,fixture.supplement);view.initialize(window,renderer);
    check(view.fire_inspector_display_stats().active && view.walker_texture_count()==8 &&
        view.fire_inspector_display_stats().additional_assets==7 &&
        view.fire_inspector_display_stats().additional_bytes==5376,"normal optional shared tail activation");
    const auto target=put(view,sim::CommandType::PlaceHousehold,{119,115});
    put(view,sim::CommandType::PlaceHousehold,{110,118});
    const auto watch=put(view,sim::CommandType::PlaceFireWatch,{111,111});
    check(view.execute(sim::set_building_operation(watch,false)).accepted,"pause initial Watch patrol");
    for (int x=111;x<=118;++x) check(view.execute({sim::CommandType::PlaceRoad,{x,112}}).accepted,"horizontal paid road");
    for (int y=113;y<=115;++y) check(view.execute({sim::CommandType::PlaceRoad,{118,y}}).accepted,"curved paid road");
    const auto id=inspector_id(view,watch);
    auto control=sim::World::restore(view.world().snapshot(),view.buildable_mask());
    const auto step=[&] { view.tick_once();control.tick();check(view.world().snapshot()==control.snapshot(),"sprite changed complete simulation outcome"); };
    while (view.world().ticks()<2000) step();
    check(view.world().building_on_fire(target) && view.world().courier(id).phase==sim::CourierPhase::IdleAtWorkshop,"natural incident fixture");
    check(view.execute(sim::set_building_operation(watch,true)).accepted &&
        control.execute(sim::set_building_operation(watch,true)).accepted,"normal Watch resume");
    step();check(view.world().building_staffed(watch) && view.world().building_on_fire(target) &&
        view.world().courier(id).phase==sim::CourierPhase::ToWarehouse,"staffed dispatch prematurely extinguished");
    int road_budget=30;
    while (road_budget-- && !(view.world().courier(id).path_vertex>=2 &&
        view.world().courier(id).edge_progress==0)) step();
    check(road_budget>=0,"Inspector did not reach a clear road waypoint");
    zoom_to(view,id,1.0);render_pure(view);
    const auto saved_tick=view.world().ticks();const auto saved=view.world().snapshot();
    const auto point=ground(view,id);const auto before=read(renderer);
    const auto first_pose=openemperor::walker_pose(view.world().courier(id),saved_tick,profile);
    check(first_pose.moving && first_pose.frame,"moving production pose missing");
    check(pixel(before.get(),static_cast<int>(point.x),static_cast<int>(point.y-8))==Pixel{0,0,255,255},"Inspector torso missing at authoritative position");
    check(pixel(before.get(),static_cast<int>(point.x),static_cast<int>(point.y-2))==Pixel{255,255,255,255},"production foot anchor missing");
    toggle(view,SDLK_F2);render_pure(view);const auto marker=read(renderer);
    bool extra_square_absent=false;
    // Every selected canvas ends at the ground reference. The old diagnostic
    // square protrudes below it, including its legacy per-instance X shift.
    for (int y=1;y<=3;++y) for (int x=-18;x<=18;++x)
        extra_square_absent=extra_square_absent ||
            pixel(before.get(),static_cast<int>(point.x)+x,static_cast<int>(point.y)+y)!=
            pixel(marker.get(),static_cast<int>(point.x)+x,static_cast<int>(point.y)+y);
    check(!view.fire_inspector_display_stats().active && view.world().snapshot()==saved &&
        extra_square_absent,"active sprite retained extra FI square or F2 changed authority");
    toggle(view,SDLK_F2);render_pure(view);
    const auto same_crop=[&](SDL_Surface* actual) {
        for (int y=-14;y<=3;++y) for (int x=-6;x<=6;++x)
            check(pixel(actual,static_cast<int>(point.x)+x,static_cast<int>(point.y)+y)==
                pixel(before.get(),static_cast<int>(point.x)+x,static_cast<int>(point.y)+y),"paused/reloaded Inspector crop changed");
    };
    for (int repeat=0;repeat<8;++repeat) { render_pure(view);const auto same=read(renderer);same_crop(same.get()); }
    toggle(view,SDLK_F5);
    check(fs::is_regular_file(fixture.root/"save.json"),"ordinary moving save missing");
    step();step();render_pure(view);toggle(view,SDLK_F9);
    check(view.world().snapshot()==saved && view.world().ticks()==saved_tick,"moving reload changed full state/path/deadlines");
    control=sim::World::restore(saved,view.buildable_mask());render_pure(view);const auto loaded=read(renderer);same_crop(loaded.get());
    for (const double zoom:{1.0,1.15,2.0,4.0,1.0}) {
        zoom_to(view,id,zoom);render_pure(view);const auto output=read(renderer);const auto foot=ground(view,id);
        check(pixel(output.get(),static_cast<int>(foot.x),static_cast<int>(foot.y-8*zoom))==Pixel{0,0,255,255} &&
            pixel(output.get(),static_cast<int>(foot.x),static_cast<int>(foot.y-2*zoom))==Pixel{255,255,255,255},"production zoom lost torso/foot");
    }
    toggle(view,SDLK_F1);render_pure(view);auto hit=ground(view,id);hit.y-=8;click(view,hit);
    check(view.input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(id) &&
        contains(view.inspection_lines(),"FireInspector") && contains(view.inspection_lines(),"Curated clip:"),"visible alpha did not pick Inspector role/phase/clip");
    render_pure(view);hit=ground(view,id);hit.x-=5;hit.y-=15;click(view,hit);
    check(view.input_diagnostic_state().selected_walker!=static_cast<std::uint32_t>(id),"transparent corner blocked ground picking");
    const auto before_diagnostics=view.world().snapshot();toggle(view,SDLK_F3);
    for (int role=0;role<4;++role) { toggle(view,SDLK_V);render_pure(view); }
    toggle(view,SDLK_F3);toggle(view,SDLK_F1);check(view.world().snapshot()==before_diagnostics,"F3/V changed authority");
    std::array<bool,4> directions{};directions[0]=true;
    bool pixels_changed=false,arrived=false;int budget=300;
    while (budget-- && (!arrived || view.world().courier(id).phase!=sim::CourierPhase::IdleAtWorkshop)) {
        const auto phase_before=view.world().courier(id).phase;step();const auto& courier=view.world().courier(id);
        const auto pose=openemperor::walker_pose(courier,view.world().ticks(),profile);
        if (pose.moving) directions[openemperor::assets::direction_index(*pose.direction)]=true;
        if (!arrived && phase_before==sim::CourierPhase::ToWarehouse && courier.phase==sim::CourierPhase::Returning) {
            arrived=true;check(!view.world().building_on_fire(target) &&
                view.world().building(target).fire_until_tick==view.world().ticks() &&
                view.world().building(target).fire_protection_until_tick==view.world().ticks()+2400,"actual arrival fire/protection deadlines changed");
            check(view.execute(sim::set_building_operation(watch,false)).accepted &&
                control.execute(sim::set_building_operation(watch,false)).accepted,"pause new patrol after arrival");
        }
        render_pure(view);
        if (pose.moving && view.world().ticks()%2==0) {
            const auto sample=read(renderer);const auto feet=ground(view,id);
            pixels_changed=pixels_changed || pixel(sample.get(),static_cast<int>(feet.x-4),static_cast<int>(feet.y-9))!=
                pixel(sample.get(),static_cast<int>(feet.x+4),static_cast<int>(feet.y-9));
        }
    }
    check(arrived && budget>=0 && pixels_changed && std::all_of(directions.begin(),directions.end(),[](bool seen){return seen;}),
        "production missed gait pixels, four directions, arrival or return");
    check(view.fire_display_stats().animated && view.fire_inspector_display_stats().fallback_draws>0 &&
        view.walker_texture_count()==8,"production resource bookkeeping after comparison/arrival");
    view.shutdown();check(openemperor::WalkerSpriteSet::live_texture_count()==0 &&
        openemperor::FireSpriteSet::live_texture_count()==0,"production session leaked Inspector/fire textures");
}
}
int main(int argc,char* argv[]) {
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        check(argc==1 || metal,"usage: Inspector view tests [metal]");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal ? "cocoa":"dummy") &&
            SDL_SetHint(SDL_HINT_RENDER_DRIVER,metal ? "metal":"software") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
        SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("Inspector production pixels",1280,720,SDL_WINDOW_HIDDEN,&window,&renderer),"renderer creation");
        check(std::string_view(SDL_GetRendererName(renderer))==(metal ? "metal":"software"),"actual backend mismatch");
        auto target=std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)>(
            SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,720),SDL_DestroyTexture);
        check(bool(target) && SDL_SetRenderTarget(renderer,target.get()),"own pre-Present pixel target");
        optional_failure_checks(window,renderer);visibility_lifecycle_checks(window,renderer,true);
        visibility_lifecycle_checks(window,renderer,false);last_fire_return_checks(window,renderer);production_checks(window,renderer);
        check(SDL_SetRenderTarget(renderer,nullptr),"release pixel target");target.reset();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"production Inspector activation/fallback/core+fire budgets/pixels/alpha picking/zoom/arrival/return passed\n";return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
