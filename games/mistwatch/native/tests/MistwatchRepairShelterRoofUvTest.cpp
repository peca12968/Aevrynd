#include "MistwatchRepairShelterRoofUv.h"
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>

// 原Test含自己的main巨集；全體標準／來源headers先在global載入，才原文納入隔離namespace。
// 不改原Test任何bytes；它的main在此是普通namespace函式，完整原正／負門先執行。
#ifdef main
#error V954_requires_unmodified_main_macro
#endif
namespace V954Baseline {
#include "MistwatchRepairShelterBoardUvTest.cpp"
}
int OriginalBoardUvTestMain(){return V954Baseline::main();}

namespace V954RoofSmoke {
using namespace Aevrynd;
unsigned checks{},negatives{};
constexpr std::size_t RoofVertices=20u*96u,RoofIndices=20u*132u;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool GeometrySame(const Vertex3D& a,const Vertex3D& b){
    return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color;
}
bool VertexSame(const Vertex3D& a,const Vertex3D& b){return GeometrySame(a,b)&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
double Jacobian(const Mesh3D& mesh,std::size_t first){
    const auto& a=mesh.vertices[mesh.indices[first]];const auto& b=mesh.vertices[mesh.indices[first+1]];
    const auto& c=mesh.vertices[mesh.indices[first+2]];
    return (double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);
}
bool SameJacobianSigns(const Mesh3D& mesh,const Mesh3D& baseline){
    if(!mesh.Valid()||!baseline.Valid()||mesh.indices!=baseline.indices)return false;
    for(std::size_t first=0;first<mesh.indices.size();first+=3){
        const double a=Jacobian(baseline,first),b=Jacobian(mesh,first);
        if(!std::isfinite(a)||!std::isfinite(b)||a==0||b==0||std::signbit(a)!=std::signbit(b))return false;
    }
    return true;
}
bool Validate(const Mesh3D& mesh,const Mesh3D& baseline){
    if(!mesh.Valid()||!baseline.Valid()||mesh.vertices.size()!=7392||baseline.vertices.size()!=7392||
       mesh.indices.size()!=10164||mesh.indices!=baseline.indices)return false;
    for(std::size_t i=0;i<mesh.vertices.size();++i){
        const auto& p=mesh.vertices[i];
        if(!GeometrySame(p,baseline.vertices[i])||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
           !std::isfinite(p.u)||!std::isfinite(p.v))return false;
        if(i>=RoofVertices&&!VertexSame(p,baseline.vertices[i]))return false;
    }
    // 從原96筆XYZ自行求bounds與expected，不呼叫候選的UV／bounds／Jacobian函式。
    for(unsigned board=0;board<20;++board){
        const auto first=std::size_t(board)*96u;
        Vec3 lo{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()};
        Vec3 hi=lo*-1.f;
        for(std::size_t i=first;i<first+96u;++i){const auto& p=baseline.vertices[i];
            lo.x=(std::min)(lo.x,p.x);hi.x=(std::max)(hi.x,p.x);lo.y=(std::min)(lo.y,p.y);hi.y=(std::max)(hi.y,p.y);
            lo.z=(std::min)(lo.z,p.z);hi.z=(std::max)(hi.z,p.z);}
        if(!(hi.x>lo.x&&hi.y>lo.y&&hi.z>lo.z))return false;
        const float phase=.1f+.025f*float(board%5u),scale=2048.f*(16.f/32767.f);
        for(std::size_t i=first;i<first+96u;++i){const auto& source=baseline.vertices[i];const auto& p=mesh.vertices[i];
            const auto n=UnpackNormalSnorm8(source.normal);const float nx=std::abs(n.x),ny=std::abs(n.y),nz=std::abs(n.z);
            float transverse{},along{},extent{};
            if(ny>=nx&&ny>=nz){transverse=(source.x-lo.x)/(hi.x-lo.x);along=source.z-lo.z;extent=hi.z-lo.z;}
            else if(nx>=nz){transverse=(source.y-lo.y)/(hi.y-lo.y);along=source.z-lo.z;extent=hi.z-lo.z;}
            else{transverse=(source.y-lo.y)/(hi.y-lo.y);along=source.x-lo.x;extent=hi.x-lo.x;}
            if(!std::isfinite(transverse)||transverse<0.f||transverse>1.f||along<0.f||along>extent)return false;
            const float expectedU=(32.f/1024.f+(48.f/1024.f)*transverse)/scale;
            const float expectedV=(phase+along*.5f)/scale;
            if(!Bits(p.u,expectedU)||!Bits(p.v,expectedV)||p.u<(32.f/1024.f)/scale||p.u>(80.f/1024.f)/scale||
               p.v<phase/scale||p.v>(phase+extent*.5f)/scale)return false;
        }
    }
    if(!SameJacobianSigns(mesh,baseline))return false;
    for(std::size_t first=0;first<mesh.indices.size();first+=3){
        const auto& a=mesh.vertices[mesh.indices[first]];const auto& b=mesh.vertices[mesh.indices[first+1]];
        const auto& c=mesh.vertices[mesh.indices[first+2]];
        Vec3 normal{};if(!TryComputeTriangleNormal({a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z},normal))return false;
        const auto packed=UnpackNormalSnorm8(a.normal);if(Dot(normal,{packed.x,packed.y,packed.z})<=.99f)return false;
    }
    return true;
}
template<class Change>void Reject(const Mesh3D& candidate,const Mesh3D& baseline,Change change,const char* message){
    auto broken=candidate;change(broken);++negatives;Check(!Validate(broken,baseline),message);
}
}

int main(){try{
    using namespace V954RoofSmoke;
    Check(OriginalBoardUvTestMain()==0,"complete unchanged original BoardUv + Visual tests first");
    const auto collisionBefore=Mistwatch::RepairShelterSolids();
    const auto baseline=Mistwatch::ShelterBoardUv::CreatePlankedRepairShelterBoardUv();
    const auto a=Mistwatch::ShelterRoofUv::CreatePlankedRepairShelterRoofUv();
    const auto b=Mistwatch::ShelterRoofUv::CreatePlankedRepairShelterRoofUv();
    Check(Validate(a,baseline),"source-derived roof UV, all raw ownership, signed Jacobians and normals");
    Check(a.indices==b.indices,"repeat exact indices");
    unsigned changed{},protectedVertices{},ownedVertices{};
    for(std::size_t i=0;i<a.vertices.size();++i){
        Check(VertexSame(a.vertices[i],b.vertices[i]),"repeat all vertex bits");
        Check(GeometrySame(a.vertices[i],baseline.vertices[i]),"all XYZ normal color raw");
        if(i<RoofVertices){++ownedVertices;changed+=!Bits(a.vertices[i].u,baseline.vertices[i].u)||!Bits(a.vertices[i].v,baseline.vertices[i].v);}
        else{++protectedVertices;Check(VertexSame(a.vertices[i],baseline.vertices[i]),"all5472 nonroof vertex fields raw");}
    }
    Check(ownedVertices==1920&&protectedVertices==5472&&changed>0,"fixed20 roof UV ownership only");
    Check(RoofIndices/3u==880&&a.indices.size()/3u==3388,"original face counts");
    const auto collisionAfter=Mistwatch::RepairShelterSolids();
    Check(collisionBefore.size()==collisionAfter.size(),"collision membership raw");
    for(std::size_t i=0;i<collisionBefore.size();++i){const auto& x=collisionBefore[i];const auto& y=collisionAfter[i];
        Check(Bits(x.center.x,y.center.x)&&Bits(x.center.y,y.center.y)&&Bits(x.center.z,y.center.z)&&
              Bits(x.extents.x,y.extents.x)&&Bits(x.extents.y,y.extents.y)&&Bits(x.extents.z,y.extents.z),"all5 original collider bits raw");}
    ++negatives;Check(!Validate(baseline,baseline),"unchanged broad-atlas roof must fail new roof contract");
    Reject(a,baseline,[](Mesh3D& m){for(std::size_t i=0;i<96u;++i)std::swap(m.vertices[i].u,m.vertices[i].v);},"wrong90deg grain");
    Reject(a,baseline,[](Mesh3D& m){for(std::size_t i=0;i<96u;++i)m.vertices[i].u=m.vertices[i].v=0.f;},"zero UV");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].u=0.f;},"outside source strip");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].v+=.025f;},"wrong board phase");
    Reject(a,baseline,[](Mesh3D& m){for(std::size_t i=0;i<96u;++i)m.vertices[i].v*=2.f;},"wrong longitudinal density");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].u*=Mistwatch::ShelterRoofUv::StaticDecodedScale;},"missing static compensation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].v=std::numeric_limits<float>::quiet_NaN();},"nonfinite UV");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].x=std::nextafter(m.vertices[0].x,std::numeric_limits<float>::infinity());},"roof position mutation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].normal^=1u;},"packed normal mutation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[0].color^=1u;},"packed color mutation");
    Reject(a,baseline,[](Mesh3D& m){std::swap(m.indices[0],m.indices[1]);},"index winding mutation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices.pop_back();},"membership mutation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[20u*96u].u+=.125f;},"protected wall UV mutation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[76u*96u].v+=.125f;},"protected equipment UV mutation");
    Reject(a,baseline,[](Mesh3D& m){m.vertices[20u*96u].z=std::nextafter(m.vertices[20u*96u].z,std::numeric_limits<float>::infinity());},"protected wall geometry mutation");
    auto reflected=a;for(std::size_t i=0;i<4u;++i)reflected.vertices[i].u=-reflected.vertices[i].u;
    ++negatives;Check(!SameJacobianSigns(reflected,baseline)&&!Validate(reflected,baseline),"explicit signed Jacobian reversal");
    std::cout<<"SHELTER_ROOF_UV_CPU_PASS checks="<<checks<<" negatives="<<negatives
             <<" owned_uv_vertices="<<ownedVertices<<" protected_vertices="<<protectedVertices<<" changed_uv_vertices="<<changed
             <<" vertices="<<a.vertices.size()<<" indices="<<a.indices.size()<<" triangles="<<a.indices.size()/3u
             <<" roof_triangles="<<RoofIndices/3u<<" source_u_px=32,80 source_v_longitudinal_cycles_per_m=.5"
             <<" geometry_changed=false material_changed=false native=false art=false\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
