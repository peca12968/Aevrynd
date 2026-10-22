#pragma once
#include "MistwatchRootSupport.h"
#include "MistwatchTerrainPlanting.h"

namespace Mistwatch {
inline constexpr std::string_view AuthoredLandscapePlantingProfile="authored-source-foot-v1";
inline constexpr std::array<std::uint32_t,16> AuthoredLandscapeOrdinals{
    177,212,496,429,519,436,423,410,37,256,465,205,48,171,58,455};
// 三 raw SHA 封印網格經一次 f32 根部相減的完整身分；原材質／法線／索引皆納入。
inline constexpr std::array<std::uint64_t,3> AuthoredLandscapeSourceFingerprints{
    12442856667128630539ull,11046199509515305128ull,12155645616207841595ull};
struct AuthoredLandscapePlant {
    std::uint32_t sourceOrdinal{};Aevrynd::Transform3D pose{};Aevrynd::Mesh3D derivedTrunk;
    Aevrynd::MeshBounds3D bounds{};RootSupportProof proof{};
};
struct AuthoredLandscapePlantingPlan {
    std::vector<AuthoredLandscapePlant> plants;
    std::uint64_t originalPoseFingerprint{},surfaceFingerprint{},renderFingerprint{},derivedFingerprint{};
    std::array<std::uint64_t,3> sourceFingerprints{};bool complete{};
};
namespace AuthoredLandscapePlantingDetail {
using namespace Aevrynd;namespace D=::Mistwatch::LandscapeSpeciesDetail;
inline void Need(bool value,const char* label){if(!value)throw std::invalid_argument(label);}
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline void Wide(std::uint64_t& h,std::uint64_t value){D::Word(h,static_cast<std::uint32_t>(value));D::Word(h,static_cast<std::uint32_t>(value>>32));}
inline void ValidateOriginal(const std::vector<Transform3D>& roots){
    Need(roots.size()==520,"AUTHORED_LANDSCAPE_SOURCE_COUNT");
    for(unsigned i=0;i<520;++i){const auto& p=roots[i];Need(D::Finite(p.position)&&D::Finite(p.rotation)&&D::Finite(p.scale)&&
        p.rotation.x==0&&p.rotation.z==0&&p.scale.x>=.5f&&p.scale.x<=.85f&&p.scale.x==p.scale.y&&p.scale.x==p.scale.z,
        "AUTHORED_LANDSCAPE_SOURCE_POSE");
        Need(p.position.x>=-150&&p.position.x<=150&&p.position.z>=-110&&p.position.z<=190&&
            !(p.position.x>=-53&&p.position.x<=53&&p.position.z>=-23&&p.position.z<=103),"AUTHORED_LANDSCAPE_SOURCE_DOMAIN");
        for(unsigned j=0;j<i;++j)Need(std::hypot(p.position.x-roots[j].position.x,p.position.z-roots[j].position.z)>=5.f,
            "AUTHORED_LANDSCAPE_SOURCE_SPACING");}
    Need(LandscapeSourcePoseFingerprint(roots)==11953736207267797409ull,"AUTHORED_LANDSCAPE_SOURCE_SEAL");
}
inline void ValidateSources(const std::array<Mesh3D,3>& sources){
    constexpr std::array<std::size_t,3> vertices{4306,99050,1329},indices{10704,232725,5133};
    for(unsigned slot=0;slot<3;++slot){Need(sources[slot].vertices.size()==vertices[slot]&&sources[slot].indices.size()==indices[slot],
            "AUTHORED_LANDSCAPE_MESH_COUNTS");
        Need(sources[slot].Valid(),"AUTHORED_LANDSCAPE_MESH_TOPOLOGY");D::ValidateMesh(sources[slot],false);
        Need(RootSupportDetail::MeshFingerprint(sources[slot])==AuthoredLandscapeSourceFingerprints[slot],"AUTHORED_LANDSCAPE_MESH_SEAL");}
    (void)D::Measure(sources);
}
inline void ValidateSurface(const Mesh3D& mesh){
    Need(mesh.Valid(),"AUTHORED_LANDSCAPE_SURFACE_TOPOLOGY");
    for(const auto& v:mesh.vertices){Need(D::Finite({v.x,v.y,v.z})&&std::isfinite(v.u)&&std::isfinite(v.v),"AUTHORED_LANDSCAPE_SURFACE_FIELD");
        Need(v.u>=0&&v.v>=0&&v.u<=1&&v.v<=1&&v.u+v.v<=1.000001f,"AUTHORED_LANDSCAPE_MATERIAL_DOMAIN");}
    // 成熟森林只允許 UV 覆蓋配方變動；不能用任意平地或修改高度冒充 Main 的外環地表。
    const auto canonical=CreateLandscapeMesh(CreateTerrain(),true);
    Need(mesh.indices==canonical.indices&&mesh.vertices.size()==canonical.vertices.size(),"AUTHORED_LANDSCAPE_SURFACE_GEOMETRY");
    for(unsigned i=0;i<mesh.vertices.size();++i){const auto& a=mesh.vertices[i];const auto& b=canonical.vertices[i];
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,"AUTHORED_LANDSCAPE_SURFACE_GEOMETRY");}
    D::ValidateMesh(mesh,true);
}
inline MeshBounds3D CompleteBounds(const std::array<Mesh3D,3>& sources,const Mesh3D& derived,const Transform3D& pose){
    const auto bounds=TransformBounds(TerrainPlantingDetail::FinalBounds(sources,derived),pose);
    Need(D::OutsideCore(bounds),"AUTHORED_LANDSCAPE_CROWN_CORE");
    Vec3 minimum{10000,10000,10000},maximum{-10000,-10000,-10000};const auto matrix=pose.Matrix();
    for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==2?derived:sources[slot];for(const auto& v:mesh.vertices){
        const auto p=TransformPoint(matrix,{v.x,v.y,v.z});Need(D::Finite(p),"AUTHORED_LANDSCAPE_CROWN_NONFINITE");
        minimum.x=(std::min)(minimum.x,p.x);minimum.y=(std::min)(minimum.y,p.y);minimum.z=(std::min)(minimum.z,p.z);
        maximum.x=(std::max)(maximum.x,p.x);maximum.y=(std::max)(maximum.y,p.y);maximum.z=(std::max)(maximum.z,p.z);}}
    Need(maximum.x< -53.f||minimum.x>53.f||maximum.z< -23.f||minimum.z>103.f,"AUTHORED_LANDSCAPE_TRUE_CROWN_CORE");return bounds;
}
}
// 只有固定完整十六株能回傳 complete；任何首分歧直接拋出，不替補、不改根域或門檻。
inline AuthoredLandscapePlantingPlan CreateAuthoredLandscapePlantingPlan(const std::vector<Aevrynd::Transform3D>& original520,
    const Aevrynd::Mesh3D& actualMaterialLandscape,const std::array<Aevrynd::Mesh3D,3>& rootedSources){
    using namespace Aevrynd;namespace A=AuthoredLandscapePlantingDetail;namespace D=LandscapeSpeciesDetail;
    A::ValidateOriginal(original520);A::ValidateSources(rootedSources);A::ValidateSurface(actualMaterialLandscape);
    AuthoredLandscapePlantingPlan plan;plan.originalPoseFingerprint=LandscapeSourcePoseFingerprint(original520);
    plan.surfaceFingerprint=RootSupportDetail::MeshFingerprint(actualMaterialLandscape);
    for(unsigned slot=0;slot<3;++slot)plan.sourceFingerprints[slot]=RootSupportDetail::MeshFingerprint(rootedSources[slot]);
    const D::Surface surface(actualMaterialLandscape);std::array<bool,520> used{};plan.plants.reserve(16);
    for(const auto ordinal:AuthoredLandscapeOrdinals){A::Need(ordinal<520&&!used[ordinal],"AUTHORED_LANDSCAPE_FIXED_ORDINAL");
        auto pose=original520[ordinal];pose.scale={1.05f,1.05f,1.05f};const auto centre=surface.At(pose.position);
        pose.position.y=static_cast<float>(centre.Approximate());
        A::Need(Compare(D::Abs(centre-D::R::Float(pose.position.y)),D::R::Float(.00005f))<=0,"AUTHORED_LANDSCAPE_ROOT_ROUNDING");
        A::Need(TerrainPlantingDetail::NearestOrdinal(original520,used,pose.position)==ordinal&&
            (pose.position.x<0)==(original520[ordinal].position.x<0)&&
            std::hypot(pose.position.x-original520[ordinal].position.x,pose.position.z-original520[ordinal].position.z)<=12.f,
            "AUTHORED_LANDSCAPE_SOURCE_MAPPING");
        auto derived=TerrainPlantingDetail::RootTrunkToLandscape(rootedSources[2],pose,surface);
        const auto bounds=A::CompleteBounds(rootedSources,derived,pose);
        for(const auto& previous:plan.plants)A::Need(!D::Overlap(bounds,previous.bounds),"AUTHORED_LANDSCAPE_CROWN_OVERLAP");
        const auto proof=RootSupportGround(pose,rootedSources[2],derived,surface,true);
        A::Need(proof.profile==RootSupportProfile&&proof.result==RootSupportResult::Safe&&proof.sourceMeshFingerprint==plan.sourceFingerprints[2]&&
            proof.derivedMeshFingerprint==RootSupportDetail::MeshFingerprint(derived)&&proof.sourceFootFingerprint==17965770522934596195ull&&
            proof.sourceRimSegments==5&&proof.sourceNonmanifoldEdges==51,"AUTHORED_LANDSCAPE_ROOT_SUPPORT");
        used[ordinal]=true;plan.plants.push_back({ordinal,pose,std::move(derived),bounds,proof});
    }
    A::Need(plan.plants.size()==16,"AUTHORED_LANDSCAPE_COMPLETE_16");
    plan.renderFingerprint=14695981039346656037ull;plan.derivedFingerprint=14695981039346656037ull;
    for(unsigned i=0;i<520;++i){D::Word(plan.renderFingerprint,i);D::Word(plan.renderFingerprint,used[i]?1u:0u);
        const auto found=std::find_if(plan.plants.begin(),plan.plants.end(),[&](const auto& plant){return plant.sourceOrdinal==i;});
        D::Pose(plan.renderFingerprint,found==plan.plants.end()?original520[i]:found->pose);}
    for(unsigned i=0;i<16;++i){D::Word(plan.derivedFingerprint,i);D::Word(plan.derivedFingerprint,plan.plants[i].sourceOrdinal);
        A::Wide(plan.derivedFingerprint,plan.plants[i].proof.derivedMeshFingerprint);}
    A::Need(LandscapeSourcePoseFingerprint(original520)==plan.originalPoseFingerprint&&
        RootSupportDetail::MeshFingerprint(actualMaterialLandscape)==plan.surfaceFingerprint,"AUTHORED_LANDSCAPE_INPUT_MUTATION");
    for(unsigned slot=0;slot<3;++slot)A::Need(RootSupportDetail::MeshFingerprint(rootedSources[slot])==plan.sourceFingerprints[slot],"AUTHORED_LANDSCAPE_MESH_MUTATION");
    plan.complete=true;return plan;
}
}
