#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorerSurface.h"
#include "MistwatchLoadPreparation.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <Windows.h>
#include <bcrypt.h>
#include <bit>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
using namespace Aevrynd;
unsigned checks{},controls{};
void Require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool Same(const JointTransform3D& a,const JointTransform3D& b){return Same(a.translation,b.translation)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale)&&
    Bits(a.quaternion.x,b.quaternion.x)&&Bits(a.quaternion.y,b.quaternion.y)&&Bits(a.quaternion.z,b.quaternion.z)&&Bits(a.quaternion.w,b.quaternion.w)&&a.usesQuaternion==b.usesQuaternion;}
void Matrix(const Matrix4& a,const Matrix4& b){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)Require(Bits(a[r][c],b[r][c]),"matrix raw field changed");}
void State(const Mistwatch::Explorer& a,const Mistwatch::Explorer& b){
    Require(Bits(a.phase,b.phase)&&Bits(a.yaw,b.yaw)&&Bits(a.locomotionDistance,b.locomotionDistance)&&Same(a.lastFeet,b.lastFeet)&&
        a.feetInitialized==b.feetInitialized&&a.planted==b.planted,"phase and IK state changed");
    for(unsigned i=0;i<2;++i)Require(Same(a.footTargets[i],b.footTargets[i]),"foot target changed");
    Require(a.pose.localTransforms.size()==b.pose.localTransforms.size()&&a.pose.globalMatrices.size()==b.pose.globalMatrices.size(),"pose population changed");
    for(std::size_t i=0;i<a.pose.localTransforms.size();++i)Require(Same(a.pose.localTransforms[i],b.pose.localTransforms[i]),"local pose raw changed");
    for(std::size_t i=0;i<a.pose.globalMatrices.size();++i)Matrix(a.pose.globalMatrices[i],b.pose.globalMatrices[i]);
}
void Art(const Mistwatch::ExplorerArtReceipt& a,const Mistwatch::ExplorerArtReceipt& b){
    Require(a.skinColorVertices==b.skinColorVertices&&a.criticalVertices==b.criticalVertices&&a.joins.size()==b.joins.size(),"art complete inventories changed");
    for(unsigned i=0;i<5;++i)Require(a.ranges[i].firstIndex==b.ranges[i].firstIndex&&a.ranges[i].indexCount==b.ranges[i].indexCount&&a.ranges[i].material==b.ranges[i].material,"range changed");
    for(unsigned i=0;i<2;++i)Require(a.eyeLensSpans[i].firstIndex==b.eyeLensSpans[i].firstIndex&&a.eyeLensSpans[i].indexCount==b.eyeLensSpans[i].indexCount&&
        a.eyeLensSpans[i].firstVertex==b.eyeLensSpans[i].firstVertex&&a.eyeLensSpans[i].vertexCount==b.eyeLensSpans[i].vertexCount,"lens changed");
    for(std::size_t i=0;i<a.joins.size();++i)Require(a.joins[i].boundary==b.joins[i].boundary&&a.joins[i].joint==b.joins[i].joint,"join changed");
}
void Whole(const Mistwatch::Explorer& a,const Mistwatch::Explorer& b){
    const auto& x=a.model;const auto& y=b.model;
    Require(x.bindMesh.indices==y.bindMesh.indices&&x.bindMesh.vertices.size()==y.bindMesh.vertices.size()&&x.influences.size()==y.influences.size()&&
        x.skeleton.joints.size()==y.skeleton.joints.size()&&x.inverseBindMatrices.size()==y.inverseBindMatrices.size()&&
        x.tangents==y.tangents&&x.skeletonCompatibilityId==y.skeletonCompatibilityId,"whole model populations changed");
    for(std::size_t i=0;i<x.bindMesh.vertices.size();++i){const auto& p=x.bindMesh.vertices[i];const auto& q=y.bindMesh.vertices[i];
        Require(Bits(p.x,q.x)&&Bits(p.y,q.y)&&Bits(p.z,q.z)&&Bits(p.u,q.u)&&Bits(p.v,q.v)&&p.normal==q.normal&&p.color==q.color,"raw mesh changed on reject");}
    for(std::size_t i=0;i<x.influences.size();++i)Require(x.influences[i].joints==y.influences[i].joints&&x.influences[i].weights==y.influences[i].weights,"weights changed on reject");
    for(std::size_t i=0;i<x.skeleton.joints.size();++i)Require(x.skeleton.joints[i].parent==y.skeleton.joints[i].parent&&Same(x.skeleton.joints[i].bindTransform,y.skeleton.joints[i].bindTransform),"rig changed on reject");
    for(std::size_t i=0;i<x.inverseBindMatrices.size();++i)Matrix(x.inverseBindMatrices[i],y.inverseBindMatrices[i]);
    State(a,b);Art(a.art,b.art);
}
std::vector<std::byte> Encode(const SkinnedModel3D& model){std::vector<std::byte> bytes;std::string error;Require(EncodeSkinnedModelAsset(model,bytes,&error),"actual production model encode failed");return bytes;}
std::string Digest(std::vector<std::byte> bytes){
    struct Provider {BCRYPT_ALG_HANDLE value{};~Provider(){if(value)BCryptCloseAlgorithmProvider(value,0);}} provider;
    Require(BCryptOpenAlgorithmProvider(&provider.value,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider failed");
    Require(bytes.size()<=(std::numeric_limits<ULONG>::max)(),"canonical hash length overflow");std::array<UCHAR,32> digest{};
    Require(BCryptHash(provider.value,nullptr,0,reinterpret_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),digest.data(),static_cast<ULONG>(digest.size()))>=0,"actual canonical SHA256 failed");
    std::ostringstream text;text<<std::uppercase<<std::hex<<std::setfill('0');for(auto byte:digest)text<<std::setw(2)<<unsigned(byte);return text.str();
}
template<class Change>void Reject(const Mistwatch::Explorer& original,Change change){
    auto bad=original;change(bad);const auto before=bad;bool rejected=false;
    try{Mistwatch::ExplorerNapeHairBinding::Apply(bad);}catch(const std::runtime_error&){rejected=true;}
    Require(rejected,"invalid source accepted at production Apply boundary");Whole(before,bad);++controls;
}
void Run(){
    Mistwatch::Explorer explorer;explorer.Update({.03f,0,.01f},1.f/60.f,true);
    explorer.footTargets={Vec3{-.1f,.2f,.3f},Vec3{.1f,.2f,.3f}};explorer.planted={true,false};explorer.lastFeet={1,0,2};explorer.feetInitialized=true;
    const auto original=explorer;const auto originalBytes=Encode(original.model);
    Require(originalBytes.size()==474644&&Digest(originalBytes)=="36C68A5897AE840FACB13F6CC4DC3AEE519707ABA8B5CC92D403785F833B35C7","original whole canonical differs");
    Mistwatch::ExplorerNapeHairBinding::Apply(explorer);State(original,explorer);
    const auto candidate=Encode(explorer.model);
    Require(candidate.size()==554724&&Digest(candidate)=="567A4CF59889C6E04446444A3A5C86889F3407931234F1702B852AA6FB89A1B7","production Apply whole canonical differs from independently verified clean recipe");
    Require(explorer.model.bindMesh.vertices.size()==9225&&explorer.model.bindMesh.indices.size()==36792&&explorer.model.skeleton.joints.size()==15,"new geometry population");
    Mistwatch::ExplorerSurface::Plan plan;Require(Mistwatch::ExplorerSurface::BuildPlan(explorer.model,explorer.art,plan),"actual applied nine-window plan failed");
    unsigned cursor{},eyes{};for(const auto& w:plan.windows){Require(w.firstIndex==cursor&&w.indexCount>0,"ordered complete windows");cursor+=w.indexCount;eyes+=w.eye;}
    Require(cursor==36792&&eyes==2&&plan.windows.size()==9,"original nine-window and two-eye route changed");
    Reject(original,[](auto& e){e.model.bindMesh.vertices[0].x+=.01f;});
    Reject(original,[](auto& e){++e.art.ranges[0].firstIndex;});
    Reject(original,[](auto& e){e.model.skeleton.joints[0].parent=1;});
    Reject(original,[](auto& e){e.model.skeletonCompatibilityId.value^=1u;});
    Reject(original,[](auto& e){e.pose.localTransforms[0].translation.x=std::numeric_limits<float>::quiet_NaN();});
    Reject(original,[](auto& e){e.pose.globalMatrices[0][0][0]=std::numeric_limits<float>::infinity();});
    Reject(original,[](auto& e){e.pose.localTransforms.pop_back();});
    Reject(original,[](auto& e){e.model.bindMesh.vertices.pop_back();});
    Reject(explorer,[](auto&){});
    // 真世界預檢及swap/rollback，不能只測default Explorer或單純copy。
    Mistwatch::SaveGame game,restored;const auto saved=game.Serialize();Require(!saved.empty()&&restored.Restore(saved),"save/restore actual snapshot failed");
    CharacterController3D controller;Mistwatch::Boar boar;Mistwatch::OrbitCamera orbit{-.65f,.3f,6};const auto terrain=Mistwatch::CreateTerrain();
    std::vector<BoxCollider3D> solids=Mistwatch::WorldSolids(false);ThirdPersonCameraMotion3D motion;ThirdPersonFramingResult3D framing;
    ThirdPersonCameraMotionResult3D cameraResult;Camera3D rendered;
    auto prepared=Mistwatch::PrepareSavedGame(restored,{controller,explorer,boar,orbit},terrain);
    Require(bool(prepared)&&Encode(prepared.candidate->explorer.model)==candidate,"applied model lost during actual load preparation");Art(prepared.candidate->explorer.art,explorer.art);
    const auto beforeCommit=explorer;Mistwatch::SavedGameCommitTargets targets{game.session,game.encounter,game.evasion,controller,explorer,boar,orbit,solids,motion,framing,cameraResult,rendered};
    Mistwatch::SwapPreparedSavedGame(*prepared.candidate,targets);Require(Encode(explorer.model)==candidate,"applied model lost on commit");Art(explorer.art,beforeCommit.art);
    Mistwatch::SwapPreparedSavedGame(*prepared.candidate,targets);Whole(beforeCommit,explorer);
    auto invalid=restored;invalid.session.feet.y=-1;
    const auto beforeReject=explorer;const auto failed=Mistwatch::PrepareSavedGame(invalid,{controller,explorer,boar,orbit},terrain);
    Require(!failed,"unsafe load accepted");Whole(beforeReject,explorer);++controls;
    std::cout<<"NAPE_HAIR_GAME_CPU_PASS checks="<<checks<<" controls="<<controls<<" records=9225 triangles=12264 canonical_exact=1 windows=9 load_prepare_commit_rollback=1 native=0 art=0\n";
}
}
int main(){try{Run();return 0;}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
