// Actual original General images in a paid, explicitly authored Rule10 scene.
// Map/model inputs are authored before preparation; no live/saved state grants.
#include "EconomyStartViewFixture.h"
#include "OriginalFinanceFixture.h"
#include "app/WalkerPose.h"
#include "app/VisualSelection.h"
#include "assets/BuildingVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/LandscapeProvenance.h"
#include "maps/MapRulesCheck.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <set>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace orig=oe::original;
namespace assets=oe::assets;
namespace f=oe_influence_fixture;
namespace fs=std::filesystem;
namespace perf=oe::performance;
using oe::testing::economy::check;
using oe::testing::economy::Bytes;
using oe::testing::economy::u16;
using oe::testing::economy::u32;
using oe::testing::economy::write;
using Point=oe::scene::Point;
using Json=nlohmann::json;
using Color=std::array<std::uint8_t,4>;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
constexpr sim::Cell foreground_cell{114,111};

void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Visual test input closed its actual session");
}
void key(oe::SandboxView& view,SDL_Keycode code) {SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);}
Point window_point(SDL_Renderer* renderer,Point p) {
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(p.x),static_cast<float>(p.y),&x,&y),
        "Actual render-to-window input transform failed");return {x,y};
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point p,bool on_map=true) {
    check(!on_map || view.layout().map.contains(p.x,p.y),"Native alpha witness is outside the map aperture");
    const auto raw=window_point(renderer,p);SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;
    e.motion.x=static_cast<float>(raw.x);e.motion.y=static_cast<float>(raw.y);event(view,e);view.update(0);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(raw.x);e.button.y=static_cast<float>(raw.y);event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);view.update(0);
}
void pan(oe::SandboxView& view,Point wanted) {
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,focus);
    for (unsigned i=0;i<1000;++i) {
        const auto current=view.camera().offset;const double dx=wanted.x-current.x,dy=wanted.y-current.y;
        if (std::abs(dx)<1e-6 && std::abs(dy)<1e-6) return;
        oe::SandboxView::PanKeyState held{};
        if (std::abs(dx)>1e-6) {held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Actual held-key pan did not reach its requested visual witness");
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;const auto p=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    for (unsigned i=0;i<3 && std::abs(view.camera().zoom-wanted)>1e-9;++i) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);e.wheel.mouse_y=static_cast<float>(p.y);
        e.wheel.y=static_cast<float>(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0);
    }
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Mouse wheel did not reach literal1/2/4 zoom");
}
Point projected(double x,double y,double height=1) {return {(x-y)*40,(x+y-144)*20+20-height*40};}
void center(oe::SandboxView& view,Point world) {
    const auto map=view.layout().map;const double z=view.camera().zoom;
    pan(view,{std::floor(map.x+map.w*.5-world.x*z),std::floor(map.y+map.h*.5-world.y*z)});
}
Surface pixels(SDL_Renderer* renderer) {
    Surface raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(raw),"Actual software frame readback failed");
    Surface rgba(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    check(bool(rgba),"Actual software frame conversion failed");return rgba;
}
Color pixel(SDL_Surface* image,Point p) {
    const int x=static_cast<int>(p.x),y=static_cast<int>(p.y);
    check(x>=0 && y>=0 && x<image->w && y<image->h,"Pixel witness escaped its actual SDL surface");
    const auto* b=static_cast<const std::uint8_t*>(image->pixels)+y*image->pitch+x*4;
    return {b[0],b[1],b[2],b[3]};
}
Color native(const assets::RgbaImage& image,int x,int y) {
    const auto at=static_cast<std::size_t>((y*image.width+x)*4);return {image.pixels.at(at),image.pixels.at(at+1),image.pixels.at(at+2),image.pixels.at(at+3)};
}
const orig::HouseState& house(const oe::SandboxView& view,sim::BuildingId id) {
    return oe_finance_fixture::house(view.world(),id);
}
void command(oe::SandboxView& view,sim::Command value) {
    const auto result=view.execute(value);if (!result.accepted || !result.changed) throw std::runtime_error("Paid visual fixture: "+result.reason);
}
sim::BuildingId buy(oe::SandboxView& view,sim::CommandType type,sim::Cell cell) {
    const auto funds=view.world().treasury();const auto spent=view.world().construction_spent_total();
    command(view,{type,cell});check(view.world().treasury()==funds-static_cast<std::int64_t>(view.world().construction_spent_total()-spent),
        "Paused paid command did not debit actual construction spending");
    return view.world().building_owner_at(cell).value_or(sim::BuildingId{});
}

