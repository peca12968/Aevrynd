#pragma once
#include "MistwatchLandscapeSpecies.h"

namespace Mistwatch {
enum class TerrainPlantingProfile : std::uint8_t { Original, AuthoredEdgeRootedV1 };
struct TerrainPlantingAnchor {float x,z,yaw,scale;};
// 近入口四株先驗；其餘皆固定作者位置，不抽樣、不補種、不降低十六株目標。
inline constexpr std::array<TerrainPlantingAnchor,16> TerrainPlantingAnchors{{
    {-58.5f,2.f,.30f,1.04f},{-58.5f,16.f,2.20f,1.08f},
    {59.5f,9.f,5.08f,1.05f},{59.5f,23.f,.90f,1.07f},
    {-58.5f,-12.f,1.40f,1.06f},{-58.5f,30.f,3.40f,1.03f},
    {-58.5f,44.f,5.60f,1.08f},{-58.5f,58.f,.70f,1.05f},
    {-58.5f,72.f,2.70f,1.04f},{-58.5f,88.f,4.80f,1.07f},
    {59.5f,-5.f,2.00f,1.06f},{59.5f,37.f,4.10f,1.03f},
    {59.5f,51.f,.10f,1.08f},{59.5f,65.f,2.90f,1.05f},
    {59.5f,79.f,4.90f,1.04f},{59.5f,93.f,1.70f,1.07f}}};
enum class TerrainPlantingResult : std::uint8_t {
    Pending,Accepted,NoNearbySource,CrownCore,TreeSpacing,CrownOverlap,AnalyticSlope,ExactSlope,GroundGap
};
inline const char* TerrainPlantingResultName(TerrainPlantingResult result){
    switch(result){case TerrainPlantingResult::Pending:return "pending";case TerrainPlantingResult::Accepted:return "accepted";
    case TerrainPlantingResult::NoNearbySource:return "no-nearby-source";case TerrainPlantingResult::CrownCore:return "crown-core";
    case TerrainPlantingResult::TreeSpacing:return "tree-spacing";case TerrainPlantingResult::CrownOverlap:return "crown-overlap";
    case TerrainPlantingResult::AnalyticSlope:return "analytic-slope";case TerrainPlantingResult::ExactSlope:return "exact-slope";
    case TerrainPlantingResult::GroundGap:return "ground-gap";}return "invalid";
}
struct TerrainPlantingDiagnostic {
    TerrainPlantingResult result{TerrainPlantingResult::Pending};unsigned sourceOrdinal{~0u};
    float sourceDistance{},slope{},rootError{};std::uint64_t meshFingerprint{};
    LandscapeSpeciesDetail::GroundProof ground{};
};
struct TerrainPlantedTree {
    unsigned anchorIndex{},sourceOrdinal{};Aevrynd::Transform3D pose{};
    Aevrynd::Mesh3D derivedTrunk;Aevrynd::MeshBounds3D finalBounds{};
    std::uint64_t meshFingerprint{};LandscapeSpeciesDetail::GroundProof ground{};
};
struct TerrainPlantingPlan {
    std::array<LandscapeSpeciesInstance,520> instances{};
    std::vector<TerrainPlantedTree> plants;
    std::array<TerrainPlantingDiagnostic,16> diagnostics{};
    unsigned firstMissingAnchor{~0u};bool complete{};
    std::uint64_t sourcePoseFingerprint{},renderFingerprint{},derivedFingerprint{};
};
inline std::array<std::uint32_t,9> TerrainPlantingPoseWords(const Aevrynd::Transform3D& p){return{
    std::bit_cast<std::uint32_t>(p.position.x),std::bit_cast<std::uint32_t>(p.position.y),std::bit_cast<std::uint32_t>(p.position.z),
    std::bit_cast<std::uint32_t>(p.rotation.x),std::bit_cast<std::uint32_t>(p.rotation.y),std::bit_cast<std::uint32_t>(p.rotation.z),
    std::bit_cast<std::uint32_t>(p.scale.x),std::bit_cast<std::uint32_t>(p.scale.y),std::bit_cast<std::uint32_t>(p.scale.z)};}
namespace TerrainPlantingDetail {
using namespace Aevrynd;
namespace D=::Mistwatch::LandscapeSpeciesDetail;
inline void Need(bool value,const char* label){if(!value)throw std::invalid_argument(label);}
inline std::uint64_t MeshFingerprint(const Mesh3D& mesh){std::uint64_t hash=14695981039346656037ull;
    D::Word(hash,static_cast<unsigned>(mesh.vertices.size()));D::Word(hash,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})D::Word(hash,std::bit_cast<std::uint32_t>(f));D::Word(hash,v.normal);D::Word(hash,v.color);}
    for(const auto i:mesh.indices)D::Word(hash,i);return hash;
}
inline unsigned NearestOrdinal(std::span<const Transform3D> original,const std::array<bool,520>& used,Vec3 root){
    Need(original.size()==520&&D::Finite(root),"TERRAIN_PLANTING_NEAREST_INPUT");unsigned best=~0u;double minimum=144.;
    for(unsigned i=0;i<520;++i){if(used[i]||(original[i].position.x<0)!=(root.x<0))continue;
        const double x=double(root.x)-original[i].position.x,z=double(root.z)-original[i].position.z,d=x*x+z*z;
        if(d<minimum||(d==minimum&&(best==~0u||i<best))){minimum=d;best=i;}}
    return best;
}
// 真 trunk 頂點在最低 .35m 連續貼合提交的外環三角面；不借核心高度 clamp。
inline Mesh3D RootTrunkToLandscape(const Mesh3D& original,const Transform3D& pose,const D::Surface& surface){
    D::ValidateMesh(original,false);Need(D::Finite(pose.position)&&D::Finite(pose.rotation)&&D::Finite(pose.scale)&&
        pose.rotation.x==0&&pose.rotation.z==0&&pose.scale.x>0&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z,
        "TERRAIN_PLANTING_ROOT_POSE");
    float rootMinimum=10000;for(const auto& v:original.vertices)rootMinimum=(std::min)(rootMinimum,v.y);
    Need(rootMinimum==0.f,"TERRAIN_PLANTING_ROOT_SOURCE_MINIMUM");
    const auto centre=surface.At(pose.position);const auto matrix=pose.Matrix();auto result=original;
    for(std::size_t i=0;i<result.vertices.size();++i){const auto& source=original.vertices[i];
        const float t=std::clamp(source.y/.35f,0.f,1.f),weight=1.f-t*t*(3.f-2.f*t);if(weight<=0)continue;
        const auto world=TransformPoint(matrix,{source.x,source.y,source.z});const auto delta=surface.At(world)-centre;
        result.vertices[i].y+=static_cast<float>(delta.Approximate())*weight/pose.scale.x;
        Need(std::isfinite(result.vertices[i].y),"TERRAIN_PLANTING_ROOT_NONFINITE");}
    std::vector<Vec3> normals(result.vertices.size());std::vector<bool> affected(result.vertices.size());
    for(unsigned f=0;f<result.indices.size();f+=3){const auto a=result.indices[f],b=result.indices[f+1],c=result.indices[f+2];
        const auto n=Cross(D::Vertex(result,b)-D::Vertex(result,a),D::Vertex(result,c)-D::Vertex(result,a));
        Need(D::Finite(n)&&Length(n)>0,"TERRAIN_PLANTING_ROOT_DEGENERATE_FACE");
        normals[a]=normals[a]+n;normals[b]=normals[b]+n;normals[c]=normals[c]+n;
        if(std::bit_cast<std::uint32_t>(original.vertices[a].y)!=std::bit_cast<std::uint32_t>(result.vertices[a].y)||
           std::bit_cast<std::uint32_t>(original.vertices[b].y)!=std::bit_cast<std::uint32_t>(result.vertices[b].y)||
           std::bit_cast<std::uint32_t>(original.vertices[c].y)!=std::bit_cast<std::uint32_t>(result.vertices[c].y))affected[a]=affected[b]=affected[c]=true;}
    // 受變形面相鄰的上幹位置不動，但其法線必須反映真面，不僅更新低頂點。
    for(std::size_t i=0;i<result.vertices.size();++i)if(affected[i]){
        Need(D::Finite(normals[i])&&Length(normals[i])>1.e-8f,"TERRAIN_PLANTING_ROOT_DEGENERATE_NORMAL");
        result.vertices[i].normal=PackNormalSnorm8(Normalize(normals[i]),UnpackNormalSnorm8(original.vertices[i].normal).w);}
    return result;
}
inline MeshBounds3D FinalBounds(const std::array<Mesh3D,3>& source,const Mesh3D& derived){
    Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};
    for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==2?derived:source[slot];
        for(const auto& v:mesh.vertices){low.x=(std::min)(low.x,v.x);low.y=(std::min)(low.y,v.y);low.z=(std::min)(low.z,v.z);
            high.x=(std::max)(high.x,v.x);high.y=(std::max)(high.y,v.y);high.z=(std::max)(high.z,v.z);}}
    const auto extents=(high-low)*.5f;return{(low+high)*.5f,Length(extents),extents};
}
inline void SealPlan(TerrainPlantingPlan& plan){plan.renderFingerprint=14695981039346656037ull;plan.derivedFingerprint=14695981039346656037ull;
    for(const auto& instance:plan.instances){D::Word(plan.renderFingerprint,instance.sourceOrdinal);D::Word(plan.renderFingerprint,static_cast<unsigned>(instance.species));D::Pose(plan.renderFingerprint,instance.render);}
    for(const auto& plant:plan.plants){D::Word(plan.derivedFingerprint,plant.anchorIndex);D::Word(plan.derivedFingerprint,plant.sourceOrdinal);
        D::Word(plan.derivedFingerprint,static_cast<std::uint32_t>(plant.meshFingerprint));D::Word(plan.derivedFingerprint,static_cast<std::uint32_t>(plant.meshFingerprint>>32));}
}
}
inline TerrainPlantingPlan CreateTerrainPlantingPlan(const Aevrynd::HeightField3D& core,const Aevrynd::Mesh3D& actualLandscape,
    std::span<const Aevrynd::Transform3D> original,const std::array<Aevrynd::Mesh3D,3>& rootedSource,std::uint64_t sourcePoseSeal,
    TerrainPlantingProfile profile=TerrainPlantingProfile::Original){
    using namespace Aevrynd;namespace D=LandscapeSpeciesDetail;using TerrainPlantingDetail::Need;
    Need(profile==TerrainPlantingProfile::Original||profile==TerrainPlantingProfile::AuthoredEdgeRootedV1,"TERRAIN_PLANTING_INVALID_PROFILE");
    Need(original.size()==520,"TERRAIN_PLANTING_SOURCE_COUNT");
    for(unsigned i=0;i<520;++i){const auto& p=original[i];Need(D::Finite(p.position)&&D::Finite(p.rotation)&&D::Finite(p.scale)&&p.rotation.x==0&&p.rotation.z==0&&
        p.scale.x>=.5f&&p.scale.x<=.85f&&p.scale.x==p.scale.y&&p.scale.x==p.scale.z,"TERRAIN_PLANTING_SOURCE_POSE");
        Need(p.position.x>=-150&&p.position.x<=150&&p.position.z>=-110&&p.position.z<=190&&
            !(p.position.x>=-53&&p.position.x<=53&&p.position.z>=-23&&p.position.z<=103),"TERRAIN_PLANTING_SOURCE_DOMAIN");
        for(unsigned j=0;j<i;++j)Need(std::hypot(p.position.x-original[j].position.x,p.position.z-original[j].position.z)>=5.f,"TERRAIN_PLANTING_SOURCE_SPACING");}
    TerrainPlantingPlan out;out.sourcePoseFingerprint=LandscapeSourcePoseFingerprint(original);Need(out.sourcePoseFingerprint==sourcePoseSeal,"TERRAIN_PLANTING_SOURCE_SEAL");
    for(unsigned i=0;i<520;++i)out.instances[i]={i,LandscapeTreeSpecies::Fir,original[i]};
    if(profile==TerrainPlantingProfile::Original){out.complete=true;TerrainPlantingDetail::SealPlan(out);return out;}
    (void)D::Measure(rootedSource);const D::Surface surface(actualLandscape);std::array<bool,520> used{};
    for(unsigned anchorIndex=0;anchorIndex<16;++anchorIndex){const auto a=TerrainPlantingAnchors[anchorIndex];auto& diagnostic=out.diagnostics[anchorIndex];
        const Vec3 probe{a.x,0,a.z};const auto height=surface.At(probe);const float y=static_cast<float>(height.Approximate());
        Transform3D pose{{a.x,y,a.z},{0,a.yaw,0},{a.scale,a.scale,a.scale}};
        diagnostic.rootError=static_cast<float>(D::Abs(height-D::R::Float(y)).Approximate());
        Need(Compare(D::Abs(height-D::R::Float(y)),D::R::Float(.00005f))<=0,"TERRAIN_PLANTING_ROOT_ROUNDING");
        diagnostic.sourceOrdinal=TerrainPlantingDetail::NearestOrdinal(original,used,pose.position);
        if(diagnostic.sourceOrdinal==~0u){diagnostic.result=TerrainPlantingResult::NoNearbySource;}
        else{const auto id=diagnostic.sourceOrdinal;diagnostic.sourceDistance=std::hypot(original[id].position.x-a.x,original[id].position.z-a.z);
            diagnostic.slope=LandscapeNormal(core,a.x,a.z).y;
            if(diagnostic.slope<.97f)diagnostic.result=TerrainPlantingResult::AnalyticSlope;
            else{
                auto derived=TerrainPlantingDetail::RootTrunkToLandscape(rootedSource[2],pose,surface);
                diagnostic.meshFingerprint=TerrainPlantingDetail::MeshFingerprint(derived);
                const auto bounds=TransformBounds(TerrainPlantingDetail::FinalBounds(rootedSource,derived),pose);
                if(!D::OutsideCore(bounds))diagnostic.result=TerrainPlantingResult::CrownCore;
                else{bool spacing{};for(unsigned j=0;j<520;++j)if(j!=id){const auto& p=out.instances[j].render.position;
                        spacing|=std::hypot(a.x-p.x,a.z-p.z)<5.f;}
                    if(spacing)diagnostic.result=TerrainPlantingResult::TreeSpacing;
                    else{bool overlap{};for(const auto& plant:out.plants)overlap|=D::Overlap(bounds,plant.finalBounds);
                        if(overlap)diagnostic.result=TerrainPlantingResult::CrownOverlap;
                        else{diagnostic.ground=D::Ground(pose,derived,surface);
                            if(diagnostic.ground.result==D::GroundResult::Slope)diagnostic.result=TerrainPlantingResult::ExactSlope;
                            else if(diagnostic.ground.result!=D::GroundResult::Safe)diagnostic.result=TerrainPlantingResult::GroundGap;
                            else{diagnostic.result=TerrainPlantingResult::Accepted;used[id]=true;out.instances[id]={id,LandscapeTreeSpecies::Broadleaf,pose};
                                out.plants.push_back({anchorIndex,id,pose,std::move(derived),bounds,diagnostic.meshFingerprint,diagnostic.ground});}}}
                }
            }
        }
        if(diagnostic.result!=TerrainPlantingResult::Accepted&&out.firstMissingAnchor==~0u)out.firstMissingAnchor=anchorIndex;
    }
    out.complete=out.plants.size()==16;TerrainPlantingDetail::SealPlan(out);return out;
}
}
