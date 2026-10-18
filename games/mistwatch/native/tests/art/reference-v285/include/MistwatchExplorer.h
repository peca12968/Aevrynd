#pragma once
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkeletalIk3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <array>
#include "MistwatchExplorerArtMesh.h"
namespace Mistwatch {
// 本作獨立製作的穿衣角色原型；不依賴舊人物資產。
struct Explorer {
    Aevrynd::SkinnedModel3D model;
    Aevrynd::AnimationPose3D pose;
    ExplorerArtReceipt art;
    float phase{},yaw{};
    std::array<Aevrynd::Vec3,2> footTargets{};
    std::array<bool,2> planted{};
    Aevrynd::Vec3 lastFeet{};
    bool feetInitialized{};
    float locomotionDistance{};
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
        model.skeletonCompatibilityId=ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
        Update({},0,false);
    }
    void Update(Aevrynd::Vec3 displacement,float dt,bool crouched) {
        using namespace Aevrynd;
        const float distance=std::hypot(displacement.x,displacement.z);
        locomotionDistance=distance;
        if(distance>.0001f){yaw=std::atan2(displacement.x,displacement.z);phase=std::fmod(phase+distance*5.f,6.2831853f);}
        pose.localTransforms.clear();
        for(const auto& joint:model.skeleton.joints)pose.localTransforms.push_back(joint.bindTransform);
        const float stride=dt>0 && distance>.0001f?.5f*std::sin(phase):0.f;
        for(unsigned side=0;side<2;++side) {
            const float swing=side?-stride:stride;
            pose.localTransforms[9+side*3].rotation.x=swing;
            pose.localTransforms[10+side*3].rotation.x=(std::max)(0.f,-swing)*.8f;
            pose.localTransforms[3+side*3].rotation.x=-swing*.7f;
        }
        if(crouched){
            pose.localTransforms[0].translation.y-=.45f;
            pose.localTransforms[1].rotation.x=.3f;
            for(unsigned side=0;side<2;++side){pose.localTransforms[9+side*3].rotation.x-=.8f;pose.localTransforms[10+side*3].rotation.x+=1.4f;}
        }
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
    }
    // 接地使用控制器相同的高度場及設施；支撐期保存世界座標以抑制滑步。
    bool PlaceFeet(Aevrynd::Vec3 feet,bool grounded,bool crouched,
        const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids) {
        using namespace Aevrynd;
        const auto toWorld=CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
        const auto toModel=CreateRotationMatrix({0,-yaw,0})*CreateTranslationMatrix(feet*-1.f);
        if(!feetInitialized || Length(feet-lastFeet)>1.f){planted={};feetInitialized=true;}
        lastFeet=feet;
        if(!grounded){planted={};return true;}
        // 直腿保留少量伸展餘裕；蹲姿骨盆高度由既有姿態提供。
        if(!crouched)pose.localTransforms[0].translation.y-=.09f;
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        const auto heightAt=[&](Vec3 p){
            float height=terrain.SampleHeight(p.x,p.z);
            for(const auto& solid:solids){
                const float top=solid.center.y+solid.extents.y;
                if(std::abs(p.x-solid.center.x)<=solid.extents.x && std::abs(p.z-solid.center.z)<=solid.extents.z &&
                    top<=feet.y+.4f && top>height)height=top;
            }
            return height;
        };
        for(unsigned side=0;side<2;++side){
            const float cycle=std::fmod(phase+float(side)*3.14159265f,6.2831853f);
            const bool moving=locomotionDistance>.0001f;
            const bool support=!moving || cycle<3.14159265f;
            Vec3 rest=TransformPoint(toWorld,{side?.12f:-.12f,.1f,0});
            if(support){
                // 不可讓支撐點超出腿長；急轉或速度過高時重新接地。
                if(!planted[side] || std::hypot(footTargets[side].x-rest.x,footTargets[side].z-rest.z)>.32f){
                    footTargets[side]=TransformPoint(toWorld,{side?.12f:-.12f,.1f,moving?.2f:0.f});
                    planted[side]=true;
                }
            }else{
                planted[side]=false;
                const float t=(cycle-3.14159265f)/3.14159265f;
                footTargets[side]=TransformPoint(toWorld,{side?.12f:-.12f,.1f,-.2f+.4f*t});
            }
            const float lift=support?0.f:.14f*std::sin(cycle-3.14159265f);
            footTargets[side].y=heightAt(footTargets[side])+.1f+lift;
            const unsigned upper=9+side*3,middle=upper+1,end=upper+2;
            TwoBoneIkResult3D result;
            if(!SolveTwoBoneIk3D(model.skeleton,{upper,middle,end},
                {TransformPoint(toModel,footTargets[side]),{side?.12f:-.12f,.5f,1},1,0},pose,&result))return false;
            // 踝關節抵銷腿部旋轉，讓靴底朝向地面。
            const auto a=pose.localTransforms[upper].quaternion,b=pose.localTransforms[middle].quaternion;
            const auto q=NormalizeQuaternion3D({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
                a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
                a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
            pose.localTransforms[end].quaternion={-q.x,-q.y,-q.z,q.w};
            pose.localTransforms[end].usesQuaternion=true;
            BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        }
        return true;
    }
};
}
