#pragma once
#include "MistwatchLandscapeStandFill.h"
#include <limits>

namespace Mistwatch::OuterForestFloor {
inline constexpr std::string_view Profile="outer-tree-habitat-v1";
inline constexpr float MinimumRadius=6.f,MaximumRadius=12.f,CoreFade=12.f,SlopeLimit=.72f;
inline constexpr double Strength=.72;
struct Receipt {
    std::size_t roots{},vertices{},changed{},seams{},protectedVertices{},protectedTriangles{},emptyVertices{},clearingVertices{},changedTriangles{};
    std::uint64_t poseFingerprint{},beforeFingerprint{},afterFingerprint{},beforeHabitatFingerprint{},afterHabitatFingerprint{};
    std::array<std::size_t,6> bandChanged{};
    double soilDelta{},rockDelta{},areaWeightedSoilDelta{};
    float minimumRadius{MaximumRadius},maximumRadius{MinimumRadius};
};
namespace Detail {
inline void Need(bool okay,const char* reason){if(!okay)throw std::invalid_argument(reason);}
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline double Smooth(double t){t=std::clamp(t,0.0,1.0);return t*t*(3.0-2.0*t);}
inline bool Seam(const Aevrynd::Vertex3D& v){return v.x>=-50.f&&v.x<=50.f&&v.z>=-20.f&&v.z<=100.f&&
    (v.x==-50.f||v.x==50.f||v.z==-20.f||v.z==100.f);}
inline double Radius(const Aevrynd::Transform3D& root){
    return 6.0+6.0*std::clamp((double(root.scale.x)-.5)/.35,0.0,1.0);
}
inline double NormalY(const Aevrynd::Vertex3D& v){const auto n=Aevrynd::UnpackNormalSnorm8(v.normal);
    const double length=std::sqrt(double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z);
    Need(std::isfinite(length)&&length>.5&&length<1.5,"OUTER_NORMAL");return double(n.y)/length;
}
// 平滑聯集使相鄰真樹根連成林下群落；不使用 max 接縫或單根小圓圈。
// 半徑為世界公尺，不是直徑。核心矩形以外十二公尺內連續收束。
inline double Field(const LandscapeStandFill::Plan& plan,double x,double z,double normalY){
    const double dx=(std::max)(0.0,std::abs(x)-50.0);
    const double dz=(std::max)({0.0,-20.0-z,z-100.0});
    const double edge=Smooth(std::hypot(dx,dz)/double(CoreFade));
    const double slope=Smooth((normalY-double(SlopeLimit))/(.90-double(SlopeLimit)));
    if(edge==0.0||slope==0.0)return 0.0;
    double remaining=1.0;
    for(const auto& root:plan.combined){const double radius=Radius(root);
        const double distance=std::hypot(x-root.position.x,z-root.position.z);
        if(distance>=radius)continue;
        const double kernel=Smooth(1.0-distance/radius);
        remaining*=1.0-.85*kernel;
    }
    return Strength*(1.0-remaining)*edge*slope;
}
inline void ValidatePlan(const Aevrynd::HeightField3D& terrain,const LandscapeStandFill::Plan& plan,
                         const Aevrynd::MeshBounds3D& bounds){
    namespace S=LandscapeStandFill;
    Need(plan.complete&&plan.combined.size()==S::BaseCount+S::Target&&plan.addedBands.size()==S::Target,"OUTER_PLAN_COUNT");
    for(const auto& p:plan.combined)Need(S::Detail::Finite(p.position)&&S::Detail::Finite(p.rotation)&&S::Detail::Finite(p.scale),"OUTER_POSE_FINITE");
    const auto original=LandscapeTrees(terrain);
    Need(original.size()==S::BaseCount,"OUTER_CANONICAL_COUNT");
    for(unsigned i=0;i<S::BaseCount;++i)Need(S::Detail::Same(original[i],plan.combined[i]),"OUTER_SOURCE_POSE");
    const auto canonical=S::Create(terrain,original,bounds);
    Need(canonical.complete,"OUTER_CANONICAL_INCOMPLETE");
    for(std::size_t i=S::BaseCount;i<plan.combined.size();++i)
        Need(S::Detail::Same(plan.combined[i],canonical.combined[i]),"OUTER_SUFFIX_POSE");
    Need(plan.addedBands==canonical.addedBands,"OUTER_BANDS");
    Need(plan.sourceFingerprint==canonical.sourceFingerprint&&plan.fingerprint==canonical.fingerprint,"OUTER_POSE_FINGERPRINT");
    Need(plan.generated==canonical.generated,"OUTER_PROPOSALS");
    for(unsigned i=0;i<S::BandCount;++i){const auto& a=plan.bands[i];const auto& b=canonical.bands[i];
        Need(a.proposals==b.proposals&&a.accepted==b.accepted&&a.slope==b.slope&&a.core==b.core&&a.edge==b.edge&&a.spacing==b.spacing,"OUTER_BAND_COUNTS");}
}
}
// 與既有完整網格指紋相同的欄位順序，可核對真正上傳的 CPU mesh。
inline std::uint64_t MeshFingerprint(const Aevrynd::Mesh3D& mesh){
    std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t v){LandscapeStandFill::Detail::Word(h,v);};
    word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));word(v.normal);word(v.color);}
    for(const auto i:mesh.indices)word(i);return h;
}
// 保留舊 habitat 收據的不同編碼順序，不把舊中間態冒充最終上傳結果。
inline std::uint64_t HabitatFingerprint(const Aevrynd::Mesh3D& mesh){
    std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t v){LandscapeStandFill::Detail::Word(h,v);};
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));word(v.color);word(v.normal);}
    for(const auto i:mesh.indices)word(i);return h;
}
// 僅供現有作者外圈候選；完整前置驗證後才交換頂點，任何拒絕均不部分寫入。
inline Receipt Apply(Aevrynd::Mesh3D& mesh,const Aevrynd::HeightField3D& terrain,
                     const LandscapeStandFill::Plan& plan,const Aevrynd::MeshBounds3D& fullTreeUnionBounds){
    using namespace Aevrynd;namespace D=Detail;
    const auto& settings=terrain.Settings();
    D::Need(settings.width==201&&settings.depth==241&&settings.cellSize==.5f&&
        settings.origin.x==-50.f&&settings.origin.y==0.f&&settings.origin.z==-20.f,"OUTER_TERRAIN_LAYOUT");
    for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x)
        D::Need(std::isfinite(terrain.Height(x,z)),"OUTER_TERRAIN_FINITE");
    D::ValidatePlan(terrain,plan,fullTreeUnionBounds);
    const auto geometry=CreateLandscapeMesh(terrain,true);
    D::Need(mesh.vertices.size()==geometry.vertices.size()&&mesh.indices.size()==geometry.indices.size(),"OUTER_MESH_COUNT");
    for(const auto i:mesh.indices)D::Need(i<mesh.vertices.size(),"OUTER_INDEX_RANGE");
    D::Need(mesh.indices==geometry.indices,"OUTER_TOPOLOGY");
    std::vector<double> normals;normals.reserve(mesh.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& a=mesh.vertices[i];const auto& b=geometry.vertices[i];
        D::Need(std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z),"OUTER_POSITION_FINITE");
        Vec2 encoded{};D::Need(TryEncodeTerrainLayerCoverage3D(a.u,a.v,encoded),"OUTER_COVERAGE");
        normals.push_back(D::NormalY(a));
        D::Need(D::Same(a.x,b.x)&&D::Same(a.y,b.y)&&D::Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,"OUTER_GEOMETRY");
    }
    Receipt r;r.roots=plan.combined.size();r.vertices=mesh.vertices.size();r.poseFingerprint=plan.fingerprint;
    r.beforeFingerprint=MeshFingerprint(mesh);r.beforeHabitatFingerprint=HabitatFingerprint(mesh);
    std::vector<bool> frozen(mesh.vertices.size(),false);
    std::vector<double> areas;areas.reserve(mesh.indices.size()/3);
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
        const auto& a=mesh.vertices[ia];const auto& b=mesh.vertices[ib];const auto& c=mesh.vertices[ic];
        const Vec3 cross=Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
        const double length=std::sqrt(double(cross.x)*cross.x+double(cross.y)*cross.y+double(cross.z)*cross.z);
        D::Need(std::isfinite(length)&&length>0&&cross.y>0,"OUTER_FACE");areas.push_back(length*.5);
        if(double(cross.y)/length<double(SlopeLimit)||normals[ia]<SlopeLimit||normals[ib]<SlopeLimit||normals[ic]<SlopeLimit){
            ++r.protectedTriangles;frozen[ia]=true;frozen[ib]=true;frozen[ic]=true;}
    }
    for(const auto& root:plan.combined){const auto radius=static_cast<float>(D::Radius(root));
        r.minimumRadius=(std::min)(r.minimumRadius,radius);r.maximumRadius=(std::max)(r.maximumRadius,radius);}
    auto candidate=mesh;
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& old=mesh.vertices[i];auto& v=candidate.vertices[i];
        const bool seam=D::Seam(old);r.seams+=seam;r.protectedVertices+=frozen[i];
        const double field=D::Field(plan,old.x,old.z,normals[i]);r.emptyVertices+=field==0.0&&!seam&&!frozen[i];
        r.clearingVertices+=field==0.0&&!seam&&!frozen[i]&&old.x>=-150.f&&old.x<=150.f&&old.z>=-110.f&&old.z<=190.f&&
            std::hypot((std::max)(0.f,std::abs(old.x)-50.f),(std::max)({0.f,-20.f-old.z,old.z-100.f}))>=CoreFade;
        if(!seam&&!frozen[i]){
            const double spare=(std::max)(0.0,1.0-double(old.u)-double(old.v));
            v.u=static_cast<float>(double(old.u)+spare*field);
            Vec2 encoded{};D::Need(TryEncodeTerrainLayerCoverage3D(v.u,v.v,encoded),"OUTER_RESULT_COVERAGE");
        }
        const bool changed=!D::Same(old.u,v.u)||!D::Same(old.v,v.v);r.changed+=changed;
        r.soilDelta+=double(v.u)-old.u;r.rockDelta+=double(v.v)-old.v;
        if(changed)for(unsigned band=0;band<LandscapeStandFill::BandCount;++band){const auto& region=LandscapeStandFill::Bands[band];
            if(v.x>=region.minX&&v.x<=region.maxX&&v.z>=region.minZ&&v.z<=region.maxZ)++r.bandChanged[band];}
    }
    for(std::size_t i=0;i<mesh.indices.size();i+=3){bool changed=false;double delta{};
        for(unsigned j=0;j<3;++j){const auto index=mesh.indices[i+j];const double d=double(candidate.vertices[index].u)-mesh.vertices[index].u;
            changed=changed||d!=0;delta+=d;}
        r.changedTriangles+=changed;r.areaWeightedSoilDelta+=areas[i/3]*delta/3.0;
    }
    D::Need(r.seams==880,"OUTER_SEAM_COUNT");
    r.afterFingerprint=MeshFingerprint(candidate);r.afterHabitatFingerprint=HabitatFingerprint(candidate);
    // v 完全保留僅代表作者岩覆蓋未改；shader 中的最終岩權重仍會隨 u 改變。
    mesh.vertices.swap(candidate.vertices);return r;
}
}
