#include "MistwatchRepairToolsVisual.h"
#include "MistwatchWorld.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
unsigned checks=0;
void Need(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
struct D3 {double x,y,z;};
D3 Pos(const Vertex3D& v){return{v.x,v.y,v.z};}
D3 Sub(D3 a,D3 b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
D3 CrossD(D3 a,D3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double DotD(D3 a,D3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
std::array<std::uint32_t,3> Key(const Vertex3D& v){
    const auto bits=[](float x){return x==0?0u:std::bit_cast<std::uint32_t>(x);};
    return{bits(v.x),bits(v.y),bits(v.z)};
}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameVec(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool Same(const Vertex3D& a,const Vertex3D& b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&
    a.color==b.color&&std::bit_cast<std::uint32_t>(a.u)==std::bit_cast<std::uint32_t>(b.u)&&
    std::bit_cast<std::uint32_t>(a.v)==std::bit_cast<std::uint32_t>(b.v);}
struct Box {D3 low,high;};
bool Separated(const Box& a,const Box& b){return a.high.x<b.low.x||b.high.x<a.low.x||
    a.high.y<b.low.y||b.high.y<a.low.y||a.high.z<b.low.z||b.high.z<a.low.z;}

// 封口按真float32 XYZ焊接；不是以重複normal records的index誤判邊界。
void GeometryGate(const RepairToolsGeometry& geometry){
    Need(geometry.metal.Valid()&&geometry.handle.Valid(),"valid actual two meshes");
    Need(geometry.metal.vertices.size()==530&&geometry.metal.indices.size()==804&&
         geometry.handle.vertices.size()==50&&geometry.handle.indices.size()==144,"fixed source counts, not old box");
    std::array<unsigned,2> nextV{},nextI{};std::array<Box,7> boxes{};
    for(unsigned p=0;p<7;++p){
        const auto& spec=RepairToolsSource[p];const auto& range=geometry.parts[p];
        const auto& mesh=range.mesh?geometry.handle:geometry.metal;
        Need(range.name==spec.name&&range.mesh==spec.mesh,"complete source part routing");
        const bool cylinder=spec.primitive==RepairToolPrimitive::CylinderAlongX;
        Need(range.firstVertex==nextV[range.mesh]&&range.firstIndex==nextI[range.mesh]&&
             range.vertexCount==(cylinder?50u:96u)&&range.indexCount==(cylinder?144u:132u),"exact primitive range partition");
        nextV[range.mesh]+=range.vertexCount;nextI[range.mesh]+=range.indexCount;
        Need(nextV[range.mesh]<=mesh.vertices.size()&&nextI[range.mesh]<=mesh.indices.size(),"range limits");
        Box box{{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}};
        std::map<std::array<std::uint32_t,3>,unsigned> welded;
        std::vector<unsigned> logical(range.vertexCount);
        std::set<unsigned> used;
        const auto expectedColor=PackColorRgba8(range.mesh?Color{.86f,.32f,.065f,1}:Color{.66f,.69f,.71f,1});
        for(unsigned v=0;v<range.vertexCount;++v){const auto& a=mesh.vertices[range.firstVertex+v];
            Need(std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&
                 std::isfinite(a.u)&&std::isfinite(a.v),"all actual float fields finite");
            Need(std::abs(a.x)<=.25f&&std::abs(a.y)<=.03f&&std::abs(a.z)<=.15f,"original visual envelope, no epsilon");
            Need(a.u==0&&a.v==0&&a.color==expectedColor,"untextured primitive and metal/handle palette routing");
            const auto n=UnpackNormalSnorm8(a.normal);
            for(unsigned byte=0;byte<4;++byte)Need(std::uint8_t(a.normal>>(byte*8))!=128,"no illegal SNORM -128");
            Need(n.w==1,"fully lit, no packed normal bypass");
            constexpr double error=.5/127.;
            const double norm2=double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z;
            const double bound=2*std::sqrt(3.)*error+3*error*error;
            Need(std::abs(norm2-1)<=bound,"normal quantization derived unit bound");
            if(cylinder){
                std::uint32_t expected;
                if(v>=24)expected=PackNormalSnorm8(v<37?Vec3{-1,0,0}:Vec3{1,0,0});
                else {constexpr float pi=3.14159265358979323846f;
                    const float angle=2.f*pi*float(v/2)/12.f;
                    expected=PackNormalSnorm8({0,std::sin(angle),-std::cos(angle)});}
                Need(a.normal==expected,"independent cylinder signed radial/cap packing");
            }
            const auto id=static_cast<unsigned>(welded.size());
            logical[v]=welded.emplace(Key(a),id).first->second;
            box.low.x=(std::min)(box.low.x,double(a.x));box.low.y=(std::min)(box.low.y,double(a.y));box.low.z=(std::min)(box.low.z,double(a.z));
            box.high.x=(std::max)(box.high.x,double(a.x));box.high.y=(std::max)(box.high.y,double(a.y));box.high.z=(std::max)(box.high.z,double(a.z));
        }
        std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,int>> edges;
        std::set<std::array<unsigned,3>> faces;
        double volume6=0;
        for(unsigned i=range.firstIndex;i<range.firstIndex+range.indexCount;i+=3){
            std::array<unsigned,3> ids{},keys{};
            for(unsigned k=0;k<3;++k){ids[k]=mesh.indices[i+k];
                Need(ids[k]>=range.firstVertex&&ids[k]<range.firstVertex+range.vertexCount,"indices owned by exact part");
                used.insert(ids[k]);keys[k]=logical[ids[k]-range.firstVertex];}
            auto sorted=keys;std::sort(sorted.begin(),sorted.end());
            Need(sorted[0]!=sorted[1]&&sorted[1]!=sorted[2]&&faces.insert(sorted).second,"no degenerate or duplicated face");
            const auto& a=mesh.vertices[ids[0]];const auto& b=mesh.vertices[ids[1]];const auto& c=mesh.vertices[ids[2]];
            const auto n=CrossD(Sub(Pos(b),Pos(a)),Sub(Pos(c),Pos(a)));
            const double area2=DotD(n,n);
            Need(std::isfinite(area2)&&area2>0,"true nonzero double area");
            const D3 fromCenter{double(a.x)-spec.center.x,double(a.y)-spec.center.y,double(a.z)-spec.center.z};
            Need(DotD(n,fromCenter)>0,"convex part truly outward, no automatic flip");
            volume6+=DotD(fromCenter,n);
            for(auto id:ids){const auto unpacked=UnpackNormalSnorm8(mesh.vertices[id].normal);
                const double alignment=DotD(n,{unpacked.x,unpacked.y,unpacked.z})/std::sqrt(area2);
                Need(alignment>.95,"every packed corner normal agrees with true signed face");}
            if(!cylinder){Vec3 actual{};
                Need(TryComputeTriangleNormal({a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z},actual),"shared true geometric normal");
                const auto unpacked=UnpackNormalSnorm8(a.normal);
                Need(Dot(actual,{unpacked.x,unpacked.y,unpacked.z})>.99f,"flat beveled face packing");}
            for(unsigned k=0;k<3;++k){const auto x=keys[k],y=keys[(k+1)%3];
                auto& edge=edges[{(std::min)(x,y),(std::max)(x,y)}];++edge.first;edge.second+=x<y?1:-1;}
        }
        Need(used.size()==range.vertexCount,"no inactive or orphan records");
        for(const auto& entry:edges)Need(entry.second.first==2&&entry.second.second==0,"all welded edges twice/opposite, true closed part");
        Need(static_cast<long long>(welded.size())-static_cast<long long>(edges.size())+
             static_cast<long long>(faces.size())==2&&volume6>0,"closed convex sphere topology and positive volume");
        boxes[p]=box;
    }
    Need(nextV[0]==530&&nextI[0]==804&&nextV[1]==50&&nextI[1]==144,"no unowned appended geometry");
    for(unsigned a=0;a<7;++a)for(unsigned b=a+1;b<7;++b)
        Need(Separated(boxes[a],boxes[b]),"strict actual AABB separating slab: no surface overlap or touching");
    // 開口為原固定設計的真留空；不能以扁平盒仍通用封口來冒稱扳手。
    Need(boxes[3].low.z>boxes[2].high.z&&boxes[1].high.x<boxes[2].high.x&&
         boxes[1].high.x<boxes[3].high.x,"source open jaw mouth and visible paired forks");
}
template<class Mutation>void Negative(const RepairToolsGeometry& original,Mutation change,const char* name){
    auto bad=original;change(bad);bool rejected=false;
    try{GeometryGate(bad);}catch(const std::exception&){rejected=true;}
    Need(rejected,name);
}
void OriginalScope(){
    const auto before=RepairShelterSolids();Need(before.size()==5,"original shelter five equipment/structure parts");
    const auto equipment=before[4];const auto point=Session::repairSites[0];
    Need(point.x==-8&&point.y==0&&point.z==18,"original actual site zero");
    Need(equipment.center.x==point.x&&equipment.center.y==.23f&&equipment.center.z==point.z&&
         equipment.extents.x==.3f&&equipment.extents.y==.23f&&equipment.extents.z==.2f,"original equipment part4 exact");
    Need(RepairToolsOffset.x==0&&RepairToolsOffset.y==.49f&&RepairToolsOffset.z==0&&
         RepairToolsEnvelope.x==.25f&&RepairToolsEnvelope.y==.03f&&RepairToolsEnvelope.z==.15f,"original box center and full envelope");
    Session session;session.collected=7;session.feet=point;
    Need(!session.CollectRepairMaterial(0,false)&&session.repairMaterials==0,"standing cannot collect");
    session.feet=point+Vec3{.81f,0,0};Need(!session.CollectRepairMaterial(0,true),"original near pickup radius unchanged");
    session.feet=point+Vec3{0,.31f,0};Need(!session.CollectRepairMaterial(0,true),"original vertical pickup gate unchanged");
    session.feet=point;Need(session.CollectRepairMaterial(0,true)&&session.repairMaterials==1,"actual crouched pickup mask");
    Need(!session.CollectRepairMaterial(0,true)&&session.repairMaterials==1,"no repeated collection or other bits");
    const auto after=RepairShelterSolids();
    for(unsigned i=0;i<5;++i)Need(SameVec(before[i].center,after[i].center)&&
        SameVec(before[i].extents,after[i].extents),"original all shelter collider fields bit exact");
}
}
int main(){try{
    OriginalScope();const auto geometry=CreateRepairToolsVisual();GeometryGate(geometry);
    const auto repeat=CreateRepairToolsVisual();
    for(unsigned m=0;m<2;++m){const auto& a=m?geometry.handle:geometry.metal;const auto& b=m?repeat.handle:repeat.metal;
        Need(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"same source deterministic topology");
        for(unsigned i=0;i<a.vertices.size();++i)Need(Same(a.vertices[i],b.vertices[i]),"every stored vertex field deterministic");}
    Negative(geometry,[](auto& g){g.metal.indices[0]=static_cast<unsigned>(g.metal.vertices.size());},"out of range negative");
    Negative(geometry,[](auto& g){g.metal.indices[1]=g.metal.indices[0];},"degenerate negative");
    Negative(geometry,[](auto& g){std::swap(g.metal.indices[0],g.metal.indices[1]);},"reversed face negative");
    Negative(geometry,[](auto& g){g.metal.vertices[0].x=std::numeric_limits<float>::quiet_NaN();},"nonfinite negative");
    Negative(geometry,[](auto& g){g.metal.vertices[0].x=.251f;},"old envelope overflow negative");
    Negative(geometry,[](auto& g){g.handle.vertices[0].normal=0;},"zero packed normal negative");
    Negative(geometry,[](auto& g){g.metal.vertices[0].normal&=0xffffffu;},"unlit packed normal bypass negative");
    Negative(geometry,[](auto& g){const auto r=g.parts[4];g.metal.indices[r.firstIndex+6]=g.metal.indices[r.firstIndex];},"opened cylinder cap negative");
    Negative(geometry,[](auto& g){for(unsigned k=0;k<3;++k)g.metal.indices[3+k]=g.metal.indices[k];},"duplicated surface negative");
    Negative(geometry,[](auto& g){const auto r=g.parts[6];for(unsigned i=r.firstVertex;i<r.firstVertex+r.vertexCount;++i)g.handle.vertices[i].x+=.20f;},"interpart surface intersection negative");
    Negative(geometry,[](auto& g){g.handle.vertices[0].color=PackColorRgba8({.66f,.69f,.71f,1});},"material domain negative");
    Negative(geometry,[](auto& g){g.metal=Mesh3D::CreateUnitBox();},"plain source box is not tool structure");
    std::cout<<"REPAIR_TOOLS_CPU_PASS checks="<<checks<<" parts=7 metal_vertices="<<geometry.metal.vertices.size()
        <<" handle_vertices="<<geometry.handle.vertices.size()<<" triangles="<<(geometry.metal.indices.size()+geometry.handle.indices.size())/3
        <<" negatives=12 native=false art=false gameplay_scope=unchanged\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
