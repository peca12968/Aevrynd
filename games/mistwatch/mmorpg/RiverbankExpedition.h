#pragma once

#include "RiverbankEncounter.h"

namespace Mistwatch {
enum class ExpeditionStage : std::uint8_t { Available, CheckTrail, Hunt, Report, Complete };
enum class ExpeditionResult : std::uint8_t { Viewed, Advanced, TooFar, HuntRequired, Finished, Downed };

struct RiverbankExpedition {
    static constexpr ArenaPosition Board{1.9f, 2.8f};
    static constexpr ArenaPosition Trail{0.f, 26.f};
    static constexpr float Reach = 2.4f;

    static constexpr ArenaPosition Destination(ExpeditionStage stage) {
        if (stage == ExpeditionStage::CheckTrail) return Trail;
        if (stage == ExpeditionStage::Hunt) return RiverbankEncounter::NorthBankCenter;
        return Board;
    }

    static constexpr bool CanInteract(ExpeditionStage stage, ArenaPosition player) {
        if (stage == ExpeditionStage::Hunt || stage == ExpeditionStage::Complete) return false;
        const auto destination = Destination(stage);
        const float x = destination.x - player.x, z = destination.z - player.z;
        return x * x + z * z <= Reach * Reach;
    }
};
}
