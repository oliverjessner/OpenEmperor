// Independent paid SDL scenario; native-window/human acceptance is separate.
#include "EconomyStartViewFixture.h"
#include "FishingFixture.h"
#include "app/OriginalHouseInspection.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <cmath>

namespace {
namespace oe=openemperor;namespace sim=oe::simulation;
namespace f=oe::testing::economy;namespace q=oe_irrigation_fixture;
namespace z=oe_fishing_fixture;namespace fs=std::filesystem;
using f::check;using Point=oe::scene::Point;
void key(oe::menu::MenuSession& menu,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;
    menu.handle_event(e);menu.advance();
}
void event(oe::SandboxView& view,SDL_Event e) {
    bool running=true;view.handle_event(e,running);check(running,"Input closed Playable Fishing");
}
void key(oe::SandboxView& view,SDL_Keycode code,SDL_Keymod mod=SDL_KMOD_NONE) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;e.key.mod=mod;event(view,e);
}
void center(oe::SandboxView& view,Point ground) {
    const auto map=view.layout().map;const auto z=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-ground.x*z,map.y+map.h*.5-ground.y*z};
    for(unsigned step=0;step<1000;++step) {
        const auto now=view.camera().offset;const auto dx=wanted.x-now.x,dy=wanted.y-now.y;
        if(std::abs(dx)<1e-6 && std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6) {held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused Playable Fishing camera did not converge");
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point render) {
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,float(render.x),float(render.y),&x,&y),"Pointer transform");
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=x;e.motion.y=y;event(view,e);view.update(0);
    e={};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;event(view,e);
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,e);view.update(0);
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::Cell cell) {
    const auto ground=oe::maps::terrain_ground({std::uint32_t(cell.x),std::uint32_t(cell.y)},1);
    view.set_tool(5);center(view,ground);click(view,renderer,view.camera().world_to_screen(ground));
}
bool contains(const std::vector<std::string>& lines,std::string_view fragment) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(fragment)!=std::string::npos;});
}
std::size_t row(const std::vector<std::string>& lines,std::string_view fragment) {
    const auto found=std::find_if(lines.begin(),lines.end(),[&](const auto& line){return line.find(fragment)!=std::string::npos;});
    if(found==lines.end())throw std::runtime_error("Missing priority inspector line: "+std::string(fragment));
    return static_cast<std::size_t>(found-lines.begin());
}
void visible(oe::SandboxView& view,const std::vector<std::string>& lines,std::string_view fragment) {
    check(view.inspector_layout().detail_row_rect(row(lines,fragment)).h>0,
        "Actual crop/Food status is below the visible inspector viewport");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    int w=0,h=0;check(SDL_GetCurrentRenderOutputSize(renderer,&w,&h),"Capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Private capture target");
    check(view.render(),"Playable Fishing render");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Private capture output");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore render target");
}
void logging_list_label(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    std::size_t index=0;for(const auto& body:view.world().buildings())if(body.placed) {if(body.id==id)break;++index;}
    const auto content=view.inspector_layout().content;float wx=0,wy=0;
    check(SDL_RenderCoordinatesToWindow(renderer,float(content.x+content.w/2),float(content.y+content.h/2),&wx,&wy),"List scroll transform");
    SDL_Event scroll{};scroll.type=SDL_EVENT_MOUSE_WHEEL;scroll.wheel.mouse_x=wx;scroll.wheel.mouse_y=wy;scroll.wheel.y=-1000;event(view,scroll);view.update(0);
    const auto row=view.inspector_layout().list_row_rect(index);check(row.h>0,"Paid Logging list row is not visible");
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"List raster dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()) && view.render(),"Render actual paid Logging list row");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> actual(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(actual),"Actual Logging raster");
    const auto scale=view.layout().scale;const float text_scale=1.25F*float(scale);
    const int x=view.layout().panel.x+10*scale,y=row.y+2*scale;
    const auto cell=view.world().building(id).cell;
    const auto label=std::to_string(static_cast<unsigned>(id))+" Logging ("+std::to_string(cell.x)+","+std::to_string(cell.y)+")";
    check(SDL_SetRenderClipRect(renderer,nullptr) && SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer) &&
        SDL_SetRenderDrawColor(renderer,235,240,250,255) && SDL_SetRenderScale(renderer,text_scale,text_scale) &&
        SDL_RenderDebugText(renderer,float(x)/text_scale,float(y)/text_scale,label.c_str()) && SDL_SetRenderScale(renderer,1,1),"Render independent literal Logging label");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> expected(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(expected),"Expected Logging raster");
    const auto ink=[](SDL_Surface* image,int px,int py) {
        std::array<std::uint8_t,4> rgba{};check(SDL_ReadSurfacePixel(image,px,py,&rgba[0],&rgba[1],&rgba[2],&rgba[3]),"Logging label pixel");
        return rgba==std::array<std::uint8_t,4>{235,240,250,255};
    };
    for(int py=y;py<y+int(8*text_scale);++py)for(int px=x;px<x+int(float(label.size())*8*text_scale);++px)
        check(ink(actual.get(),px,py)==ink(expected.get(),px,py),"Paid Logging list label changed to Fishing Quay or another name");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore Logging raster target");
}

