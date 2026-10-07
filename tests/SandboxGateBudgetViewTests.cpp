#include "app/SandboxView.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/EmperorMap.h"
#include "maps/LandscapeProvenance.h"
#include "maps/OriginalMapEntities.h"
#include "maps/TerrainRenderPlan.h"

#include <SDL3/SDL.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace openemperor::gate_budget_test {
void fail_next_preflight();
bool injected_failure_consumed();
}

namespace {
namespace maps=openemperor::maps;
namespace sim=openemperor::simulation;
using View=openemperor::SandboxView;
using Bytes=std::vector<std::uint8_t>;
constexpr unsigned width=228;
constexpr sim::Cell road_only{110,110};
constexpr sim::Cell gate_origin{112,112};
constexpr sim::Command purchase{sim::CommandType::PlaceClaySource,{100,117}};

void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void put(Bytes& b,std::size_t at,std::uint32_t value,unsigned n=4) {
    for (unsigned i=0;i<n;++i) b.at(at+i)=static_cast<std::uint8_t>(value>>(i*8U));
}
void append(Bytes& b,std::uint32_t value,unsigned n=4) {
    const auto at=b.size();b.resize(at+n);put(b,at,value,n);
}
void write(const std::filesystem::path& path,const Bytes& b) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
    check(bool(out),"authored fixture write");
}
std::size_t index(sim::Cell c) { return static_cast<std::size_t>(c.y)*width+static_cast<unsigned>(c.x); }
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("openemperor-gate-budget-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { std::filesystem::create_directories(path/"Cities");std::filesystem::create_directories(path/"DATA"); }
    ~Temp() { std::error_code error;std::filesystem::remove_all(path,error); }
};

// Authored complete manager and pixels. The Gate body may remain historical;
// road permissions must not depend on whether a presentation asset is ready.
maps::StoredMapSession fixture(const Temp& temp) {
    Bytes manager;append(manager,1,2);append(manager,1);
    append(manager,0xffff,2);append(manager,0,2);append(manager,10,2);
    constexpr std::string_view name="cGateHouse";manager.insert(manager.end(),name.begin(),name.end());
    Bytes base(181);put(base,0,4,2);base[2]=3;base[5]=1;
    put(base,8,40,2);put(base,10,40,2);put(base,12,112*width+112);put(base,16,130,2);
    put(base,112,1);put(base,161,7);manager.insert(manager.end(),base.begin(),base.end());
    append(manager,1,2);append(manager,1,2);manager.resize(manager.size()+126);append(manager,1,2);
    Bytes raw(maps::original_entities_logical_offset+manager.size());
    const Bytes signature{5,0,0xfe,0xca,0,0,2,0};std::copy(signature.begin(),signature.end(),raw.begin());
    put(raw,84,84);const maps::MapGeometry geometry{84};
    for (unsigned y=0;y<width;++y) for (unsigned x=0;x<width;++x) {
        const auto at=static_cast<std::size_t>(y)*width+x;
        put(raw,maps::terrain_logical_offset+4*at,geometry.contains({x,y}) ? 0x80:0x80000);
        raw[maps::landscape_height_offset+at]=1;
    }
    put(raw,maps::terrain_logical_offset+4*index(road_only),0xc0);
    for (int y=0;y<5;++y) for (int x=0;x<3;++x)
        put(raw,maps::terrain_logical_offset+4*index({gate_origin.x+x,gate_origin.y+y}),0x8008);
    std::copy(manager.begin(),manager.end(),raw.begin()+maps::original_entities_logical_offset);
    Bytes packed;append(packed,0xfedcbaaa);
    for (std::size_t at=0;at<raw.size();at+=16384) {
        const auto n=std::min<std::size_t>(16384,raw.size()-at);
        uLongf size=compressBound(static_cast<uLong>(n));Bytes compressed(size);
        check(compress2(compressed.data(),&size,raw.data()+at,static_cast<uLong>(n),6)==Z_OK,"authored map compression");
        append(packed,0);append(packed,static_cast<std::uint32_t>(size));append(packed,static_cast<std::uint32_t>(n));
        packed.insert(packed.end(),compressed.begin(),compressed.begin()+static_cast<std::ptrdiff_t>(size));
    }
    write(temp.path/"Cities/Authored.map",packed);
    Bytes sg3(40680+64);put(sg3,0,static_cast<std::uint32_t>(sg3.size()));put(sg3,4,213);
    put(sg3,12,1);put(sg3,16,1);put(sg3,20,1);
    constexpr std::string_view group="Zeus_system.bmp";std::copy(group.begin(),group.end(),sg3.begin()+680);
    put(sg3,680+124,1);put(sg3,680+128,1);put(sg3,40680+4,3200);put(sg3,40680+8,3200);
    put(sg3,40680+20,78,2);put(sg3,40680+22,40,2);put(sg3,40680+50,30,2);sg3[40680+55]=1;
    write(temp.path/"DATA/grass.sg3",sg3);Bytes pixels(3200);
    for (std::size_t i=0;i<pixels.size();i+=2) put(pixels,i,0x03e0,2);
    write(temp.path/"DATA/grass.555",pixels);
    const auto container=maps::EmperorContainer::open(temp.path/"Cities/Authored.map");
    auto map=maps::read_emperor_map(container,0);maps::StoredGraphicsPlan plan;
    plan.data_root=temp.path;plan.map_relative="Cities/Authored.map";plan.border=geometry.border;
    plan.profile=maps::StoredGraphicsProfile::Slot8;plan.height_bytes.assign(width*width,1);
    plan.cell_by_storage.resize(width*width);plan.status_by_storage.assign(width*width,maps::StoredStatus::Excluded);
    openemperor::assets::AssetRecord record;record.id={"DATA/grass.sg3",0};record.width=78;record.height=40;
    record.data_length=record.uncompressed_length=3200;record.image_type=30;
    maps::StoredAsset asset;asset.record=record;plan.assets.push_back(std::move(asset));
    for (unsigned y=100;y<=127;++y) for (unsigned x=100;x<=127;++x) {
        maps::StoredCell cell;cell.storage={x,y};cell.cell_index=static_cast<std::size_t>(y)*width+x;
        cell.terrain_raw=map.terrain_raw.values[cell.cell_index];cell.status=maps::StoredStatus::DecodePending;
        cell.asset_index=0;cell.footprint_index=plan.footprints.size();
        cell.world=maps::terrain_world(cell.storage,plan.border);cell.image_origin=maps::stored_image_origin(cell.world,78,40);
        plan.cell_by_storage[cell.cell_index]=plan.cells.size();plan.status_by_storage[cell.cell_index]=cell.status;
        maps::PlacedFootprint footprint;footprint.id=plan.footprints.size();footprint.asset_index=0;
        footprint.origin=cell.storage;footprint.cell_indices={plan.cells.size()};
        footprint.image_origin=cell.image_origin;footprint.status=maps::StoredStatus::DecodePending;
        plan.cells.push_back(cell);plan.footprints.push_back(footprint);
    }
    return {std::move(map),std::move(plan)};
}

