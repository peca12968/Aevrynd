#pragma once

#include <Aevrynd/Math.h>

#include <cmath>
#include <cstdint>
#include <string_view>

namespace GameplayLab {

enum class ForestTrialPhase3D : std::uint8_t {
  DefeatGuardian,
  ReturnToCottage,
  Complete
};

class GameplayForestTrial3D {
public:
  [[nodiscard]] bool Reset(Aevrynd::Vec3 cottageEntry,
                           float completionRadius = 2.25f) noexcept {
    if (!Finite(cottageEntry) || !std::isfinite(completionRadius) ||
        completionRadius < .5f || completionRadius > 6.f)
      return false;
    cottageEntry_ = cottageEntry;
    completionRadius_ = completionRadius;
    phase_ = ForestTrialPhase3D::DefeatGuardian;
    transitions_ = 0;
    initialized_ = true;
    return true;
  }

  [[nodiscard]] bool Tick(bool guardianAlive,
                          Aevrynd::Vec3 playerFeet) noexcept {
    if (!initialized_ || !Finite(playerFeet)) return false;
    if (phase_ == ForestTrialPhase3D::DefeatGuardian && !guardianAlive) {
      phase_ = ForestTrialPhase3D::ReturnToCottage;
      ++transitions_;
      return true;
    }
    if (phase_ == ForestTrialPhase3D::ReturnToCottage &&
        DistanceXZ(playerFeet, cottageEntry_) <= completionRadius_) {
      phase_ = ForestTrialPhase3D::Complete;
      ++transitions_;
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
  [[nodiscard]] float DistanceToCottage(Aevrynd::Vec3 playerFeet) const noexcept {
    return initialized_ && Finite(playerFeet)
               ? DistanceXZ(playerFeet, cottageEntry_)
               : 0.f;
  }
  [[nodiscard]] std::string_view ObjectiveLabel() const noexcept {
    switch (phase_) {
    case ForestTrialPhase3D::DefeatGuardian:
      return "DEFEAT THE FOREST GUARDIAN";
    case ForestTrialPhase3D::ReturnToCottage:
      return "RETURN TO THE COTTAGE";
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

  Aevrynd::Vec3 cottageEntry_{};
  float completionRadius_{2.25f};
  ForestTrialPhase3D phase_{ForestTrialPhase3D::DefeatGuardian};
  std::uint64_t transitions_{};
  bool initialized_{};
};

} // namespace GameplayLab
