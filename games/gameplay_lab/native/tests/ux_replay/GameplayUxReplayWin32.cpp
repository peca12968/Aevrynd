#include "GameplayUxReplayWin32.h"

#include <Windowsx.h>

#include <algorithm>
#include <bit>
#include <cmath>

namespace GameplayLab::UxReplay {
namespace {
constexpr wchar_t WindowClassName[] = L"AevryndGameplayUxReplayWindow";

std::uint64_t Mix(std::uint64_t hash, std::uint64_t value) {
  constexpr std::uint64_t Prime = 1099511628211ull;
  for (unsigned index = 0; index < 8; ++index) {
    hash ^= (value >> (index * 8)) & 0xffu;
    hash *= Prime;
  }
  return hash;
}
} // namespace

std::uint64_t HashReplayState3D(const ReplayState3D &state) {
  std::uint64_t hash = 14695981039346656037ull;
  const auto mixFloat = [&](float value) {
    hash = Mix(hash, std::bit_cast<std::uint32_t>(value));
  };
  mixFloat(state.playerPosition.x);
  mixFloat(state.playerPosition.y);
  mixFloat(state.playerPosition.z);
  mixFloat(state.yaw);
  mixFloat(state.pitch);
  mixFloat(state.cameraDistance);
  mixFloat(state.characterHeight);
  hash = Mix(hash, static_cast<std::uint64_t>(state.camera));
  hash = Mix(hash, static_cast<std::uint64_t>(state.posture));
  const bool flags[] = {state.focused, state.cursorCaptured, state.cursorVisible,
                        state.paletteOpen, state.dragging, state.grabbed,
                        state.crouched, state.prone, state.closed};
  for (const bool flag : flags)
    hash = Mix(hash, flag ? 1u : 0u);
  const std::uint64_t counters[] = {
      state.cameraToggles, state.paletteToggles, state.crouchToggles,
      state.proneToggles,
      state.jumps,        state.grabs,          state.melee,
      state.commits,      state.cancels};
  for (const auto counter : counters)
    hash = Mix(hash, counter);
  return hash;
}

GameplayUxReplayWin32Harness3D::GameplayUxReplayWin32Harness3D(
    IGameplayUxReplayAdapter3D &adapter)
    : adapter_(adapter) {}

GameplayUxReplayWin32Harness3D::~GameplayUxReplayWin32Harness3D() {
  ReleaseInputAndCursor();
  if (window_)
    DestroyWindow(window_);
}

bool GameplayUxReplayWin32Harness3D::Initialize() {
  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.lpszClassName = WindowClassName;
  if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return false;
  window_ = CreateWindowExW(0, WindowClassName, L"Aevrynd UX Replay",
                            WS_OVERLAPPEDWINDOW, 0, 0, 1280, 720, nullptr,
                            nullptr, windowClass.hInstance, this);
  return window_ != nullptr;
}

LRESULT CALLBACK GameplayUxReplayWin32Harness3D::WindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto *self = reinterpret_cast<GameplayUxReplayWin32Harness3D *>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto *creation = reinterpret_cast<const CREATESTRUCTW *>(lParam);
    self = static_cast<GameplayUxReplayWin32Harness3D *>(creation->lpCreateParams);
    self->window_ = window;
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->HandleMessage(message, wParam, lParam)
              : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT GameplayUxReplayWin32Harness3D::HandleMessage(
    UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (!focused_)
      return 0;
    input_.SetKey(static_cast<unsigned>(wParam), true);
    if (wParam == 'G' && input_.Pressed('G'))
      edges_.grabPressed = true;
    if (wParam == 'F' && input_.Pressed('F'))
      edges_.meleePressed = true;
    if (wParam == VK_ESCAPE && input_.Pressed(VK_ESCAPE))
      edges_.escapePressed = true;
    return 0;
  case WM_KEYUP:
  case WM_SYSKEYUP:
    if (!focused_)
      return 0;
    input_.SetKey(static_cast<unsigned>(wParam), false);
    return 0;
  case WM_MOUSEMOVE: {
    if (!focused_)
      return 0;
    const Aevrynd::Vec2 point{float(GET_X_LPARAM(lParam)),
                              float(GET_Y_LPARAM(lParam))};
    input_.SetMouse(point, true);
    pointerEvents_.push_back(
        {PointerPhase3D::Move, point, {}, 0.f, primaryDown_, secondaryDown_, true});
    return 0;
  }
  case WM_INPUT: {
    if (!focused_)
      return 0;
    const auto *packet =
        reinterpret_cast<const ControlledRawMousePacket3D *>(lParam);
    if (!packet || packet->magic != ControlledRawMousePacket3D::Magic)
      return DefWindowProcW(window_, message, wParam, lParam);
    pointerEvents_.push_back(
        {PointerPhase3D::RawDelta, {},
         {float(packet->deltaX), float(packet->deltaY)}, 0.f,
         primaryDown_, secondaryDown_, true});
    return 0;
  }
  case WM_MOUSEWHEEL: {
    if (!focused_)
      return 0;
    const float delta = float(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
    input_.AddMouseWheel(delta);
    pointerEvents_.push_back(
        {PointerPhase3D::Wheel, input_.Mouse(), {}, delta, primaryDown_,
         secondaryDown_, true});
    return 0;
  }
  case WM_LBUTTONDOWN:
    if (!focused_)
      return 0;
    if (primaryDown_)
      return 0;
    primaryDown_ = true;
    input_.SetMouseButton(true);
    SetCapture(window_);
    edges_.pointerPressed = true;
    pointerEvents_.push_back({PointerPhase3D::Down, input_.Mouse(), {}, 0.f,
                              true, secondaryDown_, true});
    return 0;
  case WM_LBUTTONUP:
    if (!focused_)
      return 0;
    primaryDown_ = false;
    input_.SetMouseButton(false);
    if (GetCapture() == window_)
      ReleaseCapture();
    edges_.pointerReleased = true;
    pointerEvents_.push_back({PointerPhase3D::Up, input_.Mouse(), {}, 0.f,
                              false, secondaryDown_, true});
    return 0;
  case WM_RBUTTONDOWN:
    if (!focused_)
      return 0;
    if (secondaryDown_)
      return 0;
    secondaryDown_ = true;
    input_.SetRightMouseButton(true);
    edges_.pointerCancelled = true;
    return 0;
  case WM_RBUTTONUP:
    if (!focused_)
      return 0;
    secondaryDown_ = false;
    input_.SetRightMouseButton(false);
    return 0;
  case WM_ACTIVATEAPP:
    focused_ = wParam != FALSE;
    adapter_.SetFocus(focused_);
    if (focused_)
      edges_.focusRestored = true;
    else {
      edges_.focusLost = true;
      ReleaseInputAndCursor();
    }
    return 0;
  case WM_CLOSE:
    edges_.closeRequested = true;
    return 0;
  case FixedTickMessage: {
    const ProductionInput3D mapped = ReadProductionInput3D(input_, 0.f);
    adapter_.Apply(mapped, edges_, pointerEvents_, FixedDeltaTime);
    ++fixedTick_;
    Acknowledge(message);
    input_.ConsumePresses();
    edges_ = {};
    pointerEvents_.clear();
    return static_cast<LRESULT>(acknowledgedSequence_);
  }
  case BarrierMessage:
    Acknowledge(message);
    return static_cast<LRESULT>(acknowledgedSequence_);
  }
  return DefWindowProcW(window_, message, wParam, lParam);
}

void GameplayUxReplayWin32Harness3D::Acknowledge(UINT message) {
  acknowledgedSequence_ = eventSequence_;
  const auto input = ReadProductionInput3D(input_, 0.f);
  const auto state = adapter_.Snapshot();
  receipts_.push_back({eventSequence_, acknowledgedSequence_, fixedTick_,
                       message, input, edges_, state,
                       HashReplayState3D(state)});
}

void GameplayUxReplayWin32Harness3D::ReleaseInputAndCursor() {
  for (unsigned key = 0; key < 256; ++key)
    input_.SetKey(key, false);
  input_.SetMouseButton(false);
  input_.SetRightMouseButton(false);
  primaryDown_ = secondaryDown_ = false;
  if (window_ && GetCapture() == window_)
    ReleaseCapture();
}

bool GameplayUxReplayWin32Harness3D::Dispatch(UINT message, WPARAM wParam,
                                              LPARAM lParam) {
  if (!window_)
    return false;
  ++eventSequence_;
  SendMessageW(window_, message, wParam, lParam);
  SendMessageW(window_, BarrierMessage, 0, 0);
  return acknowledgedSequence_ == eventSequence_;
}

bool GameplayUxReplayWin32Harness3D::Key(unsigned key, bool down,
                                         bool system) {
  return Dispatch(system ? (down ? WM_SYSKEYDOWN : WM_SYSKEYUP)
                         : (down ? WM_KEYDOWN : WM_KEYUP),
                  key, down ? 1 : 1u << 31);
}
bool GameplayUxReplayWin32Harness3D::MouseMove(int x, int y) {
  return Dispatch(WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
}
bool GameplayUxReplayWin32Harness3D::RawMouse(int deltaX, int deltaY) {
  ControlledRawMousePacket3D packet{};
  packet.deltaX = deltaX;
  packet.deltaY = deltaY;
  return Dispatch(WM_INPUT, RIM_INPUT,
                  reinterpret_cast<LPARAM>(&packet));
}
bool GameplayUxReplayWin32Harness3D::MouseButton(bool down, bool secondary) {
  return Dispatch(secondary ? (down ? WM_RBUTTONDOWN : WM_RBUTTONUP)
                            : (down ? WM_LBUTTONDOWN : WM_LBUTTONUP));
}
bool GameplayUxReplayWin32Harness3D::Wheel(short delta) {
  return Dispatch(WM_MOUSEWHEEL, MAKEWPARAM(0, delta));
}
bool GameplayUxReplayWin32Harness3D::Focus(bool active) {
  return Dispatch(WM_ACTIVATEAPP, active ? TRUE : FALSE);
}
bool GameplayUxReplayWin32Harness3D::Tick() {
  return Dispatch(FixedTickMessage);
}
bool GameplayUxReplayWin32Harness3D::Close() { return Dispatch(WM_CLOSE); }

} // namespace GameplayLab::UxReplay
