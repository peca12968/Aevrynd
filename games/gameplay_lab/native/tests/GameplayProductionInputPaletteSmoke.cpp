#include "GameplayInputReplay3D.h"
#include "ObjectPaletteAdapter3D.h"
#include <Aevrynd/CharacterController3D.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
using namespace GameplayLab;
using namespace Aevrynd;
namespace {
bool NearlyEqual(float left, float right) {
  return std::abs(left - right) <= 1e-6f;
}

bool ValidateCommittedRotation(float updateDelta, float expectedRotation) {
  Rules3D rules;
  CollisionWorld3D collision;
  PhysicsWorld3D physics;
  SceneECSRegistry3D scene;
  collision.CreateBox({{{0, -.5f, 0}, {20, .5f, 20}}});
  ObjectPaletteAdapter3D palette(rules, collision, physics, scene);
  const Ray3D ray{{2, 10, 2}, {0, -1, 0}};
  if (!palette.Begin(ray, PaletteCategory::Building) ||
      !palette.Update(ray, updateDelta) ||
      !NearlyEqual(palette.Preview().rotation, expectedRotation))
    return false;

  const auto previewBeforeHostile = palette.Preview();
  for (const float hostile : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity()}) {
    if (palette.Update(ray, hostile) ||
        !NearlyEqual(palette.Preview().rotation,
                     previewBeforeHostile.rotation) ||
        Length(palette.Preview().position - previewBeforeHostile.position) >
            1e-6f)
      return false;
  }

  if (!palette.Commit() || rules.Entities().size() != 1)
    return false;
  const auto id = rules.Entities().front().id;
  const auto bodyRotation = [&]() {
    const auto *body = physics.GetBody(palette.Body(id));
    return body ? body->settings.transform.rotation.y
                : std::numeric_limits<float>::quiet_NaN();
  };
  const auto sceneRotation = [&]() {
    const auto *transform = scene.Transform(palette.SceneEntity(id));
    return transform ? transform->value.rotation.y
                     : std::numeric_limits<float>::quiet_NaN();
  };
  if (!NearlyEqual(rules.Entities().front().rotation, expectedRotation) ||
      !NearlyEqual(bodyRotation(), expectedRotation) ||
      !NearlyEqual(sceneRotation(), expectedRotation))
    return false;

  if (!palette.Undo() || !rules.Entities().empty() || palette.Body(id) ||
      !palette.Redo() || rules.Entities().size() != 1 ||
      !NearlyEqual(rules.Entities().front().rotation, expectedRotation) ||
      !NearlyEqual(bodyRotation(), expectedRotation) ||
      !NearlyEqual(sceneRotation(), expectedRotation))
    return false;

