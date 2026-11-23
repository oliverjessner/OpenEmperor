// Actual paid checkpoints through the normal SandboxView/SDL production renderer.
// Scripted Metal is not native OS input or human acceptance.
#include "FoodNativePlaytest.h"
#include "core/PerformanceDiagnostics.h"
#include "app/MenuSession.h"
#include <charconv>
#include <chrono>
#include <cmath>
#include <numeric>
#include <map>
#include <set>
namespace p=oe_food_playtest;
namespace save=oe::persistence;
namespace perf=oe::performance;
namespace {
using Diagnostic=oe::SandboxView::AgricultureVisualDiagnostic;
using Domain=Diagnostic::Domain;
bool watch_native=false;
void require(bool ok,const std::string& message){p::require(ok,message);}
void no_symlinks(const fs::path& path){fs::path current;for(const auto& part:path){current/=part;std::error_code error;const auto status=fs::symlink_status(current,error);require(error==std::errc::no_such_file_or_directory||!error,"Cannot inspect input ancestry");require(error||!fs::is_symlink(status),"Input must not traverse symlinks");}}
std::string bytes(const fs::path& path,std::uintmax_t maximum) {
    no_symlinks(path);
    require(fs::is_regular_file(path)&&fs::file_size(path)>0&&fs::file_size(path)<=maximum,"Protected input exceeds its size bound");
    std::ifstream file(path,std::ios::binary);require(bool(file),"Cannot read protected input "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
struct InputGuard {
    fs::path path;std::string literal;fs::perms permissions;fs::file_time_type modified;std::uintmax_t maximum;
    explicit InputGuard(fs::path file,std::uintmax_t bound=8U*1024U*1024U):path(std::move(file)),literal(bytes(path,bound)),
        permissions(fs::status(path).permissions()),modified(fs::last_write_time(path)),maximum(bound) {}
    void unchanged()const {
        require(bytes(path,maximum)==literal&&fs::status(path).permissions()==permissions&&fs::last_write_time(path)==modified,
            "Protected input bytes, permissions or modification time changed: "+path.string());
    }
};
struct SourceGuard {
    InputGuard model,map;
    SourceGuard(const fs::path& data,const fs::path& relative):model(data/"Model/EmperorBuildingModels.txt",64U*1024U*1024U),
        map(data/relative,64U*1024U*1024U) {no_symlinks(model.path);no_symlinks(map.path);}
    void unchanged()const {model.unchanged();map.unchanged();}
};
void user_takeover() {
    SDL_Event value{};
    while(SDL_PollEvent(&value)) {
        const bool motion=value.type==SDL_EVENT_MOUSE_MOTION&&(value.motion.xrel!=0||value.motion.yrel!=0);
        const bool input=value.type==SDL_EVENT_KEY_DOWN||value.type==SDL_EVENT_KEY_UP||value.type==SDL_EVENT_TEXT_INPUT||
            value.type==SDL_EVENT_MOUSE_BUTTON_DOWN||value.type==SDL_EVENT_MOUSE_BUTTON_UP||value.type==SDL_EVENT_MOUSE_WHEEL||motion||
            value.type==SDL_EVENT_FINGER_DOWN||value.type==SDL_EVENT_WINDOW_FOCUS_LOST;
        if(value.type==SDL_EVENT_QUIT||value.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED||(watch_native&&input))
            throw std::runtime_error("Stopped scripted verification because native user input, focus takeover or window closure was observed");
    }
}
void event(oe::SandboxView& view,SDL_Event value) {
    user_takeover();bool running=true;view.handle_event(value,running);require(running,"Scripted event closed the view");user_takeover();
}
void key(oe::SandboxView& view,SDL_Keycode code) {user_takeover();p::key(view,code);user_takeover();}
void click(oe::SandboxView& view,SDL_Renderer* renderer,oe::scene::Point point) {
    float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y),SDL_GetError());
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_MOTION;value.motion.x=x;value.motion.y=y;event(view,value);view.update(0);
    value={};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.down=true;value.button.x=x;value.button.y=y;event(view,value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;value.button.down=false;event(view,value);view.update(0);
}
bool select_building_sparse(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id,std::uint32_t border) {
    user_takeover();view.set_tool(5);const auto& body=view.world().building(id);
    const auto front=sim::building_front_cell(view.world().profile(),view.world().rule_version(),body.kind,body.cell);
    auto target=oe::maps::terrain_ground({static_cast<unsigned>(front.x),static_cast<unsigned>(front.y)},border);
    target.y-=16;p::center(view,target);user_takeover();require(view.render(),SDL_GetError());
    bool selected=false;
    for(const auto cell:view.world().building_cells(id)) {
        const auto ground=oe::maps::terrain_ground({static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border);
        const auto center=view.camera().world_to_screen(ground);
        for(const int dy:{0,-8,-16,-24,-32,-48,-64,8,16}) {
            user_takeover();
            for(const int dx:{0,-4,4,-8,8}) {
                const oe::scene::Point point{center.x+dx*view.camera().zoom,center.y+dy*view.camera().zoom};
                if(!view.layout().map.contains(point.x,point.y))continue;
                const auto picked=view.pick(point);if(!picked||view.world().building_owner_at(*picked)!=id)continue;
                click(view,renderer,point);selected=view.selected_building()==id&&!view.selected_agriculture_actor();if(selected)break;
            }
            if(selected)break;
        }
        if(selected)break;
    }
    require(view.render(),SDL_GetError());return selected;
}

Json pure_frames(oe::SandboxView& view,SDL_Renderer* renderer,unsigned frames) {
    const auto before=view.world().snapshot();const auto prepared=view.agriculture_inspection_prepare_count();const auto meshes=view.agriculture_display_stats();
    std::vector<double> samples;samples.reserve(frames);
    const auto started=std::chrono::steady_clock::now();
    perf::set_enabled(true);perf::reset();
    for(unsigned frame=0;frame<frames;++frame) {
        user_takeover();const auto begin=std::chrono::steady_clock::now();
        view.update(0);require(view.render(),SDL_GetError());(void)view.inspection_lines();
        samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
    }
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    const auto total=std::accumulate(samples.begin(),samples.end(),0.0);std::sort(samples.begin(),samples.end());
    const auto percentile=static_cast<std::size_t>(std::ceil(0.95*static_cast<double>(samples.size())))-1;
    Json counters=Json::object();
    for(std::size_t i=0;i<perf::counter_names.size();++i) {
        const auto count=perf::counter(static_cast<perf::Counter>(i));counters[std::string(perf::counter_names[i])]=count;
        require(!count,"Paused inspector/render performed "+std::string(perf::counter_names[i]));
    }
    perf::set_enabled(false);require(view.world().snapshot()==before&&view.agriculture_inspection_prepare_count()==prepared,
        "Paused frames changed World or rebuilt inspection preparation");const auto after=view.agriculture_display_stats();require(after.prepare_count==meshes.prepare_count&&after.prepared_meshes==meshes.prepared_meshes&&after.cpu_mesh_bytes==meshes.cpu_mesh_bytes,"Pure frames rebuilt agriculture meshes");return {{"frames",frames},{"renderer",SDL_GetRendererName(renderer)},{"tier",watch_native?"NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},{"counters",counters},{"inspection_prepare_unchanged",true},
            {"timing_ms",{{"samples",samples.size()},{"total",total},{"wall_total",elapsed},{"mean",total/static_cast<double>(samples.size())},{"p95",samples.at(percentile)},{"max",samples.back()}}}};
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    user_takeover();require(SDL_GetRenderTarget(renderer)==nullptr,"Capture must begin at the ordinary window target");
    int width=0,height=0;require(SDL_GetCurrentRenderOutputSize(renderer,&width,&height)&&width>0&&height>0,"Cannot read capture dimensions");
    const auto before=view.world().snapshot();const auto camera=view.camera();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    require(bool(target),"Cannot create owned capture target");
    struct Restore {SDL_Renderer* renderer;bool needed=true;~Restore(){if(needed)SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    require(SDL_SetRenderTarget(renderer,target.get()),"Cannot activate capture target");
    require(view.render()&&SDL_GetRenderTarget(renderer)==target.get(),"Production render changed capture target");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    require(bool(raw),"Cannot read completed production capture");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> rgba(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    require(rgba&&rgba->w==width&&rgba->h==height,"Capture dimensions or format differ");
    require(!fs::exists(path)&&SDL_SavePNG(rgba.get(),path.string().c_str()),"Cannot write fresh private capture PNG");
    require(SDL_SetRenderTarget(renderer,nullptr),"Cannot restore window target");restore.needed=false;
    const auto after=view.camera();require(view.world().snapshot()==before&&after.zoom==camera.zoom&&
        after.offset.x==camera.offset.x&&after.offset.y==camera.offset.y&&after.viewport_width==camera.viewport_width&&after.viewport_height==camera.viewport_height,
        "Capture changed World or camera authority");user_takeover();
}
void write_json(const fs::path& path,const Json& value) {
    require(!fs::exists(path),"Preserve existing acceptance output");std::ofstream file(path);file<<value.dump(2)<<'\n';require(bool(file),"Cannot publish private acceptance JSON");
}
const char* domain_name(Domain d) {
    switch(d){case Domain::Building:return "Building";case Domain::Courier:return "Courier";case Domain::Boat:return "Boat";case Domain::Hunter:return "Hunter";case Domain::Animal:return "Animal";}return "Unknown";
}
Domain domain_from(const std::string& value) {
    for(const auto d:{Domain::Building,Domain::Courier,Domain::Boat,Domain::Hunter,Domain::Animal})if(value==domain_name(d))return d;
    throw std::runtime_error("Unknown typed agriculture actor domain");
}
struct Options {
    fs::path data,resources,input,output;
    std::string renderer="software",state="checkpoint";
    Domain domain=Domain::Building;
    std::uint64_t id=0;
    unsigned frames=3000;
    int width=1440,height=1000;
};
constexpr const char* usage="openemperor-agriculture-visual-view-acceptance --data ABS --resources ABS --input EXACT52_SAVE --output FRESH_.local/agri-mac12/SUBDIR --domain Building|Courier|Boat|Hunter|Animal --id ACTUAL_ID [--state LABEL] [--renderer software|metal] [--frames 3000] [--width 1440] [--height 1000]";
Options options(int argc,char** argv) {
    std::map<std::string,std::string> args;
    for(int i=1;i<argc;i+=2){require(i+1<argc,usage);const std::string key=argv[i];require(std::set<std::string>{"--data","--resources","--input","--output","--domain","--id","--state","--renderer","--frames","--width","--height"}.contains(key),usage);require(args.emplace(key,argv[i+1]).second,"Duplicate argument");}
    for(const char* name:{"--data","--resources","--input","--output","--domain","--id"})require(args.contains(name),usage);
    Options o;o.data=args.at("--data");o.resources=args.at("--resources");o.input=args.at("--input");o.output=args.at("--output");o.domain=domain_from(args.at("--domain"));
    const auto integer=[&](const char* name,auto& target){if(!args.contains(name))return;const auto& text=args.at(name);const auto r=std::from_chars(text.data(),text.data()+text.size(),target);require(r.ec==std::errc{}&&r.ptr==text.data()+text.size(),"Invalid numeric argument");};
    integer("--id",o.id);integer("--frames",o.frames);integer("--width",o.width);integer("--height",o.height);
    if(args.contains("--renderer"))o.renderer=args.at("--renderer");if(args.contains("--state"))o.state=args.at("--state");
    require(o.id&&o.frames>=3&&o.frames<=6000&&o.frames%3==0&&o.width>=900&&o.width<=1920&&o.height>=600&&o.height<=1280,"Out-of-range visual request");
    require(o.renderer=="software"||o.renderer=="metal","Unknown renderer");
    for(const auto* pth:{&o.data,&o.resources,&o.input,&o.output})require(pth->is_absolute(),"All paths must be explicit absolute paths");
    no_symlinks(o.data);no_symlinks(o.resources);no_symlinks(o.input);o.data=fs::canonical(o.data);o.resources=fs::canonical(o.resources);o.input=fs::canonical(o.input);
    require(!p::below(o.input,o.data),"Checkpoint must be outside Original files");return o;
}
Json describe(const Diagnostic& d) {
    Json asset=nullptr;if(d.asset)asset={{"archive",d.asset->archive_relative_path.generic_string()},{"image_index",d.asset->image_index}};
    return {{"domain",domain_name(d.domain)},{"id",d.id},{"lifetime_id",d.lifetime_id},{"object",static_cast<unsigned>(d.object)},{"direction",{d.dx,d.dy}},{"harvest_progress",d.harvest_progress},{"harvest_target",d.harvest_target},{"source",static_cast<unsigned>(d.source)},
        {"target",d.target?Json(static_cast<unsigned>(*d.target)):Json(nullptr)},{"kind",d.kind},{"state",d.state},{"art",d.art},
        {"position",{d.position.x,d.position.y}},{"progress",d.progress},{"target_progress",d.target_progress},
        {"cargo",d.cargo},{"wet",d.wet},{"moving",d.moving},{"working",d.working},{"blocked",d.blocked},
        {"sprite_drawn",d.sprite_drawn},{"submitted",d.submitted},{"asset",asset},
        {"screen_ground",{d.screen_ground.x,d.screen_ground.y}},{"image_origin",{d.image_origin.x,d.image_origin.y}},
        {"image_size",{d.image_width,d.image_height}},{"key",{d.key.depth,d.key.ground_x,static_cast<unsigned>(d.key.layer),d.key.stable_id}}};
}
Diagnostic diagnostic(const oe::SandboxView& view,Domain domain,std::uint64_t id) {
    const auto values=view.agriculture_visual_diagnostics();const auto at=std::find_if(values.begin(),values.end(),[&](const auto& d){return d.domain==domain&&d.id==id;});
    require(at!=values.end(),"Actual subject is absent from the production agriculture cache");return *at;
}
void verify_display(const sim::World& world,const Diagnostic& d) {
    const auto position=[&](sim::Position value){require(std::abs(value.x-d.position.x)<1e-9&&std::abs(value.y-d.position.y)<1e-9,"Display position differs from the actual saved physical edge");};
    const auto sign=[](int value){return (value>0)-(value<0);};
    const auto path=[&](const auto& cells,std::size_t vertex,auto edge) {
        require(d.moving==(edge>0),"Stationary/saved-edge movement classification differs");
        if(edge){require(vertex+1<cells.size(),"Drawn movement has no actual begun segment");require(d.dx==sign(cells[vertex+1].x-cells[vertex].x)&&d.dy==sign(cells[vertex+1].y-cells[vertex].y),"Drawn direction differs from the actual saved segment");}
    };
    if(d.domain==Domain::Building) {
        const auto id=static_cast<sim::BuildingId>(d.id);require(world.building(id).placed&&world.building(id).kind==d.object,"Drawn building lost its paid identity");
        for(const auto& f:world.agriculture_fields())if(f.field_id==id) {
            const auto progress=world.agriculture_field_progress(id);
            require(progress>=0&&d.progress==static_cast<unsigned>(progress),"Field artwork used another source or clock");
        }
        for(const auto& f:world.original_production_state()->hemp_farms)if(f.field_id==id)require(d.progress==static_cast<unsigned>(world.building(f.farm_id).progress),"Hemp artwork used another source or clock");
        for(const auto& f:world.plantations_state()->plants)if(f.plant_id==id)require(d.progress==f.growth,"Plant artwork used another plant or clock");
    } else if(d.domain==Domain::Courier) {
        const auto id=static_cast<sim::CourierId>(d.id);const auto& actor=world.courier(id);const auto actual=world.courier_position(id);require(actual.has_value(),"Drawn courier has no physical position");position(*actual);path(actor.path,actor.path_vertex,actor.edge_progress);
        const int cargo=actor.role==sim::CourierRole::OriginalFoodGrower?world.physical_food_grower_cargo(id):actor.role==sim::CourierRole::OriginalSeasonalHempGrower?world.seasonal_hemp_grower_cargo(id):actor.role==sim::CourierRole::OriginalPlantationGrower?world.plantation_grower_cargo(id):world.original_food_cargo(id);
        require(d.cargo==cargo&&d.source==actor.owner,"Drawn courier cargo/source differs from physical authority");
    } else if(d.domain==Domain::Boat) {
        const auto id=sim::FishingBoatId{d.id};const auto& values=world.fishing_state()->boats;const auto actor=std::find_if(values.begin(),values.end(),[&](const auto& a){return a.id==id;});require(actor!=values.end(),"Drawn Boat has no live identity");position(world.fishing_boat_position(id));path(actor->path,actor->path_vertex,actor->edge_progress);require(d.cargo==actor->cargo,"Drawn Boat manufactured cargo");
    } else if(d.domain==Domain::Hunter) {
        const auto id=sim::HuntingHunterId{d.id};const auto& values=world.hunting_state()->hunters;const auto actor=std::find_if(values.begin(),values.end(),[&](const auto& a){return a.id==id;});require(actor!=values.end(),"Drawn Hunter has no live identity");position(world.hunting_hunter_position(id));path(actor->path,actor->path_vertex,actor->edge_progress);require(d.cargo==actor->cargo,"Drawn Hunter manufactured cargo");
    } else {
        const auto id=sim::HuntingAnimalId{d.id};const auto& values=world.hunting_state()->animals;const auto actor=std::find_if(values.begin(),values.end(),[&](const auto& a){return a.id==id;});require(actor!=values.end(),"Drawn animal has no living identity");position(world.hunting_animal_position(id));require(d.moving==(actor->edge_progress>0)&&d.cargo==0,"Roaming animal used an independent animation clock/cargo");if(actor->edge_progress){require(actor->next.has_value()&&d.dx==sign(actor->next->x-actor->current.x)&&d.dy==sign(actor->next->y-actor->current.y),"Animal direction differs from its actual edge");}
    }
}
bool selected(const oe::SandboxView& view,Domain domain,std::uint64_t id) {
    if(domain==Domain::Building)return view.selected_building()==static_cast<sim::BuildingId>(id)&&!view.selected_agriculture_actor();
    const auto actual=view.selected_agriculture_actor();
    return actual&&actual->first==domain&&actual->second==id;
}
struct PixelProbeResult {bool found=false;std::size_t probes=0;};
PixelProbeResult scan_submitted_pixels(oe::SandboxView& view,SDL_Renderer* renderer,const Diagnostic& d) {
    require(d.submitted&&d.image_width>0&&d.image_height>0,"Actual subject has no submitted image bounds for normal picking");
    const auto map=view.layout().map;
    const double left=std::max(d.image_origin.x,double(map.x)),top=std::max(d.image_origin.y,double(map.y));
    const double right=std::min(d.image_origin.x+d.image_width,double(map.x+map.w));
    const double bottom=std::min(d.image_origin.y+d.image_height,double(map.y+map.h));
    PixelProbeResult result;if(left>=right||top>=bottom)return result;
    const int x0=static_cast<int>(std::ceil(left-.5)),y0=static_cast<int>(std::ceil(top-.5));
    const int x1=static_cast<int>(std::ceil(right-.5)),y1=static_cast<int>(std::ceil(bottom-.5));
    require(x1>=x0&&y1>=y0&&static_cast<std::uint64_t>(x1-x0)*static_cast<std::uint64_t>(y1-y0)<=1920U*1280U,
        "Submitted pixel scan exceeds the bounded acceptance viewport");
    // These are ordinary clicks, including alpha and roof rejection. A narrow
    // visible edge can lie between the initial proportional probes.
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
        const oe::scene::Point point{double(x)+.5,double(y)+.5};
        if(!map.contains(point.x,point.y))continue;
        click(view,renderer,point);++result.probes;
        if(selected(view,d.domain,d.id)){result.found=true;return result;}
    }
    return result;
}
[[noreturn]] void selection_refused(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& output,
    const Diagnostic& d,std::size_t sparse_probes,std::size_t dense_probes) {
    const auto suffix=std::to_string(static_cast<unsigned>(view.camera().zoom))+"x";
    capture(view,renderer,output/("selection-refused-"+suffix+".png"));
    Json submitted=Json::array();for(const auto& row:view.agriculture_visual_diagnostics())if(row.submitted)submitted.push_back(describe(row));
    write_json(output/("selection-refused-"+suffix+".json"),{{"subject",describe(d)},
        {"sparse_probes",sparse_probes},{"submitted_rectangle_probes",dense_probes},{"actual_submitted",submitted},
        {"normal_pixel_path_only",true},{"no_occlusion_override",true}});
    throw std::runtime_error("No visible subject pixel selected the actual typed identity; inspect selection-refused capture and its real roof/alpha overlap");
}
Json select_subject(oe::SandboxView& view,SDL_Renderer* renderer,Domain domain,std::uint64_t id,std::uint32_t border,const fs::path& output) {
    view.set_tool(5);require(view.render(),SDL_GetError());auto d=diagnostic(view,domain,id);
    bool found=false;std::size_t probes=0,dense_probes=0;
    if(domain==Domain::Building)found=select_building_sparse(view,renderer,static_cast<sim::BuildingId>(id),border);
    else {
        const auto world=view.camera().screen_to_world(d.screen_ground);p::center(view,world);require(view.render(),SDL_GetError());d=diagnostic(view,domain,id);
        require(d.submitted&&d.image_width>0&&d.image_height>0,"Actual actor was not submitted after ordinary camera pan");
        // Bounded diagnostic pixel probes use the actual submitted rectangle.
        // Normal select_visual decides alpha/roof occlusion for every click.
        for(const double fy:{0.8,0.6,0.4,0.2,0.95,0.5,0.7,0.3}) {
            for(const double fx:{0.5,0.35,0.65,0.2,0.8,0.1,0.9}) {
                const oe::scene::Point point{d.image_origin.x+fx*d.image_width,d.image_origin.y+fy*d.image_height};
                if(!view.layout().map.contains(point.x,point.y))continue;
                click(view,renderer,point);++probes;if(selected(view,domain,id)){found=true;break;}
            }
            if(found)break;
        }
    }
    if(!found) {
        d=diagnostic(view,domain,id);const auto scan=scan_submitted_pixels(view,renderer,d);found=scan.found;dense_probes=scan.probes;
    }
    if(!found)selection_refused(view,renderer,output,diagnostic(view,domain,id),probes,dense_probes);
    require(view.render(),SDL_GetError());require(selected(view,domain,id),"Normal pixel selection changed its actual typed identity");
    Json result={{"status","PASS"},{"probes",probes},{"submitted_rectangle_probes",dense_probes},{"typed_domain",domain_name(domain)},{"typed_id",id}};
    if(domain==Domain::Building){result["actual_id"]=static_cast<unsigned>(*view.selected_building());result["building_has_no_selected_actor"]=!view.selected_agriculture_actor();}
    else {const auto actual=view.selected_agriculture_actor();require(actual.has_value(),"Typed selection disappeared after its submitted pixel hit");result["actual_domain"]=domain_name(actual->first);result["actual_id"]=actual->second;}
    return result;
}
Json stats(const oe::SandboxView& view) {
    const auto d=view.agriculture_display_stats();const auto walkers=view.walker_display_stats();
    require(d.active&&d.cached_entities<=82&&d.cached_actors<=58,"Presentation cache exceeded actual body/channel bounds");
    require(walkers.frame_aliases<=walkers.frame_alias_limit&&walkers.decoded_assets<=oe::assets::walker_max_unique_assets&&walkers.logical_rgba_bytes<=oe::assets::walker_max_rgba_bytes,"Walker assets exceeded the historical admission budget");
    return {{"active",d.active},{"cached_entities",d.cached_entities},{"cached_actors",d.cached_actors},{"prepared_meshes",d.prepared_meshes},{"cpu_mesh_bytes",d.cpu_mesh_bytes},
        {"prepare_count",d.prepare_count},{"prepared_tick",d.prepared_tick},{"prepared_sequence",d.prepared_sequence},
        {"building_textures",view.building_texture_count()},{"walker_textures",view.walker_texture_count()},{"walker_aliases",walkers.frame_aliases},{"walker_assets",walkers.decoded_assets},{"walker_rgba_bytes",walkers.logical_rgba_bytes}};
}
} // namespace
int main(int argc,char** argv) {
    std::unique_ptr<oe::SandboxView> view;SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;fs::path output;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto o=options(argc,argv);InputGuard input(o.input);const auto document=save::read_save(o.input);
        require(document.source_schema_version==52&&document.world.rule_version==36&&document.world.agriculture_reconfiguration&&document.world.processed_food&&document.world.original_economy&&document.world.original_economy->model_source,"Only actual Rule36/schema52 checkpoints are accepted");
        const SourceGuard source(o.data,document.map_relative);output=p::output_root(o.output,o.data,".local/agri-mac12");watch_native=o.renderer=="metal";
#if defined(__APPLE__)
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,watch_native?"cocoa":"dummy"),SDL_GetError());
#else
        require(!watch_native,"Metal acceptance requires native macOS");require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),SDL_GetError());
#endif
        require(SDL_SetHint(SDL_HINT_RENDER_DRIVER,o.renderer.c_str())&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("AGRI-MAC12 actual paid agriculture visuals",o.width,o.height,watch_native?SDL_WINDOW_RESIZABLE:SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        require(SDL_GetRendererName(renderer)&&std::string(SDL_GetRendererName(renderer))==o.renderer,"Requested renderer was not selected");user_takeover();
        auto session=oe::maps::load_stored_map_session(o.data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);const auto border=session.plan.border;
        view=oe::SandboxView::create_agriculture_reconfiguration(std::move(session),document.world.original_economy->model_source->difficulty,false);
        view->configure_save(o.data,document.map_relative,o.input);p::configure(*view,oe::detect_and_select_visual_profiles(o.data,o.resources));view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);key(*view,SDLK_F9);require(view->paused()&&view->world().snapshot()==document.world,"Normal F9 changed actual paid state");
        require(view->render(),SDL_GetError());const auto first_pose=describe(diagnostic(*view,o.domain,o.id));const auto manual=output/"saves/manual.save.json";view->configure_save(o.data,document.map_relative,manual);
        const auto saved=view->save_generation();key(*view,SDLK_F5);require(view->save_generation()==saved+1&&save::read_save(manual).world==document.world,"Normal F5 changed paid authority");
        key(*view,SDLK_PERIOD);const auto step=view->world().snapshot();require(step.ticks==document.world.ticks+1,"Single Step advanced another clock");key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"F9 changed exact paid checkpoint");
        require(view->render(),SDL_GetError());require(describe(diagnostic(*view,o.domain,o.id))==first_pose,"Cold F9 changed saved actor/field display state");key(*view,SDLK_PERIOD);require(view->world().snapshot()==step,"Post-F9 continuation diverged");key(*view,SDLK_F9);
        Json captures=Json::array();for(unsigned zoom=0;zoom<3;++zoom) {
            for(unsigned tries=0;tries<3&&view->camera().zoom!=double(1U<<zoom);++tries)key(*view,SDLK_Z);
            require(view->camera().zoom==double(1U<<zoom),"Normal Z cycle missed requested zoom");
            const auto picked=select_subject(*view,renderer,o.domain,o.id,border,output);const auto d=diagnostic(*view,o.domain,o.id);verify_display(view->world(),d);require(d.submitted&&!d.art.empty(),"Submitted agriculture subject hides its art provenance");
            const auto resource=stats(*view);const auto purity=pure_frames(*view,renderer,o.frames/3);const auto file="subject-"+std::to_string(1U<<zoom)+"x.png";capture(*view,renderer,output/file);
            Json overlaps=Json::array();for(const auto& other:view->agriculture_visual_diagnostics())if(other.submitted&&(other.domain!=d.domain||other.id!=d.id)) {
                const bool overlap=other.image_origin.x<d.image_origin.x+d.image_width&&d.image_origin.x<other.image_origin.x+other.image_width&&other.image_origin.y<d.image_origin.y+d.image_height&&d.image_origin.y<other.image_origin.y+other.image_height;
                if(overlap)overlaps.push_back({{"other",describe(other)},{"other_before_subject",other.key<d.key}});
            }
            captures.push_back({{"zoom",1U<<zoom},{"path",file},{"subject",describe(d)},{"pixel_pick",picked},{"purity",purity},{"resources",resource},{"actual_painter_overlaps",overlaps},{"inspector",view->inspection_lines()}});
        }
        input.unchanged();source.unchanged();require(view->world().snapshot()==document.world,"Drawing/picking/inspection changed the paid World");
        write_json(output/"report.json",{{"format","openemperor-agriculture-visual-view-v1"},{"status","PASS"},{"rule",36},{"schema",52},{"tier",watch_native?"NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},{"renderer",o.renderer},{"video_driver",SDL_GetCurrentVideoDriver()},
            {"input",o.input.string()},{"requested_state",o.state},{"domain",domain_name(o.domain)},{"id",o.id},{"tick",document.world.ticks},{"input_preserved",true},{"private_f5_f9_exact",true},{"step_reload_replay_exact",true},{"cold_f9_pose_exact",true},{"pure_frames",o.frames},{"captures",captures},{"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"original_runtime_parity","NOT_CLAIMED"}});
        view->shutdown();view.reset();SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();std::cout<<"PASS actual paid agriculture view: "<<output/"report.json"<<'\n';return 0;
    } catch(const std::exception& e) {
        perf::set_enabled(false);if(view)view->shutdown();view.reset();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        if(!output.empty()&&!fs::exists(output/"report.json"))try{write_json(output/"report.json",{{"format","openemperor-agriculture-visual-view-v1"},{"status","FAIL"},{"reason",e.what()},{"human_acceptance","NOT_RUN"}});}catch(...){}
        std::cerr<<"Agriculture visual view FAIL: "<<e.what()<<'\n';return 1;
    }
}
