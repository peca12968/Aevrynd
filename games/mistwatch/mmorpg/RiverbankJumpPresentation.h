#pragma once

#include "RiverbankJump.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Mistwatch {
class RiverbankJumpPresentation {
public:
    void Observe(std::uint64_t tick, float height, float delta, bool enabled) {
        if (!enabled) {
            *this = {};
            return;
        }
        if (tick <= observedTick) return;
        observedTick = tick;
        if (height <= GroundTolerance && airborne) {
            // 返營或倒地會清空位移。只有符合權威落地的一刻才接落地動作。
            recovering = std::abs(delta + RiverbankJump::Height(RiverbankJump::FlightTicks - 1)) <= .003f;
            recovery = 0.f;
        }
        if (height > GroundTolerance) recovering = false;
        verticalDelta = delta;
        airborne = height > GroundTolerance;
    }

    void Advance(float dt, float visibleHeight) {
        if (!recovering || visibleHeight > GroundTolerance) return;
        recovery = std::min(RecoverySeconds, recovery + std::clamp(dt, 0.f, .1f));
        if (recovery >= RecoverySeconds) recovering = false;
    }

    [[nodiscard]] std::optional<float> ClipTime(float visibleHeight) const {
        if (visibleHeight > GroundTolerance && (airborne || recovering)) {
            // 用現有權威高度、垂直位移判斷在弧線的哪一側，晚加入也能得到正確姿勢。
            const float radius = std::sqrt(std::clamp(1.f - visibleHeight / RiverbankJump::MaximumHeightMeters, 0.f, 1.f));
            const float phase = .5f * (1.f + (verticalDelta < 0.f ? radius : -radius));
            return FlightClipStart + (LandingClipStart - FlightClipStart) * phase;
        }
        if (recovering) return LandingClipStart + recovery;
        return {};
    }

    static constexpr float GroundTolerance = .002f;
    static constexpr float FlightClipStart = .20f;
    static constexpr float LandingClipStart = .80f;
    static constexpr float RecoverySeconds = .20f;
private:
    std::uint64_t observedTick{};
    float verticalDelta{}, recovery{};
    bool airborne{}, recovering{};
};
}
