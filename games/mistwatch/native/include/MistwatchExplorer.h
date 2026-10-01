#pragma once
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkeletalIk3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <array>
namespace Mistwatch {
// 本作獨立製作的穿衣角色原型；不依賴舊人物資產。
struct Explorer {
    Aevrynd::SkinnedModel3D model;
    Aevrynd::AnimationPose3D pose;
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
        const auto sphere=Mesh3D::CreateUnitSphere(12,8);
        const auto part=[&](unsigned joint,Vec3 center,Vec3 size,Color color){
            const auto first=static_cast<std::uint32_t>(model.bindMesh.vertices.size());
            const auto transform=CreateTransformMatrix(center,{},size);
            const auto normals=CreateNormalMatrix(transform);
            for(auto v:sphere.vertices) {
                const auto p=TransformPoint(transform,{v.x,v.y,v.z});
                const auto n=UnpackNormalSnorm8(v.normal);
                v.x=p.x;v.y=p.y;v.z=p.z;
                v.normal=PackNormalSnorm8(TransformNormal(normals,{n.x,n.y,n.z}));
                v.color=PackColorRgba8(color);model.bindMesh.vertices.push_back(v);
                model.influences.push_back({{static_cast<std::uint16_t>(joint),0,0,0},{65535,0,0,0}});
            }
            for(auto index:sphere.indices)model.bindMesh.indices.push_back(first+index);
        };
        const Color coat{.24f,.32f,.22f,1},pants{.14f,.17f,.19f,1},boots{.07f,.06f,.05f,1},skin{.56f,.36f,.24f,1};
        part(0,{0,.92f,0},{.35f,.27f,.25f},pants);
        part(1,{0,1.25f,0},{.47f,.55f,.29f},coat);
        part(1,{0,1.27f,-.19f},{.32f,.4f,.19f},{.36f,.23f,.12f,1});
        part(2,{0,1.62f,0},{.23f,.3f,.23f},skin);
        part(2,{0,1.73f,-.015f},{.25f,.12f,.26f},coat);
        for(unsigned side=0;side<2;++side) {
            const float x=side?1.f:-1.f;
            const unsigned arm=3+side*3,leg=9+side*3;
            part(arm,{x*.265f,1.25f,0},{.16f,.36f,.17f},coat);
            part(arm+1,{x*.28f,.98f,0},{.13f,.3f,.14f},coat);
            part(arm+2,{x*.28f,.8f,0},{.105f,.16f,.12f},boots);
            part(leg,{x*.12f,.69f,0},{.22f,.45f,.23f},pants);
            part(leg+1,{x*.12f,.29f,0},{.17f,.43f,.18f},pants);
            part(leg+2,{x*.12f,.09f,.06f},{.18f,.18f,.32f},boots);
        }
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
