#include "GameplayForestTrial3D.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace GameplayLab;

int main() {
  GameplayForestTrial3D trial;
  if (trial.Tick(true, {}) || trial.Initialized()) return 1;
  const ForestTrialRoute3D route{{-9.85f, 0, 14},
                                 {7.15f, 0, 14},
                                 {9.2f, 0, 14},
                                 {-12, 0, 19},
                                 2.25f,
                                 2.25f};
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
      trial.ObjectiveLabel() != "FOLLOW THE TRAIL TO THE BRIDGE")
    return 3;

  // Cottage, exit and dead guardian cannot skip the ordered traversal.
  if (!trial.Tick(true, {-12, 100, 19}) || trial.Transitions() != 0 ||
      trial.Phase() != ForestTrialPhase3D::ReachBridgeApproach ||
      !trial.Tick(false, route.bridgeExit) || trial.Transitions() != 0)
    return 4;

  if (!trial.Tick(true, route.bridgeApproach) || trial.Transitions() != 1 ||
      trial.Phase() != ForestTrialPhase3D::CrossBridge || trial.Complete())
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
  if (!trial.Tick(false, route.cottageEntry) || !trial.Complete() ||
      trial.Transitions() != 5 ||
      trial.ObjectiveLabel() != "FOREST TRIAL COMPLETE")
    return 12;
  if (!trial.Tick(true, {50, 0, 50}) || !trial.Complete() ||
      trial.Transitions() != 5)
    return 13;
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
    return 14;

  std::cout << "GAMEPLAY_FOREST_TRIAL=PASS transitions="
            << completedTransitions << " routeDistance=" << routeDistance
            << '\n';
  return 0;
}
