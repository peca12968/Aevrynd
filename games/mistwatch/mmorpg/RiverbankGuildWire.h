#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankGuild.h"
#include <limits>
#include <vector>

namespace Mistwatch::RiverbankGuildWire {
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'G'}, std::byte{'D'}};
constexpr std::size_t HeaderBytes = Magic.size() + 2;
constexpr std::size_t RequestBytes = HeaderBytes + 2 * sizeof(std::uint64_t) + 1;
constexpr std::size_t ResponseBytes = HeaderBytes + 3 * sizeof(std::uint64_t) + 4;

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