  if (!palette.Delete(id) || !palette.Undo() ||
      rules.Entities().size() != 1 ||
      !NearlyEqual(rules.Entities().front().rotation, expectedRotation) ||
      !NearlyEqual(bodyRotation(), expectedRotation) ||
      !NearlyEqual(sceneRotation(), expectedRotation))
    return false;
  return true;
}
} // namespace
int main() {
  if (!ValidateCommittedRotation(1.f, .2617994f) ||
      !ValidateCommittedRotation(6.f, 1.5707964f))
    return 42;
  {
    Rules3D hostileRules;
    hostileRules.SetPaletteCategory(PaletteCategory::Building);
    const auto rejectedBefore = hostileRules.GetStats().placementRejected;
    for (const float hostile : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity()})
      if (hostileRules.Place({0, .5f, 0}, false, hostile))
        return 43;
    if (!hostileRules.Entities().empty() ||
        hostileRules.GetStats().placementRejected != rejectedBefore + 2)
      return 44;
  }
  Input input;
  input.SetKey('1', true);
  input.SetKey('9', true);
  input.SetKey('W', true);
  input.SetKey(VK_SHIFT, true);
  input.SetKey(VK_SPACE, true);
  input.SetKey('C', true);
  input.SetKey(VK_TAB, true);
  input.SetMouse({640, 360});
  input.SetMouse({650, 355});
  input.AddMouseWheel(1);
  input.SetMouseButton(true);
  auto m = ReadProductionInput3D(input, 0);
  if (m.camera != CameraMode::FirstPerson ||
      m.category != PaletteCategory::Lights || m.tick.movement.z != 1 ||
      !m.tick.run || !m.tick.jump || !m.tick.crouch || !m.tick.togglePalette ||
      !m.tick.attack || !input.MouseDown() || m.mouseDelta.x != 10 ||
      m.wheel != 1)
    return 1;
  {
    Input postureInput;
    postureInput.SetKey('Z', true);
    if (!ReadProductionInput3D(postureInput, 0).tick.prone)
      return 45;
    postureInput.SetKey(VK_CONTROL, true);
    if (ReadProductionInput3D(postureInput, 0).tick.prone ||
        !ReadProductionInput3D(postureInput, 0).tick.undo)
      return 46;
  }
  input.ConsumePresses();
  input.SetMouse({700, 390});
  if (ReadProductionInput3D(input, 0).tick.attack || !input.MouseDown())
    return 16;
  input.SetMouseButton(false);
  if (input.MouseDown())
    return 17;
  input.SetRightMouseButton(true);
  if (!ReadProductionInput3D(input, 0).tick.cancel)
    return 2;
  Rules3D rules;
  CollisionWorld3D collision;
  PhysicsWorld3D physics;
  SceneECSRegistry3D scene;
  collision.CreateBox({{{0, -.5f, 0}, {20, .5f, 20}}});
  ObjectPaletteAdapter3D palette(rules, collision, physics, scene);
  Ray3D ray{{2, 10, 2}, {0, -1, 0}};
  if (!palette.Begin(ray, PaletteCategory::Physics) ||
      !palette.Preview().valid || !palette.Update(ray, 1) ||
      !palette.Commit() || rules.Entities().size() != 1 ||
      physics.Stats().dynamicBodies != 1)
    return 3;
  const auto id = rules.Entities().front().id;
  if (!palette.Body(id) || !palette.Collider(id) || !palette.SceneEntity(id) ||
      !scene.Alive(palette.SceneEntity(id)))
    return 4;
  if (!palette.Begin(ray, PaletteCategory::Physics) || palette.Preview().valid)
    return 5;
  palette.Cancel();
  const auto colliderHandle = palette.Collider(id);
  Transform3D moved{};
  moved.position = {6, .5f, 3};
  if (!physics.SetTransform(palette.Body(id), moved) ||
      !palette.SyncDynamicTransforms())
    return 14;
  const auto *sceneTransform = scene.Transform(palette.SceneEntity(id));
  const auto *colliderState = collision.Get(colliderHandle);
  const auto movedHits = collision.QueryBox(moved.position, {.49f, .49f, .49f});
  const auto oldHits = collision.QueryBox({2, .5f, 2}, {.49f, .49f, .49f});
  if (!sceneTransform || !colliderState ||
      Length(sceneTransform->value.position - moved.position) > 1e-6f ||
      Length(colliderState->bounds.center - moved.position) > 1e-6f ||
      colliderState->userData != id ||
      std::ranges::find(movedHits, colliderHandle) == movedHits.end() ||
      std::ranges::find(oldHits, colliderHandle) != oldHits.end())
    return 15;
  const Ray3D invalidPick{{-6, .5f, -4}, {0, 0, 1}},
      validPick{{6, .5f, -4}, {0, 0, 1}};
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const auto rejectedBefore = palette.Stats().pickRejected;
  const std::array hostileRays{
      Ray3D{{nan, .5f, -4}, {0, 0, 1}},
      Ray3D{{6, .5f, -4}, {nan, 0, 1}},
      Ray3D{{6, .5f, -4}, {0, 0, 0}},
      Ray3D{{6, .5f, -4}, {std::numeric_limits<float>::max(), 0,
                           std::numeric_limits<float>::max()}}};
  for (const auto &hostile : hostileRays)
    if (palette.BeginGrab(hostile))
      return 28;
  if (palette.BeginGrab(validPick, nan) || palette.BeginGrab(validPick, 0.f) ||
      palette.BeginGrab(validPick, Rules3D::MaximumGrabDistance + 1.f) ||
      palette.Grabbed() || rules.Grabbed() ||
      palette.Stats().pickRejected != rejectedBefore + hostileRays.size() + 3)
    return 29;
  physics.Step(0.f);
  const auto controlBefore = physics.Stats();
  physics.Step(0.f);
  const auto controlAfter = physics.Stats();
  const auto probeBefore = controlAfter;
  if (palette.BeginGrab(invalidPick) || !palette.BeginGrab(validPick) ||
      palette.Grabbed() != id)
    return 18;
  for (int probe = 0; probe < 128; ++probe)
    if (!std::isfinite(palette.GrabError()))
      return 39;
  if (!palette.SyncDynamicTransforms())
    return 40;
  physics.Step(0.f);
  const auto probeAfter = physics.Stats();
  const auto controlBuildDelta =
      controlAfter.broadPhaseBuilds - controlBefore.broadPhaseBuilds;
  const auto controlReuseDelta =
      controlAfter.broadPhaseReuses - controlBefore.broadPhaseReuses;
  const auto probeBuildDelta =
      probeAfter.broadPhaseBuilds - probeBefore.broadPhaseBuilds;
  const auto probeReuseDelta =
      probeAfter.broadPhaseReuses - probeBefore.broadPhaseReuses;
  if (controlBuildDelta != 0 || controlReuseDelta != 1 ||
      probeBuildDelta != 0 || probeReuseDelta != 1)
    return 41;
  const auto *beforeHostileUpdate = physics.GetBody(palette.Body(id));
  if (!beforeHostileUpdate)
    return 30;
  const Vec3 targetBeforeHostile = palette.GrabTarget();
  const Vec3 forceBeforeHostile = beforeHostileUpdate->accumulatedForce;
  const auto dragStepsBeforeHostile = palette.Stats().dragSteps;
  const std::array hostileUpdates{
      std::pair{Ray3D{{nan, .5f, -4}, {0, 0, 1}}, 1.f / 60.f},
      std::pair{Ray3D{{6, .5f, -4}, {nan, 0, 1}}, 1.f / 60.f},
      std::pair{Ray3D{{6, .5f, -4}, {0, 0, 0}}, 1.f / 60.f},
      std::pair{validPick, nan}, std::pair{validPick, 0.f},
      std::pair{validPick, -1.f}, std::pair{validPick, .1001f}};
  for (const auto &[hostileRay, hostileDt] : hostileUpdates)
    if (palette.UpdateGrab(hostileRay, hostileDt))
      return 31;
  const auto *afterHostileUpdate = physics.GetBody(palette.Body(id));
  if (!afterHostileUpdate || !palette.Grabbed() || !rules.Grabbed() ||
      Length(palette.GrabTarget() - targetBeforeHostile) > 1e-6f ||
      Length(afterHostileUpdate->accumulatedForce - forceBeforeHostile) >
          1e-6f ||
      palette.Stats().dragSteps != dragStepsBeforeHostile)
    return 32;
  physics.Step(1.f / 60.f, 2);
  if (!palette.SyncDynamicTransforms())
    return 19;
  const auto retainedBeforeDrag = physics.MemoryStats().retainedCapacityBytes;
  for (int frame = 0; frame < 90; ++frame) {
    const Ray3D drag{{9, .5f, -4}, {0, 0, 1}};
    if (!palette.UpdateGrab(drag, 1.f / 60.f)) {
      return 20;
    }
    physics.Step(1.f / 60.f, 2);
    if (!palette.SyncDynamicTransforms())
      return 21;
  }
  const auto *dragged = physics.GetBody(palette.Body(id));
  const auto *draggedCollider = collision.Get(colliderHandle);
  const auto *draggedTransform = scene.Transform(palette.SceneEntity(id));
  if (!dragged || !draggedCollider || !draggedTransform)
    return 22;
  if (dragged->settings.transform.position.x < 7.f)
    return 23;
  if (dragged->settings.transform.position.y < .49f)
    return 24;
  if (Length(draggedCollider->bounds.center -
             dragged->settings.transform.position) > 1e-6f ||
      Length(draggedTransform->value.position -
             dragged->settings.transform.position) > 1e-6f)
    return 25;
  if (physics.MemoryStats().retainedCapacityBytes != retainedBeforeDrag)
    return 26;
  palette.EndGrab();
  if (palette.Grabbed() || rules.Grabbed() ||
      palette.Stats().pickAttempts < 2 || palette.Stats().pickRejected < 1 ||
      palette.Stats().dragSteps != 90 || palette.Stats().drops != 1)
    return 27;

  const Ray3D massPick{{6, 2, -4}, {0, 0, 1}};
  const auto resetMassBody = [&](float mass) {
    Transform3D transform{};
    transform.position = {6, 2, 3};
    if (!physics.SetTransform(palette.Body(id), transform) ||
        !palette.SyncDynamicTransforms())
      return false;
    auto *state = physics.GetBody(palette.Body(id));
    if (!state)
      return false;
    state->settings.mass = mass;
    state->velocity = {};
    state->accumulatedForce = {};
    return true;
  };
  if (!resetMassBody(0.f) || !palette.BeginGrab(massPick))
    return 33;
  const auto zeroTarget = palette.GrabTarget();
  if (palette.UpdateGrab(massPick, 1.f / 60.f) || !palette.Grabbed() ||
      Length(palette.GrabTarget() - zeroTarget) > 1e-6f)
    return 34;
  palette.EndGrab();

  const auto holdMass = [&](float mass) {
    if (!resetMassBody(mass) || !palette.BeginGrab(massPick))
      return false;
    for (int frame = 0; frame < 120; ++frame) {
      if (!palette.UpdateGrab(massPick, 1.f / 60.f))
        return false;
      const auto *state = physics.GetBody(palette.Body(id));
      if (!state || !std::isfinite(state->accumulatedForce.x) ||
          !std::isfinite(state->accumulatedForce.y) ||
          !std::isfinite(state->accumulatedForce.z))
        return false;
      physics.Step(1.f / 60.f, 2);
      if (!palette.SyncDynamicTransforms())
        return false;
    }
    const auto *state = physics.GetBody(palette.Body(id));
    return state && std::isfinite(state->settings.transform.position.y) &&
           state->settings.transform.position.y >= 1.95f;
  };
  if (!holdMass(.001f))
    return 35;
  palette.EndGrab();
  if (!holdMass(100.f))
    return 36;

  const auto dropsBeforeDelete = palette.Stats().drops;
  if (!palette.Delete(id) || palette.Grabbed() || rules.Grabbed() ||
      palette.Stats().drops != dropsBeforeDelete + 1 ||
      !rules.Entities().empty() ||
      physics.Stats().dynamicBodies || palette.Body(id) ||
      palette.Collider(id) || palette.SceneEntity(id))
    return 6;
  if (!palette.Undo())
    return 7;
  const auto undoHits = collision.QueryBox({2, .5f, 2}, {.5f, .5f, .5f});
  if (rules.Entities().size() != 1 || rules.Entities().front().id != id ||
      physics.Stats().dynamicBodies != 1 || collision.Stats().colliders != 2 ||
      undoHits.empty() || !scene.Alive(palette.SceneEntity(id)))
    return 7;
  if (!palette.PlaceAt({10, .5f, 2}, PaletteCategory::Physics))
    return 37;
  if (rules.Entities().empty() ||
      rules.Entities().back().category != PaletteCategory::Physics)
    return 39;
  const Ray3D restoredPick{{2, .5f, -4}, {0, 0, 1}};
  if (!palette.BeginGrab(restoredPick) || !palette.Undo() ||
      palette.Grabbed() || rules.Grabbed() || rules.Entities().size() != 1 ||
      rules.Entities().front().id != id || !palette.Body(id))
    return 38;
  if (!palette.Delete(id) || !rules.Entities().empty() ||
      physics.Stats().dynamicBodies || collision.Stats().colliders != 1)
    return 8;
  palette.Begin({{4, 10, 4}, {0, -1, 0}}, PaletteCategory::Props);
  palette.Cancel();
  if (palette.Stats().cancels < 1)
    return 9;
  palette.Clear();
  if (physics.Stats().dynamicBodies)
    return 10;
  CharacterController3D controller;
  controller.Reset({0, 0, 0});
  const Vec3 feet = controller.Position();
  if (!controller.SetHeight(1.15f) ||
      Length(controller.Position() - feet) > 1e-6f ||
      std::abs(controller.Extents().y - .575f) > 1e-5f)
    return 11;
  CollisionWorld3D headroom;
  headroom.CreateBox({{{0, 1.45f, 0}, {1, .15f, 1}}});
  if (!headroom.OverlapsBox(feet + Vec3{0, .9f, 0}, {.44f, .89f, .44f}) ||
      Length(controller.Position() - feet) > 1e-6f)
    return 12;
  headroom.Clear();
  if (headroom.OverlapsBox(feet + Vec3{0, .9f, 0}, {.44f, .89f, .44f}) ||
      !controller.SetHeight(1.8f) ||
      Length(controller.Position() - feet) > 1e-6f)
    return 13;
  std::cout << "input=production atomicPalette=1 crouchCapsule=1 commits="
            << palette.Stats().commits << " deletes=" << palette.Stats().deletes
            << " buildDelta=" << probeBuildDelta
            << " reuseDelta=" << probeReuseDelta << "\n";
  return 0;
}
