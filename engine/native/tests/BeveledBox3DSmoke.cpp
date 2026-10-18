#include <Aevrynd/BeveledBox3D.h>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace Aevrynd;
unsigned checks{},cases{},negatives{};
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
struct Box {Vec3 center{},size{1,1,1};float bevel{.006f};Color tint{.76f,.72f,.65f,1};
    Vec3 a{1,0,0},b{0,1,0},c{0,0,1};};
bool Append(Mesh3D& mesh,const Box& box){return AppendBeveledBox3D(mesh,box.center,box.size,box.bevel,box.tint,box.a,box.b,box.c);}
bool RawEqual(const Mesh3D& a,const Mesh3D& b) {
    static_assert(sizeof(Vertex3D)==28 && offsetof(Vertex3D,u)==20 && offsetof(Vertex3D,v)==24);
    return a.vertices.size()==b.vertices.size()&&a.indices.size()==b.indices.size()&&
        (a.vertices.empty()||std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex3D))==0)&&
        (a.indices.empty()||std::memcmp(a.indices.data(),b.indices.data(),a.indices.size()*sizeof(std::uint32_t))==0);
}
using D3=std::array<double,3>;
D3 Point(const Vertex3D& p){return {double(p.x),double(p.y),double(p.z)};}
D3 Sub(D3 a,D3 b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
D3 CrossD(D3 a,D3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double DotD(D3 a,D3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
std::array<std::uint32_t,3> Key(const Vertex3D& v) {
    const auto bits=[](float value){return value==0.f?0u:std::bit_cast<std::uint32_t>(value);};
    return {bits(v.x),bits(v.y),bits(v.z)};
}
void Geometry(const Mesh3D& mesh,const Box& box) {
    Require(mesh.Valid()&&mesh.vertices.size()==96&&mesh.indices.size()==132,"44-triangle topology");
    const D3 center{box.center.x,box.center.y,box.center.z};
    const std::array<Vec3,3> axes{box.a,box.b,box.c};
    const D3 h{double(box.size.x)*.5,double(box.size.y)*.5,double(box.size.z)*.5};
    const double tolerance=16.0*double(std::numeric_limits<float>::epsilon())*
        (1.0+std::abs(double(box.center.x))+std::abs(double(box.center.y))+std::abs(double(box.center.z))+
        16.0*double(std::numeric_limits<float>::epsilon())*(h[0]+h[1]+h[2]));
    D3 low=Point(mesh.vertices.front()),high=low;
    std::map<std::array<std::uint32_t,3>,unsigned> welded;
    std::vector<unsigned> ids;
    for(const auto& v:mesh.vertices) {
        Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"finite vertex");
        Require(v.u==0.f&&v.v==0.f&&v.color==PackColorRgba8(box.tint),"unchanged zero UV/color");
        const D3 delta=Sub(Point(v),center);
        for(unsigned axis=0;axis<3;++axis) {
            const D3 basis{axes[axis].x,axes[axis].y,axes[axis].z};
            Require(std::abs(DotD(delta,basis))<=h[axis]+tolerance,"true local halfsize");
            low[axis]=(std::min)(low[axis],double(axis==0?v.x:axis==1?v.y:v.z));
            high[axis]=(std::max)(high[axis],double(axis==0?v.x:axis==1?v.y:v.z));
        }
        const auto [found,inserted]=welded.emplace(Key(v),static_cast<unsigned>(welded.size()));
        (void)inserted;ids.push_back(found->second);
    }
    Require(welded.size()==24,"24 distinct chamfer corners");
    std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,int>> edges;
    double volume{};
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
        const auto a=Point(mesh.vertices[ia]),b=Point(mesh.vertices[ib]),c=Point(mesh.vertices[ic]);
        const auto cross=CrossD(Sub(b,a),Sub(c,a));
        const double length=std::sqrt(DotD(cross,cross));
        Require(std::isfinite(length)&&length>0.0,"independent nondegenerate triangle");
        const D3 middle{(a[0]+b[0]+c[0])/3.0,(a[1]+b[1]+c[1])/3.0,(a[2]+b[2]+c[2])/3.0};
        Require(DotD(cross,Sub(middle,center))>0.0,"outward actual winding");
        for(const auto vertex:{ia,ib,ic}) {
            const auto n=UnpackNormalSnorm8(mesh.vertices[vertex].normal);
            Require(n.w==1.f&&DotD(cross,{n.x,n.y,n.z})/length>.99,"packed normal matches true face");
        }
        volume+=DotD(Sub(a,center),CrossD(Sub(b,center),Sub(c,center)))/6.0;
        for(const auto edge:std::array<std::pair<unsigned,unsigned>,3>{{{ids[ia],ids[ib]},{ids[ib],ids[ic]},{ids[ic],ids[ia]}}}) {
            Require(edge.first!=edge.second,"no welded zero edge");
            const auto key=std::minmax(edge.first,edge.second);
            auto& count=edges[{key.first,key.second}];++count.first;count.second+=edge.first<edge.second?1:-1;
        }
    }
    Require(std::isfinite(volume)&&volume>0.0,"closed positive volume");
    Require(edges.size()==66&&welded.size()+44==edges.size()+2,"sphere Euler characteristic");
    for(const auto& [edge,count]:edges){(void)edge;Require(count.first==2&&count.second==0,"closed oriented manifold");}
    const auto bounds=mesh.Bounds();
    for(unsigned axis=0;axis<3;++axis) {
        // 凸倒角盒的支持函數：不是未倒角盒八角點的保守AABB。
        const D3 row{double(axis==0?box.a.x:axis==1?box.a.y:box.a.z),
                     double(axis==0?box.b.x:axis==1?box.b.y:box.b.z),
                     double(axis==0?box.c.x:axis==1?box.c.y:box.c.z)};
        const double sum=std::abs(row[0])+std::abs(row[1])+std::abs(row[2]);
        const double extent=std::abs(row[0])*h[0]+std::abs(row[1])*h[1]+std::abs(row[2])*h[2]-
            double(box.bevel)*(sum-(std::max)({std::abs(row[0]),std::abs(row[1]),std::abs(row[2])}));
        Require(std::abs(high[axis]-center[axis]-extent)<=tolerance&&
                std::abs(low[axis]-center[axis]+extent)<=tolerance,"independent true support bounds");
        const double actualCenter=axis==0?bounds.center.x:axis==1?bounds.center.y:bounds.center.z;
        const double actualExtent=axis==0?bounds.extents.x:axis==1?bounds.extents.y:bounds.extents.z;
        Require(std::abs(actualCenter-center[axis])<=tolerance&&std::abs(actualExtent-extent)<=tolerance,"Mesh3D actual bounds");
    }
    ++cases;
}
void Reject(const Box& box) {
    Mesh3D mesh=Mesh3D::CreateUnitBox();const Mesh3D before=mesh;
    const auto cache=mesh.raycastCache;
    const auto cached=std::make_shared<MeshRaycastIndex3D>();cache->index.store(cached);
    Require(!Append(mesh,box),"invalid rejected");
    Require(RawEqual(mesh,before)&&mesh.raycastCache==cache&&cache->index.load()==cached,"transaction preserves content/cache");
    ++negatives;
}
void Negative() {
    const Box valid{};
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(const float bad:{nan,inf,-inf}) {
        for(unsigned field=0;field<7;++field) {
            Box box=valid;
            if(field==0)box.center.x=bad;else if(field==1)box.size.y=bad;else if(field==2)box.bevel=bad;
            else if(field==3)box.tint.b=bad;else if(field==4)box.a.z=bad;else if(field==5)box.b.x=bad;else box.c.y=bad;
            Reject(box);
        }
    }
    for(const float bad:{0.f,-1.f}){Box box=valid;box.size.x=bad;Reject(box);box=valid;box.bevel=bad;Reject(box);}
    for(const float bad:{.5f,1.f}){Box box=valid;box.bevel=bad;Reject(box);}
    for(unsigned mode=0;mode<5;++mode) {
        Box box=valid;
        if(mode==0)box.a={};else if(mode==1)box.a={2,0,0};else if(mode==2)box.b=box.a;
        else if(mode==3)box.c={0,0,-1};else box.b={.001f,1,0};
        Reject(box);
    }
    Box box=valid;box.center={1e30f,1e30f,1e30f};Reject(box);
    box=valid;box.size={1e20f,1e20f,1e20f};box.bevel=1e18f;Reject(box);
    box=valid;box.size={1e-20f,1e-20f,1e-20f};box.bevel=1e-22f;Reject(box);
    box=valid;box.size.x=std::numeric_limits<float>::denorm_min();Reject(box);
    Mesh3D malformed=Mesh3D::CreateUnitBox();malformed.indices[0]=std::numeric_limits<std::uint32_t>::max();
    const Mesh3D before=malformed;Require(!Append(malformed,valid)&&RawEqual(malformed,before),"bad destination index preserved");
    malformed=Mesh3D::CreateUnitBox();malformed.indices.pop_back();const Mesh3D uneven=malformed;
    Require(!Append(malformed,valid)&&RawEqual(malformed,uneven),"nontriangle destination preserved");
    constexpr auto cap=std::size_t(std::numeric_limits<std::uint32_t>::max());
    using BeveledBoxDetail3D::CanAppendCounts;
    Require(CanAppendCounts(cap-96,cap-132),"exact uint32 count boundary");
    Require(!CanAppendCounts(cap-95,0)&&!CanAppendCounts(0,cap-129),"uint32 overflow rejected before allocation");
    Require(!CanAppendCounts(std::numeric_limits<std::size_t>::max(),0)&&
            !CanAppendCounts(0,std::numeric_limits<std::size_t>::max()),"size_t overflow rejected");
    Mesh3D success=Mesh3D::CreateUnitBox();const auto cached=std::make_shared<MeshRaycastIndex3D>();
    success.raycastCache->index.store(cached);Require(Append(success,valid),"valid commit");
    Require(!success.raycastCache->index.load(),"success invalidates stale raycast index");
}
}
int main() try {
    Negative();
    constexpr std::array<Vec3,6> rotations{{{0,0,0},{.3f,-.7f,.2f},{1.1f,.6f,-.8f},
        {0,1.5707963f,0},{-1.2f,2.1f,.9f},{.7f,.9f,-2.3f}}};
    constexpr std::array<Vec3,4> sizes{{{1,1,1},{.8f,.06f,.016f},{2.3f,.17f,.4f},{.023f,.031f,.019f}}};
    for(const auto rotation:rotations)for(const auto size:sizes)for(const float ratio:{.1f,.45f,.9f}) {
        Box box;box.center={-2.7f,1.3f,4.1f};box.size=size;
        box.bevel=(std::min)({size.x,size.y,size.z})*.5f*ratio;
        const auto matrix=CreateRotationMatrix(rotation);
        box.a={matrix[0][0],matrix[1][0],matrix[2][0]};
        box.b={matrix[0][1],matrix[1][1],matrix[2][1]};
        box.c={matrix[0][2],matrix[1][2],matrix[2][2]};
        Mesh3D mesh;Require(Append(mesh,box),"rotated multi-size box accepted");Geometry(mesh,box);
        Mesh3D repeat;Require(Append(repeat,box)&&RawEqual(mesh,repeat),"deterministic raw generation");
    }
    std::cout<<"BEVELED_BOX_CPU_PASS geometry_cases="<<cases
        <<" transactional_negative_cases="<<negatives<<" checks="<<checks<<" GPU_or_art_pass=0\n";
    return 0;
} catch(const std::exception& error){std::cerr<<"BEVELED_BOX_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}