void menu_path(const fs::path& data,const fs::path& app,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,app);menu.initialize(window,renderer);
    check(menu.state()==oe::menu::MenuSession::State::MainMenu && !menu.playable_fishing_setup(),"Fishing changed defaults");
    check(menu.diagnostic_button_rect("F Playable Fishing (rule 28)").has_value(),"Fishing missing from regular menu");
    key(menu,SDLK_F);
    check(menu.state()==oe::menu::MenuSession::State::NewSandbox && menu.playable_fishing_setup() && menu.original_agriculture_setup(),"F selection is not Fishing setup");
    key(menu,SDLK_P);check(!menu.original_paid_starter(),"Actual Fishing setup did not select Empty city");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking && std::chrono::steady_clock::now()<deadline) {
        menu.advance();std::this_thread::yield();
    }
    check(menu.map_rules_can_start() && menu.map_rules_result().request.rule_version==28,"Fishing preflight did not bind28");
    key(menu,SDLK_RETURN);
    check(menu.state()==oe::menu::MenuSession::State::Loading && menu.render(),"Fishing Loading frame");
    menu.advance();
    if(menu.state()!=oe::menu::MenuSession::State::Playing || !menu.sandbox()) throw std::runtime_error("Fishing menu Start: "+menu.message());
    check(menu.sandbox()->world().playable_fishing_enabled() && menu.sandbox()->world().rule_version()==28 && menu.sandbox()->paused(),"Wrong Fishing World");
    menu.shutdown();
}
void optional_fishing_art(const fs::path& data,SDL_Window* window,SDL_Renderer* renderer) {
    using Json=nlohmann::json;
    // Independent authored Type30 pixels, not copied game inputs. Only the
    // four Fishing images can add textures to this otherwise fallback profile.
    f::Bytes sg3(40680U+725U*64U,0),pixels(4U+4U*12800U,0);
    f::u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));f::u32(sg3,4,213);
    f::u32(sg3,12,725);f::u32(sg3,16,724);f::u32(sg3,20,2);
    const std::string system="Zeus_system.bmp",normal="synthetic.bmp";
    std::copy(system.begin(),system.end(),sg3.begin()+680);std::copy(normal.begin(),normal.end(),sg3.begin()+880);
    f::u32(sg3,680+124,200);f::u32(sg3,680+128,1);f::u32(sg3,680+132,200);
    f::u32(sg3,880+124,524);f::u32(sg3,880+128,201);f::u32(sg3,880+132,724);
    constexpr std::array<unsigned,4> heights{107,121,134,120},physicals{724,721,722,723};
    for(unsigned i=0;i<4;++i) {
        const auto at=40680U+(721U+i)*64U;
        f::u32(sg3,at,4U+i*12800U);f::u32(sg3,at+4,12800);f::u32(sg3,at+8,12800);
        f::u16(sg3,at+20,158);f::u16(sg3,at+22,static_cast<std::uint16_t>(heights[i]));
        f::u16(sg3,at+50,30);sg3[at+55]=2;sg3[at+56]=1;
        for(unsigned p=0;p<6400;++p)f::u16(pixels,4U+i*12800U+2U*p,static_cast<std::uint16_t>(0x001fU+i*64U));
    }
    const auto hash=[](const f::Bytes& bytes) {
        std::array<unsigned char,32> digest{};unsigned size=0;
        check(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&size,EVP_sha256(),nullptr)==1 && size==32,"Fishing test SHA256");
        constexpr char hex[]="0123456789abcdef";std::string value;
        for(const auto byte:digest){value+=hex[byte>>4U];value+=hex[byte&15U];}return value;
    };
    f::write(data/"DATA/China_General.sg3",sg3);f::write(data/"DATA/China_General.555",pixels);
    const Json files={{{"path","DATA/China_General.sg3"},{"sha256",hash(sg3)}},{{"path","DATA/China_General.555"},{"sha256",hash(pixels)}}};
    Json rows=Json::array(),poses=Json::array();
    for(std::size_t i=0;i<oe::assets::original_building_schema23_role_count;++i) {
        const auto& id=oe::assets::original_building_visual_identities[i];
        rows.push_back({{"model_type",id.model_type},{"original_side",id.footprint_side},{"native",false},
            {"fallback","Independent historical marker"},{"evidence","Synthetic test; no Original pixels"}});
    }
    for(unsigned orientation=0;orientation<4;++orientation) {
        const auto h=heights[physicals[orientation]-721U];
        poses.push_back({{"orientation",orientation},{"model_type",31U},{"image_index",physicals[orientation]},
            {"width",158U},{"height",h},{"base_bytes",12800U},{"ground_anchor",{79U,h-20U}}});
    }
    const Json base={{"schema_version",2U},{"mode","original_building_bases"},{"selection","canonical_static_base"},
        {"anchor_policy","type30_front_cell"},{"evidence","Independent fallback test"},{"files",files},{"buildings",rows}};
    const Json fishing={{"schema_version",1U},{"mode","curated_fishing_static"},{"selection","authored-quay-four-static-poses-v1"},
        {"anchor_policy","type30_front_cell"},{"evidence","Independent four-pose test"},{"files",files},{"quays",poses}};
    for(const bool malformed:{false,true}) {
        const auto folder=data.parent_path()/(malformed ? "invalid-irrigation-tail":"missing-irrigation-tail");fs::create_directories(folder);
        q::f::write_text(folder/"base.json",base.dump());q::f::write_text(folder/"fishing.json",fishing.dump());
        if(malformed)q::f::write_text(folder/"irrigation.json","{\"schema_version\":999}");
        auto session=oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_playable_fishing(std::move(session));
        view->configure_save(data,"Cities/A.map",folder/"unused.save.json");view->initialize(window,renderer);
        const auto before=view->world().snapshot();check(view->building_texture_count()==0,"Optional art control already owns textures");
        view->set_original_building_visuals(folder/"base.json",oe::VisualProfileSource::Builtin);
        check(view->building_texture_count()==4 && view->original_building_display_stats().preparation_error.find("Irrigation art fallback")!=std::string::npos,
            "Failed independent Irrigation metadata suppressed qualified Fishing art or its actual diagnostic");
        check(view->world().snapshot()==before,"Optional Fishing art changed World authority");
    }
}
}
int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        f::Temp temp;temp.root=fs::canonical(temp.root);
        z::Fixture source;
        auto model=z::f::read_text(source.root.data/"Model/EmperorBuildingModels.txt");
        fs::create_directories(temp.root/"data/Model");q::f::write_text(temp.root/"data/Model/EmperorBuildingModels.txt",model);
        std::vector<std::uint32_t> raw(sim::OriginalTerrainFacts::cell_count,0x80U);
        std::vector<std::uint8_t> fertility(raw.size(),100);
        for(int y=94;y<=105;++y)for(int x=100;x<=132;++x)raw[std::size_t(y)*228U+std::size_t(x)]=4U;
        f::write(temp.root/"data/Cities/A.map",z::encoded_map(raw,fertility,true));
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted Fishing SDL");
        check(SDL_CreateWindowAndRenderer("Fishing scripted acceptance",1280,900,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create Fishing renderer");
        menu_path(temp.root/"data",temp.root/"menu",window,renderer);
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8,
            oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto view=oe::SandboxView::create_playable_fishing(std::move(session));
        view->configure_save(temp.root/"data","Cities/A.map",temp.root/"fishing.save.json");
        view->initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view->execute({type,cell});
            if (!result.accepted || !result.changed) throw std::runtime_error("Paid UI construction "+
                std::to_string(static_cast<unsigned>(type))+" at "+std::to_string(cell.x)+","+
                std::to_string(cell.y)+": "+result.reason);
            return view->world().building_owner_at(cell);
        };
        for(int x=89;x<=153;++x) for(const int y:{114,117}) buy(sim::CommandType::PlaceRoad,{x,y});
        for(int y=115;y<=116;++y)for(const int x:{89,153})buy(sim::CommandType::PlaceRoad,{x,y});
        for(int x=90;x<150;x+=3)buy(sim::CommandType::PlaceHousehold,{x,112});
        for(int x=114;x<=119;++x)buy(sim::CommandType::PlaceRoad,{x,107});
        for(int y=108;y<=113;++y)buy(sim::CommandType::PlaceRoad,{119,y});
        const auto market=*buy(sim::CommandType::PlaceMarket,{134,118});
        buy(sim::CommandType::PlaceOriginalWaterWell,{138,118});buy(sim::CommandType::PlaceOriginalAncestorShrine,{142,118});
        buy(sim::CommandType::PlaceFireWatch,{146,118});buy(sim::CommandType::PlaceFireWatch,{149,118});
        std::array<sim::BuildingId,5> fields{};
        for(std::size_t i=0;i<fields.size();++i) {
            const int x=90+8*static_cast<int>(i);const auto farm=*buy(sim::CommandType::PlaceOriginalFoodFarm,{x,118});
            sim::Command command{z::m::crop_commands[i],{x+3,118}};command.building_id=farm;
            check(view->execute(command).accepted,"Paid inherited field placement");fields[i]=*view->world().building_owner_at(command.cell);
        }
        key(*view,SDLK_F,SDL_KMOD_SHIFT);check(view->tool()==49,"Fishing keyboard action");
        for(unsigned i=0;i<4;++i)key(*view,SDLK_R);
        const sim::Cell origin{114,105};center(*view,oe::maps::terrain_ground({114,105},1));
        const auto amount=view->world().treasury();click(*view,renderer,view->camera().world_to_screen(oe::maps::terrain_ground({114,105},1)));
        const auto quay=view->world().building_owner_at(origin);
        check(quay && view->world().building(*quay).kind==sim::Object::OriginalFishingQuay && view->world().treasury()<amount,"Actual UI click did not buy Quay");
        check(view->world().fishing_quay_cells(*quay).size()==5,"UI Quay lacks full dock footprint");
        check(!view->world().original_agriculture_state() && view->world().fishing_food_state(),"UI used old five-kind authority");
        const auto output=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/agri-mac02/presentation"/temp.root.filename();fs::create_directories(output);
        // Observe/save the first actual moving edge. A full or ineligible Quay
        // need not launch another boat after later House consumption.
        std::optional<sim::FishingBoatId> visible_boat;
        for (unsigned tick=0;tick<12000 && !visible_boat;++tick) {
            for (const auto& boat:view->world().fishing_state()->boats) if (boat.edge_progress) {visible_boat=boat.id;break;}
            if (!visible_boat) view->tick_once();
        }
        check(visible_boat.has_value(),"Paid Quay never earned a first physical moving Boat for inspection and F5/F9");
        const auto position=view->world().fishing_boat_position(*visible_boat);
        const Point boat_ground{(position.x-position.y)*40,(position.x+position.y-2)*20+20};
        view->set_tool(5);center(*view,boat_ground);check(view->render(),"Prepare actual moving Boat pixels");
        click(*view,renderer,view->camera().world_to_screen(boat_ground));
        check(view->selected_building()==quay && contains(view->inspection_lines(),"Boat #"),"Normal Boat pixels do not select their paid Quay");
        capture(*view,renderer,output/"moving-boat-inspector.png");
        const auto snapshot=view->world().snapshot();oe::performance::set_enabled(true);oe::performance::reset();
        for(unsigned frame=0;frame<10;++frame)check(view->render(),"Fishing render");
        for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
            check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Fishing render did resource or gameplay work");
        oe::performance::set_enabled(false);check(snapshot==view->world().snapshot(),"Fishing render mutated World");
        view->save_now();check(oe::persistence::read_save(view->save_path()).source_schema_version==44,"F5 did not write44");
        view->tick_once();view->load_now();check(snapshot==view->world().snapshot(),"F9 did not restore exact Fishing state");
        // The restored mid-edge World must still catch, unload and supply
        // actual consumers while all inherited crop chains remain active.
        bool consumed=false;
        for(unsigned tick=0;tick<12000 && !consumed;++tick) {
            view->tick_once();
            consumed=z::consumed(view->world())>0;
        }
        check(consumed && z::source(view->world(),*quay).unloaded>0,"UI did not earn Boat catch, dock unload and actual House consumption");
        for(const auto id:fields) {
            const auto field=std::find_if(view->world().agriculture_fields().begin(),view->world().agriculture_fields().end(),[&](const auto& row){return row.field_id==id;});
            check(field!=view->world().agriculture_fields().end() && field->harvested>0,"Fishing lost inherited crop production");
        }
        select(*view,renderer,origin);auto lines=view->inspection_lines();
        check(contains(lines,"Fishing Quay31") && contains(lines,"Fish stock") && contains(lines,"Dock ") && contains(lines,"Workers "),"Quay inspector missing actual authority");
        visible(*view,lines,"Fish stock");capture(*view,renderer,output/"quay-inspector.png");
        select(*view,renderer,view->world().building(market).cell);lines=view->inspection_lines();
        check(contains(lines,"Fish ") && contains(lines,"Arrived") && row(lines,"Food total")<row(lines,"Ceramics stock"),"Market does not prioritize all six Foods");
        visible(*view,lines,"Fish ");capture(*view,renderer,output/"market-six-foods.png");
        bool inspected_house=false;
        for (const auto& stock:view->world().fishing_food_state()->stocks) if (stock.consumed[sim::fishing_food_index]>0) {
            const auto observed=oe::original_house_ui::inspect_house(view->world(),stock.building_id,oe::original_house_ui::Section::Goods);
            check(observed.fishing_food && !observed.agriculture_food &&
                observed.fishing_food->received[sim::fishing_food_index]==stock.received[sim::fishing_food_index] &&
                observed.fishing_food->consumed[sim::fishing_food_index]==stock.consumed[sim::fishing_food_index],"House inspector lost actual Fish receipts/consumption");
            check(contains(observed.sections[static_cast<std::size_t>(oe::original_house_ui::Section::Goods)],"Fish House stock"),"House Goods section hides Fish");
            inspected_house=true;break;
        }
        check(inspected_house,"No actually supplied Fish consumer to inspect");
        const auto logging=*buy(sim::CommandType::PlaceOriginalLoggingShed,{128,122});select(*view,renderer,{128,122});
        const auto logging_lines=view->inspection_lines();
        check(view->selected_building()==logging && contains(logging_lines,"Logging #") && !contains(logging_lines,"Fishing Quay"),"Paid Logging selection lost its historical identity");
        logging_list_label(*view,renderer,logging);
        optional_fishing_art(temp.root/"data",window,renderer);
        view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Fishing regular menu, paid Quay/boat/House consumption, six Food inspectors, pure rendering and F5/F9 PASS\n";return 0;
    } catch(const std::exception& error) {SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
