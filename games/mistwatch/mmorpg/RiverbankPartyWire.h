#pragma once

#include "RiverbankBattleWire.h"
#include "RiverbankParty.h"
#include <cmath>

namespace Mistwatch::RiverbankPartyWire {
using RiverbankBattleWire::detail::Put;
using RiverbankBattleWire::detail::Read;
constexpr std::array Magic{std::byte{'A'}, std::byte{'E'}, std::byte{'P'}, std::byte{'T'}, std::byte{1}};
constexpr std::size_t HeaderBytes = Magic.size() + 1;
inline constexpr std::uint32_t MapPositionsCapability = 1u << 23;

enum class Frame : std::uint8_t { Request = 1, Reply, MapRequest, MapReply };

struct Request { std::uint64_t sequence{}; PartyAction action{}; std::uint32_t target{}; bool mapPositions{}; };
struct Member { std::uint32_t actor{}; RiverbankClass playerClass{}; unsigned health{}; ArenaPosition position{}; };
struct Reply {
    std::uint64_t sequence{};
    PartyResult result{};
    std::uint32_t leader{}, inviter{};
    std::uint8_t members{};
    std::vector<Member> online;
    bool mapPositions{};
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
    *out++ = std::byte(request.mapPositions ? Frame::MapRequest : Frame::Request);
    Put(out, request.sequence);
    *out++ = std::byte(request.action);
    Put(out, request.target);
    return frame;
}

inline std::optional<Request> DecodeRequest(std::span<const std::byte> frame) {
    if (frame.size() != HeaderBytes + 13 || !HasMagic(frame)) return std::nullopt;
    const auto kind = Frame(frame[Magic.size()]);
    if (kind != Frame::Request && kind != Frame::MapRequest) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    const auto sequence = Read<std::uint64_t>(in);
    const auto action = PartyAction(*in++);
    const Request request{sequence, action, Read<std::uint32_t>(in), kind == Frame::MapRequest};
    return ValidRequest(request) ? std::optional{request} : std::nullopt;
}

inline std::vector<std::byte> EncodeReply(const Reply& reply) {
    const std::size_t memberBytes = reply.mapPositions ? 15 : 7;
    std::vector<std::byte> frame(HeaderBytes + 19 + memberBytes * reply.online.size());
    auto* out = std::ranges::copy(Magic, frame.data()).out;
    *out++ = std::byte(reply.mapPositions ? Frame::MapReply : Frame::Reply);
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
        if (reply.mapPositions) RiverbankBattleWire::detail::PutPoint(out, member.position);
    }
    return frame;
}

inline std::optional<Reply> DecodeReply(std::span<const std::byte> frame) {
    if (frame.size() < HeaderBytes + 19 || !HasMagic(frame)) return std::nullopt;
    const auto kind = Frame(frame[Magic.size()]);
    if (kind != Frame::Reply && kind != Frame::MapReply) return std::nullopt;
    auto* in = frame.data() + HeaderBytes;
    Reply reply;
    reply.mapPositions = kind == Frame::MapReply;
    const std::size_t memberBytes = reply.mapPositions ? 15 : 7;
    reply.sequence = Read<std::uint64_t>(in);
    reply.result = PartyResult(*in++);
    reply.leader = Read<std::uint32_t>(in);
    reply.inviter = Read<std::uint32_t>(in);
    reply.members = std::to_integer<std::uint8_t>(*in++);
    const auto count = std::to_integer<std::size_t>(*in++);
    if (!reply.sequence || reply.result > PartyResult::Full || reply.members == 1 ||
        reply.members > RiverbankParty::MaxMembers || count > reply.members ||
        (reply.members && (!count || reply.inviter)) || frame.size() != HeaderBytes + 19 + memberBytes * count)
        return std::nullopt;
    for (std::size_t index = 0; index < count; ++index) {
        const auto actor = Read<std::uint32_t>(in);
        const auto playerClass = RiverbankClass(*in++);
        const auto health = Read<std::uint16_t>(in);
        if (!actor || !IsRiverbankClass(playerClass) || health > CombatStatsFor(playerClass).health ||
            std::ranges::find(reply.online, actor, &Member::actor) != reply.online.end()) return std::nullopt;
        const auto position = reply.mapPositions ? RiverbankBattleWire::detail::ReadPoint(in) : ArenaPosition{};
        if (!std::isfinite(position.x) || !std::isfinite(position.z)) return std::nullopt;
        reply.online.push_back({actor, playerClass, health, position});
    }
    if (reply.leader && std::ranges::find(reply.online, reply.leader, &Member::actor) == reply.online.end())
        return std::nullopt;
    return reply;
}
}
