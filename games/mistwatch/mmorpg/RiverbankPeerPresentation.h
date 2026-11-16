#pragma once

#include <Aevrynd/Math.h>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Mistwatch {
class RiverbankPeerPresentation {
public:
    static constexpr float TickSeconds = .05f;

    // 只在已確認的兩點之間插值。停更時到達最後一點就停止。
    [[nodiscard]] bool Observe(std::uint64_t tick, Aevrynd::Vec3 position, bool downed) {
        if (lastTick && tick <= *lastTick) return false;
        const auto step = position - target;
        const auto tickGap = lastTick ? tick - *lastTick : 0;
        const bool snap = !lastTick || tickGap > 3 || std::hypot(step.x, step.z) > .32f || downed != wasDowned;
        lastTick = tick;
        wasDowned = downed;
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
    bool wasDowned{};
};
}
