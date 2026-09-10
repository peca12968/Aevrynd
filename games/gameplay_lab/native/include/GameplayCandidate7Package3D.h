#pragma once
#include <Aevrynd/CharacterPresentation3D.h>
#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>
#include <Aevrynd/SkinnedMaterialSidecar3D.h>
#include <array>
#include <Aevrynd/FootPlacement3D.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace GameplayLab {
struct Candidate7Component3D {
    Aevrynd::SkinnedModelAssetData asset;
    Aevrynd::SkinnedMaterialSidecarResult3D sidecar;
    std::filesystem::path materialRoot;
};
struct Candidate7Package3D {std::array<Candidate7Component3D,4> components;};
// MPFB 上／前臂約 253／235mm；肩與握點成對平移，保留可達距離並讓真實 56cm 劍身
// 位於正式第一人稱構圖深度，不能縮短武器或只移動手腕掩蓋比例問題。
inline constexpr Aevrynd::Vec3 Candidate7ShoulderCenter3D{.23f,-.18f,.30f};
inline constexpr Aevrynd::Vec3 Candidate7ReadyGrip3D{.38f,-.05f,.52f};

// 專用 bind 校正左乘原局部旋轉；保留動畫相對旋轉，不重寫通用動畫或骨長。
inline void ApplyCandidate7ArmRest3D(const Aevrynd::SkinnedModel3D& model,
    const Aevrynd::HumanoidSkeletonMap3D& map,Aevrynd::AnimationPose3D& pose){
    using namespace Aevrynd;
    auto fail=[](){throw std::runtime_error("Candidate7 arm rest contract failed");};
    if(model.skeleton.joints.size()!=53||!map.Valid(model.skeleton,model.skeletonCompatibilityId)||
       pose.localTransforms.size()!=53||pose.globalMatrices.size()!=53)fail();
    AnimationPose3D bind;for(const auto& j:model.skeleton.joints)bind.localTransforms.push_back(j.bindTransform);
    bind.globalMatrices.resize(53);BuildAnimationPoseGlobalMatrices3D(model.skeleton,bind);
    auto finite=[](const Matrix4& m){for(const auto& r:m.values)for(float v:r)if(!std::isfinite(v))return false;return true;};
    for(std::size_t i=0;i<53;++i)if(!finite(bind.globalMatrices[i])||!finite(pose.localTransforms[i].Matrix())||!finite(pose.globalMatrices[i]))fail();
    const HumanoidJoint3D uppers[]{HumanoidJoint3D::LeftUpperArm,HumanoidJoint3D::RightUpperArm};
    const HumanoidJoint3D lowers[]{HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::RightLowerArm};
    if(TransformPoint(bind.globalMatrices[map.Joint(uppers[0])],{}).x<=TransformPoint(bind.globalMatrices[map.Joint(uppers[1])],{}).x)fail();
    auto result=pose;
    for(unsigned side=0;side<2;++side){
        const auto upper=map.Joint(uppers[side]),lower=map.Joint(lowers[side]);
        const int parent=model.skeleton.joints[upper].parent;
        if(parent<0||model.skeleton.joints[lower].parent!=int(upper))fail();
        const auto& p=bind.globalMatrices[std::size_t(parent)];
        Vec3 columns[3]{{p[0][0],p[1][0],p[2][0]},{p[0][1],p[1][1],p[2][1]},{p[0][2],p[1][2],p[2][2]}};
        for(auto c:columns)if(std::abs(Length(c)-1)>1e-4f)fail();
        if(std::abs(Dot(columns[0],columns[1]))>1e-4f||std::abs(Dot(columns[1],columns[2]))>1e-4f||std::abs(Dot(columns[2],columns[0]))>1e-4f||Dot(Cross(columns[0],columns[1]),columns[2])<.9999f)fail();
        Vec3 source=TransformPoint(bind.globalMatrices[lower],{})-TransformPoint(bind.globalMatrices[upper],{});
        if(Length(source)<.1f||Length(source)>.5f)fail();source=Normalize(source);
        Vec3 horizontal{source.x,0,source.z};if(Length(horizontal)<.1f||source.y>=0)fail();horizontal=Normalize(horizontal);
        constexpr float angle=12.f*3.14159265359f/180.f;
        const Vec3 target=horizontal*std::sin(angle)+Vec3{0,-std::cos(angle),0};
        const Vec3 worldAxis=Cross(source,target);
        const Vec3 localAxis{Dot(columns[0],worldAxis),Dot(columns[1],worldAxis),Dot(columns[2],worldAxis)};
        const auto correction=NormalizeQuaternion3D({localAxis.x,localAxis.y,localAxis.z,1+Dot(source,target)});
        auto& t=result.localTransforms[upper];const auto q=t.usesQuaternion?t.quaternion:QuaternionFromEuler3D(t.rotation);
        t.quaternion=NormalizeQuaternion3D({correction.w*q.x+correction.x*q.w+correction.y*q.z-correction.z*q.y,
            correction.w*q.y-correction.x*q.z+correction.y*q.w+correction.z*q.x,
            correction.w*q.z+correction.x*q.y-correction.y*q.x+correction.z*q.w,
            correction.w*q.w-correction.x*q.x-correction.y*q.y-correction.z*q.z});t.usesQuaternion=true;
    }
    BuildAnimationPoseGlobalMatrices3D(model.skeleton,result);
    for(const auto& m:result.globalMatrices)if(!finite(m))fail();
    pose=std::move(result);
}

