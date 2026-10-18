#include <Aevrynd/CappedCylinder3D.h>
#include <Aevrynd/GeometryNormals3D.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using namespace Aevrynd;
std::uint64_t checks=0;
void Need(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(const Vertex3D& a,const Vertex3D& b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&
    a.normal==b.normal&&a.color==b.color&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
struct D3 {double x,y,z;};
D3 Point(const Vertex3D& v){return{v.x,v.y,v.z};}
D3 Sub(D3 a,D3 b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
D3 CrossD(D3 a,D3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double DotD(D3 a,D3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
std::array<std::uint32_t,3> PositionKey(const Vertex3D& v){
    const auto key=[](float value){return value==0?0u:std::bit_cast<std::uint32_t>(value);};
    return{key(v.x),key(v.y),key(v.z)};
}

// 品質門獨立於舊primitive／V887 migration comparison；正面由真XYZ判定。
void GeometryGate(const Mesh3D& mesh,unsigned n){
    Need(n>=3&&n<=128,"bounded accepted side domain");
    Need(mesh.Valid()&&mesh.vertices.size()==std::size_t(4)*n+2&&
         mesh.indices.size()==std::size_t(12)*n,"exact source count formula");
    std::map<std::array<std::uint32_t,3>,unsigned> welded;
    std::vector<unsigned> logical(mesh.vertices.size());std::vector<bool> used(mesh.vertices.size());
    std::array<float,3> low{.5f,.5f,.5f},high{-.5f,-.5f,-.5f};
    constexpr double error=.5/127.;
    const double unitBound=2*std::sqrt(3.)*error+3*error*error;
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];
        Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
             std::isfinite(v.u)&&std::isfinite(v.v),"all actual float fields finite");
        Need(std::abs(v.x)<=.5f&&std::abs(v.y)<=.5f&&std::abs(v.z)<=.5f,"canonical exact coordinate bounds");
        Need(v.color==0xffffffffu&&Bits(v.u,0.f)&&Bits(v.v,0.f),"canonical white and positive-zero UV");
        for(unsigned byte=0;byte<4;++byte)Need(std::uint8_t(v.normal>>(8*byte))!=128,"no SNORM -128");
        const auto packed=UnpackNormalSnorm8(v.normal);
        Need(packed.w==1&&std::abs(double(packed.x)*packed.x+double(packed.y)*packed.y+
             double(packed.z)*packed.z-1)<=unitBound,"lit unit normals within derived packing bound");
        if(i<2*n){
            const unsigned side=static_cast<unsigned>(i/2);
            constexpr float pi=3.14159265358979323846f;
            const float angle=2.f*pi*float(side)/float(n);
            const float x=std::sin(angle)*.5f,z=std::cos(angle)*.5f;
            Need(Bits(v.x,x)&&Bits(v.z,z)&&Bits(v.y,i%2==0?-.5f:.5f),"fixed f32 phase, side ordering and canonical radius formula");
            Need(v.normal==PackNormalSnorm8({2*v.x,0,2*v.z}),"independent exact radial packing");
        }else if(i<3*n){
            const auto& source=mesh.vertices[2*(i-2*n)];
            Need(Bits(v.x,source.x)&&Bits(v.y,source.y)&&Bits(v.z,source.z)&&
                 v.normal==PackNormalSnorm8({0,-1,0}),"bottom rim raw XYZ alias and independent -Y normal");
        }else if(i==3*n){
            Need(Bits(v.x,0.f)&&Bits(v.y,-.5f)&&Bits(v.z,0.f)&&
                 v.normal==PackNormalSnorm8({0,-1,0}),"bottom source center");
        }else if(i<4*n+1){
            const auto& source=mesh.vertices[2*(i-(3*n+1))+1];
            Need(Bits(v.x,source.x)&&Bits(v.y,source.y)&&Bits(v.z,source.z)&&
                 v.normal==PackNormalSnorm8({0,1,0}),"top rim raw XYZ alias and independent +Y normal");
        }else Need(Bits(v.x,0.f)&&Bits(v.y,.5f)&&Bits(v.z,0.f)&&
                  v.normal==PackNormalSnorm8({0,1,0}),"top source center");
        const auto id=static_cast<unsigned>(welded.size());
        logical[i]=welded.emplace(PositionKey(v),id).first->second;
        const std::array<float,3> position{v.x,v.y,v.z};
        for(unsigned axis=0;axis<3;++axis){low[axis]=(std::min)(low[axis],position[axis]);high[axis]=(std::max)(high[axis],position[axis]);}
    }
    std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,int>> edges;
    std::set<std::array<unsigned,3>> faces;
    std::vector<std::vector<unsigned>> graph(welded.size());double volume6=0,volumeMagnitude=0;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        std::array<unsigned,3> ids{},keys{};
        for(unsigned k=0;k<3;++k){ids[k]=mesh.indices[i+k];Need(ids[k]<mesh.vertices.size(),"every index in bounds");
            used[ids[k]]=true;keys[k]=logical[ids[k]];}
        auto sorted=keys;std::sort(sorted.begin(),sorted.end());
        Need(sorted[0]!=sorted[1]&&sorted[1]!=sorted[2]&&faces.insert(sorted).second,"distinct logical vertices and no duplicate face");
        const auto& a=mesh.vertices[ids[0]];const auto& b=mesh.vertices[ids[1]];const auto& c=mesh.vertices[ids[2]];
        const auto normal=CrossD(Sub(Point(b),Point(a)),Sub(Point(c),Point(a)));
        const double area2=DotD(normal,normal);
        Need(std::isfinite(area2)&&area2>0,"finite nondegenerate actual triangle");
        const double outward=DotD(normal,Point(a));Need(outward>0,"true outward relative to convex origin, no observed flip");
        volume6+=outward;
        const D3 uMagnitude{std::abs(double(b.x))+std::abs(double(a.x)),std::abs(double(b.y))+std::abs(double(a.y)),std::abs(double(b.z))+std::abs(double(a.z))};
        const D3 vMagnitude{std::abs(double(c.x))+std::abs(double(a.x)),std::abs(double(c.y))+std::abs(double(a.y)),std::abs(double(c.z))+std::abs(double(a.z))};
        volumeMagnitude+=std::abs(double(a.x))*(uMagnitude.y*vMagnitude.z+uMagnitude.z*vMagnitude.y)+
            std::abs(double(a.y))*(uMagnitude.z*vMagnitude.x+uMagnitude.x*vMagnitude.z)+
            std::abs(double(a.z))*(uMagnitude.x*vMagnitude.y+uMagnitude.y*vMagnitude.x);
        Vec3 shared{};Need(TryComputeTriangleNormal({a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z},shared),"existing true-normal API accepts actual face");
        for(const auto id:ids){const auto packed=UnpackNormalSnorm8(mesh.vertices[id].normal);
            Need(DotD(normal,{packed.x,packed.y,packed.z})>0&&
                 double(shared.x)*packed.x+double(shared.y)*packed.y+double(shared.z)*packed.z>0,"all packed corners have true positive orientation");}
        const unsigned faceType=static_cast<unsigned>((i/3)%4);
        if(faceType>=2){const float capY=faceType==2?.5f:-.5f;
            Need(a.y==capY&&b.y==capY&&c.y==capY&&normal.x==0&&normal.z==0&&
                 (faceType==2?normal.y>0:normal.y<0),"true planar caps signed +/-Y");}
        else Need(normal.y==0,"true vertical side plane");
        for(unsigned k=0;k<3;++k){const auto x=keys[k],y=keys[(k+1)%3];
            auto& edge=edges[{(std::min)(x,y),(std::max)(x,y)}];++edge.first;edge.second+=x<y?1:-1;
            graph[x].push_back(y);graph[y].push_back(x);}
    }
    Need(std::all_of(used.begin(),used.end(),[](bool active){return active;}),"all records referenced");
    Need(welded.size()==std::size_t(2)*n+2,"all cap rims are true geometric aliases");
    for(const auto& entry:edges)Need(entry.second.first==2&&entry.second.second==0,"every exact welded edge has two opposite uses");
    Need(static_cast<long long>(welded.size())-static_cast<long long>(edges.size())+
         static_cast<long long>(faces.size())==2,"sphere Euler characteristic");
    std::vector<bool> reached(graph.size());std::vector<unsigned> stack{0};reached[0]=true;
    while(!stack.empty()){const auto at=stack.back();stack.pop_back();for(const auto next:graph[at])if(!reached[next]){reached[next]=true;stack.push_back(next);}}
    Need(std::all_of(reached.begin(),reached.end(),[](bool value){return value;}),"single connected closed component");
    double polygon2=0,polygonMagnitude=0;
    for(unsigned side=0;side<n;++side){const auto& a=mesh.vertices[2*side];const auto& b=mesh.vertices[2*((side+1)%n)];
        polygon2+=double(a.z)*b.x-double(a.x)*b.z;
        polygonMagnitude+=std::abs(double(a.z)*b.x)+std::abs(double(a.x)*b.z);}
    const double expectedVolume6=3*polygon2;
    // 同一f32多邊形的棱柱體積；誤差界由實際double dot/累加項數推導，不是幾何epsilon。
    const double operations=double(mesh.indices.size())*16;
    const double gamma=operations*std::numeric_limits<double>::epsilon()/(1-operations*std::numeric_limits<double>::epsilon());
    Need(std::isfinite(volume6)&&volume6>0&&expectedVolume6>0&&
         std::abs(volume6-expectedVolume6)<=gamma*(volumeMagnitude+3*polygonMagnitude),"positive actual polygon prism volume within arithmetic bound");
    const auto bounds=mesh.Bounds();
    const std::array<float,3> center{bounds.center.x,bounds.center.y,bounds.center.z},extent{bounds.extents.x,bounds.extents.y,bounds.extents.z};
    for(unsigned axis=0;axis<3;++axis)Need(Bits(center[axis],(low[axis]+high[axis])*.5f)&&
        Bits(extent[axis],(high[axis]-low[axis])*.5f),"public bounds match actual f32 extrema, including odd-sided asymmetry");
    Need(low[1]==-.5f&&high[1]==.5f&&std::isfinite(bounds.radius)&&bounds.radius>0,"canonical height and finite bounds radius");
}

std::uint32_t SwizzleToX(std::uint32_t normal){
    const auto z=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(normal>>16));
    Need(z!=-128,"signed-byte swizzle excludes noncanonical -128");
    const auto minusZ=std::bit_cast<std::uint8_t>(static_cast<std::int8_t>(-int(z)));
    return ((normal>>8)&255u)|((normal&255u)<<8)|(std::uint32_t(minusZ)<<16)|(normal&0xff000000u);
}
struct MigrationFixture {Vec3 center,size;Color color;};
Mesh3D DirectRoute(const Mesh3D& canonical,const MigrationFixture& fixture){
    auto result=canonical;
    for(auto& v:result.vertices){const Vec3 point{fixture.center.x+v.y*fixture.size.x,
        fixture.center.y+v.x*fixture.size.y,fixture.center.z-v.z*fixture.size.z};
        v.x=point.x;v.y=point.y;v.z=point.z;v.normal=SwizzleToX(v.normal);
        v.color=PackColorRgba8(fixture.color);v.u=0;v.v=0;}
    result.InvalidateRaycastIndex();return result;
}
// 僅migration regression：與V887相同的源運算順序／分rim／index映射，非品質golden。
Mesh3D LegacyRoute(const MigrationFixture& fixture){
    constexpr unsigned n=12;const auto source=Mesh3D::CreateUnitCylinder(n);Mesh3D result;
    const auto emit=[&](unsigned index,Vec3 normal){auto v=source.vertices[index];
        const Vec3 point{fixture.center.x+v.y*fixture.size.x,fixture.center.y+v.x*fixture.size.y,fixture.center.z-v.z*fixture.size.z};
        v.x=point.x;v.y=point.y;v.z=point.z;v.normal=PackNormalSnorm8(normal);
        v.color=PackColorRgba8(fixture.color);v.u=0;v.v=0;result.vertices.push_back(v);};
    for(unsigned i=0;i<2*n;++i){const auto& v=source.vertices[i];emit(i,{0,2*v.x,-2*v.z});}
    for(unsigned i=0;i<n;++i)emit(2*i,{-1,0,0});emit(2*n,{-1,0,0});
    for(unsigned i=0;i<n;++i)emit(2*i+1,{1,0,0});emit(2*n+1,{1,0,0});
    const auto mapped=[](unsigned index,unsigned triangle){
        if(triangle%4==2)return index==2*n+1?4*n+1:3*n+1+index/2;
        if(triangle%4==3)return index==2*n?3*n:2*n+index/2;return index;};
    for(unsigned t=0;t<4*n;++t)result.indices.insert(result.indices.end(),{
        mapped(source.indices[3*t],t),mapped(source.indices[3*t+2],t),mapped(source.indices[3*t+1],t)});
    return result;
}
template<class Mutation>void Negative(const Mesh3D& source,Mutation change,const char* name){
    auto bad=source;change(bad);bool rejected=false;
    try{GeometryGate(bad,12);}catch(const std::exception&){rejected=true;}
    Need(rejected,name);
}
}

