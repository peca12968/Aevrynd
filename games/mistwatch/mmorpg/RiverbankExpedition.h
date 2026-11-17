#pragma once

#include "RiverbankEncounter.h"
#include <optional>

namespace Mistwatch {
enum class ExpeditionStage : std::uint8_t {
    Available, CheckTrail, Hunt, Report, Complete,
    WatchAvailable, WatchEast, WatchWest, WatchReport, WatchRecorded, WatchEastRemaining, WatchEither
};
enum class ExpeditionResult : std::uint8_t { Viewed, Advanced, TooFar, HuntRequired, Finished, Downed };

struct RiverbankExpedition {
    static constexpr ArenaPosition Board{1.9f, 2.8f};
    static constexpr ArenaPosition Trail{0.f, 26.f};
    static constexpr ArenaPosition EastWatch{21.f, 54.f};
    static constexpr ArenaPosition WestWatch{-15.f, 73.f};
    static constexpr float Reach = 2.4f;
    static constexpr float ShareReach = 5.f;

    enum class Post { East, West };

    static constexpr bool IsWatchPost(ExpeditionStage stage) {
        return stage == ExpeditionStage::WatchEast || stage == ExpeditionStage::WatchWest ||
            stage == ExpeditionStage::WatchEastRemaining || stage == ExpeditionStage::WatchEither;
    }

    static constexpr bool HasDestination(ExpeditionStage stage) {
        return stage < ExpeditionStage::Complete ||
            (stage >= ExpeditionStage::WatchAvailable && stage < ExpeditionStage::WatchRecorded) || IsWatchPost(stage);
    }

    static constexpr ArenaPosition Destination(ExpeditionStage stage) {
        if (stage == ExpeditionStage::CheckTrail) return Trail;
        if (stage == ExpeditionStage::Hunt) return RiverbankEncounter::NorthBankCenter;
        if (stage == ExpeditionStage::WatchEast || stage == ExpeditionStage::WatchEastRemaining ||
            stage == ExpeditionStage::WatchEither) return EastWatch;
        if (stage == ExpeditionStage::WatchWest) return WestWatch;
        return Board;
    }

    static constexpr bool CanInteract(ExpeditionStage stage, ArenaPosition player) {
        if (stage == ExpeditionStage::Hunt || !HasDestination(stage)) return false;
        if (stage == ExpeditionStage::WatchEither)
            return PostAt(ExpeditionStage::WatchEast, player).has_value();
        return Within(player, Destination(stage), Reach);
    }

    static constexpr std::optional<ExpeditionStage> AfterPost(ExpeditionStage stage, Post post) {
        if (stage == ExpeditionStage::WatchEast)
            return post == Post::East ? ExpeditionStage::WatchWest : ExpeditionStage::WatchEastRemaining;
        if ((stage == ExpeditionStage::WatchWest && post == Post::West) ||
            (stage == ExpeditionStage::WatchEastRemaining && post == Post::East))
            return ExpeditionStage::WatchReport;
        return {};
    }

    static constexpr ArenaPosition PostLocation(Post post) {
        return post == Post::East ? EastWatch : WestWatch;
    }

    static constexpr std::optional<Post> PostAt(ExpeditionStage stage, ArenaPosition player) {
        for (const auto post : {Post::East, Post::West})
            if (AfterPost(stage, post) && Within(player, PostLocation(post), Reach)) return post;
        return {};
    }

    static constexpr bool CanSharePost(Post post, ArenaPosition player) {
        return Within(player, PostLocation(post), ShareReach);
    }

    static constexpr ExpeditionStage RecommendedDestination(ExpeditionStage stage, ArenaPosition player) {
        if (stage != ExpeditionStage::WatchEither) return stage;
        return DistanceSquared(player, WestWatch) < DistanceSquared(player, EastWatch) ?
            ExpeditionStage::WatchWest : ExpeditionStage::WatchEast;
    }

private:
    static constexpr float DistanceSquared(ArenaPosition player, ArenaPosition destination) {
        const float x = destination.x - player.x, z = destination.z - player.z;
        return x * x + z * z;
    }

    static constexpr bool Within(ArenaPosition player, ArenaPosition destination, float reach) {
        return DistanceSquared(player, destination) <= reach * reach;
    }
};
}
