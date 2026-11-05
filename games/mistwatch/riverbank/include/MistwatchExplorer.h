#pragma once
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkeletalIk3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <Aevrynd/FootSwingTrajectory3D.h>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include "MistwatchExplorerArtMesh.h"
#include "MistwatchLocomotionMotor.h"
namespace Mistwatch {
// 僅供呼叫端觀測未提交候選；不屬於人物狀態、存檔或狀態雜湊。
enum class ExplorerFootPlacementFailure { None,TurnSupportDuration,PelvisLowering };
// 同時刻重驗保留步態metadata，只禁止時間前進與新擺腳計畫；一般呼叫預設照舊。
enum class ExplorerFootPlacementMode { Advance,RevalidateCurrent };
struct ExplorerFootCandidateDiagnostic {
    bool sampled{},lowerPelvisAvailable{};
    Aevrynd::Vec3 hip{},knee{},ankle{},target{};
    float reach{},horizontal{},requiredLowerPelvis{};
};
struct ExplorerFootPlacementDiagnostic {
    ExplorerFootPlacementFailure reason{ExplorerFootPlacementFailure::None};
    unsigned failureLine{},swingSide{},supportSide{};
    bool captured{},targetsAreLandings{},grounded{},crouched{},moving{},settling{},turning{},initialize{},mayBegin{};
    Aevrynd::Vec3 feet{},velocity{};
    float yaw{},requestedYaw{},lowerPelvis{};
    std::array<ExplorerFootCandidateDiagnostic,2> legs{};
    double turnInitial{},turnRadius{},turnOffset{},turnSpeed{},turnOriginalDuration{},turnDuration{},turnLow{},turnHigh{},turnReach{},turnVertical{};
};
namespace ExplorerDeathSupportDetail {
// 只為本次真蒙皮表面建立保守索引；容量不足或不可證明時保留完整舊掃描。
struct SolidCandidates {
    std::array<std::size_t,128> indices{};
    std::size_t count{};
    bool filtered{};
    std::size_t Size(std::span<const Aevrynd::BoxCollider3D> solids)const noexcept{return filtered?count:solids.size();}
    const Aevrynd::BoxCollider3D& At(std::span<const Aevrynd::BoxCollider3D> solids,std::size_t i)const noexcept{
        return solids[filtered?indices[i]:i];
    }
};
inline SolidCandidates Build(const Aevrynd::Mesh3D& surface,const Aevrynd::Matrix4& world,
    std::span<const Aevrynd::BoxCollider3D> solids)noexcept{
    using namespace Aevrynd;SolidCandidates out;
    if(surface.vertices.empty())return out;
    float minX=(std::numeric_limits<float>::max)(),maxX=-(std::numeric_limits<float>::max)(),minZ=minX,maxZ=maxX;
    for(const auto& vertex:surface.vertices){
        const auto p=TransformPoint(world,{vertex.x,vertex.y,vertex.z});
        // 不提前拒絕後方壞頂點：舊頂點順序仍可能先命中合法 limited 分支。
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return {};
        minX=(std::min)(minX,p.x);maxX=(std::max)(maxX,p.x);
        minZ=(std::min)(minZ,p.z);maxZ=(std::max)(maxZ,p.z);
    }
    for(std::size_t i=0;i<solids.size();++i){const auto& box=solids[i];
        if(!std::isfinite(box.center.x)||!std::isfinite(box.center.y)||!std::isfinite(box.center.z)||
            !std::isfinite(box.extents.x)||!std::isfinite(box.extents.y)||!std::isfinite(box.extents.z)||
            box.extents.x<0||box.extents.y<0||box.extents.z<0)return {};
        const float lx=minX-box.center.x,hx=maxX-box.center.x,lz=minZ-box.center.z,hz=maxZ-box.center.z;
        if(!std::isfinite(lx)||!std::isfinite(hx)||!std::isfinite(lz)||!std::isfinite(hz))return {};
        // 使用原 abs(p-center) 比較的單調端點，不以可能向內捨入的 center±extent 裁切。
        const bool outside=(minX>box.center.x&&std::abs(lx)>box.extents.x)||
            (maxX<box.center.x&&std::abs(hx)>box.extents.x)||
            (minZ>box.center.z&&std::abs(lz)>box.extents.z)||
            (maxZ<box.center.z&&std::abs(hz)>box.extents.z);
        if(outside)continue;
        if(out.count==out.indices.size())return {};
        out.indices[out.count++]=i;
    }
    out.filtered=true;return out;
}
}
// 本作獨立製作的穿衣角色原型；不依賴舊人物資產。
struct Explorer {
    Aevrynd::SkinnedModel3D model;
    Aevrynd::AnimationPose3D pose;
    ExplorerArtReceipt art;
    float phase{},yaw{},requestedYaw{};
    std::array<Aevrynd::Vec3,2> footTargets{};
    std::array<bool,2> planted{};
    std::array<std::vector<Aevrynd::Vec3>,2> soleOffsets;
    Aevrynd::Vec3 lastFeet{};
    bool feetInitialized{};
    float locomotionDistance{};
    float locomotionSeconds{};
    Aevrynd::Vec3 locomotionVelocity{};
    std::array<Aevrynd::FootSwingTrajectory3D,2> footSwings;
    std::array<float,2> footFacing{},swingFacingStart{},swingFacingEnd{};
    bool gaitGrounded{};
    unsigned nextFoot{1};
    // 根運動預判與實際擺腳共用選腳規則；拖後腳可能連續踏步，不保證左右交替。
    unsigned SelectSwingFoot(Aevrynd::Vec3 feet,Aevrynd::Vec3 direction,bool moving,bool turning)const noexcept{
        if(moving){
            const auto lag=[&](unsigned side){const auto delta=feet-footSwings[side].Landing();
                return delta.x*direction.x+delta.z*direction.z;};
            const float left=lag(0),right=lag(1);
            // 急轉通常交替；但支撐腳已明顯拖後時先回收，避免另一短步期間被根運動拉到不可達。
            const bool supportUrgent=turning&&std::max(left,right)>.35f&&std::abs(left-right)>.12f;
            if((!turning&&std::abs(left-right)>.025f)||supportUrgent)return left>right?0u:1u;
        }
        return nextFoot;
    }
    void ResetLocomotion(){
        footSwings={};footFacing={};swingFacingStart={};swingFacingEnd={};gaitGrounded=false;nextFoot=1;locomotionSeconds=0;locomotionVelocity={};
        planted={};footTargets={};lastFeet={};feetInitialized=false;locomotionDistance=0;
    }
    static constexpr float DeathDuration=1.f;
    float deathElapsed{};
    bool deathActive{},deathGroundedBasis{},deathLimited{};
    float deathMinimumClearance{};
    std::array<float,4> deathSupportGaps{};
    Aevrynd::AnimationPose3D deathStart,deathEnd;
    Aevrynd::SkinningWorkspace3D deathSkinning;
    Aevrynd::Mesh3D deathSurface;
    void ResetDeath(){deathElapsed=0;deathActive=false;deathGroundedBasis=false;deathLimited=false;
        deathMinimumClearance=0;deathSupportGaps={};deathStart={};deathEnd={};}
    bool DeathFinished()const{return deathActive&&deathElapsed>=DeathDuration;}
    float DeathProgress()const{return deathElapsed/DeathDuration;}
    bool DeathSupported()const{return DeathFinished()&&deathGroundedBasis&&!deathLimited;}
    bool DeathLimited()const{return deathLimited;}
    float DeathMinimumClearance()const{return deathMinimumClearance;}
    const std::array<float,4>& DeathSupportGaps()const{return deathSupportGaps;}
    static Aevrynd::Quaternion3D DeathMultiply(Aevrynd::Quaternion3D a,Aevrynd::Quaternion3D b){
        return Aevrynd::NormalizeQuaternion3D({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
    }
    void DeathOrient(unsigned end,Aevrynd::Quaternion3D desired,Aevrynd::AnimationPose3D& target)const{
        using namespace Aevrynd;
        std::array<Quaternion3D,15> global{};
        for(unsigned j=0;j<15;++j){const auto& t=target.localTransforms[j];
            const auto local=t.usesQuaternion?t.quaternion:QuaternionFromEuler3D(t.rotation);
            const int parent=model.skeleton.joints[j].parent;
            global[j]=parent<0?local:DeathMultiply(global[static_cast<unsigned>(parent)],local);
        }
        const auto parent=global[static_cast<unsigned>(model.skeleton.joints[end].parent)];
        target.localTransforms[end].quaternion=DeathMultiply({-parent.x,-parent.y,-parent.z,parent.w},desired);
        target.localTransforms[end].usesQuaternion=true;
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,target);
    }
    // 有限的程序式跪倒，不是布娃娃物理；根位置、碰撞體與十五關節資產皆不變。
    bool UpdateDeath(float dt,Aevrynd::Vec3 feet,const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::BoxCollider3D> solids){
        using namespace Aevrynd;
        if(!std::isfinite(dt)||dt<0||dt>DeathDuration||!std::isfinite(feet.x)||!std::isfinite(feet.y)||
           !std::isfinite(feet.z)||!std::isfinite(yaw)||model.skeleton.joints.size()!=15||pose.localTransforms.size()!=15||
           pose.globalMatrices.size()!=15||!model.skeleton.Valid()||model.inverseBindMatrices.size()!=15)return false;
        for(const auto& box:solids){for(float v:{box.center.x,box.center.y,box.center.z,box.extents.x,box.extents.y,box.extents.z})
                if(!std::isfinite(v))return false;
            if(box.extents.x<0||box.extents.y<0||box.extents.z<0)return false;
        }
        if(!std::isfinite(terrain.SampleHeight(feet.x,feet.z)))return false;
        for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float v:row)if(!std::isfinite(v))return false;
        for(const auto& local:pose.localTransforms){
            for(float v:{local.translation.x,local.translation.y,local.translation.z,local.rotation.x,local.rotation.y,local.rotation.z,
                local.scale.x,local.scale.y,local.scale.z,local.quaternion.x,local.quaternion.y,local.quaternion.z,local.quaternion.w})
                if(!std::isfinite(v))return false;
            if(local.scale.x<=0||local.scale.y<=0||local.scale.z<=0)return false;
            if(local.usesQuaternion&&local.quaternion.x*local.quaternion.x+local.quaternion.y*local.quaternion.y+
                local.quaternion.z*local.quaternion.z+local.quaternion.w*local.quaternion.w<1.e-12f)return false;
        }
        // 呼叫端在死亡期間凍結世界；換場、讀檔或重試須先 ResetDeath。
        if(deathLimited||DeathFinished())return true;
        const auto limited=[&]{deathActive=true;deathLimited=true;return true;};
        const auto world=CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
        const auto inverse=CreateRotationMatrix({0,-yaw,0})*CreateTranslationMatrix(feet*-1.f);
        const auto support=[&](Vec3 p){
            float h=terrain.SampleHeight(p.x,p.z);
            for(const auto& solid:solids){const float top=solid.center.y+solid.extents.y;
                if(std::abs(p.x-solid.center.x)<=solid.extents.x&&std::abs(p.z-solid.center.z)<=solid.extents.z&&
                   top<=feet.y+.4f&&top>h)h=top;
            }
            return h;
        };
        if(!deathActive){
            if(!model.Valid())return false;
            deathStart=pose;deathEnd=pose;
            for(unsigned j=0;j<15;++j)deathEnd.localTransforms[j]=model.skeleton.joints[j].bindTransform;
            deathEnd.localTransforms[0].translation.y=.414f;
            deathEnd.localTransforms[0].rotation.x=.95f;
            deathEnd.localTransforms[1].rotation.x=.65f;
            deathEnd.localTransforms[2].rotation.x=.15f;
            for(unsigned side=0;side<2;++side){
                deathEnd.localTransforms[9+side*3].rotation.x=-1.65f;
                deathEnd.localTransforms[10+side*3].rotation.x=2.28f;
                deathEnd.localTransforms[11+side*3].rotation.x=-1.58f;
            }
            BuildAnimationPoseGlobalMatrices3D(model.skeleton,deathEnd);
            const float base=support(feet);if(!std::isfinite(base))return false;
            deathGroundedBasis=std::abs(base-feet.y)<=.12f;
            for(unsigned side=0;side<2;++side){
                const unsigned upper=3+side*3;const float sign=side?1.f:-1.f;
                // 手套掌面以外仍有袖口厚度；腕心保留 41mm，不能只按手掌的 28mm 厚度貼地。
                Vec3 hand={sign*.30f,.041f,.53f};
                if(deathGroundedBasis){auto p=TransformPoint(world,hand);p.y=support(p)+.041f;hand=TransformPoint(inverse,p);}
                TwoBoneIkResult3D result;
                if(!SolveTwoBoneIk3D(model.skeleton,{upper,upper+1,upper+2},
                    {hand,{sign*.8f,.35f,.5f},1,0},deathEnd,&result)||result.endError>.004f)return limited();
                DeathOrient(upper+2,QuaternionFromEuler3D({-1.57079632679f,0,0}),deathEnd);
            }
            deathActive=true;deathElapsed=0;
        }
        // 零時間建立來源姿態時逐位元保留畫面，避免死亡當幀跳回 bind pose。
        if(dt==0)return true;
        const float elapsed=(std::min)(DeathDuration,deathElapsed+dt);
        const float t=elapsed/DeathDuration,blend=t*t*(3.f-2.f*t);
        AnimationPose3D candidate;
        BlendAnimationPosesInto(model.skeleton,deathStart,deathEnd,blend,candidate);
        // 腳踝沿來源至終點路徑移動；支撐時仍由正式雙骨求解器維持靴底接觸。
        for(unsigned side=0;side<2;++side){
            const unsigned upper=9+side*3,end=upper+2;
            const auto start=TransformPoint(deathStart.globalMatrices[end],{}),finish=TransformPoint(deathEnd.globalMatrices[end],{});
            Vec3 target=start+(finish-start)*blend;
            if(deathGroundedBasis){auto p=TransformPoint(world,target);p.y=(std::max)(p.y,support(p)+.1f);target=TransformPoint(inverse,p);}
            TwoBoneIkResult3D result;
            if(!SolveTwoBoneIk3D(model.skeleton,{upper,upper+1,end},{target,{side?.12f:-.12f,.4f,1},1,0},candidate,&result)||result.endError>.004f)return limited();
            DeathOrient(end,{},candidate);
        }
        if(!SkinModelInto(model,candidate,deathSkinning,deathSurface))return false;
        const auto deathSolids=ExplorerDeathSupportDetail::Build(deathSurface,world,solids);
        const auto surfaceSupport=[&](Vec3 p){
            if(!deathSolids.filtered)return support(p);
            float h=terrain.SampleHeight(p.x,p.z);
            for(std::size_t n=0;n<deathSolids.Size(solids);++n){const auto& solid=deathSolids.At(solids,n);
                const float top=solid.center.y+solid.extents.y;
                if(std::abs(p.x-solid.center.x)<=solid.extents.x&&std::abs(p.z-solid.center.z)<=solid.extents.z&&
                   top<=feet.y+.4f&&top>h)h=top;
            }
            return h;
        };
        float clearance=(std::numeric_limits<float>::max)();
        std::array<float,4> gaps;gaps.fill((std::numeric_limits<float>::max)());
        for(std::size_t i=0;i<deathSurface.vertices.size();++i){const auto& vertex=deathSurface.vertices[i];
            const auto p=TransformPoint(world,{vertex.x,vertex.y,vertex.z});
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
            const float gap=p.y-surfaceSupport(p);if(!std::isfinite(gap))return false;clearance=(std::min)(clearance,gap);
            if(gap<-.002f)return limited();
            for(std::size_t n=0;n<deathSolids.Size(solids);++n){const auto& box=deathSolids.At(solids,n);
                if(std::abs(p.x-box.center.x)<box.extents.x-.002f&&
                std::abs(p.y-box.center.y)<box.extents.y-.002f&&std::abs(p.z-box.center.z)<box.extents.z-.002f)return limited();
            }
            const auto& source=model.bindMesh.vertices[i];
            if(source.y>.40f&&source.y<.56f){const unsigned side=source.x<0?0u:1u;gaps[side]=(std::min)(gaps[side],gap);}
            for(unsigned side=0;side<2;++side)if(model.influences[i].joints[0]==5+side*3&&model.influences[i].weights[0]==65535)
                gaps[2+side]=(std::min)(gaps[2+side],gap);
        }
        // 終點必須以真正蒙皮表面證明雙膝與雙手接觸，不能拿關節中心代替皮膚與布料。
        if(elapsed>=DeathDuration&&deathGroundedBasis)for(float gap:gaps)if(gap>.04f)return limited();
        pose=std::move(candidate);deathElapsed=elapsed;deathMinimumClearance=clearance;deathSupportGaps=gaps;planted={};return true;
    }
    Explorer() {
        using namespace Aevrynd;
        constexpr std::array<Vec3,15> joints{{{0,.9f,0},{0,1.2f,0},{0,1.58f,0},
            {-.24f,1.4f,0},{-.28f,1.1f,0},{-.28f,.84f,0},
            {.24f,1.4f,0},{.28f,1.1f,0},{.28f,.84f,0},
            {-.12f,.88f,0},{-.12f,.48f,0},{-.12f,.1f,0},
            {.12f,.88f,0},{.12f,.48f,0},{.12f,.1f,0}}};
        constexpr int parents[]{-1,0,1,1,3,4,1,6,7,0,9,10,0,12,13};
        for(unsigned i=0;i<joints.size();++i) {
            JointTransform3D bind;bind.translation=parents[i]<0?joints[i]:joints[i]-joints[parents[i]];
            model.skeleton.joints.push_back({parents[i],bind});
            model.inverseBindMatrices.push_back(CreateTranslationMatrix(joints[i]*-1.f));
        }
        art=BuildExplorerArtMesh(model);
        // 從真正靴底的剛性蒙皮頂點取得踝局部輪廓，不用縮小的探針冒充鞋掌。
        for(std::size_t i=0;i<model.bindMesh.vertices.size();++i){
            const auto& v=model.bindMesh.vertices[i];const auto& w=model.influences[i];
            if(v.y>.006f||w.weights[0]!=65535||(w.joints[0]!=11&&w.joints[0]!=14))continue;
            soleOffsets[(w.joints[0]-11)/3].push_back(TransformPoint(model.inverseBindMatrices[w.joints[0]],{v.x,v.y,v.z}));
        }
        model.skeletonCompatibilityId=ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
        Update({},0,false);
    }
    void Update(Aevrynd::Vec3 displacement,float dt,bool crouched,std::optional<float> stationaryFacing=std::nullopt) {
        using namespace Aevrynd;
        const float distance=std::hypot(displacement.x,displacement.z);
        if(!feetInitialized)ResetLocomotion();
        locomotionDistance=distance;
        locomotionSeconds=std::clamp(dt,0.f,.1f);
        locomotionVelocity=locomotionSeconds>0?Vec3{displacement.x,0,displacement.z}*(1.f/locomotionSeconds):Vec3{};
        requestedYaw=yaw;
        if(distance>.0001f&&locomotionSeconds>0){
            const float desired=std::atan2(displacement.x,displacement.z);
            const float turn=std::remainder(desired-yaw,6.2831853f),limit=9.42477796f*locomotionSeconds;
            requestedYaw=std::remainder(yaw+std::clamp(turn,-limit,limit),6.2831853f);
            if(!gaitGrounded)yaw=requestedYaw;
            phase=std::fmod(phase+distance*5.f,6.2831853f);
        }
        else if(stationaryFacing&&std::isfinite(*stationaryFacing)&&locomotionSeconds>0){
            // 只提出有限轉身意圖；接地朝向仍由真雙腳的支撐、擺腳與 IK 決定。
            constexpr float tau=6.2831853f,pi=tau*.5f;
            float turn=std::remainder(*stationaryFacing-yaw,tau);
            if(std::abs(std::abs(turn)-pi)<=4*std::numeric_limits<float>::epsilon())turn=pi;
            const float limit=9.42477796f*locomotionSeconds;
            requestedYaw=std::remainder(yaw+std::clamp(turn,-limit,limit),tau);
        }
        RebuildLocomotionPose(crouched);
    }
    // 讀檔只重建目前相位的四肢姿態；不消耗時間、重播位移或再改變朝向。
    void RebuildLocomotionPose(bool crouched) {
        using namespace Aevrynd;
        pose.localTransforms.clear();
        for(const auto& joint:model.skeleton.joints)pose.localTransforms.push_back(joint.bindTransform);
        const float stride=locomotionSeconds>0 && locomotionDistance>.0001f?.5f*std::sin(phase):0.f;
        for(unsigned side=0;side<2;++side) {
            const float swing=side?-stride:stride;
            pose.localTransforms[9+side*3].rotation.x=swing;
            pose.localTransforms[10+side*3].rotation.x=(std::max)(0.f,-swing)*.8f;
            pose.localTransforms[3+side*3].rotation.x=-swing*.7f;
        }
        if(crouched){
            pose.localTransforms[0].translation.y-=.45f;
            // 蹲姿由脊椎前傾協調重心；頭部反向補償保留原本視線方向。
            // 骨盆、兩腿、腳掌目標、站姿與控制器保持原值。
            pose.localTransforms[1].rotation.x=.6f;
            pose.localTransforms[2].rotation.x=-.3f;
            // 肩膀抵銷軀幹前傾，前臂向前屈曲以協調蹲姿重心。
            // 保留步態擺動、骨盆、頭部及兩側足部反向運動學。
            for(unsigned side=0;side<2;++side){
                pose.localTransforms[3+side*3].rotation.x-=.6f;
                pose.localTransforms[4+side*3].rotation.x=-.65f;
            }
            for(unsigned side=0;side<2;++side){pose.localTransforms[9+side*3].rotation.x-=.8f;pose.localTransforms[10+side*3].rotation.x+=1.4f;}
        }
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
    }
    // 接地使用控制器相同的高度場及設施；支撐期保存世界座標以抑制滑步。
    bool PlaceFeet(Aevrynd::Vec3 feet,bool grounded,bool crouched,
        const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls={},unsigned* failureLine=nullptr,
        ExplorerFootPlacementDiagnostic* diagnostic=nullptr,
        ExplorerFootPlacementMode mode=ExplorerFootPlacementMode::Advance,bool braking=false) {
        // 腳步與姿態一起提交；第二腳或 IK 失敗時不能留下第一腳已開始的新計畫。
        auto priorPose=pose;
        const auto priorTargets=footTargets;const auto priorPlanted=planted;const auto priorSwings=footSwings;
        const auto priorFacing=footFacing,priorStart=swingFacingStart,priorEnd=swingFacingEnd;
        const auto priorFeet=lastFeet;const bool priorInitialized=feetInitialized,priorGrounded=gaitGrounded;
        const unsigned priorNext=nextFoot;const float priorYaw=yaw;
        const auto restore=[&]{
            pose=std::move(priorPose);footTargets=priorTargets;planted=priorPlanted;footSwings=priorSwings;
            footFacing=priorFacing;swingFacingStart=priorStart;swingFacingEnd=priorEnd;
            lastFeet=priorFeet;feetInitialized=priorInitialized;gaitGrounded=priorGrounded;nextFoot=priorNext;yaw=priorYaw;
        };
        if(failureLine)*failureLine=0;
        if(diagnostic)*diagnostic={};
        try{if(PlaceFeetCandidate(feet,grounded,crouched,terrain,solids,hulls,failureLine,diagnostic,mode,braking)){if(failureLine)*failureLine=0;return true;}}
        catch(...){restore();throw;}
        restore();return false;
    }
private:
    bool PlaceFeetCandidate(Aevrynd::Vec3 feet,bool grounded,bool crouched,
        const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,unsigned* failureLine,
        ExplorerFootPlacementDiagnostic* diagnostic,ExplorerFootPlacementMode mode,bool braking) {
        using namespace Aevrynd;
        const auto reject=[&](unsigned line){if(failureLine)*failureLine=line;return false;};
        const float elapsedSeconds=mode==ExplorerFootPlacementMode::RevalidateCurrent?0.f:locomotionSeconds;
        // 必須在本步身體轉向前保留意圖；不能因身體先跟上幾度就省略重新落腳。
        const bool pivoting=mode==ExplorerFootPlacementMode::Advance&&grounded&&gaitGrounded&&
            elapsedSeconds>0&&locomotionDistance<=.0001f&&
            std::abs(std::remainder(requestedYaw-yaw,6.2831853f))>.00001f;
        for(const auto* hull:hulls)if(!hull||!hull->AxisCount())return reject(__LINE__);
        // 先以真擺脚位置與落點約束身體轉向，不能讓髖部轉過仍固定在世界座標的雙腳。
        if(grounded&&gaitGrounded&&feetInitialized&&Length(feet-lastFeet)<=1.f){
            std::array<Vec3,2> predicted;
            for(unsigned side=0;side<2;++side){
                auto path=footSwings[side];FootSwingSample3D point;
                if(!path.Advance(elapsedSeconds,point))return reject(__LINE__);
                predicted[side]=point.positionWorld;
            }
            constexpr float separation=.11f,tau=6.2831853f;
            const float limit=9.42477796f*elapsedSeconds,wanted=std::remainder(requestedYaw-yaw,tau);
            float low=-limit,high=limit;
            for(const auto span:{predicted[1]-predicted[0],footSwings[1].Landing()-footSwings[0].Landing()}){
                const float width=std::hypot(span.x,span.z);
                if(!std::isfinite(width)||width<separation)return reject(__LINE__);
                const float half=std::acos(std::clamp(separation/width,0.f,1.f));
                const float center=std::remainder(std::atan2(-span.z,span.x)-yaw,tau);
                bool found=false;
                for(int wrap=-1;wrap<=1;++wrap){
                    const float a=std::max(low,center+float(wrap)*tau-half),b=std::min(high,center+float(wrap)*tau+half);
                    if(a<=b){low=a;high=b;found=true;break;}
                }
                if(!found)return reject(__LINE__);
            }
            yaw=std::remainder(yaw+std::clamp(wanted,low,high),tau);
        }
        const auto toWorld=CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
        const auto toModel=CreateRotationMatrix({0,-yaw,0})*CreateTranslationMatrix(feet*-1.f);
        if(!feetInitialized || Length(feet-lastFeet)>1.f){planted={};footSwings={};gaitGrounded=false;nextFoot=1;feetInitialized=true;}
        lastFeet=feet;
        // 移動時保留屈膝支撐姿態，減速後連續回到直立；站立不固定壓低骨盆。
        if(grounded&&!crouched){
            const float blend=std::clamp(std::hypot(locomotionVelocity.x,locomotionVelocity.z)/2.f,0.f,1.f);
            pose.localTransforms[0].translation.y-=.09f*blend*blend*(3-2*blend);
        }
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        const auto uncachedHeightAt=[&](Vec3 p,Vec3& normal,bool pointOnly=false){
            normal={0,1,0};
            float height=terrain.SampleHeight(p.x,p.z);
            for(const auto& solid:solids){
                const float top=solid.center.y+solid.extents.y;
                if(std::abs(p.x-solid.center.x)<=solid.extents.x && std::abs(p.z-solid.center.z)<=solid.extents.z &&
                    top<=feet.y+.4f && top>height)height=top;
            }
            // 同一屋頂構件供足底探測；有限腳掌厚度/寬度，不拿凸包最高點當地面。
            constexpr float halfY=.0001f,depth=1.5f;
            // 凸形掃掠要求正體積；輪廓頂點只用 0.1mm 的有限探針保留接觸語意。
            const Vec3 origin{p.x,feet.y+.4f+halfY,p.z},extent{pointOnly?.0001f:.02f,halfY,pointOnly?.0001f:.035f};
            for(const auto* hull:hulls){
                ConvexAabbSweep3D::Result hit;
                if(!hull->Sweep(origin,extent,{0,-depth,0},hit)||hit.status==ConvexAabbSweepStatus3D::StartedOverlapping)
                    return std::numeric_limits<float>::quiet_NaN();
                if(hit.status!=ConvexAabbSweepStatus3D::Hit)continue;
                Vec3 support{};
                for(std::size_t axis=0;axis<hull->AxisCount();++axis)for(bool positive:{false,true}){
                    if(!(positive?hit.positiveContacts:hit.negativeContacts)[axis])continue;
                    Vec3 n;if(!hull->Normal(axis,positive,n))return std::numeric_limits<float>::quiet_NaN();
                    if(n.y>=.65f&&n.y>support.y)support=n;
                }
                const float top=float(double(origin.y)-hit.first*depth)-halfY;
                if(support.y>=.65f&&top>height){height=top;normal=support;}
            }
            return height;
        };
        // 僅本次接地呼叫重用完全相同的查詢；不同高度不影響固定腳根的支撐探測。
        struct HeightAtMemoEntry {
            std::uint32_t x{},z{};
            bool pointOnly{};
            float height{};
            Vec3 normal{};
        };
        std::array<HeightAtMemoEntry,64> heightAtMemo{};
        std::size_t heightAtMemoCount{};
        const auto heightAt=[&](Vec3 p,Vec3& normal,bool pointOnly=false){
            // 非有限查詢仍完整走原核心，不新增拒絕，也不把失敗結果記成成功。
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
                return uncachedHeightAt(p,normal,pointOnly);
            const auto x=std::bit_cast<std::uint32_t>(p.x),z=std::bit_cast<std::uint32_t>(p.z);
            for(std::size_t i=0;i<heightAtMemoCount;++i){
                const auto& entry=heightAtMemo[i];
                if(entry.x==x&&entry.z==z&&entry.pointOnly==pointOnly){
                    normal=entry.normal;return entry.height;
                }
            }
            const float height=uncachedHeightAt(p,normal,pointOnly);
            if(heightAtMemoCount<heightAtMemo.size()&&std::isfinite(height)&&
               std::isfinite(normal.x)&&std::isfinite(normal.y)&&std::isfinite(normal.z))
                heightAtMemo[heightAtMemoCount++]={x,z,pointOnly,height,normal};
            return height;
        };
        const auto surfaceTarget=[&](unsigned footSide,float facing,Vec3 p,Vec3& target,Vec3& normal,bool requireSupport=true){
            const float height=heightAt(p,normal);
            if(!std::isfinite(height)||(requireSupport&&height<feet.y-.35f))return reject(__LINE__);
            target=p;target.y=height+.1f;
            if(hulls.empty())return true;
            const auto rotation=CreateRotationMatrix({0,facing,0});
            const auto q=NormalizeQuaternion3D({normal.z,0,-normal.x,1+normal.y});
            const Vec3 axis{q.x,q.y,q.z};
            // 依該側真正靴底輪廓驗證，不借用另一腳形狀。
            {
                if(soleOffsets[footSide].empty())return reject(__LINE__);
                for(const auto& offset:soleOffsets[footSide]){
                    const auto oriented=TransformPoint(rotation,offset);
                    const auto worldOffset=oriented+Cross(axis,oriented)*(2*q.w)+Cross(axis,Cross(axis,oriented))*2;
                    Vec3 ignored;const float h=heightAt(p+worldOffset,ignored,true);
                    if(!std::isfinite(h))return reject(__LINE__);
                    target.y=std::max(target.y,h-worldOffset.y+.005f);
                }
            }
            return true;
        };
        if(!grounded){
            // 空中不宣稱支撐，但仍檢查真靴底淨空；不把角色碰撞根吸回地面。
            for(unsigned side=0;side<2;++side){
                if(soleOffsets[side].empty())return reject(__LINE__);
                const unsigned upper=9+side*3,end=upper+2;
                Vec3 target=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[end],{}));
                float raise=0;
                for(const auto& sole:soleOffsets[side]){
                    const auto p=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[end],sole));
                    Vec3 normal;const float height=heightAt(p,normal,true);
                    if(!std::isfinite(height))return reject(__LINE__);
                    raise=std::max(raise,height+.005f-p.y);
                }
                if(raise>0){
                    std::array<Quaternion3D,15> rotations{};
                    for(unsigned j=0;j<15;++j){const auto& local=pose.localTransforms[j];
                        const auto q=local.usesQuaternion?local.quaternion:QuaternionFromEuler3D(local.rotation);
                        const int parent=model.skeleton.joints[j].parent;
                        rotations[j]=parent<0?q:DeathMultiply(rotations[static_cast<unsigned>(parent)],q);
                    }
                    target.y+=raise;TwoBoneIkResult3D result;
                    if(!SolveTwoBoneIk3D(model.skeleton,{upper,upper+1,end},
                        {TransformPoint(toModel,target),{side?.12f:-.12f,.5f,1},1,0},pose,&result)||result.endError>.005f)return reject(__LINE__);
                    DeathOrient(end,rotations[end],pose);
                }
                footTargets[side]=target;
                for(const auto& sole:soleOffsets[side]){
                    const auto p=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[end],sole));
                    Vec3 normal;const float height=heightAt(p,normal,true);
                    if(!std::isfinite(height)||p.y<height-.005f)return reject(__LINE__);
                }
            }
            planted={};footSwings={};gaitGrounded=false;return true;
        }
        const bool initialize=!gaitGrounded;
        const float speed=std::hypot(locomotionVelocity.x,locomotionVelocity.z);
        const bool moving=locomotionDistance>.0001f&&locomotionSeconds>0;
        const auto direction=speed>.0001f?locomotionVelocity*(1.f/speed):Vec3{};
        const std::array activeAtFrameStart{footSwings[0].Active(),footSwings[1].Active()};
        // 共同預覽本步結束的接觸狀態；前腳本步已真正落地，就可在幀末安排後腳。
        // 新擺腳仍從零時間開始，不回填本步，也不取決於左右腳的迴圈順序。
        auto endOfStepSwings=footSwings;
        for(unsigned side=0;side<2;++side)if(activeAtFrameStart[side]){
            FootSwingSample3D end;
            if(!endOfStepSwings[side].Advance(elapsedSeconds,end))return reject(__LINE__);
        }
        const bool mayBegin=mode==ExplorerFootPlacementMode::Advance&&
            !endOfStepSwings[0].Active()&&!endOfStepSwings[1].Active();
        const auto facingAt=[&](unsigned side,const FootSwingTrajectory3D& path){
            const double u=path.Duration()>0?path.Elapsed()/path.Duration():1;
            const double blend=u*u*u*(10+u*(-15+6*u));
            return swingFacingStart[side]+std::remainder(swingFacingEnd[side]-swingFacingStart[side],6.2831853f)*float(blend);
        };
        for(unsigned side=0;side<2;++side)if(mayBegin&&activeAtFrameStart[side]){
            // 即使同一腳立即再踏步，舊落點仍先驗證真正支撐，不可用新擺腳免除接地門。
            const auto& completed=endOfStepSwings[side];
            const auto landing=completed.Landing();const float facing=facingAt(side,completed);
            Vec3 required,normal;
            if(!surfaceTarget(side,facing,landing,required,normal)||landing.y<required.y-.0005f)return reject(__LINE__);
            footSwings[side]=completed;footFacing[side]=facing;
        }
        const bool turning=pivoting||(moving&&std::abs(std::remainder(std::atan2(direction.x,direction.z)-yaw,6.2831853f))>.26179939f);
        unsigned nextAtFrameStart=SelectSwingFoot(feet,direction,moving,turning);
        const bool settling=!pivoting&&!moving&&speed<.001f&&!crouched&&!initialize&&mayBegin;
        // 在拒絕前擷取真候選；回滾後不得用上幀已提交姿態充當失敗證據。
        const auto captureCandidate=[&](ExplorerFootPlacementFailure reason,unsigned line,bool landings){
            if(!diagnostic)return;
            diagnostic->reason=reason;diagnostic->failureLine=line;diagnostic->captured=true;
            diagnostic->targetsAreLandings=landings;diagnostic->grounded=grounded;diagnostic->crouched=crouched;
            diagnostic->moving=moving;diagnostic->settling=settling;diagnostic->turning=turning;
            diagnostic->initialize=initialize;diagnostic->mayBegin=mayBegin;
            diagnostic->feet=feet;diagnostic->velocity=locomotionVelocity;
            diagnostic->yaw=yaw;diagnostic->requestedYaw=requestedYaw;
            for(unsigned side=0;side<2;++side){
                auto& leg=diagnostic->legs[side];const unsigned upper=9+side*3;
                leg.sampled=true;
                leg.hip=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper],{}));
                leg.knee=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+1],{}));
                leg.ankle=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+2],{}));
                leg.target=landings?footSwings[side].Landing():footTargets[side];
                leg.reach=Length(leg.knee-leg.hip)+Length(leg.ankle-leg.knee)-.002f;
                const float x=leg.target.x-leg.hip.x,z=leg.target.z-leg.hip.z;
                leg.horizontal=std::hypot(x,z);
                const float squared=leg.reach*leg.reach-x*x-z*z;
                leg.lowerPelvisAvailable=std::isfinite(squared)&&leg.reach>0&&squared>=0;
                leg.requiredLowerPelvis=leg.lowerPelvisAvailable?
                    leg.hip.y-leg.target.y-std::sqrt(squared):std::numeric_limits<float>::quiet_NaN();
            }
        };
        std::array<float,2> restErrors{};
        if(settling){
            for(unsigned side=0;side<2;++side){
                const auto rest=TransformPoint(toWorld,{side?.12f:-.12f,.1f,0});
                const auto delta=footSwings[side].Landing()-rest;
                restErrors[side]=std::hypot(delta.x,delta.z);
                if(std::abs(std::remainder(footFacing[side]-yaw,6.2831853f))>.05f)
                    restErrors[side]=std::max(restErrors[side],.03f);
            }
            // 停步後一次只收一腳，另一腳保持支撐；使用既有連續軌跡而非重設腳點。
            nextAtFrameStart=restErrors[0]>=restErrors[1]?0u:1u;
            // 偏側站姿不能只按距離收腳：較遠腳的中立落點可能被另一支撐腳佔住。
            // 先移開能合法回到中立位置的那腳，再收另一腳；沿用既有11cm分隔門。
            const auto restCompatible=[&](unsigned side){
                const auto target=TransformPoint(toWorld,{side?.12f:-.12f,.1f,0});
                const auto span=side?target-footSwings[0].Landing():footSwings[1].Landing()-target;
                return TransformPoint(CreateRotationMatrix({0,-yaw,0}),span).x>=.11f;
            };
            const unsigned alternate=1-nextAtFrameStart;
            if(!restCompatible(nextAtFrameStart)&&restErrors[alternate]>.02f&&restCompatible(alternate))
                nextAtFrameStart=alternate;
        }
        // 急轉時兩腳都必須重新建立支撐；持續優先同一拖後腳會讓另一腳永久限制身體朝向。
        if(mayBegin&&turning&&!initialize){
            // 高差會耗盡站立腿的三維可達餘裕，不能只按水平拖後距離選腳。
            // 只有原支撐不能維持最短合法跨步、另一腳可以時才交換；不改進行中的擺腳。
            const auto supportsMinimumTurn=[&](unsigned supportSide){
                const unsigned upper=9+supportSide*3;
                const auto localHip=TransformPoint(pose.globalMatrices[upper],{});
                const auto hip=TransformPoint(toWorld,localHip);
                const auto knee=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+1],{}));
                const auto ankle=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+2],{}));
                const auto support=footSwings[supportSide].Landing();
                const double reach=Length(knee-hip)+Length(ankle-knee)-.002f;
                const double vertical=std::max(0.,double(hip.y-support.y)-.35);
                if(!std::isfinite(reach)||!std::isfinite(vertical)||reach<=vertical)return false;
                const double radius=std::sqrt(reach*reach-vertical*vertical);
                const double initial=std::hypot(double(hip.x-support.x),double(hip.z-support.z));
                const double offset=std::hypot(double(localHip.x),double(localHip.z));
                constexpr double h=double(LocomotionMotor::FixedStep),t=.05+2*h;
                const double extent=initial+double(speed)*t+.5*LocomotionMotor::Deceleration*t*(t+h)+
                    2*offset*std::sin(std::min(9.42477796*t,3.141592653589793)/2);
                return std::isfinite(extent)&&extent<=radius;
            };
            if(!supportsMinimumTurn(1-nextAtFrameStart)&&supportsMinimumTurn(nextAtFrameStart))
                nextAtFrameStart=1-nextAtFrameStart;
        }
        std::array<Vec3,2> supportNormals{};
        for(unsigned side=0;side<2;++side){
            const auto rest=TransformPoint(toWorld,{side?.12f:-.12f,.1f,0});
            const bool completedThisStep=mayBegin&&activeAtFrameStart[side];
            if(initialize){
                Vec3 target;
                footFacing[side]=swingFacingStart[side]=swingFacingEnd[side]=yaw;
                if(!surfaceTarget(side,footFacing[side],rest,target,supportNormals[side])||!footSwings[side].Reset(target))return reject(__LINE__);
            }
            const bool requested=!initialize&&mayBegin&&side==nextAtFrameStart&&
                (moving||pivoting||(settling&&restErrors[side]>.02f));
            bool began=false;
            if(requested){
                // 不能用起步瞬間的低速鎖住長擺腳時間；按控制器加速上界預留最多30cm根位移。
                constexpr double travel=.3,acceleration=LocomotionMotor::Acceleration;
                double duration=settling?std::clamp(.28*double(restErrors[side])/double(.12f),.12,.28):std::clamp(2*travel/(double(speed)+std::sqrt(double(speed)*speed+2*acceleration*travel)),.05,.22);
                const double originalDuration=duration;
                if(turning){
                    // 轉向中的另一腳要支撐到本腳落地；不能只按根位移決定跨步時長。
                    // 直線跑步另有真騰空／落地週期，不用此轉向界假定全程都在支撐。
                    // 此界保留目前支撐高度的可達餘裕，地形改變仍須逐步通過正式 IK／鞋底驗證。
                    const unsigned upper=9+(1-side)*3;
                    const auto localHip=TransformPoint(pose.globalMatrices[upper],{});
                    const auto hip=TransformPoint(toWorld,localHip);
                    const auto knee=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+1],{}));
                    const auto ankle=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+2],{}));
                    const auto support=footSwings[1-side].Landing();
                    const double reach=Length(knee-hip)+Length(ankle-knee)-.002f;
                    const double vertical=std::max(0.,double(hip.y-support.y)-.35);
                    if(reach<=vertical)return reject(__LINE__);
                    const double radius=std::sqrt(reach*reach-vertical*vertical);
                    const double initial=std::hypot(double(hip.x-support.x),double(hip.z-support.z));
                    const double offset=std::hypot(double(localHip.x),double(localHip.z));
                    constexpr double h=double(LocomotionMotor::FixedStep),maxAcceleration=LocomotionMotor::Deceleration;
                    const auto extent=[&](double t){return initial+double(speed)*t+
                        .5*maxAcceleration*t*(t+h)+2*offset*std::sin(std::min(9.42477796*t,3.141592653589793)/2);};
                    double low=0,high=duration+2*h;
                    if(extent(0)>radius)return reject(__LINE__);
                    if(extent(high)>radius){
                        for(unsigned iteration=0;iteration<32;++iteration){
                            const double mid=(low+high)*.5;
                            if(extent(mid)<=radius)low=mid;else high=mid;
                        }
                        duration=std::min(duration,low-2*h);
                    }
                    // 不把不足的支撐時間硬夾到最短步；足速上限仍交由 Begin 驗證。
                    if(duration<.05){
                        const unsigned line=__LINE__;
                        captureCandidate(ExplorerFootPlacementFailure::TurnSupportDuration,line,true);
                        if(diagnostic){
                            diagnostic->swingSide=side;diagnostic->supportSide=1-side;
                            diagnostic->turnInitial=initial;diagnostic->turnRadius=radius;diagnostic->turnOffset=offset;
                            diagnostic->turnSpeed=speed;diagnostic->turnOriginalDuration=originalDuration;
                            diagnostic->turnDuration=duration;diagnostic->turnLow=low;diagnostic->turnHigh=high;
                            diagnostic->turnReach=reach;diagnostic->turnVertical=vertical;
                        }
                        return reject(line);
                    }
                }
                const auto backward=TransformPoint(CreateRotationMatrix({0,yaw,0}),{0,0,-1});
                // 轉身時支撐腳可能偏離身體中線；落腳線要避開它，不可只在固定 ±12cm 上尋找。
                const auto otherLocal=TransformPoint(toModel,footSwings[1-side].Landing());
                const float stepLateral=side?std::max(.12f,otherLocal.x+.12f):std::min(-.12f,otherLocal.x-.12f);
                const auto stepRest=TransformPoint(toWorld,{stepLateral,.1f,0});
                const auto narrow=rest+(feet-rest)*.5f;
                // 急轉中速度方向不等於身體前向；原前探會橫越另一腳而全部被拒絕。
                // 沿已安全分隔的落腳線保留前後分量，避免橫跨而被迫只選原地短步。
                const auto forward=backward*-1.f;
                const auto alongFacing=forward*(direction.x*forward.x+direction.z*forward.z);
                // 高速制動保留一步實際行程的前探；無法通過原腿長及鞋底門時仍檢查既有候選。
                const float lead=std::max(.4f,float(double(speed)*duration));
                // 放鍵制動只縮短本次新步的首選前探；舊備選及完整足部驗證維持不變。
                // 此提示不預知下一步的新輸入，也不改寫已開始的腳步軌跡。
                float firstLead=lead;
                if(braking&&mode==ExplorerFootPlacementMode::Advance&&grounded&&moving&&!crouched&&!turning&&!initialize){
                    const double stoppingDistance=double(speed)*double(speed)/(2*LocomotionMotor::Deceleration);
                    if(std::isfinite(lead)&&std::isfinite(stoppingDistance)&&stoppingDistance>=0&&stoppingDistance<double(lead))
                        firstLead=float(stoppingDistance);
                }
                const std::array candidates{stepRest+direction*firstLead,stepRest+alongFacing*lead,
                    stepRest+direction*.4f,stepRest+alongFacing*.4f,
                    stepRest+direction*.25f,stepRest+alongFacing*.25f,stepRest,
                    stepRest+backward*.2f,stepRest-backward*.2f,narrow,narrow+backward*.2f,narrow-backward*.2f,
                    narrow+backward*.4f,narrow-backward*.4f,narrow+backward*.55f,narrow-backward*.55f};
                bool accepted=false;
                for(unsigned candidateIndex=0;candidateIndex<(settling?1u:static_cast<unsigned>(candidates.size()));++candidateIndex){
                    const auto candidate=settling?rest:candidates[candidateIndex];
                    const float lateral=TransformPoint(toModel,candidate).x;
                    if(side?lateral<.055f:lateral>-.055f)continue;
                    Vec3 landing,normal;if(!surfaceTarget(side,yaw,candidate,landing,normal))continue;
                    // 落點符合地面高度不代表腿長可達；根被牆或陡坡擋住時也必須能完成落腳。
                    // 預留減速回直立的髖高，不依賴本幀移動屈膝或預期根位移來湊可達範圍。
                    const unsigned upper=9+side*3;
                    auto hip=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper],{}));
                    const auto knee=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+1],{}));
                    const auto ankle=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+2],{}));
                    const float reach=Length(knee-hip)+Length(ankle-knee)-.002f;
                    if(!crouched){
                        const float blend=std::clamp(speed/2.f,0.f,1.f);
                        hip.y+=.09f*blend*blend*(3-2*blend);
                    }
                    const float dx=landing.x-hip.x,dz=landing.z-hip.z;
                    const float horizontalSquared=dx*dx+dz*dz;
                    const float verticalSquared=reach*reach-horizontalSquared;
                    if(!std::isfinite(verticalSquared)||reach<=0||verticalSquared<=0||
                        hip.y-landing.y-std::sqrt(verticalSquared)>.35f)continue;
                    // 本腳落點必須與仍固定的另一腳相容；只檢查左右半身不能排除兩靴落在一起。
                    const auto pair=side?landing-footSwings[0].Landing():footSwings[1].Landing()-landing;
                    const auto localPair=TransformPoint(CreateRotationMatrix({0,-yaw,0}),pair);
                    if(localPair.x<.11f)continue;
                    // 整段先驗鞋底高度，不在取樣後抬高腳來掩蓋穿模；固定次數，無無界剩餘時間迴圈。
                    const std::array<double,4> lifts=settling||pivoting?std::array<double,4>{.04,.08,.12,.16}:std::array<double,4>{.14,.21,.28,.35};
                    for(double lift:lifts){
                        auto plan=footSwings[side];
                        if(!plan.Begin(landing,duration,lift,60))continue;
                        auto probe=plan;bool clear=true;
                        for(unsigned sample=0;sample<12;++sample){
                            FootSwingSample3D point;Vec3 required,normalAtPoint;
                            if(!probe.Advance(duration/12,point)){clear=false;break;}
                            const double u=probe.Elapsed()/duration,blend=u*u*u*(10+u*(-15+6*u));
                            const float facing=footFacing[side]+std::remainder(yaw-footFacing[side],6.2831853f)*float(blend);
                            if(!surfaceTarget(side,facing,point.positionWorld,required,normalAtPoint,false)||
                                point.positionWorld.y<required.y-.0005f){clear=false;break;}
                        }
                        if(clear){
                            swingFacingStart[side]=footFacing[side];swingFacingEnd[side]=yaw;
                            footSwings[side]=plan;accepted=began=true;nextFoot=1-side;break;
                        }
                    }
                    if(accepted)break;
                }
                if(!accepted)return reject(__LINE__);
            }
            FootSwingSample3D sample;
            // 在幀末建立新計畫，本幀不回填整段 dt；落地後餘量作支撐停留，下一幀才安排新步。
            if(!footSwings[side].Advance(initialize||began||completedThisStep?0:elapsedSeconds,sample))return reject(__LINE__);
            if(footSwings[side].Duration()>0)footFacing[side]=facingAt(side,footSwings[side]);
            Vec3 required;
            if(!surfaceTarget(side,footFacing[side],sample.positionWorld,required,supportNormals[side],sample.planted)||sample.positionWorld.y<required.y-.0005f)return reject(__LINE__);
            footTargets[side]=sample.positionWorld;planted[side]=sample.planted;
        }
        gaitGrounded=true;
        {
            // 身體盒在坡面較高處受支撐，雙脚較低；先調骨盆，再解兩條腿，避免伸長腿骨。
            float lowerPelvis=0;
            for(unsigned side=0;side<2;++side){
                const unsigned upper=9+side*3;
                const auto hip=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper],{}));
                const auto knee=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+1],{}));
                const auto ankle=TransformPoint(toWorld,TransformPoint(pose.globalMatrices[upper+2],{}));
                const float reach=Length(knee-hip)+Length(ankle-knee)-.002f;
                const auto target=footTargets[side];const float x=target.x-hip.x,z=target.z-hip.z;
                if(reach<=0||x*x+z*z>=reach*reach)return reject(__LINE__);
                lowerPelvis=std::max(lowerPelvis,hip.y-target.y-std::sqrt(reach*reach-x*x-z*z));
            }
            if(!std::isfinite(lowerPelvis)||lowerPelvis>.35f){
                const unsigned line=__LINE__;
                captureCandidate(ExplorerFootPlacementFailure::PelvisLowering,line,false);
                if(diagnostic)diagnostic->lowerPelvis=lowerPelvis;
                return reject(line);
            }
            pose.localTransforms[0].translation.y-=lowerPelvis;
            BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        }
        for(unsigned side=0;side<2;++side){
            const unsigned upper=9+side*3,middle=upper+1,end=upper+2;
            TwoBoneIkResult3D result;
            if(!SolveTwoBoneIk3D(model.skeleton,{upper,middle,end},
                {TransformPoint(toModel,footTargets[side]),{side?.12f:-.12f,.5f,1},1,0},pose,&result))return reject(__LINE__);
            // 求解器可能回傳被夾限的姿態；有凸形支撐時不可將未到達的腳當成成功。
            if(!std::isfinite(result.endError)||result.endError>=.005f)return reject(__LINE__);
            // 踝關節抵銷腿部旋轉，讓靴底朝向地面。
            const auto a=pose.localTransforms[upper].quaternion,b=pose.localTransforms[middle].quaternion;
            const auto q=NormalizeQuaternion3D({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
                a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
                a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
            pose.localTransforms[end].quaternion={-q.x,-q.y,-q.z,q.w};
            pose.localTransforms[end].usesQuaternion=true;
            BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
            const auto n=supportNormals[side];
            const auto tilt=NormalizeQuaternion3D({n.z,0,-n.x,1+n.y});
            const auto worldFoot=DeathMultiply(tilt,QuaternionFromEuler3D({0,footFacing[side],0}));
            DeathOrient(end,DeathMultiply(QuaternionFromEuler3D({0,-yaw,0}),worldFoot),pose);
        }
        return true;
    }
};
}
