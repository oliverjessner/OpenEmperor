#include "core/PerformanceDiagnostics.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "renderer/BuildingSprite.h"
#include "renderer/RoadSpriteSet.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "renderer/TextureCompatibility.h"
#include "renderer/WalkerSpriteSet.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace assets=openemperor::assets;
namespace maps=openemperor::maps;
namespace scene=openemperor::scene;
namespace perf=openemperor::performance;
using Color=std::array<std::uint8_t,4>;
using Bytes=std::vector<std::uint8_t>;
constexpr Color background{16,32,48,255},red{255,0,0,255},green{0,255,0,255},blue{0,0,255,255};
constexpr int width=640,height=480;
constexpr SDL_Rect clip{24,18,592,444};
void check(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
Color blend(Color source,Color destination,unsigned modulation=255) {
    const unsigned alpha=unsigned(source[3])*modulation/255;
    Color result{};
    for (std::size_t n=0;n<3;++n)
        result[n]=std::uint8_t((unsigned(source[n])*alpha+unsigned(destination[n])*(255-alpha))/255);
    result[3]=255;return result;
}
bool close(Color a,Color b) {
    for(std::size_t n=0;n<4;++n) if(std::abs(int(a[n])-int(b[n]))>2) return false;
    return true;
}
struct Surface {
    SDL_Surface* value=nullptr;
    explicit Surface(SDL_Surface* s):value(s) {}
    ~Surface(){SDL_DestroySurface(value);}
    Surface(const Surface&)=delete;
};
struct Context {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    explicit Context(const char* backend="software") {
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,std::string(backend)=="software" ? "dummy":"cocoa")&&SDL_Init(SDL_INIT_VIDEO),"SDL setup");
        window=SDL_CreateWindow("authored texture compatibility",width,height,SDL_WINDOW_HIDDEN);
        check(window,"window");
        renderer=SDL_CreateRenderer(window,backend);check(renderer,"explicit renderer");
        check(std::string(SDL_GetRendererName(renderer))==backend,"actual renderer matches requested backend");
        std::cout<<"SDL headers="<<SDL_MAJOR_VERSION<<"."<<SDL_MINOR_VERSION<<"."<<SDL_MICRO_VERSION
            <<" runtime="<<SDL_GetVersion()<<" revision="<<SDL_GetRevision()
            <<" backend="<<SDL_GetRendererName(renderer)<<" driver="<<SDL_GetCurrentVideoDriver()
            <<" size="<<width<<"x"<<height<<" clip=24,18,592,444 format=RGBA32"
            <<" blend=BLEND scale=NEAREST"<<'\n';
    }
    ~Context() {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    }
    void clear() const {
        check(SDL_SetRenderClipRect(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&
            SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderDrawColor(renderer,
                background[0],background[1],background[2],255)&&SDL_RenderClear(renderer)&&
            SDL_SetRenderClipRect(renderer,&clip),"clear frame and map clip");
    }
    Color pixel(SDL_Surface* surface,scene::Point point) const {
        Color color{};
        check(SDL_ReadSurfacePixel(surface,int(point.x),int(point.y),
            &color[0],&color[1],&color[2],&color[3]),"sample authored interior");
        return color;
    }
    std::uint64_t hash() const {
        Surface source(SDL_RenderReadPixels(renderer,nullptr));check(source.value,"frame readback");
        Surface rgba(SDL_ConvertSurface(source.value,SDL_PIXELFORMAT_RGBA32));check(rgba.value,"RGBA frame");
        std::uint64_t value=1469598103934665603ULL;
        for(int y=0;y<height;++y) for(int x=0;x<width*4;++x) {
            value^=static_cast<const std::uint8_t*>(rgba.value->pixels)[y*rgba.value->pitch+x];
            value*=1099511628211ULL;
        }
        return value;
    }
};
struct Shape {
    assets::RgbaImage image;
    scene::Point opaque,hole,half;
    Color color;
    bool holes=false,half_alpha=false;
};
Shape shape(unsigned w,unsigned h,Color color,bool holes,bool half_alpha) {
    Shape s;s.image.width=std::uint16_t(w);s.image.height=std::uint16_t(h);
    s.image.pixels.assign(std::size_t(w)*h*4,0);s.color=color;
    // Widely separated authored interior regions avoid raster-edge assertions.
    const unsigned body_y=h-12;
    s.opaque={double(w/4),double(body_y)};
    s.hole={double(w/2),double(body_y)};
    s.half={double(3*w/4),double(body_y)};
    s.holes=holes;s.half_alpha=half_alpha;
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        Color c=color;
        if(holes && (x<3 || x>=w-3 || y<3 || y>=h-3 ||
            (x>=w/2-4 && x<=w/2+4 && y>=body_y-4 && y<=body_y+4))) c[3]=0;
        if(half_alpha && x>=3*w/4-4 && x<=3*w/4+4 && y>=body_y-4 && y<=body_y+4) c[3]=128;
        const auto p=(std::size_t(y)*w+x)*4;
        std::copy(c.begin(),c.end(),s.image.pixels.begin()+std::ptrdiff_t(p));
    }
    return s;
}
scene::Point origin_for(scene::Point sample,double zoom,scene::Point screen={320.25,160.25}) {
    return {screen.x-(sample.x+0.5)*zoom,screen.y-(sample.y+0.5)*zoom};
}
scene::Point screen_sample(scene::Point origin,scene::Point sample,double zoom) {
    return {origin.x+(sample.x+0.5)*zoom,origin.y+(sample.y+0.5)*zoom};
}
void pure_counters() {
    for(auto c:{perf::Counter::FileReads,perf::Counter::FileWrites,perf::Counter::AssetDecodes,
        perf::Counter::TextureUploads,perf::Counter::WorldCopies,perf::Counter::WorldExecutes,
        perf::Counter::BfsCalls,perf::Counter::RouteRefreshes})
        check(perf::counter(c)==0,"render-only resource/authority counter stays zero");
}
struct Sample {scene::Point source;Color expected;};
// All frames reuse the caller's initialized texture set. No readback per draw.
void exercise(Context& c,const std::string& name,scene::Point anchor,
    const std::vector<Sample>& samples,const std::function<void(scene::Point,double)>& draw,
    const std::function<void(scene::Point,double)>& hit={}) {
    const auto frame=[&](double zoom,scene::Point pivot,bool inspect,bool hash_frame=false) {
        c.clear();const auto origin=origin_for(anchor,zoom,pivot);SDL_ClearError();draw(origin,zoom);
        std::uint64_t frame_hash=0;
        if(inspect) {
            Surface output(SDL_RenderReadPixels(c.renderer,nullptr));check(output.value,name+" readback");
            for(const auto& sample:samples) {
                const auto p=screen_sample(origin,sample.source,zoom);
                if(p.x<clip.x+2 || p.y<clip.y+2 || p.x>=clip.x+clip.w-2 || p.y>=clip.y+clip.h-2) continue;
                const auto actual=c.pixel(output.value,p);
                check(close(actual,sample.expected),name+" opaque/hole/alpha pixel zoom="+
                    std::to_string(zoom)+" actual="+std::to_string(actual[0])+","+
                    std::to_string(actual[1])+","+std::to_string(actual[2]));
            }
            if(hit) hit(origin,zoom);
        }
        if(hash_frame) frame_hash=c.hash();
        // Never inspect retained backbuffer contents after Present.
        check(SDL_RenderPresent(c.renderer),name+" present");return frame_hash;
    };
    // At 1x the complete image begins 0.5 px inside the map clip. Fractional
    // scaling crosses its left edge, while the opaque witness stays interior.
    const scene::Point home{double(clip.x)+anchor.x+1,270.25};
    const auto reference=frame(1,home,true,true);
    for(int repeat=0;repeat<100;++repeat)
        for(double zoom:{1.0,1.125,1.0}) frame(zoom,home,true);
    for(double zoom:{1.0,1.25,1.5,1.0,2.0,4.0,1.0})
        for(int repeat=0;repeat<3;++repeat) frame(zoom,home,true);
    for(const auto pivot:std::array<scene::Point,7>{{{32.25,160.25},{607.25,160.25},
        {320.25,26.25},{320.25,453.25},{-800,-800},{1400,1400},home}})
        for(double zoom:{1.0,1.125,4.0}) frame(zoom,pivot,true);
    for(int repeat=0;repeat<100;++repeat)
        for(double zoom:{1.0,1.125,1.0}) frame(zoom,home,false);
    check(frame(1,home,true,true)==reference,name+" complete return frame equality");
    pure_counters();
}
std::vector<Sample> samples(const Shape& s,unsigned modulation=255) {
    std::vector<Sample> result{{s.opaque,blend(s.color,background,modulation)}};
    if(s.holes) result.push_back({s.hole,background});
    if(s.half_alpha) {auto color=s.color;color[3]=128;result.push_back({s.half,blend(color,background,modulation)});}
    return result;
}
void building_case(Context& c,bool stages,bool previews=false) {
    assets::BuildingVisualProfile p;
    const auto s=shape(stages ? 158U:78U,238,stages ? green:red,stages,stages);
    p.unique_images.push_back(s.image);
    p.entries[assets::role_index(assets::BuildingVisualRole::Farm)]=
        assets::BuildingVisualEntry{{"authored",1},0,39,218,1,"authored"};
    p.entries[assets::role_index(assets::BuildingVisualRole::Market)]=p.entries[assets::role_index(assets::BuildingVisualRole::Farm)];
    if(stages) for(unsigned level=0;level<3;++level) {
        auto v=s.image;if(level) {for(std::size_t n=0;n<v.pixels.size();n+=4)
            if(v.pixels[n+3]) {v.pixels[n]=level==1 ? 255:0;v.pixels[n+1]=0;v.pixels[n+2]=level==2 ? 255:0;}
            p.unique_images.push_back(std::move(v));}
        p.entries[assets::role_index(assets::household_stage_roles[level])]=
            assets::BuildingVisualEntry{{"authored",level+1},level,79,218,2,"authored stage"};
    }
    const auto before=openemperor::BuildingSprite::live_texture_count();
    openemperor::BuildingSprite sprites;sprites.initialize(c.renderer,p);
    check(sprites.texture_count()==(stages ? 3U:1U),"shared/prepared building texture count");
    perf::reset();
    for(unsigned level=0;level<(stages ? 3U:1U);++level) {
    const auto& entry=stages ? *p.find(p.household_role(level)):*p.find(assets::BuildingVisualRole::Farm);
    auto selected=s;selected.color=stages ? (level==0 ? green:level==1 ? red:blue):red;
    exercise(c,stages ? "building-house-stage-"+std::to_string(level):"building-shared",s.opaque,samples(selected),
        [&](scene::Point top,double z) {
            // Preview shares the exact physical texture, then normal draw must
            // restore full alpha. The separate normal instance is checked too.
            const scene::Point ground{top.x+entry.ground_x*z,top.y+entry.ground_y*z};
            if(previews) check(sprites.draw({ground.x-300*z,ground.y},z,p,entry,true),"building preview");
            check(sprites.draw(ground,z,p,entry),"normal after building preview");
            check(sprites.draw({ground.x+300*z,ground.y},z,p,entry),"second shared building");
        });
    check(sprites.texture_count()==p.unique_images.size(),"stage selection keeps prepared texture count");
    }
    // A standalone preview must blend once, with no alpha leaking to the next frame.
    const auto& preview_entry=stages ? *p.find(p.household_role(0)):*p.find(assets::BuildingVisualRole::Farm);
    c.clear();auto top=origin_for(s.opaque,1);check(sprites.draw({top.x+preview_entry.ground_x,top.y+preview_entry.ground_y},1,p,preview_entry,true),"preview-only building");
    {Surface out(SDL_RenderReadPixels(c.renderer,nullptr));check(out.value,"building preview readback");
     check(close(c.pixel(out.value,screen_sample(top,s.opaque,1)),blend(s.color,background,128)),"building preview alpha128");}
    if(stages) for(unsigned level=0;level<3;++level) {
        c.clear();const auto& selected=*p.find(p.household_role(level));
        check(sprites.draw({top.x+selected.ground_x,top.y+selected.ground_y},1,p,selected),"prepared house stage switch");
        Surface out(SDL_RenderReadPixels(c.renderer,nullptr));check(out.value,"house stage readback");
        const Color expected=level==0 ? green:level==1 ? red:blue;
        check(close(c.pixel(out.value,screen_sample(top,s.opaque,1)),expected),"stage prepared pixels selected");
    }
    pure_counters();sprites.shutdown();check(openemperor::BuildingSprite::live_texture_count()==before,"building shutdown releases owned textures");
}
void walker_case(Context& c,bool small) {
    const auto s=shape(small ? 24U:78U,small ? 36U:238U,blue,true,true);
    assets::WalkerVisualProfile p;p.unique_images={s.image,s.image};
    assets::WalkerRoleVisual role;
    role.frames={{"first",{"authored",1},0,0,0},{"second",{"authored",2},0,0,1}};
    role.clips[0]={0,1};p.roles[0]=role;
    const auto before=openemperor::WalkerSpriteSet::live_texture_count();
    openemperor::WalkerSpriteSet sprites;sprites.initialize(c.renderer,p);check(sprites.texture_count()==2,"prepared walker frames");
    perf::reset();std::size_t n=0;
    exercise(c,small ? "walker-small":"walker-large",s.opaque,samples(s),
        [&](scene::Point top,double z) {
            check(sprites.draw(n++%2,top,z,role,p,{double(clip.x),double(clip.y)},
                {double(clip.x+clip.w),double(clip.y+clip.h)}),"walker frame draw");
        });
    pure_counters();sprites.shutdown();check(openemperor::WalkerSpriteSet::live_texture_count()==before,"walker shutdown releases owned textures");
}
void road_case(Context& c,unsigned kind,bool previews=false) {
    const auto color=kind==0 ? red:kind==1 ? green:blue;
    const auto s=shape(78,40,color,true,false);
    assets::RoadVisualProfile p;p.replaces_ground=true;p.unique_images={s.image};
    const std::uint8_t mask=kind==0 ? 5:kind==1 ? 6:15;
    p.tiles[mask]=assets::RoadVisualEntry{{"authored",kind+1},0,39,20,"authored line/corner/cross"};
    const auto before=openemperor::RoadSpriteSet::live_texture_count();
    openemperor::RoadSpriteSet sprites;sprites.initialize(c.renderer,p);check(sprites.texture_count()==1,"road texture dedupe");
    perf::reset();const auto& entry=*p.find(mask);
    // At integer zoom RoadSpriteSet aligns the complete raster to pixels.
    // Use integer origins here; fractional zoom still uses fractional cameras.
    exercise(c,"road-"+std::to_string(mask),s.opaque,samples(s),
        [&](scene::Point top,double z) {
            const scene::Point ground{top.x+39*z,top.y+20*z};
            if(previews) check(sprites.draw({ground.x-300*z,ground.y},z,p,entry,true),"road preview");
            check(sprites.draw(ground,z,p,entry),"normal road after preview");
        });
    c.clear();auto top=origin_for(s.opaque,1);check(sprites.draw({top.x+39,top.y+20},1,p,entry,true),"road preview-only");
    {Surface out(SDL_RenderReadPixels(c.renderer,nullptr));check(out.value,"road preview readback");
     check(close(c.pixel(out.value,screen_sample(top,s.opaque,1)),blend(color,background,128)),"road preview alpha128");}
    pure_counters();sprites.shutdown();check(openemperor::RoadSpriteSet::live_texture_count()==before,"road shutdown releases owned textures");
}
void u16(Bytes& b,std::size_t p,unsigned n) {b.at(p)=std::uint8_t(n);b.at(p+1)=std::uint8_t(n>>8);}
void u32(Bytes& b,std::size_t p,unsigned n) {u16(b,p,n);u16(b,p+2,n>>16);}
void write(const std::filesystem::path& file,const Bytes& b) {
    std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));
    check(bool(out),"authored archive write");
}
void skip(Bytes& b,unsigned count) {while(count) {const auto n=std::min(count,254U);b.push_back(255);b.push_back(std::uint8_t(n));count-=n;}}
struct Archive {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("openemperor-texture-compat-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::array<unsigned,2> lengths{};
    Archive() {
        std::filesystem::create_directories(root/"DATA");Bytes sg3(40680+3*72,0),bitmap(4,0);
        u32(sg3,0,unsigned(sg3.size()));u32(sg3,4,214);u32(sg3,12,3);u32(sg3,16,3);u32(sg3,20,1);
        const std::string name="authored.bmp";std::copy(name.begin(),name.end(),sg3.begin()+680);u32(sg3,804,3);
        for(unsigned index=0;index<2;++index) {
            const unsigned side=index ? 4:1,w=80*side-2,h=index ? 238:140,base_y=h-40*side;
            Bytes payload;
            const std::array<std::pair<unsigned,unsigned>,16> origins{{{120,78},{80,98},{160,98},
                {40,118},{120,118},{200,118},{0,138},{80,138},{160,138},{240,138},
                {40,158},{120,158},{200,158},{80,178},{160,178},{120,198}}};
            for(unsigned tile=0;tile<side*side;++tile) {
                const unsigned ox=index ? origins[tile].first:0,oy=index ? origins[tile].second:base_y;
                for(unsigned y=0;y<40;++y) {
                    const unsigned start=y<20 ? 38-2*y:2*y-40;
                    for(unsigned x=start;x<78-start;++x) {
                        const unsigned px=ox+x,py=oy+y;
                        const bool hole=index ? (px>=155&&px<=163&&py>=142&&py<=150):
                            (px>=35&&px<=43&&py>=118&&py<=126);
                        const unsigned color=hole ? 0xf81f:0x7c00;
                        payload.push_back(std::uint8_t(color));payload.push_back(std::uint8_t(color>>8));
                    }
                }
            }
            const unsigned start_x=index ? 144:28;
            unsigned position=0;
            for(unsigned y=20;y<44;++y) for(unsigned x=start_x;x<start_x+28;++x) {
                if(x>=start_x+10&&x<=start_x+18&&y>=27&&y<=35) continue;
                const unsigned next=y*w+x;skip(payload,next-position);
                payload.push_back(1);payload.push_back(0xe0);payload.push_back(3);position=next+1;
            }
            lengths[index]=unsigned(payload.size());const auto r=40680+(index+1)*72;
            u32(sg3,r,unsigned(bitmap.size()));u32(sg3,r+4,lengths[index]);u32(sg3,r+8,3200*side*side);
            u16(sg3,r+20,w);u16(sg3,r+22,h);u16(sg3,r+50,30);sg3[r+55]=std::uint8_t(side);
            bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        }
        write(root/"DATA/authored.sg3",sg3);write(root/"DATA/authored.555",bitmap);
    }
    ~Archive(){std::error_code e;std::filesystem::remove_all(root,e);}
    maps::StoredGraphicsPlan plan(bool spatial) const {
        maps::StoredGraphicsPlan p;p.data_root=root;p.landscape_layers_available=true;p.border=0;
        p.cell_by_storage.resize(228*228);p.status_by_storage.assign(228*228,maps::StoredStatus::Excluded);
        p.height_bytes.assign(228*228,0);
        assets::AssetRecord r;r.id={"DATA/authored.sg3",spatial ? 2U:1U};
        r.width=spatial ? 318:78;r.height=spatial ? 238:140;r.image_type=30;
        r.isometric_size_flag=spatial ? 4:1;r.uncompressed_length=spatial ? 51200:3200;r.data_length=lengths[spatial ? 1:0];
        p.assets.push_back({r,maps::StoredStatus::DecodePending,false,false,{}});
        const unsigned side=spatial ? 4:1;
        maps::PlacedFootprint f;f.id=0;f.asset_index=0;f.origin={0,0};f.width_cells=side;f.height_cells=side;
        f.image_origin=spatial ? scene::Point{-159,-78}:scene::Point{-39,-120};
        f.status=maps::StoredStatus::DecodePending;
        auto generated=std::make_shared<maps::RegeneratedMapRenderPlan>();
        maps::RegeneratedLandscapeInstance instance;
        for(unsigned y=0;y<side;++y) for(unsigned x=0;x<side;++x) {
            maps::StoredCell cell;cell.storage={x,y};cell.cell_index=y*228+x;cell.terrain_raw=0x80;
            cell.world={(int(x)-int(y))*40.,(x+y)*20.};cell.asset_index=0;cell.footprint_index=0;
            cell.status=maps::StoredStatus::DecodePending;cell.image_origin=f.image_origin;
            p.cell_by_storage[cell.cell_index]=p.cells.size();f.cell_indices.push_back(p.cells.size());
            instance.cell_indices.push_back(p.cells.size());instance.geometry.owned_cells.push_back(cell.storage);
            p.cells.push_back(cell);
        }
        p.footprints.push_back(f);
        if(spatial) {
            instance.geometry.origin={0,0};instance.geometry.side=4;instance.geometry.draw_cell={0,3};
            instance.geometry.depth_cell=maps::GridCell{3,3};
            instance.geometry.explicit_anchor=maps::LandscapeInstanceAnchor{{3,3},159,20};
            instance.geometry.explicit_height=maps::LandscapeInstanceHeight{maps::LandscapeInstanceHeightSource::SerializedCellHeight,{0,3}};
            instance.geometry.selection.family=maps::LandscapeFamily::GreatWall;instance.geometry.selection.evidence=maps::SelectorEvidence::Preview;
            instance.composition_policy=maps::LandscapeCompositionPolicy::SpatialCombined;instance.asset_index=0;
            generated->cells.resize(p.cells.size());generated->footprint_assets.resize(1);
            for(auto& cell:generated->cells) {cell.instance_index=0;cell.asset_index=0;}
            generated->instances.push_back(instance);p.regenerated=generated;
        }
        return p;
    }
};
void stored_case(Context& c,const Archive& archive,unsigned kind) {
    const bool spatial=kind==3;auto plan=archive.plan(spatial);
    const auto before=openemperor::StoredGraphicsRenderer::live_texture_count();
    openemperor::StoredGraphicsRenderer sprites(std::move(plan));sprites.initialize(c.renderer);
    sprites.set_landscape_mode(kind==0 ? openemperor::LandscapeDebugMode::Snapshot:openemperor::LandscapeDebugMode::Regenerated);
    const auto uploads=sprites.upload_count();check(uploads==3,"stored Combined/Base/Overlay eagerly uploaded once");
    const scene::Point image_origin=spatial ? scene::Point{-159,-78}:scene::Point{-39,-120};
    const scene::Point body=kind==2 ? scene::Point{32,24}:spatial ? scene::Point{144,146}:scene::Point{39,110};
    const scene::Point hole=kind==2 ? scene::Point{42,30}:spatial ? scene::Point{159,146}:scene::Point{39,122};
    const std::string name=kind==0 ? "stored-combined":kind==1 ? "stored-base":kind==2 ? "stored-overlay":"stored-spatial";
    perf::reset();scene::Camera2D camera;camera.viewport_width=width;camera.viewport_height=height;
    exercise(c,name,body,{{body,kind==2 ? green:red},{hole,background}},
        [&](scene::Point top,double zoom) {
            camera.zoom=zoom;camera.offset={top.x-image_origin.x*zoom,top.y-image_origin.y*zoom};
            if(kind==1) {sprites.begin_frame();check(sprites.draw_ground_item(0,camera),"stored Base");}
            else if(kind==2) {sprites.begin_frame();check(sprites.draw_item(0,camera),"stored Overlay");}
            else check(sprites.render(camera,{}),"stored full frame");
        },[&](scene::Point top,double zoom) {
            const auto solid=screen_sample(top,body,zoom),empty=screen_sample(top,hole,zoom);
            const bool solid_visible=solid.x>=clip.x&&solid.x<clip.x+clip.w&&solid.y>=clip.y&&solid.y<clip.y+clip.h;
            if(solid_visible) check(bool(sprites.hit_test(solid,camera))==(kind!=1),"stored visible alpha picking");
            check(!sprites.hit_test(empty,camera),"stored transparent hole click-through");
        });
    check(sprites.upload_count()==uploads,"stored uploads constant across zooms");pure_counters();
    sprites.shutdown();check(openemperor::StoredGraphicsRenderer::live_texture_count()==before,"stored shutdown releases all components");
}
void run_cases(Context& c,const Archive& archive,const std::function<void(const std::string&,const std::function<void()>&)>& run) {
    run("building-shared-preview",[&]{building_case(c,false);});
    run("building-preview-normal",[&]{building_case(c,false,true);});
    run("building-house-levels",[&]{building_case(c,true);});
    run("walker-large-frames",[&]{walker_case(c,false);});
    run("walker-small-frames",[&]{walker_case(c,true);});
    for(unsigned n=0;n<3;++n) run("road-"+std::to_string(n),[&]{road_case(c,n);});
    run("road-preview-normal",[&]{road_case(c,0,true);});
    for(unsigned n=0;n<4;++n) run("stored-"+std::to_string(n),[&]{stored_case(c,archive,n);});
}
void policy_cases() {
    using namespace openemperor::texture_compatibility;
    check(eager_rgba_access_for("software",SDL_VERSIONNUM(3,4,14))==SDL_TEXTUREACCESS_STREAMING,"observed software runtime policy");
    for(int version:{SDL_VERSIONNUM(3,4,10),SDL_VERSIONNUM(3,4,13),SDL_VERSIONNUM(3,4,15)})
        check(eager_rgba_access_for("software",version)==SDL_TEXTUREACCESS_STATIC,"other SDL runtimes stay STATIC");
    for(const auto backend:{"metal","opengl","unknown",""})
        check(eager_rgba_access_for(backend,SDL_VERSIONNUM(3,4,14))==SDL_TEXTUREACCESS_STATIC,"other backends stay STATIC");
}
}
int main(int argc,char** argv) {
    try {
        check(argc==1 || (argc==2&&std::string(argv[1])=="metal"),"usage: texture-compatibility-tests [metal]");
        Context context(argc==2 ? "metal":"software");Archive archive;perf::set_enabled(true);
        policy_cases();
        run_cases(context,archive,[](const auto& name,const auto& run){run();std::cout<<name<<" stable\n";});
        perf::set_enabled(false);std::cout<<"Production texture zoom compatibility regressions passed\n";return 0;
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
