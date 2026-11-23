#include "renderer/AgricultureFallbackRenderer.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace openemperor {
namespace {
using P=scene::Point;
using Color=std::array<std::uint8_t,4>;
constexpr Color ink{51,43,35,255},skin{220,176,124,255},straw{209,180,105,255};
constexpr Color wood{136,91,51,255},wood_light{188,140,82,255},wood_dark{86,58,38,255};
constexpr Color leaf{82,128,67,255},earth{128,103,67,255},water{67,135,153,255};
Color rear_tone(Color color,bool rear) {
    if(rear)for(std::size_t i=0;i<3;++i)color[i]=static_cast<std::uint8_t>(std::max(0,int(color[i])-16));
    return color;
}
struct Builder {
    AgricultureMesh mesh;
    void triangle(P a,P b,P c,Color color) {
        if(mesh.count==mesh.triangles.size()) throw std::logic_error("Agriculture mesh exceeds bounded geometry");
        mesh.triangles[mesh.count++]={{a,b,c},color};
    }
    void quad(P a,P b,P c,P d,Color color) {triangle(a,b,c,color);triangle(a,c,d,color);}
    void rect(double x,double y,double w,double h,Color color) {
        quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},color);
    }
    void line(P a,P b,double width,Color color) {
        const auto dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
        if(length==0)return;
        const auto x=-dy*width*.5/length,y=dx*width*.5/length;
        quad({a.x+x,a.y+y},{b.x+x,b.y+y},{b.x-x,b.y-y},{a.x-x,a.y-y},color);
    }
    void oval(double x,double y,double rx,double ry,Color color,unsigned sides=8) {
        constexpr double tau=6.28318530717958647692;
        for(unsigned i=0;i<sides;++i) {
            const auto a=tau*i/sides,b=tau*(i+1)/sides;
            triangle({x,y},{x+std::cos(a)*rx,y+std::sin(a)*ry},
                {x+std::cos(b)*rx,y+std::sin(b)*ry},color);
        }
    }
    AgricultureMesh finish() {
        if(!mesh.count)return mesh;
        double right=-std::numeric_limits<double>::infinity(),bottom=right;
        mesh.left=mesh.top=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<mesh.count;++i)for(const auto p:mesh.triangles[i].points) {
            mesh.left=std::min(mesh.left,p.x);mesh.top=std::min(mesh.top,p.y);
            right=std::max(right,p.x);bottom=std::max(bottom,p.y);
        }
        mesh.width=right-mesh.left;mesh.height=bottom-mesh.top;return mesh;
    }
};
double cross(P a,P b,P c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
void person(Builder& b,const AgricultureActorPose& pose,double origin_x=0,double origin_y=0) {
    const double facing=pose.dx-pose.dy<0 ? -1:1;
    const bool rear=pose.dx+pose.dy<0;
    const double stride=pose.moving ? std::array<double,4>{-2,0,2,0}[pose.phase%4]:0;
    const bool unloading=pose.action==AgricultureActorAction::Unloading;
    const bool working=pose.working && pose.action==AgricultureActorAction::Working;
    const double reach=unloading ? std::array<double,4>{1,4,2,0}[pose.phase%4]:
        working ? std::array<double,4>{3,7,5,1}[pose.phase%4]:1;
    const auto p=[&](double x,double y){return P{origin_x+x*facing,origin_y+y};};
    const Color coat=rear_tone(pose.kind==AgricultureActorKind::HempGrower ? Color{155,125,76,255}:
        pose.kind==AgricultureActorKind::PlantationGrower ? Color{111,115,163,255}:
        pose.kind==AgricultureActorKind::Hunter ? Color{150,106,58,255}:Color{76,141,102,255},rear);
    b.line(p(-2,-8),p(-3-stride,0),2.5,wood_dark);
    b.line(p(2,-8),p(3+stride,0),2.5,wood_dark);
    b.line(p(-3-stride,0),p(-1-stride,0),2,ink);
    b.line(p(3+stride,0),p(5+stride,0),2,ink);
    b.quad(p(-4,-18),p(4,-18),p(5,-7),p(-5,-7),coat);
    b.line(p(-4,-16),p(-6,-9-stride*.4),2.5,coat);
    b.line(p(4,-16),p(7+reach,-12-reach*.5),2.5,skin);
    b.oval(origin_x,origin_y-22,3.6,4,rear ? Color{113,83,55,255}:skin);
    b.triangle(p(-7,-24),p(0,-29),p(7,-24),straw);
    b.line(p(-7,-24),p(7,-24),1.5,wood_dark);
    if(!rear)b.rect(origin_x+facing*2-.6,origin_y-22,.9,1.1,ink);
    if(pose.kind==AgricultureActorKind::Hunter && !unloading) {
        b.line(p(8,-20),p(12,-13),1,wood_light);b.line(p(12,-13),p(8,-6),1,wood_light);
        b.line(p(8,-20),p(8,-6),.8,ink);
    } else if(working) {
        b.line(p(6+reach,-20),p(12+reach,-1),1.5,wood_light);
        b.line(p(10+reach,-2),p(16+reach,-2),2,ink);
    }
    if(pose.cargo>0 && pose.kind!=AgricultureActorKind::FoodCourier) {
        b.quad(p(-7,-17),p(-3,-16),p(-3,-8),p(-8,-9),wood_light);
        b.line(p(-8,-12),p(-3,-11),1,wood_dark);
    }
}
void animal(Builder& b,const AgricultureActorPose& pose) {
    const double facing=pose.dx-pose.dy<0 ? -1:1;
    const bool rear=pose.dx+pose.dy<0;
    const auto p=[&](double x,double y){return P{x*facing,y};};
    const auto stride=pose.moving ? std::array<double,4>{-2,0,2,0}[pose.phase%4]:0;
    const bool bird=pose.kind==AgricultureActorKind::Pheasant;
    const bool antelope=pose.kind==AgricultureActorKind::Antelope;
    const Color hide=rear_tone(bird ? Color{151,91,58,255}:antelope ? Color{185,149,91,255}:Color{119,92,73,255},rear);
    const double body_y=antelope ? -12:-6;
    for(const double x:{-5.0,5.0}) {
        b.line(p(x,body_y),p(x+stride,0),bird ? 1:1.8,wood_dark);
        if(!bird)b.line(p(x+2,body_y),p(x+2-stride,1),1.4,ink);
    }
    b.oval(0,body_y,bird ? 6:10,bird ? 4:5,hide,10);
    if(bird) {
        b.triangle(p(-4,-7),p(-17,-13),p(-8,-3),Color{71,109,96,255});
        b.line(p(4,-8),p(7,-13),3,Color{111,135,79,255});
        b.oval(facing*7,-13,2.8,2.8,Color{159,72,51,255});
        b.triangle(p(9,-13),p(13,-12),p(9,-11),straw);
    } else if(antelope) {
        b.line(p(7,-13),p(10,-22),5,hide);b.oval(facing*12,-23,4.5,3,hide);
        b.line(p(10,-25),p(8,-34),1.3,wood_dark);
        b.line(p(13,-25),p(13,-35),1.3,wood_dark);
        b.triangle(p(7,-23),p(3,-27),p(8,-25),hide);
    } else {
        b.oval(facing*9,-8,5,4,hide);b.rect(facing*13-1,-8,3,3,Color{164,121,104,255});
        b.triangle(p(7,-10),p(7,-16),p(11,-11),hide);
        b.line(p(-9,-7),p(-13,-10),1.5,hide);
    }
    if(!rear)b.rect(facing*(bird ? 8:antelope ? 14:10)-.5,bird ? -14:antelope ? -24:-9,1,1,ink);
}
} // namespace

