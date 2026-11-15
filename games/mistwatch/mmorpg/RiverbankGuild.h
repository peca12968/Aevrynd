#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Mistwatch {
inline constexpr std::size_t GuildNameBytes = 48;

enum class GuildRank : std::uint8_t { None, Member, Officer, Leader };
enum class GuildAction : std::uint8_t { View, Create, Invite, Join, Decline, Leave, Promote, Demote, Transfer };
enum class GuildResult : std::uint8_t { Ok, Rejected, InvalidName, NameTaken, TransferRequired, OwnsTerritory };

struct GuildState {
    std::int64_t guildId{};
    GuildRank rank{GuildRank::None};
    std::string name;
    std::int64_t inviteId{};
    std::string inviteName;
};
}
