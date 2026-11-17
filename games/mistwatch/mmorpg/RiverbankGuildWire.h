#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankGuild.h"
#include <limits>
#include <vector>

namespace Mistwatch::RiverbankGuildWire {
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'G'}, std::byte{'D'}};
constexpr std::size_t HeaderBytes = Magic.size() + 2;
constexpr std::uint32_t RosterCapability = 1u << 27;
static_assert((RosterCapability & (RiverbankBattleWire::DownedEntityFlag | RiverbankBattleWire::DodgingEntityFlag |
    RiverbankBattleWire::DodgeCapability | RiverbankBattleWire::CampReturnCapability |
    RiverbankBattleWire::StructureCoverCapability)) == 0);
constexpr std::size_t RosterQueryBytes = HeaderBytes + 12;
constexpr std::size_t RosterReplyBytes = HeaderBytes + 25;
constexpr std::size_t RosterRowBytes = 15;
constexpr std::size_t RequestBytes = HeaderBytes + 2 * sizeof(std::uint64_t) + 1;
constexpr std::size_t ResponseBytes = HeaderBytes + 3 * sizeof(std::uint64_t) + 4;

struct RosterQuery { std::uint64_t sequence{}; std::uint32_t page{}; };
struct RosterReply { std::uint64_t sequence{}; GuildRosterPage roster; };

struct Request {
    std::uint64_t sequence{};
    GuildAction action{};
    std::uint64_t target{};
    std::string_view name;
};

struct Response {
    std::uint64_t sequence{};
    GuildResult result{};
    GuildState state;
};

inline bool HasMagic(std::span<const std::byte> input) {
    return input.size() >= Magic.size() && std::ranges::equal(input.first(Magic.size()), Magic);
}

inline std::optional<RosterQuery> DecodeRosterQuery(std::span<const std::byte> frame) {
    if (frame.size() != RosterQueryBytes || !HasMagic(frame) || frame[4] != std::byte{1} ||
        frame[5] != std::byte{3}) return std::nullopt;
    auto* cursor = frame.data() + HeaderBytes;
    const auto sequence = RiverbankBattleWire::detail::Read<std::uint64_t>(cursor);
    const auto page = RiverbankBattleWire::detail::Read<std::uint32_t>(cursor);
    return sequence ? std::optional{RosterQuery{sequence, page}} : std::nullopt;
}

inline auto EncodeRosterQuery(RosterQuery query) {
    std::array<std::byte, RosterQueryBytes> frame{};
    auto* cursor = std::ranges::copy(Magic, frame.data()).out;
    *cursor++ = std::byte{1};
    *cursor++ = std::byte{3};
    RiverbankBattleWire::detail::Put(cursor, query.sequence);
    RiverbankBattleWire::detail::Put(cursor, query.page);
    return frame;
}

inline std::optional<RosterReply> DecodeRosterReply(std::span<const std::byte> frame) {
    if (frame.size() < RosterReplyBytes || !HasMagic(frame) || frame[4] != std::byte{1} ||
        frame[5] != std::byte{4}) return std::nullopt;
    using RiverbankBattleWire::detail::Read;
    auto* cursor = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(cursor);
    GuildRosterPage roster;
    roster.guildId = Read<std::uint64_t>(cursor);
    roster.page = Read<std::uint32_t>(cursor);
    roster.total = Read<std::uint32_t>(cursor);
    const auto count = std::to_integer<std::uint32_t>(*cursor++);
    constexpr auto maxId = std::uint64_t((std::numeric_limits<std::int64_t>::max)());
    const auto offset = std::uint64_t(roster.page) * GuildRosterPageSize;
    if (!sequence || roster.guildId > maxId || count > GuildRosterPageSize ||
        frame.size() != RosterReplyBytes + count * RosterRowBytes ||
        (!roster.guildId && (roster.page || roster.total || count)) ||
        (roster.guildId && (offset >= roster.total ||
         count != (std::min)(std::uint64_t(GuildRosterPageSize), roster.total - offset)))) return std::nullopt;
    bool sawSelf = false;
    for (std::uint32_t i = 0; i < count; ++i) {
        GuildRosterMember entry;
        entry.characterId = Read<std::uint64_t>(cursor);
        entry.onlinePlayer = Read<std::uint32_t>(cursor);
        entry.playerClass = RiverbankClass(*cursor++);
        entry.rank = GuildRank(*cursor++);
        const auto self = std::to_integer<unsigned>(*cursor++);
        if (!entry.characterId || entry.characterId > maxId || !IsRiverbankClass(entry.playerClass) ||
            entry.rank < GuildRank::Member || entry.rank > GuildRank::Leader ||
            self > 1 || (self && (sawSelf || !entry.onlinePlayer))) return std::nullopt;
        if (!roster.members.empty()) {
            const auto& previous = roster.members.back();
            if (previous.rank < entry.rank || (previous.rank == entry.rank && previous.characterId >= entry.characterId))
                return std::nullopt;
        }
        if (std::ranges::any_of(roster.members, [&](const auto& previous) {
            return entry.characterId == previous.characterId ||
                (entry.onlinePlayer && entry.onlinePlayer == previous.onlinePlayer);
        })) return std::nullopt;
        entry.self = self != 0;
        sawSelf |= entry.self;
        roster.members.push_back(entry);
    }
    return RosterReply{sequence, std::move(roster)};
}

