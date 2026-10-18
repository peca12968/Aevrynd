#include "MistwatchLandscapeSpecies.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

// 合成網格 CPU 契約：不宣稱完整世界、實際資產碰撞、效能或 GPU 已驗收。
// Cells 既有超出 int 範圍的有限查詢未涵蓋；此測試不更改其政策。
namespace {
using namespace Aevrynd;
namespace D=Mistwatch::LandscapeSpeciesDetail;
using R=D::Exact::Rational;
unsigned checks{},negatives{},queries{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Result {R height;unsigned kind{};std::string error;};
Result Observe(const D::Surface& surface,Vec3 point,bool bounds){
    try{return {surface.At(point,bounds),0,{}};}
    catch(const std::invalid_argument& e){return {{},1,e.what()};}
    catch(const std::runtime_error& e){return {{},2,e.what()};}
}
Result Same(const D::Surface& surface,Vec3 point){
    const auto original=Observe(surface,point,false),bounded=Observe(surface,point,true);++queries;
    Need(original.kind==bounded.kind&&original.error==bounded.error,"point sampler exact error changed");
    if(original.kind==0){
        Need(Compare(original.height,bounded.height)==0,"point sampler exact rational height changed");
        Need(Compare(surface.At(point),bounded.height)==0,"default entry differs from explicit bounded entry");
    }else ++negatives;
    return bounded;
}
void Reject(const D::Surface& surface,Vec3 point,unsigned kind,std::string_view error){
    const auto result=Same(surface,point);
    Need(result.kind==kind&&result.error==error,"query did not preserve expected exact rejection");
}
Mesh3D Grid(float sx,float sz,float offset=.25f){
    Mesh3D mesh;
    for(unsigned z=0;z<=18;++z)for(unsigned x=0;x<=18;++x){
        Vertex3D vertex{};vertex.x=-9.f+float(x);vertex.z=-9.f+float(z);
        vertex.y=sx*vertex.x+sz*vertex.z+offset;
        vertex.normal=PackNormalSnorm8(Normalize({-sx,1,-sz}));vertex.color=0xffffffffu;
        mesh.vertices.push_back(vertex);
    }
    for(unsigned z=0;z<18;++z)for(unsigned x=0;x<18;++x){const unsigned a=z*19+x;
        mesh.indices.insert(mesh.indices.end(),{a,a+19,a+1,a+1,a+19,a+20});}
    return mesh;
}
void Plane(const D::Surface& surface,Vec3 query,float sx,float sz,float offset=.25f){
    const auto result=Same(surface,query);
    if(result.kind)std::cerr<<"PLANE_DIVERGENCE query="<<query.x<<','<<query.y<<','<<query.z
        <<" slopes="<<sx<<','<<sz<<" offset="<<offset<<" error="<<result.error<<'\n';
    Need(result.kind==0,"plane fixture query unexpectedly rejected");
    // 三個坡面與作者座標使用二進位可精確表示數值；預期直接由平面方程生成。
    const auto expected=R::Float(sx)*R::Float(query.x)+R::Float(sz)*R::Float(query.z)+R::Float(offset);
    Need(Compare(result.height,expected)==0,"height differs from independent exact plane equation");
}
void GridCases(){
    const float inf=(std::numeric_limits<float>::infinity)();
    for(const Vec3 slope:{Vec3{0,0,0},Vec3{.125f,0,0},Vec3{-.0625f,0,.03125f}}){
        auto mesh=Grid(slope.x,slope.z);const D::Surface surface(mesh);
        for(unsigned z=0;z<=36;++z)for(unsigned x=0;x<=36;++x)
            Plane(surface,{-9.f+.5f*float(x),0,-9.f+.5f*float(z)},slope.x,slope.z);
        // 三角共邊／角、網格外緣與正負 8m cell 界，向兩側各取相鄰 float ULP。
        for(float boundary:{-9.f,-8.f,-1.f,0.f,1.f,8.f,9.f})for(float cross:{-8.f,-.5f,0.f,.5f,8.f}){
            for(float adjacent:{std::nextafter(boundary,-inf),boundary,std::nextafter(boundary,inf)}){
                for(Vec3 query:{Vec3{adjacent,0,cross},Vec3{cross,0,adjacent}}){
                    if(query.x>=-9.f&&query.x<=9.f&&query.z>=-9.f&&query.z<=9.f)
                        Plane(surface,query,slope.x,slope.z);
                    else Reject(surface,query,1,"LANDSCAPE_SPECIES_ROOT_SURFACE_MISSING");
                }
            }
        }
        for(float x:{-8.f,0.f,8.f})for(float z:{-8.f,0.f,8.f})
            for(float dx:{std::nextafter(x,-inf),x,std::nextafter(x,inf)})
                for(float dz:{std::nextafter(z,-inf),z,std::nextafter(z,inf)})
                    Plane(surface,{dx,17.f,dz},slope.x,slope.z);
        for(Vec3 query:{Vec3{1.e9f,0,0},Vec3{-1.e9f,0,0},Vec3{0,0,1.e9f},Vec3{0,0,-1.e9f}})
            Reject(surface,query,1,"LANDSCAPE_SPECIES_ROOT_SURFACE_MISSING");
        // 高度不在 XZ 點判定內，但仍由原 exact 入口驗證有限性。
        for(float y:{-1.e9f,1.e9f})Plane(surface,{.25f,y,-.25f},slope.x,slope.z);
        const auto before=surface.At({.25f,0,-.25f});
        for(auto& vertex:mesh.vertices)vertex.y+=1.f;
        Plane(surface,{.25f,0,-.25f},slope.x,slope.z,1.25f);
        Need(Compare(surface.At({.25f,0,-.25f}),before+R(1))==0,"mutable source height was cached stale");
        const unsigned centre=9*19+9;mesh.vertices[centre].y+=.125f;
        const auto vertexHeight=Same(surface,{0,0,0});
        Need(vertexHeight.kind==0&&Compare(vertexHeight.height,R::Float(mesh.vertices[centre].y))==0,
             "mutable single vertex height did not reach exact sampler");
    }
}
void MissingAndOverlap(){
    auto hole=Grid(0,0);hole.indices.erase(hole.indices.begin(),hole.indices.begin()+6);
    const D::Surface missing(hole);
    Reject(missing,{-8.75f,0,-8.75f},1,"LANDSCAPE_SPECIES_ROOT_SURFACE_MISSING");
    Plane(missing,{-8.f,0,-8.f},0,0);
    const float tolerance=.00005f;
    for(float delta:{0.f,.000025f,std::nextafter(tolerance,0.f),tolerance,
                     std::nextafter(tolerance,1.f),.001f}){
        auto mesh=Grid(0,0,0);const unsigned start=static_cast<unsigned>(mesh.vertices.size());
        for(unsigned i:{0u,19u,1u}){auto vertex=mesh.vertices[i];vertex.y=delta;mesh.vertices.push_back(vertex);}
        mesh.indices.insert(mesh.indices.end(),{start,start+1,start+2});const D::Surface surface(mesh);
        if(delta<=tolerance)Plane(surface,{-8.75f,0,-8.75f},0,0,0);
        else Reject(surface,{-8.75f,0,-8.75f},1,"LANDSCAPE_SPECIES_AMBIGUOUS_SURFACE");
    }
}
void MutableBounds(){
    Mesh3D mesh;
    for(Vec3 p:{Vec3{0,0,0},Vec3{0,0,2},Vec3{2,0,0}}){
        Vertex3D vertex{};vertex.x=p.x;vertex.y=p.y;vertex.z=p.z;
        vertex.normal=PackNormalSnorm8({0,1,0});vertex.color=0xffffffffu;mesh.vertices.push_back(vertex);
    }
    mesh.indices={0,1,2};const D::Surface surface(mesh);
    Plane(surface,{.25f,0,.25f},0,0,0);
    // 變動仍在原索引涵蓋的 cell；不假設原有引用式 Surface 會重新建立 cell 索引。
    for(auto& vertex:mesh.vertices){vertex.x+=.5f;vertex.z+=.5f;}
    Reject(surface,{.25f,0,.25f},1,"LANDSCAPE_SPECIES_ROOT_SURFACE_MISSING");
    Plane(surface,{.75f,0,.75f},0,0,0);
    Plane(surface,{2.25f,0,.75f},0,0,0);
    for(auto& vertex:mesh.vertices){vertex.x=(vertex.x-.5f)*1.5f;vertex.z=(vertex.z-.5f)*1.5f;vertex.y=2.f;}
    Plane(surface,{2.25f,0,.25f},0,0,2);
}
float& Axis(Vec3& value,unsigned axis){if(axis==0)return value.x;if(axis==1)return value.y;return value.z;}
void InvalidAndHelper(){
    auto mesh=Grid(0,0);const D::Surface surface(mesh);
    const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
    for(unsigned axis=0;axis<3;++axis)for(float invalid:{nan,inf,-inf}){
        Vec3 query{};Axis(query,axis)=invalid;
        Reject(surface,query,2,"nonfinite exact input");
    }
    const Vec3 a{0,0,0},b{0,0,2},c{2,0,0};
    for(Vec3 query:{a,b,c,Vec3{0,0,1},Vec3{1,0,0},Vec3{1,0,1}})
        Need(TrianglePointMayOverlapXZ3D(query,a,b,c),"strict broad phase discarded edge or corner");
    Need(TrianglePointMayOverlapXZ3D({1.75f,0,1.75f},a,b,c),"broad phase incorrectly proves triangle containment");
    for(Vec3 query:{Vec3{std::nextafter(0.f,-inf),0,1},Vec3{std::nextafter(2.f,inf),0,1},
                    Vec3{1,0,std::nextafter(0.f,-inf)},Vec3{1,0,std::nextafter(2.f,inf)}})
        Need(!TrianglePointMayOverlapXZ3D(query,a,b,c),"strict broad phase retained separated adjacent ULP");
    for(unsigned input=0;input<4;++input)for(unsigned axis=0;axis<3;++axis)for(float invalid:{nan,inf,-inf}){
        std::array<Vec3,4> values{Vec3{100,0,100},a,b,c};Axis(values[input],axis)=invalid;
        Need(TrianglePointMayOverlapXZ3D(values[0],values[1],values[2],values[3]),"invalid input pruned before exact rejection");
    }
    const float maximum=(std::numeric_limits<float>::max)();
    Need(!TrianglePointMayOverlapXZ3D({maximum,0,maximum},a,b,c),"finite huge separated helper query retained");
    Need(TrianglePointMayOverlapXZ3D({1,maximum,1},a,b,c),"helper used Y as height predicate");
    Need(TrianglePointMayOverlapXZ3D({maximum,0,maximum},{-maximum,0,-maximum},
         {maximum,0,-maximum},{maximum,0,maximum}),"helper overflowed finite extreme bounds");
    // 來源於建構後變成非有限：helper 仍讓原 exact 轉換拒絕，不回傳舊高度。
    auto mutableMesh=Grid(0,0);const D::Surface mutableSurface(mutableMesh);
    mutableMesh.vertices[0].y=nan;
    Reject(mutableSurface,{-8.75f,0,-8.75f},2,"nonfinite exact input");
}
}
int main(){try{
    GridCases();MissingAndOverlap();MutableBounds();InvalidAndHelper();
    std::cout<<"GROUND_POINT_SAMPLER_EXACT_EQUIVALENCE_OK; checks="<<checks<<"; negatives="<<negatives
        <<"; queries="<<queries<<"; synthetic_slopes=3; world_verified=0; GPU_verified=0; performance_verified=0; out_of_int_cells_verified=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"GROUND_POINT_SAMPLER_TEST_FAILED: "<<e.what()<<'\n';return 1;}}
