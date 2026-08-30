#pragma once

#include "CollisionWorld3D.h"
#include "PhysicsWorld3D.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace Aevrynd {

enum class EnvironmentColliderSemantic3D : std::uint8_t {
  Terrain,
  Tree,
  Rock,
  Building,
  Water,
  Grass,
  Decoration
};

struct EnvironmentColliderDescriptor3D {
  StableObjectId3D id{};
  EnvironmentColliderSemantic3D semantic{};
  Vec3 center{};
  Vec3 extents{.5f, .5f, .5f};
  float yaw{};
  std::uint32_t layer{1};
  std::uint32_t collisionMask{CollisionLayerAll3D};
  std::uint64_t userData{};
};

struct EnvironmentCollisionPartition3D {
  std::uint64_t id{};
  Vec3 center{};
  float activationDistance{96.f};
};

struct EnvironmentCollisionStreamingSettings3D {
  std::size_t maximumPartitions{256};
  std::size_t maximumRegisteredDescriptors{65536};
  std::size_t maximumActiveColliders{8192};
  float deactivationDistanceScale{1.2f};
  PhysicsMaterial3D staticMaterial{.72f, .03f};
};

struct EnvironmentCollisionQueryHit3D {
  CollisionObjectRef3D object{};
  float distance{std::numeric_limits<float>::infinity()};
  Vec3 position{}, normal{};
  std::uint64_t userData{};
  bool startedOverlapping{};
  explicit operator bool() const { return bool(object); }
};

struct EnvironmentCollisionStreamingReceipt3D {
  std::uint64_t stateHash{};
  std::size_t partitions{}, registeredDescriptors{}, solidDescriptors{};
  std::size_t nonSolidDescriptors{}, activePartitions{}, activeColliders{};
  std::size_t activePhysicsBodies{}, retainedBytes{};
  std::uint64_t updates{}, activations{}, deactivations{}, failedActivations{};
  std::uint64_t rollbacks{}, invalidRequests{}, generation{};
};

struct EnvironmentCollisionReleaseReceipt3D {
  std::uint64_t stateHashBefore{}, stateHashAfter{};
  std::size_t partitionsReleased{}, descriptorsReleased{};
  std::size_t collidersReleased{}, physicsBodiesReleased{};
  std::size_t retainedBytesBefore{}, retainedBytesAfter{};
};

// 依分區管理大型環境的靜態碰撞；渲染 descriptor 與物理資源生命週期保持解耦。
class EnvironmentCollisionStreaming3D {
public:
  explicit EnvironmentCollisionStreaming3D(
      PhysicsWorld3D &physics,
      EnvironmentCollisionStreamingSettings3D settings = {});
  ~EnvironmentCollisionStreaming3D();
  EnvironmentCollisionStreaming3D(const EnvironmentCollisionStreaming3D &) =
      delete;
  EnvironmentCollisionStreaming3D &
  operator=(const EnvironmentCollisionStreaming3D &) = delete;

  [[nodiscard]] bool RegisterPartition(
      EnvironmentCollisionPartition3D partition,
      std::span<const EnvironmentColliderDescriptor3D> descriptors);
  [[nodiscard]] bool UnregisterPartition(std::uint64_t partitionId);
  [[nodiscard]] bool UpdateObserver(Vec3 position);

  [[nodiscard]] EnvironmentCollisionQueryHit3D
  Raycast(Ray3D ray,
          float maximumDistance = std::numeric_limits<float>::infinity(),
          CollisionQueryFilter3D filter = {}) const;
  [[nodiscard]] EnvironmentCollisionQueryHit3D
  SweepSphere(Vec3 center, float radius, Vec3 displacement,
              CollisionQueryFilter3D filter = {}) const;
  [[nodiscard]] bool OverlapsBox(Vec3 center, Vec3 extents,
                                 CollisionQueryFilter3D filter = {}) const;
  [[nodiscard]] std::vector<CollisionObjectRef3D>
  QueryBox(Vec3 center, Vec3 extents,
           CollisionQueryFilter3D filter = {}) const;

  [[nodiscard]] EnvironmentCollisionStreamingReceipt3D Receipt() const;
  [[nodiscard]] EnvironmentCollisionReleaseReceipt3D Release();
  [[nodiscard]] bool IsPartitionActive(std::uint64_t partitionId) const;

  // 僅供失敗交易測試；負值代表停用，零代表第一個 collider 前失敗。
  void SetActivationFailureCheckpointForTesting(int checkpoint) noexcept {
    activationFailureCheckpoint_ = checkpoint;
  }

private:
  struct PartitionRecord;
  [[nodiscard]] bool Activate(PartitionRecord &partition);
  void Deactivate(PartitionRecord &partition) noexcept;
  [[nodiscard]] static bool IsSolid(EnvironmentColliderSemantic3D semantic)
      noexcept;
  [[nodiscard]] std::uint64_t StateHash() const noexcept;
  [[nodiscard]] std::size_t RetainedBytes() const noexcept;

  PhysicsWorld3D &physics_;
  EnvironmentCollisionStreamingSettings3D settings_{};
  std::vector<PartitionRecord> partitions_;
  std::size_t registeredDescriptors_{}, activeColliders_{};
  std::uint64_t updates_{}, activations_{}, deactivations_{};
  std::uint64_t failedActivations_{}, rollbacks_{}, invalidRequests_{};
  std::uint64_t generation_{};
  int activationFailureCheckpoint_{-1};
};

} // namespace Aevrynd
