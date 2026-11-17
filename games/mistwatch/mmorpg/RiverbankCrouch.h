#pragma once

#include "RiverbankStructureMovement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace Mistwatch {
inline constexpr std::uint32_t RiverbankCrouchCapability = 1u << 31;
inline constexpr std::uint32_t RiverbankCrouchedFlag = 1u << 1;

namespace RiverbankCrouchWire {
inline constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'C'}, std::byte{'S'}};
inline bool HasMagic(std::span<const std::byte> data) {
    return data.size() >= Magic.size() && std::ranges::equal(data.first(Magic.size()), Magic);
}
inline auto Encode(bool want) {
    return std::array{Magic[0], Magic[1], Magic[2], Magic[3], std::byte{1}, std::byte(want)};
}
inline std::optional<bool> Decode(std::span<const std::byte> data) {
    if (data.size() != 6 || !HasMagic(data) || data[4] != std::byte{1} || data[5] > std::byte{1}) return {};
    return data[5] == std::byte{1};
}
}

class RiverbankCrouch {
public:
    // 沿用單人版的站蹲高度及行走比例。
    inline static constexpr float CrouchHeight = 1.4f;
    inline static constexpr float MaximumStep = .045f;

    void Request(bool wants) noexcept { requested_ = wants; }
    bool Active() const noexcept { return active_; }
    bool Requested() const noexcept { return requested_; }
    Aevrynd::Vec3 Body() const noexcept {
        auto body = RiverbankStructureMovement::PlayerBody;
        if (active_ && loweringTicks_ >= LoweringTicks) body.y = CrouchHeight * .5f;
        return body;
    }

    void Advance(ArenaPosition feet, bool grounded, bool available) {
        if (!available) {
            active_ = false;
            loweringTicks_ = 0;
        } else if (grounded) {
            const bool lacksHeadroom = !RiverbankStructureMovement::CanStandInShelter(feet);
            active_ = requested_ || lacksHeadroom;
            // 早一拍同步姿勢。屋外的角色先降低身體，再用較低的碰撞體通過屋簷。
            if (!active_) loweringTicks_ = 0;
            else if (lacksHeadroom) loweringTicks_ = LoweringTicks;
            else if (loweringTicks_ < LoweringTicks) ++loweringTicks_;
        }
    }

    ArenaPosition Constrain(ArenaPosition delta) const noexcept {
        if (!active_) return delta;
        const auto length = std::hypot(delta.x, delta.z);
        const auto scale = length > MaximumStep ? MaximumStep / length : 1.f;
        return {delta.x * scale, delta.z * scale};
    }

private:
    static constexpr unsigned LoweringTicks = 3;
    unsigned loweringTicks_{};
    bool requested_{}, active_{};
};
}
