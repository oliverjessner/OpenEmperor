#include "renderer/SourceCellRampRenderer.h"
#include "core/PerformanceDiagnostics.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace oe=openemperor;
namespace sim=oe::simulation;
namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
void check(bool condition,const std::string& why){if(!condition)throw std::runtime_error(why);}
void near(double actual,double expected,const char* why){check(std::abs(actual-expected)<1e-9,why);}
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
struct Target {
    SDL_Renderer* renderer;SDL_Texture* previous;SDL_Texture* texture;
    Target(SDL_Renderer* r,int w,int h):renderer(r),previous(SDL_GetRenderTarget(r)),
        texture(SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h)) {
        check(texture,SDL_GetError());
        if(!SDL_SetRenderTarget(r,texture)){SDL_DestroyTexture(texture);throw std::runtime_error(SDL_GetError());}
    }
    ~Target(){(void)SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(texture);}
};
Surface read(SDL_Renderer* renderer) {
    Surface raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(raw),SDL_GetError());
    Surface rgba(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);check(bool(rgba),SDL_GetError());return rgba;
}
std::array<Uint8,4> pixel(SDL_Surface* surface,int x,int y) {
    check(x>=0&&y>=0&&x<surface->w&&y<surface->h,"pixel witness outside owned target");
    const auto* p=static_cast<const Uint8*>(surface->pixels)+y*surface->pitch+x*4;
    return {p[0],p[1],p[2],p[3]};
}
// Independently projected one-cell plane. This oracle does not call the
// renderer helper or World height getter whose foot agreement it checks.
oe::scene::Point expected(oe::scene::Point low,sim::Cell d,double along,double side,bool elevated) {
    const double x=d.x*along-d.y*side,y=d.y*along+d.x*side;
    return {low.x+(x-y)*40.,low.y+(x+y)*20.-(elevated?40*(along+.5):0.)};
}
void draw(oe::SourceCellRampPresentation& view,SDL_Renderer* renderer,const oe::scene::Camera2D& camera,
    oe::scene::Point low,const sim::SourceCellRamp& ramp,bool elevated,SDL_Color background,
    oe::BridgeTint tint=oe::BridgeTint::Normal,bool preview=false) {
    check(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE)&&
        SDL_SetRenderDrawColor(renderer,background.r,background.g,background.b,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(const bool front:{false,true})
        check(preview?view.draw_part(renderer,camera,low,ramp,front,elevated,tint):
            view.draw_part(renderer,camera,low,ramp.id,front,elevated,tint),"prepared source-cell draw failed");
    SDL_BlendMode mode{};check(SDL_GetRenderDrawBlendMode(renderer,&mode)&&mode==SDL_BLENDMODE_NONE,"draw lost caller blend mode");
}
}

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        std::optional<fs::path> output;
        if(argc!=1) {
            check(argc==3&&std::string(argv[1])=="--capture","usage: source-cell-ramp-rendering-tests [--capture FRESH-.local/oe53-ROOT]");
            const auto base=fs::weakly_canonical(fs::current_path()/".local/oe53");
            const auto path=fs::weakly_canonical(argv[2]);const auto relative=path.lexically_relative(base);
            check(!relative.empty()&&relative!="."&&*relative.begin()!=".."&&!fs::exists(path),"fresh private OE53 capture root required");
            fs::create_directories(path);output=path;
        }
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        window=SDL_CreateWindow("Authored source-cell ramp pixel tests",1280,800,SDL_WINDOW_HIDDEN);check(window,SDL_GetError());
        renderer=SDL_CreateRenderer(window,"software");check(renderer,SDL_GetError());
        oe::SourceCellRampPresentation view;
        view.prepare(std::span<const sim::SourceCellRamp>{});
        check(view.geometry_bytes()>0&&view.geometry_bytes()<=8U*1024U*1024U&&view.face_count()<=512&&
            view.maximum_part_runs()<=4096,"eager empty-city preview cache exceeds its bounds");
        check(!view.prepared({1}),"empty preview cache invented an owned source-cell ID");
        constexpr std::array<sim::Cell,4> directions{{{0,1},{-1,0},{0,-1},{1,0}}};
        constexpr std::array<const char*,4> names{{"south","west","north","east"}};
        constexpr std::array<std::pair<int,int>,3> sizes{{{1280,800},{1600,900},{1920,1080}}};
        Json captures=Json::array();std::size_t cases=0,witnesses=0,opaque_pixels=0;
        for(std::size_t direction=0;direction<directions.size();++direction) {
            const auto d=directions[direction];
            for(const int level:{-2,-1,0,1,2}) {
                const sim::SourceCellRamp ramp{{1},{8,8},{8+d.x,8+d.y},std::int8_t(level),std::int8_t(level+1),40,0};
                view.prepare(std::span<const sim::SourceCellRamp>(&ramp,1));
                check(view.prepared(ramp.id),"prepared source-cell ID missing");
                const oe::scene::Point low{120,160-level*40.};
                // The source centre is half a level high; the bitmap/mesh
                // reference remains the signed LOW plane, never low+.5.
                for(const double along:{-.5,-.25,0.,.25,.5})for(const double side:{-.15,0.,.15}) {
                    const auto actual=oe::source_cell_ramp_surface_point(low,ramp,along,side);
                    const auto oracle=expected(low,d,along,side,true);
                    near(actual.x,oracle.x,"source-cell X differs from independent projected plane");
                    near(actual.y,oracle.y,"source-cell foot differs from independent projected plane");
                }
                const auto middle=oe::source_cell_ramp_surface_point(low,ramp,0,0);
                near(middle.y,low.y-20,"source-cell centre silently retained RampLink endpoint height");
                for(const auto& size:sizes) {
                    Target target(renderer,size.first,size.second);
                    for(const double zoom:{1.,2.,4.,1.73})for(const bool elevated:{false,true}) {
                        oe::scene::Camera2D camera;camera.zoom=zoom;
                        camera.viewport_width=size.first;camera.viewport_height=size.second;
                        camera.offset={size.first*.5-low.x*zoom,size.second*.5-(low.y-20)*zoom};
                        draw(view,renderer,camera,low,ramp,elevated,{23,67,113,255});
                        check(SDL_GetRenderTarget(renderer)==target.texture,"draw changed owned readback target");
                        auto first=read(renderer);
                        draw(view,renderer,camera,low,ramp,elevated,{211,31,83,255});
                        auto second=read(renderer);
                        for(const double along:{-.4,-.2,0.,.2,.4}) {
                            const auto foot=expected(low,d,along,0,elevated);
                            check(view.hit_part(foot,low,ramp.id,false,elevated)||view.hit_part(foot,low,ramp.id,true,elevated),
                                "authored footline/edge-on crest is not covered by prepared visual geometry");
                            const auto screen=camera.world_to_screen(foot);
                            const auto a=pixel(first.get(),int(std::floor(screen.x)),int(std::floor(screen.y)));
                            const auto b=pixel(second.get(),int(std::floor(screen.x)),int(std::floor(screen.y)));
                            check(a==b&&a!=std::array<Uint8,4>{23,67,113,255},"actual footline/crest exposes clear background");++witnesses;
                        }
                        // Raster/hit agreement at native-pixel centres is
                        // checked independently against actual opaque pixels.
                        // Integer zoom uses identical nearest shared boundaries.
                        if(zoom!=1.73)for(int y=-63;y<45;++y)for(int x=-55;x<55;++x) {
                            const oe::scene::Point local{low.x+x+.5,low.y+y+.5};
                            const bool hit=view.hit_part(local,low,ramp.id,false,elevated)||view.hit_part(local,low,ramp.id,true,elevated);
                            const auto screen=camera.world_to_screen(local);
                            const auto a=pixel(first.get(),int(std::floor(screen.x)),int(std::floor(screen.y)));
                            const auto b=pixel(second.get(),int(std::floor(screen.x)),int(std::floor(screen.y)));
                            check((a==b)==hit,"source-cell hit mask differs from its actual prepared opaque pixels");opaque_pixels+=std::size_t(hit);
                        }
                        check(!view.hit_part({low.x+100,low.y},low,ramp.id,false,elevated)&&
                            !view.hit_part({low.x+100,low.y},low,ramp.id,true,elevated),"empty space selected as source-cell ramp");
                        // A preview changes only tint and uses exactly the same
                        // prepared shape even before its ID exists in World.
                        if(level==0&&zoom==1.&&elevated)for(const auto tint:{oe::BridgeTint::Valid,oe::BridgeTint::Unaffordable,oe::BridgeTint::Blocked}) {
                            auto preview=ramp;preview.id={64};const auto bytes=view.geometry_bytes();
                            draw(view,renderer,camera,low,preview,true,{23,67,113,255},tint,true);
                            check(view.geometry_bytes()==bytes&&!view.prepared(preview.id),"preview allocated or published an owned ID");
                        }
                        if(output&&level==0&&elevated&&zoom!=1.73) {
                            const auto name=std::string(names[direction])+"-"+std::to_string(size.first)+"x"+std::to_string(size.second)+"-"+std::to_string(int(zoom))+"x.png";
                            check(SDL_SavePNG(first.get(),(*output/name).c_str()),SDL_GetError());
                            captures.push_back({{"file",name},{"direction",names[direction]},{"size",{size.first,size.second}},{"zoom",zoom},
                                {"runtime","SYNTHETIC_SDL dummy/software"},{"owned_rgba32_target",true},{"readback_before_present",true}});
                        }
                        ++cases;
                    }
                }
            }
        }
        // Invalid replacements retain the complete working prepared cache.
        const sim::SourceCellRamp good{{1},{8,8},{9,8},0,1,40,0};
        view.prepare(std::span<const sim::SourceCellRamp>(&good,1));const auto bytes=view.geometry_bytes();
        for(int variant=0;variant<5;++variant) {
            auto bad=good;std::vector<sim::SourceCellRamp> list{bad};std::size_t legacy=0;
            if(variant==0)list[0].high_neighbor={9,9};
            if(variant==1)list[0].high_height=2;
            if(variant==2)list.push_back(bad);
            if(variant==3)list[0].id={65};
            if(variant==4)legacy=64;
            bool rejected=false;try{view.prepare(list,legacy);}catch(const std::exception&){rejected=true;}
            check(rejected&&view.prepared(good.id)&&view.geometry_bytes()==bytes,"bad visual replacement partially published");
        }
        std::vector<sim::SourceCellRamp> maximum(64,good);
        for(std::size_t i=0;i<maximum.size();++i)maximum[i].id={std::uint32_t(i+1)};
        view.prepare(maximum);check(view.prepared({64})&&view.geometry_bytes()==bytes,"64-ID prepared cache duplicated geometry");
        {
            Target target(renderer,1280,800);oe::scene::Camera2D camera;camera.offset={640,420};
            const auto cache=view.geometry_bytes();oe::performance::set_enabled(true);oe::performance::reset();
            for(int frame=0;frame<100;++frame)for(const bool front:{false,true})
                check(view.draw_part(renderer,camera,{},good,front,true),"pure preview/committed draw failed");
            for(std::size_t i=0;i<std::size_t(oe::performance::Counter::Count);++i)
                check(oe::performance::counter(static_cast<oe::performance::Counter>(i))==0,"pure prepared frame performed prohibited work");
            oe::performance::set_enabled(false);check(view.geometry_bytes()==cache,"pure frames changed prepared cache");
        }
        check(!view.hit_part({std::numeric_limits<double>::quiet_NaN(),0},{},good,false),"nonfinite visual hit accepted");
        if(output) {
            Json report{{"status","PASS"},{"scope","Independently authored source-cell renderer/pixel fixture; no World gameplay or original art parity"},
                {"cases",cases},{"foot_witnesses",witnesses},{"opaque_pixel_witnesses",opaque_pixels},{"captures",captures},
                {"prepared_bytes",view.geometry_bytes()},{"prepared_faces",view.face_count()},{"maximum_part_runs",view.maximum_part_runs()},
                {"native","NOT_RUN"},{"human","NOT_RUN"},{"visual_review","NOT_RUN"}};
            std::ofstream f(*output/"report.json");check(bool(f<<report.dump(2)<<'\n'),"write private renderer report failed");
        }
        view.clear();check(view.geometry_bytes()==0&&!view.prepared({1}),"source-cell cache did not release");
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<"Authored source-cell pixels PASS: "<<cases<<" cases, "<<witnesses<<" foot witnesses; native/human NOT_RUN\n";return 0;
    }catch(const std::exception& error){oe::performance::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;}
}
