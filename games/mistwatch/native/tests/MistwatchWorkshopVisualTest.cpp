#include "MistwatchWorkshopVisual.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

namespace {
using namespace Aevrynd;
unsigned checks{},negatives{};
void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
float Axis(Vec3 v,unsigned axis){return axis==0?v.x:axis==1?v.y:v.z;}
constexpr std::array<BoxCollider3D,4> Source{{
    {{-7,2.7f,59},{2.5f,.15f,2}},{{-9.4f,1.3f,59},{.1f,1.3f,2}},
    {{-7,1.3f,57.1f},{2.5f,1.3f,.1f}},{{-7,1.3f,60.9f},{2.5f,1.3f,.1f}}}};
constexpr std::array<unsigned,4> Counts{20,16,20,20},Axes{0,2,0,0};
constexpr std::array<Vec2,4> SourceBands{{{132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
    {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
struct Bounds{Vec3 low,high;};
Bounds ExpectedBounds(unsigned part,unsigned board) {
    // 直接由獨立literal四碰撞box推算板位置；不呼叫候選的counts/bounds/UV函式。
    const auto s=Source[part];const unsigned axis=Axes[part];
    const float length=2.f*Axis(s.extents,axis),center=Axis(s.center,axis),step=length/float(Counts[part]);
    const float localLow=-length*.5f+float(board)*step+(board==0?0.f:.0035f);
    const float localHigh=-length*.5f+float(board+1)*step-(board+1==Counts[part]?0.f:.0035f);
    const float lo=(std::max)(center-length*.5f,center+localLow);
    const float hi=(std::min)(center+length*.5f,center+localHigh);
    const float mid=lo+(hi-lo)*.5f,radius=std::nextafter((std::min)(mid-lo,hi-mid),0.f);
    Vec3 c=s.center,h=s.extents;
    if(axis==0){c.x=mid;h.x=radius;}else{c.z=mid;h.z=radius;}
    return {c-h,c+h};
}
Bounds ActualBounds(const Mesh3D& mesh,std::size_t first) {
    Vec3 lo{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()},hi=lo*-1.f;
    for(std::size_t i=first;i<first+96u;++i){const auto& v=mesh.vertices[i];
        lo.x=(std::min)(lo.x,v.x);hi.x=(std::max)(hi.x,v.x);lo.y=(std::min)(lo.y,v.y);hi.y=(std::max)(hi.y,v.y);
        lo.z=(std::min)(lo.z,v.z);hi.z=(std::max)(hi.z,v.z);}
    return {lo,hi};
}
Vec2 ExpectedUv(const Vertex3D& v,const Bounds& b,unsigned part,unsigned board) {
    const auto n=UnpackNormalSnorm8(v.normal);
    const float nx=std::abs(n.x),ny=std::abs(n.y),nz=std::abs(n.z);
    const auto unit=[](float p,float lo,float hi){return (p-lo)/(hi-lo);};
    float transverse{},meters{};
    if(part==0){
        if(ny>=nx&&ny>=nz){transverse=unit(v.x,b.low.x,b.high.x);meters=v.z-b.low.z;}
        else if(nx>=nz){transverse=unit(v.y,b.low.y,b.high.y);meters=v.z-b.low.z;}
        else{transverse=unit(v.y,b.low.y,b.high.y);meters=v.x-b.low.x;}
    }else if(ny>=nx&&ny>=nz){
        transverse=part==1?unit(v.z,b.low.z,b.high.z):unit(v.x,b.low.x,b.high.x);
        const float depth=part==1?unit(v.x,b.low.x,b.high.x):unit(v.z,b.low.z,b.high.z);
        const float top=v.y>=b.low.y+(b.high.y-b.low.y)*.5f?b.high.y-b.low.y:0.f;
        meters=top+depth*.05f;
    }else if(nx>=nz){transverse=unit(v.z,b.low.z,b.high.z);meters=v.y-b.low.y;}
    else{transverse=unit(v.x,b.low.x,b.high.x);meters=v.y-b.low.y;}
    const auto source=SourceBands[(board+part*3u)%4u];
    const float scale=2048.f*(16.f/32767.f);
    return {(source.x+transverse*(source.y-source.x))/scale,
        (.17f*float((board+part*2u)%5u)+meters/1.4f)/scale};
}
double Jacobian(Vec2 a,Vec2 b,Vec2 c){return (double(b.x)-a.x)*(double(c.y)-a.y)-(double(c.x)-a.x)*(double(b.y)-a.y);}
bool Validate(const Mesh3D& mesh) {
    if(!mesh.Valid()||mesh.vertices.size()!=7296u||mesh.indices.size()!=10032u)return false;
    std::size_t first{},indexFirst{};std::array<unsigned,4> used{};
    for(unsigned part=0;part<4u;++part) {
        Bounds previous{};
        for(unsigned board=0;board<Counts[part];++board) {
            const Bounds expected=ExpectedBounds(part,board),actual=ActualBounds(mesh,first);
            if(!Same(expected.low,actual.low)||!Same(expected.high,actual.high))return false;
            const auto s=Source[part];const Vec3 sourceLow=s.center-s.extents,sourceHigh=s.center+s.extents;
            if(board&&!(Axis(actual.low,Axes[part])-Axis(previous.high,Axes[part])>=.0069f))return false;
            previous=actual;const auto band=SourceBands[(board+part*3u)%4u];++used[(board+part*3u)%4u];
            const float shade=.86f+.02f*float((board*5u+part*3u)%7u);
            const auto color=PackColorRgba8({shade,shade,shade,1});
            unsigned flat{},edge{},corner{};
            for(std::size_t i=first;i<first+96u;++i) {
                const auto& v=mesh.vertices[i];
                if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||
                   v.x<sourceLow.x||v.x>sourceHigh.x||v.y<sourceLow.y||v.y>sourceHigh.y||
                   v.z<sourceLow.z||v.z>sourceHigh.z||v.color!=color)return false;
                // 每個真倒角頂點落在外平面或向內4mm的平面，不能還原成尖角整盒。
                const Vec3 p{v.x,v.y,v.z};unsigned outer{};
                for(unsigned axis=0;axis<3u;++axis) {
                    const float value=Axis(p,axis),lo=Axis(actual.low,axis),hi=Axis(actual.high,axis);
                    const float nearest=(std::min)({std::abs(value-lo),std::abs(value-hi),std::abs(value-(lo+.004f)),std::abs(value-(hi-.004f))});
                    if(nearest>2.e-5f)return false;
                    outer+=std::abs(value-lo)<=2.e-5f||std::abs(value-hi)<=2.e-5f;
                }
                if(outer!=1u)return false;
                const auto n=UnpackNormalSnorm8(v.normal);
                const unsigned nonzero=unsigned(std::abs(n.x)>.01f)+unsigned(std::abs(n.y)>.01f)+unsigned(std::abs(n.z)>.01f);
                if(nonzero==1u)++flat;else if(nonzero==2u)++edge;else if(nonzero==3u)++corner;else return false;
                const Vec2 uv=ExpectedUv(v,actual,part,board);
                if(!Bits(v.u,uv.x)||!Bits(v.v,uv.y))return false;
                const float u=v.u*(2048.f*(16.f/32767.f));
                if(u<band.x-1.e-7f||u>band.y+1.e-7f)return false;
            }
            if(flat!=24u||edge!=48u||corner!=24u)return false;
            // 每板18個四角面與8個三角角面必須完整覆蓋；重複合法面不能掩蓋缺面。
            for(unsigned face=0;face<18u;++face){
                const std::array<unsigned,6> local{0,1,2,0,2,3};
                for(unsigned j=0;j<6u;++j)
                    if(mesh.indices[indexFirst+face*6u+j]!=first+face*4u+local[j])return false;
            }
            for(unsigned cornerFace=0;cornerFace<8u;++cornerFace)for(unsigned j=0;j<3u;++j)
                if(mesh.indices[indexFirst+108u+cornerFace*3u+j]!=first+72u+cornerFace*3u+j)return false;
            const Vec3 boardCenter=(actual.low+actual.high)*.5f;
            for(std::size_t k=indexFirst;k<indexFirst+132u;k+=3u) {
                for(unsigned j=0;j<3u;++j)if(mesh.indices[k+j]<first||mesh.indices[k+j]>=first+96u)return false;
                const auto& a=mesh.vertices[mesh.indices[k]];const auto& b=mesh.vertices[mesh.indices[k+1]];
                const auto& c=mesh.vertices[mesh.indices[k+2]];
                const Vec3 p{a.x,a.y,a.z},e1=Vec3{b.x,b.y,b.z}-p,e2=Vec3{c.x,c.y,c.z}-p;
                Vec3 n{};if(!TryComputeTriangleNormal(p,p+e1,p+e2,n))return false;
                const auto packed=UnpackNormalSnorm8(a.normal);
                if(a.normal!=b.normal||a.normal!=c.normal||Dot(n,{packed.x,packed.y,packed.z})<=.99f||Dot(n,p-boardCenter)<=0.f)return false;
                const double jac=Jacobian({a.u,a.v},{b.u,b.v},{c.u,c.v});
                const double oracle=Jacobian(ExpectedUv(a,actual,part,board),ExpectedUv(b,actual,part,board),ExpectedUv(c,actual,part,board));
                if(!std::isfinite(jac)||jac==0||!std::isfinite(oracle)||oracle==0||std::signbit(jac)!=std::signbit(oracle))return false;
                const float du1=b.u-a.u,dv1=b.v-a.v,du2=c.u-a.u,dv2=c.v-a.v;
                const Vec3 tangent=(e1*dv2-e2*dv1)*float(1.0/jac),bitangent=(e2*du1-e1*du2)*float(1.0/jac);
                const float handed=Dot(Cross(tangent,bitangent),n);
                if(!std::isfinite(handed)||handed==0.f||std::signbit(handed)!=std::signbit(jac))return false;
                const Vec3 rebuilt=tangent*du1+bitangent*dv1;
                if(std::abs(rebuilt.x-e1.x)>1.e-4f||std::abs(rebuilt.y-e1.y)>1.e-4f||std::abs(rebuilt.z-e1.z)>1.e-4f)return false;
            }
            first+=96u;indexFirst+=132u;
        }
    }
    return first==7296u&&indexFirst==10032u&&std::all_of(used.begin(),used.end(),[](unsigned n){return n==19u;});
}
std::uint64_t IndependentUvHash(const Mesh3D& mesh) {
    std::uint64_t h=14695981039346656037ull;
    for(const auto& v:mesh.vertices)for(const float value:{v.u,v.v}) {
        const auto word=std::bit_cast<std::uint32_t>(value);
        for(unsigned shift=0;shift<32u;shift+=8u){h^=(word>>shift)&255u;h*=1099511628211ull;}
    }
    return h;
}
template<class Change>void Reject(const Mesh3D& mesh,Change change,const char* reason){auto broken=mesh;change(broken);++negatives;Check(!Validate(broken),reason);}
}

int main()try {
    const auto before=Mistwatch::WorkshopSolids();
    Check(before.size()==4u,"Original four Workshop solids");
    for(unsigned i=0;i<4u;++i)Check(Same(before[i].center,Source[i].center)&&Same(before[i].extents,Source[i].extents),"Independent literal original solid dimensions");
    Check(Same(Mistwatch::Session::fuse,{-7,0,59}),"Original fuse interaction root");
    const auto cabinet=Mistwatch::FuseCaseSolid();
    Check(Same(cabinet.center,{-7.9f,.45f,59})&&Same(cabinet.extents,{.35f,.45f,.4f}),"Original fuse cabinet collider");
    CollisionWorld3D world;
    for(const auto s:before)Check(bool(world.CreateBox({s})),"Original workshop collider installed");
    Check(bool(world.CreateBox({cabinet})),"Original fuse cabinet installed");
    const auto stateBefore=world.StateHash();
    // 東向入口的完整站姿路徑、三牆與屋頂仍由原collider阻擋，不把板縫當通道。
    for(unsigned i=0;i<=40u;++i)Check(!world.OverlapsBox({-3.f-.1f*float(i),.9f,59},{.25f,.9f,.25f}),"East entry standing route unchanged");
    Check(world.OverlapsBox({-9.4f,1.3f,59},{.1f,.2f,.1f}),"West wall remains solid");
    Check(world.OverlapsBox({-7,1.3f,57.1f},{.1f,.2f,.1f}),"South wall remains solid");
    Check(world.OverlapsBox({-7,1.3f,60.9f},{.1f,.2f,.1f}),"North wall remains solid");
    Check(world.OverlapsBox({-7,2.7f,59},{.1f,.1f,.1f}),"Roof collision remains solid");
    const auto a=Mistwatch::WorkshopVisual::Create(),b=Mistwatch::WorkshopVisual::Create();
    Check(Validate(a)&&Validate(b),"All literal board bounds seams bevels UV tangent and face invariants");
    Check(a.indices==b.indices,"Repeat index topology");
    for(std::size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        Check(Bits(x.x,y.x)&&Bits(x.y,y.y)&&Bits(x.z,y.z)&&Bits(x.u,y.u)&&Bits(x.v,y.v)&&x.normal==y.normal&&x.color==y.color,"All vertex fields repeat bit exact");}
    Check(Mistwatch::WorkshopVisual::Fingerprint(a)==IndependentUvHash(a)&&IndependentUvHash(a)==IndependentUvHash(b),"Independent complete UV fingerprint");
    Check(world.StateHash()==stateBefore,"Visual creation does not change collision world");
    const auto after=Mistwatch::WorkshopSolids();
    Check(after.size()==4u,"Original collision count after visual creation");
    for(unsigned i=0;i<4u;++i)Check(Same(after[i].center,Source[i].center)&&Same(after[i].extents,Source[i].extents),"All original collider bits unchanged");
    Mistwatch::Session session;session.guardianDefeated=true;session.fuseClueRead=true;session.feet={-7,0,59};
    for(unsigned wheel=0;wheel<3u;++wheel)for(unsigned step=0;step<std::array<unsigned,3>{2,0,3}[wheel];++step)
        Check(session.CycleFuseDial(wheel),"Original interior fuse dial access");
    Check(session.CollectFuse()&&session.fuseCollected,"Original solved fuse interaction still available");
    Mistwatch::Session outside;outside.guardianDefeated=true;outside.fuseClueRead=true;outside.fuseDials=50;outside.feet={-10,0,59};
    Check(!outside.CycleFuseDial(0)&&!outside.CollectFuse(),"No interaction through west wall");
    outside.feet={-7,2.85f,59};Check(!outside.CycleFuseDial(0)&&!outside.CollectFuse(),"No interaction from roof");
    Reject(a,[](Mesh3D& m){m.vertices.pop_back();},"Missing complete board vertex membership rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].x=-20.f;},"Geometry outside original box rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].x+=.002f;},"Non-beveled plane vertex deformation rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].u=0.f;},"Wrong source band rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].v+=.01f;},"Wrong longitudinal phase rejected");
    Reject(a,[](Mesh3D& m){for(std::size_t i=0;i<96u;++i)std::swap(m.vertices[i].u,m.vertices[i].v);},"Wrong grain projection rejected");
    Reject(a,[](Mesh3D& m){for(std::size_t i=0;i<96u;++i)m.vertices[i].u=m.vertices[i].v=0.f;},"Degenerate UV rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].v=std::numeric_limits<float>::quiet_NaN();},"Nonfinite UV rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].normal^=1u;},"Packed face normal mutation rejected");
    Reject(a,[](Mesh3D& m){m.vertices[0].color^=1u;},"Board tint mutation rejected");
    Reject(a,[](Mesh3D& m){std::swap(m.indices[0],m.indices[1]);},"Inward winding rejected");
    Reject(a,[](Mesh3D& m){m.indices[0]=96u;},"Triangle crossing board membership rejected");
    Reject(a,[](Mesh3D& m){for(unsigned j=0;j<3u;++j)m.indices[3u+j]=m.indices[j];},"Duplicate valid triangle masking missing face rejected");
    std::cout<<"WORKSHOP_VISUAL_CPU_PASS checks="<<checks<<" negatives="<<negatives
        <<" boards=76 vertices=7296 indices=10032 triangles=3344 source_solids=4 uv_fingerprint="<<IndependentUvHash(a)
        <<" collision_changed=false native=false art=false\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
