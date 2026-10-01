#pragma once
#include "MistwatchEncounter.h"
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkeletalIk3D.h>
#include <Aevrynd/HeightField3D.h>
namespace Mistwatch {
// 本作原創四足野豬原型；模型、骨架及姿態均由目前來源重建，不使用舊人偶。
struct Boar {
    Aevrynd::SkinnedModel3D model;
    Aevrynd::AnimationPose3D pose;
    float yaw{},cycle{},displacement{},brace{};
    std::array<Aevrynd::Vec3,4> footTargets{};
    std::array<bool,4> planted{};
    Aevrynd::Vec3 lastFeet{};
    bool initialized{};
    static constexpr std::array<Aevrynd::Vec3,4> restFeet{{
        {-.25f,.075f,.43f},{.25f,.075f,.43f},{-.25f,.075f,-.52f},{.25f,.075f,-.52f}}};
    Boar() {
        using namespace Aevrynd;
        const std::array<Vec3,16> joints{{{0,.62f,0},{0,.75f,.49f},{0,.52f,.94f},
            {-.25f,.57f,.4f},{-.25f,.31f,.28f},restFeet[0],
            {.25f,.57f,.4f},{.25f,.31f,.28f},restFeet[1],
            {-.25f,.57f,-.49f},{-.25f,.31f,-.3f},restFeet[2],
            {.25f,.57f,-.49f},{.25f,.31f,-.3f},restFeet[3],{0,.75f,-.75f}}};
        constexpr int parents[]{-1,0,1,0,3,4,0,6,7,0,9,10,0,12,13,0};
        for(unsigned i=0;i<joints.size();++i) {
            JointTransform3D bind;bind.translation=parents[i]<0?joints[i]:joints[i]-joints[parents[i]];
            model.skeleton.joints.push_back({parents[i],bind});
            model.inverseBindMatrices.push_back(CreateTranslationMatrix(joints[i]*-1.f));
        }
        const auto sphere=Mesh3D::CreateUnitSphere(16,10),cone=Mesh3D::CreateUnitCone(10);
        const auto part=[&](unsigned joint,const Mesh3D& mesh,Vec3 center,Vec3 size,Vec3 rotation,Color color) {
            const auto base=static_cast<std::uint32_t>(model.bindMesh.vertices.size());
            const auto transform=CreateTransformMatrix(center,rotation,size),normals=CreateNormalMatrix(transform);
            for(auto vertex:mesh.vertices) {
                const auto p=TransformPoint(transform,{vertex.x,vertex.y,vertex.z});
                const auto n=UnpackNormalSnorm8(vertex.normal);
                vertex.x=p.x;vertex.y=p.y;vertex.z=p.z;
                vertex.normal=PackNormalSnorm8(TransformNormal(normals,{n.x,n.y,n.z}));
                vertex.color=PackColorRgba8(color);model.bindMesh.vertices.push_back(vertex);
                model.influences.push_back({{static_cast<std::uint16_t>(joint),0,0,0},{65535,0,0,0}});
            }
            for(auto index:mesh.indices)model.bindMesh.indices.push_back(base+index);
        };
        const Color hide{.23f,.15f,.085f,1},shoulder{.18f,.11f,.055f,1},hoof{.055f,.035f,.025f,1};
        part(0,sphere,{0,.65f,-.15f},{.74f,.72f,1.48f},{},hide);
        part(0,sphere,{0,.74f,.28f},{.81f,.73f,.77f},{},shoulder);
        part(1,sphere,{0,.68f,.67f},{.53f,.53f,.62f},{.2f,0,0},hide);
        part(2,sphere,{0,.49f,1.01f},{.39f,.28f,.36f},{},hide);
        part(2,sphere,{0,.49f,1.18f},{.35f,.24f,.08f},{},{.105f,.065f,.045f,1});
        for(float sign:{-1.f,1.f}) {
            part(1,cone,{sign*.22f,.91f,.53f},{.2f,.35f,.12f},{.1f,0,-sign*.35f},shoulder);
            part(1,sphere,{sign*.235f,.78f,.82f},{.055f,.055f,.075f},{},{.025f,.015f,.01f,1});
            part(1,sphere,{sign*.257f,.792f,.831f},{.013f,.013f,.02f},{},{.65f,.62f,.52f,1});
            part(2,sphere,{sign*.095f,.515f,1.219f},{.062f,.046f,.014f},{},hoof);
            part(2,cone,{sign*.225f,.5f,1.04f},{.09f,.22f,.085f},{-.45f,0,-sign*.5f},{.7f,.64f,.47f,1});
        }
        for(unsigned leg=0;leg<4;++leg) {
            const unsigned upper=3+leg*3;
            const auto a=joints[upper],b=joints[upper+1],c=joints[upper+2];
            part(upper,sphere,(a+b)*.5f,{.2f,.34f,.22f},{},hide);
            part(upper+1,sphere,(b+c)*.5f,{.13f,.3f,.16f},{},shoulder);
            // 分趾蹄保留兩瓣輪廓，蹄底在模型根部平面以上。
            for(float side:{-1.f,1.f})part(upper+2,sphere,c+Vec3{side*.035f,-.002f,.045f},{.067f,.14f,.19f},{},hoof);
        }
        part(15,cone,{0,.71f,-.87f},{.06f,.28f,.06f},{-1.3f,0,0},shoulder);
        for(unsigned tuft=0;tuft<11;++tuft) {
            const float z=-.65f+float(tuft)*.09f;
            part(0,cone,{0,1.04f,z},{.04f,.12f+.025f*std::sin(float(tuft)),.04f},{-.25f,0,0},hoof);
        }
        model.skeletonCompatibilityId=ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
        Update({},0,EnemyPhase::Pursuit,0,{});
    }
    bool Update(Aevrynd::Vec3 movement,float dt,EnemyPhase phase,float phaseTime,Aevrynd::Vec3 look) {
        using namespace Aevrynd;
        if(!Session::Finite(movement)||!Session::Finite(look)||!std::isfinite(dt)||dt<0||dt>.1f||
            !std::isfinite(phaseTime)||phaseTime<0||unsigned(phase)>unsigned(EnemyPhase::Defeated))return false;
        displacement=std::hypot(movement.x,movement.z);
        if(displacement>.0001f)cycle=std::fmod(cycle+displacement*6.f,6.2831853f);
        if(std::hypot(look.x,look.z)>.001f)yaw=std::atan2(look.x,look.z);
        brace=phase==EnemyPhase::Windup?std::clamp(1.f-phaseTime/Encounter::WindupDuration,0.f,1.f):0.f;
        pose.localTransforms.clear();
        for(const auto& joint:model.skeleton.joints)pose.localTransforms.push_back(joint.bindTransform);
        pose.localTransforms[0].translation.y-=.025f+.045f*brace;
        pose.localTransforms[1].rotation.x=phase==EnemyPhase::Recovery?-.13f:.12f+.36f*brace;
        pose.localTransforms[2].rotation.x=-.1f*brace;
        pose.localTransforms[15].rotation.z=.15f*std::sin(cycle);
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        return true;
    }
    bool PlaceFeet(Aevrynd::Vec3 feet,const Aevrynd::HeightField3D& terrain) {
        using namespace Aevrynd;
        if(!Session::Finite(feet))return false;
        const auto world=CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
        const auto local=CreateRotationMatrix({0,-yaw,0})*CreateTranslationMatrix(feet*-1.f);
        if(!initialized || Length(feet-lastFeet)>1.f){planted={};initialized=true;}
        lastFeet=feet;
        for(unsigned leg=0;leg<4;++leg) {
            // 前左／後右為同一對角支撐組，另兩足相差半個步態週期。
            const float phase=std::fmod(cycle+((leg==1||leg==2)?3.14159265f:0.f),6.2831853f);
            const bool moving=displacement>.0001f,support=!moving||phase<3.14159265f;
            const auto rest=TransformPoint(world,restFeet[leg]);
            if(support) {
                if(!planted[leg]||std::hypot(footTargets[leg].x-rest.x,footTargets[leg].z-rest.z)>.23f) {
                    footTargets[leg]=TransformPoint(world,restFeet[leg]+Vec3{0,0,moving?.13f:0.f});
                    planted[leg]=true;
                }
            } else {
                const float t=(phase-3.14159265f)/3.14159265f;
                planted[leg]=false;
                footTargets[leg]=TransformPoint(world,restFeet[leg]+Vec3{0,0,-.13f+.26f*t});
            }
            footTargets[leg].y=terrain.SampleHeight(footTargets[leg].x,footTargets[leg].z)+restFeet[leg].y+
                (support?0.f:.11f*std::sin(phase-3.14159265f));
            const unsigned upper=3+leg*3;
            // 前肢肘關節向後、後肢膝關節向前；共用引擎兩骨 IK，不直接移動蒙皮頂點。
            const Vec3 pole{restFeet[leg].x,.32f,leg<2?-.2f:.2f};
            if(!SolveTwoBoneIk3D(model.skeleton,{upper,upper+1,upper+2},
                {TransformPoint(local,footTargets[leg]),pole,1,0},pose))return false;
            const auto a=pose.localTransforms[upper].quaternion,b=pose.localTransforms[upper+1].quaternion;
            if(pose.localTransforms[upper].usesQuaternion&&pose.localTransforms[upper+1].usesQuaternion) {
                const Quaternion3D product{
                    a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
                auto& ankle=pose.localTransforms[upper+2];
                ankle.usesQuaternion=true;ankle.quaternion={-product.x,-product.y,-product.z,product.w};ankle.rotation={};
            }
        }
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        return true;
    }
};
}