AgricultureMesh prepare_agriculture_actor_mesh(const AgricultureActorPose& pose) {
    Builder b;
    if(pose.kind==AgricultureActorKind::WildPig || pose.kind==AgricultureActorKind::Pheasant ||
       pose.kind==AgricultureActorKind::Antelope) animal(b,pose);
    else if(pose.kind==AgricultureActorKind::Boat) {
        double dx=double(pose.dx-pose.dy),dy=double(pose.dx+pose.dy)*.5;
        const auto length=std::hypot(dx,dy);if(length==0){dx=1;dy=0;}else{dx/=length;dy/=length;}
        const auto p=[&](double u,double v,double h=0){return P{u*dx-v*dy,u*dy+v*dx-h};};
        b.quad(p(-19,-3),p(14,-5),p(21,0),p(-14,6),wood_dark);
        b.quad(p(-17,-4,3),p(14,-5,3),p(20,0,3),p(-13,5,3),wood);
        b.line(p(-16,-4,3),p(14,-5,3),1.5,wood_light);
        b.line(p(-13,5,3),p(20,0,3),1.5,wood_light);
        const bool fishing=pose.action==AgricultureActorAction::Working;
        const bool unloading=pose.action==AgricultureActorAction::Unloading;
        const double reach=(pose.working && fishing) || pose.moving ? std::array<double,4>{0,3,5,2}[pose.phase%4]:0;
        b.line(p(-2,0,3),p(-4,0,16),4,leaf);b.oval(p(-4,0,19).x,p(-4,0,19).y,3,3,skin);
        b.triangle(p(-9,0,21),p(-4,0,25),p(1,0,21),straw);
        if(unloading) {
            const auto hand=std::array<double,4>{0,2,4,1}[pose.phase%4];
            b.line(p(-3,0,13),p(4,-1,7+hand),2,skin);
            b.line(p(-12,2,4),p(13,2,4),1.3,wood_light);
        } else b.line(p(-2,-1,13),p(10,-10-reach,0),1.3,wood_light);
        if(fishing)b.line(p(10,-10-reach,0),p(10,-10-reach,-4),.8,Color{193,211,210,255});
        for(int i=0;i<std::clamp(pose.cargo,0,4);++i) {
            const auto c=p(4+(i%2)*4,-1+(i/2)*3,5);
            b.oval(c.x,c.y,2.5,1.3,Color{112,180,194,255},4);
        }
    } else {
        const bool cart=pose.kind==AgricultureActorKind::FoodCourier;
        const bool reflect_cart=cart && pose.dx-pose.dy<0;
        auto local_pose=pose;
        // Person, hitch, wheels and physical load share one facing transform.
        // Retain the actual front/rear direction while preparing its local pose.
        if(reflect_cart)std::swap(local_pose.dx,local_pose.dy);
        person(b,local_pose,cart ? -9:0,0);
        if(cart) {
            b.line({-4,-10},{9,-7},1.5,wood_light);
            b.quad({0,-9},{15,-11},{19,-5},{4,-3},wood);
            b.line({4,-3},{4,-10},2,wood_light);b.line({18,-5},{18,-12},2,wood_light);
            b.oval(5,-1,3,3,wood_dark);b.oval(16,-3,3,3,wood_dark);
            for(int i=0;i<std::clamp(pose.cargo,0,4);++i)
                b.rect(4+(i%2)*5,-14-(i/2)*4,4,4,straw);
            if(reflect_cart)for(std::size_t i=0;i<b.mesh.count;++i)
                for(auto& point:b.mesh.triangles[i].points)point.x=-point.x;
        }
    }
    return b.finish();
}

