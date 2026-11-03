#pragma once

// Explicit OE46 software-SDL QA through the production View and input paths.
// This helper neither advances World nor changes construction/service authority.
#include "app/SandboxView.h"
#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace oe_citywide_reliability_view {
namespace oe=openemperor;
namespace sim=oe::simulation;
using Json=nlohmann::json;
using Point=oe::scene::Point;
using Rect=oe::sandbox_ui::Rect;

inline void require(bool value,const char* reason) {
    if(!value)throw std::runtime_error(reason);
}

template<class ReadText,class Center>
Json run(oe::SandboxView& view,SDL_Window* window,SDL_Renderer* renderer,
         const oe::persistence::SaveDocument& saved,const std::string& input,
         const std::filesystem::path& save_copy,const std::filesystem::path& output,
         sim::BuildingId elite,bool food_study,bool fire_study,ReadText read_text,Center center) {
    require((view.world().original_deadline_fire_dispatch_enabled()&&saved.source_schema_version==32)||
            (view.world().original_deadline_service_dispatch_enabled()&&saved.source_schema_version==33)||
            (view.world().original_deadline_food_dispatch_enabled()&&saved.source_schema_version==34),
        "OE46 citywide capture requires the exact earned Rule16/schema32 or Rule17/schema33 World");
    Json captures=Json::array(),cases=Json::array();std::uint64_t actions=0;
    const auto pure=[&] {
        require(view.paused()&&view.world().snapshot()==saved.world,
            "OE46 presentation/input changed complete World authority");
    };
    const auto send=[&](SDL_Event event) {
        require(SDL_PushEvent(&event),SDL_GetError());bool running=true;
        while(SDL_PollEvent(&event)){view.handle_event(event,running);pure();}
        require(running,"OE46 input exited the actual SandboxView");view.update(0);pure();++actions;
    };
    const auto key=[&](SDL_Keycode value) {
        SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=value;send(e);
        e={};e.type=SDL_EVENT_KEY_UP;e.key.key=value;send(e);
    };
    const auto middle=[](Rect r){return Point{r.x+r.w*.5,r.y+r.h*.5};};
    const auto raw=[&](Point p) {
        float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,float(p.x),float(p.y),&x,&y),SDL_GetError());
        return Point{x,y};
    };
    const auto click=[&](Point p,bool ui) {
        const auto at=raw(p);const auto d=view.input_diagnostic_state(at);
        require(d.render_position&&d.ui==ui,"OE46 click differs from production render/input boundary");
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=float(at.x);e.motion.y=float(at.y);send(e);
        e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        e.button.x=float(at.x);e.button.y=float(at.y);send(e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;send(e);
    };
    const auto wheel=[&](Point p,float amount) {
        const auto at=raw(p);SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;
        e.wheel.mouse_x=float(at.x);e.wheel.mouse_y=float(at.y);e.wheel.y=amount;send(e);
    };
    const auto render=[&] {require(view.render(),"OE46 production SandboxView render failed");pure();};
    const auto resize=[&](int width,int height) {
        require(SDL_SetWindowSize(window,width,height),SDL_GetError());
        SDL_Event e{};e.type=SDL_EVENT_WINDOW_RESIZED;e.window.data1=width;e.window.data2=height;send(e);
        e={};e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;send(e);render();
        int actual_width=0,actual_height=0;
        require(SDL_GetRenderOutputSize(renderer,&actual_width,&actual_height)&&actual_width==width&&actual_height==height,
            "OE46 software output size differs from requested window size");
    };
    const auto zoom=[&](double wanted) {
        wheel(middle(view.layout().map),float(std::log(wanted/view.camera().zoom)/std::log(1.15)));
        require(std::abs(view.camera().zoom-wanted)<1e-5,"OE46 actual wheel zoom differs");
    };
    const auto overlay=[&](oe::SandboxView::OriginalHouseOverlay wanted) {
        for(unsigned i=0;i<4&&view.original_house_overlay()!=wanted;++i)key(SDLK_F10);
        require(view.original_house_overlay()==wanted,"OE46 actual F10 overlay cycle failed");render();
    };
    const auto persistence=[&] {
        const auto saves=view.save_generation(),loads=view.load_generation();key(SDLK_F5);
        require(view.save_generation()==saves+1&&read_text(save_copy)==input,
            "OE46 actual F5 changed complete SaveDocument bytes");
        key(SDLK_F9);
        require(view.load_generation()==loads+1&&read_text(save_copy)==input,
            "OE46 actual F9 did not restore the complete saved document");pure();
    };
    const auto capture=[&](const std::string& name,const std::string& scope,
                           std::optional<sim::BuildingId> selected,const std::string& position) {
        render();const auto path=output/(name+".png");
        require(!std::filesystem::exists(path),"Preserve an existing OE46 PNG capture");
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
        require(frame&&SDL_SavePNG(frame.get(),path.string().c_str()),"Cannot save actual OE46 software SDL capture");
        Json markers=Json::array();for(const auto& record:view.original_house_overlay_records())
            markers.push_back({{"house",static_cast<unsigned>(record.building_id)},{"marker_visible",record.marker_visible},
                {"visible_cells",record.visible_cells},{"symbol",std::string(1,record.symbol)},{"value",record.value},
                {"screen_ground",{record.screen_ground.x,record.screen_ground.y}}});
        Json row{{"file",path.filename().string()},{"scope",scope},{"width",frame->w},{"height",frame->h},
            {"zoom",view.camera().zoom},{"tick",view.world().ticks()},{"position",position},
            {"selected_house",selected?Json(static_cast<unsigned>(*selected)):Json(nullptr)},
            {"overlay",std::string(oe::original_house_ui::overlay_name(view.original_house_overlay()))},
            {"overlay_legend",view.original_house_overlay_legend()},{"overlay_houses",markers},
            {"map_rect",{view.layout().map.x,view.layout().map.y,view.layout().map.w,view.layout().map.h}},
            {"camera",{{"zoom",view.camera().zoom},{"offset",{view.camera().offset.x,view.camera().offset.y}}}},
            {"complete_world_equal",true},{"paused",true},{"visually_inspected",false}};
        if(selected) {
            const auto diagnostic=view.original_house_visual_diagnostic(*selected);
            const auto stats=view.original_house_display_stats();
            const auto slot=static_cast<std::size_t>(diagnostic.house_type)-2;
            require(diagnostic.active&&diagnostic.image_width>0&&diagnostic.image_height>0&&
                slot<stats.drawn_instances.size()&&stats.drawn_instances[slot]>0&&stats.authored_fallbacks[slot]==0,
                "OE46 focused paid House did not draw its actual original art");
            row["section"]=std::string(oe::original_house_ui::section_name(view.original_house_section()));
            row["scroll"]=view.original_house_scroll();row["inspection_lines"]=view.original_house_section_lines();
            row["actual_type"]=diagnostic.house_type;row["actual_side"]=diagnostic.actual_side;
            const auto origin=view.camera().world_to_screen(diagnostic.image_origin);
            row["sprite_screen_bounds"]={origin.x,origin.y,diagnostic.image_width*view.camera().zoom,diagnostic.image_height*view.camera().zoom};
            row["actual_original_draws_for_type"]=stats.drawn_instances[slot];
            row["authored_fallbacks_for_type"]=stats.authored_fallbacks[slot];
        }
        captures.push_back(std::move(row));pure();
    };

    resize(1920,1080);if(view.layout().panel_open)key(SDLK_TAB);render();zoom(1);
    std::vector<sim::BuildingId> houses;
    for(const auto& house:view.world().original_economy_state()->houses) {
        require(house.house.occupants>0,"OE46 citywide view requires all13 actual occupied Houses");
        houses.push_back(house.building_id);
    }
    require(houses.size()==13,"OE46 expected13 actual inhabited Houses");
    overlay(oe::SandboxView::OriginalHouseOverlay::Supply);
    const auto framing_diagnostic=[&] {
        const auto map=view.layout().map;const auto camera=view.camera();Json markers=Json::array();
        for(const auto& record:view.original_house_overlay_records()) {
            const auto world=camera.screen_to_world(record.screen_ground);
            const auto visual=view.original_house_visual_diagnostic(record.building_id);
            markers.push_back({{"house",static_cast<unsigned>(record.building_id)},
                {"screen_ground",{record.screen_ground.x,record.screen_ground.y}},
                {"camera_inverted_world_ground",{world.x,world.y}},
                {"visual_world_ground",{visual.ground.x,visual.ground.y}},
                {"visual_world_image_bounds",{visual.image_origin.x,visual.image_origin.y,visual.image_width,visual.image_height}},
                {"origin",{record.origin.x,record.origin.y}},
                {"footprint",{record.footprint.width,record.footprint.height}},
                {"marker_visible",record.marker_visible},{"visible_cells",record.visible_cells}});
        }
        return Json{{"map_rect",{map.x,map.y,map.w,map.h}},{"scale",view.layout().scale},
            {"panel_open",view.layout().panel_open},{"camera",{{"zoom",camera.zoom},
                {"offset",{camera.offset.x,camera.offset.y}},
                {"viewport",{camera.viewport_width,camera.viewport_height}}}},
            {"markers",markers}};
    };
    Json framing{{"format","openemperor-oe46-whole-city-framing-v1"},
        {"authority","production Supply overlay screen_ground inverted through the current production camera, with production original House sprite bounds"},
        {"before",framing_diagnostic()}};
    require(view.original_house_overlay_records().size()==13,"OE46 production Supply overlay omitted an actual House");
    double left=std::numeric_limits<double>::max(),right=-left,top=left,bottom=-left;
    for(const auto& record:view.original_house_overlay_records()) {
        const auto ground=view.camera().screen_to_world(record.screen_ground);
        require(std::isfinite(ground.x)&&std::isfinite(ground.y),"OE46 Supply marker lacks a finite production anchor");
        left=std::min(left,ground.x);right=std::max(right,ground.x);
        top=std::min(top,ground.y);bottom=std::max(bottom,ground.y);
        const auto visual=view.original_house_visual_diagnostic(record.building_id);
        require(visual.active&&visual.image_width>0&&visual.image_height>0,
            "OE46 whole-city House lacks active original sprite bounds");
        left=std::min(left,visual.image_origin.x);right=std::max(right,visual.image_origin.x+visual.image_width);
        top=std::min(top,visual.image_origin.y);bottom=std::max(bottom,visual.image_origin.y+visual.image_height);
    }
    const auto map=view.layout().map;
    const auto margin=32*view.layout().scale;
    const int width=std::max(1920,int(std::ceil(right-left))+1920-map.w+margin*2);
    const int height=std::max(1080,int(std::ceil(bottom-top))+1080-map.h+margin*2);
    require(width<=2560&&height<=2160,"OE46 actual13-House marker span exceeds the bounded1x capture window");
    if(width!=1920||height!=1080)resize(width,height);
    center(Point{(left+right)*.5,(top+bottom)*.5});pure();render();
    framing["after"]=framing_diagnostic();
    std::size_t visible=0;for(const auto& record:view.original_house_overlay_records())visible+=record.marker_visible;
    framing["visible_markers"]=visible;
    const auto diagnostic_path=output/"whole-city-framing.json";
    require(!std::filesystem::exists(diagnostic_path),"Preserve existing OE46 framing diagnostics");
    {std::ofstream document(diagnostic_path);document<<framing.dump(2)<<'\n';require(bool(document),"Cannot save OE46 framing diagnostics");}
    if(view.original_house_overlay_stats().houses!=13||visible!=13) {
        capture("city-framing-failure","whole-city-framing-diagnostic",{},"failure-before-assertion");
    }
    require(view.original_house_overlay_stats().houses==13&&visible==13,
        "OE46 whole-city1x capture does not show all13 production Supply markers");
    for(const auto id:houses) {
        const auto visual=view.original_house_visual_diagnostic(id);const auto at=view.camera().world_to_screen(visual.image_origin);
        const auto aperture=view.layout().map;
        require(at.x>=aperture.x&&at.y>=aperture.y&&
            at.x+visual.image_width*view.camera().zoom<=aperture.x+aperture.w&&
            at.y+visual.image_height*view.camera().zoom<=aperture.y+aperture.h,
            "OE46 whole-city1x capture clips an actual production House sprite bound");
    }
    const auto city_name="city-"+std::to_string(width)+"x"+std::to_string(height)+"-1x";
    capture(city_name+"-supply","whole-city-all13",{},"whole-city");persistence();
    overlay(oe::SandboxView::OriginalHouseOverlay::Off);
    capture(city_name+"-off","whole-city-all13",{},"whole-city");persistence();
    if(fire_study) {
        overlay(oe::SandboxView::OriginalHouseOverlay::Risk);
        capture(city_name+"-risk","whole-city-all13",{},"whole-city");persistence();
    }

    resize(1280,800);if(!view.layout().panel_open)key(SDLK_TAB);render();
    std::vector<sim::BuildingId> problems;
    if(fire_study) {
        const auto house=view.world().building_owner_at({121,126});
        require(house&&std::find(houses.begin(),houses.end(),*house)!=houses.end(),
            "OE48 House19 site has no actual occupied paid House");
        problems={*house,elite};
    } else if(food_study) {
        // Bind the baseline problem sites to actual complete paid footprints;
        // purchase-order IDs may differ between legal city controls.
        for(const auto cell:std::array<sim::Cell,6>{{{121,128},{113,138},{111,137},{121,132},{113,115},{121,126}}}) {
            const auto owner=view.world().building_owner_at(cell);
            require(owner&&std::find(houses.begin(),houses.end(),*owner)!=houses.end(),
                "OE47 problem site has no actual occupied paid House");
            problems.push_back(*owner);
        }
        problems.push_back(elite);
    } else problems={static_cast<sim::BuildingId>(25),static_cast<sim::BuildingId>(24),elite};
    const std::vector<std::array<int,2>> sizes=fire_study ?
        std::vector<std::array<int,2>>{{1280,800},{1600,900},{1920,1080}}:
        std::vector<std::array<int,2>>{{1280,800}};
    for(const auto size:sizes)for(const auto house:problems) {
        if(fire_study){resize(size[0],size[1]);if(!view.layout().panel_open)key(SDLK_TAB);render();}
        require(std::find(houses.begin(),houses.end(),house)!=houses.end(),"OE46 problem House is not an actual occupied recipient");
        zoom(1);const auto visual=view.original_house_visual_diagnostic(house);
        center(visual.ground);pure();render();key(SDLK_5);
        bool picked=false;
        for(const auto offset:std::array<Point,9>{{{0,0},{0,-2},{0,-8},{-8,-8},{8,-8},{0,-16},{-16,-16},{16,-16},{0,-24}}}) {
            const auto p=view.camera().world_to_screen({visual.ground.x+offset.x,visual.ground.y+offset.y});
            if(!view.layout().map.contains(p.x,p.y))continue;
            const auto d=view.input_diagnostic_state(raw(p));
            if(d.ui||!d.ground_cell||view.world().building_owner_at(*d.ground_cell)!=house)continue;
            click(p,false);picked=view.selected_building()==house;if(picked)break;
        }
        require(picked&&view.original_house_inspector_active(),"OE46 actual map input failed to select the problem House");
        const Point art_center{visual.image_origin.x+visual.image_width*.5,visual.image_origin.y+visual.image_height*.5};
        for(const unsigned z:{1U,2U,4U}) {
            zoom(z);center(art_center);pure();overlay(oe::SandboxView::OriginalHouseOverlay::Supply);
            for(unsigned index=0;index<4;++index) {
                const auto section=static_cast<oe::SandboxView::OriginalHouseSection>(index);
                const auto rect=view.original_house_section_rect(section);require(rect.w>0&&rect.h>0,"OE46 actual House tab is clipped");
                click(middle(rect),true);render();require(view.original_house_section()==section&&view.selected_building()==house,
                    "OE46 actual tab selected another House/section");
                const auto content=view.original_house_content_rect();const auto lines=view.original_house_section_lines();
                require(content.w>0&&content.h>0&&!lines.empty(),"OE46 actual inspector section has no readable document");
                wheel(middle(content),1000000);render();require(view.original_house_scroll()==0,"OE46 section cannot return to its first row");
                const auto name="house"+std::to_string(static_cast<unsigned>(house))+"-"+std::to_string(size[0])+"x"+std::to_string(size[1])+"-"+std::to_string(z)+"x-"+
                    std::string(oe::original_house_ui::section_name(section));
                capture(name+"-top","problem-house",house,"top");std::vector<bool> reached(lines.size());
                for(unsigned step=0;;++step) {
                    require(step<512,"OE46 inspector scroll exceeded bounded document traversal");
                    const auto scroll=view.original_house_scroll();const auto scale=view.layout().scale;
                    for(std::size_t row=0;row<lines.size();++row)
                        if(oe::sandbox_ui::visible_text_row(content.y+int(row)*17*scale-scroll,12*scale,content).h>0)reached[row]=true;
                    wheel(middle(content),-1);render();if(view.original_house_scroll()==scroll)break;
                    require(view.original_house_scroll()>scroll,"OE46 inspector document scrolled backwards");
                }
                require(std::all_of(reached.begin(),reached.end(),[](bool v){return v;}),"OE46 inspector contains an unreachable complete text row");
                if(view.original_house_scroll()>0)capture(name+"-end","problem-house",house,"end");
                persistence();Json result{{"house",static_cast<unsigned>(house)},{"zoom",z},{"section",index},
                    {"rows",lines.size()},{"all_rows_reached",true},{"complete_f5_f9_equality",true}};
                if(fire_study){result["width"]=size[0];result["height"]=size[1];}
                cases.push_back(std::move(result));
            }
        }
    }
    require(cases.size()==problems.size()*sizes.size()*12&&captures.size()<=(fire_study?3U:2U)+problems.size()*sizes.size()*24,"OE46 citywide capture matrix exceeds its fixed bound");
    Json problem_ids=Json::array();for(const auto id:problems)problem_ids.push_back(static_cast<unsigned>(id));
    return {{"format",fire_study?"openemperor-oe48-citywide-view-v1":food_study?"openemperor-oe47-citywide-view-v1":"openemperor-oe46-citywide-view-v1"},{"status","PASS"},{"rule",view.world().rule_version()},{"schema",saved.source_schema_version},
        {"tick",view.world().ticks()},{"actual_sandbox_renderer",true},{"backend",SDL_GetRendererName(renderer)},
        {"scripted_dummy_software",true},{"all13_whole_city_markers_visible_at1x",true},
        {"all13_whole_city_sprite_bounds_inside_map_at1x",true},
        {"problem_houses",problem_ids},{"cases",cases},{"captures",captures},{"actions",actions},
        {"complete_f5_f9_equality",true},{"complete_world_unchanged",true},{"paused_after_every_action",true},
        {"service_inspection","Actual Needs/Visits tabs and existing Supply overlay; no legacy service timer substituted"},
        {"separate_original_service_overlay","UNAVAILABLE: production original-economy UI has Supply/DES/Risk only; legacy V Service is not an original nine-timer overlay"},
        {"captures_visually_inspected",false},{"native_human","NOT_RUN"}};
}
} // namespace oe_citywide_reliability_view
