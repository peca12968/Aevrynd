#include "ProductionInputReplayHarness3D.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/HeightField3D.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace Aevrynd;
using namespace GameplayLab;
using namespace GameplayLab::Tests;

namespace {
constexpr float kTolerance = 1e-5f;

bool Near(float left, float right, float tolerance = kTolerance) {
  return std::abs(left - right) <= tolerance;
}

bool Near(Vec3 left, Vec3 right, float tolerance = kTolerance) {
  return Length(left - right) <= tolerance;
}

ProductionReplaySnapshot3D Hold(ProductionInputReplayHarness3D &replay,
                                unsigned firstKey, unsigned secondKey = 0,
                                int ticks = 120) {
  replay.SetKey(firstKey, true);
  if (secondKey)
    replay.SetKey(secondKey, true);
  ProductionReplaySnapshot3D result{};
  for (int tick = 0; tick < ticks; ++tick)
    result = replay.Tick();
  replay.SetKey(firstKey, false);
  if (secondKey)
    replay.SetKey(secondKey, false);
  return result;
}

struct MovementResult {
  Vec3 position{};
  Vec3 releasedPosition{};
  std::uint64_t traceHash{};
};

MovementResult ReplayMovement(unsigned firstKey, unsigned secondKey = 0) {
  ProductionInputReplayHarness3D replay(.37f);
  const auto moved = Hold(replay, firstKey, secondKey);
  const auto released = replay.Tick();
  return {moved.position, released.position, replay.TraceHash()};
}

std::uint64_t ReplayCompleteScenario() {
  ProductionInputReplayHarness3D replay(.37f);
  for (const unsigned key : {'1', '2', '3'}) {
    replay.SetKey(key, true);
    replay.Tick();
    replay.SetKey(key, false);
    replay.Tick();
  }
  Hold(replay, 'W', 'D', 90);
  replay.Tick();
  replay.SetKey('C', true);
  replay.Tick();
  for (int held = 0; held < 8; ++held)
    replay.Tick();
  replay.SetKey('C', false);
  replay.Tick();
  replay.SetHeadroom(false);
  replay.SetKey('C', true);
  replay.Tick();
  replay.SetKey('C', false);
  replay.Tick();
  return replay.TraceHash();
}
} // namespace

