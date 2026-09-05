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

enum class GameplaySceneEcsSubmissionOwner3D : std::uint8_t {
  None,
  HlodWorldRuntime
};

[[nodiscard]] constexpr const char *GameplaySceneEcsSubmissionOwnerName3D(
    GameplaySceneEcsSubmissionOwner3D value) noexcept {
  switch (value) {
  case GameplaySceneEcsSubmissionOwner3D::None:
    return "None";
  case GameplaySceneEcsSubmissionOwner3D::HlodWorldRuntime:
    return "HlodWorldRuntime";
  }
  return "None";
}

enum class GameplayHlodWorldReadiness3D : std::uint8_t {
  RetryableNotReady,
  Ready,
  Fatal
};

enum class GameplayHlodWorldTerminalReason3D : std::uint8_t {
  None,
  FirstFramePending,
  RetryLimitExceeded,
  RuntimeUnavailable,
  FrameInvariantViolation,
  IdentityInvariantViolation,
  DataInvariantViolation
};

enum class GameplayHlodWorldFailureInjection3D : std::uint8_t {
  None,
  IdentityInvariant,
  DataInvariant,
  LateDataInvariant
};

// 測試控制仍走真實 WorldPartition 與 activation target；只控制來源完成時機與
// 指定失敗點，避免用 constexpr/helper 冒充 runtime 證據。
struct GameplayHlodWorldRuntimeOptions3D {
  std::uint32_t sourceCompletionDelayPolls{}, prewarmUpdates{};
  GameplayHlodWorldFailureInjection3D failureInjection{
      GameplayHlodWorldFailureInjection3D::None};
  std::uint64_t failureFrame{};
};

[[nodiscard]] constexpr const char *GameplayHlodWorldReadinessName3D(
    GameplayHlodWorldReadiness3D value) noexcept {
  switch (value) {
  case GameplayHlodWorldReadiness3D::RetryableNotReady:
    return "RetryableNotReady";
  case GameplayHlodWorldReadiness3D::Ready:
    return "Ready";
  case GameplayHlodWorldReadiness3D::Fatal:
    return "Fatal";
  }
  return "Fatal";
}

[[nodiscard]] constexpr const char *GameplayHlodWorldTerminalReasonName3D(
    GameplayHlodWorldTerminalReason3D value) noexcept {
  switch (value) {
  case GameplayHlodWorldTerminalReason3D::None:
    return "None";
  case GameplayHlodWorldTerminalReason3D::FirstFramePending:
    return "FirstFramePending";
  case GameplayHlodWorldTerminalReason3D::RetryLimitExceeded:
    return "RetryLimitExceeded";
  case GameplayHlodWorldTerminalReason3D::RuntimeUnavailable:
    return "RuntimeUnavailable";
  case GameplayHlodWorldTerminalReason3D::FrameInvariantViolation:
    return "FrameInvariantViolation";
  case GameplayHlodWorldTerminalReason3D::IdentityInvariantViolation:
    return "IdentityInvariantViolation";
  case GameplayHlodWorldTerminalReason3D::DataInvariantViolation:
    return "DataInvariantViolation";
  }
  return "RuntimeUnavailable";
}

struct GameplayHlodWorldContractInput3D {
  std::uint64_t frameId{}, submittedFrame{};
  bool identityInvariantValid{true}, dataInvariantValid{true};
};

struct GameplayHlodWorldContractDecision3D {
  GameplayHlodWorldReadiness3D readiness{
      GameplayHlodWorldReadiness3D::Fatal};
  GameplayHlodWorldTerminalReason3D reason{
      GameplayHlodWorldTerminalReason3D::RuntimeUnavailable};
};

[[nodiscard]] constexpr GameplayHlodWorldContractDecision3D
EvaluateGameplayHlodWorldContract3D(
    GameplayHlodWorldContractInput3D input) noexcept {
  if (!input.frameId ||
      (input.submittedFrame && input.frameId <= input.submittedFrame))
    return {GameplayHlodWorldReadiness3D::Fatal,
            GameplayHlodWorldTerminalReason3D::FrameInvariantViolation};
  if (!input.identityInvariantValid)
    return {GameplayHlodWorldReadiness3D::Fatal,
            GameplayHlodWorldTerminalReason3D::IdentityInvariantViolation};
  if (!input.dataInvariantValid)
    return {GameplayHlodWorldReadiness3D::Fatal,
            GameplayHlodWorldTerminalReason3D::DataInvariantViolation};
  return {GameplayHlodWorldReadiness3D::Ready,
          GameplayHlodWorldTerminalReason3D::None};
}

[[nodiscard]] constexpr GameplayHlodWorldContractDecision3D
EvaluateGameplayHlodWorldRetry3D(std::uint32_t retryCount,
                                std::uint32_t retryLimit) noexcept {
  if (!retryLimit || retryCount >= retryLimit)
    return {GameplayHlodWorldReadiness3D::Fatal,
            GameplayHlodWorldTerminalReason3D::RetryLimitExceeded};
  return {GameplayHlodWorldReadiness3D::RetryableNotReady,
          GameplayHlodWorldTerminalReason3D::FirstFramePending};
}

