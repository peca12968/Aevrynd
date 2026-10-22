#pragma once
#include "MistwatchLandscape.h"
#include "MistwatchSettlementGround.h"
#include <bit>
#include <string_view>

namespace Mistwatch::CliffCoverage {
enum class Domain : std::uint8_t { Core,Ring };
enum class Profile : std::uint8_t { Original,WestCliffV1 };
inline constexpr std::string_view ProfileName="west-cliff-rock-coverage-v1";
struct Receipt {
    std::size_t vertices{},changed{},zeroMask{},protectedVertices{},fullMask{};
    std::uint64_t beforeFingerprint{},afterFingerprint{},beforeGeometryFingerprint{},afterGeometryFingerprint{};
    float maximumMask{},maximumSoil{},minimumFullRock{1.f};
};
struct Result {Aevrynd::Mesh3D mesh;Receipt receipt;};
inline bool Protected(float x,float y,float z) {
    return y<=RiverHeight||RiverDistance(x,z)<=RiverOuterWidth(x)||
        CentralTrailVisualWeight(x,z,ForestFloorMaskSelector::ForestWood)>0.f||
        SettlementTrailDistance(x,z)<=1.9f||SettlementGround::InInfluence(x,z);
}
inline float GeographicMask(float x,float z) {
    return ForestFloorSmooth((x+56.f)/2.f)*ForestFloorSmooth((-40.f-x)/2.f)*
        ForestFloorSmooth((z-50.f)/2.f)*ForestFloorSmooth((66.f-z)/2.f);
}
inline float Mask(const Aevrynd::HeightField3D& core,float x,float y,float z) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))throw std::invalid_argument("CLIFF_MASK_NONFINITE");
    const float geographic=GeographicMask(x,z);
    if(geographic==0.f||Protected(x,y,z))return 0.f;
    // core.Contains 含邊界；core/ring 同座標查同法線，不分別以各網格面積平均。
    const auto normal=LandscapeNormal(core,x,z);
    if(!std::isfinite(normal.y))throw std::invalid_argument("CLIFF_NORMAL_NONFINITE");
    constexpr float cosine40=.7660444431f,cosine52=.6156614753f;
    return geographic*ForestFloorSmooth((cosine40-normal.y)/(cosine40-cosine52));
}
namespace Detail {
inline void Need(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
// 幾何封印只含 XYZ 與索引；完整封印另含法線、色與材質覆蓋。
inline std::uint64_t GeometryFingerprint(const Aevrynd::Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    const auto mix=[&](std::uint32_t word){for(unsigned b=0;b<4;++b){hash^=(word>>(b*8))&255u;hash*=1099511628211ull;}};
    for(const auto& v:mesh.vertices)for(float value:{v.x,v.y,v.z})mix(std::bit_cast<std::uint32_t>(value));
    for(auto index:mesh.indices)mix(index);
    return hash;
}
inline void Validate(const Aevrynd::Mesh3D& mesh,const Aevrynd::HeightField3D& core,Domain domain,Profile profile) {
    Need(domain==Domain::Core||domain==Domain::Ring,"CLIFF_UNKNOWN_DOMAIN");
    Need(profile==Profile::Original||profile==Profile::WestCliffV1,"CLIFF_UNKNOWN_PROFILE");
    const auto& settings=core.Settings();
    Need(settings.origin.x==-50.f&&settings.origin.y==0.f&&settings.origin.z==-20.f&&settings.width==201&&
        settings.depth==241&&settings.cellSize==.5f,"CLIFF_CORE_SETTINGS");
    for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x)
        Need(std::isfinite(core.Height(x,z)),"CLIFF_CORE_HEIGHT");
    Need(mesh.Valid()&&mesh.vertices.size()<=100000&&mesh.indices.size()<=600000,"CLIFF_MESH_SHAPE");
    for(const auto& v:mesh.vertices){
        Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.y)<=10000.f,"CLIFF_VERTEX");
        Need(std::isfinite(v.u)&&std::isfinite(v.v)&&v.u>=0.f&&v.v>=0.f&&v.u<=1.f&&v.v<=1.f&&v.u+v.v<=1.f,"CLIFF_ENCODED_UV");
        const auto n=Aevrynd::UnpackNormalSnorm8(v.normal);const float length=Aevrynd::Length(Aevrynd::Vec3{n.x,n.y,n.z});
        Need(std::isfinite(length)&&length>=.98f&&length<=1.02f,"CLIFF_PACKED_NORMAL");
        if(domain==Domain::Core)Need(v.x>=-50.f&&v.x<=50.f&&v.z>=-20.f&&v.z<=100.f,"CLIFF_CORE_DOMAIN");
        else Need(v.x>=-200.f&&v.x<=200.f&&v.z>=-170.f&&v.z<=250.f&&
            (v.x<=-50.f||v.x>=50.f||v.z<=-20.f||v.z>=100.f),"CLIFF_RING_DOMAIN");
    }
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];
        const auto& c=mesh.vertices[mesh.indices[i+2]];
        const double area=(double(b.x)-a.x)*(double(c.z)-a.z)-(double(b.z)-a.z)*(double(c.x)-a.x);
        Need(std::isfinite(area)&&std::abs(area)>1e-12,"CLIFF_DEGENERATE_XZ");
    }
}
}
inline Result Create(const Aevrynd::Mesh3D& encodedSource,const Aevrynd::HeightField3D& core,
    Domain domain,Profile profile=Profile::WestCliffV1) {
    Detail::Validate(encodedSource,core,domain,profile);
    // 輸入為 const；任何檢查或配置失敗都不會留下部分改寫。
    Result out{encodedSource,{}};auto& r=out.receipt;r.vertices=encodedSource.vertices.size();
    r.beforeFingerprint=EntranceRoadDomainDetail::Fingerprint(encodedSource);
    r.beforeGeometryFingerprint=Detail::GeometryFingerprint(encodedSource);
    for(auto& v:out.mesh.vertices){
        r.protectedVertices+=Protected(v.x,v.y,v.z);
        const float mask=profile==Profile::Original?0.f:Mask(core,v.x,v.y,v.z);
        r.maximumMask=(std::max)(r.maximumMask,mask);
        if(mask==0.f){++r.zeroMask;continue;}
        const float u=v.u*(1.f-mask),rock=mask==1.f?.95f:v.v+(.95f-v.v)*mask;
        Aevrynd::Vec2 encoded{};
        Detail::Need(Aevrynd::TryEncodeTerrainLayerCoverage3D(u,rock,encoded),"CLIFF_OUTPUT_COVERAGE");
        r.changed+=std::bit_cast<std::uint32_t>(v.u)!=std::bit_cast<std::uint32_t>(encoded.x)||
            std::bit_cast<std::uint32_t>(v.v)!=std::bit_cast<std::uint32_t>(encoded.y);
        v.u=encoded.x;v.v=encoded.y;r.maximumSoil=(std::max)(r.maximumSoil,v.u);
        if(mask==1.f){++r.fullMask;r.minimumFullRock=(std::min)(r.minimumFullRock,v.v);}
    }
    r.afterFingerprint=EntranceRoadDomainDetail::Fingerprint(out.mesh);
    r.afterGeometryFingerprint=Detail::GeometryFingerprint(out.mesh);
    return out;
}
}
