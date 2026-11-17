#pragma once

#include "RiverbankEncounter.h"
#include <Aevrynd/Math.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Mistwatch {
class RiverbankPeerPresentation {
public:
    static constexpr float TickSeconds = .05f;

    // 只在已確認的兩點之間插值。停更時到達最後一點就停止。
    [[nodiscard]] bool Observe(std::uint64_t tick, Aevrynd::Vec3 position, bool downed, bool supportsRun = false, bool dodging = false) {
        if (lastTick && tick <= *lastTick) return false;
        const auto step = position - target;
        const auto tickGap = lastTick ? tick - *lastTick : 0;
        // 閃避結束的快照可能涵蓋上一刻的短衝，仍以最近的權威標記限制插值。
        const bool dodgeInterval = dodging || wasDodging;
        const float speedLimit = dodgeInterval ? RiverbankEncounter::DodgeStepMeters : .16f;
        const float limit = (supportsRun || dodgeInterval) && tickGap <= 3
            ? std::max(.32f, speedLimit * static_cast<float>(tickGap) + .002f) : .32f;
        const bool snap = !lastTick || tickGap > 3 || std::hypot(step.x, step.z) > limit || downed != wasDowned;
        lastTick = tick;
        wasDowned = downed;
        wasDodging = dodging;
        target = position;
        if (snap) {
            shown = target;
            remaining = 0.f;
        } else {
            remaining = TickSeconds * static_cast<float>(tickGap);
        }
        return snap;
    }

    Aevrynd::Vec3 Advance(float dt) {
        if (remaining <= 0.f || dt <= 0.f) return {};
        const auto before = shown;
        if (dt >= remaining) {
            shown = target;
            remaining = 0.f;
        } else {
            shown = shown + (target - shown) * (dt / remaining);
            remaining -= dt;
        }
        return shown - before;
    }

    std::optional<Aevrynd::Vec3> Position() const {
        if (!lastTick) return {};
        return shown;
    }

private:
    std::optional<std::uint64_t> lastTick;
    Aevrynd::Vec3 shown{}, target{};
    float remaining{};
    bool wasDowned{}, wasDodging{};
};
}
