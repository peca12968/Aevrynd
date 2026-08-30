#include <Aevrynd/EnvironmentCollisionStreaming3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;

namespace {

std::vector<EnvironmentColliderDescriptor3D>
MakeShowcaseDescriptors(std::uint64_t partitionId) {
  std::vector<EnvironmentColliderDescriptor3D> result;
  result.reserve(156);
  auto add = [&](EnvironmentColliderSemantic3D semantic, Vec3 center,
                 Vec3 extents, std::uint64_t userData = 0, float yaw = 0.f) {
    result.push_back({{partitionId, result.size() + 1}, semantic, center,
                      extents, yaw, 1, CollisionLayerAll3D, userData});
  };
  add(EnvironmentColliderSemantic3D::Terrain, {0, -1, 0}, {220, .5f, 220},
      1001);
  for (int index = 0; index < 17; ++index)
    add(EnvironmentColliderSemantic3D::Water,
        {-180.f + index * 8.f, 10, 180}, {3, .1f, 5}, 2000 + index);
  for (int index = 0; index < 72; ++index)
    add(EnvironmentColliderSemantic3D::Tree,
        {40.f + float(index % 12) * 8.f, 3.f,
         -80.f + float(index / 12) * 12.f},
        {.45f, 3.f, .45f}, 3000 + index);
  for (int index = 0; index < 48; ++index)
    add(EnvironmentColliderSemantic3D::Grass,
        {-180.f + float(index % 12) * 2.f, 10,
         160.f + float(index / 12) * 2.f},
        {.4f, .6f, .4f}, 4000 + index);
  for (int index = 0; index < 12; ++index)
    add(EnvironmentColliderSemantic3D::Rock,
        {-80.f + float(index) * 5.f, 1.f, -40}, {1.5f, 1.f, 1.1f},
        5000 + index, index == 0 ? .78539816f : 0.f);
  for (int index = 0; index < 6; ++index)
    add(EnvironmentColliderSemantic3D::Building,
        {20.f + float(index) * 9.f, 2.5f, 25}, {3.f, 2.5f, 3.f},
        6000 + index);
  return result;
}

bool Contains(std::span<const CollisionObjectRef3D> values,
              StableObjectId3D id) {
  return std::ranges::any_of(values,
                             [&](const auto &value) { return value.id == id; });
}

int Fail(int code, const char *message) {
  std::cerr << message << '\n';
  return code;
}

} // namespace

