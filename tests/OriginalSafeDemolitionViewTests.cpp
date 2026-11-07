// Paid authored map/model and real production handlers. Scripted SDL only.
#include "EconomyStartViewFixture.h"
#include "OriginalProductionFixture.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <memory>
#include <string>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace fs=std::filesystem;
using oe::testing::economy::check;
using Rect=oe::sandbox_ui::Rect;
void event(oe::SandboxView& view,SDL_Event value) {
    check(SDL_PushEvent(&value),"Queue actual demolition SDL event");bool running=true;
    while (SDL_PollEvent(&value)) view.handle_event(value,running);
    check(running,"Demolition input unexpectedly quit the view");
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);
}
void boundary(oe::SandboxView& view,SDL_EventType type) {SDL_Event e{};e.type=type;event(view,e);}
oe::scene::Point window_point(SDL_Renderer* renderer,oe::scene::Point p) {
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(p.x),static_cast<float>(p.y),&x,&y),
        "Invert actual view input coordinates");return {x,y};
}
void mouse(oe::SandboxView& view,SDL_Renderer* renderer,oe::scene::Point p,bool down) {
    const auto raw=window_point(renderer,p);SDL_Event e{};
    e.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(raw.x);e.button.y=static_cast<float>(raw.y);event(view,e);
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Rect rect) {
    check(rect.w>0 && rect.h>0,"Demolition control has no visible rectangle");
    const oe::scene::Point p{rect.x+rect.w*.5,rect.y+rect.h*.5};mouse(view,renderer,p,true);mouse(view,renderer,p,false);
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
    check(false,"Ordinary paused camera pan did not converge");
}
oe::scene::Point ground(sim::Cell cell) {return {double(cell.x-cell.y)*40,double(cell.x+cell.y-144)*20};}
void focus(oe::SandboxView& view,sim::Cell cell) {
    const auto map=view.layout().map;const auto g=ground(cell);const auto z=view.camera().zoom;
    pan(view,{map.x+map.w*.5-g.x*z,map.y+map.h*.5-g.y*z});
}
oe::scene::Point cell_point(oe::SandboxView& view,sim::Cell cell) {
    focus(view,cell);check(view.render(),"Render actual paid target before picking");
    const auto center=view.camera().world_to_screen(ground(cell));
    for (int dy=-12;dy<=12;dy+=2) for (int dx=-12;dx<=12;dx+=2) {
        const oe::scene::Point p{center.x+dx,center.y+dy};
        if (view.layout().map.contains(p.x,p.y) && view.pick(p)==cell) return p;
    }
    throw std::runtime_error("Paid footprint has no visible actual ground-cell pick");
}
void click_cell(oe::SandboxView& view,SDL_Renderer* renderer,sim::Cell cell) {
    const auto p=cell_point(view,cell);mouse(view,renderer,p,true);mouse(view,renderer,p,false);
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    view.set_tool(5);click_cell(view,renderer,view.world().building(id).cell);
    check(view.selected_building()==id,"Actual Select mouse gesture missed paid building");
    // Selection uses the actual origin pick above. Frame the complete authored
    // footprint afterwards through ordinary paused camera input, including4x.
    const auto& body=view.world().building(id);
    const auto footprint=sim::building_footprint(view.world().profile(),view.world().rule_version(),body.kind);
    auto center=ground(body.cell);center.y+=10.0*(footprint.width+footprint.height);
    const auto map=view.layout().map;const auto z=view.camera().zoom;
    pan(view,{map.x+map.w*.5-center.x*z,map.y+map.h*.5-center.y*z});
    check(view.render(),"Render selected paid inspector");
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;const auto p=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    for (unsigned attempt=0;attempt<3 && std::abs(view.camera().zoom-wanted)>=1e-9;++attempt) {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);e.wheel.mouse_y=static_cast<float>(p.y);
        e.wheel.y=static_cast<float>(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0);
    }
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Real wheel missed literal1/2/4 zoom");
}
Rect modal_button(const oe::SandboxView& view,bool confirm) {
    const int scale=view.layout().scale;const auto map=view.layout().map;
    const int width=std::min(540*scale,std::max(0,map.w-24*scale));
    const int x=map.x+(map.w-width)/2,y=map.y+(map.h-260*scale)/2;
    Rect rect{x+12*scale,y+202*scale,std::min(250*scale,std::max(0,(width-36*scale)/2)),34*scale};
    if (!confirm) rect.x+=rect.w+16*scale;return rect;
}
void bounded(const oe::SandboxView& view) {
    const auto footer=view.inspector_layout();const auto content=view.original_house_content_rect();const auto panel=view.layout().panel;
    check(footer.demolition.w>0 && footer.demolition.h>0 && footer.hints.h>0,"Rule19 House lacks shared demolition footer");
    for (const auto rect:{footer.demolition,footer.hints,content})
        check(rect.w>0 && rect.h>0 && panel.contains(rect.x,rect.y) && panel.contains(rect.x+rect.w-1,rect.y+rect.h-1),
            "House document or demolition footer escapes panel");
    check(content.y+content.h<=footer.demolition.y && footer.demolition.y+footer.demolition.h<=footer.hints.y,
        "House section overlaps demolition controls or refusal reason");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& output,const std::string& name,nlohmann::json& observations) {
    if (output.empty()) return;
    check(SDL_GetRenderTarget(renderer)==nullptr,"View capture requires the ordinary window target");
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height) && width>0 && height>0,"Read actual view output dimensions");
    const auto before=view.world().snapshot();const auto camera=view.camera();
    const auto buildings_before=view.building_display_stats();
    const auto houses_before=view.original_house_display_stats();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Create own view capture target");
    struct Restore {SDL_Renderer* renderer;~Restore(){SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    check(view.render(),"Render demolition capture into owned target");
    check(SDL_GetRenderTarget(renderer)==target.get(),"View render changed the owned readback target");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(frame && frame->w==width && frame->h==height && SDL_SavePNG(frame.get(),(output/(name+".png")).string().c_str()),
        "Write same-output-size actual demolition PNG");
    const auto after=view.camera();
    check(view.world().snapshot()==before && after.zoom==camera.zoom && after.offset.x==camera.offset.x && after.offset.y==camera.offset.y &&
        after.viewport_width==camera.viewport_width && after.viewport_height==camera.viewport_height,
        "Same-output-size capture changed World or camera");
    const auto roads=view.road_display_stats();const auto painter=view.painter_stats();
    const auto buildings_after=view.building_display_stats();
    const auto houses_after=view.original_house_display_stats();
    std::uint64_t bodies=0;
    for (std::size_t i=0;i<buildings_before.drawn_instances.size();++i)
        bodies+=buildings_after.drawn_instances[i]-buildings_before.drawn_instances[i]+
            buildings_after.placeholder_fallbacks[i]-buildings_before.placeholder_fallbacks[i];
    // Original-economy Houses have no generic visual role (visual_role returns
    // nullopt for original_save_fixture_). Both art and authored House fallback
    // therefore use their separate counters in this exact-generation fixture.
    std::uint64_t house_art=0,house_fallbacks=0;
    for (std::size_t i=0;i<houses_before.drawn_instances.size();++i) {
        house_art+=houses_after.drawn_instances[i]-houses_before.drawn_instances[i];
        house_fallbacks+=houses_after.authored_fallbacks[i]-houses_before.authored_fallbacks[i];
    }
    bodies+=house_art+house_fallbacks;
    const auto couriers=view.last_courier_draws();
    // Unified rendering submits roads in the preceding ground pass, then
    // records only the spatial suffix in WorldMergeStats::sandbox_items.
    const auto submissions=painter.road_ground_items+painter.sandbox_items;
    const auto footer=view.inspector_layout();const auto rect=footer.demolition;
    observations.push_back({{"capture",name+".png"},{"ticks",view.world().ticks()},{"command_sequence",view.world().command_sequence()},
        {"zoom",view.camera().zoom},{"demolition_pending",view.demolition_pending()},{"button",{rect.x,rect.y,rect.w,rect.h}},
        {"reason",view.selected_building() ? view.world().demolition_status(*view.selected_building()).reason:std::string{}},
        {"capture_target","owned same-output-size RGBA32 TARGET texture"},{"readback_before_present",true},
        {"owned_target_retained_after_render",true},{"output_size",{width,height}},
        {"world_camera_unchanged",true},{"road_submissions",roads.draws+roads.fallback_draws},
        {"live_buildings",view.world().buildings().size()},{"sandbox_submissions",painter.sandbox_items},
        {"building_submissions",bodies},{"courier_submissions",couriers},
        {"original_house_art_submissions",house_art},
        {"original_house_authored_fallback_submissions",house_fallbacks},
        {"road_ground_pass",painter.road_ground_pass},{"road_ground_submissions",painter.road_ground_items},
        {"total_sandbox_submissions",submissions},{"unified_depth",view.unified_depth()},
        {"scripted_sdl",true},{"native","NOT_RUN"},{"human","NOT_RUN"}});
    oe_influence_fixture::write_text(output/"capture-diagnostics.json",observations.dump(2));
    // Idle Clay and Pottery couriers remain visible in the existing production
    // presentation contract. Count them separately rather than dropping them.
    if (name=="clay.rebuilt") check(roads.draws+roads.fallback_draws==16 && view.world().buildings().size()==6 &&
        bodies==6 && couriers==2 && submissions==16+6+2,
        ("Post-rebuild capture omitted the sixteen roads or six surviving paid bodies: "+observations.back().dump()).c_str());
}
sim::BuildingId buy(oe::SandboxView& view,sim::CommandType type,sim::Cell at) {
    const auto result=view.execute({type,at});check(result.accepted && result.changed,result.reason.c_str());return view.world().building_owner_at(at).value_or(sim::BuildingId{});
}
void modal_cancels(oe::SandboxView& view,SDL_Window* window,SDL_Renderer* renderer) {
    const auto before=view.world().snapshot();const auto document=view.capture_save_document();const auto dirty=view.dirty();
    const auto generation=view.save_generation();
    const auto unchanged=[&] {
        check(!view.demolition_pending() && view.world().snapshot()==before && view.dirty()==dirty && view.save_generation()==generation,
            "Cancelled demolition mutated authority or manual-save state");oe_influence_fixture::same_document(document,view.capture_save_document());
    };
    click(view,renderer,view.inspector_layout().demolition);check(view.demolition_pending() && !view.recovery_safe_point(),"Demolish did not open blocking confirmation");
    view.tick_once();view.update(1);check(view.world().snapshot()==before,"Pending demolition advanced World ticks");key(view,SDLK_ESCAPE);unchanged();
    click(view,renderer,view.inspector_layout().demolition);click(view,renderer,modal_button(view,false));unchanged();
    click(view,renderer,view.inspector_layout().demolition);const auto confirm=modal_button(view,true);
    mouse(view,renderer,{confirm.x+confirm.w*.5,confirm.y+confirm.h*.5},true);mouse(view,renderer,{8,8},false);
    check(view.demolition_pending() && view.world().snapshot()==before,"Mismatched modal release confirmed demolition");key(view,SDLK_ESCAPE);unchanged();
    click(view,renderer,view.inspector_layout().demolition);boundary(view,SDL_EVENT_WINDOW_FOCUS_LOST);
    mouse(view,renderer,{confirm.x+confirm.w*.5,confirm.y+confirm.h*.5},false);unchanged();boundary(view,SDL_EVENT_WINDOW_FOCUS_GAINED);
    click(view,renderer,view.inspector_layout().demolition);
    int width=0,height=0;check(SDL_GetWindowSize(window,&width,&height) && SDL_SetWindowSize(window,width+1,height),"Resize actual pending modal");
    boundary(view,SDL_EVENT_WINDOW_RESIZED);mouse(view,renderer,{confirm.x+confirm.w*.5,confirm.y+confirm.h*.5},false);unchanged();
    check(SDL_SetWindowSize(window,width,height),"Restore actual window size");boundary(view,SDL_EVENT_WINDOW_RESIZED);check(view.render(),"Draw after modal resize cancellation");
}
void remove_by_mouse(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    const auto before=view.world().snapshot();const auto cell=view.world().building(id).cell;const auto refresh=view.world().route_refresh_count();
    const auto owned=static_cast<std::size_t>(std::count_if(view.world().couriers().begin(),view.world().couriers().end(),[&](const auto& c){return c.owner==id;}));
    check(view.world().demolition_status(id).allowed,"Paid UI positive has no authoritative permission");
    click(view,renderer,view.inspector_layout().demolition);check(view.demolition_pending(),"Allowed Demolish did not open modal");
    click(view,renderer,modal_button(view,true));
    check(!view.demolition_pending() && !view.selected_building() && view.world().ticks()==before.ticks &&
        view.world().treasury()==before.treasury && view.world().construction_spent_total()==before.construction_spent_total &&
        view.world().taxes_collected_total()==before.taxes_collected_total && view.world().maintenance_spent_total()==before.maintenance_spent_total &&
        view.world().next_building_id()==before.next_building_id && view.world().next_courier_id()==before.next_courier_id &&
        view.world().command_sequence()==before.command_sequence+1 && view.world().road_revision()==before.road_revision+1 &&
        view.world().route_refresh_count()==refresh+1 && view.world().buildings().size()+1==before.buildings.size() &&
        view.world().couriers().size()+owned==before.couriers.size(),"Confirmed demolition changed money/IDs/ticks or left entities");
    for (int y=0;y<2;++y) for (int x=0;x<2;++x)
        check(!view.world().building_owner_at({cell.x+x,cell.y+y}) && view.world().object_at({cell.x+x,cell.y+y})==sim::Object::Empty,
            "Confirmed demolition retained part of its2x2 footprint");
    check(view.world().production_balance_valid() && view.world().navigation_valid() && view.world().city_economy_valid() && view.render(),
        "Post-demolition authority or submitted sprite frame is invalid");
}
void blocked(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    select(view,renderer,id);const auto before=view.world().snapshot();const auto document=view.capture_save_document();
    const auto status=view.world().demolition_status(id);check(!status.allowed && !status.reason.empty(),"Expected strict World refusal");
    click(view,renderer,view.inspector_layout().demolition);
    check(!view.demolition_pending() && view.last_message()==status.reason && view.world().snapshot()==before,
        "Blocked inspector action opened modal, changed World or hid authoritative reason");
    oe_influence_fixture::same_document(document,view.capture_save_document());
}
} // namespace

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==1 || (argc==3 && std::string_view(argv[1])=="--capture"),"Unknown safe-demolition view arguments");
        const fs::path output=argc==3 ? fs::absolute(argv[2]):fs::path{};if (!output.empty()) fs::create_directories(output);
        nlohmann::json observations=nlohmann::json::array();
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL view");
        check(SDL_CreateWindowAndRenderer("Safe demolition view",1280,720,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create actual demolition view");
        oe::testing::economy::Temp source;const auto root=fs::canonical(source.root);
        const auto data=root/"data",save=root/"saves/demolition.json";
        oe_production_fixture::write_model(data,37,0,1);
        const auto session=[&]{return oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);};
        auto view=oe::SandboxView::create_original_safe_demolition(session());view->configure_save(data,"Cities/A.map",save);view->initialize(window,renderer);
        check(view->paused() && view->world().rule_version()==19,"Rule19 did not start paused");
        const sim::Cell home_cell{96,96};auto home=buy(*view,sim::CommandType::PlaceHousehold,home_cell);key(*view,SDLK_F5);
        check(fs::is_regular_file(save),("Initial F5 failed: "+view->last_message()).c_str());
        const auto original_bytes=oe_influence_fixture::read_text(save);
        select(*view,renderer,home);const auto pure=view->world().snapshot();
        oe::performance::set_enabled(true);oe::performance::reset();
        for (int i=0;i<100;++i) {view->update(0);check(view->render(),"Render pure Rule19 House inspector");}
        for (std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
            check(oe::performance::counter(static_cast<oe::performance::Counter>(i))==0,
                "Rule19 inspector frame performed authority, source preparation, decode/upload or routing");
        oe::performance::set_enabled(false);check(view->world().snapshot()==pure,"Pure Rule19 footer changed World");
        for (const auto size:std::array{std::pair{1280,720},std::pair{1600,900},std::pair{1920,1080}}) {
            check(SDL_SetWindowSize(window,size.first,size.second),"Resize actual inspector");boundary(*view,SDL_EVENT_WINDOW_RESIZED);check(view->render(),"Draw resized inspector");
            for (const double z:{1.,2.,4.}) {
                zoom(*view,renderer,z);select(*view,renderer,home);bounded(*view);
                const auto name=std::to_string(size.first)+".zoom"+std::to_string(static_cast<int>(z));
                capture(*view,renderer,output,"house.ready."+name,observations);
                click(*view,renderer,view->inspector_layout().demolition);check(view->demolition_pending(),"House button is not reachable through original inspector");
                capture(*view,renderer,output,"house.confirm."+name,observations);key(*view,SDLK_ESCAPE);
                modal_cancels(*view,window,renderer);bounded(*view);
                check(oe_influence_fixture::read_text(save)==original_bytes,"UI cancellation rewrote manual save bytes");
            }
        }
        for (const double z:{1.,2.,4.}) {
            zoom(*view,renderer,z);select(*view,renderer,home);const auto old=home;
            remove_by_mouse(*view,renderer,home);capture(*view,renderer,output,"house.removed.zoom"+std::to_string(static_cast<int>(z)),observations);
            for (int y=0;y<2;++y) for (int x=0;x<2;++x) {
                const auto p=cell_point(*view,{home_cell.x+x,home_cell.y+y});check(view->pick(p)==sim::Cell{home_cell.x+x,home_cell.y+y},"Freed cell retained a geometric hitbox");
            }
            const auto funds=view->world().treasury();const auto spent=view->world().construction_spent_total();
            view->set_tool(7);click_cell(*view,renderer,home_cell);home=*view->world().building_owner_at(home_cell);
            check(home>old && view->world().treasury()==funds-sim::Rules::household_cost &&
                view->world().construction_spent_total()==spent+static_cast<std::uint64_t>(sim::Rules::household_cost) && view->world().household_population(home)==0,
                "Actual same-cell House rebuild reused identity, avoided payment or granted population");
            select(*view,renderer,home);capture(*view,renderer,output,"house.rebuilt.zoom"+std::to_string(static_cast<int>(z)),observations);
        }
        key(*view,SDLK_F5);const auto saved=view->capture_save_document();const auto saved_bytes=oe_influence_fixture::read_text(save);
        view->tick_once();key(*view,SDLK_F9);oe_influence_fixture::same_document(saved,view->capture_save_document());
        check(oe_influence_fixture::read_text(save)==saved_bytes,"F9 changed post-demolition manual bytes");view->shutdown();view.reset();
        auto fresh=oe::SandboxView::create_original_safe_demolition(session());fresh->configure_save(data,"Cities/A.map",save,oe::persistence::read_save(save));fresh->initialize(window,renderer);
        oe_influence_fixture::same_document(saved,fresh->capture_save_document());view=std::move(fresh);
        // Real first Clay extraction, delivery and physical return. No stock edits.
        for (int x=104;x<=119;++x) buy(*view,sim::CommandType::PlaceRoad,{x,103});
        const auto clay=buy(*view,sim::CommandType::PlaceClaySource,{104,104});const auto kiln=buy(*view,sim::CommandType::PlacePottery,{107,104});
        const auto warehouse=buy(*view,sim::CommandType::PlaceWarehouse,{110,104});
        const auto occupied=buy(*view,sim::CommandType::PlaceHousehold,{104,101});buy(*view,sim::CommandType::PlaceHousehold,{107,101});
        sim::CourierId courier{};for (const auto& c:view->world().couriers()) if (c.owner==clay && c.role==sim::CourierRole::Clay) courier=c.id;
        blocked(*view,renderer,clay);
        for (int i=0;i<4000 && view->world().courier(courier).phase!=sim::CourierPhase::ToWarehouse;++i) view->tick_once();
        check(view->world().courier(courier).phase==sim::CourierPhase::ToWarehouse && view->world().courier(courier).cargo==1 &&
            view->world().building(clay).clay_extracted==1 && view->world().building(clay).output==0 && view->world().building(clay).progress==0,
            "Paid scene did not earn the first actual Clay outbound");
        check(view->execute(sim::set_building_operation(clay,false)).accepted,"Pause actual loaded Clay source");blocked(*view,renderer,clay);
        for (int i=0;i<4000 && view->world().courier(courier).phase!=sim::CourierPhase::IdleAtWorkshop;++i) view->tick_once();
        check(view->world().courier(courier).phase==sim::CourierPhase::IdleAtWorkshop && view->world().building(kiln).input_clay==1 &&
            view->world().demolition_status(clay).allowed,"Earned Clay did not physically arrive and fully return before demolition");
        blocked(*view,renderer,occupied);blocked(*view,renderer,warehouse);select(*view,renderer,clay);
        capture(*view,renderer,output,"clay.drained",observations);modal_cancels(*view,window,renderer);remove_by_mouse(*view,renderer,clay);
        capture(*view,renderer,output,"clay.removed",observations);const auto funds=view->world().treasury();
        view->set_tool(2);click_cell(*view,renderer,{104,104});const auto rebuilt=*view->world().building_owner_at({104,104});
        check(rebuilt>clay && view->world().treasury()==funds-sim::Rules::clay_source_cost && view->world().building(kiln).input_clay==1 &&
            view->world().clay_extracted_total()==1,"Clay rebuild refunded money, inherited production, duplicated arrival or reused ID");
        select(*view,renderer,rebuilt);capture(*view,renderer,output,"clay.rebuilt",observations);key(*view,SDLK_F5);
        const auto final=view->capture_save_document();const auto final_bytes=oe_influence_fixture::read_text(save);view->tick_once();key(*view,SDLK_F9);
        oe_influence_fixture::same_document(final,view->capture_save_document());check(oe_influence_fixture::read_text(save)==final_bytes,"Final F9 changed source bytes");
        view->shutdown();view.reset();
        if (!output.empty()) oe_influence_fixture::write_text(output/"observations.json",nlohmann::json{{"rule_version",19},{"schema",35},
            {"captures",observations},{"scripted_sdl",true},{"native","NOT_RUN"},{"human","NOT_RUN"}}.dump(2)+"\n");
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Rule19 House/earned drained Clay SDL demolition, strict refusal, cancellation boundaries, paid rebuild,1/2/4x and F5/F9 PASS; native/human NOT_RUN\n";return 0;
    } catch (const std::exception& error) {
        oe::performance::set_enabled(false);std::cerr<<error.what()<<'\n';if (renderer) SDL_DestroyRenderer(renderer);if (window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
