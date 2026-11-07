// Scripted software-SDL acceptance of the shipped optional Original menu path.
// Inputs remain read-only. Only normal SDL events and MenuSession::update drive
// gameplay; this tool never calls World commands, fixture factories or tick_once.
#include "app/MenuSession.h"
#include "maps/MapGeometry.h"
#include "persistence/RecoveryStore.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Menu=oe::menu::MenuSession;
using Point=oe::scene::Point;
constexpr std::uint64_t continuation_ticks=160;
constexpr std::string_view usage=
    "Usage: openemperor-original-starter-menu-acceptance --create|--resume DATA RESOURCE_ROOT SAVE_DIR OUTPUT_DIR\n";

void require(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
bool contains(const fs::path& root,const fs::path& path) {
    return std::mismatch(root.begin(),root.end(),path.begin(),path.end()).first==root.end();
}
void no_symlinks(const fs::path& path) {
    fs::path prefix;
    for (const auto& component:fs::absolute(path).lexically_normal()) {
        prefix/=component;std::error_code error;const auto status=fs::symlink_status(prefix,error);
        require(!error || error==std::errc::no_such_file_or_directory,
            "Cannot inspect acceptance path: "+prefix.string());
        require(!fs::is_symlink(status),"Acceptance paths must not contain symlinks: "+prefix.string());
    }
}
void output_paths(const fs::path& data,const fs::path& resources,const fs::path& app,const fs::path& out) {
    no_symlinks(app);no_symlinks(out);
    const auto source_tree=fs::canonical(fs::absolute(__FILE__)).parent_path().parent_path();
    const auto save_root=fs::weakly_canonical(app),output_root=fs::weakly_canonical(out);
    bool allowed=false;
    for (const char* task:{"oe30","oe31"}) {
        const auto directory=fs::weakly_canonical(source_tree/".local/original-economy"/task);
        allowed|=contains(directory,save_root) && save_root!=directory &&
            contains(directory,output_root) && output_root!=directory;
    }
    require(allowed,"Acceptance outputs must be below one repository OE30/OE31 private task directory");
    require(!contains(save_root,output_root) && !contains(output_root,save_root),
        "Acceptance SAVE_DIR and OUTPUT_DIR must be separate disjoint directories");
    for (const auto& input:{fs::canonical(data),fs::canonical(resources)})
        for (const auto& target:{save_root,output_root}) require(!contains(input,target) && !contains(target,input),
            "Acceptance outputs and read-only inputs must be disjoint in both directions");
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read evidence: "+path.string());
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void json_file(const fs::path& path,const Json& value) {
    no_symlinks(path);require(!fs::exists(path),"Acceptance report already exists: "+path.string());
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    output<<value.dump(2)<<'\n';
    require(bool(output),"Cannot write acceptance report: "+path.string());
}
void same_document(const oe::persistence::SaveDocument& expected,
                   const oe::persistence::SaveDocument& actual) {
    require(expected.map_relative==actual.map_relative && expected.map_sha256==actual.map_sha256 &&
        expected.buildable_sha256==actual.buildable_sha256 &&
        expected.map_permissions_policy_version==actual.map_permissions_policy_version &&
        expected.map_permissions_sha256==actual.map_permissions_sha256 && expected.world==actual.world,
        "Actual menu session differs from the complete authoritative SaveDocument");
}
class FileDialog final:public oe::menu::DialogAdapter {
public:
    void open_folder(SDL_Window*,Callback callback) override { pending_=std::move(callback); }
    void open_file(SDL_Window*,Callback callback) override { pending_=std::move(callback); }
    void cancel_pending() override { pending_={}; }
    void select(const fs::path& path) {
        require(bool(pending_),"Actual menu did not request a file chooser");
        auto callback=std::move(pending_);pending_={};
        callback({oe::menu::DialogResult::Kind::Selected,path.string()});
    }
private:
    Callback pending_;
};

struct Harness {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    std::unique_ptr<Menu> menu;
    FileDialog* dialog=nullptr;
    fs::path data,resources,app,out;
    unsigned border=0;
    std::uint64_t event_count=0,update_count=0;
    Json captures=Json::array();
    Json last_click=Json::array();

    Harness(fs::path data_path,fs::path resource_path,fs::path app_path,fs::path output)
        :data(fs::canonical(data_path)),resources(fs::canonical(resource_path)),
         app(fs::absolute(app_path)),out(fs::absolute(output)) {
        require(!fs::equivalent(data,resources),"Resource metadata must be separate from original data");
        fs::create_directories(out);
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        window=SDL_CreateWindow("OpenEmperor Original menu acceptance",1280,800,SDL_WINDOW_HIDDEN);
        require(window!=nullptr,SDL_GetError());
        renderer=SDL_CreateRenderer(window,"software");require(renderer!=nullptr,SDL_GetError());
        require(std::string_view(SDL_GetRendererName(renderer))=="software","Acceptance requires the software renderer");
        const auto catalog=oe::menu::validate_data_root(data);
        const auto found=std::find_if(catalog.entries.begin(),catalog.entries.end(),[](const auto& entry){
            return entry.relative_path==fs::path("Cities/Xia.map");
        });
        require(found!=catalog.entries.end() && found->declared_size.has_value(),"Real Cities/Xia.map is unavailable");
        const oe::maps::MapGeometry geometry(*found->declared_size);
        require(geometry.supported,"Xia has unsupported geometry");border=geometry.border;
        auto adapter=std::make_unique<FileDialog>();dialog=adapter.get();
        menu=std::make_unique<Menu>(data,app,std::move(adapter),resources);
        menu->initialize(window,renderer);
        require(menu->state()==Menu::State::MainMenu,"Actual menu failed data setup: "+menu->message());
    }
    ~Harness() {
        if (menu) {menu->shutdown();menu.reset();}
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
    oe::SandboxView& view() {
        require(menu->state()==Menu::State::Playing && menu->sandbox(),"Actual menu is not Playing: "+menu->message());
        return *menu->sandbox();
    }
    void event(SDL_Event value) {
        require(SDL_PushEvent(&value),"Cannot queue actual SDL event");++event_count;
        while (SDL_PollEvent(&value)) menu->handle_event(value);
        require(menu->running(),"Scripted input unexpectedly closed the application");
    }
    void key(SDL_Keycode code) {
        SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(e);
        e={};e.type=SDL_EVENT_KEY_UP;e.key.key=code;event(e);menu->advance();
    }
    Point window_point(Point p) {
        float x=0,y=0;
        require(SDL_RenderCoordinatesToWindow(renderer,float(p.x),float(p.y),&x,&y),
            "Actual render-to-window transform failed");return {x,y};
    }
    Json input(Point raw) {
        const auto d=view().input_diagnostic_state(raw);
        Json result{{"ui",d.ui},{"map_pressed",d.map_pressed},{"ui_pressed",d.ui_pressed},
            {"road_drag",d.road_drag},{"input_focused",d.input_focused},{"tool",view().tool()},
            {"budget_warning",view().budget_warning_pending()},{"message",view().last_message()},
            {"road_plan_valid",view().road_preview().valid},{"road_plan_reason",view().road_preview().reason}};
        if (d.render_position) result["render_position"]={d.render_position->x,d.render_position->y};
        if (d.ground_cell) result["ground_cell"]={d.ground_cell->x,d.ground_cell->y};
        if (d.ui_action) result["ui_action"]=static_cast<unsigned>(*d.ui_action);
        return result;
    }
    void click(Point p) {
        const auto raw=window_point(p);
        last_click=Json::array();
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=float(raw.x);e.motion.y=float(raw.y);event(e);
        menu->update(0);++update_count;
        if (menu->state()==Menu::State::Playing) last_click.push_back({{"phase","motion"},{"input",input(raw)}});
        e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
        e.button.x=float(raw.x);e.button.y=float(raw.y);event(e);
        if (menu->state()==Menu::State::Playing) last_click.push_back({{"phase","press"},{"input",input(raw)}});
        e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(e);menu->advance();menu->update(0);++update_count;
        if (menu->state()==Menu::State::Playing) last_click.push_back({{"phase","release"},{"input",input(raw)}});
    }
    void menu_click(const std::string& label) {
        if (menu->state()==Menu::State::MainMenu &&
            !menu->diagnostic_button_rect(label) &&
            menu->diagnostic_button_rect("Advanced / Legacy"))
            menu_click("Advanced / Legacy");
        require(menu->render(),"Cannot render actual menu");
        const auto rect=menu->diagnostic_button_rect(label);
        require(rect.has_value(),"Missing actual menu control: "+label);
        const double scale=1.5*std::max(1.0F,SDL_GetWindowPixelDensity(window));
        const Point p{(rect->x+rect->w/2)*scale,(rect->y+rect->h/2)*scale};
        const auto raw=window_point(p);const auto hit=menu->diagnostic_button_hit(float(raw.x),float(raw.y));
        require(hit && hit->enabled,"Actual menu control is disabled: "+label);click(p);
    }
    void ui(oe::sandbox_ui::Action action) {
        require(menu->render(),"Cannot render actual BuildUI");
        const auto& buttons=view().layout().buttons;
        const auto found=std::find_if(buttons.begin(),buttons.end(),[&](const auto& b){return b.action==action;});
        require(found!=buttons.end() && found->enabled,"Required actual BuildUI control is unavailable");
        click({found->rect.x+found->rect.w*.5,found->rect.y+found->rect.h*.5});
    }
    void focus() {SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(e);}
    void paused(bool wanted) {
        if (view().paused()!=wanted) key(SDLK_SPACE);
        require(view().paused()==wanted,"Space did not change the actual clock pause state");
    }
    void ticks(std::uint64_t count) {
        ui(oe::sandbox_ui::Action::Speed1);paused(false);
        const auto target=view().world().ticks()+count;
        while (view().world().ticks()<target) {
            const auto remaining=target-view().world().ticks();
            const auto before=view().world().ticks();
            menu->update(double(std::min<std::uint64_t>(remaining,5))/sim::Rules::ticks_per_second);++update_count;
            require(view().world().ticks()>before && view().world().ticks()<=target,"Real TickDriver made unexpected progress");
        }
        paused(true);
    }
    Json summary() {
        const auto& world=view().world();Json houses=Json::array(),moving=Json::array();
        require(world.original_economy_state() && world.original_production_state(),"Original state is absent");
        for (const auto& h:world.original_economy_state()->houses) houses.push_back({
            {"building_id",static_cast<unsigned>(h.building_id)},{"type",static_cast<unsigned>(h.house.type)},
            {"occupants",h.house.occupants},{"water",h.house.water},{"ancestor",h.house.ancestor},
            {"food_quality",h.house.food_quality},{"hemp",h.house.hemp},{"ceramics",h.house.ceramics}});
        for (const auto& c:world.couriers()) if (c.phase==sim::CourierPhase::ToWarehouse || c.phase==sim::CourierPhase::Returning)
            moving.push_back({{"id",static_cast<unsigned>(c.id)},{"role",static_cast<unsigned>(c.role)},
                {"phase",static_cast<unsigned>(c.phase)},{"cargo",c.cargo},{"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress}});
        const auto& p=*world.original_production_state();
        return {{"ticks",world.ticks()},{"command_sequence",world.command_sequence()},
            {"funds",world.treasury()},{"construction_spent",world.construction_spent_total()},
            {"taxes",world.taxes_collected_total()},{"maintenance",world.maintenance_spent_total()},
            {"population",world.total_population()},{"pottery_handed_off",p.pottery_handed_off},
            {"residents_admitted",p.residents_admitted},{"houses",houses},{"moving_couriers",moving},
            {"service_witnesses",p.services.witnesses.size()},{"paused",view().paused()}};
    }
    void capture(const std::string& name) {
        require(menu->render(),"Actual frame failed: "+name);
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> image(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
        require(image && image->w==1280 && image->h==800,"Actual capture is not1280x800");
        const auto destination=out/(name+".png");no_symlinks(destination);
        require(!fs::exists(destination),"Acceptance capture already exists: "+destination.string());
        require(SDL_SavePNG(image.get(),destination.string().c_str()),"Cannot write actual SDL capture");
        Json row{{"name",name},{"width",image->w},{"height",image->h}};
        if (menu->state()==Menu::State::Playing) {row["zoom"]=view().camera().zoom;row["observation"]=summary();}
        captures.push_back(std::move(row));
    }
    std::optional<Point> ground(sim::Cell cell) {
        const auto& v=view();const auto& world=v.world();
        const double h=world.map_permissions() ? world.map_permissions()->cell_height(cell):0;
        const Point base{(cell.x-cell.y)*40.0,(cell.x+cell.y-2.0*border)*20+20-h*40};
        for (const int dy:{20,12,28,4,-4,36,-12}) for (const int dx:{0,-8,8}) {
            const auto point=v.camera().world_to_screen({base.x+dx,base.y+dy});
            if (!v.layout().map.contains(point.x,point.y)) continue;
            const auto d=v.input_diagnostic_state(window_point(point));
            if (!d.ui && d.ground_cell && *d.ground_cell==cell) return point;
        }
        return std::nullopt;
    }
    void zoom(double wanted,Point anchor) {
        const auto raw=window_point(anchor);SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;
        e.wheel.mouse_x=float(raw.x);e.wheel.mouse_y=float(raw.y);
        e.wheel.y=float(std::log(wanted/view().camera().zoom)/std::log(1.15));event(e);menu->update(0);++update_count;
        require(std::abs(view().camera().zoom-wanted)<1e-5,"Actual wheel did not reach the requested zoom");
    }
    void zoom_captures(const std::string& name) {
        const auto map=view().layout().map;Point anchor{map.x+map.w*.5,map.y+map.h*.5};
        // Keep a real visible House at the wheel anchor; the wheel is the normal
        // camera controller, with no camera setter or held-state test adapter.
        double best=1e30;
        for (const auto& b:view().world().buildings()) if (b.placed && sim::original_house_kind(b.kind)) {
            const auto cells=sim::building_footprint_cells(view().world().profile(),view().world().rule_version(),b.kind,b.cell);
            for (const auto cell:cells) if (const auto p=ground(cell)) {
                const double d=std::hypot(p->x-anchor.x,p->y-anchor.y);
                if (d<best) {best=d;anchor=*p;}
            }
        }
        for (const int z:{1,2,4}) {zoom(z,anchor);capture(name+".zoom"+std::to_string(z));}
        zoom(1,anchor);
    }
    void check_recovery_controls() {
        require(menu->settings().profile==sim::RulesProfile::CityV11 && menu->settings().prepared_starter &&
            menu->settings().autosave_enabled,"Original entry changed legacy settings defaults");
        require(menu->message().find("Autosave ON")!=std::string::npos,
            "Original recovery setting is not visible");
        const auto catalog=oe::persistence::RecoveryStore(app,data,
            oe::persistence::recovery_storage_budget_bytes,
            oe::persistence::RecoveryAccess::OriginalProduction).catalog();
        const auto current=std::find_if(catalog.histories.begin(),catalog.histories.end(),[&](const auto& h){
            return h.history_id==menu->recovery_history_id();
        });
        require(current!=catalog.histories.end() && current->error.empty() && !current->entries.empty(),
            "Original mode did not publish its valid active recovery history");
    }
    void finish_loading() {
        require(menu->state()==Menu::State::Loading,"Actual menu did not defer loading");
        require(menu->render(),"Cannot render Loading screen");menu->advance();
        (void)view();focus();check_recovery_controls();
        require(view().world().rule_version()==10 && view().world().original_production_enabled(),
            "Actual optional menu entry did not select active Rule10");
    }
    void road_actions() {
        paused(true);ui(oe::sandbox_ui::Action::Road);
        require(view().tool()==1,"Actual Road toolbar press did not select the Road tool");
        std::optional<sim::Cell> selected;Point point{};
        const auto& world=view().world();
        for (int y=0;y<world.height() && !selected;++y) for (int x=0;x<world.width();++x) {
            const sim::Cell c{x,y};
            if (world.object_at(c)!=sim::Object::Empty || world.road_cell(c) || world.fixed_passage(c)) continue;
            const auto purchase=world.validate({sim::CommandType::PlaceRoad,c});
            if (!purchase.accepted || !purchase.changed) continue;
            bool next_to_road=false;
            for (const sim::Cell d:std::array<sim::Cell,4>{{{1,0},{-1,0},{0,1},{0,-1}}})
                if (world.object_at({x+d.x,y+d.y})==sim::Object::Road) next_to_road=true;
            if (next_to_road) if (const auto p=ground(c)) {selected=c;point=*p;break;}
        }
        require(selected.has_value(),"No visible legal empty spur cell for actual BuildUI road exercise");
        const auto before=summary();const auto sequence=world.command_sequence();
        const auto funds=world.treasury();const auto spent=world.construction_spent_total();click(point);
        json_file(out/"road.purchase-observed.json",{{"cell",{selected->x,selected->y}},
            {"render_point",{point.x,point.y}},{"before",before},{"after",summary()},
            {"actual_object",static_cast<unsigned>(world.object_at(*selected))},{"click",last_click}});
        require(world.object_at(*selected)==sim::Object::Road && world.command_sequence()==sequence+1 &&
            world.construction_spent_total()>spent &&
            funds-world.treasury()==static_cast<std::int64_t>(world.construction_spent_total()-spent),
            "Actual Road UI did not perform one normal paid purchase: "+view().last_message()+
            "; see road.purchase-observed.json for actual pointer/press/release and budget state");
        capture("road.purchased");ui(oe::sandbox_ui::Action::RemoveRoad);click(point);
        require(world.object_at(*selected)==sim::Object::Empty && world.command_sequence()==sequence+2,
            "Actual RemoveRoad UI did not remove its safe paused spur");
        capture("road.removed");ui(oe::sandbox_ui::Action::Road);click(point);
        require(world.object_at(*selected)==sim::Object::Road && world.command_sequence()==sequence+3,
            "Actual Road UI did not repurchase its removed road");
        capture("road.repaired");ui(oe::sandbox_ui::Action::Select);
        json_file(out/"road-actions.json",{{"cell",{selected->x,selected->y}},
            {"before",before},{"after",summary()},{"scope","visible additional spur; live transport disconnection tested separately"}});
    }
};

void original_start(Harness& h) {
    h.capture("menu.main");h.menu_click("O Original economy");
    require(h.menu->state()==Menu::State::NewSandbox && h.menu->original_setup() &&
        h.menu->original_paid_starter() && h.menu->original_difficulty()==oe::original::Difficulty::Normal,
        "Actual optional menu setup is not Normal with the paid four-house plan");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while (h.menu->map_rules_result().status==oe::maps::MapRulesStatus::Checking &&
           std::chrono::steady_clock::now()<deadline) {h.menu->advance();std::this_thread::yield();}
    require(h.menu->map_rules_can_start(),"Real Xia preflight refused: "+h.menu->map_rules_result().reason);
    const auto& request=h.menu->map_rules_result().request;
    require(request.map_relative==fs::path("Cities/Xia.map") && request.rule_version==10 && request.policy_version==3,
        "Actual Original setup did not preflight Xia/Rule10/Policy3");
    h.capture("menu.original-setup");h.menu_click("Start sandbox");h.finish_loading();
    require(h.view().paused() && h.view().world().ticks()==0,"Actual paid starter did not begin paused atTick0");
    const auto& houses=h.view().world().original_economy_state()->houses;
    require(houses.size()==4 && std::all_of(houses.begin(),houses.end(),[](const auto& row){
        return row.house.type==oe::original::HouseType::Vacant && row.house.occupants==0 &&
            row.house.hemp==0 && row.house.ceramics==0;
    }),"Actual paid starter granted residents, house stages or goods");
    require(h.view().world().construction_spent_total()>0 &&
        h.view().world().treasury()==sim::original_production_starting_treasury-
            static_cast<std::int64_t>(h.view().world().construction_spent_total()),
        "Actual paid starter did not debit the unchanged starting funds");
    h.zoom_captures("starter.tick0");h.road_actions();
}

void run_to_upgrade(Harness& h) {
    h.ui(oe::sandbox_ui::Action::Speed4);h.paused(false);
    bool moving_capture=false,upgraded=false;
    while (h.view().world().ticks()<12000) {
        const auto before=h.view().world().ticks();h.menu->update(.1);++h.update_count;
        require(h.view().world().ticks()>before && h.view().world().ticks()-before<=8,
            "Ordinary real TickDriver failed its bounded update");
        if (!moving_capture && std::any_of(h.view().world().couriers().begin(),h.view().world().couriers().end(),
            [](const auto& c){return c.phase==sim::CourierPhase::ToWarehouse && c.cargo>0;})) {
            h.capture("starter.actual-moving-courier");moving_capture=true;
        }
        const auto& houses=h.view().world().original_economy_state()->houses;
        upgraded=houses.size()==4 && std::all_of(houses.begin(),houses.end(),[](const auto& row){
            return static_cast<unsigned>(row.house.type)>=5 && row.house.occupants>0;
        });
        if (moving_capture && upgraded) break;
    }
    h.paused(true);require(moving_capture && upgraded,"Actual paid Xia starter did not reach four occupied PlainCottages and cargo transport within12000ticks");
    h.zoom_captures("starter.upgraded");
}

Json common_report(Harness& h,const char* mode) {
    return {{"mode",mode},{"authority","production MenuSession; actual queued SDL input; ordinary TickDriver update"},
        {"data",h.data.generic_string()},{"resource_root",h.resources.generic_string()},
        {"app_root",h.app.generic_string()},{"rule",10},{"schema",26},{"policy",3},
        {"development_revision",27},{"map","Cities/Xia.map"},{"renderer",SDL_GetRendererName(h.renderer)},
        {"native_input",false},{"human_acceptance","NOT_RUN"},{"original_scene_parity","PARTIAL"},
        {"captures",h.captures},{"sdl_events",h.event_count},{"ordinary_updates",h.update_count},
        {"final",h.summary()}};
}
void create(const fs::path& data,const fs::path& resources,const fs::path& app,const fs::path& out) {
    output_paths(data,resources,app,out);
    require(!fs::exists(app) || fs::is_empty(app),"--create requires a fresh empty SAVE_DIR");
    require(!fs::exists(out) || fs::is_empty(out),"--create requires a fresh empty OUTPUT_DIR");
    oe::menu::Settings settings;settings.data_root=fs::canonical(data);settings.last_map="Cities/Xia.map";
    oe::menu::write_settings(app,settings);
    Harness h(data,resources,app,out);original_start(h);run_to_upgrade(h);
    h.key(SDLK_F5);const auto save=h.menu->settings().last_save;
    require(!save.empty() && fs::is_regular_file(save),"Actual F5 did not publish a manual save");
    const auto literal=bytes(save);const auto saved=oe::persistence::read_save(save);
    same_document(saved,h.view().capture_save_document());
    h.ticks(continuation_ticks);const auto future=h.view().capture_save_document();
    oe::persistence::write_save(h.out/"reference-continuation.save.json",future,h.data,h.view().buildable_mask());
    h.key(SDLK_F9);same_document(saved,h.view().capture_save_document());
    require(h.view().paused() && bytes(save)==literal,"Actual F9 changed its source save or did not pause");
    h.capture("starter.f9-restored");h.check_recovery_controls();
    auto report=common_report(h,"create");report["save"]=save.generic_string();
    report["f5_full_document_equal"]=true;report["f9_full_document_equal"]=true;
    report["manual_save_bytes_unchanged"]=true;report["reference_continuation_ticks"]=continuation_ticks;
    json_file(h.out/"create.json",report);
    std::cout<<"Actual paid Xia menu starter, four PlainCottages, cargo, Road UI and F5/F9 PASS; save "<<save<<'\n';
}
void resume(const fs::path& data,const fs::path& resources,const fs::path& app,const fs::path& out) {
    output_paths(data,resources,app,out);no_symlinks(out/"create.json");
    require(!fs::exists(out/"resume.json"),"Resume acceptance already completed in this OUTPUT_DIR");
    const auto prior=Json::parse(bytes(out/"create.json"));
    require(prior.at("mode")=="create" && prior.at("data")==fs::canonical(data).generic_string() &&
        prior.at("resource_root")==fs::canonical(resources).generic_string() &&
        prior.at("app_root")==fs::absolute(app).generic_string() &&
        prior.at("f5_full_document_equal")==true && prior.at("f9_full_document_equal")==true,
        "Resume requires the matching completed create acceptance report");
    const auto settings=oe::menu::read_settings(app);require(!settings.needs_reset,"Restart settings are invalid");
    const auto save=settings.value.last_save;no_symlinks(save);
    require(contains(fs::canonical(app),fs::canonical(save)) && prior.at("save")==save.generic_string(),
        "Resume must load the prior create process's manual save below SAVE_DIR");
    const auto literal=bytes(save);
    const auto saved=oe::persistence::read_save(save);
    const auto future=oe::persistence::read_save(fs::absolute(out)/"reference-continuation.save.json");
    Harness h(data,resources,app,out);h.menu_click("L Load Save");
    h.menu_click("Open save file...");h.dialog->select(save);h.menu->advance();h.finish_loading();
    require(h.view().paused(),"New process manual load did not pause");
    same_document(saved,h.view().capture_save_document());h.zoom_captures("restart.restored");
    h.ticks(continuation_ticks);same_document(future,h.view().capture_save_document());
    h.capture("restart.exact-continuation");h.key(SDLK_F9);
    same_document(saved,h.view().capture_save_document());require(bytes(save)==literal,"Restart/F9 modified supplied save bytes");
    h.check_recovery_controls();auto report=common_report(h,"resume");
    report["save"]=save.generic_string();report["new_process_full_document_equal"]=true;
    report["continuation_full_document_equal"]=true;report["f9_full_document_equal"]=true;
    report["manual_save_bytes_unchanged"]=true;report["reference_continuation_ticks"]=continuation_ticks;
    json_file(h.out/"resume.json",report);
    std::cout<<"Actual menu new-process manual restore, complete160tick continuation and F9 PASS\n";
}
} // namespace

int main(int argc,char** argv) {
    try {
        if (argc==2 && std::string_view(argv[1])=="--help") {std::cout<<usage;return 0;}
        require(argc==6,std::string(usage));const std::string_view mode=argv[1];
        require(mode=="--create" || mode=="--resume",std::string(usage));
        if (mode=="--create") create(argv[2],argv[3],fs::absolute(argv[4]),fs::absolute(argv[5]));
        else resume(argv[2],argv[3],fs::absolute(argv[4]),fs::absolute(argv[5]));
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
