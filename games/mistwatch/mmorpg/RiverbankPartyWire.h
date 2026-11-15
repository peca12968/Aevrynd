#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankParty.h"

namespace Mistwatch::RiverbankPartyWire {
using RiverbankBattleWire::detail::Put;
using RiverbankBattleWire::detail::Read;
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'P'}, std::byte{'T'}, std::byte{1}};
constexpr std::size_t HeaderBytes = Magic.size() + 1;

struct Request { std::uint64_t sequence{}; PartyAction action{}; std::uint32_t target{}; };
struct Member { std::uint32_t actor{}; RiverbankClass playerClass{}; unsigned health{}; };
struct Reply {
    std::uint64_t sequence{};
    PartyResult result{};
    std::uint32_t leader{}, inviter{};
    std::uint8_t members{};
    std::vector<Member> online;
};

inline bool HasMagic(std::span<const std::byte> frame) {
    return frame.size() >= Magic.size() && std::ranges::equal(frame.first(Magic.size()), Magic);
}

inline bool ValidRequest(const Request& request) {
    return request.sequence && request.action <= PartyAction::Leave &&
        bool(request.target) == (request.action >= PartyAction::Invite && request.action <= PartyAction::Decline);
}

inline auto EncodeRequest(const Request& request) {
    std::array<std::byte, HeaderBytes + 13> frame{};
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{1};
    Put(out, request.sequence);
    *out++ = std::byte(request.action);
    Put(out, request.target);
    return frame;
}

inline std::optional<Request> DecodeRequest(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 13 || !HasMagic(frame) || frame[Magic.size()] != std::byte{1}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(in);
    const auto action = PartyAction(*in++);
    const Request request{sequence, action, Read<std::uint32_t>(in)};
    return ValidRequest(request) ? std::optional{request} : std::nullopt;
}

inline std::vector<std::byte> EncodeReply(const Reply& reply) {
    std::vector<std::byte> frame(HeaderBytes + 19 + 7 * reply.online.size());
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte{2};
    Put(out, reply.sequence);
    *out++ = std::byte(reply.result);
    Put(out, reply.leader);
    Put(out, reply.inviter);
    *out++ = std::byte(reply.members);
    *out++ = std::byte(reply.online.size());
    for (const auto& member : reply.online) {
        Put(out, member.actor);
        *out++ = std::byte(member.playerClass);
        Put(out, static_cast<std::uint16_t>(member.health));
    }
    return frame;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    if (frame.size() < HeaderBytes + 19 || !HasMagic(frame) || frame[Magic.size()] != std::byte{2}) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    Reply reply;
    reply.sequence = Read<std::uint64_t>(in);
    reply.result = PartyResult(*in++);
    reply.leader = Read<std::uint32_t>(in);
    reply.inviter = Read<std::uint32_t>(in);
    reply.members = std::to_integer<std::uint8_t>(*in++);
    const auto count = std::to_integer<std::size_t>(*in++);
    if (!reply.sequence || reply.result > PartyResult::Full || reply.members == 1 ||
        reply.members > RiverbankParty::MaxMembers || count > reply.members ||
        (reply.members && (!count || reply.inviter)) || frame.size() != HeaderBytes + 19 + 7 * count)
        return std::nullopt;
    for (std::size_t index = 0; index < count; ++index) {
        const auto actor = Read<std::uint32_t>(in);
        const auto playerClass = RiverbankClass(*in++);
        const auto health = Read<std::uint16_t>(in);
        if (!actor || !IsRiverbankClass(playerClass) || health > CombatStatsFor(playerClass).health ||
            std::ranges::find(reply.online, actor, &Member::actor) != reply.online.end()) return std::nullopt;
        reply.online.push_back({actor, playerClass, health});
    }
    if (reply.leader && std::ranges::find(reply.online, reply.leader, &Member::actor) == reply.online.end())
        return std::nullopt;
    return reply;
}
}