AgricultureMesh prepare_agriculture_building_mesh(simulation::Object kind,int side,unsigned phase,bool supplied) {
    if(side<1 || side>5)throw std::logic_error("Agriculture fallback footprint outside supported geometry");
    Builder b;const bool mill=kind==simulation::Object::OriginalMill53;
    const bool shop=kind==simulation::Object::OriginalFoodShop66;
    const bool pump=kind==simulation::Object::OriginalIrrigationPump;
    const double s=side,cy=-20*(s-1),w=33*s,d=16*s,h=mill ? 58:shop ? 33:38;
    b.quad({0,cy-20*s},{40*s,cy},{0,20},{-40*s,cy},earth);
    b.quad({-w,cy},{0,cy+d},{0,cy+d-h},{-w,cy-h},Color{184,152,104,255});
    b.quad({0,cy+d},{w,cy},{w,cy-h},{0,cy+d-h},Color{145,111,77,255});
    b.quad({-w-4,cy-h},{0,cy-d-h-20},{w+4,cy-h},{0,cy+d-h},
        shop ? Color{92,132,108,255}:Color{112,100,84,255});
    b.line({-w,cy-h},{0,cy+d-h},3,wood_dark);b.line({0,cy+d-h},{w,cy-h},3,wood_dark);
    b.quad({-14,cy+d-1},{4,cy+d-10},{4,cy+d-31},{-14,cy+d-22},wood_dark);
    if(shop) {
        b.quad({-w+5,cy+8},{0,cy+d+3},{18,cy+d-8},{-w+23,cy-3},Color{184,83,60,255});
        for(int i=0;i<3;++i)b.oval(-w+18+i*16,cy+10+i*7,6,3,straw);
    } else if(mill || pump) {
        const P centre{w*.64,cy-h*.36};
        const double radius=pump ? 14:22;
        b.oval(centre.x,centre.y,radius,radius,wood_dark,12);
        b.oval(centre.x,centre.y,radius-4,radius-4,wood_light,12);
        constexpr double tau=6.28318530717958647692;
        for(unsigned i=0;i<6;++i) {
            const auto a=tau*(i+double(phase%4)/4)/6;
            b.line(centre,{centre.x+(radius-4)*std::cos(a),centre.y+(radius-4)*std::sin(a)},2,wood_dark);
        }
        b.oval(centre.x,centre.y,4,4,wood_dark,6);
        if(pump)b.quad({-w,cy+3},{-w+22,cy+14},{-w+29,cy+10},{-w+7,cy-1},supplied ? water:earth);
    } else {
        for(int i=0;i<3;++i)b.rect(-w*.5+i*9,cy+d*.2-14,6,9,wood_light);
    }
    return b.finish();
}