Bytes map_bytes(bool foreground) {
    using namespace oe::maps;
    Bytes raw(static_cast<std::size_t>(original_entities_logical_offset+6),0);
    const std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};std::copy(signature.begin(),signature.end(),raw.begin());
    u32(raw,84,84);u16(raw,static_cast<std::size_t>(original_entities_logical_offset),1);
    for (std::size_t i=0;i<228U*228U;++i) {
        u32(raw,static_cast<std::size_t>(candidate_word_logical_offset)+4*i,0xc000);
        raw[static_cast<std::size_t>(candidate_byte_logical_offset)+i]=64;
        u32(raw,static_cast<std::size_t>(terrain_logical_offset)+4*i,0x80);
        raw[static_cast<std::size_t>(landscape_height_offset)+i]=static_cast<std::uint8_t>(i%228<=125 ? 1:0);
    }
    // One real paid Rule10 ramp can cross the authored signed delta1. This
    // deliberately does not claim an original-map slope or original cliff art.
    if (foreground) u32(raw,static_cast<std::size_t>(candidate_word_logical_offset)+4U*(foreground_cell.y*228U+foreground_cell.x),0xc001);
    Bytes out{0xaa,0xba,0xdc,0xfe};
    for (std::size_t at=0;at<raw.size();at+=32768) {
        const auto count=std::min<std::size_t>(32768,raw.size()-at);uLongf capacity=compressBound(static_cast<uLong>(count));Bytes compressed(capacity);
        check(compress2(compressed.data(),&capacity,raw.data()+at,static_cast<uLong>(count),6)==Z_OK,"Authored visual map zlib failed");
        compressed.resize(capacity);const auto start=out.size();out.resize(start+12);u32(out,start,0x12345678);
        u32(out,start+4,static_cast<std::uint32_t>(capacity));u32(out,start+8,static_cast<std::uint32_t>(count));
        out.insert(out.end(),compressed.begin(),compressed.end());
    }
    return out;
}
void authored_foreground(const fs::path& data) {
    auto archive=oe::testing::economy::sg3(true);
    // Register a second independent one-cell Type30 image in the same source
    // group. Its blue stripes and clear gaps are authored painter witnesses.
    archive.resize(40680U+204U*64U);u32(archive,0,static_cast<std::uint32_t>(archive.size()));
    u32(archive,12,204);u32(archive,16,203);
    constexpr int width=78,height=180;Bytes image(6400,0);
    for (std::size_t i=3200;i<6400;i+=2) u16(image,i,0xf81f);
    const auto overlay_begin=image.size();
    for (int y=0;y<height;++y) {
        int x=0;
        while (x<width) {
            const bool opaque=(x/8)%2==0;const int end=std::min(width,(x/8+1)*8),count=end-x;
            if (opaque) {image.push_back(static_cast<std::uint8_t>(count));for (int i=0;i<count;++i) {image.push_back(0x1f);image.push_back(0);}}
            else {image.push_back(255);image.push_back(static_cast<std::uint8_t>(count));}
            x=end;
        }
    }
    const auto at=40680U+202U*64U;u32(archive,at,3200);u32(archive,at+4,static_cast<std::uint32_t>(3200+image.size()-overlay_begin));
    u32(archive,at+8,3200);u16(archive,at+20,width);u16(archive,at+22,height);u16(archive,at+50,30);archive[at+55]=1;
    // The existing first diamond retains its original authored green pixels.
    for (std::size_t i=0;i<3200;i+=2) u16(image,i,0x03e0);
    write(data/"DATA/China_Terrain.sg3",archive);write(data/"DATA/China_Terrain.555",image);
}
fs::path road_profile(const fs::path& data) {
    Json tiles=Json::object();constexpr char hex[]="0123456789abcdef";
    for (unsigned mask=0;mask<16;++mask) tiles[std::string("0x")+hex[mask]]={
        {"archive","DATA/China_Terrain.sg3"},{"image_index",201},{"ground_anchor",{39,20}},
        {"evidence","Authored unchanged green diamond for independent F6 comparison"}};
    const auto file=data/"visual-roads.json";f::write_text(file,Json{{"schema_version",1},{"mode","curated_road_preview"},{"replaces_ground",true},{"tiles",tiles}}.dump(2));return file;
}
struct Scene {std::vector<sim::BuildingId> commons;sim::BuildingId elite{};std::vector<sim::BuildingId> paused;};
Scene build(oe::SandboxView& view) {
    Scene result;
    for (int y:{94,99}) for (int x=100;x<124;++x) buy(view,sim::CommandType::PlaceRoad,{x,y});
    for (int y=95;y<99;++y) buy(view,sim::CommandType::PlaceRoad,{120,y});
    for (int x=100;x<124;x+=3) result.commons.push_back(buy(view,sim::CommandType::PlaceHousehold,{x,92}));
    result.elite=buy(view,sim::CommandType::PlaceOriginalElitePlot,{100,95});
    buy(view,sim::CommandType::PlaceOriginalWaterWell,{104,95});buy(view,sim::CommandType::PlaceOriginalAncestorShrine,{106,95});
    buy(view,sim::CommandType::PlaceMarket,{109,95});
    result.paused.push_back(buy(view,sim::CommandType::PlaceClaySource,{112,95}));
    result.paused.push_back(buy(view,sim::CommandType::PlacePottery,{115,95}));buy(view,sim::CommandType::PlaceWarehouse,{118,95});
    result.paused.push_back(buy(view,sim::CommandType::PlaceOriginalConfucianAcademy,{121,95}));
    const auto food=buy(view,sim::CommandType::PlaceOriginalFoodFarm,{100,100});result.paused.push_back(food);
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{103,100}};field.building_id=food;command(view,field);
    const auto hemp=buy(view,sim::CommandType::PlaceOriginalHempFarm,{105,100});result.paused.push_back(hemp);
    field={sim::CommandType::PlaceOriginalHempField,{107,100}};field.building_id=hemp;command(view,field);
    result.paused.push_back(buy(view,sim::CommandType::PlaceOriginalPaperMaker,{109,100}));
    for (const auto id:result.paused) command(view,sim::set_building_operation(id,false));
    for (int x=124;x<=126;++x) buy(view,sim::CommandType::PlaceRoad,{x,94});
    const auto before=view.world().treasury();command(view,sim::place_ramp({125,94},{126,94}));
    check(before-view.world().treasury()==40 && view.world().ramps().size()==1 && view.authored_terrain_side_count()==0,
        "Actual Rule10 ramp did not pay40 or enabled Rule6-only authored terrain sides");
    check(view.world().ticks()==0 && !view.world().total_population() && !view.world().taxes_collected_total(),
        "Paid initial scene invented residents, taxes or elapsed time");return result;
}
Json visual_json(const oe::SandboxView::OriginalHouseVisualDiagnostic& d) {
    Json result{{"applicable",d.applicable},{"configured",d.configured},{"active",d.active},{"type",d.house_type},{"actual_side",d.actual_side},
        {"authored_vacant_foundation",d.authored_vacant_foundation},
        {"native_dimensions",{d.image_width,d.image_height}},{"ground_anchor",{d.ground_x,d.ground_y}},
        {"world_ground",{d.ground.x,d.ground.y}},{"world_image_origin",{d.image_origin.x,d.image_origin.y}},
        {"selection",d.selection},{"evidence",d.evidence},{"fallback_reason",d.fallback_reason}};
    if (d.asset) result["asset"]={{"archive",d.asset->archive_relative_path.generic_string()},{"image_index",d.asset->image_index}};
    return result;
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& data,const fs::path& out,
             sim::BuildingId id,const std::string& name,Json extra=Json::object()) {
    if (out.empty()) return;
    check(view.render(),"Original House capture render failed");auto frame=pixels(renderer);
    check(SDL_SavePNG(frame.get(),(out/(name+".png")).string().c_str()),"Original House actual PNG write failed");
    const auto d=view.original_house_visual_diagnostic(id);const auto& h=house(view,id);
    Json observed{{"capture",name},{"ticks",view.world().ticks()},{"building_id",static_cast<unsigned>(id)},
        {"house",visual_json(d)},{"occupants",h.occupants},{"zoom",view.camera().zoom},{"rule_version",view.world().rule_version()},
        {"schema",26},{"development_revision",27},{"authored_map",true},{"authored_model",true},
        {"original_art",d.active},{"original_map",false},{"original_variation_restored",false},
        {"original_cliff_parity","UNKNOWN"},{"native_actions",false},{"human_acceptance","NOT_RUN"},
        {"renderer",SDL_GetRendererName(renderer)},{"actual_paid_ramps",view.world().ramps().size()}};
    observed.update(extra);f::write_text(out/(name+".observed.json"),observed.dump(2)+"\n");
    oe::persistence::write_save(out/(name+".save.json"),view.capture_save_document(),data,view.buildable_mask());
}
void select_owned(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    if (view.debug_diagnostics()) key(view,SDLK_F1);
    view.set_tool(5);const auto& b=view.world().building(id);
    const auto footprint=sim::building_footprint(view.world().profile(),view.world().rule_version(),b.kind);
    center(view,projected(b.cell.x+footprint.width-1,b.cell.y+footprint.height-1));check(view.render(),"Paid selection frame failed");
    for (int y=footprint.height-1;y>=0;--y) for (int x=footprint.width-1;x>=0;--x) {
        auto ground=projected(b.cell.x+x,b.cell.y+y);ground.y-=20;const auto p=view.camera().world_to_screen(ground);
        for (int dy=-8;dy<=8;dy+=4) for (int dx=-8;dx<=8;dx+=4) {
            const Point at{p.x+dx,p.y+dy};if (!view.layout().map.contains(at.x,at.y)) continue;
            if (const auto cell=view.pick(at);cell && view.world().building_owner_at(*cell)==id) {
                click(view,renderer,at);check(view.selected_building()==id,"Actual owned-cell mouse selection chose a different House");return;
            }
        }
    }
    throw std::runtime_error("Paid House has no visible real owned cell for mouse selection");
}
Json verify_visual(oe::SandboxView& view,SDL_Renderer* renderer,const assets::BuildingVisualProfile& profile,sim::BuildingId id) {
    const auto& b=view.world().building(id);const auto& h=house(view,id);
    const auto side=sim::building_footprint(view.world().profile(),view.world().rule_version(),b.kind).width;
    const auto d=view.original_house_visual_diagnostic(id);const auto type=static_cast<std::uint16_t>(h.type);
    const auto* entry=profile.find_original_house(type,static_cast<std::uint8_t>(side));
    check(d.applicable && d.house_type==type && d.actual_side==side && d.configured==bool(entry),
        "View stage selection disagrees with actual paid HouseType/body side");
    const auto expected_ground=projected(b.cell.x+side-1,b.cell.y+side-1);
    check(std::abs(d.ground.x-expected_ground.x)<1e-9 && std::abs(d.ground.y-expected_ground.y)<1e-9,
        "Native House anchor did not use the actual signed-height front cell exactly once");
    if (!entry) {
        check(!d.active && !d.asset && !d.fallback_reason.empty(),"Unsupported actual type/body silently borrowed another House image");
        center(view,expected_ground);check(view.render(),"Named authored fallback frame failed");return {{"named_fallback",d.fallback_reason}};
    }
    const auto& image=profile.unique_images.at(entry->image_index);const double z=view.camera().zoom;
    check(d.active && d.asset==entry->id && d.image_width==image.width && d.image_height==image.height &&
          d.ground_x==entry->ground_x && d.ground_y==entry->ground_y && d.image_origin.x==d.ground.x-entry->ground_x &&
          d.image_origin.y==d.ground.y-entry->ground_y && d.selection=="canonical static representative" && !d.evidence.empty(),
          "Actual House sprite changed identity/native dimensions/ground anchor or omitted selector evidence");
    center(view,{d.image_origin.x+image.width*.5,d.image_origin.y+image.height*.5});
    if (!view.debug_diagnostics()) key(view,SDLK_F1);
    view.set_tool(5);check(view.render(),"Native sprite alpha frame failed");const auto frame=pixels(renderer);
    const auto origin=view.camera().world_to_screen(d.image_origin);std::optional<Point> opaque,hole;
    // The expected coordinates use independent decoded native pixels. Search
    // bounded native image coordinates, never renderer hit-cache internals.
    for (int y=1;y<image.height-1 && (!opaque || !hole);y+=2) for (int x=1;x<image.width-1 && (!opaque || !hole);x+=2) {
        const auto color=native(image,x,y);const Point p{origin.x+(x+.5)*z,origin.y+(y+.5)*z};
        if (!view.layout().map.contains(p.x,p.y)) continue;
        if (!opaque && color[3]==255 && pixel(frame.get(),p)==color) {
            click(view,renderer,p);if (view.selected_building()==id && !view.input_diagnostic_state().selected_landscape) opaque=p;
        }
        if (!hole && color[3]==0 && x>4 && y>4) {
            click(view,renderer,p);if (view.selected_building()!=id) hole=p;
        }
    }
    check(opaque.has_value(),"No visible opaque native pixel selected the same actual paid House");
    check(hole.has_value(),"Transparent native pixel incorrectly retained the House's rectangular hit");
    click(view,renderer,*opaque);check(view.selected_building()==id,"Native alpha selection changed after a transparent-hole click");
    check(view.render(),"Native alpha selected frame failed");
    return {{"opaque_native_pixel",{opaque->x,opaque->y}},{"transparent_native_pixel",{hole->x,hole->y}},
        {"submitted_dimensions",{image.width*z,image.height*z}},{"native_alpha_picking",true}};
}
void stages(oe::SandboxView& view,SDL_Renderer* renderer,const assets::BuildingVisualProfile& profile,
            const fs::path& data,const fs::path& out,sim::BuildingId id,const std::string& name,unsigned expected) {
    check(static_cast<unsigned>(house(view,id).type)==expected,"Paid transition checkpoint has the wrong actual HouseType");
    const auto snapshot=view.world().snapshot();
    for (double z:{1.,2.,4.}) {
        zoom(view,renderer,z);select_owned(view,renderer,id);const auto details=verify_visual(view,renderer,profile,id);
        capture(view,renderer,data,out,id,name+".zoom"+std::to_string(static_cast<int>(z)),details);
    }
    check(view.world().snapshot()==snapshot,"Stage camera/selection/pixels changed authoritative paid state");
}
void vacant_matrix(oe::SandboxView& view,SDL_Window* window,SDL_Renderer* renderer,
        const fs::path& data,const fs::path& save,const fs::path& out,sim::BuildingId id,
        const std::string& label,bool shelter,bool real_map,const assets::BuildingVisualProfile* profile=nullptr) {
    const auto authority=view.world().snapshot();const auto textures=view.building_texture_count();
    const auto type=shelter ? 3U:2U;
    check(static_cast<unsigned>(house(view,id).type)==type && (house(view,id).occupants>0)==shelter,
        "Vacant/Shelter matrix did not use a real empty/admitted House");
    for(const auto size:std::array<std::array<int,2>,3>{{{1280,800},{1600,900},{1920,1080}}}) {
        check(SDL_SetWindowSize(window,size[0],size[1]),"Vacant matrix resize failed");
        SDL_Event e{};e.type=SDL_EVENT_WINDOW_RESIZED;e.window.data1=size[0];e.window.data2=size[1];event(view,e);
        e={};e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,e);view.update(0);
        for(const double z:{1.,2.,4.}) {
            zoom(view,renderer,z);const auto d=view.original_house_visual_diagnostic(id);
            check(d.house_type==type && d.actual_side==2 && d.authored_vacant_foundation==(!shelter && !d.active) &&
                (!real_map || (d.active && d.asset && d.asset->image_index==(shelter ? 1509U:1508U))),
                "Vacant/Shelter selected the wrong original lifecycle identity or lost its fallback");
            Json native_pixels=Json::object();
            if (profile) {
                native_pixels["native_alpha_preflight"]=verify_visual(view,renderer,*profile,id);
                native_pixels["native_alpha_preflight_camera_offset"]={view.camera().offset.x,view.camera().offset.y};
            }
            center(view,{d.ground.x,d.ground.y-20});view.set_tool(5);
            if(view.debug_diagnostics())key(view,SDLK_F1);
            check(view.render(),"Vacant matrix frame failed");
            for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
                const Point tile{d.ground.x+(x-y)*40,d.ground.y+(x+y-2)*20};
                const auto p=view.camera().world_to_screen(tile);
                const auto picked=view.pick(p);
                check(picked && view.world().building_owner_at(*picked)==id,"House footprint-cell ground pick lost its actual owner");
                click(view,renderer,p);check(view.selected_building()==id,"House footprint click selected another owner");
            }
            if(!shelter && !d.active) {
                key(view,SDLK_F1);view.set_tool(5);check(view.render(),"Vacant visible-pixel frame failed");
                click(view,renderer,view.camera().world_to_screen({d.ground.x,d.ground.y-20}));
                check(view.selected_building()==id,"Authored foundation's visible interior was not selectable");
                // A transparent bounding-box corner also lies outside the
                // logical footprint, so ordinary ground selection cannot
                // legitimately choose this same House after the visual miss.
                const auto corner=view.camera().world_to_screen({d.ground.x+65,d.ground.y-48});
                const auto corner_cell=view.pick(corner);
                check(!corner_cell || view.world().building_owner_at(*corner_cell)!=id,
                    "Transparent corner witness still lies in the authoritative House footprint");
                click(view,renderer,corner);
                check(view.selected_building()!=id,"Invisible foundation margin retained a rectangular House hit");
                key(view,SDLK_F1);view.set_tool(5);
                click(view,renderer,view.camera().world_to_screen({d.ground.x,d.ground.y-20}));
                check(view.original_house_inspector_active(),"Vacant click did not open the real House inspector");
                const auto lines=view.original_house_section_lines();
                std::string inspector;for(const auto& line:lines)inspector+=line+' ';
                check(inspector.find("authored vacant foundation")!=std::string::npos,
                    "Vacant inspector omitted the explicit authored representation");
                // Independently chosen opaque interiors must remain readable
                // without a red marker or borrowed native roof.
                check(view.render(),"Vacant material witness frame failed");const auto frame=pixels(renderer);
                const auto masonry=pixel(frame.get(),view.camera().world_to_screen({d.ground.x+47,d.ground.y-20}));
                const auto earth=pixel(frame.get(),view.camera().world_to_screen({d.ground.x+62,d.ground.y-20}));
                check(masonry[0]>150 && masonry[1]>150 && masonry[2]>120 && earth[0]>earth[1] && earth[1]>earth[2],
                    "Unselected foundation lost pale masonry or warm cleared-earth contrast");
            }
            if (d.active) {
                const auto lines=view.original_house_section_lines();std::string inspector;
                for(const auto& line:lines)inspector+=line+' ';
                check(inspector.find("#"+std::to_string(d.asset->image_index))!=std::string::npos &&
                    inspector.find("authored vacant foundation")==std::string::npos,
                    "Native House inspector omitted its actual original image or claimed an authored foundation");
            }
            const auto name="oe59-"+label+"-"+std::to_string(size[0])+"x"+std::to_string(size[1])+"-"+std::to_string(int(z))+"x";
            key(view,SDLK_F5);const auto bytes=f::read_text(save);
            const auto persisted=oe::persistence::read_save(save);
            check(persisted.world==authority,"Vacant/Shelter persisted capture changed complete World authority");
            check(view.render(),"Vacant/Shelter final frame failed");
            check(view.world().snapshot()==authority,"Vacant/Shelter capture changed complete World authority");
            if(!out.empty()) {
                auto frame=pixels(renderer);check(SDL_SavePNG(frame.get(),(out/(name+".png")).string().c_str()),"Vacant matrix PNG failed");
                const auto pointer=view.camera().world_to_screen({d.ground.x,d.ground.y-(shelter ? 0:20)});
                Json observed{{"actual_sandbox_renderer",true},{"scripted_software",true},{"native_human","NOT_RUN"},
                    {"real_map_and_model",real_map},{"tick",view.world().ticks()},{"building_id",static_cast<unsigned>(id)},
                    {"occupants",house(view,id).occupants},{"visual",visual_json(d)},
                    {"window",size},{"zoom",view.camera().zoom},{"camera_offset",{view.camera().offset.x,view.camera().offset.y}},
                    {"map_rect",{view.layout().map.x,view.layout().map.y,view.layout().map.w,view.layout().map.h}},
                    {"selection_render_point",{pointer.x,pointer.y}},{"selected_building",static_cast<unsigned>(*view.selected_building())},
                    {"rule",view.world().rule_version()},{"schema",persisted.source_schema_version},
                    {"funds",view.world().treasury()},{"construction_spent",view.world().construction_spent_total()},
                    {"complete_world_equal",true},{"persisted_save_world_equal",true},{"actual_all_four_owned_cells_picked",true}};
                observed.update(native_pixels);f::write_text(out/(name+".observed.json"),observed.dump(2)+"\n");
            }
            key(view,SDLK_F9);key(view,SDLK_F5);
            check(view.world().snapshot()==authority && f::read_text(save)==bytes &&
                view.capture_save_document().world==authority && view.building_texture_count()==textures &&
                view.original_house_visual_diagnostic(id).asset==d.asset &&
                view.original_house_visual_diagnostic(id).active==d.active,
                "Vacant/Shelter camera/picking/F5/F9 changed complete World, save bytes or texture ownership");
            if(real_map && !shelter && size[0]==1280) {
                // The ordinary player view must show the paid body without
                // relying on either a selected footprint or its inspector.
                std::optional<Point> clear;
                for(const Point offset:{Point{-40,20},Point{40,20},Point{-80,-40},Point{80,-40}}) {
                    const auto p=view.camera().world_to_screen({d.ground.x+offset.x,d.ground.y+offset.y});
                    const auto cell=view.pick(p);
                    if(view.layout().map.contains(p.x,p.y) && cell && !view.world().building_owner_at(*cell)) {clear=p;break;}
                }
                check(clear.has_value(),"Unselected witness did not find adjacent real unowned ground");
                click(view,renderer,*clear);check(!view.selected_building(),"Ground click did not clear House selection");
                for(const bool hovered:{false,true}) {
                    const auto pointer=hovered ? view.camera().world_to_screen({d.ground.x,d.ground.y-20}):Point{1,1};
                    const auto raw=window_point(renderer,pointer);SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;
                    motion.motion.x=static_cast<float>(raw.x);motion.motion.y=static_cast<float>(raw.y);event(view,motion);view.update(0);
                    check(!view.selected_building() && !view.original_house_inspector_active() &&
                        (hovered ? view.hovered_cell() && view.world().building_owner_at(*view.hovered_cell())==id:!view.hovered_cell()),
                        "Actual normal/hover capture retained House selection or lost its requested pointer state");
                    check(view.render() && view.world().snapshot()==authority,"Normal/hover frame changed paid authority");
                    const auto frame=pixels(renderer);
                    // Sample the source pixel's interior. Exact source edges
                    // can truncate into the preceding readback row after the
                    // double camera and SDL_FRect float conversions.
                    const auto material_point=view.camera().world_to_screen({d.image_origin.x+126.5,d.image_origin.y+48.5});
                    const auto material=pixel(frame.get(),material_point);
                    const auto* entry=profile ? profile->find_original_house(2,2):nullptr;
                    check(entry && material==native(profile->unique_images.at(entry->image_index),126,48),
                        "Unselected/hover House lost an independently decoded opaque1508 pixel");
                    if(!out.empty()) {
                        const auto extra=name+(hovered ? "-hover":"-unselected");
                        check(SDL_SavePNG(frame.get(),(out/(extra+".png")).string().c_str()),"Normal/hover PNG failed");
                        Json observed{{"actual_sandbox_renderer",true},{"scripted_software",true},{"native_human","NOT_RUN"},
                            {"real_map_and_model",true},{"tick",view.world().ticks()},{"building_id",static_cast<unsigned>(id)},
                            {"visual",visual_json(d)},{"window",size},{"zoom",view.camera().zoom},
                            {"camera_offset",{view.camera().offset.x,view.camera().offset.y}},
                            {"map_rect",{view.layout().map.x,view.layout().map.y,view.layout().map.w,view.layout().map.h}},
                            {"pointer_render_point",{pointer.x,pointer.y}},{"selected_building",nullptr},{"hovered_house",hovered},
                            {"native_material_source_pixel",{126,48}},{"native_material_render_point",{material_point.x,material_point.y}},
                            {"native_material_rgba",material},
                            {"rule",view.world().rule_version()},{"schema",persisted.source_schema_version},
                            {"funds",view.world().treasury()},{"construction_spent",view.world().construction_spent_total()},
                            {"complete_world_equal",true},{"persisted_save_world_equal",true}};
                        f::write_text(out/(extra+".observed.json"),observed.dump(2)+"\n");
                    }
                }
            }
        }
    }
    // Render/hover only: no new preparation, simulation or navigation work.
    perf::set_enabled(true);perf::reset();
    for(unsigned n=0;n<10;++n) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=1;e.motion.y=1;event(view,e);
        view.update(0);check(view.render(),"Pure vacant/Shelter frame failed");
    }
    for(std::size_t n=0;n<static_cast<std::size_t>(perf::Counter::Count);++n)
        check(perf::counter(static_cast<perf::Counter>(n))==0,"Vacant/Shelter render or hover performed preparation/World work");
    perf::set_enabled(false);check(view.world().snapshot()==authority,"Pure vacant frames changed authority");
    (void)data;
}
void real_fifth_house(SDL_Window* window,SDL_Renderer* renderer,const fs::path& original,
        const fs::path& resources,const fs::path& out,const fs::path& root) {
    check(SDL_SetWindowSize(window,1280,800),"Real fifth-House initial resize failed");
    const auto checked=oe::maps::check_map_rules({fs::canonical(original),"Cities/Xia.map",sim::RulesProfile::CityV16,18,3});
    check(checked.status==oe::maps::MapRulesStatus::MapRulesChecked && !checked.input_sha256.empty(),
        "Real Xia/Rule18 map-rules preflight failed");
    auto session=oe::maps::load_stored_map_session(original,"Cities/Xia.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
        oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,checked.input_sha256,true);
    auto view=oe::SandboxView::create_original_deadline_food_dispatch(std::move(session),orig::Difficulty::Normal,true);
    const auto art=oe::detect_and_select_visual_profiles(original,resources);
    if(!art.building.empty())view->set_building_visuals(art.building,art.building_source);
    if(!art.road.empty())view->set_road_visuals(art.road,art.road_source);
    view->set_original_house_visuals(art.original_house,art.original_house_source,art.original_house_fallback_reason);
    view->set_original_building_visuals(art.original_building,art.original_building_source,art.original_building_fallback_reason);
    assets::BuildingVisualProfile profile;assets::append_original_house_visual_profile(original,art.original_house,profile);
    const auto save=root/"save/oe59-paid-fifth.json";view->configure_save(original,"Cities/Xia.map",save);
    view->initialize(window,renderer);
    const auto& initial=view->world().original_economy_state()->houses;
    check(view->world().rule_version()==18 && initial.size()==4 && !view->world().total_population() &&
        view->world().treasury()==10596 && view->world().ticks()==0,"Real Xia/Normal four-House starter differs");
    const auto origin=view->world().building(initial.front().building_id).cell;
    const auto first_ground=view->original_house_visual_diagnostic(initial.front().building_id).ground;
    for(const auto& h:initial) {
        const auto d=view->original_house_visual_diagnostic(h.building_id);
        const orig::HouseState empty{.type=orig::HouseType::Vacant};
        check(h.house==empty && d.active && d.asset && d.asset->image_index==1508 &&
            d.image_width==158 && d.image_height==88 && d.ground_x==79 && d.ground_y==68 && !d.authored_vacant_foundation,
            "Actual paid four-House starter did not select native1508 from its empty persisted origin");
    }
    vacant_matrix(*view,window,renderer,original,save,out,initial.front().building_id,"xia-starter-vacant",false,true,&profile);
    zoom(*view,renderer,1);if(view->debug_diagnostics())key(*view,SDLK_F1);
    buy(*view,sim::CommandType::PlaceRoad,{origin.x+16,origin.y+1});
    const auto before=view->world().treasury();const auto spent=view->world().construction_spent_total();
    const auto sequence=view->world().command_sequence();
    const Point fifth_ground{first_ground.x+12*40,first_ground.y+12*20};
    center(*view,{fifth_ground.x,fifth_ground.y-40});view->set_tool(5);
    check(view->render(),"Vacant preview background frame failed");
    const auto preview_sample=view->camera().world_to_screen({fifth_ground.x+47.5,fifth_ground.y-19.5});
    const auto background=pixel(pixels(renderer).get(),preview_sample);
    const auto preview_authority=view->world().snapshot();view->set_tool(7);
    const auto pointer=window_point(renderer,view->camera().world_to_screen({fifth_ground.x,fifth_ground.y-40}));
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=static_cast<float>(pointer.x);
    motion.motion.y=static_cast<float>(pointer.y);event(*view,motion);view->update(0);
    check(view->hovered_cell()==sim::Cell{origin.x+12,origin.y} && view->preview(*view->hovered_cell()).accepted,
        "Actual fifth-House preview did not target its legal paid footprint");
    SDL_BlendMode blend_before{},blend_after{};check(SDL_GetRenderDrawBlendMode(renderer,&blend_before),"Preview blend query failed");
    check(view->render() && SDL_GetRenderDrawBlendMode(renderer,&blend_after) && blend_after==blend_before,
        "Vacant preview leaked renderer blend state");
    const auto preview=pixel(pixels(renderer).get(),preview_sample);
    for(std::size_t channel=0;channel<3;++channel) {
        const auto* entry=profile.find_original_house(2,2);check(entry!=nullptr,"Native Vacant preview lacks its admitted image");
        const auto color=native(profile.unique_images.at(entry->image_index),126,48);
        check(color[3]==255,"Independent Vacant preview witness is transparent");
        const double expected=(128.*color[channel]+127.*background[channel])/255.;
        check(std::abs(preview[channel]-expected)<3,"Vacant preview lost actual native1508 translucent compositing");
    }
    check(view->world().snapshot()==preview_authority,"Placement preview bought or changed the unbuilt House");
    if(!out.empty()) {
        const auto frame=pixels(renderer);check(SDL_SavePNG(frame.get(),(out/"oe59-xia-fifth-preview.png").string().c_str()),"Vacant preview PNG failed");
        f::write_text(out/"oe59-xia-fifth-preview.observed.json",Json{{"actual_sandbox_renderer",true},{"scripted_software",true},
            {"native_human","NOT_RUN"},{"real_map_and_model",true},{"window",{1280,800}},{"zoom",view->camera().zoom},
            {"background_pixel",background},{"preview_pixel",preview},{"complete_world_equal",true},
            {"renderer_blend_state_equal",true},{"not_purchased",true}}.dump(2)+"\n");
    }
    click(*view,renderer,view->camera().world_to_screen({fifth_ground.x,fifth_ground.y-40}));
    const auto id=view->world().building_owner_at({origin.x+12,origin.y});
    check(id && view->world().original_economy_state()->houses.size()==5 &&
        view->world().treasury()==before-80 && view->world().construction_spent_total()==spent+80 &&
        view->world().command_sequence()==sequence+1 && view->world().ticks()==0 && house(*view,*id).occupants==0,
        "Actual fifth-House map click did not buy exactly one empty80-Funds House");
    const auto construction=view->world().construction_spent_total();
    if(!out.empty())oe::persistence::write_save(out/"oe59-xia-fifth-vacant.save.json",view->capture_save_document(),original,view->buildable_mask());
    vacant_matrix(*view,window,renderer,original,save,out,*id,"xia-fifth-vacant",false,true,&profile);
    const auto empty=view->world().snapshot();key(*view,SDLK_F4);
    const auto fallback=view->original_house_visual_diagnostic(*id);
    check(fallback.configured && !fallback.active && fallback.authored_vacant_foundation &&
        fallback.asset && fallback.asset->image_index==1508 && fallback.fallback_reason=="F4 authored comparison",
        "F4 did not preserve the native identity and visible OE58 foundation fallback");
    center(*view,{fallback.ground.x,fallback.ground.y-20});check(view->render(),"OE58 F4 fallback frame failed");
    if(!out.empty()) {
        const auto frame=pixels(renderer);check(SDL_SavePNG(frame.get(),(out/"oe59-xia-fifth-f4-foundation.png").string().c_str()),"F4 foundation PNG failed");
    }
    key(*view,SDLK_F4);check(view->world().snapshot()==empty && view->original_house_visual_diagnostic(*id).active,
        "Native/foundation comparison changed authority or failed to restore native art");
    key(*view,SDLK_SPACE);check(!view->paused(),"Space did not start actual immigration");
    while(!house(*view,*id).occupants && view->world().ticks()<2000)view->update(.1);
    key(*view,SDLK_SPACE);check(view->paused(),"Space did not pause the admitted House");
    check(house(*view,*id).occupants>0 && house(*view,*id).type==orig::HouseType::Shelter &&
        view->world().building_owner_at({origin.x+12,origin.y})==id &&
        view->world().construction_spent_total()==construction && view->world().command_sequence()==sequence+1,
        "Real immigration did not preserve the paid House identity/construction while changing Vacant2 to Shelter3");
    vacant_matrix(*view,window,renderer,original,save,out,*id,"xia-fifth-shelter",true,true,&profile);
    if(!out.empty())oe::persistence::write_save(out/"oe59-xia-fifth-shelter.save.json",view->capture_save_document(),original,view->buildable_mask());
    // Current World has no supported eviction-to-Vacant command. A former
    // resident cannot be relabelled as a fresh purchase through JSON/restore.
    const auto admitted=view->world().snapshot();const auto bytes=f::read_text(save);
    auto bad=view->capture_save_document();
    for(auto& h:bad.world.original_economy->houses)if(h.building_id==*id)h.house=orig::HouseState{.type=orig::HouseType::Vacant};
    bool refused=false;
    try {oe::persistence::write_save(save,bad,original,view->buildable_mask());}
    catch(const std::exception&){refused=true;}
    check(refused && f::read_text(save)==bytes && view->world().snapshot()==admitted &&
        view->original_house_visual_diagnostic(*id).asset->image_index==1509,
        "Forged formerly occupied Vacant was published, reset authority or selected fresh1508");
    view->shutdown();
}
void vacant_grass_contrast(SDL_Window* window,SDL_Renderer* renderer,const fs::path& out,bool dark) {
    oe::testing::economy::Temp temp;const auto data=fs::canonical(temp.root)/"data";
    Bytes grass(3200);for(std::size_t n=0;n<grass.size();n+=2)u16(grass,n,dark ? 0x0d23:0x3f6a);
    write(data/"DATA/China_Terrain.555",grass);
    fs::create_directories(data/"Model");f::write_text(data/"Model/EmperorBuildingModels.txt",oe_finance_fixture::authored_model(3));
    auto session=oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
        oe::maps::StoredGraphicsProfile::Slot8);
    auto view=oe::SandboxView::create_original_production_save_fixture(std::move(session));
    const auto save=temp.root/"saves/vacant.json";view->configure_save(data,"Cities/A.map",save);view->initialize(window,renderer);
    buy(*view,sim::CommandType::PlaceRoad,{111,113});const auto id=buy(*view,sim::CommandType::PlaceHousehold,{110,111});
    vacant_matrix(*view,window,renderer,data,save,out,id,dark ? "authored-dark-grass":"authored-light-grass",false,false);
    view->shutdown();
}
void camera_edges(oe::SandboxView& view,SDL_Renderer* renderer,const assets::BuildingVisualProfile& profile,
                  const fs::path& data,const fs::path& out,sim::BuildingId id,sim::BuildingId other) {
    const auto saved=view.world().snapshot();const auto d=view.original_house_visual_diagnostic(id);
    const std::string prefix=d.house_type==2 ? "houses.vacant-":"houses.";
    const auto* entry=profile.find_original_house(d.house_type,d.actual_side);check(entry!=nullptr,"Culling witness lacks proven native House art");
    const auto& image=profile.unique_images.at(entry->image_index);
    for (double z:{1.,2.,4.}) {
        zoom(view,renderer,z);if (!view.debug_diagnostics()) key(view,SDLK_F1);view.set_tool(5);
        const auto map=view.layout().map;
        // Put the actual front/ground center below the aperture. The native
        // image's independently opaque upper pixels must still draw and pick.
        pan(view,{std::floor(map.x+map.w*.5-(d.image_origin.x+image.width*.5)*z),
                  std::floor(map.y+map.h+12-d.ground.y*z)});
        check(view.render(),"Ground-outside/native-roof-inside culling frame failed");const auto frame=pixels(renderer);
        const auto ground=view.camera().world_to_screen(d.ground),origin=view.camera().world_to_screen(d.image_origin);
        check(ground.y>=map.y+map.h && !map.contains(ground.x,ground.y),"Culling fixture retained its ground inside the map");
        std::optional<Point> visible;
        for (int y=0;y<image.height && !visible;++y) for (int x=0;x<image.width && !visible;++x) {
            const auto color=native(image,x,y);if (color[3]!=255 || entry->ground_y-y<24) continue;
            const Point p{origin.x+(x+.5)*z,origin.y+(y+.5)*z};if (!map.contains(p.x,p.y) || pixel(frame.get(),p)!=color) continue;
            click(view,renderer,p);if (view.selected_building()==id && !view.input_diagnostic_state().selected_landscape) visible=p;
        }
        check(visible.has_value(),"Visible native roof was culled or unpickable because its ground center lies outside map");
        capture(view,renderer,data,out,id,prefix+"camera-edge.zoom"+std::to_string(static_cast<int>(z)),
            {{"front_ground_outside_map",{ground.x,ground.y}},{"visible_opaque_native_pixel",{visible->x,visible->y}},
             {"native_edge_culling_and_picking",true}});
    }
    zoom(view,renderer,1);select_owned(view,renderer,other);if (!view.debug_diagnostics()) key(view,SDLK_F1);
    const auto panel=view.layout().panel;check(view.layout().panel_open && panel.w>30 && panel.h>60,"UI-blocking witness requires the actual sidepanel");
    std::optional<Point> opaque;
    for (int y=1;y<image.height-1 && !opaque;++y) for (int x=1;x<image.width-1 && !opaque;++x)
        if (native(image,x,y)[3]==255) opaque=Point{double(x)+.5,double(y)+.5};
    check(opaque.has_value(),"UI-blocking native House contains no opaque pixel");
    const Point covered{double(panel.x+panel.w-12),double(panel.y+panel.h-30)};
    pan(view,{covered.x-d.image_origin.x-opaque->x,covered.y-d.image_origin.y-opaque->y});check(view.render(),"Clipped original House/UI frame failed");
    check(panel.contains(covered.x,covered.y) && !view.layout().map.contains(covered.x,covered.y),"Covered native pixel did not lie under actual UI");
    click(view,renderer,covered,false);
    check(view.selected_building()==other && !view.input_diagnostic_state().selected_landscape,
        "Native image under the sidepanel intercepted a real UI click");
    capture(view,renderer,data,out,id,prefix+"ui-clipped",{{"opaque_native_pixel_covered_by_ui",{covered.x,covered.y}},
        {"different_selected_house",static_cast<unsigned>(other)},{"ui_click_did_not_select_hidden_house",true}});
    check(view.world().snapshot()==saved,"Camera-edge or UI clipping proof changed paid World authority");
}
void ramp_surface(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& data,const fs::path& out,sim::BuildingId id) {
    const auto before=view.world().snapshot();check(view.world().ramps().size()==1 && view.road_visuals_active(),"Actual ramp comparison lacks its paid link/active road material");
    const auto& ramp=view.world().ramps().front();
    check(ramp.endpoint_a==sim::Cell{125,94} && ramp.endpoint_b==sim::Cell{126,94} && ramp.height_a==1 && ramp.height_b==0,
        "Actual same-World paid ramp differs from the independently authored signed delta1");
    key(view,SDLK_F6);check(!view.road_visuals_active(),"F6 did not select the prepared authored ramp material");
    for (double z:{1.,2.}) {
        zoom(view,renderer,z);const auto world=projected(125.43,94.07,.57);center(view,world);
        if (!view.debug_diagnostics()) key(view,SDLK_F1);view.set_tool(5);
        check(view.render(),"Actual Rule10 authored ramp fallback frame failed");const auto frame=pixels(renderer);
        const auto point=view.camera().world_to_screen(world);const auto color=pixel(frame.get(),point);
        check(color[3]==255 && color[0]>=120 && color[1]>=110 && color[0]>color[1] && color[2]>=65 && color[2]<color[1],
            "F6-off actual graded surface has no visible compacted-earth/stone pixel at its independent signed-height point");
        click(view,renderer,point);const auto picked=view.input_diagnostic_state();
        check(picked.selected_cell && (*picked.selected_cell==ramp.endpoint_a || *picked.selected_cell==ramp.endpoint_b) &&
            !picked.selected_landscape && !picked.selected_walker && !view.selected_building(),
            "Actual opaque graded ramp pixel did not select one of its real paid endpoints");
        key(view,SDLK_F1);capture(view,renderer,data,out,id,"houses.rule10-ramp-f6-off.zoom"+std::to_string(static_cast<int>(z)),
            {{"independent_world_surface",{world.x,world.y}},{"visible_surface_pixel",{point.x,point.y}},{"surface_rgba",color},
             {"picked_paid_endpoint",{picked.selected_cell->x,picked.selected_cell->y}},{"authored_terrain_sides",false},
             {"road_material_enabled",false},{"original_house_art_active",view.original_house_visual_diagnostic(id).active}});
    }
    key(view,SDLK_F6);check(view.road_visuals_active() && view.world().snapshot()==before && view.authored_terrain_side_count()==0,
        "Authored ramp comparison changed paid authority or enabled Rule6-only terrain sides");
}
bool walker_feet(oe::SandboxView& view,SDL_Renderer* renderer,const assets::WalkerVisualProfile& profile,
                 const fs::path& data,const fs::path& out,sim::BuildingId house_id) {
    for (const auto& courier:view.world().couriers()) {
        if (courier.role!=sim::CourierRole::Clay || !courier.enabled || courier.edge_progress<=0 ||
            courier.path_vertex+1>=courier.path.size()) continue;
        const auto a=courier.path[courier.path_vertex],b=courier.path[courier.path_vertex+1];
        if (a.x>125 || b.x>125) continue;
        const double t=static_cast<double>(courier.edge_progress)/view.world().active_rules().courier_edge_ticks;
        const auto expected=projected(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t);
        zoom(view,renderer,2);center(view,expected);check(view.render(),"Real moving native Walker frame failed");
        const auto pose=oe::walker_pose(courier,view.world().ticks(),profile);check(pose.role && pose.frame,"Moving Clay lacks actual native frame");
        const auto& frame=profile.find(*pose.role)->frames.at(*pose.frame);const auto wanted=view.camera().world_to_screen(expected);
        for (const auto& row:view.walker_diagnostics()) if (row.id==courier.id) {
            check(row.submitted && row.sprite_drawn && row.native_asset==frame.id && std::abs(row.screen_ground.x-wanted.x)<1e-6 &&
                  std::abs(row.screen_ground.y-wanted.y)<1e-6 && std::abs(row.image_origin.x+frame.foot_x*2-wanted.x)<1e-6 &&
                  std::abs(row.image_origin.y+frame.foot_y*2-wanted.y)<1e-6,"Native Walker feet lost actual paid edge progress/signed ground beside House art");
            const auto rendered=pixels(renderer);const auto& image=profile.unique_images.at(frame.image_index);std::optional<Point> visible;
            for (int y=0;y<image.height && !visible;++y) for (int x=0;x<image.width && !visible;++x) {
                const auto color=native(image,frame.flip_x ? image.width-1-x:x,y);if (color[3]!=255) continue;
                const Point p{row.image_origin.x+(x+.5)*2,row.image_origin.y+(y+.5)*2};
                if (view.layout().map.contains(p.x,p.y) && pixel(rendered.get(),p)==color) visible=p;
            }
            // Submitted geometry alone is insufficient: continue normal ticks
            // until at least one independently opaque native pixel is visible.
            if (!visible) return false;
            capture(view,renderer,data,out,house_id,"houses.walker-feet",{{"courier_id",static_cast<unsigned>(courier.id)},
                {"edge_from",{a.x,a.y}},{"edge_to",{b.x,b.y}},{"edge_progress",courier.edge_progress},
                {"independent_screen_ground",{wanted.x,wanted.y}},{"visible_opaque_native_pixel",{visible->x,visible->y}},
                {"actual_native_walker",true}});return true;
        }
    }
    return false;
}
void foreground( SDL_Window* window,SDL_Renderer* renderer,const fs::path& data,const fs::path& metadata,
                 const assets::BuildingVisualProfile& profile,const fs::path& out,bool vacant=false) {
    const auto session=[&](const char* map){return oe::maps::load_stored_map_session(data,map,
        oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);};
    std::array<std::unique_ptr<oe::SandboxView>,2> views;
    std::array<sim::BuildingId,2> ids{};
    for (unsigned n=0;n<2;++n) {
        const auto map=n ? "Cities/B.map":"Cities/A.map";
        views[n]=oe::SandboxView::create_original_production_save_fixture(session(map));
        views[n]->configure_save(data,map,data/(n ? "foreground-save.json":"foreground-control-save.json"));
        views[n]->set_original_house_visuals(metadata);views[n]->initialize(window,renderer);
        buy(*views[n],sim::CommandType::PlaceRoad,{112,107});ids[n]=buy(*views[n],sim::CommandType::PlaceHousehold,{112,108});
        if(!vacant)for (unsigned i=0;i<120;++i) views[n]->tick_once();
        check(house(*views[n],ids[n]).type==(vacant ? orig::HouseType::Vacant:orig::HouseType::Shelter),
            "Isolated paid foreground House has the wrong actual empty/admitted state");
        key(*views[n],SDLK_F1);views[n]->set_tool(5);
    }
    check(views[0]->world().snapshot()==views[1]->world().snapshot(),"Authored foreground changed the identical paid World control");
    const auto* entry=profile.find_original_house(vacant ? 2:3,2);check(entry!=nullptr,"Actual House foreground witness has no proven native sprite");
    const auto& image=profile.unique_images.at(entry->image_index);
    for (double z:{1.,2.,4.}) {
        std::array<Surface,2> frames{Surface(nullptr,SDL_DestroySurface),Surface(nullptr,SDL_DestroySurface)};
        for (unsigned n=0;n<2;++n) {
            zoom(*views[n],renderer,z);const auto d=views[n]->original_house_visual_diagnostic(ids[n]);
            center(*views[n],{d.image_origin.x+image.width*.5,d.image_origin.y+image.height*.5});
            check(views[n]->render(),"Actual foreground/control native House frame failed");frames[n]=pixels(renderer);
        }
        const auto d=views[1]->original_house_visual_diagnostic(ids[1]);const auto origin=views[1]->camera().world_to_screen(d.image_origin);
        const auto fg=projected(foreground_cell.x,foreground_cell.y);const Point fg_origin{fg.x-39,fg.y-160};
        std::optional<Point> solid,hole;Color hole_color{};
        for (int y=0;y<image.height && (!solid || !hole);++y) for (int x=0;x<image.width && (!solid || !hole);++x) {
            const auto color=native(image,x,y);if (color[3]!=255 || color==Color{0,0,255,255}) continue;
            const double wx=d.image_origin.x+x+.5,wy=d.image_origin.y+y+.5;
            const int fx=static_cast<int>(std::floor(wx-fg_origin.x)),fy=static_cast<int>(std::floor(wy-fg_origin.y));
            if (fx<0 || fx>=78 || fy<0 || fy>=180) continue;
            const Point p{origin.x+(x+.5)*z,origin.y+(y+.5)*z};if (!views[1]->layout().map.contains(p.x,p.y)) continue;
            if (pixel(frames[0].get(),p)!=color) continue;
            if (!solid && (fx/8)%2==0 && pixel(frames[1].get(),p)==Color{0,0,255,255}) solid=p;
            if (!hole && (fx/8)%2==1 && pixel(frames[1].get(),p)==color) {hole=p;hole_color=color;}
        }
        check(solid.has_value() && hole.has_value(),"Actual production painter lost the authored opaque-front/native-House-hole pair");
        check(views[1]->render(),"Foreground actual input frame failed");click(*views[1],renderer,*solid);
        check(views[1]->input_diagnostic_state().selected_landscape,"Opaque foreground click selected the hidden House instead of the visible stored overlay");
        click(*views[1],renderer,*hole);check(views[1]->selected_building()==ids[1] && !views[1]->input_diagnostic_state().selected_landscape,
            "Actual foreground transparent opening did not select the visible native House");
        capture(*views[1],renderer,data,out,ids[1],std::string(vacant ? "houses.vacant-foreground.zoom":"houses.foreground.zoom")+std::to_string(static_cast<int>(z)),
            {{"opaque_foreground_pixel",{solid->x,solid->y}},{"native_house_through_hole",{hole->x,hole->y}},
             {"opaque_expected_rgba",{0,0,255,255}},{"hole_expected_rgba",hole_color},
             {"authored_foreground",true},{"original_cliff_identity",false},{"paid_control_snapshot_equal",true}});
        check(views[0]->world().snapshot()==views[1]->world().snapshot(),"Foreground pixel/picking comparison changed paid World authority");
    }
    for (auto& view:views) {view->shutdown();view.reset();}
}
void legacy_refusal(SDL_Window* window,SDL_Renderer* renderer,const fs::path& data,const fs::path& metadata) {
    auto session=oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
        oe::maps::StoredGraphicsProfile::Slot8);
    oe::SandboxView legacy(std::move(session),false,sim::RulesProfile::CityV16,4);
    legacy.configure_save(data,"Cities/A.map",data/"legacy-refusal.json");
    legacy.set_building_visuals(metadata.parent_path()/"buildings.json",oe::VisualProfileSource::Builtin);
    legacy.initialize(window,renderer);check(legacy.render(),"Legacy core-profile negative-control frame failed");
    const auto before=legacy.world().snapshot();const auto textures=legacy.building_texture_count();
    check(textures>0 && !legacy.original_house_display_stats().configured,"Legacy negative control lacks its actual unchanged core textures");
    bool refused=false;perf::set_enabled(true);perf::reset();
    try {legacy.set_original_house_visuals(metadata,oe::VisualProfileSource::Custom);}
    catch (const std::exception& error) {refused=std::string(error.what()).find("require explicit Original production")!=std::string::npos;}
    check(refused,"Explicit custom original House art silently accepted an incompatible Legacy World");
    legacy.set_original_house_visuals(metadata,oe::VisualProfileSource::Builtin);
    const auto result=legacy.original_house_display_stats();
    check(!result.configured && !result.active && result.fallback_reason.find("Original production")!=std::string::npos,
        "Optional built-in House supplement on Legacy lost its named inactive reason");
    for (std::size_t n=0;n<static_cast<std::size_t>(perf::Counter::Count);++n)
        check(perf::counter(static_cast<perf::Counter>(n))==0,"Incompatible House supplement performed asset/World/navigation work");
    perf::set_enabled(false);
    check(legacy.world().snapshot()==before && legacy.building_texture_count()==textures && legacy.render(),
        "Incompatible House supplement changed Legacy authority/core textures/rendering");legacy.shutdown();
}
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==4 && std::string(argv[1])=="--real","Expected --real DATA RESOURCE_ROOT");
        const auto original=fs::canonical(argv[2]),resources=fs::canonical(argv[3]);
        const auto metadata=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1/original-houses.json";
        const auto walkers=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1/walkers.json";
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") && SDL_Init(SDL_INIT_VIDEO),"Original House visual SDL init failed");
        check(SDL_CreateWindowAndRenderer("Paid original House art",1280,900,SDL_WINDOW_HIDDEN,&window,&renderer),"Original House software window failed");
        check(std::string(SDL_GetRendererName(renderer))=="software","Original House visual test did not use actual SDL software rendering");
        oe::testing::economy::Temp temp;const auto root=fs::canonical(temp.root),data=root/"data";
        fs::path out;if (const auto* dir=std::getenv("OPENEMPEROR_UI_CAPTURE_DIR")) {out=dir;fs::create_directories(out);}
        real_fifth_house(window,renderer,original,resources,out,root);
        vacant_grass_contrast(window,renderer,out,false);vacant_grass_contrast(window,renderer,out,true);
        check(SDL_SetWindowSize(window,1280,900),"Restore historical House visual test window failed");
        for (const auto* name:{"China_General.sg3","China_General.555","SprMain.sg3","SprMain.555"})
            check(fs::copy_file(original/"DATA"/name,data/"DATA"/name),"Read-only original asset copy into private fixture failed");
        authored_foreground(data);write(data/"Cities/A.map",map_bytes(false));write(data/"Cities/B.map",map_bytes(true));
        fs::create_directories(data/"Model");f::write_text(data/"Model/EmperorBuildingModels.txt",oe_finance_fixture::authored_model(3));
        assets::BuildingVisualProfile profile;assets::append_original_house_visual_profile(data,metadata,profile);
        check(profile.original_house_prepared && !profile.unique_images.empty(),"Protected real House extension prepared no original native images");
        const auto walker_profile=assets::load_walker_visual_profile(data,walkers);
        const auto session=[&](const char* map){return oe::maps::load_stored_map_session(data,map,oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);};
        const auto manual=root/"save/houses.json";auto view=oe::SandboxView::create_original_production_save_fixture(session("Cities/A.map"));
        view->configure_save(data,"Cities/A.map",manual);view->set_original_house_visuals(metadata);
        view->set_walker_visuals(walkers);view->set_road_visuals(road_profile(data));view->initialize(window,renderer);
        check(view->paused() && view->original_house_display_stats().configured && view->original_house_display_stats().active,
            "Explicit protected original House extension did not activate");
        const auto scene=build(*view);const auto first=scene.commons.front();
        stages(*view,renderer,profile,data,out,first,"houses.common-vacant",2);
        stages(*view,renderer,profile,data,out,scene.elite,"houses.elite-plot",11);
        camera_edges(*view,renderer,profile,data,out,first,scene.commons[1]);
        while (view->world().ticks()<120) view->tick_once();
        check(view->world().building(scene.elite).kind==sim::Object::OriginalEliteHouse && house(*view,scene.elite).occupants==2,
            "Paid first admission did not atomically materialize the real4x4 Elite body");
        stages(*view,renderer,profile,data,out,first,"houses.common-shelter",3);
        stages(*view,renderer,profile,data,out,scene.elite,"houses.elite-first-modest",13);
        bool hut=false,abandoned=false;
        for (unsigned i=0;i<12000 && (!hut || !abandoned);++i) {
            view->tick_once();
            if (!hut && house(*view,first).type==orig::HouseType::Hut) {stages(*view,renderer,profile,data,out,first,"houses.common-hut",4);hut=true;}
            if (!abandoned && house(*view,scene.elite).type==orig::HouseType::EliteAbandoned) {
                check(view->world().building(scene.elite).kind==sim::Object::OriginalEliteHouse,"Actual Elite downgrade discarded its paid4x4 body");
                stages(*view,renderer,profile,data,out,scene.elite,"houses.elite-abandoned",12);abandoned=true;
            }
        }
        check(hut && abandoned,"Paid housing never produced actual Common3→4 and Elite13→12 shortage transitions");
        for (const auto id:scene.paused) command(*view,sim::set_building_operation(id,true));
        bool plain=false,recovered=false,feet=false;
        for (unsigned i=0;i<30000 && (!plain || !recovered || !feet);++i) {
            view->tick_once();
            if (!plain && house(*view,first).type==orig::HouseType::PlainCottage) {stages(*view,renderer,profile,data,out,first,"houses.common-plain",5);plain=true;}
            if (!recovered && house(*view,scene.elite).type==orig::HouseType::ModestSiheyuan) {
                check(house(*view,scene.elite).hemp>0 && house(*view,scene.elite).ceramics>0,"Elite recovery has no real delivered material witness");
                stages(*view,renderer,profile,data,out,scene.elite,"houses.elite-recovered",13);recovered=true;
            }
            if (!feet) feet=walker_feet(*view,renderer,walker_profile,data,out,scene.elite);
        }
        check(plain && recovered && feet,"Paid supply never achieved real Common5, Elite12→13 and moving native Walker feet");
        zoom(*view,renderer,1);center(*view,projected(125.5,94,.5));check(view->render(),"Paid Rule10 ramp/House same-scene render failed");
        check(view->original_house_visual_diagnostic(scene.commons.back()).active && view->world().ramps().size()==1 && view->authored_terrain_side_count()==0,
            "Actual paid Rule10 ramp context lost original House art or widened authored-cliff authority");
        capture(*view,renderer,data,out,scene.commons.back(),"houses.rule10-ramp-context",{{"authored_signed_delta",1},{"authored_terrain_sides",false}});
        ramp_surface(*view,renderer,data,out,scene.commons.back());
        camera_edges(*view,renderer,profile,data,out,scene.elite,first);
        zoom(*view,renderer,1);select_owned(*view,renderer,scene.elite);verify_visual(*view,renderer,profile,scene.elite);
        const auto saved=view->world().snapshot();const auto textures=view->building_texture_count();const auto selected=view->original_house_visual_diagnostic(scene.elite);
        key(*view,SDLK_F4);check(view->render(),"F4 authored comparison failed");const auto off=view->original_house_visual_diagnostic(scene.elite);
        check(off.configured && !off.active && off.asset==selected.asset && off.fallback_reason=="F4 authored comparison","F4 lost the prepared original identity or borrowed another stage");
        capture(*view,renderer,data,out,scene.elite,"houses.f4-authored");key(*view,SDLK_F4);
        key(*view,SDLK_F6);check(!view->road_visuals_active() && view->original_house_visual_diagnostic(scene.elite).active && view->render(),"F6 changed original House art");
        capture(*view,renderer,data,out,scene.elite,"houses.f6-roads-off");key(*view,SDLK_F6);
        check(view->debug_diagnostics(),"Alpha picking did not use the actual F1 visual inspection path");
        key(*view,SDLK_F8);check(view->render(),"Actual flat Snapshot comparison failed");
        const auto flat=view->original_house_visual_diagnostic(scene.elite);
        check(flat.asset==selected.asset && flat.ground.x==selected.ground.x && flat.ground.y==selected.ground.y+40,
            "Snapshot changed original physical art or retained an elevated ground anchor");
        capture(*view,renderer,data,out,scene.elite,"houses.snapshot-flat");
        for (unsigned i=0;i<7;++i) key(*view,SDLK_F8);
        check(view->render() && view->original_house_visual_diagnostic(scene.elite).ground.y==selected.ground.y,"F8 return lost the exact signed-height anchor");
        key(*view,SDLK_F7);check(view->render(),"F7 historical painter comparison failed");key(*view,SDLK_F7);
        check(view->world().snapshot()==saved,"F4/F6/F7/F8 comparisons mutated World authority");
        key(*view,SDLK_F5);const auto bytes=f::read_text(manual);const auto document=oe::persistence::read_save(manual);
        check(document.world==saved && document.source_schema_version==26,"Actual F5 changed original production save authority");
        for (unsigned i=0;i<23;++i) view->tick_once();
        perf::set_enabled(true);perf::reset();key(*view,SDLK_F9);
        check(perf::counter(perf::Counter::AssetDecodes)==0 && perf::counter(perf::Counter::TextureUploads)==0,
            "F9 decoded or uploaded already prepared original House art");perf::set_enabled(false);
        check(view->world().snapshot()==saved && f::read_text(manual)==bytes && view->building_texture_count()==textures &&
            view->original_house_visual_diagnostic(scene.elite).asset==selected.asset,"Actual F9 changed paid state/save bytes/prepared sprite identity");
        zoom(*view,renderer,1);select_owned(*view,renderer,scene.elite);verify_visual(*view,renderer,profile,scene.elite);
        const auto pure=view->world().snapshot();const auto cache=view->world().original_influence_cache_build_count();
        const auto plans=view->road_plan_build_count(),water=view->water_preview_build_count(),routes=view->road_connectivity_check_count();
        const auto walker_textures=view->walker_texture_count();perf::set_enabled(true);perf::reset();
        for (unsigned i=0;i<100;++i) {
            if (i%10==0) {key(*view,SDLK_F4);key(*view,SDLK_F6);}
            const auto map=view->layout().map;const auto p=window_point(renderer,{map.x+map.w*.5+i%3,map.y+map.h*.5});
            SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=static_cast<float>(p.x);e.motion.y=static_cast<float>(p.y);event(*view,e);
            view->update(0);check(view->render(),"Pure original House visual frame failed");
            (void)view->original_house_visual_diagnostic(scene.elite);
        }
        for (std::size_t n=0;n<static_cast<std::size_t>(perf::Counter::Count);++n)
            if (perf::counter(static_cast<perf::Counter>(n))) throw std::runtime_error("Pure original House frame performed "+std::string(perf::counter_names[n]));
        perf::set_enabled(false);
        check(view->world().snapshot()==pure && view->world().original_influence_cache_build_count()==cache &&
            view->road_plan_build_count()==plans && view->water_preview_build_count()==water && view->road_connectivity_check_count()==routes &&
            view->walker_texture_count()==walker_textures && view->building_texture_count()==textures,"Pure original House frames changed caches/routes/textures/World");
        foreground(window,renderer,data,metadata,profile,out,true);
        foreground(window,renderer,data,metadata,profile,out);
        legacy_refusal(window,renderer,data,metadata);
        // Prepared sprite selection must survive loss of its *private* copied
        // source, never delete or rewrite the supplied original installation.
        fs::remove(data/"DATA/China_General.sg3");fs::remove(data/"DATA/China_General.555");
        check(view->render() && view->original_house_visual_diagnostic(scene.elite).asset==selected.asset && view->building_texture_count()==textures,
            "Prepared original House rendering reread unavailable private source assets");
        view->shutdown();view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Original House actual General native1/2/4, paid Common2→3→4→5 and Elite11→13→12→13, alpha picking, signed-height Walker/Ramp, F4/F6/Snapshot/F5/F9 and100 pure frames PASS; authored map/model; native/human NOT_RUN\n";
        return 0;
    } catch (const std::exception& error) {
        perf::set_enabled(false);std::cerr<<error.what()<<'\n';if (renderer) SDL_DestroyRenderer(renderer);if (window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
