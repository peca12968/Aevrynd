#pragma once

#include "LocalWorldClient.h"
#include "RiverbankTerritoryHud.h"
#include <Aevrynd/Engine.h>
#include <algorithm>

namespace Mistwatch {
class RiverbankGuildPanel {
public:
    bool Open() const { return open_; }

    void OpenForCapture() { open_ = true; }

    void Toggle(const Aevrynd::Input& input, const LocalWorldClient& client) {
        if (!client.CanUseGuild()) { open_ = false; mode_ = Mode::Actions; return; }
        if (!input.Focused() || input.FocusLost()) return;
        if (open_ && input.Pressed(VK_ESCAPE)) {
            if (mode_ == Mode::Actions) open_ = false;
            mode_ = Mode::Actions;
            nextView_ = {};
        } else if (mode_ == Mode::Actions && input.Pressed('G')) {
            open_ = !open_;
            if (open_) nextView_ = {};
        }
    }

    void Update(const Aevrynd::Input& input, LocalWorldClient& client) {
        if (!client.CanUseGuild()) {
            open_ = false;
            mode_ = Mode::Actions;
            nextView_ = {};
            renderedSequence_ = selected_ = confirmTarget_ = 0;
            requested_ = GuildAction::View;
            message_.clear();
            return;
        }
        if (mode_ == Mode::History) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextView_ && client.RequestTerritoryHistory()) nextView_ = now + std::chrono::seconds(1);
            return;
        }
        if (mode_ == Mode::Roster) {
            UpdateRoster(input, client);
            return;
        }
        const auto* reply = client.GuildReply();
        if (reply && reply->sequence != renderedSequence_) {
            renderedSequence_ = reply->sequence;
            if (requested_ != GuildAction::View) message_ = ResultText(reply->result);
        }
        if (open_ && input.Focused() && !input.FocusLost()) {
            if (mode_ == Mode::Name) {
                EditName(input, client);
            } else if (mode_ != Mode::Actions) {
                if (input.Pressed(VK_RETURN) && Send(client, mode_ == Mode::Leave ? GuildAction::Leave : GuildAction::Transfer, confirmTarget_))
                    mode_ = Mode::Actions;
            } else if (input.Pressed('H') && client.CanReadTerritoryHistory()) {
                mode_ = Mode::History;
                nextView_ = {};
                return;
            } else if (input.Pressed('V') && reply && reply->state.guildId && client.CanReadGuildRoster()) {
                mode_ = Mode::Roster;
                rosterPage_ = 0;
                nextView_ = {};
                return;
            } else if (reply) {
                SelectTarget(input, client);
                Act(input, client, reply->state);
            }
        }
        const auto now = std::chrono::steady_clock::now();
        // 未入會玩家即使關閉面板也可收到邀請，已入會玩家在開啟面板時才更新。
        if ((open_ || !reply || !reply->state.guildId) && now >= nextView_ &&
            Send(client, GuildAction::View))
            nextView_ = now + std::chrono::seconds(open_ ? 1 : 2);
    }

    void Draw(Aevrynd::DrawList& draw, const LocalWorldClient& client) const {
        if (!client.CanUseGuild()) return;
        const auto* reply = client.GuildReply();
        const bool invited = reply && !reply->state.guildId && reply->state.inviteId;
        const float width = invited ? 195.f : 111.f;
        draw.Quad({9.5f + width * .5f, 202.f}, {width, 27.f}, {.041f,.068f,.055f,.95f});
        draw.SystemText({20, 188}, invited ? "G 公會 · 有新邀請" : "G 公會", 15.f,
                        invited ? Aevrynd::Color{.98f,.85f,.55f,1} : Aevrynd::Color{.78f,.91f,.83f,1});
        if (!open_) return;
        draw.Quad({640, 393}, {860, 376}, {.035f,.065f,.052f,.97f});
        draw.SystemText({246, 221}, "河岸公會", 25.f, {.81f,.97f,.86f,1});
        draw.SystemText({246, 266}, "你的玩家編號 " + std::to_string(client.PlayerId()) + "   ·   Esc 返回", 17.f, {1,1,1,1});
        if (mode_ == Mode::History) {
            Line(draw, 310, "北岸瞭望地 · 最近五次歸屬變更（UTC）");
            const auto* claims = client.TerritoryHistory();
            if (!claims) { Line(draw, 354, "讀取領地紀錄中"); return; }
            if (claims->empty()) { Line(draw, 354, "尚無領地佔領紀錄"); return; }
            float y = 354.f;
            for (const auto& claim : *claims) {
                const auto from = claim.previousOwner.empty() ? std::string("無") : RiverbankTerritoryDisplayName(claim.previousOwner);
                const auto date = claim.claimedUtc.substr(0, 10) + " " + claim.claimedUtc.substr(11, 8);
                Line(draw, y, "#" + std::to_string(claim.revision) + "  " + date + "   " + from + " → " +
                    RiverbankTerritoryDisplayName(claim.winner));
                y += 39.f;
            }
            return;
        }
        if (mode_ == Mode::Roster) {
            DrawRoster(draw, client);
            return;
        }
        if (!reply) { Line(draw, 311, "讀取公會資料中"); return; }
        const auto& guild = reply->state;
        Line(draw, 310, guild.guildId ? guild.name + "   ·   " + RankName(guild.rank) : "尚未加入公會");
        if (mode_ == Mode::Name) {
            Line(draw, 353, "輸入英文名稱，或用 Ctrl+V 貼上中文名稱");
            draw.Quad({640, 407}, {780, 45}, {.10f,.17f,.13f,1});
            Line(draw, 395, name_.empty() ? "公會名稱" : name_);
            Line(draw, 450, "Enter 建立  ·  Esc 取消");
        } else if (mode_ != Mode::Actions) {
            Line(draw, 353, mode_ == Mode::Leave ? "確定離開公會？單人成會時會解散公會。" :
                "確定將會長移交給玩家 " + std::to_string(confirmTarget_) + "？");
            Line(draw, 399, "Enter 確認  ·  Esc 取消");
        } else if (!guild.guildId) {
            Line(draw, 353, "N 建立公會");
            if (guild.inviteId) {
                Line(draw, 395, "收到「" + guild.inviteName + "」的邀請");
                Line(draw, 437, "J 接受  ·  K 婉拒");
            }
        } else {
            Line(draw, 353, selected_ ? "目前選取玩家 " + std::to_string(selected_) + "  ·  ↑↓ 切換" : "目前沒有其他在線玩家");
            if (guild.rank >= GuildRank::Officer) Line(draw, 395, "I 邀請選取的玩家");
            if (guild.rank == GuildRank::Leader) Line(draw, 437, "P 升為幹部  ·  O 改為成員  ·  T 移交會長");
            Line(draw, 479, "L 離開公會");
        }
        if (mode_ == Mode::Actions) {
            const bool showRoster = guild.guildId && client.CanReadGuildRoster();
            const bool showHistory = client.CanReadTerritoryHistory();
            if (showRoster || showHistory)
                Line(draw, 516, showRoster ? (showHistory ? "V 成員名單   ·   H 領地紀錄" : "V 成員名單") : "H 領地紀錄");
        }
        if (!message_.empty()) Line(draw, 551, message_);
    }

