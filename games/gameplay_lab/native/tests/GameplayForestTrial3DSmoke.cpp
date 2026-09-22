#include "GameplayForestTrial3D.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace GameplayLab;

int main() {
  GameplayForestTrial3D trial;
  if (trial.Tick(true, {}) || trial.Initialized()) return 1;
  if (trial.Reset({std::numeric_limits<float>::quiet_NaN(), 0, 0}) ||
      trial.Reset({-12, 0, 19}, .49f) || trial.Reset({-12, 0, 19}, 6.01f))
    return 2;
  if (!trial.Reset({-12, 1.25f, 19}) || trial.Complete() ||
      trial.Phase() != ForestTrialPhase3D::DefeatGuardian ||
      trial.ObjectiveLabel() != "DEFEAT THE FOREST GUARDIAN")
    return 3;

  // Reaching the cottage cannot skip the combat objective.
  if (!trial.Tick(true, {-12, 100, 19}) || trial.Transitions() != 0 ||
      trial.Phase() != ForestTrialPhase3D::DefeatGuardian)
    return 4;
  if (!trial.Tick(false, {-12, 100, 19}) || trial.Transitions() != 1 ||
      trial.Phase() != ForestTrialPhase3D::ReturnToCottage ||
      trial.Complete())
    return 5;
  // The kill transition and arrival transition require distinct simulation
  // ticks so one stale position sample cannot complete the complete loop.
  if (!trial.Tick(false, {-9.70f, -50, 19}) || trial.Complete()) return 6;
  if (!trial.Tick(false, {-9.76f, 80, 19}) || !trial.Complete() ||
      trial.Transitions() != 2 ||
      trial.ObjectiveLabel() != "FOREST TRIAL COMPLETE")
    return 7;
  if (!trial.Tick(true, {50, 0, 50}) || !trial.Complete() ||
      trial.Transitions() != 2)
    return 8;
  if (!trial.Reset({-12, 0, 19}) || trial.Complete() ||
      trial.Transitions() != 0 ||
      trial.Phase() != ForestTrialPhase3D::DefeatGuardian)
    return 9;

  std::cout << "GAMEPLAY_FOREST_TRIAL=PASS transitions="
            << trial.Transitions() << " distance="
            << trial.DistanceToCottage({-12, 0, 19}) << '\n';
  return 0;
}
