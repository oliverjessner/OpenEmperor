// Original pixels on an explicitly supplied, earned Rule18 city. No private
// maps, saves, images or model fixtures belong in this source or its resources.
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "app/SandboxVisualOrder.h"
#include "app/WalkerPose.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace assets=oe::assets;
namespace perf=oe::performance;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Point=oe::scene::Point;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
using Color=std::array<std::uint8_t,4>;
void check(bool value,const std::string& why) {if(!value)throw std::runtime_error(why);}
std::string bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);check(bool(input),"Cannot read explicit fixture: "+path.string());
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void write(const fs::path& path,const std::string& value) {
    check(!fs::exists(path),"Preserve existing output: "+path.string());
    std::ofstream output(path,std::ios::binary);output.write(value.data(),static_cast<std::streamsize>(value.size()));
    check(bool(output),"Cannot write private fixture output");
}
bool contains(const fs::path& root,const fs::path& path) {
    auto a=root.begin(),b=path.begin();
    for(;a!=root.end();++a,++b)if(b==path.end()||*a!=*b)return false;
    return true;
}
void no_symlinks(const fs::path& path) {
    fs::path current;
    for(const auto& part:fs::absolute(path).lexically_normal()) {
        current/=part;std::error_code error;const auto status=fs::symlink_status(current,error);
        check(!error||error==std::errc::no_such_file_or_directory,"Cannot inspect output ancestry");
        check(error||!fs::is_symlink(status),"Fixture paths may not traverse symbolic links");
    }
}
fs::path fresh_output(const fs::path& repo,const fs::path& requested) {
    const auto parent=fs::absolute(requested).lexically_normal();no_symlinks(parent);
    const auto old_allowed=repo/".local/oe52",oe60_allowed=repo/".local/oe60",allowed=repo/".local/oe60-5";
    const auto resolved=fs::weakly_canonical(parent);
    check((parent!=allowed&&contains(allowed,resolved)) ||
        (parent!=oe60_allowed&&contains(oe60_allowed,resolved)) ||
        (parent!=old_allowed&&contains(old_allowed,resolved)),"Use a private OE52/OE60/OE60.5 output parent");
    fs::create_directories(parent);
    const auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for(unsigned attempt=0;attempt<32;++attempt) {
        const auto child=parent/("view-"+stamp+"-"+std::to_string(attempt));std::error_code error;
        if(fs::create_directory(child,error))return child;
        check(!error||error==std::errc::file_exists,"Cannot reserve a unique private output directory");
    }
    throw std::runtime_error("Could not reserve independent visual evidence output");
}
void event(oe::SandboxView& view,SDL_Event e) {
    check(SDL_PushEvent(&e),"Queue scripted SDL input");bool running=true;
    while(SDL_PollEvent(&e))view.handle_event(e,running);
    check(running,"Visual inspection input closed the actual view");
}
void key(oe::SandboxView& view,SDL_Keycode code) {SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);}
void focus(oe::SandboxView& view) {SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,e);}
Point window_point(SDL_Renderer* renderer,Point point) {
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y),
        "Invert production input transform");return {x,y};
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    check(view.layout().map.contains(point.x,point.y),"Alpha witness is outside map aperture");
    const auto raw=window_point(renderer,point);SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;
    e.motion.x=static_cast<float>(raw.x);e.motion.y=static_cast<float>(raw.y);event(view,e);view.update(0);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(raw.x);e.button.y=static_cast<float>(raw.y);event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);view.update(0);
}
void center(oe::SandboxView& view,Point world) {
    focus(view);const auto map=view.layout().map;const auto z=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-world.x*z,map.y+map.h*.5-world.y*z};
    for(unsigned step=0;step<1000;++step) {
        const auto current=view.camera().offset;const auto dx=wanted.x-current.x,dy=wanted.y-current.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Ordinary paused camera input did not converge");
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;const auto p=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    for(unsigned attempt=0;attempt<3&&std::abs(view.camera().zoom-wanted)>1e-9;++attempt) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);e.wheel.mouse_y=static_cast<float>(p.y);
        e.wheel.y=static_cast<float>(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0);
    }
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Actual wheel missed literal 1/2/4 zoom");
}
Json diagnostic(const oe::SandboxView::OriginalBuildingVisualDiagnostic& d) {
    Json result{{"applicable",d.applicable},{"configured",d.configured},{"active",d.active},
        {"role",assets::original_building_role_name(d.role)},{"model_type",d.model_type},{"actual_side",d.actual_side},
        {"dimensions",{d.image_width,d.image_height}},{"anchor",{d.ground_x,d.ground_y}},
        {"world_ground",{d.ground.x,d.ground.y}},{"world_image_origin",{d.image_origin.x,d.image_origin.y}},
        {"selection",d.selection},{"evidence",d.evidence},{"fallback_reason",d.fallback_reason}};
    if(d.asset)result["asset"]={{"archive",d.asset->archive_relative_path.generic_string()},{"physical_record",d.asset->image_index}};
    if(d.field_stage)result["authored_field_stage"]=*d.field_stage;
    if(d.field_farm)result["field_farm_id"]=static_cast<unsigned>(*d.field_farm);
    if(d.field_stage)result["field_farm_progress"]=d.field_farm_progress;
    return result;
}
Surface capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& out,const std::string& name,Json& records) {
    check(SDL_GetRenderTarget(renderer)==nullptr,"Capture must begin at ordinary window target");
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height)&&width>0&&height>0,"Read actual output size");
    const auto before=view.world().snapshot();const auto camera=view.camera();
    const auto original=view.original_building_display_stats();const auto houses=view.original_house_display_stats();
    const auto core=view.building_display_stats();const auto walkers=view.walker_display_stats();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Create owned RGBA32 capture target");
    struct Restore {SDL_Renderer* renderer;~Restore(){SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    check(view.render()&&SDL_GetRenderTarget(renderer)==target.get(),"Render changed owned readback target");
    Surface raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(raw),"Read production pixels before Present");
    Surface rgba(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    check(rgba&&rgba->w==width&&rgba->h==height,"Captured dimensions/format differ from output");
    if(!name.empty())check(!fs::exists(out/(name+".png"))&&SDL_SavePNG(rgba.get(),(out/(name+".png")).string().c_str()),"Write fresh private PNG");
    const auto after=view.camera();check(view.world().snapshot()==before&&after.zoom==camera.zoom&&after.offset.x==camera.offset.x&&
        after.offset.y==camera.offset.y&&after.viewport_width==camera.viewport_width&&after.viewport_height==camera.viewport_height,
        "Readback changed complete World or camera/viewport");
    const auto displayed=view.original_building_display_stats();const auto drawn_houses=view.original_house_display_stats();
    const auto drawn_core=view.building_display_stats();const auto drawn_walkers=view.walker_display_stats();
    Json delta=Json::array(),house_delta=Json::array(),core_delta=Json::array(),walker_delta=Json::array();
    for(std::size_t i=0;i<assets::original_building_role_count;++i)delta.push_back({
        {"role",assets::original_building_role_name(static_cast<assets::OriginalBuildingVisualRole>(i))},
        {"original_draws",displayed.drawn_instances[i]-original.drawn_instances[i]},
        {"fallback_draws",displayed.authored_fallbacks[i]-original.authored_fallbacks[i]}});
    for(std::size_t i=0;i<houses.drawn_instances.size();++i)house_delta.push_back({{"type",i+2},
        {"original_draws",drawn_houses.drawn_instances[i]-houses.drawn_instances[i]},
        {"fallback_draws",drawn_houses.authored_fallbacks[i]-houses.authored_fallbacks[i]}});
    for(std::size_t i=0;i<assets::building_role_count;++i)core_delta.push_back({{"role",assets::building_role_name(static_cast<assets::BuildingVisualRole>(i))},
        {"draws",drawn_core.drawn_instances[i]-core.drawn_instances[i]},
        {"fallbacks",drawn_core.placeholder_fallbacks[i]-core.placeholder_fallbacks[i]}});
    for(std::size_t i=0;i<assets::walker_visual_role_count;++i)walker_delta.push_back({{"family",assets::walker_role_name(static_cast<assets::WalkerVisualRole>(i))},
        {"draws",drawn_walkers.roles[i].draws-walkers.roles[i].draws}});
    records.push_back({{"file",name.empty()?Json(nullptr):Json(name+".png")},{"output_size",{width,height}},{"zoom",camera.zoom},
        {"tick",view.world().ticks()},{"original_building_deltas",delta},{"house_deltas",house_delta},{"legacy_core_deltas",core_delta},
        {"walker_deltas",walker_delta},{"owned_rgba32_target",true},{"readback_before_present",true},{"world_camera_unchanged",true},
        {"visually_reviewed",false},{"native","NOT_RUN"},{"human","NOT_RUN"}});
    return rgba;
}
Color pixel(SDL_Surface* image,int x,int y) {
    check(x>=0&&y>=0&&x<image->w&&y<image->h,"Pixel witness outside output");
    const auto* p=static_cast<const std::uint8_t*>(image->pixels)+y*image->pitch+x*4;return {p[0],p[1],p[2],p[3]};
}
Color native(const assets::RgbaImage& image,int x,int y) {
    const auto at=(static_cast<std::size_t>(y)*image.width+static_cast<std::size_t>(x))*4;
    return {image.pixels.at(at),image.pixels.at(at+1),image.pixels.at(at+2),image.pixels.at(at+3)};
}
void selection_contract() {
    assets::CompatibilityResult supported;supported.status=assets::CompatibilityStatus::Compatible;
    supported.profile=assets::CompatibilityProfile{};supported.profile->building_profile="legacy-building.json";
    supported.profile->original_house_profile="original-houses.json";supported.original_house_status=assets::CompatibilityStatus::Compatible;
    supported.profile->original_building_profile="original-buildings.json";supported.original_building_status=assets::CompatibilityStatus::Compatible;
    const auto automatic=oe::select_visual_profiles(supported);
    check(automatic.original_building==supported.profile->original_building_profile&&automatic.original_building_source==oe::VisualProfileSource::Builtin,
        "Compatible optional building metadata was not selected");
    const auto custom=oe::select_visual_profiles(supported,{},"custom-building.json");
    check(custom.building=="custom-building.json"&&custom.original_building.empty()&&custom.original_house.empty()&&
        custom.original_building_source==oe::VisualProfileSource::Custom&&!custom.original_building_fallback_reason.empty(),
        "Custom core did not suppress both built-in building supplements");
    supported.original_building_status=assets::CompatibilityStatus::FingerprintMismatch;supported.original_building_detail="Different General bytes";
    const auto mismatch=oe::select_visual_profiles(supported);
    check(mismatch.original_building.empty()&&mismatch.original_building_fallback_reason==supported.original_building_detail&&
        mismatch.building==automatic.building&&mismatch.original_house==automatic.original_house,
        "Optional fingerprint failure changed independent core/House selection");
    supported.status=assets::CompatibilityStatus::MissingFile;supported.detail="Required data file unavailable";
    const auto missing=oe::select_visual_profiles(supported);
    check(missing.original_building.empty()&&!missing.original_building_fallback_reason.empty(),"Missing core admitted optional Original building art");
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
struct Expected {sim::Object kind;std::uint16_t model;std::uint32_t record;};
constexpr std::array expected{
    Expected{sim::Object::OriginalWaterWell72,72,1559},Expected{sim::Object::ServicePost,208,1593},
    Expected{sim::Object::OriginalMusicSchool211,211,589},Expected{sim::Object::OriginalAcrobatSchool212,212,460},
    Expected{sim::Object::OriginalDramaSchool213,213,501},Expected{sim::Object::OriginalDaoistShrine215,215,2245},
    Expected{sim::Object::OriginalBuddhistShrine217,217,2258},Expected{sim::Object::OriginalConfucianAcademy219,219,2309},
    Expected{sim::Object::OriginalPaperMaker45,45,2811},Expected{sim::Object::OriginalWeaver47,47,2768},
    Expected{sim::Object::OriginalHempFarm,192,825},Expected{sim::Object::OriginalFoodFarm,193,793}};
constexpr std::array fields{
    // This immutable earned city bought Millet only. The independent Rule18
    // farm lifecycle check buys Wheat normally; never rewrite this fixture.
    std::pair{sim::Object::OriginalHempField,194},
    std::pair{sim::Object::OriginalMilletField,196}};
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        selection_contract();
        if(argc==2&&std::string_view(argv[1])=="--selection-only") {
            std::cout<<"Original building selection/custom/fingerprint guards PASS; original-data View NOT_RUN\n";return 0;
        }
        check(argc==5,"Expected DATA REPO_ROOT PAID_RULE18_SAVE PRIVATE_OUTPUT_PARENT, or --selection-only");
        for(int i=1;i<5;++i)no_symlinks(argv[i]);
        const auto data=fs::canonical(argv[1]),repo=fs::canonical(argv[2]),input=fs::canonical(argv[3]);
        const auto original_bytes=bytes(input);const auto document=oe::persistence::read_save(input);
        check(document.world.profile==sim::RulesProfile::CityV16&&document.world.rule_version==18&&document.source_schema_version==34&&
            document.map_permissions_policy_version==3&&document.map_relative==fs::path("Cities/Xia.map"),"Fixture must be earned Rule18/schema34/policy3 Xia");
        check(document.world.buildings.size()==66&&document.world.couriers.size()==46,"Use the complete paid66-building/46-courier reference city");
        const auto output=fresh_output(repo,argv[4]);check(!contains(data,output)&&!contains(output,data)&&!contains(output,input),"Outputs overlap source inputs");
        const auto compatibility=assets::detect_compatibility(data,repo/"resources/compatibility");
        const auto art=oe::select_visual_profiles(compatibility);
        check(compatibility.original_building_compatible()&&!art.original_building.empty(),"Explicit data does not qualify for the original building supplement");
        const auto working=output/"working.save.json";write(working,original_bytes);
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software")&&SDL_Init(SDL_INIT_VIDEO),"Initialize scripted dummy/software SDL");
        check(SDL_CreateWindowAndRenderer("Original building paid View",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create real production renderer");
        check(std::string_view(SDL_GetRendererName(renderer))=="software","Unexpected renderer backend");
        const auto session=[&]{return oe::maps::load_stored_map_session(data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);};
        auto view=oe::SandboxView::create_original_deadline_food_dispatch(session(),std::nullopt,false);
        view->configure_save(data,document.map_relative,working,document);configure(*view,art);view->initialize(window,renderer);focus(*view);
        check(view->paused()&&view->world().snapshot()==document.world,"Production restore changed complete paid World");
        key(*view,SDLK_F5);check(bytes(working)==original_bytes,"Initial actual F5 changed complete canonical save bytes");
        const auto texture_count=view->building_texture_count();check(texture_count==56,"Startup must upload exactly56 shared core/House/Original building textures");
        const auto initial_stats=view->original_building_display_stats();
        check(initial_stats.active&&std::all_of(initial_stats.configured_roles.begin(),initial_stats.configured_roles.end(),[](bool value){return value;}),
            "Twelve native bodies and three complete authored field image tables were not atomically prepared");
        Json records=Json::array(),matrix=Json::array(),alpha=Json::array(),bodies=Json::array(),actors=Json::array();
        std::array<sim::BuildingId,expected.size()> owners{};std::array<assets::RgbaImage,expected.size()> images;
        std::array<sim::BuildingId,fields.size()> field_owners{};
        std::array<assets::RgbaImage,fields.size()> field_images;
        for(std::size_t i=0;i<expected.size();++i) {
            const auto found=std::find_if(view->world().buildings().begin(),view->world().buildings().end(),[&](const auto& b){return b.kind==expected[i].kind;});
            check(found!=view->world().buildings().end(),"Paid reference lacks a selected Original building category");owners[i]=found->id;
            const auto d=view->original_building_visual_diagnostic(found->id);
            check(d.applicable&&d.active&&d.asset&&d.model_type==expected[i].model&&d.asset->image_index==expected[i].record,
                "Actual paid kind selected a different original model/record");
            images[i]=assets::load_sg3_image({data/d.asset->archive_relative_path,d.asset->image_index});
            check(d.actual_side==sim::building_footprint(view->world().profile(),view->world().rule_version(),found->kind).width&&
                d.image_width==images[i].width&&d.image_height==images[i].height&&d.ground_x==40*d.actual_side-1&&d.ground_y==d.image_height-20,
                "Native source dimensions/footprint/authored anchor differ from prepared diagnostic");
        }
        for(std::size_t i=0;i<fields.size();++i) {
            const auto found=std::find_if(view->world().buildings().begin(),view->world().buildings().end(),
                [&](const auto& b){return b.kind==fields[i].first;});
            check(found!=view->world().buildings().end(),"Paid reference lacks a crop field category");
            field_owners[i]=found->id;
            const auto d=view->original_building_visual_diagnostic(found->id);
            const auto base=fields[i].second==194 ? 2458U:2410U;
            check(d.applicable&&d.model_type==fields[i].second&&d.actual_side==1&&d.configured&&d.active&&d.asset&&
                d.field_stage&&d.field_farm&&*d.field_stage<6&&d.asset->image_index==base+*d.field_stage&&
                d.selection.find("authored six-stage Farm progress mapping")!=std::string::npos&&
                view->world().building(*d.field_farm).progress==d.field_farm_progress,
                "Paid field lost its explicit authored progress mapping or canonical native frame");
            field_images[i]=assets::load_sg3_image({data/d.asset->archive_relative_path,d.asset->image_index});
            check(d.image_width==field_images[i].width&&d.image_height==field_images[i].height&&
                d.ground_x==39&&d.ground_y==d.image_height-20,"Field image/one-cell anchor mismatch");
        }
        for(const auto dimensions:std::array{std::pair{1280,800},std::pair{1600,900},std::pair{1920,1080}}) {
            check(SDL_SetWindowSize(window,dimensions.first,dimensions.second),"Resize actual production View");
            SDL_Event resized{};resized.type=SDL_EVENT_WINDOW_RESIZED;resized.window.data1=dimensions.first;resized.window.data2=dimensions.second;event(*view,resized);focus(*view);
            for(const double z:{1.,2.,4.}) {
                zoom(*view,renderer,z);Point city_center{};
                for(const auto id:owners){const auto ground=view->original_building_visual_diagnostic(id).ground;
                    city_center.x+=ground.x/static_cast<double>(expected.size());city_center.y+=ground.y/static_cast<double>(expected.size());}
                center(*view,city_center);
                capture(*view,renderer,output,"overview-"+std::to_string(dimensions.first)+"x"+std::to_string(dimensions.second)+"-"+
                    std::to_string(static_cast<int>(z))+"x",records);
                for(std::size_t i=0;i<expected.size();++i) {
                const auto d=view->original_building_visual_diagnostic(owners[i]);
                center(*view,{d.image_origin.x+d.image_width*.5,d.image_origin.y+d.image_height*.5});
                const auto name="model"+std::to_string(expected[i].model)+"-"+std::to_string(dimensions.first)+"x"+std::to_string(dimensions.second)+"-"+std::to_string(static_cast<int>(z))+"x";
                const bool save_category=dimensions.first==1920&&z==1;
                const auto before=view->original_building_display_stats();auto frame=capture(*view,renderer,output,save_category?name:std::string{},records);
                const auto after=view->original_building_display_stats();const auto slot=static_cast<std::size_t>(d.role);
                check(after.drawn_instances[slot]>before.drawn_instances[slot]&&after.authored_fallbacks[slot]==before.authored_fallbacks[slot],"Mapped category submitted fallback instead of original art");
                matrix.push_back({{"building_id",static_cast<unsigned>(owners[i])},{"diagnostic",diagnostic(d)},{"size",{dimensions.first,dimensions.second}},{"zoom",z},
                    {"file",save_category?Json(name+".png"):Json(nullptr)}});
                if(dimensions.first==1920&&z==1) {
                    const auto origin=view->camera().world_to_screen(d.image_origin);view->set_tool(5);
                    std::optional<Point> opaque,hole;
                    for(int y=1;y<images[i].height-1&&(!opaque||!hole);y+=2)for(int x=1;x<images[i].width-1&&(!opaque||!hole);x+=2) {
                        const auto color=native(images[i],x,y);const Point p{origin.x+x+.5,origin.y+y+.5};
                        if(!view->layout().map.contains(p.x,p.y))continue;
                        if(!opaque&&color[3]==255&&pixel(frame.get(),static_cast<int>(p.x),static_cast<int>(p.y))==color) {
                            click(*view,renderer,p);if(view->selected_building()==owners[i]&&!view->input_diagnostic_state().selected_landscape)opaque=p;
                        }
                        if(!hole&&color[3]==0&&x>4&&y>4) {click(*view,renderer,p);if(view->selected_building()!=owners[i])hole=p;}
                    }
                    check(opaque&&hole,"No distinct visible opaque and transparent alpha-picking witnesses for model "+std::to_string(expected[i].model));
                    click(*view,renderer,*opaque);check(view->selected_building()==owners[i],"Opaque original selection did not remain stable");
                    alpha.push_back({{"model",expected[i].model},{"opaque",{opaque->x,opaque->y}},{"transparent",{hole->x,hole->y}},{"status","PASS"}});
                    key(*view,SDLK_F4);const auto off_before=view->original_building_display_stats();
                    auto fallback=capture(*view,renderer,output,name+"-f4-fallback",records);const auto off_after=view->original_building_display_stats();
                    check(!off_after.active&&off_after.authored_fallbacks[slot]>off_before.authored_fallbacks[slot]&&
                        off_after.drawn_instances[slot]==off_before.drawn_instances[slot],"F4 did not take the historical fallback path");
                    check(pixel(frame.get(),static_cast<int>(opaque->x),static_cast<int>(opaque->y))!=pixel(fallback.get(),static_cast<int>(opaque->x),static_cast<int>(opaque->y)),"F4 comparison did not change its proven original pixel");
                    key(*view,SDLK_F4);check(view->original_building_display_stats().active,"F4 did not restore configured art");
                }
                check(view->world().snapshot()==document.world&&view->paused()&&view->building_texture_count()==texture_count,"Inspection changed paid state or uploaded new textures");
                }
                for(std::size_t i=0;i<fields.size();++i) {
                    const auto d=view->original_building_visual_diagnostic(field_owners[i]);
                    center(*view,{d.image_origin.x+d.image_width*.5,d.image_origin.y+d.image_height*.5});
                    const auto name="model"+std::to_string(fields[i].second)+"-authored-field-growth-"+
                        std::to_string(dimensions.first)+"x"+std::to_string(dimensions.second)+"-"+
                        std::to_string(static_cast<int>(z))+"x";
                    const bool save_category=dimensions.first==1920&&z==1;
                    const auto before=view->original_building_display_stats();
                    auto frame=capture(*view,renderer,output,save_category?name:std::string{},records);
                    const auto after=view->original_building_display_stats();const auto slot=static_cast<std::size_t>(d.role);
                    check(d.configured&&d.active&&d.asset&&d.field_stage&&d.field_farm&&
                        after.drawn_instances[slot]>before.drawn_instances[slot]&&
                        after.authored_fallbacks[slot]==before.authored_fallbacks[slot],
                        "Paid authored field stage did not submit its prepared native frame");
                    matrix.push_back({{"building_id",static_cast<unsigned>(field_owners[i])},{"diagnostic",diagnostic(d)},
                        {"size",{dimensions.first,dimensions.second}},{"zoom",z},{"expected","ORIGINAL_FIELD_IMAGES_AUTHORED_PROGRESS"},
                        {"file",save_category?Json(name+".png"):Json(nullptr)}});
                    if(save_category) {
                        const auto origin=view->camera().world_to_screen(d.image_origin);view->set_tool(5);
                        std::optional<Point> opaque,hole;
                        for(int y=1;y<field_images[i].height-1&&(!opaque||!hole);++y)
                            for(int x=1;x<field_images[i].width-1&&(!opaque||!hole);++x) {
                                const auto color=native(field_images[i],x,y);const Point p{origin.x+x+.5,origin.y+y+.5};
                                if(!view->layout().map.contains(p.x,p.y))continue;
                                if(!opaque&&color[3]==255&&pixel(frame.get(),static_cast<int>(p.x),static_cast<int>(p.y))==color) {
                                    click(*view,renderer,p);if(view->selected_building()==field_owners[i])opaque=p;
                                }
                                if(!hole&&color[3]==0&&x>4&&y>4) {
                                    click(*view,renderer,p);if(view->selected_building()!=field_owners[i])hole=p;
                                }
                            }
                        check(opaque&&hole,"No distinct source-alpha picking witnesses for paid field");
                        click(*view,renderer,*opaque);
                        alpha.push_back({{"model",fields[i].second},{"stage",*d.field_stage},
                            {"opaque",{opaque->x,opaque->y}},{"transparent",{hole->x,hole->y}},{"status","PASS"}});
                        key(*view,SDLK_F4);const auto off_before=view->original_building_display_stats();
                        capture(*view,renderer,output,name+"-f4-fallback",records);
                        const auto off_after=view->original_building_display_stats();
                        check(!view->original_building_visual_diagnostic(field_owners[i]).active&&
                            off_after.authored_fallbacks[slot]>off_before.authored_fallbacks[slot]&&
                            off_after.drawn_instances[slot]==off_before.drawn_instances[slot],
                            "F4 field comparison did not retain the historical fallback");
                        key(*view,SDLK_F4);
                    }
                    check(view->world().snapshot()==document.world&&view->paused()&&view->building_texture_count()==texture_count,
                        "Field image rendering changed paid state or prepared resources");
                }
            }
        }
        for(const auto& b:view->world().buildings()) {
            const auto d=view->original_building_visual_diagnostic(b.id);const auto h=view->original_house_visual_diagnostic(b.id);
            Json row{{"id",static_cast<unsigned>(b.id)},{"kind",static_cast<unsigned>(b.kind)},{"origin",{b.cell.x,b.cell.y}},
                {"footprint",sim::building_footprint(view->world().profile(),view->world().rule_version(),b.kind).width}};
            if(d.applicable)row["new_original"]=diagnostic(d);
            else if(h.applicable) {row["house_type"]=h.house_type;row["original_house_active"]=h.active;row["fallback_reason"]=h.fallback_reason;
                if(h.asset)row["asset"]={{"archive",h.asset->archive_relative_path.generic_string()},{"physical_record",h.asset->image_index}};}
            else if(const auto role=oe::building_visual_role(b.kind)) {row["legacy_role"]=assets::building_role_name(*role);row["selection"]="existing curated core";}
            else {
                // This generic lookup is deliberately not a second production
                // selector. OriginalInfluenceSource, for example, is handled
                // contextually by SandboxView::visual_role().
                row["selection"]="No generic kind mapping; production-specific audit required";
                row["generic_lookup_only"]=true;
            }
            bodies.push_back(std::move(row));
        }
        check(view->render(),"Render final Walker diagnostic frame");
        for(const auto& d:view->walker_diagnostics()) {
            Json row{{"id",static_cast<unsigned>(d.id)},{"role",static_cast<unsigned>(d.role)},{"owner",static_cast<unsigned>(d.owner)},
                {"phase",static_cast<unsigned>(d.phase)},{"live_visible",d.live_visible},{"submitted",d.submitted},{"sprite_drawn",d.sprite_drawn},
                {"family",d.family},{"source",d.profile_source},{"clip",d.clip_id},{"reason",d.status},{"flip_x",d.flip_x},
                {"screen_ground",{d.screen_ground.x,d.screen_ground.y}},{"image_origin",{d.image_origin.x,d.image_origin.y}},{"image_size",{d.image_width,d.image_height}}};
            if(d.native_asset)row["asset"]={{"archive",d.native_asset->archive_relative_path.generic_string()},{"physical_record",d.native_asset->image_index}};
            actors.push_back(std::move(row));
        }
        check(bodies.size()==66&&actors.size()==46,"Complete paid body/actor diagnostic matrix is incomplete");
        const auto pure=view->world().snapshot();perf::set_enabled(true);perf::reset();
        for(int frame=0;frame<100;++frame){view->update(0);check(view->render(),"Render pure prepared frame");}
        Json counters=Json::object();for(std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i) {
            const auto value=perf::counter(static_cast<perf::Counter>(i));counters[std::string(perf::counter_names[i])]=value;
            check(value==0,"Pure frame performed authority, I/O, decode/upload or routing work");
        }
        perf::set_enabled(false);check(view->world().snapshot()==pure,"Pure frames mutated paid World");
        for(const auto& path:{output/"missing-profile.json",output/"malformed-profile.json"}) {
            if(path.filename()=="malformed-profile.json")write(path,"{\"schema_version\":999,\"unknown\":true}");
            const auto textures=view->building_texture_count();const auto old=view->original_building_display_stats();const auto houses=view->original_house_display_stats();
            view->set_original_building_visuals(path,oe::VisualProfileSource::Builtin);
            const auto failed=view->original_building_display_stats();
            check(failed.active&&failed.configured_roles==old.configured_roles&&!failed.preparation_error.empty()&&view->building_texture_count()==textures&&
                view->original_house_display_stats().configured_types==houses.configured_types&&view->world().snapshot()==pure,
                "Failed optional replacement discarded active core/House/building textures or paid state");
        }
        key(*view,SDLK_F5);check(bytes(working)==original_bytes,"Render/F4/failed preparation changed full F5 document");
        key(*view,SDLK_F9);key(*view,SDLK_F5);check(view->world().snapshot()==document.world&&bytes(working)==original_bytes&&bytes(input)==original_bytes,
            "Actual F5/F9 changed the complete independent paid save or protected source");
        view->shutdown();view.reset();
        auto first_failed=oe::SandboxView::create_original_deadline_food_dispatch(session(),std::nullopt,false);
        first_failed->configure_save(data,document.map_relative,output/"first-append-working.save.json",document);
        auto without_tail=art;without_tail.original_building.clear();without_tail.original_building_source=oe::VisualProfileSource::Fallback;
        configure(*first_failed,without_tail);first_failed->initialize(window,renderer);
        check(first_failed->building_texture_count()==28&&!first_failed->original_building_display_stats().configured&&
            first_failed->original_house_display_stats().configured,"No-tail control must retain12 core and16 House textures");
        const auto retained_houses=first_failed->original_house_display_stats().configured_types;
        for(const auto& path:{output/"missing-first-profile.json",output/"malformed-profile.json"}) {
            perf::set_enabled(true);perf::reset();first_failed->set_original_building_visuals(path,oe::VisualProfileSource::Builtin);
            const auto uploads=perf::counter(perf::Counter::TextureUploads);perf::set_enabled(false);
            check(uploads==0&&first_failed->building_texture_count()==28&&!first_failed->original_building_display_stats().configured&&
                first_failed->original_house_display_stats().configured_types==retained_houses&&first_failed->world().snapshot()==document.world,
                "Failed first optional append reuploaded/discarded retained core/House textures or changed World");
        }
        perf::set_enabled(true);perf::reset();first_failed->set_original_building_visuals(art.original_building,oe::VisualProfileSource::Builtin);
        const auto appended_uploads=perf::counter(perf::Counter::TextureUploads);perf::set_enabled(false);
        check(appended_uploads==28&&first_failed->building_texture_count()==56&&first_failed->original_building_display_stats().active&&
            first_failed->original_house_display_stats().configured_types==retained_houses&&first_failed->world().snapshot()==document.world,
            "Successful first append did not reuse the shared28-image prefix and add exactly28 new textures");
        auto historical=Json::parse(bytes(art.original_building));historical["schema_version"]=2;historical.erase("field_selection");
        for(auto& row:historical.at("buildings"))if(row.at("model_type").get<unsigned>()>=194&&row.at("model_type").get<unsigned>()<=196) {
            row=Json{{"model_type",row.at("model_type")},{"original_side",row.at("original_side")},
                {"native",false},{"fallback",row.at("fallback")},{"evidence",row.at("evidence")}};
        }
        const auto historical_path=output/"historical-schema2-profile.json";write(historical_path,historical.dump(2)+"\n");
        first_failed->set_original_building_visuals(historical_path,oe::VisualProfileSource::Builtin);
        check(first_failed->building_texture_count()==39&&first_failed->world().snapshot()==document.world&&
            first_failed->original_house_display_stats().configured_types==retained_houses,
            "Historical schema2 replacement did not retain the old39-image pool and paid authority");
        for(const auto field:field_owners) {
            const auto d=first_failed->original_building_visual_diagnostic(field);
            check(d.applicable&&!d.configured&&!d.active&&!d.asset&&!d.field_stage,
                "Historical schema2 inherited a stale schema3 field-stage table");
        }
        first_failed->set_original_building_visuals(art.original_building,oe::VisualProfileSource::Builtin);
        check(first_failed->building_texture_count()==56&&first_failed->world().snapshot()==document.world,
            "Schema3 reactivation did not preserve the shared pool and complete World");
        for(const auto field:field_owners)check(first_failed->original_building_visual_diagnostic(field).field_stage.has_value(),
            "Schema3 reactivation did not recover its actual paid field stage");
        first_failed->shutdown();first_failed.reset();
        auto custom=oe::SandboxView::create_original_deadline_food_dispatch(session(),std::nullopt,false);
        custom->configure_save(data,document.map_relative,output/"custom-working.save.json",document);
        const auto custom_art=oe::select_visual_profiles(compatibility,{},art.building);configure(*custom,custom_art);
        custom->set_original_building_visuals(art.original_building,oe::VisualProfileSource::Builtin);custom->initialize(window,renderer);
        check(!custom->original_building_display_stats().configured&&!custom->original_house_display_stats().configured&&custom->building_texture_count()==12&&
            custom->world().snapshot()==document.world,"Custom core unexpectedly acquired built-in Original tails or changed World");
        custom->shutdown();custom.reset();
        auto legacy_source=session();const auto border=oe::maps::MapGeometry(legacy_source.map.declared_map_size).border;
        auto legacy=std::make_unique<oe::SandboxView>(std::move(legacy_source),false,sim::RulesProfile::CityV16,4);
        legacy->configure_save(data,document.map_relative,output/"legacy-working.save.json");
        configure(*legacy,art);legacy->initialize(window,renderer);
        check(!legacy->original_building_display_stats().configured&&!legacy->original_house_display_stats().configured&&legacy->building_texture_count()==12,
            "Ordinary City-v16 acquired Original-only building images");
        std::optional<sim::BuildingId> post;
        for(int y=0;y<legacy->world().height()&&!post;++y)for(int x=0;x<legacy->world().width()&&!post;++x) {
            const sim::Command c{sim::CommandType::PlaceServicePost,{x,y}};
            if(legacy->world().validate(c).accepted) {const auto result=legacy->execute(c);check(result.accepted&&result.changed,"Ordinary paid ServicePost command failed");post=legacy->world().building_owner_at({x,y});}
        }
        check(post.has_value()&&sim::building_footprint(legacy->world().profile(),legacy->world().rule_version(),sim::Object::ServicePost).width==1&&
            !legacy->original_building_visual_diagnostic(*post).applicable,"Legacy ServicePost footprint/visual identity changed");
        const auto front=sim::building_front_cell(legacy->world().profile(),legacy->world().rule_version(),sim::Object::ServicePost,legacy->world().building(*post).cell);
        const auto height=legacy->world().map_permissions()->cell_height(front);
        center(*legacy,{double(front.x-front.y)*40,(double(front.x+front.y)-2*static_cast<double>(border))*20+20-height*40});
        const auto before=legacy->building_display_stats();check(legacy->render(),"Render ordinary legacy ServicePost");
        check(legacy->building_display_stats().drawn_instances[assets::role_index(assets::BuildingVisualRole::ServicePost)]>
            before.drawn_instances[assets::role_index(assets::BuildingVisualRole::ServicePost)],"Legacy ServicePost no longer uses its original core role");
        legacy->shutdown();legacy.reset();check(oe::BuildingSprite::live_texture_count()==0&&oe::WalkerSpriteSet::live_texture_count()==0,"Owned texture leak after view shutdown");
        check(bytes(input)==original_bytes,"Protected paid input changed");
        write(output/"report.json",Json{{"status","PASS"},{"runtime","SCRIPTED_SDL dummy/software"},{"source_save",input.string()},
            {"rule",18},{"schema",34},{"tick",document.world.ticks},{"complete_world_unchanged",true},{"full_f5_f9_bytes_equal",true},
            {"captures",records},{"matrix",matrix},{"alpha_picking",alpha},{"bodies",bodies},{"actors",actors},{"pure_frame_counters",counters},
            {"body_export_scope","Generic core lookup; specialized production routing requires a separate source/pixel audit"},
            {"custom_core_suppression",true},{"failed_replacements_atomic",true},{"failed_first_append_uploads",0},
            {"successful_first_append_uploads",appended_uploads},{"legacy_service_post_unchanged",true},
            {"capture_review","NOT_RUN"},{"native","NOT_RUN"},{"human","NOT_RUN"}}.dump(2));
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<"Paid Rule18 Original building View PASS; report "<<(output/"report.json")<<"; native/human NOT_RUN\n";return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);std::cerr<<error.what()<<'\n';if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
