#define Explorer LegacyExplorer
#include "MistwatchExplorerBaseline.h"
#undef Explorer
#include "MistwatchExplorer.h"
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchSaveGame.h"
#include "MistwatchWorld.h"
#include "MistwatchExplorerNormalDiagnostic.h"
#include <bit>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using namespace Aevrynd;
unsigned checks{};
void Require(bool value,const std::string& name){++checks;if(!value)throw std::runtime_error(name);}
Vec3 Position(const Vertex3D& v){return {v.x,v.y,v.z};}
Vec3 Normal(const Vertex3D& v){const auto n=UnpackNormalSnorm8(v.normal);return {n.x,n.y,n.z};}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void SameMatrix(const Matrix4& a,const Matrix4& b,const char* name){for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)Require(SameFloat(a[i][j],b[i][j]),name);}
void CheckRig(const Mistwatch::Explorer& explorer,const Mistwatch::LegacyExplorer& baseline) {
    Require(explorer.model.skeleton.joints.size()==15,"15-joint contract");
    Require(explorer.model.skeletonCompatibilityId==baseline.model.skeletonCompatibilityId,"structural skeleton compatibility changed");
    for(unsigned i=0;i<15;++i){const auto& a=explorer.model.skeleton.joints[i];const auto& b=baseline.model.skeleton.joints[i];
        Require(a.parent==b.parent,"joint parent changed");SameMatrix(a.bindTransform.Matrix(),b.bindTransform.Matrix(),"bind transform changed");
        SameMatrix(explorer.model.inverseBindMatrices[i],baseline.model.inverseBindMatrices[i],"inverse bind changed");}
}
struct EdgeUse {unsigned count{},forward{},reverse{};};
using Edge=std::array<std::uint32_t,2>;
std::map<Edge,EdgeUse> Edges(const Mesh3D& mesh) {
    std::map<Edge,EdgeUse> result;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1u],c=mesh.indices[i+2u];
        for(auto edge:std::array<Edge,3>{Edge{a,b},Edge{b,c},Edge{c,a}}){const bool forward=edge[0]<edge[1];if(!forward)std::swap(edge[0],edge[1]);
            auto& use=result[edge];++use.count;if(forward)++use.forward;else ++use.reverse;}}
    return result;
}
void MeshContract(const Mistwatch::Explorer& explorer) {
    const auto& model=explorer.model;const auto& mesh=model.bindMesh;
    Require(model.Valid(),"existing model.Valid rejects candidate");
    Require(mesh.vertices.size()<10000&&mesh.indices.size()/3u<12000,"authored mesh budget");
    unsigned blended{},threeWay{};
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];const auto& skin=model.influences[i];
        Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"nonfinite authored vertex");
        Require(Length(Normal(v))>.98f&&Length(Normal(v))<1.02f,"packed normal length");
        std::uint32_t sum{};unsigned active{};for(unsigned slot=0;slot<4;++slot){sum+=skin.weights[slot];active+=skin.weights[slot]>0;
            Require(!skin.weights[slot]||skin.joints[slot]<15,"joint index");
            Require(skin.weights[slot]||!skin.joints[slot],"zero weight has nonzero joint");
            if(slot)Require(skin.weights[slot]<=skin.weights[slot-1u],"influence ordering");}
        Require(sum==65535,"exact influence sum");blended+=active>1;threeWay+=active>2;
    }
    Require(blended>300&&threeWay>=3,"blended shoulder/hip transitions absent");
    float minimumNormalDot=1;unsigned negativeNormals{};
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1u],c=mesh.indices[i+2u];
        Require(a<mesh.vertices.size()&&b<mesh.vertices.size()&&c<mesh.vertices.size(),"index range");
        const auto face=Cross(Position(mesh.vertices[b])-Position(mesh.vertices[a]),Position(mesh.vertices[c])-Position(mesh.vertices[a]));
        Require(Dot(face,face)>1.e-16f,"degenerate authored face");
        const float direction=Dot(face,Normal(mesh.vertices[a])+Normal(mesh.vertices[b])+Normal(mesh.vertices[c]));
        minimumNormalDot=(std::min)(minimumNormalDot,direction);negativeNormals+=direction<=0;
    }
    std::cout<<"bind minimum_normal_dot="<<minimumNormalDot<<" reversed_normal_faces="<<negativeNormals<<'\n';
    Mistwatch::ExplorerArtDiagnostic::PrintNegativeFaces(explorer);
    Require(negativeNormals==0,"face / smooth normal orientation diverges");
    const auto edges=Edges(mesh);unsigned joinEdges{};
    for(const auto& join:explorer.art.joins){Require(join.boundary.size()>=12,"joint ring inventory incomplete");
        for(std::size_t i=0;i<join.boundary.size();++i){Edge edge{join.boundary[i],join.boundary[(i+1u)%join.boundary.size()]};
            if(edge[0]>edge[1])std::swap(edge[0],edge[1]);const auto found=edges.find(edge);
            Require(found!=edges.end(),"joint ring missing edge");
            Require(found->second.count==2,"joint ring open/nonmanifold at joint "+std::to_string(join.joint));
            Require(found->second.forward==1&&found->second.reverse==1,"joint ring winding mismatch at joint "+std::to_string(join.joint));++joinEdges;}
    }
    Require(explorer.art.joins.size()==14,"coat/hip/elbow/wrist/knee/ankle joins missing");
    const auto bounds=mesh.Bounds();
    Require(bounds.center.y-bounds.extents.y>=0&&bounds.center.y+bounds.extents.y<1.82f,"adult standing height / sole bounds");
    Require(bounds.extents.x<.40f&&bounds.extents.x>.30f&&bounds.extents.z<.32f,"body silhouette bounds");
    std::uint32_t cursor{};for(unsigned group=0;group<explorer.art.ranges.size();++group){const auto& range=explorer.art.ranges[group];
        Require(range.firstIndex==cursor&&range.indexCount>0&&range.indexCount%3u==0,"material range coverage");cursor+=range.indexCount;}
    Require(cursor==mesh.indices.size(),"material ranges do not cover full mesh");
    const auto materials=Mistwatch::ExplorerArtMaterials();
    Require(materials[0].surface.roughness>materials[4].surface.roughness&&materials[4].surface.metallic>0,"cloth versus metal PBR separation");
    Require(materials[2].shadingModel==CharacterShadingModel3D::Skin&&materials[3].shadingModel==CharacterShadingModel3D::Hair,"existing character shading routed");
    std::vector<SkinnedVertex3D> gpu;
    Require(BuildGpuSkinnedVertices(model,gpu)&&gpu.size()==mesh.vertices.size(),"existing GPU upload preparation rejects mesh");
    std::cout<<"author vertices="<<mesh.vertices.size()<<" triangles="<<mesh.indices.size()/3u<<" blended="<<blended<<" threeway="<<threeWay<<" verified_join_edges="<<joinEdges<<'\n';
}
void PoseContract(Mistwatch::Explorer& explorer,Mistwatch::LegacyExplorer& baseline) {
    const auto terrain=Mistwatch::CreateTerrain();const auto solids=Mistwatch::WorldSolids(true);
    SkinningWorkspace3D workspace,legacyWorkspace;Mesh3D posed,legacyPosed;
    Mistwatch::SaveGame save;save.evasion.Tick(.02f,true,true,false,{1,0,0});
    const auto saved=save.Serialize();Require(!saved.empty(),"canonical active evasion snapshot");
    CharacterController3D actor;actor.Reset({0,0,36});
    Mistwatch::PlayerLocomotion owner;Mistwatch::Evasion evasion;
    Require(actor.Step({},solids,{},terrain,Mistwatch::LocomotionMotor::FixedStep)&&actor.Grounded(),"actual controller initial support");
    explorer.Update({},0,false);
    Require(explorer.PlaceFeet(actor.Position(),actor.Grounded(),false,terrain,solids),"actual initial pose");
    unsigned posedCriticalVertices{},lifted{},locked{};
    float maxFootError{},maxBoneError{},maxJointRingStretch=0;
    std::array<Vec3,2> priorTargets{};std::array<bool,2> priorPlanted{};
    for(unsigned action=0;action<3;++action){const bool crouched=action==2;const unsigned ticks=action==1?240u:1u;
        for(unsigned tick=0;tick<ticks;++tick){
            Mistwatch::PlayerLocomotionInput input;input.crouched=crouched;input.speedMultiplier=.25f;
            if(action==1&&tick<180)input.movement={0,0,1};
            const auto result=owner.Advance(actor,evasion,explorer,input,1.f/60.f,solids,{},terrain,[]{return true;});
            if(!result.accepted)std::cerr<<"POSE_OWNER action="<<action<<" tick="<<tick<<" line="<<result.poseFailureLine<<'\n';
            Require(result.accepted&&actor.Grounded(),"actual owner rejected supported action");
            const auto feet=actor.Position();
            Require(SkinModelInto(explorer.model,explorer.pose,workspace,posed),"existing CPU skinning rejected action");
            // 舊網格共用正式姿態驗證相容蒙皮；不拿舊瞬轉、固定壓低骨盆當新步態答案。
            Require(SkinModelInto(baseline.model,explorer.pose,legacyWorkspace,legacyPosed),"legacy rig cannot skin current owner pose");
            for(unsigned joint=0;joint<15;++joint){
                const int parent=explorer.model.skeleton.joints[joint].parent;
                if(parent>=0){const auto p=static_cast<unsigned>(parent);
                    const float actual=Length(TransformPoint(explorer.pose.globalMatrices[joint],{})-TransformPoint(explorer.pose.globalMatrices[p],{}));
                    const float expected=Length(explorer.model.skeleton.joints[joint].bindTransform.translation);
                    // 骨盆下移只改根節點；每根四肢骨骼仍須保持真實長度。
                    const float error=std::abs(actual-expected);maxBoneError=std::max(maxBoneError,error);
                    Require(error<.0001f,"owner pose changed bone length");}
                Require(!explorer.art.criticalVertices[joint].empty(),"critical joint vertex inventory empty");
                for(const auto index:explorer.art.criticalVertices[joint]){const auto p=Position(posed.vertices.at(index));
                    Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"critical posed vertex nonfinite");
                    Require(Length(Normal(posed.vertices[index]))>.97f,"critical posed normal cancelled");++posedCriticalVertices;}}
            for(const auto& join:explorer.art.joins)for(std::size_t i=0;i<join.boundary.size();++i){const auto a=join.boundary[i],b=join.boundary[(i+1u)%join.boundary.size()];
                const float bindLength=Length(Position(explorer.model.bindMesh.vertices[a])-Position(explorer.model.bindMesh.vertices[b]));
                const float posedLength=Length(Position(posed.vertices[a])-Position(posed.vertices[b]));
                Require(bindLength>0&&posedLength>0,"joint ring collapsed edge");
                maxJointRingStretch=(std::max)(maxJointRingStretch,posedLength/bindLength);
                Require(posedLength/bindLength<2.f,"joint ring unexpectedly stretched beyond 2x");}
            const auto world=CreateTransformMatrix(feet,{0,explorer.yaw,0},{1,1,1});
            for(unsigned side=0;side<2;++side){const auto ankle=TransformPoint(world*explorer.pose.globalMatrices[11u+side*3u],{});
                maxFootError=(std::max)(maxFootError,Length(ankle-explorer.footTargets[side]));
                if(action==1){lifted+=!explorer.planted[side]&&ankle.y>.16f;
                    locked+=explorer.planted[side]&&priorPlanted[side]&&Length(explorer.footTargets[side]-priorTargets[side])<.001f;}
                priorTargets[side]=explorer.footTargets[side];priorPlanted[side]=explorer.planted[side];}
            const auto bounds=posed.Bounds();Require(bounds.center.y-bounds.extents.y>=-.02f,"posed sole penetrates ground");
            Require(bounds.center.y+bounds.extents.y<(crouched?1.40f:1.83f),"posed height outside original standing/crouch envelope");
            Require(save.Serialize()==saved,"render/pose changed save state");
        }
    }
    Require(maxBoneError<.0001f&&maxFootError<.003f&&locked>50&&lifted>20,"original IK safety and actual motion invariants");
    Mistwatch::SaveGame restored;Require(restored.Restore(saved)&&restored.Serialize()==saved,"SaveGame roundtrip invariant");
    Require(!restored.Restore(saved+" invalid")&&restored.Serialize()==saved,"SaveGame transactional rejection invariant");
    std::cout<<"stand_walk_crouch critical_samples="<<posedCriticalVertices<<" locked="<<locked<<" lifted="<<lifted
        <<" max_foot_error="<<maxFootError<<" max_bone_error="<<maxBoneError<<" max_ring_stretch="<<maxJointRingStretch<<'\n';
}
}
int main(){try{Mistwatch::Explorer explorer;Mistwatch::LegacyExplorer baseline;CheckRig(explorer,baseline);MeshContract(explorer);PoseContract(explorer,baseline);
    std::cout<<"ExplorerArtCpuTest PASS checks="<<checks<<" native_image_tested=0 photorealism_claimed=0\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
