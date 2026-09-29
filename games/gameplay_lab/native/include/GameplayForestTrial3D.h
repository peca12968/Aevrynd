#pragma once

#include <Aevrynd/Math.h>

#include <cmath>
#include <array>
#include <cstdint>
#include <string_view>

namespace GameplayLab {

enum class ForestTrialPhase3D : std::uint8_t {
  ReachBridgeApproach,
  CrossBridge,
  DefeatGuardian,
  ReturnAcrossBridge,
  ReturnToCottage,
  Complete
};

struct ForestTrialRoute3D {
  Aevrynd::Vec3 bridgeApproach{};
  Aevrynd::Vec3 bridgeExit{};
  Aevrynd::Vec3 guardianArena{};
  Aevrynd::Vec3 cottageEntry{};
  float checkpointRadius{2.25f};
  float completionRadius{2.25f};
};

struct ForestTrialArenaStone3D {
  Aevrynd::Vec3 offset{};
  Aevrynd::Vec3 scale{1.f, 1.f, 1.f};
  float yaw{};
};

inline constexpr std::uint64_t GameplayForestTrialArenaStoneIdentityHigh3D =
    0xaec04152454e4101ull;
inline constexpr std::uint64_t GameplayForestTrialObjectiveIdentityHigh3D =
    0xaec04f424a454354ull;

// A broken stone ring gives the east-bank guardian encounter a readable place
// in the world without fencing the player in.  The gaps face the bridge and
// forest exit, every stone remains outside the melee envelope, and the props
// are render-only so the authoritative controller/collision route is unchanged.
inline constexpr std::array<ForestTrialArenaStone3D, 8>
    GameplayForestTrialArenaStones3D{{
        {{-3.65f, 0.f, -.85f}, {.78f, .62f, 1.05f}, .28f},
        {{-2.55f, 0.f, -2.85f}, {.92f, .70f, .76f}, 1.04f},
        {{-.55f, 0.f, -3.75f}, {.72f, .58f, 1.10f}, 1.82f},
        {{2.05f, 0.f, -3.10f}, {1.08f, .76f, .82f}, 2.48f},
        {{3.70f, 0.f, -.60f}, {.84f, .66f, 1.02f}, -.36f},
        {{3.15f, 0.f, 2.20f}, {1.02f, .72f, .78f}, .72f},
        {{.75f, 0.f, 3.75f}, {.76f, .60f, 1.14f}, 1.44f},
        {{-2.05f, 0.f, 3.05f}, {1.10f, .78f, .80f}, 2.12f},
    }};

class GameplayForestTrial3D {
public:
  [[nodiscard]] bool Reset(const ForestTrialRoute3D &route) noexcept {
    if (!ValidRoute(route))
      return false;
    route_ = route;
    phase_ = ForestTrialPhase3D::ReachBridgeApproach;
    transitions_ = 0;
    initialized_ = true;
    return true;
  }

  [[nodiscard]] bool Tick(bool guardianAlive,
                          Aevrynd::Vec3 playerFeet) noexcept {
    if (!initialized_ || !Finite(playerFeet))
      return false;
    // Exactly one phase may advance per authoritative simulation tick.  This
    // prevents a teleported/stale position from satisfying several route
    // checkpoints at once and turning the trial into a HUD-only claim.
    switch (phase_) {
    case ForestTrialPhase3D::ReachBridgeApproach:
      if (Within(playerFeet, route_.bridgeApproach, route_.checkpointRadius)) {
        phase_ = ForestTrialPhase3D::CrossBridge;
        ++transitions_;
      }
      break;
    case ForestTrialPhase3D::CrossBridge:
      if (Within(playerFeet, route_.bridgeExit, route_.checkpointRadius)) {
        phase_ = ForestTrialPhase3D::DefeatGuardian;
        ++transitions_;
      }
      break;
    case ForestTrialPhase3D::DefeatGuardian:
      if (!guardianAlive) {
        phase_ = ForestTrialPhase3D::ReturnAcrossBridge;
        ++transitions_;
      }
      break;
    case ForestTrialPhase3D::ReturnAcrossBridge:
      if (Within(playerFeet, route_.bridgeApproach,
                 route_.checkpointRadius)) {
        phase_ = ForestTrialPhase3D::ReturnToCottage;
        ++transitions_;
      }
      break;
    case ForestTrialPhase3D::ReturnToCottage:
      if (Within(playerFeet, route_.cottageEntry, route_.completionRadius)) {
        phase_ = ForestTrialPhase3D::Complete;
        ++transitions_;
      }
      break;
    case ForestTrialPhase3D::Complete:
      break;
    }
    return true;
  }

