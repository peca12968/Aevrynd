#pragma once
#include "Mesh3D.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace Aevrynd {
enum class RigidBodyType3D : uint8_t {Static,Dynamic,Kinematic};
struct PhysicsMaterial3D {float friction{.58f},restitution{.08f};};
struct RigidBodyHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const RigidBodyHandle3D&)const=default;};
struct RigidBodySettings3D {RigidBodyType3D type{RigidBodyType3D::Static};Transform3D transform{};Vec3 colliderExtents{.5f,.5f,.5f};float mass{1.f},gravityScale{1.f};PhysicsMaterial3D material{};uint32_t layer{1},collisionMask{0xffffffffu};bool allowSleep{true};};
struct RigidBodyState3D {RigidBodyHandle3D handle{};RigidBodySettings3D settings{};Vec3 velocity{},accumulatedForce{};float sleepClock{};bool sleeping{};};
struct PhysicsWorldStats3D {std::size_t bodies{},staticBodies{},dynamicBodies{},kinematicBodies{},sleepingBodies{};uint64_t steps{},substeps{},broadPhaseTests{},contacts{},sleepTransitions{};};

class PhysicsWorld3D {
public:
    RigidBodyHandle3D CreateBody(RigidBodySettings3D settings);
    bool DestroyBody(RigidBodyHandle3D handle);
    void Clear();
    RigidBodyState3D* GetBody(RigidBodyHandle3D handle);
    const RigidBodyState3D* GetBody(RigidBodyHandle3D handle)const;
    bool SetTransform(RigidBodyHandle3D handle,Transform3D transform,bool wake=true);
    bool SetVelocity(RigidBodyHandle3D handle,Vec3 velocity);
    bool AddForce(RigidBodyHandle3D handle,Vec3 force);
    bool ApplyImpulse(RigidBodyHandle3D handle,Vec3 impulse);
    void Step(float deltaTime,unsigned substeps=1);
    void SetGravity(Vec3 gravity){gravity_=gravity;}
    Vec3 Gravity()const{return gravity_;}
    PhysicsWorldStats3D Stats()const;
private:
    static bool LayersMatch(const RigidBodyState3D& left,const RigidBodyState3D& right);
    static bool Overlaps(const RigidBodyState3D& left,const RigidBodyState3D& right);
    void ResolveAxis(RigidBodyState3D& body,float deltaTime,int axis);
    std::unordered_map<uint32_t,RigidBodyState3D> bodies_;
    Vec3 gravity_{0,-9.81f,0};PhysicsWorldStats3D stats_{};uint32_t nextHandle_{1};
};
}
