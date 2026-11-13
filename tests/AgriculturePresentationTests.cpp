// Authored input and scripted SDL checks; no native or human acceptance claim.
#include "EconomyStartViewFixture.h"
#include "AgricultureFixture.h"
#include "app/SandboxUiLayout.h"
#include "app/OriginalHouseInspection.h"
#include "assets/BuildingVisualProfile.h"
#include "persistence/RecoveryStore.h"
#include "assets/RiceFieldTopology.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/StoredMapSession.h"
#include <openssl/evp.h>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <set>
#include <nlohmann/json.hpp>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace q=oe_agriculture_fixture;
namespace fs=std::filesystem;
using q::check;
using Menu=oe::menu::MenuSession;
fs::path repository_root() {return fs::canonical(fs::path(__FILE__).parent_path().parent_path());}
class Dialog final:public oe::menu::DialogAdapter {
public:
    void open_folder(SDL_Window*,Callback) override {}
    void open_file(SDL_Window*,Callback) override {}
    void cancel_pending() override {}
};
void key(Menu& menu,SDL_Keycode code,SDL_Keymod modifiers=SDL_KMOD_NONE) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;event.key.mod=modifiers;
    menu.handle_event(event);menu.advance();
}
void wait_map(Menu& menu,unsigned version) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(menu.map_rules_result().status==oe::maps::MapRulesStatus::Checking&&
        std::chrono::steady_clock::now()<deadline) {menu.advance();std::this_thread::yield();}
    check(menu.map_rules_can_start(),"Agriculture explicit map preflight refused");
    check(menu.map_rules_result().request.rule_version==version,"Preflight changed explicit mode");
}
void capture(Menu& menu,SDL_Renderer* renderer,const fs::path& path) {
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Own capture target");
    check(menu.render(),"Render Agriculture capture");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface&&SDL_SavePNG(surface.get(),path.string().c_str()),"Save Agriculture scripted capture");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore capture target");
}
using Bytes=std::vector<std::uint8_t>;
using Json=nlohmann::json;
using Point=oe::scene::Point;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
using Color=std::array<std::uint8_t,4>;
std::string sha256(const Bytes& bytes) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned length=0;
    check(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)==1,"Hash authored art");
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(unsigned i=0;i<length;++i)out<<std::setw(2)<<static_cast<unsigned>(digest[i]);
    return out.str();
}
Bytes read_bytes(const fs::path& path) {
    const auto value=q::f::read_text(path);return Bytes(value.begin(),value.end());
}
fs::path rice_profile(const fs::path& data,const fs::path& output,const std::optional<fs::path>& real_root) {
    Json tiles=Json::array();
    if(real_root) {
        // The caller explicitly supplies Original assets. Private copies stay
        // below ignored .local and are removed after this isolated SDL run.
        for(const auto name:{"China_General.sg3","China_General.555"})
            fs::copy_file(*real_root/"DATA"/name,data/"DATA"/name,fs::copy_options::overwrite_existing);
        const auto source=Json::parse(q::f::read_text(repository_root()/
            "resources/compatibility/gog-derived-2.0.0.2-en-assetset-1/original-agriculture-rice-buildings.json"));
        for(const auto& row:source.at("buildings"))if(row.at("model_type")==197)tiles=row.at("tiles");
        check(tiles.size()==47,"Real asset metadata lacks its complete inactive Rice bank");
        for(const auto& file:source.at("files"))
            check(sha256(read_bytes(data/fs::path(file.at("path").get<std::string>())))==file.at("sha256").get<std::string>(),
                "Supplied real Rice assets do not match the independently pinned compatibility source");
    } else {
        // Independent, visibly blue/magenta diamond rasters; no Original pixels.
        Bytes sg3(40680U+248U*64U),bitmap(4U+47U*3200U);
        oe::testing::economy::u32(sg3,0,static_cast<std::uint32_t>(sg3.size()));
        oe::testing::economy::u32(sg3,4,213);oe::testing::economy::u32(sg3,12,248);
        oe::testing::economy::u32(sg3,16,247);oe::testing::economy::u32(sg3,20,2);
        const std::string system="Zeus_system.bmp",normal="authored-rice.bmp";
        std::copy(system.begin(),system.end(),sg3.begin()+680);
        std::copy(normal.begin(),normal.end(),sg3.begin()+880);
        for(const auto [at,count,first,last]:std::array{std::array{680U,200U,1U,200U},std::array{880U,47U,201U,247U}}) {
            oe::testing::economy::u32(sg3,at+124,count);oe::testing::economy::u32(sg3,at+128,first);
            oe::testing::economy::u32(sg3,at+132,last);
        }
        for(unsigned group=0;group<173;++group)
            oe::testing::economy::u16(sg3,80U+2U*group,201);
        oe::testing::economy::u16(sg3,80U+2U*((0x42dU%512U)-1U),201);
        for(unsigned offset=0;offset<47;++offset) {
            const auto at=40680U+(201U+offset)*64U;
            oe::testing::economy::u32(sg3,at,4U+offset*3200U);
            oe::testing::economy::u32(sg3,at+4,3200);oe::testing::economy::u32(sg3,at+8,3200);
            oe::testing::economy::u16(sg3,at+20,78);oe::testing::economy::u16(sg3,at+22,40);
            oe::testing::economy::u16(sg3,at+50,30);sg3[at+55]=1;sg3[at+56]=1;
            for(unsigned pixel=0;pixel<1600;++pixel)
                oe::testing::economy::u16(bitmap,4U+offset*3200U+pixel*2U,
                    static_cast<std::uint16_t>(0x001fU+((offset%31U)<<11U)));
            tiles.push_back({{"offset",offset},{"image_index",201U+offset},{"width",78},{"height",40},
                {"base_bytes",3200},{"ground_anchor",{39,20}}});
        }
        oe::testing::economy::write(data/"DATA/China_General.sg3",sg3);
        oe::testing::economy::write(data/"DATA/China_General.555",bitmap);
    }
    Json rows=Json::array();
    for(const auto& identity:oe::assets::original_building_visual_identities) {
        Json row={{"model_type",identity.model_type},{"original_side",identity.footprint_side},
            {"native",false},{"fallback","Explicit authored fallback outside tested inactive Rice bank"},
            {"evidence","Isolated presentation fixture; no production authority"}};
        if(identity.role==oe::assets::OriginalBuildingVisualRole::RiceField) {
            row["native"]=true;row["archive"]="DATA/China_General.sg3";row["group_key"]=0x42d;
            row["tiles"]=tiles;
        }
        rows.push_back(std::move(row));
    }
    Json profile={{"schema_version",6},{"mode","original_building_bases"},{"selection","canonical_static_base"},
        {"anchor_policy","type30_front_cell"},{"field_selection",oe::assets::original_field_selection_contract},
        {"agriculture_field_selection",oe::assets::agriculture_field_selection_contract},
        {"rice_selection","source-inactive-rice-topology-canonical-0-v1"},
        {"evidence","Scripted inactive Rice visual test; Original work/season bank and Food remain unavailable"},
        {"files",{{{"path","DATA/China_General.sg3"},{"sha256",sha256(read_bytes(data/"DATA/China_General.sg3"))}},
                  {{"path","DATA/China_General.555"},{"sha256",sha256(read_bytes(data/"DATA/China_General.555"))}}}},
        {"buildings",std::move(rows)}};
    const auto path=output/"rice-test-profile.json";q::f::write_text(path,profile.dump(2));return path;
}
void event(oe::SandboxView& view,SDL_Event value) {
    bool running=true;view.handle_event(value,running);check(running,"Isolated Rice input closed view");
}
void view_key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(view,value);
}
Point window_point(SDL_Renderer* renderer,Point point) {
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y),
        "Invert production Rice input transform");return {x,y};
}
void move(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    const auto raw=window_point(renderer,point);SDL_Event value{};value.type=SDL_EVENT_MOUSE_MOTION;
    value.motion.x=static_cast<float>(raw.x);value.motion.y=static_cast<float>(raw.y);event(view,value);view.update(0);
}
void button(oe::SandboxView& view,SDL_Renderer* renderer,Point point,bool down) {
    const auto raw=window_point(renderer,point);SDL_Event value{};
    value.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    value.button.button=SDL_BUTTON_LEFT;value.button.x=static_cast<float>(raw.x);value.button.y=static_cast<float>(raw.y);event(view,value);
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    move(view,renderer,point);button(view,renderer,point,true);button(view,renderer,point,false);view.update(0);
}
Point screen_cell(const oe::SandboxView& view,sim::Cell cell) {
    return view.camera().world_to_screen(oe::maps::terrain_ground(
        {static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},72));
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;const auto raw=window_point(renderer,{map.x+map.w*.5,map.y+map.h*.5});
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_WHEEL;
    value.wheel.mouse_x=static_cast<float>(raw.x);value.wheel.mouse_y=static_cast<float>(raw.y);
    value.wheel.y=static_cast<float>(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,value);view.update(0);
    check(std::abs(view.camera().zoom-wanted)<1e-6,"Actual Rice wheel input missed requested literal zoom");
}
void center(oe::SandboxView& view,Point world) {
    const auto map=view.layout().map;const auto scale=view.camera().zoom;
    const Point wanted{map.x+map.w*.5-world.x*scale,map.y+map.h*.5-world.y*scale};
    for(unsigned step=0;step<1000;++step) {
        const auto current=view.camera().offset;const auto dx=wanted.x-current.x,dy=wanted.y-current.y;
        if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused Rice camera input did not converge");
}
Surface frame(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path={}) {
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Read Rice output size");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target&&SDL_SetRenderTarget(renderer,target.get()),"Rice capture target");
    check(view.render(),"Render actual Rice frame");
    Surface raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(raw!=nullptr,"Read actual Rice glyph pixels");
    Surface surface(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    check(surface!=nullptr,"Normalize Rice readback to the source RGBA byte layout");
    if(!path.empty())check(SDL_SavePNG(surface.get(),path.string().c_str()),"Save Rice capture");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore Rice output target");return surface;
}
Color pixel(const SDL_Surface& surface,int x,int y) {
    check(x>=0&&y>=0&&x<surface.w&&y<surface.h,"Rice pixel outside output");
    const auto* p=static_cast<const std::uint8_t*>(surface.pixels)+y*surface.pitch+x*4;
    return {p[0],p[1],p[2],p[3]};
}
Color pixel(const oe::assets::RgbaImage& image,int x,int y) {
    const auto at=(static_cast<std::size_t>(y)*image.width+static_cast<std::size_t>(x))*4;
    return {image.pixels.at(at),image.pixels.at(at+1),image.pixels.at(at+2),image.pixels.at(at+3)};
}
void no_food(const sim::World& world) {
    check(!sim::agriculture_production_clock_verified,"Rice art activated a production clock");
    const auto& state=*world.original_agriculture_state();
    for(const auto& field:state.fields)check(!field.quantity&&!field.productive_events&&!field.harvested&&!field.last_harvest_cycle,
        "Rice presentation changed quantity, tending or harvest");
    for(const auto& stock:state.stocks)check(stock.amounts==sim::AgricultureAmounts{} && stock.received==sim::AgricultureCounters{} &&
        stock.departed==sim::AgricultureCounters{}&&stock.returned==sim::AgricultureCounters{}&&stock.consumed==sim::AgricultureCounters{},
        "Rice presentation changed typed physical Food");
    check(state.trips.empty()&&world.original_production_state()->food==sim::OriginalFoodState{},"Rice art created transport or mirrored Food");
}
void rice_render_contract(SDL_Window* window,SDL_Renderer* renderer,const fs::path& data,const fs::path& output,
                          const std::optional<fs::path>& real_root) {
    const auto manifest=rice_profile(data,output,real_root);
    auto session=oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
        oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
    auto isolated_view=oe::SandboxView::create_original_agriculture(std::move(session),std::nullopt,false);
    const auto working=output/"rice-fields.save.json";
    isolated_view->configure_save(data,"Cities/A.map",working);
    isolated_view->set_original_building_visuals(manifest,oe::VisualProfileSource::Custom);
    isolated_view->initialize(window,renderer);auto& view=*isolated_view;
    SDL_Event focused{};focused.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,focused);
    check(view.world().buildings().empty(),"Isolated Rice visual fixture inherited a paid starter");
    oe::assets::BuildingVisualProfile prepared;
    oe::assets::append_original_building_visual_profile(data,manifest,prepared);
    check(prepared.original_rice_prepared&&prepared.unique_images.size()==47,"Rice bank was not eagerly prepared/deduplicated");
    for(const auto [mask,offset]:std::array{std::pair{0U,0U},std::pair{4U,2U},std::pair{64U,4U},std::pair{8U,0U},
            std::pair{255U,46U},std::pair{5U,5U},std::pair{7U,9U},std::pair{20U,6U},std::pair{28U,10U},std::pair{85U,27U},std::pair{87U,32U}}) {
        const auto* entry=prepared.find_original_rice(static_cast<std::uint8_t>(mask));
        const auto* base=prepared.find_original_rice(0);
        check(entry&&base&&entry->id.image_index==base->id.image_index+offset,"Rice canonical source topology offset differs");
    }
    check(!prepared.find_original_rice(0,2,0)&&!prepared.find_original_rice(0,1,1),"Unsupported Rice side/quantity selected native art");
    const auto purchase=[&](sim::CommandType type,sim::Cell cell,sim::BuildingId farm=sim::BuildingId{}) {
        sim::Command command{type,cell};command.building_id=farm;const auto result=view.execute(command);
        check(result.accepted&&result.changed,"Paid Rice fixture placement refused at "+std::to_string(cell.x)+","+std::to_string(cell.y)+": "+result.reason);
        return *view.world().building_owner_at(cell);
    };
    const auto farm_a=purchase(sim::CommandType::PlaceOriginalFoodFarm,{104,111});
    const auto farm_b=purchase(sim::CommandType::PlaceOriginalFoodFarm,{112,111});
    std::vector<std::pair<sim::Cell,sim::BuildingId>> fields;
    const auto rice=[&](sim::Cell cell,sim::BuildingId farm) {
        const auto id=purchase(sim::CommandType::PlaceOriginalRiceField,cell,farm);fields.emplace_back(cell,id);return id;
    };
    const auto isolated=rice({108,115},farm_a);
    rice({101,108},farm_a);rice({102,108},farm_a);rice({102,109},farm_a); // L-shaped corner.
    for(int y=114;y<=115;++y)for(int x=101;x<=102;++x)rice({x,y},farm_a); // Full rectangle.
    for(int y=108;y<=110;++y)for(int x=107;x<=109;++x)if(x!=108||y!=109)rice({x,y},farm_a); // Hole.
    const auto cross_a=rice({109,113},farm_a),cross_b=rice({110,113},farm_b);
    const auto verify=[&](bool fixed_shapes=true) {
        const auto stats=view.original_rice_display_stats();
        check(stats.configured&&stats.active&&stats.prepared_fields==fields.size()&&stats.prepared_fields<=32&&
            stats.prepared_preview_cells<=256&&stats.prepared_preview_cells<=fields.size()*8&&
            stats.revision==view.world().road_revision(),"Rice cache lost its bounded World revision publication");
        constexpr std::array<sim::Cell,8> neighbors{{{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}}};
        for(const auto& [cell,id]:fields) {
            unsigned mask=0;
            for(unsigned direction=0;direction<neighbors.size();++direction)
                for(const auto& [other,other_id]:fields) {
                    (void)other_id;
                    if(other==sim::Cell{cell.x+neighbors[direction].x,cell.y+neighbors[direction].y})mask|=1U<<direction;
                }
            const auto d=view.original_building_visual_diagnostic(id);const auto* selected=prepared.find_original_rice(static_cast<std::uint8_t>(mask));
            check(d.active&&d.asset&&selected&&d.rice_neighbor_mask==mask&&*d.asset==selected->id,
                "Paid Rice shape did not select its prepared eight-neighbor native glyph");
            const auto& native=prepared.unique_images.at(selected->image_index);
            check(d.image_width==78&&d.image_width==native.width&&d.image_height==native.height&&
                d.ground_x==39&&d.ground_y==native.height-20&&d.ground_y==selected->ground_y,
                "Rice native image dimensions or complete front-cell anchor changed");
        }
        if(fixed_shapes)check(view.original_building_visual_diagnostic(isolated).rice_neighbor_mask==0&&
            view.original_building_visual_diagnostic(cross_a).rice_neighbor_mask==4&&
            view.original_building_visual_diagnostic(cross_b).rice_neighbor_mask==64,
            "Isolated or cross-Farm Rice adjacency differs from same-kind geometry");
        no_food(view.world());
    };
    verify();check(view.execute(sim::set_building_operation(farm_a,false)).accepted,"Pause actual Rice Farm");verify();
    view.set_tool(5);
    if(view.debug_diagnostics())view_key(view,SDLK_F1);
    zoom(view,renderer,2);
    for(const auto& [name,cell]:std::array{std::pair{"rice-isolated-2x",sim::Cell{108,115}},
            std::pair{"rice-corner-2x",sim::Cell{102,108}},std::pair{"rice-rectangle-2x",sim::Cell{101,114}},
            std::pair{"rice-hole-2x",sim::Cell{108,109}},std::pair{"rice-cross-farm-2x",sim::Cell{110,113}}}) {
        center(view,oe::maps::terrain_ground({static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},72));
        frame(view,renderer,output/(std::string(name)+".png"));
    }
    zoom(view,renderer,.5);center(view,oe::maps::terrain_ground({107,112},72));
    view.set_tool(43);move(view,renderer,screen_cell(view,{115,116}));
    check(view.original_rice_display_stats().preview_cell==sim::Cell{115,116}&&
        view.original_rice_display_stats().preview_mask==0,
        "Unlisted Rice ghost cell did not use the prepared sparse cache's mask0 default");
    const sim::Cell ghost{108,109};const auto ghost_point=screen_cell(view,ghost);
    check(view.layout().map.contains(ghost_point.x,ghost_point.y),"Rice ghost lies outside map aperture");
    const auto before_preview=view.world().snapshot();move(view,renderer,ghost_point);
    const auto preview=view.original_rice_display_stats();
    check(preview.preview_cell==ghost&&preview.preview_mask==255&&preview.preview_asset==prepared.find_original_rice(255)->id,
        "Hole-filling Rice ghost did not use its separately prepared full-neighbor cache");
    check(view.world().snapshot()==before_preview&&preview.prepared_fields==fields.size(),"Rice ghost changed paid topology or World");
    button(view,renderer,ghost_point,true);view_key(view,SDLK_ESCAPE);
    check(view.world().snapshot()==before_preview&&!view.original_rice_display_stats().preview_cell,
        "Escape committed or retained the cancelled Rice ghost cache");
    button(view,renderer,ghost_point,false);view.set_tool(5);
    check(view.world().snapshot()==before_preview&&!view.original_rice_display_stats().preview_cell,
        "Cancelled mouse release or tool change committed Rice ghost authority");
    view.set_tool(43);move(view,renderer,ghost_point);click(view,renderer,ghost_point);
    check(view.world().building_owner_at(ghost).has_value(),"Actual Rice click did not commit its paid field");
    fields.emplace_back(ghost,*view.world().building_owner_at(ghost));verify();
    view.set_tool(5);frame(view,renderer,output/"rice-topologies-overview.png");
    zoom(view,renderer,1);center(view,view.original_building_visual_diagnostic(isolated).ground);
    view_key(view,SDLK_F1);check(view.debug_diagnostics(),"Rice alpha-picking mode did not activate");
    auto shown=frame(view,renderer,output/"rice-topologies.png");
    const auto d=view.original_building_visual_diagnostic(isolated);const auto* entry=prepared.find_original_rice(0);
    const auto& image=prepared.unique_images.at(entry->image_index);const auto origin=view.camera().world_to_screen(d.image_origin);
    std::optional<Point> opaque,hole;unsigned visible_source=0,transparent_source=0;Json alpha_attempts=Json::array();
    for(int y=1;y<image.height-1&&(!opaque||!hole);++y)for(int x=1;x<image.width-1&&(!opaque||!hole);++x) {
        const auto color=pixel(image,x,y);const Point point{origin.x+(x+.5)*view.camera().zoom,origin.y+(y+.5)*view.camera().zoom};
        if(!view.layout().map.contains(point.x,point.y))continue;
        if(!opaque&&color[3]==255&&pixel(*shown,static_cast<int>(point.x),static_cast<int>(point.y))==color) {
            ++visible_source;click(view,renderer,point);if(view.selected_building()==isolated)opaque=point;
            else if(alpha_attempts.size()<8)alpha_attempts.push_back({{"source",{x,y}},{"screen",{point.x,point.y}},
                {"selected",view.selected_building() ? Json(static_cast<unsigned>(*view.selected_building())):Json(nullptr)},
                {"landscape",view.input_diagnostic_state().selected_landscape}});
        }
        if(!hole&&color[3]==0) {++transparent_source;click(view,renderer,point);if(view.selected_building()!=isolated)hole=point;}
    }
    q::f::write_text(output/"rice-alpha-diagnostic.json",Json{{"visible_source",visible_source},{"transparent_source",transparent_source},
        {"opaque",opaque.has_value()},{"hole",hole.has_value()},{"zoom",view.camera().zoom},{"origin",{origin.x,origin.y}},
        {"field_order_adjusted",d.field_order_adjusted},{"field_order_ambiguous",d.field_order_ambiguous},{"attempts",alpha_attempts}}.dump(2));
    check(opaque&&hole,"Rice native glyph lacks source-alpha pick witnesses: visible="+std::to_string(visible_source)+
        " transparent="+std::to_string(transparent_source)+" opaque="+std::to_string(opaque.has_value())+" hole="+std::to_string(hole.has_value()));
    view_key(view,SDLK_F4);const auto fallback_before=view.original_building_display_stats();
    auto fallback=frame(view,renderer,output/"rice-f4-fallback.png");const auto fallback_after=view.original_building_display_stats();
    const auto slot=static_cast<std::size_t>(oe::assets::OriginalBuildingVisualRole::RiceField);
    check(!view.original_building_visual_diagnostic(isolated).active&&
        fallback_after.authored_fallbacks[slot]>fallback_before.authored_fallbacks[slot]&&
        pixel(*shown,static_cast<int>(opaque->x),static_cast<int>(opaque->y))!=pixel(*fallback,static_cast<int>(opaque->x),static_cast<int>(opaque->y)),
        "F4 did not distinguish an actual Rice sprite from the authored fallback");
    view_key(view,SDLK_F4);verify();
    const auto before_profile_switch=view.world().snapshot();auto historical=Json::parse(q::f::read_text(manifest));
    historical["schema_version"]=5;historical.erase("rice_selection");
    for(auto& row:historical["buildings"])if(row["model_type"]==197) {
        row["native"]=false;row.erase("archive");row.erase("group_key");row.erase("tiles");
    }
    const auto old_manifest=output/"rice-historical-schema5.json";q::f::write_text(old_manifest,historical.dump(2));
    view.set_original_building_visuals(old_manifest,oe::VisualProfileSource::Custom);
    check(!view.original_rice_display_stats().configured&&!view.original_building_visual_diagnostic(isolated).active,
        "Historical schema5 silently acquired new native Rice rendering");
    frame(view,renderer,output/"rice-schema5-fallback.png");
    view.set_original_building_visuals(manifest,oe::VisualProfileSource::Custom);verify();
    check(view.world().snapshot()==before_profile_switch,"Rice profile toggling changed paid World authority");
    view_key(view,SDLK_F1);view.set_tool(43);move(view,renderer,screen_cell(view,{103,109}));
    check(view.render(),"Warm Rice prepared/preview frame");const auto frozen=view.world().snapshot();
    const auto cache=view.original_rice_display_stats();oe::performance::set_enabled(true);oe::performance::reset();
    for(int n=0;n<50;++n){view.update(0);check(view.render(),"Pure Rice frame failed");}
    for(std::size_t index=0;index<static_cast<std::size_t>(oe::performance::Counter::Count);++index)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(index)),"Pure Rice frame performed forbidden World/I/O/asset/route work");
    oe::performance::set_enabled(false);
    check(view.world().snapshot()==frozen&&view.original_rice_display_stats().revision==cache.revision&&
        view.original_rice_display_stats().preview_mask==cache.preview_mask,"Pure Rice frame mutated World or prepared topology");
    view.set_tool(5);view_key(view,SDLK_F5);const auto bytes=q::f::read_text(working);
    view.tick_once();view_key(view,SDLK_F9);check(view.world().snapshot()==frozen,"Rice F9 changed paid fields or paused source state");
    verify();view_key(view,SDLK_F5);check(q::f::read_text(working)==bytes,"Rice F5/F9 canonical bytes differ");
    no_food(view.world());
    // Exercise the exact prepared-row bound without admitting an extra field.
    for(int y=108;y<=116&&fields.size()<32;++y)for(int x=101;x<=109&&fields.size()<32;++x) {
        const sim::Cell cell{x,y};if(view.world().object_at(cell)!=sim::Object::Empty)continue;
        sim::Command command{sim::CommandType::PlaceOriginalRiceField,cell};command.building_id=farm_a;
        if(view.world().validate(command).accepted)rice(cell,farm_a);
    }
    check(fields.size()==32,"Paid Rice fixture did not reach the unchanged32-field cache bound");verify(false);
    const auto limit_state=view.world().snapshot();bool refused=false;
    for(int y=108;y<=116&&!refused;++y)for(int x=101;x<=109&&!refused;++x) {
        const sim::Cell cell{x,y};if(view.world().object_at(cell)!=sim::Object::Empty)continue;
        sim::Command command{sim::CommandType::PlaceOriginalRiceField,cell};command.building_id=farm_a;
        const auto result=view.execute(command);refused=!result.accepted&&!result.changed;
    }
    check(refused&&view.world().snapshot()==limit_state,"Over-limit Rice field changed World, IDs or funds");
    frame(view,renderer,output/"rice-cache-limit32.png");no_food(view.world());view.shutdown();
}
void pure_contracts() {
    using A=oe::sandbox_ui::Action;
    const auto legacy=oe::sandbox_ui::make_layout(1280,800,1280,800,true,true,true,true,true,true,true,true,true,true);
    const auto agriculture=oe::sandbox_ui::make_layout(1280,800,1280,800,
        true,true,true,true,true,true,true,true,true,true,false,false,false,true);
    const auto contains=[](const auto& layout,A action) {return std::any_of(layout.buttons.begin(),layout.buttons.end(),[&](const auto& b){return b.action==action;});};
    for(const auto action:{A::OriginalRiceField,A::OriginalCabbageField,A::OriginalSoybeanField}) {
        check(!contains(legacy,action)&&contains(agriculture,action),"New crop tool leaked into historical layout or is missing");
        const auto button=std::find_if(agriculture.buttons.begin(),agriculture.buttons.end(),[&](const auto& b){return b.action==action;});
        check(button->rect.w>0&&button->rect.h>0&&agriculture.button_at(button->rect.x+1,button->rect.y+1)==action,"Crop drawing/input rectangles differ");
    }
    using oe::assets::agriculture_field_quantity_stage;
    check(agriculture_field_quantity_stage(0)==0&&agriculture_field_quantity_stage(99)==0&&
        agriculture_field_quantity_stage(100)==1&&agriculture_field_quantity_stage(1999)==1&&
        agriculture_field_quantity_stage(2000)==2&&agriculture_field_quantity_stage(8000)==5&&
        agriculture_field_quantity_stage(10000)==5&&!agriculture_field_quantity_stage(10001),"Verified quantity visual boundary differs");
    q::Fixture fixture;
    const auto historical=oe::persistence::prepare_original_production_rules_model(fixture.root.data);
    for(const auto building:{oe::original::ModelBuilding::RiceField,oe::original::ModelBuilding::CabbageField,oe::original::ModelBuilding::SoybeanField})
        check(!historical->supports_building(building)&&fixture.model->supports_building(building),"Historical model gained Agriculture capability");
    check(fixture.model->building_value(oe::original::ModelBuilding::RiceField,oe::original::BuildingField::Cost)==7,
        "Rice prepared model cost differs from independent authored fixture");
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==1 || (argc==3&&std::string_view(argv[1])=="--real"),"Expected optional --real ORIGINAL_DATA_ROOT");
        const std::optional<fs::path> real_root=argc==3 ? std::optional{fs::canonical(argv[2])}:std::nullopt;
        pure_contracts();
        oe::testing::economy::Temp temp;
        struct PrivateCopy {
            fs::path root;
            ~PrivateCopy(){if(!root.empty()){std::error_code ignored;fs::remove_all(root,ignored);}}
        } private_copy;
        if(real_root) {
            const auto parent=repository_root()/".local/agri-02/rice-presentation";fs::create_directories(parent);
            private_copy.root=parent/("private-input-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            check(fs::create_directory(private_copy.root),"Reserve private real-asset input fixture");
            fs::copy(temp.root/"data",private_copy.root/"data",fs::copy_options::recursive);
        }
        const auto work=real_root ? private_copy.root:temp.root;
        const auto data=work/"data",app=work/"app";
        const auto source_general=real_root ? sha256(read_bytes(*real_root/"DATA/China_General.sg3")):std::string{};
        const auto source_pixels=real_root ? sha256(read_bytes(*real_root/"DATA/China_General.555")):std::string{};
        oe::testing::economy::write(data/"Cities/A.map",oe::testing::economy::map_file(true,true,true));
        fs::create_directories(data/"Model");
        q::f::write_text(data/"Model/EmperorBuildingModels.txt",q::authored_model());
        const auto source=q::f::read_text(data/"Cities/A.map");
        oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/A.map";
        oe::menu::write_settings(app,settings);
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");check(SDL_Init(SDL_INIT_VIDEO),"Initialize scripted SDL");
        check(SDL_CreateWindowAndRenderer("Agriculture presentation",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create Agriculture window");
        check(SDL_GetRendererName(renderer)&&std::string_view(SDL_GetRendererName(renderer))=="software",
            "Isolated Rice presentation requires the requested software renderer");
        Menu menu({},app,std::make_unique<Dialog>());menu.initialize(window,renderer);
        check(!menu.original_agriculture_setup()&&!menu.diagnostic_button_rect("A Agriculture (rule 25)"),"Agriculture became a default");
        key(menu,SDLK_U);wait_map(menu,18);key(menu,SDLK_ESCAPE);
        key(menu,SDLK_A);wait_map(menu,25);
        check(menu.original_agriculture_setup(),"A did not explicitly select Agriculture");
        key(menu,SDLK_RETURN);check(menu.state()==Menu::State::Loading&&menu.render(),"Present deferred Loading");menu.advance();
        check(menu.state()==Menu::State::Playing&&menu.sandbox(),"Agriculture candidate failed");
        auto& view=*menu.sandbox();check(view.world().original_agriculture_enabled()&&view.paused(),"Wrong active mode or unpaused startup");
        check(!sim::agriculture_production_clock_verified && view.last_message().find("AUTHORED Wheat/Millet; Rice/Cabbage/Soy locked")!=std::string::npos,
            "Rule25 lacks the explicit authored-food/locked-crops boundary");
        key(menu,SDLK_R,SDL_KMOD_SHIFT);check(view.tool()==43,"Rice keyboard action differs from shared tool path");
        key(menu,SDLK_C,SDL_KMOD_SHIFT);check(view.tool()==44,"Cabbage keyboard action differs from shared tool path");
        key(menu,SDLK_S,SDL_KMOD_SHIFT);check(view.tool()==45,"Soy keyboard action differs from shared tool path");
        const auto before=view.world().snapshot();for(int frame=0;frame<30;++frame)check(menu.render(),"Pure Agriculture frame failed");
        check(view.world().snapshot()==before,"Presentation changed paid Agriculture state");
        key(menu,SDLK_F5);const auto document=oe::persistence::read_save(menu.settings().last_save);
        check(document.source_schema_version==41&&document.world.rule_version==25,"Actual F5 saved wrong generation");
        const auto bytes=q::f::read_text(menu.settings().last_save);view.tick_once();key(menu,SDLK_F9);
        check(view.world().snapshot()==before&&q::f::read_text(menu.settings().last_save)==bytes,"F9 changed generation or source bytes");
        const auto output=repository_root()/".local/agri-02/presentation";fs::create_directories(output);
        capture(menu,renderer,output/(real_root ? "agriculture-1280-real.png":"agriculture-1280.png"));
        const auto rice_output=repository_root()/".local/agri-02/rice-presentation"/(real_root ? "real":"hermetic");fs::create_directories(rice_output);
        rice_render_contract(window,renderer,data,rice_output,real_root);
        check(q::f::read_text(data/"Cities/A.map")==source,"UI changed original-map fixture bytes");
        if(real_root)check(sha256(read_bytes(*real_root/"DATA/China_General.sg3"))==source_general&&
            sha256(read_bytes(*real_root/"DATA/China_General.555"))==source_pixels,"Real Rice inspection changed supplied asset bytes");
        menu.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Agriculture explicit menu, typed tools, pure render, model capability, F5/F9, Rice topology/glyph/preview/alpha/cache and quantity selector PASS\n";return 0;
    } catch(const std::exception& error) {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
