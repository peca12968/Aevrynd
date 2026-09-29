#include "GameplayForestTrial3D.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <set>

using namespace GameplayLab;

int main() {
  GameplayForestTrial3D trial;
  if (trial.Tick(true, {}) || trial.Initialized()) return 1;
  const ForestTrialRoute3D route{{-9.85f, 0, 14},
                                 {7.15f, 0, 14},
                                 {9.2f, 0, 14},
                                 {-12, 0, 21.6f},
                                 2.25f,
                                 .75f};
  auto invalid = route;
  invalid.bridgeApproach.x = std::numeric_limits<float>::quiet_NaN();
  if (trial.Reset(invalid))
    return 2;
  invalid = route;
  invalid.checkpointRadius = .49f;
  if (trial.Reset(invalid)) return 2;
  invalid = route;
  invalid.completionRadius = 6.01f;
  if (trial.Reset(invalid)) return 2;
  invalid = route;
  invalid.bridgeExit = {-8.f, 0, 14};
  if (trial.Reset(invalid)) return 2;

  if (!trial.Reset(route) || trial.Complete() ||
      trial.Phase() != ForestTrialPhase3D::ReachBridgeApproach ||
      trial.ObjectiveLabel() != "FOLLOW THE TRAIL TO THE BRIDGE" ||
      trial.ObjectiveStep() != 1 || trial.ObjectiveStepCount() != 5 ||
      trial.DirectionLabel({-16.f, 0.f, 8.f}, 0.f) != "RIGHT")
    return 3;

  // Cottage, exit and dead guardian cannot skip the ordered traversal.
  if (!trial.Tick(true, {-12, 100, 19}) || trial.Transitions() != 0 ||
      trial.Phase() != ForestTrialPhase3D::ReachBridgeApproach ||
      !trial.Tick(false, route.bridgeExit) || trial.Transitions() != 0)
    return 4;

  if (!trial.Tick(true, route.bridgeApproach) || trial.Transitions() != 1 ||
      trial.Phase() != ForestTrialPhase3D::CrossBridge || trial.Complete() ||
      trial.ObjectiveStep() != 2 ||
      trial.DirectionLabel(route.bridgeApproach, 0.f) != "RIGHT")
    return 5;
  if (!trial.Tick(false, route.bridgeApproach) || trial.Transitions() != 1 ||
      trial.Phase() != ForestTrialPhase3D::CrossBridge)
    return 6;
  if (!trial.Tick(false, route.bridgeExit) || trial.Transitions() != 2 ||
      trial.Phase() != ForestTrialPhase3D::DefeatGuardian)
    return 7;

  // Entering the guardian arena and killing it are separate authoritative
  // events; returning to the cottage requires another bridge crossing.
  if (!trial.Tick(true, route.guardianArena) || trial.Transitions() != 2 ||
      trial.ObjectiveLabel() != "DEFEAT THE FOREST GUARDIAN")
    return 8;
  if (!trial.Tick(false, route.guardianArena) || trial.Transitions() != 3 ||
      trial.Phase() != ForestTrialPhase3D::ReturnAcrossBridge ||
      trial.Complete())
    return 9;
  // The cottage cannot be completed from the east bank: returning across the
  // physical bridge is an explicit ordered checkpoint.
  if (!trial.Tick(false, route.cottageEntry) || trial.Complete() ||
      trial.Transitions() != 3)
    return 10;
  if (!trial.Tick(false, route.bridgeApproach) || trial.Complete() ||
      trial.Transitions() != 4 ||
      trial.Phase() != ForestTrialPhase3D::ReturnToCottage)
    return 11;
  // The exterior porch and threshold are not completion points. The final
  // checkpoint is beyond the front wall, inside the cottage footprint.
  if (!trial.Tick(false, {-12.f, 0.f, 20.4f}) || trial.Complete() ||
      trial.Transitions() != 4)
    return 12;
  if (!trial.Tick(false, route.cottageEntry) || !trial.Complete() ||
      trial.Transitions() != 5 ||
      trial.ObjectiveLabel() != "FOREST TRIAL COMPLETE" ||
      trial.ObjectiveStep() != trial.ObjectiveStepCount() ||
      trial.DirectionLabel(route.cottageEntry, 0.f) != "ARRIVED")
    return 13;
  if (!trial.Tick(true, {50, 0, 50}) || !trial.Complete() ||
      trial.Transitions() != 5)
    return 14;
  const auto completedTransitions = trial.Transitions();
  const float routeDistance =
      std::hypot(route.bridgeApproach.x + 16.f,
                 route.bridgeApproach.z - 8.f) +
      std::hypot(route.bridgeExit.x - route.bridgeApproach.x,
                 route.bridgeExit.z - route.bridgeApproach.z) +
      std::hypot(route.guardianArena.x - route.bridgeExit.x,
                 route.guardianArena.z - route.bridgeExit.z) +
      std::hypot(route.bridgeApproach.x - route.guardianArena.x,
                 route.bridgeApproach.z - route.guardianArena.z) +
      std::hypot(route.cottageEntry.x - route.bridgeApproach.x,
                 route.cottageEntry.z - route.bridgeApproach.z);
  if (!trial.Reset(route) || trial.Complete() || trial.Transitions() != 0 ||
      trial.Phase() != ForestTrialPhase3D::ReachBridgeApproach)
    return 15;

  // Direction cues are authoritative XZ bearings relative to the same yaw
  // convention used by production movement: yaw zero faces +Z.
  ForestTrialRoute3D compassRoute = route;
  compassRoute.bridgeApproach = {0.f, 0.f, 10.f};
  compassRoute.bridgeExit = {0.f, 0.f, 20.f};
  compassRoute.guardianArena = {0.f, 0.f, 21.f};
  compassRoute.cottageEntry = {-10.f, 0.f, 0.f};
  if (!trial.Reset(compassRoute) ||
      trial.DirectionLabel({0.f, 0.f, 0.f}, 0.f) != "AHEAD" ||
      trial.DirectionLabel({0.f, 0.f, 0.f}, 3.14159265f) != "BEHIND" ||
      trial.DirectionLabel({10.f, 0.f, 10.f}, 0.f) != "LEFT" ||
      trial.DirectionLabel({-10.f, 0.f, 10.f}, 0.f) != "RIGHT")
    return 16;

  // The guardian arena is physical world composition, not a HUD-only target.
  // Keep a clear melee centre and two useful approach gaps while proving all
  // render identities and authored transforms are deterministic and finite.
  std::set<std::uint64_t> arenaIdentities;
  float minimumArenaRadius = std::numeric_limits<float>::max();
  float maximumArenaRadius = 0.f;
  for (std::size_t index = 0;
       index < GameplayForestTrialArenaStones3D.size(); ++index) {
    const auto &stone = GameplayForestTrialArenaStones3D[index];
    const float radius = std::hypot(stone.offset.x, stone.offset.z);
    minimumArenaRadius = (std::min)(minimumArenaRadius, radius);
    maximumArenaRadius = (std::max)(maximumArenaRadius, radius);
    if (!std::isfinite(stone.offset.x) || !std::isfinite(stone.offset.z) ||
        !std::isfinite(stone.yaw) || stone.scale.x <= 0.f ||
        stone.scale.y <= 0.f || stone.scale.z <= 0.f ||
        !arenaIdentities.insert(
             GameplayForestTrialArenaStoneIdentityHigh3D ^ (index + 1u))
             .second)
      return 17;
  }
  if (GameplayForestTrialArenaStones3D.size() != 8u ||
      GameplayForestTrialObjectiveIdentityHigh3D == 0u ||
      GameplayForestTrialObjectiveIdentityHigh3D ==
          GameplayForestTrialArenaStoneIdentityHigh3D ||
      minimumArenaRadius < 3.45f || maximumArenaRadius > 3.90f)
    return 18;

  std::cout << "GAMEPLAY_FOREST_TRIAL=PASS transitions="
            << completedTransitions << " routeDistance=" << routeDistance
            << " arenaStones=" << GameplayForestTrialArenaStones3D.size()
            << " arenaRadius=" << minimumArenaRadius << ".."
            << maximumArenaRadius << '\n';
  return 0;
}
