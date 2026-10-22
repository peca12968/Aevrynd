#pragma once
#include "MistwatchLandscape.h"
#include <array>
#include <bit>
#include <numeric>
#include <string_view>

namespace Mistwatch::LandscapeStandFill {
inline constexpr std::string_view Profile="landscape-stand-fill-v2";
inline constexpr unsigned BaseCount=520,BandCount=6,PerBand=16,Target=96,Grid=24;
inline constexpr unsigned MaximumProposals=BandCount*Grid*Grid;
struct Band {float minX,maxX,minZ,maxZ;};
// 固定世界肩坡，不用相機決定位置；陡壁仍由既有坡度門拒絕。
inline constexpr std::array<Band,BandCount> Bands{{
    {-102.f,-56.f,-12.f,48.f},{-106.f,-56.f,48.f,98.f},
    {56.f,102.f,-12.f,48.f},{56.f,106.f,48.f,98.f},
    {-48.f,-2.f,108.f,160.f},{2.f,48.f,108.f,160.f}}};
struct Counts {unsigned proposals{},accepted{},slope{},core{},edge{},spacing{};};
struct Plan {
    std::vector<Aevrynd::Transform3D> combined;
    std::vector<unsigned> addedBands;
    std::array<Counts,BandCount> bands{};
    unsigned generated{};bool complete{};
    std::uint64_t sourceFingerprint{},fingerprint{};
};
namespace Detail {
inline void Need(bool yes,const char* message){if(!yes)throw std::invalid_argument(message);}
inline bool Finite(Aevrynd::Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline void Word(std::uint64_t& h,std::uint32_t value){for(unsigned i=0;i<4;++i){h^=(value>>(8*i))&255u;h*=1099511628211ull;}}
inline void Pose(std::uint64_t& h,const Aevrynd::Transform3D& p){
    for(float f:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z})
        Word(h,std::bit_cast<std::uint32_t>(f));
}
inline bool Same(const Aevrynd::Transform3D& a,const Aevrynd::Transform3D& b){
    const std::array<float,9> x{a.position.x,a.position.y,a.position.z,a.rotation.x,a.rotation.y,a.rotation.z,a.scale.x,a.scale.y,a.scale.z};
    const std::array<float,9> y{b.position.x,b.position.y,b.position.z,b.rotation.x,b.rotation.y,b.rotation.z,b.scale.x,b.scale.y,b.scale.z};
    for(unsigned i=0;i<x.size();++i)if(std::bit_cast<std::uint32_t>(x[i])!=std::bit_cast<std::uint32_t>(y[i]))return false;
    return true;
}
inline std::uint32_t Mix(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline std::uint64_t Fingerprint(const std::vector<Aevrynd::Transform3D>& poses){
    std::uint64_t h=14695981039346656037ull;Word(h,static_cast<std::uint32_t>(poses.size()));
    for(const auto& p:poses)Pose(h,p);return h;
}
}
// shortfall 是有意保留的候選結果，絕不降低門檻或回報假足額。
// fullTreeUnionBounds 必須涵蓋四個 LOD2 slot 與 near twig；本函式不載入或新增 GPU 資源。
inline Plan Create(const Aevrynd::HeightField3D& terrain,
                   const std::vector<Aevrynd::Transform3D>& original520,
                   const Aevrynd::MeshBounds3D& fullTreeUnionBounds,
                   unsigned proposalLimit=MaximumProposals){
    using namespace Aevrynd;namespace D=Detail;
    D::Need(proposalLimit<=MaximumProposals,"STAND_PROPOSAL_LIMIT");
    const auto& s=terrain.Settings();
    D::Need(s.width==201&&s.depth==241&&s.cellSize==.5f&&s.origin.x==-50.f&&s.origin.y==0.f&&s.origin.z==-20.f,
            "STAND_TERRAIN_LAYOUT");
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)
        D::Need(std::isfinite(terrain.Height(x,z)),"STAND_TERRAIN_FINITE");
    const auto& b=fullTreeUnionBounds;
    D::Need(D::Finite(b.center)&&D::Finite(b.extents)&&std::isfinite(b.radius)&&
        b.extents.x>0&&b.extents.y>0&&b.extents.z>0&&b.radius>0&&
        b.radius>=(std::max)({b.extents.x,b.extents.y,b.extents.z}),"STAND_UNION_BOUNDS");
    D::Need(original520.size()==BaseCount,"STAND_SOURCE_COUNT");
    const auto canonical=LandscapeTrees(terrain);
    D::Need(canonical.size()==BaseCount,"STAND_CANONICAL_COUNT");
    for(unsigned i=0;i<BaseCount;++i)D::Need(D::Same(original520[i],canonical[i]),"STAND_SOURCE_POSE");
    Plan result;result.combined=original520;result.combined.reserve(BaseCount+Target);
    result.sourceFingerprint=D::Fingerprint(original520);
    std::array<unsigned,Grid*Grid> order{};std::iota(order.begin(),order.end(),0u);
    const auto radius=[](unsigned cell){const int x=2*int(cell%Grid)-int(Grid-1),z=2*int(cell/Grid)-int(Grid-1);return x*x+z*z;};
    std::sort(order.begin(),order.end(),[&](unsigned a,unsigned c){return radius(a)!=radius(c)?radius(a)<radius(c):a<c;});
    for(unsigned band=0;band<BandCount;++band){auto& stats=result.bands[band];const auto& region=Bands[band];
        for(const auto cell:order){
            if(stats.accepted==PerBand||result.generated==proposalLimit)break;
            ++result.generated;++stats.proposals;
            // 格點內確定的微位移避免可見列隊；每一生成姿態都計入 proposal。
            const auto h=D::Mix(band*Grid*Grid+cell+1u);
            const float jx=.15f+.7f*float(h&65535u)/65535.f,jz=.15f+.7f*float(h>>16)/65535.f;
            const float x=region.minX+(region.maxX-region.minX)*(float(cell%Grid)+jx)/float(Grid);
            const float z=region.minZ+(region.maxZ-region.minZ)*(float(cell/Grid)+jz)/float(Grid);
            const auto n=LandscapeNormal(terrain,x,z);
            D::Need(D::Finite(n),"STAND_SURFACE_FINITE");
            if(n.y<.78f){++stats.slope;continue;}
            const auto v=D::Mix(h);const float scale=.5f+.35f*float(v&65535u)/65535.f;
            Transform3D pose{{x,LandscapeSurfaceHeight(terrain,x,z),z},{0,float(v>>16)/65535.f*6.2831853f,0},{scale,scale,scale}};
            D::Need(D::Finite(pose.position),"STAND_SURFACE_FINITE");
            const auto world=TransformBounds(b,pose);const auto lo=world.center-world.extents,hi=world.center+world.extents;
            D::Need(D::Finite(lo)&&D::Finite(hi),"STAND_TRANSFORM_FINITE");
            if(!(hi.x< -53.f||lo.x>53.f||hi.z< -23.f||lo.z>103.f)){++stats.core;continue;}
            if(lo.x< -200.f||hi.x>200.f||lo.z< -170.f||hi.z>250.f){++stats.edge;continue;}
            bool occupied=false;for(const auto& old:result.combined)
                if(std::hypot(double(x)-old.position.x,double(z)-old.position.z)<5.0){occupied=true;break;}
            if(occupied){++stats.spacing;continue;}
            result.combined.push_back(pose);result.addedBands.push_back(band);++stats.accepted;
        }
    }
    result.complete=result.addedBands.size()==Target;
    result.fingerprint=D::Fingerprint(result.combined);
    for(auto band:result.addedBands)D::Word(result.fingerprint,band);
    return result;
}
}
