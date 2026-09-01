#pragma once

#include "GameplayInputReplay3D.h"

#include <Windows.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace GameplayLab::UxReplay {

enum class PointerPhase3D : std::uint8_t { Move, Down, Up, Wheel, RawDelta };

struct PointerEvent3D {
  PointerPhase3D phase{PointerPhase3D::Move};
  Aevrynd::Vec2 position{}, delta{};
  float wheel{};
  bool primary{}, secondary{}, insideCanvas{};
};

struct ReplayEdges3D {
  bool focusLost{}, focusRestored{}, closeRequested{};
  bool grabPressed{}, meleePressed{}, escapePressed{};
  bool pointerPressed{}, pointerReleased{}, pointerCancelled{};
};

struct ReplayState3D {
  Aevrynd::Vec3 playerPosition{};
  CameraMode camera{CameraMode::ThirdPerson};
  float yaw{}, pitch{}, cameraDistance{3.4f}, characterHeight{1.8f};
  bool focused{true}, cursorCaptured{}, cursorVisible{true};
  bool paletteOpen{}, dragging{}, grabbed{}, crouched{}, closed{};
  std::uint64_t cameraToggles{}, paletteToggles{}, crouchToggles{};
  std::uint64_t jumps{}, grabs{}, melee{}, commits{}, cancels{};
};

struct ReplayReceipt3D {
  std::uint64_t eventSequence{}, acknowledgedSequence{}, fixedTick{};
  std::uint32_t message{};
  ProductionInput3D input{};
  ReplayEdges3D edges{};
  ReplayState3D state{};
  std::uint64_t stateHash{};
};

// GameplayLab 的 --ux-replay 模式只需實作此介面；harness 不直接修改遊戲狀態。
class IGameplayUxReplayAdapter3D {
public:
  virtual ~IGameplayUxReplayAdapter3D() = default;
  virtual void Apply(const ProductionInput3D &input,
                     const ReplayEdges3D &edges,
                     std::span<const PointerEvent3D> pointerEvents,
                     float fixedDeltaTime) = 0;
  [[nodiscard]] virtual ReplayState3D Snapshot() const = 0;
  virtual void SetFocus(bool focused) = 0;
};

struct ControlledRawMousePacket3D {
  static constexpr std::uint64_t Magic = 0x58525559414c5045ull;
  std::uint64_t magic{Magic};
  LONG deltaX{}, deltaY{};
};

class GameplayUxReplayWin32Harness3D {
public:
  static constexpr UINT BarrierMessage = WM_APP + 0x314;
  static constexpr UINT FixedTickMessage = WM_APP + 0x315;
  static constexpr float FixedDeltaTime = 1.f / 120.f;

  explicit GameplayUxReplayWin32Harness3D(IGameplayUxReplayAdapter3D &adapter);
  ~GameplayUxReplayWin32Harness3D();
  GameplayUxReplayWin32Harness3D(const GameplayUxReplayWin32Harness3D &) =
      delete;
  GameplayUxReplayWin32Harness3D &
  operator=(const GameplayUxReplayWin32Harness3D &) = delete;

  [[nodiscard]] bool Initialize();
  [[nodiscard]] HWND Window() const { return window_; }
  [[nodiscard]] std::uint64_t LastAcknowledgedSequence() const {
    return acknowledgedSequence_;
  }
  [[nodiscard]] const std::vector<ReplayReceipt3D> &Receipts() const {
    return receipts_;
  }

  [[nodiscard]] bool Key(unsigned key, bool down, bool system = false);
  [[nodiscard]] bool MouseMove(int x, int y);
  [[nodiscard]] bool RawMouse(int deltaX, int deltaY);
  [[nodiscard]] bool MouseButton(bool down, bool secondary = false);
  [[nodiscard]] bool Wheel(short delta);
  [[nodiscard]] bool Focus(bool active);
  [[nodiscard]] bool Tick();
  [[nodiscard]] bool Close();

private:
  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                                     LPARAM lParam);
  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
  bool Dispatch(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
  void Acknowledge(UINT message);
  void ReleaseInputAndCursor();

  IGameplayUxReplayAdapter3D &adapter_;
  Aevrynd::Input input_{};
  HWND window_{};
  std::uint64_t eventSequence_{}, acknowledgedSequence_{}, fixedTick_{};
  ReplayEdges3D edges_{};
  std::vector<PointerEvent3D> pointerEvents_{};
  std::vector<ReplayReceipt3D> receipts_{};
  bool focused_{true}, primaryDown_{}, secondaryDown_{};
};

[[nodiscard]] std::uint64_t HashReplayState3D(const ReplayState3D &state);

} // namespace GameplayLab::UxReplay
