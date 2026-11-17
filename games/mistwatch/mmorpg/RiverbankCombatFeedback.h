#pragma once

#include "RiverbankBattleWire.h"

namespace Mistwatch {

class RiverbankCombatFeedback {
public:
    enum class Action { None, Attack, Skill };
    static constexpr float NoticeSeconds = 1.6f;
    static constexpr float RewardNoticeSeconds = 5.f;

    void Update(const RiverbankBattleWire::Encounter* frame, float elapsed) {
        if (elapsed > 0.f) {
            healthRemaining_ = std::max(0.f, healthRemaining_ - elapsed);
            actionRemaining_ = std::max(0.f, actionRemaining_ - elapsed);
            rewardRemaining_ = std::max(0.f, rewardRemaining_ - elapsed);
        }
        if (!frame) {
            last_.reset();
            ClearNotices();
            rewardRemaining_ = 0.f;
            return;
        }
        UpdateRewards(*frame);
        if (!last_ || frame->epoch != last_->epoch || frame->owner != last_->owner ||
            frame->player.playerClass != last_->player.playerClass) {
            last_ = *frame;
            ClearNotices();
            return;
        }
        if (frame->tick <= last_->tick) return;
        // 跨越大段缺失狀態時，以新快照為基準，避免重連或延遲期間的變化被當成立即回饋。
        if (frame->tick - last_->tick > 6) {
            last_ = *frame;
            ClearNotices();
            return;
        }

        const auto& current = frame->player;
        const auto& previous = last_->player;
        if (current.health != previous.health) {
            healthDelta_ = static_cast<int>(current.health) - static_cast<int>(previous.health);
            recovered_ = previous.health == 0 && current.health != 0;
            healthRemaining_ = NoticeSeconds;
        }
        // 伺服器只在成功結算攻擊或技能時重設對應冷卻。
        if (current.skillCooldown > previous.skillCooldown) {
            action_ = Action::Skill;
            actionRemaining_ = NoticeSeconds;
        } else if (current.attackCooldown > previous.attackCooldown) {
            action_ = Action::Attack;
            actionRemaining_ = NoticeSeconds;
        }
        last_ = *frame;
    }

    int HealthDelta() const { return healthRemaining_ > 0.f ? healthDelta_ : 0; }
    bool Recovered() const { return healthRemaining_ > 0.f && recovered_; }
    Action LatestAction() const { return actionRemaining_ > 0.f ? action_ : Action::None; }
    unsigned RewardDelta() const { return rewardRemaining_ > 0.f ? rewardDelta_ : 0; }

private:
    struct RewardBaseline {
        std::uint64_t world, tick;
        RiverbankClass role;
        unsigned total;
    };

    void UpdateRewards(const RiverbankBattleWire::Encounter& frame) {
        const auto& player = frame.player;
        if (!rewardBaseline_ || rewardBaseline_->world != frame.epoch ||
            rewardBaseline_->role != player.playerClass) {
            rewardBaseline_ = RewardBaseline{frame.epoch, frame.tick, player.playerClass, player.rewards};
            rewardRemaining_ = 0.f;
            return;
        }
        if (frame.tick <= rewardBaseline_->tick) return;
        rewardBaseline_->tick = frame.tick;
        // 戰利品永久累計。保留同世界的結算基準，讓斷線期間的發獎可在重連時顯示一次。
        if (player.rewards > rewardBaseline_->total) {
            rewardDelta_ = RewardDelta() + (player.rewards - rewardBaseline_->total);
            rewardBaseline_->total = player.rewards;
            rewardRemaining_ = RewardNoticeSeconds;
        }
    }

    void ClearNotices() { healthRemaining_ = actionRemaining_ = 0.f; }

    std::optional<RiverbankBattleWire::Encounter> last_;
    std::optional<RewardBaseline> rewardBaseline_;
    float healthRemaining_{}, actionRemaining_{}, rewardRemaining_{};
    unsigned rewardDelta_{};
    int healthDelta_{};
    Action action_{};
    bool recovered_{};
};

}