SDL_Event key(SDL_Keycode value) { SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=value;return e; }
void click(View& view,sim::Cell cell,bool& running) {
    const auto p=view.camera().world_to_screen(maps::terrain_ground(
        {static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},72));
    check(view.layout().map.contains(p.x,p.y) && view.pick(p)==cell,"fixture map click location");
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(p.x);e.button.y=static_cast<float>(p.y);view.handle_event(e,running);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(e,running);
}
void place_road(View& view,bool& running) {
    view.set_tool(1);click(view,road_only,running);
    check(!view.budget_warning_pending() && view.world().object_at(road_only)==sim::Object::Road &&
        view.world().treasury()==1298 && view.world().command_sequence()==1,"normal road-only purchase");
}
void permissions_unchanged(const View& view,const std::shared_ptr<const sim::MapPermissions>& policy,
                           const std::string& canonical) {
    check(view.world().map_permissions()==policy && policy->canonical_state()==canonical,
        "UI budget path changed immutable map permissions");
    check(policy->cell_height(road_only)==1 && policy->fixed_passage({113,114}) &&
        policy->transport_edge_allowed({111,114},{112,114}) &&
        !policy->transport_edge_allowed({112,113},{112,114}),"gate/height/edge authority lost");
}
void no_warning_purchase(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp;View view(fixture(temp),false,sim::RulesProfile::CityV16,3);view.initialize(window,renderer);
    bool running=true;const auto policy=view.world().map_permissions();const auto canonical=policy->canonical_state();
    check(policy->road_allowed(road_only) && !policy->building_allowed(road_only),"road-only input fixture");
    place_road(view,running);const auto before=view.world().snapshot();
    check(before.taxes_collected_total==0,"budget path fixture already collected tax");
    view.set_tool(2);
    try { click(view,purchase.cell,running); }
    catch (const std::invalid_argument&) {
        check(view.world().snapshot()==before,"failed purchase preflight changed actual source World");
        permissions_unchanged(view,policy,canonical);
        std::cerr<<"normal UI Road→Clay preflight threw; actual source snapshot/policy unchanged\n";
        throw;
    }
    check(!view.budget_warning_pending() && view.world().object_at(purchase.cell)==sim::Object::ClaySource &&
        view.world().command_sequence()==before.command_sequence+1 &&
        view.world().treasury()==before.treasury-120 && view.world().ticks()==before.ticks,
        "normal building UI purchase after road-only cell");
    permissions_unchanged(view,policy,canonical);view.shutdown();
}
void modal_purchase(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp;View view(fixture(temp),false,sim::RulesProfile::CityV16,3);view.initialize(window,renderer);
    bool running=true;place_road(view,running);
    for (const sim::Cell p:std::array<sim::Cell,5>{{{100,101},{103,101},{106,101},{109,101},{112,101}}}) {
        const auto result=view.request_execute({sim::CommandType::PlaceHousehold,p});
        if (view.budget_warning_pending()) check(view.resolve_budget_warning(true),"paid House fixture confirmation");
        else check(result.accepted && result.changed,"paid House fixture purchase");
    }
    const auto before=view.world().snapshot();const auto policy=view.world().map_permissions();
    const auto canonical=policy->canonical_state();
    const sim::Command warned{sim::CommandType::PlaceHousehold,{115,101}};
    const auto requested=view.request_execute(warned);
    check(!requested.accepted && view.budget_warning_pending() && view.world().snapshot()==before,
        "rule3 starter budget warning mutated source before confirmation");
    check(view.budget_warning()->purchase_cost==80 && view.budget_warning()->funds_after_purchase==before.treasury-80,
        "rule3 budget warning values");
    view.tick_once();view.update(0.25);check(view.render() && view.world().snapshot()==before,"budget modal freeze");
    view.handle_event(key(SDLK_ESCAPE),running);
    check(!view.budget_warning_pending() && view.world().snapshot()==before,"rule3 budget cancel changed source");
    permissions_unchanged(view,policy,canonical);
    check(!view.request_execute(warned).accepted && view.budget_warning_pending(),"rule3 budget reopen");
    view.handle_event(key(SDLK_Y),running);
    check(!view.budget_warning_pending() && view.world().object_at(warned.cell)==sim::Object::Household &&
        view.world().command_sequence()==before.command_sequence+1 &&
        view.world().treasury()==before.treasury-80 && view.world().ticks()==before.ticks &&
        view.world().next_building_id()==before.next_building_id+1,"rule3 budget confirmation did not commit exactly once");
    permissions_unchanged(view,policy,canonical);
    const auto committed=view.world().snapshot();
    const auto rejected=view.request_execute({sim::CommandType::PlaceClaySource,{112,113}});
    check(!rejected.accepted && !view.budget_warning_pending() && view.world().snapshot()==committed,
        "protected gate purchase failed to preserve source");
    view.shutdown();
}
void failed_preflight(SDL_Window* window,SDL_Renderer* renderer) {
    Temp temp;View view(fixture(temp),false,sim::RulesProfile::CityV16,3);view.initialize(window,renderer);
    bool running=true;place_road(view,running);view.set_tool(2);
    const auto before=view.world().snapshot();const auto policy=view.world().map_permissions();
    const auto canonical=policy->canonical_state();
    openemperor::gate_budget_test::fail_next_preflight();
    namespace perf=openemperor::performance;
    perf::set_enabled(true);perf::reset();
    click(view,purchase.cell,running);
    check(openemperor::gate_budget_test::injected_failure_consumed(),"failure seam was not reached through normal UI");
    check(view.world().snapshot()==before && !view.budget_warning_pending() &&
        view.world().object_at(purchase.cell)==sim::Object::Empty,
        "failed budget preflight executed a purchase or left pending modal state");
    const auto input=view.input_diagnostic_state();
    check(!input.map_pressed && !input.ui_pressed && !input.road_drag,
        "failed budget preflight left an armed gesture");
    check(view.last_message()=="Purchase check failed: Authored budget preflight failure",
        "failed budget preflight lacks an actionable message");
    for (const auto counter:{perf::Counter::WorldCopies,perf::Counter::WorldRestores,
            perf::Counter::WorldExecutes,perf::Counter::RouteRefreshes,perf::Counter::BfsCalls,
            perf::Counter::AssetDecodes,perf::Counter::TextureUploads,perf::Counter::FileReads,
            perf::Counter::FileWrites})
        check(perf::counter(counter)==0,"injected preflight failure performed source/asset/navigation work");
    perf::set_enabled(false);permissions_unchanged(view,policy,canonical);
    check(view.render() && view.world().snapshot()==before,"rejected preflight cannot render safely");
    click(view,purchase.cell,running);
    check(view.world().object_at(purchase.cell)==sim::Object::ClaySource &&
        view.world().command_sequence()==before.command_sequence+1 && view.world().treasury()==before.treasury-120,
        "preflight rejection prevented a subsequent valid purchase");
    permissions_unchanged(view,policy,canonical);view.shutdown();
}
}
int main() {
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") &&
            SDL_Init(SDL_INIT_VIDEO),"SDL setup");
        SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("authored Gate budget UI",1100,700,SDL_WINDOW_HIDDEN,&window,&renderer),"SDL window");
        no_warning_purchase(window,renderer);modal_purchase(window,renderer);failed_preflight(window,renderer);
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"rule3 normal UI road-only/building budget, cancel/confirm and immutable authority PASS\n";return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';SDL_Quit();return 1;}
}
