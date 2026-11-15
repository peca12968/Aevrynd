#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace Mistwatch {
enum class PartyAction : std::uint8_t { View, Invite, Accept, Decline, Leave };
enum class PartyResult : std::uint8_t { Ok, Rejected, Full };

class RiverbankParty {
public:
    using ProfileId = std::int64_t;
    static constexpr std::size_t MaxMembers = 4;

    std::span<const ProfileId> Members(ProfileId profile) const {
        const auto group = std::ranges::find_if(groups_, [profile](const auto& entries) {
            return std::ranges::find(entries, profile) != entries.end();
        });
        return group == groups_.end() ? std::span<const ProfileId>{} : std::span<const ProfileId>{*group};
    }

    ProfileId Inviter(ProfileId target, std::uint64_t tick) const {
        const auto entry = invites_.find(target);
        return entry == invites_.end() || tick >= entry->second.deadline ? 0 : entry->second.sender;
    }

    void ExpireInvites(std::uint64_t tick) {
        std::erase_if(invites_, [tick](const auto& invite) { return tick >= invite.second.deadline; });
    }

    PartyResult Apply(ProfileId actor, PartyAction action, ProfileId target, std::uint64_t tick) {
        if (actor <= 0) return PartyResult::Rejected;
        switch (action) {
        case PartyAction::View: return PartyResult::Ok;
        case PartyAction::Invite: {
            const auto group = Members(actor);
            if (target <= 0 || target == actor || !Members(target).empty() || Inviter(target, tick) ||
                (!group.empty() && group.front() != actor)) return PartyResult::Rejected;
            if (group.size() == MaxMembers) return PartyResult::Full;
            invites_.insert_or_assign(target, Invite{actor, tick + InviteLifetimeTicks});
            return PartyResult::Ok;
        }
        case PartyAction::Accept:
        case PartyAction::Decline:
            if (target <= 0 || Inviter(actor, tick) != target) return PartyResult::Rejected;
            invites_.erase(actor);
            if (action == PartyAction::Decline) return PartyResult::Ok;
            return Join(actor, target);
        case PartyAction::Leave:
            if (Members(actor).empty()) return PartyResult::Rejected;
            Forget(actor);
            return PartyResult::Ok;
        }
        return PartyResult::Rejected;
    }

    void CancelInvites(ProfileId profile) {
        std::erase_if(invites_, [profile](const auto& entry) {
            return entry.first == profile || entry.second.sender == profile;
        });
    }

    void Forget(ProfileId profile) {
        CancelInvites(profile);
        for (auto group = groups_.begin(); group != groups_.end(); ++group) {
            if (!std::erase(*group, profile)) continue;
            if (group->size() <= 1) {
                if (!group->empty()) CancelInvites(group->front());
                groups_.erase(group);
            }
            return;
        }
    }

private:
    static constexpr std::uint64_t InviteLifetimeTicks = 600;
    struct Invite { ProfileId sender; std::uint64_t deadline; };
    std::vector<std::vector<ProfileId>> groups_;
    std::map<ProfileId, Invite> invites_;

    PartyResult Join(ProfileId member, ProfileId leader) {
        if (!Members(member).empty()) return PartyResult::Rejected;
        for (auto& group : groups_) {
            if (std::ranges::find(group, leader) == group.end()) continue;
            if (group.front() != leader) return PartyResult::Rejected;
            if (group.size() == MaxMembers) return PartyResult::Full;
            group.push_back(member);
            CancelInvites(member);
            return PartyResult::Ok;
        }
        groups_.push_back({leader, member});
        CancelInvites(member);
        invites_.erase(leader);
        return PartyResult::Ok;
    }
};
}
