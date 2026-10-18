#include <Aevrynd/ConvexAabbSweep3D.h>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Aevrynd;
namespace {
using Hull=ConvexAabbSweep3D;
using Status=ConvexAabbSweepStatus3D;
unsigned checks{};
void Need(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
bool Close(double a,double b,double tolerance=1e-10){return std::abs(a-b)<=tolerance;}
std::array<Vec3,8> Box(float s=1,Vec3 offset={}){
    std::array<Vec3,8> p{};unsigned i{};
    for(float x:{-s,s})for(float y:{-s,s})for(float z:{-s,s})p[i++]=offset+Vec3{x,y,z};
    return p;
}
Hull::Result Sweep(const Hull& h,Vec3 p,Vec3 e,Vec3 d){Hull::Result r;Need(h.Sweep(p,e,d,r),"valid sweep");return r;}
// 獨立 long double 原始軸方盒 oracle，沒有共用 prepared hull／軸產生程式。
Hull::Result Oracle(Vec3 p,Vec3 e,Vec3 d){
    const float c[]{p.x,p.y,p.z},extent[]{e.x,e.y,e.z},delta[]{d.x,d.y,d.z};
    long double enter=-std::numeric_limits<long double>::infinity(),leave=-enter;
    bool overlap=true,parallelBoundary=false,miss=false;
    for(unsigned i=0;i<3;++i){
        const long double low=-1.L-extent[i],high=1.L+extent[i];
        overlap=overlap&&(c[i]>low&&c[i]<high);
        if(delta[i]==0){miss=miss||c[i]<low||c[i]>high;parallelBoundary=parallelBoundary||c[i]==low||c[i]==high;}
        else{long double a=(low-c[i])/delta[i],b=(high-c[i])/delta[i];if(a>b)std::swap(a,b);enter=std::max(enter,a);leave=std::min(leave,b);}
    }
    Hull::Result r;
    if(overlap){r.status=Status::StartedOverlapping;r.first=0;r.last=double(std::clamp(leave,0.L,1.L));}
    else if(!miss&&enter<=leave&&leave>=0&&enter<=1){r.first=double(std::max(enter,0.L));r.last=double(std::min(leave,1.L));r.status=!parallelBoundary&&enter<leave&&leave>0?Status::Hit:Status::Touching;}
    return r;
}
void Basic(){
    Hull h;auto points=Box();Need(h.Prepare(points),"box prepare");Need(h.AxisCount()<=Hull::MaximumAxes,"bounded axes");
    const Vec3 e{.5f,.5f,.5f};
    auto r=Sweep(h,{-3,0,0},e,{4,0,0});Need(r.status==Status::Hit&&Close(r.first,.375),"cross box");
    Vec3 n;bool left=false;for(std::size_t i=0;i<h.AxisCount();++i)if(r.negativeContacts[i]){Need(h.Normal(i,false,n),"normal");left=left||n.x<-.999f;}Need(left,"left outward normal");
    Need(Sweep(h,{0,0,0},e,{}).status==Status::StartedOverlapping,"overlap zero");
    Need(Sweep(h,{-3,0,0},e,{}).status==Status::Separated,"separated zero");
    Need(Sweep(h,{-1.5f,0,0},e,{1,0,0}).status==Status::Hit,"touch inward");
    Need(Sweep(h,{-1.5f,0,0},e,{-1,0,0}).status==Status::Touching,"touch outward");
    Need(Sweep(h,{-1.5f,0,0},e,{0,1,0}).status==Status::Touching,"touch tangent");
    Need(Sweep(h,{-1.5f,0,0},e,{}).status==Status::Touching,"touch stationary");
    r=Sweep(h,{-2.5f,0,0},e,{1,0,0});Need(r.status==Status::Hit&&r.first==1,"endpoint");
    r=Sweep(h,{-2.5f,-2.5f,0},e,{2,2,0});Need(r.status==Status::Hit&&Close(r.first,.5),"corner time");
    bool x=false,y=false;for(std::size_t i=0;i<h.AxisCount();++i)if(r.negativeContacts[i]){Need(h.Normal(i,false,n),"corner normal");x=x||n.x<-.999f;y=y||n.y<-.999f;}Need(x&&y,"simultaneous corner axes");
    uint32_t random=41793;auto value=[&](){random=random*1664525u+1013904223u;return float(int((random>>8)%129)-64)/16.f;};
    for(unsigned i=0;i<2000;++i){
        Vec3 p{value(),value(),value()},d{value(),value(),value()},ext{std::abs(value())+.0625f,std::abs(value())+.0625f,std::abs(value())+.0625f};
        const auto actual=Sweep(h,p,ext,d),expected=Oracle(p,ext,d);
        Need(actual.status==expected.status,"box oracle status");
        Need(Close(actual.first,expected.first)&&Close(actual.last,expected.last),"box oracle interval");
    }
    for(float scale:{.00000001f,.0001f,1.f,10000.f,100000000.f}){
        const auto p=Box(scale);Need(h.Prepare(p),"scaled hull");
        r=Sweep(h,{-3*scale,0,0},{scale*.5f,scale*.5f,scale*.5f},{4*scale,0,0});
        Need(r.status==Status::Hit&&Close(r.first,.375,1e-7),"scaled crossing");
    }
}
void Invalid(){
    Hull h;Hull::Result r;r.first=.123;r.status=Status::Hit;
    Need(!h.Sweep({},{1,1,1},{},r)&&r.first==.123&&r.status==Status::Hit,"unprepared output atomic");
    auto points=Box();Need(h.Prepare(points),"valid before reject");const auto count=h.AxisCount();
    auto bad=points;bad[0]=bad[1];Need(!h.Prepare(bad)&&h.AxisCount()==count,"duplicate atomic");
    bad=points;bad[0].x=std::numeric_limits<float>::quiet_NaN();Need(!h.Prepare(bad),"NaN reject");
    bad=points;bad[0].y=std::numeric_limits<float>::infinity();Need(!h.Prepare(bad),"infinity reject");
    Need(!h.Prepare(std::span(points).first(3)),"short reject");
    std::array<Vec3,Hull::MaximumPoints+1> excess{};Need(!h.Prepare(excess),"bound reject");
    std::array<Vec3,4> flat{{{0,0,0},{1,0,0},{0,1,0},{1,1,0}}};Need(!h.Prepare(flat),"coplanar reject");
    const std::array<Vec3,4> cancellation{{{0,0,0},{2306372,2326494,-754808},{3155055,4096980,-4236157},{5461427,6423474,-4990965}}};
    Need(!h.Prepare(cancellation),"exactly coplanar cancellation reject");
    const std::array<Vec3,4> reviewedFlat{{{0,0,0},{262144,262145,262147},{262148,524294,1048586},{524292,786439,1310733}}};
    Need(!h.Prepare(reviewedFlat),"independent coplanar cancellation reject");
    Need(!h.Sweep({},{0,1,1},{},r)&&r.first==.123,"zero extent reject");
    Need(!h.Sweep({},{-1,1,1},{},r)&&r.first==.123,"negative extent reject");
    Need(!h.Sweep({},{1,1,1},{std::numeric_limits<float>::infinity(),0,0},r)&&r.first==.123,"invalid displacement atomic");
    Need(Sweep(h,{-3,0,0},{.5f,.5f,.5f},{4,0,0}).first==.375,"old hull survives invalid prepare");
    Vec3 n{3,4,5};Need(!h.Normal(Hull::MaximumAxes,true,n)&&n.x==3&&n.y==4,"normal bounds atomic");
}
void Roof(){
    // 屋頂解析碰撞夾具，非已接入 m2782 視覺網格；刻意不使用大包圍盒。
    std::array<Vec3,10> points{};unsigned i{};
    for(float x:{-9.5f,-4.5f})for(Vec3 yz:std::array<Vec3,5>{{{0,2.85f,-1.94f},{0,2.85f,1.94f},{0,2.92f,1.94f},{0,3.76f,0},{0,2.92f,-1.94f}}})points[i++]={x,yz.y,59+yz.z};
    Hull h;Need(h.Prepare(points),"roof prepare");const Vec3 ext{.45f,.9f,.45f};
    for(float z:{58.5f,59.f,59.5f}){
        auto cross=Sweep(h,{-3,3.9f,z},ext,{-4,0,0});
        Need(cross.status==Status::Hit&&Close(cross.first,(1.5-double(ext.x))/4,1e-7),"three gable crossings");
        const auto drop=Sweep(h,{-7,5.4f,z},ext,{0,-3,0});
        const double feet=double(5.4f)-double(ext.y)-3*drop.first;
        const double expected=double(3.76f)-(double(3.76f)-double(2.92f))/(double(points[2].z)-59)*std::max(std::abs(double(z)-59)-double(ext.z),0.);
        Need(drop.status==Status::Hit&&Close(feet,expected,1e-6),"finite body slope/ridge support");
        std::cout<<"roof z="<<z<<" crossing_t="<<cross.first<<" contact_feet="<<feet<<" analytic_feet="<<expected<<'\n';
    }
    const auto drop=Sweep(h,{-7,5.4f,60.25f},ext,{0,-3,0});
    const double feet=double(5.4f)-ext.y-3*drop.first;Need(feet<3.5&&feet>3.3,"not lifted to ridge AABB");
    Need(Sweep(h,{-3,.9f,59},ext,{-4,0,0}).status==Status::Separated,"entry headroom preserved");
    Need(Sweep(h,{-7,2,59},ext,{0,2,0}).status==Status::StartedOverlapping,"starts across roof bottom");
    auto reversed=points;std::reverse(reversed.begin(),reversed.end());Hull other;Need(other.Prepare(reversed),"reordered hull");
    const auto reordered=Sweep(other,{-7,5.4f,60.25f},ext,{0,-3,0});Need(reordered.status==drop.status&&Close(reordered.first,drop.first),"reordered interval");
}
bool Separates(Vec3 center,Vec3 extents,Vec3 axis,std::span<const Vec3> points){
    double lo=std::numeric_limits<double>::infinity(),hi=-lo;
    for(auto p:points){const double q=double(axis.x)*p.x+double(axis.y)*p.y+double(axis.z)*p.z;lo=std::min(lo,q);hi=std::max(hi,q);}
    const double p=double(axis.x)*center.x+double(axis.y)*center.y+double(axis.z)*center.z;
    const double r=std::abs(double(axis.x))*extents.x+std::abs(double(axis.y))*extents.y+std::abs(double(axis.z))*extents.z;
    return p+r<lo||p-r>hi;
}
void EdgeAndThin(){
    const std::array<Vec3,4> tetra{{{0,0,0},{2,1,0},{0,2,1},{1,0,2}}};
    const Vec3 p{-.125f,2,1.5f},ext{.125f,.25f,.375f};
    for(auto n:std::array<Vec3,3>{{{1,0,0},{0,1,0},{0,0,1}}})Need(!Separates(p,ext,n,tetra),"edge fixture overlaps xyz");
    for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)for(unsigned k=j+1;k<4;++k)
        Need(!Separates(p,ext,Cross(tetra[j]-tetra[i],tetra[k]-tetra[i]),tetra),"edge fixture overlaps all face axes");
    Need(Separates(p,ext,{-2,0,1},tetra),"independent edge separator");
    Hull h;Need(h.Prepare(tetra),"oblique tetra prepare");Need(Sweep(h,p,ext,{}).status==Status::Separated,"edge-cross axis required");
    auto thin=Box();for(auto& v:thin)v.x*=.02f;Need(h.Prepare(thin),"thin gable prepare");
    auto hit=Sweep(h,{-100,0,0},{.45f,.9f,.45f},{200,0,0});
    Need(hit.status==Status::Hit&&Close(hit.first,(100.-double(.02f)-double(.45f))/200.),"high-speed 4cm gable");
    Need(h.Prepare(Box()),"tiny move box");
    hit=Sweep(h,{-1.5f,0,0},{.5f,.5f,.5f},{1e-30f,0,0});Need(hit.status==Status::Hit&&hit.first==0,"tiny inward not discarded");
    std::array<Vec3,Hull::MaximumPoints> maximum{};for(unsigned i=0;i<Hull::MaximumPoints;++i){const auto x=float(i);maximum[i]={x,x*x,x*x*x};}
    Need(h.Prepare(maximum)&&h.AxisCount()==Hull::MaximumAxes,"all bounded axes capacity");
    Need(Sweep(h,{10000,10000,10000},{1,1,1},{}).status==Status::Separated,"maximum query");
}
}
int main()try{Basic();Invalid();Roof();EdgeAndThin();std::cout<<"CONVEX_AABB_SWEEP_PASS checks="<<checks<<" box_oracle_cases=2000 roof_crossings=3 roof_landings=3 controller_integrated=0\n";return 0;}
catch(const std::exception& e){std::cerr<<"CONVEX_AABB_SWEEP_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}
