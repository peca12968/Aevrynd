#pragma once

#include "RiverbankExpedition.h"

namespace Mistwatch {
enum class TrailSupplyAction : std::uint8_t { View, Exchange };
enum class TrailSupplyResult : std::uint8_t { Viewed, Exchanged, NotEnough, TooFar, Downed, AlreadyOwned, Rejected };

struct RiverbankTrailSupplies {
    static constexpr unsigned HideCost = 2;
    static constexpr unsigned RecoveryPulseTicks = 5;
    static constexpr unsigned RecoveryPerPulse(bool equipped) { return equipped ? 2u : 1u; }
    static constexpr bool AtCampBoard(ArenaPosition position) {
        return RiverbankExpedition::CanInteract(ExpeditionStage::Available, position);
    }
};

struct TrailSupplyState {
    unsigned earnedHides{};
    bool ownsPack{};

    unsigned AvailableHides() const {
        return ownsPack ? earnedHides - RiverbankTrailSupplies::HideCost : earnedHides;
    }
};
}