AgricultureMesh prepare_agriculture_crop_mesh(simulation::Object kind,unsigned progress,unsigned target,bool wet) {
    Builder b;b.quad({0,-19},{38,0},{0,19},{-38,0},wet ? Color{84,113,83,255}:earth);
    const double growth=target ? std::clamp(double(progress)/target,0.0,1.0):0;
    const bool rice=kind==simulation::Object::OriginalRiceField;
    const bool cabbage=kind==simulation::Object::OriginalCabbageField;
    if(simulation::original_industry_plant(kind)) {
        const bool tea=kind==simulation::Object::OriginalTeaBush26;
        if(tea)for(const double x:{-13.,0.,13.})b.oval(x,-2-8*growth,6+growth*5,3+growth*4,leaf,8);
        else {
            const auto height=12+growth*22;
            b.line({0,3},{0,-height},3,wood);
            b.line({0,-height*.6},{-8,-height*.9},2,wood);
            b.line({0,-height*.6},{9,-height*.95},2,wood);
            const auto crown=kind==simulation::Object::OriginalMulberryTree28 ? Color{89,143,81,255}:Color{107,125,64,255};
            b.oval(-7,-height,5+growth*6,4+growth*6,crown,8);
            b.oval(7,-height-2,5+growth*6,4+growth*6,crown,8);
        }
        return b.finish();
    }
    if(rice)b.quad({0,-15},{31,0},{0,15},{-31,0},water);
    for(int row=0;row<3;++row)for(int col=0;col<3;++col) {
        const double x=(col-row)*10,y=(row+col)*5-10,h=2+growth*16;
        const Color crop=progress>=target && target ? straw:leaf;
        if(cabbage)b.oval(x,y-h*.25,2+growth*4,2+growth*2,crop,6);
        else {b.line({x,y},{x,y-h},1.5,crop);b.line({x,y-h*.5},{x-3,y-h*.7},1,crop);}
    }
    return b.finish();
}

bool draw_agriculture_mesh(SDL_Renderer* renderer,const AgricultureMesh& mesh,P ground,double zoom,bool preview) {
    if(mesh.count>mesh.triangles.size())return false;
    if(!renderer || !std::isfinite(zoom) || zoom<=0 || !std::isfinite(ground.x) || !std::isfinite(ground.y))return false;
    std::array<SDL_Vertex,AgricultureMesh::triangle_limit*3> vertices{};
    for(std::size_t i=0;i<mesh.count;++i)for(std::size_t v=0;v<3;++v) {
        const auto& t=mesh.triangles[i];const auto p=t.points[v];
        vertices[i*3+v]={{float(ground.x+p.x*zoom),float(ground.y+p.y*zoom)},
            {t.color[0]/255.F,t.color[1]/255.F,t.color[2]/255.F,t.color[3]/255.F*(preview ? .45F:1.F)},{}};
    }
    if(!mesh.count)return true;
    SDL_BlendMode previous; if(!SDL_GetRenderDrawBlendMode(renderer,&previous))return false;
    const bool drawn=SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND) &&
        SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(mesh.count*3),nullptr,0);
    const bool restored=SDL_SetRenderDrawBlendMode(renderer,previous);return drawn && restored;
}
bool hit_agriculture_mesh(const AgricultureMesh& mesh,P p,double tolerance) {
    if(mesh.count>mesh.triangles.size())return false;
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(tolerance) || tolerance<0 || tolerance>1)return false;
    if(p.x<mesh.left-tolerance || p.y<mesh.top-tolerance ||
       p.x>mesh.left+mesh.width+tolerance || p.y>mesh.top+mesh.height+tolerance)return false;
    for(std::size_t i=mesh.count;i>0;--i) {
        const auto& t=mesh.triangles[i-1];if(!t.color[3])continue;
        const auto a=cross(t.points[0],t.points[1],p),b=cross(t.points[1],t.points[2],p),c=cross(t.points[2],t.points[0],p);
        if((a>=0 && b>=0 && c>=0) || (a<=0 && b<=0 && c<=0))return true;
        if(tolerance>0)for(std::size_t edge=0;edge<3;++edge) {
            // Intersect the already prepared edge with the displayed pixel's
            // axis-aligned rounding envelope. SDL software can round both
            // coordinates; a circular radius incorrectly omits diagonal edges.
            const auto u=t.points[edge],v=t.points[(edge+1)%3];double begin=0,end=1;
            const auto clip=[&](double origin,double delta,double centre){
                if(delta==0)return std::abs(origin-centre)<=tolerance;
                auto first=(centre-tolerance-origin)/delta,last=(centre+tolerance-origin)/delta;
                if(first>last)std::swap(first,last);
                begin=std::max(begin,first);end=std::min(end,last);return begin<=end;
            };
            if(clip(u.x,v.x-u.x,p.x) && clip(u.y,v.y-u.y,p.y))return true;
        }
    }
    return false;
}
} // namespace openemperor
