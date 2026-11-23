// Authored map/model/pixels and ordinary paid controls only. This executable
// proves synthetic scripted SDL integration, never Original/native/human parity.
#include "EconomyStartViewFixture.h"
#include "AgricultureReconfigurationFixture.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/AgricultureFallbackRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace t=oe_agriculture_reconfiguration_fixture;
namespace art=oe::testing::economy;namespace fs=std::filesystem;
namespace perf=oe::performance;
using t::check;using Point=oe::scene::Point;
using Diagnostic=oe::SandboxView::AgricultureVisualDiagnostic;
using Domain=Diagnostic::Domain;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
using Color=std::array<std::uint8_t,4>;

const fs::path& presentation_output() {
    static const auto directory=[] {
        const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
        const auto path=fs::path(__FILE__).parent_path().parent_path()/".local/agri-mac12"/
            ("presentation-"+std::to_string(nonce));fs::create_directories(path);return path;
    }();
    return directory;
}

void event(oe::SandboxView& view,SDL_Event value) {bool running=true;view.handle_event(value,running);check(running,"Visual input closed the ordinary view");}
void key(oe::SandboxView& view,SDL_Keycode code) {SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(view,value);}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,float(point.x),float(point.y),&x,&y),"Invert the one production pointer transform");
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_MOTION;value.motion.x=x;value.motion.y=y;event(view,value);view.update(0);
    value={};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.x=x;value.button.y=y;event(view,value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,value);view.update(0);
}
void center(oe::SandboxView& view,Point ground) {
    const auto map=view.layout().map;const auto zoom=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-ground.x*zoom,map.y+map.h*.5-ground.y*zoom};
    for(unsigned attempt=0;attempt<1000;++attempt) {
        const auto now=view.camera().offset;const auto dx=wanted.x-now.x,dy=wanted.y-now.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else{held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused ordinary camera did not converge");
}
void zoom(oe::SandboxView& view,double wanted) {
    for(unsigned attempt=0;attempt<5&&view.camera().zoom!=wanted;++attempt)key(view,SDLK_Z);
    check(view.camera().zoom==wanted,"Normal zoom action missed1x/2x/4x");
}
Diagnostic diagnostic(const oe::SandboxView& view,Domain domain,std::uint64_t id) {
    const auto rows=view.agriculture_visual_diagnostics();const auto row=std::find_if(rows.begin(),rows.end(),
        [&](const auto& value){return value.domain==domain&&value.id==id;});
    check(row!=rows.end(),"A real paid entity is absent from the normal cached visual diagnostics");return *row;
}
bool same_observations(const std::vector<Diagnostic>& a,const std::vector<Diagnostic>& b) {
    if(a.size()!=b.size())return false;
    for(const auto& row:a) {
        const auto other=std::find_if(b.begin(),b.end(),[&](const auto& value){return value.domain==row.domain&&value.id==row.id;});
        if(other==b.end()||row.lifetime_id!=other->lifetime_id||row.source!=other->source||row.target!=other->target||
           row.object!=other->object||row.position!=other->position||row.kind!=other->kind||row.state!=other->state||
           row.progress!=other->progress||row.target_progress!=other->target_progress||row.harvest_progress!=other->harvest_progress||
           row.harvest_target!=other->harvest_target||row.dx!=other->dx||row.dy!=other->dy||row.cargo!=other->cargo||
           row.wet!=other->wet||row.moving!=other->moving||row.working!=other->working||row.blocked!=other->blocked||
           row.asset!=other->asset)return false;
    }
    return true;
}
Surface pixels(oe::SandboxView& view,SDL_Renderer* renderer) {
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Read actual synthetic viewport size");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get())&&view.render(),"Render the ordinary view to owned capture target");
    Surface raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    Surface rgba(raw?SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32):nullptr,SDL_DestroySurface);
    check(bool(rgba),"Read owned pre-Present synthetic pixels");check(SDL_SetRenderTarget(renderer,nullptr),"Restore ordinary window target");return rgba;
}
Color pixel(SDL_Surface* image,int x,int y) {
    Color value{};check(SDL_ReadSurfacePixel(image,x,y,&value[0],&value[1],&value[2],&value[3]),"Read an actual drawn silhouette pixel");return value;
}
bool near(Color a,Color b) {
    for(std::size_t i=0;i<3;++i)if(std::abs(int(a[i])-int(b[i]))>2)return false;
    return a[3]>240;
}
std::uint64_t map_hash(SDL_Surface* image,oe::sandbox_ui::Rect map) {
    std::uint64_t hash=1469598103934665603ULL;
    for(int y=map.y;y<map.y+map.h;++y)for(int x=map.x;x<map.x+map.w;++x)
        for(const auto value:pixel(image,x,y)){hash^=value;hash*=1099511628211ULL;}
    return hash;
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    const auto image=pixels(view,renderer);check(SDL_SavePNG(image.get(),path.string().c_str()),"Write private synthetic visual evidence");
}
void pure(oe::SandboxView& view,SDL_Renderer* renderer,unsigned frames=12) {
    check(view.paused(),"Pure display control requires the ordinary paused clock");
    const auto before=view.world().snapshot();const auto stats=view.agriculture_display_stats();const auto observations=view.agriculture_visual_diagnostics();
    const auto first=pixels(view,renderer);const auto expected=map_hash(first.get(),view.layout().map);
    perf::set_enabled(true);perf::reset();
    for(unsigned frame=0;frame<frames;++frame){view.update(.017);check(view.render(),"Submit a pure ordinary agriculture frame");(void)view.inspection_lines();}
    for(std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(!perf::counter(static_cast<perf::Counter>(i)),"Paused ordinary visual frame performed resource/World/navigation work");
    perf::set_enabled(false);const auto last=pixels(view,renderer);const auto after=view.agriculture_display_stats();
    check(view.world().snapshot()==before&&after.prepare_count==stats.prepare_count&&after.prepared_tick==stats.prepared_tick&&
        after.prepared_sequence==stats.prepared_sequence&&same_observations(observations,view.agriculture_visual_diagnostics())&&
        expected==map_hash(last.get(),view.layout().map),"Paused frames changed saved-state poses, silhouette pixels, identity or cache epoch");
}

struct Harness {
    t::Fixture source;art::Temp graphics;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window{nullptr,SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer{nullptr,SDL_DestroyRenderer};
    std::unique_ptr<oe::SandboxView> view;
    fs::path output=presentation_output();
    explicit Harness(bool latest=true,bool central_lake=false) {
        // macOS's temporary directory may be reached through /var. Saves
        // correctly refuse symlink ancestry; use the real owned fixture path.
        graphics.root=fs::canonical(graphics.root);
        if(central_lake)for(int y=94;y<=105;++y)for(int x=100;x<=132;++x)source.raw[std::size_t(y)*228U+std::size_t(x)]=4U;
        fs::copy(graphics.root/"data/DATA",source.root.data/"DATA",fs::copy_options::recursive);
        art::write(source.root.data/t::f::map_relative,oe_fishing_fixture::encoded_map(source.raw,source.fertility,true));
        SDL_Window* raw_window=nullptr;SDL_Renderer* raw_renderer=nullptr;
        check(SDL_CreateWindowAndRenderer("Agriculture visuals synthetic scripted SDL",1440,1000,SDL_WINDOW_HIDDEN,&raw_window,&raw_renderer),"Create owned synthetic view");
        window.reset(raw_window);renderer.reset(raw_renderer);
        auto session=oe::maps::load_stored_map_session(source.root.data,t::f::map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        view=latest?oe::SandboxView::create_agriculture_reconfiguration(std::move(session),std::nullopt,false):
            oe::SandboxView::create_food_warehouses(std::move(session),std::nullopt,false);
        view->configure_save(source.root.data,t::f::map_relative,graphics.root/(latest?"visual52.save.json":"historical51.save.json"));
        view->initialize(window.get(),renderer.get());SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);zoom(*view,1);
        fs::create_directories(output);check(!view->debug_diagnostics()&&view->paused(),"Synthetic view bypassed normal inspection or paused start");
    }
    sim::BuildingId buy(sim::CommandType type,sim::Cell cell,sim::BuildingId owner=sim::BuildingId{}) {
        sim::Command command{type,cell};command.building_id=owner;const auto result=view->execute(command);
        if(!result.accepted||!result.changed)throw std::runtime_error("Ordinary paid visual construction: "+result.reason);
        return view->world().building_owner_at(cell).value_or(sim::BuildingId{});
    }
    template<class Predicate>void until(Predicate done,const char* why,unsigned limit=14000) {
        for(unsigned tick=0;tick<limit&&!done();++tick){view->tick_once();}
        check(done(),why);
    }
    void save_restore() {
        const auto before=view->world().snapshot();const auto observations=view->agriculture_visual_diagnostics();const auto saved=view->save_generation();
        key(*view,SDLK_F5);
        if(view->save_generation()!=saved+1)throw std::runtime_error("Normal F5 did not publish private exact-generation state: path="+
            view->save_path().string()+" generation="+std::to_string(saved)+"->"+std::to_string(view->save_generation())+
            " recovery_safe="+std::to_string(view->recovery_safe_point())+" exists="+std::to_string(fs::exists(view->save_path()))+
            " last_message="+view->last_message());
        key(*view,SDLK_PERIOD);check(view->world().ticks()==before.ticks+1,"Normal single Step did not advance exactly one World tick");
        const auto loaded=view->load_generation();key(*view,SDLK_F9);
        check(view->render(),"Submit freshly restored actual visual state");
        check(view->load_generation()==loaded+1&&view->world().snapshot()==before&&
            same_observations(observations,view->agriculture_visual_diagnostics()),"F9 did not rebuild the exact state/pose/lifetime from saved World authority");
    }
};
struct CropScene {sim::BuildingId farm{},field{};};
CropScene crop_city(Harness& h) {
    for(int x=89;x<=132;++x)h.buy(sim::CommandType::PlaceRoad,{x,109});
    for(int x=90;x<126;x+=3)h.buy(sim::CommandType::PlaceHousehold,{x,107});
    CropScene city;city.farm=h.buy(sim::CommandType::PlaceOriginalFoodFarm,{90,110});
    city.field=h.buy(sim::CommandType::PlaceOriginalWheatField,{93,110},city.farm);
    h.buy(sim::CommandType::PlaceMarket,{97,110});h.buy(sim::CommandType::PlaceOriginalWaterWell,{101,110});
    h.buy(sim::CommandType::PlaceOriginalAncestorShrine,{105,110});
    for(int x:{109,112})h.buy(sim::CommandType::PlaceFireWatch,{x,110});return city;
}
void center_entity(Harness& h,Domain domain,std::uint64_t id) {
    check(h.view->render(),"Submit current camera before using last-frame hit diagnostics");
    auto row=diagnostic(*h.view,domain,id);center(*h.view,h.view->camera().screen_to_world(row.screen_ground));check(h.view->render(),"Submit centered actual entity");
    row=diagnostic(*h.view,domain,id);
    if(row.submitted&&row.image_width>0&&row.image_height>0) {
        center(*h.view,h.view->camera().screen_to_world({row.image_origin.x+row.image_width*.5,row.image_origin.y+row.image_height*.5}));
        check(h.view->render(),"Submit actual image-bound center");
    }
}
void select_body(Harness& h,sim::BuildingId id,bool roof=false) {
    h.view->set_tool(5);center_entity(h,Domain::Building,static_cast<unsigned>(id));
    const auto row=diagnostic(*h.view,Domain::Building,static_cast<unsigned>(id));check(row.submitted&&row.image_width>0&&row.image_height>0,"Live fallback body was not submitted");
    const auto image=pixels(*h.view,h.renderer.get());const auto map=h.view->layout().map;
    for(const double fy:roof?std::array{.2,.3,.4}:std::array{.5,.7,.85})for(const double fx:{.5,.3,.7,.15,.85}) {
        const Point point{row.image_origin.x+row.image_width*fx,row.image_origin.y+row.image_height*fy};
        if(!map.contains(point.x,point.y))continue;
        if(roof&&!near(pixel(image.get(),int(point.x),int(point.y)),{112,100,84,255}))continue;
        click(*h.view,h.renderer.get(),point);
        if(h.view->selected_building()==id&&!h.view->selected_agriculture_actor())return;
    }
    throw std::runtime_error(roof?"Visible authored roof did not select its complete paid body":"Normal crop/body picking failed");
}
std::vector<Color> palette(const std::string& kind) {
    Color front{};bool person=false;
    if(kind=="Food Grower"||kind=="Food Courier"){front={76,141,102,255};person=true;}
    else if(kind=="Hemp Grower"){front={155,125,76,255};person=true;}
    else if(kind=="Plantation Grower"){front={111,115,163,255};person=true;}
    else if(kind=="Hunter"){front={150,106,58,255};person=true;}
    else if(kind=="Wild Pig")front={119,92,73,255};
    else if(kind=="Pheasant")front={151,91,58,255};
    else if(kind=="Antelope")front={185,149,91,255};
    else return {{220,176,124,255},{209,180,105,255},{136,91,51,255},{86,58,38,255}};
    auto rear=front;for(std::size_t i=0;i<3;++i)rear[i]=static_cast<std::uint8_t>(int(rear[i])-16);
    std::vector<Color> result{front,rear};
    // Genuine visible head/hat pixels remain selectable when the torso is
    // correctly occluded. These are authored raster colours, never bbox hits.
    if(person)for(const Color color:std::array<Color,3>{{{220,176,124,255},{113,83,55,255},{209,180,105,255}}})result.push_back(color);
    return result;
}
Point select_actor(Harness& h,Domain domain,std::uint64_t id,bool subpixel=false) {
    h.view->set_tool(5);center_entity(h,domain,id);const auto row=diagnostic(*h.view,domain,id);
    check(row.submitted&&!row.sprite_drawn&&row.art.find("AUTHORED")!=std::string::npos,"Synthetic actor borrowed unqualified Original art or has no submitted body");
    const auto image=pixels(*h.view,h.renderer.get());const auto map=h.view->layout().map;const auto colors=palette(row.kind);
    std::vector<Point> candidates;
    for(int y=std::max(map.y,int(std::floor(row.image_origin.y)));y<std::min(map.y+map.h,int(std::ceil(row.image_origin.y+row.image_height)));++y)
        for(int x=std::max(map.x,int(std::floor(row.image_origin.x)));x<std::min(map.x+map.w,int(std::ceil(row.image_origin.x+row.image_width)));++x)
            if(std::any_of(colors.begin(),colors.end(),[&](Color value){return near(pixel(image.get(),x,y),value);}))candidates.push_back({x+.5,y+.5});
    const Point preferred{row.screen_ground.x,row.screen_ground.y-14*h.view->camera().zoom};
    std::stable_sort(candidates.begin(),candidates.end(),[&](Point a,Point b){return std::hypot(a.x-preferred.x,a.y-preferred.y)<std::hypot(b.x-preferred.x,b.y-preferred.y);});
    bool selected=false;Point chosen{};
    for(std::size_t i=0;i<std::min<std::size_t>(candidates.size(),192);++i) {
        click(*h.view,h.renderer.get(),candidates[i]);
        selected=h.view->selected_agriculture_actor()==std::make_optional(std::make_pair(domain,id));if(selected){chosen=candidates[i];break;}
    }
    if(!selected) {
        const auto actual=h.view->selected_agriculture_actor();
        const auto stem="selection-failure-"+std::to_string(static_cast<unsigned>(domain))+"-"+std::to_string(id);
        capture(*h.view,h.renderer.get(),h.output/(stem+".png"));
        nlohmann::json report{{"kind",row.kind},{"domain",static_cast<unsigned>(domain)},{"id",id},{"lifetime",row.lifetime_id},
            {"tick",h.view->world().ticks()},{"source",static_cast<unsigned>(row.source)},{"position",{row.position.x,row.position.y}},
            {"screen_ground",{row.screen_ground.x,row.screen_ground.y}},{"image_origin",{row.image_origin.x,row.image_origin.y}},
            {"image_size",{row.image_width,row.image_height}},{"candidate_count",candidates.size()},
            {"selected_building",static_cast<unsigned>(h.view->selected_building().value_or(sim::BuildingId{}))},
            {"selected_domain",actual?nlohmann::json(static_cast<unsigned>(actual->first)):nlohmann::json(nullptr)},
            {"selected_id",actual?nlohmann::json(actual->second):nlohmann::json(nullptr)},
            {"last_message",h.view->last_message()},{"camera_zoom",h.view->camera().zoom}};
        report["candidate_sample"]=nlohmann::json::array();for(std::size_t i=0;i<std::min<std::size_t>(candidates.size(),16);++i)
            report["candidate_sample"].push_back({candidates[i].x,candidates[i].y});
        std::ofstream diagnostic_file(h.output/(stem+".json"));diagnostic_file<<report.dump(2)<<'\n';
        throw std::runtime_error("Actual drawn opaque actor pixels did not select their real typed entity: kind="+row.kind+
            " domain="+std::to_string(static_cast<unsigned>(domain))+" id="+std::to_string(id)+" lifetime="+std::to_string(row.lifetime_id)+
            " tick="+std::to_string(h.view->world().ticks())+" candidates="+std::to_string(candidates.size())+
            " selected_domain="+(actual?std::to_string(static_cast<unsigned>(actual->first)):"none")+
            " selected_id="+(actual?std::to_string(actual->second):"none")+
            " selected_building="+std::to_string(static_cast<unsigned>(h.view->selected_building().value_or(sim::BuildingId{})))+
            " last_message="+h.view->last_message());
    }
    if(subpixel) {
        const auto y=int(std::floor(row.screen_ground.y-13.25*h.view->camera().zoom));int right=-1;
        for(int x=int(row.screen_ground.x-8*h.view->camera().zoom);x<=int(row.screen_ground.x+8*h.view->camera().zoom);++x)
            if(map.contains(x+.5,y+.5)&&near(pixel(image.get(),x,y),{76,141,102,255}))right=x;
        check(right>=0,"Real Grower torso has no rasterized boundary witness");
        click(*h.view,h.renderer.get(),{right+.98,y+.5});
        check(h.view->selected_agriculture_actor()==std::make_optional(std::make_pair(domain,id)),"Fractional pointer missed a genuinely painted actor boundary pixel");
    }
    const Point transparent{row.image_origin.x+.12*h.view->camera().zoom,row.image_origin.y+.12*h.view->camera().zoom};
    check(map.contains(transparent.x,transparent.y),"Transparent-corner test escaped map aperture");click(*h.view,h.renderer.get(),transparent);
    check(h.view->selected_agriculture_actor()!=std::make_optional(std::make_pair(domain,id)),"Transparent bounding-box corner stole actor selection");
    click(*h.view,h.renderer.get(),chosen);
    check(h.view->selected_agriculture_actor()==std::make_optional(std::make_pair(domain,id)),"Normal actor re-selection failed");
    return chosen;
}
void toggle_operation(Harness& h,sim::BuildingId id) {
    check(h.view->selected_building()==id,"Normal operation control is bound to another owner");const auto button=h.view->inspector_layout().operation_toggle;
    check(button.h>0&&button.w>0,"Normal operation control is hidden");click(*h.view,h.renderer.get(),{button.x+button.w*.5,button.y+button.h*.5});
}

void paid_roofs_budgets_and_historical_boundary() {
    Harness h;const auto city=crop_city(h);auto& view=*h.view;
    const auto stats=view.agriculture_display_stats();check(stats.active&&stats.cached_entities>=2&&stats.cached_actors==view.world().hunting_state()->animals.size()&&
        stats.prepared_tick==view.world().ticks()&&stats.prepared_sequence==view.world().command_sequence()&&stats.prepared_meshes<=82+58&&
        stats.cpu_mesh_bytes<=(82+58)*sizeof(oe::AgricultureMesh),"Prepared visual cache expanded budgets or missed actual publication epoch");
    for(const double scale:{1.,2.,4.}) {
        zoom(view,scale);select_body(h,city.farm,true);capture(view,h.renderer.get(),h.output/("farm-roof-"+std::to_string(int(scale))+"x.png"));
        const auto row=diagnostic(view,Domain::Building,static_cast<unsigned>(city.farm));
        const Point empty{row.image_origin.x+row.image_width*.04,row.image_origin.y+row.image_height*.14};
        check(view.layout().map.contains(empty.x,empty.y),"Roof transparent witness is outside ordinary map");click(view,h.renderer.get(),empty);
        check(view.selected_building()!=city.farm,"Transparent roof bounding rectangle selected the wrong paid Farm");
    }
    pure(view,h.renderer.get(),24);t::valid(view.world());
    Harness historical(false);const auto old=crop_city(historical);(void)old;
    check(!historical.view->agriculture_display_stats().active&&historical.view->agriculture_display_stats().cached_entities==0&&
        historical.view->agriculture_display_stats().prepared_meshes==0&&historical.view->agriculture_visual_diagnostics().empty(),
        "Rule35 acquired Rule36 agriculture meshes/state overlays");pure(*historical.view,historical.renderer.get());
    capture(*historical.view,historical.renderer.get(),historical.output/"historical35.png");
}

void food_work_ready_loaded_pick_and_exact_f5_f9() {
    Harness h;const auto city=crop_city(h);auto& view=*h.view;
    h.until([&]{return view.world().agriculture_field_progress(city.field)>0;},"Normal visual city never earned actual Food work");
    const auto worker=t::worker(view.world(),city.farm);check(worker.latest_trip.has_value(),"Real field work lacks actual Grower lifetime");
    for(const double scale:{1.,2.,4.}) {
        zoom(view,scale);select_actor(h,Domain::Courier,static_cast<unsigned>(worker.courier_id),true);
        const auto row=diagnostic(view,Domain::Courier,static_cast<unsigned>(worker.courier_id));
        check(row.source==city.farm&&row.target==city.field&&row.lifetime_id==worker.latest_trip->grower_id.value&&
            row.state=="Working"&&!row.moving&&row.cargo==0,"Normal at-field worker inspector lost its real state/identity");
        capture(view,h.renderer.get(),h.output/("food-working-"+std::to_string(int(scale))+"x.png"));pure(view,h.renderer.get());
    }
    h.until([&]{return view.world().agriculture_field_progress(city.field)==80;},"Real crop never reached held ready80");zoom(view,1);select_body(h,city.field);
    auto ready=diagnostic(view,Domain::Building,static_cast<unsigned>(city.field));
    check(ready.progress==80&&ready.target_progress==80&&ready.state=="Ready"&&ready.submitted&&ready.image_width>0,
        "Held ready80 disappeared or borrowed another crop's work");
    const auto qualified=view.original_building_visual_diagnostic(city.field);
    if(qualified.active)check(qualified.asset&&qualified.field_stage==5,"Ready80 lost the prepared final qualified crop image");
    capture(view,h.renderer.get(),h.output/"field-ready80.png");h.save_restore();
    check(diagnostic(view,Domain::Building,static_cast<unsigned>(city.field)).progress==80,"F9 changed held ready crop");
    h.until([&]{return view.world().physical_food_grower_cargo(worker.courier_id)>0;},"Real crop never became loaded Grower cargo");
    select_actor(h,Domain::Courier,static_cast<unsigned>(worker.courier_id));const auto loaded=diagnostic(view,Domain::Courier,static_cast<unsigned>(worker.courier_id));
    check(loaded.cargo==1&&loaded.state=="Returning"&&!loaded.working&&loaded.lifetime_id==worker.latest_trip->grower_id.value,
        "Loaded Grower display credited stock or kept a false harvest action");
    capture(view,h.renderer.get(),h.output/"food-loaded-return.png");toggle_operation(h,city.farm);
    check(!view.world().building(city.farm).operating_enabled&&diagnostic(view,Domain::Courier,static_cast<unsigned>(worker.courier_id)).cargo==1,
        "Normal source Pause lost already picked crop cargo");h.save_restore();select_actor(h,Domain::Courier,static_cast<unsigned>(worker.courier_id));pure(view,h.renderer.get());
    capture(view,h.renderer.get(),h.output/"food-loaded-paused.png");
    toggle_operation(h,city.farm);check(view.world().building(city.farm).operating_enabled,"Normal Farm Resume failed before lifetime reuse control");
    const auto old_pixel=select_actor(h,Domain::Courier,static_cast<unsigned>(worker.courier_id));
    check(view.render(),"Paint the genuinely selected old Grower lifetime");
    const auto old_life=diagnostic(view,Domain::Courier,static_cast<unsigned>(worker.courier_id)).lifetime_id;
    const auto retired_before=t::worker(view.world(),city.farm).retired;
    check(view.selected_agriculture_actor()==std::make_optional(std::make_pair(Domain::Courier,static_cast<std::uint64_t>(worker.courier_id))),
        "Stale-lifetime control did not begin with a genuinely selected painted actor");
    // Deliberately retain the displayed old frame. Ordinary paid World ticks
    // retire its lifetime and later reuse the fixed movement channel, without
    // allowing a render/diagnostic to manufacture a new visible hit rectangle.
    h.until([&]{const auto& current=t::worker(view.world(),city.farm);
        return current.latest_trip&&!current.latest_trip->returned_tick&&current.latest_trip->grower_id.value>old_life&&
            current.latest_trip->phase==sim::PhysicalFoodGrowerPhase::Working&&current.latest_trip->work_ticks>0;},
        "Same real Grower channel never earned its next working lifetime");
    const auto& replacement=t::worker(view.world(),city.farm);
    check(replacement.courier_id==worker.courier_id&&replacement.retired>retired_before&&
        !view.selected_agriculture_actor(),"Already-selected retired lifetime silently became the new same-channel actor");
    const auto new_life=diagnostic(view,Domain::Courier,static_cast<unsigned>(worker.courier_id));
    check(new_life.lifetime_id>old_life&&!new_life.submitted,"New lifetime inherited old-frame submitted geometry/provenance");
    const auto untouched=view.world().snapshot();click(view,h.renderer.get(),old_pixel);
    check(!view.selected_agriculture_actor()&&view.world().snapshot()==untouched,
        "Clicking an obsolete painted actor selected its new same-channel lifetime or mutated World");
    select_actor(h,Domain::Courier,static_cast<unsigned>(worker.courier_id));
    check(diagnostic(view,Domain::Courier,static_cast<unsigned>(worker.courier_id)).lifetime_id==new_life.lifetime_id,
        "Freshly painted normal input did not select the actual replacement lifetime");
    capture(view,h.renderer.get(),h.output/"food-new-lifetime.png");pure(view,h.renderer.get());t::valid(view.world());
}

void boat_hunter_work_unload_and_source_pause() {
    Harness h(true,true);auto& view=*h.view;
    for(int x=89;x<=153;++x)for(int y:{114,117})h.buy(sim::CommandType::PlaceRoad,{x,y});
    for(int y=115;y<=116;++y)for(int x:{89,153})h.buy(sim::CommandType::PlaceRoad,{x,y});
    for(int x=90;x<150;x+=3)h.buy(sim::CommandType::PlaceHousehold,{x,112});
    for(int x=114;x<=119;++x)h.buy(sim::CommandType::PlaceRoad,{x,107});for(int y=108;y<=113;++y)h.buy(sim::CommandType::PlaceRoad,{119,y});
    h.buy(sim::CommandType::PlaceMarket,{134,118});h.buy(sim::CommandType::PlaceOriginalWaterWell,{138,118});h.buy(sim::CommandType::PlaceOriginalAncestorShrine,{142,118});
    h.buy(sim::CommandType::PlaceFireWatch,{110,118});h.buy(sim::CommandType::PlaceFireWatch,{146,118});
    const auto farm=h.buy(sim::CommandType::PlaceOriginalFoodFarm,{98,118});h.buy(sim::CommandType::PlaceOriginalMilletField,{101,118},farm);
    const auto quay=h.buy(sim::CommandType::PlaceOriginalFishingQuay,{114,105});for(int y=118;y<=120;++y)h.buy(sim::CommandType::PlaceRoad,{129,y});
    const auto tent=h.buy(sim::CommandType::PlaceOriginalHuntersTent,{128,121});
    h.until([&]{return std::any_of(view.world().fishing_state()->boats.begin(),view.world().fishing_state()->boats.end(),
        [](const auto& boat){return boat.phase==sim::FishingBoatPhase::Fishing&&boat.work_progress>0;});},"Paid Quay never earned real fishing work");
    auto boat=*std::find_if(view.world().fishing_state()->boats.begin(),view.world().fishing_state()->boats.end(),
        [](const auto& value){return value.phase==sim::FishingBoatPhase::Fishing&&value.work_progress>0;});
    select_actor(h,Domain::Boat,boat.id.value);auto row=diagnostic(view,Domain::Boat,boat.id.value);
    check(row.state=="Working"&&!row.moving&&row.progress==boat.work_progress%4&&row.source==quay,"Boat fishing pose used another work/owner clock");
    capture(view,h.renderer.get(),h.output/"boat-working.png");h.save_restore();
    h.until([&]{return std::any_of(view.world().fishing_state()->boats.begin(),view.world().fishing_state()->boats.end(),
        [](const auto& value){return value.phase==sim::FishingBoatPhase::Unloading&&value.work_progress>0&&value.cargo>0;});},"Real Boat never returned to dock unloading");
    boat=*std::find_if(view.world().fishing_state()->boats.begin(),view.world().fishing_state()->boats.end(),
        [](const auto& value){return value.phase==sim::FishingBoatPhase::Unloading&&value.work_progress>0&&value.cargo>0;});
    select_actor(h,Domain::Boat,boat.id.value);row=diagnostic(view,Domain::Boat,boat.id.value);
    check(row.state=="Unloading"&&!row.moving&&row.cargo==boat.cargo&&row.progress==boat.work_progress%4,
        "Actual dock unload was presented as fishing/tending or travelled movement");
    capture(view,h.renderer.get(),h.output/"boat-unloading.png");toggle_operation(h,quay);
    check(!view.world().building(quay).operating_enabled,"Normal Quay Pause control did not reach its paid owner");
    for(unsigned tick=0;tick<12;++tick)view.tick_once();const auto frozen=diagnostic(view,Domain::Boat,boat.id.value);
    check(frozen.position==row.position&&frozen.progress==row.progress&&frozen.cargo==row.cargo&&frozen.state=="Unloading"&&
        diagnostic(view,Domain::Building,static_cast<unsigned>(quay)).blocked,"Source-paused dock unload falsely advanced pose/work or lost cargo");
    select_actor(h,Domain::Boat,boat.id.value);pure(view,h.renderer.get());h.save_restore();capture(view,h.renderer.get(),h.output/"boat-unloading-paused.png");
    h.until([&]{return std::any_of(view.world().hunting_state()->hunters.begin(),view.world().hunting_state()->hunters.end(),
        [](const auto& hunter){return hunter.phase==sim::HuntingHunterPhase::Hunting&&hunter.work_progress>0;});},"Paid Tent never earned actual contact hunting work");
    const auto hunter=*std::find_if(view.world().hunting_state()->hunters.begin(),view.world().hunting_state()->hunters.end(),
        [](const auto& value){return value.phase==sim::HuntingHunterPhase::Hunting&&value.work_progress>0;});
    select_actor(h,Domain::Hunter,hunter.id.value);row=diagnostic(view,Domain::Hunter,hunter.id.value);
    check(row.state=="Working"&&!row.moving&&row.progress==hunter.work_progress%4&&row.source==tent&&row.cargo==0,
        "Hunter contact work borrowed a route/fishing/unload animation");capture(view,h.renderer.get(),h.output/"hunter-working.png");pure(view,h.renderer.get());t::valid(view.world());
}

void wildlife_and_material_workers_use_live_silhouettes() {
    Harness h;const auto city=crop_city(h);auto& view=*h.view;
    const auto tea=h.buy(sim::CommandType::PlaceOriginalTeaCuringShed,{115,110});h.buy(sim::CommandType::PlaceOriginalTeaBush,{115,114},tea);
    const auto hemp=h.buy(sim::CommandType::PlaceOriginalHempFarm,{127,110});h.buy(sim::CommandType::PlaceOriginalHempField,{129,110},hemp);
    for(const auto species:{sim::HuntingSpecies::WildPig75,sim::HuntingSpecies::Pheasant76,sim::HuntingSpecies::Antelope77}) {
        const auto animals=view.world().hunting_state()->animals;const auto animal=std::find_if(animals.begin(),animals.end(),[&](const auto& value){return value.species==species;});
        check(animal!=animals.end(),"Fresh authored World lacks a supported real animal species");
        for(const double scale:{1.,2.,4.}) {
            zoom(view,scale);select_actor(h,Domain::Animal,animal->id.value);const auto row=diagnostic(view,Domain::Animal,animal->id.value);
            check(row.position==view.world().hunting_animal_position(animal->id)&&row.lifetime_id==animal->id.value&&!row.working&&row.cargo==0,
                "Normal wildlife selection borrowed another identity/position or harvest work");
            capture(view,h.renderer.get(),h.output/("animal-"+std::to_string(static_cast<unsigned>(species))+"-"+std::to_string(int(scale))+"x.png"));
        }
    }
    zoom(view,1);bool tea_seen=false,hemp_seen=false;
    for(unsigned tick=0;tick<14000&&!(tea_seen&&hemp_seen);++tick) {
        view.tick_once();const auto rows=view.agriculture_visual_diagnostics();
        for(const auto& row:rows)if(row.domain==Domain::Courier&&row.cargo>0&&((row.source==tea&&!tea_seen)||(row.source==hemp&&!hemp_seen))) {
            select_actor(h,row.domain,row.id);const auto actual=diagnostic(view,row.domain,row.id);
            check(actual.state=="Returning"&&!actual.working&&actual.cargo==1&&actual.lifetime_id>0,
                "Instant material pickup invented a sustained tending/unloading action");
            capture(view,h.renderer.get(),h.output/(row.source==tea?"tea-loaded-worker.png":"hemp-loaded-worker.png"));
            h.save_restore();if(row.source==tea)tea_seen=true;else hemp_seen=true;
        }
    }
    check(tea_seen&&hemp_seen,"Normal paid city never earned both actual Tea/Hemp loaded workers");
    (void)city;pure(view,h.renderer.get());t::valid(view.world());
}
} // namespace

int main() {
    try {
        std::cout<<"Synthetic presentation output: "<<presentation_output()<<'\n';
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");check(SDL_Init(SDL_INIT_VIDEO),"Initialize independent scripted visual SDL");
        paid_roofs_budgets_and_historical_boundary();
        food_work_ready_loaded_pick_and_exact_f5_f9();
        boat_hunter_work_unload_and_source_pause();
        wildlife_and_material_workers_use_live_silhouettes();
        SDL_Quit();std::cout<<"Agriculture visual presentation4 cases: normal roof/body/raster-boundary picking1/2/4, actual states, paused pixels/caches, exact F5/F9 and historical35 isolation PASS; synthetic scripted SDL only; qualified ready80 Original bank/native/human acceptance NOT_RUN here\n";return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