// 僅供已釘選的 MPFB 53 骨架；以 runtime bind 和鞋底頂點推導，不沿用舊角色局部軸。
inline Aevrynd::FootPlacementSettings3D BuildCandidate7FootProfile3D(
    const Aevrynd::SkinnedModel3D& body,const Aevrynd::SkinnedModel3D& lower,
    const Aevrynd::HumanoidSkeletonMap3D& map){
    using namespace Aevrynd;
    auto fail=[](){throw std::runtime_error("Candidate7 foot bind/profile contract failed");};
    if(!body.Valid()||!lower.Valid()||body.skeleton.joints.size()!=53||
       body.skeletonCompatibilityId!=lower.skeletonCompatibilityId||!map.Valid(body.skeleton,body.skeletonCompatibilityId))fail();
    AnimationPose3D bind;
    for(const auto& joint:body.skeleton.joints)bind.localTransforms.push_back(joint.bindTransform);
    bind.globalMatrices.resize(53);BuildAnimationPoseGlobalMatrices3D(body.skeleton,bind);
    FootPlacementSettings3D settings;
    const HumanoidJoint3D feet[]{HumanoidJoint3D::LeftFoot,HumanoidJoint3D::RightFoot};
    const HumanoidJoint3D calves[]{HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::RightLowerLeg};
    for(unsigned side=0;side<2;++side){
        const std::size_t foot=map.Joint(feet[side]),ball=foot+1;
        if(foot!=(side?51u:47u)||body.skeleton.joints[foot].parent!=int(map.Joint(calves[side]))||body.skeleton.joints[ball].parent!=int(foot))fail();
        const auto& global=bind.globalMatrices[foot];const auto& inverse=body.inverseBindMatrices[foot];
        const auto identity=global*inverse;
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            if(!std::isfinite(identity[r][c])||std::abs(identity[r][c]-(r==c?1.f:0.f))>1e-4f)fail();
        auto direction=[](const Matrix4& m,Vec3 v){auto q=m*Vec4{v.x,v.y,v.z,0};return Vec3{q.x,q.y,q.z};};
        // 不接受縮放、剪切或鏡像 bind，避免正規化掩蓋退化輸入。
        const auto x=direction(global,{1,0,0}),y=direction(global,{0,1,0}),z=direction(global,{0,0,1});
        if(std::abs(Length(x)-1)>1e-4f||std::abs(Length(y)-1)>1e-4f||std::abs(Length(z)-1)>1e-4f||
           std::abs(Dot(x,y))>1e-4f||std::abs(Dot(y,z))>1e-4f||std::abs(Dot(z,x))>1e-4f||Dot(Cross(x,y),z)<.9999f)fail();
        const Vec3 origin=TransformPoint(global,{});
        Vec3 forward=TransformPoint(bind.globalMatrices[ball],{})-origin;forward.y=0;
        if(!std::isfinite(Length(forward))||Length(forward)<.01f)fail();
        forward=Normalize(forward);
        auto& profile=settings.feet[side];profile.soleUpLocal=Normalize(direction(inverse,{0,1,0}));
        profile.soleForwardLocal=Normalize(direction(inverse,forward));
        float sole=std::numeric_limits<float>::infinity();unsigned count=0;
        for(std::size_t i=0;i<lower.bindMesh.vertices.size();++i){
            unsigned weight=0;for(unsigned k=0;k<4;++k)if(lower.influences[i].joints[k]==foot||lower.influences[i].joints[k]==ball)weight+=lower.influences[i].weights[k];
            if(weight<32768)continue;
            const auto& v=lower.bindMesh.vertices[i];
            if(!std::isfinite(v.y)||std::abs(v.x-origin.x)>.20f||std::abs(v.z-origin.z)>.40f)fail();
            sole=(std::min)(sole,v.y);++count;
        }
        profile.soleOffset=origin.y-sole;
        if(count<8||!std::isfinite(profile.soleOffset)||profile.soleOffset<.02f||profile.soleOffset>.15f)fail();
    }
    FootPlacement3D validator;if(!validator.Configure(settings))fail();
    return settings;
}

