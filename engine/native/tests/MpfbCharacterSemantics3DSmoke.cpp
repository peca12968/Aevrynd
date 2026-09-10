#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/SkeletonSemantics3D.h>
#include <Aevrynd/CharacterPresentation3D.h>
#include <array>
#include <iostream>

using namespace Aevrynd;
int main(int argc,char** argv){
    if(argc<2)return 2;
    auto loaded=LoadSkinnedModelAssetFileDetailed(argv[1]);
    if(!loaded){std::cerr<<"asset load failed\n";return 3;}
    auto& a=loaded.asset;HumanoidSkeletonMap3D map;SkeletonSemanticError3D error{};
    if(!ResolveCharacterRig3D(a,map,&error)){std::cerr<<"MPFB resolve error="<<int(error)<<'\n';return 10;}
    // 實際 candidate7 的完整必要語意；不能只測試任意人形骨數。
    constexpr std::array<const char*,16> expected{"Root","pelvis","spine_01","head","thigh_l","calf_l","foot_l","thigh_r","calf_r","foot_r","upperarm_l","lowerarm_l","hand_l","upperarm_r","lowerarm_r","hand_r"};
    if(a.jointNames.size()!=53)return 11;
    for(std::size_t i=0;i<expected.size();++i)if(a.jointNames[map.joints[i]]!=expected[i])return 12;
    const auto fails=[&](auto bad,SkeletonSemanticError3D reason,bool refresh){
        if(refresh)bad.model.skeletonCompatibilityId=ComputeSkeletonCompatibilityId3D(bad.jointNames,bad.model.skeleton,bad.model.inverseBindMatrices);
        auto output=map;SkeletonSemanticError3D e{};
        const bool ok=ResolveHumanoidSkeletonMap3D(bad.jointNames,bad.model,bad.model.skeletonCompatibilityId,output,&e);
        return !ok&&e==reason&&output.joints==map.joints&&output.compatibility==map.compatibility;
    };
    auto bad=a;bad.jointNames[map.Joint(HumanoidJoint3D::Spine)]="spine_04";
    if(!fails(bad,SkeletonSemanticError3D::MissingJoint,true))return 20;
    bad=a;bad.jointNames[3]="spine";
    if(!fails(bad,SkeletonSemanticError3D::DuplicateJoint,true))return 21;
    bad=a;bad.jointNames[map.Joint(HumanoidJoint3D::LeftHand)]="hand_unknown";
    if(!fails(bad,SkeletonSemanticError3D::CompatibilityMismatch,false))return 22;
    bad=a;std::swap(bad.jointNames[map.Joint(HumanoidJoint3D::LeftHand)],bad.jointNames[map.Joint(HumanoidJoint3D::RightHand)]);
    if(!fails(bad,SkeletonSemanticError3D::InvalidTopology,true))return 23;
    // 別名擴充必須仍接受原本命名契約。
    constexpr std::array<const char*,16> legacy{"Root","Hips","Spine","Head","LeftUpperLeg","LeftLowerLeg","LeftFoot","RightUpperLeg","RightLowerLeg","RightFoot","LeftUpperArm","LeftLowerArm","LeftHand","RightUpperArm","RightLowerArm","RightHand"};
    bad=a;for(std::size_t i=0;i<legacy.size();++i)bad.jointNames[map.joints[i]]=legacy[i];
    bad.model.skeletonCompatibilityId=ComputeSkeletonCompatibilityId3D(bad.jointNames,bad.model.skeleton,bad.model.inverseBindMatrices);
    HumanoidSkeletonMap3D legacyMap;
    if(!ResolveCharacterRig3D(bad,legacyMap)||legacyMap.joints!=map.joints)return 24;
    FallbackHumanoidAnimationSet3D clips;std::string detail;
    if(!CreateFallbackHumanoidAnimationSet3D(a,map,clips,&detail)){std::cerr<<detail<<'\n';return 30;}
    for(const auto& clip:clips.clips)if(!clip.Valid(a.model.skeleton))return 31;
    // 正式 AESKIN loader 驗證每個附件，而非只相信 glTF 或清單自報同骨架。
    for(int i=2;i<argc;++i){
        auto part=LoadSkinnedModelAssetFileDetailed(argv[i]);
        if(!part||part.asset.jointNames!=a.jointNames||part.asset.model.skeletonCompatibilityId!=a.model.skeletonCompatibilityId){
            std::cerr<<"component skeleton mismatch index="<<i-1<<'\n';return 40;
        }
        HumanoidSkeletonMap3D partMap;
        if(!ResolveCharacterRig3D(part.asset,partMap)||partMap.joints!=map.joints)return 41;
        std::cout<<"COMPONENT="<<i-1<<" skeletonIdentity=PASS submeshes="<<part.asset.submeshes.size()<<'\n';
    }
    std::cout<<"MPFB_SEMANTICS=PASS joints=53 semantic=16 hostile=4 legacy=PASS fallbackClips="<<clips.clips.size()<<" scope=semantic-and-clip-validity-not-posed-mesh-quality\n";
}
