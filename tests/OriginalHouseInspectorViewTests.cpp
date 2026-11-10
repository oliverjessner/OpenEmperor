// Paid authored SDL scenario. Neither original pixels nor mutable World grants
// are used; every resident, timer, stock and delivery comes from normal ticks.
#include "EconomyStartViewFixture.h"
#include "OriginalFinanceFixture.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace orig=oe::original;
namespace f=oe_influence_fixture;
namespace fs=std::filesystem;
using oe::testing::economy::check;
using Section=oe::SandboxView::OriginalHouseSection;
using Overlay=oe::SandboxView::OriginalHouseOverlay;
using Status=oe::original_house_ui::Status;
using Json=nlohmann::json;
constexpr std::array sections{Section::State,Section::Needs,Section::Goods,Section::Visits};
constexpr std::array overlays{Overlay::Off,Overlay::Supply,Overlay::Desirability,Overlay::Risk};
constexpr std::array<const char*,4> section_names{"state","needs","goods","visits"};
constexpr std::array<const char*,4> overlay_names{"off","supply","desirability","risk"};

void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Inspector input unexpectedly closed its session");
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);
}
void boundary(oe::SandboxView& view,SDL_EventType type) { SDL_Event e{};e.type=type;event(view,e); }
oe::scene::Point window_point(SDL_Renderer* renderer,oe::scene::Point render) {
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(render.x),static_cast<float>(render.y),&x,&y),
          "Inspector inverse input coordinate transform failed");
    return {x,y};
}
void button(oe::SandboxView& view,SDL_Renderer* renderer,oe::sandbox_ui::Rect rect,bool down) {
    check(rect.w>0 && rect.h>0,"Inspector control has no actual hit rectangle");
    const auto p=window_point(renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});
    SDL_Event e{};e.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button=SDL_BUTTON_LEFT;e.button.x=static_cast<float>(p.x);e.button.y=static_cast<float>(p.y);event(view,e);
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,oe::sandbox_ui::Rect rect) {
    button(view,renderer,rect,true);button(view,renderer,rect,false);view.update(0);
}
void section(oe::SandboxView& view,SDL_Renderer* renderer,Section wanted) {
    click(view,renderer,view.original_house_section_rect(wanted));
    check(view.original_house_section()==wanted,"Actual mouse section did not select its matching tab");
    check(view.original_house_scroll()==0,"A section change retained another section's scroll");
}
void overlay(oe::SandboxView& view,Overlay wanted) {
    for (unsigned i=0;i<4 && view.original_house_overlay()!=wanted;++i) key(view,SDLK_F10);
    check(view.original_house_overlay()==wanted,"Actual F10 did not select the requested overlay");
}
void pan(oe::SandboxView& view,oe::scene::Point wanted) {
    boundary(view,SDL_EVENT_WINDOW_FOCUS_GAINED);
    for (unsigned step=0;step<1000;++step) {
        const auto p=view.camera().offset;const double dx=wanted.x-p.x,dy=wanted.y-p.y;
        if (std::abs(dx)<1e-6 && std::abs(dy)<1e-6) return;
        oe::SandboxView::PanKeyState held{};
        if (std::abs(dx)>1e-6) {held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    check(false,"Ordinary held-key camera pan did not converge");
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;
    const auto p=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    for (unsigned attempt=0;attempt<3 && std::abs(view.camera().zoom-wanted)>=1e-9;++attempt) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;
        e.wheel.mouse_x=static_cast<float>(p.x);e.wheel.mouse_y=static_cast<float>(p.y);
        e.wheel.y=static_cast<float>(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0);
    }
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Real mouse wheel missed literal1/2/4 camera zoom");
}
oe::scene::Point ground(sim::Cell cell) {
    // Independent flat authored A.map projection; its declared84 border is72.
    return {double(cell.x-cell.y)*40,double(cell.x+cell.y-144)*20+20};
}
void focus(oe::SandboxView& view,sim::BuildingId id) {
    const auto& body=view.world().building(id);
    const auto front=sim::building_front_cell(view.world().profile(),view.world().rule_version(),body.kind,body.cell);
    const auto p=ground(front);const auto map=view.layout().map;const double z=view.camera().zoom;
    pan(view,{map.x+map.w*.5-p.x*z,map.y+map.h*.5-(p.y-28)*z});
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    view.set_tool(5);focus(view,id);view.update(0);check(view.render(),"House selection frame failed");
    const auto& body=view.world().building(id);
    const auto footprint=sim::building_footprint(view.world().profile(),view.world().rule_version(),body.kind);
    std::optional<oe::scene::Point> found;
    // At4x a4x4 body's origin may be above the aperture while its front is
    // visible. Search only its at-most16 real owned cells, front first.
    for (int y=footprint.height-1;y>=0 && !found;--y) for (int x=footprint.width-1;x>=0 && !found;--x) {
        const sim::Cell cell{body.cell.x+x,body.cell.y+y};auto center=ground(cell);center.y-=20;
        const auto p=view.camera().world_to_screen(center);
        for (int dy=-8;dy<=8 && !found;dy+=2) for (int dx=-8;dx<=8 && !found;dx+=2) {
            const oe::scene::Point screen{p.x+dx,p.y+dy};
            if (!view.layout().map.contains(screen.x,screen.y)) continue;
            const auto picked=view.pick(screen);
            if (picked && view.world().building_owner_at(*picked)==id) found=screen;
        }
    }
    check(found.has_value(),"Paid House has no visible selectable owned cell through actual projected picking");
    const auto raw=window_point(renderer,*found);SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;
    e.motion.x=static_cast<float>(raw.x);e.motion.y=static_cast<float>(raw.y);event(view,e);view.update(0);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(raw.x);e.button.y=static_cast<float>(raw.y);event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);
    if (view.selected_building()!=id || !view.original_house_inspector_active()) {
        const auto diagnostic=view.input_diagnostic_state(raw);
        std::cerr<<"House selection expected="<<static_cast<unsigned>(id)<<" actual="<<
            (view.selected_building() ? std::to_string(static_cast<unsigned>(*view.selected_building())):"none")<<
            " active="<<view.original_house_inspector_active()<<" panel="<<view.layout().panel_open<<
            " tool="<<view.tool()<<" raw="<<raw.x<<','<<raw.y<<" screen="<<found->x<<','<<found->y<<
            " ui="<<diagnostic.ui<<" map_pressed="<<diagnostic.map_pressed<<" focused="<<diagnostic.input_focused<<'\n';
    }
    check(view.selected_building()==id && view.original_house_inspector_active(),"Paid House mouse selection did not open the original inspector");
}
void wheel_panel(oe::SandboxView& view,SDL_Renderer* renderer,float notches) {
    const auto rect=view.original_house_content_rect();const auto p=window_point(renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);
    e.wheel.mouse_y=static_cast<float>(p.y);e.wheel.y=notches;event(view,e);view.update(0);
}
std::string joined(const std::vector<std::string>& lines) {
    std::string value;for (const auto& line:lines) {value+=line;value+=' ';}return value;
}
void bounded_text(const oe::SandboxView& view) {
    const auto rect=view.original_house_content_rect();const int unit=view.layout().scale;
    check(rect.w>0 && rect.h>=18*unit,"Inspector leaves no complete glyph row inside its content rectangle");
    check(view.layout().panel.contains(rect.x,rect.y) &&
          view.layout().panel.contains(rect.x+rect.w-1,rect.y+rect.h-1),"Inspector content escapes its actual sidepanel");
    const auto lines=view.original_house_section_lines();check(!lines.empty(),"Inspector section is empty");
    for (const auto& line:lines) {
        check(line.find("...")==std::string::npos,"Inspector lost diagnostic text through ellipsis");
        check(line.size()*static_cast<std::size_t>(10*unit)<=static_cast<std::size_t>(rect.w),
              "Wrapped inspector row exceeds the real SDL debug glyph width");
    }
    check(view.original_house_scroll()>=0 && view.original_house_scroll()<=static_cast<int>(lines.size())*18*unit,
          "Inspector scroll is outside its bounded wrapped rows");
    for (const auto s:sections) {
        const auto tab=view.original_house_section_rect(s);
        check(tab.w>0 && tab.h>0 && view.layout().panel.contains(tab.x,tab.y) &&
              view.layout().panel.contains(tab.x+tab.w-1,tab.y+tab.h-1),"Inspector tab overflows the sidepanel");
    }
}
fs::path walker_profile(const fs::path& data) {
    using namespace oe::testing::economy;
    Bytes archive(40680U+2U*72U,0);u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
    u32(archive,12,2);u32(archive,16,2);u32(archive,20,1);const std::string name="authored-clip.bmp";
    std::copy(name.begin(),name.end(),archive.begin()+680);u32(archive,804,2);
    const auto at=40680U+72U;u32(archive,at,4);u32(archive,at+4,10);
    u16(archive,at+20,2);u16(archive,at+22,2);u16(archive,at+50,256);
    write(data/"DATA/inspector-walker.sg3",archive);
    write(data/"DATA/inspector-walker.555",{0,0,0,0,2,0,0x7c,0,0x7c,2,0,0x7c,0,0x7c});
    const auto path=data/"inspector-walker.json";
    f::write_text(path,R"({"schema_version":1,"mode":"curated_walker_preview","role":"clay","ticks_per_frame":1,
"evidence":"Authored two-pixel clip for the actual F2/F3 inspector boundary",
"frames":[{"alias":"dot","archive":"DATA/inspector-walker.sg3","image_index":1,"foot_anchor":[1,1]}],
"clips":{"pos_x":["dot"],"neg_x":["dot"],"pos_y":["dot"],"neg_y":["dot"]},"idle":"dot"})");return path;
}
fs::path road_profile(const fs::path& data) {
    Json tiles=Json::object();constexpr char hex[]="0123456789abcdef";
    for (unsigned mask=0;mask<16;++mask) tiles[std::string("0x")+hex[mask]]={
        {"archive","DATA/China_Terrain.sg3"},{"image_index",201U},{"ground_anchor",{39,20}},
        {"evidence","Independent authored green terrain diamond used only for F6 gating"}};
    const auto path=data/"inspector-road.json";
    f::write_text(path,Json{{"schema_version",1},{"mode","curated_road_preview"},{"replaces_ground",true},{"tiles",tiles}}.dump(2));
    return path;
}
struct Scene { std::vector<sim::BuildingId> commons;sim::BuildingId elite{},herbalist{}; };
Scene build(oe::SandboxView& view) {
    Scene scene;const auto buy=[&](sim::CommandType type,sim::Cell cell) {
        const auto before=view.world().construction_spent_total();const auto funds=view.world().treasury();
        const auto result=view.execute({type,cell});check(result.accepted && result.changed,result.reason.c_str());
        check(view.world().treasury()==funds-static_cast<std::int64_t>(view.world().construction_spent_total()-before),
              "Paused fixture purchase did not pay its exact construction debit");
        return view.world().building_owner_at(cell);
    };
    for (int x=104;x<=128;++x) buy(sim::CommandType::PlaceRoad,{x,103});
    for (int x:{104,107,110,113,116,119}) scene.commons.push_back(*buy(sim::CommandType::PlaceHousehold,{x,101}));
    buy(sim::CommandType::PlaceMarket,{104,104});scene.herbalist=*buy(sim::CommandType::PlaceHealthPost,{106,104});
    buy(sim::CommandType::PlaceOriginalAncestorShrine,{108,104});buy(sim::CommandType::PlaceOriginalWaterWell,{110,104});
    buy(sim::CommandType::PlaceClaySource,{112,104});buy(sim::CommandType::PlacePottery,{114,104});
    buy(sim::CommandType::PlaceWarehouse,{116,104});const auto farm=*buy(sim::CommandType::PlaceOriginalFoodFarm,{118,104});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{121,104}};field.building_id=farm;
    const auto result=view.execute(field);check(result.accepted && result.changed,result.reason.c_str());
    scene.elite=*buy(sim::CommandType::PlaceOriginalElitePlot,{124,104});
    check(view.world().ticks()==0 && view.world().total_population()==0 &&
          view.world().taxes_collected_total()==0 && view.world().original_goods_balance(orig::Goods::Ceramics).stock==0,
          "Paid inspector scene granted residents, goods or taxes at construction");return scene;
}
Json records(const oe::SandboxView& view) {
    Json result=Json::array();for (const auto& r:view.original_house_overlay_records()) result.push_back({
        {"building_id",static_cast<unsigned>(r.building_id)},{"origin",{r.origin.x,r.origin.y}},
        {"footprint",{r.footprint.width,r.footprint.height}},{"status",static_cast<unsigned>(r.status)},
        {"symbol",std::string(1,r.symbol)},{"value",r.value},{"visible_cells",r.visible_cells},{"marker_visible",r.marker_visible}});
    return result;
}
void overlay_bounds(const oe::SandboxView& view,const Scene& scene) {
    const auto stats=view.original_house_overlay_stats();const auto rows=view.original_house_overlay_records();
    if (view.original_house_overlay()==Overlay::Off) {check(stats.houses==0 && stats.cells==0 && stats.markers==0,"Off overlay drew House diagnostics");return;}
    check(rows.size()==scene.commons.size()+1 && stats.houses==rows.size() && stats.houses<=20 && stats.cells<=320 && stats.markers<=20,
          "Actual overlay does not obey the20-House/320-cell bounds");
    for (const auto& row:rows) {
        const auto& body=view.world().building(row.building_id);
        const auto footprint=sim::building_footprint(view.world().profile(),view.world().rule_version(),body.kind);
        check(row.origin==body.cell && row.footprint==footprint && row.visible_cells<=static_cast<std::size_t>(footprint.width*footprint.height),
              "Overlay changed a paid House origin, footprint or visible cell bound");
        if (row.building_id==scene.elite) check(row.footprint==sim::BuildingFootprint{4,4},"Overlay reduced the actually materialized Elite to2x2");
        if (row.marker_visible) check(view.layout().map.contains(row.screen_ground.x,row.screen_ground.y),"Overlay marker escaped map culling");
    }
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& data,const fs::path& folder,const std::string& name) {
    if (folder.empty()) return;
    check(view.render(),"Inspector capture frame failed");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(frame!=nullptr && SDL_SavePNG(frame.get(),(folder/(name+".png")).string().c_str()),"Inspector PNG capture failed");
    const auto id=*view.selected_building();const auto& home=oe_finance_fixture::house(view.world(),id);
    const auto content=view.original_house_content_rect();
    Json observed{{"capture",name},{"rule_version",view.world().rule_version()},{"development_revision",27},
        {"ticks",view.world().ticks()},{"selected_house",static_cast<unsigned>(id)},
        {"type",static_cast<unsigned>(home.type)},{"occupants",home.occupants},{"food_quality",home.food_quality},
        {"herbalist_timer",home.herbalist},{"ceramics",home.ceramics},{"zoom",view.camera().zoom},
        {"section",static_cast<unsigned>(view.original_house_section())},{"overlay",static_cast<unsigned>(view.original_house_overlay())},
        {"wrapped_section_lines",view.original_house_section_lines()},{"scroll_pixels",view.original_house_scroll()},
        {"content_rect",{content.x,content.y,content.w,content.h}},{"overlay_records",records(view)},
        {"authored_model",true},{"authored_map",true},{"original_art_restored",false},{"native_actions",false},
        {"human_acceptance","NOT_RUN"},{"renderer",SDL_GetRendererName(renderer)},{"video_driver",SDL_GetCurrentVideoDriver()}};
    f::write_text(folder/(name+".observed.json"),observed.dump(2)+"\n");
    oe::persistence::write_save(folder/(name+".save.json"),view.capture_save_document(),data,view.buildable_mask());
}
}

