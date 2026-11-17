#pragma once

#include "RiverbankClass.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Mistwatch {
inline constexpr std::size_t GuildNameBytes = 48;
inline constexpr std::uint32_t GuildRosterPageSize = 6;

enum class GuildRank : std::uint8_t { None, Member, Officer, Leader };
enum class GuildAction : std::uint8_t { View, Create, Invite, Join, Decline, Leave, Promote, Demote, Transfer };
enum class GuildResult : std::uint8_t { Ok, Rejected, InvalidName, NameTaken, TransferRequired, OwnsTerritory };

struct GuildRosterMember {
    std::uint64_t characterId{};
    std::uint32_t onlinePlayer{};
    RiverbankClass playerClass{};
    GuildRank rank{};
    bool self{};
};

struct GuildRosterPage {
    std::uint64_t guildId{};
    std::uint32_t page{}, total{};
    std::vector<GuildRosterMember> members;
};

struct GuildState {
    std::int64_t guildId{};
    GuildRank rank{GuildRank::None};
    std::string name;
    std::int64_t inviteId{};
    std::string inviteName;
};
}