int main() {
  // 正式 runtime 曾只在 Rules3D 拒絕蹲跳，卻仍把原始 Space 傳給
  // CharacterController3D。這裡走相同控制器路徑，避免規則統計看似正確、
  // 實際角色卻離地的分層分歧。
  {
    Rules3D postureRules;
    CharacterController3D controller;
    CollisionWorld3D collisions;
    HeightField3D terrain;
    controller.Reset({0, 0, 0});
    terrain.Configure({{-32, 0, -32}, 17, 17, 4});
    controller.Step({}, collisions, terrain,
                    ProductionInputReplayHarness3D::FixedDeltaTime);
    TickInput crouchInput{};
    crouchInput.crouch = true;
    postureRules.Tick(crouchInput);
    controller.SetHeight(postureRules.CharacterHeight());
    TickInput crouchedJump{};
    crouchedJump.jump = true;
    postureRules.Tick(crouchedJump);
    const CharacterControl3D control =
        BuildProductionCharacterControl3D(crouchedJump, postureRules);
    controller.Step(control, collisions, terrain,
                    ProductionInputReplayHarness3D::FixedDeltaTime);
    if (controller.Position().y > 1e-5f ||
        postureRules.GetStats().jumps != 0)
      return 10;
  }

  // 視角必須由正式按鍵邊緣依序切換，且每種投影都產生獨立雜湊。
  ProductionInputReplayHarness3D cameras(.37f);
  std::array<std::uint64_t, 3> cameraHashes{};
  const std::array expectedModes{CameraMode::FirstPerson,
                                 CameraMode::ThirdPerson,
                                 CameraMode::GodView};
  for (std::size_t index = 0; index < expectedModes.size(); ++index) {
    const unsigned key = static_cast<unsigned>('1' + index);
    cameras.SetKey(key, true);
    const auto snapshot = cameras.Tick();
    cameras.SetKey(key, false);
    cameras.Tick();
    if (snapshot.camera != expectedModes[index] || !IsFinite(snapshot))
      return 1;
    cameraHashes[index] = snapshot.cameraHash;
  }
  if (cameraHashes[0] == cameraHashes[1] ||
      cameraHashes[0] == cameraHashes[2] ||
      cameraHashes[1] == cameraHashes[2] ||
      cameras.Rules().GetStats().cameraSwitches != 3)
    return 2;

  const auto forward = ReplayMovement('W');
  const auto backward = ReplayMovement('S');
  const auto left = ReplayMovement('A');
  const auto right = ReplayMovement('D');
  const auto diagonal = ReplayMovement('W', 'D');
  const std::array movements{forward, backward, left, right, diagonal};
  for (const auto &movement : movements) {
    if (!std::isfinite(movement.position.x) ||
        !std::isfinite(movement.position.y) ||
        !std::isfinite(movement.position.z) ||
        !Near(movement.position, movement.releasedPosition))
      return 3;
  }
  // 相反方向需對稱，對角線輸入不可比單軸移動更快。
  if (!Near(forward.position, backward.position * -1.f) ||
      !Near(left.position, right.position * -1.f) ||
      !Near(Length(forward.position), 8.f) ||
      !Near(Length(right.position), 8.f) ||
      !Near(Length(diagonal.position), 8.f) ||
      Dot(forward.position, Vec3{std::sin(.37f), 0.f, std::cos(.37f)}) <
          7.999f ||
      Dot(right.position, Vec3{std::cos(.37f), 0.f, -std::sin(.37f)}) <
          7.999f)
    return 4;
  if (ReplayMovement('W').traceHash != forward.traceHash ||
      ReplayMovement('S').traceHash != backward.traceHash ||
      ReplayMovement('A').traceHash != left.traceHash ||
      ReplayMovement('D').traceHash != right.traceHash ||
      ReplayMovement('W', 'D').traceHash != diagonal.traceHash)
    return 5;

  ProductionInputReplayHarness3D crouch;
  const Vec3 standingFeet = crouch.Snapshot().position;
  crouch.SetKey('C', true);
  const auto crouched = crouch.Tick();
  for (int held = 0; held < 20; ++held)
    crouch.Tick();
  if (!crouched.crouched || !Near(crouched.characterHeight, 1.15f) ||
      !Near(crouched.position, standingFeet) ||
      crouch.Rules().GetStats().crouches != 1)
    return 6;
  crouch.SetKey('C', false);
  crouch.Tick();
  crouch.SetHeadroom(false);
  const auto beforeRejected = crouch.Snapshot();
  crouch.SetKey('C', true);
  const auto rejected = crouch.Tick();
  if (!rejected.crouched || !Near(rejected.characterHeight, 1.15f) ||
      !Near(rejected.position, beforeRejected.position) ||
      crouch.Rules().GetStats().headroomRejected != 1)
    return 7;
  crouch.SetKey('C', false);
  crouch.Tick();
  crouch.SetHeadroom(true);
  crouch.SetKey('C', true);
  const auto standing = crouch.Tick();
  if (standing.crouched || !Near(standing.characterHeight, 1.8f) ||
      !Near(standing.position, standingFeet) || !IsFinite(standing))
    return 8;

  ProductionInputReplayHarness3D prone;
  const Vec3 proneFeet = prone.Snapshot().position;
  prone.SetKey('Z', true);
  const auto proneEntered = prone.Tick();
  prone.SetKey('Z', false);
  prone.Tick();
  if (proneEntered.posture != PostureState::Prone || !proneEntered.prone ||
      proneEntered.crouched || !Near(proneEntered.characterHeight, .6f) ||
      !Near(proneEntered.position, proneFeet) ||
      prone.Rules().GetStats().prones != 1)
    return 11;
  const auto crawlStart = prone.Snapshot().position;
  const auto crawlEnd = Hold(prone, 'W', VK_SHIFT);
  if (!Near(Length(crawlEnd.position - crawlStart), 3.f, .001f))
    return 12;
  const auto jumpsBefore = prone.Rules().GetStats().jumps;
  prone.SetKey(VK_SPACE, true);
  const auto proneJump = prone.Tick();
  prone.SetKey(VK_SPACE, false);
  prone.Tick();
  if (!Near(proneJump.position.y, proneFeet.y) ||
      prone.Rules().GetStats().jumps != jumpsBefore)
    return 13;
  prone.SetHeadroom(false);
  prone.SetKey('Z', true);
  const auto proneBlocked = prone.Tick();
  prone.SetKey('Z', false);
  prone.Tick();
  if (!proneBlocked.prone ||
      prone.Rules().GetStats().headroomRejected != 1)
    return 14;
  prone.SetHeadroom(true);
  prone.SetKey('Z', true);
  const auto proneStanding = prone.Tick();
  prone.SetKey('Z', false);
  prone.Tick();
  if (proneStanding.posture != PostureState::Standing ||
      proneStanding.prone || !Near(proneStanding.characterHeight, 1.8f))
    return 15;

  const auto firstScenario = ReplayCompleteScenario();
  const auto secondScenario = ReplayCompleteScenario();
  if (!firstScenario || firstScenario != secondScenario)
    return 9;

  std::cout << "Gameplay production input replay passed: state="
            << firstScenario << " cameras=" << cameraHashes[0] << ','
            << cameraHashes[1] << ',' << cameraHashes[2] << '\n';
  return 0;
}
