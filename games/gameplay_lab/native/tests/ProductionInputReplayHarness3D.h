#pragma once

#include "GameplayInputReplay3D.h"
#include <Aevrynd/Camera.h>
#include <bit>
#include <cmath>
#include <cstdint>

namespace GameplayLab::Tests {

struct ProductionReplaySnapshot3D {
  CameraMode camera{CameraMode::ThirdPerson};
  Aevrynd::Camera3D cameraState{};
  Aevrynd::Vec3 position{};
  float characterHeight{1.8f};
  bool crouched{};
  std::uint64_t stateHash{};
  std::uint64_t cameraHash{};
};

class ProductionInputReplayHarness3D {
public:
  static constexpr float FixedDeltaTime = 1.f / 60.f;

  explicit ProductionInputReplayHarness3D(float yaw = 0.f) : yaw_(yaw) {}

  void SetKey(unsigned key, bool down) { input_.SetKey(key, down); }
  void SetHeadroom(bool clear) { rules_.SetHeadroom(clear); }

  ProductionReplaySnapshot3D Tick() {
    const ProductionInput3D mapped = ReadProductionInput3D(input_, yaw_);
    if (mapped.camera)
      rules_.SetCamera(*mapped.camera);
    rules_.Tick(mapped.tick, FixedDeltaTime);
    const auto snapshot = Snapshot();
    traceHash_ = Mix(traceHash_, snapshot.stateHash);
    traceHash_ = Mix(traceHash_, snapshot.cameraHash);
    input_.ConsumePresses();
    return snapshot;
  }

  ProductionReplaySnapshot3D Snapshot() const {
    ProductionReplaySnapshot3D result{};
    result.camera = rules_.Camera();
    result.position = rules_.CharacterPosition();
    result.characterHeight = rules_.CharacterHeight();
    result.crouched = rules_.Crouched();
    result.cameraState = MakeCamera(result.camera, result.position);
    result.cameraHash = HashCamera(result.cameraState);
    result.stateHash = HashState(result);
    return result;
  }

  const Rules3D &Rules() const { return rules_; }
  std::uint64_t TraceHash() const { return traceHash_; }

private:
  static std::uint64_t Mix(std::uint64_t hash, std::uint64_t value) {
    constexpr std::uint64_t prime = 1099511628211ull;
    for (unsigned byte = 0; byte < 8; ++byte) {
      hash ^= (value >> (byte * 8)) & 0xffu;
      hash *= prime;
    }
    return hash;
  }

  static std::uint64_t MixFloat(std::uint64_t hash, float value) {
    return Mix(hash, std::bit_cast<std::uint32_t>(value));
  }

  Aevrynd::Camera3D MakeCamera(CameraMode mode,
                               Aevrynd::Vec3 feet) const {
    const Aevrynd::Vec3 eye =
        feet + Aevrynd::Vec3{0.f, rules_.CharacterHeight() - .12f, 0.f};
    switch (mode) {
    case CameraMode::FirstPerson:
      return Aevrynd::MakeFirstPersonCamera(eye, yaw_, -.06f);
    case CameraMode::ThirdPerson:
      return Aevrynd::MakeThirdPersonCamera(
          feet + Aevrynd::Vec3{0.f, 1.1f, 0.f}, yaw_, .35f, 5.5f);
    case CameraMode::GodView:
      return Aevrynd::MakeOverheadCamera(feet, 42.f, true);
    }
    return {};
  }

  static std::uint64_t HashCamera(const Aevrynd::Camera3D &camera) {
    std::uint64_t hash = 14695981039346656037ull;
    const auto mixVector = [&](Aevrynd::Vec3 value) {
      hash = MixFloat(hash, value.x);
      hash = MixFloat(hash, value.y);
      hash = MixFloat(hash, value.z);
    };
    mixVector(camera.position);
    mixVector(camera.target);
    mixVector(camera.up);
    hash = Mix(hash, static_cast<std::uint64_t>(camera.projection.mode));
    hash = MixFloat(hash, camera.projection.verticalFieldOfView);
    hash = MixFloat(hash, camera.projection.orthographicHeight);
    hash = MixFloat(hash, camera.projection.nearPlane);
    hash = MixFloat(hash, camera.projection.farPlane);
    return hash;
  }

  static std::uint64_t HashState(const ProductionReplaySnapshot3D &state) {
    std::uint64_t hash = 14695981039346656037ull;
    hash = Mix(hash, static_cast<std::uint64_t>(state.camera));
    hash = MixFloat(hash, state.position.x);
    hash = MixFloat(hash, state.position.y);
    hash = MixFloat(hash, state.position.z);
    hash = MixFloat(hash, state.characterHeight);
    hash = Mix(hash, state.crouched ? 1u : 0u);
    return hash;
  }

  Aevrynd::Input input_{};
  Rules3D rules_{};
  float yaw_{};
  std::uint64_t traceHash_{14695981039346656037ull};
};

inline bool IsFinite(const ProductionReplaySnapshot3D &snapshot) {
  const auto finiteVector = [](Aevrynd::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
  };
  return finiteVector(snapshot.position) &&
         finiteVector(snapshot.cameraState.position) &&
         finiteVector(snapshot.cameraState.target) &&
         finiteVector(snapshot.cameraState.up) &&
         std::isfinite(snapshot.characterHeight);
}

} // namespace GameplayLab::Tests
