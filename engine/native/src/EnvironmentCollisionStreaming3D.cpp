#include "Aevrynd/EnvironmentCollisionStreaming3D.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <memory>
#include <new>
#include <utility>

namespace Aevrynd {
namespace {

bool Finite(Vec3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

void HashScalar(std::uint64_t &hash, std::uint64_t value) noexcept {
  for (unsigned byte = 0; byte < 8; ++byte) {
    hash ^= (value >> (byte * 8)) & 255u;
    hash *= 1099511628211ull;
  }
}

void HashFloat(std::uint64_t &hash, float value) noexcept {
  // IEEE +0/-0 語意相同，先正規化以維持跨重建的 canonical hash。
  if (value == 0.f)
    value = 0.f;
  HashScalar(hash, std::bit_cast<std::uint32_t>(value));
}

BoxCollider3D RotatedBounds(const EnvironmentColliderDescriptor3D &value) {
  const float sine = std::abs(std::sin(value.yaw));
  const float cosine = std::abs(std::cos(value.yaw));
  return {value.center,
          {cosine * value.extents.x + sine * value.extents.z,
           value.extents.y,
           sine * value.extents.x + cosine * value.extents.z}};
}

} // namespace

struct EnvironmentCollisionStreaming3D::PartitionRecord {
  EnvironmentCollisionPartition3D partition{};
  std::vector<EnvironmentColliderDescriptor3D> descriptors;
  std::unique_ptr<CollisionWorld3D> collision;
  std::vector<RigidBodyHandle3D> physicsBodies;
  std::vector<CollisionObjectRef3D> solidObjects;
};

EnvironmentCollisionStreaming3D::EnvironmentCollisionStreaming3D(
    PhysicsWorld3D &physics, EnvironmentCollisionStreamingSettings3D settings)
    : physics_(physics), settings_(settings) {
  if (!settings_.maximumPartitions)
    settings_.maximumPartitions = 1;
  if (!settings_.maximumRegisteredDescriptors)
    settings_.maximumRegisteredDescriptors = 1;
  if (!settings_.maximumActiveColliders)
    settings_.maximumActiveColliders = 1;
  if (!std::isfinite(settings_.deactivationDistanceScale) ||
      settings_.deactivationDistanceScale < 1.f)
    settings_.deactivationDistanceScale = 1.f;
  // 非有限材質值會在第一次接觸時污染共享物理世界，先回復安全預設值。
  settings_.staticMaterial.friction = Clamp(
      std::isfinite(settings_.staticMaterial.friction)
          ? settings_.staticMaterial.friction
          : .72f,
      0.f, 1.f);
  settings_.staticMaterial.restitution = Clamp(
      std::isfinite(settings_.staticMaterial.restitution)
          ? settings_.staticMaterial.restitution
          : .03f,
      0.f, 1.f);
  partitions_.reserve(settings_.maximumPartitions);
}

EnvironmentCollisionStreaming3D::~EnvironmentCollisionStreaming3D() {
  [[maybe_unused]] const auto released = Release();
}

bool EnvironmentCollisionStreaming3D::IsSolid(
    EnvironmentColliderSemantic3D semantic) noexcept {
  switch (semantic) {
  case EnvironmentColliderSemantic3D::Terrain:
  case EnvironmentColliderSemantic3D::Tree:
  case EnvironmentColliderSemantic3D::Rock:
  case EnvironmentColliderSemantic3D::Building:
    return true;
  case EnvironmentColliderSemantic3D::Water:
  case EnvironmentColliderSemantic3D::Grass:
  case EnvironmentColliderSemantic3D::Decoration:
    return false;
  }
  return false;
}

bool EnvironmentCollisionStreaming3D::RegisterPartition(
    EnvironmentCollisionPartition3D partition,
    std::span<const EnvironmentColliderDescriptor3D> descriptors) {
  const auto duplicate = std::lower_bound(
      partitions_.begin(), partitions_.end(), partition.id,
      [](const PartitionRecord &value, std::uint64_t id) {
        return value.partition.id < id;
      });
  if (!partition.id || !Finite(partition.center) ||
      !std::isfinite(partition.activationDistance) ||
      partition.activationDistance < 0 || descriptors.empty() ||
      !std::isfinite(partition.activationDistance *
                     settings_.deactivationDistanceScale) ||
      (duplicate != partitions_.end() &&
       duplicate->partition.id == partition.id) ||
      partitions_.size() >= settings_.maximumPartitions ||
      descriptors.size() >
          settings_.maximumRegisteredDescriptors - registeredDescriptors_) {
    ++invalidRequests_;
    return false;
  }

  std::vector<EnvironmentColliderDescriptor3D> candidate;
  try {
    candidate.assign(descriptors.begin(), descriptors.end());
    std::ranges::sort(candidate, {}, &EnvironmentColliderDescriptor3D::id);
  } catch (...) {
    ++invalidRequests_;
    return false;
  }
  for (std::size_t index = 0; index < candidate.size(); ++index) {
    const auto &value = candidate[index];
    const bool semanticValid =
        value.semantic <= EnvironmentColliderSemantic3D::Decoration;
    if (!semanticValid || value.id.high != partition.id ||
        value.id.low != index + 1 || !Finite(value.center) ||
        !Finite(value.extents) || !std::isfinite(value.yaw) ||
        value.extents.x <= 0 || value.extents.y <= 0 || value.extents.z <= 0 ||
        (IsSolid(value.semantic) &&
         (!value.layer || !value.collisionMask)) ||
        !Finite(RotatedBounds(value).extents)) {
      ++invalidRequests_;
      return false;
    }
  }

  PartitionRecord record{};
  record.partition = partition;
  record.descriptors = std::move(candidate);
  try {
    partitions_.insert(duplicate, std::move(record));
  } catch (...) {
    ++invalidRequests_;
    return false;
  }
  registeredDescriptors_ += descriptors.size();
  ++generation_;
  return true;
}

bool EnvironmentCollisionStreaming3D::Activate(PartitionRecord &partition) {
  const std::size_t solidCount = std::ranges::count_if(
      partition.descriptors, [](const auto &value) {
        return IsSolid(value.semantic);
      });
  if (solidCount > settings_.maximumActiveColliders - activeColliders_) {
    ++failedActivations_;
    return false;
  }

  std::unique_ptr<CollisionWorld3D> candidate;
  try {
    candidate = std::make_unique<CollisionWorld3D>();
  } catch (...) {
    ++failedActivations_;
    return false;
  }
  if (!candidate->PrepareStable(partition.descriptors.size()) ||
      !candidate->SetIdentityMode(CollisionWorldIdentityMode3D::AuthoritativeStable,
                                  {partition.partition.id, 1, false})) {
    ++failedActivations_;
    return false;
  }
  std::vector<RigidBodyHandle3D> bodies;
  std::vector<CollisionObjectRef3D> solidObjects;
  try {
    bodies.reserve(solidCount);
    solidObjects.reserve(solidCount);
  } catch (...) {
    ++failedActivations_;
    return false;
  }

  std::size_t checkpoint{};
  try {
    for (const auto &descriptor : partition.descriptors) {
      // 非 solid descriptor 仍保留 stable ID，但不應進入碰撞或物理世界。
      if (!IsSolid(descriptor.semantic))
        continue;
      if (activationFailureCheckpoint_ >= 0 &&
          checkpoint++ ==
              static_cast<std::size_t>(activationFailureCheckpoint_))
        throw std::bad_alloc{};
      const BoxCollider3D bounds = RotatedBounds(descriptor);
      CollisionStableCreateResult3D created{};
      // 每個分區的 low ID 必須連續；略過非 solid 時以實際建立序號映射。
      const StableObjectId3D physicalId{partition.partition.id,
                                       bodies.size() + 1};
      if (!candidate->CreateStableBox(
              physicalId, 0,
              {bounds, descriptor.layer, descriptor.collisionMask,
               descriptor.userData, false},
              created))
        throw std::bad_alloc{};
      RigidBodySettings3D settings{};
      settings.type = RigidBodyType3D::Static;
      settings.transform.position = bounds.center;
      settings.colliderExtents = bounds.extents;
      settings.material = settings_.staticMaterial;
      settings.layer = descriptor.layer;
      settings.collisionMask = descriptor.collisionMask;
      bodies.push_back(physics_.CreateBody(settings));
      solidObjects.push_back({descriptor.id, 1});
    }
  } catch (...) {
    for (RigidBodyHandle3D body : bodies)
      physics_.DestroyBody(body);
    ++failedActivations_;
    ++rollbacks_;
    return false;
  }
  partition.collision = std::move(candidate);
  partition.physicsBodies = std::move(bodies);
  partition.solidObjects = std::move(solidObjects);
  activeColliders_ += solidCount;
  ++activations_;
  ++generation_;
  return true;
}

void EnvironmentCollisionStreaming3D::Deactivate(
    PartitionRecord &partition) noexcept {
  if (!partition.collision)
    return;
  for (RigidBodyHandle3D body : partition.physicsBodies)
    physics_.DestroyBody(body);
  activeColliders_ -= partition.physicsBodies.size();
  std::vector<RigidBodyHandle3D>{}.swap(partition.physicsBodies);
  std::vector<CollisionObjectRef3D>{}.swap(partition.solidObjects);
  partition.collision.reset();
  ++deactivations_;
  ++generation_;
}

bool EnvironmentCollisionStreaming3D::UpdateObserver(Vec3 position) {
  if (!Finite(position)) {
    ++invalidRequests_;
    return false;
  }
  ++updates_;
  bool succeeded = true;
  for (auto &partition : partitions_) {
    const Vec3 delta = position - partition.partition.center;
    const double distanceSquared = double(delta.x) * delta.x +
                                   double(delta.y) * delta.y +
                                   double(delta.z) * delta.z;
    const double limit = partition.partition.activationDistance *
                         (partition.collision
                              ? settings_.deactivationDistanceScale
                              : 1.f);
    const bool desired = std::isfinite(distanceSquared) &&
                         distanceSquared <= limit * limit;
    if (desired && !partition.collision)
      succeeded &= Activate(partition);
    else if (!desired && partition.collision)
      Deactivate(partition);
  }
  return succeeded;
}

bool EnvironmentCollisionStreaming3D::UnregisterPartition(
    std::uint64_t partitionId) {
  const auto found = std::lower_bound(
      partitions_.begin(), partitions_.end(), partitionId,
      [](const PartitionRecord &value, std::uint64_t id) {
        return value.partition.id < id;
      });
  if (found == partitions_.end() || found->partition.id != partitionId) {
    ++invalidRequests_;
    return false;
  }
  Deactivate(*found);
  registeredDescriptors_ -= found->descriptors.size();
  partitions_.erase(found);
  ++generation_;
  return true;
}

EnvironmentCollisionQueryHit3D EnvironmentCollisionStreaming3D::Raycast(
    Ray3D ray, float maximumDistance, CollisionQueryFilter3D filter) const {
  EnvironmentCollisionQueryHit3D result{};
  for (const auto &partition : partitions_) {
    if (!partition.collision)
      continue;
    const auto hit = partition.collision->Raycast(ray, maximumDistance, filter);
    if (!hit)
      continue;
    CollisionObjectRef3D object{};
    if (!partition.collision->Resolve(hit.collider, object))
      continue;
    if (!object.id.low || object.id.low > partition.solidObjects.size())
      continue;
    object = partition.solidObjects[object.id.low - 1];
    if (hit.distance < result.distance ||
        hit.distance == result.distance && object < result.object)
      result = {object, hit.distance, hit.position, hit.normal, hit.userData,
                false};
  }
  return result;
}

EnvironmentCollisionQueryHit3D EnvironmentCollisionStreaming3D::SweepSphere(
    Vec3 center, float radius, Vec3 displacement,
    CollisionQueryFilter3D filter) const {
  EnvironmentCollisionQueryHit3D result{};
  for (const auto &partition : partitions_) {
    if (!partition.collision)
      continue;
    const auto hit =
        partition.collision->SweepSphere(center, radius, displacement, filter);
    if (!hit)
      continue;
    CollisionObjectRef3D object{};
    if (!partition.collision->Resolve(hit.collider, object))
      continue;
    if (!object.id.low || object.id.low > partition.solidObjects.size())
      continue;
    object = partition.solidObjects[object.id.low - 1];
    if (hit.distance < result.distance ||
        hit.distance == result.distance && object < result.object)
      result = {object, hit.distance, hit.position, hit.normal, hit.userData,
                hit.startedOverlapping};
  }
  return result;
}

bool EnvironmentCollisionStreaming3D::OverlapsBox(
    Vec3 center, Vec3 extents, CollisionQueryFilter3D filter) const {
  return std::ranges::any_of(partitions_, [&](const PartitionRecord &partition) {
    return partition.collision &&
           partition.collision->OverlapsBox(center, extents, filter);
  });
}

std::vector<CollisionObjectRef3D> EnvironmentCollisionStreaming3D::QueryBox(
    Vec3 center, Vec3 extents, CollisionQueryFilter3D filter) const {
  std::vector<CollisionObjectRef3D> result;
  for (const auto &partition : partitions_) {
    if (!partition.collision)
      continue;
    for (ColliderHandle3D local :
         partition.collision->QueryBox(center, extents, filter)) {
      CollisionObjectRef3D object{};
      if (partition.collision->Resolve(local, object) && object.id.low &&
          object.id.low <= partition.solidObjects.size())
        result.push_back(partition.solidObjects[object.id.low - 1]);
    }
  }
  std::ranges::sort(result);
  return result;
}

std::uint64_t EnvironmentCollisionStreaming3D::StateHash() const noexcept {
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto &partition : partitions_) {
    HashScalar(hash, partition.partition.id);
    HashFloat(hash, partition.partition.center.x);
    HashFloat(hash, partition.partition.center.y);
    HashFloat(hash, partition.partition.center.z);
    HashFloat(hash, partition.partition.activationDistance);
    HashScalar(hash, partition.collision ? 1 : 0);
    for (const auto &value : partition.descriptors) {
      HashScalar(hash, value.id.high);
      HashScalar(hash, value.id.low);
      HashScalar(hash, static_cast<std::uint64_t>(value.semantic));
      HashFloat(hash, value.center.x);
      HashFloat(hash, value.center.y);
      HashFloat(hash, value.center.z);
      HashFloat(hash, value.extents.x);
      HashFloat(hash, value.extents.y);
      HashFloat(hash, value.extents.z);
      HashFloat(hash, value.yaw);
      HashScalar(hash, value.layer);
      HashScalar(hash, value.collisionMask);
      HashScalar(hash, value.userData);
    }
  }
  return hash;
}

std::size_t EnvironmentCollisionStreaming3D::RetainedBytes() const noexcept {
  std::size_t bytes = partitions_.capacity() * sizeof(PartitionRecord);
  for (const auto &partition : partitions_) {
    bytes += partition.descriptors.capacity() *
             sizeof(EnvironmentColliderDescriptor3D);
    bytes += partition.physicsBodies.capacity() * sizeof(RigidBodyHandle3D);
    bytes += partition.solidObjects.capacity() * sizeof(CollisionObjectRef3D);
    if (partition.collision)
      bytes += partition.collision->MemoryStats().retainedCapacityBytes;
  }
  return bytes;
}

EnvironmentCollisionStreamingReceipt3D
EnvironmentCollisionStreaming3D::Receipt() const {
  EnvironmentCollisionStreamingReceipt3D result{};
  result.stateHash = StateHash();
  result.partitions = partitions_.size();
  result.registeredDescriptors = registeredDescriptors_;
  result.activeColliders = activeColliders_;
  result.activePhysicsBodies = activeColliders_;
  result.retainedBytes = RetainedBytes();
  result.updates = updates_;
  result.activations = activations_;
  result.deactivations = deactivations_;
  result.failedActivations = failedActivations_;
  result.rollbacks = rollbacks_;
  result.invalidRequests = invalidRequests_;
  result.generation = generation_;
  for (const auto &partition : partitions_) {
    result.activePartitions += partition.collision != nullptr;
    for (const auto &descriptor : partition.descriptors) {
      if (IsSolid(descriptor.semantic))
        ++result.solidDescriptors;
      else
        ++result.nonSolidDescriptors;
    }
  }
  return result;
}

bool EnvironmentCollisionStreaming3D::IsPartitionActive(
    std::uint64_t partitionId) const {
  const auto found = std::lower_bound(
      partitions_.begin(), partitions_.end(), partitionId,
      [](const PartitionRecord &value, std::uint64_t id) {
        return value.partition.id < id;
      });
  return found != partitions_.end() && found->partition.id == partitionId &&
         found->collision != nullptr;
}

EnvironmentCollisionReleaseReceipt3D
EnvironmentCollisionStreaming3D::Release() {
  EnvironmentCollisionReleaseReceipt3D result{};
  result.stateHashBefore = StateHash();
  result.partitionsReleased = partitions_.size();
  result.descriptorsReleased = registeredDescriptors_;
  result.collidersReleased = activeColliders_;
  result.physicsBodiesReleased = activeColliders_;
  result.retainedBytesBefore = RetainedBytes();
  for (auto &partition : partitions_)
    Deactivate(partition);
  std::vector<PartitionRecord>{}.swap(partitions_);
  registeredDescriptors_ = 0;
  activeColliders_ = 0;
  ++generation_;
  result.stateHashAfter = StateHash();
  result.retainedBytesAfter = RetainedBytes();
  return result;
}

} // namespace Aevrynd
