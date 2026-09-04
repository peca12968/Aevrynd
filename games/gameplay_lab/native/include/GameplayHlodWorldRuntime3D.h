#pragma once

#include "ShowcaseRenderAssetBindings3D.h"

#include <Aevrynd/Camera.h>
#include <Aevrynd/Hlod3D.h>
#include <Aevrynd/Scene3D.h>

#include <cstdint>
#include <memory>
#include <span>

namespace Aevrynd {
class Engine;
class DrawList;
class SceneECSRegistry3D;
}

namespace GameplayLab {

// Gameplay 環境來源必須位於一般物件 identity domain；最高位只保留給 HLOD proxy。
inline constexpr std::uint64_t GameplayEnvironmentIdentityDomain3D =
    0x2ec047454e560001ull;
inline constexpr Aevrynd::StableObjectId3D GameplayHlodWorldIdentity3D{
    GameplayEnvironmentIdentityDomain3D, 0x484c4f44574f524cull};

[[nodiscard]] constexpr bool GameplayEnvironmentIdentityContract3D(
    Aevrynd::StableObjectId3D id) noexcept {
  return id && id.high == GameplayEnvironmentIdentityDomain3D &&
      !(id.high & Aevrynd::HlodProxyIdentityDomainBit3D);
}

[[nodiscard]] constexpr Aevrynd::StableObjectId3D
GameplayEnvironmentObjectIdentity3D(std::uint64_t descriptorOrdinal) noexcept {
  return descriptorOrdinal
      ? Aevrynd::StableObjectId3D{GameplayEnvironmentIdentityDomain3D,
                                  descriptorOrdinal}
      : Aevrynd::StableObjectId3D{};
}

static_assert(GameplayEnvironmentIdentityContract3D(
    GameplayEnvironmentObjectIdentity3D(1)));
static_assert(Aevrynd::HlodProxyRenderObjectId3D(
                  GameplayEnvironmentObjectIdentity3D(1)) !=
              GameplayEnvironmentObjectIdentity3D(1));

struct GameplayHlodWorldRuntimeReceipt3D {
  ShowcaseProductionHlodBuildReceipt3D build{};
  Aevrynd::WorldPartitionStats3D partition{};
  Aevrynd::HlodIntegrationStats3D integration{};
  Aevrynd::HlodRuntimeFrameStats3D frame{};
  Aevrynd::SceneECSCollectionStats3D collection{};
  std::uint64_t frozenFrame{};
  bool worldPartitionResident{}, selected{}, frozen{}, collected{};
  // 第一階段刻意記錄尚未達成的範圍，避免把代表性低階模型誤報成正式聚合 proxy。
  bool gameplayLabOnRenderConnected{}, aggregateProxyBuilt{},
      allDescriptorKindsCovered{};

  [[nodiscard]] constexpr bool SeamComplete() const noexcept {
    return build.Complete() && worldPartitionResident && selected && frozen &&
        collected && frozenFrame == frame.frameId && frame.frozen &&
        integration.attachedClusters == build.clusters &&
        collection.submittedRenderables != 0;
  }
};

// 這是 Gameplay 正式資產到既有 HLOD/WorldPartition/SceneECS 的可重用接線層；
// 呼叫端負責在 Scene3D::Submit 前追加 CollectFrame。
class GameplayHlodWorldRuntime3D final {
public:
  GameplayHlodWorldRuntime3D();
  ~GameplayHlodWorldRuntime3D();
  GameplayHlodWorldRuntime3D(const GameplayHlodWorldRuntime3D &) = delete;
  GameplayHlodWorldRuntime3D &operator=(const GameplayHlodWorldRuntime3D &) = delete;

  bool Initialize(Aevrynd::Engine &engine,
                  Aevrynd::SceneECSRegistry3D &registry,
                  ShowcaseRenderAssetBindings3D &assets,
                  std::span<const ShowcaseSceneDescriptor3D> descriptors,
                  Aevrynd::StableObjectId3D worldId);
  bool CollectFrame(std::uint64_t frameId, const Aevrynd::Camera3D &camera,
                    Aevrynd::Vec2 viewport, Aevrynd::Scene3D &scene,
                    Aevrynd::DrawList &draw);
  void Shutdown();
  [[nodiscard]] bool Initialized() const noexcept;
  [[nodiscard]] GameplayHlodWorldRuntimeReceipt3D Receipt() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // GameplayLab 命名空間
