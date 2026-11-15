#pragma once

#include "RiverbankGuild.h"
#include <algorithm>
#include <span>
#include <vector>

namespace Mistwatch {
class RiverbankTerritory {
public:
    static constexpr float CenterX = 14.f, CenterZ = 54.f, Radius = 3.f;
    static constexpr unsigned HoldTicks = 240;

    struct Presence {
        std::int64_t profile{}, guild{};
        GuildRank rank{};
    };

    static bool Contains(float x, float z) {
        const float dx = x - CenterX, dz = z - CenterZ;
        return dx * dx + dz * dz <= Radius * Radius;
    }

    std::int64_t Owner() const { return owner_; }
    std::int64_t Challenger() const { return challenger_; }
    unsigned Progress() const { return progress_; }
    bool Contested() const { return contested_; }

    void ApplyOwnership(std::int64_t owner) {
        owner_ = owner;
        challenger_ = 0;
        progress_ = 0;
        contested_ = false;
    }

    std::vector<std::int64_t> Tick(std::span<const Presence> players) {
        if (players.empty()) {
            ApplyOwnership(owner_);
            return {};
        }
        const auto firstGuild = players.front().guild;
        if (challenger_ && std::ranges::none_of(players, [this](const Presence& player) { return player.guild == challenger_; })) {
            challenger_ = 0;
            progress_ = 0;
        }
        contested_ = std::ranges::any_of(players, [firstGuild](const Presence& player) { return player.guild != firstGuild; });
        if (contested_ || firstGuild == owner_) return {};
        if (!challenger_) {
            if (std::ranges::none_of(players, [](const Presence& player) { return player.rank >= GuildRank::Officer; })) return {};
            challenger_ = firstGuild;
        }
        if (++progress_ != HoldTicks) return {};

        // 先提交領地交易，才同步新的歸屬。
        std::vector<std::int64_t> recipients;
        recipients.reserve(players.size());
        for (const auto& player : players) recipients.push_back(player.profile);
        return recipients;
    }

private:
    std::int64_t owner_{}, challenger_{};
    unsigned progress_{};
    bool contested_{};
};

struct TerritoryState {
    std::int64_t revision{}, owner{}, challenger{};
    unsigned progress{}, rewards{};
    bool contested{};
    std::string ownerName, challengerName;
};
}