int main(){try{
    for(const auto n:std::array<unsigned,5>{3,4,12,32,128}){
        const auto mesh=CreateUnitCappedCylinder3D(n);GeometryGate(mesh,n);
        const auto repeat=CreateUnitCappedCylinder3D(n);Need(mesh.indices==repeat.indices,"deterministic indices");
        for(std::size_t i=0;i<mesh.vertices.size();++i)Need(Same(mesh.vertices[i],repeat.vertices[i]),"every deterministic vertex bit");
    }
    for(const auto invalid:std::array<unsigned,6>{0,1,2,129,256,(std::numeric_limits<unsigned>::max)()}){
        bool rejected=false;try{const auto mesh=CreateUnitCappedCylinder3D(invalid);(void)mesh;}
        catch(const std::invalid_argument&){rejected=true;}Need(rejected,"invalid sides rejected, never clamped");
    }
    const auto mesh=CreateUnitCappedCylinder3D(12);
    const auto defaults=CreateUnitCappedCylinder3D();
    Need(mesh.indices==defaults.indices&&mesh.vertices.size()==defaults.vertices.size(),"default is canonical twelve sides");
    for(std::size_t i=0;i<mesh.vertices.size();++i)Need(Same(mesh.vertices[i],defaults.vertices[i]),"default all fields bit exact");
    Negative(mesh,[](auto& m){m.indices[0]=static_cast<std::uint32_t>(m.vertices.size());},"index overflow negative");
    Negative(mesh,[](auto& m){m.indices[1]=m.indices[0];},"degenerate negative");
    Negative(mesh,[](auto& m){std::swap(m.indices[0],m.indices[1]);},"inward face negative");
    Negative(mesh,[](auto& m){for(unsigned k=0;k<3;++k)m.indices[3+k]=m.indices[k];},"duplicated/open surface negative");
    Negative(mesh,[](auto& m){m.vertices[24].normal=m.vertices[0].normal;},"cap radial normal negative");
    Negative(mesh,[](auto& m){m.indices[6]=1;},"shared side normal on cap negative");
    Negative(mesh,[](auto& m){m.vertices[0].x=std::numeric_limits<float>::quiet_NaN();},"nonfinite negative");
    Negative(mesh,[](auto& m){m.vertices[0].z=.501f;},"canonical envelope negative");
    Negative(mesh,[](auto& m){m.vertices[0].normal&=0xffffffu;},"packed lighting bypass negative");
    Negative(mesh,[](auto& m){m.vertices[0].normal=(m.vertices[0].normal&0xffffff00u)|128u;},"SNORM -128 negative");
    Negative(mesh,[](auto& m){m.vertices[0].u=1;},"raw zero UV negative");
    Negative(mesh,[](auto& m){m.vertices[0].color=0;},"raw white color negative");
    const std::array<MigrationFixture,2> fixtures{{
        {{.0455f,-.006f,.087f},{.269f,.012f,.012f},{.66f,.69f,.71f,1}},
        {{-.162f,-.006f,.087f},{.142f,.046f,.046f},{.86f,.32f,.065f,1}}
    }};
    for(const auto& fixture:fixtures){const auto before=LegacyRoute(fixture),after=DirectRoute(mesh,fixture);
        Need(before.vertices.size()==50&&after.vertices.size()==50&&before.indices==after.indices&&
             after.indices.size()==144,"V887 two cylinder migration exact count/index regression");
        for(std::size_t i=0;i<before.vertices.size();++i)Need(Same(before.vertices[i],after.vertices[i]),"V887 all XYZ/normal/color/UV bits equal after API migration");}
    std::cout<<"CAPPED_CYLINDER_CPU_PASS checks="<<checks
        <<" sides=3,4,12,32,128 invalid_sides=6 geometry_negatives=12 migration_cylinders=2 native=false art=false\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
