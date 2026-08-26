#pragma once
#include "Camera.h"
#include <cstdint>
#include <span>

namespace Aevrynd {
struct BoxCollider3D {Vec3 center{},extents{.5f,.5f,.5f};};
struct CharacterControl3D {Vec3 movement{};bool jump{};};
struct CharacterControllerSettings3D {
    float radius{.45f};
    float height{1.8f};
    float moveSpeed{6.f};
    float jumpSpeed{7.f};
    float gravity{20.f};
    float maximumFallSpeed{35.f};
};
struct CharacterControllerStats3D {uint64_t steps{},collisions{},jumps{},landings{};};

class CharacterController3D {
public:
    explicit CharacterController3D(CharacterControllerSettings3D settings={}):settings_(settings){}
    void Reset(Vec3 feetPosition={});
    void Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,float deltaTime);
    Vec3 Position()const{return position_;}
    Vec3 Velocity()const{return velocity_;}
    Vec3 Center()const{return position_+Vec3{0,settings_.height*.5f,0};}
    Vec3 Extents()const{return{settings_.radius,settings_.height*.5f,settings_.radius};}
    bool Grounded()const{return grounded_;}
    const CharacterControllerSettings3D& Settings()const{return settings_;}
    CharacterControllerStats3D Stats()const{return stats_;}
private:
    CharacterControllerSettings3D settings_{};
    CharacterControllerStats3D stats_{};
    Vec3 position_{},velocity_{};
    bool grounded_{};
};

Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,std::span<const BoxCollider3D> colliders,float padding=.2f);
}