struct GameplayHlodSceneEcsOwnershipReceipt3D {
  static constexpr std::uint32_t SchemaVersion = 1;
  std::uint32_t schemaVersion{SchemaVersion};
  GameplaySceneEcsSubmissionOwner3D owner{
      GameplaySceneEcsSubmissionOwner3D::None};
  std::uint64_t frameRequests{}, acceptedSubmissions{},
      rejectedDuplicateFrames{}, submittedFrame{};
  bool independentRegistry{};

  [[nodiscard]] constexpr bool Complete() const noexcept {
    return schemaVersion == SchemaVersion && independentRegistry &&
        owner == GameplaySceneEcsSubmissionOwner3D::HlodWorldRuntime &&
        acceptedSubmissions && submittedFrame;
  }
};

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
  static constexpr std::uint32_t FirstFrameRetryLimit = 8;
  ShowcaseProductionHlodBuildReceipt3D build{};
  Aevrynd::WorldPartitionStats3D partition{};
  Aevrynd::HlodIntegrationStats3D integration{};
  Aevrynd::HlodRuntimeFrameStats3D frame{};
  Aevrynd::SceneECSCollectionStats3D collection{};
  GameplayHlodSceneEcsOwnershipReceipt3D ownership{};
  std::uint64_t frozenFrame{};
  std::uint64_t partitionRequests{}, partitionDispatches{},
      partitionCompletions{}, attemptFrame{}, successfulFrames{},
      stagingCopyNanoseconds{}, stagingCopyBytesEstimate{};
  std::uint32_t firstFrameRetryCount{},
      firstFrameRetryLimit{FirstFrameRetryLimit}, sourceWaitIdleCalls{};
  GameplayHlodWorldReadiness3D readiness{
      GameplayHlodWorldReadiness3D::RetryableNotReady};
  GameplayHlodWorldTerminalReason3D terminalReason{
      GameplayHlodWorldTerminalReason3D::FirstFramePending};
  GameplayHlodWorldTerminalReason3D lastRetryReason{
      GameplayHlodWorldTerminalReason3D::None};
  bool worldPartitionResident{}, selected{}, frozen{}, collected{};
  bool partitionPending{}, firstFrameRetryObserved{}, attemptRetryCounted{},
      frameInvariantPassed{}, identityInvariantPassed{}, dataInvariantPassed{},
      lifecycleInvariantPassed{}, transactionalStagingWip{};
  // 第一階段刻意記錄尚未達成的範圍，避免把代表性低階模型誤報成正式聚合 proxy。
  bool gameplayLabOnRenderConnected{}, aggregateProxyBuilt{},
      allDescriptorKindsCovered{};

  [[nodiscard]] constexpr bool CoreComplete() const noexcept {
    return build.Complete() && worldPartitionResident && selected && frozen &&
        collected && frozenFrame == frame.frameId && frame.frozen &&
        ownership.Complete() && ownership.submittedFrame == frozenFrame &&
        integration.attachedClusters == build.clusters &&
        collection.submittedRenderables != 0;
  }

  [[nodiscard]] constexpr bool LifecycleConsistent() const noexcept {
    return partition.totalQueued >= partition.totalStarted &&
        partition.totalStarted >= partition.totalCompleted &&
        !partition.totalFailed && !partition.sourceFailures &&
        !partition.loadFailures && !partition.cpuBudgetFailures &&
        !partition.preparationBudgetFailures &&
        !partition.activationBudgetFailures && !partition.gpuBudgetFailures &&
        !partition.activationFailures && !partition.deltaLoadFailures &&
        !partition.deltaSaveFailures && !partition.staleDeltaCompletions &&
        !partition.ownerThreadViolations &&
        partition.lastFailure == Aevrynd::WorldPartitionFailureReason3D::None;
  }

  [[nodiscard]] constexpr bool ReadyLifecycleComplete() const noexcept {
    return LifecycleConsistent() && !partitionPending &&
        worldPartitionResident && partition.resident == 1;
  }

  [[nodiscard]] constexpr bool SeamComplete() const noexcept {
    return CoreComplete() && readiness == GameplayHlodWorldReadiness3D::Ready &&
        terminalReason == GameplayHlodWorldTerminalReason3D::None &&
        frameInvariantPassed && identityInvariantPassed &&
        dataInvariantPassed && lifecycleInvariantPassed &&
        ReadyLifecycleComplete();
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
                  ShowcaseRenderAssetBindings3D &assets,
                  std::span<const ShowcaseSceneDescriptor3D> descriptors,
                  Aevrynd::StableObjectId3D worldId,
                  GameplayHlodWorldRuntimeOptions3D options = {});
  GameplayHlodWorldReadiness3D
  CollectFrame(std::uint64_t frameId, const Aevrynd::Camera3D &camera,
               Aevrynd::Vec2 viewport, Aevrynd::Scene3D &scene,
               Aevrynd::DrawList &draw);
  void Shutdown();
  [[nodiscard]] bool Initialized() const noexcept;
  [[nodiscard]] GameplayHlodWorldRuntimeReceipt3D Receipt() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
  // 尚未初始化或初始化失敗也必須留下真實終態，不能回傳預設的等待中收據。
  GameplayHlodWorldRuntimeReceipt3D unavailableReceipt_{};
};

} // GameplayLab 命名空間
