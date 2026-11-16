#pragma once

#include "RiverbankEncounter.h"

namespace Mistwatch {
enum class ExpeditionStage : std::uint8_t {
    Available, CheckTrail, Hunt, Report, Complete,
    WatchAvailable, WatchEast, WatchWest, WatchReport, WatchRecorded
};
enum class ExpeditionResult : std::uint8_t { Viewed, Advanced, TooFar, HuntRequired, Finished, Downed };

struct RiverbankExpedition {
    static constexpr ArenaPosition Board{1.9f, 2.8f};
    static constexpr ArenaPosition Trail{0.f, 26.f};
    static constexpr ArenaPosition EastWatch{21.f, 54.f};
    static constexpr ArenaPosition WestWatch{-15.f, 73.f};
    static constexpr float Reach = 2.4f;
    static constexpr float ShareReach = 5.f;

    static constexpr bool IsWatchPost(ExpeditionStage stage) {
        return stage == ExpeditionStage::WatchEast || stage == ExpeditionStage::WatchWest;
    }

    static constexpr bool HasDestination(ExpeditionStage stage) {
        return stage < ExpeditionStage::Complete ||
            (stage >= ExpeditionStage::WatchAvailable && stage < ExpeditionStage::WatchRecorded);
    }

    static constexpr ArenaPosition Destination(ExpeditionStage stage) {
        if (stage == ExpeditionStage::CheckTrail) return Trail;
        if (stage == ExpeditionStage::Hunt) return RiverbankEncounter::NorthBankCenter;
        if (stage == ExpeditionStage::WatchEast) return EastWatch;
        if (stage == ExpeditionStage::WatchWest) return WestWatch;
        return Board;
    }

    static constexpr bool CanInteract(ExpeditionStage stage, ArenaPosition player) {
        if (stage == ExpeditionStage::Hunt || !HasDestination(stage)) return false;
        return Within(stage, player, Reach);
    }

    static constexpr bool CanShareWatchPost(ExpeditionStage stage, ArenaPosition player) {
        return IsWatchPost(stage) && Within(stage, player, ShareReach);
    }

private:
    static constexpr bool Within(ExpeditionStage stage, ArenaPosition player, float reach) {
        const auto destination = Destination(stage);
        const float x = destination.x - player.x, z = destination.z - player.z;
        return x * x + z * z <= reach * reach;
    }
};
}