int main() {
  constexpr std::uint64_t PartitionId = 0xaec00001ull;
  const auto descriptors = MakeShowcaseDescriptors(PartitionId);
  if (descriptors.size() != 156)
    return Fail(1, "Showcase descriptor fixture count changed");

  PhysicsWorld3D physics;
  EnvironmentCollisionStreaming3D streaming(
      physics, {8, 512, 256, 1.2f, {.8f, .02f}});
  const auto initialHash = streaming.Receipt().stateHash;
  if (!streaming.RegisterPartition({PartitionId, {0, 0, 0}, 260},
                                   descriptors))
    return Fail(2, "Showcase partition registration failed");
  const auto registered = streaming.Receipt();
  if (registered.partitions != 1 || registered.registeredDescriptors != 156 ||
      registered.solidDescriptors != 91 ||
      registered.nonSolidDescriptors != 65 || registered.activeColliders ||
      physics.Stats().bodies)
    return Fail(3, "Registered semantic counts are incorrect");

  if (!streaming.UpdateObserver({0, 2, 0}))
    return Fail(4, "Near observer activation failed");
  const auto active = streaming.Receipt();
  if (active.activePartitions != 1 || active.activeColliders != 91 ||
      active.activePhysicsBodies != 91 || physics.Stats().staticBodies != 91 ||
      active.activations != 1)
    return Fail(5, "Active collision/physics counts are incorrect");

  const auto terrain = streaming.Raycast(MakeRay3D({0, 8, 0}, {0, -1, 0}), 20);
  if (!terrain || terrain.object.id != StableObjectId3D{PartitionId, 1} ||
      terrain.userData != 1001 || std::abs(terrain.position.y + .5f) > .001f)
    return Fail(6, "Terrain ray query lost stable identity");
  const StableObjectId3D firstTreeId{PartitionId, 19};
  const auto tree = streaming.Raycast(MakeRay3D({40, 3, -90}, {0, 0, 1}), 20);
  if (!tree || tree.object.id != firstTreeId || tree.userData != 3000)
    return Fail(7, "Tree ray query returned internal rather than public ID");
  const auto melee = streaming.QueryBox({40, 3, -80}, {1.5f, 2.f, 1.5f});
  if (!Contains(melee, firstTreeId))
    return Fail(8, "Melee overlap query missed a tree collider");
  if (streaming.OverlapsBox({-180, 10, 180}, {1, .05f, 1}) ||
      streaming.OverlapsBox({-180, 10, 160}, {.2f, .2f, .2f}))
    return Fail(9, "Water or grass was incorrectly registered as solid");

  // 動態物件代表角色與拖曳生成物；共享 PhysicsWorld 必須能與地形接觸。
  RigidBodySettings3D spawned{};
  spawned.type = RigidBodyType3D::Dynamic;
  spawned.transform.position = {0, 6, 0};
  spawned.colliderExtents = {.45f, .5f, .45f};
  spawned.mass = 1.f;
  const auto spawnedHandle = physics.CreateBody(spawned);
  for (int frame = 0; frame < 360; ++frame)
    physics.Step(1.f / 120.f, 1);
  const auto *settled = physics.GetBody(spawnedHandle);
  if (!settled || settled->settings.transform.position.y < -.01f ||
      settled->settings.transform.position.y > .02f ||
      physics.Stats().contacts == 0)
    return Fail(10, "Spawned object did not settle on streamed terrain");
  physics.DestroyBody(spawnedHandle);

  // 角色型動態剛體高速移動時仍應由建築物的連續碰撞阻擋。
  RigidBodySettings3D character{};
  character.type = RigidBodyType3D::Dynamic;
  character.transform.position = {10, 2.5f, 25};
  character.colliderExtents = {.5f, 1.f, .5f};
  character.gravityScale = 0;
  const auto characterHandle = physics.CreateBody(character);
  physics.SetVelocity(characterHandle, {45, 0, 0});
  for (int frame = 0; frame < 8; ++frame)
    physics.Step(.1f, 1);
  const auto *blocked = physics.GetBody(characterHandle);
  if (!blocked || blocked->settings.transform.position.x > 16.51f)
    return Fail(23, "Character body tunneled through a streamed building");
  physics.DestroyBody(characterHandle);
  const auto rockSweep =
      streaming.SweepSphere({-80, 1, -48}, .4f, {0, 0, 16});
  if (!rockSweep || rockSweep.object.id != StableObjectId3D{PartitionId, 139})
    return Fail(24, "Weapon/object sweep missed the rotated rock collider");

  const auto activeHash = streaming.Receipt().stateHash;
  if (!streaming.UpdateObserver({1000, 0, 1000}) ||
      streaming.IsPartitionActive(PartitionId) ||
      physics.Stats().bodies != 0)
    return Fail(11, "Distance unload retained collision resources");
  if (!streaming.UpdateObserver({0, 0, 0}) ||
      streaming.Receipt().stateHash != activeHash ||
      physics.Stats().staticBodies != 91)
    return Fail(12, "Reload was not deterministic");
  for (int cycle = 0; cycle < 8; ++cycle) {
    if (!streaming.UpdateObserver({1000, 0, 1000}) ||
        !streaming.UpdateObserver({0, 0, 0}))
      return Fail(13, "Repeated load/unload cycle failed");
  }
  if (physics.Stats().bodies != 91)
    return Fail(14, "Repeated streaming leaked physics bodies");

  if (!streaming.UpdateObserver({1000, 0, 1000}))
    return Fail(15, "Rollback setup unload failed");
  const auto rollbackHash = streaming.Receipt().stateHash;
  streaming.SetActivationFailureCheckpointForTesting(5);
  if (streaming.UpdateObserver({0, 0, 0}) ||
      streaming.IsPartitionActive(PartitionId) || physics.Stats().bodies != 0 ||
      streaming.Receipt().stateHash != rollbackHash ||
      streaming.Receipt().rollbacks != 1)
    return Fail(16, "Activation failure was not atomically rolled back");
  streaming.SetActivationFailureCheckpointForTesting(-1);
  if (!streaming.UpdateObserver({0, 0, 0}))
    return Fail(17, "Activation did not recover after rollback");

  const auto stableBeforeInvalid = streaming.Receipt().stateHash;
  const float nan = std::numeric_limits<float>::quiet_NaN();
  auto invalidDescriptors = descriptors;
  for (auto &descriptor : invalidDescriptors)
    descriptor.id.high = 2;
  invalidDescriptors[0].extents.x = nan;
  if (streaming.RegisterPartition({2, {}, 10}, invalidDescriptors) ||
      streaming.UpdateObserver({nan, 0, 0}) ||
      streaming.Receipt().stateHash != stableBeforeInvalid ||
      streaming.RegisterPartition({PartitionId, {}, 10}, descriptors))
    return Fail(18, "Invalid/duplicate request changed semantic state");

  PhysicsWorld3D cappedPhysics;
  EnvironmentCollisionStreaming3D capped(
      cappedPhysics, {2, 200, 90, 1.1f, {}});
  if (!capped.RegisterPartition({PartitionId, {}, 100}, descriptors) ||
      capped.UpdateObserver({}) || capped.Receipt().activeColliders ||
      cappedPhysics.Stats().bodies || capped.Receipt().failedActivations != 1)
    return Fail(19, "Capacity rejection was not atomic");

  PhysicsWorld3D secondPhysics;
  EnvironmentCollisionStreaming3D second(
      secondPhysics, {8, 512, 256, 1.2f, {.8f, .02f}});
  if (!second.RegisterPartition({PartitionId, {0, 0, 0}, 260}, descriptors) ||
      !second.UpdateObserver({0, 0, 0}) ||
      second.Receipt().stateHash != activeHash ||
      second.QueryBox({0, 0, 0}, {250, 20, 250}) !=
          streaming.QueryBox({0, 0, 0}, {250, 20, 250}))
    return Fail(20, "Equivalent worlds produced different receipts/queries");

  const auto beforeRelease = streaming.Receipt();
  const auto released = streaming.Release();
  if (released.partitionsReleased != 1 ||
      released.descriptorsReleased != 156 || released.collidersReleased != 91 ||
      released.physicsBodiesReleased != 91 || released.retainedBytesAfter != 0 ||
      streaming.Receipt().partitions || streaming.Receipt().activeColliders ||
      physics.Stats().bodies || released.stateHashAfter != initialHash)
    return Fail(21, "Release retained module or physics resources");
  [[maybe_unused]] const auto secondReleased = second.Release();
  [[maybe_unused]] const auto cappedReleased = capped.Release();
  physics.TrimUnusedMemory();
  secondPhysics.TrimUnusedMemory();
  cappedPhysics.TrimUnusedMemory();
  if (physics.MemoryStats().reclaimableCapacityBytes ||
      secondPhysics.MemoryStats().reclaimableCapacityBytes ||
      cappedPhysics.MemoryStats().reclaimableCapacityBytes)
    return Fail(22, "Physics capacity remained reclaimable after trim");

  // 敵意資料不得把 NaN 材質係數帶進共享物理世界，否則一次接觸就會污染速度。
  {
    PhysicsWorld3D hostilePhysics;
    const auto sentinel = hostilePhysics.CreateBody({});
    EnvironmentCollisionStreamingSettings3D hostileSettings{};
    hostileSettings.staticMaterial = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()};
    EnvironmentCollisionStreaming3D hostile(hostilePhysics, hostileSettings);
    const std::array hostileDescriptors{
        EnvironmentColliderDescriptor3D{{700, 1},
                                        EnvironmentColliderSemantic3D::Terrain,
                                        {0, -1, 0}, {8, .5f, 8}, 0, 1,
                                        CollisionLayerAll3D, 7001}};
    if (!hostile.RegisterPartition({700, {}, 10}, hostileDescriptors) ||
        !hostile.UpdateObserver({}))
      return Fail(25, "Finite-material hostile setup failed");
    RigidBodySettings3D falling{};
    falling.type = RigidBodyType3D::Dynamic;
    falling.transform.position = {0, 2, 0};
    falling.colliderExtents = {.25f, .25f, .25f};
    const auto fallingHandle = hostilePhysics.CreateBody(falling);
    hostilePhysics.SetVelocity(fallingHandle, {2, 0, 0});
    for (int frame = 0; frame < 240; ++frame)
      hostilePhysics.Step(1.f / 120.f, 1);
    const auto *finiteBody = hostilePhysics.GetBody(fallingHandle);
    if (!finiteBody || !std::isfinite(finiteBody->velocity.x) ||
        !std::isfinite(finiteBody->velocity.y) ||
        !std::isfinite(finiteBody->velocity.z))
      return Fail(26, "Non-finite material contaminated shared physics");
    hostilePhysics.DestroyBody(fallingHandle);
    const auto hostileRelease = hostile.Release();
    if (!hostilePhysics.GetBody(sentinel) || hostilePhysics.Stats().bodies != 1 ||
        hostileRelease.physicsBodiesReleased != 1)
      return Fail(27, "Release destroyed or miscounted foreign physics body");
    hostilePhysics.DestroyBody(sentinel);
  }

  // descriptor 輸入順序不影響 canonical ID；重複、缺號與錯誤語意必須原子拒絕。
  {
    PhysicsWorld3D orderPhysics;
    EnvironmentCollisionStreaming3D ordered(orderPhysics, {8, 64, 32, 1.2f, {}});
    std::array<EnvironmentColliderDescriptor3D, 4> values{{
        {{800, 1}, EnvironmentColliderSemantic3D::Building, {0, 1, 0},
         {1, 1, 1}, 0, 1, CollisionLayerAll3D, 8001},
        {{800, 2}, EnvironmentColliderSemantic3D::Water, {4, 1, 0},
         {1, 1, 1}, 0, 1, CollisionLayerAll3D, 8002},
        {{800, 3}, EnvironmentColliderSemantic3D::Grass, {8, 1, 0},
         {1, 1, 1}, 0, 1, CollisionLayerAll3D, 8003},
        {{800, 4}, EnvironmentColliderSemantic3D::Tree, {12, 1, 0},
         {1, 1, 1}, 0, 1, CollisionLayerAll3D, 8004},
    }};
    std::ranges::reverse(values);
    if (!ordered.RegisterPartition({800, {}, 10}, values) ||
        !ordered.UpdateObserver({}))
      return Fail(28, "Descriptor reordering was not canonicalized");
    const auto canonical = ordered.QueryBox({6, 1, 0}, {20, 2, 2});
    if (canonical != std::vector<CollisionObjectRef3D>{{{800, 1}, 1},
                                                       {{800, 4}, 1}})
      return Fail(29, "Canonical public IDs or non-solid semantics changed");
    const auto activeReference = canonical.front();
    for (int cycle = 0; cycle < 64; ++cycle) {
      if (!ordered.UpdateObserver({cycle & 1 ? 10.01f : 9.99f, 0, 0}) ||
          !ordered.IsPartitionActive(800))
        return Fail(30, "Observer boundary oscillation defeated hysteresis");
    }
    if (ordered.Receipt().activations != 1 ||
        ordered.Receipt().deactivations != 0 ||
        !ordered.UpdateObserver({12, 0, 0}) ||
        !ordered.IsPartitionActive(800) ||
        !ordered.UpdateObserver({12.01f, 0, 0}) ||
        ordered.IsPartitionActive(800) || ordered.QueryBox({}, {20, 20, 20}).size())
      return Fail(31, "Activation/deactivation boundary contract changed");
    if (!ordered.UpdateObserver({}) ||
        ordered.QueryBox({0, 1, 0}, {2, 2, 2}) !=
            std::vector<CollisionObjectRef3D>{activeReference})
      return Fail(32, "Active reference was not stable across unload/reload");

    auto duplicate = values;
    for (auto &value : duplicate)
      value.id.high = 801;
    duplicate[0].id.low = duplicate[1].id.low;
    auto gap = values;
    for (auto &value : gap)
      value.id.high = 802;
    gap[0].id.low = 9;
    auto invalidSemantic = values;
    for (auto &value : invalidSemantic)
      value.id.high = 803;
    invalidSemantic[0].semantic =
        static_cast<EnvironmentColliderSemantic3D>(255);
    auto overflow = values;
    for (auto &value : overflow)
      value.id.high = 804;
    overflow[0].extents = {(std::numeric_limits<float>::max)(), 1,
                           (std::numeric_limits<float>::max)()};
    overflow[0].yaw = .78539816f;
    const auto beforeInvalid = ordered.Receipt();
    if (ordered.RegisterPartition({801, {}, 10}, duplicate) ||
        ordered.RegisterPartition({802, {}, 10}, gap) ||
        ordered.RegisterPartition({803, {}, 10}, invalidSemantic) ||
        ordered.RegisterPartition(
            {804, {}, std::numeric_limits<float>::infinity()}, overflow) ||
        ordered.RegisterPartition({804, {}, 10}, overflow) ||
        ordered.RegisterPartition(
            {804, {std::numeric_limits<float>::infinity(), 0, 0}, 10},
            overflow) ||
        ordered.Receipt().partitions != beforeInvalid.partitions ||
        ordered.Receipt().registeredDescriptors !=
            beforeInvalid.registeredDescriptors ||
        ordered.Receipt().stateHash != beforeInvalid.stateHash)
      return Fail(33, "Duplicate/gap/semantic rejection was not atomic");
    [[maybe_unused]] const auto release = ordered.Release();
  }

  // 註冊容量失敗不能插入半套 partition 或消耗 descriptor 配額。
  {
    PhysicsWorld3D registrationPhysics;
    EnvironmentCollisionStreaming3D registrationCap(
        registrationPhysics, {1, 1, 1, 1.2f, {}});
    const std::array first{EnvironmentColliderDescriptor3D{
        {850, 1}, EnvironmentColliderSemantic3D::Terrain, {}, {1, 1, 1}}};
    const std::array overCapacity{EnvironmentColliderDescriptor3D{
        {851, 1}, EnvironmentColliderSemantic3D::Terrain, {}, {1, 1, 1}}};
    if (!registrationCap.RegisterPartition({850, {}, 10}, first))
      return Fail(39, "Registration-capacity fixture failed");
    const auto beforeCapacity = registrationCap.Receipt();
    if (registrationCap.RegisterPartition({851, {}, 10}, overCapacity) ||
        registrationCap.Receipt().partitions != beforeCapacity.partitions ||
        registrationCap.Receipt().registeredDescriptors !=
            beforeCapacity.registeredDescriptors ||
        registrationCap.Receipt().stateHash != beforeCapacity.stateHash)
      return Fail(40, "Registration capacity failure was not atomic");
    [[maybe_unused]] const auto release = registrationCap.Release();
  }

  // 所有 solid checkpoint 都要保證零殘留，且共享 world 中的外部 body 不受影響。
  {
    for (int checkpoint = 0; checkpoint < 2; ++checkpoint) {
      PhysicsWorld3D checkpointPhysics;
      const auto foreign = checkpointPhysics.CreateBody({});
      EnvironmentCollisionStreaming3D checkpointWorld(checkpointPhysics);
      const std::array checkpointDescriptors{
          EnvironmentColliderDescriptor3D{{900, 1},
                                          EnvironmentColliderSemantic3D::Terrain,
                                          {}, {1, 1, 1}},
          EnvironmentColliderDescriptor3D{{900, 2},
                                          EnvironmentColliderSemantic3D::Water,
                                          {3, 0, 0}, {1, 1, 1}},
          EnvironmentColliderDescriptor3D{{900, 3},
                                          EnvironmentColliderSemantic3D::Tree,
                                          {6, 0, 0}, {1, 1, 1}}};
      if (!checkpointWorld.RegisterPartition({900, {}, 10}, checkpointDescriptors))
        return Fail(34, "Checkpoint fixture registration failed");
      const auto before = checkpointWorld.Receipt().stateHash;
      checkpointWorld.SetActivationFailureCheckpointForTesting(checkpoint);
      if (checkpointWorld.UpdateObserver({}) ||
          checkpointWorld.IsPartitionActive(900) ||
          checkpointWorld.Receipt().stateHash != before ||
          checkpointWorld.Receipt().rollbacks != 1 ||
          checkpointPhysics.Stats().bodies != 1 ||
          !checkpointPhysics.GetBody(foreign))
        return Fail(35, "Checkpoint rollback leaked or damaged shared ownership");
      [[maybe_unused]] const auto release = checkpointWorld.Release();
      checkpointPhysics.DestroyBody(foreign);
    }
  }

  // 不同 partition 可重用 low ID，但 public high ID 必須消除碰撞並維持排序。
  {
    PhysicsWorld3D crossPhysics;
    EnvironmentCollisionStreaming3D cross(crossPhysics);
    const std::array left{EnvironmentColliderDescriptor3D{
        {1000, 1}, EnvironmentColliderSemantic3D::Terrain, {}, {1, 1, 1}}};
    const std::array right{EnvironmentColliderDescriptor3D{
        {1001, 1}, EnvironmentColliderSemantic3D::Terrain, {}, {1, 1, 1}}};
    if (!cross.RegisterPartition({1001, {}, 10}, right) ||
        !cross.RegisterPartition({1000, {}, 10}, left) ||
        !cross.UpdateObserver({}) ||
        cross.QueryBox({}, {2, 2, 2}) !=
            std::vector<CollisionObjectRef3D>{{{1000, 1}, 1},
                                              {{1001, 1}, 1}})
      return Fail(36, "Cross-partition stable IDs collided or reordered");
    [[maybe_unused]] const auto release = cross.Release();
  }

  // +0 與 -0 是同一場景狀態，至少 50 次重建都必須得到同一 receipt。
  {
    std::uint64_t deterministicHash{};
    for (int iteration = 0; iteration < 64; ++iteration) {
      PhysicsWorld3D deterministicPhysics;
      EnvironmentCollisionStreaming3D deterministic(deterministicPhysics);
      const float signedZero = iteration & 1 ? -0.f : 0.f;
      const std::array value{EnvironmentColliderDescriptor3D{
          {1100, 1}, EnvironmentColliderSemantic3D::Building,
          {signedZero, 0, 0}, {1, 1, 1}}};
      if (!deterministic.RegisterPartition({1100, {signedZero, 0, 0}, 10},
                                           value) ||
          !deterministic.UpdateObserver({}))
        return Fail(37, "Determinism fixture failed");
      if (!iteration)
        deterministicHash = deterministic.Receipt().stateHash;
      else if (deterministic.Receipt().stateHash != deterministicHash)
        return Fail(38, "Equivalent signed-zero worlds changed state hash");
      [[maybe_unused]] const auto release = deterministic.Release();
    }
  }

  std::cout << "descriptors=" << beforeRelease.registeredDescriptors
            << " solids=" << beforeRelease.solidDescriptors
            << " nonSolids=" << beforeRelease.nonSolidDescriptors
            << " activations=" << beforeRelease.activations
            << " deactivations=" << beforeRelease.deactivations
            << " rollbacks=" << beforeRelease.rollbacks
            << " stateHash=" << activeHash
            << " releasedBytes=" << released.retainedBytesBefore << '\n';
  return 0;
}
