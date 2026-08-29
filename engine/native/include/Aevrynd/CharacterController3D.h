#pragma once
#include "Camera.h"
#include "CollisionWorld3D.h"
#include "HeightField3D.h"
#include <cstdint>
#include <span>
#include <vector>

namespace Aevrynd {
struct CharacterControl3D {Vec3 movement{};bool jump{};};
struct CharacterControllerSettings3D {
    float radius{.45f};
    float height{1.8f};
    float moveSpeed{6.f};
    float jumpSpeed{7.f};
    float gravity{20.f};
    float maximumFallSpeed{35.f};
    float groundSnapDistance{.4f};
};
struct CharacterControllerStats3D {uint64_t steps{},collisions{},jumps{},landings{},terrainContacts{};};

class CharacterController3D {
public:
    explicit CharacterController3D(CharacterControllerSettings3D settings={}):settings_(settings){}
    void Reset(Vec3 feetPosition={});
    void Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,float deltaTime);
    void Step(CharacterControl3D control,const CollisionWorld3D& world,float deltaTime,uint32_t layerMask=0xffffffffu,uint32_t characterLayer=1);
    void Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,const HeightField3D& terrain,float deltaTime);
    void Step(CharacterControl3D control,const CollisionWorld3D& world,const HeightField3D& terrain,float deltaTime,uint32_t layerMask=0xffffffffu,uint32_t characterLayer=1);
    Vec3 Position()const{return position_;}
    Vec3 Velocity()const{return velocity_;}
    Vec3 Center()const{return position_+Vec3{0,settings_.height*.5f,0};}
    Vec3 Extents()const{return{settings_.radius,settings_.height*.5f,settings_.radius};}
    bool Grounded()const{return grounded_;}
    bool SetHeight(float height){if(!std::isfinite(height)||height<.6f||height>3.f)return false;settings_.height=height;return true;}
    const CharacterControllerSettings3D& Settings()const{return settings_;}
    CharacterControllerStats3D Stats()const{return stats_;}
    bool CanRebaseOrigin(Vec3 delta)const noexcept;
    void RebaseOrigin(Vec3 delta)noexcept;
private:
    CharacterControllerSettings3D settings_{};
    CharacterControllerStats3D stats_{};
    Vec3 position_{},velocity_{};
    bool grounded_{};
    std::vector<BoxCollider3D> collisionScratch_;
};

Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,std::span<const BoxCollider3D> colliders,float padding=.2f);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const CollisionWorld3D& world,float padding=.2f,uint32_t layerMask=0xffffffffu,uint32_t cameraLayer=1);
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const HeightField3D& terrain,float padding=.2f,unsigned samples=32);
}