// 舊 M2 全身資產同樣必須由實際 bind 與鞋底頂點校正；預設 +Y/+Z 與 8 cm
// 只適用測試骨架，不能作為匯入資產的足底契約。
inline Aevrynd::FootPlacementSettings3D BuildSkinnedAssetFootProfile3D(
    const Aevrynd::SkinnedModel3D& body,const Aevrynd::SkinnedModel3D& soleMesh,
    const Aevrynd::HumanoidSkeletonMap3D& map){
    using namespace Aevrynd;
    auto fail=[](){throw std::runtime_error("Skinned asset foot bind/profile contract failed");};
    if(!body.Valid()||!soleMesh.Valid()||body.skeletonCompatibilityId!=soleMesh.skeletonCompatibilityId||
       !map.Valid(body.skeleton,body.skeletonCompatibilityId)||
       soleMesh.bindMesh.vertices.size()!=soleMesh.influences.size())fail();
    AnimationPose3D bind;
    for(const auto& joint:body.skeleton.joints)bind.localTransforms.push_back(joint.bindTransform);
    bind.globalMatrices.resize(body.skeleton.joints.size());BuildAnimationPoseGlobalMatrices3D(body.skeleton,bind);
    FootPlacementSettings3D settings;
    const HumanoidJoint3D feet[]{HumanoidJoint3D::LeftFoot,HumanoidJoint3D::RightFoot};
    const HumanoidJoint3D calves[]{HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::RightLowerLeg};
    for(unsigned side=0;side<2;++side){
        const std::size_t foot=map.Joint(feet[side]);
        if(foot>=body.skeleton.joints.size()||body.skeleton.joints[foot].parent!=int(map.Joint(calves[side])))fail();
        std::size_t ball=body.skeleton.joints.size();
        for(std::size_t joint=0;joint<body.skeleton.joints.size();++joint)if(body.skeleton.joints[joint].parent==int(foot)){if(ball!=body.skeleton.joints.size())fail();ball=joint;}
        if(ball==body.skeleton.joints.size())fail();
        const auto& global=bind.globalMatrices[foot];const auto& inverse=body.inverseBindMatrices[foot];
        const auto identity=global*inverse;
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            if(!std::isfinite(identity[r][c])||std::abs(identity[r][c]-(r==c?1.f:0.f))>1e-4f)fail();
        auto direction=[](const Matrix4& m,Vec3 v){auto q=m*Vec4{v.x,v.y,v.z,0};return Vec3{q.x,q.y,q.z};};
        const auto x=direction(global,{1,0,0}),y=direction(global,{0,1,0}),z=direction(global,{0,0,1});
        if(std::abs(Length(x)-1)>1e-4f||std::abs(Length(y)-1)>1e-4f||std::abs(Length(z)-1)>1e-4f||
           std::abs(Dot(x,y))>1e-4f||std::abs(Dot(y,z))>1e-4f||std::abs(Dot(z,x))>1e-4f||Dot(Cross(x,y),z)<.9999f)fail();
        const Vec3 origin=TransformPoint(global,{});
        Vec3 forward=TransformPoint(bind.globalMatrices[ball],{})-origin;forward.y=0;
        if(!std::isfinite(Length(forward))||Length(forward)<.01f)fail();forward=Normalize(forward);
        auto& profile=settings.feet[side];profile.soleUpLocal=Normalize(direction(inverse,{0,1,0}));profile.soleForwardLocal=Normalize(direction(inverse,forward));
        float sole=std::numeric_limits<float>::infinity();unsigned count=0;
        for(std::size_t vertex=0;vertex<soleMesh.bindMesh.vertices.size();++vertex){
            unsigned weight=0;for(unsigned component=0;component<4;++component)if(soleMesh.influences[vertex].joints[component]==foot||soleMesh.influences[vertex].joints[component]==ball)weight+=soleMesh.influences[vertex].weights[component];
            if(weight<32768)continue;const auto& position=soleMesh.bindMesh.vertices[vertex];
            if(!std::isfinite(position.y)||std::abs(position.x-origin.x)>.20f||std::abs(position.z-origin.z)>.40f)fail();
            sole=(std::min)(sole,position.y);++count;
        }
        profile.soleOffset=origin.y-sole;
        if(count<8||!std::isfinite(profile.soleOffset)||profile.soleOffset<.02f||profile.soleOffset>.18f)fail();
    }
    FootPlacement3D validator;if(!validator.Configure(settings))fail();return settings;
}

