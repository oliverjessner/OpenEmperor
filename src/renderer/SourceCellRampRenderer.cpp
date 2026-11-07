#include "renderer/SourceCellRampRenderer.h"
#include "simulation/World.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace openemperor {
namespace {
using Point=scene::Point;
using Ramp=simulation::SourceCellRamp;
constexpr std::size_t max_runs=4096,max_faces=512,max_bytes=8U*1024U*1024U;
constexpr int raster_side=128,raster_origin=64;
constexpr std::array<simulation::Cell,4> directions{{{0,1},{-1,0},{0,-1},{1,0}}};
struct Run { std::int8_t x,y;std::uint8_t width,r,g,b; };
static_assert(sizeof(Run)==6);
using Part=std::vector<Run>;
struct Layout {std::array<Part,2> parts;std::size_t faces=0;};
struct Face {std::array<Point,4> points;SDL_Color color;bool front=false;};
struct Pixel {SDL_Color color{};bool front=false;};

bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y);}
std::size_t direction(const Ramp& r) {
    const auto dx=std::int64_t(r.high_neighbor.x)-r.source_cell.x;
    const auto dy=std::int64_t(r.high_neighbor.y)-r.source_cell.y;
    for(std::size_t i=0;i<directions.size();++i)
        if(dx==directions[i].x&&dy==directions[i].y)return i;
    throw std::invalid_argument("source-cell visual requires an orthogonal high neighbor");
}
bool inside(Point p,const Face& face) {
    double sign=0,area=0;
    for(std::size_t i=0;i<4;++i) {
        const auto a=face.points[i],b=face.points[(i+1)%4];
        area+=a.x*b.y-b.x*a.y;
        const double cross=(b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
        if(std::abs(cross)<1e-9)continue;
        if(sign!=0&&(cross>0)!=(sign>0))return false;
        sign=cross;
    }
    return std::abs(area)>1e-8;
}
std::uint32_t grain(int x,int y) {
    auto n=std::uint32_t(x+137)*0x8da6b343U^std::uint32_t(y+193)*0xd8163841U;
    n^=n>>13;n*=0x85ebca6bU;n^=n>>16;return n;
}
bool same(SDL_Color a,SDL_Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
Layout make_layout(std::size_t orientation,bool elevated) {
    Ramp ramp{{1},{8,8},{8+directions[orientation].x,8+directions[orientation].y},0,1,40,0};
    std::vector<Face> faces;faces.reserve(40);
    const auto point=[&](double s,double side,double lift=0.) {
        auto p=source_cell_ramp_surface_point({},ramp,s,side,elevated);
        p.y-=lift*40.;return p;
    };
    const auto emit=[&](std::array<Point,4> p,SDL_Color c,bool front) {
        if(faces.size()==max_faces)throw std::length_error("source-cell visual face bound exceeded");
        faces.push_back({p,c,front});
    };
    // Cardinal projection determines the near bank, without a camera-specific
    // exception. Terrain authority and the walking height remain untouched.
    const int near= directions[orientation].x-directions[orientation].y>0?1:-1;
    const auto banks=[&](int side,bool front) {
        for(int segment=0;segment<8;++segment) {
            const double a=-.5+segment/8.,b=a+.125;
            const auto ridge=[](double s){return .055*std::sin((s+.5)*3.141592653589793);};
            const double ra=elevated?ridge(a):0.,rb=elevated?ridge(b):0.;
            const auto topa=point(a,side*.44,ra),topb=point(b,side*.44,rb);
            auto toea=point(a,side*.49),toeb=point(b,side*.49);
            // The support meets the original low plane outside the slope.
            // This is an authored earth bank, not inferred original cliff art.
            if(elevated){toea.y+=40*(a+.5);toeb.y+=40*(b+.5);}
            emit({topa,topb,toeb,toea},{108,101,78,255},front);
            emit({point(a,side*.31),point(b,side*.31),topb,topa},
                {174,159,118,255},front);
        }
    };
    banks(-near,false);
    emit({point(-.5,-.32),point(.5,-.32),point(.5,.32),point(-.5,.32)},
        {205,178,127,255},false);
    // East/South surfaces collapse to a line in the fixed 2:1 projection.
    // A two-pixel opaque crest records that limit instead of moving the foot
    // or stretching an original bitmap over a different surface.
    if(elevated&&(orientation==0||orientation==3)) {
        const auto a=point(-.5,-.32),b=point(.5,.32);
        const double length=std::hypot(b.x-a.x,b.y-a.y);
        // These opposite corners can coincide on one projected diagonal;
        // choose the extremal corners of the complete source-cell surface.
        const std::array<Point,4> corners{{point(-.5,-.32),point(.5,-.32),point(.5,.32),point(-.5,.32)}};
        Point low=corners[0],high=corners[0];
        for(const auto p:corners){if(p.x<low.x)low=p;if(p.x>high.x)high=p;}
        const double n=std::hypot(high.x-low.x,high.y-low.y);
        if(n==0||!std::isfinite(length))throw std::logic_error("invalid source-cell crest");
        const Point normal{-(high.y-low.y)/n,(high.x-low.x)/n};
        emit({Point{low.x-normal.x,low.y-normal.y},Point{high.x-normal.x,high.y-normal.y},
              Point{high.x+normal.x,high.y+normal.y},Point{low.x+normal.x,low.y+normal.y}},
             {205,178,127,255},false);
    }
    banks(near,true);
    Layout result;result.faces=faces.size();
    // One complete raster owns each pixel once. The retained front/back owner
    // prevents alpha seams or doubled bank fragments at the local painter split.
    for(int y=-64;y<64;++y) {
        std::array<Pixel,raster_side> row{};
        for(int x=-64;x<64;++x)for(const auto& face:faces)if(inside({x+.5,y+.5},face)) {
            auto color=face.color;const int shade=int(grain(x,y)%21)-10;
            color.r=Uint8(std::clamp(int(color.r)+shade,0,255));
            color.g=Uint8(std::clamp(int(color.g)+shade,0,255));
            color.b=Uint8(std::clamp(int(color.b)+shade,0,255));
            row[std::size_t(x+raster_origin)]={color,face.front};
        }
        for(int x=0;x<raster_side;) {
            const auto pixel=row[std::size_t(x)];if(!pixel.color.a){++x;continue;}
            int end=x+1;while(end<raster_side&&row[std::size_t(end)].front==pixel.front&&same(row[std::size_t(end)].color,pixel.color))++end;
            auto& part=result.parts[std::size_t(pixel.front)];
            if(part.size()==max_runs)throw std::length_error("source-cell visual run bound exceeded");
            part.push_back({std::int8_t(x-raster_origin),std::int8_t(y),std::uint8_t(end-x),pixel.color.r,pixel.color.g,pixel.color.b});x=end;
        }
    }
    for(auto& part:result.parts)part.shrink_to_fit();
    return result;
}
SDL_FColor color(const Run& run,BridgeTint tint) {
    const SDL_FColor base{run.r/255.F,run.g/255.F,run.b/255.F,1};
    if(tint==BridgeTint::Normal)return base;
    const SDL_FColor overlay=tint==BridgeTint::Valid?SDL_FColor{.2F,.7F,.42F,1}:
        tint==BridgeTint::Unaffordable?SDL_FColor{.85F,.62F,.2F,1}:SDL_FColor{.8F,.25F,.23F,1};
    return {overlay.r*.8F+base.r*.2F,overlay.g*.8F+base.g*.2F,overlay.b*.8F+base.b*.2F,1};
}
bool draw_runs(SDL_Renderer* renderer,const scene::Camera2D& camera,Point ground,const Part& part,BridgeTint tint) {
    if(!renderer||!finite(ground)||!std::isfinite(camera.zoom)||camera.zoom<=0||!finite(camera.offset))return false;
    SDL_BlendMode previous=SDL_BLENDMODE_NONE;
    if(!SDL_GetRenderDrawBlendMode(renderer,&previous)||!SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND))return false;
    struct Restore {SDL_Renderer* r;SDL_BlendMode mode;~Restore(){(void)SDL_SetRenderDrawBlendMode(r,mode);}} restore{renderer,previous};
    std::array<SDL_Vertex,max_runs*4> vertices;std::array<int,max_runs*6> indices;
    for(std::size_t i=0;i<part.size();++i) {
        const auto& run=part[i];const auto c=color(run,tint);
        const std::array<Point,4> corners{{{double(run.x),double(run.y)},{double(int(run.x)+run.width),double(run.y)},
            {double(int(run.x)+run.width),double(int(run.y)+1)},{double(run.x),double(int(run.y)+1)}}};
        for(std::size_t j=0;j<4;++j){const auto p=camera.world_to_screen({ground.x+corners[j].x,ground.y+corners[j].y});vertices[i*4+j]={{float(std::round(p.x)),float(std::round(p.y))},c,{}};}
        constexpr int triangles[]{0,1,2,0,2,3};for(std::size_t j=0;j<6;++j)indices[i*6+j]=int(i*4)+triangles[j];
    }
    return part.empty()||SDL_RenderGeometry(renderer,nullptr,vertices.data(),int(part.size()*4),indices.data(),int(part.size()*6));
}
bool hit_runs(Point world,Point ground,const Part& part) {
    if(!finite(world)||!finite(ground))return false;
    const Point p{world.x-ground.x,world.y-ground.y};
    for(const auto& r:part)if(p.x>=r.x&&p.x<int(r.x)+r.width&&p.y>=r.y&&p.y<int(r.y)+1)return true;
    return false;
}
} // namespace