inline std::vector<std::byte> EncodeRosterReply(std::uint64_t sequence, const GuildRosterPage& roster) {
    if (roster.members.size() > GuildRosterPageSize) return {};
    std::vector<std::byte> frame(RosterReplyBytes + roster.members.size() * RosterRowBytes);
    auto* cursor = std::ranges::copy(Magic, frame.data()).out;
    *cursor++ = std::byte{1};
    *cursor++ = std::byte{4};
    using RiverbankBattleWire::detail::Put;
    Put(cursor, sequence);
    Put(cursor, roster.guildId);
    Put(cursor, roster.page);
    Put(cursor, roster.total);
    *cursor++ = std::byte(roster.members.size());
    for (const auto& member : roster.members) {
        Put(cursor, member.characterId);
        Put(cursor, member.onlinePlayer);
        *cursor++ = std::byte(member.playerClass);
        *cursor++ = std::byte(member.rank);
        *cursor++ = std::byte(member.self ? 1 : 0);
    }
    return frame;
}

inline std::optional<Request> DecodeRequest(std::span<const std::byte> frame) {
    if (frame.size() < RequestBytes || frame.size() > RequestBytes + GuildNameBytes || !HasMagic(frame) ||
        frame[Magic.size()] != std::byte{1} || frame[Magic.size() + 1] != std::byte{1}) return std::nullopt;
    auto* input = frame.data() + HeaderBytes;
    const auto sequence = RiverbankBattleWire::detail::Read<std::uint64_t>(input);
    const auto action = GuildAction(*input++);
    const auto target = RiverbankBattleWire::detail::Read<std::uint64_t>(input);
    const std::string_view name(reinterpret_cast<const char*>(input), frame.size() - RequestBytes);
    if (!sequence || action > GuildAction::Transfer) return std::nullopt;
    switch (action) {
    case GuildAction::Create: if (target) return std::nullopt; break;
    case GuildAction::View:
    case GuildAction::Leave: if (target || !name.empty()) return std::nullopt; break;
    default:
        if (!target || target > std::uint64_t((std::numeric_limits<std::int64_t>::max)()) || !name.empty()) return std::nullopt;
        break;
    }
    return Request{sequence, action, target, name};
}

inline std::vector<std::byte> EncodeRequest(const Request& request) {
    if (request.name.size() > GuildNameBytes) return {};
    std::vector<std::byte> frame(RequestBytes + request.name.size());
    auto* cursor = std::ranges::copy(Magic, frame.data()).out;
    *cursor++ = std::byte{1};
    *cursor++ = std::byte{1};
    RiverbankBattleWire::detail::Put(cursor, request.sequence);
    *cursor++ = std::byte(request.action);
    RiverbankBattleWire::detail::Put(cursor, request.target);
    for (const auto character : request.name) *cursor++ = std::byte(character);
    return DecodeRequest(frame) ? std::move(frame) : std::vector<std::byte>{};
}

inline std::vector<std::byte> EncodeResponse(std::uint64_t sequence, GuildResult result, const GuildState& state) {
    std::vector<std::byte> frame(ResponseBytes + state.name.size() + state.inviteName.size());
    auto* cursor = std::ranges::copy(Magic, frame.data()).out;
    *cursor++ = std::byte{1};
    *cursor++ = std::byte{2};
    RiverbankBattleWire::detail::Put(cursor, sequence);
    *cursor++ = std::byte(result);
    *cursor++ = std::byte(state.rank);
    RiverbankBattleWire::detail::Put(cursor, std::uint64_t(state.guildId));
    RiverbankBattleWire::detail::Put(cursor, std::uint64_t(state.inviteId));
    for (const auto name : {std::string_view(state.name), std::string_view(state.inviteName)}) {
        *cursor++ = std::byte(name.size());
        for (const auto character : name) *cursor++ = std::byte(character);
    }
    return frame;
}

inline std::optional<Response> DecodeResponse(std::span<const std::byte> frame) {
    if (frame.size() < ResponseBytes || frame.size() > ResponseBytes + 2 * GuildNameBytes || !HasMagic(frame) ||
        frame[Magic.size()] != std::byte{1} || frame[Magic.size() + 1] != std::byte{2}) return std::nullopt;
    auto* cursor = frame.data() + HeaderBytes;
    const auto sequence = RiverbankBattleWire::detail::Read<std::uint64_t>(cursor);
    const auto result = GuildResult(*cursor++);
    const auto rank = GuildRank(*cursor++);
    const auto guild = RiverbankBattleWire::detail::Read<std::uint64_t>(cursor);
    const auto invite = RiverbankBattleWire::detail::Read<std::uint64_t>(cursor);
    constexpr auto maxId = std::uint64_t((std::numeric_limits<std::int64_t>::max)());
    if (!sequence || result > GuildResult::OwnsTerritory || rank > GuildRank::Leader ||
        guild > maxId || invite > maxId || (guild && invite) || bool(guild) != (rank != GuildRank::None)) return std::nullopt;
    const auto nameSize = std::to_integer<std::size_t>(*cursor++);
    if (nameSize > GuildNameBytes || frame.size() < ResponseBytes + nameSize || bool(nameSize) != bool(guild)) return std::nullopt;
    const std::string_view name(reinterpret_cast<const char*>(cursor), nameSize);
    cursor += nameSize;
    const auto inviteSize = std::to_integer<std::size_t>(*cursor++);
    if (inviteSize > GuildNameBytes || frame.size() != ResponseBytes + nameSize + inviteSize || bool(inviteSize) != bool(invite))
        return std::nullopt;
    return Response{sequence, result, {static_cast<std::int64_t>(guild), rank, std::string(name),
                                     static_cast<std::int64_t>(invite), std::string(reinterpret_cast<const char*>(cursor), inviteSize)}};
}
}