int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") && SDL_Init(SDL_INIT_VIDEO),
              "House inspector SDL software init failed");
        check(SDL_CreateWindowAndRenderer("Original House inspector",1100,800,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE,&window,&renderer),
              "House inspector software window failed");
        check(std::string(SDL_GetRendererName(renderer))=="software","House inspector did not exercise the actual software renderer");
        oe::testing::economy::Temp temp;const auto root=fs::canonical(temp.root),data=root/"data";
        auto model=oe_production_fixture::authored_model_text(37);
        for (std::size_t row=0;row<orig::house_row_count;++row) oe_production_fixture::set_house_field(model,row,18,3);
        for (std::size_t row=0;row<3;++row) {
            oe_production_fixture::set_house_field(model,row,1,row==2 ? 100:-99);
            oe_production_fixture::set_house_field(model,row,17,37);
            for (unsigned field:{2U,3U,8U,9U,10U,21U}) oe_production_fixture::set_house_field(model,row,field,0);
        }
        // One real visit and actual Food/Ceramics, never source existence,
        // are necessary for the authored Plain Cottage's positive case.
        for (std::size_t row=0;row<3;++row) oe_production_fixture::set_house_field(model,row,2,1);
        oe_production_fixture::set_house_field(model,2,8,20);
        oe_production_fixture::set_house_field(model,2,10,1);
        fs::create_directories(data/"Model");f::write_text(data/"Model/EmperorBuildingModels.txt",model);
        const auto session=[&]{return oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8);};
        const auto save=root/"saves/inspector.json";
        auto view=oe::SandboxView::create_original_production_save_fixture(session());
        view->configure_save(data,"Cities/A.map",save);view->set_walker_visuals(walker_profile(data));
        view->set_road_visuals(road_profile(data));view->initialize(window,renderer);
        check(view->paused() && view->original_house_section()==Section::State && view->original_house_overlay()==Overlay::Off,
              "Inspector session state is not State/Off by default");
        const auto scene=build(*view);const auto first=scene.commons.front();
        select(*view,renderer,first);section(*view,renderer,Section::State);
        const auto vacant_authority=view->world().snapshot();
        check(view->original_house_visual_diagnostic(first).authored_vacant_foundation &&
            joined(view->original_house_section_lines()).find("authored vacant foundation")!=std::string::npos &&
            joined(view->original_house_section_lines()).find("Original art unavailable")!=std::string::npos &&
            view->world().snapshot()==vacant_authority,"Empty House inspector concealed its authored Vacant presentation or changed authority");
        for (int i=0;i<120;++i) view->tick_once();
        check(view->world().building(scene.elite).kind==sim::Object::OriginalEliteHouse &&
              view->world().household_population(scene.elite)==2,"Paid Elite failed actual120t first-arrival materialization");
        zoom(*view,renderer,1);select(*view,renderer,first);
        fs::path folder;if (const auto* value=std::getenv("OPENEMPEROR_UI_CAPTURE_DIR")) {folder=value;fs::create_directories(folder);}
        const auto negative=view->world().original_service_availability(first,orig::ServiceRole::Herbalist);
        check(negative.provider_count==1 && negative.available_source_count>0 && negative.delivered_timer==0 && !negative.last_arrival_tick,
              "Provider purchase fabricated the first real Herbalist visit");
        section(*view,renderer,Section::Visits);overlay(*view,Overlay::Supply);check(view->render(),"Source-only negative frame failed");
        const auto negative_rows=view->original_house_overlay_records();
        const auto negative_row=std::find_if(negative_rows.begin(),negative_rows.end(),[&](const auto& row){return row.building_id==first;});
        check(negative_row!=negative_rows.end() && negative_row->status!=Status::Good,
              "Supply overlay marked a source-only, never-visited House supplied");
        capture(*view,renderer,data,folder,"inspector.source-without-arrival");
        bool actual=false;
        for (unsigned i=0;i<8000 && !actual;++i) {
            view->tick_once();const auto& h=oe_finance_fixture::house(view->world(),first);
            actual=h.herbalist>0 && h.food_quality>0 && h.ceramics>0 && h.type==orig::HouseType::PlainCottage &&
                !view->world().building_on_fire(first) && !view->world().building_on_fire(scene.elite);
        }
        check(actual,"Paid sources never produced a real visited Plain Cottage with delivered Food quality and Ceramics");
        const auto positive=view->world().original_service_availability(first,orig::ServiceRole::Herbalist);
        check(positive.delivered_timer>0 && positive.last_arrival_tick && *positive.last_arrival_tick<=view->world().ticks(),
              "Positive visit has no actual arrival witness and authoritative countdown");
        const auto authority=view->world().snapshot();
        for (std::size_t i=0;i<sections.size();++i) {
            section(*view,renderer,sections[i]);bounded_text(*view);key(*view,SDLK_PAGEDOWN);
            check(view->original_house_section()==sections[(i+1)%4],"PageDown did not cycle the actual inspector section");
            key(*view,SDLK_PAGEUP);check(view->original_house_section()==sections[i],"PageUp did not reverse the section cycle");
        }
        section(*view,renderer,Section::Goods);const auto goods=joined(view->original_house_section_lines());
        for (const auto* word:{"produced","reserved","transit","delivered","consumed"})
            check(goods.find(word)!=std::string::npos,"Goods inspector merged or omitted a required conservation category");
        section(*view,renderer,Section::Visits);const auto visits=joined(view->original_house_section_lines());
        check(visits.find(std::to_string(positive.delivered_timer))!=std::string::npos &&
              visits.find(std::to_string(*positive.last_arrival_tick))!=std::string::npos,
              "Visits inspector omitted its true timer or actual arrival tick");
        for (std::size_t i=0;i<overlays.size();++i) {
            overlay(*view,overlays[i]);check(view->render(),"Overlay cycle frame failed");overlay_bounds(*view,scene);
            click(*view,renderer,view->original_house_overlay_rect());
            check(view->original_house_overlay()==overlays[(i+1)%4],"Mouse overlay cycle did not match F10");
        }
        // F5/F9 restores only authoritative state; the current session's
        // section and overlay intentionally survive the load.
        section(*view,renderer,Section::Visits);overlay(*view,Overlay::Risk);key(*view,SDLK_F5);
        const auto document=oe::persistence::read_save(save);const auto bytes=f::read_text(save);
        check(document.world==authority && document.source_schema_version==26 &&
              document.world.original_production->development_revision==27,"Inspector F5 changed the saved contract or authority");
        section(*view,renderer,Section::Needs);overlay(*view,Overlay::Desirability);
        for (int i=0;i<13;++i) view->tick_once();
        key(*view,SDLK_F9);
        check(view->world().snapshot()==authority && view->paused() && f::read_text(save)==bytes &&
              view->original_house_section()==Section::Needs && view->original_house_overlay()==Overlay::Desirability,
              "F9 failed exact authority restore or serialized session-only inspector choices");
        select(*view,renderer,first);
        // Help, focus, press/release mismatch, resize and panel close must not
        // dispatch hidden inspector controls or leak their click to the map.
        const auto unchanged=view->world().snapshot();
        section(*view,renderer,Section::State);overlay(*view,Overlay::Off);
        key(*view,SDLK_H);key(*view,SDLK_PAGEDOWN);key(*view,SDLK_F10);
        check(view->help_open() && view->original_house_section()==Section::State && view->original_house_overlay()==Overlay::Off,
              "Inspector shortcut escaped Help modal");key(*view,SDLK_ESCAPE);
        const auto needs=view->original_house_section_rect(Section::Needs);
        button(*view,renderer,needs,true);button(*view,renderer,view->original_house_section_rect(Section::Goods),false);
        check(view->original_house_section()==Section::State,"Mismatched inspector mouse release activated another tab");
        button(*view,renderer,needs,true);boundary(*view,SDL_EVENT_WINDOW_FOCUS_LOST);
        key(*view,SDLK_PAGEDOWN);key(*view,SDLK_F10);button(*view,renderer,needs,false);
        check(view->original_house_section()==Section::State && view->original_house_overlay()==Overlay::Off,
              "Unfocused inspector accepted key or stale mouse activation");boundary(*view,SDL_EVENT_WINDOW_FOCUS_GAINED);
        button(*view,renderer,needs,true);boundary(*view,SDL_EVENT_WINDOW_RESIZED);button(*view,renderer,needs,false);
        check(view->original_house_section()==Section::State,"Resize retained an armed inspector press");
        key(*view,SDLK_TAB);key(*view,SDLK_PAGEDOWN);
        check(!view->original_house_inspector_active() && view->original_house_section()==Section::State,"Hidden panel accepted inspector section keys");
        key(*view,SDLK_TAB);check(view->original_house_inspector_active(),"Reopened panel lost its selected paid House");
        check(view->walker_visuals_active() && view->road_visuals_active(),"Authored walker/road profiles did not activate real F2/F6 controls");
        key(*view,SDLK_F2);check(!view->walker_visuals_active(),"F2 comparison toggle was blocked by inspector");key(*view,SDLK_F2);
        key(*view,SDLK_F6);check(!view->road_visuals_active(),"F6 road toggle was blocked by inspector");key(*view,SDLK_F6);
        key(*view,SDLK_F3);key(*view,SDLK_PAGEDOWN);key(*view,SDLK_F10);
        check(view->original_house_section()==Section::State && view->original_house_overlay()==Overlay::Off,
              "Inspector controls escaped actual F3 walker modal");key(*view,SDLK_F3);
        key(*view,SDLK_F1);check(view->debug_diagnostics(),"F1 runtime diagnosis did not open");
        for (int i=0;i<8;++i) {key(*view,SDLK_F8);check(view->render(),"F8 landscape compatibility frame failed");}
        key(*view,SDLK_F1);
        check(view->world().snapshot()==unchanged,"Inspector/modal/visual comparison input mutated World");
        // Actual resize, including the two-row narrow tabs and panel-less
        // minimum, plus a measured software2x target. This is a synthetic DPI
        // transform, never a claim of native Retina/human acceptance.
        for (const auto size:std::array{std::array{720,800},std::array{640,700},std::array{1100,800}}) {
            check(SDL_SetWindowSize(window,size[0],size[1]),"Inspector real SDL resize failed");boundary(*view,SDL_EVENT_WINDOW_RESIZED);
            check(view->render(),"Inspector resized software frame failed");
            if (view->layout().panel_open) for (const auto s:sections) {section(*view,renderer,s);bounded_text(*view);}
            else check(!view->original_house_inspector_active(),"Subminimum panel still exposes active invisible inspector");
        }
        SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,2200,1600);
        check(target!=nullptr && SDL_SetRenderTarget(renderer,target),"Inspector software2x target failed");
        boundary(*view,SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);check(view->render(),"Inspector software2x frame failed");
        check(view->layout().scale==2,"Measured2x software output did not select actual HiDPI layout scale");
        // A target changes output density without creating a physical Retina
        // window. Exercise keyboard sections here; ordinary transformed mouse
        // coordinates are covered above in the real window, not fabricated
        // outside-window coordinates under this synthetic render target.
        for (unsigned i=0;i<4;++i) {
            key(*view,SDLK_PAGEDOWN);check(view->render(),"Inspector software2x section frame failed");bounded_text(*view);
        }
        check(SDL_SetRenderTarget(renderer,nullptr),"Inspector software2x target reset failed");SDL_DestroyTexture(target);
        boundary(*view,SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);check(view->render(),"Inspector1x restoration frame failed");
        select(*view,renderer,first);section(*view,renderer,Section::Goods);
        const auto camera=view->camera();wheel_panel(*view,renderer,-1000);
        check(view->original_house_scroll()>0 && view->camera().zoom==camera.zoom &&
              view->camera().offset.x==camera.offset.x && view->camera().offset.y==camera.offset.y,
              "Inspector wheel failed text scrolling or leaked to the camera");bounded_text(*view);
        wheel_panel(*view,renderer,1000);check(view->original_house_scroll()==0,"Inspector scroll did not clamp back to zero");
        //100 UI-only frames change all sections/modes and ordinary hover but
        // must perform none of the measured authority/preparation operations.
        const auto pure=view->world().snapshot();const auto cache=view->world().original_influence_cache_build_count();
        const auto road_plans=view->road_plan_build_count(),water_plans=view->water_preview_build_count();
        const auto route_checks=view->road_connectivity_check_count();const auto textures=view->walker_texture_count();
        oe::performance::set_enabled(true);oe::performance::reset();
        for (unsigned i=0;i<100;++i) {
            key(*view,SDLK_PAGEDOWN);key(*view,SDLK_F10);
            const auto map=view->layout().map;
            const auto point=window_point(renderer,{map.x+map.w*.5+double(i%3),map.y+map.h*.5});
            SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;
            motion.motion.x=static_cast<float>(point.x);motion.motion.y=static_cast<float>(point.y);event(*view,motion);
            view->update(0);check(view->render(),"Pure inspector frame failed");
        }
        for (std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i) {
            if (oe::performance::counter(static_cast<oe::performance::Counter>(i)))
                throw std::runtime_error("Pure inspector frame performed "+std::string(oe::performance::counter_names[i]));
        }
        oe::performance::set_enabled(false);
        check(view->world().snapshot()==pure && view->world().original_influence_cache_build_count()==cache &&
              view->road_plan_build_count()==road_plans && view->water_preview_build_count()==water_plans &&
              view->road_connectivity_check_count()==route_checks && view->walker_texture_count()==textures,
              "Pure inspector frames rebuilt caches, routes, textures or World authority");
        for (const double z:{1.,2.,4.}) {
            zoom(*view,renderer,z);select(*view,renderer,scene.elite);
            for (std::size_t i=0;i<sections.size();++i) {
                section(*view,renderer,sections[i]);overlay(*view,Overlay::Off);bounded_text(*view);
                capture(*view,renderer,data,folder,std::string("inspector.")+section_names[i]+".zoom"+std::to_string(static_cast<int>(z)));
            }
            for (std::size_t i=1;i<overlays.size();++i) {
                overlay(*view,overlays[i]);check(view->render(),"Zoomed diagnostic overlay frame failed");overlay_bounds(*view,scene);
                capture(*view,renderer,data,folder,std::string("inspector.overlay.")+overlay_names[i]+".zoom"+std::to_string(static_cast<int>(z)));
            }
        }
        check(view->world().snapshot()==pure,"Inspector zoom/captures changed authoritative paid state");
        view->shutdown();view.reset();
        auto fresh=oe::SandboxView::create_original_production_save_fixture(session());
        fresh->configure_save(data,"Cities/A.map",save,document);fresh->initialize(window,renderer);
        check(fresh->world().snapshot()==authority && fresh->original_house_section()==Section::State && fresh->original_house_overlay()==Overlay::Off,
              "A fresh saved session inherited view-local inspector choices");fresh->shutdown();fresh.reset();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Original House inspector paid Common/Elite, actual visits/Food/goods, four sections/three overlays, F5/F9/modal/input,1/2/4x/2x-DPI/resize and100 pure frames PASS; native/human NOT_RUN\n";
        return 0;
    } catch (const std::exception& error) {
        oe::performance::set_enabled(false);std::cerr<<error.what()<<'\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();return 1;
    }
}