  [[nodiscard]] ForestTrialPhase3D Phase() const noexcept { return phase_; }
  [[nodiscard]] bool Initialized() const noexcept { return initialized_; }
  [[nodiscard]] bool Complete() const noexcept {
    return initialized_ && phase_ == ForestTrialPhase3D::Complete;
  }
  [[nodiscard]] std::uint64_t Transitions() const noexcept {
    return transitions_;
  }
  [[nodiscard]] constexpr std::uint32_t ObjectiveStepCount() const noexcept {
    return 5;
  }
  [[nodiscard]] std::uint32_t ObjectiveStep() const noexcept {
    if (!initialized_)
      return 0;
    switch (phase_) {
    case ForestTrialPhase3D::ReachBridgeApproach:
      return 1;
    case ForestTrialPhase3D::CrossBridge:
      return 2;
    case ForestTrialPhase3D::DefeatGuardian:
      return 3;
    case ForestTrialPhase3D::ReturnAcrossBridge:
      return 4;
    case ForestTrialPhase3D::ReturnToCottage:
    case ForestTrialPhase3D::Complete:
      return 5;
    }
    return 0;
  }
  [[nodiscard]] const ForestTrialRoute3D &Route() const noexcept {
    return route_;
  }
  [[nodiscard]] float
  DistanceToObjective(Aevrynd::Vec3 playerFeet) const noexcept {
    if (!initialized_ || !Finite(playerFeet))
      return 0.f;
    switch (phase_) {
    case ForestTrialPhase3D::ReachBridgeApproach:
      return DistanceXZ(playerFeet, route_.bridgeApproach);
    case ForestTrialPhase3D::CrossBridge:
      return DistanceXZ(playerFeet, route_.bridgeExit);
    case ForestTrialPhase3D::DefeatGuardian:
      return DistanceXZ(playerFeet, route_.guardianArena);
    case ForestTrialPhase3D::ReturnAcrossBridge:
      return DistanceXZ(playerFeet, route_.bridgeApproach);
    case ForestTrialPhase3D::ReturnToCottage:
      return DistanceXZ(playerFeet, route_.cottageEntry);
    case ForestTrialPhase3D::Complete:
      return 0.f;
    }
    return 0.f;
  }
  [[nodiscard]] Aevrynd::Vec3 ObjectivePosition() const noexcept {
    if (!initialized_)
      return {};
    switch (phase_) {
    case ForestTrialPhase3D::ReachBridgeApproach:
    case ForestTrialPhase3D::ReturnAcrossBridge:
      return route_.bridgeApproach;
    case ForestTrialPhase3D::CrossBridge:
      return route_.bridgeExit;
    case ForestTrialPhase3D::DefeatGuardian:
      return route_.guardianArena;
    case ForestTrialPhase3D::ReturnToCottage:
    case ForestTrialPhase3D::Complete:
      return route_.cottageEntry;
    }
    return {};
  }
  [[nodiscard]] std::string_view
  DirectionLabel(Aevrynd::Vec3 playerFeet, float playerYaw) const noexcept {
    if (!initialized_ || !Finite(playerFeet) || !std::isfinite(playerYaw) ||
        Complete())
      return "ARRIVED";
    const auto target = ObjectivePosition();
    const float x = target.x - playerFeet.x;
    const float z = target.z - playerFeet.z;
    const float distance = std::hypot(x, z);
    if (!std::isfinite(distance) || distance <= .05f)
      return "ARRIVED";
    const float directionX = x / distance;
    const float directionZ = z / distance;
    const float forwardX = std::sin(playerYaw);
    const float forwardZ = std::cos(playerYaw);
    const float dot = forwardX * directionX + forwardZ * directionZ;
    constexpr float thirtyDegreeCosine = .8660254f;
    if (dot >= thirtyDegreeCosine)
      return "AHEAD";
    if (dot <= -thirtyDegreeCosine)
      return "BEHIND";
    const float right = forwardZ * directionX - forwardX * directionZ;
    return right >= 0.f ? "RIGHT" : "LEFT";
  }
  [[nodiscard]] std::string_view ObjectiveLabel() const noexcept {
    switch (phase_) {
    case ForestTrialPhase3D::ReachBridgeApproach:
      return "FOLLOW THE TRAIL TO THE BRIDGE";
    case ForestTrialPhase3D::CrossBridge:
      return "CROSS THE RIVER BRIDGE";
    case ForestTrialPhase3D::DefeatGuardian:
      return "DEFEAT THE FOREST GUARDIAN";
    case ForestTrialPhase3D::ReturnAcrossBridge:
      return "CROSS THE BRIDGE BACK TO THE COTTAGE";
    case ForestTrialPhase3D::ReturnToCottage:
      return "ENTER THE COTTAGE";
    case ForestTrialPhase3D::Complete:
      return "FOREST TRIAL COMPLETE";
    }
    return "FOREST TRIAL INVALID";
  }

private:
  [[nodiscard]] static bool Finite(Aevrynd::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
  }
  [[nodiscard]] static float DistanceXZ(Aevrynd::Vec3 left,
                                        Aevrynd::Vec3 right) noexcept {
    return std::hypot(left.x - right.x, left.z - right.z);
  }

  [[nodiscard]] static bool Within(Aevrynd::Vec3 left, Aevrynd::Vec3 right,
                                   float radius) noexcept {
    return DistanceXZ(left, right) <= radius;
  }
  [[nodiscard]] static bool ValidRadius(float value) noexcept {
    return std::isfinite(value) && value >= .5f && value <= 6.f;
  }
  [[nodiscard]] static bool
  ValidRoute(const ForestTrialRoute3D &route) noexcept {
    if (!Finite(route.bridgeApproach) || !Finite(route.bridgeExit) ||
        !Finite(route.guardianArena) || !Finite(route.cottageEntry) ||
        !ValidRadius(route.checkpointRadius) ||
        !ValidRadius(route.completionRadius))
      return false;
    // A real bridge traversal must span more than two checkpoint diameters.
    // The guardian remains inside the production AI's wake-up range after the
    // player reaches the far bank.
    return DistanceXZ(route.bridgeApproach, route.bridgeExit) >=
               route.checkpointRadius * 2.f &&
           DistanceXZ(route.bridgeExit, route.guardianArena) >= .5f &&
           DistanceXZ(route.bridgeExit, route.guardianArena) <= 10.f &&
           DistanceXZ(route.cottageEntry, route.bridgeApproach) >=
               route.checkpointRadius;
  }

  ForestTrialRoute3D route_{};
  ForestTrialPhase3D phase_{ForestTrialPhase3D::ReachBridgeApproach};
  std::uint64_t transitions_{};
  bool initialized_{};
};

} // namespace GameplayLab
