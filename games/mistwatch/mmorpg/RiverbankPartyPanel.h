#pragma once

#include "LocalWorldClient.h"
#include <Aevrynd/Engine.h>

namespace Mistwatch {
class RiverbankPartyPanel {
public:
    bool Open() const noexcept { return open_; }

    void Toggle(const Aevrynd::Input& input, const LocalWorldClient& client, bool guildOpen) {
        if (!client.CanUseGuild()) {
            open_ = false;
            renderedSequence_ = selected_ = 0;
            message_ = {};
            nextView_ = {};
            return;
        }
        if (guildOpen || !input.Focused() || input.FocusLost()) return;
        if (input.Pressed('P')) open_ = !open_;
        else if (input.Pressed(VK_ESCAPE)) open_ = false;
    }

    void Update(const Aevrynd::Input& input, LocalWorldClient& client) {
        if (!client.CanUseGuild()) return;
        const auto* reply = client.Party();
        if (reply && reply->sequence != renderedSequence_) {
            renderedSequence_ = reply->sequence;
            if (requested_ != PartyAction::View) message_ = reply->result == PartyResult::Ok ? ResultText(requested_) :
                reply->result == PartyResult::Full ? "隊伍已滿" : "操作未完成，請確認最新隊伍狀態";
        }
        if (open_ && input.Focused() && !input.FocusLost() && reply) {
            SelectTarget(input, client);
            if (reply->inviter && input.Pressed('J')) Send(client, PartyAction::Accept, reply->inviter);
            else if (reply->inviter && input.Pressed('K')) Send(client, PartyAction::Decline, reply->inviter);
            else if (reply->members && input.Pressed('L')) Send(client, PartyAction::Leave);
            else if (selected_ && (!reply->members || reply->leader == client.PlayerId()) && input.Pressed('I'))
                Send(client, PartyAction::Invite, selected_);
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextView_ && Send(client, PartyAction::View)) nextView_ = now + std::chrono::seconds(1);
    }

    void Draw(Aevrynd::DrawList& draw, const LocalWorldClient& client) const {
        if (!client.CanUseGuild()) return;
        const auto* reply = client.Party();
        draw.Quad({109,232}, {195,27}, {.041f,.068f,.055f,.95f});
        draw.SystemText({20, 218}, reply && reply->inviter ? "P 隊伍 · 有新邀請" : "P 隊伍", 15.f, {.78f,.91f,.83f,1});
        if (!open_) return;
        draw.Quad({640, 389}, {760, 368}, {.035f,.065f,.052f,.97f});
        draw.SystemText({282, 226}, "河岸隊伍", 25.f, {.81f,.97f,.86f,1});
        Line(draw, 267, "你的玩家編號 " + std::to_string(client.PlayerId()) + "   ·   P 或 Esc 返回");
        if (!reply) { Line(draw, 304, "讀取隊伍資料中"); return; }
        if (reply->inviter) {
            Line(draw, 306, "玩家 " + std::to_string(reply->inviter) + " 邀請你組隊");
            Line(draw, 339, "J 接受  ·  K 婉拒");
        } else if (reply->members) {
            Line(draw, 305, "隊伍 " + std::to_string(reply->members) + " / 4 位   ·   L 離隊");
            float row = 333.f;
            for (const auto& member : reply->online) {
                std::string text = member.actor == reply->leader ? "隊長 " : "隊員 ";
                text += std::to_string(member.actor) + "   " + std::string(RiverbankClassName(member.playerClass)) +
                    "   ·   生命 " + std::to_string(member.health) + " / " + std::to_string(CombatStatsFor(member.playerClass).health);
                Line(draw, row, text);
                row += 27.f;
            }
            const auto offline = reply->members - reply->online.size();
            if (offline) Line(draw, row, std::to_string(offline) + " 位隊員正在重連");
        } else {
            Line(draw, 305, "目前未加入隊伍");
        }
        if (!reply->inviter && (!reply->members || reply->leader == client.PlayerId()) && reply->members < RiverbankParty::MaxMembers) {
            if (selected_) Line(draw, 484, "↑↓ 選取玩家 " + std::to_string(selected_) + "   ·   I 邀請");
            else Line(draw, 484, "目前沒有其他可邀請的在線玩家");
        }
        if (!message_.empty()) Line(draw, 527, message_);
    }

private:
    bool open_{};
    std::uint32_t selected_{};
    std::uint64_t renderedSequence_{};
    PartyAction requested_{PartyAction::View};
    std::chrono::steady_clock::time_point nextView_{};
    std::string_view message_;

    static void Line(Aevrynd::DrawList& draw, float y, std::string_view text) {
        draw.SystemText({282, y}, text, 16.f, {.89f,.94f,.90f,1});
    }

    static std::string_view ResultText(PartyAction action) {
        switch (action) {
        case PartyAction::Invite: return "隊伍邀請已送出";
        case PartyAction::Accept: return "已加入隊伍";
        case PartyAction::Decline: return "已婉拒邀請";
        case PartyAction::Leave: return "已離開隊伍";
        default: return {};
        }
    }

    bool Send(LocalWorldClient& client, PartyAction action, std::uint32_t target = 0) {
        if (!client.RequestParty(action, target)) return false;
        requested_ = action;
        return true;
    }

    void SelectTarget(const Aevrynd::Input& input, const LocalWorldClient& client) {
        std::vector<std::uint32_t> players;
        if (const auto* view = client.Snapshot()) {
            const auto* roster = client.Party();
            for (const auto& entity : view->entities) {
                const auto id = static_cast<std::uint32_t>(entity.id.low);
                if (id != client.PlayerId() && (!roster ||
                    std::ranges::find(roster->online, id, &RiverbankPartyWire::Member::actor) == roster->online.end())) players.push_back(id);
            }
        }
        if (players.empty()) { selected_ = 0; return; }
        auto selected = std::ranges::find(players, selected_);
        if (selected == players.end()) selected = players.begin();
        else if (input.Pressed(VK_DOWN)) { if (++selected == players.end()) selected = players.begin(); }
        else if (input.Pressed(VK_UP)) { if (selected == players.begin()) selected = players.end(); --selected; }
        selected_ = *selected;
    }
};
}
