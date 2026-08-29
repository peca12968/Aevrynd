#pragma once
#include "Geometry3D.h"
#include "Memory.h"
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Aevrynd {
inline constexpr uint32_t CollisionLayerAll3D=0xffffffffu;
struct BoxCollider3D {Vec3 center{},extents{.5f,.5f,.5f};};
struct ColliderHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const ColliderHandle3D&)const=default;};
struct StaticBoxCollider3D {BoxCollider3D bounds{};uint32_t layer{1},collisionMask{CollisionLayerAll3D};uint64_t userData{};bool trigger{};};
struct CollisionQueryFilter3D {uint32_t layerMask{CollisionLayerAll3D},queryLayer{1};bool includeTriggers{};};
struct CollisionRayHit3D {ColliderHandle3D collider{};float distance{std::numeric_limits<float>::infinity()};Vec3 position{},normal{};uint64_t userData{};explicit operator bool()const{return bool(collider);}};
struct CollisionWorldStats3D {std::size_t colliders{},solidColliders{},triggers{};uint64_t queries{},boundsTested{},hits{};};

class CollisionWorld3D {
public:
    ColliderHandle3D CreateBox(StaticBoxCollider3D collider);
    bool UpdateBox(ColliderHandle3D handle,StaticBoxCollider3D collider);
    bool Destroy(ColliderHandle3D handle);
    void Clear();
    std::size_t TrimUnusedMemory();
    ReusableMemoryStats MemoryStats()const;
    const StaticBoxCollider3D* Get(ColliderHandle3D handle)const;
    CollisionRayHit3D Raycast(Ray3D ray,float maximumDistance=std::numeric_limits<float>::infinity(),CollisionQueryFilter3D filter={})const;
    bool OverlapsBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter={})const;
    std::vector<ColliderHandle3D> QueryBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter={})const;
    void CollectBoxes(std::vector<BoxCollider3D>& output,CollisionQueryFilter3D filter={})const;
    CollisionWorldStats3D Stats()const;
    bool CanRebaseOrigin(Vec3 delta)const noexcept;
    void RebaseOrigin(Vec3 delta)noexcept;
private:
    bool Matches(const StaticBoxCollider3D& collider,CollisionQueryFilter3D filter)const;
    std::unordered_map<uint32_t,StaticBoxCollider3D> colliders_;
    mutable CollisionWorldStats3D stats_{};
    std::size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
    uint32_t nextHandle_{1};
};
}
