#pragma once
#include "MistwatchBoar.h"
#include <limits>
#include <numbers>

namespace Mistwatch {
class BoarDefeatView {
public:
    static constexpr float Duration=.78f;

    void Clear() noexcept {visible_=false;}
    [[nodiscard]] bool Visible() const noexcept {return visible_;}
    [[nodiscard]] const Aevrynd::AnimationPose3D& Pose() const noexcept {return pose_;}
    [[nodiscard]] const Aevrynd::Matrix4& Transform() const noexcept {return transform_;}

    void Commit(float dt,unsigned previousHealth,const Encounter& enemy,bool guardianDefeated,
                bool playerAlive,const Boar& boar,const Aevrynd::HeightField3D& terrain) {
        if(!guardianDefeated){Clear();return;}
        if(previousHealth>0&&enemy.health==0&&enemy.phase==EnemyPhase::Defeated&&playerAlive){
            Begin(enemy.feet,boar,terrain);
        }else if(visible_&&elapsed_<Duration&&std::isfinite(dt)&&dt>0){
            const float next=(std::min)(Duration,elapsed_+(std::min)(dt,.1f));
            Aevrynd::Matrix4 candidate;
            float lift{};
            if(Placement(next,candidate,lift,terrain)){
                elapsed_=next;
                transform_=candidate;
            }
        }
    }

private:
    void Begin(Aevrynd::Vec3 feet,const Boar& boar,const Aevrynd::HeightField3D& terrain) {
        using namespace Aevrynd;
        Clear();
        if(!Session::Finite(feet)||!std::isfinite(boar.yaw)||
           !SkinModelInto(boar.model,boar.pose,skinning_,surface_))return;
        feet_=feet;
        yaw_=boar.yaw;
        elapsed_=0.f;
        initialLift_=0.f;
        const auto side=TransformPoint(CreateRotationMatrix({0,yaw_,0}),{.55f,0,0});
        const auto left=feet_-side;
        const auto right=feet_+side;
        if(!terrain.Contains(left.x,left.z)||!terrain.Contains(right.x,right.z))return;
        // 朝較低的一側倒下。平地以同一側開始，讓重播一致。
        side_=terrain.SampleHeight(left.x,left.z)<=terrain.SampleHeight(right.x,right.z)?1.f:-1.f;
        Aevrynd::Matrix4 checked;
        if(!Placement(0.f,checked,initialLift_,terrain))return;
        transform_=CreateTransformMatrix(feet_,{0,yaw_,0},{1,1,1});
        pose_=boar.pose;
        visible_=true;
    }

    bool Placement(float elapsed,Aevrynd::Matrix4& out,float& lift,
                   const Aevrynd::HeightField3D& terrain) const noexcept {
        using namespace Aevrynd;
        const float t=elapsed/Duration;
        const float lean=t*t*(3.f-2.f*t);
        const auto offset=TransformPoint(CreateRotationMatrix({0,yaw_,0}),{-.28f*side_*lean,0,0});
        auto position=feet_+offset;
        const Vec3 angles{.08f*lean,yaw_,side_*(std::numbers::pi_v<float>*.5f)*lean};
        const auto unplaced=CreateTransformMatrix(position,angles,{1,1,1});
        // 以同一個蒙皮姿勢的表面取樣，讓斜坡上實際先接觸地面的部位承重。
        lift=-(std::numeric_limits<float>::infinity)();
        for(const auto& vertex:surface_.vertices){
            const auto point=TransformPoint(unplaced,{vertex.x,vertex.y,vertex.z});
            if(!Session::Finite(point)||!terrain.Contains(point.x,point.z))return false;
            lift=(std::max)(lift,terrain.SampleHeight(point.x,point.z)-point.y);
        }
        if(!std::isfinite(lift))return false;
        const float settle=(std::min)(1.f,t*5.f);
        position.y+=lift-initialLift_*(1.f-settle);
        out=CreateTransformMatrix(position,angles,{1,1,1});
        return true;
    }

    Aevrynd::SkinningWorkspace3D skinning_;
    Aevrynd::Mesh3D surface_;
    Aevrynd::AnimationPose3D pose_;
    Aevrynd::Matrix4 transform_{};
    Aevrynd::Vec3 feet_{};
    float elapsed_{},yaw_{},side_{1.f},initialLift_{};
    bool visible_{};
};
}