scene::Point source_cell_ramp_surface_point(scene::Point low,const simulation::SourceCellRamp& r,
    double along,double side,bool elevated) {
    const double dx=double(r.high_neighbor.x)-r.source_cell.x,dy=double(r.high_neighbor.y)-r.source_cell.y;
    const double x=dx*along-dy*side,y=dy*along+dx*side;
    const double height=elevated?simulation::source_cell_ramp_height_at(r,r.source_cell.x+x,r.source_cell.y+y)-r.low_height:0.;
    return {low.x+(x-y)*40.,low.y+(x+y)*20.-height*40.};
}
struct SourceCellRampPresentation::Data {
    std::array<std::array<Layout,4>,2> layouts;
    std::array<std::uint8_t,simulation::source_cell_ramp_max_links+1> orientations{};
    std::size_t bytes=0,faces=0,maximum_runs=0;
};
SourceCellRampPresentation::SourceCellRampPresentation()=default;
SourceCellRampPresentation::~SourceCellRampPresentation()=default;
SourceCellRampPresentation::SourceCellRampPresentation(SourceCellRampPresentation&&) noexcept=default;
SourceCellRampPresentation& SourceCellRampPresentation::operator=(SourceCellRampPresentation&&) noexcept=default;
void SourceCellRampPresentation::clear(){data_.reset();}
void SourceCellRampPresentation::prepare(const simulation::World& world) {
    if(!world.source_cell_ramps_supported()){clear();return;}
    prepare(world.source_cell_ramps(),world.ramps().size());
}
void SourceCellRampPresentation::prepare(std::span<const Ramp> ramps,std::size_t historical_links) {
    if(historical_links>simulation::source_cell_ramp_max_links||
        ramps.size()>simulation::source_cell_ramp_max_links-historical_links)
        throw std::length_error("combined ramp visual link bound exceeded");
    auto candidate=std::make_unique<Data>();
    for(const auto& r:ramps) {
        if(!r.id.value||r.id.value>simulation::source_cell_ramp_max_links||
            candidate->orientations[r.id.value]||int(r.high_height)-int(r.low_height)!=1)
            throw std::invalid_argument("invalid source-cell visual identity or height");
        candidate->orientations[r.id.value]=std::uint8_t(direction(r)+1);
    }
    for(std::size_t elevated=0;elevated<2;++elevated)for(std::size_t d=0;d<4;++d) {
        auto& layout=candidate->layouts[elevated][d];layout=make_layout(d,elevated!=0);
        candidate->faces+=layout.faces;
        for(const auto& part:layout.parts) {
            candidate->bytes+=part.capacity()*sizeof(Run);
            candidate->maximum_runs=std::max(candidate->maximum_runs,part.size());
        }
    }
    if(candidate->faces>max_faces||candidate->bytes>max_bytes)
        throw std::length_error("source-cell visual preparation budget exceeded");
    data_=std::move(candidate);
}
bool SourceCellRampPresentation::prepared(simulation::SourceCellRampId id) const {
    return data_&&id.value<data_->orientations.size()&&data_->orientations[id.value]!=0;
}
std::size_t SourceCellRampPresentation::geometry_bytes() const{return data_?data_->bytes:0;}
std::size_t SourceCellRampPresentation::face_count() const{return data_?data_->faces:0;}
std::size_t SourceCellRampPresentation::maximum_part_runs() const{return data_?data_->maximum_runs:0;}
bool SourceCellRampPresentation::draw_part(SDL_Renderer* renderer,const scene::Camera2D& camera,Point ground,
    simulation::SourceCellRampId id,bool front,bool elevated,BridgeTint tint) const {
    if(!prepared(id))return false;
    const auto& part=data_->layouts[std::size_t(elevated)][std::size_t(data_->orientations[id.value])-1].parts[std::size_t(front)];
    return draw_runs(renderer,camera,ground,part,tint);
}
bool SourceCellRampPresentation::draw_part(SDL_Renderer* renderer,const scene::Camera2D& camera,Point ground,
    const Ramp& ramp,bool front,bool elevated,BridgeTint tint) const {
    if(!data_||int(ramp.high_height)-int(ramp.low_height)!=1)return false;
    try{return draw_runs(renderer,camera,ground,data_->layouts[std::size_t(elevated)][direction(ramp)].parts[std::size_t(front)],tint);}
    catch(const std::invalid_argument&){return false;}
}
bool SourceCellRampPresentation::hit_part(Point world,Point ground,simulation::SourceCellRampId id,bool front,bool elevated) const {
    if(!prepared(id))return false;
    const auto& part=data_->layouts[std::size_t(elevated)][std::size_t(data_->orientations[id.value])-1].parts[std::size_t(front)];
    return hit_runs(world,ground,part);
}
bool SourceCellRampPresentation::hit_part(Point world,Point ground,const Ramp& ramp,bool front,bool elevated) const {
    if(!data_||int(ramp.high_height)-int(ramp.low_height)!=1)return false;
    try{return hit_runs(world,ground,data_->layouts[std::size_t(elevated)][direction(ramp)].parts[std::size_t(front)]);}
    catch(const std::invalid_argument&){return false;}
}
} // namespace openemperor
