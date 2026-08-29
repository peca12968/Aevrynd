#pragma once
#include <Aevrynd/Animation3D.h>
#include <cstdint>

namespace GameplayLab {
enum class AnimationState3D : std::uint8_t {Idle,Walk,Run,Crouch,Jump,Fall,Attack1,Attack2,Attack3,Hit,Death};
class GameplayAnimation3D {
public:
    GameplayAnimation3D();
    void SetState(AnimationState3D state,bool restart=false);
    void Tick(float deltaTime);
    [[nodiscard]] AnimationState3D State()const{return state_;}
    [[nodiscard]] const Aevrynd::Skeleton3D& Skeleton()const{return skeleton_;}
    [[nodiscard]] const Aevrynd::AnimationPose3D& Pose()const{return pose_;}
private:
    void Evaluate();
    Aevrynd::Skeleton3D skeleton_{};Aevrynd::AnimationPose3D pose_{};AnimationState3D state_{AnimationState3D::Idle};float time_{};
};
}
