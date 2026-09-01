#include "GameplayUxReplayWin32.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
using namespace GameplayLab;
using namespace GameplayLab::UxReplay;

namespace {
class ControlledGameplayAdapter3D final : public IGameplayUxReplayAdapter3D {
public:
  void Apply(const ProductionInput3D &input, const ReplayEdges3D &edges,
             std::span<const PointerEvent3D> pointerEvents,
             float fixedDeltaTime) override {
    if (edges.focusLost)
      state_.cursorCaptured = false;
    if (edges.closeRequested)
      state_.closed = true;
    if (!state_.focused)
      return;
    if (input.camera)
      SetCamera(*input.camera);
    if (input.cycleCamera)
      SetCamera(CameraMode((unsigned(state_.camera) + 1) % 3));
    if (input.tick.togglePalette) {
      state_.paletteOpen = !state_.paletteOpen;
      ++state_.paletteToggles;
      if (!state_.paletteOpen)
        state_.dragging = false;
    }
    if (input.tick.crouch) {
      if (!state_.crouched || headroomClear_) {
        state_.crouched = !state_.crouched;
        state_.characterHeight = state_.crouched ? 1.15f : 1.8f;
        ++state_.crouchToggles;
      }
    }
    if (input.tick.jump)
      ++state_.jumps;
    if (edges.escapePressed) {
      if (state_.paletteOpen || state_.dragging) {
        state_.paletteOpen = state_.dragging = false;
        ++state_.cancels;
      } else
        state_.closed = true;
    }
    if ((edges.pointerCancelled || input.tick.cancel) &&
        (state_.paletteOpen || state_.dragging)) {
      state_.dragging = false;
      state_.cursorCaptured = false;
      ++state_.cancels;
    }
    if (edges.grabPressed && !state_.paletteOpen) {
      state_.grabbed = !state_.grabbed;
      ++state_.grabs;
    }
    if ((input.tick.attack || edges.meleePressed) && !state_.paletteOpen &&
        !state_.dragging)
      ++state_.melee;
    for (const auto &pointer : pointerEvents) {
      if (pointer.phase == PointerPhase3D::RawDelta &&
          !state_.paletteOpen) {
        state_.yaw -= pointer.delta.x * .0025f;
        state_.pitch = std::clamp(state_.pitch - pointer.delta.y * .0025f,
                                  -.75f, .9f);
      }
      if (pointer.phase == PointerPhase3D::Down && state_.paletteOpen &&
          pointer.position.x < 320.f) {
        state_.dragging = true;
        state_.cursorCaptured = true;
      }
      if (pointer.phase == PointerPhase3D::Up && state_.dragging) {
        state_.dragging = false;
        state_.cursorCaptured = false;
        ++state_.commits;
      }
    }
    Vec3 movement = input.tick.movement;
    const float length = Length(movement);
    if (length > 1.f)
      movement = movement * (1.f / length);
    const float speed = input.tick.run ? 8.f : 4.f;
    state_.playerPosition =
        state_.playerPosition + movement * (speed * fixedDeltaTime);
    // 中文註解：受控障礙位於 z=2，第三人稱相機距離不可穿越障礙。
    state_.cameraDistance = std::clamp(state_.cameraDistance, .25f, 3.4f);
  }

  ReplayState3D Snapshot() const override { return state_; }
  void SetFocus(bool focused) override {
    state_.focused = focused;
    state_.cursorVisible = !focused || state_.paletteOpen;
    if (!focused)
      state_.cursorCaptured = false;
  }
  void SetHeadroom(bool clear) { headroomClear_ = clear; }

private:
  void SetCamera(CameraMode mode) {
    if (state_.camera != mode) {
      state_.camera = mode;
      ++state_.cameraToggles;
    }
  }
  ReplayState3D state_{};
  bool headroomClear_{true};
};

bool Edge(GameplayUxReplayWin32Harness3D &harness, unsigned key,
          bool system = false) {
  return harness.Key(key, true, system) && harness.Tick() &&
         harness.Key(key, false, system) && harness.Tick();
}

std::uint64_t RunRate(unsigned rate) {
  ControlledGameplayAdapter3D adapter;
  GameplayUxReplayWin32Harness3D harness(adapter);
  if (!harness.Initialize() || !harness.Key('W', true))
    return 0;
  constexpr unsigned FixedRate = 120;
  for (unsigned frame = 0; frame < rate; ++frame)
    for (unsigned step = frame * FixedRate / rate;
         step < (frame + 1) * FixedRate / rate; ++step)
      if (!harness.Tick())
        return 0;
  if (!harness.Key('W', false) || !harness.Tick())
    return 0;
  return HashReplayState3D(adapter.Snapshot());
}
} // namespace

