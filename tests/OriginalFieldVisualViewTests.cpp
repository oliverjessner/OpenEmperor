// Real, ordinarily paid Rule18 Xia/Normal production. Original files are read
// only; no authored maps, saved-state injection or private fixtures live here.
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "assets/BuildingVisualProfile.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/MapRulesCheck.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace assets=oe::assets;
namespace perf=oe::performance;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Point=oe::scene::Point;
using Color=std::array<std::uint8_t,4>;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
void check(bool value,const std::string& reason) {if(!value)throw std::runtime_error(reason);}
unsigned number(sim::BuildingId id) {return static_cast<unsigned>(id);}
std::string bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);check(bool(input),"Cannot read own saved document");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void write(const fs::path& path,const std::string& value) {
    check(!fs::exists(path)&&!fs::is_symlink(path),"Preserve prior evidence: "+path.string());
    std::ofstream output(path,std::ios::binary);output<<value;check(bool(output),"Cannot publish own evidence");
}
bool contains(const fs::path& root,const fs::path& path) {
    auto a=root.begin(),b=path.begin();
    for(;a!=root.end();++a,++b)if(b==path.end()||*a!=*b)return false;
    return true;
}
void no_symlinks(const fs::path& path) {
    fs::path current;
    for(const auto& part:fs::absolute(path).lexically_normal()) {
        current/=part;std::error_code error;const auto state=fs::symlink_status(current,error);
        check(!error||error==std::errc::no_such_file_or_directory,"Cannot inspect explicit path ancestry");
        check(error||!fs::is_symlink(state),"Explicit paths may not traverse symlinks");
    }
}
fs::path fresh_output(const fs::path& repo,const fs::path& requested) {
    no_symlinks(requested);const auto parent=fs::absolute(requested).lexically_normal();
    const auto allowed=repo/".local/oe60-5";
    check(contains(allowed,fs::weakly_canonical(parent)),"Use only the private OE60.5 output root");
    fs::create_directories(parent);
    const auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for(unsigned attempt=0;attempt<32;++attempt) {
        const auto child=parent/("fields-"+stamp+"-"+std::to_string(attempt));std::error_code error;
        if(fs::create_directory(child,error))return child;
        check(!error||error==std::errc::file_exists,"Cannot reserve fresh field evidence directory");
    }
    throw std::runtime_error("Could not reserve field evidence output");
}
void event(oe::SandboxView& view,SDL_Event value) {
    bool running=true;view.handle_event(value,running);check(running,"Scripted input closed the real session");
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);view.update(0);
}
Point window_point(SDL_Renderer* renderer,Point point) {
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y),
        "Invert the production positional-input transform");return {x,y};
}
void move(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    const auto raw=window_point(renderer,point);SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;
    e.motion.x=static_cast<float>(raw.x);e.motion.y=static_cast<float>(raw.y);event(view,e);view.update(0);
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    check(view.layout().map.contains(point.x,point.y),"Pointer witness is outside the map aperture");
    move(view,renderer,point);const auto raw=window_point(renderer,point);SDL_Event e{};
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(raw.x);e.button.y=static_cast<float>(raw.y);event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);view.update(0);
}
void center(oe::SandboxView& view,Point world) {
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,focus);
    const auto map=view.layout().map;const auto z=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-world.x*z,map.y+map.h*.5-world.y*z};
    for(unsigned n=0;n<1000;++n) {
        const auto current=view.camera().offset;const auto dx=wanted.x-current.x,dy=wanted.y-current.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Ordinary paused camera input did not converge");
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double nominal) {
    const auto map=view.layout().map;const auto p=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    for(unsigned n=0;n<3&&std::abs(view.camera().zoom-nominal)>1e-9;++n) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);e.wheel.mouse_y=static_cast<float>(p.y);
        e.wheel.y=static_cast<float>(std::log(nominal/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0);
    }
    check(std::isfinite(view.camera().zoom)&&std::abs(view.camera().zoom-nominal)<1e-9,"Actual wheel missed nominal 1/2/4 zoom");
}
Json camera(const oe::SandboxView& view) {
    const auto& c=view.camera();const auto m=view.layout().map;
    return {{"actual_zoom",c.zoom},{"offset",{c.offset.x,c.offset.y}},
        {"viewport",{c.viewport_width,c.viewport_height}},{"map_rect",{m.x,m.y,m.w,m.h}}};
}
Json resources(const oe::SandboxView& view) {
    return {{"building_textures",view.building_texture_count()},{"walker_textures",view.walker_texture_count()},
        {"influence_builds",view.world().original_influence_cache_build_count()},
        {"road_plans",view.road_plan_build_count()},{"water_previews",view.water_preview_build_count()},
        {"connectivity_checks",view.road_connectivity_check_count()}};
}
Json counters() {
    Json result=Json::object();
    for(std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i) {
        const auto value=perf::counter(static_cast<perf::Counter>(i));result[std::string(perf::counter_names[i])]=value;
        check(value==0,"Pure field frame performed "+std::string(perf::counter_names[i]));
    }
    return result;
}
Json pure_frame(oe::SandboxView& view) {
    perf::set_enabled(true);perf::reset();const bool rendered=view.render();perf::set_enabled(false);
    check(rendered,"Render the real prepared field view");return counters();
}
Surface capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& file,Json& frame_counters) {
    check(SDL_GetRenderTarget(renderer)==nullptr,"Capture begins at the default window target");
    int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h)&&w>0&&h>0,"Read actual output aperture");
    const auto before=view.world().snapshot();const auto c=camera(view),r=resources(view);
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Create owned RGBA32 readback target");
    struct Restore {SDL_Renderer* renderer;~Restore(){SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    frame_counters=pure_frame(view);check(SDL_GetRenderTarget(renderer)==target.get(),"Productive render changed the owned target");
    Surface raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(raw),"Read productive pixels before Present");
    Surface rgba(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    check(rgba&&rgba->w==w&&rgba->h==h,"Readback aperture or RGBA format differs");
    if(!file.empty())check(!fs::exists(file)&&SDL_SavePNG(rgba.get(),file.string().c_str()),"Write exactly one fresh representative PNG");
    check(view.world().snapshot()==before&&camera(view)==c&&resources(view)==r,"Readback mutated full World/camera/resources");
    return rgba;
}
Color pixel(const SDL_Surface& image,Point p) {
    const auto x=static_cast<int>(p.x),y=static_cast<int>(p.y);
    check(x>=0&&y>=0&&x<image.w&&y<image.h,"Pixel witness escaped the actual output");
    const auto* value=static_cast<const std::uint8_t*>(image.pixels)+y*image.pitch+x*4;
    return {value[0],value[1],value[2],value[3]};
}
Color source(const assets::RgbaImage& image,int x,int y) {
    const auto at=(static_cast<std::size_t>(y)*image.width+static_cast<std::size_t>(x))*4;
    return {image.pixels.at(at),image.pixels.at(at+1),image.pixels.at(at+2),image.pixels.at(at+3)};
}
struct Field {
    sim::Object kind;
    unsigned model,base;
    sim::BuildingId id{},farm{};
    std::array<assets::RgbaImage,6> images;
    std::array<bool,6> stages{};
    std::array<bool,80> progress_values{};
    std::optional<std::uint64_t> started,first_harvest;
    Field(sim::Object field_kind,unsigned model_type,unsigned first_record)
        :kind(field_kind),model(model_type),base(first_record) {}
};
// Independent interval oracle, deliberately not the production quantizer.
unsigned stage_of(int progress) {
    constexpr std::array lower{0,14,27,40,54,67};
    check(progress>=0&&progress<80,"Actual parent Farm progress is outside its existing clock");
    unsigned stage=0;while(stage+1<lower.size()&&progress>=lower[stage+1])++stage;return stage;
}
std::uint64_t harvested(const sim::World& world,const Field& field) {
    const auto& production=*world.original_production_state();
    if(field.kind==sim::Object::OriginalHempField) {
        for(const auto& farm:production.hemp_farms)if(farm.farm_id==field.farm&&farm.field_id==field.id)return farm.harvested;
    } else for(const auto& farm:production.food.farms)if(farm.farm_id==field.farm&&farm.field_id==field.id) {
        check(farm.crop==(field.kind==sim::Object::OriginalWheatField?sim::OriginalFoodCrop::Wheat:sim::OriginalFoodCrop::Millet),
            "Field crop differs from its actual paid parent record");return farm.harvested;
    }
    throw std::runtime_error("Missing exact saved Field-to-Farm ownership");
}
Json diagnostic(const oe::SandboxView& view,const Field& field) {
    const auto d=view.original_building_visual_diagnostic(field.id);const auto& farm=view.world().building(field.farm);
    const auto stage=stage_of(farm.progress);check(d.applicable&&d.configured&&d.active&&d.model_type==field.model&&
        d.actual_side==1&&d.asset&&d.asset->archive_relative_path==fs::path("DATA/China_General.sg3")&&
        d.asset->image_index==field.base+stage&&d.field_farm==field.farm&&
        d.field_stage==stage&&d.field_farm_progress==farm.progress,"Native field is not derived from its actual paid parent progress");
    check(d.image_width==78&&d.image_height==field.images[stage].height&&d.ground_x==39&&d.ground_y==d.image_height-20,
        "Canonical field lost its complete one-cell image/anchor");
    check(view.world().building(field.id).progress==0&&d.selection.find("authored")!=std::string::npos,
        "Presentation invented Field authority or omitted its authored mapping classification");
    return {{"building",number(field.id)},{"model",field.model},{"farm",number(field.farm)},
        {"farm_progress",farm.progress},{"stage_zero_based",stage},{"record",d.asset->image_index},
        {"dimensions",{d.image_width,d.image_height}},{"anchor",{d.ground_x,d.ground_y}},
        {"ground",{d.ground.x,d.ground.y}},{"image_origin",{d.image_origin.x,d.image_origin.y}},
        {"selection",d.selection},{"harvested",harvested(view.world(),field)}};
}
void configure(oe::SandboxView& view,const oe::VisualSelection& art) {
    if(!art.walker.empty())view.set_walker_visuals(art.walker,art.walker_source);
    if(!art.building.empty())view.set_building_visuals(art.building,art.building_source);
    if(!art.road.empty())view.set_road_visuals(art.road,art.road_source);
    view.set_fire_visuals(art.fire,art.fire_source,art.fire_fallback_reason);
    view.set_fire_inspector_visuals(art.fire_inspector,art.fire_inspector_source,art.fire_inspector_fallback_reason);
    view.set_market_walker_visuals(art.market_walker,art.market_walker_source,art.market_walker_fallback_reason);
    view.set_service_walker_visuals(art.service_walker,art.service_walker_source,art.service_walker_fallback_reason);
    view.set_health_walker_visuals(art.health_walker,art.health_walker_source,art.health_walker_fallback_reason);
    view.set_original_house_visuals(art.original_house,art.original_house_source,art.original_house_fallback_reason);
    view.set_original_building_visuals(art.original_building,art.original_building_source,art.original_building_fallback_reason);
}
std::string joined(const std::vector<std::string>& lines) {
    std::string result;for(const auto& line:lines){result+=line;result+=' ';}
    std::istringstream in(result);std::string word;result.clear();
    while(in>>word){if(!result.empty())result+=' ';result+=word;}return result;
}
Json detail_tail(oe::SandboxView& view,SDL_Renderer* renderer,const Field& field,unsigned stage) {
    const auto panel=view.inspector_layout();check(panel.content.h>0,"Generic field details panel is closed");
    const auto p=window_point(renderer,{panel.content.x+panel.content.w*.5,panel.content.y+panel.content.h*.5});
    SDL_Event scroll{};scroll.type=SDL_EVENT_MOUSE_WHEEL;scroll.wheel.mouse_x=static_cast<float>(p.x);scroll.wheel.mouse_y=static_cast<float>(p.y);
    scroll.wheel.y=-1000;event(view,scroll);view.update(0);pure_frame(view);
    const auto lines=view.inspection_lines();const auto text=joined(lines);
    check(text.find("Field visual stage "+std::to_string(stage+1)+"/6 (authored)")!=std::string::npos&&
        text.find("Farm #"+std::to_string(number(field.farm))+" progress "+std::to_string(view.world().building(field.farm).progress)+"/80")!=std::string::npos&&
        text.find("Original crop images; authored growth and anchor")!=std::string::npos&&
        text.find("Canonical variation0; original calendar not reproduced")!=std::string::npos,
        "Wrapped productive Inspector omitted the actual Farm/stage or authored scope");
    std::size_t start=lines.size();
    for(std::size_t i=0;i<lines.size();++i)if(lines[i].find("Field visual stage ")!=std::string::npos){start=i;break;}
    check(start<lines.size(),"No wrapped stage row in actual Inspector");Json visible=Json::array();
    for(std::size_t i=start;i<lines.size();++i) {
        const auto rect=view.inspector_layout().detail_row_rect(i);
        check(rect.h>0,"Complete authored field-stage explanation is clipped in the selected Inspector");
        visible.push_back({{"text",lines[i]},{"rect",{rect.x,rect.y,rect.w,rect.h}}});
    }
    return visible;
}
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==4,"Expected DATA REPO_ROOT PRIVATE_OE60_5_OUTPUT_PARENT");
        no_symlinks(argv[1]);no_symlinks(argv[2]);
        const auto data=fs::canonical(argv[1]),repo=fs::canonical(argv[2]),output=fresh_output(repo,argv[3]);
        check(!contains(data,output)&&!contains(output,data),"Evidence output overlaps the supplied original input");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software")&&SDL_Init(SDL_INIT_VIDEO),
            "Initialize scripted SDL dummy/software");
        check(SDL_CreateWindowAndRenderer("Paid Rule18 field growth",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create real Sandbox renderer");
        check(std::string_view(SDL_GetRendererName(renderer))=="software","Unexpected render backend");
        const auto preflight=oe::maps::check_map_rules({data,"Cities/Xia.map",sim::RulesProfile::CityV16,18,3});
        check(preflight.status==oe::maps::MapRulesStatus::MapRulesChecked&&!preflight.input_sha256.empty(),"Strict source-bound Xia preflight failed");
        auto session=oe::maps::load_stored_map_session(data,"Cities/Xia.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,preflight.input_sha256,true);
        auto view=oe::SandboxView::create_original_deadline_food_dispatch(std::move(session),oe::original::Difficulty::Normal,true);
        // REPO_ROOT owns lowercase source metadata; installed application roots
        // instead use Resources/Compatibility. Do not rely on case folding.
        const auto compatibility=assets::detect_compatibility(data,repo/"resources/compatibility");
        const auto art=oe::select_visual_profiles(compatibility);
        check(!art.original_building.empty(),"Actual data/profile fingerprint does not admit field graphics");
        configure(*view,art);const auto working=output/"working.save.json";
        view->configure_save(data,"Cities/Xia.map",working);view->initialize(window,renderer);
        check(view->paused()&&view->world().ticks()==0&&view->world().rule_version()==18,"Real paid Rule18 starter did not begin paused at zero");
        Json commands=Json::array();
        const auto execute=[&](sim::Command command,const std::string& reason) {
            const auto funds=view->world().treasury();const auto spent=view->world().construction_spent_total();
            const auto result=view->execute(command);
            check(result.accepted&&result.changed,"Ordinary paid command refused: "+reason+": "+result.reason);
            check(view->world().treasury()==funds-static_cast<std::int64_t>(view->world().construction_spent_total()-spent),
                "Paused command granted funds or failed to debit paid construction");
            commands.push_back({{"purpose",reason},{"tick",result.tick},{"sequence",result.sequence},
                {"type",static_cast<unsigned>(command.type)},{"cell",{command.cell.x,command.cell.y}},
                {"building",number(command.building_id)},{"enabled",command.operating_enabled},
                {"funds_before",funds},{"funds_after",view->world().treasury()}});
        };
        const auto& starter=view->world().buildings();const auto food=std::find_if(starter.begin(),starter.end(),
            [](const auto& b){return b.kind==sim::Object::OriginalFoodFarm;});
        check(food!=starter.end(),"Normal starter lacks its paid Food Farm");const sim::Cell origin{food->cell.x,food->cell.y-10};
        const sim::Cell wheat_cell{origin.x+10,origin.y+10};
        execute({sim::CommandType::PlaceOriginalFoodFarm,wheat_cell},"Buy second Food Farm for Wheat");
        const auto wheat_farm=view->world().building_owner_at(wheat_cell);check(wheat_farm.has_value(),"Paid Wheat Farm identity absent");
        sim::Command wheat{sim::CommandType::PlaceOriginalWheatField,{origin.x+13,origin.y+10}};wheat.building_id=*wheat_farm;
        execute(wheat,"Buy separately paid Wheat Field bound to its actual Farm");
        constexpr std::array<sim::Cell,12> plots{{{12,2},{15,2},{18,2},{21,2},{0,7},{3,7},{6,7},{9,7},{12,7},{15,7},{18,7},{21,7}}};
        for(const auto p:plots)execute({sim::CommandType::PlaceHousehold,{origin.x+p.x,origin.y+p.y}},"Buy ordinary additional worker House");
        // Commands publish a replacement World; retain typed IDs rather than
        // a borrowed buildings() span across any such publication.
        std::vector<sim::BuildingId> pause_ids;
        for(const auto& b:view->world().buildings()) {
            const bool pause=b.kind==sim::Object::ClaySource||b.kind==sim::Object::Pottery||b.kind==sim::Object::ServicePost||
                b.kind==sim::Object::HealthPost||b.kind==sim::Object::OriginalMusicSchool211||
                b.kind==sim::Object::OriginalAncestorShrine214||b.kind==sim::Object::OriginalWaterWell72;
            if(pause)pause_ids.push_back(b.id);
        }
        for(const auto id:pause_ids)
            execute(sim::set_building_operation(id,false),"Pause non-Farm work through the normal command");
        check(view->world().buildings().size()==33&&view->world().couriers().size()==16&&view->world().treasury()==9581,
            "Ordinary setup changed the existing paid33-body/16-channel/9581-Funds baseline");
        std::array fields{Field{sim::Object::OriginalHempField,194,2458},Field{sim::Object::OriginalWheatField,195,2422},
            Field{sim::Object::OriginalMilletField,196,2410}};
        for(auto& field:fields) {
            const auto& bodies=view->world().buildings();const auto body=std::find_if(bodies.begin(),bodies.end(),[&](const auto& b){return b.kind==field.kind;});
            check(body!=bodies.end(),"Ordinary paid city is missing one of the three Field categories");field.id=body->id;
            const auto& production=*view->world().original_production_state();
            for(const auto& farm:production.hemp_farms)if(farm.field_id==field.id)field.farm=farm.farm_id;
            for(const auto& farm:production.food.farms)if(farm.field_id==field.id)field.farm=farm.farm_id;
            check(number(field.farm)>0,"Actual saved Field has no paid parent Farm");
            for(unsigned stage=0;stage<6;++stage) {
                field.images[stage]=assets::load_sg3_image({data/"DATA/China_General.sg3",field.base+stage});
                check(field.images[stage].width==78&&field.images[stage].height>=40,"Canonical source image lost its one-cell geometry");
            }
        }
        auto merged=assets::load_building_visual_profile(data,art.building);
        assets::append_original_house_visual_profile(data,art.original_house,merged);
        assets::append_original_building_visual_profile(data,art.original_building,merged);
        std::uint64_t rgba_bytes=0;for(const auto& image:merged.unique_images)rgba_bytes+=image.pixels.size();
        check(merged.unique_images.size()==56&&view->building_texture_count()==56&&rgba_bytes==4770512,
            "Field publication changed the expected deduplicated56-image/4,770,512-byte pool");
        Json checkpoints=Json::array(),matrix=Json::array(),picking=Json::array(),observations=Json::array();
        const auto checkpoint=[&](const std::string& label) {
            const auto before=view->world().snapshot();key(*view,SDLK_F5);
            const auto saved=oe::persistence::read_save(working);const auto canonical=bytes(working);
            check(saved.source_schema_version==34&&saved.world==before,"Actual F5 lost full Rule18 authority");
            write(output/(label+".save.json"),canonical);key(*view,SDLK_F9);key(*view,SDLK_F5);
            check(view->world().snapshot()==before&&bytes(working)==canonical&&view->paused(),"Real F9/F5 changed complete saved bytes/World");
            write(output/(label+".roundtrip.json"),bytes(working));Json roles=Json::array();
            for(const auto& field:fields)roles.push_back(diagnostic(*view,field));
            checkpoints.push_back({{"label",label},{"tick",view->world().ticks()},{"full_F5_F9_bytes_equal",true},{"fields",roles}});
            return canonical;
        };
        const auto stage_matrix=[&](Field& field,unsigned stage) {
            const auto label="model"+std::to_string(field.model)+"-stage"+std::to_string(stage);
            const auto canonical=checkpoint(label);const auto authority=view->world().snapshot();
            for(const auto size:std::array<std::array<int,2>,3>{{{1280,800},{1600,900},{1920,1080}}}) {
                check(SDL_SetWindowSize(window,size[0],size[1]),"Resize real field window");
                SDL_Event resized{};resized.type=SDL_EVENT_WINDOW_RESIZED;resized.window.data1=size[0];resized.window.data2=size[1];event(*view,resized);view->update(0);
                for(const int nominal:{1,2,4}) {
                    zoom(*view,renderer,nominal);const auto d=view->original_building_visual_diagnostic(field.id);
                    center(*view,{d.image_origin.x+d.image_width*.5,d.image_origin.y+d.image_height*.5});view->set_tool(5);pure_frame(*view);
                    const auto before=view->original_building_display_stats();Json frame_counts;
                    auto frame=capture(*view,renderer,{},frame_counts);const auto after=view->original_building_display_stats();
                    const auto slot=static_cast<std::size_t>(d.role);
                    check(after.drawn_instances[slot]>before.drawn_instances[slot]&&after.authored_fallbacks[slot]==before.authored_fallbacks[slot],
                        "Actual Field stage submitted fallback instead of the selected original image");
                    const auto& image=field.images[stage];std::optional<Point> opaque;std::array<int,2> source_point{};Color opaque_color{};
                    const auto c=view->camera();
                    for(int y=1;y<image.height-1&&!opaque;++y)for(int x=1;x<image.width-1&&!opaque;++x) {
                        const auto expected=source(image,x,y);if(expected[3]!=255)continue;
                        const auto p=c.world_to_screen({d.image_origin.x+x+.5,d.image_origin.y+y+.5});
                        if(view->layout().map.contains(p.x,p.y)&&pixel(*frame,p)==expected) {
                            // A matching neighboring sprite color is insufficient:
                            // the real opaque-pointer path must select this Field.
                            click(*view,renderer,p);
                            if(view->selected_building()==field.id) {opaque=p;source_point={x,y};opaque_color=expected;}
                        }
                    }
                    check(opaque.has_value(),"No exact independently decoded visible native pixel for "+label);
                    Json alpha=nullptr,visible=nullptr;std::string png;
                    if(size[0]==1280&&nominal==4) {
                        click(*view,renderer,*opaque);check(view->selected_building()==field.id,"Actual opaque alpha pick did not select its paid Field");
                        std::optional<Point> hole;
                        for(int y=1;y<image.height-1&&!hole;++y)for(int x=1;x<image.width-1&&!hole;++x) {
                            if(source(image,x,y)[3]!=0)continue;
                            const auto p=c.world_to_screen({d.image_origin.x+x+.5,d.image_origin.y+y+.5});
                            if(!view->layout().map.contains(p.x,p.y))continue;
                            const auto ground=view->pick(p);if(ground&&view->world().building_owner_at(*ground)==field.id)continue;
                            click(*view,renderer,p);if(view->selected_building()!=field.id)hole=p;
                        }
                        check(hole.has_value(),"No transparent alpha miss outside the authoritative Field footprint");
                        click(*view,renderer,*opaque);check(view->selected_building()==field.id,"Opaque alpha pick did not remain stable");
                        key(*view,SDLK_F4);check(!view->original_building_visual_diagnostic(field.id).active,"F4 did not disable field sprites");
                        Json fallback_counts;auto fallback=capture(*view,renderer,{},fallback_counts);
                        check(pixel(*fallback,*opaque)!=opaque_color,"F4 did not change the exact native source pixel");
                        key(*view,SDLK_F4);check(view->original_building_visual_diagnostic(field.id).active,"F4 did not restore the same actual Field stage");
                        alpha={{"model",field.model},{"stage",stage},{"opaque",{opaque->x,opaque->y}},
                            {"source_pixel",source_point},{"source_rgba",opaque_color},{"transparent",{hole->x,hole->y}},
                            {"F4_native_pixel_changed",true}};picking.push_back(alpha);
                        visible=detail_tail(*view,renderer,field,stage);png=label+"-1280x800-4x.png";
                        Json final_counts;frame=capture(*view,renderer,output/png,final_counts);frame_counts=final_counts;
                    }
                    const auto pure=view->world().snapshot();const auto stable_resources=resources(*view),stable_camera=camera(*view);
                    perf::set_enabled(true);perf::reset();
                    for(unsigned n=0;n<3;++n){move(*view,renderer,*opaque);check(view->render(),"Pure hover/frame failed");}
                    perf::set_enabled(false);const auto pure_counts=counters();
                    check(view->world().snapshot()==pure&&camera(*view)==stable_camera&&resources(*view)==stable_resources&&
                        view->world().snapshot()==authority,"Pure frames changed complete World/camera/cache/texture ownership");
                    matrix.push_back({{"model",field.model},{"stage",stage},{"tick",view->world().ticks()},{"window",size},
                        {"nominal_zoom",nominal},{"camera",camera(*view)},{"visual",diagnostic(*view,field)},
                        {"source_pixel",source_point},{"source_rgba",opaque_color},{"screen_pixel_center",{opaque->x,opaque->y}},
                        {"owned_RGBA32_before_Present",true},{"render_counters",frame_counts},{"three_hover_frame_counters",pure_counts},
                        {"file",png.empty()?Json(nullptr):Json(png)},{"visible_authored_stage_details",visible},{"alpha_picking",alpha},
                        {"complete_World_camera_resources_unchanged",true}});
                }
            }
            key(*view,SDLK_F5);check(view->world().snapshot()==authority&&bytes(working)==canonical,"Stage matrix changed complete saved bytes");
        };
        bool pause_done=false,resume_done=false;std::optional<std::uint64_t> resume_tick;int frozen_progress=-1;
        while(true) {
            Json current=Json::array();
            for(auto& field:fields) {
                const auto progress=view->world().building(field.farm).progress;const auto stage=stage_of(progress);
                field.progress_values[static_cast<std::size_t>(progress)]=true;
                if(progress>0&&!field.started)field.started=view->world().ticks();
                if(harvested(view->world(),field)>0&&!field.first_harvest)field.first_harvest=view->world().ticks();
                current.push_back(diagnostic(*view,field));
                if(!field.stages[stage]){stage_matrix(field,stage);field.stages[stage]=true;}
            }
            observations.push_back({{"tick",view->world().ticks()},{"fields",current}});
            auto& wheat_field=fields[1];const auto t=view->world().ticks();
            if(!pause_done&&view->world().building(wheat_field.farm).progress==40) {
                frozen_progress=40;execute(sim::set_building_operation(wheat_field.farm,false),"Pause actual Wheat Farm at its reached stage3");
                checkpoint("wheat-stage3-paused");pause_done=true;resume_tick=t+10;
            }
            if(pause_done&&!resume_done) {
                check(view->world().building(wheat_field.farm).progress==frozen_progress&&
                    view->original_building_visual_diagnostic(wheat_field.id).field_stage==3,"Paused Farm changed its actual field stage");
                if(t==*resume_tick) {
                    checkpoint("wheat-stage3-ten-real-ticks-paused");
                    execute(sim::set_building_operation(wheat_field.farm,true),"Resume the same actual Wheat Farm after ten real ticks");
                    checkpoint("wheat-stage3-resumed");resume_done=true;
                }
            }
            const bool all_done=std::all_of(fields.begin(),fields.end(),[](const auto& field) {
                return std::all_of(field.stages.begin(),field.stages.end(),[](bool value){return value;})&&field.first_harvest.has_value();});
            if(all_done&&resume_done)break;
            check(t<400,"Actual paid workforce/crop clock did not reach every stage and harvest within400 ticks");
            key(*view,SDLK_PERIOD);check(view->paused()&&view->world().ticks()==t+1,"Real Step did not advance exactly one tick");
        }
        check(matrix.size()==162&&picking.size()==18,"The real3-fields/six-stages/nine-aperture matrix is incomplete");
        Json summary=Json::array();
        for(const auto& field:fields) {
            check(std::all_of(field.progress_values.begin(),field.progress_values.end(),[](bool value){return value;}),
                "A Field did not actually observe each of the80 parent productive-clock values");
            summary.push_back({{"model",field.model},{"field",number(field.id)},{"farm",number(field.farm)},
                {"first_positive_progress_tick",*field.started},{"first_actual_harvest_tick",*field.first_harvest},
                {"all80_actual_parent_progress_values_observed",true},{"all6_actual_field_stages_observed",true}});
        }
        checkpoint("complete-paid-cycle");const auto end_tick=view->world().ticks();view->shutdown();view.reset();
        check(oe::BuildingSprite::live_texture_count()==0&&oe::WalkerSpriteSet::live_texture_count()==0,"Owned textures leaked after field view shutdown");
        write(output/"report.json",Json{{"status","AUTOMATED_PASS"},{"runtime","SCRIPTED_SDL dummy/software"},
            {"rule",18},{"schema",34},{"map","Cities/Xia.map"},{"difficulty","Normal"},{"end_tick",end_tick},
            {"commands",commands},{"fields",summary},{"observations",observations},{"checkpoints",checkpoints},
            {"readback_matrix_cases",matrix.size()},{"representative_pngs",18},{"matrix",matrix},{"alpha_picking",picking},
            {"building_textures",56},{"combined_RGBA_bytes",rgba_bytes},{"complete_F5_F9_bytes_equal",true},
            {"pause_resume_actual_parent_ticks",10},{"stage_mapping","AUTHORED existing80-productive-tick Farm progress -> six canonical Original images"},
            {"original_quantity_terrain_calendar_reproduction","NOT_CLAIMED"},{"capture_review","NOT_RUN"},
            {"native_desktop","NOT_RUN"},{"human","NOT_RUN"}}.dump(2)+"\n");
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<"Paid Rule18 field growth View PASS; report "<<(output/"report.json")<<"; native/human NOT_RUN\n";return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);std::cerr<<error.what()<<'\n';if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
