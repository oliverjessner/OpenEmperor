#include "renderer/AgricultureFallbackRenderer.h"
#include "core/PerformanceDiagnostics.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>

namespace {
using namespace openemperor;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
using Renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>;
constexpr std::array<std::uint8_t,4> background{27,39,48,255};
std::array<std::uint8_t,4> pixel(SDL_Surface* image,int x,int y){
    std::array<std::uint8_t,4> p{};check(SDL_ReadSurfacePixel(image,x,y,&p[0],&p[1],&p[2],&p[3]),"Read actual rendered pixel");return p;
}
void clear(SDL_Renderer* renderer){check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255)&&SDL_RenderClear(renderer),"Clear synthetic scene");}
std::uint64_t pixels_hash(SDL_Surface* image){
    std::uint64_t hash=UINT64_C(1469598103934665603);
    for(int y=0;y<image->h;++y)for(int x=0;x<image->w;++x)for(const auto value:pixel(image,x,y))hash=(hash^value)*UINT64_C(1099511628211);
    return hash;
}
}
int main(){try {
    using namespace openemperor;
    Surface image(SDL_CreateSurface(256,256,SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    check(bool(image),"Create pixel surface");Renderer renderer(SDL_CreateSoftwareRenderer(image.get()),SDL_DestroyRenderer);
    check(bool(renderer),"Create actual software renderer");
    constexpr std::array<std::pair<int,int>,4> directions{{{1,0},{0,1},{-1,0},{0,-1}}};
    std::size_t checked=0;
    for(unsigned kind=0;kind<9;++kind)for(const auto [dx,dy]:directions)for(unsigned phase=0;phase<4;++phase) {
        AgricultureActorPose pose;pose.kind=static_cast<AgricultureActorKind>(kind);pose.dx=static_cast<std::int8_t>(dx);pose.dy=static_cast<std::int8_t>(dy);
        pose.phase=static_cast<std::uint8_t>(phase);pose.moving=true;pose.cargo=kind<6?2:0;
        const auto mesh=prepare_agriculture_actor_mesh(pose);check(mesh.count>0&&mesh.count<=AgricultureMesh::triangle_limit,"Bounded prepared authored actor");
        for(const double zoom:{1.,2.,4.}) {
            clear(renderer.get());check(SDL_SetRenderDrawBlendMode(renderer.get(),SDL_BLENDMODE_NONE),"Set caller blend");
            check(draw_agriculture_mesh(renderer.get(),mesh,{128,176},zoom)&&SDL_FlushRenderer(renderer.get()),"Draw real authored actor pixels");
            SDL_BlendMode blend;check(SDL_GetRenderDrawBlendMode(renderer.get(),&blend)&&blend==SDL_BLENDMODE_NONE,"Draw restored caller blend");
            unsigned visible=0,missed=0;
            for(int y=20;y<215;++y)for(int x=25;x<231;++x)if(pixel(image.get(),x,y)!=background) {
                ++visible;
                // SDL's rasterized boundary has subpixel vertex rounding.
                // Use the production envelope: one displayed pixel per axis.
                const scene::Point local{(x+.5-128)/zoom,(y+.5-176)/zoom};
                const bool hit=hit_agriculture_mesh(mesh,local,1./zoom);
                if(!hit) {
                    ++missed;
                    if(missed<=8) {
                        bool diagonal=false;
                        for(const int ox:{-1,1})for(const int oy:{-1,1})
                            diagonal=diagonal||hit_agriculture_mesh(mesh,{local.x+ox*.6/zoom,local.y+oy*.6/zoom});
                        std::cerr<<"Opaque edge miss pixel="<<x<<','<<y<<" local="<<local.x<<','<<local.y<<" diagonal_pixel_neighborhood="<<diagonal<<'\n';
                    }
                }
            }
            check(visible>25*zoom*zoom,"No substantial visible actor silhouette");
            if(missed>2) {
                const auto dir=std::filesystem::path(".local/agri-mac12/pixel-failures");std::filesystem::create_directories(dir);
                const auto name="kind-"+std::to_string(kind)+"-dx"+std::to_string(dx)+"-dy"+std::to_string(dy)+"-phase"+std::to_string(phase)+"-zoom"+std::to_string(int(zoom))+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".png";
                const auto path=dir/name;check(!std::filesystem::exists(path)&&SDL_SavePNG(image.get(),path.string().c_str()),"Preserve first private failure pixels");
                std::cerr<<"Mismatch kind="<<kind<<" direction="<<dx<<','<<dy<<" phase="<<phase<<" zoom="<<zoom<<" visible="<<visible<<" missed="<<missed<<" image="<<std::filesystem::absolute(path)<<'\n';
            }
            check(missed<=2,"Drawn opaque pixels cannot be picked from their actual mesh");
            check(!hit_agriculture_mesh(mesh,{mesh.left-1,mesh.top-1}),"Transparent outside mesh selected actor");
            ++checked;
        }
    }
    // Stationary actors keep the exact same rendered pose despite arbitrary
    // external frame numbers; real edge/work inputs are the only animation.
    for(unsigned kind=0;kind<9;++kind) {
        AgricultureActorPose pose;pose.kind=static_cast<AgricultureActorKind>(kind);
        std::set<std::uint64_t> frozen;
        for(unsigned phase=0;phase<4;++phase){pose.phase=static_cast<std::uint8_t>(phase);clear(renderer.get());
            check(draw_agriculture_mesh(renderer.get(),prepare_agriculture_actor_mesh(pose),{128,176},2)&&SDL_FlushRenderer(renderer.get()),"Draw stationary actor");frozen.insert(pixels_hash(image.get()));}
        check(frozen.size()==1,"Stationary actor falsely animated an untravelled route or harvest");
        std::set<std::uint64_t> facing;
        for(const auto [dx,dy]:directions) {
            pose.dx=static_cast<std::int8_t>(dx);pose.dy=static_cast<std::int8_t>(dy);pose.phase=2;pose.moving=true;
            clear(renderer.get());check(draw_agriculture_mesh(renderer.get(),prepare_agriculture_actor_mesh(pose),{128,176},4)&&SDL_FlushRenderer(renderer.get()),"Draw actual four-direction facing");
            facing.insert(pixels_hash(image.get()));
        }
        check(facing.size()==4,"Authored actor art collapses distinct actual land/water directions");
    }
    {
        AgricultureActorPose right;right.kind=AgricultureActorKind::FoodCourier;
        right.dx=1;right.dy=0;right.moving=true;right.phase=2;right.cargo=4;
        auto left=right;left.dx=0;left.dy=1;
        const auto east=prepare_agriculture_actor_mesh(right),west=prepare_agriculture_actor_mesh(left);
        check(east.count==west.count,"Turning cart lost body/load geometry");
        for(std::size_t i=0;i<east.count;++i)for(std::size_t v=0;v<3;++v) {
            const auto a=east.triangles[i].points[v],b=west.triangles[i].points[v];
            check(a.x==-b.x&&a.y==b.y,"Turning courier left its hitch, wheels or cargo facing the old direction");
        }
    }
    for(const auto kind:{AgricultureActorKind::Boat,AgricultureActorKind::Hunter}) {
        AgricultureActorPose pose;pose.kind=kind;pose.working=true;pose.phase=2;pose.cargo=1;
        pose.action=AgricultureActorAction::Working;clear(renderer.get());
        check(draw_agriculture_mesh(renderer.get(),prepare_agriculture_actor_mesh(pose),{128,176},4)&&SDL_FlushRenderer(renderer.get()),"Draw actual working phase");
        const auto working=pixels_hash(image.get());pose.action=AgricultureActorAction::Unloading;clear(renderer.get());
        check(draw_agriculture_mesh(renderer.get(),prepare_agriculture_actor_mesh(pose),{128,176},4)&&SDL_FlushRenderer(renderer.get()),"Draw distinct actual unloading phase");
        check(working!=pixels_hash(image.get()),"Dock/Tent unloading falsely depicted fishing or hunting work");
    }
    for(const auto [kind,side]:std::array<std::pair<simulation::Object,int>,4>{{{simulation::Object::OriginalMill53,5},{simulation::Object::OriginalFoodShop66,2},{simulation::Object::OriginalFoodFarm,3},{simulation::Object::OriginalIrrigationPump,2}}}) {
        const auto mesh=prepare_agriculture_building_mesh(kind,side,2);
        check(mesh.count>0&&mesh.count<=AgricultureMesh::triangle_limit,"Bounded authored building mesh");
        check(mesh.left>=-40*side&&mesh.left+mesh.width<=40*side,"Building body escaped paid horizontal footprint envelope");
        check(mesh.top+mesh.height<=20,"Building feet moved forward of complete footprint");
        bool roof=false;for(double y=mesh.top;y<mesh.top+25;y+=1)for(double x=mesh.left;x<mesh.left+mesh.width;x+=1)roof=roof||hit_agriculture_mesh(mesh,{x,y});
        check(roof,"Raised authored roof has no exact picking geometry");
    }
    const auto output=std::filesystem::path(".local/agri-mac12")/("mesh-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directories(output),"Use fresh private visual output");
    for(const int z:{1,2,4}) {
        Surface sheet(SDL_CreateSurface(9*62*z,70*z,SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
        Renderer sheet_renderer(SDL_CreateSoftwareRenderer(sheet.get()),SDL_DestroyRenderer);check(bool(sheet)&&bool(sheet_renderer),"Create authored contact sheet");clear(sheet_renderer.get());
        for(unsigned kind=0;kind<9;++kind) {
            AgricultureActorPose pose;pose.kind=static_cast<AgricultureActorKind>(kind);pose.phase=2;pose.dx=1;pose.dy=0;
            pose.moving=true;pose.cargo=kind<6?2:0;
            check(draw_agriculture_mesh(sheet_renderer.get(),prepare_agriculture_actor_mesh(pose),{double(31+kind*62)*z,double(48*z)},z),"Draw contact sheet actor");
        }
        check(SDL_FlushRenderer(sheet_renderer.get())&&SDL_SavePNG(sheet.get(),(output/("actors-"+std::to_string(z)+"x.png")).string().c_str()),"Write private authored visual capture");
    }
    const auto mesh=prepare_agriculture_actor_mesh({});
    performance::set_enabled(true);performance::reset();
    for(unsigned i=0;i<3000;++i)check(draw_agriculture_mesh(renderer.get(),mesh,{128,176},1),"Pure prepared mesh frame");
    for(std::size_t i=0;i<static_cast<std::size_t>(performance::Counter::Count);++i)
        check(performance::counter(static_cast<performance::Counter>(i))==0,"Mesh draw performed forbidden World, route, decode, upload or file work");
    performance::set_enabled(false);
    std::cout<<"PASS "<<checked<<" actor/direction/phase/zoom pixel cases; shared-mesh raster picking, frozen stationary poses, bounded building roofs and3000 pure mesh frames; captures="<<std::filesystem::absolute(output)<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"Agriculture mesh pixels FAIL: "<<e.what()<<'\n';return 1;}}