int main() {
  ControlledGameplayAdapter3D adapter;
  GameplayUxReplayWin32Harness3D harness(adapter);
  if (!harness.Initialize())
    return 1;

  if (!harness.Focus(false) || !harness.Key('W', true) || !harness.Tick() ||
      Length(adapter.Snapshot().playerPosition) != 0.f ||
      !harness.Focus(true) || !harness.Tick() ||
      Length(adapter.Snapshot().playerPosition) != 0.f)
    return 2;
  if (!Edge(harness, '1') || !Edge(harness, '2') || !Edge(harness, '3') ||
      !Edge(harness, 'V') || adapter.Snapshot().cameraToggles != 4)
    return 3;
  const auto cameraTogglesBeforeRepeat = adapter.Snapshot().cameraToggles;
  if (!harness.Key('V', true) || !harness.Tick() ||
      !harness.Key('V', true) || !harness.Tick() ||
      !harness.Key('V', false) || !harness.Tick() ||
      adapter.Snapshot().cameraToggles != cameraTogglesBeforeRepeat + 1)
    return 3;

  if (!harness.Key('W', true) || !harness.Tick() ||
      !harness.Key('D', true) || !harness.Tick() ||
      !harness.Key('W', false) || !harness.Key('D', false) ||
      !harness.Tick())
    return 4;
  const Vec3 released = adapter.Snapshot().playerPosition;
  if (!harness.Tick() || Length(adapter.Snapshot().playerPosition - released) >
                             1e-6f)
    return 5;

  if (!harness.RawMouse(32, -16) || !harness.Tick() ||
      !std::isfinite(adapter.Snapshot().yaw) ||
      !std::isfinite(adapter.Snapshot().pitch))
    return 6;
  if (!harness.Key('C', true) || !harness.Tick() ||
      !harness.Key('C', true) || !harness.Tick() ||
      !harness.Key('C', false) || !harness.Tick() ||
      !adapter.Snapshot().crouched || adapter.Snapshot().crouchToggles != 1)
    return 7;
  adapter.SetHeadroom(false);
  if (!Edge(harness, 'C') || !adapter.Snapshot().crouched ||
      adapter.Snapshot().crouchToggles != 1)
    return 8;
  if (!harness.Key(VK_SHIFT, true) || !harness.Key('W', true) ||
      !harness.Tick() || !harness.Key('W', false) ||
      !harness.Key(VK_SHIFT, false) || !Edge(harness, VK_SPACE))
    return 9;

  if (!harness.Key(VK_TAB, true) || !harness.Tick() ||
      !harness.Key(VK_TAB, true) || !harness.Tick() ||
      !harness.Key(VK_TAB, false) || !harness.Tick() ||
      !adapter.Snapshot().paletteOpen || adapter.Snapshot().paletteToggles != 1 ||
      !harness.MouseMove(100, 100) || !harness.MouseButton(true) ||
      GetCapture() != harness.Window() ||
      !harness.Tick() || !adapter.Snapshot().dragging ||
      !adapter.Snapshot().cursorCaptured || !harness.MouseMove(600, 300) ||
      !harness.MouseButton(false) || !harness.Tick() ||
      GetCapture() == harness.Window() ||
      adapter.Snapshot().commits != 1 || adapter.Snapshot().dragging)
    return 10;
  const auto meleeBeforeUi = adapter.Snapshot().melee;
  if (!harness.MouseButton(true) || !harness.MouseButton(false) ||
      !harness.Tick() || !Edge(harness, 'F') ||
      adapter.Snapshot().melee != meleeBeforeUi)
    return 11;
  if (!harness.MouseMove(100, 100) || !harness.MouseButton(true) ||
      !harness.Tick() || !adapter.Snapshot().dragging ||
      !harness.MouseButton(true, true) || !harness.MouseButton(false, true) ||
      !harness.Tick() || adapter.Snapshot().dragging ||
      adapter.Snapshot().cancels != 1 || adapter.Snapshot().commits != 1 ||
      !harness.MouseButton(false))
    return 11;
  if (!Edge(harness, 'B') || adapter.Snapshot().paletteOpen ||
      !Edge(harness, 'G') || !adapter.Snapshot().grabbed ||
      !Edge(harness, 'G') || adapter.Snapshot().grabbed ||
      !harness.MouseButton(true) || !harness.MouseButton(false) ||
      !harness.Tick() || !Edge(harness, 'F') ||
      adapter.Snapshot().melee != meleeBeforeUi + 2)
    return 12;
  if (!harness.Wheel(WHEEL_DELTA) || !harness.Tick())
    return 12;

  if (!Edge(harness, VK_ESCAPE) || !adapter.Snapshot().closed ||
      !harness.Close() || harness.LastAcknowledgedSequence() == 0)
    return 13;
  bool sawWheel{};
  for (const auto &receipt : harness.Receipts()) {
    if (receipt.eventSequence != receipt.acknowledgedSequence ||
        !receipt.stateHash)
      return 14;
    sawWheel = sawWheel || std::abs(receipt.input.wheel - 1.f) < 1e-6f;
  }
  if (!sawWheel || adapter.Snapshot().cameraDistance < .25f ||
      adapter.Snapshot().cameraDistance > 3.4f)
    return 14;

  const std::array hashes{RunRate(30), RunRate(60), RunRate(144)};
  if (!hashes[0] || hashes[0] != hashes[1] || hashes[0] != hashes[2])
    return 15;

  std::cout << "uxReplay=pass receipts=" << harness.Receipts().size()
            << " fixedTick=" << harness.Receipts().back().fixedTick
            << " rates=30,60,144 hash=" << hashes[0]
            << " uiPriority=pass collision=pass\n";
  return 0;
}