private:
    enum class Mode { Actions, Name, Leave, Transfer, History, Roster };
    bool open_{};
    Mode mode_{Mode::Actions};
    std::uint64_t selected_{}, confirmTarget_{}, renderedSequence_{};
    std::uint32_t rosterPage_{};
    GuildAction requested_{GuildAction::View};
    std::chrono::steady_clock::time_point nextView_{};
    std::string name_, message_;

    void UpdateRoster(const Aevrynd::Input& input, LocalWorldClient& client) {
        if (const auto* roster = client.GuildRoster(); roster && roster->total) {
            const auto last = (roster->total - 1) / GuildRosterPageSize;
            rosterPage_ = (std::min)(rosterPage_, last);
            if (rosterPage_ == roster->page && input.Focused() && !input.FocusLost()) {
                const auto previous = rosterPage_;
                if (input.Pressed(VK_LEFT) && rosterPage_ > 0) --rosterPage_;
                if (input.Pressed(VK_RIGHT) && rosterPage_ < last) ++rosterPage_;
                if (previous != rosterPage_) nextView_ = {};
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextView_ && client.RequestGuildRoster(rosterPage_))
            nextView_ = now + std::chrono::seconds(1);
    }

    void DrawRoster(Aevrynd::DrawList& draw, const LocalWorldClient& client) const {
        const auto* roster = client.GuildRoster();
        if (!roster || roster->page != rosterPage_) { Line(draw, 311, "讀取成員名單中"); return; }
        if (!roster->guildId) { Line(draw, 311, "目前沒有公會成員資料"); return; }
        const auto pages = (roster->total - 1) / GuildRosterPageSize + 1;
        Line(draw, 310, "成員 " + std::to_string(roster->total) + " 位 · 第 " +
            std::to_string(roster->page + 1) + " / " + std::to_string(pages) + " 頁");
        float y = 345.f;
        for (const auto& member : roster->members) {
            auto label = "角色 " + std::to_string(member.characterId) + (member.self ? "（你）" : "");
            label += " · " + std::string(RiverbankClassName(member.playerClass)) + " · " + RankName(member.rank);
            label += member.onlinePlayer ? " · 在線（玩家 " + std::to_string(member.onlinePlayer) + "）" : " · 離線";
            Line(draw, y, label);
            y += 32.f;
        }
        if (pages > 1) Line(draw, 551, "← → 翻頁");
    }

    bool Send(LocalWorldClient& client, GuildAction action, std::uint64_t target = 0, std::string_view name = {}) {
        if (!client.RequestGuild(action, target, name)) return false;
        requested_ = action;
        return true;
    }

    void SelectTarget(const Aevrynd::Input& input, const LocalWorldClient& client) {
        std::vector<std::uint64_t> players;
        for (const auto& entity : client.Snapshot()->entities) if (entity.id.low != client.PlayerId()) players.push_back(entity.id.low);
        if (players.empty()) { selected_ = 0; return; }
        auto next = std::ranges::find(players, selected_);
        if (next == players.end()) next = players.begin();
        else if (input.Pressed(VK_DOWN)) { if (++next == players.end()) next = players.begin(); }
        else if (input.Pressed(VK_UP)) { if (next == players.begin()) next = players.end(); --next; }
        selected_ = *next;
    }

    void Act(const Aevrynd::Input& input, LocalWorldClient& client, const GuildState& guild) {
        if (!guild.guildId) {
            if (input.Pressed('N')) { name_.clear(); mode_ = Mode::Name; message_.clear(); }
            else if (guild.inviteId && input.Pressed('J')) (void)Send(client, GuildAction::Join, std::uint64_t(guild.inviteId));
            else if (guild.inviteId && input.Pressed('K')) (void)Send(client, GuildAction::Decline, std::uint64_t(guild.inviteId));
        } else if (input.Pressed('L')) {
            mode_ = Mode::Leave;
            confirmTarget_ = 0;
        } else if (selected_ && guild.rank >= GuildRank::Officer && input.Pressed('I')) {
            (void)Send(client, GuildAction::Invite, selected_);
        } else if (selected_ && guild.rank == GuildRank::Leader) {
            if (input.Pressed('P')) (void)Send(client, GuildAction::Promote, selected_);
            else if (input.Pressed('O')) (void)Send(client, GuildAction::Demote, selected_);
            else if (input.Pressed('T')) { mode_ = Mode::Transfer; confirmTarget_ = selected_; }
        }
    }

    void EditName(const Aevrynd::Input& input, LocalWorldClient& client) {
        if (input.Pressed(VK_RETURN)) {
            if (!name_.empty() && Send(client, GuildAction::Create, 0, name_)) mode_ = Mode::Actions;
        } else if (input.Down(VK_CONTROL)) {
            if (input.Pressed('V')) PasteName();
        } else if (input.Pressed(VK_BACK) && !name_.empty()) {
            auto last = name_.size() - 1;
            while (last && (static_cast<unsigned char>(name_[last]) & 0xc0u) == 0x80u) --last;
            name_.resize(last);
        } else {
            const bool uppercase = input.Down(VK_SHIFT);
            for (unsigned key = 'A'; key <= 'Z' && name_.size() < GuildNameBytes; ++key) {
                if (input.Pressed(key)) name_ += static_cast<char>(uppercase ? key : key + ('a' - 'A'));
            }
            for (unsigned key = '0'; key <= '9' && name_.size() < GuildNameBytes; ++key) {
                if (input.Pressed(key)) name_ += static_cast<char>(key);
            }
            if (input.Pressed(VK_SPACE) && name_.size() < GuildNameBytes) name_ += ' ';
        }
    }

    void PasteName() {
        if (!OpenClipboard(nullptr)) return;
        const auto handle = GetClipboardData(CF_UNICODETEXT);
        if (const auto* text = handle ? static_cast<const wchar_t*>(GlobalLock(handle)) : nullptr) {
            const auto size = (std::min)(GlobalSize(handle) / sizeof(wchar_t), GuildNameBytes + 1);
            const auto* end = std::find(text, text + size, L'\0');
            std::array<char, GuildNameBytes> copy{};
            if (end != text + size) {
                const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, static_cast<int>(end - text),
                    copy.data(), static_cast<int>(copy.size()), nullptr, nullptr);
                if (bytes > 0) name_.assign(copy.data(), static_cast<std::size_t>(bytes));
            }
            GlobalUnlock(handle);
        }
        CloseClipboard();
    }

    static const char* RankName(GuildRank rank) {
        if (rank == GuildRank::Leader) return "會長";
        if (rank == GuildRank::Officer) return "幹部";
        return "成員";
    }

    static const char* ResultText(GuildResult result) {
        switch (result) {
        case GuildResult::Ok: return "公會已更新";
        case GuildResult::Rejected: return "目前無法執行這項公會操作";
        case GuildResult::InvalidName: return "請確認名稱長度及字元";
        case GuildResult::NameTaken: return "此公會名稱已被使用";
        case GuildResult::TransferRequired: return "有其他成員時，請先移交會長";
        case GuildResult::OwnsTerritory: return "公會持有領地，請先邀請成員並移交會長";
        }
        return "";
    }

    static void Line(Aevrynd::DrawList& draw, float y, std::string_view text) {
        draw.SystemText({246, y}, text, 18.f, {.91f,.96f,.93f,1});
    }
};
}
