#pragma once

#include "LocalWorldClient.h"
#include <Aevrynd/Engine.h>

namespace Mistwatch {
class RiverbankGearPanel {
public:
    bool Open() const noexcept { return open_; }

    void Toggle(const Aevrynd::Input& input, const LocalWorldClient& client, bool otherOpen) {
        if (!client.CanUseGuild()) { open_ = false; needsView_ = false; return; }
        if (otherOpen || !input.Focused() || input.FocusLost()) return;
        if (input.Pressed('B')) {
            open_ = !open_;
            needsView_ = open_;
        } else if (input.Pressed(VK_ESCAPE)) open_ = false;
    }

    void Update(const Aevrynd::Input& input, LocalWorldClient& client) {
        if (!open_) return;
        if (needsView_) {
            if (client.RequestGear(GearAction::View)) needsView_ = false;
            return;
        }
        const auto* gear = client.Gear();
        if (gear && gear->gear.earned && input.Focused() && !input.FocusLost() && input.Pressed(VK_RETURN))
            (void)client.RequestGear(gear->gear.equipped ? GearAction::Unequip : GearAction::Equip);
    }

    void Draw(Aevrynd::DrawList& draw, const LocalWorldClient& client) const {
        if (!client.CanUseGuild()) return;
        draw.Quad({80,262}, {137,27}, {.041f,.068f,.055f,.95f});
        draw.SystemText({20,248}, "B 裝備", 15.f, {.78f,.91f,.83f,1});
        if (!open_) return;
        draw.Quad({640,376}, {730,302}, {.035f,.065f,.052f,.97f});
        draw.SystemText({300,251}, "首領戰利品", 23.f, {.81f,.97f,.86f,1});
        const auto role = client.SelectedClass();
        draw.SystemText({300,302}, BossGearName(role), 21.f, {.94f,.85f,.58f,1});
        Line(draw, 340, Benefit(role));
        const auto* gear = client.Gear();
        if (!gear || needsView_) Line(draw, 384, "正在讀取裝備狀態");
        else if (!gear->gear.earned) Line(draw, 384, "參戰並取得巡林野豬的首領戰利品後可裝備");
        else if (gear->gear.equipped) Line(draw, 384, "已裝備 · Enter 卸下");
        else Line(draw, 384, "已取得 · Enter 裝備");
        Line(draw, 452, "B 或 Esc 返回");
    }

private:
    bool open_{}, needsView_{};

    static void Line(Aevrynd::DrawList& draw, float y, std::string_view text) {
        draw.SystemText({300,y}, text, 16.f, {.89f,.94f,.90f,1});
    }

    static std::string Benefit(RiverbankClass role) {
        using Gear = RiverbankBossGearStats;
        switch (role) {
        case RiverbankClass::Guardian: return "護衛期間受到的首領傷害降至 " + std::to_string(Gear::GuardDamageTaken);
        case RiverbankClass::Ranger: return "連射傷害提高至 " + std::to_string(Gear::VolleyDamage);
        case RiverbankClass::Priest: return "治療量提高至 " + std::to_string(Gear::HealAmount);
        default: return {};
        }
    }
};
}
