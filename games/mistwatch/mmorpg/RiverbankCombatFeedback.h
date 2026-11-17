#pragma once

#include "RiverbankBattleWire.h"

namespace Mistwatch {

class RiverbankCombatFeedback {
public:
    enum class Action { None, Attack, Skill };
    static constexpr float NoticeSeconds = 1.6f;

    void Update(const RiverbankBattleWire::Encounter* frame, float elapsed) {
        if (elapsed > 0.f) {
            healthRemaining_ = std::max(0.f, healthRemaining_ - elapsed);
            actionRemaining_ = std::max(0.f, actionRemaining_ - elapsed);
        }
        if (!frame) {
            last_.reset();
            ClearNotices();
            return;
        }
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

private:
    void ClearNotices() { healthRemaining_ = actionRemaining_ = 0.f; }

    std::optional<RiverbankBattleWire::Encounter> last_;
    float healthRemaining_{}, actionRemaining_{};
    int healthDelta_{};
    Action action_{};
    bool recovered_{};
};

}