// 專用固定版本，不接受任意模型冒充 candidate7；舊九材質路徑完全獨立。
template<class Verify> Candidate7Package3D LoadCandidate7Package3D(const std::filesystem::path& root,Verify verify){
    constexpr std::array<const char*,4> assets{"face-detail-59/candidate7/body.aeskin","garment-shell-followup-94/bounded-shell/upper.aeskin","character-split-lower-17/character.aeskin","hair-backfill-92/combined.aeskin"};
    constexpr std::array<const char*,4> sidecars{"face-detail-59/body-materials.json","face-detail-59/upper-iris-readable.json","face-detail-59/upper-materials.json","hair-backfill-92/combined.materials.json"};
    constexpr std::array<const char*,4> assetSha{"70a52801adea19c8342de36b914ac8c12a8487a30069e666d970acf97ba6e141","34a04c6906662563e89cc92fed715f8c1b74f631768cb0c73e4070d51ae0a316","33753b00c845712aebcac7efb14c794c215320d4e7d0427d96750d5aea60707d","24f6ff72aaf36cfe1bdd525eae58a7a795be51328e2b5c7bac231c570103ef9c"};
    constexpr std::array<const char*,4> materialSha{"913bbba1710b0381307c98c14ad69355f405f0bb0e596d437d458287ce74683a","753257d585bb04408ad4974a746a2984f6a0c20ca9e18e6c445bd771921bee07","46ca7c640bec1dd7fac9ceca782ddc47430bf6a6c63b810cffcc1e27872dffdb","f78ce3cb8927c5f0a87c68b9ce13056e4a981789153837a567460c8316ec6a43"};
    constexpr std::array<std::size_t,4> slots{6,6,2,4};
    Candidate7Package3D result;
    for(std::size_t i=0;i<4;++i){
        const auto file=root/assets[i],sidecar=root/sidecars[i];
        if(!verify(file,assetSha[i])||!verify(sidecar,materialSha[i]))throw std::runtime_error("Candidate7 pinned file identity mismatch");
        auto loaded=Aevrynd::LoadSkinnedModelAssetFileDetailed(file);
        if(!loaded||loaded.asset.jointNames.size()!=53||loaded.asset.materialSlotNames.size()!=slots[i])throw std::runtime_error("Candidate7 typed asset contract failed");
        Aevrynd::HumanoidSkeletonMap3D map;
        if(!Aevrynd::ResolveCharacterRig3D(loaded.asset,map))throw std::runtime_error("Candidate7 semantic map failed");
        auto& part=result.components[i];part.asset=std::move(loaded.asset);part.materialRoot=sidecar.parent_path();
        if(i&&(part.asset.jointNames!=result.components[0].asset.jointNames||part.asset.model.skeletonCompatibilityId!=result.components[0].asset.model.skeletonCompatibilityId))throw std::runtime_error("Candidate7 component bind identity mismatch");
        if(i==2){
            // 已釘選的歷史衣裝側車有六槽；先完整驗證，再明確取下裝兩槽，未放寬解析器。
            const std::array<std::string,6> sourceSlots{"Sclera","DarkBrownIris","Pupil","DarkBrownIris.001","ReferenceTrouserCloth","ReferenceLeather"};
            if(part.asset.materialSlotNames!=std::vector<std::string>{sourceSlots[4],sourceSlots[5]})throw std::runtime_error("Candidate7 lower material identity failed");
            part.sidecar=Aevrynd::LoadSkinnedMaterialSidecarFile3D(sidecar,sourceSlots,part.materialRoot);
            if(part.sidecar){part.sidecar.materials.erase(part.sidecar.materials.begin(),part.sidecar.materials.begin()+4);part.sidecar.pbrOverrides.erase(part.sidecar.pbrOverrides.begin(),part.sidecar.pbrOverrides.begin()+4);part.sidecar.normalScaleOverrides.erase(part.sidecar.normalScaleOverrides.begin(),part.sidecar.normalScaleOverrides.begin()+4);}
        }else part.sidecar=Aevrynd::LoadSkinnedMaterialSidecarFile3D(sidecar,part.asset.materialSlotNames,part.materialRoot);
        if(!part.sidecar||part.sidecar.materials.size()!=slots[i])throw std::runtime_error("Candidate7 material sidecar failed component="+std::to_string(i)+" "+part.sidecar.error.message);
    }
    return result;
}

inline Aevrynd::FirstPersonArmsMeshResult3D BuildCandidate7Arms3D(const Aevrynd::SkinnedModelAssetData& source){
    // 只提供萃取器的名稱視圖；索引、權重、bind 與輸出 compatibility 均維持原本角色。
    auto view=source;
    constexpr std::array<const char*,8> from{"clavicle_l","upperarm_l","lowerarm_l","hand_l","clavicle_r","upperarm_r","lowerarm_r","hand_r"};
    constexpr std::array<const char*,8> to{"LeftShoulder","LeftUpperArm","LeftLowerArm","LeftHand","RightShoulder","RightUpperArm","RightLowerArm","RightHand"};
    for(auto& name:view.jointNames)for(std::size_t i=0;i<from.size();++i)if(name==from[i]){name=to[i];break;}
    return Aevrynd::BuildFirstPersonArmsSkinnedMesh3D(view);
}
}
